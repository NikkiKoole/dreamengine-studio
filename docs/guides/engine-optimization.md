# Engine optimization — the safe loop, and the ledger

How we make the engine faster **without changing what it draws or plays**. The whole
discipline is one rule:

> **An optimization must prove it's invisible.** Same pixels (byte-identical), same
> audio, same behaviour — *then* measure that it's faster. If you can't prove "same," it's
> not an optimization, it's a change.

This guide is the repeatable loop for that, the tooling that makes it cheap, and a
**ledger** of what we've done. Pair it with [profiler.md](profiler.md) (where the time
goes) and [debug-harness.md](debug-harness.md) (how to make a run deterministic).

---

## The loop

1. **Profile first — find the real hot path, don't guess.** `⏱ profile` (editor) gives
   the §2 call-graph (cost) + §3 draw counts (frequency). See [profiler.md](profiler.md).
   Optimize what's actually hot; a 90%-of-frame function is the only thing worth touching.
2. **Build a stress cart that pins it.** A deterministic cart that hammers the hot path so
   the profile is unambiguous *and* the output is byte-reproducible frame-to-frame. This is
   your correctness oracle (next step) and your profiling target. `polystress` (the polygon
   rasterizer) and `trifill_stress` (the off-screen scan-clamp) are the templates — both
   registered `tech-demo` carts, both pure functions of a frame counter (no time, no random).
3. **Keep the old code beside the new, switchable at runtime.** Don't delete the thing you're
   replacing — keep it as `*_legacy` behind a flag flipped by an **env var** (so any cart can
   be A/B'd headless or in the editor with no recompile). See the pattern below.
4. **Prove byte-identical.** Dump frames both ways and compare hashes:
   ```bash
   node tools/play.js <cart> script /dev/null --headless --frames 90 --dump /tmp/new --dump-every 15
   ENV=legacy … --dump /tmp/old …
   diff <(shasum /tmp/new/*.png|awk '{print $1}') <(shasum /tmp/old/*.png|awk '{print $1}')
   ```
   Identical = safe. A mismatch is a gift — it's a bug the oracle caught *before* it shipped.
   When one differs, **pixel-diff to the exact `(x,y)`** and reproduce it in a tiny standalone
   C program (matching the engine's `float` math); don't reason about rounding in your head
   (worked example below — two real bugs found this way).
5. **Run the engine's own regression guards.** Some invariants live in dedicated carts/tools:
   `raster_test` (fill==outline boundary, `mismatches:"0"`), `tune-check`, `level-check`,
   `fx-check`, `soak-check`, `web-audio-check`. Re-run the ones your change could touch
   (see CLAUDE.md "After touching …").
6. **A/B measure, same build.** Flip the flag and read `perf.json` (`workMsAvg`/`workMsMax`).
   Both runs must be the *same build* — comparing a `play.js` number to an editor `⏱` number
   is apples-to-oranges (different `-O` flags).
7. **Record it in the ledger** (bottom of this doc) with real before/after numbers.

---

## The keep-both A/B switch (the pattern)

Replacing a hot function? Keep the original verbatim as `<name>_legacy`, gate on a global
flipped by an env var at startup. Concretely (from `runtime/studio.c`):

```c
static bool poly_fill_fast = true;          // up with the other globals

// in main(), before anything draws:
{ const char *pf = getenv("DE_POLY_FILL");
  if (pf && strcmp(pf, "legacy") == 0) poly_fill_fast = false; }

static void poly_fill_cov_legacy(...) { /* the exact old code, untouched */ }
static void poly_fill_cov(...) {
    if (!poly_fill_fast) { poly_fill_cov_legacy(...); return; }
    /* the new path */
}
```

Why env var, not a `#define` or a key:
- **No recompile** between A and B — the same binary does both, so the comparison can't be
  contaminated by a stray flag.
- **Inherits everywhere.** `play.js` spawns the native binary, the editor spawns it too — both
  inherit the parent env. Headless A/B is per-run (`DE_POLY_FILL=legacy node tools/play.js …`);
  the editor is session-wide (`DE_POLY_FILL=legacy make`).
- **Both functions stay compiled and referenced**, so neither bit-rots and there's no
  `-Wunused` noise.

Keep the legacy path until the new one has soaked across many carts; then delete it + the
flag in a dedicated commit. Mark it `TODO` so it doesn't become permanent.

**A/B any cart, headless:**
```bash
ab() { rm -f build/perf.json; env "$1" node tools/play.js "$2" script /dev/null --headless --frames 240 >/dev/null 2>&1;
       node -e 'const p=require("./build/perf.json");console.log(`${p.workMsAvg.toFixed(3)}ms avg / ${p.workMsMax.toFixed(3)}ms max`)'; }
ab "DE_POLY_FILL=legacy" roadlab   # old
ab "" roadlab                      # new
```

---

## Worked example #1 — the polygon rasterizer span fill (2026-06)

**The cost.** `polyfill`/`trifill`/`ngonfill`/`starfill` all fill in software via
`poly_fill_cov`: scan the bounding box, test every pixel with the even-odd `poly_inside`,
and `pset` (→ `DrawPixel`) each inside one. The profile of `polystress` (159 fills/frame):
**~68%** of cart-active CPU was the per-pixel *write* (`DrawPixel` → one immediate-mode
`rlVertex3f` *per pixel*, plus `rlSetTexture` churn), **~26%** the per-pixel `poly_inside`.

**The change.** Rewrote `poly_fill_cov` as a **scanline span fill**: per row, compute the
sorted edge crossings (instead of testing each pixel), and paint each solid span as **one
`DrawRectangle`** (instead of N `DrawPixel`). The inside *decision* stays 100% on the CPU at
pixel-centre `x+0.5` — only the *plotting* of an already-decided run is batched — so it does
**not** revive the rejected "let the GPU decide coverage" approach
([rasterization-consistency.md](../design/rasterization-consistency.md) direction 2; an
integer height-1 rect paints exactly the same pixels on desktop GL and web GL ES, no
diagonal-edge ambiguity). The fast path is gated on an **axis-aligned camera** (rotation 0,
zoom 1) and a **solid** fill; a rotated/zoomed camera or active `fillp()` dither falls back to
per-pixel `plot_pat` (still skipping per-pixel `poly_inside`, so still faster), preserving the
documented rotated-fill staircase and the dither lattice exactly.

**The two bugs the byte-identical oracle caught** (both invisible to the eye, fatal to the
"invisible" claim — found by pixel-diff → standalone-C repro, not by squinting):
- **Notch bleed.** A trim that used the global `poly_inside` to find span ends latched onto a
  pixel that was inside via the *adjacent* span across a sub-pixel gap, filling a 1px star-notch
  that should've stayed open. Fix: bound each span by *its own* two crossings, not the global
  predicate.
- **Half-open off-by-one.** `poly_inside`'s strict `<` makes the inside interval `[cL, cR)`
  (left-closed, right-open). The naive `floor(cL)+1` left bound differs from the correct
  `ceil(cL-0.5)` exactly when a crossing lands on a half-integer — 83 mismatched pixels across
  the test frames until fixed. Verified the half-open rule exhaustively (reference vs formula,
  every pixel, every frame → 0 mismatches).

**Result** (same-build A/B, `perf.json` via `play.js`):

| cart | legacy | fast | speedup | max frame |
|---|---|---|---|---|
| `roadlab` (all-solid road strips — the real target) | 4.95ms | **1.83ms** | **2.7×** | 13.0 → 3.4ms |
| `polystress` (half its fills are dithered → fallback) | 8.47ms | **5.18ms** | 1.6× | 19.6 → 6.6ms |

(Editor `⏱ profile` build, the other yardstick: `polystress` 9.0 → 5.4ms typical, 54% → 32%
of budget.) `roadlab` gets the full win because it's all solid fills; `polystress` under-sells
it on purpose (its big dithered polys take the per-pixel fallback). The **max-frame** drop is
the bigger story — the per-pixel path spiked, the span path is steady.

**Validation:** `polystress` byte-identical (legacy == fast == pre-change original);
`raster_test` 20/20 `mismatches:"0"`. Commit `8f201c5`.

**Follow-up — measured & PARKED (2026-06): dither-span batching (`draw_span_pat`).** The
idea: batch a dithered span into same-colour runs (one `DrawRectangle` each) instead of
per-pixel `plot_pat`. Measured first (polystress scene 0, six big dithered stars, one per
`FILL_*` pattern) and parked:
- The span fill **already halved the dither cost** by removing per-pixel `poly_inside`
  (1.75 → 0.91ms on the torture scene), even though dither still plots per-pixel. So the big
  win was already banked; `draw_span_pat` could only attack the residual ~0.9ms of per-pixel
  `DrawPixel`.
- That residual is **pattern-dependent** (avg run length → calls-vs-per-pixel): HLINES 2%,
  DOTS 16%, GRID/DIAG ~33% — but **CHECKER and VLINES are 1.0 (no win, slightly worse from
  rect overhead)**, and those are the most commonly used patterns. So it's a coin-flip with a
  regression risk on the common case.
- Verdict: not worth it now — sub-millisecond ceiling on a torture scene, real dither fills are
  small accents. If a real cart ever profiles a dither hotspot, ship a **conditional** version
  (precompute per `fillp()` whether the pattern's run-length pays; batch only then → zero
  regression). The run-length data lives in this section. (Routing through the existing
  `rectfill_pat` is the natural vehicle, but mind the session-14 dither-on-`plot_pat` invariant.)

---

## Fleet survey — where the engine spends time across carts (2026-06)

To pick engine-level targets (not per-cart fixes), profile *many* carts and aggregate the
draw-call composition — a primitive that's hot across 20+ carts is worth fixing once in
`studio.c`. Re-runnable: **`node tools/profile-fleet.js`** (headless §1 + §3 across a
graphics-heavy default set; pass cart names to scope it). It measures cost + call frequency,
**not** per-function time (no §2 — that needs the editor `⏱` / a `sample` attach), and counts
**under-state large software fills**, so a low-count/high-ms cart (e.g. `orbit`) is a "pull §2
on this one" flag, not a cheap one.

First run, 32 carts (calls/frame summed across the fleet):

| primitive | sum/frame | #carts | path |
|---|---|---|---|
| **`pset`** | **59,593** | 21 | software — `DrawPixel` → 1 `rlVertex3f` **per pixel** |
| `rectfill` | 24,214 | 29 | GPU (`DrawRectanglePro`, 1 quad each) |
| `line` | 3,892 | 23 | GPU |
| **`circfill`** | 1,028 | 20 | **software per-pixel (disc)** |
| `polyfill`+`trifill` | 966 | — | software — *already span-optimized* ✓ |
| `ovalfill` | 117 | 9 | software per-pixel (disc) |

Slowest carts: `galerijflat` 12.8ms (rectfill 1194 + pset 364), `orbit` 11.0ms (few large
discs — count hides the cost; **now 1.3ms** after the zoom-gate fix below), `lotfill` 4.7ms
(pset 9271 + rectfill 7664), `dpaint` 3.6ms
(**pset 30,535**), `flyover` 3.4ms (rectfill 11,330), `interiors` 3.0ms (pset 18,036).

**Two engine targets fall out of this, in priority order:**

1. **`pset`/`DrawPixel` is the dominant cost by 2.5×** — 59.6k/frame across 21 carts, one
   immediate-mode GPU vertex *per pixel* (the polyfill villain, but in the raw `pset` path carts
   hit directly: `dpaint` 30k, `interiors` 18k, `lotfill` 9k). Raw `pset(x,y)` calls are
   arbitrary points, so they can't be coalesced into spans like polyfill. The engine fix that
   kills it fleet-wide is a **CPU framebuffer**: route `pset`/`DrawPixel` into a `uint32` array,
   `UpdateTexture` once per frame → `rlVertex3f` gone from *every* per-pixel path (incl. the
   CPU-shader carts). **This is an architecture change** (hybrid GPU+CPU compositing, camera,
   clip) — wants a **design doc before code**, not a patch. Highest leverage the data supports.
   → **Design doc written:** [../design/software-canvas.md](../design/software-canvas.md)
   (rasterize the whole 64k-px frame on the CPU, `UpdateTexture` once/frame; forks + staged plan).
2. **`circfill`/`ovalfill` span fill** — SHIPPED (`40c38d5`, see ledger). Disc/ellipse rows are
   one contiguous span → one `DrawRectangle`; `disc_inside` was only 3.2% of `circfill`, so the
   win is almost entirely the per-pixel-write batching (+ a scan clamp the disc path lacked).
   `discstress` 8.4×; real carts that are *circfill-bound* (oersoep 2.5×, pinball 2.2×) win,
   others don't.
   - **Then extended to allow ZOOM (`00dd3f6`):** the original gate was `cam.zoom==1 &&
     rotation==0`, which forced every fill under a *zoomed* `camera_ex` onto the per-pixel
     fallback. But a rotation-0 camera is axis-aligned affine, so the span tiling is byte-safe
     under zoom too — only rotation breaks it. Relaxed the gate to `rotation==0`; validated
     byte-identical with `discstress`'s ZOOM scene. **This is what finally fixed `orbit`.**
   - **The `orbit` saga (I was wrong twice, here's the truth):** `orbit`'s 11ms IS discs — the big
     planet (`R_PLANET=120`, ~5 circfills, §2 = 74.7% in `draw_planet › circfill › pset →
     rlVertex3f`). The §3 count (7 circfill) is *low but huge-area*, which fooled a count-only read.
     And it didn't benefit from the first span-fill commit because `orbit` draws under a *zoomed*
     `camera_ex` → it was stuck on the per-pixel fallback by the `zoom==1` gate. The zoom relaxation
     above batches it: **`orbit` 11.46 → 1.28ms (9×)**. Lesson: a low draw-COUNT can still be the
     hot spot (large fills); confirm with §2, and check the camera before blaming the primitive.

`rectfill` (24k/frame, GPU) is high *volume* but cheap *per call*; batching it is a bigger lift
for a less certain gain — lower priority.

### Triangles — measured & PARKED (seam left)
`trifill` is `poly_fill_cov(n=3)`, so it already gets the span fill (one `DrawRectangle`/row).
A dedicated triangle rasterizer (sort verts by y, walk two edges with incremental `x += slope`,
no per-row division) was considered and parked: `tristress` BIG (8 large) 0.33ms, MANY (421 tiny)
0.97ms, and no real cart is trifill-bound. The win it'd chase — per-row division + per-call
clamp cost — is sub-millisecond even in torture. A documented **seam** is left in `trifill`
(`studio.c`) and the rig is `tools/carts/tristress.c` (BIG/MANY scenes).

### Per-frame clamp-box cache — SHIPPED (`13fdeca`)
`poly_clamp_scan` called `GetScreenToWorld2D` **4× per fill call** (a camera-matrix inverse ×4),
yet the visible-region box is **constant for the whole frame** unless the cart calls `camera()`.
Now cached, keyed on a camera+viewport signature: a hit skips the inverts, any camera/viewport
change misses and recomputes (byte-identical by construction — the signature captures every
input). Benefits `polyfill`/`trifill`/`circfill`/`ovalfill` together. Rig: `clampstress`
(STATIC + a PAN scene that proves invalidation). Pre-flagged in
[../design/rasterization-consistency.md](../design/rasterization-consistency.md).
**Honest result:** big on the many-tiny-fills *tail* (`clampstress` 1508 fills 2.03→1.47ms, 28%),
modest on real fill-heavy carts (qbert 9%, oersoep 10%), **neutral** on large-fill carts
(polystress unchanged — there fill *area* dominates, not the per-call clamp). Cheap (one signature
compare/call) and never regresses, so it ships; it's a tail/many-small-fills win, not a headline.

---

## Ledger

| date | area | what | proof | result | flag / commit |
|---|---|---|---|---|---|
| 2026-06-02 | software polys | clamp scan box to on-screen region (off-screen bbox no longer scanned) | `raster_test` 0 | `trifill_stress` 46.7→2.7ms | (in rasterization-consistency.md) |
| 2026-06 | software polys | `poly_fill_cov` scanline span fill (solid → one `DrawRectangle`) | `polystress` byte-identical, `raster_test` 0 | `roadlab` 2.7×, `polystress` 1.6× | `DE_POLY_FILL=legacy` · `8f201c5` |
| 2026-06 | software discs | `circfill`/`ovalfill` scanline span fill + scan clamp | `discstress` byte-identical, `raster_test` 0 | `discstress` 8.4×; oersoep 2.5×, pinball 2.2× (circfill-bound only) | `DE_DISC_FILL=legacy` · `40c38d5` |
| 2026-06 | software discs | relax span gate `zoom==1 && rot==0` → `rot==0` (zoom is byte-safe; only rotation breaks span tiling) | `discstress` ZOOM scene byte-identical, `raster_test` 0 | **`orbit` 11.46→1.28ms (9×)**, discstress ZOOM 2.46→0.52ms | `DE_DISC_FILL=legacy` · `00dd3f6` |
| 2026-06 | all software fills | per-frame clamp-box cache (4 matrix inverts → per camera-change) | `clampstress` STATIC+PAN byte-identical, `raster_test` 0 | `clampstress` 28%; qbert 9%, oersoep 10%; neutral on large-fill carts | `DE_CLAMP_CACHE=off` · `13fdeca` |

---

## See also
- [profiler.md](profiler.md) — `⏱ profile`: hot functions, call paths, budget, draw counts.
- [debug-harness.md](debug-harness.md) — deterministic `--dump`/`--trace`, live `perf.json` snapshot.
- [../design/rasterization-consistency.md](../design/rasterization-consistency.md) — why the fill
  family is CPU-per-pixel (the invariant any rasterizer optimization must preserve).
