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
**in its simple forms** (pedal wah = `FILTER_BAND` + res + live `note_cutoff`; rhythmic
auto-wah = `LFO_CUTOFF`; the funky-clav *quack* = a fast per-note `ENV_CUTOFF` snap, ~100ms,
on a resonant filter — the `epiano.c` clav uses this) — **but the realistic "woah woah"
auto-wah is NOT covered here; it's a deferred bus effect** (a bandpass on the *summed mix*
with an exponential sweep + an envelope follower tracking the whole performance — a per-voice
filter can do neither: it can't sweep a chord coherently or pump with the groove;
[instrument-engines §8.10](../design/instrument-engines.md), confirmed 2026-06-08 by rendering
navkit). NB a per-note `ENV_CUTOFF` is **not** an envelope follower — per-note retrigger ≠
continuous amplitude-tracking. tremolo = `LFO_VOLUME`; vibrato = `LFO_PITCH`;
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

## Correction history

- **2026-06-08 — wah classification (now folded into "Why" above).** The original audit filed
  wah as fully "already covered today" and called a per-note `ENV_CUTOFF` an envelope follower.
  Rendering navkit's actual wah (`tools/navkit-render.c`) split it: **simple** wah is a per-voice
  recipe, but the realistic "woah woah" **auto-wah is a deferred bus effect** (summed-mix bandpass
  + follower — [instrument-engines §8.10](../design/instrument-engines.md)), and a per-note env is
  not a follower. The "Why → Already covered today" wah clause now states this directly; the
  *decision itself* (effects are recipes, roster closed) was never affected.

- **2026-06-10 — the modulated-delay buffer landed early (via chorus), and the decision held.**
  This doc reserved a single master "wow/flutter buffer" (the master-stage row) as the home for a
  real modulated-delay chorus + flanger + tape-wow, deferred until tape, and ruled plain chorus =
  `detune`. The Juno showcase brought the *real* BBD chorus forward, so that buffer **shipped now,
  via chorus instead of tape** (`chorus(rate, depth, mix)`, a navkit BBD port). Crucially it landed
  exactly as the roster prescribes — **a MASTER-STAGE INSERT (the reserved buffer), NOT a third
  send bus** — so the "shared buses capped at two (echo, reverb)" line is untouched, and there is
  **no `instrument_chorus`** (master-wide; per-part waits for aux routing). Flanger (= same buffer +
  feedback + short delay) and tape wow/flutter (= same buffer + slow LFO) are now queued *uses of
  this one buffer*, not new primitives — the "write the mod-delay once, place it many ways"
  discipline. So the decision didn't bend: chorus is a recipe/use of a rostered primitive, the
  buffer was always sanctioned, only its *timing* moved (chorus-first, not tape-first). The
  detune-as-cheap-chorus note still stands as the zero-cost stand-in. Build: [instrument-engines
  §8.10](../design/instrument-engines.md) build-state; showcase: the `juno` cart.

- **2026-06-10 — the "one true gap" (flanger) is closed, exactly as predicted.** The Deliberate-
  refusals list called flanger "the one true gap: needs a modulated short delay; if the master
  wow/flutter buffer ever lands for tape, flanger falls out of it free." Chorus landed that
  modulated-delay technique (above), so flanger shipped as its **second use** —
  `flanger(rate, depth, feedback, mix)`, a MASTER INSERT, no new primitive. One refinement to the
  earlier wording: chorus and flanger don't share one *physical buffer* (different sizes, both can
  run at once); they share the modulated-delay **technique** — the Hermite read was generalized
  (`moddel_hermite(buf, len, pos)`) so each effect reads its own buffer through one helper ("write
  the technique once, instantiate per effect," like the SVF serving filter/formant/wah). The
  decision is *vindicated*, not bent: flanger was always a use of the rostered mod-delay primitive,
  and it cost zero new roster entries. **Tape** is the queued third use (slow wow/flutter LFO +
  saturation). Build: [instrument-engines §8.10](../design/instrument-engines.md); showcase: the
  `mistress` cart (EHX Electric Mistress).

- **2026-06-11 — the AUX-routing step landed; the send/insert split is now physical.** Per-instrument
  inserts shipped (`instrument_chorus`/`instrument_flanger(slot,…)`, an 8-bus pool). This makes the
  send-vs-insert distinction the roster always implied **concrete**: **inserts (chorus/flanger) get
  per-bus state** (each routed instrument its own comb buffer — that's what "the signal passes
  *through* it" requires), while **sends (echo/reverb) stay ONE shared tank** with per-slot send
  coefficients. Per-bus reverb/echo was explicitly **rejected**, not deferred: it would mean N
  separate rooms/echoes (musically wrong — one room is the point — and the echo line ×8 ≈ 2.7 MB) and
  would break the per-slot `instrument_echo`/`instrument_reverb` a dozen carts use. So the two-bus
  *send* cap still holds (echo + reverb, one tank each); the bus *pool* is an insert-routing
  mechanism, a different axis. No new primitives — routing, not roster. Build:
  [instrument-engines §8.10](../design/instrument-engines.md).

- **2026-06-11 — the reserved "wow/flutter buffer" landed, as tape.** The master-stage roster row
  listed "soft-clip ✓ · (wow/flutter buffer, if tape ever lands)" — and tape ever landed.
  `tape(wow, flutter, saturation)` is the **third use of the modulated-delay technique** (after
  chorus + flanger), so the buffer the chorus/flanger story kept invoking now has its namesake
  customer. Zero new roster entries: tape = the rostered mod-delay primitive (wow/flutter) +
  saturation (a soft-clip nonlinearity, already rostered as the master-stage entry) + a baked HF
  rolloff (a filter, rostered). The effects roster is now essentially built out — echo, reverb,
  chorus, flanger, tape — and the remaining items (auto-wah bus, formant/vocoder, ring-mod,
  bitcrush, leslie recipe, compression) are all either recipes, deferrals, or unrostered candidates
  exactly as this decision laid out. Build: [instrument-engines §8.10](../design/instrument-engines.md);
  showcase: the `tapeloop` cart (Frippertronics).
