# The unison primitive (`instrument_unison`) — design notes

> **STATUS: SHIPPED (2026-07-09).** `instrument_unison` + `instrument_unison_detune` + `LFO_DETUNE` /
> `ENV_DETUNE` are wired end-to-end (studio.h/sound.h/studioDocs.js/shell.js), compile- and
> tune-gated, and the **`supersaw`** showcase cart is built + baked. Decision recorded in
> [ADR-0030](../decisions/0030-unison-is-a-primitive.md); it follows the recipe→macro→engine ladder
> ([ADR-0016](../decisions/0016-combo-organ-recipe-then-macro-or-engine.md),
> [ADR-0017](../decisions/0017-three-macro-core-plus-engine-aux-channel.md)). Stereo **width** is the
> one deliberate follow-up (see below).

## What it is

A first-class **unison** primitive: one slot renders N slightly-detuned copies of its own oscillator,
summed. That beating between the copies is the fat, wide, alive tone a single oscillator can't make —
the JP-8000 Super Saw, the analog-poly "wall." Today carts build this by hand with the **multi-slot
trick**: allocate N extra slots, program them identically, fire the note across all of them, each
`instrument_tune`'d a few cents apart, and let the mixer sum them. It works (zero engine code) but it
makes a cart hand-manage a slot pool and fire every note N times — and it can't be swept or
motion-recorded without the cart threading detune through every voice by hand.

The promotion collapses that into: **one slot, one call.**

## Why promote — the honest trigger

The rule is *≥2 carts want it → promote* (recipe→macro→engine). The earlier trigger list overstated
the case, and correcting it matters — **verified by reading each cart's voice-firing, not a grep:**

- **moog** does NOT migrate. Its three oscillators are *independent voices*, not detuned copies: each
  has its own wave, octave, level; OSC2 has hard sync; there's per-voice random drift; and detune
  rides live per-frame via `note_pitch`. That's multi-**voice** architecture. `instrument_unison`
  gives symmetric copies of *one* waveform — it can't express `{saw@0, saw+0.08, square@−1oct}`. moog
  keeps its hand-rolled 3-slot design.
- **mt70** does NOT migrate, and shouldn't. It stacks sines at **harmonic ratios** (+19.02 semitones =
  ratio 3.0) with per-partial volume and decay. That's **additive synthesis**, not unison detune —
  folding it in would be a category error.

The **genuine** unison demand, the real promotion trigger:

| cart | today | with the primitive |
|---|---|---|
| `supersaw` (showcase, unbuilt) | — | the reason it exists: 7 detuned saws, the detune-bloom gesture |
| `motionbox` lead | wants it; dreads the slot-pool refactor | lead stays **one slot**, refactor evaporates |
| `groovebox` pad | single-slot `instrument_tune(SL_PAD, 0.12)` shimmer | a real unison spread |
| `filterenv` saw pair | `SLOT`+`SLOT2` hand-detuned | one call (its 3rd slot is an octave-down triangle SUB — not unison, stays separate) |

So the primitive is promoted on **new, clean demand** — not on four carts collapsing into it.

## The API — locked, and all precedent

`instrument_tune(slot, semitones)` already exists but is a single **constant per-slot offset**, not
internal unison — it shifts a slot's pitch, it doesn't spread copies. The primitive is new:

```c
void instrument_unison(int slot, int voices, float detune);   // configure the stack
void instrument_unison_detune(int slot, float detune);        // ride the spread alone — the bloom (LIVE)
#define LFO_DETUNE 8   // sweep the unison spread — the breathing/chorusing width wobble
#define ENV_DETUNE 6   // one-shot spread contour — THE bloom: one thin saw opening into a wall of 7 on the attack
```

- **`voices`** (1..7) — structural, **note-on-face**: it applies at the next `note_on`, exactly the way
  moog/`instrument()` snapshot the waveform. Changing voice count live would alloc/free oscillators
  and click; taking it at the next note is honest and matches the house rule.
- **`detune`** — the spread, in **semitones** (fractions are the point — `0.06` = shimmer, up toward a
  wide supersaw), matching `instrument_tune`'s units because they're the same family.
- **The two-call shape mirrors `instrument_filter(...)` + `note_cutoff`/`note_res`:** a bundled call to
  configure, an atomic scalar to *ride* the one value that moves. `note_cutoff` exists for exactly this
  reason — a live-ride value bundled with structural params gets its own atomic twin.
- **The twin is slot-based, not handle-based** (`instrument_unison_detune(slot, …)`, not
  `note_detune(handle, …)`) — mirroring `instrument_tune`, unison detune's nearest sibling (also a
  detune, also "LIVE — every sounding voice on the slot bends," also no handle twin). Unison width is a
  *patch* property, not a per-key one, so slot-level is the honest match — and it's what `motionbox`'s
  slot-oriented per-hit motion wants.
- **`LFO_DETUNE`/`ENV_DETUNE`** mirror the macro dests (`LFO_/ENV_HARMONICS/TIMBRE/MORPH`), which exist
  precisely so a sweep can be **motion-recorded**. `ENV_DETUNE` on the attack literally *is* the bloom,
  and it drops into `motionbox`'s motion lanes with zero special-casing — the cart's soul (timbre-as-
  motion) applied to width.

### The bloom, in a cart

```c
instrument_unison(LEAD, 7, 0.0f);          // 7 saws, starting tight
// ride it — one thin saw opening into a wall of seven:
instrument_unison_detune(LEAD, knob);      // motionbox writes `knob` into a motion lane per hit
// or declaratively, motion-recordable:
instrument_env(LEAD, 0, ENV_DETUNE, 0, 400, 1.0f);   // spread blooms over the first 400ms of each note
```

## How it shipped (the engine, briefly)

The N copies render **inside one slot's voice**: `v->phase` is the center (exact pitch), plus up to six
extra phase accumulators (`uni_ph[]`, mirroring the existing hard-sync `sync_ph`) advanced at detuned
increments and summed, then `× 1/√N` so loudness stays put. The detune multiplier is the linearized
`2^(x/12) ≈ 1 + 0.0577623·x` — near unity the beating is what matters, not exact cents, and it stays
bit-portable (no per-sample transcendental). Wavetable slots only; unison takes precedence over hard
sync when both are set. Off (`voices ≤ 1`) is byte-identical — the branch never runs. Plumbed through
the request queue (`SR_INSTR_UNISON` / `SR_INSTR_UNISON_DETUNE`) like every other slot param
([ADR-0027](../decisions/0027-sound-state-flows-through-the-request-queue.md)).

## Deferred (deliberately)

- **Stereo width.** Half the JP-8000 magic is the detuned voices panned across the field. But
  `instrument_sync`/`note_sync`, `instrument_drive`/`note_drive` all arrived one scalar at a time —
  add `instrument_unison_width(slot, w)` **if** mono unison proves not enough, sized against the
  `supersaw` showcase. Not front-loaded into v1.
- **CPU.** Unison multiplies the per-sample oscillator cost by the voice count on that slot (capped at
  7, opt-in). Watch `profile-fleet` if a cart stacks unison on heavy polyphony.

## The showcase — `supersaw`

A Roland JP-8000 Super Saw trance lead: 7 detuned saws, the most-unison synth ever made. The
**detune-bloom gesture** — one thin saw opening into a wall of seven — *is* the primitive's reason to
exist, and it's the extreme end that moog's 3-oscillator fat doesn't reach. It's what proves the
primitive earns its place the moment it lands.

## See also

- [`instrument-engines.md`](instrument-engines.md) — the rich-instrument port program + the 3-macro discipline.
- [`voice-engine.md`](voice-engine.md) — a worked promotion (recipe → locked public API) in the same shape.
- `motionbox` — the `de:meta` TODO carries the cart-side view (ENGINE PROMOTION item).
- Hand-rolled references still using the multi-slot trick: `moog`, `filterenv`, `groovebox`.
