import AVFoundation

// The AUv3 instrument extension — hosting the REAL dreamengine (not the spike arpeggio), played
// by host MIDI. It runs the same engine the standalone app does. Each render block, in order:
//
//   1. feed host MIDI into the engine's ring (de_midi_event), parsed from the realtime event list.
//   2. de_frame() advances one 60Hz tick of cart logic — the cart's keybed drains the MIDI ring
//      (keybed_update → note_on/off) and plays. SAMPLE-CLOCKED — one frame per 735 rendered samples
//      (44100/60), NOT a wall-clock timer — so it stays correct under a host's OFFLINE render too.
//   3. de_audio_render() = sound.h's mixer, filling interleaved stereo.
//
// All three run on the audio thread, in order, so there are NO cross-thread races on engine state
// (the MIDI ring's producer and consumer are the same thread here). The staged cart (gen/au/cart.c)
// is a KEYBED instrument (epiano): silent until the host sends notes, then it plays them. One engine
// instance per process: studio.c/sound.h use file-scope globals; iOS loads the AUv3 out-of-process,
// and one hosted rack is the case we support (multiple instances in one process would share state).
public final class TinyjamAU: AUAudioUnit {
    private let format = AVAudioFormat(standardFormatWithSampleRate: 44100, channels: 2)!
    private var _outputBusArray: AUAudioUnitBusArray!
    private var _inputBusArray: AUAudioUnitBusArray!

    private static let SAMPLES_PER_FRAME = 735      // 44100 / 60
    // stable interleaved L,R scratch — allocated once so the render block never allocates on the
    // audio thread. Generous cap; n*2 is asserted ≤ this in the render block.
    private let scratchCap = 16384 * 2
    private let scratch = UnsafeMutablePointer<Float>.allocate(capacity: 16384 * 2)
    // sample-clock state, owned by the render block (single audio thread).
    private let acc = UnsafeMutablePointer<Int>.allocate(capacity: 1)     // samples since last frame
    private let frame = UnsafeMutablePointer<UInt64>.allocate(capacity: 1)

    public override init(componentDescription: AudioComponentDescription,
                         options: AudioComponentInstantiationOptions = []) throws {
        try super.init(componentDescription: componentDescription, options: options)
        let outBus = try AUAudioUnitBus(format: format)
        _outputBusArray = AUAudioUnitBusArray(audioUnit: self, busType: .output, busses: [outBus])
        _inputBusArray  = AUAudioUnitBusArray(audioUnit: self, busType: .input,  busses: [])
        acc.pointee = 0
        frame.pointee = 0
        de_init(DE_RENDERER_SOFTWARE)        // sound_init() + the cart's init()
    }

    public override var outputBusses: AUAudioUnitBusArray { _outputBusArray }
    public override var inputBusses: AUAudioUnitBusArray { _inputBusArray }

    public override var internalRenderBlock: AUInternalRenderBlock {
        let scratch = self.scratch, cap = self.scratchCap, acc = self.acc, frame = self.frame
        let spf = TinyjamAU.SAMPLES_PER_FRAME
        return { _, _, frameCount, _, outputData, eventListHead, _ in
            let n = Int(frameCount)
            if n * 2 > cap { return kAudioUnitErr_TooManyFramesToProcess }
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
            // 2) advance the sequencer in step with rendered audio (one de_frame per 735 samples).
            //    The cart's update() drains the MIDI ring (keybed_update → note_on/off) here.
            acc.pointee += n
            while acc.pointee >= spf {
                acc.pointee -= spf
                frame.pointee &+= 1
                de_frame(Double(frame.pointee) / 60.0)
            }
            let abl = UnsafeMutableAudioBufferListPointer(outputData)
            de_audio_render(scratch, Int32(n))                       // sound.h mixer → interleaved L,R
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

    deinit { scratch.deallocate(); acc.deallocate(); frame.deallocate() }
}

// NSExtensionPrincipalClass — the system instantiates this to get the AU.
public final class TinyjamAUFactory: NSObject, AUAudioUnitFactory {
    public func beginRequest(with context: NSExtensionContext) {}
    public func createAudioUnit(with componentDescription: AudioComponentDescription) throws -> AUAudioUnit {
        try TinyjamAU(componentDescription: componentDescription, options: [])
    }
}
