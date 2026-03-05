#pragma once

#include <vector>
#include <string>
#include <cstdint>

/// Cross-platform abstraction interface for file I/O, texture loading, and system utilities.
/// Each platform (iOS, Android) provides its own implementation.
struct IPlatform {
    virtual ~IPlatform() = default;

    /// Load a file from the given path and return its contents as a byte vector.
    /// Handles both absolute paths and relative paths (platform resolves search locations).
    /// Returns an empty vector on failure.
    virtual std::vector<uint8_t> LoadFile(const std::string& path) = 0;

    /// Load a texture from the given absolute path and create an OpenGL texture.
    /// Returns the OpenGL texture ID, or 0 on failure.
    /// The GL context must be current when calling this.
    virtual uint32_t LoadTexture(const std::string& path) = 0;

    /// Check if a file exists at the given path.
    virtual bool FileExists(const std::string& path) = 0;

    /// Get the current time in seconds (monotonic clock).
    virtual double GetTimeSeconds() = 0;

    /// Log a message.
    virtual void Log(const std::string& message) = 0;
};
