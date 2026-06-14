# roadnet — session handoff (2026-06-14)

Where the `roadnet` work stands, and how it maps to the original goal. Full design
detail lives in [`roadnet.md`](roadnet.md); this is the orientation + "what's next"
for whoever picks it up.

## The original question

Build a fresh cart to experiment with **procedural terrain / world / city generation**
for a **GTA1-style + cataclysm-DDA-flavoured driving/adventure game**: ideally
**infinite**, deterministic, good for **driving around a city** and roaming. Do it in
**phases**, starting from the simplest sensible thing. (Kicked off from a long chat
about procgen algorithms — alternatives to Perlin, curvy non-axis roads, determinism,
rivers/biomes, RCI zoning, tile-vs-spline.)

## What got built

One new cart — **`tools/carts/roadnet.c`** — a deterministic, infinite, top-down
**world map** with a **magnifier into a zoned street level** beneath it. Free-fly
testbed (pan/zoom/drag), not a game yet. Commit trail (oldest → newest):

| commit | what |
|---|---|
| `8bdda57` | **rung 1** — node-lattice Catmull-Rom spline network; cracks the "curved roads can't be infinite/deterministic" wall (control points are pure fns of cell coords → seam-true) |
| `de8631a` | left-drag pan + mousewheel zoom out to a continental overview (screen-space terrain sampling, no buffer ceiling) |
| `fc78d42` | **rung 2** — terrain-aware routing via `link_path()`: roads bend around water/peaks, verify every sample is passable |
| `f23df0c` | **rung 2.5** — bridges across short water gaps (rivers/straits); wide water stays a barrier |
| `e432fa2` | **rung 3** — node **rank** (hamlet/town/city/metro) + road **class** (motorway→dirt) styling + dashed centre-lines |
| `c16c9a2` | rung 3 refinements — blue-noise **node spacing** (priority suppression) + **valley-following** |
| `9799866` | **magnifier** (toggle **L**) — inset window into the street level (the L2 harness) |
| `08bd284` | **L2 zone model** — RCI **anchored to roadnet cities** (urbanity field + concentric land use) |
| `9ddc45e` | de-circle the zones — domain-warp + patchy farms (no more bullseye) |
| `51fdc57` | **L2 city sliders** — city size / downtown / farms / blocks, live |
| `52a2d97` | **L2 street level** — nested street grid carving **blocks** + building **lots**; grid **aligned to arterials** per city (`city_dir`) with an `align` patchwork slider; **`road_at()`** query seam + arterials **carved through** as real road surface (renderer calls `road_at` → screen == collision) |
| `8d5b1f5` | **class-aware street roads** — the carve owns the loupe surface; dirt=brown/paved=grey, centre-lines on big roads, sidewalks |
| `9212a88` | **`road w` slider** + **outskirts gradient** (occupancy thins the suburb) + **ribbon development** along roads + **Voronoi farm fields** (crops + hedgerows); `road_at` gains a `built` flag |

Earlier in the session there's also an unrelated tiny commit (`be105f8`, an sh101 doc
note) that has nothing to do with roadnet.

### Feature inventory (what the cart does today)
- **Infinite + deterministic** world: every feature is a pure function of world
  coords, streamed around the camera, no global pass. Same seed → same world.
- **Terrain**: layered-noise heightmap + rivers (borrowed from `worldgen.c`).
- **Road network**: connected hierarchy — **highways** (hub lattice) + **feeders**
  (town→nearest hub) + **minor roads** (town→nearest town). Curvy (Catmull-Rom),
  terrain-aware (bend/bridge/drop), **classed** by endpoint rank (width/colour/dashes).
- **Spacing**: rare high ranks + priority suppression → cities feel distributed.
- **Navigation**: intro panel (live sliders + ROLL/EXPLORE over a live preview),
  mousewheel zoom (continental ↔ close), left-drag pan, cell-grid seam overlay.
- **Magnifier (L)**: a lens showing the **street level** at the crosshair — the same
  highways (aligned) **plus** an RCI-zoned city the map is too coarse to draw.
- **L2 zones**: urbanity bumps around cities → concentric, domain-warped land use:
  wild → **farm** → **res** → **com** core, with **industrial** fringe, **harbours** on
  water, **parks** carved in.
- **L2 street level** (in the lens): a nested **street grid** carves built-up zones into
  **blocks** filled with building **lots**; the grid is **aligned to the arterial** that
  enters each city, blended per-district by an `align` slider. **Arterials carve through**
  as real road surface, **class-aware** (dirt/paved, centre-lines on big roads, sidewalks).
- **L2 outskirts**: the suburb **thins** toward the edge (occupancy gradient), **houses
  follow the roads out of town** (ribbon development), and open land is **Voronoi farm
  fields** (crops + hedgerows) — no more hard blob edge or flat farm ring.
- **`road_at(wx,wy)`**: the world query seam — `{on_road, class, zone, built}` — that the
  renderer itself calls (so screen == collision), and that **sloop will drive on**.
- **Seven live L2 knobs**: city size / downtown / farms / blocks / align / road w / field.

## Where we arrived vs. the goal

**Mental model (LOD stack):**
- **L0 region map** — ✅ done (roadnet: highways, cities, terrain).
- **L1 city layout** — ✅ zones done (RCI land-use field) + ✅ **street grid** (nested
  local/avenue lattice carving blocks, density emergent from urbanity).
- **L2 block contents** — 🔨 **mostly built** (2026-06-14): street grid + building
  **lots**, grid aligned to arterials (`city_dir`/`align`), arterials **carved through**
  (class-aware), the **outskirts gradient** (occupancy thinning + ribbon development), and
  **Voronoi farm fields** (crops + hedgerows). All readable via the **`road_at()` seam**
  (now with a `built` flag). **Access-street tier added 2026-06-14** (`CL_ACCESS`, the L3
  finer lanes bisecting suburban blocks — see the L3 realization below). Still ⛔: **park
  contents** (the football field — PARK is still flat tint), building **footprints** drawn as
  *collidable rects* (lots are colour only), parking lots, landmarks.
- **L3 car + collision + play** — ⛔ not built, **but `road_at()` now exists** — the seam
  sloop will drive on. Wiring it in is rung 4 (the next leap).

**So: we built the *world/map* thoroughly and the *street level* is now real content in a
lens — blocks, lots, class-aware roads, a proper city→country gradient with fields.** What
we still have NOT built is the part you actually *drive*: a car on those streets, building
*collision*, and the map↔street mode transition. roadnet is the world; it is not yet "a
GTA1 you can play" — but the `road_at()` seam to make it one is in place.

Against the original phases: terrain ✅, splined infinite/deterministic roads ✅ (the
hard one), rivers+bridges ✅, RCI zones ✅, street-level blocks/roads/fields ✅. Deferred:
driving (sloop), building collision + footprints, landmarks-as-gameplay, the map↔street
mode transition, tunnels/carving (parked, may not fit 2D top-down).

## The L3 realization (2026-06-14) — a level was missing
Looking at the district loupe: built-up blocks are solid masses of lots with **no capillary
streets** — the houses have no road frontage. The road hierarchy stops too early (we have
highway→arterial→collector→local; the **access tier** is missing), and those finest streets
+ individual footprints only read at a **deeper zoom** than the district loupe. So there's a
missing level — **L3, the on-the-street view** — and it's *exactly where sloop drives*.

Captured in a new design doc: [`roadnet-streetlevel.md`](roadnet-streetlevel.md) (the gap,
the explicit LOD stack, the L3 content, and the **seams** that keep it deterministic). The
build **harness is in**: a second, deeper **"BLOCK" loupe** (top-right inset, ~3 tiles
across) alongside the district "STREET" loupe — currently shows L2 zoomed in; the access
streets + footprints get built *into* it next, same playbook as the L2 lens.

## Next time — where to pick up (recommended order)
1. ✅ **L3 access-street tier** (DONE 2026-06-14) — `ST_ACCESS`/`CL_ACCESS`: half-pitch
   (`block_sp/2`) lanes that bisect the suburban blocks (where locals are suppressed) so
   every lot fronts a street; pure-fn, flows through `grid_at`→`road_at`, draw-gated to the
   BLOCK loupe via `zoom >= LOUPE2_ZOOM`. *(See roadnet-streetlevel.md → "What's built".)*
1b. ✅ **View-switcher + road-graph layer** (DONE 2026-06-14) — `TAB` cycles MAP↔GRAPH,
   `ZMAX` raised 2.5→12 (the main camera flies to street level = the eventual sloop camera),
   and `build_graph()` packs the visible network into `RoadEdge gedge[]` (arterials, exact;
   grid/access streets still field). `VIEW_GRAPH` debug-renders the graph as crisp vector
   centre-lines + node dots over dimmed terrain. *(roadnet-streetlevel.md → "What's built (cont.)".)*
2. **Grid/access streets → graph edges + nodes/adjacency** — *next move for the car sim*.
   Vectorise the intra-city grid (per-city/per-district; validate vs `road_at()`), append to
   `gedge[]`, add intersection nodes + adjacency → routing/snap/traffic. Then **footprints +
   `building_at()`** — lots → real footprint rects (inset/outline/driveway toward the fronting
   access lane), exposed as a collision seam (the precise version of `road_at`'s `built`).
3. **Park contents + the football field** — PARK is the last flat-tint zone (small win).
4. **Rung 4 — drive it** — wire `road_at()` + `building_at()` into **sloop** at L3; the
   BLOCK lens becomes the car's camera. The leap from viewer to playable.
5. **Polish backlog**: the in-city **bridge gap** (road_at doesn't pave water + loupe no
   longer strokes the spline → a bridge shows as a break); promote `U_CORE`/field/lane
   widths to sliders; snap the district grid-shear to a road; loupe **UI/placement**.

## How the pieces relate (cousin carts)
- **`sloop.c`** — the **car physics** (already solved). The eventual consumer; will
  drive the street level via a `road_at()` query.
- **`worldgen.c`** — gtascii port; roadnet borrowed its heightmap/rivers/hash verbatim.
- **`procplaces.c`** — the **tile-city RCI** model. At rung 4 it becomes the *interior
  painter* fed by roadnet's rank (it stops generating its own "where are the cities").
- **`trackgen.c`** — finite single-track cousin; its corner-relax = a future
  drivability clamp.

## Key invariants (don't break these)
- **Locality contract**: road/zone geometry must be a pure function of the integer
  coords of the cells it touches — no accumulated state, no draw-order or
  view-dependence. This is what makes infinite + deterministic + seam-true work.
- **`link_path()` is the one geometry seam**: render *and* the future sloop `road_at()`
  must both read it, so the screen and collision can never disagree. Reroute/bridge/
  bend all live inside it.
- The street level is drawn in the **same world coords** as the map, so it aligns with
  the arterials by construction.

## How to run / see it
- Editor: open the **roadnet** cart (tutorials panel). Opens on the setup panel —
  drag sliders, **EXPLORE** to roam, **L** for the magnifier, pan over a city to see
  its zones in the lens.
- Headless screenshot (for agents): build then `--run`, read
  `build/.bake/roadnet/screenshot.png` (it opens in panel mode; to inspect explore
  mode, temporarily flip `static int mode = 0 → 1`, bake, revert).

## Next steps (in recommended order)
1. **L2 blocks** — 🔨 *street grid + building lots + grid-aligned-to-arterials +
   `road_at()` & arterials-carved-through done (2026-06-14, see [`roadnet.md`](roadnet.md)
   → "L2 block contents" / "Aligned to the arterials" / "`road_at()` + arterials carved
   through")*. The street level is now **queryable** (`road_at` → `{on_road,cls,zone}`,
   the seam sloop drives on) and arterials carve through the blocks as real road surface
   (renderer calls `road_at`, so screen == collision). Remaining: farm *fields*, park
   contents (the football field), parking; footprints as collision-ready rects (today
   lots are colour only). Iterate live under the magnifier.
2. **Landmarks** — gas / hospital / gun-shop point POIs (deferred from rung 3; they
   want gameplay context).
3. **Rung 4 — drive it** — wire `road_at()` into `sloop`, drop the L2 city into hub
   footprints, and add a **map↔street mode** (two-mode is fine; seamless LOD is a
   someday). This is where it becomes a playable prototype.
4. Optional polish: true min-distance node spacing, the zone "mix" knob (patchy vs
   concentric), promote lower-bang L2 knobs (industry/park density) to sliders.
