/* de:meta
{
  "slug": "tentacle",
  "collection": ["physics"],
  "title": "Tentacle",
  "status": "active",
  "created": "2026-07-13",
  "kind": [
    "tech-demo",
    "probe"
  ],
  "teaches": [
    "verlet-integration",
    "procedural-mesh"
  ],
  "lineage": "Spike for the bone-driven textured-mesh (skinning) capability: composes the shared runtime/physics.h verlet toolkit (the BONES) with the engine's tritex() affine textured triangle (the SKIN) — the 'bodies ARE bones' pattern the deform/puppet experiments kept reaching for, at its smallest. No new engine code.",
  "description": "Five tentacles sway in the current, each in a different skin — scales, candy stripes, a checkerboard — so you can watch the affine texture warp as they bend. Each is a chain of verlet bones (runtime/physics.h) wearing textured triangles (tritex) stretched between the bones; the skin bends and shears with the motion. Grab any tentacle and fling it; it whips and settles back into the drift. R resets. The point of the spike: physics points make the skeleton, tritex makes the deformable skin — that's all skinning is."
}
de:meta */
#include "studio.h"
#include "physics.h"   // the BONES — shared verlet toolkit (Layer 0)

// TENTACLE — the bone+skin spike. Each tentacle is a verlet bone chain (physics.h)
// with a ribbon of tritex() textured triangles stretched across consecutive bones.
// Move the bones (gravity, a swaying current, your finger) and the skin deforms with
// them. Mouse: grab+fling a tentacle. R: reset.

#define NT      5          // tentacles (one per skin)
#define NB      9          // bones per tentacle
#define REST   12.0f       // bone spacing (also the segment length)
#define BASE_W 13.0f       // ribbon width at the anchored base
#define TIP_W   3.0f       // ribbon width at the free tip
#define GRAV    0.40f      // downward pull, px/frame^2
#define DAMP    0.99f      // water drag
#define ITERS   6          // constraint relaxation passes
#define TOPY    10.0f      // where the tentacles are anchored

static PhysPt bone[NT][NB];
static int    skin[NT];      // sheet tile (0..2) each tentacle wears
static int    tipcol[NT];    // matching cap colour for the rounded tip
static int    grabT = -1, grabB = -1;
static float  gw, pmx, pmy;

static float vlen(float x, float y) { return fsqrt(x * x + y * y); }

static void reset_scene(void) {
    static const int SKINS[] = { 0, 1, 2, 3, 4 };        // scales ×3, vstripes, checker
    static const int TIPS[]  = { 11, 14, 12, 8, 6 };     // matching cap colours
    for (int t = 0; t < NT; t++) {
        float bx = SCREEN_W * (t + 0.5f) / NT;              // spread across the top
        for (int b = 0; b < NB; b++)
            phys_pt(&bone[t][b], bx, TOPY + b * REST, b == 0 ? 0.0f : 1.0f, 0.0f);  // bone 0 pinned (w=0)
        skin[t]   = SKINS[t];
        tipcol[t] = TIPS[t];
    }
    grabT = grabB = -1;
}

void init(void) { reset_scene(); }

void update(void) {
    float mx = (float)mouse_x(), my = (float)mouse_y();

    if (mouse_pressed(MOUSE_LEFT)) {                        // grab the nearest bone (never the pin)
        float best = 14.0f;
        for (int t = 0; t < NT; t++)
            for (int b = 1; b < NB; b++) {
                float d = vlen(mx - bone[t][b].x, my - bone[t][b].y);
                if (d < best) { best = d; grabT = t; grabB = b; }
            }
        if (grabT >= 0) { gw = bone[grabT][grabB].w; bone[grabT][grabB].w = 0; }
    }
    if (grabT >= 0 && mouse_down(MOUSE_LEFT))
        phys_grab(&bone[grabT][grabB], mx, my, pmx, pmy);   // drag; prev-mouse = throw speed
    if (mouse_released(MOUSE_LEFT) && grabT >= 0) { bone[grabT][grabB].w = gw; grabT = grabB = -1; }

    if (keyp('R')) reset_scene();

    for (int t = 0; t < NT; t++) {
        float gx = sin_deg(now() * 60.0f + t * 90.0f) * 0.08f;   // a gentle current sways each one
        for (int b = 0; b < NB; b++) phys_integrate(&bone[t][b], gx, GRAV, DAMP);
        for (int it = 0; it < ITERS; it++)
            for (int b = 0; b < NB - 1; b++) phys_link(&bone[t][b], &bone[t][b + 1], REST);
    }
    pmx = mx; pmy = my;
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    rectfill(0, SCREEN_H - 6, SCREEN_W, 6, CLR_DARK_GREEN);   // seabed

    for (int t = 0; t < NT; t++) {
        float u0 = skin[t] * 16.0f;
        float uL = u0 + 0.5f, uR = u0 + 15.5f, vT = 0.5f, vB = 15.5f;  // inset 0.5 so we never sample the neighbour tile

        // Ribbon corners are computed PER BONE (not per segment) using a shared normal
        // at each joint — the average of the two adjacent bone directions. Because
        // consecutive segments then reuse the very same corner points, they meet on one
        // edge with no gap: the seams at bends disappear, without any weighted skinning.
        float lx[NB], ly[NB], rx[NB], ry[NB];
        for (int b = 0; b < NB; b++) {
            float tx = 0, ty = 0;                            // tangent = sum of adjacent unit dirs
            if (b > 0)    { float dx = bone[t][b].x - bone[t][b-1].x, dy = bone[t][b].y - bone[t][b-1].y;
                            float d = vlen(dx, dy); if (d > 0.001f) { tx += dx/d; ty += dy/d; } }
            if (b < NB-1) { float dx = bone[t][b+1].x - bone[t][b].x, dy = bone[t][b+1].y - bone[t][b].y;
                            float d = vlen(dx, dy); if (d > 0.001f) { tx += dx/d; ty += dy/d; } }
            float tl = vlen(tx, ty); if (tl < 0.001f) { tx = 0; ty = 1; tl = 1; }
            float nx = -ty/tl, ny = tx/tl;                   // unit normal = perpendicular of the tangent
            float w = (BASE_W + (TIP_W - BASE_W) * b / (float)(NB - 1)) * 0.5f;
            lx[b] = bone[t][b].x - nx*w;  ly[b] = bone[t][b].y - ny*w;
            rx[b] = bone[t][b].x + nx*w;  ry[b] = bone[t][b].y + ny*w;
        }
        for (int b = 0; b < NB - 1; b++) {                   // shared corners → seamless quads
            tritex((int)lx[b],(int)ly[b], uL,vT,  (int)rx[b],  (int)ry[b],   uR,vT,  (int)rx[b+1],(int)ry[b+1], uR,vB);
            tritex((int)lx[b],(int)ly[b], uL,vT,  (int)rx[b+1],(int)ry[b+1], uR,vB,  (int)lx[b+1],(int)ly[b+1], uL,vB);
        }
        PhysPt *tip = &bone[t][NB - 1];
        circfill((int)tip->x, (int)tip->y, (int)TIP_W, tipcol[t]);            // rounded cap
        circfill((int)bone[t][0].x, (int)bone[t][0].y, 3, CLR_DARK_GREY);     // anchor rock
    }

    font(FONT_SMALL);
    print("bones = physics.h  ·  skin = tritex   —   drag a tentacle · R reset", 4, 4, CLR_LIGHT_GREY);
}
