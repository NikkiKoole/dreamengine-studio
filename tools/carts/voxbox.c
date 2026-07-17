/* de:meta
{
  "slug": "voxbox",
  "title": "voxbox",
  "status": "active",
  "created": "2026-07-17",
  "kind": ["instrument"],
  "teaches": ["audio-input"],
  "lineage": "The first cart where the mic MODULATES a synth — the payoff of the Tier-1 mic seam meeting the parked vocoder/talkbox family (docs/design/audio-notes.md §13, effects-recipes.md). A held saw chord (the carrier) is driven by your voice (the modulator): its VOLUME follows your loudness (mic_level) so the chord speaks your rhythm, and its VOWEL follows your pitch (mic_pitch) through the shipped formant filter. Sibling of `vowel` (the same formant chord drone, but played with a knob instead of your mouth).",
  "homage": "The vocoder / talkbox (Alvin Lucier, Kraftwerk, Daft Punk, Frampton) — a synth that speaks in your voice.",
  "description": {
    "summary": "Talk or beatbox and a synth chord speaks in your rhythm — the vocoder move. Your loudness gates a held saw chord (so it pulses with your speech) and your pitch sweeps its vowel through the formant filter, for the robot-choir / talkbox sound.",
    "detail": "Carrier x modulator, the fantasy-console way: the CARRIER is a held formant-filtered saw chord; the MODULATOR is your mic. mic_level() rides the chord's volume (fast attack / slow release) so the chord only sounds while you talk — it takes on your speech RHYTHM, the recognizable vocoder pulse. mic_pitch() (YIN) sweeps the formant VOWEL, so the chord's colour moves as your voice does. HONEST SCOPE: this is the talkbox-LITE / 1-band version — it transfers your envelope + pitch, not your actual vowels. A true vocoder splits your voice into many frequency bands and imposes each band's envelope on the carrier; that needs per-band mic analysis in the engine (see audio-notes §13). This proves the routing + feel with zero engine work.",
    "controls": "CLICK or SPACE: enable/disable the mic (allow the prompt). C: cycle the chord. Then talk, sing, or beatbox into it."
  }
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// VOXBOX — mic-as-modulator talkbox-lite. A held saw chord (carrier) through the formant filter,
// driven by the mic: loudness (mic_level) gates the chord volume (speech rhythm = the vocoder pulse),
// pitch (mic_pitch) sweeps the vowel. NOT a true N-band vocoder (that needs per-band mic analysis) —
// it transfers your envelope + pitch, not your vowels. The payoff of the Tier-1 mic seam.

#define PAD 5
static int   carrier[4];                                // held carrier notes (gated by the mic)
static const int CHORDS[][4] = {
    { 48, 52, 55, 60 },   // Cmaj    C3 E3 G3 C4
    { 48, 51, 55, 58 },   // Cm7     C3 Eb3 G3 Bb3
    { 45, 52, 57, 60 },   // Am      A2 E3 A3 C4
    { 50, 54, 57, 62 },   // Dmaj    D3 F#3 A3 D4
};
static const char *CHORD_NM[] = { "Cmaj", "Cm7", "Am", "Dmaj" };
static int   chord_sel = 0;
static const char *VNAME[5] = { "OO", "OH", "AH", "EH", "EE" };

static float env = 0.0f;                                 // smoothed loudness → chord volume (0..7)
static float vw  = 0.4f;                                 // smoothed vowel position (0..1)

static float lastv = -1.0f;
static void apply_vowel(void) {                          // change-guarded (set-and-hold coeff update, like `vowel`)
    if (fabsf(vw - lastv) < 0.008f) return;
    instrument_formant(PAD, vw, 0.8f, 0.9f);
    lastv = vw;
}

void init(void) {
    instrument(PAD, INSTR_SAW, 6, 120, 7, 220);          // held drone: quick attack, full sustain — lots of harmonics for the formants
    for (int i = 0; i < 4; i++) carrier[i] = note_on(CHORDS[chord_sel][i], PAD, 0);  // start silent; the mic gates them up
    instrument_formant(PAD, vw, 0.8f, 0.9f);             // per-slot vowel filter: the carrier "sings"
}

void update(void) {
    if (keyp(KEY_SPACE) || mouse_pressed(MOUSE_LEFT)) {
        if (mic_active()) mic_stop(); else mic_start();
    }
    if (keyp('C')) {                                     // cycle the carrier chord (repitch held notes, no retrigger)
        chord_sel = (chord_sel + 1) % 4;
        for (int i = 0; i < 4; i++) note_pitch(carrier[i], (float)CHORDS[chord_sel][i]);
    }

    if (!mic_active()) {                                 // idle: fade the chord out
        env += (0.0f - env) * 0.1f;
        for (int i = 0; i < 4; i++) note_vol(carrier[i], env);
        return;
    }

    float lvl = mic_level();
    float tgt = lvl * 26.0f; if (tgt > 7.0f) tgt = 7.0f;
    env += (tgt - env) * (tgt > env ? 0.5f : 0.12f);     // fast attack / slow release → the speech-rhythm gate

    float hz = mic_pitch();
    if (hz > 0.0f) {                                     // pitch → vowel brightness (a crude spectral proxy — honest fake)
        float t = log2f(hz / 90.0f) / 2.2f;             // ~90..410 Hz → 0..1
        if (t < 0) t = 0; if (t > 1) t = 1;
        vw += (t - vw) * 0.15f;
    }
    apply_vowel();

    for (int i = 0; i < 4; i++) note_vol(carrier[i], env); // the whole chord speaks your rhythm
}

// a mouth that OPENS with loudness and WIDENS with the vowel (OO round → EE wide) — pure feedback
static void draw_mouth(int cx, int cy, float open, float vowel, int col) {
    int hw = 12 + (int)(vowel * 22.0f);                  // wider for brighter vowels
    int hh = 1  + (int)(open * 24.0f);                   // opens with loudness
    for (int x = -hw; x <= hw; x++) {
        float t = (float)x / hw;
        int h = (int)(hh * sqrtf(1.0f - t * t));
        line(cx + x, cy - h, cx + x, cy + h, col);
    }
}

void draw(void) {
    cls(CLR_BLACK);
    print("VOXBOX", 4, 4, CLR_INDIGO);

    if (!mic_active()) {
        font(FONT_COMIC); print("voxbox", SCREEN_W/2 - 34, 40, CLR_INDIGO); font(FONT_NORMAL);
        print("click / space to start", SCREEN_W/2 - 84, SCREEN_H/2 + 6, CLR_INDIGO);
        print("then talk or beatbox", SCREEN_W/2 - 76, SCREEN_H/2 + 20, CLR_MEDIUM_GREY);
        print("the chord speaks in your voice", SCREEN_W/2 - 88, SCREEN_H/2 + 36, CLR_DARK_GREY);
        return;
    }

    float open = env / 7.0f;
    int mcol = open < 0.15f ? CLR_DARK_BLUE : open < 0.5f ? CLR_INDIGO : open < 0.8f ? CLR_BLUE : CLR_WHITE;
    draw_mouth(SCREEN_W/2, SCREEN_H/2 - 6, open, vw, mcol);

    // chord bars pulsing with the gate
    for (int i = 0; i < 4; i++) {
        int bw = 26, bx = SCREEN_W/2 - 2*bw - 6 + i * (bw + 4), by = SCREEN_H - 40;
        int h = 2 + (int)(open * 30.0f);
        rectfill(bx, by - h, bw, h, open > 0.1f ? CLR_BLUE : CLR_DARK_BLUE);
        rect(bx, by - 32, bw, 32, CLR_DARKER_GREY);
    }

    // vowel ruler OO..EE with a marker
    int rx = SCREEN_W/2 - 60, ry = 20, rw = 120;
    line(rx, ry, rx + rw, ry, CLR_DARK_GREY);
    for (int i = 0; i < 5; i++) print(VNAME[i], rx + i * rw/4 - 5, ry - 10, CLR_MEDIUM_GREY);
    int mx = rx + (int)(vw * rw);
    rectfill(mx - 1, ry - 2, 3, 5, CLR_YELLOW);

    // readouts
    char buf[48]; float hz = mic_pitch();
    snprintf(buf, sizeof buf, "%s   vowel %s   %d Hz", CHORD_NM[chord_sel], VNAME[(int)(vw*4+0.5f)], (int)(hz+0.5f));
    print(buf, 4, SCREEN_H - 12, CLR_MEDIUM_GREY);
    print("C: chord   space: mic", SCREEN_W - 130, 4, CLR_MEDIUM_GREY);
}
