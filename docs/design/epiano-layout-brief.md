# epiano — layout brief (the responsive redesign worksheet)

STATUS: DRAFT (2026-07-07) — the **faithful** epiano Phase-3 responsive re-layout: the classic
chromatic piano keybed that scales with device width + a disclosing sound panel. Validated by the
`epianofit` mock. **Scope note (re-scoped 2026-07-07):** the scale-locked pad grid we prototyped is a
*separate* feature with its own home decision — it lives in [`scale-grid.md`](scale-grid.md), not
here. epiano keeps its piano; the grid is an optional future editor. **Both, eventually.** Second
worked example of [`acidrack-layout-brief.md`](acidrack-layout-brief.md)'s template.

> **What this doc is.** The committed design artifact epiano's redesign gets built — and judged —
> against (field note [018](../field-notes/018-passing-the-gates-felt-like-done.md)): decisions
> written *before* the cart changes, so "done" means *matches this on device*. Where acidrack is a
> 5-strip **rack**, epiano is the other shape — ONE instrument whose identity is a **piano keybed**
> — so the model differs (a pinned hero + a disclosing panel, not N folding strips). The mock:
> `tools/carts/epianofit.c` (note: the mock *also* explores the scale-grid feature — that half is
> tracked in [`scale-grid.md`](scale-grid.md), not this brief).

## 0 · House rules (apply to every rack — maker, 2026-07-05; carried from acidrack §0)

- **Decide what to show; never try to show everything.** Everything stays *reachable* — disclosure,
  not amputation.
- **Good icons are smaller than text.** Fantasy console — LEDs, glyphs, 7-seg where a word would eat
  width.
- **Use the fonts and colours we have** — `FONT_TINY`→`FONT_LARGE`, pico32, design-system §7 roles.
- **No shame in stealing from the homage** — steal the *information architecture* (Nord/Rhodes:
  keyboard is the instrument, small type-selected panel, advanced under a lid), never the control
  sizes; the finger budget governs sizing.

## 1 · The shape (epiano's core model)

The **classic piano keybed** is the **pinned performing surface** — it never folds. Only the
**sound panel** above it discloses, and a slim **note bar** sits between them. Three zones:

```
┌──────────────────────────────────────────────┐
│ EPIANO                                    [<] │  title + back-to-root keep-out
├──────────────────────────────────────────────┤
│ SOUND PANEL  (DISCLOSES — §2)                 │  machine · macros · pedals · preset · raw
├──────────────────────────────────────────────┤
│                          [OCT-]  C3  [OCT+]   │  the NOTE BAR (§3) — octave, pinned
├──────────────────────────────────────────────┤
│ KEYBED  ▓ ▓ ▓ ▓ ▓ ▓ ▓ ▓ ▓ ▓ ▓ ▓ ▓ ▓ ▓ ▓ ▓ ▓  │  PINNED hero — the chromatic piano (keybed.h)
└──────────────────────────────────────────────┘
```

**DECISION D1 — split NOTE input from SOUND.** Octave (and, if/when the grid lands, scale + key)
live in the note bar with the keybed; machine/macros/pedals/preset (how it sounds) live in the
panel. They were tangled in the first pass; separating them makes both legible. The note bar is
pinned with the keybed.

## 2 · The SOUND panel — disclosure per shape

epiano's inventory (from `tools/carts/epiano.c`): a 3-way **machine** selector (RHODES/WURLI/CLAV),
ride-live **macros** (BARK + brightness), **3 per-machine pedals** (footswitch + knobs), a **preset**
column, and a drawer of **raw macros** (HARM/TIMB), plus **autoplay**. Tiering by cadence:

| tier | controls | where |
|---|---|---|
| ride-live | machine selector · BRITE · BARK · preset | always in the panel |
| shape-it | the 3 FX pedals + their knobs | inline on roomy; FX tab on tight |
| rare | raw macros (HARM/TIMB), autoplay | inline on roomy; FX tab / drawer on tight |

**DECISION D2 — the panel has two states (the keybed is pinned, so no third):**
- **INLINE (roomy / iPad):** selector row on top, then three columns — macros · pedals · (preset +
  raw macros). Everything open above a generous keybed.
- **COMPACT (tight / phone):** selector + `[BRITE][BARK][◀preset▶][FX ▸]`. The pedals + raw macros
  live one tap away behind the **FX tab** (an overlay sheet over the keybed area).

## 3 · The keybed — the classic piano that scales with width

This is epiano's soul and its own `de:meta` Phase-3 plan: *"keybed width scales to the device (more
octaves on iPad/landscape)… pedalboard reflows."* No swap, no grid — just the `keybed.h` piano,
sized right.

**DECISION D3 — the keybed fills the width; octave count follows.** As many finger-wide white keys as
fit; more octaves on iPad/landscape, fewer on a phone. The `OCT−/OCT+` bar shifts the range.

**DECISION D5 — finger-gate the keys (measured, not eyeballed).** White-key touch width ≥ **0.9
finger** (`KEY_MIN_F`; a white is a tall target so a hair under a full finger is fine — was 0.62 in
the first pass, sub-touch-target). Count *derives from the floor*, so keys never shrink below it; the
readout reports width **in fingers** with a **CRAMPED** flag. (acidfit's "comfort threshold, not fit
threshold.")

**DECISION D7 — a prominent `OCT− C3 OCT+`** cluster (finger-sized buttons + a root readout),
replacing the two crammed arrows of the first pass.

Measured (piano) across the matrix, from the mock — the acceptance target:

| shape | white keys | @ finger |
|---|---|---|
| iPhone landscape | 18 white / 2.6 oct | 0.90 finger |
| iPhone portrait | ~9 white / ~1.4 oct | tight — the phone's weak spot |
| iPad | wider | ok |

**The EP truth this surfaces: piano range scales with width** (portrait ~1.4 oct vs landscape 3.6).
That's the honest cost of a chromatic piano on a small canvas — and exactly the gap the **scale-grid**
([`scale-grid.md`](scale-grid.md)) is meant to fill later as an *optional* editor. epiano ships the
piano first; the grid is the parked swap-target (the **editor-swap** is the seam — same note data,
alternate surface — once the grid's home is built).

## 4 · Arrangements per shape (the topologies)

| shape | sound panel | keybed |
|---|---|---|
| roomy — iPad (either orientation) | **INLINE** (selector + macros + pedals + preset + raw) | wide piano, many octaves |
| tall — phone portrait | **COMPACT** strip + FX tab | piano (few octaves — `OCT` shifts range) |
| short-wide — phone landscape | **COMPACT** strip + FX tab | **wide piano** (the win — more octaves) |

**Pinned always:** the note bar + keybed. **Orientation policy:** `free`, but **portrait is the weak
spot for a piano** — revisit a `lock-landscape` hint if portrait proves cramped in play (the grid, when
it lands, is the other answer). epiano's `de:meta` already flags "consider a lock-landscape hint."

## 5 · Sizing constants (the finger footprints — filled, not guessed)

- `KEY_MIN_F = 0.9` — piano white-key minimum, in fingers.
- note bar height ≈ 0.85 finger; OCT cluster ≈ 3.4 fingers wide.
- panel budget: **INLINE ≈ 5.8 fingers** tall, **COMPACT ≈ 2.7**, with a hard **≥ 3-finger keybed**
  guarantee (the hero never gets squeezed below that).

These are the acceptance numbers — the build matches the §3/§4 tables on device or it isn't done.

## 6 · Open taste calls (what's left for the maker)

- **O5 · raw macros + autoplay home on phone** — currently raw macros sit in the FX tab. Right
  drawer, or their own?
- **O6 · orientation** — `free` vs a `lock-landscape` hint on phone-portrait (the piano is cramped
  there). Confirm on glass. (The scale-grid would relieve this — but that's [`scale-grid.md`](scale-grid.md).)
- **O7 · does the keybed graduate into `keybed.h`?** epiano's `de:meta` suggests the
  width-scaling-keybed "likely graduates into keybed.h so every keybed cart (moog/touchpiano/
  mellotron) benefits." Decide whether Phase-3 lands in `keybed.h` (shared win) or in epiano first.

> **Moved out (2026-07-07 re-scope):** the scale selector, KEY/root selector, isomorphic pad grid,
> editor-swap, and `GRID_MAX_OCT` — all now in [`scale-grid.md`](scale-grid.md) (a separate feature,
> its home TBD). They're good; they're just not epiano's *core* redesign.

## 7 · Done means

- Matches the §3/§4 tables on device — the wide-piano iPad arrangement and the compact-panel phone.
- Every white key ≥ its finger floor at every shape (§5), reported in fingers; no CRAMPED.
- `ui-audit --explore --resize` matrix clean + the judgment pass clean (density held, no missing
  arrangement).
- **The maker played each arrangement on glass** (baked, not screenshots) and calls it delightful.

## 8 · Device matrix + related

Design against the carried matrix in [`device-matrix.md`](device-matrix.md) §2 (don't re-table it).

Related: [`scale-grid.md`](scale-grid.md) (the split-out grid feature — build its home first) ·
[`../guides/responsive-instrument-ui.md`](../guides/responsive-instrument-ui.md) (the playbook — this
brief is its step 3, `epianofit` its step 4) ·
[`acidrack-layout-brief.md`](acidrack-layout-brief.md) (the sibling rack brief + template) ·
[`device-adaptive-layout.md`](device-adaptive-layout.md) (the engine plan) ·
[`acidrack-ui-research.md`](acidrack-ui-research.md) (touch/density numbers) ·
[`design-system.md`](design-system.md) §8.3 (widget/editor swap) · `tools/carts/epianofit.c` (the
mock) · `tools/carts/epiano.c` (the instrument) · `runtime/keybed.h` · `runtime/lay.h`.
