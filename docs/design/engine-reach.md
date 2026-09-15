# Engine reach: what the console cannot make yet

> **STATUS: SHIPPED (2026-09-14)** for tier 1 — matcher wiring, no new DSP. Partial on the
> commercial ten-sample table (those WAVs are not in the repo, so the named §8 failures were
> not re-scored). **§7.1 (`INSTR_MODAL`) is also SHIPPED** — filter bank + exciter mix +
> `modal` cart; both three-macro routes stay a live A/B. **§7.3 (note-tracking ringmod) is
> SHIPPED as a parameter**, not an engine (`ringmod_ratio` / `instrument_ringmod_ratio`).
> **§7.2 (`INSTR_FM4`) SHIPPED 2026-09-15** — four operators, eight OPN/OPM/OPZ algorithms,
> `fm4op` cart; both three-macro routes stay a live A/B. B′ (irrational detent on shipped
> `INSTR_FM`) is deferred: the 4-op voicings carry √2 instead. Tiers 2 and the remaining
> open questions sit below. Root doc: it owns the *what and why*; the *how* to ship an
> engine is the playbook in [`instrument-engines.md`](instrument-engines.md) §8.8.2, and the
> candidate catalog that predates this doc is §8.9 there.

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

## 3. Tier 1: reach we already own — SHIPPED (2026-09-14)

The matcher used to nail analog extras shut on every render (`instrument_duty(s, 0.5f)`,
`instrument_unison(s, 1, 0.0f)`, `instrument_bandlimit(s, 0)`, and `instrument_sync` never called).
That structurally could not produce a supersaw, a PWM pad, a sync lead, or a clean bandlimited saw,
even though those APIs already ship.

Now shipped, no DSP:

1. **Voice vector dims `V_DUTY`, `V_UNISON`, `V_DETUNE`, `V_SYNC`, `V_BANDLIMIT`.** Unison count
   (7) and bandlimit (2) are snapped detents, measured by `detents.c --check`. `pmcart.c` /
   `pmbreed.h` write the real `instrument_*` calls every render so leftovers cannot leak.
   Searched on wavetable engines only (duty on SQUARE, bandlimit on SAW). **Refine-only**, not
   the engine race: they change colour, not which oscillator is right, and at 12 pop they drown
   the ranking.
2. **`V_DRIVE` / `V_DRIVEMODE` live in the voice stage.** `F_DRIVE` / `F_DRIVEMODE` stay in the
   fx enum so old `pm:vec` indices do not shift; they are no longer searched or applied. Fold
   co-optimises with cutoff in refine. The editor lifts an old 19-float vec's `f[0]` into the
   voice slot.
3. **`pm_engine_modes` finished.** `studio.h` only declares `MODE_*` for four engines
   (PIANO / GUITAR / ORGAN / BOWED) — "4 of 18" was the whole roster. PIANO now owns all six
   (weight / click / decay / **knock** / **stretch** / stiff); it used to skip knock and stretch.
4. **Measured** (`--quick --stage1`, 6 engines × seeds 1/2/3/7 — same protocol before and after).
   Race ranking is **unchanged**: **23/24** top-3, **19/24** first. SAW seed 1 loss is
   bit-identical (0.12856). The one miss is still FM seed 1 ranked 4th (PIPE / SQUARE / SAW
   then FM). Published 24/24 used the fuller race budget (16 pop / 22 gens), not `--quick`.
   Commercial ten-sample WAVs (`sklocken`, `birdtopper`, `taplay`) are not in the repo, so
   those named §8 losses were **not re-scored** — honest partial. Analog-dim reach is gated
   by `detents.c --reach` (11/11: live axes differ, dead axes match). `--selftest SQUARE
   --analog` at full refine recovered unison (detent 7/7) and detune (0.550 → 0.554); duty /
   sync / fold moved but missed the truth on that seed.

Cut list: none of the new dims were silent on the engine that owns them. Analog extras were cut
from the **race** (not from the vector) for the reason in item 1 — and that cut is what kept
the race bit-identical to the baseline. `LFO_DUTY` / `LFO_DETUNE` / `ENV_DETUNE` stay
unsearched — they are periodic modulation and belong with the fx-stage wobble set, not this
ticket.

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
grow it. **Two engines survive.** Everything else that reads like an engine turns out to be a
dimension, a header, or the same mechanism wearing a different name. §7.3's conditional third
(note-tracking ring mod) was answered with a parameter.

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

So the exciter is not a nice-to-have, it is what makes this one engine instead of three.

> **§5 RESEARCH ROUND: DONE (2026-09-14).**
> [`engine-reach-modal-research.md`](engine-reach-modal-research.md). It confirms the collapse above
> and changed four things here:
>
> 1. **The engine must be a bank of excited FILTERS, not decaying sine oscillators**, and this is
>    load-bearing. Our existing struck engines (MALLET 4, MEMBRANE 6, EPIANO 12) are decaying sines,
>    which have no input, so noise cannot be injected into them. A filter bank has an input, and that
>    is the only reason this engine absorbs tuned noise and additive instead of becoming a fourth
>    fixed family beside the three we already have. A decaying-sine design splits this row back into
>    three engines.
> 2. **The exciter is a MIX, not a menu.** Elements ships three generators with independent levels
>    (bow, blow, strike) rather than a selector, and both it and STK blend un-resonated exciter back
>    into the output, which is where the attack bite lives.
> 3. **Mode count is a per-voicing BUDGET, not a constant.** Cook's "practical and efficient, if few
>    modes" makes it the variable that decides whether this ships; STK uses 4, Rings trades modes for
>    voices on hardware solving the same phone-shaped constraint. 4 is the floor, not the target.
> 4. **Geometry is a real axis with published endpoints** (free-free bar 1.0 / 2.765 / 5.404 / 8.933,
>    Bessel ratios for a round plate, near-harmonic for a string, odd multiples for a stopped tube),
>    so "continuous inharmonicity" is a mechanism rather than a knob we invented.
>
> Cost, gotchas and licence are answered there. Both reference implementations are MIT and one is
> already on disk. The open question it hands to the paper round is the **three-macro mapping**:
> Elements needs four resonator controls plus three exciter levels, and we get three macros.
>
> **SHIPPED (2026-09-14) as `INSTR_MODAL` (31).** Filter bank, not decaying sines. Exciter is a
> mix (bow / blow / strike) plus un-resonated bleed (`MODE_MODAL_DIRECT`). Mode count is a
> budget (`MODE_MODAL_MODES`, 4..12). Geometry is a smooth lerp across four published endpoints.
> Three live macros; both paper-round routes (`engine-reach-macro-mapping.md` §2) are a runtime
> `MODE_MODAL_MAP`, not a `#define` — the `modal` cart toggles them on a tappable A/B. Pinned-Hz
> modes leave the geometry axis (option 1). Matcher races it. Ear still open on which mapping
> won. Next in serial order is §7.2.

### 7.2 Four-operator FM

**The second pick, and the only other distinct mechanism on the list.** 2-op reaches a thin slice of
FM's spectra. FM already wins or places on the metallic and bell targets (`toytone`, `wisp` at 0.350,
`sklocken` at 0.418) mostly as the *least wrong* option, which is the signature of an engine pointed
at the right family that cannot get there.

What it buys: four operators with a handful of fixed algorithms puts the DX vocabulary in reach:
bells, metallic percussion, the whole 80s electric-piano and bass corpus. The algorithm choice is a
snapped axis, which the matcher already handles well.

**Why four and not six, stated as a hypothesis the research round must confirm or overturn.**
Operator count is normally a research *output*, so naming it up front needs a reason:

- **Four is where the price/reach knee sits.** Six operators is the DX7. Four is the TX81Z, DX21,
  DX100 and the OPL/OPM chips, which is most of the FM vocabulary a listener actually recognises, at
  two thirds of the per-voice cost.
- **We already have a four-operator reference on disk.** STK's `FM` class takes
  `unsigned int operators = 4`, and all seven of its FM instruments (`TubeBell`, `Rhodey`, `Wurley`,
  `HevyMetl`, `BeeThree`, `PercFlut`, `FMVoices`) are built on it, implementing *named TX81Z
  algorithms*: `TubeBell.cpp`'s own docblock says "algorithm 5 of the TX81Z". MIT, and already
  fetched under `build/ref-render/stk/`. A six-operator design would satisfy §5 item 5 with nothing
  local.

Two questions the round must settle that matter more than the count:

- **It is phase modulation, not frequency modulation.** Every digital "FM" chip does PM. That is what
  keeps the feedback operator stable and the spectra clean, and getting it wrong is how you ship an
  engine that is subtly not-DX for reasons nobody can name.
- **A curated algorithm set, or a free routing matrix?** A matrix is more general, wrecks
  searchability, and cuts against the §1 mechanism rule. Expect a curated set; say why in the note.

> **SHIPPED (2026-09-15) as `INSTR_FM4` (32).** Four operators, eight OPN/OPM/OPZ algorithms,
> phase-mod foundation inherited from 2-op. Three live macros; both paper-round mappings stay
> a live `MODE_FM4_MAP` (recommended = voicing, alt = algorithm). Showcase: `fm4op`.
> B′ deferred — see the STATUS entry.
>
> **§5 RESEARCH ROUND: DONE (2026-09-14).**
> [`engine-reach-fm-research.md`](engine-reach-fm-research.md). Four operators confirmed, both
> questions above answered, and one finding that does not wait on this engine at all:
>
> 1. **Four is a standard, not a cost trade.** Eight algorithms is shared across Yamaha's OPN, OPM
>    and OPZ families (TX81Z, DX21, DX100, DX9), so the routing set is not ours to invent. Curated
>    set, not a matrix.
> 2. **PM versus FM is contentious in the literature**, not settled as stated above: the modulator
>    is added to the phase accumulator (PM) while Yamaha's patent says FM. What is not contentious is
>    that modulating phase keeps feedback stable, **and our 2-op already does it that way**, so the
>    4-op inherits a correct foundation.
> 3. **Our shipped FM cannot make an inharmonic spectrum, and that is a one-line fix.** All ten
>    `harmonics` detents are rational with denominator 1 or 2, and a rational ratio p/q can only
>    produce a spectrum periodic at carrier/q. Measured both ways: our `bell` preset puts partials at
>    half-integer multiples of the carrier, while STK's `TubeBell` (a real bell) is built on **√2**
>    and lands nowhere near a common fundamental. Adding an irrational detent is cheap and has
>    `sklocken` waiting for it. The compatibility cost is real and is written up there.
> 4. **Three details that shape the sound and that we do differently**: the DX7 averages two samples
>    in the feedback path (we store one), its envelopes are linear in dB, and it lowpasses its own
>    output at 16 kHz because FM aliases by construction.
>
> Cost, gotchas and licence are answered there, including the patent question, which matters because
> we ship paid apps. The reference is wired into `ref-render` and `TubeBell` is characterised.

### 7.3 Conditional: note-tracking ring modulation — NOT AN ENGINE

The §8.9 catalog row said the fixed-Hz `ringmod()` bus effect mostly covers AM and ring mod, and what
an *engine* would add is a modulator that **tracks the played pitch**, so the clang stays harmonic
per note.

> **§5 RESEARCH ROUND: DONE (2026-09-14). DECISION A.**
> [`engine-reach-ringmod-research.md`](engine-reach-ringmod-research.md). The cheap path works.
> `ringmod_ratio(ratio, mix)` / `instrument_ringmod_ratio(slot, ratio, mix)` ride the shipped
> `FX_RINGMOD` insert: carrier = ratio × last-started voice pitch. Hz API unchanged (Dalek /
> robot / atonal clang). Proof: the `ringtrack` cart. A voice-local multiply would only buy a
> *chord* of independent clangs, which analog practice never treated as the job and which FM
> already reaches. Count stays 2. Do not reopen from vibes.

**Check the cheap thing first.** Done. It was a parameter.

### 7.4 Things that read like an engine and are not

Recorded here so they are not re-proposed:

| Candidate | What it actually is |
|---|---|
| **Wavefolder** | Tier 1, item 2 — done. `DRIVE_FOLD` ships; the matcher now searches it in the voice stage so fold and cutoff can co-optimise. No new engine. |
| **Subtractive imitation** (the Minimoog playing a trumpet, rather than the trumpet) | A cart-land `subtractive.h` header holding published parameter values as data, per the §8.9 argument and the `acid303.h` precedent. Escalate to an engine only if the pieces prove they must sit closer to the voice. |
| **Tuned noise** | §7.1 with a continuous-noise exciter. |
| **Additive** | §7.1 with a sustaining exciter. |
| **Wavetable scan, and any general approximator** | Ruled out in §1, permanently. |

### 7.5 The count

**2 engines, 2 showcase carts, 2 research rounds — both engines now ship
(`INSTR_MODAL` 2026-09-14, `INSTR_FM4` 2026-09-15).** §7.3 was answered with a parameter
(`ringmod_ratio` / `instrument_ringmod_ratio` on the shipped insert). That is the whole tier, and
it sits on top of tier 1, which needs no DSP at all.

## 8. Order of work

1. ~~Tier 1, all four items, measured before and after.~~ Done 2026-09-14 (matcher wiring;
   ten-sample table N/A — commercial WAVs not in-repo).
2. Answer §7.3 with a parameter if it can be answered with a parameter. **DONE (2026-09-14):**
   ratio API on the shipped ringmod; count stays two. See
   [`engine-reach-ringmod-research.md`](engine-reach-ringmod-research.md).
3. Decide the tier 2 question (is a two-layer patch still a patch?).
4. ~~Research round for §7.1, then its paper design, then the engine, then its cart.~~ **Done
   2026-09-14:** `INSTR_MODAL` + `modal` cart. Research and paper mapping are live; the ear
   still owns which three-macro route won. It was first because it absorbs two other
   candidates; a decaying-sine design would have split it back into three engines.
5. ~~Research round for §7.2, then its paper design, then the engine, then its cart.~~
   **Done 2026-09-15:** `INSTR_FM4` + `fm4op` cart. Research and paper mapping are live;
   the ear still owns which three-macro route won. B′ (2-op irrational detent) deferred.
6. Re-measure. The ten-sample table is the scoreboard for this whole programme, so it gets re-run
   after every tier and the numbers land back in [`patch-matching.md`](patch-matching.md) §8.

## See also

- [`patch-matching.md`](patch-matching.md) - the matcher, the loss scale, the ten-sample measurement
  this doc is built on
- [`patch-matching-cart.md`](patch-matching-cart.md) - the ear-as-loss-function fork
- [`engine-reach-modal-research.md`](engine-reach-modal-research.md) - §7.1 research:
  filter bank, not decaying sines; exciter mix; mode-count budget
- [`engine-reach-macro-mapping.md`](engine-reach-macro-mapping.md) - paper-round macros;
  both modal routes stay hearable from one build
- [`engine-reach-ringmod-research.md`](engine-reach-ringmod-research.md) - §7.3 research:
  note-tracking ringmod is a ratio parameter, not a third engine
- [`instrument-engines.md`](instrument-engines.md) - the engine program: §8.8.2 playbook, §8.9
  candidate catalog, §8.2 buffer-free versus buffered
- [`synth-secrets-audit.md`](synth-secrets-audit.md) - the synthesizer-literature audit and its
  findings against shipped engines
- [`../guides/porting-from-navkit.md`](../guides/porting-from-navkit.md) - how to port an oscillator
  without compounding errors
- [`../guides/instrument-carts.md`](../guides/instrument-carts.md) - showcase cart form factors
- [`../guides/checks-and-oracles.md`](../guides/checks-and-oracles.md) - which gate to run for which
  change
