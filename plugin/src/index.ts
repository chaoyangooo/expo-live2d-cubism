import {
  ConfigPlugin,
  withDangerousMod,
  withXcodeProject,
} from '@expo/config-plugins';
import path from 'path';
import fs from 'fs-extra';

/**
 * Define the plugin properties.
 * Expected to receive a property `modelPath` relative to the project root.
 */
export interface Live2DModelsPluginProps {
  modelPath: string;
}

/**
 * Android Plugin: Copies model files to the android assets directory.
 */
const withLive2DModelsAndroid: ConfigPlugin<Live2DModelsPluginProps> = (
  config,
  { modelPath }
) => {
  return withDangerousMod(config, [
    'android',
    async (config) => {
      const projectRoot = config.modRequest.projectRoot;
      const srcPath = path.resolve(projectRoot, modelPath);
      const destPath = path.resolve(
        config.modRequest.platformProjectRoot,
        'app/src/main/assets/live2d-models'
      );

      if (fs.existsSync(srcPath)) {
        // Ensure destination directory exists
        fs.ensureDirSync(destPath);
        // Copy files
        fs.copySync(srcPath, destPath, { overwrite: true });
        console.log(`[expo-live2d-cubism] Copied models to Android assets: ${destPath}`);
      } else {
        console.warn(
          `[expo-live2d-cubism] Warning: Source model path "${srcPath}" does not exist. Skipping Android copy.`
        );
      }

      return config;
    },
  ]);
};

/**
 * iOS Plugin: Copies model files to the Xcode project and adds a Folder Reference.
 */
const withLive2DModelsIos: ConfigPlugin<Live2DModelsPluginProps> = (
  config,
  { modelPath }
) => {
  // 1. Copy files using withDangerousMod because we need file system access
  config = withDangerousMod(config, [
    'ios',
    async (config) => {
      const projectRoot = config.modRequest.projectRoot;
      const srcPath = path.resolve(projectRoot, modelPath);
      const destPath = path.resolve(
        config.modRequest.platformProjectRoot,
        'live2d-models'
      );

      if (fs.existsSync(srcPath)) {
        fs.ensureDirSync(destPath);
        fs.copySync(srcPath, destPath, { overwrite: true });
        console.log(`[expo-live2d-cubism] Copied models to iOS project: ${destPath}`);
      } else {
        console.warn(
          `[expo-live2d-cubism] Warning: Source model path "${srcPath}" does not exist. Skipping iOS copy.`
        );
      }

      return config;
    },
  ]);

  // 2. Add Xcode project reference using withXcodeProject
  return withXcodeProject(config, async (config) => {
    const xcodeProject = config.modResults;
    const projectRoot = config.modRequest.projectRoot;
    const srcPath = path.resolve(projectRoot, modelPath);

    if (fs.existsSync(srcPath)) {
      // Add the folder reference if it doesn't already exist.
      // addResourceFile internally handles adding to the main group and target.
      // passing `isFolderRef: true` makes it a blue folder.
      xcodeProject.addResourceFile(
        'live2d-models',
        { target: config.modRequest.projectName },
        config.modRequest.projectName
      );
      console.log(`[expo-live2d-cubism] Added folder reference 'live2d-models' to Xcode project.`);
    }

    return config;
  });
};

/**
 * Main Plugin Export
 */
const withLive2DModels: ConfigPlugin<Live2DModelsPluginProps> = (
  config,
  props
) => {
  if (!props || !props.modelPath) {
    console.warn(
      '[expo-live2d-cubism] "modelPath" property is required. Skipping plugin configuration.'
    );
    return config;
  }

  config = withLive2DModelsAndroid(config, props);
  config = withLive2DModelsIos(config, props);

  return config;
};

export default withLive2DModels;
