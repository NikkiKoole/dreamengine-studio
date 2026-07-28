# Synth Secrets — the build plan

STATUS: BUILDING — **Phase 0 is DONE (2026-07-28)**; Phase 1 is next. The ordered work ledger derived from
[`synth-secrets-audit.md`](synth-secrets-audit.md). The audit is the *findings*; this is the *doing*.
Phase 0 (free/factual, no ear needed) is complete — its results are in §3. Nothing beyond it is approved.

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
| 1.1 | `solina`: use `LFO_DETUNE` + a Random-shape LFO on it (§F2) | LISTEN | 1 | `solina` | **BUILT, awaiting ear** — key **W** cycles CLASSIC / RANDOM WOW / BREATHING DETUNE. See results below |
| 1.2 | 808 cymbal: three bands, three unequal decays (§J5) | LISTEN | 1 | `tr808` | The mechanism that makes a real cymbal's spectrum migrate. `tr808.h` only |
| 1.3 | Velocity → snare tone/noise balance (§J9) | LISTEN | 1 | `tr808`, `tr909` | Harder hits should read noisier |
| 1.4 | Brass preset: 1 ms attack → 100 ms, 1200 ms release → short (§E10) | LISTEN | 1 | `brass` | ⚠ release also governs the bore ring-down, so A/B rather than edit |
| 1.5 | A two-slot layered piano patch (§I9) | LISTEN | 1 | `piano` | Part 45's whole conclusion, and free |
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

What to listen for, per Reid: CLASSIC should reveal "an unnaturally regular modulation" once you notice
it, and RANDOM should keep the same thickness while the cycle stops being audible — "thick and unstable
… 'analogue', or perhaps 'human'". If you cannot hear the difference over ~30 s of the AUTO progression,
rung 2 is a DROP and only BREATHING DETUNE is worth keeping.

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
