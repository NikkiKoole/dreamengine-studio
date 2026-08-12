// au-transport-check.swift — the headless gate for AUv3 HOST TRANSPORT (phase 2 of the AU arc).
//
// WHY THIS EXISTS: everything about host sync was verified by a person pressing buttons in
// GarageBand. That worked, but it means touching runtime/sync.h or the render block again has no
// safety net short of asking the owner to go press buttons. This is that net, and it needs no DAW
// and no device: it IS a host. auval cannot cover it — auval never SETS musicalContextBlock, so it
// only ever exercises the "host supplies no transport" path.
//
//   swiftc -O -o au-transport-check au-transport-check.swift -framework AVFoundation
//   ./au-transport-check            # exits 0 = PASS
//
// Requires the plug-in registered (zsh ios/mac.sh). Run from ios/; mac.sh runs it for you.
//
// WHAT IT ASSERTS, and each maps to something a user actually reported or relied on:
//   1. with the host transport MOVING, the rack plays                  (it made sound in GarageBand)
//   2. at double tempo it fires roughly twice as many notes            ("the tempo follows")
//   3. with the host STOPPED, it goes quiet                            ("when i press stop it doesnt
//                                                                        stop" — the bug this fixed)
// The fake host advances its beat position from the RENDERED SAMPLE COUNT, not a wall clock, so the
// whole run is deterministic and independent of machine speed.

import AVFoundation

// ── the component we built (must match project-mac.yml's AudioComponents entry) ──
func fourCC(_ s: String) -> OSType { s.utf8.reduce(0) { ($0 << 8) | OSType($1) } }
let desc = AudioComponentDescription(componentType: kAudioUnitType_MusicDevice,
                                     componentSubType: fourCC("tacj"),
                                     componentManufacturer: fourCC("Mpla"),
                                     componentFlags: 0, componentFlagsMask: 0)

let SR = 44100.0          // the engine is compile-time 44.1k; matching it keeps this about TRANSPORT
let fmt = AVAudioFormat(standardFormatWithSampleRate: SR, channels: 2)!

// ── instantiate the plug-in (out of process, as a real host does) ──
var au: AVAudioUnit?
let sem = DispatchSemaphore(value: 0)
AVAudioUnit.instantiate(with: desc, options: []) { u, _ in au = u; sem.signal() }
_ = sem.wait(timeout: .now() + 20)
guard let avAU = au else {
    print("✗ could not instantiate aumu/tacj/Mpla — is it registered? run: zsh ios/mac.sh")
    exit(1)
}

// ── THE FAKE HOST. These two blocks are the entire point: they are what a DAW supplies and what
//    auval never does. `hostBeat` is advanced by the render loop from samples rendered. ──
var hostTempo = 90.0
var hostBeat  = 0.0
var hostMoving = true

// NEGATIVE CONTROL: `--free` skips installing the blocks, so the plug-in gets no transport and
// free-runs on its own clock. The tempo check must then FAIL — a cart on its own clock fires notes in
// proportion to TIME, so the same 8 host beats at double tempo (half the wall time) yield about HALF
// the notes, i.e. a ratio near 0.5. Run it that way once after touching this file: a gate that cannot
// fail is decoration, and every other checker in this repo carries the same kind of known answer.
let freeRun = CommandLine.arguments.contains("--free")
if freeRun { print("  (--free: installing NO transport blocks; the tempo check SHOULD fail)") }

if !freeRun { avAU.auAudioUnit.musicalContextBlock = { tempo, tsNum, tsDen, beat, offset, downbeat in
    tempo?.pointee = hostTempo
    tsNum?.pointee = 4; tsDen?.pointee = 4
    beat?.pointee = hostBeat
    offset?.pointee = 0
    downbeat?.pointee = (hostBeat / 4.0).rounded(.down) * 4.0
    return true
} }
if !freeRun { avAU.auAudioUnit.transportStateBlock = { flags, samplePos, cycleStart, cycleEnd in
    flags?.pointee = hostMoving ? [.moving] : []
    samplePos?.pointee = hostBeat / hostTempo * 60.0 * SR
    cycleStart?.pointee = 0; cycleEnd?.pointee = 0
    return true
} }

// ── offline render rig ──
let engine = AVAudioEngine()
engine.attach(avAU)
engine.connect(avAU, to: engine.mainMixerNode, format: fmt)
try engine.enableManualRenderingMode(.offline, format: fmt, maximumFrameCount: 4096)
try engine.start()
let buf = AVAudioPCMBuffer(pcmFormat: engine.manualRenderingFormat, frameCapacity: 4096)!

// Count note ONSETS with an ADAPTIVE threshold, and count them over a span of BEATS rather than
// seconds. Both choices were forced by a first version that failed honestly:
//   · a fixed "re-arm once the signal drops below 0.01" rule counted 6 onsets in 4s where there
//     were ~24, because the reverb and delay tails never drop that low — and it got WORSE at higher
//     tempo (denser notes, fewer gaps), which reads exactly like "the tempo isn't followed" when
//     nothing of the sort is happening. A detector whose sensitivity depends on the thing under
//     test is worthless.
//   · comparing equal TIME at two tempos also compares different musical spans. Equal BEATS is the
//     invariant that actually means "it followed": the same bars should fire the same notes at any
//     tempo, so the two counts should MATCH rather than differ by the tempo ratio.
// An ENVELOPE RISE, compared against the envelope a fixed 6ms earlier. Both properties matter:
// smoothing |x| throws away waveform detail (a 303 saw's flyback is a huge per-sample delta and would
// swamp any first-difference detector — the same trap click-check.js documents), and a FIXED lookback
// makes the test rate-independent, which the previous two attempts were not.
let LOOK = Int(0.006 * SR)
var envRing = [Float](repeating: 0, count: LOOK)
var ringIdx = 0
var env: Float = 0
var sinceOnset = 99999
func render(beats: Double) -> (peak: Float, onsets: Int) {
    var peak: Float = 0, onsets = 0
    let until = hostBeat + beats
    while hostBeat < until {
        guard (try? engine.renderOffline(2048, to: buf)) == .success else { break }
        let n = Int(buf.frameLength)
        if let ch = buf.floatChannelData {
            for i in 0..<n {
                let a = abs(ch[0][i])
                peak = max(peak, a)
                env += (a - env) * 0.02                 // ~3ms envelope follower
                let then = envRing[ringIdx]             // the envelope 6ms ago
                envRing[ringIdx] = env; ringIdx = (ringIdx + 1) % LOOK
                sinceOnset += 1
                if env > then * 1.8 + 0.02 && sinceOnset > Int(0.02 * SR) { onsets += 1; sinceOnset = 0 }
            }
        }
        hostBeat += Double(n) / SR * (hostTempo / 60.0)   // the host's playhead advances with audio
    }
    return (peak, onsets)
}

// ── the three checks ──
var failures = 0
func check(_ name: String, _ ok: Bool, _ detail: String) {
    print("  \(ok ? "✓" : "✗") \(name)  — \(detail)")
    if !ok { failures += 1 }
}

_ = render(beats: 1.0)                         // let the rack settle / the first notes land
hostTempo = 90;  let atSlow = render(beats: 8.0)
hostTempo = 180; let atFast = render(beats: 8.0)

check("plays while the host transport is MOVING", atSlow.onsets >= 8,
      "\(atSlow.onsets) onsets over 8 beats at 90 BPM, peak \(String(format: "%.3f", atSlow.peak))")

// The real assertion: the SAME 8 beats fire the same notes at either tempo, i.e. the sequencer is
// on the HOST's grid. If it ran on its own clock instead, doubling the host tempo would leave the
// note count flat in TIME and so halve it per beat.
let ratio = atSlow.onsets > 0 ? Double(atFast.onsets) / Double(atSlow.onsets) : 0
check("the same 8 beats fire the same notes at 2x tempo", ratio > 0.7 && ratio < 1.4,
      "\(atFast.onsets) onsets at 180 vs \(atSlow.onsets) at 90 → ratio \(String(format: "%.2f", ratio))")

hostMoving = false
_ = render(beats: 3.0)                         // release + reverb/delay tails decay
let stopped = render(beats: 6.0)
check("STOPS when the host stops", freeRun ? stopped.onsets > 0 : (stopped.onsets == 0 && stopped.peak < 0.05),
      "\(stopped.onsets) onsets, peak \(String(format: "%.4f", stopped.peak)) after a 3-beat settle"
      + (freeRun ? "  (--free: inverted — it SHOULD keep playing)" : ""))

engine.stop()
print(failures == 0 ? "\nPASS — the plug-in follows host transport."
                    : "\n\(failures) check(s) FAILED")
exit(failures == 0 ? 0 : 1)
