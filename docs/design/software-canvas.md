# Software canvas — kill `rlVertex3f` by rasterizing the whole frame on the CPU

> **Genre: design exploration → PROPOSAL, not built.** This is the design pass the
> [engine-optimization fleet survey](../guides/engine-optimization.md#fleet-survey--where-the-engine-spends-time-across-carts-2026-06)
> asked for before any code: its #1 fleet-wide cost is the per-pixel GPU submission path
> (`pset`/`DrawPixel` → one `rlVertex3f` per pixel), and the fix is an architecture change,
> not a patch. Decide the open forks (below) before writing it. Companion to
> [cpu-shaders.md](cpu-shaders.md) #5 (the parked "true offscreen buffer") and
> [rasterization-consistency.md](rasterization-consistency.md) (the CPU-coverage invariant).

## The problem (measured)

The fleet survey (`tools/profile-fleet.js`, 32 carts) is unambiguous: **`pset` is the dominant
engine cost by 2.5×** — 59,593 calls/frame across 21 carts (`dpaint` 30k, `interiors` 18k,
`lotfill` 9k), each one a single immediate-mode GPU vertex (`pset → DrawPixel → DrawPixelV →
rlVertex3f`). Every profile in this whole optimization arc has the same shape: **`rlVertex3f`
on top.** The span-fill work (polyfill, circfill/ovalfill) killed it for *fills* by replacing N
`DrawPixel` with one `DrawRectangle` — but raw `pset(x,y)` calls are arbitrary points; they can't
be coalesced. The CPU-shader carts (`pset_rgb`: shadelab/caustics/raymarch) are 100% this path.

## The thesis

**At this resolution, a software framebuffer uploaded once per frame beats GPU immediate-mode.**
The canvas is 320×200 = **64k pixels** — tiny. The current renderer draws *into* a
`RenderTexture` on the GPU (`studio.c`: `BeginTextureMode(canvas)` + `BeginMode2D(cam)`), so every
primitive pays GPU per-vertex/per-call submission overhead — which, at 64k px, **dwarfs the actual
pixel work**. That's exactly what the profiles show: it's never the math, always `rlVertex3f`.

So: keep a CPU pixel buffer that *is* the canvas. Every primitive writes to the buffer (CPU
rasterization). Once per frame: `UpdateTexture(canvas, buf)` + the existing `DrawTexturePro`
scale-up to the window. **`rlVertex3f` disappears from the entire draw path at once** — `pset`,
every fill, sprites, lines, text. This is what PICO-8 / TIC-80 / every fantasy console does;
we're currently the odd one out doing GPU immediate-mode for a 64k canvas.

The span fills + clamp cache we just shipped aren't wasted — they're the *CPU rasterizers* a
software canvas needs (span fill = how you fill a polygon into a buffer; the clamp is the bounds
check). They become the implementation, not dead code. The dither `plot_pat` path, likewise.

## Architecture sketch

```
static uint8_t  cbuf[SCREEN_W*SCREEN_H];   // palette-index canvas (0..63), OR
static uint32_t cbuf[SCREEN_W*SCREEN_H];   // RGBA — see fork 1
// pset:        cbuf[y*W+x] = color;                    (+ clip bounds check)
// rectfill:    memset rows                             (CPU, trivial)
// spr:         blit from spritesheet, palette/pal()/colorkey in the loop
// line/circ/poly/oval/tri: the CPU rasterizers we already have, writing cbuf
// fade/blend:  read-modify-write on cbuf               (EASIER on CPU than GPU)
// end of frame: UpdateTexture(canvas.texture, cbuf); DrawTexturePro(canvas→window)  // one upload, one quad
```

## The design forks — decide these first

1. **Index buffer (`uint8`) vs RGBA (`uint32`).** Index keeps the palette model native:
   `pal()` recolor and palette cycling are free (swap the lookup at upload), `pset(x,color)` is a
   byte write, the buffer is 64KB not 256KB. **But `pset_rgb`/`pget_rgb` (the CPU-shader true-color
   path) needs 24-bit.** Options: (a) RGBA buffer throughout (simplest, true-color native, 256KB,
   palette ops do the lookup on write); (b) index buffer + a separate RGBA path only when a cart
   uses `pset_rgb` (dual buffers/compositing — complex); (c) index buffer, and `pset_rgb` writes a
   packed value into a parallel plane. **Lean (a) RGBA** — 256KB is nothing, it's the most general,
   and the upload is one `UpdateTexture` either way.

2. **`camera_ex()` zoom/rotation — the hard one.** Today the GPU `Camera2D` gives zoom/rotation
   for free. In software, the options are: (a) **software-transform every primitive** (rotate/scale
   coordinates before rasterizing — correct, but every primitive needs the transform and rotation
   reintroduces the gap/seam problems `rasterization-consistency.md` fought); (b) **render the world
   un-transformed into `cbuf`, then GPU-sample it transformed at upload** (one textured quad with
   rotation/zoom — keeps camera GPU-cheap, but breaks if the cart mixes camera and screen-space draws
   in one frame); (c) **keep a thin GPU path only for `camera_ex`** and software-canvas everything
   else (hybrid — compositing-order pain). `camera()` (plain translation) is trivial in all cases
   (offset the writes / the upload quad). **This fork gates the whole thing** — most carts use
   `camera()` or no camera; `camera_ex` (rotation/zoom) is rarer. A staged answer: software-canvas
   the no/translation-camera case first, leave `camera_ex` carts on the GPU path initially.

3. **Compositing / does anything *stay* GPU?** If the whole frame is one CPU buffer, painter's
   order is automatic (writes happen in call order) — no compositing problem *as long as every
   primitive writes to the same buffer*. The danger is a half-migration where some primitives are
   GPU and some CPU: then z-order across the two needs flush points. **Cleanest is all-or-nothing
   per frame.** Text (`print` → `DrawTextEx` from a font texture) and `tritex` (textured triangle)
   must become CPU blits too — both straightforward (glyph blit, affine texture map).

4. **`clip()`** → a bounds check on every write (cheaper than the GPU scissor). **`fade`/blend
   tables** → read-modify-write on `cbuf` (strictly easier than the GPU shader approach). These get
   *simpler*, not harder.

5. **`pget`/`zoom_rect`/screenshot/snapshot** read the canvas back today (GPU readback, slow). With
   a CPU buffer they're **free** (`cbuf` is already in RAM) — this also fixes the `pget`-per-frame
   allocation churn noted in profiler.md. A real win, not just a cost.

6. **Web (emscripten).** A CPU buffer + one `UpdateTexture` works the same on web and removes the
   `rlVertex3f`-on-WebGL cost (which is *worse* than native). And `pget` is currently disabled on
   web — a CPU buffer would enable it. Net positive for the web build.

## Migration — stage it, don't big-bang

1. **Prototype behind a flag** (`DE_SOFTWARE_CANVAS=on`), RGBA buffer, no/translation camera only.
   Route `pset`/`pset_rgb` + the fills + `rectfill`/`spr`/`line`/`print` to `cbuf`; `camera_ex`
   carts fall back to the GPU path. One `UpdateTexture`+`DrawTexturePro` at frame end.
2. **Validate byte-identical** on the stress carts (the dump+shasum oracle) and `raster_test`, and
   **A/B on the fleet** (`profile-fleet.js`): expect `dpaint`/`interiors`/`orbit`/CPU-shaders to
   drop hard, everything else neutral-or-better.
3. **Decide `camera_ex`** (fork 2) with data in hand — software-transform vs GPU-sample-at-upload.
4. If it wins broadly, **make it the default**; keep the GPU path behind the flag for one cycle,
   then retire it (and the span-fill `DrawRectangle` fast paths fold into the buffer writers).

## Risks / why it might *not* be worth it

- **`camera_ex` rotation correctness** (fork 2) is real work and re-opens the rotated-fill seam.
- **It's a big-bang-ish rewrite** of the draw layer — high blast radius, many carts to re-validate
  (the byte-identical oracle + `raster_test` + fleet A/B are the safety net, but it's a lot).
- **Most carts are already at 60fps** — this buys *headroom* (bigger canvases, more ambitious
  per-pixel carts, the CPU-shader trilogy, web) more than it rescues anything shipping today. The
  honest framing: it removes the *ceiling*, it doesn't fix a present fire.
- **raylib is built around GPU draw** — fighting the grain in a few spots (though `UpdateTexture` +
  a textured quad is a well-trodden raylib path).

## Recommendation

Worth prototyping (step 1) **because the data keeps pointing here** — every profile in this arc
ends at `rlVertex3f`, and the span fills only routed *around* it for fills. A software canvas is
the move that removes it for the whole engine, and at 64k px the precedent (every fantasy console)
says it should win. But it's gated on fork 2 (`camera_ex`) and it's a rewrite, so: **prototype
behind `DE_SOFTWARE_CANVAS`, RGBA, translation-camera-only, A/B on the fleet, then decide.** If the
prototype shows `dpaint`/`interiors`/CPU-shaders dropping the way the survey predicts, commit to the
staged migration; if `camera_ex` proves too costly to match, keep it as an opt-in mode for the
per-pixel-heavy carts (which is still a big win for exactly the carts the survey flagged).

## See also
- [../guides/engine-optimization.md](../guides/engine-optimization.md) — the fleet survey that
  pointed here + the ledger of the fill optimizations this would subsume.
- [cpu-shaders.md](cpu-shaders.md) #5 — the parked "true offscreen buffer"; a software canvas is a
  superset (the whole canvas is in RAM, multipass becomes trivial).
- [rasterization-consistency.md](rasterization-consistency.md) — the CPU per-pixel coverage
  invariant the software rasterizers already satisfy; the rotated-camera seam fork 2 must respect.
