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
  "lineage": "Soft-body twin of the `tentacle` skinning spike: where tentacle skins a verlet CHAIN, jelly skins a PRESSURIZED verlet ring with a radial tritex fan. The ring holds its shape by gas pressure (area preservation) — not rigid spokes, which let a distance-only blob fold and never reform — so you can squash it flat and it re-rounds. Blobs collide rim-point vs rim-edge (the physics.c box-stacking trick) so a heap of them stacks. Composes runtime/physics.h + tritex, no new engine code.",
  "description": "Four wobbly jellies flop down and stack in a squishy pile — each a pressurized soft-body blob (a verlet rim ring that preserves its area, runtime/physics.h) that collides with the others and wears a texture fanned out with tritex, so the printed face (or checkerboard) deforms as the blob squashes against its neighbours. Grab any jelly to pull it out of the heap and drop it back on top; let go and the pressure re-rounds it. R resets. The checker one makes the squish obvious; the faces make it cute."
}
de:meta */
#include "studio.h"
#include "physics.h"   // the soft-body — shared verlet toolkit (Layer 0)

// JELLY — the soft-body skinning spike. Each blob is a ring of verlet rim points held
// round by TWO things: rim links (fixed edge lengths) + PRESSURE (the polygon pushes its
// edges outward to restore a rest area). Pressure is what lets it fully squash and reform —
// a distance-only ring folds into a shape that still satisfies every length and never
// re-rounds. The hub is just the render centroid for the radial tritex skin fan.

#define NB       4          // blobs
#define NR       14         // rim points per blob
#define R0       16.0f      // rest radius
#define GRAV     0.34f
#define DAMP     0.99f
#define ITERS    6
#define PRESSURE 0.18f      // how hard the blob re-inflates toward its rest area
#define FLOORY   (SCREEN_H - 8)

typedef struct {
    PhysPt hub;             // render-only: the rim centroid (fan centre for the skin)
    PhysPt rim[NR];
    float  texA[NR];        // each rim's FIXED texture angle (deg) — the skin is painted on
} Blob;

static Blob  blob[NB];
static int   skin[NB];
static float rimRest, restArea;
static int   grabBl = -1, grabK = -1;
static float gw, pmx, pmy;

static float vlen(float x, float y) { return fsqrt(x * x + y * y); }

// signed polygon area (shoelace) of a blob's rim — sign encodes winding, so restoring it
// toward the rest value also un-does an inversion (a folded blob has the wrong sign).
static float poly_area(Blob *b) {
    float A = 0.0f;
    for (int k = 0; k < NR; k++) {
        int k2 = (k + 1) % NR;
        A += b->rim[k].x * b->rim[k2].y - b->rim[k2].x * b->rim[k].y;
    }
    return 0.5f * A;
}

static void set_hub(Blob *b) {                       // hub = centroid, for the skin fan centre
    float sx = 0, sy = 0;
    for (int k = 0; k < NR; k++) { sx += b->rim[k].x; sy += b->rim[k].y; }
    b->hub.x = sx / NR; b->hub.y = sy / NR;
}

static void spawn(int i, float cx, float cy, int s) {
    Blob *b = &blob[i];
    for (int k = 0; k < NR; k++) {
        float a = (float)k / NR * 360.0f;
        phys_pt(&b->rim[k], cx + cos_deg(a) * R0, cy + sin_deg(a) * R0, 1.0f, 2.0f);  // r=2 → floor contact
        b->texA[k] = a;
    }
    set_hub(b);
    skin[i] = s;
}

void init(void) {
    rimRest = 2.0f * R0 * sin_deg(180.0f / NR);      // chord between adjacent rim points
    spawn(0, 150,  24, 0);                            // a loose column → they fall and stack
    spawn(1, 168,  66, 1);
    spawn(2, 150, 108, 2);
    spawn(3, 168, 150, 0);
    restArea = poly_area(&blob[0]);                  // all blobs spawn identical → one rest area
    grabBl = grabK = -1;
}

void update(void) {
    float mx = (float)mouse_x(), my = (float)mouse_y();

    if (mouse_pressed(MOUSE_LEFT)) {                  // grab the nearest rim point
        float best = 16.0f;
        for (int i = 0; i < NB; i++)
            for (int k = 0; k < NR; k++) {
                float d = vlen(mx - blob[i].rim[k].x, my - blob[i].rim[k].y);
                if (d < best) { best = d; grabBl = i; grabK = k; }
            }
        if (grabBl >= 0) { gw = blob[grabBl].rim[grabK].w; blob[grabBl].rim[grabK].w = 0; }
    }
    if (grabBl >= 0 && mouse_down(MOUSE_LEFT)) phys_grab(&blob[grabBl].rim[grabK], mx, my, pmx, pmy);
    if (mouse_released(MOUSE_LEFT) && grabBl >= 0) { blob[grabBl].rim[grabK].w = gw; grabBl = grabK = -1; }

    if (keyp('R')) init();

    for (int i = 0; i < NB; i++)                          // integrate every rim point
        for (int k = 0; k < NR; k++) phys_integrate(&blob[i].rim[k], 0.0f, GRAV, DAMP);

    for (int it = 0; it < ITERS; it++) {
        for (int i = 0; i < NB; i++) {                    // each blob's own shape: perimeter + pressure
            Blob *b = &blob[i];
            for (int k = 0; k < NR; k++) phys_link(&b->rim[k], &b->rim[(k + 1) % NR], rimRest);
            float corr = PRESSURE * (restArea - poly_area(b)) / restArea;   // restore area → re-round
            for (int k = 0; k < NR; k++) {
                if (b->rim[k].w == 0) continue;           // don't fight a grabbed/pinned point
                int kp = (k + NR - 1) % NR, kn = (k + 1) % NR;
                b->rim[k].x += 0.5f * (b->rim[kn].y - b->rim[kp].y) * corr;   // +gradient of signed area
                b->rim[k].y += 0.5f * (b->rim[kp].x - b->rim[kn].x) * corr;
            }
        }
        for (int i = 0; i < NB; i++)                       // BLOB vs BLOB — each rim point vs the other
            for (int j = 0; j < NB; j++) {                 // blob's rim edges (solid faces → they stack)
                if (i == j) continue;
                for (int p = 0; p < NR; p++)
                    for (int e = 0; e < NR; e++)
                        phys_collide_seg(&blob[i].rim[p], &blob[j].rim[e], &blob[j].rim[(e + 1) % NR]);
            }
        for (int i = 0; i < NB; i++)                       // walls + floor last
            for (int k = 0; k < NR; k++) phys_bounds(&blob[i].rim[k], 0, 0, SCREEN_W, FLOORY, 0.4f, 0.9f);
    }
    for (int i = 0; i < NB; i++) set_hub(&blob[i]);
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
            tritex((int)b->hub.x,     (int)b->hub.y,     ucx, vcy,      // fan: hub(centroid) -> texture centre
                   (int)b->rim[k].x,  (int)b->rim[k].y,  u1,  v1,
                   (int)b->rim[k2].x, (int)b->rim[k2].y, u2,  v2);
        }
    }

    font(FONT_SMALL);
    print("pressurized soft-bodies that collide + stack (physics.h + tritex)  —  grab · stack · R reset", 4, 4, CLR_LIGHT_GREY);
}
