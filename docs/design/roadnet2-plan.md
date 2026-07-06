# roadnet 2 — the vector-native rebuild (plan / sketch, 2026-06-15)

STATUS: BUILDING — design sketch for the vector-native v2 rewrite; foundation in place; hierarchy + interchange ongoing.

A clean restart of roadnet's street level, keeping the elegant L0 spline-highway core and
fixing the one architectural mistake that made v1 messy. **This is a design sketch, not built
yet.** Cart: [`tools/carts/roadnet2.c`](../../tools/carts/roadnet2.c) (currently = v1's clean
rung-1..3 baseline, commit `c16c9a2`, verbatim — the foundation). v1 lives on at
[`roadnet.md`](roadnet.md) / [`roadnet-streetlevel.md`](roadnet-streetlevel.md) /
[`roadnet-handoff.md`](roadnet-handoff.md) as the reference + testbed (don't delete it).
**The generation-realism work on top of this spine (density field, tensor-field arterials,
per-district fill, rivers/rails, the SNDi oracle) is specced as a rung ladder in
[`worldgen-plan.md`](worldgen-plan.md)** — this doc stays the substrate plan (scale, cameras,
the junction matrix).

## Why a v2 (the one lesson)
v1 grew **two parallel representations of the streets** that then diverged:
- a **field** — `grid_at`/`street_axis`/`road_at`, sampled per-pixel by `render_streetlevel`
  (the loupes), and
- a **graph** — `gedge[]`/`gnode[]`, extracted *from* the field, then grown with buildings /
  stitch / cul-de-sacs **graph-only**.

By the end the loupes (field) and the GRAPH view (graph) disagreed about what streets exist
(cul-de-sacs were graph-only). That broke the "one source of truth, screen == collision"
invariant that made the L0 core elegant. Root cause: we committed to a modular `road_at`
**field** for the deeper roads too early, instead of extending the highways' own vector query
downward. The fix is to **stay in vectorland all the way down**.

Evidence it works: `arterial_at()` already answers "am I on a road, which class" by
**point-to-nearest-spline distance** over the highway polylines. That *is* a vector-native
`road_at`. v1's error was inventing a *second* mechanism for the deeper tiers instead of
reusing this one.

## Scale — the base unit (do this FIRST)
v1 had **no anchoring unit** — `block_sp` was "world tiles," footprints "0.10 tiles," LOD
thresholds "16px," and the car didn't exist. Every quantity was tuned in its own units against
nothing, which is *why* the road:building ratio felt off and the LOD thresholds were a soup.
v2 fixes this before generating anything:

**1 world unit = 1 metre.** Everything below is a real-world figure you can sanity-check against
intuition, and changing one (e.g. block size) rescales the rest *coherently*. Starting anchor
table (tune in-motion; not gospel):

| thing | metres | note |
|---|---|---|
| car | 4.5 × 1.8 | the moving reference (a *derived* size, not the unit) |
| lane | ~3.5 | right-of-way = lanes + verge |
| access / residential street | ~6 | 1 lane each way, tight — the finest tier |
| collector | ~10 | |
| arterial | ~16 | |
| highway | ~25–30 | multi-lane |
| house footprint | ~10 × 10 | |
| lot frontage | ~15–20 | → 4–6 houses on an 80 m block side |
| **city block** | **~80–110 / side** | the key feel unit |
| city (downtown → edge) | ~0.5–3 km | scope decision (see below) |
| inter-city gap | ~3–30 km | |

Two quantities fall straight out of metres and are the ones that matter:
- **px/m replaces arbitrary pixel thresholds.** Driving ≈ **4–5 px/m** (car ≈ 20 px); city
  overview ≈ 0.1 px/m; continental ≈ 0.001. LOD becomes principled: *draw a tier when its
  width ≥ ~2 px* (so streets appear at px/m ≥ ~0.3). No more magic 16/34/44.
- **m/s is the feel knob.** City driving ~14 m/s (50 km/h) → an 80 m block takes **~6 s to
  cross**. That single number — speed vs. block — is the core driving feel; you tune it live on
  the bare highways (build step 2). If 6 s feels long for the arcade feel, shrink blocks or raise
  speed — now a defensible decision, not a blind slider.

**Road half-widths, footprints, lattice spacings, and LOD are all expressed in metres / px/m
from here** — never re-tuned independently.

**Caveat → scope.** float32 holds ~7 digits, so a chunk ~10⁶ m (1000 km) from origin keeps only
~0.1 m precision (worse further out). Camera-relative rendering hides it on screen and the
generation hashes use integer cell indices (exact), but footprint geometry *far* out would
jitter. Fine for any realistically-explored region — but it's a reason to lean **bounded-city**
(one characterful place you roam, GTA1-style) over truly-infinite if you ever want sub-metre
detail everywhere. **Decide scope when you lock scale**; it sets the top of the ladder.
Leaning (not committed): **~500 km bound** is more than enough — no one drives 1000s of km in
this — and it sits comfortably under the float-precision wall, keeping sub-metre detail
everywhere. Good to know; defer the actual decision until scale is felt.

## Two cameras + GPS — the driving harness
You can't drive on the overview map; the drive view must be its own camera. The clean model is
**two cameras over the one (metre) world** — it's the v1 loupe idea *inverted* (main = street/
drive, inset = map):
- **DRIVE cam** — tight, follows the car (~4–8 px/m). True-proportion road + car, a faint
  **reference grid** (e.g. 100 m / 10 m lines) so you *watch the metres go by* — the scale
  device. A **GPS minimap** inset (corner) shows the car + nearby network.
- **MAP cam** — the overview (today's view): network + cities, free pan/zoom, schematic.

**Flow:** explore opens in MAP → **click a spot drops/teleports the car there** and enters DRIVE
→ drive with the GPS → **click the GPS (or M) → MAP fullscreen** → click elsewhere to fast-travel
→ DRIVE. This also **dissolves the LOD threshold soup**: each camera has ONE fixed detail level
(DRIVE = fine + near; MAP = network), no per-zoom pop. And it makes the car metre-real so 130 km/h
*feels* like 130. Click-to-go starts as **teleport/fast-travel** (great for probing scale); the
**routing** version (a highlighted route on the GPS) is a later layer on the same widget once the
graph is back. Forks to settle by feel: heading-up vs north-up GPS; teleport vs route.

## Highways as a proximity graph (β-skeleton) — done 2026-06-15
The backbone was a 4-connected lattice → every hub an at-grade crossing of 4–6 highways (a grid
of knots, not a highway network). Replaced with a **β-skeleton proximity graph** over the hubs:
link A–B only if no third hub C sits in the lune (`|CA|²+|CB|² < β·|AB|²`). β=1 = Gabriel (looped,
"highway map"); β→1.8 = RNG-ish (tree). Result: sparse, **branching corridors** with low-degree
junctions. Local + deterministic (gather hubs near the view with a margin so on-screen edges' lunes
are covered → no pop on pan; then pure geometry). The old **`diag` slider is repurposed → `web`**
(`P_web`, β = 1.8 − web): left = sparse/tree, right = looped/mesh. Curves use reflected control
points (straight base; `link_path` still bends for terrain).

### Part B — hierarchy + interchanges (in progress)
> **The interchange-geometry work has its own thread now** — master map:
> [`road-geometry-handoff.md`](road-geometry-handoff.md) (carts: interchange → rampkit → roadlab; the
> junction **DSL** [`interchange-dsl.md`](interchange-dsl.md); the geometry **research**
> [`road-geometry-refs.md`](road-geometry-refs.md); cart state [`interchange-handoff.md`](interchange-handoff.md)).

Approached from both ends; they meet at one interface — *given two crossing roads + a type → draw
the ramps.*
- **Geometry, bottom-up (started)** — a self-contained sandbox cart **`interchange.c`**: a highway +
  a hand-placed crossing road, **fake-3D grade separation** (crossing road drawn OVER the highway →
  overpass; no real z-levels — we decided to fake-z first), and a ramp-curve drawer. Types: OVERPASS
  + DIAMOND **fully dialed (✅ 2026-06-15)** — smooth single-polygon ribbons (no blobs), ramps off the
  outer lane EDGE, consistent across lane counts (1–4), near-side overpass connection, off-ramp
  tapers (no fold), ~1-lane ramps overlapping the road surfaces so they connect cleanly (no taper
  wedge needed). CLOVERLEAF / TRUMPET still first-draft (tangled — to nail). **Live sliders** for the
  ramp shape (reach / gore / taper / run-on); lanes (L) + median + dashed lane lines; angle (←→).
  Roads = subdivided `polyfill` ribbons. Grow shapes here, then bake the chosen slider values as
  constants + port the drawer into roadnet2. Catalog (overpass / on-off ramp / diamond / parclo /
  cloverleaf / trumpet / stack) + the fake-z-vs-real-z decisions are in the convo.

  **The junction matrix** — THREE axes:
  - **TOPOLOGY** (the leg layout): Cross (4-way, both through) / T-split (3-way, one road ends in) /
    Y-split (3-way, a road forks).
  - **CLASS-PAIR** (decides at-grade vs grade-separated): AR×AR / HW×AR / HW×HW. Rule: *any highway
    present → grade-separated (bridge + ramps), else at-grade; HW×HW = a heavier "system" interchange
    vs HW×AR's "service" one.*
  - **COMPLETENESS** (which movements are served) — carried by the **ramp-STYLE** knob (`itype`).
    **Full** interchanges serve every movement (diamond, cloverleaf, trumpet, stack); **partial /
    half** interchanges serve a deliberate SUBSET (half-diamond, parclo, half-trumpet). A partial is
    *not* a broken full — it's a real type you build on purpose: one-way frontage, space/budget
    limits, access you don't need in both directions. So a grade-separated cell isn't one junction —
    it's a **menu** of ramp-styles spanning full↔partial. (This is the axis we were missing: it's why
    "the T-split only connects one carriageway" was *correct as a half-diamond*, just mislabeled
    trumpet.)

  The grade-separated cells and their ramp-style menus:

  | topology | AR×AR (at-grade) | HW×AR / HW×HW (grade-sep) → ramp-style menu |
  |---|---|---|
  | **Cross** | crossroads / roundabout | **full:** diamond · cloverleaf · (HW×HW: stack) — **partial:** half-diamond · parclo |
  | **T-split** (one road ends in) | T-intersection | **full:** trumpet (bridge-over + loop + connectors → both carriageways) — **partial:** half-trumpet · **half-diamond** (near carriageway only) |
  | **Y-split** (a road forks) | Y / fork | **wye** (directional diverge — a fork has no "partial") |

  3-way cells (T, Y) also need a **flip** toggle — mirror the 3rd leg to either side. **Sandbox
  controls:** `1`/`2` road class · `T` ramp-style · topology toggle (cross/T/Y) · flip · `L` lanes ·
  `←→` angle · ramp-shape sliders. (The `itype` enum still only has OVERPASS/DIAMOND/CLOVERLEAF — it
  needs to GROW into the menus above, ideally topology-aware: e.g. T-split offers HALF-DIAMOND ↔
  TRUMPET, Cross+HW×HW offers STACK ↔ CLOVERLEAF.)

  **Slot status** (sandbox):
  - ✅ Cross+AR×AR — crossroads
  - ✅ Cross+HW×AR — **diamond** (full)
  - ⛔ Cross+HW×AR — cloverleaf (itype stub, tangled), half-diamond, parclo
  - ✅ T+AR×AR — T-intersection
  - ✅ T+HW — **HALF-DIAMOND** (the two near-side ramps + trunk-at-merge; serves ONE carriageway —
    a deliberate *partial*, **relabeled** from the earlier mistaken "trumpet" ✅)
  - ⛔ T+HW — **trumpet** (full; bridge-over + loop + connectors → both carriageways), half-trumpet
  - ✅ Y+HW — **wye** (tangent diverge, at-grade)
  - 🔶 Cross+HW×HW — overpass+ramps stub
  - ⛔ HW×HW — **stack** (the showpiece), cloverleaf (full loops — itype exists but draft)
- **Placement, top-down (parked — it's worldgen-ish)** — towns currently T straight onto highway hubs
  ("driveway onto a freeway"). Fix is *settlement-relationship driven*: tag each town **satellite** (a
  city within region radius → arterial into that city, which holds the interchange) vs **isolated** (no
  city near → reach the highway itself / chain to a neighbour); connections follow the hierarchy. We
  already have 4 ranks (hamlet/town/city/metro) — likely no new size tier, just the relationship.
  Deferred while the geometry is the focus.

## The v2 architecture — one graph, one query
- **Everything is spline edges in ONE graph.** Highways → collectors → access → cul-de-sacs
  are all `RoadEdge`s (a class + a polyline, sampled from control points). Nodes are real
  intersections with adjacency. No `grid_at`, no `street_axis`, no separate field.
- **`road_at(x,y) = nearest edge within its class half-width.** One query answers surface +
  class + collision for *every* tier (generalise `arterial_at` to all edges). Centre-line
  distance falls out for free (lane-keeping).
- **Rendering = stroking edges** (sharp at any zoom). This deletes per-pixel field sampling —
  the thing that caused v1's smear *and* its perf cost. (LOD = which tiers you stroke, by
  on-screen spacing; keep the idea, it was right.)
- **Buildings follow edges** (v1's buildings-follow-graph was the good part — keep it):
  parcels along each residential edge, set back, as footprint rects = the `building_at()`
  collision boxes. Reachable by construction.
- **Spatial index** — the one new piece vectorland needs: bucket edges into a coarse world
  grid so `road_at`/`building_at` test only nearby edges (the field gave O(1) locality for
  free; nearest-edge-over-all is O(n) without this). Cheap hash-grid keyed on world cell.

## Deeper roads, done as splines (the pretty part)
Highways look good because they're **splines through deterministic control points**. Do the
same one tier down and the whole network feels cohesive.

- **Warped grid** is the sweet spot for collectors/locals: lay the local lattice's control
  points, **displace them with low-frequency smooth noise** (the same domain-warp that
  de-circles the zones), then spline through them. Result: curvy, organic streets that are
  **still topologically a clean connected grid** — so connectivity, determinism, and "every
  lot fronts a road" stay trivial, but it stops looking like graph paper.
- **Per-district pattern palette** (carry the v1 idea, vector-native): the zone field still
  decides where/what.
  - **core** → straight grid (vector edges, *no* warp — curvy downtown reads as noise),
  - **suburb** → warped grid OR cul-de-sac / loops-and-lollipops (curved naturally),
  - **exurb** → sparse / organic.
- Control points are pure functions of district cell coords (the highway lattice is the
  template) → deterministic, infinite, seam-true. **The generation algorithm is the real work**
  (connect cleanly, don't cross into spaghetti, fill space) — more than the rendering.

## What carries over vs. what's dropped
**Keep (copy):** the L0 spline core verbatim (terrain, rivers, hub/town lattice, ranks,
classes, `link_path` bends/bridges, pan/zoom/sliders) — it's ~640 lines of good code, already
in this cart. The **urbanity → RCI zone field** (still needed to decide *where* roads densify
and *what* buildings go) — but as a *land-use* field only, never a *road* field. The
**buildings-follow-graph** model. The **stable-`gid` district identity** (v1's drag-flip fix).

**Drop:** `grid_at` / `street_axis` (the modular street field), `render_streetlevel`'s per-pixel
sampling, the field-based loupes, `lot_color`/`lot_built`/`occupancy` (buildings come from
edges now), the field↔graph extraction step (graph is generated directly, not extracted).

## Build order (each a stop-and-look milestone)
1. **L0 verified** — the cart already runs the clean highways (done; it's the baseline).
2. **Re-base to metres + LOCK SCALE by driving** — 🔨 in progress.
   - ✅ *Car* (ported from `steer.c`) + *two-camera harness*: **MAP** overview ⟷ **DRIVE** follow-cam.
     Click the map to drop/teleport the car (→ DRIVE), arrows/WASD steer, a faint **100m reference
     grid** gives the scale feel, a **GPS minimap** inset shows you-are-here; click the GPS → back
     to MAP (fast-travel). HUD reads **km/h + odometer** via `M_PER_UNIT` (the scale knob).
   - ✅ **Metre re-base done.** `1 unit = 1 metre`; `WS=300` stretches terrain features + the
     bend/bridge/valley distances; lattice in km (towns 3–8 km via `node_cs`, cities 25–60 km via
     `hub_cs`); zoom range re-spanned (`ZMIN 0.0008` continental … `OVERVIEW_ZOOM 0.012` town net …
     `DRIVE_ZOOM 1.5`); car speed real (`CAR_MAXSPD 0.6` → **~130 km/h, verified the HUD reads ~118
     not 10,000**); car + 20 m grid metre-sized; spawn moved to land. Terrain rescaled cleanly.
   - ✅ **Metre-width roads** — `ROAD_HW_M[class]` (motorway 12 … dirt 2.5 m half-width) drawn at
     `hw·P` px, floored to 1px so they stay visible on the map. The car now sits *on* a real-width
     road. (Also fixed the post-rebase **hang**: the bend-search + road strokers had loops that step
     in world units — scaled them so loop counts stay constant; `BEND_STEP`, off-screen-span skip.)
   - ✅ **Clean road ribbons** — strokers now draw each span as a `polyfill` quad (centre-line ±
     perpendicular·r) + a joint `circfill`, instead of a chain of circles that scalloped into
     "blobs" when wide. Straight crisp edges; also fewer ops than the per-pixel circle chain.
   - ⛔ *Polish still open:* the reference **grid is faint** (and too dense at mid-zooms); road
     **density** wants a tuning pass (towns sparse in spots); **node markers** (hub/town dots) are
     big at drive zoom; widths may be a touch wide; **scope** (~500 km) to commit. None block driving.
3. **Unified `road_at` = nearest-edge** over the *highway* edges first (prove the query +
   spatial index on the tier we already have), render as strokes. No field anywhere.
4. **Collector tier as a warped grid**, generated directly as spline edges into the graph,
   connected to the highways, sized in metres to the locked scale. Per-district straight-vs-warped.
5. **Access + cul-de-sacs** as the finest spline tier; the palette per district.
6. **Buildings on edges** (port v1's `building_at` + parcels), footprints in metres.
7. **Drive it for real** — full sloop: `road_at` (surface), `building_at` (collision), graph
   (routing). The car from step 2 graduates from measuring stick to gameplay.

## Honest caveats (so v2 doesn't repeat v1)
- **Empty looked clean.** Any fill-in is busier than blank terrain; "clean" comes from
  *restraint* (fewer tiers, bigger blocks, denser/larger buildings so road:building reads
  right), not from the fresh file. Design the fill-in sparing.
- **Spatial index is mandatory**, not optional — without it the nearest-edge query is too slow
  for collision once there are many edges.
- **Sketch the control-point scheme for each deeper tier on paper before coding** — that's
  where it lives or dies, and it's the part v1 never designed (it fell out of a field instead).
