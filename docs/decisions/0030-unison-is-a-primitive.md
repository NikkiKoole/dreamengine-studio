# 0030 — Unison is a primitive (`instrument_unison`), not a per-cart multi-slot hand-roll
Date: 2026-07-09 · Status: accepted

## Context
A fat, wide analog voice — the JP-8000 Super Saw, the poly "wall" — comes from summing several
slightly-detuned copies of the same oscillator; the beating between them is the life. dreamengine has
no unison primitive, so carts build it with the **multi-slot trick**: grab N extra instrument slots,
program them identically, fire every note across all of them, each `instrument_tune`'d a few cents
apart, and let the mixer sum them. It works (zero engine code) but it makes a cart hand-manage a slot
pool and fire each note N times, and the spread can't be swept or motion-recorded without threading
detune through every voice by hand.

The promotion trigger is the usual rule — *≥2 carts want it → promote*
([ADR-0016](0016-combo-organ-recipe-then-macro-or-engine.md),
[ADR-0017](0017-three-macro-core-plus-engine-aux-channel.md), recipe→macro→engine). An earlier
statement of the trigger was **overstated**, and correcting it is half the point of this record.
Verified by reading each cart's voice-firing (not a grep):

- **moog does NOT migrate.** Its three oscillators are *independent voices* — each its own wave,
  octave, level; OSC2 hard-synced; per-voice random drift; detune ridden live per-frame via
  `note_pitch`. That's multi-**voice** architecture, not detuned copies of one wave.
- **mt70 does NOT migrate**, and shouldn't. It stacks sines at **harmonic ratios** (+19.02 semitones =
  ratio 3.0) with per-partial volume and decay — **additive synthesis**, a different thing.

The genuine unison demand is `groovebox`'s pad (single-slot `instrument_tune` shimmer), `filterenv`'s
saw *pair* (its 3rd slot is an octave-down triangle sub — not unison), `motionbox`'s lead, and the
`supersaw` showcase. So the primitive is promoted on **new, clean demand**, not on four carts
collapsing into it.

`instrument_tune(slot, semitones)` already exists but is a single **constant per-slot pitch offset**,
not internal unison — it can't stand in.

## Decision
Add a first-class unison primitive, shaped to mirror existing engine conventions on every axis (see
the full design in [`unison-primitive.md`](../design/unison-primitive.md)):

```c
void instrument_unison(int slot, int voices, float detune);   // configure: voices (note-on-face) + spread (semitones)
void instrument_unison_detune(int slot, float detune);        // ride the spread alone — the bloom (LIVE, per slot)
#define LFO_DETUNE   // sweep the unison spread — the breathing width wobble
#define ENV_DETUNE   // one-shot spread contour — the attack bloom
```

- **bundled configure + atomic live twin** mirrors `instrument_filter(...)` + `note_cutoff`/`note_res`
  (the live-ride field gets its own atomic setter).
- **the twin is slot-based** (`instrument_unison_detune`), mirroring `instrument_tune` — unison
  detune's nearest sibling, also slot-live, also no handle twin. Unison width is a *patch* property,
  not per-key.
- **`LFO_DETUNE`/`ENV_DETUNE`** mirror the macro dests (`LFO_/ENV_HARMONICS/TIMBRE/MORPH`), which exist
  so a sweep can be motion-recorded — `ENV_DETUNE` on the attack *is* the bloom.

Engine shape: one slot renders N (1..7) detuned copies of its own **wavetable** oscillator, summed and
loudness-normalized. Wavetable slots only — modeled engines (`INSTR_PLUCK`+) are out of scope; when
unison is on, it takes precedence over hard sync (the two are alternative fatteners).

## What we did NOT do
- **No handle-based `note_detune`.** Detune is a patch property; the slot-level twin covers the ride,
  matching `instrument_tune`.
- **No stereo width in v1.** Half the JP-8000 magic is voices panned across the field, but
  `instrument_sync`/`drive` twins arrived one scalar at a time — `instrument_unison_width(slot, w)` is
  a fast-follow *if* mono unison proves not enough, sized against the `supersaw` showcase.
- **Did NOT fold in moog / mt70.** They keep their hand-rolled designs (see Context); forcing them
  into a unison primitive would be a regression.

## Consequences
- Carts stop hand-managing a slot pool for fatness. `motionbox`'s lead becomes a one-slot unison — the
  slot-pool refactor its TODO dreaded evaporates.
- `groovebox` / `filterenv` can simplify to the primitive over time (not required; no rush).
- Showcase cart `supersaw` proves it: 7 detuned saws, the detune-bloom gesture that moog's 3-osc fat
  can't reach.
- CPU: unison multiplies the per-sample oscillator cost by the voice count on that slot; N is capped at
  7 and it's opt-in (off = byte-identical). Watch `profile-fleet` if a cart stacks unison on polyphony.
