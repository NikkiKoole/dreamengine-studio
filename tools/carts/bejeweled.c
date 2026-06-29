/* de:meta
{
  "title": "Bejeweled",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine",
    "particle-system"
  ],
  "lineage": "Faithful Bejeweled (2001) clone; the state machine (IDLE/SWAP/BACK/CLEAR/FALL/OVER) is the primary teaching artifact — a clean worked example of animated-phase sequencing for a puzzle game.",
  "genre": "puzzle",
  "homage": "Bejeweled (2001)",
  "description": "An 8x8 well of faceted jewels begging to be matched. Click a gem, then a neighbor, to swap them — but the swap only sticks if it lines up three or more of a color; an illegal try wiggles back for free. Matches flash white and shrink into a spray of colored sparks, the gems above ease down into the gaps, fresh ones stream in from the top, and every new line the fall creates cascades for a climbing score and a chord that pitches up with the chain. Run the board dry of legal moves and it reshuffles itself; spend your 30 swaps for the high score, which is saved between runs. Mouse only: click a gem then an adjacent gem (or drag from one to a neighbor) to swap; click to play again on game over."
}
de:meta */
#include "studio.h"
#include <stddef.h>

// BEJEWELED
// 8x8 grid of jewels. Click a gem, then an adjacent gem, to swap them.
// A swap only sticks if it makes a line of 3+ same color. Matches pop for
// points, gems above fall, new ones drop in — and cascades chain for bonus.
// No legal move left? The board reshuffles. Spend your 30 moves for a high score.
//
// Mouse: click a gem then an adjacent gem (or drag) to swap. Click to restart.

#define N        8                 // grid is N x N
#define CELL     20                // px per cell
#define BX       96                // board pixel origin
#define BY       28
#define NCOL     7                 // number of gem colors
#define EMPTY    -1
#define START_MOVES 30

static const int GEMCOL[NCOL] = {
    CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN,
    CLR_BLUE, CLR_PINK, CLR_WHITE
};
// a paler shade per gem for the cut/highlight facet
static const int GEMLIT[NCOL] = {
    CLR_PEACH, CLR_LIGHT_PEACH, CLR_LIGHT_YELLOW, CLR_LIME_GREEN,
    CLR_BLUE, CLR_LIGHT_PEACH, CLR_LIGHT_GREY
};

static int  grid[N][N];            // gem color index, or EMPTY
static float yoff[N][N];           // per-cell vertical pixel offset for fall animation
static float pop[N][N];            // per-cell pop timer (1 -> 0) while clearing; 0 = not clearing
static bool clearing[N][N];        // marked for this clear pass

// game state machine
enum { ST_IDLE, ST_SWAP, ST_BACK, ST_CLEAR, ST_FALL, ST_OVER };
static int   state;
static float anim_t;               // 0..1 progress of the current animated transition

// active swap (two adjacent cells)
static int   sa_r, sa_c, sb_r, sb_c;

static int   sel_r = -1, sel_c = -1;   // currently selected cell (-1 none)
static int   drag_r = -1, drag_c = -1; // cell the press started on (for drag)

static int   score, hiscore, moves, cascade;
static float clock_t;              // elapsed seconds, for light pressure / display
static float pulse;                // selection pulse

// particle puffs
typedef struct { float x, y, vx, vy, life; int col; } Bit;
static Bit bits[120];

// ---------------------------------------------------------------- helpers

static void spawn_bits(float x, float y, int col) {
    for (int n = 0; n < 7; n++)
        for (int i = 0; i < 120; i++)
            if (bits[i].life <= 0) {
                bits[i] = (Bit){ x, y,
                    rnd_float_between(-1.8f, 1.8f),
                    rnd_float_between(-2.4f, 0.4f),
                    rnd_float_between(0.4f, 0.8f), col };
                break;
            }
}

static int cell_px(int c) { return BX + c * CELL; }
static int cell_py(int r) { return BY + r * CELL; }

// does a 3+ run exist anywhere? (used to validate a board / swap)
static bool any_match(void) {
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
            int g = grid[r][c];
            if (g == EMPTY) continue;
            if (c + 2 < N && grid[r][c+1] == g && grid[r][c+2] == g) return true;
            if (r + 2 < N && grid[r+1][c] == g && grid[r+2][c] == g) return true;
        }
    return false;
}

// mark all gems that are part of a 3+ run; return how many cells got marked
static int mark_matches(void) {
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) clearing[r][c] = false;

    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
            int g = grid[r][c];
            if (g == EMPTY) continue;
            // horizontal run
            if (c + 2 < N && grid[r][c+1] == g && grid[r][c+2] == g) {
                int cc = c;
                while (cc < N && grid[r][cc] == g) { clearing[r][cc] = true; cc++; }
            }
            // vertical run
            if (r + 2 < N && grid[r+1][c] == g && grid[r+2][c] == g) {
                int rr = r;
                while (rr < N && grid[rr][c] == g) { clearing[rr][c] = true; rr++; }
            }
        }
    int cnt = 0;
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) if (clearing[r][c]) cnt++;
    return cnt;
}

// is there any swap that would create a match?
static bool has_legal_move(void) {
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
            // swap right
            if (c + 1 < N) {
                int t = grid[r][c]; grid[r][c] = grid[r][c+1]; grid[r][c+1] = t;
                bool m = any_match();
                t = grid[r][c]; grid[r][c] = grid[r][c+1]; grid[r][c+1] = t;
                if (m) return true;
            }
            // swap down
            if (r + 1 < N) {
                int t = grid[r][c]; grid[r][c] = grid[r+1][c]; grid[r+1][c] = t;
                bool m = any_match();
                t = grid[r][c]; grid[r][c] = grid[r+1][c]; grid[r+1][c] = t;
                if (m) return true;
            }
        }
    return false;
}

// fill the whole board with random gems that has at least one legal move and
// (ideally) no pre-existing matches
static void fill_board(void) {
    do {
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++) {
                int g;
                do { g = rnd(NCOL); }
                while ((c >= 2 && grid[r][c-1] == g && grid[r][c-2] == g) ||
                       (r >= 2 && grid[r-1][c] == g && grid[r-2][c] == g));
                grid[r][c] = g;
                yoff[r][c] = 0; pop[r][c] = 0;
            }
    } while (!has_legal_move());
}

static void reset_game(void) {
    for (int i = 0; i < 120; i++) bits[i].life = 0;
    fill_board();
    state = ST_IDLE; anim_t = 0;
    sel_r = sel_c = drag_r = drag_c = -1;
    score = 0; moves = START_MOVES; cascade = 0; clock_t = 0;
}

void init(void) {
    hiscore = load(0);
    reset_game();
}

static bool adjacent(int r1, int c1, int r2, int c2) {
    int d = abs(r1 - r2) + abs(c1 - c2);
    return d == 1;
}

static void begin_swap(int r1, int c1, int r2, int c2) {
    sa_r = r1; sa_c = c1; sb_r = r2; sb_c = c2;
    state = ST_SWAP; anim_t = 0;
    note(64, INSTR_SINE, 2);
}

static void do_swap_cells(void) {
    int t = grid[sa_r][sa_c]; grid[sa_r][sa_c] = grid[sb_r][sb_c]; grid[sb_r][sb_c] = t;
}

// resolve the marked clear: score, spray particles, empty the cells
static void apply_clear(int n) {
    cascade++;
    int gained = n * 10 * cascade;
    score += gained;
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
            if (clearing[r][c]) {
                spawn_bits(cell_px(c) + CELL / 2.0f, cell_py(r) + CELL / 2.0f,
                           GEMCOL[grid[r][c]]);
                grid[r][c] = EMPTY;
            }
    // rising sound + shake scaling with chain depth
    int root = 60 + min(cascade, 8) * 2;
    chord(root, cascade >= 3 ? CHORD_MAJ7 : CHORD_MAJ, INSTR_TRI, 4);
    shake(2.0f + cascade * 1.5f);
}

// collapse columns: move gems down into empties, set yoff for the fall anim,
// and drop new gems in from above
static void collapse_and_refill(void) {
    for (int c = 0; c < N; c++) {
        // write target from the bottom up
        int write = N - 1;
        for (int r = N - 1; r >= 0; r--) {
            if (grid[r][c] != EMPTY) {
                if (write != r) {
                    grid[write][c] = grid[r][c];
                    yoff[write][c] = (float)((r - write) * CELL); // negative: it visually came from above
                    grid[r][c] = EMPTY;
                } else {
                    yoff[write][c] = 0;
                }
                write--;
            }
        }
        // fill the rest from the top with new gems, stacked above the well.
        // they sit in a column just above the top edge: the lowest empty row
        // starts one cell up, each higher row another cell higher, so the whole
        // stack streams down into place. 'rank' counts empties from the bottom.
        int rank = 1;
        for (int r = write; r >= 0; r--) {
            grid[r][c] = rnd(NCOL);
            yoff[r][c] = (float)(-(r + rank) * CELL); // negative = above the board
            rank++;
        }
    }
    state = ST_FALL; anim_t = 0;
}

// ---------------------------------------------------------------- update

void update(void) {
    float d = dt();
    clock_t += d;
    pulse += d * 6.0f;

    // particles
    for (int i = 0; i < 120; i++)
        if (bits[i].life > 0) {
            bits[i].x += bits[i].vx; bits[i].y += bits[i].vy;
            bits[i].vy += 0.18f; bits[i].life -= d;
        }

    if (state == ST_OVER) {
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_SPACE) || keyp(KEY_ENTER))
            reset_game();
        return;
    }

    // -------- animated transitions --------
    if (state == ST_SWAP || state == ST_BACK) {
        anim_t += d * 6.0f;
        if (anim_t >= 1.0f) {
            anim_t = 0;
            if (state == ST_SWAP) {
                do_swap_cells();
                if (mark_matches() > 0) {
                    cascade = 0;
                    if (moves > 0) moves--;
                    state = ST_CLEAR;
                } else {
                    // illegal — swap back
                    note(40, INSTR_SAW, 3);
                    state = ST_BACK;
                }
            } else { // ST_BACK finished
                do_swap_cells();   // undo
                state = ST_IDLE;
            }
        }
        return;
    }

    if (state == ST_CLEAR) {
        anim_t += d * 4.0f;
        // drive per-cell pop timers
        bool done = true;
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++)
                if (clearing[r][c]) {
                    pop[r][c] = clamp(1.0f - anim_t, 0, 1);
                    if (pop[r][c] > 0) done = false;
                }
        if (anim_t >= 1.0f || done) {
            int n = 0;
            for (int r = 0; r < N; r++)
                for (int c = 0; c < N; c++) if (clearing[r][c]) n++;
            apply_clear(n);
            for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) pop[r][c] = 0;
            collapse_and_refill();
        }
        return;
    }

    if (state == ST_FALL) {
        anim_t += d * 5.0f;
        bool settled = true;
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++) {
                if (yoff[r][c] != 0) {
                    yoff[r][c] = lerp(yoff[r][c], 0, ease_out(clamp(anim_t, 0, 1)) * 0.5f + 0.18f);
                    if (yoff[r][c] < -0.5f || yoff[r][c] > 0.5f) settled = false;
                    else yoff[r][c] = 0;
                }
            }
        if (settled) {
            if (mark_matches() > 0) {
                state = ST_CLEAR; anim_t = 0;   // cascade!
            } else {
                cascade = 0;
                // out of moves?
                if (moves <= 0) {
                    state = ST_OVER;
                    if (score > hiscore) { hiscore = score; save(0, hiscore); }
                    return;
                }
                // deadlock guard
                if (!has_legal_move()) {
                    note(72, INSTR_SINE, 3);
                    fill_board();
                    state = ST_FALL; anim_t = 0;  // tiny settle of the reshuffle
                    return;
                }
                state = ST_IDLE;
            }
        }
        return;
    }

    // -------- ST_IDLE: handle input --------
    int mx = mouse_x(), my = mouse_y();
    int hc = (mx - BX) / CELL, hr = (my - BY) / CELL;
    bool over = (mx >= BX && my >= BY && hc >= 0 && hc < N && hr >= 0 && hr < N);

    if (mouse_pressed(MOUSE_LEFT) && over) {
        if (sel_r < 0) {
            sel_r = hr; sel_c = hc;
            drag_r = hr; drag_c = hc;
            note(67, INSTR_SINE, 1);
        } else if (sel_r == hr && sel_c == hc) {
            sel_r = sel_c = -1;   // deselect
        } else if (adjacent(sel_r, sel_c, hr, hc)) {
            begin_swap(sel_r, sel_c, hr, hc);
            sel_r = sel_c = -1;
        } else {
            sel_r = hr; sel_c = hc;   // reselect elsewhere
            drag_r = hr; drag_c = hc;
            note(67, INSTR_SINE, 1);
        }
    }

    // drag-to-swap: released over an adjacent cell from where we pressed
    if (mouse_released(MOUSE_LEFT) && drag_r >= 0 && over) {
        if (adjacent(drag_r, drag_c, hr, hc)) {
            begin_swap(drag_r, drag_c, hr, hc);
            sel_r = sel_c = -1;
        }
        drag_r = drag_c = -1;
    }
}

// ---------------------------------------------------------------- draw

// a faceted gem at pixel (px,py) sized s, color index gem (0..NCOL-1).
// scale lets the pop animation shrink it; flash draws it white.
static void draw_gem(int px, int py, int gem, float scale, bool flash) {
    if (scale <= 0.02f) return;
    int cx = px + CELL / 2, cy = py + CELL / 2;
    int h = (int)((CELL / 2 - 2) * scale);
    if (h < 1) return;
    int body = flash ? CLR_WHITE : GEMCOL[gem];
    int lit  = flash ? CLR_WHITE : GEMLIT[gem];

    // diamond body (two triangles)
    int t = cy - h, b = cy + h, l = cx - h, r = cx + h;
    trifill(cx, t, r, cy, cx, b, body);
    trifill(cx, t, l, cy, cx, b, body);
    // top-left bright facet
    trifill(cx, t, l, cy, cx, cy, lit);
    // outline
    line(cx, t, r, cy, CLR_BLACK);
    line(r, cy, cx, b, CLR_BLACK);
    line(cx, b, l, cy, CLR_BLACK);
    line(l, cy, cx, t, CLR_BLACK);
    // tiny sparkle
    if (!flash) pset(cx - h / 3, cy - h / 3, CLR_WHITE);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    int boardpx = N * CELL;

    // well frame
    rectfill(BX - 4, BY - 4, boardpx + 8, boardpx + 8, CLR_DARK_BLUE);
    rect(BX - 4, BY - 4, boardpx + 8, boardpx + 8, CLR_INDIGO);

    // checker floor under the gems for depth
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
            int col = ((r + c) & 1) ? CLR_DARK_BLUE : CLR_DARKER_PURPLE;
            rectfill(cell_px(c), cell_py(r), CELL, CELL, col);
        }

    clip(BX - 4, BY - 4, boardpx + 8, boardpx + 8);

    // selection / drag highlight
    if (sel_r >= 0) {
        int sx = cell_px(sel_c), sy = cell_py(sel_r);
        int g = 4 + (int)(2 * sin_deg(pulse * 30));
        rect(sx + 1, sy + 1, CELL - 2, CELL - 2, CLR_YELLOW);
        rect(sx, sy, CELL, CELL, g > 5 ? CLR_LIGHT_YELLOW : CLR_YELLOW);
    }

    // gems
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
            int g = grid[r][c];
            if (g == EMPTY) continue;

            int px = cell_px(c);
            int py = cell_py(r) + (int)yoff[r][c];
            float scale = 1.0f;
            bool flash = false;

            // selected gem lifts + pulses
            if (r == sel_r && c == sel_c) {
                scale = 1.0f + 0.12f * sin_deg(pulse * 40);
                py -= 2;
            }

            // animate the two swapping cells gliding toward each other
            if ((state == ST_SWAP || state == ST_BACK) &&
                ((r == sa_r && c == sa_c) || (r == sb_r && c == sb_c))) {
                float t = ease_in_out(clamp(anim_t, 0, 1));
                int ox = (sb_c - sa_c) * CELL, oy = (sb_r - sa_r) * CELL;
                if (r == sa_r && c == sa_c) { px += (int)(ox * t); py += (int)(oy * t); }
                else                        { px -= (int)(ox * t); py -= (int)(oy * t); }
            }

            // clearing gems flash then shrink
            if (state == ST_CLEAR && clearing[r][c]) {
                scale = ease_out(clamp(pop[r][c], 0, 1));
                flash = ((frame() / 2) & 1);
            }

            draw_gem(px, py, g, scale, flash);
        }

    // particles
    for (int i = 0; i < 120; i++)
        if (bits[i].life > 0) {
            pset((int)bits[i].x, (int)bits[i].y, bits[i].col);
            if (bits[i].life > 0.4f)
                pset((int)bits[i].x + 1, (int)bits[i].y, CLR_WHITE);
        }

    clip(0, 0, 0, 0);

    // ---------------- HUD ----------------
    print("BEJEWELED", BX - 4, 8, CLR_PINK);

    int hx = BX + boardpx + 14;
    print("SCORE", hx, 30, CLR_LIGHT_GREY);
    print(str("%d", score), hx, 40, CLR_WHITE);
    print("MOVES", hx, 60, CLR_LIGHT_GREY);
    print(str("%d", moves), hx, 70, moves <= 5 ? CLR_RED : CLR_GREEN);
    print("BEST", hx, 90, CLR_LIGHT_GREY);
    print(str("%d", hiscore), hx, 100, CLR_YELLOW);
    print(str("%ds", (int)clock_t), hx, 124, CLR_INDIGO);

    if (cascade >= 2 && (state == ST_CLEAR || state == ST_FALL))
        print(str("x%d CHAIN!", cascade), hx, 140, CLR_LIGHT_YELLOW);

    // left-side hint
    print("CLICK A", 8, 40, CLR_DARK_GREY);
    print("GEM, THEN", 8, 50, CLR_DARK_GREY);
    print("A NEIGHBOR", 8, 60, CLR_DARK_GREY);
    print("TO SWAP", 8, 70, CLR_DARK_GREY);

    if (state == ST_OVER) {
        fade(0.5f);
        int bw = 160, bh = 64, bxp = SCREEN_W / 2 - bw / 2, byp = SCREEN_H / 2 - bh / 2;
        rectfill(bxp, byp, bw, bh, CLR_DARK_BLUE);
        rect(bxp, byp, bw, bh, CLR_PINK);
        print_centered("GAME OVER", SCREEN_W/2, byp + 8, CLR_PINK);
        print_centered(str("SCORE %d", score), SCREEN_W/2, byp + 24, CLR_WHITE);
        print_centered(str("BEST %d", hiscore), SCREEN_W/2, byp + 36, CLR_YELLOW);
        print_centered("click to play again", SCREEN_W/2, byp + 50, CLR_LIGHT_GREY);
    }
}
