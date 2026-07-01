# roadkit.h — when streetlab/roadlab reach the drivable world

STATUS: BUILDING (2026-07-01) — Phase 1 (connectivity: one connected asphalt surface + bridges/tunnels)
shipped in `citydrive`; markings/pavements next, then the `roadkit.h` extract. The decision + plan to extract the shared road-rendering &
junction grammar into a cart-land library header (`runtime/roadkit.h`, per [ADR-0006](../decisions/0006-library-carts-not-engine.md))
so `streetlab` (the spec-locked *source*), `roadlab` (interchange bench) and — **the first render consumer
— `citydrive`** (the pseudo-3D real-OSM view where the grammar is actually *visible*) all call ONE
implementation. **`citydrive` leads, not `sloop`:** it's already playable, already loads OSM, and is the
close/pitched view where markings & curb returns aren't sub-pixel; sloop's rig hooks in later via the
shared `road_at()` seam. Sits under [`driving-world-program.md`](driving-world-program.md) (the umbrella)
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

## ★ The first consumer is `citydrive`, not `sloop` (decided 2026-07-01)

The scale-mismatch conclusion has a name, and it's a cart that already exists: **`citydrive`** — the
pseudo-3D, pitched, *close* view that already loads the same OSM `.rvb` and is already drivable. That is
precisely where the grammar (markings, pavements, curb returns) is **visible** rather than sub-pixel. So
the geometry-first road-rendering story leads in citydrive, not in sloop's fast top-down drive:

- **citydrive is the natural home for the render** — ADR-0021 says geometry is authored **2D on the
  ground plane** and the pseudo-3D view is the **adapter**; citydrive *is* that adapter, proven over
  arbitrary OSM polygons + terrain. Lane lines, zebras, pavement/kerb bands are 2D ground decals that
  project into its view for free. Its roads today are flat class-coloured quads (`road_seg`) — the exact
  crude state sloop's ribbons are in, but with the close camera that makes the fix *pay off on screen*.
- **It's already playable and already loads OSM** — the cheapest place to make "our roads are
  geometry-first, with real markings" real and demoable. Strengthens the geometry/rendering story now.
- **The cool car hooks back in later, cheaply** — `road_at()` + sloop's grip/collision physics are
  producer- AND renderer-agnostic (the seam is clean), so once the rendering is proven in citydrive,
  either bring that ground-plane render to sloop's top-down or bring sloop's rigid-body rig into
  citydrive. Both are wiring against the shared seam, not a rewrite.
- **Honest tradeoff:** citydrive has **no rig physics** today (it's a camera drive, not sloop's
  grid-of-parts body). So citydrive-first means the geometry/markings showcase leads and the deep car
  *feel* waits. That's the right order when the goal is the geometry-first roads story.

## The three paths (and the call)

0. **Connectivity FIRST — the foundation** *(days, no roadkit).* citydrive draws each segment as an
   independent quad (`road_seg`), so roads **facet on bends** (outer-gap) and **pile at junctions** (no
   merge). Before any dressing: draw a draped **disc of radius = half-width at every polyline vertex +
   endpoint** — one primitive that rounds the bends AND merges the meeting ways at each shared node into
   a connected (blobby, pre-grammar) junction. Markings on disconnected roads look broken, so this comes
   *before* #1. **← START HERE.**
1. **Cheap street-dressing — in `citydrive`** *(days, no roadkit; after #0).* Centre-line + lane
   markings on the now-connected projected roads, then class-based widths, then pavement/kerb bands.
   ~70% of the "this is a real street" feel **where it's visible** (the close pitched camera). Mirrorable
   onto sloop's ribbons later.
2. **`roadkit.h` — the real integration** *(the architecturally correct move; this doc).* Extract the
   grammar so **citydrive** (then, later, sloop) renders through streetlab/roadlab's *actual* geometry —
   curb returns, the leg model, the typed cross-section. The thing we've circled — **decision: GO**,
   designed from citydrive as the working consumer. **The field renderer stays on the table as the clean
   successor** (below) — it makes #0's connectivity, #1's markings, and the curb-returns all *thresholds
   on one distance field*, superseding the cheap disc-joins rather than layering on them.
3. **`sloop` gets it later** — either the ground-plane render brought to its top-down (mostly wasted
   at driving zoom → likely only its own street-camera tier) or, more likely, **sloop's rig driven
   inside citydrive**. Comes after the render is proven; cheap because of the shared seam.

## Why now (overriding the old "not yet")

`road-program-state.md` and the umbrella's build-order note said: *extract only once a consumer needs
the grammar as a callable renderer from a place that isn't its bench — then, not now.* That condition is
**now met.** The OSM work (sloop drives real Delft; citydrive extrudes real Delft) surfaced a real
consumer *and* the **★ rung-3 finding**: streetlab assumes collinear arm-PAIRS, but real nodes are **N
independent arms** — the per-arm casing path can't express them; only the **field path**
(`fr_render`/`fr_arm`, already N-arm-native) can. So the trigger fired: the N-arm-native renderer *is*
roadkit's renderer. Extracting now designs the interface from **citydrive as the working consumer**
(its projected ground plane), not a guess.

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

- **`streetlab`** → stays the *source of truth*: a thin bench that calls roadkit + **keeps its `spec()`
  as the regression lock** (104 assertions pin roadkit's geometry; per [`spec.h`](../../runtime/spec.h)'s
  pattern a lib can expose a `roadkit_selfcheck()` the cart's `spec()` calls). The spec is the safety net
  for the whole move — it never becomes the drivable *world*, it *guards* the grammar the world uses.
- **`citydrive`** → **the first RENDER consumer.** Its flat `road_seg` quads become geometry-first
  ground (markings → pavements/kerbs → curb-return junctions) rendered through roadkit and projected by
  its existing pseudo-3D adapter. This is the moment the grammar becomes something you drive through.
- **`roadlab`** → calls roadkit for the grade-separated family.
- **`sloop`** → later, via the shared `road_at()` seam: either the ground-plane render mirrored onto its
  top-down (likely only a street-camera tier, since it's sub-pixel at speed) or — more likely — its
  rigid-body rig driven inside citydrive. Not on the critical path for the geometry story.

## Phasing (each step gated; stop at any natural line)

1. **Connectivity in citydrive — ✅ DONE (2026-07-01).** citydrive already had disc-joins (rounds bends,
   merges junctions); the missing piece was that per-class NEON fills made junctions a colour-patchwork, so
   motor roads now render as **one connected asphalt surface** (dark casing pass + `DARK_GREY` fill,
   hierarchy by width), bike/foot/canal keep own colour. Plus: canals draw **under** roads, and real OSM
   **bridges** (raised decks) / **tunnels** (dashed) landed as a bonus of the same pass structure
   (osm-roads carries bridge/tunnel/layer; see [`external-data-carts.md`](external-data-carts.md)). Roads
   read as a coherent network — the foundation everything else sits on.
2. **Cheap street-dressing in citydrive** *(days) — ← NEXT.* Markings + widths + pavement/kerb bands on the
   now-connected projected roads. Visible payoff; validates the ground-decal approach before extraction.
3. **Pure-geometry extract (safe first roadkit step).** Move the pure fns (`curb_return`, the leg model,
   `cross_hw`, corner counts) into `roadkit.h`; `streetlab` `#include`s it and calls them unchanged.
   **`spec` must stay 104/0** — a pure move, no behaviour change. De-risks + gives a real interface.
   (Note: even `ux`/`uy` differ between streetlab (near-zero snap) and roadlab (none) — roadkit needs a
   deliberate canonical form; don't blind-copy.)
4. **Field renderer into roadkit** *(KEPT ON THE TABLE — the clean successor).* One lateral distance
   field: asphalt = within half-width of the nearest centerline → connectivity #1, markings #2, and
   curb-returns all become **thresholds on the field**, replacing the disc-joins + decals with the
   uniform method. N-arm-native; **citydrive** renders its ground through it (streetlab's
   `DE_FIELD_ROADS` path is the reference). Per-pixel cost gated on `software-canvas.md` but affordable
   at citydrive's near field. `road-check` + `mirror-diff` gate it.
5. **Grade dispatch** — `roadlab` calls roadkit; one `roadkit_junction(legs, grade)` routes at-grade vs
   grade-separated. Fed identically from a seed or OSM.
6. **`sloop` gets it** — the car ∪ the render, via the shared seam. The easy, last step.

## Risks / dependencies

- **Perf** — whole-city junction render is gated on `software-canvas.md`; but citydrive's **pitched
  camera only shows a handful of junctions near the player**, so the geometry render is affordable there
  well before the whole-city top-down case. Scope roadkit's first use to citydrive's near field.
- **Keeping `spec` green** — the extraction's safety net; run `node tools/spec.js streetlab` after every
  step. If the pure-geometry extract can't hold 104/0, the boundary is wrong — fix the seam, don't loosen the spec.
- **Scale-invariance** — roadkit must render at arbitrary world scale (streetlab: one junction fills the
  screen; citydrive: projected + near/far). The field renderer is naturally scale-free; the per-arm
  casing path is not — a reason to lead with the field path.
- **Pseudo-3D projection** — citydrive renders ground decals through its `project()` adapter (ADR-0021);
  roadkit emits the grammar as **2D ground geometry** and citydrive projects it, so roadkit stays
  projection-agnostic (the same 2D output a future top-down consumer would use).

## Pointers

- Umbrella + the four bridges: [`driving-world-program.md`](driving-world-program.md)
- Whole road-tier state + the old "Shared code & seams" note this supersedes: [`road-program-state.md`](road-program-state.md)
- The renderer roadkit adopts: [`field-based-road-rendering.md`](field-based-road-rendering.md)
- Library-cart policy: [ADR-0006](../decisions/0006-library-carts-not-engine.md) · shared-selfcheck pattern: [`runtime/spec.h`](../../runtime/spec.h)
- Carts: `tools/carts/{streetlab,roadlab,sloop}.c`
