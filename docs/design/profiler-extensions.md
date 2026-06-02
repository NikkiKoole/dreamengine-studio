# Profiler extensions — first-frame & region timing (PARKED)

Two follow-ups to the shipped profiler ([`../guides/profiler.md`](../guides/profiler.md)).
Both are **parked** — captured here so we don't lose the design, not started.

## The shared insight

The shipped profiler has two engines: macOS `sample` (statistical call-graph →
"hottest functions") and in-engine `-DDE_PROFILE` instrumentation (frame timer +
`PROF` draw-call counters → `build/perf.json`).

`sample` is the **wrong tool** for both ideas below:
- A single frame is ~16 samples at 1ms — far too coarse.
- A sampler has no concept of a named "region" of your code.

Both are answered by extending the **in-engine timing** half into **named
time-zones**, reusing the existing `perf.json` pipeline. No new sampling
machinery. One mechanism covers both: `init` and `frame 0` are just built-in
zones.

---

## Run duration (shared concern — matters most for zones)

The shipped profiler runs a **fixed ~4s** (`cfg.profileSeconds || 4`). That's fine
for "where does a steady scene spend time," but wrong for the zone use-case: you
often need to *reach* the part you care about (play to the boss fight, level 3,
the particle storm) before the interesting work even starts. And zone averages
get steadier the longer they accumulate.

Two ways to give it room, not mutually exclusive:

1. **Configurable duration.** A small control next to `⏱ profile` (e.g. 4s / 10s /
   30s) or a settings field. Trivial — it's already a parameter.
2. **Run-until-you-close (preferred for zones).** Launch the cart, let the author
   *play* to the exact section, then close the window — and read the report for
   that whole session. Mechanics line up nicely:
   - The in-engine data (`perf.json`: frame timing, `PROF` counts, zones) is
     cumulative and flushed every 30 frames + on clean exit, so a long interactive
     session just yields steadier averages.
   - `sample` takes a *max* duration but **ends early when the target exits** — so
     start it with a long cap (e.g. 120s) and it returns the moment the window
     closes. The editor then reads the (partial) call graph + `perf.json`.
   - This turns "profile a deep part" into: enable → play to the spot → close →
     read. Zones make that per-section actionable.

   Caveat: cumulative averages blend the *whole* session (menu + gameplay). If we
   want "just the boss fight," that argues for a **reset/marker** (a key in-cart, or
   `zone_reset()`) that zeroes the accumulators — or per-section zones the author
   only enters during the part of interest.

---

## Q1 — profile the first frame explicitly

**Why it's invisible today:** the frame timer deliberately *skips the first 3
frames* (warm-up: texture/font loads, first GL submit) so the steady-state peak
isn't dominated by one-time cost, and sampling only starts ~1s in. So init cost
is hidden by design.

**Proposal:** time it directly in-engine and report it *separately* from
steady-state.
- Wrap `init()` and frame 0 with the timer we already have → add `initMs` and
  `frame0Ms` to `perf.json`.
- Build-log line, e.g.: `init 142ms · first frame 38ms (one-time)`.
- Optional stretch: keep the first ~30 frames' work-ms in an array to draw the
  warm-up curve (when does it settle?).

**Limitation:** this gives *how long* init/frame-0 took, not a per-function
breakdown of it (sampling can't resolve one frame). If we ever want "what made
init slow," that needs either region zones (Q2, bracket the suspects) or
instrumenting the heavy one-time calls (`LoadTexture`, etc.).

Nearly free — the timer already exists; this is just two more measurements that
bypass the warm-up skip.

---

## Q2 — profile specific parts deep inside (region/zone timing)

A scoped-timer API the cart author brackets around any code — the "deep inside"
answer, and squarely in the `watch()`/trace spirit. Accumulates wall time per
named zone per frame, reports into `perf.json` → build log:

```
terrain 3.1ms · sprites 0.8ms · hud 0.2ms
```

**Must always compile.** Like `watch()`, the API is always declared in
`studio.h`; it's a no-op in a normal build and live only under `-DDE_PROFILE`
(zero cost shipped). Carts can call it unguarded.

**Open design question — the API shape:**

| Shape | Sketch | Trade-off |
|---|---|---|
| Single re-pointing call | `zone("terrain"); …; zone("sprites"); …; zone(NULL);` | Fewest calls; flat (no nesting); easy to forget the closing `zone(NULL)` |
| Begin/end pair | `zone_begin("terrain"); …; zone_end();` | Explicit; supports nesting with a stack; two calls, mismatch risk |
| Auto-closing scope | `ZONE("terrain") { … }` (macro) | Can't forget to close; nests naturally; macro magic, brace-scoped only |

Leaning toward the **single re-pointing call** for beginner-legibility (it reads
like a narration of the frame) unless we want nested zones, in which case
begin/end with a stack.

**Reporting:** add a `zones` array to `perf.json` (`{name, msPerFrame}`),
rendered as a new build-log section under the draw-call counts. Same flush-every-
30-frames path so it survives the editor killing the process.

---

## Status

Parked 2026-06-02. Build order if/when we resume: Q1 first (nearly free, the
timer exists), then Q2 (needs the API-shape decision above + a `studio.h`
addition wired through the [four-places rule](../../CLAUDE.md)). Neither touches
the `sample` path, so the shipped profiler is unaffected.
