#include "studio.h"
#include <stddef.h>
#include <string.h>

// ── MINESWEEPER — the desktop classic ─────────────────────────
//   • LEFT-CLICK  — reveal a cell (a 0 flood-fills its region)
//   • RIGHT-CLICK — place / remove a flag
//   • click the FACE (or press R) — new game
//   First click is always safe.

#define GW 16          // grid width  (cells)
#define GH 14          // grid height (cells)
#define MINES 30
#define CELLS (GW * GH)
#define CS 16          // cell size in pixels
#define TOP 24         // HUD height
#define ORIGIN_Y TOP

static int  ox;                  // board left edge (centered)

static bool mine[CELLS];
static bool shown[CELLS];
static bool flag[CELLS];
static int  count[CELLS];        // adjacent mine count

static bool placed;              // mines placed yet? (after first click)
static bool dead;                // hit a mine
static bool won;
static float clock_t;            // frozen elapsed time
static int  best;

#define ST_IDLE  0
#define ST_PLAY  1
#define ST_OVER  2
static int  state;

static int idx(int cx, int cy) { return cy * GW + cx; }
static bool in_grid(int cx, int cy) { return cx >= 0 && cy >= 0 && cx < GW && cy < GH; }

static void new_game(void) {
    memset(mine,  0, sizeof(mine));
    memset(shown, 0, sizeof(shown));
    memset(flag,  0, sizeof(flag));
    memset(count, 0, sizeof(count));
    placed = false;
    dead = won = false;
    clock_t = 0;
    state = ST_IDLE;
    ox = (SCREEN_W - GW * CS) / 2;
    timer_reset();
}

void init(void) {
    best = load_int("best_time", 0);
    instrument(5, INSTR_NOISE, 2, 220, 0, 180);   // detonation boom
    new_game();
}

// place mines avoiding the safe cell + its neighbours, then count
static void place_mines(int safe) {
    int sx = safe % GW, sy = safe / GW;
    int placed_n = 0;
    while (placed_n < MINES) {
        int c = rnd(CELLS);
        if (mine[c]) continue;
        int cx = c % GW, cy = c / GW;
        if (abs(cx - sx) <= 1 && abs(cy - sy) <= 1) continue;  // keep first-click region clear
        mine[c] = true;
        placed_n++;
    }
    for (int cy = 0; cy < GH; cy++)
        for (int cx = 0; cx < GW; cx++) {
            if (mine[idx(cx, cy)]) continue;
            int n = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    if (!dx && !dy) continue;
                    if (in_grid(cx + dx, cy + dy) && mine[idx(cx + dx, cy + dy)]) n++;
                }
            count[idx(cx, cy)] = n;
        }
    placed = true;
}

static bool check_win(void) {
    for (int c = 0; c < CELLS; c++)
        if (!mine[c] && !shown[c]) return false;
    return true;
}

// iterative flood-fill (no recursion, no heap) — opens an empty region
static int stack[CELLS];
static void reveal_flood(int start) {
    int sp = 0;
    stack[sp++] = start;
    while (sp > 0) {
        int c = stack[--sp];
        if (shown[c] || flag[c] || mine[c]) continue;
        shown[c] = true;
        if (count[c] != 0) continue;                 // boundary number — stop here
        int cx = c % GW, cy = c / GW;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                if (!dx && !dy) continue;
                if (in_grid(cx + dx, cy + dy)) {
                    int n = idx(cx + dx, cy + dy);
                    if (!shown[n] && !mine[n]) stack[sp++] = n;
                }
            }
    }
}

static void reveal_cell(int c) {
    if (shown[c] || flag[c]) return;
    if (!placed) {
        place_mines(c);
        timer_reset();
        state = ST_PLAY;
    }
    if (mine[c]) {                                   // boom
        shown[c] = true;
        dead = true;
        state = ST_OVER;
        clock_t = timer();
        shake(6);
        note(36, 5, 6);
        hit(28, INSTR_TRI, 5, 320);
        return;
    }
    if (count[c] == 0) reveal_flood(c);
    else { shown[c] = true; note(72, INSTR_SQUARE, 2); }

    if (check_win()) {
        won = true;
        state = ST_OVER;
        clock_t = timer();
        for (int i = 0; i < CELLS; i++) if (mine[i]) flag[i] = true;
        strum(60, CHORD_MAJ7, INSTR_TRI, 4, 45);
        int t = (int)clock_t;
        if (best == 0 || t < best) { best = t; save_int("best_time", best); }
    }
}

static int flags_placed(void) {
    int n = 0;
    for (int c = 0; c < CELLS; c++) if (flag[c]) n++;
    return n;
}

// face button geometry (top HUD, centered)
static const int FACE_R = 9;
static int face_cx(void) { return SCREEN_W / 2; }
static int face_cy(void) { return TOP / 2; }

void update(void) {
    if (key('R') || keyp('R')) { new_game(); return; }

    int mx = mouse_x(), my = mouse_y();

    // face click → restart
    if (mouse_pressed(MOUSE_LEFT) &&
        near(mx, my, face_cx(), face_cy(), FACE_R + 2)) {
        new_game();
        return;
    }

    if (state == ST_OVER) return;

    // board hit-test
    int cx = (mx - ox) / CS;
    int cy = (my - ORIGIN_Y) / CS;
    if (mx < ox || my < ORIGIN_Y || !in_grid(cx, cy)) return;
    int c = idx(cx, cy);

    if (mouse_pressed(MOUSE_LEFT)) {
        if (!flag[c]) reveal_cell(c);
    } else if (mouse_pressed(MOUSE_RIGHT)) {
        if (!shown[c]) {
            flag[c] = !flag[c];
            note(flag[c] ? 84 : 60, INSTR_SQUARE, 2);
        }
    }
}

// canonical minesweeper number colors
static int num_color(int n) {
    switch (n) {
        case 1: return CLR_BLUE;
        case 2: return CLR_GREEN;
        case 3: return CLR_RED;
        case 4: return CLR_DARK_BLUE;
        case 5: return CLR_DARK_RED;
        case 6: return CLR_BLUE_GREEN;
        case 7: return CLR_DARK_PURPLE;
        default: return CLR_DARK_GREY;
    }
}

static void draw_covered(int x, int y) {
    rectfill(x, y, CS, CS, CLR_LIGHT_GREY);
    // raised bevel
    line(x, y, x + CS - 1, y, CLR_WHITE);
    line(x, y, x, y + CS - 1, CLR_WHITE);
    line(x + CS - 1, y + 1, x + CS - 1, y + CS - 1, CLR_DARK_GREY);
    line(x + 1, y + CS - 1, x + CS - 1, y + CS - 1, CLR_DARK_GREY);
}

static void draw_flag(int x, int y) {
    int px = x + 7, py = y + 4;
    line(px, py, px, y + 12, CLR_DARK_GREY);             // pole
    rectfill(px - 4, py, 4, 3, CLR_RED);                 // flag
    rectfill(x + 3, y + 12, 9, 2, CLR_BLACK);            // base
}

static void draw_mine(int x, int y, bool boom) {
    int cx = x + CS / 2, cy = y + CS / 2;
    rectfill(x, y, CS, CS, boom ? CLR_RED : CLR_LIGHT_GREY);
    line(x, y, x + CS - 1, y, CLR_MEDIUM_GREY);
    line(x, y, x, y + CS - 1, CLR_MEDIUM_GREY);
    circfill(cx, cy, 4, CLR_BLACK);
    line(cx - 6, cy, cx + 6, cy, CLR_BLACK);
    line(cx, cy - 6, cx, cy + 6, CLR_BLACK);
    line(cx - 4, cy - 4, cx + 4, cy + 4, CLR_BLACK);
    line(cx - 4, cy + 4, cx + 4, cy - 4, CLR_BLACK);
    pset(cx - 1, cy - 1, CLR_WHITE);
}

static void draw_face(void) {
    int cx = face_cx(), cy = face_cy();
    circfill(cx, cy, FACE_R, CLR_YELLOW);
    circ(cx, cy, FACE_R, CLR_BLACK);
    if (dead) {
        // X eyes
        line(cx - 5, cy - 4, cx - 2, cy - 1, CLR_BLACK);
        line(cx - 2, cy - 4, cx - 5, cy - 1, CLR_BLACK);
        line(cx + 2, cy - 4, cx + 5, cy - 1, CLR_BLACK);
        line(cx + 5, cy - 4, cx + 2, cy - 1, CLR_BLACK);
        arc(cx, cy + 6, 3, 180, 360, CLR_BLACK);          // frown
    } else if (won) {
        line(cx - 5, cy - 3, cx - 2, cy - 3, CLR_BLACK);  // cool/closed eyes
        line(cx + 2, cy - 3, cx + 5, cy - 3, CLR_BLACK);
        arc(cx, cy + 1, 3, 0, 180, CLR_BLACK);            // smile
    } else {
        pset(cx - 3, cy - 2, CLR_BLACK);
        pset(cx + 3, cy - 2, CLR_BLACK);
        arc(cx, cy + 1, 3, 0, 180, CLR_BLACK);            // smile
    }
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    // HUD bar
    rectfill(0, 0, SCREEN_W, TOP, CLR_DARK_GREY);
    rectfill(0, TOP - 1, SCREEN_W, 1, CLR_BLACK);

    int remaining = MINES - flags_placed();
    if (remaining < 0) remaining = 0;
    print(str("MINES %03d", remaining), 6, 4, CLR_RED);

    float t = (state == ST_OVER) ? clock_t : (placed ? timer() : 0);
    int secs = (int)t;
    if (secs > 999) secs = 999;
    print_right(str("%03d", secs), SCREEN_W - 6, 4, CLR_LIME_GREEN);
    print_right("TIME", SCREEN_W - 6, 14, CLR_LIGHT_GREY);
    print("BEST", 6, 14, CLR_LIGHT_GREY);
    if (best > 0) print(str("%d", best), 36, 14, CLR_LIGHT_YELLOW);

    draw_face();

    // board
    for (int cy = 0; cy < GH; cy++)
        for (int cx = 0; cx < GW; cx++) {
            int c = idx(cx, cy);
            int x = ox + cx * CS, y = ORIGIN_Y + cy * CS;

            if (dead && mine[c] && !flag[c]) {
                draw_mine(x, y, shown[c]);
                continue;
            }
            if (dead && flag[c] && !mine[c]) {            // wrong flag
                rectfill(x, y, CS, CS, CLR_LIGHT_GREY);
                draw_mine(x, y, false);
                line(x + 2, y + 2, x + CS - 3, y + CS - 3, CLR_RED);
                line(x + CS - 3, y + 2, x + 2, y + CS - 3, CLR_RED);
                continue;
            }

            if (shown[c]) {
                rectfill(x, y, CS, CS, CLR_MEDIUM_GREY);
                rect(x, y, CS, CS, CLR_DARK_GREY);
                if (count[c] > 0)
                    print(str("%d", count[c]), x + 5, y + 4, num_color(count[c]));
            } else {
                draw_covered(x, y);
                if (flag[c]) draw_flag(x, y);
            }
        }

    // banner
    if (state == ST_OVER) {
        const char *msg = won ? "CLEARED!  click face / R" : "BOOM  click face / R";
        int w = text_width(msg) + 16;
        int bx = (SCREEN_W - w) / 2, by = ORIGIN_Y + (GH * CS) / 2 - 7;
        rectfill(bx, by, w, 14, won ? CLR_DARK_GREEN : CLR_DARK_RED);
        rect(bx, by, w, 14, CLR_WHITE);
        print_centered(msg, by + 4, CLR_WHITE);
    }
}
