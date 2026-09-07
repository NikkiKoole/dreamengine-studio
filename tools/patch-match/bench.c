// bench.c — the three facts tools/patch-match is built on, measured and then
// GATED, so an engine change that breaks one of them says so instead of quietly
// making every search score noise.
//
//   bash tools/patch-match/build.sh tools/patch-match/bench.c build/pm-bench && ./build/pm-bench
//
//   1. SPEED. How long one candidate takes, which is what decides whether a
//      search can ever run live in a cart or stays an offline tool. MEASURED on
//      an M1: ~26 ms per second of rendered audio, and the loss is ~5.7 ms. The
//      RENDER is the bottleneck, not the analysis, which is the opposite of the
//      usual advice about this kind of tool.
//   2. DETERMINISM. Two renders of one patch must be byte-identical, or part of
//      every score is noise and the optimizer chases it.
//   3. ORDER INDEPENDENCE. Render A, then B, then A: the two A's must match, or
//      a candidate's score depends on what was evaluated before it.
//
// WHY A FRESH ENGINE PER CANDIDATE. 2 and 3 both FAIL when candidates share one
// instance: MALLET rendered differently every time, differing from sample 0, and
// a flush four times longer did not help, so it was never a ringing tail but
// per-voice state carried across notes. A fresh de_instance_create fixes both
// byte-exactly and is CHEAPER, because it skips the flush render entirely.
// `-shared` reproduces the defect on purpose (the NEGATIVE CONTROL): without it,
// "the renders match" would also pass if the two protocols were the same thing.
//
// LIVENESS. Every check here passes perfectly on silence, which is exactly what
// a broken render protocol produces, so a silent render is a FAILURE and not a
// very good score.
// de:engine-owner multi — this gate exists to compare the fresh-instance render protocol
// against the shared-instance one (`-shared`, its negative control), so it deliberately
// creates engines both ways.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "platform.h"
#include "pmpatch.h"

extern PmPatch pm_req;
extern int pm_fire, pm_silence, pm_midi, pm_hold_ms, pm_vol;

#define SR      44100
#define WINDOW  (SR)
#define FLUSH   (SR / 4)

static double now_s(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static float stereo[WINDOW * 2];
static int shared = 0, frame_no = 0;
static DeInstance *persistent = NULL;
static int failures = 0;

static void ok(int cond, const char *what, const char *detail)
{
    if (cond) printf("  \033[32m✓\033[0m %-26s %s\n", what, detail);
    else { printf("  \033[31m✗\033[0m %-26s %s\n", what, detail); failures++; }
}

// The shipping protocol: one frame issues the calls (they queue), then ONE pull.
// hit() schedules its own note-off in SAMPLES and sound_tick does no DSP, so the
// audio does not need stepping frame by frame.
static void render(const PmPatch *p, float *mono)
{
    DeInstance *in;
    if (shared) {
        if (!persistent) persistent = de_instance_create(DE_RENDERER_SOFTWARE);
        in = persistent;
        pm_silence = 1;
        de_frame(in, frame_no++ / 60.0);
        de_audio_render(in, stereo, FLUSH);
    } else {
        in = de_instance_create(DE_RENDERER_SOFTWARE);
    }
    pm_req = *p; pm_fire = 1; pm_midi = 60; pm_hold_ms = 1000; pm_vol = 5;
    de_frame(in, shared ? frame_no++ / 60.0 : 0.0);
    de_audio_render(in, stereo, WINDOW);
    for (int i = 0; i < WINDOW; i++) mono[i] = 0.5f * (stereo[i*2] + stereo[i*2+1]);
    if (!shared) de_instance_destroy(in);
}

static float peak(const float *x, int n) { float m = 0; for (int i = 0; i < n; i++) { float a = fabsf(x[i]); if (a > m) m = a; } return m; }

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "-shared")) shared = 1;
    static float a1[WINDOW], a2[WINDOW], b[WINDOW];
    char msg[160];

    PmPatch A, B, P;
    pm_patch_default(&A, 17); A.v[V_HARM]=0.7f; A.v[V_TIMB]=0.3f; A.v[V_MORPH]=0.8f;  // MALLET, rings
    pm_patch_default(&B, 25); B.v[V_TIMB]=0.9f;                                        // PIPE, noisy
    pm_patch_default(&P, 27); P.v[V_TIMB]=0.4f;                                        // PIANO

    printf("\n  window %.2fs   %s\n\n", WINDOW / (double)SR,
           shared ? "\033[33m-shared: NEGATIVE CONTROL, these must FAIL\033[0m" : "one fresh engine per candidate");

    render(&A, a1);   // warm up; the first render pays one-time init

    const int K = 40;
    double t0 = now_s();
    for (int i = 0; i < K; i++) { A.v[V_TIMB] = 0.2f + 0.6f * (i / (float)K); render(&A, a1); }
    double dt = now_s() - t0;
    printf("  render        %6.2f ms each  (%.0f candidates/sec/core; pop-40 x 160 gens = %.0fs of rendering)\n\n",
           dt / K * 1000.0, K / dt, 40 * 160 * dt / K);

    A.v[V_TIMB] = 0.3f;
    struct { const char *name; PmPatch *p; } cases[] = { {"MALLET", &A}, {"PIPE (noisy)", &B}, {"PIANO", &P} };
    for (int i = 0; i < 3; i++) {
        render(cases[i].p, a1);
        render(cases[i].p, a2);
        snprintf(msg, sizeof msg, "%s twice, peak %.4f", cases[i].name, peak(a1, WINDOW));
        ok(memcmp(a1, a2, sizeof a1) == 0, "deterministic", msg);
    }
    for (int i = 0; i < 3; i++) {
        render(cases[i].p, a1);
        render(&B, b);
        render(cases[i].p, a2);
        snprintf(msg, sizeof msg, "%s, other, %s", cases[i].name, cases[i].name);
        ok(memcmp(a1, a2, sizeof a1) == 0, "order independent", msg);
    }
    render(&A, a1); render(&B, b);
    snprintf(msg, sizeof msg, "MALLET %.4f, PIPE %.4f", peak(a1, WINDOW), peak(b, WINDOW));
    ok(peak(a1, WINDOW) > 1e-6f && peak(b, WINDOW) > 1e-6f, "audible (liveness)", msg);

    if (shared) {
        printf("\n  %s\n\n", failures ? "\033[32mCONTROL OK — the shared-instance protocol fails, as it must\033[0m"
                                      : "\033[31mCONTROL BROKEN — the defect did not reproduce, so the checks above prove nothing\033[0m");
        return failures ? 0 : 1;
    }
    printf("\n  %s\n\n", failures ? "\033[31mFAIL\033[0m" : "\033[32mPASS\033[0m");
    return failures ? 1 : 0;
}
