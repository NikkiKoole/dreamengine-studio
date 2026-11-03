// gain.c — the host for NoteOpts.gain (tools/carts/unschedcheck.c built with UC_MODE=2), run by run.sh.
//
// 18 booked clicks in threes: plain (NULL opts) · gain 0.5 · opts with gain left 0 (= unset). Per click
// the PEAK of its first 400 samples. A booked note's gain is a fraction of its volume, so:
//   gain 0.5 / plain  must be 0.5 (±1%)       — the gain reaches the voice
//   unset / plain     must be 1 exactly       — the CONTROL: 0 means full, so a host that never sets
//                                               gain hears nothing change (a zeroed NoteOpts is safe)
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../runtime/platform.h"

#define BLOCK 1024
#define STEP  11025

int main(void) {
    DeInstance *in = de_instance_create(DE_RENDERER_SOFTWARE);   // de:engine-owner
    if (!in) { printf("✗ no instance\n"); return 1; }
    const int frames = 60 * 6;
    float *mono = calloc((size_t)frames * 735 + BLOCK * 4, sizeof(float));
    float buf[BLOCK * 2];
    long n = 0, owed = 0;
    for (int f = 0; f < frames; f++) {
        de_frame(in, f / 60.0);
        owed += 735;
        while (owed >= BLOCK) {
            de_audio_render(in, buf, BLOCK);
            for (int i = 0; i < BLOCK; i++) mono[n++] = buf[2 * i];
            owed -= BLOCK;
        }
    }
    long first = -1;
    for (long i = 0; i < n; i++) if (fabsf(mono[i]) > 1e-4f) { first = i; break; }
    if (first < 0) { printf("  ✗ silent: the probe did not play\n"); return 1; }
    double peak[18] = { 0 };
    for (int k = 0; k < 18; k++) {
        long s0 = first + (long)k * STEP;
        for (long i = s0; i < s0 + 400 && i < n; i++) if (fabs(mono[i]) > peak[k]) peak[k] = fabs(mono[i]);
    }
    double half = 0, unset = 0; int ok = 1;
    for (int g = 0; g < 6; g++) {
        double p = peak[3 * g], h = peak[3 * g + 1] / p, u = peak[3 * g + 2] / p;
        if (p <= 0 || fabs(h - 0.5) > 0.005 || fabs(u - 1.0) > 1e-6) ok = 0;
        half += h / 6; unset += u / 6;
    }
    printf("  gain 0.5 / plain = %.4f · unset / plain = %.6f\n", half, unset);
    printf("  %s NoteOpts.gain: half the gain, half the peak; gain left 0 plays exactly like no opts\n",
           ok ? "\033[32m✓\033[0m" : "\033[31m✗\033[0m");
    de_instance_destroy(in);
    return ok ? 0 : 1;
}
