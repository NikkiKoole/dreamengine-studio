import AVFoundation
// CoreAudio explicitly: UnsafeMutableAudioBufferListPointer is declared in the CoreAudio Swift
// overlay (confirmed by grepping both SDKs' .swiftinterface files, rather than guessing imports one
// build at a time). `import AVFoundation` re-exports it on iOS but NOT under Mac Catalyst, which is
// why the iOS build never needed this line. Harmless on both.
import CoreAudio
import os

// ── A DIAGNOSTIC THAT SURVIVES THE DEVICE ───────────────────────────────────────────────────────
// On iOS, NSLog is os_log underneath, and os_log REDACTS every dynamic value by default. So
// `NSLog("… %llu live", n)` arrives in Console.app as the single word `<private>` — the line is
// there, the timestamp is there, and the one thing you needed is gone. It reads as a broken logger
// rather than a privacy default, and it costs a build cycle to discover.
//
// Only a format string marked `%{public}` opts out, and NSLog has no way to say that, so this goes
// through os_log directly. The message is preformatted and handed over as one public argument,
// which keeps the call sites reading like the NSLog they replaced.
//
// ⚠ NOT applied to the other `[tinyjam]` lines in this target yet, deliberately: `--panel` greps
// for the PANEL line, NSLog writes to stderr and os_log does not, and changing that while a gate
// depends on it is a separate change with its own blast radius. They are all redacted on device
// too — see docs/design/ios-plan.md.
private let deDiagLog = OSLog(subsystem: "com.tinyjam", category: "diag")
// Internal, not private: the view controller logs through it too, so the PANEL diagnostic is
// readable on a device for the same reason the ledger is.
func deDiag(_ s: String) { os_log("%{public}@", log: deDiagLog, type: .default, s as NSString) }

// An OSType back to the four ASCII bytes it is ("aumf"), so a diagnostic names the type the way the
// plist and `auval` do rather than printing 1635085670 and making the reader do the arithmetic.
func fourCCString(_ t: OSType) -> String {
    let b = [UInt8(truncatingIfNeeded: t >> 24), UInt8(truncatingIfNeeded: t >> 16),
             UInt8(truncatingIfNeeded: t >> 8),  UInt8(truncatingIfNeeded: t)]
    return b.allSatisfy { $0 >= 0x20 && $0 < 0x7f } ? String(decoding: b, as: UTF8.self) : "0x\(String(t, radix: 16))"
}

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

    // ── EFFECT INPUT (docs/design/auv3-plugin-types.md §4.1) ──────────────────────────────────────
    // An INSTRUMENT (`aumu`) declares NO input bus and this stays false, so everything below is
    // inert and instrument racks are byte-for-byte unaffected. An EFFECT (`aumf`) declares one, and
    // the render block pulls the host's track into the engine's input ring — which `input_monitor()`
    // feeds into the master insert chain, i.e. the cart's pedals (sound.h:6536, before the chain at
    // 6541). That is what makes "hear your piano through the pedalboard" work.
    //
    // Read from our OWN componentDescription rather than a build flag: the type is already declared
    // in the Info.plist that the system used to instantiate us, so this cannot drift from it. A
    // `-D` would be a second source of truth for the same fact.
    //
    // ⚠⚠ ONE INSTANCE ONLY, TODAY. The input ring is process-GLOBAL: `extin_mon_on`/`extin_mon_gain`
    // moved into the per-instance context, but `sound_extin[]`, `extin_w`, `extin_r` and `extin_on`
    // did not — deliberately, and `tools/ctx-classification.json` says why in words that name their
    // own expiry ("ONE CAPTURE DEVICE per process… Revisit if an instance ever needs its own mic
    // routing"). That ring is SINGLE-producer/SINGLE-consumer by construction, so two effect
    // instances would be two producers racing on `extin_w` AND two consumers each eating samples the
    // other needed — garbled at ANY sample rate, not just off 44.1k. Making the extin group
    // per-instance (plus `rs_q`/`rs_prev`, function-local statics in mic_input_push) is the
    // prerequisite for a second instance. §8 Q2.
    private let isEffect: Bool
    private let inScratch = UnsafeMutablePointer<Float>.allocate(capacity: 16384 * 2)  // pull target, per-channel halves
    private let inMono    = UnsafeMutablePointer<Float>.allocate(capacity: 16384)      // downmix the ring actually takes
    private let inABL     = AudioBufferList.allocate(maximumBuffers: 2)

    // ── EFFECT-INPUT DIAGNOSTICS (auv3-plugin-types.md §4.1b, suspects 1 and 5) ───────────────
    // The defect this exists for: `pedalboard` loads in GarageBand, the panel works, and the
    // host's track is SILENT. The render block below skips the whole input path when the pull
    // fails, and says nothing, so "the host never gave us audio" and "we got audio and lost it
    // downstream" produce an identical symptom. These counters split those two, and they have
    // disjoint fixes.
    //
    // ⚠ WRITTEN ON THE AUDIO THREAD, read on a timer, with no synchronisation. That is a benign
    // race BY CONSTRUCTION and must stay one: these are counters for a human to read, never
    // control flow. A torn UInt64 on the one report that catches it changes nothing. Do not
    // branch on any of them, and do not "fix" this with a lock — a lock here would be a real
    // audio-thread defect in exchange for a cosmetic one.
    struct InDiag {
        var pulls = UInt64(0)       // pull attempts
        var fails = UInt64(0)       // pull returned something other than noErr
        var oversize = UInt64(0)    // frameCount exceeded our scratch, so we never even tried
        var pushed = UInt64(0)      // samples handed to de_audio_input
        var peak = Float(0)         // loudest |sample| SINCE THE LAST REPORT, not since boot
        var lastN = Int32(0)        // the last frameCount the host asked for
        var lastABL = Int32(0)      // buffers the host actually filled — suspect 5 reads this
        var lastErr = Int32(0)      // the last non-noErr OSStatus, verbatim
        // ⚠ THE AMBIGUITY THAT MADE `peak 0` USELESS ON ITS OWN. Fresh pages from the kernel are
        // ZERO-FILLED, so "the host wrote digital silence" and "the host returned noErr and wrote
        // NOTHING" both read as peak 0.00000 — and they are different faults with different fixes
        // (theirs vs OURS). So poison the buffer with a value no audio path produces and see if it
        // survives the pull. Without this the probe points confidently at the wrong half.
        var unwritten = UInt64(0)   // pull said noErr and left our sentinel in place
        var nonfinite = UInt64(0)   // a pull delivered inf/NaN (auval does; a DAW should not)
    }
    private let inDiag = UnsafeMutablePointer<InDiag>.allocate(capacity: 1)
    private var inDiagTimer: DispatchSourceTimer?

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
        // de:engine-owner — an audio unit IS a rack; it creates the engine and hands the SAME pointer to
        // its view (that is why the AUv3 never had the double-engine bug the app did).
        engine = de_instance_create(DE_RENDERER_SOFTWARE)   // THIS unit's own engine
        // Both effect types, because `aufx` is a legitimate declaration even though §4.1 argues for
        // `aumf` (same wiring cost, and `aumf` also gets notes — so an effect rack keeps its own
        // playable instrument). Deciding here means the bus follows the plist, whichever was chosen.
        let ct = componentDescription.componentType
        isEffect = (ct == kAudioUnitType_MusicEffect || ct == kAudioUnitType_Effect)
        try super.init(componentDescription: componentDescription, options: options)
        let outBus = try AUAudioUnitBus(format: format)
        _outputBusArray = AUAudioUnitBusArray(audioUnit: self, busType: .output, busses: [outBus])
        // An instrument keeps the EMPTY array it has always had. Declaring an input bus on an `aumu`
        // is not merely useless — hosts read the bus arrays to decide what to offer the user, and
        // Apple's own instrument template declares none.
        let inBusses = isEffect ? [try AUAudioUnitBus(format: format)] : []
        _inputBusArray  = AUAudioUnitBusArray(audioUnit: self, busType: .input,  busses: inBusses)
        inDiag.initialize(to: InDiag())
        if isEffect { deDiag("[tinyjam] AU is an EFFECT (\(fourCCString(ct))) — input bus declared") }
        acc.pointee = 0
        rate.initialize(to: RateState())
        startWorker()   // BEFORE any render: a view can open while the host is stopped and still
                        // needs frames
        TinyjamAU.bootLock.lock()
        TinyjamAU.instanceCount += 1
        instanceID = TinyjamAU.instanceCount        // 1-based, so 0 stays free to mean "nobody"
        TinyjamAU.liveCount += 1
        let live = TinyjamAU.liveCount
        TinyjamAU.bootLock.unlock()
        // ── THE TEARDOWN LEDGER ─────────────────────────────────────────────────────────────────
        // Paired with the line in deinit, and the pair is the whole point: a leak of ~1 MB per rack
        // is invisible to a human watching a memory graph, so "the iPad did not complain" is not
        // evidence either way. CREATE/DESTROY lines are. LIVE returning to 0 after you remove every
        // plug-in is the falsifiable form of "the rack is given back"; LIVE only ever climbing is
        // the falsifiable form of the bug this pair was added to watch (deinit never firing, so
        // de_instance_destroy never called). Read with Console.app, filter `tinyjam`.
        deDiag(String(format: "[tinyjam] AU CREATE  · instance %llu · %llu live", instanceID, live))
        buildParameterTree()   // the host's automation menu — see below
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
    // ⚠ STILL SHARED: THE CART'S OWN STATE. The engine is per-instance (state, transport and all),
    // but a cart that keeps its state in file-scope statics rather than de_state() has ONE
    // sequencer across every instance — acidcandy has 136 statics and no de_state(), so only the
    // engine its sequencer happens to fire into makes sound. That is the next step and it is cart
    // work, not engine work. tools/instance-check reports it explicitly rather than asserting it.
    private static let bootLock = NSLock()          // instantiation only; never touched by audio
    // internal, not fileprivate: the view controller (another file) hands it to the panel
    let engine: OpaquePointer

    // ══ HOST PARAMETERS ═════════════════════════════════════════════════════════════════════════
    // Until now this unit exposed NO parameterTree at all, so a host saw ZERO parameters: nothing on
    // the rack was automatable or recordable and the automation menu was empty. That is also why the
    // mod wheel had to be mapped to the master filter by hand — a workaround for having none.
    //
    // The tree is built from what the CART declared (runtime/param.h → param_bind), not from a list
    // written here. A table in Swift would be a second source of truth for a fantasy console whose
    // whole point is swapping carts: change the cart and the plug-in's parameters change with it,
    // with nothing to keep in sync.
    // ⚠ NO `parameterTree` OVERRIDE. The first cut stored the tree in a property and overrode the
    // accessor with a no-op setter, reasoning that "a host does not get to replace our tree". That
    // swallowed AUAudioUnit's OWN setter, which is not a formality: it is what installs the tree with
    // the framework, and out of process that installation is what keeps the HOST-SIDE MIRROR fed.
    // The symptom was oddly specific and cost a long detour — a host could SEE all 21 parameters and
    // WRITE them (the write reached the DSP, measured), but reading one back returned the value it
    // held BEFORE the write, because the extension never published anything to mirror. Assign it the
    // ordinary way and the framework does its half.

    private func buildParameterTree() {
        let n = Int(de_param_count(engine))
        guard n > 0 else { return }   // a cart that binds nothing shows an empty menu, as before
        var params: [AUParameter] = []
        for i in 0..<n {
            var addr: Int32 = 0, lo: Float = 0, hi: Float = 1, def: Float = 0
            var namePtr: UnsafePointer<CChar>? = nil
            guard de_param_info(engine, Int32(i), &addr, &namePtr, &lo, &hi, &def) != 0 else { continue }
            let name = namePtr.map { String(cString: $0) } ?? "P\(addr)"
            // ⚠ NO .flag_CanRamp. Claiming it invites the host to send ramped parameter events in the
            // render block's event list, which we would silently ignore: the engine applies a value
            // once per FRAME (loop_step), not per sample. Better to tell the truth and let the host
            // step the value than to advertise smoothing we do not do.
            let p = AUParameterTree.createParameter(
                withIdentifier: "p\(addr)", name: name,
                address: AUParameterAddress(addr),
                min: lo, max: hi, unit: .generic, unitName: nil,
                flags: [.flag_IsReadable, .flag_IsWritable],
                valueStrings: nil, dependentParameters: nil)
            p.value = def
            params.append(p)
        }
        guard !params.isEmpty else { return }
        let tree = AUParameterTree.createTree(withChildren: params)
        let eng = engine   // capture the pointer, never self — these blocks outlive nothing but must
                           // not resurrect the unit (the same rule the frame worker learned)
        // A host write. QUEUED by the engine and applied at the top of the next frame, which is what
        // makes this safe to call from the host's automation thread.
        tree.implementorValueObserver = { param, value in
            de_param_set(eng, Int32(param.address), value)
        }
        // A host read. Goes straight to the cart's float, so a knob the player just dragged reads
        // back correctly without the engine having to mirror anything.
        tree.implementorValueProvider = { param in
            de_param_get(eng, Int32(param.address))
        }
        parameterTree = tree
        deDiag("[tinyjam] AU PARAMS · \(params.count) exposed")
    }

    // The other direction: the PANEL moved a knob, so the host's lane should follow the glass. Polled
    // on the frame worker (never the render thread — AUParameter is not realtime-safe) right after
    // the frame that could have moved something.
    // ⚠ A host WRITE does not come back out of de_param_changed: the drain records it as already
    // reported. Without that, every automated value would be echoed back at the host that just sent
    // it, and a lane would fight the value it is writing.
    private func publishPanelMoves() {
        guard let tree = parameterTree else { return }
        var addr: Int32 = 0, v: Float = 0
        var guard_ = 0
        while de_param_changed(engine, &addr, &v) != 0 && guard_ < 64 {
            guard_ += 1
            tree.parameter(withAddress: AUParameterAddress(addr))?.setValue(v, originator: nil)
        }
    }

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
    private static var liveCount: UInt64 = 0        // created minus destroyed — see THE TEARDOWN LEDGER
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

    // ══ SESSION STATE ═══════════════════════════════════════════════════════════════════════════
    // Without this the plug-in has no memory: save a song with three racks, reopen it, and every one
    // is back at factory defaults — silently, and on the FIRST save anybody does. (Unlike the
    // two-tracks-share-one-engine defect, which needed two instances before it showed.)
    //
    // What travels is INTENT, not the context struct: the engine's sound-config log plus the cart
    // slices marked `de_state_for_saved`. The context is ~4 MB of pointers, GPU handles and derived
    // DSP scratch — meaningless to restore, and it would put megabytes in the host's project file.
    // A restore guarantees the ENGINE comes back with no held voices and nothing scheduled. It does
    // NOT put a cart's sequencer at step 0 — acidcandy derives its position from host transport, so
    // the host's playhead wins one frame later whatever the blob said. See runtime/platform.h.
    //
    // Gated by `bash tools/state-check/run.sh` (20 assertions, four negative controls) — which runs
    // the round trip on the desktop DE_NO_RAYLIB build, because nothing in the repo can instantiate
    // this class. Same blind spot that let three double-engine bugs ship; the engine half is covered,
    // the twelve lines below are not.
    //
    // ⚠ `super` FIRST, both ways. AUAudioUnit's own fullState carries what the host needs to
    // re-instantiate us at all (component description, preset bookkeeping). Returning only our key
    // would strip it; setting ours without passing the rest on would drop it.
    private static let stateKey = "dreamengineRack"

    // ── COMPRESSION ─────────────────────────────────────────────────────────────────────────────
    // The engine's blob is ~589 KB and MEASURED 99.5% ZERO BYTES: the rack is mostly pattern arrays
    // (steps × voices × patterns, times the autosave plus six song slots) and almost all of it is
    // empty. gzip took a real 437 KB on-disk bank to 774 bytes. So this is not a micro-optimisation,
    // it is the difference between a project file carrying half a megabyte per track and carrying
    // about a kilobyte.
    //
    // WHY IN SWIFT AND NOT THE ENGINE: what is big is the HOST's project file, and the host is
    // Apple-specific. A deflate compressor in studio.c would either add a dependency the engine does
    // not have (stb_image ships an INflater only) or tie it to Apple's libcompression, and the engine
    // has to stay portable. Nothing about the on-the-wire DES1 format changes.
    //
    // SELF-DESCRIBING, so old projects keep working: compressed blobs carry a "DEZ1" header + the
    // original length; a raw engine blob still starts "DES1" and is passed straight through. The
    // maker already saved test projects with raw blobs, and those must keep loading.
    private static let zMagic = Data("DEZ1".utf8)

    private static func pack(_ raw: Data) -> Data {
        guard let z = try? (raw as NSData).compressed(using: .zlib) as Data else { return raw }
        var out = zMagic
        var n = UInt32(raw.count).littleEndian
        withUnsafeBytes(of: &n) { out.append(contentsOf: $0) }
        out.append(z)
        // A pathological rack could in principle deflate larger than it started; then just ship raw.
        return out.count < raw.count ? out : raw
    }

    private static func unpack(_ d: Data) -> Data? {
        guard d.count >= 8, d.prefix(4).elementsEqual(zMagic) else { return d }   // raw DES1, or legacy
        var n: UInt32 = 0
        _ = withUnsafeMutableBytes(of: &n) { d.subdata(in: 4..<8).copyBytes(to: $0) }
        guard let un = try? (Data(d.dropFirst(8)) as NSData).decompressed(using: .zlib) as Data
        else { NSLog("[tinyjam] STATE could not inflate a %d-byte blob", d.count); return nil }
        guard un.count == Int(UInt32(littleEndian: n)) else {
            NSLog("[tinyjam] STATE inflated to %d bytes, header said %u", un.count, UInt32(littleEndian: n))
            return nil
        }
        return un
    }

    public override var fullState: [String: Any]? {
        get {
            var s = super.fullState ?? [:]
            let need = de_save_state(engine, nil, 0)          // size probe: writes nothing
            if need > 0 {
                var d = Data(count: Int(need))
                let wrote: Int32 = d.withUnsafeMutableBytes { raw in
                    de_save_state(engine, raw.baseAddress, need)
                }
                if wrote > 0 {
                    d.count = Int(wrote)
                    s[TinyjamAU.stateKey] = TinyjamAU.pack(d)
                } else {
                    NSLog("[tinyjam] STATE save produced nothing (need=%d) — rack will not persist", need)
                }
            }
            return s
        }
        set {
            super.fullState = newValue
            // No key = a project saved by a build before this shipped. Correct behaviour is to leave
            // the rack at its defaults, not to complain.
            guard let packed = newValue?[TinyjamAU.stateKey] as? Data, !packed.isEmpty else { return }
            guard let d = TinyjamAU.unpack(packed) else { return }   // unpack already said why
            let accepted: Int32 = d.withUnsafeBytes { raw in
                de_load_state(engine, raw.baseAddress, Int32(d.count))
            }
            // Three outcomes, all worth a line. REFUSED is not an error to swallow ("my sounds came
            // back wrong" needs a trail), and MIGRATED is the one that says an update did NOT eat a
            // saved song: an older project whose slices are shorter than this build's is prefix-
            // restored, with anything added since left at its default.
            switch accepted {
            case 1: break                                     // exact restore, the quiet common case
            case 2: NSLog("[tinyjam] STATE migrated a %d-byte blob from an older build — restored, new controls at their defaults", d.count)
            default: NSLog("[tinyjam] STATE refused a %d-byte blob (incompatible layout or ABI) — rack stays at defaults", d.count)
            }
        }
    }

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
    // ONE WORKER PER INSTANCE. It used to be one per process, which worked only while there was one
    // engine: a semaphore signal carries no identity, so a shared worker cannot know WHICH rack to
    // advance. Per-instance also makes teardown trivial — nothing is shared, so nothing is stranded.
    // A thread parked on a semaphore costs essentially nothing.
    // (A paragraph claiming the opposite — "ONE worker per process … it outlives every instance" —
    // stood here until 2026-08-15, contradicting the one above it in the same comment block.)
    private var worker: Thread?
    private let frameSignal = DispatchSemaphore(value: 0)
    private var frameCount: UInt64 = 0

    private func startWorker() {
        guard worker == nil else { return }
        // ⚠ THE THREAD MUST NOT HOLD A STRONG `self` ACROSS THE WAIT, or this unit can never
        // deallocate. `Thread { [weak self] in self?.workerLoop() }` READS as weak and is not: the
        // moment it unwraps, the call gets a strong reference for its whole duration, and
        // workerLoop() never returns. So `deinit` never fired, the four manual allocations below
        // leaked, and de_instance_destroy was never called — which is why the engine-side destroy
        // work of 2026-08-14 bought nothing on a device.
        //
        // The fix is which object the closure captures. The SEMAPHORE is captured strongly (it is
        // not self, and it must outlive the wait); self is re-acquired per iteration and held only
        // while a frame is actually running.
        //
        // That also makes teardown safe BY CONSTRUCTION rather than by timing: if the guard below
        // succeeds, a strong reference exists, so deinit cannot be running and cannot free the
        // engine under de_frame. If deinit IS running, the weak load yields nil and the thread
        // returns. There is no window where both are true, so no flag or lock is needed.
        let signal = frameSignal
        let t = Thread { [weak self] in
            while true {
                signal.wait()
                guard let s = self else { return }   // the unit is gone: end the thread
                s.frameCount &+= 1
                de_frame(s.engine, Double(s.frameCount) / 60.0)
                s.publishPanelMoves()   // a finger on the glass → the host's automation lane
            }
        }
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
    //
    // ⚠ `frameCount` is written by the worker and read here on MAIN, with no synchronisation. That
    // was dormant until this method got a caller (2026-08-15) and is deliberately left alone: the
    // load is an aligned 64-bit word, so it cannot tear on any platform we ship, and BOTH ways of
    // reading it stale are harmless — a value one frame behind signals a frame the audio side was
    // about to signal anyway (the worker coalesces, it does not queue work per signal), and a value
    // one frame ahead skips one display tick, i.e. ~16 ms of panel latency on a host that is by
    // definition idle. Making it atomic would cost an ordering argument on the audio path to buy
    // nothing observable.
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

        // SUSPECT 5, answered once per allocation rather than guessed. auval warned that we accept
        // layouts we cannot handle ("InputChan:4, OutputChan:5") because the AU declares no channel
        // capabilities, while the pull target below is two mono buffers. This prints what the host
        // ACTUALLY negotiated, which is the only way to tell a mishandled layout from a dead pull.
        if isEffect {
            inDiag.pointee = InDiag()        // counters are per allocation, not per process
            let o = _outputBusArray[0].format
            let inDesc: String
            if _inputBusArray.count > 0 {
                let f = _inputBusArray[0].format
                inDesc = "\(f.channelCount)ch @\(Int(f.sampleRate)) \(f.isInterleaved ? "interleaved" : "deinterleaved")"
            } else {
                inDesc = "NO INPUT BUS"      // would explain everything, in one word
            }
            deDiag("[tinyjam] INDIAG · negotiated: in \(inDesc) · out \(o.channelCount)ch @\(Int(o.sampleRate)) \(o.isInterleaved ? "interleaved" : "deinterleaved") · maxFrames \(maximumFramesToRender)")
            startInDiag()
        }
    }

    public override func deallocateRenderResources() {
        inDiagTimer?.cancel(); inDiagTimer = nil
        // ONE FINAL LINE, or the probe cannot see a short-lived host. auval instantiates, renders a
        // burst and tears down inside a second, so the 1 Hz timer never fired for it and it reported
        // NOTHING — which made the one control that matters (a host known to feed real input)
        // unreadable, and looked exactly like "auval does not render". Summarise on the way out.
        if isEffect, inDiag.pointee.pulls > 0 {
            let v = inDiag.pointee
            let db = v.peak > 0 ? 20 * log10(Double(v.peak)) : -999.0
            deDiag(String(format:
                "[tinyjam] INDIAG · FINAL inst %llu · pulls %llu · FAIL %llu · UNWRITTEN %llu · oversize %llu · nonfinite %llu · pushed %llu smp · peak %.5f (%.1f dBFS) · lastN %d · ablCount %d · lastErr %d",
                instanceID, v.pulls, v.fails, v.unwritten, v.oversize, v.nonfinite, v.pushed, Double(v.peak), db,
                v.lastN, v.lastABL, v.lastErr))
        }
        super.deallocateRenderResources()
    }

    // One line a second while the effect renders. It captures the POINTER and the id, never `self`:
    // a timer holding the AU would keep it alive past deinit and break the teardown ledger.
    private func startInDiag() {
        guard isEffect, inDiagTimer == nil else { return }
        let t = DispatchSource.makeTimerSource(queue: DispatchQueue.global(qos: .utility))
        t.schedule(deadline: .now() + 1.0, repeating: 1.0)
        let d = inDiag, id = instanceID
        t.setEventHandler {
            let v = d.pointee
            d.pointee.peak = 0               // per-report, so a second of silence reads 0.00000
            if v.pulls == 0 { return }       // not rendering: stay quiet rather than fill the log
            let db = v.peak > 0 ? 20 * log10(Double(v.peak)) : -999.0
            deDiag(String(format:
                "[tinyjam] INDIAG · inst %llu · pulls %llu · FAIL %llu · UNWRITTEN %llu · oversize %llu · nonfinite %llu · pushed %llu smp · peak %.5f (%.1f dBFS) · lastN %d · ablCount %d · lastErr %d",
                id, v.pulls, v.fails, v.unwritten, v.oversize, v.nonfinite, v.pushed, Double(v.peak), db,
                v.lastN, v.lastABL, v.lastErr))
            // §4.1c: what the ENGINE holds on the master bus, printed beside the input reading. The
            // cart pushed its chain once in init() and, being set-and-hold, will not push again
            // until a control moves — so if this reads 0 at boot and jumps the moment the maker
            // touches a pedal, the engine lost the chain after init and the hypothesis is proved.
            // Logged from the TIMER, never the audio thread: this walks engine state.
            var kinds = [Int32](repeating: 0, count: 16)
            let cn = kinds.withUnsafeMutableBufferPointer { de_fx_chain_probe(0, $0.baseAddress, 16) }
            var list = ""
            if cn > 0 { for i in 0..<Int(min(cn, 16)) { list += (i == 0 ? "" : ",") + String(kinds[i]) } }
            deDiag("[tinyjam] FXCHAIN · bus 0 holds \(cn) insert(s)" + (cn > 0 ? " · kinds \(list)" : " · EMPTY")
                   + " · dropped \(de_sound_dropped())")
        }
        t.resume()
        inDiagTimer = t
    }

    public override var internalRenderBlock: AUInternalRenderBlock {
        let scratch = self.scratch, cap = self.scratchCap, acc = self.acc
        let echunk = self.echunk, rate = self.rate, signal = self.frameSignal
        let spf = TinyjamAU.SAMPLES_PER_FRAME
        // Effect input, resolved off the audio thread like everything else in this capture list. The
        // HOST RATE is read here rather than in the block because that is where it is knowable: the
        // render block is re-fetched after allocateRenderResources, which is what negotiates it.
        let isEffect = self.isEffect, inScratch = self.inScratch, inMono = self.inMono, inABL = self.inABL
        let inDiag = self.inDiag
        let hostRate = Int32(_outputBusArray[0].format.sampleRate.rounded())
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
        return { _, timestamp, frameCount, _, outputData, eventListHead, pullInput in
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
                    de_sync_position(engine, beat, tempo, playing ? 1 : 0)
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
                        case 0x90: de_midi_event(engine, d2 > 0 ? 1 : -1, Int32(d1), Int32(d2))   // note-on (vel 0 = off)
                        case 0x80: de_midi_event(engine, -1, Int32(d1), Int32(d2))                // note-off
                        case 0xE0: de_midi_bend(engine, Int32(((d2 << 7) | d1) - 8192))           // pitch-bend
                        // CONTROL CHANGE — the host's MOD WHEEL is CC1, and a DAW's automation lanes
                        // ride this too, so until now NO continuous host control reached the engine at
                        // all. The channel nibble is KEPT (the engine's CC path is channel-aware by
                        // design, unlike its omni note path).
                        case 0xB0: de_midi_cc(engine, Int32(m.data.0 & 0x0F), Int32(d1), Int32(d2))
                        default: break
                        }
                    }
                }
                ev = UnsafePointer(e.pointee.head.next)
            }

            // 1.5) EFFECT INPUT → the engine's input ring, BEFORE anything below renders a sample.
            //      THE ORDERING IS THE LATENCY. tools/insert-latency.js measured this path at 0
            //      samples, and that zero is a property of push-then-render at 1:1, not of the ring:
            //      its reader is aligned to its writer on start and then takes exactly one sample per
            //      output sample. Push AFTER rendering and every sample is a full block late, for
            //      free, with nothing to show it but a phase shift nobody looks at.
            //      ⚠ Still not necessarily zero IN A HOST: the engine renders in whole 735-sample cart
            //      frames while a host may hand us 512, so input pushed now can be read by a frame
            //      that renders later. The ring absorbs that (it is what it is for) at up to one
            //      engine frame of delay. MEASURE IN THE HOST before claiming a `latency` to it.
            if isEffect, let pull = pullInput {
                var flags = AudioUnitRenderActionFlags()
                // Point the pull target at our own preallocated memory and state the size, every
                // block: a render block must not allocate, and the host is entitled to hand us a
                // different frameCount each time.
                let half = 16384
                inABL[0].mNumberChannels = 1
                inABL[1].mNumberChannels = 1
                inABL[0].mDataByteSize = UInt32(n * 4)
                inABL[1].mDataByteSize = UInt32(n * 4)
                inABL[0].mData = UnsafeMutableRawPointer(inScratch)
                inABL[1].mData = UnsafeMutableRawPointer(inScratch + half)
                // Split into three named outcomes rather than one silent `if`. Each records which
                // of §4.1b's suspects is alive: an oversize block, a refused pull (with the host's
                // own OSStatus, which usually names the cause), or audio that arrived and can be
                // measured. Counting costs a few adds per block on a path that already loops n.
                inDiag.pointee.pulls &+= 1
                inDiag.pointee.lastN = Int32(n)
                var st = OSStatus(noErr)
                if n > half { inDiag.pointee.oversize &+= 1; st = -1 }
                else {
                    // POISON, then pull. -777 is not a value any audio path produces, so if it is
                    // still there afterwards the host did not write our buffers — which noErr does
                    // not tell us. Four stores per block; the ends of both channels are enough,
                    // since a host writes a whole buffer or none of it.
                    let SENT = Float(-777)
                    inScratch[0] = SENT; inScratch[n - 1] = SENT
                    inScratch[half] = SENT; inScratch[half + n - 1] = SENT
                    st = pull(&flags, timestamp, frameCount, 0, inABL.unsafeMutablePointer)
                    if st != noErr { inDiag.pointee.fails &+= 1; inDiag.pointee.lastErr = st }
                    // NOTE: the scratch keeping its poison is NOT a fault by itself — it just means
                    // the host rendered into its own buffer, which is legal and normal. UNWRITTEN is
                    // counted below, per channel, on the pointers the host actually returned.
                }
                if st == noErr {
                    // ⚠ READ THE POINTERS THE HOST LEFT IN THE ABL, NEVER `inScratch`. This was the
                    // whole defect. An upstream is ENTITLED to ignore the buffers you offer and
                    // point mData at its own instead — that is how no-copy rendering works, and it
                    // returns noErr while your scratch is never touched. We set mData to inScratch
                    // and then read inScratch, so we fed the engine whatever was already there:
                    // kernel-zeroed pages, i.e. perfect silence, with every counter green. Proven
                    // by the sentinel above coming back intact on 100% of pulls under auval.
                    // MONO, because that is what the ring takes (mic_input_push is one channel — it
                    // was built for a microphone). Averaging L+R is the honest downmix for a pedal
                    // chain whose effects are mono-in anyway; a stereo insert path is a later job.
                    inDiag.pointee.lastABL = Int32(inABL.count)
                    let b0 = inABL[0]
                    let p0 = b0.mData?.assumingMemoryBound(to: Float.self)
                    let p1 = inABL.count >= 2 ? inABL[1].mData?.assumingMemoryBound(to: Float.self) : nil
                    // PER CHANNEL, because the two can disagree: one run came back with buffer 0
                    // still ours (poison intact) and buffer 1 the host's, and averaging them fed
                    // the engine 388.5 — half the sentinel. A channel still carrying the poison was
                    // never written, so it contributes NOTHING rather than a huge DC step. The
                    // poison must never reach the engine; that would be a worse bug than silence.
                    let SENT = Float(-777)
                    func live(_ p: UnsafeMutablePointer<Float>?) -> Bool {
                        guard let p = p else { return false }
                        return !(p[0] == SENT && p[n - 1] == SENT)
                    }
                    let l0 = live(p0), l1 = live(p1)
                    // Take the layout from what came BACK, not from what we asked for — the honest
                    // answer to suspect 5, since a host handing us a shape we did not expect is now
                    // downmixed correctly instead of misread.
                    if b0.mNumberChannels == 2, let L = p0, l0 {
                        for i in 0..<n { inMono[i] = (L[2 * i] + L[2 * i + 1]) * 0.5 }  // interleaved
                    } else if let L = p0, let R = p1, l0, l1 {
                        for i in 0..<n { inMono[i] = (L[i] + R[i]) * 0.5 }              // deinterleaved
                    } else if let L = p0, l0 {
                        for i in 0..<n { inMono[i] = L[i] }                             // left only
                    } else if let R = p1, l1 {
                        for i in 0..<n { inMono[i] = R[i] }                             // right only
                    } else {
                        for i in 0..<n { inMono[i] = 0 }                                // nothing written
                        inDiag.pointee.unwritten &+= 1
                    }
                    // THE LEVEL PROBE, read AFTER the downmix and BEFORE the engine: this is the one
                    // number that says whether the host handed us sound at all. A successful pull
                    // carrying digital silence looks identical to a failed pull from the outside,
                    // and the two mean opposite things.
                    var pk = Float(0)
                    var bad = false
                    for i in 0..<n {
                        let a = abs(inMono[i])
                        if !a.isFinite { bad = true; continue }   // one inf must not eat the reading
                        if a > pk { pk = a }
                    }
                    if bad { inDiag.pointee.nonfinite &+= 1 }
                    if pk > inDiag.pointee.peak { inDiag.pointee.peak = pk }
                    inDiag.pointee.pushed &+= UInt64(n)
                    // No engine handle: de_audio_input is the process-global seam (see the ⚠ on
                    // isEffect). hostRate, not ENGINE_RATE — mic_input_push resamples when they
                    // differ, and lying here would pitch the host's track by the rate ratio.
                    de_audio_input(inMono, Int32(n), hostRate)
                }
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

    // Reachable since 2026-08-15 — see startWorker for what was holding it. Everything here was
    // already written except the destroy; none of it had ever run.
    deinit {
        // Wake the parked worker so it observes a nil `self` and returns. Without this the thread
        // stays blocked on the semaphore for the life of the process: harmless to this object, which
        // is already deallocating, but it strands a thread per rack the user ever loaded.
        frameSignal.signal()
        // The other half of THE TEARDOWN LEDGER (see init). If this line never appears when you
        // remove the plug-in, `deinit` is not firing and everything below it is dead code — which
        // is exactly the state this file shipped in until 2026-08-15.
        TinyjamAU.bootLock.lock()
        if TinyjamAU.liveCount > 0 { TinyjamAU.liveCount -= 1 }
        let live = TinyjamAU.liveCount
        TinyjamAU.bootLock.unlock()
        deDiag(String(format: "[tinyjam] AU DESTROY · instance %llu · %llu live", instanceID, live))
        // GIVE THE RACK BACK. de_instance_destroy releases the canvas, present, state and sample
        // buffers as well as the struct (studio.c + sound_free_buffers), which is ~1 MB per rack.
        // Safe here for the reason spelled out in startWorker: the worker cannot be inside de_frame
        // while this runs. The other reader is the panel's blitter, and the view controller cuts it
        // loose in its own deinit, which runs first.
        de_instance_destroy(engine)
        inDiagTimer?.cancel(); inDiagTimer = nil
        scratch.deallocate(); acc.deallocate(); echunk.deallocate(); rate.deallocate()
        inDiag.deallocate()
    }
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
