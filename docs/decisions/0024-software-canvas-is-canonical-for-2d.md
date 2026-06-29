# 0024 — Software canvas is canonical for 2D; tritex/3D is GPU-only
Date: 2026-06-29 · Status: accepted

## Context
Porting the real engine to iOS (Phase 2, [`ios-plan.md`](../design/ios-plan.md)) hung on one
load-bearing question, laid out in [`engine-portability.md`](../design/engine-portability.md):
**is the canonical renderer the software canvas (the engine hands the host a finished CPU
framebuffer, `sw_cbuf`, and the host blits one texture) or the GPU (the host runs Raylib's GL
through ANGLE)?** The two are the *same* decision as "stay ANGLE-free on iOS" — a CPU framebuffer
means no GL stack on the phone; a GPU-canonical path drags the large, self-described
"experimental" ANGLE/`raylib-iOS` dependency back in.

The engine already forks `if (sw_canvas_active) {...} else {<GPU>}` per primitive, so both
renderers exist behind the `platform.h` seam ("two renderers, one seam"). The open question was
purely **performance**: can the CPU rasterizer hold 60fps on real phone silicon for the carts we'd
ship? We committed to deciding by measurement, not opinion — on **both** a desktop and a physical
device.

## Decision
**The software canvas is the canonical renderer for 2D carts on portable targets (iOS first).** iOS
ships ANGLE-free: the engine rasterizes into `sw_cbuf` and the host blits it (proven in Phase 2 —
omnichord/heroes pixel-identical, tb303 byte-identical, all on the SW path). **`tritex`
(perspective-correct textured triangles) and the 3D carts built on it stay GPU-only and are off the
initial iOS target list** — they are the explicit exception, not part of the portable 2D promise.

The GPU renderer stays first-class behind the seam (it is Raylib on desktop today, and the seat for
a future Metal backend) — this decision is about which renderer is *canonical/portable*, not about
deleting the GPU path.

## Why — the measurement (both halves agree)
60fps budget = **16.67ms/frame**. `engine` = `de_frame()` CPU per frame.

Desktop (real engine headless, `-O2`, fast Mac CPU):

| cart | type | GPU | SW canvas |
|------|------|-----|-----------|
| omnichord | light | 0.26ms | 0.58ms |
| neonrain | fill-heavy | 0.12ms | 0.38ms |
| flank | sprite-heavy | 0.52ms | 0.36ms |
| podracer | **tritex/3D** | 0.22ms | **19.32ms** |

Device — the real iOS app on an **iPhone SE 2nd-gen (A13, iOS 15.4.1)**, software canvas, via
`ios/measure-device.sh` (a Debug `-O0` build, so engine numbers are *pessimistic* — Release runs
several× faster):

| cart | type | engine avg | blit avg | fps |
|------|------|-----------|----------|-----|
| omnichord | light | ~5.3ms | ~0.6ms | **59–60** |
| neonrain | fill-heavy | ~4.9ms | ~0.65ms | **59–60** |
| flank | sprite-heavy | ~4.9ms | ~0.6ms | **59–60** |
| podracer | **tritex/3D** | ~89ms | ~0.2ms | ~10 |

- **2D holds a locked 59–60fps on the phone** — engine+blit ≈ 5.6ms, ~⅓ of the budget, *unoptimized*.
  The desktop "2D is a non-issue on CPU" result survived real phone silicon with room to spare.
- **`tritex` is the lone killer, on both** — desktop SW 19.3ms × ~4.6 (phone vs the fast Mac) ≈ the
  measured ~89ms. Optimization won't bring it into budget; it genuinely wants a GPU.
- The blit (flip + CGImage upload) is cheap everywhere (~0.6ms) — rasterization is the cost, not the
  CPU→screen handoff.
- **Correctness gate passed earlier:** `canvas-diff omnichord` = 0px vs GPU every frame, so the SW
  output is trusted, not just fast.

## Consequences
- iOS needs **no ANGLE, no `raylib-iOS` fork, no GL stack** — the biggest porting risk is gone, and
  it fits the own-the-stack ethos.
- **GPU-only feature parity** — audited after this decision (see
  [`engine-portability.md`](../design/engine-portability.md) §"GPU-only feature parity — audited"), and
  the gap is narrower than this ADR first assumed: **`pal()` already has full software parity**
  (`sw_recolor`, `canvas-diff` 0px), the **scale filter is a non-issue** (no cart uses it; the iOS host
  scales), and **camera rotation now works** on the software canvas too (an offscreen world layer
  rotate-composited about the screen centre — a 25° probe is 0.04% off the GPU). The only remaining
  portable-2D gap is **`smooth_zoom`**'s offscreen-RT antialiasing (degrades to plain zoom, 1 cart).
  `tritex`/3D stays off-list by perf, per this ADR.
- A pile of GPU-vs-SW A/B scaffolding can eventually be pruned for the portable build (see
  `engine-portability.md` §"Settle and prune the A/B flags").
- **Not forever-exclusive:** the seam keeps the GPU path alive, so a later Metal backend can take 3D
  (and heavy carts) on iOS without reopening this decision — it would *add* a renderer, not replace
  the canonical 2D one.

## Alternatives considered
- **GPU canonical (ANGLE on iOS).** Most faithful to desktop, least engine surgery — but reintroduces
  the large experimental dependency this whole portable effort was structured to avoid, and the 2D
  measurement shows it buys nothing for the carts we'd actually ship.
- **Optimize the SW triangle rasterizer to rescue `tritex`.** Possible, but ~89ms → <16ms on a phone
  CPU is a >5× ask for a perspective-correct textured rasterizer; a GPU is the right tool. Deferred,
  not pursued, for the initial target.
