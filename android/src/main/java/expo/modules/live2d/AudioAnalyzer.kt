package expo.modules.live2d

import android.content.Context
import android.content.res.AssetFileDescriptor
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.media.MediaCodec
import android.media.MediaExtractor
import android.media.MediaFormat
import android.util.Log
import kotlin.math.sqrt
import kotlin.math.max
import kotlin.math.min
import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.PI

/**
 * Audio analyzer for Live2D lip sync (optimized v3).
 *
 * Two-track strategy matching iOS L2DAudioEngine:
 *   Track 1: RMS (volume) → mouthOpenY — updated per decode buffer (~100-200Hz)
 *   Track 2: FFT → vowel weights — updated per 2048 samples with 75% overlap (~86Hz)
 *
 * Frame interpolation: updateFrame() interpolates between last/target values
 * for smooth 60fps output regardless of analysis update rate.
 */
class AudioAnalyzer(private val context: Context) {

    // ============================================================
    // Public lip sync values (read from GL thread)
    // ============================================================
    @Volatile var mouthOpenY: Float = 0f; private set
    @Volatile var vowelA: Float = 0f; private set
    @Volatile var vowelI: Float = 0f; private set
    @Volatile var vowelU: Float = 0f; private set
    @Volatile var vowelE: Float = 0f; private set
    @Volatile var vowelO: Float = 0f; private set
    @Volatile var isPlaying: Boolean = false; private set

    // ============================================================
    // Internal state
    // ============================================================
    private var playbackThread: Thread? = null
    private var stopRequested = false

    // FFT config (matching iOS)
    private val fftSize = 2048
    private val fftSizeOver2 = fftSize / 2
    private val hopSize = fftSize / 4  // 75% overlap

    // Shared magnitudes (FFT thread → GL thread)
    private val sharedMagnitudes = FloatArray(fftSizeOver2)
    @Volatile private var magnitudesReady = false
    @Volatile private var currentSampleRate = 44100f

    // RMS target (decode thread → GL thread, updated per buffer)
    @Volatile private var targetRMS: Float = 0f

    // Smoothed values (GL thread only)
    private var smoothA = 0f
    private var smoothI = 0f
    private var smoothU = 0f
    private var smoothE = 0f
    private var smoothO = 0f
    private var smoothMouth = 0f

    // Hann window
    private val hannWindow = FloatArray(fftSize) { i ->
        (0.5f * (1.0f - cos(2.0 * PI * i / (fftSize - 1)))).toFloat()
    }

    // ============================================================
    // Public API
    // ============================================================

    fun play(assetPath: String) {
        stop()
        stopRequested = false
        magnitudesReady = false
        targetRMS = 0f
        playbackThread = Thread {
            playAndAnalyze(assetPath)
        }.apply {
            name = "Live2DAudio"
            start()
        }
    }

    fun stop() {
        stopRequested = true
        isPlaying = false
        magnitudesReady = false
        targetRMS = 0f
        playbackThread?.join(500)
        playbackThread = null
    }

    /**
     * Called every frame from GL thread (~60fps).
     * Smoothly interpolates all values for fluid animation.
     */
    fun updateFrame() {
        if (!isPlaying) {
            // Decay (matching iOS 0.85)
            val decay = 0.85f
            smoothA *= decay; smoothI *= decay; smoothU *= decay
            smoothE *= decay; smoothO *= decay; smoothMouth *= decay
            vowelA = smoothA; vowelI = smoothI; vowelU = smoothU
            vowelE = smoothE; vowelO = smoothO; mouthOpenY = smoothMouth
            return
        }

        // ---- Track 1: RMS → mouthOpenY (fast, per-buffer updates) ----
        val rmsTarget = min(1f, targetRMS * 12.0f) // Scale RMS to 0-1
        // Faster alpha for mouth (responsive) - 0.4 for quicker open, 0.25 for slower close
        val mouthAlpha = if (rmsTarget > smoothMouth) 0.4f else 0.25f
        smoothMouth += mouthAlpha * (rmsTarget - smoothMouth)
        mouthOpenY = smoothMouth

        // ---- Track 2: FFT → vowels (slower, analytical) ----
        if (magnitudesReady) {
            val binWidth = currentSampleRate / fftSize.toFloat()

            val lowEnergy  = energyInBand(100f, 400f, binWidth)
            val midEnergy  = energyInBand(400f, 1000f, binWidth)
            val highMidE   = energyInBand(1000f, 1600f, binWidth)
            val highEnergy = energyInBand(1600f, 2800f, binWidth)
            val totalEnergy = lowEnergy + midEnergy + highMidE + highEnergy + 0.0001f

            val lowRatio  = lowEnergy / totalEnergy
            val midRatio  = midEnergy / totalEnergy
            val highMidR  = highMidE / totalEnergy
            val highRatio = highEnergy / totalEnergy

            // Vowel mapping (iOS coefficients)
            val rawA = min(1f, midRatio * 2.5f + highMidR * 1.5f)
            val rawI = min(1f, lowRatio * 1.5f + highRatio * 3.0f)
            val rawU = min(1f, lowRatio * 2.0f + midRatio * 1.0f)
            val rawE = min(1f, midRatio * 1.5f + highRatio * 2.0f)
            val rawO = min(1f, midRatio * 2.5f)

            // Mouth gate
            val mouthGate = if (smoothMouth > 0.1f) 1.0f else smoothMouth / 0.1f

            // Smooth vowels (alpha=0.3 matching iOS)
            val alpha = 0.3f
            smoothA += alpha * (rawA * mouthGate - smoothA)
            smoothI += alpha * (rawI * mouthGate - smoothI)
            smoothU += alpha * (rawU * mouthGate - smoothU)
            smoothE += alpha * (rawE * mouthGate - smoothE)
            smoothO += alpha * (rawO * mouthGate - smoothO)

            vowelA = smoothA; vowelI = smoothI; vowelU = smoothU
            vowelE = smoothE; vowelO = smoothO
        }
    }

    fun release() {
        stop()
    }

    // ============================================================
    // Energy calculation
    // ============================================================

    private fun energyInBand(freqLow: Float, freqHigh: Float, binWidth: Float): Float {
        var binLow = (freqLow / binWidth).toInt()
        var binHigh = (freqHigh / binWidth).toInt()
        binLow = max(0, min(binLow, fftSizeOver2 - 1))
        binHigh = max(binLow, min(binHigh, fftSizeOver2 - 1))
        var sum = 0f
        for (i in binLow..binHigh) sum += sharedMagnitudes[i]
        return sum / (binHigh - binLow + 1).toFloat()
    }

    // ============================================================
    // Decode + Play + Analyze (background thread)
    // ============================================================

    private fun playAndAnalyze(assetPath: String) {
        var extractor: MediaExtractor? = null
        var codec: MediaCodec? = null
        var audioTrack: AudioTrack? = null

        try {
            val afd: AssetFileDescriptor = context.assets.openFd(assetPath)
            extractor = MediaExtractor()
            extractor.setDataSource(afd.fileDescriptor, afd.startOffset, afd.length)
            afd.close()

            var audioTrackIndex = -1
            var format: MediaFormat? = null
            for (i in 0 until extractor.trackCount) {
                val f = extractor.getTrackFormat(i)
                if ((f.getString(MediaFormat.KEY_MIME) ?: "").startsWith("audio/")) {
                    audioTrackIndex = i; format = f; break
                }
            }
            if (audioTrackIndex < 0 || format == null) return

            extractor.selectTrack(audioTrackIndex)
            val sampleRate = format.getInteger(MediaFormat.KEY_SAMPLE_RATE)
            val channelCount = format.getInteger(MediaFormat.KEY_CHANNEL_COUNT)
            val mime = format.getString(MediaFormat.KEY_MIME) ?: return
            currentSampleRate = sampleRate.toFloat()

            codec = MediaCodec.createDecoderByType(mime)
            codec.configure(format, null, null, 0)
            codec.start()

            val channelConfig = if (channelCount == 1)
                AudioFormat.CHANNEL_OUT_MONO else AudioFormat.CHANNEL_OUT_STEREO
            val bufferSize = AudioTrack.getMinBufferSize(
                sampleRate, channelConfig, AudioFormat.ENCODING_PCM_16BIT
            )
            audioTrack = AudioTrack.Builder()
                .setAudioAttributes(AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
                .setAudioFormat(AudioFormat.Builder()
                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                    .setSampleRate(sampleRate)
                    .setChannelMask(channelConfig).build())
                .setBufferSizeInBytes(bufferSize)
                .build()

            audioTrack.play()
            isPlaying = true
            Log.d(TAG, "Playing: $assetPath (rate=$sampleRate)")

            val fftAccumulator = ShortArray(fftSize)
            var fftPos = 0
            val info = MediaCodec.BufferInfo()
            var inputEOS = false

            while (!stopRequested) {
                if (!inputEOS) {
                    val inputIndex = codec.dequeueInputBuffer(10000)
                    if (inputIndex >= 0) {
                        val inputBuffer = codec.getInputBuffer(inputIndex) ?: continue
                        val readSize = extractor.readSampleData(inputBuffer, 0)
                        if (readSize < 0) {
                            codec.queueInputBuffer(inputIndex, 0, 0, 0,
                                MediaCodec.BUFFER_FLAG_END_OF_STREAM)
                            inputEOS = true
                        } else {
                            codec.queueInputBuffer(inputIndex, 0, readSize,
                                extractor.sampleTime, 0)
                            extractor.advance()
                        }
                    }
                }

                val outputIndex = codec.dequeueOutputBuffer(info, 10000)
                if (outputIndex >= 0) {
                    if (info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0) {
                        codec.releaseOutputBuffer(outputIndex, false)
                        break
                    }

                    val outputBuffer = codec.getOutputBuffer(outputIndex) ?: continue
                    val pcmData = ShortArray(info.size / 2)
                    outputBuffer.asShortBuffer().get(pcmData)

                    // ---- Track 1: Compute RMS per buffer (fast, ~100-200Hz) ----
                    computeRMS(pcmData, channelCount)

                    // Write to AudioTrack (blocking)
                    audioTrack.write(pcmData, 0, pcmData.size)

                    // ---- Track 2: Accumulate for FFT with 75% overlap ----
                    var i = 0
                    while (i < pcmData.size) {
                        fftAccumulator[fftPos] = pcmData[i]
                        fftPos++
                        if (fftPos >= fftSize) {
                            runFFT(fftAccumulator)
                            System.arraycopy(fftAccumulator, hopSize, fftAccumulator, 0, fftSize - hopSize)
                            fftPos = fftSize - hopSize
                        }
                        i += channelCount
                    }

                    codec.releaseOutputBuffer(outputIndex, false)
                }
            }

            Log.d(TAG, "Playback complete: $assetPath")
        } catch (e: Exception) {
            Log.e(TAG, "Error: $assetPath", e)
        } finally {
            isPlaying = false
            audioTrack?.let { it.stop(); it.release() }
            codec?.let { it.stop(); it.release() }
            extractor?.release()
        }
    }

    // ============================================================
    // RMS calculation (fast, per-buffer)
    // ============================================================

    private fun computeRMS(pcmData: ShortArray, channelCount: Int) {
        var sumSq = 0.0
        var count = 0
        var i = 0
        while (i < pcmData.size) {
            val sample = pcmData[i].toDouble() / 32768.0
            sumSq += sample * sample
            count++
            i += channelCount
        }
        if (count > 0) {
            targetRMS = sqrt(sumSq / count).toFloat()
        }
    }

    // ============================================================
    // FFT (matching iOS vDSP pipeline)
    // ============================================================

    private fun runFFT(pcmData: ShortArray) {
        val n = pcmData.size
        val real = DoubleArray(n)
        val imag = DoubleArray(n)
        for (i in 0 until n) {
            real[i] = (pcmData[i].toDouble() / 32768.0) * hannWindow[i]
            imag[i] = 0.0
        }
        fft(real, imag, n)
        val scale = 1.0f / (n * 2).toFloat()
        for (k in 0 until fftSizeOver2) {
            sharedMagnitudes[k] = ((real[k] * real[k] + imag[k] * imag[k]) * scale).toFloat()
        }
        magnitudesReady = true
    }

    private fun fft(real: DoubleArray, imag: DoubleArray, n: Int) {
        var j = 0
        for (i in 0 until n) {
            if (i < j) {
                var t = real[i]; real[i] = real[j]; real[j] = t
                t = imag[i]; imag[i] = imag[j]; imag[j] = t
            }
            var m = n / 2
            while (m >= 1 && j >= m) { j -= m; m /= 2 }
            j += m
        }
        var step = 2
        while (step <= n) {
            val half = step / 2
            val angle = -2.0 * PI / step
            for (i in 0 until n step step) {
                for (k in 0 until half) {
                    val a = angle * k
                    val c = cos(a); val s = sin(a)
                    val tR = c * real[i + k + half] - s * imag[i + k + half]
                    val tI = s * real[i + k + half] + c * imag[i + k + half]
                    real[i + k + half] = real[i + k] - tR
                    imag[i + k + half] = imag[i + k] - tI
                    real[i + k] += tR
                    imag[i + k] += tI
                }
            }
            step *= 2
        }
    }

    companion object {
        private const val TAG = "AudioAnalyzer"
    }
}
