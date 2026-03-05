#import "PlatformIOS.hpp"
#import <Foundation/Foundation.h>
#import <GLKit/GLKit.h>
#import <QuartzCore/QuartzCore.h>

// ============================================================
// Construction
// ============================================================

PlatformIOS::PlatformIOS(NSBundle* resourceBundle)
    : _resourceBundle((__bridge void*)resourceBundle)
{}

// ============================================================
// Path resolution
// ============================================================

static NSString* ResolveFilePath(const std::string& path, void* resourceBundlePtr) {
    NSString* nsPath = [NSString stringWithUTF8String:path.c_str()];
    NSBundle* resourceBundle = (__bridge NSBundle*)resourceBundlePtr;

    // Absolute path — use directly
    if ([nsPath hasPrefix:@"/"]) {
        return nsPath;
    }

    // Relative path — search in resource bundle, then main bundle
    NSString* fileName = [nsPath lastPathComponent];
    NSString* fullPath = nil;

    if (resourceBundle) {
        fullPath = [[resourceBundle resourcePath] stringByAppendingPathComponent:nsPath];
        if (![[NSFileManager defaultManager] fileExistsAtPath:fullPath]) {
            fullPath = [[resourceBundle resourcePath] stringByAppendingPathComponent:fileName];
        }
    }
    if (!fullPath || ![[NSFileManager defaultManager] fileExistsAtPath:fullPath]) {
        fullPath = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:nsPath];
    }
    if (!fullPath || ![[NSFileManager defaultManager] fileExistsAtPath:fullPath]) {
        fullPath = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:fileName];
    }

    return fullPath ?: nsPath;
}

// ============================================================
// IPlatform implementation
// ============================================================

std::vector<uint8_t> PlatformIOS::LoadFile(const std::string& path) {
    NSString* resolvedPath = ResolveFilePath(path, _resourceBundle);
    NSData* data = [NSData dataWithContentsOfFile:resolvedPath];
    if (!data || data.length == 0) {
        return {};
    }
    const uint8_t* bytes = static_cast<const uint8_t*>(data.bytes);
    return std::vector<uint8_t>(bytes, bytes + data.length);
}

uint32_t PlatformIOS::LoadTexture(const std::string& path) {
    NSString* resolvedPath = ResolveFilePath(path, _resourceBundle);
    NSError* error = nil;
    GLKTextureInfo* info = [GLKTextureLoader textureWithContentsOfFile:resolvedPath
                                                              options:nil error:&error];
    if (!info) {
        NSLog(@"[Live2D] PlatformIOS::LoadTexture failed: %@ (error: %@)",
              [resolvedPath lastPathComponent], error.localizedDescription);
        return 0;
    }

    // Set texture parameters
    glBindTexture(GL_TEXTURE_2D, info.name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return info.name;
}

bool PlatformIOS::FileExists(const std::string& path) {
    NSString* nsPath = [NSString stringWithUTF8String:path.c_str()];
    return [[NSFileManager defaultManager] fileExistsAtPath:nsPath];
}

double PlatformIOS::GetTimeSeconds() {
    return CACurrentMediaTime();
}

void PlatformIOS::Log(const std::string& message) {
    NSLog(@"%s", message.c_str());
}
