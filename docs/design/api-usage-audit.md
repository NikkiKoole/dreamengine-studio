# API usage audit — which functions do the carts actually use?

*Snapshot: 2026-06-04 — 182 `studio.h` functions × 233 carts. Numbers go stale; the
method doesn't. Re-run anytime:*

```bash
node tools/api-usage.js            # full table, least-used first
node tools/api-usage.js --unused   # only the never/once-used tail
```

The script counts word-boundary `name(` matches per cart in `tools/carts/*.c` —
textual, but reliable here because cart code shares one namespace with the API.
It also cross-checks the "four places" rule (`studio.h` ↔ `studioDocs.js` ↔
`shell.js` help-tab keys) and reports gaps.

## Never used in any cart (11 of 182)

| function | reading |
|---|---|
| `tap`, `touch_controls`, `stick_x`, `stick_y` (+ `touch_x`/`touch_y` at 1 cart each) | touch/gamepad input — nothing exercises it on desktop. Fair, but untested code. |
| `music` | **CUT same day** ([decision 0013](../decisions/0013-cut-music-api.md)): the only pattern in `music_bank` was the built-in demo (`music 0` = bass+hihat), and there was no cart-facing API to author patterns. Carts build music from `note`/`schedule`/`strum`/`bpm` instead — the direction `guides/game-music.md` teaches. |
| `bezier_cubic` | **CUT** ([decision 0014](../decisions/0014-cut-unused-convenience-helpers.md)): quadratic `bezier` covers the need; even that is only in 2 carts |
| `map_scale` | every map cart runs at default scale |
| `bounce_at_edges` | **CUT** ([decision 0014](../decisions/0014-cut-unused-convenience-helpers.md)): carts hand-roll bouncing — the 4-pointer-arg signature was more friction than the two `if`s it saved |
| `anim_once` | **CUT** ([decision 0014](../decisions/0014-cut-unused-convenience-helpers.md)): looping `anim` is in 23 carts; a one-shot is `min((int)((now()-t0)*fps), n-1)` in cart code |
| `watch_visible`, `paused` | host/debug conveniences — and `paused` is also the one function **missing from `studioDocs.js` and `shell.js`** (the four-places check found exactly this one gap) |

Used exactly once: `arcoutline`, `thicklineoutline`, `hgradient`, `noise3`, `keyr`,
`ringoutline`, `sget`, `epoch`, `touching_color` — mostly the `*outline` shape
variants and odd corners.

## The other end

`draw` 233 (by contract) · `print` 225 · `update` 222 · `cls` 220 · `rectfill` 203
(1788 calls!) · `str` 193 · `init` 184 · `line` 168 · `circfill`/`rect` 164 ·
`btnp` 158 · `note` 146 · `print_centered` 143 · `pset` 140 · `clamp` 130.

## What the shape of the data says

- **The unused set splits into two camps.** Input the platform can't test
  (touch/stick) vs. *convenience helpers that lost to hand-rolling*
  (`bounce_at_edges`, `anim_once`, `music`, `bezier_cubic`). The second camp
  confirms the CLAUDE.md adage: a primitive without a cart demonstrating it goes
  unnoticed. Either ship a cart that makes each one shine, or cut it
  (→ `decisions/` if cut).
- **Gradient asymmetry**: `vgradient` 6 carts, `gradient` 3, `hgradient` 1 —
  vertical sky gradients are the real use case.
- **`printh` is in only 3 carts but 44 calls** — smells like leftover debug
  prints worth sweeping.
- **`sfx()` is really a stop button**: 8 of its 11 call sites are `sfx(-1)`
  ("silence ringing sounds before a dramatic note"); only the `soundcheck`
  self-test plays a slot. The 6 demo slots stay for first contact, but the bank
  was never cart-authorable — see [decision 0013](../decisions/0013-cut-music-api.md).
- **The sound API's "live voice" tier is healthy**: `note_on`/`note_off`/
  `note_pitch`/`note_vol` all land in 8–14 carts; the deep modulation tier
  (`note_res`, `note_filter`, `note_env`) sits at 2–3 carts — used, barely.
- **Doc coverage is excellent**: 182 functions, exactly one gap (`paused`).

## Tooling note: clangd/LSP works on this repo

Claude Code's LSP tool (clangd) resolves this codebase fine — `documentSymbol`
on `studio.h` returns the full API with signatures, and goToDefinition /
findReferences / call-hierarchy all work. Two caveats:

- `sound.h` is only compiled inside `studio.c`, so clangd can't follow
  references *into* it (e.g. `findReferences` on `music` finds only the
  declaration even though `sound.h:702` implements it). Same root cause as the
  known "analyzers parsing sound.h standalone" false errors in CLAUDE.md.
- For *bulk* questions ("how often is X used across 233 carts?") the textual
  scan in `tools/api-usage.js` is faster than per-symbol LSP queries; use LSP
  for the precise, single-symbol questions (where is this defined, who calls
  it, what's its real type).
