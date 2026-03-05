"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const config_plugins_1 = require("@expo/config-plugins");
const path_1 = __importDefault(require("path"));
const fs_extra_1 = __importDefault(require("fs-extra"));
/**
 * Android Plugin: Copies model files to the android assets directory.
 */
const withLive2DModelsAndroid = (config, { modelPath }) => {
    return (0, config_plugins_1.withDangerousMod)(config, [
        'android',
        async (config) => {
            const projectRoot = config.modRequest.projectRoot;
            const srcPath = path_1.default.resolve(projectRoot, modelPath);
            const destPath = path_1.default.resolve(config.modRequest.platformProjectRoot, 'app/src/main/assets/live2d-models');
            if (fs_extra_1.default.existsSync(srcPath)) {
                // Ensure destination directory exists
                fs_extra_1.default.ensureDirSync(destPath);
                // Copy files
                fs_extra_1.default.copySync(srcPath, destPath, { overwrite: true });
                console.log(`[expo-live2d-cubism] Copied models to Android assets: ${destPath}`);
            }
            else {
                console.warn(`[expo-live2d-cubism] Warning: Source model path "${srcPath}" does not exist. Skipping Android copy.`);
            }
            return config;
        },
    ]);
};
/**
 * iOS Plugin: Copies model files to the Xcode project and adds a Folder Reference.
 */
const withLive2DModelsIos = (config, { modelPath }) => {
    // 1. Copy files using withDangerousMod because we need file system access
    config = (0, config_plugins_1.withDangerousMod)(config, [
        'ios',
        async (config) => {
            const projectRoot = config.modRequest.projectRoot;
            const srcPath = path_1.default.resolve(projectRoot, modelPath);
            const destPath = path_1.default.resolve(config.modRequest.platformProjectRoot, 'live2d-models');
            if (fs_extra_1.default.existsSync(srcPath)) {
                fs_extra_1.default.ensureDirSync(destPath);
                fs_extra_1.default.copySync(srcPath, destPath, { overwrite: true });
                console.log(`[expo-live2d-cubism] Copied models to iOS project: ${destPath}`);
            }
            else {
                console.warn(`[expo-live2d-cubism] Warning: Source model path "${srcPath}" does not exist. Skipping iOS copy.`);
            }
            return config;
        },
    ]);
    // 2. Add Xcode project reference using withXcodeProject
    return (0, config_plugins_1.withXcodeProject)(config, async (config) => {
        const xcodeProject = config.modResults;
        const projectRoot = config.modRequest.projectRoot;
        const srcPath = path_1.default.resolve(projectRoot, modelPath);
        if (fs_extra_1.default.existsSync(srcPath)) {
            // Add the folder reference if it doesn't already exist.
            // addResourceFile internally handles adding to the main group and target.
            // passing `isFolderRef: true` makes it a blue folder.
            xcodeProject.addResourceFile('live2d-models', { target: config.modRequest.projectName }, config.modRequest.projectName);
            console.log(`[expo-live2d-cubism] Added folder reference 'live2d-models' to Xcode project.`);
        }
        return config;
    });
};
/**
 * Main Plugin Export
 */
const withLive2DModels = (config, props) => {
    if (!props || !props.modelPath) {
        console.warn('[expo-live2d-cubism] "modelPath" property is required. Skipping plugin configuration.');
        return config;
    }
    config = withLive2DModelsAndroid(config, props);
    config = withLive2DModelsIos(config, props);
    return config;
};
exports.default = withLive2DModels;
