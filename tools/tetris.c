#include "studio.h"

// TETRIS
// Stack the falling tetrominoes; complete a horizontal line to clear it.
// Left/Right: move    Down: soft drop    Up: hard drop
// Z: rotate left      X: rotate right
// Speed climbs every 10 lines. Best score is saved.

#define BW   10          // board width  (cells)
#define BH   20          // board height
#define CELL 8
#define BX   120         // board pixel origin
#define BY   20

// each piece = 4 rotation states, each a 4x4 bitmask (bit = row*4 + col)
static const unsigned short PIECE[7][4] = {
    { 0x00F0, 0x2222, 0x00F0, 0x2222 },   // I
    { 0x0066, 0x0066, 0x0066, 0x0066 },   // O
    { 0x0072, 0x0262, 0x0270, 0x0232 },   // T
    { 0x0036, 0x0462, 0x0036, 0x0462 },   // S
    { 0x0063, 0x0264, 0x0063, 0x0264 },   // Z
    { 0x0071, 0x0226, 0x0470, 0x0322 },   // J
    { 0x0074, 0x0622, 0x0170, 0x0223 },   // L
};
static const int PCOL[7] = { CLR_BLUE, CLR_YELLOW, CLR_INDIGO, CLR_GREEN, CLR_RED, CLR_TRUE_BLUE, CLR_ORANGE };

static int   board[BH][BW];          // 0 empty, else colour index + 1
static int   cur, rot, cx, cy, nxt;
static int   score, hiscore, lines, level, state;   // state: 0 play, 1 over
static int   fall_count, das_dir, das_t;
static int   flash_t;                // brief flash after a clear

// ====================================================================

static bool cell_set(int type, int r, int c, int rr) {
    return (PIECE[type][r] >> (rr * 4 + c)) & 1;
}

static bool collide(int type, int r, int px, int py) {
    for (int rr = 0; rr < 4; rr++)
        for (int cc = 0; cc < 4; cc++) {
            if (!cell_set(type, r, cc, rr)) continue;
            int bx = px + cc, by = py + rr;
            if (bx < 0 || bx >= BW || by >= BH) return true;
            if (by >= 0 && board[by][bx]) return true;
        }
    return false;
}

static void spawn() {
    cur = nxt; nxt = rnd(7);
    rot = 0; cx = 3; cy = 0;
    fall_count = 0;
    if (collide(cur, rot, cx, cy)) {
        state = 1;
        if (score > hiscore) { hiscore = score; save(0, hiscore); }
    }
}

static void reset_game() {
    for (int y = 0; y < BH; y++) for (int x = 0; x < BW; x++) board[y][x] = 0;
    score = 0; lines = 0; level = 1; state = 0; das_dir = 0; flash_t = 0;
    nxt = rnd(7);
    spawn();
}

void init() { hiscore = load(0); reset_game(); }

static bool try_move(int dx, int dy) {
    if (collide(cur, rot, cx + dx, cy + dy)) return false;
    cx += dx; cy += dy; return true;
}

static void rotate(int dir) {
    int nr = (rot + dir + 4) % 4;
    int kicks[5] = { 0, -1, 1, -2, 2 };
    for (int k = 0; k < 5; k++)
        if (!collide(cur, nr, cx + kicks[k], cy)) {
            rot = nr; cx += kicks[k];
            note(67, INSTR_SQUARE, 1);
            return;
        }
}

static void clear_lines() {
    int cleared = 0;
    for (int y = BH - 1; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < BW; x++) if (!board[y][x]) { full = false; break; }
        if (full) {
            cleared++;
            for (int yy = y; yy > 0; yy--)
                for (int x = 0; x < BW; x++) board[yy][x] = board[yy - 1][x];
            for (int x = 0; x < BW; x++) board[0][x] = 0;
            y++;   // re-check the row that shifted down
        }
    }
    if (cleared > 0) {
        static const int pts[5] = { 0, 100, 300, 500, 800 };
        score += pts[cleared] * level;
        lines += cleared;
        level = 1 + lines / 10;
        flash_t = 8;
        hit(cleared >= 4 ? 84 : 72, INSTR_SINE, 4, 120);
    }
}

static void lock_piece() {
    for (int rr = 0; rr < 4; rr++)
        for (int cc = 0; cc < 4; cc++)
            if (cell_set(cur, rot, cc, rr)) {
                int by = cy + rr, bx = cx + cc;
                if (by >= 0) board[by][bx] = PCOL[cur] + 1;
            }
    note(48, INSTR_SQUARE, 2);
    clear_lines();
    spawn();
}

void update() {
    if (flash_t > 0) flash_t--;
    if (state == 1) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) reset_game();
        return;
    }

    // horizontal move with auto-repeat
    int dir = (btn(0, BTN_RIGHT) ? 1 : 0) - (btn(0, BTN_LEFT) ? 1 : 0);
    if (dir != 0) {
        if (dir != das_dir) { try_move(dir, 0); das_dir = dir; das_t = 12; }
        else if (--das_t <= 0) { try_move(dir, 0); das_t = 3; }
    } else das_dir = 0;

    if (btnp(0, BTN_A)) rotate(-1);
    if (btnp(0, BTN_B)) rotate(1);

    // hard drop
    if (btnp(0, BTN_UP)) {
        while (try_move(0, 1)) score += 2;
        lock_piece();
        return;
    }

    // gravity (soft drop on Down)
    bool soft = btn(0, BTN_DOWN);
    int delay = max(3, 48 - (level - 1) * 4);
    if (soft) delay = 2;
    if (++fall_count >= delay) {
        fall_count = 0;
        if (!try_move(0, 1)) lock_piece();
        else if (soft) score += 1;
    }
}

// ====================================================================
// drawing
// ====================================================================

static void block(int sx, int sy, int col) {
    rectfill(sx + 1, sy + 1, CELL - 1, CELL - 1, col);
    pset(sx + 1, sy + 1, CLR_WHITE);                 // tiny highlight
}

static int ghost_y() {
    int gy = cy;
    while (!collide(cur, rot, cx, gy + 1)) gy++;
    return gy;
}

void draw() {
    cls(CLR_BLACK);

    // playfield well
    rectfill(BX - 2, BY - 2, BW * CELL + 4, BH * CELL + 4, CLR_DARKER_BLUE);
    rect(BX - 2, BY - 2, BW * CELL + 4, BH * CELL + 4, CLR_LIGHT_GREY);

    // settled blocks
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++)
            if (board[y][x]) block(BX + x * CELL, BY + y * CELL, board[y][x] - 1);

    if (state == 0) {
        // ghost (landing preview)
        int gy = ghost_y();
        for (int rr = 0; rr < 4; rr++)
            for (int cc = 0; cc < 4; cc++)
                if (cell_set(cur, rot, cc, rr)) {
                    int sx = BX + (cx + cc) * CELL, sy = BY + (gy + rr) * CELL;
                    rect(sx + 1, sy + 1, CELL - 1, CELL - 1, PCOL[cur]);
                }
        // active piece
        for (int rr = 0; rr < 4; rr++)
            for (int cc = 0; cc < 4; cc++)
                if (cell_set(cur, rot, cc, rr) && cy + rr >= 0)
                    block(BX + (cx + cc) * CELL, BY + (cy + rr) * CELL, PCOL[cur]);
    }

    // line-clear flash
    if (flash_t > 0 && (flash_t / 2) % 2)
        rectfill(BX, BY, BW * CELL, BH * CELL, CLR_WHITE);

    // left panel — stats
    print("SCORE", 12, 30, CLR_LIGHT_GREY);
    print(str("%d", score), 12, 40, CLR_WHITE);
    print("LINES", 12, 60, CLR_LIGHT_GREY);
    print(str("%d", lines), 12, 70, CLR_WHITE);
    print("LEVEL", 12, 90, CLR_LIGHT_GREY);
    print(str("%d", level), 12, 100, CLR_WHITE);
    print("BEST", 12, 124, CLR_LIGHT_GREY);
    print(str("%d", hiscore), 12, 134, CLR_YELLOW);

    // right panel — next piece
    print("NEXT", 224, 30, CLR_LIGHT_GREY);
    for (int rr = 0; rr < 4; rr++)
        for (int cc = 0; cc < 4; cc++)
            if ((PIECE[nxt][0] >> (rr * 4 + cc)) & 1)
                block(224 + cc * CELL, 44 + rr * CELL, PCOL[nxt]);

    print_centered("TETRIS", 4, CLR_GREEN);

    if (state == 1) {
        rectfill(BX - 4, SCREEN_H / 2 - 24, BW * CELL + 8, 50, CLR_BLACK);
        rect    (BX - 4, SCREEN_H / 2 - 24, BW * CELL + 8, 50, CLR_RED);
        print_centered("GAME OVER",            SCREEN_H / 2 - 14, CLR_RED);
        print_centered(str("%d", score),       SCREEN_H / 2 - 1,  CLR_YELLOW);
        print_centered("Z to restart",         SCREEN_H / 2 + 13, CLR_LIGHT_GREY);
    }
}
