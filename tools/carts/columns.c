/* de:meta
{
  "title": "Columns",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine",
    "grid-movement"
  ],
  "lineage": "Homage to Sega Columns (1990); novel: cascade chain resolved as a discrete flash-then-collapse state machine, with diagonal match scanning and a climbing-arpeggio score sonification.",
  "genre": "puzzle",
  "homage": "Columns (1990)",
  "description": "The SEGA arcade gem-dropper, distilled. A vertical column of three jewels falls into a narrow well — you can't reshape it, only slide it left and right and cycle its three colors mid-fall. Land 3 or more of a color in a line (horizontal, vertical, OR diagonal) and they flash, clear, and the stack collapses — fresh matches cascade into escalating chains that ring out as a climbing arpeggio while the screen kicks. Speed ramps as you clear, best score is saved, and stacking to the top ends the run. Left/Right slide, Up or Z cycle the colors, Down soft-drops, Z/X restart."
}
de:meta */
#include "studio.h"
#include <string.h>

// COLUMNS
// A vertical column of 3 gems falls into the well. You can't change its shape —
// only slide it left/right and CYCLE the three colors. Land 3+ of the same color
// in a line (horizontal / vertical / diagonal) and they clear; the stack collapses
// and fresh matches CHAIN for bigger score. Speed ramps. Top-out loses.
//
// Left/Right: slide    Up or Z: cycle colors    Down: soft drop    Z/X: restart

#define GW   6           // grid width  (cells)
#define GH   13          // grid height
#define CELL 14          // cell size in pixels
#define GX   ((SCREEN_W - GW * CELL) / 2)   // well pixel origin
#define GY   18

#define NCOL 6           // number of gem colors in play
static const int GEM[NCOL] = {
    CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_PINK
};

static int grid[GH][GW];     // 0 empty, else color index + 1
static int gems[3];          // the falling column's 3 colors (top→bottom), 0-based
static int px, py;           // column position: col, top-row
static float fall_acc;       // gravity accumulator (seconds)

static int score, hiscore, cleared_total, level, state;  // state: 0 play, 1 over
static int flash_t;          // frames remaining of the clear flash
static int chain_show;       // last chain count, for the HUD pop
static float chain_pop;      // seconds remaining on the chain banner

// cells currently flashing before they clear
static bool marked[GH][GW];
static bool resolving;       // mid-cascade: don't accept input, animate the flash

// ====================================================================

static float fall_delay(void) {
    float d = 0.65f - (level - 1) * 0.05f;
    return d < 0.12f ? 0.12f : d;
}

static bool occupied(int c, int r) {
    if (c < 0 || c >= GW || r >= GH) return true;   // walls / floor
    if (r < 0) return false;                         // above the well is open
    return grid[r][c] != 0;
}

// can the whole 3-tall column sit with its top at (c, topr)?
static bool col_fits(int c, int topr) {
    for (int i = 0; i < 3; i++)
        if (occupied(c, topr + i)) return false;
    return true;
}

static void spawn_column(void) {
    px = GW / 2;
    py = -3;                 // start just above the well
    for (int i = 0; i < 3; i++) gems[i] = rnd(NCOL);
    fall_acc = 0;
}

static void reset_game(void) {
    memset(grid, 0, sizeof(grid));
    memset(marked, 0, sizeof(marked));
    score = 0; cleared_total = 0; level = 1; state = 0;
    flash_t = 0; resolving = false; chain_pop = 0; chain_show = 0;
    spawn_column();
}

void init(void) {
    hiscore = load(0);
    reset_game();
}

// drop the three gems into the grid where the column rests
static void lock_column(void) {
    for (int i = 0; i < 3; i++) {
        int r = py + i;
        if (r >= 0 && r < GH) grid[r][px] = gems[i] + 1;
        else if (r < 0) { state = 1;            // settled above the top → top-out
                          if (score > hiscore) { hiscore = score; save(0, hiscore); }
                          note(36, INSTR_SAW, 4); return; }
    }
    note(48, INSTR_SQUARE, 2);
}

// mark every gem that belongs to a run of 3+ in any of the 4 line directions.
// returns how many cells got marked.
static int mark_matches(void) {
    memset(marked, 0, sizeof(marked));
    static const int DIRS[4][2] = { {1,0}, {0,1}, {1,1}, {1,-1} };
    int n = 0;
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) {
            int v = grid[r][c];
            if (!v) continue;
            for (int d = 0; d < 4; d++) {
                int dc = DIRS[d][0], dr = DIRS[d][1];
                // only start a run if the previous cell in this dir differs
                int pc = c - dc, pr = r - dr;
                if (pc >= 0 && pc < GW && pr >= 0 && pr < GH && grid[pr][pc] == v) continue;
                // count the run
                int len = 0, cc = c, rr = r;
                while (cc >= 0 && cc < GW && rr >= 0 && rr < GH && grid[rr][cc] == v) {
                    len++; cc += dc; rr += dr;
                }
                if (len >= 3) {
                    cc = c; rr = r;
                    for (int k = 0; k < len; k++) { marked[rr][cc] = true; cc += dc; rr += dr; }
                }
            }
        }
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) if (marked[r][c]) n++;
    return n;
}

// remove marked cells and collapse each column down
static void clear_and_collapse(void) {
    for (int c = 0; c < GW; c++) {
        int write = GH - 1;
        for (int r = GH - 1; r >= 0; r--) {
            if (grid[r][c] && !marked[r][c]) grid[write--][c] = grid[r][c];
        }
        while (write >= 0) grid[write--][c] = 0;
    }
    memset(marked, 0, sizeof(marked));
}

// chain step: count matches, score them, flash. the flash timer drives the rest.
static int chain;
static void begin_resolve(void) {
    chain = 0;
    int n = mark_matches();
    if (n == 0) { spawn_column(); return; }
    resolving = true;
    chain = 1;
    flash_t = 12;
    score += n * 10 * chain;
    cleared_total += n;
    level = 1 + cleared_total / 18;
    note(degree(SCALE_PENTA, 4, chain), INSTR_SINE, 4);
    shake(2.0f);
    chain_show = chain; chain_pop = 0.9f;
}

// called when a flash finishes — clear, collapse, look for the next cascade link
static void advance_resolve(void) {
    clear_and_collapse();
    int n = mark_matches();
    if (n == 0) { resolving = false; spawn_column(); return; }
    chain++;
    flash_t = 12;
    score += n * 10 * chain;
    cleared_total += n;
    level = 1 + cleared_total / 18;
    note(degree(SCALE_PENTA, 4, chain), INSTR_SINE, 5);
    hit(degree(SCALE_PENTA, 5, chain), INSTR_TRI, 4, 90);
    shake(2.0f + chain);
    chain_show = chain; chain_pop = 1.0f;
}

void update(void) {
    if (chain_pop > 0) chain_pop -= dt();

    if (state == 1) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B) || keyp(KEY_SPACE)) reset_game();
        return;
    }

    // animating a clear flash: count it down, then take the next cascade step
    if (resolving) {
        if (flash_t > 0) { flash_t--; if (flash_t == 0) advance_resolve(); }
        return;
    }

    // ---- player input on the live column ----
    if (btnp(0, BTN_LEFT)  && col_fits(px - 1, py)) { px--; note(60, INSTR_SQUARE, 1); }
    if (btnp(0, BTN_RIGHT) && col_fits(px + 1, py)) { px++; note(60, INSTR_SQUARE, 1); }

    if (btnp(0, BTN_UP) || btnp(0, BTN_A)) {          // cycle colors top→bottom
        int t = gems[2];
        gems[2] = gems[1]; gems[1] = gems[0]; gems[0] = t;
        note(72, INSTR_TRI, 2);
    }

    // gravity (soft drop on Down)
    bool soft = btn(0, BTN_DOWN);
    fall_acc += dt();
    float delay = soft ? 0.045f : fall_delay();
    if (fall_acc >= delay) {
        fall_acc -= delay;
        if (col_fits(px, py + 1)) { py++; if (soft) score += 1; }
        else { lock_column(); if (state == 0) begin_resolve(); }
    }
}

// ====================================================================
// drawing
// ====================================================================

static void gem(int sx, int sy, int color, bool bright) {
    rectfill(sx + 1, sy + 1, CELL - 2, CELL - 2, bright ? CLR_WHITE : color);
    if (!bright) {
        pset(sx + 2, sy + 2, CLR_WHITE);                       // sparkle
        line(sx + 2, sy + CELL - 3, sx + CELL - 3, sy + 2, CLR_BLACK);   // facet
    }
    rect(sx + 1, sy + 1, CELL - 2, CELL - 2, CLR_BLACK);
}

void draw(void) {
    cls(CLR_DARK_BLUE);

    int ww = GW * CELL, wh = GH * CELL;

    // well
    rectfill(GX - 3, GY - 3, ww + 6, wh + 6, CLR_DARKER_BLUE);
    rect    (GX - 3, GY - 3, ww + 6, wh + 6, CLR_LIGHT_GREY);
    // faint grid lines
    for (int c = 1; c < GW; c++) line(GX + c * CELL, GY, GX + c * CELL, GY + wh - 1, CLR_DARKER_PURPLE);

    // settled gems (marked ones flash white in time with flash_t)
    bool blink_on = (flash_t / 3) % 2;
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++)
            if (grid[r][c])
                gem(GX + c * CELL, GY + r * CELL, GEM[grid[r][c] - 1],
                    marked[r][c] && blink_on);

    // the live falling column
    if (state == 0 && !resolving)
        for (int i = 0; i < 3; i++) {
            int r = py + i;
            if (r >= 0) gem(GX + px * CELL, GY + r * CELL, GEM[gems[i]], false);
        }

    // ---- HUD ----
    print("COLUMNS", 8, 6, CLR_YELLOW);

    int panel = GX + ww + 12;
    print("SCORE", panel, 24, CLR_LIGHT_GREY);
    print(str("%d", score), panel, 34, CLR_WHITE);
    print("LEVEL", panel, 54, CLR_LIGHT_GREY);
    print(str("%d", level), panel, 64, CLR_GREEN);
    print("GEMS",  panel, 84, CLR_LIGHT_GREY);
    print(str("%d", cleared_total), panel, 94, CLR_BLUE);
    print("BEST",  panel, 120, CLR_LIGHT_GREY);
    print(str("%d", hiscore), panel, 130, CLR_ORANGE);

    // chain banner
    if (chain_pop > 0 && chain_show >= 2) {
        int yy = GY + wh / 3;
        print_centered(str("CHAIN x%d", chain_show), SCREEN_W/2, yy, blink(4) ? CLR_YELLOW : CLR_PEACH);
    }

    // controls hint along the bottom-left
    print("L/R move", 8, SCREEN_H - 30, CLR_DARK_GREY);
    print("UP cycle", 8, SCREEN_H - 20, CLR_DARK_GREY);
    print("DN drop",  8, SCREEN_H - 10, CLR_DARK_GREY);

    if (state == 1) {
        int bw = ww + 30, bx = GX - 15, by = SCREEN_H / 2 - 26;
        rectfill(bx, by, bw, 52, CLR_BLACK);
        rect    (bx, by, bw, 52, CLR_RED);
        print_centered("GAME OVER", SCREEN_W/2, by + 8, CLR_RED);
        print_centered(str("%d", score), SCREEN_W/2, by + 22, CLR_YELLOW);
        print_centered("Z to restart", SCREEN_W/2, by + 36, CLR_LIGHT_GREY);
    }
}
