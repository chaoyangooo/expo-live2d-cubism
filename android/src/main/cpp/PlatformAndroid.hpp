#pragma once

#include "Platform.hpp"
#include <jni.h>
#include <android/asset_manager.h>

/// Android platform implementation of IPlatform.
/// Uses JNI callbacks for file I/O and texture loading, __android_log_print for logging.
class PlatformAndroid : public IPlatform {
public:
    PlatformAndroid(JNIEnv* env, jobject assetManagerObj);
    ~PlatformAndroid();

    std::vector<uint8_t> LoadFile(const std::string& path) override;
    uint32_t LoadTexture(const std::string& path) override;
    bool FileExists(const std::string& path) override;
    double GetTimeSeconds() override;
    void Log(const std::string& message) override;

    /// Set JNI references for texture loading callback
    void SetTextureLoaderCallback(JNIEnv* env, jobject callbackObj);

private:
    AAssetManager* _assetManager;
    jobject _textureCallback;   // Global ref to Java texture loader
    JavaVM* _jvm;
    
    JNIEnv* GetEnv();
};
