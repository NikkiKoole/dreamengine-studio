// au-transport-check.swift — the headless gate for AUv3 HOST TRANSPORT (phase 2 of the AU arc),
// and for the SAMPLE RATE the host renders us at (the hazard ios-plan.md recorded but never exercised).
//
// WHY THIS EXISTS: everything about host sync was verified by a person pressing buttons in
// GarageBand. That worked, but it means touching runtime/sync.h or the render block again has no
// safety net short of asking the owner to go press buttons. This is that net, and it needs no DAW
// and no device: it IS a host. auval cannot cover it — auval never SETS musicalContextBlock, so it
// only ever exercises the "host supplies no transport" path.
//
//   swiftc -O -o au-transport-check au-transport-check.swift -framework AVFoundation -framework CoreAudioKit
//   ./au-transport-check            # the transport checks at 44.1k; exits 0 = PASS
//   ./au-transport-check --free     # NEGATIVE CONTROL: no transport blocks, the tempo check must fail
//   ./au-transport-check --rate 48000   # the same transport checks, host rendering at 48k
//   ./au-transport-check --wav /tmp/x   # also dump what it rendered → /tmp/x-<rate>.wav, to LISTEN
//   ./au-transport-check --view      # is the UI extension wired? (the picture still needs eyes)
//
// Requires the plug-in registered (zsh ios/mac.sh). Run from ios/; mac.sh runs it for you.
//
// WHAT IT ASSERTS, and each maps to something a user actually reported or relied on:
//   1. with the host transport MOVING, the rack plays                  (it made sound in GarageBand)
//   2. at double tempo it fires roughly twice as many notes            ("the tempo follows")
//   3. with the host STOPPED, it goes quiet                            ("when i press stop it doesnt
//                                                                        stop" — the bug this fixed)
// The fake host advances its beat position from the RENDERED SAMPLE COUNT, not a wall clock, so the
// whole run is deterministic and independent of machine speed. --rate runs all three at another host
// rate, which guards a property worth guarding: the sequencer is RATE-IMMUNE, because the step comes
// from sync_beats() and a host states its playhead absolutely.
//
// ── why there is no pitch check in this file, and where the pitch oracle actually lives ───────────
// There WAS one, and it was wrong in a way worth keeping written down, because the wrongness is the
// generic trap: A BROKEN ANALYSER AND THE REAL DEFECT PRINT THE SAME NUMBER.
//
// It rendered the same 8 bars at 44.1k and at 48k and compared the pitch of the low band, estimated
// from per-window zero crossings. At ~50 Hz a 2048-sample window holds only 4 or 5 crossings, so the
// reading was quantized to a coarse grid — and the grid's POSITIONS are (crossings / 2 / window
// seconds), so they move with the sample rate. 5 crossings in a 46.4 ms window reads 53.83 Hz; 5 in a
// 42.7 ms window reads 58.59 Hz. Their ratio is 48000/44100 EXACTLY. So the tool reported "+147 cents
// sharp at 48k, exactly the predicted 1200·log2(48000/44100)" — a number that looked like a
// confirmed diagnosis and was in fact the sample-rate ratio wearing a costume. ANY signal would have
// produced it. The check even carried an A/A null, which passed at 0 cents: a same-rate null cannot
// see a rate-dependent estimator, so it certified nothing.
//
// What caught it was the control that should have existed first: resample a 44.1k render to 48k with a
// KNOWN-GOOD converter (afconvert -f WAVE -d LEI16@48000) and re-measure. Same music, same pitch, and
// the estimator moved 53.83 → 46.88. That is a measurement of the rate, not of the instrument.
//
// So the pitch oracle is ios/rate-convert-check.swift instead: a 220 Hz SINE through the real
// AU/RateConvert.swift, where the answer is 220.000 Hz at every rate and there is nothing to argue
// with. The lesson generalises past this file — a statistic over a musical mix from a cart with
// per-step probability and noise drums is not a reference signal, and picking the reference signal is
// most of the work in an oracle.

import AVFoundation
import AppKit
// CoreAudioKit, not AudioToolbox: requestViewControllerWithCompletionHandler is declared there, as a
// CATEGORY on AUAudioUnit (CoreAudioKit/AUViewController.h). Without this import the method simply does
// not exist and the compiler says AUAudioUnit "has no member" — found by grepping the SDK headers
// rather than guessing an import per build, the same technique that settled the CoreAudio one.
import CoreAudioKit

// ── the component we built (must match project-mac.yml's AudioComponents entry) ──
func fourCC(_ s: String) -> OSType { s.utf8.reduce(0) { ($0 << 8) | OSType($1) } }
let desc = AudioComponentDescription(componentType: kAudioUnitType_MusicDevice,
                                     componentSubType: fourCC("tacj"),
                                     componentManufacturer: fourCC("Mpla"),
                                     componentFlags: 0, componentFlagsMask: 0)

let ENGINE_SR = 44100.0   // what the engine is COMPILED for; the baseline every rate is compared to

// ── args ──
let argv = CommandLine.arguments
func flagValue(_ flag: String, _ fallback: Double) -> Double {
    guard let i = argv.firstIndex(of: flag), i + 1 < argv.count, let v = Double(argv[i + 1]) else { return fallback }
    return v
}
let freeRun  = argv.contains("--free")
let hostRate = flagValue("--rate", ENGINE_SR)
// --wav <prefix> dumps what was rendered → <prefix>-<rate>.wav. Not a check, a listening aid: the
// fastest way to answer "does 48k sound in tune" is still to play the file.
let wavPrefix: String? = {
    guard let i = argv.firstIndex(of: "--wav"), i + 1 < argv.count else { return nil }
    return argv[i + 1]
}()

// ── instantiate the plug-in (out of process, as a real host does) ──
// PUMP THE MAIN RUN LOOP instead of blocking on a semaphore. This used to be a semaphore wait, and it
// worked right up until the extension gained a VIEW: a `com.apple.AudioUnit-UI` extension does part of
// its loading on the main queue, so blocking the main thread here deadlocks and instantiation "fails"
// after the timeout with a message blaming registration — while auval, which has a real run loop,
// loads the same plug-in happily. That misdirection cost a confused minute; worth the comment.
var au: AVAudioUnit?
var instantiated = false
AVAudioUnit.instantiate(with: desc, options: []) { u, _ in au = u; instantiated = true }
let deadline = Date().addingTimeInterval(20)
while !instantiated, Date() < deadline {
    RunLoop.main.run(mode: .default, before: Date().addingTimeInterval(0.05))
}
guard let avAU = au else {
    print("✗ could not instantiate aumu/tacj/Mpla — is it registered? run: zsh ios/mac.sh")
    exit(1)
}

// ── THE FAKE HOST. These two blocks are the entire point: they are what a DAW supplies and what
//    auval never does. `hostBeat` is advanced by the render loop from samples rendered. ──
var hostTempo = 90.0
var hostBeat  = 0.0
var hostMoving = true
var hostSR = ENGINE_SR        // the rate the CURRENT rig renders at; the blocks report positions in it

// NEGATIVE CONTROL: `--free` skips installing the blocks, so the plug-in gets no transport and
// free-runs on its own clock. The tempo check must then FAIL — a cart on its own clock fires notes in
// proportion to TIME, so the same 8 host beats at double tempo (half the wall time) yield about HALF
// the notes, i.e. a ratio near 0.5. Run it that way once after touching this file: a gate that cannot
// fail is decoration, and every other checker in this repo carries the same kind of known answer.
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
    samplePos?.pointee = hostBeat / hostTempo * 60.0 * hostSR
    cycleStart?.pointee = 0; cycleEnd?.pointee = 0
    return true
} }

// ── one offline render rig at one sample rate ────────────────────────────────────────────────────
// A rig owns an AVAudioEngine and takes the AU INSTANCE, so a run can tear one rig down and build
// another at a different rate around the same plug-in, the way a host switching device rate does.
final class Rig {
    let sr: Double
    private let engine = AVAudioEngine()
    private let au: AVAudioUnit
    private let buf: AVAudioPCMBuffer
    // onset detector state (per rig: a fresh rate means a fresh lookback ring)
    private let look: Int
    private var envRing: [Float]
    private var ringIdx = 0
    private var env: Float = 0
    private var sinceOnset = 99999

    init(au: AVAudioUnit, sr: Double) throws {
        self.sr = sr; self.au = au
        let fmt = AVAudioFormat(standardFormatWithSampleRate: sr, channels: 2)!
        look = Int(0.006 * sr)
        envRing = [Float](repeating: 0, count: look)
        engine.attach(au)
        engine.connect(au, to: engine.mainMixerNode, format: fmt)
        try engine.enableManualRenderingMode(.offline, format: fmt, maximumFrameCount: 4096)
        try engine.start()
        buf = AVAudioPCMBuffer(pcmFormat: engine.manualRenderingFormat, frameCapacity: 4096)!
        hostSR = sr
    }

    // What the PLUG-IN's own output bus ended up at, which is the fact --rate exists to establish:
    // if AVAudioEngine had quietly kept the AU at 44.1k and converted downstream, a run at "48k"
    // would be exercising AVFoundation's converter and not ours. Assert, never assume.
    var auRate: Double { au.auAudioUnit.outputBusses[0].format.sampleRate }

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
    // Set to dump what this rig rendered, at its OWN rate, for any tool (or ear) outside this file.
    // This is how the bogus pitch estimator was caught: dump, convert with afconvert, re-measure.
    var wav: AVAudioFile?

    func render(beats: Double) -> (peak: Float, onsets: Int) {
        var peak: Float = 0, onsets = 0
        let until = hostBeat + beats
        while hostBeat < until {
            guard (try? engine.renderOffline(2048, to: buf)) == .success else { break }
            let n = Int(buf.frameLength)
            if let w = wav { try? w.write(from: buf) }
            if let ch = buf.floatChannelData {
                for i in 0..<n {
                    let a = abs(ch[0][i])
                    peak = max(peak, a)
                    env += (a - env) * 0.02                 // ~3ms envelope follower
                    let then = envRing[ringIdx]             // the envelope 6ms ago
                    envRing[ringIdx] = env; ringIdx = (ringIdx + 1) % look
                    sinceOnset += 1
                    if env > then * 1.8 + 0.02 && sinceOnset > Int(0.02 * sr) { onsets += 1; sinceOnset = 0 }
                }
            }
            hostBeat += Double(n) / sr * (hostTempo / 60.0)   // the host's playhead advances with audio
        }
        return (peak, onsets)
    }

    func teardown() { engine.stop(); engine.detach(au) }   // detach: an AU node lives in one engine
}

// ── check plumbing ──
var failures = 0
func check(_ name: String, _ ok: Bool, _ detail: String) {
    print("  \(ok ? "✓" : "✗") \(name)  — \(detail)")
    if !ok { failures += 1 }
}
func cents(_ ratio: Double) -> Double { 1200.0 * log2(ratio) }

// ═══ --view: can a host actually GET our plug-in view? ═══════════════════════════════════════════
// The narrow question this can answer headlessly: is the UI extension wired up — `com.apple.AudioUnit-UI`
// as the extension point, an AUViewController that is also the AUAudioUnitFactory as the principal
// class, and the view actually loading when asked. Get any of that wrong and the plug-in still passes
// auval and still plays; the host just quietly shows its own generic sliders forever, which is the
// failure this mode exists to catch early.
// What it CANNOT answer is whether the picture looks right — that needs eyes in a DAW. The pixel path
// itself (de_copy_frame → the blit) is gated separately by tools/present-race-check.
if argv.contains("--view") {
    print("▸ plug-in view: can a host obtain it?")
    var vc: NSViewController?
    var answered = false
    avAU.auAudioUnit.requestViewController { controller in vc = controller; answered = true }
    let vdl = Date().addingTimeInterval(15)
    while !answered, Date() < vdl { RunLoop.main.run(mode: .default, before: Date().addingTimeInterval(0.05)) }
    check("the host is handed a view controller", answered && vc != nil,
          answered ? "got \(vc.map { String(describing: type(of: $0)) } ?? "nil — the host would fall back to generic sliders")"
                   : "requestViewController never answered within 15s")
    if let v = vc {
        // Reading `view` forces the controller to load, which is where our CanvasView is built and its
        // display link starts. If that fails or the view is empty, a DAW gets a blank panel.
        let loaded = v.view.frame
        let size = v.preferredContentSize
        check("its view loads and asks for a size", size.width > 0 && size.height > 0,
              "view frame \(Int(loaded.width))x\(Int(loaded.height)), preferredContentSize \(Int(size.width))x\(Int(size.height))")
    }
    print(failures == 0 ? "\nPASS — the UI extension is wired; LOOK at it in a DAW to judge the picture."
                        : "\n\(failures) check(s) FAILED")
    exit(failures == 0 ? 0 : 1)
}

// ═══ the three transport checks ══════════════════════════════════════════════════════════════════
if hostRate != ENGINE_SR { print("▸ host rendering at \(Int(hostRate)) Hz (engine is compiled for \(Int(ENGINE_SR)))") }
let rig = try! Rig(au: avAU, sr: hostRate)
if let p = wavPrefix {
    let url = URL(fileURLWithPath: "\(p)-\(Int(hostRate)).wav")
    try? FileManager.default.removeItem(at: url)
    rig.wav = try? AVAudioFile(forWriting: url,
                               settings: [AVFormatIDKey: kAudioFormatLinearPCM,
                                          AVSampleRateKey: hostRate,
                                          AVNumberOfChannelsKey: 2,
                                          AVLinearPCMBitDepthKey: 16,
                                          AVLinearPCMIsFloatKey: false])
    print("▸ dumping → \(url.path)")
}
if hostRate != ENGINE_SR {
    check("the host really moved the plug-in's own bus to \(Int(hostRate)) Hz",
          abs(rig.auRate - hostRate) < 1.0, "AU output bus reports \(Int(rig.auRate)) Hz")
}

_ = rig.render(beats: 1.0)                     // let the rack settle / the first notes land
hostTempo = 90;  let atSlow = rig.render(beats: 8.0)
hostTempo = 180; let atFast = rig.render(beats: 8.0)

check("plays while the host transport is MOVING", atSlow.onsets >= 8,
      "\(atSlow.onsets) onsets over 8 beats at 90 BPM, peak \(String(format: "%.3f", atSlow.peak))")

// The real assertion: the SAME 8 beats fire the same notes at either tempo, i.e. the sequencer is
// on the HOST's grid. If it ran on its own clock instead, doubling the host tempo would leave the
// note count flat in TIME and so halve it per beat.
let ratio = atSlow.onsets > 0 ? Double(atFast.onsets) / Double(atSlow.onsets) : 0
check("the same 8 beats fire the same notes at 2x tempo", ratio > 0.7 && ratio < 1.4,
      "\(atFast.onsets) onsets at 180 vs \(atSlow.onsets) at 90 → ratio \(String(format: "%.2f", ratio))")

hostMoving = false
_ = rig.render(beats: 3.0)                     // release + reverb/delay tails decay
let stopped = rig.render(beats: 6.0)
check("STOPS when the host stops", freeRun ? stopped.onsets > 0 : (stopped.onsets == 0 && stopped.peak < 0.05),
      "\(stopped.onsets) onsets, peak \(String(format: "%.4f", stopped.peak)) after a 3-beat settle"
      + (freeRun ? "  (--free: inverted — it SHOULD keep playing)" : ""))

rig.teardown()
print(failures == 0 ? "\nPASS — the plug-in follows host transport."
                    : "\n\(failures) check(s) FAILED")
exit(failures == 0 ? 0 : 1)
