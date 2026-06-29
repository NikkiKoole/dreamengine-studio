/* de:meta
{
  "title": "Marble Madness",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game",
    "probe"
  ],
  "teaches": [
    "save-load-persistence",
    "verlet-integration",
    "isometric-projection"
  ],
  "lineage": "Marble Madness (Atari 1984) — isometric tile course with slope gravity and wall bounce; novel addition is a position-stream ghost race saved across sessions.",
  "genre": "arcade",
  "homage": "Marble Madness (1984)",
  "description": "Roll a chrome marble down three isometric plateaus to the blinking GOAL before the 30-second clock runs out — then RACE YOUR GHOST: every winning run that sets a new best time is recorded (a position stream, saved between sessions) and replays as a see-through blue marble rolling that exact run alongside you. Real momentum physics: arrows only nudge, gravity does the rest. Ramps are the only way UP a ledge; rolling OFF one gives real airtime — the speedrun tech: a one-unit drop on the east side skips the second ramp, but overshoot it and you fly off the open edge into the void. Acid kills, checkpoints bank progress, a filtered rumble rises with your speed, and a landing thump + screen kick sell every drop. Arrows roll the marble (relative to the iso view); Z or Enter restarts."
}
de:meta */
#include "studio.h"
#include <math.h>

// MARBLE MADNESS
// Arrows roll the marble down an isometric course of three PLATEAUS to the GOAL
// before time runs out. Ramps are the only way UP a ledge; rolling OFF one gives a
// little airborne hop — the speedrun tech: the east ledge skips the second ramp,
// but overshoot it and you fly off the open edge. Acid kills. Z/Enter to restart.
//
// RACE YOUR GHOST: every run is recorded; the run that sets a new best time becomes
// the GHOST — a see-through blue marble re-rolling that exact run alongside you,
// saved between sessions (save_bytes). Beat it and your new run takes its place.
// The ghost is a POSITION stream, not replayed input: this sim integrates with
// dt() (variable timestep), so input-replay would diverge — see
// docs/design/input-recording-looper.md ("ghost" section) for the trade-off.

// ---- course grid -------------------------------------------------
// the course is a small tile grid. each tile has a TYPE and a base HEIGHT.
// world coords: gx,gy in tile units; gz = elevation (world up).
#define GW 12
#define GH 12
#define TILE 22          // iso half-width per tile step (screen px)
#define TILE_H 11        // iso half-height per tile step (screen px)
#define ZSTEP 14         // screen px per unit of elevation
#define MR     0.22f     // marble radius in tile units (it collides as a circle, not a point)
#define MCLIMB 0.45f     // tallest ledge the marble can roll up — taller needs a ramp

enum { T_VOID, T_FLOOR, T_WALL, T_ACID, T_START, T_GOAL, T_CHK,
       T_RN, T_RE, T_RS, T_RW };   // ramps tilt toward N/E/S/W (downhill that way)

// tile type per cell, and base elevation (height of the LOW corner)
static unsigned char ttype[GH][GW];
static signed char   thgt [GH][GW];

// the hand-laid course. '#' wall, '.' floor, 'A' acid, 'S' start, 'G' goal,
// 'C' checkpoint, ' ' void, ramps n/e/s/w (lowercase, downhill that way).
// HGT carries elevation per tile: floor = surface height, ramp = its LOW base
// (a ramp always bridges exactly +1), wall = its drawn top.
// Three plateaus: top room (h2) -> ramp -> mid (h1) -> ramp -> bottom (h0).
// The (6,8) tile is the SHORTCUT: a one-unit drop off the mid ledge that skips
// the second ramp — but east of it the floor ends in open void.
static const char *LAYOUT[GH] = {
    "            ",
    " ######     ",
    " #S...#     ",
    " #....#     ",
    " ##ss##     ",
    "  #..A.#    ",
    "  #....#    ",
    "  #C...#    ",
    "  ###s.     ",
    "  #....     ",
    "  #.G..     ",
    "  #####     ",
};
static const char *HGT[GH] = {
    "            ",
    " 333333     ",
    " 322223     ",
    " 322223     ",
    " 331133     ",
    "  211112    ",
    "  211112    ",
    "  211112    ",
    "  22200     ",
    "  10000     ",
    "  10000     ",
    "  11111     ",
};

static int start_gx, start_gy;
static int chk_gx, chk_gy;   // active checkpoint (starts at start)

static void build_course(void) {
    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            char c = LAYOUT[y][x];
            int t;
            switch (c) {
                case '#': t = T_WALL;  break;
                case '.': t = T_FLOOR; break;
                case 'A': t = T_ACID;  break;
                case 'S': t = T_START; break;
                case 'G': t = T_GOAL;  break;
                case 'C': t = T_CHK;   break;
                case 'n': t = T_RN; break;
                case 'e': t = T_RE; break;
                case 's': t = T_RS; break;
                case 'w': t = T_RW; break;
                default:  t = T_VOID;  break;
            }
            char hc = HGT[y][x];
            ttype[y][x] = (unsigned char)t;
            thgt [y][x] = (signed char)(hc >= '0' && hc <= '9' ? hc - '0' : 0);
            if (c == 'S') { start_gx = x; start_gy = y; }
        }
    }
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
static int   bounce_cool;      // frames until the wall-bounce sound may fire again

// can the marble's edge enter (fx,fy)? walls always block; a walkable tile blocks
// when its surface is a ledge too tall to hop (> MCLIMB above the marble). void
// doesn't block — rolling onto it is how you fall off the course.
static bool hits_side(float fx, float fy) {
    int x = (int)fx, y = (int)fy;
    if (x < 0 || x >= GW || y < 0 || y >= GH) return false;
    int t = ttype[y][x];
    if (t == T_WALL) return true;
    if (t == T_VOID) return false;
    return ground_z(fx, fy) > mz + MCLIMB;
}

enum { ST_PLAY, ST_WIN, ST_OVER };
static int gstate;
static bool newbest;           // this win set a new best (and saved the ghost)

static int roll_slot = 5;

// ---- ghost: your best run, re-raced against you -------------------
// recorded as a position stream (6 bytes/frame), one sample per play frame.
#define G_MAX   1800                     // 30s at 60fps — the whole timer
#define G_MAGIC 0x4D42474C               // blob header — bump when the course changes
typedef struct { short x, y, z; } GPos;  // marble position, grid units x256
static GPos grec[G_MAX];  static int grec_n;   // this run, being recorded
static GPos gbest[G_MAX]; static int gbest_n;  // the best run, replaying
static int  gframe;                            // ghost playback cursor
static struct { int magic; GPos p[G_MAX]; } gblob;   // disk image: header + stream

// record this frame's position + advance the ghost. called once per play frame.
static void ghost_tick(void) {
    if (grec_n < G_MAX)
        grec[grec_n++] = (GPos){ (short)(mx * 256), (short)(my * 256), (short)(mz * 256) };
    gframe++;                  // past gbest_n = ghost finished and vanishes
}

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
    time_left = 30.0f;
    gstate = ST_PLAY;
    newbest = false;
    grec_n = 0; gframe = 0;    // fresh recording; ghost rolls from the start
    respawn();
}

void init(void) {
    // a rolling-rumble instrument: filtered saw, sustained
    instrument(roll_slot, INSTR_SAW, 8, 40, 5, 120);
    instrument_filter(roll_slot, FILTER_LOW, 700, 4);
    best_time = load_int("best2", 0);
    int n = load_bytes(&gblob, sizeof gblob);          // yesterday's ghost...
    if (n >= 4 && gblob.magic == G_MAGIC) {            // ...unless it ran an older course
        gbest_n = (n - 4) / (int)sizeof(GPos);
        for (int i = 0; i < gbest_n; i++) gbest[i] = gblob.p[i];
    }
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
        ghost_tick();          // the ghost saw your falls too
        if (fall_t > 0.55f) die();
        return;
    }

    // ---- input: tilt nudge, mapped to the iso axes ----
    // screen-up should move the marble up-and-back in the iso world: -gx,-gy.
    // in the air you barely steer — commit to the drop before you take it.
    bool air = mz > ground_z(mx, my) + 0.02f;
    const float ACC = 9.0f;
    float ix = 0, iy = 0;
    if (btn(0, BTN_UP))    { ix -= 1; iy -= 1; }
    if (btn(0, BTN_DOWN))  { ix += 1; iy += 1; }
    if (btn(0, BTN_LEFT))  { ix -= 1; iy += 1; }
    if (btn(0, BTN_RIGHT)) { ix += 1; iy -= 1; }
    float il = length((int)(ix*100), (int)(iy*100)) / 100.0f;
    if (il > 0.001f) { ix /= il; iy /= il; }
    vx += ix * ACC * (air ? 0.35f : 1.0f) * d;
    vy += iy * ACC * (air ? 0.35f : 1.0f) * d;

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

    // ---- integrate, with radius + ledge-aware wall collision (axis-separated) ----
    float nx = mx + vx * d;
    float ex = nx + (vx > 0 ? MR : -MR);                      // leading edge of the ball
    if (hits_side(ex, my - MR * 0.7f) || hits_side(ex, my + MR * 0.7f)) {
        if (fabsf(vx) > 1.0f && bounce_cool == 0) { note(55, INSTR_SQUARE, 2); bounce_cool = 6; }
        vx = -vx * 0.4f;
        nx = mx;
    }
    mx = nx;

    float ny = my + vy * d;
    float ey = ny + (vy > 0 ? MR : -MR);
    if (hits_side(mx - MR * 0.7f, ey) || hits_side(mx + MR * 0.7f, ey)) {
        if (fabsf(vy) > 1.0f && bounce_cool == 0) { note(55, INSTR_SQUARE, 2); bounce_cool = 6; }
        vy = -vy * 0.4f;
        ny = my;
    }
    my = ny;
    if (bounce_cool > 0) bounce_cool--;

    // ---- vertical: hug the ground uphill, fall off ledges with real airtime ----
    float gz = ground_z(mx, my);
    if (gz > -900.0f) {
        if (mz > gz + 0.02f) {              // airborne: gravity until touchdown
            vz -= 10.0f * d;
            mz += vz * d;
            if (mz <= gz) {
                if (vz < -3.0f) { hit(38, roll_slot, 4, 70); shake(1.2f); }  // landing thump
                mz = gz; vz = 0;
            }
        } else { mz = gz; vz = 0; }         // grounded: ride the surface (ramps, small steps)
    }

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
        newbest = (best_time == 0 || cs > best_time);
        if (newbest) {
            best_time = cs; save_int("best2", cs);
            gblob.magic = G_MAGIC;                          // this run IS the new ghost
            for (int i = 0; i < grec_n; i++) { gblob.p[i] = grec[i]; gbest[i] = grec[i]; }
            gbest_n = grec_n;
            save_bytes(&gblob, 4 + grec_n * (int)sizeof(GPos));
        }
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

    ghost_tick();

#ifdef DE_TRACE
    watch("pos", "%.2f %.2f z=%.2f vz=%.2f air=%d", mx, my, mz, vz, air);
    watch("ghost", "rec=%d play=%d/%d", grec_n, gframe, gbest_n);
#endif

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

// the ghost: a see-through (checkered) blue marble re-rolling the best run.
// gframe was already advanced this frame, so the current sample is gframe-1.
static void draw_ghost(void) {
    GPos g = gbest[gframe - 1];
    float gx = g.x / 256.0f, gy = g.y / 256.0f, gz = g.z / 256.0f;
    int sx, sy; iso(gx, gy, gz, &sx, &sy);
    sy -= 5;
    fillp(FILL_CHECKER, -1);              // every other pixel transparent
    circfill(sx, sy, 6, CLR_BLUE);
    circfill(sx - 2, sy - 2, 2, CLR_WHITE);
    fillp_reset();
    circ(sx, sy, 6, CLR_DARK_BLUE);
}

static bool ghost_alive(void) { return gstate == ST_PLAY && gframe > 0 && gframe <= gbest_n; }

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
        // draw ghost + marble in their diagonal bands so they overlap correctly
        if (ghost_alive() &&
            (int)(gbest[gframe - 1].x / 256.0f + gbest[gframe - 1].y / 256.0f) == s) draw_ghost();
        if ((int)(mx + my) == s) draw_marble();
    }
    if (ghost_alive() &&
        (int)(gbest[gframe - 1].x / 256.0f + gbest[gframe - 1].y / 256.0f) >= GW + GH) draw_ghost();
    if ((int)(mx + my) >= GW + GH) draw_marble();

    camera(0, 0);

    // ---- HUD ----
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    // lives as little marbles
    for (int i = 0; i < lives; i++) circfill(7 + i * 11, 5, 3, CLR_LIGHT_GREY);
    if (gbest_n > 0) print("vs GHOST", 42, 2, CLR_BLUE);
    // timer
    int col = time_left < 10 ? (blink(6) ? CLR_RED : CLR_ORANGE) : CLR_WHITE;
    print_centered(str("%05.2f", time_left), SCREEN_W/2, 2, col);
    if (best_time > 0)
        print_right(str("BEST %.1f", best_time / 100.0f), SCREEN_W - 3, 2, CLR_LIGHT_YELLOW);

    // ---- overlays ----
    if (gstate == ST_WIN) {
        fade(0.4f);
        rectfill(SCREEN_W/2-70, SCREEN_H/2-32, 140, 64, CLR_BLACK);
        rect    (SCREEN_W/2-70, SCREEN_H/2-32, 140, 64, CLR_YELLOW);
        print_centered("GOAL!", SCREEN_W/2, SCREEN_H/2-24, CLR_YELLOW);
        print_centered(str("TIME LEFT %.2f", time_left), SCREEN_W/2, SCREEN_H/2-10, CLR_WHITE);
        if (newbest)
            print_centered("NEW BEST - GHOST SAVED", SCREEN_W/2, SCREEN_H/2+4, CLR_BLUE);
        print_centered("Z to play again", SCREEN_W/2, SCREEN_H/2+18, CLR_LIGHT_GREY);
    } else if (gstate == ST_OVER) {
        fade(0.5f);
        rectfill(SCREEN_W/2-70, SCREEN_H/2-26, 140, 52, CLR_BLACK);
        rect    (SCREEN_W/2-70, SCREEN_H/2-26, 140, 52, CLR_RED);
        print_centered(time_left <= 0 ? "TIME UP" : "GAME OVER", SCREEN_W/2, SCREEN_H/2-18, CLR_RED);
        print_centered("the marble is lost", SCREEN_W/2, SCREEN_H/2-4, CLR_LIGHT_GREY);
        print_centered("Z to try again", SCREEN_W/2, SCREEN_H/2+12, CLR_WHITE);
    }
}
