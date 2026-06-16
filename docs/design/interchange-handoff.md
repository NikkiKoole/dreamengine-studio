# interchange — session handoff (2026-06-16)

Where the `interchange.c` junction-geometry sandbox stands after the **atom-decomposition** pass,
and what's next. This is roadnet2's **part-B** (how roads cross/connect a highway); the bigger
picture + the junction matrix live in [`roadnet2-plan.md`](roadnet2-plan.md) and
[`roadnet2-handoff.md`](roadnet2-handoff.md). Cart: [`tools/carts/interchange.c`](../../tools/carts/interchange.c).

## ⭐ Reference: roads.org.uk/interchanges
**THE reference for interchange geometry: <https://www.roads.org.uk/interchanges>** — clean,
colour-coded-by-movement diagrams of every junction type. Far better than tracing fuzzy GIFs.
**Caveat: it's British → drive-on-LEFT.** The *shapes/topology are identical* to ours; only the
handedness is mirrored. So: read the British diagram, **mirror left↔right** for our drive-on-right.
The planned `DRIVE` constant (see below) makes that mirroring a one-place flip, so we can consume
these diagrams directly without re-deriving chirality by eye.

## Update 2026-06-16 (pass 2): trumpet loop rebuilt + a second loop started
Worked the trumpet's far-side loops with the owner via annotated screenshots (red = ideal path,
X = the broken spot — now the standard way to drive this geometry work; see the
`annotated-screenshots-collab` memory). State at this commit:
- **`loop_ramp` (OUTER loop, trunk → far carriageway-west) — DONE & good.** Rebuilt from the old
  tangle into: trunk runs up tall and *becomes* the loop's straight side (**candy-cane**); **tangent
  entry** off the rightmost lane; **180° bell** over the top on the far side; **reverse-curve exit**
  (bend one way round, then bend the OTHER way to flatten tangent into the carriageway — no cusp).
  Widened to the **whole northbound carriageway** so every trunk lane can get on it.
- **Draw order fixed:** the trumpet ramps now draw AFTER road B, so the loop sits ON TOP of the
  trunk bridge and reads clearly (was buried under it).
- **Trunk poke:** for the trumpet the trunk now crosses the highway and pokes past the far edge up
  to the loop entry (`n0` in `draw()` keyed to the loop reach) — forms the candy-cane straight side.
- **New "loop end" slider** (`s_loopend` → `Geo.endk`): live-tunes how long the loop exit runs along
  the carriageway as it merges. 5th slider; matrix shifted down to fit.
- **`loop_inner` (INNER loop, far carriageway-west → trunk-south) — WIP / first pass.** The second,
  tighter loop that nests inside the outer so the two read as **one big circular form** (owner's
  priority). 270° bell, tangent entry off the westbound carriageway, exit down the southbound trunk.
  Geometry roughed in & nesting not yet dialed — refine next pass against the (mirrored) British ref.

### Next pass (the agreed plan)
1. **Introduce `DRIVE` (+1 = right, −1 = left)** as the single source of truth for handedness; have
   the ramp atoms derive "which side is the driving side" from it (a `right_of(dir)` helper). Kills
   chirality guesswork and lets us mirror British refs in one place.
2. **Extract reusable patterns** the loop work surfaced — `diverge(road,dir)` (peel off the
   driving-side outer lane, tangent) and `merge_curve(fromPt,fromDir → road)` (the reverse-curve
   align-and-merge). Then `loop_ramp`/`loop_inner`/`flyover`/cloverleaf are all compositions of the
   same vocabulary = the roadnet2 drawer interface.
3. **Finish the inner-loop nesting** against the mirrored roads.org.uk trumpet diagram.
4. **Support both far-side variants** — loop+flyover (classic) vs loop+loop (the owner's ref) — as a
   per-junction choice, same as the matrix already selects configs.
5. The parked **half-diamond draw-order** fix (near ramps should merge at-grade, not duck under the
   highway).

## The big idea this session: junctions = composed ATOMS
Instead of each junction type being bespoke code, an interchange is now a **composition of reusable
ramp atoms**, each a small helper reading a shared `Geo` context (so no long param lists):

| atom | what it draws | key params (from `Geo`) |
|---|---|---|
| `near_pair(g, sy)` | the **half-diamond fan** — two ramps tying the trunk to ONE carriageway (`sy`=±1 side) | reach, spread, gore, taper, run-on |
| `loop_ramp(g, ss)` | the **teardrop loop** (~300°) reaching the FAR carriageway, +pp side, with an exit merge stub | reach (radius = reach·0.55) |
| `flyover(g, ss)` | the **semi-direct ramp** trunk → far carriageway, −pp side, drawn over the highway | reach |

`Geo` = `{cx,cy, ux,uy (trunk dir), hw,bw (half-widths), reach,spread,gore,taper,runon, ang}`,
built once in `draw()` from the sliders.

**Junction recipes (the payoff — tune an atom once, it's consistent everywhere it's used):**
- **half-diamond** = `near_pair(ss)` (near carriageway only)
- **diamond** = `near_pair(-1) + near_pair(+1)` (both carriageways)
- **trumpet** = `near_pair(ss) + flyover(ss) + loop_ramp(ss)` — literally "half-diamond + the loop
  on the other side" (the owner's framing; matches the reference GIF)
- **cloverleaf** = `near_pair` both sides + 4 **inline** loop circles (NOT yet on `loop_ramp` —
  still the old tangled inline arcs; rebuild on the `loop_ramp`/`draw_loop` primitive later)

## The 4 ramp-shape sliders (reading order in the panel)
Tune by feel; **bake the chosen values as constants when porting the drawer into roadnet2**.
- **reach** — ramp length: `R = 24 + s·66` px
- **gore** — how far apart the two `near_pair` ramps land along the highway (the side-spacing):
  `goreM = 1.0 + s·1.0`, used as `hx = cx ± R·goreM`
- **taper** — `kA` off-ramp taper handle: `0.5 + s·1.3` ×reach
- **run-on** — `kB` run-on handle along the overpass: `0.4 + s·1.0` ×reach

> **Tried & reverted (2026-06-16):** we briefly split a separate **`spread`** slider out of `gore`
> (spread = highway-end spacing, gore repurposed to trunk-end nose tightness) — then reverted to the
> original 4 sliders to keep focus on the trumpet geometry. **To revisit later.** The `near_pair`
> atom still takes `spread` + `gore` as *internal* geometry params; `Geo` just feeds them the old way
> (`spread = R·goreM`, `gore = BW·0.35` fixed), so re-splitting them is a small change when we return.

## Debugging tooling added this session
- **Atom-isolation toggles** (`near` / `loop` / `fly` buttons in the panel header; green frame = on).
  They mask which atoms draw — flip to just `loop` to tune the teardrop with nothing else in the way.
  **This is the way to work on the trumpet geometry.** They gate the diamond/half-diamond `near_pair`,
  the cloverleaf loops, and all three trumpet atoms.
- **UI toggle button** (top-left `UI -`/`UI +`, in the panel area). Persists when the panel is
  hidden, so you can declutter for a clean screenshot and click it back. (`P` key still works.)
- **Lane direction arrows** — filled yellow triangles down each lane (drive-on-right: on the
  horizontal highway top carriageway = ←, bottom = →). On highway, arterial, and the crossing
  highway/trunk. Makes it obvious which carriageway a ramp feeds — essential for judging trumpet merges.
- **Crossing highway markings** — when road B / the trunk is a highway it now draws its own median +
  dashed lane lines (was a plain grey slab). `lane_marks()` = the angled-road version of `hdash`.

## Trumpet status: structurally done, geometry ROUGH (this is the next work)
The trumpet now composes from the atoms and **reads as a trumpet** (fan + loop + flyover), but the
loop/flyover geometry is a **first cut**:
- **Loop is too small** (`reach·0.55`) and **crowds the trunk** — should be a big sweeping bell.
- **The loop's exit stub doesn't land tangent** on the far carriageway — it dives toward centre
  instead of merging cleanly into a lane.
- **Flyover overlaps the near-fan** near the throat → busy centre.

### Next steps (recommended order) — use the atom toggles to isolate each
1. **Tune `loop_ramp`** (toggle off `near`+`fly`, view the loop alone): enlarge radius toward ≈`reach`,
   push the centre further out on +pp so it stops crowding the trunk, and make the **exit stub land
   tangent** on a specific far-carriageway lane (right now it just touches). Decide what "merge" means
   with the owner — into a specific lane? gore-nose triangle? one-way arrows on the ramp?
2. **Tune `flyover`** (toggle to just `fly`): clean semi-direct curve to the far carriageway that
   doesn't fight the near-fan at the throat.
3. **Decide loop-vs-flyover side** (open design Q): the reference GIF nests them on the **same side**
   (loop inside the flyover's sweep); current code splits them left/right (+spread vs −spread), which
   reads cleaner at 320×200 but isn't GIF-accurate. Pick one.
4. **The "half-over" variant** (owner idea, parked): a half-diamond serving the FAR carriageway by the
   trunk **bridging over** the highway and fanning down the other side — i.e. `near_pair` on the
   far side WITH an overpass. Useful as a standalone testable piece and a trumpet building block.
   Open Q: draw the trunk continuing across as an overpass, or just the ramps reaching over?
5. **Cloverleaf** — rebuild its 4 inline loop circles on `loop_ramp`/`draw_loop` (currently tangled).
6. **Double trumpet** (future) — two trumpets mirrored = a full **Cross + HW×HW** 4-way; a real
   candidate for that matrix cell alongside stack/cloverleaf. Reference image saved (see below).
7. Eventually: bake slider values → constants and **port the drawer into roadnet2** (the interface:
   *given two crossing roads + a topology → draw the junction*).

## Reference images (the trumpet target)
- **<https://www.roads.org.uk/interchanges> — the gold reference** (clean colour-coded movement
  diagrams of every junction type; British/drive-on-left → mirror for ours). See the ⭐ section up top.
- [`refs/trumpet.gif`](refs/trumpet.gif) — single trumpet. Trunk fans into the near carriageway
  (= the half-diamond), loop + semi-direct serve the far carriageway.
- [`refs/double-trumpet.png`](refs/double-trumpet.png) — two trumpets back-to-back = HW×HW 4-way.

## How to run / bake / inspect
- **Run:** open `interchange` in the editor → ▶. Click any matrix button to switch junction (cross /
  diamond / clover• / stack• · tee / half / trumpet · fork / wye). Drag the 5 sliders. `near/loop/fly`
  isolate atoms. **F** flip 3rd leg · **◄►** angle · **L** lanes · **P** / **UI** button hide panel.
- **Bake a specific state (TEMP dance):** the cart opens on CROSS/diamond. To bake another state
  (e.g. the trumpet), temporarily set the `static` defaults (`itype`, `topo`, `clsB`, `flip`,
  `show_panel`, `show_near/loop/fly`), bake, then **revert before committing** (`grep -n TEMP
  tools/carts/interchange.c` must be empty; the committed thumbnail = the normal open state).
- **Build + bake screenshot:**
  ```bash
  node tools/make-cart.js tools/carts/interchange.c editor/public/carts/interchange.cart.png
  node tools/make-cart.js --run editor/public/carts/interchange.cart.png   # bakes the thumbnail
  ```
  The screenshot bake compiles the whole engine (incl. `runtime/sound.h`), so a parallel agent's
  mid-edit breaks it — **retry `--run` in a loop until it prints "updated"**, or syntax-check the
  cart alone (it never includes `sound.h`):
  ```bash
  clang -fsyntax-only -I runtime -DSCREEN_W=320 -DSCREEN_H=200 -DSCALE=4 -DCELL_W=16 -DCELL_H=16 \
    -DMAP_W=64 -DMAP_H=64 tools/carts/interchange.c
  ```
- **To verify a bake, read `build/.bake/interchange/screenshot.png`** — NOT `build/screenshot.png`
  (that one belongs to the running editor and drifts).

## Key facts that bit / are easy to miss
- `polyfill` renders an empty fill for one screen-long quad — long straights are subdivided into
  short segments (`straight()` in the cart). Roads are subdivided `polyfill` ribbons.
- Two `BW` in `draw()`: the outer `int BW = rwidth(clsB)` (road B half-width) is shadowed inside the
  panel by `const int MBW` (matrix button width) — renamed to avoid the trap.
- The matrix is data-driven (`JUNCS[]`): every supported junction is one button; a click sets the
  whole config (topology + both classes + ramp-style). `built=0` → red dot + "DRAFT" in the tooltip.
