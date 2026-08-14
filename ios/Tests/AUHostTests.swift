import XCTest
import AVFoundation
@testable import TinyjamHello

// Our own minimal AUv3 host — no GarageBand/AUM/device needed. Finds the embedded Tinyjam
// extension, instantiates it, renders it offline through an AVAudioEngine, and asserts it
// produces sound. This proves the extension is a valid, instantiable, audible AUv3.
final class AUHostTests: XCTestCase {
    private func fourCC(_ s: String) -> OSType {
        var r: OSType = 0
        for b in s.utf8 { r = (r << 8) | OSType(b) }
        return r
    }

    private var desc: AudioComponentDescription {
        AudioComponentDescription(componentType: kAudioUnitType_MusicDevice,
                                  componentSubType: fourCC("tnyj"),
                                  componentManufacturer: fourCC("Tnyj"),
                                  componentFlags: 0, componentFlagsMask: 0)
    }

    func testExtensionIsRegistered() {
        let comps = AVAudioUnitComponentManager.shared().components(matching: desc)
        XCTAssertFalse(comps.isEmpty, "Tinyjam AUv3 not found in the component registry")
        NSLog("[auhost] found %d matching component(s): %@", comps.count,
              comps.map { $0.name }.joined(separator: ", "))
    }

    func testExtensionInstantiatesAndRenders() async throws {
        let avAU = try await instantiate(desc)

        let engine = AVAudioEngine()
        engine.attach(avAU)
        let fmt = AVAudioFormat(standardFormatWithSampleRate: 44100, channels: 2)!
        engine.connect(avAU, to: engine.mainMixerNode, format: fmt)
        try engine.enableManualRenderingMode(.offline, format: fmt, maximumFrameCount: 4096)
        try engine.start()

        let buf = AVAudioPCMBuffer(pcmFormat: engine.manualRenderingFormat, frameCapacity: 4096)!
        func renderPeak(_ frames: Int) throws -> Float {
            var peak: Float = 0, done = 0
            while done < frames {
                guard try engine.renderOffline(2048, to: buf) == .success else { break }
                if let ch = buf.floatChannelData {
                    for i in 0..<Int(buf.frameLength) { peak = max(peak, abs(ch[0][i])) }
                }
                done += Int(buf.frameLength)
            }
            return peak
        }

        // ⚠ WHICH CART IS EMBEDDED CHANGES WHAT "CORRECT" MEANS, and this test used to assume one.
        // It asserted SILENCE with no MIDI, which is right for a keybed (epiano, the old AU_CART)
        // and plain wrong for a self-running sequencer (acidcandy, what the plug-in ships since
        // 2026-08-14) — that one plays on its own, which is the whole point of it. The AU was fine;
        // the test's premise had gone stale. So branch on what the cart demonstrably IS, and assert
        // something real either way rather than relaxing the bar to whatever passes.
        let idlePeak = try renderPeak(11025)                 // ~0.25s, no MIDI
        NSLog("[auhost] idle (no MIDI) peak=%.3f", idlePeak)

        let sched = avAU.auAudioUnit.scheduleMIDIEventBlock
        XCTAssertNotNil(sched, "AUv3 does not expose scheduleMIDIEventBlock (no MIDI input path)")

        if idlePeak < 0.02 {
            // MIDI-DRIVEN CART (epiano). The strong assertion: a host note-on must make sound.
            NSLog("[auhost] cart is MIDI-driven — asserting the host MIDI path")
            let noteOn: [UInt8] = [0x90, 60, 100]
            sched?(AUEventSampleTimeImmediate, 0, noteOn.count, noteOn)
            let playedPeak = try renderPeak(22050)           // ~0.5s with the note held
            NSLog("[auhost] played (note-on 60) peak=%.3f", playedPeak)
            XCTAssertGreaterThan(playedPeak, 0.05, "AUv3 produced no sound for a host MIDI note")
            let noteOff: [UInt8] = [0x80, 60, 0]
            sched?(AUEventSampleTimeImmediate, 0, noteOff.count, noteOff)
        } else {
            // SELF-RUNNING RACK (acidcandy). It is audible by construction, so "it made sound" is
            // the liveness assertion, and it still has to KEEP running — a rack that fires once and
            // dies would pass a single peak check.
            NSLog("[auhost] cart self-runs — asserting sustained output")
            let laterPeak = try renderPeak(22050)
            NSLog("[auhost] sustained peak=%.3f", laterPeak)
            XCTAssertGreaterThan(laterPeak, 0.02, "the rack stopped producing audio after the first render")
            // ⚠ HONEST GAP, stated so a green run is not read as more than it is: this branch does
            // NOT cover the host MIDI note path (scheduleMIDIEventBlock → de_midi_event → a voice),
            // because a self-running rack sounds the same either way. To exercise it, build the
            // extension with the keybed cart: `AU_CART=epiano ./build.sh`, then re-run this test.
            // The engine-side MIDI note path has its own end-to-end gate in tools/midi-check
            // (phase C), which is a different route to the same code.
        }
        engine.stop()
    }

    private func instantiate(_ d: AudioComponentDescription) async throws -> AVAudioUnit {
        try await withCheckedThrowingContinuation { cont in
            AVAudioUnit.instantiate(with: d, options: []) { au, err in   // iOS always loads out-of-process
                if let au = au { cont.resume(returning: au) }
                else { cont.resume(throwing: err ?? NSError(domain: "auhost", code: -1)) }
            }
        }
    }
}
