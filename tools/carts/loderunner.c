#include "studio.h"
#include <stddef.h>
#include <string.h>

// LODE RUNNER
// Run, climb ladders, cross ropes, dig holes to trap a guard, grab all the
// gold, then climb the revealed exit ladder. Two levels. Drawn from primitives.
//
// Controls: arrows run/climb/rope, Z dig down-left, X dig down-right, R restart.

// ---- world geometry --------------------------------------------------------
#define COLS   20
#define ROWS   11
#define CELL   16
#define ORG_Y  16          // play field starts below the HUD bar

// cell types
#define T_EMPTY  0
#define T_BRICK  1         // diggable floor
#define T_STONE  2         // solid, undiggable
#define T_LADDER 3
#define T_ROPE   4         // hang/cross
#define T_GOLD   5
#define T_EXIT   6         // exit-ladder cells (hidden until gold == 0)

static char grid[ROWS][COLS];
static float regrow[ROWS][COLS];   // >0 = this brick is a hole, counting back

#define HOLE_TIME 4.0f             // seconds a dug hole stays open

// ---- entities --------------------------------------------------------------
// positions are pixel positions of the top-left of a CELL-sized actor
static float px, py;               // player
static int   pdir;                 // facing: -1 left, +1 right
static float gx, gy;               // one guard
static int   gdir;
static float grespawn;             // >0 = guard is crushed/respawning timer

#define SPD    52.0f               // run/climb/fall px per second (snapped to grid)

static int   gold_total, gold_left;
static int   level;                // 0 or 1
static int   state;                // 0 play, 1 dead, 2 level-clear, 3 win
static float state_t;
static int   best_gold;

// ---- levels (string layouts) ----------------------------------------------
// chars: ' '=empty  #=brick  =stone(=)  H=ladder  -=rope  $=gold
//        P=player start  G=guard start  X=exit-ladder cell
// 20 cols x 11 rows.
static const char *LEVELS[2][ROWS] = {
  {
    "X         $        X",
    "X #######H##### ###X",
    "X $   H  H   $  H  X",
    "X#####H#####H##H##=X",
    "  $   H  G  H  $ H  ",
    "######H#####H#H##H##",
    "$  H        H H  H $",
    "###H##H######-----H#",
    "   H  H  $       H  ",
    "===H==H=======H==H==",
    "@@@@@@@@@@@@@@@@@@@@@",
  },
  {
    "X      $    $      X",
    "X#H######H######H##X",
    " $H  $   H   $  GH $",
    "##H####H#H#H####H##=",
    "  H   $H H H$   H   ",
    "##H#H##H#H#H##H#H#H##",
    "$ H H  $ H $  H H H $",
    "#-H-H####H####H-H-H#",
    "  H H       $ H H H ",
    "==H=H===H=====H=H=H=",
    "@@@@@@@@@@@@@@@@@@@@@",
  },
};

// the bottom solid floor row uses '@' (stone). Player/guard starts noted with P/G
// but our layouts above embed G; P start is computed (first open cell top-left).

static float pstart_x, pstart_y, gstart_x, gstart_y;

// ---- helpers ---------------------------------------------------------------
static int cell_at(int cx, int cy) {
    if (cx < 0 || cx >= COLS || cy < 0 || cy >= ROWS) return T_STONE; // walls block
    return grid[cy][cx];
}

// is this cell something you can stand ON (its top is solid)?
static bool solid_cell(int cx, int cy) {
    if (cx < 0 || cx >= COLS || cy >= ROWS) return true;   // edges/floor solid
    if (cy < 0) return false;
    int t = grid[cy][cx];
    if (t == T_BRICK && regrow[cy][cx] <= 0) return true;  // intact brick
    if (t == T_STONE) return true;
    return false;
}

static bool is_ladder(int cx, int cy) {
    int t = cell_at(cx, cy);
    return t == T_LADDER || (t == T_EXIT && gold_left == 0);
}
static bool is_rope(int cx, int cy) { return cell_at(cx, cy) == T_ROPE; }

// can an actor occupy this cell center (not blocked by intact brick/stone)?
static bool passable(int cx, int cy) {
    if (cx < 0 || cx >= COLS || cy < 0 || cy >= ROWS) return false;
    int t = grid[cy][cx];
    if (t == T_STONE) return false;
    if (t == T_BRICK && regrow[cy][cx] <= 0) return false;
    return true;
}

static void load_level(int lv) {
    gold_total = 0;
    memset(regrow, 0, sizeof(regrow));
    pstart_x = CELL; pstart_y = 0;
    gstart_x = CELL * (COLS - 2); gstart_y = 0;
    bool got_p = false;
    for (int y = 0; y < ROWS; y++) {
        const char *row = LEVELS[lv][y];
        for (int x = 0; x < COLS; x++) {
            char c = row[x];
            char t = T_EMPTY;
            switch (c) {
                case '#': t = T_BRICK;  break;
                case '@':
                case '=': t = T_STONE;  break;
                case 'H': t = T_LADDER; break;
                case '-': t = T_ROPE;   break;
                case '$': t = T_GOLD;   gold_total++; break;
                case 'X': t = T_EXIT;   break;
                case 'G': t = T_EMPTY;  gstart_x = x*CELL; gstart_y = y*CELL; break;
                default:  t = T_EMPTY;  break;
            }
            grid[y][x] = t;
        }
    }
    // player start: first empty-ish cell scanning top-left rows
    for (int y = 0; y < ROWS && !got_p; y++)
        for (int x = 1; x < COLS-1 && !got_p; x++)
            if (grid[y][x] == T_EMPTY && solid_cell(x, y+1)) {
                pstart_x = x*CELL; pstart_y = y*CELL; got_p = true;
            }
    gold_left = gold_total;
    px = pstart_x; py = pstart_y; pdir = 1;
    gx = gstart_x; gy = gstart_y; gdir = -1; grespawn = 0;
}

void init() {
    level = 0;
    state = 0; state_t = 0;
    best_gold = load_int("best_gold", 0);
    load_level(level);
}

// snap helper: nearest cell index from a pixel coord
static int colof(float x) { return (int)((x + CELL/2) / CELL); }
static int rowof(float y) { return (int)((y + CELL/2) / CELL); }

// ---- movement for an actor (shared by player & guard) ----------------------
// returns via pointers. mode-of-movement is driven by intent flags.
// handles: run on ground, climb ladder, hang/cross rope, fall.
static void step_actor(float *ax, float *ay, int *adir,
                       int wantx, int wanty, float spd) {
    int cx = colof(*ax), cy = rowof(*ay);
    bool aligned_x = ((int)(*ax) % CELL) == 0 || abs((int)(*ax) - cx*CELL) < 2;
    bool aligned_y = ((int)(*ay) % CELL) == 0 || abs((int)(*ay) - cy*CELL) < 2;

    float d = spd * dt();

    bool on_ladder = is_ladder(cx, cy);
    bool on_rope   = is_rope(cx, cy);
    bool support   = solid_cell(cx, cy+1) || on_ladder ||
                     is_ladder(cx, cy+1) || on_rope;

    // FALL when nothing supports us and we aren't climbing
    bool climbing_now = (wanty != 0) && (on_ladder ||
                        (wanty > 0 && is_ladder(cx, cy+1)));
    if (!support && !climbing_now) {
        // snap to column, fall straight down
        *ax = (float)(cx * CELL);
        float ny = *ay + d;
        int below = rowof(ny);
        if (solid_cell(cx, below) && below*CELL <= ny) {
            ny = (float)((below-1) * CELL); // land on top of solid
            if (ny < *ay) ny = *ay;
        }
        // also stop on a brick top one row down if we cross it
        if (solid_cell(cx, cy+1) && ny > cy*CELL) ny = (float)(cy*CELL);
        *ay = ny;
        return;
    }

    // VERTICAL intent (ladder climb)
    if (wanty < 0 && aligned_x) {                  // up
        if (is_ladder(cx, cy) || is_ladder(cx, cy-1)) {
            *ax = (float)(cx*CELL);
            float ny = *ay - d;
            if (!is_ladder(cx, rowof(ny)) && !is_ladder(cx, cy) ) ny = *ay;
            if (rowof(ny) < 0) ny = 0;
            *ay = ny; return;
        }
    }
    if (wanty > 0 && aligned_x) {                  // down
        if (is_ladder(cx, cy+1) || is_ladder(cx, cy) ||
            (!solid_cell(cx, cy+1) && passable(cx, cy+1))) {
            *ax = (float)(cx*CELL);
            float ny = *ay + d;
            int below = rowof(ny);
            if (solid_cell(cx, below)) ny = (float)((below-1)*CELL);
            *ay = ny; return;
        }
    }

    // HORIZONTAL intent (run / rope shuffle)
    if (wantx != 0 && aligned_y) {
        *adir = wantx;
        int ncx = colof(*ax + wantx * (CELL/2 + 1));
        if (passable(ncx, cy) || ncx == cx) {
            *ay = (float)(cy*CELL);
            float nx = *ax + wantx * d;
            // block at solid neighbour
            int edgecol = colof(nx + (wantx>0 ? CELL-1 : 0));
            if (!passable(edgecol, cy)) {
                nx = (float)(cx*CELL);
            }
            if (nx < 0) nx = 0;
            if (nx > (COLS-1)*CELL) nx = (float)((COLS-1)*CELL);
            *ax = nx; return;
        } else {
            *ax = (float)(cx*CELL); // pressed into a wall: just snap & face
        }
    }
}

// ---- digging ---------------------------------------------------------------
static void try_dig(int sidedir) {
    int cx = colof(px), cy = rowof(py);
    int tx = cx + sidedir;
    int ty = cy + 1;                  // the floor cell beside-and-below
    if (tx < 0 || tx >= COLS || ty >= ROWS) return;
    if (grid[ty][tx] == T_BRICK && regrow[ty][tx] <= 0) {
        // must be able to stand where we are / brick must be reachable (nothing above the brick blocking is fine)
        regrow[ty][tx] = HOLE_TIME;
        note(40, INSTR_NOISE, 5);
        hit(30, INSTR_NOISE, 4, 120);
        shake(2.5f);
    }
}

// ---- guard AI ---------------------------------------------------------------
static void guard_think() {
    if (grespawn > 0) {
        grespawn -= dt();
        if (grespawn <= 0) { gx = gstart_x; gy = gstart_y; gdir = -1; }
        return;
    }
    int gcx = colof(gx), gcy = rowof(gy);

    // if sitting in a hole that's regrowing & nearly closed -> crushed
    if (grid[gcy][gcx] == T_BRICK && regrow[gcy][gcx] > 0) {
        // stuck in the pit; if it's about to close, crush
        if (regrow[gcy][gcx] < 0.6f) {
            grespawn = 1.4f;
            note(28, INSTR_SAW, 6);
            shake(3.0f);
        }
        return; // can't move while pitted
    }

    int wx = 0, wy = 0;
    int dcx = colof(px) - gcx;
    int dcy = rowof(py) - gcy;

    bool g_canclimb_up = is_ladder(gcx, gcy) || is_ladder(gcx, gcy-1);
    bool g_canclimb_dn = is_ladder(gcx, gcy+1);

    // prefer vertical if player is on a different row and a ladder helps
    if (dcy < 0 && g_canclimb_up)       wy = -1;
    else if (dcy > 0 && (g_canclimb_dn || !solid_cell(gcx, gcy+1))) wy = 1;
    else if (dcx != 0)                  wx = sgn(dcx);
    else if (dcx == 0)                  wx = 0;

    // if no useful vertical and on same row, chase horizontally
    if (wy == 0 && wx == 0 && dcx != 0) wx = sgn(dcx);

    step_actor(&gx, &gy, &gdir, wx, wy, SPD * 0.78f);
}

// ---- collect & win/lose -----------------------------------------------------
static void check_gold() {
    int cx = colof(px), cy = rowof(py);
    if (cell_at(cx, cy) == T_GOLD) {
        grid[cy][cx] = T_EMPTY;
        gold_left--;
        int n = gold_total - gold_left;
        note(72 + (n % 6) * 3, INSTR_SINE, 4);
        if (gold_left == 0) {
            // reveal exit: arpeggio
            note(84, INSTR_TRI, 5);
            schedule(120, 88, INSTR_TRI, 5);
            schedule(240, 91, INSTR_TRI, 5);
        }
    }
}

void update() {
    if (state == 1) {                  // dead
        state_t += dt();
        if (state_t > 1.2f && (btnp(0,BTN_A)||btnp(0,BTN_B)||keyp('R'))) {
            state = 0; state_t = 0; load_level(level);
        }
        return;
    }
    if (state == 2) {                  // level clear -> next
        state_t += dt();
        if (state_t > 1.0f) {
            level = 1 - level;          // toggle; if back to 0 we've done both
            if (level == 0) { state = 3; state_t = 0; }  // win
            else { state = 0; state_t = 0; load_level(level); }
        }
        return;
    }
    if (state == 3) {                  // win
        state_t += dt();
        if (btnp(0,BTN_A)||btnp(0,BTN_B)||keyp('R')) {
            int score = gold_total; // simple
            if (score > best_gold) { best_gold = score; save_int("best_gold", best_gold); }
            level = 0; state = 0; state_t = 0; load_level(level);
        }
        return;
    }

    // ---- playing ----
    int wx = 0, wy = 0;
    if (btn(0,BTN_LEFT))  wx = -1;
    if (btn(0,BTN_RIGHT)) wx =  1;
    if (btn(0,BTN_UP))    wy = -1;
    if (btn(0,BTN_DOWN))  wy =  1;

    if (btnp(0,BTN_A) || keyp('Z')) try_dig(-1);
    if (btnp(0,BTN_B) || keyp('X')) try_dig( 1);

    step_actor(&px, &py, &pdir, wx, wy, SPD);
    check_gold();

    // tick down all holes; regrowing
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            if (regrow[y][x] > 0) {
                regrow[y][x] -= dt();
                if (regrow[y][x] < 0) regrow[y][x] = 0;
            }

    guard_think();

    // death: guard touches player on open ground (not while pitted)
    if (grespawn <= 0) {
        int gcx = colof(gx), gcy = rowof(gy);
        bool g_pitted = (grid[gcy][gcx] == T_BRICK && regrow[gcy][gcx] > 0);
        if (!g_pitted && boxes_touch((int)px+2,(int)py+2,CELL-4,CELL-4,
                                     (int)gx+2,(int)gy+2,CELL-4,CELL-4)) {
            state = 1; state_t = 0;
            note(36, INSTR_NOISE, 7);
            hit(24, INSTR_SAW, 6, 400);
            shake(5.0f);
            return;
        }
    }

    // win: gold gone & player reached top exit cell
    if (gold_left == 0) {
        int cx = colof(px), cy = rowof(py);
        if (cell_at(cx, cy) == T_EXIT && cy <= 0) {
            state = 2; state_t = 0;
            fade(0); // (reset)
            note(76, INSTR_TRI, 5); schedule(120, 83, INSTR_TRI, 5);
            schedule(240, 88, INSTR_TRI, 6);
        }
    }
}

// ---- drawing ---------------------------------------------------------------
static void draw_brick(int sx, int sy) {
    rectfill(sx, sy, CELL, CELL, CLR_BROWN);
    // mortar lines (offset courses)
    for (int yy = 0; yy < CELL; yy += 4) {
        line(sx, sy+yy, sx+CELL-1, sy+yy, CLR_DARK_BROWN);
        int off = ((yy/4) % 2) ? 4 : 8;          // stagger the vertical joints
        line(sx+off,   sy+yy, sx+off,   sy+yy+3, CLR_DARK_BROWN);
        line(sx+off+8, sy+yy, sx+off+8, sy+yy+3, CLR_DARK_BROWN);
    }
    line(sx, sy, sx+CELL-1, sy, CLR_DARK_PEACH); // highlight top
}

static void draw_stone(int sx, int sy) {
    rectfill(sx, sy, CELL, CELL, CLR_DARK_GREY);
    rect(sx, sy, CELL, CELL, CLR_DARKER_GREY);
    pset(sx+3, sy+5, CLR_LIGHT_GREY);
    pset(sx+10, sy+9, CLR_LIGHT_GREY);
}

static void draw_ladder(int sx, int sy, int color) {
    line(sx+3,  sy, sx+3,  sy+CELL-1, color);
    line(sx+12, sy, sx+12, sy+CELL-1, color);
    for (int yy = 2; yy < CELL; yy += 5)
        line(sx+3, sy+yy, sx+12, sy+yy, color);
}

static void draw_rope(int sx, int sy) {
    line(sx, sy+2, sx+CELL-1, sy+2, CLR_BROWN);
    for (int xx = 0; xx < CELL; xx += 3)
        pset(sx+xx, sy+3, CLR_DARK_BROWN);
}

static void draw_gold(int sx, int sy) {
    int b = blink(20) ? 1 : 0;
    rectfill(sx+3, sy+5, 10, 7, CLR_YELLOW);
    rectfill(sx+3, sy+5, 10, 2, CLR_LIGHT_YELLOW);
    rect(sx+3, sy+5, 10, 7, CLR_ORANGE);
    if (b) pset(sx+5, sy+6, CLR_WHITE);
}

static void draw_actor(int sx, int sy, int dir, int body, int trim) {
    // simple chunky runner
    int legf = anim(2, 8, 0);
    rectfill(sx+5, sy+1, 6, 5, trim);        // head
    pset(sx + (dir>0?9:6), sy+2, CLR_BLACK); // eye
    rectfill(sx+4, sy+6, 8, 6, body);        // torso
    // arms
    line(sx+3, sy+7, sx+3, sy+10, body);
    line(sx+12, sy+7, sx+12, sy+10, body);
    // legs animate
    if (legf) { rectfill(sx+4, sy+12, 3, 4, trim); rectfill(sx+9, sy+12, 3, 4, trim); }
    else      { rectfill(sx+5, sy+12, 3, 4, trim); rectfill(sx+8, sy+12, 3, 4, trim); }
}

void draw() {
    cls(CLR_BROWNISH_BLACK);

    // HUD bar
    rectfill(0, 0, SCREEN_W, ORG_Y, CLR_DARKER_BLUE);
    print(str("LEVEL %d", level+1), 4, 4, CLR_LIGHT_PEACH);
    print_centered(str("GOLD %d/%d", gold_total-gold_left, gold_total), 4, CLR_YELLOW);
    print_right(str("BEST %d", best_gold), SCREEN_W-4, 4, CLR_LIME_GREEN);

    camera(0, -ORG_Y);

    // tiles
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++) {
            int sx = x*CELL, sy = y*CELL;
            int t = grid[y][x];
            switch (t) {
                case T_BRICK:
                    if (regrow[y][x] > 0) {
                        // a hole: show partial refill from the edges as it regrows
                        float p = 1.0f - clamp(regrow[y][x] / HOLE_TIME, 0, 1);
                        int g = (int)(p * (CELL/2));
                        if (g > 0) {
                            rectfill(sx, sy, g, CELL, CLR_BROWN);
                            rectfill(sx+CELL-g, sy, g, CELL, CLR_BROWN);
                        }
                    } else draw_brick(sx, sy);
                    break;
                case T_STONE:  draw_stone(sx, sy); break;
                case T_LADDER: draw_ladder(sx, sy, CLR_PEACH); break;
                case T_ROPE:   draw_rope(sx, sy); break;
                case T_GOLD:   draw_gold(sx, sy); break;
                case T_EXIT:
                    if (gold_left == 0) {
                        int c = blink(8) ? CLR_LIGHT_YELLOW : CLR_YELLOW;
                        draw_ladder(sx, sy, c);
                    }
                    break;
                default: break;
            }
        }

    // guard
    if (grespawn <= 0) {
        draw_actor((int)gx, (int)gy, gdir, CLR_RED, CLR_DARK_ORANGE);
    } else if (blink(4)) {
        // crushed flash at spawn point
        draw_actor((int)gstart_x, (int)gstart_y, -1, CLR_DARK_RED, CLR_RED);
    }

    // player
    if (state != 1 || blink(3))
        draw_actor((int)px, (int)py, pdir, CLR_BLUE, CLR_LIGHT_PEACH);

    camera(0, 0);

    // overlays
    if (state == 1) {
        print_centered("CAUGHT!", SCREEN_H/2-6, CLR_RED);
        print_centered("Z / R to retry", SCREEN_H/2+6, CLR_LIGHT_GREY);
    } else if (state == 2) {
        print_centered("LEVEL CLEAR", SCREEN_H/2-6, CLR_GREEN);
    } else if (state == 3) {
        rectfill(SCREEN_W/2-70, SCREEN_H/2-22, 140, 44, CLR_BLACK);
        rect    (SCREEN_W/2-70, SCREEN_H/2-22, 140, 44, CLR_YELLOW);
        print_centered("YOU ESCAPED!", SCREEN_H/2-12, CLR_YELLOW);
        print_centered(str("GOLD %d", gold_total), SCREEN_H/2, CLR_LIME_GREEN);
        print_centered("Z to play again", SCREEN_H/2+12, CLR_LIGHT_GREY);
    } else if (gold_left == 0) {
        if (blink(15)) print_centered("ESCAPE UP THE LADDER!", SCREEN_H-12, CLR_YELLOW);
    }
}
