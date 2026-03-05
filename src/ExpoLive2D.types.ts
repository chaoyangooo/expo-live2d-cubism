import type { StyleProp, ViewStyle } from 'react-native';

export type ExpoLive2DViewProps = {
  modelPath: string;
  scale?: number;
  gyroEnabled?: boolean;
  onLoad?: (event: { nativeEvent: { url: string } }) => void;
  style?: StyleProp<ViewStyle>;
};

export type ExpoLive2DViewRef = {
  playMotion: (group: string, index: number) => Promise<void>;
  getMotionCount: (group: string) => Promise<number>;
  setExpression: (name: string) => Promise<void>;
};
