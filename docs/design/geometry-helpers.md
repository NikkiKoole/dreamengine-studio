# dreamengine — 2D geometry helpers (proposal)

Status: **open / proposed** · Date: 2026-06-01 · None merged.
Refines: [decision 0006](../decisions/0006-library-carts-not-engine.md) (small sharp core),
[decision 0009](../decisions/0009-small-3d-leaf-helpers.md) (leaf-primitives, not engines).
Related: [`rasterization-consistency.md`](rasterization-consistency.md) (fills/outlines must agree),
api-notes.md → "Cart-pattern analysis".

This note answers one question: **what small drawing primitives would most lower the cost
of building art out of code?** It's evidence-led — every proposal is tied to a pattern the
existing carts re-derive by hand (grepped across all 176 carts, 2026-06-01).

---

## Why: geometry-first is the identity, not a gap

A survey of the corpus made this concrete:

- **64 / 176 carts draw with triangles; only 31 touch `spr()`.** More than twice as many
  games build their art from `trifill`/`oval`/`circ` than from pixel sprites.
- **80 / 176 carts** contain a `cos_deg`/`sin_deg` loop feeding `trifill`/`line` — i.e. they
  hand-roll polygons, gears, gems, sparkles, asteroids, hex tiles.
- The welcome scene (`zoo`) draws every animal from ovals + triangles. That's the house
  dialect, not a one-off.

This is the product's edge, and it's worth doubling down on rather than papering over with a
sprite/asset pipeline:

- **No asset pipeline.** Every cart is self-contained, readable C. It diffs cleanly, it's
  tiny, and you can *read why a shape looks the way it does* — the teaching goal.
- **Resolution- and palette-coherent.** Shapes scale and recolor (`pal`) for free.
- **It's the PICO-8 / DIV lineage, but more so.** Sprites still matter for detailed
  characters; the data just says the corpus leans procedural, so the leverage is in making
  the procedural path cheaper.

So the design rule for everything below: **leaf primitives over the existing
`trifill`/`line`/`circfill`, immediate-mode, palette indices (0–31), angles in degrees** —
exactly the flavor of `quadfill` (decision 0009). None of these duplicate today's API
(there is no poly / ngon / gradient / rounded-rect / thick-line / bezier in `studio.h`).

---

## The proposals, ranked by how much boilerplate they kill

### 1. Regular polygons & stars — the single biggest win
**Evidence:** the 80-cart `cos_deg`/`sin_deg` + fill loop is the most-repeated drawing idiom
after the 3D depth-sort. Gems, gears, asteroids, badges, hex grids, and *every* sparkle
effect re-derive it.

```c
void ngon(int x, int y, int r, int sides, float rot, int color);       // n-sided regular polygon outline
void ngonfill(int x, int y, int r, int sides, float rot, int color);   // …filled (triangle fan)
void star(int x, int y, int r_out, int r_in, int points, float rot, int color);      // star outline
void starfill(int x, int y, int r_out, int r_in, int points, float rot, int color);  // …filled
```
`ngonfill(x,y,r,6,0,c)` = hexagon; `ngonfill(...,3,...)` = a centered triangle; `star(...,5,...)`
= the sparkle juice effects want. Implementation: a `trifill` fan from the center, points
placed with `dx`/`dy`. `rot` in degrees. This is the most "geometry-first" of the set.

### 2. Polygon from a point list — completes `tri → quad → poly`
**Evidence:** 9 carts keep explicit `px[]/py[]` vertex arrays and decompose them into
`trifill`s by hand. `trifill` and `quadfill` already exist; the n-gon decomposition is the
obvious next rung.

```c
void poly(int *xy, int n, int color);       // outline through n (x,y) pairs, auto-closed
void polyfill(int *xy, int n, int color);   // filled CONVEX polygon (triangle fan)
```
`xy` is `{x0,y0, x1,y1, …}`. Convex-only via fan keeps it a true leaf (no ear-clipping). A
concave fill would be a bigger algorithm — punt it to a library cart if ever wanted.

### 3. Gradient fill — every sky and backdrop
**Evidence:** carts stack huge runs of `rectfill` to fake gradients — **neonrain 72, zak 48,
larry 44, monkey 33, tradewinds 29** rectfills — plus every sky (`flyover`, `zoo`).

```c
void vgradient(int x, int y, int w, int h, int c_top, int c_bot);    // vertical
void hgradient(int x, int y, int w, int h, int c_left, int c_right); // horizontal
```
**Open design question (settle before building).** The palette is a fixed 32 colors, so a
gradient between two indices can't be smooth RGB. Two honest options:
- **Stepped:** walk N equal bands through chosen palette entries. Easy, but it's basically
  the loop the carts already write — modest value.
- **Dithered:** `fillp`-blend the two endpoint colors with the blend ratio shifting across
  the span (the checker trick `solid3d` uses for its ground). Genuinely hard to hand-roll,
  high value, and on-brand for the dither aesthetic. **Recommended**, possibly as the only
  form. (A `colors[]`-array variant is the third option but leans toward "the loop again".)

### 4. Rounded rectangle — UI panels & speech bubbles
**Evidence:** dialog/HUD boxes are assembled from `rect` + corner `circfill` across many
carts (panels, score boxes, menus, bubbles).

```c
void rrect(int x, int y, int w, int h, int r, int color);      // rounded outline
void rrectfill(int x, int y, int w, int h, int r, int color);  // rounded filled
```
Implementation: center cross of rects + four corner `circfill`s (filled), or four `arc`s +
edges (outline). Watch the edge-consistency rule in `rasterization-consistency.md` so the
corners meet the straight edges without seams.

### 5. Thick line — the `pen_size` callback
**Evidence:** ~15 carts (alleycat, columns, geometrydash, katamari, neonrain, puyo,
rivercity, rollswarm, …) emulate width by drawing offset/parallel lines.

```c
void line_thick(int x1, int y1, int x2, int y2, int w, int color);  // line with pixel width
```
Note: this is the *one* genuinely useful thing the removed turtle API had (`pen_size`,
[decision 0008](../decisions/0008-cut-turtle-graphics-api.md)) — worth keeping as a plain
primitive even though the turtle is gone. Implementation: a `quadfill`/`trifill` of the line
expanded perpendicular by `w/2`, or round caps via `circfill` at the ends.

### 6. (lower priority) Bezier curve
**Evidence:** only 4 carts (larry, outrun, podracer, racer — roads & curves).

```c
void bezier(int x0,int y0, int x1,int y1, int x2,int y2, int segs, int color); // quadratic, segs line segments
```
Cheap (a loop of `line` over `lerp`-of-`lerp`), but niche. Build only if a curve-heavy cart
asks for it.

---

## Recommended first batch

**#1 (ngon/star) + #2 (poly/polyfill)** — highest leverage, most "geometry-first", cut the
most code, and compose cleanly on `trifill` exactly like `quadfill`. Ship them as one batch
with a demo/tutorial cart (a "shapes from code" gallery: rotating gears, a hex grid, a
sparkle burst, a polygon blob) and a baked thumbnail, per the cart-authoring workflow.

**#3 (gradient)** next, once the stepped-vs-dithered question is settled (recommend
dithered).

## What to leave out (for now)
- **Concave/arbitrary polygon fill** — needs ear-clipping/scanline; a library cart, not a
  leaf primitive. Keep `polyfill` convex.
- **Outline-color parameters on every fill** (`rectfill` with a border color, etc.) — bloats
  signatures; the `fill` then `outline` two-call pattern is fine and readable.
- **Antialiasing / sub-pixel edges** — against the crisp retro look; and it fights the
  palette-index model.
- **Bezier beyond quadratic / splines** — until a cart needs it.

## Wiring checklist (when any of these land)
Each new call must hit all four places (see root `CLAUDE.md` → "Adding a new API function"):
`runtime/studio.h` (decl + one-liner) → `runtime/studio.c` (impl on existing primitives) →
`editor/src/studioDocs.js` (bilingual `sig`/`doc`/`docNL`) → `editor/src/shell.js` (help-tab
section — likely a new **"shapes"** group, or fold into the existing graphics group). Then
ship a demo cart with a baked thumbnail, and update [`STATUS.md`](../STATUS.md).
