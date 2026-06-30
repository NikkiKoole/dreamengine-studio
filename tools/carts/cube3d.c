/* de:meta
{
  "title": "3D wireframe",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "software-rasterizer"
  ],
  "lineage": "Follows the earlier raycaster cart as the next step in the 3D tutorial arc; introduces the full rotate-then-perspective-project pipeline via engine helpers rot3/project3 on multiple Platonic solids.",
  "description": "Real 3D this time (the raycaster only faked it). A solid is a list of 3D corner points and the edges joining them; each frame rotates every point with sin/cos, projects it to 2D with perspective (far points shrink toward the centre), and connects the dots — the whole 3D pipeline in miniature. Z cycles cube/tetrahedron/octahedron, up/down zoom, left/right spin speed.",
  "todo": [
    "Better mouse support (wheel-zoom, drag-rotate).",
    "The L/R spin is weird: it stops the spin but can't restart it.",
    "Add an onscreen toggle for auto-rotate; leave seams for touch pinch-zoom."
  ]
}
de:meta */
#include "studio.h"

// 3D WIREFRAME — real 3D this time (the raycaster only faked it).
//
// A shape is just a list of corner points in 3D space and a list of edges
// joining them. Each frame we ROTATE every point (spin it around the X and Y
// axes with sin/cos), then PROJECT it to 2D with perspective — points further
// away move toward the centre and shrink — and connect the dots. That's the
// whole pipeline behind every 3D game.
//
// (V3, rot3 and project are engine helpers now — see studio.h.)
//
//   Z  next solid     up/down  zoom     left/right  spin speed

static V3  cubeV[8] = {{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
static int cubeE[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
static V3  tetV[4] = {{1,1,1},{1,-1,-1},{-1,1,-1},{-1,-1,1}};
static int tetE[6][2] = {{0,1},{0,2},{0,3},{1,2},{1,3},{2,3}};
static V3  octV[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
static int octE[12][2] = {{0,2},{0,3},{0,4},{0,5},{1,2},{1,3},{1,4},{1,5},{2,4},{2,5},{3,4},{3,5}};

static V3 *VERT[3] = { cubeV, tetV, octV };
static int (*EDGE[3])[2] = { cubeE, tetE, octE };
static int NV[3] = { 8, 4, 6 };
static int NE[3] = { 12, 6, 12 };
static const char *SNAME[3] = { "CUBE", "TETRAHEDRON", "OCTAHEDRON" };

static int   shape = 0;
static float ax = 25, ay = 35, spin = 1.0f, zoom = 42;

#define FOCAL 3.0f

void update(void) {
    if (btnp(0, BTN_A)) shape = (shape + 1) % 3;
    if (btn(0, BTN_UP))    zoom = min(90, zoom + 1.5f);
    if (btn(0, BTN_DOWN))  zoom = max(18, zoom - 1.5f);
    if (btnp(0, BTN_LEFT))  spin = max(0, spin - 0.4f);
    if (btnp(0, BTN_RIGHT)) spin = min(4, spin + 0.4f);
    ax += 0.7f * spin; ay += 1.1f * spin;
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    V3 *V = VERT[shape];
    int (*E)[2] = EDGE[shape];

    // project all vertices once
    static int sx[8], sy[8]; static float sz[8];
    for (int i = 0; i < NV[shape]; i++) {
        V3 r = rot3(V[i], ay, ax);
        project3(r, FOCAL, zoom, &sx[i], &sy[i]);
        sz[i] = r.z;
    }
    // edges, shaded by depth (nearer = brighter)
    for (int e = 0; e < NE[shape]; e++) {
        int a = E[e][0], b = E[e][1];
        float zavg = (sz[a] + sz[b]) / 2;
        int col = zavg < -0.6f ? CLR_WHITE : zavg < 0.0f ? CLR_BLUE
                : zavg < 0.6f ? CLR_TRUE_BLUE : CLR_INDIGO;
        line(sx[a], sy[a], sx[b], sy[b], col);
    }
    for (int i = 0; i < NV[shape]; i++) circfill(sx[i], sy[i], 2, CLR_LIGHT_YELLOW);

    print(str("%s   spin x%.1f", SNAME[shape], spin), 4, 4, CLR_WHITE);
    print("Z: next solid   up/down: zoom   L/R: spin", 4, SCREEN_H - 9, CLR_LIGHT_GREY);
}
