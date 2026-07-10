# The flight recorder — always-on deterministic session capture

STATUS: BUILDING / v1 SHIPPED 2026-07-10. Every native ▶ Run records its inputs deterministically
to a rolling scratch ring; you "keep a take" (promote it to `tools/clips/`) only when you care. The
payoff is two-for-one: an effortless clip source AND an `rr`-style exact repro for any bug you hit.

Companion reading: [`input-recording-looper.md`](input-recording-looper.md) (the *music* looper and
gameplay ghosts — a different recording level; §"one idea is two"), [`debug-harness.md`](../guides/debug-harness.md)
(the `--record`/`--replay` harness this rides on), [`song-codec.md`](song-codec.md) (the seed-in-a-file
idea; shares the mechanism), and the take→clip pipeline in `tools/make-gif.js`.

## The problem

Recording a play was a deliberate act: press ▶ record, perform in one take from boot, parked on
window-close (`studio:record`, `editor/electron/main.cjs`). So the *interesting* moment — the bug
you didn't expect, the run you didn't plan to keep — is already gone. You can only capture what you
decided to capture before it happened.

## The model — always record, keep almost nothing

Three homes, one flow:

```
  every ▶ Run                    "keep take" (⌘⇧K)              consumers
  ───────────                    ─────────────────              ─────────
  build/.rec/<slug>/<sess>.rec   tools/clips/<slug>/NN-take.rec  make-gif · reel · spec · repro
  (gitignored scratch RING,      (committed, self-contained     (deterministic replay:
   last 10 per cart, auto-evict)  via its `# seed` header)       same take → same world)
```

- **Always-on**: every native Run launches `--det --record <ring> --seed <random>`. The scratch ring
  keeps the last `RING_KEEP` (10) sessions per cart and evicts the oldest — we throw away almost
  everything.
- **Keep-to-commit**: when a cart closes, the console's `↩ session recorded → …` line carries a
  **✂ keep take** button (and a **📂** reveal) — one click promotes that session's ring file into
  `tools/clips/<slug>/NN-take.rec`, the same home authored takes use, feeding the whole fan-out
  (replay · clip · reel · attract · spec seed). The resulting `✓ kept → …` line in turn grows a
  **🎬 bake** button (runs `make-gif` on the take in place) + a 📂, so record → keep → clip → find
  never leaves the console. (Global **⌘⇧K** / the Debug menu do the same but work *mid-play* too,
  without closing.)
- **"Record from the moment I'm in" is a trim, not a savestate.** Because we record from a
  deterministic boot, the interesting in-point is just `make-gif --start <n>` at clip time. We never
  snapshot mid-play state (see Boundaries).

## Why it's cheap (measured)

The recorder (`harness_input`, `runtime/studio.c`) is **delta-encoded** — a line only when an input
*changes* (`<frame> k|m|b|w …`). So:

- **CPU**: a handful of int compares + an occasional `fprintf`. Nanoseconds/frame; never near a hot
  function. The old per-frame `fflush` is relaxed to every ~30 frames (a live tail / mid-play keep
  still sees a ≤0.5s-fresh file, without a syscall per frame).
- **Storage**: worst case (constant mouse drag) ≈ 1 KB/s ≈ ~3.5 MB/hour; button-driven play is a tiny
  fraction (idle frames write nothing), and it's repetitive text that gzips ~10×. The ring's
  keep-last-10 bound makes it a non-issue.

## The determinism contract (the enabling mechanism)

A `.rec` is **inputs only, replayed from a deterministic boot**. `--det` fixes both the clock
(`clk()`→`det_clock`) AND the per-frame `dt` (`frame_dt = DET_DT`) and seeds the RNG
(`SetRandomSeed(seed); srand(seed)`). So a same-build replay of the input tape reconstructs the world
exactly — for input-driven, dt-integrated, and `rnd()`-using carts alike.

The one missing piece was the **seed**: the default was 1 and it was never written down, so an
always-random session couldn't be replayed. Fix (`runtime/studio.c`): the recorder writes a
`# seed <n>` header line first; `load_replay` reads it back and applies it before `SetRandomSeed`, so
**any** replay path (play.js, make-gif, editor) reconstructs the same world with zero extra args. An
explicit `--seed` on the CLI still wins. The header is inert to every other consumer (`load_replay`
skips any non-`<int> <char> <int> <int>` line; `make-gif` only reads `# frames/fps/scale/crf`), so
old headerless `.rec` files replay unchanged.

**Honest caveats (documented, not solved):**
- A cart that reads wall-clock (`GetTime()`) or unseeded `rand()` directly bypasses determinism and
  won't replay faithfully. Most carts use the engine's `now()`/`dt()`/`rnd()`/`noise()` and are fine
  (`noise()` is a pure coord hash — unseeded and already deterministic).
- Replay is faithful on the **same machine + same build**. Cross-*architecture* float drift is not
  guaranteed — that's the `tools/det-probes/` bit-identical story, out of scope here.
- Making every Run `--det` is a behaviour change (game-time vs wall-time under stutter). For a
  fantasy console at a steady 60fps it's near-invisible and arguably more correct; a settings toggle
  ("record every play") disables it for wall-clock-sensitive carts. Net games skip it (netplay owns
  its own `--det` seed via the handshake). The live/libtcc backend is not recorded in v1.
- **Recording is an editor ▶ Run concern only — no exported/deployed build ever records.** The
  `--det --record` flags are injected by the editor at spawn time, never baked into a binary:
  **iOS** (`DE_NO_RAYLIB`) compiles no `main()`/`--record` arg loop (it uses the `de_frame` entry
  points); the **web/wasm** build compiles the record harness out entirely (`harness_input` is under
  `#ifndef PLATFORM_WEB`); and a double-clicked **Mac `.app` / `.exe`** is launched with no
  `--record` flag, so `rec_file` stays `NULL` and the record branch never fires.

## The debugging payoff

This is `rr`-style record-and-replay for the console, at near-zero runtime cost *because the sim is
already deterministic-capable*. Hit a glitch → the ring already holds the exact session → replay it
headless for a bit-identical repro (`node tools/play.js <cart> replay <ring>.rec`), then keep it as a
regression seed. **Every close logs where the session landed** (`↩ session recorded → build/.rec/…`,
with a 📂 reveal-in-Finder button + the ready-to-paste replay command), so a repro is always findable
even when you didn't think to keep it. Bridging a kept `.rec` into an automatic `spec()` regression is a noted future
consumer (see [`spec-harness.md`](spec-harness.md) — today `spec()` is hand-authored, not `.rec`-fed).

## Next rung — the replay-takes regression gate (proposed, not built)

Now that keeping takes is one click, they accumulate — and a pile of deterministic, self-seeding
takes is a regression net waiting to be used. Proposed: a `tools/replay-takes.js` gate (the
behavioural twin of `build-all.js`, which only *compile*-checks) that replays every committed
`tools/clips/*.rec` headless and fails on:

- **a crash / non-zero exit** during replay (catches API rot + null derefs the compile gate misses),
- **a determinism break** — replay twice, diff the `--trace`; a mismatch means the cart stopped being
  reproducible (some new wall-clock/`rand()` dependency crept in).

Deliberately **not** a brittle golden-trace-vs-committed diff — the spec harness avoids that on purpose
([`spec-harness.md`](spec-harness.md): "not a predicate swept over a recording"), because any intended
behaviour change would fail it. This is a *smoke + self-consistency* gate: does every recorded session
still run and still reproduce? A cart that wants a real assertion still writes a `spec()`. Rides
entirely on what's already shipped, and turns the recorder's output into bug-*prevention*, not just
bug-repro. Sequence it next to `build-all` in the pre-commit checks once written.

**Prerequisite — an uncapped headless replay, or don't build this at all.** Headless replay today
runs in *real time*: `SetTargetFPS(60)` is unconditional (`runtime/studio.c`), so a 49-second take
costs 49 wall-clock seconds. Measured 2026-07-10: 28 committed takes ≈ 3 minutes of recorded play →
×2 for the double-replay determinism check + ~20 cart compiles ≈ **8–10 minutes per full run, growing
linearly with every take kept** — and effortless take accumulation is the whole point of the recorder.
The fix is cheap and decides viability: under `--det` the sim clock is already decoupled from wall
time (`frame_dt = DET_DT`), so the 60fps cap is purely cosmetic in a headless run — a turbo flag
(skip the FPS cap; ideally skip presenting the frame) makes replay CPU-bound, likely 10–50× faster.
Precedent in the same file: `--spec` mode already runs the loop unpaced to completion. Even with
turbo, scope it: pre-commit replays only takes of carts touched by the diff; the full sweep is a
nightly/occasional run, like `build-all` itself.

## Boundaries (what this is NOT)

- **Not a savestate.** `de_state()` has no serialize, and cart state is scattered across statics — a
  general mid-play snapshot is the emulator-savestate problem. We dodge it entirely via the trim
  model above. No `de_state` snapshot is taken.
- **Not the music live-looper or gameplay ghosts.** Those record at a different level (audio events /
  a second body) and stay in [`input-recording-looper.md`](input-recording-looper.md). This is the
  *harness* recorder — whole-session, file-based, dev-facing, engine-owned only as a debug seam
  (ADR-0006).

## Where the code lives

- `runtime/studio.c` — `# seed` header write on `--record` open; `load_replay` reads it back;
  per-frame `fflush` relaxed to every 30 frames.
- `editor/electron/main.cjs` — `studio:run` records every native run (`--det --seed <rand> --record
  <ring>`); the `build/.rec/<slug>/` ring + `pruneRing`; `keepTake`/`keepTakeFromMenu`;
  `studio:record-keep` IPC; Debug-menu *Keep take* + global `⌘⇧K` (twin of the attach-profiler's
  `⌘⇧P` — same handle table).
- `editor/src/settings.js` — the "record every play" toggle (default on).
- Consumers unchanged: `tools/make-gif.js` / `tools/play.js` replay the kept `.rec` and self-seed
  from its header; `--start` trims the boot to the in-point.
