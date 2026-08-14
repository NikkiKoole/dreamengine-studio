# The instance seam — giving the host a handle

> **STATUS: building — STEPS 1-3 ARE DONE** (2026-08-14). The handle exists, every host passes it,
> `studio.c`'s state moved, and the AUv3 creates one engine + one frame worker per audio unit.
> **What is LEFT: [`per-instance-remaining.md`](per-instance-remaining.md)** — the single checklist.
> The prerequisite work is done:
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
  - ✅ **BUILT 2026-08-14 — and the plan above was wrong in two places.** `de_save_state` /
    `de_load_state` ship (`runtime/platform.h`), gated by `bash tools/state-check/run.sh`. What the
    route above got wrong, both found by reading the code rather than trusting the plan:
    - **"Serialize the `de_state()` block" cannot be a verbatim copy, because the block STARTS WITH
      POINTERS.** `de_state_for`'s arena header sits at offset 0 and its `key` members are the
      addresses of file-scope sentinels — different in every process. Restoring it verbatim leaves
      every later `de_state_for` lookup comparing against garbage. The format therefore skips the
      arena entirely and matches slice payloads back by **registration index + size**, with a layout
      **fingerprint** that makes a blob from another build/architecture get REFUSED rather than
      copied into mismatched slices.
    - **The "no pointers" rule was ALREADY VIOLATED — by `ui.h`,** the header every rack cart
      includes (`ui_grab_evt` / `ui_rel_evt` are `void *` touch-event identities). And pointers were
      not even the worst of it: `keybed.h`, `solo.h` and `radio.h` hold **live voice handles**, which
      are plain `int`s and so invisible to any pointer lint, yet name voices in the instance that
      allocated them and nothing at all in the one a blob is restored into.
    - **So the split is per-slice and opt-in, with SCRATCH as the default.** `de_state_for` = not
      saved; `de_state_for_saved` = saved (`DE_CTX_BLOCK` / `DE_CTX_BLOCK_SAVED` in `cart_ctx.h`).
      That default direction is the whole safety argument: a header that forgets to mark itself
      *loses a setting*, which is a missing feature; the other default *restores a stale handle*,
      which is corruption. Fail towards the recoverable mistake.
    - Saved today: `acidcandy_state.h` (598 lines, zero pointers), `tr808.h`, `tr909.h`, `drumkit.h`
      (all three have cart-facing setters). Scratch: `ui.h`, `cursor.h`, `gestures.h`, `keybed.h`,
      `solo.h`, `radio.h`.
    - The sound half needed **one new request kind**, `SR_STATE_RESTORE`: `SR_CART_SWITCH` already
      does reset-then-replay but no-ops when the target context is the active one, and a session
      restore is always "the context you are already in". The **reset is load-bearing** — a config
      log holds append-only entries (a wavetable define, an auto-bus allocation) as well as keyed
      knobs, so replaying it over an engine that already ran `init()` would allocate a *second* bus
      instead of re-setting the first.
    - **The apply is DEFERRED to the top of the next `de_frame`.** The host sets state on its own
      thread while the frame worker runs, and the cart's state belongs to `de_frame`. So
      `de_load_state` returning 1 means *accepted*, and the rack changes one frame later.
    - **The lint the plan asked for is written: `tools/lint-saved-state.js`** (in repo-doctor, 10
      known answers both directions). Two tiers, because they are not equally strong: a POINTER is an
      ERROR (the type proves it), a live HANDLE is ADVISORY and matched by NAME, since a handle is a
      plain `int` that no static check can distinguish. **It found a real defect on its first run** —
      `acidcandy`'s `nav_poison[6]`, an array of widget pointers sitting inside the saved `CartState`,
      committed an hour before under a comment claiming the struct had no pointers. The claim came
      from grepping for X-list rows in a file that has no X-list, so "0 matches" meant "nothing to
      check", not "nothing wrong". Moved to a scratch slice.
    - **Slice granularity is all-or-nothing** — a cart that needs half its struct saved and half not
      has to declare *two* slices, one of each kind. That is the general limit, and it is real.
      ⚠ **But do not conclude from it that `acidcandy` has a playhead problem — I did, and it was
      wrong.** `CartState` does contain `g_phase` and `playing`, so they *are* written to the blob.
      They do not survive contact with a host, and that is by design:
      - `s_step` is **derived, not stored** — `s_step = ctr % STEPS` recomputes from `g_phase` every
        frame, so restoring it changes nothing.
      - **In a DAW the host owns the position.** An AUv3 pushes `de_sync_position` every render
        block, so `sync_active()` is always true, and the cart's own docblock states the consequence:
        *"While a clock is present it owns ALL THREE of tempo, transport and POSITION … `g_phase =
        sync_beats()*4` DERIVED instead of accumulated."* The restored `g_phase` is overwritten on the
        first frame. Same for `playing` whenever `sync_transport()` holds — and the host's PLAY edge
        resets `laststep`/`laststep303[]` itself.

      So for `fullState`, which is the DAW case and the whole point, a reopened project sits at the
      **host's** playhead. Not because of the restore's reset, but because the cart is already a
      proper transport slave ([`external-clock-sync.md`](external-clock-sync.md)).

      The saved values apply only where nothing drives transport (the standalone app, or a
      tempo-only clock). There the rack resumes mid-bar — which is **existing intent, not a
      regression**: `acidcandy` already calls `autosave_tick()`, commented *"rolling autosave
      (resume-where-you-left-off)"*. The one residue is that a restored `playing = 1` lets a
      transport-less host start the rack on load; consistent with that autosave, so left alone.
      ⚠ If you go looking, `HANDOFF.md` has a *different* `playing=1` story — a hosted panel whose
      engine never saw host transport, so it free-ran while the audible engine sat stopped. Same
      symptom, unrelated cause, **and it is fixed** (`b43cd813`, the panel no longer creates its own
      engine). It survives only inside that lane's `▼ superseded` block, which the lane's own header
      says is factually wrong and kept for the trail. Do not read it as a live report.

      **The lesson worth keeping:** "field X is in the saved struct" does not tell you X is restored
      state. Check who WRITES it each frame first. Reading the struct is not enough.
  - Note the plug-in does **no** state handling today at all (no `fullState`, no `parameterTree`, no
    presets), so a reopened session starts every rack at defaults — a separate user-visible gap from
    the two-racks-interfere bug.
- **`colorkey()`**, which destroys and rebuilds the shared sprite-sheet texture from a cart API.
  Needs its own decision (per-instance texture variant, or software-only).


## ⚠ STEP 4 IS BIGGER THAN "THE CART'S STATICS" — measured 2026-08-14

The plan said *"acidcandy's statics → `de_state()`"*, on a handoff figure of ~20. The real number is
**209 mutable file-scope statics in the cart's translation unit**, and they are not all the cart's:

| where | count |
|---|---|
| the cart itself (`acidcandy.c`) | 120 |
| `runtime/ui.h` | 19 |
| `tr808.h` · `tr909.h` · `cursor.h` | 10 |
| unattributed (multi-line decls + other cart-land headers) | 60 |

**The cart-land HEADERS hold state too.** `ui.h`'s widget table, the drum-machine banks, the cursor —
all `static`, all compiled into the cart's one TU, so two instances of one cart share them. A rack
whose widget state is shared is not two racks however independent the engine underneath is.

That matters because those headers are included by **553 carts**. Moving their declarations into a
per-cart context is not a local change.

### Three ways out, with the cost of each

**(a) Generate context-ified COPIES of the headers for the plug-in build only.** The AU build already
stages generated artifacts (`build/cart.c`), so it can stage generated `ui.h` etc. with declarations
removed, leaving the originals untouched for the other 552 carts. Mechanical, reuses `ctx-gen`, no
blast radius. Cost: generated header copies that can drift from their originals.

**(b) Move the declarations in the shared headers themselves.** One mechanism for every cart, and the
macro block keeps every call site identical — but it makes a per-cart context struct mandatory for
all 553, which is a change to what a cart IS.

**(c) Ship one rack per project** — the "honest single-instance" fallback the lane always carried.
The engine work still pays for itself (it removed real cross-instance corruption and unlocks the
editor previewing two carts, offline render while playing, and parallel tests), and multi-instance
becomes a stated limitation rather than a defect.

**Recommendation: (a)**, and it is worth deciding before writing any of it, because (a) and (b) are
the same work aimed at different targets and only one of them can be undone cheaply.

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
2. **✅ DONE — `studio.c`'s state** moved into `DeVideo`, byte-identical (116 of its statics; 108
   remain, all deliberate). **What it taught, and both lessons outlive it:**
   · **A generator driven by ONE configuration's AST cannot rewrite conditionally-declared state.**
     `studio.c` forks on `DE_NO_RAYLIB` throughout and has ~40 statics per side the other build never
     compiles (the platform seam and software rotation on one, netplay/desktop-mic/CoreMIDI on the
     other). Moving one produced a struct with a duplicate member for one build and a missing member
     for the other. `ctx-gen` now REFUSES anything inside a preprocessor conditional and says so.
   · **The probe was checking the easy half.** It built `DE_NO_RAYLIB` four ways and never built the
     Raylib path at all — so a batch compiled four times, was applied, and failed in the build the
     probe never touched. It now builds BOTH renderers, and says loudly when raylib headers are
     missing rather than quietly testing less.
3. **✅ DONE — the AUv3** creates one engine per audio unit with its own frame worker, semaphore and
   frame counter; `bootEngineOnce` is deleted; the canvas channel blits its OWNER's engine.
   **VERIFIED IN GARAGEBAND (maker, 2026-08-14): one track is clean, panel stable, no regression.**
   ⚠ The DAW test found what no gate did: a HOSTED PANEL WAS BOOTING ITS OWN ENGINE. Step 1 made
   `CanvasView` call `de_instance_create` unconditionally, so each open panel started a second engine
   and ticked it from the display link — with the cart's state shared, two tracks with panels open
   was four engines on one sequencer. Fixed: a hosted view creates nothing and is handed the audio
   unit's engine. **The lesson is about the seam, not the bug: giving every caller the ability to
   create an instance meant a caller that should only ever BORROW one created its own.** A view is a
   consumer of an engine, never a producer, and the API should have said so.
4. **✅ DONE — gated.** `bash tools/instance-check/run.sh`: two instances from `de_instance_create`,
   different transport, and their frames and audio differ. Control: two fresh instances driven the
   SAME come back byte-identical, so the headline is the transport rather than noise. **This is the
   test `refactor-guard` cannot be.** Its footer lists what a PASS does not cover.

**Still owed after step 3's engine half:** `de_sync_position` is still process-wide (it takes no
instance), the Swift frame worker is still one per process, and nothing yet runs two instances
CONCURRENTLY on two threads — `present-race-check` covers one instance only.

Step 1 is deliberately first and deliberately boring: it changes only the shape, on one engine, where
every existing gate still applies.
