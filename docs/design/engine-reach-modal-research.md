# Research round: exciter into resonator (the modal bank)

> **STATUS: RESEARCH COMPLETE (2026-09-14); ENGINE + CART SHIPPED (2026-09-14).** Reference
> RENDERED AND CHARACTERISED (§7). The §5 round owed by
> [`engine-reach.md`](engine-reach.md) §7.1, answering its six questions with citations and a
> reference implementation rather than from memory. **It confirms the §7.1 collapse and pins one
> implementation choice that the collapse silently depends on** (§3 below). Paper-round mapping
> is a live A/B in the `modal` cart (not frozen). Pinned-Hz modes leave the geometry axis
> (macro-mapping §2c option 1) — v1 does not implement `MODE_PIN`.

## 1. The standard algorithm, and who published it

Modal synthesis represents a vibrating structure as a set of resonant modes, each with a frequency,
a damping coefficient and a gain, and reconstructs any vibration as a superposition of them. The
canonical reference is **Adrien, J.-M. (1991), "The missing link: modal synthesis"**, in De Poli,
Piccialli and Roads (eds), *Representations of Musical Signals*, MIT Press, pp. 269 to 298.

The **exciter into resonator** split is not a separate technique, it is the standard decomposition of
a physical model: an exciter supplies energy, a resonator vibrates, and the coupling between them is
the interaction. Modal synthesis is the resonator half.

Perry Cook's own course notes on the method (he wrote STK's `Modal` class, so this is the reference
implementation's author describing it) make three claims we should build against:

- **"Essentially a subtractive model in that there is some excitation and some filters to shape it."**
- **"Practical and efficient, if few modes."** Cheapness is conditional on mode count, stated by the
  source rather than assumed by us.
- Mode ratios are a property of the geometry, and he gives them: a **free-free bar** is 1.0, 2.765,
  5.404, 8.933 (harmonic in principle, stretched by stiffness); a **round plate or drum** is
  inharmonic at Bessel ratios; a **string** is near-harmonic with slight stiffness stretch; an
  **open plus closed tube** is odd multiples of the fundamental.

That last point is the mechanism behind §7.1's "continuous inharmonicity" claim: geometry is a real
axis with published endpoints, not a made-up knob.

Further reading the notes cite, worth having if the paper round needs depth: Rossing (1976) on
percussion acoustics, Doutaut and Chaigne (1993) on xylophone bars, Larouche and Meillier (1994) on
multichannel excitation/filter modelling, and Cook (1997), "Physically Informed Sonic Modeling
(PhISM): Synthesis of Percussive Sounds", *Computer Music Journal* 21:3.

## 2. A finding that collides with our own §1 rule, on purpose

Cook's notes describe **"residual excitation", also called "parametric sampling"**: analyse a
recorded sound (LPC, ARMA, peak-picked sines), extract modal filter parameters plus the residue, then
drive the filters with that residue and get the original back, modifying parameters on the way.

**That is the technique [`engine-reach.md`](engine-reach.md) §1 rules out**, arrived at from the
other direction and sitting in the canonical literature as normal practice. It is recorded here so
nobody re-imports it later believing it is standard and therefore fine. It is standard. We still do
not want it, for the reason §1 gives: the engine would be a copy of the answer, and no knob would
mean anything to a stranger.

The useful half survives. Analysis of a *real instrument* to get its mode table is fine and is how
STK's presets were made (§5 below). Analysis of **the user's target sample** at match time is not.
The line is whether the answer is baked in at authoring time by us, or extracted at run time from the
thing we are supposed to be matching.

## 3. The finding that pins the implementation

**The engine must be a bank of excited filters, not a bank of decaying sine oscillators.** This is
the one thing that could have quietly broken the §7.1 collapse, and it is invisible until you ask
how a continuous exciter works.

Our existing struck engines take the **decaying sine** route: `INSTR_MALLET` is four decaying sine
modes, `INSTR_MEMBRANE` six at circular-membrane ratios, `INSTR_EPIANO` twelve. A decaying sine is an
oscillator that is *started* with an amplitude and then dies. You cannot inject noise into it. It has
no input.

A filter bank has an input, which is what makes the exciter menu possible at all, and therefore what
makes §7.1 absorb both tuned noise and the additive row rather than being a fourth fixed family
alongside the three we already have. STK's `Modal::tick()` is exactly this shape, in six lines:

```
temp  = masterGain * onepole( wave->tick() * envelope.tick() )   // the exciter
temp2 = sum over modes of filters[i]->tick(temp)                 // the resonator
temp2 = temp2*(1 - directGain) + directGain*temp                 // how much raw strike you hear
```

Three things to take from it beyond the shape: the exciter is a **wavetable through an envelope and a
one-pole**, not an impulse, so the strike has a spectrum and a length; `directGain` blends unresonated
exciter back in, which is where the attack bite lives; and the modes are plain two-pole resonators
(`BiQuad::setResonance(frequency, radius)`), so per-mode cost is a biquad.

## 4. What good implementations do that the naive one does not

**Mutable Instruments' Elements** is the modern realisation and is worth copying the *shape* of. Its
answers, from the manual:

- **Three exciters that MIX, not a switch.** BOW ("a bow scratching a material... combines a raw,
  scratching, granular noise with a purer sound"), BLOW ("continuous, noise-like sounds reminiscent
  of blowing, breathing, wind"), STRIKE ("impulsive bursts and percussive noises"). Each has its own
  level. I had assumed a menu in §7.1; a mix is better and is what shipped.
- **Geometry is one continuous parameter.** `GEOMETRY` spans "plates, to strings, to bars/tubes, to
  bells/bowls". That is §7.1's continuous-inharmonicity axis, already proven playable on hardware.
- **The other three controls are** `BRIGHTNESS` (high-frequency mode attenuation, muted to reflective
  material), `DAMPING` (how fast energy dissipates) and `POSITION` (where on the surface it is
  excited).
- **STRIKE bleeds onto the output past 2 o'clock**, which is Elements' version of STK's `directGain`.
  Two independent implementations both decided the raw exciter must be audible.

**A tension this creates for us, to be resolved in the paper round, not here.** Elements exposes four
resonator controls plus three exciter levels. We get three macros
([`instrument-engines.md`](instrument-engines.md) §8.1.1, no per-engine named params, ever). So the
paper round must choose, and say why. The obvious candidate is harmonics = geometry, timbre =
brightness, morph = damping, with position and the exciter mix baked per voicing and reachable
through `MODE_*`. That is a proposal, not a finding.

## 5. Cost, and the mode-count question

Cook's "practical and efficient, **if few modes**" is the whole answer to §5 item 3, and it makes mode
count the single design variable that decides whether this engine ships.

Two data points:

- **STK's `Modal` defaults to 4 modes** and `ModalBar` uses exactly 4 for every one of its nine
  instruments. Four biquads per voice is cheap and is in the same class as what `INSTR_MALLET`
  already costs.
- **Rings and Elements trade modes for voices.** The Rings manual says outright that "the module
  might reduce the number of harmonics in the generated signals to cope with the higher polyphony" in
  its 2 and 4 voice modes. Community documentation reports the split as 60 filters going to 2x30 or
  4x15; the manual itself gives no number, so treat the principle as confirmed and the figure as
  unverified.

So mode count is a **budget to spend per voice, not a constant**, and the paper round should make it
a property of the voicing rather than a fixed number. Our polyphony target is a phone, which is the
constraint Rings is also solving, and it reached a shipping answer at a far higher mode count than
STK's four. Somewhere between 4 and 15 modes per voice is the real design space.

## 6. Gotchas, each from a source rather than a guess

| Gotcha | Where it comes from |
|---|---|
| **Modes above Nyquist.** A high mode on a high note aliases. STK guards it explicitly: `setRatioAndRadius` halves the ratio until `ratio * baseFrequency < nyquist`, and logs "aliasing would occur here ... correcting". | `Modal.cpp`, read locally |
| **Some modes must NOT track pitch.** STK encodes a negative ratio as an absolute frequency in Hz, so a mode can be pinned to a fixed resonance while the rest transpose. `ModalBar`'s Marimba preset uses -2443, Agogo -3725. A design that assumes every mode is a ratio cannot express these presets. | `Modal.cpp` and `ModalBar.cpp`, read locally |
| **Per-mode decay is per-mode, and the numbers are delicate.** `ModalBar` stores a pole radius per mode, and the difference between a marimba and a resonant bar is 0.9996 versus 0.99996. A single shared decay knob cannot voice this family. | `ModalBar.cpp`, read locally |
| **Level normalisation as geometry moves.** Not stated by a source, but it is the standing engine rule (§8.8.2 step 2: every macro position must strike at the same loudness) and it is sharper here, because moving inharmonicity redistributes energy between modes. | our own playbook |
| **Continuous interaction is the hard part.** Cook lists it as a weakness of the method: "hard to interact directly and continuously (rubbing, damping, etc.)". Elements solved bowing with a dedicated generator rather than by feeding the resonator raw noise. Do not assume BOW falls out of the filter bank for free. | Cook's notes, Elements manual |
| **Determinism.** The noise exciters need a seeded generator or a table, per [`determinism.md`](determinism.md). | our own rule |
| **Clicks when the exciter changes on a held note.** `click-check.js` exists for this class. | our own gates |

## 7. A second implementation to be wrong against (§5 item 5), already on disk

Both candidates are present and MIT licensed:

| Reference | Where | What it gives us |
|---|---|---|
| STK `Modal.h` / `ModalBar.h` | `build/ref-render/stk/` (already fetched) | the canonical 4-mode implementation plus **nine measured preset mode tables** (marimba, vibraphone, agogo, wood, reso, beats, two-fixed, clump, tibetan bowl) |
| STK `BandedWG`, `Resonate`, `Shakers` | same | the neighbouring approaches, including Cook's PhISM |
| Mutable `elements/` and `rings/` | `github.com/pichenettes/eurorack`, MIT | the modern exciter-mix and continuous-geometry design |

`ModalBar` is now wired in and characterised (2026-09-14):
`bash tools/ref-render/run.sh stk ModalBar <hz> <amp> <preset 0-8>`, with its verdict recorded in
that script's per-reference state table. **Verdict: usable as-is**, the first reference here to
clear the bar without a caveat since Clarinet.

**The measurement, which is the part worth having.** Preset 0 (marimba) at 220 Hz puts its modes at
220 / 878 / 2343 Hz. The preset table says 1.0 / 3.99 / 10.65. That is exact.

Then the same preset at 110 Hz:

| | mode 1 | mode 2 | mode 3 | mode 4 |
|---|---|---|---|---|
| rendered at 220 Hz | 220 | 878 | 2343 | **2443** |
| rendered at 110 Hz | 110 | 439 | 1172 | **2443** |

Everything halves except one mode, which sits at 2443 Hz in both. **That is §6's negative-ratio
gotcha, confirmed by ear-independent measurement rather than by reading the source**: the marimba
preset pins its fourth mode at an absolute frequency that does not transpose, at -36 dB relative to
the fundamental at 220 Hz and -48 dB at 110 Hz, so it is audible, not a rounding artefact. An engine
whose mode table holds only ratios cannot voice this preset. Design consequence: **a mode needs a
flag or a sign convention for "absolute Hz", and the macro that moves geometry must leave those
modes alone.**

Two further things only a render shows:

- **It is shorter than our harness.** Pole radius 0.9996 works out to a T60 of about 0.39s, so the
  bar is silent by ~0.9s and `ref-render`'s 6.0s note-off damps nothing. Analyse near the strike.
  The header's standing "give `peaks.js` a 3s window" rule is for *sustained* references and cannot
  apply to a struck one, which is a real conflict in that advice and now noted in the state table.
- **STK's exciter is a recorded strike**, `rawwaves/marmstk1.raw` through an envelope and a one-pole,
  not a synthetic burst. So the attack is not portable: we get the architecture from STK and must
  source the excitation ourselves. Elements' STRIKE generator is the model for that half.

## 8. Licence

STK is MIT (Cook and Scavone, 1995 to 2023); we already borrow from it with attribution, as
`BOW_BODY_HZ` does. Mutable's STM32F projects, including Elements and Rings, are MIT. Both are
therefore borrowable with credit rather than measure-only, unlike the GPL-2.0 `flute-lv2` noted in
`ref-render`'s header.

The mode tables in `ModalBar` are measured data about real instruments and carry STK's licence; if we
ship a voicing table derived from them, it cites them, the way `rhythmbox.h` cites its sources per
rhythm.

## 9. What this round changed

1. **The §7.1 collapse holds, but only for a filter bank.** Pinned in §3. A decaying-sine design would
   split §7.1 back into three engines, which is the outcome the whole tier was built to avoid.
2. **The exciter is a mix, not a menu.** §7.1's "exciter menu" wording should be corrected.
3. **Mode count is a per-voicing budget**, not a constant, and 4 is the floor rather than the target.
4. **Residual excitation is in the literature and is still out**, for the reason §1 already gives.
5. **A mode table of pure ratios is not enough**, measured in §7: some modes are pinned to absolute
   Hz, and the geometry macro must not drag them.
6. **The exciter is ours to build.** STK's is a recorded sample, so only the architecture ports.

## 10. Linux proof (2026-09-14, DE_NO_RAYLIB)

No Raylib / no xxd on the agent VM. `bash tools/clips/modal/render-nr.sh` builds
`tools/clips/modal/render-nr.c` against `studio.c` + the `modal` cart and drives the
five committed scripts. Measured after the z1 feed was scaled by `(1-r)` (a constant
per-sample dump clipped a held blow; a decaying-sine dump on attack is still how a
strike speaks past STK's tiny `b0`):

| clip | peak | rms | clip% | notes |
|---|---|---|---|---|
| marimba-strike | 0.42 | −26 dB | 0 | decays; mallet control on the same host was 0.12 |
| breath-blow | 0.30 | −26 dB | 0 | held A is continuous (rms/sec flat) |
| bowed | 0.23 | −20 dB | 0 | held A, scratch + tone |
| map-a (recommended) | 0.08 | −40 dB | 0 | bowl strike |
| map-b (Elements) | 0.12 | −36 dB | 0 | same bowl, opposite mapping — bytes differ |

Mac: `node tools/play.js modal script tools/clips/modal/<clip>.script --headless --frames 180 --wav out.wav`.

## Sources

- [Adrien, "The missing link: modal synthesis" (1991)](https://dl.acm.org/doi/10.5555/131150.131158)
- [Perry R. Cook, "Modal Synthesis" course notes, CCRMA](https://ccrma.stanford.edu/workshops/dsp2008/prc/modal.pdf)
- [Elements manual, exciter and resonator sections](https://pichenettes.github.io/mutable-instruments-documentation/modules/elements/manual/)
- [Rings manual, resonator models and polyphony](https://pichenettes.github.io/mutable-instruments-documentation/modules/rings/manual/)
- [Elements open-source page (MIT)](https://pichenettes.github.io/mutable-instruments-documentation/modules/elements/open_source/)
- [Mutable Instruments source](https://github.com/pichenettes/eurorack)
- [Excite and Resonate: A History of Physical Modelling Synthesis](https://www.attackmagazine.com/features/long-read/excite-and-resonate-a-history-of-physical-modelling-synthesis/)
- [IRCAM Modalys introduction](https://support.ircam.fr/docs/Modalys/3.6/Introduction.html)
- STK source read locally at `build/ref-render/stk/` (`Modal.h`, `Modal.cpp`, `ModalBar.cpp`, `FM.h`)

## See also

- [`engine-reach.md`](engine-reach.md) - the root doc; §5 is the rule this note answers, §7.1 the engine
- [`instrument-engines.md`](instrument-engines.md) - §8.8.2 the playbook, §8.1.1 the three-macro rule
- [`../guides/porting-from-navkit.md`](../guides/porting-from-navkit.md) - how to port an oscillator
