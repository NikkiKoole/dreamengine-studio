/* de:meta
{
  "slug": "jelly",
  "collection": ["physics"],
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
  "lineage": "Soft-body twin of the `tentacle` skinning spike: skins a PRESSURIZED verlet ring with a radial tritex fan. Two rest shapes — a circle and a rounded rectangle (superellipse) — both held by gas pressure (area preservation) + per-edge rim links, so they squash and re-round. Blobs collide via SAT point-vs-polygon (max signed distance), which ejects even a deeply-penetrated point — the fix for blobs sinking into each other. Composes runtime/physics.h + tritex, no new engine code.",
  "description": "A heap of wobbly jellies — round ones and rounded-rectangle ones — flop down and stack, squashing against each other and springing back. Each is a pressurized soft-body (a verlet rim ring that preserves its area, runtime/physics.h) wearing a texture fanned out with tritex, so the printed face or checkerboard deforms as the blob does. They collide as solid bodies (SAT), so one can rest on another instead of sinking through. Grab any jelly to pull it out of the pile and drop it back on top; let go and pressure re-rounds it. R resets."
}
de:meta */
#include "studio.h"
#include "physics.h"   // the soft-body — shared verlet toolkit (Layer 0)

// JELLY — pressurized soft-body blobs with a tritex skin, in two shapes.
//   shape  = the rest outline: a circle, or a rounded rect (superellipse, exponent 4)
//   held   = rim links (per-edge rest length) + gas PRESSURE (restore the rest area)
//   skin   = a radial tritex fan; each rim carries a FIXED texture coord so the skin
//            deforms with the blob
//   collide= SAT point-vs-polygon so they stack instead of sinking into each other
// Grab a rim point to squish/throw. R resets.

#define NB       6
#define NR       14
#define R0       15.0f
#define GRAV     0.34f
#define DAMP     0.93f       // velocity retention: lower = calmer, less erratic jiggle
#define ITERS    3          // constraint/collision passes per substep
#define SUBSTEPS 4          // split each frame → a fast blob can't tunnel through another
#define PRESSURE 0.18f
#define FLOORY   (SCREEN_H - 8)
#define SHAPE_CIRCLE 0
#define SHAPE_RRECT  1

typedef struct {
    PhysPt hub;                 // render-only: the rim centroid (skin fan centre + collision "outward")
    PhysPt rim[NR];
    float  texU[NR], texV[NR];  // each rim's FIXED texture coord (tile-local 0..16)
    float  edgeRest[NR];        // per-edge rest length (edge k = rim[k]..rim[k+1])
    float  restArea;
    int    skin;
} Blob;

static Blob  blob[NB];
static int   grabBl = -1, grabK = -1;
static float gw, pmx, pmy;

static float vlen(float x, float y) { return fsqrt(x * x + y * y); }

// signed polygon area (shoelace) — sign encodes winding, so restoring it un-does inversions.
static float poly_area(Blob *b) {
    float A = 0.0f;
    for (int k = 0; k < NR; k++) { int k2 = (k + 1) % NR; A += b->rim[k].x * b->rim[k2].y - b->rim[k2].x * b->rim[k].y; }
    return 0.5f * A;
}
static void set_hub(Blob *b) {
    float sx = 0, sy = 0;
    for (int k = 0; k < NR; k++) { sx += b->rim[k].x; sy += b->rim[k].y; }
    b->hub.x = sx / NR; b->hub.y = sy / NR;
}

// rest OUTLINE offset from centre for parameter t in [0,1). Circle, or a superellipse
// (exponent 4 → rounded rectangle: flat-ish sides, rounded corners).
static void shape_pt(int shape, float t, float *ox, float *oy) {
    float a = t * 360.0f, c = cos_deg(a), s = sin_deg(a);
    if (shape == SHAPE_RRECT) {
        float ac = c < 0 ? -c : c, as = s < 0 ? -s : s;
        *ox = R0 * 1.40f * (c < 0 ? -1 : 1) * fsqrt(ac);   // fsqrt == the exponent-4 superellipse power
        *oy = R0 * 0.80f * (s < 0 ? -1 : 1) * fsqrt(as);
    } else { *ox = c * R0; *oy = s * R0; }
}
// matching texture coord (tile-local, centred at 8): disc for the circle, squircle for the rect.
static void shape_uv(int shape, float t, float *u, float *v) {
    float a = t * 360.0f, c = cos_deg(a), s = sin_deg(a);
    if (shape == SHAPE_RRECT) {
        float ac = c < 0 ? -c : c, as = s < 0 ? -s : s;
        *u = 8.0f + 7.2f * (c < 0 ? -1 : 1) * fsqrt(ac);
        *v = 8.0f + 7.2f * (s < 0 ? -1 : 1) * fsqrt(as);
    } else { *u = 8.0f + c * 6.0f; *v = 8.0f + s * 6.0f; }
}

static void spawn(int i, float cx, float cy, int shape, int skin) {
    Blob *b = &blob[i];
    for (int k = 0; k < NR; k++) {
        float ox, oy; shape_pt(shape, (float)k / NR, &ox, &oy);
        phys_pt(&b->rim[k], cx + ox, cy + oy, 1.0f, 2.0f);   // r=2 → floor + blob contact
        shape_uv(shape, (float)k / NR, &b->texU[k], &b->texV[k]);
    }
    for (int k = 0; k < NR; k++) { int k2 = (k + 1) % NR; b->edgeRest[k] = vlen(b->rim[k2].x - b->rim[k].x, b->rim[k2].y - b->rim[k].y); }
    set_hub(b);
    b->restArea = poly_area(b);
    b->skin = skin;
}

// SAT: point P vs blob B. Find the edge of B with the MAX signed distance (outward normals
// oriented via the centroid). If that's still < P.r, P penetrates → eject it along that edge's
// normal (split with the edge). Because "max signed distance" is the shallowest wall, a point
// buried DEEP inside is pushed out the short way — the old within-radius test left it stuck.
static void blob_vs_point(PhysPt *P, Blob *B) {
    float maxSD = -1e9f, mnx = 0, mny = 0; int me = -1;
    for (int e = 0; e < NR; e++) {
        int e2 = (e + 1) % NR;
        float ax = B->rim[e].x, ay = B->rim[e].y, ex = B->rim[e2].x - ax, ey = B->rim[e2].y - ay;
        float el = vlen(ex, ey); if (el < 0.001f) continue;
        float nx = ey / el, ny = -ex / el;
        float midx = ax + ex * 0.5f, midy = ay + ey * 0.5f;
        if ((midx - B->hub.x) * nx + (midy - B->hub.y) * ny < 0) { nx = -nx; ny = -ny; }   // outward from centroid
        float sd = (P->x - ax) * nx + (P->y - ay) * ny;
        if (sd > maxSD) { maxSD = sd; mnx = nx; mny = ny; me = e; }
    }
    if (me < 0 || maxSD >= P->r) return;                 // outside → no contact
    float push = P->r - maxSD;
    int e2 = (me + 1) % NR; PhysPt *a = &B->rim[me], *b = &B->rim[e2];
    if (P->w) {
        P->x += mnx * push * 0.5f;  P->y += mny * push * 0.5f;
        float tx = -mny, ty = mnx;                        // tangent along the contact edge
        float vt = (P->x - P->px) * tx + (P->y - P->py) * ty;   // sideways slip
        P->px += tx * vt * 0.35f;  P->py += ty * vt * 0.35f;    // rub some off → blobs grip + stack
    }
    if (a->w) { a->x -= mnx * push * 0.25f; a->y -= mny * push * 0.25f; }
    if (b->w) { b->x -= mnx * push * 0.25f; b->y -= mny * push * 0.25f; }
}

void init(void) {
    spawn(0, 158,  18, SHAPE_CIRCLE, 0);   // one loose column of mixed shapes → they stack
    spawn(1, 165,  52, SHAPE_RRECT,  3);
    spawn(2, 155,  86, SHAPE_CIRCLE, 1);
    spawn(3, 164, 120, SHAPE_RRECT,  4);
    spawn(4, 157, 154, SHAPE_CIRCLE, 2);
    spawn(5, 162, 186, SHAPE_RRECT,  3);
    grabBl = grabK = -1;
}

void update(void) {
    float mx = (float)mouse_x(), my = (float)mouse_y();

    if (mouse_pressed(MOUSE_LEFT)) {                      // grab the nearest rim point
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

    for (int s = 0; s < SUBSTEPS; s++) {                  // substeps: small moves → no tunneling
        for (int i = 0; i < NB; i++)
            for (int k = 0; k < NR; k++) phys_integrate(&blob[i].rim[k], 0.0f, GRAV / SUBSTEPS, DAMP);

        for (int it = 0; it < ITERS; it++) {
            for (int i = 0; i < NB; i++) {                // each blob's own shape: edges + pressure
                Blob *b = &blob[i];
                for (int k = 0; k < NR; k++) phys_link(&b->rim[k], &b->rim[(k + 1) % NR], b->edgeRest[k]);
                float corr = PRESSURE * (b->restArea - poly_area(b)) / b->restArea;
                for (int k = 0; k < NR; k++) {
                    if (b->rim[k].w == 0) continue;
                    int kp = (k + NR - 1) % NR, kn = (k + 1) % NR;
                    b->rim[k].x += 0.5f * (b->rim[kn].y - b->rim[kp].y) * corr;
                    b->rim[k].y += 0.5f * (b->rim[kp].x - b->rim[kn].x) * corr;
                }
            }
            for (int i = 0; i < NB; i++) set_hub(&blob[i]);   // centroid current before SAT uses it
            for (int i = 0; i < NB; i++)                      // BLOB vs BLOB (SAT point-vs-polygon)
                for (int j = 0; j < NB; j++) {
                    if (i == j) continue;
                    for (int p = 0; p < NR; p++) blob_vs_point(&blob[i].rim[p], &blob[j]);
                }
            for (int i = 0; i < NB; i++)                      // walls + floor last
                for (int k = 0; k < NR; k++) phys_bounds(&blob[i].rim[k], 0, 0, SCREEN_W, FLOORY, 0.4f, 0.6f);
        }
    }
    for (int i = 0; i < NB; i++) set_hub(&blob[i]);
    pmx = mx; pmy = my;
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    rectfill(0, FLOORY, SCREEN_W, SCREEN_H - FLOORY, CLR_DARK_GREEN);

    for (int i = 0; i < NB; i++) {
        Blob *b = &blob[i];
        float u0 = b->skin * 16.0f;
        for (int k = 0; k < NR; k++) {
            int k2 = (k + 1) % NR;
            tritex((int)b->hub.x,     (int)b->hub.y,     u0 + 8.0f,        8.0f,          // fan centre
                   (int)b->rim[k].x,  (int)b->rim[k].y,  u0 + b->texU[k],  b->texV[k],
                   (int)b->rim[k2].x, (int)b->rim[k2].y, u0 + b->texU[k2], b->texV[k2]);
        }
    }

    font(FONT_SMALL);
    print("soft-bodies: circles + rounded rects, collide + stack (physics.h + tritex)  —  grab · R reset", 4, 4, CLR_LIGHT_GREY);
}
