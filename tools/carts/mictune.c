/* de:meta
{
  "slug": "mictune",
  "title": "mictune (auto-tune demo)",
  "status": "active",
  "kind": ["tech-demo"],
  "teaches": ["audio-input"],
  "created": "2026-07-17",
  "lineage": "The showcase for the engine's AUTO-TUNE — sample_autotune() (docs/design/transparent-autotune.md). Formant-preserving pitch correction: a sung/recorded take snaps onto a scale and de-wobbles while STILL sounding like the singer (not a chipmunk). Born as the offline TD-PSOLA spike (proven in two rounds: known-f0 formant preservation, then detector-driven correct-to-scale on a wobbly source); the DSP now lives in the engine and this cart just calls it. NOT hardtune (robot auto-tune — a vocoded carrier, throws your voice away); this KEEPS your voice.",
  "description": {
    "summary": "Auto-tune that keeps your voice. A wobbly, slightly-off 'ah' is pitch-corrected onto the scale by the engine's sample_autotune() — the wobble flattens and it snaps in tune, but it still sounds like the same vowel, not a chipmunk. A/B raw vs tuned. Press R to tune YOUR own voice from the mic.",
    "detail": "Showcase for sample_autotune(slot, root, scale, amount) — the engine's formant-preserving pitch correction (docs/design/transparent-autotune.md). init() synthesizes a hard, voice-like source (a sawtooth 'ah' through a fixed formant bank, pitch wobbling ~35 cents sharp of a scale note with vibrato + drift + breath), loads it into two slots, and auto-tunes one. The engine tracks the pitch, snaps it to the key, and shifts it there with TD-PSOLA so the formants (the vowel/identity) stay put. A raw vs S tuned: the tuned one is steady and in-tune but still an 'ah'. Press R to mic_record ~2 s of your live voice and run the exact same call (live-only, ADR-0032).",
    "controls": "A: play RAW (wobbly, off). S: play TUNED. R: record your voice + auto-tune it (then Z raw / X tuned). Space: A/B raw vs tuned."
  }
}
de:meta */
#include "studio.h"
#include <math.h>

// MICTUNE — showcase for the engine's AUTO-TUNE (sample_autotune, docs/design/transparent-autotune.md).
// A wobbly, off-pitch 'ah' is corrected onto the scale with the voice KEPT (formant-preserving). All
// the DSP is now in the engine; this cart just synthesizes a test voice and calls it. R tunes YOURS.

#define SR      44100
#define NLEN    88200            // 2.0 s
#define ROOT    0                // key = C
#define SCALE   SCALE_MINOR
#define AMOUNT  1.0f             // full correction (0 = off; ride it for gentler)

static float raw[NLEN];          // the wobbly 'ah'
static float tunedview[NLEN];    // a copy of the tuned slot, for the waveform display
static int   tunedN = 0;
static int   playing = 0, fc = 0, autocyc = 0;
static int   live_state = 0;     // 0 idle, 1 starting mic, 2 recording, 3 ready

// synth a HARD 'ah': wobbly pitch (vibrato + drift) + fixed formants + breath noise — a stand-in for
// a real (imperfect) sung note, so the auto-tune has something honest to correct.
static void synth_wobbly(float *out, int n) {
    static float saw[NLEN];
    float ph = 0.0f, drift = 0.0f; int seed = 12345;
    float baseM = 46.35f;                                 // ~119 Hz — a third of a semitone sharp of A#2, inside its basin
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        seed = seed * 1103515245 + 12345;
        float rnd = ((seed >> 16) & 0xffff) / 32767.5f - 1.0f;
        drift += 0.0004f * rnd; if (drift > 0.2f) drift = 0.2f; if (drift < -0.2f) drift = -0.2f;
        float vib = 0.35f * sinf(2.0f * 3.14159265f * 5.5f * t);
        float f0  = 440.0f * powf(2.0f, (baseM + vib + drift - 69.0f) / 12.0f);
        saw[i] = 2.0f * ph - 1.0f; ph += f0 / SR; if (ph >= 1.0f) ph -= 1.0f;
    }
    for (int i = 0; i < n; i++) out[i] = 0.0f;
    float FC[3] = { 730.0f, 1090.0f, 2440.0f }, GA[3] = { 1.0f, 0.7f, 0.35f };
    for (int b = 0; b < 3; b++) {
        float w0 = 2.0f * 3.14159265f * FC[b] / SR, cw = cosf(w0), sw = sinf(w0), q = 9.0f, alpha = sw / (2 * q);
        float b0 = alpha, b2 = -alpha, a0 = 1 + alpha, a1 = -2 * cw, a2 = 1 - alpha;
        b0 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (int i = 0; i < n; i++) {
            float x = saw[i], y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y; out[i] += GA[b] * y;
        }
    }
    int seed2 = 99;
    for (int i = 0; i < n; i++) { seed2 = seed2 * 1103515245 + 12345; out[i] += 0.05f * (((seed2 >> 16) & 0xffff) / 32767.5f - 1.0f); }
    float pk = 1e-6f; for (int i = 0; i < n; i++) { float a = fabsf(out[i]); if (a > pk) pk = a; }
    float g = 0.6f / pk; for (int i = 0; i < n; i++) out[i] *= g;
}

void init(void) {
    synth_wobbly(raw, NLEN);
    sample_load(0, raw, NLEN);                        // RAW (untouched)
    sample_load(1, raw, NLEN);                        // TUNED slot — start from raw, then correct
    sample_autotune(1, ROOT, SCALE, AMOUNT);          // ← the engine primitive does all the work
    tunedN = sample_read(1, tunedview, NLEN);         // pull it back for the waveform display
    for (int s = 0; s < 2; s++) { instrument(10 + s, INSTR_SAMPLE, 2, 0, 7, 40); instrument_sample(10 + s, s, 57); }
    instrument(12, INSTR_SAMPLE, 2, 0, 7, 40);        // live raw
    instrument(13, INSTR_SAMPLE, 2, 0, 7, 40);        // live tuned
}

static void fire(int which) { playing = which; hit(57, 10 + which, 7, 2000); }

void update(void) {
    if (keyp('A')) fire(0);
    if (keyp('S')) fire(1);

    // live mic: R → record 2 s → auto-tune YOUR voice with the same engine call
    if (keyp('R') && live_state == 0) { mic_start(); live_state = 1; }
    if (live_state == 1 && mic_active()) { mic_record(2.0f); live_state = 2; }
    if (live_state == 2 && !mic_recording()) {
        static float lr[NLEN];
        int ln = mic_record_read(lr, NLEN);
        if (ln > 2048) {
            sample_load(2, lr, ln); sample_load(3, lr, ln);
            sample_autotune(3, ROOT, SCALE, AMOUNT);
            instrument_sample(12, 2, 57); instrument_sample(13, 3, 57);
        }
        live_state = 3;
    }
    if (live_state == 3) {
        if (keyp('Z')) { playing = 2; hit(57, 12, 7, 2000); }
        if (keyp('X')) { playing = 3; hit(57, 13, 7, 2000); }
    }

    if (autocyc || fc < 260) { if (fc == 10) fire(0); if (fc == 130) fire(1); }
    fc++; if (autocyc && fc > 270) fc = 0;
    if (keyp(KEY_SPACE)) autocyc = !autocyc;

#ifdef DE_TRACE
    watch("playing", "%d", playing);
    watch("tunedN", "%d", tunedN);
#endif
}

static void draw_wave(const float *buf, int n, int y0, int h, int col) {
    int step = n / SCREEN_W; if (step < 1) step = 1;
    for (int x = 0; x < SCREEN_W; x++) {
        float mn = 1, mx = -1;
        for (int k = 0; k < step; k++) { int i = x * step + k; if (i < n) { float v = buf[i]; if (v < mn) mn = v; if (v > mx) mx = v; } }
        line(x, y0 + (int)(h * 0.5f * (1 - mx)), x, y0 + (int)(h * 0.5f * (1 - mn)), col);
    }
}

void draw(void) {
    cls(CLR_BLACK);
    print("MICTUNE  formant-preserving auto-tune", 6, 6, CLR_INDIGO);
    print("RAW  wobbly 'ah', off-pitch", 6, 26, playing == 0 ? CLR_WHITE : CLR_LIGHT_GREY);
    draw_wave(raw, NLEN, 38, 36, CLR_LIGHT_GREY);
    print("TUNED  snapped to scale, vowel kept", 6, 86, playing == 1 ? CLR_WHITE : CLR_GREEN);
    draw_wave(tunedview, tunedN, 98, 36, CLR_GREEN);
    print("tuned = in-tune + steady, still an 'ah'", 6, 146, CLR_MEDIUM_GREY);
    if (live_state == 2) print("recording... sing an 'ah'", 6, SCREEN_H - 26, CLR_ORANGE);
    else if (live_state == 3) print("your voice tuned!  Z raw  X tuned", 6, SCREEN_H - 26, CLR_GREEN);
    print("A raw   S tuned   SPACE a/b   R record + tune your voice", 6, SCREEN_H - 12, CLR_MEDIUM_GREY);
}
