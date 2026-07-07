# epiano — layout brief (the responsive redesign worksheet)

STATUS: DRAFT (2026-07-07) — decisions captured live from the `epianofit` mock (the
responsive-instrument-ui.md **step-4** artifact). The keybed model — a note/sound split, the
piano↔grid **editor-swap**, and a **finger-gate** — is decided and validated across the device
matrix; a handful of taste calls (§6) remain. Ready to build the real `epiano` against once those
close. Second worked example of [`acidrack-layout-brief.md`](acidrack-layout-brief.md)'s template.

> **What this doc is.** The committed design artifact the redesign gets built — and judged —
> against (field note [018](../field-notes/018-passing-the-gates-felt-like-done.md)): decisions
> written down *before* the real cart changes, so "done" means *matches this on device*, not "no
> audit findings". Where acidrack is a 5-strip **rack**, epiano is the other shape — ONE instrument
> whose identity is a **keybed** — so the model differs (a pinned hero + a disclosing panel, not N
> folding strips). The mock that produced these calls: `tools/carts/epianofit.c`.

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

The keybed is the **pinned performing surface** — it never folds. Only the **sound panel** above it
discloses, and a slim **note bar** sits between them. Three zones, top→bottom:

```
┌──────────────────────────────────────────────┐
│ EPIANO                                    [<] │  title + back-to-root keep-out
├──────────────────────────────────────────────┤
│ SOUND PANEL  (DISCLOSES — §2)                 │  machine · macros · pedals · preset · raw
├──────────────────────────────────────────────┤
│ SCALE ▸ MAJOR              [OCT-]  C3  [OCT+] │  the NOTE BAR (§3) — pinned
├──────────────────────────────────────────────┤
│ KEYBED  (EDITOR-SWAPS — §3)  ▓ ▓ ▓ ▓ ▓ ▓ ▓ ▓ │  PINNED hero — piano keys OR scale grid
└──────────────────────────────────────────────┘
```

**DECISION D1 — split NOTE input from SOUND.** Scale + octave (what notes you can play) live in the
note bar with the keybed; machine/macros/pedals/preset (how it sounds) live in the panel. They were
tangled in the first pass; separating them is what makes both legible. The note bar is pinned with
the keybed (you always need octave + scale reachable while playing).

## 2 · The SOUND panel — disclosure per shape

epiano's inventory (from `tools/carts/epiano.c`): a 3-way **machine** selector (RHODES/WURLI/CLAV),
two ride-live **macros** (BRITE/BARK), **3 FX pedals** (icon + knob(s)), a **preset** browser, and a
drawer of **raw macros** (HARM/TIMB/MORPH). Tiering by cadence (ride-live → set-once → rare):

| tier | controls | where |
|---|---|---|
| ride-live | machine selector · BRITE · BARK · preset | always in the panel |
| shape-it | the 3 FX pedals + their knobs | inline on roomy; FX tab on tight |
| rare | raw macros (HARM/TIMB/MORPH), autoplay | inline on roomy; FX tab / drawer on tight |

**DECISION D2 — the panel has two states (the keybed is pinned, so no third):**
- **INLINE (roomy / iPad):** selector row on top, then three columns — macros · pedals · (preset
  grid + raw macros). Everything open above a generous keybed.
- **COMPACT (tight / phone):** selector + `[BRITE][BARK][◀preset▶][FX ▸]`. The pedals + raw macros
  live one tap away behind the **FX tab** (an overlay sheet over the keybed area).

## 3 · The keybed EDITOR-SWAP (the heart of this brief)

Same note data, two editors — the panel-level twin of design-system §8.3's widget swap, and the
concrete answer to acidrack-brief §3's "editor swap by budget":

**DECISION D3 — the swap rule.** Real **piano keys** where there's width AND the scale is chromatic;
a **scale-locked pad grid** otherwise:

```
use_grid = (scale != chromatic)  ||  (portrait && !tablet)
```

So: iPad/desktop chromatic → piano; a phone portrait → grid; **any locked scale → grid on every
shape** (including iPad landscape — a field of squarish pads, which is the point of scale-lock).
A `g` override forces piano/grid for comparison (mock only).

**DECISION D4 — the scale selector.** CHROMA / MAJOR / MINOR / PENTA / DORIAN, in the note bar. When
non-chromatic, the grid shows **only in-scale notes** (roots tinted), so there are **no wrong
notes** and a phone gets real range. Chromatic pads tint sharps darker (piano-black cue).

**DECISION D5 — finger-gate both editors (measured, not eyeballed).** A key's touch width is sized
against the finger unit (44 pt) and the count *derives from the floor*, so keys can never shrink
below it:
- piano white keys ≥ **0.9 finger** (`KEY_MIN_F`; a white is a tall target, so a hair under a full
  finger is fine). Was 0.62 in the first pass — sub-touch-target.
- grid pads ≥ **1.0 finger** (discrete buttons — a full finger).
- the readout reports width **in fingers** with a **CRAMPED** flag (orange) if the floor can't be
  met. This is the acidfit "comfort threshold, not fit threshold" lesson, enforced.

**DECISION D6 — the grid has two sizing regimes (Law 2 done right).**
- **tight (phone):** pack as many 1-finger pads as fit — range is precious.
- **roomy (iPad):** **cap** the range to `GRID_MAX_OCT` octaves and **grow** the pads to fill — big
  comfy squares, not 20 octaves of minimum pads. `OCT-`/`OCT+` window the range up/down.
  `GRID_MAX_OCT = 4` for now (see §6 O1).

**DECISION D7 — a prominent `OCT- C3 OCT+`** cluster (finger-sized buttons + a root readout),
replacing the two crammed arrows of the first pass.

Measured across the matrix (from the mock — this is the §5 evidence, and the acceptance target):

| shape · scale | editor | measure |
|---|---|---|
| iPad landscape · chroma | piano | wide, ok |
| iPad landscape · **MAJOR** | grid (roomy) | 30 pads / **4.3 oct** @ **2.41 finger** |
| iPad portrait · chroma | piano | ok |
| iPhone landscape · chroma | piano | 18 white / 2.6 oct @ **0.90 finger** |
| iPhone landscape · **MAJOR** | grid | swaps from piano |
| iPhone portrait · chroma | grid (tight) | 54 pads / 4.5 oct @ **1.25 finger** |
| iPhone SE · chroma | grid (tight) | 36 pads / 3.0 oct @ **1.01 finger** |

The EP truth it makes visible: **piano range scales with width** (1.4 oct portrait vs 3.6 landscape),
which is *why* the grid exists — it rescues the phone. So epiano is NOT a hard lock-landscape rack
(the grid makes portrait genuinely playable); see §6 O6.

## 4 · Arrangements per shape (the topologies)

| shape | sound panel | keybed |
|---|---|---|
| roomy — iPad (either orientation) | **INLINE** (selector + macros + pedals + preset grid + raw) | piano if chromatic, else grid (capped + grown) |
| tall — phone portrait | **COMPACT** strip + FX tab | **grid** (piano too narrow here) |
| short-wide — phone landscape | **COMPACT** strip + FX tab | **piano** if chromatic (wide keybed = the win), else grid |

**Pinned always:** the note bar + keybed. **Orientation policy:** `free` — the grid makes portrait
playable, so unlike otafamily's ribbons this rack needn't lock landscape (revisit if the piano-only
chromatic feel proves to want it — §6 O6).

## 5 · Sizing constants (the finger footprints — filled, not guessed)

Decided against the mock, not invented in code:

- `KEY_MIN_F = 0.9` — piano white-key minimum, in fingers.
- grid pad minimum = **1.0** finger.
- `GRID_MAX_OCT = 4` — the roomy grid's octave cap (§6 O1).
- note bar height ≈ 0.85 finger; OCT cluster ≈ 3.4 fingers wide; scale chip ≈ 4.2 fingers.
- panel budget: **INLINE ≈ 5.8 fingers** tall, **COMPACT ≈ 2.7**, with a hard **≥ 3-finger keybed**
  guarantee (the hero never gets squeezed below that).

These are the acceptance numbers — the build matches the §3 table on device or it isn't done.

## 6 · Open taste calls (what's left for the maker)

- **O1 · `GRID_MAX_OCT` value** — 4 now (generous small-keyboard range). 3 = bigger pads/less range;
  5 = more range/smaller. One-line change.
- **O2 · piano reachable under a locked scale?** A locked scale forces the grid on every shape. Do
  players who want *real keys in a key* get a toggle/loupe back to the piano, or is grid-when-scaled
  absolute? (`g` is the mock's override; not a shipping control yet.)
- **O3 · a ROOT / KEY selector.** Scales are hardcoded to C. A real instrument wants a key (play in
  G, in D…). Almost certainly a second small control in the note bar (`KEY ▸ C`). **Likely a yes —
  flag for the build.**
- **O4 · grid layout — linear vs isomorphic.** The grid ascends bottom-left linearly. An isomorphic
  layout (rows offset by a 4th/5th, à la Launchpad/Push) is far more playable for chords/runs. Worth
  a spike before committing linear.
- **O5 · raw macros + autoplay home on phone** — currently raw macros sit in the FX tab. Is that the
  right drawer, or do they want their own?
- **O6 · orientation** — leaning `free` (grid saves portrait). Confirm on glass: does phone-portrait
  grid actually feel good enough to not lock landscape?

## 7 · Done means

- Matches the §3/§4 tables on device — **including the iPad-landscape scale-grid** (big pads) and the
  phone-portrait grid.
- Every key/pad ≥ its finger floor at every shape (§5), reported in fingers; no CRAMPED.
- `ui-audit --explore --resize` matrix clean + the judgment pass clean (density held, no missing
  arrangement).
- **The maker played each arrangement on glass** (baked, not screenshots) — piano AND grid, each
  scale — and calls it delightful.

## 8 · Device matrix + related

Design against the carried matrix in [`device-matrix.md`](device-matrix.md) §2 (don't re-table it).

Related: [`../guides/responsive-instrument-ui.md`](../guides/responsive-instrument-ui.md) (the
playbook — this brief is its step 3, `epianofit` its step 4) ·
[`acidrack-layout-brief.md`](acidrack-layout-brief.md) (the sibling rack brief + template) ·
[`device-adaptive-layout.md`](device-adaptive-layout.md) (the engine plan) ·
[`acidrack-ui-research.md`](acidrack-ui-research.md) (the touch/density numbers) ·
[`design-system.md`](design-system.md) §8.3 (widget/editor swap) · `tools/carts/epianofit.c` (the
mock) · `tools/carts/epiano.c` (the instrument) · `runtime/solo.h` (the scale-lock the real build
reuses — don't hand-roll the scale math) · `runtime/keybed.h` · `runtime/lay.h`.
