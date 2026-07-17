/* de:meta
{
  "slug": "voxbox",
  "title": "voxbox",
  "status": "active",
  "created": "2026-07-17",
  "kind": ["instrument"],
  "teaches": ["audio-input", "vocoder"],
  "lineage": "The REAL vocoder cart — a synth chord that speaks in your voice. Upgraded from the talkbox-lite first cut (mic_level gate + formant knob) to the engine's actual 12-band vocoder (vocoder() + vocoder_mic()) once Phase 2 landed the live-mic audio-thread ring (docs/design/vocoder.md). Carrier = a held saw chord; modulator = the LIVE mic. Sibling of `vocode` (same vocoder, modulated by a synth phrase instead of your voice) and `vowel` (the single-input formant knob).",
  "homage": "The vocoder / talkbox (Kraftwerk, Wendy Carlos, Daft Punk, Frampton) — a synth that speaks.",
  "description": {
    "summary": "Sing or talk and a synth chord speaks in YOUR voice — the real vocoder. A held saw chord is the carrier; your live mic is the modulator, run through the engine's 12-band vocoder, so the chord takes on your actual vowels and rhythm. Silent until you make a sound.",
    "detail": "Carrier × modulator, for real: a held INSTR_SAW chord is the CARRIER (rich + broadband so every band has energy); the LIVE MIC is the MODULATOR, fed to the vocoder through the Phase-2 audio-thread ring (vocoder_mic). The 12-band filterbank imposes your voice's spectral envelope on the chord — its vowels AND rhythm come through, so the chord literally speaks/sings your words. Hold a chord and talk. v2: the UNVOICED band (vocoder_unvoiced) makes your consonants — s/t/sh — come through as noise instead of tonal mush; V toggles it. LIVE by nature (the mic drives the sound in real time), so a voxbox performance does NOT replay deterministically — decision 0032. C cycles the chord under your voice.",
    "controls": "CLICK or SPACE: enable/disable the mic (allow the prompt). C: cycle the carrier chord. V: A/B the v2 unvoiced band (your consonants — s/t/sh — cutting through vs going to mush). Then sing, talk, or beatbox — the chord speaks it."
  }
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// VOXBOX — the real vocoder: a held saw chord (carrier) spoken by the LIVE mic (modulator) through
// the engine's 12-band vocoder (vocoder + vocoder_mic, Phase 2). The chord is silent until your voice
// drives it. Upgraded from the talkbox-lite first cut now that the mic-audio-thread ring exists.

#define PAD 5
static int   carrier[4];
static const int CHORDS[][4] = {
    { 48, 52, 55, 60 },   // Cmaj    C3 E3 G3 C4
    { 48, 51, 55, 58 },   // Cm7     C3 Eb3 G3 Bb3
    { 45, 52, 57, 60 },   // Am      A2 E3 A3 C4
    { 50, 54, 57, 62 },   // Dmaj    D3 F#3 A3 D4
};
static const char *CHORD_NM[] = { "Cmaj", "Cm7", "Am", "Dmaj" };
static int   chord_sel = 0;
static int   voc_live  = 0;      // vocoder currently engaged (mic open)
static int   uv        = 1;      // v2: unvoiced band on? (your consonants — s/t/sh — cut through)
static float env       = 0.0f;   // smoothed mic input loudness (mouth viz only)

void init(void) {
    instrument(PAD, INSTR_SAW, 8, 150, 7, 220);          // rich, broadband carrier — held
    for (int i = 0; i < 4; i++) carrier[i] = note_on(CHORDS[chord_sel][i], PAD, 0);  // silent until the mic opens
}

void update(void) {
    if (keyp(KEY_SPACE) || mouse_pressed(MOUSE_LEFT)) {
        if (mic_active()) mic_stop(); else mic_start();
    }
    if (keyp('C')) {                                     // cycle the carrier chord (repitch held notes, no retrigger)
        chord_sel = (chord_sel + 1) % 4;
        for (int i = 0; i < 4; i++) note_pitch(carrier[i], (float)CHORDS[chord_sel][i]);
    }
    if (keyp('V')) { uv = !uv; vocoder_unvoiced(uv ? 0.85f : 0.0f); }   // A/B your consonants (v2)

    int active = mic_active();
    if (active && !voc_live) {                           // mic opened → carrier up + real vocoder on
        for (int i = 0; i < 4; i++) note_vol(carrier[i], 6.0f);
        vocoder(0.97f);                                  // the chord (master mix) wears the mic's spectrum
        vocoder_mic(1.0f);                               // the LIVE mic is the modulator
        vocoder_unvoiced(uv ? 0.85f : 0.0f);             // v2: your s/t/sh come through as noise, not mush
        voc_live = 1;
    } else if (!active && voc_live) {                    // mic closed → silence + vocoder off
        for (int i = 0; i < 4; i++) note_vol(carrier[i], 0.0f);
        vocoder(0.0f);
        vocoder_mic(0.0f);
        vocoder_unvoiced(0.0f);
        voc_live = 0;
    }
    float lvl = active ? mic_level() : 0.0f;
    env += (lvl - env) * (lvl > env ? 0.4f : 0.12f);     // for the mouth viz
}

// a mouth that OPENS with your voice's loudness — pure feedback
static void draw_mouth(int cx, int cy, float open, int col) {
    int hw = 26, hh = 1 + (int)(open * 26.0f);
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
        print("then sing or talk", SCREEN_W/2 - 64, SCREEN_H/2 + 20, CLR_MEDIUM_GREY);
        print("the chord speaks in your voice", SCREEN_W/2 - 88, SCREEN_H/2 + 36, CLR_DARK_GREY);
        return;
    }

    float open = env * 3.0f; if (open > 1.0f) open = 1.0f;
    int mcol = open < 0.15f ? CLR_DARK_BLUE : open < 0.5f ? CLR_INDIGO : open < 0.8f ? CLR_BLUE : CLR_WHITE;
    draw_mouth(SCREEN_W/2, SCREEN_H/2 - 4, open, mcol);

    // carrier chord bars pulsing with your voice
    for (int i = 0; i < 4; i++) {
        int bw = 26, bx = SCREEN_W/2 - 2*bw - 6 + i * (bw + 4), by = SCREEN_H - 42;
        int h = 2 + (int)(open * 30.0f);
        rectfill(bx, by - h, bw, h, open > 0.1f ? CLR_BLUE : CLR_DARK_BLUE);
        rect(bx, by - 32, bw, 32, CLR_DARKER_GREY);
    }

    print("sing / talk — the chord speaks", SCREEN_W/2 - 92, 22, CLR_MEDIUM_GREY);
    print(uv ? "consonants: ON" : "consonants: off", SCREEN_W/2 - 44, 34, uv ? CLR_GREEN : CLR_DARK_GREY);
    char buf[52];
    snprintf(buf, sizeof buf, "REAL VOCODER   %s   C: chord   V: consonants", CHORD_NM[chord_sel]);
    print(buf, 4, SCREEN_H - 12, CLR_MEDIUM_GREY);
}
