# roadnet — a spline arterial network over the heightmap (design)

**Status: rungs 1–2 done (2026-06-14).** A fresh testbed cart
([`tools/carts/roadnet.c`](../../tools/carts/roadnet.c)) for the one thing
[`procgen-places.md`](procgen-places.md) explicitly parked as **"the wall"**:
**arced / non-axis-aligned roads in a deterministic, infinite world.** This doc is
that wall's design conversation (2026-06-13).

Related:
- [`procgen-places.md`](procgen-places.md) → "Scope / rungs" v2 — the wall this
  cart attacks. Its tile-grid city model is *kept*; roadnet sits **above** it (see
  "Two scales" below).
- [`sloop.md`](sloop.md) — the eventual consumer (its rig drives this world).
- worldgen (`tools/carts/worldgen.c`) — the gtascii port roadnet borrows its
  heightmap / rivers / hashed-node / passable-LOS helpers from, verbatim.
- **trackgen** (`tools/carts/trackgen.c`) — the **finite single-track cousin**. It
  generates *one bounded closed circuit* globally (scatter → convex hull → push
  apart → midpoint-displace → **drivability relax** → cardinal-spline), then drives
  it with the steer.c car model. roadnet is the opposite topology — an *open,
  infinite, streaming network* — so trackgen's generation can't be reused (it's a
  global pass). But two pieces transfer directly: its **cardinal-spline through
  points** is the same family as our Catmull-Rom, and its **corner-relax** (clamp a
  turn too sharp to drive) *is* roadnet's rung-2 drivability clamp — lift that idea
  into `link_path()` when we get there.

## The wall, and why it's softer than it looked

procgen-places parked curved roads with a real reason: *"splines aren't a local
function of position, so they fight the pure-function-per-cell model that makes the
whole thing seedable and zoomable."*

That's true for a **grown** spline — an L-system or drunk-driver agent, whose shape
depends on where the path came *from* (non-local history). It is **not** true for a
**node-lattice** spline:

- A coarse grid of cells; each cell hashes to **at most one node** (a POI), at a
  jittered position. Node presence + position are **pure functions of the cell's
  integer coords** — worldgen's `city_at()` already does exactly this.
- A road between node **N** and a neighbour **M** is a curve whose control data
  (the two endpoints and their tangents) is **recomputed from cell coords**, not
  remembered. The tangent at N is a pure function of N's neighbour nodes; M's of
  M's. So **any chunk can independently recompute the identical curve** for any
  link in its neighbourhood — no global pass, no cross-chunk communication.
- The only cost over a flat field is a **wider neighbourhood read**: Catmull-Rom /
  Hermite tangents need neighbours-of-neighbours, so the draw loop scans a ~2-cell
  margin. worldgen already scans a 2-cell margin for its cities — same cost.

**worldgen already proves the hard half** (deterministic node placement +
forward-only pairing so each link draws once). roadnet's new work is "swap the
straight `thick_line` for a sampled curve" — plus keeping it seam-true.

### The locality contract (the thing that must not break)

> A road's geometry must be a **pure function of the integer coords of the cells it
> touches** — nothing else. No accumulated state, no draw-order dependence, no
> "what chunk am I being viewed from." Hold this and infinite + deterministic +
> zoomable all come free; break it and roads tear at chunk borders.

## Two scales — the network stacks on top of the tile city

These two models stop competing and become a hierarchy (the "Main Rib Strategy"):

| layer | what | model | where |
|---|---|---|---|
| **arterial network** | long curvy roads *between* places | **roadnet** — spline over the POI node lattice, world-space | this cart |
| **local streets** | the dense grid *inside* a place | procplaces' two-field tile model | reused, scoped to a POI footprint |

A **POI is simultaneously a network node and a procplaces city site**: arterials are
the skeleton, the tile city is the flesh that fills a node where the arterials
arrive. The two carts converge at rung 4.

## The node → tangent → link scheme (rung 1)

Per node **N** at world pos `P_N`:

- **presence + position** — `node_at(cx,cy)`: `hash2(cx,cy)` gates presence (~a
  fraction of cells), jitters within the cell, rejects nodes off habitable land
  (height outside the passable band). Pure function of `(cx,cy)`.
- **tangent** `T_N` — sum of `(P_neighbour − P_N)` over the **present** neighbour
  nodes in the 8 surrounding cells, scaled by a handle factor. Symmetric and pure,
  so it's identical from either endpoint's chunk. (Imperfection accepted for rung
  1: the tangent counts neighbours even where the link is later dropped over water;
  it leans slightly toward a dead direction. Seam-safe, and invisible at overview
  zoom — revisit if it shows.)

Per link **N → M** (drawn **forward-only** — only when M's cell sorts after N's, so
each link draws once):

- `link_path(N, M, out[])` — samples a **cubic Hermite** from `P_N` (tangent `T_N`)
  to `P_M` (tangent `T_M`) into ~12 points; the renderer strokes a thick ribbon
  through them. **This one function is the seam** (see below).
- the link is **dropped** if the straight LOS between the nodes isn't passable
  (worldgen's `road_clear` — water / peak in the way). Cheap honest terrain
  reaction for now; routing *around* the obstacle is rung 2.

## Connected by hierarchy (rung 1.5 — done)

A **flat** node lattice fragments: a link needs *both* adjacent cells to hold a node
(0.4 × 0.4 ≈ 0.16) and then passable land between — a random graph below its
percolation threshold, so it reads as disconnected clumps, not a road *network*.
Real road networks are connected. The fix is the **road hierarchy** (the pasted
research's "Main Rib"), and it stays fully local/deterministic:

- **Highways** — `get_hub()`: hubs on a **coarse** lattice (`HUB_CS`), ~85% dense,
  each linked to its neighbour hubs (same forward-only Catmull-Rom). A lattice graph
  is **connected by construction**, so this spine spans the world — broken only
  where water/peak fails the passable-LOS (honest; **v2 bridges** reconnect it).
  **Diagonals are gated per-quad** (the `diag` slider): linking *both* diagonals of
  every 2×2 quad makes a "World-of-Goo" X through each square, which reads as noise.
  So each quad hashes its corner to pick *at most one* diagonal (or none), gated by
  the slider — clean orthogonal grid at 0, occasional shortcuts higher, never an X.
- **Minor roads** — every town (`get_node`) links to its **nearest hub** (a feeder
  onto the spine, `nearest_hub`, 5×5 hub-cell search) **and** to its **nearest other
  town** (`nearest_town`, a local road). Local roads chain clusters together so a
  town far from any hub still connects via town→town→…→hub — *not everything needs a
  highway*. Degree ~1 each → no criss-cross. Mutual-nearest pairs double-draw
  harmlessly (identical pixels). Some terrain-locked islands remain — honest, and
  what v2 bridges/ferries fix.
- The *promiscuous* town↔town lattice (link every forward neighbour) is gone — that
  was the original criss-cross; "nearest one" is the controlled replacement.

Both tiers are the same pure-function-of-cell-coords spline, so seam-safety holds.
Cell sizes are sized to the view (~`HUB_CS` 20 tiles, `NODE_CS` 10) so several hubs
fit the ~80-tile-wide screen — a coarser spine than the screen reads as empty.

## Intro / setup panel (done)

The cart opens on a `ui.h` setup panel: seven sliders (hub gap, town gap, hub/town
density, wiggle, diag, water) + ROLL + EXPLORE, over the **live full-screen world** — no
separate preview renderer, because terrain and roads are pure functions of the
params, so dragging a slider re-rolls the world behind the panel instantly. The
params are `P_*` floats in 0..1; getters map each to its real range and the
`NODE_CS`/`HUB_CS` macros call the getters, so every call site stayed unchanged.
Only the **water** slider touches the terrain cache (added to its key); cell-size /
density / wiggle only affect the per-frame road draw, so they're free to live-edit.

**Mousewheel zoom** (ZMIN 0.12 .. ZMAX 2.5, pivot = screen centre): tile-pixel size
is `P = TILE·zoom`; all screen transforms use `P`. Terrain is **sampled per screen
cell** — `height_at` at the world coord under each 2px block — *not* cached in a
per-tile buffer, so cost is fixed at any zoom and there's **no zoom-out ceiling**
(an earlier `colgrid` buffer capped it). **LOD:** below zoom 0.45 the town/feeder
tier is dropped (highway skeleton only) and hub markers shrink (3→1px), so a
continental view reads as a road network rather than a polka-dot field. `vcols/vrows`
(the visible world-tile span) is still recomputed per frame for the road-loop bounds.
**Left-drag pans** ("grab the map" — screen-px delta ÷ P → world tiles); in the setup
panel it's gated to `mouse_x > PANEL_W` so it doesn't fight the sliders.

## Rungs

1. **Spline seam + connectivity** *(done — see above)* — hub/town/feeder hierarchy,
   forward-only Catmull-Rom ribbons, passable-drop. Heightmap bands underneath
   (worldgen). Free-fly pan camera (testbed, not driving — sloop owns driving).
   **Milestone = the seam test:** pan across a cell boundary (toggle G for the
   border overlay) and the *same* curve draws from either side — held by
   construction (curve points are world-space, camera applied last). The wall is
   down.
2. **Terrain-aware routing** *(done)* — the real `link_path()` seam now exists: it
   samples the base Catmull-Rom plus a **perpendicular bow that bends the road around
   impassable terrain**, and **verifies every sample sits on passable land** before
   returning (so a road is never drawn over water/peak — strictly better than rung 1,
   which checked only the straight line then drew a curve that could clip water). The
   bend grows until the whole path clears; a genuinely **boxed-in** link returns 0 and
   drops (bridges are v2). One function, world-space samples → render *and* future
   `road_at()` read the identical geometry. *(Drivability corner-clamp — trackgen's
   relax — folds into the same function when sloop starts driving; deferred till then,
   it's meaningless without a car.)*
3. **POI typing** — nodes get a kind (town / fuel / landmark…), Poisson-ish minimum
   spacing so they're not uniformly one-per-cell; **road class** (path → local →
   avenue → arterial → highway, per procgen-places' table) chosen by the rank of the
   POIs a link joins. Class drives width / colour / markings of the ribbon.
4. **Fill the node** — drop procplaces' tile city into a POI footprint where the
   arterials enter. The two carts meet; `road_at()` becomes the shared query seam
   for sloop.
5. **(v2 terrain features — see seams below.)**

## v2 seams — documented, planned, NOT built (per this session's decision)

The pasted GTA-procgen research went deep on road↔terrain interaction. Most of it
is **parked** — some of it may never fit a 2D top-down pixel view — but rung 1
leaves the **hooks** so v2 is an extension, not a rewrite:

| v2 feature | the seam left for it | rule |
|---|---|---|
| **obstacle avoidance** | ✅ *done in rung 2* — `link_path()` bows the curve around impassable terrain and verifies passability | callers (render + future `road_at()`) never change — **one path function, both paths call it** (procgen-places' "one wrong turn to avoid") |
| **valley-following** (roads *prefer* low/flat ground, not just avoid water) | a cost term inside `link_path()`'s bend search | extends the existing bend logic, doesn't change the seam |
| **bridges** | the per-sample loop already queries `height_at` at each sample; tag a sample whose height < water level as `bridge`, render differently | a link over water becomes a bridge instead of being dropped |
| **tunnels / carving / embankments / Z-slope** | same per-sample height tag | **deferred hard** — likely doesn't map to top-down 2D; do not let it shape rung-1/2 structure |
| **sloop collision (`road_at`)** | sloop calls `link_path()` for the *same* geometry it sees | the locality contract guarantees screen == collision |

**The single rule that keeps all of this open:** never let a warp / reroute / drop
live only in the draw loop. Geometry comes out of `link_path()`; both render and
collision read it. Set up cheap now (rung 1), painful to retrofit later.

## Decisions
- ✓ **Fresh cart** (not an evolve of worldgen/procplaces) — worldgen stays a clean
  gtascii port; procplaces stays the tile-city model; roadnet is the network layer
  that will later *consume* both.
- ✓ **Node-lattice spline**, not grown/agent splines — preserves the locality
  contract (the whole reason the wall looked unclimbable).
- ✓ **Cheap terrain reaction first** (drop non-passable links); valley-following,
  bridges, tunnels are v2 with seams left, per the 2026-06-13 call.
- ⏸ **road class, POI typing, tile-city fill, sloop `road_at()`** — rungs 3–4.
