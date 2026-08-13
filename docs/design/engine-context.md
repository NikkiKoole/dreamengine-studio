# The engine context — giving `sound.h` and `studio.c` per-instance state

> **STATUS: building.** Guardrail, classification, generator and **batch 1 of `sound.h` are DONE and
> byte-identical** — 269 of its 293 statics now live in a context struct (`node tools/engine-statics.js`
> reads 23 for `sound.h`, down from 293). Batch 2 is the 13 typed declarations that need the type-hoist. Lane: [`HANDOFF.md`](../HANDOFF.md) → the AUv3 thread.
> Sibling docs: [`ios-plan.md`](ios-plan.md) (how the AUv3 got here),
> [`external-clock-sync.md`](external-clock-sync.md) (the transport seam it rides on).

## Why

An AUv3 puts **every instance of a plug-in in one process** — confirmed by an Apple engineer on the
developer forums, and there is no setting that changes it. Our engine keeps its state in file-scope
statics, so it is a singleton by construction: load the plug-in on two DAW tracks and both render
blocks drive one engine. That is the live defect ("the sound goes weird" on the second track).

Apple's own shipped Xcode Audio Unit Extension template holds every piece of DSP state as members of
a kernel object owned by the `AUAudioUnit` instance, and contains **zero** file-scope statics. So
per-instance state is what the platform expects; **the non-conforming part is ours.**

## Shape

One context struct. Access goes through a generated macro block, one line per member:

```c
#define echo_fb (ctx->echo_fb)
```

so the DSP code keeps reading exactly as it reads today and the thousands of use sites are never
touched. The public API (`note_on()` and friends) is called by carts and cannot grow a parameter
without changing every cart, so **a thread-local pointer lives at the door**; below that line `ctx`
is an explicit parameter.

**Measured, `bash tools/tls-spike/run.sh`:** the parameter costs nothing (+0.8%, inside noise); a
thread-local costs **+0.83 ns per opaque function entry** and nothing at all once a function inlines,
because clang hoists the lookup out of the loop rather than paying it per access. So performance does
not decide this. What decides it is that **the parameter is compiler-checked**: miss a call site and
it will not build, where a thread-local compiles fine and reads another instance's state at runtime.

The choice is reversible per function, because member access goes through the same macro either way.

## The counts

`node tools/engine-statics.js` (clang's AST, not a grep — the figure this replaced missed every
declaration with a trailing comment and undercounted 2.7×):

| | statics | non-zero initialisers | function-local |
|---|---|---|---|
| `runtime/sound.h` | 293 | 29 | 3 |
| `runtime/studio.c` | 213 | 40 | 2 |
| others | 39 | 2 | 2 |
| **total** | **545** | **71** | **7** |

`#define` collisions across the whole translation unit: **2** (`voices`, `palette`). That number is
what makes the macro approach viable, and it should be re-checked before each file.

## Layout: the type-hoist

The struct must be **complete before the first use of any member**, but the types its members need
(`Voice`, `ReverbTank`, `GrainTank`, …) are defined *interleaved* through `sound.h`, each immediately
before the statics that use it. So a single struct at one point is not automatically placeable.

The fix is to hoist the **transitive closure** of needed type definitions above the struct. Verified
on a throwaway copy of `runtime/`: the closure pulls in **14 types and 18 macros** — five types more
than the obvious list, including `SoundReqKind`, `OctaveUp` and `SoundBiquad` — and the engine
compiles clean. The generator computes that closure rather than carrying a hand-written list.

## The classification

Machine-readable: [`tools/ctx-classification.json`](../../tools/ctx-classification.json). **The
default is per-instance** — 250+ of the 293 are ordinary audio state — so only exceptions are
recorded. Produced by three parallel read-only audits, one per region of the file.

**Why this needed judgement rather than a script.** A byte-exact gate cannot check any of it: a
single-instance run is identical whether a variable is shared or not. It only shows up later as a
broken second instance. Four findings carried the audit:

- **`lfo_seed_ctr`** (`sound.h:5822`, inside `sound_setup_note`) is a per-voice-start counter whose
  stated purpose is to keep `--det` renders byte-reproducible. Left shared, two instances interleaving
  note starts **both** lose determinism — and `refactor-guard` would sit green through it. It is also
  function-local, so the macro cannot fix it; the declaration has to move.
- **The scope and record rings are public cart API**, not debug taps, despite sitting in the debug
  neighbourhood of the file directly after the WAV capture. `scope_read` and `record_arm`/`record_grab`
  are in `studio.h` and exported to cart code. Classifying them as harness would have quietly deleted
  a shipping feature.
- **Several "flags" are live DSP gates.** `drop_used`, `noise_used`, `vari_used`, `fxmod_any` read as
  telemetry but are bypass gates in the per-sample loop; sharing them lets one instance's effects
  switch on and off inside another. Likewise `shim_next` reads as a scratch iterator but is a pool
  allocator cursor — shared, the second instance asks for a shimmer tank and is refused because the
  first consumed the pool.
- **Atomic does not mean shared.** `req_head`/`req_tail` are `atomic_int` for main-thread-to-audio-thread
  ordering *within one engine*. Shared, the single-producer/single-consumer invariant itself breaks:
  two producers and two consumers on one ring means lost and duplicated events, not merely crosstalk.

## Memory

`size -m` on a compiled `studio.o`: the engine's mutable statics are **6.2 MB**, and a context is
exactly that per instance. The dylib fallback was once argued against on memory grounds; that
argument never discriminated, since it costs the same.

What the struct buys instead is control. **~3.4 MB of the 6.2 is in large buffers** that an idle
instance never touches — the grain pool at 1.0 MB, varispeed tape at 689 KB, the two echo lines at
353 KB each, the voice pool at ~320 KB, the cart-context log at 288 KB. Allocated on demand, an
instance costs 1–2 MB rather than 6.

## Open questions

Four, all in [`ctx-classification.json`](../../tools/ctx-classification.json) with what breaks if each
is guessed wrong. **None block the first batch.** The sharpest:

**The mic input path.** Does host audio arrive per instance (each AU render block's input bus) or from
one process-wide capture device? If process-wide, the ring stays shared and becomes one-producer /
N-consumer, and `extin_on` has to become "any instance wants the mic" — a refcount, not a bool. A
naive per-instance copy means the capture thread reads whichever copy the linker picked, and the mic
is silently dead on every other instance.

The others: whether two instances may hold different carts (decides where the 288 KB context log
lives), whether "harness" means debug-by-purpose or debug-by-build-guard (decides whether four
diagnostic counters ship inside the context), and whether the AUv3 build ever has a device stream at
all (may make `sound_synth_mode` a build-time constant).

## Verification

Every step is gated by **`node tools/refactor-guard.js`** — a pure state move must be byte-identical
across audio, frames and the `watch()` trace, and the guard localizes any divergence. It is proven to
go red on a real `sound.h` edit. **A red is a bug in the refactor, never a new baseline.**

⚠ Its one blind spot is the reason the classification above exists: the guard runs ONE instance, so it
cannot see a variable that should have been per-instance and was not. `lfo_seed_ctr` is the worked
example. Those defects are caught by review and by the two-engine probe
(`tools/engine-dylib-spike/probe.c`), not by the byte-exact gate.

## What batch 1 taught (all three were silent failures)

- **The include landed inside `#if defined(__SSE__)`.** "After the last `#include` in the first 200
  lines" put it in an x86-only block, so on arm64 it was never included and every moved name became an
  undeclared identifier. The generator now tracks preprocessor depth and only accepts a depth-0 include.
- **The probe passed four times without testing anything.** A quoted `#include "sound.h"` resolves
  relative to the *including file's* own directory before any `-I` is consulted, so compiling
  `runtime/studio.c` read `runtime/sound.h` no matter what `-I` said. `--probe` now compiles the COPY's
  `studio.c` and carries a `#error` sentinel proving it reached the generated header. **This is the
  same failure shape as a `sed` that silently fails to match** — a green that means nothing.
- **The generator swept in variables nobody had decided about.** `sound_synth_mode` and the whole
  `extin_*` mic group went into the struct because the classification listed them under
  *open questions* rather than as exclusions. Harmless in step A (one context) and wrong in step B.
  Hence the **`defer`** group: anything without a decision must be named, or it moves by default.

## Order

1. **`sound.h` alone** — self-contained, strongest oracle, clean bail-out if the pattern is wrong.
   **Batch 1 done** (269 primitive-typed statics, byte-identical); batch 2 = the 13 typed ones.
2. `studio.c`.
3. `acidcandy`'s own statics → `de_state()`.
4. Thread the context through the platform seam; `TinyjamAU` makes one per instance and
   `bootEngineOnce`'s idempotence goes away (it exists only because instances shared an engine).
5. The three things a context does not fix: the Swift frame worker, the CoreMIDI virtual source name,
   and `save_bytes` (one `cart.blob` for N instances — `de_set_save_dir` already exists to scope it).
6. Gate with `engine-dylib-spike/probe.c`, whose assertions and negative control port unchanged once
   `dlopen` becomes a context call.
