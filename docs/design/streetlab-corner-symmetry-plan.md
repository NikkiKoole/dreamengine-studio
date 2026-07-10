# streetlab — symmetric-corner mirror-blit fix (plan of attack)

> **Status: BUILT + verified (2026-06-23).** The mirror-blit shipped in `streetlab.c`
> (`mirror_blit()` + its call after the corner loop in `draw()`), guarded to the symmetric
> orthogonal 4-way (`!isT && skew==0 && !freeRight`). Verified: `node tools/mirror-diff.js
> streetlab --band 20,110` shows **0 kerb (`BROWNISH_BLACK`) mismatches** (was 7); the
> remaining ~108 are lane-dash phase, legitimately asymmetric. The route there is below —
> note the refined diagnosis and the `sline` negative result in the UPDATE section, which is
> why the blit (not a symmetric line) is the fix. Verifier: `tools/mirror-diff.js`; petri-dish
> demos: `arcsym` (blit principle), `linesym`/`axissym` (primitive symmetry).
> **Follow-on (cosmetic, 2026-06-24):** mirroring made the four corners identical, which surfaced a
> *pre-existing* ~2px dark notch — the straight arms get a 1px-proud `BROWNISH_BLACK` casing (asphalt
> pass insets it) but the corner did not, so the two straight casings poked past the HW-tangent kerb.
> Fixed by adding a **casing fillet** (a `fill_corner` at `HW+1` under the `HW` asphalt fillet — the
> casing IS the kerb, like the straight arms). **Gated to ~90° corners only:** the fillet's ring
> width is ~`1/sin(half-angle)`, so on SKEW corners it fattens to 2–3px — so orthogonal gets the
> casing fillet (notch gone), and skew keeps the uniform-1px `stroke_corner` it always used (it never
> had the notch). Clean 1px kerb at every angle; symmetry stays 0; `spec` 104/0.
> **Discoverability:** STATUS.md §43 (updated → RESOLVED), `docs/README.md` layout pointer,
> and the cross-links in [`rasterization-consistency.md`](rasterization-consistency.md) (the
> `line()`-is-the-GL-holdout correction) and [`software-canvas.md`](software-canvas.md) (`sline`
> as the canvas's CPU-line + the determinism/perf coupling). Skew/T/free-right still accept the
> floor (no symmetry expectation). **API candidates (not yet promoted via the 4-place process):**
> `sline` (reflection-symmetric CPU `line()`) and `mirror_blit` (reusable symmetric-shape helper).
>
> Related: `tools/carts/streetlab.c` (the `SEAM` note by `ri()` + `mirror_blit`),
> [`../STATUS.md`](../STATUS.md) §43 (the visual test-coverage blind spot, now RESOLVED),
> [`rasterization-consistency.md`](rasterization-consistency.md) (`line()` is the GL holdout) and
> [`software-canvas.md`](software-canvas.md) (`sline` = the canvas's CPU line), and the `arcsym`
> cart (the mechanism in isolation).

---

## UPDATE 2026-06-23 — refined diagnosis (the fill was never broken) + `sline` negative result

A deeper investigation (probe carts `linesym` + `axissym`, and a colour-classify pass over the
mirror-diff frame — `band 20,110` mismatches grouped by pixel colour) sharpened the whole picture.
**Read this before the original plan below; it changes the recommendation.**

**1. The corner FILL is already mirror-symmetric. The entire floor is the kerb STROKE.**
Classifying streetlab's 131 band-mismatches by colour: **124 are lane dashes** (white/yellow vs
asphalt — dash *phase*, legitimately asymmetric) and the real kerb floor is **7 pixels, every one
involving `CLR_BROWNISH_BLACK`** — the kerb line. There are **zero** asphalt/grass mismatches: the
fill (`fill_corner → polyfill`) reflects exactly. Why: `polyfill` decides its pixels on the **CPU**
(float edge-crossings → integer span via `poly_fill_cov`), then GL only *fills* the chosen integer
span. `line()`, by contrast, is raylib **GPU `DrawLine`** — GL *chooses* the staircase, and that
choice is direction-dependent, so it isn't reflection-symmetric. (Engine audit: `line()` — and
`bezier`/2-pt `poly` that call it — is the only **axis-aligned** cart-facing primitive that lets GL
pick pixels; the rest of that surface is the rotation/texture family (`rectfill_rot`, `spr_rot`/
`sspr_ex`, `tritex`, + rotating `camera_ex`) — see `software-canvas.md`'s audit — but those are
rotation/sampling, not the axis-aligned kerb case. Every
other fill/circle/oval/ring/arc/thickline is CPU-decided → symmetric-capable.)

**2. A symmetric software line does NOT fix the kerb — it REGRESSES it 7 → 27 (measured).**
We built `sline` (a CPU per-axis DDA, reflection-symmetric — see §"`sline`" below) and wired it into
`stroke_corner`. The kerb floor got *worse*. Root cause is an **even-grid snapping conflict**:
- mirror-diff pairs pixel **cells** `x ↔ 319-x` (sum **319**; centres average to the true centre 160).
- A 1px stroke *is* a cell, so to be symmetric its two endpoints must be **cell-mirrors** (sum 319).
- But the arc verts are points symmetric about **160.0**, and `ri()` (round) snaps them to cells
  summing to **320** (`ri(172)=172`, `ri(148)=148`). `polyfill` is happy with that — it's point/area
  based and reflects about 160. `sline` is exact only for *cell*-mirror endpoints, so fed the fill's
  *point*-mirror endpoints it lands 1px off on every segment.

This `ri()` snap boundary has a second, rarer effect too: **double-rounding** (`ri(mirror(v)) ≠
mirror(ri(v))` — the original `arcsym` mechanism), which jitters ≤1px only when an arc sample lands
exactly on `k+0.5`. It's a minor contributor next to the systematic cell-vs-point offset, and the
same root. Both are why no rasteriser fix suffices and why making `ri()` itself symmetric is a dead
end: a stroke wants `floor`-style snapping (cell-mirror, sum 319), the fill wants `round`/point-mirror
(sum 320) — the shared arc verts can't satisfy both. All three fixes above sidestep snapping entirely.

**Fills and 1px strokes want opposite snap conventions on an even grid.** That is the wall, and it's
exactly why the mirror-blit (Option A below) was always the clean local fix: it copies *rendered
pixels*, sidestepping the snap question entirely. `sline` is the correct *general* symmetric line
(proven **0-mismatch** in `linesym`/`axissym` whenever endpoints are true cell-mirrors, about **any**
axis — symmetry is segment-local, not centre-special), but **"symmetric line" ≠ "symmetric kerb."**

**Revised recommendation.** For streetlab specifically, the local fix is still **Option A
(mirror-blit)** — now better justified (it's the only one that dodges the snap conflict). Two other
real options: **(b)** draw the kerb as the fill's *coverage-boundary* (so the stroke inherits the
fill's symmetry by construction, like `poly_stroke_cov`); **(c)** the **software canvas**
([`software-canvas.md`](software-canvas.md)), where `line()` becomes a CPU rasteriser engine-wide
with one convention — `sline` is exactly the CPU-line that doc's architecture sketch assumes but the
engine lacks. `sline` is banked in `streetlab.c` (marked `__attribute__((unused))`) for that day.

### `sline` — the reflection-symmetric software line (recipe, proven)

Iterate the **major** axis per integer step, round the **minor** coordinate; break exact `.5` ties
**toward the segment midpoint** (reflection-invariant — under `c↦K-c` value, midpoint and comparison
all flip together, so it works about any axis and any grid parity); at the lone degenerate
(`tie == midpoint`) **plot both** candidate pixels (≤1px nub at the dead-centre of half-integer-
centred segments). No direction-dependent error term ⇒ a line and its mirror rasterise identically.
Progression measured in `linesym` (line-only mismatch): GPU `DrawLine` 88 → round-half-even 30 →
toward-midpoint 2 → +plot-both **0**. `axissym` confirms 0 about axes 64/100/137/200.

### Banked next — the field-based road refactor (proven in `skewlab`, NOT done here)

> **Full method + contract + port plan: [`field-based-road-rendering.md`](field-based-road-rendering.md).**
> Short version below; that doc is the home for the refactor whenever it's picked up.


The deeper fix for the *skew/curve* edge issues (missing outline, stray pixels at the crossing,
doubled edges — all pre-existing, byte-identical to before this work) is to stop drawing road edges
**per-arm** (casing bands) and draw the **union outline** instead: fill the whole road union, then a
coverage border = "an asphalt pixel with a grass neighbour" (the engine's own `circ`/`poly` rule).
The `skewlab` cart proves this is generator-agnostic — clean 1px (or N-px, uniform) kerb at any
skew, and at any **curve/Perlin** path — and that the same nested-coverage idea gives a full
cross-section (asphalt → kerb → **sidewalk** → grass) cleanly at any angle. That refactor would
*delete* the per-arm casing + `mirror_blit` + casing-fillet special-cases (the union fill is already
symmetric; the outline is uniform at any angle), so it's a net removal of ad-hoc code. **Deliberately
deferred** — it's a core rewrite of streetlab's road drawing that interacts with sidewalks/bike/
roundabout/free-right, so it wants its own focused run. Basis cart: `skewlab` (registered).

---

## The bug (confirmed)

On the default 4-way intersection (`cx=160.0`, an **even-width** boundary), the four kerb
fillets are mirror-symmetric in *real space* yet the **rendered kerb edges land ≤1px apart**.
Measured: `node tools/mirror-diff.js streetlab --band 20,110` → **131 mismatched mirror-pairs**;
the overlay shows the central octagon's left kerb edges lighting up (the rest is directional
markings + dashed-line phase, which *should* differ).

**Cause.** The corner loop at `streetlab.c:766–779` draws each corner independently:

```
edge_corner → curb_return → fill_corner (polyfill) / stroke_corner (line) / corner_bike
```

Each arc vertex is `ri()`-snapped to the grid and *then* `polyfill`/`line`-rasterised. As
`arcsym` proves, re-rasterising mirrored geometry **double-rounds** — `ri(mirror(v)) ≠
mirror(ri(v))` at half-pixel phases — so the edge drifts.

**Why the cheap fixes don't work** (both ruled out by the `arcsym` probes):
- Tweaking the rounding can't make every orientation agree — a flip about a pixel boundary
  maps pixel-centres to pixel-centres, so the residual is intrinsic to independent rasterisation.
- Even reflecting the *already-snapped integer vertices* and re-filling fails: the engine's
  `polyfill`/`line` **fill rule itself is not mirror-symmetric** (a half-open scan interval +
  per-scanline crossing rounding), so identical mirrored vertices still fill to off-by-one
  pixels (the probe showed a uniform 1px edge shift).

The only exact fix is to **reflect the rendered pixels**, exactly as the docs predicted:
*"compute one corner, blit it rotated/mirrored for non-skew junctions."*

## Scope / guard

Apply the fix only where the four corners *should* be pixel-mirror-images — the D4-symmetric
case:

```c
!roundabout && !isT && skew == 0      // 4 arms at 0/90/180/270, uniform HW & cornerR
```

`cornerR` and `HW` are single globals (`streetlab.c:173`, `cross_hw()`), so uniformity is
automatic. **Skew, T-junctions, and the roundabout keep the current per-corner path** — there
is no symmetry expectation there, so the accepted floor simply stays for those (note it in the
docs rather than chasing it). Marking toggles (turn lanes / median / bike / parking) are fine to
leave on: the kerb arcs stay 4-fold symmetric and the markings are drawn *after* the corners.

## The fix — "render one quadrant, mirror-blit the other three"

Insert **after** the corner loop and **before** the per-arm markings loop (`streetlab.c:783`).
This ordering is load-bearing: arrows / stop bars / turn arrows are directional (drive-on-right)
and must **not** be mirrored.

### Option A — `pget`/`pset` capture-and-mirror (recommended first)

~30–40 lines, no rasteriser duplication.

1. `enable_pget(true)` once in `init()` (the cart never enables read-back today).
2. Define a central box around `(cx,cy)`, radius ≈ `HW + cornerR + (peds?SW:0) + 2`.
3. For the three non-canonical quadrants, read the canonical quadrant's pixels with `pget` and
   write them mirrored with `pset`:
   - left/right: `x' = 2*round(cx) - 1 - x`
   - up/down:    `y' = 2*round(cy) - 1 - y`
   (the even-width bijection — no fixed pixel; the same reflection `mirror-diff.js` uses).

**Trade-off:** `pget` reads the *previous* frame, so there's a one-frame transient right after a
parameter change; in steady state the draw is deterministic, so the read is exact. Negligible for
a sandbox. The box is ~50×50, so the 3× pget/pset cost is trivial.

### Option B — software cell-buffer (the lag-free upgrade)

Rasterise one corner into an index buffer (reuse `arcsym`'s `fillpoly` + `bres`), then `pset`-blit
it ×4. Deterministic, no `pget` lag, and spec-able — but it reimplements the fill/stroke/bike
compositing. Do this only if A's one-frame lag or perf ever bites.

**Recommendation:** ship **A** (lowest risk, literally the docs' recipe); keep **B** documented as
the upgrade path.

## Verification — the golden-pixel-diff gate

`tools/mirror-diff.js` is the harness (promoted from the `pngdiff.js` investigation prototype). It
renders a cart headless, reflects the framebuffer about `cx`/`cy`, and counts mismatched
mirror-pairs — the framebuffer twin of `tune-check`/`level-check`, and the visual-regression gate
[`../STATUS.md`](../STATUS.md) §43 said was missing.

```bash
node tools/mirror-diff.js streetlab --band 20,110           # measure (now: 131)
node tools/mirror-diff.js streetlab --band 20,110 --overlay /tmp/x.png   # see WHERE
node tools/mirror-diff.js streetlab --xband 130,190 --band 40,70 --quiet # gate just the kerb band
```

- **Before:** ~131 mismatched pairs over the junction band (most is directional markings + dash
  phase; the kerb-band-restricted count is the real target).
- **After the fix:** 0 in the kerb band for the symmetric case.

To gate cleanly, restrict to a column/row band that excludes the dashed lane lines (or render with
markings off), so dash phase doesn't create false positives. `--quiet` exits 1 on any mismatch (CI).

## Build sequence (when streetlab frees up)

1. Add the guard predicate + `enable_pget(true)` + the central-box mirror-blit (Option A) in the
   non-roundabout branch, after `streetlab.c:779`.
2. Pin the gate: pick the kerb-band `--xband`/`--band`, confirm 131→0 for the symmetric default,
   and confirm skew/T are untouched (still use the per-corner path). Wire it into a check (or note
   the invocation in [`debug-harness.md`](../guides/debug-harness.md)).
3. Update the three "accepted floor" mentions — [`road-program-state.md`](road-program-state.md) "Accepted floor",
   `streetlab.c` `SEAM` note, `../STATUS.md` §43 — from *accepted* to *fixed for symmetric
   junctions via mirror-blit; skew/T still accept the floor*.

## Gotchas

- **Draw order**: blit after sidewalk/asphalt/corners, before markings. The earlier passes are
  already symmetric, so over-stamping them is a safe no-op.
- The 1px seam at the fixed centre column/row sits under the central asphalt overlap — hidden.
- `corner_bike` (the terracotta corner wrap) is inside the box → captured/mirrored for free.
- Keep the box clear of the toolbar/HUD; the junction is centred, the box is small, so it's fine.
