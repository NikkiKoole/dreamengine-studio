# 0036 — Depth-tested triangles and a near-plane clip, not a 3D engine
Date: 2026-09-25 · Status: accepted (not built) · Refines: [0009](0009-small-3d-leaf-helpers.md)

## Context
STATUS's cut list rules out a **3D *engine*** — "scene graph / mat4 stack / z-buffer / per-pixel
depth". [ADR-0009](0009-small-3d-leaf-helpers.md) shipped the leaf-helpers under that line
(`V3`/`rot3`/`project3`/`zsort`/`quadfill`) and left lighting, culling and shade mapping as cart
**policy**.

A larger idea was put on the table first: a "fantasy PS1" layer with meshes, built-in per-vertex
lighting, fog, a triangle budget and a Metal backend. Before deciding anything we asked what the
carts actually suffer from. On 2026-09-25 we audited every cart that draws triangles or projects
points. The method, the numbers and the per-cart table are in
[`design/depth-tested-triangles.md`](../design/depth-tested-triangles.md) §"The evidence". In short:

- **The sort is cheap.** It costs ≤2.3% of `draw()` in every cart except `polyroom`, where it costs
  19%. That is an insertion sort at n≈630.
- **Real sorting errors are few, and they are a CLASS no sort order fixes.**
  - `citydrive` paints walls across their own roofs on complex footprints. Drawing the roofs last
    breaks tall towers instead, so no fixed order works.
  - `dreamcad` gives an order-dependent result for two objects that overlap.
  - `polyroom` shows 1–2% wrong pixels once its subdivision is turned off.
  - `flyover`'s props are never tested against the terrain.
  - `outrun`/`racer` drop a whole sprite past a hill crest, because they cannot hide part of one.
- **What sorting really costs is authoring rules.** Polygons are subdivided to about a tile (+53%
  triangles in `polyroom`), "parts must abut, never interpenetrate", furniture is redesigned, and a
  swap to an unstable sort once made `tenement`'s wall corners strobe. `tenement` then wrote its
  own depth buffer in cart land and deleted all of these rules.
- **The other common defect is the NEAR PLANE, not ordering.** `project3` rejects a point behind
  the camera, so a triangle crossing the near plane is dropped whole or distorted. That is visible
  in `citydrive`, `infiniminer` and `26-first3d`. A depth buffer does not fix it.
- **The software rasterizer is not the bottleneck any more.** The 10fps iPhone figure for `tritex`
  was overturned on 2026-07-02 (`podracer` ~6ms, `infiniminer` ~11ms on an A13, Debug build;
  [ADR-0024](0024-software-canvas-is-canonical-for-2d.md) Update). So performance argues for no GPU
  3D path.

## Decision
Narrow the cut line. **Ship two ADR-0009-style leaf primitives, and nothing else:**

1. **Depth-tested triangles.** `trifill_z` and `tritex_z` are `trifill`/`tritex` plus one depth per
   corner, and `sspr_z` is `sspr` at one depth. The depth buffer is canvas-sized and allocated on
   first use, so a 2D cart pays nothing: no memory and no per-pixel work.
2. **A near-plane clip.** One camera-space polygon clip against a minimum depth, returning at most
   n+1 vertices with their UVs. The cart projects the result with `project3` exactly as it does
   now.

Both are rasterized by the **software** path on every platform, and the software result is the
definition of correct. The engine does not grow a lighting model, fog, a mesh format, a triangle
budget, a camera, or a GPU 3D backend.

## Why this and not more
- **It removes the one thing a cart cannot fix itself.** Per-pixel occlusion needs a buffer the
  cart cannot reach efficiently. Lighting, fog and meshes are already doable in cart land (face
  colour, `pal()` ramps, `blend()`, C arrays), and they are where carts differ. A shared lighting
  model is the `hud()` trap one level up: every 3D cart would get the same ramps.
- **It deletes constraints instead of adding API weight.** No subdivision for sortability, no
  must-abut rule, no stability trap, and sprites can live inside 3D terrain. The teaching carts
  (`cube3d`, `solid3d`, `26-first3d`) keep showing the pipeline by hand, because nothing here hides
  it.
- **It keeps pixel precision cheap.** Screen vertices are already integers (`tritex` takes `int`),
  so coverage is exact in float. Depth is plain `+ - * /` under `FP_CONTRACT OFF`, which is the
  basis of the cross-platform determinism in [`design/determinism.md`](../design/determinism.md).
  One software definition means there is no GPU parity to tolerate.

## What stays out, and what would bring it back
- **Built-in lighting / fog / palette-ramp shading** comes back only when three or more carts have
  hand-rolled the same code on top of the depth test. That is the rule ADR-0009 used for the
  pipeline helpers.
- **A palette shade table** (`shade(c, level)` from the live palette, built the way the blend tables
  are) is a separate, 2D-useful candidate. It is not part of this decision.
- **A triangle budget** is out: it has no evidence, and it would break `infiniminer` (~3.9k
  triangles).
- **A GPU/Metal 3D path** comes back only if a measured cart misses its frame budget on the software
  path. Even then it must match the software output byte for byte, which is realistic only as an
  integer compute port. "Within a tolerance" is ruled out.
- **Scene graph, matrix stack, skeletal animation, shadow maps and GPU shaders** stay cut.

## Consequences
- The STATUS cut line changes to "no 3D engine — *except* the depth test + near clip of ADR-0036".
- New engine state (the depth buffer) must be **per instance**: it lives in the context struct,
  resizes with `de_resize`, and is classified for `ctx-gen --verify`.
- The four-place API rule applies (studio.h / studio.c / studioDocs.js / shell.js), plus
  `drawall.c` for the three draw calls.
- This ships only with a **det-probes gate** and a **trifill-equivalence gate**, because correctness
  is the whole point. See the design doc §Verification.
- `polyroom`'s own note says its cart-land depth path is faster than sorting. The audit measured it
  ~13% *slower* (0.38 vs 0.34ms), probably because the builds differed. The engine version must be
  measured on its own terms, not assumed from either figure.
