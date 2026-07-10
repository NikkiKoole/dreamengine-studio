# roadkit.h — when streetlab/roadlab reach the drivable world

STATUS: BUILDING (2026-07-01) — Phase 1 (connectivity: one connected asphalt surface + bridges/tunnels)
**and Phase 2 (cheap street-dressing: light pavement/kerb bands + dashed white centre-line markings, class-based
widths, + an in-cart junction/node graph)** shipped in `citydrive`. **B2 SHIPPED 2026-07-10: `runtime/roadkit.h` extracted** — the pure junction geometry (curb_return, edge_corner, rk_count_corners, rk_cross_hw) now lives in the shared header; streetlab calls it, spec 104/0 + mirror-diff + road-check all green (see Phasing 3). Next: B3 the field renderer into citydrive + B4 grade dispatch. The decision + plan to extract the shared road-rendering &
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
2. **Cheap street-dressing in citydrive — ✅ DONE (2026-07-01).** Ground decals on the now-connected
   projected roads, all riding the existing per-layer sweep (so junctions still fuse): a light
   **pavement/kerb band** (`CLR_LIGHT_GREY`, drawn widest-first before the casing) on urban street-tier
   roads only (`has_pavement()` — arterial/road/secondary/tertiary; motorways & tracks/service stay bare),
   and a **dashed white centre-line** (`paint_markings()`, drawn last over the asphalt) on carriageways
   wide enough for a lane division (`hw_m >= 2.5m`). Widths were already class-based (`ROAD[].hw_m`). Reads
   as "a real street" in the heading-up/near-top-down view; validated the ground-decal approach before any
   extraction. **Zoom LOD:** both passes gate on effective px-per-metre at the focus (`eff` = flat-mode
   `zoom`, or `zoom*90/setback` in perspective) — pavement drops below `eff 0.5`, markings below `eff 1.2`
   — so zoomed-out roads render as clean thin lines instead of sub-pixel dash-noise (and save the fill). *Known artifacts (deliberately left for the field renderer):* centre-line dashes run
   straight across junctions (markings don't stop at nodes), lane markings are a single centre-line (no
   per-lane divisions inferred from width yet), and these decals are throwaway once #4 lands.

   **Junction/node model + crosswalks (2026-07-01).** citydrive gained an in-cart port of
   `data-tools/roadview/osm-junction.js`: `build_junctions()` runs once at load, quantizes every at-grade
   motor-road vertex into a heap-scratch node hash (`J_QUANT` 1 m grid), accumulates incident-arm bearings,
   dedups within 8° (incl. wrap-around), and keeps nodes with **≥3 distinct arms** as the compact resident
   `junc[]` (position + per-arm bearing + class). Verified **exact-match** with the offline extractor (Delft
   centre = 217 junctions both ways). **This node graph is the data layer every junction treatment needs** —
   the "the cart has no node model" gap the umbrella flagged is now closed on the OSM side.

   **First render consumer: voorrang / haaientanden (2026-07-01).** We know which road is bigger from the arm
   classes, so `draw_giveway()` ranks each junction's arms (`road_rank()`: motorway>arterial>secondary>tertiary
   >road>track>service = the Dutch "grotere weg gaat voor") and paints a row of white shark's-teeth (give-way
   B6) across each **minor** approach — apex pointing back at the yielding driver — with **nothing on the
   priority road**. When every arm is the same class it paints nothing (rechts-voor-links / roundabout is
   genuinely ambiguous without the signs we don't have). On Delft centre **72 of 217 junctions** have a clear
   priority and get teeth; the other 145 are equal-class and correctly stay bare. Same `lod_markings` zoom gate.
   *Rejected first consumer:* a zebra-crosswalk pass was prototyped off `junc[]` but **pulled** — a crossing on
   every walkable arm of every junction isn't where real crossings sit, and **crossings aren't in our data at
   all**: `osm-roads.js` only queries `node["natural"="tree"]`, so OSM's `highway=crossing` nodes (the marked
   VOPs) never reach the `.rvb`. Real crossings need the importer taught to carry crossing nodes; deferred.

   **Per-feature toggles + red cycleways (2026-07-01).** The dressing now has seams: `P`/`L`/`G` toggle
   pavement / lane-markings / give-way independently (master `R` still wraps the whole road layer) — the
   instrument for judging whether the cheap decals carry the look before committing to the field-path rewrite.
   Separate cycleway ways render as **Dutch red fietspaden** (a dark-red casing under a warm-red surface),
   so a bike path reads as its own ribbon alongside the road. **What the `.rvb` still can't give** (importer
   drops it, all cheap way-tags): `sidewalk=*` (the *real* per-street pavement, vs our class heuristic),
   `cycleway=lane` (on-road bike lanes — a big share of NL bike infra, invisible here), `oneway`/`lanes`
   (correct lane-count markings), `surface` (brick/klinker vs asphalt), `maxspeed` (30-zones), and
   `junction=roundabout`. Teaching `osm-roads.js` to preserve those six is the next high-leverage data step.

   **Importer widened — Tier-1 way-attributes (2026-07-01).** Done: `osm-roads.js` now packs a road's useful
   OSM tags into the existing `sub` string as a compact `;`-delimited token list (`roadSub()`) — **surface**
   (`U` a/p/g/w/o), **sidewalk** (`W` b/l/r/n), **oneway** (`O`), **lanes** (`L<n>`), **maxspeed** (`M<n>`),
   **on-road cycleway** (`C` b/l/r), **roundabout** (`R`) — with the bridge/tunnel token kept FIRST so
   `parse_deck` and every existing reader are unchanged (backward-compatible; old `.rvb`s still load and fall
   back). Re-fetched `data/delft-centre.rvb`: **surface on 1621 roads, maxspeed 622, sidewalk 406, oneway 354,
   lanes 161, on-road cycleway 39.** Consumers wired in `citydrive` (`sub_tok()` reads the tokens into
   `rways.surf`/`.sw`): **road surface now drives the carriageway colour** (`surf_col()` — brick/klinker roads
   render warm instead of grey; asphalt/concrete stay grey → a real Delft brick-street look with the arterials
   standing out), and **pavement uses the real `sidewalk` tag when present** (falls back to the `has_pavement`
   class heuristic only where untagged) — this retires the "we fake per-street pavement" gap. **On-road cycle
   lanes** (the `C` token) now render too: a red *fietsstrook* painted along the carriageway edge on the
   tagged side(s) (`paint_side_strip()`), revealing bike infra a separate-cycleway-ways-only render missed.
   **Lane markings** now use OSM `lanes` + `oneway` (`paint_markings`/`paint_lane_line`): a carriageway with
   N lanes draws N-1 dashed lane dividers instead of one centre-line, and one-way streets drop the centre-line
   entirely (378 of central Delft's roads are one-way — a big, correct change).

   **Carriageway width model (2026-07-02).** Width was the last class-only fake — every `road` drew 6 m wide
   regardless. `road_hw(w)` now decides it: OSM `width` tag (authoritative, ~1% coverage) → `lanes`×~3 m →
   else the class default, **narrowed to ~0.6× for one-way residential-tier streets** (one direction ≈ one
   lane; `oneway` has real ~20% coverage, the workhorse signal). The freed width isn't lost — the pavement
   pass draws out to the *fixed class corridor*, so a narrowed one-way street automatically gets a **fatter
   pavement** (the real Dutch centre look: narrow one-way carriageway, wide footway, same building-to-building
   corridor). All the carriageway passes (casing, surface, markings, on-road cycle lane) key off `road_hw`;
   only the pavement's outer edge stays at the class corridor.

   **Node-level control tier (2026-07-02).** `osm-roads.js` now fetches the control *nodes*
   (`highway=crossing`/`give_way`/`stop`/`traffic_signals`, `traffic_calming`) and emits them as new POINT
   kinds (24–28, appended to `KIND_IX` + the `K_*` enums in citydrive **and** roadview). citydrive stores them
   in `pnode[]` and **renders real crossings** as a zebra at the OSM node, oriented across the nearest
   carriageway (`draw_zebra`/`nearest_motor_dir`) — the marked-crossing render done from data, replacing the
   pulled per-arm guess. Delft centre: 43 crossings / 92 signals / 43 give-way / 57 calming.

   **Give-way/stop → haaientanden (2026-07-02).** `build_junctions()` now attaches each real give-way/stop
   node to the junction arm it sits on (nearest junction ≤35 m, best-aligned arm), setting a per-arm `yield`
   bitmask + `realctl` flag. `draw_giveway` uses that ground truth where present (teeth on exactly the arms
   with a give-way node, priority road bare) and falls back to the class-priority inference elsewhere — 21 of
   Delft centre's junctions are now real-data-driven, the rest inferred. **Next:** a traffic-signals marker.
   needs new kind indices (enum surgery in `citydrive.c` + `roadview.c`), so it's the next importer chunk.
3. **Pure-geometry extract (safe first roadkit step). ✅ SHIPPED 2026-07-10 (B2).** `runtime/roadkit.h`
   now holds the pure param-driven primitives — `curb_return` (the M1 fillet), `edge_corner` (the corner
   solver), `rk_count_corners(brg,n)` (corner count from a **bearings array** — no more streetlab-legs
   coupling, so citydrive/citygen pass their own arms) and `rk_cross_hw(...)` (the typed-section sum) —
   plus the snap-safe `rk_ux`/`rk_uy`/`rk_ri`. `streetlab` `#include`s it: its `curb_return`/`edge_corner`
   are gone, `count_corners`/`cross_hw` are thin delegating wrappers over its own knobs. **Gates all
   green: `spec` 104/0, `mirror-diff` byte-identical to baseline (68=68), `road-check --all` all PASS.**
   The `ux`/`uy` divergence was handled as flagged — roadkit carries streetlab's **snapping** version
   (kept its spec exact); roadlab's non-snapping form reconciles when it adopts roadkit at B4.
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
