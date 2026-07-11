# The distortion lab — a "destruction unit" playground

Status: **BUILDING (2026-07-11).** Rungs 0–2 shipped in the **`distlab`** cart: a master insert
CHAIN (PRE-EQ → DRIVE A → DRIVE B → POST-EQ) on a held acid drone, with a live transfer curve (the
composed cascade shape) + a `scope_read` oscilloscope + a flat param-cursor UI across the chain.
Rungs 3–5 open. A cart that
goes *beyond* the four built-in drive modes into modern "super distortion plugin" territory
(stacking, tone-stacking, combos). The plan is **start simple, grow** — the increment ladder below is
the proposed order, not a spec. Born from the acidrack chat where the 303 got a drive-mode selector
and the maker wanted to experiment further.

## Why — and what we already have

The engine already carries a real distortion palette, so this is **not greenfield**:

- **`instrument_drive(slot, amount)` + `instrument_drive_mode(slot, DRIVE_*)`** — four waveshapers,
  post-filter, true-bypass at 0, loudness-normalized (character not volume):
  - `DRIVE_SOFT` — tanh soft-clip, warm tube overdrive (**the default** — what everything used before
    modes existed, and still the 303's default)
  - `DRIVE_HARD` — hard clip, buzzy square-edged digital fuzz
  - `DRIVE_FOLD` — sine wavefolder, metallic/glassy/ring-mod-ish (the "West Coast" texture)
  - `DRIVE_ASYM` — asymmetric tube, fat **even** harmonics (+ a little DC, cleaned by the DC blocker)
- **`drive_insert(amount, mode, mix)` + `drive_insert_inst(instance, …)`** — the same waveshapers as a
  **mix-bus insert**, with a *second instance* → you can **cascade** (overdrive INTO an amp) in one
  chain via `fx_order(0, {FX_DRIVE, FX_INST(FX_DRIVE,1)})`.
- **`eq(lo, mid, hi)`** (±12 dB) — the **tone stack**. The effects-bus doc notes EQ *before AND after*
  a clipper is buffer-free and audibly different (input-shape vs output-shape). This is the
  Decapitator/guitar-amp move that turns fizz into character.
- **`crush()` / `tape()`** — bitcrush (digital decimation grit) and tape (wow/flutter/soft-sat) to
  layer on for total destruction.

### What already exists in cart-land (checked 2026-07-11 — do NOT duplicate)

- **`drivemodes`** (tech-demo) — the clean **reference**: the 4 modes side-by-side on a growling
  bass, a representative waveshape drawn under each panel, LEFT/RIGHT to pick, UP/DOWN for amount.
  Keep it as the tidy "here are the 4 flavours" showcase. The lab is the *playground beyond* it.
- **`bitcrush`** — the crush showcase.
- **`acidrack`** — as of 2026-07-11 each 303's `[fx]` view has a SOFT/HARD/FOLD/ASYM selector
  (`instrument_drive_mode`). The lab's payoff loop is to pull a richer recipe back here (below).

So the lab's job is the ground **none of those cover**: stacking, tone-stacking, and combos.

## The growth ladder (start simple → grow)

Each rung is playable/committable on its own; stop wherever it stops being fun.

- **Rung 0 — one stage, interactive. ✅ SHIPPED (`distlab`, 2026-07-11).** A held acid drone (one
  `note_on` voice slid along a riff with `note_pitch`, so it's continuous) through ONE drive stage:
  LEFT/RIGHT (or tap the `[DRIVE]` block) picks the mode, UP/DOWN (or drag the bar) rides the amount.
  A live **transfer curve** (input→output, identity ghosted) AND a real **`scope_read` oscilloscope**
  of the post-FX output sit side by side. The chain row shows one `[DRIVE]` block + ghosted `+` slots
  to signal growth. Verified: compiles, fx-frame clean, scope populates from real output.
- **Rung 1 — the tone stack. ✅ SHIPPED (2026-07-11).** EQ *before* and *after* the clipper as a real
  master insert chain: `fx_order(0, {FX_EQ, FX_DRIVE, FX_INST(FX_EQ, 1)})`, with `eq()` = pre
  (instance 0), `drive_insert()` = the dirt, `eq_inst(1, …)` = post. The drive moved from
  per-instrument to the master bus (the chain row now literally maps to `fx_order` — the architecture
  rungs 2–4 build on). A flat param cursor walks all 8 params; tapping a block jumps to it. Pre-EQ
  picks what gets distorted, post-EQ tames the fizz. A/B-verified the EQ alters the spectrum.
- **Rung 2 — cascade. ✅ SHIPPED (2026-07-11).** A SECOND drive stage via `drive_insert_inst(1, …)`
  + `FX_INST(FX_DRIVE, 1)` in the chain → PRE-EQ → DRIVE A → DRIVE B → POST-EQ. Each stage has its
  own mode + amount; the transfer curve draws the COMPOSED shape (B∘A). Stack waveshapers (soft →
  fold, fold → hard) — "overdrive into the amp." A/B-verified B stacks on A. (`shape()` bypasses at
  amt 0 so a dormant stage = identity in the composed curve.)
- **Rung 3 — destruction combos.** Fold `crush` + `tape` into the chain → grit + lo-fi + saturation
  stacked. The "go nuts" rung.
- **Rung 4 — reorderable chain.** Make the stages a `pedalboard`-style drag-to-reorder chain so
  order-dependence is audible (drive→eq→crush vs crush→eq→drive). `fx_order` already models this.
- **Rung 5 — live spectrum.** A spectrum/FFT strip alongside the transfer curve, to *watch* the
  harmonics bloom. (Nice-to-have; the transfer curve alone teaches most of it.)

## Cart-land vs engine — the whole initial lab is CART-LAND (verified 2026-07-11)

The entire ladder (rungs 0–5) needs **no engine changes** — every piece is an existing studio.h API:

- Rungs 0–4 use `instrument_drive`/`_mode`, `drive_insert`/`_inst`, `eq`, `crush`, `tape`, `fx_order`.
- **Rung 5 (spectrum) is cart-land too** thanks to **`scope_read(dst, n)`** — it copies the latest
  N samples of the *actual post-FX output mix* (−1..1) into a cart buffer, zero cost until first
  called (`scope_read2` for stereo/vectorscope). Hand-roll a small FFT on that buffer. A live
  *waveform* scope is nearly free — `scope_read` IS the oscilloscope feed.
- **Transfer curve (rung 0):** the drive shaper math is NOT exposed the way `lfo_value()` is, so to
  draw the exact curve the cart re-implements the 4 formulas (tanh / hard-clip / sin-fold / asym —
  trivial), OR just shows the real output waveform via `scope_read` (truthful, no re-implementation).

So: **build the whole playground in cart-land first**, ship + experiment, and touch `sound.h` ONLY
if we later reach for the three gaps below. No hot-file risk until then.

## Engine gaps (NOT cart work — note as engine todos if we want them)

The genuinely-modern tricks the engine can't do yet — flag, don't fake:

- **Multiband distortion** (FabFilter Saturn's signature) — split into bands, distort each
  differently. Needs a band-splitter in `sound.h`.
- **Drawable / arbitrary transfer curves** — a waveshaper fed a user-drawn lookup table. Would be a
  new `DRIVE_*` kind + a curve buffer.
- **Rectification / octave-up fuzz** — half/full-wave rectify (ASYM is the closest built-in). Cheap
  new mode if wanted.

## Open questions (undecided — decide when we build, not now)

- **Core interaction:** reorderable pedal chain (richest) · fixed 3-stage rig (pre-EQ→drive→post-EQ)
  · dual-stage A/B (narrowest, closest to `drivemodes`+). Leaning: start at a fixed rig or dual-stage,
  grow toward the reorderable chain.
- **Through-voice:** a playable `keybed.h` (play what you distort) · an auto-riff drone (hands-free,
  like `drivemodes`) · both (auto by default, grab a keybed on a key). Leaning: auto-riff first
  (fastest), add keybed later.

## The payoff loop

The lab isn't just a toy: once a stacked recipe sounds great (say **fold → post-EQ → asym**, or a
crush-flavoured "Muffler"), pull it back into a real voice — a two-stage **"Muffler"-style** drive
for the 303 (the acidrack DEEP-page notes already wanted a Devil-Fish-style post-VCA Muffler). Cart
→ recipe → instrument.

## See also

- [`effects-recipes.md`](../guides/effects-recipes.md) — the drive-modes recipe table + crush/tape/EQ
  recipes (the `drivemodes` showcase row).
- [`effects-bus-architecture.md`](effects-bus-architecture.md) — reorderable inserts (Increment A,
  shipped) + the "EQ before AND after distortion" note; the routing model the reorderable rung uses.
- [`audio-notes.md`](audio-notes.md) — the sound HUB; §17 effect ledger, the drive-modes contract.
- Carts: `drivemodes` (the 4-mode reference), `bitcrush`, `pedalboard` (the reorderable-chain
  pattern to copy), `acidrack` (the 303 mode selector + the payoff target).
