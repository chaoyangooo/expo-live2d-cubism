# 中文说明

[English](./README.md) | [中文说明](./README-zh.md)

这是一个为 Expo 和 React Native 打造的高性能跨平台 Live2D 原生模块。
基于 Live2D Cubism 官方 SDK 构建，提供包括底层 C++ 跨平台渲染、平滑唇形同步、动画与表情播放等全套完整功能。

> **注意：** 由于官方 Live2D Cubism Core 引擎依赖特定商业授权并闭源，本 npm 包**不包含**核心静态库！你需要自行前往官网下载 SDK Core 组件并注入到你的项目中才能成功编译。

## 核心特性

- 🚀 **完整的前端跨平台：** 统一的 Shared C++ 架构，抹平 iOS 与 Android 渲染差异。
- 🎙️ **实时唇形同步 (Lip-sync)：** 内置双轨策略（RMS 容量计算控制开合 + FFT 频域分析提取元音），实现媲美原生底层的 60fps 丝滑唇形同步。
- 🎬 **动作与表情：** 完美支持 `.motion3.json` 和 `.exp3.json` 属性读取与触发。
- 📱 **硬件加速渲染：** 基于 Android GLSurfaceView 和 iOS GLKView 的底层原生渲染支持。

---

## 安装使用

```bash
npx expo install expo-live2d-cubism
```

### 先决条件：注入 Live2D Cubism Core

1. 前往官方页面下载 [Cubism SDK for Native](https://www.live2d.com/en/sdk/download/native/)。
2. 解压缩下载的 SDK 文件。
3. 提取 `Core` 文件夹中的预编译静态库，将其补全到本地 `node_modules` 中：
   - **iOS:** 将 `live2dcubismcore.framework` 复制到 `node_modules/expo-live2d-cubism/ios/Vendor/`
   - **Android:** 将对应 ABI 的 `libLive2DCubismCore.a` 放入 `node_modules/expo-live2d-cubism/android/Vendor/Core/lib/`

### 编译运行

本模块深度依赖原生 C++ 代码及 JNI/OC，无法在纯 Expo Go 环境运行，请编译为开发包：

```bash
npx expo run:ios
npx expo run:android
```

---

## 示例代码

```tsx
import { useRef } from 'react';
import { StyleSheet, View, Button } from 'react-native';
import { ExpoLive2DView } from 'expo-live2d-cubism';

export default function App() {
  const live2dRef = useRef(null);

  return (
    <View style={styles.container}>
      <ExpoLive2DView
        ref={live2dRef}
        style={styles.live2d}
        // modelPath 支持打包资源 assets 或沙盒绝对路径
        modelPath="live2d_models/kei_vowels_pro/kei_vowels_pro.model3.json"
        scale={1.5}
        gyroEnabled={true} // 开启陀螺仪跟随
        onLoad={() => console.log('模型加载完毕！')}
      />

      <Button title="播放闲置动画 1" onPress={() => live2dRef.current?.playMotion('Idle', 0)} />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#333' },
  live2d: { width: '100%', height: 400 },
});
```

---

## 授权说明与许可证

本项目的包装代码（React Native Bridge / JS 侧交互）基于 MIT 协议开源。
任何引入 `Cubism SDK` 的编译及发布行为，必须遵循官方 [Live2D Open Software License Agreement](https://www.live2d.com/en/license/lia/)。
