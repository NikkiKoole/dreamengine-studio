// shimisotest — verification cart (throwaway): instrument_shimmer ISOLATION. shimmer is on SL_PAD's
// bus only. A stab on the DRY slot (SL_DRUM, another bus) must NOT bloom; the same stab on SL_PAD
// must bloom. Master shimmer() is OFF, so any tail is purely the per-instrument bus. Source-only.
#include "studio.h"

#define SL_PAD  8
#define SL_DRUM 9

void init(void) {
    instrument(SL_PAD,  INSTR_PLUCK, 2, 0, 3, 200);
    instrument(SL_DRUM, INSTR_PLUCK, 2, 0, 3, 200);
    instrument_shimmer(SL_PAD, 0.85f, 0.4f, 0.7f, 0.7f);   // shimmer ONLY the pad bus
    note_on(60, SL_DRUM, 6);                                // stab on the DRY slot → should stay dry
}
void update(void) {}
void draw(void) { cls(0); print("shimisotest: stab on the DRY slot", 8, 8, 7); }
