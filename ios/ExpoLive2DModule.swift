import ExpoModulesCore

public class ExpoLive2DModule: Module {
  public func definition() -> ModuleDefinition {
    Name("ExpoLive2D")

    View(ExpoLive2DView.self) {
      Prop("modelPath") { (view: ExpoLive2DView, prop: String) in
        view.setModelPath(prop)
      }

      Prop("scale") { (view: ExpoLive2DView, scale: Float) in
        view.setModelScale(scale)
      }

      Prop("gyroEnabled") { (view: ExpoLive2DView, enabled: Bool) in
        view.setGyroEnabled(enabled)
      }

      Events("onLoad")

      // Play a motion by group and index
      AsyncFunction("playMotion") { (view: ExpoLive2DView, group: String, index: Int) in
        view.playMotion(group: group, index: index)
      }

      // Get motion count for a group
      AsyncFunction("getMotionCount") { (view: ExpoLive2DView, group: String) -> Int in
        return view.getMotionCount(group: group)
      }

      // Set expression preset
      AsyncFunction("setExpression") { (view: ExpoLive2DView, name: String) in
        view.setExpression(name)
      }
    }
  }
}
