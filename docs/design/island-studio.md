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
areas, farms as `building`, plus `land_km2` in the header. `osm-roads.js --convert` packs it to `.rvb`
for `roadview` and `sndi-check`.

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

## Next rungs

4. **Farmland**: fields spread from villages along roads onto flat ground, replacing natural cover.
5. **Bake + hand to backroads**: export the grid (heights, rivers, roads, villages) and drive it.
