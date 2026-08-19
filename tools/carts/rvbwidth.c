/* de:meta
{
  "slug": "rvbwidth",
  "title": "reverb width probe",
  "status": "active",
  "created": "2026-08-19",
  "kind": [
    "probe"
  ],
  "teaches": [
    "positional-audio"
  ],
  "lineage": "docs/design/analog-outboard-chain.md asks whether a STEREO reverb tank is worth new DSP. The claim 'the tank is mono' is easy to state and hard to price, because nothing in the repo measures what a mono send COSTS a stereo mix. This probe makes that a number: two voices panned hard apart, one key that raises the reverb send, and tools/stereo-check.js reads the width before and after.",
  "description": {
    "summary": "A measurement probe, not a toy: two plucks panned hard left and hard right, and one key that raises the shared reverb send. Renders it so stereo-check.js can price what the engine's MONO reverb tank costs a stereo image.",
    "detail": "Slot 5 is panned hard left, slot 6 hard right, and they alternate on the beat, which gives a wide dry image (measurable as side/mid RMS). Press 1 to raise the reverb send on both. Because there is exactly one shared MONO tank, the wet copy of a hard-left voice arrives dead centre, so raising the send does not widen the tail, it narrows the whole mix: the wet is common-mode signal added to both channels. Two headless renders (send 0 vs send up) diffed with stereo-check.js turn 'the tank is mono' into a width number. The probe also prices the CPU: at send 0 the tank is dormant and costs literally nothing (the `used` flag skips it), so timing the two renders isolates reverb_process().",
    "controls": "1 raises the reverb send, 2 drops it back to dry; the voices play themselves"
  }
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>   // snprintf

// REVERB WIDTH PROBE — how much stereo image does the one shared MONO reverb tank cost?
//
// The engine's reverb is a SEND to a single mono reverberator (runtime/sound.h: "Mono in v1"), so
// the wet copy of a hard-panned voice comes back in the CENTRE. Two hard-panned voices therefore
// lose image as the send rises, instead of gaining a wide tail. This probe renders that so
// tools/stereo-check.js can put a number on it.

#define SL_L 5
#define SL_R 6

static float send = 0.0f;
static int   last_beat = -1;
static int   which = 0;
static int   mode = 0;      // 0 = master send (mono tank), 3 = tank 1 + FX_CHORUS after it

void init(void) {
    bpm(96);
    instrument(SL_L, INSTR_PLUCK, 1, 400, 3, 300);
    instrument(SL_R, INSTR_PLUCK, 1, 400, 3, 300);
    instrument_pan(SL_L, -1.0f);          // hard left
    instrument_pan(SL_R,  1.0f);          // hard right
    reverb(0.72f, 0.30f);                 // a plate-ish size, the same voicing outboard.h uses
    instrument_reverb(SL_L, 0.0f);
    instrument_reverb(SL_R, 0.0f);
}

void update(void) {
    if (keyp('1') && send == 0.0f) {      // SET-AND-HOLD: only on the change
        send = 0.8f;
        instrument_reverb(SL_L, send);
        instrument_reverb(SL_R, send);
    }
    if (keyp('2') && send != 0.0f) {
        send = 0.0f;
        instrument_reverb(SL_L, send);
        instrument_reverb(SL_R, send);
    }
    // MODE 3 — the CART-LAND recipe for a wide tail with no engine change: route the sends into
    // tank 1 (a real aux bus whose chain starts with FX_REVERB) and put a CHORUS after the reverb.
    // chorus_process reads the bus mono but writes ANTIPHASE taps to L and R, so the tail comes
    // back decorrelated even though the tank itself is mono.
    // MODE 5 — the ENGINE's own plate voicing (reverb_plate + reverb_plate_width): two pickups on
    // one steel sheet, so the wet arrives already decorrelated with no chorus in the path.
    if (keyp('5') && mode == 0) {
        mode = 5;
        send = 0.8f;
        reverb_plate(1.0f);
        reverb_plate_width(1.0f);
        instrument_reverb(SL_L, send);
        instrument_reverb(SL_R, send);
    }
    if ((keyp('3') || keyp('4')) && mode == 0) {
        int subtle = keyp('4');
        mode = subtle ? 4 : 3;
        send = 0.8f;
        reverb_bus(1, 0.72f, 0.30f);
        if (subtle) reverb_bus_fx(1, FX_CHORUS, 0.12f, 0.18f, 0.5f);   // barely-there wobble
        else        reverb_bus_fx(1, FX_CHORUS, 0.45f, 0.55f, 1.0f);   // rate, depth, mix
        instrument_reverb_bus(SL_L, 1, 1.0f);
        instrument_reverb_bus(SL_R, 1, 1.0f);
        instrument_reverb(SL_L, send);
        instrument_reverb(SL_R, send);
    }
    if (beat() != last_beat) {
        last_beat = beat();
        which = !which;
        hit(which ? 64 : 57, which ? SL_R : SL_L, 6, 400);
    }
#ifdef DE_TRACE
    watch("send", "%.2f", send);
    watch("mode", "%d", mode);
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    print("REVERB WIDTH PROBE", 8, 8, CLR_WHITE);
    print("slot 5 = hard LEFT, slot 6 = hard RIGHT", 8, 24, CLR_LIGHT_GREY);
    char buf[48];
    snprintf(buf, sizeof buf, "reverb send  %.2f", send);
    print(buf, 8, 40, send > 0.0f ? CLR_LIME_GREEN : CLR_DARK_GREY);
    print("1 = mono send   2 = dry", 8, 56, CLR_MEDIUM_GREY);
    print("3 = tank1+chorus  4 = subtle", 8, 66, mode ? CLR_LIME_GREEN : CLR_MEDIUM_GREY);
    print("5 = reverb_plate (two pickups)", 8, 74, mode == 5 ? CLR_LIME_GREEN : CLR_MEDIUM_GREY);
    print("mono wet NARROWS a stereo image;", 8, 84, CLR_DARK_GREY);
    print("a chorus after the tank widens it", 8, 96, CLR_DARK_GREY);
    rectfill(8, 112, (int)(send * 120.0f), 8, CLR_LIME_GREEN);
    rect(7, 111, 122, 10, CLR_DARKER_GREY);
}
