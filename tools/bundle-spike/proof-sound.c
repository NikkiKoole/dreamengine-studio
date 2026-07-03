// proof-sound.c — deterministic round-trip proof for de_switch_cart's per-cart sound
// context (share-panel.md build-ladder rung 1). Run via proof-sound.sh.
//
// Two fake "carts" share instrument SLOT 9 with deliberately different sounds — the
// exact collision the maker heard in the first bundle build (acid's 909 came back as
// yacht's strat). The script plays the same note three times into one WAV:
//
//   A  (0.5s)  cart 0's sound            — ctx 0 active
//   B  (2.0s)  cart 1's sound            — after de_switch_cart(1) + cart 1 config
//   A' (3.5s)  cart 0's sound RESTORED   — after de_switch_cart(0), NO reconfigure
//
// PASS = corr(A, A') ≈ 1 (the context restored cart 0's slot 9 bit-for-bit config)
// AND corr(A, B) low (the two sounds really differ — the comparison has teeth).
// Without the context mechanism, A' would play cart 1's sound (corr(A, A') low).
#include "studio.h"

static void cart0_config(void) {              // bright filtered saw, long tail
    instrument(9, INSTR_SAW, 5, 120, 700, 300);
    instrument_filter(9, FILTER_LOW, 2500, 400);
}
static void cart1_config(void) {              // dull thin square, short + detuned
    instrument(9, INSTR_SQUARE, 40, 80, 200, 60);
    instrument_duty(9, 0.15f);
    instrument_filter(9, FILTER_LOW, 500, 100);
    instrument_tune(9, 3.0f);
}

void init(void) { cart0_config(); }

void update(void) {
    int f = frame();
    if (f ==  30) note(60, 9, 7);             // A
    if (f ==  90) { de_switch_cart(1); cart1_config(); }
    if (f == 120) note(60, 9, 7);             // B
    if (f == 180) de_switch_cart(0);          // restore — cart 0 is NOT reconfigured
    if (f == 210) note(60, 9, 7);             // A'
}

void draw(void) { cls(0); print("ctx round-trip proof", 8, 8, 7); }
