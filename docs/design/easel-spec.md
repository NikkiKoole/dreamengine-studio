# easel — a Buchla Music Easel (complete west-coast voice) — spec

**Status: ✅ BUILT 2026-06-22** (`tools/carts/easel.c`) — v1 shipped per this spec (mono voice;
poly is deferred). Verified: the low-pass gate collapses brightness 1.86 → 0.02 as it closes (the
FM complex-osc + folder give it rich harmonics to gate — far stronger than `lpg`'s triangle). A *complete* west-coast synthesis voice built around
the low-pass gate from [`lpg.c`](../../tools/carts/lpg.c) (shipped 2026-06-22). Where `lpg` showed
the gate in isolation, `easel` is the whole instrument the gate belongs to — complex oscillator →
wavefolder → low-pass gate, played by a pressure ribbon, modulated by a function generator. Lineage:
**Buchla Music Easel (1973)** / Make Noise. Sibling of the liveset playthings
([`cart-library-direction.md`](cart-library-direction.md) §2c).

## North star

West-coast synthesis makes timbre **additively** — a complex oscillator generating harmonics by
cross-modulation + wavefolding, then a **low-pass gate** that opens/closes volume AND brightness
together for organic, struck, vocal motion. The opposite of every subtractive cart we have. `easel`
is one voice, four stages, played expressively by hand — *and* able to play itself.

## The voice chain (the signal path)

A small **pool of held voices** (≈4, for overlapping notes/plucks) on one `INSTR_FM` slot
(`DRIVE_FOLD` + `FILTER_LOW`), each stage ridden live per voice:

1. **Complex oscillator** (`INSTR_FM`) — primary + modulator. Three knobs, all live:
   - **RATIO** → `note_harmonics` (carrier:mod ratio; integer = harmonic/mellow, offset = bell/clang)
   - **TIMBRE** → `note_timbre` (mod index = cross-mod depth; near-sine → bright → clangorous)
   - **GROWL** → `note_morph` (FM feedback; clean → growl → metallic) *(optional 3rd knob; may fold into TIMBRE for space)*
2. **Wavefolder** — **FOLD** → `note_drive` (`DRIVE_FOLD`): folds the tone for glassy/metallic harmonics.
3. **Low-pass gate** — the final voicing: `note_vol` + `note_cutoff` driven *together* from the gate
   level (the `lpg.c` coupling — `cutoff = lo + (hi−lo)·level^1.6`, so highs die first). This is where
   today's LPG lives in the chain.

## The function generator (NEW module)

The west-coast envelope/LFO — one reusable primitive the library lacks. A **slope** with:
- **RISE** + **FALL** times.
- **MODE: one-shot** (triggered → rise then fall → a pluck/bonk) **or CYCLE** (loops rise↔fall = an
  LFO/oscillator — the "Maths" trick).
- Drives the **gate level** by default; a **DEST** toggle can instead point it at **TIMBRE** (auto
  cross-mod sweep) or **PITCH** (a wobble). One-shot + gate = the classic struck plonk; cycle + timbre
  = evolving west-coast motion with no hands.

Drawn as a live slope with a moving dot, so you see it fire/cycle.

## The pressure ribbon (NEW surface)

The play surface (the hero). A wide horizontal strip:
- **X = pitch** — continuous, with on-strip **scale guide marks**. **SNAP** toggle: snap-to-scale
  (playable, default) vs **FREE** (continuous/microtonal — the authentic Easel capacitive keyboard).
- **Y = pressure** — drag up = harder: opens the gate further and adds fold, so a firmer press is
  louder + brighter + more metallic. Expressivity beyond on/off keys.

Two **play modes** (toggle):
- **PRESS** — Y rides the gate live (swell notes by hand, the FG sets the slope you swell along).
- **PLUCK** — a tap *triggers the function generator* (one-shot AD → a bonk); Y = velocity. Re-tap to
  re-pluck; hold for the sustain portion.

## Self-play (a PULSER + a random source)

A **PULSER** (a clock at a RATE knob) that auto-triggers the function generator and picks a pitch
from a **random source** (S&H over the current scale) — so it plays itself (a generative west-coast
patch) while you ride RATIO/TIMBRE/FOLD/RISE/FALL. **RUN** toggles it. (The Easel's 5-step *pitch*
sequencer is deferred to v2; v1's self-play is pulser + random.)

## Screen layout (320×200, sketch — refine at first screenshot)

```
┌ EASEL   west-coast voice          PLUCK ┐  top: title + play-mode
│  COMPLEX OSC → FOLD → LPG    ╱╲ FG slope │  upper: the signal-chain flow + the
│  (live levels / the patch flow)  ╱  ╲    │         function-generator scope
│  ◯RATIO ◯TIMBRE ◯FOLD ◯RISE ◯FALL ◯RATE │  knob row (≈6)
│  [PRESS|PLUCK] [FG CYCLE] [RUN] [SNAP]   │  toggles
├──────────────────────────────────────────┤
│  ▏ ▏ ▎ │ ▏ ▏ ▎ │  pressure RIBBON  Y=press │  bottom: the play surface (hero)
│  C  D  E  G  A  …  (X=pitch, scale marks) │     X = pitch, drag up = harder
└──────────────────────────────────────────┘
```

## Controls (summary)

- **Ribbon**: drag — X pitch, Y pressure. Tap (PLUCK mode) fires the function generator.
- **Knobs**: RATIO · TIMBRE · FOLD · RISE · FALL · RATE (pulser).
- **Toggles**: PRESS/PLUCK · FG CYCLE (loop the slope) · FG DEST (gate/timbre/pitch) · RUN (pulser) ·
  SNAP/FREE tuning · SCALE select.
- **Keyboard**: A–K play scale steps (so it's playable without a touch ribbon); SPACE = RUN; arrows
  nudge a focused knob.

## Reused chassis + what's new

- **lpg.c** → the coupled `note_vol`/`note_cutoff`/`note_drive` gate (the final stage) + the
  held-voice pool.
- **heldnotes.c / martenot.c** → the ribbon (a held-note continuous-pitch surface); add the Y-pressure
  dimension on top.
- **`INSTR_FM`** → the complex oscillator (ratio/index/feedback as RATIO/TIMBRE/GROWL via
  `note_harmonics`/`note_timbre`/`note_morph`, all live-slewed).
- **NEW (all cart-side, no engine work):** the **function generator** (triggerable/cycling slope as a
  player module), the **pressure ribbon** (X-pitch × Y-pressure), the **complex-osc timbre control**
  (cross-mod as a playable axis), and a **pulser + S&H** self-play.

## Scope

**v1 (build):** the 4-voice FM→fold→LPG chain with live RATIO/TIMBRE/FOLD; the function generator
(rise/fall, one-shot/cycle, → gate/timbre); the pressure ribbon (X-pitch snap/free, Y-pressure) +
A–K keys; PRESS/PLUCK modes; a pulser + random-pitch self-play (RUN); the signal-flow + slope visuals.

**v2 (shipped 2026-06-22):** **polyphony** (4 voices — chords on A–K, overlapping plucks, sequence
notes ring over each other) + the **5-step sequencer** (RUN steps a 5-note pitch sequence you set by
tapping the step cells; a **SEQ/RND** toggle picks sequence vs random). PRESS still grabs one voice;
the rest of the pool is poly.

**Deferred (v3):** a second (modulation) oscillator with its own controls (true dual-osc complex); a
dual LPG (two gates); preset/patch save; MIDI/pressure from a real controller.

## Risks / open questions

- **Knob count.** Six knobs + several toggles + a ribbon on 320×200 is dense. Mitigation: drop GROWL
  into TIMBRE (5 knobs), compact toggles, and let the ribbon be the only big element. Decide at first
  screenshot (the dubdesk density lesson).
- **FM macro feel.** `note_timbre`/`note_morph` are slewed — fast sweeps may lag; fine for west-coast
  (slow, liquid motion). Confirm RATIO via `note_harmonics` reaches a *sounding* voice (the studio.h
  note says FM ratio does; verify at build, else set ratio at trigger only).
- **Tuning playability.** FREE ribbon is authentic but hard to play in tune; default SNAP, FREE as a
  toggle. Scale guide marks on the strip either way.
- **CPU.** 4 FM voices + per-voice fold + per-frame macro/gate rides — comfortable; profile once.

## Build plan

1. The voice: one `INSTR_FM` slot + `DRIVE_FOLD` + `FILTER_LOW`, a 4-voice held pool; wire RATIO/
   TIMBRE/FOLD knobs to `note_harmonics`/`note_timbre`/`note_drive`. Get a tone you can sweep.
2. The low-pass gate: drive `note_vol`+`note_cutoff` from a per-voice gate level.
3. The function generator: rise/fall slope, one-shot + cycle; route to the gate. Tune the pluck.
4. The pressure ribbon: X-pitch (snap/free + scale marks), Y-pressure → gate/fold; A–K keys; PRESS/
   PLUCK modes. **Guard the ribbon's raw-touch read with `ui_captured(touch_id)` + sticky-until-lift
   ownership** (added to `ui.h` + `dubdesk.c` 2026-06-22) so a finger that started on a knob and
   drags over the ribbon doesn't trigger it — the density-overlap fix the dense layout needs.
5. The pulser + random self-play (RUN).
6. Visuals (signal flow + slope), ui-audit, bake, register (teaches: `west-coast-synthesis`,
   `fm-synth`, `wavefolder`, `lowpass-gate`, `adsr-envelope`), shelf row, demo clip.
