/* de:meta
{
  "title": "ui.h loupe",
  "status": "active",
  "created": "2026-06-10",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "description": "The magnifier loupe, now built into ui.h — free for any widget cart. A deliberately cramped synth panel (tiny 6px knobs, thin faders, a 16-step button row) with ONE line of loupe code: ui_loupe(1). Drag the corner box and a 3x lens floats above your finger, magnifying whatever ui.h widgets it covers; a second finger edits the fat versions inside, while the lens blocks touches to the real tiny widgets underneath. Grab a knob in the lens and drag out — it keeps turning with no jump (ui.h re-anchors the knob at the lens edge; inside the lens a full sweep needs 3x travel, so it doubles as fine-control). Tap empty space to dismiss. The integrated companion to the standalone loupe POC. See docs/design/loupe-notes.md."
}
de:meta */
// uiloupe.c — the ui.h loupe, on a real ui.h widget panel.
//
// Proves the loupe is FREE for any ui.h cart: a deliberately cramped synth panel
// (tiny 6px knobs + thin faders + a 16-step button row) and the ONLY loupe code
// is `ui_loupe(1)`. Drag the corner ⊕ handle and a 3x lens appears above your
// finger, magnifying whatever it's over; a second finger edits the fat versions
// inside. Grab a knob in the lens and drag out — it keeps turning, no jump (the
// re-anchor lives in ui.h). Tap empty space to dismiss.
//
// The lens shows the WHOLE scene (checkerboard, labels, the orange step fills —
// everything, not just the widgets) because ui.h's loupe magnifies a COPY of the
// canvas with the engine's zoom_rect primitive. See docs/design/loupe-notes.md.

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

static const char *knobName[NKNOB] = { "CUT", "RES", "ENV", "DEC", "TUN", "VOL" };

void update(void) {
    static int started = 0;
    if (!started) {                      // seed a live-looking state + park the lens for the thumbnail
        started = 1;
        for (int s = 0; s < NSTEP; s += 4) step[s] = 1;
        ui_loupe_at(58, 58);
    }
}

void draw(void) {
    cls(CLR_DARKER_PURPLE);
    ui_begin();
    ui_loupe(1);                         // <-- the entire loupe integration

    // faint checkerboard so the magnification reads as live even over gaps
    for (int y = 28; y < SCREEN_H; y += 6)
        for (int x = 0; x < SCREEN_W; x += 6)
            if (((x / 6) ^ (y / 6)) & 1) rectfill(x, y, 6, 6, CLR_DARK_PURPLE);

    print("UI.H LOUPE \x1b cramped synth panel", 6, 6, CLR_WHITE);
    print("drag the corner box; 2nd finger edits", 6, 16, CLR_MEDIUM_GREY);

    for (int k = 0; k < NKNOB; k++)
        ui_knob(&knob[k], 36 + k * 44, 58, knobName[k]);

    for (int f = 0; f < NFADER; f++)
        ui_slider(&fader[f], 36, 96 + f * 12, 90, 0);

    for (int s = 0; s < NSTEP; s++) {
        int bx = 150 + s * 10, by = 96;
        if (ui_button(bx, by, 8, 8, 0)) step[s] = !step[s];
        if (step[s]) rectfill(bx + 2, by + 2, 4, 4, CLR_ORANGE);
    }

#ifdef DE_TRACE
    watch("k0", "%.3f", knob[0]);
    watch("k1", "%.3f", knob[1]);
#endif

    ui_end();                            // draws the lens (a magnified copy of the canvas) + handle
}
