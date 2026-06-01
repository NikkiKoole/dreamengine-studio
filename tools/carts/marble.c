#include "studio.h"

// MARBLE MADNESS
// Arrows roll the marble through an isometric course to the GOAL before time runs out.
// Slopes accelerate you, edges drop you off (respawn), acid kills. Z/Enter to restart.

// ---- course grid -------------------------------------------------
// the course is a small tile grid. each tile has a TYPE and a base HEIGHT.
// world coords: gx,gy in tile units; gz = elevation (world up).
#define GW 12
#define GH 12
#define TILE 22          // iso half-width per tile step (screen px)
#define TILE_H 11        // iso half-height per tile step (screen px)
#define ZSTEP 14         // screen px per unit of elevation

enum { T_VOID, T_FLOOR, T_WALL, T_ACID, T_START, T_GOAL, T_CHK,
       T_RN, T_RE, T_RS, T_RW };   // ramps tilt toward N/E/S/W (downhill that way)

// tile type per cell, and base elevation (height of the LOW corner)
static unsigned char ttype[GH][GW];
static signed char   thgt [GH][GW];

// the hand-laid course. '#' wall, '.' floor, 'A' acid, 'S' start, 'G' goal,
// 'C' checkpoint, ' ' void, ramps n/e/s/w (lowercase). digits after = none.
// elevation is layered in by code below.
static const char *LAYOUT[GH] = {
    "   ######   ",
    "  #S....e#  ",
    "  #.##.#.#  ",
    "  #..A..s#  ",
    "  ##.#####  ",
    "   #C#      ",
    "   #.####   ",
    "   #..n.#   ",
    "   ##.#A#   ",
    "    #..s#   ",
    "    #CG#    ",
    "    ####    ",
};

static int start_gx, start_gy;
static int chk_gx, chk_gy;   // active checkpoint (starts at start)

static void build_course(void) {
    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            char c = LAYOUT[y][x];
            int t = T_VOID, h = 0;
            switch (c) {
                case '#': t = T_WALL;  h = 2; break;
                case '.': t = T_FLOOR; h = 0; break;
                case 'A': t = T_ACID;  h = 0; break;
                case 'S': t = T_START; h = 0; break;
                case 'G': t = T_GOAL;  h = 0; break;
                case 'C': t = T_CHK;   h = 0; break;
                case 'n': t = T_RN; h = 1; break;
                case 'e': t = T_RE; h = 1; break;
                case 's': t = T_RS; h = 1; break;
                case 'w': t = T_RW; h = 1; break;
                default:  t = T_VOID; h = 0; break;
            }
            ttype[y][x] = (unsigned char)t;
            thgt [y][x] = (signed char)h;
            if (c == 'S') { start_gx = x; start_gy = y; }
        }
    }
    // give the course a gentle overall tilt so the top is higher than the bottom
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            if (ttype[y][x] != T_VOID)
                thgt[y][x] += (signed char)((GH - 1 - y) / 3);
}

static bool walkable(int t) {
    return t == T_FLOOR || t == T_START || t == T_GOAL || t == T_CHK ||
           t >= T_RN;   // ramps too
}

// elevation (in world z units, float) of the GROUND surface at fractional cell (fx,fy)
static float ground_z(float fx, float fy) {
    int x = (int)fx, y = (int)fy;
    if (x < 0 || x >= GW || y < 0 || y >= GH) return -999.0f;
    int t = ttype[y][x];
    if (t == T_VOID) return -999.0f;
    float base = (float)thgt[y][x];
    float lx = fx - x, ly = fy - y;   // 0..1 within the tile
    switch (t) {                       // ramp: +1 unit across the tile toward downhill
        case T_RN: return base + ly;          // high at south, low at north
        case T_RS: return base + (1.0f - ly); // high at north
        case T_RW: return base + lx;          // high at east, low at west
        case T_RE: return base + (1.0f - lx); // high at west
        default:   return base;
    }
}

// downhill push vector (in grid units) from the local slope. zero on flat tiles.
static void slope_push(float fx, float fy, float *ax, float *ay) {
    *ax = 0; *ay = 0;
    int x = (int)fx, y = (int)fy;
    if (x < 0 || x >= GW || y < 0 || y >= GH) return;
    switch (ttype[y][x]) {
        case T_RN: *ay = -1; break;
        case T_RS: *ay =  1; break;
        case T_RW: *ax = -1; break;
        case T_RE: *ax =  1; break;
        default: break;
    }
}

// ---- iso projection ----------------------------------------------
static int cam_x, cam_y;

static void iso(float fx, float fy, float fz, int *sx, int *sy) {
    int ox = SCREEN_W / 2;
    int oy = 46;
    *sx = ox + (int)((fx - fy) * TILE);
    *sy = oy + (int)((fx + fy) * TILE_H) - (int)(fz * ZSTEP);
}

// ---- marble + game state -----------------------------------------
static float mx, my, mz;       // marble position (grid units + z world)
static float vx, vy;           // velocity in grid units / sec
static float vz, mh;           // vertical velocity + visual hop height while falling
static int   lives;
static float time_left;
static bool  falling;
static float fall_t;
static int   best_time;        // *100 (centiseconds), 0 = none

enum { ST_PLAY, ST_WIN, ST_OVER };
static int gstate;

static int roll_slot = 5;

static void respawn(void) {
    mx = chk_gx + 0.5f;
    my = chk_gy + 0.5f;
    mz = ground_z(mx, my);
    vx = vy = vz = 0; mh = 0;
    falling = false;
}

static void start_game(void) {
    build_course();
    chk_gx = start_gx; chk_gy = start_gy;
    lives = 3;
    time_left = 60.0f;
    gstate = ST_PLAY;
    respawn();
}

void init(void) {
    // a rolling-rumble instrument: filtered saw, sustained
    instrument(roll_slot, INSTR_SAW, 8, 40, 5, 120);
    instrument_filter(roll_slot, FILTER_LOW, 700, 4);
    best_time = load_int("best", 0);
    start_game();
}

static void die(void) {
    lives--;
    falling = false; fall_t = 0;
    shake(5);
    note(40, INSTR_NOISE, 6);
    if (lives <= 0) { gstate = ST_OVER; }
    else            { respawn(); }
}

void update(void) {
    if (gstate != ST_PLAY) {
        if (btnp(0, BTN_A) || keyp(KEY_ENTER) || keyp(KEY_SPACE)) start_game();
        return;
    }

    float d = dt();
    if (d > 0.05f) d = 0.05f;

    // ---- falling off an edge / into acid : a short drop animation ----
    if (falling) {
        fall_t += d;
        vz -= 22.0f * d;        // gravity pulls the marble down out of view
        mz += vz * d;
        mh += 1.0f;
        mx += vx * d; my += vy * d;
        if (fall_t > 0.55f) die();
        return;
    }

    // ---- input: tilt nudge, mapped to the iso axes ----
    // screen-up should move the marble up-and-back in the iso world: -gx,-gy.
    const float ACC = 9.0f;
    float ix = 0, iy = 0;
    if (btn(0, BTN_UP))    { ix -= 1; iy -= 1; }
    if (btn(0, BTN_DOWN))  { ix += 1; iy += 1; }
    if (btn(0, BTN_LEFT))  { ix -= 1; iy += 1; }
    if (btn(0, BTN_RIGHT)) { ix += 1; iy -= 1; }
    float il = length((int)(ix*100), (int)(iy*100)) / 100.0f;
    if (il > 0.001f) { ix /= il; iy /= il; }
    vx += ix * ACC * d;
    vy += iy * ACC * d;

    // ---- slope gravity ----
    float ax, ay; slope_push(mx, my, &ax, &ay);
    const float SLOPE_G = 6.5f;
    vx += ax * SLOPE_G * d;
    vy += ay * SLOPE_G * d;

    // ---- rolling friction ----
    float fric = 0.86f;
    vx *= 1.0f - (1.0f - fric) * (d * 60.0f) * 0.4f;
    vy *= 1.0f - (1.0f - fric) * (d * 60.0f) * 0.4f;

    // ---- speed clamp ----
    float sp = length((int)(vx*100), (int)(vy*100)) / 100.0f;
    const float VMAX = 5.0f;
    if (sp > VMAX) { vx = vx / sp * VMAX; vy = vy / sp * VMAX; }

    // ---- integrate, with wall collision (axis-separated) ----
    float nx = mx + vx * d;
    {
        int cx = (int)nx, cy = (int)my;
        if (cx < 0 || cx >= GW || ttype[cy][cx] == T_WALL ||
            ttype[cy][cx] == T_VOID) {
            // wall: bounce; void: let it roll off (handled below)
            if (cx >= 0 && cx < GW && ttype[cy][cx] == T_WALL) {
                vx = -vx * 0.4f; note(55, INSTR_SQUARE, 2);
            } else nx = nx;   // allow walking onto void to trigger fall
        }
    }
    {
        int cx = (int)nx, cy = (int)my;
        if (cx >= 0 && cx < GW && ttype[cy][cx] == T_WALL) nx = mx;
    }
    mx = nx;

    float ny = my + vy * d;
    {
        int cx = (int)mx, cy = (int)ny;
        if (cy >= 0 && cy < GH && cx >= 0 && cx < GW && ttype[cy][cx] == T_WALL) {
            vy = -vy * 0.4f; ny = my; note(55, INSTR_SQUARE, 2);
        }
    }
    my = ny;

    // keep z glued to the ground surface
    float gz = ground_z(mx, my);
    if (gz > -900.0f) mz = lerp(mz, gz, 1.0f);

    // ---- tile under marble: edge / acid / goal / checkpoint ----
    int cx = (int)mx, cy = (int)my;
    int under = (cx < 0 || cx >= GW || cy < 0 || cy >= GH) ? T_VOID : ttype[cy][cx];

    if (under == T_VOID) {           // rolled off an edge
        falling = true; vz = -0.5f; fall_t = 0;
        note(60, INSTR_SINE, 3);
    } else if (under == T_ACID) {    // sank into acid
        falling = true; vz = -0.5f; fall_t = 0;
        note(34, INSTR_NOISE, 6);
    } else if (under == T_GOAL) {
        gstate = ST_WIN;
        int cs = (int)(time_left * 100);
        if (best_time == 0 || cs > best_time) { best_time = cs; save_int("best", cs); }
        // little fanfare
        note(72, INSTR_SINE, 6);
        schedule(110, 76, INSTR_SINE, 6);
        schedule(220, 79, INSTR_SINE, 6);
    } else if (under == T_CHK) {
        if (chk_gx != cx || chk_gy != cy) {
            chk_gx = cx; chk_gy = cy;
            note(84, INSTR_SINE, 4);
        }
    }

    // ---- rolling rumble: pitch tracks speed ----
    if (gstate == ST_PLAY) {
        if (sp > 0.6f && frame() % 6 == 0) {
            int midi = 36 + (int)(sp * 6.0f);
            hit(midi, roll_slot, 2 + (int)(sp), 90);
        }
    }

    // ---- camera follows the marble ----
    int psx, psy; iso(mx, my, mz, &psx, &psy);
    cam_x = psx - SCREEN_W / 2;
    cam_y = psy - SCREEN_H / 2;

    // ---- timer ----
    time_left -= d;
    if (time_left <= 0) { time_left = 0; gstate = ST_OVER; }
}

// ---- drawing -----------------------------------------------------

// draw one iso tile as a top diamond + left/right side faces down to z=0 floor
static void draw_tile(int x, int y) {
    int t = ttype[y][x];
    if (t == T_VOID) return;

    // surface elevation of the four corners (handle ramps with corner z)
    float base = (float)thgt[y][x];
    float zN = base, zE = base, zS = base, zW = base;  // corners: N=(x,y) etc per iso
    // ramp corner heights: top diamond is built from cell corners (x,y)-(x+1,y)-(x+1,y+1)-(x,y+1)
    float z00 = ground_z(x + 0.001f, y + 0.001f);
    float z10 = ground_z(x + 0.999f, y + 0.001f);
    float z11 = ground_z(x + 0.999f, y + 0.999f);
    float z01 = ground_z(x + 0.001f, y + 0.999f);

    int ax, ay, bx, by, ccx, ccy, dxp, dyp;
    iso((float)x,     (float)y,     z00, &ax,  &ay);   // top corner   (screen)
    iso((float)x + 1, (float)y,     z10, &bx,  &by);   // right corner
    iso((float)x + 1, (float)y + 1, z11, &ccx, &ccy);  // bottom corner
    iso((float)x,     (float)y + 1, z01, &dxp, &dyp);  // left corner

    int topc, lc, rc;
    switch (t) {
        case T_WALL:  topc = CLR_LIGHT_GREY; lc = CLR_DARK_GREY;  rc = CLR_DARK_GREY;  break;
        case T_ACID:  topc = CLR_GREEN;      lc = CLR_DARK_GREEN; rc = CLR_DARK_GREEN; break;
        case T_START: topc = CLR_BLUE;       lc = CLR_DARK_BLUE;  rc = CLR_DARKER_BLUE; break;
        case T_GOAL:  topc = blink(10) ? CLR_YELLOW : CLR_ORANGE; lc = CLR_DARK_ORANGE; rc = CLR_BROWN; break;
        case T_CHK:   topc = CLR_PINK;       lc = CLR_DARK_PURPLE; rc = CLR_DARKER_PURPLE; break;
        case T_RN: case T_RE: case T_RS: case T_RW:
                      topc = CLR_MEDIUM_GREY; lc = CLR_DARK_GREY; rc = CLR_DARKER_GREY; break;
        default:      topc = CLR_INDIGO;     lc = CLR_DARK_GREY; rc = CLR_DARKER_GREY; break;
    }

    // side faces (left = bottom-left edge, right = bottom-right edge), dropped to floor z=-1
    int fx_l, fy_l, fx_b, fy_b, fx_r, fy_r;
    iso((float)x,     (float)y + 1, -1.0f, &fx_l, &fy_l);
    iso((float)x + 1, (float)y + 1, -1.0f, &fx_b, &fy_b);
    iso((float)x + 1, (float)y,     -1.0f, &fx_r, &fy_r);

    // left face: top edge D->C , bottom edge fl->fb
    trifill(dxp, dyp, ccx, ccy, fx_b, fy_b, lc);
    trifill(dxp, dyp, fx_b, fy_b, fx_l, fy_l, lc);
    // right face: top edge C->B, bottom edge fb->fr
    trifill(ccx, ccy, bx, by, fx_r, fy_r, rc);
    trifill(ccx, ccy, fx_r, fy_r, fx_b, fy_b, rc);

    // acid shimmer on the top
    if (t == T_ACID && blink(6)) topc = CLR_LIME_GREEN;

    // top diamond (two tris)
    trifill(ax, ay, bx, by, ccx, ccy, topc);
    trifill(ax, ay, ccx, ccy, dxp, dyp, topc);

    // tile rim for readability
    line(ax, ay, bx, by, CLR_BLACK);
    line(bx, by, ccx, ccy, CLR_BLACK);
    line(ccx, ccy, dxp, dyp, CLR_BLACK);
    line(dxp, dyp, ax, ay, CLR_BLACK);
}

static void draw_marble(void) {
    int sx, sy; iso(mx, my, mz + 0.0f, &sx, &sy);
    sy -= 5;                      // sit the marble's center above the surface
    if (falling) sy += (int)(mh * 0.6f);

    int r = 6;
    // shadow on the ground
    int shx, shy; iso(mx, my, mz, &shx, &shy);
    if (!falling) ovalfill(shx, shy + 2, 6, 3, CLR_DARKER_BLUE);

    // marble body, shaded
    circfill(sx, sy, r, CLR_LIGHT_GREY);
    circfill(sx, sy, r, CLR_INDIGO);
    circfill(sx + 1, sy + 1, r - 1, CLR_LIGHT_GREY);
    circfill(sx + 2, sy + 2, r - 3, CLR_DARK_GREY);
    // highlight
    circfill(sx - 2, sy - 2, 2, CLR_WHITE);
    pset(sx - 1, sy - 1, CLR_WHITE);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    camera(cam_x, cam_y);

    // painter's order: back (low x+y) to front. iterate by x+y diagonal.
    for (int s = 0; s < GW + GH; s++) {
        for (int y = 0; y < GH; y++) {
            int x = s - y;
            if (x < 0 || x >= GW) continue;
            draw_tile(x, y);
        }
        // draw the marble in its diagonal band so it overlaps correctly
        if ((int)(mx + my) == s) draw_marble();
    }
    if ((int)(mx + my) >= GW + GH) draw_marble();

    camera(0, 0);

    // ---- HUD ----
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    // lives as little marbles
    for (int i = 0; i < lives; i++) circfill(7 + i * 11, 5, 3, CLR_LIGHT_GREY);
    // timer
    int col = time_left < 10 ? (blink(6) ? CLR_RED : CLR_ORANGE) : CLR_WHITE;
    print_centered(str("%05.2f", time_left), 2, col);
    if (best_time > 0)
        print_right(str("BEST %.1f", best_time / 100.0f), SCREEN_W - 3, 2, CLR_LIGHT_YELLOW);

    // ---- overlays ----
    if (gstate == ST_WIN) {
        fade(0.4f);
        rectfill(SCREEN_W/2-70, SCREEN_H/2-26, 140, 52, CLR_BLACK);
        rect    (SCREEN_W/2-70, SCREEN_H/2-26, 140, 52, CLR_YELLOW);
        print_centered("GOAL!", SCREEN_H/2-18, CLR_YELLOW);
        print_centered(str("TIME LEFT %.2f", time_left), SCREEN_H/2-4, CLR_WHITE);
        print_centered("Z to play again", SCREEN_H/2+12, CLR_LIGHT_GREY);
    } else if (gstate == ST_OVER) {
        fade(0.5f);
        rectfill(SCREEN_W/2-70, SCREEN_H/2-26, 140, 52, CLR_BLACK);
        rect    (SCREEN_W/2-70, SCREEN_H/2-26, 140, 52, CLR_RED);
        print_centered(time_left <= 0 ? "TIME UP" : "GAME OVER", SCREEN_H/2-18, CLR_RED);
        print_centered("the marble is lost", SCREEN_H/2-4, CLR_LIGHT_GREY);
        print_centered("Z to try again", SCREEN_H/2+12, CLR_WHITE);
    }
}
