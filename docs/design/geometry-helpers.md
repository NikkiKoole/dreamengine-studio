# dreamengine — 2D geometry helpers (proposal)

Status: **open / proposed** · Date: 2026-06-01 · None merged.
Refines: [decision 0006](../decisions/0006-library-carts-not-engine.md) (small sharp core),
[decision 0009](../decisions/0009-small-3d-leaf-helpers.md) (leaf-primitives, not engines).
Related: [`rasterization-consistency.md`](rasterization-consistency.md) (fills/outlines must agree),
[api-notes.md](api-notes.md) → "Cart-pattern analysis".

This note answers one question: **what small drawing primitives would most lower the cost
of building art out of code?** It's evidence-led — every proposal is tied to a pattern the
existing carts re-derive by hand (grepped across all 176 carts, 2026-06-01).

---

## Why: geometry-first is the identity, not a gap

A survey of the corpus made this concrete (counts over all **183** carts, 2026-06-01):

- **66 carts draw with triangles; only 31 touch `spr()`.** More than twice as many games
  build their art from `trifill`/`oval`/`circ` than from pixel sprites.
- **136 carts use circles**, **45 use ovals** — the "soft body" base (heads, eyes, wheels,
  shadows). The welcome scene (`zoo`) draws every animal from ovals + triangles; that's the
  house dialect, not a one-off.
- **80 carts** contain a `cos_deg`/`sin_deg` loop feeding `trifill`/`line` — i.e. they
  hand-roll polygons, gears, gems, sparkles, asteroids, hex tiles.
- **46 carts use `fillp`** to fake shades/textures, and dozens stack long runs of `rectfill`
  to fake gradients (see the taxonomy below).

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

## Graphics taxonomy — what the carts actually draw with

How the 183 carts get pixels on screen, by primary technique. Most carts mix several; this
is "what dominates the look." Use it to see which proposed helper pays off where.

| Technique | Carts | What it makes | Example carts |
|---|---:|---|---|
| **Circles / ovals — the soft-body base** | 136 circ / 45 oval | heads, eyes, wheels, balls, shadows, glows | `zoo`, `pool`, `rocketleague`, `marble`, nearly everything |
| **Triangles — the polygon workhorse** | 66 | beaks, fins, roofs, ships, sparkles, all 3D faces | `asteroids`, `solid3d`, `flyover`, `zoo` |
| **Trig + fill loops — hand-rolled radial polygons** | 80 | gears, gems, asteroids, badges, hex tiles, radial bursts | `asteroids`, `bejeweled`, `geometrydash`, `katamari` → **the `ngon`/`star` case** |
| **Dither / `fillp` — fake shades & textures** | 46 | PS1 face shading, brick/grid textures, see-through fences, 50% shadows, haze | `solid3d`, `textured3d`, `zoo`, `patterns`, `holes` |
| **Gradient-by-`rectfill` — stacked colour bands** | dozens | skies, backdrops, UI panels, energy fields | `neonrain` (72 rectfills), `zak` (48), `larry` (44), `monkey` (33), `tradewinds` (29) → **the gradient / `lerp_color` case** |
| **Arc / ring — radial UI** | 9 | cooldown sweeps, gauges, pies, dials | `arcs`, `digdug`, `incmachine`, `minesweeper`, `summergames` |
| **Sprites — pixel art (the minority)** | 31 | detailed characters & tilesets | `platform`, `hotline`, `finalfight`, `advancewars`, `xcom`, `heroes`, `frogger` |
| **Tilemaps — `map()` worlds** | 35 | scrolling levels from a tile grid | `10-world`, `platform`, `zelda`, `boulderdash` |
| **3D / pseudo-3D** | ~12 | true-3D (`rot3`/`project3` pipeline), scanline road/floor, raycast walls, voxels | `cube3d`/`solid3d`/`textured3d`/`flyover` (true), `raycaster`/`doom`/`mode7`/`outrun`/`racer`/`podracer` (pseudo), `infiniminer`/`elite` |
| **Generative / cellular fields** | ~12 | cellular automata, flow fields, physics sims — often per-pixel `pset` or grid `rectfill` | `doomfire`, `fire`, `sand`, `ripple`, `rope`, `pendulum`, `boids`, `crowd`, `lsystem`, `particles`, `neonrain` |

Two readings jump out:

- **The fill techniques split into "shade" and "blend."** Carts fake extra shades two ways:
  *dither* (`fillp` checker between two palette colours — 46 carts, crisp + retro) and
  *banding* (stacking `rectfill`s of hand-picked palette colours — the skies). Dither is
  well-served today; smooth blends are **not** (you can't get a colour between two palette
  indices) — which is exactly what the `lerp_color` proposal below is about.
- **The radial-loop carts (80) are the biggest un-helped group.** They're the clearest
  evidence for `ngon`/`star`, and they span every genre — arcade, puzzle, action, demoscene.

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
**Open design question (settle before building).** The fixed 32-color palette has no color
*between* two indices, so a gradient's smoothness depends on what colors it can draw. Three
approaches, in increasing power:
- **Stepped:** walk N equal bands through chosen palette entries. Easy, but it's basically
  the loop the carts already write — modest value.
- **Dithered:** `fillp`-blend the two endpoint colors with the blend ratio shifting across
  the span (the checker trick `solid3d` uses for its ground). Hard to hand-roll, on-brand
  for the dither aesthetic, works with *today's* palette model — the best **no-new-color**
  option.
- **Real RGB lerp:** if we add true-color interpolation (see the next section), `vgradient`
  becomes a genuinely smooth blend — per row, draw in `lerp_color(c_top, c_bot, t)`. This is
  what "smooth gradient" normally means, and it makes `vgradient` a thin wrapper.

**Recommendation:** ship the **dithered** form. (The real-RGB path depends on the
true-color idea in the next section, which is **parked** — don't block gradients on it;
[`galerijflat.md`](galerijflat.md) is the cart design that keeps eyeing it.)

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

## Smooth color interpolation — `lerp_color` / `rgb` (real lerping)

> **⚠ Status: a THOUGHT, parked — do NOT start building.** Captured here because it's an
> interesting idea, not because it's blessed. There are real second thoughts (below): it
> introduces a two-class color model and chips at the fixed-palette identity that gives carts
> their coherence — the exact thing the rest of this doc says to lean *into*. The shape
> helpers (#1–#6) are independent of this and can proceed without it. **This needs a
> deliberate decision (an ADR) before anyone writes code; until then it stays a note.**

Separate from the shape helpers, but the thing that makes gradients (and a lot more) *real*
instead of faked. Today every draw call takes a palette **index** 0–31, so there is no value
*between* two palette colors — every "blend" in the corpus is faked with dither or banding.
A true `lerp_color(a, b, t)` unlocks genuine gradients, glows, heat/fire ramps, neon, and
color fades (hit-flash toward white, fade toward a tint) — i.e. "real lerping and stuff,"
general, not gradient-only.

### The constraint it runs into
The palette is a fixed 32-color array; `colorN` args index it, and `pal()` (remap),
`colorkey` (sprite transparency), and `pget` (read a pixel back as an index) all assume the
indexed model. There is no central color resolver today — **67 sites** index `palette[...]`
directly (≈35 of them inside primitives as `palette[c % PALETTE_SIZE]`). So a real lerp has
to produce a color the draw path can accept that *isn't* one of the 32.

### Options
- **A — nearest-index lerp.** `lerp_color` RGB-blends then returns the closest of the 32
  indices. Zero renderer change, stays purely indexed — but it is **not smooth** (snaps,
  bands hard). Useful as a building block; does not deliver what was asked.
- **B — packed true-color token (recommended).** Color ints stay 0–31 for the palette, but a
  high tag bit encodes a 24-bit RGB value directly. `rgb()`/`lerp_color()` return such tokens;
  any primitive accepts them. Full smooth color, **backward compatible** (existing 0–31
  untouched), immediate-mode, one change point.
- **C — writable palette registers.** Grow the palette to e.g. 48 and let `set_color(i,r,g,b)`
  fill slots 32–47. Stays "indexed" (so `pget`/`pal` could see them), but only N computed
  colors exist at once — a 200-row smooth gradient can't have 200 registers, so it's awkward
  exactly where you want it, and it's stateful.
- **D — gradient-only internal lerp.** Make `vgradient` blend in true color internally and
  expose nothing else. Minimal, but no reuse for glows/fades/particles — wastes the idea.

### Recommended surface (option B)
```c
int rgb(int r, int g, int b);              // a true-color token (channels 0..255) usable anywhere a color arg is taken
int lerp_color(int c0, int c1, float t);   // smooth RGB blend of two colors → a true-color token. c0/c1 may be palette indices OR tokens; t 0..1 (clamped)
```
```c
// a real dawn sky — one smooth blend, no rectfill stacking, no dither:
for (int y = 0; y < SCREEN_H; y++)
    rectfill(0, y, SCREEN_W, 1, lerp_color(CLR_DARK_BLUE, CLR_LIGHT_PEACH, (float)y / SCREEN_H));
// a heat glow, and a hit-flash that fades toward white:
circfill(x, y, r, lerp_color(CLR_YELLOW, CLR_RED, heat));
spr_or_shape_in( lerp_color(base, CLR_WHITE, flash) );
```
With this, `vgradient`/`hgradient` (#3) become thin wrappers, and `lerp` (which we already
have for floats) gains a color sibling that reads the same way.

### How it works + cost
- **Encode:** `token = (1<<30) | (r<<16) | (g<<8) | b`. Indices 0–31 are tiny positives, so
  the tag bit never collides; color args stay plain `int`.
- **Resolve:** introduce a single `static Color resolve_color(int c)` — `if (c & (1<<30))`
  unpack RGB, else `palette[c & 31]` (through the `pal()` swap). Replace the ≈35 inline
  `palette[c % PALETTE_SIZE]` primitive sites with `resolve_color(c)`. Bounded refactor, and
  it *also* centralizes palette/`pal()` handling that's currently copy-pasted — a cleanup in
  its own right.

### Caveats to reason about (the honest part)
- **`pal()` and `colorkey` are index operations** — true-color tokens bypass them (a token is
  not an index). Document it: `pal()` won't recolor a true-color draw. Fine — it's an opt-in
  escape hatch, and sprite pixels stay fully indexed.
- **`pget` reads a pixel back as an index** by matching palette RGB; a true-color pixel
  matches nothing → returns 0/`CLR_BLACK`. Document: true-color is a *draw-time* color, not
  part of the indexed framebuffer readback.
- **Identity / teaching risk.** "Colors are 0–31, not RGB" is the lesson *and* the look.
  Keep tutorials palette-first so learners don't default to `rgb()` and lose the palette
  discipline that gives carts their coherence. Framing: the palette is the dialect;
  `rgb`/`lerp_color` are for the few effects that genuinely need a blend. The studio.h
  palette comment gets a footnote, not a rewrite.

### Status: parked — second thoughts
This is the part to be wary of, and the reason it's a thought rather than a plan:

- **It splits the color model in two** (indexed *and* true-color). Every future feature then
  has to answer "does this work on true-color?" — `pal`, `pget`, `colorkey` already don't.
  That's permanent cognitive overhead for a learn-C console whose whole pitch is *small and
  legible*.
- **It erodes the identity this very doc argues for.** The fixed 32-color palette is the
  look *and* the lesson; an `rgb()` escape hatch is exactly what makes carts stop looking
  coherent once learners reach for it (and they will).
- **The main use case is already covered, in-aesthetic.** Dithered gradients (#3) give the
  sky/backdrop blend without leaving the palette — and they look *more* like the rest of the
  engine, not less.

So: **not recommended to start.** If it's ever revisited, it's gated behind explicit
`rgb()`/`lerp_color()` calls (default model untouched) and needs its own **ADR** weighing the
above — same weight class as decision 0007 ("pal recolors sprites"). The lighter, no-regret
fallback that needs no decision: ship `lerp_color` as **nearest-index** (option A) — honest
about the 32 colors, zero renderer change — and do gradients **dithered**.

---

## Recommended first batch

**#1 (ngon/star) + #2 (poly/polyfill)** — highest leverage, most "geometry-first", cut the
most code, and compose cleanly on `trifill` exactly like `quadfill`. Ship them as one batch
with a demo/tutorial cart (a "shapes from code" gallery: rotating gears, a hex grid, a
sparkle burst, a polygon blob) and a baked thumbnail, per the [cart-authoring](../guides/cart-authoring.md) workflow.

**Color (`lerp_color`/`rgb`) is NOT in any batch — it's parked** (see its section). If
gradients (#3) are wanted, do them **dithered**; that needs no color-model decision. The
true-color idea only comes back if someone deliberately reopens it with an ADR. #1/#2 don't
touch color at all.

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
