/* de:meta
{
  "slug": "pacman",
  "title": "pac-man",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "finite-state-ai",
    "tilemap-collision",
    "title-play-gameover-loop"
  ],
  "lineage": "Faithful port of Namco's 1980 Pac-Man; the ghost-personality targeting system (Blinky direct-chase, Pinky 4-tile lookahead, Inky reflection off Blinky, Clyde distance-shy) is reproduced from the original arcade AI spec.",
  "genre": "arcade",
  "homage": "Pac-Man (1980)",
  "description": "The maze classic, ghost AI and all. Eat every dot to clear the board; power pellets turn the ghosts blue so you can gobble them for escalating points. The four ghosts each hunt differently — Blinky chases head-on, Pinky aims four tiles ahead to ambush, Inky flanks using Blinky's position, and Clyde charges until he gets close then loses his nerve. Scatter/chase mode cycling, frightened mode, and eyes that scurry home to respawn. 3 lives, best score saved. Arrows / WASD to steer.",
  "todo": [
    "Mimic the exact level-1 Pac-Man layout."
  ]
}
de:meta */
#include "studio.h"

// PAC-MAN
// Eat every dot to clear the maze. Power pellets turn the ghosts blue —
// gobble them for big points before they recover.
// The four ghosts each hunt differently: Blinky chases you head-on, Pinky
// aims four tiles ahead to ambush, Inky flanks using Blinky's position, and
// Clyde charges until he gets close, then loses his nerve and backs off.
// Arrows / WASD to steer. 3 lives, best score saved.

#define MW    27
#define MH    21
#define TILE  8
#define X0    52
#define Y0    20

#define PAC_SPD   0.16f
#define G_SPD     0.135f
#define FRIGHT_SPD 0.09f
#define EYE_SPD   0.34f
#define FRIGHT_T  6.0f

enum { UP, RIGHT, DOWN, LEFT };
static const int DX[4] = { 0, 1, 0, -1 };
static const int DY[4] = { -1, 0, 1, 0 };
enum { SCATTER, CHASE };
enum { NORMAL, EYES };

// the maze: # wall, . dot, o power pellet, space = open (tunnels + ghost house)
static const char *MAZE[MH] = {
    "###########################",
    "#............#............#",
    "#.####.#####.#.#####.####.#",
    "#o####.#####.#.#####.####o#",
    "#.####.#####.#.#####.####.#",
    "#.........................#",
    "#.####.##.#######.##.####.#",
    "#.....##.....#.....##.....#",
    "#.###.######   ######.###.#",
    "#.###.####       ####.###.#",
    " ........#       #........ ",
    "#.###.####       ####.###.#",
    "#.###.###############.###.#",
    "#.....##.....#.....##.....#",
    "#.####.##.#######.##.####.#",
    "#.........................#",
    "#.####.#####.#.#####.####.#",
    "#o####.#####.#.#####.####o#",
    "#.####.#####.#.#####.####.#",
    "#............#............#",
    "###########################",
};

typedef struct { int tx, ty, dir, want; float prog; int facing; } Pac;
typedef struct { int tx, ty, dir; float prog; int state; int sx, sy; } Ghost; // sx,sy scatter corner

static char  grid[MH][MW];
static Pac   pac;
static Ghost gh[4];
static const int GCOL[4] = { CLR_RED, CLR_PINK, CLR_BLUE, CLR_ORANGE };

static int   dots;
static int   lives, score, hiscore, level;
static int   state;        // 0 play, 1 dead-pause, 2 game over, 3 level clear
static float fright_until, round_start, state_until;
static int   eat_combo;

// ====================================================================

static bool in_house(int tx, int ty) { return ty >= 8 && ty <= 11 && tx >= 10 && tx <= 16; }

static int wrapx(int tx) { return tx < 0 ? MW - 1 : tx >= MW ? 0 : tx; }

static bool ghost_open(int tx, int ty) {
    if (ty < 0 || ty >= MH) return false;
    tx = wrapx(tx);
    return grid[ty][tx] != '#';
}
static bool pac_open(int tx, int ty) {
    if (ty < 0 || ty >= MH) return false;
    tx = wrapx(tx);
    if (grid[ty][tx] == '#') return false;
    if (in_house(tx, ty)) return false;       // Pac can't enter the ghost house
    return true;
}

static void load_maze() {
    dots = 0;
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            char c = MAZE[y][x];
            grid[y][x] = c;
            if (c == '.' || c == 'o') dots++;
        }
}

static void reset_actors() {
    pac.tx = 13; pac.ty = 15; pac.dir = LEFT; pac.want = LEFT; pac.prog = 0; pac.facing = LEFT;
    int sx[4] = { 25, 1, 25, 1 }, sy[4] = { 1, 1, 19, 19 };
    int gtx[4] = { 13, 13, 11, 15 }, gty[4] = { 8, 10, 10, 10 };
    for (int i = 0; i < 4; i++) {
        gh[i].tx = gtx[i]; gh[i].ty = gty[i]; gh[i].dir = (i == 0) ? LEFT : UP;
        gh[i].prog = 0; gh[i].state = NORMAL; gh[i].sx = sx[i]; gh[i].sy = sy[i];
    }
    fright_until = 0; eat_combo = 0; round_start = now();
}

static void new_level() { load_maze(); reset_actors(); state = 0; }

static void reset_game() { lives = 3; score = 0; level = 1; new_level(); }

void init() { hiscore = load(0); reset_game(); }

// ====================================================================
// movement
// ====================================================================

static void advance(int *tx, int *ty, int dir, float *prog, float spd) {
    if (dir < 0) return;
    *prog += spd;
    if (*prog >= 1.0f) {
        *prog = 0;
        *tx = wrapx(*tx + DX[dir]);
        *ty += DY[dir];
    }
}

static int sq(int v) { return v * v; }

static void ghost_target(int gi, int mode, int *ttx, int *tty) {
    Ghost *g = &gh[gi];
    if (g->state == EYES)            { *ttx = 13; *tty = 10; return; }
    if (g->ty >= 8 && g->ty <= 11 && g->tx >= 10 && g->tx <= 16) { *ttx = 13; *tty = 7; return; } // exit house
    if (mode == SCATTER)             { *ttx = g->sx; *tty = g->sy; return; }
    switch (gi) {
        case 0: *ttx = pac.tx; *tty = pac.ty; break;                                  // Blinky
        case 1: *ttx = pac.tx + 4 * DX[pac.facing]; *tty = pac.ty + 4 * DY[pac.facing]; break; // Pinky
        case 2: {                                                                     // Inky
            int ax = pac.tx + 2 * DX[pac.facing], ay = pac.ty + 2 * DY[pac.facing];
            *ttx = 2 * ax - gh[0].tx; *tty = 2 * ay - gh[0].ty; break;
        }
        default:                                                                      // Clyde
            if (sq(g->tx - pac.tx) + sq(g->ty - pac.ty) > 64) { *ttx = pac.tx; *tty = pac.ty; }
            else { *ttx = g->sx; *tty = g->sy; }
    }
}

static void ghost_choose(int gi, int mode) {
    Ghost *g = &gh[gi];
    int rev = (g->dir + 2) % 4;
    bool frightened = now() < fright_until && g->state != EYES &&
                      !(g->ty >= 8 && g->ty <= 11 && g->tx >= 10 && g->tx <= 16);

    if (frightened) {                       // wander randomly when scared
        int opts[4], n = 0;
        for (int d = 0; d < 4; d++)
            if (d != rev && ghost_open(g->tx + DX[d], g->ty + DY[d])) opts[n++] = d;
        if (n == 0) g->dir = rev;
        else        g->dir = opts[rnd(n)];
        return;
    }

    int ttx, tty; ghost_target(gi, mode, &ttx, &tty);
    int best = -1, bestd = 1 << 30;
    for (int d = 0; d < 4; d++) {           // order up,right,down,left handles ties
        if (d == rev) continue;
        int nx = g->tx + DX[d], ny = g->ty + DY[d];
        if (!ghost_open(nx, ny)) continue;
        int dd = sq(wrapx(nx) - ttx) + sq(ny - tty);
        if (dd < bestd) { bestd = dd; best = d; }
    }
    g->dir = (best >= 0) ? best : rev;      // dead end → turn around
}

// ====================================================================
// update
// ====================================================================

static void lose_life() {
    lives--;
    note(36, INSTR_SAW, 5);
    if (lives <= 0) {
        state = 2;
        if (score > hiscore) { hiscore = score; save(0, hiscore); }
    } else {
        state = 1; state_until = now() + 1.4f;   // brief pause, then respawn
    }
}

void update() {
    if (state == 1) {                         // death pause
        if (now() > state_until) { reset_actors(); state = 0; }
        return;
    }
    if (state == 2 || state == 3) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) {
            if (state == 3) { level++; new_level(); }
            else reset_game();
        }
        return;
    }

    // --- Pac input + move ---
    if (btn(0, BTN_UP))    pac.want = UP;
    if (btn(0, BTN_DOWN))  pac.want = DOWN;
    if (btn(0, BTN_LEFT))  pac.want = LEFT;
    if (btn(0, BTN_RIGHT)) pac.want = RIGHT;

    if (pac.prog == 0) {
        if (pac.want >= 0 && pac_open(pac.tx + DX[pac.want], pac.ty + DY[pac.want])) pac.dir = pac.want;
        if (pac.dir < 0 || !pac_open(pac.tx + DX[pac.dir], pac.ty + DY[pac.dir]))    pac.dir = -1;
        if (pac.dir >= 0) pac.facing = pac.dir;
    }
    advance(&pac.tx, &pac.ty, pac.dir, &pac.prog, PAC_SPD);

    // --- eat ---
    if (pac.prog == 0) {
        char *c = &grid[pac.ty][pac.tx];
        if (*c == '.') { *c = ' '; score += 10; dots--; if (frame() % 2) note(72, INSTR_SQUARE, 1); else note(67, INSTR_SQUARE, 1); }
        else if (*c == 'o') {
            *c = ' '; score += 50; dots--; fright_until = now() + FRIGHT_T; eat_combo = 0;
            note(48, INSTR_SAW, 3);
        }
        if (dots <= 0) { state = 3; score += 200; if (score > hiscore) { hiscore = score; save(0, hiscore); } return; }
    }

    // --- mode (scatter early, then chase, repeating) ---
    float t = now() - round_start;
    int mode = (((int)(t / 7)) % 3 == 0 && t < 35) ? SCATTER : CHASE;

    // --- ghosts ---
    for (int i = 0; i < 4; i++) {
        Ghost *g = &gh[i];
        float spd = G_SPD + level * 0.004f;
        if (g->state == EYES) spd = EYE_SPD;
        else if (now() < fright_until) spd = FRIGHT_SPD;
        if (g->prog == 0) ghost_choose(i, mode);
        advance(&g->tx, &g->ty, g->dir, &g->prog, spd);
        if (g->state == EYES && g->tx == 13 && (g->ty == 10 || g->ty == 11)) g->state = NORMAL;

        // collision with Pac
        if (g->tx == pac.tx && g->ty == pac.ty) {
            bool frightened = now() < fright_until && g->state == NORMAL;
            if (frightened) {
                eat_combo++; int pts = 200 * (1 << (eat_combo - 1)); if (pts > 1600) pts = 1600;
                score += pts; g->state = EYES;
                note(84, INSTR_SQUARE, 4);
            } else if (g->state == NORMAL) {
                lose_life(); return;
            }
        }
    }
}

// ====================================================================
// drawing
// ====================================================================

static int px_of(int tx, int dir, float prog) { return X0 + tx * TILE + TILE / 2 + (dir >= 0 ? (int)(DX[dir] * prog * TILE) : 0); }
static int py_of(int ty, int dir, float prog) { return Y0 + ty * TILE + TILE / 2 + (dir >= 0 ? (int)(DY[dir] * prog * TILE) : 0); }

static void draw_pac() {
    int x = px_of(pac.tx, pac.dir, pac.prog), y = py_of(pac.ty, pac.dir, pac.prog);
    circfill(x, y, 4, CLR_YELLOW);
    int ang = pac.facing == RIGHT ? 0 : pac.facing == DOWN ? 90 : pac.facing == LEFT ? 180 : 270;
    float open = 6 + 26 * (0.5f + 0.5f * sin_deg(frame() * 22));  // chomp
    trifill(x, y,
            x + (int)dx(7, ang - open), y + (int)dy(7, ang - open),
            x + (int)dx(7, ang + open), y + (int)dy(7, ang + open), CLR_BLACK);
}

static void draw_ghost(int i) {
    Ghost *g = &gh[i];
    int x = px_of(g->tx, g->dir, g->prog), y = py_of(g->ty, g->dir, g->prog);
    bool frightened = now() < fright_until && g->state == NORMAL;

    if (g->state != EYES) {
        int col = GCOL[i];
        if (frightened) {
            bool blnk = (fright_until - now() < 2.0f) && !blink(6);
            col = blnk ? CLR_WHITE : CLR_DARK_BLUE;
        }
        circfill(x, y - 1, 4, col);
        rectfill(x - 4, y - 1, 8, 5, col);
        rectfill(x - 4, y + 3, 2, 2, col);    // little feet
        rectfill(x - 1, y + 3, 2, 2, col);
        rectfill(x + 2, y + 3, 2, 2, col);
        if (frightened) {
            pset(x - 2, y - 1, CLR_PINK); pset(x + 2, y - 1, CLR_PINK);
            line(x - 3, y + 2, x + 3, y + 2, CLR_PINK);
            return;
        }
    }
    // eyes (also the whole sprite in EYES mode)
    int ex = DX[g->dir < 0 ? LEFT : g->dir], ey = DY[g->dir < 0 ? LEFT : g->dir];
    circfill(x - 2, y - 1, 2, CLR_WHITE);
    circfill(x + 2, y - 1, 2, CLR_WHITE);
    pset(x - 2 + ex, y - 1 + ey, CLR_DARK_BLUE);
    pset(x + 2 + ex, y - 1 + ey, CLR_DARK_BLUE);
}

void draw() {
    cls(CLR_BLACK);

    // maze
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            int sx = X0 + x * TILE, sy = Y0 + y * TILE;
            char c = grid[y][x];
            if (c == '#') {
                rectfill(sx, sy, TILE, TILE, CLR_TRUE_BLUE);
            } else if (c == '.') {
                rectfill(sx + TILE / 2 - 1, sy + TILE / 2 - 1, 2, 2, CLR_PEACH);
            } else if (c == 'o') {
                if (!blink(8)) circfill(sx + TILE / 2, sy + TILE / 2, 3, CLR_LIGHT_PEACH);
            }
        }
    // ghost-house door
    rectfill(X0 + 12 * TILE, Y0 + 8 * TILE + 3, TILE * 3, 2, CLR_PINK);

    for (int i = 3; i >= 0; i--) draw_ghost(i);
    if (state != 1) draw_pac();

    // HUD
    print(str("SCORE %d", score), 2, 2, CLR_WHITE);
    print_right(str("HI %d", hiscore), SCREEN_W - 2, 2, CLR_YELLOW);
    print(str("LVL %d", level), 2, 190, CLR_LIGHT_GREY);
    for (int i = 0; i < lives; i++) circfill(SCREEN_W - 12 - i * 12, 193, 4, CLR_YELLOW);

    if (state == 2) {
        rectfill(SCREEN_W / 2 - 60, SCREEN_H / 2 - 12, 120, 30, CLR_BLACK);
        rect    (SCREEN_W / 2 - 60, SCREEN_H / 2 - 12, 120, 30, CLR_RED);
        print_centered("GAME OVER", SCREEN_W/2, SCREEN_H / 2 - 6, CLR_RED);
        print_centered("Z to play again", SCREEN_W/2, SCREEN_H / 2 + 6, CLR_LIGHT_GREY);
    }
    if (state == 3) {
        print_centered("LEVEL CLEAR!", SCREEN_W/2, SCREEN_H / 2 - 6, CLR_YELLOW);
        print_centered("Z to continue", SCREEN_W/2, SCREEN_H / 2 + 6, CLR_LIGHT_GREY);
    }
}
