package expo.modules.live2d

import android.content.Context
import android.graphics.BitmapFactory
import android.opengl.GLES20
import android.opengl.GLSurfaceView
import android.opengl.GLUtils
import android.util.Log
import java.util.concurrent.ConcurrentLinkedQueue
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

/**
 * GLSurfaceView.Renderer with command queue + state machine.
 * All JNI calls happen on the GL thread via the command queue.
 */
class Live2DRenderer(private val context: Context) : GLSurfaceView.Renderer {

    enum class State { IDLE, LOADING, READY, ERROR }

    private val commandQueue = ConcurrentLinkedQueue<() -> Unit>()
    var nativeHandle: Long = 0
        private set
    var state = State.IDLE
        private set
    private var viewWidth = 0f
    private var viewHeight = 0f
    private var currentScale = 1.0f
    private var sdkInitialized = false
    var audioAnalyzer: AudioAnalyzer? = null

    fun queueCommand(cmd: () -> Unit) = commandQueue.add(cmd)

    fun setScale(scale: Float) {
        currentScale = scale
    }

    fun loadModel(modelPath: String) {
        queueCommand {
            try {
                state = State.LOADING

                // Initialize SDK if needed
                if (!sdkInitialized) {
                    val assetManager = context.assets
                    NativeBridge.nativeInitSDK(assetManager)
                    NativeBridge.nativeSetTextureLoader(TextureLoader(context))
                    sdkInitialized = true
                    Log.d(TAG, "SDK initialized on GL thread")
                }

                // Destroy previous model
                if (nativeHandle != 0L) {
                    NativeBridge.nativeDestroy(nativeHandle)
                    nativeHandle = 0
                }

                // Create new model
                nativeHandle = NativeBridge.nativeCreate(modelPath)
                if (nativeHandle != 0L) {
                    NativeBridge.nativeOnSurfaceCreated(nativeHandle)
                    state = State.READY
                    Log.d(TAG, "Model loaded: $modelPath (handle=$nativeHandle)")
                } else {
                    state = State.ERROR
                    Log.e(TAG, "Failed to create model: $modelPath")
                }
            } catch (e: Exception) {
                state = State.ERROR
                Log.e(TAG, "Error loading model", e)
            }
        }
    }

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        Log.d(TAG, "Surface created")
        // If model was loaded before surface was created, set up GL resources
        if (nativeHandle != 0L) {
            NativeBridge.nativeOnSurfaceCreated(nativeHandle)
        }
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        viewWidth = width.toFloat()
        viewHeight = height.toFloat()
        GLES20.glViewport(0, 0, width, height)
        Log.d(TAG, "Surface changed: ${width}x${height}")
    }

    override fun onDrawFrame(gl: GL10?) {
        // Consume all queued commands on the GL thread
        while (commandQueue.isNotEmpty()) {
            commandQueue.poll()?.invoke()
        }

        // Update lip sync values from AudioAnalyzer
        audioAnalyzer?.let { analyzer ->
            analyzer.updateFrame()
            if (nativeHandle != 0L) {
                NativeBridge.nativeSetMouthOpenY(nativeHandle, analyzer.mouthOpenY)
                NativeBridge.nativeSetVowelValues(
                    nativeHandle,
                    analyzer.vowelA, analyzer.vowelI, analyzer.vowelU,
                    analyzer.vowelE, analyzer.vowelO
                )
            }
        }

        // Clear
        GLES20.glClearColor(0f, 0f, 0f, 0f)
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT)

        // Draw model
        if (nativeHandle != 0L && state == State.READY) {
            NativeBridge.nativeOnDrawFrame(nativeHandle, viewWidth, viewHeight, currentScale)
        }
    }

    fun destroy() {
        if (nativeHandle != 0L) {
            NativeBridge.nativeDestroy(nativeHandle)
            nativeHandle = 0
        }
        state = State.IDLE
    }

    /**
     * Texture loader callback — called from C++ via JNI.
     * Decodes an image from assets and creates an OpenGL texture.
     */
    class TextureLoader(private val context: Context) {
        fun loadTexture(path: String): Int {
            try {
                val inputStream = context.assets.open(path)

                // Disable premultiplied alpha to match iOS GLKTextureLoader behavior
                val options = BitmapFactory.Options().apply {
                    inPremultiplied = false
                }
                val bitmap = BitmapFactory.decodeStream(inputStream, null, options)
                inputStream.close()

                if (bitmap == null) {
                    Log.e(TAG, "Failed to decode bitmap: $path")
                    return 0
                }

                val texIds = IntArray(1)
                GLES20.glGenTextures(1, texIds, 0)
                GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, texIds[0])
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR)
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR)
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE)
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE)
                GLUtils.texImage2D(GLES20.GL_TEXTURE_2D, 0, bitmap, 0)
                GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0)

                bitmap.recycle()
                Log.d(TAG, "Texture loaded: $path (id=${texIds[0]})")
                return texIds[0]
            } catch (e: Exception) {
                Log.e(TAG, "Failed to load texture: $path", e)
                return 0
            }
        }
    }

    companion object {
        private const val TAG = "Live2DRenderer"
    }
}
