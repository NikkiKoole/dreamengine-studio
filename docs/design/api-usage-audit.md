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

The script counts word-boundary `name(` matches after stripping comments and string literals,
textual but reliable here because cart code shares one namespace with the API. It reads **two
corpora**: the `carts` column is `tools/carts/*.c`, and
`elsewhere` is every other first-party call site (the cart-land shelf, the generated per-cart
contexts, one cart's private modules, the app shim). A zero in both means nobody calls it anywhere;
`--where <fn>` names the files. Engine internals are excluded on purpose, since `sound.h`/`studio.c`
*define* these functions and scanning them would score every name as used.
It also cross-checks the "four places" rule (`studio.h` ↔ `studioDocs.js` ↔
`shell.js` help-tab keys) and reports gaps.

## Two counting bugs, fixed 2026-08-16 — both inflated confidence in a cut list

Worth recording because they nearly cost something, and because they failed in opposite directions:
one hid real consumers, the other invented them.

### It read only carts, so four zeros were false

Until this snapshot the scan read `tools/carts/*.c` **only** — and since the previous audit a whole
tier of shared cart-land headers (`runtime/*.h`,
[ADR-0006](../decisions/0006-library-carts-not-engine.md)) had become real consumers, so a call made
from a header counted as a flat zero. Four of the ten zeros were that:

| function | where it actually lives |
|---|---|
| `de_switch_cart` | the multi-cart app dispatcher, in a template string in `build-app.js` |
| `de_state_for` | `ui.h` + `acidcandy_state.h` — the per-instance-state seam |
| `de_state_for_saved` | `acidcandy_state.h`, the only call in the repo |
| `instrument_bandlimit` | `acid303.h`, so tb303/acidrack/acidcandy all use it through one call site |

This was never cosmetic. **This document's standing advice is "ship a cart that makes it shine, or
cut it"**, so a false zero is a live instruction to cut — and the top two entries are the seam the
AUv3 multi-instance work is built on. A miscount in a table nobody acts on is a nit; a miscount in
a table whose whole purpose is a cut list is a loaded gun.

`api-usage.js` now scans both corpora and prints an `elsewhere` column, `--where <fn>` names the
files, and `--unused` means unused *anywhere*. The shelf/engine-internals split it needs is the one
`lint-docs.js` already maintained for its discoverability gate, so it was extracted to
`tools/cart-land-headers.js` and both read one list rather than two copies that rot apart — which
is exactly the failure mode that gate was written to catch, and had already suffered once.

*Known undercount, deliberately:* `cart_ctx.h` passes `de_state_for` to `DE_CTX_BLOCK` as a macro
**argument**, so every cart using that macro is a transitive consumer no number here counts.
Matching bare names would pick up the three headers that only *mention* it in comments. A
documented undercount beats a silent overcount — read `elsewhere` as a floor.

### It counted prose, so some usage was imaginary

Adding the header corpus surfaced the second bug, because headers are prose-heavy (`cart-dupes.js`
had already measured 56% of `runtime/*.h` "vocabulary" as comment text). The scan matched raw
source, so **2306 of its matches repo-wide were comments or string literals** — 84 of `draw`'s 669,
and the entire `elsewhere` column for `draw` was 27 mentions and zero calls.

Two functions were scored as *used* by text saying they are **not**:

- `paused` — `isoroom`'s only hit is `// NOT `paused` — studio.h already has a paused() built-in`,
  a comment warning against a name collision.
- `harmonize_mic` — a `voxshift` docblock string reading "Once harmonize_mic() has a home cart…".

Both are now correctly zero, and the previous snapshot's claim that `isoroom` had picked `paused`
up was simply wrong. `device_class` is 2 carts (`acidcandy`, `facedemo`), not the 4 first counted:
`deviceface` mentions it in a comment and `roomyface` in its `de:meta` summary. The fix is the
`decomment()` routine `cart-dupes.js` already used for exactly this reason.

**Genuinely unconsumed (8 of 396):**

| function | reading |
|---|---|
| `tapr` | tap-release edge — `tap` (7 carts) and `touch_ended_x/y` (2) cover the need so far |
| `watch_visible` | host/debug convenience, unexercised across three audits |
| `paused` | host/debug convenience, likewise — and never actually picked up, see above |
| `harmonize_mic` | the live twin of voxshift's offline pitch work; still waiting on a home cart, and its own docblock says so |
| `midi_send_bend` | new 2026-08-13; `midiout` exercises the rest of the send family, not bend yet |
| `instrument_glide` | new 2026-07-30 (portamento as a patch property) — no cart shipped for it yet |
| `instrument_glide_scale` | ditto, same day, same gap |
| `instrument_multiband` | new 2026-07-26 (multiband squash on one instrument's bus) — no consumer |

Four of those landed **after** the last audit, so they are exactly the cut-or-ship list this doc
keeps producing: ship a cart that makes each shine, or cut it. The other four have now survived
three audits without a consumer, which is its own answer.

**One prediction from the 2026-07-10 audit came true**, which is the argument for keeping the list:
`device_class` was called "brand-new, the Phase-3 responsive racks are its intended consumers", and
`acidcandy` + `facedemo` now branch on it. The companion prediction about `paused` did not — that
entry was a miscount, not a consumer.

**The 2026-06-04 unused list is otherwise resolved** — the two camps it identified both
closed the way the audit predicted:
- *Convenience helpers that lost to hand-rolling* were **cut** the same week
  (`music` → [decision 0013](../decisions/0013-cut-music-api.md); `bezier_cubic`,
  `bounce_at_edges`, `anim_once` → [decision 0014](../decisions/0014-cut-unused-convenience-helpers.md)).
- *Input the platform couldn't test* got tested: the [touch-controls](touch-controls.md) program made touch
  mainstream — `touch_x`/`touch_y` are in **70 carts** as of 2026-08-16 (were 1 in June, 57 in July),
  `tap` in 7, `stick_x`/`stick_y`/`touch_controls` each found a consumer. Even `map_scale` found a cart.

The once/twice-used tail (~40 fns) is still dominated by the **per-instrument FX family**
(`instrument_ringmod` 2, `_univibe` 1, `_leslie` 1, `_gate` 1, `_tape` 1, `crush_inst` 1,
`eq_inst` 2) — that lone cart is `mixbooth`, shipped 2026-07-01 *precisely* to give the family its
first consumer (the demo-cart rule applied deliberately). Unmoved in five weeks: the demo cart
discharged the obligation but did not start a trend. Third audit in a row where the odd corners are
`*outline` shape variants (`arcoutline`, `thicklineoutline`, `ringoutline` — 2 carts each).

## The other end

`draw` 581 (by contract) · `print` 558 (4287 calls!) · `cls` 557 · `update` 549 ·
`rectfill` 477 (3374) · `init` 410 · `line` 393 (1952) · `rect` 364 · `circfill` 350 ·
`keyp` 314 (1869) · `str` 301 · **`instrument` 270 (1063)** · `pset` 269 · `font` 243 ·
`clamp` 219 · `btnp` 209 · `watch` 205 · `circ` 194 · `print_centered` 194 · `rnd` 193.

`keyp` 314 and `instrument` 270 running WITH the drawing primitives is still the audit's
one-line portrait of what the repo became: half fantasy console, half instrument workshop.
New to the top 20: **`font` 243 (1570 calls)** — the six-font shelf stopped being a novelty and
became routine typography.

## What the shape of the data says

- **The cut-or-ship adage keeps working, and keeps having new work.** The eight unconsumed today
  are four host/debug-ish leftovers plus four functions younger than the last audit. The steady
  state is "the API carries little dead weight, and always has ~4 fresh names waiting for a cart."
- **The audit's method needed fixing more than its numbers did.** Both bugs above were invisible
  in the output: every figure stayed plausible while the corpus quietly stopped matching where the
  code lives, and while prose got counted as code. That is the expensive shape — a wrong number
  that looks wrong gets checked, and a wrong number that looks right becomes a decision. Worth
  re-asking each audit: *is this still reading the right files, and is it reading only code?*
- **`watch()` is in 205 carts (1038 calls)**, up from 162/785 — the DE_TRACE harness convention is
  now default practice by a wide margin, not a specialist tool.
- **Gradient asymmetry persists** (third audit): `vgradient` 12 · `gradient` 2 · `hgradient` 2 —
  vertical sky gradients remain the real use case.
- **`printh` is unchanged at 3 carts / 44 calls** — still the sound tools'
  export-as-code mechanism (decision-0003 flow), still not leftover debug. Unmoved across three audits.
- **`sfx()` is still really a stop button**: 26 of its 29 calls are `sfx(-1)`
  (silence-before-a-dramatic-note), across 28 carts. The ratio has not budged.
- **The live-voice sound tier kept climbing**: `note_on` 127 · `note_off` 84 · `note_pitch` 71 ·
  `note_vol` 37 (were 106/70/57/33). The deep modulation tier moved barely — `note_res` 11 ·
  `note_filter` 6 · `note_env` 2 (were 10/5/2): still the thinnest tier in the API. (July's figures
  are pre-decomment and so read a little high; the direction is right, the deltas are soft.)
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
