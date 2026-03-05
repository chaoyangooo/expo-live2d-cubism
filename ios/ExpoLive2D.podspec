Pod::Spec.new do |s|
  s.name           = 'ExpoLive2D'
  s.version        = '1.0.0'
  s.summary        = 'Live2D Cubism rendering for React Native via Expo Modules'
  s.description    = 'Native iOS module that renders Live2D models using OpenGL ES 2.0'
  s.author         = ''
  s.homepage       = 'https://docs.expo.dev/modules/'
  s.platforms      = { :ios => '15.1' }
  s.source         = { git: '' }
  s.static_framework = true

  s.dependency 'ExpoModulesCore'

  s.compiler_flags = '-fcxx-modules'

  # Auto-copy shared C++ files from cpp/shared/ into ios/ on pod install
  s.prepare_command = <<-CMD
    cp -f ../cpp/shared/*.hpp ../cpp/shared/*.cpp .
  CMD
  
  s.pod_target_xcconfig = {
    'DEFINES_MODULE' => 'YES',
    'SWIFT_COMPILATION_MODE' => 'wholemodule',
    'GCC_C_LANGUAGE_STANDARD' => 'c11',
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++14',
    'CLANG_CXX_LIBRARY' => 'libc++',
    'HEADER_SEARCH_PATHS' => '"${PODS_TARGET_SRCROOT}/Vendor/Core/include" "${PODS_TARGET_SRCROOT}/Vendor/Framework/src" "${PODS_TARGET_SRCROOT}"',
    'GCC_PREPROCESSOR_DEFINITIONS' => 'CSM_TARGET_IPHONE_ES2=1'
  }

  # All local source: Swift/ObjC++ bridge + shared C++ (copied by prepare_command) + Framework
  s.source_files = "*.{h,hpp,m,mm,swift,cpp}",
                   "Vendor/Framework/src/**/*.cpp"
  
  # Mark C++ headers as private to exclude from the umbrella header
  s.private_header_files = "*.hpp",
                           "Vendor/Framework/src/**/*.hpp"
  
  # Exclude non-iOS rendering backends
  s.exclude_files = "Vendor/Framework/src/Rendering/D3D9/*",
                    "Vendor/Framework/src/Rendering/D3D11/*",
                    "Vendor/Framework/src/Rendering/Vulkan/*",
                    "Vendor/Framework/src/Rendering/Metal/*"
  
  s.preserve_paths = "Vendor/Core/include/**/*.h",
                     "Vendor/Framework/src/**/*.{hpp,h}",
                     "*.hpp"
  
  # Runtime resources: model assets + GLSL shader files
  s.resource_bundles = {
    'ExpoLive2DAssets' => [
      'Resources/**/*',
      'Vendor/Framework/src/Rendering/OpenGL/Shaders/StandardES/*.{vert,frag}'
    ]
  }
  
  # Pre-compiled Live2D Core static library
  s.vendored_libraries = "Vendor/Core/lib/Release-iphoneos/libLive2DCubismCore.a"

end
