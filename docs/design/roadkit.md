# roadkit.h — when streetlab/roadlab reach the drivable world

STATUS: READY TO BUILD (2026-07-01). The decision + plan to extract the shared road-rendering &
junction grammar into a cart-land library header (`runtime/roadkit.h`, per [ADR-0006](../decisions/0006-library-carts-not-engine.md)),
so `streetlab` (at-grade bench), `roadlab` (interchange bench) and `sloop` (the *drivable* world) all
call ONE implementation. Sits under [`driving-world-program.md`](driving-world-program.md) (the umbrella)
and [`road-program-state.md`](road-program-state.md) (which long said "no roadkit.h *yet*" — this doc is
the "now"). Pairs with [`field-based-road-rendering.md`](field-based-road-rendering.md) (the renderer
roadkit adopts) and [`road-hierarchy-notes.md`](road-hierarchy-notes.md) (the grammar it holds).

## The question this answers

We built beautiful, spec-locked road geometry in `streetlab` (curb-return fillets, turn lanes,
sidewalks, crosswalks, mini-roundabouts, typed cross-sections, road markings — 104 assertions) and
`roadlab` (the grade-separated interchange catalogue). **None of it is used by the thing you actually
drive.** `sloop`'s Rung B renders the real road network as flat coloured ribbons (`rectfill_rot` per
segment) and buildings as plain boxes — zero curb returns, no pavements, no markings. So: *when does
the grammar stop being research and become the world?*

## The insight that reframes it — a scale mismatch

`streetlab` renders ONE junction filling most of a 320×200 screen. When you *drive* in `sloop` you see
many blocks at once, so each junction is a handful of pixels and a curb-return fillet or a lane dash is
**sub-pixel — invisible while moving.** So the full grammar's payoff is at *close range*: a slow /
parked **street-camera** tier (the "sloop street-camera" [`roadnet-streetlevel.md`](roadnet-streetlevel.md)
sketches) or the pseudo-3D `citydrive` view down among the buildings — **not** the fast top-down drive.
Rendering streetlab-grade junctions across a whole scrolling city every frame is also a per-pixel-fill
cost problem, gated on [`software-canvas.md`](software-canvas.md) — which is exactly why the field-render
cutover is parked. **Conclusion: aim the fidelity where it's visible, not behind a fast camera.**

## The three paths (and the call)

1. **Cheap street-dressing in `sloop` now** *(independent of roadkit, days).* Using only the segments
   sloop already has: centre-line dashes, class-based lane widths, a pavement band alongside each ribbon,
   casing edges. ~70% of the "this is a real street" feel *at driving zoom*, for little effort. Does NOT
   touch junction geometry (sub-pixel while moving anyway). **Do this in parallel; not blocked on roadkit.**
2. **`roadkit.h` — the real integration** *(the architecturally correct move; this doc).* Extract the
   grammar so the drivable world (at a street-camera tier) renders through streetlab/roadlab's actual
   geometry. The thing we've circled for a while — **decision: bite the bullet, start Phase 1 now.**
3. **A zoom / 3D tier** that renders the full grammar only when close enough to see it — the *consumer*
   of roadkit #2, not a rival. Comes after roadkit exists.

## Why now (overriding the old "not yet")

`road-program-state.md` and the umbrella's build-order note said: *extract only once a consumer needs
BOTH renderers from one place — then, not now.* That condition is **now met.** `sloop`'s Rung B is a
real third consumer (it drives real OSM Delft and will want junction detail at close range), and the
OSM work already surfaced the **★ rung-3 finding**: streetlab assumes collinear arm-PAIRS, but real
nodes are **N independent arms** — the per-arm casing path can't express them; only the **field path**
(`fr_render`/`fr_arm`, already N-arm-native) can. So the trigger fired: the N-arm-native renderer *is*
roadkit's renderer. Extracting now designs the interface from a working consumer, not a guess.

## What roadkit.h holds

- **The leg model** — a junction as N arms at bearings (no 90°/collinear assumption).
- **The curb-return solver** — tangent-arc fillets (`curb_return`, `count_corners`, `arm_face`).
- **The typed cross-section** — `cross_hw()` = Σ lanes; median / bike / parking; every primitive keys
  off HW so widening re-solves the junction.
- **The field renderer** — one lateral distance field, every feature (asphalt / kerb / sidewalk /
  markings / crosswalk) a threshold on it. This is the N-arm-native path AND scale-invariant. It is the
  [`field-based-road-rendering.md`](field-based-road-rendering.md) method, promoted to the shared home.
- **The interchange entry** — `roadlab`'s `make_junction(legs, type)` for grade-separated crossings,
  dispatched by grade (at-grade → streetlab grammar, grade-separated → roadlab), keyed on the same
  `(legs)` input from a seed OR from OSM (`bridge`/`layer`/`*_link` + shared-node topology).

## Consumers after extraction

- **`streetlab`** → a thin bench that calls roadkit + **keeps its `spec()` as the regression lock**
  (104 assertions pin roadkit's geometry; per [`spec.h`](../../runtime/spec.h)'s pattern a lib can expose
  a `roadkit_selfcheck()` the cart's `spec()` calls). The spec is the safety net for the whole move.
- **`roadlab`** → calls roadkit for the grade-separated family.
- **`sloop`** → at the street-camera tier, renders the visible junctions through roadkit; the fast
  top-down drive keeps the cheap ribbons (path #1). This is the moment the grammar becomes the world.

## Phasing (each step spec-gated; stop at any natural line)

1. **Pure-geometry extract (safe first step).** Move the pure fns (`curb_return`, leg model,
   `cross_hw`, corner counts) into `roadkit.h`; `streetlab` `#include`s it and calls them unchanged.
   **`spec` must stay 104/0** — a pure move, no behaviour change. This alone de-risks and gives a real
   interface to design the rest from. **← START HERE.**
2. **Field renderer into roadkit** as the N-arm-native render entry; `streetlab` renders through it
   (its opt-in `DE_FIELD_ROADS` path becomes the default source). `road-check` + `mirror-diff` gate it.
3. **Grade dispatch** — `roadlab` calls roadkit; one `roadkit_junction(legs, grade)` routes at-grade
   vs grade-separated. Fed identically from a seed or OSM.
4. **`sloop` consumes it** at a street-camera zoom tier (the visible-junction render), leaving the fast
   drive on ribbons.

## Risks / dependencies

- **Perf** — whole-city junction render is gated on `software-canvas.md`; but a *street-camera* renders
  FEW junctions, so Phase 4 is affordable well before the whole-city case. Scope roadkit's first sloop
  use to the close camera.
- **Keeping `spec` green** — the extraction's safety net; run `node tools/spec.js streetlab` after every
  step. If Phase 1 can't hold 104/0, the boundary is wrong — fix the seam, don't loosen the spec.
- **Scale-invariance** — roadkit must render at arbitrary world scale (streetlab: one junction fills the
  screen; sloop: tiny). The field renderer is naturally scale-free; the per-arm casing path is not — a
  reason to lead with the field path.

## Pointers

- Umbrella + the four bridges: [`driving-world-program.md`](driving-world-program.md)
- Whole road-tier state + the old "Shared code & seams" note this supersedes: [`road-program-state.md`](road-program-state.md)
- The renderer roadkit adopts: [`field-based-road-rendering.md`](field-based-road-rendering.md)
- Library-cart policy: [ADR-0006](../decisions/0006-library-carts-not-engine.md) · shared-selfcheck pattern: [`runtime/spec.h`](../../runtime/spec.h)
- Carts: `tools/carts/{streetlab,roadlab,sloop}.c`
