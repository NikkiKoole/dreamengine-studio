#include "studio.h"
#include <math.h>

// FLYOVER — a third-person flight sim over a procedurally generated earth.
//
// Three engine idioms fused into one sky:
//   1. MODE 7 GROUND  — one perlin-noise world sampled once per scanline with a
//      perspective divide (the SNES floor trick from mode7.c): sea, beach, grass,
//      forest, rock, snow. This is the "earth made with noise + textures".
//   2. BILLBOARDS      — forests, mountains and whole cities of 3D-ish buildings
//      scattered across the world, each projected with the SAME mode-7 math and
//      drawn far-to-near (painter's order), so they rise out of the terrain and
//      shrink with distance. Clouds are billboards too, just lifted into the sky.
//   3. POLYGON PLANE   — a flat-shaded low-poly aircraft (solid3d.c technique:
//      rotate every corner, sort faces, fillp-dither the in-between shades) pinned
//      to the foreground. It banks when you turn and pitches when you climb/dive.
//
// Free-flight sandbox — just go look at the world.
//   Left/Right  bank & turn     Up/Down  throttle     Z  climb     X  dive

// ── world ────────────────────────────────────────────────────────────────────
#define TEX   256
#define MASK  255
#define HZ    78                 // horizon row on screen
#define BS    2                  // ground block size (chunky = faster + retro)
#define F     140.0f             // mode-7 focal length (ground + billboards share it)

static unsigned char world[TEX * TEX];   // baked terrain colour per cell

// ── props (trees / buildings / mountains / clouds) ─────────────────────────────
#define TREE     0
#define BUILDING 1
#define MOUNTAIN 2
#define CLOUD    3
#define MAXP   1500
static float px_[MAXP], py_[MAXP], ph[MAXP];   // world x, world y, height/size/altitude
static unsigned char ptype[MAXP], pv[MAXP];     // type, variant
static int nprop = 0;

// camera / flight state
static float cx, cy, ang, spd, H;       // world pos, heading, speed, altitude
static float bank, pitch;               // visual roll & pitch of the plane model

// layered noise → terrain height 0..1 (same field the ground is baked from)
static float terrain(float x, float y) {
    return 0.55f * noise2(x * 0.025f, y * 0.025f)
         + 0.30f * noise2(x * 0.060f, y * 0.060f)
         + 0.15f * noise2(x * 0.130f, y * 0.130f);
}

static void add_prop(float x, float y, int type, float h, int variant) {
    if (nprop >= MAXP) return;
    px_[nprop] = x; py_[nprop] = y; ptype[nprop] = type; ph[nprop] = h; pv[nprop] = variant;
    nprop++;
}

void init(void) {
    // 1. bake the earth: noise → terrain colour bands
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++) {
            // per-cell hash jitter dithers the band boundaries so the colour
            // thresholds stipple into each other instead of drawing hard contour lines
            unsigned int hh = (unsigned int)(x * 374761393u + y * 668265263u);
            hh = (hh ^ (hh >> 13)) * 1274126177u;
            float jitter = ((hh & 1023) / 1023.0f - 0.5f) * 0.05f;
            float h = terrain(x, y) + jitter;
            int c;
            if      (h < 0.30f) c = CLR_DARK_BLUE;     // deep sea
            else if (h < 0.42f) c = CLR_TRUE_BLUE;     // sea
            else if (h < 0.47f) c = CLR_BLUE;          // shallows
            else if (h < 0.50f) c = CLR_LIGHT_PEACH;   // beach
            else if (h < 0.60f) c = CLR_DARK_GREEN;    // grass
            else if (h < 0.72f) c = CLR_MEDIUM_GREEN;  // forest
            else if (h < 0.82f) c = CLR_BROWN;         // rock
            else if (h < 0.90f) c = CLR_DARK_GREY;     // crag
            else                c = CLR_WHITE;         // snow
            world[y * TEX + x] = c;
        }

    nprop = 0;

    // 2. forests — trees scattered through the forest band
    for (int i = 0; i < 900; i++) {
        float x = rnd(TEX), y = rnd(TEX);
        float h = terrain(x, y);
        if (h > 0.58f && h < 0.74f && chance(70))
            add_prop(x, y, TREE, rnd_float_between(1.4f, 2.6f), rnd(2));   // round or pine
    }

    // 3. mountains — big peaks where the terrain is rock/snow
    for (int i = 0; i < 400; i++) {
        float x = rnd(TEX), y = rnd(TEX);
        float h = terrain(x, y);
        if (h > 0.80f && chance(55))
            add_prop(x, y, MOUNTAIN, remap(h, 0.80f, 1.0f, 16, 42), h > 0.90f ? 1 : 0); // snowcap if high
    }

    // 4. cities — a handful of building clusters on habitable land
    int placed = 0;
    for (int tries = 0; tries < 4000 && placed < 6; tries++) {
        float cxx = rnd(TEX), cyy = rnd(TEX);
        float h = terrain(cxx, cyy);
        if (h < 0.50f || h > 0.62f) continue;                  // grass / coastal flats only
        placed++;
        int n = rnd_between(30, 60);
        for (int b = 0; b < n; b++) {
            // gaussian-ish cluster (sum of two uniforms), downtown = centre
            float ox = (rnd_float() + rnd_float() - 1.0f) * 16;
            float oy = (rnd_float() + rnd_float() - 1.0f) * 16;
            float bx = cxx + ox, by = cyy + oy;
            float bh = terrain(bx, by);
            if (bh < 0.48f) continue;                          // don't build in the sea
            float dist = length((int)ox, (int)oy);
            float height = remap(clamp(dist, 0, 18), 0, 18, 24, 7) * rnd_float_between(0.7f, 1.2f);
            add_prop(bx, by, BUILDING, clamp(height, 6, 28), rnd(3));
        }
    }

    // 5. clouds — billboards lifted high into the sky, drifting on the wind
    for (int i = 0; i < 50; i++)
        add_prop(rnd(TEX), rnd(TEX), CLOUD, rnd_float_between(24, 64),  // altitude
                 (int)rnd_float_between(5, 10));                         // puff size

    // flight start
    cx = 128; cy = 128; ang = 0; spd = 1.6f; H = 30; bank = 0; pitch = 0;

    // a soft engine drone
    instrument(5, INSTR_TRI, 120, 200, 5, 300);
    instrument_filter(5, FILTER_LOW, 600, 4);
}

// shortest wrapped delta (the world tiles every 256 units)
static float wrapd(float d) { while (d > 128) d -= TEX; while (d < -128) d += TEX; return d; }

void update(void) {
    float turn = 0;
    if (btn(0, BTN_LEFT)  || btn(1, BTN_LEFT))  { ang -= 1.9f; turn = -1; }
    if (btn(0, BTN_RIGHT) || btn(1, BTN_RIGHT)) { ang += 1.9f; turn =  1; }

    float climb = 0;
    if (btn(0, BTN_UP)   || btn(1, BTN_UP))   { H = clamp(H + 0.7f, 12, 70); climb = -1; } // steer nose up
    if (btn(0, BTN_DOWN) || btn(1, BTN_DOWN)) { H = clamp(H - 0.7f, 12, 70); climb =  1; } // steer nose down

    if (btn(0, BTN_A) || btn(1, BTN_A)) spd = clamp(spd + 0.05f, 0.5f, 3.6f);  // Z throttle up
    if (btn(0, BTN_B) || btn(1, BTN_B)) spd = clamp(spd - 0.05f, 0.5f, 3.6f);  // X throttle down

    // visual bank / pitch ease toward the input — a plane banks INTO the turn
    bank  = lerp(bank,  turn  * -30, 0.12f);
    pitch = lerp(pitch, climb *  16, 0.10f);

    // fly forward
    cx += cos_deg(ang) * spd;
    cy += sin_deg(ang) * spd;

    // drift the clouds on a steady wind
    for (int i = 0; i < nprop; i++) {
        if (ptype[i] != CLOUD) continue;
        px_[i] += 0.20f; py_[i] += 0.06f;
        if (px_[i] >= TEX) px_[i] -= TEX;
        if (py_[i] >= TEX) py_[i] -= TEX;
    }

    // engine drone — pitch rises with throttle
    if (frame() % 11 == 0) hit(36 + (int)(spd * 5), 5, 2, 240);
}

// ── billboard drawing — every prop rises from its ground point (sx,gy) and is
//    scaled by `sc` (= F/distance), so far things are small. ph[i] = world size. ──
static void draw_tree(int sx, int gy, float sc, float size, int variant) {
    float r = size * sc;
    if (r < 1) { pset(sx, gy, CLR_DARK_GREEN); return; }
    int tw = max(1, (int)(sc * 0.5f));
    int th = max(1, (int)(size * sc * 0.6f));
    rectfill(sx - tw / 2, gy - th, tw, th, CLR_BROWN);             // trunk
    int cy = gy - th;
    if (variant == 0) {                                            // round, leafy
        circfill(sx, cy - (int)r, (int)r, CLR_DARK_GREEN);
        circfill(sx - (int)(r * 0.3f), cy - (int)(r * 1.2f), (int)(r * 0.7f), CLR_MEDIUM_GREEN);
    } else {                                                       // pine
        trifill(sx, cy - (int)(r * 2.2f), sx - (int)r, cy, sx + (int)r, cy, CLR_DARK_GREEN);
        trifill(sx, cy - (int)(r * 1.4f), sx - (int)(r * 0.8f), cy - (int)(r * 0.4f),
                sx + (int)(r * 0.8f), cy - (int)(r * 0.4f), CLR_MEDIUM_GREEN);
    }
}

static void draw_mountain(int sx, int gy, float sc, float height, int snowy) {
    float sw = (10.0f + height * 0.25f) * sc;
    float sh = height * sc;
    int apexx = sx,            apexy = gy - (int)sh;
    int bl = sx - (int)(sw / 2), br = sx + (int)(sw / 2);
    if (sw < 2) { rectfill(sx, apexy, 1, (int)sh, CLR_DARK_GREY); return; }
    trifill(apexx, apexy, bl, gy, br, gy, CLR_MEDIUM_GREY);                 // lit rock
    trifill(apexx, apexy, sx, gy, br, gy, CLR_DARK_GREY);                   // shaded right face
    if (snowy) {                                                           // snow cap
        int capy = apexy + (int)(sh * 0.32f);
        int capw = (int)(sw * 0.20f);
        trifill(apexx, apexy, apexx - capw, capy, apexx + capw, capy, CLR_WHITE);
    }
}

static void draw_building(int sx, int gy, float sc, float height, int variant) {
    int front, side, top;
    if      (variant == 0) { front = CLR_LIGHT_GREY; side = CLR_DARK_GREY;  top = CLR_WHITE; }
    else if (variant == 1) { front = CLR_TRUE_BLUE;  side = CLR_DARK_BLUE;  top = CLR_BLUE;  }
    else                   { front = CLR_MEDIUM_GREY;side = CLR_DARK_BROWN; top = CLR_LIGHT_PEACH; }

    float sw = 4.5f * sc;
    float sh = height * sc;
    if (sw < 1.2f) { rectfill(sx, gy - (int)sh, 1, (int)sh, side); return; }
    int x0 = sx - (int)(sw / 2);
    int w  = max(1, (int)sw);
    int y0 = gy - (int)sh;
    int hh = max(1, (int)sh);
    int ox = (int)(sw * 0.42f), oy = -(int)(sw * 0.30f);          // iso depth offset (up-right)

    // top face
    trifill(x0, y0, x0 + w, y0, x0 + w + ox, y0 + oy, top);
    trifill(x0, y0, x0 + w + ox, y0 + oy, x0 + ox, y0 + oy, top);
    // right side face
    trifill(x0 + w, y0, x0 + w, gy, x0 + w + ox, gy + oy, side);
    trifill(x0 + w, y0, x0 + w + ox, gy + oy, x0 + w + ox, y0 + oy, side);
    // front face (nearest → drawn on top)
    rectfill(x0, y0, w, hh, front);

    // windows — only when the building is big enough on screen to read
    if (w >= 12 && hh >= 14) {
        unsigned int seed = (unsigned int)(sx * 73 + gy * 31 + w * 17);
        for (int wy = y0 + 4; wy < gy - 4; wy += 6)
            for (int wx = x0 + 3; wx < x0 + w - 3; wx += 5) {
                seed = seed * 1103515245u + 12345u;
                int lit = (seed >> 16) & 7;
                rectfill(wx, wy, 2, 3, lit == 0 ? CLR_YELLOW : (variant == 1 ? CLR_DARKER_BLUE : CLR_DARK_GREY));
            }
    }
}

static void draw_cloud(int sx, int cloudy, float sc, float size) {
    float s = clamp(size * sc, 2, 52);
    ovalfill(sx, cloudy + (int)(s * 0.15f), (int)s, (int)(s * 0.55f), CLR_LIGHT_GREY); // soft underside
    ovalfill(sx - (int)(s * 0.55f), cloudy, (int)(s * 0.6f), (int)(s * 0.45f), CLR_WHITE);
    ovalfill(sx + (int)(s * 0.55f), cloudy, (int)(s * 0.6f), (int)(s * 0.45f), CLR_WHITE);
    ovalfill(sx, cloudy - (int)(s * 0.30f), (int)(s * 0.75f), (int)(s * 0.5f), CLR_WHITE);
}

// ── the plane: a low-poly model, flat-shaded (solid3d technique) ────────────────
typedef struct { float x, y, z; } V3;
static V3 PV[] = {
    /*0 N */{0,0,3.4f},   /*1 T */{0,0.15f,-2.4f}, /*2 U */{0,0.5f,0.3f}, /*3 D */{0,-0.4f,0.3f},
    /*4 L */{-0.5f,0.05f,0.3f}, /*5 R */{0.5f,0.05f,0.3f},
    /*6 */{-0.45f,0.05f,0.7f}, /*7 */{-0.45f,0.05f,-1.1f}, /*8 */{-3.3f,0.2f,-0.3f}, /*9 */{-3.3f,0.2f,-0.9f},   // L wing
    /*10*/{0.45f,0.05f,0.7f},  /*11*/{0.45f,0.05f,-1.1f},  /*12*/{3.3f,0.2f,-0.3f},  /*13*/{3.3f,0.2f,-0.9f},    // R wing
    /*14*/{0,0.0f,-1.7f}, /*15*/{0,0.15f,-2.4f}, /*16*/{0,-1.1f,-2.4f},                                          // tail fin
    /*17*/{-1.3f,0.05f,-2.2f}, /*18*/{1.3f,0.05f,-2.2f}, /*19*/{0,0.1f,-1.8f}, /*20*/{0,0.1f,-2.4f},             // tailplane
    /*21*/{-0.25f,0.5f,1.2f}, /*22*/{0.25f,0.5f,1.2f}, /*23*/{0.25f,0.55f,0.2f}, /*24*/{-0.25f,0.55f,0.2f},      // canopy
};
// each face: 3 vertex indices + bright colour + dark colour
static int PF[][5] = {
    {0,2,4, CLR_LIGHT_PEACH, CLR_BROWN}, {0,5,2, CLR_LIGHT_PEACH, CLR_BROWN},     // nose top
    {0,3,5, CLR_LIGHT_GREY,  CLR_DARK_GREY}, {0,4,3, CLR_LIGHT_GREY, CLR_DARK_GREY}, // nose bottom
    {1,2,5, CLR_LIGHT_GREY,  CLR_DARK_GREY}, {1,4,2, CLR_LIGHT_GREY, CLR_DARK_GREY}, // body back top
    {1,5,3, CLR_DARK_GREY,   CLR_DARKER_GREY}, {1,3,4, CLR_DARK_GREY, CLR_DARKER_GREY}, // body back bottom
    {6,8,9, CLR_RED, CLR_DARK_RED}, {6,9,7, CLR_RED, CLR_DARK_RED},               // left wing
    {10,13,12, CLR_RED, CLR_DARK_RED}, {10,11,13, CLR_RED, CLR_DARK_RED},         // right wing
    {14,16,15, CLR_RED, CLR_DARK_RED},                                            // tail fin
    {19,17,20, CLR_RED, CLR_DARK_RED}, {20,18,19, CLR_RED, CLR_DARK_RED},         // tailplane
    {21,22,23, CLR_DARK_BLUE, CLR_DARKER_BLUE}, {21,23,24, CLR_DARK_BLUE, CLR_DARKER_BLUE}, // canopy
};
#define NPF (int)(sizeof(PF) / sizeof(PF[0]))
#define FP_  8.0f
#define PS_  27.0f

static V3 plane_rot(V3 p, float roll, float ptch) {
    // roll about forward (z), then pitch about x (folds in the view tilt)
    float x =  p.x * cos_deg(roll) - p.y * sin_deg(roll);
    float y =  p.x * sin_deg(roll) + p.y * cos_deg(roll);
    float z =  p.z;
    float y2 =  y * cos_deg(ptch) - z * sin_deg(ptch);
    float z2 =  y * sin_deg(ptch) + z * cos_deg(ptch);
    return (V3){ x, y2, z2 };
}

static void draw_plane(void) {
    float ptch = -22 + pitch;         // tilt so we see the plane's TOP, nose up toward the horizon
    float bob  = sin_deg(now() * 90) * 1.5f;
    float pcx = SCREEN_W / 2.0f, pcy = 126 + bob;

    static int sx[32], sy[32]; static V3 rv[32];
    int nv = (int)(sizeof(PV) / sizeof(PV[0]));
    for (int i = 0; i < nv; i++) {
        rv[i] = plane_rot(PV[i], bank, ptch);
        float f = FP_ / (FP_ + rv[i].z);
        sx[i] = (int)(pcx + rv[i].x * f * PS_);
        sy[i] = (int)(pcy - rv[i].y * f * PS_);
    }

    // sort faces far → near (no z-buffer)
    typedef struct { int f; float z; } FaceZ;
    FaceZ fz[NPF];
    for (int i = 0; i < NPF; i++) {
        int *F_ = PF[i];
        fz[i].f = i;
        fz[i].z = (rv[F_[0]].z + rv[F_[1]].z + rv[F_[2]].z) / 3;
    }
    for (int i = 1; i < NPF; i++)
        for (int j = i; j > 0 && fz[j].z > fz[j - 1].z; j--) {
            FaceZ t = fz[j]; fz[j] = fz[j - 1]; fz[j - 1] = t;
        }

    float lx = -0.5f, ly = 0.7f, lz = -0.5f;     // light from upper-left-front
    for (int i = 0; i < NPF; i++) {
        int *F_ = PF[fz[i].f];
        int a = F_[0], b = F_[1], c = F_[2];
        V3 ra = rv[a], rb = rv[b], rc = rv[c];
        float ux = rb.x-ra.x, uy = rb.y-ra.y, uz = rb.z-ra.z;
        float vx = rc.x-ra.x, vy = rc.y-ra.y, vz = rc.z-ra.z;
        float nx = uy*vz-uz*vy, ny = uz*vx-ux*vz, nz = ux*vy-uy*vx;
        float len = sqrtf(nx*nx+ny*ny+nz*nz); if (len < 0.0001f) len = 1;
        float lam = fabsf((nx*lx+ny*ly+nz*lz) / len);
        int bright = F_[3], dark = F_[4];
        if (lam > 0.66f) {
            trifill(sx[a],sy[a], sx[b],sy[b], sx[c],sy[c], bright);
        } else if (lam > 0.33f) {
            fillp(FILL_CHECKER, dark);
            trifill(sx[a],sy[a], sx[b],sy[b], sx[c],sy[c], bright);
            fillp_reset();
        } else {
            trifill(sx[a],sy[a], sx[b],sy[b], sx[c],sy[c], dark);
        }
    }
}

void draw(void) {
    // horizon drops as you climb — at altitude you look down, so the far (shimmery)
    // terrain band shrinks toward a lower horizon line.
    int hz = (int)clamp(HZ + (H - 12) * 0.75f, HZ, 132);

    // --- sky ---
    rectfill(0, 0,  SCREEN_W, 28,      CLR_DARKER_BLUE);
    rectfill(0, 28, SCREEN_W, 24,      CLR_DARK_BLUE);
    rectfill(0, 52, SCREEN_W, 14,      CLR_TRUE_BLUE);
    rectfill(0, 66, SCREEN_W, hz - 66, CLR_BLUE);
    circfill(SCREEN_W - 54, 26, 12, CLR_LIGHT_YELLOW);
    circfill(SCREEN_W - 54, 26, 16, CLR_LIGHT_YELLOW);   // soft glow (overdraw)

    // --- MODE 7 GROUND: one perspective-correct texture sample per scanline ---
    float ca = cos_deg(ang), sa = sin_deg(ang);
    float cxs = SCREEN_W / 2.0f;
    for (int sy = hz; sy < SCREEN_H; sy += BS) {
        int   dy = sy - hz + 1;
        float z  = (H * F) / dy;
        if (z > 1700) z = 1700;
        float stepx = -sa * (z / F) * BS;
        float stepy =  ca * (z / F) * BS;
        float lat0  = (0 - cxs) / F * z;
        float wx = cx + ca * z + (-sa) * lat0;
        float wy = cy + sa * z + ( ca) * lat0;
        for (int sx = 0; sx < SCREEN_W; sx += BS) {
            rectfill(sx, sy, BS, BS, world[(((int)wy) & MASK) * TEX + (((int)wx) & MASK)]);
            wx += stepx; wy += stepy;
        }
    }
    // atmospheric haze over the farthest, shimmery rows just below the horizon
    fillp(FILL_CHECKER, -1);
    rectfill(0, hz,     SCREEN_W, 4, CLR_BLUE);
    rectfill(0, hz + 4, SCREEN_W, 4, CLR_TRUE_BLUE);
    fillp_reset();

    // --- BILLBOARDS: project every prop with the mode-7 math, collect, sort, draw ---
    typedef struct { float d; int sx, gy; float sc; int idx; } Vis;
    static Vis vis[MAXP]; int nv = 0;
    for (int i = 0; i < nprop; i++) {
        float dxx = wrapd(px_[i] - cx), dyy = wrapd(py_[i] - cy);
        float fwd = dxx * ca + dyy * sa;
        int type = ptype[i];
        float md = type == TREE ? 70 : type == BUILDING ? 125 : type == MOUNTAIN ? 140 : 150;
        float nd = type == CLOUD ? 42 : 4;
        if (fwd < nd || fwd > md) continue;
        float sc = F / fwd;
        int sx = (int)(cxs + (dxx * (-sa) + dyy * ca) * sc);
        if (sx < -90 || sx > SCREEN_W + 90) continue;
        vis[nv].d = fwd; vis[nv].sx = sx; vis[nv].sc = sc;
        vis[nv].gy = (int)(hz + H * sc); vis[nv].idx = i; nv++;
    }
    // painter's sort: far first
    for (int i = 1; i < nv; i++)
        for (int j = i; j > 0 && vis[j].d > vis[j - 1].d; j--) {
            Vis t = vis[j]; vis[j] = vis[j - 1]; vis[j - 1] = t;
        }
    for (int k = 0; k < nv; k++) {
        int i = vis[k].idx;
        switch (ptype[i]) {
            case TREE:     draw_tree(vis[k].sx, vis[k].gy, vis[k].sc, ph[i], pv[i]); break;
            case MOUNTAIN: draw_mountain(vis[k].sx, vis[k].gy, vis[k].sc, ph[i], pv[i]); break;
            case BUILDING: draw_building(vis[k].sx, vis[k].gy, vis[k].sc, ph[i], pv[i]); break;
            case CLOUD: {
                int cloudy = vis[k].gy - (int)(ph[i] * vis[k].sc);
                draw_cloud(vis[k].sx, cloudy, vis[k].sc, pv[i]);
                break;
            }
        }
    }

    // --- plane shadow on the ground (dithered ellipse, fades & shrinks with altitude) ---
    float shs = clamp(remap(H, 12, 70, 16, 4), 4, 16);
    int   shy = SCREEN_H - 14 - (int)((H - 12) * 0.35f);
    fillp(FILL_CHECKER, -1);
    ovalfill(SCREEN_W / 2 + (int)(bank * 0.4f), shy, (int)shs, (int)(shs * 0.4f), CLR_BLACK);
    fillp_reset();

    // --- the plane ---
    draw_plane();

    // --- HUD ---
    print(str("ALT %d", (int)H), 6, 6, CLR_WHITE);
    print(str("SPD %d%%", (int)(spd / 3.6f * 100)), 6, 16, CLR_LIGHT_GREY);
    int hdg = ((int)ang % 360 + 360) % 360;
    const char *dir[] = { "E", "SE", "S", "SW", "W", "NW", "N", "NE" };
    print_right(str("HDG %03d %s", hdg, dir[((hdg + 22) / 45) % 8]), SCREEN_W - 6, 6, CLR_WHITE);

    print_centered("L/R turn   UP/DN climb-dive   Z faster   X slower", SCREEN_H - 9, CLR_DARK_GREY);
}

