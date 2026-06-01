#include "studio.h"
#include <math.h>
#include <stddef.h>

// INFINIMINER (lo-fi) — the proto-Minecraft idea, tiny.
// A 16x16x6 voxel world you walk through first-person. Mouse-look, a crosshair
// targets the block face you face, left-click mines, right-click places. Four
// block types on tritex-textured cube faces.
//
//   WASD move   mouse look   L-click mine   R-click place   1-4 block   R regenerate

// ------------------------------------------------------------ world
#define WX 16
#define WY 6          // height
#define WZ 16
#define EYE 1.6f      // eye height in blocks

// block ids: 0 = air, then materials. material i textures from sprite slot i-1.
#define AIR   0
#define GRASS 1
#define DIRT  2
#define STONE 3
#define WOOD  4

static unsigned char grid[WX][WY][WZ];

static float px = 8.0f, py = 4.5f, pz = 2.0f;   // camera position (blocks)
static float yaw = 90.0f, pitch = 8.0f;          // degrees
static int   held = GRASS;
static int   last_mx = -1, last_my = -1;
static int   sub = 2;                            // face subdivision

// targeting result (set each frame)
static bool  have_target = false;
static int   tgx, tgy, tgz;        // targeted solid cell
static int   pgx, pgy, pgz;        // empty cell on the near side (place here)

#define FOCAL 1.6f
#define TEX 16
// V3 is an engine type now (studio.h)

static int block_at(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= WX || y >= WY || z >= WZ) return AIR;
    return grid[x][y][z];
}

// ------------------------------------------------------------ worldgen
static void regen(void) {
    for (int x = 0; x < WX; x++)
      for (int z = 0; z < WZ; z++) {
        // ground height 1..3 from smooth noise
        float n = noise2(x * 0.28f, z * 0.28f);
        int h = 1 + (int)(n * 2.99f);          // 1..3
        for (int y = 0; y < WY; y++) {
            int b = AIR;
            if (y == 0)          b = STONE;     // bedrock floor
            else if (y < h)      b = DIRT;
            else if (y == h)     b = GRASS;
            grid[x][y][z] = (unsigned char)b;
        }
      }
    // a little hill in one corner
    for (int x = 2; x <= 5; x++)
      for (int z = 2; z <= 5; z++) {
        int peak = 4 - (abs(x - 3) + abs(z - 3));
        for (int y = 1; y <= peak && y < WY; y++)
            grid[x][y][z] = (y == peak) ? GRASS : DIRT;
      }
    // two trees: wood trunk + a leafy cap (reuse grass as leaves for the lo-fi look)
    int tx[2] = { 11, 8 }, tz[2] = { 11, 4 };
    for (int t = 0; t < 2; t++) {
        int bx = tx[t], bz = tz[t];
        // find ground top
        int gy = 1; while (gy < WY && block_at(bx, gy, bz) != AIR) gy++;
        int trunk = gy + 2; if (trunk >= WY) trunk = WY - 1;
        for (int y = gy; y <= trunk; y++) grid[bx][y][bz] = WOOD;
        // canopy ring
        for (int dx = -1; dx <= 1; dx++)
          for (int dz = -1; dz <= 1; dz++) {
            int cy = trunk; if (cy < WY) {
                int cx = bx + dx, cz = bz + dz;
                if (block_at(cx, cy, cz) == AIR && !(dx == 0 && dz == 0))
                    grid[cx][cy][cz] = GRASS;
            }
          }
        if (trunk + 1 < WY) grid[bx][trunk + 1][bz] = GRASS;
    }
}

// instrument for dig/place
static void setup_audio(void) {
    instrument(5, INSTR_SQUARE, 2, 90, 2, 80);
    instrument_duty(5, 0.35f);
    instrument_filter(5, FILTER_LOW, 1100, 6);
}

void init(void) {
    regen();
    setup_audio();
}

// ------------------------------------------------------------ camera basis
// forward / right unit vectors from yaw+pitch. world: x east, y up, z south.
static V3 fwd(void) {
    float cp = cos_deg(pitch);
    return (V3){ cos_deg(yaw) * cp, sin_deg(pitch), sin_deg(yaw) * cp };
}
static V3 rightv(void) {
    return (V3){ cos_deg(yaw + 90.0f), 0, sin_deg(yaw + 90.0f) };
}

// transform a world point into camera space (camera looks down +z_cam)
static V3 to_cam(V3 w) {
    V3 f = fwd(), r = rightv();
    // up = f x r  (gives a proper right-handed-ish basis for screen)
    V3 u = { f.y * r.z - f.z * r.y, f.z * r.x - f.x * r.z, f.x * r.y - f.y * r.x };
    float dxp = w.x - px, dyp = w.y - py, dzp = w.z - pz;
    return (V3){
        dxp * r.x + dyp * r.y + dzp * r.z,   // right
        dxp * u.x + dyp * u.y + dzp * u.z,   // up
        dxp * f.x + dyp * f.y + dzp * f.z    // depth (forward)
    };
}

static int PXs(V3 c, float scale) { return (int)(SCREEN_W / 2 + c.x / c.z * scale); }
static int PYs(V3 c, float scale) { return (int)(SCREEN_H / 2 - c.y / c.z * scale); }
#define VIEWSCALE (FOCAL * SCREEN_W * 0.5f)

// ------------------------------------------------------------ raycast (DDA)
static void cast(void) {
    have_target = false;
    V3 d = fwd();
    float ox = px, oy = py, oz = pz;
    int x = (int)floorf(ox), y = (int)floorf(oy), z = (int)floorf(oz);
    int stepx = d.x > 0 ? 1 : -1, stepy = d.y > 0 ? 1 : -1, stepz = d.z > 0 ? 1 : -1;
    float inf = 1e9f;
    float tdx = d.x != 0 ? fabsf(1.0f / d.x) : inf;
    float tdy = d.y != 0 ? fabsf(1.0f / d.y) : inf;
    float tdz = d.z != 0 ? fabsf(1.0f / d.z) : inf;
    float tmx = d.x != 0 ? (((d.x > 0 ? (x + 1) : x) - ox) / d.x) : inf;
    float tmy = d.y != 0 ? (((d.y > 0 ? (y + 1) : y) - oy) / d.y) : inf;
    float tmz = d.z != 0 ? (((d.z > 0 ? (z + 1) : z) - oz) / d.z) : inf;
    int px_=x, py_=y, pz_=z;   // previous (air) cell
    for (int i = 0; i < 40; i++) {
        px_ = x; py_ = y; pz_ = z;
        if (tmx < tmy && tmx < tmz)      { x += stepx; tmx += tdx; }
        else if (tmy < tmz)              { y += stepy; tmy += tdy; }
        else                             { z += stepz; tmz += tdz; }
        if (x < 0 || y < 0 || z < 0 || x >= WX || y >= WY || z >= WZ) return;
        if (block_at(x, y, z) != AIR) {
            have_target = true;
            tgx = x; tgy = y; tgz = z;
            pgx = px_; pgy = py_; pgz = pz_;
            return;
        }
    }
}

// ------------------------------------------------------------ update
void update(void) {
    // mouse-look: integrate pointer deltas (pointer is clamped to canvas)
    int mx = mouse_x(), my = mouse_y();
    if (last_mx >= 0) {
        yaw   += (mx - last_mx) * 0.45f;
        pitch -= (my - last_my) * 0.45f;
        pitch  = clamp(pitch, -85.0f, 85.0f);
    }
    last_mx = mx; last_my = my;

    // movement on the ground plane, relative to yaw
    float spd = 4.0f * dt();
    float fx = cos_deg(yaw), fz = sin_deg(yaw);
    float rx = cos_deg(yaw + 90.0f), rz = sin_deg(yaw + 90.0f);
    float mvx = 0, mvz = 0;
    if (key('W')) { mvx += fx; mvz += fz; }
    if (key('S')) { mvx -= fx; mvz -= fz; }
    if (key('D')) { mvx += rx; mvz += rz; }
    if (key('A')) { mvx -= rx; mvz -= rz; }
    float ml = fsqrt(mvx * mvx + mvz * mvz);
    if (ml > 0.001f) { mvx /= ml; mvz /= ml; }
    float nx = px + mvx * spd, nz = pz + mvz * spd;
    // keep inside world bounds with a little margin
    px = clamp(nx, 0.3f, WX - 0.3f);
    pz = clamp(nz, 0.3f, WZ - 0.3f);

    // block select
    if (keyp('1')) held = GRASS;
    if (keyp('2')) held = DIRT;
    if (keyp('3')) held = STONE;
    if (keyp('4')) held = WOOD;
    if (keyp('R')) regen();

    cast();

    if (mouse_pressed(MOUSE_LEFT) && have_target && tgy > 0) {     // bedrock row 0 stays
        int m = grid[tgx][tgy][tgz];
        grid[tgx][tgy][tgz] = AIR;
        note(48 + m * 3, 5, 5);          // pitched per material
        shake(1.2f);
    }
    if (mouse_pressed(MOUSE_RIGHT) && have_target) {
        if (block_at(pgx, pgy, pgz) == AIR &&
            pgx >= 0 && pgy >= 0 && pgz >= 0 && pgx < WX && pgy < WY && pgz < WZ) {
            // don't place inside the player's own cell
            int cpx = (int)floorf(px), cpy = (int)floorf(py), cpz = (int)floorf(pz);
            if (!(pgx == cpx && pgy == cpy && pgz == cpz)) {
                grid[pgx][pgy][pgz] = (unsigned char)held;
                note(60 + held * 2, 5, 4);
            }
        }
    }
}

// ------------------------------------------------------------ rendering
// each cube: 8 corners, 6 quads (TL,TR,BR,BL) + outward normal direction.
static const int CQ[6][4] = {
    {4,5,6,7},  // +z (south, front when looking -z)
    {1,0,3,2},  // -z (north)
    {5,1,2,6},  // +x (east)
    {0,4,7,3},  // -x (west)
    {3,7,6,2},  // +y (top)
    {0,1,5,4},  // -y (bottom)
};
// neighbour offset per face — to test if that face is exposed (borders air)
static const int FN[6][3] = {
    { 0, 0, 1}, { 0, 0,-1}, { 1, 0, 0}, {-1, 0, 0}, { 0, 1, 0}, { 0,-1, 0}
};

// a visible cube to draw, with its camera-space center depth for sorting
typedef struct { int x, y, z, dist2; } Cube;
#define MAXCUBES 700
static Cube cubes[MAXCUBES];

static V3 quadpt(V3 A, V3 B, V3 C, V3 D, float s, float t) {
    V3 top = { A.x + (B.x - A.x) * s, A.y + (B.y - A.y) * s, A.z + (B.z - A.z) * s };
    V3 bot = { D.x + (C.x - D.x) * s, D.y + (C.y - D.y) * s, D.z + (C.z - D.z) * s };
    return (V3){ top.x + (bot.x - top.x) * t, top.y + (bot.y - top.y) * t,
                 top.z + (bot.z - top.z) * t };
}

#define QUARTER 0x7BDE

static void draw_cube(int gx, int gy, int gz, bool highlight) {
    int m = grid[gx][gy][gz];
    if (m == AIR) return;
    int slot = m - 1;                    // material -> sprite slot
    // 8 world corners
    V3 wc[8];
    float bx = gx, by = gy, bz = gz;
    wc[0] = (V3){bx,   by,   bz  };
    wc[1] = (V3){bx+1, by,   bz  };
    wc[2] = (V3){bx+1, by+1, bz  };
    wc[3] = (V3){bx,   by+1, bz  };
    wc[4] = (V3){bx,   by,   bz+1};
    wc[5] = (V3){bx+1, by,   bz+1};
    wc[6] = (V3){bx+1, by+1, bz+1};
    wc[7] = (V3){bx,   by+1, bz+1};
    V3 cc[8];
    for (int i = 0; i < 8; i++) cc[i] = to_cam(wc[i]);

    for (int fi = 0; fi < 6; fi++) {
        // only draw faces that border air (hidden-face cull)
        if (block_at(gx + FN[fi][0], gy + FN[fi][1], gz + FN[fi][2]) != AIR) continue;
        const int *Q = CQ[fi];
        V3 A = cc[Q[0]], B = cc[Q[1]], C = cc[Q[2]], D = cc[Q[3]];
        // near-plane reject: skip if any corner is behind/at the camera
        if (A.z < 0.05f || B.z < 0.05f || C.z < 0.05f || D.z < 0.05f) continue;
        // back-face cull in screen space (signed area)
        int ax = PXs(A, VIEWSCALE), ay = PYs(A, VIEWSCALE);
        int bx2 = PXs(B, VIEWSCALE), by2 = PYs(B, VIEWSCALE);
        int cx2 = PXs(C, VIEWSCALE), cy2 = PYs(C, VIEWSCALE);
        long area = (long)(bx2 - ax) * (cy2 - ay) - (long)(by2 - ay) * (cx2 - ax);
        if (area >= 0) continue;            // facing away
        float uo = slot * TEX;
        for (int sy = 0; sy < sub; sy++)
          for (int sx = 0; sx < sub; sx++) {
            float s0 = (float)sx / sub, t0 = (float)sy / sub;
            float s1 = (float)(sx + 1) / sub, t1 = (float)(sy + 1) / sub;
            V3 p00 = quadpt(A,B,C,D,s0,t0), p10 = quadpt(A,B,C,D,s1,t0);
            V3 p11 = quadpt(A,B,C,D,s1,t1), p01 = quadpt(A,B,C,D,s0,t1);
            int X00=PXs(p00,VIEWSCALE),Y00=PYs(p00,VIEWSCALE);
            int X10=PXs(p10,VIEWSCALE),Y10=PYs(p10,VIEWSCALE);
            int X11=PXs(p11,VIEWSCALE),Y11=PYs(p11,VIEWSCALE);
            int X01=PXs(p01,VIEWSCALE),Y01=PYs(p01,VIEWSCALE);
            float u0=uo+s0*TEX, u1=uo+s1*TEX, v0=t0*TEX, v1=t1*TEX;
            tritex(X00,Y00,u0,v0, X10,Y10,u1,v0, X11,Y11,u1,v1);
            tritex(X00,Y00,u0,v0, X11,Y11,u1,v1, X01,Y01,u0,v1);
          }
        // dither shade by face direction (top brightest, sides medium, bottom dark)
        int pat = 0;
        if (fi == 4)      pat = 0;          // top: full bright
        else if (fi == 5) pat = FILL_CHECKER;
        else if (fi == 0 || fi == 1) pat = QUARTER;   // front/back
        else              pat = QUARTER;
        if (pat) {
            fillp(pat, -1);
            trifill(ax,ay, bx2,by2, cx2,cy2, CLR_BLACK);
            trifill(ax,ay, cx2,cy2, PXs(D,VIEWSCALE),PYs(D,VIEWSCALE), CLR_BLACK);
            fillp_reset();
        }
        // highlight the targeted face's outline
        if (highlight) {
            int dx2 = PXs(D,VIEWSCALE), dy2 = PYs(D,VIEWSCALE);
            int hl = blink(8) ? CLR_WHITE : CLR_YELLOW;
            line(ax,ay, bx2,by2, hl); line(bx2,by2, cx2,cy2, hl);
            line(cx2,cy2, dx2,dy2, hl); line(dx2,dy2, ax,ay, hl);
        }
    }
}

void draw(void) {
    // sky gradient + ground band
    cls(CLR_BLUE);
    rectfill(0, 0, SCREEN_W, SCREEN_H / 2 - 4, CLR_TRUE_BLUE);
    fillp(FILL_CHECKER, CLR_TRUE_BLUE);
    rectfill(0, SCREEN_H / 2 - 4, SCREEN_W, 6, CLR_BLUE);
    fillp_reset();

    // gather visible solid cubes (with at least one exposed face), in front of camera
    int n = 0;
    for (int x = 0; x < WX; x++)
      for (int y = 0; y < WY; y++)
        for (int z = 0; z < WZ; z++) {
            if (grid[x][y][z] == AIR) continue;
            // skip fully-buried cubes (all 6 neighbours solid)
            bool exposed = false;
            for (int fi = 0; fi < 6 && !exposed; fi++)
                if (block_at(x+FN[fi][0], y+FN[fi][1], z+FN[fi][2]) == AIR) exposed = true;
            if (!exposed) continue;
            float ccx = x + 0.5f - px, ccy = y + 0.5f - py, ccz = z + 0.5f - pz;
            // forward dot to drop most cubes behind the camera
            V3 f = fwd();
            if (ccx*f.x + ccy*f.y + ccz*f.z < -1.0f) continue;
            int d2 = (int)((ccx*ccx + ccy*ccy + ccz*ccz) * 16);
            if (d2 > 16 * 16 * 18) continue;      // draw distance ~18 blocks
            if (n < MAXCUBES) {
                cubes[n].x = x; cubes[n].y = y; cubes[n].z = z; cubes[n].dist2 = d2; n++;
            }
        }
    // painter's sort: far first (insertion sort, n is small-ish)
    for (int i = 1; i < n; i++) {
        Cube t = cubes[i]; int j = i - 1;
        while (j >= 0 && cubes[j].dist2 < t.dist2) { cubes[j+1] = cubes[j]; j--; }
        cubes[j+1] = t;
    }
    for (int i = 0; i < n; i++) {
        bool hl = have_target && cubes[i].x == tgx && cubes[i].y == tgy && cubes[i].z == tgz;
        draw_cube(cubes[i].x, cubes[i].y, cubes[i].z, hl);
    }

    // crosshair
    int cx = SCREEN_W / 2, cy = SCREEN_H / 2;
    line(cx - 4, cy, cx + 4, cy, CLR_WHITE);
    line(cx, cy - 4, cx, cy + 4, CLR_WHITE);
    pset(cx, cy, CLR_BLACK);

    // HUD: held block swatch + label
    const char *names[5] = { "", "GRASS", "DIRT", "STONE", "WOOD" };
    int hudc[5] = { 0, CLR_GREEN, CLR_BROWN, CLR_LIGHT_GREY, CLR_DARK_BROWN };
    rectfill(4, SCREEN_H - 22, 96, 18, CLR_DARKER_BLUE);
    rect(4, SCREEN_H - 22, 96, 18, CLR_WHITE);
    spr(held - 1, 6, SCREEN_H - 22);                 // textured tile preview
    rect(6, SCREEN_H - 22, 16, 16, hudc[held]);
    print(names[held], 26, SCREEN_H - 16, CLR_WHITE);

    print("INFINIMINER", 4, 4, CLR_WHITE);
    print("WASD move  mouse look  LMB mine  RMB place  1-4 block  R regen",
          4, 13, CLR_LIGHT_GREY);
}
