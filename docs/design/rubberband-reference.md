# Rubber Band Library as a reference (not as a dependency)

STATUS: LIVING (2026-07-26) — a reference note, not a plan. Read of **Rubber Band Library 4.0.0**
(Chris Cannam / Breakfast Quay, the industry-standard time-stretch + pitch-shift library) to answer
one question: are its algorithms useful to this engine? Verdict: **not as code** (GPL, and its
architecture is the opposite of `sound.h`'s), **yes as a reference** on three specific points, one of
which is immediately actionable. Feeds [`transparent-autotune.md`](transparent-autotune.md) §(b) and
[`contemporary-rebirth.md`](contemporary-rebirth.md) GAP 1 / GAP 3.

The source read was a local tarball (`~/Downloads/rubberband-4.0.0`, from
[breakfastquay.com/rubberband](https://breakfastquay.com/rubberband/)). It is **not vendored into this
repo and should not be** (see the licence section). Line references below are to that tree.

## 1. The licence is the headline

`COPYING` is GNU GPL v2-or-later. The README is unusually explicit about our exact distribution
channel:

> "If you wish to distribute code using Rubber Band Library under terms other than those of the GNU
> General Public License, you must obtain a commercial licence from us before doing so. In particular,
> **you may not legally distribute through any Apple App Store** unless you have a commercial licence."

We ship to the App Store (`apps/`, [`tools/asc-push.js`](../../tools/asc-push.js), `ios/`). So:

- **Linking the library: out.** Not from a cart, not from the runtime.
- **Copying code out of `src/finer/` or `src/common/`: out.** Those are Cannam's own files.
- **Close transcription: also out.** Retyping a GPL function into C with renamed variables is a
  derivative work. The rule for this repo: read it for the *idea*, then implement from the published
  algorithm, from our own primitives, in our own structure.

Most of what makes Rubber Band good is published in the literature anyway (cepstral liftering,
phase-locked vocoders, median-filter harmonic/percussive separation). The library is a *very well
tuned* assembly of known parts, and the tuning is the part we cannot take.

### 1a. What in that tree IS reusable

Three vendored third-party directories carry permissive licences and are independently droppable:

| Path | Licence | Why we might care |
|---|---|---|
| `src/ext/kissfft/` | BSD-3-Clause | `runtime/sound.h` contains **no FFT at all** (verified: zero matches). `kiss_fftr.c` is a real-input FFT, no dependencies, allocation only at plan time. **Every frequency-domain idea in section 2 is gated behind this one step.** |
| `src/ext/speex/resample.c` | BSD (Xiph) | Band-limited variable-ratio sinc resampler. Relevant only if `varispeed()` / sample repitch ever needs better than LERP. |
| `src/ext/pommier/` | BSD-like | Vectorised exp/log/sin for SSE and NEON. |

Careful: **`src/common/BQResampler.cpp` is Cannam's own code and therefore GPL.** The speex one is
the permissive resampler in that tree, not the BQ one.

## 2. The algorithms, mapped to our open gaps

Rubber Band is a **phase vocoder**. Our pitch machinery
([`sound.h:7289` `at_psola_slot`](../../runtime/sound.h)) is **time-domain PSOLA**. So it does not
hand us a drop-in answer to anything, but three pieces bear directly on logged gaps.

### 2a. The formant scale is one number, not a mode (→ GAP 3, actionable)

`R3Stretcher.cpp:1454` `analyseFormant` is textbook cepstral liftering: log-magnitude spectrum,
inverse FFT, keep the first `cutoff` cepstral coefficients (discard the rest), forward FFT, `exp`, and
the result is the spectral envelope. `adjustFormant` (`:1490`) then rescales each bin by
`envelope(i · sourceFactor) / envelope(i · targetFactor)`.

The load-bearing line is `:1504`:

```c
if (formantScale == 0.0) formantScale = 1.0 / m_pitchScale;
```

That is the whole insight. **"Preserve formants" is not a mode, it is `formantScale = 1/pitchScale`.**
Set it to `pitchScale` and you get chipmunk. Set it anywhere in between and you get a continuous dial.
Rubber Band exposes exactly that as a public `setFormantScale(double)`.

[`contemporary-rebirth.md`](contemporary-rebirth.md) §2b logs **GAP 3** as "the dial between chipmunk
and transparent is missing", and Rung B's parked half is the measured reason why: re-spacing epochs
while holding the grain content (`fstep` = 1) does hold the formants, but the pitch stays unstable at
every interval, because a grain that carries the source periodicity still sounds at the source pitch
however you re-space it.

**Rubber Band's structure is the answer to exactly that, and it is an architectural point, not a
tuning one: it treats pitch and envelope as two independent operations in sequence.**

1. **Move the pitch for real**, by shifting/resampling. The spectral envelope moves along with it.
   That is accepted, not fought.
2. **Then rescale the envelope back** by a separate factor, multiplying each bin by
   `envelope(i·sourceFactor) / envelope(i·targetFactor)`.

Our `sample_shift` has step 1 and it works (f0 within 0.5 Hz at every interval, measured in
[`voxshift`](../../tools/carts/voxshift.c)). It has **no step 2 at all.** That is why the only lever
available was `fstep`, which conflates the two: turning it down to hold formants also undoes the pitch
move. The dial cannot be built out of `fstep` alone, because `fstep` is step 1's knob.

So the honest cost of GAP 3's dial is **an envelope-rescale stage**, and that is where it gets
expensive: Rubber Band does it with a cepstrum, which means an FFT. The time-domain alternatives are a
short-time LPC fit or a time-varying filter bank, both of which are real work. This is worth knowing
*before* the next attempt, because it reframes GAP 3 from "plumb a parameter through
`at_psola_slot`'s existing `formant` argument" (which is what `0.0f` at
[`sound.h:7447`](../../runtime/sound.h) invites) into "add a spectral-envelope stage, then the dial is
one scalar over it".

And when that stage exists, the dial is trivial and should be spelled Rubber Band's way: a single
scalar, `0` meaning "derive the preserving default `1/pitchScale`", any other value giving chipmunk
(`= pitchScale`), transparent, or the interesting territory between. Not a mode enum.

#### 2a-bis. The obvious cheap objection, tested and refused (2026-07-26)

**The claim above was challenged and it held.** Worth recording because the objection is the one any
DSP-literate reader will raise, and answering it from the armchair is how you end up buying the wrong
thing.

*The objection:* classic TD-PSOLA is supposed to be formant-preserving **by construction** — the pitch
move comes from re-spacing the epochs (`Tt`), while `fstep` only resamples the grain *content*. So
`fstep = 1` plus re-spacing ought to move the pitch and hold the envelope with no envelope stage at
all, making §2a's two-stage architecture (and its cost) unnecessary. Supporting evidence looked strong:
`sample_autotune` runs at `fstep = 1` permanently and demonstrably works, holding F1 at 560 Hz while
flattening a 6.6 Hz wobble to 1.1 Hz.

*The test:* wire `at_psola_slot`'s unused `formant` argument to
`fstep = powf(2, (semis/12) * (1 - formant))` and force `formant = 1`, so `fstep` becomes exactly 1
while the epoch re-spacing still targets the shifted period. Run `voxshift`. Crucially this was run
**after** the same day's two grain-geometry fixes (the down-shift overlap clamp and the LERPed
fractional read), so the substrate no longer has the bugs the original parked experiment ran on — which
was the whole substance of the objection.

*The result:* refused, and the down-octave case refutes it cleanly.

| take (`formant` = 1, so `fstep` = 1) | f0 | F1 | F2 |
|---|---|---|---|
| RAW (control) | 220.5 | 560 | 991 |
| **DOWN−12** | **220.5** (unmoved) | **560** | **991** |
| UP+12 | 202.2 (wobble 77 Hz) | 764 | 958 |

The envelope was held *perfectly* and **the pitch did not move at all**. Not degraded, not unstable:
unmoved. So `fstep` is not merely *one* lever that moves pitch, it is the *only* one, and §2a's
sentence "turning it down to hold formants also undoes the pitch move" is literally what happens.

*Why `sample_autotune` misleads:* its corrections are fractions of a semitone, where source and target
periodicity sit within a few percent and the grain's own periodicity is close enough to pass. At 2:1
the source periodicity dominates and wins outright. **A working formant-preserving corrector at ±1
semitone is not evidence of a working formant-preserving shifter at ±12.**

*What this settles:* GAP 3 really is "add an envelope-rescale stage", not "wire a parameter", and the
`formant` argument sitting unused in `at_psola_slot` is a trap rather than a half-built feature. §3 is
still right that the stage need not be an FFT (LPC or a filter bank are the time-domain routes), so
"expensive" here means *a new stage*, not *a phase vocoder*.

### 2b. Phase continuity confirms the parked epoch spike (→ Rung B, no shortcut)

The comment at [`sound.h:7405`](../../runtime/sound.h) diagnoses the residual clicks in
`sample_shift` at non-integer ratios: the analysis epoch is picked as the *nearest* one to a
free-running pointer, so pulse phase is only approximately continuous, which is why -7 (157 clicks) is
worse than -12 (120) even though -12 is the bigger shift. The prescribed fix is a phase-continuous
analysis pointer, parked as a spike.

`PhaseAdvance.h` `GuidedPhaseAdvance` is built on precisely that discipline in the frequency domain:
output phase advances from the **previous output phase**, never re-derived from whichever source frame
happens to be nearest, and peak-locked bins are carried as a group so a partial and its neighbours
stay coherent. Different domain, same law.

**This is corroboration, not a shortcut.** Nothing in Rubber Band shortens the PSOLA rewrite. It does
raise our confidence that the parked diagnosis is the real one and worth doing properly rather than
patching grain widths again.

### 2c. StretchCalculator is the portable idea for halftime (→ GAP 1)

[`contemporary-rebirth.md`](contemporary-rebirth.md) §2a logs **GAP 1**: nothing time-stretches the
mix, because `varispeed(0.5)` couples pitch and time (it is a tape).

`src/common/StretchCalculator.cpp` (791 lines) is the piece worth studying, and the valuable part is
not DSP at all: it decides **where in the signal to spend the stretch**, driven by an onset curve, so
transients keep their original timing and the sustained regions absorb the change. Stretch uniformly
and a drum break smears; stretch non-uniformly against onsets and it survives.

That idea transfers onto our existing grain machinery without an FFT, and it is far cheaper than the
alternative. **Reimplementing `R3Stretcher` is not realistic and should not be attempted**: Guide +
BinSegmenter + BinClassifier + PhaseAdvance + StretchCalculator is roughly 4,000 lines of templated
C++ with per-frame `std::map` lookups, `shared_ptr`, allocators and threads. `sound.h` is plain C with
no allocation on the audio thread. The impedance mismatch is total.

### 2d. Two more worth knowing exist

- **`BinClassifier.h`** splits every bin into Harmonic / Percussive / Residual using a **moving median
  along time** (steady content survives it, so it reads harmonic) and a **moving median along
  frequency** (broadband content survives, so it reads percussive). This is the standard HPSS trick.
  It would be useful to us well beyond stretching, as an honest transient detector for `multiband()`
  and `sidechain()`. Needs an FFT.
- **`R3LiveShifter`** (new in 4.0, `R3LiveShifter.cpp`, 1202 lines) is the low-latency fixed-ratio
  shifter: the frequency-domain answer to our `autotune_mic`. Its trick is combining resampling with
  the vocoder to keep a fixed block size. Relevant only if the live path escalates to a vocoder, which
  [`transparent-autotune.md`](transparent-autotune.md) §(b) already decided to defer.
- Design detail worth borrowing if we ever do build a vocoder: `Guide.h` runs **three different FFT
  sizes across frequency bands** (longest window at low frequencies for resolution, shortest at high
  for time response, `:176-190`) and **four phase-lock bands** whose peak-neighbourhood width widens
  with frequency (`p` = 1, 2, 5, ..., `:239+`). A single FFT size is a compromise everywhere at once.

## 3. The FFT question, stated plainly

`sound.h` has no FFT. Sections 2c (partially), 2d and Rubber Band's entire approach sit behind adding
one. That is a real architectural step for this engine: a real-input FFT on the audio thread, plan
allocated up front, plus the discipline that comes with frame-based rather than sample-based
processing. `transparent-autotune.md` already concluded PSOLA-first, vocoder-only-if-forced, and this
read does not overturn that. **Nothing in the currently open gaps requires an FFT**, and 2a in
particular is cheaper without one.

## 4. What NOT to do with this

- Do not vendor the tarball, or any file from `src/finer/` or `src/common/`, into this repo.
- Do not "port" a Rubber Band function to C. Implement from the published algorithm instead, and if a
  file was open while writing, that is a signal to stop and re-derive.
- Do not treat this note as a reason to build a phase vocoder. It is a reason to be more confident
  about three things we already had planned in the time domain.
