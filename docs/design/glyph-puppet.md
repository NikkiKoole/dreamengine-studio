# Glyph puppet — animation without frames

STATUS: EXPLORING (2026-07-11) — `glyphpup.c` is a working experiment (dude/dog/bird cast + glyph
confetti). The MODEL below is settled through hands-on iteration; the SCALE channel and the authoring
tool are proposed, not built. Grew out of fmbox's LCD dancer (`fmbox.c` `draw_viz`).

The bet: **you don't bake animation frames, you compose motion from a tiny bank of shapes.** A figure
is a loose cluster of small monochrome glyph cells (6×6 in the prototype), and it comes alive from a
few cheap channels applied over the same handful of hand-drawn shapes. A dozen glyphs give an
unbounded motion space, and you *grow the bank one shape at a time* as a new pose demands it.

Cart: [`tools/carts/glyphpup.c`](../../tools/carts/glyphpup.c). Lineage: [fmbox's LCD dancer](fmbox-blind-brief.md).

## The channels

1. **Glyph swap** — change which shape sits in a cell (legs A↔B = a walk, mouth open on a hit, wings
   up = a flap). Fires even under grid-snap.
2. **Whole-pixel nudge** — offset a cell off its slot. The life channel (idle bob, beat-bounce, a
   deliberate arm/head lift).
3. **Scale / squash-&-stretch** *(proposed)* — stretch a glyph on two axes independently (wide-flat
   on impact, tall-thin on a jump). See [Arbitrary 2-axis scaling](#arbitrary-2-axis-scaling-squash--stretch).

## The honest lo-fi rules (settled)

These were nailed down by iterating on the prototype — each fixed a real artifact:

- **Small LOGICAL pixel grid, scaled up uniformly afterwards.** Everything lives in a small grid
  (think 32×32); a separate uniform ×N step makes it big. The prototype fakes the upscale in-cart
  (`PX`-sized blocks); the honest version is to author at a tiny native resolution and let the engine
  integer-scale it (see [Open items](#open-items)).
- **A part lands on any WHOLE logical pixel — never a fraction.** Free of the 6-cell grid (x=8, x=13
  are fine) but never x=8.5. **Quantize in logical space FIRST, then scale** — rounding *after*
  multiplying by the zoom lets a part smear across the raster (the first bug we hit).
- **The body is RIGID.** Idle bob / beat-bounce is ONE shared offset added to every cell. Because
  each slot is already a whole number of pixels, one shared offset shifts every cell by the identical
  rounded amount, so the gap between legs and body never drifts. (The second bug: per-cell independent
  drift made the waist seam shimmer — read as sub-pixel jitter even though each part was whole-pixel.)
- **Parts move relative to each other ONLY for a deliberate articulation** (an arm gesture, a leg
  swap) — never idle jitter. Scale (squash) counts as deliberate; the *interior* pixels may go
  non-uniform, but the shape is still *placed* at a whole pixel.

## The payoff: a shared bank, cheap cast

Press `V` to cycle three creatures that reuse the **exact same glyphs**:

| Creature | Reused glyphs | New glyph |
|---|---|---|
| **Dude** (upright) | HEAD, TORSO, LEGS pair, ARM×2 | — |
| **Dog** (side view) | TORSO body, LEGS pair placed twice (out of phase = a trot), ARM tail | `DOGHEAD` |
| **Bird** (chick) | TORSO body, ARM×2 wings (flap), LEGS feet | `BIRDHEAD` |

**A whole new animal costs one glyph** (its head) + a new slot layout. That's the thesis in one line.

## Particles = glyphs as confetti

Events throw the same 6×6 glyphs as particles (a white RING on the kick, a red HEART on the snare, a
yellow CROSS on the off-beat). They live in the same logical grid, rise in whole-pixel steps, and
vanish. Universal across all casts — a particle is just a glyph with a position, a velocity, and a
lifetime. No flicker/blink on death (tried it, cut it).

## Arbitrary 2-axis scaling (squash & stretch)

The engine already has the source-rect → dest-rect blit: **`sspr(sx,sy,sw,sh, dx,dy,dw,dh)`** scales
when `dw≠sw`/`dh≠sh`, and **`sspr_ex(...deg,ox,oy)`** adds rotation + flip (negative `sw`/`sh`).
Picking `dw` and `dh` independently = squash & stretch.

Key facts for staying honest:
- **Nearest-neighbor sampling** (no bilinear blur — `studio.c` notes the "non-integer scales by up to
  one texel" GPU/SW difference). So a scaled glyph stays *hard pixels*.
- **Integer scale = uniform blocks** (6→48 = ×8, every source pixel a clean 8×8). **Non-integer
  scale = uneven pixel sizes** (some source pixel → 2 dest px, its neighbour → 3) — which is the
  classic chunky squash look, not mush. Fine for a *deliberate* squash; the position rule still holds
  (place the glyph at a whole pixel).

**Cost of adopting it in glyphpup:** the glyphs must move onto the sprite sheet (`sspr` reads
`build/sprites.png`), replacing the `rectfill`-per-pixel loop. That trades away the author-in-plain-
text bank — unless the sheet is *generated from the same strings* at startup (keeps both). See below.

## Authoring: keep the plain-text bank AND get a visual tool

The glyphs are currently authored as **XPM-style** strings (`"..##.."`, `#` = ink) directly in the
source. That representation has a real name — **XPM (X PixMap)**: an image format whose files are
literally valid C (`char *[]` of pixel rows + a colour legend), so the picture is *readable and
diffable in code*. Our bank is essentially a **1-bit XPM**. (The monochrome sibling XBM is hex bytes —
loses the "you can see it" magic.)

The tension: a spritesheet is nice to *draw* in; the XPM-in-code is nice to *read*. **Don't choose —
round-trip, with the ASCII as source of truth:**

- **`glyphkit.js` (proposed, small):** point it at a cart's bank → render every glyph big + labelled
  to one PNG (eyeball instantly, twin of [`sprite-preview.js`](../../tools/sprite-preview.js)); plus
  an interactive pixel-toggle mode that prints the `"..##.."` string back to paste in. Bank stays
  plain text.
- **Rig composer (proposed, bigger):** place glyphs into slots to build a "funny character" (the
  dude/dog/bird layouts are hand-written today), and export both the bank *and* the slot layout.

Recommendation: build `glyphkit.js` first (draw-a-glyph → XPM string + render-bank → PNG); only build
the rig composer if hand-writing layouts in code starts to hurt. Related: the editor's two-sources-of-
truth sprite story ([editor-cart-workflow.md](editor-cart-workflow.md) §Gap 2) and
[`sprite-draw.js`](../../tools/sprite-draw.js) / [`sprite-preview.js`](../../tools/sprite-preview.js).

## Open items

The live, actionable list is the cart's `de:meta.todo[]` (`node tools/cart-todos.js glyphpup`). The
big ones:

- **SCALE channel:** wire `sspr` squash-on-kick / stretch-on-jump into glyphpup so the third channel
  is felt. Requires the glyphs on the sheet (generate from the XPM strings at startup to keep both).
- **Tiny native resolution:** author at e.g. 48×48 and drop the in-cart `PX` block-scaling — let the
  engine's own integer upscale be the "scale the grid up" step (the honest form of the model).
- **`glyphkit.js`** authoring/preview tool (above).
- **`procedural-animation` teaches vocab term** (`tools/teaches-vocab.js`) if this graduates from
  experiment — currently borrows `motion-sequencing`.
