# 0015 — Effects are recipes, not primitives: the roster is closed
Date: 2026-06-05 · Status: accepted

## Context
Drive shipped (`instrument_drive`/`note_drive`, [audio-notes §17](../design/audio-notes.md))
and the echo + reverb buses are queued behind it. That raised the obvious worry: **does
every effect cost a new API family?** Drive cost 2 functions; echo will cost 3, reverb
2–3. Extrapolated across a DAW-style effect roster (chorus, flanger, phaser, wah,
compressor, ring mod, octaver, …) that's unbounded API growth — the thing the engine-macro
discipline ("three knobs, six functions, forever", §8.1.1) exists to prevent.

Two shapes were on the table:

1. **Named functions per primitive** (the shipped path): `instrument_drive`, `instrument_echo`, …
2. **One generic family** that never grows: `instrument_fx(slot, FX_DRIVE, x)` /
   `instrument_send(slot, BUS_ECHO, x)`.

## Decision
Keep **named functions**, and close the primitive roster at the §17 taxonomy:

| layer | primitives | API cost |
|---|---|---|
| voice insert | drive ✓ · bitcrush (maybe master-internal) | 2 + 0–2 |
| shared bus | echo · reverb — **capped at two buses by design** (one rhythm, one room) | 3 + 2–3 |
| oscillator param | detune | 2 |
| master stage | soft-clip ✓ · (wow/flutter buffer, if tape ever lands) | 0 — internal |

**~12 functions, forever, for the entire effects story.** Everything else people call an
"effect" is either a *recipe* of these primitives or a *refusal with the musical need
covered elsewhere*. A new primitive must prove it cannot be a recipe.

## Why
The pedalboard audit. Running the classic effects through the four layers:

**Already covered today** — distortion/overdrive/fuzz = the drive knob's range; wah-wah
in all three forms (pedal wah = `FILTER_BAND` + res + live `note_cutoff`, auto-wah =
`LFO_CUTOFF`, touch wah = `instrument_env(ENV_CUTOFF)` — it retriggers per note, which
is exactly what an envelope-follower does); tremolo = `LFO_VOLUME`; vibrato = `LFO_PITCH`;
EQ = `instrument_filter`; whammy = `note_pitch`/`note_glide`; amp/cab sim = drive + lowpass;
limiter = the master soft-clip.

**Unlocked by the queued §17 work** — slapback/dub/tape echo → echo bus; room/hall →
reverb bus; chorus/unison/supersaw → **detune** (two notes a few cents apart *is* chorus);
lo-fi → bitcrush; sidechain pump → cart recipe (`note_vol` ducked on the kick step);
stutter/slicer → `note_vol` chopped by the beat clock.

**Worked example: the Leslie.** Rotating speaker = tremolo (`LFO_VOLUME`) + Doppler
(`LFO_PITCH`) + brightness swirl (`LFO_CUTOFF`) + pushed-tube growl (`instrument_drive`).
A slot has exactly three LFOs — the full budget, spent on one effect. The iconic
chorale↔tremolo spin-up is a cart-side `lerp` of the shared LFO rate via `note_lfo()`,
which keeps phase and so never clicks — the lerp *is* the horn's mechanical inertia.
No `instrument_leslie`, ever.

**Deliberate refusals** — flanger (the one true gap: needs a modulated short delay;
if the master wow/flutter buffer ever lands for tape, flanger falls out of it free);
ring mod (its musical job — metallic inharmonic clang — is already INSTR_FM's
off-integer detents); octaver (a pedal exists because a guitar has one string — we
have polyphony: play the octave); auto-pan/stereo width (the engine is mono — console
identity, not a missing feature); granular/shimmer/freeze (DAW-tier; a cart has
`wave_set()` and ambition).

**Why named functions despite the growth?** Because `studio.h`'s one-liner-per-function
style *is the documentation system* — autocomplete, hover and the help tab all hang off
the bare name. `instrument_drive` teaches at the moment of typing; `instrument_fx(slot,
FX_DRIVE, …)` teaches enums. A 12-function ceiling doesn't justify giving up legibility,
and the engine macros set the same precedent (named `instrument_harmonics/timbre/morph`,
not `instrument_macro(slot, which, x)`). The defense against bloat is not a calling
convention — it's **refusing to admit new primitives**.

## Consequences
- The moment anyone is tempted to add `instrument_flanger` / `instrument_chorus` /
  `instrument_wah`, the answer is this document, not four-place wiring.
- Chorus, tape, Leslie, wah, sidechain et al. belong in **docs/guides or carts as
  recipes** (a hammond cart with the Leslie ramp would exercise drive + all three LFOs).
- Echo and reverb, when they land, complete the roster; their API shapes are sketched
  in [audio-notes §17](../design/audio-notes.md) and bounded by this decision.
- Supersedes nothing; extends the §8.1.1 macro discipline from engines to effects.
