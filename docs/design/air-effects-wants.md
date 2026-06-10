# AIR — the voices/effects I wanted but can't have yet

The blind-band brief for the **AIR** station (Moon Safari; an *artist* station, five song
archetypes) named an ideal lineup *from the music*, before reading any cousin cart — the
intent-first discipline in [`../guides/cart-authoring-prompt.md`](../guides/cart-authoring-prompt.md).
The brief itself is [`air-blind-brief.md`](air-blind-brief.md).

Shopping that brief against the [palette](../guides/instrument-recipes.md) went *well* on
**timbre**: AIR's lineup maps onto modeled engines we already have, and the cart finally voices
three of them for this artist — **`INSTR_VOICE`** (the Kelly vocoder lead — first melodic-lead
use of the formant engine on the dial), **`INSTR_PIPE`** (the Cherry flute — untapped shelf),
and **`INSTR_REED`** (the Playground tenor sax) — plus `INSTR_EPIANO` Rhodes, `INSTR_GUITAR`
nylon, and a fuzzy `instrument_drive` Moog bass. What came up short is, as with afrobeat, the
**effects bus**: AIR isn't just instruments — it's instruments *drenched in reverb, run through
a string-machine chorus, onto warm tape.* That layer is missing.

> **Genre: design exploration.** A wishlist tied to one station's blind brief, not a bug list.
> Shipped `air.c` works around every hole below (it leans on the **echo bus** — a delay — as a
> reverb stand-in, and a detuned-SAW pair as a chorus stand-in). Companion docs: the bus is
> **designed, not built** in [`instrument-engines.md`](instrument-engines.md) § 8.10, disciplined
> by [decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md); the sequenced
> build-list is in [`sound-next-steps.md`](sound-next-steps.md) § "Each effect → its showcase
> cart" (AIR is now a demand witness there). Sibling: [`afrobeat-effects-wants.md`](afrobeat-effects-wants.md).

---

## The wants, each mapped to the effect that would unblock it

| want (the voice the music asks for) | why it's short today | unblocked by |
|---|---|---|
| **Reverb — the drench.** Every AIR record is washed in long, lush reverb; the ambience *is* the AIR sound (the Solina pad, the Rhodes, the flute, all blooming into a hall). | No reverb at all. `air.c` fakes depth with the **echo bus** (`instrument_echo`) — but that's a *delay*, repeating taps, not a room. It reads as slapback, not space. | **reverb** (§8.10 #1, bus-only — "the biggest"). AIR is a top vote. |
| **Chorus / ensemble width — the Solina.** The ARP/Solina string-machine wash is a *chorused* detuned ensemble; it's half of AIR's signature. | No chorus. Faked in-cart by a **detuned-SAW pair** (`instrument_tune` 0.07–0.12) + a volume LFO. That's a unison shimmer, not the swirling 3-stage BBD chorus a string machine *is*. | **chorus / unison width** (§8.10 #2; build-list → Juno). |
| **A real vocoder — Kelly's robot lead.** A vocoder carries *harmony* (a chord) modulated by a voice's formants; AIR's robotic leads are a vocoder, not a single sung vowel. | We have `INSTR_VOICE` (formant *synth*) — the cart sings one vowel-shaped monophonic lead. A true vocoder is an **effect**: a carrier (the synth) shaped by a modulator's spectral envelope. | **formant filter / vocoder** (build-list → vocoder/talkbox cart). |
| **Tape saturation / analog warmth.** Moon Safari is vintage analog gear onto tape — soft compression, harmonic warmth, a little wow. | No tape stage. `instrument_drive` adds grit on the bass, but not the gentle whole-mix glue/warmth. | **tape (wow/flutter/sat)** (build-list → Frippertronics). |

## Engine note — `INSTR_PIPE` intonation at the top (NOT an effects-bus issue)

While voicing the Cherry flute I hit a tuning problem: at a **high register** the PIPE jet-drive
went audibly out of tune (station-owner heard it live: *"that pipe is hella out of tune"*). The
engine's own design note (instrument-engines.md §8.8.8) flags the cause — `harmonics` = overblow
drives the jet gain `2 + overblow·8`, and **"high gain may screech at the top"** of the range.
The fix in `air.c` was cart-side, not engine: pin the flute to the showcase `pipe/flute` recipe
(`h0.00 t0.38 m0.70`, overblow **0** so it stays out of the screech zone) and drop the Cherry
lead register from 74–88 down to **67–83**. That tuned up in practice. **Residual question for
the engine owner:** is PIPE's fundamental tracking dependable across the *full* playable range at
overblow 0, or does intonation drift toward the top even without overblow? If the latter, it's a
real engine-tuning item (a pitch-tracking / bore-length calibration pass), not effects-bus work —
worth a STEP-0 check the next time PIPE is touched. Logged here so it isn't mistaken for a
missing-effect want.

## Not-effects: the cart's own generative business

- **Recognizably-different songs of one artist** — solved as a **chord brain #4 (stolen
  playbook)** layered with per-archetype groove/tempo/lead/form (the artist-station pattern,
  game-music.md). Pure cart C, no engine/effect needed.
- **The melodic "rolling" bassline (La Femme d'Argent)** — a **bass brain** (a seeded ostinato
  outlining root/5th/octave/passing tones over the dorian loop). Cart logic; graduates to a
  header if a second station wants this specific roll.

---

## What this says about the effects bus

AIR is a second strong vote (after afrobeat) for **reverb** and **chorus**, and adds the
clearest demand yet for a **true vocoder** — the one effect that turns `INSTR_VOICE` from "a
synth that sings a vowel" into "the AIR robot voice." When the bus ships, `air.c` is a ready
acceptance test: the Solina wants the chorus, the whole mix wants the reverb, and Kelly wants
the vocoder.

*(Pairs with `tools/carts/air.c` and its `radio-voices.md` chart.)*
