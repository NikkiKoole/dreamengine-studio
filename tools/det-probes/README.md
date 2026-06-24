# det-probes — cross-compile determinism oracles for the software canvas

Standalone reproducers that answer one question for [`docs/design/software-canvas.md`](../../docs/design/software-canvas.md)'s
**goal B** ("the whole cart draw layer runs on a software renderer, with no GPU path picking pixels"):

> Does the float math in the software rasterizers pick the **bit-identical** set of pixels on
> arm64, x86-64, and wasm? — the precondition for replays / ghosts / lockstep on any device.

Each `.c` is **self-contained** (no `studio.h`, no raylib): it lifts a rasterizer verbatim,
routes `pset` into a flat `320×200` byte buffer, and hashes that buffer in-C (FNV-1a) so the
output is plain text that compares identically native-vs-wasm. The whole point is that the *same
source*, compiled three ways, prints the *same hash*.

```
bash tools/det-probes/run.sh      # build+run all three on arm64 / x86-64 (Rosetta) / wasm; exit 0 = all match
```

| probe | rasterizer under test | the bar it proves |
|---|---|---|
| `detstress.c` | `sline` + `sfill` (from `tools/carts/linesym.c`) | dense slope fan + rotating polys → float path is device-stable |
| `stritex.c`   | software `tritex` (edge-function + top-left rule) | textured triangle is device-stable **and** two tris tile a quad with `overlap=0 gap=0` |
| `rotstroke.c` | rotated outline via "rotate endpoints → `sline`" | a square outline stays **1 connected component at all 360°** (no gap), device-stable |
| `rotfill.c`   | rotated fill via **inverse** mapping vs forward (Fork 2) | inverse is **gap-free at all 360°** (1 component, stable area), device-stable; forward leaves up to **1137 holes** (~19%) |
| `rotline.c`   | crisp rotated 1px line via `sline` (rotated strokes) | **uniform-width (excess 0) + connected (1 comp) at all 360°**, device-stable; only residual is per-degree *shimmer* (churn up to ~line length), an aesthetic of crisp/no-AA rotation |
| `rotspr.c`    | rotated *sprite*: nearest vs supersample vs **RotSprite** (EPX×3 upscale→rotate→mode-downscale) | footprint gap-free + device-stable. **RotSprite keeps thin LINES nearly intact** (frame ≤2 pieces vs nearest's 7; diagonal continuous) — the real win. A truly-lone 1px dot is a resolution-floor edge case (nearest 308 > super 252 > RotSprite 156 /360): point-sampling catches a speck best, vote-based methods drop it. |
| `textrot.c`   | rotated *text* (1px-stroke "E") nearest vs supersample vs RotSprite | text = glyph blits, so it reduces to rotated-sprite algorithms. Surfaced the **trig-determinism caveat** (below); at 8px all methods are rough (RotSprite's edge needs ≥16px) — the practical fix is bigger text (`print_rot_scaled`) |

## Findings (2026-06-24, Apple M1 host)

All seven are **bit-identical** across arm64 / x86-64 / wasm, and FMA-contraction-immune
(`-ffp-contract=on/fast/off` all agree — the divides leave no contractable surface). So goal-B
determinism is reachable with the **existing float code** — no fixed-point rewrite forced.

**Trig-determinism caveat (found by `textrot`):** raw libm `cosf`/`sinf` are **not** bit-identical
across arm64 / x86-64 / wasm (they differ ~1 ULP). For *integer-endpoint* rotation (`rotline`,
`rotstroke`, `detstress` — trig → `lroundf` → int) that washes out. But for **per-pixel** rotation
(`rotfill`, `rotspr`, `textrot` — trig feeds a per-pixel `floor`/compare) a 1-ULP difference flips a
pixel and breaks determinism — invisibly at large scales (those two passed by luck at first), visibly
on an 8px glyph. **Fix:** quantize the rotation matrix — `c = roundf(cosf(a)*4096)/4096`, same for `s`
— so the sub-ULP disagreement rounds away. All three per-pixel rotation probes now do this and are
device-stable. **Implication for goal B:** any software rotation path must derive `(c,s)` from a
shared/quantized trig (not bare libm), or rotation won't be bit-identical across devices. The
*non-rotated* path (Phase 0: `pset`/`rectfill`/fills/`sline`) uses no trig and is unaffected.

`rotfill` also proves **Fork 2's inverse-mapping claim**: rotating a *filled* shape by visiting each
dest pixel and rotating it back to shape-local coords is gap-free by construction at every angle,
where forward-mapping (rotate each source pixel + round) staircases into holes. So rotated fills can
run in software without a GPU fallback; rotated *strokes* remain the harder sub-case (see the doc).

**The one build rule this surfaces:** never `-ffast-math`. It *does* shift the result (consistently
across platforms, but it's a footgun if one build enables it and another doesn't). Keep FP flags
identical across the native and emcc builds.

**Not yet covered (the genuinely hard one — Probe 2):** *fill-vs-bounding-outline pixel agreement.*
Each probe passes in isolation because each picked its own coverage convention; `linesym.c` shows
the tension (fills reflect about the true centre `320-x`, strokes about the cell mirror `319-x`).
Settling one coverage convention every primitive obeys is a design decision, not an algorithm.
