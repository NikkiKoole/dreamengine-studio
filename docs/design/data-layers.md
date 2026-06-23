# data-layers — the SimCity simulation behind the map (design seed)

**Status: DESIGN SEED (2026-06-23).** A new experiment / pillar the big-game backlog
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
- ⬜ **The develop/abandon growth valve** driven by a global **RCI demand** meter — zones
  gain/lose a development *level* over time instead of being instantly full, so the map
  starts moving on its own. (Next; will change the seed + spec, since painted zones will
  grow from empty rather than start dense.)
- ⬜ **Positive land-value drivers** (answers "nothing pushes values *really* high yet"):
  parks/amenity bonus, a direct lift from police/fire coverage, commercial-core premium —
  so land value isn't capped by centre-proximity (100) + a local water bonus.

### v2 — full
Densities (light/dense), power grid flood, water pipes, traffic (trip generation along
roads — the one non-blur layer), service coverage for fire/education/health, and the EQ/LE
aggregate meters. This is where land value's full factor set (water/hills proximity, parks,
the lot) comes in.

### Open questions to resolve while building
- Field resolution: full-res vs Micropolis-style half/quarter — decide by profiling, not upfront.
- City-centre distance: a fixed seed point, or computed centroid of developed tiles?
- Terrain desirability: water-proximity (Micropolis) is in from v1.5; SC2000 elevation/hills
  is a v2 nicety — confirm we want height at all given "tile-based, none of the complex stuff."
- Traffic without a full pathfinder: how cheap a trip-generation model still reads right.
