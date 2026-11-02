/* de:meta
{
  "slug": "groupprobe",
  "title": "group bus + submix probe",
  "status": "hidden",
  "created": "2026-11-02",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "algorithm-visualization"
  ],
  "lineage": "Measurement probe for instrument_group() and reverb_bus_into() (issue #47: a whole band on one worn record, its room included, with the sound effects left dry). Rendered headless, stem by stem with --solo-slot, and read back by pitch and level; not played by hand.",
  "description": "Measurement probe, not a playable cart (status hidden). Three held sines: slot 5 (A4) and slot 6 (E5, from frame 31) form the band, slot 7 (A6) stands in for a sound effect. VARIANT 0 groups nothing; 1 groups 5+6 under ONE tape (heavy wow + warble) on the lead; 2 gives them a tape EACH, slot 6's started half a second later (the control: two wobbles that must not agree). 3 and 5 fold a reverb tank into the group (reverb_bus_into called before / after reverb_bus) behind a -12 dB mid cut on the lead, and release the notes at 3 s so the TAIL is left: it must come out 12 dB down with the band; 4 is the same without the fold, whose tail must stay full. Render it with `node tools/play.js groupprobe script /dev/null --headless --frames 420 --wav out.wav --solo-slot 5`."
}
de:meta */
// groupprobe — the group bus + submix measurement (issue #47).
//
// THE QUESTION is whether two slots wobble TOGETHER. Rendered one stem at a time (--solo-slot), each
// sine's pitch track should be the SAME curve in VARIANT 1 (one tape for the group) and two DIFFERENT
// curves in VARIANT 2 (a tape each) — the control without which "they correlate" proves nothing. Slot 7
// must not move at all. The submix half asks a different question, by LEVEL: does the reverb tail come
// back THROUGH the group's chain (3, 5) or around it (4)?
#include "studio.h"

static int VARIANT = 0;

static int f = 0;
static int h5, h6, h7;

void init(void) {
    for (int s = 5; s <= 7; s++) instrument(s, INSTR_SINE, 10, 0, 7, 300);
    if (VARIANT >= 1 && VARIANT != 2) instrument_group(6, 5);
    if (VARIANT == 1 || VARIANT == 2) {
        instrument_tape(5, 1.0f, 0.0f, 0.0f);          // all wow, no flutter, no saturation: a pure pitch wobble
        instrument_tape_warble(5, 0.8f);                // + the worn tape's random sags (what #47 asked for)
    }
    // VARIANT 2 = the CONTROL, a tape EACH. Wow is a deterministic LFO that starts at the same phase on
    // every bus, so two tapes set at once wobble identically and "they correlate" would be vacuous —
    // slot 6's tape is set in update(), half a second later.
    if (VARIANT >= 3) {
        if (VARIANT == 3) reverb_bus_into(1, 5);       // BEFORE the tank is claimed: the pending path
        reverb_bus(1, 0.8f, 0.2f);
        if (VARIANT == 5) reverb_bus_into(1, 5);       // AFTER: the direct path
        instrument_reverb_bus(5, 1, 0.6f);
        instrument_reverb_bus(6, 1, 0.6f);
        instrument_eq(5, 0.0f, -12.0f, 0.0f);          // the group's chain: -12 dB on the band the sines live in
    }
}

void update(void) {
    if (VARIANT == 2 && f == 30) { instrument_tape(6, 1.0f, 0.0f, 0.0f); instrument_tape_warble(6, 0.8f); }
    if (f == 0) { h5 = note_on(69, 5, 5); h7 = note_on(93, 7, 3); }
    if (f == 31) h6 = note_on(76, 6, 5);               // after slot 6's late tape: a voice keeps the bus it STARTED on
    if (VARIANT >= 3 && f == 180) { note_off(h5); note_off(h6); note_off(h7); }
    f++;
#ifdef DE_TRACE
    watch("variant", "%d", VARIANT);
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    print("groupprobe", 4, 4, CLR_WHITE);
}
