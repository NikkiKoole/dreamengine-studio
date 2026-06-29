# iOS profiling & instrumentation — what Apple's tooling can tell us

STATUS: SURVEY + build candidates (2026-06-30). The iOS deploy is fully terminal-driven
(`ios/device.sh`, no Xcode GUI), and Apple's whole Instruments suite is reachable the same way via
**`xcrun xctrace`** — it can *record a trace headlessly and export it to XML*, so an agent can profile
a cart on the device and read the hot-symbol list back. This doc maps what's worth pulling for a
**software-canvas engine on real phone silicon** and the small tools we could build around it.

Companions: [`engine-portability.md`](engine-portability.md) (the renderer decision + the on-device
FPS/bunnymark numbers), [`ios-plan.md`](ios-plan.md) (the iOS roadmap), `ios/measure-device.sh` (the
FPS harness this would deepen), `tools/profile-fleet.js` (the *desktop* CPU profiler — this is its
on-device counterpart).

## Why this matters for us

We committed to the software canvas for 2D ([ADR-0024](../decisions/0024-software-canvas-is-canonical-for-2d.md)):
the phone rasterizes every pixel on the CPU (no GL/ANGLE). So our perf story is a CPU story, and the question
that actually moves it is **"which function eats the frame on the A13?"** — `sw_blit`? `sw_recolor`?
the rotate-composite? `sound_callback`? Our own counters (the `#if DEBUG` overlay in `CanvasView`,
the in-cart FPS readout) tell us *how fast*; they can't tell us *where the time goes*. Instruments can,
on the real device, at function granularity.

## What `xctrace` offers (and what's worth pulling)

`xcrun xctrace version` = 15.1 here; `xctrace list templates` shows the full set. Ranked for us:

| template | what it gives | value to dreamengine |
|----------|---------------|----------------------|
| **Time Profiler** | periodic CPU stack sampling → hottest functions | **#1.** Where the software-canvas frame goes. Directly answers "what caps bunnymark / what to optimize." |
| **Audio System Trace** | audio-thread activity, render-deadline misses, glitches | We pull a CoreAudio render callback (`de_audio_render`) + the AUv3. Catches the crackle you hear but can't see. |
| **Animation Hitches** | dropped `CADisplayLink` frames + the cause | The honest "did we hold 60?" beyond our own counter, with the stall attributed. |
| **Allocations / Leaks / Game Memory** | heap growth, leaks, footprint | Did the touch pool / baked fonts / sprite image leak? Does a cart grow unbounded over a session? |
| **CPU Counters** | cycles, cache misses, branch misses | The blit loop is memory-bound; cache behaviour is the next layer once Time Profiler points the way. |
| ~~Metal System Trace / GPU~~ | GPU workload | **Skip** — we're pure software canvas; nothing to capture until a Metal backend exists. |

Two complements that aren't xctrace templates:
- **`os_signpost`** — emit labeled intervals from the code (`de_frame` phases: input/update/draw/
  composite/present, and `de_audio_render`). They appear as exact-duration regions on the Instruments
  timeline — better than sampling for "how long did *this* phase take," and nearly free. The natural
  pairing: gate them on a flag (like the existing `DE_TRACE`) and they double as the source for the
  `CanvasView` perf overlay.
- **lldb** — already in our loop (`ios-deploy` spawns it; `ios-deploy --debug` for an interactive
  session): breakpoints, stepping, backtraces for a crash or a logic bug on device.

## The agentic workflow (why this fits our loop)

`xctrace record` runs headless and `xctrace export --input <trace> --xpath …` dumps the data as XML —
so the loop is: **build+install (`device.sh`) → record a few seconds → export → an agent parses the
hot-symbol table and reports "N% in `sw_blit`, here's the line."** No GUI, no manual trace-reading.
That turns "profile it" into something an agent can run and summarize end to end.

Sketch:
```bash
xcrun xctrace record --template 'Time Profiler' --device <UDID> \
  --launch -- <app.app> --time-limit 12s --output bun.trace
xcrun xctrace export --input bun.trace --xpath '/trace-toc/run/data/table[@schema="time-profile"]' > prof.xml
# parse prof.xml → top symbols by sample count
```

## What we could build

1. **`tools/profile-device.sh` (or extend `ios/measure-device.sh`)** — the on-device agentic profiler:
   stage+build+install a cart, `xctrace record` a Time Profiler trace, `xctrace export`, and print a
   ranked hot-symbol summary. The device counterpart of `profile-fleet.js`. *Highest value* — it's the
   thing that tells us what to optimize, repeatably.
2. **`os_signpost` instrumentation in the engine** — wrap the `de_frame` phases + `de_audio_render`
   behind a `DE_SIGNPOST` flag. Gives precise per-phase intervals in Instruments *and* a clean feed for
   the perf overlay. Small, and pays off every profile after.
3. **Audio-glitch pass** — an `Audio System Trace` recipe over the AUv3 + the app to assert no
   render-deadline misses under load (the audio twin of `Animation Hitches`).
4. **Memory soak** — an `Allocations`/`Leaks` pass over a long cart session (e.g. drive a radio station
   for minutes) to catch unbounded growth before it ships.

## Caveats

- **Symbols:** for function names in the profile, build with debug symbols — the Debug build has them
  (and no inlining, so the call tree is readable); Release + the `.dSYM` works too and gives
  representative timings, but inlining can fold `sw_blit` into the caller. For *finding the hotspot*,
  Debug is clearer; for *absolute ms*, Release. The relative breakdown holds across both.
- **Load:** a profile is only useful under load. Carts that idle without input (bunnymark with no
  finger down) show nothing — either drive them (hold a finger during the record) or profile a cart
  that's busy on its own (`camrot` auto-rotates + composites every frame; a radio station auto-plays
  audio).
- **Device:** must be paired + unlocked; device recording is finickier than simulator (but the
  simulator runs on the Mac CPU, so its *timings* aren't representative — use it only for memory/logic).

## First measurement (bunnymark, A13) — 2026-06-30

**Setup:** Debug build (full symbols, no inlining → readable call tree), 30s **Time Profiler** attached
to the running app on the **iPhone SE 2nd-gen (A13)** via `xctrace record --attach TinyjamHello`, a
finger held down to pour bunnies the whole time (heavy load, near the 60fps sag). Exported with
`xctrace export` and parsed (the agentic loop this doc proposes — it worked end to end). **38,704
samples** (~1ms each, all threads).

**Thread split** (where the CPU was):

| subtree | % of samples |
|---------|--------------|
| main render (`de_frame` → draw) | **77.6%** |
| audio (`de_audio_render`) | 12.8% |
| idle / other | 9.6% |

Only ~10% idle = the device was near-saturated (at the load ceiling, as expected when pouring max bunnies).

**Top self-time functions:**

| % of all samples | function | |
|---|---|---|
| **76.7%** | **`sw_blit`** | the software sprite blit — *the* limiter |
| 8.2% | `apply_insert` | sound.h request-queue apply (audio thread) |
| 4.6% | `sound_callback` | the mixer (audio thread) |
| <1.5% ea | `___chkstk_darwin`, `nanov2_*`/`malloc`/`free`, Swift value-witness/metadata | stack-probe + allocator + Swift glue — all noise |

**Takeaways:**
- **The sprite ceiling is `sw_blit`, full stop** (≈77%). It's a *pure* rasterization cost — not the
  framebuffer→screen handoff (the blit-to-CGImage was ~0.6ms elsewhere), not the Swift/C boundary
  (Swift glue is <2% combined), not allocation (malloc is <2%). So the lever for "more sprites on the
  phone" is the per-pixel blit inner loop itself (SIMD the row copy / widen the `DE_BLIT_FAST` fast
  path), nothing around it. This is the cleanest possible benchmark result.
- **Audio is a surprising ~13%** — and mostly `apply_insert` (8.2%), not the mixer. That's the sound
  *request queue* under bunnymark's per-bunny spawn blips: thousands of sound events/sec hammer the
  queue. A real cart triggers orders of magnitude fewer sounds, so this is bunnymark-specific — but
  good to know the queue-apply cost scales with sound-event rate, not just voice count.
- **The C engine is tight** — negligible allocator/Swift overhead confirms the hot path is the
  hand-written rasterizer, exactly as intended.
- **Method validated:** headless `xctrace record --attach` → `export` → parse gave a function-level
  breakdown with zero GUI. This is the `profile-device.sh` build candidate, proven manually — worth
  packaging.

### Optimization attempts

- **Branchless masked blend in the `sw_blit` fast path — TRIED, REVERTED (1.5× slower).** The profile
  put 76.7% in `sw_blit`'s inner loop, whose per-pixel `if (transparent) continue;` is a data-dependent
  branch that also blocks auto-vectorization. The textbook fix — drop the branch, write *every* pixel
  via an alpha-mask blend (`drow[i] = (src&m)|(dst&~m)`) so clang can vectorize — was implemented behind
  an A/B flag, verified **bit-identical** (canvas-diff + an injected-bunny A/B harness), and measured:
  **37.0 ms/frame vs 24.5 ms/frame** for the branchy original. It *lost*. Why: a 32×32 bunny is mostly
  transparent, so the branchy `continue` skips ~half the cell with no memory touch, while the blend
  read-modify-writes all 1024 px/bunny — the extra bandwidth costs more than the branch + SIMD saves.
  **Lesson:** for *sparse* sprites the transparent-skip is already near-optimal on a scalar CPU; the
  blend would only pay off for *dense/opaque* sprites. Not worth a per-sprite-density code split. The
  20k-bunny ceiling looks like the honest CPU/bandwidth limit, not a fixable hotspot. (Hand-NEON with a
  real masked store *might* help, but at the arch-specific + determinism-parity cost called out above —
  not justified by the measured headroom.)
