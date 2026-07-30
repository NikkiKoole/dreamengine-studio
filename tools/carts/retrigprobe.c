/* de:meta
{
  "slug": "retrigprobe",
  "title": "retrig probe",
  "status": "hidden",
  "created": "2026-07-30",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "algorithm-visualization"
  ],
  "lineage": "Measurement probe for note_retrig (synth-secrets audit §B3/§K6) — the deterministic target the click-check and onset-transient numbers in audio-notes.md were taken from.",
  "description": "Measurement probe for note_retrig, not a playable cart (status hidden). Holds one note and re-articulates it three ways at known times so click-check.js and wav-envelope.js have something deterministic to point at."
}
de:meta */
#include "studio.h"

// PROBE for note_retrig — the acceptance evidence for audit §B3/§K6, not a demo.
// One held note; three articulations at known second boundaries so an oracle knows where to look:
//
//   f=60  (~0.98s)  note_retrig()      — the claim under test
//   f=120 (~1.98s)  note_off+note_on   — the OLD path, as a built-in positive control
//   f=180 (~2.98s)  note_off           — ring out
//
// (f counts update() calls, so f=60 lands at ~59/60s, i.e. in the 0.975s analysis window, NOT 1.000.
// That one-window offset already caused one wrong "the chiff never fired" reading — don't re-derive it.)
//
// ── THE TWO CLAIMS AND HOW TO RE-MEASURE THEM ────────────────────────────────────────────────────
//
// 1) A RETRIG DOES NOT CLICK.  Default build (INSTR_SINE, sustain 3/7):
//      node tools/play.js retrigprobe script /dev/null --headless --frames 260 --wav /tmp/r.wav
//      node tools/click-check.js /tmp/r.wav
//    Expect PASS. The retrig events land at ~1.5x the local step-rms — the same as the ordinary
//    note-on attack ramp, i.e. no more of a discontinuity than starting a note normally.
//    SINE matters: a saw's flyback is itself a big first-difference every cycle, so on SAW the plain
//    note-on reads 16-23x and a real splice would hide inside that. SINE is the clean control.
//    sustain 3/7 matters too: at sustain 1.0 the envelope rewind is a no-op and the test measures
//    nothing while still passing.
//
// 2) THE ONSET TRANSIENT RE-ARMS (§K6, the flute chiff).  Set PROBE_WAVE to INSTR_PIPE and define
//    PROBE_FLAT, then read brightness/centroid around 0.97s:
//      node tools/wav-envelope.js /tmp/r.wav 25 --from 0.9 --to 1.2
//    Measured: brightness 0.036 / centroid 4953 Hz at the retrig, against a 0.016-0.020 / ~3500 Hz
//    sustain baseline — a stronger chiff signature than the fresh note_on control's 0.031 / 4510 Hz.
//    PROBE_FLAT is what makes this conclusive: with no attack and full sustain the envelope rewind
//    is a provable no-op (level 1.0 before and after), so the ONLY thing left that can move the
//    spectrum is the onset transient.

#define SLOT 5

// PROBE_WAVE / PROBE_FLAT select which claim is under test — see the two recipes above.
#ifndef PROBE_WAVE
#define PROBE_WAVE INSTR_SINE
#endif

static int h = 0;
static int f = 0;

void init(void) {
#ifdef PROBE_FLAT
    instrument(SLOT, PROBE_WAVE, 0, 0, 7, 800);      // flat: isolates the onset transient (claim 2)
#else
    instrument(SLOT, PROBE_WAVE, 200, 300, 3, 800);  // 200ms attack, decay to sustain 3/7 (claim 1)
#endif
}

void update(void) {
    f++;
    if (f == 1)   h = note_on(60, SLOT, 6);
    if (f == 60)  note_retrig(h);                              // the claim under test
    if (f == 120) { note_off(h); h = note_on(60, SLOT, 6); }    // the old path, for comparison
    if (f == 180) note_off(h);
}

void draw(void) {
    cls(CLR_BLACK);
    font(FONT_SMALL);
    print("note_retrig measurement probe", 8, 8, CLR_WHITE);
    print("not a demo - see the header for the two recipes", 8, 20, CLR_DARK_GREY);
    // a bare timeline, so a stray click on it in the editor still explains itself
    const struct { int f; const char *label; } EV[3] = {
        { 60, "retrig" }, { 120, "off+on" }, { 180, "release" }
    };
    int y = 48;
    line(8, y, SCREEN_W - 8, y, CLR_DARKER_GREY);
    for (int i = 0; i < 3; i++) {
        int x = 8 + (SCREEN_W - 16) * EV[i].f / 240;
        bool past = f >= EV[i].f;
        line(x, y - 4, x, y + 4, past ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
        print(EV[i].label, x - 12, y + 8, past ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    }
    int px = 8 + (SCREEN_W - 16) * (f < 240 ? f : 240) / 240;
    line(px, y - 10, px, y + 20, CLR_BLUE);
    print(str("frame %d", f), 8, SCREEN_H - 12, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
