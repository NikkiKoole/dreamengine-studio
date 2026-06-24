# Software-canvas Phase-0 mechanism probe — the executable plan

> **Genre: RAN — result GO (2026-06-24).** Phases 0a+0b are built behind `DE_SOFTWARE_CANVAS`
> (env, off by default) in `studio.c`. **Byte-identity gate (0a):** the `swcanvas_test` cart
> (`cls`/`pset`/`pset_rgb`) is **byte-identical** GPU vs `DE_SOFTWARE_CANVAS=on` (same shasum).
> **Perf gate (0b):** `cityplan`, same harness, **GPU `workMsAvg` 11.4ms → software 5.0ms = 2.3×
> faster.** The decisive lesson: **fills must stay span-based** — routing `rectfill` per-pixel
> through `plot_pat` was *2.4× slower* than GPU; a `sw_fillrect` row-memset is what wins. Commits:
> Phase 0a `133b9d0e`, Phase 0b `ec1b855c`.
>
> **Phase 2 done (2026-06-24): the renderer is feature-complete for common carts + the rotation
> split is wired.** `2a` spr/sspr CPU blit (`e19a479b`), `2b` print all 5 fonts via glyph blit
> (`bda3d59d`), `2c` tritex via the `stritex` rasterizer (`dd4fd9ab`), `2d` circ/oval/poly fills kept
> span-based (`90611408`), `2e` **camera_ex zoom in software** + **rotation falls back to GPU**
> (`30e4ba0b`), and the fix that **rotated *primitives* (`rectfill_rot`/`spr_rot`/`sspr_ex`) also
> trigger the GPU fallback** (`74dcb8cf`). Mechanism: the flag split into `sw_canvas_enabled` (env)
> and a per-frame `sw_canvas_active` (= enabled AND `!sw_force_gpu`); any rotation — camera *or*
> primitive — sets the sticky `sw_force_gpu` so that cart rides the GPU path (Fork-2/C).
>
> **Fleet split confirmed across the whole shape spectrum:**
> - **pset / fill / sprite-bound** (cityplan 2.1×, interiors 2.9×, dpaint 2.8×, pixelperfect 2.3×,
>   lotfill 1.7×, gridcity 1.6×) → **win on the canvas**. cityplan renders with correct zoom (z6.34).
> - **geometry / 3D / rotation-bound** (`sloop`, `cityview`, `textured3d`, `caustics`, `raymarch`) →
>   stay on GPU (fallback or just faster there). `sloop`/`cityview` are *heading-up rotating* +
>   `rectfill_rot`/`tritex` + ~0 `pset` — fall back, render correctly, no win and no penalty.
>
> **Why Option 3 ("rotate in HW, rasterize in SW") has no cart to justify it yet** — and the cost that
> decides it: rotating a *pre-rendered image* needs the screen's rotated **corners** filled, so you
> must render an **oversized buffer** (the screen diagonal ≈ 377² ≈ **~2.2× the pixels**) every frame,
> then crop. The GPU avoids this entirely by transforming *geometry* per-vertex (only rasterizes
> visible pixels). So Option 3 only pays off for a cart that is **rotation-heavy AND pset/fill-bound**
> (big SW win to absorb the over-render). Every rotating cart in the fleet (sloop/cityview) is instead
> *geometry*-bound (rectfill_rot/tritex, ~0 pset) → GPU is strictly better. **Trigger to build Option
> 3 (+ HUD-layer compositing): a rotating top-down pixel-art/Mode-7 world with heavy fills + a HUD.**
>
> **Remaining tail (small, before flipping the default):** rotated `spr_rot`/`sspr_ex`/`print_rot`
> as real CPU blits (today they GPU-fall-back), and `pal()` recolor + runtime `colorkey()` for the
> sprite blit (PNG alpha works). Then Phase 3/4 (flip default for non-rotation frames + free wins:
> `pget`→array read, cross-device determinism).
>
> Caveat: `cityplan`/`gridcity`/etc. are not byte-identical to GPU under the flag (world-space
> `line`→`sline` + per-pixel fills differ by design — `sline` is the *better*, deterministic one); the
> byte-identity guarantee is for the integer-primitive set, proven on `swcanvas_test`.
>
> **Original plan (below), now executed:** the concrete step for
> [`software-canvas.md`](software-canvas.md) — its Build-runbook Phase 0→1 made executable. The
> primitive-level questions were all answered (the `tools/det-probes/` suite + the dated notes in
> the design doc); this was the one remaining experiment: wire the mechanism into `studio.c` and
> A/B on a real cart. It was the GO/NO-GO for the whole software canvas.

## The one question this answers

Everything else is de-risked. Each rasterizer is proven to work in software, gap-free, and
bit-identical across arm64/x86/wasm (`tools/det-probes/run.sh`). The *only* unproven thing is the
integration: **does a CPU `cbuf` + one `UpdateTexture`/frame actually (a) reproduce the GPU output
and (b) deliver the perf win on a `pset`-bound cart?** `cityplan` is the trigger cart (19.9ms/frame,
~47fps, ~73% of wall in the `pset → rlVertex3f` path — see software-canvas.md).

**Kill criterion:** if `cbuf`+upload does **not** materially drop `cityplan`'s frame time, the thesis
is wrong — stop here. We'll have spent ~½–1 day, not the multi-week rewrite. (Unlike the
`DE_BATCH_PSET` probe, this *removes* the per-vertex `rlVertex2f` emission that IS the cost, so it's
the honest test.)

## What the probes already settled (don't re-litigate)

- **Buffer format:** `uint32_t` RGBA on desktop (Fork 1). 256KB, `pset_rgb` native.
- **Determinism:** float rasterizers are bit-identical across arches **without `-ffast-math`** — so
  keep FP flags identical in `main.cjs` (native) and `build-site.js` (emcc); never `-ffast-math`.
- **Lines:** `sline` (the validated CPU DDA) is the `cbuf` line — *not* a deferred GPU overlay.
  This matters for `cityplan` (see the catch below).
- **Rotation / `spr` / `tritex`:** all proven doable in software (inverse-map, `stritex`, RotSprite)
  but **out of scope for Phase 0** — warn-and-skip; they're Phase 2+.

## Scope — V0 surface (behind `DE_SOFTWARE_CANVAS`, off by default)

Mirror the `DE_BATCH_PSET` flag pattern. When `sw_canvas_active`:

| primitive | Phase-0 behaviour |
|---|---|
| `cls` | clear `cbuf` (memset the RGBA clear colour) |
| `pset` / `pset_rgb` | `cbuf[y*W+x] = rgba` (+ `clip` bounds, + `camera()` translation as a write offset) |
| `rectfill` / `rect` | `cbuf` row writes (memset spans) |
| `circ`/`circfill`/`oval`/`poly`/fills | **free** — they already fall to `plot_pat → pset`; just disable the `DrawRectangle` span fast-path while active so they route through `pset → cbuf` |
| `line` | route to **`sline` → `cbuf`** (world-space lines must be in the buffer for correct z-order — see catch) |
| `print` | **deferred GPU overlay**: record calls during `draw()`, replay after the `UpdateTexture` so HUD text lands on top (same `DrawTextEx` → unchanged pixels) |
| `spr` / `sspr` / `spr_rot` / `tritex` / `camera_ex` | **warn-and-skip** (not in v0; `cityplan` uses none) |

Frame end: `UpdateTexture(canvas.texture, cbuf)` → replay deferred `print` into `canvas` → the
existing `DrawTexturePro` scale-up (untouched — crisp scaling stays GPU).

## The `cityplan` catch — why the byte-identity gate splits

`cityplan` issues **~485 `line`/frame**, and they're **world-space** (block outlines interleaved with
fills), not a top HUD layer. So the deferred-overlay trick from the original doc **does not work for
them** (z-order breaks). They must go into `cbuf` via `sline` — which means `cityplan`'s output is
**not byte-identical** to the GPU build (GPU `DrawLine` ≠ `sline` pixels; `sline` is the *better*,
deterministic one, but different). Therefore split the validation:

- **Byte-identity gate → a line-free cart.** Prove the mechanism reproduces the GPU exactly on a cart
  that is only `cls`+`pset`/`pset_rgb`+`rectfill`+fills+HUD-`print` (no world `line`/`spr`). Candidates:
  `dpaint`, `pixelperfect` (check it has no world lines), or a tiny purpose-built `swcanvas_test`.
  Gate: `play.js --dump` off-vs-on, `shasum` every frame → identical.
- **Perf gate → `cityplan`.** Accept that world lines route through `sline` (output changes, by design
  better). Gate: the ~20ms → ~6–8ms drop. Confirm visually (bake) that it still looks right.

## Steps

1. **Phase 0a — flag + buffer + `cls` (½ day).**
   - Add `DE_SOFTWARE_CANVAS` (compile flag) + `sw_canvas_active` (runtime) + `static uint32_t cbuf[...]`.
   - Branch the draw block at `runtime/studio.c:1354–1365` (`BeginTextureMode(canvas)` … `EndTextureMode()`):
     when active, *skip* `BeginTextureMode`/`BeginMode2D`, run `cart_draw()/draw()`, then
     `UpdateTexture(canvas.texture, cbuf)` so the present at `:1397+` is unchanged. Wire `cls` only.
   - *Gate:* a `cls`-only cart is byte-identical on/off.
2. **Phase 0b — route the V0 primitives.** `pset`/`pset_rgb`/`rectfill`/`rect` → `cbuf`; disable the
   span fast-path so fills fall through; `line` → `sline`→`cbuf`; defer `print`; warn-skip the rest.
3. **Phase 0c — validate.** Byte-identity on the line-free cart; perf A/B on `cityplan`
   (`tools/play.js cityplan ... ⏱`-profile or the live profiler); web A/B (debug-harness "Web perf
   A/B"); run `tools/det-probes/run.sh` (unchanged — the rasterizer regression gate).
4. **Decision.** Win on `cityplan` + byte-identical on the line-free cart → proceed to Phase 1/2
   (the full v0 target set, then the blits). No win → stop; write the negative result into the doc.

## Coordination & safety (this touches the hot shared file)

- **`runtime/studio.c` is edited by several agents** (the road agent is active near rendering). Do
  this when the tree is quiet, **targeted `Edit`s only — never full-file `Write`**, re-`Read` each
  region immediately before editing, and after committing confirm the change survived
  (`git show HEAD:runtime/studio.c | grep <sentinel>`).
- **Compile-gate before commit:** `node tools/play.js soundcheck script /dev/null --headless --frames 2`
  (no warnings) + `node tools/build-all.js` (every cart still compiles vs the changed `studio.c`).
- Commit by explicit pathspec; keep the flag **off by default** so no other cart changes behaviour.

## Effort & shape of the bet

Phase 0a–0c is ~½–1 day and **decisive on the exact cart**. Phases 1–2 (the full target set + porting
`spr`/`print`/`tritex` as real blits) are the ~1.5–2 week cost and start **only** if Phase 0c wins.
Feasibility is no longer in question — only whether the integrated buffer-and-upload delivers the perf.
