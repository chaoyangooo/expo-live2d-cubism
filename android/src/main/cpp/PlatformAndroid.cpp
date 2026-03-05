#include "PlatformAndroid.hpp"

#include <android/log.h>
#include <android/asset_manager_jni.h>
#include <android/bitmap.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <chrono>
#include <cstring>

#define LOG_TAG "Live2D"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ============================================================
// Construction / Destruction
// ============================================================

PlatformAndroid::PlatformAndroid(JNIEnv* env, jobject assetManagerObj)
    : _assetManager(nullptr)
    , _textureCallback(nullptr)
    , _jvm(nullptr)
{
    env->GetJavaVM(&_jvm);
    _assetManager = AAssetManager_fromJava(env, assetManagerObj);
    LOGD("PlatformAndroid created (AAssetManager=%p)", _assetManager);
}

PlatformAndroid::~PlatformAndroid() {
    if (_textureCallback) {
        JNIEnv* env = GetEnv();
        if (env) env->DeleteGlobalRef(_textureCallback);
    }
}

JNIEnv* PlatformAndroid::GetEnv() {
    JNIEnv* env = nullptr;
    if (_jvm) _jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    return env;
}

void PlatformAndroid::SetTextureLoaderCallback(JNIEnv* env, jobject callbackObj) {
    if (_textureCallback) env->DeleteGlobalRef(_textureCallback);
    _textureCallback = env->NewGlobalRef(callbackObj);
}

// ============================================================
// IPlatform implementation
// ============================================================

std::vector<uint8_t> PlatformAndroid::LoadFile(const std::string& path) {
    if (!_assetManager) {
        LOGE("LoadFile: AAssetManager is null");
        return {};
    }

    AAsset* asset = AAssetManager_open(_assetManager, path.c_str(), AASSET_MODE_BUFFER);

    // Fallback: search in Shaders/ directory (for Cubism Framework shaders)
    if (!asset) {
        // Extract filename from path
        std::string filename = path;
        auto slashPos = filename.find_last_of('/');
        if (slashPos != std::string::npos) filename = filename.substr(slashPos + 1);

        std::string shaderPath = "Shaders/" + filename;
        asset = AAssetManager_open(_assetManager, shaderPath.c_str(), AASSET_MODE_BUFFER);
        if (asset) {
            LOGD("LoadFile: Found in fallback path: %s", shaderPath.c_str());
        }
    }

    if (!asset) {
        LOGE("LoadFile: Failed to open asset: %s", path.c_str());
        return {};
    }

    off_t size = AAsset_getLength(asset);
    std::vector<uint8_t> buffer(size);
    AAsset_read(asset, buffer.data(), size);
    AAsset_close(asset);

    return buffer;
}

uint32_t PlatformAndroid::LoadTexture(const std::string& path) {
    if (!_textureCallback || !_jvm) {
        LOGE("LoadTexture: No texture callback set");
        return 0;
    }

    JNIEnv* env = GetEnv();
    if (!env) return 0;

    // Call Java side: NativeBridge.loadTexture(path) -> returns GL texture ID
    jclass cls = env->GetObjectClass(_textureCallback);
    jmethodID method = env->GetMethodID(cls, "loadTexture", "(Ljava/lang/String;)I");
    if (!method) {
        LOGE("LoadTexture: loadTexture method not found");
        return 0;
    }

    jstring jPath = env->NewStringUTF(path.c_str());
    jint texId = env->CallIntMethod(_textureCallback, method, jPath);
    env->DeleteLocalRef(jPath);
    env->DeleteLocalRef(cls);

    return static_cast<uint32_t>(texId);
}

bool PlatformAndroid::FileExists(const std::string& path) {
    if (!_assetManager) return false;
    AAsset* asset = AAssetManager_open(_assetManager, path.c_str(), AASSET_MODE_UNKNOWN);
    if (asset) {
        AAsset_close(asset);
        return true;
    }
    return false;
}

double PlatformAndroid::GetTimeSeconds() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now.time_since_epoch()).count();
}

void PlatformAndroid::Log(const std::string& message) {
    LOGD("%s", message.c_str());
}
