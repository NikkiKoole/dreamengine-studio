/* de:meta
{
  "title": "Boulder Dash",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "cellular-automata",
    "camera-follow",
    "tilemap-collision"
  ],
  "lineage": "Boulder Dash (First Star Software, 1984) rebuilt on the sand.c gravity idiom; novel: a falling[] persistence flag across settle passes so stacked columns cascade one cell per tick rather than teleporting, plus sokoban-style sideways push.",
  "genre": "arcade",
  "homage": "Boulder Dash (1984)",
  "description": "Tunnel through a dirt-packed cave for diamonds while boulders and gems obey gravity — dig out what holds a rock up and it falls, rolling off the rounded tops of other rocks into open gaps, and a falling rock crushes you (or a wandering enemy) flat. Grab the diamond quota to flash the exit open, then scramble to it before the timer runs out. The cave settles on a steady tick so falling stacks cascade one cell at a time, and the camera follows your miner across a cave bigger than the screen. Arrows step and dig in a direction; walk into a boulder sideways to push it into an empty cell beyond; R or B restarts the cave."
}
de:meta */
#include "studio.h"
#include <string.h>

// ── BOULDER DASH — dig, grab, don't get crushed ───────────────────
// Tunnel through dirt to collect diamonds. Boulders AND diamonds obey
// gravity: dig out what holds them up and they fall — and roll off the
// rounded tops of other rocks. A rock that lands on your head crushes
// you (and squashes the enemy too). Grab the quota to flash the exit
// open, then reach it before the timer runs out.
//
//   ARROWS  step / dig — push a boulder by walking into it sideways
//   R / B   restart the cave
//
// Built from primitives: the cave is a cell grid, gravity is one
// bottom-up settle pass (the sand.c idiom), pushing is the sokoban rule.

// cell kinds
enum { EMPTY, DIRT, WALL, ROCK, GEM, PLAYER, EXIT, ENEMY };

#define GW 28          // cave width  in cells
#define GH 18          // cave height in cells
#define CS 16          // cell pixels

// the hand-built cave. legend:
//   X wall  . dirt  (space) empty  O boulder  *  diamond
//   P player  E exit  e enemy
static const char *CAVE[GH] = {
    "XXXXXXXXXXXXXXXXXXXXXXXXXXXX",  // 28 wide, every row exactly GW chars
    "XP...O....*...O...*...O....X",
    "X..........................X",
    "X..OO..*..XXXX..O...*..OO..X",
    "X..........X.........e.....X",
    "X..*...O...X...*...O...*...X",
    "X.....XXX...O......XXX.....X",
    "X..O...*..O...*..O...*..O..X",
    "X..........................X",
    "X.*..XXXX..*..O..*..XXXX...X",
    "X..O........e............O.X",
    "X.....*..O...*..O...*......X",
    "X..XXX.............XXX.....X",
    "X..O...*..O...*..O...*..O..X",
    "X..........................X",
    "X..*..O...*..O..*..O...*...X",
    "X.......................E..X",
    "XXXXXXXXXXXXXXXXXXXXXXXXXXXX",
};

static unsigned char grid[GH][GW];
static int  px, py;            // player cell
static int  diamonds, quota;
static float time_left;        // seconds
static bool open_exit, won, dead;
static float tick_acc;         // settle-tick accumulator
static int   exit_x, exit_y;

// per-cell "just fell" flag so a rock can only fall once per settle pass
static unsigned char falling[GH][GW];

static int best_time = 0;

static void build(void) {
    diamonds = 0; quota = 0;
    open_exit = won = dead = false;
    time_left = 90.0f; tick_acc = 0;
    memset(falling, 0, sizeof(falling));
    for (int y = 0; y < GH; y++) {
        const char *row = CAVE[y];
        for (int x = 0; x < GW; x++) {
            char c = (row[0] && (int)strlen(row) > x) ? row[x] : ' ';
            unsigned char k = EMPTY;
            switch (c) {
                case 'X': k = WALL;   break;
                case '.': k = DIRT;   break;
                case 'O': k = ROCK;   break;
                case '*': k = GEM;    quota++; break;
                case 'P': k = PLAYER; px = x; py = y; break;
                case 'E': k = EXIT;   exit_x = x; exit_y = y; break;
                case 'e': k = ENEMY;  break;
                default:  k = EMPTY;  break;
            }
            grid[y][x] = k;
        }
    }
    quota = (quota * 3) / 4;        // need most, not every, gem
    if (quota < 1) quota = 1;
    best_time = load(0);
}

void init(void) { build(); }

static bool solid_for_step(unsigned char k) {
    return k == WALL || k == ROCK || k == ENEMY;
}

// try to move the player by (dx,dy); handles dig / collect / push
static void try_move(int dx, int dy) {
    int nx = px + dx, ny = py + dy;
    if (nx < 0 || ny < 0 || nx >= GW || ny >= GH) return;
    unsigned char t = grid[ny][nx];

    if (t == WALL) return;
    if (t == ENEMY) { dead = true; sfx(-1); shake(6); note(40, INSTR_NOISE, 6); return; }
    if (t == EXIT && open_exit) {                       // step into the open exit
        grid[py][px] = EMPTY; px = nx; py = ny;
        won = true;
        strum(60, CHORD_MAJ7, INSTR_TRI, 5, 50);
        return;
    }
    if (t == EXIT) return;                               // still locked

    if (t == ROCK) {                                     // push only sideways
        if (dy != 0) return;
        int bx = nx + dx;
        if (bx < 0 || bx >= GW) return;
        if (grid[ny][bx] != EMPTY) return;
        grid[ny][bx] = ROCK;
        grid[ny][nx] = EMPTY;
        note(48, INSTR_SQUARE, 2);
        // player then steps in below
    }

    if (t == GEM) {
        diamonds++;
        note(84, INSTR_SINE, 4); hit(88, INSTR_SINE, 4, 90);
        if (!open_exit && diamonds >= quota) {
            open_exit = true;
            strum(72, CHORD_MAJ, INSTR_TRI, 4, 40);
        }
    } else if (t == DIRT) {
        note(36, INSTR_NOISE, 1);                        // dig crunch
    }

    grid[py][px] = EMPTY;
    grid[ny][nx] = PLAYER;
    px = nx; py = ny;
}

// one bottom-up settle pass: rocks & gems fall / roll, crush what's below.
// `falling` persists across passes (a rock keeps its momentum) so a rock that
// fell last tick can crush the player/enemy directly beneath it this tick.
static unsigned char nf[GH][GW];      // next-frame falling state
static unsigned char vac[GH][GW];     // cells vacated this pass (one cell/tick)
static void settle(void) {
    memset(nf, 0, sizeof(nf));
    memset(vac, 0, sizeof(vac));
    bool any_thud = false;
    for (int y = GH - 2; y >= 0; y--) {
        for (int x = 0; x < GW; x++) {
            unsigned char k = grid[y][x];
            if (k != ROCK && k != GEM) continue;
            unsigned char below = grid[y + 1][x];
            bool was_falling = falling[y][x];

            // fall straight down into empty (but not into a cell vacated THIS
            // pass — that keeps a falling column a one-cell-per-tick cascade)
            if (below == EMPTY && !vac[y + 1][x]) {
                grid[y + 1][x] = k; grid[y][x] = EMPTY;
                nf[y + 1][x] = 1; vac[y][x] = 1;  // still in motion
                continue;
            }
            // a moving rock lands on the player or an enemy → crush
            if ((below == PLAYER || below == ENEMY) && was_falling) {
                if (below == PLAYER) { dead = true; px = -99; shake(7); }
                else any_thud = true;
                grid[y + 1][x] = k; grid[y][x] = EMPTY;   // rock takes the cell
                note(40, INSTR_NOISE, 5);
                continue;
            }
            // roll off a rounded top (rock/gem/wall) into an empty diagonal
            if (below == ROCK || below == GEM || below == WALL) {
                if (x > 0 && grid[y][x - 1] == EMPTY && grid[y + 1][x - 1] == EMPTY && !vac[y][x - 1]) {
                    grid[y][x - 1] = k; grid[y][x] = EMPTY;
                    nf[y][x - 1] = 1; vac[y][x] = 1; continue;
                }
                if (x < GW - 1 && grid[y][x + 1] == EMPTY && grid[y + 1][x + 1] == EMPTY && !vac[y][x + 1]) {
                    grid[y][x + 1] = k; grid[y][x] = EMPTY;
                    nf[y][x + 1] = 1; vac[y][x] = 1; continue;
                }
            }
            // blocked and not rolling → it comes to rest (thud if it was moving)
            if (was_falling) { any_thud = true; note(45, INSTR_SQUARE, 2); }
        }
    }
    memcpy(falling, nf, sizeof(falling));
    if (any_thud) { shake(2); }
}

// simple enemy: drift toward an empty/dirt neighbour now and then
static void move_enemies(void) {
    static int phase = 0;
    phase ^= 1;
    for (int y = GH - 1; y >= 0; y--)
        for (int xx = 0; xx < GW; xx++) {
            int x = phase ? xx : GW - 1 - xx;
            if (grid[y][x] != ENEMY) continue;
            // prefer horizontal wandering toward the player
            int dir = (px > x) ? 1 : -1;
            int nx = x + dir;
            if (nx < 0 || nx >= GW) { dir = -dir; nx = x + dir; }
            unsigned char t = (nx >= 0 && nx < GW) ? grid[y][nx] : WALL;
            if (t == PLAYER) { dead = true; shake(6); return; }
            if (t == EMPTY || t == DIRT) {
                grid[y][x] = EMPTY; grid[y][nx] = ENEMY;
            }
        }
}

void update(void) {
    if (won || dead) {
        if (won && time_left > best_time) { best_time = (int)time_left; save(0, best_time); }
        if (key('R') || keyp('R') || btnp(0, BTN_B) || keyp(KEY_SPACE) || keyp(KEY_ENTER))
            build();
        return;
    }

    time_left -= dt();
    if (time_left <= 0) { time_left = 0; dead = true; shake(5); }

    if      (btnp(0, BTN_LEFT))  try_move(-1, 0);
    else if (btnp(0, BTN_RIGHT)) try_move( 1, 0);
    else if (btnp(0, BTN_UP))    try_move( 0, -1);
    else if (btnp(0, BTN_DOWN))  try_move( 0,  1);
    if (key('R') || btnp(0, BTN_B)) build();

    // fixed-cadence settle tick (gravity reads as a steady cascade)
    tick_acc += dt();
    while (tick_acc >= 0.10f) {
        tick_acc -= 0.10f;
        settle();
        static int slow = 0;
        if ((++slow % 3) == 0) move_enemies();
        if (px == -99) dead = true;
    }
}

// ── drawing ────────────────────────────────────────────────────────
static void draw_dirt(int x, int y) {
    rectfill(x, y, CS, CS, CLR_DARK_BROWN);
    // speckle
    pset(x + 3,  y + 4,  CLR_BROWN);
    pset(x + 9,  y + 7,  CLR_BROWN);
    pset(x + 5,  y + 11, CLR_BROWNISH_BLACK);
    pset(x + 12, y + 3,  CLR_BROWNISH_BLACK);
    pset(x + 11, y + 12, CLR_BROWN);
}
static void draw_wall(int x, int y) {
    rectfill(x, y, CS, CS, CLR_DARK_GREY);
    rect(x, y, CS, CS, CLR_LIGHT_GREY);
    line(x, y + 8, x + CS - 1, y + 8, CLR_BROWNISH_BLACK);
    line(x + 8, y, x + 8, y + 7, CLR_BROWNISH_BLACK);
}
static void draw_rock(int x, int y) {
    int cx = x + CS / 2, cy = y + CS / 2;
    circfill(cx, cy, 7, CLR_MEDIUM_GREY);
    circfill(cx - 2, cy - 2, 4, CLR_LIGHT_GREY);   // highlight
    circ(cx, cy, 7, CLR_DARKER_GREY);
}
static void draw_gem(int x, int y, bool sparkle) {
    int cx = x + CS / 2, cy = y + CS / 2;
    int c = sparkle && blink(8) ? CLR_WHITE : CLR_BLUE;
    trifill(cx, y + 3, x + 3, cy, x + CS - 3, cy, c);        // top facet
    trifill(x + 3, cy, x + CS - 3, cy, cx, y + CS - 3, CLR_TRUE_BLUE);
    pset(cx - 1, cy - 2, CLR_WHITE);
}
static void draw_exit(int x, int y) {
    bool flash = open_exit && blink(6);
    int frame_c = open_exit ? (flash ? CLR_YELLOW : CLR_GREEN) : CLR_DARK_GREY;
    int fill_c  = open_exit ? (flash ? CLR_WHITE  : CLR_DARK_GREEN) : CLR_BROWNISH_BLACK;
    rectfill(x, y, CS, CS, fill_c);
    rect(x, y, CS, CS, frame_c);
    rect(x + 4, y + 3, CS - 8, CS - 6, frame_c);
}
static void draw_enemy(int x, int y) {
    int cx = x + CS / 2, cy = y + CS / 2;
    int wob = (frame() / 6) % 2 ? 1 : -1;
    circfill(cx, cy, 6, CLR_DARK_RED);
    circ(cx, cy, 6, CLR_ORANGE);
    pset(cx - 2 + wob, cy - 1, CLR_YELLOW);
    pset(cx + 2 + wob, cy - 1, CLR_YELLOW);
}
static void draw_player(int x, int y) {
    int cx = x + CS / 2;
    rectfill(x + 4, y + 6, 8, 8, CLR_WHITE);        // body
    circfill(cx, y + 5, 4, CLR_LIGHT_PEACH);        // head
    rectfill(x + 4, y + 1, 8, 3, CLR_RED);          // miner cap
    pset(cx - 1, y + 4, CLR_BLACK);
    pset(cx + 1, y + 4, CLR_BLACK);
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);

    // camera follows the miner over the cave, clamped to bounds
    int wpx = (px == -99 ? exit_x : px) * CS + CS / 2;
    int wpy = (px == -99 ? exit_y : py) * CS + CS / 2;
    follow(wpx, wpy + 6, GW * CS, GH * CS + 14);

    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            int sx = x * CS, sy = y * CS;
            switch (grid[y][x]) {
                case DIRT:   draw_dirt(sx, sy); break;
                case WALL:   draw_wall(sx, sy); break;
                case ROCK:   draw_rock(sx, sy); break;
                case GEM:    draw_gem(sx, sy, true); break;
                case EXIT:   draw_exit(sx, sy); break;
                case ENEMY:  draw_enemy(sx, sy); break;
                case PLAYER: draw_player(sx, sy); break;
                default: break;                         // EMPTY = the void
            }
        }

    // HUD (screen space — reset camera)
    camera(0, 0);
    rectfill(0, 0, SCREEN_W, 13, CLR_BLACK);
    print("BOULDER DASH", 4, 3, CLR_LIME_GREEN);
    int gemc = open_exit ? CLR_GREEN : CLR_LIGHT_YELLOW;
    print(str("GEMS %d/%d", diamonds, quota), 112, 3, gemc);
    int tcol = time_left < 15 ? (blink(6) ? CLR_RED : CLR_ORANGE) : CLR_WHITE;
    print_right(str("TIME %d", (int)time_left), SCREEN_W - 4, 3, tcol);

    if (open_exit && !won && !dead)
        print_centered(blink(10) ? "EXIT OPEN!" : "", SCREEN_W/2, 16, CLR_GREEN);

    if (dead) {
        fade(0.55f);
        int w = 200, bx = (SCREEN_W - w) / 2;
        rectfill(bx, 78, w, 40, CLR_DARK_RED);
        rect(bx, 78, w, 40, CLR_RED);
        print_centered("CRUSHED!", SCREEN_W/2, 88, CLR_WHITE);
        print_centered("R / SPACE to dig again", SCREEN_W/2, 104, CLR_LIGHT_PEACH);
    }
    if (won) {
        int w = 220, bx = (SCREEN_W - w) / 2;
        rectfill(bx, 74, w, 48, CLR_DARK_GREEN);
        rect(bx, 74, w, 48, CLR_LIME_GREEN);
        print_centered("CAVE CLEARED!", SCREEN_W/2, 84, CLR_WHITE);
        print_centered(str("time left %d  best %d", (int)time_left, best_time), SCREEN_W/2, 98, CLR_LIGHT_YELLOW);
        print_centered("R / SPACE to play again", SCREEN_W/2, 110, CLR_LIGHT_PEACH);
    }
}
