# Host parameters — the knobs a DAW can automate and record

> **STATUS: BUILDING (2026-08-16) — the seam is wired end to end, gated, and read-back is FIXED.** A cart binds floats it already owns and a host sees them, automates them and
> moves the mix with them. Gated by `bash tools/param-check/run.sh` (9 assertions, engine half) and
> `./au-transport-check --params` in `ios/mac.sh` (the real out-of-process plug-in).
> See also [`host-midi-notes.md`](host-midi-notes.md) (the sibling host seam, and where this came
> from), [`engine-instance-seam.md`](engine-instance-seam.md) (why the table is per-instance),
> [`ios-plan.md`](ios-plan.md) (the AUv3 extension),
> [`auv3-plugin-types.md`](auv3-plugin-types.md) (the survey of the OTHER four plug-in shapes, and
> why the parameter tree is the one thing every one of them needs).

## Why

The AUv3 exposed **no `AUParameterTree` at all**, so a host saw zero parameters. Nothing on the rack
was automatable, nothing recordable, the lane menu empty. It surfaced while answering a different
question (*"is any of this recordable?"*) and it reframed something already shipped: mapping the mod
wheel to the master filter was a **workaround for having no parameters**, not a design anyone would
have chosen freely. Filed as item 3 of STATUS's "AUv3 session state" entry, with no design.

## The model: the parameter *is* the knob

A cart binds a float it already owns and changes nothing else:

```c
param_bind(P_M_FLT, &mflt, "FLT", 0, 1);
```

The panel keeps dragging `mflt`, the DSP keeps reading `mflt`, and the host now reads and writes that
same float. **Because there is one storage location and not two**, "the host automates it", "the
panel follows automation" and "a finger drag shows up in the host's lane" are the same fact rather
than three sync paths to keep honest.

`param_bind` is idempotent, so it is safe to call every frame, and the first bind captures the slot's
current value as the **default**.

**The tree is built from what the CART declared, not from a table in Swift.** A Swift table would be
a second source of truth for a fantasy console whose whole point is swapping carts: change the cart
and the plug-in's parameters change with it, with nothing to keep in sync.

## The pieces

| where | what |
|---|---|
| `runtime/param_ctx.h` | the per-instance table (an AUv3 puts every instance in one process, so a shared table would mean two tracks sharing one set of knobs) |
| `runtime/param.h` | `param_bind` + the five seam functions |
| `runtime/studio.c` | the table in `DeInstance`, the drain in `loop_step`, `--param` for the harness |
| `runtime/studio.h` | `param_bind` / `param_count`, plus the usual four places |
| `ios/AU/TinyjamAU.swift` | builds the `AUParameterTree` from the seam; observer → `de_param_set`, provider → `de_param_get`; the frame worker polls `de_param_changed` |

**Threads.** A host writes from wherever it likes (automation on the render thread, a generic-view
slider on the main one) and the cart reads its knobs on the frame thread. So `de_param_set` **queues**
and the drain applies at the top of the frame — the same discipline as the input ring, and for the
same reason: a cross-thread write into cart state is exactly what that seam exists to stop.

## Two traps this hit

**The drain has to live in `loop_step`, not `de_frame`.** `de_frame` is the *host's* entry; the
native loop calls `loop_step` directly and never goes through it. Put the drain in `de_frame` and
every parameter works perfectly under a DAW and does nothing at all under `play.js` — which is to say
nothing any gate in this repo can see. It cost one confused round, with the trace showing a knob that
would not move while the queue filled up behind it.

**Bind in `init()`, not in `draw()`.** The knobs are drawn only on the *focused* face, so binding
where they are drawn would make parameters appear and vanish from the host's menu as you tap around.
And `param_bind` captures the slot as the default, so it must run **before the rolling autosave
restore** — bind after it and the host is told last session's values are the factory defaults, so its
"reset parameter" returns you to Tuesday.

## What is exposed, and why not everything

21 parameters: the MST performance controls (FLT, RES, GLU, FB, PUMP), the acid five on each 303, and
DIST/SEND/VERB on each drum machine. The rack has ~40 knob sites; the per-voice drum tone knobs, the
p-lock lanes and the pattern editors are deliberately **out**. They are how you *build* the sound, the
session blob already carries them, and every parameter lives in the host's menu forever.

**⚠ Addresses are forever.** A saved project's automation stores nothing but the number, so changing
what one means silently re-points somebody's lane at a different knob — no version to refuse it, no
error to see. Same hazard `lint-saved-state.js` exists for on the other side of the seam, and the same
rule: **append, never renumber**; retire one by leaving the hole. Not yet lint-enforced, which is the
obvious next protection.

## The read-back bug, and how it was actually found

**Fixed 2026-08-16.** It is worth writing down because three plausible causes were wrong and the
answer came from instrumentation, not reasoning.

**The symptom:** out of process, a host read a parameter back and got the value it held *before* its
own write, forever. Automation worked; the display lied.

**The measurement that ended it.** Logging both sides of the seam showed the reads were exactly one
write behind, at the *same timestamp and thread* as the write:

```
SET addr 1 = 0.02   →   GET = 0.5     (the previous value)
SET addr 1 = 0.5    →   GET = 0.02    (the previous value)
```

**A host reads a parameter straight back after setting it, in the same call**, to populate the cache
that every later read is served from. `de_param_set` only QUEUED — deliberately, so cart state is
only ever written on the frame thread — so that read-back landed *before* the drain and honestly
reported the old slot. The host cached it and never asked again.

**The fix** is a `want` shadow in `param_ctx.h`: a set records its intent (clamped exactly as the
drain will clamp it) and a get prefers it until the drain applies it, at which point the shadow is
dropped and reads go back to the live slot. The cart still only ever sees the value on the frame
thread. The host just stops being told a value it did not ask for.

**Four causes ruled out first**, each recorded so nobody pays for them twice: the panel-move poll,
the drain's echo suppression, the `parameterTree` override (a real defect, fixed, but not this one),
and out-of-process mirroring — which the first write-up confidently blamed and which was never
involved. ⚠ The override being wrong also *invalidated* the poll's first test, which had run while
the tree was not properly installed; it was re-run afterwards.

**And it surfaced a structural gap on the way.** The per-instance seam — `struct DeInstance` and
every resolver — lives inside studio.c's `#ifdef DE_NO_RAYLIB` block, so the native build has no
instances at all. That stayed invisible while the only native caller was `de_param_set(NULL, …)`,
where `-O2` folds the `if (in)` branch away and never emits the reference. The moment a seam
function grew a body the optimiser could not fold, the **native build stopped linking** on code that
runs fine under a host. `runtime/param.h` now carries a guarded stub. The coverage implication is
the part worth keeping: **headless gates exercise the default shared table, a host exercises the
per-instance one**, so `param-check` being green says nothing about the path a DAW takes. That is
what `au-transport-check --params` is for.

**Two assertions now, not one.** The gate checks that a written value reads back *and* that an
untouched parameter still reads its live value — because the fix must not degenerate into "echo
back whatever was last set".

## Open

- **No ramping.** The tree does not claim `flag_CanRamp`, because the engine applies a value once per
  *frame*, not per sample. Telling the truth beats advertising smoothing we do not do, but it means a
  fast automation sweep steps at 60 Hz.
- **No `factoryPresets`.** Still empty, and it is the other half of STATUS item 3.
- **An address lint** on the append-only rule.
- **Only `acidcandy` binds anything.** Every other cart shows a host an empty tree, which is exactly
  what they all did before — but the API is now there for any of them.
