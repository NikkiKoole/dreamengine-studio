import AVFoundation

// CoreAudio render: an AVAudioSourceNode pulls from the REAL engine mixer (de_audio_render,
// = sound.h's sound_callback) once per audio quantum and feeds the main mixer. de_audio_render
// fills STEREO INTERLEAVED floats @ 44100; we use a non-interleaved stereo node and split the
// engine's interleaved buffer into the two channel planes. de_init() (CanvasView) must have run
// first — it calls sound_init(); the audio thread only ever pulls, never inits.
final class AudioEngine {
    static let shared = AudioEngine()
    private let engine = AVAudioEngine()
    private var src: AVAudioSourceNode?
    private var started = false
    private var scratch = [Float](repeating: 0, count: 8192 * 2)   // interleaved L,R,L,R…

    func start() {
        guard !started else { return }
        let sr = 44100.0

        let fmt = AVAudioFormat(standardFormatWithSampleRate: sr, channels: 2)!
        let node = AVAudioSourceNode { [weak self] _, _, frameCount, ablPointer -> OSStatus in
            guard let self = self else { return noErr }
            let n = Int(frameCount)
            let abl = UnsafeMutableAudioBufferListPointer(ablPointer)
            if self.scratch.count < n * 2 { self.scratch = [Float](repeating: 0, count: n * 2) }
            self.scratch.withUnsafeMutableBufferPointer { buf in
                de_audio_render(buf.baseAddress!, Int32(n))       // fills L,R interleaved
                let interleaved = buf.baseAddress!
                if abl.count >= 2,
                   let l = abl[0].mData?.assumingMemoryBound(to: Float.self),
                   let r = abl[1].mData?.assumingMemoryBound(to: Float.self) {
                    for i in 0..<n { l[i] = interleaved[i*2]; r[i] = interleaved[i*2 + 1] }
                } else if let l = abl.first?.mData?.assumingMemoryBound(to: Float.self) {
                    for i in 0..<n { l[i] = interleaved[i*2] }    // mono fallback: left only
                }
            }
            return noErr
        }
        src = node
        engine.attach(node)
        engine.connect(node, to: engine.mainMixerNode, format: fmt)

        do {
            try AVAudioSession.sharedInstance().setCategory(.playback, mode: .default)
            try AVAudioSession.sharedInstance().setActive(true)
            try engine.start()
            started = true
            NSLog("[tinyjam] audio engine started @ %.0f Hz (stereo, real mixer)", sr)
        } catch {
            NSLog("[tinyjam] audio start failed: %@", error.localizedDescription)
        }
    }
}
