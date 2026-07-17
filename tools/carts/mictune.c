/* de:meta
{
  "slug": "mictune",
  "title": "mictune (auto-tune)",
  "status": "active",
  "kind": ["instrument", "tech-demo"],
  "teaches": ["audio-input"],
  "created": "2026-07-17",
  "lineage": "The showcase for the engine's AUTO-TUNE — sample_autotune() (docs/design/transparent-autotune.md). Formant-preserving pitch correction: sing a note and it snaps onto the scale + steadies, but STILL sounds like you (not a chipmunk, not the robot vocoder — that's hardtune). Born as the offline TD-PSOLA spike (formant preservation + detector-driven correct-to-scale), now an engine primitive this app just calls. The pitch ribbon shows your wobbly voice snapping to the scale grid.",
  "description": {
    "summary": "Sing a note and hear yourself in tune. Press SPACE, hold an 'ah' for a couple of seconds, and the engine's auto-tune snaps your pitch onto the scale and steadies the wobble — while you still sound like YOU. TAB compares your raw take to the tuned one; the ribbon shows your pitch snapping to the notes. No mic? Press D for a built-in demo voice.",
    "detail": "A front-end for sample_autotune(slot, root, scale, amount) — formant-preserving pitch correction (docs/design/transparent-autotune.md). SPACE opens the mic and records ~2.5 s; the engine tracks your pitch, snaps it to the key, and shifts it there with TD-PSOLA so your formants (your voice, the vowel) stay put — not the chipmunk a naive resample gives, not the robot a vocoder gives. It loops the result; TAB A/Bs raw vs tuned; S cycles the scale (re-tunes live); D loads a synthetic demo voice so you can hear it without a mic. The pitch ribbon draws your recorded pitch (dim) against where it snapped (bright) over a scale-note grid. LIVE mic path — a recorded take does not replay deterministically (ADR-0032).",
    "controls": "SPACE: sing (record + auto-tune), or sing again. TAB: A/B raw vs tuned. S: cycle scale. D: demo voice (no mic needed)."
  }
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// MICTUNE — the auto-tune app: sing → hear yourself snapped in tune, still sounding like you.
// A thin front-end for the engine's sample_autotune() (docs/design/transparent-autotune.md). The
// pitch ribbon visualizes your voice snapping to the scale; TAB A/Bs raw vs tuned; D = no-mic demo.

#define SR       44100
#define NLEN     132300          // 3.0 s take buffer
#define REC_SECS 2.5f
#define TRK      256             // pitch-contour samples for the ribbon
#define ROOT     0               // key = C

enum { ST_IDLE, ST_ARM, ST_REC, ST_DONE };
static int   st = ST_IDLE;
static float take[NLEN];         // the current RAW take (mic or demo) — kept so S can re-tune it
static int   takeN = 0;
static float trk[TRK];           // raw pitch contour, MIDI (for the ribbon)
static int   nTrk = 0;
static int   ab_tuned = 1;       // playback: 1 = tuned, 0 = raw
static int   scale_i = SCALE_MINOR;
static int   is_demo = 0;
static int   rec_frame = 0;

// scale tables mirrored for the VIZ snap + note grid (the AUDIO snap lives in the engine)
static const int SLEN[6] = { 7, 7, 5, 5, 6, 12 };
static const int SDEG[6][12] = {
    { 0,2,4,5,7,9,11 }, { 0,2,3,5,7,8,10 }, { 0,2,4,7,9 },
    { 0,3,5,7,10 }, { 0,3,5,6,7,10 }, { 0,1,2,3,4,5,6,7,8,9,10,11 },
};
static const char *SNAME[6] = { "major", "minor", "penta", "penta-", "blues", "chroma" };
static const char *NOTE[12] = { "C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B" };
static float vsnap(float m) {                          // nearest scale note to MIDI m (viz only)
    int len = SLEN[scale_i]; float best = m, bd = 1e9f;
    for (int o = 0; o < 11; o++) for (int i = 0; i < len; i++) {
        float c = 12.0f * o + ROOT + SDEG[scale_i][i], d = fabsf(m - c);
        if (d < bd) { bd = d; best = c; }
    }
    return best;
}
static float hz2m(float hz) { return hz > 0 ? 69.0f + 12.0f * log2f(hz / 440.0f) : 0.0f; }

// synthesize a demo 'ah' (wobbly, off-pitch) + its pitch contour, so the app works with no mic
static void make_demo(void) {
    static float saw[NLEN];
    float ph = 0.0f, drift = 0.0f; int seed = 12345; float baseM = 46.35f;
    nTrk = 0;
    for (int i = 0; i < NLEN; i++) {
        float t = (float)i / SR;
        seed = seed * 1103515245 + 12345;
        drift += 0.0004f * (((seed >> 16) & 0xffff) / 32767.5f - 1.0f);
        if (drift > 0.2f) drift = 0.2f; if (drift < -0.2f) drift = -0.2f;
        float sweep = 1.3f * sinf(2.0f * 3.14159265f * 0.35f * t);   // slow glide across a few notes → a visible staircase
        float m = baseM + sweep + 0.3f * sinf(2.0f * 3.14159265f * 5.5f * t) + drift;
        float f0 = 440.0f * powf(2.0f, (m - 69.0f) / 12.0f);
        saw[i] = 2.0f * ph - 1.0f; ph += f0 / SR; if (ph >= 1.0f) ph -= 1.0f;
        if (i % (NLEN / 200) == 0 && nTrk < TRK) trk[nTrk++] = m;
    }
    for (int i = 0; i < NLEN; i++) take[i] = 0.0f;
    float FC[3] = { 730, 1090, 2440 }, GA[3] = { 1.0f, 0.7f, 0.35f };
    for (int b = 0; b < 3; b++) {
        float w0 = 2 * 3.14159265f * FC[b] / SR, cw = cosf(w0), sw = sinf(w0), al = sw / 18.0f;
        float b0 = al, b2 = -al, a0 = 1 + al, a1 = -2 * cw, a2 = 1 - al;
        b0 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (int i = 0; i < NLEN; i++) { float x = saw[i], y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = x; y2 = y1; y1 = y; take[i] += GA[b] * y; }
    }
    float pk = 1e-6f; for (int i = 0; i < NLEN; i++) { float a = fabsf(take[i]); if (a > pk) pk = a; }
    for (int i = 0; i < NLEN; i++) take[i] *= 0.6f / pk;
    takeN = NLEN;
}

static void retune_and_play(void) {                   // load the raw take, tune a copy, loop the A/B pick
    sample_load(0, take, takeN);                      // slot 0 = raw
    sample_load(1, take, takeN);                      // slot 1 = tuned
    sample_autotune(1, ROOT, scale_i, 1.0f);          // ← the engine primitive
    note_off_all();
    instrument_sample(10, ab_tuned ? 1 : 0, 57);
    note_on(57, 10, 7);
}

void init(void) {
    instrument(10, INSTR_SAMPLE, 3, 0, 7, 60);
    instrument_sample_mode(10, SAMPLE_LOOP);          // loop the take so you can listen + toggle
}

void update(void) {
    if (keyp('S')) { scale_i = (scale_i + 1) % 6; if (st == ST_DONE) retune_and_play(); }
    if (keyp('D')) { make_demo(); is_demo = 1; ab_tuned = 1; retune_and_play(); st = ST_DONE; }

    if (keyp(KEY_SPACE) && (st == ST_IDLE || st == ST_DONE)) { mic_start(); st = ST_ARM; }
    if (st == ST_ARM && mic_active()) { mic_record(REC_SECS); nTrk = 0; rec_frame = 0; is_demo = 0; note_off_all(); st = ST_REC; }
    if (st == ST_REC) {
        rec_frame++;
        float hz = mic_pitch();                        // sample the live pitch contour for the ribbon
        if (nTrk < TRK) { float mv = hz > 0 ? hz2m(hz) : (nTrk ? trk[nTrk - 1] : 0.0f); trk[nTrk++] = mv; }
        if (!mic_recording()) {
            takeN = mic_record_read(take, NLEN);
            if (takeN > 2048) { ab_tuned = 1; retune_and_play(); }
            st = ST_DONE;
        }
    }
    if (st == ST_DONE && keyp(KEY_TAB)) { ab_tuned = !ab_tuned; note_off_all(); instrument_sample(10, ab_tuned ? 1 : 0, 57); note_on(57, 10, 7); }

#ifdef DE_TRACE
    watch("st", "%d", st);
    watch("takeN", "%d", takeN);
#endif
}

// the hero: the pitch ribbon — your recorded pitch (dim) vs where it SNAPPED (bright), on a note grid
static void draw_ribbon(int x0, int y0, int w, int h) {
    float mlo = 43.0f, mhi = 67.0f;                    // G2..G4 vocal window
    if (nTrk > 0) {                                    // auto-center on the take
        float sum = 0; int c = 0;
        for (int i = 0; i < nTrk; i++) if (trk[i] > 1) { sum += trk[i]; c++; }
        if (c) { float mean = sum / c; mlo = mean - 12; mhi = mean + 12; }
    }
    // scale-note gridlines + names
    for (int mm = (int)mlo; mm <= (int)mhi; mm++) {
        if (fabsf(vsnap((float)mm) - mm) > 0.01f) continue;   // only notes IN scale
        int y = y0 + h - (int)((mm - mlo) / (mhi - mlo) * h);
        line(x0, y, x0 + w, y, CLR_DARKER_GREY);
        print(NOTE[((mm % 12) + 12) % 12], x0 - 18, y - 3, CLR_DARK_GREY);
    }
    if (nTrk < 2) { print("(sing to see your pitch)", x0 + 8, y0 + h / 2, CLR_DARK_GREY); return; }
    for (int i = 0; i < nTrk; i++) {
        if (trk[i] < 1) continue;
        int x = x0 + i * w / nTrk;
        int yr = y0 + h - (int)((trk[i]        - mlo) / (mhi - mlo) * h);
        int yt = y0 + h - (int)((vsnap(trk[i]) - mlo) / (mhi - mlo) * h);
        if (yr >= y0 && yr < y0 + h) pset(x, yr, CLR_ORANGE);   // your raw pitch (wobbly)
        if (yt >= y0 && yt < y0 + h) pset(x, yt, CLR_GREEN);    // snapped to the scale
    }
}

void draw(void) {
    cls(CLR_BLACK);
    font(FONT_COMIC); print("mictune", 8, 5, CLR_INDIGO); font(FONT_NORMAL);
    print("auto-tune", 92, 9, CLR_MEDIUM_GREY);

    if (st == ST_IDLE) {
        print("Press SPACE and sing an 'ah'", SCREEN_W/2 - 108, 74, CLR_WHITE);
        print("hear it snap in tune - still your voice", SCREEN_W/2 - 152, 94, CLR_LIGHT_GREY);
        print("no mic?  press D for a demo", SCREEN_W/2 - 100, 118, CLR_MEDIUM_GREY);
    } else if (st == ST_ARM) {
        print("opening mic... (allow the prompt)", SCREEN_W/2 - 128, 90, CLR_ORANGE);
    } else if (st == ST_REC) {
        print("SING NOW", SCREEN_W/2 - 32, 38, CLR_ORANGE);
        int bw = 200, bx = SCREEN_W/2 - bw/2;          // countdown bar
        int fillw = (int)(bw * (1.0f - rec_frame / (REC_SECS * 60.0f)));
        rect(bx, 58, bw, 10, CLR_DARK_GREY);
        rectfill(bx, 58, fillw < 0 ? 0 : fillw, 10, CLR_ORANGE);
        draw_ribbon(30, 84, SCREEN_W - 44, 100);
    } else { // ST_DONE
        print(is_demo ? "demo" : "you", 8, 30, CLR_MEDIUM_GREY);
        print(ab_tuned ? "> TUNED" : "> RAW", 44, 30, ab_tuned ? CLR_GREEN : CLR_ORANGE);
        print(ab_tuned ? "TAB: hear raw" : "TAB: hear tuned", 118, 30, CLR_DARK_GREY);
        draw_ribbon(30, 50, SCREEN_W - 44, 118);
        print("orange = you    green = snapped", 30, 174, CLR_DARK_GREY);
    }

    char buf[48];
    snprintf(buf, sizeof buf, "SPACE sing   TAB a/b   S %s   D demo", SNAME[scale_i]);
    print(buf, 8, SCREEN_H - 12, CLR_MEDIUM_GREY);
}
