# Squarepusher — the blind band brief

> Phase-1 intent-first brief for a candidate radio station (`squarepusher.c`), written
> from the music before reading any cousin cart (the firewall). Companion to the runbook
> [`../guides/radio-station-howto.md`](../guides/radio-station-howto.md) and the candidate
> note in [`future-stations.md`](future-stations.md) ("Squarepusher — the ratchet engine at
> maximum + the Hosono bassline generator turned virtuoso slap"). Sibling briefs:
> [`braindance-blind-brief.md`](braindance-blind-brief.md), [`plantasia-blind-brief.md`](plantasia-blind-brief.md).

## The one thing that must be true

There is already an Aphex/IDM station — **`braindance.c`** (drill'n'bass chaos + a sweet,
slightly-wrong melody floating over it: *beauty over brutality*). Squarepusher must NOT be
"braindance again." Its essence, from the music, is different in three ways:

1. **THE BASS IS THE PROTAGONIST.** Tom Jenkinson is a *bass virtuoso* — fretless and slap
   electric bass is the lead, not a sub. It solos, it runs impossibly fast melodic lines, it
   slaps and pops. braindance's lead is a music-box/synth; Squarepusher's lead is *a bass
   playing jazz-fusion*. (This is the docs' "Hosono bassline generator turned virtuoso slap.")
2. **JAZZ-FUSION HARMONY**, not a modal float. Real changes — extended/altered chords, ii-V
   motion, Jaco Pastorius / Weather Report color, chromatic fusion turnarounds. braindance
   floats one sweet-wrong scale; Squarepusher *moves through fusion harmony*.
3. **IT LURCHES between two worlds** — manic drill'n'bass mania and tender, gorgeous
   fusion-ballad (the "Tommib" / "Iambic 9 Poetry" calm). The record swings violently between
   them; that swing is the identity.

## The record, from the music

Squarepusher (1994–) is a *range* — like Aphex — but the through-line is **bass + jazz-fusion
meets hyper-edited breakbeat**. The poles:

- **Manic** ("Come On My Selector", "My Red Hot Car", "Greenways Trajectory"): impossibly fast
  programmed amen-break edits, ratchets past the point of possibility, a slap bass tearing
  virtuoso lines through it, acid squelch.
- **Fusion** (*Music Is Rotted One Note*, Shobaleader One): him on *live* instruments — fretless
  bass, drums, Rhodes — playing lo-fi jazz-rock. Real harmony, real groove, no programming.
- **Tender** ("Tommib", "Iambic 9 Poetry"): beatless/sparse, lush Rhodes-and-strings chords, a
  lyrical bass melody. The pretty interlude that makes the chaos land.

## The brains

- **BASS-AS-LEAD — the virtuoso bass brain (the headline).** The bass plays *the melody and
  the groove at once*: fast fusion lines, slapped/popped articulation, walking-into-changes
  when comping, shredding when soloing. Built from the **Hosono counterpoint generator (`ymo.c`)
  pushed to virtuoso density** + the **improviser (`improv.h`) bass-solo mode (`cocktail.c`,
  register ~45) cranked to max density**. The new layer is *slap articulation* — a transient
  pop/click on accented notes (a noise/click transient layered on the attack).
- **JAZZ-FUSION CHORD BRAIN.** Extended/altered changes — a cousin of `cocktail.c`'s ii-V walk
  but fusion-colored: sus chords, altered dominants (#9/b13), modal interchange, chromatic
  planing. Richer than braindance's single-scale float; this is the harmonic motion the bass
  improvises *over*.
- **RATCHETS AT MAXIMUM + volatility grammar — reused, pushed harder.** `braindance.c` already
  shipped the rhythm-as-grammar volatility knob and sample-exact ratchets; Squarepusher runs
  them *hotter* (more sub-hits, faster, the break chopped finer). Palette-shop reuse, not new.
- **THE TWO-MODE LURCH (form + the variety lever).** A song is a sequence of MODE blocks —
  MANIC / FUSION-JAM / TENDER / ACID — and the form *lurches* between them (a tender 8 bars
  detonating into drill chaos, the Squarepusher cut). The seed rolls the balance.

## The voices (palette mapping — to confirm in Phase 2)

| imagined voice | likely engine (confirm Phase 2) | notes |
|---|---|---|
| **lead/slap bass (protagonist)** | `INSTR_SAW`/pulse → resonant LP + fast `ENV_CUTOFF` + drive, glide for fretless slides; a `NOISE` click on slapped attacks | fretless = `note_glide` legato; slap = the transient pop. The Hosono generator + improv.h bass solo. |
| **drum chaos** | the `braindance.c` kit grammar (volatility + ratchets), pushed | reuse on purpose; cite braindance in the palette shop. |
| **fusion keys (Rhodes)** | `INSTR_EPIANO`/`INSTR_FM` (`fm/rhodes`) | extended-chord comping; the *Music Is Rotted* lo-fi Rhodes. |
| **lush pad / strings** | `INSTR_SAW` string-machine | the tender-interlude bed. |
| **acid squelch** | `INSTR_SAW` + live `note_cutoff`/`note_res` per step | the still-unclaimed fast acid squelch (also flagged for an Aphex-acid station). |

## NOT SAMEY — the mode lurch + per-song rolls

1. **The MODE balance (per seed) — the big lever.** How much of the song is manic vs fusion-jam
   vs tender vs acid, and how violently it lurches between them. A "mostly-tender with two
   detonations" seed vs a "relentless drill workout" seed are different records.
2. **Jazz-fusion key + changes** — which progression family, how altered/chromatic.
3. **Bass timbre** — fretless-round vs slap-bright vs fuzz-driven; glide amount, pop intensity.
4. **Volatility + ratchet density** — the drum chaos amount (the braindance knobs).

## Player knobs (radio.h chrome)

- **chaos** — drum volatility + ratchet density (calm groove ↔ drill detonation).
- **tone** — bass/keys brightness.
- **tempo**, **density** (arrangement), SPACE re-rolls the mode balance.

## Window / face — the LED mask / scope

Squarepusher performs behind an **LED face-mask/visor** (Shobaleader One) and his *Ufabulum*
shows are all **wireframe oscilloscope** visuals. Propose: a reactive **oscilloscope/waveform**
that the bass and drums drive — the bass line drawn as a glowing Lissajous/scope trace, the
drum hits flashing the wireframe — optionally framed as the LED mask. The bass made visible.

## What this station adds (why it earns the slot — distinct from braindance)

- **The first BASS-LED station** — the protagonist is a virtuoso fusion bass (the Hosono
  generator graduated, via `improv.h`, to a slapping frontman) — nothing on the dial does this.
- **Jazz-fusion harmony under IDM** — real altered/extended changes, not a modal float.
- **The two-mode lurch** — manic↔tender as a form brain (the violent swing that *is* Squarepusher).
- **Reuses, on purpose:** braindance's volatility+ratchet rhythm brains, ymo's Hosono bass,
  cocktail's improviser — the new work is the bass-as-lead brain + slap articulation + the
  fusion chord brain + the lurch.

> **Phase-3 chassis hint** (not yet read): a metered, improviser-driven cousin
> (`cocktail`/`braindance` run `improv.h`/grammar on the `radio.h` step clock) — copy the
> *wiring*, promote the bass to the lead chair, voice the band from this brief.
