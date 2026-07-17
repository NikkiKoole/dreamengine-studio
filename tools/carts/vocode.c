/* de:meta
{
  "slug": "vocode",
  "title": "vocode",
  "status": "active",
  "created": "2026-07-17",
  "kind": ["instrument", "tech-demo"],
  "teaches": ["vocoder"],
  "lineage": "Phase-1 showcase for the engine's new master-stage VOCODER (docs/design/vocoder.md): carrier×modulator cross-synthesis, proven with two SYNTH sources so it's fully deterministic (no mic yet). A held saw chord is the CARRIER; an INSTR_VOICE phrase is the MODULATOR (vocoder_send, send-only) — the chord takes on the phrase's rhythm + spectral shape. The mic becomes the modulator in Phase 2 (the live voxbox → real vocoder).",
  "description": {
    "summary": "A saw chord that SPEAKS a synth voice's phrase — the vocoder. A held chord (carrier) is shaped, band by band, by a looping INSTR_VOICE line (modulator), so the chord pulses in the phrase's rhythm and shifts colour with its vowels. Press Z to A/B the vocoder on/off and hear what it does.",
    "detail": "The first cart on the engine's master-stage vocoder (vocoder() / vocoder_send()). Carrier = the master mix (a held INSTR_SAW chord — rich, broadband, so every band has energy to shape). Modulator = an INSTR_VOICE slot looping a short phrase, routed in with vocoder_send() (SEND-ONLY: you never hear its dry, only how it carves the chord). The 12-band filterbank imposes the phrase's spectral envelope on the chord: its rhythm and vowel motion come through the chord. Fully DETERMINISTIC — both sources are synths, no mic — so it doubles as the acceptance test for the vocoder DSP. Z bypasses (mix 0) to A/B; [/] ride the wet mix.",
    "controls": "Z: A/B the vocoder (on/off). [ / ]: wet mix down/up. It plays itself — the modulator phrase loops."
  }
}
de:meta */
#include "studio.h"
#include <stdio.h>

// VOCODE — Phase-1 showcase for the master-stage vocoder (docs/design/vocoder.md). A saw chord
// (carrier = the master mix) is cross-synthesized with a looping INSTR_VOICE phrase (modulator,
// vocoder_send send-only). Deterministic (two synth sources, no mic) → also the DSP's acceptance test.

#define CAR 5                                   // carrier slot: a saw chord
#define MOD 6                                   // modulator slot: the "talking" voice
static int   car[4];
static const int CARCH[4] = { 48, 52, 55, 59 };  // Cmaj7 — C3 E3 G3 B3, saw = broadband carrier
static int   bypass = 0;
static float mix = 0.9f;

// a looping 16-step modulator phrase (MIDI note, 0 = rest) — gives rhythm + pitch/vowel motion
static const int PHRASE[16] = { 60,0,60,62, 64,0,62,60, 55,0,57,59, 60,0,64,0 };
static int   step = -1, fc = 0;

void init(void) {
    instrument(CAR, INSTR_SAW,   8, 150, 7, 220);         // held chord drone (the carrier)
    for (int i = 0; i < 4; i++) car[i] = note_on(CARCH[i], CAR, 6);
    instrument(MOD, INSTR_VOICE, 4,  70, 7, 130);         // the modulator voice
    vocoder_send(MOD, 1.0f);                              // MOD is the modulator (send-only: dry muted)
    vocoder(mix);                                         // carrier (the chord) wears MOD's spectrum
}

void update(void) {
    if (keyp('Z')) { bypass = !bypass; vocoder(bypass ? 0.0f : mix); }
    if (keyp(']')) { mix += 0.1f; if (mix > 1.0f) mix = 1.0f; if (!bypass) vocoder(mix); }
    if (keyp('[')) { mix -= 0.1f; if (mix < 0.0f) mix = 0.0f; if (!bypass) vocoder(mix); }

    if (++fc % 7 == 0) {                                  // step the phrase (~8.5 steps/sec — a lively rhythm)
        step = (step + 1) % 16;
        int n = PHRASE[step];
        if (n > 0) hit(n, MOD, 6, 130);                   // fire a modulator note (heard only through the chord)
    }
}

void draw(void) {
    cls(CLR_BLACK);
    font(FONT_COMIC); print("vocode", 8, 6, bypass ? CLR_MEDIUM_GREY : CLR_BLUE); font(FONT_NORMAL);
    print(bypass ? "vocoder OFF (raw chord)" : "the chord speaks the phrase", 8, 30, CLR_LIGHT_GREY);

    // carrier chord bars
    print("CARRIER  saw chord", 8, 52, CLR_MEDIUM_GREY);
    for (int i = 0; i < 4; i++) {
        int bx = 8 + i * 26, by = 64;
        rectfill(bx, by, 22, 26, CLR_DARK_BLUE);
        rect(bx, by, 22, 26, CLR_DARKER_GREY);
    }

    // modulator step sequencer
    print("MODULATOR  voice phrase", 8, 104, CLR_MEDIUM_GREY);
    for (int i = 0; i < 16; i++) {
        int bx = 8 + i * 18, by = 116;
        int on = PHRASE[i] > 0;
        int cur = (i == step);
        rectfill(bx, by, 15, 15, cur ? CLR_YELLOW : (on ? CLR_INDIGO : CLR_DARKER_GREY));
        rect(bx, by, 15, 15, CLR_DARK_GREY);
    }

    char buf[48];
    snprintf(buf, sizeof buf, "wet mix %.0f%%   Z: A/B   [ ]: mix", mix * 100.0f);
    print(buf, 8, SCREEN_H - 14, CLR_MEDIUM_GREY);
}
