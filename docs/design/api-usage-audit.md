# API usage audit — which functions do the carts actually use?

STATUS: LIVING (snapshot 2026-08-16, re-runnable) — `node tools/api-usage.js`; drift tracked by the de:driftable marker.

<!-- de:driftable cmd="node tools/api-usage.js" as-of="2026-08-16" inputs="runtime/studio.h,tools/carts" -->

*Snapshot: 2026-08-16 — 396 `studio.h` functions × 581 carts (2026-06-04: 182 × 233;
2026-07-04: 323 × 466; 2026-07-10: 330 × 481 — still growing, no longer doubling). Numbers go
stale; the method doesn't. Re-run anytime:*

```bash
node tools/api-usage.js            # full table, least-used first
node tools/api-usage.js --unused   # only the never/once-used tail
```

The script counts word-boundary `name(` matches per cart in `tools/carts/*.c` —
textual, but reliable here because cart code shares one namespace with the API. **It does not read
`runtime/*.h`**, so a call made from a shared cart-land header counts as zero; that blind spot is
now load-bearing enough to have its own section below.
It also cross-checks the "four places" rule (`studio.h` ↔ `studioDocs.js` ↔
`shell.js` help-tab keys) and reports gaps.

## Read the zeros in two piles now — the scan has a blind spot

The scan counts `name(` in `tools/carts/*.c` **only**. Since the last audit a whole tier of shared
cart-land headers (`runtime/*.h`, ADR-0006) became real consumers, and a call made from a header is
invisible here. So a 0 no longer means "dead weight" — check `runtime/` before concluding anything.
Four of this snapshot's ten zeros are that false pile:

| function | where it actually lives |
|---|---|
| `de_switch_cart` | the multi-cart app dispatcher (`build-app.js` shim), not cart code — a platform seam, correctly 0 here |
| `de_state_for` | `ui.h`, `gestures.h`, `tr909.h`, `acidcandy_state.h`, `cart_ctx.h` — the per-instance-state seam; carts reach it through `DE_CTX_BLOCK`, never by name |
| `de_state_for_saved` | same, via `DE_CTX_BLOCK_SAVED` (`acidcandy_state.h` is the live consumer) |
| `instrument_bandlimit` | `acid303.h` — so tb303/acidrack/acidcandy all use it, at one call site |

**Genuinely unconsumed (6 of 396):**

| function | reading |
|---|---|
| `tapr` | tap-release edge — `tap` (14 carts) and `touch_ended_x/y` (2) cover the need so far |
| `watch_visible` | host/debug convenience, still unexercised (three audits running) |
| `midi_send_bend` | new 2026-08-13; `midiout` exercises the rest of the send family, not bend yet |
| `instrument_glide` | new 2026-07-30 (portamento as a patch property) — no cart shipped for it yet |
| `instrument_glide_scale` | ditto, same day, same gap |
| `instrument_multiband` | new 2026-07-26 (multiband squash on one instrument's bus) — no consumer |

Every one of those four new ones landed **after** the last audit, so they are exactly the
cut-or-ship list this doc keeps producing: ship a cart that makes each shine, or cut it.

**Two predictions from the 2026-07-10 audit came true**, which is the argument for keeping the
list: `device_class` was called "brand-new, the Phase-3 responsive racks are its intended
consumers" — it is now in 4 carts (`acidcandy`, `deviceface`, `facedemo`, `roomyface`). `paused`
was "host/debug convenience, still unexercised" — `isoroom` picked it up.

**The 2026-06-04 unused list is otherwise resolved** — the two camps it identified both
closed the way the audit predicted:
- *Convenience helpers that lost to hand-rolling* were **cut** the same week
  (`music` → [decision 0013](../decisions/0013-cut-music-api.md); `bezier_cubic`,
  `bounce_at_edges`, `anim_once` → [decision 0014](../decisions/0014-cut-unused-convenience-helpers.md)).
- *Input the platform couldn't test* got tested: the [touch-controls](touch-controls.md) program made touch
  mainstream — `touch_x`/`touch_y` are in **70 carts** as of 2026-08-16 (were 1 in June, 57 in July),
  `tap` in 14, `stick_x`/`stick_y`/`touch_controls` each found a consumer. Even `map_scale` found a cart.

The once/twice-used tail (~40 fns) is still dominated by the **per-instrument FX family**
(`instrument_ringmod` 2, `_univibe` 1, `_leslie` 1, `_gate` 1, `_tape` 1, `crush_inst` 1,
`eq_inst` 2) — that lone cart is `mixbooth`, shipped 2026-07-01 *precisely* to give the family its
first consumer (the demo-cart rule applied deliberately). Unmoved in five weeks: the demo cart
discharged the obligation but did not start a trend. Third audit in a row where the odd corners are
`*outline` shape variants (`arcoutline`, `thicklineoutline`, `ringoutline` — 2 carts each).

## The other end

`draw` 581 (by contract) · `print` 559 (4295 calls!) · `cls` 558 · `update` 549 ·
`rectfill` 477 (3381) · `init` 410 · `line` 400 (2016) · `rect` 370 · `circfill` 350 ·
`keyp` 314 (1874) · `str` 302 · **`instrument` 272 (1104)** · `pset` 269 · `font` 244 ·
`clamp` 225 · `note` 210 · `btnp` 209 · `watch` 206 · `hit` 203 · `rnd` 198.

`keyp` 314 and `instrument` 272 running WITH the drawing primitives is still the audit's
one-line portrait of what the repo became: half fantasy console, half instrument workshop.
New to the top 20: **`font` 244 (1577 calls)** — the six-font shelf stopped being a novelty and
became routine typography.

## What the shape of the data says

- **The cut-or-ship adage keeps working, and keeps having new work.** Every 2026-07 name is
  shipped-for or cut; the six unconsumed today are two long-standing host/debug conveniences plus
  four functions younger than the last audit. The steady state is "the API carries no dead weight,
  and always has ~4 fresh names waiting for their cart."
- **The zeros need a second look now** — see the two piles above. Four of ten are called from
  `runtime/*.h`, not from a cart, and the scan cannot see them. This is the audit's newest
  methodological caveat and the one most likely to mislead: an agent reading "0 carts" and reaching
  for the shears would have cut `de_state_for`, the per-instance-state seam the AUv3 work is built on.
- **`watch()` is in 206 carts (1044 calls)**, up from 162/785 — the DE_TRACE harness convention is
  now default practice by a wide margin, not a specialist tool.
- **Gradient asymmetry persists** (third audit): `vgradient` 12 · `gradient` 4 · `hgradient` 2 —
  vertical sky gradients remain the real use case.
- **`printh` is unchanged at 3 carts / 44 calls** — still the sound tools'
  export-as-code mechanism (decision-0003 flow), still not leftover debug. Unmoved across three audits.
- **`sfx()` is still really a stop button**: 26 of its 32 calls are `sfx(-1)`
  (silence-before-a-dramatic-note), across 30 carts. The ratio has not budged.
- **The live-voice sound tier kept climbing**: `note_on` 128 · `note_off` 84 · `note_pitch` 72 ·
  `note_vol` 37 (were 106/70/57/33). The deep modulation tier finally moved too — `note_res` 12 ·
  `note_filter` 6 · `note_env` 2 (were 10/5/2): waking, slowly, still the thinnest tier in the API.
- **Four-places gaps: zero**, third audit running. The 2026-07 finding — `de_data_path`/`de_dropped_file`/
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
