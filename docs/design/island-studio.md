# island studio: a finite island worth exploring

STATUS: BUILDING (2026-11-19). Rung 1 (land: shape, relief, drainage, valleys) is built in the
[`islandstudio`](../../tools/carts/islandstudio.c) bench cart. Roads and villages come next.

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
5. **Carve**: lower each cell by about `sqrt(drained area)`, blurred so valleys are wider than one
   cell, then drain again for the final rivers.

Views: terrain (hillshaded bands), **slope** (flat = where farms and towns go), flow. The panel
shows land area, peak, % flat lowland, % above 300 m, river length, and the two real islands.

**Known issues**: mountain basins fill into fairly large lakes (could breach instead of fill);
rivers on very flat ground run in straight 8-direction lines; 156 m cells are too coarse to drive on
directly (backroads will add fine detail on top, as it does today).

## Next rungs

2. **Settle**: a suitability field (flat, low, near water/coast, river crossings) → villages and a few
   towns, sized by how much good land surrounds them.
3. **Roads**: least-cost paths between settlements over slope and river crossings (bridges cost),
   reusing shared segments so a hierarchy forms from use; a coast road; farm tracks and dead ends.
   Compare road density against the table above.
4. **Bake + hand to backroads**: export the grid (heights, rivers, roads, villages) and drive it.
