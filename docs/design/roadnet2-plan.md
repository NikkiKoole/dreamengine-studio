# roadnet 2 — the vector-native rebuild (plan / sketch, 2026-06-15)

A clean restart of roadnet's street level, keeping the elegant L0 spline-highway core and
fixing the one architectural mistake that made v1 messy. **This is a design sketch, not built
yet.** Cart: [`tools/carts/roadnet2.c`](../../tools/carts/roadnet2.c) (currently = v1's clean
rung-1..3 baseline, commit `c16c9a2`, verbatim — the foundation). v1 lives on at
[`roadnet.md`](roadnet.md) / [`roadnet-streetlevel.md`](roadnet-streetlevel.md) /
[`roadnet-handoff.md`](roadnet-handoff.md) as the reference + testbed (don't delete it).

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
2. **Unified `road_at` = nearest-edge** over the *highway* edges first (prove the query +
   spatial index on the tier we already have), render as strokes. No field anywhere.
3. **Collector tier as a warped grid**, generated directly as spline edges into the graph,
   connected to the highways. Per-district straight-vs-warped by the zone field.
4. **Access + cul-de-sacs** as the finest spline tier; the palette per district.
5. **Buildings on edges** (port v1's `building_at` + parcels).
6. **Drive it** — wire sloop: `road_at` (surface), `building_at` (collision), graph (routing).

## Honest caveats (so v2 doesn't repeat v1)
- **Empty looked clean.** Any fill-in is busier than blank terrain; "clean" comes from
  *restraint* (fewer tiers, bigger blocks, denser/larger buildings so road:building reads
  right), not from the fresh file. Design the fill-in sparing.
- **Spatial index is mandatory**, not optional — without it the nearest-edge query is too slow
  for collision once there are many edges.
- **Sketch the control-point scheme for each deeper tier on paper before coding** — that's
  where it lives or dies, and it's the part v1 never designed (it fell out of a field instead).
