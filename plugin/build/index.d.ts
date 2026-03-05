import { ConfigPlugin } from '@expo/config-plugins';
/**
 * Define the plugin properties.
 * Expected to receive a property `modelPath` relative to the project root.
 */
export interface Live2DModelsPluginProps {
    modelPath: string;
}
/**
 * Main Plugin Export
 */
declare const withLive2DModels: ConfigPlugin<Live2DModelsPluginProps>;
export default withLive2DModels;
