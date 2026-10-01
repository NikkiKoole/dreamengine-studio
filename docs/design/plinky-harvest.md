# What we can take from Plinky's open-source release

> **STATUS: BUILDING (2026-10-01)** ‑ surveyed the voice + sampler + FX code; shipped §3 `INSTR_WAVESCAN`
> (the scanned wavetable bank into a low-pass gate, `wavescan` cart) and §4.1 `INSTR_GRAIN` (a granular
> voice per note, `grainstrings` cart + `grainprobe`). Open: §4.2 multisample round-robin, §4.3 the playing
> surface (pressure on every parameter), §4.4 loading the CHOMPI tables into the same engine. Found on the
> way, §2: Plinky's shipped wavetable is not what its source says it is.

[Plinky](https://github.com/plinkysynth/plinky_public) is an 8-voice polysynth played on eight touch
strips, everything locked to a scale. It's the top pick of [open-source-audio-toys.md](open-source-audio-toys.md)
§2.1: same language as us (C), touch-first, made for anyone to play. Surveyed from a clone of the repo
(last push 2025-10), reading `sw/Core/Src/plinky.c` (`RunVoice()`, `UpdateEnvelope()`, `Reverb2()`),
`sw/emu/main.cpp` (the table generator) and `params.h` (the defaults).

## 0 · Survey: the engine is small, the surface is the instrument

| Plinky | ours | verdict |
|---|---|---|
| **wavetable scan**: one knob through 16 band-limited cycles, the two neighbours crossfading | `INSTR_USER0-3` single drawn cycles + `wave_set()`; no bank, no scan | **gap → §3, shipped** |
| a 2-pole low-pass whose coefficient IS the envelope (a **low-pass gate**, on every voice) | the LPG inside `shallow()`, bus only; `ENV_CUTOFF` uses a separate envelope | **gap → §3, shipped** (the `morph` macro) |
| 4 oscillators per voice, `interval` on one pair, `microtune` spreads them | `instrument_unison()`, `instrument_tune()` | have it; ported anyway as part of the voice (§3) |
| shape knob's negative side: PWM made by subtracting two phase-shifted saws | `LFO_DUTY` on square slots | have it (a different construction of the same sound); not ported |
| **per-note granular sampler**: 8 slices, finger scrubs, two crossfading grain pairs, jitter, time-stretch | `grains()` is a bus cloud; `INSTR_SAMPLE` + `instrument_sample_region()` is plain playback | **gap → §4.1, shipped** |
| multisample: nearest slice to the note + round-robin | none | **gap → §4.2** (mostly cart-land) |
| delay with wobble, reverb with shimmer, HPF, saturation | `echo_insert_bbd()`, `shimmer()`, `filter()`, `drive_insert()` | have it |
| pressure on each strip modulates ANY parameter; 4 LFOs + tilt; arp/seq with probability + Euclid | `note_*` live setters, `instrument_lfo`, `patgen.h`, `euclid()` | the playing surface → **§4.3** |

So, three real engine gaps. The bigger lesson is the surface: most of what you hear in a Plinky video is
pressure riding the gate and the scan, not exotic DSP.

## 1 · License, and what not to touch

- **The software is MIT** (`LICENSE.md`: "the rest of the software of plinky is licensed under the MIT
  License"). Ported code keeps the credit in its comment: *Copyright (c) Alex Evans*.
- **The logo, panel and every graphic element are CC BY-SA 4.0**, and the author asks derived products
  not to use the Plinky name (the Mutable Instruments convention). So: no cart, app or rack called Plinky
  or looking like one. The cart is `wavescan`; this doc's name describes a source, which is fine.
- **Third-party parts keep their own licenses**: STM32 Cube, tinyusb, dear imgui, portaudio.
- **The hardware is CERN-OHL-P v2.** Nothing of ours touches it.

## 2 · Finding: the shipped wavetable is not what the source says

The tables come from `main.cpp`'s generator: `eval_wave()` defines each shape, then a Hann-windowed sinc
builds a 9-level mip pyramid (512 … 2 samples, one guard sample each, 1031 per shape). That's the code we
wanted. But the committed `wavetable.h` doesn't match it:

- `eval_wave()` reads a WAV file for slots 2-15 when `cyclenames[]` names one, and it does:
  `"c:/temp/waves/saw_1024.wav"`, `"wave1_2048.wav"` … on the author's own disk, origin unknown.
- Slot 0 is the procedural **saw** (we reproduce it **byte-identically**). Slot 1 is a **cosine**, not the
  square `wtenum.h` labels it: the table was generated under an older enum order.
- None of the other fifteen slots match any recipe (closest mean error 5,600-9,600 LSB of 16,384).

So we **regenerate from the recipes** (`tools/gen-wavescan.js`): unambiguously MIT, and reproducible. We
don't ship the data: we can't say where those WAVs came from. Same rule as CHOMPI's factory samples.

## 3 · `INSTR_WAVESCAN` (engine 36): shipped

Plinky's voice, the positive side of its shape knob. From `RunVoice()`:

- **The bank** (`runtime/wavescan_data.h`, generated, const): twelve recipes (sine, sine+3rd, three folded
  sines, an FM cycle, square, saw, three noise-folded sines, one cycle of white noise as a pitched buzz),
  **ordered by measured spectral centroid** so turning `harmonics` up only ever brightens:
  1.0 ≤ 1.6 ≤ 2.4 ≤ 2.4 ≤ 2.7 ≤ 3.6 ≤ 5.0 ≤ 6.9 ≤ 13.5 ≤ 14.4 ≤ 28.1 ≤ 102.7 harmonics.
  Plinky's own order was saw, square, sin, sin2, fm, folds, noises; reordering is ours.
- **Band-limiting**: each oscillator reads the largest mip level no longer than its period (Plinky's
  `shift = 16 - clz(dphase)` rule, written in samples so it holds at 44.1 kHz too).
- **The scan**: `harmonics` 0..1 crossfades shape k into k+1, continuous (no detents), live.
- **The low-pass gate**: upstream's `y1 += (in − (y2−y1)·res − y1)·g; y2 += (y1−y2)·g` with g = the
  envelope, its pole and its 0.999 leak carried from 31.25 kHz to our rate. `morph` blends g from 1 (off)
  to the envelope; at 0 the filter is skipped entirely, so the bare oscillators come out exact.
- **Macros**: harmonics = SCAN · timbre = SPREAD (upstream's microtune) · morph = GATE. **Aux** (note-on):
  `MODE_WAVESCAN_RES` (0..1.9, with upstream's drive trim against it), `_NOISE` (squared, as upstream),
  `_INTERVAL` (-12..+12 semitones on two of the four oscillators, 0.5 = unison).

**Changes from upstream, each deliberate:**
- **The detune is recentred.** Upstream's offsets sit −0.19 st flat at full microtune; ours spread each
  pair ±0.125 st around the note, so the voice stays in tune (tune-check below).
- **Four oscillators on the table path.** Upstream reads one per stereo side there, four only on its saw
  path. Summed mono: our pan stage owns stereo.
- **Our VCA still runs after the gate**, so morph 1 is VCA × LPG, a touch shorter than upstream's
  gate-only plonk.
- **Phases reset and the noise reseeds at note-on.** Upstream free-runs. Ours renders a patch the same
  way every time.
- **The tables are const data, not built at boot.** A runtime build would call `sin()`, which rounds
  differently natively and in wasm ([determinism.md](determinism.md)), and would be shared mutable state
  for two plug-in instances.

### Evidence (2026-10-01)

| check | result |
|---|---|
| `gen-wavescan.js --selfcheck` | saw byte-identical to Plinky's slot 0; sine within rounding of slot 1 (2 samples off by 1 LSB, their `cosf`); guard samples; centroid order; level 3 band-limited to −40 dB at harmonics 30-31, **and a raw saw FAILS the same test** (−26 dB): the negative control |
| tune-check, default sweep | A2-A5 within +0.3¢ |
| tune-check `--engine WAVESCAN --macros 0.45,0.6,0.8 --range 36-96` (spread + gate on) | C2-C7 within ±0.7¢ (+2.6¢ at C2): the recentred detune holds |
| level-check | peak −15.1 dBFS = the library median exactly; every other engine Δ 0.0 (baseline blessed with the new row) |
| soundcheck 900 frames | silent (slot 16 = scan 0.45, spread, full gate, res, noise, a fifth) |
| aliasing probe: non-harmonic energy, saw position, gate off | **WAVESCAN −32 / −29 / −25 dB at A6 / A7 / C8; naive `INSTR_SAW` −13 / −10 / −10 dB.** 16-19 dB cleaner, not perfectly clean at the very top |
| gate probe: centroid every 133 ms over a decaying note | GATE 1: 2751 → 2201 → 1765 → 1398 → 1065 → 779 Hz as the level falls −21 → −37 dB. GATE 0: 3361 … 3369 Hz, flat |
| `wavescan` cart `spec()` | 23 / 23 (Plinky's stride table worked by hand, every pad in the scale, interval snaps) |

**The aliasing residual** (−25 dB at C8) comes from Plinky's rule picking a table as long as the period:
the kernel's skirt plus linear interpolation leave a little above Nyquist. One level shorter (period/2)
would trade the top octave's brightness for cleanliness. Kept upstream's choice; flagged here.

**Not measured:** CPU per voice (four oscillators × two table reads, one `powf` per sample when spread is
on, one more when the gate is on). Profile before a phone cart stacks eight of them.

### The `wavescan` cart
Plinky's surface, minimally: 8 strings × 8 pads, one voice per string, tuned by Plinky's `stride()`
(C major, a fifth: the strings start on degrees 0 4 8 … 28, and string 6 takes F, not F♯). Hold a pad,
slide along a string to glide. The bank is drawn from the engine's own table (`#include
"wavescan_data.h"`), so the thumbnails ARE what plays, with the scan cursor between the two shapes it
is crossfading. It boots playing an arpeggio while SCAN sweeps, until you touch it.

## 4 · Still open

### 4.1 `INSTR_GRAIN` (engine 37): a granular voice per note, shipped
From the sampler branch of `RunVoice()`. Each voice runs **two grain chains**. A chain holds an OLD grain
and a NEW one and crossfades old into new over one grain length (upstream's `vol24` ramp, linear); when the
fade ends, new becomes old and a fresh grain starts. So every grain lives two lengths under a triangle,
and two chains overlap. It reads the buffer bound with `instrument_sample()` over the chop from
`instrument_sample_region()` (read live, INSTR_SAMPLE's rule), looping inside it.

- **Macros**: harmonics = POSITION in the region (live: `note_harmonics(handle, x)` scrubs ONE note,
  which is Plinky's finger-on-a-strip) · timbre = SIZE (5 ms … 1 s, exponential, 0.5 = 70 ms) · morph =
  SPEED of the playhead (0 = frozen, 0.5 = original, 1 = double). **Aux** (note-on): `MODE_GRAIN_SCATTER`
  (forward position jitter up to a grain + 0.26 s, upstream's `(grainsize + 8192)`), `_DETUNE` (upward pitch
  jitter up to an octave, squared like upstream's `gratejit²`), `_REVERSE`.
- `instrument_playhead()` now also reports a grain voice's playhead, so a cart can draw each finger.

**Changes from upstream, each deliberate:**
- **SPEED is independent of PITCH.** Upstream's playhead moves at rate × pitch × time-stretch, so time
  rides pitch like tape. Ours keeps a sample's timing at any note, which is the point of a granular voice.
- **Grains are CENTRED on the playhead.** A grain read faster than the playhead outruns it for its whole
  life, so you hear the future. Each grain now starts `(rate − speed) × one grain` behind. Measured before
  the fix: an octave up read **2.3×** and the sweep wrapped 0.1 s early. After: **2.000×**, on time. At 1× on
  the root the offset is zero.
- **The second chain starts half a grain late**, so the two chains' window dips interleave (upstream starts
  them together and relies on size jitter).
- **A 2 ms fade at the loop seam.** `record_grab()` cuts a buffer anywhere, so its end and start rarely
  match; a grain crossing the wrap would jump. It removes the seam click the plain `INSTR_SAMPLE` loop has
  (below).
- **Always loops inside the region** (upstream is silent outside it unless looping). No size jitter and no
  sampler-side low-pass gate (upstream has both). The two chains are summed mono.

**Evidence (2026-10-01), `grainprobe` → `tools/grain-check.js`** (a 0.5 s 200 → 800 Hz sweep as the sample, because a sweep's pitch
IS its position: a steady tone can't tell a moving playhead from a stopped one):

| test | result |
|---|---|
| T1 GRAIN 1× on the root vs T0 `INSTR_SAMPLE` loop | correlation **0.996**, RMS ratio 0.997: at 1× the voice IS the sample (the seam fade is the 0.4%) |
| T2 FROZEN mid-sweep | pitch holds 450-550 Hz (sd 36 Hz vs 171 Hz moving); the residue is the 70 ms of sweep inside one grain |
| T3 an octave up at 1× | pitch ratio to T1 **2.000** (median), wraps at the same 0.5 s |
| T4 scatter 0.6 + detune 0.4 | finite, peak 0.086 |
| click-check | nothing inside any grain body; the only flags are note-ons from silence (<1% of peak, a ratio over ~0 RMS) and T0's own seam (26.7% of peak), which T1 does NOT reproduce |
| soundcheck 900 frames | silent (slot 15 over a generated buffer: half speed, scatter, detune, reverse) |

In `grainstrings` the demo's worst click-check events (4.3-5.0×) match the SOURCE's own (4.9× at its mallet
strikes, before capture): grains replaying sharp attacks, not splices. Changing the seam fade moved none.

**Not measured:** CPU (four interpolated reads per sample per voice, cheap) and level against the library
sweep (the tune/level sweep has no sample to play; T1 shows it sits at the sample's own level).

**The `grainstrings` cart.** The console sings a chord (VOICE "ah" + MALLET), `record_grab()`s its own
output into sample 0, and hands it over on eight strings tuned to a pentatonic, one INSTR_GRAIN slot each
(so each string shows its own playhead). Height on a string is position in the sound; drag to scrub. SPEED
down to FROZEN holds one instant as a chord. RESAMPLE sings a new phrase; MIC records three seconds of you.

### 4.2 Multisample round-robin
On a trigger, pick the slice whose root note is nearest the played note, breaking ties toward the one
used longest ago. That's cart-land logic over `instrument_sample()` slots unless a cart needs it in the
engine.

### 4.3 The playing surface
Pressure → every parameter (Plinky's mod matrix: each param has a base + per-source amounts for
pressure, envelope, A/B/X/Y LFOs, random). We have the destinations (`note_vol/_cutoff/_harmonics/…`)
but no pressure source on a mouse, and no "every knob takes a modulation amount" pattern. Worth a design
note of its own before a cart hand-rolls it. Also compare its arp (probability + Euclid) with `patgen.h`.

### 4.4 The CHOMPI tables into the same engine
[open-source-audio-toys.md](open-source-audio-toys.md) §4.1 measured CHOMPI's 7 MIT tables × 33 frames
at ≤64 harmonics, ~118 KB at 256 samples a frame, and said they need "a real multi-frame wavetable voice".
`INSTR_WAVESCAN` is that voice: a second bank (or a bank-select aux) is data plus a generator, not new DSP.
Their mip levels would come from the same kernel.

## 5 · Reproduce

```bash
node tools/grain-check.js                   # the INSTR_GRAIN gate: renders grainprobe, asserts T1-T4
node tools/gen-wavescan.js --selfcheck      # the table oracle (needs no clone: known answers are embedded)
node tools/gen-wavescan.js --report         # centroids per shape
node tools/tune-check.js --engine WAVESCAN --macros 0.45,0.6,0.8 --range 36-96 --step 6
node tools/level-check.js --quiet
node tools/spec.js wavescan
```
