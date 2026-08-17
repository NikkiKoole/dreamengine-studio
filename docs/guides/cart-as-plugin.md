# Shipping a cart as an AUv3 plug-in

> **STATUS: guide (2026-08-17).** The ordered runbook for taking a cart from "a nice rack" to "a
> plug-in a DAW can load twice". Written after `pedalboard` went through it and every step cost
> something worth writing down.
>
> Companions: [`../design/auv3-plugin-types.md`](../design/auv3-plugin-types.md) (WHICH of the five
> plug-in shapes a cart should be — read that first if the answer isn't obviously "instrument"),
> [`../design/engine-context.md`](../design/engine-context.md) (the per-instance design this guide
> executes), [`../design/ios-plan.md`](../design/ios-plan.md) (the build ladder),
> [`checks-and-oracles.md`](checks-and-oracles.md) (the change→gate index).

## The two questions, in this order

1. **Is this cart worth a plug-in slot?** A plug-in competes on a shelf, and the type you declare
   decides which shelf. `auv3-plugin-types.md` §4 ranks the shapes. Get this wrong and the rest is
   wasted: `pedalboard` audition-passed every structural gate as an `aumu` while being unable to do
   the one thing its name promises. The engine's audio-input capability ✓ SHIPS (`de_audio_input`,
   `mic_start`, `input_monitor` — and its insert latency measures 0 samples); what an `aumu`
   declares is no input BUS, so there is nothing for the host to hand it.
2. **Will it survive being loaded twice?** Apple puts **every instance of one plug-in in a single
   process** — there is no setting that changes it. A cart is ONE translation unit, so without the
   work in Part 2, two DAW tracks share every `static` in the cart and in every header it includes.

Part 1 answers the first cheaply. Part 2 is the second.

## Part 1 — audition it on this Mac

```bash
APP=<app> zsh ios/mac.sh                 # stage the app's cart → build → register → auval → gates
APP=<app> RESIZABLE=1 zsh ios/mac.sh     # a resizable cart: let it REFLOW into the host's panel
CART=<cart> zsh ios/mac.sh               # a bare cart inside APP's plug-in shell (says so, loudly)
```

Everything per-app is **derived from `apps/<app>/app.json`** by [`../../ios/au-identity.sh`](../../ios/au-identity.sh):
the component identity (`auName`/`auSubtype`/`auManufacturer`/`auDisplayName`) and the throwaway
**carrier** app that registers the extension. Both must be per-app, for different reasons:

- ⚠ **The component triple is FOREVER.** A DAW stores `(type, subtype, manufacturer)` in the saved
  project to re-instantiate the plug-in, so changing a shipped subtype orphans every project that
  used it. Pick once, per app, never edit. Manufacturer is the *vendor* and is shared (`Mpla`).
- **The carrier must differ too, or building app B deregisters app A's plug-in** — silently, looking
  exactly like the plug-in broke. On macOS an AUv3 is registered by launching the app bundle that
  contains it, so one shared carrier path means one plug-in at a time. Renaming the `.app` is *not*
  enough: LaunchServices keys apps by bundle id, so two copies of one id still means one wins,
  unpredictably.

What a green run proves, and what it doesn't:

| gate | proves | blind to |
|---|---|---|
| `auval` | Apple's validator: repeated instantiation, many rates/buffer sizes | anything needing host transport (it never sets `musicalContextBlock`) |
| rate converter | the engine's compile-time 44.1 kHz stays in tune at the host's rate | — |
| `--loadable` | a DAW can load our code at all | — |
| `--view` / `--panel` | the UI extension is wired, and the panel belongs to the unit that RENDERS | **whether the picture is any good — go look** |
| `--state` / `--wheel` / `--params` | `fullState`, CC, and the automatable knob tree | — |
| transport | the rack follows play/stop/tempo/loop | — |

⚠ **The transport gate is written for a SEQUENCER.** Four of its six checks require `onsets >= 8`
over 8 beats — at least one onset per beat, which is `acidcandy`'s drum density. A cart that strums
once a bar fails them while playing perfectly well. Read the numbers, not the ✗: a peak well above
silence with a low onset count means "playing sparsely", not "broken".

⚠ **`mac.sh` treats that gate as fatal**, so a cart which legitimately fails it never reaches the
view and panel gates. Run those by hand:

```bash
AU_SUBTYPE=<sub> AU_MANUF=<mfr> ./au-transport-check --view    # and --panel, --params, --state
```

**Following host tempo is opt-in, not automatic.** `beat()` is the *cart's* musical clock, driven by
its own `bpm()`. Following a DAW means explicitly reading `sync_active()`/`sync_bpm()`/
`sync_playing()`/`sync_transport()` and deriving position from `sync_beats()` — an accumulator knows
how fast but never *where*, so it cannot follow a loop jump. `acidcandy` is the worked example (its
`de:meta` documents the whole integration); [`../design/external-clock-sync.md`](../design/external-clock-sync.md)
is the design.

## Part 2 — make the cart safe to load twice

This is mechanical, tooled, and byte-exact-verifiable. Do not hand-edit statics.

**1. Bless a baseline first.** The whole refactor is a pure move, so the proof is that nothing
changed:

```bash
node tools/play.js <cart> script /dev/null --headless --frames 400 --wav before.wav
```

**2. Stage the cart and dry-run the generator.** `build/cart.c` is line-identical to
`tools/carts/<cart>.c`, and the generator works there:

```bash
node tools/play.js <cart> run --headless --frames 1      # → build/cart.c
grep -m1 '"slug"' build/cart.c                            # ⚠ VERIFY — this path is shared (see Traps)
node tools/ctx-gen.js --target cart                       # dry run: what would move, what is skipped
```

**3. Clear everything it skips.** A partly-moved set is its own bug — the moved half is
per-instance, the skipped half is still shared, and nothing says so. Two classes, both of which bit
`pedalboard`:

- **A local shadowing a file-scope name.** `bool amp = …` inside a function, against a file-scope
  `static float amp[NSTR]`. The access macro `#define amp (de_cart->amp)` would rewrite the local's
  declaration too. **Fix: rename the local** (it is the smaller blast radius).
- **An anonymous struct type.** `static struct { … } kmeta[KM_N];` cannot become a struct *member*,
  because the member needs a type name. **Fix: `typedef` it.**

The generator is deliberately conservative — it skips anything it cannot parse with certainty rather
than guessing — so its skip list is a work list, not a warning. Iterate until it reads `skipped 0`.

**4. Probe, then write.**

```bash
node tools/ctx-gen.js --target cart --probe    # applies to a COPY, compiles plain/TRACE/SPEC/CART_CTX
node tools/ctx-gen.js --target cart --write    # generates runtime/<cart>_state.h, rewrites build/cart.c
diff tools/carts/<cart>.c build/cart.c         # ⚠ READ THIS — expect only deletions + one #include
cp build/cart.c tools/carts/<cart>.c
```

The generated header is named after the **cart**, not "cart": `runtime/` is on every build's include
path, so a generic `cart_state.h` would be clobbered the moment a second cart became a rack.
`*_state.h` is auto-classified as generated, so it needs no CLAUDE.md entry.

**5. Turn the opt-in on**, above `studio.h` so the cart-land headers fork too:

```c
#define DE_CART_CTX
#include "studio.h"
```

**6. Verify both paths render byte-identically.** With the define, and (by commenting it out) without:

```bash
node tools/play.js <cart> script /dev/null --headless --frames 400 --wav after.wav
cmp before.wav after.wav        # must be silent, on BOTH paths
```

The default path matters even though this cart no longer uses it: it is what the other ~550 carts
compile, and it must stay exactly the declarations that were there. The opt-in path is identical for
a *single* instance because each instance copies the compile-time template.

**7. Gates.** `node tools/spec.js <cart>` · `node tools/lint-carts.js` ·
`node tools/lint-saved-state.js` · `bash tools/instance-check/run-uictx.sh` (the pattern gate — it
builds both paths and asserts opposite things, including the `ctx-gen --target cart` shape itself) ·
then re-bake (`make-cart.js` re-embed **then** `--run`).

## Part 3 — what else a rack needs before it earns a slot

Being loadable twice is table stakes. This is the checklist a host actually notices, measured by
comparing `acidcandy` (does all of it) against `pedalboard` (did none of it):

| capability | how | what a host does without it |
|---|---|---|
| automatable knobs | `param_bind` (`runtime/param.h`) | `parameterTree is nil` — the DAW shows no parameters at all |
| host keyboard | `midi_get` + the PITCH lens | a played note does nothing |
| session state | `de_state_for_saved` / `DE_CTX_BLOCK_SAVED` | your patch is gone when the project reopens |
| host tempo | the `sync_*` set (Part 1) | it plays at its own speed, ignoring the project |
| two instances | Part 2 | track 2's knob moves track 1's |
| audio input | an `aumf` input bus — **not yet wired**, `auv3-plugin-types.md` §4.1 | it cannot hear the track it is inserted on |

⚠ **Parameter addresses are forever too**, in the same way the component triple is: a saved
automation lane stores nothing else. Append, never renumber.

## Traps

- **`build/cart.c` is shared across agents.** `play.js` writes it, and a sibling compiling in the gap
  leaves you generating a context for *their* cart. Always re-check the slug immediately before
  `--write` and before copying back. (`mac.sh` retries three times for exactly this reason.)
- **Never trust a static COUNT from `grep`.** `grep -c '^static'` counts `static const` tables and
  static *functions*: it read 114 for `pedalboard` where clang's AST said 43. Ask
  `node tools/engine-statics.js --list --tu build/cart.c --files build/cart.c`.
- **Never cache what `de_state_for` returns.** Another header registering its key can grow the state
  block and move every slice; a cached pointer is a use-after-realloc waiting for the second header.
  The generated access macros re-fetch every time — keep it that way.
- **The template is READ-ONLY on the opt-in path.** Nothing may write through it, or instance 7
  inherits instance 0's mutations. The engine learned this by shipping live heap pointers into a
  second instance and corrupting the heap in GarageBand.
- **Put any non-zero init ABOVE the access macros.** After them the member names are macros, so
  `c->x` expands to `c->(ctx_()->x)` and will not compile.
- **A cart cannot `extern` an engine function.** If it needs one, the seam is missing an API — that
  is how `canvas_resize` was born. Gated by `node tools/lint-engine-seam.js`.
