/* de:meta
{
  "slug": "sokoban",
  "title": "sokoban",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "grid-movement",
    "turn-based-loop"
  ],
  "lineage": "Faithful Sokoban clone with a full undo-stack (Step history array, pull-back on A); the undo mechanism is the implementation detail worth studying beyond the standard puzzle loop.",
  "genre": "puzzle",
  "homage": "Sokoban (1981)",
  "description": "The warehouse puzzle classic. Push every crate onto a red goal dot. You can only PUSH (never pull) and only one crate at a time, so a crate shoved into a corner is stuck for good — plan ahead. Arrow keys walk and push, A undoes your last move, B restarts the level. Three built-in levels; solve one to advance. Levels are stored as plain text, so adding your own is easy.",
  "todo": [
    "Some text-overflow issues.",
    "Add more levels — look up original Sokoban levels or design interesting puzzles."
  ]
}
de:meta */
#include "studio.h"

// ── SOKOBAN — push the crates ─────────────────────────────────
// Push every crate ($) onto a goal (the red dots). You can only
// PUSH, never pull, and only one crate at a time — so think ahead.
//
//   • ARROW KEYS  — walk / push
//   • A           — undo a move
//   • B           — restart the level
//   • A (on win)  — next level
//
// Built-in levels stored as plain text — easy to add your own.

#define MAXW 24
#define MAXH 16

// '#'=wall  ' '=floor  '.'=goal  '$'=crate  '@'=you  '*'=crate-on-goal  '+'=you-on-goal
static const char *levels[] = {
    "#######\n"
    "#     #\n"
    "# @$. #\n"
    "#     #\n"
    "#######",

    "#########\n"
    "#       #\n"
    "# @$  . #\n"
    "#  $  . #\n"
    "#       #\n"
    "#########",

    "#######\n"
    "#  .  #\n"
    "#  $  #\n"
    "#.$ @ #\n"
    "#######",
};
#define NLEVELS ((int)(sizeof(levels) / sizeof(levels[0])))

static char wall[MAXH][MAXW], goal[MAXH][MAXW], box[MAXH][MAXW];
static int  W, H, ppx, ppy;
static int  level = 0, moves = 0, pushes = 0;
static bool won = false;
static bool ready = false;

// undo history
typedef struct { int px, py, bx, by; bool pushed; } Step;
static Step hist[2048];
static int  histn = 0;

static void load_level(int n) {
    const char *s = levels[n];
    W = H = 0;
    for (int r = 0; r < MAXH; r++)
        for (int c = 0; c < MAXW; c++) { wall[r][c] = goal[r][c] = box[r][c] = 0; }
    int r = 0, c = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '\n') { if (c > W) W = c; c = 0; r++; continue; }
        char ch = *p;
        if (ch == '#') wall[r][c] = 1;
        if (ch == '.' || ch == '*' || ch == '+') goal[r][c] = 1;
        if (ch == '$' || ch == '*') box[r][c] = 1;
        if (ch == '@' || ch == '+') { ppx = c; ppy = r; }
        c++;
    }
    if (c > W) W = c;
    H = r + 1;
    moves = pushes = 0; histn = 0; won = false;
}

static void reset_game(void) { level = 0; load_level(0); ready = true; }

static bool check_win(void) {
    for (int r = 0; r < H; r++)
        for (int c = 0; c < W; c++)
            if (box[r][c] && !goal[r][c]) return false;
    return true;
}

static void do_move(int dx, int dy) {
    int nx = ppx + dx, ny = ppy + dy;
    if (nx < 0 || ny < 0 || nx >= W || ny >= H || wall[ny][nx]) return;

    Step st = { ppx, ppy, 0, 0, false };
    if (box[ny][nx]) {                          // a crate is in the way
        int bx = nx + dx, by = ny + dy;
        if (bx < 0 || by < 0 || bx >= W || by >= H || wall[by][bx] || box[by][bx]) return;
        box[ny][nx] = 0; box[by][bx] = 1;
        st.pushed = true; st.bx = nx; st.by = ny;   // record crate's FROM cell
        pushes++;
        note(60, INSTR_SQUARE, 2);
    }
    ppx = nx; ppy = ny; moves++;
    if (histn < 2048) hist[histn++] = st;

    if (check_win()) { won = true; strum(60, CHORD_MAJ, INSTR_TRI, 4, 45); }
}

static void undo(void) {
    if (histn == 0) return;
    Step st = hist[--histn];
    if (st.pushed) {                            // pull the crate back
        box[ppy][ppx] = 0;                      // it was at the player's current cell
        box[st.by][st.bx] = 1;
        pushes--;
    }
    ppx = st.px; ppy = st.py; moves--;
    won = false;
}

void update(void) {
    if (!ready) reset_game();

    if (won) {
        if (btnp(0, BTN_A)) { level = (level + 1) % NLEVELS; load_level(level); }
        if (btnp(0, BTN_B)) load_level(level);
        return;
    }
    if      (btnp(0, BTN_LEFT))  do_move(-1, 0);
    else if (btnp(0, BTN_RIGHT)) do_move( 1, 0);
    else if (btnp(0, BTN_UP))    do_move( 0, -1);
    else if (btnp(0, BTN_DOWN))  do_move( 0,  1);
    if (btnp(0, BTN_A)) undo();
    if (btnp(0, BTN_B)) load_level(level);
}

// ── drawing ───────────────────────────────────────────────────
static int cell_size(void) {
    int cw = (SCREEN_W - 8) / W, ch = (SCREEN_H - 36) / H;
    int s = min(cw, ch);
    return min(s, 20);
}

static void draw_crate(int x, int y, int s, bool on) {
    int body = on ? CLR_MEDIUM_GREEN : CLR_BROWN;
    int edge = on ? CLR_DARK_GREEN   : CLR_DARK_BROWN;
    rectfill(x + 1, y + 1, s - 2, s - 2, body);
    rect(x + 1, y + 1, s - 2, s - 2, edge);
    line(x + 1, y + 1, x + s - 2, y + s - 2, edge);     // the classic crate cross
    line(x + s - 2, y + 1, x + 1, y + s - 2, edge);
}

static void draw_pawn(int x, int y, int s) {
    int cx = x + s / 2, cy = y + s / 2;
    circfill(cx, cy + s / 4, s / 3, CLR_DARKER_GREY);   // shadow
    rectfill(cx - s / 4, cy - s / 6, s / 2, s / 3, CLR_BLUE);   // body
    circfill(cx, cy - s / 4, s / 4, CLR_LIGHT_PEACH);   // head
}

void draw(void) {
    if (!ready) reset_game();
    cls(CLR_DARKER_BLUE);

    int s  = cell_size();
    int ox = (SCREEN_W - W * s) / 2;
    int oy = 16 + (SCREEN_H - 36 - H * s) / 2;

    for (int r = 0; r < H; r++)
        for (int c = 0; c < W; c++) {
            int x = ox + c * s, y = oy + r * s;
            if (wall[r][c]) {
                rectfill(x, y, s, s, CLR_DARK_GREY);
                rect(x, y, s, s, CLR_LIGHT_GREY);
                continue;
            }
            // floor
            rectfill(x, y, s, s, ((r + c) & 1) ? CLR_DARK_BLUE : CLR_DARKER_PURPLE);
            if (goal[r][c]) {                              // goal marker
                circfill(x + s / 2, y + s / 2, max(2, s / 6), CLR_DARK_RED);
                circ(x + s / 2, y + s / 2, max(2, s / 6), CLR_RED);
            }
            if (box[r][c]) draw_crate(x, y, s, goal[r][c]);
        }
    draw_pawn(ox + ppx * s, oy + ppy * s, s);

    // HUD
    rectfill(0, 0, SCREEN_W, 12, CLR_BLACK);
    print("SOKOBAN", 4, 2, CLR_YELLOW);
    print(str("LEVEL %d/%d", level + 1, NLEVELS), 78, 2, CLR_WHITE);
    print_right(str("MOVES %d  PUSHES %d", moves, pushes), SCREEN_W - 4, 2, CLR_LIGHT_GREY);

    rectfill(0, SCREEN_H - 11, SCREEN_W, 11, CLR_BLACK);
    print_centered("arrows: push   A: undo   B: restart", SCREEN_W/2, SCREEN_H - 9, CLR_DARK_GREY);

    if (won) {
        const char *t = (level + 1 < NLEVELS) ? "LEVEL COMPLETE!  press A for next"
                                              : "ALL LEVELS SOLVED!  press A to replay";
        int w = 250, bx = (SCREEN_W - w) / 2;
        rectfill(bx, 88, w, 26, CLR_DARK_GREEN);
        rect(bx, 88, w, 26, CLR_WHITE);
        print_centered(t, SCREEN_W/2, 98, CLR_WHITE);
    }
}
