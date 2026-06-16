# Waveguide pitch-bend + brass character — handoff (2026-06-16)

Orientation for whoever picks up the delay-line sound engines next. One session did two
interlocking things — **made BRASS brassier** and **made the waveguides bend pitch DOWN** — and left
three concrete threads open. This is the map + the exact resume steps + the measurement workflow.
Everything here lives in `runtime/sound.h` (the one hot shared file — see CLAUDE.md "Hot shared
source files" before editing: targeted `Edit`s only, re-Read the region first, compile-gate, grep
`HEAD` after commit).

## What shipped this session

| commit | what |
|---|---|
| `8dfd12a` | **BRASS brassier** (level-coupled shock-wave brightness) + **brass slide bends DOWN** |
| `d7e6957` | **REED / PIPE / BOWED** can now bend pitch **down** (same fix) |
| `4e10dc1`, `73c4eb5`, `4c69238` | docs: STATUS #41, mark brightness shipped, "still more brassy", tooling |

Gates used throughout (all green): `node tools/play.js soundcheck script /dev/null --headless
--frames 2` (compile), `node tools/dc-check.js --quiet` (shaper DC), `node tools/tune-check.js
--quiet` (tuning). SINE reads 0¢ = the rig is honest.

## The mental model (read this first)

The waveguide engines (REED 23, PIPE 25, BOWED 28, BRASS 29) are a **delay line** (`v->ks_buf`, a
circular buffer) closed by a reflection + a nonlinear exciter (reed valve / jet / bow / lip). Two
facts that explain everything below:

- **Pitch = the READ delay** (`effLen`), recomputed every sample from the live freq. NOT the buffer
  size. The buffer just has to be ≥ `effLen`.
- **Bending DOWN needs a LONGER delay.** The buffer is sized at note-on; if it's sized exactly at the
  note, `effLen` clamps at the buffer length and the note can only bend UP. That was the bug (the
  trombone slide only slid up; bass lines re-triggered lower).

**The fix pattern (applied to all four):** size the buffer **×2.5** at note-on (~16 semitones of
down-room, capped by `SOUND_KS_MAX` = 1024) and pick the init-freq reference so the **note-on read
length is unchanged** → tuning byte-identical, only the clamp ceiling rises. Per-engine init-freq
trick: brass/reed use `d0i*f/len`; pipe already had `f*targetBore/len`; bowed folds the buffer into
`bw_initfreq = (SR−0.5f)/(nutLen+brLen)` so the *total* effective delay (hence pitch) is independent
of buffer size. BOWED also needed its pitch-`ratio` clamp widened 4→12.

**Caveat that bit:** BOWED is full-wavelength (2× a half-wave engine), so the buffer cap bites at the
bottom — down-bend works from ~**E2 up**, but **E1 (open low-E, ~41 Hz) can't bend** (buffer already
at `SOUND_KS_MAX`). PLUCK/GUITAR already had ~1 octave (2× headroom); simple oscillators
(SINE/TRI/SAW/SQUARE/FM/PD) bend freely.

## Open threads — concrete next steps

### A. Make BRASS brassier (the owner still hears room)
The brightness fix is a **conservative first pass** — it stops at ~2.3% energy >4kHz to dodge fizz,
but a realistic forte trumpet has energy to ~8–10kHz. All the knobs are on the OUTPUT stage of
`sound_brass_sample` (do NOT touch the bore loop — opening the in-loop bell LP flips the register an
octave down, that's why brightness is generated downstream):
- `br_env` — bore-amplitude follower → 0..1 `lvl` (the "playing level").
- `driveOut = 1 + bright*(2.2 + blow*4 + lvl*2.5)` — asym-shaper steepening; the `lvl*2.5` is the
  level coupling, push it.
- `brite = bright*(1−0.6·dark)*(2.0 + 6.0·lvl)` then `br_hp` high-shelf `blaat += edge*brite` — the
  >4kHz lift; the main "more blat" knob.
- `blaat *= 1/(1+0.19·brite)` — self-balancing loudness trim; raise the `0.19` if pushing brite hard
  makes it too loud (target peak ≈ −3 dBFS at forte/vol7).

**Workflow (measure, don't guess):**
```bash
node tools/play.js brasspec script /dev/null --headless --frames 240 --wav /tmp/b.wav --det
node tools/harmonic-spec.js /tmp/b.wav 220 --n 28   # highest-h-within-20dB + energy>4kHz + crest
node tools/wav-analyze.js /tmp/b.wav | grep -iE 'peak|rms|crest'   # watch peak ≤ ~-3 dBFS
```
`tools/carts/brasspec.c` plays one sustained forte trumpet (A3=220); edit its `SPEC_*` defines to
audition other engines/macros/notes. Baseline→shipped was: highest-h h9→h17, >4kHz 0.2%→2.3%, crest
6.3→14.6dB. Always re-run `dc-check --quiet` after any shaper change, `tune-check --quiet` if you
touch anything pitch-related.

> **Tried & reverted (2026-06-16): the output stage is at its limit.** Pushed all four levers
> together in a measured A/B (driveOut `lvl` 2.5→3.6, asym 0.7→0.82, brite `+6lvl`→`+8.5lvl`, LP
> 0.42→0.30, trim 0.19→0.24 to hold −2.7 dBFS). Result: +3dB across the **2.8–5kHz blat band**
> (highest-h h17→h19), DC/peak clean — but **inaudible** in a solo A/B to the owner. The energy
> the ear wants at 8–10kHz (h36–45 on a 220Hz tone) can only be *generated* by driving the
> waveshaper harder, which **aliases into fizz** (no oversampling). Conclusion: more brass now
> lives in the **macros** (`instrument_timbre`/`instrument_morph`); the only real engine lever
> for genuine top-end is **oversampling the shaper** — a separate project, not a knob tweak.

**Bigger, unshipped levers:** the deferred **mute/plunger axis** (the
harmon-cup bite — a `note_cutoff`+`note_res` cart recipe or an `instrument_mode` index, NOT a 4th
macro) and handoff fix #3 (model the bell to fill the harmonic series natively). Full context:
[`brass-realism-handoff.md`](brass-realism-handoff.md) + [`audio-notes.md`](audio-notes.md) §19.

### B. Waveguide tuning — flatten at the top (✅ FIXED 2026-06-16, commit e458af1)
BRASS/REED/PLUCK top-octave flatness is fixed. The diagnosis here ("a fractional read tap on the
engines that lack it") was **wrong** — all three reads already interpolate. The real fix: REED/BRASS
sized the note-on bore from an integer-truncated delay (→ sharp high notes); use the true fractional
delay as the init reference. The residual smooth ramp was the bell-LP loop group delay → subtract
`(1−lpCoeff)/lpCoeff` from `effLen` (BRASS ×0.5, REED ×1.0). PLUCK: −0.5 on the tap for the Karplus
averaging's exact half-sample delay. Full writeup: audio-notes §18 conclusion 7. **Still open:** PIPE's
morph≈0 top octave (the hollow presets) — needs the jet∝bore re-voicing (tuning-handoff.md → NEXT).

### C. Revisit the carts built around the old "can't bend down" limit (STATUS #41)
`tools/carts/upright.c` (upright bass, `INSTR_BOWED`) hard-codes an **up-only** pull-bend
(`fabsf(dpx)` → always `+vbend`) and fret-walks (re-articulates) downward — *because the engine
couldn't bend down*. Now it can (above ~E2). Make the pull-bend **signed** (pull down → smooth
flatten), keep fret-walk as the fallback below E2 and as deliberate walking-bass articulation. Update
its description ("…can't be bent below its pitch" is now false). `pdbass.c` was spun off only to get a
two-way slide via `INSTR_PD` — still valid as the buzzy-CZ variant, but the upright no longer needs
the workaround.

## Tooling (kept, committed this session)
- `tools/carts/brasspec.c` — one sustained note on a chosen engine/macros, for clean spectrum renders.
- `tools/harmonic-spec.js <wav> <fundHz> [--n N]` — harmonic series (dB rel. f1) + highest-harmonic-
  within-20dB + energy>4kHz + a crude pitch readout. Pairs with `tools/wav-analyze.js` (peak/rms/crest).
- Existing: `tune-check.js` (tuning gate), `dc-check.js` (shaper DC gate), `play.js … --wav` (render).

## Doc map
- [`brass-realism-handoff.md`](brass-realism-handoff.md) — the brass diagnosis + as-built brightness fix.
- [`audio-notes.md`](audio-notes.md) §18 (tuning audit, §18.6 = down-bend), §19 (brass character).
- [`instrument-engines.md`](instrument-engines.md) §8.8.10 (brass design), §8.8.x (the other engines).
- [STATUS](../STATUS.md) #31 (tuning), #41 (down-bend + cart revisit).
