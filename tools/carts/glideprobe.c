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
  "lineage": "Measurement probe for portamento (synth-secrets audit §B1, plan item 3.26) — glides between real notes at four intervals with one glide setting, so \"does ms mean ms, at every interval\" is a table instead of an argument.",
  "description": "Measurement probe for the glide curve and its timing, not a playable cart (status hidden). Glides C4 up and back at four intervals (semitone, fifth, octave, three octaves) with a single note_glide setting, so every leg should take the same measured time."
}
de:meta */
#include "studio.h"

// PROBE for portamento — audit §B1 / plan 3.26. Not a demo.
//
// It answers two different questions, and they needed two different fixes:
//
//   1. IS THE CURVE EVEN IN PITCH?  A slide up and the same slide back down must be mirror images.
//      The old linear-Hz slew was 82.4% through the interval going up and 37.2% coming down at the
//      same instant — a 45-point asymmetry. Every leg below is an up/down PAIR so this stays visible.
//
//   2. DOES `ms` MEAN MILLISECONDS, AT EVERY INTERVAL?  This is the one a one-pole could never pass.
//      It approaches asymptotically, so `ms` could only name a time constant, and the duration you
//      actually HEAR then depended on the interval: with remaining distance `D·e^(-t/τ)` and ~5 cents
//      as the audibility floor, a semitone settled at ~3.0τ but three octaves took ~6.6τ. So the four
//      legs below deliberately span a semitone to three octaves at ONE `note_glide` setting. With a
//      fixed-duration ramp they must all finish together; that is the whole claim.
//
// ── HOW TO RUN IT ────────────────────────────────────────────────────────────────────────────────
//   node tools/play.js glideprobe script /dev/null --headless --frames 820 --wav /tmp/g.wav
//   node tools/wav-envelope.js /tmp/g.wav 25 --from 0.9 --to 2.6      # semitone leg
//   node tools/wav-envelope.js /tmp/g.wav 25 --from 3.9 --to 5.6      # fifth
//   node tools/wav-envelope.js /tmp/g.wav 25 --from 6.9 --to 8.6      # octave
//   node tools/wav-envelope.js /tmp/g.wav 25 --from 9.9 --to 11.6     # three octaves
//
// Read the CENTROID column: on a sine it tracks pitch closely enough, and the glide is done at the
// window where it stops changing. Compare that against GLIDE_MS after the leg's start time.
//
// TIMING TRAP (same one retrigprobe documents): an event at frame N happens at t=(N-1)/60, so the
// legs start at 0.983s / 3.983s / 6.983s / 9.983s, NOT on the round seconds. Read them off the frames.
//
// WHY SINE: autocorrelation and the centroid both have nothing to trip on, and the envelope must not
// colour the pitch reading. Same reason tune-check uses it as its control.

#define SLOT 5
#define BASE 60          // C4
#define GLIDE_MS 600     // one setting for every leg — that is the point

// each leg: the note to glide TO, and a label. Up then back down, so the mirror check is free.
static const int LEG_NOTE[4] = { 61, 67, 72, 96 };            // +1, +7, +12, +36 semitones
static const char *LEG_NAME[4] = { "semitone", "fifth", "octave", "3 octaves" };

static int h = 0;
static int f = 0;
static int leg = -1;

void init(void) {
    instrument(SLOT, INSTR_SINE, 2, 0, 7, 200);   // flat + sustaining
}

void update(void) {
    f++;
    if (f == 1) { h = note_on(BASE, SLOT, 6); note_glide(h, GLIDE_MS); }
    // leg i: glide UP at frame 60 + i*180, back DOWN 90 frames later. 90 frames = 1.5s, so each
    // 600ms glide is followed by ~0.9s of steady tone — enough plateau to see where it landed.
    for (int i = 0; i < 4; i++) {
        if (f == 60 + i * 180)      { note_pitch(h, (float)LEG_NOTE[i]); leg = i; }
        if (f == 150 + i * 180)       note_pitch(h, (float)BASE);
    }
    if (f == 800) note_off(h);
}

void draw(void) {
    cls(CLR_BLACK);
    font(FONT_SMALL);
    print("glide probe (audit B1 / plan 3.26)", 8, 8, CLR_WHITE);
    print("not a demo - see the header for the recipe", 8, 20, CLR_DARK_GREY);
    print(str("one setting for every leg:  note_glide(%d)", GLIDE_MS), 8, 34, CLR_LIGHT_YELLOW);

    int y0 = 56, y1 = 150, x0 = 24, x1 = SCREEN_W - 8;
    line(x0, y1, x1, y1, CLR_DARKER_GREY);
    print("C4", 6, y1 - 3, CLR_DARK_GREY);
    print("C7", 6, y0 - 3, CLR_DARK_GREY);
    // one bracket per leg, drawn at its interval height so the four spans are visible at a glance
    for (int i = 0; i < 4; i++) {
        int up = 60 + i * 180, dn = 150 + i * 180;
        int xa = x0 + (x1 - x0) * up / 820;
        int xb = x0 + (x1 - x0) * dn / 820;
        int semis = LEG_NOTE[i] - BASE;
        int y = y1 - (y1 - y0) * semis / 36;
        int c = (leg == i) ? CLR_LIGHT_YELLOW : CLR_DARK_GREY;
        line(xa, y1, xa, y, c);
        line(xa, y, xb, y, c);
        line(xb, y, xb, y1, c);
        print(LEG_NAME[i], xa + 2, y - 8, c);
    }
    int px = x0 + (x1 - x0) * (f < 820 ? f : 820) / 820;
    line(px, y0 - 4, px, y1 + 6, CLR_BLUE);
    print(str("frame %d   every leg should take %dms", f, GLIDE_MS), 8, SCREEN_H - 12, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
