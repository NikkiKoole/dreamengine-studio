/* de:meta
{
  "title": "Wildfire",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "cellular-automata",
    "dithering-gradient",
    "state-machine"
  ],
  "lineage": "Lifts the 1993 Doom-fire heat-propagation rule out of its original bottom-up backdrop use and repurposes it as a top-down chasing hazard that climbs walls and burns through them.",
  "genre": "arcade",
  "description": "The doom-fire propagation rule turned into a chasing hazard — outrun a top-down floor fire that spreads cell-to-cell and burns through walls to reach the green exit. WASD/arrows move, Z/A dashes (short cooldown), R/X instant-retry.",
  "todo": [
    "ui-audit: the bottom HUD (\"DASH\", \"REACH THE EXIT\", \"RUN/ASH\") runs off the bottom edge and the labels overlap."
  ]
}
de:meta */
#include "studio.h"
#include <stddef.h>

// WILDFIRE — doomfire as a WEAPON, not a backdrop.
//
// The famous 1993 doom-fire propagation rule — keep a grid of "heat", each cell
// cooling a little as it copies from a hotter neighbour — but UNCHAINED from the
// bottom-up case. Here heat spreads ACROSS the floor in every direction and
// CLIMBS walls, so the flame front genuinely chases you across a top-down room.
//
// You're a tiny figure on a stone floor. A fire seeds in one corner and grows,
// the front advancing on the same cooling rule that makes doom-fire look alive.
// Touch live fire and you're ash. Stay ahead of it and reach the GREEN EXIT on
// the far side. One dash (a short sprint) buys you a gap when you're boxed in.
// Death is instant-restart, hotline-style — a fresh fire, same room.
//
//   MOVE     WASD / arrows / d-pad
//   DASH     Z / A      (a quick burst, short cooldown)
//   RETRY    R / X      (instant — or auto after the death beat)
//
// The fire is drawn at sub-cell resolution off a heat field; the player and
// walls live in pixel space on top. The heat ramp runs black -> dark-red ->
// red -> orange -> yellow -> white, dithered at the cool edges so the front
// shimmers. Walls glow and char as the fire eats them, then collapse into more
// floor for the fire to pour through — no wall is a permanent shield.

// ---- fire heat grid (sub-cell: finer than the tilemap, the way doomfire is) ----
#define FW   80          // heat grid width  (320 / 4)
#define FH   50          // heat grid height (200 / 4)
#define FCELL 4          // pixels per heat cell
#define NRAMP 9
#define HMAX  (NRAMP - 1) // hottest heat value

// ---- room tilemap (coarser: walls / floor / exit), in 16px tiles ----
#define TW   20          // 320 / 16
#define TH   12          // 192 / 16  (bottom 8px is the HUD strip)
#define TILE 16
#define PLAYFIELD_H (TH * TILE)   // 192

#define T_FLOOR 0
#define T_WALL  1
#define T_EXIT  2

// a burning wall has its own little heat/hp so it chars then collapses
#define WALL_HP 90

enum { ST_PLAY, ST_DEAD, ST_WIN };

static int   heat[FW * FH];     // 0..HMAX live fire heat
static int   wall_hp[TW * TH];  // remaining hp of each wall tile (0 once burnt away)
static int   grid[TW * TH];     // T_FLOOR / T_WALL / T_EXIT
static int   exit_cx, exit_cy;  // exit tile

static const int RAMP[NRAMP] = {
    CLR_BLACK, CLR_BROWNISH_BLACK, CLR_DARK_RED, CLR_RED,
    CLR_DARK_ORANGE, CLR_ORANGE, CLR_DARK_PEACH, CLR_YELLOW, CLR_WHITE,
};

// ---- player ----
static struct {
    float x, y;     // pixel centre
    float aim;      // facing (degrees) for the little flame-shadow lean
    bool  alive;
    float dash_cd;  // seconds until dash ready again
    float dash_t;   // seconds of dash burst remaining
    float dash_dir; // locked dash direction
} pl;

static int   state;
static float state_t;     // seconds in current state (for the death/win beat)
static int   wins, deaths;
static float burn_clock;  // throttles fire stepping to a fixed cadence
static float music_t;

// =====================================================================
// helpers
// =====================================================================

static int tget(int cx, int cy) {
    if (cx < 0 || cx >= TW || cy < 0 || cy >= TH) return T_WALL;
    return grid[cy * TW + cx];
}

// is this pixel solid to the PLAYER? (live walls block; burnt-out walls don't)
static bool solid_px(float px, float py) {
    int cx = (int)px / TILE, cy = (int)py / TILE;
    return tget(cx, cy) == T_WALL;
}

// heat at a pixel (samples the fine grid)
static int heat_px(int px, int py) {
    int hx = px / FCELL, hy = py / FCELL;
    if (hx < 0 || hx >= FW || hy < 0 || hy >= FH) return 0;
    return heat[hy * FW + hx];
}

// =====================================================================
// build the room
// =====================================================================

static void build_room(void) {
    for (int i = 0; i < FW * FH; i++) heat[i] = 0;

    // walls round the border, plus a few interior baffles to make the front
    // weave (the fire has to find its way around — and through — them).
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            int t = T_FLOOR;
            if (x == 0 || y == 0 || x == TW - 1 || y == TH - 1) t = T_WALL;
            grid[y * TW + x] = t;
            wall_hp[y * TW + x] = (t == T_WALL) ? WALL_HP : 0;
        }

    // interior baffles (hand-placed so there's always a gap to slip through)
    static const int BAF[][4] = {     // {x, y, w, h}
        { 4, 2, 1, 6 },
        { 8, 5, 5, 1 },
        { 13, 2, 1, 5 },
        { 6, 9, 8, 1 },
        { 15, 7, 1, 4 },
    };
    for (int b = 0; b < (int)(sizeof BAF / sizeof *BAF); b++) {
        for (int yy = BAF[b][1]; yy < BAF[b][1] + BAF[b][3]; yy++)
            for (int xx = BAF[b][0]; xx < BAF[b][0] + BAF[b][2]; xx++)
                if (xx > 0 && xx < TW - 1 && yy > 0 && yy < TH - 1) {
                    grid[yy * TW + xx] = T_WALL;
                    wall_hp[yy * TW + xx] = WALL_HP;
                }
    }

    // exit: a gap in the far wall, glowing green, on the opposite corner to the seed
    exit_cx = TW - 1; exit_cy = TH - 2;
    grid[exit_cy * TW + exit_cx] = T_EXIT;
    wall_hp[exit_cy * TW + exit_cx] = 0;

    // player spawns top-left, fire seeds top-left-ish behind them... actually
    // we want the fire to CHASE toward the exit, so seed top-left, player just
    // ahead of it, exit bottom-right.
    pl.x = 2 * TILE + 8; pl.y = 2 * TILE + 8;
    pl.aim = 0; pl.alive = true;
    pl.dash_cd = 0; pl.dash_t = 0; pl.dash_dir = 0;

    // seed a hot core in the top-left corner cells
    for (int y = 1; y <= 4; y++)
        for (int x = 1; x <= 4; x++)
            heat[y * FW + x] = HMAX;

    burn_clock = 0;
}

static void restart(void) {
    build_room();
    state = ST_PLAY; state_t = 0;
}

// =====================================================================
// the fire step — doomfire's rule, but omnidirectional across the floor
// =====================================================================
//
// Classic doomfire: each cell copies the one BELOW minus random cooling, with
// a sideways wind nudge. We generalise the "source" direction: a cell pulls
// heat from whichever neighbour is currently HOTTEST, then cools by a random
// amount. Spreading from the hottest neighbour means the front advances in
// every direction at the cooling-limited speed — exactly doom-fire, untethered
// from "up". Live walls smother it (heat can't pass a wall) but slowly catch
// fire themselves; a wall that's fully charred collapses to floor and the fire
// floods through.

static void fire_step(void) {
    static int next[FW * FH];

    for (int y = 0; y < FH; y++) {
        for (int x = 0; x < FW; x++) {
            int idx = y * FW + x;

            // which tile is this heat cell in? a live wall can't hold floor-fire
            int tx = (x * FCELL) / TILE, ty = (y * FCELL) / TILE;
            bool in_wall = (tget(tx, ty) == T_WALL);

            if (in_wall) {
                // walls don't carry the floor field; their burning is handled
                // separately (see burn_walls). keep their heat cell dark here.
                next[idx] = 0;
                continue;
            }

            // hottest open neighbour (4-connected + self), skipping live walls
            int hottest = heat[idx];
            static const int DX[4] = { -1, 1, 0, 0 };
            static const int DY[4] = { 0, 0, -1, 1 };
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (nx < 0 || nx >= FW || ny < 0 || ny >= FH) continue;
                int ntx = (nx * FCELL) / TILE, nty = (ny * FCELL) / TILE;
                if (tget(ntx, nty) == T_WALL) continue;  // walls block the spread
                int nh = heat[ny * FW + nx];
                if (nh > hottest) hottest = nh;
            }

            // cool: copy hottest neighbour minus a random bite. random cooling
            // is what gives doom-fire its living, flickering edge.
            int v = hottest - rnd(2);   // 0 or 1 of cooling per step
            if (v < 0) v = 0;
            next[idx] = v;
        }
    }

    for (int i = 0; i < FW * FH; i++) heat[i] = next[i];

    // keep the seed core blazing so the fire never dies out — the threat is
    // perpetual; you can only outrun it, not extinguish it.
    for (int y = 1; y <= 3; y++)
        for (int x = 1; x <= 3; x++)
            heat[y * FW + x] = HMAX;
}

// walls in contact with hot fire catch, char, then collapse to floor
static void burn_walls(void) {
    for (int cy = 1; cy < TH - 1; cy++) {
        for (int cx = 1; cx < TW - 1; cx++) {
            int gi = cy * TW + cx;
            if (grid[gi] != T_WALL) continue;

            // is there hot fire pressed against any open side of this wall?
            int px = cx * TILE, py = cy * TILE;
            int hottest = 0;
            int probes[4][2] = {
                { px - 1,        py + TILE / 2 },
                { px + TILE,     py + TILE / 2 },
                { px + TILE / 2, py - 1        },
                { px + TILE / 2, py + TILE     },
            };
            for (int p = 0; p < 4; p++) {
                int h = heat_px(probes[p][0], probes[p][1]);
                if (h > hottest) hottest = h;
            }
            // hot neighbours eat the wall; the hotter the faster
            if (hottest >= 5) wall_hp[gi] -= (hottest - 4);
            if (wall_hp[gi] <= 0) {
                grid[gi] = T_FLOOR;                 // collapse — fire pours in
                // light the newly opened cells so the breach reads as a burn-through
                int hx = px / FCELL, hy = py / FCELL;
                for (int yy = 0; yy < TILE / FCELL; yy++)
                    for (int xx = 0; xx < TILE / FCELL; xx++) {
                        int fi = (hy + yy) * FW + (hx + xx);
                        if (fi >= 0 && fi < FW * FH) heat[fi] = HMAX - 2;
                    }
                shake(1.5f);
                hit(40, INSTR_NOISE, 3, 80);
            }
        }
    }
}

// =====================================================================
// init / sound
// =====================================================================

void init(void) {
    bpm(96);
    // a low, breathing roar bed: filtered saw drone with a slow cutoff LFO,
    // plus a soft noise "crackle" we trigger on the beat.
    instrument(5, INSTR_SAW, 200, 400, 5, 400);
    instrument_filter(5, FILTER_LOW, 360, 7);
    instrument_lfo(5, 0, LFO_CUTOFF, 0.30f, 240.0f);
    lfo_shape(5, 0, LFO_SHAPE_RANDOM);   // organic drift, not a mechanical sine (STATUS #39)
    instrument(6, INSTR_NOISE, 1, 50, 0, 40);      // crackle
    instrument_filter(6, FILTER_HIGH, 1800, 2);

    wins = load_int("wf_wins", 0);
    deaths = load_int("wf_deaths", 0);
    restart();
}

// how much of the room is on fire right now (drives the music intensity)
static float fire_fraction(void) {
    int hot = 0, total = 0;
    for (int i = 0; i < FW * FH; i += 7) {   // sparse sample is plenty
        total++;
        if (heat[i] >= 4) hot++;
    }
    return total ? (float)hot / total : 0;
}

static void music_tick(float intensity) {
    if (every(1)) {
        // the roar swells with how much is ablaze
        int vol = 2 + (int)(intensity * 4);
        note(28, 5, min(7, vol));
    }
    // crackle gets denser as the fire grows
    int dens = 2 + (int)(intensity * 6);
    if (euclid(dens, 8, beat())) hit(70 + rnd(20), 6, 1 + (int)(intensity * 3), 24);
}

// =====================================================================
// update
// =====================================================================

static void try_move(float nx, float ny) {
    // axis-separated so we slide along walls instead of sticking
    float r = 3;   // player half-size for wall checks
    if (!solid_px(nx - r, pl.y) && !solid_px(nx + r, pl.y) &&
        !solid_px(nx, pl.y - r) && !solid_px(nx, pl.y + r))
        pl.x = nx;
    if (!solid_px(pl.x - r, ny) && !solid_px(pl.x + r, ny) &&
        !solid_px(pl.x, ny - r) && !solid_px(pl.x, ny + r))
        pl.y = ny;
}

static void update_play(void) {
    float d = dt();
    state_t += d;

    // step the fire at a steady cadence (independent of framerate)
    burn_clock += d;
    while (burn_clock >= 1.0f / 30.0f) {
        burn_clock -= 1.0f / 30.0f;
        fire_step();
        burn_walls();
    }

    // ---- input / movement ----
    int mvx = (btn(0, BTN_RIGHT) || key('D') ? 1 : 0) - (btn(0, BTN_LEFT) || key('A') ? 1 : 0);
    int mvy = (btn(0, BTN_DOWN)  || key('S') ? 1 : 0) - (btn(0, BTN_UP)   || key('W') ? 1 : 0);

    if (pl.dash_cd > 0) pl.dash_cd -= d;

    // start a dash
    if ((btnp(0, BTN_A) || keyp('Z')) && pl.dash_cd <= 0 && (mvx || mvy)) {
        pl.dash_t = 0.16f; pl.dash_cd = 1.1f;
        pl.dash_dir = angle_to(0, 0, mvx * 100, mvy * 100);
        hit(60, INSTR_SINE, 3, 60);
    }

    float speed;
    float a;
    if (pl.dash_t > 0) {
        pl.dash_t -= d;
        speed = 230;            // burst
        a = pl.dash_dir;
        pl.aim = a;
        try_move(pl.x + dx(speed * d, a), pl.y + dy(speed * d, a));
    } else if (mvx || mvy) {
        speed = 64;
        a = angle_to(0, 0, mvx * 100, mvy * 100);
        pl.aim = a;
        try_move(pl.x + dx(speed * d, a), pl.y + dy(speed * d, a));
    }

    // keep the player inside the playfield (the HUD strip is off-limits)
    pl.x = clamp(pl.x, 4, SCREEN_W - 4);
    pl.y = clamp(pl.y, 4, PLAYFIELD_H - 4);

    // ---- death: standing on live fire ----
    if (heat_px((int)pl.x, (int)pl.y) >= 3) {
        pl.alive = false;
        state = ST_DEAD; state_t = 0;
        deaths++; save_int("wf_deaths", deaths);
        shake(6);
        // a descending hiss-thud
        schedule(0,   46, INSTR_NOISE, 6);
        schedule(120, 40, INSTR_NOISE, 6);
        schedule(280, 34, INSTR_SAW,   6);
        return;
    }

    // ---- win: reached the exit tile ----
    if (tget((int)pl.x / TILE, (int)pl.y / TILE) == T_EXIT) {
        state = ST_WIN; state_t = 0;
        wins++; save_int("wf_wins", wins);
        schedule(0,   72, INSTR_SINE, 5);
        schedule(110, 76, INSTR_SINE, 5);
        schedule(220, 79, INSTR_SINE, 6);
        return;
    }

    music_tick(fire_fraction());
}

void update(void) {
    // instant retry from anywhere
    if (keyp('R') || btnp(0, BTN_B)) { restart(); return; }

    if (state == ST_PLAY) {
        update_play();
    } else {
        state_t += dt();
        music_tick(state == ST_DEAD ? fire_fraction() : 0.1f);
        // auto-restart a moment after death so it stays snappy; win waits for input
        if (state == ST_DEAD && state_t > 1.1f) restart();
        if (state == ST_WIN && (state_t > 0.4f) &&
            (btnp(0, BTN_A) || keyp('Z') || keyp('R') || btnp(0, BTN_B))) restart();
    }
}

// =====================================================================
// draw
// =====================================================================

static void draw_fire(void) {
    for (int y = 0; y < FH; y++) {
        for (int x = 0; x < FW; x++) {
            int h = heat[y * FW + x];
            if (h <= 0) continue;
            int col = RAMP[h];
            // dither the cool leading edge so the front shimmers instead of
            // banding — the cool ramp values get a sparse checker against black
            if (h <= 2) { fillp(FILL_CHECKER, CLR_BLACK); }
            rectfill(x * FCELL, y * FCELL, FCELL, FCELL, col);
            if (h <= 2) fillp_reset();
        }
    }
}

static void draw_walls(void) {
    for (int cy = 0; cy < TH; cy++) {
        for (int cx = 0; cx < TW; cx++) {
            int gi = cy * TW + cx;
            int t = grid[gi];
            int x = cx * TILE, y = cy * TILE;
            if (t == T_WALL) {
                // char the wall toward red/black as its hp drains
                int hp = wall_hp[gi];
                int base = CLR_LIGHT_GREY;
                int top  = CLR_MEDIUM_GREY;
                if (hp < WALL_HP) {
                    // heating up: grey -> brown -> dark-red glow at the eaten edge
                    if (hp > WALL_HP * 2 / 3)      { base = CLR_DARK_GREY;  top = CLR_MEDIUM_GREY; }
                    else if (hp > WALL_HP / 3)      { base = CLR_BROWN;      top = CLR_DARK_ORANGE; }
                    else                            { base = CLR_DARK_BROWN; top = CLR_RED; }
                }
                rectfill(x, y, TILE, TILE, base);
                rectfill(x, y, TILE, 3, top);                  // lit top edge
                // brick seams
                line(x, y + TILE / 2, x + TILE - 1, y + TILE / 2, CLR_BROWNISH_BLACK);
                if (hp < WALL_HP / 2 && blink(4))
                    pset(x + rnd(TILE), y + rnd(TILE), CLR_YELLOW);  // embers
            } else if (t == T_EXIT) {
                // glowing green doorway — pulses so it reads as the goal
                int pulse = 2 + (int)(sin_deg(now() * 240) * 2);
                rectfill(x, y, TILE, TILE, CLR_DARK_GREEN);
                rect(x + 1, y + 1, TILE - 2, TILE - 2, CLR_GREEN);
                circ(x + TILE / 2, y + TILE / 2, 3 + pulse, CLR_LIME_GREEN);
                print("EX", x + 1, y + 5, CLR_WHITE);
            }
        }
    }
}

static void draw_player(void) {
    if (!pl.alive) return;
    int x = (int)pl.x, y = (int)pl.y;

    // shadow
    ovalfill(x, y + 4, 4, 2, CLR_BROWNISH_BLACK);

    // dash trail
    if (pl.dash_t > 0) {
        for (int k = 1; k <= 3; k++) {
            int tx = (int)(x - dx(k * 4, pl.aim));
            int ty = (int)(y - dy(k * 4, pl.aim));
            circfill(tx, ty, 3 - k, CLR_BLUE);
        }
    }

    // body — a little runner; cyan/white so it pops against the fire
    circfill(x, y - 3, 3, CLR_LIGHT_PEACH);          // head
    rectfill(x - 2, y - 1, 4, 5, CLR_BLUE);          // torso
    // legs flicker for a running feel while moving/dashing
    int legf = (frame() / 4) & 1;
    line(x - 1, y + 4, x - 2 - legf, y + 6, CLR_DARK_BLUE);
    line(x + 1, y + 4, x + 2 + legf, y + 6 - legf, CLR_DARK_BLUE);
    pset(x, y - 4, CLR_WHITE);                        // tiny highlight
}

static void draw_hud(void) {
    rectfill(0, PLAYFIELD_H, SCREEN_W, SCREEN_H - PLAYFIELD_H, CLR_BLACK);
    line(0, PLAYFIELD_H, SCREEN_W, PLAYFIELD_H, CLR_DARK_RED);

    // dash readiness pip
    bool ready = pl.dash_cd <= 0;
    print("DASH", 4, PLAYFIELD_H + 1, ready ? CLR_BLUE : CLR_DARK_GREY);
    if (ready) circfill(34, PLAYFIELD_H + 4, 2, blink(10) ? CLR_BLUE : CLR_LIGHT_GREY);
    else {
        float p = 1.0f - clamp(pl.dash_cd / 1.1f, 0, 1);
        bar(40, PLAYFIELD_H + 2, 24, 4, p, CLR_BLUE, CLR_DARKER_GREY);
    }

    print_centered("REACH THE EXIT", SCREEN_W/2, PLAYFIELD_H + 1, CLR_LIME_GREEN);
    print_right(str("RUN %d  ASH %d", wins, deaths), SCREEN_W - 4, PLAYFIELD_H + 1, CLR_LIGHT_GREY);
}

static void panel(const char *title, int tcol, const char *sub, const char *prompt) {
    fade(0.45f);
    int w = 220, h = 70, x = SCREEN_W / 2 - w / 2, y = 56;
    rectfill(x, y, w, h, CLR_BROWNISH_BLACK);
    rect(x, y, w, h, tcol);
    print_scaled(title, SCREEN_W / 2 - text_width(title), y + 10, tcol, 2);
    print_centered(sub, SCREEN_W/2, y + 34, CLR_LIGHT_PEACH);
    print_centered(prompt, SCREEN_W/2, y + 52, blink(20) ? CLR_YELLOW : CLR_LIGHT_GREY);
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);

    // the stone floor under everything
    rectfill(0, 0, SCREEN_W, PLAYFIELD_H, CLR_DARKER_GREY);
    // a faint cool tile grid so motion reads
    for (int x = 0; x <= SCREEN_W; x += TILE) line(x, 0, x, PLAYFIELD_H, CLR_DARKER_PURPLE);
    for (int y = 0; y <= PLAYFIELD_H; y += TILE) line(0, y, SCREEN_W, y, CLR_DARKER_PURPLE);

    draw_walls();     // walls/exit (the fire is drawn over the floor, behind walls' faces)
    draw_fire();      // the live front — over floor + spilling against walls
    draw_player();
    draw_hud();

    if (state == ST_DEAD) {
        // a hot flash that fades over the death beat
        if (state_t < 0.3f) { fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, PLAYFIELD_H, CLR_ORANGE); fillp_reset(); }
        panel("ASHES", CLR_RED, "The front caught you.", "R / X to run again");
    } else if (state == ST_WIN) {
        panel("ESCAPED", CLR_LIME_GREEN, "You outran the fire.", "Z / R to run again");
    }
}
