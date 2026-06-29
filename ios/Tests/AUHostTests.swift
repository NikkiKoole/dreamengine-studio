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
        var peak: Float = 0, rendered = 0
        while rendered < 22050 {                              // ~0.5s
            let status = try engine.renderOffline(2048, to: buf)
            guard status == .success else { break }
            if let ch = buf.floatChannelData {
                for i in 0..<Int(buf.frameLength) { peak = max(peak, abs(ch[0][i])) }
            }
            rendered += Int(buf.frameLength)
        }
        engine.stop()
        NSLog("[auhost] rendered %d frames through the AUv3, peak=%.3f", rendered, peak)
        XCTAssertGreaterThan(peak, 0.01, "AUv3 produced silence")
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
