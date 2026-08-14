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
//
// TWO MODES, and `hosted` is the whole difference. Standalone (hosted: false) this view owns the
// world: it calls de_init, starts CoreAudio, and ticks de_frame from the display link. Inside the
// AUv3 (hosted: true) the PLUG-IN owns all three — de_init in its init, audio from the host's render
// block, and the frame ticked there too, SAMPLE-CLOCKED so the sequencer survives an offline bounce.
// A hosted view therefore only ever BLITS, and it must read a snapshot (de_copy_frame) rather than
// the live canvas, because the engine is drawing on the audio thread while we draw here on main.
final class CanvasView: UIView {
    private let hosted: Bool

    // ── where a HOSTED frame comes from ──────────────────────────────────────────────────────────
    // Default: de_copy_frame, i.e. THIS process's engine. In an out-of-process AUv3 that is the
    // wrong engine — the UI extension builds its own TinyjamAU, so the panel has always been
    // blitting an instance nobody can hear (docs/design/ios-plan.md → "The out-of-process wall").
    // Set this and the view pulls the frame from wherever the closure says instead, which is how the
    // panel gets connected to the engine that is actually making sound.
    // nil = keep the old local behaviour, so the standalone app and any in-process host are untouched.
    var remoteFrame: (() -> (px: Data, w: Int, h: Int)?)?
    // Hosted only: called once per display tick so the plug-in can advance a frame when the HOST is
    // not rendering audio (a stopped DAW stops pulling, and then nothing ticks the engine — the panel
    // freezes and, worse, stops responding to clicks). TinyjamAU.uiTick() decides whether it is needed.
    var onDisplayTick: (() -> Void)?
    // hosted only: a bottom-up snapshot of the last published frame. A manual allocation rather than
    // a Swift Array because the flip below needs a pointer that OUTLIVES the access — escaping one out
    // of withUnsafeMutableBufferPointer is undefined behaviour, however well it seems to work.
    private var snap: UnsafeMutablePointer<UInt32>? = nil
    private var snapCap = 0
    private var link: CADisplayLink?
    private var start: CFTimeInterval = 0
    // Phase 2: the active canvas size is DYNAMIC — a resizable cart reflows to the device via
    // de_resize (layoutSubviews). Re-read from the engine each frame (syncSize) instead of baking
    // in the boot size, so the blit + touch mapping track a reflow / rotation.
    // The engine instance this view shows. The seam names its instance now
    // (docs/design/engine-instance-seam.md); today there is exactly one, so hosted and standalone
    // both resolve to it. Step 3 has the plug-in hand its OWN instance to its view instead.
    // THE ENGINE THIS VIEW SHOWS. Standalone creates one; HOSTED is GIVEN one by the audio unit
    // that owns the panel — it must NOT create its own. A UI extension runs in its own process, so
    // calling de_instance_create there boots a whole second engine (and runs the cart's init) inside
    // the view process: pointless at best, and it is what stopped the panel reporting.
    var engine: OpaquePointer!
    private var w = 0
    private var h = 0
    private let cs = CGColorSpaceCreateDeviceRGB()
    private var flipped: [UInt32] = []       // top-down scratch the CGImage reads
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

    init(frame: CGRect, hosted: Bool = false) {
        self.hosted = hosted
        super.init(frame: frame)
        backgroundColor = .black
        isMultipleTouchEnabled = true
        layer.magnificationFilter = .nearest
        layer.contentsGravity = .resizeAspect
#if !AU_EXT
        if !hosted {
            engine = de_instance_create(DE_RENDERER_SOFTWARE)   // this app's one engine
            // ⚠ HAND IT THE SAME INSTANCE. AudioEngine used to create its own, which was invisible
            // while de_instance_create returned a singleton and became total silence the moment it
            // started allocating — the speakers rendered an engine no touch ever reached.
            AudioEngine.shared.start(instance: engine)   // CoreAudio pulls de_audio_render on its own thread
        }                                    // hosted: the audio unit hands us its engine (see `engine`)
#endif
        if engine != nil {
            w = Int(de_screen_w(engine)); h = Int(de_screen_h(engine))
            flipped = [UInt32](repeating: 0, count: w * h)
        }
        start = CACurrentMediaTime()
        let l = CADisplayLink(target: self, selector: #selector(tick))
        l.add(to: .main, forMode: .common)
        link = l
    }
    override convenience init(frame: CGRect) { self.init(frame: frame, hosted: false) }
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
        guard de_is_resizable(engine) != 0 else { return }
        let k = max(1, pixelChunk)
        de_set_backing_scale(engine, Float(k))   // pt per logical px → finger_px() (a finger = 44pt/k logical px)
        let b = bounds.size
        if b.width > 0, b.height > 0 { de_resize(engine, Int32(b.width / k), Int32(b.height / k)) }
        // hand the engine the notch / home-bar / status-bar insets — in the SAME logical-pixel units as
        // the canvas (÷k) — so a resizable cart lays its controls inside safe_rect() while its
        // background bleeds to the edges.
        let ins = safeAreaInsets
        de_set_safe_area(engine, Int32(ins.left / k), Int32(ins.top / k), Int32(ins.right / k), Int32(ins.bottom / k))
    }

    // pick up a de_resize: re-read the engine's active dims and resize the flip scratch when they
    // change, so the CGImage blit and touch mapping use the current canvas.
    private func syncSize() {
        guard engine != nil else { return }   // hosted, before the audio unit handed us one
        let nw = Int(de_screen_w(engine)), nh = Int(de_screen_h(engine))
        if nw != w || nh != h { w = nw; h = nh; flipped = [UInt32](repeating: 0, count: w * h) }
    }

    @objc private func tick() {
        let t0 = CACurrentMediaTime()
        var base: UnsafePointer<UInt32>
        if hosted, let pull = remoteFrame {
            // REMOTE: the frame comes from the process that is actually making sound. Reuses `snap`
            // and falls through to the SAME blit below — one blit path, so the remote case cannot
            // drift from the local one. No onDisplayTick: that exists to tick a LOCAL engine and
            // there is no local engine on this path.
            guard let f = pull(), f.w > 0, f.h > 0, f.px.count == f.w * f.h * 4 else { return }
            let need = f.w * f.h
            if need > snapCap {
                snap?.deallocate()
                snap = UnsafeMutablePointer<UInt32>.allocate(capacity: need)
                snapCap = need
            }
            guard let s = snap else { return }
            _ = f.px.withUnsafeBytes { raw in
                memcpy(UnsafeMutableRawPointer(s), raw.baseAddress!, need * 4)
            }
            if f.w != w || f.h != h { w = f.w; h = f.h; flipped = [UInt32](repeating: 0, count: w * h) }
            base = UnsafePointer(s)
        } else if hosted {
            onDisplayTick?()      // keep the engine alive when the host is not rendering (see above)
            // BLIT ONLY. The plug-in's render block already ticked the engine on the audio thread, so
            // all we do is take a snapshot. de_copy_frame reports the size it has even when it refuses
            // to copy, which is how the buffer grows: ask, resize, get it next tick.
            var pw: Int32 = 0, ph: Int32 = 0
            guard engine != nil else { return }
            var ok = snap != nil && de_copy_frame(engine, snap, Int32(snapCap), &pw, &ph) == 1
            if !ok {
                _ = de_copy_frame(engine, nil, 0, &pw, &ph)         // dst == nil: report the size, copy nothing
                let need = Int(pw) * Int(ph)
                if need > snapCap, need > 0 {
                    snap?.deallocate()                      // main thread owns this buffer alone
                    snap = UnsafeMutablePointer<UInt32>.allocate(capacity: need)
                    snapCap = need
                    ok = de_copy_frame(engine, snap, Int32(snapCap), &pw, &ph) == 1
                }
            }
            guard ok, let s = snap, pw > 0, ph > 0 else { return }   // nothing yet: keep the last image
            if Int(pw) != w || Int(ph) != h {               // the engine reflowed; resize our scratch
                w = Int(pw); h = Int(ph); flipped = [UInt32](repeating: 0, count: w * h)
            }
            base = UnsafePointer(s)
        } else {
#if !AU_EXT
            AudioEngine.shared.pollMic()   // open/close the mic to match the cart's mic_start()/mic_stop()
#endif
            syncSize()
            guard engine != nil else { return }
            de_frame(engine, t0 - start)
            // A resizable cart can de_resize the canvas DURING de_frame (e.g. acidcandy's chunky reflow
            // shrinks 426×196 → 217×100). Re-sync dims + the flip scratch AFTER de_frame, or the blit
            // below memcpy's the OLD (larger) w×h out of the NEW (smaller) framebuffer → SIGSEGV over-read.
            syncSize()
            guard engine != nil, let live = de_framebuffer(engine) else { return }
            base = live
        }
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

    deinit { link?.invalidate(); snap?.deallocate() }

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
            if engine != nil { de_touch_begin(engine, id, x, y) }
        }
    }
    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        for t in touches {
            guard let id = touchIds[ObjectIdentifier(t)], let (x, y) = toFB(t.location(in: self)) else { continue }
            if engine != nil { de_touch_moved(engine, id, x, y) }
        }
    }
    private func end(_ touches: Set<UITouch>) {
        for t in touches {
            guard let id = touchIds.removeValue(forKey: ObjectIdentifier(t)) else { continue }
            let (x, y) = toFB(t.location(in: self)) ?? (0, 0)
            if engine != nil { de_touch_ended(engine, id, x, y) }
        }
    }
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) { end(touches) }
    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) { end(touches) }
}

#if !AU_EXT   // the app's SwiftUI wrapper; the extension builds its view in TinyjamAUViewController
struct CanvasViewRep: UIViewRepresentable {
    func makeUIView(context: Context) -> CanvasView { CanvasView(frame: .zero) }
    func updateUIView(_ uiView: CanvasView, context: Context) {}
}
#endif
