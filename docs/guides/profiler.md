# Profiler — where is my cart spending its time?

A one-click CPU profiler built into the editor. It runs your cart for a few
seconds, watches what it does, and prints — straight into the build log — the
hottest functions, the call paths that reach them, the per-frame CPU budget, and
exact draw-call counts. No external tools, no macOS Instruments window: it's all
behind the scenes.

Like the [debug harness](debug-harness.md), it's **off by default, desktop-only,
and leaves normal builds byte-identical**. The ▶ run button, the web build, and a
shipped cart touch none of it.

---

## Turning it on

The button is hidden until you ask for it:

1. **settings** tab → **profiler** → check *"show ⏱ profile button in the toolbar"*.
2. A **`⏱ profile`** button appears next to ▶ run.

(Desktop app only — like ▶ run, it needs to spawn the compiler. In the browser
tab the button reports that.)

## Using it

Click **`⏱ profile`**. The editor compiles a special profiling build, launches
your cart in its own window for ~4 seconds, samples it, then closes it and fills
the build log with the report. **Drive the cart during those seconds** if the
part you care about needs input — you're profiling whatever is actually on screen.

---

## Reading the report

Three sections, top to bottom. Here's a real run of `flyover` (the voxel terrain):

```
⏱ profiled 4s
  CPU 4.5ms/frame typical · 5.9ms p95 · 28% of the 16.6ms budget · smooth 60fps

hottest functions in your update()/draw()  (592/3307 samples, ~18% of wall; rest = vsync/system)
   58.6%  347  rlVertex3f
            222  ← draw › draw_terrain › pset › DrawPixelV › rlVertex3f
             69  ← draw › rectfill › DrawRectanglePro › rlVertex3f
    5.9%   35  poly_inside
             35  ← draw › trifill › poly_fill_cov › poly_inside
    …

draw calls per frame  (your direct calls; nested ones counted once)
    11543  rectfill
       71  trifill
       48  circfill
    …
```

### 1. Frame budget — *should I even care?*

```
  CPU 4.5ms/frame typical · 5.9ms p95 · 28% of the 16.6ms budget · smooth 60fps
```

60fps gives you a **16.6ms** budget per frame. This is the CPU time spent in your
`update()` + `draw()` (the vsync wait is excluded). *typical* is the **median**
frame and *p95* is the worst 1-in-20 — both deliberately ignore one-off stalls (a
single frame can spike to 100+ms when `sample` attaches and suspends the process;
that's measurement noise, not your cart, so it's kept out of the headline). The
first few frames (one-time texture/font loads) are skipped too. At 28% you have
plenty of headroom; when it creeps toward 100% — or the verdict stops saying
*smooth 60fps* — that's when the rest matters.

### 2. Hottest functions — *what's expensive, and why is it called?*

This comes from **statistical sampling**: every millisecond the profiler notes
which function is running. Only work reached through your `update()`/`draw()` is
counted — the engine's own per-frame plumbing (the vsync frame-limiter, the canvas
blit, the audio thread) is filtered out, which is what the `~18% of wall` note
means. The percentage is each function's share of that cart-active CPU.

Each hot function lists the **call paths** that reached it, newest call on the
right, rooted at your `draw`/`update` so **your own function names stay in the
path**. `rlVertex3f` is a raylib internal you never call directly — the path
`draw › draw_terrain › pset › …` shows it's *your* `draw_terrain`'s `pset` calls
driving it. That's the actionable part.

> If a cart is nearly idle you'll see a **⚠ low sample count** warning — with only a
> few dozen samples the ranking is noise; drive the busy part or profile longer.

### 3. Draw calls per frame — *how many times am I calling things?*

Exact counts from inside the engine (not sampled). The smoking gun here:
**`rectfill` 11,543×/frame**. Sampling told you `rlVertex3f` was hot; this tells
you *why* — eleven thousand rects become ~66k vertices. Counts are your **direct**
calls: if `circfill` internally calls `trifill`, it's counted once as `circfill`.

**Use the two together.** Sampling (§2) ranks by *cost*; counts (§3) rank by
*frequency*. A function can be hot because each call is expensive (`trifill`) or
because it's called a zillion times (`rectfill`) — the fix is different for each.

---

## A worked example — catching an off-screen cost

The `trifill stress` cart (in the carts gallery) is a pinwheel of a dozen thin
software-filled triangles whose far vertices sit ~1100px *outside* the 320×200
screen. Profile it and §1 is damning:

```
  CPU 46.7ms/frame typical · 48.6ms p95 · 281% of the 16.6ms budget · 21fps
```

§3 says `trifill ×12` — only twelve calls, yet 46ms. So each call is the problem
(the §2 lens), not the count. §2 confirms it: nearly all the time is in
`draw › trifill › poly_fill_cov › poly_inside` — the software rasterizer testing
point-in-polygon over each triangle's bounding box. Twelve thin spokes shouldn't
cost much *visible* area… but their boxes ran far off-screen, so `poly_fill_cov`
was scanning millions of cells that plot nothing.

The fix lived in the engine, not the cart: `poly_fill_cov` now clamps its scan to
the on-screen region first ([rasterization-consistency.md](../design/rasterization-consistency.md)).
Re-profile the same cart:

```
  CPU 2.7ms/frame typical · 3.8ms p95 · 16% of the 16.6ms budget · smooth 60fps
```

Same image, ~17× less CPU — and the profiler is how you both *found* the cost (§2
pointing at `poly_inside`, not the count) and *proved* the win (§1 before/after).
The cart now doubles as a regression guard: if the clamp ever breaks, its fps
tanks again. (This is a worst case; most carts whose polys fit on screen see
little — see the design note for the real-cart numbers.)

---

## How it works

The profile build differs from ▶ run in three ways:

- **`-O1 -fno-inline`** instead of `-Os` — keeps `studio.c` primitives as distinct,
  named symbols so the sampler can attribute time to `trifill`/`circfill`/… instead
  of folding them into `draw()`.
- **`-DDE_PROFILE`** — compiles in the in-engine instrumentation (see below).
- After launch, the editor attaches macOS **`sample`** to the running process for a
  few seconds (this is what produces §2's call graph) and reads it back.

The in-engine part (`runtime/studio.c`, all under `#ifdef DE_PROFILE`):

- A `PROF("name")` line at the top of every public draw primitive, with a
  re-entrancy guard so only the **outermost** call is counted (§3).
- A timer around `update()`+`draw()` each frame (§1).
- Both are written to `build/perf.json` every 30 frames, so the data survives the
  editor killing the process when sampling ends.

A normal build `#define`s `PROF()` to nothing — zero cost, nothing emitted.

---

## Caveats

- **macOS-only.** It uses the `sample` CLI. (The in-engine counts/timing in
  `perf.json` are portable; only §2's call graph is mac-specific.)
- **Sampling is statistical.** It nails hotspots over a few seconds; it is not a
  cycle-exact per-call timer. Run a little longer, or drive the busy part, for a
  steadier picture.
- **The counters perturb the sampler slightly** — `PROF` bookkeeping shows up as a
  cheap child of each primitive. Negligible in practice, but it means §1's timing
  is a good budget estimate, not a microbenchmark.
- **§2 shows only `update()`/`draw()` work.** Audio runs on its own thread and the
  vsync frame-limiter isn't your code, so both are excluded — §2 is "where does my
  per-frame logic and drawing go," not a whole-process accounting.

---

## See also

- [debug-harness.md](debug-harness.md) — deterministic record/replay/trace, the
  other half of "make a run inspectable." Pair them: script the exact moment with
  the harness, profile it here.
- `fps()` in `runtime/studio.h` — the in-cart frame-rate read-out, for a number
  your players can see rather than a one-off measurement.
