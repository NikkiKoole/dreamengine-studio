# island studio: a finite island worth exploring

STATUS: BUILDING (2026-11-19). Rungs 1 (land), 2 (climate, cover, settlements) and a first pass of 3
(roads, farmsteads, zoom view) are built in the [`islandstudio`](../../tools/carts/islandstudio.c)
bench cart. Road density is about a third to a half of the real references; see rung 3's open list.

The world for [`backroads`](backroads.md), and a replacement for its use of the endless
[`worldnet.h`](../../runtime/worldnet.h) spine.

## Why: the endless world was boring

Driving backroads over worldnet (2026-11-19) and looking at it on the TAB map made the problem plain:

- **Nothing is unique.** An endless world made by one rule has no far side. Every 20 km is like every
  other 20 km, so there is nothing to discover.
- **Density barely varies.** Towns sit on a jittered lattice, roughly evenly spaced. There are no
  empty moors and no crowded valleys.
- **Roads have no history.** Each link is one smooth curve between two lattice nodes. They don't
  follow valleys, climb passes, hug coasts or dead-end at a farm.
- **No drainage.** A pure per-point function can't know where water goes, so rivers are a stamped
  noise band (it made a 200 m slot canyon in backroads) and valleys don't exist.

The maker's reference was [Test Drive III's maps](https://github.com/s-macke/Test-Drive-3-Maps):
small, finite, hand-placed from a kit of tiles (a 32×16 grid per map, ten maps), and remembered
because "there was so much to see, so much to discover."

## The decision: a finite island, grown

- **Finite and bounded**: an island in a 40×40 km frame. Coast to coast is 40–60 minutes of driving,
  round the coast 1.5–2 hours. Finite means the map can be **baked once with whole-map passes**
  (drainage, pathfinding) instead of a pure function of x, y.
- **Grown, not drawn**: noise for the shape, then water drains, then (next rungs) villages settle
  where land is flat, fertile and near water, and roads are found across the terrain. The empty and
  full parts come from the land itself. Hand-placed set pieces can be added on top later.

## Real references (measured 2026-11-19 from OpenStreetMap)

Fetched with `node data-tools/roadview/osm-roads.js --lean --bbox …` into the git-ignored `data/`.
Lengths are corrected for web-Mercator stretch (×cos latitude).

| Island | Land | Peak | Main roads | Minor roads | Tracks | Structure |
|---|---|---|---|---|---|---|
| Isle of Man | 572 km² | 621 m | 0.92 km/km² | 0.83 | 1.03 | Spine of moorland (nearly roadless), dense lanes everywhere else, a main-road loop, Douglas as the hub |
| Sado | 855 km² | 1172 m | 0.69 km/km² | 1.91 | 1.65 | Two forested ranges (nearly empty) with a packed farmed plain between them; one coast road all the way round, one hairpin road over the north range |

Takeaways for the generator:

- Real islands have **roughly ten times** the road length worldnet gives, much of it farm tracks.
- **Density follows the land**: crowded on flat, low, fertile ground; almost empty above a few hundred
  metres. Sado's minor roads are mostly in the plain.
- A **coast road** is nearly universal; **pass roads** over the high ground are few and winding.

Bornholm (the farmland-and-granite reference) timed out on every Overpass mirror; retry with the
bbox split into quarters.

## Rung 1: the land (built)

All on one N×N grid (256², 156 m cells), rebuilt when a slider moves (~30 ms):

1. **Shape**: distance from the centre through a two-scale domain warp, with the radius itself
   varying by direction (noise sampled on the unit circle, so it wraps). Faded to sea near the frame.
2. **Relief**: rolling hills (3-octave noise) plus **ridged** mountains (folded noise, `1-|2n-1|`,
   sharpened) inside a mountain mask that is biased toward the interior. Mountains slider at zero
   gives a low island.
3. **Drain**: priority flood (Barnes et al. 2014): grow inward from every sea cell, lowest first.
   Each land cell's downstream is the neighbour it was reached from; pits are raised to their spill
   level, which marks lakes.
4. **Flow**: visit cells in reverse flood order and add each cell's water to its downstream.
5. **Carve** (v2, 2026-11-19): valleys are cut from the **smoothed river lines**, not per cell.
   Every cell is lowered by its distance to the nearest river line with a smooth U profile; depth
   (`55 m × k × sqrt(km² drained)`) and half-width (`110 + 95 × sqrt(km²)` m) grow with the river.
   Only streams draining ≥ ~2 km² carve. Then drain, **breach** (walk each river downstream and
   lower any bed cell above the one feeding it, unless the cut would exceed 25 m), drain again.
   *(v1 lowered each river cell and blurred 3×3. A diagonal river is a chain of cells touching
   only at corners, so its neighbours stayed high: sawtooth walls, rock "teeth", diagonal bands in
   the hillshade, which the maker spotted in the zoom view. Carving every tiny stream also grooved
   even slopes with parallel 45° stripes, and the 70 % depth cap made beds run uphill near the
   coast, speckling valley floors with fake lakes.)* Lakes are pits deeper than 3 m.

Views: terrain (hillshaded bands), **slope** (flat = where farms and towns go), flow. The panel
shows land area, peak, % flat lowland, % above 300 m, river length, and the two real islands.

**Known issues**: mountain basins fill into fairly large lakes (could breach instead of fill);
rivers on very flat ground run in straight 8-direction lines; 156 m cells are too coarse to drive on
directly (backroads will add fine detail on top, as it does today).

## Rung 2: climate, cover, settlements (built)

6. **Climate**: a prevailing wind (slider). For each cell, march ~9 km upwind and find the highest
   ground the air crossed. The **rain shadow** is how much higher that ridge is than this cell
   (`exp(-shadow / k)`): air that crossed a higher ridge has rained out and sunk. Windward slopes and
   summits have nothing higher upwind, so they stay wet; a windward slope right here and high ground
   get a bonus. Blurred, because the nearest-cell march leaves streaks.
   *(First version dried the air by every metre it climbed, which made summits the driest places on
   the island, the reverse of reality. Raising the tree line then grew scrub, not forest.)*
7. **Natural cover** (before people): beach, marsh (flat, low, wet, by rivers/lakes/river mouths),
   bare rock (steep or high and steep), moor (above a tree line, slider: ~400 m bare Scottish hills to
   ~1300 m wooded mountains, lower in dry air), forest (wet,
   or moderately wet on slopes and by streams), scrub (dry), grassland (the rest). Distances to
   fresh water and to the sea come from a two-pass chamfer distance transform.
8. **Settlements**: suitability = flat × low × near fresh water or coast × fertile (rain), ×1.25
   where a river meets the sea, zero on rivers, lakes, marsh, rock and beach. A river or lake counts
   more than the sea as a water source, averaged over ~1 km and normalised to the island's best spot.
   Greedy placement best-first with a spacing (slider, 1.5–4.5 km). Size = good land within 2.5 km;
   the biggest (about one per 300 km², 10 km apart) become towns, the upper half villages, the rest
   hamlets.

Nine default seeds: forest 26–42 %, moor 0–28 %, 30–70 settlements with 2–4 towns. The **coastal**
stat (settlements within 1 km of the sea, about a fifth of the land) went from 61 % to 50 % on
average after weakening the coast pull: 35–40 % on islands with inland plains, 60–67 % on mountain
islands whose only lowland is the coast. The dry lee gets fewer villages, because fertility follows
rain: nobody coded that, it falls out.

## Rung 3: roads (first pass, built)

Not only for backroads: the island is meant as a shared world (backroads, sloop, citydrive, roadview),
so roads use the repo's road ladder and should export to the shared typed-network format.

**Routing on the coarse grid** (A*, 8 directions). Step cost = length × grade along the step
(contours are cheap; above ~14 % it climbs steeply, above 7 % for the expressway) × cover (marsh,
rock, beach, forest, moor) × a per-cell ±20 % jitter (old roads wander; flat land would otherwise get
ruler lines) × **0.3–0.4 on an existing road** (routes merge into trunks), plus a bridge cost when
entering a river or slipping diagonally between two river cells, and ×6 for running along a river.

Order: (1) the **ring**: coastal settlements in angular order, a cost that prefers a band 0.2–2 km
inland; a leg that would cost more than 4× its straight distance is dropped, so the ring breaks where
the coast is impassable (none of nine seeds broke yet). (2) **links**: the Gabriel graph of
settlements (no third place inside the circle between them), capped in length, plus every town pair
(weighted ×4), routed in order of gravity (size × size / distance^1.5). (3) **classes from traffic**:
quantiles over road cells, top 22 % main, next 43 % secondary, the rest lane; the ring is at least
main. (4) **expressway** if the two biggest towns are more than 10 km apart: gentle grades, cheaper
bridges, no merging (a new alignment). Four of nine seeds get one. (5) **village tracks** into their
best farmland. (6) **farmsteads** on a jittered ~650 m grid, kept with probability ∝ suitability,
each joined to the nearest road (Dijkstra until it touches the network; >900 m = lane, else track);
~40 % get a second connection in the opposite direction, which turns dead ends into loops.

**Geometry**: the road edges form a graph; chains between junctions are simplified (Douglas-Peucker,
0.6 cell) and re-curved (Catmull-Rom every ~20 m). Routing takes 20–55 ms; it runs on slider release.

**Zoom view** (TAB, full screen, 1–60 m/px): terrain resampled with **bicubic** heights + fine noise
(bilinear made flat triangular facets the hillshade outlined, a second "diagonal grid"), cover with
jittered cell edges, roads with casing at their real half-widths (track 2, lane 2.8, secondary 3.8,
main 5, expressway 10 m), farms as house + barn, settlements as placeholder rings.

- **Rivers** are smooth lines: chains from each source or confluence down to the next confluence or
  the sea, simplified and re-curved like roads, width `0.35 × sqrt(cells drained)` (1.5–30 m).
- **Bridges** wherever a road line crosses a river line wider than 3.5 m (narrower = a culvert):
  found with a bucket grid over river segments; drawn as a deck wider than the road, the road on
  it, parapets on both sides; length from river width and crossing angle.
- **Forestry and upland tracks**: points on a jittered grid in forest (~850 m, 65 %) and on moor or
  high scrub (~1.2 km, 55 %), each joined to the nearest road as a dead-end track.
- **The expressway is access-controlled**: farms and tracks can't join it, and other routes get no
  discount for running along it (they cross it).

**Measured** (km of road per km² of land; main = expressway + main + secondary):

| | main | minor (lane) | track |
|---|---|---|---|
| Isle of Man | 0.92 | 0.83 | 1.03 |
| Sado | 0.69 | 1.91 | 1.65 |
| islandstudio, 9 seeds | 0.22–0.47 | 0.06–0.38 | 0.19–0.65 |

Low islands reach about half the Isle of Man; mountain islands are far sparser. Known reasons: no town
streets yet (OSM minor includes them), farms only on good land, mountain interiors mostly roadless.

**Open, in rough order**: hairpins on steep legs (the grid can't make them; a geometry pass should);
real junction shapes via `roadkit.h`; town street networks (citygen); export to the `.json`/`.rvb` vector format so
`roadview` opens the island and the OSM stats tools measure it the same way as the real islands.

## Export + like-for-like measurement (built)

`X` writes `data/islandstudio-<seed>.json` in roadview's vector-features format (the IR
`data-tools/roadview/osm-roads.js` writes for real OSM): roads on the OSM ladder (expressway →
highway, main → arterial, secondary, lane → road, track), bridge spans split out with `sub: "B1"`,
rivers as `canal`, the coastline (marching squares on H = 0) as `coast`, settlements as `residential`
areas, farms as `building`, plus `land_km2` in the header. It writes the packed
`data/islandstudio-<seed>.rvb` next to it in the same step (what `backroads`, `roadview`, `citydrive`
and `sndi-check` load), byte-identical to `osm-roads.js --convert` of that JSON, so there is no manual
convert any more. (To re-check the parity: `--convert` the JSON to a scratch path and `cmp` it.)

The export also carries **land cover as areas** (runs of one class per row on a 128 grid: forest →
`green`/wood, grassland → `farm`, beach → `sand`, sea and lakes → `water`, moor/scrub/rock/marsh →
`other_area` with their natural tag) and a **heightfield** (`"heightfield": {g, z}`, up to 256², rows
south → north), packed as RVB3 in the `.rvb`. So **`citydrive` drives an exported island today**:
`DE_DATA=$(pwd)/data/islandstudio-1.rvb node tools/play.js citydrive run` (or drag the `.rvb` onto
it), key 3 for the perspective camera, `'` to raise the eye. It spawns at the densest buildings (farms)
and draws terrain only near the camera, so it's a quick look, not the target view.

`node tools/road-density.js <file.json> [--lat L --area KM2] [--svg out.svg]` measures it exactly as the
real islands were measured (and renders a true-scale SVG for side-by-side looks).

**First comparison (seed 1 vs the Isle of Man, `sndi-check` on driveable roads, no tracks):**

| | seed 1 | Isle of Man |
|---|---|---|
| circuity | 1.55 | 1.57 |
| sinuosity | 1.17 | 1.13 |
| orientation entropy | 0.94 | 0.99 |
| dead ends | **52 %** | 35 % |
| T junctions | 47 % | 56 % |
| crossroads (4+) | **1 %** | 5 % |
| dendricity | 0.31 | 0.25 |

How roads wind and how roundabout trips are already match. The **topology** doesn't: too many dead
ends and almost no crossroads. Side by side at the same scale, the Isle of Man has a **mesh of main
roads** every 5–8 km joining towns and villages directly; ours is the ring plus a few trunks, with the
interior left to lanes and stubs. (`.rvb` lengths/areas are web-Mercator for the real islands, so
compare sndi's shape metrics, not its km or km².)

## Calibration round 1 (2026-11-20)

Loop: export six seeds, run `road-density.js` + `sndi-check --json` on each, compare with the Isle of
Man and Sado, change one thing, repeat. Changes that moved the numbers, in order:

1. **Weaker merge discount** (0.3/0.4 → 0.55/0.65 on existing roads, 0.75 → 0.85 for joining one).
   Every route had collapsed onto a trunk, so new roads merged (T) instead of crossing (X):
   dead ends 43–63 % → 17–23 %, crossroads 0–3 % → 4–9 %, dendricity into range.
2. **A class floor from what a link joins**: town–town = main; anything touching a village or town =
   secondary (B/C roads, OSM tertiary+, which the real "main" figure includes); hamlet–hamlet = lane.
   Traffic can still raise a road, never lower it.
3. **Farm access with one way out is a track**; a farm with a second way out (45 %) turns both legs
   into a lane (a public through road).
4. **Field tracks**: every farm gets a dead-end track 300–650 m into its own fields.
5. **The village network**: the same Gabriel rule over villages + towns only, so a village-to-village
   road stays a B-road when it passes a hamlet (these mostly upgrade existing roads).
6. **Denser farms** (~500 m), on a **fractional** jittered grid: whole-cell spacing made the slider
   jump between ~310 m and ~470 m and density swing with it.
7. **A silent-cap bug**: the road-line pool held 3,000 lines and dropped the rest, which ate whole
   main roads once farms got denser ("more farms, less road"). Pools raised (16k lines, 600k points,
   6k farms, 2k bridges) and every overflow is now counted (`st_overflow`, red warning in the panel,
   `overflow` in the trace). It must stay 0.

| | main | minor | track | dead | T | X | dendr. | circuity |
|---|---|---|---|---|---|---|---|---|
| Isle of Man | 0.92 | 0.83 | 1.03 | 35 % | 56 % | 5 % | 0.25 | 1.57 |
| Sado | 0.69 | 1.91 | 1.65 | 20 % | 68 % | 12 % | 0.14 | 1.40 |
| seed 1 (low) | 0.57 | 0.69 | 1.04 | 19 % | 72 % | 8 % | 0.17 | 1.61 |
| seed 6 (low) | 0.75 | 0.61 | 1.01 | 20 % | 72 % | 8 % | 0.18 | 1.45 |
| seeds 2, 4, 5 (mountain) | 0.29–0.36 | 0.12–0.21 | 0.41–0.56 | 18–23 % | 70–73 % | 4–11 % | 0.20–0.24 | 1.40–1.94 |

Low islands now sit at or near the real ones on every metric; side by side they read as the same kind
of network (a mesh of B-roads joining every village, a coast ring, lanes and tracks between). Mountain
islands are sparser per km², which is honest (a big empty mountain lowers the average). Still off:
T-junction share a little high, dead ends at Sado's level rather than the Isle of Man's, and the coast
ring stair-steps where it hugs a jagged coast. Routing takes up to ~100 ms (on slider release).

## Resolution: 256 live, 512 / 1024 to bake (2026-11-20)

The grid size is a runtime setting (`G` key or the RES button): 256 (156 m cells) for live sliders,
512 (78 m) or 1024 (39 m) for a final bake. Arrays are sized for 1024; everything that used to count
**cells** now counts metres or km² (`AREA_K` scales the drained-cell thresholds; settlement, climate,
farm and track radii are in metres), so the same seed gives the same island at every resolution, only
sharper. The overview always draws 256 px and samples the grid; the export and zoom use full detail.

| grid | terrain | roads |
|---|---|---|
| 256 | 66 ms | ~85 ms |
| 512 | 0.29 s | 0.21 s |
| 1024 | 1.7 s | 0.85–1.0 s |

At 1024 roads follow the terrain in long curves instead of straight runs with kinks (sinuosity 1.2,
between the Isle of Man's 1.13 and Sado's 1.35), and densities hold (seed 1: 0.56 / 0.80 / 1.07).

Fixed along the way:
- **Hidden caps**: the path smoother read only the first 1,000 cells of a chain (a 1024 road can be
  4,000); a quadratic insertion sort on traffic and a per-cell scan of every settlement (`is_node`)
  would have made 1024 crawl. All replaced (buffers sized from `MAXN`, `qsort`, an `IS_SET` grid).
- **Roads crossing diagonally between cells** (one NW-SE, one NE-SW in the same 2x2 block) shared no
  cell, so there was no junction: they passed through each other. 9-24 per island; one is now
  rerouted around the corner (`fix_diagonal_crossings`).
- **Staggered junctions**: at 39 m cells two side roads that met "almost opposite" landed a cell or
  two apart (two Ts instead of a crossroads: crossroads fell to 2-4 %). Routes now get a discount
  for passing through an existing junction (`JN >= 3`, x0.6): 3-5 % at 1024, 8-15 % at 256.
- **Zoom cover edges** were softened by ~1 cell, which showed stair-steps at 39 m cells; now a fixed
  ~140 m + 30 m of jitter.

## Smooth roads at 1024 (2026-11-20)

Driving the 1024 island in backroads, main roads had **teeth, hooks and little U-loops** (the maker's
map screenshot). Measured as sharp kinks (> 60° turn within ~20 m either side) per 100 km of road:

| | main | secondary | lane | track |
|---|---|---|---|---|
| Isle of Man | 7 | 8 | 78 | 71 |
| seed 1 @1024, before | 62 | 76 | 78 | 66 |
| seed 1 @1024, after | 30 | 33 | 41 | 46 |

Causes and fixes (rendering close-ups of the kinks found them; guessing didn't):
- The "old roads wander" jitter was **random per cell**: gentle at 156 m, a zigzag at 39 m. Now a
  smooth noise field (~300 m features).
- Routes **ran alongside** existing roads instead of on them (the weaker merge discount from
  calibration): at 39 m that's a second road 39 m away, jogging into the first wherever they touch,
  with tiny triangles in the cell graph chopping it into hooks. Now a cell within ~60 m of a road
  but not on it costs ×1.6 ("join it or keep away"); below 60 m/cell (the 256 grid) it is off.
- **Corner cut**: a plain bend a→b→c with a, c neighbours becomes a→c (835 cuts on seed 1),
  ironing out right-angle steps incl. the diagonal-crossing reroute.
- The junction discount (×0.6 through an existing junction) made routes detour; it's off.

What's left are mostly real turns at junctions (a main road turning at a T). Calibration at 256 holds
(crossroads 3.7–9.2 %); at 1024 crossroads are now 2 % (staggered T pairs): turning near T pairs into
crossroads belongs in a graph pass, not in routing detours. Open.

## Junction consolidation (2026-11-20)

On fine grids (cells < 100 m), two junctions within three road steps and ~90 m of each other become
one: the side roads of junction b move onto junction a (a straight cell run), b becomes a plain bend,
and the corner cut runs again. Never moves a settlement node or the expressway, and never makes a
fork narrower than 35°. Staggered T pairs become crossroads; slivers of field between near
junctions go. Seed 1 @1024 (393 consolidations), against the Isle of Man:

| | dead ends | T | crossroads | dendricity | circuity |
|---|---|---|---|---|---|
| Isle of Man | 35 % | 56 % | 5.2 % | 0.25 | 1.57 |
| before | 29 % | 69 % | 2.1 % | 0.23 | 1.46 |
| after | 35 % | 60 % | 4.8 % | 0.28 | 1.59 |

Open: every route into a town ends at the town's centre cell, so towns are 6–8-arm stars whose
corner fillets leave holes in backroads. That's for town streets (citygen), not more patching.
(Done: see "Town streets" below.)

## Grade separation (2026-11-20)

The expressway is its own **layer** in the road graph: node-ness is counted per layer, so where it
crosses another road the two run through each other with no junction. Every such crossing (except a
town the expressway ends in) gets a **260 m bridge span** on the expressway (`build_flyovers`),
exported with the river bridges' `B1` tag; the crossing road must pass THROUGH (≥ 2 arms: a dead end
touching the expressway isn't a crossing). backroads raises them (see `backroads.md`).

**Interchanges (diamonds)** at flyovers over a public road (lane or better), ≥ 3 km apart and ≥ 1.5 km
from the expressway's ends: four slip roads, drive-on-the-right, each `roadkit.h`'s `scurve_spline`
(roadlab's biarc) from the expressway's edge ~320 m before/after the bridge to the crossing road ~110 m
out. The ramp ends are snapped onto the crossing road and the road is **split** there, so backroads finds
a real junction; an interchange is skipped unless the road is there on both sides (a dead-end crossing
made ramps that met only each other). Seed 3 @1024 (a 9 km expressway): 17 flyovers, 1 diamond. The
expressway still meets the network at grade where it ends (in towns).

## Next rungs

4. **Farmland**: fields spread from villages along roads onto flat ground, replacing natural cover.
5. **Bake + hand to backroads**: export the grid (heights, rivers, roads, villages) and drive it.

## Town streets (2026-11-20)

Towns and villages used to be stars: every route ended at one centre cell. `town_streets()` now
gives each a street grid, using `citygen.h`'s district-fill method (rung 4: a rotated lattice, a
spanning tree for connectivity plus a fraction of loops) re-done **on the road grid**, so streets go
through the same smoothing, corner cut, junction consolidation, polyline splitting and export as
every other road. `citygen.h` itself isn't included: it reads `worldnet.h`'s terrain, not this
island's, and its building/lot layer was never finished anyway (worldgen-plan rung 7).

- A lattice of ~130 m blocks out to a **built-up radius** set by the **town size** slider (biggest
  town 300 m .. 1.5 km, default 600 m; other places scale with sqrt(their size), 0.7–1.0 of that;
  villages 0.38 of a town). The slider only re-routes roads. Farms keep 150 m clear of it, and the
  export's residential circle uses it, clamped so backroads' rank thresholds hold (town > 450 m,
  village 190–440 m). At 0.8 seed 3's biggest town reaches ~1.3 km: minor 0.70, X 18 %.
- The lattice is turned to follow the biggest road
  into the centre (walked ~250 m out), with a ragged edge and more jitter outside the core.
- Lattice nodes near an existing road **snap onto it**, so the routes in become the main streets;
  streets never touch water, a river cell, the expressway or slopes over 12 %.
- Loops: 85 % in the core (closed blocks, crossroads allowed), 40 % outside (Ts, cul-de-sacs).
- Street cells are rasterized with Bresenham: the first version walked diagonal-then-straight, an L
  that the smoother turned into an S-wiggle (towns looked like spaghetti).
- Pieces cut off by water join the town by a short route, or stay out.
- Exported as `road` (lane class), so backroads' contract is unchanged.

At 256 (156 m cells, one street per cell) it barely shows and calibration holds (seeds 1/3/6:
minor +0.01–0.02, X 7–9 %). At 1024, against the stars:

| | minor | dead | T | X | circuity |
|---|---|---|---|---|---|
| Isle of Man | 0.83 | 35 % | 56 % | 5.2 % | 1.57 |
| seed 1 before | 0.74 | 35 % | 60 % | 4.8 % | 1.59 |
| seed 1 after | 0.81 | 34 % | 56 % | 9.8 % | 1.46 |
| seed 3 before | 0.46 | 35 % | 60 % | 5.1 % | 1.62 |
| seed 3 after | 0.52 | 34 % | 55 % | 11.2 % | 1.45 |

Crossroads now sit between the Isle of Man (5 %) and Sado (12 %): town-centre grids are where real
crossroads are. Open: old field tracks from nearby hamlets still cross some towns; houses/lots
(citygen rung 7's unfinished half) for when backroads drives into villages.

## Kinks on main roads (2026-11-20)

The kink count from "Smooth roads at 1024" is now a tool: `node tools/kink-check.js <file.json>
[--lat L] [--where main]` (sharp kinks per 100 km by class; `--where` lists the spots to render).
It reproduces the Isle of Man's main-road 7; Sado's main roads have 32 (mountain hairpins are real).

Close-ups of seed 3's main-road kinks showed one cause: a **one-cell sideways jog** (39 m) in the
middle of a road. Simplification kept any cell more than 0.6 cell (23 m) off the line, so the jog
stayed and the spline drew it as a hook. Main roads now simplify with a 45 m floor, secondaries
35 m (`sm_tol_min` in `emit_poly`); lanes, tracks and rivers are unchanged, and so is the 256 grid
(0.6 cell = 94 m there already; the export is byte-identical).

| sharp kinks / 100 km | main | secondary | lane | track |
|---|---|---|---|---|
| Isle of Man | 7 | 13 | 108 | 104 |
| Sado | 32 | 37 | 132 | 180 |
| seed 1 @1024 before | 30 | 22 | 31 | 31 |
| seed 1 @1024 after | 4 | 11 | 31 | 31 |
| seed 3 @1024 before | 26 | 27 | 34 | 33 |
| seed 3 @1024 after | 3 | 14 | 34 | 33 |

Side effects checked on seed 3: no main or secondary road over the sea, bridges 164 → 163, flyovers
15 → 14, main-road km −2 % (straighter). Lanes and tracks are now *smoother* than the real islands
(31–34 vs 104–180), which may be worth roughening later; not a defect for driving.
