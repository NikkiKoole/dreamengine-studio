# yacht-rack — the session-desk rack: the chord chart is the star

STATUS: SHIPPED (2026-07-02, same day as the design — the second design→cart-in-a-day rack) —
**`yachtrack.c` ("session desk"), all of it**: the chart+interpreters model exactly as specced
(chord row with the mu vocab + MU-IFY/SUS-MELT + root arrows + KEY, editable form row + GEAR,
5-lane drum skeleton with three drummer chairs + RECHART, the 32-step sax hook cell; bass
runs / comp anticipation / ghosts / swing / stabs / laid-back as per-player feel knobs), THE
BAND carried over whole, autosave, tap-to-nudge song-code entry, WAV export. **The radio→rack
seed handoff is PROVEN**: the generator is yacht's `new_song` verbatim and the acceptance
corpus runs green — 44 golden `(seed, songsum)` pairs harvested from the radio's new
`DE_TRACE` songsum watch, frozen into the cart's `spec()` (48 asserts,
`node tools/spec.js yachtrack`). Build notes vs the plan: **the radio already had a seed
display** (the `#%08X` on the marina dial — build step 1 shrank to the harvest watch), and
the **`rack.h` extraction is deferred**: the honest overlap with `acidrack` turned out to be
an *idiom* (accordion strips + mini playheads, finger routing, code digits — ~80 lines,
copied) rather than identical code; a third rack decides what actually extracts. Open tails
in the cart's de:meta todo (root-nudge drag, per-section loops, the naming pass).

> A new row for the [`tinyjam-racks.md`](tinyjam-racks.md) rack table (yacht wasn't in the
> original eight — but the doc already named it an "editing genre," and
> [`tinyjam-marketing.md`](../marketing/tinyjam/tinyjam-marketing.md) §3 flags yacht-rock as
> "✅ hot right now," the HBO DOCKumentary revival). Maker framing captured here (2026-07-02):
> acid's grid is notes — the pattern IS the song; yacht is the inverse — **harmony IS the
> song**, so the editing surface players actually want is the chord row, not a drum grid.

Companion reading: [`tinyjam-racks.md`](tinyjam-racks.md) (the rack program; lane format,
export tiers, trademark rule — this doc answers its open "per-lane humanize" question),
[`rebirth-classic.md`](rebirth-classic.md) (the shipped pilot this builds beside; accordion
layout + invariants inherited from it), [`song-codec.md`](song-codec.md) (READY on the board —
the yacht chart is tiny and is its natural first customer),
[`radio-instrument-options.md`](radio-instrument-options.md) (the yacht band chairs, carried
over whole), [`../guides/radio-station-howto.md`](../guides/radio-station-howto.md) +
`runtime/radio.h` (the seed rules the handoff depends on).

## Why this rack (what it proves that acidrack didn't)

- **The chord row becomes real.** `tinyjam-racks.md` sketches "4 bars × (root pc, quality),
  voice-led at playback" — no shipped rack exercises it. Yacht's whole identity is that row:
  five qualities including THE MU CHORD, tap-to-reharmonize while the band keeps playing.
- **The radio handoff.** `acidrack` had no station to stay compatible with; yacht is a shipped
  station (#12) with seed history already kept for `[` `]`. This rack proves the 8-hex
  song-code pipeline radio→rack — the "one dangerous part" (§3).
- **The interpreter model.** The program's open question — does a rack expose the radios'
  performance randomness as humanize knobs, or bake it? — gets its designed answer here:
  **lanes are the chart ("what's written"); players have feel knobs ("how it's played");
  the seed rolls defaults for both.**
- **A maximally-different `rack.h` customer #2.** acidrack is cells all the way down; yacht
  adds a chord row, a form row, and interpreter knobs. What survives *both* shapes is what
  belongs in `rack.h` (§5) — the second-customer rule working as intended.

## 1. Ground truth from `yacht.c` (research pass 2026-07-02)

### The seam is already in the code

Yacht already separates **composition** (`srnd`, the seeded stream, drawn only in
`new_song()`) from **performance** (`chance()`/`rnd()`, unseeded, rolled in `play_step()`
every step: 55% Purdie ghosts, 40% kick pickup, 25% open hat, `rnd(3)` timing dust,
`mel_note`'s `rnd(2)` tiebreak). The chart/interpreter split is not an imposition on this
cart — it's the code's existing architecture made visible and grabbable. Chart = what the
seed writes. Feel knobs = what `chance()` was rolling.

### Where every `Song` field lands

| seed field (`new_song`, yacht.c:152) | rack surface |
|---|---|
| `keyPc`, `minorKey` | key control on the chart view |
| `loop[4]` (root off + quality, 5-quality vocab incl. `Q_MU`) | **the chord row** — the star edit surface |
| `groove` (session / Purdie / CR-78) | the **drummer chair** — picking one rewrites the skeleton lanes + kit voices + feel defaults |
| `gearUp` (+2 last chorus), the bridge subdominant lift | **form row** properties |
| `melOn[6]`/`melN` (32-step onset grid!), `melReg` | the **sax lane** — already lane-shaped in the seed; + a register knob |
| `hasRide` | a generator-written cell row in the drum lanes |
| band timbre rolls (EP tine/growl, strat pick, bass filter) | chair-panel knobs (already live via THE BAND) |
| `title`, `freq`, tempo 92–114 | chrome + transport |

### What is performed, not written (→ interpreters + feel knobs)

- **The kit**: per-groove hardcoded step logic + chance rolls → deterministic *skeleton*
  becomes 5 trigger lanes (kick/snare/hat-c/hat-o/ride, 16 steps); the chance layer becomes
  GHOSTS / FILLS knobs; the Purdie 58% off-beat lands as a SWING knob.
- **Bass** (`bass_near` + the step-14 chromatic approach run *into the next chord*): stays a
  player reading the chord row — this is the reactive magic: edit a chord, the runs re-aim.
  Knob: RUNS (approach chance).
- **Epiano comp**: fixed hit-points (step 0 on even bars, step 6) + the and-of-4 anticipation
  pushing the *next* bar's chord, voice-led via `rad_lead_to`. Stays a player. Knob:
  ANTICIPATE. (A comp-rhythm lane is a compatible v2 — see open questions.)
- **Strat 9th-stabs, chorus-only pad, `level_of` density, `mel_note` chord-tone
  voice-leading**: all stay players. The sax lane edits *where* the hook lands; the player
  still decides *what note* fits the chord — chart + interpreter in one lane.

Slot pressure: none. Yacht uses ~10 slots (5 band + 5 kit; CR-78 re-`instrument()`s the same
kit slots) vs acidrack's 24 — no curation math needed.

## 2. The design

### The model in one line

**The generator writes the chart; the players read it; the knobs set the feel; the seed rolls
defaults for all of it.**

### The truth state (the save blob, ~80 bytes)

```
Chart {
    u8 keyPc, minorKey;
    Ch loop[4];                  // the chord row (off + quality)
    u8 form[8];                  // section per 8-bar slot: intro/verse/chorus/bridge/outro
    u8 gearUp;                   // form property: +2 on the last chorus
    u8 drum[5][16];              // skeleton lanes: 0 off / 1 hit / 2 accent
    u8 saxOn[32], melReg;        // the hook cell + register
    u8 drummer, feel[NFEEL];     // GHOSTS / SWING / RUNS / ANTICIPATE / FILLS (0..8 each)
    u8 chair[4];                 // ep / bass / lead / pad picks (THE BAND, carried over)
    u8 tempo;
    u32 seed;                    // provenance: the code this chart came from (0 = hand-made)
}
```

Persisted whole via `save_bytes()` — the same blob the program's `song.h` export tier will
read, and the natural first payload for [`song-codec.md`](song-codec.md) (it's small enough
to be a *shareable code even after editing* — the thing the raw seed code can't do).

### The chart view (the star, always visible)

- **Chord row** — 4 big cells showing `root + quality` labels (the radio's `chord_label`
  strings: `Cmu`, `Fmaj7`, `G9sus`…). Tap a cell's quality to cycle the five-quality vocab
  (mu → maj7 → m9 → 13 → 9sus); nudge the root by semitone. Two **flavor buttons** run the
  generator's own melt rolls on demand: **MU-IFY** (melt a maj7 into the Steely chord) and
  **SUS-MELT** (melt the V13 into a 9sus). Voice-leading stays at playback (`rad_lead_to`),
  so *any* edit sounds like session players, never block chords.
- **Form row** — 8 slots × section (the radio's fixed `FORM[8]` made editable: tap to cycle
  intro/verse/chorus/bridge/outro). The bridge slot carries the subdominant lift; the GEAR
  toggle arms the +2 on the final chorus. This is yacht's version of acidrack's chain row —
  "sections become pattern banks," but with interpreters the density (`level_of`) comes with
  the section for free. Playhead ticks along the row.

### The lanes

- **Drums**: 5 × 16 skeleton grid. The **drummer chair** (session / Purdie / CR-78) is
  preset-load semantics: picking a drummer rewrites the skeleton lanes, re-`instrument()`s
  the kit slots, and resets feel-knob defaults; cell edits layer on top after. Feel knobs
  ride playback live: GHOSTS (the 55% snare dust), SWING (the Purdie 58%), FILLS (pickup +
  open-hat chances).
- **The hook (sax) lane**: the 32-step onset grid, tapped like any lane — plus REGISTER and
  a BEHIND knob (the shipped +14ms laid-back placement, made visible). Note choice remains
  `mel_note`'s chord-tone voice-leading: the lane edits the *rhythm* of the hook, the player
  keeps it inside the changes.

### The players (no lanes, knobs only — v1)

| chair | reads | feel knobs |
|---|---|---|
| bass | chord row (+ next chord for the approach run) | RUNS, register |
| epiano comp | chord row (anticipation reads the *next* bar) | ANTICIPATE |
| strat stabs | chord row | stab density |
| strings/brass pad | chord row, chorus sections only | — |

THE BAND panel carries over whole (chairs + candidates from
[`radio-instrument-options.md`](radio-instrument-options.md), sel 0 = the shipped marina
sound), plus the seed's timbre rolls surfaced as chair knobs.

### 320×240, accordion (inherits acidrack's shell + both invariants)

- **Transport bar** (~24px): play/stop, tempo, song code, title.
- **The chart** (~48px, always visible — the star gets the fixed real estate): chord row +
  form row + flavor/GEAR buttons.
- **Accordion strips** (one expanded at a time, folded ≈ 22px with name LED + mini playhead +
  mute + level): **DRUMS** (grid + drummer chair + kit feel knobs), **HOOK** (sax lane +
  register/behind), **BAND** (chairs + player feel knobs), **FX/master** (tone, the radio's
  master chrome).
- Both acidrack invariants hold: **sound never depends on what's expanded** (the chart and
  scheduler are global; the accordion is pure view), and **folded ≠ blind** (mini playheads).

## 3. The seed handoff — the program milestone (the one dangerous part)

This rack proves the radio→rack pipeline. Rules, per radio.h's seed-compatibility law:

1. **The yacht radio grows a seed display first** — 8 hex chars on the marina chassis (it
   already keeps seed history for `[` `]`). Tiny, independent, ships on its own; also the
   template for house/every station later.
2. **The rack ports `new_song()` verbatim** — the *entire* `srnd` draw sequence in order,
   including the draws that don't look compositional: the band timbre rolls
   (`instrument_timbre`/`morph`/`filter`, yacht.c:184–188) and the title words all consume
   stream draws. Skip or reorder one and every code breaks.
3. **Chart derivation consumes zero `srnd` draws.** The drum skeletons are deterministic per
   groove; every lane fills from `sng` fields *after* the verbatim draws. Feel-knob defaults
   derive from the groove, not from new draws.
4. **Acceptance test = a `Song`-struct dump diff, not a schedule trace.** The performance
   layer is unseeded *by design* (that's the whole model), so radio-vs-rack traces legally
   differ in the `chance()` dust. The honest oracle: both sides dump the composed state
   (`sng` + the derived chart) for a corpus of seeds (e.g. 256) and byte-compare. A WAV
   listen A/B is the taste check on top, not the gate.

Song code semantics (program tier 2, unchanged): the 8-hex code reproduces the *unedited*
chart. Once edited, the chart blob is the truth (`save_bytes`) — and being ~80 bytes, it's
the [`song-codec.md`](song-codec.md) pilot payload for sharing *edited* songs as codes.

## 4. Identity & trademark

Nothing Roland-shaped here; the homage names (Steely Dan, Hall & Oates, Purdie) live in
`de:meta` `homage`/`lineage` where they belong, never on the faceplate. Original identity
from day one (the `moog` → "dream synth" precedent): the **session desk** — producer at the
console, the band as chairs, the chart on the music stand. Marina palette carried over from
the radio. Cart name = maker call; working name `yachtrack` (the `acidrack` parallel).

## 5. Build order

1. **Radio-side: the seed display** on yacht's chassis (independent, tiny; unblocks every
   future handoff, house included).
2. **The cart, playable** — self-contained, hand-authored default chart so it's an instrument
   before it's a generator: chart view (chord row + form row + flavor buttons), drum skeleton
   lanes + drummer chairs + feel knobs, the sax lane, THE BAND carried over, interpreters
   ported from `play_step` (at default knobs they play the radio's exact readings),
   `save_bytes` persistence.
3. **The handoff**: verbatim `new_song` port + zero-draw chart derivation; song-code entry on
   the transport; the `Song`-dump acceptance corpus (§3.4).
4. **Export**: WAV button (arms the `.bake/wav_request` live capture); song-code display;
   hand the edited-chart blob shape to [`song-codec.md`](song-codec.md) as its first
   customer.
5. **`rack.h` extraction, scoped honestly** — this build is customer #2, so the rule fires,
   but only for what genuinely survived both shapes: transport bar, accordion-strip chrome +
   mini playheads, lane-grid draw/edit block, song-code display/entry, WAV arm, save
   plumbing. The chord row, form row, and interpreter model stay cart-side until a third
   customer (a second chord-first rack) earns their extraction. Don't force the chart model
   into `rack.h` to make the extraction look bigger.

## Open questions

- **Per-section chord loops.** Real yacht songs change per section; the radio (and v1) run
  one loop with bridge/gear lifts. Compatible v2: a form slot gains a loop index (chord row
  becomes 2–3 loops). Decide after playing v1 — the lift trick covers more than expected.
- **A comp-rhythm lane in v2?** The epiano's hit-points as a 16-step lane (with the
  anticipation as a cell state) — only if ANTICIPATE + density knobs prove too coarse.
- **Drummer-chair rewrite semantics** when skeleton lanes are hand-edited: silently replace,
  confirm, or keep edits and swap voices only? Lean: swap voices + feel defaults, replace
  lanes only on an explicit long-press ("re-chart").
- **Feel knob scope** — one GHOSTS knob for the kit, or per-lane? Lean: per-player (kit-wide),
  matching the "players, not channels" model.
- **The XY play-pad** (universal control layout): smearing a scale-locked lead over the chart
  is the loopstation crossover — v1 omits it; the HOOK strip is the natural later home.
- **Naming** — maker call, as with acidrack.
