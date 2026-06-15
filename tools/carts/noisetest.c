// noisetest — verification cart (throwaway): amp_noise() with NO notes playing, so we can confirm
// the rig-noise floor is present in "silence" (RMS > 0) when ON, and exactly 0 when OFF. Source-only.
#include "studio.h"

void init(void) {
    amp_noise(0.6f, 0.4f, 60);   // hiss + hum, no notes — should hum/hiss on its own
}
void update(void) {}
void draw(void) { cls(0); print("noisetest: amp_noise(0.6, 0.4, 60), no notes", 8, 8, 7); }
