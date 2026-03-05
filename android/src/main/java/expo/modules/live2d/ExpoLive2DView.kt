package expo.modules.live2d

import android.annotation.SuppressLint
import android.content.Context
import android.opengl.GLSurfaceView
import android.util.Log
import android.view.MotionEvent
import expo.modules.kotlin.AppContext
import expo.modules.kotlin.views.ExpoView

/**
 * Native Android view wrapping GLSurfaceView for Live2D rendering.
 * Handles touch input for drag control and delegates rendering to Live2DRenderer.
 */
@SuppressLint("ViewConstructor")
class ExpoLive2DView(context: Context, appContext: AppContext) : ExpoView(context, appContext) {

    val renderer = Live2DRenderer(context)
    private val glView: GLSurfaceView
    val audioAnalyzer = AudioAnalyzer(context)

    init {
        glView = GLSurfaceView(context).apply {
            setEGLContextClientVersion(2)
            setRenderer(renderer)
            renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
        }
        addView(glView, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))

        // Connect audio analyzer to renderer for lip sync
        renderer.audioAnalyzer = audioAnalyzer
    }

    fun setModelPath(path: String) {
        if (path.isBlank()) return
        Log.d(TAG, "setModelPath: $path")
        renderer.loadModel(path)
    }

    fun setScale(scale: Float) {
        renderer.setScale(scale)
    }

    fun setGyroEnabled(enabled: Boolean) {
        // TODO: Phase 3 — implement gyroscope sensor integration
        Log.d(TAG, "setGyroEnabled: $enabled (not yet implemented)")
    }

    fun playMotion(group: String, index: Int): String {
        if (renderer.nativeHandle == 0L) return ""
        // Queue motion on GL thread, play audio with analyzer on main thread
        renderer.queueCommand {
            val soundPath = NativeBridge.nativePlayMotion(renderer.nativeHandle, group, index)
            if (soundPath.isNotEmpty()) {
                post { audioAnalyzer.play(soundPath) }
            }
        }
        return ""
    }

    fun getMotionCount(group: String): Int {
        if (renderer.nativeHandle == 0L) return 0
        return NativeBridge.nativeGetMotionCount(renderer.nativeHandle, group)
    }

    fun setExpression(name: String) {
        if (renderer.nativeHandle == 0L) return
        renderer.queueCommand {
            NativeBridge.nativeSetExpression(renderer.nativeHandle, name)
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (renderer.nativeHandle == 0L) return super.onTouchEvent(event)

        val viewX = event.x / width * 2.0f - 1.0f
        val viewY = -(event.y / height * 2.0f - 1.0f)

        when (event.action) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE -> {
                renderer.queueCommand {
                    NativeBridge.nativeSetDrag(renderer.nativeHandle, viewX, viewY)
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                renderer.queueCommand {
                    NativeBridge.nativeSetDrag(renderer.nativeHandle, 0f, 0f)
                }
            }
        }
        return true
    }

    fun cleanup() {
        audioAnalyzer.release()
        glView.queueEvent {
            renderer.destroy()
        }
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        cleanup()
    }

    companion object {
        private const val TAG = "ExpoLive2DView"
    }
}
