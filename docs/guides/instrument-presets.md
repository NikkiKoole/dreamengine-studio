# Instrument presets — the named-patch catalog

A **preset** is one cart's recipe for turning an engine into a *named voice*: an
`INSTR_*` engine plus its `instrument()` ADSR, filter, drive, and `harmonics/timbre/
morph` macros. The engines live once in `runtime/sound.h`; the **presets do not** — every
station hand-rolls them in its `init()`, and authors copy recipes between carts by hand.
That copying is invisible in the source, so two stations can share a sound (or quietly
drift apart) and nobody can see it.

This file gives each recipe a **clear name** so the per-station voice charts
([`radio-voices.md`](radio-voices.md)) can reference the name instead of repeating four
lines of `instrument_*` calls — and so shared recipes become visible on one page.

> **Status: growing.** Charted so far: **italo**, **house**, **citypop**, **motorik** (see
> [`radio-voices.md`](radio-voices.md) for why we started with italo). Grow one station
> at a time. When a new station reuses a recipe already named here, add it to that
> preset's **used by** line rather than minting a duplicate.

## How to read an entry

```
### group/name
`INSTR_*` · A# D# S# R# · <filter> · <macros/env/drive>   ← the recipe, compact
One line of character — what it sounds like and why.
- tier: shared | variant | unique
- origin: <cart it was first written in>
- used by: <cart (slot)> · <cart (slot)> …
```

Recipe shorthand: `A/D/R` in ms, `S` = sustain 0–7, from `instrument(slot, ENG, a, d, s,
r)`. `LP/HP/BP n/q` = `instrument_filter` low/high/band-pass at *n* Hz, resonance *q*.
`h/t/m` = `instrument_harmonics/timbre/morph` (0–1). `drive`, `pitch-env`, `cut-env` from
`instrument_drive` / `instrument_env(…ENV_PITCH/ENV_CUTOFF…)`.

**Sharing tiers** — the reason this catalog exists:

| Tier | Means | Presentation | Watch out |
|---|---|---|---|
| **shared** | byte-identical recipe in 2+ carts | one entry, all carts in **used by** | hand-copied, *not* a shared symbol — editing one does **not** move the others. A latent drift bug. Candidates to extract into a real shared patch helper. |
| **variant** | the *same instrument* re-tuned — same engine + role, numbers drift but the sound is "the same thing" | one entry + a variants table (numbers per cart) | usually fine; if the drift is accidental, worth reconciling. |
| **cousin** | the same recipe *skeleton* deliberately voiced into **different-sounding** instruments (e.g. one driven/distorted, one clean) | separate entries, cross-linked "see also" | not duplication — it's a reused *technique*. Fine, but the cross-link makes the shared trick visible. |
| **unique** | one cart only | one entry | nothing to reconcile. |

---

## Drums

### drum/french-house-kick
`INSTR_SINE` · A0 D480 S0 R60 · LP 250/3 · pitch-env →26 (0/50) · drive 0.30
Bridged-T boom — the saturated four-on-the-floor thud. The French-house *pump* is that
drive running hot, not a sidechain.
- tier: **shared** (byte-identical)
- origin: house
- used by: house (`SL_KICK`) · italo (`SL_KICK`)

### drum/house-clap
`INSTR_NOISE` · A0 D110 S0 R50 · BP 1100/5
Band-passed handclap — the disco backbeat.
- tier: **shared** (byte-identical)
- origin: house
- used by: house (`SL_CP`) · italo (`SL_CP`)

### drum/house-hats
`INSTR_SQUARE` · closed A0 D42 S0 R16 / open A0 D340 S0 R90 · HP 7000/3 · `instrument_choke(closed→open)`
A metal-bank square run high-passed to a tick; the closed hat chokes the open one.
- tier: **shared** (byte-identical, choke included)
- origin: house
- used by: house (`SL_HATC`/`SL_HATO`) · italo (`SL_HATC`/`SL_HATO`)

### drum/house-maraca
`INSTR_NOISE` · A0 D24 S0 R10 · HP 6500/2
A tiny high-passed noise tick — the maraca/shaker. (italo drops this one from the kit.)
- tier: unique
- origin: house
- used by: house (`SL_MAR`)

### drum/house-crash
`INSTR_SQUARE` · A0 D850 S0 R200 · HP 3440/3
The long square ring standing in for a crash cymbal.
- tier: **shared** (byte-identical)
- origin: house
- used by: house (`SL_CYM`) · italo (`SL_CYM`)

### drum/simmons-tom
`INSTR_SINE` · A0 D220 S0 R40 · LP 1800/2 · pitch-env →10 (0/110) · drive 0.2
The "dewww" — a downward pitch-swept sine, the electronic Simmons tom of '80s disco.
- tier: unique
- origin: italo
- used by: italo (`SL_TOM`)

> **The synth-drum kit vs the 808 box.** Two drum lineages run through the dance stations.
> house + italo share a characterful *808-style box* (long bridged-T `drum/french-house-kick`,
> SQUARE metal hats) — byte-identical, **shared**. citypop + motorik instead each hand-build
> a *generic synth kit* (sine kick + band-passed-noise snare + HP-noise hat) — same recipes,
> re-tuned, so they're **variant** families below. The kit is the single most-rebuilt thing
> in the radio family.

### drum/synth-kick
A short downward-pitch-swept sine — the generic four-on-the-floor synth thump (distinct
from house's long bridged-T boom).

| cart (slot) | recipe |
|---|---|
| citypop (`I_KICK`) | SINE A0 D80 S0 R30 · pitch-env →14 (0/40) — tight 80s pop |
| motorik (`I_KICK`) | SINE A0 D100 S0 R40 · pitch-env →12 (0/50) — the motorik thud |

- tier: **variant**
- origin: undetermined (both look hand-rolled; confirm against earlier stations)
- used by: citypop (`I_KICK`) · motorik (`I_KICK`)

### drum/noise-snare
A band-passed noise burst — the synth backbeat.

| cart (slot) | recipe |
|---|---|
| citypop (`I_SNARE`) | NOISE A0 D75 S0 R50 · BP 2400/5 · pitch-env →10 (0/30) — bright crack |
| motorik (`I_SNR`)   | NOISE A0 D70 S0 R45 · BP 1800/5 — darker, understated |

- tier: **variant**
- origin: undetermined
- used by: citypop (`I_SNARE`) · motorik (`I_SNR`)

### drum/noise-hat
A high-passed noise tick.

| cart (slot) | recipe |
|---|---|
| citypop (`I_HAT`) | NOISE A0 D16 **S2** R70 · HP 8000/2 — sustain>0 so the open hat *washes* |
| motorik (`I_HAT`) | NOISE A0 D12 S0 R26 · HP 8200/2 — a tight straight-8th tick |

- tier: **variant** (the sustain split makes citypop's wash vs motorik's tick — borderline cousin)
- origin: undetermined
- used by: citypop (`I_HAT`) · motorik (`I_HAT`)

## Bass

### saw/italo-seq-bass
`INSTR_SAW` · A1 D120 S5 R50 · LP 700/5 · cut-env →1400 (0/60) · drive 0.25
The octave-bouncing sequencer bass — a saw with a fast cutoff sparkle and a little grit.
- tier: unique
- origin: italo
- used by: italo (`I_BASS`)

### pulse/moog-bass
`INSTR_SAW`/`INSTR_SQUARE` (rolled per song) · A2 D90 S3 R100 · LP ~800·feel/1
The Moog octave-pulse bass — round saw or buzzy square depending on the seed's `bassWave`
roll. Drives the relentless motorik pulse.
- tier: unique
- origin: motorik
- used by: motorik (`I_BASS`)

### tri/disco-bass
A round triangle bass with a short pitch-env "pluck snap" — the funk/disco low end.
**Confirmed variant across two stations** (the numbers drift, the sound is the same idea):

| cart (slot) | recipe |
|---|---|
| house (`I_BASS`)   | TRI A1 D150 S4 R60 · LP 600/2 · pitch-env →4 (0/15) · *cutoff swept live by the ride* |
| citypop (`I_BASS`) | TRI A1 D160 S4 R70 · LP ~900·feel/2 · pitch-env →6 (0/18) — the "octave-pop" slap |

- tier: **variant**
- origin: house (likely; confirm against earlier stations)
- used by: house (`I_BASS`) · citypop (`I_BASS`)

## Comp & rhythm

### epiano/rhodes-detent
`INSTR_EPIANO` · A2 D0 S6 R900 · LP 2400/2 · h0.15 t0.4 m0.3
A Rhodes with the tine "detent" bark dialed low — the harmonic bed under the stabs.
- tier: unique
- origin: italo
- used by: italo (`I_KEYS`)

### tri/funk-guitar
`INSTR_TRI` · A0 D140 S1 R60 · cut-env →1200 (0/60) · *(filter follows feel)*
Clean two-voice funk guitar — 16th-note muted chucks and stabs. The fast cutoff env gives
each chuck its "scratch" attack.
- tier: unique
- origin: citypop
- used by: citypop (`I_GTR`)

## Stabs & leads

### fm/brass-stab
`INSTR_FM` · A2 D200 S3 R120 · LP 3200/3 · h0.18 t0.68 m0.22
2-op FM near a 1:1/2:1 ratio — bright, brassy, with a feedback edge. The orchestra-hit
punctuation.
- tier: unique
- origin: italo
- used by: italo (`I_STAB`)

### saw/house-stab
`INSTR_SAW` · A1 D130 S2 R60 · LP 1400/2 · drive 0.25 · *(cutoff is the filter ride)*
The "chopped sample" — a gritty saw stab standing in for a sliced disco loop. The static
1400 Hz cutoff is nominal; the ride re-aims `I_STAB`'s filter live every frame, which is
the whole French-house gesture.
- tier: unique
- origin: house
- used by: house (`I_STAB`)

### square/da-funk-lead
`INSTR_SQUARE` · A8 D140 S5 R100 · duty 0.30 · LP 2100/4 · pitch-LFO 5.4 Hz/0.08 · drive 0.45
The Da Funk mono lead — a PWM'd square, lightly vibrato'd, run **distorted** (drive 0.45).
The grit is the hook, not an accident.
- tier: **cousin** of `square/glossy-lead` — same PWM-square + ~5.5 Hz vibrato skeleton; the
  drive is what splits them (house = distorted hook, citypop = clean gloss).
- origin: house
- used by: house (`I_LEAD`)

### square/glossy-lead
`INSTR_SQUARE` · A6 D160 S5 R140 · duty 0.30 · LP 2800·feel/2 · pitch-LFO 5.6 Hz/0.10 · *(no drive)*
The bright, clean chorus synth of city pop — the glossy comp lead. Its **solo stop**
(citypop `I_SOLO`) is the same voice opened up: duty 0.32, vibrato 0.16, LP 4200/3 — louder
and brighter to sit on top.
- tier: **cousin** of `square/da-funk-lead` (clean vs distorted, same skeleton)
- origin: citypop
- used by: citypop (`I_LEAD` comp · `I_SOLO` solo stop)

### pluck/seq-arp
`INSTR_PLUCK` · A0 D200 S0 R120 · LP 3600/2 · h0.45 t0.7
A high Karplus-Strong pluck driven as a 16th-note sequencer arpeggio.
- tier: unique
- origin: italo
- used by: italo (`I_ARP`)

### pd/soaring-lead
`INSTR_PD` · A4 D160 S6 R140 · LP 2600/5 · h0.6 t0.55 m0.5
Resonant Casio-CZ phase-distortion topline, DCW sweep on the strike — the chorus hook.
- tier: unique
- origin: italo
- used by: italo (`I_LEAD`)

### saw/brass-section
`INSTR_SAW` · A4 D180 S5 R120 · LP 2000·feel/3 · pitch-env →−2 (0/40)
A saw stack standing in for a horn section, hitting the anticipations. The small **downward**
pitch-env (→−2) is the section's "fall" off each stab.
- tier: unique
- origin: citypop
- used by: citypop (`I_BRASS`)

### mallet/vibes
`INSTR_MALLET` · A1 D0 S7 R~1100–1200 · **h0.25 t0.50 m0.90** · LP ~2600/2
The jazz vibraphone — soft mallet, metal bar, long ring with motor tremolo (morph's top).
**This is the catalog's clearest copy-propagation case** and the first preset born in a
*showcase* cart rather than a station:

| cart (slot) | recipe (h / t / m) | provenance comment in source |
|---|---|---|
| **mallet.c** (`PRESET[3]` "vibes", key 4) | 0.25 / 0.50 / 0.90 | — the origin (a knob-position preset) |
| lowend (`I_LEAD`, opt-in)   | 0.25 / 0.50 / 0.90 | "the vibes preset from the **mallet cart** (key 4)" ✓ |
| motorik (`I_SOLO`)          | 0.25 / 0.50 / 0.90 | "the vibes preset (**lowend.c** key 4)" ✗ — actually mallet's |
| cocktail (`I_PSOLO`, MJQ)   | 0.25 / 0.50 / **0.85** | "the vibes preset from the **mallet cart**" ✓ |

- tier: **shared** (mallet/lowend/motorik macros byte-identical) — cocktail is a hair-off variant (m0.85)
- origin: **mallet.c** `PRESET[3]` "vibes" (key 4)
- used by: motorik (`I_SOLO`) · lowend (`I_LEAD`, opt-in behind `leadVibes`) · cocktail (`I_PSOLO`)
- ⚠ **provenance drift in the wild:** motorik's comment credits lowend.c, but lowend was
  only the middleman — the recipe is mallet.c's. A hand-copied preset whose chain of
  custody is *already* garbled in the comments. Prime candidate to extract into a real
  shared helper. (lowend/cocktail are referenced here for this preset only — not yet fully charted.)

## Pads

### saw/string-machine
`INSTR_SAW` · long attack (A280–360) · LP ~900–1100 · held 3-voice pad
A Solina-style saw string pad — the canvas the filter-pump rides on. **The same idea
recurs across stations with re-tuned numbers** (the clearest cross-genre echo in the
catalog so far):

| cart (slot) | recipe |
|---|---|
| italo (`I_STR`)   | A280 D500 S6 R800 · LP 1100/2 |
| house (`I_STR`)   | A320 D500 S6 R900 · LP 900/2 · *cutoff = the ride* |
| motorik (`I_PAD`) | A360 D600 S5 R1100 · LP 1800·feel/2 · slow pitch-LFO 0.18 Hz/0.05 (tape wow) |

- tier: **variant**
- origin: house (likely earliest of the three — all hand-rolled, none shares a symbol)
- used by: italo (`I_STR`) · house (`I_STR`) · motorik (`I_PAD`)

### organ/combo-drone
`INSTR_ORGAN` · A70 D200 S6 R360 · h0.19 t(rolled Farfisa↔Vox) m0.18 · LP 2200·feel/2 · held
A thin combo-organ registration held forever — the Farfisa/Vox drone bed. `timbre` is
seed-rolled (bright Farfisa vs dark Vox); `morph 0.18` adds a touch of scanner chorus.
The author's comment ties it to organ.c's registration #1.
- tier: unique
- origin: motorik (registration after organ.c #1)
- used by: motorik (`I_ORG`)

---

## Notes for growing this file

- One station at a time. Add its voices, and for any recipe that **matches** one already
  here, append to **used by** + set the tier (`shared` if byte-identical, `variant` if
  re-tuned). Never duplicate an entry.
- When a `used by` line crosses 2+ carts at tier **shared**, that recipe is a candidate
  to extract into a real shared helper (a `preset_*()` function or a header) so it stops
  being a drift-prone hand-copy. Flag it; don't auto-extract.
- Keep names `group/name`, grouped by engine or role, so the catalog sorts sensibly and
  the name hints at the sound.
