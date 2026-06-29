import UIKit
import SwiftUI

// Hosts the REAL dreamengine on screen (Phase 2). A CADisplayLink ticks once per vsync —
// THAT is the inverted loop: iOS drives us, the engine never owns main(). Each tick we
// call de_frame(t), then blit the engine's software framebuffer (de_framebuffer) as a
// CGImage. sw_cbuf is BOTTOM-UP, so we flip it once via a vertically-mirrored CGContext.
// Nearest-neighbour scaling keeps the lo-fi pixels crisp; resizeAspect letterboxes.
//
// Touch: UIKit gives us points in the VIEW's coordinate space; we map them through the
// same aspect-fit rect the layer uses, into framebuffer pixels, and feed de_touch_*.
final class CanvasView: UIView {
    private var link: CADisplayLink?
    private var start: CFTimeInterval = 0
    private let w = Int(de_screen_w())
    private let h = Int(de_screen_h())
    private let cs = CGColorSpaceCreateDeviceRGB()
    private var flipped: [UInt32]            // top-down scratch the CGImage reads
    private var touchIds: [ObjectIdentifier: Int32] = [:]   // UITouch → stable engine id
    private var nextId: Int32 = 1

    override init(frame: CGRect) {
        flipped = [UInt32](repeating: 0, count: w * h)
        super.init(frame: frame)
        backgroundColor = .black
        isMultipleTouchEnabled = true
        layer.magnificationFilter = .nearest
        layer.contentsGravity = .resizeAspect
        de_init(DE_RENDERER_SOFTWARE)
        start = CACurrentMediaTime()
        AudioEngine.shared.start()           // CoreAudio pulls de_audio_render on its own thread
        let l = CADisplayLink(target: self, selector: #selector(tick))
        l.add(to: .main, forMode: .common)
        link = l
    }
    required init?(coder: NSCoder) { fatalError("not used") }

    @objc private func tick() {
        de_frame(CACurrentMediaTime() - start)
        guard let base = de_framebuffer() else { return }
        // flip bottom-up sw_cbuf → top-down (row y ↔ h-1-y)
        flipped.withUnsafeMutableBufferPointer { dst in
            for y in 0..<h {
                let srcRow = base + (h - 1 - y) * w
                memcpy(dst.baseAddress! + y * w, srcRow, w * 4)
            }
        }
        let info = CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipLast.rawValue)   // ignore A; frame is opaque
        guard let provider = CGDataProvider(data: Data(bytes: flipped, count: w * h * 4) as CFData),
              let img = CGImage(width: w, height: h, bitsPerComponent: 8, bitsPerPixel: 32,
                                bytesPerRow: w * 4, space: cs, bitmapInfo: info, provider: provider,
                                decode: nil, shouldInterpolate: false, intent: .defaultIntent)
        else { return }
        layer.contents = img
    }

    deinit { link?.invalidate() }

    // ---- touch → framebuffer pixels ----------------------------------------
    // The layer aspect-fits a w×h image into bounds. Recreate that rect to invert a
    // touch point back into framebuffer space (and clamp; off-image touches are dropped).
    private func toFB(_ p: CGPoint) -> (Float, Float)? {
        let b = bounds.size
        guard b.width > 0, b.height > 0 else { return nil }
        let scale = min(b.width / CGFloat(w), b.height / CGFloat(h))
        let dw = CGFloat(w) * scale, dh = CGFloat(h) * scale
        let ox = (b.width - dw) / 2, oy = (b.height - dh) / 2
        let fx = (p.x - ox) / scale, fy = (p.y - oy) / scale
        if fx < 0 || fy < 0 || fx >= CGFloat(w) || fy >= CGFloat(h) { return nil }
        return (Float(fx), Float(fy))
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        for t in touches {
            guard let (x, y) = toFB(t.location(in: self)) else { continue }
            let id = nextId; nextId += 1
            touchIds[ObjectIdentifier(t)] = id
            de_touch_begin(id, x, y)
        }
    }
    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        for t in touches {
            guard let id = touchIds[ObjectIdentifier(t)], let (x, y) = toFB(t.location(in: self)) else { continue }
            de_touch_moved(id, x, y)
        }
    }
    private func end(_ touches: Set<UITouch>) {
        for t in touches {
            guard let id = touchIds.removeValue(forKey: ObjectIdentifier(t)) else { continue }
            let (x, y) = toFB(t.location(in: self)) ?? (0, 0)
            de_touch_ended(id, x, y)
        }
    }
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) { end(touches) }
    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) { end(touches) }
}

struct CanvasViewRep: UIViewRepresentable {
    func makeUIView(context: Context) -> CanvasView { CanvasView(frame: .zero) }
    func updateUIView(_ uiView: CanvasView, context: Context) {}
}
