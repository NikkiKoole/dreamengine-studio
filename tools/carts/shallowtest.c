// shallowtest — verification cart (throwaway): a sustained sine through shallow() full-wet, so we can
// render it and confirm the K-field delay + Low Pass Gate transform the signal while a STEADY tone
// keeps the gate open (level near dry; the gate only closes on decaying tails). Source-only.
#include "studio.h"

#define I_S 0

void init(void) {
    instrument(I_S, INSTR_SINE, 1, 0, 7, 50);
    instrument_env(I_S, 0, ENV_CUTOFF, 0, 0, 0.0f);
    shallow(2.0f, 0.8f, 1.0f);   // full wet so the effect is the whole output
    note_on(67, I_S, 6);          // sustained tone — the gate should stay OPEN, warble audible
}
void update(void) {}
void draw(void) { cls(0); print("shallowtest: sine -> shallow(2, 0.8, 1)", 8, 8, 7); }
