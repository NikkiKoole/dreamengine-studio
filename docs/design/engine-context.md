# The engine context — giving `sound.h` and `studio.c` per-instance state

> **STATUS: building. `sound.h` IS DONE and byte-identical** — 327 of its 340 statics live in the
> context struct; `node tools/engine-statics.js` reads **13** for that file, and every one of the 13
> is a recorded decision (1 shared, 5 harness, 5 deferred, 2 dead-weight), not a leftover. Next:
> `studio.c` (222). Lane: [`HANDOFF.md`](../HANDOFF.md) → the AUv3 thread.
> Sibling docs: [`engine-instance-seam.md`](engine-instance-seam.md) (the host-facing handle — DESIGN THAT FIRST,
> `studio.c`'s move is only correct if shaped for it), [`ios-plan.md`](ios-plan.md) (how the AUv3 got here),
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
| `runtime/sound.h` | 340 | 31 | 3 |
| `runtime/studio.c` | 222 | 43 | 2 |
| others | 39 | 2 | 2 |
| **total** | **601** | **76** | **7** |

⚠ **These figures replace an earlier 545 / 293, which this tool itself got wrong.** clang's AST dump
prints a filename only when it CHANGES, so every later entry inherits the last one — and when that
inheritance goes stale a declaration is attributed to whatever file was named last (`sound_bpm` came
out living in `stdbool.h`). Such a row simply falls outside the engine file set: the count looks
plausible and the variable is never processed. The tool now VERIFIES each row against the source
before believing it, and an independent source-side count agrees (338 vs 340 for `sound.h`, where
before it did not). **The third undercount of this refactor, after the original grep and the
`#define` collision check.**

`#define` collisions across the whole translation unit: **5** — `rvb_tank` and `delayed` (both since
renamed), plus `band_x/y/w/h` and `palette` still ahead in `studio.c`. An earlier "2" missed the
STRUCT FIELD case entirely: the preprocessor does not know about `->`, so a same-named field turns
`ins->rvb_tank` into `ins->(de_snd->rvb_tank)` and will not compile. Re-check before each file.

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
default is per-instance** — the large majority of the 340 are ordinary audio state — so only exceptions are
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


## ⚠ `studio.c` is NOT a repeat of `sound.h`

`sound.h` was almost entirely per-instance audio state, so a macro move finished the job. `studio.c`
is different in two ways, and the second one changes what "done" means.

**It holds real shared state.** GPU handles (the sprite sheet, six font atlases, three shader
programs and their uniform locations), open OS file handles, the process `argv`. Duplicating any of
those is N GPU allocations at best and a handle freed while a sibling samples it at worst. The
classification records ~123 exceptions for this file, against 13 for `sound.h`.

**And the platform seam has no instance argument.** This is the finding that matters. The AUv3 today
runs ONE process-wide frame worker (its own comment: *"ONE worker per process, not per instance"*)
and calls `de_frame(t)` with no instance parameter — as do `de_resize`, `de_copy_frame`,
`de_set_safe_area`, `de_set_backing_scale`, `de_audio_render` and `de_set_save_dir`. **Once
`studio.c`'s state is per-instance, one `de_frame()` per tick advances exactly one rack and the
others freeze.**

The dangerous part is that **the macro move compiles and passes every byte-exact gate while leaving
this broken**, because `refactor-guard` runs a single instance. So the mechanical pass is a
prerequisite, not a completion.

**Two things NOT to reach for when fixing the seam.** A mutable global "current instance" pointer
would be written by the UI thread and read by the audio thread on every call — precisely the race the
pending/seqlock machinery exists to prevent, reintroduced one level up. And **a thread-local pointer
does not work here either**, which is a correction to the shape chosen for `sound.h`: the same
instance is touched from THREE threads (UI thread resizes, the frame worker draws, the XPC/view
thread copies the frame), and one worker serves many instances. There is no "current instance" for a
thread to hold. **The seam must take an explicit handle.** Concretely `de_copy_frame(dst, cap, &w, &h)`
has no instance parameter at all, and its reader must load `de_pres_buf` from the same context it
loaded `de_pres_seq` from or the seqlock means nothing.

**→ The shape is now designed: [`engine-instance-seam.md`](engine-instance-seam.md).** An explicit
`DeInstance *` at every seam function, plus a thread-local that is set and restored *within* a seam
call so the cart API and the macro accesses need no change. Build the handle BEFORE moving
`studio.c`'s state.

**A signal handler cannot reach a context either.** `crash_handler` is registered with the OS and
reads `watches`/`watch_count` to dump them; a handler takes no argument. Either keep a static
"last active instance" for it to walk, or drop the watch dump from the crash path. Design, not macro.


## Cart-land: the same problem, one layer up

The engine's state is per-instance, but a cart is ONE translation unit, so two instances of one cart
share everything the cart and its headers declare `static`. Measured on `acidcandy`: **209 statics in
its TU** — 120 the cart's own, the rest in `ui.h`, `tr808.h`, `tr909.h`, `cursor.h` and friends. A
rack whose widget table is shared is not a second rack.

Route **(b)** was chosen over generating transformed copies at build time, because with ~20 AUv3 apps
planned the plug-in build IS the product, and a permanent gap between the code you read and the code
that ships is a comprehension tax paid forever. See
[`engine-instance-seam.md`](engine-instance-seam.md) for the three routes and their costs.

### The pattern, proven on `ui.h`

Declare the header's state ONCE, as a list, and expand it two ways:

```c
#define UI_STATE(X)                               \
    X(UiWid, ui_wids,   [UI_MAX_WID], {0})        \
    X(int,   ui_wid_n,  ,            0)           \
    …

#ifndef DE_CART_CTX
#define UI_DECL_(t, n, d, i) static t n d = i;    /* DEFAULT: exactly the statics that were here */
UI_STATE(UI_DECL_)
#else
typedef struct { … } UiCtx;                       /* OPTED IN: the same list, as a context */
static char ui_ctx_key_;                          /* the key is an ADDRESS: unique per TU */
static UiCtx *ui_ctx_(void) { … de_state_for(&ui_ctx_key_, sizeof(UiCtx)) … }
#define ui_wid_n (ui_ctx_()->ui_wid_n)            /* no call site in the file changes */
#endif
```

**Why it costs the other 552 carts nothing:** the default expansion is the declarations that were
there before, so a single-instance cart compiles to the same thing. Verified: 580/580 carts build and
`refactor-guard` is byte-identical.

**Where the state lives.** `de_state()` is one block per instance, which is right for a cart but not
enough for the headers it includes — and they cannot be aggregated into the cart's struct without an
include-ordering knot, since the struct would need types the headers have not declared yet. So
`de_state_for(key, bytes)` carves a slice of that same block, keyed by the ADDRESS of a file-scope
sentinel: unique per TU by construction, no slot numbers, no registry, and nothing new to make
per-instance because the arena lives inside the per-instance block it carves up.
⚠ **Never cache what it returns** — registering another key can grow the block and move every slice,
which is why the access macros re-fetch every time.

**Gated by `bash tools/instance-check/run-uictx.sh`**, which builds the probe TWICE and asserts
OPPOSITE things: the default path must stay shared, the opted-in path must not. A seam checked only
in its enabled state is half a seam, and the half nobody checks is the one every cart uses.

### Doing the next header

`runtime/cart_ctx.h` holds the shared half: `DE_CTX_STATICS(LIST)` for the default path and
`DE_CTX_BLOCK(lc, Uc, LIST)` for the struct + key + accessor. A header supplies its list, its
`#define name (lc_ctx_()->name)` lines (the preprocessor cannot generate those), and an init for any
NON-ZERO default — `de_state_for` zero-fills, so most headers need none.

⚠ **Put the init DEFINITION above the access macros.** After them the member names are macros, so
`c->dk_base` expands to `c->(dk_ctx_()->dk_base)` and will not compile. `DE_CTX_BLOCK` forward-declares
it for exactly this reason.

**HOW MANY HEADERS ACTUALLY NEED THIS — measured, after three wrong answers.** 30 cart-land headers
exist; **8 hold mutable file-scope state**. The other 22 are pure functions and tables, or keep their
state in a struct the CART owns (`acid303.h` is the model — it was multi-instance-safe by design).

| header | statics | status |
|---|---|---|
| `keybed.h` | 15 | todo |
| `solo.h` | ~10 | todo |
| `gestures.h` · `radio.h` · `tr808.h` | ~6 each | todo |
| `ui.h` | 28 | **done** |
| `cursor.h` | 3 | **done** |
| `drumkit.h` · `tr909.h` | 1 each | **done** |

⚠ **ENUMERATE WITH THE COMPILER, NOT A REGEX.** Getting a header's static list wrong leaves
declarations behind that then collide with the access macros, and it fails as
*"invalid storage class specifier in function declarator"*, which points nowhere near the cause. My
AST attribution missed two of `solo.h`'s and my source scan counted function PARAMETERS as
declarations — the fourth and fifth bad counts of this refactor, both from not asking clang.
`tools/engine-statics.js` has the validated approach (AST + source-verified attribution); point it at
the header's TU rather than writing another scan.

Remaining after the headers: the cart's own 120 statics. Only then do two racks actually work — this
proves the mechanism, not the product.

## Memory

`size -m` on a compiled `studio.o`: the engine's mutable statics are **6.2 MB**, and a context is
exactly that per instance. The dylib fallback was once argued against on memory grounds; that
argument never discriminated, since it costs the same.

What the struct buys instead is control. **~3.4 MB of the 6.2 is in large buffers** that an idle
instance never touches — the grain pool at 1.0 MB, varispeed tape at 689 KB, the two echo lines at
353 KB each, the voice pool at ~320 KB, the cart-context log at 288 KB. Allocated on demand, an
instance costs 1–2 MB rather than 6.

## Open questions

All four are recorded in [`ctx-classification.json`](../../tools/ctx-classification.json). **Three are
now closed by reading the code, and none of them blocks work.**

**The mic path — CLOSED, and the answer is "not applicable yet".** The plug-in has no mic path at
all: the mic host is `ios/Sources/AudioEngine.swift` + `CanvasView.swift`, which are the STANDALONE
app, and nothing in `ios/AU/TinyjamAU.swift` opens or pushes a mic. Nor could it be per-instance if
it did — we register as **`aumu`, an instrument**, and an instrument AU is a generator that the host
hands no audio, so there is no per-instance input bus; and `mic.h` is *device-free by design* (its own
header says the engine never opens a capture device), so the engine could not open N of them. Mic
permission is granted per host APP, not per instance. **So the `extin_*` group stays shared, which is
the status quo.** If it ever arrives it will be one process-wide capture: shared ring and write
cursor, per-instance read cursor, and `extin_on` as a REFCOUNT rather than a bool.
*The real question hiding behind it is a product one:* should the rack also PROCESS host audio (an
`aufx` effect) rather than only generate? That is the only thing that would create a per-instance
input bus, and it is not a refactor decision.

**`sound_synth_mode` — CLOSED.** `de_init()`, the platform seam the plug-in and iOS both enter, sets
it `true` unconditionally (`studio.c:3015`) and never creates a stream; the host pulls
`de_audio_render` on its own thread. It is a constant in that build, identical for every instance, and
`sound_stream` is dead code there. Stays shared, correctly.

**The diagnostic counters — DECIDED, per-instance** (already moved). Low stakes: a few bytes, and each
instance reporting its own dropped notes beats a shared tally. Their siblings (`bow_body_overflow`,
`rvb_bus_overflow`, `grain_overflow`) are already per-instance pool bookkeeping reset in `sound_init`.

**Still genuinely open: cart switching.** Can two instances hold two DIFFERENT carts and switch
independently? The 288 KB `ctx_log` has already moved to per-instance, which assumes yes. If cart
choice belongs to the umbrella app instead, that log belongs to the shell and we are paying 288 KB per
instance for nothing. Not blocking; revisit before the memory-trimming pass.

## Verification

Every step is gated by **`node tools/refactor-guard.js`** — a pure state move must be byte-identical
across audio, frames and the `watch()` trace, and the guard localizes any divergence. It is proven to
go red on a real `sound.h` edit. **A red is a bug in the refactor, never a new baseline.**

⚠ Its one blind spot is the reason the classification above exists: the guard runs ONE instance, so it
cannot see a variable that should have been per-instance and was not. `lfo_seed_ctr` is the worked
example. Those defects are caught by review and by the two-engine probe
(`tools/engine-dylib-spike/probe.c`), not by the byte-exact gate.

## What the `sound.h` pass taught (every one was a SILENT failure)

- **The include landed inside `#if defined(__SSE__)`.** "After the last `#include` in the first 200
  lines" put it in an x86-only block, so on arm64 it was never included and every moved name became an
  undeclared identifier. The generator now tracks preprocessor depth and only accepts a depth-0 include.
- **The probe passed four times without testing anything.** A quoted `#include "sound.h"` resolves
  relative to the *including file's* own directory before any `-I` is consulted, so compiling
  `runtime/studio.c` read `runtime/sound.h` no matter what `-I` said. `--probe` now compiles the COPY's
  `studio.c` and carries a `#error` sentinel proving it reached the generated header. **This is the
  same failure shape as a `sed` that silently fails to match** — a green that means nothing.
- **The measurement tool undercounted, twice, in ways that hid work rather than invented it.** The
  `#define` collision check never looked at struct FIELDS, so batch 2 hit `ins->rvb_tank` becoming
  `ins->(de_snd->rvb_tank)`. And stale file attribution in the AST parse hid 47 of `sound.h`'s
  statics, four of which a batch then reported as moved while leaving them behind. **The generator now
  refuses to run unless every static is either moved or explicitly skipped** — that accounting
  invariant is the only thing that can catch a silent drop, because `refactor-guard` cannot: a
  variable that did not move cannot change the output, so the gate stays green.
- **The generator swept in variables nobody had decided about.** `sound_synth_mode` and the whole
  `extin_*` mic group went into the struct because the classification listed them under
  *open questions* rather than as exclusions. Harmless in step A (one context) and wrong in step B.
  Hence the **`defer`** group: anything without a decision must be named, or it moves by default.

## Order

1. **`sound.h` — DONE**, byte-identical, 327 moved and 13 deliberately left.
2. `studio.c`.
3. `acidcandy`'s own statics → `de_state()`.
4. Thread the context through the platform seam; `TinyjamAU` makes one per instance and
   `bootEngineOnce`'s idempotence goes away (it exists only because instances shared an engine).
5. The three things a context does not fix: the Swift frame worker, the CoreMIDI virtual source name,
   and `save_bytes` (one `cart.blob` for N instances — `de_set_save_dir` already exists to scope it).
6. Gate with `engine-dylib-spike/probe.c`, whose assertions and negative control port unchanged once
   `dlopen` becomes a context call.
