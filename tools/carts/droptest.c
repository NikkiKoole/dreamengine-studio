// droptest — verification cart (NOT a showcase): a steady sine through dropout(), so we can render
// it and confirm the tape-catches actually remove energy (lower RMS than dry) and gate momentarily.
// Source-only, unregistered.
#include "studio.h"

#define I_S 0

void init(void) {
    instrument(I_S, INSTR_SINE, 1, 0, 7, 50);
    instrument_env(I_S, 0, ENV_CUTOFF, 0, 0, 0.0f);
    dropout(0.9f, 0.95f);   // heavy: frequent, near-full catches — should visibly gate the sine
    note_on(74, I_S, 5);
}
void update(void) {}
void draw(void) { cls(0); print("droptest: sine -> dropout(0.9, 0.95)", 8, 8, 7); }
