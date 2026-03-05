package expo.modules.live2d

import expo.modules.kotlin.modules.Module
import expo.modules.kotlin.modules.ModuleDefinition

class ExpoLive2DModule : Module() {
    override fun definition() = ModuleDefinition {
        Name("ExpoLive2D")

        View(ExpoLive2DView::class) {
            Prop("modelPath") { view: ExpoLive2DView, path: String ->
                view.setModelPath(path)
            }

            Prop("scale") { view: ExpoLive2DView, scale: Float ->
                view.setScale(scale)
            }

            Prop("gyroEnabled") { view: ExpoLive2DView, enabled: Boolean ->
                view.setGyroEnabled(enabled)
            }

            Events("onLoad")

            AsyncFunction("playMotion") { view: ExpoLive2DView, group: String, index: Int ->
                view.playMotion(group, index)
            }

            AsyncFunction("getMotionCount") { view: ExpoLive2DView, group: String ->
                view.getMotionCount(group)
            }

            AsyncFunction("setExpression") { view: ExpoLive2DView, name: String ->
                view.setExpression(name)
            }
        }
    }
}
