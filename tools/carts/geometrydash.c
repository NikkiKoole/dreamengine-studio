/* de:meta
{
  "title": "geometry dash",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "tilemap-collision",
    "particle-system",
    "state-machine",
    "title-play-gameover-loop"
  ],
  "lineage": "Homage to RobTop's Geometry Dash (2013); novel: a hand-authored tile array drives both collision and scrolling, with an explosion particle pool and a three-state (play/dead/won) loop.",
  "genre": "platformer",
  "homage": "Geometry Dash (2013)",
  "description": "One-button rhythm runner. You auto-run to the right over a driving beat; tap (or hold) to jump, and the cube flips as it flies. Touch a spike or crash into a block and you explode — instant respawn at the start, with an attempt counter and live progress %. Z / X / Up / arrows / , / . to jump, or tap the screen."
}
de:meta */
#include "studio.h"

// GEOMETRY DASH — a one-button rhythm runner.
// You auto-run to the right. Tap (and hold) to jump. Touch a spike or crash
// into a block and you blow up — instant respawn at the start. Ride the beat
// to the end of the level.
//
// JUMP: Z / X / Up / , / . / arrows  (any of them — or tap the screen)
// when you win or die you keep going automatically; finish the level to win.

#define ROWS        6
#define TILE        14
#define FLOOR_Y     170
#define MAXCOLS     240
#define S           12                 // cube size
#define SPEED      2.0f
#define GRAV       0.45f
#define JUMP      -5.2f
#define ROT_SPD    4.0f

#define ST_PLAY     0
#define ST_DEAD     1
#define ST_WON      2

static char  tiles[ROWS][MAXCOLS];     // '#' block, '^' spike, ' ' empty
static int   level_cols;

static float px, py, vy, ang;
static bool  grounded;
static int   state, dead_t, attempts, bi;

typedef struct { float x, y, vx, vy; int life, col; } Particle;
static Particle parts[96];

static int tile_top(int r) { return FLOOR_Y - (ROWS - r) * TILE; }

static void spawn(float x, float y, float vx, float vy, int life, int col) {
    for (int i = 0; i < 96; i++)
        if (parts[i].life <= 0) { parts[i] = (Particle){ x, y, vx, vy, life, col }; return; }
}

// ---- level authoring ------------------------------------------------------
static void gspike(int c, int n) { for (int k = 0; k < n; k++) if (c + k < MAXCOLS) tiles[ROWS - 1][c + k] = '^'; }
static void block(int c, int h, int w) {
    for (int r = 0; r < h; r++)
        for (int k = 0; k < w; k++)
            if (c + k < MAXCOLS) tiles[ROWS - 1 - r][c + k] = '#';
}

static void build_level(void) {
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < MAXCOLS; c++) tiles[r][c] = ' ';
    int c = 22;
    gspike(c, 1); c += 11;
    gspike(c, 1); c += 11;
    gspike(c, 2); c += 14;
    gspike(c, 1); c += 12;
    block(c, 1, 4); c += 4 + 7;
    gspike(c, 1); c += 11;
    gspike(c, 2); c += 14;
    gspike(c, 1); c += 11;
    block(c, 1, 5); c += 5 + 8;
    gspike(c, 1); c += 11;
    gspike(c, 2); c += 14;
    gspike(c, 1); c += 11;
    gspike(c, 1); c += 11;
    block(c, 1, 4); c += 4 + 7;
    gspike(c, 2); c += 14;
    gspike(c, 1); c += 12;
    level_cols = c + 8;
}

static void reset_run(void) {
    px = 24; py = FLOOR_Y - S; vy = 0; ang = 0; grounded = true;
    state = ST_PLAY; bi = 0;
    for (int i = 0; i < 96; i++) parts[i].life = 0;
}

void init(void) { build_level(); attempts = 1; reset_run(); }

static bool jumping(void) {
    return btn(0, BTN_A) || btn(0, BTN_B) || btn(0, BTN_UP)
        || btn(1, BTN_A) || btn(1, BTN_B) || btn(1, BTN_UP)
        || touch_count() > 0;
}

static bool solid(int c, int r) { return c >= 0 && c < MAXCOLS && r >= 0 && r < ROWS && tiles[r][c] == '#'; }
static bool spike(int c, int r) { return c >= 0 && c < MAXCOLS && r >= 0 && r < ROWS && tiles[r][c] == '^'; }

static void die(void) {
    state = ST_DEAD; dead_t = 0;
    for (int i = 0; i < 30; i++)
        spawn(px + S / 2, py + S / 2, rnd_float_between(-3, 3), rnd_float_between(-3, 2),
              rnd_between(16, 34), (i & 1) ? CLR_YELLOW : CLR_WHITE);
    hit(40, INSTR_NOISE, 5, 220);
}

void update(void) {
    // particles always tick
    for (int i = 0; i < 96; i++)
        if (parts[i].life > 0) { parts[i].x += parts[i].vx; parts[i].y += parts[i].vy;
                                 parts[i].vy += 0.15f; parts[i].life--; }

    if (state == ST_DEAD) { if (++dead_t > 24) { attempts++; reset_run(); } return; }
    if (state == ST_WON)  { if (jumping()) init(); return; }

    // ---- beat ----
    bpm(150);
    if (every(1)) {
        int bass[8] = { 36, 36, 43, 36, 41, 36, 39, 43 };
        hit(bass[bi % 8], INSTR_TRI, 4, 90);
        note(bass[bi % 8] + 12, INSTR_SAW, 2);
        bi++;
    }
    if (euclid(3, 8, beat())) hit(78, INSTR_NOISE, 1, 28);

    // ---- jump ----
    if (grounded && jumping()) { vy = JUMP; grounded = false;
                                 hit(72, INSTR_SQUARE, 2, 40); }

    // ---- vertical: gravity, floor, land on block tops ----
    float oldBottom = py + S;
    vy += GRAV;
    py += vy;
    grounded = false;
    if (py + S >= FLOOR_Y) { py = FLOOR_Y - S; vy = 0; grounded = true; }

    if (vy >= 0) {
        int c0 = (int)px / TILE, c1 = (int)(px + S - 1) / TILE, best = 9999;
        for (int r = 0; r < ROWS; r++)
            for (int c = c0; c <= c1; c++)
                if (solid(c, r)) {
                    int tt = tile_top(r);
                    if (oldBottom <= tt + 2 && py + S >= tt && tt < best) best = tt;
                }
        if (best < 9999) { py = best - S; vy = 0; grounded = true; }
    }
    if (grounded) ang = ((int)((ang + 45) / 90)) * 90.0f;   // snap rotation on landing
    else          ang += ROT_SPD;

    // ---- advance + trail ----
    px += SPEED;
    if (frame() % 2 == 0) spawn(px + 1, py + S - 2, -0.5f, rnd_float_between(-0.4f, 0.4f), 14, CLR_ORANGE);

    // ---- collision / death ----
    int c0 = (int)px / TILE, c1 = (int)(px + S - 1) / TILE;
    for (int r = 0; r < ROWS; r++) {
        int tt = tile_top(r);
        if (py + S <= tt || py >= tt + TILE) continue;       // no vertical overlap
        for (int c = c0; c <= c1; c++) {
            if (spike(c, r) && boxes_touch((int)px + 1, (int)py + 1, S - 2, S - 2,
                                           c * TILE + 3, tt + 4, TILE - 6, TILE - 4)) { die(); return; }
            if (solid(c, r) && py + S > tt + 2) { die(); return; }   // hit a block's side
        }
    }

    if (px >= (level_cols - 2) * TILE) { state = ST_WON; strum(60, CHORD_MAJ7, INSTR_SQUARE, 5, 60); }
}

static void draw_cube(float cx, float cy, float a, int fill, int edge) {
    float h = S / 2.0f, cs = cos_deg(a), sn = sin_deg(a);
    float lx[4] = { -h, h, h, -h }, ly[4] = { -h, -h, h, h };
    int X[4], Y[4];
    for (int k = 0; k < 4; k++) { X[k] = (int)(cx + lx[k] * cs - ly[k] * sn);
                                  Y[k] = (int)(cy + lx[k] * sn + ly[k] * cs); }
    trifill(X[0], Y[0], X[1], Y[1], X[2], Y[2], fill);
    trifill(X[0], Y[0], X[2], Y[2], X[3], Y[3], fill);
    for (int k = 0; k < 4; k++) line(X[k], Y[k], X[(k + 1) % 4], Y[(k + 1) % 4], edge);
    circfill((int)cx, (int)cy, 2, edge);
}

void draw(void) {
    cls(CLR_TRUE_BLUE);

    int camx = (int)px + S / 2 - SCREEN_W / 3;
    camx = mid(0, camx, level_cols * TILE - SCREEN_W);
    camera(camx, 0);

    // scrolling background grid
    int gx0 = (camx / TILE) * TILE;
    for (int x = gx0; x < camx + SCREEN_W + TILE; x += TILE) line(x, 0, x, FLOOR_Y, CLR_DARKER_BLUE);

    // floor
    rectfill(camx, FLOOR_Y, SCREEN_W, SCREEN_H - FLOOR_Y, CLR_DARK_BLUE);
    line(camx, FLOOR_Y, camx + SCREEN_W, FLOOR_Y, CLR_BLUE);

    // obstacles in view
    int c0 = camx / TILE - 1, c1 = (camx + SCREEN_W) / TILE + 1;
    for (int c = c0; c <= c1; c++)
        for (int r = 0; r < ROWS; r++) {
            int x = c * TILE, tt = tile_top(r);
            if (solid(c, r)) { rectfill(x, tt, TILE, TILE, CLR_BLACK);
                               line(x, tt, x + TILE - 1, tt, CLR_WHITE);
                               rect(x, tt, TILE, TILE, CLR_DARKER_BLUE); }
            else if (spike(c, r)) {
                trifill(x + 1, tt + TILE, x + TILE / 2, tt + 1, x + TILE - 1, tt + TILE, CLR_WHITE);
                tri(x + 1, tt + TILE, x + TILE / 2, tt + 1, x + TILE - 1, tt + TILE, CLR_LIGHT_GREY);
            }
        }

    // particles
    for (int i = 0; i < 96; i++)
        if (parts[i].life > 0) pset((int)parts[i].x, (int)parts[i].y, parts[i].col);

    // cube
    if (state != ST_DEAD)
        draw_cube(px + S / 2, py + S / 2, ang, CLR_YELLOW, CLR_WHITE);

    camera(0, 0);

    // HUD — progress bar
    int prog = (int)(px * 100 / (level_cols * TILE));
    if (prog > 100) prog = 100;
    rectfill(40, 6, 240, 6, CLR_DARKER_BLUE);
    rectfill(40, 6, 240 * prog / 100, 6, CLR_YELLOW);
    rect(40, 6, 240, 6, CLR_WHITE);
    print(str("%d%%", prog), 286, 5, CLR_WHITE);
    print(str("ATT %d", attempts), 6, 5, CLR_LIGHT_GREY);

    if (state == ST_WON) {
        rectfill(90, 80, 140, 40, CLR_BLACK);
        rect(90, 80, 140, 40, CLR_YELLOW);
        print_centered("LEVEL COMPLETE!", SCREEN_W/2, 90, CLR_YELLOW);
        print_centered(str("attempts: %d", attempts), SCREEN_W/2, 102, CLR_LIGHT_GREY);
    }
}
