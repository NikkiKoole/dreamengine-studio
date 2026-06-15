// brasspec — throwaway VERIFICATION cart (not a showcase): one long sustained note on a chosen
// engine+macros so we can render a clean WAV and FFT the harmonic series (brass-realism work,
// docs/design/brass-realism-handoff.md). Pick engine/macros with -D at compile time via play.js
// isn't wired, so they're #defined here; edit + re-render to A/B. Default = forte trumpet A3.
#include "studio.h"

#ifndef SPEC_ENGINE
#define SPEC_ENGINE INSTR_BRASS
#endif
#ifndef SPEC_H
#define SPEC_H 0.15f      // harmonics (trumpet bore)
#endif
#ifndef SPEC_T
#define SPEC_T 0.80f      // timbre   (high brassiness = forte)
#endif
#ifndef SPEC_M
#define SPEC_M 0.55f      // morph    (breath)
#endif
#ifndef SPEC_MIDI
#define SPEC_MIDI 57      // A3 = 220 Hz → dense harmonics below Nyquist for the FFT
#endif

#define I_S 0

void init(void) {
    instrument(I_S, SPEC_ENGINE, 1, 0, 7, 4000);   // long held tone, no decay
    instrument_harmonics(I_S, SPEC_H);
    instrument_timbre(I_S, SPEC_T);
    instrument_morph(I_S, SPEC_M);
    note_on(SPEC_MIDI, I_S, 7);
}

void update(void) {}

void draw(void) {
    cls(0);
    print("brasspec: sustained note for FFT", 8, 8, 7);
}
