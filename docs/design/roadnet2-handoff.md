# roadnet2 — session handoff (2026-06-15)

STATUS: BUILDING — vector-native rebuild; metre base + two-camera harness done; the unified
`road_at()` graph query SHIPPED + EXTRACTED to `runtime/worldnet.h` (2026-07-06); street
generation continues on the worldgen ladder.

> **2026-07-06 — this handoff is partially superseded.** Worldgen rung 1 landed: the world core
> (terrain + lattice + `link_path` + the new edge-graph `road_at()` query) was **extracted to
> `runtime/worldnet.h`**, and sloop drives the spine behind its own seam (N key). The car in this
> cart reads the surface (off-road drag, HUD class tag, C = spawn snapped onto a road, G = the
> graph-vs-strokes overlay). **The live thread + resume point is
> [`worldgen-plan.md`](worldgen-plan.md)** (rungs 0–3 shipped; rung 4 pick-up note there); this
> file remains accurate for the two-camera harness, the β-skeleton, and the interchange sandbox.

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
   > **2026-06-16: refactored into composed ATOMS** (`near_pair` / `loop_ramp` / `flyover`), added a
   > `spread` slider, atom-isolation toggles, lane direction arrows, and rebuilt the trumpet from the
   > atoms (geometry still rough). **Dedicated, current handoff:
   > [`interchange-handoff.md`](interchange-handoff.md) — read that for interchange work.**
   - **Diamond fully dialed** (✅): smooth ramps off the lane edge, lane-count consistent,
     near-side overpass connection, off-ramp tapers, clean connections.
   - **Live UI = THE junction matrix** (data-driven `JUNCS[]`): every supported junction is one
     clickable button — `cross / diamond / clover• / stack•` · `tee / half / trumpet` · `fork / wye`
     (rows = topology, columns = that row's variants; • = red dot = stub/draft). One click sets the
     WHOLE config (topology + both classes + ramp-style); active framed yellow; hover → bottom-strip
     tooltip with full name + expected render + build status. Keys are now just modifiers:
     **F** flip · **◄►** angle · **L** lanes · **P** panel · **H** hud. Plus ramp-shape sliders
     (reach / gore / taper / run-on). To inspect a junction unobstructed, **P** hides the panel.

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
- 🔶 T-split + HW — **trumpet** FIRST PASS (itype `T_TRUMPET`): trunk comes down to a *throat* just
  above the highway; a `draw_loop()` teardrop (~300°) springs tangent from the throat; a flyover
  connector crosses the centreline to the FAR carriageway and merges tangent to the highway. The loop
  reads. STILL ROUGH → the loop-end and the connector only *touch* the lanes (need short tangent
  **merge stubs**); the `flip`/ss side is uneyeballed; reach/gore/taper/run-on aren't wired into the
  loop yet (only the connector). **This is the next thing to refine.**
- ⛔ to build: **cloverleaf** (old tangled loop-circles — rebuild on `draw_loop`, 4 loops), **HW×HW
  stack** (showpiece), parclo / half-trumpet.
- The `itype` enum is now OVERPASS / DIAMOND / CLOVERLEAF / **TRUMPET**. Each `JUNCS[]` row carries its
  own ramp-style, so the matrix button picks it directly (no separate ramp-style control anymore).
- **`draw_loop(cx,cy,rad,a0,sweep)`** is the reusable teardrop primitive (ribbon arc) — it's what the
  cloverleaf's 4 loops + parclo will copy. Get it dialed on the trumpet first.

## What's next (recommended order)
1. **Refine the TRUMPET (in progress).** The `draw_loop()` teardrop works; what's left is the *merge*:
   short tangent stubs so the loop-end and the flyover connector kiss the highway lanes (right now they
   just touch); verify the `flip` (ss=+1) side; wire reach/gore/taper into the loop (radius/centre),
   not only the connector. Open the cart → click **trumpet** (the bake's default is CROSS). Decide with
   Nikki what "right" looks like (merge into specific lanes? gore-nose triangles? one-way arrows?).
2. **Cloverleaf** — rebuild the (currently tangled) `clover` cell on `draw_loop`: 4 teardrops, one per
   quadrant, + 4 outer ramps. Then the **stack** (HW×HW showpiece). Lower priority: parclo, half-trumpet.
   (Wye ✅, half-diamond ✅ partial, trumpet 🔶 first pass.)
3. **Bake the slider values → constants, and PORT the drawer into roadnet2.** Interface:
   *given two crossing roads (positions, dirs, classes) + a topology → draw the junction.* Wire it
   to roadnet2's road graph (where roads meet → choose junction from the matrix).
4. **Settlement placement** (parked — it's worldgen-ish): tag towns **satellite** (→ arterial into
   their city) vs **isolated** (→ reach a highway via an interchange / chain to a neighbour), so
   towns stop T-ing straight onto highway hubs. The "where interchanges go" half of part B.
5. Polish backlog: faint/dense reference grid, big node-marker dots at drive zoom, road-density
   tuning, the ~500 km scope decision.

## How to run / see it
- **roadnet2**: open the cart, ▶ run → click **GO** (or ENTER) into explore. You're in **MAP** view;
  **click a spot** to drop the car and enter **DRIVE** (arrows/WASD drive, camera follows, GPS
  inset bottom-right). **Click the GPS** to pop back to MAP; click elsewhere to fast-travel. `web`
  + the other panel sliders re-roll the network live.
- **interchange**: open it → CROSS/diamond shows. **Click any junction button** in the matrix to
  switch (cross / diamond / clover• / stack• · tee / half / trumpet · fork / wye). Hover a button for
  its tooltip. **F** flip, **◄►** angle, **L** lanes, **P** hide-panel-to-inspect. Drag the sliders to
  shape the ramps.

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
