import {
  ConfigPlugin,
  withDangerousMod,
  withXcodeProject,
  IOSConfig,
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
      // Manually add the folder reference to bypass xcode library's `addResourceFile` bug
      // where it attempts to correct paths for 'Resources' group and crashes on null.
      const pbxFile = require('xcode/lib/pbxFile');
      const fileOptions = { target: config.modRequest.projectName, lastKnownFileType: 'folder' };
      const file = new pbxFile('live2d-models', fileOptions);
      file.uuid = xcodeProject.generateUuid();
      file.fileRef = xcodeProject.generateUuid();

      xcodeProject.addToPbxBuildFileSection(file);
      xcodeProject.addToPbxFileReferenceSection(file);
      xcodeProject.addToPbxResourcesBuildPhase(file);

      const projectName = config.modRequest.projectName || 'app';
      const group = IOSConfig.XcodeUtils.ensureGroupRecursively(xcodeProject, projectName);
      if (group && group.children) {
        group.children.push({ value: file.fileRef, comment: file.basename });
      }

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
