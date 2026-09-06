// detents.c — WHICH macro axes are stepped, and where are the steps?
//
//   bash tools/patch-match/build.sh tools/patch-match/detents.c build/pm-detents
//   ./build/pm-detents [--steps n] [--engine NAME]
//
// studio.h says several macros are "snapped" (FM's carrier:mod ratio, ORGAN's
// drawbar registrations, PIANO's voicings, PD's wavetypes) but never says how
// many positions there are or where they sit. The patch matcher needs that,
// because differential evolution takes DIFFERENCES between parameter vectors:
// on a stepped axis it is climbing a staircase while expecting a slope, and the
// measured cost is real (FM and ORGAN recover ~3x worse than the continuous-macro
// engines, and PIANO worse still).
//
// The measurement is exact rather than heuristic, and that is the whole trick: a
// snapped axis produces a BYTE-IDENTICAL render for every value inside one
// detent, because the value is quantized before it reaches the DSP. So sweeping
// the macro and grouping identical renders recovers the detent boundaries
// exactly, with no threshold to argue about. A continuous axis changes at every
// step and reports as one group per step, which is how the two are told apart.
//
// This is also a plain statement about the engine that nothing else records.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "platform.h"
#include "pmpatch.h"

extern PmPatch pm_req;
extern int pm_fire, pm_silence, pm_midi, pm_hold_ms, pm_vol;

#define SR   44100
#define RLEN (SR / 6)          // 0.17s: long enough to be past the attack, cheap enough for a sweep

static float stereo[RLEN * 2];

static uint64_t render_hash(const PmPatch *p, int midi)
{
    DeInstance *in = de_instance_create(DE_RENDERER_SOFTWARE);
    pm_req = *p; pm_fire = 1; pm_midi = midi; pm_hold_ms = 300; pm_vol = 5;
    de_frame(in, 0.0);
    de_audio_render(in, stereo, RLEN);
    de_instance_destroy(in);
    uint64_t h = 1469598103934665603ull;                 // FNV-1a over the raw samples
    const unsigned char *b = (const unsigned char *)stereo;
    for (size_t i = 0; i < sizeof(float) * RLEN * 2; i++) { h ^= b[i]; h *= 1099511628211ull; }
    return h;
}

int main(int argc, char **argv)
{
    int steps = 201, check = 0; const char *only = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--engine") && i+1 < argc) only = argv[++i];
        else if (!strcmp(argv[i], "--check")) { check = 1; steps = 101; }
    }

    // ── the GATE. pmpatch.h carries these detent positions as a table and the
    // search TRIES each one, so a table that has drifted from the engine is not a
    // stale comment, it is a search enumerating positions that no longer exist.
    // Re-measure and diff. Tolerance is half a sweep step, since a centre can only
    // ever be located to the resolution it was measured at.
    if (check) {
        uint64_t *hh = (uint64_t*)malloc(sizeof(uint64_t) * steps);
        int bad = 0;
        float tol = 0.5f / (float)(steps - 1) + 1e-4f;
        printf("\n  re-measuring the %d snapped axes pmpatch.h has a table for:\n\n", PM_NDETENT);
        for (int t = 0; t < PM_NDETENT; t++) {
            const PmDetents *d = &PM_DETENT[t];
            PmPatch p; pm_patch_default(&p, d->engine);
            for (int s = 0; s < steps; s++) { p.v[d->dim] = s / (float)(steps - 1); hh[s] = render_hash(&p, 60); }
            int n = 1;
            for (int s = 1; s < steps; s++) if (hh[s] != hh[s-1]) n++;
            int ok = (n == d->n), start = 0, k = 0;
            float worst = 0.0f;
            for (int s = 1; s <= steps && ok; s++)
                if (s == steps || hh[s] != hh[s-1]) {
                    float c = ((start + s - 1) * 0.5f) / (float)(steps - 1);
                    float e = c - d->centre[k]; if (e < 0) e = -e;
                    if (e > worst) worst = e;
                    if (e > tol) ok = 0;
                    start = s; k++;
                }
            printf("  %s %-15s %-10s table %d detents, measured %d, worst centre drift %.4f (tol %.4f)\n",
                   ok ? "\033[32m✓\033[0m" : "\033[31m✗\033[0m", pm_engine_name(d->engine) + 6,
                   d->dim == V_HARM ? "harmonics" : (d->dim == V_TIMB ? "timbre" : "morph"),
                   d->n, n, worst, tol);
            if (!ok) bad++;
        }
        free(hh);
        printf("\n  %s\n\n", bad ? "\033[31mDRIFT — pmpatch.h's PM_DETENT no longer matches the engine; re-run without --check and update it\033[0m"
                                   : "\033[32mtable matches the engine\033[0m");
        return bad ? 1 : 0;
    }
    const char *mname[3] = { "harmonics", "timbre", "morph" };
    const int   midx[3]  = { V_HARM, V_TIMB, V_MORPH };
    uint64_t *h = (uint64_t*)malloc(sizeof(uint64_t) * steps);

    printf("\n  sweeping each engine macro in %d steps, grouping BYTE-IDENTICAL renders.\n", steps);
    printf("  a snapped axis quantizes before the DSP, so one detent = one group.\n");
    printf("  groups == steps means the axis is CONTINUOUS (it changes everywhere).\n\n");
    printf("  %-15s %-11s %6s  %s\n", "engine", "macro", "groups", "detent centres");

    for (int e = 0; e < PM_NENGINES; e++) {
        if (only && !strcasestr(PM_ENGINE_NAME[e], only)) continue;
        for (int mm = 0; mm < 3; mm++) {
            PmPatch p; pm_patch_default(&p, PM_ENGINE[e]);
            for (int s = 0; s < steps; s++) {
                p.v[midx[mm]] = s / (float)(steps - 1);
                h[s] = render_hash(&p, 60);
            }
            // group RUNS of identical renders. Runs, not a set: a value repeating
            // far away is a different plateau, not the same detent.
            int groups = 1;
            for (int s = 1; s < steps; s++) if (h[s] != h[s-1]) groups++;
            const char *verdict = (groups >= steps - 1) ? "continuous" : "SNAPPED";
            printf("  %-15s %-11s %6d  %s", PM_ENGINE_NAME[e] + 6, mname[mm], groups, verdict);
            if (groups < steps - 1 && groups <= 24) {
                printf(" ->");
                int start = 0;
                for (int s = 1; s <= steps; s++)
                    if (s == steps || h[s] != h[s-1]) {
                        printf(" %.3f", ((start + s - 1) * 0.5f) / (float)(steps - 1));
                        start = s;
                    }
            }
            printf("\n");
        }
    }
    free(h);
    return 0;
}
