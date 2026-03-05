package expo.modules.live2d

/**
 * JNI bridge to the native Live2D C++ layer.
 * All methods forward to JniBridge.cpp via handle-based dispatch.
 */
object NativeBridge {
    init {
        System.loadLibrary("live2d-jni")
    }

    // SDK lifecycle
    external fun nativeInitSDK(assetManager: Any)
    external fun nativeSetTextureLoader(callback: Any)

    // Model lifecycle
    external fun nativeCreate(modelPath: String): Long
    external fun nativeDestroy(handle: Long)

    // Rendering
    external fun nativeOnSurfaceCreated(handle: Long)
    external fun nativeOnDrawFrame(handle: Long, width: Float, height: Float, scale: Float)

    // Interaction
    external fun nativeSetDrag(handle: Long, x: Float, y: Float)
    external fun nativePlayMotion(handle: Long, group: String, index: Int): String
    external fun nativeGetMotionCount(handle: Long, group: String): Int
    external fun nativeSetExpression(handle: Long, name: String)

    // Lip sync
    external fun nativeSetMouthOpenY(handle: Long, value: Float)
    external fun nativeSetVowelValues(handle: Long, a: Float, i: Float, u: Float, e: Float, o: Float)
}
