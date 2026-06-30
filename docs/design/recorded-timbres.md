# Faking a "recording" — the Mellotron problem

STATUS: SHIPPED — mellotron.c realizes the attack-transient + ensemble + tape recipe that fakes recorded timbres.

How to make a *synthesized* voice read as a *recorded* instrument, in an engine that has **no sample
playback**. Written up off the `tapeloop` cart (which fakes tape-strings/choir/flute with drawn
waves + `tape()`), as the design notes for a more-convincing **`mellotron` cart**.

> **Built.** The `mellotron` cart (`tools/carts/mellotron.c`) now implements the recipe below: drawn
> STRINGS/CHOIR/FLUTE/BRASS waves + a noise attack-chiff + `chorus()` ensemble + `tape()`/`reverb()`,
> with the **~8-second forced release** as the headline behaviour and an M400-cabinet look. Recipe
> rows in [`../guides/instrument-recipes.md`](../guides/instrument-recipes.md) (`user0/mellotron-*`);
> shelf row in [`../guides/instrument-carts.md`](../guides/instrument-carts.md). This doc stays as the
> *why* behind the build.

> **Scope: design exploration, not a spec.** The recipe here is all *cart-side* — no engine work. It
> composes things we already shipped (`wave_set`, `instrument_tune`, the LFOs, `chorus`/`tape`/
> `reverb`). The honest endpoint is stated at the bottom.

## The problem

We're a code-first synthesis engine and **we deliberately don't do sample playback** (no asset
storage, no PCM banks — it's against the fantasy-console ethos; see [decisions](../decisions/)). So
a "recording" can only ever be *faked*. The cheapest fake — a single-cycle drawn wave (`wave_set` →
`INSTR_USER0`, as `tapeloop` does) — gives you the right **spectrum** but it's a *static, looping*
oscillator: same shape every cycle, no onset, no movement. The ear hears "synth," not "a tape of a
string section." `tapeloop` gets away with it because `tape()`'s wow/flutter/saturation + the slow
echo loop supply the wear and motion the wave itself lacks — but that's leaning on the *effect* to
hide a thin *source*.

## Why a real recording sounds real (what we have to fake)

Four things a microphone captures that a static wavetable doesn't:

1. **An attack transient** — the *non-pitched* onset: bow scrape, breath chiff, hammer thunk, pick
   click, the tape playback head engaging. This is the single biggest "tell." The ear identifies an
   instrument largely by its attack.
2. **Ensemble + movement** — a real string *section* is many players slightly apart in pitch and
   time, each with their own vibrato and drift. Even a solo player has continuous micro-pitch and
   timbre movement. A wavetable is one perfectly-steady voice.
3. **Noise / air** — bow rosin noise, breath turbulence, room tone, tape hiss. A little broadband
   noise under the tone reads as "this was recorded in a place."
4. **Spectral evolution** — the timbre changes over the note (the attack is bright, the sustain
   settles, the release dims). A looped single cycle is spectrally frozen.

## The levers we already have (no engine work)

Compose these on top of a drawn `wave_set` source:

1. **Attack-transient layering** — fire a short (~20–40 ms) percussive onset *on the same note* as
   the sustaining drawn voice: a noise burst (`INSTR_NOISE`, fast decay) for breath/bow, or a short
   bright click. This is LA synthesis's whole trick, and we already do it in the 909 kick / cr78
   snare. **It's the highest-value lever** — the attack sells the instrument. (Already flagged as a
   "free recipe win" in [`sound-next-steps.md`](sound-next-steps.md).)
2. **Ensemble detune** — play the note on 2–3 slots a few cents apart (`instrument_tune ±0.05–0.12`,
   the gamelan *ombak* trick) so one wave reads as a *section*; the beating is the ensemble. Or send
   it through **`chorus()`** (the BBD ensemble) — `tapeloop`/`solina` lean on this. Section blend is
   the other "free recipe win" in sound-next-steps.
3. **Movement** — `LFO_PITCH` for vibrato (slow, shallow), a slow `LFO_CUTOFF` drift so the timbre
   isn't frozen, and a slight attack swell (`instrument` attack ms) so notes *bloom* rather than
   click on.
4. **Noise / air** — a faint continuous `INSTR_NOISE` layer (very low volume, lowpassed) under the
   tone = breath/room. `tape()` saturation already adds a hint of this character.
5. **Tape + room** — `tape()` (wow/flutter/saturation/HF-rolloff) = the worn-tape layer; `reverb()`
   = the room the recording was made in. Both shipped; both per-instrument now.

The realism is the **stack**, not any single piece. A drawn wave alone is synthetic; drawn wave +
attack transient + ensemble + a little movement + tape + reverb crosses into "that's a recording."

## The Mellotron specifically

A Mellotron *is* literally tape recordings — each key plays back a length of tape with a real
strings/choir/flute/brass recording on it. To fake one convincingly:

- **Drawn voices** — strings / choir (vowel-formant) / flute / brass spectra via `wave_set`
  (`tapeloop` already builds the first three — reuse them).
- **Attack transient** per voice — the tape-head engage + the instrument's own onset (a soft noise
  chiff for flute/choir, a bow-scrape tick for strings).
- **Ensemble detune** — the recorded ensembles (3 violins, an 8-voice choir) → a detune stack /
  `chorus()`.
- **The ~8-second tape limit** — *the* signature Mellotron quirk: each key's tape is finite, so a
  held note **cannot sustain past ~8 s** — it runs out and stops. Model as a forced release at ~8 s
  regardless of whether the key is still held. (This is what makes Mellotron pads *breathe* in
  8-second waves.)
- **Per-note pitch instability** — slight wow on each note's start (tape speed settling) — `tape()`
  wow does this globally; a tiny per-note `instrument_tune` jitter adds the rest.
- **`tape()` + `reverb()`** over the top.

### Cart sketch — `mellotron`
A keybed (`note_on`/`note_off`, multitouch), a VOICE selector (strings/choir/flute/brass via the
drawn waves), the **8-second forced release** as the headline behaviour, attack-transient + ensemble
stack baked into each voice, `tape()` + `reverb()` sends. The "more real" payoff over `tapeloop` is
the per-note attack + the 8s limit + playability (it's an instrument, not a self-playing loop).
Closest cousins to copy the chassis from: `tapeloop` (the drawn voices + tape) and a held-notes /
keybed cart (`heldnotes`/`touchpiano`) per the [instrument-carts shelf](../guides/instrument-carts.md).

## The honest limit

Real sample playback would get you all the way there in one step — but it's a deliberate non-goal
(asset storage + the code-first synthesis identity). Everything above is the *synthesis* path to the
same destination: you don't reproduce the recording, you reconstruct the *cues* the ear uses to
believe one. For a Mellotron that's plenty, because the Mellotron's own sound is already a degraded,
wobbly, finite tape — imperfection is the aesthetic, and imperfection is exactly what we're good at
faking.

*Companion: [`sound-next-steps.md`](sound-next-steps.md) "Free recipe wins"; the drawn-voice source
in `tools/carts/tapeloop.c`; the recipe palette [`../guides/instrument-recipes.md`](../guides/instrument-recipes.md).*
