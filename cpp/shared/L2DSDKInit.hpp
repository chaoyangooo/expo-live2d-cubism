#pragma once

#include "Platform.hpp"

/// Initialize the Cubism SDK with the given platform implementation.
/// Thread-safe: uses std::call_once internally.
void L2DInitializeSDK(IPlatform* platform);

/// Dispose the Cubism SDK.
void L2DDisposeSDK();

/// Get the current platform instance (set during initialization).
IPlatform* L2DGetPlatform();
