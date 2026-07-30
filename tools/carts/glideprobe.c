/* de:meta
{
  "slug": "glideprobe",
  "title": "glide probe",
  "status": "hidden",
  "created": "2026-07-30",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "algorithm-visualization"
  ],
  "lineage": "Measurement probe for the portamento CURVE (synth-secrets audit §B1, plan item 3.26) — glides three octaves up and then the same three octaves down at a known time constant, so the up/down asymmetry of a linear-Hz slew is one number instead of an argument.",
  "description": "Measurement probe for the glide curve, not a playable cart (status hidden). Glides three octaves up and then back down with a 1000ms time constant, so f0 sampled at exactly one time constant says whether the slide is even in pitch or in linear Hz."
}
de:meta */
#include "studio.h"

// PROBE for the portamento CURVE — audit §B1 / plan 3.26. Not a demo.
//
// THE TEST, and it is a single number. Reid (Part 15/16) says hardware portamento lags the pitch CV,
// which is 1V/octave, so a glide is exponential in VOLTAGE, i.e. even in semitones. A one-pole lag on
// linear Hz is a different curve, and the way to see it is to ask WHERE THE PITCH IS after exactly one
// time constant. A one-pole is 63.2% of the way through in whatever domain it slews, so:
//
//   slewing linear Hz   → up glide is 81.3% through the INTERVAL, down glide only 38.7%.  Not mirrored.
//   slewing pitch       → both are 63.2%.                                                 Mirrored.
//
// Those two percentages are scale-invariant (they hold for any interval), which is what makes this a
// clean gate rather than a table of Hz to eyeball.
//
// ── HOW TO RUN IT ────────────────────────────────────────────────────────────────────────────────
//   node tools/play.js glideprobe script /dev/null --headless --frames 480 --wav /tmp/g.wav
//   node tools/formant-check.js /tmp/g.wav 1.955 2.015     # UP,   one time constant in
//   node tools/formant-check.js /tmp/g.wav 6.955 7.015     # DOWN, one time constant in
//   node tools/formant-check.js /tmp/g.wav 5.80 5.90       # UP settled  (expect ~1046 Hz)
//   node tools/formant-check.js /tmp/g.wav 7.90 7.98       # DOWN settled (expect ~131 Hz)
//
// Convert an f0 to "percent through the interval" with log2(f/131) / 3.
//
// WHY THESE NUMBERS: SINE so autocorrelation has nothing to trip on; C3→C6 (three octaves) because the
// asymmetry grows with the interval and three octaves is the audit's own example; a 1000ms time constant
// so the pitch barely moves across a 60ms analysis window; and the down-glide starts at frame 360, five
// time constants after the up-glide, so it begins from a settled note and not mid-slide.
//
// TIMING TRAP (the same one retrigprobe documents): an event at frame N happens at t=(N-1)/60, so the
// glides start at 0.9833s and 5.9833s, NOT 1.0 and 6.0. The measurement windows above already account
// for it. Read them off the frames, never off the round numbers.

#define SLOT 5
#define LO 48        // C3, 130.81 Hz
#define HI 84        // C6, 1046.50 Hz — exactly three octaves up
#define GLIDE_MS 1000

static int h = 0;
static int f = 0;

void init(void) {
    instrument(SLOT, INSTR_SINE, 2, 0, 7, 200);   // flat and sustaining: the envelope must not colour f0
}

void update(void) {
    f++;
    if (f == 1)  { h = note_on(LO, SLOT, 6); note_glide(h, GLIDE_MS); }
    if (f == 60)   note_pitch(h, (float)HI);    // UP   — glide starts at t = 59/60
    if (f == 360)  note_pitch(h, (float)LO);    // DOWN — glide starts at t = 359/60
    if (f == 478)  note_off(h);
}

void draw(void) {
    cls(CLR_BLACK);
    font(FONT_SMALL);
    print("glide curve probe (audit B1)", 8, 8, CLR_WHITE);
    print("not a demo - see the header for the recipe", 8, 20, CLR_DARK_GREY);
    // Draw where the PITCH should be, as a 0..1 fraction of the three-octave interval, so a glance
    // shows the shape: a linear-Hz slew visibly races away from the middle on the way up.
    int y0 = 60, y1 = 150, x0 = 20, x1 = SCREEN_W - 20;
    line(x0, y0, x0, y1, CLR_DARKER_GREY);
    line(x0, y1, x1, y1, CLR_DARKER_GREY);
    print("C6", 4, y0 - 3, CLR_DARK_GREY);
    print("C3", 4, y1 - 3, CLR_DARK_GREY);
    for (int i = 0; i < 3; i++) {   // the two glide starts + the halfway marks
        int fr = (i == 0) ? 60 : (i == 1) ? 360 : 478;
        int x = x0 + (x1 - x0) * fr / 480;
        line(x, y0, x, y1, CLR_DARKER_GREY);
    }
    int px = x0 + (x1 - x0) * (f < 480 ? f : 480) / 480;
    line(px, y0 - 6, px, y1 + 6, CLR_BLUE);
    print(str("frame %d   glide tau %dms   C3->C6->C3", f, GLIDE_MS), 8, SCREEN_H - 12, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
