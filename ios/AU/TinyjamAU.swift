import AVFoundation
// CoreAudio explicitly: UnsafeMutableAudioBufferListPointer is declared in the CoreAudio Swift
// overlay (confirmed by grepping both SDKs' .swiftinterface files, rather than guessing imports one
// build at a time). `import AVFoundation` re-exports it on iOS but NOT under Mac Catalyst, which is
// why the iOS build never needed this line. Harmless on both.
import CoreAudio

// The AUv3 instrument extension — hosting the REAL dreamengine (not the spike arpeggio), played
// by host MIDI. It runs the same engine the standalone app does. Each render block, in order:
//
//   1. feed host MIDI into the engine's ring (de_midi_event), parsed from the realtime event list.
//   2. SIGNAL a frame — one per 735 rendered samples (44100/60), so the cart's clock is driven by
//      RENDERED AUDIO and not by a wall-clock timer. The frame itself runs on the worker thread (see
//      "THE FRAME WORKER"); under an OFFLINE render it runs inline, where exactness beats latency.
//   3. de_audio_render() = sound.h's mixer, filling interleaved stereo.
//   4. if the host asked for a rate other than 44100, RESAMPLE to it (see "the rate seam" below).
//
// Steps 1, 3 and 4 are the audio thread's whole job. The cart's update+draw is NOT on it — that was
// the arrangement until 2026-08-13, and it cost a wedged GarageBand transport. One engine instance per
// process: studio.c/sound.h use file-scope globals; iOS loads the AUv3 out-of-process, and one hosted
// rack is the case we support (multiple instances in one process would share state).
//
// ── the rate seam ────────────────────────────────────────────────────────────────────────────────
// The engine is COMPILE-TIME 44.1 kHz: SOUND_SAMPLE_RATE sizes every delay line and envelope, every
// audio gate in the repo assumes it, and the bit-determinism work (demath.h) depends on a fixed rate.
// The standalone app never had to care, because it feeds an AVAudioEngine source node that converts
// to the device rate. An AUv3 has no such buffer: the host calls us at the HOST's rate, and a host
// follows its audio INTERFACE, so 48k is common even though GarageBand/Logic/Live all default to 44.1.
//
// Measured before fixing (ios/au-transport-check --pitch, and the numbers are in ios-plan.md): at 48k
// the whole rack played +147 cents sharp = exactly 1200·log2(48000/44100), with every envelope and
// delay time 8.8% fast. What did NOT drift was the sequencer — the step comes from sync_beats() and a
// host states its playhead absolutely — so the defect was always confined to the SOUND, which is why
// the fix is a converter HERE and not an engine refactor.
//
// So: the engine always runs at 44100 and always in 735-sample frames. When the host rate differs,
// this file pulls those frames and resamples them to the host's rate with a 4-point Catmull-Rom
// interpolator (the sampler-standard: cheap, and it holds high frequencies far better than linear).
// Two details that matter more than the interpolator:
//   · At exactly 44100 the OLD code path runs, untouched and bit-identical. Every existing gate keeps
//     its meaning, and the common case pays nothing — no interpolation, no history, no extra copy.
//   · Below 44100 (auval renders us at 11025) plain interpolation would ALIAS, so the engine stream
//     goes through four cascaded one-poles at 0.45·hostRate first. The coefficient is 1.0 when the
//     host is at or above our rate, and a one-pole with coefficient 1.0 is an exact bypass, so the
//     upsampling path runs the same instructions with no branch and no filtering.
public final class TinyjamAU: AUAudioUnit {
    private let format = AVAudioFormat(standardFormatWithSampleRate: 44100, channels: 2)!
    private var _outputBusArray: AUAudioUnitBusArray!
    private var _inputBusArray: AUAudioUnitBusArray!

    private static let ENGINE_RATE = 44100.0        // what sound.h is COMPILED for. Not negotiable.
    private static let SAMPLES_PER_FRAME = 735      // 44100 / 60
    // stable interleaved L,R scratch — allocated once so the render block never allocates on the
    // audio thread. Generous cap; n*2 is asserted ≤ this in the render block.
    private let scratchCap = 16384 * 2
    private let scratch = UnsafeMutablePointer<Float>.allocate(capacity: 16384 * 2)
    // sample-clock state, owned by the render block (single audio thread). The FRAME counter is not
    // here: it belongs to the shared engine, so it lives with the worker (see ONE ENGINE PER PROCESS).
    private let acc = UnsafeMutablePointer<Int>.allocate(capacity: 1)     // samples since last frame
    // one cart frame of ENGINE-rate audio, pulled from by the resampler (interleaved L,R).
    private let echunk = UnsafeMutablePointer<Float>.allocate(capacity: 735 * 2)

    // The converter (AU/RateConvert.swift) plus the read cursor into echunk, behind one pointer —
    // matching how acc/frame are held, so the render block mutates it on the audio thread with no
    // allocation and no ARC, and the hot loop can lift the whole thing into a local copy.
    private struct RateState {
        var rc = RateConvert()
        var eIdx = 735           // read cursor into echunk; == SAMPLES_PER_FRAME means "spent, refill"
    }
    private let rate = UnsafeMutablePointer<RateState>.allocate(capacity: 1)

    public override init(componentDescription: AudioComponentDescription,
                         options: AudioComponentInstantiationOptions = []) throws {
        // BEFORE super.init: Swift requires every `let` initialized first, and this unit's engine is
        // one. de_instance_create is a plain C call with no dependency on the AU being constructed.
        engine = de_instance_create(DE_RENDERER_SOFTWARE)   // THIS unit's own engine
        try super.init(componentDescription: componentDescription, options: options)
        let outBus = try AUAudioUnitBus(format: format)
        _outputBusArray = AUAudioUnitBusArray(audioUnit: self, busType: .output, busses: [outBus])
        _inputBusArray  = AUAudioUnitBusArray(audioUnit: self, busType: .input,  busses: [])
        acc.pointee = 0
        rate.initialize(to: RateState())
        startWorker()   // BEFORE any render: a view can open while the host is stopped and still
                        // needs frames
        TinyjamAU.bootLock.lock()
        TinyjamAU.instanceCount += 1
        instanceID = TinyjamAU.instanceCount        // 1-based, so 0 stays free to mean "nobody"
        TinyjamAU.bootLock.unlock()
    }

    // ══ ONE ENGINE PER INSTANCE ═════════════════════════════════════════════════════════════════
    // It used to be one per PROCESS, and that was the defect. studio.c and sound.h kept their state
    // in file-scope globals, so an engine was a singleton by construction. It was recorded as a
    // known limitation on the theory that a host gives each instance its own process — GarageBand
    // does not. A sample taken while the maker's session was wedged found THREE TinyjamAU instances
    // in ONE process, each with its own de_init and its own frame worker, all writing the same
    // globals. The host had asked our view controller for an audio unit once per panel it opened.
    //
    // The engine's state is now per-instance (docs/design/engine-context.md) and the seam names its
    // instance (docs/design/engine-instance-seam.md), so each audio unit owns an engine and a frame
    // worker. Two tracks are two racks. Proven by tools/instance-check, which drives two engines
    // with different transport and asserts their frames and audio differ.
    //
    // ⚠ STILL SHARED: de_sync_position takes no instance, so the HOST TRANSPORT is process-wide.
    // That is mostly BENIGN rather than broken — two tracks in one DAW project share one transport,
    // so both render blocks push the same beat/tempo/playing and the engines agree. It goes wrong
    // only where two instances legitimately differ: an OFFLINE BOUNCE of one track while another
    // plays in realtime. Instance-scoping sync.h is the fix; it is not what makes two racks work.
    private static let bootLock = NSLock()          // instantiation only; never touched by audio
    fileprivate let engine: OpaquePointer

    // ══ WHICH INSTANCE IS THE AUDIBLE ONE ═══════════════════════════════════════════════════════
    // The one question the panel needs answered, and the reason it needs a new mechanism: the OLD
    // diagnostic compared the message channel's pid against the view controller's own pid and called
    // a match "talking to itself, still the wrong engine". Those two pids are the same BY
    // CONSTRUCTION — the channel is fetched from the view controller's own local audio unit, so the
    // call never leaves the process and can only ever report that process. The "connected" branch was
    // unreachable, which means the reading taken in GarageBand ("PANEL TALKING TO ITSELF") was
    // guaranteed output carrying no information, and the conclusion drawn from it — that the panel is
    // orphaned and both routes are closed — was never actually measured. A diagnostic whose other
    // branch cannot be reached is the same failure as a gate that cannot go red.
    //
    // What CAN be measured, without any cross-process API: audio is rendered by exactly one instance,
    // and an engine is per-process. So stamp the renderer's identity where the view can read it. Then
    // "the panel is orphaned" has a falsifiable form — NOTHING in this process has ever rendered —
    // and "the panel is fine" has one too, in two flavours worth telling apart.
    private var instanceID: UInt64 = 0
    private static var instanceCount: UInt64 = 0    // also the honest measure of how many front-ends
                                                    // are fighting over the one engine (defect B)
    // Written on the audio thread, so a plain pointer rather than a `static var` (no swift_once on the
    // hot path) and a UInt64 rather than a reference (no ARC). Read on main. A torn read is
    // impossible for an aligned 64-bit store and would only misname a diagnostic anyway. 0 = nobody.
    private static let renderedBy = UnsafeMutablePointer<UInt64>.allocate(capacity: 1)

    /// DIAGNOSTIC ONLY. `rendering == 0` means no instance in this process has ever rendered audio,
    /// which is what an orphaned panel looks like — and also what a host that has not started looks
    /// like, so it is read over time rather than once.
    public var audibilityReport: (mine: UInt64, rendering: UInt64, instances: UInt64) {
        (instanceID, TinyjamAU.renderedBy.pointee, TinyjamAU.instanceCount)
    }

    public override var outputBusses: AUAudioUnitBusArray { _outputBusArray }
    public override var inputBusses: AUAudioUnitBusArray { _inputBusArray }

    // ══ PARAMETER BRIDGE: TESTED, AND IT DOES NOT BRIDGE (2026-08-13) ═══════════════════════════
    // A probe parameter lived here briefly. Measured in GarageBand: a value written by the UI
    // process was observed ONLY in the UI process (same pid), exactly like the message channel. So
    // Apple's own supported route for UI↔DSP does not reach our rendering instance either.
    //
    // The parameter itself is REMOVED rather than left behind: a stray "Bridge Probe" shows up in
    // every host's automation list and generic UI, and this app is on the store. The finding is in
    // docs/design/ios-plan.md; re-adding the probe is ten lines if it is ever needed again.
    //
    // ⚠ Do not read that result as "AUv3 cannot do this". Commercial AUv3s have working
    // out-of-process UIs through exactly this mechanism, so the suspect is OUR configuration — the
    // view controller is both principal class and factory, and something about that is leaving it
    // with an orphaned AU. The way to find it is to diff this extension against a KNOWN-WORKING
    // AUv3 (Apple's AUv3FilterDemo, or bradhowes/LPF) and run the same pid probe on that, not to
    // add more instrumentation here. iPad is also untested and has historically hosted the audio
    // unit and the view in ONE process, which could make the whole problem vanish.

    // ══ CROSS-PROCESS CHANNEL (spike scaffolding) ═══════════════════════════════════════════════
    // A host runs our UI and our audio in DIFFERENT PROCESSES, so a view that blits the engine's
    // framebuffer draws an engine nobody can hear (docs/design/ios-plan.md → "The out-of-process
    // wall"). Two of the four ways out need a pipe across that boundary, and AUMessageChannel is
    // the one AUv3 provides. This is the minimum that makes the transport MEASURABLE — an echo —
    // so ios/au-msgchannel-spike.swift can answer "pixels or state?" with numbers instead of
    // estimates. It carries no engine data yet and is deliberately not wired to the view.
    // AUMessageChannel is 16.0+ and this extension deploys lower, so the override is gated.
    // Worth knowing beyond the spike: if the cross-process channel becomes the real transport,
    // it sets a hard OS floor for the PLUG-IN (the app itself is unaffected).
    @available(macCatalyst 16.0, iOS 16.0, macOS 13.0, *)
    private static let canvasChannel = TinyjamCanvasChannel()

    @available(macCatalyst 16.0, iOS 16.0, macOS 13.0, *)
    public override func messageChannel(for name: String) -> AUMessageChannel {
        // ⚠ RETAINED, not constructed per call. The first cut returned a fresh
        // TinyjamCanvasChannel() and nothing held it, so it was deallocated the instant this
        // method returned — the host's proxy then called into a dead object and every reply came
        // back EMPTY, which reads exactly like "not implemented yet".
        if name == "com.tinyjam.canvas" {
            Self.canvasChannel.owner = self   // the panel blits THIS unit's engine, not a process-wide one
            return Self.canvasChannel
        }
        return super.messageChannel(for: name)
    }

    // ══ THE FRAME WORKER ════════════════════════════════════════════════════════════════════════
    // de_frame() does not run on the audio thread. It used to, and that is a plug-in writing a cheque
    // its render deadline cannot cash: a frame is the cart's whole update AND its whole software-
    // rasterised UI. Measured on this cart, 0.4–0.9 ms typical and 2 ms peak in a Debug build (0.13–0.25
    // ms at -O2) — comfortably inside a 512-sample buffer's 11.6 ms, and comfortably OUTSIDE a
    // 64-sample buffer's 1.45 ms. A host at a small buffer therefore overruns periodically, and the
    // symptom is not a crash: GarageBand stops the transport and will not start again.
    //
    // So the render block SIGNALS this worker (never waits on it) and returns to doing only audio.
    // That restores the arrangement the standalone app always had — a game thread producing into
    // sound.h's SPSC queue, an audio thread consuming it — and as a bonus moves every allocation the
    // frame can make (the framebuffer realloc on resize, the present buffer's growth) off the audio
    // thread too, which was the other realtime sin left in here.
    //
    // Cost: note timing is quantised to the worker's wakeup rather than the exact sample, a jitter of
    // at most one frame. That is precisely what the standalone app has always done, and it is
    // inaudible next to an overrun.
    // Shared with the engine they drive: ONE worker per process, not per instance (see ONE ENGINE PER
    // PROCESS). It outlives every instance — a thread parked on a semaphore costs nothing, and tearing
    // it down when one instance goes away would strand the others mid-frame.
    // ONE WORKER PER INSTANCE. It used to be one per process, which worked only while there was one
    // engine: a semaphore signal carries no identity, so a shared worker cannot know WHICH rack to
    // advance. Per-instance also makes teardown trivial — nothing is shared, so nothing is stranded.
    // A thread parked on a semaphore costs essentially nothing.
    private var worker: Thread?
    private let frameSignal = DispatchSemaphore(value: 0)
    private var frameCount: UInt64 = 0

    private func workerLoop() {
        while true {
            frameSignal.wait()
            frameCount &+= 1
            de_frame(engine, Double(frameCount) / 60.0)
        }
    }
    private func startWorker() {
        guard worker == nil else { return }
        let t = Thread { [weak self] in self?.workerLoop() }
        t.name = "dreamengine.frame"
        t.qualityOfService = .userInteractive   // it feeds the sequencer; it must not be starved
        t.start()
        worker = t
    }
    // ── THE UI KEEP-ALIVE ───────────────────────────────────────────────────────────────────────
    // The frame is driven by RENDERED AUDIO (one per 735 samples), which is what keeps the sequencer
    // on the host's grid. But a host that is STOPPED may stop pulling audio altogether — GarageBand
    // does — and then nothing ticks the engine at all. The panel freezes on its last frame, and far
    // worse, the cart stops reading INPUT: its own play button does nothing, because nobody is running
    // the code that would notice the click. That is exactly what a stopped-at-bar-33 plug-in looked
    // like, and "I can't start it from the host OR from the plug-in" is the symptom.
    //
    // So the VIEW calls this once per display tick. It signals a frame ONLY when the audio side has
    // not advanced one since the last call, so a rendering host keeps its sample-clocked timing and an
    // idle host still gets a live, clickable panel at display rate. Same worker either way, so there
    // is still exactly one thread inside the engine.
    private var lastSeenFrame: UInt64 = 0
    public func uiTick() {
        let f = frameCount
        if f == lastSeenFrame { frameSignal.signal() }  // audio is not driving: we will
        lastSeenFrame = f
    }

    // The host's rate is only knowable here: it sets the bus format, and it may set a DIFFERENT one
    // later (a user changing interface or project rate re-allocates). So configure per allocation and
    // reset the converter's state, rather than reading the rate once at init.
    public override func allocateRenderResources() throws {
        try super.allocateRenderResources()
        let host = _outputBusArray[0].format.sampleRate
        var s = RateState()
        s.rc.configure(engineRate: TinyjamAU.ENGINE_RATE, hostRate: host)
        rate.pointee = s
        acc.pointee = 0                      // the fast path's sample clock; stale after a rate change
    }

    public override var internalRenderBlock: AUInternalRenderBlock {
        let scratch = self.scratch, cap = self.scratchCap, acc = self.acc
        let echunk = self.echunk, rate = self.rate, signal = self.frameSignal
        let spf = TinyjamAU.SAMPLES_PER_FRAME
        // Capture SELF unretained, not the two host blocks themselves. The host assigns
        // musicalContextBlock / transportStateBlock AFTER it fetches this render block, so reading
        // them out here would capture nil forever — the documented AUv3 trap. takeUnretainedValue()
        // adds no retain/release, so it is safe on the audio thread.
        let unownedSelf = Unmanaged.passUnretained(self)
        // Resolved HERE, off the audio thread, for the same reason as everything else in this capture
        // list: a `static let` touched inside the block would run swift_once on the render path.
        let myID = self.instanceID, renderedBy = TinyjamAU.renderedBy
        // Same rule as the rest of this capture list: resolved HERE, off the audio thread. Touching
        // a `static var` inside the block would put swift_once on the render path.
        let engine = self.engine
        return { _, _, frameCount, _, outputData, eventListHead, _ in
            let n = Int(frameCount)
            if n * 2 > cap { return kAudioUnitErr_TooManyFramesToProcess }
            renderedBy.pointee = myID       // "this is the instance you can hear" — see audibilityReport
            let me = unownedSelf.takeUnretainedValue()

            // 0) HOST TRANSPORT → runtime/sync.h, before the frame below ticks so the cart's
            //    sequencer sees this block's position. The host states its playhead ABSOLUTELY
            //    (tempo + beat), which is the easy half of the sync seam: nothing to measure.
            //    Both blocks are nullable and a host may supply neither — then we push nothing and
            //    the cart free-runs on its own clock exactly as before.
            if let ctx = me.musicalContextBlock {
                var tempo = 0.0, beat = 0.0
                if ctx(&tempo, nil, nil, &beat, nil, nil), tempo > 0 {
                    var playing = true          // no transport block = "if it renders, it plays"
                    if let ts = me.transportStateBlock {
                        var flags = AUHostTransportStateFlags()
                        if ts(&flags, nil, nil, nil) { playing = flags.contains(.moving) }
                    }
                    de_sync_position(beat, tempo, playing ? 1 : 0)
                }
            }
            // 1) feed host MIDI into the engine ring FIRST, so the frame ticked below sees it.
            //    Walk the realtime event list; handle note-on/off (0x90/0x80) + pitch-bend (0xE0).
            var ev = eventListHead
            while let e = ev {
                if e.pointee.head.eventType == .MIDI {
                    let m = e.pointee.MIDI
                    if m.length >= 3 {
                        let status = m.data.0 & 0xF0, d1 = Int(m.data.1), d2 = Int(m.data.2)
                        switch status {
                        case 0x90: de_midi_event(d2 > 0 ? 1 : -1, Int32(d1), Int32(d2))   // note-on (vel 0 = off)
                        case 0x80: de_midi_event(-1, Int32(d1), Int32(d2))                // note-off
                        case 0xE0: de_midi_bend(Int32(((d2 << 7) | d1) - 8192))           // pitch-bend
                        default: break
                        }
                    }
                }
                ev = UnsafePointer(e.pointee.head.next)
            }
            let abl = UnsafeMutableAudioBufferListPointer(outputData)
            let offline = me.isRenderingOffline

            if rate.pointee.rc.passthrough {
                // ── 2a) THE HOST IS AT OUR RATE (44100): the original path, byte for byte. ────────
                //    advance the sequencer in step with rendered audio (one de_frame per 735 samples).
                //    The cart's update() drains the MIDI ring (keybed_update → note_on/off) here.
                acc.pointee += n
                while acc.pointee >= spf {
                    acc.pointee -= spf
                    // OFFLINE (a bounce) runs the frame INLINE: there is no deadline to miss, and a
                    // bounce must be exact — handing it to a worker would let audio render ahead of
                    // the sequencer that is supposed to be driving it. Realtime signals instead.
                    if offline { me.frameCount &+= 1; de_frame(engine, Double(me.frameCount) / 60.0) }
                    else       { signal.signal() }
                }
                de_audio_render(engine, scratch, Int32(n))                   // sound.h mixer → interleaved L,R
            } else {
                // ── 2b) THE HOST IS AT ANOTHER RATE: pull engine frames and convert. ─────────────
                //    The engine's clock is now driven by ENGINE samples consumed, not by the host's
                //    frameCount — that is the actual bug being fixed. One refill == one cart frame ==
                //    735 engine samples, so the tick lands where it belongs no matter what n is (and
                //    slightly more accurately than 2a, which batches its frames at the buffer edge).
                var st = rate.pointee            // local copy: the hot loop stays in registers
                for j in 0..<n {
                    while st.rc.needsFrame {
                        if st.eIdx >= spf {
                            if offline { me.frameCount &+= 1; de_frame(engine, Double(me.frameCount) / 60.0) }
                            else       { signal.signal() }        // see the fast path's note
                            de_audio_render(engine, echunk, Int32(spf))
                            st.eIdx = 0
                        }
                        st.rc.push(echunk[st.eIdx*2], echunk[st.eIdx*2 + 1])
                        st.eIdx += 1
                    }
                    let (l, r) = st.rc.sample()
                    scratch[j*2] = l; scratch[j*2 + 1] = r
                    st.rc.advance()
                }
                rate.pointee = st
            }

            if abl.count >= 2,
               let l = abl[0].mData?.assumingMemoryBound(to: Float.self),
               let r = abl[1].mData?.assumingMemoryBound(to: Float.self) {
                for i in 0..<n { l[i] = scratch[i*2]; r[i] = scratch[i*2 + 1] }
            } else if let l = abl.first?.mData?.assumingMemoryBound(to: Float.self) {
                for i in 0..<n { l[i] = scratch[i*2] }               // mono fallback
            }
            return noErr
        }
    }

    deinit { scratch.deallocate(); acc.deallocate(); echunk.deallocate(); rate.deallocate() }
}

// NSExtensionPrincipalClass — the system instantiates this to get the AU.
// The echo end of the spike's pipe. Bounces the payload straight back so a round trip can be timed;
// what a real implementation would do here is hand back a framebuffer (option 4) or a state delta
// (the netplay-style alternative). Kept trivial ON PURPOSE — a spike that measures its own cleverness
// measures nothing.
@available(macCatalyst 16.0, iOS 16.0, macOS 13.0, *)
final class TinyjamCanvasChannel: NSObject, AUMessageChannel {
    var callHostBlock: CallHostBlock?

    // WHICH engine this channel blits. There is one engine per audio unit now, so a channel that
    // reached for a process-wide one would serve whichever instance happened to be there — the exact
    // "the panel is showing an engine nobody can hear" class of bug this channel exists to close.
    // Set by the audio unit that hands the channel out.
    weak var owner: TinyjamAU?

    // The snapshot buffer lives here, in the AUDIO process, where the engine everyone can hear is.
    // Grown the same way CanvasView grows its own: ask, resize, get it next call.
    // stamped once per process, so a reply names WHICH engine answered
    static let instanceNonce = Int.random(in: 1...999_999)

    private var buf: UnsafeMutablePointer<UInt32>?
    private var cap = 0

    // ⚠ A METHOD, not a stored closure property. Swift imports AUMessageChannel's callAudioUnit as
    // an OPTIONAL ObjC protocol method, which on the *calling* side reads as an optional closure —
    // so `var callAudioUnit: ((...)->...)?` looks like it conforms, compiles clean, and registers no
    // selector at all. The host's proxy then finds nothing implemented and every reply comes back
    // EMPTY, which is indistinguishable from "the plug-in has no channel yet".
    //
    // THREADING: this runs on whatever thread the XPC call arrives on, never the audio thread — and
    // de_copy_frame is built for exactly that (a seqlock over the published frame, gated by
    // tools/present-race-check with a TSan run and a -bypass control). This is the seam being used
    // for the thing it was written for.
    func callAudioUnit(_ message: [AnyHashable: Any]) -> [AnyHashable: Any] {
        guard let op = message["op"] as? String else { return [:] }
        switch op {
        case "echo":
            var reply = message
            reply["ok"] = true          // marker: tells a real answer from a no-op channel
            return reply

        // A per-INSTANCE id. The one question a spike run from a host cannot answer: is the AU the
        // PANEL talks to the same instance as the one making sound? In an out-of-process host the UI
        // extension constructs its OWN TinyjamAU (see TinyjamAUViewController.createAudioUnit), so
        // "same process" is not a given and neither is "same engine". Two channels reporting
        // different nonces is the disconnect, stated as a number instead of inferred from a symptom.
        case "nonce":
            return ["nonce": TinyjamCanvasChannel.instanceNonce, "pid": Int(ProcessInfo.processInfo.processIdentifier)]

        case "frame":
            guard let engine = owner?.engine else { return [:] }   // no owner = nothing to show
            var pw: Int32 = 0, ph: Int32 = 0
            var ok = buf != nil && de_copy_frame(engine, buf, Int32(cap), &pw, &ph) == 1
            if !ok {
                _ = de_copy_frame(engine, nil, 0, &pw, &ph)      // dst == nil: report the size, copy nothing
                let need = Int(pw) * Int(ph)
                if need > cap, need > 0 {
                    buf?.deallocate()
                    buf = UnsafeMutablePointer<UInt32>.allocate(capacity: need)
                    cap = need
                    ok = de_copy_frame(engine, buf, Int32(cap), &pw, &ph) == 1
                }
            }
            guard ok, let b = buf, pw > 0, ph > 0 else { return [:] }
            // 4 BYTES PER PIXEL. The spike's first estimate assumed a 1-byte palette index and so
            // undercounted a frame 4x; the engine publishes RGBA UInt32. At this cart's 160x100 that
            // is 64KB, and a 320x200 cart is 256KB — both already measured on this channel.
            return ["w": Int(pw), "h": Int(ph),
                    "px": Data(bytes: b, count: Int(pw) * Int(ph) * 4)]

        default:
            return [:]
        }
    }
}


public final class TinyjamAUFactory: NSObject, AUAudioUnitFactory {
    public func beginRequest(with context: NSExtensionContext) {}
    public func createAudioUnit(with componentDescription: AudioComponentDescription) throws -> AUAudioUnit {
        try TinyjamAU(componentDescription: componentDescription, options: [])
    }
}
