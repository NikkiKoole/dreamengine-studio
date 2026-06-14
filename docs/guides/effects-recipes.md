# Effects recipes — the grabbable-effect cookbook

The effects companion to [`instrument-recipes.md`](instrument-recipes.md). That file is the
supply-side palette of **instrument patches** (how to voice an `INSTR_*` engine); **this file
is the same shelf for the engine EFFECTS** — echo, reverb, chorus, flanger, tape, auto-wah,
drive (+ its waveshaper modes), bitcrush and EQ. It answers "I want a dub throw / a stone-hall
bloom / a Juno shimmer / an acid squelch / 8-bit grit / a low-end lift — what do I call, and who
already does it well?"

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

> **⚠ Effects are SET-AND-HOLD — (re)configure them only when a value CHANGES, never every frame.**
> `crush()`/`tape()`/`eq()`/`chorus()`/`flanger()`/`reverb()`… reconfigure bus DSP (ring buffers,
> filter coefficients) on each call. Wiring a knob straight into `update()`/`draw()` so the configure
> fn fires 60×/s churns the audio thread and reads as **stutter / lag — not a crash** (so it's easy
> to miss and easy to blame on the wrong thing). Re-apply **only when the value actually moved**:
> keep a `last-applied` copy and compare, or gate on the `ui_*` widget's "changed" return. Same for
> the live per-note setters (`note_cutoff`/`note_reverb`/`note_vol`): push only when the target
> changed — a value that's genuinely sweeping every frame (a tape-speed wobble, a sidechain duck) is
> fine, but stop re-issuing it once it settles. **Pattern to copy: `groovebox`'s `apply_fx()`** (a
> change-gated re-apply; its pad cutoff only pushes while the pump is actually moving). This was a
> real lag bug in `groovebox` v1 — profiler-confirmed once the per-frame churn was removed.

> **The showcase carts ARE the de-facto effect library.** Each effect has one cart whose whole
> point is *that effect as the instrument* — crib from it first. Showcases:
> `spacecho` (echo) · `cathedral` (reverb) · `juno`/`solina` (chorus) · `mistress` (flanger) ·
> `tapeloop` (tape) · `clavinet` (wah) · `drivemodes` (drive modes) · `bitcrush` (bitcrush) · `eq` (EQ) ·
> `epiano` (tremolo + phaser) · `organ` (leslie) · `vowel` (formant) · `djfilter` (filter).
>
> **The two bus-effect *homes* (effects on a whole mix, not one voice):** `pedalboard` is the
> SERIAL-insert showcase (`fx_order` — one guitar → a reorderable chain); **`groovebox`** is the
> SUMMED-BUS showcase (six looping voices → one master → live CRUSH/EQ/TAPE/reverb + the master
> `fx_order` toggle + a faked sidechain PUMP). Crib the live-knob master rack from `groovebox`.

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

### reverb send-buses — `reverb_bus(tank, size, damp)` · `instrument_reverb_bus(slot, tank, mix)`

**Two reverbs at once.** `reverb()` above is one shared room (tank 0); these carve **independent**
spaces on tanks 1–2, each on its own bus, so different instruments bloom into different rooms in the
same mix. Configure the space with `reverb_bus(tank, size, damp)`, route a slot in with
`instrument_reverb_bus(slot, tank, mix)`. (Tank 0 stays `reverb()`'s master send — use 1–2 here.)
**Showcase: `reverb spaces`** (a tight mallet room + a vast organ plate, ringing together).

| recipe | call | character | used by |
|---|---|---|---|
| tight bright room | `reverb_bus(1, 0.34f, 0.15f)` + send 0.6–0.8 | small, close, lively — percussion/plucks that need to stay crisp | `reverbspace` (ROOM) |
| vast dark plate | `reverb_bus(2, 0.95f, 0.55f)` + send 0.8 | endless, warm, swallowing — pads/organ that should drown | `reverbspace` (PLATE) |

#### effects AFTER the reverb — `reverb_bus_fx(tank, fx, a, b, c)`

The send-bus's signature trick: run a pedal **on the wet tail** (the reverb rings, then the effect
chews it — a plain send can't, its wet always returns clean). `fx` = `FX_CRUSH` / `FX_EQ` / `FX_TAPE`
/ `FX_CHORUS`; `a/b/c` are that effect's own params (same scale as its dedicated API — crush: bits,
rate, mix · eq: low/mid/high dB · tape: wow/flutter/sat · chorus: rate/depth/mix). Effects stack in
call order; `crush`/`chorus` `mix 0` = bypass. Call `reverb_bus(tank,…)` first.

| recipe | call | character | used by |
|---|---|---|---|
| crushed lo-fi tail | `reverb_bus_fx(2, FX_CRUSH, 5, 8, 1.0f)` | the plate decays into grainy, aliased 5-bit dust — shoegaze/vaporwave space | `reverbspace` (CRUSH toggle) |
| dark-tail EQ | `reverb_bus_fx(1, FX_EQ, 0, 0, -8.0f)` | roll the highs off the wet only — a warm tail without dulling the dry source | — |
| tape-worn reverb | `reverb_bus_fx(2, FX_TAPE, 0.4f, 0.2f, 0.5f)` | the tail wows + saturates like a reverb printed to tape | — |

#### reverb as an in-line pedal — `reverb_insert(size, damp, mix)`

The third reverb shape: a dry/wet-MIX insert **on the master bus**, so it's a real reorderable pedal
(unlike `reverb()`, a parallel send whose chain position is cosmetic). Put `FX_REVERB` in your
`fx_order(0, …)` list to place it; dragging it before/after another pedal is then audible. `mix 0` = bypass.

| recipe | call | character | used by |
|---|---|---|---|
| honest pedalboard reverb | `reverb_insert(0.7f, 0.3f, 0.45f)` + `FX_REVERB` in `fx_order(0,…)` | a guitar-pedal reverb whose position matters: before crush = crush the wet tail; after = reverb the crushed guitar | `pedalboard` (REVERB pedal), `groovebox` (SPACE knob — the ORDER toggle IS reverb↔crush on the summed mix) |

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
| warm & drifting | `tape(0.22f, 0.11f, 0.34f)` | gentle vintage glue across the whole mix | `air`, `groovebox` (TAPE knob) |
| clean & tight | `tape(0.10f, 0.08f, 0.24f)` | barely-there warmth, no audible pitch drift | `air` |
| lo-fi just the drums | `instrument_tape(SL_DRUMS, 0.4f, 0.5f, 0.7f)` | wonky tape on the kit, synths stay clean | (per-instrument pattern) |

## auto-wah — `wah(sensitivity, resonance, mix)` · `instrument_wah(slot, …)` · LFO: `wah_lfo(rate, resonance, mix)` · `instrument_wah_lfo(slot, …)`

A resonant bandpass swept on the bus — two flavors of the same insert. **Envelope** (`wah`): the band
opens with how hard you play (the funk quack, dynamics-driven). **LFO** (`wah_lfo`): a sine rocks the
band on its own at `rate` Hz — the hands-free rhythmic "wah-wah", ignores dynamics. Both share the one
`FX_WAH` bus bandpass (so a bus is in *one* mode at a time — calling `wah` switches to envelope,
`wah_lfo` to LFO); both are set-and-hold (the LFO runs inside the engine). Best on ONE rich/percussive
part. **Showcase: `clavinet`** (envelope); **`pedalboard`** WAH pedal exposes both via its MOD knob.

| recipe | call | character | used by |
|---|---|---|---|
| talking clav | `instrument_wah(SL_CLAV, 0.6f, 0.7f, 0.85f)` | the Hohner D6 quack — opens on the stab, shuts between | `clavinet` |
| wah-bar sweep | `instrument_wah(slot, 0.4+amt*0.6, 0.45+amt*0.4, 0.75+amt*0.25)` | one knob `amt` 0→1 ramps the whole effect in | `epiano` |
| envelope auto-wah | `wah(0.5f, 0.55f, 0.7f)` | dynamics-driven funk quack on the whole bus — dig in and it opens | `pedalboard` (WAH, MOD→ENV) |
| LFO auto-wah | `wah_lfo(0.5f+rate*9.5f, 0.55f, 0.7f)` | hands-free rhythmic wah rocking on its own at the set rate | `pedalboard` (WAH, MOD→LFO) |

## formant — `formant(vowel, q, mix)` · `instrument_formant(slot, vowel, q, mix)`

The VOWEL filter — four bandpasses parked at the human formant frequencies (F1–F4), so any source
takes on an "ooh/aah/eee" vocal colour. `vowel` 0–1 sweeps **OO→OH→AH→EH→EE** (sweep it and a synth
talks); `q` 0–1 narrows the peaks (broad → pronounced/nasal); `mix` 0–1 (0 = off). A wah is the
one-peak version of this; this is the multi-peak vowel. **Single-input** (the talkbox family, knob
for a mouth) — *not* a vocoder (that needs a second carrier×modulator input — a future effect once
the sidechain lands). Reuses navkit's measured vowel table (shared with `INSTR_VOICE`). Master or
per-instrument. A sweeping `vowel` every frame is fine (it's a coefficient update, no buffer churn —
unlike crush/tape). **Showcase: `vowel`** (a saw chord that talks).

| recipe | call | character | used by |
|---|---|---|---|
| talking saw | `formant(swept 0→1, 0.6f, 0.95f)` | a buzzing chord that says its vowels as the knob moves — the talkbox move | `vowel`, `pedalboard` (MANUAL mode) |
| nasal "ee" lead | `instrument_formant(slot, 1.0f, 0.8f, 0.85f)` | a pinched, nasal vocal colour on one lead/pad, rest of the mix plain | (per-instrument pattern) |
| open "ah" pad | `instrument_formant(I_PAD, 0.5f, 0.5f, 0.7f)` | a soft choral "ah" colour on a held pad — broad peaks, gentle | (pad pattern) |
| randomized crowd vowels | `instrument_formant(slot, base[i]±jitter + excite·k, ~0.6f, 0.9f)` — base re-rolled per note-on, vowel re-pushed per frame | a few voices each on a different, slightly-random vowel that OPENS with intensity — a crowd going "ah… AAAH/eee" the scarier it gets, not one unison tone | `coaster` (roller-coaster riders) |
| syllable-per-pluck | `formant()` re-pushed each note-on with the next vowel in a list, glided | each strum speaks a new vowel — the guitar recites a "word" (cart-side: `formant()` has no trigger input, so the cart advances the vowel on each pluck) | `pedalboard` (STEP mode) |
| picking-driven open | `formant()` driven by a strum envelope (vowel opens on attack, relaxes) | hands-free vocal swell — the auto-wah gesture wearing a vowel | `pedalboard` (ENV mode) |
| auto-sweep LFO | `formant()` re-pushed each frame with `vowel = 0.5−0.5·cos(phase)` | the vowel rocks OO↔EE on its own at a set rate — the rhythmic "yoy-yoy" wobble, hands-free | `pedalboard` (LFO mode), `vowel` (AUTO) |

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
| 8-bit master | `crush(6.0f, 8.0f, 1.0f)` | crunchy downsampled whole-mix grit — the retro-console sound | `bitcrush`, `groovebox` (CRUSH knob — the summed SP-1200 grit on a full mix) |
| gnarly destroy | `crush(2.0f, 16.0f, 1.0f)` | barely-recognizable, steppy quantization noise | `bitcrush` |
| subtle dirt | `crush(10.0f, 2.0f, 0.5f)` | a touch of aliasing whine + step, mostly clean | `bitcrush` |
| lo-fi just the bass | `instrument_crush(I_BASS, 5.0f, 6.0f, 1.0f)` | crush ONE part (the bass) while the lead stays crystal — the point of the per-instrument bus | `bitcrush` |

> ⚠ `floorf` quantization adds a small negative DC bias at low bits (~−0.014 @ bits 5) — kept as
> on-brand lo-fi character, not blocked. See [`audio-notes.md §17`](../design/audio-notes.md).

## EQ — `eq(low_gain, mid_gain, high_gain)` · `instrument_eq(slot, low_gain, mid_gain, high_gain)`

3-band tone control, and **the library's only BOOST** — every other tone tool (the SVF filters)
can only cut. Three bands split at ~80 Hz and ~6 kHz — LOW (`<80 Hz`, body/sub) / MID (`80 Hz–6 kHz`,
the meat of most sounds) / HIGH (`>6 kHz`, presence/air) — each `±12 dB` (+ = boost, − = cut,
**0/0/0 = flat = off**, byte-identical to no EQ). Master (whole mix) or per-instrument. The classic
partner to `DRIVE_ASYM`: EQ before/after a clipper = a real **guitar-amp tone**. **Showcase: `eq`**
(drag the LOW/MID/HIGH nodes in the grid, the response curve bends to follow).

| recipe | call | character | used by |
|---|---|---|---|
| warm & dark | `eq(4.0f, 0.0f, -3.0f)` | lift the body, tame the fizz — cozy/lo-fi | `eq` |
| smile (loudness) | `eq(6.0f, -2.0f, 5.0f)` | boosted lows AND highs, scooped mids — the hi-fi "smile" curve | `eq` |
| mid scoop (metal) | `eq(3.0f, -8.0f, 4.0f)` | gut the mids for a scooped, aggressive rhythm tone | `eq` |
| air / presence | `instrument_eq(slot, 0.0f, 0.0f, 6.0f)` | open up a dull lead or pad with top-end sparkle, rest untouched | `eq` |
| fat bass | `instrument_eq(I_BASS, 7.0f, 0.0f, 0.0f)` | weight under one part while the mix stays clear (per-instrument bus) | `eq` |
| guitar-amp tone | `instrument_drive_mode(slot, DRIVE_ASYM); instrument_drive(slot, 0.55f); eq(3.0f, 2.0f, -4.0f)` | asymmetric clip + a mid-forward EQ around it — the cranked-amp move | `eq` |
| mud cut | `eq(-5.0f, 1.0f, 2.0f)` | thin out a boomy low end, nudge clarity up top | (cut pattern) |
| live 3-band | `eq((lo-0.5f)*24, (mid-0.5f)*24, (hi-0.5f)*24)` | three knobs over the whole mix, ±12 dB each (0.5 = flat = off). A *louder* EQ is what makes the `fx_order` crush↔eq order audible (a gentle tilt isn't enough to hear) | `groovebox` (LO/MID/HI) |

## tremolo — `tremolo(rate, depth, shape)` · `instrument_tremolo(slot, rate, depth, shape)`

A volume LFO that ducks the level up and down — the Fender/Wurlitzer **amp wobble**, the "electric
piano" throb. `rate` 0.1–20 Hz, `depth` 0–1 (0 = off; only attenuates, never boosts — like a real
amp: `out = in·(1 − depth·(1 − lfo))`), `shape` `TREM_SINE` (smooth, default) / `TREM_SQUARE` (hard
chop) / `TREM_TRI` (ramp). A VERBATIM port of navkit's `processTremolo` (A/B-matched on a sine —
rate + depth dead-on). **One shared LFO phase per bus**, so a per-instrument tremolo wobbles that
instrument's WHOLE output in unison — the coherent amp wobble a per-voice `LFO_VOLUME` can't give
(there each note has its own phase = a shimmer, not a throb). Master or per-instrument. **Showcase:
`epiano`** (the Rhodes/Wurli throb, off on the clav). Dormant until depth>0 → non-users byte-identical.

| recipe | call | character | used by |
|---|---|---|---|
| Wurlitzer 200A throb | `instrument_tremolo(I_EP, 5.0f+amt*1.5f, amt*0.85f, TREM_SINE)` | the soul-ballad EP wobble — deeper than the Rhodes; the tremolo slider rides rate+depth together | `epiano` |
| suitcase Rhodes wobble | `instrument_tremolo(I_EP, 5.4f, 0.38f, TREM_SINE)` | the classic gentle ~5 Hz suitcase pulse | `epiano` |
| helicopter chop | `tremolo(8.0f, 0.9f, TREM_SQUARE)` | hard on/off gate on the whole mix — stutter/strobe | (square pattern) |

> A real clav has **no** tremolo — the `epiano` clav preset sets depth 0 (bypass). Tremolo is the
> Rhodes/Wurli signature, not a universal EP effect.

## auto-pan — `autopan(rate, depth, shape)` · `instrument_autopan(slot, rate, depth, shape)`

The **stereo sibling of tremolo**: the same amplitude LFO, but applied **antiphase** to L/R, so the
sound sweeps side to side instead of throbbing in place — the **Rhodes Suitcase stereo vibrato**, the
auto-pan stompbox, rotary-style motion. The left gain is the plain tremolo gain `1 − depth·(1 − lfo)`,
the right its complement `1 − depth·lfo` (depth 1 = a channel hits silence = full L↔R). `rate` 0.1–20
Hz, `depth` 0–1 (0 = off, 1 = full width), `shape` `TREM_SINE` (smooth glide, default) / `TREM_SQUARE`
(hard ping-pong) / `TREM_TRI` (linear sweep). Only attenuates (never boosts), so the summed level
never exceeds the dry input. **Its OWN insert (`FX_PAN`), not a mode of tremolo** — so it STACKS with
`tremolo` on one bus (a fast amplitude throb AND a slow stereo drift at once, two independent LFOs),
and on the pedalboard it's a real reorderable pedal beside TREMOLO. Master or per-instrument. Dormant
until depth>0 → non-users byte-identical. (Verified: a centered mono source reads correlation +1.0;
with auto-pan, 0.33 / −1.78 dB mono-fold = real width, and a slow sweep swings the balance ±0.56 dB.)

| recipe | call | character | used by |
|---|---|---|---|
| Suitcase stereo vibrato | `instrument_autopan(I_EP, 3.0f+spd*4.0f, 0.35f+dep*0.55f, TREM_SINE)` | the Rhodes Suitcase L↔R sway — the whole instrument incl. the tine tails pans coherently | `epiano` (VIBE pedal) |
| slow stereo drift | `autopan(0.4f, 0.8f, TREM_SINE)` | a wide, lazy wash across the mix — great stacked under a `tremolo` throb | `pedalboard` (AUTOPAN) |
| hard ping-pong | `autopan(4.0f, 1.0f, TREM_SQUARE)` | the sound jumps L/R/L/R — a rhythmic stutter-pan | `pedalboard` (AUTOPAN, WAV→square) |

> **Why not a flag on `tremolo()`?** Because then a bus could only ever throb OR pan, never both, and a
> "separate function, same state" API would *look* combinable but silently isn't. A distinct `FX_PAN`
> insert is the honest, composable form — two LFOs, two pedals. See
> [`../design/effects-bus-architecture.md`](../design/effects-bus-architecture.md) §0.

## phaser — `phaser(rate, depth, feedback, mix, stages)` · `instrument_phaser(slot, …)`

A chain of allpass filters swept by an LFO carves moving NOTCHES in the spectrum — the **70s
electric-piano / Small Stone swirl** (vocal, hollow; softer + rounder than a flanger's metallic comb).
`rate` 0–10 Hz (sweep speed; ~0.3–0.7 for the classic slow swirl), `depth` 0–1 (how far the notches
travel), `feedback` −0.95..0.95 (resonance around the notches), `mix` 0–1 (**0 = off**, and note
**0.5 is the deepest** — an all-wet allpass has no notches, the notches form in the dry+wet sum),
`stages` 2–8 (notch count — **4 = the classic Phase-90**, more = thicker). A VERBATIM port of navkit's
`processPhaser` (A/B-matched sample-for-sample, 0.99999 correlation). Master or per-instrument.
**Showcase: `epiano`** (the `P` toggle; on by default on the `suitcase` preset). Dormant until mix>0.

| recipe | call | character | used by |
|---|---|---|---|
| phased suitcase Rhodes | `instrument_phaser(I_EP, 0.5f, 0.8f, 0.4f, 0.5f, 4)` | the slow 4-stage Phase-90 swirl on an EP — the 70s signature | `epiano` |
| krautrock phased drone | `instrument_phaser(I_ORG, 0.12f, 1.0f, 0.6f, 0.5f, 6)` | very slow (~8s/sweep), deep, resonant on a held organ drone — the Neu!/Cluster hypnotic swirl; extreme in depth, gentle in rate so it evolves *with* the motorik pulse, not against it | `motorik` |
| deep 6-stage swirl | `phaser(1.0f, 1.0f, 0.6f, 0.5f, 6)` | thicker, more notches, more resonant — a wider whole-mix sweep | (6-stage pattern) |
| through-zero-ish | `phaser(0.3f, 0.9f, -0.6f, 0.5f, 4)` | negative feedback hollows the notches differently — a more "inside-out" sweep | (neg-fb pattern) |

> **Phaser vs flanger:** both sweep and whoosh, but a phaser is allpass NOTCHES (vocal, smooth — the
> Rhodes/Small Stone) and a flanger is a swept COMB with delay (metallic, jet-like — needs a rich
> source). For an electric piano, reach for the phaser.

## leslie — `leslie(speed, drive, balance, doppler, mix)` · `instrument_leslie(slot, …)`

THE rotary-speaker cabinet — the organ's voice. A spinning treble **HORN** (pitch wobble via a
real **Doppler** delay-line + a swirling volume) over a bass **DRUM**, split at an 800 Hz crossover.
The two rotors spin at independent speeds with **real spin-up/down inertia**, so flipping `speed`
gives the iconic **chorale↔tremolo swell** on its own — you don't ramp it, the engine does. `speed`
= `LESLIE_STOP` / `LESLIE_SLOW` (chorale ~0.7 Hz) / `LESLIE_FAST` (tremolo ~6 Hz); `drive` 0–1 (tube
preamp grit); `balance` 0=drum..1=horn; `doppler` 0–1 (horn pitch-wobble depth); `mix` 0–1 (**0 =
off**). A VERBATIM navkit `processLeslie` port (A/B 0.99999). **Pinned LAST** in the chain (the
cabinet output stage, not a reorderable pedal). **Showcase: `organ`** (the `L` footswitch).

| recipe | call | character | used by |
|---|---|---|---|
| jazz B3 swell | `instrument_leslie(I_ORG, LESLIE_SLOW, 0.0f, 0.5f, 0.7f, 1.0f)` → flip to `LESLIE_FAST` | chorale sway, then hit the switch for the spin-up to tremolo — the signature Hammond move | `organ` |
| overdriven rock organ | `instrument_leslie(I_ORG, LESLIE_FAST, 0.4f, 0.5f, 0.8f, 1.0f)` | cranked tube preamp into a fast horn — the cabinet screams | `organ` |
| horn-forward | `instrument_leslie(slot, LESLIE_FAST, 0.0f, 0.85f, 1.0f, 1.0f)` | balance toward the treble horn + deep Doppler = maximum shimmer/wobble | (balance pattern) |
| brake | `instrument_leslie(slot, LESLIE_STOP, …)` | rotors coast to a halt — the static, slightly-doubled "stopped" tone between songs | (stop pattern) |

> **Leslie vs the recipe:** decision 0015 first refused this as "tremolo + `LFO_PITCH` + drive, a
> recipe" — then building it proved an LFO can't band-split at a crossover, can't run a delay-line
> Doppler, and can't drive two inertial rotors. The reversal (0015, 2026-06-12) is the model case of
> the gate working. Great on organ; also try it on electric piano, guitar, or a whole psych mix.

---

## filter — `filter(int mode, float cutoff_hz, float resonance)`

THE master resonant filter — the **DJ-filter / build-up sweep**. A state-variable filter on the whole
mix in a selectable mode; **RIDE the cutoff live** (cheap to re-call every frame, unlike the buffer
effects). A reorderable insert (`FX_FILTER`), so its chain position matters (filter→crush vs crush→filter).
mode = `FILTER_LOW`/`HIGH`/`BAND`/`NOTCH` (`FILTER_OFF` = bypass), cutoff 20..~20000 Hz, resonance 0..1.
Same SVF as wah/formant, reused as a plain swept filter (0015: filter-reuse, not a new primitive).

| recipe | call | character | used by |
|---|---|---|---|
| breakdown muffle | `filter(FILTER_LOW, cut, 0.5f)` ride cut 300→8000 | close to a thumping muffle, open for the build/drop — the four-floor breakdown | `djfilter` (the BUILD breakdown) |
| screaming build | `filter(FILTER_LOW, cut, 0.9f)` sweep cut up | high resonance = an acid scream riding up into the drop | `djfilter` (the BUILD riser) |
| thin it out | `filter(FILTER_HIGH, 800.0f, 0.3f)` | roll off the lows — a radio/telephone thinning, or a transition | `djfilter` (CW = high-pass) |
| bipolar DJ filter | one knob: center = `FILTER_OFF`, CCW → `FILTER_LOW` 18k→150 Hz, CW → `FILTER_HIGH` 20→6k (resonance rises as you close) | the mixer one-knob filter — muffle on the breakdown, thin on a transition | `djfilter` (THE knob), `groovebox` (the FILTER knob) |
| the filter ride | `filter(FILTER_LOW, ride_hz(songpos), res)` per frame | a master cutoff curve AS the arrangement (the house "filter ride IS the song", for real) | — |

**Showcase: `djfilter`** (the bipolar one-knob mixer filter over a self-playing house loop, with a live response curve and a breakdown→riser→DROP BUILD).

## sidechain & bus compression — `sidechain(victim_bus, key, amount, atk, rel)` · `sidechain_key(slot, key, send)` · `glue(victim_bus, amount, atk, rel)`

DYNAMICS on the SUMMED signal (a gain stage, **not** a chain insert — not in `fx_order`). `sidechain`
ducks a victim bus every time a TRIGGER fires; route the trigger (the kick) into a key with
`sidechain_key`. `glue` is the same envelope→gain with no trigger — a bus compressor reading its own
level. amount 0 = dormant (byte-identical). **One comp per bus** (sidechain OR glue on a given bus,
not both). **Showcase: `groovebox`** (PUMP + GLUE share the master comp). Effects-bus Increment D.

| recipe | call | character | used by |
|---|---|---|---|
| the house pump | `sidechain_key(SL_KICK, 0, 1.0f); sidechain(0, 0, 0.6f, 1, 140)` | the whole mix breathes against the four-floor kick — bass/pad duck and bloom across the beat | `groovebox` (PUMP) |
| gentle bus glue | `glue(0, 0.4f, 8, 150)` | the mix squashed as one lump — drum-bus/master "glue", no trigger | `groovebox` (GLUE) |
| tight pump | `sidechain(0, 0, 0.8f, 1, 90)` | deeper duck + faster recovery — aggressive EDM pump | — |

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
