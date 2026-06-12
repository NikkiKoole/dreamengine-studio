# Porting an instrument or effect from navkit

Most of our `INSTR_*` engines and bus effects are ports of **navkit**
(`~/Projects/navkit/soundsystem`), a large, mature header-only software synth — the
sibling project our sound engine grew out of. When you're adding a new modeled voice, or a
voice/effect sounds "like an attempt" next to navkit's, this is the playbook. It's distilled
from the INSTR_EPIANO **clav** port, which is the worked example throughout (the same arc
applies to reed/pipe/brass/membrane/etc. and to the bus effects).

> **The one rule that matters: port the oscillator VERBATIM, oscillator-first.**
> Copy navkit's `processXxxOscillator` + `initXxxSettings` line-for-line — do not paraphrase,
> "crib," or match formulas piece by piece. Piecemeal matching *compounds* errors and never
> converges (we proved this the hard way on the clav: matching individual formulas left it
> honky/clangy through five rounds; the verbatim copy landed every partial within ~1 dB in one
> pass). **Effect/knob AMOUNTS are the exception** — those are dial-by-ear targets, not numbers
> to copy (see Gotcha 5).

This complements [`instrument-engines.md`](../design/instrument-engines.md) (the *why* and the
port program / engine catalog) and [decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md)
(effects are recipes, not primitives). For the A/B tooling, see
[`debug-harness.md`](debug-harness.md) → "A/B against navkit".

> **Picking up the port program (effects especially)?** Start with
> [`navkit-porting-handoff.md`](navkit-porting-handoff.md) — the proven sequence, the saved A/B kit,
> the gotchas that bit, and the queue of what's left. This file is the oscillator detail it leans on.

## Where the source lives

| navkit file | holds |
|---|---|
| `engines/synth_oscillators.h` | the `processXxxOscillator` per-sample fns + `initXxxSettings` (mode tables, amp profiles, decay) |
| `engines/effects.h` | the bus effects (`processWah`, `processChorus`, …) + their default params |
| `engines/instrument_presets.h` | the preset bank — search it for the index + every `p_*` constant (`instrumentPresets[180].name = "Clav Funky"`) |
| `engines/synth.h` | the `Voice` / settings structs, `applyDistortion`, `EPIANO_MODES`, the drive/velocity glue |
| `tools/preset_audition.c` | headless preset→WAV renderer (build standalone, no raylib) — your reference render |

## The workflow

1. **Render the reference first, and FFT-compare — never tweak blind.** Build navkit's
   renderer and render the target preset to a WAV; render our equivalent (`play.js … --wav`);
   compare spectra. Full recipe + the `preset_audition` build line: [`debug-harness.md`](debug-harness.md)
   → "A/B against navkit". Ears flag "messier" — a noise-floor/FFT diff tells you *why*.
2. **Rule out the cheap explanations before blaming the engine.** On the clav we burned rounds
   until we'd eliminated: the **render pipeline** (a pure sine through it read −123 dB = clean),
   **aliasing** (the excess was mid-band, not high — aliasing is high-band + pitch-monotonic), and
   a **velocity mismatch** (Gotcha 1). Only then was it clearly the engine.
3. **Copy the oscillator verbatim** into a type-specific branch (keep our other voices untouched —
   we ported only the clav `ty==2` path; Rhodes/Wurli kept our engine). Reconcile macros per
   "Mapping" below.
4. **Verify:** compile-gate + sound tripwire + `tune-check` (a pitched engine); then FFT the
   result against the reference. Target: partials within ~1 dB, mode frequencies dead-on.
5. **Dial the effect/preset amounts by ear** (drive, wah, filter) — see Gotcha 5.

## The gotchas (where the time went)

These are subtle, high-impact, and a code-skim *will* miss them — they're why "looks like I
copied it" still sounds wrong.

1. **Velocity is normalized: `vel = clampf(velocity·2, 0, 1)`.** navkit's volume range is ~0–0.5,
   so it doubles then clamps. Two consequences: (a) when velocity-matching an A/B, our
   `note_on(…,6)` = 6/7 ≈ 0.857 means render navkit at `-v 0.43`, not `-v 0.857`; (b) inside the
   port, feed the oscillator *our* native 0–1 velocity (don't re-apply the ×2). Skip this and your
   A/B silently compares two different dynamics.
2. **The ratio BLEND toward integer harmonics.** navkit crossfades each mode's frequency between a
   harmonic table (1,2,3…) and an inharmonic table by a per-mode `blendScale` — so the *raw*
   inharmonic ratios in the table (clav: `5.15×`, `6.35×`) are **not** what plays; they're pulled
   most of the way to `5.0×`/`6.1×`. Our old crib used the raw table → a clangy, detuned upper
   stack. This was *the* big tonal fix. Easy to miss because the inharmonic table is right there
   looking authoritative.
3. **The amplitude NORMALIZE.** navkit scales all mode amps so their sum ≈ velocity
   (`target = vel; scale = target/ampSum`). The pickup nonlinearity is amplitude-dependent, so
   this sets how hard the modes drive it — skip it (our crib did) and the whole harmonic balance +
   intermod is off.
4. **The preset's always-on voice filter.** navkit's clav preset has `p_filterEnabled` — its clav
   is **never** unfiltered. We spent a round comparing our bare voice to navkit's filtered one. If
   the dry tone won't match, check whether the preset bakes in a filter/filter-env.
5. **Knob/effect AMOUNTS do NOT transfer 1:1 — match by ear.** Same word, different math.
   **Drive** is the canonical case: navkit `tanh(s·(1 + drive·3))` (linear, no normalize) vs ours
   `tanh(s·drive²·24)/tanh(drive²·24)` (quadratic, loudness-preserving) — so navkit's panel "0.10"
   ≈ our 0.20 *by ear*, and navkit's panel value *also* hides a `velToDrive·velocity²` term added on
   top. We deliberately did **not** align our drive model to navkit's (24 carts depend on ours +
   its loudness-preservation; and the hidden velToDrive means a swap wouldn't transfer cleanly
   anyway). If navkit ports ever get frequent, the escape hatch is an opt-in `DRIVE_NAVKIT`
   waveshaper mode (navkit's exact `1 + x·3` raw tanh) — see [`debug-harness.md`](debug-harness.md)
   → "A/B against navkit". Until then: copy the oscillator's numbers, **ear the effects' numbers.**
6. **The noise-floor metric is relative-to-peak and pitch-dependent.** A single "median magnitude"
   reading misleads across pitches; use sub-bands (200–1k / 1k–3k / 3k–8k) and remember the ear is
   the final judge — low-mid difference-tones 70+ dB under the fundamental are usually masked
   (inaudible) even when the meter flags them.

## Mapping navkit's params ↔ our 3 macros

navkit presets carry many fixed `p_*` params (hardness, pickupPos, pickupDist, bell, bellTone,
decay…); our engines expose **three live macros** (`harmonics` / `timbre` / `morph`, per
[`instrument-engines.md`](../design/instrument-engines.md) §8.1.1). So decide which navkit param
each macro drives, and **hardcode the rest as the preset's constants.** For the clav we mapped
`timbre → pickupPos`, `morph → pickupDist` (the honk), and baked in navkit's `hardness 0.8 /
bell 0.15 / bellTone 0.3 / decay 1.0`. Don't try to expose every navkit param — pick the three
that matter for the macro contract.

## Worked example — the clav (the whole arc)

1. Our cribbed clav sounded "messier / noisy bell" and "an attempt" next to navkit.
2. Ruled out pipeline (sine −123), aliasing (mid-band, not high), velocity (matched: gap held).
3. Proved it was **intermod from the oscillator** (bypassing the nonlinearity dropped the grass to
   navkit levels), exposed because (a) we used the **raw inharmonic ratios** instead of the blend,
   (b) we **skipped the amp-normalize**, (c) our register scaling *attenuated* upper modes where
   navkit *boosts* them.
4. **Verbatim port** of `initEPianoSettings` + `processEPianoOscillator` (contact pickup),
   clav-only → every partial within ~1 dB, frequencies dead-on, in one pass.
5. Added the preset's **drive** (navkit Soft 0.10 → our ~0.2 *by ear*) for the "muted" character.
6. Added **clav + wah** = navkit's master wah ported verbatim (our `wah_process` *was* already
   navkit's `processWah`; re-added its LFO mode), at navkit's exact panel values (LFO, 2.0, 0.70,
   1.0) — dropping a non-navkit per-voice hybrid.

## Checklist

- [ ] Found the preset index + every `p_*` constant in `instrument_presets.h`
- [ ] Rendered the navkit reference (`preset_audition`), **velocity-matched** (Gotcha 1)
- [ ] Ruled out pipeline / aliasing / velocity before touching the engine
- [ ] Copied `initXxx` + `processXxx` **verbatim** (incl. ratio blend, amp normalize, per-mode velocity curves)
- [ ] Mapped 3 macros → navkit params; hardcoded the rest as preset constants
- [ ] Kept other voices untouched (type-specific branch)
- [ ] Gated: compile + sound tripwire + `tune-check`
- [ ] FFT-matched the reference (partials within ~1 dB)
- [ ] Dialed effect/preset amounts (drive, wah, filter) **by ear**, not by copying navkit's numbers
- [ ] Updated [`instrument-engines.md`](../design/instrument-engines.md) / recipe docs
