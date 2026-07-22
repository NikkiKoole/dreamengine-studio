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

    // ── microphone INPUT (mic-and-sampling.md Tier-1) ────────────────────────
    // The engine's mic host: capture is opened LAZILY, only when a cart calls mic_start()
    // (de_mic_wanted() flips to 1), so a cart that never listens never prompts for permission
    // and never switches the audio session to record. pollMic() is the iOS twin of the desktop
    // mic_desktop_poll(); CanvasView calls it once per display-link tick.
    private var micRunning = false
    private var micRequesting = false

    func pollMic() {
        let want = de_mic_wanted() != 0
        if      want && !micRunning && !micRequesting { requestAndStartMic() }
        else if !want && micRunning                   { stopMic() }
    }

    private func requestAndStartMic() {
        micRequesting = true
        AVAudioSession.sharedInstance().requestRecordPermission { [weak self] granted in
            DispatchQueue.main.async {
                guard let self = self else { return }
                self.micRequesting = false
                guard granted, de_mic_wanted() != 0 else { return }   // denied, or cart bailed while prompting
                self.installMicTap()
            }
        }
    }

    private func installMicTap() {
        guard !micRunning else { return }
        // The session is ALREADY .playAndRecord (set once at start) and the engine is running, so we
        // just tap the input — NO engine stop/restart and NO category switch. The old code stopped +
        // reconfigured + restarted the engine here, which dropped the output render node → the whole
        // app went silent when GUITAR IN turned on. Tapping a live engine leaves output untouched.
        let input = engine.inputNode
        let fmt = input.inputFormat(forBus: 0)
        guard fmt.channelCount > 0, fmt.sampleRate > 0 else {
            NSLog("[tinyjam] no mic input channels"); return
        }
        let sr = Int32(fmt.sampleRate)
        input.installTap(onBus: 0, bufferSize: 2048, format: fmt) { buffer, _ in
            guard let ch = buffer.floatChannelData else { return }
            de_audio_input(ch[0], Int32(buffer.frameLength), sr)   // channel 0 = the mono take
        }
        micRunning = true
        de_mic_set_active(1)
        NSLog("[tinyjam] mic tap installed @ %.0f Hz", fmt.sampleRate)
    }

    private func stopMic() {
        engine.inputNode.removeTap(onBus: 0)   // stops input use → iOS drops the mic indicator; output keeps running
        micRunning = false
        de_mic_set_active(0)
    }

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
            // .playAndRecord from the START (not .playback) so enabling the mic later is a no-op tap
            // with zero output disruption. This does NOT prompt for mic permission — only actually
            // reading the input (requestRecordPermission / installing the input tap) does — so a cart
            // that never listens never prompts. .defaultToSpeaker routes playback to the loud speaker;
            // .allowBluetoothA2DP keeps Bluetooth OUTPUT hi-fi (input falls back to the built-in mic).
            try AVAudioSession.sharedInstance().setCategory(.playAndRecord, mode: .default,
                                                            options: [.defaultToSpeaker, .allowBluetoothA2DP])
            try AVAudioSession.sharedInstance().setActive(true)
            try engine.start()
            started = true
            NSLog("[tinyjam] audio engine started @ %.0f Hz (stereo, real mixer)", sr)
        } catch {
            NSLog("[tinyjam] audio start failed: %@", error.localizedDescription)
        }
    }
}
