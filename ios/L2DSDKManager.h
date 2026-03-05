#pragma once

#import <Foundation/Foundation.h>

/// Returns the resource bundle used for shader and model file loading.
/// May be nil if the bundle was not found.
NSBundle* _Nullable L2DGetResourceBundle(void);

/// Ensures the Cubism SDK is initialized (only once, thread-safe).
void L2DEnsureSDKInitialized(void);
