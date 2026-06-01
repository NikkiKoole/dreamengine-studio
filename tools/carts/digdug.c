#include "studio.h"
#include <string.h>

// DIG DUG
// Tunnel through solid earth, carving passages as you move. Two monster types:
// round red Pooka and green fire-breathing Fygar. Pop a monster by holding the
// harpoon to inflate it (Z), or dig out the dirt beneath a rock so it crashes
// down. Clear every monster to dig deeper. Monsters can ghost through the dirt.
// Arrows move/dig, Z pumps. Best score saved.

#define MW   22
#define MH   13
#define TILE 14
#define X0   6
#define Y0   18

#define DIG_SPD   3.6f    // tiles per second (player)
#define MON_SPD   2.0f    // monster tiles per second
#define GHOST_SPD 1.3f
#define ROCK_FALL 5.5f

enum { UP, RIGHT, DOWN, LEFT };
static const int DX[4] = { 0, 1, 0, -1 };
static const int DY[4] = { -1, 0, 1, 0 };

// grid cells
enum { DIRT = '#', OPEN = '.', ROCK = 'O' };

enum { POOKA, FYGAR };
enum { M_DIRT, M_TUNNEL, M_GHOST, M_INFLATE, M_DEAD };

typedef struct {
    int   tx, ty, dir;
    float prog;          // 0..1 between tiles
    int   facing;
    bool  pumping;
    float pump_charge;   // visual harpoon reach
} Player;

typedef struct {
    int   type;
    int   tx, ty, dir;
    float prog;
    int   state;
    int   inflate;       // 0..4, 4 = pop
    float deflate_t;     // when to deflate one stage if not pumped
    float ghost_t;       // when it may go ghost
    float fire_t;        // fygar breath timer
    bool  firing;
    bool  alive;
    // ghost target captured at entry
    int   gtx, gty;
} Mon;

#define MAXMON 8
#define MAXROCK 6

typedef struct {
    int   tx;
    float fy;            // float y in tiles
    int   state;         // 0 rest, 1 wobble, 2 fall, 3 broken
    float t;
    bool  active;
} Rock;

static char   grid[MH][MW];
static Player pl;
static Mon    mon[MAXMON];
static Rock   rock[MAXROCK];
static int    nmon, nrock;

static int    score, hiscore, lives, level, alive_count;
static int    state;          // 0 play, 1 die-pause, 2 clear, 3 gameover
static float  state_until;
static int    pump_stage_snd;

// ====================================================================

static bool inb(int tx, int ty) { return tx >= 0 && tx < MW && ty >= 0 && ty < MH; }
static bool is_open(int tx, int ty) { return inb(tx, ty) && grid[ty][tx] != DIRT && grid[ty][tx] != ROCK; }
static bool is_dirt(int tx, int ty) { return inb(tx, ty) && grid[ty][tx] == DIRT; }

static int px_of(int tx, int dir, float prog) {
    return X0 + tx * TILE + TILE / 2 + (dir >= 0 ? (int)(DX[dir] * prog * TILE) : 0);
}
static int py_of(int ty, int dir, float prog) {
    return Y0 + ty * TILE + TILE / 2 + (dir >= 0 ? (int)(DY[dir] * prog * TILE) : 0);
}

// ====================================================================
// level build
// ====================================================================

static void carve(int tx, int ty) { if (inb(tx, ty) && grid[ty][tx] == DIRT) grid[ty][tx] = OPEN; }

static void build_level() {
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++)
            grid[y][x] = (y == 0) ? OPEN : DIRT;   // open sky strip on top row

    // player start nook: a vertical shaft from the top + small pocket
    int psx = MW / 2;
    pl.tx = psx; pl.ty = 2; pl.dir = -1; pl.prog = 0; pl.facing = DOWN;
    pl.pumping = false; pl.pump_charge = 0;
    for (int y = 0; y <= pl.ty; y++) carve(psx, y);
    carve(psx - 1, pl.ty);
    carve(psx + 1, pl.ty);

    // monsters: nest each in a small open pocket, spread across depth
    nmon = mid(3, 3 + level, MAXMON);
    alive_count = nmon;
    for (int i = 0; i < nmon; i++) {
        int mx, my, tries = 0;
        do {
            mx = rnd_between(2, MW - 2);
            my = rnd_between(4, MH - 1);
            tries++;
        } while (tries < 40 && (abs(mx - psx) < 3 && my < 5));
        carve(mx, my);
        Mon *m = &mon[i];
        m->type = (i % 3 == 2) ? FYGAR : POOKA;   // ~1/3 fygar
        m->tx = mx; m->ty = my; m->dir = LEFT; m->prog = 0;
        m->state = M_TUNNEL; m->inflate = 0; m->deflate_t = 0;
        m->ghost_t = now() + rnd_float_between(6.0f, 12.0f);
        m->fire_t = now() + rnd_float_between(2.0f, 5.0f);
        m->firing = false; m->alive = true;
    }

    // rocks: drop a few into solid dirt, not in the start shaft
    nrock = mid(2, 2 + level / 2, MAXROCK);
    for (int i = 0; i < nrock; i++) {
        int rx, ry, tries = 0;
        do {
            rx = rnd_between(2, MW - 2);
            ry = rnd_between(3, MH - 3);
            tries++;
        } while (tries < 40 && (rx == psx || grid[ry][rx] != DIRT));
        grid[ry][rx] = ROCK;
        rock[i].tx = rx; rock[i].fy = ry; rock[i].state = 0; rock[i].t = 0; rock[i].active = true;
    }
}

static void build_level_keep(void);
static void next_level() { level++; build_level(); state = 0; }

static void reset_game() {
    score = 0; lives = 3; level = 1; build_level(); state = 0;
}

void init() { hiscore = load(0); reset_game(); }

// ====================================================================
// sounds
// ====================================================================

static void snd_step()    { hit(48 + rnd(4), INSTR_SQUARE, 1, 25); }
static void snd_pump(int stage) { note(56 + stage * 4, INSTR_SQUARE, 3); }
static void snd_pop()     { note(72, INSTR_SAW, 5); note(60, INSTR_SAW, 4); }
static void snd_rock()    { hit(34, INSTR_NOISE, 6, 220); shake(4); }
static void snd_fire()    { hit(40, INSTR_NOISE, 4, 180); }
static void snd_die()     { note(45, INSTR_SAW, 6); note(33, INSTR_SAW, 6); }
static void snd_clear()   { for (int i = 0; i < 4; i++) schedule(i * 120, 60 + i * 5, INSTR_SQUARE, 4); }

// ====================================================================
// monster ai
// ====================================================================

static void mon_choose(Mon *m) {
    // greedy: pick the open direction that reduces distance to player; bias forward
    int best = -1; long bestd = 1L << 40;
    int rev = (m->dir + 2) % 4;
    for (int d = 0; d < 4; d++) {
        int nx = m->tx + DX[d], ny = m->ty + DY[d];
        if (!is_open(nx, ny)) continue;
        long dd = (long)(nx - pl.tx) * (nx - pl.tx) + (long)(ny - pl.ty) * (ny - pl.ty);
        if (d == rev) dd += 6;          // dislike reversing
        if (dd < bestd) { bestd = dd; best = d; }
    }
    if (best < 0) best = rev;
    m->dir = best;
}

static void mon_update(Mon *m) {
    if (!m->alive) return;
    float t = now();

    if (m->state == M_INFLATE) {
        // deflate over time if not being pumped this frame (handled in update via pumping)
        if (t > m->deflate_t && m->inflate > 0) {
            m->inflate--;
            m->deflate_t = t + 0.6f;
            if (m->inflate == 0) m->state = M_TUNNEL;
        }
        return;
    }
    if (m->state == M_DEAD) return;

    // chance to ghost: go through dirt straight toward player
    if (m->state == M_TUNNEL && t > m->ghost_t && m->prog == 0) {
        // only ghost if player is not adjacent in open tunnel
        m->state = M_GHOST;
        // head roughly toward player, cardinal
        if (abs(pl.tx - m->tx) > abs(pl.ty - m->ty)) m->dir = pl.tx > m->tx ? RIGHT : LEFT;
        else                                          m->dir = pl.ty > m->ty ? DOWN : UP;
        m->ghost_t = t + rnd_float_between(8.0f, 14.0f);
    }

    float spd = (m->state == M_GHOST) ? GHOST_SPD : MON_SPD;
    spd += level * 0.12f;

    if (m->prog == 0) {
        if (m->state == M_GHOST) {
            // re-aim each tile toward player; rematerialize when on open tunnel
            if (is_open(m->tx, m->ty) && distance(m->tx, m->ty, pl.tx, pl.ty) > 1.5f) {
                m->state = M_TUNNEL;
            } else {
                if (abs(pl.tx - m->tx) >= abs(pl.ty - m->ty)) m->dir = pl.tx >= m->tx ? RIGHT : LEFT;
                else                                           m->dir = pl.ty >= m->ty ? DOWN : UP;
                // don't leave the grid
                if (!inb(m->tx + DX[m->dir], m->ty + DY[m->dir])) m->state = M_TUNNEL;
            }
        } else {
            mon_choose(m);
        }
    }

    // fygar fire (only when in tunnel, facing player along a row)
    if (m->type == FYGAR && m->state == M_TUNNEL) {
        if (t > m->fire_t && m->ty == pl.ty && abs(m->tx - pl.tx) <= 3 && m->prog == 0) {
            m->firing = true; m->fire_t = t + rnd_float_between(3.0f, 6.0f);
            m->dir = pl.tx > m->tx ? RIGHT : LEFT;
            snd_fire();
            // damage if player in the breath line and tiles open
            bool clear = true;
            int sx = pl.tx > m->tx ? 1 : -1;
            for (int x = m->tx + sx; x != pl.tx; x += sx) if (!is_open(x, m->ty)) { clear = false; break; }
            if (clear && pl.dir < 0) { /* hit handled in collision below via flag */ }
        } else if (t > m->fire_t - 2.5f) {
            m->firing = false;
        }
    }

    m->prog += spd * dt();
    if (m->prog >= 1.0f) {
        m->prog = 0;
        m->tx += DX[m->dir];
        m->ty += DY[m->dir];
    }
}

// ====================================================================
// rocks
// ====================================================================

static void rock_update(Rock *r) {
    if (!r->active) return;
    int ry = (int)(r->fy + 0.001f);

    if (r->state == 0) {                 // resting — fall if dirt below is gone
        if (is_open(r->tx, ry + 1)) { r->state = 1; r->t = now() + 0.5f; }
    } else if (r->state == 1) {          // wobble
        if (now() > r->t) {
            if (is_open(r->tx, ry + 1)) { r->state = 2; grid[ry][r->tx] = OPEN; snd_rock(); }
            else r->state = 0;
        }
    } else if (r->state == 2) {          // falling
        r->fy += ROCK_FALL * dt();
        int ny = (int)(r->fy + 0.5f);
        // kill monsters / player in path
        for (int i = 0; i < nmon; i++) {
            Mon *m = &mon[i];
            if (m->alive && m->state != M_DEAD && m->tx == r->tx && m->ty == ny) {
                m->alive = false; m->state = M_DEAD; alive_count--;
                score += m->type == FYGAR ? 500 : 300;
            }
        }
        if (state == 0 && pl.tx == r->tx && pl.ty == ny) {
            r->state = 3; r->t = now() + 1.0f;
            lives--; snd_die(); fade(0.0f);
            state = (lives <= 0) ? 3 : 1; state_until = now() + 1.4f;
            if (lives <= 0 && score > hiscore) { hiscore = score; save(0, hiscore); }
            return;
        }
        if (!is_open(r->tx, ny + 1) || ny >= MH - 1) {
            r->fy = ny; r->state = 3; r->t = now() + 0.8f;
            snd_rock();
        }
    } else if (r->state == 3) {          // shattered
        if (now() > r->t) r->active = false;
    }
}

// ====================================================================
// update
// ====================================================================

static void try_pump() {
    int fx = pl.tx + DX[pl.facing], fy = pl.ty + DY[pl.facing];
    int fx2 = pl.tx + 2 * DX[pl.facing], fy2 = pl.ty + 2 * DY[pl.facing];
    Mon *target = NULL;
    for (int i = 0; i < nmon; i++) {
        Mon *m = &mon[i];
        if (!m->alive || m->state == M_DEAD || m->state == M_GHOST) continue;
        if ((m->tx == fx && m->ty == fy) || (m->tx == fx2 && m->ty == fy2)) { target = m; break; }
    }
    if (!target) return;
    target->state = M_INFLATE;
    if (target->inflate < 4) {
        // inflate steadily while held — gated by deflate_t reuse as pump cooldown
        if (now() > target->deflate_t) {
            target->inflate++;
            target->deflate_t = now() + 0.35f;
            snd_pump(target->inflate);
            if (target->inflate >= 4) {
                target->alive = false; target->state = M_DEAD; alive_count--;
                score += target->type == FYGAR ? 400 : 200;
                snd_pop();
            }
        }
    }
}

void update() {
    if (state == 1) {                      // death pause → respawn
        if (now() > state_until) { build_level_keep(); }
        return;
    }
    if (state == 2) {                      // level clear
        if (now() > state_until && (btnp(0, BTN_A) || btnp(0, BTN_B))) next_level();
        return;
    }
    if (state == 3) {                      // game over
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) reset_game();
        return;
    }

    // --- player move/dig ---
    int want = -1;
    if (btn(0, BTN_UP))    want = UP;
    if (btn(0, BTN_DOWN))  want = DOWN;
    if (btn(0, BTN_LEFT))  want = LEFT;
    if (btn(0, BTN_RIGHT)) want = RIGHT;

    pl.pumping = btn(0, BTN_A);

    if (!pl.pumping) {
        if (pl.prog == 0) {
            if (want >= 0) {
                pl.facing = want;
                int nx = pl.tx + DX[want], ny = pl.ty + DY[want];
                if (inb(nx, ny) && grid[ny][nx] != ROCK) {
                    bool wasdirt = is_dirt(nx, ny);
                    carve(nx, ny);
                    pl.dir = want;
                    if (wasdirt) snd_step();
                } else pl.dir = -1;
            } else pl.dir = -1;
        }
        if (pl.dir >= 0) {
            pl.prog += DIG_SPD * dt();
            if (pl.prog >= 1.0f) {
                pl.prog = 0;
                pl.tx += DX[pl.dir]; pl.ty += DY[pl.dir];
                pl.dir = -1;
            }
        }
        pl.pump_charge = 0;
    } else {
        pl.dir = -1; pl.prog = 0;
        pl.pump_charge = clamp(pl.pump_charge + dt() * 4.0f, 0, 1);
        try_pump();
    }

    // --- monsters ---
    for (int i = 0; i < nmon; i++) {
        Mon *m = &mon[i];
        // deflate inflated monsters that aren't currently being pumped
        if (m->state == M_INFLATE && !pl.pumping) {
            if (now() > m->deflate_t && m->inflate > 0) {
                m->inflate--; m->deflate_t = now() + 0.6f;
                if (m->inflate == 0) m->state = M_TUNNEL;
            }
            continue;
        }
        if (m->state == M_INFLATE && pl.pumping) continue; // frozen while pumped
        mon_update(m);

        // collision with player → death (unless pumping that monster, handled above)
        if (state == 0 && m->alive && m->state != M_DEAD && m->state != M_INFLATE) {
            int mpx = px_of(m->tx, m->dir, m->prog), mpy = py_of(m->ty, m->dir, m->prog);
            int ppx = px_of(pl.tx, pl.dir, pl.prog), ppy = py_of(pl.ty, pl.dir, pl.prog);
            if (near(mpx, mpy, ppx, ppy, 8)) {
                lives--; snd_die();
                state = (lives <= 0) ? 3 : 1; state_until = now() + 1.4f;
                if (lives <= 0 && score > hiscore) { hiscore = score; save(0, hiscore); }
                return;
            }
        }
        // fygar breath collision
        if (m->firing && m->type == FYGAR && m->ty == pl.ty) {
            int sx = m->dir == RIGHT ? 1 : -1;
            bool clear = true;
            for (int x = m->tx + sx; x != pl.tx; x += sx) {
                if (x < 0 || x >= MW) { clear = false; break; }
                if (!is_open(x, m->ty)) { clear = false; break; }
            }
            if (clear && ((sx > 0 && pl.tx > m->tx && pl.tx <= m->tx + 3) ||
                          (sx < 0 && pl.tx < m->tx && pl.tx >= m->tx - 3))) {
                lives--; snd_die();
                state = (lives <= 0) ? 3 : 1; state_until = now() + 1.4f;
                if (lives <= 0 && score > hiscore) { hiscore = score; save(0, hiscore); }
                return;
            }
        }
    }

    // --- rocks ---
    for (int i = 0; i < nrock; i++) rock_update(&rock[i]);

    // --- win check ---
    if (alive_count <= 0) {
        state = 2; state_until = now() + 0.6f; score += 1000;
        snd_clear();
        if (score > hiscore) { hiscore = score; save(0, hiscore); }
    }
}

// rebuild positions but keep score/level/lives after a death
static void build_level_keep() {
    int s = score, lv = level, li = lives;
    build_level();
    score = s; level = lv; lives = li;
    state = 0;
}

// ====================================================================
// drawing
// ====================================================================

static const int BAND[5] = { CLR_BROWN, CLR_DARK_BROWN, CLR_BROWNISH_BLACK, CLR_DARKER_PURPLE, CLR_DARKER_GREY };

static void draw_dirt() {
    for (int y = 0; y < MH; y++) {
        int band = mid(0, y / 3, 4);
        int col = BAND[band];
        for (int x = 0; x < MW; x++) {
            int sx = X0 + x * TILE, sy = Y0 + y * TILE;
            char c = grid[y][x];
            if (c == DIRT) {
                rectfill(sx, sy, TILE, TILE, col);
                // speckle texture
                pset(sx + 3, sy + 4, CLR_DARK_BROWN == col ? CLR_BROWNISH_BLACK : CLR_DARK_BROWN);
                pset(sx + 9, sy + 9, CLR_BROWNISH_BLACK);
            } else if (c == OPEN || c == ROCK) {
                rectfill(sx, sy, TILE, TILE, CLR_BLACK);
            }
        }
    }
}

static void draw_player() {
    int x = px_of(pl.tx, pl.dir, pl.prog), y = py_of(pl.ty, pl.dir, pl.prog);

    // harpoon when pumping
    if (pl.pumping) {
        int len = 6 + (int)(pl.pump_charge * 10);
        int hx = x + (int)dx(len, pl.facing == RIGHT ? 0 : pl.facing == DOWN ? 90 : pl.facing == LEFT ? 180 : 270);
        int hy = y + (int)dy(len, pl.facing == RIGHT ? 0 : pl.facing == DOWN ? 90 : pl.facing == LEFT ? 180 : 270);
        line(x, y, hx, hy, CLR_LIGHT_GREY);
        rectfill(hx - 1, hy - 1, 3, 3, CLR_WHITE);
    }

    // body — white-suited digger with a round helmet
    circfill(x, y, 5, CLR_WHITE);
    rectfill(x - 4, y, 8, 6, CLR_WHITE);
    // helmet + visor
    arcfill(x, y - 1, 5, 180, 360, CLR_BLUE);
    pset(x - 2, y - 1, CLR_BLACK);
    pset(x + 2, y - 1, CLR_BLACK);
    // little feet
    rectfill(x - 4, y + 5, 3, 2, CLR_BLUE);
    rectfill(x + 1, y + 5, 3, 2, CLR_BLUE);
}

static void draw_mon(Mon *m) {
    if (!m->alive && m->state != M_DEAD) return;
    if (m->state == M_DEAD) return;
    int x = px_of(m->tx, m->dir, m->prog), y = py_of(m->ty, m->dir, m->prog);
    int body = m->type == FYGAR ? CLR_GREEN : CLR_RED;
    int r = 5 + m->inflate;   // inflate grows the body

    if (m->state == M_GHOST) {
        // floating eyes only
        circfill(x - 2, y, 2, CLR_WHITE);
        circfill(x + 2, y, 2, CLR_WHITE);
        pset(x - 2, y, CLR_DARK_BLUE);
        pset(x + 2, y, CLR_DARK_BLUE);
        return;
    }

    if (m->state == M_INFLATE && m->inflate > 0) {
        // swelling balloon, flashing
        if (blink(4)) pal(body, CLR_WHITE);
        circfill(x, y, r, body);
        pal_reset();
    } else {
        circfill(x, y, r, body);
    }

    // goggle eyes
    int ex = DX[m->dir < 0 ? LEFT : m->dir], ey = DY[m->dir < 0 ? LEFT : m->dir];
    circfill(x - 2, y - 1, 2, CLR_WHITE);
    circfill(x + 2, y - 1, 2, CLR_WHITE);
    pset(x - 2 + ex, y - 1 + ey, CLR_DARK_BLUE);
    pset(x + 2 + ex, y - 1 + ey, CLR_DARK_BLUE);

    if (m->type == FYGAR) {
        // little spikes + fire breath
        pset(x, y - r, CLR_YELLOW);
        if (m->firing) {
            int sx = m->dir == RIGHT ? 1 : -1;
            int fx = x + sx * (r + 2);
            for (int s = 0; s < 3; s++) {
                trifill(fx + sx * s * 6, y - 3, fx + sx * s * 6, y + 3,
                        fx + sx * (s * 6 + 6), y, blink(3) ? CLR_ORANGE : CLR_YELLOW);
            }
        }
    }
}

static void draw_rock(Rock *r) {
    if (!r->active) return;
    int x = X0 + r->tx * TILE + TILE / 2;
    int y = Y0 + (int)(r->fy * TILE) + TILE / 2;
    if (r->state == 3) {
        // shatter
        for (int i = 0; i < 6; i++) {
            int a = i * 60;
            circfill(x + (int)dx(6, a), y + (int)dy(6, a), 1, CLR_LIGHT_GREY);
        }
        return;
    }
    int wob = (r->state == 1) ? (blink(3) ? 1 : -1) : 0;
    circfill(x + wob, y, 6, CLR_MEDIUM_GREY);
    circ(x + wob, y, 6, CLR_LIGHT_GREY);
    pset(x + wob - 2, y - 2, CLR_WHITE);
}

void draw() {
    cls(CLR_DARK_BLUE);                    // sky

    draw_dirt();

    for (int i = 0; i < nrock; i++) draw_rock(&rock[i]);
    for (int i = 0; i < nmon; i++) draw_mon(&mon[i]);
    if (state != 1) draw_player();

    // HUD
    rectfill(0, 0, SCREEN_W, Y0, CLR_BLACK);
    print(str("SCORE %d", score), 3, 2, CLR_WHITE);
    print_centered(str("DEPTH %d", level), 2, CLR_LIGHT_YELLOW);
    print_right(str("HI %d", hiscore), SCREEN_W - 3, 2, CLR_YELLOW);
    for (int i = 0; i < lives; i++) circfill(SCREEN_W - 10 - i * 12, 11, 4, CLR_WHITE);
    print(str("LEFT %d", alive_count), 3, 10, CLR_LIME_GREEN);

    if (state == 2) {
        rectfill(SCREEN_W / 2 - 64, SCREEN_H / 2 - 14, 128, 30, CLR_BLACK);
        rect    (SCREEN_W / 2 - 64, SCREEN_H / 2 - 14, 128, 30, CLR_GREEN);
        print_centered("LAYER CLEAR!", SCREEN_H / 2 - 8, CLR_LIME_GREEN);
        print_centered("Z to dig deeper", SCREEN_H / 2 + 4, CLR_LIGHT_GREY);
    }
    if (state == 3) {
        fade(0.4f);
        rectfill(SCREEN_W / 2 - 64, SCREEN_H / 2 - 14, 128, 30, CLR_BLACK);
        rect    (SCREEN_W / 2 - 64, SCREEN_H / 2 - 14, 128, 30, CLR_RED);
        print_centered("GAME OVER", SCREEN_H / 2 - 8, CLR_RED);
        print_centered("Z to play again", SCREEN_H / 2 + 4, CLR_LIGHT_GREY);
    }
}
