/* de:meta
{
  "slug": "rottext",
  "title": "rotated text (playground)",
  "status": "active",
  "created": "2026-06-12",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "description": "A throwaway sandbox for the experimental print_rot() — does the bitmap font hold up rotated? A fan of the same word radiates from one anchor at 30-degree steps (so you can see the pivot sits at the start of the text), one big word you drag through every angle live, and a column of fixed 0/45/90/135-degree samples for a crispness check (right angles stay pixel-crisp; odd angles go softly GPU-sampled). LEFT/RIGHT nudge the angle, SPACE toggles auto-spin."
}
de:meta */
// rottext — print_rot playground. NOT a shipped cart; a throwaway to eyeball
// how the bitmap font looks rotated at every angle before we commit the API.
//   ← / →  nudge the live angle    SPACE  spin    Z  pivot    X  font
#include "studio.h"
#include <stdio.h>

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

STATE {
    float angle;     // the live, user-driven angle
    int   spinning;
    int   pivot;     // 0 = top-left (x,y)   1 = text center
    int   fi;        // font index into FONTS[]
};

static const int   FONTS[]  = { FONT_NORMAL, FONT_SMALL, FONT_TINY, FONT_COMIC,    FONT_THIN };
static const char *FNAMES[] = { "NORMAL 8x8", "SMALL 4x6", "TINY 3x5", "COMIC 10x20", "THIN CGA 8x8" };
#define NFONTS 5

void init(void) {
    S->fi = 3;   // show off the comic font by default; X cycles through them all
}

void update(void) {
    if (S->spinning) S->angle += 1.5f;
    if (btn(0, BTN_LEFT))  S->angle -= 2.0f;
    if (btn(0, BTN_RIGHT)) S->angle += 2.0f;
    if (keyp(KEY_SPACE))   S->spinning = !S->spinning;
    if (btnp(0, BTN_A))    S->pivot = !S->pivot;            // Z
    if (btnp(0, BTN_B))    S->fi = (S->fi + 1) % NFONTS;    // X
    if (S->angle >= 360) S->angle -= 360;
    if (S->angle < 0)    S->angle += 360;
}

void draw(void) {
    int p = S->pivot;
    cls(CLR_DARK_BLUE);

    // the headline: the CURRENT font's name rendered IN that font, centered up
    // top — you read the name and see the glyphs at once. X cycles all 5 fonts.
    font(FONTS[S->fi]);
    print_centered(FNAMES[S->fi], SCREEN_W / 2, 4, CLR_YELLOW);

    // chrome stays in the default 8x8 so labels don't move as we toggle
    font(FONT_NORMAL);
    print("print_rot  (X cycles font)", 6, 30, CLR_WHITE);
    print(p ? "pivot: CENTER (z)" : "pivot: top-left (z)", 6, 41, CLR_ORANGE);
    print("< > angle  space spin", 6, SCREEN_H - 12, CLR_INDIGO);

    // the rotated samples use whichever font we're auditing
    font(FONTS[S->fi]);

    // 1) a fan: the same word from one anchor at fixed 30-deg steps. with the
    //    top-left pivot words radiate outward; with center they pinwheel in place
    int cx = SCREEN_W / 2, cy = SCREEN_H / 2 - 6;
    pset(cx, cy, CLR_RED);
    for (int a = 0; a < 360; a += 30) {
        int col = CLR_DARK_GREY + (a / 30) % 8;
        print_rot("dream", cx, cy, (float)a, col, p);
    }

    // 2) the live one, big and bright — drag it through every angle. the red dot
    //    marks the anchor (x,y) so you can see what each pivot rotates around
    int lx = 60, ly = 50;
    pset(lx, ly, CLR_RED);
    print_rot("ROTATE ME", lx, ly, S->angle, CLR_YELLOW, p);

    // 3) crispness check: fixed angles 0/45/90/135 in a column, labelled
    int sx = SCREEN_W - 70, sy = 36;
    int fixed[4] = { 0, 45, 90, 135 };
    for (int i = 0; i < 4; i++) {
        char lbl[8];
        snprintf(lbl, sizeof lbl, "%d", fixed[i]);
        print(lbl, sx - 18, sy + i * 28, CLR_LIGHT_GREY);
        print_rot("abc", sx, sy + i * 28, (float)fixed[i], CLR_GREEN, p);
    }

    char buf[24];
    snprintf(buf, sizeof buf, "angle %d", (int)S->angle);
    font(FONT_NORMAL);
    print(buf, 6, 52, CLR_PINK);
}
