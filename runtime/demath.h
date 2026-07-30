// demath.h — deterministic float math: the same bits on arm64, x86-64 and wasm.
//
// WHY THIS EXISTS
// IEEE 754 mandates correctly-rounded results for + - * / and sqrt, so those agree everywhere.
// It says NOTHING about sin/cos/tan/exp/log/pow/tanh. Correctly rounding a transcendental is
// expensive (the table-maker's dilemma), so no libm does it: Apple's libm, glibc, musl and
// emscripten's all differ by ~1 ULP, and glibc even dispatches to different SIMD kernels
// depending on the CPU it detects at load time. Same source, same input, different bits.
//
// That already cost this repo twice:
//   • tools/det-probes/README.md — per-pixel rotation broke on an 8px glyph (patched by
//     quantizing the rotation matrix to 1/4096).
//   • tools/web-audio-check.js — the BOWED engine's chaotic stick-slip friction amplifies a
//     1-ULP libm difference into a different micro-waveform, which is why that gate carries a
//     two-tier verdict instead of demanding sample parity.
//
// WHAT TO USE, AND WHAT NOT TO BOTHER WITH
// Roughly half the math surface needs no help. These are already bit-exact by spec — keep
// calling libm, do NOT route them through here:
//     sqrtf fabsf floorf ceilf truncf roundf fmodf fminf fmaxf copysignf ldexpf
// The ones worth replacing are the transcendentals: sinf cosf tanf atan2f expf logf powf tanhf.
// This header covers the four that dominate runtime/sound.h. See docs/design/determinism.md.
//
// HOW IT STAYS DETERMINISTIC
//   1. Only + - * / and comparisons. Every one is correctly rounded, so every target agrees.
//   2. FP contraction is pinned OFF per function. A Horner step is exactly the a*x+b shape a
//      compiler fuses into an FMA, and an FMA keeps the product at full width instead of
//      rounding it, so a contracted build and a non-contracted build disagree in the last bit.
//      -ffp-contract=fast contracts across statements too, so splitting the expression is NOT
//      enough; the pragma is. It beats clang's default (-ffp-contract=on) but NOT an explicit
//      -ffp-contract=fast, which -ffast-math turns on. That is already a standing build rule
//      here ("never -ffast-math", tools/det-probes/README.md) and demath.c gates it.
//   3. Powers of two are built from the exponent bits, not by calling a library.
//   4. Results below the normal float range are flushed to zero on purpose. Denormal handling is
//      a per-CPU mode bit (SSE FTZ/DAZ, ARM FZ), so a denormal result is not portable either.
//
// Accuracy: every function below is within ~1 ULP of the correctly-rounded float result, which
// is at or under the resolution of a float. Coefficients are relative-error least-squares fits;
// they converge to the classical series (2*pi, ln2, -1/3, 2/15), which is the sanity check.

#ifndef DE_MATH_H
#define DE_MATH_H

#include <stdint.h>
#include <string.h>

// Pinned on every function below. Placing it per-function keeps the effect contained to this
// header: a file-scope pragma in a header silently changes codegen for everything included
// after it, which is not a thing a header should do to its includer.
#define DE_NO_CONTRACT _Pragma("clang fp contract(off)")

#define DE_TWO_PI      6.28318530717958647692f
#define DE_INV_TWO_PI  0.15915494309189533577f  // 1 / (2*pi)
#define DE_LOG2E       1.44269504088896340736f  // 1 / ln(2)

// ---------------------------------------------------------------- helpers

static inline float de_bits_to_float(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }

// 2^n, exact, built straight from the exponent field. n outside the normal range saturates.
static inline float de_pow2i(int n) {
    if (n >  127) return de_bits_to_float(0x7F800000u);   // +inf
    if (n < -126) return 0.0f;                            // flush, see note 4 above
    return de_bits_to_float((uint32_t)(n + 127) << 23);
}

// floor for the range a phase accumulator ever reaches. Above 2^23 a float is already an
// integer, so it is its own floor.
static inline float de_floorf(float x) {
    DE_NO_CONTRACT
    if (x >= 8388608.0f || x <= -8388608.0f) return x;
    float t = (float)(int32_t)x;              // truncates toward zero
    return (t > x) ? t - 1.0f : t;
}

static inline double de_floord(double x) {
    DE_NO_CONTRACT
    if (x >= 4503599627370496.0 || x <= -4503599627370496.0) return x;   // 2^52
    double t = (double)(int64_t)x;
    return (t > x) ? t - 1.0 : t;
}

// ---------------------------------------------------------------- sin / cos

// sin(2*pi*t), t in TURNS. This is the form sound.h actually wants: phase accumulators already
// run 0..1, so reduction is `t - floorf(t)` (exact) with no pi multiply to lose bits in.
// The core. Takes turns as a double so the wrap AND the quadrant fold both happen at full
// precision; only the final small quarter-angle is narrowed to float. Doing it the other way
// round (narrow first, fold second) costs the fold everything it needs: near half a turn the
// fold is a subtraction of two nearly-equal numbers, so a float input there leaves the reduced
// angle with only a couple of correct digits.
static inline float de_sin_turns_d(double t) {
DE_NO_CONTRACT
    // Reduce SYMMETRICALLY into [-0.5, 0.5), not into [0,1). Wrapping a tiny negative phase up
    // near 1.0 would round most of its precision away (floats step by 1.2e-7 up there).
    t = t - de_floord(t + 0.5);
    double q;
    if      (t >=  0.25) q =  0.5 - t;        // sin(pi - a) == sin(a)
    else if (t <  -0.25) q = -0.5 - t;        // sin(-pi - a) == sin(a)
    else                 q =  t;              // already in the good quarter, untouched
    // q now in [-0.25, 0.25]; odd polynomial, coefficients on x^1..x^9
    float x = (float)q;
    float s = x * x;
    float p = 39.827521108f;
    p = p * s + -76.588529412f;
    p = p * s + 81.602718746f;
    p = p * s + -41.341683053f;
    p = p * s + 6.2831852837f;
    return x * p;
}

// float -> double is exact, so these are the same values the all-float path produced.
static inline float de_sin_turns(float t) { DE_NO_CONTRACT return de_sin_turns_d((double)t); }
static inline float de_cos_turns(float t) { DE_NO_CONTRACT return de_sin_turns_d((double)t + 0.25); }

// Radians, for the handful of call sites that are not phase-based. The radians -> turns scale
// and the wrap run in DOUBLE: in float, x/(2*pi) for x around 20 keeps only ~5 digits of the
// fractional turn, which is the part that matters. Double + - * / are correctly rounded and
// mandated by IEEE 754 exactly as the float ones are, so this stays bit-portable. (The old
// 80-bit x87 hazard does not apply: x86-64 uses SSE, arm64 and wasm are true binary64.)
static inline float de_sinf(float x) { DE_NO_CONTRACT return de_sin_turns_d((double)x * 0.15915494309189533577); }
static inline float de_cosf(float x) { DE_NO_CONTRACT return de_sin_turns_d((double)x * 0.15915494309189533577 + 0.25); }

// ---------------------------------------------------------------- exp

// 2^f for f in [-0.5, 0.5] — the shared core. Split out so the exponent reduction can happen in
// whatever precision the caller needs before the polynomial runs.
static inline float de_exp2_frac(float f) {
DE_NO_CONTRACT
    float p = 0.00015331506148f;
    p = p * f + 0.0013394718260f;
    p = p * f + 0.0096184945599f;
    p = p * f + 0.055503422476f;
    p = p * f + 0.24022647334f;
    p = p * f + 0.69314719922f;
    p = p * f + 1.0000000004f;
    return p;
}

// 2^y, reduction in double. Every caller goes through here.
static inline float de_exp2d(double y) {
    DE_NO_CONTRACT
    if (y >=  128.0) return de_bits_to_float(0x7F800000u);
    if (y <= -126.0) return 0.0f;
    double n = de_floord(y + 0.5);                       // nearest integer
    return de_exp2_frac((float)(y - n)) * de_pow2i((int)n);
}

static inline float de_exp2f(float x) { DE_NO_CONTRACT return de_exp2d((double)x); }

// e^x. The x * log2(e) scale runs in double on purpose: in float it rounds, and a 1-ULP slip in
// an exponent of magnitude 43 comes out as ~30 ULP in the result (measured, before this fix).
static inline float de_expf(float x) { DE_NO_CONTRACT return de_exp2d((double)x * 1.4426950408889634074); }

// ---------------------------------------------------------------- tanh

// The soft-clip / saturator that sound.h leans on 33 times.
static inline float de_tanhf(float x) {
DE_NO_CONTRACT
    float a = (x < 0.0f) ? -x : x;
    float r;
    if (a < 0.55f) {
        // Near zero the (e-1)/(e+1) form cancels catastrophically, so use a direct fit.
        float s = a * a;
        float p = -0.0063233815386f;
        p = p * s + 0.021100912969f;
        p = p * s + -0.053857498316f;
        p = p * s + 0.13332604656f;
        p = p * s + -0.33333315774f;
        p = p * s + 0.99999999932f;
        r = a * p;
    } else if (a > 9.0f) {
        r = 1.0f;                             // tanh(9) is 1 - 2.5e-8, under float resolution
    } else {
        float e = de_exp2d((double)a * 2.8853900817779268149);   // e^(2a); >= 3, so no cancellation
        r = (e - 1.0f) / (e + 1.0f);
    }
    return (x < 0.0f) ? -r : r;
}

// ---------------------------------------------------------------- log / pow

// log2, returned as a double because de_powf multiplies it by the exponent: any error here gets
// scaled by y, so throwing away the extra bits before that multiply defeats the point.
// x must be > 0 (0 gives -inf, negative gives NaN, matching libm).
static inline double de_log2d(float x) {
    DE_NO_CONTRACT
    if (x <  0.0f) return (double)de_bits_to_float(0x7FC00000u);   // NaN
    if (x == 0.0f) return (double)de_bits_to_float(0xFF800000u);   // -inf
    int e = 0;
    uint32_t b; memcpy(&b, &x, 4);
    if ((b >> 23) == 0u) { x = x * 33554432.0f; e = -25; memcpy(&b, &x, 4); }  // lift a denormal
    e += (int)((b >> 23) & 0xFFu) - 127;
    b = (b & 0x807FFFFFu) | 0x3F800000u;      // mantissa alone, in [1,2)
    float m; memcpy(&m, &b, 4);
    if (m > 1.41421356f) { m = m * 0.5f; e += 1; }   // centre on 1, so t stays small
    // log2(m) = t * P(t^2) with t = (m-1)/(m+1); P converges to (2/ln2) * [1, 1/3, 1/5, 1/7, 1/9]
    double t = ((double)m - 1.0) / ((double)m + 1.0);
    double s = t * t;
    double p = 0.339784933646;
    p = p * s + 0.411722327606;
    p = p * s + 0.577082789151;
    p = p * s + 0.961796677292;
    p = p * s + 2.88539008179;
    return (double)e + t * p;
}

static inline float de_log2f(float x)  { DE_NO_CONTRACT return (float)de_log2d(x); }
static inline float de_logf(float x)   { DE_NO_CONTRACT return (float)(de_log2d(x) * 0.69314718055994530942); }
static inline float de_log10f(float x) { DE_NO_CONTRACT return (float)(de_log2d(x) * 0.30102999566398119521); }

// x^y. This is the one that matters most in sound.h: note frequency is 440 * powf(2, (midi-69)/12),
// so it sets the oscillator increment, and an error here does not stay put — it accumulates as
// phase drift for as long as the note sounds.
static inline float de_powf(float x, float y) {
    DE_NO_CONTRACT
    if (y == 0.0f) return 1.0f;
    if (x == 0.0f) return (y > 0.0f) ? 0.0f : de_bits_to_float(0x7F800000u);
    if (x < 0.0f) {
        // Only defined for an integer exponent, same as libm.
        float ay = (y < 0.0f) ? -y : y;
        if (de_floorf(ay) != ay) return de_bits_to_float(0x7FC00000u);   // NaN
        float h = ay * 0.5f;
        float r = de_exp2d((double)y * de_log2d(-x));
        return (de_floorf(h) != h) ? -r : r;      // odd exponent keeps the sign
    }
    return de_exp2d((double)y * de_log2d(x));
}

// ---------------------------------------------------------------- atan / atan2

// atan(u) for u in [0,1], in double. Two-stage reduction: fold [tan(pi/12), 1] down onto
// [-tan(pi/12), tan(pi/12)] with atan(u) = pi/6 + atan((u*sqrt3 - 1)/(sqrt3 + u)), then one short
// polynomial. Fitting [0,1] directly needs a high-degree poly whose normal equations go
// ill-conditioned; this way six terms land at 8e-12 relative, far under a float ULP.
// Coefficients converge to the classical 1, -1/3, 1/5, -1/7 series.
static inline double de_atan_unit(double u) {
    DE_NO_CONTRACT
    double add = 0.0;
    if (u > 0.26794919243112270) {                      // tan(pi/12)
        u = (u * 1.7320508075688772 - 1.0) / (1.7320508075688772 + u);
        add = 0.52359877559829882;                      // pi/6
    }
    double s = u * u;
    double p = -0.076151839713966524;
    p = p * s + 0.10996260772280680;
    p = p * s + -0.14281485544443365;
    p = p * s + 0.19999928298210498;
    p = p * s + -0.33333332878503963;
    p = p * s + 0.99999999999513633;
    return add + u * p;
}

// The angle of (x,y). Always divides the smaller magnitude by the larger, so the polynomial only
// ever sees [0,1] and the division cannot overflow. Signs come off the SIGN BIT, not a `< 0`
// test, so negative zero behaves like libm (atan2(-0.0, 1.0) is -0.0, not +0.0).
static inline float de_atan2f(float y, float x) {
    DE_NO_CONTRACT
    uint32_t xb, yb; memcpy(&xb, &x, 4); memcpy(&yb, &y, 4);
    int xneg = (int)(xb >> 31), yneg = (int)(yb >> 31);
    double ax = xneg ? -(double)x : (double)x;
    double ay = yneg ? -(double)y : (double)y;
    double a;
    if (ax == 0.0 && ay == 0.0) a = 0.0;                // atan2(+-0, +0) is +-0
    else if (ay <= ax)          a = de_atan_unit(ay / ax);
    else                        a = 1.57079632679489662 - de_atan_unit(ax / ay);   // pi/2 - atan(1/u)
    if (xneg) a = 3.14159265358979324 - a;              // second/third quadrant
    return (float)(yneg ? -a : a);
}

// ---------------------------------------------------------------- inverse trig / hypot

// sqrt is IEEE-mandated correctly rounded, so it is already bit-portable — the builtin maps
// straight to the hardware instruction and needs no libm.
#define DE_SQRT(x) __builtin_sqrt(x)

static inline float de_atanf(float x) {
    DE_NO_CONTRACT
    double a = (x < 0.0f) ? -(double)x : (double)x;
    double r = (a <= 1.0) ? de_atan_unit(a) : (1.57079632679489662 - de_atan_unit(1.0 / a));
    return (float)((x < 0.0f) ? -r : r);
}

// Both take the atan2 shape (angle of the point (cos, sin)) rather than atan(x/sqrt(1-x*x)),
// which would divide by zero at |x| = 1. Always feeding the polynomial the SMALLER of the two
// legs over the larger keeps the ratio inside [0,1], so no case needs a special branch.
//
// `1 - a*a` looks like it should cancel near a = 1, but it does not: a*a lands in [0.5, 1] there,
// and a subtraction of two values within a factor of two is exact (Sterbenz).
static inline float de_asinf(float x) {
    DE_NO_CONTRACT
    double v = (double)x;
    if (v > 1.0 || v < -1.0) return de_bits_to_float(0x7FC00000u);   // NaN, same as libm
    double a = (v < 0.0) ? -v : v;                       // |x|, the sine of the angle
    double c = DE_SQRT(1.0 - a * a);                     // the cosine, >= 0
    double r = (a <= c) ? de_atan_unit(a / c)
                        : 1.57079632679489662 - de_atan_unit(c / a);
    return (float)((v < 0.0) ? -r : r);
}

// Not pi/2 - asin(x): near x = 1 acos is tiny and that subtraction would throw away most of its
// significant digits. Computing the angle directly keeps full precision at both ends.
static inline float de_acosf(float x) {
    DE_NO_CONTRACT
    double v = (double)x;
    if (v > 1.0 || v < -1.0) return de_bits_to_float(0x7FC00000u);
    double a = (v < 0.0) ? -v : v;
    double c = DE_SQRT(1.0 - a * a);
    double r = (c <= a) ? de_atan_unit(c / a)             // acos(|x|), in [0, pi/2]
                        : 1.57079632679489662 - de_atan_unit(a / c);
    return (float)((v < 0.0) ? (3.14159265358979324 - r) : r);
}

// sqrt(x*x + y*y). Squaring in DOUBLE removes the overflow/underflow dance libm's hypotf does
// (a float's square always fits a double), and both the multiply and the sqrt are correctly
// rounded, so this is deterministic by construction.
static inline float de_hypotf(float x, float y) {
    DE_NO_CONTRACT
    double a = (double)x, b = (double)y;
    return (float)DE_SQRT(a * a + b * b);
}

// ---------------------------------------------------------------- odds and ends

static inline float de_tanf(float x) {
    DE_NO_CONTRACT
    double t = (double)x * 0.15915494309189533577;
    return de_sin_turns_d(t) / de_sin_turns_d(t + 0.25);
}

static inline float de_sinhf(float x) {
    DE_NO_CONTRACT
    float a = (x < 0.0f) ? -x : x;
    float r;
    if (a < 0.5f) {                            // the exp form cancels here, same as tanh
        float s = a * a;
        float p = 1.0f / 362880.0f;
        p = p * s + 1.0f / 5040.0f;
        p = p * s + 1.0f / 120.0f;
        p = p * s + 1.0f / 6.0f;
        p = p * s + 1.0f;
        r = a * p;
    } else {
        r = 0.5f * (de_expf(a) - de_expf(-a));
    }
    return (x < 0.0f) ? -r : r;
}

#endif // DE_MATH_H
