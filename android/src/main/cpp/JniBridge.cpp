#include <jni.h>
#include <android/asset_manager_jni.h>
#include <unordered_map>
#include <memory>

#include "PlatformAndroid.hpp"
#include "L2DModel.hpp"
#include "L2DSDKInit.hpp"

// ============================================================
// Handle management
// ============================================================
static std::unordered_map<jlong, std::unique_ptr<L2DModel>> g_models;
static jlong g_nextHandle = 1;
static PlatformAndroid* g_platform = nullptr;

// ============================================================
// JNI functions
// ============================================================
extern "C" {

JNIEXPORT void JNICALL
Java_expo_modules_live2d_NativeBridge_nativeInitSDK(
    JNIEnv* env, jobject /* thiz */, jobject assetManager
) {
    if (g_platform) return; // Already initialized
    g_platform = new PlatformAndroid(env, assetManager);
    L2DInitializeSDK(g_platform);
}

JNIEXPORT void JNICALL
Java_expo_modules_live2d_NativeBridge_nativeSetTextureLoader(
    JNIEnv* env, jobject /* thiz */, jobject callback
) {
    if (g_platform) {
        g_platform->SetTextureLoaderCallback(env, callback);
    }
}

JNIEXPORT jlong JNICALL
Java_expo_modules_live2d_NativeBridge_nativeCreate(
    JNIEnv* env, jobject /* thiz */, jstring modelPath
) {
    if (!g_platform) return 0;

    const char* path = env->GetStringUTFChars(modelPath, nullptr);
    std::string pathStr(path);
    env->ReleaseStringUTFChars(modelPath, path);

    // Load settings file
    auto settingData = g_platform->LoadFile(pathStr);
    if (settingData.empty()) return 0;

    // Extract model directory from path
    std::string modelDir = pathStr.substr(0, pathStr.find_last_of('/'));

    auto model = std::make_unique<L2DModel>(g_platform);
    if (!model->LoadModelData(settingData.data(),
                              static_cast<unsigned int>(settingData.size()),
                              modelDir)) {
        return 0;
    }

    jlong handle = g_nextHandle++;
    g_models[handle] = std::move(model);
    return handle;
}

JNIEXPORT void JNICALL
Java_expo_modules_live2d_NativeBridge_nativeDestroy(
    JNIEnv* /* env */, jobject /* thiz */, jlong handle
) {
    g_models.erase(handle);
}

JNIEXPORT void JNICALL
Java_expo_modules_live2d_NativeBridge_nativeOnSurfaceCreated(
    JNIEnv* /* env */, jobject /* thiz */, jlong handle
) {
    auto it = g_models.find(handle);
    if (it != g_models.end()) {
        it->second->SetupGLResources();
    }
}

JNIEXPORT void JNICALL
Java_expo_modules_live2d_NativeBridge_nativeOnDrawFrame(
    JNIEnv* /* env */, jobject /* thiz */, jlong handle,
    jfloat width, jfloat height, jfloat scale
) {
    auto it = g_models.find(handle);
    if (it == g_models.end()) return;

    auto* model = it->second.get();
    if (!model->IsModelLoaded()) return;

    if (!model->IsReadyToDraw()) {
        model->SetupGLResources();
        if (!model->IsReadyToDraw()) return;
    }

    CubismMatrix44 projection;
    if (width > 0 && height > 0) {
        float aspect = width / height;
        if (aspect > 1.0f) {
            projection.Scale(scale / aspect, scale);
        } else {
            projection.Scale(scale, scale * aspect);
        }
    }

    model->Draw(projection);
}

JNIEXPORT void JNICALL
Java_expo_modules_live2d_NativeBridge_nativeSetDrag(
    JNIEnv* /* env */, jobject /* thiz */, jlong handle,
    jfloat x, jfloat y
) {
    auto it = g_models.find(handle);
    if (it != g_models.end()) {
        it->second->SetDragging(x, y);
    }
}

JNIEXPORT jstring JNICALL
Java_expo_modules_live2d_NativeBridge_nativePlayMotion(
    JNIEnv* env, jobject /* thiz */, jlong handle,
    jstring group, jint index
) {
    auto it = g_models.find(handle);
    if (it == g_models.end()) return env->NewStringUTF("");

    const char* groupStr = env->GetStringUTFChars(group, nullptr);
    std::string soundPath = it->second->StartMotionByGroupIndex(groupStr, index);
    env->ReleaseStringUTFChars(group, groupStr);

    return env->NewStringUTF(soundPath.c_str());
}

JNIEXPORT jint JNICALL
Java_expo_modules_live2d_NativeBridge_nativeGetMotionCount(
    JNIEnv* env, jobject /* thiz */, jlong handle, jstring group
) {
    auto it = g_models.find(handle);
    if (it == g_models.end()) return 0;

    const char* groupStr = env->GetStringUTFChars(group, nullptr);
    jint count = it->second->GetMotionCount(groupStr);
    env->ReleaseStringUTFChars(group, groupStr);
    return count;
}

JNIEXPORT void JNICALL
Java_expo_modules_live2d_NativeBridge_nativeSetExpression(
    JNIEnv* env, jobject /* thiz */, jlong handle, jstring name
) {
    auto it = g_models.find(handle);
    if (it == g_models.end()) return;

    const char* nameStr = env->GetStringUTFChars(name, nullptr);
    it->second->SetExpression(nameStr);
    env->ReleaseStringUTFChars(name, nameStr);
}

JNIEXPORT void JNICALL
Java_expo_modules_live2d_NativeBridge_nativeSetMouthOpenY(
    JNIEnv* /* env */, jobject /* thiz */, jlong handle, jfloat value
) {
    auto it = g_models.find(handle);
    if (it != g_models.end()) {
        it->second->SetMouthOpenY(value);
    }
}

JNIEXPORT void JNICALL
Java_expo_modules_live2d_NativeBridge_nativeSetVowelValues(
    JNIEnv* /* env */, jobject /* thiz */, jlong handle,
    jfloat a, jfloat i, jfloat u, jfloat e, jfloat o
) {
    auto it = g_models.find(handle);
    if (it != g_models.end()) {
        it->second->SetVowelValues(a, i, u, e, o);
    }
}

} // extern "C"
