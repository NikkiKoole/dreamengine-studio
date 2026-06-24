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
be coalesced.

> **Correction (2026-06) — the "CPU-shader trilogy is `pset_rgb`" claim throughout this doc is
> stale; re-profiling found them rectfill-bound.** `raymarch` uses **zero** `pset`/`pset_rgb`;
> `caustics`/`shadelab` use it negligibly (pset ≈ 0–3/frame). They actually paint the screen as
> **rectfill spans** (`caustics` rectfill ≈ 16k/frame, `raymarch`/`shadelab` ≈ 7k) — a manual span
> optimisation, which is a draw-call-**count** shape (the `sloop` profile), *not* the per-pixel
> `rlVertex3f` path. So wherever this doc lists caustics/shadelab/raymarch as the pset target set,
> read it as: **the genuinely pset-per-pixel-bound carts are `dpaint` (30k), `pixelperfect` (26k),
> `interiors` (18k), `lotfill` (9k)** — those are v0's real win. The CPU-shaders are still valid v0
> targets (v0 routes `rectfill` → `cbuf` too), but via the draw-call-count win — and the
> `DE_BATCH_PSET` result warns that win may be small on M1, where the driver is cheap on draw-call
> count (see "Cheaper alternatives → Measured"). They drop hard only where small-draw submission is
> expensive (weak GPU / WebGL-on-old-hardware).

> **First cart to cross trigger (a) — `cityplan`, measured 2026-06-23.** The Recommendation parks the
> build until "a cart is actually `pset`-bound below 60fps." `cityplan` is the first one that is:
> **19.9ms/frame typical, 22.0ms p95 — 119% of the 16.6ms budget, dropping to ~47fps.** The profile is
> textbook for this doc — **~73% of wall is the removable `pset → DrawPixelV → rlVertex3f` path**
> (62.1% `rlVertex3f` + 5.4% `DrawPixelV` + 3.2% `rlSetTexture` + 1.9% `pset`), against only ~15% real
> work (`noise_*`/`cover_at`) and ~4% `prof_bump` (profiler artifact, gone in a normal build). At
> **213,916 `pset`/frame** it draws ~3.3× the 64k-px canvas — heavy overdraw (roofs over `rows_fill`),
> which is precisely the worst shape for GPU immediate-mode (each overdrawn pixel = another vertex
> submit) and cheap on a `cbuf` (repeated stores). Back-of-envelope removing the GPU path: ~20ms →
> ~6–8ms, back under budget with headroom. **Caveat on v0 fit:** it's mostly `pset`+`rectfill`+~4
> `print` (clean), but it also issues **~485 `line`/frame** — if those are world-space block outlines
> interleaved with fills (not a top HUD layer), the v0 deferred-`line`-overlay trick doesn't preserve
> z-order for them, so cityplan may need `sline`/CPU-line (Phase 2) rather than being a pure v0 target.
> Verify before treating it as the Phase-1 proof cart. **This is evidence, not a go** — logged so the
> trigger condition is captured whenever the decision is revisited.

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

> **Note (2026-06-23) — the sketch's one false assumption: there is NO CPU `line` today.** The line
> "the CPU rasterizers we already have" is true for circ/poly/oval (the `poly_fill_cov`/coverage
> family decides pixels on the CPU) but **`line()` is the sole GL holdout** — it's raylib GPU
> `DrawLine`, which *chooses* the staircase on the GPU. An audit while chasing streetlab's corner
> floor confirmed it's the only *axis-aligned* cart-facing primitive that lets GL pick pixels — the
> rest of the GL-picks-pixels surface is the rotation family (`rectfill_rot`, `spr_rot`/`sspr_ex`,
> `tritex`, rotating `camera_ex`), audited in full below. So the canvas has a hole exactly there. The missing piece is
> **`sline`** — a reflection-symmetric CPU per-axis DDA, prototyped in the probe carts
> `tools/carts/{linesym,axissym}.c` and banked (unused) in `tools/carts/streetlab.c`; recipe +
> proof in [`streetlab-corner-symmetry-plan.md`](streetlab-corner-symmetry-plan.md). It is the line
> rasteriser this architecture needs.
>
> **This sharpens the determinism case (the "free win" below), and the perf coupling.** GPU
> `DrawLine` is not just non-deterministic across drivers — it isn't even reflection-self-consistent
> (a line and its exact mirror rasterise to *different* pixels: 88 mismatches in the `linesym`
> probe). A CPU line (`sline`) is required for the bit-identical-across-devices property — it's a
> *prerequisite* for replays/ghosts/lockstep, not just a perf detail. And the coupling runs the
> other way too: a symmetric line is **unavoidably per-pixel** (a 1px line has no horizontal run to
> coalesce, unlike a span fill), so globally swapping `line() → sline` on *today's* GL renderer turns
> each line into N `pset`s — *raising* total `pset`/frame (e.g. `sloop`'s 528 lines/frame) and thus
> the priority of the very `pset` optimisation this doc is about. On the canvas, those per-pixel
> writes are cheap `cbuf` stores. So **global `sline` and the software canvas are one bet, pulling
> the same way** — and the canvas *needs* `sline` regardless. (Caveat learned the hard way: a
> symmetric line alone does **not** make symmetric *art* — fills are point-mirrored, 1px strokes are
> cell-mirrored, and on an even grid those snap conventions conflict; the canvas must pick one. See
> the plan doc's "`sline` negative result.")

> **Audit (2026-06-24) — the complete GL-picks-pixels surface is FOUR primitives, and they're all
> rotation.** Read against the current `runtime/studio.c`, the holdouts where GL's own rasterizer
> *chooses which pixels light up* (the non-deterministic, can't-just-blit cases) are exactly:
> 1. **`line()`** — raylib `DrawLine` (line ~2636). Being closed by **`sline`** (above); the only
>    *axis-aligned* one, so `sline` fully solves it for unrotated use.
> 2. **`rectfill_rot()`** — `DrawRectanglePro()` rotated quad (line ~2342).
> 3. **`spr_rot()` / `sspr_ex()`** — `DrawTexturePro(…, deg, …)` rotated sprite blit (line ~2214+):
>    GL rasterizes the tilted footprint.
> 4. **`tritex()`** — `rlBegin(RL_QUADS)` + `rlVertex2f` affine textured triangle (line ~2607, the 3D
>    carts). **The one genuinely-hard new rasterizer** — no "skip rotation" fallback freebie; it must
>    be CPU-scanline-rasterized (affine u,v interp) to live on the canvas. Phase 2's biggest item.
> 5. *(whole-block)* **rotating `camera_ex(angle≠0)`** — `BeginMode2D(cam)` with `cam.rotation` (line
>    ~2813) transforms *everything* drawn inside the block on the GPU. This is the case Fork-2/C
>    quarantines (per-frame GPU fallback), not a single primitive.
>
> Everything else is **not** GL picking pixels and is canvas-native: `pset`/`rectfill`/`rect` are
> integer-coord axis-aligned writes (→ `cbuf` stores); `circ`/`circfill`/`oval`/`poly`/`ngon`/`star`/
> `thick` are already **full CPU coverage** (span fills / per-pixel predicates) that just reroute
> their `pset`/`DrawRectangle` into `cbuf`; unrotated `spr`/`sspr` are blits (Phase-2 CPU blit loop);
> HUD `print` is the orthogonal **deferred-GPU-overlay** trick, never a pixel-picking holdout.
> **Net:** the whole remaining GL pixel-selection surface collapses to the single concept *rotation*
> — `sline` removes the axis-aligned line hole, and Fork-2/C leaves the four rotation cases on the
> GPU per-frame. `tritex` is the only one that isn't covered by either move (needs a real CPU
> rasterizer) if the 3D carts are ever to run on the canvas. The rotated-*stroke* sub-case
> (a rotated 1px `line`/outline) still needs the Xiaolin-Wu-class drawer flagged in Fork 2.

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

> **But "Recommend RGBA" is M1/desktop-correct, NOT universal — it's hardware-conditional.** On a
> small-cache / narrow-bus target the `uint8` index buffer flips straight back into contention, for
> two reasons RGBA can't answer: (1) **cache fit** — 64KB (indexed) lives in a small L2 where 256KB
> (RGBA) spills to main RAM every frame; (2) **smaller memory passing** — the per-frame
> `UpdateTexture` is an R8 upload, **4× less data**, which on a narrow bus is real budget (≈15–25% of
> a frame on a Pi Zero, vs <1% on M1 unified memory). That can outweigh the `pset_rgb` dealbreaker
> (handle true-color via the parallel RGBA plane *only when `pset_rgb` is used*, or simply don't
> offer true-color in the low-end profile). So **`uint8` is the low-end build profile**, not a dead
> option — same `DE_SOFTWARE_CANVAS` machinery, selected by a buffer-format compile flag. See
> "Low-end / small-memory targets" in the build runbook below.

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

> **Note — *inverse* mapping is the third software option for the rotated case (and the one
> option A missed).** Option A above is *forward* mapping (transform each primitive's coords, then
> rasterize) — that's what staircases into holes. The Mode-7/SNES trick is the other direction:
> iterate the *screen-space* pixels in the shape's transformed bounding box, and for each one apply
> the **inverse** camera transform to get its shape-local coordinate, then test inside (`x²+y²≤r²`
> etc.). Because you visit every output pixel exactly once, it is **gap-free by construction** — no
> seam class to re-fight. The inner loop is cheap (two adds per pixel for a rotated step; one
> multiply for zoom), and at 64k px the math is in the noise. Two honest bounds on this:
> - It applies cleanly to **fills and source-sampling blits** (the disc/poly fill, a rotated `spr`,
>   a Mode-7 background) — i.e. *exactly* the case the 2026-06 note isolated as the only thing
>   rotation actually forces onto the per-pixel path. So if Fork-2/C ever needs to grow a
>   rotating-fill path, inverse mapping is how, without a GPU fallback.
> - It does **not** rescue rotated *stroked* primitives (a 1px `line`/`circ` outline). Those are
>   still rasterized forward in screen space, so they re-inherit the consistency/aliasing problem
>   (`rasterization-consistency.md`) and need a sub-pixel line drawer (Xiaolin-Wu-class) to look
>   stable while rotating. So "kill Fork 2, do *all* rotation in software" overclaims; inverse
>   mapping shrinks the hard part to stroked outlines, it doesn't erase it. Fork-2/C stays the
>   recommendation; inverse mapping is the tool *inside* C if the rotating-fill demand ever shows up.

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
- **Determinism across devices — a *feature*, not just the migration oracle.** The byte-identical
  check below is framed as a pass/fail test, but it's also a capability: a frame computed entirely
  in C is bit-identical on every machine (native/web, Apple/Intel/ARM), where the GPU path can
  round/AA 1px differently per driver. That's the precondition for replays, ghost runs, and
  lockstep netcode — and it plugs straight into infrastructure we already have (`play.js --det`,
  the dump+shasum oracle, byte-reproducible `--wav`). Worth naming as a reason this architecture is
  *right* for a fantasy console, beyond the perf win.
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

## Cheaper alternatives — try these *before* committing to the rewrite

If the goal is narrowly "**lower `pset` time**" (not "earn the whole architecture"), there's a much
smaller move that the rewrite framing skips over — and it's worth measuring first, because it
de-risks the big decision either way.

**The mechanism, precisely (raylib 5.5).** `pset → DrawPixel → DrawPixelV` draws each pixel with
the shapes-texture and toggles `rlSetTexture` around it. rlgl *already* batches immediate-mode
vertices into an 8192-element buffer and only flushes on buffer-full / draw-mode change / **texture
change** / `EndDrawing`. So the popular "you pay 60,000 GPU handshakes" story is *not* generically
true for raylib — **except** that `DrawPixel`'s per-pixel texture toggling is itself a per-pixel
texture-state change, which is exactly the `rlSetTexture` churn the survey already measured
alongside `rlVertex3f`. In other words: the per-pixel path *defeats* the batcher that would
otherwise coalesce it.

**That implies a cheap, reversible experiment (no `cbuf`, no draw-layer rewrite):**

1. **Batch the `pset` runs yourself.** Within a frame, emit all per-pixel writes inside *one*
   `rlSetTexture` + `rlBegin(RL_QUADS)`/`rlEnd()` block (or accumulate `{x,y,color}` into a CPU
   array and flush once with the rlgl vertex API), instead of one `DrawPixel` call each. This stops
   re-touching texture state per pixel, so rlgl's existing batch coalesces the lot. The change is
   local to `studio.c`'s `pset`/`pset_rgb` (and the `plot_pat` per-pixel fill path), behind a flag,
   A/B-able with `profile-fleet.js` in an afternoon.

**What it tells you — the real value is the measurement.** The experiment splits `pset` cost into
its two components: *state churn* (which batching removes) vs. *raw per-vertex transform/append*
(which only a software canvas removes, by not emitting vertices at all). If batching alone recovers
most of the frame on `dpaint`/the CPU-shaders, **you may not need the software canvas at all** —
you have a 20-line fix for a problem you "don't really have." If it recovers only part, you've
*quantified* exactly what the rewrite buys, and the v0 below becomes a much better-informed bet.

**What it does *not* give** (so it's a perf patch, not the architecture): the pixels stay
GPU-resident, so `pget`/`pget_rgb` still need the readback + the 256KB `Image` alloc, `fade`/blend
stay shader-side, web `pget` stays disabled, and there's no cross-device determinism win. Those are
all software-canvas-only. Batching lowers the `pset` ceiling; it doesn't move the floor up to the
free wins.

**Order of operations, then:** run the batching experiment first (cheap, reversible, and it
*measures* the thing the whole rewrite decision turns on) → only if the residual per-vertex cost is
big enough to matter does v0 below earn its keep.

> **Measured (2026-06) — the batching experiment was run, and it does *not* lower `pset` time on
> Apple Silicon.** The path is implemented behind `DE_BATCH_PSET=on` (`studio.c` `px_emit` — same 1px
> quad as `DrawPixel`, same winding, but it leaves the white texture bound instead of toggling
> `rlSetTexture(0)` per pixel, so a run of psets coalesces into one draw call; flushes correctly when
> any other primitive changes texture/mode). Two results:
> - **Correctness: byte-identical.** dump+shasum oracle matches off-vs-on on `dpaint`, `interiors`,
>   `pixelperfect`, `lotfill` (the last two *interleave* pset with rectfill/line/circfill, so they
>   exercise the order-flush path). The technique is sound and reusable.
> - **Perf (native, Apple M1): no measurable gain at real-cart densities.** Drift-cancelling
>   interleaved A/B on `dpaint` (pset=30k/frame, ~pure pset runs): off ≈ 3.53ms vs on ≈ 3.58ms, with
>   per-run variance (±0.9ms) an order of magnitude larger than the difference. `interiors` +2%,
>   `pixelperfect` +1%, `lotfill` −2% — all inside noise. (A first single-shot fleet run *looked*
>   like −20–28%, but `raymarch`/`caustics` — which have **pset≈0** — "improved" the same amount,
>   proving that pass was lighter system load, a global offset, not a pset win. Always drift-cancel.)
>   The win only emerges far past any real cart: a synthetic `psetstress` probe at **128k psets/frame**
>   shows a small *repeatable* −6% (9.33 → 8.76ms), so the churn does start to matter at extreme
>   density — but 128k is 4× the heaviest real cart.
> - **Perf (WebGL, M1 / ANGLE-Metal): also no gain.** Tested for real — two emcc builds of `psetstress`
>   (`-DDE_BATCH_PSET_DEFAULT=0/1`) driven through the *system Chrome, headful* (real GPU, renderer =
>   "ANGLE Metal Renderer: Apple M1") via puppeteer-core, frame time read from a `requestAnimationFrame`
>   interval timer (the file profiler is compiled out on web). Reusable recipe (scripts + the gotchas):
>   [`../guides/debug-harness.md`](../guides/debug-harness.md) → "Web perf A/B". At 768k psets/frame (well past the
>   vsync cap, ~20fps): **off median 50ms vs on median 50ms = 0%** (reproduced). Batching gave a
>   slightly *tighter tail* (p90 50.9 vs 66.6ms) but identical throughput.
>
> **What it proves:** on this hardware — native *and* WebGL — the `pset` cost is the **per-vertex CPU
> emission** (4× `rlVertex2f`/pixel, *identical* in both paths), **not** the draw-call/texture-state
> churn batching removes. Crucially, rlgl *already* coalesces those vertices into its 8192-element
> batch buffer regardless of the per-pixel `rlSetTexture` toggle (768k psets in 50ms = ~thousands of
> pixels per real GPU draw, not one draw each), so the "60,000 GPU handshakes" story doesn't hold on
> ANGLE-Metal — and the WebGL-is-different hypothesis was **falsified** for M1. So **there is no cheap
> off-ramp here**: only the software canvas (which replaces those 4 `rlVertex2f`/pixel with a single
> `cbuf` store) can move `pset` time, which *raises* the bar for the rewrite rather than dodging it.
> Door still open only for **weak/old GPU drivers** (Intel iGPU, low-end Android, Pi-class) where
> small-draw submission is genuinely expensive — untested; the `DE_BATCH_PSET` flag + the
> `psetstress` cart (`-DSTRESS_PASSES`, `-DDE_BATCH_PSET_DEFAULT`) stay in as the committed reproducer
> for that day.

## Build runbook — the phased order, if we bite the bullet

The forks are decided (RGBA buffer on desktop, Fork-2/C, defer the blits), so this is the *build*
order, not a redesign. Every phase passes the same validation spine — the **dump+shasum byte-identical
oracle** (`play.js --dump` + `shasum`), the `raster_test` cart, `profile-fleet.js` for the A/B, and
the **web-perf flow** ([`../guides/debug-harness.md`](../guides/debug-harness.md) → "Web perf A/B").
Build it behind `DE_SOFTWARE_CANVAS` (off by default), reusing the `DE_BATCH_PSET` flag pattern.

**Phase 0 — Mechanism (½ day).** In the frame loop (`studio.c` ~`BeginTextureMode(canvas)`), branch on
`sw_canvas_active`: clear `cbuf`, run `draw()`, then `UpdateTexture(canvas.texture, cbuf)` → the
existing `DrawTexturePro` scale-up (untouched — crisp scaling stays GPU). Wire `cls` only.
*Gate:* a `cls`-only cart is byte-identical on/off.

**Phase 1 — v0: the headline win on the narrow set (~2–3 days) ⟵ GO/NO-GO.** This is the detailed
[Minimal slice (v0)](#minimal-slice-v0--the-smallest-thing-that-shows-a-real-validatable-result)
above: route `pset`/`pset_rgb`/`rectfill`/`rect`/fills to `cbuf` (disable the `DrawRectangle` span
fast-path so fills fall through `plot_pat → pset → cbuf`); **defer HUD `print`/`line` as a GPU overlay
replayed after the upload** (byte-identical, no port); `spr`/`tritex`/`camera_ex` warn-and-skip.
Targets: `caustics`/`shadelab`/`raymarch`/`dpaint`/`pixelperfect`/`interiors`/`lotfill`.
*Gates:* byte-identical on all targets **+** fleet A/B drops the pset carts hard **+** web A/B.
*Kill criterion:* if v0 does **not** drop the pset carts, stop — the thesis is wrong, and we've spent
days not the rewrite. (Unlike the batching probe, v0 eliminates the per-vertex `rlVertex2f` emission
that *is* the cost, so this is the honest test of the thesis.)

**Phase 2 — v1: port the blits (~1 week, the bulk).** `spr`/`print`(all `FONT_*`)/`tritex` → CPU
blits, retiring the deferred-overlay hack. **New dependency:** keep the spritesheet + font atlases as
CPU `Image`s in RAM (don't `UnloadImage` after upload) and make the sprite editor's live edits update
the CPU copy. `tritex` is the one real new rasterizer (affine scanline, textbook).
*Gate:* `build-all` compile sweep, then byte-identical on a broad set incl. 3D/`tritex` carts; fleet
A/B neutral-or-better everywhere. High blast radius — the oracle is the net.

**Phase 3 — `camera_ex` fallback wiring.** You only learn a cart uses `camera_ex` mid-`draw()`.
Resolution: on first non-identity `camera_ex`, set a **sticky per-cart flag** → that cart runs today's
GPU path for the rest of the session (one transitional frame). *Optional, only if a rotating-**fill**-
bound cart actually surfaces:* the inverse-mapping fill path (Fork 2 note) instead of GPU fallback —
defer until measured demand.
*Gate:* a zoom+rotation cart renders identically to today; a translation-camera cart stays on the
fast software path.

**Phase 4 — default + free wins.** Flip software-canvas to default for non-`camera_ex` frames
(`DE_SOFTWARE_CANVAS=off` escape hatch for one cycle); fold the span-fill fast paths into the `cbuf`
writers; cash the free wins (`pget`/`pget_rgb` → array read, **enables `pget` on web**; `fade`/blend →
read-modify-write; determinism → wire into the `--det` harness for replays/ghosts); write the
`decisions/` ADR.

**Shape of the bet:** Phase 0–1 is cheap (~3–4 days) and decisive on the exact carts. Phases 2–4 are
the real cost (~1.5–2 weeks) and start **only** on a trigger beyond "Phase 1 worked" — a cart actually
pset-bound below 60fps, or a committed web/low-end target. Feasibility ≠ need.

### Low-end / small-memory targets (Pi-class) — the options to keep, not lose

The whole calculus changes on weak hardware, and these are the levers that make a software canvas not
just viable but *mandatory* there (GPU immediate-mode is 2–5fps on a Pi Zero). None are needed on M1;
all are real if low-end ever becomes a target — captured here so they're not lost:

- **`uint8` paletted buffer = "smaller memory passing."** The Fork-1 reopener: 64KB fits a small L2
  (256KB RGBA spills), and the per-frame upload is **4× smaller** (R8 vs RGBA) — 15–25% of a Pi Zero
  frame, not <1%. This is the low-end build profile; `pset_rgb` either goes through a parallel
  RGBA plane (only when used) or isn't offered on that profile. Free bonus the index buffer brings
  back: full-screen **palette cycling** (animate the LUT — water/fire) that RGBA forfeits.
- **Fixed-point, never float, in the inner loops.** The ARMv6 FPU is slow; the camera/inverse-mapping
  math (and any per-pixel step) should be integer fixed-point on that profile.
- **Overdraw is the bandwidth ceiling.** memset/blend bandwidth, not math, is the Pi limit — skip
  `cls` when the background is opaque, minimise full-screen blend layers.
- **SIMD as a later lever** — NEON (Pi 2+, and M1) can do `cls`/blits 4–8× faster; the Pi Zero (ARMv6)
  has none, so keep inner loops tight and branch-free there. Something GPU immediate-mode could never
  exploit.
- **Triple-buffer the `cbuf`** to dodge a CPU/GPU contention stall on the upload (matters once the
  bus is the bottleneck).

All untested — the committed web-perf flow plus a Pi test rig would validate. The `uint8`-vs-RGBA
choice is the one that must be made at Phase 0 if a low-end profile is in scope (it's a buffer-format
fork, awkward to retrofit), so decide that *before* Phase 0 if Pi is a real target.

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

But keep the perspective honest: **this buys headroom, not relief from a present fire** — most
carts already hit 60fps, and the one rotating-*and*-pset-bound cart the rewrite's hardest part would
serve may not even exist in the fleet (`sloop`, the one off-shape profile, is draw-call-bound, not
`pset`-bound). The cheap off-ramp — draw-call batching (`DE_BATCH_PSET`) — **was tried and measured
null on Apple Silicon** ("Cheaper alternatives" → Measured 2026-06): `pset` cost is per-vertex CPU
emission, which batching doesn't touch. So on *this* hardware there's no shortcut: the software
canvas is the only thing that moves `pset` time, and it's a draw-layer rewrite for headroom. That's
a *strong reason to wait* until either (a) a cart is actually `pset`-bound below 60fps, or (b) web/
low-end becomes a target (where the batching probe should be re-run first — it may pay there even
though it didn't here).

When the canvas *is* picked up: prototype Fork-2/C behind `DE_SOFTWARE_CANVAS`, choose option 2
(narrow per-pixel opt-in) to prove the mechanism + the `dpaint`/CPU-shader win cheaply, A/B on the
fleet, and only commit to option 1 (porting `spr`/`print`/`tritex`) if those numbers land the way
the survey predicts. Earn the rewrite with data at each step — same discipline as the rest of the
ledger.

## See also
- [../guides/engine-optimization.md](../guides/engine-optimization.md) — the fleet survey that
  pointed here + the ledger of the fill optimizations this would subsume.
- [cpu-shaders.md](cpu-shaders.md) #5 — the parked "true offscreen buffer"; a software canvas is a
  superset (the whole canvas is in RAM, multipass becomes trivial).
- [rasterization-consistency.md](rasterization-consistency.md) — the CPU per-pixel coverage
  invariant the software rasterizers already satisfy; the rotated-camera seam fork 2 must respect.
