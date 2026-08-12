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
//   2. de_frame() advances one 60Hz tick of cart logic — the cart's keybed drains the MIDI ring
//      (keybed_update → note_on/off) and plays. SAMPLE-CLOCKED — one frame per 735 rendered samples
//      (44100/60), NOT a wall-clock timer — so it stays correct under a host's OFFLINE render too.
//   3. de_audio_render() = sound.h's mixer, filling interleaved stereo.
//   4. if the host asked for a rate other than 44100, RESAMPLE to it (see "the rate seam" below).
//
// All of it runs on the audio thread, in order, so there are NO cross-thread races on engine state
// (the MIDI ring's producer and consumer are the same thread here). The staged cart (gen/au/cart.c)
// is a KEYBED instrument (epiano): silent until the host sends notes, then it plays them. One engine
// instance per process: studio.c/sound.h use file-scope globals; iOS loads the AUv3 out-of-process,
// and one hosted rack is the case we support (multiple instances in one process would share state).
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
    // sample-clock state, owned by the render block (single audio thread).
    private let acc = UnsafeMutablePointer<Int>.allocate(capacity: 1)     // samples since last frame
    private let frame = UnsafeMutablePointer<UInt64>.allocate(capacity: 1)
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
        try super.init(componentDescription: componentDescription, options: options)
        let outBus = try AUAudioUnitBus(format: format)
        _outputBusArray = AUAudioUnitBusArray(audioUnit: self, busType: .output, busses: [outBus])
        _inputBusArray  = AUAudioUnitBusArray(audioUnit: self, busType: .input,  busses: [])
        acc.pointee = 0
        frame.pointee = 0
        rate.initialize(to: RateState())
        de_init(DE_RENDERER_SOFTWARE)        // sound_init() + the cart's init()
    }

    public override var outputBusses: AUAudioUnitBusArray { _outputBusArray }
    public override var inputBusses: AUAudioUnitBusArray { _inputBusArray }

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
        let scratch = self.scratch, cap = self.scratchCap, acc = self.acc, frame = self.frame
        let echunk = self.echunk, rate = self.rate
        let spf = TinyjamAU.SAMPLES_PER_FRAME
        // Capture SELF unretained, not the two host blocks themselves. The host assigns
        // musicalContextBlock / transportStateBlock AFTER it fetches this render block, so reading
        // them out here would capture nil forever — the documented AUv3 trap. takeUnretainedValue()
        // adds no retain/release, so it is safe on the audio thread.
        let unownedSelf = Unmanaged.passUnretained(self)
        return { _, _, frameCount, _, outputData, eventListHead, _ in
            let n = Int(frameCount)
            if n * 2 > cap { return kAudioUnitErr_TooManyFramesToProcess }
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

            if rate.pointee.rc.passthrough {
                // ── 2a) THE HOST IS AT OUR RATE (44100): the original path, byte for byte. ────────
                //    advance the sequencer in step with rendered audio (one de_frame per 735 samples).
                //    The cart's update() drains the MIDI ring (keybed_update → note_on/off) here.
                acc.pointee += n
                while acc.pointee >= spf {
                    acc.pointee -= spf
                    frame.pointee &+= 1
                    de_frame(Double(frame.pointee) / 60.0)
                }
                de_audio_render(scratch, Int32(n))                   // sound.h mixer → interleaved L,R
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
                            frame.pointee &+= 1
                            de_frame(Double(frame.pointee) / 60.0)
                            de_audio_render(echunk, Int32(spf))
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

    deinit { scratch.deallocate(); acc.deallocate(); frame.deallocate()
             echunk.deallocate(); rate.deallocate() }
}

// NSExtensionPrincipalClass — the system instantiates this to get the AU.
public final class TinyjamAUFactory: NSObject, AUAudioUnitFactory {
    public func beginRequest(with context: NSExtensionContext) {}
    public func createAudioUnit(with componentDescription: AudioComponentDescription) throws -> AUAudioUnit {
        try TinyjamAU(componentDescription: componentDescription, options: [])
    }
}
