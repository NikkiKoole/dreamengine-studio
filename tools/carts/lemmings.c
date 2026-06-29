/* de:meta
{
  "title": "Lemmings",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine",
    "tilemap-collision",
    "title-play-gameover-loop"
  ],
  "lineage": "Tribute to DMA Design's 1991 Lemmings; novel in that the terrain byte-grid is both the render buffer and the physics — diggers and bashers erase pixels that collision then reads, with no separate physics representation.",
  "genre": "puzzle",
  "homage": "Lemmings (1991)",
  "description": "A one-screen tribute to DMA Design's 1991 classic. A trapdoor drops a stream of brainless green-haired lemmings that trudge right, fall off ledges, splat in the lava pit, and turn at walls with zero self-preservation. The terrain IS the physics: a destructible pixel buffer where diggers and bashers erase dirt and collision just reads it back. Arm a job from the bottom toolbar then click a lemming to assign it — DIG straight down, BUILD a diagonal bridge, BLOCK as a living wall that turns the herd around, or BASH sideways through dirt — and steer enough of them into the glowing HOME exit before you run out. Save 10 of 20 to win. Controls: click a job tile in the bottom bar to arm it, click on a lemming to assign the armed job, right-click or click the armed tile again to disarm, R to restart."
}
de:meta */
#include "studio.h"
#include <stddef.h>

// ── LEMMINGS — a one-screen tribute to DMA Design's 1991 classic ──
// A trapdoor drops a stream of brainless green-haired lemmings. They
// trudge right, fall off ledges, and turn at walls — with zero sense
// of self-preservation. Your only power: arm a JOB from the bottom bar,
// then click a lemming to assign it. Steer enough of the herd into the
// glowing EXIT before you run out. Save the quota → WIN.
//
// The terrain IS the physics: a byte grid (SOLID/EMPTY). Diggers and
// bashers erase pixels; collision just reads them. Same data, both jobs.
//
//   • Click a job tile (bottom bar) to arm it
//   • Click on a lemming to assign the armed job
//   • Right-click / click the armed tile again = disarm
//   • R = restart

// terrain grid — drawn at 2x to fill the 320x200 canvas
#define GW 160
#define GH 92            // top 0 rows free, bottom bar lives below 184px
#define CELL 2
#define TOP_OFF 0        // terrain y=0 maps to screen y=0
#define BAR_Y 184        // toolbar top (screen px)

enum { EMPTY = 0, SOLID = 1, METAL = 2 };   // METAL = unbashable border/floor
static unsigned char terr[GW * GH];

// ── lemming pool ──────────────────────────────────────────────
#define MAX_LEM 40
enum { ST_DEAD, ST_FALL, ST_WALK, ST_DIG, ST_BUILD, ST_BLOCK, ST_BASH, ST_SAVED, ST_SPLAT };
typedef struct {
    int state;
    float x, y;          // feet position, in terrain-grid units (sub-pixel)
    int dir;             // -1 left, +1 right
    float fall;          // pixels fallen this drop (for splat)
    float acc;           // job tick accumulator
    int steps;           // build steps remaining
    float splat_t;       // splat anim timer
} Lem;
static Lem lems[MAX_LEM];

// ── jobs ──────────────────────────────────────────────────────
enum { JOB_NONE = -1, JOB_DIG, JOB_BUILD, JOB_BLOCK, JOB_BASH, NJOBS };
static const char *JNAME[NJOBS] = { "DIG", "BUILD", "BLOCK", "BASH" };
static int armed = JOB_NONE;

// ── level / game state ────────────────────────────────────────
#define M_TOTAL 20       // lemmings that will spawn
#define N_NEED  10       // needed to win
#define SPAWN_X 18
#define SPAWN_Y 8
#define EXIT_X  142
#define EXIT_Y  78
#define WALK_SPD 14.0f   // grid px / sec
#define FALL_SPD 60.0f
#define DIG_RATE 14.0f   // rows per sec
#define BASH_RATE 18.0f
#define BUILD_RATE 6.0f
#define MAX_FALL 26      // grid px — fall farther than this and you splat

static int g_state;      // 0 play, 1 win, 2 lose
static int spawned, saved, lost;
static float spawn_acc;
static int hover;        // lemming index under cursor, or -1
static float music_t;

// ── terrain helpers (the cart's own pget/pset) ────────────────
static int tget(int x, int y) {
    if (x < 0 || x >= GW || y < 0 || y >= GH) return SOLID;  // walls all around (off-screen = solid so they turn)
    return terr[y * GW + x];
}
static bool solid_at(int x, int y) { return tget(x, y) != EMPTY; }
static void tset(int x, int y, int v) {
    if (x < 0 || x >= GW || y < 0 || y >= GH) return;
    if (terr[y * GW + x] == METAL) return;   // metal never carved
    terr[y * GW + x] = v;
}

// ── build the single level ────────────────────────────────────
static void build_level(void) {
    for (int i = 0; i < GW * GH; i++) terr[i] = EMPTY;

    // metal frame at the very bottom so nothing digs out of the world
    for (int x = 0; x < GW; x++) { terr[(GH - 1) * GW + x] = METAL; terr[(GH - 2) * GW + x] = METAL; }

    // main ground slab (right half), with a gradient surface
    for (int y = 70; y < GH - 2; y++)
        for (int x = 0; x < GW; x++)
            terr[y * GW + x] = SOLID;

    // spawn ledge on the left up high
    for (int y = 16; y < 22; y++)
        for (int x = 8; x < 46; x++)
            terr[y * GW + x] = SOLID;
    // a short pillar/wall they must dig or bash through after a drop
    for (int y = 38; y < 71; y++)
        for (int x = 60; x < 72; x++)
            terr[y * GW + x] = SOLID;
    // a raised plateau in the middle
    for (int y = 52; y < 71; y++)
        for (int x = 84; x < 120; x++)
            terr[y * GW + x] = SOLID;

    // a lava gap in the floor between ledge and pillar (no floor → splat pit)
    for (int y = 70; y < GH - 2; y++)
        for (int x = 46; x < 60; x++)
            terr[y * GW + x] = EMPTY;

    // carve the exit basin so the floor is flat right at the exit
    for (int y = EXIT_Y - 2; y < EXIT_Y + 8; y++)
        for (int x = EXIT_X - 4; x < EXIT_X + 8; x++)
            if (y < GH - 2) terr[y * GW + x] = SOLID;
}

static void reset_game(void) {
    build_level();
    for (int i = 0; i < MAX_LEM; i++) lems[i].state = ST_DEAD;
    spawned = saved = lost = 0;
    spawn_acc = 0;
    armed = JOB_NONE;
    g_state = 0;
    hover = -1;
}

void init(void) {
    enable_pget(true);
    // a soft pluck for spawns / saves
    instrument(5, INSTR_SINE, 4, 60, 3, 120);
    instrument(6, INSTR_TRI, 2, 40, 4, 90);
    instrument(7, INSTR_NOISE, 1, 30, 0, 80);
    bpm(96);
    reset_game();
}

// is there floor directly under this lemming's feet?
static bool floor_under(Lem *L) {
    int fx = (int)L->x, fy = (int)L->y;
    return solid_at(fx, fy) || solid_at(fx, fy + 1);
}

static void spawn_one(void) {
    for (int i = 0; i < MAX_LEM; i++) {
        if (lems[i].state == ST_DEAD) {
            lems[i] = (Lem){0};
            lems[i].state = ST_FALL;
            lems[i].x = SPAWN_X;
            lems[i].y = SPAWN_Y;
            lems[i].dir = 1;
            spawned++;
            note(72, 5, 2);
            return;
        }
    }
}

static void do_splat(Lem *L) {
    L->state = ST_SPLAT;
    L->splat_t = 0;
    lost++;
    hit(40, 7, 4, 120);
    shake(2.5f);
}

static void do_save(Lem *L) {
    L->state = ST_SAVED;
    saved++;
    strum(76, CHORD_MAJ, 6, 4, 30);
}

// ── one lemming's tick ────────────────────────────────────────
static void tick_lem(Lem *L, float d) {
    int fx = (int)L->x, fy = (int)L->y;

    // reaching the exit (only when on the ground / walking near it)
    if (L->state != ST_FALL && L->state != ST_SPLAT && L->state != ST_SAVED) {
        if (abs(fx - EXIT_X) <= 3 && abs(fy - (EXIT_Y + 5)) <= 4) { do_save(L); return; }
    }

    switch (L->state) {
    case ST_FALL: {
        L->y += FALL_SPD * d;
        L->fall += FALL_SPD * d;
        if (floor_under(L)) {
            // snap onto the surface
            while (solid_at((int)L->x, (int)L->y)) L->y -= 1;
            if (L->fall > MAX_FALL) { do_splat(L); return; }
            L->state = ST_WALK;
            L->fall = 0;
        }
        break;
    }
    case ST_WALK: {
        if (!floor_under(L)) { L->state = ST_FALL; L->fall = 0; break; }
        float nx = L->x + L->dir * WALK_SPD * d;
        int ix = (int)nx, iy = (int)L->y;
        // wall ahead at body height? try to step up a small lip, else turn
        if (solid_at(ix, iy - 1) && solid_at(ix, iy - 2)) {
            // see if a 1-2px lip lets us climb
            if (!solid_at(ix, iy - 3) && !solid_at(ix, iy - 4)) {
                L->y -= 2; L->x = nx;            // step up the lip
            } else {
                L->dir = -L->dir;                // turn at the wall
            }
        } else {
            L->x = nx;
        }
        break;
    }
    case ST_DIG: {
        if (!solid_at((int)L->x, (int)L->y + 1) && !solid_at((int)L->x - 1, (int)L->y + 1) &&
            !solid_at((int)L->x + 1, (int)L->y + 1)) {
            L->state = ST_FALL; L->fall = 0; break;   // dug into open air
        }
        L->acc += DIG_RATE * d;
        while (L->acc >= 1.0f) {
            L->acc -= 1.0f;
            int dy = (int)L->y + 1;
            tset((int)L->x - 1, dy, EMPTY);
            tset((int)L->x,     dy, EMPTY);
            tset((int)L->x + 1, dy, EMPTY);
            L->y += 1;
            if (L->y > GH - 3) { do_splat(L); return; }
        }
        break;
    }
    case ST_BASH: {
        // chew sideways; finish when no more dirt ahead
        bool dirt = false;
        for (int yy = -4; yy <= 0; yy++) if (solid_at(fx + L->dir, fy + yy)) dirt = true;
        if (!dirt) { L->state = ST_WALK; break; }
        L->acc += BASH_RATE * d;
        while (L->acc >= 1.0f) {
            L->acc -= 1.0f;
            for (int yy = -5; yy <= 0; yy++) {
                tset(fx + L->dir, fy + yy, EMPTY);
            }
            L->x += L->dir;
            fx = (int)L->x;
        }
        if (!floor_under(L)) { L->state = ST_FALL; L->fall = 0; }
        break;
    }
    case ST_BUILD: {
        L->acc += BUILD_RATE * d;
        while (L->acc >= 1.0f && L->steps > 0) {
            L->acc -= 1.0f;
            // lay a 5-wide brick step, then move up-forward
            for (int k = 0; k < 5; k++) tset(fx + L->dir * k, fy, SOLID);
            L->x += L->dir * 2;
            L->y -= 1;
            fx = (int)L->x; fy = (int)L->y;
            // place the brick we now stand on
            for (int k = 0; k < 4; k++) tset(fx + L->dir * k, fy + 1, SOLID);
            L->steps--;
            note(64 + (10 - L->steps), 6, 2);
            // bumped head on a ceiling → stop building, turn
            if (solid_at(fx, fy - 2)) { L->dir = -L->dir; L->state = ST_WALK; break; }
        }
        if (L->steps <= 0) L->state = ST_WALK;
        break;
    }
    case ST_BLOCK:
        // a living wall: doesn't move. handled in turning logic below.
        if (!floor_under(L)) { L->state = ST_FALL; L->fall = 0; }
        break;
    case ST_SPLAT:
        L->splat_t += d;
        if (L->splat_t > 0.5f) L->state = ST_DEAD;
        break;
    case ST_SAVED:
    case ST_DEAD:
        break;
    }
}

// blockers turn nearby walkers around
static void apply_blockers(void) {
    for (int b = 0; b < MAX_LEM; b++) {
        if (lems[b].state != ST_BLOCK) continue;
        int bx = (int)lems[b].x, by = (int)lems[b].y;
        for (int i = 0; i < MAX_LEM; i++) {
            Lem *L = &lems[i];
            if (i == b || (L->state != ST_WALK)) continue;
            int lx = (int)L->x, ly = (int)L->y;
            if (abs(ly - by) <= 4 && abs(lx - bx) <= 3) {
                // turn the walker away from the blocker
                if (sgn(bx - lx) == L->dir || lx == bx) L->dir = -L->dir;
            }
        }
    }
}

// ── input: arm a job, assign to a lemming ─────────────────────
static bool can_assign(Lem *L) {
    return L->state == ST_WALK || L->state == ST_FALL ||
           L->state == ST_DIG  || L->state == ST_BASH ||
           L->state == ST_BUILD || L->state == ST_BLOCK;
}

static void try_assign(int mx, int my) {
    // bar lives in screen space; convert mouse to grid for the field
    int gx = mx / CELL, gy = my / CELL;
    int best = -1; float bestd = 9;
    for (int i = 0; i < MAX_LEM; i++) {
        if (!can_assign(&lems[i])) continue;
        float dd = distance(gx, gy, (int)lems[i].x, (int)lems[i].y - 4);
        if (dd < bestd) { bestd = dd; best = i; }
    }
    if (best < 0) return;
    Lem *L = &lems[best];
    switch (armed) {
    case JOB_DIG:   if (L->state == ST_WALK) { L->state = ST_DIG;  L->acc = 0; note(60, 6, 2); } break;
    case JOB_BASH:  if (L->state == ST_WALK) { L->state = ST_BASH; L->acc = 0; note(62, 6, 2); } break;
    case JOB_BLOCK: if (L->state == ST_WALK) { L->state = ST_BLOCK; note(57, 6, 2); } break;
    case JOB_BUILD: if (L->state == ST_WALK) { L->state = ST_BUILD; L->steps = 8; L->acc = 0; note(64, 6, 2); } break;
    }
}

void update(void) {
    if (g_state != 0) {
        if (key('R') || keyp('R')) reset_game();
        return;
    }
    if (key('R')) reset_game();

    float d = dt();

    // spawn stream
    if (spawned < M_TOTAL) {
        spawn_acc += d;
        if (spawn_acc >= 1.1f) { spawn_acc = 0; spawn_one(); }
    }

    apply_blockers();
    for (int i = 0; i < MAX_LEM; i++)
        if (lems[i].state != ST_DEAD) tick_lem(&lems[i], d);

    // hover detection (field only)
    hover = -1;
    int mx = mouse_x(), my = mouse_y();
    if (my < BAR_Y) {
        int gx = mx / CELL, gy = my / CELL;
        float bestd = 9;
        for (int i = 0; i < MAX_LEM; i++) {
            if (!can_assign(&lems[i])) continue;
            float dd = distance(gx, gy, (int)lems[i].x, (int)lems[i].y - 4);
            if (dd < bestd) { bestd = dd; hover = i; }
        }
    }

    // mouse: bar = arm job, field = assign
    if (mouse_pressed(MOUSE_RIGHT)) armed = JOB_NONE;
    if (mouse_pressed(MOUSE_LEFT)) {
        if (my >= BAR_Y) {
            int bw = SCREEN_W / NJOBS;
            int j = mx / bw;
            if (j >= 0 && j < NJOBS) armed = (armed == j) ? JOB_NONE : j;
            note(80, 6, 1);
        } else if (armed != JOB_NONE) {
            try_assign(mx, my);
        }
    }

    // win / lose
    if (saved >= N_NEED) { g_state = 1; strum(72, CHORD_MAJ7, 6, 5, 60); }
    else if (spawned >= M_TOTAL) {
        bool alive = false;
        for (int i = 0; i < MAX_LEM; i++) {
            int s = lems[i].state;
            if (s != ST_DEAD && s != ST_SAVED && s != ST_SPLAT) { alive = true; break; }
        }
        if (!alive) g_state = (saved >= N_NEED) ? 1 : 2;
    }

    // gentle music bed
    music_t += d;
    if (g_state == 0 && every(2)) tone(SCALE_PENTA, 3, INSTR_TRI, 1);
}

// ── draw ──────────────────────────────────────────────────────
static void draw_terrain(void) {
    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            int m = terr[y * GW + x];
            if (m == EMPTY) continue;
            int col;
            if (m == METAL) col = CLR_DARK_GREY;
            else {
                // textured dirt: lighter near the surface
                int above = (y > 0) ? terr[(y - 1) * GW + x] : SOLID;
                int h = (x * 13 + y * 7) & 3;
                if (above == EMPTY) col = CLR_LIME_GREEN;        // grassy top
                else col = (h == 0) ? CLR_DARK_BROWN : CLR_BROWN;
            }
            rectfill(x * CELL, y * CELL, CELL, CELL, col);
        }
    }
}

static void draw_lava(void) {
    // the open pit between ledge and pillar — draw a glowing lava floor
    int y0 = (GH - 4) * CELL;
    for (int x = 46; x < 60; x++) {
        int wob = (int)(sin_deg((x * 12 + frame() * 6)) * 1.5f);
        rectfill(x * CELL, y0 + wob, CELL, CELL * 3, blink(6) ? CLR_DARK_ORANGE : CLR_ORANGE);
    }
}

static void draw_lem(Lem *L) {
    int x = (int)(L->x * CELL), y = (int)(L->y * CELL);
    if (L->state == ST_SAVED || L->state == ST_DEAD) return;
    if (L->state == ST_SPLAT) {
        circfill(x, y, 3, CLR_RED);
        pset(x - 3, y - 2, CLR_DARK_RED); pset(x + 3, y - 1, CLR_DARK_RED);
        return;
    }
    int body = CLR_BLUE, hair = CLR_GREEN, skin = CLR_LIGHT_PEACH;
    // body (the iconic blue smock)
    bool walking = (L->state == ST_WALK);
    int bob = walking ? (anim(2, 8, (L->x) * 0.05f)) : 0;
    rectfill(x - 2, y - 7 + bob, 4, 5, body);   // smock
    rectfill(x - 1, y - 9, 2, 2, skin);          // face
    rectfill(x - 2, y - 11, 4, 2, hair);         // green hair
    // feet
    if (L->state != ST_FALL) {
        pset(x - 2, y - 1, body); pset(x + 1, y - 1, body);
    }
    // job indicators
    if (L->state == ST_DIG) { line(x - 2, y, x + 2, y + 2, CLR_YELLOW); }
    if (L->state == ST_BASH) { line(x + L->dir * 2, y - 4, x + L->dir * 5, y - 4, CLR_YELLOW); }
    if (L->state == ST_BUILD) { rectfill(x, y - 5, 2, 2, CLR_ORANGE); }
    if (L->state == ST_BLOCK) {
        // arms out — a living wall
        line(x - 4, y - 5, x + 4, y - 5, CLR_RED);
        rect(x - 3, y - 8, 6, 8, CLR_RED);
    }
}

static void draw_exit(void) {
    int ex = EXIT_X * CELL, ey = EXIT_Y * CELL;
    // a glowing portal / trapdoor home
    rectfill(ex - 6, ey, 12, 12, CLR_DARK_PURPLE);
    rect(ex - 6, ey, 12, 12, blink(10) ? CLR_PINK : CLR_PEACH);
    for (int i = 0; i < 3; i++) {
        int yy = ey + 10 - ((frame() / 3 + i * 4) % 12);
        pset(ex + rnd(7) - 3, yy, CLR_LIGHT_YELLOW);
    }
    print("HOME", ex - 7, ey - 9, CLR_PEACH);
}

static void draw_spawn(void) {
    int sx = SPAWN_X * CELL, sy = SPAWN_Y * CELL;
    rectfill(sx - 7, sy - 6, 14, 5, CLR_DARK_GREY);
    rectfill(sx - 6, sy - 1, 12, 2, CLR_LIGHT_GREY);
    line(sx, sy + 1, sx, sy + 6, CLR_YELLOW);   // drop chute
}

static void draw_bar(void) {
    rectfill(0, BAR_Y, SCREEN_W, SCREEN_H - BAR_Y, CLR_BROWNISH_BLACK);
    line(0, BAR_Y, SCREEN_W, BAR_Y, CLR_DARK_GREY);
    int bw = SCREEN_W / NJOBS;
    int mx = mouse_x(), my = mouse_y();
    for (int j = 0; j < NJOBS; j++) {
        int bx = j * bw;
        bool hov = my >= BAR_Y && mx >= bx && mx < bx + bw;
        if (j == armed) rectfill(bx + 1, BAR_Y + 1, bw - 2, SCREEN_H - BAR_Y - 2, CLR_TRUE_BLUE);
        else if (hov)   rectfill(bx + 1, BAR_Y + 1, bw - 2, SCREEN_H - BAR_Y - 2, CLR_DARKER_BLUE);
        rect(bx, BAR_Y, bw, SCREEN_H - BAR_Y, CLR_DARK_GREY);
        int tw = text_width(JNAME[j]);
        print(JNAME[j], bx + (bw - tw) / 2, BAR_Y + 6, j == armed ? CLR_YELLOW : CLR_LIGHT_GREY);
    }
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    // sky / cave backdrop
    rectfill(0, 0, SCREEN_W, BAR_Y, CLR_DARKER_PURPLE);
    for (int i = 0; i < 14; i++)
        pset((i * 53 + 17) % SCREEN_W, (i * 37) % 80, CLR_INDIGO);

    draw_lava();
    draw_terrain();
    draw_spawn();
    draw_exit();

    for (int i = 0; i < MAX_LEM; i++)
        if (lems[i].state != ST_DEAD) draw_lem(&lems[i]);

    // hover ring on the targeted lemming
    if (hover >= 0 && armed != JOB_NONE) {
        Lem *L = &lems[hover];
        circ((int)(L->x * CELL), (int)(L->y * CELL) - 4, 7, blink(4) ? CLR_WHITE : CLR_YELLOW);
    }

    draw_bar();

    // HUD
    int out = 0;
    for (int i = 0; i < MAX_LEM; i++) {
        int s = lems[i].state;
        if (s != ST_DEAD && s != ST_SAVED && s != ST_SPLAT) out++;
    }
    rectfill(0, 0, SCREEN_W, 10, CLR_BLACK);
    print(str("OUT %d", out), 4, 1, CLR_GREEN);
    print_centered(str("SAVED %d / %d", saved, N_NEED), SCREEN_W/2, 1, CLR_LIGHT_YELLOW);
    print_right(str("LEFT %d  LOST %d", M_TOTAL - spawned, lost), SCREEN_W - 4, 1, CLR_PEACH);

    if (g_state != 0) {
        int w = 220, bx = (SCREEN_W - w) / 2;
        rectfill(bx, 70, w, 44, g_state == 1 ? CLR_DARK_GREEN : CLR_DARK_PURPLE);
        rect(bx, 70, w, 44, CLR_WHITE);
        print_centered(g_state == 1 ? "ALL RIGHT!" : "OH NO!", SCREEN_W/2, 78, g_state == 1 ? CLR_LIME_GREEN : CLR_RED);
        print_centered(str("you saved %d of %d", saved, M_TOTAL), SCREEN_W/2, 92, CLR_LIGHT_YELLOW);
        print_centered("- press R to try again -", SCREEN_W/2, 102, CLR_LIGHT_GREY);
    }
}
