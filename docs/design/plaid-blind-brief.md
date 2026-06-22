# Plaid — the blind band brief

> Phase-1 intent-first brief for a candidate radio station (`plaid.c`), written from the
> music before reading any cousin cart (the firewall). Companion to the runbook
> [`../guides/radio-station-howto.md`](../guides/radio-station-howto.md) and the candidate
> note in [`future-stations.md`](future-stations.md) ("Plaid / B12 — melodic IDM, arps in
> odd meters, bells. Gentlest entry."). Siblings in the IDM wing:
> [`braindance-blind-brief.md`](braindance-blind-brief.md), [`squarepusher-blind-brief.md`](squarepusher-blind-brief.md).

## The one thing that must be true

The IDM wing already has two stations, and both are about **aggression**: `braindance`
(drill chaos + a sweet melody floating over it) and `squarepusher` (a bass tearing through
fusion chaos). Plaid is the **anti-brutality** pole of Warp — and that's exactly the gap:

1. **GENTLE, not chaotic.** Intricate but warm, emotional, music-box-like. No drill rolls, no
   shredding. The detail is in the *weave*, not the violence.
2. **INTERLOCKING ARPEGGIOS are the protagonist**, not a bass or a drum grammar. Several
   small melodic cells woven together so the melody *emerges from the overlap* — hand-knitted
   counterpoint.
3. **ODD METERS that FLOW.** 5s and 7s and shifting bars that *lilt* — you barely notice the
   math (the opposite of a limping Balkan aksak). The title *Not for Threes* is a meter joke.

## The record, from the music

Plaid (Andy Turner & Ed Handley, ex-Black Dog; *Tekkonkinkreet* OST; worked with Björk) is
warm melodic IDM — *Rest Proof Clockwork*, *Double Figure*, *Spokes*. The feel: bittersweet,
playful, nostalgic, intricate-but-tender. The parts:

- **Interlocking arps** ("Eyen", "Itsu") — two or three short bell/pluck lines, each simple,
  cycling at *different* lengths so they phase and interlock; the tune is what their overlap
  spells. The core of the sound.
- **Warm bell / mallet / FM timbres** — glassy, music-box, marimba-ish, a little detuned;
  rounded synth leads. Nostalgic.
- **Odd, flowing meters** — 5/4, 7/8, shifting bars, smoothed by the kit so they lilt.
- **Bittersweet modal harmony** — maj7 / add9 / sus, gentle modal drift, the occasional
  emotional lift. Jazzy but soft.
- **Gentle, detailed broken-beat** — clicky kick, soft rim/snare, brushed hats, woodblocks,
  a little vinyl dust; mid-tempo (~90–120). Detail in service of the tune, never aggressive.
- (**B12**, the parking-lot partner, is the Detroit-techno-flavored cousin — deeper, spacier,
  *Artificial Intelligence*-era Warp; a possible second mode/feel.)

## The brains

- **INTERLOCKING ARPEGGIOS — the headline (a NEW brain).** N arp voices (3–4), each a short
  cyclic cell — a seeded pattern of onsets + chord-tone indices — but with **different cycle
  LENGTHS** (e.g. 5, 7, 8 steps) running against the bar, so they drift and interlock and the
  combined melody is **emergent**. It's the melodic cousin of gamelan's **kotekan** (complementary
  onset masks) and eno's **coprime loops**, but pitched, over *moving modal harmony*, in *odd
  meters*. The new thing on the dial: interlocking *melodic* arps (gamelan interlocks rhythm/
  pitch in 12-TET pairs; this is 3–4 arps weaving a tune). Likely graduates to a shared header.
- **ODD-METER FLOWING GROOVE.** The bar is an odd length (5/7 → variable `BARSTEPS`), rolled
  per track; the kit *smooths* it so it lilts rather than limps (satie/Balkan proved odd bars;
  Plaid's contribution is making them *flow*).
- **BITTERSWEET MODAL CHORD BRAIN.** Soft extended chords (maj7/add9/sus) over a slow modal
  drift with gentle lifts — warmer and more static-pretty than cocktail/squarepusher's driving
  ii-V. The arps weave *over* it; accommodation keeps them consonant.
- **FORM = ACCRETION (process, not sections).** Arps and layers enter and recede, the weave
  thickens and thins, the melody develops — no hard verse/chorus (motorik proved the process
  form; Plaid's is *melodic layering*, not a one-chord pulse).

## The voices (palette mapping — to confirm in Phase 2)

| imagined voice | likely engine (confirm Phase 2) | notes |
|---|---|---|
| **the interlocking arps (×3–4)** | `INSTR_FM` (`fm/glass-bell`) · `INSTR_MALLET` (celesta/marimba) · `INSTR_PLUCK` | the protagonist; each arp its own timbre + cycle length. |
| **warm pad bed** | `INSTR_SAW`/`INSTR_PD` lush, slightly detuned | bittersweet maj7/add9 chords. |
| **rounded lead** (optional top) | `INSTR_TRI`/`INSTR_SAW` soft, glide | a melody riding the weave, or just the arps. |
| **gentle melodic bass** | `INSTR_SINE`/`INSTR_SAW` round sub | supportive, not the lead (unlike squarepusher). |
| **soft broken-beat kit** | `INSTR_SINE` kick · `INSTR_NOISE` rim/hat · woodblock · vinyl dust | clicky, detailed, never aggressive. |

Reuses our bell/FM/mallet/pad timbres — **no new engine API**; the new work is all brains.

## NOT SAMEY — the variety lever

1. **The METER + the arp WEAVE (per seed) — the big lever.** The odd meter (5/7/8/shifting)
   and the set of arps — how many, their cycle lengths, their timbres — define the track's
   whole feel. A sparse two-arp 5/4 vs a dense four-arp 7/8 weave are different records.
2. **Mode + key + the harmonic mood** — bright vs melancholy, how much drift/lift.
3. **Per-arp timbre rolls** — bell vs marimba vs pluck, detune, register.
4. **Tempo + the broken-beat kit pattern**; a **B12 "deep" mode** roll (spacier, slower).

## Player knobs (radio.h chrome)

- **weave** — density: how many arps are live, how busy the interlock.
- **tone** — bell/pad brightness.
- **tempo**, SPACE re-rolls the meter + weave; **B** the band (arp timbres).

## Window / face — the plaid weave itself (the name IS the visual)

The obvious, perfect face: a **woven tartan/plaid pattern** that the arps *build*. Each arp is
a colored **thread** (its cycle length sets the stripe spacing); the threads cross warp-and-weft
and **color where they overlap** — so the interlocking-arp brain is literally drawn as the
emerging plaid. The weave thickens with the accretion form. (Lineage: tango's bellows / eno's
loop-field / carlos's lattice — the generative mechanism made visible; here it's the band name.)

## What this station adds (why it earns the slot)

- **First INTERLOCKING-ARPEGGIO brain** — melodic kotekan in odd meters; nothing on the dial
  weaves 3–4 pitched arps of different cycle lengths into an emergent tune.
- **The gentle/warm pole of Warp IDM** — the anti-braindance/squarepusher; pretty, not violent.
- **Odd meters that FLOW** (not limp) — the kit-smoothed lilt.
- **The woven-plaid window** — the mechanism made visible, on-name.
- **Reuses** bell/FM/mallet/pad timbres and the `radio.h` metered chassis — the new work is the
  arp-weave brain + the odd-meter flowing groove + the modal-pretty harmony.

> **Phase-3 chassis hint** (not yet read): a metered station on the `radio.h` step clock
> (`braindance`/`squarepusher`/`cocktail`) — copy the *wiring*, set `BARSTEPS` to the rolled
> odd meter, and voice the arp-weave from this brief.
