// shimmertest — verification cart (throwaway): a short chord stab into a full-wet shimmer with a
// strong climb, then silence, so we can render a long window and confirm (a) the feedback loop is
// STABLE (peak stays bounded, no runaway), (b) the tail BLOOMS and sustains (the ascending shimmer).
#include "studio.h"

#define I_S 0

void init(void) {
    instrument(I_S, INSTR_ORGAN, 40, 0, 7, 200);
    shimmer(0.85f, 0.4f, 0.7f, 0.7f);                  // sustained chord under the shimmer — it should build + ascend
    note_on(48, I_S, 6); note_on(55, I_S, 6); note_on(60, I_S, 6);   // a held C-major chord (organ sustains)
}
void update(void) {}
void draw(void) { cls(0); print("shimmertest: chord -> shimmer(0.88,0.35,0.8,1)", 8, 8, 7); }
