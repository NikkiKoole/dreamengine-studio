/* de:meta
{
  "title": "Puyo",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop",
    "state-machine"
  ],
  "lineage": "Puyo Puyo (1991) falling-pair puzzle; the flood-fill group detection and chain-reaction cascade are the core mechanics, implemented from scratch over a minimal state machine.",
  "genre": "puzzle",
  "homage": "Puyo Puyo (1991)",
  "description": "Drop pairs of colorful blobs into a narrow well, sliding and rotating them as they fall. Connect four or more of the same color and they pop in a flash of sparks — and when the collapsing stack lands into another match, it pops too, igniting a CHAIN that climbs in pitch and score the longer it runs. Chains are the whole game: a clean 5-chain rings out like a fanfare and sends the screen shaking. Stack to the top and it's over; your best score and longest chain are saved. Left/Right move, Up or Z rotate, Down soft-drop, Z to restart.",
  "todo": [
    "UI labels overlap all over the place.",
    "Needs a start panel.",
    "Bug: blocks that touch the ground overlap and then jump back.",
    "Touch: onscreen joystick.",
    "ui-audit: \"connect 4+ to pop\", \"chains = big score\" and \"press Z to start\" overlap the SCORE / 0 readouts."
  ]
}
de:meta */
#include "studio.h"
#include <stddef.h>

// PUYO
// Falling-pair color matcher. Rotate the pair, drop it, connect 4+ of one
// color to pop them. Pops drop the stack and can trigger CHAINS for big score.
// Left/Right: move   Up or Z: rotate   Down: soft drop   (Z restarts when over)

#define BW    6           // board width  (cells)
#define BH    12          // board height
#define CELL  14
#define BX    98          // board pixel origin
#define BY    24
#define NCOL  4           // number of puyo colors
#define EMPTY 0           // grid stores color+1; 0 = empty

// game states
#define ST_TITLE   0
#define ST_FALL    1      // a pair is falling under player control
#define ST_SETTLE  2      // locked puyos column-drop to rest
#define ST_POP     3      // a group is flashing before it vanishes
#define ST_DROP    4      // post-pop gravity, then re-resolve
#define ST_OVER    5

static const int PCOL[NCOL]  = { CLR_RED, CLR_GREEN, CLR_BLUE, CLR_YELLOW };
static const int PDARK[NCOL] = { CLR_DARK_RED, CLR_DARK_GREEN, CLR_TRUE_BLUE, CLR_ORANGE };

static int grid[BH][BW];          // 0 empty, else color+1

// falling pair: pivot puyo + a satellite at one of 4 orientations
static int pivot_c, sat_c;        // colors (0..NCOL-1)
static int pcx, pcy;              // pivot cell column/row (pcy can be fractional via pfall)
static int orient;                // 0=sat up, 1=right, 2=down, 3=left
static int next_a, next_b;        // previewed next pair colors
static float pfall;               // fractional fall progress 0..1 within a cell

static int state;
static int score, hiscore, max_chain, chain;
static float stimer;              // generic state timer (seconds)

// pop bookkeeping
static int popmark[BH][BW];       // 1 = this cell pops this step
static int pop_count;

// settle bookkeeping: per-column target for the two locked puyos
static int lock_cells[2][3];      // [i] = {col, row, color}, row = -1 if unused
static int n_lock;

// particle sparks
#define NSPARK 64
static struct { float x, y, vx, vy, life; int col; } spark[NSPARK];

static void emit_spark(float x, float y, int col) {
    for (int i = 0; i < NSPARK; i++) {
        if (spark[i].life <= 0) {
            float a = rnd_float_between(0, 360);
            float sp = rnd_float_between(20, 70);
            spark[i].x = x; spark[i].y = y;
            spark[i].vx = dx(sp, a); spark[i].vy = dy(sp, a);
            spark[i].life = rnd_float_between(0.25f, 0.5f);
            spark[i].col = col;
            return;
        }
    }
}

// ---- pair geometry -------------------------------------------------

static void sat_offset(int o, int *ox, int *oy) {
    switch (o & 3) {
        case 0: *ox = 0;  *oy = -1; break;  // up
        case 1: *ox = 1;  *oy = 0;  break;  // right
        case 2: *ox = 0;  *oy = 1;  break;  // down
        default: *ox = -1; *oy = 0; break;  // left
    }
}

static bool cell_free(int cx, int cy) {
    if (cx < 0 || cx >= BW) return false;
    if (cy >= BH) return false;
    if (cy < 0) return true;            // above the well is fine
    return grid[cy][cx] == EMPTY;
}

static bool pair_fits(int cx, int cy, int o) {
    int ox, oy; sat_offset(o, &ox, &oy);
    return cell_free(cx, cy) && cell_free(cx + ox, cy + oy);
}

// ---- spawn / reset -------------------------------------------------

static void spawn_pair() {
    pivot_c = next_a; sat_c = next_b;
    next_a = rnd(NCOL); next_b = rnd(NCOL);
    pcx = BW / 2 - 1; pcy = 0; orient = 0; pfall = 0;
    chain = 0;
    if (!pair_fits(pcx, pcy, orient)) {
        state = ST_OVER;
        if (score > hiscore) { hiscore = score; save(0, hiscore); }
        if (max_chain > load(1)) save(1, max_chain);
        note(36, INSTR_SAW, 4);
        shake(5);
    } else {
        state = ST_FALL;
    }
}

static void reset_game() {
    for (int y = 0; y < BH; y++) for (int x = 0; x < BW; x++) grid[y][x] = EMPTY;
    for (int i = 0; i < NSPARK; i++) spark[i].life = 0;
    score = 0; max_chain = 0; chain = 0;
    next_a = rnd(NCOL); next_b = rnd(NCOL);
    spawn_pair();
}

void init() {
    hiscore = load(0);
    max_chain = 0;
    instrument(5, INSTR_TRI, 4, 80, 4, 120);   // soft chain bell
    state = ST_TITLE;
    reset_game();
    state = ST_TITLE;
}

// ---- locking & settling -------------------------------------------

// once the pair can't fall, record its two puyos and switch to settle.
static void lock_pair() {
    int ox, oy; sat_offset(orient, &ox, &oy);
    int py = (int)pcy;
    n_lock = 0;
    lock_cells[n_lock][0] = pcx;       lock_cells[n_lock][1] = py;      lock_cells[n_lock][2] = pivot_c; n_lock++;
    lock_cells[n_lock][0] = pcx + ox;  lock_cells[n_lock][1] = py + oy; lock_cells[n_lock][2] = sat_c;   n_lock++;
    // place them in the grid first (so they can be settled by gravity below)
    for (int i = 0; i < n_lock; i++) {
        int cx = lock_cells[i][0], cy = lock_cells[i][1];
        if (cy < 0) cy = 0;
        if (cy >= 0 && cy < BH) grid[cy][cx] = lock_cells[i][2] + 1;
    }
    note(48, INSTR_SQUARE, 2);
    state = ST_SETTLE;
    stimer = 0;
}

// drop every puyo straight down into gaps (full-board gravity)
static bool apply_gravity() {
    bool moved = false;
    for (int x = 0; x < BW; x++) {
        int write = BH - 1;
        for (int y = BH - 1; y >= 0; y--) {
            if (grid[y][x] != EMPTY) {
                int v = grid[y][x];
                if (write != y) { grid[write][x] = v; grid[y][x] = EMPTY; moved = true; }
                write--;
            }
        }
    }
    return moved;
}

// ---- pop detection (flood fill) -----------------------------------

static int ff_stack[BH * BW][2];

static int flood(int sx, int sy, int color, int out[][2]) {
    int top = 0, n = 0;
    ff_stack[top][0] = sx; ff_stack[top][1] = sy; top++;
    static int seen[BH][BW];
    for (int y = 0; y < BH; y++) for (int x = 0; x < BW; x++) seen[y][x] = 0;
    seen[sy][sx] = 1;
    while (top > 0) {
        top--;
        int x = ff_stack[top][0], y = ff_stack[top][1];
        out[n][0] = x; out[n][1] = y; n++;
        int dxs[4] = { 1, -1, 0, 0 }, dys[4] = { 0, 0, 1, -1 };
        for (int d = 0; d < 4; d++) {
            int nx = x + dxs[d], ny = y + dys[d];
            if (nx < 0 || nx >= BW || ny < 0 || ny >= BH) continue;
            if (seen[ny][nx]) continue;
            if (grid[ny][nx] != color + 1) continue;
            seen[ny][nx] = 1;
            ff_stack[top][0] = nx; ff_stack[top][1] = ny; top++;
        }
    }
    return n;
}

// mark all groups of >=4 for popping. returns total cells marked.
static int mark_pops() {
    static int visited[BH][BW];
    for (int y = 0; y < BH; y++) for (int x = 0; x < BW; x++) { visited[y][x] = 0; popmark[y][x] = 0; }
    static int grp[BH * BW][2];
    int total = 0;
    for (int y = 0; y < BH; y++) for (int x = 0; x < BW; x++) {
        if (grid[y][x] == EMPTY || visited[y][x]) continue;
        int color = grid[y][x] - 1;
        int n = flood(x, y, color, grp);
        for (int i = 0; i < n; i++) visited[grp[i][1]][grp[i][0]] = 1;
        if (n >= 4) {
            for (int i = 0; i < n; i++) {
                popmark[grp[i][1]][grp[i][0]] = 1;
                emit_spark(BX + grp[i][0] * CELL + CELL / 2, BY + grp[i][1] * CELL + CELL / 2, color);
            }
            total += n;
        }
    }
    return total;
}

// begin the resolve cycle: gravity, then look for pops.
static void begin_resolve() {
    apply_gravity();
    pop_count = mark_pops();
    if (pop_count > 0) {
        chain++;
        if (chain > max_chain) max_chain = chain;
        // chain scoring: group size * 10 * (chain multiplier ramps)
        int mult = 1 << (chain - 1);          // 1,2,4,8,16...
        if (mult > 64) mult = 64;
        score += pop_count * 10 * mult;
        // rising sting per chain step
        int midi = 60 + (chain - 1) * 3;
        if (midi > 96) midi = 96;
        note(midi, 5, 5);
        if (chain >= 3) shake(2 + chain);
        else shake(2);
        if (chain >= 4) chord(midi, CHORD_MAJ, INSTR_SINE, 3);
        state = ST_POP;
        stimer = 0;
    } else {
        spawn_pair();   // settle done, no (more) pops
    }
}

// ---- update --------------------------------------------------------

static int das_dir, das_t;

static void update_fall() {
    // horizontal move with auto-repeat
    int dir = (btn(0, BTN_RIGHT) ? 1 : 0) - (btn(0, BTN_LEFT) ? 1 : 0);
    if (dir != 0) {
        if (dir != das_dir) {
            if (pair_fits(pcx + dir, (int)pcy, orient)) pcx += dir;
            das_dir = dir; das_t = 11;
        } else if (--das_t <= 0) {
            if (pair_fits(pcx + dir, (int)pcy, orient)) pcx += dir;
            das_t = 4;
        }
    } else das_dir = 0;

    // rotate clockwise (Up or Z), with a simple wall-kick
    if (btnp(0, BTN_UP) || btnp(0, BTN_A)) {
        int no = (orient + 1) & 3;
        if (pair_fits(pcx, (int)pcy, no)) { orient = no; note(67, INSTR_SQUARE, 1); }
        else if (pair_fits(pcx - 1, (int)pcy, no)) { orient = no; pcx -= 1; note(67, INSTR_SQUARE, 1); }
        else if (pair_fits(pcx + 1, (int)pcy, no)) { orient = no; pcx += 1; note(67, INSTR_SQUARE, 1); }
    }

    // gravity: fractional fall, soft-drop on Down
    bool soft = btn(0, BTN_DOWN);
    float rate = soft ? 9.0f : 1.6f;          // cells per second
    pfall += rate * dt();
    while (pfall >= 1.0f) {
        if (pair_fits(pcx, (int)pcy + 1, orient)) { pcy += 1; pfall -= 1.0f; if (soft) score += 1; }
        else { pfall = 0; lock_pair(); return; }
    }
}

void update() {
    // particles always tick
    for (int i = 0; i < NSPARK; i++) if (spark[i].life > 0) {
        spark[i].life -= dt();
        spark[i].x += spark[i].vx * dt();
        spark[i].y += spark[i].vy * dt();
        spark[i].vy += 140 * dt();
    }

    switch (state) {
        case ST_TITLE:
            if (btnp(0, BTN_A) || btnp(0, BTN_B) || btnp(0, BTN_UP) || key(KEY_ENTER)) {
                reset_game();
            }
            break;

        case ST_FALL:
            update_fall();
            break;

        case ST_SETTLE:
            // brief beat, then resolve (gravity + pops)
            stimer += dt();
            if (stimer > 0.06f) begin_resolve();
            break;

        case ST_POP:
            stimer += dt();
            if (stimer > 0.32f) {
                // actually remove the marked cells
                for (int y = 0; y < BH; y++) for (int x = 0; x < BW; x++)
                    if (popmark[y][x]) grid[y][x] = EMPTY;
                state = ST_DROP;
                stimer = 0;
            }
            break;

        case ST_DROP:
            stimer += dt();
            if (stimer > 0.10f) begin_resolve();   // re-resolve → chain
            break;

        case ST_OVER:
            if (btnp(0, BTN_A) || btnp(0, BTN_B) || key(KEY_ENTER)) reset_game();
            break;
    }
}

// ---- drawing -------------------------------------------------------

static void draw_panels(void);

static void puyo(int sx, int sy, int color, float scale) {
    int r = (int)((CELL / 2 - 1) * scale);
    if (r < 1) return;
    int cx = sx + CELL / 2, cy = sy + CELL / 2;
    circfill(cx, cy, r, PDARK[color]);
    circfill(cx, cy, r - 1, PCOL[color]);
    if (r > 3) {
        pset(cx - r / 3, cy - r / 3, CLR_WHITE);
        pset(cx - r / 3 + 1, cy - r / 3, CLR_WHITE);
    }
}

static void draw_board_bg() {
    rectfill(BX - 3, BY - 3, BW * CELL + 6, BH * CELL + 6, CLR_DARKER_BLUE);
    rect(BX - 3, BY - 3, BW * CELL + 6, BH * CELL + 6, CLR_INDIGO);
    rect(BX - 1, BY - 1, BW * CELL + 2, BH * CELL + 2, CLR_DARK_BLUE);
    // faint cell grid
    for (int x = 1; x < BW; x++) line(BX + x * CELL, BY, BX + x * CELL, BY + BH * CELL - 1, CLR_DARKER_BLUE);
}

static void draw_settled() {
    for (int y = 0; y < BH; y++) for (int x = 0; x < BW; x++) {
        if (grid[y][x] == EMPTY) continue;
        float sc = 1.0f;
        if (state == ST_POP && popmark[y][x]) {
            // flash + shrink out
            float t = clamp(stimer / 0.32f, 0, 1);
            sc = 1.0f - ease_out(t);
            if (((int)(stimer * 30)) % 2 == 0) {
                rectfill(BX + x * CELL + 1, BY + y * CELL + 1, CELL - 2, CELL - 2, CLR_WHITE);
                continue;
            }
        }
        puyo(BX + x * CELL, BY + y * CELL, grid[y][x] - 1, sc);
    }
}

static void draw_pair() {
    int ox, oy; sat_offset(orient, &ox, &oy);
    float yoff = pfall * CELL;
    int psx = BX + pcx * CELL;
    int psy = BY + (int)pcy * CELL + (int)yoff;
    puyo(psx, psy, pivot_c, 1.0f);
    int ssx = BX + (pcx + ox) * CELL;
    int ssy = BY + ((int)pcy + oy) * CELL + (int)yoff;
    if ((int)pcy + oy >= 0) puyo(ssx, ssy, sat_c, 1.0f);
}

static void draw_preview(int x, int y, int a, int b) {
    puyo(x, y, b, 0.85f);
    puyo(x, y + CELL, a, 0.85f);
}

void draw() {
    cls(CLR_BLACK);
    // backdrop wash
    for (int i = 0; i < SCREEN_H; i += 4) line(0, i, SCREEN_W, i, CLR_BROWNISH_BLACK);

    draw_board_bg();

    if (state == ST_TITLE) {
        print_centered("P U Y O", SCREEN_W/2, 60, CLR_GREEN);
        print_centered("connect 4+ to pop", SCREEN_W/2, 84, CLR_LIGHT_GREY);
        print_centered("chains = big score", SCREEN_W/2, 96, CLR_YELLOW);
        if (blink(20)) print_centered("press Z to start", SCREEN_W/2, 124, CLR_WHITE);
        // little demo cluster
        puyo(BX + 1 * CELL, BY + 9 * CELL, 0, 1);
        puyo(BX + 2 * CELL, BY + 9 * CELL, 0, 1);
        puyo(BX + 2 * CELL, BY + 8 * CELL, 1, 1);
        puyo(BX + 3 * CELL, BY + 9 * CELL, 2, 1);
        draw_panels();
        return;
    }

    draw_settled();
    if (state == ST_FALL) draw_pair();

    // particles
    for (int i = 0; i < NSPARK; i++) if (spark[i].life > 0)
        pset((int)spark[i].x, (int)spark[i].y, spark[i].col == -1 ? CLR_WHITE : PCOL[spark[i].col]);

    draw_panels();

    // chain popup
    if ((state == ST_POP || state == ST_DROP) && chain >= 2) {
        print_centered(str("%d CHAIN!", chain), SCREEN_W/2, BY + 4, CLR_YELLOW);
    }

    if (state == ST_OVER) {
        rectfill(BX - 6, SCREEN_H / 2 - 28, BW * CELL + 12, 60, CLR_BLACK);
        rect    (BX - 6, SCREEN_H / 2 - 28, BW * CELL + 12, 60, CLR_RED);
        print_centered("GAME OVER", SCREEN_W/2, SCREEN_H / 2 - 18, CLR_RED);
        print_centered(str("%d", score), SCREEN_W/2, SCREEN_H / 2 - 4, CLR_YELLOW);
        print_centered("Z to restart", SCREEN_W/2, SCREEN_H / 2 + 14, CLR_LIGHT_GREY);
    }
}

static void draw_panels(void) {
    // right panel — NEXT + stats
    int rx = BX + BW * CELL + 12;
    print("NEXT", rx, BY, CLR_LIGHT_GREY);
    draw_preview(rx + 6, BY + 12, next_a, next_b);

    print("SCORE", rx, 86, CLR_LIGHT_GREY);
    print(str("%d", score), rx, 96, CLR_WHITE);
    print("CHAIN", rx, 116, CLR_LIGHT_GREY);
    print(str("%d", max_chain), rx, 126, CLR_GREEN);

    // left panel — best
    print("BEST", 22, BY, CLR_LIGHT_GREY);
    print(str("%d", hiscore), 22, BY + 10, CLR_YELLOW);
    print("MAX", 22, BY + 30, CLR_LIGHT_GREY);
    print("CHAIN", 22, BY + 40, CLR_LIGHT_GREY);
    print(str("%d", load(1)), 22, BY + 50, CLR_GREEN);

    print_right("PUYO", SCREEN_W - 4, 4, CLR_INDIGO);
}
