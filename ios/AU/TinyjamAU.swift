import AVFoundation

// The AUv3 instrument extension. Its render block calls the SAME C synth as the standalone
// app (de_audio_render in audio.c) — so a "Tinyjam rack" loaded inside a host (AUM,
// GarageBand, Logic, or our own test host) plays the exact engine the app does.
public final class TinyjamAU: AUAudioUnit {
    private let format = AVAudioFormat(standardFormatWithSampleRate: 44100, channels: 2)!
    private var _outputBusArray: AUAudioUnitBusArray!
    private var _inputBusArray: AUAudioUnitBusArray!

    public override init(componentDescription: AudioComponentDescription,
                         options: AudioComponentInstantiationOptions = []) throws {
        try super.init(componentDescription: componentDescription, options: options)
        let outBus = try AUAudioUnitBus(format: format)
        _outputBusArray = AUAudioUnitBusArray(audioUnit: self, busType: .output, busses: [outBus])
        _inputBusArray  = AUAudioUnitBusArray(audioUnit: self, busType: .input,  busses: [])
        de_audio_init(Int32(format.sampleRate))
    }

    public override var outputBusses: AUAudioUnitBusArray { _outputBusArray }
    public override var inputBusses: AUAudioUnitBusArray { _inputBusArray }

    public override var internalRenderBlock: AUInternalRenderBlock {
        return { _, _, frameCount, _, outputData, _, _ in
            let abl = UnsafeMutableAudioBufferListPointer(outputData)
            let n = Int(frameCount)
            guard let ch0raw = abl.first?.mData else { return noErr }
            let ch0 = ch0raw.assumingMemoryBound(to: Float.self)
            de_audio_render(ch0, Int32(n))                 // fill channel 0 from the C synth
            for i in 1..<abl.count {                        // copy to remaining channels
                if let dst = abl[i].mData {
                    memcpy(dst, ch0raw, n * MemoryLayout<Float>.size)
                }
            }
            return noErr
        }
    }
}

// NSExtensionPrincipalClass — the system instantiates this to get the AU.
public final class TinyjamAUFactory: NSObject, AUAudioUnitFactory {
    public func beginRequest(with context: NSExtensionContext) {}
    public func createAudioUnit(with componentDescription: AudioComponentDescription) throws -> AUAudioUnit {
        try TinyjamAU(componentDescription: componentDescription, options: [])
    }
}
