# data-layers — the SimCity simulation behind the map (design seed)

**Status: SHIPPED (2026-06-23).** Built as the `gridcity` cart — all planned layers are in
(v1 → v2 complete, 32 spec assertions); see the "All planned layers are in" note at the bottom.
Optional future depth is listed there but nothing is committed/postponed. A new experiment / pillar the big-game backlog
doesn't cover: not road *geometry* (roadlab/streetlab) and not a top-down *generator*
(procplaces) — the **aggregate tile-data simulation** behind SimCity 2000. Given a
terrain, tile-based roads, and painted RCI zones at various densities, **compute the
emergent data layers** — land value, crime, pollution, population density, service
coverage — that evolve over time through feedback loops. Working cart name: **`gridcity`**
(provisional). Build plan + phasing at the bottom.

This is squarely the [second north star](second-north-star.md): the data-layer
dependency graph IS a deep, honest simulation; the overlay views are the humble lo-fi
surface. The whole thing is `int` fields and blur passes — no physics, no vector road
geometry.

## Related — where this sits

- [`procgen-places.md`](procgen-places.md) — the **mirror image**. procplaces *paints*
  R/C/I top-down from noise (the authoring/supply side). This sim *derives* values
  bottom-up from placed zones + feedback (the consequence side). One day procplaces
  output could seed this sim's zone map.
- [`streetlevel-content.md`](streetlevel-content.md) — lotfill's `zone_at`/`density`
  fields are the same R/C/I × intensity idea, one tier down.
- [`second-north-star.md`](second-north-star.md) — the deep-sim-behind-lo-fi thesis this serves.
- [`big-game-backlog.md`](big-game-backlog.md) — the other pillar program; this is a new,
  independent experiment, not one of its five seams.

## The core insight — it's two mechanisms, nothing more

Every "smart" thing SimCity's map does reduces to:

1. **Cross-layer formulas** — each field is a function of the *other* fields. Land value
   is a function of pollution + crime + accessibility + terrain; crime is a function of
   density + land value + police. The fields form a **dependency DAG**, recomputed each
   sim tick.
2. **Spreading passes** — a field's influence *spreads* to neighbours by iterated local
   passes (no per-tile radius checks). Two flavours, and the v1 build proved you need
   **both**:
   - **Diffusion blur** (4-neighbour *average*) — for pollution, density, water-value. It
     conserves "mass", so a value bleeds outward and thins.
   - **Decaying spread** (each cell = strongest-neighbour − decay = a distance falloff) —
     for **service coverage** (police/fire radius). A blur *cannot* do this: averaging a
     single 250-strength station over its neighbourhood thins it to near-zero, so it never
     suppresses anything. Coverage needs a falloff that stays strong at the source and
     fades with radius (`radius ≈ source/decay`). This was the one non-obvious finding from v1.

If we build a clean `smooth(field)` and a clean per-tick `recompute()` over the DAG, we
have SimCity's whole data engine. (This is the open-source *Micropolis* core, which SC2000
is built on — so the real formula *shapes* are knowable; exact constants we tune.)

## The full SimCity 2000 layer catalogue

What the SC2000 "Maps" window could overlay, grouped by how it's produced:

**Built / structural** (placed or flood-filled, not computed)
- Structures — what's on the tile
- Zones — R / C / I, each in **light/dense** (the "various densities")
- Power grid — boolean flood from plants along conductive tiles
- **Water system** — underground pipes; zones need water to fully develop *(new in SC2000)*

**Computed scalar fields** (the interesting ones — the DAG)
- Population **density**
- Population **growth rate** (rate-of-change — booming vs dying districts)
- Traffic density
- Pollution
- **Crime**
- **Land value**

**Service coverage fields** (a source map, smoothed into a radius of effect)
- Police coverage → suppresses crime
- Fire coverage → suppresses fire spread
- **Education** (schools/colleges/libraries) → city **EQ** (Education Quotient) *(new in SC2000)*
- **Health** (hospitals) → **Life Expectancy** *(new in SC2000)*

**Global aggregate meters** (one number for the whole city, drive development)
- **RCI demand** — the famous three-bar indicator: how much the city *wants* more
  Residential / Commercial / Industrial. The valve that makes zones develop or abandon.

## The dependency DAG (computation order per tick)

```
zones + census ─► population ─► PopDensity ──(smooth)──┐
                                                       │
pollution emitters (I, traffic, coal, ports, fire) ─► Pollution ──(smooth)──┐
                                                       │                     │
terrain: proximity to water/coast (+ SC2000 elevation) ─► terrainValue ──────┤
city-centre distance ────────────────────────────────────────────────────────┤
                                                       │                     ▼
   LandValue = clamp( centreProximity + terrainValue − Pollution − Crime_prev )
                                                       │
PopDensity ───────────────────────────────────────────┤
police stations ─► PoliceCoverage ──(smooth)──┐        │
                                              ▼        ▼
   Crime = clamp( base + PopDensity − LandValue − PoliceCoverage )
                                              │
   ── Crime feeds next tick's LandValue (the slum spiral) ──┘

roads + zone trip-generation ─► Traffic density
LandValue, Pollution, Crime, Traffic-access, Power, (SC2000: Water), RCI-demand
                                              ▼
   zone GrowthRate ─► zones develop ↑ / abandon ↓  ─► (next tick's population)
```

### The formula *shapes* (factors + signs are the contract; constants are tuned)

- **Land value** ↑ proximity to city centre · ↑ terrain desirability (near water/coast;
  parks) · ↓ pollution · ↓ crime. Clamped. → produces the gradient for free.
- **Crime** ↑ population density · ↓ land value · ↓ police coverage. Clamped. → worst where
  it's dense, poor, and unpoliced.
- **Pollution** = sum of emitting tiles (industry, traffic, coal plants, seaports, fires),
  then smoothed so it drifts onto neighbours.
- **Population density** = residential population per cell, smoothed.
- **Traffic** = trip generation: zones emit trips along connected roads toward compatible
  zones; traffic accumulates on road tiles. (The one layer that needs road *connectivity*,
  not just a blur — defer to the "full" phase.)
- **Service coverage** = station tiles as a source map, smoothed N times = the radius.
- **Growth/decline valve** = a zone develops when local land value is good, pollution/crime
  low, it has power + road access (+ SC2000 water), AND the global RCI meter wants that
  zone type. Otherwise it stagnates or abandons.

### The feedback loop that makes it feel alive

`Crime ↑ → LandValue ↓ → Crime ↑ …` — a slum spiral, broken only by police coverage.
That single loop (≈4 lines of math) is the most "alive" thing in SimCity. **v1 exists to
prove exactly this loop converges and that dropping a police station visibly arrests it.**

## Data model (sketch)

- A grid (start 64×64 or 96×96). Each cell: terrain type, zone type+density, structure,
  power/water flags.
- Parallel `int` field arrays per computed layer (`landvalue[]`, `crime[]`, `pollution[]`,
  `popdensity[]`, `police[]`, …). Micropolis ran several at half/quarter resolution for
  speed; start full-res for clarity, downsample later only if profiling says so.
- A fixed-step sim tick (decoupled from frame rate) runs `recompute()` over the DAG.
  Smoothing passes dominate cost; everything is O(cells × passes).
- Overlay views: number keys flip the active layer; render the field as a heat ramp over
  the base map. (Re-uses the palette; a 0–250 value → colour ramp.)

## Build plan — bare first, then full

### v1 — bare (prove the loop) — ✅ BUILT (`tools/carts/gridcity.c`, 2026-06-23)
Terrain + paintable tile roads + paintable R/C/I (single density) + layers **land value,
crime, police coverage, population density**, plus the two spreading primitives
(`smooth()` diffusion + `spread()` falloff) and the tick loop. Number keys 1–5 toggle
overlays; the cart opens on a seeded sample city with one unpoliced slum. `spec()` proves
both halves: an unpoliced dense block develops crime (87) and depressed land value (44);
dropping a station there collapses crime (→0) and land value rebounds (→87). Verified
visually — the land-value overlay shows the centre-proximity diamond gradient with the
dense core pulled down by its own crime; crime localizes to the peripheral slum.
Tuning found: `CP_MAX 100`, `CRIME_BASE 30`, police `spread(14, 18)`. Constants still loose.

### v1.5 — pollution + density + the growth valve
- ✅ **Pollution** (2026-06-23): industry emits heavy (220), commerce light (40); it
  diffuses (`smooth ×3`) and subtracts from land value (`−pollution/3`). Overlay 6.
  `spec()` pins it: industry source ~180, diffuses to 64 two cells off, drops that cell's
  land value 84→63. Density was already in v1.
- ✅ **The develop/abandon growth valve** + **RCI demand** meter (2026-06-23): every zoned
  cell carries a development `level` (0=empty lot … `LMAX`). Every `GROW_EVERY` ticks it
  builds up when local desirability (`landvalue − crime/2 − pollution/4`) clears a threshold
  AND its type is in demand, or empties out when road-starved / choked by crime+pollution /
  oversupplied. The global RCI meter is gentle by design (`base + (jobs−pop)/6` etc.) so a
  balanced city sits mildly positive and *local* conditions decide which lots grow — no
  whiplash. Painted lots start empty and fill in; the seeded city is **pre-warmed 80 ticks**
  so it opens already grown. `spec()` (now 13 assertions) proves growth from empty, that a
  road-starved zone never develops, and the meter is live — while the crime/pollution formula
  tests freeze the valve (`growth_on=0`) to stay isolated.
  Two tuning lessons: the street grid must be **dense** (every 6 cells) or most zones aren't
  road-served and never grow; and the centre-proximity falloff had to soften (`CP_FALL 1`)
  so the whole map is buildable and crime/pollution — not distance — make the variation.
- ✅ **Positive land-value drivers + schools/hospitals** (2026-06-23): land value now has
  *upward* terms so it can reach the ceiling, not just centre-proximity + water:
  - **Parks** (`G`) — a strong, local land-value boost (`spread`, ~8-cell reach).
  - **Service amenity** — police + school + hospital coverage all lift land value
    (`(police+school+health)/8`), so police now both cuts crime *and* raises value.
  - **Commercial premium** — built downtown raises nearby land value (`commv`, smoothed).
  - **Schools (`S`) + hospitals (`H`)** — new service buildings using the same `spread()`
    coverage primitive as police; they feed two SC2000 city aggregates shown in the HUD:
    **EQ** (Education Quotient 0–100) and **LE** (Life Expectancy ~60–90), averaged over
    populated cells. New **Services** overlay (7) shows combined coverage.
  `spec()` (17 assertions): a cell next to a park+school+hospital goes land value 16→250,
  EQ 0→39, LE 60→71. Magnitudes are loose — three adjacent civics saturate a cell; spread
  out in a real city it grades nicely (verified: land-value overlay now ranges green↔red).

### v2 — full
- ✅ **Light/dense zoning** (2026-06-23): each zoned cell carries a density (`D` toggles the
  brush light↔dense). Light caps development at `LIGHT_CAP` (2 = suburbs/low-rise); dense
  allows up to `LMAX` (4 = downtown towers). The growth valve respects the per-cell cap.
  City view now shades zones by development level, so the skyline is legible (dim suburbs →
  bright tall core). Seed: dense inner city, light outskirts. `spec()` (20): two identical
  served blocks, light tops out at 2, dense rises past it.
- ⬜ **Traffic** — deterministic trip generation: each developed zone BFS-walks the road
  network to the nearest *compatible* zone within a step budget and deposits traffic on the
  path; aggregate = a road-traffic field that adds to pollution, lowers land value, and
  (when congested) caps growth. The one layer needing road *connectivity*, not a blur.
- ✅ **Power grid** (2026-06-23): a power plant (`L`) floods power by boolean BFS through the
  connected built fabric (roads/zones/civic conduct; bare land + water don't). A lot needs
  **both** road access AND power to develop, so cutting a district off the grid browns it out
  and stops its growth. New **Power** overlay (8): lit (yellow) vs blackout (red). `spec()`
  (23): a road-served, in-demand zone stays at 0 with no plant, then develops once a plant is
  wired into its grid. (Flood lives in `recompute_static` — topology only changes on edit.)
- ✅ **Traffic** (2026-06-23): deterministic trip generation. Two multi-source BFS *routing
  fields* over the road network (`distJob` from job-adjacent roads, `distHome` from
  home-adjacent roads); each developed zone then walks the gradient to its nearest
  compatible destination, depositing its `level` as traffic on every road tile en route
  (capped at `TRIP_MAX` steps). Roads on many home↔job paths congest. Traffic feeds
  pollution (road fumes) → which lowers land value, so a car-dependent layout is visibly
  dirtier. Overlay 9. `spec()` (26): homes+jobs+roads generate 3197 traffic; turn the jobs
  into homes and it drops to 0 (proving trips are directed home↔job, not random motion).
- ✅ **Fire** (2026-06-23): the first *dynamic* hazard (everything else is a settling field).
  Fire stations (`F`) give a coverage field (same `spread()` primitive); the player ignites a
  building with the `B` brush. A fierce fire jumps to flammable, poorly-covered neighbours each
  tick; it loses fuel over time (coverage extinguishes it faster) and knocks development down
  as it burns — burn long enough and the lot is razed. Flames render live in the city view
  (red/orange). Overlay 0 = fire coverage + flames. `spec()` (29): an uncovered blaze razed
  256 cells, fire-station coverage held the same ignition to 111.
- ✅ **Water** (2026-06-23): a water pump (`U`) floods water through the built fabric like
  power — but a pump only draws if it sits **beside a water tile** (the lake earns its keep),
  and water is a **soft** gate: an unwatered lot still builds but can't rise past
  `WATER_DRY_CAP` (1). So power = hard on/off, water = height cap — two distinct gates from the
  same flood. Overlay reached via `[`/`]` cycle (the 11th, past the number keys). `spec()` (32):
  a powered, served, in-demand dense block tops out at 1 dry, rises to 4 once a lake-side pump
  is wired in.

**All planned layers are in** (v1 → v2 complete). gridcity now models, on one 64×48 grid:
land value · crime · police · population density · pollution · service coverage
(police/school/hospital/fire) · EQ/LE stats · the RCI growth valve · light/dense zoning ·
power · traffic (trip generation) · fire (dynamic) · water. 32 spec assertions. Future
depth if wanted: per-cell water/power *consumption* budgets, disasters beyond fire, a budget/
tax economy, or feeding a real procplaces-seeded map in as the starting zone layout.

### Open questions to resolve while building
- Field resolution: full-res vs Micropolis-style half/quarter — decide by profiling, not upfront.
- City-centre distance: a fixed seed point, or computed centroid of developed tiles?
- Terrain desirability: water-proximity (Micropolis) is in from v1.5; SC2000 elevation/hills
  is a v2 nicety — confirm we want height at all given "tile-based, none of the complex stuff."
- Traffic without a full pathfinder: how cheap a trip-generation model still reads right.
