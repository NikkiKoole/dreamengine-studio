// RateConvert.swift — the AUv3's sample-rate converter, and the ONLY place that conversion lives.
//
// WHY IT IS ITS OWN FILE. The engine is compile-time 44.1 kHz (SOUND_SAMPLE_RATE sizes every delay
// line and envelope; demath.h's bit-determinism depends on a fixed rate), but an AUv3 has no
// converter in front of it: the host calls us at the HOST's rate, and a host follows its audio
// interface, so 48k is common. Measured before this existed: the whole rack played +147 cents sharp
// at 48k = exactly 1200·log2(48000/44100). The fix belongs here rather than in the engine.
//
// It is EXTRACTED from TinyjamAU.swift so that ios/rate-convert-check.swift can gate the REAL code
// against known answers. That mattered: the first attempt measured pitch out of the running plug-in
// instead, and could not, because the hosted cart has per-step probability and noise-based drums and
// so does not reproduce the same audio twice — a nondeterministic instrument cannot be the reference
// signal for a converter. A sine can. Feed this struct a sine and the answer is a number.
//
// SHAPE: pull-based and closure-free, so the audio thread does no allocation and no ARC traffic. The
// caller drives it, which keeps the engine's frame tick (one de_frame per 735 engine samples) where
// it belongs, in the render block:
//
//     for j in 0..<n {
//         while rc.needsFrame { rc.push(engineL, engineR) }    // pull one ENGINE frame
//         let (l, r) = rc.sample()                             // interpolate at the current position
//         rc.advance()
//     }
import Foundation      // exp() in configure(); nothing else here needs a framework

struct RateConvert {
    // ENGINE frames consumed per OUTPUT frame: 44100/hostRate. Exactly 1.0 means the host is at our
    // rate, and TinyjamAU skips this struct entirely in that case (see "the fast path" there).
    private(set) var ratio: Double = 1
    // Position inside the interpolation window. Starts at 4.0 so the first four pushes PRIME the
    // 4-frame history, which is why there is no separate `primed` flag to get wrong.
    private var frac: Double = 4
    // 4-frame interpolation history, oldest to newest. sample() interpolates between l1 and l2.
    private var l0: Float = 0, l1: Float = 0, l2: Float = 0, l3: Float = 0
    private var r0: Float = 0, r1: Float = 0, r2: Float = 0, r3: Float = 0
    // Anti-alias cascade, run at the ENGINE rate on the way in. Only engaged when the host is BELOW
    // our rate, where plain interpolation would fold everything above the host's Nyquist back down.
    private var aL0: Float = 0, aL1: Float = 0, aL2: Float = 0, aL3: Float = 0
    private var aR0: Float = 0, aR1: Float = 0, aR2: Float = 0, aR3: Float = 0
    // < 1 only when the host is BELOW the engine rate; push() skips the cascade entirely otherwise.
    // It is tempting to leave it at 1.0 and let y += (x - y)*1 stand in as a branch-free bypass, and
    // that was the first version — but it is not one: two roundings per pole leave ~2e-9 of error,
    // which rate-convert-check caught by demanding a BIT-exact identity at 44.1k. A predictable
    // branch is cheaper than four multiply-adds anyway.
    private var aCoef: Float = 1

    init() {}
    init(engineRate: Double, hostRate: Double) { configure(engineRate: engineRate, hostRate: hostRate) }

    var passthrough: Bool { ratio == 1.0 }

    mutating func configure(engineRate: Double, hostRate: Double) {
        self = RateConvert()
        // A nonsense rate would be fatal rather than wrong: ratio = inf or NaN makes the caller's
        // `while needsFrame` loop never terminate, which hangs the audio thread and takes the host
        // down with it. Anything outside what real hardware does falls back to passthrough.
        guard hostRate.isFinite, engineRate.isFinite,
              hostRate >= 8000, hostRate <= 384000, engineRate > 0 else { return }
        ratio = engineRate / hostRate
        if hostRate < engineRate {
            // Four one-poles at 0.45·host. Gentle per pole, but -24 dB/oct in cascade, which is what
            // makes auval's 11025 Hz render sound like a lowpassed rack rather than a bag of aliases.
            let fc = 0.45 * hostRate
            aCoef = Float(1.0 - exp(-2.0 * Double.pi * fc / engineRate))
        }
    }

    /// True while the interpolator needs another ENGINE frame before it can emit an output frame.
    var needsFrame: Bool { frac >= 1.0 }

    /// Hand it the next ENGINE-rate frame. Only call while `needsFrame`.
    mutating func push(_ inL: Float, _ inR: Float) {
        var xl = inL, xr = inR
        if aCoef < 1 {                      // downsampling only: anti-alias before we decimate
            aL0 += (xl - aL0) * aCoef; aL1 += (aL0 - aL1) * aCoef
            aL2 += (aL1 - aL2) * aCoef; aL3 += (aL2 - aL3) * aCoef
            aR0 += (xr - aR0) * aCoef; aR1 += (aR0 - aR1) * aCoef
            aR2 += (aR1 - aR2) * aCoef; aR3 += (aR2 - aR3) * aCoef
            xl = aL3; xr = aR3
        }
        l0 = l1; l1 = l2; l2 = l3; l3 = xl
        r0 = r1; r1 = r2; r2 = r3; r3 = xr
        frac -= 1.0
    }

    /// The output frame at the current position: 4-point Catmull-Rom between l1 and l2. Chosen over
    /// linear because linear's sinc² rolloff audibly dulls a 303 at these ratios, and over a
    /// windowed-sinc polyphase because this runs per sample on the audio thread inside a plug-in.
    func sample() -> (Float, Float) {
        let t = Float(frac)
        let cl1 = 0.5 * (l2 - l0)
        let cl2 = l0 - 2.5*l1 + 2.0*l2 - 0.5*l3
        let cl3 = 0.5*(l3 - l0) + 1.5*(l1 - l2)
        let cr1 = 0.5 * (r2 - r0)
        let cr2 = r0 - 2.5*r1 + 2.0*r2 - 0.5*r3
        let cr3 = 0.5*(r3 - r0) + 1.5*(r1 - r2)
        return (((cl3*t + cl2)*t + cl1)*t + l1,
                ((cr3*t + cr2)*t + cr1)*t + r1)
    }

    /// Step forward by one OUTPUT frame.
    mutating func advance() { frac += ratio }
}
