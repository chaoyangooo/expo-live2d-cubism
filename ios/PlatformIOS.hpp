#pragma once

#include "Platform.hpp"

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif

/// iOS platform implementation of IPlatform.
/// Uses NSData, GLKTextureLoader, CACurrentMediaTime, NSLog.
class PlatformIOS : public IPlatform {
public:
#ifdef __OBJC__
    /// Create with optional resource bundle for relative path resolution.
    explicit PlatformIOS(NSBundle* resourceBundle);
#endif

    std::vector<uint8_t> LoadFile(const std::string& path) override;
    uint32_t LoadTexture(const std::string& path) override;
    bool FileExists(const std::string& path) override;
    double GetTimeSeconds() override;
    void Log(const std::string& message) override;

private:
    void* _resourceBundle;  // NSBundle* stored as void* for C++ header compat
};
