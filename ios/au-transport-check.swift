// au-transport-check.swift — the headless gate for AUv3 HOST TRANSPORT (phase 2 of the AU arc),
// and for the SAMPLE RATE the host renders us at (the hazard ios-plan.md recorded but never exercised).
//
// WHY THIS EXISTS: everything about host sync was verified by a person pressing buttons in
// GarageBand. That worked, but it means touching runtime/sync.h or the render block again has no
// safety net short of asking the owner to go press buttons. This is that net, and it needs no DAW
// and no device: it IS a host. auval cannot cover it — auval never SETS musicalContextBlock, so it
// only ever exercises the "host supplies no transport" path.
//
//   swiftc -O -o au-transport-check au-transport-check.swift -framework AVFoundation
//   ./au-transport-check            # the transport checks at 44.1k; exits 0 = PASS
//   ./au-transport-check --free     # NEGATIVE CONTROL: no transport blocks, the tempo check must fail
//   ./au-transport-check --rate 48000   # the same transport checks, host rendering at 48k
//   ./au-transport-check --pitch    # the RATE-INVARIANCE A/B (44.1k vs 48k). See "the pitch check".
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
//
// ── the pitch check (--pitch) ────────────────────────────────────────────────────────────────────
// A SEPARATE question from transport, and the one that needed a new tool: the engine is compile-time
// 44.1 kHz (SOUND_SAMPLE_RATE sizes every delay line and envelope; the render block ticks one frame
// per 735 samples), but an AUv3 has no converter in front of it, so the host calls us at the HOST's
// rate. What that breaks is NOT the sequencer: acidcandy derives its step from sync_beats() and a
// host states its playhead absolutely, so the notes stay on the host's grid at any rate. What breaks
// is PITCH, plus every envelope and delay time, because an oscillator adding a fixed phase increment
// per sample runs (hostRate / 44100) too fast. At 48k that is +147 cents, a semitone and a half sharp.
//
// So --pitch renders the SAME BARS twice, once at 44.1k and once at the alternate rate, and compares
// the pitch of the low band. Rendering the same bars is possible only because the host playhead is
// ABSOLUTE: rewind hostBeat to where it started and the cart replays those steps, no matter what the
// sample rate did in between. Two known answers, so the check cannot be vacuous:
//   · rate-invariant  → the two pitches match       (what a resampler in the render block would buy)
//   · rate-blind      → they differ by exactly 1200·log2(altRate/44100)  (today's engine)
// It is OPT-IN and mac.sh does not run it, because today it correctly reports the known defect and
// so would fail every build. Wire it into mac.sh the day the resampler lands.

import AVFoundation

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
let freeRun   = argv.contains("--free")
let pitchMode = argv.contains("--pitch")
let hostRate  = flagValue("--rate", pitchMode ? 48000.0 : ENGINE_SR)

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

// ── the low band's pitch, by Schmitt-triggered zero crossings ────────────────────────────────────
// Why not autocorrelation, and why the low band: the rack is a mix of two 303s and two drum machines,
// and NOISE is the trap. A hat is white-ish noise generated one sample at a time, so its crossing
// count scales with the SAMPLE RATE whether or not the engine is rate-blind — measure the full mix
// and both known answers produce the same number, which is a tool that cannot tell them apart. Three
// cascaded one-poles at 200 Hz (-18 dB/oct, so a hat at 8k is ~85 dB down) leave the kick and the two
// basslines, all of them OSCILLATORS, which is exactly the population whose pitch the rate moves.
// The Schmitt trigger (cross +thr, then -thr) is what keeps a decaying tail from ringing up a count.
//
// Measured per WINDOW and then MEDIANED over the loud windows, not averaged over the whole span. A
// first version divided total crossings by total seconds, which silently measures "how much of this
// span was loud" as much as it measures pitch: the same bars with shorter reverb tails score lower
// with every note identical. It read +676 cents where the largest defect possible is +147. Per-window
// medians drop the silence entirely and are immune to a stray window.
final class LowPitch {
    private var p1: Float = 0, p2: Float = 0, p3: Float = 0
    private var sign = 1
    private let a: Float
    private let thr: Float = 0.02
    private let sr: Double
    private let win = 2048                      // ~46ms at 44.1k: a fraction of a 16th note at 90 BPM
    private var winCross = 0, winN = 0
    private var winPeak: Float = 0
    private(set) var rates: [Double] = []       // crossings/sec, one entry per LOUD window
    init(sr: Double) { self.sr = sr; a = Float(1.0 - exp(-2.0 * Double.pi * 200.0 / sr)) }
    func push(_ x: Float) {
        p1 += (x - p1) * a; p2 += (p1 - p2) * a; p3 += (p2 - p3) * a
        winPeak = max(winPeak, abs(p3))
        if sign < 0, p3 > thr { sign = 1; winCross += 1 }
        else if sign > 0, p3 < -thr { sign = -1; winCross += 1 }
        winN += 1
        if winN >= win {
            if winPeak > 0.05 { rates.append(Double(winCross) / 2.0 / (Double(winN) / sr)) }
            winCross = 0; winN = 0; winPeak = 0
        }
    }
    var hz: Double {
        guard !rates.isEmpty else { return 0 }
        let s = rates.sorted()
        return s.count % 2 == 1 ? s[s.count / 2] : (s[s.count / 2 - 1] + s[s.count / 2]) / 2
    }
    var loudWindows: Int { rates.count }
}

// ── one offline render rig at one sample rate ────────────────────────────────────────────────────
// A rig owns an AVAudioEngine; the AU INSTANCE is shared across rigs so that a --pitch A/B keeps one
// plug-in (and so one cart state) across the rate change, the way a host switching device rate does.
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

    // What the PLUG-IN's own output bus ended up at. Load-bearing for --pitch: if AVAudioEngine had
    // quietly kept the AU at 44.1k and converted downstream, the A/B would be measuring the
    // converter and would report "rate-invariant" for a rate-blind engine. Assert, never assume.
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
    func render(beats: Double, pitch: LowPitch? = nil) -> (peak: Float, onsets: Int) {
        var peak: Float = 0, onsets = 0
        let until = hostBeat + beats
        while hostBeat < until {
            guard (try? engine.renderOffline(2048, to: buf)) == .success else { break }
            let n = Int(buf.frameLength)
            if let ch = buf.floatChannelData {
                for i in 0..<n {
                    let a = abs(ch[0][i])
                    peak = max(peak, a)
                    pitch?.push(ch[0][i])
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

// ═══ MODE A: the pitch / sample-rate A/B ═════════════════════════════════════════════════════════
if pitchMode {
    if freeRun { print("  (--free with --pitch is meaningless: the A/B rewinds the HOST playhead)"); exit(2) }
    print("▸ rate invariance: the same 8 bars at \(Int(ENGINE_SR)) Hz vs \(Int(hostRate)) Hz")

    func measure(_ sr: Double) -> (hz: Double, peak: Float, auRate: Double, windows: Int) {
        hostBeat = 0; hostTempo = 90; hostMoving = true
        let rig = try! Rig(au: avAU, sr: sr)
        _ = rig.render(beats: 2.0)                      // settle: let the first notes and tails start
        let p = LowPitch(sr: sr)
        let r = rig.render(beats: 8.0, pitch: p)
        let out = (p.hz, r.peak, rig.auRate, p.loudWindows)
        rig.teardown()
        return out
    }

    // THREE measurements, not two. The third is an A/A NULL: the base rate again, at the END, so it
    // carries the same "this plug-in has already played and been rewound" history the alt-rate one
    // does. Without it a difference cannot be attributed — the A/B and "the second pass simply plays
    // something else" produce the same number, and the first version of this check fell into exactly
    // that hole (+676 cents, where the largest possible defect is +147). The null is the noise floor,
    // and a verdict is refused if the noise floor is not well under the effect being judged.
    let a  = measure(ENGINE_SR)
    let b  = measure(hostRate)    // REWIND is inside measure(): an absolute playhead replays the bars
    let a2 = measure(ENGINE_SR)

    check("the host really moved the plug-in's own bus to \(Int(hostRate)) Hz",
          abs(b.auRate - hostRate) < 1.0,
          "AU output bus reports \(Int(b.auRate)) Hz (at \(Int(ENGINE_SR)) it reported \(Int(a.auRate)))")

    let ratio = a.hz > 0 ? b.hz / a.hz : 0
    let blind = hostRate / ENGINE_SR              // the known answer for a rate-blind engine
    let off   = cents(ratio)
    let null  = a2.hz > 0 && a.hz > 0 ? abs(cents(a2.hz / a.hz)) : 9999
    print(String(format: "    low-band pitch: %.2f Hz at %d  →  %.2f Hz at %d   (%+.0f cents)",
                 a.hz, Int(ENGINE_SR), b.hz, Int(hostRate), off))
    print(String(format: "    A/A null: %.2f Hz on a repeat at %d  (%.0f cents of run-to-run noise)",
                 a2.hz, Int(ENGINE_SR), null))
    print(String(format: "    peaks %.3f / %.3f / %.3f  ·  loud windows %d / %d / %d  ·  rate-blind would be %+.0f cents (ratio %.4f)",
                 a.peak, b.peak, a2.peak, a.windows, b.windows, a2.windows, cents(blind), blind))

    // The null has to be small relative to the effect, or nothing below it means anything.
    let trustworthy = null < abs(cents(blind)) / 3.0
    check("the measurement repeats (A/A null well under the effect)", trustworthy,
          trustworthy ? "\(String(format: "%.0f", null)) cents of noise vs a \(String(format: "%.0f", abs(cents(blind)))) cent effect"
                      : "\(String(format: "%.0f", null)) cents of noise — the same bars do NOT replay identically, so no verdict is possible")

    if trustworthy {
        let invariant = abs(off) < 20.0
        let isBlind   = abs(cents(ratio / blind)) < 20.0
        check("pitch is invariant to the host's sample rate", invariant,
              invariant ? "within 20 cents"
              : isBlind ? "RATE-BLIND: sharp by the sample-rate ratio itself, exactly as ios-plan.md predicted"
                        : "off by \(String(format: "%.0f", off)) cents, which matches NEITHER known answer — look at the numbers above")
    }

    print(failures == 0 ? "\nPASS — the plug-in sounds the same at any host rate."
                        : "\n\(failures) check(s) FAILED")
    exit(failures == 0 ? 0 : 1)
}

// ═══ MODE B: the three transport checks (the default gate) ═══════════════════════════════════════
if hostRate != ENGINE_SR { print("▸ host rendering at \(Int(hostRate)) Hz (engine is compiled for \(Int(ENGINE_SR)))") }
let rig = try! Rig(au: avAU, sr: hostRate)
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
