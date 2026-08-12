// rate-convert-check.swift — known-answer gate for the AUv3's sample-rate converter.
//
//   xcrun swiftc -O -o rate-convert-check rate-convert-check.swift AU/RateConvert.swift
//   ./rate-convert-check          # exits 0 = PASS
//
// It compiles the REAL AU/RateConvert.swift, not a copy, so it cannot drift from what ships.
//
// WHY THIS EXISTS, and it is worth reading before adding to it. The first attempt at gating the rate
// fix measured PITCH OUT OF THE RUNNING PLUG-IN, comparing the same 8 bars rendered at 44.1k and 48k
// (ios/au-transport-check.swift --pitch). That cannot work, and the way it failed is the lesson: the
// hosted cart has per-step drum PROBABILITY and noise-based drum voices, so two passes over "the same"
// bars are not the same audio. A same-rate A/A control correlated at 0.045 and its spectral centroid
// differed frame by frame, while a known-good conversion of one pass tracked its own source almost
// exactly. So the instrument was never a usable reference signal, and the -240 cents that attempt
// reported was measuring musical difference, not pitch.
//
// A sine, on the other hand, has an exact answer: put 220 Hz in, 220 Hz must come out, at every rate
// a host can ask for. Zero crossings of a pure tone are an unambiguous frequency reading, so the
// oracle here needs no spectral analysis and no tolerance games.
//
// WHAT IT ASSERTS:
//   1. at the engine's own rate the converter is an identity (a 1-frame lead, nothing else)
//   2. 220 Hz stays 220 Hz at 48000 / 96000 / 192000 / 22050 / 11025
//   3. amplitude survives: no gain drift, no interpolation collapse
//   4. downsampling REJECTS content above the host's Nyquist instead of folding it back down
//      (a 15 kHz tone into an 11025 Hz host must come out quiet, not as a 3.9 kHz alias)
//   5. the guard: a nonsense host rate falls back to passthrough rather than hanging the audio
//      thread in `while needsFrame` — which would take the host down with it

import Foundation

let ENGINE = 44100.0
var failures = 0

func check(_ name: String, _ ok: Bool, _ detail: String) {
    print("  \(ok ? "✓" : "✗") \(name)  — \(detail)")
    if !ok { failures += 1 }
}

// Run a sine of `hz` through the converter to `hostRate` and report what came out.
// Zero crossings are counted on the RISING edge through zero, which for a pure tone is exact.
func convertSine(hz: Double, hostRate: Double, seconds: Double = 1.0)
        -> (freq: Double, peak: Float, rms: Double) {
    var rc = RateConvert(engineRate: ENGINE, hostRate: hostRate)
    let outCount = Int(hostRate * seconds)
    var phase = 0.0
    let inc = 2.0 * Double.pi * hz / ENGINE
    var crossings = 0
    var prev: Float = 0
    var peak: Float = 0
    var sumsq = 0.0
    var first = -1.0, last = -1.0        // times of the first and last rising crossing
    for j in 0..<outCount {
        while rc.needsFrame {
            let s = Float(sin(phase)); phase += inc
            rc.push(s, s)
        }
        let (l, _) = rc.sample()
        rc.advance()
        peak = max(peak, abs(l))
        sumsq += Double(l) * Double(l)
        // rising zero crossing, with the exact sub-sample position by linear interpolation, so a
        // 1-second window gives a frequency good to far better than a cent
        if prev <= 0, l > 0, j > 0 {
            let frac = Double(-prev) / Double(l - prev)
            let t = (Double(j - 1) + frac) / hostRate
            if first < 0 { first = t } else { last = t; crossings += 1 }
        }
        prev = l
    }
    let freq = (crossings > 0 && last > first) ? Double(crossings) / (last - first) : 0
    return (freq, peak, sqrt(sumsq / Double(outCount)))
}

// @main, not top-level code: this file is compiled TOGETHER with AU/RateConvert.swift (that is the
// whole point — it gates the shipping struct, not a copy), and Swift only allows top-level statements
// in a lone file or in one literally named main.swift.
@main struct RateConvertCheck {
static func main() {

print("▸ RateConvert: a 220 Hz sine in, at every rate a host can ask for")

// ── 1. identity at the engine's own rate ─────────────────────────────────────────────────────────
// Not just "the pitch is right": every sample must be the input, because TinyjamAU's fast path
// depends on ratio == 1.0 meaning literally nothing happens to the audio.
do {
    var rc = RateConvert(engineRate: ENGINE, hostRate: ENGINE)
    var input = [Float](repeating: 0, count: 512)
    for i in 0..<512 { input[i] = Float(sin(2.0 * Double.pi * 220.0 * Double(i) / ENGINE)) }
    var out = [Float](repeating: 0, count: 256)
    var read = 0
    for j in 0..<256 {
        while rc.needsFrame { rc.push(input[read], input[read]); read += 1 }
        out[j] = rc.sample().0
        rc.advance()
    }
    // priming consumes 4 frames before the first emit, so out[j] is input[j+1]
    var worst: Float = 0
    for j in 0..<250 { worst = max(worst, abs(out[j] - input[j + 1])) }
    check("at \(Int(ENGINE)) Hz it is an exact identity", rc.passthrough && worst == 0,
          "passthrough=\(rc.passthrough), largest sample difference \(worst)")
}

// ── 2/3. pitch and level at every rate a host might pick ─────────────────────────────────────────
for host in [48000.0, 96000.0, 192000.0, 22050.0, 11025.0] {
    let r = convertSine(hz: 220, hostRate: host)
    let off = r.freq > 0 ? 1200.0 * log2(r.freq / 220.0) : -9999
    // A cent is inaudible; a rate-blind converter would be 147 cents out at 48k, so 5 cents is a
    // tight bar that still tolerates the crossing estimator's own resolution.
    check("220 Hz → \(Int(host)) Hz stays in tune",
          abs(off) < 5.0,
          String(format: "%.3f Hz  (%+.2f cents)  peak %.3f  rms %.3f", r.freq, off, r.peak, r.rms))
    // sin() rms is 0.7071; interpolation of a 220 Hz tone at these ratios should not move it.
    check("220 Hz → \(Int(host)) Hz keeps its level",
          abs(r.rms - 0.7071) < 0.02 && r.peak > 0.95 && r.peak < 1.05,
          String(format: "rms %.4f vs 0.7071, peak %.4f", r.rms, r.peak))
}

// ── 4. downsampling must reject, not fold ────────────────────────────────────────────────────────
// 15 kHz into an 11025 Hz host: Nyquist is 5512 Hz, so without the anti-alias cascade this comes back
// as a loud 3.9 kHz tone that is not in the music. With it, it should be most of the way to gone.
do {
    let alias = convertSine(hz: 15000, hostRate: 11025)
    check("15 kHz into an 11025 Hz host is REJECTED, not folded down",
          alias.rms < 0.05,
          String(format: "rms %.4f (a fold-down would arrive near 0.7)  peak %.3f", alias.rms, alias.peak))
    // and the sanity twin: the same low host rate must still pass a tone it CAN represent
    let ok = convertSine(hz: 220, hostRate: 11025)
    check("220 Hz into an 11025 Hz host still comes through",
          ok.rms > 0.6, String(format: "rms %.4f", ok.rms))
}

// ── 5. the hang guard ────────────────────────────────────────────────────────────────────────────
// ratio = inf or NaN makes `while needsFrame` in the caller loop forever. On the audio thread that is
// not a wrong number, it is a dead host — so a nonsense rate must land on passthrough.
for bad in [0.0, -48000.0, Double.nan, Double.infinity, 1.0, 999999.0] {
    var rc = RateConvert(engineRate: ENGINE, hostRate: bad)
    check("a host rate of \(bad) falls back to passthrough", rc.passthrough && rc.ratio == 1.0,
          "ratio \(rc.ratio)")
    // and prove the loop actually terminates: prime it and emit one frame
    var pushes = 0
    while rc.needsFrame, pushes < 100 { rc.push(0, 0); pushes += 1 }
    _ = rc.sample(); rc.advance()
    check("  and its pull loop terminates", pushes < 100, "\(pushes) pushes to prime")
}

print(failures == 0 ? "\nPASS — the converter is in tune at every rate."
                    : "\n\(failures) check(s) FAILED")
exit(failures == 0 ? 0 : 1)
}   // static func main
}   // @main struct
