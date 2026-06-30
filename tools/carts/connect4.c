/* de:meta
{
  "title": "Connect Four",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "save-load-persistence",
    "finite-state-ai"
  ],
  "lineage": "Classic Connect Four against a depth-5 alpha-beta minimax AI; win/loss/draw tally persisted across sessions.",
  "genre": "tabletop",
  "description": "The kitchen-table classic against a real opponent. Discs clatter down a blue 7x6 grid — you drop red, a depth-5 alpha-beta minimax AI answers in yellow, each disc accelerating under gravity into the lowest open slot with a rising blip and a tiny screen kick on landing. Line up four in any direction — horizontal, vertical, or diagonal — and the winning run flashes white while a major chord rings out; lose and the board sinks under a low saw growl; fill it with nobody connecting for a draw. Your running win/loss/draw tally is saved between runs. Hover a column and left-click to drop, or use A/D or Left/Right to aim and Space/Z to drop; click or Space/Z to play again.",
  "todo": [
    "Text overflows the bottom panel — drop to a smaller font where it clips.",
    "Dropping a disc should animate sliding down the column, not appear instantly.",
    "Add a jazzy background loop (brush snare + upright bass) via the generative-music layer.",
    "ui-audit: the D / L / YOUR TURN status labels overlap each other."
  ]
}
de:meta */
#include "studio.h"
#include <string.h>

// CONNECT FOUR
// Drop discs into a 7x6 grid; line up four to win. You (red) vs a minimax AI (yellow).
// Mouse: hover a column, click to drop.  Keys: A/D or Left/Right to aim, Space/Z to drop.
// Click or Space/Z to play again. Win/loss/draw tally is saved.

#define COLS 7
#define ROWS 6
#define EMPTY 0
#define HUMAN 1   // red
#define AI    2   // yellow

#define CELL 26
#define BX   ((SCREEN_W - COLS * CELL) / 2)   // board pixel origin (top-left of cells)
#define BY   30
#define RAD  10                                // disc radius

static int  grid[ROWS][COLS];      // [row 0 = top] EMPTY / HUMAN / AI
static int  cursor;                // selected column (keyboard)
static int  turn;                  // whose move: HUMAN or AI
static int  state;                 // 0 play, 1 win, 2 draw
static int  winner;                // HUMAN / AI when state==1
static int  winr[4], winc[4];      // four winning cells (when state==1)
static int  flash_t;               // win-celebration timer

// falling disc animation
static bool  falling;
static int   fall_col, fall_row, fall_who;
static float fall_y;               // current pixel y of the disc center
static float fall_target;          // resting pixel y

// AI deliberation delay so it doesn't move instantly
static float ai_think;

// stats
static int wins, losses, draws;

// ---------------------------------------------------------------- helpers

static int cell_cx(int c) { return BX + c * CELL + CELL / 2; }
static int cell_cy(int r) { return BY + r * CELL + CELL / 2; }

// lowest empty row in a column, or -1 if full (works on any board)
static int drop_row(int b[ROWS][COLS], int c) {
    for (int r = ROWS - 1; r >= 0; r--)
        if (b[r][c] == EMPTY) return r;
    return -1;
}

static bool board_full(int b[ROWS][COLS]) {
    for (int c = 0; c < COLS; c++) if (b[0][c] == EMPTY) return false;
    return true;
}

// find a 4-run for `who`; if found, fill out[] cells (rout/cout) and return true
static bool find_four(int b[ROWS][COLS], int who, int *rout, int *cout) {
    static const int dr[4] = { 0, 1, 1, 1 };
    static const int dc[4] = { 1, 0, 1, -1 };
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            if (b[r][c] != who) continue;
            for (int d = 0; d < 4; d++) {
                int er = r + dr[d] * 3, ec = c + dc[d] * 3;
                if (er < 0 || er >= ROWS || ec < 0 || ec >= COLS) continue;
                bool ok = true;
                for (int k = 1; k < 4; k++)
                    if (b[r + dr[d] * k][c + dc[d] * k] != who) { ok = false; break; }
                if (ok) {
                    if (rout)
                        for (int k = 0; k < 4; k++) {
                            rout[k] = r + dr[d] * k;
                            cout[k] = c + dc[d] * k;
                        }
                    return true;
                }
            }
        }
    return false;
}

// ---------------------------------------------------------------- minimax AI

#define AI_DEPTH 5
#define WIN_SCORE 100000

// score a length-4 window for the AI's perspective
static int score_window(int a, int b2, int c2, int d) {
    int ai = 0, hu = 0;
    int w[4] = { a, b2, c2, d };
    for (int i = 0; i < 4; i++) {
        if (w[i] == AI) ai++;
        else if (w[i] == HUMAN) hu++;
    }
    if (ai > 0 && hu > 0) return 0;        // mixed window, dead
    if (ai == 3) return 50;
    if (ai == 2) return 10;
    if (ai == 1) return 1;
    if (hu == 3) return -60;               // block their threats slightly harder
    if (hu == 2) return -10;
    if (hu == 1) return -1;
    return 0;
}

static int evaluate(int b[ROWS][COLS]) {
    int s = 0;
    // center column control
    for (int r = 0; r < ROWS; r++)
        if (b[r][COLS / 2] == AI) s += 6;
        else if (b[r][COLS / 2] == HUMAN) s -= 6;
    // all length-4 windows
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            if (c + 3 < COLS)
                s += score_window(b[r][c], b[r][c+1], b[r][c+2], b[r][c+3]);
            if (r + 3 < ROWS)
                s += score_window(b[r][c], b[r+1][c], b[r+2][c], b[r+3][c]);
            if (c + 3 < COLS && r + 3 < ROWS)
                s += score_window(b[r][c], b[r+1][c+1], b[r+2][c+2], b[r+3][c+3]);
            if (c - 3 >= 0 && r + 3 < ROWS)
                s += score_window(b[r][c], b[r+1][c-1], b[r+2][c-2], b[r+3][c-3]);
        }
    return s;
}

// search columns center-out for better pruning
static const int COL_ORDER[COLS] = { 3, 2, 4, 1, 5, 0, 6 };

static int minimax(int b[ROWS][COLS], int depth, int alpha, int beta, bool maxer) {
    if (find_four(b, AI, NULL, NULL))    return  WIN_SCORE + depth;
    if (find_four(b, HUMAN, NULL, NULL)) return -WIN_SCORE - depth;
    if (depth == 0 || board_full(b))     return evaluate(b);

    if (maxer) {
        int best = -1000000;
        for (int i = 0; i < COLS; i++) {
            int c = COL_ORDER[i];
            int r = drop_row(b, c);
            if (r < 0) continue;
            b[r][c] = AI;
            int v = minimax(b, depth - 1, alpha, beta, false);
            b[r][c] = EMPTY;
            if (v > best) best = v;
            if (best > alpha) alpha = best;
            if (alpha >= beta) break;
        }
        return best;
    } else {
        int best = 1000000;
        for (int i = 0; i < COLS; i++) {
            int c = COL_ORDER[i];
            int r = drop_row(b, c);
            if (r < 0) continue;
            b[r][c] = HUMAN;
            int v = minimax(b, depth - 1, alpha, beta, true);
            b[r][c] = EMPTY;
            if (v < best) best = v;
            if (best < beta) beta = best;
            if (alpha >= beta) break;
        }
        return best;
    }
}

static int ai_pick(void) {
    int best = -2000000, bestcol = 3;
    int tmp[ROWS][COLS];
    for (int i = 0; i < COLS; i++) {
        int c = COL_ORDER[i];
        int r = drop_row(grid, c);
        if (r < 0) continue;
        memcpy(tmp, grid, sizeof(grid));
        tmp[r][c] = AI;
        int v = minimax(tmp, AI_DEPTH - 1, -2000000, 2000000, false);
        if (v > best) { best = v; bestcol = c; }
    }
    return bestcol;
}

// ---------------------------------------------------------------- flow

static void reset_game(void) {
    memset(grid, 0, sizeof(grid));
    cursor = 3;
    turn = HUMAN;
    state = 0;
    winner = 0;
    flash_t = 0;
    falling = false;
    ai_think = 0;
}

void init(void) {
    wins   = load(0);
    losses = load(1);
    draws  = load(2);
    instrument(5, INSTR_SINE, 4, 80, 4, 120);   // soft drop blip
    reset_game();
}

// begin dropping `who`'s disc into column c (must be a legal move)
static void begin_drop(int c, int who) {
    int r = drop_row(grid, c);
    if (r < 0) return;
    falling     = true;
    fall_col    = c;
    fall_row    = r;
    fall_who    = who;
    fall_y      = BY - CELL / 2;        // start just above the board
    fall_target = cell_cy(r);
}

static void resolve_landing(void) {
    grid[fall_row][fall_col] = fall_who;
    falling = false;

    // pitch rises as the stack gets taller
    note(52 + (ROWS - fall_row) * 2, 5, 4);
    shake(2);

    if (find_four(grid, fall_who, winr, winc)) {
        state = 1;
        winner = fall_who;
        flash_t = 0;
        if (winner == HUMAN) { wins++;  save(0, wins);  strum(60, CHORD_MAJ, INSTR_TRI, 5, 60); }
        else                 { losses++; save(1, losses); hit(40, INSTR_SAW, 4, 400); }
        return;
    }
    if (board_full(grid)) {
        state = 2;
        draws++; save(2, draws);
        note(48, INSTR_TRI, 3);
        return;
    }
    turn = (fall_who == HUMAN) ? AI : HUMAN;
    if (turn == AI) ai_think = 0.35f;
}

void update(void) {
    float d = dt();

    if (state != 0) {
        flash_t++;
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_SPACE) || keyp('Z') || btnp(0, BTN_A))
            reset_game();
        return;
    }

    // animate a falling disc (accelerating)
    if (falling) {
        float dist = fall_target - fall_y;
        float speed = 120.0f + dist * 6.0f;     // gravity-ish: faster the longer it falls
        fall_y += speed * d;
        if (fall_y >= fall_target) { fall_y = fall_target; resolve_landing(); }
        return;
    }

    if (turn == HUMAN) {
        // keyboard cursor
        if (keyp('A') || keyp(KEY_LEFT)  || btnp(0, BTN_LEFT))  cursor = (cursor + COLS - 1) % COLS;
        if (keyp('D') || keyp(KEY_RIGHT) || btnp(0, BTN_RIGHT)) cursor = (cursor + 1) % COLS;

        // mouse hover sets the column
        int mx = mouse_x();
        if (mx >= BX && mx < BX + COLS * CELL) {
            int hc = (mx - BX) / CELL;
            if (hc >= 0 && hc < COLS) cursor = hc;
        }

        bool drop = keyp(KEY_SPACE) || keyp('Z') || btnp(0, BTN_A);
        if (mouse_pressed(MOUSE_LEFT) && mx >= BX && mx < BX + COLS * CELL) drop = true;

        if (drop && drop_row(grid, cursor) >= 0) begin_drop(cursor, HUMAN);
    } else {
        // AI turn
        ai_think -= d;
        if (ai_think <= 0) {
            int c = ai_pick();
            begin_drop(c, AI);
        }
    }
}

// ---------------------------------------------------------------- draw

static int disc_color(int who) { return who == HUMAN ? CLR_RED : CLR_YELLOW; }

static bool is_winning_cell(int r, int c) {
    if (state != 1) return false;
    for (int k = 0; k < 4; k++) if (winr[k] == r && winc[k] == c) return true;
    return false;
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    // drop-column highlight + preview on the human's turn
    if (state == 0 && turn == HUMAN && !falling) {
        rectfill(BX + cursor * CELL, BY, CELL, ROWS * CELL, CLR_DARK_BLUE);
        circfill(cell_cx(cursor), BY - CELL / 2, RAD, CLR_RED);   // hovering disc
    }

    // falling disc (drawn behind the board face so holes "swallow" it)
    if (falling)
        circfill(cell_cx(fall_col), (int)fall_y, RAD, disc_color(fall_who));

    // board face: a blue slab with holes punched out
    rectfill(BX, BY, COLS * CELL, ROWS * CELL, CLR_TRUE_BLUE);
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            int x = cell_cx(c), y = cell_cy(r);
            int v = grid[r][c];
            if (v == EMPTY) {
                circfill(x, y, RAD, CLR_DARKER_BLUE);   // empty hole = background
            } else {
                int col = disc_color(v);
                if (is_winning_cell(r, c) && (flash_t / 6) % 2) col = CLR_WHITE;
                circfill(x, y, RAD, col);
                circ(x, y, RAD, CLR_BLACK);
            }
        }
    rect(BX, BY, COLS * CELL, ROWS * CELL, CLR_DARK_BLUE);

    // title + tally
    print_centered("CONNECT FOUR", SCREEN_W/2, 6, CLR_YELLOW);
    print(str("W %d", wins),    6, SCREEN_H - 10, CLR_RED);
    print_centered(str("D %d", draws), SCREEN_W/2, SCREEN_H - 10, CLR_LIGHT_GREY);
    print_right(str("L %d", losses), SCREEN_H - 6, SCREEN_H - 10, CLR_YELLOW);

    // turn indicator
    if (state == 0 && !falling) {
        if (turn == HUMAN) print_centered("YOUR TURN", SCREEN_W/2, BY + ROWS * CELL + 4, CLR_RED);
        else               print_centered("AI THINKING", SCREEN_W/2, BY + ROWS * CELL + 4, CLR_YELLOW);
    }

    // result overlay
    if (state != 0) {
        fade(0.4f);
        int cy = SCREEN_H / 2 - 18;
        rectfill(BX - 6, cy, COLS * CELL + 12, 44, CLR_BLACK);
        rect    (BX - 6, cy, COLS * CELL + 12, 44, CLR_WHITE);
        if (state == 2)
            print_centered("DRAW", SCREEN_W/2, cy + 8, CLR_LIGHT_GREY);
        else if (winner == HUMAN)
            print_centered("YOU WIN!", SCREEN_W/2, cy + 8, CLR_RED);
        else
            print_centered("AI WINS", SCREEN_W/2, cy + 8, CLR_YELLOW);
        print_centered("click / Z to play again", SCREEN_W/2, cy + 26, CLR_LIGHT_GREY);
    }
}
