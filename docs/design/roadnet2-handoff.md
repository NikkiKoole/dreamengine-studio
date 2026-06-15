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
THREE axes: TOPOLOGY (cross / T-split / Y-split) × CLASS-PAIR (AR×AR at-grade / HW×AR / HW×HW;
*any highway → grade-separated*) × **COMPLETENESS** (the ramp-STYLE knob): **full** interchanges
serve every movement (diamond / cloverleaf / trumpet / stack); **partial / half** ones serve a
deliberate subset (half-diamond / parclo / half-trumpet) — a *legit* type you pick on purpose, not a
broken full. So a grade-sep cell is a MENU of ramp-styles, not one junction.
- ✅ Cross + AR×AR (crossroads), ✅ Cross + HW×AR (**diamond**, full), ✅ T-split + AR×AR (**T-intersection**),
  ✅ **Y-split / wye** (tangent diverge off the highway — at-grade, since a fork isn't a crossing).
- ✅ T-split + HW — **HALF-DIAMOND** (the two near-side ramps + trunk-at-merge; serves ONE carriageway —
  a deliberate *partial*. **Relabeled** from the earlier mistaken "trumpet": it only connects the near
  side, which is correct *as a half-diamond*).
- ⛔ T-split + HW — **trumpet** (the FULL version: bridge-over + loop + connectors → both carriageways).
- 🔶 / ⛔ to build: **cloverleaf** (itype stub), **HW×HW stack** (Cross+HW×HW just overpass+ramps now),
  parclo / half-trumpet.
- NOTE: the `itype` enum is still OVERPASS/DIAMOND/CLOVERLEAF — it needs to grow into per-topology
  ramp-style menus (T-split: HALF-DIAMOND ↔ TRUMPET; Cross+HW×HW: STACK ↔ CLOVERLEAF).

## What's next (recommended order)
1. **Nail the remaining ramp-styles in the sandbox.** The big one is the **loop ramp** (teardrop) —
   build it once and it drives both the **trumpet** (T-split, full: 1 loop + connectors + the arterial
   bridges OVER to reach both carriageways) and the **cloverleaf** (Cross, 4 loops). Then the **stack**
   (HW×HW system interchange — the showpiece). Lower priority: parclo, half-trumpet.
   (Wye ✅ done — tangent diverge. T-split half-diamond ✅ — but it's the *partial*; the full trumpet
   is still to build.)
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
