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
    "summary": "A saw chord that SPEAKS a synth phrase — the vocoder, now with CONSONANTS. A held chord (carrier) is shaped by a looping modulator of vowels (a voice) + consonants (noise 's'/'t' bursts), so it pulses in the phrase's rhythm and speaks its vowels AND crisp consonants. Z: A/B the vocoder; X: A/B the v2 unvoiced band and hear the consonants appear.",
    "detail": "The showcase + acceptance test for the master-stage vocoder (vocoder() / vocoder_send()) AND its v2 unvoiced band (vocoder_unvoiced()). Carrier = the master mix (a held INSTR_SAW chord — rich, broadband). Modulator = two send-only slots: an INSTR_VOICE looping VOWELS + an INSTR_NOISE firing CONSONANT bursts (the 's'/'t' onsets). The 12-band bank imposes the modulator's spectral envelope on the chord. v2: when the modulator is broadband hiss (a consonant) the top bands swap to NOISE instead of the tonal carrier, so 's'/'t' cut through crisply instead of turning to mush. Fully DETERMINISTIC (all synth sources, no mic) → doubles as the DSP acceptance test. Z bypasses (mix 0); X toggles the unvoiced band; [/] ride the wet mix.",
    "controls": "Z: A/B the vocoder (on/off). X: A/B the v2 unvoiced/consonant band. [ / ]: wet mix down/up. It plays itself — the phrase loops."
  }
}
de:meta */
#include "studio.h"
#include <stdio.h>

// VOCODE — Phase-1 showcase for the master-stage vocoder (docs/design/vocoder.md). A saw chord
// (carrier = the master mix) is cross-synthesized with a looping INSTR_VOICE phrase (modulator,
// vocoder_send send-only). Deterministic (two synth sources, no mic) → also the DSP's acceptance test.

#define CAR  5                                  // carrier slot: a saw chord
#define MOD  6                                  // modulator slot: the "talking" VOWEL voice
#define MODC 7                                  // modulator slot: the CONSONANT (an INSTR_NOISE fricative burst)
static int   car[4];
static const int CARCH[4] = { 48, 52, 55, 59 };  // Cmaj7 — C3 E3 G3 B3, saw = broadband carrier
static int   bypass = 0;
static float mix = 0.9f;
static int   uv = 1;                             // v2: unvoiced band on? (consonants come through as noise, not mush)
static float uva = 0.85f;                        // unvoiced amount when on

// a looping 16-step modulator phrase. VOWEL = a pitched note (0 = no vowel); CONS = fire a noise
// "sss"/"t" burst → a broadband, unpitched onset the vocoder can only render with the unvoiced band.
static const int VOWEL[16] = { 60,0,60,62, 64,0,62,60, 55,0,57,59, 60,0,64, 0 };
static const int CONS [16] = {  1,0, 0, 0,  1,0, 0, 1,  1,0, 0, 0,  0,0, 1, 0 };
static int   step = -1, fc = 0;

void init(void) {
    instrument(CAR, INSTR_SAW,   8, 150, 7, 220);         // held chord drone (the carrier)
    for (int i = 0; i < 4; i++) car[i] = note_on(CARCH[i], CAR, 6);
    instrument(MOD,  INSTR_VOICE, 4,  70, 7, 130);        // the modulator VOWEL voice
    instrument(MODC, INSTR_NOISE, 2,  55, 0,  35);        // the modulator CONSONANT (a short fricative hiss)
    vocoder_send(MOD,  1.0f);                             // both are modulators (send-only: dry muted)
    vocoder_send(MODC, 1.0f);
    vocoder(mix);                                         // carrier (the chord) wears the modulator spectrum
    vocoder_unvoiced(uv ? uva : 0.0f);                    // v2: consonants render as noise, not tonal mush
}

void update(void) {
    if (keyp('Z')) { bypass = !bypass; vocoder(bypass ? 0.0f : mix); }
    if (keyp('X')) { uv = !uv; vocoder_unvoiced(uv ? uva : 0.0f); }   // A/B the unvoiced band
    if (keyp(']')) { mix += 0.1f; if (mix > 1.0f) mix = 1.0f; if (!bypass) vocoder(mix); }
    if (keyp('[')) { mix -= 0.1f; if (mix < 0.0f) mix = 0.0f; if (!bypass) vocoder(mix); }

    if (++fc % 7 == 0) {                                  // step the phrase (~8.5 steps/sec — a lively rhythm)
        step = (step + 1) % 16;
        if (VOWEL[step] > 0) hit(VOWEL[step], MOD, 6, 130);   // a pitched vowel (heard only through the chord)
        if (CONS[step])      hit(72,          MODC, 6, 110);  // a noise consonant (an "s"/"t" onset)
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

    // modulator step sequencer — vowels (indigo) + consonants (orange = the noise "s"/"t")
    print("MODULATOR  vowels + consonants", 8, 104, CLR_MEDIUM_GREY);
    for (int i = 0; i < 16; i++) {
        int bx = 8 + i * 18, by = 116;
        int cur = (i == step);
        int col = cur ? CLR_YELLOW : (CONS[i] ? CLR_ORANGE : (VOWEL[i] > 0 ? CLR_INDIGO : CLR_DARKER_GREY));
        rectfill(bx, by, 15, 15, col);
        rect(bx, by, 15, 15, CLR_DARK_GREY);
    }
    print("indigo=vowel  orange=consonant", 8, 138, CLR_DARK_GREY);

    // v2 unvoiced-band state
    print("UNVOICED", 8, 158, CLR_MEDIUM_GREY);
    print(uv ? "ON  consonants cut through" : "OFF  consonants -> mush",
          80, 158, uv ? CLR_GREEN : CLR_MEDIUM_GREY);

    char buf[64];
    snprintf(buf, sizeof buf, "wet %.0f%%  Z:vocoder  X:unvoiced  [ ]:mix", mix * 100.0f);
    print(buf, 8, SCREEN_H - 14, CLR_MEDIUM_GREY);
}
