# Research round: four-operator FM

> **STATUS: RESEARCH COMPLETE (2026-09-14)**, reference RENDERED AND CHARACTERISED (§7), paper
> design not started. The §5 round owed by [`engine-reach.md`](engine-reach.md) §7.2. It confirms
> four operators, settles both of the questions §7.2 said mattered more than the count, and
> **turns up a cheap fix to the 2-op engine we already ship** (§3), which is worth having whether or
> not the 4-op engine ever gets built.

## 1. The standard algorithm, and who published it

**Chowning, J. (1973), "The Synthesis of Complex Audio Spectra by Means of Frequency Modulation"**,
*Journal of the Audio Engineering Society* 21(7), pp. 526 to 534. That is the whole theory: one
oscillator modulates another, and the modulation index controls how much energy spreads into
sidebands at carrier ± n × modulator.

The **operator count and routing** are not from the paper, they are the hardware's. Six operators
with 32 algorithms is the DX7. **Four operators with eight algorithms is the TX81Z, DX21, DX100 and
DX9, and the same eight routings are shared across Yamaha's OPN, OPM and OPZ chip families.** That
matters more than it looks: the 4-op algorithm set is not a design decision we have to make and
defend, it is a standard that held across a decade of shipping hardware.

## 2. Both questions §7.2 flagged, answered

**Is it phase modulation or frequency modulation?** The honest answer is more interesting than the
confident one I put in §7.2. The modulating operator's output is added to the *phase accumulator* of
the carrier, which is phase modulation; Yamaha's own patent describes it as frequency modulation, and
the reverse-engineering literature calls the distinction contentious rather than settled. The
practical consequence is what matters and is not contentious: modulating phase is what keeps operator
feedback stable.

**We are already on the right side of this.** `sound_fm_sample` in `runtime/sound.h` computes
`de_sinf(v->phase * TWO_PI + m * beta)`, adding the modulator to the phase argument rather than to
the frequency increment, and its feedback path does the same. So a 4-op engine inherits a correct
foundation rather than a correction.

**Curated algorithm set or free routing matrix?** Curated, and §1 above is the reason: eight is what
the hardware standardised on, a matrix would wreck searchability (the matcher handles a snapped axis
well and a free routing graph not at all), and [`engine-reach.md`](engine-reach.md) §1 asks an engine
to have a mechanism rather than enough parameters.

## 3. The finding: our shipped FM cannot make an inharmonic spectrum

This came out of reading the reference and is measured in both directions. It is the most actionable
thing in this round.

`INSTR_FM`'s `harmonics` macro snaps to ten carrier:modulator ratios:

```c
static const float RATIO[10] = { 0.5, 1.0, 1.5, 2.0, 3.0, 3.5, 4.0, 5.0, 7.0, 14.0 };
```

Every one of those is rational with denominator 1 or 2. FM puts sidebands at carrier × (1 ± n·ratio),
so a ratio of p/q gives a spectrum that is **periodic at carrier/q**: a harmonic series, possibly with
gaps, but harmonic. The engine's own comment calls 1.5 and 3.5 "the deliberately clangorous ones",
and clangorous they are, but they are not inharmonic.

**Measured, our side.** The `fm` cart's `bell` preset sits on the 3.5 detent. Rendered, a 294 Hz note
puts partials at 294, 734 and 1762 Hz, which is 294 × 1, × 2.5 and × 6. A 392 Hz note gives 392, 980,
2352, the same multipliers. Half-integer multiples of the carrier, so a harmonic series on half the
fundamental.

**Measured, the reference.** STK's `TubeBell` is a bell, and its defining ratio is **√2** (the source
sets `setRatio(1, 1.414 * 0.995)` and `setRatio(3, 1.414)`). Rendered at 220 Hz its partials land at
91, 221, 400, 528, 712, 838, 1023, 1154, 1334 and 1465 Hz, which is exactly carrier ± n × 311 where
311 = √2 × 220. As multiples of the fundamental that is 0.41, 1.81, 2.39, 3.22, 3.79, 4.63, 5.22.
**No common fundamental. That is what inharmonic means, and that is what a bell is.**

So the gap is not "our FM is only 2-op". A 2-op FM at an irrational ratio makes an inharmonic
spectrum perfectly well. **The gap is that no detent in our table is irrational**, and the search can
only reach detents.

**The cheap fix, available now and independent of the 4-op engine.** Add one or two irrational
detents to that table (√2 = 1.414 is the bell; 2.414 and the golden ratio are the other classics).
This is a one-line change to a shipped engine plus a re-bless of whatever it moves, and it has a
measured failure waiting for it: [`patch-matching.md`](patch-matching.md) §8 records `sklocken` at
0.418 with FM as the least-wrong engine and MALLET at 1.58 even at its best macros. A bell-like target
that no engine can voice is exactly the shape of a missing irrational ratio.

**Caveat, stated rather than hidden.** Adding a detent changes what every existing `harmonics` value
maps to, because the index is `(int)(harm * 9.999)` over the table length. Existing carts and presets
would re-voice. The paper round has to decide between appending (11 detents, every old position
shifts) and replacing a near-duplicate (7.0 and 14.0 are both "high clang"). This is a real
compatibility cost, not a free win.

## 4. What good implementations do that the naive one does not

From the DX7 reverse-engineering work, each of these is a deliberate choice the obvious
implementation gets wrong:

- **Feedback needs an anti-hunting filter.** The DX7 averages the latest sample with the previous one
  in the feedback path. Without it, self-modulation hunts and aliases. **Our 2-op does not do this**:
  `v->fm_fb = m;` stores a single sample. Worth testing as a quality fix alongside §3.
- **Envelopes live in the log domain.** A 12-bit envelope value is Q8 fixed-point log2 gain, so linear
  gain is `2^(value/256)`, giving about 96 dB of range in roughly 0.0234 dB steps. Envelope segments
  are therefore **linear in dB**, not linear or exponential in amplitude. This is a real part of the
  DX feel and is not what our ADSR does.
- **The sine table is a log-sine table.** A quarter period of 1024 entries mirrors and flips into
  4096, at 14-bit fixed point, stored as log so that applying the envelope is an *addition* rather
  than a multiply. We have no reason to copy the fixed-point tricks, but the log-domain envelope is
  the part that shapes the sound.
- **The hardware lowpasses its own output** at a fixed 16 kHz to remove the spurious highs FM
  generates. An admission, in silicon, that FM aliases.

## 5. Cost

Four operators is four sine evaluations and four envelopes per sample per voice, against our current
two and one. Call it roughly twice our present FM at the same polyphony, against `SOUND_VOICES` of 32
whose own comment already says CPU is the real cost. State is small (a phase accumulator and an
envelope per operator, so tens of floats), so this is buffer-free in the
[`instrument-engines.md`](instrument-engines.md) §8.2 sense and needs no part of the shared per-voice
delay line.

The paper round should decide whether four operators run always or whether an algorithm using fewer
skips the unused ones, which is free to implement and matters at 32 voices.

## 6. Gotchas

| Gotcha | Source |
|---|---|
| **Feedback hunting and aliasing** without the two-sample average | DX7 reverse-engineering |
| **FM aliases by construction**, which is why the hardware fixes a 16 kHz lowpass on the output. Our engine already zeroes the index when the modulator crosses 0.45 × sample rate, which is a different and cruder guard | the same, plus `sound.h` read locally |
| **Envelope domain changes the instrument.** Linear-in-dB segments are part of what "DX" means | DX7 reverse-engineering |
| **Level normalisation per algorithm** is severe: the same operator levels through different routings give wildly different output levels | the standing engine rule, §8.8.2 step 2 |
| **A rational ratio can only make a harmonic spectrum** | derived, and measured both ways in §3 |
| **Changing the detent table re-voices every existing patch** | §3 |

## 7. The reference, rendered and characterised (§5 item 5)

Seven STK FM instruments are now wired into `ref-render` and build clean:

```
bash tools/ref-render/run.sh stk <TubeBell|Rhodey|Wurley|HevyMetl|BeeThree|PercFlut|FMVoices> [hz] [amp]
```

All seven derive from STK's `FM` class, whose constructor signature is
`FM( unsigned int operators = 4 )`, and each is a named TX81Z algorithm (`TubeBell.cpp`'s docblock
says "algorithm 5 of the TX81Z"). So this is a four-operator reference implementing the exact
algorithm family §1 identifies, on disk, MIT.

`TubeBell` is characterised in §3 and behaves exactly as its source says it should, which makes it
usable as-is. The other six are wired but not yet characterised; do that before trusting any of them,
per `ref-render`'s own state table where three of the older references are marked partly broken.

One structural note for the port: like `ModalBar`, these load `rawwaves/*.raw` files
(`sinewave.raw`, `fwavblnk.raw`) rather than computing their oscillators, so what ports is the
architecture and the ratio/gain/envelope tables, not the code.

## 8. Licence

STK is MIT (Cook and Scavone). On the patent question, STK's own `TubeBell` docblock is the clearest
statement we have: "The basic Chowning/Stanford FM patent expired in 1995, but there exist follow-on
patents, mostly assigned to Yamaha. If you are of the type who should worry about this (making money)
worry away." We do ship paid apps, so the paper round should note this rather than skip it. Building
an FM engine from the 1973 paper is not the risk; copying a specific named product's parameter tables
is a different question from implementing the technique.

## 9. What this round changed

1. **Four operators confirmed**, with the stronger reason than cost: eight algorithms is a standard
   shared across the OPN, OPM and OPZ families, not a choice we have to invent.
2. **PM versus FM is contentious in the literature, and we are already doing the right thing**, so
   §7.2's confident framing should be softened to match.
3. **A cheap fix to the shipped 2-op engine exists** (§3), with a measured failure waiting for it and
   a stated compatibility cost.
4. **Three implementation details that shape the sound** and that we currently do differently:
   feedback averaging, log-domain envelopes, and the output lowpass.

## Sources

- [Chowning (1973), "The Synthesis of Complex Audio Spectra by Means of Frequency Modulation"](https://web.eecs.umich.edu/~fessler/course/100/misc/chowning-73-tso.pdf)
- [Yamaha DX7 Technical Analysis (ajxs)](https://ajxs.me/blog/Yamaha_DX7_Technical_Analysis.html)
- [Yamaha DX7 chip reverse-engineering, part 4: how algorithms are implemented (righto.com)](http://www.righto.com/2021/12/yamaha-dx7-chip-reverse-engineering.html)
- [Dexed, the DX7 plugin built on the msfa core](https://github.com/asb2m10/dexed)
- [Yamaha TX81Z (8 algorithms, YM2414 OPZ)](https://en.wikipedia.org/wiki/Yamaha_TX81Z)
- [Yamaha YM2151 (OPM, the shared 4-op algorithm family)](https://en.wikipedia.org/wiki/Yamaha_YM2151)
- [Collecting info on Yamaha FM soundchips](https://gist.github.com/bryc/e85315f758ff3eced19d2d4fdeef01c5)
- STK source read locally at `build/ref-render/stk/` (`FM.h`, `TubeBell.cpp`)
- `runtime/sound.h` `sound_fm_sample` read locally; renders measured with `tools/wav-envelope.js`

## See also

- [`engine-reach.md`](engine-reach.md) - the root doc; §5 the rule this answers, §7.2 the engine
- [`engine-reach-modal-research.md`](engine-reach-modal-research.md) - the sibling round, for §7.1
- [`instrument-engines.md`](instrument-engines.md) - §8.8.3 the original 2-op FM design, §8.8.2 the playbook
- [`patch-matching.md`](patch-matching.md) - §7 the snapped-detent map, §8 the `sklocken` failure
