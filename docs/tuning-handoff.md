# Tuning work — handoff / TODO (2026-06-11)

A skim-this-first summary of the PIPE / engine-tuning session. Detail lives in
[`STATUS.md`](STATUS.md) #31 (engine residuals) + #32 (sound.h split) and
[`design/audio-notes.md`](design/audio-notes.md) §18 (conclusions). This file is just the
index + the decisions that are **yours** to make.

## ✅ Done & committed (don't redo)

- **Built a tuning meter** — `tools/tune-check.js` (+ the `tunecheck` cart). Measures any
  engine's cents-of-detune; SINE is the 0.0¢ control. `--quiet` = CI gate.
  - **Recipe mode**: `node tools/tune-check.js --engine PIPE --macros h,t,m --range lo-hi`
    — checks an engine at the macros a CART actually uses (the default sweep tests macros 0,
    the worst case). This is the verify-before-shipping step for a flute voice.
- **HEAR it — the `pipetune` cart** (gallery: **"pipe tuner"**, `tools/carts/pipetune.c`). The
  audible companion to the meter: a chromatic sweep sounded against a pure SINE so drift = beating.
  Keys **1–5** flip the PIPE presets (flute/piccolo lock; recorder/breathy/pan-pipe go audibly
  flat); **UP/DOWN** rides the embouchure live to hear a macro retune the model. Verdicts validated
  against `tune-check.js`. Doubles as the prototype for the parked orchestra-tuner ([STATUS #33](STATUS.md)).
- **PIPE engine fixed** — was an octave low + flat. Now in tune ~±3¢ from C4 to ~E6 **at a
  focused embouchure (morph ≳ 0.5)**. Fix = half-wavelength bore − a jet-length-derived loop
  delay + the fractional-read trick. (`runtime/sound.h` `sound_pipe_start`.)
- **Carts retuned** — `air.c` Cherry flute register reopened 67–83 → **64–86**; `polopan.c`
  NANGA flute dropped an octave to **60–77** (natural register, in PIPE's in-tune zone).
- **Warnings added** so an agent picking PIPE is told it needs tuning care: `studio.h`
  `INSTR_PIPE` define, the editor hover (`studioDocs.js`), and the
  [`instrument-recipes.md`](guides/instrument-recipes.md) PIPE shelf (per-preset tuning column).
- **Clobber guards** — `.githooks/pre-commit` (compile-gate for sound.h/studio.h commits) +
  the CLAUDE.md "Hot shared source files" protocol.
- The old air "is PIPE dependable?" residual is marked **RESOLVED** in
  [`design/air-effects-wants.md`](design/air-effects-wants.md).

## ☐ Your call — decisions I left for you

- [ ] **Enable the pre-commit hook?** Currently tracked but **dormant**. One command turns it
      on for this clone (adds ~2s to sound.h/studio.h commits, may block another live agent's
      commit — that's why I didn't flip it):
      `git config core.hooksPath .githooks`   (undo: `git config --unset core.hooksPath`)
- [ ] **Schedule the `sound.h` per-engine split** ([STATUS #32](STATUS.md)). The real fix for
      the clobber, but it's a big refactor that must be done in a **quiet window** (freeze other
      audio agents, split, verify byte-identical, unfreeze). Only worth it if you keep running
      several audio agents at once. Not urgent — the cheap guards hold the line.

## ☐ Bigger idea, parked behind a prerequisite — [`instrument-bank-plan.md`](instrument-bank-plan.md)

- [ ] **Instrument bank + orchestra-tuner cart** ([STATUS #33](STATUS.md)). A shared
      machine-readable registry of the named dependable voices (kills the copy-paste-from-doc),
      + an audible/visual tuner cart that auditions a voice against a sine reference. Groundwork
      audit done (zero drift; vanilla anchors pinned). **Parked until a clean 4th-axis/aux-param
      API lands** (today's `eng_tune` is experimental) — otherwise the preset schema can't capture
      pizz/fundamental/nasal. Full spec + locked decisions in the plan doc. *(The "radio voices
      missing from the docs" side-task was a false positive — resolved on inspection, plan doc §6.)*

## ✅ Engine residuals — PLUCK / REED / BRASS top-octave (FIXED 2026-06-16, commit e458af1)

The old diagnosis below was **wrong**: all three reads already interpolate (the down-bend
session added it), so "add a fractional read tap" was a no-op. The real causes, now fixed:
- **REED/BRASS** sized the note-on bore from an INTEGER-truncated delay → high notes sharp &
  erratic (BRASS C#6 was +64.5¢). Now use the TRUE fractional delay as the init reference.
- The residual smooth flat ramp was the **bell-LP loop group delay** → subtract
  `(1−lpCoeff)/lpCoeff` from `effLen`, scaled per engine (BRASS ×0.5, REED ×1.0).
- **PLUCK**'s flatness was the Karplus damping average's exact half-sample delay → −0.5 on the
  tap (exact at all freqs). Verified: all three in tune at real macros; SINE control 0.0¢.

## ☐ NEXT — follow-ups from the 2026-06-16 ear test (engine-tuner cart)

- [x] ~~**BOWED bow PRESSURE.**~~ **DONE 2026-06-16 (commit d90f2a3).** Turned out the bright
      end was *too* high — scratchy (>4kHz noise 3.6% vs 0.5% at the sweet spot). Recompressed
      the timbre→pressure map [0.12,0.32]→[0.10,0.26] so even full timbre stays musical; typical
      voices sit in the clean 0.15–0.20 singing zone. Pitch unchanged. Heard via the engine tuner.
- [x] ~~**PIPE presets still off.**~~ **MOSTLY DONE 2026-06-16 (commit 97a794e).** The jet
      loop-delay was under-compensated at long jets (hollow embouchure); the recorder/breathy/
      pan-pipe ran flat to ~−56¢ by G5. Fix: a clamped-linear jet-delay correction past jetLen 5
      (measured ~+0.8 sample, SATURATING — not the quadratic I first tried), ZERO at jetLen ≤ 5 so
      flute/piccolo stay byte-identical. **All 5 presets now in tune through A5**; morph-0 extreme
      improved too (A5 −84¢→−32¢). **Still open:** the morph≈0 / hollow top octave (above ~A5) still
      mode-flips — that's the jet∝bore re-voicing (residual below), a separate bigger fix.

## ☐ Open engine residuals (tracked, not urgent — [STATUS #31](STATUS.md))
- [ ] **PIPE at morph ≲ 0.4** (hollow/low embouchure, e.g. pipe.c's `recorder`/`breathy`
      presets) drifts flat up high, and **morph ≈ 0** is seed-unstable in its top octave.
      Real recipes use morph ≳ 0.5, so this is mostly a "don't voice a flute hollow + high"
      note — documented in the shelf. Fully closing it needs a jet-length re-voicing.
- [ ] *(optional)* re-voice pipe.c's `recorder`/`breathy` presets toward morph ≳ 0.5 if you'd
      rather they be in tune than characterful — left as-is + documented for now.

## Quick reference

```bash
node tools/tune-check.js                                   # full default sweep (all engines)
node tools/tune-check.js --engine PIPE --macros 0,0.38,0.70 --range 48-90   # one recipe
node tools/tune-check.js --quiet                           # CI gate (exit 1 if out of tune)
```
