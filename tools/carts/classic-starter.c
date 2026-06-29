/* de:meta
{
  "title": "classic starter (v1 default)",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "description": "The original cart the editor used to greet you with, kept for posterity. A little explorer strolls a starfield world twice as wide as the screen — the camera follows, a tile floor scrolls past, a clipped minimap tracks you in the corner, and a 2× sspr() portrait sits by the help line. Backed by a jazzy vi-IV-I-V chord loop with a brushed hihat. Shows off camera/follow, map(), sprf() flipping, pget() glow checks, clip(), sspr(), and the timing/chord sound API. WASD move, Z melody note, X strum."
}
de:meta */
#include "studio.h"

#define WORLD_W (SCREEN_W * 2)

int x    = SCREEN_W;
int y    = 92;
int face = 1;           // 1 = right, -1 = left  (set by sgn())

void init() {
    enable_pget(true);
    // build a tile floor (draw something in sprite slot 1 to see it)
    for (int cx = 0; cx < MAP_W; cx++) mset(cx, 11, 1);
}

void update() {
    bpm(85);

    // chord progression: Am7 → Fmaj7 → Cmaj7 → G7  (vi - IV - I - V in C)
    int roots[] = { 57, 53, 60, 55 };
    int types[] = { CHORD_MIN7, CHORD_MAJ7, CHORD_MAJ7, CHORD_DOM7 };
    int ci      = (beat() / 4) % 4;

    if (every(4)) strum(roots[ci], types[ci], INSTR_TRI, 3, 45);
    if (every(2)) note(roots[ci] - 12, INSTR_TRI, 4);

    // hihat: closed (short) every beat — except beat 3 of each bar, where it opens up
    if (every(1)) {
        if ((beat() % 4) == 2) hit(60, INSTR_NOISE, 3, 200);    // open hihat
        else                    hit(60, INSTR_NOISE, 2,  25);    // closed hihat
    }

    int melody[] = { 69, 72, 74, 76, 79 };
    if (every(1) && chance(25)) note(melody[rnd(5)], INSTR_SINE, 2);

    int dx = 0, dy = 0;
    if (btn(0, BTN_RIGHT)) dx =  2;
    if (btn(0, BTN_LEFT))  dx = -2;
    if (btn(0, BTN_UP))    dy = -2;
    if (btn(0, BTN_DOWN))  dy =  2;
    x += dx;
    y += dy;
    if (dx != 0) face = sgn(dx);              // remember facing direction

    // mid() clamps without 4 lines of if/else
    x = mid(0, x, WORLD_W - 16);
    y = mid(0, y, SCREEN_H - 16);

    if (btnp(0, BTN_A)) note(melody[rnd(5)], INSTR_TRI, 5);
    if (btnp(0, BTN_B)) strum(roots[ci], types[ci], INSTR_TRI, 5, 30);
}

void draw() {
    // camera follows player; world is twice as wide as the screen
    camera(mid(0, x - SCREEN_W/2, WORLD_W - SCREEN_W), 0);
    cls(CLR_BLUE);

    // starfield in world coords — camera makes it scroll past
    for (int i = 0; i < 40; i++) {
        pset((i * 73) % WORLD_W, (i * 41) % SCREEN_H, CLR_WHITE);
    }

    // draw the map — skips empty cells, respects camera()
    map(0, 0, 0, 0, MAP_W, MAP_H);

    // bob with now(), flip with sprf()
    int bob = ((int)(now() * 4)) % 2 ? 0 : -1;
    sprf(0, x, y + bob, face < 0, false);

    // pget(): if a star was just under the player last frame, draw a glow.
    // pget reads the previous frame's canvas — great for after-the-fact checks.
    if (pget(x + 8, y + 17) == CLR_WHITE) {
        circ(x + 8, y + 8, 12, CLR_WHITE);
    }

    // HUD in screen space — reset camera before drawing UI
    camera(0, 0);

    // minimap (clip): restrict drawing to a 60×24 box in the top-right
    int mm_x = SCREEN_W - 64, mm_y = 4, mm_w = 60, mm_h = 24;
    clip(mm_x, mm_y, mm_w, mm_h);
    rectfill(mm_x, mm_y, mm_w, mm_h, CLR_BLACK);
    rect    (mm_x, mm_y, mm_w, mm_h, CLR_WHITE);
    pset(mm_x + (x * mm_w) / WORLD_W, mm_y + (y * mm_h) / SCREEN_H, CLR_RED);
    clip(0, 0, 0, 0);   // disable

    // sspr(): draw a 2x-sized portrait of sprite 0 next to the help text
    sspr(0, 0, 16, 16, 8, 176, 24, 24);

    int r = 6 - (int)(beat_pos() * 4.0f);
    circfill(SCREEN_W - 12, SCREEN_H - 12, mid(2, r, 6), CLR_WHITE);
    print("wasd move   z melody   x strum", 40, 184, CLR_WHITE);
}
