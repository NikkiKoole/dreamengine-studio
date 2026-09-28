// probe.c — does a booked note land on its SAMPLE when frames and audio callbacks do not line up?
//
// The --wav harness cannot answer this: it pumps exactly 735 samples per 60 Hz frame, so the audio
// is locked to the frames and every scheduling method looks perfect. A real device is not like that:
// the audio thread pulls 1024-sample buffers (native) on its own cadence, and schedule_hit's delay is
// counted from whichever buffer happens to drain the request — up to one buffer (23 ms) of error per
// note, sweeping as the two cadences beat against each other (docs/design/audio-timing.md).
//
// So this host drives the real engine the way a device does: de_frame() at 60 Hz, de_audio_render()
// in fixed 1024-sample blocks whenever the frames have "owed" that much audio. The probe cart books
// a click every 0.25 s (11025 samples); this finds each click's onset and measures the gaps.
//
//   SC_MODE=1 (schedule_at on audio_time): every gap must be exactly 11025 (±1 sample)
//   SC_MODE=0 (NEGATIVE CONTROL, a schedule_hit delay from a frame clock): the gaps must SPREAD
//            by a good fraction of a buffer — else this host is not reproducing the device's
//            misalignment, and the green in mode 1 would mean nothing.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../runtime/platform.h"   // the host seam: DeInstance, de_frame, de_audio_render

#define BLOCK 1024
#define STEP  11025
#ifndef SC_MODE
#define SC_MODE 1
#endif

int main(void) {
    DeInstance *in = de_instance_create(DE_RENDERER_SOFTWARE);   // de:engine-owner
    if (!in) { printf("✗ no instance\n"); return 1; }
    const int frames = 60 * 12;                                    // 12 s: 40 clicks + the lead-in
    float *mono = calloc((size_t)frames * 735 + BLOCK * 4, sizeof(float));
    float buf[BLOCK * 2];
    long n = 0, owed = 0;
    for (int f = 0; f < frames; f++) {
        de_frame(in, f / 60.0);
        owed += 735;
        while (owed >= BLOCK) {                                    // the device pulls whole buffers
            de_audio_render(in, buf, BLOCK);
            for (int i = 0; i < BLOCK; i++) mono[n++] = buf[2 * i];
            owed -= BLOCK;
        }
    }
    // onsets: the first non-silent sample after at least 2000 samples of silence
    long on[64]; int non = 0; long quiet = 100000;
    for (long i = 0; i < n && non < 64; i++) {
        if (fabsf(mono[i]) > 1e-3f) { if (quiet > 2000) on[non++] = i; quiet = 0; }
        else quiet++;
    }
    long worst = 0; double sum = 0;
    for (int k = 1; k < non; k++) { long e = labs((on[k] - on[k - 1]) - STEP); if (e > worst) worst = e; sum += e; }
    int gaps = non - 1;
    printf("  mode %s: %d clicks, gap error worst %ld samples (%.2f ms), mean %.1f\n",
           SC_MODE ? "schedule_at " : "schedule_hit", non, worst, worst * 1000.0 / 44100.0, gaps > 0 ? sum / gaps : 0.0);
    int pass;
    if (gaps < 30) { printf("  ✗ too few clicks (%d) — the probe did not play\n", non); pass = 0; }
    else if (SC_MODE) pass = worst <= 1;
    else pass = worst >= BLOCK / 4;      // the control must show the device's swing
    printf("  %s %s\n", pass ? "\033[32m✓\033[0m" : "\033[31m✗\033[0m",
           SC_MODE ? "every click lands exactly one step apart"
                   : "NEGATIVE CONTROL: the old way swings by a buffer here (so this host reproduces the device)");
    de_instance_destroy(in);
    return pass ? 0 : 1;
}
