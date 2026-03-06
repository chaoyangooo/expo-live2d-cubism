import ExpoModulesCore
import GLKit
import OpenGLES
import CoreMotion

class ExpoLive2DView: ExpoView {
  let onLoad = EventDispatcher()
  
  private var glkView: GLKView!
  private var context: EAGLContext!
  private var displayLink: CADisplayLink?
  private var live2dAdapter: Live2DObjCAdapter?
  private var currentScale: Float = 1.0
  private var currentModelPath: String?
  
  // Gyroscope tracking
  private let motionManager = CMMotionManager()
  private var isTouching = false
  private var gyroEnabled = true
  
  required init(appContext: AppContext? = nil) {
    super.init(appContext: appContext)
    clipsToBounds = true
    isUserInteractionEnabled = true
    isMultipleTouchEnabled = true
    
    guard let ctx = EAGLContext(api: .openGLES2) else {
      print("[Live2D] Failed to create EAGLContext")
      return
    }
    context = ctx
    
    let view = GLKView(frame: .zero, context: context)
    view.drawableColorFormat = .RGBA8888
    view.drawableDepthFormat = .formatNone
    view.drawableStencilFormat = .formatNone
    view.isOpaque = false
    view.backgroundColor = .clear
    view.enableSetNeedsDisplay = false
    view.delegate = self
    view.isUserInteractionEnabled = false
    glkView = view
    addSubview(view)
    
    // Native pinch-to-zoom
    let pinch = UIPinchGestureRecognizer(target: self, action: #selector(handlePinch(_:)))
    addGestureRecognizer(pinch)
    
    // Start gyroscope
    startGyroscope()
  }
  
  // MARK: - Gyroscope
  
  private func startGyroscope() {
    guard motionManager.isDeviceMotionAvailable else {
      print("[Live2D] DeviceMotion not available")
      return
    }
    motionManager.deviceMotionUpdateInterval = 1.0 / 30.0
    motionManager.startDeviceMotionUpdates()
    print("[Live2D] Gyroscope started")
  }
  
  private func updateGazeFromGyroscope() {
    guard gyroEnabled, !isTouching,
          let attitude = motionManager.deviceMotion?.attitude else { return }
    
    // pitch: tilting phone forward/backward → Y axis (-1 ~ 1)
    // roll: tilting phone left/right → X axis (-1 ~ 1)
    // In portrait mode, phone held upright:
    //   pitch ~0 = vertical, positive = tilted back
    //   roll ~0 = no tilt, positive = tilted right
    
    let roll = Float(attitude.roll)   // left/right tilt → X
    let pitch = Float(attitude.pitch)  // forward/back tilt → Y
    
    // Map to -1~1 range. Phone at ~90° pitch when held upright.
    // Subtract ~1.2 rad (roughly 70°) as the "neutral" upright position.
    let neutralPitch: Float = 1.2
    let sensitivity: Float = 1.5
    
    let x = max(-1.0, min(1.0, roll * sensitivity))
    let y = max(-1.0, min(1.0, (pitch - neutralPitch) * sensitivity))
    
    live2dAdapter?.setDragX(x, y: y)
  }
  
  // MARK: - Layout & Model
  
  override func layoutSubviews() {
    super.layoutSubviews()
    guard glkView != nil, bounds.width > 0, bounds.height > 0 else { return }
    glkView.frame = bounds
    
    if displayLink == nil {
      let link = CADisplayLink(target: self, selector: #selector(renderFrame))
      link.add(to: .main, forMode: .common)
      displayLink = link
    }
  }
  
  public func setModelPath(_ path: String) {
    // Skip if same path — avoid destroying model on React re-render
    if path == currentModelPath { return }
    currentModelPath = path
    live2dAdapter?.releaseModel()
    live2dAdapter = nil
    live2dAdapter = Live2DObjCAdapter(modelPath: path)
    
    if let adapter = live2dAdapter {
      let groups = adapter.availableMotionGroups()
      onLoad([
        "motionGroups": groups
      ])
    }
  }
  
  public func playMotion(group: String, index: Int) {
    live2dAdapter?.playMotionGroup(group, index: index)
  }
  
  public func getMotionCount(group: String) -> Int {
    return live2dAdapter?.motionCount(forGroup: group) ?? 0
  }
  
  public func setModelScale(_ scale: Float) {
    currentScale = scale
    live2dAdapter?.setScale(scale)
  }
  
  public func setGyroEnabled(_ enabled: Bool) {
    gyroEnabled = enabled
    if !enabled {
      live2dAdapter?.setDragX(0, y: 0)
    }
  }
  
  public func setExpression(_ name: String) {
    live2dAdapter?.setExpression(name)
  }
  
  // MARK: - Pinch
  
  @objc private func handlePinch(_ gesture: UIPinchGestureRecognizer) {
    switch gesture.state {
    case .changed:
      let newScale = max(0.3, min(5.0, currentScale * Float(gesture.scale)))
      live2dAdapter?.setScale(newScale)
    case .ended, .cancelled:
      let newScale = max(0.3, min(5.0, currentScale * Float(gesture.scale)))
      currentScale = newScale
      live2dAdapter?.setScale(newScale)
    default:
      break
    }
    if gesture.state == .ended || gesture.state == .cancelled {
      gesture.scale = 1.0
    }
  }
  
  // MARK: - Render loop
  
  @objc private func renderFrame() {
    guard glkView != nil,
          glkView.bounds.width > 0,
          glkView.bounds.height > 0,
          live2dAdapter != nil else { return }
    
    // Update gaze from gyroscope when not touching
    updateGazeFromGyroscope()
    
    EAGLContext.setCurrent(context)
    glkView.display()
  }
  
  // MARK: - Touch handling (overrides gyro when touching)
  
  override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
    guard let touch = touches.first else { return }
    isTouching = true
    updateDragFromTouch(touch)
  }
  
  override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
    guard let touch = touches.first else { return }
    updateDragFromTouch(touch)
  }
  
  override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
    isTouching = false
    // Don't reset drag — gyroscope will take over smoothly
  }
  
  override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
    isTouching = false
  }
  
  private func updateDragFromTouch(_ touch: UITouch) {
    let point = touch.location(in: self)
    let w = bounds.width
    let h = bounds.height
    guard w > 0, h > 0 else { return }
    
    let normalizedX = Float((point.x / w) * 2.0 - 1.0)
    let normalizedY = Float(1.0 - (point.y / h) * 2.0)
    
    live2dAdapter?.setDragX(normalizedX, y: normalizedY)
  }
  
  // MARK: - Cleanup
  
  override func willMove(toWindow newWindow: UIWindow?) {
    super.willMove(toWindow: newWindow)
    if newWindow == nil {
      cleanup()
    }
  }
  
  private func cleanup() {
    displayLink?.invalidate()
    displayLink = nil
    motionManager.stopDeviceMotionUpdates()
    live2dAdapter?.releaseModel()
    live2dAdapter = nil
    if let ctx = context {
      EAGLContext.setCurrent(ctx)
      EAGLContext.setCurrent(nil)
    }
  }
  
  deinit {
    cleanup()
  }
}

extension ExpoLive2DView: GLKViewDelegate {
  func glkView(_ view: GLKView, drawIn rect: CGRect) {
    glClearColor(0.0, 0.0, 0.0, 0.0)
    glClear(GLbitfield(GL_COLOR_BUFFER_BIT))
    live2dAdapter?.draw(in: rect)
  }
}
