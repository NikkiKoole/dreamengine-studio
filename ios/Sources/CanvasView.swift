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
    // Phase 2: the active canvas size is DYNAMIC — a resizable cart reflows to the device via
    // de_resize (layoutSubviews). Re-read from the engine each frame (syncSize) instead of baking
    // in the boot size, so the blit + touch mapping track a reflow / rotation.
    private var w = Int(de_screen_w())
    private var h = Int(de_screen_h())
    private let cs = CGColorSpaceCreateDeviceRGB()
    private var flipped: [UInt32]            // top-down scratch the CGImage reads
    private var touchIds: [ObjectIdentifier: Int32] = [:]   // UITouch → stable engine id
    private var nextId: Int32 = 1

    // ---- on-device perf measurement (the renderer-decision gate; #if DEBUG only) ----
    // Accumulates over a ~1s window then NSLogs: engine de_frame() time, blit time, and the
    // ACTUAL displaylink interval (→ real fps). ios-deploy streams these off the device.
    private var pf_engineSum = 0.0, pf_engineMax = 0.0
    private var pf_blitSum = 0.0, pf_blitMax = 0.0
    private var pf_frames = 0
    private var pf_lastLog: CFTimeInterval = 0
    private var pf_lastTick: CFTimeInterval = 0
    private var pf_intervalSum = 0.0, pf_intervalMax = 0.0
    // perf goes to a file in the app's Documents — pulled off the device with
    // `ios-deploy --download` (lldb console forwarding is unreliable for an installed app).
    private lazy var pf_url: URL? = {
        let u = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)
                  .first?.appendingPathComponent("perf.log")
        if let u = u { try? "".write(to: u, atomically: true, encoding: .utf8) }   // truncate per run
        return u
    }()

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

    // A resizable cart fills the device: hand the engine our bounds (points; SCALE=1 on iOS → they
    // are framebuffer px). Fires on first layout AND on every rotation, so rotation reflows for free.
    // de_resize is a no-op when the size is unchanged, so calling it every layout is cheap. A fixed
    // cart (de_is_resizable()==0) is left at its compile-time size → letterboxed, exactly as before.
    // Pixel chunkiness ("physically-sized" pixels): the cart reflows to (points / pixelChunk) LOGICAL
    // pixels, not the raw point size. K=1 = hi-res tiny pixels (each is 1pt); higher K = chunkier lo-fi
    // pixels + proportionally bigger, finger-friendlier controls. CanvasView blits the small framebuffer
    // up to the full view (nearest), so a bigger K just means fatter pixels on screen.
    private let pixelChunk: CGFloat = 2

    override func layoutSubviews() {
        super.layoutSubviews()
        guard de_is_resizable() != 0 else { return }
        let k = max(1, pixelChunk)
        let b = bounds.size
        if b.width > 0, b.height > 0 { de_resize(Int32(b.width / k), Int32(b.height / k)) }
        // hand the engine the notch / home-bar / status-bar insets — in the SAME logical-pixel units as
        // the canvas (÷k) — so a resizable cart lays its controls inside safe_rect() while its
        // background bleeds to the edges.
        let ins = safeAreaInsets
        de_set_safe_area(Int32(ins.left / k), Int32(ins.top / k), Int32(ins.right / k), Int32(ins.bottom / k))
    }

    // pick up a de_resize: re-read the engine's active dims and resize the flip scratch when they
    // change, so the CGImage blit and touch mapping use the current canvas.
    private func syncSize() {
        let nw = Int(de_screen_w()), nh = Int(de_screen_h())
        if nw != w || nh != h { w = nw; h = nh; flipped = [UInt32](repeating: 0, count: w * h) }
    }

    @objc private func tick() {
        syncSize()
        let t0 = CACurrentMediaTime()
        de_frame(t0 - start)
        guard let base = de_framebuffer() else { return }
        let t1 = CACurrentMediaTime()
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
        perfTick(engine: t1 - t0, blit: CACurrentMediaTime() - t1, now: t0)
    }

    private func perfTick(engine: Double, blit: Double, now: CFTimeInterval) {
    #if DEBUG
        if pf_lastTick > 0 {
            let dt = now - pf_lastTick
            pf_intervalSum += dt; pf_intervalMax = max(pf_intervalMax, dt)
        }
        pf_lastTick = now
        pf_engineSum += engine; pf_engineMax = max(pf_engineMax, engine)
        pf_blitSum += blit;     pf_blitMax = max(pf_blitMax, blit)
        pf_frames += 1
        if pf_lastLog == 0 { pf_lastLog = now }
        if now - pf_lastLog >= 1.0 {
            let n = Double(pf_frames), ms = 1000.0
            let fps = pf_intervalSum > 0 ? Double(pf_frames - 1) / pf_intervalSum : 0
            let line = String(format: "[perf] %dx%d  engine avg %.2fms max %.2fms | blit avg %.2fms max %.2fms | fps %.1f (interval avg %.2fms max %.2fms) | budget 16.67ms\n",
                  w, h, pf_engineSum/n*ms, pf_engineMax*ms, pf_blitSum/n*ms, pf_blitMax*ms,
                  fps, pf_intervalSum/max(1,Double(pf_frames-1))*ms, pf_intervalMax*ms)
            if let u = pf_url, let h = try? FileHandle(forWritingTo: u) {   // append to Documents/perf.log
                h.seekToEndOfFile(); h.write(Data(line.utf8)); try? h.close()
            }
            NSLog("%@", line)                                  // also to the device console (if anyone's watching)
            pf_engineSum = 0; pf_engineMax = 0; pf_blitSum = 0; pf_blitMax = 0
            pf_intervalSum = 0; pf_intervalMax = 0; pf_frames = 0; pf_lastLog = now
        }
    #endif
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
