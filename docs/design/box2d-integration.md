# Box2D v3 integration — the rigid-body experiment

> **Status: SHIPPED (2026-07-14) — vendored + four-target compile gate PASSED; two carts live
> (`tumble` = destruction, `buggy` = a hill-climb vehicle).** This is the "does rigid-body physics earn a place in dreamengine" experiment
> from [physics-notes.md](physics-notes.md)'s rigid-vs-particles fork. The library is in and
> proven portable; the open question is whether a cart built on it *feels* worth the dependency
> (vs the code-first `physics.h` verlet path we already ship).

## Why a vendored lib here, a cart for fluids

The decision comes from [physics-notes.md](physics-notes.md) → "The technical fork" and "Fluids
— a third world": **rigid bodies are the one case where hand-rolling is genuinely hard** (SAT
contact manifolds, stable stacking, friction, tunnelling — the whole Box2D machinery), so an
off-the-shelf lib is justified. Box2D **v3** (the 2024 pure-C rewrite) is the right lib because
it drops into the C cart-land model ([ADR-0006](../decisions/0006-library-carts-not-engine.md)) with no C++ shim. (Fluids
invert this — hand-rolling PBD/SPH is easy and on-grain, and the only lib, LiquidFun, is a dead
C++ fork. So fluids = a `physics.h` cart, not a dependency.)

## What's vendored

`runtime/box2d/` — Box2D **v3.1.1**, `include/` + `src/` + `LICENSE`, copied unmodified (see its
`README.md`). Pure C, MIT, zero OS deps. Built per-target by `tools/build-box2d.sh` into
`build/box2d/<target>/libbox2d.a` (gitignored — rebuild on demand, like any build output).

**Cart-land helper: `runtime/boxrig.h`** — the shared "sprite region → ≤8-vert Box2D polygon +
texture" toolkit (the *puppetmaker* representation: a body part = one convex poly of ≤8 `(x,y)`
points + a texture). Four jobs: `boxrig_hull(ox,oy,w,h,ppm,…)` convex-hulls a sprite rect's opaque
pixels down to Box2D's `B2_MAX_POLYGON_VERTICES` (feed to `b2MakePolygon`); `boxrig_draw(…)`
`tritex`-textures the polygon from its own verts so the paint exactly covers the collision shape;
`boxrig_point_in_body(…)` is the click-to-grab test (point inside a part's polygon); and
`boxrig_resolve_box/poly(…)` (only when `physics.h` is included first) do the verlet↔Box2D coupling —
push a verlet point out of a rigid shape and return the contact so the cart picks its reaction-impulse
policy. The cart keeps body/shape/joint + reaction policy. Used by `puppet` (marionette + lamp),
`boxlab` (self-righting), and `boxjelly` (verlet blobs + crates + puppet in one world). Full contract
in the file header. The carts built on it + the "what is this for" question:
[box2d-puppets-and-playground.md](box2d-puppets-and-playground.md).

## The four-target compile gate (evidence, 2026-07-14)

A `.a` is never portable across platforms — but the *source* is, because Box2D touches no OS
surface. All four toolchains this repo ships compile the 35 sources cleanly:

| Target | Toolchain | Flags that matter | Result |
|---|---|---|---|
| macOS arm64 | `clang` | `-std=gnu17` | 35/35 · smoke test settles at y=0.4999 |
| Windows | `x86_64-w64-mingw32-gcc` | `-std=gnu17` | 35/35 |
| iOS arm64 | `clang` + `xcrun --sdk iphoneos` | `-arch arm64 -miphoneos-version-min=13.0` | 35/35 |
| web/wasm | `emcc` | `-std=gnu17 -msimd128 -msse2` **or** `-DBOX2D_DISABLE_SIMD` | 35/35 (both) |

Two flags earned their place (both baked into `build-box2d.sh`):

- **`-std=gnu17`, not `-std=c17`.** `src/timer.c` calls POSIX `clock_gettime`; strict ISO C
  hides it (emscripten was the strict one that failed — mac/ios/mingw headers exposed it anyway).
- **wasm SIMD.** Box2D's `core.h` maps the WASM CPU to the **SSE2** code path and relies on
  emscripten's SSE2→wasm-simd emulation, which needs **`-msimd128 -msse2`** together. The
  alternative is `-DBOX2D_DISABLE_SIMD` (scalar `B2_SIMD_NONE` path) — fully portable, marginally
  slower; fine for a fantasy-console cart.

Reproduce: `tools/build-box2d.sh --check` (mac + smoke test), `--win`, `--wasm`, `--ios`.

## Wiring a cart against it (not built yet)

A cart would `#include "box2d/box2d.h"` after `studio.h`, and its build line adds
`-I runtime/box2d/include build/box2d/<target>/libbox2d.a`. The **default cart pipeline
(`tools/make-cart.js`, `editor/electron/main.cjs`) is deliberately NOT modified** — box2d is
opt-in so it never bloats the other ~450 carts' compile. The demo cart wires it via its own
build path (mirror `tools/build-nr.sh` / `build-app.js`), or we add a `--box2d` flag to the
builders when the cart lands.

Minimal working API (from the smoke test, `runtime/box2d/smoke-check.c`):

```c
b2WorldDef wd = b2DefaultWorldDef();  wd.gravity = (b2Vec2){0, -10};
b2WorldId world = b2CreateWorld(&wd);
b2BodyDef bd = b2DefaultBodyDef();    bd.type = b2_dynamicBody;  bd.position = (b2Vec2){0, 8};
b2BodyId body = b2CreateBody(world, &bd);
b2Polygon box = b2MakeBox(0.5f, 0.5f);
b2ShapeDef sd = b2DefaultShapeDef();  b2CreatePolygonShape(body, &sd, &box);
// each frame:
b2World_Step(world, 1.0f/60.0f, 4);
b2Vec2 p = b2Body_GetPosition(body);   // read back to draw
```

## Two gotchas the cart author must handle

1. **Coordinate systems differ.** Box2D is **metres, y-up**, gravity `-y`; studio.h is
   **pixels, y-down**. The cart needs a world↔screen transform: pick a pixels-per-metre scale
   (e.g. 32), and flip y (`sy = SCREEN_H - wy*PPM`). Box2D docs warn against huge/tiny sizes —
   keep bodies roughly 0.1–10 m, so scale the scene, don't feed it pixel coordinates.

2. **Not deterministic across platforms → not lockstep-safe out of the box.** Box2D v3's SIMD
   width differs by target (AVX2=8, SSE2/NEON=4, scalar=1), and float results differ with it. So
   a box2d cart is **not** bit-identical mac↔web↔iOS and can't feed `net.h` lockstep netplay
   (`tools/net-check.js`) without forcing one code path (`-DBOX2D_DISABLE_SIMD` everywhere) and
   verifying — and even then Box2D makes no cross-platform determinism guarantee. This is a real
   mark against it *if* multiplayer rigid-body is ever wanted; single-player toys are unaffected.

## The demo to build (the actual experiment)

Pick a task **verlet is bad at**, so the A/B against `physics.h` is honest — **crate stacking**
or **dominoes** (stable resting contact + friction; `physics.h`'s pseudo-boxes wobble and sink
there). Ship it under the two-part bar ([ADR-0022](../decisions/0022-collaboration-is-the-north-star.md)):
it must settle correctly *and* be a delight to knock over. Then sit with both feels and decide:
does this earn the Layer-1 opaque API + backend seam (physics-notes.md), or just live as one
more optional cart-land library?
