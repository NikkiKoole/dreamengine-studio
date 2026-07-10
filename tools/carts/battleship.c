/* de:meta
{
  "slug": "battleship",
  "title": "Battleship",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "finite-state-ai",
    "turn-based-loop"
  ],
  "lineage": "Classic Battleship board game — two-grid setup/battle phases with a hunt-then-target CPU AI (stack of candidate cells, checkerboard hunt bias).",
  "genre": "tabletop",
  "description": "Classic two-grid naval Battleship against a scheming CPU. Set up your fleet of five ships on the left ocean — a ghost ship snaps to the cursor and turns red when it won't fit — then trade salvos with the hidden enemy grid on the right. Hits flash and shake the screen with a low boom, misses splash, and a major-chord sting rings out when you sink the Carrier, Battleship, Cruiser, Submarine, or Destroyer. The enemy fires back with a hunt-then-target AI: random checkerboard shots until it draws blood, then it probes the neighbours of its last hit until your ship is dead. First fleet sunk loses; wins and losses are tallied across runs. SETUP: move the mouse to aim, click to place, R to rotate, click RANDOM to auto-fill, click READY to fight. BATTLE: click an enemy cell to fire. GAME OVER: click or press Z to play again.",
  "todo": [
    "Touch: an onscreen pixel button to rotate ships so it's playable on touch.",
    "Add a win/lose panel at the end.",
    "Consider real spritework for the ships.",
    "ui-audit: \"PLACING: CARRIER (5)\" runs off the right edge and overlaps \"PLACE YOUR FLEET\"."
  ]
}
de:meta */
#include "studio.h"
#include "endcard.h"
#include <stddef.h>
#include <string.h>

// ---- BATTLESHIP -----------------------------------------------------------
// Two 10x10 grids. Left = your fleet, right = enemy ocean (hidden).
// SETUP: click to place ships, R to rotate, RANDOM to auto-fill.
// BATTLE: click an enemy cell to fire; CPU replies with hunt/target AI.

#define N      10          // grid size
#define CELL   13          // pixel size of a cell
#define NSHIP  5

// board layout (top-left of each grid + label row)
#define GY     34
#define LGX    8           // your grid x
#define RGX    180         // enemy grid x

// cell state flags
#define ST_EMPTY 0
#define ST_SHIP  1   // bit: occupied by ship
#define ST_HIT   2   // bit: shot landed and hit
#define ST_MISS  4   // bit: shot landed, water

// game phases
#define PH_SETUP 0
#define PH_BATTLE 1
#define PH_OVER  2

typedef struct {
    int len;
    int cx, cy;     // anchor cell
    int horiz;      // 1 = horizontal
    int placed;
    int hits;       // pegs landed on it
    const char *name;
} Ship;

// boards store ST_* bitflags. ship[] tells which ship owns a SHIP cell.
static unsigned char myCell[N][N];
static signed char   myOwn[N][N];   // ship index owning the cell, -1 if none
static unsigned char enCell[N][N];
static signed char   enOwn[N][N];

static Ship myFleet[NSHIP];
static Ship enFleet[NSHIP];

static int phase;
static int placing;        // index of ship currently being placed (setup)
static int ghostHoriz;     // current rotate state for ghost
static int playerWon;
static float end_t;        // seconds since the game-over card came up
static int wins, losses;

// turn pacing
static int turn;           // 0 = player to fire, 1 = cpu thinking
static float cpuTimer;     // counts up while cpu "thinks"

// messages
static char banner[48];
static float bannerT;
static char badFlash;      // bad-placement red flash frames
static float badFlashT;

// cpu AI hunt/target
static int aiTx[8], aiTy[8], aiTn;   // target stack of candidate cells
static int aiActive;                 // currently hunting a wounded ship

static void set_banner(const char *s) {
    strncpy(banner, s, sizeof(banner) - 1);
    banner[sizeof(banner) - 1] = 0;
    bannerT = now();
}

static const char *SHIP_NAMES[NSHIP] = {
    "CARRIER", "BATTLESHIP", "CRUISER", "SUBMARINE", "DESTROYER"
};
static const int SHIP_LENS[NSHIP] = { 5, 4, 3, 3, 2 };

// can ship of len fit at (cx,cy) horiz/vert on this board without overlap?
static int fits(unsigned char cell[N][N], int len, int cx, int cy, int horiz) {
    for (int i = 0; i < len; i++) {
        int x = cx + (horiz ? i : 0);
        int y = cy + (horiz ? 0 : i);
        if (x < 0 || x >= N || y < 0 || y >= N) return 0;
        if (cell[y][x] & ST_SHIP) return 0;
    }
    return 1;
}

static void stamp(unsigned char cell[N][N], signed char own[N][N],
                  Ship *s, int idx) {
    for (int i = 0; i < s->len; i++) {
        int x = s->cx + (s->horiz ? i : 0);
        int y = s->cy + (s->horiz ? 0 : i);
        cell[y][x] |= ST_SHIP;
        own[y][x] = (signed char)idx;
    }
}

static void random_fleet(unsigned char cell[N][N], signed char own[N][N],
                         Ship fleet[NSHIP]) {
    memset(cell, 0, sizeof(unsigned char) * N * N);
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) own[y][x] = -1;
    for (int i = 0; i < NSHIP; i++) {
        fleet[i].len = SHIP_LENS[i];
        fleet[i].name = SHIP_NAMES[i];
        fleet[i].hits = 0;
        fleet[i].placed = 1;
        int tries = 0;
        for (;;) {
            int h = rnd(2);
            int cx = rnd(N), cy = rnd(N);
            if (fits(cell, fleet[i].len, cx, cy, h)) {
                fleet[i].cx = cx; fleet[i].cy = cy; fleet[i].horiz = h;
                stamp(cell, own, &fleet[i], i);
                break;
            }
            if (++tries > 500) break;
        }
    }
}

static void new_game(void) {
    phase = PH_SETUP;
    end_t = 0;
    placing = 0;
    ghostHoriz = 1;
    turn = 0;
    cpuTimer = 0;
    aiTn = 0; aiActive = 0;
    badFlash = 0;
    banner[0] = 0;

    // clear my board, set up fleet defs (unplaced)
    memset(myCell, 0, sizeof(myCell));
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) myOwn[y][x] = -1;
    for (int i = 0; i < NSHIP; i++) {
        myFleet[i].len = SHIP_LENS[i];
        myFleet[i].name = SHIP_NAMES[i];
        myFleet[i].hits = 0;
        myFleet[i].placed = 0;
        myFleet[i].horiz = 1;
    }

    // enemy fleet hidden + randomly placed now
    random_fleet(enCell, enOwn, enFleet);
    set_banner("PLACE YOUR FLEET");
}

void init(void) {
    wins = load_int("bs_wins", 0);
    losses = load_int("bs_losses", 0);
    // sink sting instrument
    instrument(5, INSTR_SAW, 4, 120, 3, 200);
    instrument_filter(5, FILTER_LOW, 1200, 8);
    new_game();
}

// pixel -> cell on a grid at gx; returns 1 if inside, fills cx/cy
static int cell_at(int gx, int *cx, int *cy) {
    int mx = mouse_x(), my = mouse_y();
    if (mx < gx || my < GY) return 0;
    int x = (mx - gx) / CELL;
    int y = (my - GY) / CELL;
    if (x < 0 || x >= N || y < 0 || y >= N) return 0;
    *cx = x; *cy = y;
    return 1;
}

static int in_box(int bx, int by, int bw, int bh) {
    return point_in_box(mouse_x(), mouse_y(), bx, by, bw, bh);
}

// fleet fully placed?
static int all_placed(void) {
    for (int i = 0; i < NSHIP; i++) if (!myFleet[i].placed) return 0;
    return 1;
}

// returns owner ship idx if a shot at (x,y) sinks it; -1 otherwise.
// caller has already flagged ST_HIT.
static int register_hit(signed char own[N][N], Ship fleet[NSHIP],
                        int x, int y) {
    int idx = own[y][x];
    if (idx < 0) return -1;
    fleet[idx].hits++;
    if (fleet[idx].hits >= fleet[idx].len) return idx;
    return -2; // hit but not sunk
}

static int fleet_dead(Ship fleet[NSHIP]) {
    for (int i = 0; i < NSHIP; i++)
        if (fleet[i].hits < fleet[i].len) return 0;
    return 1;
}

// ---- CPU AI ---------------------------------------------------------------
static void ai_push(int x, int y) {
    if (x < 0 || x >= N || y < 0 || y >= N) return;
    if (myCell[y][x] & (ST_HIT | ST_MISS)) return; // already shot
    if (aiTn >= 8) return;
    for (int i = 0; i < aiTn; i++)
        if (aiTx[i] == x && aiTy[i] == y) return;
    aiTx[aiTn] = x; aiTy[aiTn] = y; aiTn++;
}

static void cpu_fire(void) {
    int x = -1, y = -1;
    // target mode: pop from stack
    while (aiTn > 0 && x < 0) {
        aiTn--;
        int tx = aiTx[aiTn], ty = aiTy[aiTn];
        if (!(myCell[ty][tx] & (ST_HIT | ST_MISS))) { x = tx; y = ty; }
    }
    // hunt mode: random unshot cell (checkerboard bias)
    if (x < 0) {
        int tries = 0;
        for (;;) {
            int rx = rnd(N), ry = rnd(N);
            if (!(myCell[ry][rx] & (ST_HIT | ST_MISS))) {
                if (((rx + ry) & 1) == 0 || ++tries > 60) { x = rx; y = ry; break; }
                tries++;
            } else if (++tries > 400) {
                // grid almost full — take anything
                for (int yy = 0; yy < N; yy++)
                    for (int xx = 0; xx < N; xx++)
                        if (!(myCell[yy][xx] & (ST_HIT | ST_MISS))) { x = xx; y = yy; }
                break;
            }
        }
    }
    if (x < 0) return;

    if (myCell[y][x] & ST_SHIP) {
        myCell[y][x] |= ST_HIT;
        shake(3.0f);
        hit(40, INSTR_NOISE, 5, 140);
        int r = register_hit(myOwn, myFleet, x, y);
        if (r >= 0) {
            // sunk one of ours
            set_banner(str("CPU SANK YOUR %s", myFleet[r].name));
            aiTn = 0; aiActive = 0; // ship dead, resume hunting
            strum(48, CHORD_MIN7, 5, 5, -40);
        } else {
            // wounded — queue neighbours
            aiActive = 1;
            ai_push(x - 1, y); ai_push(x + 1, y);
            ai_push(x, y - 1); ai_push(x, y + 1);
        }
        if (fleet_dead(myFleet)) {
            phase = PH_OVER; playerWon = 0;
            losses++; save_int("bs_losses", losses);
            set_banner("YOUR FLEET IS SUNK");
        }
    } else {
        myCell[y][x] |= ST_MISS;
        hit(60, INSTR_SINE, 2, 80);
    }
    turn = 0; // back to player
}

// ---- player fire ----------------------------------------------------------
static void player_fire(int x, int y) {
    if (enCell[y][x] & (ST_HIT | ST_MISS)) return; // already shot
    if (enCell[y][x] & ST_SHIP) {
        enCell[y][x] |= ST_HIT;
        shake(4.0f);
        hit(38, INSTR_NOISE, 6, 160);
        int r = register_hit(enOwn, enFleet, x, y);
        if (r >= 0) {
            set_banner(str("YOU SUNK THE %s!", enFleet[r].name));
            strum(72, CHORD_MAJ7, 5, 6, 50);
        } else {
            set_banner("HIT!");
        }
        if (fleet_dead(enFleet)) {
            phase = PH_OVER; playerWon = 1;
            wins++; save_int("bs_wins", wins);
            set_banner("VICTORY - ENEMY FLEET SUNK");
            return;
        }
    } else {
        enCell[y][x] |= ST_MISS;
        hit(64, INSTR_SINE, 3, 90);
        set_banner("MISS");
    }
    turn = 1; cpuTimer = 0; // hand off to cpu
}

void update(void) {
    if (phase == PH_OVER) {
        end_t += dt();
        if (mouse_pressed(MOUSE_LEFT) || keyp('Z') || keyp(KEY_ENTER)) new_game();
        return;
    }

    if (phase == PH_SETUP) {
        if (keyp('R')) ghostHoriz = !ghostHoriz;

        // RANDOM button
        if (mouse_pressed(MOUSE_LEFT) && in_box(8, GY + N*CELL + 10, 70, 16)) {
            random_fleet(myCell, myOwn, myFleet);
            placing = NSHIP;
            set_banner("FLEET READY - CLICK READY");
            return;
        }
        // READY button (only when everything placed)
        if (all_placed() && mouse_pressed(MOUSE_LEFT)
            && in_box(86, GY + N*CELL + 10, 70, 16)) {
            phase = PH_BATTLE; turn = 0;
            set_banner("FIRE AT THE ENEMY OCEAN");
            return;
        }

        // place current ship on click in my grid
        if (!all_placed() && mouse_pressed(MOUSE_LEFT)) {
            int cx, cy;
            if (cell_at(LGX, &cx, &cy)) {
                Ship *s = &myFleet[placing];
                if (fits(myCell, s->len, cx, cy, ghostHoriz)) {
                    s->cx = cx; s->cy = cy; s->horiz = ghostHoriz; s->placed = 1;
                    stamp(myCell, myOwn, s, placing);
                    note(72, INSTR_SQUARE, 3);
                    // advance
                    do { placing++; } while (placing < NSHIP && myFleet[placing].placed);
                    if (all_placed()) set_banner("FLEET READY - CLICK READY");
                } else {
                    badFlash = 8; badFlashT = now();
                    note(48, INSTR_SQUARE, 2);
                }
            }
        }
        return;
    }

    // PH_BATTLE
    if (turn == 0) {
        if (mouse_pressed(MOUSE_LEFT)) {
            int cx, cy;
            if (cell_at(RGX, &cx, &cy)) player_fire(cx, cy);
        }
    } else {
        // cpu thinking pause then fire
        cpuTimer += dt();
        if (cpuTimer > 0.6f) cpu_fire();
    }
}

// ---- draw -----------------------------------------------------------------
static void draw_grid(int gx, const char *title, int hl) {
    print(title, gx, GY - 11, hl ? CLR_YELLOW : CLR_LIGHT_GREY);
    // ocean backdrop
    rectfill(gx, GY, N*CELL, N*CELL, CLR_DARK_BLUE);
    // grid lines
    for (int i = 0; i <= N; i++) {
        line(gx + i*CELL, GY, gx + i*CELL, GY + N*CELL, CLR_DARKER_BLUE);
        line(gx, GY + i*CELL, gx + N*CELL, GY + i*CELL, CLR_DARKER_BLUE);
    }
}

static void draw_peg(int gx, int x, int y, unsigned char st) {
    int px = gx + x*CELL + CELL/2;
    int py = GY + y*CELL + CELL/2;
    if (st & ST_HIT)  circfill(px, py, 3, CLR_RED);
    else if (st & ST_MISS) circfill(px, py, 2, CLR_WHITE);
}

static void draw_ship_block(int gx, int cx, int cy, int len, int horiz, int color) {
    for (int i = 0; i < len; i++) {
        int x = cx + (horiz ? i : 0);
        int y = cy + (horiz ? 0 : i);
        rectfill(gx + x*CELL + 1, GY + y*CELL + 1, CELL - 1, CELL - 1, color);
    }
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);

    // title bar
    rectfill(0, 0, SCREEN_W, 12, CLR_DARKER_BLUE);
    print_centered("BATTLESHIP", SCREEN_W/2, 3, CLR_LIGHT_PEACH);
    print(str("W:%d", wins), 4, 3, CLR_GREEN);
    print_right(str("L:%d", losses), SCREEN_W - 4, 3, CLR_RED);

    // --- YOUR GRID (left) ---
    draw_grid(LGX, "YOUR FLEET", phase != PH_SETUP && turn == 1);
    // your placed ships
    for (int i = 0; i < NSHIP; i++) {
        if (myFleet[i].placed)
            draw_ship_block(LGX, myFleet[i].cx, myFleet[i].cy,
                            myFleet[i].len, myFleet[i].horiz, CLR_DARK_GREY);
    }
    // enemy shots on your board
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++)
            draw_peg(LGX, x, y, myCell[y][x]);

    // --- ENEMY GRID (right) ---
    draw_grid(RGX, "ENEMY OCEAN", phase == PH_BATTLE && turn == 0);
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            unsigned char st = enCell[y][x];
            // reveal sunk ships in dark red so you can see your kills
            if ((st & ST_HIT) && enOwn[y][x] >= 0
                && enFleet[enOwn[y][x]].hits >= enFleet[enOwn[y][x]].len)
                rectfill(RGX + x*CELL + 1, GY + y*CELL + 1, CELL - 1, CELL - 1, CLR_DARK_RED);
            draw_peg(RGX, x, y, st);
        }

    // --- SETUP overlays ---
    if (phase == PH_SETUP) {
        // ghost ship preview
        if (!all_placed()) {
            int cx, cy;
            if (cell_at(LGX, &cx, &cy)) {
                int ok = fits(myCell, myFleet[placing].len, cx, cy, ghostHoriz);
                draw_ship_block(LGX, cx, cy, myFleet[placing].len, ghostHoriz,
                                ok ? CLR_GREEN : CLR_RED);
            }
        }
        // buttons
        int by = GY + N*CELL + 10;
        rectfill(8, by, 70, 16, CLR_BROWN);
        print("RANDOM", 18, by + 4, CLR_WHITE);
        int rdy = all_placed();
        rectfill(86, by, 70, 16, rdy ? CLR_DARK_GREEN : CLR_DARKER_GREY);
        print("READY", 102, by + 4, rdy ? CLR_WHITE : CLR_DARK_GREY);

        // which ship + rotate hint
        if (!all_placed()) {
            print(str("PLACING: %s (%d)", myFleet[placing].name,
                      myFleet[placing].len), 180, by + 1, CLR_YELLOW);
            print("R = ROTATE", 180, by + 11, CLR_LIGHT_GREY);
        } else {
            print("CLICK READY TO FIGHT", 180, by + 5, CLR_GREEN);
        }
        if (badFlash > 0 && (now() - badFlashT) < 0.25f) {
            print_centered("CAN'T PLACE THERE", SCREEN_W/2, GY + N*CELL + 32, CLR_RED);
        } else badFlash = 0;
    }

    // --- battle status line ---
    if (phase == PH_BATTLE) {
        int by = GY + N*CELL + 10;
        if (turn == 1)
            print_centered("ENEMY IS FIRING...", SCREEN_W/2, by + 3, CLR_ORANGE);
        else
            print_centered("CLICK AN ENEMY CELL TO FIRE", SCREEN_W/2, by + 3, CLR_LIGHT_GREY);
    }

    // --- banner ---
    if (banner[0] && (now() - bannerT) < 2.2f) {
        int w = text_width(banner) + 8;
        rectfill(SCREEN_W/2 - w/2, SCREEN_H - 26, w, 12, CLR_BLACK);
        print_centered(banner, SCREEN_W/2, SCREEN_H - 23, CLR_LIGHT_PEACH);
    }

    // --- game over --- the shared end-screen treatment (endcard.h)
    if (phase == PH_OVER) {
        EndCard c = endcard(end_t, 220, 52, SCREEN_H/2 - 26, CLR_DARK_BLUE,
                            playerWon ? CLR_YELLOW : CLR_RED, CLR_TRUE_BLUE);
        if (c.settled) {
            print_centered(playerWon ? "VICTORY!" : "DEFEAT", SCREEN_W/2, c.y + 10, playerWon ? CLR_YELLOW : CLR_RED);
            print_centered(banner, SCREEN_W/2, c.y + 24, CLR_WHITE);
            if (blink(18)) print_centered("CLICK OR Z TO PLAY AGAIN", SCREEN_W/2, c.y + 40, CLR_LIGHT_GREY);
        }
    }
}
