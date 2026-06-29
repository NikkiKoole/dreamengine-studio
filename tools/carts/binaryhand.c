/* de:meta
{
  "title": "one-hand binary",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "description": "Count 0 to 31 on a single hand — each finger is a bit worth 16, 8, 4, 2, 1, so any combination is a number and 2^5 = 32 patterns. Exactly how computers count: just on/off. (Yes, 4 is the middle finger all by itself.) Z adds one, X takes one away."
}
de:meta */
#include "studio.h"

// ONE-HAND BINARY — count from 0 to 31 on a single hand.
//
// Each finger is one binary digit (a "bit"). A raised finger is a 1, a folded
// finger is a 0. Reading left to right the fingers are worth 16, 8, 4, 2, 1 —
// the powers of two — so any combination adds up to a number from 0 to 31.
// Five fingers, 2^5 = 32 possible patterns.
//
// This is exactly how computers count: not ten symbols like us, just on/off.
//
// (And yes — the number 4 is the middle finger all on its own. Computers have
//  no manners.)
//
//   Z / →   add one
//   X / ←   take one away

#define PALM_TOP 118
#define VAL_NONE 0

static int value = 4;   // start on the cheeky one

// the four upright fingers, left → right, with their bit value and length
static const int FX[4]   = { 120, 142, 164, 186 };  // x of each finger
static const int FLEN[4] = {  40,  54,  62,  52 };  // pinky, ring, middle, index
static const int FVAL[4] = {  16,   8,   4,   2 };  // place value of each

// draw one upright finger: full length if its bit is set, a folded stub if not
static void finger(int cx, bool up, int len) {
    int w = 16, h = up ? len : 16;
    int topY = PALM_TOP - h;
    rectfill(cx - w / 2, topY, w, h, CLR_LIGHT_PEACH);
    circfill(cx, topY, w / 2, CLR_LIGHT_PEACH);
    rect(cx - w / 2, topY, w, h, CLR_DARK_PEACH);
    if (up) rectfill(cx - 3, topY + 1, 6, 5, CLR_WHITE);  // a fingernail
}

static void thumb(bool up) {
    int by = 140, w = 16;
    if (up) {
        int x0 = 204, len = 32;
        rectfill(x0, by - w / 2, len, w, CLR_LIGHT_PEACH);
        circfill(x0 + len, by, w / 2, CLR_LIGHT_PEACH);
        rect(x0, by - w / 2, len, w, CLR_DARK_PEACH);
    } else {
        rectfill(200, by - 11, 16, 22, CLR_LIGHT_PEACH);
        circfill(216, by, 8, CLR_LIGHT_PEACH);
        rect(200, by - 11, 16, 22, CLR_DARK_PEACH);
    }
}

void update(void) {
    if (btnp(0, BTN_A) || btnp(0, BTN_RIGHT)) { value = (value + 1) % 32;  hit(72, INSTR_SQUARE, 2, 30); }
    if (btnp(0, BTN_B) || btnp(0, BTN_LEFT))  { value = (value + 31) % 32; hit(64, INSTR_SQUARE, 2, 30); }
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    print_centered("ONE-HAND BINARY", SCREEN_W/2, 4, CLR_WHITE);

    // place-value header — each power of two sits over its finger, lit when set
    int lblx[5] = { 120, 142, 164, 186, 218 };
    int lblv[5] = {  16,   8,   4,   2,   1 };
    for (int i = 0; i < 5; i++) {
        const char *s = str("%d", lblv[i]);
        int w = 0; for (const char *p = s; *p; p++) w += 8;
        print(s, lblx[i] - w / 2, 44, (value & lblv[i]) ? CLR_YELLOW : CLR_DARK_GREY);
    }

    // the hand
    rectfill(112, PALM_TOP, 96, 60, CLR_LIGHT_PEACH);
    circfill(120, PALM_TOP + 58, 8, CLR_LIGHT_PEACH);
    circfill(200, PALM_TOP + 58, 8, CLR_LIGHT_PEACH);
    rect(112, PALM_TOP, 96, 60, CLR_DARK_PEACH);
    for (int i = 0; i < 4; i++) finger(FX[i], value & FVAL[i], FLEN[i]);
    thumb(value & 1);

    // readout panel, top-left
    char bits[6];
    for (int i = 0; i < 5; i++) bits[i] = (value & (16 >> i)) ? '1' : '0';
    bits[5] = 0;
    print("VALUE", 6, 24, CLR_LIGHT_GREY);
    print(str("%d", value), 6, 34, CLR_YELLOW);
    print(bits, 6, 48, CLR_GREEN);

    if (value == 4) print_centered("...real mature.", SCREEN_W/2, 188, CLR_PINK);
    else            print_centered("Z +1    X -1    (0-31, one hand)", SCREEN_W/2, 188, CLR_LIGHT_GREY);
}
