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

**These forks are not equally hard.** Fork 2 (`camera_ex`) genuinely *gates* the design; the
other three have clear answers once Fork 2 is committed. Read in that order.

### Fork 1 — RGBA (`uint32`) vs index (`uint8`) buffer

There's an elegant index option worth naming first: **`uint8` index buffer + a palette-LUT shader
at the final upload** — upload the byte buffer as an R8 texture, and the existing `DrawTexturePro`
runs a tiny fragment shader mapping `index → palette[index]`. That makes `pset`/`spr`/fills byte
writes (cheapest, native to the 0–63 model), `pal()` a remap, and — the real prize — **full-screen
palette *cycling* free** (animate the LUT uniform: the classic water/fire trick).

**The dealbreaker is `pset_rgb`.** The CPU-shader trilogy (shadelab/caustics/raymarch) and any
true-color gradient paint arbitrary 24-bit, which can't live in a `uint8`. You'd need a parallel
RGBA plane only-when-`pset_rgb`-is-used → two planes to composite per pixel → true-color becomes a
second-class citizen when it's actually a first-class teaching feature.

**Recommend RGBA.** 256KB is nothing, `pset_rgb` is native, `pal()` recolor applies cheaply at the
write (LUT lookup before the store), one uniform path, no dual-plane compositing. Name the two real
losses so the choice is informed:
- **Cheap full-screen palette *cycling*** — RGBA bakes the colour at write, so cycling means
  redrawing. No evidence carts use screen-wide cycling today (`pal()` is per-draw sprite recolor,
  ADR-0007), so this is a hypothetical loss, recoverable later as an opt-in paletted mode.
- **Index-based blend tables** (parked STATUS-18, `blend-tables.md`) are inherently index-space
  LUTs; an RGBA canvas would blend in RGB instead — arguably more flexible, but it changes how that
  future feature would be built.

### Fork 2 — `camera_ex()` zoom/rotation (the one that gates)

Today `BeginMode2D(cam)` gives zoom/rotation free on the GPU. Three ways to do it in software, two
with killers:

- **(A) Software-transform every primitive** (rotate/scale coords before rasterizing). **Killer:**
  rotation re-opens *exactly* the gap/seam class `rasterization-consistency.md` spent four sessions
  killing — a rotated software fill staircases into holes — and now *every* primitive (even `pset`,
  even `spr`) has it, plus sprites become affine texture-maps. Re-fighting solved bugs across the
  whole draw layer.
- **(B) Render un-transformed into `cbuf`, GPU-sample it transformed at upload** (one rotated/zoomed
  quad). Keeps camera cheap and primitives axis-aligned (no gaps). **Killer:** a cart that draws
  world-space *then a screen-space HUD* in one frame — most carts — can't be one flat buffer + one
  transform; the HUD rotates too. Fixing it means flush points at every `camera()` toggle, and the
  compositing complexity creeps back.
- **(C) Software-canvas the no/translation-camera case; `camera_ex` frames fall back to today's GPU
  path.** Plain `camera()` (translation) is *trivial* in software — offset the write coords
  (`pset → px-camx, py-camy`); reset to (0,0) and writes hit literal coords, so **world + HUD both
  work** with no upload-time transform. Only zoom/rotation can't be offset-written, and those carts
  use the GPU path **per frame**.

**Recommend C — and it's aimed, not a compromise.** The carts that need this are the `pset`-heavy
ones: `dpaint`, `interiors`, the CPU-shaders — **none use `camera_ex`**. `camera_ex` is rare, and
C ships the win exactly where the survey points, sidesteps both killers, and defers
rotation-in-software until there's measured demand. (The "translation-camera-only prototype" below
*is* committing to C.)

> **Note (2026-06) — `camera_ex` ≠ "needs the software canvas".** `camera_ex` splits into ZOOM and
> ROTATION, and they're very different for fills. A rotation-0 camera (zoom only) is **axis-aligned
> affine**, so the disc/poly span fast-paths are byte-safe under it — and the disc gate was relaxed
> to `rotation==0` (`00dd3f6`), which **fixed `orbit` (a zoomed planet-circfill cart) 9× without any
> software canvas.** Only **rotation** (`rotation != 0`) actually forces fills onto the per-pixel
> path. So the software canvas's fill win is for the *rotating-camera* subset, which is rarer still —
> and a rotating cart that's *also* per-pixel-bound is the genuinely-open "may not exist" question
> (`orbit` was NOT it; it was zoom-only).

> **Concrete example of a different profile shape: `sloop`** (the spline road world). Profiled
> 3.5ms mean / 4.2ms p95, but **GPU-draw-call-bound, not pset-bound** (984 `rectfill_rot` + 528
> `line` ≈ 1500 GPU calls/frame; `pset` only 41). Its cost isn't *any* software fill — `rectfill_rot`
> and `line` are GPU primitives — so neither the disc/poly span fills, the zoom-gate relaxation, nor
> the software-canvas v0 (which is about the `pset` path) touch it. A software canvas would only help
> `sloop` via the *full* option-1 path (port `rectfill_rot`/`line` to CPU writes), and only then.
> It's the proof that a real non-`pset`, non-fill profile shape exists — GPU draw-call *count* — and
> that "the software canvas isn't a universal speedup" is a fact, not a hedge.

### Fork 3 — compositing (falls out of Fork 2)

A compositing problem only exists if some primitives draw GPU and some CPU **in the same frame**
(then cross-path z-order needs flush/interleave). The rule that dissolves it: **in software-canvas
mode, *everything* writes to `cbuf`** — painter's order is automatic (call order == pixel order).
And because Fork-2/C's fallback is **per-frame** (a frame is wholly software or wholly the GPU path,
never mixed), there's no within-frame mixing to reconcile. So "all-or-nothing per frame" isn't a
safety rule you impose — it's what C already gives you. The price it implies is Fork 4.

### Fork 4 — text / `tritex` / `spr` as CPU blits

Graduated labour:
- **`spr`** — blit a 16×16 sheet region with colorkey/`pal`/flip. Nested loop, 256 px. Easy.
- **`print`** — glyph blits from the bitmap font atlas; same shape as `spr`, 8×8/char (all `FONT_*`
  are just different source images). Easy.
- **`tritex`** — affine texture-mapped triangle (scanline fill, interpolated u,v). The one real
  rasterizer — more code, but textbook (2D affine, no perspective). Used by the 3D carts.

**Wiring requirement this surfaces:** the spritesheet + font must be kept as CPU `Image`s in RAM
(not just GPU `Texture`s), and the sprite editor's live edits must update the CPU copy. Mostly
"don't `UnloadImage` after upload," but it's a real dependency.

### The free wins (these retire existing pain, not just "also nice")

- **`clip()`** → an `if` per write, cheaper than GPU scissor. **`fade`/blend** → read-modify-write
  on `cbuf`, *easier* than the shader path.
- **`pget`/`pget_rgb`** → direct array read. Today that's a GPU readback **plus a 256KB `Image`
  alloc every frame** (the churn `profiler.md` flagged) — gone. And `pget` is **disabled on web**
  today; a CPU buffer enables it.
- **Web** → `UpdateTexture` works identically and removes `rlVertex3f`-on-WebGL (worse than native).
- The scale-up to the window stays GPU (`UpdateTexture` RGBA + `DrawTexturePro` nearest-neighbour),
  so crisp pixel scaling is untouched.

## The cleanest v1 (what the forks point to)

> **RGBA `cbuf`; software-canvas for no/translation-camera frames with `camera_ex` falling back to
> today's GPU path per-frame; everything (incl. `spr`/`print`/`tritex`) writes to `cbuf`; one
> `UpdateTexture` + `DrawTexturePro` at frame end.**

Not the scary version — a well-trodden fantasy-console architecture, with Fork-2/C carving off the
genuinely-hard part (rotation) cleanly.

## Migration — and the honest catch about a partial slice

The tempting cheap first step is "route only `pset`/`pset_rgb` + fills to `cbuf`, leave
`spr`/`print` on the GPU." **It doesn't validate** — per Fork 3, mixing CPU `cbuf` writes and GPU
`spr`/`print` in one frame breaks z-order (a sprite drawn between two `pset`s would composite
wrong). So the first *validatable* prototype is either:

1. **Full draw-layer in `cbuf`** (port `spr`/`print`/`tritex` too) for no/translation-camera frames,
   `camera_ex` → GPU fallback. This is the real v1; bigger first step but it's the only honest one.
2. **Scope to carts that don't mix** — i.e. pure per-pixel carts (the CPU-shaders, a `dpaint`-like)
   that only use `pset`/fills/`rectfill` and no `spr`/`print` mid-scene. A narrower opt-in mode that
   proves the buffer + upload mechanism and the `dpaint`/CPU-shader win without porting the blits.

Then: **validate byte-identical** (dump+shasum oracle + `raster_test`) and **A/B on the fleet**
(`profile-fleet.js`) — expect `dpaint`/`interiors`/`orbit`/CPU-shaders to drop hard, everything else
neutral-or-better. If it lands, make software-canvas the default for non-`camera_ex` frames and keep
the GPU path behind `DE_SOFTWARE_CANVAS=off` for a cycle; the span-fill `DrawRectangle` fast paths
fold into the `cbuf` writers.

## Minimal slice (v0) — the smallest thing that shows a real, validatable result

The naive "route `pset`+fills to `cbuf`, leave `spr`/`print` on GPU" slice fails (Fork 3
z-mixing). But two observations collapse it into something small **and** byte-identical-checkable:

- **Target only the carts that need it and don't mix**: the CPU-shaders (caustics/shadelab/raymarch
  = `cls` + `pset_rgb` + HUD `print`), `dpaint` (`cls` + `pset` + `rectfill`/`rect` + HUD `print`),
  and `pixelperfect` (`pset_rgb` per-pixel world view + HUD `print` + 2 `line`s). No `spr`, no
  `tritex`, no `camera_ex`. These are exactly the survey's pset-heavy carts.
  > Independently confirmed three+ times: the survey's aggregate, plus standalone `⏱ profile` runs
  > on `lotfill`/`orbit`/`dpaint`/`pixelperfect` all bottom out in `… → pset[_rgb] → DrawPixelV →
  > rlVertex3f` (≈60–75% of frame), with `rlSetTexture` churn alongside (the white-texture RLGL
  > state the per-pixel path re-touches). The diagnosis isn't in doubt; only the build-it decision is.
- **HUD text is the top layer, so defer it.** Keep `print` on the GPU but **record its calls during
  `draw()` and replay them AFTER the `cbuf` upload** (a tiny text-command list). Text lands on top
  (where it already was, drawn last) and stays the *same GPU `DrawTextEx`* → byte-identical, and you
  **don't port `print`**.

**v0 surface (behind `DE_SOFTWARE_CANVAS`, opt-in):**
- `uint32 cbuf[SCREEN_W*SCREEN_H]` + a `sw_canvas_active` flag.
- `cls` → clear `cbuf`. `pset`/`pset_rgb` → `cbuf` write (+ `clip` bounds, + `camera()` translation
  as a write offset). `rectfill`/`rect` → `cbuf` row writes.
- **Fills come for free**: `circfill`/`polyfill`/etc. already fall to `plot_pat → pset` for their
  pixels — once `pset` writes `cbuf`, they route automatically. (Just disable the `DrawRectangle`
  span fast-path while `sw_canvas_active` — use the per-pixel `plot_pat` path; still no `rlVertex3f`.)
- `print` (and any HUD `line`, e.g. `pixelperfect`'s 2) → deferred GPU overlay: record the call,
  replay after the upload so it lands on top, same GPU call → byte-identical, no port. `spr`/
  `tritex`/`camera_ex` → **unsupported in v0**: warn and skip (the target carts don't use them).
  (The deferred-overlay trick only holds while those calls are genuinely the top layer — true for
  HUDs; it is NOT a general `line` solution, just a v0 convenience for the target carts.)
- Frame end: `UpdateTexture(canvas.texture, cbuf)` → replay deferred text into `canvas` → the
  existing `DrawTexturePro` scale-up. (One upload, one quad, plus the few text calls.)

**Why it validates cleanly:** `cls`/`pset`/`pset_rgb`/`rectfill` are integer-coordinate writes, so
`cbuf[y*W+x]=c` produces the *same pixels* as the GPU `DrawPixel`/`DrawRectanglePro` they replace;
deferred `print` is the same GPU call. So a CPU-shader cart should be **byte-identical** software-vs-
GPU on the dump+shasum oracle — a real pass/fail, not "looks right." (The fills are also already CPU
coverage, so they match too.)

**The result you'd see:** A/B `caustics`/`shadelab`/`raymarch`/`dpaint` with `profile-fleet.js`,
`DE_SOFTWARE_CANVAS=on` vs off. These are ~pure `pset_rgb`/`pset` carts, so this is where the survey
predicted the biggest drop — v0 confirms (or refutes) the whole thesis on the carts that matter,
for a fraction of the full-rewrite cost, and without touching `camera_ex` or the blits.

**What v0 deliberately leaves for later:** `spr`/`print`/`tritex` as real CPU blits (the option-1
full draw layer), and `camera_ex` (Fork 2). v0 only proves the buffer+upload mechanism and the
headline win; it is *not* the path to making software-canvas the default (that needs the blits).

## Risks / why it might *not* be worth it

- **It's a rewrite of the draw layer**, and the partial-slice catch above means the first honest
  prototype is option-1-sized (port the blits) unless you take the narrow option-2 opt-in. High
  blast radius; the byte-identical oracle + `raster_test` + fleet A/B are the net, but it's a lot of
  carts to re-validate.
- **`camera_ex` carts bifurcate** — they stay on the GPU path, so two renderers coexist until/unless
  rotation-in-software is solved. Acceptable because that set is small and disjoint from the
  pset-heavy set, but it's real maintenance surface.
- **Most carts are already at 60fps** — this buys *headroom* (bigger canvases, the CPU-shader work,
  web), it doesn't fix a present fire. The decision isn't "is it correct" (the design is sound) —
  it's "is removing the `rlVertex3f` ceiling worth a draw-layer rewrite *now*."

## Recommendation

**Park the decision, not the design** — the data keeps pointing here (every profile in this arc ends
at `rlVertex3f`; the span fills only routed *around* it for fills), so a software canvas is the move
that removes it engine-wide, and at 64k px the precedent (every fantasy console) says it should win.
When picked up: prototype Fork-2/C behind `DE_SOFTWARE_CANVAS`, choose option 2 (narrow per-pixel
opt-in) to prove the mechanism + the `dpaint`/CPU-shader win cheaply, A/B on the fleet, and only
commit to option 1 (porting `spr`/`print`/`tritex`) if those numbers land the way the survey
predicts. Earn the rewrite with data at each step — same discipline as the rest of the ledger.

## See also
- [../guides/engine-optimization.md](../guides/engine-optimization.md) — the fleet survey that
  pointed here + the ledger of the fill optimizations this would subsume.
- [cpu-shaders.md](cpu-shaders.md) #5 — the parked "true offscreen buffer"; a software canvas is a
  superset (the whole canvas is in RAM, multipass becomes trivial).
- [rasterization-consistency.md](rasterization-consistency.md) — the CPU per-pixel coverage
  invariant the software rasterizers already satisfy; the rotated-camera seam fork 2 must respect.
