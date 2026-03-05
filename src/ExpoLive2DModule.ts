import { NativeModule, requireNativeModule } from 'expo';

declare class ExpoLive2DModule extends NativeModule<Record<string, never>> {}

export default requireNativeModule<ExpoLive2DModule>('ExpoLive2D');
