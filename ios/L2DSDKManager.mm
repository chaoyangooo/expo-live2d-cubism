#import "L2DSDKManager.h"
#import "Live2DObjCAdapter.h"
#import "PlatformIOS.hpp"
#include "L2DSDKInit.hpp"

// ============================================================
// Globals (iOS-specific)
// ============================================================
static NSBundle* s_resourceBundle = nil;
static PlatformIOS* s_platformIOS = nil;

NSBundle* L2DGetResourceBundle(void) {
    return s_resourceBundle;
}

// ============================================================
// SDK singleton initialization
// ============================================================
void L2DEnsureSDKInitialized(void) {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // Find resource bundle
        NSBundle *classBundle = [NSBundle bundleForClass:[Live2DObjCAdapter class]];
        NSURL *bundleURL = [classBundle URLForResource:@"ExpoLive2DAssets" withExtension:@"bundle"];
        if (bundleURL) {
            s_resourceBundle = [NSBundle bundleWithURL:bundleURL];
            NSLog(@"[Live2D] Resource bundle: %@", [s_resourceBundle resourcePath]);
        } else {
            NSLog(@"[Live2D] WARNING: ExpoLive2DAssets.bundle not found!");
        }

        // Create platform-specific implementation
        s_platformIOS = new PlatformIOS(s_resourceBundle);

        // Initialize shared SDK
        L2DInitializeSDK(s_platformIOS);
        NSLog(@"[Live2D] SDK initialized (iOS, once)");
    });
}
