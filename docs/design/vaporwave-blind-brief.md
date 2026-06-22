# Vaporwave — the blind band brief

> Phase-1 intent-first brief for a new radio station (`vapor.c`), written from the music
> before reading any cousin cart (the firewall). Companion to the runbook
> [`../guides/radio-station-howto.md`](../guides/radio-station-howto.md). A **genre** station
> (one coherent hazy texture the seed re-keys), and crucially an **EFFECTS-FORWARD** one —
> the production is the point, so [`../guides/effects-recipes.md`](../guides/effects-recipes.md)
> is half the build.

## The one thing that must be true

**The processing IS the genre.** Vaporwave is 80s/90s smooth-jazz / mall-muzak / city-pop
SLOWED DOWN and drowned in reverb + chorus + tape warble + echo, looped hypnotically — the
"screwed", dead-battery-walkman, dead-mall haze. We have no sampler, so we *generate* the
lounge source (lush Rhodes + pad + a sax/bell dab) and run it through that exact FX chain.
The composition is deliberately simple and repetitive; **the drench and the slow wobble are
the band.**

## The record, from the music

Macintosh Plus (*Floral Shoppe*), Blank Banshee, 2814, Saint Pepsi, Eco Virtual. The feel:
nostalgic, hazy, melancholic, plasticky, retro-futurist. Slow (~60–85 effective bpm). The parts:

- **Lush lounge keys** — Rhodes / FM electric piano, the core; smooth, warm, a little detuned.
- **Big synth pad** — DX/Solina-ish wash under everything.
- **A smooth melodic dab** — a sax line or a soft synth lead, sparse, reverbed.
- **Bell / chime sparkle** — the mall PA / elevator twinkle.
- **Round smooth bass** — slow, simple.
- **Slow gated machine beat** (optional) — often a simple reverbed kit; "mallsoft" is beatless.
- **The texture** — vinyl crackle / tape hiss under it all.
- **A short 2–4 chord lush loop** — extended jazz (maj7 / m9 / 9 / 11), repeated hypnotically.

Harmony: lush, smooth, lounge-y, *static* — a few extended chords looped, not a progression
that "goes" anywhere. The haze, not the development, is the point.

## The brains

- **THE DRENCH — the processing as the form (the headline).** The whole signal runs through a
  set-and-hold vaporwave chain: big **reverb** + **chorus** + **tape** (heavy wow/flutter/sat) +
  **echo** + a touch of **bitcrush** (the lo-fi). This is vaporwave as the *effects showcase* on
  a generative lounge loop — "drench it" as a station.
- **THE SLOW WOBBLE (the new live gesture) — `varispeed`.** The "slowed/screwed" pitch sag: a
  slow, gentle **`varispeed`** drift (it's sweep-safe — ridden live each frame, glides cleanly),
  so the whole mix wavers like a tape running slightly slow / a dying battery. The signature.
  *(Everything else is set-and-hold — only varispeed and `note_*` ride live; copy groovebox's
  apply_fx gate.)*
- **HAZY-LOOP CHORD BRAIN.** A short 2–4 chord lush-extended loop (maj7/m9/9/11), slow, repeated;
  no functional cadence — static and dreamy. The seed rolls the loop + key.
- **HYPNOTIC FORM.** Repetition with the FX breathing (filter drift, the wobble, layers fading
  in/out over minutes); a "tape side" that just loops. Minimal sections.

## The voices (palette — gloss + effects native, no gaps)

| imagined voice | engine + recipe | notes |
|---|---|---|
| **lounge keys** | `INSTR_EPIANO` (Rhodes) + chorus | the core; warm, detuned, drenched |
| **lush pad** | `INSTR_SAW` string-machine / `INSTR_PD` | the wash bed |
| **smooth lead dab** | `INSTR_REED` sax / soft `INSTR_SINE` | sparse, reverbed |
| **bell / chime** | `INSTR_MALLET` / `INSTR_FM` | the mall sparkle |
| **smooth bass** | `INSTR_SINE` / `INSTR_SAW`+LP | slow, round |
| **slow kit (opt)** | `INSTR_SINE` kick · `INSTR_NOISE` snare, gated/reverbed | mallsoft = none |
| **the texture** | `INSTR_NOISE` vinyl-crackle / tape-hiss bed | lo-fi haze |
| **the production** | `reverb` big · `chorus` · `tape` heavy · `echo` · `crush` light · `varispeed` slow wobble | the genre itself |

**No gap** — we own every voice and every effect; vaporwave is maximally engine-native.

## NOT SAMEY — roll the sub-style

Vaporwave is a family; roll the per-song MODE (each sets tempo, beat, FX amounts, palette):
- **CLASSIC** — *Floral Shoppe* lounge: Rhodes + sax, a slow gated beat, heavy chorus + tape.
- **MALLSOFT** — beatless dead-mall: max reverb/echo, bell sparkle, a PA-announcement haze.
- **UTOPIAN / ECO** — brighter, eco-virtual pads, cleaner, hopeful (less crush).
- **FUTURE-FUNK-LITE** — a touch faster + funkier (chopped Rhodes stabs), less wobble.

Plus per-song key, the lush loop, and the FX-amount rolls (reverb size, tape wow, crush, the
wobble depth).

## Player knobs (radio.h chrome)

- **haze** — the wet/drench dial (reverb + echo + wobble depth together).
- **tone**, **tempo**; SPACE re-rolls; **B** band (keys/lead/beat). A "WOBBLE" feel if it fits.

## Window / face — the vaporwave scene

The iconic look: a **sunset gradient** (pink → purple → teal), a **wireframe grid floor**
receding to the horizon, a **marble bust** silhouette, a palm, maybe glitching katakana / a
Windows-95 title bar. Pink/teal/purple. The whole thing can **wobble/scanline** subtly with the
varispeed drift — the slowed-tape feel made visible. アアア 1 9 9 5.

## What this station adds (why it earns the slot)

- **The first EFFECTS-FORWARD station** — where the FX chain *is* the genre (the vaporwave
  drench on a generative lounge loop); a different axis from every harmony/rhythm station.
- **The `varispeed` slow-wobble** as a live musical gesture (the slowed/screwed sag) — a new use.
- A **hazy, nostalgic, plasticky** mood the dial doesn't have, and a gloriously kitsch face.
- Maximally **engine-native** — no faking, the "so obvious it builds itself" station.

> **Phase-3 chassis hint** (not yet read): a slow metered (or beatless, for mallsoft) station on
> the `radio.h` clock — a slow cousin of `citypop`/`ambient`; the FX chain set once per song/mode
> (set-and-hold, gated), varispeed ridden live for the wobble. Voice the lounge loop from this brief.
