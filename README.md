# expo-live2d-cubism

[English](./README.md) | [中文说明](./README-zh.md)

A high-performance, cross-platform Live2D native module for Expo and React Native, utilizing the official Live2D Cubism SDK.
Provides seamless iOS and Android support with smooth lip-syncing, motion playback, expressions, and hardware-accelerated rendering.

> **Note:** The official Live2D Cubism Core SDK (`Live2DCubismCore`) is closed-source geometry and relies on an individual licensing agreement with Live2D Inc.
> Therefore, this npm package **does not** bundle the core static libraries. You must download them from the official website and inject them into your project to compile successfully.

## Features

- 🚀 **Full cross-platform support:** Shared C++ architecture for iOS and Android.
- 🎙️ **Real-time Lip-sync:** Advanced dual-track approach (RMS volume + FFT vowel mapping) for ultra-smooth 60fps mouth animation syncing to audio.
- 🎬 **Motions & Expressions:** Full playback support for `.motion3.json` and `.exp3.json`.
- 📱 **Hardware Accelerated:** Native OpenGL ES rendering.

---

## Installation

```bash
npx expo install expo-live2d-cubism
```

### Pre-requisite: Inject Live2D Cubism Core

1. Download the [Cubism SDK for Native](https://www.live2d.com/en/sdk/download/native/) from the official Live2D website.
2. Extract the SDK.
3. You need to copy the `Core` folder into your exact `node_modules` path or use a custom patch script (Recommended).
   - **iOS:** Place `live2dcubismcore.framework` into `node_modules/expo-live2d-cubism/ios/Vendor/`
   - **Android:** Place `libLive2DCubismCore.a` into `node_modules/expo-live2d-cubism/android/Vendor/Core/lib/`

### Build

Since this module relies on native C++ code, you cannot use Expo Go. You must use custom dev clients:

```bash
npx expo run:ios
npx expo run:android
```

---

## Usage

```tsx
import { useState } from 'react';
import { StyleSheet, View } from 'react-native';
import { ExpoLive2DView } from 'expo-live2d-cubism';

export default function App() {
  const [motionPlaying, setMotionPlaying] = useState(false);

  return (
    <View style={styles.container}>
      <ExpoLive2DView
        style={styles.live2d}
        // modelPath can be bundled assets or remote/local filesystem paths
        modelPath="live2d_models/kei_vowels_pro/kei_vowels_pro.model3.json"
        scale={1.5}
        gyroEnabled={true}
        onLoad={() => console.log('Model Loaded!')}
        onPress={() => {
          // Play random idle expression
          const ref = useRef<ExpoLive2DView>(null);
          ref.current?.playMotion('Idle', 0);
        }}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#000' },
  live2d: { width: '100%', height: 500 },
});
```

---

## License

This project's React Native Bindings and module wrapper are licensed under the MIT License.
When compiling or distributing this SDK, you must agree and follow to the official [Live2D Open Software License Agreement](https://www.live2d.com/en/license/lia/).
