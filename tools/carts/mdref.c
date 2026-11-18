/* de:meta
{
  "slug": "mdref",
  "title": "mega drive reference probe",
  "status": "active",
  "created": "2026-11-17",
  "kind": [
    "probe"
  ],
  "teaches": [
    "fm-synth"
  ],
  "lineage": "Measurement probe: plays the exact note sequence of the zxspec Mega Drive patch-test ROM (megadrive/build.py with PATCHTEST=1, recorded through MAME's YM2612) on dreamchip's FM4 versions of those patches, so the two renders can be compared (zxspec/compare/). Not meant to be played.",
  "description": "Bass A2 E2 A3, e-piano A3 A4 E5, then the C pad chord, each held 8 ticks of 7 frames, the same timing as the Mega Drive test ROM. Render with: node tools/play.js mdref run --headless --frames 840 --wav out.wav"
}
de:meta */
// mdref — the dreamengine side of the Mega Drive comparison. Same notes, same timing as
// zxspec/megadrive patchtest.bin; patches = dreamchip's FM4 translation of the Mega Jams ones.
#include "studio.h"
#include <math.h>

#define ALG(n) (((n) + 0.5f) / 8.0f)
static float R(float ratio) {
    static const float T[13] = {0.5f, 1, 1.414f, 1.5f, 1.618f, 2, 2.414f, 3, 3.5f, 4, 5, 7, 14};
    int best = 0;
    for (int i = 1; i < 13; i++) if (fabsf(T[i] - ratio) < fabsf(T[best] - ratio)) best = i;
    return (best + 0.5f) / 13.0f;
}
static void fm(int slot, int a, int d, int s, int r, int alg, float r0, float r1, float r2, float r3,
               float bright, float feedback) {
    instrument(slot, INSTR_FM4, a, d, s, r);
    instrument_mode(slot, MODE_FM4_MAP, 1.0f);
    instrument_harmonics(slot, ALG(alg));
    instrument_mode(slot, MODE_FM4_R0, R(r0)); instrument_mode(slot, MODE_FM4_R1, R(r1));
    instrument_mode(slot, MODE_FM4_R2, R(r2)); instrument_mode(slot, MODE_FM4_R3, R(r3));
    instrument_timbre(slot, bright);
    instrument_morph(slot, feedback);
    instrument_crush(slot, 10.0f, 1.0f, 1.0f);
}

#define S_BASS 10
#define S_EP   11
#define S_PAD  12
#define TICK   (7.0 / 59.92)            // NTSC Mega Drive frames

void update(void) {
    static bool booked = false;
    if (booked) return;
    booked = true;
    fm(S_BASS, 1, 280, 4, 90, 0, 1, 1, 2, 1, 0.40f, 0.50f);              // dreamchip's bass
    instrument_env(S_BASS, 0, ENV_TIMBRE, 1, 110, 0.30f);
    instrument_drive(S_BASS, 0.10f);
    fm(S_EP, 2, 420, 3, 500, 4, 1, 1, 14, 1, 0.42f, 0.18f);              // dreamchip's e-piano
    instrument_env(S_EP, 0, ENV_TIMBRE, 1, 60, 0.20f);
    fm(S_PAD, 520, 0, 7, 700, 4, 1, 1, 2, 1, 0.28f, 0.20f);              // dreamchip's pad
    instrument_analog(S_PAD, 0.5f);

    double t0 = audio_time() + 0.2;
    int tick = 4;
    const int bass[3] = {45, 40, 57}, ep[3] = {57, 69, 76}, chord[3] = {55, 60, 64};
    for (int k = 0; k < 3; k++, tick += 12)
        schedule_at(t0 + tick * TICK, bass[k], S_BASS, 6, (int)(8 * TICK * 1000));
    for (int k = 0; k < 3; k++, tick += 12)
        schedule_at(t0 + tick * TICK, ep[k], S_EP, 6, (int)(8 * TICK * 1000));
    for (int k = 0; k < 3; k++)
        schedule_at(t0 + tick * TICK, chord[k], S_PAD, 5, (int)(16 * TICK * 1000));
}

void draw(void) { cls(CLR_BLACK); print("mega drive reference probe", 8, 8, CLR_LIGHT_GREY); }
