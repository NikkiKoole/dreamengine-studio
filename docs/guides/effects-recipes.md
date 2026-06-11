# Effects recipes — the grabbable-effect cookbook

The effects companion to [`instrument-recipes.md`](instrument-recipes.md). That file is the
supply-side palette of **instrument patches** (how to voice an `INSTR_*` engine); **this file
is the same shelf for the engine EFFECTS** — echo, reverb, chorus, flanger, tape, auto-wah,
drive (+ its waveshaper modes) and bitcrush. It answers "I want a dub throw / a stone-hall
bloom / a Juno shimmer / an acid squelch / 8-bit grit — what do I call, and who already does
it well?"

Three layers cover effects, each a different question:

- **[`studio.h`](../../runtime/studio.h) + [`studioDocs.js`](../../editor/src/studioDocs.js)
  = the API** — every effect function's signature + a one-line doc (drives autocomplete + the
  help tab). *What can I call?*
- **[`audio-notes.md`](../design/audio-notes.md) (§8.10, §17) = the why** — the design
  rationale: why echo/wah are shared buses, why drive sits post-filter, the DSP. *Why is it
  built this way?*
- **`effects-recipes.md` (this file) = the how-to** — named, copy-paste settings + the cart
  that proves each one. *What numbers actually sound good, and where do I crib them?*

Most effects are a **shared bus** (one `echo()`/`reverb()`/`chorus()`… for the whole mix) with
an optional **per-instrument** form (`instrument_echo(slot, …)` etc.) so you can effect one part
and leave the rest dry. `drive` is the exception — it's a **per-voice** insert (`instrument_drive`
/ `note_drive`). Full architecture: [`audio-notes.md §8.10`](../design/audio-notes.md).

> **The showcase carts ARE the de-facto effect library.** Each effect has one cart whose whole
> point is *that effect as the instrument* — crib from it first. Showcases:
> `spacecho` (echo) · `cathedral` (reverb) · `juno`/`solina` (chorus) · `mistress` (flanger) ·
> `tapeloop` (tape) · `clavinet` (wah) · `drivemodes` (drive modes) · `bitcrush` (bitcrush).

---

## echo — `echo(time_ms, feedback, tone)` · `instrument_echo(slot, send)` · `note_echo`

THE shared tape-echo bus. `time_ms` 1–2000 (sweep it live → the tail pitch-bends like tape speed),
`feedback` 0–1.1 (>1.0 self-oscillates into saturation, not a blow-up), `tone` 0–1 (repeats darken).
Per-slot `send` 0–1 chooses how much each instrument throws into it. **Showcase: `spacecho`** (RE-201).

| recipe | call | character | used by |
|---|---|---|---|
| dub throw | `instrument_echo(I_SKANK, 0.18f)` base → `0.9f` on the throw | reggae skank chop that blooms on cue; ride feedback `0.45→0.8` for the meltdown | `dub` |
| dotted-8th tape loop | `echo(60000/(bpm*4)*3, 0.45f, 0.22f)` | tempo-locked dub repeats, dark tail | `dub` |
| slapback | `instrument_echo(slot, 0.10f–0.16f)` | tight doubling on a lead/EP/guitar, no wash | `air` (lead/EP/gtr), `sh101` |
| dynamic room echo | `echo(90+open*300, 0.10f+open*0.30f, 0.35f+open*0.25f)` | echo that opens with the space: corridor→hall driven by one `open` 0..1 | `deeper` |
| hard feed into a loop | `instrument_echo(PAD, 0.7f)` | a pad fed heavily into a long echo = an evolving wash (the tape-loop trick) | `tapeloop`, `wowflutter` |
| live delay pedal | `echo(rate_ms(), fb_x(), tone_x())` from knobs | the effect played as the instrument; feedback into the red = self-osc drone | `spacecho`, `tb303` |

## reverb — `reverb(size, damping)` · `instrument_reverb(slot, send)` · `note_reverb`

THE shared room/hall (Schroeder). `size` 0–1 (small room → endless hall), `damping` 0–1 (bright →
dark tail). Bus-only configure; per-slot `send`. **Showcase: `cathedral`** (organ into stone).

| recipe | call | character | used by |
|---|---|---|---|
| stone hall | `reverb(0.94f, 0.32f)` + sends 0.5–0.9 | vast bright cathedral bloom; the chord becomes the instrument | `cathedral`, `deeper` (hall) |
| lush plate | `reverb(0.62f, 0.38f)` | a roomy, slightly-dark pop verb that doesn't drown the mix | `air` |
| small warm room | `reverb(0.30f, 0.55f)` | tight dark ambience — upright bass, close jazz | `upright` |
| worn-tape ensemble | `reverb(0.55f, 0.45f)` | mid room under chorus+tape = the Mellotron "recording" | `mellotron` |
| dynamic openness | `reverb(0.12f+open*0.82f, 0.50f-open*0.18f)` | corridor→hall on one knob; sends 0.08→0.92 (10× range) | `deeper` |
| dry bus, wet parts | bass/kick send `0.0f`, mallet/pad `0.32–0.40f` | keep the low end tight + punchy while melody hangs in the hall | `polopan` |

## chorus — `chorus(rate, depth, mix)` · `instrument_chorus(slot, rate, depth, mix)`

BBD/Juno ensemble widening. `rate` 0.1–5 Hz, `depth` 0–1, `mix` 0–1 (0 = off). Master (whole mix) or
per-instrument. **Showcase: `juno` (master) + `solina` (per-part ensemble).**

| recipe | call | character | used by |
|---|---|---|---|
| Solina ensemble II | `chorus(1.6f, 0.70f, 0.72f)` | THE deep lush string-machine swirl — the signature | `solina` |
| Solina ensemble I | `instrument_chorus(I_PAD, 0.9f, 0.50f, 0.60f)` | classic lush, less intense; per-part so only the pad swims | `solina`, `air` |
| Juno I | `chorus(0.1f, 0.45f, k_mix)` | slow gentle thickening — the Juno-6 mode I | `juno` |
| Juno II | `chorus(0.22f, 0.62f, k_mix)` | faster + wetter, more obvious wobble | `juno` |
| Rhodes width | `instrument_chorus(I_EP, 0.7f, 0.30f, 0.28f)` | gentle stereo on an EP, not a full swirl | `air` |
| metallic shimmer | `instrument_chorus(slot, 1.4f, 0.55f, 0.55f)` | brighter, faster — the Martenot "métallique" diffuseur | `martenot` |

## flanger — `flanger(rate, depth, feedback, mix)` · `instrument_flanger(slot, …)`

Swept comb with feedback (the jet-whoosh). `feedback` is signed: + = jet/metallic, − = through-zero.
Needs a rich source (chords, noise). **Showcase: `mistress`** (on a strummed guitar).

| recipe | call | character | used by |
|---|---|---|---|
| jet guitar | `instrument_flanger(GTR, rate, depth, fb, mix)` live | classic swept-comb whoosh; sweep the rate by hand | `mistress` |
| metallic static wash | `instrument_flanger(NZ, 0.12f, 0.97f, 0.9f, 0.6f)` | slow, deep, high-feedback comb on a noise bed — a drone texture | `mistress` |

## tape — `tape(wow, flutter, saturation)` · `instrument_tape(slot, …)`

Vintage analog: `wow` (slow pitch drift) + `flutter` (fast warble) + `saturation` (warm clip + HF
rolloff). Master or per-instrument. **Showcase: `tapeloop`.**

| recipe | call | character | used by |
|---|---|---|---|
| worn tape | `tape(0.65f, 0.45f, 0.50f)` | pronounced wobble + warmth — the most degraded setting | `mellotron`, `wowflutter` |
| warm & drifting | `tape(0.22f, 0.11f, 0.34f)` | gentle vintage glue across the whole mix | `air` |
| clean & tight | `tape(0.10f, 0.08f, 0.24f)` | barely-there warmth, no audible pitch drift | `air` |
| lo-fi just the drums | `instrument_tape(SL_DRUMS, 0.4f, 0.5f, 0.7f)` | wonky tape on the kit, synths stay clean | (per-instrument pattern) |

## auto-wah — `wah(sensitivity, resonance, mix)` · `instrument_wah(slot, …)`

A resonant bandpass swept by an envelope follower on the bus — opens with how hard you play (the
funk quack). Best on ONE rich/percussive part. **Showcase: `clavinet`.**

| recipe | call | character | used by |
|---|---|---|---|
| talking clav | `instrument_wah(SL_CLAV, 0.6f, 0.7f, 0.85f)` | the Hohner D6 quack — opens on the stab, shuts between | `clavinet` |
| wah-bar sweep | `instrument_wah(slot, 0.4+amt*0.6, 0.45+amt*0.4, 0.75+amt*0.25)` | one knob `amt` 0→1 ramps the whole effect in | `epiano` |

## drive — `instrument_drive(slot, amount)` · `note_drive` · `instrument_drive_mode(slot, mode)`

Per-voice saturation AFTER the filter (so resonance screams into it — the grit knob). `amount` 0–1;
**`instrument_drive_mode(slot, DRIVE_*)` picks the waveshaper.** All four bypass cleanly at 0 and hold
full-scale loudness (character, not volume). **Showcase: `drivemodes`.**

| recipe | call | character | used by |
|---|---|---|---|
| acid squelch | `FILTER_LOW` + high res + `instrument_drive(slot, 0.3–0.5f)`, live `note_drive` | the 303: resonant peak driven into saturation; ride DRV with RES | `tb303`, `moog`, `spacecho` |
| distorted house lead | `instrument_drive(I_LEAD, 0.45f)` [SOFT] | the genre hook — a hard, present saw lead | `house` |
| warehouse kick | `instrument_drive(SL_KICK, 0.30–0.35f)` | saturated kick weight, the French-house pump | `tr909`, `tr808`, `italo`, `house` |
| bass grind | `instrument_drive(I_BASS, 0.40f + fuzz*0.0055f)` (0.40–0.95) | a fuzz knob mapped onto a bass, clean→wall | `air` |
| **guitar grit** (mode) | `instrument_drive_mode(slot, DRIVE_ASYM); instrument_drive(slot, 0.5f)` | asymmetric tube — fat **even** harmonics, the round single-ended-amp warmth | `drivemodes` |
| **digital fuzz** (mode) | `DRIVE_HARD` + amount 0.6+ | buzzy, square-edged, harsh — chiptune lead bite | `drivemodes` |
| **metallic / ring-ish** (mode) | `DRIVE_FOLD` + amount swept up | sine wavefolder — glassy, clangy as you push it | `drivemodes` |
| bark → drive | `note_drive(h, bark)` live, slewed | an EP/organ that digs in when you lean on a knob | `epiano`, `organ`, `modrack` |

## bitcrush — `crush(bits, rate, mix)` · `instrument_crush(slot, bits, rate, mix)`

Lo-fi quantization: drop the **bit depth** (`bits` 1–16, floor to 2^bits levels) and the **sample
rate** (`rate` 1–64, sample-and-hold every Nth sample). Master or per-instrument. Dormant until first
call (mix 0 = off). **Showcase: `bitcrush`** (its scope doubles as an XY pad — X=rate, Y=bits).

| recipe | call | character | used by |
|---|---|---|---|
| 8-bit master | `crush(6.0f, 8.0f, 1.0f)` | crunchy downsampled whole-mix grit — the retro-console sound | `bitcrush` |
| gnarly destroy | `crush(2.0f, 16.0f, 1.0f)` | barely-recognizable, steppy quantization noise | `bitcrush` |
| subtle dirt | `crush(10.0f, 2.0f, 0.5f)` | a touch of aliasing whine + step, mostly clean | `bitcrush` |
| lo-fi just the bass | `instrument_crush(I_BASS, 5.0f, 6.0f, 1.0f)` | crush ONE part (the bass) while the lead stays crystal — the point of the per-instrument bus | `bitcrush` |

> ⚠ `floorf` quantization adds a small negative DC bias at low bits (~−0.014 @ bits 5) — kept as
> on-brand lo-fi character, not blocked. See [`audio-notes.md §17`](../design/audio-notes.md).

---

## detune as an effect — `instrument_tune(slot, semitones)`

Not a pitch-shift but **unison/ensemble shimmer**: tiny fractional offsets make voices beat against
each other. (Read live by every sounding voice on the slot — fire-and-forget hits bend too.)

| recipe | call | character | used by |
|---|---|---|---|
| divide-down spread | `instrument_tune(slot, 0.05f + t*0.005f)` per voice | the string-machine cluster (0.05–0.10 across tabs) | `solina`, `air` |
| opposite-detuned pair | `instrument_tune(A, +dt); instrument_tune(B, -dt*1.4f)` | two layers pulled apart = wide unison | `braindance` |
| gong ombak | detuned gong pair (per-degree cents) | the gamelan beating shimmer | `gamelan` |

---

## Adding to this book

Shipping or using an engine effect in a cart? Add the recipe here — name it, give the exact call +
values, one line of character, and the cart under **used by** (don't mint a duplicate of an existing
recipe; add your cart to its row instead). This is the effects analog of the rule for
[`instrument-recipes.md`](instrument-recipes.md); see [CLAUDE.md](../../CLAUDE.md) → "Shipping a sound
cart? Keep the recipe docs current."
