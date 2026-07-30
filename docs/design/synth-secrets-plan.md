# Synth Secrets — the build plan

STATUS: BUILDING — **Phase 0 DONE; Phase 1 DONE, 7 of 7 (2026-07-29)**: 1.1 solina, 1.2 tr808 cymbal and
1.3 the 808/909 snare **shipped by the owner's ear** (each keeping its old sound on a toggle); 1.5 the
layered piano **liked but kept opt-in** (it guards acceptance tests); **1.4 brass and 1.6 Hammond are
recorded DROPs** — Reid loses all three brass envelope numbers, and the Hammond item was mis-priced;
1.7 martenot built, awaiting an ear. The ordered work ledger
derived from [`synth-secrets-audit.md`](synth-secrets-audit.md). The audit is the *findings*; this is the
*doing*. **PHASE 1 IS COMPLETE (7 of 7)**: 1.1/1.2/1.3 shipped by ear, 1.5 liked-but-opt-in, 1.4 and 1.6
recorded DROPs, 1.7 built and awaiting an ear call. It also produced one engine FIX — `instrument_mode`
rejected two of its own indices, so two `piano` sliders had never worked. Nothing past Phase 1 is approved;
**Phase 2 (the four cross-cutting themes) is where the leverage is**, starting with keytracking.
**Phase 2 so far:** 2.1, 2.2 and **2.3(a) SHIPPED**. 2.3's premise failed on measurement — PIANO's
dispersion chain was inert and its stretched-tuning seam was cancelled a frame after note-on — and both are
now FIXED and shipped by the owner's ear (`MODE_PIANO_STIFF`, real stiff-string inharmonicity at B ≈ 1.1e-4,
plus the completed Railsback curve). **2.3(b) is unblocked.** §I4d open; 2.4 not started.

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
| 1.6 | Hammond: the sawtooth-ish and square-ish registrations (§L5) | LISTEN | 2 | `organ` | ❌ **DROPPED — not two rows after all** (owner, 2026-07-29). The detent table is in the engine; widening it remaps 13 carts. See §8 |
| 1.7 | Loudness→brightness by waveform morph; filter-as-gate (§F7) | LISTEN | 1 | `martenot` | ✅ **DONE — GATE IS THE DEFAULT** (owner's ear, 2026-07-29: *"the morph sounds a bit too clean/bright, I like gate"*). FILTER + MORPH stay on key **0**. Two bugs found on the way: MORPH **crackled** (→ the `click-check` oracle) and GATE **droned at rest** once promoted. See below |

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

### 1.7 martenot — ✅ GATE IS THE DEFAULT (owner's ear, 2026-07-29)

**The verdict:** *"the martenot morph sounds a bit too clean/bright I think? I think I like gate."* So
`MODE_GATE` ships as the default and both losers stay on key **0** — a verdict is a preference, not a
deletion.

**The ear agreed with the table, which is worth recording because it usually doesn't.** MORPH parks the
cutoff at 12 kHz and measured a centroid of **647/587/564 Hz** against FILTER's **284/282/377** at the same
three touche levels. "Too clean/bright" was visible in the numbers before anyone heard it. That is not an
argument for skipping the ear — 1.4's brass release measured *clean* and was still wrong — but it is a case
where the measurement carried real information about the verdict rather than just about the mechanism.

**⚠ PROMOTING A TOGGLE TO A DEFAULT IS ITS OWN CHANGE, and it exposed a bug the toggle had hidden.** GATE
holds `note_vol` constant, and this cart keeps one voice ringing the whole time, relying on `note_vol`
reaching 0 to be silent at rest. As a toggle you always arrived in GATE mid-gesture, so nobody saw it: as
the **default**, the cart droned from boot. Measured on an empty script, FILTER renders true silence
(peak −inf dBFS) and GATE rendered **−15.2 dBFS of continuous tone**. Not a nuance — a broken instrument.

Fix: `v = (intens > 0.0f) ? 6 : 0`. `intens` is already snapped to exactly 0 at rest, so it doubles as a
gesture flag, and gating the last millimetre is what the real Ondes does anyway — the touche is also the
on/off. Within the playing range the VCA still never moves, so the 30 dB claim is untouched. After the fix
the at-rest render is **bit-identical to FILTER's silence** (peak −inf), and the swell take has **no click
≥4x** the local step-rms (worst 3.0x, under the 3.3x control).

**A tooling note, because it will bite the next person:** `ab-render` shouts when two variants render
byte-identical audio, since that usually means the flag never reached the DSP. For the at-rest check
byte-identical **was the pass condition** (both modes must be silent). The warning is a heuristic about
*intent*, not a verdict — read what you asked for before believing it.

### 1.7 martenot — how it was built, Part 51's two tricks (built 2026-07-29)

Key **0** cycles three modes; the state shows above the touche. Both of Part 51's liftable moves land in
one cart, because `martenot` already has the wire and the pressure button that Reid's whole argument rests
on. Committed clips: `01-touche-swell` (what you listen to) and `02-steady-note` (what you measure).

| mode | what drives brightness | measured, steady note at three touche levels (0.20 / 0.50 / 0.90) |
|---|---|---|
| **filter** (as shipped) | a lowpass on `intens²` | peak −26.1 / −16.1 / −10.0 dBFS · centroid 284 / 282 / 377 Hz |
| **morph** | the WAVE dulls toward a triangle, cutoff parked wide open at 12 kHz | peak −28.3 / −16.5 / −10.0 · centroid 647 / 587 / 564 Hz |
| **gate** | volume held CONSTANT, the touche drives cutoff alone | peak **−40.4 / −21.6 / −10.2 / −10.0** across 0.05→0.90 |

**GATE is the one the numbers prove.** With `note_vol` pinned at a constant 6, the filter alone produces
**30 dB of range** — so it is genuinely "not only shaping the tone of the sound, it's also differentiating
one note from the next". Two implementation notes worth keeping: the floor and the curve are load-bearing
(my first attempt used `60 + intens²·5200`, which reaches 268 Hz at a light touch and so passes the 130 Hz
bottom note unattenuated — it gated *nothing*, and the measurement said so), and a 12 dB/oct `FILTER_LOW`
attenuates rather than mutes, so this approaches Reid's effect rather than reproducing it. His "at low
cutoff nothing passes" is a 24 dB/oct claim.

**MORPH is ear-only, and honestly so.** No oracle here can adjudicate it: the `brightness(HF/total)` proxy
reads **0.000** at every setting (this voice has almost nothing above the metric's threshold), and the
spectral centroid is dominated by the fundamental, so it *falls* as the note gets louder and is
noise-dominated at the quiet end. That is why the centroid column above runs the "wrong" way for both
filter and morph. The morph is doing what the code says it does — the wave demonstrably changes shape and
the renders differ — but whether it reads as Reid's blown-instrument behaviour is a listening call, like
1.1's middle rung.

**⚠ MORPH CRACKLED, and the write-up you are reading is why it shipped that way.** This paragraph used to
say the morph's wavetable was "quantised to 8 steps and rebuilt only when the step moves — inaudible as
stepping on a slow swell". That claim was **never measured**, and the owner's ear caught it (2026-07-29)
before the ear call on the *mode* had even been made. The lesson is not about wavetables: it is that a
sentence asserting inaudibility, sitting inside an otherwise well-measured item, reads as evidence. It was
the only unmeasured claim in item 1.7, and it was the one that was wrong.

**The cause, and there are TWO of them** — fixing either alone leaves the crackle. `wave_set` replaces the
table under a running oscillator, so at each swap the output jumps from `old[phase]` to `new[phase]`: a
one-sample discontinuity per step crossed, up to 16 per swell.

1. **The grid.** A step's jump scales as 1/`NDULL`, so 8 steps made each one large.
2. **The rate.** `intens` slews at up to 0.5/frame at note-on, so the step index could move *dozens* of
   steps in one frame — and a multi-step jump is exactly as big as a coarse-grid jump. This is why a finer
   grid alone still clicked at every note onset, which is the part that would have been easy to miss.

Fix: `NDULL` 8 → **64**, plus a **±2 steps/frame rate limit**. Measured with the new
[`click-check.js`](../../tools/click-check.js) on `01-touche-swell` (events whose first difference is ≥4x
the *local* step-rms; the control is MODE_FILTER, which never calls `wave_set` while a note runs):

| config | splice-like events | worst |
|---|---|---|
| control (MODE_FILTER) | 0 | 3.3x |
| NDULL 8, no limit — **as shipped** | **13** | **15.4x** |
| NDULL 64, no limit | 5 | 12.0x |
| NDULL 64, limit 4 | 2 | 7.0x |
| **NDULL 64, limit 2 — shipped now** | **1** | **4.1x** (0.6% of peak) |
| NDULL 64, limit 1 | 1 | 4.2x — saturated, so 2 is the knee |
| NDULL 8, limit 2 | 13 | 15.4x — the rate limit alone does nothing |

`MODE_FILTER` renders **byte-identical** after the fix, and `MODE_GATE` never enters the morph branch, so
the two modes still awaiting judgement are untouched. The finer grid is free: a rebuild writes 64 floats
and touches no bus DSP, so it is not the `lint-fx-frame` hazard (that one is crush/eq/tape reallocating
filters) — the lint passes and no `[sound]` queue warning appears at ~1.4 rebuilds/frame.

**The oracle is the real output of this bug: [`tools/click-check.js`](../../tools/click-check.js).** First
difference judged against the *local* step-rms, because a saw's flyback is a huge step and is not a click,
while an audible click is 6-20x its neighbourhood. Nothing we had could see this: `wav-envelope`'s
amplitude and centroid curves are identical in shape whether a transition is a clean ramp or a splice —
the same blind spot that made item 1.4's brass release call so hard ("no oracle here can tell a clean short
decay from a resonator being cut off"). Indexed in
[`guides/checks-and-oracles.md`](../guides/checks-and-oracles.md); run it after any mid-note table swap.

**Also fixed `ab-render` while here:** it only matched `static` file-scope values, and this cart declares
its globals bare (`int loud_mode = …` at column 0), which is a common cart style. It now accepts that form
too — anchored at **column 0 only**, since that is the one cheap way to tell a file-scope global from an
indented local, and substituting a local would silently measure nothing.

---

## 5. Phase 2 — the four cross-cutting themes

**This is the highest-leverage phase and the reason the ledger is ordered this way.** Each theme closes
between four and six separate audit findings at once. Doing them as themes rather than as scattered
per-engine items is the single biggest saving available.

| # | theme | closes | kind | rung | note |
|---|---|---|---|---|---|
| 2.1 | **Keyboard tracking** (§B2) | Parts 6, 23/24, 26, 46, 54 — six chapters | ✅ **SHIPPED (both halves)** 2026-07-29 | new enum + a call | The most-requested missing feature in the whole series. Part 26 pins the value: cutoff should track at **≈0.93/octave** ("190 percent" per octave, not 200). Two halves: `instrument_keytrack(slot, amount)`, and env/LFO cutoff depth in **octaves** rather than Hz |
| 2.2 | **Trigger policy** (§B3) | §L4, §K6, §H9, §M8, and four monosynth carts each hand-rolling it | ✅ **SHIPPED** 2026-07-29 (`mono.h` + `sh101`) | `mono.h` | §L4 vs §K6 is the argument: the **Hammond percussion must be single-trigger** and the **flute chiff must be multi-trigger**, and in both cases it decides whether the defining transient happens at all. So it is a property an *instrument declares*, not a cart convention. Start as a cart-land header per 0016 |
| 2.3(a) | **The premise failed, and both halves are now FIXED** (§I4b, §I4c, §I4d) | unblocks 2.3(b) | ✅ **SHIPPED 2026-07-30** — §I4c (stretch) + §I4b (`MODE_PIANO_STIFF`) by the owner's ear; §I4d open | Measuring before building (as this row told us to) found PIANO's dispersion chain **inert** (B ≈ 2e-6 where a grand is ~1e-4; GUITAR and PLUCK harmonic too) **and** its stretched-tuning seam working in the **treble only**. §I4c is now fixed and, more importantly, **tune-check now asserts the stretch instead of tolerating it**. Write-up below |
| 2.3(b) | **Level-dependent inharmonicity** (§E8, §H, §I4, §J8, §K8) | five families | LISTEN — **UNBLOCKED 2026-07-30** | 6 | One physical fact — partials sharpen with *amplitude* as well as pitch — modelled statically at best in five engines. Prototype on **one** engine (PIANO has the machinery), A/B, then decide whether to generalise. **The machinery turned out not to work**, so (a) came first — and now that PIANO has real, knob-controlled inharmonicity there is finally something to make level-dependent |
| 2.4 | **Coupling** (§E5, §H5, §I3, §M2) | four findings | DESIGN → 6 | engine | One architectural question with four faces: the brass bell that should fill the series natively, the guitar body with no return path, the piano tricord that does not exchange energy, and §M2's cheaper alternative (three parallel 1-4 ms delay lines *are* a body). **Do §M2's A/B first** — it may answer all four cheaply |

**Sequencing note:** 2.1 and 2.2 are prerequisites for a lot of Phase 3 and for §G, so they come first
even though they are the largest items here. 2.4 should start with a measurement, not a build.

### 2.1(a) `instrument_keytrack()` — SHIPPED 2026-07-29

**The engine can follow the keyboard now.** One multiply at note-on:
`cutoff *= 2^(amount * (midi - 60) / 12)`. `amount` 0 = absolute Hz (the default), 1 = true 1V/oct,
0.93 = Reid's musically-nice "190 percent per octave". With tracking on, the cutoff passed to
`instrument_filter()` is the value **at C4** and scales from there.

**Zero risk, and measured rather than asserted:** `amount` defaults to 0, which makes the multiply an exact
1.0, and `22-filter`, `filterenv` and `moog` all render **byte-identical** against the pre-change engine.
Gates: soundcheck silent, `tune-check` no new drift, 569/569 carts compile.

**The acceptance test is a number, not an ear** — which is why this was a VERIFY item. Spectral centroid of
a resonant ladder (cutoff 400 Hz at C4, res 15) across four octaves:

| | C3 | C4 | C5 | C6 |
|---|---|---|---|---|
| **tracking 0** (fixed Hz) | 391 Hz | 438 | 511 | 686 |
| **tracking 1** (follows) | 259 | 438 | **800** | **1535** |

Fixed cutoff barely moves the spectrum across the whole keyboard — that *is* §B2's complaint, quantified: a
patch is voiced correctly in one register and wrong everywhere else. With tracking the centroid lands on the
intended 200/400/800/1600 cutoffs.

**Two decisions worth remembering** (agreed before building): `amount = 1` means **true 1V/oct**, not
Reid's 0.93, because exactly 1.0 is what lets a self-oscillating filter be played in tune — 0.93 is
documented as the taste value instead. And keytrack applies at **note-on only**; `note_cutoff()` stays
absolute, so a live sweep (the martenot wah, the brass mute) means the same thing at every pitch.

**New cart: [`keytrack`](../../tools/carts/keytrack.c)**, because this needed to be *seen* as well as heard.
It runs a phrase four octaves up and back while plotting the note pitch as dots and the cutoff as a line:
at tracking 0 the line is dead **flat** while the notes climb away from it, and at 1.00 the two run
**parallel**. That is the whole feature in one glance. Buttons 1/2/3 pick the three amounts, and the
resonance control is there for Reid's Part 63 Juno trick (crank it and the filter's own whistle becomes a
playable voice). Pair: `build/ab/7-keytrack-{OFF-fixed-Hz,ON-follows}.wav`.

**Two cart-authoring traps hit while writing it**, both worth the CLAUDE.md list: `SCALE` is a **`-D`
compile flag** (the window scale factor), so `static const int SCALE[30]` expands to `int 4[30]` and fails
with a syntax error that points at the array, not the name; and `S` is `#define`d by the starter cart.

### 2.1(b) `ENV_CUTOFF_OCT` / `LFO_CUTOFF_OCT` — SHIPPED 2026-07-29

**The same complaint one level up: a filter env's DEPTH has units too, and Hz is the wrong one.** Half (a)
made the cutoff follow the note; the sweep on top of it still didn't. `ENV_CUTOFF` asks for Hz, so one
`+1200 Hz` setting is **three octaves** of sweep on a 200 Hz bass note and **under one** two octaves up —
the patch's defining "pew" quietly evaporates as you play up the keyboard, which is §B2's complaint again
with the base cutoff already fixed.

Three new destinations, not a redefinition — **59 carts** use the Hz forms and they are untouched:

| dest | reaches | unit |
|---|---|---|
| `ENV_CUTOFF_OCT` (7) | `instrument_env` / `note_env` | octaves, bipolar (2 = the peak opens two octaves up) |
| `LFO_CUTOFF_OCT` (9) | `instrument_lfo` / `note_lfo` | octaves (depth 1 = ±1 octave) |
| `LFO_CUTOFF_OCT` | `instrument_follow` / `note_follow` | octaves — an auto-wah whose THROW is pitch-independent |

**One decision worth remembering: octave modulation MULTIPLIES, and it is applied last.** The additive Hz
terms sum into `cutoff` exactly as before, then a separate `cutoff_mul` (which starts at an exact 1.0)
multiplies once at the end. That is the shape `pitch_mul` already had, and it buys order-independence: a
patch mixing a Hz LFO with an octave env means the same thing regardless of which mod source ran first.
It also composes cleanly with 2.1(a) — keytrack the base, sweep in octaves, and the patch is
pitch-independent end to end, which is the pair of settings a real synth's KYBD + ENV AMOUNT gives you.

**A VERIFY item again, and the number is the whole argument.** Attack spectral centroid (first 80 ms) of a
keytracked resonant ladder — base cutoff 200/400/800/1600 Hz — with the sweep set to **+2 octaves at C4 in
both units**, so only pitch separates them:

| | C3 | C4 | C5 | C6 | ratio per octave |
|---|---|---|---|---|---|
| **depth in Hz** (`ENV_CUTOFF` 1200) | 604 Hz | 805 | 1163 | 1919 | 1.33 · 1.44 · 1.65 |
| **depth in octaves** (`ENV_CUTOFF_OCT` 2.0) | 352 | 696 | 1379 | 2746 | **1.98 · 1.98 · 1.99** |

The octave form doubles per octave to within 1%: the sweep is the same musical gesture everywhere. The Hz
form doesn't merely flatten out — it **inverts the contour**, giving the bass note the biggest sweep (604
against 352 at C3, because 1200 Hz is 3 octaves over a 200 Hz base) and the top note the smallest (1919
against 2746). One setting, five different patches.

**All three destinations were reach-tested, not just the one that produced the table.** The same modulator
routed to cutoff twice — once in Hz, once in octaves, sized to agree at C4 — through the LFO, the mod-env
and the follower: `ab-render --set mode=0,1,2,3,4,5` gives **six distinct shas**, so no branch is silently
dead. (Worth doing deliberately: a wired-but-unreachable control is exactly the class of bug Phase 1 found
in `instrument_mode`, where two `piano` sliders had never worked.) The OCT variant is the brighter of each
pair, as it must be, since it keeps sweeping in the top octaves where the Hz form has run out.

**Reproduce it** — four isolated notes, an octave apart, one second apart so no tail pollutes the next
(`node tools/ab-render.js octprobe --set unit=0,1 --frames 300 --keep`, then
`node tools/wav-envelope.js <wav> --from 1.00 --to 1.08` at t = 0/1/2/3). The probe is a ruler, not a cart,
so it is not committed; this is the whole of it:

```c
#include "studio.h"
#define SL 5
static int unit = 1;                       // 0 = Hz env, 1 = OCT env (ab-render flips this)
static const int NOTES[4] = { 48, 60, 72, 84 };
static int fired = 0, fr = 0;
void init(void) {
    instrument(SL, INSTR_SAW, 2, 0, 7, 90);
    instrument_filter(SL, FILTER_LADDER, 400, 12);
    instrument_keytrack(SL, 1.0f);         // base cutoff follows: 200/400/800/1600 Hz
    if (unit == 0) instrument_env(SL, 0, ENV_CUTOFF,     0, 200, 1200.0f);  // +2 oct at C4 ONLY
    else           instrument_env(SL, 0, ENV_CUTOFF_OCT, 0, 200,    2.0f);  // +2 oct at EVERY pitch
}
void update(void) { if (fired < 4 && fr == fired * 60) { hit(NOTES[fired], SL, 6, 300); fired++; } fr++; }
void draw(void) { cls(CLR_BLACK); }
```

**Zero risk, measured the same way as (a):** `filterenv`, `moog`, `22-filter` and `tb303` render
**byte-identical** to the pre-change engine (`c62132223aba` / `59a6466ca284` / `f6401707fd05` /
`6106d8552827`). `keytrack` did too, checked *before* it was extended to use the new dests — it cannot be
re-checked now, which is the point of doing that render first. Gates: soundcheck silent, `tune-check` no new
drift (same 3 waived residuals), `build-all` 570/570, `ui-audit` clean on `keytrack` (it caught two real
bugs first: the footer ran 372 px wide on a 320 px screen, and the status line sat under the graph's `50`
axis label).

**Tool addition that made that measurement honest: `DE_RUNTIME_DIR`.** `make-cart.js` (so `play.js` too)
now takes an engine tree from the environment. Copy `runtime/`, restore the touched file from git, and the
same cart with the same harness args renders against **both** engines — so "byte-identical" is a
measurement rather than an argument, without ever running a destructive `git checkout` on a hot shared
header. The control that proves the harness works: a cart using `ENV_CUTOFF_OCT` **fails to compile**
against the baseline tree.

**The cart is [`keytrack`](../../tools/carts/keytrack.c) again, extended rather than cloned**, because this
is the same finding and the same graph: row **4/5/6** picks the sweep unit, and the graph gains an orange
**sweep-top** line plus a vertical tick per note. On a log axis that tick's LENGTH *is* the depth in
octaves — so "sweep in Hz" visibly shrinks as the phrase climbs while "in OCTAVES" holds its length. The
tour is `3` then `5` versus `6`. Both halves on = the two lines run parallel to the notes.

**The A/B pair is committed** as `tools/clips/keytrack/0{1,2}-sweep-{hz,oct}.script`, and each header says
what it does **and does not** prove: pressing `3` first is load-bearing (with tracking off the two units
are identical, so a seed without it would prove nothing), and the pair is the **ear** check only — the
phrase gates 420 ms notes every 200 ms, so notes overlap and a per-note spectral region is polluted by the
two before it (measured: non-monotonic, and both modes read the same at the first C). That is why the
acceptance table above comes from the isolated-note probe instead. Renders:
`build/ab/8b-sweep-{Hz-collapses,OCT-holds}.wav` — the Hz take is **0.9 dB hotter** overall, which is
exactly the kind of level difference §1 says to name before anyone listens.

---

### 2.2 `mono.h` — note priority + trigger policy — SHIPPED 2026-07-29

**The largest item in the audit, and it needed no engine change at all.** Reid gives Part 18 entirely to
the two questions a one-voice keyboard must answer — *which* held note sounds, and whether a press
*restarts the envelope* — and his claim is that this, not the oscillators or the filter, is what decides
whether a synth feels playable: "my playing sounded punchier on the Odyssey … The answer lay in the
engineering within the instruments." Four priority schemes crossed with the triggering variants is what he
counts as "at least 24 keyboard characteristics".

[`runtime/mono.h`](../../runtime/mono.h) is a `Mono` struct (the held-key stack, in press order) plus
`mono_press`/`mono_release`, which return what the cart should *do*: `MONO_START`, `MONO_GLIDE`,
`MONO_RETRIG` or `MONO_STOP`. Priority is `MONO_LAST`/`LOW`/`HIGH`/`FIRST`; triggering is `MONO_SINGLE`
(only onto an empty keyboard — Figure 8, "exactly how a Minimoog works"), `MONO_MULTI` (every press,
Figure 9, the ARP) or `MONO_ANY` (Figure 11 — any transition, so a *release* that hands the voice over
re-attacks too). Rung 3 per ADR-0016: it is pure bookkeeping, so it gets no engine surface.

**`sh101` drives it**, with PRIO and TRIG switches in the free space under the TUNE knob. Both default to
what the cart already did, and the shipped defaults render **byte-identical** to the pre-change cart on an
overlapping-note seed (`6b67a46db046`).

**THE FINDING, and it is better than the feature: the SH-101's PORTAMENTO switch is secretly a TRIGGER
switch.** The cart's own answer conflates Reid's two axes — gliding the pitch and not retriggering the
envelope are the same code path (`glide_to` vs `start_note`) — so moving PORTA silently moves you between
his policies. Byte-identical renders prove the mapping instead of arguing it:

| the 101's own controls | is byte-identical to |
|---|---|
| PORTA **OFF** (`ddb9d398da39`) | Reid's **ANY** at PORTA OFF (`ddb9d398da39`) |
| PORTA **AUTO** / **ON** (`3ad0f95fe278`) | Reid's **SINGLE** at PORTA AUTO (`3ad0f95fe278`) |
| — nothing — | Reid's **MULTI** (`16e347617178`) |

So the real machine can reach only two of the three characters, and **MULTI is unreachable on its own
panel**: re-attack on every press but glide on a hand-over. That is a concrete, measured answer to "what
does conflating the two axes cost you", which is exactly what §B3 asked. (AUTO and ON coincide *for this
seed* because every press in it is legato; they differ only when the voice is sounding with no key held.)

**⚠ The first version of that table was measured at MISMATCHED glide settings and the equivalences have to
be read per PORTA position** — which only became true after fixing the bug below. Compare the trigger
policies at the *same* PORTA setting or the comparison is meaningless.

**A bug the owner heard within minutes of the commit, and the fix is the item's own thesis.** I split
Reid's two axes in the *decision* (`mono.h` owns re-attack-vs-legato) but left the glide TIME coupled to a
switch whose OFF position I then ignored: classic mode never reaches `glide_to` with PORTA OFF, but
SINGLE/MULTI do, so they glided **125 ms with the switch reading OFF**. The owner's words were "some kind
of detuned arpy character", and the pitch track agrees exactly — mid-slide the fundamental sat at **137.7
then 134.1 Hz** between D3 (146.8) and C3 (130.8), i.e. audibly out of tune for over 100 ms. `glide_to` now
reads `(porta_mode == 1) ? 0 : f_porta(porta_v)`, so OFF means 0 ms and a legato move is an instant pitch
change with no new attack — legato *without* portamento, which is the thing SINGLE is supposed to
demonstrate. After the fix the same windows read a clean 146.6 → 131.0 with no intermediate pitch, the
defaults are still byte-identical (`ddb9d398da39`), and SINGLE now renders the same whatever the glide knob
says. **The lesson: separating two conflated axes is not done when the DECISION splits, only when every
consumer of both axes splits too.** The glide knob was still reading from the old joint.

**Not a bug, and worth writing down because it surprised the owner too:** this cart's default voice is SAW
at 1.0 plus the SUB oscillator (a square, exactly −1 octave) at 0.75, with `vmod_v` 0 (no LFO→pitch), TUNE
centred and no per-source detune anywhere. So the "thick, almost two-note" quality is a genuine octave
stack, not detuning. The pitch tracker makes it objective: on the same take it locks to **146.6 Hz** (the
saw) at D3 and to **98.0 Hz** (the sub) at G3, flipping between the two components depending on which
dominates — a good measurement of how present that sub actually is at 0.75.

**The specs are the point of the header, not an afterthought.** Part 18 is already a test suite: the same
played sequence through four priorities gives four different pitch sequences, so `mono_selfcheck()`
asserts that table outright, plus the attack COUNT per trigger policy (SINGLE 1, MULTI 3, ANY 4 over the
same six events — if two of those ever come out equal, the switch has stopped meaning anything). 47
assertions via `node tools/spec.js sh101`, and **every one was watched failing** before it was kept: four
mutations (LOW inverted, MULTI not retriggering a losing press, ANY not re-attacking on hand-over, FIRST
behaving like LAST) turn 4/1/1/3 of them red respectively.

**I shipped a decorative switch and the oracle caught it.** The first cut kept a `prio_sel` beside
`mono.prio` and synced them only inside the tap handler, so `init()` forced LAST back and **all four
priorities rendered byte-identical** — a switch that moved a label and nothing else. `ab-render`'s
"byte-identical means the flag never reached the DSP" warning is what flagged it. `mono.prio` is now the
switch itself, and a spec assertion drives the panel and checks the policy followed, which is the
assertion that would have caught it.

**Two traps in my own measuring, both of which briefly produced confident wrong conclusions:**
- **An ascending test sequence cannot see two of the four schemes.** Play 48, 50, 52 and "last pressed"
  IS "highest held", so LAST ≡ HIGH; likewise FIRST ≡ LOW. The committed seed is deliberately
  non-monotonic (mid, low, high) and releases middle-first so the hand-over path fires. `mono_selfcheck`
  uses the same shape for the same reason.
- **`seq 1 0` prints "1 0" on macOS**, not nothing. A `for i in $(seq 1 $n)` loop generating "n taps"
  scripts therefore emitted TWO taps at n=0, so the baseline was sitting on HIGH, every variant matched,
  and I nearly wrote up "the priority switch does not reach the audio". A `watch()` trace of `mono.prio`
  caught it. Both seeds are now committed files in `tools/clips/sh101/` precisely so no loop can lie about
  the click frames again. (Sibling of the zsh word-splitting note in CLAUDE.md — the shell keeps finding
  new ways to invalidate a measurement.)

**A collision worth budgeting for on any instrument cart:** `spec.h` declares `key_down`/`key_up`, which
`sh101` had owned since it was written, so it could not host a spec until they were renamed to
`sh_key_down`/`sh_key_up`. It was already documented in
[`spec-harness.md`](spec-harness.md#reserved-names--step-is-the-one-that-bites) next to the `step` trap —
reading that first would have saved the detour. Now recorded there as a cart *family* risk, since those are
the natural names in every keyboard cart.

**Still open, deliberately:** `tb303`, `acidrack` and `moog` still hand-roll their own answers. Converting
them is one cart at a time, each with its own byte-identity check, and each is where the §L4-vs-§K6
argument (Hammond percussion must be SINGLE, flute chiff must be MULTI) will decide whether this ever
earns promotion into `sound.h` as a property an instrument *declares*. The header is the rung below that,
and it holds for now.

### The premise failed: three defects found by measuring first (2026-07-29)

*Item 2.3(a) in the table above. The heading carries no dotted item number on purpose: `handoff.js`
normalises `2.3(a)` to `2-3-a` while GitHub renders it `23a`, so no single anchor satisfies both and the
Resume-at link silently breaks.*

**This row said "prototype on PIANO, which already has the machinery". The machinery does not work.**
Item 2.3 asks for inharmonicity that responds to level. Before building that, the honest first question is
how much inharmonicity PIANO has *now* — and the answer is **almost none**. Two independent defects, both
of the same shape: **a value the engine computes correctly and then never lets reach the sound.**

**Neither was findable with the audio gates we had**, which is the reason this took a new oracle:
`tune-check` measures the *fundamental*, `harmonic-spec` measures partial *levels* at frequencies it
*assumes*. A partial that has drifted 30 cents sharp reads as slightly quieter and nothing goes red. So
this began by building **[`tools/inharm-spec.js`](../../tools/inharm-spec.js)** — partial frequencies in
cents against the ideal `n·f0`, plus the fitted stiff-string `B` of `f_n = n·f0·√(1+Bn²)` and the residual
from that law. Same blind-spot family as the one `click-check.js` was built for in 1.4.

**`--check` is load-bearing here and was written before the conclusion.** A tool that reports "no
inharmonicity" and a tool that is silently broken print the identical table, so the null result means
nothing until the tool is shown to see the effect. It synthesises stiff-string spectra at known `B` and
recovers them: `B=0 → 5.8e-11`, `1e-5 → 9.96e-6`, `1e-4 → 9.96e-5`, `5e-4 → 4.98e-4`, with h16 read to
within 0.5¢. **Only then** was the PIANO reading trustworthy.

#### §I4b — the dispersion chain is inert

INSTR_PIANO, grand voicing, C3, cents sharp of the ideal `n·f0`:

| | h2 | h4 | h6 | h8 | h10 | h12 | h16 | fitted B | resid |
|---|---|---|---|---|---|---|---|---|---|
| **vel 1** (pp) | +0.4 | +0.4 | +0.2 | +0.3 | +0.2 | +0.2 | +0.3 | 1.9e-6 | 0.2¢ |
| **vel 7** (ff) | +0.4 | +0.3 | +0.2 | +0.3 | +0.3 | +0.2 | +0.2 | 1.8e-6 | 0.2¢ |

A real middle-register grand is **B ≈ 1e-4**, which would put h16 about **+22¢** sharp. We measure
**+0.2¢** and **B ≈ 2e-6** — a factor of ~50 short, i.e. the partials are harmonic to within the width of
the measurement. It is not a piano-only result: **GUITAR B = −3.1e-7**, **PLUCK B = 1.6e-8**, PIANO
*dulcimer* −1.5e-6, PIANO *celesta* 1.4e-5 but with a **12.7¢ residual** (noise, not a stiff-string law).
So audit §H's "stretched harmonics" note has the same status. So does the ✅ in the docstring.

**Cause, and it is arithmetic rather than a dead code path.** `sound_piano_start` sets
`B = stiff²·0.015` then `pt = B·(i+1)·freq/SR` and `c = (1−pt)/(1+pt)`
([`sound.h:4687-4696`](../../runtime/sound.h)). At C3 on the grand (`stiff` 0.25) that is
**pt = 2.6e-6**, so **c = 0.9999948** — a first-order allpass with `c → 1` is the *identity*, so the chain
passes the string through untouched. Meaningful dispersion needs `pt` in the **0.1–0.5** region, four to
five orders of magnitude away, and no musical pitch closes that gap because the term is scaled by
`freq/SR` (≤0.05 across the whole keyboard). Confirmed by sweep: scaling `pt` by **1, 30, 100, 300, 1000,
3000×** moves the measured B not at all (it stays ~1e-8, i.e. noise). The `if (pt > 0.9f)` clamp on the
next line is the tell — it guards a ceiling the expression cannot reach.

#### §I4c — the stretched-tuning seam is cancelled in the BASS only, and a clamp is what saves the treble

Chasing §I4b turned up a second, independent defect in the same engine. `piano_stretch_freq` (the
Feynman/Railsback seam, `PIANO_STRETCH_K`) computes correctly — instrumenting the engine shows a C3
nominal 130.813 Hz arriving as **130.6617 Hz**, exactly the −2.0¢ that `K=2` specifies. It then does not
survive the frame:

```
[pnratio] v->freq=130.6626  pn_initf=130.6617  ratio=1.000007  len=337  effLen=336.998   ← frame 1
[pnratio] v->freq=130.8115  pn_initf=130.6617  ratio=1.001146  len=337  effLen=336.614   ← frame 2 onward
```

Note-on sets `v->freq = v->freq_target = sound_midi_to_freq(midi)`
([`:5229`](../../runtime/sound.h)). `sound_piano_start` then overwrites **`v->freq`** with the stretched
value — and **not `v->freq_target`** ([`:4633`](../../runtime/sound.h)). So the per-frame glide slew
`v->freq += (v->freq_target − v->freq) * v->freq_slew` ([`:6398`](../../runtime/sound.h)) pulls it back to
the nominal within one frame, while `pn_initf` keeps the stretched value. `ratio = v->freq / pn_initf` is
then permanently off by exactly the stretch, and `effLen = len/ratio` divides it back out.

**But only in one direction, and that is the whole character of this bug.** The next line clamps:
`if (effLen > (float)len) effLen = (float)len;` ([`:4735`](../../runtime/sound.h)).

- **Treble** (`soct > 0`): the stretch is *sharp*, so `stretched > nominal`, `ratio < 1`, and `effLen`
  would exceed `len` — **the clamp catches it, the cancellation never happens, and the stretch survives
  intact.**
- **Bass** (`soct < 0`): the stretch is *flat*, so `ratio > 1` and `effLen < len` — unclamped, so the
  cancellation applies and **the stretch is destroyed.**

Sweeping `PIANO_STRETCH_K` at one note in each half proves it. At **C3** (bass, design predicts `−K` cents):

| `PIANO_STRETCH_K` | 0 | 2 | 24 | 48 | 120 |
|---|---|---|---|---|---|
| predicted | 0¢ | −2¢ | −24¢ | −48¢ | −120¢ |
| **measured** | +0.22¢ | +0.11¢ | −0.92¢ | −1.99¢ | **−5.20¢** |

about **1/23** of the intended effect. At **C5** (treble, predicts `+K` cents):

| `PIANO_STRETCH_K` | 0 | 2 | 24 | 48 | 120 |
|---|---|---|---|---|---|
| predicted | 0¢ | +2¢ | +24¢ | +48¢ | +120¢ |
| **measured** | +0.91¢ | +2.92¢ | +24.95¢ | +48.98¢ | **+120.93¢** |

**tracking 1:1, every value the prediction plus a constant 0.91¢.** So PIANO has been playing **half a
Railsback curve** all along: the treble stretch has always worked, the bass stretch has never existed.

**The one-line fix is `v->freq_target = freq` alongside the existing write-back.** The comment on that
line — *"write back so per-sample pitch tracking (ratio = f/pn_initf) stays consistent"* — states the
intent exactly; it just missed the second field.

**A methodology note against myself, because it nearly shipped as a wrong finding.** The first version of
this section said the stretch was cancelled outright and the pitch was equal temperament. That came from
measuring **one note** (C3) and generalising to the engine. The tune-check table was on screen the whole
time showing the actual shape — monotonic sharp, *no flat bass* — and that missing bass half was the clue,
read at the time as uninteresting residue. **Measure both halves of a signed curve before describing it.**

**Why it survived so long.** `sound.h:4611-4616` says *"It intentionally departs from ET, so tune-check
flags PIANO by design — that IS the stretch, not a bug."* **tune-check does not flag PIANO.** It passes.
**A source comment pre-emptively explained away the one gate that would have caught this**, so a green
check read as confirmation instead of as a contradiction. When a comment says a gate is *expected* to be
red, verify that it IS red — a passing gate under a "this will fail by design" comment is a finding.

**And §I4b and §I4c hid each other.** The stretch exists (per
[`piano-engine.md`](piano-engine.md) §6) so that the stiff string's sharp partials agree across notes —
*"that clash is what makes plain dispersion read as sour metal"*. With no dispersion there was no clash to
hear, and with only half a stretch there was nothing holding the dispersion honest.

#### §I4d — the loop has its own uncompensated tuning offset (new, smaller, still open)

Measuring the variants exposed a third thing. With the mechanism fixed and `K=0` (no stretch at all),
PIANO is **not** in equal temperament: on the `--engine PIANO --range 45-79` view it runs **+1.3¢ at A2
rising to +4.0¢ at G5**. That is the KS loop's own delay bookkeeping — the averaging filter
`(cur+nxt)*0.5` contributes about half a sample, the brightness blend moves it, and none of it is
subtracted from `len`, so a fixed sub-sample error becomes a larger fraction as `len` shrinks up the
keyboard. Small, real, and a separate row from §I4b/§I4c.

**It is window-dependent, which is worth knowing before anyone blesses a number.** The *same* engine at
the *same* macros reads **+0.1/+0.4/+0.6¢** on tune-check's default sweep and **+1.3…+4.0¢** in recipe
mode. Both are correct: PIANO's pitch drifts *within* a note because the brightness bloom moves `ksb`,
which moves the loop's effective delay, and the two modes average over different windows. So a residual
baseline is only meaningful per measurement window — which is why the gate below blesses only the default
sweep's own notes and lets recipe mode report without gating.

#### What needs a call before 2.3(b) can start

Measured tuning curves for the three candidates, cents off equal temperament (PIANO grand, via
`tune-check --engine PIANO`):

| note | (1) today | (2) fixed, K=0 | (3) fixed, K=2 | (3)−(1) |
|---|---|---|---|---|
| A2 | +0.8¢ | +1.3¢ | **−1.8¢** | −2.6¢ |
| C#3 | +1.2¢ | +1.5¢ | −0.2¢ | −1.4¢ |
| G3 | +1.8¢ | +1.9¢ | +1.6¢ | −0.2¢ |
| B3 | +2.2¢ | +2.2¢ | +2.2¢ | **+0.0¢** |
| A4 | +4.4¢ | +3.3¢ | +4.4¢ | +0.0¢ |
| G5 | +9.1¢ | +4.0¢ | +9.1¢ | +0.0¢ |

**Read the last column: the fix changes nothing at all above B3, and only the bass below it.** Variant (3)
is the real signed-quadratic curve — flat in the bass, crossing zero near C#3–D#3, sharp in the treble.

**Recommendation, and it is the opposite of what the first draft of this section said: take (3), the
one-line fix at the current `K=2`.** The reasoning that argued against it — *"a Railsback stretch is
compensation for inharmonicity, so do not apply it to a string whose partials are harmonic (§I4b)"* —
turns out to be moot, because **we already ship the sharp treble half of that stretch and always have.**
The engine is committed to Railsback in the register where it is most audible; the fix simply stops it
being asymmetric. Doing nothing is not the conservative option here, it is the inconsistent one.

Caveats stated plainly: there is **no byte-identical option** (today's bass tuning is itself an artifact),
the change is **bass-only and one-directional** (up to −2.6¢ at A2, growing to about −8¢ by A1), and it is
**below the ~5–10¢ just-noticeable difference for a melodic interval** — so it reads not as "the piano is
retuned" but as bass notes sitting slightly differently against other instruments. §I4d is untouched by
all three and can be fixed separately.

### §I4c SHIPPED, and the gate that now asserts it (2026-07-30)

**Taken: option (3), the one-line fix at `K=2`.** In `sound_piano_start`, alongside the existing
write-back:

```c
v->freq = freq;            // existing: keeps ratio = f/pn_initf consistent
v->freq_target = freq;     // NEW: ...and stops the per-frame glide slew dragging it back to nominal
```

`PIANO_STRETCH_K` is unchanged at `2.0f`. That was deliberate: the constant was chosen while the mechanism
was broken, but the treble half has been sounding at `K=2` the whole time, so keeping it makes the bass
agree with the treble that already ships rather than introducing a strength nobody has heard.

**The gate came first, and this is the part worth keeping.** An ET-only check could not see this feature at
all: the deviation the stretch creates is smaller than `WARN_CENTS`, so PIANO printed ✓ with no stretch,
with half a stretch, and with the full one. So `tune-check` grew an `INTENDED_DETUNE` table — engines that
are *supposed* to leave equal temperament declare their intended curve, and the gate measures the
**residual against that intent** instead of the deviation from ET. `K` is parsed out of `sound.h` so there
is one source of truth and the check follows the constant if anyone retunes it.

Proven to fail on the bug it was written for, per §1's rule (this was the first version, since
superseded by the differential below):

| | A2 (the sentinel) | A3 | A4 |
|---|---|---|---|
| intended detune | −3.1¢ | −0.1¢ | +1.1¢ |
| **without the fix** — measured vs ET | **+0.0¢** | +0.4¢ | +1.8¢ |
| **without the fix** — residual | **+3.2¢ ✗ OFF INTENT** | +0.5¢ | +0.6¢ |
| **with the fix** — measured vs ET | −3.0¢ | +0.3¢ | +1.8¢ |
| **with the fix** — residual | **+0.1¢ ✓** | +0.4¢ | +0.6¢ |

`--quiet` exits **1** without the fix and **0** with it. Look at the second row: without the fix A2
measures **+0.0¢ against equal temperament**, which is why the old check called it perfect — it was
perfectly in tune and perfectly wrong. Residual tolerance is a tight ±1.5¢ because the defect it exists to
catch is only ~3¢, and the blessed residual (§I4d, the loop's own offset) is per-measurement-window.

Gates: soundcheck silent, `tune-check --quiet` 0, **570/570 carts compile**. The change is confined to
`sound_piano_start` by construction, so no non-PIANO engine can be affected; the other engines' sweep rows
are unchanged. Ten carts use `INSTR_PIANO`, and the audible register is the bottom two octaves — note that
the `piano` cart itself is a C4–C5 keybed, so **the fix is inaudible there**; the ear pair below is a
deliberate A1–A4 arpeggio plus a low stack instead.

```bash
afplay build/ab/piano-stretch-OFF-bass-flat-missing.wav   # before: bass sits at ET
afplay build/ab/piano-stretch-ON-full-railsback.wav       # after:  bass flattens into the curve
```

Listen to the first three notes (A1, E2, A2) and the low stack at the end. Bias to declare: the pair is
level-matched to within ~1 dB (peak −12.9 vs −13.4 dBFS, rms −38.0 vs −37.0), the rms difference being the
low stack beating differently, which is the change itself.

**Also deleted: the comment that caused the blindness.** `sound.h` asserted *"it departs from ET, so
tune-check flags PIANO by design — that IS the stretch, not a bug."* It never did. That sentence is
replaced with what actually happened, so the next reader does not re-derive the same false comfort.

### The sharper check: `MODE_PIANO_STRETCH` and a differential gate (2026-07-30)

The blessed-baseline gate above worked but had a flaw worth removing: the number it blesses is §I4d, the
loop's own delay error, which **is not constant** — it drifts within a note as the brightness bloom moves
`ksb`, so the same engine at the same macros reads +0.1/+0.4/+0.6¢ on the default sweep and +1.3…+4.0¢
in recipe mode. Any blessed value is therefore per-measurement-window and needs re-blessing whenever a
window moves. And the deeper problem: a compile-time `#define` **cannot be A/B'd by a gate at all**, which
is part of why §I4c hid for months.

**So the stretch became a runtime parameter: `MODE_PIANO_STRETCH` (`instrument_mode` idx 4).** `0.5` is
the engine's own curve, `0` is plain equal temperament, `1` is double. The default is **byte-identical** to
before it was a parameter (`PIANO_STRETCH_K * (0.5f * 2.0f)` is exactly `2.0f`), verified by sha against a
pre-change render. It also earns its place as a feature rather than a test hook: a cart playing the piano
in unison with a fixed-pitch sampled or chiptune part wants the stretch off, where a stretched bass reads
as sour rather than rich.

**The gate now renders PIANO twice in one pass** — normally, and with the stretch forced off (`ET_ENTRY`
in `tunecheck.c`) — and asserts the **difference** against the intended curve. The difference cancels
every constant error the loop carries, §I4d included, so there is nothing to bless:

| | A2 | A3 | A4 |
|---|---|---|---|
| intended curve | −3.13¢ | −0.13¢ | +1.12¢ |
| **measured difference** | −3.20¢ | −0.10¢ | +1.20¢ |
| off by | **−0.07¢** | +0.03¢ | +0.08¢ |

Within 0.08¢ everywhere, against a ±0.6¢ tolerance, with no blessed numbers. Compare the old approach's
±1.5¢ tolerance around three hand-blessed values. And it still fails on §I4c, now with a **localised**
diagnosis rather than a bare "A2 is off": removing the fix gives A2 off by +2.93¢ ✗, A3 +0.13¢, A4 +0.08¢,
which reads directly as *the bass half of the curve is missing and the treble half is fine*.

**One trap found while building it, worth the CLAUDE.md-grade warning.** The `eng_p` bound exists
**twice** — in `instrument_mode()` (the public setter) and again in the `SR_ENG_TUNE` request handler.
Widening only the setter is a **silent no-op**: it accepts the value, queues it, and the handler drops it.
The first run of the new gate reported a 0¢ stretch at every note, including the treble where the stretch
demonstrably works, which is what gave it away. This is the *same* failure mode the setter's own comment
documents for idx 2 and 3 ("both were silently dropped HERE, in the setter") one layer deeper, and that
comment's instruction to "widen `eng_p[]` AND this bound together" undercounted the bounds. A sixth aux
param needs **three** edits: the array width, the handler bound, and the setter bound. Both sites now say
so. Also: adding a sweep entry means raising `renderSweep`'s frame count, or the *last* entry silently
truncates — and the last entry is now the differential pass.

Gates: soundcheck silent, `tune-check --quiet` 0, `dc-check` 0, recipe mode still runs, **570/570 carts
compile**, default render byte-identical, and `api-usage` confirms the new constant is registered in all
four places.

§I4b is the one that stays **DESIGN, not a one-liner**: pushing `pt` into a range that actually disperses
also adds loop delay, which drops the pitch unless the chain's phase delay at the fundamental is
subtracted from `len` — that compensation is the real work, and it interacts with §I4d. It also wants a
decision on whether `B` becomes the real physical coefficient (so `inharm-spec` numbers can be compared
against published piano data) rather than the current arbitrary `stiff²·0.015`. **§I4b step 1 is now
done — see below.**

### §I4b step 1: the dispersion cascade is FEASIBLE, and the diagnosis was wrong (2026-07-30)

The gating question was "can a cascade of first-order allpasses in the KS loop reach a real grand's
inharmonicity, and what does it cost in pitch?" **Answer: yes, at every pitch from C2 to C6, for 3–7% of
the delay line.** The 4-stage structure already in the engine is the right one. What is broken is only the
coefficient mapping — and not in the way the first diagnosis said.

**Method, and it is the reusable part: [`tools/disp-model.js`](../../tools/disp-model.js), analytic.** A
KS loop resonates where the round-trip phase lag is a whole number of cycles, `w·L + N·θ_ap(w) = 2πn`, so
the partial frequencies are a root-find, not a render. This replaced a first attempt that patched
`sound.h` and rendered a 24-cell grid, which was slow *and* unsafe — see the postscript below. Validated
against the real engine at one point (C3, 2 stages, `c = −0.7770`):

| | model | engine (`inharm-spec`) |
|---|---|---|
| fitted B | 1.00e-4 | **1.02e-4** |
| h16 | +19.8¢ | **+19.9¢** |
| fit residual | 1.1¢ | **1.2¢** |
| f0 | 124.97 Hz | 124.84 Hz |

(The 1.8¢ of f0 disagreement is PIANO's own Railsback stretch plus §I4d, neither of which the model
includes.) `--check` encodes this point, so a future edit to the maths gets caught.

**Finding 1: the sign is wrong, and the clamp forecloses the fix.** A first-order allpass with
**positive** `c` has phase delay that *rises* with frequency — `pt` at DC to exactly 1 sample at Nyquist —
which **flattens** the upper partials. String stiffness needs the opposite, so it needs `c < 0`. The engine
computes `c = (1−pt)/(1+pt)` with `pt` clamped to ≤ 0.9, so `c` is always in (0.05, 1] and the entire
useful half of the parameter space is unreachable **by construction**. That is why scaling `pt` by 3000×
moved nothing: it was scaling inside the wrong half. The `if (pt > 0.9f)` clamp is not a harmless guard on
an unreachable ceiling, it is the thing standing in the way.

**Finding 2: the pitch dependence is backwards too.** The `|c|` needed for a fixed B *falls* as pitch
rises — `−0.72` at C3 down to `−0.09` at C6 with 4 stages — because a high note's partials span more of
the Nyquist band, which is where an allpass's delay variation actually lives. The engine's
`pt = B·(i+1)·freq/SR` grows with frequency, moving `c` the other way.

**Finding 3: it is affordable, and no register is lost.** Cost of hitting B = 1e-4 (`h16` ideal +21.9¢):

| note | L0 | stages | solved c | delay@f0 | L left | resid | h16 |
|---|---|---|---|---|---|---|---|
| C2 | 674 | 4 | −0.8494 | 49.1 | 624.9 | 0.7¢ | +20.5¢ |
| C3 | 337 | 4 | −0.7215 | 24.7 | 312.3 | 0.7¢ | +20.5¢ |
| C4 | 169 | 4 | −0.5224 | 12.7 | 156.3 | 0.7¢ | +20.5¢ |
| C6 | 42 | 4 | −0.0915 | 4.8 | 37.2 | 0.9¢ | +20.1¢ |

`delay@f0` is the compensation that must come out of the delay line; `L left` is what remains, and it
stays healthy everywhere, so the treble does not run out of line. **More stages track the physical law
better**: the fit residual falls 1.7¢ (1 stage) → 0.7¢ (4) → 0.4¢ (8), and `h16` converges on the ideal.
Four stages, which the engine already has, lands within 1.4¢ of a true stiff string.

**So §I4b downgrades from "DESIGN, may not be possible" to "a tractable change with a known recipe":**
solve `c` from a target B and the note's f0 at note-on; subtract `θ_ap(w0)·N/w0` from the delay line
(**this is the same delay-budget refactor §I4d needs — do them together**); make B a per-voicing physical
coefficient so it is comparable to published piano data; then re-voice the six voicings by ear, which is
the genuine remaining cost and makes this a LISTEN item. **2.3(b) then becomes the small step it was
originally billed as.**

#### Step 2, first attempt: the compensation and the dispersion are NOT separable (2026-07-30)

Step 1 said the recipe was "solve `c` for a target B, then subtract the cascade's phase delay from the
line". **Prototyping that showed the two halves cannot be designed independently, which is a stronger
statement than "they interact".**

With the line shortened by the computed compensation, the measured result is not a stiff string at all:

| target B | measured B | fit residual | A2 pitch | A3 pitch |
|---|---|---|---|---|
| 1e-4, 4 stages | 1.45e-3 (14×) | **42.7¢** | −119.5¢ | — |
| 1e-4, 4 stages, compensation calibrated to bring A2 into tune | 5.8e-4 | **80.6¢** | −2.3¢ | **+128.8¢** |
| 1e-4, 2 stages, calibrated | 9.1e-4 | **48.1¢** | −15.5¢ | −8.1¢ |

A fit residual of 48–96¢ means the partials are **scattered, not stretched** — they have stopped
following `f_n = n·f0·√(1+Bn²)` in any recognisable way. Forcing the fundamental into tune with a
calibrated compensation does not repair that, and a single global calibration factor does not even hold
across notes (A2 in tune while A3 sits 129¢ sharp). Shortening the line raises the dispersion's effect
relative to the loop, so `c` and `L` form a genuine coupled system; the 3-step fixed point in
`solveDesign` is not the right formulation.

**Two rules for whoever does step 2 properly.** First, **the acceptance criterion must include
`inharm-spec`'s fit RESIDUAL, not just B and pitch** — every broken attempt above hit a plausible-looking
B while sounding like a scattered metallic mess, and B alone cannot tell those apart. Second, the
uncompensated single point that validated the model (2 stages, `c = −0.7770`) had a residual of **1.2¢**,
so the structure is right when the line is left alone; the damage arrives with the compensation.

**What that leaves for the ear, and it is real.** Rather than ship a broken compensation, the A/B below
uses the *uncompensated* validated configuration and pitch-matches it with `instrument_tune`, which gives
a clean comparison of the TIMBRE at one pitch — the actual question. Single held note, struck twice:

```bash
afplay build/ab/piano-inharm-A-OFF-today.wav            # harmonic: B 1.7e-6, h8 +0.3¢
afplay build/ab/piano-inharm-B-real-stiff-string.wav    # stiff:    B 2.2e-4, h8 +17.1¢, residual 3.8¢
```

Both land within ~1¢ of the same pitch (−1.0¢ and −0.4¢), so this is a timbre comparison, not a tuning
one. **Caveats to state before any verdict:** B ≈ 2.2e-4 is roughly *twice* a typical middle-register
grand, deliberately on the audible side for a first listen, and the two takes differ by **4.6 dB in rms**
(peak matches to 0.8 dB) because the dispersed one decays faster — the stiff take is the *quieter* one, so
if it loses, check it is not losing on loudness. It also cannot yet be extended to a musical phrase,
because a phrase needs the per-note compensation that does not work.

##### ⚠ The owner's ear killed this pair, and it was right: the A/B was CONFOUNDED BY DECAY

Reported on first listen, untrained, without seeing any numbers: *"it sounds like B has the sustain pedal
pressed in, so it dies out earlier"*. Measured with `wav-envelope`, that is not subtle:

| t | A (today) | B (stiff) |
|---|---|---|
| 0.13s | 0.43 | 0.20 |
| 0.26s | 0.29 | **0.08** |
| 0.65s | 0.15 | 0.04 |
| 1.31s | 0.09 | **0.01** |

**B is ~11 dB down by 0.26 s and ~19 dB down by 1.3 s.** So the dominant audible difference in that pair
is the DECAY, not the inharmonicity, and the pair is useless as a timbre test. **The process failure is
mine and worth naming: I checked peak and rms and never checked the ENVELOPE SHAPE before handing it
over.** rms being 4.6 dB low was the symptom and I wrote it off as "decays faster, flagged as a caveat"
instead of recognising it as a confound that invalidates the comparison. A level caveat covers a constant
offset; it does not cover a different decay curve. Add `wav-envelope` to the pre-handover check for any
A/B, not just the level numbers.

**And the finding underneath is bigger than the botched A/B.** A real stiff piano string rings for
*seconds* — losing sustain is the opposite of what dispersion should do, so this is not a voicing
difference to be re-tuned, it is energy leaving the loop. Two hints for whoever picks it up: B's spectral
centroid *rises* as it decays (1026 → 1656 Hz while amplitude falls), which is backwards for a struck
string, and a first-order allpass is theoretically lossless, so the loss is coming from the interaction
with the rest of the loop (the averaging/brightness filter and `effDamp`) rather than from the allpasses
themselves. **§I4b now has a third open question alongside the coupled compensation: does dispersion in
this loop cost sustain, and why.** Until that is answered there is no point running another timbre A/B —
it would be confounded the same way.

##### RESOLVED: dispersion costs NO sustain, and step 2 works. Two bugs, both mine (2026-07-30)

The diagnostic was a controlled decay measurement — hold everything constant, vary one thing:

| case | h1 | h2 | h3 (dB/s) |
|---|---|---|---|
| 1 today (no tune, no dispersion) | −8.5 | −9.1 | −10.6 |
| 2 **tune +0.97st only, NO dispersion** | **−19.6** | **−53.5** | **−55.4** |
| 3 dispersion + tune | −17.5 | −47.2 | −64.6 |
| 5 **dispersion, NO tune** | **−7.6** | **−7.8** | **−5.3** |

**Case 2 is the answer: my own pitch-matching hack caused all of it.** No dispersion present at all, just
`instrument_tune`, and the decay collapses. Case 5 shows dispersion without the tune offset actually
sustains *better* than today's engine. The owner's ear was reporting a real defect, in my test rig.

**Engine finding worth its own row: bending a Karplus-Strong string DAMPS it.** `instrument_tune` (and
`note_glide`/`note_pitch`/pitch envelopes) shift pitch through the per-sample `effLen = len / ratio` path,
so `ratio ≠ 1` forces fractional-interpolation reads *every sample* — a lowpass inside the feedback loop,
bleeding energy on every round trip. Measured at +0.97 semitones on PIANO: h2 decay goes from −9.1 to
−53.5 dB/s, a **6× faster** decay. This applies to `PLUCK`, `GUITAR` and `PIANO` alike, and nothing
documented it. It also means **`instrument_tune` is not a safe way to pitch-match a KS A/B** — compensate
at note-on instead, where `ratio` stays 1.

**Then the second bug, and it is what actually broke the partial structure: the compensation has to reach
EVERY delay line in the voice.** `ideal2`, the detuned second string of the grand/bright voicings, is
computed independently — `SR / (freq * pv->detune)` ([`:4738`](../../runtime/sound.h)) — so shortening
`ideal` left string 2 uncompensated while it still ran through the same dispersion allpasses. The two
strings ended up ~80¢ apart, which is what produced the "scattered partials" verdict. Compensate both and
it comes right:

| | f0 | fitted B | residual | h4 | h8 | h16 | decay h1..h4 |
|---|---|---|---|---|---|---|---|
| comp on string 1 only | −2.8¢ | −1.08e-4 | 30.5¢ | −37.8 | −32.7 | −19.9 | −7.7 −7.9 −7.0 −7.2 |
| **comp on BOTH strings** | **−0.2¢** | **1.13e-4** | **1.4¢** | **+2.3** | **+8.1** | **+22.2** | −8.4 −9.3 −10.4 −20.7 |
| A today (reference) | −1.5¢ | 1.68e-6 | 0.2¢ | +0.3 | +0.3 | +0.2 | −8.5 −9.1 −10.6 −21.9 |

In tune, a real grand's inharmonicity, a **1.4¢** fit residual, a textbook progressive stretch, and a
decay curve matched to today's engine. **So the step-1 recipe was right all along** — solve `c` for a
target B, subtract the cascade's phase delay at note-on — and the earlier "the compensation and the
dispersion are not separable" conclusion was wrong, an artifact of compensating one string of two.
(That earlier subsection is left standing above as the record of a wrong turn; this is the correction.)

**Generalise the lesson past the piano:** any per-note delay correction in a multi-line voice has to be
applied to every line, and the voicings that would expose it are exactly the ones most people test last —
`harpsi`/`clavichord`/`celesta` all have `detune 1.0` and a single string, so this bug is *invisible*
unless you test `grand` or `bright`.

**The ear pair is now clean and comparable** (peak within 0.1 dB, brightness 0.144 vs 0.140, decay curves
tracking), so the only audible difference is the inharmonicity:

```bash
afplay build/ab/piano-inharm-A-OFF-today.wav          # harmonic: B 1.7e-6
afplay build/ab/piano-inharm-B-real-stiff-string.wav  # stiff:    B 1.1e-4, h16 +22.2¢, residual 1.4¢
```

**And a methodology rule earned twice over: run `wav-envelope` on BOTH takes before calling anything an
A/B.** Peak and rms passed on the previous pair while its decay differed by 19 dB. Comparability is a
property of the envelope, not of two scalars.

##### OWNER'S VERDICT on the clean pair: B preferred, "very subtle" (2026-07-30)

Recorded verbatim because the *strength* of a verdict matters as much as its direction: *"it's hard to
explain the difference I hear, it's very subtle in any case, I feel I like B better."* So **the stiff
string wins, weakly.** That is a green light to keep going, not a mandate to make it the default.

**The subtlety is expected and the test understates the effect.** A single mid-register note is the
*weakest* case for inharmonicity: its real payoff is in the **bass** (where stretched partials give the
clang a real piano has) and in **chords and intervals**, where the stretched partials of *different* notes
beat against each other — which is the entire reason the Railsback stretch exists (§I4c). A one-note
comparison cannot show either. **Now that the compensation works, a musical phrase is finally renderable**
(it was not before — that is why the first pair was one note), so the next ear test should be a bass
passage plus a chord, where the difference should stop being subtle.

##### The PHRASE pair — bass + chords, where inharmonicity earns its keep (2026-07-30)

```bash
afplay build/ab/piano-phrase-A-OFF-today.wav           # harmonic
afplay build/ab/piano-phrase-B-real-stiff-string.wav   # stiff, B ≈ 1.1e-4
```

Four events: **A1 alone**, then **A2 alone** (bass, where the clang lives), then an **A2–C#3–E3 triad**,
then a **wide A1–E2–C#3–G#3 spread** — the last two being the case a single note cannot show, where the
stretched partials of *different* notes beat against each other.

Per-note compensation verified across the register before rendering, and the important column is that the
**pitch is identical in both takes**, so nothing about tuning is in play:

| note | pitch A | pitch B | B (stiff take) | residual |
|---|---|---|---|---|
| A1 (m33) | −9.5¢ | **−9.5¢** | 1.2e-4 | 1.0¢ |
| A2 (m45) | −2.2¢ | **−2.3¢** | 1.2e-4 | 1.0¢ |
| A3 (m57) | +1.3¢ | **+1.3¢** | 1.1e-4 | 2.0¢ |

(The −9.5¢ at A1 is not an error, it is the Railsback stretch doing its job: `soct = −2.25` gives
`2·(−2.25)·2.25 = −10.1¢`, and it is present in *both* takes.)

Comparability: **rms matched to 0.22 dB** (−37.76 vs −37.98 dBFS), brightness 0.097 vs 0.096, centroid
3186 vs 3083 Hz, decay shapes tracking. Peak differs by 2.4 dB (B lower) — and **that is a real
consequence of inharmonicity, not a rig artifact**: harmonic partials periodically phase-align into tall
peaks, inharmonic ones never do, so the same rms arrives with a lower crest. Worth knowing rather than
correcting away.

##### ✅ SHIPPED 2026-07-30 — `MODE_PIANO_STIFF`, on the owner's verdict *"B is clearly better in the chords, let's ship it"*

**The engine now has real stiff-string inharmonicity, and §I4b is closed.** What shipped:

- **`pn_solve_dispersion()`** — solves the allpass coefficient from the delay DROP between the fundamental
  and a reference partial. One scalar equation, **monotone in `c`**, 28 bisection steps at note-on. Do not
  replace it with a direct fit of `B` over many partials: that is not monotone at strong coefficients and
  an earlier attempt overshot the target **46×**. The reference partial is 14 where Nyquist allows
  (calibrated to land within 2% across the register) and backs off in the top octave — C8 still works at
  reference 4.
- **The compensation goes into `ideal` AND `ideal2`.** Both delay lines run the same allpasses, and this
  is the bug that cost the most: compensating only the first leaves the grand's second string ~80¢off.
- **`MODE_PIANO_STIFF`** (`instrument_mode` idx 5): `0` = a perfectly harmonic string, `0.5` = the
  voicing's own amount, `1` = double. Target `B` scales from `PianoVoicing.stiff`, calibrated so the grand
  at centre lands on **1.1e-4** — the exact value the ear approved.
- The old mapping is gone, with a comment recording *why* it could never have worked (positive `c` flattens
  partials; the `pt ≤ 0.9` clamp made the useful half of the space unreachable).

Measured on the shipped engine, grand voicing, no patching — **and the row that matters is the pitch, which
does not move at all**, so the knob is a pure timbre control:

| knob | A1 | C3 | C4 | C5 | fitted B | h8 | residual |
|---|---|---|---|---|---|---|---|
| **0.0** | −9.5¢ | −1.0¢ | +1.7¢ | +5.1¢ | 2.4e-6 | +0.3¢ | 0.2¢ |
| **0.5** | −9.5¢ | −1.0¢ | +1.7¢ | +5.0¢ | **1.1e-4** | **+8.1¢** | 1.0¢ |
| **1.0** | −9.5¢ | −1.0¢ | +1.7¢ | +5.0¢ | 2.5e-4 | +17.8¢ | 2.9¢ |

**`0` is measured-equivalent to the old engine but NOT byte-identical**, and the reason is worth stating:
the old chain ran 1–4 allpasses at `c ≈ 0.9999948`, a near-identity that is now skipped entirely. Both
measure as harmonic (B ≈ 2e-6, h8 +0.3¢), so the difference is inaudible, but do not expect a matching sha.

**It broke tune-check, and the reason is a trap worth knowing: YIN cannot track an inharmonic string.** A
stiff string is genuinely non-periodic, so autocorrelation locks onto a shorter lag pulled by the stretched
upper partials — PIANO read **+26.1¢ sharp at A2 with confidence falling to 0.65**, while a spectral-peak
measurement of the same render puts the fundamental exactly where it belongs. The sharp reading was the
*detector*. Fix: `tunecheck.c` sets `MODE_PIANO_STIFF 0` for both PIANO passes, so the tuning sweep and the
stretch differential measure the one thing they are for on a signal their detector can track. Inharmonicity
has its own oracle (`inharm-spec`, Goertzel-based and immune). **Follow-up:** an automated "pitch is
invariant across `MODE_PIANO_STIFF`" assertion belongs in `inharm-spec`, not tune-check, precisely because
it needs the spectral method — the evidence above is measured but not yet automated.

**The `piano` cart gets a `stiff` slider** (4 columns × 2 rows now; a third row falls off-screen). Two
things came out of that: the knob indices are now a **named enum**, because inserting `stiff` mid-list is
exactly the cross-wiring trap CLAUDE.md warns about — with raw indices `decay` would have driven `knock`
and the presets would have written the wrong slots. And a **pre-existing bug surfaced**: the tuning row's
bars were drawn `CLR_DARKER_GREY` on a `CLR_DARKER_GREY` track, so `decay`/`knock`/`velo` had *always* been
invisible unless selected. `ui-audit` passed it (it finds off-screen and overlapping, not low contrast),
which is why reading the baked frame is the other half of that check.

Gates: soundcheck silent · `tune-check --quiet` 0 · `dc-check` 0 · `lint-aux-params` 0 · `lint-fx-frame` 0 ·
`lint-carts` ok · `api-usage` all four places registered · **570/570 carts compile** · `ui-audit piano`
clean · baked frame read.

**Still open:** the other five voicings' `B` now scales from `stiff` but only the grand was ear-checked, so
they want a pass (celesta 2.4e-4 down to clavichord 4.4e-5 — plausible ordering, unverified). `B` is
constant across the register, where a real Railsback curve rises at both ends. §I4d untouched. **And
2.3(b), the original level-dependent item, is now UNBLOCKED** — there is finally inharmonicity to make
level-dependent.

##### Reproducing every measurement in §2.3(a) from a cold start

The WAV pairs above live in `build/ab/`, which is **not committed**, and the scripts that made them were
scratchpad throwaways. Everything is regenerable from committed tools — and now that
`MODE_PIANO_STIFF` is a *runtime* knob, no engine patching is needed for any of it:

```bash
node tools/inharm-spec.js --check                 # trust the oracle FIRST (a null result and a broken
                                                  # tool print the same table)
node tools/inharm-spec.js --midi 48 --voicing 0   # PIANO's inharmonicity, per velocity × time window
node tools/disp-model.js                          # what a target B costs, per note/stage-count
node tools/disp-model.js --curve                  # the forward transfer curve, both signs of c
node tools/tune-check.js                          # the stretched-tuning differential (§I4c)
node tools/lint-aux-params.js                     # the five-places bound (§the aux-param trap)
```

To re-render an A/B of the inharmonicity itself, drop this in `tools/carts/_pnab.c`, set the knob to
`0.0f` for the harmonic take and `0.5f` for the stiff one, and render each:

```c
#include "studio.h"
void init(void){ instrument(5, INSTR_PIANO, 1,0,7,3200);
  instrument_harmonics(5,0.0f); instrument_timbre(5,0.5f); instrument_morph(5,0.55f);
  instrument_mode(5, MODE_PIANO_STIFF, 0.5f); }        // ← 0.0f = harmonic, 0.5f = the voicing's amount
static int f=-1;
void update(void){ f++;
  if (f == 0)   note_on(33, 5, 6);                                     // A1 alone
  if (f == 110) note_on(45, 5, 6);                                     // A2 alone
  if (f == 220) { note_on(45,5,5); note_on(49,5,5); note_on(52,5,5); } // triad
  if (f == 360) { note_on(33,5,5); note_on(40,5,5); note_on(49,5,5); note_on(56,5,5); } }
void draw(void){ cls(0); }
```

```bash
node tools/play.js _pnab run --headless --frames 540 --wav build/ab/take.wav
```

**Before calling any such pair an A/B, check it is comparable** — the hard-won rule of this thread:

```bash
node tools/wav-analyze.js  build/ab/take.wav                        # peak + rms
node tools/wav-envelope.js build/ab/take.wav 130.8 --from 0 --to 9  # the DECAY CURVE — peak and rms
                                                                    # passed on a pair whose decay
                                                                    # differed by 19 dB
node tools/inharm-spec.js build/ab/take.wav --f0 110 --decay        # per-partial, if sustain differs
```

Measure structure on an **isolated** note, never on the phrase: overlapping ringing notes make a
single-`f0` partial analysis meaningless (it read a 28¢ residual on a take that was actually clean).

##### Design commitment: inharmonicity must be SWAPPABLE, with "perfectly harmonic" available

Asked directly by the owner: *"the path we are taking, will we have the option to swap this around? Say
you want a perfect harmonic piano at some point?"* **Yes, and it is not an afterthought — it is the same
pattern `MODE_PIANO_STRETCH` just established, for the same two reasons.**

The shape, matching `MODE_PIANO_DECAY`/`_KNOCK`/`_STRETCH` exactly so there is one idiom to learn:

| value | meaning |
|---|---|
| `0` | **perfectly harmonic** — a pure string, i.e. today's sound |
| `0.5` | the voicing's own baked amount (1.0×) — the default |
| `1` | double the voicing's amount |

That gives three things at once. **A pure harmonic piano is always one call away**, which is a legitimate
musical want (chiptune contexts, unison with fixed-pitch sampled layers, or simply preferring it).
**Backward compatibility is free**, because `0` reproduces the current engine. And **it is testable**: a
compile-time constant cannot be A/B'd by a gate, which is half of why §I4b and §I4c hid for so long — so
the runtime seam is a correctness requirement, not a convenience.

The per-voicing amount should come from the existing `PianoVoicing.stiff` field, which is what was
*supposed* to drive this all along, so `harmonics` (the voicing macro) keeps varying character — celesta
stiffer, harpsichord less, per Reid — while the mode index scales it explicitly.

**Cost to be honest about:** `eng_p[]` is now 5 wide and full (indices 0–4), so a sixth parameter means
widening the array **and both `idx >= N` bounds** (the setter *and* the `SR_ENG_TUNE` handler — the trap
that has now bitten twice), plus the four-place constant registration. That is precisely the case for
building the bounds lint in the postscript below *before* adding index 5.

#### Postscript: would `spec()` have caught any of this? Mostly no — three other gates would

Asked directly by the owner mid-thread, and worth answering in writing because the answer redirects effort
away from the obvious tool. Sorting this thread's bugs by what would actually have caught them:

- **A static bounds LINT would have caught the worst one.** The `eng_p` bound exists twice (the public
  `instrument_mode` setter *and* the `SR_ENG_TUNE` request handler), and widening only one is a silent
  no-op. That is three numbers that must agree — `eng_p[N]`, both `idx >= N` guards, and the highest
  `MODE_*` constant — all greppable. It would also have caught the **earlier instance of the same bug**:
  the piano decay/knock sliders that were dead for months because the setter rejected indices 2 and 3.
  Cheapest real win here.
- **`spec()` could NOT have caught the tuning bugs.** A cart spec runs cart logic headlessly and cannot
  measure the pitch of rendered audio. §I4c was "a value computed correctly and then overwritten
  downstream" — invisible from cart-land. That needed an audio oracle, hence tune-check's differential.
  The one spec-shaped piece, a `<lib>_selfcheck()` asserting the stretch curve's *shape*, would have
  **passed happily while the bug was live**, because the formula was never wrong. The plumbing was.
- **An A/B COMPARABILITY gate is missing and should exist.** Before two WAVs may be called an A/B, assert
  peak, rms **and decay envelope** are within tolerance, else refuse. Checking the first two while the
  third differed by 19 dB is exactly the failure above.

**But the pattern worth naming is this: four separate bugs in this thread were the same shape — a value
computed correctly that never reaches the sound.** The inert dispersion (§I4b), the cancelled bass stretch
(§I4c), the dropped `eng_p` index, and the dead decay/knock sliders before them. `spec()` is the wrong
instrument for that class. The right one already exists in miniature: **`ab-render` shouts when two
variants render byte-identical audio.** Generalised — *every engine parameter should be flippable at
runtime and carry a gate asserting that flipping it changes the output in the intended direction* — that
single pattern would have caught all four. `MODE_PIANO_STRETCH` plus the tune-check differential is the
first instance of it; the runtime seam is what makes it possible, which is why a compile-time `#define`
for a tunable is a testability bug and not just a style choice.

Ranked follow-up: (1) the MODE/`eng_p` bounds lint, (2) the A/B comparability gate, (3) extend the
runtime-seam-plus-differential pattern across the `instrument_*`/`MODE_*` surface. None is `spec()`.

#### Postscript: do not patch a shared engine to search a grid

The first attempt at this measurement patched `runtime/sound.h` and rendered 24 variants. It left the
engine broken **twice**, and both failures are worth knowing because a `try/finally` looks like it covers
them and does not:

1. **A foreground timeout kills with SIGTERM, and node exits without running `finally`.** The sweep hit
   the 2-minute limit mid-grid and left `pt = 0.03f` compiled into the engine.
2. **Signal handlers do not help either**, because a signal cannot interrupt a *synchronous*
   `execFileSync` — the handler only runs once the blocking call returns, which is exactly when you no
   longer need it. And backgrounding with `&` made the tool report "completed" while node kept running,
   holding the engine patched **for minutes while another agent was rendering audio**. That render
   happened not to use `INSTR_PIANO`, so nothing of theirs was corrupted, but that was luck.

The rule that follows: **model the sweep, and patch the engine only to confirm a single chosen point**
(one render, seconds). When a patch is unavoidable, restore by targeted edit and verify the file matches
byte-for-byte before doing anything else — `git checkout` on a shared path can take a parallel agent's
uncommitted work with it.

Nothing here is approved. **`sound.h` was not changed** — every number above is from a measurement on the
committed engine, or from a sweep that restored the file in a `finally` block.

§I4b itself is **DESIGN, not a one-liner**: pushing `pt` into a range that actually disperses also adds
loop delay, which *drops the pitch* unless the dispersion chain's phase delay at the fundamental is
subtracted from `len` — that compensation is the real work, and it is the same shape as the problem
`filter-spec.js` was built to watch. It also wants a decision on whether `B` becomes a real per-voicing
`B` (the physical coefficient, so `inharm-spec` numbers can be compared against published piano data)
rather than the current arbitrary `stiff²·0.015`.

Both are recorded as findings, not as approved work. **Nothing was changed in `sound.h`** — every number
above is from a measurement on the committed engine, or from a sweep that restored the file in a
`finally` block.

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
| ~~3.44~~ | ~~Cutoff depth in octaves~~ **SHIPPED 2026-07-29 as [2.1(b)](#21b-env_cutoff_oct--lfo_cutoff_oct--shipped-2026-07-29)** — three new dests, 59 Hz-form carts untouched; the demo landed on `keytrack` (same finding, same graph) rather than `filterenv` | B2b | ✅ | 5 | `keytrack` |

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
| **Hammond saw/square registrations (1.6)** | L5 | ❌ **DROPPED 2026-07-29.** The audit called it "two rows in `REG[8][9]`"; it is not. The table lives in the ENGINE as **8 snapped detents**, so adding rows re-maps `instrument_harmonics` for the **13 carts** that set it on an organ slot (harm 0.5 would go from Jimmy Smith to Ballad) — silently, and unfixable by inspection wherever a cart's value came from ear rather than a detent index. The zero-risk route exists (`MODE_ORGAN_XREG` behind ORGAN's aux channel, idx 0 is free) but spends **permanent public API surface on two novelty presets**. And §L1 already verified all nine drawbar footages against Part 55, so the organ is not wrong — these are colours, not a fix. Recorded, not built |
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
