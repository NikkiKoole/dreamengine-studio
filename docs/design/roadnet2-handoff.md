# roadnet2 — session handoff (2026-06-15)

Orientation for picking up the **vector-native road rebuild**. The deep plan + the junction
matrix live in [`roadnet2-plan.md`](roadnet2-plan.md); this is "where it stands, what's next,
how to run it." (v1 — the original `roadnet` cart — is the reference/testbed; its docs are
[`roadnet.md`](roadnet.md) / [`roadnet-streetlevel.md`](roadnet-streetlevel.md) /
[`roadnet-handoff.md`](roadnet-handoff.md).)

## Why v2 exists (the one lesson)
v1's street level grew **two representations of the roads** — a field (`road_at`/`grid_at`) and
a graph (`gedge[]`) extracted from it — that diverged. v2 stays **vector-native**: one graph of
spline edges, one query (`road_at` = *nearest edge*), no modular field. Started from v1's clean
rung-1..3 baseline (the elegant terrain-aware spline highways), rebuilt upward.

## Two carts
1. **`tools/carts/roadnet2.c`** — the world. What's done:
   - **Metre scale** (1 unit = 1 m; `WS=300` stretches terrain + routing distances). Calibrated:
     car reads ~130 km/h, not 10,000.
   - **Two cameras**: **MAP** overview ⟷ **DRIVE** follow-cam, a **GPS minimap**, a 20 m reference
     grid, **click-to-fast-travel**. Drivable car (`steer.c` physics).
   - **Highways = β-skeleton proximity graph** (branching corridors, not a lattice-of-knots); the
     `web` slider tunes density (sparse-tree ↔ looped-mesh).
   - **Metre-width roads**, clean `polyfill` ribbons up close, continuous lines on the map.
   - Lattice: towns 3–8 km apart, cities 25–60 km (slider ranges).
2. **`tools/carts/interchange.c`** — the junction-geometry sandbox (part B). Hand-placed roads,
   **fake-3D grade separation** (one road drawn over the other = overpass; no real z-levels).
   - **Diamond fully dialed** (✅): smooth ramps off the lane edge, lane-count consistent,
     near-side overpass connection, off-ramp tapers, clean connections.
   - **Two axes wired**: TOPOLOGY (cross / T-split / Y-split, + flip) × CLASS-PAIR (highway/arterial
     each). Junction follows the pairing: AR×AR = at-grade (crossroads / T-intersection); any
     highway = grade-separated (ramps).
   - **Live UI**: toggle buttons (topology / flip / A class / B class / ramp type) + ramp-shape
     sliders (reach / gore / taper / run-on). Keys mirror them (Y / F / 1 / 2 / T / L / ←→ / P / H).

## The junction matrix (the part-B map) — full table in roadnet2-plan.md
TOPOLOGY × CLASS-PAIR, rule = *topology is the shape; a highway present → grade-separated*.
- ✅ Cross + AR×AR (crossroads), ✅ Cross + HW×AR (**diamond**), ✅ T-split + AR×AR (**T-intersection**),
  ✅ T-split + HW×AR (ramps; trunk now begins AT the ramp merge, no centreline spear),
  ✅ **Y-split / wye** (a tangent diverge off the highway — at-grade, since a fork isn't a crossing;
  fork angle = the angle slider remapped shallow).
- 🔶 first-draft / stub: **cloverleaf**, **HW×HW stack** (Cross+HW×HW currently just overpass+ramps).

## What's next (recommended order)
1. **Nail the remaining cells in the sandbox** — the **cloverleaf** (4 clean loop ramps, reusing the
   diamond's ramp+taper machinery), then the **stack** (HW×HW system interchange — the showpiece).
   (Wye ✅ done — tangent diverge.)
2. **Bake the slider values → constants, and PORT the drawer into roadnet2.** Interface:
   *given two crossing roads (positions, dirs, classes) + a topology → draw the junction.* Wire it
   to roadnet2's road graph (where roads meet → choose junction from the matrix).
3. **Settlement placement** (parked — it's worldgen-ish): tag towns **satellite** (→ arterial into
   their city) vs **isolated** (→ reach a highway via an interchange / chain to a neighbour), so
   towns stop T-ing straight onto highway hubs. The "where interchanges go" half of part B.
4. Polish backlog: faint/dense reference grid, big node-marker dots at drive zoom, road-density
   tuning, the ~500 km scope decision.

## How to run / see it
- **roadnet2**: open the cart, ▶ run → click **GO** (or ENTER) into explore. You're in **MAP** view;
  **click a spot** to drop the car and enter **DRIVE** (arrows/WASD drive, camera follows, GPS
  inset bottom-right). **Click the GPS** to pop back to MAP; click elsewhere to fast-travel. `web`
  + the other panel sliders re-roll the network live.
- **interchange**: open it → the diamond shows. Click the panel **buttons** (or keys) to flip
  topology / classes / flip / ramp type; drag the **sliders** to shape the ramps.

## Gotchas (cost real time this session)
- **Screenshot bakes compile the whole engine, which includes `runtime/sound.h`.** A parallel agent
  often has `sound.h` mid-edit → the bake fails. Retry `node tools/make-cart.js --run …` in a loop
  until it prints "updated", or **syntax-check the cart alone** (it never includes `sound.h`):
  `clang -fsyntax-only -I runtime -DSCREEN_W=320 -DSCREEN_H=200 -DSCALE=4 -DCELL_W=16 -DCELL_H=16
  -DMAP_W=64 -DMAP_H=64 tools/carts/<name>.c`.
- **TEMP-bake dance**: to bake a specific state (a drive view, a topology), temporarily set it in
  `init()` / a default, bake, then **revert before committing** (grep for `TEMP`). The committed
  thumbnail should be the cart's normal open state.
- **`polyfill` rendered an empty fill for one screen-long quad** — subdivide long straight roads
  into short segments (see `straight()` in interchange.c).
- Distance-stepped loops must scale with `WS` (a metre re-base blew them up → a hang; fixed).

## Key decisions (frozen)
- 1 unit = 1 metre; fake-z grade separation (no real z-levels yet); highways = β-skeleton; one
  graph + nearest-edge query (no field); buildings/junctions hang off the graph; bounded ~500 km
  (leaning, not committed).
