// demath.c — determinism oracle for runtime/demath.h.
//
// The question: does de_sin_turns / de_exp2f / de_expf / de_tanhf return the SAME BITS on
// arm64, x86-64 and wasm? That is the whole point of the header — libm's sinf/expf/tanhf do
// NOT (they differ ~1 ULP per implementation), which is what broke per-pixel rotation in
// textrot.c and what forced tools/web-audio-check.js into a two-tier verdict.
//
// Self-contained by the det-probes convention: no studio.h, no raylib, no math.h. It sweeps a
// fixed input grid, hashes the raw bit pattern of every result with FNV-1a, and prints plain
// text so native and wasm output compare directly.
//
//   clang -O2 demath.c -o /tmp/demath && /tmp/demath
//
// run.sh also builds it at -ffp-contract=off/on/fast. Those must agree too: a Horner step is
// the exact a*x+b shape a compiler fuses into an FMA, and a fused step keeps the product at
// full width instead of rounding it. demath.h pins contraction off per function so they do.

#include <stdint.h>
#include <stdio.h>
#include "../../runtime/demath.h"

static uint64_t h = 1469598103934665603ULL;   // FNV-1a offset basis
static void feed(float v) {
    uint32_t b; memcpy(&b, &v, 4);
    for (int i = 0; i < 4; i++) { h ^= (b >> (i * 8)) & 0xFF; h *= 1099511628211ULL; }
}

int main(void) {
    // The probe's OWN grid arithmetic is `base + i * step`, which is exactly the shape a
    // compiler fuses. Without this the probe would be measuring how main() got compiled rather
    // than what the header computes.
    DE_NO_CONTRACT
    // Phase accumulators in turns: several wraps either side of zero, plus the quadrant seams.
    for (int i = 0; i <= 200000; i++) {
        float t = -4.0f + (float)i * (8.0f / 200000.0f);
        feed(de_sin_turns(t));
        feed(de_cos_turns(t));
    }
    // The exact quadrant boundaries, where the branch picks a different reduction.
    float seams[] = { -1.0f, -0.75f, -0.5f, -0.25f, 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    for (int i = 0; i < 9; i++) { feed(de_sin_turns(seams[i])); feed(de_cos_turns(seams[i])); }

    // Radians form, over a few periods.
    for (int i = 0; i <= 50000; i++) {
        float x = -25.0f + (float)i * (50.0f / 50000.0f);
        feed(de_sinf(x)); feed(de_cosf(x));
    }
    // exp2 across the full normal range, including the flush and overflow edges.
    for (int i = 0; i <= 100000; i++) {
        float x = -130.0f + (float)i * (260.0f / 100000.0f);
        feed(de_exp2f(x));
    }
    // exp over the range sound.h actually uses (envelope/t60 arguments).
    for (int i = 0; i <= 100000; i++) {
        float x = -30.0f + (float)i * (60.0f / 100000.0f);
        feed(de_expf(x));
    }
    // tanh across both branches and the 0.55 / 9.0 seams.
    for (int i = 0; i <= 100000; i++) {
        float x = -12.0f + (float)i * (24.0f / 100000.0f);
        feed(de_tanhf(x));
    }
    float tseams[] = { -9.0f, -0.55f, -0.0f, 0.0f, 0.55f, 9.0f };
    for (int i = 0; i < 6; i++) feed(de_tanhf(tseams[i]));

    // A few spot values printed too, so a mismatch says WHICH function moved.
    printf("hash=%016llx | sin(.125)=%.9g exp2(0.5)=%.9g exp(1)=%.9g tanh(1)=%.9g\n",
           (unsigned long long)h,
           (double)de_sin_turns(0.125f), (double)de_exp2f(0.5f),
           (double)de_expf(1.0f), (double)de_tanhf(1.0f));
    return 0;
}
