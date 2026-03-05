#include "L2DSDKInit.hpp"
#include "CubismFramework.hpp"
#include "ICubismAllocator.hpp"

#include <mutex>
#include <cstdlib>
#include <cstring>

using namespace Csm;
using namespace Live2D::Cubism::Framework;

// ============================================================
// Globals
// ============================================================
static IPlatform* g_platform = nullptr;
static bool g_initialized = false;
static std::once_flag g_initFlag;

// ============================================================
// Memory allocator (pure C++, same as original iOS version)
// ============================================================
class L2DAllocator : public ICubismAllocator {
public:
    void* Allocate(const csmSizeType size) override { return malloc(size); }
    void Deallocate(void* memory) override { free(memory); }
    void* AllocateAligned(const csmSizeType size, const csmUint32 alignment) override {
        size_t offset = alignment - 1 + sizeof(void*);
        void* allocation = malloc(size + static_cast<csmUint32>(offset));
        size_t aligned = reinterpret_cast<size_t>(allocation) + sizeof(void*);
        size_t shift = aligned % alignment;
        if (shift) aligned += (alignment - shift);
        void** preamble = reinterpret_cast<void**>(aligned);
        preamble[-1] = allocation;
        return reinterpret_cast<void*>(aligned);
    }
    void DeallocateAligned(void* alignedMemory) override {
        void** p = static_cast<void**>(alignedMemory);
        free(p[-1]);
    }
};

static L2DAllocator s_allocator;

// ============================================================
// File loader callback for CubismFramework
// ============================================================
static csmByte* L2DLoadFile(const std::string filePath, csmSizeInt* outSize) {
    if (!g_platform) {
        *outSize = 0;
        return nullptr;
    }
    auto data = g_platform->LoadFile(filePath);
    if (data.empty()) {
        g_platform->Log("[Live2D] FileLoader FAILED: " + filePath);
        *outSize = 0;
        return nullptr;
    }
    csmSizeInt size = static_cast<csmSizeInt>(data.size());
    csmByte* buffer = static_cast<csmByte*>(malloc(size));
    memcpy(buffer, data.data(), size);
    *outSize = size;
    return buffer;
}

static void L2DReleaseBytes(csmByte* bytes) {
    free(bytes);
}

// ============================================================
// Public API
// ============================================================
void L2DInitializeSDK(IPlatform* platform) {
    std::call_once(g_initFlag, [platform]() {
        g_platform = platform;

        static CubismFramework::Option option;
        option.LogFunction = nullptr;
        option.LoggingLevel = CubismFramework::Option::LogLevel_Verbose;
        option.LoadFileFunction = L2DLoadFile;
        option.ReleaseBytesFunction = L2DReleaseBytes;

        CubismFramework::StartUp(&s_allocator, &option);
        CubismFramework::Initialize();
        g_initialized = true;

        platform->Log("[Live2D] SDK initialized (shared)");
    });
}

void L2DDisposeSDK() {
    if (g_initialized) {
        CubismFramework::Dispose();
        g_initialized = false;
        g_platform = nullptr;
    }
}

IPlatform* L2DGetPlatform() {
    return g_platform;
}
