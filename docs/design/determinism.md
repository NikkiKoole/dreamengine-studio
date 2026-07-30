# Float determinism — the same bits on every device (2026-07-30)

**Status: SHIPPED for the audio engine.** `runtime/sound.h` contains zero non-deterministic libm
calls and all 16 audio engines are bit-identical native-vs-wasm. The draw layer was already
covered for its non-rotated path; carts are NOT yet covered (see [Open](#open)).

This is the doc [`runtime/demath.h`](../../runtime/demath.h) points at. It explains why the engine
ships its own `sin`/`cos`/`exp`/`log`/`pow`/`tanh`/`atan2` instead of calling the system library,
and why `runtime/studio.h` opens with a compiler pragma.

## The problem

A fantasy console that wants replays, ghosts, or lockstep multiplayer needs the same cart to
compute the same numbers on every device. It also wants a cart to *sound* the same in the browser
as it does natively. Neither is free.

**IEEE 754 mandates correctly-rounded results for `+ - * /` and `sqrt`.** Those agree everywhere,
on every conforming platform, forever. **It says nothing about `sin`, `cos`, `tan`, `exp`, `log`,
`pow`, `tanh`, or `atan`.** Correctly rounding a transcendental is expensive (the table-maker's
dilemma: you cannot know in advance how many extra digits you need to round correctly), so no
production libm does it. Apple's libm, glibc, musl and emscripten's all differ by roughly 1 ULP,
and glibc will dispatch to different SIMD kernels depending on the CPU it detects at load time.

So: same source, same input, different bits.

### What is already safe, and what is not

Roughly half the math surface needs no help at all. Do **not** route these through `demath.h`:

| already bit-exact by spec | why |
|---|---|
| `sqrtf` | IEEE 754 mandates it correctly rounded, same as `+ - * /` |
| `fabsf` `copysignf` | sign-bit manipulation, no rounding |
| `floorf` `ceilf` `truncf` `roundf` | exact integer results |
| `fmodf` `fminf` `fmaxf` `ldexpf` | exact operations |

That is lucky: `sqrtf` is the single most common math call in cart code (distance checks), and it
was never a problem. The ones that needed replacing are the transcendentals.

## Two causes, not one

This bit the repo three times before it was understood, and the first two fixes each addressed
only half of it.

**Cause 1: libm is not specified to the bit.** Found by
[`det-probes/textrot.c`](../../tools/det-probes/README.md): per-pixel rotation broke on an 8px
glyph because `cosf`/`sinf` differed by 1 ULP across targets and that flipped a pixel. Patched at
the time by quantizing the rotation matrix to 1/4096, which works for a rotation matrix but does
not generalise (you cannot quantize your way out of a physics integrator calling `expf` every
step).

**Cause 2: fused multiply-add.** Modern CPUs compute `a*b+c` in one instruction, keeping the
product at full width instead of rounding it first. It is a *better* answer, just a different one.
Compilers emit it automatically for any `a*b+c` they see; clang's default is `-ffp-contract=on`.
**wasm has no scalar FMA instruction**, so a native build fuses where the web build cannot. This
affects the ordinary arithmetic throughout the DSP, not just the libm calls, which makes it the
larger of the two effects by call count.

### The A/B that settled it

Measured with `tools/web-audio-check.js`, native clang vs emcc, 16 engines:

| configuration | BOWED | other engines |
|---|---|---|
| libm + contract=on (the old default) | -4.4 dB, 93.2% of samples differ | 8 engines at 1-3 LSB |
| demath + contract=on | -1.1 dB, still diverging | still diverging |
| libm + contract=off | -6.0 dB, still diverging | still 1-4 LSB |
| **demath + contract=off** | **bit-identical** | **all 16 at 0 LSB** |

**Neither fix alone is sufficient. Together they are exact.** This is worth remembering, because
each fix on its own looks like it failed.

BOWED is the loudest case because its stick-slip friction is a nonlinear feedback oscillator with
sensitive dependence on initial conditions: a single-ULP difference at the excitation grows into a
visibly different micro-waveform. It is the canary, not a special case.

## What shipped

### `runtime/demath.h`

`de_sinf` `de_cosf` `de_tanf` `de_sin_turns` `de_cos_turns` `de_expf` `de_exp2f` `de_logf`
`de_log2f` `de_log10f` `de_powf` `de_tanhf` `de_sinhf` `de_atan2f`.

Built from only the correctly-rounded operations. Powers of two come from the exponent bits rather
than a library call. Results below the normal float range are flushed to zero on purpose, because
denormal handling is a per-CPU mode bit (SSE FTZ/DAZ, ARM FZ) and therefore not portable either.
Contraction is pinned off per function via `DE_NO_CONTRACT`.

Coefficients are relative-error least-squares fits. The check that they are right is that they
converge to the classical series: 2π, ln2, -1/3, 2/15, 2/ln2, and 1, -1/3, 1/5, -1/7 for `atan`.

Two design notes worth keeping:

- **`de_sin_turns` takes TURNS, not radians.** 50 of the 66 `sin`/`cos` call sites in `sound.h`
  were already `sinf(phase * SOUND_TWO_PI)` with a 0..1 phase accumulator. Taking turns directly
  makes the range reduction exact (`t - floor(t)`) with no π multiply to lose bits in. It is both
  more accurate *and* **45% faster than libm's `sinf`**, because it skips libm's general-purpose
  range reduction. Where a call site is genuinely radians, `de_sinf` wraps it.
- **Reduce in double, evaluate in float.** The wrap and the quadrant fold run in double, and only
  the final small angle is narrowed. Doing it the other way round costs the fold everything: near
  half a turn the fold subtracts two nearly-equal numbers, so a float input leaves the reduced
  angle with two correct digits. Double `+ - * /` are correctly rounded and mandated exactly as the
  float ones are, so this costs no determinism. (The old 80-bit x87 hazard does not apply: x86-64
  uses SSE, arm64 and wasm are true binary64.)

### The pragma in `studio.h`

```c
#pragma STDC FP_CONTRACT OFF
```

File-scope, on purpose. It applies to the rest of every translation unit that includes `studio.h`,
which is the engine, every cart, and the audio harness.

**Why a pragma and not a build flag:** there are **19 compiler invocation sites** in this repo. A
flag would eventually be missed on one, and a missed one fails *silently* — it still compiles, it
just quietly stops matching. The pragma travels with the header.

**Measured cost: +0.7%** (interleaved min-of-7; the DSP renders at ~45× realtime, so there is
enormous headroom). Naive single-shot benchmarks gave +6.2% and -4.5% on the same machine, i.e.
pure noise, so measure this interleaved if you ever revisit it.

**It does NOT override an explicit `-ffp-contract=fast`,** which `-ffast-math` turns on. That is
already a standing build rule here (never `-ffast-math`,
[`det-probes/README.md`](../../tools/det-probes/README.md)).

## Accuracy

Against a double-precision reference, in ULPs of float:

| function | max ULP | note |
|---|---|---|
| `de_log2f` `de_logf` | 0 | |
| `de_exp2f` `de_expf` `de_powf` `de_atan2f` | 1 | matches libm |
| `de_tanhf` `de_sinhf` | 2 | |
| `de_sin_turns` `de_cos_turns` `de_sinf` `de_tanf` | 4 | 2.4e-07 absolute, i.e. -132 dB |

The number that actually matters for audio: **MIDI note → Hz is off by 0.00034 cents**, against a
~1 cent perception threshold. Three thousand times below audible.

Two places where `demath` and libm disagree and **`demath` is the more correct one**, so do not
"fix" them:

- `de_sin_turns(-2.0)` returns exactly 0. libm returns -3.5e-07, because `-2 * 2π` rounds before
  `sinf` ever sees it. Taking turns avoids the multiply entirely.
- `de_atan2f(±0, -1)` returns `3.14159274`, libm returns `3.1415925`. `3.14159274` is the
  correctly-rounded float value of π; libm is one ULP low.

## How it stays true

[`tools/det-probes/demath.c`](../../tools/det-probes/README.md), wired into `det-probes/run.sh`.
It sweeps every function, hashes the raw bit pattern of ~2M results with FNV-1a, and the same
source compiled for arm64, x86-64 and wasm must print the same hash.

**The probe carries its own control.** A gate that passes could simply be insensitive, so the same
grid was run through libm: it prints **three different hashes** where `demath` prints one. That is
the whole thesis reproducible in one command.

`tools/web-audio-check.js` is the end-to-end gate: it compiles the real engine both ways and
demands bit-parity on all 16 engines.

## Open

- **Carts are not covered.** 834 calls to non-deterministic libm functions across 573 carts, mostly
  decorative (`sinf` for a title-screen wobble, which nobody will ever notice). The ones that would
  matter are the 15 carts carrying both a `spec()` gate and unsafe math, especially the geometry
  ones (`streetlab` `citydrive` `sloop`). The intended fix is to expose `de_*` through `studio.h`
  so a cart that needs determinism can opt in, leaving decorative use on libm.
- **`roadkit.h` and `citygen.h`** use trig for geometry that feeds pixel decisions, and `roadkit`
  has a spec-locked oracle (`streetlab`, 104/0). A 1-ULP shift on another platform could move that
  count and report a failure with no bug to find.
- **Not provided yet:** `asinf` `acosf` `atanf` (scalar), `coshf`, `hypotf`, `cbrtf`. Add them the
  same way if a call site appears.
- **Denormals** are flushed inside `demath.h`, but the engine at large does not set a consistent
  FTZ/DAZ mode. Not currently a known problem; worth knowing it is unpinned.

## See also

- [`web-audio-parity.md`](web-audio-parity.md) — the doc that predicted both causes under "Axis 1"
- [`software-canvas.md`](software-canvas.md) — goal B, the same requirement for the draw layer
- [`../../tools/det-probes/README.md`](../../tools/det-probes/README.md) — the probe suite
- [`multiplayer-research.md`](multiplayer-research.md) — lockstep, which is what this unlocks
