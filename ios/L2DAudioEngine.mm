#import "L2DAudioEngine.h"
#import <AVFoundation/AVFoundation.h>
#import <Accelerate/Accelerate.h>

static const int kFFTSize = 2048;
static const int kFFTSizeOver2 = kFFTSize / 2;

@implementation L2DAudioEngine {
    AVAudioEngine *_engine;
    AVAudioPlayerNode *_playerNode;
    AVAudioMixerNode *_mixerNode;
    
    // FFT setup
    FFTSetup _fftSetup;
    float *_window;
    float *_fftInReal;
    DSPSplitComplex _fftOut;
    float *_magnitudes;
    
    // Smoothed vowel values
    float _smoothA, _smoothI, _smoothU, _smoothE, _smoothO;
    float _smoothMouth;
    float _sampleRate;
    BOOL _hasNewData;
    
    // Thread-safe magnitude buffer
    float *_sharedMagnitudes;
    BOOL _magnitudesReady;
    
    // Generation counter to handle stale completion callbacks
    NSInteger _playGeneration;
}

- (instancetype)init {
    self = [super init];
    if (!self) return nil;
    
    // FFT setup
    _fftSetup = vDSP_create_fftsetup(log2f(kFFTSize), kFFTRadix2);
    _window = (float *)calloc(kFFTSize, sizeof(float));
    _fftInReal = (float *)calloc(kFFTSize, sizeof(float));
    _fftOut.realp = (float *)calloc(kFFTSizeOver2, sizeof(float));
    _fftOut.imagp = (float *)calloc(kFFTSizeOver2, sizeof(float));
    _magnitudes = (float *)calloc(kFFTSizeOver2, sizeof(float));
    _sharedMagnitudes = (float *)calloc(kFFTSizeOver2, sizeof(float));
    
    vDSP_hann_window(_window, kFFTSize, vDSP_HANN_NORM);
    
    _smoothA = _smoothI = _smoothU = _smoothE = _smoothO = 0;
    _smoothMouth = 0;
    _sampleRate = 44100;
    _magnitudesReady = NO;
    
    return self;
}

- (void)dealloc {
    [self stop];
    vDSP_destroy_fftsetup(_fftSetup);
    free(_window);
    free(_fftInReal);
    free(_fftOut.realp);
    free(_fftOut.imagp);
    free(_magnitudes);
    free(_sharedMagnitudes);
}

// MARK: - Playback

- (void)playAudioFile:(NSString *)path {
    // Stop current playback but keep engine alive
    if (_playerNode) {
        [_playerNode stop];
    }
    _magnitudesReady = NO;
    
    NSURL *url = [NSURL fileURLWithPath:path];
    AVAudioFile *file = [[AVAudioFile alloc] initForReading:url error:nil];
    if (!file) {
        NSLog(@"[L2DAudio] Cannot open file: %@", path);
        return;
    }
    
    _sampleRate = file.processingFormat.sampleRate;
    
    // Create engine only if needed (first time or after full stop)
    if (!_engine) {
        _engine = [[AVAudioEngine alloc] init];
        _playerNode = [[AVAudioPlayerNode alloc] init];
        [_engine attachNode:_playerNode];
        
        _mixerNode = _engine.mainMixerNode;
        AVAudioFormat *format = [_mixerNode outputFormatForBus:0];
        
        [_engine connect:_playerNode to:_mixerNode format:file.processingFormat];
        
        // Install tap on mixer for FFT analysis
        __weak L2DAudioEngine *weakSelf = self;
        [_mixerNode installTapOnBus:0 bufferSize:kFFTSize format:format
                              block:^(AVAudioPCMBuffer *buffer, AVAudioTime *when) {
            [weakSelf processBuffer:buffer];
        }];
        
        NSError *error = nil;
        [_engine startAndReturnError:&error];
        if (error) {
            NSLog(@"[L2DAudio] Engine start error: %@", error);
            return;
        }
        NSLog(@"[L2DAudio] Engine created (sr=%.0f)", _sampleRate);
    }
    
    // Increment generation to ignore stale completionHandlers from previous files
    _playGeneration++;
    NSInteger currentGen = _playGeneration;
    
    __weak L2DAudioEngine *weakSelf = self;
    [_playerNode scheduleFile:file atTime:nil completionHandler:^{
        dispatch_async(dispatch_get_main_queue(), ^{
            L2DAudioEngine *strongSelf = weakSelf;
            // Only set isPlaying=NO if this is still the current generation
            if (strongSelf && strongSelf->_playGeneration == currentGen) {
                strongSelf->_isPlaying = NO;
                NSLog(@"[L2DAudio] Playback finished (gen=%ld)", (long)currentGen);
            }
        });
    }];
    [_playerNode play];
    _isPlaying = YES;
    NSLog(@"[L2DAudio] Playing: %@ (gen=%ld sr=%.0f)", [path lastPathComponent], (long)currentGen, _sampleRate);
}

- (void)stop {
    if (_engine) {
        @try { [_mixerNode removeTapOnBus:0]; } @catch (NSException *e) {}
        [_playerNode stop];
        [_engine stop];
        _engine = nil;
        _playerNode = nil;
        _mixerNode = nil;
    }
    _isPlaying = NO;
    _magnitudesReady = NO;
}

// MARK: - FFT Processing (audio thread)

- (void)processBuffer:(AVAudioPCMBuffer *)buffer {
    const float *channelData = buffer.floatChannelData[0];
    UInt32 frameCount = buffer.frameLength;
    if (frameCount < kFFTSize) return;
    
    // Apply window function
    vDSP_vmul(channelData, 1, _window, 1, _fftInReal, 1, kFFTSize);
    
    // Pack into split complex
    vDSP_ctoz((DSPComplex *)_fftInReal, 2, &_fftOut, 1, kFFTSizeOver2);
    
    // Forward FFT
    vDSP_fft_zrip(_fftSetup, &_fftOut, 1, log2f(kFFTSize), FFT_FORWARD);
    
    // Compute magnitudes
    vDSP_zvmags(&_fftOut, 1, _magnitudes, 1, kFFTSizeOver2);
    
    // Normalize
    float scale = 1.0f / (float)(kFFTSize * 2);
    vDSP_vsmul(_magnitudes, 1, &scale, _magnitudes, 1, kFFTSizeOver2);
    
    // Copy to shared buffer (thread-safe enough for our purpose)
    memcpy(_sharedMagnitudes, _magnitudes, kFFTSizeOver2 * sizeof(float));
    _magnitudesReady = YES;
}

// MARK: - Vowel Analysis (main thread, called each frame)

- (void)updateAnalysis {
    if (!_isPlaying || !_magnitudesReady) {
        // Decay to zero when not playing
        float decay = 0.85f;
        _smoothA *= decay; _smoothI *= decay; _smoothU *= decay;
        _smoothE *= decay; _smoothO *= decay; _smoothMouth *= decay;
        _vowelA = _smoothA; _vowelI = _smoothI; _vowelU = _smoothU;
        _vowelE = _smoothE; _vowelO = _smoothO; _mouthOpenY = _smoothMouth;
        return;
    }
    
    // Helper: get energy in frequency range
    float binWidth = _sampleRate / (float)kFFTSize;
    
    // Formant frequency bands for vowels (simplified)
    // Each vowel has characteristic F1 (first formant) and F2 (second formant):
    //   A: F1=800Hz,  F2=1200Hz  (open mouth, wide)
    //   I: F1=300Hz,  F2=2300Hz  (closed, spread)
    //   U: F1=350Hz,  F2=1000Hz  (closed, round)
    //   E: F1=500Hz,  F2=1800Hz  (half open)
    //   O: F1=500Hz,  F2=1000Hz  (round)
    
    float lowEnergy  = [self energyFrom:100 to:400 binWidth:binWidth];   // Low formant region
    float midEnergy  = [self energyFrom:400 to:1000 binWidth:binWidth];  // Mid formant region
    float highMidE   = [self energyFrom:1000 to:1600 binWidth:binWidth]; // High-mid region
    float highEnergy = [self energyFrom:1600 to:2800 binWidth:binWidth]; // High formant region
    float totalEnergy = lowEnergy + midEnergy + highMidE + highEnergy + 0.0001f;
    
    // Normalize ratios
    float lowRatio  = lowEnergy / totalEnergy;
    float midRatio  = midEnergy / totalEnergy;
    float highMidR  = highMidE / totalEnergy;
    float highRatio = highEnergy / totalEnergy;
    
    // Map to vowels based on formant patterns
    float rawA = fminf(1.0f, midRatio * 2.5f + highMidR * 1.5f);       // A: strong mid + high-mid
    float rawI = fminf(1.0f, lowRatio * 1.5f + highRatio * 3.0f);       // I: low F1 + very high F2
    float rawU = fminf(1.0f, lowRatio * 2.0f + midRatio * 1.0f);        // U: low F1 + low F2
    float rawE = fminf(1.0f, midRatio * 1.5f + highRatio * 2.0f);       // E: mid F1 + high F2
    float rawO = fminf(1.0f, midRatio * 2.5f);                          // O: strong mid only
    
    // Overall mouth open = total energy mapped to 0-1
    float rawMouth = fminf(1.0f, sqrtf(totalEnergy) * 8.0f);
    
    // Apply mouth gate: if mouth nearly closed, reduce vowel values
    float mouthGate = rawMouth > 0.1f ? 1.0f : rawMouth / 0.1f;
    rawA *= mouthGate;
    rawI *= mouthGate;
    rawU *= mouthGate;
    rawE *= mouthGate;
    rawO *= mouthGate;
    
    // Smooth values (exponential moving average)
    float alpha = 0.3f;
    _smoothA = _smoothA + alpha * (rawA - _smoothA);
    _smoothI = _smoothI + alpha * (rawI - _smoothI);
    _smoothU = _smoothU + alpha * (rawU - _smoothU);
    _smoothE = _smoothE + alpha * (rawE - _smoothE);
    _smoothO = _smoothO + alpha * (rawO - _smoothO);
    _smoothMouth = _smoothMouth + alpha * (rawMouth - _smoothMouth);
    
    // Output
    _vowelA = _smoothA;
    _vowelI = _smoothI;
    _vowelU = _smoothU;
    _vowelE = _smoothE;
    _vowelO = _smoothO;
    _mouthOpenY = _smoothMouth;
}

- (float)energyFrom:(float)freqLow to:(float)freqHigh binWidth:(float)binWidth {
    int binLow = (int)(freqLow / binWidth);
    int binHigh = (int)(freqHigh / binWidth);
    binLow = MAX(0, MIN(binLow, kFFTSizeOver2 - 1));
    binHigh = MAX(binLow, MIN(binHigh, kFFTSizeOver2 - 1));
    
    float sum = 0;
    for (int i = binLow; i <= binHigh; i++) {
        sum += _sharedMagnitudes[i];
    }
    return sum / (float)(binHigh - binLow + 1);
}

@end
