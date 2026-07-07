# scale-grid — a scale-locked isomorphic pad grid (where should it live?)

STATUS: EXPLORING (2026-07-07) — **first decision to make: where does this live** (a `keybed.h`
mode · its own cart · a new `grid.h` library — see §3). Prototyped in `epianofit.c`; born from the
epiano responsive spike but it is **not** epiano-specific — it's a general note-input surface. Split
out of [`epiano-layout-brief.md`](epiano-layout-brief.md) so the piano redesign stays faithful and
this good-but-separate idea gets its own home.

> **Why this doc exists (the drift check, 2026-07-07).** Designing epiano's responsive layout, we
> co-designed a *new* input surface — a scale-locked pad grid — onto the mock. It's a genuinely nice
> idea, but the real `epiano` is a **chromatic piano keybed you slide across** (`keybed.h`); a grid
> is a *different* gesture (Push/Launchpad-like). Bolting it onto the Rhodes would blur the
> instrument's soul. So: **epiano keeps its classic piano keybed** (its faithful Phase-3 redesign);
> the grid becomes an **optional, separately-homed feature** — and the maker wants *both, eventually*.
> This doc owns the grid; the brief owns the piano.

## 1 · What it is

A note-input surface where the player picks a **SCALE** (chromatic / major / minor / pentatonic /
dorian) and a **KEY** (root, C…B). Instead of a piano's white/black keys you get a grid of finger
pads:

- **Scale-locked** — non-chromatic scales show *only in-scale notes*, so there are **no wrong notes**;
  roots are tinted. Chromatic shows all 12 (sharps tinted darker).
- **Isomorphic** — the row offset is a fixed number of scale *degrees*, so a chord/scale shape is
  identical in every key and octave **and survives a reflow** (unlike a column-wrap layout, whose
  shapes shift when the grid resizes). Default **ISO-OCT** (row up = +one octave; roots in a clean
  left column — the legible cold-open); **ISO-4TH** (row up = +a fourth; diagonal chord shapes) kept
  as a selectable seam; **LINEAR dropped** (reflow-defective). Per
  [ADR-0028](../decisions/0028-sensible-defaults-optional-tweaks.md).
- **Finger-gated** — pads are ≥ 1 finger (44pt); the count derives from the floor, never crammed.
- **Range: cap-and-grow** — tight shapes pack finger-pads; roomy shapes cap to `GRID_MAX_OCT` (4)
  octaves and *grow* the pads to fill (big comfy squares); `OCT−/OCT+` window the range.

## 2 · The prototype (what exists today)

All of the above is live in **`tools/carts/epianofit.c`** (the responsive mock — no audio):
`s` scale · `r` key/root · `i` layout (LINEAR/ISO-4TH/ISO-OCT) · `z`/`x` octave · `g` force
piano/grid · `n` native full-bleed. Measured across the device matrix (finger-gated, all touch-safe):

| shape · scale | grid | pads @ finger |
|---|---|---|
| iPad landscape · MAJOR | ISO-OCT, cap+grow | 30 pads @ 2.41 finger |
| iPhone portrait · chroma | ISO-OCT, pack | 54 pads @ 1.25 finger |
| iPhone SE · chroma | pack | 36 pads @ 1.01 finger |
| native 320×240 landscape · MAJOR | cap+grow | 30 pads @ 1.03 finger |

The scale/key/isomorphic maths and finger-gating are decided; what's undecided is **where the
feature is built** so it's reusable and doesn't distort epiano.

## 3 · THE FIRST QUESTION — where does it live?

Three homes; this is the decision to make before any build:

- **A · an opt-in mode inside `keybed.h`.** The header already powers every keybed cart
  (epiano / moog / touchpiano / mellotron). Add a "grid mode": the cart's editor-swap draws piano
  *or* grid, sharing note-on/off, octave, and touch pooling. **Pro:** one implementation, *every*
  keybed cart gains it free, epiano keeps its piano default and gets the grid as a swap. **Con:**
  `keybed.h`'s job is a *piano manual* (white/black, glissando); a grid is a different topology —
  risks a muddy two-headed header.
- **B · its own cart.** A dedicated grid instrument. **Pro:** clean identity, `keybed.h` stays
  focused. **Con:** reinvents note plumbing, lifts no other cart, another cart to maintain.
- **C · a new library header `grid.h`** (a twin of `keybed.h`, reusing `solo.h`'s scale-lock maths).
  Any cart draws it as an alternate note surface via the editor-swap; it can also stand alone as its
  own cart. **Pro:** `keybed.h` stays the piano; the grid is a clean, shared module; lifts every
  cart AND works solo. **Con:** one more header to design (but it's small and well-scoped).

**Recommendation: C (a `grid.h` library).** It keeps the piano keybed pure, gives the grid a real
home the whole shelf can reuse, composes with the editor-swap in `epiano`, and doesn't fork into a
one-off cart. Reuse `solo.h` for the scale→semitone maths rather than re-deriving it. But this is a
**maker's architecture call** — it's the first thing to solve, and it gates the build.

## 4 · Open sub-questions (after the home is picked)

- **`GRID_MAX_OCT` value** — 4 now (generous small-keyboard range). 3 = bigger pads / 5 = more range.
- **Confirm the ISO-OCT default WITH SOUND** — the mock is silent; a note layout is a feel/sound
  call (ADR-0028's "confirm on glass"). ISO-4TH is one keystroke away if it wins.
- **Isomorphic layout** — DECIDED at the mock level (fixed degree offset; ISO-OCT default), but
  re-confirm once it's audible.
- **Chord affordances** — isomorphic shapes beg for chord pads / strum; out of scope for v1, note it.

## 5 · Relationship to epiano (both, eventually)

`epiano`'s redesign ([`epiano-layout-brief.md`](epiano-layout-brief.md)) stays **faithful**: the
classic chromatic piano keybed that scales with device width + the reflowing pedalboard (its own
`de:meta` Phase-3 plan). The **editor-swap** is the seam: once `grid.h` (or whichever home wins)
exists, epiano can offer the scale-grid as an *optional* alternate editor — the piano is the soul,
the grid is a bonus. Neither blocks the other; build the grid home first (the maker's chosen
starting point), then wire epiano's swap to it.

Related: [`epiano-layout-brief.md`](epiano-layout-brief.md) (the piano half) ·
[`acidrack-ui-research.md`](acidrack-ui-research.md) (touch/density numbers) ·
[`../guides/responsive-instrument-ui.md`](../guides/responsive-instrument-ui.md) (the playbook) ·
[ADR-0028](../decisions/0028-sensible-defaults-optional-tweaks.md) (defaults + seams) ·
`tools/carts/epianofit.c` (the prototype) · `runtime/keybed.h` · `runtime/solo.h` (scale-lock to
reuse) · [`device-adaptive-layout.md`](device-adaptive-layout.md).
