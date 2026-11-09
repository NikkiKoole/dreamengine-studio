# Lo-fi jazzy hip-hop — the blind band brief

STATUS: SHIPPED — the lofi.c radio station is built (drunk-pocket time feel, hazy jazz harmony).
Later: [`radio-arranger-lessons.md`](radio-arranger-lessons.md) recast lofi.c on loficity's arranger.

> Phase-1 intent-first brief for a new radio station (`lofi.c`), written from the music
> before reading any cousin cart (the firewall). Companion to the runbook
> [`../guides/radio-station-howto.md`](../guides/radio-station-howto.md).

## The one thing that must be true (and what it is NOT)

We already ship **`lowend`** — jazz-rap **boom-bap** (the hard *Low End Theory* / Tribe lane).
This is the **other** jazzy-hip-hop pole: **lo-fi / Nujabes / J Dilla** — dreamier, jazzier,
hazier, the "beats to study to" mood. Two things define it:

1. **THE DRUNK POCKET** — the off-grid, behind-the-beat feel: the snare dragged late, the kick
   lazy, the hats swung, the whole thing wobbling like it was played by hand off a dusty record.
   **But ADJUSTABLE and tasteful** — a *pocket* knob from tight → drunk, defaulting to a
   moderate drag (never the seasick extreme unless the player cranks it). This is the brain
   `lowend` undersold; here it's a real, dialable time feel.
2. **LUSH JAZZ HARMONY, HAZY** — smoky extended Rhodes chords (maj9 / m11 / 13), a slow loop,
   wrapped in lo-fi crackle and tape warmth. The "jazzy" *and* the "lo-fi" at once.

## The record, from the music

Nujabes, J Dilla (*Donuts*), the lo-fi-beats canon, the jazzier Madlib. Relaxed (~78–92 bpm),
warm, melancholic-cozy. The parts:

- **Rhodes / electric piano** — the harmonic core: lush extended jazz voicings, soft, a little detuned.
- **Upright / round bass** — walking gently, warm, behind the beat.
- **The swung, dusty kit** — a soft boom-bap kick + a fat lazy snare + swung hats; the *pocket* is the point.
- **A sampled-feel melodic dab** — a muted horn / vibe / Rhodes lick, sparse, filtered like a chopped sample.
- **Lo-fi haze** — vinyl crackle, tape wow/flutter, a gentle high rolloff, a touch of echo/reverb.
- **A short jazz loop** — 2–4 extended chords, repeated, no big cadence.

## The brains

- **THE DRUNK POCKET (the headline, dialable).** Per-voice timing offsets: the snare lands a few
  ms LATE, the kick a touch lazy, hats swung — all scaled by a **pocket knob** (0 = on the grid,
  max = deep Dilla drag). Default ~moderate. Seeded humanize jitter on top (pure performance, so
  R replays the tune but the wobble breathes). The time brain `lowend` shipped at half strength,
  now done right *and* under the player's control.
- **HAZY JAZZ-LOOP CHORD BRAIN** — a short 2–4 chord extended loop (maj9/m11/13), slow and static;
  the seed rolls the loop + key. Smoky, not functional-driving.
- **THE LO-FI RACK** — reuse `vapor`'s set-and-hold processing: vinyl-crackle bed, tape
  (wow/flutter/sat), gentle echo/reverb, a high rolloff. Recombination, not new DSP.
- **SPARSE TOPLINE** — a chopped-sample-feel melodic dab (muted horn/vibe), filtered + reverbed,
  laid back; the loop and the pocket carry it, not a busy lead.

## The voices (palette — all native, no gaps)

| imagined voice | engine + recipe | notes |
|---|---|---|
| **Rhodes (the core)** | `INSTR_EPIANO` + gentle chorus | lush maj9/m11 comp |
| **round / upright bass** | `INSTR_TRI` upright / `INSTR_SINE` | warm, behind the beat |
| **dusty kit** | `INSTR_SINE` boom kick · `INSTR_NOISE` fat snare + swung hat | the drunk pocket lives here |
| **sampled dab** | `INSTR_REED` muted horn / `INSTR_MALLET` vibe | sparse, filtered, reverbed |
| **lo-fi haze** | `INSTR_NOISE` vinyl-crackle bed · `tape` · `echo`/`reverb` · high rolloff | reuse vapor's rack |

No gap — Rhodes + upright + a swung kit + the lo-fi rack are all in hand.

## NOT SAMEY — roll the mood

Roll a per-song **mood** (sets tempo, chord color, crackle amount, default pocket depth):
**sleepy** (slow, warm, deep crackle) · **dusty** (mid, sample-chopped, more drag) · **rainy**
(minor, melancholic, soft) · **sunny** (brighter major9, lighter swing). Plus key + the jazz loop.

## Player knobs (radio.h chrome)

- **pocket** — THE feel dial: tight (on-grid) → drunk (deep Dilla drag). Default moderate.
- **tone**, **tempo**; SPACE re-rolls the mood; **B** band (keys / bass / kit).

## Window / face — the cozy lo-fi scene

The "beats to study to" energy: a rainy window with running droplets, a desk lamp, a turntable
slowly spinning a record (the platter rotating), maybe a steaming mug / a dozing cat silhouette.
Warm dim palette (amber lamp, blue rain). The vinyl crackle made visible as the spinning platter.

## What this station adds (why it earns the slot)

- **The DRUNK-POCKET time brain** — dialable off-grid drag (snare late / lazy kick / swing),
  the loose feel `lowend` undersold, now a real and adjustable feature.
- **The lo-fi/Nujabes/Dilla mood** — distinct from `lowend`'s hard boom-bap; the cozy, hazy,
  jazz-school pole.
- **Reuses `vapor`'s lo-fi rack** + native Rhodes/upright — moderate build, one new brain.

> **Phase-3 chassis hint** (not yet read): a slow metered station on the `radio.h` clock (a
> cousin of `citypop`/`vapor`); the pocket = per-voice `schedule_hit` dly offsets scaled by the
> knob; the lo-fi FX set-and-hold (gated), as in `vapor`. Voice the jazz loop from this brief.
