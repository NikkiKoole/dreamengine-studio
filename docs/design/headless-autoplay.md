# Headless autoplay — growing the harness toward agents that play themselves

STATUS: EXPLORING (scratchpad) — the staged path toward agents that play the carts; not committed.

> **Genre: design exploration (scratchpad).** Where the debug harness could go, and
> the staged path to get there. It is **not** the status ledger and **not** a
> committed plan:
> - "What's shipped / open / cut?" → [`../STATUS.md`](../STATUS.md).
> - The harness as it exists *today* → [`../guides/debug-harness.md`](../guides/debug-harness.md).
>
> Origin: "can an agent go play a cart on its own to find issues — headless, no
> window, at great fps?" The sibling project **navkit** already does this; this note
> studies how, measures where we are, and lays out incremental stages.

---

## Where we are today

The harness (see the guide) gives us the expensive foundation: **determinism**
(`--det` pins the timestep to 1/60 and seeds both RNGs), **input injection**
(`--record`/`--replay`/`--script`), a **per-frame JSONL trace** (`--trace`), and a
**filmstrip dump**. A failing run is reproducible byte-for-byte.

What it is **not**, yet:

- **Not actually headless.** `--headless` only *hides* the window
  (`SetWindowState(FLAG_WINDOW_HIDDEN)`); `main()` still calls `InitWindow()`,
  creates a GL context, boots audio, builds a `RenderTexture`, and runs `draw()`
  every frame.
- **Not fast.** `studio.c` calls `SetTargetFPS(60)`. Measured: **3000 frames = 51 s**
  (exactly realtime) at **16% CPU** — it *sleeps* to hold 60fps. Actual compute was
  ~5.2 s of CPU, i.e. there's ~10× headroom just sitting behind the cap.
- **Not in the loop.** Inputs are *pre-baked*. The harness replays a fixed plan; no
  agent observes state and decides the next input.

---

## The reference: how navkit runs windowless

navkit (`~/Projects/navkit`, a C sim) runs the **same binary** GUI-or-headless via a
runtime flag, never touching graphics in headless. Worth copying:

| Pattern | navkit location | What it buys |
|---|---|---|
| **Never `InitWindow()` headless** | `src/main.c` `RunHeadless()` (~L1177) | no GL context needed at all |
| **Sim split from render** | `Tick()`/`TickWithDt(dt)` in `src/entities/mover.c` vs `src/render/` | drawing is optional, never on the hot path |
| **Fixed 60 Hz tick** | `TICK_DT = 1/60` (`mover.h`) | deterministic physics |
| **Game-speed scalar** | `gameDeltaTime = tickDt * gameSpeed` (`src/core/time.c`) | 1× / 10× / 100× from one knob |
| **No FPS cap headless** | bare `for (t<ticks)` loop (`main.c` ~L960) | ~**3000 ticks/sec**, runs as fast as CPU allows |
| **Seeded RNG** | `ResetTestState(seed)` (`src/core/time.c`) | reproducible runs |
| **State-load instead of input** | `--load save.bin.gz` | start from any scenario |
| **Invariant audit** | `src/core/state_audit.c` (6 checks) + `--audit` | catch corruption without a human |
| **Telemetry dump** | `=== HEADLESS RESULTS ===` block (`main.c` ~L998) | per-system counts/timings after a run |
| **Tests express time** | `RunGameSeconds(10.0f)` (`src/core/time.c`) | tests read in seconds, not tick counts |

The decisive structural fact: **navkit's separation is 100% runtime, not
compile-time** — one binary, `--headless` switches behaviour. We can do the same
because a dreamengine cart already obeys the split: `update()` is logic, `draw()`
is rendering. The runtime just has to stop forcing the render path.

---

## The gap for dreamengine

Everything GL-bound lives in three places in `runtime/studio.c`:

1. `main()` → `InitWindow()`, `InitAudioDevice()`, `LoadRenderTexture()`,
   `pal_shader_init()`, `LoadTextureFromImage()` for the sprite sheet.
2. `loop_step()` → `BeginTextureMode()/draw()/EndTextureMode()`, the `pget`
   snapshot, `BeginDrawing()/DrawTexturePro()/EndDrawing()`.
3. The drawing primitives themselves (`spr`, `rect`, …) — but **only if a cart
   calls them**, and by convention carts call them from `draw()`, not `update()`.

So the logic core (`update()`, the musical clock, input, `watch()`/trace) is
already GL-free **as long as we don't run `draw()`**. That's the wedge.

---

## Growth stages (each independently shippable)

### Stage 1 — uncap the clock *(tiny; do first)*
A `--uncapped` flag (or implicit under `--det`/batch) skips `SetTargetFPS(60)` and
the per-frame present-wait. Expected: ~575 fps *with* rendering (10× realtime) for
free. Risk: near zero — `dt()` is already fixed in `--det`, so simulation is
decoupled from wall-clock.

### Stage 2 — logic-only mode `--no-render`
Gate `loop_step()` so it runs `update()` + `harness_trace()` but skips
`BeginTextureMode/draw/EndTextureMode`, the `pget` snapshot, and the present.
Still calls `InitWindow()` (cheap, once), but no per-frame GL. Expected: multiple
thousand fps. Caveat: a cart that (mis)uses a draw primitive in `update()`, or
reads `pget()`, won't work here — detect and warn, or treat as "not logic-safe."

### Stage 3 — true windowless
Gate `InitWindow()` and the one-time GL resource creation (render texture, pal
shader, sprite/font textures) behind "render enabled." In `--no-render`,
`spritesheet`/`canvas` stay zero and the primitives no-op. This is the navkit
end-state: no window, no GL, no audio. **The hard, platform-specific part** —
raylib/GLFW on macOS generally wants a display to make a context — so confirm
whether a no-`InitWindow` build links and runs, or whether we split the cart logic
into a core that doesn't link raylib at all (the cleanest, biggest refactor).

### Stage 4 — game-speed scalar
Borrow navkit's `gameSpeed`: feed `sound_tick`/`dt()` a `DET_DT * speed` step (or
run `speed` sim-steps per loop). Lets a windowed session fast-forward, and lets a
headless run trade fidelity for throughput. Orthogonal to the FPS cap.

### Stage 5 — closed-loop control (agent *in* the loop)
The real autonomy unlock. Two options, not mutually exclusive:
- **Iterate-on-replay** (works *today*, no new code): agent writes a script → runs
  headless → reads trace → writes a refined script → reruns. Determinism makes each
  iteration cheap. Good for search/fuzzing; *not* reactive.
- **Stepping protocol** (new): the binary reads input commands from **stdin** and
  emits the frame's state JSON to **stdout**, one exchange per frame (or per N). An
  agent process drives it live: observe → decide → inject → repeat. This is true
  reactive play. Design Qs: framing (line-delimited JSON?), batching N frames per
  exchange for speed, and a "run until condition" opcode so the agent isn't paying a
  round-trip every single frame.

### Stage 6 — issue detection (define what a "bug" is)
"Find issues" needs machine-checkable failure conditions:
- **Crashes** — already captured (`crash_handler` dumps `watch()` values). Make it
  exit non-zero and write a final trace line so a driver notices.
- **Soft-locks** — trace-linter: no meaningful state change for N frames.
- **Range/NaN invariants** — a cart-side `assert_de(cond, "msg")` that lands in the
  trace, plus generic checks (coords on-screen, score ≥ 0, etc.). navkit's
  `state_audit.c` is the model.
- **Anomaly sweep** — a `tools/` trace-linter that flags the above from JSONL.

### Stage 7 — fuzz / autoplay driver
A `tools/` driver that sweeps RNG seeds + random or grammar-based input streams,
runs each headless-uncapped, collects crashes/locks/assert-hits, and — because runs
are deterministic — **auto-shrinks** a failing input to a minimal repro (drop
events, re-run, keep if it still fails). This is "the agent plays thousands of
games overnight and hands you 5 minimal repros."

---

## The north star

```
fuzz/autoplay driver  ──seeds+inputs──▶  headless windowless core (Stage 1-3)
        ▲                                         │ per-frame trace + asserts
        │ shrink failing repro                    ▼
   issue detector  ◀──JSONL/exit-code──   crash/lock/invariant signals (Stage 6)
        │
        └──▶ minimal .rec repro  ──▶  you replay it in a real window to watch it happen
```

Reactive agents (Stage 5 stepping protocol) plug into the same core when the bug
needs *play skill* to reach, not just input volume.

---

## Open questions / decisions to make

- **Does macOS raylib run with no `InitWindow()`?** If not, Stage 3 means extracting
  a raylib-free logic core (bigger, but the navkit-grade outcome). Test early.
- **Reactive vs fuzz first?** Fuzz reuses almost everything we have; reactive needs
  the stepping protocol. Fuzz likely finds crashes/locks sooner; reactive finds
  "you can't beat this level" bugs.
- **Where do invariants live** — per-cart `assert_de`, or a generic runtime linter,
  or both? (navkit does both: domain audits + per-system profiling.)
- **Speed knob semantics** — uncapped-fixed-step (Stage 1, exact) vs game-speed
  scalar (Stage 4, may change sim if anything reads wall-clock). Keep `--det` exact.

## Cheapest high-value next step

Stage 1 + Stage 2 together are a few dozen lines and turn 51 s into ~1–5 s for the
same 3000 frames — enough to make iterate-on-replay (Stage 5a) and a first seed-sweep
fuzzer (Stage 7) pleasant to use *today*, before any windowless or protocol work.
