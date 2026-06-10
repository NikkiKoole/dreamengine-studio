// uiloupe.c — the ui.h loupe, on a real ui.h widget panel.
//
// Proves the loupe is FREE for any ui.h cart: a deliberately cramped synth
// panel (tiny 6px knobs + thin faders + a small step row), and the ONLY loupe
// code is `ui_loupe(1)`. Drag the corner ⊕ handle and a 3x lens appears above
// your finger, magnifying whatever widgets it's over; a second finger edits the
// fat versions inside. Grab a knob in the lens and drag out — it keeps turning,
// no jump (the re-anchor lives in ui.h now). Tap empty space to dismiss.
//
// Companion to the standalone `loupe` POC (which hand-rolls everything). This is
// the integrated version. See docs/design/loupe-notes.md.

#define UI_KNOB_R 6        // deliberately tiny — the fat-finger problem
#define UI_SLIDER_H 6
#include "studio.h"
#include "ui.h"

#define NKNOB 6
#define NFADER 4
#define NSTEP 16

static float knob[NKNOB]   = { 0.5f, 0.3f, 0.7f, 0.4f, 0.6f, 0.5f };
static float fader[NFADER] = { 0.8f, 0.5f, 0.6f, 0.3f };
static int   step[NSTEP];

static const char *knobName[NKNOB]   = { "CUT", "RES", "ENV", "DEC", "TUN", "VOL" };

void update(void) {
    static int started = 0;
    if (!started) {                      // seed a live-looking state + park the lens
        started = 1;
        for (int s = 0; s < NSTEP; s += 4) step[s] = 1;
        ui_loupe_at(60, 58);             // over the first couple of knobs, for the thumbnail
    }
}

void draw(void) {
    cls(CLR_DARKER_PURPLE);
    ui_begin();
    ui_loupe(1);                         // <-- the entire loupe integration

    print("UI.H LOUPE \x1b cramped synth panel", 6, 6, CLR_WHITE);
    print("drag the corner box; 2nd finger edits", 6, 16, CLR_MEDIUM_GREY);

    // row of tiny knobs
    for (int k = 0; k < NKNOB; k++)
        ui_knob(&knob[k], 36 + k * 44, 58, knobName[k]);

    // a cluster of thin faders
    for (int f = 0; f < NFADER; f++)
        ui_slider(&fader[f], 36, 96 + f * 12, 90, 0);

    // a 16-step row of small buttons (tapp-free: these are ui_button toggles)
    for (int s = 0; s < NSTEP; s++) {
        int bx = 150 + s * 10, by = 96;
        if (ui_button(bx, by, 8, 8, 0)) step[s] = !step[s];
        if (step[s]) rectfill(bx + 2, by + 2, 4, 4, CLR_ORANGE);
    }

#ifdef DE_TRACE
    watch("k0", "%.3f", knob[0]);
    watch("k1", "%.3f", knob[1]);
#endif

    ui_end();
}
