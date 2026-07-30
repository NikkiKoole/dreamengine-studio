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
//   3. AND THE GLIDE SCALE AXIS.  With PROBE_PER_OCT the same four legs are run through
//      note_glide_scale(GLIDE_PER_OCT), where `ms` is the time per OCTAVE. Now they must deliberately
//      DIFFER, in exact arithmetic ratios: 600ms/oct gives 50ms / 350ms / 600ms / 1800ms for a
//      semitone / fifth / octave / three octaves. Same probe, opposite expected result, which is what
//      makes it a real test of the switch rather than of the ramp.
//
// ── HOW TO RUN IT ────────────────────────────────────────────────────────────────────────────────
//   node tools/play.js glideprobe script /dev/null --headless --frames 1460 --wav /tmp/g.wav
//   node tools/wav-envelope.js /tmp/g.wav 25 --from 0.9  --to 3.0     # semitone leg
//   node tools/wav-envelope.js /tmp/g.wav 25 --from 6.9  --to 9.0     # fifth
//   node tools/wav-envelope.js /tmp/g.wav 25 --from 12.9 --to 15.0    # octave
//   node tools/wav-envelope.js /tmp/g.wav 25 --from 18.9 --to 21.0    # three octaves
//
// Read the CENTROID column: on a sine it tracks pitch closely enough, and the glide is done at the
// window where it stops changing. Compare that against GLIDE_MS after the leg's start time.
// For the per-octave run, set PROBE_PER_OCT to 1 and expect the four legs to differ 1:7:12:36.
//
// TIMING TRAP (same one retrigprobe documents): an event at frame N happens at t=(N-1)/60, so the
// legs start at 0.983s / 6.983s / 12.983s / 18.983s, NOT on the round seconds. Read them off the frames.
//
// MEASURING FLOOR, worth knowing before you call a leg asymmetric: `wav-envelope` reports integer Hz,
// and a semitone above C4 is only 15.6 Hz of total travel — so the last 5% of that leg is 0.8 Hz and
// the centroid reads "arrived" early. The semitone leg cannot be timed this way in either mode. The
// bigger three can.
//
// WHY SINE: autocorrelation and the centroid both have nothing to trip on, and the envelope must not
// colour the pitch reading. Same reason tune-check uses it as its control.

#define SLOT 5
#define BASE 60          // C4
#define GLIDE_MS 600     // one setting for every leg — that is the point

// PROBE_PER_OCT switches the GLIDE SCALE axis under test (see the second recipe in the header).
#ifndef PROBE_PER_OCT
#define PROBE_PER_OCT 0
#endif

// each leg: the note to glide TO, and a label. Up then back down, so the mirror check is free.
static const int LEG_NOTE[4] = { 61, 67, 72, 96 };            // +1, +7, +12, +36 semitones
static const char *LEG_NAME[4] = { "semitone", "fifth", "octave", "3 octaves" };

// Legs are 180 frames (3.0s) apart so the widest PER-OCTAVE leg still fits: three octaves at
// 600ms/oct is a 1.8s slide, which would not have finished inside the 1.5s spacing that was
// plenty for constant-time. One spacing serves both configurations.
#define LEG_UP(i) (60  + (i) * 360)
#define LEG_DN(i) (240 + (i) * 360)

static int h = 0;
static int f = 0;
static int leg = -1;

void init(void) {
    instrument(SLOT, INSTR_SINE, 2, 0, 7, 200);   // flat + sustaining
}

void update(void) {
    f++;
    if (f == 1) {
        h = note_on(BASE, SLOT, 6);
        note_glide(h, GLIDE_MS);
#if PROBE_PER_OCT
        note_glide_scale(h, GLIDE_PER_OCT);
#endif
    }
    for (int i = 0; i < 4; i++) {
        if (f == LEG_UP(i)) { note_pitch(h, (float)LEG_NOTE[i]); leg = i; }
        if (f == LEG_DN(i))   note_pitch(h, (float)BASE);
    }
    if (f == 1430) note_off(h);
}

void draw(void) {
    cls(CLR_BLACK);
    font(FONT_SMALL);
    print("glide probe (audit B1 / plan 3.26)", 8, 8, CLR_WHITE);
    print("not a demo - see the header for the recipe", 8, 20, CLR_DARK_GREY);
    print(str("note_glide(%d)   scale: %s", GLIDE_MS,
              PROBE_PER_OCT ? "GLIDE_PER_OCT" : "GLIDE_CONSTANT"), 8, 34, CLR_LIGHT_YELLOW);

    int y0 = 56, y1 = 148, x0 = 24, x1 = SCREEN_W - 8;
    line(x0, y1, x1, y1, CLR_DARKER_GREY);
    print("C4", 6, y1 - 3, CLR_DARK_GREY);
    print("C7", 6, y0 - 3, CLR_DARK_GREY);
    // one bracket per leg, drawn at its interval height so the four spans are visible at a glance
    for (int i = 0; i < 4; i++) {
        int xa = x0 + (x1 - x0) * LEG_UP(i) / 1460;
        int xb = x0 + (x1 - x0) * LEG_DN(i) / 1460;
        int semis = LEG_NOTE[i] - BASE;
        int y = y1 - (y1 - y0) * semis / 36;
        int c = (leg == i) ? CLR_LIGHT_YELLOW : CLR_DARK_GREY;
        line(xa, y1, xa, y, c);
        line(xa, y, xb, y, c);
        line(xb, y, xb, y1, c);
        print(LEG_NAME[i], xa + 2, y - 8, c);
        // the EXPECTED slide time for this leg — constant, or scaled by the interval in octaves
        int want = PROBE_PER_OCT ? (GLIDE_MS * semis) / 12 : GLIDE_MS;
        print(str("%dms", want), xa + 2, y + 2, c);
    }
    int px = x0 + (x1 - x0) * (f < 1460 ? f : 1460) / 1460;
    line(px, y0 - 4, px, y1 + 6, CLR_BLUE);
    print(str("frame %d   %s", f, PROBE_PER_OCT ? "legs should differ 1:7:12:36"
                                                : "every leg should take the same time"),
          8, SCREEN_H - 12, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
