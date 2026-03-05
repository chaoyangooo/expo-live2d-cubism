cmake_minimum_required(VERSION 3.18)
project(live2d-jni)

# ============================================================
# Path definitions
# ============================================================
set(SHARED_CPP_DIR "${CMAKE_SOURCE_DIR}/../../../../cpp/shared")
set(VENDOR_DIR "${CMAKE_SOURCE_DIR}/../../../Vendor")
set(CORE_DIR "${VENDOR_DIR}/Core")
set(FRAMEWORK_DIR "${VENDOR_DIR}/Framework/src")

# ============================================================
# Cubism Core (pre-compiled static library)
# ============================================================
add_library(Live2DCubismCore STATIC IMPORTED)
set_target_properties(Live2DCubismCore PROPERTIES
    IMPORTED_LOCATION "${CORE_DIR}/lib/android/${ANDROID_ABI}/libLive2DCubismCore.a"
)

# ============================================================
# Cubism Framework (compiled from source)
# ============================================================
file(GLOB_RECURSE FRAMEWORK_SRC "${FRAMEWORK_DIR}/*.cpp")

# Exclude non-OpenGL rendering backends
list(FILTER FRAMEWORK_SRC EXCLUDE REGEX ".*/Rendering/D3D9/.*")
list(FILTER FRAMEWORK_SRC EXCLUDE REGEX ".*/Rendering/D3D11/.*")
list(FILTER FRAMEWORK_SRC EXCLUDE REGEX ".*/Rendering/Vulkan/.*")
list(FILTER FRAMEWORK_SRC EXCLUDE REGEX ".*/Rendering/Metal/.*")

add_library(CubismFramework STATIC ${FRAMEWORK_SRC})
target_include_directories(CubismFramework PUBLIC
    "${CORE_DIR}/include"
    "${FRAMEWORK_DIR}"
)
target_compile_definitions(CubismFramework PUBLIC CSM_TARGET_ANDROID_ES2)

# ============================================================
# Shared C++ (cross-platform model logic from cpp/shared/)
# ============================================================
file(GLOB SHARED_SRC "${SHARED_CPP_DIR}/*.cpp")

# ============================================================
# JNI Bridge library
# ============================================================
add_library(live2d-jni SHARED
    JniBridge.cpp
    PlatformAndroid.cpp
    ${SHARED_SRC}
)

target_include_directories(live2d-jni PRIVATE
    "${SHARED_CPP_DIR}"
    "${CORE_DIR}/include"
    "${FRAMEWORK_DIR}"
)

target_link_libraries(live2d-jni
    CubismFramework
    Live2DCubismCore
    GLESv2
    android
    log
    jnigraphics
)
