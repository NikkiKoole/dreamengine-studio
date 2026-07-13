/* de:meta
{
  "slug": "jelly",
  "title": "Jelly",
  "status": "active",
  "created": "2026-07-13",
  "kind": [
    "tech-demo",
    "probe"
  ],
  "teaches": [
    "verlet-integration",
    "soft-body",
    "procedural-mesh"
  ],
  "lineage": "Soft-body twin of the `tentacle` skinning spike: where tentacle skins a verlet CHAIN, jelly skins a verlet RING+HUB (the physics.c soft-body blob) with a radial tritex fan — the hub maps to the texture centre, each rim point to a point on a circle, so the texture squishes with the blob. Composes runtime/physics.h + tritex, no new engine code.",
  "description": "Three wobbly jellies flop to the floor and squish under their own weight, then spring back — each a soft-body blob (a ring of verlet points around a hub, runtime/physics.h) wearing a texture fanned out with tritex, so the printed face (or checkerboard) deforms as the blob does. Grab any jelly and fling it; watch it splat and re-round. R resets. The checker one makes the squish obvious; the faces make it cute."
}
de:meta */
#include "studio.h"
#include "physics.h"   // the soft-body — shared verlet toolkit (Layer 0)

// JELLY — the soft-body skinning spike. Each blob is a ring of verlet rim points around a
// hub (spokes keep it inflated, rim links hold the perimeter — the physics.c blob). The skin
// is a radial tritex fan: hub -> texture centre, each rim -> a fixed point on a texture
// circle, so the texture deforms WITH the blob. Grab to squish/throw. R resets.

#define NB     3           // blobs
#define NR     12          // rim points per blob
#define R0     15.0f       // rest radius
#define GRAV   0.38f
#define DAMP   0.99f
#define ITERS  6
#define FLOORY (SCREEN_H - 8)
#define HUB    NR          // grab index that means "the hub"

typedef struct {
    PhysPt hub;
    PhysPt rim[NR];
    float  texA[NR];        // each rim's FIXED texture angle (deg) — the skin is painted on
} Blob;

static Blob  blob[NB];
static int   skin[NB];
static float spokeRest, rimRest;
static int   grabBl = -1, grabIdx = -1;
static float gw, pmx, pmy;

static float vlen(float x, float y) { return fsqrt(x * x + y * y); }

static PhysPt *grabbed(void) {
    if (grabBl < 0) return 0;
    return grabIdx == HUB ? &blob[grabBl].hub : &blob[grabBl].rim[grabIdx];
}

static void spawn(int i, float cx, float cy, int s) {
    Blob *b = &blob[i];
    phys_pt(&b->hub, cx, cy, 1.0f, 0.0f);
    for (int k = 0; k < NR; k++) {
        float a = (float)k / NR * 360.0f;
        phys_pt(&b->rim[k], cx + cos_deg(a) * R0, cy + sin_deg(a) * R0, 1.0f, 2.0f);  // r=2 → floor contact
        b->texA[k] = a;
    }
    skin[i] = s;
}

void init(void) {
    spokeRest = R0;
    rimRest   = 2.0f * R0 * sin_deg(180.0f / NR);   // chord between adjacent rim points
    spawn(0,  70, 60, 0);
    spawn(1, 160, 38, 1);
    spawn(2, 250, 66, 2);
    grabBl = grabIdx = -1;
}

void update(void) {
    float mx = (float)mouse_x(), my = (float)mouse_y();

    if (mouse_pressed(MOUSE_LEFT)) {                 // grab the nearest point (rim or hub)
        float best = 16.0f;
        for (int i = 0; i < NB; i++) {
            float dh = vlen(mx - blob[i].hub.x, my - blob[i].hub.y);
            if (dh < best) { best = dh; grabBl = i; grabIdx = HUB; }
            for (int k = 0; k < NR; k++) {
                float d = vlen(mx - blob[i].rim[k].x, my - blob[i].rim[k].y);
                if (d < best) { best = d; grabBl = i; grabIdx = k; }
            }
        }
        if (grabBl >= 0) { PhysPt *p = grabbed(); gw = p->w; p->w = 0; }
    }
    if (grabBl >= 0 && mouse_down(MOUSE_LEFT)) phys_grab(grabbed(), mx, my, pmx, pmy);
    if (mouse_released(MOUSE_LEFT) && grabBl >= 0) { grabbed()->w = gw; grabBl = grabIdx = -1; }

    if (keyp('R')) init();

    for (int i = 0; i < NB; i++) {
        Blob *b = &blob[i];
        phys_integrate(&b->hub, 0.0f, GRAV, DAMP);
        for (int k = 0; k < NR; k++) phys_integrate(&b->rim[k], 0.0f, GRAV, DAMP);
        for (int it = 0; it < ITERS; it++) {
            for (int k = 0; k < NR; k++) phys_link(&b->hub, &b->rim[k], spokeRest);         // spokes inflate
            for (int k = 0; k < NR; k++) phys_link(&b->rim[k], &b->rim[(k + 1) % NR], rimRest);  // perimeter
            phys_bounds(&b->hub, 0, 0, SCREEN_W, FLOORY, 0.4f, 0.9f);
            for (int k = 0; k < NR; k++) phys_bounds(&b->rim[k], 0, 0, SCREEN_W, FLOORY, 0.4f, 0.9f);
        }
    }
    pmx = mx; pmy = my;
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    rectfill(0, FLOORY, SCREEN_W, SCREEN_H - FLOORY, CLR_DARK_GREEN);   // floor

    for (int i = 0; i < NB; i++) {
        Blob *b = &blob[i];
        float ucx = skin[i] * 16.0f + 8.0f, vcy = 8.0f, tr = 6.0f;     // texture disc: centre + radius
        for (int k = 0; k < NR; k++) {
            int k2 = (k + 1) % NR;
            float u1 = ucx + cos_deg(b->texA[k])  * tr, v1 = vcy + sin_deg(b->texA[k])  * tr;
            float u2 = ucx + cos_deg(b->texA[k2]) * tr, v2 = vcy + sin_deg(b->texA[k2]) * tr;
            tritex((int)b->hub.x,     (int)b->hub.y,     ucx, vcy,      // fan: hub -> texture centre
                   (int)b->rim[k].x,  (int)b->rim[k].y,  u1,  v1,
                   (int)b->rim[k2].x, (int)b->rim[k2].y, u2,  v2);
        }
    }

    font(FONT_SMALL);
    print("soft-body (physics.h) + radial tritex skin  —  grab to squish · fling · R reset", 4, 4, CLR_LIGHT_GREY);
}
