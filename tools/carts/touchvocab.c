/* de:meta
{
  "title": "touch vocabulary",
  "status": "active",
  "created": "2026-07-01",
  "kind": [
    "probe"
  ],
  "teaches": [],
  "description": {
    "summary": "Reference cart for docs/design/touch-controls.md's Layout vocabulary table - cycles through every SHIPPED touch_layout() scheme over a mock game scene, so each row can be baked as one clean image (deck/rails/overlay previewed via DE_WINDOW).",
    "detail": "Unlike touchpad.c (a hand-mocked pixel-art impression that predates the real API), this cart calls the real touch_layout() for each row, so a bake is the actual engine widget, not an artist's guess. Cycle order matches the vocabulary table: single floating stick (0 buttons), analog + A/B (2), d-pad + A/B (2), d-pad8 + 3. Twin-stick and buttons-only aren't implemented yet (no second move slot, no TOUCH_NONE), so they're not in the cycle - see the doc's Open Questions. Run plain for OVERLAY (matched aspect, the default), DE_WINDOW=640x900 for DECK, or DE_WINDOW=900x400 for RAILS.",
    "controls": "SPACE = next scheme   ·   arrows move, Z/X/C/V = A/B/X/Y   ·   DE_WINDOW=WxH env var previews deck/rails   ·   DE_SCHEME=n picks the starting row (0-3), for scripted bakes"
  }
}
de:meta */
// touchvocab — bakes a reference image per named scheme in docs/design/touch-controls.md's
// "Layout vocabulary" table, using the real touch_layout() API (not a mockup). One process run
// per DE_WINDOW value gives OVERLAY / DECK / RAILS; SPACE cycles the scheme within a run.
#include "studio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *name; int mode; int buttons; } Scheme;

static const Scheme SCHEMES[] = {
    { "single floating stick", TOUCH_ANALOG, 0 },
    { "analog + A/B",          TOUCH_ANALOG, 2 },
    { "d-pad + A/B",           TOUCH_DPAD4,  2 },
    { "d-pad8 + 3",            TOUCH_DPAD8,  3 },
};
#define N_SCHEMES ((int)(sizeof(SCHEMES) / sizeof(SCHEMES[0])))

static int scheme = 0;
static float px, py;

static void apply_scheme(void) { touch_layout(SCHEMES[scheme].mode, SCHEMES[scheme].buttons); }

static const char *placement_name(int p) {
    switch (p) {
        case TOUCH_LAYOUT_DECK:  return "DECK";
        case TOUCH_LAYOUT_RAILS: return "RAILS";
        default:                  return "OVERLAY";
    }
}

void update(void) {
    static bool inited = false;
    if (!inited) {
        px = (float)(SCREEN_W / 2); py = (float)(SCREEN_H * 2 / 3 - 10);
        const char *s = getenv("DE_SCHEME");   // scripted-bake convenience: pick the starting row without SPACE-timing
        if (s) { int n = atoi(s); if (n >= 0 && n < N_SCHEMES) scheme = n; }
        apply_scheme(); inited = true;
    }

    if (keyp(' ')) { scheme = (scheme + 1) % N_SCHEMES; apply_scheme(); }

    float spd = 1.2f;
    if (btn(0, BTN_LEFT))  px -= spd;
    if (btn(0, BTN_RIGHT)) px += spd;
    if (btn(0, BTN_UP))    py -= spd;
    if (btn(0, BTN_DOWN))  py += spd;
    if (px < 4) px = 4; if (px > SCREEN_W - 4) px = SCREEN_W - 4;
    if (py < 4) py = 4; if (py > SCREEN_H - 4) py = SCREEN_H - 4;
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    int gh = SCREEN_H / 3;
    rectfill(0, SCREEN_H - gh, SCREEN_W, gh, CLR_DARK_GREEN);   // a horizon, so this reads as a "game"
    circfill((int)px, (int)py, 4, CLR_YELLOW);                  // a marker you can drive with the scheme

    font(FONT_COMIC); print("TOUCH VOCABULARY", 8, 5, CLR_WHITE);
    font(FONT_TINY);

    char row[64];
    snprintf(row, sizeof row, "%d/%d  %s  (%d buttons)", scheme + 1, N_SCHEMES,
             SCHEMES[scheme].name, SCHEMES[scheme].buttons);
    print(row, 8, 20, CLR_YELLOW);

    char place[64];
    snprintf(place, sizeof place, "placement=%s  ctrl_scale=%.2f",
             placement_name(touch_layout_mode()), touch_ctrl_scale());
    print(place, 8, 30, CLR_GREEN);

    print("SPACE = next scheme", 8, SCREEN_H - 10, CLR_DARK_GREY);
}
