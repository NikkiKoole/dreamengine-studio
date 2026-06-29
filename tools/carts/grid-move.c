/* de:meta
{
  "title": "grid move",
  "status": "active",
  "created": "2026-06-04",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "grid-movement"
  ],
  "lineage": "Pac-Man-style cell-tween mover; introduces the turn-buffer (the grid world's coyote time) as the key feel concept, completing a trilogy with platform-rects and platform-rails.",
  "genre": "maze",
  "description": "The third movement paradigm: position is a CELL, motion is a short tween between cells (the player is always logically at a cell, visually sliding to the next). The feel lesson is the TURN BUFFER — the Pac-Man cornering trick: press a turn a few frames before a junction and it's remembered, then fires the moment it's legal. It's the grid world's coyote time. Press X to toggle the buffer and feel every corner go sticky. Also shows bump-against-wall feedback and seamless cell-to-cell chaining. Eat all the dots. Arrows/WASD move, X toggles the buffer, Z restarts."
}
de:meta */
#include "studio.h"

// GRID MOVE — the third movement paradigm. Solid-space (platform-rects) has a
// free position; rails (platform-rails) bind you to an edge; here position is a
// CELL. Motion is a short tween between cells: the player is always logically
// at (cx,cy) and visually sliding toward the next cell with `prog` 0..1.
//
// The feel lesson is the TURN BUFFER — the Pac-Man cornering trick. Press a
// turn a few frames BEFORE you reach a junction and it's remembered, then fires
// the moment it becomes legal. It's the grid world's coyote time: invisible,
// but without it every corner feels sticky (you must press on the exact frame
// you sit on the cell). Press X to toggle the buffer and feel corners go stiff.
//
// Eat all the dots.   Move: arrows / WASD.   X: toggle buffer.   Z: restart.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

// ── TUNING — edit + hot-reload ───────────────────────────────────────────────
#define CS          16      // cell size (px)
#define MOVE_FRAMES 8.0f    // frames to cross one cell (speed)
#define BUF         10      // frames a turn press is remembered
#define GW 20
#define GH 11
#define OY 12               // maze offset below the HUD
#define SPAWN_X 10
#define SPAWN_Y  9

// the level: a string per row — '#' wall, '.' floor (gets a dot)
static const char *maze[GH] = {
    "####################",
    "#........##........#",
    "#.##.###.##.###.##.#",
    "#..................#",
    "#.##.#.######.#.##.#",
    "#....#...##...#....#",
    "#.##.###.##.###.##.#",
    "#..................#",
    "#.##.###.##.###.##.#",
    "#..................#",
    "####################",
};

static bool walkable(int cx, int cy) {
    if (cx < 0 || cx >= GW || cy < 0 || cy >= GH) return false;
    return maze[cy][cx] != '#';
}

STATE {
    int   cx, cy;          // the cell we're standing on (or leaving)
    int   mx, my;          // current step direction; 0,0 = idle
    float prog;            // 0..1 progress across the cell
    int   wantx, wanty;    // the buffered turn
    int   want_t;          // frames left before the buffer expires
    int   lastx, lasty;    // facing (for the eyes)
    int   bumpx, bumpy;    // bumped-into-wall feedback
    int   bump_t;
    bool  buffer_on;
    bool  dots[GH][GW];
    int   eaten, total;
    bool  win;
};

static void reset_game(void) {
    S->total = 0; S->eaten = 0; S->win = false;
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            bool d = maze[y][x] == '.' && !(x == SPAWN_X && y == SPAWN_Y);
            S->dots[y][x] = d;
            if (d) S->total++;
        }
    S->cx = SPAWN_X; S->cy = SPAWN_Y;
    S->mx = S->my = 0; S->prog = 0;
    S->want_t = 0; S->bump_t = 0;
    S->lastx = 1; S->lasty = 0;
    S->buffer_on = true;
}

void init(void) { reset_game(); }

static bool held_dir(int x, int y) {
    if (x < 0) return btn(0, BTN_LEFT);
    if (x > 0) return btn(0, BTN_RIGHT);
    if (y < 0) return btn(0, BTN_UP);
    return btn(0, BTN_DOWN);
}

static void eat_here(void) {
    if (!S->dots[S->cy][S->cx]) return;
    S->dots[S->cy][S->cx] = false;
    S->eaten++;
    note(70 + (S->eaten % 16), INSTR_TRI, 2);          // pitch climbs as you clear
    if (S->eaten == S->total) { S->win = true; strum(72, CHORD_MAJ, INSTR_TRI, 4, 40); }
}

// decide the next step while sitting on a cell. Priority order IS the feel:
// buffered turn > keep rolling the held way > any held direction > bump.
static void start_step(int prevx, int prevy) {
    if (S->want_t > 0 && walkable(S->cx + S->wantx, S->cy + S->wanty)) {
        S->mx = S->wantx; S->my = S->wanty;            // the cornering assist fires
        S->lastx = S->mx; S->lasty = S->my;
        S->want_t = 0;
        return;
    }
    if ((prevx || prevy) && held_dir(prevx, prevy) && walkable(S->cx + prevx, S->cy + prevy)) {
        S->mx = prevx; S->my = prevy;                  // chain seamlessly, no pause
        return;
    }
    if (btn(0, BTN_LEFT)  && walkable(S->cx - 1, S->cy)) { S->mx = -1; S->my = 0; S->lastx = -1; S->lasty = 0; return; }
    if (btn(0, BTN_RIGHT) && walkable(S->cx + 1, S->cy)) { S->mx =  1; S->my = 0; S->lastx =  1; S->lasty = 0; return; }
    if (btn(0, BTN_UP)    && walkable(S->cx, S->cy - 1)) { S->mx = 0; S->my = -1; S->lastx = 0; S->lasty = -1; return; }
    if (btn(0, BTN_DOWN)  && walkable(S->cx, S->cy + 1)) { S->mx = 0; S->my =  1; S->lastx = 0; S->lasty =  1; return; }
    // held into a wall → a little bump so the input never feels ignored
    int hx = (btn(0, BTN_RIGHT) ? 1 : 0) - (btn(0, BTN_LEFT) ? 1 : 0);
    int hy = (btn(0, BTN_DOWN) ? 1 : 0) - (btn(0, BTN_UP) ? 1 : 0);
    if ((hx || hy) && S->bump_t == 0) {
        S->bumpx = hx; S->bumpy = hy; S->bump_t = 10;
        if (hx) { S->lastx = hx; S->lasty = 0; } else { S->lastx = 0; S->lasty = hy; }
        note(38, INSTR_NOISE, 1);
    }
}

void update(void) {
#ifdef DE_TRACE
    watch("g", "cell=%d,%d prog=%.2f mv=%d,%d want=%d,%d wt=%d eat=%d",
          S->cx, S->cy, S->prog, S->mx, S->my, S->wantx, S->wanty, S->want_t, S->eaten);
#endif
    if (S->win) { if (btnp(0, BTN_A)) reset_game(); return; }

    if (btnp(0, BTN_B)) S->buffer_on = !S->buffer_on;

    // the newest direction press goes into the turn buffer
    if (btnp(0, BTN_LEFT))  { S->wantx = -1; S->wanty = 0; S->want_t = S->buffer_on ? BUF : 1; }
    if (btnp(0, BTN_RIGHT)) { S->wantx =  1; S->wanty = 0; S->want_t = S->buffer_on ? BUF : 1; }
    if (btnp(0, BTN_UP))    { S->wantx = 0; S->wanty = -1; S->want_t = S->buffer_on ? BUF : 1; }
    if (btnp(0, BTN_DOWN))  { S->wantx = 0; S->wanty =  1; S->want_t = S->buffer_on ? BUF : 1; }
    if (S->want_t > 0) S->want_t--;
    if (S->bump_t > 0) S->bump_t--;

    if (S->mx || S->my) {
        S->prog += 1.0f / MOVE_FRAMES;
        if (S->prog >= 1.0f) {                         // arrived on the next cell
            S->cx += S->mx; S->cy += S->my;
            int px_ = S->mx, py_ = S->my;
            S->mx = S->my = 0; S->prog = 0;
            eat_here();
            start_step(px_, py_);                      // junction decision happens HERE
        }
    } else {
        start_step(S->lastx, S->lasty);
    }
}

void draw(void) {
    cls(CLR_BLACK);

    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            int sx = x * CS, sy = OY + y * CS;
            if (maze[y][x] == '#') {
                rectfill(sx, sy, CS, CS, CLR_DARKER_BLUE);
                rect(sx, sy, CS, CS, CLR_TRUE_BLUE);
            } else if (S->dots[y][x]) {
                circfill(sx + CS / 2, sy + CS / 2, 2, CLR_LIGHT_PEACH);
            }
        }

    // player: cell position + tween, leaning into the wall when bumping
    float fx = (S->cx + S->mx * S->prog) * CS + CS / 2;
    float fy = (S->cy + S->my * S->prog) * CS + CS / 2 + OY;
    int bx = 0, by = 0;
    if (S->bump_t > 0) {
        float k = (S->bump_t > 5 ? 10 - S->bump_t : S->bump_t) / 5.0f;   // out then back
        bx = (int)(S->bumpx * 3 * k); by = (int)(S->bumpy * 3 * k);
    }
    circfill((int)fx + bx, (int)fy + by, 6, CLR_YELLOW);
    circfill((int)fx + bx + S->lastx * 3, (int)fy + by + S->lasty * 3, 1, CLR_BLACK);

    rectfill(0, 0, SCREEN_W, 10, CLR_DARKER_BLUE);
    print("grid move", 4, 2, CLR_WHITE);
    print(str("dots %d/%d", S->eaten, S->total), 110, 2, CLR_YELLOW);
    print(S->buffer_on ? "buffer ON (X)" : "buffer OFF(X)",
          SCREEN_W - 108, 2, S->buffer_on ? CLR_GREEN : CLR_RED);

    if (S->win) {
        print_centered("CLEARED!", SCREEN_W / 2, SCREEN_H / 2 - 6, CLR_YELLOW);
        print_centered("press Z to play again", SCREEN_W / 2, SCREEN_H / 2 + 4, CLR_WHITE);
    }
}
