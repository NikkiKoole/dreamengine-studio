# What is left before the per-instance refactor is DONE

> **STATUS: building.** The one page that answers *"how much of this is finished?"*. Everything else
> in this lane explains HOW; this says WHAT REMAINS, in the order it should be done, with the reason
> each item exists. Siblings: [`engine-context.md`](engine-context.md) (the state move),
> [`engine-instance-seam.md`](engine-instance-seam.md) (the host handle), and
> [`engine-simplification.md`](engine-simplification.md) — its **round 2** is the cleanup this
> refactor left behind, including the half-moved groups (`kv_data`, `sw_rot_*`) that are correctness
> gaps rather than tidying.
>
> **Progress: RUN `node tools/engine-statics.js`.** Do not trust a count written here — this line
> said "601 → 148" until 2026-08-14, when the tool was found to be silently dropping 30 rows it
> could not attribute and to have never been pointed at `runtime/midi_input.h` at all. It now names
> anything it cannot place and exits nonzero, so the number it prints is the number.
> **The CART is done too, and so is the PICTURE** (2026-08-14). `acidcandy`'s 198 statics are
> per-instance, and so are the framebuffer group and the published frame — so two racks are
> independent in both sound and image. **Two GarageBand tracks sound correct, verified by the maker.**
> Nothing below blocks two racks any more; what remains is a list of correctness gaps to decide.

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

### ✅ DONE — `de_pres_*`, the framebuffer group, and `de_pend_*` (2026-08-14)
The whole group moved at once into `DeVideo`: `fb_w`/`fb_h`/`de_sw`/`de_sh`, `sw_dst`/`sw_world_buf`,
the `de_pres_*` seqlock and the `de_pend_*` deferred-request block. **This is what made two open
panels flicker** — both views pushed their own size into the ONE pending slot, and both blitted the
ONE published buffer, which alternated between the two engines' renders.

Added BY HAND (ctx-gen refuses conditionals and refuses to re-run on `studio.c`), so `studio_ctx.h`
is now partly hand-maintained and says so. The other half is `de_vid_of(in)`: the four seam functions
that run on the HOST's thread reach the instance through the handle rather than the thread-local,
which there names the default engine.

`instance-check` gained the assertion it structurally could not make before — each instance publishes
its OWN frame, proven by the two frames differing in SIZE.

### ▶ THE ONE THING LEFT HERE: `ctx-gen` cannot reason across BOTH build configurations
It takes one AST, so anything inside a preprocessor conditional is refused. That is why the group
above had to be added by hand, and why `studio_ctx.h` is now partly hand-maintained. The fix is to
union the statics from a `DE_NO_RAYLIB` dump and a Raylib dump, dedupe by name, and refuse only on a
genuine type conflict. **Not a blocker** — nothing is waiting on it — but every future hand-add is a
chance to get wrong what a generator would have got right.

---

## ▶ CORRECTNESS GAPS THE WORK EXPOSED (not blockers, but do not ship without deciding)

### `lfo_seed_ctr` — determinism, and invisible to every gate
`sound.h`, function-local, inside `sound_setup_note`. Its documented job is keeping `--det` renders
byte-reproducible. Shared, two instances interleaving note starts **both** lose determinism — and
`refactor-guard` sits green through it, because one instance is unaffected.

### ✅ The `--panel` gate is GREEN as of 2026-08-14 — re-verify before believing it is broken
Re-run after the session-state work (a full `zsh ios/mac.sh`, Release): four ✓ and PASS, including the
one that was supposedly missing — *"it reported the OTHER verdict first (so the check can go red)"*,
with two verdict lines from the audio unit's own pid. So the "not one `[tinyjam] PANEL` line was
logged at all" reading below is **no longer what the evidence says**; whatever caused it (a stale
build, or the Debug configuration) is gone. Left in place because the lesson stands: fix the
observation before believing the verdict. Nothing was done to it deliberately — treat this as
"currently green, cause unknown", not "fixed".

### `save_dir` — HALF fixed
`de_set_save_dir` now reaches its instance through the handle (it used to `(void)in` and write the
DEFAULT engine's dir, so instance 3's save location landed on instance 0). But **N racks still write
one `cart.sav` / `cart.kv`**, because nothing gives them distinct DIRECTORIES — and the host is what
has to choose them. The in-memory mirrors are written back WHOLE, so it is last-writer-wins at FILE
granularity, not per key. `save_path()`'s `static char buf[600]` must move too or the bug relocates.

### ⚠ THE OTHER HALF OF THAT CLASS: a HOST component that quietly creates its OWN engine
`de_instance_create` used to return the same singleton every call, so it did not matter who asked
for the engine. **It allocates now, and every extra caller is a separate rack.** No call site
changed; their meaning did. Three found, all in host code, none caught by any gate:

| where | symptom |
|---|---|
| hosted `CanvasView` | the AUv3 panel booted a second engine — the **flickering** the maker saw |
| `AudioEngine` | the app rendered an engine nobody touched — **total silence on device** |
| `GameHost` | a third engine booted just to ask two questions, on a comment reading *"de_instance_create is idempotent"* — true when written |

⚠ **Grep `de_instance_create` before trusting any host.** The rule is one engine per rack, created
by whoever owns the rack and PASSED to everything else. A component that creates its own is not
obviously wrong at the call site — that is what makes this expensive.

**Why nothing caught it:** the AUv3 never had the bug (an audio unit owns one engine and hands the
same pointer to its view), `instance-check` drives instances it creates itself, and `refactor-guard`
runs the desktop build, which has no `CanvasView`. **Nothing in the repo instantiates the iOS app's
object graph.** That is the real gap.

### ⚠ A BUG CLASS, not a list of bugs: a seam function that IGNORES its handle
Six found in one day — `de_resize`, `de_copy_frame`, `de_set_save_dir`, `de_framebuffer`,
`de_screen_w`, `de_screen_h`. Each took a `DeInstance *`, dropped it, and read through the
thread-local, which on the HOST's thread names the default engine. **It never fails loudly.** The
function compiles, returns plausible values, and silently operates on the wrong rack — and having a
handle in the signature makes it read as already done. `(void)in` is the marker to grep for. Only
`de_is_resizable` legitimately keeps it (`de_reflow` is a compile-time flag).

⚠ The same shape crosses translation units, where the compiler cannot see it at all: `face.h` and 7
carts declared `extern void de_resize(int, int)` against the 3-argument definition. That is undefined
behaviour, it was live for weeks, and it surfaced as a crash with `in = 0xa7` — the canvas width. If
a cart needs an `extern` for an engine function, **the seam is missing an API** (that one became
`canvas_resize`).

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

- ~~**The plug-in has NO session state**~~ **— DONE 2026-08-14.** `de_save_state`/`de_load_state` +
  `fullState`, gated by `bash tools/state-check/run.sh` (20 assertions, four negative controls). The
  design as written on this page was wrong twice — the `de_state()` block cannot be serialized
  verbatim (its arena header is pointers) and the "no pointers" rule was already broken by `ui.h`,
  with live voice HANDLES in `keybed.h`/`solo.h`/`radio.h` being the worse case a pointer lint cannot
  see. Corrected in [`engine-instance-seam.md`](engine-instance-seam.md). The lint exists too
  (`tools/lint-saved-state.js`) and earned itself immediately — it caught a `void *` array inside
  `acidcandy`'s saved slice that had shipped an hour earlier. The Swift half is gated too —
  `./au-transport-check --state` in `ios/mac.sh` drives the real out-of-process plug-in, including the
  **property-list round trip** a DAW performs when it writes `fullState` into a project file (anything
  not plist-representable is dropped silently, so an in-memory test cannot see it). 8/8.
- **GarageBand's iPad-layout toggle has never worked** for this plug-in. Pre-dates all of this; the
  CART is fine (`play.js acidcandy --resize` reflows correctly). Filed in `STATUS.md`.
- **▼ SUPERSEDED — the `--panel` gate went GREEN later the same day; see
  [the §"✅ The `--panel` gate is GREEN"](#-the---panel-gate-is-green-as-of-2026-08-14--re-verify-before-believing-it-is-broken)
  block above.** Kept for the trail of how it was diagnosed, because the reasoning ("fix the
  observation before believing the verdict") is the reusable part. Do NOT act on the text below.
  ~~**The `--panel` gate is RED, and what it is failing on is now narrower.**~~ Re-run 2026-08-14 after
  the cart move: **not one `[tinyjam] PANEL` line was logged at all** — not even the pre-render
  "NO AUDIO HAS RENDERED" one, which is the gate's OWN negative control and fires before any
  connection question arises. So `reportAudibility` never ran in the harness (the view controller's
  `au` is nil, or the view is never asked for on that path), and the gate cannot currently tell
  "the panel is orphaned" from "the panel was never opened". Its headline —
  *"the panel may be driving an engine nobody hears"* — is therefore not what the evidence says.
  Counter-evidence: the `--view` gate passes (a host is handed the view controller and it lays out),
  and the maker has watched the panel work in GarageBand. **Fix the observation before believing the
  verdict.** Still UNVERIFIED whether it predates this work; this session already found one gate
  (`present-race-check`) silently dead for weeks, so do not assume.

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
| **VERIFIED** | **iPadOS: the AUv3 panel works in GarageBand** (maker, 2026-08-14) — it draws and taps land where you press. The same cart is now a plug-in with a working panel on macOS AND iPadOS. |
| **VERIFIED** | **TWO GarageBand tracks sound correct** (maker, 2026-08-14). This is the goal at the top of this page, met. Defect (B) — "load it on two tracks and the sound goes weird" — is CLOSED. Everything still listed above is a picture problem or a gap to decide, not two racks fighting over one engine. |

### How the cart move was done (the part that kept going wrong)

Three separate attempts to rename the cart's colliding names by **scanning braces** failed — a local
whose scope the scan got wrong silently rebinds to the static of the same name, which still compiles.
What worked was making the **compiler** the oracle: rename the DECLARATION, and every use that was
the static errors as *undeclared* while every use bound to a local stays silent. 93 uses in one pass,
zero judgement calls. Same trick for the three hoisted names that were not unique.

This is the sixth time in this refactor that a regex over C gave a confident wrong answer. The
lesson is in [`engine-context.md`](engine-context.md); the shortest form is **ask clang**.
