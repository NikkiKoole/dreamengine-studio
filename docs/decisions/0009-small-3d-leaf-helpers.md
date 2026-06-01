# 0009 — A small set of 3D leaf-helpers (rot3/project3/zsort/quadfill + V3)
Date: 2026-06-01 · Status: accepted · Refines: [0006](0006-library-carts-not-engine.md)

## Context
STATUS listed **"general 3D — out of scope."** That stance was right against a *3D
engine* (scene graph, mat4 stack, z-buffer, per-pixel depth). But a survey of the 11
3D/pseudo-3D carts found one family — `cube3d`, `solid3d`, `textured3d`, `flyover` —
re-deriving the **same five-step pipeline** (rotate → project → cull → painter-sort →
fill) by hand, every time. Concretely duplicated:

- an identical yaw+pitch `rotate()` (cube3d:33, solid3d:63, textured3d:42),
- the same `typedef struct {float x,y,z;} V3` in **4** carts (also elite, infiniminer),
- the perspective divide `f=focal/(focal+z)` in 4 carts,
- the painter's insertion-sort in **6+** carts (the single most-copied 3D idiom),
- quad-as-two-triangles in 4 carts.

## Decision
Ship **four leaf-helpers + one struct**, all riding on the existing `trifill`/`tritex`:

```c
typedef struct { float x, y, z; } V3;
V3   rot3(V3 p, float yaw, float pitch);
bool project3(V3 p, float focal, float zoom, int *sx, int *sy);
void zsort(float *key, int *order, int n);
void quadfill(int x0,int y0,int x1,int y1,int x2,int y2,int x3,int y3,int color);
```

This **refines** 0006 rather than overturning it: these are convenience *primitives* (a
rotate, a divide, a sort, a quad — same shelf as `dx`/`distance`/`trifill`), not an engine
or an algorithm-with-content. No z-buffer, no matrix stack, no scene graph.

## Why this and not more
`zsort` returns an **index order** (caller draws via `order[i]`), so it works for faces,
sprites, and billboards without a payload type. `project3` puts the origin at screen
centre and returns `false` behind the camera; per-cart framing nudges (e.g. solid3d's
`-4`) stay cart-side. We deliberately left out: normal/cull/Lambert shading (cull
threshold + light dir + shade mapping are cart *policy*), the road/mode-7/raycaster
scanline renderers (welded to per-cart texture/curve state), and any Vec3/mat4 library.

## Naming note
The obvious name `project` collided with a **local `project()` in 8 carts** (doom, elite,
flyover, mode7, outrun, raycaster, racer, podracer). Rather than rename eight carts, the
engine call is **`project3`** — collision-free, and it pairs with `rot3`. A reminder that
new short API names must be checked against existing cart locals before landing.

## Consequences
- `cube3d`/`solid3d`/`textured3d`/`flyover` refactored to use the helpers (verified:
  identical baked output). `elite`/`infiniminer` keep their own projection but drop the
  duplicate `V3` typedef.
- The 8 carts with a local `project()` are untouched and still compile.
- STATUS "general 3D" cut line is narrowed to "no 3D *engine*"; these helpers are shipped.
- Pseudo-3D scanline renderers (road, mode-7, raycaster) remain firmly cart-side.
