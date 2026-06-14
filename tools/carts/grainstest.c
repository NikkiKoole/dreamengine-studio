// grainstest — verification cart (NOT a showcase): a steady 600 Hz sine through the master grains()
// granular delay, so we can render it to a WAV and A/B against navkit's processGranularDelay.
// Note: ours uses a 3 s capture buffer vs navkit's 5 s, so absolute lookback (the position knob)
// differs — sample-identity is not the goal; the grain CHARACTER (RMS/crest/spectrum) is.
#include "studio.h"

#define I_S 0

void init(void) {
    instrument(I_S, INSTR_SINE, 1, 0, 7, 50);
    instrument_env(I_S, 0, ENV_CUTOFF, 0, 0, 0.0f);
    grains(120, 12, 0.8f, 0.3f, 0.2f, 0.5f);   // the values we verify against navkit
    note_on(74, I_S, 5);                       // D5 ≈ 587 Hz (close to the harness's 600 Hz probe)
}

void update(void) {}

void draw(void) {
    cls(0);
    print("grainstest: sine -> grains(120,12,0.8,0.3,0.2,0.5)", 8, 8, 7);
}
