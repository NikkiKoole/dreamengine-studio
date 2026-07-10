# 0031 — Blend tables ship before the palette decision (build them from `palette[]`, don't bake)
Date: 2026-07-10 · Status: accepted

## Context
dreamengine has no way to draw something that **mixes with what's already on screen** —
`pal()` remaps colors and `fillp` dithers holes, but neither reads the destination pixel. So
every glow, translucent pane, drop shadow, and fog band in the corpus is faked with dither or
palette banding, and effects that genuinely depend on the background (additive light, soft
smoke, fading particle trails) are simply impossible. This is [STATUS open item 18](../STATUS.md)
and the substantive capability gap flagged by the Picotron comparison.

The mechanism is the standard 8-bit-indexed technique (Doom `TRANMAP`, Allegro `color_map`): a
32×32 table of palette indices, `blend[src][dst] = result`, where the RGB mixing happens **once,
at table-build time** (mix the two palette entries, snap to the nearest of the 32). At draw time
it's a pure lookup and the framebuffer still holds nothing but the 32 indices — so `pget`,
`colorkey`, and the fixed-palette identity all keep working unchanged. The full design,
prior art, and the validated cart prototype (`blendlab.c`) live in
[`blend-tables.md`](../design/blend-tables.md).

The prototype's sequencing note said the engine ADR *should follow* the palette decision
(STATUS #18 / [`palette-and-color.md`](../design/palette-and-color.md): ours vs. a lifted PICO-8
palette, fixed vs. settable), because a blend table is a pure function of the palette's RGB
values. Read literally that reads like a blocker. **It isn't** — and pinning down why is the
whole point of this record.

## Decision
Build the blend-table engine feature **now, before the palette decision is made**. The coupling
is a pure-data dependency, not a mechanism one, and it is neutralized by a single rule:

> **Build the tables at engine startup from the live `palette[]` array — never bake table
> constants into the source, and never bake the palette's RGB values into the table builder.**

With that rule, the palette decision cannot invalidate this work:

- **Palette stays fixed** → tables are correct as built.
- **The default palette changes** (ours ↔ lifted PICO-8) → the tables rebuild from the new
  array on the next startup. Zero code change, zero API change, no shipped data to re-bless.
- **The palette becomes settable at runtime** → add one hook that rebuilds the tables when the
  palette swaps. A small, known addition — not a redesign.

The prototype already validated the build is data-free and instant: ~32k nearest-color searches
built in `init()` with no perceptible cost (`blend-tables.md` finding #4). The nearest-match
uses a luma-weighted distance (`2·Δr² + 4·Δg² + 3·Δb²`); plain Euclidean picked perceptually-off
neighbors for warm mixes (finding #4).

API surface (follows the `pal`/`fillp` sticky-scope shape blessed by
[decision 0010](0010-fade-is-immediate-mode.md)):

```c
void blend(int mode);     // all draws now mix with the screen: BLEND_AVG / BLEND_ADD / BLEND_MUL / BLEND_SUB
void blend_reset(void);   // back to plain overwrite (the default)
```

`BLEND_AVG` (glass/water/shadow), `BLEND_ADD` (glow/light/laser), `BLEND_MUL` (fog/darken/tint)
cover the demand list from `galerijflat.md` and `geometry-helpers.md` (finding #3). **`BLEND_SUB`
(carve light toward black — cold shadow, scorch) was added as a fourth preset**, consciously
overriding finding #3's "nothing needs a fourth": it is the symmetric complement to `BLEND_ADD`
(the one mode `BLEND_MUL` can't reach — MUL tints toward the product, SUB drains uniformly toward
black) and costs one more 4KB table. A `blend_table()` escape hatch stays unbuilt until a cart
asks for it (decision 0006 instinct).

## Why proceeding early is safe *and* worthwhile
- **The real engineering is palette-independent.** The hard part is not the tables — it's
  reading the in-progress canvas on a GPU renderer, where `pset` *is* `DrawPixel` and nothing
  reads back the framebuffer. The chosen design (below) is the same shader machinery
  [decision 0007](0007-pal-recolors-sprites.md) already shipped for `pal()`, and it does not
  touch the palette question at all. Waiting on the palette decision delays none of the work
  that's actually risky.
- **`pget()` is the wrong dst source — this is why the feature needs the engine.** The prototype
  proved it (finding #5): `pget()` reads a *last-frame* snapshot, which already contains the
  blended shapes, so a stationary additive glow feeds back and blooms to white over ~a second.
  A correct blend must read what's been drawn **so far this frame** — a capability only the
  engine can provide, which is exactly why this can't stay a cart-space trick.
- **It pays off far beyond particles.** Glow, soft light falloff, and translucent panes are
  already parked and waiting on this (`galerijflat.md`); particles are just the newest caller.

## The implementation crux — reading dst on a GPU renderer
Everything draws through the GPU into a `RenderTexture`. The chosen path (option 1 in
`blend-tables.md`, the lane decision 0007 opened):

- **Shader + canvas snapshot per blend scope.** The 32×32 table uploads as a tiny LUT texture.
  The fragment shader recovers the src index the way the `pal()` shader already does (exact match
  against the 32 exact palette RGBs), samples dst from a **copy of the canvas taken when
  `blend()` activates** (and on each re-activation), and outputs the looked-up palette RGB.
- Cost is one canvas copy per blend *scope*, not per call — cheap when carts blend one batch
  after drawing the scene (the natural usage).
- **Snapshot semantics:** blended shapes within one scope see the snapshot, not each other
  (so two overlapping additive glows don't double-bright). The prototype says this reads fine;
  re-snapshotting per primitive is the correct-but-slower dial if a caller ever needs it.
- Ships in GLSL 330 (desktop) + GLSL ES 100 (web), like the `pal()` shader.

## What we did NOT do / decide here
- **Did NOT decide the palette question.** This ADR deliberately decouples from it; STATUS #18 /
  `palette-and-color.md` stays open. The commitment here is only "blend doesn't wait for it."
- **No `blend_table()` escape hatch in v1** — presets first; add it when a cart wants
  "fire over water = steam".
- **No per-call blend variants** — sticky `blend(mode)` matches `pal`/`fillp`, keeping the
  surface to two functions + three constants.
- **No opacity dial in v1** (25/50/75 as three AVG tables). `fillp`-composition already yields
  intermediate opacity (pattern holes skip, pattern 1-bits blend); revisit only if that's not
  enough.

## Status of the build (2026-07-10) — BOTH paths landed + verified
**Software-canvas path** — the canonical 2D renderer ([decision 0024](0024-software-canvas-is-canonical-for-2d.md)),
what ships on iOS/web (both force the software canvas on). Reads dst directly from its own
framebuffer (no snapshot) and mixes in `sw_plot1` (points/lines/sprites) and `sw_fillrect`
(rect/circ/oval/poly spans); gated on `blend_mode != NONE` so it's byte-identical when off.
Tables build from `base_palette[]` in `load_palette()` (the ADR's dynamic-build rule).

**GPU desktop path** (`SW_CANVAS_DEFAULT=0`, the editor ▶ run default) — the option-1
shader+snapshot: `blend()` snapshots the canvas-so-far into a dedicated `blend_snap` RT (the
`zoom_rect` unwind/copy/restore, `-de_sh` flip to keep orientation), then `BeginShaderMode` runs
`BLEND_FS` (GLSL 330 + an ES-100 flavor) — it mixes `src` (draw colour) with the snapshot `dst`,
snaps to nearest base palette (same 2·4·3 luma metric as the software path), outputs the current
palette colour so `pal()` composes. Left-open scopes are closed after `cart_draw()`.

All five modes verified by eye on the `blendfx` demo, on **both** paths (software via
`DE_SOFTWARE_CANVAS=on`, GPU via the normal bake): ADD brightens toward white over a bright band,
MUL vanishes over black / darkens the band warm, SUB carves toward black with cool residue.

**Two implementation gotchas worth recording** (both cost a debugging pass):
1. `SetShaderValueTexture` must be called *after* `BeginShaderMode` — before it, the sampler bind
   is lost and unit 1 reads the "zero texture" (black dst).
2. `cur_pal_rgb` isn't populated until the first `pal()` call, so the shader's output palette must
   be built from the live `palette[]` in `blend_gpu_begin`, not read from `cur_pal_rgb`.

**Snapshot vs live-dst — an intended divergence.** The GPU path reads a per-scope *snapshot*, so
overlapping blended draws in one scope don't accumulate with each other; the software path reads
the *live* dst, so they do (dense ADD particles stack toward white). Measured ~580px/64000 (<1%)
on `blendfx`, concentrated in the overlapping-particle column. This is the option-1 caveat, not a
bug. Consequence: **blend is deliberately NOT in `drawall.c`** — `canvas-diff` asserts GPU==software
pixel-identity, which blend intentionally breaks. A future blend oracle would compare each path to
its own expected output, not to each other. (Making the GPU path accumulate like software needs a
per-primitive re-snapshot — too slow; parked as a dial.)

## Consequences
- New API in the four standard places (`studio.h`, `studio.c`, `studioDocs.js`, `shell.js`).
  Not in `drawall.c` — see the snapshot-vs-live divergence above.
- Both fill paths carry blend per [decision 0011](0011-two-fill-pattern-paths-must-stay-in-sync.md):
  software `sw_fillrect`/`sw_plot1`, GPU via the shader wrapping all draws.
- `pal()` composes: both paths output the current palette colour for the result index; the software
  path outputs `palette[result]`, the GPU shader outputs `curPal[i]`. `colorkey`/transparent texels
  are skipped (no blend), as now.
- `raylib_compat.h/.c` gain a `SetShaderValueTexture` stub so the no-raylib (iOS) build compiles;
  GPU blend never runs there (software canvas only). Verified via `build-nr.sh blendfx`.
- If the palette ever becomes settable, one rebuild-on-swap hook is owed (software `blend_lut`
  rebuild; the GPU path already reads live `palette[]`).
- `blend-tables.md` STATUS: EXPLORING → BUILT. Demo cart `blendfx` (teaches `blend-modes`) is the
  worked example; `teaches-vocab.js` gains `blend-modes`.
