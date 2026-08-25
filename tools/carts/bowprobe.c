/* de:meta
{
  "slug": "bowprobe",
  "title": "bowprobe",
  "status": "active",
  "created": "2026-08-25",
  "kind": ["tech-demo"],
  "teaches": ["waveguide-synth"],
  "lineage": "Measurement probe for the INSTR_BOWED A/B against chrisjz/luthier (MIT), whose FDTD string was rendered offline and measured with the same oracles. Driven by tools/play.js --wav, not played by hand.",
  "description": "The emptiest cart that can answer 'what does one bowed string sound like': ONE sustained INSTR_BOWED note at A3 (MIDI 57), violin macros, bow on at 0.5s and off at 6.0s to match the reference render exactly. Exists because the `bowed` showcase cart has autoplay ON by default, so every render made through it is a CHORD — which silently contaminated a whole round of spectrum measurements before it was noticed. Nothing else here makes a sound, so every partial in the output came from that one string. BODY on unless -DPROBE_NOBODY. Read the numbers with tools/harmonic-spec.js and tools/inharm-spec.js; mind that a short analysis window ALIASES the engine's own 5.3 Hz / plus-minus 15 cent vibrato and will report a partial stretch that is not there."
}
de:meta */
#include "studio.h"

#define SLOT 0
static int h = -1;
static int f = 0;

void init(void) {
    instrument(SLOT, INSTR_BOWED, 80, 0, 7, 300);
    instrument_harmonics(SLOT, 0.45f);   // bow position — the cart's violin preset
    instrument_timbre   (SLOT, 0.30f);   // bow pressure
    instrument_morph    (SLOT, 0.70f);   // bow speed
#ifdef PROBE_NOBODY
    instrument_mode(SLOT, MODE_BOW_BODY, 0.0f);
#else
    instrument_mode(SLOT, MODE_BOW_BODY, 1.0f);
#endif
    instrument_mode(SLOT, MODE_BOW_SIZE, BOW_SIZE_VIOLIN);
}

void update(void) {
    f++;
    if (f == 30  && h < 0) h = note_on(57, SLOT, 100);   // 0.5s @60fps — A3, 220 Hz
    if (f == 360 && h >= 0) { note_off(h); h = -1; }     // 6.0s
}

void draw(void) {
    cls(CLR_BLACK);
    print(h >= 0 ? "BOWING A3" : "silent", 8, 8, CLR_WHITE);
}
