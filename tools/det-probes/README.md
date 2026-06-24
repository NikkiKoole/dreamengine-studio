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

## Findings (2026-06-24, Apple M1 host)

All three are **bit-identical** across arm64 / x86-64 / wasm, and FMA-contraction-immune
(`-ffp-contract=on/fast/off` all agree — the divides leave no contractable surface). So goal-B
determinism is reachable with the **existing float code** — no fixed-point rewrite forced.

**The one build rule this surfaces:** never `-ffast-math`. It *does* shift the result (consistently
across platforms, but it's a footgun if one build enables it and another doesn't). Keep FP flags
identical across the native and emcc builds.

**Not yet covered (the genuinely hard one — Probe 2):** *fill-vs-bounding-outline pixel agreement.*
Each probe passes in isolation because each picked its own coverage convention; `linesym.c` shows
the tension (fills reflect about the true centre `320-x`, strokes about the cell mirror `319-x`).
Settling one coverage convention every primitive obeys is a design decision, not an algorithm.
