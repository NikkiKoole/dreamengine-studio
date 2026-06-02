# Rasterization consistency — one clean way to fill/outline a shape

> **Genre: design exploration → mostly SHIPPED.** This captured a recurring class of
> pixel bug and the goal: *one* consistent way to rasterize a shape so its fill, outline,
> dither, solid and adjacent neighbours all agree pixel-for-pixel. **That goal is now
> reached** for every filled+outlined primitive — we chose and applied **direction 1**
> (per-pixel CPU coverage). What remains is verification, not design: `thickline` full
> automation, a perf measurement, a web-GL-ES confirmation, and folding the GPU→CPU
> `tri`/`trifill` change into a `decisions/` ADR. See "What's left" at the bottom.
> - State of individual fixes → [`../STATUS.md`](../STATUS.md).
> - Per-pixel precedent for baked shapes → [`baked-rotation-atlas.md`](baked-rotation-atlas.md).
> - `pal()` sprite recolor (a related "one definition" win) → [`../decisions/0007-pal-recolors-sprites.md`](../decisions/0007-pal-recolors-sprites.md).

## The symptom (it kept coming back) — all three now resolved

Across several carts the same kind of artifact showed up at shape **edges**:

- **arcs cart** — `arcfill`/`ring` (old triangle-fan) left 1px **seam cracks** between
  fan triangles, and the rim **overshot** so a stroked `arc`/`circ` outline didn't line up.
  ✅ *Resolved* (session 9, per-pixel `sector_fill`; still consistent — its outer test
  `d²≤r_out²` from the pixel centre *is* `disc_inside`. Now guarded in `raster_test` page 3.)
- **solid3d** — dithered faces showed **black hairlines** between adjacent triangles.
  ✅ *Resolved* (session 14 — `trifill` on `poly_inside`: each pixel centre falls on exactly
  one side of a shared edge, so neighbouring faces are watertight — no crack, no overlap.
  Confirmed in-cart: the open lines are gone.)
- **katamari** — a dithered circle's **outline didn't hug the fill** (the `circ` ring
  traced a different circle than the dithered `circfill`), and a "draw a smaller dither
  circle on top of a solid one" attempt left **rim artifacts** because the two circles
  had different edges.
  ✅ *Resolved* (session 11 — `circ`/`circfill` share `disc_inside`; outline = boundary ring.)

Different carts, different primitives — **same root cause**, now fixed once at the root.

## The root cause: the engine has *several* definitions of the same shape

A "circle of radius r centred at (x,y)" is currently rasterized **three different ways**,
and they do not produce the same set of pixels:

| Definition | Used by | Edge rule |
|---|---|---|
| **raylib GPU** | `circfill` (solid → `DrawCircle`), `circ` (`DrawCircleLines`), solid `trifill` (`DrawTriangle`) | GPU coverage (pixel-center, top-left fill rule) |
| **CPU scanline `*_pat`** | any fill while `fillp` is on — `ovalfill_pat`, `rectfill_pat`, `trifill_pat` | per-row span, integer rounding |
| **per-pixel** | `arcfill`/`ring`/`arc` (the new ones) — `sector_fill` | pixel-center distance test `(x+0.5-cx)²+… ≤ r²` |

When two of these meet at an edge they disagree by up to a pixel:
- **fill vs outline:** `circ` (GPU lines) over a dithered `circfill` (CPU scanline) → the
  ring sits 1px off the fill.
- **solid vs dither:** a GPU `trifill` next to a `trifill_pat` dither face → the CPU side
  truncates short of the shared edge → background shows (black crack).
- **fill vs fill:** two scanline triangles meeting on a diagonal → rounding gap.

So every "edge artifact" is really *"which rasterizer drew this pixel, and did the
neighbour use the same one?"*

## Point-fixes shipped so far (each forced agreement locally)

These all work, but each is a *local* patch, not the single rule:

1. **`arcfill`/`ring` → exact per-pixel**, measured from **pixel centres** (`x+0.5-cx`) so
   the disc matches the `circ`/`circfill` convention; **`arc` is a 1px ring of that same
   circle**, so fill + outline come from one definition. *(commit: arc rasterization fix.)*
2. **`trifill_pat` conservative span** — floor the left end, ceil the right, so adjacent
   dithered triangles **overlap by ≤1px instead of gapping**. Fixes solid3d's black lines.
   Only the `fillp` CPU path; solid GPU `trifill` untouched.
3. **katamari** — (a) one **single-shape** dithered fill (base colour on the 0-bits, a
   darker shade of itself on the 1-bits) instead of two stacked circles; (b) the "too big"
   marker is a slightly larger **solid backing** drawn *behind* the fill, so a solid `s+1`
   shape always contains the `s` fill (any rasterizer) and leaves a clean 1px ring.

Pattern across all three: **make the edge come from a single definition** — per-pixel,
or conservative-overlap, or "one shape, not two."

## The goal — reached (via direction 1)

> **One rasterization path that fill, outline, dither and solid all share**, so that:
> - an outline always exactly traces its fill (`circ` hugs `circfill`, for any radius) — ✅
> - adjacent shapes never crack and never overlap-bleed — ✅ (watertight; solid3d confirms)
> - a dithered fill never specks past where the solid fill would be — ✅ (same `plot_pat` path)
> - it behaves identically on desktop GL and web GL ES — **likely ✅ but unverified** (see below)

We picked **direction 1 (per-pixel CPU coverage)** and applied it everywhere: one
`*_inside(x,y,…)` predicate per shape (`disc_inside`, `ellipse_inside`, `rrect_inside`,
even-odd `poly_inside`, and `sector_fill`'s distance test), measured from the pixel centre;
**solid fill, dithered fill, and outline (the boundary ring) all read from it.** No primitive
goes through raylib GPU coverage anymore (`DrawTriangle`/`DrawCircle`/`DrawLine` outlines all
gone). The whole `fillp` `*_pat` scanline family is collapsed into one `plot_pat` per pixel.

### Why not the other directions
- **Direction 2 (keep GPU as truth, match the CPU paths to it):** rejected — "match raylib's
  exact coverage" is brittle and differs desktop GL vs web GL ES, the very thing we wanted to
  kill. Direction 1 sidesteps it: CPU per-pixel is identical on both backends.
- **Direction 3 (document a two-layer convention, push discipline onto cart authors):**
  rejected — direction 1 made it unnecessary; authors can now freely dither-then-outline.

### Sub-problems — how each was nailed
- **Pixel-centre vs corner** → one convention everywhere: `x+0.5`, `y+0.5`.
- **Outline = boundary of the fill** → every outline is "inside AND a 4-neighbour outside"
  (or `sector_fill`'s rim), derived from the same predicate — never a separate stroke.
- **Adjacency / shared edges** → watertight by pixel-centre ownership: a centre is on exactly
  one side of a shared edge, so neighbours never both-claim (overlap) nor both-drop (crack).
  Replaces the old `trifill_pat` conservative-overlap hack (deleted).
- **Thin shapes / silhouettes** → no conservative-overlap halo anymore; coverage is exact.
- **`fillp` lattice under `camera()`** → `plot_pat` keys the pattern off screen `x&3,y&3`;
  still worth an explicit scroll-shimmer check (listed in "What's left").

## Shipped (2026-06-01, session 11)

`circ`/`circfill` and `oval`/`ovalfill` unified on a single pixel-center coverage test:
`disc_inside(x,y,cx,cy,r)` = `(x+0.5-cx)²+(y+0.5-cy)² ≤ r²` and the ellipse equivalent.
Outline = boundary ring of the fill (pixels inside with ≥1 outside 4-neighbour) — never
a pixel outside the fill. `plot_pat` handles both solid and dithered fills in one loop,
so dither and solid are the same path.

**Regression test:** `tools/carts/raster_test.c` — SPACE to freeze + analyse, Z/X to
toggle outline/dither. Harness: `tools/raster_test.script`. All meaningful states
(solid+outline, dithered+outline) report 0 mismatches.

## Shipped (2026-06-01, session 12)

`rrect`/`rrectfill` unified on a single pixel-center coverage test, `rrect_inside(px,py,
x,y,w,h,r)` = within `r` of the inner rect of corner centres (straight edges → distance 0
→ inside; corners reduce to `disc_inside` against the nearest corner centre). `rrectfill`
plots every inside pixel via `plot_pat` (solid + dither one path); `rrect` is the boundary
ring (inside with ≥1 outside 4-neighbour). This **removes the stacked-shape anti-pattern**
the old `rrectfill` used (3 rectfills + 4 circfills) and the old `rrect`'s lines-meet-arcs
seam — fill, outline and dither now come from one definition. Extended `raster_test.c` to
cover 6 rrects (varied radii); all four states report 0 mismatches.

**Harness fix (same session):** the freeze/analyse rework (carts d8c2d18/777a784) had left
`tools/raster_test.script` stale — it never pressed SPACE, so analysis never ran and the
trace was empty despite the script comment saying "read mismatches from the trace." The
cart now `watch()`es `mismatches` + `state` under `#ifdef DE_TRACE`, and the script drives
SPACE → Z → X → Z through all four states; counts land on frames 3/7/11/15.

> **Detector sensitivity (measured):** the marker test flags a fill-edge with *no adjacent
> outline* (yellow) or an outline with *no adjacent fill* (pink). A **1px** outline/fill
> misalignment keeps everything 4-adjacent and so reads as 0 mismatches; a **≥2px** gap is
> caught (verified: a +2px outline offset → 657 mismatches). So "0 mismatches" means
> "agree to within 1px," not "bit-identical." Good enough for the seam-crack class of bug;
> note it if a sub-pixel guarantee is ever needed.

## Shipped (2026-06-01, session 14)

The whole polygon family unified on **one even-odd point-in-polygon coverage** —
`poly_inside(fx,fy,xy,n)` measured from the pixel centre, same convention as the
disc/rrect tests. `poly_fill_cov` plots every inside pixel (`plot_pat` → solid + dither
one path); `poly_stroke_cov` is the boundary ring (inside with ≥1 outside 4-neighbour).
Rewritten onto it: `ngon`/`ngonfill`, `star`/`starfill`, `poly`/`polyfill`, **and
`tri`/`trifill`** (a triangle is just a 3-vertex polygon). Wins:
- outline is exactly the boundary of the fill for every one of them — no more GPU
  line-vs-fill drift (the "triangles differ slightly" bug);
- even-odd handles **concave** `poly` and the star's reflex points (the old fan filled
  them wrong); winding-independent, so 3D back/front faces both fill;
- `trifill_pat` (the CPU dither-scanline special case) deleted — `plot_pat` covers dither.

`trifill` is now CPU per-pixel (the 3D carts — `solid3d`/`cube3d`/`flyover` via `quadfill`
— go through it). Smoke-tested: `solid3d` renders filled+dithered with no face cracks,
no hang. **Perf:** the CPU cost is real (podracer froze when its haze spammed ~190 large
software tris/frame; fixed cart-side by moving bulk fills to GPU). The off-screen-bbox
worry below is now **fixed in the engine** — `poly_fill_cov`/`poly_stroke_cov` clamp their
scan to the on-screen region (mapped through the camera), so huge off-screen tris cost
nothing extra. See the "off-screen bbox clamp" entry near the end.

**Detector rewritten (same session).** The old marker test was a local adjacency guess and
had two failure modes: it tolerated ≤1px drift, and once the pink rule was relaxed to stop
false-flagging sharp tips it went blind to connected offsets. Replaced with a **global
invariant**: reconstruct the fill region from the single render (`fill ∪ outline`) and
require the outline to be *exactly its boundary* — a FILL pixel that itself touches
background = an uncovered edge (yellow); an OUTLINE pixel with no background neighbour =
buried inside the fill (pink). This catches a 1px offset at any angle yet never flags a
correct tip (a tip pixel is on the boundary). Verified it now catches what it used to miss
(GPU triangles → 282 mismatches before conversion; 0 after).

> **Residual blind spot (known):** a single combined render still can't distinguish a
> uniformly-**1px-proud** outline (drawn just outside the fill but still touching it) from
> a correct boundary — both read as a boundary pixel with fill on one side, bg on the other.
> Closing that needs a **two-pass** test (render fill-only → set F, outline-only → set O,
> assert O == boundary(F)). Not needed today: every primitive is now coverage-based, so the
> outline *is* boundary(F) by construction and the bug class can't arise. The two-pass test
> is the upgrade if a future GPU/stroke primitive is reintroduced.

## Regression test coverage (`raster_test`, 4 pages, C cycles)

| Page | Primitives | Check |
|---|---|---|
| 1 | `rect`/`rectfill`, `circ`/`circfill`, `oval`/`ovalfill`, `rrect`/`rrectfill` | outline == fill boundary |
| 2 | `tri`/`trifill`, `quadfill`, `ngon`, `star`, `poly` | outline (or `poly` of corners) == fill boundary |
| 3 | `arcfill`/`arcoutline`, `ring`/`ringoutline`, `thickline`/`thicklineoutline`; `arcfill` vs `circ` | outline == fill boundary |
| 4 | `ring`, `arcfill` sectors, `thickline` — **equivalence self-test** | pixel-set vs independent reference |

Pages 1–3 (marker detector): each × {solid, dithered} × {outline on/off} → 11 analysed states,
all **0 mismatches**. The detector checks one global invariant: outline == boundary of the
fill region (`fill ∪ outline`); a fill pixel touching background = uncovered edge (yellow), an
outline pixel with no background neighbour = buried (pink). Catches a 1px offset at any angle,
never false-flags a sharp tip. Verified to have teeth (GPU triangles → 282 before conversion).

Page 4 (equivalence) covers the **open strokes** the marker detector can't score, by comparing
their pixel set to an independently-rasterized reference across consecutive frames (`pget`
reads the prior frame's snapshot):
- `ring(…,0,360)` == `circfill(ro)` minus `circfill(ri)` — `sector_fill` radial bound vs `disc_inside`.
- `arcfill(0,180)` ∪ `arcfill(180,360)` == `circfill` — the angular seam tiles with no crack/overlap.
- `thickline` has no enclosed background — the cap/body merge is solid.
All three report 0.

## Shipped (2026-06-01, session 15) — `thickline` + full coverage

`thickline` rewritten as a **single capsule coverage** (pixel centre within `w/2` of the
segment, clamped projection gives the round caps) instead of `quadfill` + two `circfill` caps.
The equivalence test **found a real bug** in the old version: a 1px enclosed-background pixel at
the quad/cap seam (at one endpoint), because the body half-width was `w*0.5` (5.5 for w=11) but
the caps used `circfill(…, w/2)` (integer 5) — different widths, so they didn't merge. One
coverage definition makes a seam impossible. With this, **every filled primitive is unified on
per-pixel coverage** and exercised in `raster_test` (4 pages).

## Shipped (2026-06-01, session 16) — outline primitives for the open strokes

The filled sector/ring/capsule had no matching *outline* primitive, so in the test they were
static visual-only shapes (a different colour the detector ignored) — inconsistent with every
other shape, which has a fill + an outline that hugs it. Added **`arcoutline`** (pie wedge:
rim + the two radial edges), **`ringoutline`** (annulus sector), and **`thicklineoutline`**
(capsule border) — each the boundary ring of the *same* coverage as its fill (`sector_inside`
/ `capsule_inside` refactored out and shared by fill + stroke). Now `arcfill`/`ring`/`thickline`
on page 3 draw fill + boundary-ring outline + dither and are graded by the marker detector like
everything else (0 mismatches, partial sectors included). `arc` stays the curved-rim-only stroke
(distinct from `arcoutline`'s closed pie). Fully wired (studio.h/.c, studioDocs, shell help tab).

## What's left

- **Perf — measured, the cost is real (2026-06-01, `podracer`).** Direction 1 is
  CPU-per-pixel: `trifill` → `poly_fill_cov` scans the triangle's bounding box and does a
  point-in-polygon test + `pset`/`DrawPixel` for *every* interior pixel. Fine for a handful
  of shapes; **pathological when a cart spams large trifills**. `podracer` (the pseudo-3D
  textured canyon racer) **froze to a crawl** because its distance haze dithered the far
  third with **6 big trifills per far segment** — ~190 large software-filled triangles a
  frame, covering the full-height canyon walls, heavy overdraw. The textured geometry was
  never the problem: `tritex` is one GPU quad each, and `rectfill`/`line` are GPU too. Only
  the software `trifill` overdraw stalled it.
  - **The cart-level fix** (no engine change — `trifill` stays exactly as consistent as this
    doc requires): move bulk fills *off* `trifill` onto GPU primitives. The per-segment haze
    became **two dithered `rectfill` bands** near the horizon (`fillp` + `rectfill` = one
    textured quad each, holes transparent so the canyon shows through the dither), and the
    per-segment lane-guide edge strips became `line()`s. Same look, ~330 software triangles
    a frame → ~3 GPU draws + ~140 GPU lines. Rendering verified still correct via the
    headless dump harness.
  - **Rule of thumb for cart authors:** `trifill`/`quadfill`/`ngon`/`poly` are *software*
    per-pixel — great for correctness, cheap only at low count/area. For large areas or
    high counts (per-segment, per-scanline, full-screen overlays) reach for the GPU paths:
    `rectfill` (incl. dithered via `fillp`), `line`, `tritex`.
  - **Engine mitigation — SHIPPED (2026-06-02).** `poly_fill_cov`/`poly_stroke_cov` clamp
    their scan box to the on-screen region before scanning. The clamp is in the primitive's
    own coordinate space, which the camera shifts, so it maps the four screen corners back
    through the `Camera2D` (`GetScreenToWorld2D`) and takes their AABB — a conservative
    superset under rotation, so translate/zoom/rotation are all handled and no visible cell
    is ever dropped. Huge off-screen tris no longer iterate their off-screen bbox. Measured
    on the `trifill_stress` cart (12 thin spokes, ~1100px off-screen reach): **46.7ms →
    2.7ms/frame (~17×), ~21fps → 60fps**, with `raster_test` still `mismatches:"0"` on all
    analyse frames (byte-identical output). This is the win the "clamp the fill bbox" note
    earlier in the doc anticipated.
- **Web GL ES.** All-CPU coverage *should* render bit-identically on desktop GL and web GL ES
  (only the final scale-up is GPU). Confirm by running `raster_test` in the emscripten build —
  but note `pget` is disabled on web, so the in-cart detector won't run there; confirmation is
  visual (or move the detector behind a native-only build).
- **ADR.** `tri`/`trifill` changed observable behaviour (GPU→CPU, now winding-independent and
  pixel-exact), and `thickline` changed shape (capsule vs quad+caps). Per this doc's own rule,
  that earns a `decisions/` entry.

Once those are closed, this stops being a design doc and becomes a settled decision.
