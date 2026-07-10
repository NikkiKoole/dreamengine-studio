/* de:meta
{
  "slug": "26-first3d",
  "title": "26. your first 3D",
  "status": "active",
  "created": "2026-07-01",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "software-rasterizer"
  ],
  "lineage": "the on-ramp teaching-gaps.md flagged as missing: cube3d/solid3d/textured3d demo the full 3D pipeline but nothing teaches its moving parts one at a time on the smallest possible shape (a single quad, not a solid) -- including the affine warp + subdivision fix textured3d otherwise introduces alone.",
  "description": {
    "summary": "The whole 3D pipeline, shrunk to one spinning textured square: a point in space, rotated, projected to the screen, then textured with two triangles -- plus the warp that trick causes, and the subdivision fix.",
    "detail": "Every 3D cart in this engine (cube3d, solid3d, textured3d) is this same loop run over more points: ROTATE each 3D corner with rot3(), PROJECT it down to a 2D screen position with project3(), then FILL the shape -- here with tritex(), which paints a triangle by mapping its 3 screen corners to 3 pixels on the sprite sheet. A quad is just two triangles sharing a diagonal, so tritex() gets called twice -- and because each triangle is mapped independently in screen space (no perspective correction), the texture creases along that diagonal as the quad turns. Press X to chop the quad into a finer grid of smaller triangles: each one warps less, so the crease melts away.",
    "controls": "left/right: spin speed. up/down: tilt (pitch). Z: wireframe overlay. X: subdivision level"
  }
}
de:meta */
#include "studio.h"

// 26. YOUR FIRST 3D -- the whole pipeline, shrunk to one shape.
//
// A 3D shape is just a list of points in space (V3 = x,y,z). To put one on
// screen, every cart in this engine does the same three steps:
//
//   1. ROTATE   spin each point around the Y axis (yaw) and X axis (pitch).
//               rot3(point, yaw, pitch) does the sin/cos math for you.
//   2. PROJECT  turn a 3D point into a 2D screen position. Far points land
//               closer to the centre and end up more squeezed together --
//               that squeeze IS perspective. project3(point, focal, zoom,
//               &sx, &sy) returns false if the point is behind the camera.
//   3. FILL     connect the projected points into a shape and colour it in.
//
// A flat square (a "quad") has 4 corners, but tritex() -- the textured-fill
// primitive -- only knows triangles. So a quad is drawn as TWO triangles
// sharing a diagonal: corners (A,B,C) and (A,C,D). Each tritex() call maps
// its 3 screen corners to 3 pixels on the sprite sheet (u,v) and smears the
// texture between them in SCREEN space -- no perspective correction. Watch
// it spin and you'll see the picture crease along that diagonal: the two
// triangles warp by different amounts, because they're stretched to fit the
// screen independently. That's the PS1-era affine-texture look.
//
// THE FIX -- SUBDIVISION: chop the quad into an N x N grid of smaller quads
// first, and texture-map each one separately. Smaller triangles warp less
// (their corners are closer together in both 3D and on screen), so a finer
// grid makes the crease shrink toward invisible. Press X to step the grid
// 1 -> 2 -> 4 -> 8 -> 1 and watch it happen live.

#define TEX   16     // the texture is one 16x16 sprite slot
#define FOCAL 4.0f
#define ZOOM  60.0f

static float yaw = 0, pitch = 15;
static float spin = 1.0f;
static bool  wire = false;
static int   sub = 1;    // subdivision level: 1x1, 2x2, 4x4, or 8x8 sub-quads

void update(void) {
    if (btn(0, BTN_LEFT))  spin = max(0.0f, spin - 0.05f);
    if (btn(0, BTN_RIGHT)) spin = min(4.0f, spin + 0.05f);
    if (btn(0, BTN_UP))    pitch = min(80.0f, pitch + 1.0f);
    if (btn(0, BTN_DOWN))  pitch = max(-80.0f, pitch - 1.0f);
    if (btnp(0, BTN_A))    wire = !wire;
    if (btnp(0, BTN_B))    sub = sub >= 8 ? 1 : sub * 2;

    yaw += spin;
    if (yaw > 360) yaw -= 360;
}

// a point s,t of the way across the quad (s: A->B / D->C, t: top->bottom) --
// plain bilinear interpolation between the 4 (already-rotated) corners.
static V3 quadpt(V3 A, V3 B, V3 C, V3 D, float s, float t) {
    V3 top = { A.x + (B.x - A.x) * s, A.y + (B.y - A.y) * s, A.z + (B.z - A.z) * s };
    V3 bot = { D.x + (C.x - D.x) * s, D.y + (C.y - D.y) * s, D.z + (C.z - D.z) * s };
    return (V3){ top.x + (bot.x - top.x) * t, top.y + (bot.y - top.y) * t, top.z + (bot.z - top.z) * t };
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    // step 1: the quad's 4 corners, flat in space (z=0), before any spin.
    // order goes around the face: top-left, top-right, bottom-right, bottom-left.
    V3 rA = rot3((V3){-1,  1, 0}, yaw, pitch);
    V3 rB = rot3((V3){ 1,  1, 0}, yaw, pitch);
    V3 rC = rot3((V3){ 1, -1, 0}, yaw, pitch);
    V3 rD = rot3((V3){-1, -1, 0}, yaw, pitch);

    // step 2: project the corners just to check none is behind the camera
    // (only possible here at extreme tilt) -- bail out for that one frame
    // rather than draw a broken shape.
    int junk_x, junk_y;
    bool ok = project3(rA, FOCAL, ZOOM, &junk_x, &junk_y)
           && project3(rB, FOCAL, ZOOM, &junk_x, &junk_y)
           && project3(rC, FOCAL, ZOOM, &junk_x, &junk_y)
           && project3(rD, FOCAL, ZOOM, &junk_x, &junk_y);

    if (ok) {
        // step 3: fill -- as an N x N grid of sub-quads, each its own pair of
        // tritex() calls. sub=1 is the raw two-triangle warp from the top of
        // this file; higher sub tames it.
        for (int gy = 0; gy < sub; gy++) {
            for (int gx = 0; gx < sub; gx++) {
                float s0 = (float)gx / sub,       t0 = (float)gy / sub;
                float s1 = (float)(gx + 1) / sub, t1 = (float)(gy + 1) / sub;
                V3 p00 = quadpt(rA, rB, rC, rD, s0, t0);
                V3 p10 = quadpt(rA, rB, rC, rD, s1, t0);
                V3 p11 = quadpt(rA, rB, rC, rD, s1, t1);
                V3 p01 = quadpt(rA, rB, rC, rD, s0, t1);
                int x00, y00, x10, y10, x11, y11, x01, y01;
                project3(p00, FOCAL, ZOOM, &x00, &y00);
                project3(p10, FOCAL, ZOOM, &x10, &y10);
                project3(p11, FOCAL, ZOOM, &x11, &y11);
                project3(p01, FOCAL, ZOOM, &x01, &y01);

                float u0 = s0 * TEX, u1 = s1 * TEX, v0 = t0 * TEX, v1 = t1 * TEX;
                tritex(x00, y00, u0, v0,  x10, y10, u1, v0,  x11, y11, u1, v1);
                tritex(x00, y00, u0, v0,  x11, y11, u1, v1,  x01, y01, u0, v1);

                if (wire) {
                    tri(x00, y00, x10, y10, x11, y11, CLR_WHITE);
                    tri(x00, y00, x11, y11, x01, y01, CLR_RED);
                }
            }
        }
    } else {
        print("(spun past the camera -- ease tilt)", 4, SCREEN_H / 2, CLR_WHITE);
    }

    print("YOUR FIRST 3D", 4, 4, CLR_WHITE);
    print("rot3() -> project3() -> tritex()", 4, 13, CLR_LIGHT_GREY);
    print(str("yaw %.0f  pitch %.0f  sub %dx%d", yaw, pitch, sub, sub),
          4, SCREEN_H - 18, sub == 1 ? CLR_ORANGE : CLR_LIME_GREEN);
    print("l/r spin  u/d tilt  Z wire  X subdivide", 4, SCREEN_H - 9, CLR_LIGHT_GREY);
}
