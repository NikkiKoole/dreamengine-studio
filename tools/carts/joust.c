/* de:meta
{
  "slug": "joust",
  "title": "joust",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "finite-state-ai",
    "title-play-gameover-loop"
  ],
  "lineage": "Tribute to Williams Joust (1982); enemy AI uses a simple flap-cooldown chase with randomized upward bias, and the egg-hatching timer creates a secondary urgency loop not present in earlier arcade tributes in the library.",
  "genre": "arcade",
  "homage": "Joust (1982)",
  "description": "A tribute to the Williams arcade classic. Ride your flying mount over a lava pit: tap A to FLAP against constant gravity, steer with LEFT/RIGHT (you carry momentum), and the screen wraps left to right. Charge enemy riders — whoever is higher at the moment of contact wins; equal height bounces you both apart. A beaten rider drops an egg: grab it for points before it hatches into a fresh enemy. Clear the wave to advance. Three lives; the lava is always fatal.",
  "todo": [
    "The enemies and background shouldn't share the same color."
  ]
}
de:meta */
#include "studio.h"

// ── JOUST — flap, charge, collect ─────────────────────────────
// Ride your flying mount. Tap A to FLAP (gravity is always pulling
// you down). Charge an enemy rider: whoever's higher wins the joust.
// A beaten rider drops an EGG — grab it for points before it hatches
// into a new enemy. Don't touch the lava. Clear a wave to advance.
//
//   • LEFT / RIGHT — steer (you carry momentum)
//   • A            — flap your wings
//   • the screen wraps left↔right, just like the arcade
//
// Three lives. Hit a rider from below and you lose one; from above
// and they're an omelette.

#define HUD_H   10
#define LAVA_Y  188
#define GRAV    0.13f
#define FLAP   -1.7f
#define VY_UP  -3.4f
#define VY_DN   3.2f

typedef struct { int x, y, w; } Plat;
static const Plat plats[] = {
    { 0, 182, 108 }, { 212, 182, 108 },     // bottom ledges (lava gap in the middle)
    { 120, 152, 80 },                        // low island
    { 20, 124, 72 }, { 228, 124, 72 },       // mid sides
    { 120, 92, 80 },                         // high island
};
#define NPLAT ((int)(sizeof(plats) / sizeof(plats[0])))

// ── player ──
static float px, py, pvx, pvy;
static int   pface = 1, inv;
static bool  pground;

// ── enemies ──
#define MAXE 12
typedef struct { float x, y, vx, vy; int on, flap_cd, col; } Mon;
static Mon mon[MAXE];

// ── eggs ──
#define MAXG 12
typedef struct { float x, y, vy; int timer; bool on; } Egg;
static Egg egg[MAXG];

static int  score, lives, wave, wave_flash;
static bool gameover;
static bool ready = false;

// ── platform landing: returns top y if a faller lands, else -1 ──
static int land_y(float x, float yprev, float y) {
    for (int i = 0; i < NPLAT; i++) {
        const Plat *p = &plats[i];
        if (x >= p->x - 2 && x <= p->x + p->w + 2 && yprev <= p->y + 1 && y >= p->y)
            return p->y;
    }
    return -1;
}

static void spawn_egg(float x, float y) {
    for (int i = 0; i < MAXG; i++)
        if (!egg[i].on) { egg[i] = (Egg){ x, y, 0, 300, true }; return; }
}

static void spawn_enemy(float x, float y) {
    int cols[3] = { CLR_RED, CLR_ORANGE, CLR_PINK };
    for (int i = 0; i < MAXE; i++)
        if (!mon[i].on) {
            mon[i] = (Mon){ x, y, 0, 0, 1, 10 + i * 5, cols[i % 3] };
            return;
        }
}

static void spawn_wave(void) {
    int spots[3][2] = { { 160, 90 }, { 56, 122 }, { 264, 122 } };
    int n = 2 + wave;
    if (n > MAXE) n = MAXE;
    for (int i = 0; i < MAXE; i++) mon[i].on = 0;
    for (int i = 0; i < MAXG; i++) egg[i].on = false;
    for (int i = 0; i < n; i++) spawn_enemy(spots[i % 3][0], spots[i % 3][1] - (i / 3) * 20);
    wave_flash = 90;
}

static void respawn_player(void) {
    px = 160; py = 150; pvx = pvy = 0; pface = 1; pground = false; inv = 90;
}

static void reset_game(void) {
    score = 0; lives = 3; wave = 0; gameover = false;
    spawn_wave(); respawn_player();
    ready = true;
}

static void hurt(void) {
    lives--; note(36, INSTR_SAW, 5);
    if (lives < 0) gameover = true;
    else respawn_player();
}

static int alive_total(void) {
    int n = 0;
    for (int i = 0; i < MAXE; i++) if (mon[i].on) n++;
    for (int i = 0; i < MAXG; i++) if (egg[i].on) n++;
    return n;
}

// shortest horizontal delta accounting for wrap
static float wrap_dx(float to, float from) {
    float d = to - from;
    if (d > SCREEN_W / 2) d -= SCREEN_W;
    if (d < -SCREEN_W / 2) d += SCREEN_W;
    return d;
}

void update(void) {
    if (!ready) reset_game();
    if (gameover) { if (btnp(0, BTN_A)) reset_game(); return; }
    if (wave_flash > 0) wave_flash--;
    if (inv > 0) inv--;

    // ── player flight ──
    if (btn(0, BTN_LEFT))  { pvx -= 0.18f; pface = -1; }
    if (btn(0, BTN_RIGHT)) { pvx += 0.18f; pface =  1; }
    pvx = clamp(pvx, -2.2f, 2.2f);
    if (pground && !btn(0, BTN_LEFT) && !btn(0, BTN_RIGHT)) pvx *= 0.8f;   // ground friction
    if (btnp(0, BTN_A)) { pvy += FLAP; if (pvy < VY_UP) pvy = VY_UP; pground = false; note(72, INSTR_SQUARE, 2); }

    pvy = clamp(pvy + GRAV, VY_UP, VY_DN);
    float ppy = py;
    px += pvx; py += pvy;
    if (px < 0) px += SCREEN_W; if (px > SCREEN_W) px -= SCREEN_W;     // wrap

    pground = false;
    if (pvy >= 0) {
        int ly = land_y(px, ppy, py);
        if (ly >= 0) { py = ly; pvy = 0; pground = true; }
    }
    if (py >= LAVA_Y) { hurt(); return; }                              // lava!

    // ── enemies ──
    for (int e = 0; e < MAXE; e++) {
        Mon *m = &mon[e];
        if (!m->on) continue;
        // AI: chase the player's column, flap to stay a touch above
        float dx = wrap_dx(px, m->x);
        m->vx = clamp(m->vx + sgn(dx) * 0.10f, -1.7f, 1.7f);
        if (--m->flap_cd <= 0) {
            if (m->y > py - 4 || chance(25)) m->vy += FLAP * 0.9f;
            if (m->vy < VY_UP) m->vy = VY_UP;
            m->flap_cd = 16 + rnd(20);
        }
        m->vy = clamp(m->vy + GRAV, VY_UP, VY_DN);
        float mpy = m->y;
        m->x += m->vx; m->y += m->vy;
        if (m->x < 0) m->x += SCREEN_W; if (m->x > SCREEN_W) m->x -= SCREEN_W;
        if (m->vy >= 0) { int ly = land_y(m->x, mpy, m->y); if (ly >= 0) { m->y = ly; m->vy = 0; } }
        if (m->y >= LAVA_Y) { m->on = 0; continue; }                   // enemy fell in lava

        // joust!
        if (near((int)px, (int)py, (int)m->x, (int)m->y, 11)) {
            if (py < m->y - 3) {                                        // player higher → win
                m->on = 0; score += 500; spawn_egg(m->x, m->y);
                pvy = -1.6f; strum(72, CHORD_MAJ, INSTR_TRI, 4, 30);
            } else if (m->y < py - 3) {                                 // enemy higher → lose
                if (inv == 0) { hurt(); return; }
            } else {                                                    // tie → bounce apart
                pvx = -pvx; m->vx = -m->vx; pvy = -1.2f; m->vy = -1.2f;
                note(50, INSTR_NOISE, 2);
            }
        }
    }

    // ── eggs ──
    for (int i = 0; i < MAXG; i++) {
        Egg *g = &egg[i];
        if (!g->on) continue;
        float gpy = g->y;
        g->vy = clamp(g->vy + GRAV, 0, VY_DN);
        g->y += g->vy;
        int ly = land_y(g->x, gpy, g->y);
        if (g->vy >= 0 && ly >= 0) { g->y = ly; g->vy = 0; }
        if (g->y >= LAVA_Y) { g->on = false; continue; }
        if (--g->timer <= 0) { g->on = false; spawn_enemy(g->x, g->y - 6); continue; }  // hatch!
        if (near((int)px, (int)py, (int)g->x, (int)g->y - 3, 10)) {     // collect
            g->on = false; score += 250; note(96, INSTR_SINE, 3);
        }
    }

    if (alive_total() == 0) { wave++; spawn_wave(); }
}

// ── drawing ───────────────────────────────────────────────────
static void draw_rider(int x, int y, int dir, int mountcol, int ridercol) {
    circfill(x, y - 4, 5, mountcol);                       // mount body
    trifill(x - dir * 5, y - 4, x - dir * 9, y - 7, x - dir * 9, y - 2, mountcol); // tail
    line(x + dir * 4, y - 5, x + dir * 8, y - 9, mountcol);   // neck
    circfill(x + dir * 8, y - 10, 2, mountcol);            // head
    line(x - 2, y, x - 2, y + 2, mountcol);                // legs
    line(x + 2, y, x + 2, y + 2, mountcol);
    circfill(x, y - 9, 3, ridercol);                       // rider
    line(x + dir, y - 11, x + dir * 11, y - 14, CLR_LIGHT_GREY);   // lance
}

void draw(void) {
    if (!ready) reset_game();
    cls(CLR_BROWNISH_BLACK);

    // platforms — rock ledges
    for (int i = 0; i < NPLAT; i++) {
        const Plat *p = &plats[i];
        rectfill(p->x, p->y, p->w, LAVA_Y - p->y + 2, CLR_DARK_GREY);
        rectfill(p->x, p->y, p->w, 3, CLR_LIGHT_GREY);
        rect(p->x, p->y, p->w, 4, CLR_DARKER_GREY);
    }

    // lava
    rectfill(0, LAVA_Y, SCREEN_W, SCREEN_H - LAVA_Y, CLR_RED);
    for (int x = 0; x < SCREEN_W; x += 6) {
        int yy = LAVA_Y + (int)(sin_deg(x * 4 + frame() * 4) + 1);
        rectfill(x, yy, 4, 2, CLR_ORANGE);
        if ((x / 6 + frame() / 8) % 4 == 0) pset(x + 2, LAVA_Y + 5, CLR_YELLOW);
    }

    // eggs
    for (int i = 0; i < MAXG; i++) if (egg[i].on) {
        int x = (int)egg[i].x, y = (int)egg[i].y;
        bool soon = egg[i].timer < 90 && !blink(6);            // wobble before hatching
        circfill(x + (soon ? 1 : 0), y - 3, 3, CLR_LIGHT_PEACH);
        pset(x - 1, y - 4, CLR_WHITE);
    }

    // enemies + player
    for (int e = 0; e < MAXE; e++)
        if (mon[e].on) draw_rider((int)mon[e].x, (int)mon[e].y, sgn(mon[e].vx) ? sgn(mon[e].vx) : 1, CLR_DARK_GREY, mon[e].col);
    if (inv <= 0 || blink(4)) draw_rider((int)px, (int)py, pface, CLR_LIGHT_YELLOW, CLR_BLUE);

    // HUD
    rectfill(0, 0, SCREEN_W, HUD_H, CLR_BLACK);
    print(str("SCORE %d", score), 4, 1, CLR_YELLOW);
    print_centered(str("WAVE %d", wave + 1), SCREEN_W/2, 1, CLR_WHITE);
    for (int i = 0; i < lives; i++) {
        int lx = SCREEN_W - 9 - i * 11;
        circfill(lx, 5, 3, CLR_LIGHT_YELLOW); circfill(lx, 3, 2, CLR_BLUE);
    }

    if (wave_flash > 0) print_centered(str("WAVE %d", wave + 1), SCREEN_W/2, 96, CLR_LIGHT_YELLOW);
    if (gameover) {
        int w = 200, bx = (SCREEN_W - w) / 2;
        rectfill(bx, 82, w, 32, CLR_DARK_PURPLE);
        rect(bx, 82, w, 32, CLR_WHITE);
        print_centered("GAME OVER", SCREEN_W/2, 90, CLR_RED);
        print_centered(str("score %d  -  press A", score), SCREEN_W/2, 102, CLR_YELLOW);
    }
}
