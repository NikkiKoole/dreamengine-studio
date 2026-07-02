# API usage audit — which functions do the carts actually use?

<!-- de:driftable cmd="node tools/api-usage.js" as-of="2026-07-02" inputs="runtime/studio.h,tools/carts" -->

*Snapshot: 2026-07-02 — 318 `studio.h` functions × 453 carts (the 2026-06-04 audit saw
182 × 233; the repo nearly doubled in a month). Numbers go stale; the method doesn't.
Re-run anytime:*

```bash
node tools/api-usage.js            # full table, least-used first
node tools/api-usage.js --unused   # only the never/once-used tail
```

The script counts word-boundary `name(` matches per cart in `tools/carts/*.c` —
textual, but reliable here because cart code shares one namespace with the API.
It also cross-checks the "four places" rule (`studio.h` ↔ `studioDocs.js` ↔
`shell.js` help-tab keys) and reports gaps.

## Never used in any cart (3 of 318)

| function | reading |
|---|---|
| `tapr` | tap-release edge — `tap` (6 carts) and `touch_ended_x/y` (2) cover the need so far |
| `watch_visible` | host/debug convenience, still unexercised |
| `paused` | host/debug convenience, still unexercised (its doc gap from the last audit was fixed; the fn itself still has no consumer) |

**The 2026-06-04 unused list is otherwise resolved** — the two camps it identified both
closed the way the audit predicted:
- *Convenience helpers that lost to hand-rolling* were **cut** the same week
  (`music` → [decision 0013](../decisions/0013-cut-music-api.md); `bezier_cubic`,
  `bounce_at_edges`, `anim_once` → [decision 0014](../decisions/0014-cut-unused-convenience-helpers.md)).
- *Input the platform couldn't test* got tested: the touch-controls program made touch
  mainstream — `touch_x`/`touch_y` are in **47 carts** now (were 1), `tap` in 6,
  `stick_x`/`stick_y`/`touch_controls` each found a consumer. Even `map_scale` found a cart.

The once/twice-used tail (~40 fns) is dominated by the **per-instrument FX family**
(`instrument_ringmod`/`_univibe`/`_leslie`/`_gate`/`_tape`, `crush_inst`, `eq_inst`, …,
mostly at exactly 1 cart) — that lone cart is `mixbooth`, shipped 2026-07-01 *precisely* to
give the family its first consumer (the demo-cart rule applied deliberately). Second audit
in a row where the odd corners are `*outline` shape variants (`arcoutline`,
`thicklineoutline`, `ringoutline` — 2 carts each).

## The other end

`draw` 453 (by contract) · `update` 436 · `print` 435 (3085 calls!) · `cls` 432 ·
`rectfill` 379 (2822) · `init` 337 · `rect` 308 · `line` 305 · `circfill` 277 ·
`str` 259 · `pset` 223 · `keyp` 221 · **`instrument` 221 (898)** · `clamp` 202 ·
`btnp` 199 · `print_centered` 180 · `hit` 178 · `rnd` 177 · `note` 175.

`instrument` and `keyp` tying the drawing primitives at 221 carts is the audit's one-line
portrait of what the repo became: half fantasy console, half instrument workshop.

## What the shape of the data says

- **The cut-or-ship adage worked.** Everything on last month's "either ship a cart that
  makes it shine, or cut it" list was shipped-for or cut. The unused set is down to 3
  host/debug conveniences — the API carries essentially no dead weight.
- **`watch()` is in 147 carts (687 calls)** — the DE_TRACE harness convention became
  default practice, not a specialist tool.
- **Gradient asymmetry persists**: `vgradient` 11 · `gradient` 4 · `hgradient` 2 —
  vertical sky gradients remain the real use case.
- **`printh` is unchanged at 3 carts / 44 calls** — still the sound tools'
  export-as-code mechanism (decision-0003 flow), still not leftover debug.
- **`sfx()` is still really a stop button**: 25 of its 29 calls are `sfx(-1)`
  (silence-before-a-dramatic-note), across 28 carts.
- **The live-voice sound tier boomed**: `note_on` 96 · `note_off` 61 · `note_pitch` 51 ·
  `note_vol` 29 (were 8–14). The deep modulation tier stays thin — `note_res` 9 ·
  `note_filter` 5 · `note_env` 2 — used, barely, same as last audit.
- **Four-places gaps: exactly three, all deliberate-looking** — `de_data_path`,
  `de_dropped_file`, `de_open_path` are missing from both `studioDocs.js` and `shell.js`.
  These are the EXPERIMENTAL runtime-data hooks
  ([external-data-carts.md](external-data-carts.md) explicitly calls them "not committed
  API"), so undocumented-pending-commitment is defensible — but decide it, don't drift it:
  either document them or mark the experiment's exit criteria. (`paused`'s gap from last
  audit was fixed.)

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
