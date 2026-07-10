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

> **Supply vs demand.** This file (with `radio-voices.md`) is the **demand side** — what the
> 20 radio stations *use*. Its companion [`instrument-recipes.md`](instrument-recipes.md) is
> the **supply side** — every instrument recipe *available to grab* across the showcase carts,
> the Roland machines, and the whimsical instruments, organized by engine. Several presets
> named here trace straight to a supply-side recipe (e.g. `mallet/vibes` → mallet.c's
> "vibraphone"); the cross-references run both ways.

> **Status: complete — all 20 stations charted.** italo · house · citypop · motorik ·
> cocktail · lowend · bossa · dub · jangle · jingle · addis · yacht · roadhouse · satie ·
> gamelan · tango · carlos · exotica · ymo · ambient. See [`radio-voices.md`](radio-voices.md)
> for the per-station charts and the findings summary. If a new station reuses a recipe named
> here, add it to that preset's **used by** line rather than minting a duplicate.

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

**Naming rule — the test is "would a musician call these two different instruments or
techniques?"** If **yes**, they get separate names: a different **engine**, a different
**playing implement/technique** (brushes vs sticks — a real sound difference even though the
console fakes both with the same engine), or a character change you'd hear as a different
instrument (clean vs distorted). If it's the *same instrument merely re-tuned*
across carts (ADSR, filter, a macro nudged) — like one disco bass two stations both build —
that's **one name + a variations table**, not a new name. The `group/` prefix is
usually the engine (`tri/`, `fm/`, `mallet/`), or a family (`drum/`, `kit/`) for percussion,
so a name hints at what it is at a glance. The point: **talk about the choices a radio makes**
with names, not name every knob wiggle.

**Sharing tiers** — the reason this catalog exists:

| Tier | Means | Presentation | Watch out |
|---|---|---|---|
| **shared** | byte-identical recipe in 2+ carts | one entry, all carts in **used by** | hand-copied, *not* a shared symbol — editing one does **not** move the others. A latent drift bug. Candidates to extract into a real shared patch helper. |
| **variant** | the *same instrument* re-tuned — same engine + role, numbers drift but the sound is "the same thing" | one entry + a variants table (numbers per cart) | usually fine; if the drift is accidental, worth reconciling. |
| **cousin** | the same recipe *skeleton* deliberately voiced into **different-sounding** instruments (e.g. one driven/distorted, one clean) | separate entries, cross-linked "see also" | not duplication — it's a reused *technique*. Fine, but the cross-link makes the shared trick visible. |
| **unique** | one cart only | one entry | nothing to reconcile. |

**Borderline? Don't merge on suspicion — merge on proof.** When two voices are arguably "the
same instrument" but not clearly so, keep them as separate entries with a `kin:` cross-link
noting the relationship. Collapsing into one name is a *cut*, and a cut is worth making only
once a cluster has visibly piled up (a dozen near-identical uprights, say). The catalog's job
is to make that pile visible; the link is enough until the evidence is.

**Nights (intra-cart variety).** Some stations seed-roll a slot *per song*. Each genuinely
different instrument or technique the roll can land on is its **own named preset**, and the
voice chart lists them all — so you see every instrument a slot can become (cocktail's solo
stop → piano *or* vibes *or* guitar; its drums → brush kit *or* stick kit). Nothing hides
behind "it varies."

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

> **The synth-drum kit vs the 808 box.** Two drum lineages run through the dance/groove
> stations. house + italo share a characterful *808-style box* (long bridged-T
> `drum/french-house-kick`, SQUARE metal hats) — byte-identical, **shared**. citypop,
> motorik, lowend **and dub** instead each hand-build a *generic synth kit* (sine kick +
> band-passed-noise snare + HP-noise hat) — same recipes, re-tuned per cart, so they're
> **variant** families below. At **five stations** (citypop · motorik · lowend · dub ·
> jangle), this kit is by far the most-rebuilt thing in the radio family — past the point
> where a shared `drum_*()` helper would pay for itself. The strongest extract candidate;
> the bass (below) is the runner-up.

### drum/synth-kick
A short downward-pitch-swept sine — the generic four-on-the-floor synth thump (distinct
from house's long bridged-T boom).

| cart (slot) | recipe |
|---|---|
| citypop (`I_KICK`) | SINE A0 D80 S0 R30 · pitch-env →14 (0/40) — tight 80s pop |
| motorik (`I_KICK`) | SINE A0 D100 S0 R40 · pitch-env →12 (0/50) — the motorik thud |
| lowend (`I_KICK`)  | SINE A0 D95 S0 R35 · pitch-env →16 (0/50) — the boom-bap "boom" |
| dub (`I_KICK`)     | SINE A0 D110 S0 R45 · pitch-env →12 (0/55) — soft, deep one-drop |
| jangle (`I_KICK`)  | SINE A1 D90 S0 R40 · pitch-env →14 (0/45) — CR-78-ish thud |
| jingle (`I_KICK`)  | SINE A1 D85 S0 R40 · pitch-env →12 (0/40) — soft songwriter kick |

- tier: **variant** (6 stations)
- origin: undetermined (all hand-rolled; confirm against earlier stations)
- used by: citypop · motorik · lowend · dub · jangle · jingle (all `I_KICK`)

### drum/noise-snare
A band-passed noise burst — the synth backbeat.

| cart (slot) | recipe |
|---|---|
| citypop (`I_SNARE`) | NOISE A0 D75 S0 R50 · BP 2400/5 · pitch-env →10 (0/30) — bright crack |
| motorik (`I_SNR`)   | NOISE A0 D70 S0 R45 · BP 1800/5 — darker, understated |
| lowend (`I_SNARE`)  | NOISE A0 D85 S0 R40 · BP 1700/7 · pitch-env →14 (0/25) — the "bap," tight & low |
| jangle (`I_SNARE`)  | NOISE A0 D70 S0 R30 · BP 1100/7 — a boxy little snare |

- tier: **variant** (4 stations; dub uses a cross-stick instead of a snare)
- origin: undetermined
- used by: citypop (`I_SNARE`) · motorik (`I_SNR`) · lowend (`I_SNARE`) · jangle (`I_SNARE`)

### drum/noise-hat
A high-passed noise tick.

| cart (slot) | recipe |
|---|---|
| citypop (`I_HAT`) | NOISE A0 D16 **S2** R70 · HP 8000/2 — sustain>0 so the open hat *washes* |
| motorik (`I_HAT`) | NOISE A0 D12 S0 R26 · HP 8200/2 — a tight straight-8th tick |
| lowend (`I_HAT`)  | NOISE A0 D18 S0 R14 · HP 7500/3 — closed, lo-fi, sits back |
| dub (`I_HAT`)     | NOISE A0 D14 **S2** R60 · HP 7500/2 — sustain>0, washes (like citypop) |
| jangle (`I_HAT`)  | NOISE A0 D24 S0 R16 · HP 6500/3 — tight tick |
| jingle (`I_HAT`)  | NOISE A0 D20 S0 R14 · HP 7000/3 — tight tick |

- tier: **variant** (6 stations; the S2 "wash" cut — citypop, dub — vs the tight ticks — motorik, lowend, jangle, jingle)
- origin: undetermined
- used by: citypop · motorik · lowend · dub · jangle · jingle (all `I_HAT`)

### noise/vinyl-dust
`INSTR_NOISE` · A0 D10 S0 R8 · HP 4000/2
A tiny mid-high noise tick fired as continuous crackle — the lo-fi vinyl "dust" that
defines boom-bap's surface. A texture voice, not a kit piece.
- tier: unique
- origin: lowend
- used by: lowend (`I_VINYL`)

### noise/caxixi
A 16th-note shaker — high-passed noise with a soft attack. Two stations build a shaker this way:

| cart (slot) | recipe |
|---|---|
| bossa (`I_SHAKER`)  | NOISE A1 D45 S0 R25 · HP 5200/4 — caxixi |
| addis (`SL_SHAK`)   | NOISE A1 D36 S0 R20 · HP 5600/3 — ethio shaker 8ths |
| exotica (`SL_SHAK`) | NOISE A1 D40 S0 R22 · HP 5600/3 — lounge shaker |

- tier: **variant** (3 stations)
- origin: bossa (or earlier)
- used by: bossa (`I_SHAKER`) · addis (`SL_SHAK`) · exotica (`SL_SHAK`)
- kin: HP-noise like the `drum/noise-hat` family — but a shaker, not a hat.

### membrane/kebero
`INSTR_MEMBRANE` · A1 D0 S7 R280 · h0.45 t0.30 m0.20
The kebero / low drum on the one — the struck-drumhead engine tuned low, near-centre thump.
- tier: unique
- origin: addis
- used by: addis (`SL_KEB`)
- note: addis is the first charted station to use `INSTR_MEMBRANE` — real modelled drums
  instead of the synth kit. `kebero`/`conga`/`bongo` are one engine at three tunings (three
  different drums → three names, per the naming rule).

### noise/chicharra
`INSTR_NOISE` · A1 D34 S0 R20 · BP 3400/8
The chicharra — a tight band-passed scratch (behind the violin's bridge). tango has no drum
kit; the percussion is the band itself.
- tier: unique
- origin: tango
- used by: tango (`SL_CHIC`)

### noise/golpe
`INSTR_NOISE` · A0 D40 S0 R24 · LP 900/4 · pitch-env →4 (0/30)
The golpe — knuckles on the bandoneón box, a low woody knock.
- tier: unique
- origin: tango
- used by: tango (`SL_GLP`)

### sine/boo-bam
`INSTR_SINE` · A0 D190 S0 R70 · pitch-env →7 (0/40)
The boo-bam / low conga heartbeat — a SINE tuned drum with a pitch-env thump.
- tier: unique
- origin: exotica
- used by: exotica (`SL_CONGA`)
- kin: shares the SINE + pitch-env recipe of `drum/synth-kick`, voiced as a melodic low tom.

### fm/finger-cymbal
`INSTR_FM` · A0 D700 S0 R500 · h0.55 t0.75 m0.55
A tiny FM clang — the finger cymbal.
- tier: unique
- origin: exotica
- used by: exotica (`SL_FCYM`)
- kin: the FM family (a clang voicing).

### fx/aviary
The aviary — bird calls and a frog croak, **aleatoric performance** (pure engine `rnd`, never
seeded; "the birds never sing it the same way twice"):

| voice | recipe |
|---|---|
| chirp (`I_CHIRP`) | SINE A1 D60 S2 R30 · pitch-env →7 (0/50) · pitch-LFO 11 Hz/0.5 — falling chirp |
| swoop (`I_SWOOP`) | SINE A30 D300 S5 R160 · pitch-env →−9 (0/380, low→up) · pitch-LFO 6.5/0.35 — rising gull |
| frog (`I_FROG`)   | SQUARE A4 D90 S2 R40 · duty 0.18 · LP 700/5 · pitch-env →3 (0/90) — the croak |

- tier: unique (an FX set)
- origin: exotica
- used by: exotica (`I_CHIRP`/`I_SWOOP`/`I_FROG`)
- note: novelty FX, not pitched instruments — the only fully-aleatoric voice group in the catalog.

### membrane/kendang
A paired hand-drum (kendang) on `INSTR_MEMBRANE` — an interlocking high/low pair, one
drummer:

| drum | recipe |
|---|---|
| lanang, high (`I_KENL`) | MEMBRANE A1 D0 S7 R120 · h0.70 t0.60 m0.12 |
| wadon, low (`I_KENW`)   | MEMBRANE A1 D0 S7 R220 · h0.45 t0.28 m0.18 |

- tier: unique
- origin: gamelan
- used by: gamelan (`I_KENL` · `I_KENW`)
- kin: addis's MEMBRANE drums (`membrane/kebero`/`conga`/`bongo`) — same engine, gamelan
  hand-drum tuning.

### membrane/conga
`INSTR_MEMBRANE` · A1 D0 S7 R200 · h0.55 t0.35 m0.15
The open conga — the syncopated heart of the groove, mid-tuned.
- tier: **shared** (byte-identical across addis/afrobeat)
- origin: addis
- used by: addis (`SL_CONGA`) · afrobeat (`SL_CONGA`)

### membrane/bongo
`INSTR_MEMBRANE` · A1 D0 S7 R120 · h0.72 t0.65 m0.10
The bongo / edge-slap accents — bright, short, high-tuned hard mallet.
- tier: unique
- origin: addis
- used by: addis (`SL_BONGO`)

### mallet/gankogui-lpg
`INSTR_MALLET` · h0.60 t0.32 m0.30 · LP 2000/2 · `ENV_CUTOFF` 0/170/+3200 · pan +0.35
The Afrobeat *timeline* double bell (hi 86 / lo 79, euclid 7-in-16). **Was a bright FM bell
(`fm/gankogui-bell`) — it read harsh/clangy; re-voiced** to a soft struck-bar agogo (MALLET
sine modes) through a **LOW-PASS GATE**: the cutoff snaps open on the strike and closes as
the note rings (the `ENV_CUTOFF` decays to base) while the MALLET amp decays — brightness +
level fall together, the vactrol bonk (the easel/dubdesk west-coast voicing, per struck hit
so the euclid timeline stays sample-tight).
- tier: unique (kin: `mallet/celesta`; LPG technique kin `easel`/`dubdesk`)
- origin: afrobeat
- used by: afrobeat (`SL_BELL`)

### sine/afro-kick
`INSTR_SINE` · A1 D60 S0 R90 · pitch-env →28 (0/45)
A bespoke punchy synth kick — sine with a fast pitch thwack. Deliberately *not* the shared
`drum/synth-kick`: tuned for the syncopated Afrobeat pocket, not four-on-the-floor.
- tier: unique
- origin: afrobeat
- used by: afrobeat (`SL_KICK`)

### noise/afro-snare
`INSTR_NOISE` · A1 D0 S0 R120 · BP 1900/4
The crack for the backbeat *and* Tony Allen's scattered ghost notes — same voice, low
velocity off the grid (the broken-funk feel is performance rnd on the velocity + delay).
- tier: unique
- origin: afrobeat
- used by: afrobeat (`SL_SNR`)

### noise/afro-shekere
`INSTR_NOISE` · A1 D30 S0 R22 · HP 6000/3
The gourd shaker — continuous 16ths, accented on the beat. kin: `noise/caxixi` (the same
high-passed-noise shaker idea, a hair brighter/shorter).
- tier: unique (kin `noise/caxixi`)
- origin: afrobeat
- used by: afrobeat (`SL_SHK`)

### noise/cross-stick
A woody cross-stick / rim clave — tightly band-passed noise with a quick pitch blip for the
"tok." Two latin/reggae stations build it nearly identically:

| cart (slot) | recipe |
|---|---|
| bossa (`I_RIM`)  | NOISE A0 D28 S0 R18 · BP 1800/9 · pitch-env →18 (0/20) |
| dub (`I_RIM`)    | NOISE A0 D30 S0 R25 · BP 1600/7 · pitch-env →16 (0/20) · echo send 0.25 |
| jingle (`I_RIM`)  | NOISE A0 D26 S0 R18 · BP 1500/8 · pitch-env →16 (0/18) |
| exotica (`SL_RIM`)| NOISE A0 D26 S0 R16 · BP 2000/9 — clave tick (no pitch blip) |

- tier: **variant** (4 stations — cross-stick / clave; exotica's omits the pitch blip)
- origin: bossa (or earlier; confirm)
- used by: bossa (`I_RIM`) · dub (`I_RIM`) · jingle (`I_RIM`) · exotica (`SL_RIM`)

### kit/cocktail-brushes
The jazz trio kit played with **brushes** — soft, swept, the sweep is a sustained circular
wash. cocktail's drums on brush nights.

| piece | recipe |
|---|---|
| ride (`SL_RIDE`) | SQUARE A1 D260 R160 · HP 5400/3 — soft ping |
| hat (`SL_HAT`)   | NOISE A0 D30 R14 · HP 8200/3 |
| sweep (`SL_BRSH`)| NOISE A60 D300 S2 R200 · HP 4800/1 — the circular brush sweep |
| kick (`SL_KICK`) | SINE A0 D150 R60 · LP 220/1 · pitch-env →9 (0/35) — feathered |

- tier: unique
- origin: cocktail
- used by: cocktail (brush nights)
- pair: rolls against `kit/cocktail-sticks`. (Its HP-noise hat and pitch-enved sine kick are
  kin to the `drum/noise-hat` / `drum/synth-kick` families — same engines, jazz voicing.)

### kit/cocktail-sticks
The same trio kit played with **sticks** — brighter and tighter, and the brush sweep
becomes a short **tap**. cocktail's drums on stick nights.

| piece | recipe |
|---|---|
| ride (`SL_RIDE`) | SQUARE A0 D320 R180 · HP 6400/4 — brighter ring |
| hat (`SL_HAT`)   | NOISE A0 D22 R12 · HP 9000/3 — tighter |
| tap (`SL_BRSH`)  | NOISE A2 D60 R40 · BP 3200/4 — the sweep becomes a tap |
| kick (`SL_KICK`) | SINE A0 D170 R60 · LP 300/1 · pitch-env →11 (0/38) — forward |

- tier: unique
- origin: cocktail
- used by: cocktail (stick nights)
- pair: rolls against `kit/cocktail-brushes`.

### kit/yacht-studio **(voicings)**
A tight, dry **studio session kit** — same engines as the synth kit but more produced
(filtered kick, cut-env snare), seed-rolled between two "nights":

| piece | night A | night B |
|---|---|---|
| kick (`SL_KICK`) | SINE A0 D230 R50 · LP 300/2 · pitch-env →16 (0/35) | SINE A0 D170 R40 · LP 320/2 · pitch-env →13 (0/45) |
| snare (`SL_SN`)  | NOISE A0 D120 R60 · BP 1500/4 · cut-env →800 | NOISE A0 D130 R35 · BP 1700/3 · cut-env →900 |
| hats (`SL_HATC`/`SL_HATO`) | NOISE · HP 8000/7400 | NOISE · BP 7800/7200 |
| ride (`SL_RIDE`) | SQUARE A0 D300 R160 · HP 6000/4 (always on) | — |

- tier: unique (two nights)
- origin: yacht
- used by: yacht (`SL_KICK`/`SL_SN`/`SL_HATC`/`SL_HATO`/`SL_RIDE`)
- kin: the synth-kit families (`drum/synth-kick`, `drum/noise-snare`, `drum/noise-hat`) —
  same engines, but yacht produces them into a studio kit rather than a drum machine.

### kit/cr78
The CR-78 (CompuRhythm) kit — **voice circuits lifted from `cr78.c`** (the drum-machine
showcase cart). The first **whole-kit** showcase-cart lineage (vs single-preset borrows):

| piece | recipe |
|---|---|
| kick (`SL_KICK`)        | SINE A0 D170 R40 · LP 320/2 · pitch-env →13 (0/45) — soft round |
| snare shell (`SL_SNB`)  | SINE A0 D70 R25 · LP 900/1 · pitch-env →5 (0/25) — tonal layer |
| snare rattle (`SL_SNN`) | NOISE A0 D130 R35 · BP 1700/3 · cut-env →900 — noise layer |
| hats (`SL_HATC`/`SL_HATO`) | NOISE · BP 7800/7200 — the famous swish |
| metal beat (`SL_MET`)   | SQUARE A0 D210 R60 · BP 3100/6 |
| claves (`SL_CLV`)       | TRI A0 D36 R14 · LP 3600/6 |

- tier: unique (a whole kit)
- origin: **cr78.c** (the CompuRhythm showcase cart)
- used by: ymo (`SL_*`)
- note: its **two-layer snare** (tonal SINE shell + bandpassed NOISE rattle) is richer than
  the single `drum/noise-snare`. Kin to the synth-kit families, but a specific machine
  recreation — and the first time a station borrows an entire kit, not one voice.

### kit/roadhouse-live
A live rock trio kit — produced like yacht's, plus a tambourine:

| piece | recipe |
|---|---|
| kick (`SL_KICK`) | SINE A0 D240 R50 · LP 300/2 · pitch-env →15 (0/40) |
| snare (`SL_SN`)  | NOISE A0 D110 R55 · BP 1600/4 |
| hat (`SL_HATC`)  | NOISE A0 D32 R15 · HP 7800/3 |
| ride (`SL_RIDE`) | SQUARE A0 D280 R150 · HP 5800/4 |
| tamb (`SL_TAMB`) | NOISE A0 D80 R28 · BP 8200/9 — a tight band-passed tambourine |

- tier: unique
- origin: roadhouse
- used by: roadhouse (`SL_KICK`/`SL_SN`/`SL_HATC`/`SL_RIDE`/`SL_TAMB`)
- kin: the synth-kit families + yacht's `kit/yacht-studio` (same engines, live-rock produced).
  The tambourine is roadhouse's own.

## Bass

> **The bass is the second-most-rebuilt voice** (runner-up to the synth kit). Almost every
> station hand-rolls its own — and they cluster by engine: a **TRI pile** (`tri/disco-bass`,
> `tri/upright-bass`, `tri/fingered-bass`) and a **SINE pile** (`sine/gut-bass`,
> `sine/boom-bap-bass`, `sine/riddim-bass`, `sine/round-bass`), each a fingered/round bass differing mainly by
> the pitch-env "thumb/snap." Kept separate for now (each has a real feel), but this is the
> pile most likely to earn a `merge-on-proof` cut next. Watching it.

### saw/italo-seq-bass
`INSTR_SAW` · A1 D120 S5 R50 · LP 700/5 · cut-env →1400 (0/60) · drive 0.25
The octave-bouncing sequencer bass — a saw with a fast cutoff sparkle and a little grit.
- tier: unique
- origin: italo
- used by: italo (`I_BASS`)

### bass/yacht-fingered **(voicings)**
*One* round fingered-electric bass part (A2 D220 S4 R90 · pitch-env →3 (0/14)), seed-rolled
across **three oscillators** — the clearest "same part, different gear" case in the catalog
(contrast cocktail's two genuinely-different basses):

| voicing | recipe |
|---|---|
| TRI  | INSTR_TRI — round fingered electric |
| SINE | INSTR_SINE · LP 500/2 — pure low end |
| SAW  | INSTR_SAW · LP 900/2 — bright, slap-adjacent |

- tier: unique (one part, oscillator-rolled)
- origin: yacht
- used by: yacht (`I_BASS`)
- kin: touches both the TRI and SINE bass piles — but here it's deliberately one part you
  pick the oscillator for, not three conceived instruments.

### fm/rhodes-bass
`INSTR_FM` · A2 D500 S4 R180 · h0.15 (epiano detent) · seeded timbre/morph
roadhouse's Rhodes piano bass — `INSTR_FM`'s epiano detent voiced an octave down for the
left-hand ostinato (the Doors' Ray Manzarek move). One of two ways `I_PBASS` is built.
- tier: unique
- origin: roadhouse
- used by: roadhouse (`I_PBASS`, Rhodes-bass mode)
- kin: the `INSTR_FM` voices (`fm/rhodes`, `fm/ostinato-bass`). I_PBASS rolls between this and
  `tri/upright-bass` (the session-bassist night).

### square/hosono-bass
`INSTR_SQUARE` · A1 D140 S4 R60 · duty 0.35 · cut-env →700 (0/70)
ymo's Hosono counterpoint bass — a PWM **synth** bass (not an upright fake), a melodic
8th-note second line. A new oscillator for the bass piles (SQUARE, alongside TRI/SINE/SAW/FM).
- tier: unique
- origin: ymo
- used by: ymo (`I_BASS`)
- kin: the PWM-square family (duty + the square engine) used as a bass.

### fm/ostinato-bass
`INSTR_FM` · A2 D240 S5 R160 · h0.25 t0.30 · pitch-env →2 (0/16)
addis's electric/upright bass — 2-op FM at a low harmonic ratio (fundamental-heavy, round),
with a short pitch-env attack. The ethio-jazz ostinato.
- tier: unique
- origin: addis
- used by: addis (`I_BASS`)
- kin: a third bass engine alongside the TRI and SINE piles (and italo's SAW, motorik's
  pulse); same `INSTR_FM` engine as `fm/brass-stab`, voiced as a bass.

### pulse/moog-bass
`INSTR_SAW`/`INSTR_SQUARE` (rolled per song) · A2 D90 S3 R100 · LP ~800·feel/1
The Moog octave-pulse bass — round saw or buzzy square depending on the seed's `bassWave`
roll. Drives the relentless motorik pulse.
- tier: unique
- origin: motorik
- used by: motorik (`I_BASS`)

### tri/upright-bass
Woody fingered upright — a TRI walking bass, the short pitch-env →2 the "thumb." Two
non-dance stations build the session upright **near-identically** (a real cross-cart link):

| cart (slot) | recipe |
|---|---|
| cocktail (`I_BASS`, upright night)            | TRI A3 D300 S5 R110 · pitch-env →2 (0/16) |
| roadhouse (`I_PBASS`, session-bassist night)  | TRI A3 D300 S5 R110 · pitch-env →2 (0/16) · LP 480/1 |
| tango (`I_BASS`, marcato)                     | TRI A3 D280 S5 R100 · pitch-env →2 (0/16) |
| exotica (`I_BASS`, two-feel)                  | TRI A4 D260 S4 R140 · pitch-env →2 (0/18) — looser/longer |
| afrobeat (`I_BASS`, the ostinato)             | TRI A3 D280 S5 R130 · pitch-env →3 (0/18) · LP 700/1 — warm, replaced a farty FM |

- tier: **variant** (5 stations — the session upright; the first three near-byte-identical)
- origin: cocktail (or earlier)
- used by: cocktail (`I_BASS`) · roadhouse (`I_PBASS`) · tango (`I_BASS`) · exotica (`I_BASS`) · afrobeat (`I_BASS`)
- kin: the TRI-bass pile (`tri/disco-bass`, `tri/fingered-bass`). This one is the **acoustic
  jazz/classical** cluster's shared bass (vs the dance stations' disco-bass).

### tri/fingered-bass
`INSTR_TRI` · A2 D200 S4 R80 · LP 700/2
A soft, round fingered bass — bossa's surdo-pattern low end. No pluck snap (unlike the disco
basses), just warm and even.
- tier: unique
- origin: bossa
- used by: bossa (`I_BASS`)
- kin: joins the growing **TRI-bass pile** (`tri/disco-bass`, `tri/upright-bass`) — all
  fingered basses on TRI, differing by pitch-env snap/thumb. Three so far; watch it.

### sine/boom-bap-bass
`INSTR_SINE` · A4 D220 S5 R120 · LP 480/1 · pitch-env →4 (0/35) · drive 0.20
The Low End's star — a deep upright-ish sine with a long pitch-env "thump" and gentle tape
saturation (warmth, not fuzz). Front and centre, Ron-Carter-style.
- tier: unique
- origin: lowend
- used by: lowend (`I_BASS`)
- kin: same engine + pitch-env-thumb idea as cocktail's `sine/gut-bass` — both model a jazz
  upright on SINE. **Kept separate** (decided): lowend's is saturated/deeper (boom-bap sub),
  cocktail's a clean dark gut upright. If a cluster of near-identical uprights piles up
  later, collapse it *then* — on evidence, not on suspicion.

### sine/round-bass
"Round and simple" — a warm even SINE under the vamp, **no pitch-env at all** (the plainest
of the SINE basses). The songwriter pair build it near-identically:

| cart (slot) | recipe |
|---|---|
| jangle (`I_BASS`) | SINE A2 D250 S4 R90 · LP 600/1 |
| jingle (`I_BASS`) | SINE A3 D280 S4 R110 · LP 520/1 |

- tier: **variant** (the jangle/jingle songwriter pair)
- origin: jangle
- used by: jangle (`I_BASS`) · jingle (`I_BASS`) · napoleon (`I_BASS`, SERENADE/FRIENDS)
- kin: the wider SINE-bass pile (`sine/gut-bass`, `sine/boom-bap-bass`, `sine/riddim-bass`)
  — those add a pitch-env thumb/snap; these two omit it.

### sine/riddim-bass
`INSTR_SINE` · A3 D260 S5 R130 · LP 420·feel/1 · pitch-env →3 (0/30)
The dub riddim — deep, round, full of rests; the deepest thing on the dial. *The bassline is
the song.*
- tier: unique
- origin: dub
- used by: dub (`I_BASS`)
- kin: the SINE-bass pile (`sine/gut-bass`, `sine/boom-bap-bass`) — same engine + pitch-env
  idea, reggae-deep voicing.

### sine/gut-bass
`INSTR_SINE` · A4 D340 S5 R130 · LP 380/1 · pitch-env →2 (0/16)
Darker, rounder "gut strings" upright — cocktail's walking bass on its mellower nights.
- tier: unique
- origin: cocktail
- used by: cocktail (`I_BASS`, gut nights)

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

### tri/funk-slap
`INSTR_SAW` · A1 D150 S4 R70 · LP ~900·tone/4 · cut-env →1600 (0/70) · drive 0.22
The popping funk strut of napoleon's DANCE archetype ("Canned Heat") — a SAW bass with a fast
bright cutoff envelope (the slap "pop") and a little grit, playing a syncopated 16th line with
octave pops and ghost notes. Brighter/more aggressive than the `tri/disco-bass` pile — the
cut-env is the slap, where the disco basses use a small pitch-env pluck.
- tier: unique
- origin: napoleon
- used by: napoleon (`I_BASS`, DANCE)
- kin: the disco/funk low end (`tri/disco-bass`) — same role, a slapped SAW vs a plucked TRI.

### fm/afro-bass — RETIRED
`INSTR_FM` · A2 D240 S5 R160 · h0.25 t0.32 · pitch-env →2 (0/16)
Was afrobeat's syncopated bass ostinato — but the FM ratio/mod-index read **farty** (buzzy
low-mid sidebands). Re-voiced to an **upright** (`tri/upright-bass`, below). No current users;
kept as a cautionary note: a low-register FM voice with a non-fundamental ratio + audible
mod-index buzzes. kin: addis `fm/ostinato-bass`.
- tier: retired · origin: afrobeat (now on `tri/upright-bass`)

### guitar/guitarron
`INSTR_GUITAR` · A1 D0 S7 R400 · h0.35 t0.18 m0.50 · LP 1400/1
The mariachi guitarrón — the big fretless-gut acoustic bass, voiced low on the bodied-string
engine: dark, woody, a quick stop (mid mute), playing the root-fifth "boom" on the meter's
strong beats. kin: the `INSTR_GUITAR` family (afrobeat's guitars, air's nylon) — same engine,
a deep bass register.
- tier: unique
- origin: mariachi
- used by: mariachi (`I_GTRON`)

### saw/ms10-bass
`INSTR_SAW` · A2 D160 S4 R120 · LP 720/7 · cut-env →1500 (0/70) · drive 0.22
The fat resonant Korg **MS-10** mono-synth bass — a saw through a high-resonance lowpass with a
fast cut-env "wow" and a little drive: the propulsive riff that IS the song on Tunnel (and the
Ani Kuni drive). The slot also voices a *round* sibling for the other archetypes (res 2,
cut-env →900, drive 0.08). The first MS-10-style resonant analog bass on the dial.
- tier: unique (**new recipe**; round sibling folded in)
- origin: polopan
- used by: polopan (`I_BASS`, Tunnel/Ani Kuni riff + round elsewhere)
- kin: the SAW-bass idea (italo's `saw/italo-seq-bass`, carlos's `saw/fat-moog`) — same
  resonant-LP + cut-env + drive path, voiced as a driving mono bass.

## Comp & rhythm

### epiano/rhodes-detent
`INSTR_EPIANO` · A2 D0 S6 R900 · LP 2400/2 · h0.15 t0.4 m0.3
A Rhodes with the tine "detent" bark dialed low — the harmonic bed under the stabs.
- tier: unique
- origin: italo
- used by: italo (`I_KEYS`)

### epiano/wurli-comp
`INSTR_EPIANO` · A4 D0 S6 R900 · h0.55 m0.30 · (seeded timbre ~0.35)
addis's Wurlitzer comp — the EPIANO with harmonics pushed up toward the Wurli "bark."
- tier: unique
- origin: addis
- used by: addis (`I_KEYS`, EPIANO mode)
- kin: same `INSTR_EPIANO` engine as italo's `epiano/rhodes-detent` — opposite voicing
  (high-h Wurli bark vs low-h Rhodes detent). I_KEYS rolls between this and `organ/addis-comp`.

### epiano/funk-clav
`INSTR_EPIANO` · A1 D220 S3 R160 · h0.85 (Clav) t0.72 m0.5 · FILTER_BAND 1300·tone/6 ·
`instrument_follow(LFO_CUTOFF, 4/120, 1800)` (auto-wah)
napoleon's DANCE clavinet ("Canned Heat") — the EPIANO Clav model played as percussive 16th
chord chucks, with a touch-following auto-wah (a band-pass peak that opens when you dig in =
the quack). The in-cart stand-in for a real wah pedal (see [napoleon-effects-wants.md](../design/napoleon-effects-wants.md)).
- tier: unique
- origin: napoleon
- used by: napoleon (`I_COMP`, DANCE)
- kin: epiano.c's `epiano/clav` (the showcase Clavinet) — same model + env-wah idea, voiced
  for funk comping. The follower-wah trick is shared with afrobeat's rhythm-guitar stopgap.

### organ/addis-comp
`INSTR_ORGAN` · A6 D0 S7 R200 · h0.55 m0.30 (scanner chorus)
addis's tonewheel-organ comp — a fuller registration than dub's hollow reggae chop or
motorik's thin combo drone, with a little scanner-chorus motion.
- tier: unique
- origin: addis
- used by: addis (`I_KEYS`, ORGAN mode)
- kin: the `INSTR_ORGAN` family (`organ/combo-drone` motorik #1, `organ/reggae-chop` dub #0)
  — three stations, three registrations of the one engine.

### fm/rhodes
`INSTR_FM` · A2 D700 S3 R350 · h0.15 (the detent) · vol-LFO 4.6 Hz (session tremolo) · seeded timbre/morph
yacht's electric-piano center — an **FM** Rhodes (1:1 detent, tine on), with a slow session
tremolo. Voicings (seeded "gear change"): tremolo depth 0.10 vs 0.14; an alt voicing adds
`timbre 0.30`; and the slot can also render as a bright **PLUCK "clav"** (h0.35 t0.85 m0.15).
- tier: unique (voicings)
- origin: yacht
- used by: yacht (`I_EP`)
- kin: a 3rd "Rhodes" approach — italo/addis voice theirs on `INSTR_EPIANO`
  (`epiano/rhodes-detent`, `epiano/wurli-comp`); yacht uses `INSTR_FM` (same engine as
  `fm/brass-stab`, `fm/ostinato-bass`).

### user0/combo-organ **(voicings)**
`INSTR_USER0` (a drawn drawbar single-cycle wave, via `wave_set`) · A18 D90 S6 R120 · LP ~2600 · pitch-LFO 6.2 Hz (cheesy vibrato tab)
roadhouse's Vox-style combo organ — the **first station to pull the `wave_set` lever**: a
hand-drawn drawbar cycle on `INSTR_USER0` rather than the modelled `INSTR_ORGAN`. Its **solo
stop** (`I_ORGL`) is the same wave brighter (LP 3400+, deeper vibrato). Section voicings nudge
the filter/vibrato up for the jam.
- tier: unique (voicings + solo stop)
- origin: roadhouse
- used by: roadhouse (`I_ORG` comp · `I_ORGL` solo stop)
- kin: a 3rd "organ" approach — `organ/combo-drone`, `organ/reggae-chop`, `organ/addis-comp`
  use the modelled `INSTR_ORGAN`; roadhouse draws its own drawbar wave.

### pluck/krieger-guitar **(voicings)**
`INSTR_PLUCK` · A1 D0 S7 R800 · h0.55 · seeded timbre/morph
roadhouse's Krieger guitar line — a Karplus-Strong electric, seed-rolled across a pedalboard:

| voicing | recipe |
|---|---|
| clean    | LP 2400/2 · drive 0 |
| fuzz     | LP 1900/2 · drive 0.45 — darker, growling |
| flatpick | LP 2400/2 · timbre 0.75 · drive 0 — bright pick |

- tier: unique (voicings — one guitar, pedal/pick choices)
- origin: roadhouse
- used by: roadhouse (`I_GTR`)
- kin: the PLUCK-guitar family (`pluck/strat-stab`, `pluck/jangle-guitar`, `pluck/archtop`…).

### square/toy-organ
`INSTR_SQUARE` · A6 D130 S6 R130 · duty 0.44 · LP 2400·tone/3 · pitch-LFO 5.2 Hz/0.10 · drive 0.12
The deadpan **John Swihart score** identity in napoleon's SWIHART archetype — a deliberately
*cheap* combo/Casio organ: a fat-ish square with a cheesy vibrato tab and a hair of grit,
doubled by a **detune-twin** (a second SQUARE, `instrument_tune +0.08` = ~8¢ sharp) so the
pair beats like an out-of-tune toy organ. The melody is doubled again by a muted pizz guitar +
glockenspiel. Innocent, wonky, lo-fi — the cart's second new recipe.
- tier: unique (a detuned pair)
- origin: napoleon
- used by: napoleon (`I_LEAD` melody + `I_COMP` detune-twin, SWIHART)
- kin: roadhouse's `user0/combo-organ` (a *drawn-wave* combo organ) — a different approach to
  the same "cheap organ" goal; the detune-pair beating is gamelan's ombak trick, here for
  cheapness not shimmer. Wants a real chorus (napoleon-effects-wants.md).

### user0/bandoneon **(voicings)**
`INSTR_USER0` (a drawn free-reed single cycle, via `wave_set`) · vol-LFO (bellows tremble)
tango's bandoneón — the **2nd `INSTR_USER0` drawn-wave station** (after roadhouse's organ): a
hand-drawn free-reed cycle, two hands:

| hand | recipe |
|---|---|
| left (`I_BAND`)  | INSTR_USER0 A~ D320 S5 R120 · vol-LFO 4.2 Hz (seeded) — the chords |
| right (`I_BANDL`)| INSTR_USER0 A~ D220 S5 R110 · vol-LFO 5.6 Hz/0.08 (bellows tremble) — the canto + variación |

- tier: unique (two hands)
- origin: tango
- used by: tango (`I_BAND` · `I_BANDL`)
- kin: roadhouse's `user0/combo-organ` — both draw their own single-cycle wave via `wave_set`
  rather than using a modelled engine (`INSTR_REED` exists, but tango draws a free reed).

### pluck/strat-stab
`INSTR_PLUCK` · A1 D0 S7 R600 · h0.35 · LP 2800/1 · seeded timbre/morph
Clean Strat 9th-stabs — a bright Karplus-Strong electric, the AOR comp guitar.
- tier: unique
- origin: yacht
- used by: yacht (`I_GTR`)
- kin: the PLUCK-guitar family (`pluck/nylon-guitar`, `pluck/jangle-guitar`, `pluck/archtop`)
  — same engine, a bright-electric voicing.

### tri/tremolo-rhodes
`INSTR_TRI` · A8 D300 S3 R200 · LP 1600/2 · vol-LFO 4.2 Hz/0.10 (tremolo) · cut-env →900 (0/110, bark)
A *fake* Rhodes — TRI rather than `INSTR_EPIANO` — whose signature is the **volume-LFO
tremolo** and a cutoff "bark" on the attack. The wobbly electric-piano stabs of boom-bap.
- tier: unique
- origin: lowend
- used by: lowend (`I_RHODES`)
- kin: shares the TRI + volume-LFO tremolo skeleton with dub's `tri/organ-bubble`.

### tri/organ-bubble
`INSTR_TRI` · A4 D70 S3 R50 · vol-LFO 6.5 Hz/0.12 (tremolo)
A short, tremolo'd TRI stab — dub's "organ bubble," the bouncing offbeat keys.
- tier: unique
- origin: dub
- used by: dub (`I_ORG`)
- kin: same TRI + volume-LFO tremolo idea as lowend's `tri/tremolo-rhodes` (here short &
  bubbly, there a sustained EP).

### tri/skank-chop
`INSTR_TRI` · A1 D90 S0 R50 · cut-env →800 (0/50, filtered upstroke) · echo send
dub's offbeat skank — a choppy filtered TRI standing in for a guitar upstroke. One of two
ways dub builds the skank (a code choice).
- tier: unique
- origin: dub
- used by: dub (`I_SKANK`, guitar mode)
- pair: alternates with `organ/reggae-chop`.

### organ/reggae-chop
`INSTR_ORGAN` · A1 D90 S0 R50 · h0.06 t0.55 m0.00 · echo send
A real Hammond chop on the "reggae" registration — hollow and thin, no scanner motion (the
chop *is* the rhythm). dub's other skank voice.
- tier: unique
- origin: **organ.c preset 0** (the "reggae" registration) — a showcase-cart borrow, like
  `mallet/vibes` and `organ/combo-drone` (organ.c #1). Showcase carts are the de-facto
  preset library.
- used by: dub (`I_SKANK`, organ mode)
- pair: alternates with `tri/skank-chop`.

### tri/funk-guitar
`INSTR_TRI` · A0 D140 S1 R60 · cut-env →1200 (0/60) · *(filter follows feel)*
Clean two-voice funk guitar — 16th-note muted chucks and stabs. The fast cutoff env gives
each chuck its "scratch" attack.
- tier: unique
- origin: citypop
- used by: citypop (`I_GTR`)

### tri/felt-piano
A soft felt piano on `INSTR_TRI` (no modeled `INSTR_PIANO`), the cut-env giving "hammer
softness." satie's *only* instrument — solo piano, two hands at different register/tunings:

| hand | recipe |
|---|---|
| right (`I_PNO`)  | TRI A1 D700 S1 R500 · LP 2400/1 · cut-env →600 (0/200) — melody + chords |
| left (`I_PNOB`)  | TRI A2 D1100 S1 R700 · LP 1300/1 — longer, rounder |

- tier: unique
- origin: satie
- used by: satie (`I_PNO` · `I_PNOB`)
- kin: the **fake-piano-on-TRI** cluster — cocktail's `tri/felt-grand` + `sine/closed-lid-piano`
  do the same thing (no charted station uses the modeled `INSTR_PIANO`; they all fake it). satie's
  is the most sustained, voiced for held gymnopédies.
- ⬆ **upgrade candidate:** satie *predates* the `INSTR_PIANO` engine — the TRI fake is a fossil,
  not a choice. The modeled piano exists now; switching is a judgment call (TRI may sit better in
  this bare mix), but "fakes X, engine now models X" is exactly the kind of gap this catalog
  exists to make visible.

### tri/felt-grand
A soft, woody "felt grand" — a *fake* piano on TRI (cut-env = hammer softness). Two acoustic
stations build it near-identically (same cut-env →700/0/70):

| cart (slot) | recipe |
|---|---|
| cocktail (`I_PNO`, felt-grand night) | TRI A2 D600 S2 R240 · cut-env →700 (0/70) |
| tango (`I_PNO`, felt registration)   | TRI A2 D520 S2 R200 · cut-env →700 (0/70) |
| exotica (`I_VIBE`, felt-piano roll)  | TRI A2 D600 S2 R240 · cut-env →700 (0/70) — **byte-identical to cocktail** (its source comment says so) |

- tier: **variant** (the acoustic fake-piano cluster — exotica's is a verbatim copy)
- origin: cocktail (or earlier)
- used by: cocktail (`I_PNO`) · tango (`I_PNO`) · exotica (`I_VIBE`, one vibe-chair roll)
- kin: the fake-piano-on-TRI cluster (`sine/closed-lid-piano`, satie's `tri/felt-piano`).

### sine/closed-lid-piano
The dark, intimate "closed-lid" piano — a fake piano on SINE. The same two acoustic stations
build a dark registration this way:

| cart (slot) | recipe |
|---|---|
| cocktail (`I_PNO`, closed-lid night) | SINE A3 D700 S2 R260 · LP 1400/1 |
| tango (`I_PNO`, dark registration)   | SINE A2 D520 S2 R200 · LP 1100/1 · cut-env →400 (0/70) |

- tier: **variant** (the acoustic fake-piano cluster)
- origin: cocktail (or earlier)
- used by: cocktail (`I_PNO`) · tango (`I_PNO`)
- kin: `tri/felt-grand`, satie's `tri/felt-piano` — fake pianos all (none uses `INSTR_PIANO`).

### tri/piano-solo-stop
`INSTR_TRI` · A1 D500 S3 R220 · cut-env →900 (0/60)
The piano's brighter "solo stop" — same TRI family as the comp, opened up for the
`improv.h` solo.
- tier: unique
- origin: cocktail
- used by: cocktail (`I_PSOLO`, piano-stop nights)

### pluck/archtop
`INSTR_PLUCK` · A1 D0 S7 R700 · h0.50 t0.45 m0.5 · LP 2300/1
A warm archtop jazz guitar, neck pickup — the "Herb Ellis night" of cocktail's solo stop.
- tier: unique
- origin: cocktail
- used by: cocktail (`I_PSOLO`, Herb-Ellis nights)

### tri/jangle-guitar
A TRI guitar whose **constant chorus warble** (a pitch LFO + a soft tremolo under it) *is*
the sound — the jangle-pop guitar tone. The songwriter pair build it nearly identically,
jingle gentler:

| cart (slot) | recipe |
|---|---|
| jangle (`I_GTR`) | TRI A1 D350 S2 R180 · LP 2400/2 · pitch-LFO 5.5/0.12 · vol-LFO 4.7/0.08 |
| jingle (`I_GTR`) | TRI A1 D420 S2 R260 · LP 2000/2 · pitch-LFO 4.6/0.08 · vol-LFO 3.9/0.06 — gentler warble, longer ring |

- tier: **variant** (the jangle/jingle songwriter pair)
- origin: jangle
- used by: jangle (`I_GTR`, default) · jingle (`I_GTR`)
- pair: jangle opts into `pluck/jangle-guitar` (real KS string) via a code flag — the same
  fake↔real guitar toggle bossa uses (`tri/nylon-fake` / `pluck/nylon-guitar`). jingle has
  no real-string option.

### pluck/jangle-guitar
`INSTR_PLUCK` · A1 D0 S7 R180 · LP 2600/2 · h0.5 t0.75 m0.2 · pitch-LFO 5.5 Hz/0.12 · vol-LFO 4.7 Hz/0.08
The *real* jangle string — Karplus-Strong, ~1.2 s ring, bright pick near the bridge (it *is*
jangle), with the same chorus warble laid over the KS read tap.
- tier: unique
- origin: jangle
- used by: jangle (`I_GTR`, opt-in)
- pair: alternative to `tri/jangle-guitar`.

### tri/sequencer
`INSTR_TRI` · A0 D70 S2 R40
ymo's "conveyor belt" — a short, dry TRI pluck ticking chord tones on 8ths/16ths, the
sequencer layer.
- tier: unique
- origin: ymo
- used by: ymo (`I_ARP`)
- kin: italo's `pluck/seq-arp` — same arpeggiator role, a different (cheaper) oscillator.

### pluck/exotica-nylon
`INSTR_PLUCK` · A1 D0 S7 R900 · h0.42 · seeded timbre/morph
A soft nylon rhythm-guitar comp — longer ring than bossa's nylon (it comps rather than picks).
- tier: unique
- origin: exotica
- used by: exotica (`I_GTR`)
- kin: the PLUCK nylon guitars (`pluck/nylon-guitar`).

### tri/nylon-fake
`INSTR_TRI` · A1 D180 S1 R120 · LP 2200/3 · cut-env →1400 (0/90, attack sparkle)
bossa's nylon guitar comp — the *fake* (shipped default): a TRI with a cutoff sparkle on the
attack standing in for a plucked nylon string.
- tier: unique
- origin: bossa
- used by: bossa (`I_GTR`, default)
- pair: the slot opts into `pluck/nylon-guitar` (real string engine) via a code flag.

### pluck/nylon-guitar
`INSTR_PLUCK` · A1 D0 S7 R120 · LP 2200/3 · h0.40 t0.15 m0.45
The *real* nylon — Karplus-Strong with a short ring (nylon decays fast), soft thumb (no pick
brightness), mid-string warmth. bossa's opt-in guitar.
- tier: unique
- origin: bossa
- used by: bossa (`I_GTR`, opt-in)
- pair: alternative to the shipped `tri/nylon-fake`.

### guitar/afro-tenor  *(FIRST radio `INSTR_GUITAR`)*
`INSTR_GUITAR` · A1 D0 S7 R1200 · h0.20 t0.60 m0.85 · pan −0.55
The muted "tenor" rhythm guitar — tight pizzicato chuck (heavy mute, quick stop), the left
half of the interlocking weave. **No radio station had used `INSTR_GUITAR` before** (the
Karplus-Strong + resonant-body engine); the dance stations all faked guitar on PLUCK/TRI.
- tier: unique
- origin: afrobeat
- used by: afrobeat (`I_GTR1`)
- pair: interlocks with `guitar/afro-rhythm`.

### guitar/afro-rhythm
`INSTR_GUITAR` · A1 D0 S7 R1200 · h0.52 t0.70 m0.35 · pan +0.55
The brighter rhythm guitar — more body resonance, a touch more ring than the muted tenor;
the right half of the weave, panned opposite. The two-guitar interlock IS the Afrobeat sound.
- tier: unique
- origin: afrobeat
- used by: afrobeat (`I_GTR2`)
- pair: interlocks with `guitar/afro-tenor`.

### organ/afro-comp
`INSTR_ORGAN` · A1 D0 S7 R200 · h0.44 t0.55 m0.75
The combo-organ comp — jazz-B3 registration, percussion + scanner chorus (the morph). Held
cluster re-voiced per 2-bar via the shared voice-leader. kin: organ.c `organ/jimmy`.
- tier: unique (kin `organ/jimmy`)
- origin: afrobeat
- used by: afrobeat (`I_ORG`)

### guitar/vihuela
`INSTR_GUITAR` · A1 D0 S7 R700 · h0.70 t0.80 m0.55 · LP 4200/1 · pan +0.30
The vihuela — the small high 5-string that drives the mariachi *manico*. Bright, hard-strummed,
a short ring (the heavy-ish mute + quick stop give it the percussive chuck), voiced high. The
rhythmic engine of the band.
- tier: unique
- origin: mariachi
- used by: mariachi (`I_VIH`)

### guitar/mariachi-rhythm
`INSTR_GUITAR` · A1 D0 S7 R900 · h0.55 t0.55 m0.30 · LP 3200/1 · pan −0.25
The *guitarra de golpe* — the mid body-strum filling between vihuela and guitarrón. A chair
flag swaps it to a warmer **nylon** voicing (h0.45 t0.20 m0.30) for the bolero feel. kin: the
nylon/steel guitars (bossa/exotica/air) on the same engine.
- tier: unique (voicings — steel/nylon)
- origin: mariachi
- used by: mariachi (`I_GTR`)

## Stabs & leads

### fm/brass-stab
`INSTR_FM` · A2 D200 S3 R120 · LP 3200/3 · h0.18 t0.68 m0.22
2-op FM near a 1:1/2:1 ratio — bright, brassy, with a feedback edge. The orchestra-hit
punctuation.
- tier: unique
- origin: italo
- used by: italo (`I_STAB`)

### bowed/mariachi-violin  *(FIRST radio `INSTR_BOWED`)*
`INSTR_BOWED` · A24–70 D0 S7 R340 · h0.45 t0.30 m0.70 · vib 5.6 Hz/0.10 · scoop `ENV_PITCH`
(groove-gated: vals −1.0/80ms, son/huapango −0.25/38ms) · LP ~3400·tone
The mariachi violin section — the **real bowed string** (bowed.c's `bowed/violin` macros),
two desks (`I_VLN1`/`I_VLN2`, the 2nd a touch warmer h0.48 t0.32 m0.62) panned wide for the
ensemble spread, leading the copla in parallel thirds. The portamento **scoop INTO the note**
follows the groove — it sings on the slow vals, stays a hint on the staccato son (a deep scoop
on a short note just squeals; the attack shortens for fast passages too). **No radio station
used `INSTR_BOWED` before** — tango faked its violins on SAW.
- tier: unique
- origin: mariachi
- used by: mariachi (`I_VLN1`/`I_VLN2`)
- kin: tango's `saw/violins-arco` (the *faked* prior-art violin); bowed.c `bowed/violin` (the rig).

### saw/mariachi-strings
`INSTR_SAW` · A55 D200 S6 R320 · vib 5.6 Hz/0.09 · LP ~3400·tone
The **`sinte` fallback** for the violin chair — smooth saw synth-strings (no bow-friction
scratch), the soft alternative when the real bow is too aggressive. The faked-violin road
tango/ambient take. kin: tango's `saw/violins-arco`.
- tier: unique
- origin: mariachi
- used by: mariachi (`I_VLN1`/`I_VLN2`, sinte chair)

### brass/mariachi-trumpet
`INSTR_BRASS` · A1 D0 S4 R1200 · h0.15 t0.62 m0.42 · LP ~3800·tone/2
The two mariachi trumpets — bright, blatty real `INSTR_BRASS` (brass.c's `trumpet` macros),
voiced a third apart (`I_TPT1`/`I_TPT2`), trading call-and-response with the violins. A chair
flag (`una`) drops the second to a mellow flugel (h0.46 t0.30). kin: afrobeat's horn-section
trumpet — both real `INSTR_BRASS`, voiced bright.
- tier: unique (kin afrobeat trumpet)
- origin: mariachi
- used by: mariachi (`I_TPT1`/`I_TPT2`)

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

### tri/sparse-hook-lead
`INSTR_TRI` · A10 D180 S4 R200 · LP 2200/2 · pitch-LFO 5.4 Hz/0.12 (vibrato)
A soft TRI lead with a slow vibrato — lowend's sparse melodic hook (its shipped default;
the slot can opt into `mallet/vibes` instead).
- tier: unique
- origin: lowend
- used by: lowend (`I_LEAD`, default)

### mallet/addis-vibes
`INSTR_MALLET` · A1 D0 S7 R~1200–1500 · **seeded** h≈0.15 t≈0.42 m≈0.55 (per-song jitter)
addis's vibraphone — the star. Unlike the borrowed `mallet/vibes`, addis tunes its **own**
bar: silvery and motor-rung, with the macros seed-jittered (±0.01·srnd) so each song's vibe
is subtly different. An alt voicing drops to h0.06 t0.50 m0.28 (woodier, drier, shorter ring).
- tier: unique (two voicings, seeded)
- origin: addis
- used by: addis (`I_VIBE`, lead + improv.h solo)
- kin: `mallet/vibes` — same `INSTR_MALLET` vibraphone, but addis voices its own rather than
  copying mallet.c's preset. The one vibe cart that *didn't* lift the shared recipe.

### mallet/exotica-vibes
`INSTR_MALLET` · A1 D0 S7 R1500 · seeded h≈0.12 t≈0.40 m≈0.72 (**motor ON**)
exotica's vibraphone — the star, motor tremolo *on* (high morph) for the lounge shimmer;
macros seed-jittered per song. An alt voicing drops to h0.05 t0.45 m0.30 (woodier). The slot
can also roll into `tri/felt-grand` (a fake felt piano).
- tier: unique (voicings + a felt-piano roll)
- origin: exotica
- used by: exotica (`I_VIBE`)
- kin: the MALLET vibe family (`mallet/vibes`, `mallet/addis-vibes`) — exotica's runs the motor.

### mallet/celesta
`INSTR_MALLET` · A1 D500 S2 R400 · h0.50 t0.55 m0.45
The celesta — glass-bell sparkle. **`mallet.c` `PRESET[2]` "celesta"**, lifted into exotica's
bell chair: a **4th showcase-cart lineage** after vibes / the organ.c registrations / moog.c.
- tier: unique
- origin: **mallet.c `PRESET[2]` "celesta"**
- used by: exotica (`I_BELL`, celesta roll)
- kin: `mallet/vibes` (also from mallet.c); the bell chair rolls between this and `fm/glass-bell`.

### mallet/marimba
`INSTR_MALLET` · A1 D0 S7 R1200 · h0.00 t0.45 m0.35
The warm wooden struck bar — **`mallet.c` `PRESET[0]` "marimba"**, voiced into polopan's mallet
chair (Canopée's bounce, doubled with the pizzicato). The first charted *station* to use the
marimba preset (vs the vibe/celesta the others borrow). In Canopée's stacked-mallet outro it
piles up in octaves.
- tier: unique
- origin: **mallet.c `PRESET[0]` "marimba"**
- used by: polopan (`I_MAL`, Canopée roll)
- kin: the `mallet/vibes` / `mallet/celesta` family (all mallet.c) — the wood end of the bar.

### mallet/balafon
`INSTR_MALLET` · A1 D0 S7 R1200 · h0.10 t0.72 m0.18
Polo & Pan's harder, woodier African bar — brighter mallet (t0.72) and a shorter ring (m0.18)
than the round marimba, the West-African gourd-resonated xylophone. polopan's Nanga balafon.
- tier: unique (**new recipe**)
- origin: polopan
- used by: polopan (`I_MAL`, Nanga roll)
- kin: `mallet/marimba` — same engine, harder/drier; the balafon to its marimba.

### fm/glass-bell
`INSTR_FM` · A1 D500 S2 R400 · h0.55 t0.55 m0.12 (the 3.5 bell detent)
exotica's other glass sparkle — an FM bell on the 3.5-ratio detent. The bell chair rolls
between this and `mallet/celesta`.
- tier: unique
- origin: exotica
- used by: exotica (`I_BELL`, FM roll)
- kin: the FM family (`fm/brass-stab`, `fm/rhodes`).

### saw/violins-arco
`INSTR_SAW` · A70 D400 S6 R220 · pitch-LFO ~5 Hz (vibrato) · pitch-env scoop (→−1, seeded)
tango's violin section (one desk), **bowed** — a SAW with a slow attack, vibrato, and the
characteristic upward "scoop" into the note.
- tier: unique
- origin: tango
- used by: tango (`I_VLN`, arco sections)
- pair: rolls to `saw/violins-pizz` by section (arco ≠ pizzicato — different technique, like
  brushes vs sticks).

### saw/violins-pizz
`INSTR_SAW` · A2 D180 S0 R60 · LP 2000/1
The same violins **plucked** (pizzicato) — short, dry, for the síncopa.
- tier: unique
- origin: tango
- used by: tango (`I_VLN`, pizzicato sections)
- pair: rolls against `saw/violins-arco`.

### bowed/polopan-pizz
`INSTR_BOWED` (PIZZICATO via `eng_tune(slot,0,1)`) · A1 D0 S7 R400 · h0.30 t0.42 m0.30
The plucked-string bounce that IS Polo & Pan's groove — the **real** bowed-string waveguide
plucked instead of bowed (a warm finger pizzicato that rings down on its own), doubled with the
marimba on Canopée and comping the changes on Coeur Croisé. Where **mariachi** *bows*
`INSTR_BOWED` and **tango** *fakes* pizz on SAW (`saw/violins-pizz`), this plucks the modelled
string — the engine's own arco↔pizz split.
- tier: unique
- origin: polopan
- used by: polopan (`I_PIZZ`)
- kin: `saw/violins-pizz` (tango's faked pizz) and `bowed/mariachi-violin` (the bowed side of
  the same engine) — polopan is the BOWED *pizzicato* technique.

### mallet/bronze
`INSTR_MALLET` · A1 D0 S7 R1100 · h0.55 t0.40 m0.30 (ring, **no motor**) · `instrument_tune()` per degree
A bank of tuned bronze metallophones — **one MALLET slot per scale degree** (`I_BRONZE`+0..6),
each `instrument_tune()`'d to the sléndro/pélog scale. Higher harmonics than a vibe (toward
bell/metal), no motor tremolo. Seeded per-degree timbre jitter. The core gamelan voice.
- tier: unique
- origin: gamelan
- used by: gamelan (`I_BRONZE`+0..6 bank)
- note: **first station to use `instrument_tune()` for a microtonal scale bank** (non-12-TET).
- kin: the MALLET family (`mallet/vibes`, `mallet/addis-vibes`) — same engine, voiced as
  struck bronze rather than a motor vibraphone.

### mallet/gong
`INSTR_MALLET` · A1 D0 S7 R3500 · h0.95 (deep inharmonic bell) t0.20 m0.30 · echo 0.18
The kettle-gong (gong ageng) — a deep inharmonic bell, ~3.5 s ring, soft round strike, a
little pavilion air. Struck as a **detuned pair** (`I_GONG` + `I_GONG2`, ~7¢ apart) so the two
beat against each other: **ombak**, the gamelan shimmer.
- tier: unique
- origin: gamelan
- used by: gamelan (`I_GONG` + `I_GONG2` ombak twin)
- note: the **ombak** detuned-pair technique (`instrument_tune` + a few cents → beating) is
  gamelan's own — the first use of deliberate detune-for-beating in the catalog.
- kin: the MALLET family.

### reed/suling
`INSTR_REED` · A40 D0 S6 R220 · h0.30 (hollow pipe) t0.35 m0.45 (breath swell) · LP 2400/1 · `note_pitch` float
The suling bamboo flute — the **first charted use of `INSTR_REED`** (the blown-reed
waveguide). Played with continuous `note_pitch()` (truly gliding, not gridded to the bronze
bank), breath swell + a little vibrato on the macros.
- tier: unique
- origin: gamelan
- used by: gamelan (`I_SUL`)
- note: first station on the `INSTR_REED` engine.

### pd/synth-horn
`INSTR_PD` · A14–26 D0 S6 R~200 · h0.56–0.94 t0.55–0.70 m0.40–0.50 · LP 2600–3200
addis's horn line — `INSTR_PD` synth-brass (the engine's nearest thing to a reedy horn).
Two voicings: a tamed, reedy one (h0.56, LP 2600) and a brighter, buzzier one (h0.94, LP 3200).
- tier: unique (two voicings)
- origin: addis
- used by: addis (`I_HORN`, head unison + solo)
- kin: same `INSTR_PD` engine as italo's `pd/soaring-lead` — voiced as a brass section here,
  a lead there.

### square/whistle
`INSTR_SQUARE` · A18 D150 S5 R160 · duty 0.22 · LP 1900/2
A thin, slidey whistle lead — held legato (one voice, `note_pitch`/`note_glide` between
notes), the narrow 0.22 duty making it reedy and small. jangle's lazy topline. Its **solo
stop** (`I_SOLO`) opens it up: duty 0.24, a singing 5.3 Hz pitch-LFO wobble, LP 3200/2.
- tier: unique
- origin: jangle
- used by: jangle (`I_LEAD` comp · `I_SOLO` solo stop)
- kin: the PWM-square family (`square/da-funk-lead`, `square/glossy-lead`, `square/melodica`,
  `square/siren`) — same SQUARE + duty skeleton, thinnest member.

### square/melodica
`INSTR_SQUARE` · A12 D160 S5 R180 · duty 0.38 · pitch-LFO 5.0 Hz/0.18 · echo send 0.25
A reedy melodica — PWM square with a reed wobble, always singing into the tape echo. dub's
melodic voice. Its **solo stop** (`I_SOLO`, the solo.h jam melodica) is the same opened up:
duty 0.40, LP 2200/2, more echo (0.30), a touch brighter.
- tier: unique
- origin: dub
- used by: dub (`I_MELO` comp · `I_SOLO` solo stop)
- kin: the PWM-square family — `square/da-funk-lead`, `square/glossy-lead`, `square/siren`
  all share the SQUARE + duty + ~5 Hz pitch-LFO skeleton, voiced very differently.

### square/siren
`INSTR_SQUARE` · A5 D200 S4 R150 · duty 0.25 · LP 1800/4 · pitch-env →12 (0/220, long rise)
The dub "meltdown toy" — a PWM square with a long pitch-env sweep, fired as an FX siren.
- tier: unique
- origin: dub
- used by: dub (`I_SIREN`)
- kin: PWM-square family (see `square/melodica`).

### square/ymo-lead
`INSTR_SQUARE` · A4 D150 S5 R120 · duty 0.25 · pitch-LFO 5.6 Hz/0.12
"The proud square" — ymo's yonanuki-pentatonic melody lead. A clean PWM square with a
singing vibrato; the square wave *is* the instrument (no imitation).
- tier: unique
- origin: ymo
- used by: ymo (`I_LEAD`)
- kin: the PWM-square lead family (`square/glossy-lead`, `square/da-funk-lead`,
  `square/whistle`…) — the catalog's most-reused *technique* (square + duty + ~5.5 Hz vibrato),
  seven voices, none a copy.

### square/sax **(voicings)**
yacht's chorus lead — a breathy narrow-pulse "sax." Seed-rolls between three voicings:

| voicing | recipe |
|---|---|
| sax    | SQUARE A25 D160 S5 R130 · duty **0.12** (narrow, breathy) · LP 1900/2 · pitch-LFO 5.1/0.16 |
| synth  | SQUARE A25 D160 S5 R130 · duty **0.30** (fatter) · LP 2200/2 · pitch-LFO 5.1/0.10 |
| gtr solo | INSTR_PLUCK A1 D0 S6 R500 · h0.5 t0.75 · LP 2600/1 — a session guitar solo |

- tier: unique (voicings)
- origin: yacht
- used by: yacht (`I_SAX`)
- kin: sax/synth voicings are the PWM-square family (`square/whistle`, `square/melodica`…);
  the gtr-solo voicing is kin to `pluck/strat-stab`.

### sine/singing-lead
`INSTR_SINE` · A14 D200 S4 R240 · LP 2300/2 · pitch-LFO 5.1 Hz/0.12 (vibrato)
A soft, wordless "singing" lead — a SINE with a gentle vibrato, jingle's delicate topline.
Its **solo stop** (`I_SOLO`) is brighter: LP 3000/2, a touch more vibrato (5.2/0.16).
- tier: unique
- origin: jingle
- used by: jingle (`I_MEL` comp · `I_SOLO` solo stop)
- kin: the SINE-lead-with-vibrato idea also in bossa's `sine/breathy-flute` (there a flute,
  here a wordless vocal).

### voice/croon
`INSTR_VOICE` · A60 D80 S7 R220 · harmonics(VOWEL) 0.42 ahh / 0.18 ooh · timbre(SIZE) 0.66
(small tract = falsetto) · morph(EFFORT) 0.46 · pitch-LFO 5.4 Hz/0.35 (croon vibrato) · LP 2600·tone
napoleon's headline voice — a sweet, **sung** vibrato falsetto. Kip's serenade lead ("Always &
Forever", a vowel "ahh") and the Jamiroquai "ooh" stabs of the dance (a tighter vowel) are the
same recipe at two vowels. The jam ribbon hands the player this exact voice on the SERENADE
archetype (you sing the serenade). **The first radio voice to SING on `INSTR_VOICE`** — AIR
reached the engine first (`voice/air-vocoder`) but as a robotic vocoder lead, not a singer.
- tier: unique
- origin: napoleon
- used by: napoleon (`I_VOX` — SERENADE lead · DANCE "ooh" · FRIENDS voice; `I_SOLO` SERENADE jam)
- kin: air's `voice/air-vocoder` (the only other radio `INSTR_VOICE`) — a *cousin*: same engine,
  opposite intent (a sung croon vs a robot vocoder). The wordless-vocal idea also in jingle's
  `sine/singing-lead` (there faked on SINE; here the real formant engine).

### saw/fat-moog **(voicings)**
`INSTR_SAW` · resonant LP · cut-env "wow" (the Minimoog filter env) · drive (seeded)
The fat Moog — **the exact `moog.c` signal path** (SAW through a resonant lowpass with a
per-note filter-envelope "wow" and a little drive), driven by carlos's two-voice counterpoint
generator instead of a keyboard. The two voices of the invention:

| voice | recipe |
|---|---|
| upper (`I_UP`) | SAW A5 D220 S3 R170 · LP 2200/resU · cut-env →1300 (0/110) · drive — brighter |
| lower (`I_LO`) | SAW A6 D300 S3 R220 · LP 1450/resL · cut-env →1000 (0/150) · drive — rounder, fatter |

- tier: unique (two voices)
- origin: **`moog.c`** signal path (the dream-synth cart) — a showcase-cart lineage, like
  `mallet/vibes` / the organ.c registrations
- used by: carlos (`I_UP` · `I_LO`)
- kin: the SAW family — italo's `saw/italo-seq-bass` shares the cut-env + drive idea; this is
  the resonant-lead voicing.

### saw/moog-pedal
`INSTR_SAW` · A400 D600 S5 R1200 · LP 820/1 · pitch-LFO 0.16 Hz/0.04 (analog drift)
The sustained tonic pedal — the "Switched-On wall" held under the counterpoint, with a hair
of slow analog pitch drift.
- tier: unique
- origin: carlos
- used by: carlos (`I_PED`)
- kin: a held SAW drone like `saw/string-machine` — here a single sustained pedal.

### sine/breathy-flute
`INSTR_SINE` · A25 D120 S5 R140 · LP 2600/2 · pitch-LFO 5.2 Hz/0.18 (vibrato)
A breathy flute lead — slow sine attack with a singing vibrato. bossa's melodic voice. Its
**solo stop** (`I_SOLO`, the solo.h jam flute) is the same flute opened up: LP 3600, a touch
more vibrato (5.6/0.22), louder — to sit on top.
- tier: unique
- origin: bossa
- used by: bossa (`I_FLUTE` comp · `I_SOLO` solo stop)

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
- used by: motorik (`I_SOLO`) · lowend (`I_LEAD`, opt-in behind `leadVibes`) · cocktail (`I_PSOLO`, MJQ nights only) · air (`I_VIBE`, the Cherry/hook sparkle)
- ⚠ **provenance drift in the wild:** motorik's comment credits lowend.c, but lowend was
  only the middleman — the recipe is mallet.c's. A hand-copied preset whose chain of
  custody is *already* garbled in the comments. Prime candidate to extract into a real
  shared helper. (lowend/cocktail are referenced here for this preset only — not yet fully charted.)

### reed/afro-tenor-sax
`INSTR_REED` · A1 D0 S4 R1200 · h0.82 t0.30 m0.62 · pan −0.28
The breathy tenor sax — the top of the horn section *and* the soloist (the shared improviser
walks the mode through it). kin: reed.c `reed/tenor_sax` (the 2nd radio `INSTR_REED`, after
gamelan's suling). The "sax only" chair voicing opens it toward an alto (h0.78 t0.45 m0.55).
- tier: unique (kin reed.c `tenor_sax`)
- origin: afrobeat
- used by: afrobeat (`I_SAX`)

### brass/afro-trumpet  *(FIRST radio `INSTR_BRASS`)*
`INSTR_BRASS` · A1 D0 S4 R1200 · h0.15 t0.60 m0.42 · *(plays oct +1)* · pan +0.28
The bright blatty trumpet — voiced a 3rd below the sax to make the section, panned opposite
for width (the in-cart fake for the missing chorus effect; see [afrobeat-effects-wants.md](../design/afrobeat-effects-wants.md)).
**No radio station had used `INSTR_BRASS` before** — the only "brass" on the dial was faked
(italo's FM stabs, citypop's saw hits). kin: brass.c `brass/trumpet`.
- tier: unique (kin brass.c `trumpet`)
- origin: afrobeat
- used by: afrobeat (`I_TPT`)

## Pads

### saw/ambient-pad
`INSTR_SAW` · A1400 D600 S6 R2800 · LP 620/1 (LFO breathes it)
ambient's chord pad — a very slow-swell filtered saw, 4 held voices, long dark fade. The
extreme attack/release makes it pure wash.
- tier: unique
- origin: ambient
- used by: ambient (`I_PAD`)
- kin: the `saw/string-machine` pad pile — the slowest, darkest member.

### sine/sub-drone
`INSTR_SINE` · A900 D400 S6 R2000 · LP 300/1
A held sine sub-bass root — "felt, not heard." The floor under the drift.
- tier: unique
- origin: ambient
- used by: ambient (`I_SUB`)

### sine/bell
`INSTR_SINE` · A4 D1400 S0 R600 · LP 3400/2
A long, glassy sine bell that dies slowly — sparse arps on chord changes.
- tier: unique
- origin: ambient
- used by: ambient (`I_BELL`)
- kin: a 3rd "bell" approach — `fm/glass-bell` (FM) and `mallet/celesta` (MALLET) are the
  others; ambient's is the plainest, a pure sine.

### noise/wind
`INSTR_NOISE` · A2000 D800 S5 R2500 · BP 700/6
The "weather" — a held, band-passed noise drone with a 2-second swell. Texture, not rhythm
(unlike the short HP-noise hats/shakers).
- tier: unique
- origin: ambient
- used by: ambient (`I_WIND`)

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
| air (`I_PAD`), aka **`saw/solina-ensemble`** (kin `solina.c`) | A600 D400 S6 R1600 · LP 2000/2 · `instrument_tune` +0.05 (subtle divide-down beating) · pitch-LFO 0.18 Hz/0.05 (wow) · **THE ENSEMBLE = a real per-part `instrument_chorus`** (0.9/0.50, mix 0.58–0.70 via the strings chair) + reverb send + master `tape()` | the AIR signature — the genuine Solina recipe (saw + ensemble chorus, à la `solina.c`), at radio scale: ONE slot not six, and now the **per-part** chorus so it swirls the pad alone (master chorus off → Kelly's kit stays dry) |
| napoleon (`I_COMP` FOREVER / `I_AUX` SERENADE) | A220–360 D400 S6 R900–1200 · LP 1700–1800·tone/2 · `instrument_tune` +0.05/+0.06 (shimmer) · vol-LFO 4.2–4.4 Hz (string-machine chorus) | the 80s synth-pop wash (FOREVER) and the quiet-storm string pad under Kip's serenade — the pile's most generic, swelled members |

- tier: **variant**
- origin: house (likely earliest of the three — all hand-rolled, none shares a symbol)
- used by: italo (`I_STR`) · house (`I_STR`) · motorik (`I_PAD`) · air (`I_PAD`, the detuned-pair "solina-ensemble" voicing) · napoleon (`I_COMP`/`I_AUX`, FOREVER pad + SERENADE strings)

### pluck/drone
`INSTR_PLUCK` · A1 D0 S7 R1400 · h0.75 (long ring) · t0.25
roadhouse's open-string pedal — a long-ringing Karplus-Strong drone held under the moving
guitar line.
- tier: unique
- origin: roadhouse
- used by: roadhouse (`I_DRONE`)
- kin: the PLUCK family — here a sustained pedal rather than a played line.

### organ/combo-drone
`INSTR_ORGAN` · A70 D200 S6 R360 · h0.19 t(rolled Farfisa↔Vox) m0.18 · LP 2200·feel/2 · held
A thin combo-organ registration held forever — the Farfisa/Vox drone bed. `timbre` is
seed-rolled (bright Farfisa vs dark Vox); `morph 0.18` adds a touch of scanner chorus.
The author's comment ties it to organ.c's registration #1.
- tier: unique
- origin: motorik (registration after organ.c #1)
- used by: motorik (`I_ORG`)

### saw/yacht-strings **(voicings)**
yacht's "choruses-only" pad — a soft SAW string machine, with a syn-brass alternative:

| voicing | recipe |
|---|---|
| strings   | INSTR_SAW A260 D400 S5 R700 · LP 850/1 — soft strings |
| syn-brass | INSTR_SAW A10 D400 S5 R700 · LP 1400/1 · pitch-env →−2 (0/40) — short attack, the "fall" |

- tier: unique (voicings)
- origin: yacht
- used by: yacht (`I_PAD`)
- kin: the strings voicing joins the `saw/string-machine` pad pile; the syn-brass voicing
  reuses the exact `→−2` "fall" gesture of citypop's `saw/brass-section`.

---

## air (Moon Safari) — new recipes

The artist station reconfigures `I_BASS` and `I_LEAD` **per song archetype**, so several of
these are roll-selected, not fixed. Clustered here by cart (the file is by-engine elsewhere, but
an artist station's identity is the *set*). The Solina pad is folded into `saw/string-machine`
above (the detuned-pair "solina-ensemble" voicing); `mallet/vibes` is reused (see its entry).

### voice/air-vocoder  *(FIRST radio melodic `INSTR_VOICE`)*
`INSTR_VOICE` · A8 D90 S6 R220 · h0.62 (O→A vowel) t0.40 (size) m0.70 (pressed effort) · `instrument_drive` 0.25 · pitch-LFO 5.6 Hz/0.06 · LP 2400/2 · echo 0.14
The Kelly-Watch-the-Stars robot lead — a sung vowel, not a true vocoder (that needs the §8.10
formant/vocoder bus; see [`../design/air-effects-wants.md`](../design/air-effects-wants.md)).
No station had used `INSTR_VOICE` for a melodic lead before.
- tier: unique · origin: air · used by: air (`I_LEAD`, KELLY archetype)

### voice/polopan-chant
`INSTR_VOICE` · A40 D90 S7 R180 · h0.52 (≈"A" vowel) t0.42 (size) m0.55 (effort) · pitch-LFO 5.5 Hz/0.22
The Polo & Pan "ah-ah" sung chant — an open-vowel `INSTR_VOICE` lead with a rich vibrato, the
topline of the Canopée and Ani Kuni archetypes (the hypnotic Ani Kuni cell, doubled by the
glockenspiel). kin: air's `voice/air-vocoder` — the second melodic `INSTR_VOICE` on the dial,
voiced more open and vibrato-sung than air's pressed robot lead.
- tier: unique (kin `voice/air-vocoder`)
- origin: polopan
- used by: polopan (`I_STAR`, Canopée / Ani Kuni)

### pipe/polopan-flute
`INSTR_PIPE` · A45 D0 S6 R240 · h0.00 (overblow OFF) t0.42 m0.68 · register kept mid · pitch-LFO 5.0/0.14
Nanga's breathy flute — the showcase `pipe/flute` recipe. **Same intonation caveat air logged**
(PIPE goes sharp toward the top with overblow on / a high register), so overblow is OFF and the
melody sits mid-register — and polopan ships a live **`pipe`/`sine` A/B** in the band panel so
the player can swap to a clean in-tune SINE breathy flute (`sine/breathy-flute` kin) if the
waveguide still drifts on their ear. Engine note: `../design/air-effects-wants.md`.
- tier: unique (kin pipe.c `pipe/flute`, air's `pipe/air-flute`)
- origin: polopan
- used by: polopan (`I_STAR`, Nanga — A/B with a SINE fake)

### pipe/air-flute  *(FIRST radio `INSTR_PIPE`)*
`INSTR_PIPE` · A1 D0 S4 R1200 · **h0.00 (overblow OFF — stays out of the jet-gain "screech at
the top" zone), t0.38 air, m0.70 embouchure** · register 67–83 · pitch-LFO 5.0/0.08 · echo 0.22
The Cherry-Blossom-Girl flute — the showcase `pipe/flute` recipe verbatim. **Intonation note:**
overblow had to stay at 0 and the register kept low or PIPE goes audibly sharp up top — logged
in [`../design/air-effects-wants.md`](../design/air-effects-wants.md) ("Engine note").
- tier: unique (kin pipe.c `pipe/flute`) · origin: air · used by: air (`I_LEAD`, CHERRY)

### pd/air-moog-lead
`INSTR_PD` · A20 D320 S4 R300 · h0.94 (resonant wavetype) t0.55 m0.45 · pitch-LFO 5.0/0.10 · LP 2600/2 · echo 0.22
The resonant Moog topline (La Femme d'Argent melody, Sexy Boy hook). kin: pd.c `pd/reso-lead`,
italo's `pd/soaring-lead`.
- tier: unique (kin pd.c `reso-lead`) · origin: air · used by: air (`I_LEAD`, SEXY/ARGENT)

### reed/air-tenor-sax
`INSTR_REED` · A1 D0 S4 R1200 · h0.82 t0.30 m0.62 · sits ~22 ms late · echo 0.22
The smoky Playground-Love tenor — same recipe family as afrobeat's sax. kin: `reed/afro-tenor-sax`,
reed.c `reed/tenor_sax`.
- tier: unique (kin `reed/afro-tenor-sax`) · origin: air · used by: air (`I_LEAD`, PLAYGROUND)

### saw/air-fuzz-bass
`INSTR_SAW` · A2 D150 S4 R110 · LP 720/2 · ENV_CUTOFF 0/130/900 (pluck snap) · **`instrument_drive` 0.40–0.95 (seed-rolled fuzz)** · echo dry
The saturated Moog bass — the *star* of Sexy Boy (the grind) and Kelly's driving pulse.
- tier: unique · origin: air · used by: air (`I_BASS`, SEXY/KELLY)

### tri/air-round-bass
`INSTR_TRI` · A2 D220 S5 R130 · LP 560–760/2–3 (Argent sings higher) · clean
The warm fingered bass — voice-led roots for Playground/Cherry, and the **rolling melodic
ostinato** (root–5–oct–passing over the dorian loop) that is La Femme d'Argent's signature.
- tier: unique · origin: air · used by: air (`I_BASS`, ARGENT/PLAYGROUND/CHERRY)

### epiano/air-rhodes · epiano/air-wurli
`INSTR_EPIANO` · A2 D0 S6 R900 · LP 2200/2 · echo 0.14 · **Rhodes** h0.15 t0.35 m0.20 · **Wurli** (B-chair) h0.50 t0.42 m0.35
The comp bed for Argent/Playground (the B-chair swaps the model live).
- tier: unique · origin: air · used by: air (`I_EP`)

### guitar/air-nylon
`INSTR_GUITAR` · A1 D0 S7 R1100 · h0.45 (nylon body) t0.22 m0.24 · LP 2600/2 · echo 0.16
Cherry's fingerpicked arpeggios. kin: guitar.c `guitar/nylon`, afrobeat's guitars.
- tier: unique (kin guitar.c `nylon`) · origin: air · used by: air (`I_GTR`, CHERRY)

### sine/air-kick · noise/air-clap · noise/air-hat · sine/air-jam
The soft vintage kit + jam-strip lead: `sine/air-kick` (SINE, ENV_PITCH 0/42/16), `noise/air-clap`
(NOISE BP1500/4, claps on 2&4 / brushed on the ballad), `noise/air-hat` (NOISE HP6800/3), and
`sine/air-jam` (SINE, the `solo.h` strip lead — soft sine + 5.2 Hz vibrato + echo 0.22).
- tier: unique · origin: air · used by: air (`I_KICK`/`I_SNR`/`I_HAT`/`I_SOLO`)

---

## eno (Music for Airports) — new recipes

The beatless station that brought real `INSTR_PIANO` and a sustained `INSTR_VOICE` choir onto
the dial. All four voices are conditional on the seed-rolled SETUP (1/1 piano · 2/1 voices ·
1/2 duet · 2/2 synth); none uses a chord brain — each loop holds one in-mode note and the
harmony emerges from phase. Per-voice `note_pitch`-float detune (the ombak) makes paired loops
beat; held voices get a slow pitch-LFO (tape wow) + cutoff-LFO (breathing); bed reverb
`reverb(0.92, 0.35)`.

### piano/airports
`INSTR_PIANO` · A4 D2800 S0 R1400 · voicing rolled (grand h~0.1 most songs, glassy celesta
h~0.9 sometimes) · hammer `timbre` tracks the tone knob · `morph 0.85` (open pedal) · LP 2600·tone/1
The first radio piano — struck, rings down on its own (sustain 0); the loop just strikes it and
the long decay/release is the ring. The "can't sustain" trait that kept PIANO off the dial is
*correct* here: Eno's 1/1 piano IS struck-and-rings.
- tier: unique · origin: eno · used by: eno (`I_PIANO`, setups 1/1 + duet)
- kin: the faked TRI pianos (`tri/felt-grand`) everywhere else — this is the real engine.

### voice/choir-swell
`INSTR_VOICE` · A2400 D0 S7 R1400 · vowel `harmonics` rolled (O→A→E) · `timbre 0.45` (size) ·
effort `morph` tracks tone · `voice_nasal 0.10` · LP 2400·tone/1
The first SUSTAINED radio choir — a long-swell held vowel that holds while sounding then
releases into the gap. air's VOICE was a vocoder *lead*; this is held vowel pads.
- tier: unique · origin: eno · used by: eno (`I_VOICE`, setups 2/1 + duet)

### saw/airports-pad · pd/airports-drift
The synth bed. `saw/airports-pad` (`INSTR_SAW` A1800 D600 S7 R2600 · LP 650·tone/2) is the warm
drone bed (and the faint bed under the 1/1 piano); `pd/airports-drift` (`INSTR_PD` A2200 D600 S7
R2800 · `morph` rolled · LP 760·tone/2) is its morphing partner — together they are the 2/2
synth-only drone.
- tier: unique (saw bed kin `saw/ambient-pad`) · origin: eno · used by: eno (`I_PAD`/`I_PAD2`, setup 2/2 + 1/1 bed)

---

## plantasia (Music for Plants) — new recipes

The first melody-forward station, all-Moog: the **dream-synth (`moog.c`) signal path on loan,
voiced FOUR ways** — `INSTR_SAW` → `FILTER_LADDER` + per-note `ENV_CUTOFF` "wow" + `DRIVE`, the
same loan `carlos.c` took for two counterpoint voices, here spread across lead/bass/arp/bed.
The lead is a held mono voice driven per-frame so it glides (the seeded SONGWRITER tune).

### saw/moog-lead
`INSTR_SAW` · A6 D200 S6 R280 · `FILTER_LADDER` 1600·tone/4 · drive .42 · `ENV_CUTOFF` 6/260/1000 ·
`instrument_tune` .05 (a hair of Moog fat) · held + `note_glide` (per-feel 26–130ms) + vibrato `note_lfo` 5.5Hz/.18
The gliding, singing mono lead — the protagonist. Straight off moog.c's LEAD factory patch.
- tier: variant (moog.c LEAD, one-slot) · origin: plantasia · used by: plantasia (`I_LEAD`)
- kin: `carlos.c`'s Moog voices (same loan); the B-panel swaps the wave (saw/square/tri) live.

### saw/moog-bass
`INSTR_SAW` · A2 D170 S3 R160 · `FILTER_LADDER` 540/6 · drive .40 · `ENV_CUTOFF` 2/150/1700 (the snap)
The springy filter-pluck bass — the bounce is the fast filter contour. moog.c's BASS patch.
- tier: variant (moog.c BASS) · origin: plantasia · used by: plantasia (`I_BASS`)

### saw/moog-burble · saw/moog-bed
`saw/moog-burble` (`INSTR_SAW` A2 D90 S0 R110 · `FILTER_LADDER` 1200·tone/7 · `ENV_CUTOFF` 1/80/900)
is the percolating sequencer arp; `saw/moog-bed` (`INSTR_SAW` A60 D300 S5 R360 · `FILTER_LADDER`
820·tone/3 · slow `ENV_CUTOFF` 80/400/1500) is the warm chord bed (moog.c BRASS-ish swell).
- tier: variant (moog.c path) · origin: plantasia · used by: plantasia (`I_ARP`/`I_PAD`)

### mallet/celesta
`INSTR_MALLET` · h0.85 t0.40 m0.50 · S7 R700 — glassy celesta sparkle answering the lead.
- tier: unique · origin: plantasia · used by: plantasia (`I_BELL`)
- kin: `cocktail`'s vibes solo-chair (same MALLET, different macros).

---

## squarepusher (bass-virtuoso drill'n'bass) — new recipes

The most reuse-dense station: it borrows braindance's `ratchet()`+drum grammar, ymo's Hosono
bass walker, and cocktail's jazz harmony tables + `improv.h` bass solo. The genuinely new
voice is the **lead/slap bass** — the protagonist.

### saw/slap-bass
`INSTR_SAW` (or `INSTR_SQUARE` = fuzz) · A2 D150 S4 R150 · `FILTER_LADDER` 700·sel/6 (sel1
fuzz: 1100/9) · drive .40 (.62 fuzz) · `ENV_CUTOFF` 2/130/1900 (the slap/pluck snap) · paired
with `noise/slap-pop`
The lead fretless/slap fusion bass — moog-ish resonant path with a fast cutoff env for the
pluck. Played by the Hosono walker (groove/shred) and `improv.h` (solos).
- tier: unique · origin: squarepusher · used by: squarepusher (`I_BASS`)
- kin: `moog.c`'s BASS path (LADDER + cutoff env), here as a generative lead.

### noise/slap-pop · epiano/fusion-rhodes · saw/string-pad · saw/acid-303
`noise/slap-pop` (`INSTR_NOISE` BP2600, 22ms) is the slap transient layered on accented bass
notes; `epiano/fusion-rhodes` (`INSTR_EPIANO` h0/0.5 Rhodes/Wurli, LP2400) comps the extended
voicings; `saw/string-pad` (`INSTR_SAW` A220 D400 S6 LP1700 · `instrument_tune` .05) is the
tender-interlude bed; `saw/acid-303` (`INSTR_SAW` LP900/12 · `ENV_CUTOFF` 0/110/2300 · drive
.35) is the manic/solo squelch.
- tier: unique (acid kin braindance's `I_ACID`) · origin: squarepusher · used by: squarepusher (`I_SLAP`/`I_RHODES`/`I_PAD`/`I_ACID`)

---

## plaid (warm melodic IDM) — new recipes

The gentle IDM station: 3–4 interlocking arps (each a different cycle length) woven over a
modal drift. All existing timbres — the new work is the **interlocking-arp brain**, not the
voices. The four arps are deliberately *different* engines so the weave reads.

### fm/glass-bell · mallet/marimba · pluck/soft · fm/sparkle
The four interlocking arp voices: `fm/glass-bell` (`INSTR_FM` h0.62 t0.55 m0.12 D360 — the
glassy anchor); `mallet/marimba` (`INSTR_MALLET` h0.35 t0.5 m0.3 — wood→bell); `pluck/soft`
(`INSTR_PLUCK` h0.5 t0.35 LP2400); `fm/sparkle` (`INSTR_FM` h0.7 t0.65 D300 — the high octave).
Each on its own cycle length (5/7/8/9/11) + register; `instrument_tune` ±.04 on the two FM
voices for warmth.
- tier: unique (kin: braindance's `I_BELL` MALLET music-box; FM bells across the FM carts) · origin: plaid · used by: plaid (`I_ARP1..4`)

### saw/lush-pad · sine/round-bass · kit/soft-broken
`saw/lush-pad` (`INSTR_SAW` A240 D400 S6 R900 · LP1500 · `instrument_tune` .05) is the
bittersweet maj7/9 bed; `sine/round-bass` (`INSTR_SINE` A4 D260 S4 · LP600) follows the modal
drift, gentle; `kit/soft-broken` is a low-velocity broken-beat (SINE kick · NOISE rim/hat ·
woodblock) that *smooths* the odd meter so it lilts.
- tier: unique (pad kin `saw/ambient-pad`/`saw/string-machine`) · origin: plaid · used by: plaid (`I_PAD`/`I_BASS`/kit)

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
