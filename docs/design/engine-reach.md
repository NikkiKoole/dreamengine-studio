# Engine reach: what the console cannot make yet

> **STATUS: READY TO BUILD (2026-09-14)** for tier 1 (search dimensions over engines we already
> ship, no DSP at all). Tiers 2 and 3 are specced here but each engine owes a research round
> (§5) before a line of code. **After the rules in §1 and §5 are applied, tier 3 is TWO new
> engines** (§7.5), possibly three; the rest of the candidate field turns out to be dimensions,
> a cart-land header, or one mechanism under several names. Root doc: it owns the *what and why*; the *how* to ship an engine
> is the playbook in [`instrument-engines.md`](instrument-engines.md) §8.8.2, and the candidate
> catalog that predates this doc is §8.9 there.

The patch matcher ([`patch-matching.md`](patch-matching.md)) gave us something no roadmap had before:
a **measured** answer to "what can this console actually make". Throw ten real samples at it and read
the losses, and the gaps stop being taste. This doc is the work that measurement implies.

The framing that matters: [`instrument-engines.md`](instrument-engines.md) is the *realism* program
(port an acoustic instrument, judge it against the real thing). This doc is the **reach** program:
cover more of the space of sounds, per CPU cycle, per engine. They share a playbook and disagree
about the scoreboard, which is fine and worth keeping separate.

## 1. Ruled out first: resynthesis

**A wavetable filled from the target's own analysed spectrum is not in scope, now or later.** That
is not matching a sample, it is resampling it, and it fails the north star
([ADR-0022](../decisions/0022-collaboration-is-the-north-star.md)) on both halves of the bar: there
is no honest core (the "engine" is a copy of the answer), and there is nothing for a stranger to
learn from or play with (no knob means anything).

The same objection applies, in weaker form, to **any general approximator engine**, which is why a
free-running wavetable-scan engine is not on the list below either. Two of the matcher's best
signals depend on the engines occupying *distinct* territory:

- the **ranking** is often the more useful output than the loss ("that is a MALLET, then EPIANO");
  the doc already notes PD never places first because SAW reaches its tones. An engine that can
  approximate everything wins every race, and trades a diagnosis for a number.
- the **flat-top-eight failure signature** (§8 of the matcher doc) works precisely because unrelated
  engines scoring alike is anomalous.

So the rule for everything below: **an engine earns its place by having a mechanism, not by having
enough parameters.** If you cannot say in one sentence what physical or electrical thing it is, it
is not a candidate.

## 2. What the measurement actually said

From [`patch-matching.md`](patch-matching.md) §8, ten samples from a commercial toy-keyboard library.
A good match is ~0.18, an unrelated real sample is ~1.0, noise is 1.65.

Three failures, and each names a different missing thing:

| sample | loss | what it says |
|---|---|---|
| `sklocken` | 0.418 (FM), and **1.58** at MALLET's best macros | the partials are not where any engine can put them. Not a budget problem: no search budget helps. |
| `birdtopper` | 0.534, fx stage did **nothing at all** | a 0.14s transient with a body behind it. One slot cannot be two layers. |
| `taplay` | 0.619, flat across PIPE/FM/VOICE | nothing here is distinctly right. |

And one near-miss worth reading the other way: `analok` matched at 0.182 on a bare `INSTR_SAW` with
the fx stage contributing nothing. The cheap end of the roster is carrying real weight, which is the
argument for widening it before adding anything exotic.

## 3. Tier 1: reach we already own and the search pins shut

This is the cheapest work in the repo and probably the largest single gain. `pmcart.c` (the file
that turns a candidate vector into engine calls) currently nails these to neutral on every render:

```c
instrument_duty(s, 0.5f);        // PWM pinned to square
instrument_unison(s, 1, 0.0f);   // unison OFF
instrument_bandlimit(s, 0);      // naive saw always
// instrument_sync() is never called at all
```

So the search **structurally cannot produce a supersaw, a PWM pad, a sync lead, or a clean
bandlimited saw**, even though `instrument_unison` / `instrument_unison_detune` / `note_duty` /
`instrument_sync` / `note_sync` / `LFO_DUTY` / `LFO_DETUNE` / `ENV_DETUNE` all ship today. That is a
large slice of real-world sampled material (pads, leads, most toy-keyboard "strings", most 80s
anything), and detune beating is exactly the kind of signature a log-mel loss picks up instantly.

Work items, no DSP:

1. **Add `V_DUTY`, `V_UNISON`, `V_DETUNE`, `V_SYNC`, `V_BANDLIMIT` to the voice vector.** Unison
   voice count and bandlimit are discrete, so they want the snapped-detent treatment the harmonics
   axis already has (§7 of the matcher doc, `detents.c`).
2. **Move `F_DRIVE` / `F_DRIVEMODE` from the fx stage into the voice stage.** `DRIVE_FOLD` is a
   wavefolder, which is an oscillator operation, not an effect. Freezing the voice before fitting it
   means fold and cutoff can never co-optimise, and by the matcher's own §5d rule (split dimensions
   by what a dial *does*, not by which API owns it) it belongs in stage 2.
3. **Finish `pm_engine_modes`.** It wires modes for 4 of 18 engines, and even for PIANO it skips
   `MODE_PIANO_STRETCH` and `MODE_PIANO_KNOCK`.
4. **Re-run the ten-sample table and the self-recovery oracle** before and after, so the gain is a
   number. Widening the vector costs search budget; if a dim does not pay, cut it.

Acceptance: the oracle still finds the right engine in the top three 24/24, and at least one of the
three named failures moves. A dim that changes nothing is a dim that never reached the DSP, which is
what `ab-render.js` exists to catch.

## 4. Tier 2: the patch shape

**Every candidate today is one slot, one engine.** Real sampled sounds are routinely two layers: a
transient and a body, a chiff and a tone, a click and a ring. `birdtopper` is that shape exactly, and
no single engine will ever reach it.

A two-layer patch (two slots, a balance dim, an optional per-layer detune) is likely worth more than
any single new engine, and it costs no DSP. It is not free, though, and the costs are the reason it
is tier 2 rather than tier 1:

- the search space roughly doubles, and the render cost with it (the render is the bottleneck, §5b).
- **it can launder a wrong voice**, the same way the fx stage can. The mitigation is the one already
  proven: stage it, freeze layer 1, and report what layer 2 bought, so a person can read which half
  did the work.
- the output stops being one pasteable `instrument()` block, which was a deliberate design property.

Open question for the maker: is a two-layer patch still "a patch", or is it a small arrangement? The
answer decides whether this lands in the matcher or stays a thing a person does by hand afterwards.

## 5. The research rule (this applies to every engine below)

**No engine gets written from memory or from vibes.** The engines here should be top notch, cheap and
great, and the only way that happens is a research round *before* the paper design. This is now a
required step 0 alongside the playbook's "render the reference first".

Every candidate engine owes a short research note (one screen, in this doc or its own file, cited)
answering:

1. **What is the industry-standard algorithm, and who published it?** Name the paper, the book
   chapter, or the reference implementation. Most of this space was settled between 1973 and 2010 and
   the answers are public. If we cannot find a citation, that is itself a finding worth writing down.
2. **What do the good implementations do that the naive one does not?** This is where the quality
   lives. Almost always: antialiasing, normalisation, and what happens at the extremes of a knob.
3. **What is the per-voice cost?** State and cycles. Buffer-free or does it need the shared per-voice
   delay line (§8.2 of the engine program)? An engine that cannot run polyphonically on a phone is
   not a candidate, it is a research project.
4. **What are the known gotchas?** Each of these classes has already bitten this repo at least once,
   so ask each one explicitly:
   - **aliasing** (naive saw/sync/fold are the classic offenders; PolyBLEP / BLEP / DPW are the
     standard cures, and `instrument_bandlimit` already ships one of them)
   - **level across the macro range** (every macro position must strike at the same loudness or the
     knobs read as volume controls, §8.8.2 step 2)
   - **DC offset** (`dc-check.js`), **clicks on parameter change** (`click-check.js`), **tuning**
     (`tune-check.js`, and note PIPE shipped with tuning that tracks a macro non-monotonically)
   - **determinism**: anything whose output is compared uses `de_*` from
     [`demath.h`](../../runtime/demath.h), never libm ([`determinism.md`](determinism.md))
   - **denormals and runaway feedback** (`soak-check.js`)
5. **Is there a second implementation to be wrong against?** [`tools/ref-render/`](../../tools/ref-render/)
   exists because every other gate checks an output property and none asks whether the *mechanism* is
   sane, which is how `INSTR_BOWED` ran for months with a friction coefficient above 1.0 behind a
   green board. Where a reference exists, render it.
6. **Licence.** Measure against anything. Borrow only with a compatible licence and attribution (STK
   and the `luthier` code are MIT and we already borrow with credit; `flute-lv2` is GPL-2.0 and we do
   not copy from it).

Canonical starting points, so nobody starts from a blank search box: Julius O. Smith III's CCRMA
volumes (waveguides, physical modelling), the Synthesis ToolKit (Cook and Scavone), Perry Cook's
*Real Sound Synthesis*, Chowning's 1973 FM paper, Välimäki and colleagues on alias-suppressed
oscillators, Brandt on hard sync without aliasing, Zavalishin's *The Art of VA Filter Design* for
filter topology, Bilbao's *Numerical Sound Synthesis* for the modal and finite-difference end, and
Szabo's supersaw study for the detune end. The repo's own audit,
[`synth-secrets-audit.md`](synth-secrets-audit.md), is the other half of this: it is the
*synthesizer* literature rather than the *acoustics* literature, and it has already produced findings
about shipped engines.

## 6. The engine rule: one engine, one cart

**Every new engine ships with its own showcase cart, in the same change.** This is playbook step 5 and
it is not a documentation chore, it is the acceptance test, for three reasons:

- **it is how the mapping gets judged.** Presets are nothing but baked macro positions, so if pressing
  the preset named after a piece of hardware does not sound like that hardware, the mapping is wrong,
  not the preset.
- **it is the tuning rig.** Keys play it, the three macros are draggable and audition while dragging,
  autoplay keeps it sounding, `watch()` the knobs under `DE_TRACE`. Everything before this step exists
  to make this loop fast.
- **it is the ADR-0022 bar.** An engine that passes the gates but that a stranger cannot pick up and
  enjoy has cleared half the bar. The cart is where the other half is checked.

The existing single-engine showcases (`pluck`, `mallet`, `organ`, `epiano`, `fm`, `pd`, `tabla`,
`handpan`, `bowed`) are the template and run ~250 to 390 lines. See
[`../guides/instrument-carts.md`](../guides/instrument-carts.md) for the form factors.

An engine is also not finished until it is in the places that keep it honest: `soundcheck.c` (same
commit), the four API registration sites, `tune-check` if it is pitched, the matcher's `PM_ENGINE`
table and `pm_engine_modes`, and its recipe in [`instrument-recipes.md`](../guides/instrument-recipes.md).

## 7. Tier 3: the engine candidates, after the rules are applied

The rules in §1 and §5 are not decoration: applied honestly they shrink this list more than they
grow it. **Two engines survive, possibly three.** Everything else that reads like an engine turns out
to be a dimension, a header, or the same mechanism wearing a different name.

The collapse that does the most work: **two engines with one mechanism are one engine.** A "tuned
noise" engine is a continuous-noise exciter into one or two resonant modes, which is §7.1 at
different settings, not a second mechanism. The **additive** row that has sat in the §8.9 catalog
since June (bell/choir/strings; partial count, spectral tilt, per-partial decay and inharmonicity) is
a modal bank with a sustaining exciter, which is also §7.1. One build covers all three.

Ranked by (coverage gained) x (cheapness) x (searchability). That last factor is specific to this
program: a smooth continuous axis is worth more to a search than a discrete family, and §7.1 is
valuable precisely because it turns a discrete choice into a continuous one.

Note the cost of the list itself: stage 1 of the matcher scales linearly in engine count and the
render dominates, so even two more engines is a measurable wall-clock tax on every run. Two is a
feature.

### 7.1 Exciter into resonator (a general modal bank)

**The first pick, and the one that absorbs the others.** MALLET (4 modes), MEMBRANE (6) and EPIANO
(12) are three *fixed families* with snapped material macros. Nothing in the roster gives continuous
partial placement (inharmonicity), a continuous decay tilt across frequency, and a free choice of
exciter. `sklocken` scoring 1.58 at MALLET's best macros is that gap stated as a number: the partials
are not at ratios this engine can produce, and no search budget fixes it.

What it buys, in one engine:

- everything struck, plucked, scraped or rung that the three fixed families cannot voice, on a
  **smooth searchable surface** instead of four discrete islands
- **the noise half**: a continuous-noise exciter into a few modes is breath, chiff, wind, cymbals,
  brushes and snare bodies. `INSTR_NOISE` is excluded from the matcher race because the targets are
  pitched, and there is currently no tuned-noise voice at all. Paired with tier 2, this is also the
  transient half of every two-layer patch.
- **the additive half**: a sustaining exciter into many modes is the §8.9 additive row (choir, bell,
  string pads), which ORGAN's 9 fixed drawbar sines only partly reaches.

So the exciter menu is not a nice-to-have, it is what makes this one engine instead of three. The
research round must settle it up front: impulse/strike, plucked, noise burst, continuous noise, and
whether a sustaining bowed or blown excitation belongs here or stays with the existing waveguides.

Research targets: modal synthesis proper (Adrien, Bilbao), the exciter/resonator split as a design
(Mutable's Rings and Elements are the well-known modern realisation and the source is public, so
check the licence before borrowing rather than measuring), and the numerical question of how many
modes you can afford per voice. Gotchas to ask about up front: mode count versus polyphony cost,
per-mode level normalisation as inharmonicity moves, the click risk when the exciter changes on a
held note, and where the noise generator's determinism comes from.

### 7.2 Multi-operator FM

**The second pick, and the only other distinct mechanism on the list.** 2-op reaches a thin slice of
FM's spectra. FM already wins or places on the metallic and bell targets (`toytone`, `wisp` at 0.350,
`sklocken` at 0.418) mostly as the *least wrong* option, which is the signature of an engine pointed
at the right family that cannot get there.

What it buys: 4 operators with a handful of fixed algorithms puts the DX vocabulary in reach: bells,
metallic percussion, the whole 80s electric-piano and bass corpus. The algorithm choice is a snapped
axis, which the matcher already handles well.

Research targets: Chowning first, then the actual DX7 algorithm set and what the published
reimplementations (Dexed and the Music Synthesizer for Android lineage) learned about envelope and
operator scaling. Gotchas: the feedback operator is where aliasing and instability live, the
level-per-algorithm normalisation problem is severe, and the envelope *rates* are as much of the
sound as the operator topology.

### 7.3 Conditional: note-tracking ring modulation

The §8.9 catalog row says the fixed-Hz `ringmod()` bus effect mostly covers AM and ring mod, and what
an *engine* would add is a modulator that **tracks the played pitch**, so the clang stays harmonic
per note.

**Check the cheap thing first.** `instrument_ringmod(slot, freq_hz, mix)` takes an absolute
frequency. If it can take a ratio instead, the tracking is a parameter change on a shipped effect and
this is not an engine at all. Only if the modulator must sit inside the voice (and the research round
should say why) does it earn a row. Resolve this before planning around it, because it is the
difference between two engines and three.

### 7.4 Things that read like an engine and are not

Recorded here so they are not re-proposed:

| Candidate | What it actually is |
|---|---|
| **Wavefolder** | Tier 1, item 2. `DRIVE_FOLD` ships; it needs moving from the fx stage to the voice stage so it can co-optimise with cutoff. No new engine. |
| **Subtractive imitation** (the Minimoog playing a trumpet, rather than the trumpet) | A cart-land `subtractive.h` header holding published parameter values as data, per the §8.9 argument and the `acid303.h` precedent. Escalate to an engine only if the pieces prove they must sit closer to the voice. |
| **Tuned noise** | §7.1 with a continuous-noise exciter. |
| **Additive** | §7.1 with a sustaining exciter. |
| **Wavetable scan, and any general approximator** | Ruled out in §1, permanently. |

### 7.5 The count

**2 engines, 2 showcase carts, 2 research rounds.** Three if §7.3 cannot be answered with a
parameter. That is the whole tier, and it sits on top of tier 1, which needs no DSP at all.

## 8. Order of work

1. Tier 1, all four items, measured before and after. No DSP, largest expected gain per hour.
2. Answer §7.3 with a parameter if it can be answered with a parameter. One command's worth of
   reading, and it decides whether this tier is two engines or three.
3. Decide the tier 2 question (is a two-layer patch still a patch?).
4. Research round for §7.1, then its paper design, then the engine, then its cart. It is first
   because it absorbs two other candidates; getting its exciter menu wrong is what would split it
   back into three engines.
5. Research round for §7.2, then its paper design, then the engine, then its cart.
6. Re-measure. The ten-sample table is the scoreboard for this whole programme, so it gets re-run
   after every tier and the numbers land back in [`patch-matching.md`](patch-matching.md) §8.

## See also

- [`patch-matching.md`](patch-matching.md) - the matcher, the loss scale, the ten-sample measurement
  this doc is built on
- [`patch-matching-cart.md`](patch-matching-cart.md) - the ear-as-loss-function fork
- [`instrument-engines.md`](instrument-engines.md) - the engine program: §8.8.2 playbook, §8.9
  candidate catalog, §8.2 buffer-free versus buffered
- [`synth-secrets-audit.md`](synth-secrets-audit.md) - the synthesizer-literature audit and its
  findings against shipped engines
- [`../guides/porting-from-navkit.md`](../guides/porting-from-navkit.md) - how to port an oscillator
  without compounding errors
- [`../guides/instrument-carts.md`](../guides/instrument-carts.md) - showcase cart form factors
- [`../guides/checks-and-oracles.md`](../guides/checks-and-oracles.md) - which gate to run for which
  change
