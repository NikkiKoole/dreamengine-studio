/* de:meta
{
  "title": "24. functions, arrays, structs",
  "status": "active",
  "created": "2026-07-01",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "lineage": "Track A of tutorial-curriculum.md (#25-32, 'how code actually works'), compressed from 3 planned carts into paged sections of one; sibling of 23-basics",
  "description": {
    "summary": "Writing your own functions, a fixed array you loop over, and a struct that bundles fields into one value.",
    "controls": "left/right change page; up/down adjust; A/B move the cursor on pages 2 and 3"
  }
}
de:meta */
#include "studio.h"

// 24. FUNCTIONS, ARRAYS, STRUCTS — Track A of tutorial-curriculum.md, compressed.
// Sibling of 23-basics: same paging scheme (<- -> flips the lesson).

#define NPAGES 3
static int page = 0;

// ---- page 0: functions ----
static int val = 5;   // up/down adjusts this

int square(int x) { return x * x; }

int iclamp(int x, int lo, int hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

void update_fns() {
    if (btnp(0, BTN_UP))   val = min(val + 1, 15);
    if (btnp(0, BTN_DOWN)) val = max(val - 1, -5);
}

void draw_fns() {
    print("a FUNCTION packages code you call.", 4, 44, CLR_LIGHT_GREY);
    print("PARAMETERS go in, a RETURN comes out.", 4, 54, CLR_LIGHT_GREY);

    print("int square(int x) { return x*x; }", 20, 70, CLR_WHITE);
    print(str("x = %d  (up/down to change)", val), 20, 84, CLR_WHITE);
    print(str("square(x) = %d", square(val)), 20, 96, CLR_YELLOW);

    print("int iclamp(x, lo, hi)", 20, 114, CLR_WHITE);
    print("-> lo if x<lo, hi if x>hi, else x", 20, 124, CLR_DARK_GREY);
    print(str("iclamp(x, 0, 10) = %d", iclamp(val, 0, 10)), 20, 138, CLR_GREEN);

    // a little track: raw x (may spill outside 0..10) vs the clamped result
    int lo_px = 20 + (0  - (-5)) * 10;
    int hi_px = 20 + (10 - (-5)) * 10;
    line(lo_px, 150, lo_px, 172, CLR_DARKER_GREY);
    line(hi_px, 150, hi_px, 172, CLR_DARKER_GREY);
    int raw_px = 20 + (val - (-5)) * 10;
    bool outside = (val < 0 || val > 10);
    circfill(raw_px, 155, 4, outside ? CLR_RED : CLR_WHITE);
    int clamp_px = 20 + (iclamp(val, 0, 10) - (-5)) * 10;
    circfill(clamp_px, 167, 4, CLR_GREEN);
    print("ticks mark iclamp(x,0,10) bounds", 20, 182, CLR_DARK_GREY);
}

// ---- page 1: arrays ----
#define NSLOTS 8
static int arr[NSLOTS] = { 3, 7, 2, 9, 4, 1, 6, 5 };
static int sel = 0;   // A/B moves this

void update_arrays() {
    if (btnp(0, BTN_A)) sel = max(sel - 1, 0);
    if (btnp(0, BTN_B)) sel = min(sel + 1, NSLOTS - 1);
    if (btnp(0, BTN_UP))   arr[sel] = min(arr[sel] + 1, 9);
    if (btnp(0, BTN_DOWN)) arr[sel] = max(arr[sel] - 1, 0);
}

void draw_arrays() {
    print("an ARRAY is a fixed bank of slots --", 4, 44, CLR_LIGHT_GREY);
    print("one loop can visit every one of them.", 4, 54, CLR_LIGHT_GREY);

    print("int things[8] = { 3,7,2,9,4,1,6,5 };", 20, 70, CLR_WHITE);
    print("for (i = 0; i < 8; i++) draw(things[i]);", 20, 82, CLR_WHITE);
    print("A/B pick a slot, up/down change it.", 4, 96, CLR_LIGHT_GREY);
    print(str("things[%d] = %d", sel, arr[sel]), 4, 106, CLR_LIGHT_GREY);

    for (int i = 0; i < NSLOTS; i++) {
        int x = 24 + i * 34;
        int h = arr[i] * 8;
        int col = (i == sel) ? CLR_YELLOW : CLR_BLUE_GREEN;
        rectfill(x, 190 - h, 24, h, col);
        if (i == sel) rect(x - 2, 190 - h - 2, 28, h + 4, CLR_WHITE);
    }
}

// ---- page 2: structs ----
typedef struct { int x, y, hp; } Thing;
static Thing t = { 160, 150, 5 };   // A hits it, B heals it, up/down moves it

void update_structs() {
    if (btnp(0, BTN_A)) t.hp = max(t.hp - 1, 0);
    if (btnp(0, BTN_B)) t.hp = min(t.hp + 1, 5);
    if (btnp(0, BTN_UP))   t.y = max(t.y - 4, 130);
    if (btnp(0, BTN_DOWN)) t.y = min(t.y + 4, 190);
}

void draw_structs() {
    print("a STRUCT bundles fields into ONE value.", 4, 44, CLR_LIGHT_GREY);
    print("typedef struct { int x,y,hp; } Thing;", 4, 54, CLR_LIGHT_GREY);
    print("Thing t = { 160, 150, 5 };", 20, 70, CLR_WHITE);
    print(str("t.x=%d  t.y=%d  t.hp=%d", t.x, t.y, t.hp), 20, 82, CLR_WHITE);
    print("A hits it, B heals it, up/down moves it", 20, 94, CLR_DARK_GREY);

    print("hp:", 220, 82, CLR_DARK_GREY);
    for (int i = 0; i < 5; i++) {
        int col = (i < t.hp) ? CLR_RED : CLR_DARKER_GREY;
        rectfill(244 + i * 10, 80, 8, 6, col);
    }

    circfill(t.x, t.y, 12, t.hp > 0 ? CLR_ORANGE : CLR_DARK_GREY);
}

void update() {
    if (btnp(0, BTN_RIGHT)) page = min(page + 1, NPAGES - 1);
    if (btnp(0, BTN_LEFT))  page = max(page - 1, 0);

    switch (page) {
        case 0: update_fns();     break;
        case 1: update_arrays();  break;
        case 2: update_structs(); break;
    }
}

void draw() {
    cls(CLR_DARKER_PURPLE);
    print("functions, arrays, structs.", 4, 4, CLR_WHITE);
    print(str("page %d/%d  (arrows: page / adjust)", page + 1, NPAGES), 4, 14, CLR_MEDIUM_GREY);
    line(4, 24, SCREEN_W - 4, 24, CLR_DARKER_GREY);

    switch (page) {
        case 0: draw_fns();     break;
        case 1: draw_arrays();  break;
        case 2: draw_structs(); break;
    }
}
