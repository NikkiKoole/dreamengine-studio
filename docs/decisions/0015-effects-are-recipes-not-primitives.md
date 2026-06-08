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
whammy = `note_pitch`/`note_glide`; amp/cab sim = drive + lowpass; limiter = the master
soft-clip. **EQ** splits in two: the *cut* half (and the whole DJ low/mid/high **kill**
set) is exactly the four filter modes — kill lows = `FILTER_HIGH`, kill highs =
`FILTER_LOW`, scoop mids = `FILTER_NOTCH` — and a band *boost* has a crude recipe in the
SVF's resonance peak (`FILTER_LOW` at 120Hz, res 8 = a low shelf-ish bump; the 303
squelch is "EQ boost as instrument").

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
`wave_set()` and ambition); **channel-strip EQ** (boost-this-band-leave-the-rest:
2–3 extra biquads per voice + more API for a tool whose job is carving *dense* mixes —
ours are 16 simple waves, and **register discipline is the real EQ at this fidelity**:
bass plays low, lead plays high, waveform/duty/drive set the brightness. If the cart
corpus ever proves otherwise — several music carts hand-rolling double-filter tricks,
the 0014 evidence pattern in reverse — the admitted shape is one function,
`instrument_eq(slot, low, mid, high)`, and a new interrogation here).

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

## Correction (2026-06-08) — wah is TWO things; the §35 audit oversimplified

The audit above files wah under "already covered today" and claims *touch wah =
`instrument_env(ENV_CUTOFF)` = exactly an envelope follower.* Rendering navkit's actual wah
(`tools/navkit-render.c`) proved that wrong. The truth, split:

- **Simple wah IS a per-voice recipe** (still true): pedal = `FILTER_BAND` + res + live
  `note_cutoff`; rhythmic auto-wah = `LFO_CUTOFF`; the funky-clav *quack* = a FAST per-note
  `ENV_CUTOFF` snap (~100ms) on a resonant filter. The clav showcase (`epiano.c`) uses this.
- **The realistic "woah woah" auto-wah is a BUS effect** — NOT covered today. navkit's is a
  bandpass on the **summed mix** with an **exponential** sweep (300→2500) and an envelope
  *follower* tracking the **whole performance**. Two things a per-voice filter can't do: sweep
  a chord *coherently* (one vowel for all notes), and *pump with the groove* (follow the summed
  amplitude). That belongs in the **effects-bus layer ([instrument-engines §8.10](../design/instrument-engines.md))**,
  where §8.10 already (correctly) lists wah as an aux-bus insert — the framing that conflicted
  with this audit's "per-voice, done" framing. §8.10 wins.
- A per-note env is **not** an envelope follower (per-note retrigger vs continuous
  amplitude-tracking). The follower was built as a per-voice primitive (`instrument_follow`)
  but its real home is bus-level; per-voice it's the pale version (see sound-handoff.md).

Net: the *decision* (effects are recipes, roster closed) stands; this corrects the **wah
classification** — the good auto-wah is a deferred bus effect, not a covered per-voice one.
