# The instance seam — giving the host a handle

> **STATUS: building — STEP 1 IS DONE** (2026-08-14). The handle exists and every host passes it;
> there is still exactly one instance, so nothing behaves differently yet. Steps 2-4 below remain. The prerequisite work is done:
> `sound.h`'s state is per-instance ([`engine-context.md`](engine-context.md)) and `studio.c`'s is
> classified. This doc decides the shape of the host-facing API *before* `studio.c`'s state moves,
> because the move is only correct if it is shaped for this.

## The problem, stated exactly

`runtime/platform.h` declares the whole host seam with **no instance argument**:

```c
void de_init(DeRenderer renderer);
void de_frame(double t);
int  de_copy_frame(uint32_t *dst, int cap_px, int *w, int *h);
void de_audio_render(float *out, int frames);
void de_resize(int w, int h);
void de_set_safe_area(int l, int t, int r, int b);
void de_set_backing_scale(float k);
void de_set_save_dir(const char *dir);
```

That is fine while the engine is a singleton. It stops being fine the moment `studio.c`'s state is
per-instance: **`de_frame()` would advance whichever engine the globals happen to name — one rack —
and every other rack would freeze.**

⚠ **And a byte-exact gate cannot see it.** `refactor-guard` runs one instance, so the state move
compiles, passes, and leaves the plug-in broken. This is the reason the seam is designed first.

## What the plug-in actually does today

Read from `ios/AU/TinyjamAU.swift`, not assumed:

| thread | calls | notes |
|---|---|---|
| **audio render block** | `de_audio_render`, and `de_frame` **when offline** (a bounce runs the frame inline) | per instance |
| **frame worker** `dreamengine.frame` | `de_frame` | **ONE per process** — its own comment says so — woken by a shared semaphore that *any* instance's render block signals |
| **host UI / layout** | `de_resize`, `de_set_safe_area`, `de_set_backing_scale`, `de_set_save_dir` | per instance |
| **XPC / view** | `de_copy_frame` | per instance |

Two consequences fall straight out. **One shared worker cannot drive N instances** — a signal carries
no identity, so the worker cannot know which rack to advance. And **the same instance is touched from
four different threads**, while one worker serves many instances.

## Decision 1 — an explicit opaque handle at the seam

```c
typedef struct DeInstance DeInstance;          // opaque to the host

DeInstance *de_instance_create(DeRenderer renderer);
void        de_instance_destroy(DeInstance *in);

void de_frame        (DeInstance *in, double t);
int  de_copy_frame   (DeInstance *in, uint32_t *dst, int cap_px, int *w, int *h);
void de_audio_render (DeInstance *in, float *out, int frames);
void de_resize       (DeInstance *in, int w, int h);
void de_set_safe_area(DeInstance *in, int l, int t, int r, int b);
void de_set_backing_scale(DeInstance *in, float k);
void de_set_save_dir (DeInstance *in, const char *dir);
int  de_screen_w     (DeInstance *in);
int  de_screen_h     (DeInstance *in);
void de_touch_begin  (DeInstance *in, int id, float x, float y);   /* …moved/ended, de_key_event */
```

**Why explicit and not a "current instance" global.** A mutable global would be written by the UI
thread and read by the audio thread on every call — precisely the race the existing pending-resize
and seqlock machinery was built to prevent, reintroduced one level up.

**Why not a thread-local at the seam either.** This is a correction to the shape chosen for
`sound.h`. A thread-local "current instance" assumes a thread works on one instance; here one worker
serves many, and one instance is served by four threads. There is no current instance for a thread to
hold. The handle has to be a parameter, and being a parameter makes it **compiler-checked**: a
missed call site does not build.

`de_copy_frame` is the sharp case. It has no instance parameter at all today, and its reader must
load `de_pres_buf` from the **same context** it loaded `de_pres_seq` from, or the seqlock guarantees
nothing.

## Decision 2 — a scoped thread-local, INSIDE the seam only

The handle solves the boundary. It does not solve the ~800 macro accesses (`de_snd->`, `de_vid->`)
or the entire cart API: `spr()`, `note_on()` and friends are called by cart code that cannot grow a
parameter without changing every cart in the repo.

So the two seam functions that actually run engine and cart code establish the context for the
duration of the call, and restore it on the way out:

```c
static _Thread_local DeInstance *de_cur;       /* NEVER read outside a seam call */

void de_frame(DeInstance *in, double t) {
    DeInstance *prev = de_cur; de_cur = in;
    de_snd = &in->snd;  de_vid = &in->vid;     /* the context pointers the macros expand through */
    …existing body, unchanged…
    de_cur = prev;
}
```

**Why this is not the global we just rejected.** It is per-thread, so the UI and audio threads never
write each other's slot. It is *scoped* — set on entry, restored on exit — so it never persists to be
read stale. And it is only ever consulted underneath a call that already knows its instance. A global
would be a shared mutable rendezvous between threads; this is a parameter, passed the only way C lets
you pass one to a function whose signature you cannot change.

Seam functions that do **not** run cart code — `de_copy_frame`, `de_resize`, `de_set_safe_area`,
`de_set_backing_scale` — use the handle directly and never touch `de_cur`.

## Decision 3 — split `de_init` into process init and instance create

`de_init` currently does both one-time process work and per-instance work. They have to separate,
because the classification says a large minority of `studio.c` is genuinely shared:

```c
void de_process_init(DeRenderer renderer);   /* ONCE per process: decode fonts, compile the three
                                                shaders, upload the sprite sheet. Idempotent. */
DeInstance *de_instance_create(DeRenderer);  /* per instance: allocate the contexts from the
                                                compile-time default templates, then run the cart's
                                                init() with de_cur set. */
```

`bootEngineOnce` in the Swift disappears: it exists *only* because instances shared an engine. What
remains of it is `de_process_init`, which is the honest version of the same idea.

Allocation is exact by construction: each context is a `memcpy` of the generated
`*_ctx_default` template, so a new instance starts in precisely the state a fresh process would.

## Decision 4 — one frame worker per instance

The shared worker cannot be kept: a semaphore signal carries no identity. Two shapes were considered.

**One worker per instance** (chosen). Each instance owns its thread and semaphore; its render block
signals its own worker; teardown is per-instance. A thread parked on a semaphore costs essentially
nothing, and the current comment's objection — *"tearing it down when one instance goes away would
strand the others mid-frame"* — evaporates, because nothing is shared to strand.

**One worker plus a lock-free queue of instances to advance** (rejected). It needs a new
audio-thread-safe queue for no benefit; the thing it saves is a parked thread.

This also preserves an invariant the audit flagged: `de_pres_cap` is unprotected and relies on only
one thread running `de_frame` **for a given instance** at a time. With per-instance workers that
still holds — realtime runs on the instance's worker, an offline bounce runs inline on the audio
thread, and the two never overlap for the same instance.

## Decision 5 — the signal handler keeps a static, deliberately

`crash_handler` is registered with the OS and reads `watches`/`watch_count` to dump them. A signal
handler takes no argument, so it cannot be handed a context. It keeps **one `static DeInstance *`
recording the last instance to enter `de_frame`**, and dumps that one's watches, labelled as such.
This is a deliberate exception, not an oversight: it is diagnostic output on a path that is already
crashing, and the alternative is dropping the watch dump entirely.

## What this does NOT fix

Named so the next session does not assume otherwise:

- **The CoreMIDI virtual source.** N instances would publish N sources with the same name. The handle
  makes per-instance naming *possible*; someone still has to decide the naming.
- **`save_dir`.** Per-instance `save_dir` is the mechanical fix and is strictly better than today's
  shared `cart.sav`. The right AUv3 answer is probably the AU's `fullState` so a saved session
  restores each rack — a product decision, out of scope here. **How this work relates to it**, since
  it is the obvious next question:
  - **Per-instance state is a PRECONDITION, not a coincidence.** `fullState` is defined over an
    instance; today the engine has no "this rack's state" to hand back, so it is not implementable.
  - **But the context struct is the WRONG thing to serialize**: it holds pointers and GPU handles,
    and most of it is derived DSP scratch (delay lines, filter memory, LFO phase) that is megabytes
    and meaningless to restore. `fullState` wants user INTENT.
  - **The right shape already exists in embryo.** `de_state()` is one contiguous zero-filled block
    holding a cart's whole state, `save_bytes`/`load_bytes` already serializes it, and **`ctx_log` is
    already a replay log of the cart's configuration calls** (built for `de_switch_cart`). So a
    restore has a natural shape: create a fresh instance from the generated default template, replay
    the config log, restore the `de_state()` block. Intent, not scratch.
  - **Ladder:** per-instance context (now) → per-instance `save_dir` → swap the file for a
    host-provided blob. Nothing built now needs undoing for it.
  - ⚠ **The rule it would impose:** `de_state()` is only serializable if carts keep no POINTERS in it.
    The `STATE{}` idiom encourages flat data but nothing enforces it; committing to this route means
    making that a rule, and probably a lint.
  - Note the plug-in does **no** state handling today at all (no `fullState`, no `parameterTree`, no
    presets), so a reopened session starts every rack at defaults — a separate user-visible gap from
    the two-racks-interfere bug.
- **`colorkey()`**, which destroys and rebuilds the shared sprite-sheet texture from a cart API.
  Needs its own decision (per-instance texture variant, or software-only).

## Order of work

1. **✅ DONE — the handle, with ONE instance.** `DeInstance` + `de_instance_create`/`_destroy`, the
   scoped `de_cur`, and every seam function takes a handle. All five hosts migrated: `headless-nr`,
   the three probes, and the Swift (`CanvasView`, `AudioEngine`, `GameHost`, `TinyjamAU`).
   `de_process_init` deliberately did NOT land: the shared-vs-per-instance split it formalises is
   `studio.c`'s work, so inventing it now would be a name with nothing behind it. Verified: the
   `DE_NO_RAYLIB` render is **byte-identical**, `refactor-guard` 6/6, spec 1986/0, `build-all`
   580/580, all three probes green **with their negative controls**, and the Mac Catalyst plug-in
   builds.
   **Three things step 1 taught:**
   · **`refactor-guard` is blind to this entire change** — the seam lives inside `#ifdef DE_NO_RAYLIB`
     and the guard builds the Raylib path. `build-nr.sh` + the probes are the gate here, and the
     guard staying green proves only that the *cart-facing* engine was untouched.
   · **`present-race-check` had been DEAD since `midi_output.h` landed** — it failed to LINK
     (CoreMIDI/CoreFoundation missing from its line), which in a log reads exactly like "not run".
     Fixed; it and its negative control both work again. A gate that cannot link is a gate that
     cannot fail.
   · **Engine-internal callers must not fake a handle.** `raylib_compat`'s `GetScreenWidth()` runs
     underneath a seam call that already established the instance, so it got
     `de_active_screen_w()` rather than a NULL argument — the handle-taking pair is the HOST seam,
     that pair is the engine asking about the canvas it is drawing.
2. **Move `studio.c`'s state** into `DeVideo` (`ctx-gen --target studio.c`), byte-identical.
3. **Switch the AUv3** to one instance per audio unit, one worker per instance; delete
   `bootEngineOnce`.
4. **Gate it.** `tools/engine-dylib-spike/probe.c` already drives two engines with different
   transport and asserts their frames and audio differ, with a shared-engine control that must come
   back byte-identical. Swapping its `dlopen` for `de_instance_create` ports the assertions
   unchanged — **that is the test the byte-exact gate cannot be**.

Step 1 is deliberately first and deliberately boring: it changes only the shape, on one engine, where
every existing gate still applies.
