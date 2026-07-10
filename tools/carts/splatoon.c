/* de:meta
{
  "slug": "splatoon",
  "title": "splatoon",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game",
    "tech-demo"
  ],
  "teaches": [
    "tilemap-collision"
  ],
  "lineage": "Homage to Nintendo Splatoon (2015); novel use of pget() to read live pixel ink color as a gameplay mechanic (ink type gates movement speed), with an owner grid tracking territory coverage for scoring.",
  "genre": "sandbox",
  "homage": "Splatoon (2015)",
  "description": "2-player ink battle showing off pset/pget. Every shot paints pixels onto the canvas (which the engine never clears), and you read the ink ahead with pget to swim fast through your own color and slog through the enemy's. Most floor covered in 40s wins. P1 blue: WASD move, Z shoot — P2 orange: arrows move, , shoot."
}
de:meta */
#include "studio.h"

// SPLAT ARENA — cover the most floor in ink before time runs out.
//
// Shows off pset / pget + the fact that the engine never auto-clears the
// canvas. Every shot paints pixels that STAY (we never cls), so the floor is
// a real paint layer. You read the ink right in front of you with pget() to
// swim fast through your own color and slog through the enemy's.
//
// player 1 (blue):   WASD move   Z shoot
// player 2 (orange): arrows move , shoot
// most coverage after 40s wins — Z / , to replay

#define HUD_H        14
#define CELL          4
#define GW           (SCREEN_W / CELL)        // 80 ownership columns
#define GH           (SCREEN_H / CELL)        // 50 ownership rows
#define ROW_TOP      (HUD_H / CELL + 1)       // first paintable row (skip the HUD)
#define MATCH_SEC    40

#define NEUTRAL      CLR_LIGHT_GREY
#define C1_INK       CLR_BLUE
#define C2_INK       CLR_ORANGE

typedef struct { float x, y; int owner, r; } Blob;

static unsigned char owner[GW * GH];           // 0 neutral, 1 blue, 2 orange
static Blob   queue[512];                       // paint to render this frame
static int    qn;

typedef struct {
    float x, y;        // center, pixels
    int   fdx, fdy;    // facing = last move direction (for aiming shots)
    int   px, py;      // last-drawn center, so we can erase the old squid
} Player;

static Player pl[2];
static bool   started;       // false until the first update seeds the arena
static bool   floor_inited;  // draw clears the canvas exactly once
static bool   over;
static float  t_end;         // now() at which the match ends
static int    cover[3];      // [1] = blue cells, [2] = orange cells

static int ink_color(int who) { return who == 1 ? C1_INK : C2_INK; }

// stamp ownership into the grid + queue a blob for draw() to render
static void add_paint(float x, float y, int who, int r) {
    int cx = (int)x / CELL, cy = (int)y / CELL, cr = r / CELL + 1;
    for (int gy = cy - cr; gy <= cy + cr; gy++)
        for (int gx = cx - cr; gx <= cx + cr; gx++) {
            if (gx < 0 || gx >= GW || gy < ROW_TOP || gy >= GH) continue;
            int ddx = gx * CELL + CELL / 2 - (int)x;
            int ddy = gy * CELL + CELL / 2 - (int)y;
            if (ddx * ddx + ddy * ddy <= r * r) owner[gx + gy * GW] = who;
        }
    if (qn < 512) queue[qn++] = (Blob){ x, y, who, r };
}

static void count_cover(void) {
    cover[1] = cover[2] = 0;
    for (int i = 0; i < GW * GH; i++) {
        if      (owner[i] == 1) cover[1]++;
        else if (owner[i] == 2) cover[2]++;
    }
}

static void reset(void) {
    for (int i = 0; i < GW * GH; i++) owner[i] = 0;
    qn = 0; started = false; floor_inited = false; over = false;
    cover[1] = cover[2] = 0;
    pl[0] = (Player){ 70,            110,  1, 0, 70,            110 };
    pl[1] = (Player){ SCREEN_W - 70, 110, -1, 0, SCREEN_W - 70, 110 };
}

void init(void) { enable_pget(true); reset(); }

void update(void) {
    if (over) { if (btnp(0, BTN_A) || btnp(1, BTN_A)) reset(); return; }

    qn = 0;
    if (!started) {
        started = true;
        t_end = now() + MATCH_SEC;
        for (int k = 0; k < 70; k++)               // splatter the arena so it looks alive
            add_paint(rnd_between(10, SCREEN_W - 10),
                      rnd_between(HUD_H + 10, SCREEN_H - 10), 1 + (k & 1), rnd_between(4, 9));
    }

    for (int i = 0; i < 2; i++) {
        Player *p = &pl[i];
        int mx = btn(i, BTN_RIGHT) - btn(i, BTN_LEFT);
        int my = btn(i, BTN_DOWN)  - btn(i, BTN_UP);
        if (mx || my) { p->fdx = mx; p->fdy = my; }

        // swim speed: read the ink ahead of us with pget
        float spd = 1.7f;
        if (mx || my) {
            int c   = pget((int)p->x + mx * 7, (int)p->y + my * 7);
            int me  = ink_color(i + 1);
            int foe = ink_color(2 - i);
            if      (c == me)  spd = 2.7f;          // swim fast in your own ink
            else if (c == foe) spd = 0.9f;          // slog through theirs
        }
        p->x = clamp(p->x + mx * spd, 6, SCREEN_W - 6);
        p->y = clamp(p->y + my * spd, HUD_H + 6, SCREEN_H - 6);

        if (mx || my) add_paint(p->x, p->y, i + 1, 5);   // roller trail

        if (btn(i, BTN_A)) {                              // spray ink forward
            for (int s = 1; s <= 4; s++)
                add_paint(p->x + p->fdx * 6 * s + rnd(5) - 2,
                          p->y + p->fdy * 6 * s + rnd(5) - 2, i + 1, 4);
            if (frame() % 6 == 0) hit(40 + rnd(6), INSTR_NOISE, 2, 40);
        }
    }

    if (frame() % 8 == 0) count_cover();
    if (now() >= t_end) { over = true; count_cover(); strum(60, CHORD_MAJ7, INSTR_SQUARE, 5, 50); }
}

void draw(void) {
    if (!floor_inited) { cls(NEUTRAL); floor_inited = true; }

    // erase last frame's squids by repainting the floor underneath from the grid
    for (int i = 0; i < 2; i++) {
        Player *p = &pl[i];
        for (int yy = p->py - 9; yy <= p->py + 9; yy += CELL)
            for (int xx = p->px - 9; xx <= p->px + 9; xx += CELL) {
                int gx = xx / CELL, gy = yy / CELL;
                if (gx < 0 || gx >= GW || gy < 0 || gy >= GH) continue;
                int o = owner[gx + gy * GW];
                rectfill(gx * CELL, gy * CELL, CELL, CELL,
                         o == 1 ? C1_INK : o == 2 ? C2_INK : NEUTRAL);
            }
    }

    // render this frame's paint — circles + scattered pset droplets (inky splatter)
    for (int i = 0; i < qn; i++) {
        Blob *b = &queue[i];
        int col = ink_color(b->owner);
        circfill((int)b->x, (int)b->y, b->r, col);
        for (int s = 0; s < 4; s++) {
            float a = rnd(360), dr = b->r + rnd(4);
            int sx = (int)(b->x + cos_deg(a) * dr), sy = (int)(b->y + sin_deg(a) * dr);
            if (sx < 1 || sx >= SCREEN_W - 1 || sy <= HUD_H || sy >= SCREEN_H - 1) continue;
            pset(sx, sy, col);
            int gx = sx / CELL, gy = sy / CELL;
            if (gx >= 0 && gx < GW && gy >= ROW_TOP && gy < GH) owner[gx + gy * GW] = b->owner;
        }
    }
    qn = 0;

    // squids
    for (int i = 0; i < 2; i++) {
        Player *p = &pl[i];
        int ink  = ink_color(i + 1);
        int body = i == 0 ? CLR_DARK_BLUE : CLR_BROWN;
        circfill((int)p->x, (int)p->y, 5, body);
        circ((int)p->x, (int)p->y, 5, ink);
        line((int)p->x, (int)p->y, (int)p->x + p->fdx * 6, (int)p->y + p->fdy * 6, ink);
        pset((int)p->x - 2, (int)p->y - 1, CLR_WHITE);
        pset((int)p->x + 2, (int)p->y - 1, CLR_WHITE);
        p->px = (int)p->x; p->py = (int)p->y;
    }

    // HUD strip (redrawn every frame so it never smears the play area)
    rectfill(0, 0, SCREEN_W, HUD_H, CLR_BLACK);
    int total = GW * (GH - ROW_TOP);
    int p1 = cover[1] * 100 / total, p2 = cover[2] * 100 / total;
    int barw = 90;
    rectfill(8, 4, barw, 6, CLR_DARKER_GREY);
    rectfill(8, 4, barw * cover[1] / total, 6, C1_INK);
    rectfill(SCREEN_W - 8 - barw, 4, barw, 6, CLR_DARKER_GREY);
    int fr = barw * cover[2] / total;
    rectfill(SCREEN_W - 8 - fr, 4, fr, 6, C2_INK);
    print(str("%d%%", p1), 104, 4, C1_INK);
    print(str("%d%%", p2), 184, 4, C2_INK);

    int secs = (int)(t_end - now());
    print_centered(str("%d", secs < 0 ? 0 : secs), SCREEN_W/2, 4, CLR_WHITE);

    if (over) {
        rectfill(70, 78, 180, 44, CLR_BLACK);
        rect(70, 78, 180, 44, CLR_WHITE);
        const char *msg = cover[1] > cover[2] ? "BLUE WINS!" :
                          cover[2] > cover[1] ? "ORANGE WINS!" : "TIE!";
        int c = cover[1] > cover[2] ? C1_INK : cover[2] > cover[1] ? C2_INK : CLR_WHITE;
        print_centered(msg, SCREEN_W/2, 86, c);
        print_centered(str("%d%%  vs  %d%%", p1, p2), SCREEN_W/2, 99, CLR_LIGHT_GREY);
        print_centered("Z / , to replay", SCREEN_W/2, 111, CLR_DARK_GREY);
    }
}
