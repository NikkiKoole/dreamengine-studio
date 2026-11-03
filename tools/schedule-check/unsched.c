// unsched.c — the host for the unschedule() probe (tools/carts/unschedcheck.c), run by run.sh.
//
// Frames at 60 Hz and audio in 1024-sample blocks, the same device-shaped host as probe.c. The cart
// books 20 clicks on slot 5 (on the step) and 20 on slot 6 (half a step later), then takes some back.
// Every onset is classified by its phase against the FIRST click (slot 5's click 0, which nothing
// ever unbooks), so the result is a heard/missing map per slot and click:
//   UC_MODE=1  slot 5 clicks 0..14 heard, 15..19 MISSING (open-ended, unbooked in the booking frame)
//              slot 6 clicks 0..4 + 10..19 heard, 5..9 MISSING (a window, unbooked half a second later)
//   UC_MODE=0  NEGATIVE CONTROL: nothing unbooked, so all 40 must be heard. Without it a "missing" click
//              could just be one the onset detector failed to see.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../runtime/platform.h"

#define BLOCK 1024
#define STEP  11025
#define N     20
#ifndef UC_MODE
#define UC_MODE 1
#endif

int main(void) {
    DeInstance *in = de_instance_create(DE_RENDERER_SOFTWARE);   // de:engine-owner
    if (!in) { printf("✗ no instance\n"); return 1; }
    const int frames = 60 * 8;
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
    long on[128]; int non = 0; long quiet = 100000;
    for (long i = 0; i < n && non < 128; i++) {
        if (fabsf(mono[i]) > 1e-3f) { if (quiet > 2000) on[non++] = i; quiet = 0; }
        else quiet++;
    }
    if (non == 0) { printf("  ✗ silent: the probe did not play\n"); return 1; }
    if (getenv("UC_DEBUG")) for (int k = 0; k < non && k < 12; k++) printf("  onset %d at +%ld\n", k, on[k] - on[0]);
    int heard[2][N] = { { 0 } }, stray = 0;
    for (int k = 0; k < non; k++) {
        long d = on[k] - on[0];
        long idx = lround(d * 2.0 / STEP);                 // which half-step it sits on (a half step is
        long err = labs(d - lround(idx * STEP / 2.0));     // 5512.5 samples: integer halves would drift)
        int slot = (int)(idx & 1), click = (int)(idx / 2);
        if (err > 2 || click >= N) { stray++; continue; }
        heard[slot][click] = 1;
    }
    int pass = stray == 0, nheard = 0;
    for (int s = 0; s < 2; s++) for (int c = 0; c < N; c++) {
        int want = 1;
#if UC_MODE
        if (s == 0 && c >= 15) want = 0;
        if (s == 1 && c >= 5 && c < 10) want = 0;
#endif
        nheard += heard[s][c];
        if (heard[s][c] != want) pass = 0;
    }
    for (int s = 0; s < 2; s++) {
        printf("  slot %d  ", 5 + s);
        for (int c = 0; c < N; c++) putchar(heard[s][c] ? 'x' : '.');
        putchar('\n');
    }
    printf("  %s %s (%d heard, %d stray)\n", pass ? "\033[32m✓\033[0m" : "\033[31m✗\033[0m",
           UC_MODE ? "unschedule drops exactly the window, and an open end, and leaves the other slot alone"
                   : "NEGATIVE CONTROL: with nothing unbooked, every one of the 40 clicks is heard",
           nheard, stray);
    de_instance_destroy(in);
    return pass ? 0 : 1;
}
