# Host parameters — the knobs a DAW can automate and record

> **STATUS: BUILDING (2026-08-15) — the seam is wired end to end and gated; host READ-BACK is the
> one open defect.** A cart binds floats it already owns and a host sees them, automates them and
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

## The open defect: host read-back

Out of process, a host reading a parameter back gets the value it held **before its own write**.

What is known, from measurement rather than reasoning:

- **The provider IS consulted on every read**, and this is now proven rather than inferred: pinning
  `implementorValueProvider` to a constant `0.123` made the host read back `0.123` for every
  parameter. An earlier version of this doc claimed the same thing from "a parameter nothing wrote
  reads 0.55, its cart default" — that was **not** proof, because 0.55 was also the AUParameter's
  cached default. The constant is the discriminator; the default was ambiguous.
- **The write reaches the DSP.** The mix closes, measured, repeatedly.
- **Therefore `de_param_get` genuinely returns the stale value.** The provider is asked, it reads
  `*slot`, and the answer is the pre-write value while the DSP is audibly playing the new one. The
  bug is on OUR side of the seam, not in the host's mirroring — which is where the first write-up
  pointed, wrongly.

**Three causes ruled out by experiment**, each recorded so nobody pays for them twice:

1. **The panel-move poll.** Disabling it entirely changed nothing.
2. **The drain's echo suppression.** Removing it changed nothing.
3. **The `parameterTree` override.** The first cut stored the tree in a property and overrode the
   accessor with a no-op setter, which swallowed `AUAudioUnit`'s own setter and the framework
   installation it performs. That was a real defect and is fixed (assign `parameterTree` the ordinary
   way) — but it was *not* this bug. ⚠ Note it also invalidated cause 1's first test, which had run
   while the tree was not properly installed; it was re-run afterwards and is still negative.

**Where to look next, and it is structural.** The per-instance seam — `de_instance_midi`,
`de_instance_param`, the whole block — is compiled **only under `#ifdef DE_NO_RAYLIB`**
(`runtime/studio.c:2958`). The native harness build does not contain it: a `de_param_get` call added
there fails to LINK. So the headless gates exercise the **default, shared** table via the
thread-local, while the AU exercises the **per-instance** one, and `param-check` being green says
nothing about the path a host actually takes. That is the same shape as the `de_frame`-versus-
`loop_step` trap this design already hit once, and it is the obvious suspect for a value that is
written in one place and read from another.

**Consequence if never fixed:** a host's generic view and a reopened project can show a knob at its
old value until something writes it again. Automation still *works*; it may just not display where it
starts. The gate reports this as a warning rather than a failure, deliberately — the write path is
what makes a rack automatable and it is proven.

## Open

- **Read-back** (above) — the one real defect.
- **No ramping.** The tree does not claim `flag_CanRamp`, because the engine applies a value once per
  *frame*, not per sample. Telling the truth beats advertising smoothing we do not do, but it means a
  fast automation sweep steps at 60 Hz.
- **No `factoryPresets`.** Still empty, and it is the other half of STATUS item 3.
- **An address lint** on the append-only rule.
- **Only `acidcandy` binds anything.** Every other cart shows a host an empty tree, which is exactly
  what they all did before — but the API is now there for any of them.
