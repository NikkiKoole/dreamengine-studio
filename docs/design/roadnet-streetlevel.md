# roadnet L3 — the on-the-street level (design seed, 2026-06-14)

**Status: access tier + view-switcher + full road-graph (incl. grid nodes/adjacency) BUILT
(2026-06-14); footprints next.** The deeper **"BLOCK" loupe** is in as the build harness.
The **access / residential street tier** (`CL_ACCESS`), a **TAB view-switcher** (MAP ↔
GRAPH), and the **road-graph data layer** — arterials *and* the vectorised intra-city grid
with intersection **nodes + adjacency** (`gedge[]`/`gnode[]`) — are now built (see "What's
built"). Still **not built**: stitching the grid graph onto the arterial backbone at city
entries + degree-2 node simplification (graph tidy-ups), building **footprints +
`building_at()`**, and park contents.

## What's built (2026-06-14) — the access-street tier
The suburb ring (`U_LIGHT ≤ U < U_MED`) had its local streets suppressed, so its blocks
were 4× the core's and interior lots had no frontage. `street_axis()` now adds an
**`ST_ACCESS`** tier there: half-pitch (`block_sp/2`) lanes that bisect those big blocks so
every lot fronts a street. The new road class **`CL_ACCESS`** (enum tail — stays
`> CL_ARTERIAL` so it gets no centre-line, no renumbering) flows through `grid_at()` →
`GR_ACCESS` → `road_at()` (`r.cls = CL_ACCESS`) → the renderer (narrow `CLR_DARKER_GREY`
lane, no sidewalk). It is **draw-gated to the BLOCK lens** via `zoom >= LOUPE2_ZOOM`
(hoisted to the top of the file so `street_axis` can read it): the dense core keeps only
its locals, the suburb shows the access grid, and the STREET lens (zoom 4) shows neither —
exactly the tier-by-zoom draw-gate rule below, with zero new plumbing. Verified by baking
the BLOCK lens over a suburb point with `CL_ACCESS` temporarily painted a vivid colour.

## What's built (2026-06-14 cont.) — view-switcher + the road-graph layer
Two things that came out of the "I can hardly see the small roads" review (the tiny
field-sampled loupe was the bottleneck, not the zoom number):

- **TAB view-switcher** (`view` ∈ `VIEW_MAP | VIEW_GRAPH`, cycled by `KEY_TAB` in explore;
  HUD shows the current view). And **`ZMAX` raised 2.5 → 12**, so the *main* camera now flies
  all the way down to street level — no more squinting at a 78 px inset. This full-screen
  deep-zoom view **is the eventual sloop street-camera**.
- **`VIEW_GRAPH`** — a debug view that draws the network as crisp **vector centre-lines**
  (`line()`, class-coloured: motorway red … dirt orange) at *any* zoom — the proper fix for
  "see the small roads", since a field paint aliases away when small. Over **dimmed terrain**,
  with the city-interior **street FIELD layered full-screen** once `zoom ≥ GRAPH_STREET_ZOOM`
  (so you see the grid/access streets big), and **white node dots at the city anchors**.
- **The road graph** — the **background data layer the car sim navigates** (snap-to-road,
  route, traffic), built from the *same* geometry the field reads (graph == field == screen):
  - **Edges** `RoadEdge gedge[] {x0,y0,x1,y1,cls,n0,n1}` and **nodes** `RoadNode gnode[] {x,y}`.
  - `build_graph()` packs the **arterial backbone** (the `sg*` segment cache, exact; `n0=n1=-1`).
  - `graph_add_grid()` **vectorises the intra-city grid + access streets** (LOD: street zoom +
    visible region only). The grid is **rotated per district**, so it goes *per gathered city /
    per district / in that district's own frame*: at each grid-line crossing it places a **node**
    if it validates as road, and joins adjacent road-connected crossings into **edges carrying
    both node ids** (`n0`/`n1` = routable adjacency). Every candidate is checked with `road_at()`,
    so the graph is a strict **subset of the field** *and* the per-district frame **auto-clips at
    boundaries** (a candidate that strays into a neighbour district just fails to validate).
  - The access tier was moved off the render-zoom gate onto a **`want_access` flag** so the graph
    extracts access lanes regardless of view zoom (generation ≠ draw — the renderer still gates
    drawing on zoom). Default 0; the renderer sets it from zoom, the extractor sets it 1.
  - **Locality-contract fix (2026-06-14):** district orientation (rotate-to-artery vs world-axis)
    is now keyed on a **stable per-city id `gid[]`** (hash of the city's origin cell), NOT the
    `dom_i` gather-array index. The index renumbers as cities scroll in/out of `gather_cities()`,
    so the old code re-rolled the whole grid **while panning** ("the graph flips on drag") — and
    it wasn't actually a pure fn of world pos. `gid[]` makes the orientation position-stable.
    `city_grid_coords()` and `graph_add_grid()` use the **identical** hash (they must agree).
  - **Degree-2 collapse (2026-06-14):** `graph_collapse_grid()` walks each chain from a real
    junction (degree ≠ 2) through colinear degree-2 mid-lane nodes and emits ONE merged edge —
    so a straight lane is a single edge between intersections, not a chain of half-block stubs,
    and only junctions/dead-ends draw as node dots. Arterials pass through untouched.
  - **Not yet:** stitching the grid graph onto the arterials at city entries (so a route can
    leave town).
  - *Perf note:* `graph_add_grid()` re-runs each frame in GRAPH view (~10–15k `road_at()` at
    street zoom — fine for a debug view); the car sim will build it per chunk + cache.

Related: [`roadnet.md`](roadnet.md) (L0–L2: the map + district street level),
[`roadnet-handoff.md`](roadnet-handoff.md) (where the work stands),
[`sloop.md`](sloop.md) (the car that will drive L3).

## The gap (why this level has to exist)

The district loupe (L2) draws arterials + the avenue/local grid + zones + Voronoi fields.
But inside a built-up block the lots are a **solid mass with no capillary streets** — the
interior houses have no road frontage; nobody could drive to them. A real network is
~five tiers:

> **highway → arterial → collector → local → access**

We have the **top three** (carved arterials/highways, avenues, core locals). The **access
tier** — the small residential streets and cul-de-sacs every house fronts onto — is
missing, so subdivision stops too early and houses clump.

And those finest streets + individual building footprints only *read* at a zoom deeper
than the district loupe. So this is simultaneously "**missing streets**" and "**missing a
zoom level**" — the same gap from two sides.

**Why now, before sloop:** L3 — a car on a residential street, beside individual building
footprints — is *exactly where sloop drives*. L3 isn't a detour before rung 4; it's the
level rung 4 needs. Wiring a car onto a district view where the houses have no streets
would be backwards.

## The LOD stack, made explicit

| level | view | shows | the query seam |
|---|---|---|---|
| **L0** | region map / GRAPH view | terrain, cities, the arterial network | `road_at()` + `gedge[]` (graph) |
| **L1/L2** | district loupe ("STREET") | arterials + avenue/local grid, zones, Voronoi fields, building lots | `road_at()` |
| **L3** | block loupe ("BLOCK") / GRAPH view | **access streets** subdividing blocks (✅), **individual footprints**, driveways, parking, the football field | `road_at()` + `gedge[]`/`gnode[]` (grid graph ✅) + `building_at()` *(to build)* |

Generation is **zoom-independent** (every feature a pure function of world coords); only
**which tiers draw** is gated by zoom (LOD). So zoom changes what you *see*, never what
*exists* — determinism holds.

## L3 content (what fills the BLOCK loupe)

- **Access / residential streets** — a *finer lattice* (or organic tree of cul-de-sacs)
  that subdivides each L2 block, so every lot fronts a street. A new road class
  (`CL_ACCESS`) below `CL_STREET`.
- **Building footprints** — each lot → a real footprint rect (setback/yard + outline),
  fronting its street, with a driveway. Replaces the flat lot-colour square.
- **Parking, the football field, gardens** — block contents keyed to `(block, zone)`.

## The seams (the load-bearing rules — keep L3 local & deterministic)

This is the part that matters; get the seams right and L3 is an *extension*, not a rewrite.

1. **Recursive subdivision = pure-fn-of-block-coords AND connects to the parent.** An
   access street is defined *relative to its L2 block* — whose coords + orientation are
   already deterministic (`city_grid_coords`) — and must **meet the block's bounding
   local/avenue streets** (branch off a known parent edge / dead-end as a cul-de-sac),
   never float. Same locality contract as everything else: any chunk recomputes the
   identical streets from cell coords, no global pass.
2. **`road_at()` absorbs the access tier unchanged.** The finer streets just add lines to
   the grid query; `road_at` already returns `{on_road, cls, …}`. Add `CL_ACCESS`; sloop's
   call site doesn't change — it just sees more roads when it's at L3 scale.
3. **Footprints become their own collision seam — `building_at(wx,wy)`.** A pure function
   returning `{solid, lot id}`; the renderer draws the footprint, sloop reads the *same*
   rect for collision (the `road_at` pattern again — screen == collision). `road_at`'s
   existing `built` flag is the coarse precursor; `building_at` is the precise rect.
4. **Tier-by-zoom LOD is a DRAW gate, never a generation gate.** District loupe draws down
   to local; block loupe adds access + footprints. The functions that *produce* them run
   the same at any zoom; we just skip drawing sub-pixel tiers. (This is why the harness
   works: build the content, reveal it by zooming, with zero determinism risk.)

## The harness (how we build it — same playbook as L2)

The L2 street level was built by adding a **lens** first, then growing content into it.
Repeat: a second, deeper **BLOCK loupe** inset (centred on the same crosshair, zoomed a
few levels past the district loupe) is the canvas. Build the access streets, then
footprints, into that lens incrementally — bake, look, iterate. Eventually L3 stops being
a lens and becomes **sloop's camera** (a mode, not an inset) — but the lens is enough to
build against now, exactly as before.

*Current state:* the BLOCK loupe shows the **L2 content zoomed in** (lots become visible
as buildings-to-be) — the access streets + footprints are what we build into it next.

## Open decisions (for build time)

- **Street pattern:** clean grid subdivision (simplest, reads as suburb) vs an organic
  **cul-de-sac tree / loops-and-lollipops** (more real, more work). Lean grid for MVP.
- **Footprint shape:** rect MVP (inset + outline + driveway gap toward the fronting
  street); richer shapes later.
- **Irregular parent blocks** (near a curved arterial): how access streets terminate when
  the block edge isn't axis-aligned — probably clip to the parent road.
- **One deeper loupe vs the loupe gaining tiers as the mousewheel zooms it** — a UI
  question, deferred. (Today: a separate fixed BLOCK inset.)

## Next
1. ✅ **Access-street tier** (`ST_ACCESS`/`CL_ACCESS`).
2. ✅ **View-switcher** (TAB MAP↔GRAPH, `ZMAX→12`) + **vector GRAPH debug render**.
3. ✅ **Full road graph** — arterials + vectorised intra-city grid with intersection
   **nodes + adjacency** (`gedge[]`/`gnode[]`, `want_access` flag). See "What's built (cont.)".
4. **Graph tidy-ups**: ✅ degree-2 collapse (straight lanes = single edges). Remaining (when
   routing needs it): **stitch** the grid graph onto the arterial backbone at city entries, so a
   route can leave town. A post-pass over `gedge[]`/`gnode[]`; doesn't block driving.
5. **Footprints** + `building_at()` (the collision seam) — *the next build move*. Lots are still
   flat colour; turn each into a footprint rect (setback/yard + outline + driveway toward its
   fronting access lane), exposed as `building_at(wx,wy) → {solid, lot id}` (screen == collision,
   like `road_at`).
6. **Park contents** (the football field) — PARK is the last flat-tint zone (small win).
7. **Rung 4** — sloop at L3: drives the graph (route/lane-keep via `gedge[]`/`gnode[]` + `coff`),
   stays on the surface via `road_at()`, collides with `building_at()`. The GRAPH view is its camera.

## Starting point for next session (the concrete first move + two hooks)

**First move (DONE 2026-06-14):** ~~add `CL_ACCESS` and a finer street tier in `grid_at()`~~ —
built; see "What's built". **The new first move is footprints + `building_at()`** (item 2 in
"Next"): turn each built lot into a footprint rect fronting its access lane, exposed as a
collision seam. Build it into the BLOCK loupe the same way (bake over a suburb point — reuse
the temp init/`LOUPE2_SZ` dance below — iterate, revert before committing).

Two hooks that aren't obvious from the rest of the doc (still apply — they're how the access
tier got built, and the same playbook for footprints):

- **Zoom is the LOD signal.** During a lens render `zoom` is set to that lens's value
  (`LOUPE_ZOOM` vs `LOUPE2_ZOOM`), so `render_streetlevel`/`grid_at` can gate the access
  tier on `zoom >= LOUPE2_ZOOM` — no new plumbing needed, just read `zoom`. (This is the
  "tier-by-zoom = draw gate" rule, concretely.)
- **The BLOCK loupe centres on the screen crosshair**, which at spawn sits over green —
  so the lens looks empty until you **pan over a city core**. For baking a screenshot that
  shows L3 content, position the camera over a dense city first (or temporarily nudge
  `SPAWN_X/Y`), and enlarge `LOUPE2_SZ` for inspection (restore before committing, like the
  `LOUPE_SZ` dev-bake dance).

Everything else (the seams, the LOD stack, open decisions) is above; the handoff's
"Next time" list has the ordered plan. No loose state outside these docs.
