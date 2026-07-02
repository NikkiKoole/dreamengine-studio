# Radiophonic Workshop — the vintage-sci-fi genre box: tape music the console was born to play

STATUS: IDEA / exploration (2026-07-02) — the Radiophonic Workshop row of the
[`tinyjam-racks.md`](tinyjam-racks.md) rack table, promoted to its own doc after the 2026-07-02
drift pass found it **fully engine-unblocked** (it was parked waiting on formant + AM/ring; both
shipped in June). The band and rhythm-section design below are settled enough to build from; the
**shape** (station vs rack vs playable — §4) is the open maker decision.

Companion reading: [`tinyjam-racks.md`](tinyjam-racks.md) (the rack program; lane format, export
tiers), [`tinyjam-racks-followup.md`](tinyjam-racks-followup.md) (bias knobs, the dancer),
[`future-stations.md`](future-stations.md) (station parking lot),
[`instrument-engines.md`](instrument-engines.md) §8.9 (engine ship state),
[`../guides/radio-station-howto.md`](../guides/radio-station-howto.md) +
[`../guides/cart-authoring-prompt.md`](../guides/cart-authoring-prompt.md) (the blind-brief
firewall — applies at build time, see §5).

## 1. The genre in one paragraph

The BBC Radiophonic Workshop (1958–1998; Delia Derbyshire, Daphne Oram, Brian Hodgson) made
electronic music *before synthesizers were instruments*: test oscillators, ring modulators, and
above all **tape** — loops, splices, varispeed, reverse — assembled by hand into the Doctor Who
theme, Dalek voices, and two decades of radio/TV score. Every sound built from almost nothing,
lo-fi surface, deep honest craft. It is the one genre in the rack table that *celebrates* sounding
like a fantasy console — the aesthetic argument for building it at all.

## 2. Why it's unblocked — RW technique → shipped surface (2026-07-02)

| Workshop technique | What ships today |
|---|---|
| test-oscillator leads | `INSTR_SINE`/`SAW` + three shipped lead carts: **`heldnotes.c` is literally a theremin** (the held-note API showcase), **`martenot.c` the 1928 ondes martenot** (ribbon lead — THE film-score sci-fi voice), `musicalsaw.c` the spooky cousin (playable `note_lfo` vibrato) |
| ring modulation (the Dalek) | `ringmod()`/`instrument_ringmod()` (`FX_RINGMOD`, 2026-06-14) |
| voice / ghost choir | `INSTR_VOICE` (2026-06-10; vowel/size/effort + consonant/coda) + the `formant()` vowel filter; `vox`/`voxpad`/`say` carts on the shelf |
| tape wow/flutter · varispeed · loops | `tape()` insert; `varispeed()` (ride-safe); `tapeloop.c` + `wowflutter.c`/`varispeed.c` carts |
| reverse / concrète smears | `grains()` + `grains_pitch(…, reverse)` |
| filter sweeps | master `filter()` (explicitly ride-safe) incl. `FILTER_BAND` |
| acid-adjacent bass | the tb303 voice recipe on `FILTER_DIODE` (heavy slide) |

Zero new engine work. The rack-table row's band (theremin lead / rapid 8-note arp / 303 heavy
slide / noise clicks-pops / wow-flutter) maps to shipped voices one-for-one — the leads three ways
over.

## 3. The rhythm section — concrète, not kit (designed 2026-07-02)

The Workshop's groove engine wasn't drums, it was **tape loops of unequal length phasing against
each other** — Delia Derbyshire's actual working method. That gives this box a signature rhythm
move no other rack/station has:

- **Polymetric lanes.** Per-lane loop length: a 5-step metallic clank cycling against a 16-step
  pulse never quite repeats. In a radio this is one modulo in `play_step()`; in a rack it's a
  per-lane `steps` count in the lane format (cheap, and `tinyjam-racks.md` §Open questions already
  wondered whether steps-per-bar should be a rack constant — here is the cart that answers "no").
- **The voices** (all recipes over shipped engines):
  - *pulse*: a dead, damped thud — `INSTR_SINE`/`INSTR_MEMBRANE`, the "struck mic'd table";
  - *ticks*: `INSTR_MALLET` through `instrument_ringmod()` — the struck-lampshade-plus-ring-mod
    lineage, literally;
  - *"hat"*: short fixed-Hz sine blips — BBC time pips / Sputnik telemetry, extremely idiomatic;
  - *fill*: a slow-attack noise swell cut dead (the backwards-tape cymbal); `grains` reverse is
    the deluxe version.
- **Splice feel, not humanize.** Bake small **constant** per-loop timing offsets — identical
  every cycle, like a physical splice — instead of per-pass randomness.
- **The medium breathes**: `tape()` wow/flutter on the master, so the whole groove sits on tape.

## 4. Build order — pieces in separation first (maker direction 2026-07-02)

**The maker's call: don't debut new sound ideas inside the combined box.** Several ingredients in
§3 have never been seen in separation — ship each as its own small cart first, get it *good*, and
only then combine a few. (The repo already works this way: `mt70` proved slot-stacking, `voxlab`
probed the voice, the filter-spec spike preceded acidrack.) The leads need nothing — theremin
(`heldnotes`), martenot, musicalsaw are proven solo carts. The unproven pieces, each a candidate
standalone cart:

- **the concrète tape-kit** — the §3 rhythm section as its own playable drum cart: polymetric
  unequal-length lanes, the four voices (thud / ringmod'd lampshade ticks / pips / reversed
  swell), splice offsets. This one cart proves most of §3 in isolation.
- **a ring-mod playable** — `INSTR_MALLET`/found-sounds through `instrument_ringmod()` as an
  instrument you play (nothing on the shelf showcases ring mod as a *voice* yet — `FX_RINGMOD`
  shipped as a pedal, no cart owns it).
- **BBC pips / test-tone blips** — likely folds into the tape-kit rather than earning a cart.

Related ingredient the maker flagged worth more use: **the live-looper** (`loopstation.c` — "the
first cart that records itself"). Overdubbing a theremin/martenot line *over* the tape-kit's
loops is the Workshop's actual workflow (performance onto tape), and it's the looper crossover
`tinyjam-racks.md` §Open questions + `input-recording-looper.md` already flag — this box is its
natural customer.

### The shape fork (open — maker deciding, after the pieces exist)

Three ways to spend the combined material; they are not exclusive but one goes first:

- **A. Station first** — a radiophonic station on the dial. Follows the proven
  station→rack pipeline (the station's `new_song()` becomes the rack's generator later); zero
  collision with the in-flight acidrack work (rack #2 shouldn't start before acidrack proves the
  chassis and triggers the `rack.h` extraction anyway). Cost: the polymetric-loop signature stays
  *invisible* — a radio performs it, nobody sees the unequal lanes.
- **B. Rack-first** — the rebirth-classic precedent: write the generator lane-native, skip the
  station. Shows the unequal-length lanes in the UI (the signature move is sequencer-native — its
  best argument). Cost: it would be a second rack in flight while acidrack increments 2–4 are
  unfinished, exactly what the second-customer rule says not to do.
- **C. Playable one-off** — a kaoss-lineage toy (ring-mod pad + pips + tape). Cheapest, showcases
  `INSTR_VOICE`+`ringmod` today, but feeds neither the dial nor the rack program.

The honest tension: the genre's *sound* fits a station, but its *signature rhythm idea* wants to
be seen, which is rack territory.

## 5. Process + naming notes

- **The blind-brief firewall still applies at build time**: whoever builds this imagines the
  ideal band from the genre *before* opening shelf carts
  ([`../guides/cart-authoring-prompt.md`](../guides/cart-authoring-prompt.md)). This doc is the
  codebase-mapping side (the `tinyjam-racks.md` convention: vibes arrive blind; mapping to the
  shelf is ours). §2–§3 are mapping + genre research, not a substitute for the brief.
- **Trademark**: "BBC Radiophonic Workshop" is a real BBC department. Per the
  [`product-notes.md`](product-notes.md) trademark rule the homage is fine free-side; a paid
  skin gets an original identity (the followup's `tj-…` naming riff).

## 6. Open questions

- **Seed vs polymetry**: coprime lane lengths make the full cycle very long — what exactly does a
  seed code reproduce, and does the radio.h seed-compat rule need a "phase epoch" convention?
- **The twist (scale lock)**: whole-tone? chromatic drones over a pedal? The rack table's
  universal-controls section wants one named scale per genre.
- Does the [`tinyjam-racks-followup.md`](tinyjam-racks-followup.md) §3 dancer debut here? A 1-bit
  segmented-LCD dancer reading polymetric trigger lanes is peak Workshop.
