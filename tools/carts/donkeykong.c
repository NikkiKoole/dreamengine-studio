/* de:meta
{
  "slug": "donkeykong",
  "title": "Donkey Kong",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop",
    "no-sprite-vehicles"
  ],
  "lineage": "Donkey Kong (1981 arcade) — 25m girder stage rebuilt from primitives; sloped girder geometry and barrel-path simulation are the novel contribution.",
  "genre": "platformer",
  "homage": "Donkey Kong (1981)",
  "description": "The arcade 25m girder stage, rebuilt from pure primitives: a tower of slanted red girders strung together with peach ladders, Kong thumping barrels off the top, and the princess blinking HELP at the summit. Run up the slopes, climb the ladders, and time your jumps as barrels tumble down and randomly peel off onto a ladder — clear one for +100, or grab the blinking hammer for six seconds of barrel-smashing frenzy. Touch a barrel without the hammer and the screen flashes red and shakes as you lose one of three lives; reach the top and she's saved. Hi-score persists between runs. Left/Right run along the girder, Up/Down climb ladders, Z to jump (and to restart)."
}
de:meta */
#include "studio.h"
#include <stddef.h>

// ── DONKEY KONG — the 25m girder stage ────────────────────────
// Climb slanted girders and ladders to the princess at the top
// while Kong rolls barrels down. Jump barrels for points, or grab
// the hammer to smash them. Touch a barrel without it = a life.
//
//   LEFT / RIGHT — run along the girder (follows the slope)
//   UP / DOWN     — climb a ladder you're lined up with
//   Z            — jump
//
// All drawn from primitives — no sprite sheet.

// ── stage geometry ────────────────────────────────────────────
// 6 girders, numbered 0 (bottom) .. 5 (top). Each is a line
// y = base + slope * (x - x0) across [x0, x1]. Slopes alternate
// so the man zig-zags up the tower, just like the arcade.

#define NG 6
typedef struct { int x0, x1; float y0; float slope; } Girder;
static Girder gird[NG] = {
    //  x0   x1    y0     slope (rise per px; +down)
    // Gentle ±0.020 slants over the full width: ~6px of tilt — clearly slanted
    // for the barrels to follow, but never enough to converge with the floor
    // above (29px apart), so adjacent girders never cross or visually merge.
    {   8, 312, 185.0f, -0.020f },  // 0 bottom — drops to the left
    {   8, 312, 156.0f,  0.020f },  // 1        — drops to the right
    {   8, 312, 127.0f, -0.020f },  // 2        — drops to the left
    {   8, 312,  98.0f,  0.020f },  // 3        — drops to the right
    {   8, 312,  69.0f, -0.020f },  // 4        — drops to the left
    {   8, 240,  44.0f,  0.000f },  // 5 top — flat, Kong + princess
};

static float gird_y(int g, float x) {
    if (x < gird[g].x0) x = gird[g].x0;
    if (x > gird[g].x1) x = gird[g].x1;
    return gird[g].y0 + gird[g].slope * (x - gird[g].x0);
}

// ── ladders ───────────────────────────────────────────────────
// each connects girder `lo` to `lo+1` at a given x.
#define NL 7
typedef struct { int x, lo; } Ladder;
static Ladder lad[NL] = {
    // the main path zig-zags up: right, left, right, left, then up to the princess
    { 270, 0 },   // 0->1 right side
    {  50, 1 },   // 1->2 left
    { 270, 2 },   // 2->3 right
    {  50, 3 },   // 3->4 left
    { 200, 4 },   // 4->5 — tops out beside the princess
    { 150, 1 },   // a couple of mid shortcuts (barrels take these too)
    { 160, 3 },
};

#define LAD_W 10

// ── player ────────────────────────────────────────────────────
#define PW 8
#define PH 12
static float px, py;        // feet position (py = surface y)
static int   pg;            // girder the man stands on
static int   face;          // -1 / +1
static bool  climbing;
static int   climb_lad;     // index of ladder being climbed
static bool  jumping;
static float jvy;           // jump vertical vel
static float jbase;         // surface y at jump start
#define RUN_SPD 70.0f
#define CLIMB_SPD 52.0f
#define JUMP_V0 -118.0f
#define GRAV 380.0f

// ── barrels ───────────────────────────────────────────────────
#define MAXB 10
enum { B_OFF, B_ROLL, B_LADDER, B_FALL };
typedef struct {
    int   state;
    float x, y;
    int   g;            // girder index when rolling
    int   dir;          // roll direction -1/+1
    float vy;           // when falling
    int   lad;          // ladder index when descending
    bool  scored;       // already gave jump points this pass
} Barrel;
static Barrel barrels[MAXB];
static float barrel_t;   // time until next toss

// ── hammer ────────────────────────────────────────────────────
static bool  hammer_taken;
static float hammer_t;   // remaining frenzy seconds (0 = none)
static int   hammer_g;   // girder the pickup sits on
static int   hammer_x;

// ── game state ────────────────────────────────────────────────
static int   lives, score, hi;
static int   state;      // 0 play, 1 dead-pause, 2 gameover, 3 win
static float state_t;
static float flash_t;    // red death flash
static bool  ready;

// ── helpers ───────────────────────────────────────────────────
static float fabsf_(float v) { return v < 0 ? -v : v; }
static int ladder_top_girder(int li)   { return lad[li].lo + 1; }
static int ladder_bot_girder(int li)   { return lad[li].lo; }

// which way is downhill on this girder? slope is "rise per px, +down", so a
// positive slope means the surface drops to the RIGHT (+1); negative drops LEFT (-1).
static int downhill_dir(int g) { return gird[g].slope < 0 ? -1 : 1; }

// is the man within reach of a ladder going UP from his girder?
static int ladder_up_here(void) {
    for (int i = 0; i < NL; i++)
        if (ladder_bot_girder(i) == pg && fabsf_(px - lad[i].x) < 9)
            return i;
    return -1;
}
static int ladder_down_here(void) {
    for (int i = 0; i < NL; i++)
        if (ladder_top_girder(i) == pg && fabsf_(px - lad[i].x) < 9)
            return i;
    return -1;
}

// ── setup ─────────────────────────────────────────────────────
static void spawn_player(void) {
    pg = 0; px = 20; py = gird_y(0, px);
    face = 1; climbing = false; jumping = false; climb_lad = -1;
}

static void reset_game(void) {
    for (int i = 0; i < MAXB; i++) barrels[i].state = B_OFF;
    barrel_t = 1.2f;
    hammer_taken = false; hammer_t = 0;
    hammer_g = 2; hammer_x = 160;
    lives = 3; score = 0; state = 0; flash_t = 0;
    hi = load_int("dk_hi", 0);
    spawn_player();
    ready = true;
}

static void toss_barrel(void) {
    for (int i = 0; i < MAXB; i++) {
        if (barrels[i].state != B_OFF) continue;
        barrels[i].state = B_ROLL;
        barrels[i].g = NG - 1;            // top girder
        barrels[i].x = 70;
        barrels[i].y = gird_y(NG - 1, barrels[i].x);
        barrels[i].dir = 1;              // roll right off the top
        barrels[i].scored = false;
        note(38, INSTR_NOISE, 5);
        return;
    }
}

static void lose_life(void) {
    lives--;
    flash_t = 0.4f;
    shake(5);
    note(30, INSTR_SAW, 6);
    if (lives <= 0) {
        if (score > hi) { hi = score; save_int("dk_hi", hi); }
        state = 2; state_t = 0;
    } else {
        state = 1; state_t = 0;
    }
}

// ── update ────────────────────────────────────────────────────
void update(void) {
    if (!ready) reset_game();
    float d = dt();

    if (flash_t > 0) flash_t -= d;

    if (state == 1) {                 // brief death pause then respawn
        state_t += d;
        if (state_t > 0.9f) { spawn_player(); state = 0; }
        return;
    }
    if (state == 2 || state == 3) {   // gameover / win
        state_t += d;
        if (state_t > 0.5f && (btnp(0,BTN_A) || btnp(0,BTN_LEFT) ||
            btnp(0,BTN_RIGHT) || btnp(0,BTN_UP) || keyp(KEY_SPACE) || keyp(KEY_ENTER)))
            reset_game();
        return;
    }

    // ---- hammer timer ----
    if (hammer_t > 0) {
        hammer_t -= d;
        if (hammer_t <= 0) hammer_t = 0;
    }

#ifdef DE_TRACE
    watch("pl", "pg=%d px=%.0f py=%.0f climb=%d jump=%d", pg, px, py, climbing, jumping);
#endif
    // ---- player movement ----
    if (climbing) {
        float cspd = CLIMB_SPD * d;
        if (btn(0, BTN_UP))   py -= cspd;
        if (btn(0, BTN_DOWN)) py += cspd;
        int topg = ladder_top_girder(climb_lad);
        int botg = ladder_bot_girder(climb_lad);
        float ty = gird_y(topg, lad[climb_lad].x);
        float by = gird_y(botg, lad[climb_lad].x);
        px = lad[climb_lad].x;
        if (py <= ty) {                 // reached the top girder
            py = ty; pg = topg; climbing = false;
            if (pg == NG - 1) { state = 3; state_t = 0;   // touched the top → win
                score += 800; strum(72, CHORD_MAJ, INSTR_TRI, 5, 50);
                if (score > hi) { hi = score; save_int("dk_hi", hi); } }
        } else if (py >= by) {          // back down to the lower girder
            py = by; pg = botg; climbing = false;
        }
    } else if (jumping) {
        jvy += GRAV * d;
        py += jvy * d;
        px += face * RUN_SPD * 0.6f * d;
        px = clamp(px, gird[pg].x0, gird[pg].x1);
        float surf = gird_y(pg, px);
        if (jvy > 0 && py >= surf) { py = surf; jumping = false; }
    } else {
        // run along the girder, following its slope
        float run = RUN_SPD * d;
        if (btn(0, BTN_LEFT))  { px -= run; face = -1; }
        if (btn(0, BTN_RIGHT)) { px += run; face =  1; }
        px = clamp(px, gird[pg].x0, gird[pg].x1);
        py = gird_y(pg, px);

        // start climbing
        if (btn(0, BTN_UP)) {
            int u = ladder_up_here();
            if (u >= 0) { climbing = true; climb_lad = u; px = lad[u].x; }
        }
        if (btn(0, BTN_DOWN)) {
            int dn = ladder_down_here();
            if (dn >= 0) { climbing = true; climb_lad = dn; px = lad[dn].x;
                           py -= 1; }   // nudge onto the ladder
        }
        // jump
        if (btnp(0, BTN_A)) {
            jumping = true; jvy = JUMP_V0; jbase = py;
            note(64, INSTR_SQUARE, 4);
        }
    }

    // ---- hammer pickup ----
    if (!hammer_taken && !jumping) {
        float hy = gird_y(hammer_g, hammer_x);
        if (pg == hammer_g && fabsf_(px - hammer_x) < 8 && fabsf_(py - hy) < 6) {
            hammer_taken = true; hammer_t = 6.0f;
            note(80, INSTR_SQUARE, 5);
        }
    }

    // ---- barrel spawning ----
    barrel_t -= d;
    if (barrel_t <= 0) {
        toss_barrel();
        barrel_t = 2.0f + rnd_float() * 1.4f;
    }

    // ---- barrels ----
    bool hammer_active = hammer_t > 0;
    for (int i = 0; i < MAXB; i++) {
        Barrel *b = &barrels[i];
        if (b->state == B_OFF) continue;

        if (b->state == B_ROLL) {
            float spd = 64.0f * d;
            b->x += b->dir * spd;
            // roll downhill faster (visual feel) — keep simple
            b->y = gird_y(b->g, b->x);
            // fell off the low end → drop to next girder down
            bool off_left  = b->x <= gird[b->g].x0;
            bool off_right = b->x >= gird[b->g].x1;
            if (off_left || off_right) {
                if (b->g == 0) { b->state = B_OFF; continue; } // rolled off bottom
                b->state = B_FALL; b->vy = 30;
            } else if (b->g > 0 && chance(2)) {
                // sometimes take a ladder straight down
                for (int li = 0; li < NL; li++)
                    if (ladder_top_girder(li) == b->g &&
                        fabsf_(b->x - lad[li].x) < 4) {
                        b->state = B_LADDER; b->lad = li;
                        b->x = lad[li].x; b->vy = 36; break;
                    }
            }
        } else if (b->state == B_LADDER) {
            b->y += b->vy * d;
            float by = gird_y(ladder_bot_girder(b->lad), b->x);
            if (b->y >= by) {
                b->y = by; b->g = ladder_bot_girder(b->lad);
                b->dir = downhill_dir(b->g);  // resume rolling downhill on the lower girder
                b->state = B_ROLL;
            }
        } else if (b->state == B_FALL) {
            b->vy += GRAV * 0.6f * d;
            b->y += b->vy * d;
            int ng = b->g - 1;
            if (ng < 0) { b->state = B_OFF; continue; }
            float ny = gird_y(ng, b->x);
            if (b->y >= ny) {
                b->y = ny; b->g = ng;
                b->dir = downhill_dir(ng);    // roll downhill on the new girder
                b->state = B_ROLL;
            }
        }

#ifdef DE_TRACE
        if (i == 0) watch("b0", "st=%d g=%d x=%.0f y=%.0f dir=%d", b->state, b->g, b->x, b->y, b->dir);
#endif
        // ---- collision with player ----
        if (state == 0 && !climbing) {
            float dxp = fabsf_(b->x - px);
            float dyp = fabsf_(b->y - (py - PH/2));
            if (dxp < 8 && dyp < 9) {
                if (hammer_active) {
                    b->state = B_OFF; score += 300;
                    note(52, INSTR_NOISE, 5);
                } else {
                    lose_life();
                    return;
                }
            }
            // jump-over scoring: barrel passes close under the man while airborne
            if (!b->scored && jumping && dxp < 11 && b->y > py - 2 && b->y < py + 16) {
                b->scored = true; score += 100;
                note(76, INSTR_SQUARE, 3);
            }
        }
    }
}

// ── drawing ───────────────────────────────────────────────────
static void draw_girder(int g) {
    Girder *G = &gird[g];
    int steps = (G->x1 - G->x0);
    // thick red beam with rivets
    for (int x = G->x0; x <= G->x1; x++) {
        int y = (int)gird_y(g, x);
        rectfill(x, y, 1, 6, CLR_DARK_RED);
        pset(x, y, CLR_RED);
        pset(x, y + 5, CLR_DARK_PURPLE);
    }
    for (int x = G->x0; x <= G->x1; x += 12) {
        int y = (int)gird_y(g, x);
        pset(x, y + 2, CLR_ORANGE);
    }
    (void)steps;
}

static void draw_ladder(int li) {
    int topg = ladder_top_girder(li);
    int botg = ladder_bot_girder(li);
    int x = lad[li].x;
    int yt = (int)gird_y(topg, x);
    int yb = (int)gird_y(botg, x);
    line(x - LAD_W/2, yt, x - LAD_W/2, yb, CLR_LIGHT_PEACH);
    line(x + LAD_W/2, yt, x + LAD_W/2, yb, CLR_LIGHT_PEACH);
    for (int y = yt + 3; y < yb; y += 6)
        line(x - LAD_W/2, y, x + LAD_W/2, y, CLR_PEACH);
}

static void draw_kong(void) {
    int kx = 36, ky = (int)gird_y(NG - 1, kx) - 30;
    int arm = anim(2, 2, 0) ? -2 : 2;
    // body
    rectfill(kx - 12, ky + 8, 24, 18, CLR_BROWN);
    rectfill(kx - 9,  ky + 12, 18, 12, CLR_LIGHT_PEACH); // belly
    // head
    rectfill(kx - 10, ky - 6, 20, 14, CLR_BROWN);
    rectfill(kx - 6,  ky,     12,  6, CLR_DARK_BROWN);   // muzzle
    pset(kx - 4, ky - 2, CLR_WHITE); pset(kx + 4, ky - 2, CLR_WHITE);
    pset(kx - 4, ky - 2, CLR_BLACK); pset(kx + 4, ky - 2, CLR_BLACK);
    // arms
    rectfill(kx - 16, ky + 8 + arm, 5, 12, CLR_DARK_BROWN);
    rectfill(kx + 11, ky + 8 - arm, 5, 12, CLR_DARK_BROWN);
}

static void draw_princess(void) {
    int x = 196, y = (int)gird_y(NG - 1, x);
    rectfill(x - 4, y - 16, 8, 11, CLR_PINK);     // dress
    circfill(x, y - 18, 3, CLR_LIGHT_PEACH);       // head
    rectfill(x - 4, y - 21, 8, 2, CLR_YELLOW);     // hair top
    if (blink(20)) print("HELP", x - 14, y - 32, CLR_WHITE);
}

static void draw_player(void) {
    int x = (int)px, y = (int)py;
    int top = y - PH;
    if (climbing) {
        // facing away, legs alternate
        int s = anim(2, 6, 0) ? 1 : -1;
        rectfill(x - 3, top + 2, 6, 6, CLR_BLUE);     // body
        circfill(x, top, 2, CLR_LIGHT_PEACH);          // head
        rectfill(x - 3, top + 8, 2, 4 * (s>0?1:1), CLR_DARK_BLUE);
        rectfill(x + 1, top + 8, 2, 4, CLR_DARK_BLUE);
    } else {
        bool moving = btn(0,BTN_LEFT) || btn(0,BTN_RIGHT);
        int step = (jumping) ? 2 : (moving ? (anim(2,8,0)?2:-2) : 0);
        rectfill(x - 3 - (step<0?1:0), top + 7, 3, 5, CLR_DARK_BLUE); // back leg
        rectfill(x + (step>0?1:0),     top + 7, 3, 5, CLR_DARK_BLUE); // front leg
        rectfill(x - 3, top + 1, 6, 7, CLR_RED);        // overalls/shirt
        rectfill(x - 3, top + 4, 6, 4, CLR_BLUE);       // overalls
        circfill(x, top - 1, 3, CLR_LIGHT_PEACH);        // head
        rectfill(x - 3, top - 3, 6, 2, CLR_RED);         // cap
        pset(x + face * 2, top - 1, CLR_BLACK);          // eye
        // hammer
        if (hammer_t > 0) {
            int up = anim(2, 8, 0);
            int hx = x + face * 6;
            int hy = up ? top - 6 : top + 4;
            line(x + face * 3, top + 2, hx, hy, CLR_BROWN);
            rectfill(hx - 2, hy - 2, 5, 5, CLR_YELLOW);
        }
    }
}

static void draw_barrel(Barrel *b) {
    int x = (int)b->x, y = (int)b->y - 4;
    int spin = ((int)(b->x * 0.5f)) % 2;
    circfill(x, y, 4, CLR_DARK_ORANGE);
    circ(x, y, 4, CLR_BROWN);
    if (spin) { line(x - 3, y - 1, x + 3, y - 1, CLR_BROWN);
                line(x - 3, y + 1, x + 3, y + 1, CLR_BROWN); }
    else      { line(x - 1, y - 3, x - 1, y + 3, CLR_BROWN);
                line(x + 1, y - 3, x + 1, y + 3, CLR_BROWN); }
}

void draw(void) {
    if (!ready) reset_game();
    cls(CLR_BLACK);

    // faint girder-tower frame on the right (the structure)
    rectfill(0, 192, SCREEN_W, 8, CLR_DARKER_BLUE);

    for (int g = 0; g < NG; g++) draw_girder(g);
    for (int i = 0; i < NL; i++) draw_ladder(i);

    // hammer pickup
    if (!hammer_taken) {
        int hy = (int)gird_y(hammer_g, hammer_x);
        if (blink(10)) {
            line(hammer_x, hy - 10, hammer_x, hy - 2, CLR_BROWN);
            rectfill(hammer_x - 3, hy - 14, 7, 5, CLR_YELLOW);
            rect(hammer_x - 3, hy - 14, 7, 5, CLR_ORANGE);
        }
    }

    draw_kong();
    draw_princess();

    for (int i = 0; i < MAXB; i++)
        if (barrels[i].state != B_OFF) draw_barrel(&barrels[i]);

    if (state != 1 || (frame() % 4 < 2)) draw_player();

    // ---- HUD ----
    rectfill(0, 0, SCREEN_W, 9, CLR_DARK_BLUE);
    print(str("SCORE %d", score), 4, 1, CLR_WHITE);
    print_centered(str("HI %d", hi), SCREEN_W/2, 1, CLR_LIGHT_YELLOW);
    for (int i = 0; i < lives; i++) {
        int lx = SCREEN_W - 8 - i * 9;
        rectfill(lx - 2, 2, 5, 5, CLR_RED);
        circfill(lx, 1, 1, CLR_LIGHT_PEACH);
    }
    if (hammer_t > 0)
        print(str("HAMMER %.0f", hammer_t + 0.99f), 4, 11, CLR_YELLOW);

    if (state == 2) {
        rectfill(SCREEN_W/2 - 70, 80, 140, 44, CLR_BLACK);
        rect(SCREEN_W/2 - 70, 80, 140, 44, CLR_RED);
        print_centered("GAME OVER", SCREEN_W/2, 88, CLR_RED);
        print_centered(str("score %d", score), SCREEN_W/2, 100, CLR_YELLOW);
        print_centered("press Z to retry", SCREEN_W/2, 112, CLR_LIGHT_GREY);
    } else if (state == 3) {
        rectfill(SCREEN_W/2 - 70, 80, 140, 44, CLR_BLACK);
        rect(SCREEN_W/2 - 70, 80, 140, 44, CLR_PINK);
        print_centered("YOU SAVED HER!", SCREEN_W/2, 88, blink(8) ? CLR_PINK : CLR_WHITE);
        print_centered(str("score %d", score), SCREEN_W/2, 100, CLR_YELLOW);
        print_centered("press Z to play again", SCREEN_W/2, 112, CLR_LIGHT_GREY);
    }

    // red flash overlay drawn last so it covers everything
    if (flash_t > 0) {
        fillp(FILL_CHECKER, -1);
        rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_RED);
        fillp_reset();
    }
}
