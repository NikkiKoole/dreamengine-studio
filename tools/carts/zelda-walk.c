/* de:meta
{
  "slug": "zelda-walk",
  "title": "zelda walk (grid nudge)",
  "status": "active",
  "created": "2026-06-04",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "tilemap-collision",
    "grid-movement"
  ],
  "lineage": "A direct reverse-engineering of NES Zelda 1's six-trick movement system (troygilbert.com), standing alone as a tutorial cart with a toggle to disable the assists.",
  "genre": "maze",
  "description": "The secret middle ground between grid-move and free movement, rebuilt the way NES Zelda 1 actually did it (per the community deconstructions). Three pieces: an 8px HALF-GRID (you are never more than 4px off); PASSIVE REALIGN while walking — the perpendicular axis corrects 1px per pixel walked, so 4px off = realigned after 4px, almost invisible; and CORNER FEELERS — the crosswise hitbox is only 8px wide, so clipping a corner blocks just one feeler and you get shoved around it while you keep walking (your outer pixels may overlap the wall, the classic Link look). Free feel, grid forgiveness. Collect the gems, then press X to switch the assists off and feel the same room become constant corner-snagging. Arrows/WASD move (4-way), X toggles, Z restarts."
}
de:meta */
#include "studio.h"

// ZELDA WALK — the grid nudge, done the way NES Zelda 1 actually did it.
// (Reverse-engineered by the community; see troygilbert.com "Deconstructing
// Zelda: Movement Mechanics".) SIX tricks work together — the first three are
// the movement mechanism, the last three keep it robust and invisible:
//
//  1. THE GRID TARGET. Zelda aligned Link to an 8px half-grid — and because
//     Link was exactly tile-sized (16px), his collision strip (the middle of
//     his body) sat CENTERED in a tile whenever he was aligned. Our walker is
//     12px, so to reproduce that centered look we align the body's center to
//     CELL CENTERS (16k+8). Same mechanism, target spacing scaled to the body.
//  2. PASSIVE REALIGN — while walking, the PERPENDICULAR axis is corrected
//     1px per pixel walked toward the nearest alignment. 4px off = realigned
//     after 4px of walking. Tiny, proportional, almost invisible.
//  3. CORNER FEELERS — collision crosswise is only 8px wide: two "feeler"
//     points inset from the body's edges. Both clear = walk. ONE blocked = you
//     clipped a corner: you're shoved sideways ~1px/frame around it, then
//     continue. Both blocked = a real wall.
//  4. THE TARGET MUST FIT — wherever the realign aims, the 8px strip has to
//     land inside ONE cell. Our first version aimed at raw 8px lines, which
//     let the strip straddle two cells; near block rows the realign and the
//     feelers then fought each other — a stuck-in-the-wall crawl. Cell centers
//     (with 4px of margin either side) can never straddle.
//  5. NO NEW SOLIDS — the realign and the shove are 1px lateral steps that are
//     only allowed if they don't enter a cell the body wasn't already
//     overlapping. An existing legal overlap can slide OUT along a wall, but
//     the assists can never pull you DEEPER in.
//  6. ART HIDES THE OVERLAP — during a corner shove the body's outer pixels
//     may briefly overlap a wall. Real Zelda hides this two ways and so do we:
//     the body is drawn smaller than its logical footprint (Link's sprite has
//     transparent margins), and each block is full-size with a 2px DARK RIM
//     around a bright core, on a LIGHT floor (the NES dungeon palette) — a
//     transient overlap lands on the rim and reads as passing the block's
//     shadowed edge, never as standing inside it. (We tried a dark floor
//     first: the rim vanished into it and the blocks read as tiny. Contrast
//     is part of the trick.)
//
// The result: in open floor you never notice anything; at corners you glide
// around instead of snagging. Free feel, grid forgiveness.
//
// Press X to switch the assists (2,3,5) off and feel the trick disappear.
//
// Collect the gems.   Move: arrows / WASD (4-way).   X: toggle.   Z: restart.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

// ── TUNING — edit + hot-reload ───────────────────────────────────────────────
#define SPEED  1.5f     // walk speed px/frame (Zelda alternated 1,2,1,2 = 1.5)
#define CORR   1.0f     // realign px per frame of walking (1px per ~1px walked)
#define HLEN   6        // collision half-length ALONG the walk direction
#define HWID   4        // collision half-width ACROSS it (8px = one half-grid!)
#define BODY   6        // visual half-size (12×12; overlap lands in the blocks' inset margin)
#define CS     16
#define GW     20
#define GH     11
#define OY     12

// the room: single blocks with one-tile gaps — weaving through demands alignment
static const char *room[GH] = {
    "####################",
    "#..................#",
    "#.#.#.#.#..#.#.#.#.#",
    "#..................#",
    "#.#.#.#.#..#.#.#.#.#",
    "#..................#",
    "#.#.#.#.#..#.#.#.#.#",
    "#..................#",
    "#.#.#.#.#..#.#.#.#.#",
    "#..................#",
    "####################",
};

static bool solid_cell(int cx, int cy) {
    if (cx < 0 || cx >= GW || cy < 0 || cy >= GH) return true;
    return room[cy][cx] == '#';
}
static bool blocked_at(float x, float y) { return solid_cell((int)x / CS, (int)y / CS); }

#define NGEM 5
static const int gem_cell[NGEM][2] = { {5,3}, {14,5}, {9,5}, {3,7}, {16,9} };

enum { D_NONE, D_L, D_R, D_U, D_D };

STATE {
    float px, py;        // body CENTER, room pixels (y excludes OY)
    int   cur;           // axis we're walking along
    int   fx, fy;        // facing
    bool  nudge_on;
    bool  got[NGEM];
    int   ngot;
    bool  win;
};

static void reset_game(void) {
    S->px = 3 * CS + CS / 2.0f;  // spawn ALIGNED: centered on a cell
    S->py = 1 * CS + CS / 2.0f;
    S->cur = D_NONE; S->fx = 1; S->fy = 0;
    S->nudge_on = true;
    for (int i = 0; i < NGEM; i++) S->got[i] = false;
    S->ngot = 0; S->win = false;
}

void init(void) { reset_game(); }

// Alignment targets are CELL CENTERS (16k+8) — see tricks #1 and #4 in the
// header: the aligned strip [c-4, c+4) sits centered inside one cell with 4px
// margin both sides (never straddling two cells), and the 12px body is
// VISUALLY centered in corridors and gaps, just like tile-sized Link was.
static float nearest_align(float v) {
    return (float)(((int)((v - CS / 2) / CS + 0.5f)) * CS + CS / 2);
}

// may the mover take a 1px LATERAL step? Allowed only if it enters no NEW solid
// cell (a destination point may be blocked only if the matching current point
// already was — so an existing legal overlap can slide along a wall but the
// realign/shove can never pull the body DEEPER into one).
static bool lat_ok_h(float x, float y, float ny) {   // horizontal mover stepping y→ny
    float en = (ny > y) ? ny + HWID - 1 : ny - HWID; // strip edge after the step
    float ec = (ny > y) ? y + HWID - 1 : y - HWID;   // same edge now
    return (!blocked_at(x - HLEN, en) || blocked_at(x - HLEN, ec)) &&
           (!blocked_at(x + HLEN - 1, en) || blocked_at(x + HLEN - 1, ec));
}
static bool lat_ok_v(float x, float y, float nx) {   // vertical mover stepping x→nx
    float en = (nx > x) ? nx + HWID - 1 : nx - HWID;
    float ec = (nx > x) ? x + HWID - 1 : x - HWID;
    return (!blocked_at(en, y - HLEN) || blocked_at(ec, y - HLEN)) &&
           (!blocked_at(en, y + HLEN - 1) || blocked_at(ec, y + HLEN - 1));
}

// one sub-step (≤1px) of forward movement, Zelda-style:
// feelers at the leading edge, inset so the crosswise hitbox is 8px.
// returns true if we advanced; false = stopped (wall) or shoved (corner).
static bool try_forward(int dx_, int dy_, float amt) {
    float nx = S->px + dx_ * amt, ny = S->py + dy_ * amt;
    bool b1, b2;
    if (dx_) {                                       // horizontal: feelers above/below center
        float ex = nx + dx_ * HLEN;
        b1 = blocked_at(ex, ny - HWID);              // top feeler
        b2 = blocked_at(ex, ny + HWID - 1);          // bottom feeler
    } else {                                         // vertical: feelers left/right of center
        float ey = ny + dy_ * HLEN;
        b1 = blocked_at(nx - HWID, ey);              // left feeler
        b2 = blocked_at(nx + HWID - 1, ey);          // right feeler
    }
    if (!b1 && !b2) { S->px = nx; S->py = ny; return true; }
    if (b1 != b2 && S->nudge_on) {                   // clipped a corner → shove around it
        float push = b1 ? 1.0f : -1.0f;              // away from the blocked feeler
        if (dx_) { if (lat_ok_h(S->px, S->py, S->py + push)) S->py += push; }
        else     { if (lat_ok_v(S->px, S->py, S->px + push)) S->px += push; }
    }
    return false;                                    // wall (or corner with assists off)
}

static void walk(int dx_, int dy_) {
    float left = SPEED;
    bool moved = false;
    while (left > 0.001f) {                          // 1px sub-steps, like the NES ticks
        float step = left > 1.0f ? 1.0f : left;
        if (!try_forward(dx_, dy_, step)) break;
        moved = true;
        left -= step;
    }
    if (moved && S->nudge_on) {                      // passive realign: 1px per px walked,
        if (dx_) {                                   // and never INTO a wall (checked)
            float ny = S->py + clamp(nearest_align(S->py) - S->py, -CORR, CORR);
            if (ny != S->py && lat_ok_h(S->px, S->py, ny)) S->py = ny;
        } else {
            float nx = S->px + clamp(nearest_align(S->px) - S->px, -CORR, CORR);
            if (nx != S->px && lat_ok_v(S->px, S->py, nx)) S->px = nx;
        }
    }
}

static bool dir_held(int dir) {
    if (dir == D_L) return btn(0, BTN_LEFT);
    if (dir == D_R) return btn(0, BTN_RIGHT);
    if (dir == D_U) return btn(0, BTN_UP);
    if (dir == D_D) return btn(0, BTN_DOWN);
    return false;
}

void update(void) {
#ifdef DE_TRACE
    watch("z", "px=%.1f py=%.1f cur=%d nudge=%d gems=%d", S->px, S->py, S->cur, S->nudge_on, S->ngot);
#endif
    if (S->win) { if (btnp(0, BTN_A)) reset_game(); return; }

    if (btnp(0, BTN_B)) S->nudge_on = !S->nudge_on;

    // 4-way, last-pressed axis wins; fall back to whatever is still held
    if (btnp(0, BTN_LEFT))  S->cur = D_L;
    if (btnp(0, BTN_RIGHT)) S->cur = D_R;
    if (btnp(0, BTN_UP))    S->cur = D_U;
    if (btnp(0, BTN_DOWN))  S->cur = D_D;
    if (!dir_held(S->cur)) {
        S->cur = btn(0, BTN_LEFT) ? D_L : btn(0, BTN_RIGHT) ? D_R
               : btn(0, BTN_UP)   ? D_U : btn(0, BTN_DOWN)  ? D_D : D_NONE;
    }

    if (S->cur == D_L || S->cur == D_R) {
        walk(S->cur == D_L ? -1 : 1, 0);
        S->fx = (S->cur == D_L) ? -1 : 1; S->fy = 0;
    } else if (S->cur == D_U || S->cur == D_D) {
        walk(0, S->cur == D_U ? -1 : 1);
        S->fx = 0; S->fy = (S->cur == D_U) ? -1 : 1;
    }

    // gems
    for (int i = 0; i < NGEM; i++) {
        if (S->got[i]) continue;
        float gx = gem_cell[i][0] * CS + CS / 2.0f, gy = gem_cell[i][1] * CS + CS / 2.0f;
        if (near((int)S->px, (int)S->py, (int)gx, (int)gy, 10)) {
            S->got[i] = true; S->ngot++;
            note(76 + S->ngot * 2, INSTR_TRI, 3);
            if (S->ngot == NGEM) { S->win = true; strum(72, CHORD_MAJ, INSTR_TRI, 4, 40); }
        }
    }
}

void draw(void) {
    cls(CLR_MEDIUM_GREY);   // light floor, like a NES dungeon room — makes the
                            // dark block rims read as part of the blocks

    // trick #6 (part two): each block fills its whole 16px cell, but as a DARK
    // base with a bright core inset 2px — the NES tile-bevel look. A transient
    // ≤2px overlap during a corner shove lands on the dark ring, reading as
    // "passing the block's shadowed edge", never "inside the block".
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            if (room[y][x] == '#') {
                int bx2 = x * CS, by2 = OY + y * CS;
                rectfill(bx2, by2, CS, CS, CLR_TRUE_BLUE);          // full-size block
                rect(bx2, by2, CS, CS, CLR_DARKER_BLUE);            // 2px dark rim —
                rect(bx2 + 1, by2 + 1, CS - 2, CS - 2, CLR_DARKER_BLUE); // the bevel zone
                rect(bx2 + 2, by2 + 2, CS - 4, CS - 4, CLR_BLUE);   // bright highlight
            }

    for (int i = 0; i < NGEM; i++) {
        if (S->got[i]) continue;
        int gx = gem_cell[i][0] * CS + CS / 2, gy = OY + gem_cell[i][1] * CS + CS / 2;
        trifill(gx, gy - 4, gx + 4, gy, gx, gy + 4, CLR_YELLOW);
        trifill(gx, gy - 4, gx - 4, gy, gx, gy + 4, CLR_LIGHT_YELLOW);
    }

    // the player — trick #6 (part one): drawn smaller (12×12) than his logical
    // footprint, like Link's sprite margins — drawn after the blocks so any
    // remaining overlap reads as him passing in front of the bevel
    int x = (int)(S->px + 0.5f), y = (int)(S->py + 0.5f) + OY;
    rectfill(x - BODY, y - BODY, BODY * 2, BODY * 2, CLR_GREEN);
    rect(x - BODY, y - BODY, BODY * 2, BODY * 2, CLR_MEDIUM_GREEN);
    circfill(x + S->fx * 3, y + S->fy * 3, 1, CLR_BLACK);

    rectfill(0, 0, SCREEN_W, 10, CLR_DARKER_BLUE);
    print("zelda walk", 4, 2, CLR_WHITE);
    print(str("gems %d/%d", S->ngot, NGEM), 110, 2, CLR_YELLOW);
    print(S->nudge_on ? "nudge ON (X)" : "nudge OFF(X)",
          SCREEN_W - 104, 2, S->nudge_on ? CLR_GREEN : CLR_RED);

    if (S->win) {
        print_centered("ALL GEMS!", SCREEN_W / 2, SCREEN_H / 2 - 6, CLR_YELLOW);
        print_centered("press Z to play again", SCREEN_W / 2, SCREEN_H / 2 + 4, CLR_WHITE);
    }
}
