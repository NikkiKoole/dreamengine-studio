// navkit-bowsweep — STEP-0 sweep for the bowed-string engine (docs/design/instrument-engines.md §8.5 step 9).
//
// WHY: the bowed engine got parked off a SINGLE render (erratic envelope, crest 12.6). The DSP
// literature says that's the signature of bowing OUTSIDE the Schelleng wedge (a bow force × velocity
// × position regime problem), not an unstable engine. This tool renders navkit's bowed across that
// 3D grid and measures, per cell, whether the string settles into a stable periodic Helmholtz motion
// (low crest, high pitch-period autocorrelation) — i.e. it FINDS the wedge instead of judging one point.
//
// REQUIRES the navkit sibling at ~/Projects/navkit/soundsystem. Header-only C, no audio device.
//
// BUILD + RUN:
//   NK=~/Projects/navkit/soundsystem
//   clang -I "$NK" tools/navkit-bowsweep.c -lm -o /tmp/nkbowsweep
//   /tmp/nkbowsweep 130.81           # sweep at C3 (cello-ish); prints Schelleng maps per position
//   /tmp/nkbowsweep 196.0 /tmp/best.wav  0.7 0.5 0.08   # also dump one cell's WAV for listening
//
// Classification per (pressure, velocity, position) cell, judged on the STEADY-STATE tail (last 0.5s):
//   '.' silent / collapsed   (steady RMS below floor — no oscillation, "no speech")
//   '~' surface/unstable     (oscillates but low pitch-period correlation — double-slip, noisy)
//   '#' HELMHOLTZ            (periodic, low-to-moderate crest — the wedge: clean leaning-sawtooth)
//   'X' overpressure/crunch  (periodic-ish but very high crest or clipping — raucous)

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "soundsystem.h"
#include "engines/instrument_presets.h"

static void wav16(const char *p, float *d, int n, int sr) {
    FILE *f = fopen(p, "wb"); if (!f) { printf("can't write %s\n", p); return; }
    uint16_t fmt = 1, ch = 1, bps = 16, ba = 2; uint32_t srr = sr, brate = sr * 2, dl = n * 2, sz16 = 16; int br = 36 + n * 2;
    fwrite("RIFF", 1, 4, f); fwrite(&br, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&sz16, 4, 1, f);
    fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&srr, 4, 1, f); fwrite(&brate, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bps, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dl, 4, 1, f);
    for (int i = 0; i < n; i++) { float x = d[i]; if (x > 1) x = 1; if (x < -1) x = -1; int16_t s = (int16_t)(x * 32767); fwrite(&s, 2, 1, f); }
    fclose(f);
}

#define SR 44100
#define DUR 2.0f
#define N   ((int)(SR * DUR))

static float buf[N];

// Render one cell. Returns classification char; fills metrics by pointer.
static char render_cell(int basePreset, float freq, float pressure, float velocity, float position,
                        float *out_rms, float *out_crest, float *out_corr, float *out_clip,
                        const char *wavOut) {
    SynthPatch patch = instrumentPresets[basePreset].patch;   // clone the bowed-cello patch
    patch.p_bowPressure = pressure;
    patch.p_bowSpeed    = velocity;
    patch.p_bowPosition = position;

    initSynthContext(synthCtx);
    initEffectsContext(fxCtx);
    int vi = playNoteWithPatch(freq, &patch);
    if (vi < 0) return '?';

    for (int i = 0; i < N; i++) buf[i] = processVoice(&synthCtx->voices[vi], (float)SR);

    if (wavOut) wav16(wavOut, buf, N, SR);

    // ---- analyze the steady-state tail: last 0.5s ----
    int t0 = N - SR / 2;                 // start of analysis window
    int tn = N - t0;
    double sum = 0, sumsq = 0; float peak = 0; int clipped = 0;
    for (int i = t0; i < N; i++) {
        float x = buf[i];
        sum += x; sumsq += (double)x * x;
        float a = fabsf(x); if (a > peak) peak = a;
        if (a >= 0.999f) clipped++;
    }
    float mean = (float)(sum / tn);
    float rms  = sqrtf((float)(sumsq / tn));
    float crest = (rms > 1e-6f) ? peak / rms : 0.0f;

    // Normalized autocorrelation at the fundamental period (pitch lock = Helmholtz periodicity).
    int period = (int)(SR / freq + 0.5f);
    double ac = 0, e0 = 0;
    for (int i = t0; i + period < N; i++) {
        float a = buf[i] - mean, b = buf[i + period] - mean;
        ac += (double)a * b; e0 += (double)a * a;
    }
    float corr = (e0 > 1e-9) ? (float)(ac / e0) : 0.0f;
    float clipFrac = (float)clipped / tn;

    *out_rms = rms; *out_crest = crest; *out_corr = corr; *out_clip = clipFrac;

    // ---- classify ----
    if (rms < 0.003f)            return '.';   // collapsed / no speech
    if (clipFrac > 0.02f || crest > 6.0f) return 'X';   // overpressure / raucous / clipping
    if (corr > 0.80f && crest < 4.0f)     return '#';   // stable periodic Helmholtz
    return '~';                                 // oscillating but not locked = surface/unstable
}

int main(int argc, char **argv) {
    float freq = argc > 1 ? (float)atof(argv[1]) : 130.81f;   // default C3
    const char *wavOut = argc > 2 ? argv[2] : NULL;
    initInstrumentPresets();

    const int BASE = 107;   // Bowed Cello
    printf("# navkit bowed STEP-0 sweep — preset %d (%s), %.2f Hz, steady-state tail (last 0.5s)\n",
           BASE, instrumentPresets[BASE].name, freq);
    printf("# legend: '.'=collapsed  '~'=surface/unstable  '#'=HELMHOLTZ  'X'=overpressure/crunch\n");

    // Optional single-cell WAV dump (args: wav pressure velocity position)
    if (wavOut && argc > 5) {
        float p = (float)atof(argv[3]), v = (float)atof(argv[4]), po = (float)atof(argv[5]);
        float rms, crest, corr, clip;
        char c = render_cell(BASE, freq, p, v, po, &rms, &crest, &corr, &clip, wavOut);
        printf("\nsingle cell p=%.2f v=%.2f pos=%.2f -> '%c'  rms=%.3f crest=%.2f corr=%.3f clip=%.3f\n",
               p, v, po, c, rms, crest, corr, clip);
        printf("wrote %s\n", wavOut);
        return 0;
    }

    // Sweep grid. position = bow contact point (β, fraction from nut). Violin/cello bow near bridge
    // but init splits nut/bridge so small pos = near nut. Schelleng β is usually 0.02..0.2 from bridge.
    float positions[] = { 0.05f, 0.08f, 0.11f, 0.14f, 0.18f, 0.25f };
    float pressures[] = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f };  // rows (force, high->low printed)
    float velocities[]= { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f };  // cols
    int NP = sizeof(positions)/sizeof(float);
    int NPR = sizeof(pressures)/sizeof(float);
    int NV = sizeof(velocities)/sizeof(float);

    int helmCount = 0, total = 0;
    // collect a few representative healthy cells to report
    float bestCorr = -1; float bP=0,bV=0,bPos=0,bCrest=0;

    for (int pi = 0; pi < NP; pi++) {
        printf("\n=== bow position β = %.2f ===  (Schelleng: rows=pressure/force top=high, cols=velocity left=low)\n", positions[pi]);
        printf("        v:");
        for (int vi = 0; vi < NV; vi++) printf(" %.1f", velocities[vi]);
        printf("\n");
        for (int pr = NPR - 1; pr >= 0; pr--) {   // high force at top
            printf("  P=%.1f   ", pressures[pr]);
            for (int vi = 0; vi < NV; vi++) {
                float rms, crest, corr, clip;
                char c = render_cell(BASE, freq, pressures[pr], velocities[vi], positions[pi],
                                     &rms, &crest, &corr, &clip, NULL);
                printf("  %c ", c);
                total++;
                if (c == '#') {
                    helmCount++;
                    if (corr > bestCorr) { bestCorr = corr; bP = pressures[pr]; bV = velocities[vi]; bPos = positions[pi]; bCrest = crest; }
                }
            }
            printf("\n");
        }
    }

    printf("\n# SUMMARY: %d/%d cells classified Helmholtz-stable ('#')\n", helmCount, total);
    if (helmCount > 0)
        printf("# best-locked cell: pressure=%.2f velocity=%.2f position=%.2f  (corr=%.3f crest=%.2f)\n",
               bP, bV, bPos, bestCorr, bCrest);
    else
        printf("# NO stable wedge found at this pitch — engine may need the hysteresis bow table fallback.\n");
    return 0;
}
