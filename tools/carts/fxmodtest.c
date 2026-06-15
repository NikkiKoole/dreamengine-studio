#include "studio.h"
#include <math.h>
// verification cart for fx_mod/fx_lfo (not registered). A held chord; a slow engine LFO
// swings SHIMMER_MIX 0..~1 every ~3s — the wet wash blooms in and out, so RMS/sec must
// rise and fall. Builds with -DFXMOD_OFF for the control (no modulation → flat, dry baseline).
#define SL 8
void init(void) {
    instrument(SL, INSTR_SAW, 140, 0, 6, 500);
    note_on(40, SL, 4); note_on(47, SL, 4); note_on(52, SL, 4);
    shimmer(0.85f, 0.4f, 0.7f, 0.0f);          // configured, mix 0 = off baseline
#ifndef FXMOD_OFF
    fx_lfo(0, FXMOD_SHIMMER_MIX, 0.33f, 0.5f, 0.5f);   // ~3s sweep, full 0..1 swing
#endif
}
void update(void) {}
void draw(void) { cls(0); }
