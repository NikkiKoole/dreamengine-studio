#include "studio.h"
#include <stddef.h>

// ── QBERT — hop the pyramid ───────────────────────────────────
// A pyramid of 28 cubes (7 rows), drawn as isometric diamonds from
// pure primitives. Hop DIAGONALLY cube-to-cube; each landing flips the
// cube's top to the target color. Paint all 28 to clear the board.
// Hop off the edge and you fall to your death. Coily, a purple snake,
// bounces down the pyramid chasing you.
//
//   Q / E       — hop Up-Left / Up-Right
//   A / D       — hop Down-Left / Down-Right
//   arrows      — Left/Up = UL/UR,  Down/Right = DL/DR (diagonal map)
//   Z or ENTER  — restart after game over

#define ROWS     7
#define TOP_Y    34
#define CUBE_DX  28        // half-width of a cube top (also row fan-out per col)
#define CUBE_DY  30        // vertical step per row
#define CUBE_TH  18        // cube side height
#define HOP_TIME 0.26f     // seconds per hop

// move directions
enum { UL, UR, DL, DR };

typedef struct { int row, col; } Cell;

// ── board ─────────────────────────────────────────────────────
static int  flipped[ROWS][ROWS];   // 1 = painted
static int  painted;               // count
static int  total = 28;

// ── player ────────────────────────────────────────────────────
static Cell pcell;          // resting cell
static Cell pfrom, pto;     // hop endpoints (cell coords; may be off-grid)
static float phop;          // 0..1 hop progress; >=1 means resting
static bool  pfalling;      // hop target is off the pyramid

// ── coily ─────────────────────────────────────────────────────
static Cell ccell, cfrom, cto;
static float chop;
static bool  calive;
static float chatch;        // seconds until Coily appears
static float cstep_t;       // cooldown between Coily hops

// ── game state ────────────────────────────────────────────────
static int  lives, score, level, hi;
static bool gameover, won;
static float respawn_t;     // >0 = frozen, counting down to respawn
static int   start_done;

// ── iso projection: cell center on screen ─────────────────────
static float cellX(int row, int col) {
    return SCREEN_W / 2.0f + (col - row / 2.0f) * CUBE_DX;
}
static float cellY(int row, int col) {
    (void)col;
    return TOP_Y + row * CUBE_DY;
}

static bool on_board(int row, int col) {
    return row >= 0 && row < ROWS && col >= 0 && col <= row;
}

// where does a move land?
static Cell target(Cell c, int dir) {
    Cell t = c;
    switch (dir) {
        case UL: t.row = c.row - 1; t.col = c.col - 1; break;
        case UR: t.row = c.row - 1; t.col = c.col;     break;
        case DL: t.row = c.row + 1; t.col = c.col;     break;
        case DR: t.row = c.row + 1; t.col = c.col + 1; break;
    }
    return t;
}

// ── one cube: top diamond + two shaded sides ──────────────────
static void draw_cube(int row, int col) {
    float cx = cellX(row, col);
    float cy = cellY(row, col);
    int hw = CUBE_DX, hh = CUBE_DX / 2;     // top diamond half sizes
    int th = CUBE_TH;

    int top  = flipped[row][col] ? CLR_YELLOW    : CLR_TRUE_BLUE;
    int left = flipped[row][col] ? CLR_ORANGE    : CLR_DARK_BLUE;
    int rite = flipped[row][col] ? CLR_DARK_ORANGE : CLR_DARKER_BLUE;

    int tx = (int)cx, ty = (int)cy;
    // top diamond corners
    int N = ty - hh, S = ty + hh, Wx = tx - hw, Ex = tx + hw;
    // left face (parallelogram: W, S, S+th, W+th)
    trifill(Wx, ty, tx, S, Wx, ty + th, left);
    trifill(tx, S, tx, S + th, Wx, ty + th, left);
    // right face
    trifill(tx, S, Ex, ty, Ex, ty + th, rite);
    trifill(tx, S, Ex, ty + th, tx, S + th, rite);
    // top diamond
    trifill(Wx, ty, tx, N, Ex, ty, top);
    trifill(Wx, ty, Ex, ty, tx, S, top);
    // crisp top rim
    line(Wx, ty, tx, N, CLR_LIGHT_PEACH);
    line(tx, N, Ex, ty, CLR_LIGHT_PEACH);
    line(Wx, ty, tx, S, CLR_DARK_BLUE);
    line(Ex, ty, tx, S, CLR_DARK_BLUE);
}

// ── critter render (player + coily share the shape) ───────────
static void draw_qbert(float x, float y, float lift) {
    int sx = (int)x, sy = (int)(y - lift);
    circfill(sx, sy + 5, 7, CLR_BROWNISH_BLACK);     // shadow-ish base under feet
    circfill(sx, sy, 8, CLR_ORANGE);                 // body
    circfill(sx, sy - 1, 7, CLR_DARK_ORANGE);        // shade
    circfill(sx, sy - 3, 6, CLR_ORANGE);
    // snout
    rectfill(sx - 2, sy - 1, 7, 4, CLR_LIGHT_PEACH);
    // eyes
    circfill(sx - 1, sy - 5, 2, CLR_WHITE);
    circfill(sx + 3, sy - 5, 2, CLR_WHITE);
    pset(sx - 1, sy - 5, CLR_BLACK);
    pset(sx + 3, sy - 5, CLR_BLACK);
}

static void draw_coily(float x, float y, float lift) {
    int sx = (int)x, sy = (int)(y - lift);
    circfill(sx, sy + 5, 7, CLR_BROWNISH_BLACK);
    // coiled body
    circfill(sx, sy + 2, 8, CLR_DARK_PURPLE);
    circfill(sx, sy - 2, 6, CLR_DARK_PURPLE);
    // head
    circfill(sx, sy - 8, 5, CLR_PINK);
    circfill(sx - 2, sy - 9, 1, CLR_WHITE);
    circfill(sx + 2, sy - 9, 1, CLR_WHITE);
}

// ── hop math: position along a parabolic arc ──────────────────
static void hop_pos(Cell from, Cell to, float t, float *ox, float *oy, float *olift) {
    float e = ease_out(t);
    float fx = cellX(from.row, from.col), fy = cellY(from.row, from.col);
    float txp, typ;
    if (on_board(to.row, to.col)) {
        txp = cellX(to.row, to.col); typ = cellY(to.row, to.col);
    } else {
        // off the edge — extrapolate one iso step in the move direction
        txp = fx + ((to.col - from.col) - (to.row - from.row) / 2.0f) * CUBE_DX;
        typ = fy + (to.row - from.row) * CUBE_DY;
    }
    *ox = lerp(fx, txp, e);
    *oy = lerp(fy, typ, e);
    *olift = sin_deg(t * 180.0f) * 14.0f;
    if (!on_board(to.row, to.col) && t > 0.5f) {
        // after apex, plummet
        *oy += (t - 0.5f) * (t - 0.5f) * 600.0f;
        *olift = 0;
    }
}

// ── lifecycle ─────────────────────────────────────────────────
static void reset_board(void) {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < ROWS; c++) flipped[r][c] = 0;
    painted = 0;
    pcell = (Cell){ 0, 0 };
    pfrom = pto = pcell;
    phop = 1.0f; pfalling = false;
    calive = false; chop = 1.0f;
    chatch = 2.6f - level * 0.25f; if (chatch < 0.9f) chatch = 0.9f;
    cstep_t = 0;
    won = false;
}

static void new_game(void) {
    lives = 3; score = 0; level = 0;
    gameover = false; won = false;
    respawn_t = 0;
    reset_board();
    hi = load_int("qbert_hi", 0);
    start_done = 1;
}

void init(void) { new_game(); }

static void flip_landing(void) {
    if (!flipped[pcell.row][pcell.col]) {
        flipped[pcell.row][pcell.col] = 1;
        painted++;
        score += 25;
        note(72, INSTR_SQUARE, 4);
        if (painted >= total) {
            won = true;
            strum(60, CHORD_MAJ7, INSTR_TRI, 5, 50);
            score += 500;
        }
    } else {
        note(64, INSTR_SQUARE, 3);   // already painted — softer
    }
}

static void start_hop(int dir) {
    pfrom = pcell;
    pto = target(pcell, dir);
    phop = 0.0f;
    pfalling = !on_board(pto.row, pto.col);
    note(80, INSTR_SQUARE, 3);
}

static void die(void) {
    lives--;
    shake(6);
    // descending death arpeggio
    schedule(0,   67, INSTR_TRI, 5);
    schedule(120, 60, INSTR_TRI, 5);
    schedule(240, 53, INSTR_TRI, 5);
    schedule(360, 46, INSTR_NOISE, 5);
    if (lives <= 0) {
        gameover = true;
        if (score > hi) { hi = score; save_int("qbert_hi", hi); }
    } else {
        respawn_t = 0.9f;
    }
}

static void respawn(void) {
    pcell = (Cell){ 0, 0 };
    pfrom = pto = pcell;
    phop = 1.0f; pfalling = false;
    calive = false; chop = 1.0f;
    chatch = 2.2f - level * 0.2f; if (chatch < 0.8f) chatch = 0.8f;
}

// ── Coily AI: chase down the pyramid ──────────────────────────
static void coily_choose(void) {
    cfrom = ccell;
    Cell best = ccell;
    float bestd = 1e9f;
    int dirs[2] = { DL, DR };
    // Coily only descends toward the player (classic behavior)
    for (int i = 0; i < 2; i++) {
        Cell t = target(ccell, dirs[i]);
        if (!on_board(t.row, t.col)) continue;
        float d = distance((int)cellX(t.row, t.col), (int)cellY(t.row, t.col),
                           (int)cellX(pcell.row, pcell.col), (int)cellY(pcell.row, pcell.col));
        if (d < bestd) { bestd = d; best = t; }
    }
    if (best.row == ccell.row && best.col == ccell.col) {
        // at the bottom — climb back toward the player instead
        int up[2] = { UL, UR };
        for (int i = 0; i < 2; i++) {
            Cell t = target(ccell, up[i]);
            if (!on_board(t.row, t.col)) continue;
            float d = distance((int)cellX(t.row, t.col), (int)cellY(t.row, t.col),
                               (int)cellX(pcell.row, pcell.col), (int)cellY(pcell.row, pcell.col));
            if (d < bestd) { bestd = d; best = t; }
        }
    }
    cto = best;
    chop = 0.0f;
}

void update(void) {
    if (!start_done) new_game();

    if (gameover) {
        if (keyp('Z') || keyp(KEY_ENTER) || btnp(0, BTN_A)) new_game();
        return;
    }

    // frozen during respawn pause / win
    if (respawn_t > 0) {
        respawn_t -= dt();
        if (respawn_t <= 0) respawn();
        return;
    }
    if (won) {
        if (keyp('Z') || keyp(KEY_ENTER) || btnp(0, BTN_A)) {
            level++; score += 100; reset_board();
        }
        return;
    }

    // ── player hop in progress ──
    if (phop < 1.0f) {
        phop += dt() / HOP_TIME;
        if (phop >= 1.0f) {
            phop = 1.0f;
            if (pfalling) { die(); return; }
            pcell = pto;
            flip_landing();
        }
    } else {
        // resting — accept input (turn-based feel)
        int dir = -1;
        if      (keyp('Q') || keyp(KEY_LEFT)) dir = UL;
        else if (keyp('E') || keyp(KEY_UP))   dir = UR;
        else if (keyp('A') || keyp(KEY_DOWN)) dir = DL;
        else if (keyp('D') || keyp(KEY_RIGHT))dir = DR;
        if (dir >= 0) start_hop(dir);
    }

    // ── Coily ──
    if (!calive) {
        chatch -= dt();
        if (chatch <= 0) {
            calive = true;
            ccell = (Cell){ 0, 0 };
            cfrom = cto = ccell;
            chop = 1.0f;
            cstep_t = 0.4f;
            note(50, INSTR_SAW, 4);
        }
    } else {
        if (chop < 1.0f) {
            chop += dt() / (HOP_TIME + 0.06f);
            if (chop >= 1.0f) { chop = 1.0f; ccell = cto; }
        } else {
            cstep_t -= dt();
            if (cstep_t <= 0) {
                cstep_t = 0.42f - level * 0.02f;
                if (cstep_t < 0.2f) cstep_t = 0.2f;
                coily_choose();
                if (cto.row == ccell.row && cto.col == ccell.col) chop = 1.0f;
            }
        }
    }

    // ── collision: player vs Coily (only when player is resting/landed) ──
    if (calive && phop >= 1.0f && !pfalling) {
        if (ccell.row == pcell.row && ccell.col == pcell.col &&
            chop >= 1.0f) {
            die();
            return;
        }
        // also catch mid-hop overlap by cell
        if (cto.row == pcell.row && cto.col == pcell.col && chop >= 0.6f) {
            die();
            return;
        }
    }
}

// ── render ────────────────────────────────────────────────────
void draw(void) {
    if (!start_done) new_game();
    cls(CLR_BLACK);

    // subtle starfield void
    for (int i = 0; i < 26; i++) {
        int x = (i * 53 + 11) % SCREEN_W;
        int y = (i * 29 + 7) % (SCREEN_H);
        if (blink(40 + i)) pset(x, y, CLR_DARK_GREY);
    }

    // pyramid — draw back-to-front (top rows first) so sides overlap right
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c <= r; c++)
            draw_cube(r, c);

    // player
    float px, py, plift;
    if (phop < 1.0f) hop_pos(pfrom, pto, phop, &px, &py, &plift);
    else { px = cellX(pcell.row, pcell.col); py = cellY(pcell.row, pcell.col); plift = 0; }

    // Coily
    if (calive) {
        float cx, cy, clift;
        if (chop < 1.0f) hop_pos(cfrom, cto, chop, &cx, &cy, &clift);
        else { cx = cellX(ccell.row, ccell.col); cy = cellY(ccell.row, ccell.col); clift = 0; }
        // depth order: whoever is on a lower row draws last
        if (ccell.row < pcell.row) { draw_coily(cx, cy, clift); draw_qbert(px, py, plift); }
        else { draw_qbert(px, py, plift); draw_coily(cx, cy, clift); }
    } else {
        draw_qbert(px, py, plift);
    }

    // ── HUD ──
    rectfill(0, 0, SCREEN_W, 12, CLR_BLACK);
    print("Q*BERT", 4, 2, CLR_ORANGE);
    print(str("SCORE %d", score), 70, 2, CLR_WHITE);
    print_right(str("HI %d", hi), SCREEN_W - 4, 2, CLR_LIGHT_YELLOW);
    // lives as little orange dots
    for (int i = 0; i < lives; i++)
        circfill(SCREEN_W - 70 + i * 9, 6, 3, CLR_ORANGE);
    // paint progress
    bar(150, 4, 70, 5, total ? (float)painted / total : 0, CLR_YELLOW, CLR_DARK_GREY);

    print_centered("Q E A D = diagonal hops   paint every cube", SCREEN_H - 9, CLR_DARK_GREY);

    if (won) {
        fade(0.35f);
        int w = 230, bx = (SCREEN_W - w) / 2;
        rectfill(bx, 80, w, 34, CLR_DARK_GREEN);
        rect(bx, 80, w, 34, CLR_WHITE);
        print_centered("BOARD CLEAR!", 88, CLR_YELLOW);
        print_centered("press Z for the next pyramid", 100, CLR_WHITE);
    }
    if (gameover) {
        fade(0.5f);
        int w = 230, bx = (SCREEN_W - w) / 2;
        rectfill(bx, 78, w, 40, CLR_DARK_PURPLE);
        rect(bx, 78, w, 40, CLR_WHITE);
        print_centered("GAME OVER", 86, CLR_RED);
        print_centered(str("score %d   hi %d", score, hi), 98, CLR_WHITE);
        print_centered("press Z to play again", 108, CLR_LIGHT_GREY);
    }
}
