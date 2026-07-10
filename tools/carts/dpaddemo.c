/* de:meta
{
  "slug": "dpaddemo",
  "title": "touch controls testbench",
  "status": "active",
  "created": "2026-07-01",
  "kind": [
    "probe"
  ],
  "teaches": [],
  "description": {
    "summary": "A dev test-bench for docs/design/touch-controls.md — every touch_layout() move mode, live btn() readout, and a touch_debug() overlay showing the band/grab-zone and placement.",
    "detail": "Press 1-4 to switch the on-screen control between floating analog, fixed analog, 4-way d-pad, and 8-way d-pad. Drag the control (mouse-as-touch on desktop) or use the keyboard to drive it; the read-out shows exactly what btn() sees. touch_debug(true) is always on here, drawing the control band outline (cyan) and the current move mode's grab zone (magenta) — the stick's rectangular zone, or the d-pad's local circular radius — plus the placement mode and touch_ctrl_scale() as text. Run with DE_WINDOW=640x900 to see DECK placement, or DE_WINDOW=900x400 to see RAILS (default window == game, so OVERLAY, is what you get without it).",
    "controls": "1/2/3/4 = analog / analog-fixed / dpad4 / dpad8   ·   drag the pad, or arrows + Z/X   ·   DE_WINDOW=WxH env var to preview deck/rails"
  }
}
de:meta */
// touch controls testbench — a permanent dev tool for docs/design/touch-controls.md, not a
// polished cart. Grows alongside the touch-controls work: every move mode, the touch_debug()
// overlay (band + grab-zone outlines), and the placement/scale readout, all in one place so
// engine changes here can be eyeballed without hunting for a phone or writing a throwaway probe.
#include "studio.h"
#include <stdio.h>

static int mode = TOUCH_DPAD4;

void update(void) {
    touch_debug(true);   // always on here — this cart exists to show the debug overlay
    static int last_mode = -1;
    if (keyp('1')) mode = TOUCH_ANALOG;
    if (keyp('2')) mode = TOUCH_ANALOG_FIX;
    if (keyp('3')) mode = TOUCH_DPAD4;
    if (keyp('4')) mode = TOUCH_DPAD8;
    if (mode != last_mode) { touch_layout(mode, 2); last_mode = mode; }
}

static const char *mode_name(int m) {
    switch (m) {
        case TOUCH_ANALOG:     return "1: TOUCH_ANALOG (floating stick)";
        case TOUCH_ANALOG_FIX: return "2: TOUCH_ANALOG_FIX (fixed stick)";
        case TOUCH_DPAD4:      return "3: TOUCH_DPAD4 (4-way, no diagonals)";
        default:                return "4: TOUCH_DPAD8 (8-way, diagonals)";
    }
}

static const char *placement_name(int p) {
    switch (p) {
        case TOUCH_LAYOUT_DECK:  return "DECK";
        case TOUCH_LAYOUT_RAILS: return "RAILS";
        default:                  return "OVERLAY";
    }
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    font(FONT_COMIC); print("TOUCH CONTROLS TESTBENCH", 8, 5, CLR_WHITE);
    font(FONT_TINY);
    print("press 1 2 3 4 to switch move mode", 8, 22, CLR_INDIGO);
    print(mode_name(mode), 8, 34, CLR_YELLOW);

    int u = btn(0, BTN_UP), d = btn(0, BTN_DOWN), l = btn(0, BTN_LEFT), r = btn(0, BTN_RIGHT);
    int a = btn(0, BTN_A), b = btn(0, BTN_B);
    char state[64];
    snprintf(state, sizeof state, "up=%d down=%d left=%d right=%d   A=%d B=%d", u, d, l, r, a, b);
    print(state, 8, 50, CLR_LIGHT_GREY);

    char place[64];
    snprintf(place, sizeof place, "placement=%s  ctrl_scale=%.2f", placement_name(touch_layout_mode()), touch_ctrl_scale());
    print(place, 8, 62, CLR_GREEN);

    print("DE_WINDOW=640x900 -> DECK   DE_WINDOW=900x400 -> RAILS", 8, SCREEN_H - 26, CLR_DARK_GREY);
    print("cyan=control band  magenta=grab zone (touch_debug overlay)", 8, SCREEN_H - 18, CLR_DARK_GREY);
    print("drag the pad with the mouse, or use arrows + Z/X", 8, SCREEN_H - 10, CLR_DARK_GREY);
}
