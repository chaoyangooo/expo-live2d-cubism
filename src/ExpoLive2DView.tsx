import { requireNativeView } from 'expo';
import * as React from 'react';

import { ExpoLive2DViewProps, ExpoLive2DViewRef } from './ExpoLive2D.types';

const NativeView: React.ComponentType<ExpoLive2DViewProps & { ref?: React.Ref<any> }> =
  requireNativeView('ExpoLive2D');

const ExpoLive2DView = React.forwardRef<ExpoLive2DViewRef, ExpoLive2DViewProps>((props, ref) => {
  const nativeRef = React.useRef<any>(null);

  React.useImperativeHandle(ref, () => ({
    async playMotion(group: string, index: number) {
      if (nativeRef.current) {
        await nativeRef.current.playMotion(group, index);
      }
    },
    async getMotionCount(group: string): Promise<number> {
      if (nativeRef.current) {
        return await nativeRef.current.getMotionCount(group);
      }
      return 0;
    },
    async setExpression(name: string) {
      if (nativeRef.current) {
        await nativeRef.current.setExpression(name);
      }
    },
  }));

  return <NativeView ref={nativeRef} {...props} />;
});

ExpoLive2DView.displayName = 'ExpoLive2DView';

export default ExpoLive2DView;
