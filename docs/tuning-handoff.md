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

## ☐ Open engine residuals (tracked, not urgent — [STATUS #31](STATUS.md))

- [ ] **PLUCK / REED / BRASS go flat at the very top** (A5 −17…−25¢) — integer-sample delay
      quantization; fix is a fractional-read tap like PIPE/BOWED got. Fine in normal registers.
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
