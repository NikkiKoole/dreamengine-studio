# API usage audit — which functions do the carts actually use?

STATUS: LIVING (snapshot 2026-07-10, re-runnable) — `node tools/api-usage.js`; drift tracked by the de:driftable marker.

<!-- de:driftable cmd="node tools/api-usage.js" as-of="2026-07-10" inputs="runtime/studio.h,tools/carts" -->

*Snapshot: 2026-07-10 — 330 `studio.h` functions × 481 carts (2026-06-04: 182 × 233;
2026-07-04: 323 × 466 — still growing, no longer doubling). Numbers go stale; the method doesn't.
Re-run anytime:*

```bash
node tools/api-usage.js            # full table, least-used first
node tools/api-usage.js --unused   # only the never/once-used tail
```

The script counts word-boundary `name(` matches per cart in `tools/carts/*.c` —
textual, but reliable here because cart code shares one namespace with the API.
It also cross-checks the "four places" rule (`studio.h` ↔ `studioDocs.js` ↔
`shell.js` help-tab keys) and reports gaps.

## Never used in any cart (5 of 330)

| function | reading |
|---|---|
| `tapr` | tap-release edge — `tap` (6 carts) and `touch_ended_x/y` (2) cover the need so far |
| `watch_visible` | host/debug convenience, still unexercised |
| `paused` | host/debug convenience, still unexercised (its doc gap from the last audit was fixed; the fn itself still has no consumer) |
| `device_class` | brand-new (2026-07, the device-adaptive seam — TALL/WIDE/ROOMY); the Phase-3 responsive racks being built now are its intended consumers |
| `de_switch_cart` | not cart-facing — the multi-cart app dispatcher (`build-app.js` shim) calls it, not cart code, so 0 hits in `tools/carts/*.c` is correct (like a platform seam). The device-adaptive `screen_w`/`screen_h`/`safe_rect` added the same window are NOT here — `respond` uses them. |

**The 2026-06-04 unused list is otherwise resolved** — the two camps it identified both
closed the way the audit predicted:
- *Convenience helpers that lost to hand-rolling* were **cut** the same week
  (`music` → [decision 0013](../decisions/0013-cut-music-api.md); `bezier_cubic`,
  `bounce_at_edges`, `anim_once` → [decision 0014](../decisions/0014-cut-unused-convenience-helpers.md)).
- *Input the platform couldn't test* got tested: the [touch-controls](touch-controls.md) program made touch
  mainstream — `touch_x`/`touch_y` are in **57 carts** now (were 1), `tap` in 7,
  `stick_x`/`stick_y`/`touch_controls` each found a consumer. Even `map_scale` found a cart.

The once/twice-used tail (~40 fns) is dominated by the **per-instrument FX family**
(`instrument_ringmod`/`_univibe`/`_leslie`/`_gate`/`_tape`, `crush_inst`, `eq_inst`, …,
mostly at exactly 1 cart) — that lone cart is `mixbooth`, shipped 2026-07-01 *precisely* to
give the family its first consumer (the demo-cart rule applied deliberately). Second audit
in a row where the odd corners are `*outline` shape variants (`arcoutline`,
`thicklineoutline`, `ringoutline` — 2 carts each).

## The other end

`draw` 481 (by contract) · `print` 463 (3354 calls!) · `update` 462 · `cls` 459 ·
`rectfill` 396 (2977) · `init` 352 · `rect` 325 · `line` 324 · `circfill` 293 ·
`str` 275 · `keyp` 242 · **`instrument` 238 (1040)** · `pset` 230 · `clamp` 205 ·
`btnp` 204 · `print_centered` 189 · `hit` 187 · `rnd` 185 · `note` 184.

`keyp` 242 and `instrument` 238 running WITH the drawing primitives is still the audit's
one-line portrait of what the repo became: half fantasy console, half instrument workshop.

## What the shape of the data says

- **The cut-or-ship adage worked.** Everything on last month's "either ship a cart that
  makes it shine, or cut it" list was shipped-for or cut. The unused set is down to 3
  host/debug conveniences (+ `device_class`, too new to judge) — the API carries essentially no dead weight.
- **`watch()` is in 162 carts (785 calls)** — the DE_TRACE harness convention became
  default practice, not a specialist tool.
- **Gradient asymmetry persists**: `vgradient` 12 · `gradient` 4 · `hgradient` 2 —
  vertical sky gradients remain the real use case.
- **`printh` is unchanged at 3 carts / 44 calls** — still the sound tools'
  export-as-code mechanism (decision-0003 flow), still not leftover debug.
- **`sfx()` is still really a stop button**: 25 of its 29 calls are `sfx(-1)`
  (silence-before-a-dramatic-note), across 28 carts.
- **The live-voice sound tier boomed**: `note_on` 106 · `note_off` 70 · `note_pitch` 57 ·
  `note_vol` 33 (were 8–14 in June). The deep modulation tier is waking — `note_res` 10 ·
  `note_filter` 5 · `note_env` 2 — used, barely, same as last audit.
- **Four-places gaps: zero.** The audit's one finding — `de_data_path`/`de_dropped_file`/
  `de_open_path` undocumented — was the flag that triggered
  [ADR-0025](../decisions/0025-external-data-hooks-are-committed-api.md): the experimental
  data hooks were promoted to committed API the same day (sloop/roadview/citydrive had
  already made them load-bearing). (`paused`'s gap from the last audit was also fixed.)

## Tooling note: clangd/LSP works on this repo

Claude Code's LSP tool (clangd) resolves this codebase fine — `documentSymbol`
on `studio.h` returns the full API with signatures, and goToDefinition /
findReferences / call-hierarchy all work. Two caveats:

- `sound.h` is only compiled inside `studio.c`, so clangd can't follow
  references *into* it (e.g. `findReferences` on a sound fn finds only the
  declaration even though `sound.h` implements it). Same root cause as the
  known "analyzers parsing sound.h standalone" false errors in CLAUDE.md.
- For *bulk* questions ("how often is X used across 453 carts?") the textual
  scan in `tools/api-usage.js` is faster than per-symbol LSP queries; use LSP
  for the precise, single-symbol questions (where is this defined, who calls
  it, what's its real type).
