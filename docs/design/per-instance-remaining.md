# What is left before the per-instance refactor is DONE

> **STATUS: building.** The one page that answers *"how much of this is finished?"*. Everything else
> in this lane explains HOW; this says WHAT REMAINS, in the order it should be done, with the reason
> each item exists. Siblings: [`engine-context.md`](engine-context.md) (the state move),
> [`engine-instance-seam.md`](engine-instance-seam.md) (the host handle).
>
> **Progress: the engine went from 601 process-global mutable statics to 148**, and every one of the
> 148 is a recorded decision rather than a leftover. Live numbers: `node tools/engine-statics.js`.
> **The CART is done too** (2026-08-14): `acidcandy`'s 198 are per-instance, so the thing that
> actually blocked two racks is gone. What is left below is a published FRAME that is still shared
> (two panels cannot yet show different pictures) plus a list of correctness gaps.

## The goal, stated once

**Two instances of a plug-in, on two DAW tracks, are two independent racks.** Everything below either
blocks that, or is a correctness gap the work exposed. Verified by
`bash tools/instance-check/run.sh`, which is the test `refactor-guard` structurally cannot be — the
guard runs ONE instance, so it proves a state move changed nothing and can never prove two instances
are strangers.

⚠ **Read what each gate actually drives.** `instance-check` drives the two engines through
`de_sync_position` — TRANSPORT, which is engine state — so it has never exercised the CART's own
state and does not now. The cart-side evidence comes from `run-uictx.sh`, which builds the same
probe twice and asserts opposite things, and which covers both cart-land header state and the
cart's own (two different shapes: a header's block starts zeroed and runs an init function, a
cart's starts as a copy of a compile-time template).

---

## ▶ BLOCKS TWO RACKS (do these, in this order)

### 1. `de_pres_*` — the published FRAME is still process-wide
Inside `#ifdef DE_NO_RAYLIB`, so `ctx-gen` refuses it (it sees one configuration). Consequence: two
panels cannot show different pictures, and `instance-check` says so explicitly — it asserts
independence on AUDIO only, because comparing frames would compare one shared buffer at two times.

### 2. `fb_w` / `fb_h` / `de_sw` / `de_sh` — taken back OUT of the context
They must move WITH their siblings `sw_cbuf` / `sw_dst` / `sw_world_buf`, which are also inside
`#ifdef DE_NO_RAYLIB`. A half-moved framebuffer group made `cls()` write `fb_w*fb_h` pixels into
another instance's smaller canvas. **Needs 3 and 4 together**, and both need:

### 3. `ctx-gen` must reason across BOTH build configurations
It takes one AST, so anything inside a preprocessor conditional is refused (12 lines in `studio.c`).
The fix is to union the statics from a `DE_NO_RAYLIB` dump and a Raylib dump, dedupe by name, and
refuse only on a genuine type conflict.

---

## ▶ CORRECTNESS GAPS THE WORK EXPOSED (not blockers, but do not ship without deciding)

### `lfo_seed_ctr` — determinism, and invisible to every gate
`sound.h`, function-local, inside `sound_setup_note`. Its documented job is keeping `--det` renders
byte-reproducible. Shared, two instances interleaving note starts **both** lose determinism — and
`refactor-guard` sits green through it, because one instance is unaffected.

### `save_dir` — a live defect today
N instances write one `cart.sav` / `cart.kv`. Worse, the in-memory mirrors are written back WHOLE, so
it is last-writer-wins at FILE granularity, not per key. `save_path()`'s `static char buf[600]` must
move with it or the bug just relocates.

### `crash_handler` cannot reach a context
Registered with the OS, reads `watches`/`watch_count`. A signal handler takes no argument, so it needs
a static "last active instance" by design, or the watch dump goes.

### The CoreMIDI virtual source
N instances would publish N sources with the SAME name. The handle makes per-instance naming possible;
someone still has to choose the naming.

### `colorkey()` mutates a SHARED GPU texture from a cart API
`UnloadTexture` + `LoadTextureFromImage` on the shared sprite sheet. Two instances with different
colorkeys fight; one calling it mid-draw frees a texture the other is sampling.

### `de_process_init` never landed
`de_init` still does one-time process work (fonts, shaders, sheet upload) AND per-instance work in one
call. Splitting it is what makes the shared/per-instance boundary explicit rather than implied.

### Nothing runs two instances CONCURRENTLY on two threads
`present-race-check` covers one instance. The two-instance version does not exist, and the AUv3 drives
frames from a worker while the view copies on another thread.

---

## ▶ FOUND ALONG THE WAY, NOT PART OF THIS WORK

- **The plug-in has NO session state** (no `fullState`, no presets) — a reopened DAW project starts
  every rack at defaults. Filed in [`STATUS.md`](../STATUS.md); the ladder to it runs through this
  work but the work is separate.
- **GarageBand's iPad-layout toggle has never worked** for this plug-in. Pre-dates all of this; the
  CART is fine (`play.js acidcandy --resize` reflows correctly). Filed in `STATUS.md`.
- **The `--panel` gate is RED and unexplained.** It reports no `[tinyjam] PANEL` line from the audio
  unit's process. Not the crash (fixed, confirmed). Whether it predates this work is UNVERIFIED —
  and this session already found one gate (`present-race-check`) that had been silently dead for
  weeks, so do not assume.

---

## DONE (so nobody redoes it)

| | |
|---|---|
| `sound.h` | 327 of 340 statics per-instance; the 13 left are recorded decisions |
| `studio.c` | 116 moved; the rest are shared/harness/conditional by classification |
| `sync.h` | **0 statics left** — fully per-instance, and `de_sync_position` names its instance |
| the seam | every host entry point takes a `DeInstance *`; hosts all migrated |
| instances | `de_instance_create` allocates, copies a pristine template, and they are proven strangers |
| the AUv3 | one engine + one frame worker per audio unit; `bootEngineOnce` deleted |
| cart-land | **all 8 headers** that hold state declare it once and fork; 553 carts unaffected |
| the cart | `acidcandy`'s **198 statics** (168 file-scope + 30 hoisted out of function bodies) moved, and it defines `DE_CART_CTX` so its headers fork too. **0 statics left in the cart TU.** |
| gates | `instance-check`, `run-uictx.sh`, `refactor-guard`, `engine-statics`, `ctx-gen --check` |
| verified | **one track clean in GarageBand** (maker, 2026-08-14): stable panel, no regression |

### How the cart move was done (the part that kept going wrong)

Three separate attempts to rename the cart's colliding names by **scanning braces** failed — a local
whose scope the scan got wrong silently rebinds to the static of the same name, which still compiles.
What worked was making the **compiler** the oracle: rename the DECLARATION, and every use that was
the static errors as *undeclared* while every use bound to a local stays silent. 93 uses in one pass,
zero judgement calls. Same trick for the three hoisted names that were not unique.

This is the sixth time in this refactor that a regex over C gave a confident wrong answer. The
lesson is in [`engine-context.md`](engine-context.md); the shortest form is **ask clang**.
