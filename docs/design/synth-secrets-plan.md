# Synth Secrets — the build plan

STATUS: BUILDING — **Phase 0 DONE; Phase 1 is 4 of 7 (2026-07-28)**: 1.1 solina, 1.2 tr808 cymbal and
1.3 the 808/909 snare all **shipped by the owner's ear** (each keeping its old sound on a toggle), and
**1.4 brass is a recorded DROP** — Reid loses all three of his envelope numbers. The ordered work ledger
derived from [`synth-secrets-audit.md`](synth-secrets-audit.md). The audit is the *findings*; this is the
*doing*. **1.5 (the layered piano) is built and awaiting an ear call**, and it turned up an unrelated live
bug: two of `piano`'s six sliders have never done anything (see §4 "1.5" and the top of
[`STATUS.md`](../STATUS.md) → "Open"). Nothing past Phase 1 is approved.

The audit ended with nine per-section step tables and ~106 sub-findings, which is a research output, not
a work list. This file turns it into one ordered ledger, answers **how we decide an item is done**, and
answers **when a finding justifies a new engine or effect versus changing an existing one**.

Edit this file as work happens. It is meant to rot forward, not to be rewritten.

---

## 1. How we decide something is done

The audit's items are not all the same *kind* of thing, and that matters more than their size. Four
kinds, and the kind determines the gate:

| kind | what it means | gate | owner needed? |
|---|---|---|---|
| **FACT** | The code or a doc states something untrue. | Read it and fix it. | no |
| **VERIFY** | A deterministic oracle decides pass/fail. | The tool goes green. | no |
| **LISTEN** | The change is real, but whether it is an *improvement* is a taste call. | A/B, and the owner says yes. | **yes** |
| **DESIGN** | Needs a decision before any code exists. | Agreement on shape, written into an ADR or a design doc. | **yes** |

**FACT and VERIFY are "just do it".** There is no judgement in fixing a docstring that says *flat* when
the engine measures *sharp*, or in making a comb land on the harmonic it is supposed to null. Doing all
of these first is also the cheapest way to build confidence in the rest of the ledger.

**LISTEN is where your instinct that "some things might not be an improvement" lives**, and it is the
majority of the interesting items. §H3's two-stage guitar decay, §I7's bottom-octave sub-oscillator,
§F5's bow-pressure range, §E10's brass release: each is defensible from the physics *and* might sound
worse in context. The book is not the customer; the ear is.

### The A/B protocol for LISTEN items

An A/B needs the old behaviour to still exist, so **a LISTEN item is built as an opt-in from the start** —
a flag, an `eng_p` slot, a second instrument slot — never as a replacement of the default. That is not
extra work; it is the thing that makes the comparison possible, and if the verdict is "worse" the item
costs nothing to abandon.

**Use [`tools/ab-render.js`](../../tools/ab-render.js) — do not hand-roll this.** It flips one
file-scope value, renders each variant, prints the numbers in one table, and restores the source in a
`finally` block:

```bash
node tools/ab-render.js solina --set wow=WOW_CLASSIC,WOW_RANDOM,WOW_BREATHE --frames 1800
node tools/ab-render.js brass  --set A_REL=1200,200 --f0 220        # + harmonic extent / >4kHz
```

It exists because the manual version — sed the flag, render, sed it back — is three commands and a
footgun, and the footgun fired: **a regex that matches the *initial* form of a line stops matching after
the first substitution**, so every later variant silently re-renders the FIRST state. That produced three
byte-identical WAVs which were nearly written up as "the change has no effect". So the tool **exits 2 and
shouts if any two variants render byte-identical audio**, because that means the flag never reached the
DSP and every number in the table is meaningless. Treat that warning as a hard stop, not a curiosity.

Then, per CLAUDE.md's rule for handing work over, **bake the cart** so it can be played rather than only
heard as a file:

```bash
node tools/make-cart.js tools/carts/<cart>.c editor/public/carts/<cart>.cart.png   # re-embed source
node tools/make-cart.js --run editor/public/carts/<cart>.cart.png                  # bake the thumbnail
```

Check the A/B state is actually *visible* in the baked frame before handing it over — an invisible
readout makes the ear test unverifiable. `ui-audit.js` will not catch a low-contrast label, only an
off-screen or overlapping one; read the baked PNG.

**Then also hand over the WAV pair, because that is what actually gets listened to.** Both 1.1 and 1.2
were decided from rendered files, not from the editor — a file pair can be replayed back to back at will,
which is exactly what a subtle timbre judgement needs, whereas in the cart you have to re-trigger and
re-toggle from memory. `--keep` the renders, copy them to `build/ab/` under names that say which is which,
and give the `afplay` lines:

```bash
node tools/ab-render.js <cart> --set <flag>=<off>,<on> --frames <n> --keep
cp <tmp>/…_0.wav build/ab/<cart>-<OFF-name>-stock.wav
cp <tmp>/…_1.wav build/ab/<cart>-<ON-name>-new.wav
afplay build/ab/<cart>-<ON-name>-new.wav
```

Say what to listen *for* and where in the file it happens, and flag anything that could bias the ear —
above all a **level difference**, since a louder take wins on loudness alone. 1.2 shipped ~6.8 dB hot and
the caveat had to be stated for the verdict to mean anything.

**When the verdict lands: ship exactly the take that was approved.** Record the winning render's sha and
verify the new default reproduces it byte-for-byte. This is not ceremony — while tuning 1.2 an experiment
left `tr808_cym3_vel` at `0` instead of `-3`, which would have shipped a sound 4 dB hotter than the one
that was judged. Also verify the OFF path still matches its own pre-change sha, so "the old sound is
untouched" is a measurement rather than a hope. And do not later "improve" a property the ear signed off
on: if a known caveat was accepted, note in the ledger that it was accepted *on purpose*.

**Print the transfer function over its REAL input domain before trusting a strength knob.** Not the
endpoints — every value the input actually takes. 1.3's tilt curve was `(boost * dyn + 1) / 2`, which reads
like harmless half-strength scaling; but `boost` in these carts is only ever 0, 1 or 2, so at `dyn=1` the
rounding mapped boost 1 *and* boost 2 to the same tilt and the parameter stopped depending on velocity —
which was the entire feature. It measured as a clear effect, A/B'd as a clear effect, and was one message
away from shipping as the default. **A parameter can be audibly doing something and still not be doing the
thing you claimed.** A three-line loop printing the curve over its domain catches this; no audio oracle does.

Two mechanical traps that cost real time here, both worth a glance before handing over:

- **`sprintf` into a cart's shared `char buf[32]`.** Cart draw functions keep a small shared scratch buffer;
  a longer footer string overflows the stack with **no crash and no compiler warning**. In 1.3 the only
  symptom was `play.js --dump` writing zero frames while audio rendered perfectly. Use your own sized
  buffer and `snprintf`.
- **`ui-audit.js` catches off-screen and overlapping text but NOT low contrast.** It found a pre-existing
  370px footer on a 320px screen in `tr909`; it passed a grey-on-brown readout in `solina` that was
  invisible. Run it, then also read the baked PNG.

**Where a claim is STRUCTURAL, write a `spec()` instead of re-measuring it by hand.** The audio gates and
the owner's ear cover the sound; what neither can see is an invariant that stops being true while the cart
still compiles, still renders, and still sounds right. 1.3's bug is the type specimen: the tilt curve had
stopped depending on velocity and *every audible signal said it was fine*. A three-line assertion catches
it forever. Shared headers carry their own assertions via the [`spec.h`](../../runtime/spec.h) **"specs on
an includeable"** pattern — `<lib>_selfcheck()`, called from the including cart's `spec()` — so the check
travels with the code rather than with one cart. Run `node tools/spec.js <cart>`.

Two rules that make this worth the keystrokes rather than decoration:

- **Prove the spec FAILS on the bug it was written for.** Re-introduce the defect, watch it go red, put it
  back. 1.3's assertions were verified this way (2 fail on the 808, 1 on the 909 with the old curve).
  An assertion never observed failing is a guess about what it tests.
- **Assert the things a future editor could innocently break**, not tautologies. Worth having: the tilt is
  strictly increasing in velocity; the cymbal's three decays stay unequal and ordered; the top cymbal band
  stays at/below its measured aliasing ceiling; **and the preset data the A/B clips depend on** (PLANET
  ROCK's accents must miss its snare, BOOM BAP's must land on it) — a one-character preset edit would
  otherwise leave the clip silently testing nothing.

Note `spec.h` **reserves `step`**, which is the obvious name in a step sequencer and blocks `acidcandy`
from having a spec at all. See [spec-harness.md → "reserved names"](spec-harness.md#reserved-names--step-is-the-one-that-bites).

A LISTEN item is **done** when the cart has an audible toggle, the numbers are in the ledger row, and you
have said which side wins. If neither side clearly wins, the honest outcome is **DROP** — recorded with
its measurement, so nobody re-litigates it.

---

## 2. When to add a new engine or effect, versus change an existing one

You asked this directly. It is already decided policy here, in four ADRs, and applying them beats
inventing a rule:

- [0006](../decisions/0006-library-carts-not-engine.md) — capabilities ship as **library carts**, not
  engine API, unless they can't.
- [0015](../decisions/0015-effects-are-recipes-not-primitives.md) — **the effects roster is CLOSED**, and
  *"a primitive must prove it can't be a recipe."*
- [0016](../decisions/0016-combo-organ-recipe-then-macro-or-engine.md) — a recipe now; its own macro axis
  or engine **only when a station proves it**. *"Build the engine when a real customer proves the recipe
  insufficient, not because"* it is theoretically nicer — and only a **built cart** can settle it.
- [0017](../decisions/0017-three-macro-core-plus-engine-aux-channel.md) — keep the three macros; use the
  blessed per-engine **`eng_p` aux channel** for structural exceptions, never a fourth macro.

Read together they give a ladder. **Take the lowest rung that can hold the finding, and escalate only
when a built cart has failed on the rung below.**

| rung | reach for it when | examples from the audit |
|---|---|---|
| 1. **Cart / preset** | It is only parameter values. | §E10 brass envelope · §I9 layered piano · §F2 solina's unused LFOs · §J5 the 808 cymbal's three decays |
| 2. **Data table in the engine** | Values live in `sound.h` but no code changes. | §L5 two registrations · §J2/§J3 the membrane ratio and weight arrays · §B6 formant amplitudes |
| 3. **Cart-land header** | A recipe two or more carts would otherwise each reinvent. | §G `subtractive.h` · §B3 `mono.h` — precedent: `acid303.h`, `tr808.h` |
| 4. **`eng_p` aux channel** | A note-on *structural* switch on an existing engine. | §I2 hammer position · §L2 Second/Third percussion · §K4b open/closed bore · §H7 pick position |
| 5. **New enum value** | Extends an axis that already exists. | §C3 resonance as a mod dest · §C2 LFO rate/depth · §C5 a 6 dB filter mode |
| 6. **Engine internals** | The DSP is wrong or absent, and no parameter reaches it. | §E4 brightness ramp · §H3 two-rate decay · §M3 early reflections · §H6 the pickup |
| 7. **New `INSTR_*` or effect** | Nothing above reaches it **and** a built cart proved it. | almost nothing — see below |

**The test for rung 7** is the one `morphdrum.h` discovered: an 808 kick and a 909 kick are *the same
structure at different parameter values*, so they are one engine, not two. So ask: **is the signal
topology different, or is it the same topology at other settings?** Only a different topology earns a new
id.

Applying that to the audit, **nothing clearly earns rung 7**:

- **§G subtractive imitation** is a genuinely different topology (oscillator → resonant filter → VCA, not
  a waveguide) — but every piece already exists as a primitive, so 0015/0016 put it at **rung 3**, a
  `subtractive.h` holding Reid's published patch values as a cited voicing table. Escalate only if a
  built cart proves the header can't hold it.
- **§H6 the electric guitar** is the closest call, because it produces a sound we cannot currently make at
  all, and every amp cart we ship is aimed at it. But a pickup is a second tap on an existing Karplus
  line — **rung 6**, an addition to `INSTR_GUITAR`, most likely selected by `eng_p`. Not a new engine.
- **§M's effects gaps** are all rung 6 inside existing effects (early-reflection taps, chorus paths, BBD
  scaling). 0015's closed roster is not threatened.
- **§C4's ring-mod carrier waveform** is rung 5 — a parameter on an effect we already ship.

So the honest answer to your question: **on this list, we almost never need a new engine or effect.**
Most of the work is rungs 1-2 (free, cart-visible) and rung 6 (fixing DSP that is already there). That is
a good sign about the audit — it found voicing and correctness problems, not missing machinery.

---

## 3. Phase 0 — free, factual, and no ear required

Do these first. None needs a decision, none can make anything sound worse, and **five of them decide
whether later items are worth doing at all** — 0.1 gates 3.30, 0.11 gates 3.6, 0.13 gates 3.19, 0.14
gates 3.16, and 0.15 may collapse two Phase 3 items (§H8 and §I5) into a single cause.

| # | item | kind | rung | where | note |
|---|---|---|---|---|---|
| 0.1 | Pulse-width harmonic oracle (§C12) | VERIFY | tool | none | **Decides §B5.** Assert sinc-envelope nulls, not 1/n. Also measures how badly the un-BLEP'd pulse smears |
| 0.2 | `studio.h` PIPE caveat says *flat*, measures **sharp** at morph 0.4 (§K3) | FACT | docstring | none | Actively misleads anyone voicing a flute |
| 0.3 | `studio.h` PIPE "octave flageolet" never happens (§K2) | FACT | docstring | none | Fix the doc now; the *behaviour* is 4.6 |
| 0.4 | [`audio-notes.md`](audio-notes.md) §18 "±3¢ at any sane embouchure" (§K3) | FACT | doc | none | Already annotated; narrow the source claim too |
| 0.5 | [`brass-realism-handoff.md`](brass-realism-handoff.md) h9→h17 not reproducible (§E9) | FACT | doc | none | Pin the macros the number was taken at |
| 0.6 | Comment that PIANO's averaging comb sign is **load-bearing** (§I1) | FACT | comment | none | A future "tidy-up" unifying the two combs would silently break the physics |
| 0.7 | Comment why PLUCK's `0.55` comb coefficient is defensible (§H1) | FACT | comment | none | Currently reads as a magic number; the body puts the notched harmonics back |
| 0.8 | Comment: `pn_dd` mis-attributes two-rate decay (§H3) | FACT | comment | none | It is not what says "struck"; plucked strings have it too |
| 0.9 | **Mark the four verified tables as verified** (§A3, §J1, §L1, §F1) | FACT | comment | none | Vowel formants, membrane Bessel ratios, Hammond drawbars, FM ratios — each matched the book exactly. A one-line citation stops someone "improving" them |
| 0.10 | Wet/dry filter blends are a comb, not a gentler filter (§B8) | FACT | doc | none | Into [`../guides/effects-recipes.md`](../guides/effects-recipes.md); no DSP change |
| 0.11 | Does `instrument_lfo` already accept 80 Hz? (§E3) | VERIFY | none | none | **Gates §E3.** Five minutes |
| 0.12 | Bow-position comb lands on the predicted harmonics (§F6) | VERIFY | none | `bowed` | Free proof the waveguide geometry is right |
| 0.13 | Which of three bore spectra does PIPE produce? (§K4) | VERIFY | none | `pipe` | **Gates §K4b** |
| 0.14 | MEMBRANE's perceived pitch vs the 1,1 principal (§J2b) | VERIFY | none | `tabla` | May be a fifth off what a timpanist calls the note |
| 0.15 | High-register loss: GUITAR/PLUCK harmonics and PIANO decay (§H8, §I5) | VERIFY | none | `pluck`, `piano` | Two engines, possibly one cause (loop coefficients fixed per period, not per note) |

**Deliverable:** ten doc/comment fixes and five measurements, five of which change what we do next.

### ✅ Phase 0 results (2026-07-28)

All fifteen done. Compile-gated (`soundcheck` clean, no `[sound]` warnings) and `tune-check --quiet`
exit 0. **Four rows changed the rest of the plan:**

| row | result | effect on the plan |
|---|---|---|
| **0.1** | Aliasing is a **non-issue.** 1/3-duty nulls measure **42–58 dB deep at every pitch** (A1→A5, no collapse); inter-harmonic energy — the actual aliasing probe — is **−74 dB at A1, −87 at A2, −59 at A4, −53 at A5**, so it does grow with pitch as theory says but never approaches audibility. The worst case §B5 predicted, a **PWM sweep at A5**, measures **−55.9 dB**, i.e. no worse than static. | **3.30 → DROP.** No permanent oracle tool needed either; the cheapest rung held |
| **0.11** | `instrument_lfo` has **no rate ceiling**, and 80 Hz genuinely works: modulating cutoff at 80 Hz on a 110 Hz carrier puts sidebands at **exactly 110±80 Hz, both up ~30 dB**, with the carrier depleted 12 dB. Reid's "growl" is reachable with shipped primitives. | **3.6 unblocked and cheaper** — it may not need engine work at all |
| **0.13** | `INSTR_PIPE` is a **CLOSED (odd-harmonic) pipe**: evens sit **50–73 dB** under the odds (h2 −52.5, h4 −53.8, h6 −73.5 against h3 −9.5, h5 −14.4). So it is spectrally a **pan pipe**, and its "flute"/"recorder" presets are structurally wrong — those need even harmonics. | Sharpens §K4 from "unknown" to known. **Also explains §K2**: a closed pipe overblows a *twelfth*, not the octave the docstring promised, so the doc described a different instrument than the code implements |
| **0.15** | **§H8 and §I5 are one cause.** PLUCK's harmonic extent collapses h23 → h17 → h7 → **h2** across A1→A4, tracking PIANO's decay collapse from §I0. Both are consistent with loop coefficients fixed **per pass** rather than scaled to the note, so higher pitches get more passes per second. | **Two Phase 3 items merge into one fix** |
| **0.12** | Bow-position comb **verified**: at β=1/4 the three deepest nulls are exactly **h4 (−25.0), h8 (−32.4), h12 (−35.9)**; at β=1/5 they are **h5, h10**. | The waveguide string is right, so §F4's gap really is *only* the missing body |
| **0.14** | **Confirmed:** MEMBRANE's perceived pitch is the **0,1** mode (0.0¢ at A2/A3/A4), where a real timpani's is the **1,1**. Our loudest mode is the one the detector locks to. | Folds into **3.16** rather than being separate — it is the same fix as §J2+§J3 |

The ten FACT rows are all applied: the `studio.h` PIPE docstring now records the closed bore, the
non-overblowing macro and the non-monotonic tuning; `sound.h` carries the load-bearing-comb-sign warning,
the 0.55 justification, the corrected `pn_dd` attribution, and **four "✅ VERIFIED against Part N" notes**
on the tables that matched the book exactly (vowel formants, membrane Bessel ratios, Hammond drawbars, FM
ratio detents); `audio-notes` §18 and `brass-realism-handoff` no longer overclaim; and the comb caveat is
in `effects-recipes.md`.

---

## 4. Phase 1 — cart-only, each its own audible proof

No engine change. Every one is a cart edit, so every one is immediately hearable, and they are the
cheapest LISTEN items we have.

| # | item | kind | rung | cart | note |
|---|---|---|---|---|---|
| 1.1 | `solina`: use `LFO_DETUNE` + a Random-shape LFO on it (§F2) | LISTEN | 1 | `solina` | ✅ **DONE — BREATHING kept as the default** (owner's ear, 2026-07-28), CLASSIC retained on a toggle; middle rung DROPPED. See below |
| 1.2 | 808 cymbal: three bands, three unequal decays (§J5) | LISTEN | 1 | `tr808` | ✅ **DONE — 3BAND is the default** (owner's ear, 2026-07-28), 1BAND kept on key **C**. See below |
| 1.3 | Velocity → snare tone/noise balance (§J9) | LISTEN | 1 | `tr808`, `tr909` | ✅ **DONE — `dyn=1` is the default** (owner's ear, 2026-07-28) on both machines, 0 kept on key **N**. See below |
| 1.4 | Brass preset: 1 ms attack → 100 ms, 1200 ms release → short (§E10) | LISTEN | 1 | `brass` | ❌ **DROPPED — Reid loses all three** (owner's ear, 2026-07-28). Envelope unchanged, byte-identical. The most instructive item so far; see below |
| 1.5 | A two-slot layered piano patch (§I9) | LISTEN | 1 | `piano` | ✅ **DONE — liked, and kept OPT-IN on key L** (owner, 2026-07-28). Layer-off is byte-identical, so it is purely additive. Also **found + fixed** a one-bound engine bug that had killed two sliders — see below |
| 1.6 | Hammond: the sawtooth-ish and square-ish registrations (§L5) | LISTEN | 2 | `organ` | Two rows in `REG[8][9]` |
| 1.7 | Loudness→brightness by waveform morph; filter-as-gate (§F7) | LISTEN | 1 | `martenot`, `brass` | Part 51's two liftable tricks, no filter needed for the first |

**Deliverable:** seven A/Bs, each a baked cart you can play. If all seven land, that is a visibly better
instrument shelf for zero engine risk.

### 1.1 solina — built, baked, awaiting your ear (2026-07-28)

Key **W** cycles three states, shown on the panel under the ENSEMBLE switch. Reid's Part 46 ladder is
three rungs and this exposes all of them rather than jumping to the end:

| state | what it does | measured (420 frames) |
|---|---|---|
| **CLASSIC** | as shipped — one 0.16 Hz sine wow, fixed per-tab detune | peak −5.0 dB · centroid **2267 Hz** |
| **RANDOM WOW** | same depth, but `LFO_SHAPE_RANDOM` and rates staggered per tab so the six wows never line up | peak −5.0 dB · centroid **2260 Hz** |
| **BREATHING DETUNE** | the full ladder — `instrument_unison(s,3,0.10)` + `LFO_DETUNE` on a Random shape | peak **−6.8 dB** · centroid **2489 Hz** |

`unison` is intra-voice (the detuned copies live in `Voice.uni_ph[]`), so BREATHING DETUNE costs **no
polyphony** — that was the thing worth checking before building it.

**Honest limit on the measurements.** BREATHING DETUNE is clearly different: +222 Hz centroid from the
unison sidebands, and 1.8 dB quieter because unison loudness-normalises. But **CLASSIC vs RANDOM WOW is
not measurable with any oracle we have.** The difference is the *character* of a 0.16 Hz modulation — one
cycle per six seconds — buried under a chord progression, and `wav-modrate` locks onto the 14.5 Hz chord
rate instead, the same failure mode Phase 0 hit. A 30-second render did not help. So rung 2 is
**ear-only**, which is exactly what the LISTEN category is for; there is no number to hide behind.

**✅ VERDICT (owner, 2026-07-28): BREATHING wins and is now the default — kept as a toggle, not a
one-way migration.** The switch is two states, `W` or tap the label: **BREATHING** (default) and
**CLASSIC** (as the cart shipped). The middle rung, a random-shape *pitch* wow with no unison, is
**DROPPED** — it measured indistinguishable from CLASSIC (centroid 2260 vs 2267 Hz) and no oracle we
have can see a 0.16 Hz character change under a chord progression, so shipping a third state nobody can
tell apart was panel clutter. Per §1's DROP rule the measurement and the code to restore it are recorded
in the cart's own comment, so it need not be re-derived.

**The pattern for the remaining six:** default to the winner, keep the previous behaviour on a toggle,
make the toggle *visible and tappable* (not keyboard-only), and mention it in the cart's `de:meta`
description since it becomes a user-facing control. Two traps this one hit, both worth avoiding: a
low-contrast readout that `ui-audit` passes clean (it catches off-screen and overlapping text, not
contrast — read the baked PNG), and a help array grown past its draw-loop bound.

### 1.2 tr808 cymbal — ✅ THREE BANDS IS THE DEFAULT (owner's ear, 2026-07-28)

**✅ VERDICT (owner, 2026-07-28): the three-band cymbal wins and is now the default**, with the stock
single-band voice kept on key **C** / tapping `CY 3BAND` — a toggle, not a one-way migration, the same
shape as 1.1. Judged by ear from rendered WAV pairs (`build/ab/tr808-cymbal-{1BAND-stock,3BAND-new}.wav`).
The shipped default renders **byte-identically to the take that was actually approved** (sha
`ff2477695836`) and the OFF path renders byte-identically to the pre-change cart (sha `90dc75069555`), so
both ends of the switch are provably the exact sounds that were compared.

Stop the sequencer with SPACE and hit **F**: no preset has a cymbal row, so F is the only way to strike it.
All of this is `runtime/tr808.h` + a toggle in the cart; **no engine change**.

**It works, and the measurement is unambiguous.** On an isolated crash
(`tools/clips/tr808/01-cymbal-solo.script`, committed) the spectral centroid per 100ms window:

| | at strike | +100ms | +200ms | +300ms → end |
|---|---|---|---|---|
| **1BAND** (stock) | 11512 Hz | 11882 | 11824 | 11886 … 11939 — **flat**, within 1.5% for the whole ring |
| **3BAND** | **14895 Hz** | 12929 | 11844 | 11909 … 11939 — **bit-identical to stock** |

That is exactly what Part 39 promises: a 3050 Hz downward walk in the first 200ms, then the upper bands
are gone and what remains *is* the stock low band, converging on it to the sample. The stock path is
**byte-identical** to before the change (same WAV sha, verified) — `cym3=0` is a proven no-op.

**Three things I got wrong first, all worth keeping written down:**

1. **The high band was a fold-over amplifier, not a cymbal band.** A `FILTER_HIGH` at 7800 passes
   everything above 7800, which on `INSTR_SQUARE` means it passes the oscillator's **aliasing**. As a
   stem (`play.js --solo-slot 24`) it measured −0.0 dBFS, clipping on its own, 14 dB over the band it was
   meant to colour, centroid **21942 Hz** against a Nyquist of 22050. Reid actually says *band-pass*, and
   taking him literally fixed it. Lesson: on a square-wave voice, a band above ~7 kHz needs a stem check.
2. **The real 808 value was already written down and unimplemented.** `tr808.c`'s docblock records the
   reverse-engineered cymbal as "bandpasses at **7100**/3440Hz" — only 3440 was ever built. The sweep
   independently found ~7000 to be the highest corner that doesn't fold, which lands on 7100 on its own.
3. **It cannot be fully level-matched, and both levers are dead ends.** `instrument_level` is out because
   `acidcandy` and `dubjam` use it as their per-slot *mixer* (`for i < TR808_NSLOT`, one fader → every
   slot), which would silently overwrite any balance a band set for itself. Velocity is a **cliff**, not a
   fader: `tr808__vv` clamps 0..7 and the cymbal already fires at ~2, so offset −4 goes silent and −3 is
   the last audible step. So 3BAND lands **~6.8 dB hotter at the strike** (−7.2 vs −14.0 dBFS), with no
   clipping. **This was flagged for the ear call and the ear approved it anyway**, so the loudness is part
   of the accepted sound and the mixer refactor is *not* needed. Left undone on purpose: it would touch
   three carts' mixers to "fix" a level nobody objected to.

**A voice-economy option, measured and deliberately not taken.** Firing only ONE bank member into each
colour band instead of three (`tr808_cym3_members = 1`, with `vel` at −1) matches the shipped level and
the shipped centroid walk to within **0.4 dB and ~30 Hz**, for **5 voices per crash instead of 9**:

| members / vel | strike | centroid walk | voices |
|---|---|---|---|
| **3 / −3** | −7.2 dBFS | 14895 → 12929 → 11844 Hz | 9 ← shipped (the approved take) |
| 1 / −1 | −6.8 | 14843 → 12906 → 11825 | 5 |
| 1 / 0 | −4.6 | 15619 → 13248 → 11811 | 5 (widest walk) |

Not adopted, because equal numbers are not an equal sound: with one member the colour bands are a single
pitch instead of three enharmonic ones beating together, and quietly swapping that in for the take that
was actually listened to would be substituting a different sound for an approved verdict. It is a one-line
switch documented in `tr808.h` for if voice pressure ever bites.

**Also fixed on the way** (the slot count went 14 → 16): `acidcandy.c` hard-coded `#define D909_BASE 23`,
which the two new slots would have silently overlapped. It is now derived —
`(TR808_BASE + TR808_NSLOT)` — so the next slot change can't repeat it. Checked every includer of
`tr808.h` (`acidcandy`, `acidcandy_ipad`, `dubjam`, `tr909.h`, `morphdrum.h`) and every cart of those;
all compile, and `voice-trace` shows **no steals** with the pattern running plus 8 crashes (9 voices per
crash, 143 note-ons, the only 4 chokes being the 808's intended closed-hat-mutes-open-hat).

### 1.3 snare velocity → noisiness, both machines — ✅ `dyn=1` IS THE DEFAULT (owner's ear, 2026-07-28)

**✅ VERDICT (owner, 2026-07-28): the velocity-dependent snare wins on both machines and `dyn=1` is now
the default**, with the fixed-balance original kept on key **N** — a toggle, not a migration, as in 1.1/1.2.
Both shipped defaults render byte-identically to the takes that were approved (`a6dc6c0a02e7` on the 808,
`b40f2782577b` on the 909), and soft hits are byte-identical to the pre-change carts.

Key **N** cycles 0/1/2 in both `tr808` and `tr909` (state in the 808's `hint()` footer; next to POLY on
the 909). Part 35 says harder strikes make "the spectrum become more noise-like", but `boost` was added to
the body and the noise layers *equally*, so an accent was the same snare turned up with the tone-to-noise
ratio frozen. Now they tip in opposite directions: noise up, body down. Cart-side only, **no engine change**.

The second half of §J9 — the sound evolving toward noise *over* the note — turned out to be there already
by accident: the noise slot outlives the body in both machines (808 130ms vs 100ms, 909 170ms vs 90ms), and
a single hit's centroid climbs 10890 → 12279 Hz through its own decay. Nothing to do; recorded so it isn't
"fixed" twice. Per-hit *decay* scaling is not available anyway — with `sustain 0` the slot's `decay_ms`
governs the ring, so the gate length can't shorten it, which leaves level as the only per-hit lever.

Measured on one accented 808 hit, snare stem only:

| `dyn` | peak | noise share (HF/total) | centroid |
|---|---|---|---|
| 0 (stock) | −5.1 dBFS | 1.347 | 11393 Hz |
| **1** (default) | −6.8 | **1.849** (+37%) | 11713 |
| 2 | −8.6 | 2.272 (+69%) | 12051 |

The peak dropping as the noise rises is the point: an accent buys grit instead of level. The 909 tracks it.

**The clips are self-verifying, which is the part worth copying.** Each plays a preset whose accent row
*misses* the snare, then one whose accents *hit* it — `PLANET ROCK` → `BOOM BAP` on the 808, `THE BELLS` →
`GABBER` on the 909, all straight from the carts' own preset data. The first region must come out
byte-identical between settings (it does: `5d3c3a52085a` on the 808 at all three values, `11a020cea0` on
the 909), which *proves* soft hits are untouched and only accents moved. A structural claim checked by sha
rather than by argument.

**A bug this nearly shipped, and the reason to distrust "it measured as a real effect".** The curve began
as `(boost * dyn + 1) / 2`, which reads like harmless half-strength scaling. But `boost` here is only ever
0, 1 or 2, so at `dyn=1` that rounding mapped boost 1 **and** boost 2 to the same tilt: the balance stopped
varying with velocity at all — the one thing §J9 is about. It still measured as a clear effect, still A/B'd
as a clear effect, and was offered as the "gentle" default. Nothing looked wrong. It was caught by
evaluating the curve over its actual input range when asked which setting was best, not by any oracle. The
curve is now plainly `boost * dyn`; the old `dyn=2` is byte-identical to the new `dyn=1` (verified on both
machines), so the recommended default is exactly the take that was rendered for listening.
**Lesson for the remaining items: print the transfer function over its real input domain, not just its
endpoints.** A parameter can be audibly doing something and still not be doing the thing you claimed.

**Two more, both found by tools rather than by reading:**

1. **A stack buffer overflow I introduced.** `sprintf`ing the new ~64-char footer into the carts' shared
   `char buf[32]`. No crash, no compiler warning — the only symptom was `play.js --dump` silently writing
   **zero frames** while audio rendered perfectly. Both carts now use their own sized buffer with
   `snprintf`. Worth knowing that `buf` in these carts is small and shared.
2. **A pre-existing overflow in `tr909`:** its footer was 370px wide on a 320px screen, so
   `POLY:tap=length` had never been visible to anyone. `ui-audit.js` flags it (it *does* catch off-screen
   text — it's low *contrast* it can't see). Shortened to name the two on-panel buttons instead.

### 1.4 brass amp envelope — ❌ DROPPED: Reid loses all three numbers (owner's ear, 2026-07-28)

**The most instructive item in Phase 1, precisely because it failed.** Part 26 contradicts all three of our
brass envelope numbers. Each was built as a live toggle, level-matched, and A/B'd as rendered WAVs. **All
three lose**, each for a different reason, and the envelope now stands exactly as it shipped — verified
byte-identical (`af3631b9329e`).

| | Reid (Part 26) | ours | why his doesn't transfer |
|---|---|---|---|
| **attack** | 100 ms | 1 ms | A note already reaches full level in **~40 ms** with the amp attack at 1 ms — that onset is the **bore** establishing oscillation. A Minimoog has no bore, which is exactly *why* he needed an envelope to fake one. Applied literally: ~80 ms (envelope and bore overlap rather than add) and a far softer start. A swell, not a note. |
| **sustain** | maximum | 4 of 7 | `decay_ms` is 0, so there is no decay stage and sustain is a pure **level trim**. 4 → 7 measured **+4.86 dB**; 20·log₁₀(7/4) = 4.86 dB to the decimal. "Sustain maximum" is arithmetically "turn it up". |
| **release** | ~instant | 1200 ms | **The one I expected to win.** Swept level-matched (119 / 247 / 397 / 596 / 1192 ms tails) the owner picked the **shipped 1192 ms** — the shorter ones read as *cut off*. Same cause as the attack, mirrored: the release doesn't merely close a VCA, it **truncates the bore's ring-down**. Our 1200 ms is not a pad envelope by mistake; it is roughly how long this bore takes to stop ringing. |

**Why this is worth more than a win: the short releases were provably CLEAN and still wrong.** The largest
sample-to-sample step in the release is 61% of peak — *identical in every variant including the shipped
one*, i.e. just this waveform's own slew. No click, no discontinuity; the 5 ms version falls
1.00 → 0.50 → 0.16 → 0 over ~6 ms, a smooth ramp. Nothing was broken. It simply amputated a tail the
physical model was generating, and **no oracle we have can distinguish "a clean short decay" from "the
resonator was cut off" — only an ear comparing the two.** That is the strongest argument in this whole
plan for why LISTEN is a category and not a formality.

**Two calls I got wrong, in order, both caught by the owner:**

1. I shipped 120 ms as the recommendation on the strength of a smooth measured envelope. Wrong: it reads
   as cut off.
2. When first told "both newer ones get cut off", I inferred 120 ms needed *lengthening* and swept upward.
   Also wrong — on a direct A/B against SHIPPED the answer was that **no** shortening works. And a sha
   check showed the file the owner then called "fine" was byte-identical to the one they had just called
   cut off, i.e. **an absolute judgement flipped when the comparison was put back.** Always hand over the
   pair, never a single file, and re-confirm against the incumbent before concluding anything.

**Recorded as a DROP, not silence** (§1's rule): the toggle was removed rather than left as clutter — a
control whose alternatives both lose is not a feature — and `brass.c` carries the measurements plus the
four lines needed to rebuild all three states. The driving clip is committed at
`tools/clips/brass/01-one-note-hold-release.script`.

**This sharpens §G.** Three independent numbers from one worked Reid patch, every one non-transferable to a
waveguide, each for a *structurally different* reason (the model already does it / the parameter isn't what
it is on his hardware / it destroys the model's own behaviour). Any future item that lifts numbers from a
subtractive patch should expect this and A/B rather than edit.

Fixed two pre-existing UI faults while in the cart, both flagged by `ui-audit`: the footer ran to x=394 on
a 320px screen (so `(mute = wah)` had never been visible to anyone — this also clears the cart's own
long-standing `de:meta.todo` about it), and the patch readout was printed on top of the slide's hint,
making half that line unreadable. The readout also stops hard-coding `1,0,4,1200` and now derives from the
`BRASS_*` constants, so it cannot drift from the patch. Three overlaps remain, all pre-existing.

### 1.5 piano two-slot layer — ✅ LIKED, and deliberately kept OPT-IN (owner, 2026-07-28)

**✅ VERDICT: the owner likes the layering, and it stays OFF by default on key L** — the one item so far
that is approved *and* not made the default, for a specific reason rather than hesitancy. This cart's six
presets are declared **acceptance tests** ("if 1 grand / 2 bright / 3 harpsi … don't each sound like
themselves, the macro mapping is wrong"), and layering changes what all six sound like, so defaulting it
would quietly retune the yardstick the cart exists to be. Unlike 1.2 and 1.3, the shipped behaviour here was
not *wrong* against the book — the layer is a musical addition, so opt-in is the honest place for it.
**Layer-off renders byte-identical to the pre-session voice (`25cb93583e73`), verified**, which is what makes
"purely additive" a measurement rather than a claim.

Key **L** toggles it; `build/ab/5-piano-{BEFORE,AFTER-layered}.wav` is the pair. One struck note, 7s ring
(`tools/clips/piano/01-one-note-ring.script`).

Part 45's conclusion, which Reid calls the important secret: two voices "similar enough to be
indistinguishable within the composite, but different enough to create a sound that is more interesting
than either of the components in isolation". Two `INSTR_PIANO` slots ~7 cents apart, the second darker and
knockier, **level-matched** (−23.25 vs −24.00 dBFS) with a near-identical centroid (2438 vs 2441 Hz) — being
indistinguishable *is* the spec, so matching those is the design, not a compromise. The beating stands in
for the tricord coupling of §I3 that our single-string model does not have.

**Half of it is not reachable, and the measurement says so plainly.** Reid also has layer B *outlive* A so
it is left holding the tail ("Piano 1B dominates again … thanks to the longer Decay and Release in ENV2").
Stem renders (`play.js --solo-slot`) show B dying **~0.5 s sooner** than A, and nothing available fixes it:

- a longer amp release + a longer gate do **not** extend an `INSTR_PIANO` note — the engine's own string
  decay governs the ring-down, so the envelope's tail has nothing left to hold. That is the third time this
  session the same lesson has landed (the 808 cymbal's decays, the brass release, now this);
- and **there is no string-decay aux param at all**: `INSTR_PIANO` publishes exactly
  `MODE_STRING_WEIGHT` (0) and `MODE_STRING_CLICK` (1).

So the item delivers Reid's *primary* mechanism and not his secondary one. Getting the crossfade needs a
per-slot string-decay parameter, which is an engine change — Phase 3 work, deliberately not smuggled in here.

### ✅ And it uncovered — then fixed — a one-bound engine bug that killed two sliders

Chasing the missing decay control turned up something better than the item itself: **`piano.c`'s "decay" and
"knock" sliders had never done anything, and the cart was not at fault.** `instrument_mode()` guarded with
`idx >= 2`, so indices 2 and 3 were dropped **in the setter** — while the piano engine implements both end to
end (`sound_piano_start` reads `eng_p[2]` as the double-decay scale and `eng_p[3]` as the hammer-knock scale,
copied to the voice at note-on, bank-defaulted to 0.5 = 1.0×). Nothing was missing. Two finished engine
parameters were unreachable.

Guard widened to `idx >= 4` (`eng_p` is four wide). **A no-op at rest** — a slider at 0.5 sends exactly the
bank default it was already using, so the shipped piano is byte-identical (`25cb93583e73`) — and live once a
slider moves: sweeping idx 2 moves brightness 0.067 → 0.107 → 0.165 and the centroid 2127 → 2419 → 2755 Hz.
`MODE_PIANO_DECAY` / `MODE_PIANO_KNOCK` added through the four-place treatment so no cart needs raw indices.
Gates: soundcheck silent, `tune-check` no new drift, `level-check` + `dc-check` clean, 569/569 carts compile.

**I got this wrong twice before getting it right, and both mistakes are instructive:**

1. **A probe that measured itself being clobbered.** The first "proof" that idx 2 was dead was a
   byte-identical render — but the probe *added* an `instrument_mode(I_PNO, 2, …)` call and `push_knobs()`
   then overwrote it one line later. It was a real byte-identical result measuring the wrong thing.
   **When probing a value the cart also writes, replace the cart's write; never add a second one.**
2. **A confident wrong diagnosis written into three docs.** On that bad evidence I wrote up "the indices do
   not exist" and "the fix is already in `guitar.c`" — into the cart, its `de:meta.todo`, and the top of
   `STATUS.md`. `guitar.c` was a red herring: it simply uses indices 0 and 1, which always worked. Reading
   the engine (`eng_p[4]`, and its own comments naming all four) is what settled it. **A grep-and-infer
   diagnosis deserves less confidence than the prose it gets written up in** — the audit's own rule
   ("verify a claim by reading") applies to my own findings too.

**Left open**, and now the more interesting half: `instrument_mode` **does not validate its index**, so an
out-of-range one is silently ignored. That is precisely how a dead user-facing control survives — it
compiles, runs, and looks fine. A `[sound] WARNING` would have caught it instantly, and `soundcheck` already
greps for exactly that. Recorded at the top of [`STATUS.md`](../STATUS.md) → "Open".

---

## 5. Phase 2 — the four cross-cutting themes

**This is the highest-leverage phase and the reason the ledger is ordered this way.** Each theme closes
between four and six separate audit findings at once. Doing them as themes rather than as scattered
per-engine items is the single biggest saving available.

| # | theme | closes | kind | rung | note |
|---|---|---|---|---|---|
| 2.1 | **Keyboard tracking** (§B2) | Parts 6, 23/24, 26, 46, 54 — six chapters | DESIGN → 5 | new enum + a call | The most-requested missing feature in the whole series. Part 26 pins the value: cutoff should track at **≈0.93/octave** ("190 percent" per octave, not 200). Two halves: `instrument_keytrack(slot, amount)`, and env/LFO cutoff depth in **octaves** rather than Hz |
| 2.2 | **Trigger policy** (§B3) | §L4, §K6, §H9, §M8, and four monosynth carts each hand-rolling it | DESIGN → 3 | `mono.h` | §L4 vs §K6 is the argument: the **Hammond percussion must be single-trigger** and the **flute chiff must be multi-trigger**, and in both cases it decides whether the defining transient happens at all. So it is a property an *instrument declares*, not a cart convention. Start as a cart-land header per 0016 |
| 2.3 | **Level-dependent inharmonicity** (§E8, §H, §I4, §J8, §K8) | five families | LISTEN | 6 | One physical fact — partials sharpen with *amplitude* as well as pitch — modelled statically at best in five engines. Prototype on **one** engine (PIANO has the machinery), A/B, then decide whether to generalise |
| 2.4 | **Coupling** (§E5, §H5, §I3, §M2) | four findings | DESIGN → 6 | engine | One architectural question with four faces: the brass bell that should fill the series natively, the guitar body with no return path, the piano tricord that does not exchange energy, and §M2's cheaper alternative (three parallel 1-4 ms delay lines *are* a body). **Do §M2's A/B first** — it may answer all four cheaply |

**Sequencing note:** 2.1 and 2.2 are prerequisites for a lot of Phase 3 and for §G, so they come first
even though they are the largest items here. 2.4 should start with a measurement, not a build.

---

## 6. Phase 3 — per-engine, LISTEN-gated

Ordered by (cheapest × most likely to be an improvement). Every row is opt-in per §1's protocol.

| # | item | § | kind | rung | cart |
|---|---|---|---|---|---|
| 3.1 | LFO delay / fade-in — delayed vibrato is currently **inexpressible** | C1 | LISTEN | 5 | `21-lfo` → `reed`, `bowed`, `vox` |
| 3.2 | Resonance as a mod destination | C3 | VERIFY | 5 | `tb303`, `djfilter` |
| 3.3 | LFO rate + depth as mod destinations | C2 | LISTEN | 5 | `21-lfo`, `lfoshapes` |
| 3.4 | Ring-mod carrier waveform → the inharmonic-metal family | C4 | LISTEN | 5 | `tr808`, `gamelan`, `handpan` |
| 3.5 | Brass vibrato: delayed **and** defeatable (also unblocks §E9's oracle) | E2 | LISTEN | 6 | `brass` (+ `reed`, `pipe`, `bowed` share the pattern) |
| 3.6 | Brass onset rasp: ~80 Hz, AD-gated, into the **filter** not the pitch | E3 | LISTEN | 6 | `brass` |
| 3.7 | Brass brightness **ramp** (~600 ms), not a 14 ms level follower | E4 | LISTEN | 6 | `brass` |
| 3.8 | `BOWED` gets a body — biquads, or §M2's three short delays | F4 | LISTEN | 6 | `bowed` |
| 3.9 | Fix the pizz "string + body" claim in cart text and `studio.h` | F4b | FACT | doc | `bowed` |
| 3.10 | Electric guitar: a pickup tap (position comb + velocity weighting) | H6 | LISTEN | 6 | `combo`, `pedalboard`, `tubescreamer` |
| 3.11 | Two-rate decay for GUITAR/PLUCK (port `pn_dd`) | H3 | LISTEN | 6 | `guitar` |
| 3.12 | Fractional-delay the pick comb | H2 | VERIFY | 6 | `pluck` |
| 3.13 | Sympathetic resonance on GUITAR (port `pn_symp`) | H4 | LISTEN | 6 | `jangle` |
| 3.14 | Pick position onto `eng_p` | H7 | LISTEN | 4 | `guitar` |
| 3.15 | Hammer position by register (1/7 → 1/15) | I2 | LISTEN | 4 | `piano` |
| 3.16 | Membrane: retarget the "tuned" ratios **and** mode weights together | J2+J3 | LISTEN | 2 | `tabla` |
| 3.17 | Strike position by mode **family**, not index | J4 | LISTEN | 6 | `tabla` |
| 3.18 | Refit PIPE's clamped `ex` ramp for jetLen 5, 7 **and** 9 | K3b | VERIFY | 6 | `pipetune` |
| 3.19 | Open/closed bore flag | K4b | LISTEN | 4 | `pipe` |
| 3.20 | Percussion Second/Third + Fast/Slow | L2+L3 | LISTEN | 4 | `organ` |
| 3.21 | Tonewheel leakage | C8/L6 | LISTEN | 6 | `organ` |
| 3.22 | Early-reflection taps off the existing predelay buffer | M3 | LISTEN | 6 | `reverbspace`, `cathedral` |
| 3.23 | Three-phase chorus option | M4 | LISTEN | 6 | `solina`, `juno`, `organ` (must stay single) |
| 3.24 | BBD saturation scales with tap distance | M6 | LISTEN | 6 | `aquapuss` |
| 3.25 | 6 dB/oct one-pole filter (= the honest portamento circuit) | C5 | LISTEN | 5 | `22-filter`, `eq` |
| 3.26 | Glide in semitones, not linear Hz | B1 | LISTEN | 6 | `heldnotes` — ⚠ regate `tune-check` + `psola-check` |
| 3.27 | Key-scaled envelope times | B10 | LISTEN | 6 | `piano`, `20-instruments` |
| 3.28 | Per-voice character + round-robin allocation (`analog_feel`) | B7 | LISTEN | 6 | `polystress`, `jangle` — ⚠ must be deterministic per voice index, never `rand()` |
| 3.29 | Non-monotonic formant amplitudes | B6 | LISTEN | 2 | `vowel` |
| ~~3.30~~ | ~~BLEP the pulse / PWM~~ **DROPPED by 0.1** — aliasing ≤ −53 dB even at A5, nulls intact at every pitch, PWM sweep no worse | B5 | — | — | measurement recorded in Phase 0 results |
| 3.31 | Saw PWM | C9 | LISTEN | 6 | `solina`, `juno` |
| 3.32 | Ping-pong and cross-fed delays | M5 | LISTEN | 6 | `dub`, `spacecho` |
| 3.33 | Peak level tapers with pitch | I6 | LISTEN | 6 | `piano` — pairs with §I5 |
| 3.34 | Bottom-octave sub-osc taper | I7 | LISTEN | 6 | `upright` — ⚠ **cuts against a shipped fix**; A/B carefully |
| 3.35 | Bow "surface sound" as a separate axis | F5 | LISTEN | 4 | `bowed` — reopens a range deliberately removed |
| 3.36 | Louder-goes-flat + per-period jitter for BOWED | F6b | LISTEN | 6 | `bowed` |
| 3.37 | Handoff fix #3: trade the fundamental away at forte | E5 | LISTEN | 6 | `brasspec` — pass/fail is **h8 ≥ h1** |
| 3.38 | Retaper the brassiness macro (h9 at 0.80 → h23 at 1.00) | E6 | LISTEN | 6 | `brass` |
| 3.39 | Wire `eng_p` weight/sub for the low brass bores | E7 | LISTEN | 4 | `brass` presets 4, 6 |
| 3.40 | Stretched partials for brass (reuse PIANO's allpass) | E8 | LISTEN | 6 | `brasspec` — folds into 2.3 |
| 3.41 | Amp key-tracking, negative for warmth | F8 | LISTEN | 5 | `solina` |
| 3.42 | Velocity → brightness, per slot | B9 | DESIGN | 5 | `20-instruments` — ⚠ string machines must **not** be velocity-sensitive |
| 3.43 | Pitch-to-CV: bandpass + slew, per Part 15 | C11 | VERIFY | 6 | `mictune` — aimed at a reproduced defect |
| 3.44 | Cutoff depth in octaves | B2b | DESIGN | 5 | `filterenv` — the second half of 2.1 |

---

## 7. Phase 4 — needs a decision first

| # | question | § | note |
|---|---|---|---|
| 4.1 | Build `subtractive.h`? | G | Rung 3 per 0016. Reid published exact values for ~8 patches; a cited voicing table is a feature *and* a regression test. Scope: brass, string machines, leads — **not** plucked or struck (he says outright subtractive can't reach them) |
| 4.2 | Shape of the trigger-policy surface | B3 | Header first (2.2), engine only if the header fails. Must let an instrument *declare* single vs multi |
| 4.3 | Attack-**level** on the mod envelopes | C6 | Cited in Parts 8, 25 and 29 — three families. Unlocks the spit-brass contour we keep failing |
| 4.4 | Does `harmonics` overblow, or get renamed? | K2 | Either implement the register jump (interval depends on bore topology) or rename the macro. Doc fixed in 0.3 either way |
| 4.5 | Exponential amp envelope, opt-in only | B4 | ⚠ Highest blast radius in the audit: every note of every cart, and every audio-gate baseline. The book barely argues for it. **Never a default flip** |

---

## 8. Drop candidates

Recording these so they stop costing attention. Each needs one line of agreement to close.

**Closed by measurement + ear, not by agreement — these are settled:**

| item | § | verdict |
|---|---|---|
| **Brass envelope (1.4)** | E10 | ❌ **DROPPED 2026-07-28.** All three of Reid's numbers lose on a waveguide; the shipped 1200 ms release *is* the bore's ring-down. Toggle removed, measurements + restore recipe kept in `brass.c`. [Write-up](#14-brass-amp-envelope--dropped-reid-loses-all-three-numbers-owners-ear-2026-07-28) |
| solina's middle wow rung (1.1) | F2 | ❌ **DROPPED 2026-07-28.** Measured indistinguishable from CLASSIC (centroid 2260 vs 2267 Hz) and no oracle can see a 0.16 Hz character change under a chord progression. Restore code kept in the cart |

**Still candidates, awaiting one line of agreement:**

| item | § | why drop |
|---|---|---|
| Paraphonic mode | C10 | Deliberately reproducing a *worse* behaviour. If `solina` can't make it earn its keep, close it |
| Hammond V-mode vibrato | L7 | Reid himself: "I never use any of my A100's 'V' settings" |
| Three-mode spring | M7 | Our dispersion already gives the audible signature; the 3-mode structure is the honest mechanism but likely inaudible |
| Two bridges (piano) | I8 | Listed for completeness only |
| Scanner amplitude modulation | L7 | Reid says ignore it |
| Percussion steals from sustain | L7 | One multiply, but he overlooks it too — do it only if 3.20 is already open |
| Duophony | M8 | One allocation rule; folds into 2.2, worth nothing alone |

---

## 9. Suggested first move

**Phase 0 in one sitting.** It is ten doc/comment fixes and five measurements, it cannot make anything
sound worse, and five of its rows change what we do next (0.1 decides §B5; 0.11 gates §E3; 0.13 gates
§K4b; 0.15 may merge two findings into one cause). Then **Phase 1**, which is seven baked A/B carts and
the first point where you have something to listen to.

Phase 2's keytracking (2.1) is the item I would push hardest for after that, on leverage alone: six
chapters asked for it independently and it is the prerequisite for §G.
