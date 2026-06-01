# The tool you're building: DELUXE PAINT (in-cart paint program)

## What it is
A full-featured pixel paint program — a love letter to Amiga Deluxe Paint — that runs as
a CART using the runtime (mouse + `pset` + the 32-color palette). A creative TOOL, not a
game. The explicit goal: be genuinely usable, AND **beat the editor's native sprite tool
in the places the engine can shine** — dither **patterns**, dithered **gradients**, and
**color cycling**.

## Slice (locked) — 🟡 (feature-rich but bounded)
A paint canvas + a real toolbar. Core tools: **pencil/brush** (sizes), **line**, **rect**
& **oval** (outline/filled), **flood fill**, **airbrush**, **eyedropper**, **custom
brush** (grab a region → stamp it), **mirror/symmetry**, and **undo**. The 32-color
palette as swatches. Plus the three standout features below. Save/load the image with
`save_bytes`. Cut: layers, animation timeline (beyond color cycling), file export beyond
save_bytes.

## The standout features (why it beats the native editor)
1. **Pattern fill (`fillp`):** a pattern picker (the `FILL_*` set + a few custom 16-bit
   masks) so fills and shapes lay down 4×4 dither **textures**, two-tone, with an
   optional transparent "hole" color. The native tile editor doesn't do this.
2. **Dithered gradient fill:** pick two colors + a direction; fill a region with a
   gradient faked via graduated `fillp` density (sparse→solid dither) — classic DPaint.
3. **Color cycling:** select a palette range and **animate** it — rotate those entries
   each frame with `pal()`. Because `pset` reads `palette[]` at draw time, the canvas
   pixels drawn in those indices **cycle live** (flowing water, fire, marquee lights) for
   free. This is the showpiece.

## Canvas & rendering (mind performance)
Keep state in an `int canvas[H][W]` of palette indices and redraw it each frame with
`pset`. **Bound the canvas** (e.g. ~192×144 or 160×120) with the toolbar around it so the
pset count stays sane (raylib batches the 1px rects, but don't try a full 320×200 at
huge sizes without checking the frame time — `watch()` the FPS while developing). Commit
strokes into `canvas[][]`; the live tool preview draws on top.

## UI & layout
A side toolbar (tool icons) + a palette strip (32 swatches, current FG/BG) + a pattern
picker + brush-size + the canvas. Buttons: undo, clear, save, load, mirror toggle, cycle
on/off + range. Keep it tactile and immediate; show the cursor's effect live.

## Feel (the "juice")
Live brush preview under the cursor, a crisp 1px tool cursor, satisfying click feedback,
a subtle UI hover highlight, the gradient/pattern preview swatch updating as you tweak,
and the color-cycling demo looking genuinely mesmerizing. Light, optional UI blips — a
paint tool should feel *quiet and precise*, so don't over-sound it.

## Data model (suggested)
- `int canvas[H][W]`; FG/BG color; current tool enum; brush size; current `fillp`
  pattern + hole color; mirror flags; a custom-brush buffer + its size.
- **undo:** one or a few snapshots of `canvas` (a ring of N copies) — `save_bytes` for
  save/load.
- cycling: a {lo,hi,speed} range applied via `pal()` rotation each frame.

## Controls
Mouse-primary: left-click paint with the current tool/color, right-click = secondary
(erase / pick BG), click toolbar/palette/pattern to select, drag for line/rect/oval and
to grab a custom brush. Keyboard shortcuts (b/l/r/o/f/p/i for tools, [ ] brush size,
x swap colors, u undo, g grid). Mouse is the whole experience.

## Lean into / read
`patterns.c` + `holes.c` (the `fillp` dither system — the heart of the standout feature),
`fourx.c`/`omnichord.c`/`drummachine.c` (mouse toolbar/UI patterns), `turtle`/`16-spirograph.c`
(drawing primitives), `typesave.c` (`save_bytes`). The native sprite editor lives in
`editor/src/sprite-editor.js` — you can mine it for tool ideas, but this is a standalone
runtime cart, not that editor. Skip: layers, frame animation, external file formats —
a great single-canvas painter whose patterns/gradients/cycling outshine the basics.
