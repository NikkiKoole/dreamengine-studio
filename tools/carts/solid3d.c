/* de:meta
{
  "title": "solid 3D",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "software-rasterizer",
    "dithering-gradient"
  ],
  "lineage": "Original PS1-era renderer demo; novel: fake in-between shades are synthesized by checker-pattern fillp between adjacent palette colours, interleaved with solid anchors in a shade ramp.",
  "description": "PS1-era flat-shaded polygons — no textures, no GPU 3D. The wireframe filled in: every face is one trifill, sorted far-to-near (painter's algorithm, since there's no z-buffer) and culled when it faces away. Brightness comes from the face normal dotted with a light direction; the in-between shades are faked with fillp dither between two palette colours — which is exactly how you squeeze more than 32 shades out of the palette. Z toggles the dither off so you can watch the smooth gradient collapse into hard bands. X swaps icosahedron/cube, up/down zoom, L/R spin."
}
de:meta */
#include "studio.h"
#include <math.h>

// SOLID 3D — flat-shaded, dithered, PS1-era polygons. No textures, no GPU 3D:
// every face is one `trifill`, and the SHADING comes from `fillp` dither.
//
// The whole pipeline, per frame:
//   1. ROTATE  every corner point around X and Y (sin/cos).
//   2. NORMAL  for each triangle: cross product of two edges → which way the
//              face points. Flip it outward (away from the model centre).
//   3. CULL    faces whose normal points away from the camera (normal.z >= 0).
//   4. LIGHT   brightness = how aligned the normal is with the light direction.
//   5. SORT    draw far faces first (painter's algorithm) — there's no z-buffer.
//   6. FILL    pick a shade from the ramp and `trifill` the triangle.
//
// The trick that makes it look like more than 32 colours: between two palette
// colours we `fillp(FILL_CHECKER, lighter)` so the triangle is half one colour,
// half the next — a fake in-between shade. Press Z to toggle it off and watch
// the smooth gradient collapse into hard colour bands. THAT is what patterned
// triangles buy you.
//
//   Z  dither on/off     X  next model     up/down  zoom     L/R  spin
//
// (V3, rot3, project and zsort are engine helpers now — see studio.h.)

// ── models: a list of vertices + a list of triangles (vertex indices) ────────
static V3 icoV[12] = {
    {-1, 1.618f, 0},{1, 1.618f, 0},{-1,-1.618f,0},{1,-1.618f,0},
    {0,-1,1.618f},{0,1,1.618f},{0,-1,-1.618f},{0,1,-1.618f},
    {1.618f,0,-1},{1.618f,0,1},{-1.618f,0,-1},{-1.618f,0,1}
};
static int icoT[20][3] = {
    {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},{1,5,9},{5,11,4},{11,10,2},
    {10,7,6},{7,1,8},{3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},{4,9,5},
    {2,4,11},{6,2,10},{8,6,7},{9,8,1}
};
static V3 cubeV[8] = {{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
static int cubeT[12][3] = {
    {0,1,2},{0,2,3},{4,5,6},{4,6,7},{0,3,7},{0,7,4},
    {1,2,6},{1,6,5},{0,1,5},{0,5,4},{3,2,6},{3,6,7}
};

static V3  *VERT[2] = { icoV, cubeV };
static int (*TRI[2])[3] = { icoT, cubeT };
static int  NV[2] = { 12, 8 };
static int  NT[2] = { 20, 12 };
static const char *MNAME[2] = { "ICOSAHEDRON", "CUBE" };

// ── shade ramp: dark → light. each step is a solid colour OR a checker blend
// of two colours (a fake half-shade). more steps than the 32-colour palette has.
typedef struct { int pat, draw, hole; } Shade;   // pat==FILL_SOLID → just `draw`
#define ANCHORS 6
static int anchor[ANCHORS] = { CLR_DARKER_BLUE, CLR_DARK_BLUE, CLR_TRUE_BLUE, CLR_BLUE, CLR_LIGHT_GREY, CLR_WHITE };
static Shade ramp[ANCHORS * 2 - 1];   // solid, blend, solid, blend, ... solid
static int   nramp = 0;

static int   model = 0;
static float ax = 20, ay = 30, spin = 1.0f, zoom = 48;
static bool  dither = true;

#define FOCAL 4.0f

void init(void) {
    for (int i = 0; i < ANCHORS; i++) {
        ramp[nramp++] = (Shade){ FILL_SOLID, anchor[i], 0 };
        if (i < ANCHORS - 1)
            ramp[nramp++] = (Shade){ FILL_CHECKER, anchor[i], anchor[i + 1] };
    }
}

void update(void) {
    if (btnp(0, BTN_A)) dither = !dither;
    if (btnp(0, BTN_B)) model  = (model + 1) % 2;
    if (btn(0, BTN_UP))    zoom = min(120, zoom + 1.5f);
    if (btn(0, BTN_DOWN))  zoom = max(25,  zoom - 1.5f);
    if (btnp(0, BTN_LEFT))  spin = max(0, spin - 0.4f);
    if (btnp(0, BTN_RIGHT)) spin = min(4, spin + 0.4f);
    ax += 0.5f * spin; ay += 0.8f * spin;
}

void draw(void) {
    cls(CLR_BLACK);
    // dithered ground glow — same fillp tech, on a rectangle
    fillp(FILL_CHECKER, CLR_BLACK);
    rectfill(0, SCREEN_H - 30, SCREEN_W, 30, CLR_DARKER_BLUE);
    fillp_reset();

    V3 *V = VERT[model];
    int (*T)[3] = TRI[model];

    // 1+2. rotate & project every vertex once
    static int   sx[12], sy[12];
    static V3    rv[12];
    for (int i = 0; i < NV[model]; i++) {
        V3 r = rot3(V[i], ay, ax);
        rv[i] = r;
        project3(r, FOCAL, zoom, &sx[i], &sy[i]);
        sy[i] -= 4;                         // nudge the model up a touch
    }

    // light direction (points FROM the surface TO the light): upper-left-front
    float lx = -0.5f, ly = -0.6f, lz = -0.62f;

    // collect visible faces with depth + brightness
    typedef struct { int t; float z; int lv; } Face;
    static Face face[20];
    int nf = 0;
    for (int i = 0; i < NT[model]; i++) {
        int a = T[i][0], b = T[i][1], c = T[i][2];
        V3 ra = rv[a], rb = rv[b], rc = rv[c];
        // edge vectors → normal (cross product)
        float ux = rb.x-ra.x, uy = rb.y-ra.y, uz = rb.z-ra.z;
        float vx = rc.x-ra.x, vy = rc.y-ra.y, vz = rc.z-ra.z;
        float nx = uy*vz - uz*vy, ny = uz*vx - ux*vz, nz = ux*vy - uy*vx;
        // make the normal point OUTWARD (away from origin = the model centre)
        float cx = (ra.x+rb.x+rc.x)/3, cy = (ra.y+rb.y+rc.y)/3, cz = (ra.z+rb.z+rc.z)/3;
        if (nx*cx + ny*cy + nz*cz < 0) { nx=-nx; ny=-ny; nz=-nz; }
        // 3. cull: camera looks down +z, so a face is visible when its normal
        // points back toward us (nz < 0).
        if (nz >= 0) continue;
        // 4. light: lambert = how aligned the (unit) normal is with the light
        float len = sqrtf(nx*nx + ny*ny + nz*nz); if (len < 0.0001f) len = 1;
        float lam = (nx*lx + ny*ly + nz*lz) / len;
        if (lam < 0) lam = 0;
        float bright = 0.12f + 0.88f * lam;          // ambient floor + diffuse
        int lv;
        if (dither) lv = (int)(bright * (nramp - 1) + 0.5f);
        else        lv = (int)(bright * (ANCHORS - 1) + 0.5f) * 2;  // snap to solids only
        if (lv < 0) lv = 0; if (lv > nramp - 1) lv = nramp - 1;
        face[nf].t = i; face[nf].z = (ra.z+rb.z+rc.z)/3; face[nf].lv = lv; nf++;
    }

    // 5. painter's sort: far (big z) first — zsort orders the faces for us
    static float fz[20]; static int order[20];
    for (int i = 0; i < nf; i++) fz[i] = face[i].z;
    zsort(fz, order, nf);

    // 6. fill each face with its shade
    for (int i = 0; i < nf; i++) {
        int *idx = T[face[order[i]].t];
        Shade s = ramp[face[order[i]].lv];
        if (s.pat == FILL_SOLID) {
            trifill(sx[idx[0]],sy[idx[0]], sx[idx[1]],sy[idx[1]], sx[idx[2]],sy[idx[2]], s.draw);
        } else {
            fillp(s.pat, s.hole);
            trifill(sx[idx[0]],sy[idx[0]], sx[idx[1]],sy[idx[1]], sx[idx[2]],sy[idx[2]], s.draw);
            fillp_reset();
        }
    }

    print(str("%s   %d faces", MNAME[model], nf), 4, 4, CLR_WHITE);
    print(dither ? "DITHER: ON  (smooth)" : "DITHER: OFF (banded)", 4, 13,
          dither ? CLR_LIME_GREEN : CLR_ORANGE);
    print("Z dither  X model  up/down zoom  L/R spin", 4, SCREEN_H - 9, CLR_LIGHT_GREY);
}
