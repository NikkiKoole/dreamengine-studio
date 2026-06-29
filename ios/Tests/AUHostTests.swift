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

        // 1) a keybed instrument (epiano) is SILENT until the host sends a note.
        let silentPeak = try renderPeak(11025)               // ~0.25s, no MIDI
        NSLog("[auhost] idle (no MIDI) peak=%.3f", silentPeak)
        XCTAssertLessThan(silentPeak, 0.02, "instrument made sound with no MIDI input")

        // 2) send a note-on (middle C, vel 100) via the host's MIDI schedule block, render → sound.
        let sched = avAU.auAudioUnit.scheduleMIDIEventBlock
        XCTAssertNotNil(sched, "AUv3 does not expose scheduleMIDIEventBlock (no MIDI input path)")
        let noteOn: [UInt8] = [0x90, 60, 100]
        sched?(AUEventSampleTimeImmediate, 0, noteOn.count, noteOn)
        let playedPeak = try renderPeak(22050)               // ~0.5s with the note held
        NSLog("[auhost] played (note-on 60) peak=%.3f", playedPeak)
        XCTAssertGreaterThan(playedPeak, 0.05, "AUv3 produced no sound for a host MIDI note")

        let noteOff: [UInt8] = [0x80, 60, 0]
        sched?(AUEventSampleTimeImmediate, 0, noteOff.count, noteOff)
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
