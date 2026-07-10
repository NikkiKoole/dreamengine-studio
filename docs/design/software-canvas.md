# Software canvas — kill `rlVertex3f` by rasterizing the whole frame on the CPU

STATUS: EXPLORING (proposal) — the fleet survey's #1 lever; the primitive forks are de-risked by tools/det-probes/; the GO/NO-GO probe is [software-canvas-phase0-plan.md](software-canvas-phase0-plan.md).

> **Genre: design exploration → PROPOSAL, not built.** This is the design pass the
> [engine-optimization fleet survey](../guides/engine-optimization.md#fleet-survey--where-the-engine-spends-time-across-carts-2026-06)
> asked for before any code: its #1 fleet-wide cost is the per-pixel GPU submission path
> (`pset`/`DrawPixel` → one `rlVertex3f` per pixel), and the fix is an architecture change,
> not a patch. Decide the open forks (below) before writing it. Companion to
> [cpu-shaders.md](cpu-shaders.md) #5 (the parked "true offscreen buffer") and
> [rasterization-consistency.md](rasterization-consistency.md) (the CPU-coverage invariant).

## The problem (measured)

The fleet survey (`tools/profile-fleet.js`, 32 carts) is unambiguous: **`pset` is the dominant
engine cost by 2.5×** — 59,593 calls/frame across 21 carts (`dpaint` 30k, `interiors` 18k,
`lotfill` 9k), each one a single immediate-mode GPU vertex (`pset → DrawPixel → DrawPixelV →
rlVertex3f`). Every profile in this whole optimization arc has the same shape: **`rlVertex3f`
on top.** The span-fill work (polyfill, circfill/ovalfill) killed it for *fills* by replacing N
`DrawPixel` with one `DrawRectangle` — but raw `pset(x,y)` calls are arbitrary points; they can't
be coalesced.

> **Correction (2026-06) — the "CPU-shader trilogy is `pset_rgb`" claim throughout this doc is
> stale; re-profiling found them rectfill-bound.** `raymarch` uses **zero** `pset`/`pset_rgb`;
> `caustics`/`shadelab` use it negligibly (pset ≈ 0–3/frame). They actually paint the screen as
> **rectfill spans** (`caustics` rectfill ≈ 16k/frame, `raymarch`/`shadelab` ≈ 7k) — a manual span
> optimisation, which is a draw-call-**count** shape (the `sloop` profile), *not* the per-pixel
> `rlVertex3f` path. So wherever this doc lists caustics/shadelab/raymarch as the pset target set,
> read it as: **the genuinely pset-per-pixel-bound carts are `dpaint` (30k), `pixelperfect` (26k),
> `interiors` (18k), `lotfill` (9k)** — those are v0's real win. The CPU-shaders are still valid v0
> targets (v0 routes `rectfill` → `cbuf` too), but via the draw-call-count win — and the
> `DE_BATCH_PSET` result warns that win may be small on M1, where the driver is cheap on draw-call
> count (see "Cheaper alternatives → Measured"). They drop hard only where small-draw submission is
> expensive (weak GPU / WebGL-on-old-hardware).

> **First cart to cross trigger (a) — `cityplan`, measured 2026-06-23.** The Recommendation parks the
> build until "a cart is actually `pset`-bound below 60fps." `cityplan` is the first one that is:
> **19.9ms/frame typical, 22.0ms p95 — 119% of the 16.6ms budget, dropping to ~47fps.** The profile is
> textbook for this doc — **~73% of wall is the removable `pset → DrawPixelV → rlVertex3f` path**
> (62.1% `rlVertex3f` + 5.4% `DrawPixelV` + 3.2% `rlSetTexture` + 1.9% `pset`), against only ~15% real
> work (`noise_*`/`cover_at`) and ~4% `prof_bump` (profiler artifact, gone in a normal build). At
> **213,916 `pset`/frame** it draws ~3.3× the 64k-px canvas — heavy overdraw (roofs over `rows_fill`),
> which is precisely the worst shape for GPU immediate-mode (each overdrawn pixel = another vertex
> submit) and cheap on a `cbuf` (repeated stores). Back-of-envelope removing the GPU path: ~20ms →
> ~6–8ms, back under budget with headroom. **Caveat on v0 fit:** it's mostly `pset`+`rectfill`+~4
> `print` (clean), but it also issues **~485 `line`/frame** — if those are world-space block outlines
> interleaved with fills (not a top HUD layer), the v0 deferred-`line`-overlay trick doesn't preserve
> z-order for them, so cityplan may need `sline`/CPU-line (Phase 2) rather than being a pure v0 target.
> Verify before treating it as the Phase-1 proof cart. **This is evidence, not a go** — logged so the
> trigger condition is captured whenever the decision is revisited.

> **RAN — Phase 0 result GO + fleet A/B (2026-06-24).** Built behind `DE_SOFTWARE_CANVAS` (env, off
> by default; `studio.c`, commits `133b9d0e`/`ec1b855c`). **Editor UI:** settings → *rendering* →
> "render backend" dropdown (hardware GPU / software CPU) sets this env var per ▶ run — no recompile;
> wired in `editor/src/settings.js` (`renderMode`) → `editor/electron/main.cjs` (`cartEnv()`, also
> folded into `liveSignature()` so live mode relaunches on a switch). Mechanism is **byte-identical** GPU vs
> software for the integer primitives (`swcanvas_test`: same shasum). Fleet A/B (native M1,
> `workMsAvg`, same harness) confirms the thesis is **targeted, not universal** — it wins exactly the
> `pset`/fill-bound carts and is a wash on the already-span-based CPU-shaders:
>
> | cart | GPU | SW | | kind |
> |---|---|---|---|---|
> | `interiors` | 3.05 | **0.87** | **3.5×** | fill/pset-bound ✅ |
> | `dpaint` | 3.29 | **1.16** | **2.8×** | pset-bound ✅ |
> | `pixelperfect` | 3.63 | **1.55** | **2.3×** | pset_rgb ✅ |
> | `cityplan` | 11.4 | **5.0** | **2.3×** | pset-bound ✅ |
> | `lotfill` | 4.43 | **3.23** | **1.4×** | fill-bound ✅ |
> | `shadelab` | 1.43 | 1.28 | ~1× | rectfill-bound |
> | `caustics` | 3.88 | 4.13 | **0.94×** | rectfill-bound ⚠️ |
> | `raymarch` | 4.71 | 4.97 | **0.95×** | rectfill-bound ⚠️ |
>
> **The split is the whole point:** the canvas removes `rlVertex3f` (the per-pixel `pset` cost), so it
> helps only carts that *had* that cost. The CPU-shaders already paint via `rectfill` spans (no
> `rlVertex3f` to remove), so the per-frame 256KB `UpdateTexture` is a small *added* tax — `caustics`
> +0.25ms ≈ exactly the upload. **The key implementation lesson:** fills must stay **span-based**
> (`sw_fillrect` = cbuf row-memset). A first cut that routed `rectfill` per-pixel through `plot_pat`
> was **2.4× *slower* than GPU**; the memset flipped it to 2.3× faster. **Deployment:** per-cart /
> opt-in (default on for pset-bound carts; leave rectfill-bound on GPU) — which is why it's
> flag-gated, not a blanket default. (Phase 2 ported `spr`/`print`/`tritex` + `camera_ex` zoom; the
> 2026-06-25 rotation port added `rectfill_rot`/`spr_rot`/`sspr_ex` in SW — only `print_rot` and
> rotating `camera_ex(angle)` still fall back to GPU. See the rotation note below.) Full write-up:
> [`software-canvas-phase0-plan.md`](software-canvas-phase0-plan.md).

> **Map-cart fleet A/B (2026-06-24, after the `map()` port).** The Phase-0 table above had no
> tilemap carts — `map()` rendered blank under the canvas until it was ported (raw `DrawTexturePro`,
> no SW branch), so map carts couldn't be measured on it at all. Re-measured a **map-heavy,
> rotation-free** set (rotation would trip `sw_force_gpu` and silently measure GPU instead). Numbers
> are `workMsAvg`, ~average of two 120-frame runs (single runs are noisy — `max` especially):
>
> | cart | GPU | canvas | effect | draws |
> |---|---|---|---|---|
> | `crowd` | ~6.95 | **~1.82** | **3.8×** ✅ | 45 spr + 23 ovalfill |
> | `advancewars` | ~3.08 | **~1.55** | **2.0×** ✅ | 23 spr + map + fills |
> | `sensi` | ~2.2 | ~1.5 | ~wash (noisy) | 40 line + ovalfill + spr |
> | `heroes` | ~0.94 | ~1.20 | ~0.8× | 66 rectfill |
> | `opwolf` | ~1.07 | ~2.88 | **0.37×** ❌ | line + circfill |
> | `gta` | ~0.43 | ~1.56 | **0.28×** ❌ | 93 line |
> | `pizzatycoon` | ~0.26 | ~0.98 | **0.27×** ❌ | circfill/pset/oval |
>
> **Same split as Phase 0, now confirmed on tilemap carts:** the canvas adds a ~constant **~1ms
> floor** (the 256KB `UpdateTexture` + per-pixel CPU primitive cost), so it wins only where GPU cost
> is high enough to repay it. **Crossover ≈ 2–2.5ms GPU:** above it the draw-heavy map carts win big
> (`crowd` 3.8×, `advancewars` 2.0×); below it the light carts go *slower* (`gta` 0.4→1.6ms). So
> `map()` was **correctness-first** (it was broken), and as a bonus it made the heavy map carts
> *eligible* for the 2–4× win — it did not change the per-cart opt-in rule.
>
> **How to reproduce** (the harness, so the next person doesn't re-derive it):
> ```
> # GPU baseline vs software canvas — workMsAvg per cart, from build/perf.json
> node tools/profile-fleet.js crowd advancewars sensi heroes opwolf gta pizzatycoon --frames 120
> DE_SOFTWARE_CANVAS=on node tools/profile-fleet.js  …same carts… --frames 120
> ```
> `profile-fleet.js` runs each cart headless under `play.js` and reads `workMsAvg`/draw-call counts
> from `perf.json`; it inherits the environment, so `DE_SOFTWARE_CANVAS=on` reaches the cart binary.
> Gotchas: (1) **zsh doesn't word-split an unquoted `$VAR`** — pass cart names inline or use
> `${=VAR}`, else the whole list is taken as one cart name and every run "SKIP"s. (2) **Avoid carts
> that call `camera_ex(angle≠0)`** — only a rotating *camera* still trips `sw_force_gpu` now (rotated
> *primitives* render in SW since 2026-06-25), so you'd be timing the GPU path under an `=on` flag.
> (3) Two runs minimum; `workMsAvg` swings ~20% and `max` is dominated by one-frame startup spikes.

> **Rotation-cart fleet A/B (2026-06-25, after the rotation port).** Now that rotated primitives run
> in software, the carts that previously force_gpu'd can finally be measured — this is **what software
> rotation costs vs free GPU geometry transform** (workMsAvg, ~avg of two 120-frame runs):
>
> | cart | GPU | canvas | effect | why |
> |---|---|---|---|---|
> | `drawall` | ~1.35 | **~0.67** | **2.0×** ✅ | fill/pset/blit-heavy — rotation is a small slice |
> | `cityview` | ~2.38 | ~2.73 | 1.15× | tritex-bound (1739/frame) — **but this is pre-opt; see below** |
> | `masseffect` | ~1.01 | ~1.28 | 1.27× | `spr_rot` inverse-map vs free GPU rotate |
> | `sloppytext` | ~0.38 | ~0.49 | 1.3× | rotated glyphs |
> | `rottext` | ~0.32 | ~0.89 | 2.8× | rotated text — per-glyph inverse-map, the priciest relative |
>
> **The tradeoff, quantified:** software rotation is *slower* than GPU rotation (15–180%) — expected,
> the GPU transforms geometry for free while SW inverse-maps per pixel. But (a) absolute times are
> tiny — worst is `cityview` ~2.7ms vs the 16.6ms/60fps budget; `rottext`'s "2.8×" is 0.32→0.89ms — and
> (b) the canvas still **wins** when the cart is fill/blit-heavy: `drawall` (every primitive *incl.*
> all rotation) is **2× faster** on the canvas. So a cart only nets a penalty if it's *rotation-pure*
> (`rottext`); the per-cart opt-in / auto-fallback model leaves those on GPU.
>
> > **`cityview` FLIPPED post-optimization (2026-06-25): now GPU ~2.61 vs canvas ~2.35 — SW ~10%
> > *faster*** (was 1.15× slower above). It's tritex + span-fill bound (1731 `tritex` + 676 `trifill` +
> > 311 `polyfill`/frame, near-zero rotated primitives), and **Opt A (`img_texel`)** made the 1731
> > per-pixel tritex samples cheap enough that the span fills' draw-call-count win (cbuf memset vs ~1000
> > GPU calls) tips the whole cart to SW. It runs fully on the canvas (no `camera_ex`, byte-identical).
> > The lesson: the sampling optimizations didn't just speed the SW path — they **moved a cart's HW/SW
> > verdict** across the line. (The other rows here are still the pre-opt snapshot; `cityview` is the one
> > that crossed over.)

> **Rotated-sampling optimization (2026-06-25) — baseline + the wins.** Target: the CPU blit/sampling
> path (`de_cpu_img_rot`/`sw_blit`/`sw_tritex`). Worst-case cart `tools/carts/rotstress.c` (spinning
> sprite storm, `RS_COUNT`/UP-DOWN; profile with `DE_SOFTWARE_CANVAS=on DE_AUDIO=off`, sample
> `build/rotstress-dbg`). **BASELINE (`workMsAvg`, software canvas, M1):**
>
> | sprites | GPU | canvas | slowdown |
> |---|---|---|---|
> | 400 | 0.34 | 3.17 | 9.3× |
> | 1000 | 0.47 | 4.47 | 9.4× |
> | 2000 | 0.73 | 7.39 | 10.1× |
> | 4000 | 1.14 | 12.70 | 11.1× |
>
> Sample leaves @4000 (self-time; `-O2` inlines `de_cpu_img_rot` into `spr_rot`): `spr_rot` **1799**
> (the inverse-map loop) · `GetImageColor` **377** (per-pixel gather) · `sw_pset` **244** (plot).
>
> **Three wins applied (all byte-identical — `canvas-diff` 0px on masseffect/cityview/rottext after
> each; these are determinism-critical):**
> - **A — direct texel read** (`img_texel()`): skip the non-inlined raylib `GetImageColor` call + its
>   per-pixel format `switch` with a typed RGBA8 load (falls back for other formats; `spritesheet_img`
>   forced to RGBA8 at load). Used by `sw_blit`/`sw_tritex`/`de_cpu_img_rot`.
> - **B — no-scale fast path**: `de_cpu_img_rot` skips the per-pixel `(fxu*sw/dw)` mul/div when
>   `sw==dw && sh==dh` (the `spr_rot` case) → `sx + (int)fxu`. Byte-exact at SPRITE_SIZE=16 (power of
>   two) and matches the `rotspr` probe's `floor` sampling.
> - **C — inline the plot**: on the canvas path, plot straight to the cbuf via `sw_pset`
>   instead of the `pset_rgb → pset → sw_pset` dispatch chain (the off-canvas `DE_CPU_RASTER` reference
>   keeps `pset_rgb`).
> - **D — hoist camera + row pointer** (the biggest): a `-fno-inline` profile after A–C showed the cost
>   split between the inverse-map loop and the *plot chain* (`sw_w2s` recomputing the **constant** camera
>   offset per pixel; `sw_plot1` recomputing the bottom-up row index per pixel). For the canvas zoom-1
>   case, hoist the camera offset out of the loop and the cbuf row base per output row → the inner write
>   is just `row[sx] = pack`. (zoom≠1 keeps `sw_pset`'s footprint fill; off-canvas keeps `pset_rgb`.)
>
> **Result (`workMsAvg`, 2-run avg):**
>
> | sprites | baseline | A+B+C | +D (final) | total gain | slowdown vs GPU |
> |---|---|---|---|---|---|
> | 400 | 3.17 | 2.65 | **2.30** | −27% | 9.3× → 7.0× |
> | 1000 | 4.47 | 3.88 | **3.30** | −26% | 9.4× → 7.0× |
> | 2000 | 7.39 | 5.51 | **4.73** | −36% | 10.1× → 6.9× |
> | 4000 | 12.70 | 9.81 | **8.14** | −36% | 11.1× → 7.3× |
>
> ~⅓ off the rotated-sampling path, byte-identical, no SIMD. Post-D the `-fno-inline` profile is
> `GetImageColor` **gone** (377 → 2; A worked) and the time split between the inverse-map loop math and
> the now-minimal cbuf write.
>
> **Ceiling measured — CPU micro-opts are tapped out (2026-06-25).** Tested on rotstress: `-ffast-math`
> ≈ **+4–5%** (and it reintroduces the cross-arch drift the quantized matrix fixes — stays **off**);
> `-O3 -mcpu=native` (the compiler's full auto-vectorizer + NEON) ≈ **+5%** over `-Os`. When clang with
> native NEON finds only 5%, that *is* the SIMD ceiling: the rotated loop is **gather + branch bound**
> (each pixel inverse-maps to a random source texel, then branches on alpha/colorkey), which doesn't
> vectorize. So **hand-NEON isn't worth it here** — SIMD's home is the *contiguous* ops (`cls`, solid
> `sw_fillrect`, the cbuf clear/upload), not this gather. One free, portable, deterministic lever
> remains: building the runtime `-O2`/`-O3` instead of `-Os` (~5%) — a build-policy call in
> `main.cjs`/`play.js`, not code.
>
> **The ~7× residual vs GPU is inherent, and that's the design telling the truth.** The software
> canvas's reason to exist is the **per-pixel** work — the `pset`/fill-bound carts that win 2–4×, plus
> capabilities the GPU does badly or not at all (`pget`, full-screen palette cycling, read-modify-write
> `fade`/blend, cross-device determinism). Rotation is the GPU's strength (it transforms geometry for
> free), so SW will always trail there by ~an order of magnitude no matter how tight the loop. The
> takeaway is **not** "keep optimizing rotation" — it's "rotation is cheap enough in SW to be *correct
> and deterministic* (8ms for 4000 spinning sprites, in budget), and if a cart is ever genuinely
> rotation-bound, the answer is to put *that layer* back on the GPU (see the hybrid note below), not to
> chase CPU cycles." The `uint8` paletted buffer (Fork-1, 4× smaller upload) is still the lever for
> low-end, where the upload, not the math, is the ceiling.
>
> **Hybrid (the "GPU-rotate, keep the rest SW" question) — yes, but composite, don't read back.** The
> literal "rotate on GPU and get the pixels *back* into the cbuf" is a **loser**: a GPU→CPU readback
> (`glReadPixels`/`LoadImageFromTexture`) stalls the pipeline ~1–5ms *per readback* — worse than the
> whole 8ms SW path. The viable shape is to composite on the GPU side, no readback:
> - **Whole-view rotation** (`camera_ex(angle)`): render the world to `cbuf` in SW (every per-pixel win
>   intact), then **rotate the canvas texture at the final present** (one quad — free). This is **Fork-2
>   Option 3 / B** above; the catch is a screen-space HUD needs a layer split (draw world rotated, then
>   HUD unrotated on top). It replaces today's whole-cart `sw_force_gpu` for the rotating-camera case.
> - **Rotated sprites as a top layer** (player/bullets/FX over a SW world): keep the base SW, then draw
>   the `spr_rot`/`sspr_ex` calls via GPU `DrawTexturePro` **on top of the presented canvas** — GPU
>   rotation, no readback. Constraint: those sprites must be a top layer (z-order: SW base, GPU rotated
>   over it); SW drawn *on top of* a rotated sprite needs a flush/interleave (the compositing cost).
>
> Both trade the rotated layer's **determinism + free any-z-order** for GPU speed — a per-cart choice,
> and the natural evolution of Option C (keep the SW base instead of forcing the whole cart to GPU).
> Demand-gated: no shipped cart is rotation-bound enough in SW to need it yet.

> **Implementation sketch — Case 1 (present-time whole-view rotation).** The MVP is essentially a
> 2-spot change (line refs as of 2026-06-25):
> - **State:** a per-frame `static float present_rot = 0;`, reset at the top of each frame.
> - **`camera_ex()` (~`studio.c:3199`):** today `if (angle != 0.0f) sw_force_gpu = true;`. Instead,
>   when the canvas is enabled, **stash the angle and DON'T force GPU**: `present_rot = angle;`.
>   Translation + zoom keep flowing through `sw_w2s` exactly as now — only the *angle* is deferred, so
>   every primitive still rasterises axis-aligned into the cbuf (no per-pixel rotation, no staircase).
> - **Present blit (~`studio.c:1594`):** today `origin {0,0}, angle 0`. Rotate the whole frame about
>   the screen centre — `origin = {W*SCALE/2, H*SCALE/2}` (shift `dst` so centre maps to centre),
>   `angle = present_rot`. One GPU quad rotates the entire 320×200 canvas for free.
>
> That MVP proves the win on a pure rotating-world cart, shipping with two known limits:
> - **Corners go empty** (rotating a screen-sized image). *Phase 3:* render the world layer oversized
>   (~the diagonal, ≈377×377) and crop after rotation — a bigger cbuf + an adjusted `sw_w2s` centre.
> - **A HUD rotates too** (same cbuf). *Phase 2 — the HUD layer split:* route writes to `cbuf_world`
>   while a rotating camera is active, to `cbuf_hud` once it resets (`camera()`/`angle 0`); at present,
>   rotate world then blit hud flat on top. (A second cbuf + a "which layer" pointer flipped on the
>   camera-rotation state.)
>
> **Determinism survives where it counts:** the cbuf (the world) is still SW → bit-identical across
> devices, so cbuf-hashed replays/ghosts are unaffected; only the final GPU present-rotation (the
> player's view) is device-dependent, which is fine. **Verification wrinkle:** `--dump`/`canvas-diff`
> read `canvas.texture` (the *un-rotated* cbuf) — great for the deterministic base, but to check the
> rotation itself you compare the *post-present screen* (a new capture mode, or eyeball). **Effort:**
> MVP ≈ an afternoon (the 2-spot change + per-frame reset + a `camera_ex(angle)` test cart); Phases 2–3
> are the real work, needed only once a cart wants a HUD over the rotating world or full-corner fill.
> **Phase 2 loose-ends sweep (2026-06-24).** "Feature-complete for common carts" was over-claimed —
> an audit of every `Draw*` call in `studio.c` found four primitives still hitting the GPU with **no
> `sw_canvas_active` branch**, so they silently vanished (or half-rendered) under the canvas. Now
> ported / made consistent:
> - **`map()`** — the worst gap (tilemaps are everywhere): each tile was a raw `DrawTexturePro`, so
>   tilemaps rendered **blank** under the canvas. Now routes through `sw_blit` exactly like
>   `spr`/`sspr`. *Verified visually:* `gta`/`advancewars` terrain renders correctly (was blank
>   before). At `map_scale 1` the blit sampler is the identity (`i*sw/dw == i`), so it's pixel-exact;
>   scaled tiles inherited the nearest-sampling caveat below (since FIXED — see that bullet).
> - **`rectfill_rgb()`** — `pset_rgb` was ported but its rect sibling wasn't; the true-colour
>   CPU-shader bars disappeared. Now a `sw_fillrect` row-memset (memset path → byte-exact).
> - **`print_outline()`** — only the inner `print()` was sw-aware; the 8 outline passes went to GPU,
>   so the outline/shadow was missing. Outline passes now use `sw_print` (the outlines now appear).
> - **`print_rot()` / `print_rot_scaled()`** — rotated text now renders in SW too (`de_cpu_print_rot`,
>   inverse-map per glyph). `deg==0` (+ `scale==1`) still uses `sw_print`; the rotated/scaled case no
>   longer falls back. Verified byte-exact on `sloppytext`/`rottext`/`text-fx`. See the rotation note.
>
> *Gate:* `build-all` = 433/433 compile.
>
> **Verification caveat — "byte-identical GPU↔SW" does NOT hold cart-wide, and an earlier draft of
> this note wrongly claimed it did.** The canvas is byte-exact only for `pset`/`pset_rgb` and the
> memset fills (`rectfill`/span fills). It is **deliberately not** byte-identical for:
> - **`line()`** — the canvas uses `sw_sline` (the reflection-symmetric per-axis DDA), which differs
>   from GPU `DrawLine` on diagonals *by design* (that's why `sline` exists). So any line-drawing cart
>   shows a scatter of differing pixels that is correct, not a regression. Measured: `gta` (lines +
>   `map`, no `pal`) = **203/64000 px (0.3%), visually identical**. *Why a CPU line at all, and DDA
>   vs coverage:* [`rasterization-consistency.md`](rasterization-consistency.md) (the design decision —
>   `line()` is the last GL-picks-pixels primitive) + the playable magnified demo
>   `tools/carts/linecompare.c` (`B` cycles DDA / coverage / perp-offset). The DDA-vs-coverage
>   trade-off and the "split by use" finding are in the §"DDA vs coverage for the line" note below.
> - ~~**scaled blits** (`spr`/`sspr`/`map` with scale, `tritex`) — the CPU nearest sampler
>   (`sx + i*sw/dw`) rounds differently than GPU POINT-filter at footprint boundaries.~~
>   **FIXED (2026-07-02):** `sw_blit`'s scaled path and `sw_zoom_rect` now sample at the **dest
>   pixel centre** — `floor((i+0.5)*sw/dw)` — the GPU POINT-filter convention DrawTexturePro
>   rasterizes with (`tritex` already matched via the stritex convention). Integer-ratio scales
>   (2× text/tiles) were identical under both formulas; non-integer ratios were phase-shifted up
>   to one texel (drawall's 16→24 `sspr`: 109px → ~63px). `cityview/masseffect/advancewars` still 0px.
>
>   **The residual ~63px is a nearest-neighbour BOUNDARY TIE, not a bug (accepted 2026-07-10).**
>   At a fractional scale, some dest pixels map to an *exactly integer* source coordinate — e.g. the
>   16→24 (1.5×) `sspr`, dest-local rows/cols `≡1 (mod 3)` land on source `N.0`. `floor((i+0.5)*sw/dw)`
>   in C floors `1.0`→`1`; the GPU computes the same product in 32-bit float during UV interpolation,
>   lands a hair under (`0.9999…`) and `GL_NEAREST` floors it to `0`. Which side wins the tie is
>   GPU/driver-dependent, so byte-equality here was never portable — it only held on the machine the
>   fix was blessed on. `canvas-diff drawall` now carries a `// canvas-diff: max 64` directive (the tool
>   reads it as the default budget) and drawall keeps BOTH a fractional `sspr` (the ~63px, exercising the
>   sampler at a non-integer ratio) and a tie-free 2× integer `sspr`. Integer *upscales* (2×, 3×) never
>   hit a tie; only fractional ones do.
>
>   **canvas-diff is a PARITY oracle — it does NOT byte-guard the sampler's absolute behaviour.** Proven
>   the hard way (2026-07-10): reinstating the pre-fix truncation (`i*sw/dw`) made drawall's diff *shrink*
>   63→48px — still a PASS — because on a GPU that rounds the boundary tie down, truncation agrees with the
>   GPU *better* than the correct center-sampling. And the 2× integer `sspr` can't catch it either:
>   `floor((i+0.5)/2) == floor(i/2)` for all i, so 2× is byte-identical under both samplers. A parity
>   oracle measures GPU↔SW *agreement*, which a subtle sampler change can silently *improve*. To lock the
>   SW sampler's own output, a golden byte-check is the right tool — canvas-diff only catches GROSS breakage
>   (wrong sprite / off-by-many) that blows past the budget. **That golden now exists:**
>   `node tools/canvas-diff.js <cart> --golden` renders ONLY the software canvas and pixel-compares each
>   frame to a committed golden (`tools/canvas-golden/<cart>/`), `--bless` to record. Verified 2026-07-10:
>   with the truncation bug reinstated the parity run PASSes (48px < 64) while `--golden` FAILs (109px vs
>   golden) — the golden catches exactly what parity can't.
>
> So an all-frames `shasum` A/B is the **wrong oracle** for a line-drawing cart — use a
> bounded pixel-diff (`magick compare -metric AE`) + an eyeball, and reserve byte-equality for the
> pset/fill/blit primitives (the `swcanvas_test` shasum still holds for those).
>
> **Tools for verifying a canvas change** (and where they fit in the wider gate map —
> [`../guides/checks-and-oracles.md`](../guides/checks-and-oracles.md)): **`canvas-diff.js`** (the
> GPU-vs-canvas oracle — bakes in the `sw_force_gpu` guard, the `DE_CPU_RASTER` reference, and a
> `--bytecheck` shasum mode), the **`swcanvas_test`** probe cart (byte-identical pset/fill check),
> **`mirror-diff.js`** (symmetry), **`road-check.js`** (the coverage-field road oracle), and
> **`profile-fleet.js`** for the perf A/B (recipe under the map-cart table above).
>
> **`DE_CPU_RASTER=on` neutralises the GL-vs-CPU rasterization diff for A/B (2026-06-24, renamed +
> generalised from `DE_CPU_LINE` 2026-06-25).** Some primitives GL rasterizes *differently* from the
> software canvas — `line()` (GL `DrawLine` vs `sw_sline`), `rectfill_rot` (GL `DrawRectanglePro` vs the
> inverse-map fill), and `tritex` (GL quad vs `sw_tritex`). The flag routes all of them through the
> **same CPU rasterizer even off-canvas** (`de_cpu_line` via `pset`; `de_cpu_rectfill_rot` via `pset`;
> `sw_tritex` via `pset_rgb`), so the GPU reference build and the canvas build draw the **same pixels**.
> The clean A/B is `DE_CPU_RASTER=on` (reference) vs `DE_SOFTWARE_CANVAS=on`. Measured: `gta` **203 → 0**,
> `combo` **16940 → 348** (the 348 is its audio-reactive disc, not lines), and after the `rectfill_rot`/
> `tritex` routing **`cityview` 48 → 0** (a rotated-rect + tritex cart now byte-exact). Default **off** —
> shipping look unchanged; A/B hygiene only, **not** a direction-1 commitment (the coverage-vs-DDA
> `line()` decision is still open, see §"DDA vs coverage for the line" below). `canvas-diff` sets it on
> the reference automatically (`--raw` opts out to measure the divergence). It grows as more rotated
> primitives port to SW. SEAMs in `studio.c` at `de_cpu_line` / `de_cpu_rectfill_rot`.

> **Rotation Tier-1 ported (2026-06-25) — rotated primitives now render in software.** Following the
> scoped plan (conventions already settled by the det-probes), the rotated *primitives* came off
> `sw_force_gpu`:
> - **`rectfill_rot`** → `de_cpu_rectfill_rot` (inverse-map fill, `rotfill.c` convention).
> - **`spr_rot` / `sspr_ex(deg)`** → `de_cpu_sspr_rot` (inverse-map **nearest** sprite, `rotspr.c`
>   convention — nearest == GPU point-filter quality, device-deterministic; RotSprite is the future
>   ≥16px opt-in, not built). Reuses `sw_blit`'s colorkey + `pal()` recolor; plots via `pset_rgb`.
> - **`tritex`** was already SW; it now also honours `DE_CPU_RASTER` (plots via `pset_rgb`).
>
> All quantize the rotation matrix to 1/4096 (cross-device determinism) and match raylib's `[[c,-s],
> [s,c]]` so rotation direction agrees with GPU. **Milestone:** `cityview` (rectfill_rot + tritex +
> line) and `masseffect` (spr_rot + sspr_ex) now render **fully on the software canvas with zero GPU
> fallback** — the first rotated-primitive carts to do so — and A/B byte-exact under `DE_CPU_RASTER`.
> `build-all` 433/433. **Update (2026-06-25): `print_rot`/`print_rot_scaled` ported too**
> (`de_cpu_print_rot` — a glyph is a tiny sprite, so each glyph inverse-map-rotates about the shared
> text pivot via `de_cpu_img_rot` in font mode; verified byte-exact on `sloppytext`/`rottext`/`text-fx`).
> **So Tier-1 is complete — the ONLY primitive left on `sw_force_gpu` is rotating `camera_ex(angle)`**
> (Tier 2, the demand-gated whole-view case — stays a correct GPU fallback). Quality knobs (RotSprite
> ≥16px, Xiaolin-Wu smooth strokes) remain opt-in futures, not needed for correctness.
>
> **Separately, `facegen` surfaced the still-open `zoom_rect`/`smooth_zoom` gap** — those sample
> `canvas.texture` mid-`draw()`, which is stale under the canvas (uploaded only at end-of-frame), so
> the magnified inset garbles. Distinct from rotation; the fix is a cbuf read-magnify-write.
> **Update (2026-07-02): both halves closed.** `zoom_rect` got exactly that — `sw_zoom_rect`, a pure
> cbuf read-magnify-write (dest-pixel-centre nearest, GPU POINT parity). `smooth_zoom` turned out to
> be a different bug class: its `EndTextureMode`/`BeginTextureMode(smooth_rt)` capture dance ran
> mid-sw-frame with **no render target open**, and `smooth_composite`'s leaked
> `BeginTextureMode(canvas)` then swallowed the present — the window froze on the last good frame
> (bit `sloop`: `smooth_zoom` + fractional speed-zoom under `DE_SOFTWARE_CANVAS`). Fix: skip the
> capture when `sw_canvas_active` — the canvas already renders fractional zoom natively (`sw_w2s`
> axis-aligned scale, Option 2), so smooth_zoom is a GPU-path-only device by construction.
>
> **Cross-device determinism (goal B) — verified for the algorithms, two gaps left open (2026-06-25).**
> The drift this guards against is real: raw libm `cosf`/`sinf` differ ~1 ULP across arm64 / x86-64 /
> wasm, which flips a per-pixel `floor`/compare → different pixels per device. The fix — quantizing the
> rotation matrix to 1/4096 (`roundf(cosf(a)*4096)/4096`) — is in every rotated primitive, and
> `bash tools/det-probes/run.sh` confirms all 7 rotation probes (`rotfill`/`rotspr`/`rotline`/
> `rotstroke`/`textrot`/`stritex`/`detstress`) are **bit-identical across arm64 / x86-64 / wasm**. The
> engine ports copy those algorithms verbatim and otherwise use only IEEE-deterministic ops, so the
> property transfers. **Two honest gaps, deferred (not worth the cost yet):**
> 1. **No engine-level cross-arch gate.** The probes test the *standalone* algorithm, not `studio.c`
>    end-to-end; the port is faithful by inspection only. A true gate would have to hash a cart's
>    framebuffer on all three targets — but the full engine can't be cheaply built here (brew ships
>    only an **arm** `libraylib.a`; wasm needs a GL context, so it can't run headless in `node` like a
>    probe). The achievable route is a **raylib-free headless shim** for the `studio.h` surface the SW
>    path touches (route `pset`/blits → `sw_cbuf`, hash it), which builds clean on arm/x86/wasm like
>    the probes — real work, deferred. Interim discipline: when a rotated rasterizer changes, keep its
>    mirror probe in `det-probes/` in sync and re-run `run.sh`.
> 2. **Quantization proven for integer-degree angles only.** `run.sh` sweeps 0–359° (integer). An
>    arbitrary float angle could, very rarely, hit a knife-edge where `cosf(a)*4096` lands within ~1
>    ULP of a half-integer and two libms round differently → a 1-px / 1-frame divergence. Negligible
>    per frame, but non-zero over a long lockstep replay. The fix when rigour demands it: **quantize
>    the angle too** (or a table/CORDIC trig) so the `roundf` input can't straddle a boundary.
>
> **`sw_force_gpu` makes naive A/Bs lie.** A cart that calls `spr_rot`/`sspr_ex(deg)`/`rectfill_rot`/
> `camera_ex(angle)` trips the sticky GPU fallback **on the frame it first hits the call** — so frame
> 0 is a partial canvas render and frames 1+ run entirely on GPU. An A/B on such a cart (e.g.
> `masseffect`) is comparing GPU-to-GPU after frame 0 and proves nothing about the canvas. Pick a
> rotation-free cart to actually exercise the SW path.
>
> **`pal()` / `colorkey()` in `sw_blit` — DONE (2026-06-24).** `sw_blit` now takes a `use_pal` flag
> (true for `spr`/`sspr`, false for `map`/`tritex` — matching which GPU paths wrap the swap shader);
> `sw_recolor()` is the CPU twin of `PAL_FS` (nearest base-palette entry → current palette, same
> argmin), and `colorkey()` snapshots the keyed RGB so `sw_blit`/`sw_tritex` skip it (the GPU bakes
> a transparent hole into the sheet; the canvas samples the pristine `spritesheet_img`, so it skips
> the key itself). *Verified byte-identical:* `advancewars` 20/20 (was 3897 px wrong), `chess`/`crowd`
> (pal); `05b-colorkey` 12/12, `boom`/`15-anim` (colorkey). `build-all` 433/433. **The recolor is a
> 32-entry nearest scan per texel, gated on `pal_active`** (zero cost otherwise) — fine for
> correctness; a reverse LUT is the obvious speedup if a pal-heavy cart ever profiles hot.
>
> **Still open:** nothing in this tier — `zoom_rect()` (cbuf readback: `sw_zoom_rect`) and
> `smooth_zoom()` (skipped on the canvas — `sw_w2s` renders fractional zoom natively; the GPU
> capture dance corrupted sw frames, see the 2026-07-02 update above) both resolved.

## When to enable it — HW vs SW (the per-cart rule)

The canvas is **opt-in per cart**, because it's not a universal win. The trade is simple: it replaces
**per-pixel GPU submission (`pset → rlVertex3f`)** with **CPU writes + one `UpdateTexture`/frame**. So:

| enable SW (`DE_SOFTWARE_CANVAS`) | leave on GPU (default / auto-fallback) |
|---|---|
| **pset/fill-bound** — top-down pixel-art, heavy `pset`/fills, CPU-shaders' per-pixel paint, **software mode-7** (scanline fills), the coverage-field road | **geometry / draw-call-bound** — `rectfill_rot`, line-heavy spline roads (`sloop`) |
| these *had* the `rlVertex3f` cost → **1.4–3.5× faster** | **3D / `tritex`** — the GPU's hardware texture-triangle rasterizer wins |
| measured: cityplan 2.3×, interiors 3.5×, dpaint 2.8×, mode7 2.7×, streetlab-field recovers ~60% | **rotation** (camera *or* primitive) — GPU transforms geometry per-vertex for free → auto GPU-fallback |
|  | **already-trivial** carts (sub-~0.5ms) — the ~0.4ms `UpdateTexture` upload isn't repaid (`dutchsky` 0.35→0.84) |

**The one-line principle:** SW wins when there's a lot of per-pixel `rlVertex3f` submission to *remove*;
HW wins when the GPU does the work better (geometry/texture/rotation via vertex transforms) or there
was little to remove (trivial carts pay only the upload tax). Rotation and rotated primitives
auto-select HW via the sticky `sw_force_gpu`; everything else is the env/`-DSW_CANVAS_DEFAULT` opt-in.

**Backend-consistency gotcha — a cart that never `cls()`es (2026-06-25).** Neither path clears
per-frame (that's deliberate — it's what lets carts do trail/feedback effects). But the *initial*
state differed: `sw_cbuf` is a zero-init `static` (black), while the GPU `canvas` came back from
`LoadRenderTexture` as **uninitialised GPU memory (garbage)**. So a cart that doesn't `cls()` *and*
doesn't paint every pixel (e.g. `facegen`, whose bg only covers `y<150`) showed black borders on SW
but garbage on the GPU. Fixed by clearing `canvas` to `palette[0]` **once** at creation in `studio.c`
(no per-frame clear → trails still work), so both backends start from the same black. Carts should
still `cls()`; this just makes the two paths agree when one forgets. (`facegen` also got its `cls()`.)

**The ideal cart to test Option 3** (software-raster + GPU-rotate-at-present, for *rotation* that keeps
the SW win): a cart that is **rotation-bound *and* pset/fill-bound at once** — e.g. a **heading-up
top-down pixel-art world** (GTA-1-style) whose ground is *pixel fills* (not `rectfill_rot` geometry) and
that rotates the whole view via `camera_ex(angle)`. None exist yet: `mode7` does its rotation *inside*
the fill loop (scanline `rectfill`, wins on the canvas with no Option 3); `sloop`/`cityview` rotate via
`camera_ex` but draw their roads as `rectfill_rot`/`tritex` *geometry* (GPU's turf). So the trigger is a
deliberate one — a pixel-fill rotating world (the direction the `sloop`/streetlab "big game" is heading,
*if* its ground becomes fills) — and it'd also want the HUD-layer compositing.

## The thesis

**At this resolution, a software framebuffer uploaded once per frame beats GPU immediate-mode.**
The canvas is 320×200 = **64k pixels** — tiny. The current renderer draws *into* a
`RenderTexture` on the GPU (`studio.c`: `BeginTextureMode(canvas)` + `BeginMode2D(cam)`), so every
primitive pays GPU per-vertex/per-call submission overhead — which, at 64k px, **dwarfs the actual
pixel work**. That's exactly what the profiles show: it's never the math, always `rlVertex3f`.

So: keep a CPU pixel buffer that *is* the canvas. Every primitive writes to the buffer (CPU
rasterization). Once per frame: `UpdateTexture(canvas, buf)` + the existing `DrawTexturePro`
scale-up to the window. **`rlVertex3f` disappears from the entire draw path at once** — `pset`,
every fill, sprites, lines, text. This is what PICO-8 / TIC-80 / every fantasy console does;
we're currently the odd one out doing GPU immediate-mode for a 64k canvas.

The span fills + clamp cache we just shipped aren't wasted — they're the *CPU rasterizers* a
software canvas needs (span fill = how you fill a polygon into a buffer; the clamp is the bounds
check). They become the implementation, not dead code. The dither `plot_pat` path, likewise.

## Architecture sketch

```
static uint8_t  cbuf[SCREEN_W*SCREEN_H];   // palette-index canvas (0..63), OR
static uint32_t cbuf[SCREEN_W*SCREEN_H];   // RGBA — see fork 1
// pset:        cbuf[y*W+x] = color;                    (+ clip bounds check)
// rectfill:    memset rows                             (CPU, trivial)
// spr:         blit from spritesheet, palette/pal()/colorkey in the loop
// line/circ/poly/oval/tri: the CPU rasterizers we already have, writing cbuf
// fade/blend:  read-modify-write on cbuf               (EASIER on CPU than GPU)
// end of frame: UpdateTexture(canvas.texture, cbuf); DrawTexturePro(canvas→window)  // one upload, one quad
```

> **Note (2026-06-23) — the sketch's one false assumption: there is NO CPU `line` today.** The line
> "the CPU rasterizers we already have" is true for circ/poly/oval (the `poly_fill_cov`/coverage
> family decides pixels on the CPU) but **`line()` is the sole GL holdout** — it's raylib GPU
> `DrawLine`, which *chooses* the staircase on the GPU. An audit while chasing streetlab's corner
> floor confirmed it's the only *axis-aligned* cart-facing primitive that lets GL pick pixels — the
> rest of the GL-picks-pixels surface is the rotation family (`rectfill_rot`, `spr_rot`/`sspr_ex`,
> `tritex`, rotating `camera_ex`), audited in full below. So the canvas has a hole exactly there. The missing piece is
> **`sline`** — a reflection-symmetric CPU per-axis DDA, prototyped in the probe carts
> `tools/carts/{linesym,axissym}.c` and banked (unused) in `tools/carts/streetlab.c`; recipe +
> proof in [`streetlab-corner-symmetry-plan.md`](streetlab-corner-symmetry-plan.md). It is the line
> rasteriser this architecture needs.
>
> **This sharpens the determinism case (the "free win" below), and the perf coupling.** GPU
> `DrawLine` is not just non-deterministic across drivers — it isn't even reflection-self-consistent
> (a line and its exact mirror rasterise to *different* pixels: 88 mismatches in the `linesym`
> probe). A CPU line (`sline`) is required for the bit-identical-across-devices property — it's a
> *prerequisite* for replays/ghosts/lockstep, not just a perf detail. And the coupling runs the
> other way too: a symmetric line is **unavoidably per-pixel** (a 1px line has no horizontal run to
> coalesce, unlike a span fill), so globally swapping `line() → sline` on *today's* GL renderer turns
> each line into N `pset`s — *raising* total `pset`/frame (e.g. `sloop`'s 528 lines/frame) and thus
> the priority of the very `pset` optimisation this doc is about. On the canvas, those per-pixel
> writes are cheap `cbuf` stores. So **global `sline` and the software canvas are one bet, pulling
> the same way** — and the canvas *needs* `sline` regardless. (Caveat learned the hard way: a
> symmetric line alone does **not** make symmetric *art* — fills are point-mirrored, 1px strokes are
> cell-mirrored, and on an even grid those snap conventions conflict; the canvas must pick one. See
> the plan doc's "`sline` negative result.")

> **Audit (2026-06-24) — the complete GL-picks-pixels surface is FOUR primitives, and they're all
> rotation.** Read against the current `runtime/studio.c`, the holdouts where GL's own rasterizer
> *chooses which pixels light up* (the non-deterministic, can't-just-blit cases) are exactly:
> 1. **`line()`** — raylib `DrawLine` (line ~2636). Being closed by **`sline`** (above); the only
>    *axis-aligned* one, so `sline` fully solves it for unrotated use.
> 2. **`rectfill_rot()`** — `DrawRectanglePro()` rotated quad (line ~2342).
> 3. **`spr_rot()` / `sspr_ex()`** — `DrawTexturePro(…, deg, …)` rotated sprite blit (line ~2214+):
>    GL rasterizes the tilted footprint.
> 4. **`tritex()`** — `rlBegin(RL_QUADS)` + `rlVertex2f` affine textured triangle (line ~2607, the 3D
>    carts). **The one genuinely-hard new rasterizer** — no "skip rotation" fallback freebie; it must
>    be CPU-scanline-rasterized (affine u,v interp) to live on the canvas. Phase 2's biggest item.
> 5. *(whole-block)* **rotating `camera_ex(angle≠0)`** — `BeginMode2D(cam)` with `cam.rotation` (line
>    ~2813) transforms *everything* drawn inside the block on the GPU. This is the case Fork-2/C
>    quarantines (per-frame GPU fallback), not a single primitive.
>
> Everything else is **not** GL picking pixels and is canvas-native: `pset`/`rectfill`/`rect` are
> integer-coord axis-aligned writes (→ `cbuf` stores); `circ`/`circfill`/`oval`/`poly`/`ngon`/`star`/
> `thick` are already **full CPU coverage** (span fills / per-pixel predicates) that just reroute
> their `pset`/`DrawRectangle` into `cbuf`; unrotated `spr`/`sspr` are blits (Phase-2 CPU blit loop);
> HUD `print` is the orthogonal **deferred-GPU-overlay** trick, never a pixel-picking holdout.
> **Net:** the whole remaining GL pixel-selection surface collapses to the single concept *rotation*
> — `sline` removes the axis-aligned line hole, and Fork-2/C leaves the four rotation cases on the
> GPU per-frame. `tritex` is the only one that isn't covered by either move (needs a real CPU
> rasterizer) if the 3D carts are ever to run on the canvas. The rotated-*stroke* sub-case
> (a rotated 1px `line`/outline) still needs the Xiaolin-Wu-class drawer flagged in Fork 2.

> **Findings (2026-06-24) — DDA vs coverage for the line, visualised in `linecompare`.** The "two
> rounding rules" tension (a DDA stroke and a coverage test pick *different* diagonal pixels) is now
> a playable magnified demo (`tools/carts/linecompare.c` — `B` cycles five views, `UP` fattens). It
> made three things concrete for the line primitive. (1) **DDA is intrinsically 1px** — thickness is
> an add-on. (2) Comparing the three thick-line rasterizers at 45° (the worst case), **none wins on
> all of {crisp, uniform-width, gap-free}**: *stack-DDA* (stack N cells on the minor axis) is crisp
> + gap-free but its perpendicular width *shrinks with angle* (≈3÷√2 at 45°); *coverage capsule* is
> uniform-width + gap-free but raggeder-edged and rounds unlike DDA; *perpendicular-offset DDA* (N
> parallel 1px passes) is crisp + uniform-width but **gappy** at diagonals (the passes don't
> 8-connect), and closing the gaps with denser passes just converges back to coverage. (3) **The
> clean answer is a split by use, not one rasterizer:** **1px strokes → DDA** (`sline`; crisp, and
> thickness is moot at 1px), **thick bands → coverage** (the only gap-free *and* uniform option) —
> which is exactly what [`field-based-road-rendering.md`](field-based-road-rendering.md) independently
> chose (lane lines = coverage thresholds, bands = coverage). So `sline` stays the canvas's 1px line;
> anything with width comes from the coverage rule, not a thickened DDA. (4) A coverage line also
> needs a **cap-style** parameter (`linecompare`'s `C` cycles three). *Round* caps (the capsule) make
> a thick line grow *longer* — a semicircle of radius = half-thickness past each endpoint. *Butt* caps
> cut flush (match DDA's ends) but can clip the tip pixels of a thick diagonal. *Square* is the useful
> in-between: a rectangle extended a **fixed ~1px** past each end — always covers the endpoint pixels,
> but the extension is *bounded* (doesn't grow with thickness like round does). Structural bands (a
> road meeting a junction, an edge touching a node) want **butt or square**; brush strokes want round.
> Default butt/square for the canvas, like every 2D API's `lineCap`.

## The design forks — decide these first

**These forks are not equally hard.** Fork 2 (`camera_ex`) genuinely *gates* the design; the
other three have clear answers once Fork 2 is committed. Read in that order.

### Fork 1 — RGBA (`uint32`) vs index (`uint8`) buffer

There's an elegant index option worth naming first: **`uint8` index buffer + a palette-LUT shader
at the final upload** — upload the byte buffer as an R8 texture, and the existing `DrawTexturePro`
runs a tiny fragment shader mapping `index → palette[index]`. That makes `pset`/`spr`/fills byte
writes (cheapest, native to the 0–63 model), `pal()` a remap, and — the real prize — **full-screen
palette *cycling* free** (animate the LUT uniform: the classic water/fire trick).

**The dealbreaker is `pset_rgb`.** The CPU-shader trilogy (shadelab/caustics/raymarch) and any
true-color gradient paint arbitrary 24-bit, which can't live in a `uint8`. You'd need a parallel
RGBA plane only-when-`pset_rgb`-is-used → two planes to composite per pixel → true-color becomes a
second-class citizen when it's actually a first-class teaching feature.

**Recommend RGBA.** 256KB is nothing, `pset_rgb` is native, `pal()` recolor applies cheaply at the
write (LUT lookup before the store), one uniform path, no dual-plane compositing. Name the two real
losses so the choice is informed:
- **Cheap full-screen palette *cycling*** — RGBA bakes the colour at write, so cycling means
  redrawing. No evidence carts use screen-wide cycling today (`pal()` is per-draw sprite recolor,
  ADR-0007), so this is a hypothetical loss, recoverable later as an opt-in paletted mode.
- **Index-based blend tables** (parked STATUS-18, [`blend-tables.md`](blend-tables.md)) are inherently index-space
  LUTs; an RGBA canvas would blend in RGB instead — arguably more flexible, but it changes how that
  future feature would be built.

> **But "Recommend RGBA" is M1/desktop-correct, NOT universal — it's hardware-conditional.** On a
> small-cache / narrow-bus target the `uint8` index buffer flips straight back into contention, for
> two reasons RGBA can't answer: (1) **cache fit** — 64KB (indexed) lives in a small L2 where 256KB
> (RGBA) spills to main RAM every frame; (2) **smaller memory passing** — the per-frame
> `UpdateTexture` is an R8 upload, **4× less data**, which on a narrow bus is real budget (≈15–25% of
> a frame on a Pi Zero, vs <1% on M1 unified memory). That can outweigh the `pset_rgb` dealbreaker
> (handle true-color via the parallel RGBA plane *only when `pset_rgb` is used*, or simply don't
> offer true-color in the low-end profile). So **`uint8` is the low-end build profile**, not a dead
> option — same `DE_SOFTWARE_CANVAS` machinery, selected by a buffer-format compile flag. See
> "Low-end / small-memory targets" in the build runbook below.

### Fork 2 — `camera_ex()` zoom/rotation (the one that gates)

Today `BeginMode2D(cam)` gives zoom/rotation free on the GPU. Three ways to do it in software, two
with killers:

- **(A) Software-transform every primitive** (rotate/scale coords before rasterizing). **Killer:**
  rotation re-opens *exactly* the gap/seam class `rasterization-consistency.md` spent four sessions
  killing — a rotated software fill staircases into holes — and now *every* primitive (even `pset`,
  even `spr`) has it, plus sprites become affine texture-maps. Re-fighting solved bugs across the
  whole draw layer.
- **(B) Render un-transformed into `cbuf`, GPU-sample it transformed at upload** (one rotated/zoomed
  quad). Keeps camera cheap and primitives axis-aligned (no gaps). **Killer:** a cart that draws
  world-space *then a screen-space HUD* in one frame — most carts — can't be one flat buffer + one
  transform; the HUD rotates too. Fixing it means flush points at every `camera()` toggle, and the
  compositing complexity creeps back.
- **(C) Software-canvas the no/translation-camera case; `camera_ex` frames fall back to today's GPU
  path.** Plain `camera()` (translation) is *trivial* in software — offset the write coords
  (`pset → px-camx, py-camy`); reset to (0,0) and writes hit literal coords, so **world + HUD both
  work** with no upload-time transform. Only zoom/rotation can't be offset-written, and those carts
  use the GPU path **per frame**.

**Recommend C — and it's aimed, not a compromise.** The carts that need this are the `pset`-heavy
ones: `dpaint`, `interiors`, the CPU-shaders — **none use `camera_ex`**. `camera_ex` is rare, and
C ships the win exactly where the survey points, sidesteps both killers, and defers
rotation-in-software until there's measured demand. (The "translation-camera-only prototype" below
*is* committing to C.)

> **Built (2026-06-24) — Fork-2/C is live, and refined: zoom stays software.** Implemented in
> `studio.c` (Phase 2e, `30e4ba0b` + `74dcb8cf`; full status in
> [`software-canvas-phase0-plan.md`](software-canvas-phase0-plan.md)). The split turned out finer than
> "translation only vs everything-else GPU": **zoom is axis-aligned, so it stays on the fast software
> path** (`sw_w2s` scales coords; `sw_pset` fills the zoomed pixel's footprint; cityplan renders with
> correct zoom and still wins 2.1×). Only **rotation** falls back to GPU — and not just rotated
> *cameras* but rotated *primitives* (`rectfill_rot`/`spr_rot`/`sspr_ex`) too, via a sticky
> `sw_force_gpu`. Validated on `sloop`/`cityview` (heading-up rotating + `rectfill_rot`/`tritex`): they
> fall back and render correctly with no penalty. **Why no software-rotation yet (Option 3):** rotating
> a pre-rendered image needs the screen's rotated corners filled → an oversized (~diagonal, ~2.2×)
> render every frame, which the GPU avoids by transforming geometry per-vertex. So Option 3 only pays
> off for a **rotation-heavy AND pset/fill-bound** cart (none exist yet — every rotating cart is
> geometry-bound). Trigger to build it (+ HUD-layer compositing): a rotating top-down pixel/Mode-7
> world with heavy fills + a HUD.

> **Note (2026-06) — `camera_ex` ≠ "needs the software canvas".** `camera_ex` splits into ZOOM and
> ROTATION, and they're very different for fills. A rotation-0 camera (zoom only) is **axis-aligned
> affine**, so the disc/poly span fast-paths are byte-safe under it — and the disc gate was relaxed
> to `rotation==0` (`00dd3f6`), which **fixed `orbit` (a zoomed planet-circfill cart) 9× without any
> software canvas.** Only **rotation** (`rotation != 0`) actually forces fills onto the per-pixel
> path. So the software canvas's fill win is for the *rotating-camera* subset, which is rarer still —
> and a rotating cart that's *also* per-pixel-bound is the genuinely-open "may not exist" question
> (`orbit` was NOT it; it was zoom-only).

> **Concrete example of a different profile shape: `sloop`** (the spline road world). Profiled
> 3.5ms mean / 4.2ms p95, but **GPU-draw-call-bound, not pset-bound** (984 `rectfill_rot` + 528
> `line` ≈ 1500 GPU calls/frame; `pset` only 41). Its cost isn't *any* software fill — `rectfill_rot`
> and `line` are GPU primitives — so neither the disc/poly span fills, the zoom-gate relaxation, nor
> the software-canvas v0 (which is about the `pset` path) touch it. A software canvas would only help
> `sloop` via the *full* option-1 path (port `rectfill_rot`/`line` to CPU writes), and only then.
> It's the proof that a real non-`pset`, non-fill profile shape exists — GPU draw-call *count* — and
> that "the software canvas isn't a universal speedup" is a fact, not a hedge.
>
> > **Measured now that `rectfill_rot` is SW (2026-06-25): `sloop` GPU 3.20ms vs canvas 4.33ms — ~35%
> > SLOWER in SW.** It's the canonical *rotation-bound → stays GPU* cart (the mirror of `crowd`/`drawall`).
> > The road ribbon is **992 `rectfill_rot`/frame** (each segment a rotated rect); on GPU that's hardware
> > `DrawRectanglePro`, in SW it's 992 per-pixel inverse-map fills. With ~zero per-pixel work (`pset` 41),
> > there's no SW win to offset the rotated-fill cost. Two notes: (1) that 4.33 is the SW path *actually
> > running* — headless with no steering keeps the road straight (`camera_ex(angle≈0)`), so `sw_force_gpu`
> > never trips; the moment you steer, `camera_ex(angle≠0)` trips the **sticky** fallback and sloop runs
> > on the GPU (3.20) for the session, which is correct for it. (2) The Case-1 hybrid wouldn't help —
> > sloop's cost is 992 *individual* rotated rects (Case-2 shape), not a whole-view camera transform. So
> > sloop is the clean counter-example to the per-cart rule: **do NOT opt a geometry/rotation cart into
> > the canvas** — the canvas is for the `pset`/fill/per-pixel carts.

> **Note — *inverse* mapping is the third software option for the rotated case (and the one
> option A missed).** Option A above is *forward* mapping (transform each primitive's coords, then
> rasterize) — that's what staircases into holes. The Mode-7/SNES trick is the other direction:
> iterate the *screen-space* pixels in the shape's transformed bounding box, and for each one apply
> the **inverse** camera transform to get its shape-local coordinate, then test inside (`x²+y²≤r²`
> etc.). Because you visit every output pixel exactly once, it is **gap-free by construction** — no
> seam class to re-fight. The inner loop is cheap (two adds per pixel for a rotated step; one
> multiply for zoom), and at 64k px the math is in the noise. Two honest bounds on this:
> - It applies cleanly to **fills and source-sampling blits** (the disc/poly fill, a rotated `spr`,
>   a Mode-7 background) — i.e. *exactly* the case the 2026-06 note isolated as the only thing
>   rotation actually forces onto the per-pixel path. So if Fork-2/C ever needs to grow a
>   rotating-fill path, inverse mapping is how, without a GPU fallback.
>   > **Measured (2026-06-24) — proven in `tools/det-probes/rotfill.c`.** Inverse-mapping a filled
>   > rectangle is **gap-free at all 360°** (always 1 connected component, area stable at ~6160px)
>   > and **bit-identical across arm64/x86-64/wasm**; forward-mapping the same rect leaves up to
>   > **1166 holes (~19% of the shape)** at the worst angle. So the rotated-*fill* path is validated
>   > — gap-free *and* deterministic — confirming inverse mapping (not GPU fallback) is the tool for
>   > rotating fills. (Rotated *strokes* are still the open sub-case below; RotSprite is the quality
>   > layer for rotated *sprites*, see the note after Fork 3's region.)
> - It does **not** rescue rotated *stroked* primitives (a 1px `line`/`circ` outline). Those are
>   still rasterized forward in screen space, so they re-inherit the consistency/aliasing problem
>   (`rasterization-consistency.md`) and need a sub-pixel line drawer (Xiaolin-Wu-class) to look
>   stable while rotating. So "kill Fork 2, do *all* rotation in software" overclaims; inverse
>   mapping shrinks the hard part to stroked outlines, it doesn't erase it. Fork-2/C stays the
>   recommendation; inverse mapping is the tool *inside* C if the rotating-fill demand ever shows up.
>   > **Measured (2026-06-24) — `tools/det-probes/rotline.c` softens the "need Xiaolin-Wu" worry.**
>   > A crisp rotated 1px line via `sline` is, at *every* angle: **exactly 1px-uniform** (excess 0,
>   > no fat spots), **connected** (1 component, no gaps), and **bit-identical across arm64/x86/wasm**.
>   > So rotated strokes are *correctness*-solved with the crisp drawer we already have — no sub-pixel
>   > drawer is needed for them to be right. The *only* residual is **shimmer**: a 1° rotation can flip
>   > ~a whole line-length of pixels (churn up to 268px on a ~160px line), because crisp rotation has
>   > no sub-pixel easing. That's an **aesthetic** matter, and it's consistent with the console's
>   > existing no-AA house style — so a Xiaolin-Wu drawer becomes an **optional "smooth rotation" mode**
>   > (like RotSprite for sprites), not a prerequisite. Net: rotated *strokes* are no longer the open
>   > hard part — crisp is correct and good enough; AA is a future nicety.

> **Note — RotSprite is the quality knob for rotated *sprites* (a future opt-in, captured so it's
> not lost).** Inverse-mapped nearest-neighbour rotation (above) is correct and gap-free, but on
> low-res pixel-art sprites it jaggies and drops thin features — the same failure Aseprite's **"Fast"**
> rotation has. Aseprite's other mode, **RotSprite**, is the known fix: upscale the sprite ~8× with
> an edge-aware scaler, rotate *that*, then downscale — far cleaner rotated pixel art. For us it'd be
> a CPU rotated-`spr` path that is also **bit-identical across devices** (the goal-B property GPU
> rotation can't give). Two caveats keep it opt-in, not default: it **resamples** (invents in-between
> pixels), which can clash with a crisp fantasy-console look, and it needs an ~8× temp buffer. So:
> default = inverse-mapped nearest-neighbour (crisp, cheap, deterministic); **RotSprite = an opt-in
> "smooth rotation" mode** for when a cart wants it. Only relevant once rotation runs in software at
> all (Fork-2 demand); filed here as the answer for that day. (Algorithm: the "rotsprite" scaler, as
> used by Aseprite/LibGDX.)
> > **Measured (2026-06-24) — `tools/det-probes/rotspr.c`, three methods head-to-head.** A rotated
> > sprite is `rotfill`'s inverse mapping + a texture sample, so the *footprint* is gap-free and
> > bit-identical across arches; the question is *content* quality. Tested **nearest** (== the GPU
> > point-filter path we ship today), **4×4 supersample+majority**, and real **RotSprite** (EPX/Scale2x
> > ×3 → 8× edge-aware upscale, rotate, mode-downscale). Result, and it splits by feature type:
> > - **Thin LINES (the common, realistic case): RotSprite wins decisively** — a 1px frame stays ≤2
> >   connected pieces vs nearest's 7, and the diagonal stays continuous where nearest/supersample
> >   dash it. This is the headline RotSprite win and the reason it's the route.
> > - **A truly-lone 1px pixel: a resolution-floor edge case, and RotSprite is *not* best at it** —
> >   nearest catches the lone dot most (308/360) because point-sampling rewards any overlap, while
> >   vote-based downscales drop a sub-pixel-sized feature more (super 252, RotSprite 156). EPX
> >   rescues *connected* detail, not solitary specks; a lone pixel rotating is unstable in any method.
> > Takeaway: **what we ship now (GPU point-filter) is quality-equal to software *nearest***, so moving
> > sprite rotation to software loses nothing and adds determinism; and **RotSprite is a strict upgrade
> > for real sprite content** (outlines, connected detail) that the GPU can't easily do — confirming it
> > as the opt-in quality mode, with the honest caveat that it's about lines/shape, not lone specks.
> > **Size threshold (`textrot`, 8px "E"):** RotSprite's edge-aware upscale needs material to work with
> > — at ≤~8px all three methods are about equally rough, and nearest is the better default there
> > (cheapest, deterministic with quantized trig, and best at lone specks). RotSprite only pays off at
> > **≥~16px**. So the policy is: **tiny rotated sprites/glyphs → nearest; ≥16px → RotSprite (opt-in)**;
> > and the real fix for tiny rotated *text* is to draw it bigger (`print_rot_scaled`), not to rotate a
> > tiny glyph at all.

### Fork 3 — compositing (falls out of Fork 2)

A compositing problem only exists if some primitives draw GPU and some CPU **in the same frame**
(then cross-path z-order needs flush/interleave). The rule that dissolves it: **in software-canvas
mode, *everything* writes to `cbuf`** — painter's order is automatic (call order == pixel order).
And because Fork-2/C's fallback is **per-frame** (a frame is wholly software or wholly the GPU path,
never mixed), there's no within-frame mixing to reconcile. So "all-or-nothing per frame" isn't a
safety rule you impose — it's what C already gives you. The price it implies is Fork 4.

### Fork 4 — text / `tritex` / `spr` as CPU blits

Graduated labour:
- **`spr`** — blit a 16×16 sheet region with colorkey/`pal`/flip. Nested loop, 256 px. Easy.
- **`print`** — glyph blits from the bitmap font atlas; same shape as `spr`, 8×8/char (all `FONT_*`
  are just different source images). Easy.
- **`tritex`** — affine texture-mapped triangle (scanline fill, interpolated u,v). The one real
  rasterizer — more code, but textbook (2D affine, no perspective). Used by the 3D carts.

**Wiring requirement this surfaces:** the spritesheet + font must be kept as CPU `Image`s in RAM
(not just GPU `Texture`s), and the sprite editor's live edits must update the CPU copy. Mostly
"don't `UnloadImage` after upload," but it's a real dependency.

### The free wins (these retire existing pain, not just "also nice")

- **`clip()`** → an `if` per write, cheaper than GPU scissor. **`fade`/blend** → read-modify-write
  on `cbuf`, *easier* than the shader path.
- **`pget`/`pget_rgb`** → direct array read. Today that's a GPU readback **plus a 256KB `Image`
  alloc every frame** (the churn `profiler.md` flagged) — gone. And `pget` is **disabled on web**
  today; a CPU buffer enables it.
- **Web** → `UpdateTexture` works identically and removes `rlVertex3f`-on-WebGL (worse than native).
- **Determinism across devices — a *feature*, not just the migration oracle.** The byte-identical
  check below is framed as a pass/fail test, but it's also a capability: a frame computed entirely
  in C is bit-identical on every machine (native/web, Apple/Intel/ARM), where the GPU path can
  round/AA 1px differently per driver. That's the precondition for replays, ghost runs, and
  lockstep netcode — and it plugs straight into infrastructure we already have (the deterministic `--det` harness clock,
  the dump+shasum oracle, byte-reproducible `--wav`). Worth naming as a reason this architecture is
  *right* for a fantasy console, beyond the perf win.
  > **Measured (2026-06-24) — the determinism claim is no longer a hope; it holds for the float
  > rasterizers as written.** Three standalone oracles (`tools/det-probes/`, run `bash
  > tools/det-probes/run.sh`) lift the actual rasterizers into a no-raylib harness, hash the pixel
  > buffer in-C, and compile the *same source* three ways. Results — **bit-identical across arm64,
  > x86-64 (Rosetta), and wasm** for: (1) `sline`+`sfill` over a dense slope fan + rotating polys
  > (`detstress`), (2) software `tritex` (`stritex`, which *also* tiles a quad with `overlap=0
  > gap=0` via the top-left rule), (3) a rotated outline that stays **1 connected component at all
  > 360°** (`rotstroke`). FMA contraction is a non-issue (`-ffp-contract=on/fast/off` all agree —
  > the divides leave nothing to fuse). **So goal B does NOT force a fixed-point rewrite** — the
  > existing float code is device-stable. Two rules it surfaces:
  > - **Never `-ffast-math`** (it shifts the result — consistently across platforms, but a footgun if
  >   the native and web builds disagree on the flag).
  > - **Rotation must use a quantized/shared `sin`/`cos`, not bare libm.** A later probe (`textrot`)
  >   found raw `cosf`/`sinf` differ ~1 ULP across arm64/x86/wasm; for *integer-endpoint* rotation
  >   that washes out, but for *per-pixel* rotation (rotated fills/sprites/text) it flips a pixel and
  >   breaks bit-identity — invisible at large scales, visible on an 8px glyph. Fix: quantize the
  >   matrix (`c = roundf(cosf(a)*4096)/4096`). The non-rotated Phase-0 path uses no trig, so it's
  >   unaffected; this only constrains the Fork-2 rotation work.
  >
  > Subsequent probes extended the result to the full rotation family — `rotfill` (inverse-mapped
  > fills gap-free), `rotline` (crisp rotated strokes uniform+connected), `rotspr`/`textrot` (rotated
  > sprites/text; RotSprite for quality) — all bit-identical (with quantized trig). What's still
  > *un*proven is the hard part — *fill-vs-bounding-outline* pixel agreement (one coverage convention
  > across all primitives); each probe passes in isolation by picking its own. See
  > `tools/det-probes/README.md`.
- The scale-up to the window stays GPU (`UpdateTexture` RGBA + `DrawTexturePro` nearest-neighbour),
  so crisp pixel scaling is untouched.

## The cleanest v1 (what the forks point to)

> **RGBA `cbuf`; software-canvas for no/translation-camera frames with `camera_ex` falling back to
> today's GPU path per-frame; everything (incl. `spr`/`print`/`tritex`) writes to `cbuf`; one
> `UpdateTexture` + `DrawTexturePro` at frame end.**

Not the scary version — a well-trodden fantasy-console architecture, with Fork-2/C carving off the
genuinely-hard part (rotation) cleanly.

## Migration — and the honest catch about a partial slice

The tempting cheap first step is "route only `pset`/`pset_rgb` + fills to `cbuf`, leave
`spr`/`print` on the GPU." **It doesn't validate** — per Fork 3, mixing CPU `cbuf` writes and GPU
`spr`/`print` in one frame breaks z-order (a sprite drawn between two `pset`s would composite
wrong). So the first *validatable* prototype is either:

1. **Full draw-layer in `cbuf`** (port `spr`/`print`/`tritex` too) for no/translation-camera frames,
   `camera_ex` → GPU fallback. This is the real v1; bigger first step but it's the only honest one.
2. **Scope to carts that don't mix** — i.e. pure per-pixel carts (the CPU-shaders, a `dpaint`-like)
   that only use `pset`/fills/`rectfill` and no `spr`/`print` mid-scene. A narrower opt-in mode that
   proves the buffer + upload mechanism and the `dpaint`/CPU-shader win without porting the blits.

Then: **validate byte-identical** (dump+shasum oracle + `raster_test`) and **A/B on the fleet**
(`profile-fleet.js`) — expect `dpaint`/`interiors`/`orbit`/CPU-shaders to drop hard, everything else
neutral-or-better. If it lands, make software-canvas the default for non-`camera_ex` frames and keep
the GPU path behind `DE_SOFTWARE_CANVAS=off` for a cycle; the span-fill `DrawRectangle` fast paths
fold into the `cbuf` writers.

## Minimal slice (v0) — the smallest thing that shows a real, validatable result

The naive "route `pset`+fills to `cbuf`, leave `spr`/`print` on GPU" slice fails (Fork 3
z-mixing). But two observations collapse it into something small **and** byte-identical-checkable:

- **Target only the carts that need it and don't mix**: the CPU-shaders (caustics/shadelab/raymarch
  = `cls` + `pset_rgb` + HUD `print`), `dpaint` (`cls` + `pset` + `rectfill`/`rect` + HUD `print`),
  and `pixelperfect` (`pset_rgb` per-pixel world view + HUD `print` + 2 `line`s). No `spr`, no
  `tritex`, no `camera_ex`. These are exactly the survey's pset-heavy carts.
  > Independently confirmed three+ times: the survey's aggregate, plus standalone `⏱ profile` runs
  > on `lotfill`/`orbit`/`dpaint`/`pixelperfect` all bottom out in `… → pset[_rgb] → DrawPixelV →
  > rlVertex3f` (≈60–75% of frame), with `rlSetTexture` churn alongside (the white-texture RLGL
  > state the per-pixel path re-touches). The diagnosis isn't in doubt; only the build-it decision is.
- **HUD text is the top layer, so defer it.** Keep `print` on the GPU but **record its calls during
  `draw()` and replay them AFTER the `cbuf` upload** (a tiny text-command list). Text lands on top
  (where it already was, drawn last) and stays the *same GPU `DrawTextEx`* → byte-identical, and you
  **don't port `print`**.

**v0 surface (behind `DE_SOFTWARE_CANVAS`, opt-in):**
- `uint32 cbuf[SCREEN_W*SCREEN_H]` + a `sw_canvas_active` flag.
- `cls` → clear `cbuf`. `pset`/`pset_rgb` → `cbuf` write (+ `clip` bounds, + `camera()` translation
  as a write offset). `rectfill`/`rect` → `cbuf` row writes.
- **Fills come for free**: `circfill`/`polyfill`/etc. already fall to `plot_pat → pset` for their
  pixels — once `pset` writes `cbuf`, they route automatically. (Just disable the `DrawRectangle`
  span fast-path while `sw_canvas_active` — use the per-pixel `plot_pat` path; still no `rlVertex3f`.)
- `print` (and any HUD `line`, e.g. `pixelperfect`'s 2) → deferred GPU overlay: record the call,
  replay after the upload so it lands on top, same GPU call → byte-identical, no port. `spr`/
  `tritex`/`camera_ex` → **unsupported in v0**: warn and skip (the target carts don't use them).
  (The deferred-overlay trick only holds while those calls are genuinely the top layer — true for
  HUDs; it is NOT a general `line` solution, just a v0 convenience for the target carts.)
- Frame end: `UpdateTexture(canvas.texture, cbuf)` → replay deferred text into `canvas` → the
  existing `DrawTexturePro` scale-up. (One upload, one quad, plus the few text calls.)

**Why it validates cleanly:** `cls`/`pset`/`pset_rgb`/`rectfill` are integer-coordinate writes, so
`cbuf[y*W+x]=c` produces the *same pixels* as the GPU `DrawPixel`/`DrawRectanglePro` they replace;
deferred `print` is the same GPU call. So a CPU-shader cart should be **byte-identical** software-vs-
GPU on the dump+shasum oracle — a real pass/fail, not "looks right." (The fills are also already CPU
coverage, so they match too.)

**The result you'd see:** A/B `caustics`/`shadelab`/`raymarch`/`dpaint` with `profile-fleet.js`,
`DE_SOFTWARE_CANVAS=on` vs off. These are ~pure `pset_rgb`/`pset` carts, so this is where the survey
predicted the biggest drop — v0 confirms (or refutes) the whole thesis on the carts that matter,
for a fraction of the full-rewrite cost, and without touching `camera_ex` or the blits.

**What v0 deliberately leaves for later:** `spr`/`print`/`tritex` as real CPU blits (the option-1
full draw layer), and `camera_ex` (Fork 2). v0 only proves the buffer+upload mechanism and the
headline win; it is *not* the path to making software-canvas the default (that needs the blits).

## Cheaper alternatives — try these *before* committing to the rewrite

If the goal is narrowly "**lower `pset` time**" (not "earn the whole architecture"), there's a much
smaller move that the rewrite framing skips over — and it's worth measuring first, because it
de-risks the big decision either way.

**The mechanism, precisely (raylib 5.5).** `pset → DrawPixel → DrawPixelV` draws each pixel with
the shapes-texture and toggles `rlSetTexture` around it. rlgl *already* batches immediate-mode
vertices into an 8192-element buffer and only flushes on buffer-full / draw-mode change / **texture
change** / `EndDrawing`. So the popular "you pay 60,000 GPU handshakes" story is *not* generically
true for raylib — **except** that `DrawPixel`'s per-pixel texture toggling is itself a per-pixel
texture-state change, which is exactly the `rlSetTexture` churn the survey already measured
alongside `rlVertex3f`. In other words: the per-pixel path *defeats* the batcher that would
otherwise coalesce it.

**That implies a cheap, reversible experiment (no `cbuf`, no draw-layer rewrite):**

1. **Batch the `pset` runs yourself.** Within a frame, emit all per-pixel writes inside *one*
   `rlSetTexture` + `rlBegin(RL_QUADS)`/`rlEnd()` block (or accumulate `{x,y,color}` into a CPU
   array and flush once with the rlgl vertex API), instead of one `DrawPixel` call each. This stops
   re-touching texture state per pixel, so rlgl's existing batch coalesces the lot. The change is
   local to `studio.c`'s `pset`/`pset_rgb` (and the `plot_pat` per-pixel fill path), behind a flag,
   A/B-able with `profile-fleet.js` in an afternoon.

**What it tells you — the real value is the measurement.** The experiment splits `pset` cost into
its two components: *state churn* (which batching removes) vs. *raw per-vertex transform/append*
(which only a software canvas removes, by not emitting vertices at all). If batching alone recovers
most of the frame on `dpaint`/the CPU-shaders, **you may not need the software canvas at all** —
you have a 20-line fix for a problem you "don't really have." If it recovers only part, you've
*quantified* exactly what the rewrite buys, and the v0 below becomes a much better-informed bet.

**What it does *not* give** (so it's a perf patch, not the architecture): the pixels stay
GPU-resident, so `pget`/`pget_rgb` still need the readback + the 256KB `Image` alloc, `fade`/blend
stay shader-side, web `pget` stays disabled, and there's no cross-device determinism win. Those are
all software-canvas-only. Batching lowers the `pset` ceiling; it doesn't move the floor up to the
free wins.

**Order of operations, then:** run the batching experiment first (cheap, reversible, and it
*measures* the thing the whole rewrite decision turns on) → only if the residual per-vertex cost is
big enough to matter does v0 below earn its keep.

> **Measured (2026-06) — the batching experiment was run, and it does *not* lower `pset` time on
> Apple Silicon.** The path is implemented behind `DE_BATCH_PSET=on` (`studio.c` `px_emit` — same 1px
> quad as `DrawPixel`, same winding, but it leaves the white texture bound instead of toggling
> `rlSetTexture(0)` per pixel, so a run of psets coalesces into one draw call; flushes correctly when
> any other primitive changes texture/mode). Two results:
> - **Correctness: byte-identical.** dump+shasum oracle matches off-vs-on on `dpaint`, `interiors`,
>   `pixelperfect`, `lotfill` (the last two *interleave* pset with rectfill/line/circfill, so they
>   exercise the order-flush path). The technique is sound and reusable.
> - **Perf (native, Apple M1): no measurable gain at real-cart densities.** Drift-cancelling
>   interleaved A/B on `dpaint` (pset=30k/frame, ~pure pset runs): off ≈ 3.53ms vs on ≈ 3.58ms, with
>   per-run variance (±0.9ms) an order of magnitude larger than the difference. `interiors` +2%,
>   `pixelperfect` +1%, `lotfill` −2% — all inside noise. (A first single-shot fleet run *looked*
>   like −20–28%, but `raymarch`/`caustics` — which have **pset≈0** — "improved" the same amount,
>   proving that pass was lighter system load, a global offset, not a pset win. Always drift-cancel.)
>   The win only emerges far past any real cart: a synthetic `psetstress` probe at **128k psets/frame**
>   shows a small *repeatable* −6% (9.33 → 8.76ms), so the churn does start to matter at extreme
>   density — but 128k is 4× the heaviest real cart.
> - **Perf (WebGL, M1 / ANGLE-Metal): also no gain.** Tested for real — two emcc builds of `psetstress`
>   (`-DDE_BATCH_PSET_DEFAULT=0/1`) driven through the *system Chrome, headful* (real GPU, renderer =
>   "ANGLE Metal Renderer: Apple M1") via puppeteer-core, frame time read from a `requestAnimationFrame`
>   interval timer (the file profiler is compiled out on web). Reusable recipe (scripts + the gotchas):
>   [`../guides/debug-harness.md`](../guides/debug-harness.md) → "Web perf A/B". At 768k psets/frame (well past the
>   vsync cap, ~20fps): **off median 50ms vs on median 50ms = 0%** (reproduced). Batching gave a
>   slightly *tighter tail* (p90 50.9 vs 66.6ms) but identical throughput.
>
> **What it proves:** on this hardware — native *and* WebGL — the `pset` cost is the **per-vertex CPU
> emission** (4× `rlVertex2f`/pixel, *identical* in both paths), **not** the draw-call/texture-state
> churn batching removes. Crucially, rlgl *already* coalesces those vertices into its 8192-element
> batch buffer regardless of the per-pixel `rlSetTexture` toggle (768k psets in 50ms = ~thousands of
> pixels per real GPU draw, not one draw each), so the "60,000 GPU handshakes" story doesn't hold on
> ANGLE-Metal — and the WebGL-is-different hypothesis was **falsified** for M1. So **there is no cheap
> off-ramp here**: only the software canvas (which replaces those 4 `rlVertex2f`/pixel with a single
> `cbuf` store) can move `pset` time, which *raises* the bar for the rewrite rather than dodging it.
> Door still open only for **weak/old GPU drivers** (Intel iGPU, low-end Android, Pi-class) where
> small-draw submission is genuinely expensive — untested; the `DE_BATCH_PSET` flag + the
> `psetstress` cart (`-DSTRESS_PASSES`, `-DDE_BATCH_PSET_DEFAULT`) stay in as the committed reproducer
> for that day.

## Build runbook — the phased order, if we bite the bullet

The forks are decided (RGBA buffer on desktop, Fork-2/C, defer the blits), so this is the *build*
order, not a redesign. Every phase passes the same validation spine — the **dump+shasum byte-identical
oracle** (`play.js --dump` + `shasum`), the `raster_test` cart, `profile-fleet.js` for the A/B, and
the **web-perf flow** ([`../guides/debug-harness.md`](../guides/debug-harness.md) → "Web perf A/B").
Build it behind `DE_SOFTWARE_CANVAS` (off by default), reusing the `DE_BATCH_PSET` flag pattern.

**Build-flag rule (proven by `tools/det-probes/`, applies from Phase 0):** the cross-device
bit-identity that goal B rests on holds for the float rasterizers **only without `-ffast-math`** —
audit the native (`main.cjs`) and emcc (`build-site.js`) flags to confirm neither enables it, and
that both use the *same* FP flags. FMA contraction is safe (verified). Add
`bash tools/det-probes/run.sh` to the validation spine — it's the regression gate for that property.

**Phase 0 — Mechanism (½ day).** In the frame loop (`studio.c` ~`BeginTextureMode(canvas)`), branch on
`sw_canvas_active`: clear `cbuf`, run `draw()`, then `UpdateTexture(canvas.texture, cbuf)` → the
existing `DrawTexturePro` scale-up (untouched — crisp scaling stays GPU). Wire `cls` only.
*Gate:* a `cls`-only cart is byte-identical on/off.
> **Ready-to-run plan: [`software-canvas-phase0-plan.md`](software-canvas-phase0-plan.md)** — the
> executable version of Phase 0→0c with the exact `studio.c` integration points (`:1354–1365`), the
> V0 primitive table, the `cityplan` world-line catch (route `line`→`sline`→`cbuf`, split the
> byte-identity gate onto a line-free cart + the perf gate onto `cityplan`), and the shared-file
> coordination steps. This is the GO/NO-GO experiment; everything before it is de-risked by the
> `tools/det-probes/` suite.

**Phase 1 — v0: the headline win on the narrow set (~2–3 days) ⟵ GO/NO-GO.** This is the detailed
[Minimal slice (v0)](#minimal-slice-v0--the-smallest-thing-that-shows-a-real-validatable-result)
above: route `pset`/`pset_rgb`/`rectfill`/`rect`/fills to `cbuf` (disable the `DrawRectangle` span
fast-path so fills fall through `plot_pat → pset → cbuf`); **defer HUD `print`/`line` as a GPU overlay
replayed after the upload** (byte-identical, no port); `spr`/`tritex`/`camera_ex` warn-and-skip.
Targets: `caustics`/`shadelab`/`raymarch`/`dpaint`/`pixelperfect`/`interiors`/`lotfill`.
*Gates:* byte-identical on all targets **+** fleet A/B drops the pset carts hard **+** web A/B.
*Kill criterion:* if v0 does **not** drop the pset carts, stop — the thesis is wrong, and we've spent
days not the rewrite. (Unlike the batching probe, v0 eliminates the per-vertex `rlVertex2f` emission
that *is* the cost, so this is the honest test of the thesis.)

**Phase 2 — v1: port the blits (~1 week, the bulk).** `spr`/`print`(all `FONT_*`)/`tritex` → CPU
blits, retiring the deferred-overlay hack. **New dependency:** keep the spritesheet + font atlases as
CPU `Image`s in RAM (don't `UnloadImage` after upload) and make the sprite editor's live edits update
the CPU copy. `tritex` is the one real new rasterizer (affine scanline, textbook).
*Gate:* `build-all` compile sweep, then byte-identical on a broad set incl. 3D/`tritex` carts; fleet
A/B neutral-or-better everywhere. High blast radius — the oracle is the net.

**Phase 3 — `camera_ex` fallback wiring.** You only learn a cart uses `camera_ex` mid-`draw()`.
Resolution: on first non-identity `camera_ex`, set a **sticky per-cart flag** → that cart runs today's
GPU path for the rest of the session (one transitional frame). *Optional, only if a rotating-**fill**-
bound cart actually surfaces:* the inverse-mapping fill path (Fork 2 note) instead of GPU fallback —
defer until measured demand.
*Gate:* a zoom+rotation cart renders identically to today; a translation-camera cart stays on the
fast software path.

**Phase 4 — default + free wins.** Flip software-canvas to default for non-`camera_ex` frames
(`DE_SOFTWARE_CANVAS=off` escape hatch for one cycle); fold the span-fill fast paths into the `cbuf`
writers; cash the free wins (`pget`/`pget_rgb` → array read, **enables `pget` on web**; `fade`/blend →
read-modify-write; determinism → wire into the `--det` harness for replays/ghosts); write the
`decisions/` ADR.

**Shape of the bet:** Phase 0–1 is cheap (~3–4 days) and decisive on the exact carts. Phases 2–4 are
the real cost (~1.5–2 weeks) and start **only** on a trigger beyond "Phase 1 worked" — a cart actually
pset-bound below 60fps, or a committed web/low-end target. Feasibility ≠ need.

### Low-end / small-memory targets (Pi-class) — the options to keep, not lose

The whole calculus changes on weak hardware, and these are the levers that make a software canvas not
just viable but *mandatory* there (GPU immediate-mode is 2–5fps on a Pi Zero). None are needed on M1;
all are real if low-end ever becomes a target — captured here so they're not lost:

- **`uint8` paletted buffer = "smaller memory passing."** The Fork-1 reopener: 64KB fits a small L2
  (256KB RGBA spills), and the per-frame upload is **4× smaller** (R8 vs RGBA) — 15–25% of a Pi Zero
  frame, not <1%. This is the low-end build profile; `pset_rgb` either goes through a parallel
  RGBA plane (only when used) or isn't offered on that profile. Free bonus the index buffer brings
  back: full-screen **palette cycling** (animate the LUT — water/fire) that RGBA forfeits.
- **Fixed-point, never float, in the inner loops.** The ARMv6 FPU is slow; the camera/inverse-mapping
  math (and any per-pixel step) should be integer fixed-point on that profile.
- **Overdraw is the bandwidth ceiling.** memset/blend bandwidth, not math, is the Pi limit — skip
  `cls` when the background is opaque, minimise full-screen blend layers.
- **SIMD as a later lever** — NEON (Pi 2+, and M1) can do `cls`/blits 4–8× faster; the Pi Zero (ARMv6)
  has none, so keep inner loops tight and branch-free there. Something GPU immediate-mode could never
  exploit.
- **Triple-buffer the `cbuf`** to dodge a CPU/GPU contention stall on the upload (matters once the
  bus is the bottleneck).

All untested — the committed web-perf flow plus a Pi test rig would validate. The `uint8`-vs-RGBA
choice is the one that must be made at Phase 0 if a low-end profile is in scope (it's a buffer-format
fork, awkward to retrofit), so decide that *before* Phase 0 if Pi is a real target.

## Risks / why it might *not* be worth it

- **It's a rewrite of the draw layer**, and the partial-slice catch above means the first honest
  prototype is option-1-sized (port the blits) unless you take the narrow option-2 opt-in. High
  blast radius; the byte-identical oracle + `raster_test` + fleet A/B are the net, but it's a lot of
  carts to re-validate.
- **`camera_ex` carts bifurcate** — they stay on the GPU path, so two renderers coexist until/unless
  rotation-in-software is solved. Acceptable because that set is small and disjoint from the
  pset-heavy set, but it's real maintenance surface.
- **Most carts are already at 60fps** — this buys *headroom* (bigger canvases, the CPU-shader work,
  web), it doesn't fix a present fire. The decision isn't "is it correct" (the design is sound) —
  it's "is removing the `rlVertex3f` ceiling worth a draw-layer rewrite *now*."

## Recommendation

**Park the decision, not the design** — the data keeps pointing here (every profile in this arc ends
at `rlVertex3f`; the span fills only routed *around* it for fills), so a software canvas is the move
that removes it engine-wide, and at 64k px the precedent (every fantasy console) says it should win.

But keep the perspective honest: **this buys headroom, not relief from a present fire** — most
carts already hit 60fps, and the one rotating-*and*-pset-bound cart the rewrite's hardest part would
serve may not even exist in the fleet (`sloop`, the one off-shape profile, is draw-call-bound, not
`pset`-bound). The cheap off-ramp — draw-call batching (`DE_BATCH_PSET`) — **was tried and measured
null on Apple Silicon** ("Cheaper alternatives" → Measured 2026-06): `pset` cost is per-vertex CPU
emission, which batching doesn't touch. So on *this* hardware there's no shortcut: the software
canvas is the only thing that moves `pset` time, and it's a draw-layer rewrite for headroom. That's
a *strong reason to wait* until either (a) a cart is actually `pset`-bound below 60fps, or (b) web/
low-end becomes a target (where the batching probe should be re-run first — it may pay there even
though it didn't here).

When the canvas *is* picked up: prototype Fork-2/C behind `DE_SOFTWARE_CANVAS`, choose option 2
(narrow per-pixel opt-in) to prove the mechanism + the `dpaint`/CPU-shader win cheaply, A/B on the
fleet, and only commit to option 1 (porting `spr`/`print`/`tritex`) if those numbers land the way
the survey predicts. Earn the rewrite with data at each step — same discipline as the rest of the
ledger.

## See also
- [../guides/engine-optimization.md](../guides/engine-optimization.md) — the fleet survey that
  pointed here + the ledger of the fill optimizations this would subsume.
- [cpu-shaders.md](cpu-shaders.md) #5 — the parked "true offscreen buffer"; a software canvas is a
  superset (the whole canvas is in RAM, multipass becomes trivial).
- [rasterization-consistency.md](rasterization-consistency.md) — the CPU per-pixel coverage
  invariant the software rasterizers already satisfy; the rotated-camera seam fork 2 must respect.
- [field-based-road-rendering.md](field-based-road-rendering.md) — a concrete per-pixel-coverage
  consumer (draw a road from one distance field; asphalt/kerb/sidewalk/lanes = thresholds). Proven
  in the `skewlab` cart; the queued streetlab refactor. It's `pset`-per-pixel today, so it's cheap
  *on* the canvas and a natural thing to land with it — and it's where the symmetric CPU line
  (`sline`, see the note above) gets used for real.
