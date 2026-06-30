# Engine portability — the platform seam + the renderer decision

STATUS: SHIPPED — the renderer decision is **SETTLED** (2026-06-29; desktop + device FPS both measured → see
"Measured" below, now [ADR-0024](../decisions/0024-software-canvas-is-canonical-for-2d.md)). This doc
captures the refactors that make the engine cleanly portable (iOS first, but they help web too) and
the load-bearing decision they hung on: **software canvas vs GPU as the canonical renderer** — measured
on both desktop and a physical iPhone, and decided: **software canvas canonical for 2D, `tritex`/3D
GPU-only.**

Companion reading: [`ios-plan.md`](ios-plan.md) (the iOS spike ladder + Phase 2), the existing
software-canvas probe notes in [`software-canvas.md`](software-canvas.md), and the engine source
(`runtime/studio.c`, `runtime/sound.h`).

## Why this exists

Phase 2 of the iOS work runs the **real** `studio.c` + `sound.h` + a cart (target: `omnichord`) on
iOS, not the stand-in `canvas.c`/`audio.c` the spikes used. The maker confirmed (2026-06-29) **we may
change the engine itself** rather than work around it — so the goal is a *clean platform seam*, not a
hack. These changes are good architecture regardless of iOS, and they untangle the existing
desktop/web `#ifdef` sprawl.

## The load-bearing decision: software canvas vs GPU

This is the first domino — the iOS render architecture and the shape of the whole platform seam branch
on it. **Undecided; to be settled by measurement, not opinion.**

Why it's load-bearing:
1. **Does iOS need a GL→Metal layer at all?** Software-canvas → the engine hands iOS a finished CPU
   framebuffer (`sw_cbuf[SCREEN_W*SCREEN_H]`) and iOS blits one texture (proven in spike 1): no ANGLE,
   no `raylib-iOS` fork. GPU path → iOS must run Raylib's GL through **ANGLE** (large, "experimental"
   dependency). One choice = "blit a buffer" vs "host a GL stack."
2. **It sets the platform seam's `present`/`draw` interface** — hand over pixels, or host a GL context?
   Can't design the seam before deciding.
3. **Feature parity.** GPU-only features today: `pal()` (palette-swap shader), the scale/present
   filters (`scale_shader`, `SCALE_FILTER`), `smooth_zoom` (offscreen render-texture), `tritex`
   (RL_QUADS). Software-canvas-canonical means reimplementing these on CPU or dropping them, or those
   carts break on a CPU-only iOS.

Not strictly either/or forever — both could live behind the seam — but then iOS still needs ANGLE for
the GPU path, defeating the clean story. So "commit to the software canvas" and "stay ANGLE-free on
iOS" are the **same** decision.

### How to decide it (the measurement plan)

- A 320×200 console is cheap to CPU-rasterize, but heavy carts (many fills, big sprites, 3D `tritex`)
  may not hold 60fps on CPU — **especially on the phone, not just the Mac**.
- Measure both modes (`DE_SOFTWARE_CANVAS=on` vs the GPU default) on representative carts: a light one
  (`omnichord`), a fill-heavy one, a sprite-heavy one, and a 3D/`tritex` one. Compare frame-time/fps.
- Tools already in the repo: `tools/profile-fleet.js` (batch CPU profile), `tools/canvas-diff.js`
  (GPU-vs-software correctness oracle — confirms the CPU output matches before we trust it for perf).
  Add a **device** measurement (run on the iPhone via `ios/device.sh`-style deploy and read frame-time)
  — desktop fps is necessary but not sufficient.
- Decide, then record as an ADR (this doc becomes the rationale feed).

### Measured — desktop half (2026-06-29)

Ran the real engine (`studio.c`) headless on representative carts, `DE_SOFTWARE_CANVAS=on` (runtime
toggle, studio.c:1832) vs the GPU default, via `profile-fleet.js`. `workMsAvg` = engine CPU time per
frame; the 60fps budget is **16.6ms**. Mac native (this is the desktop half — NOT the simulator, which
runs the stand-in `canvas.c` and reflects Mac perf anyway, so it's no proxy for a phone).

| cart | type | GPU | **SW canvas** |
|------|------|-----|---------------|
| `omnichord` | light (the iOS target) | 0.26ms | **0.58ms** |
| `neonrain` | fill-heavy (83 fills/frame) | 0.12ms | **0.38ms** |
| `flank` | sprite-heavy | 0.52ms | **0.36ms** |
| `podracer` | **3D / `tritex`** | 0.22ms | **19.32ms** |

Findings:
- **2D is a non-issue on CPU.** The three 2D carts run sub-0.6ms in software mode — ~30× under the
  60fps budget, with the *real* engine. `flank` is even cheaper in SW than GPU (tight CPU span-fills
  beat GL per-call overhead for many small `rectfill`s).
- **`tritex` (perspective-correct textured triangles) is the lone killer** — 19.3ms on a *fast Mac*
  CPU, already over budget here, and a phone CPU is several × slower. This is the GPU-only-parity
  problem in concrete form (the §"Open questions" `tritex` item).
- **Correctness gate passed:** `canvas-diff omnichord` = **0px** diff vs GPU, every frame — the SW
  numbers are trustworthy and the output is pixel-identical, not just fast.

**Provisional read** (gated on the device half before it becomes an ADR): commit to the software
canvas as canonical for 2D → ANGLE-free iOS; treat `tritex`/3D as an explicit exception — either
optimize the SW triangle rasterizer, or keep 3D carts GPU-only and off the initial iOS target list.
The 2D headroom is so large it will survive a phone; the device measurement can only sharpen the
`tritex` verdict, not overturn the 2D one.

### Measured — device half (2026-06-29) ✅ — the prediction held

Ran the **real iOS app** (the DE_NO_RAYLIB software canvas, `CanvasView` blitting `de_framebuffer()`)
on a physical **iPhone SE 2nd-gen (A13, iOS 15.4.1)** via `ios/measure-device.sh` — which stages each
cart as the app cart, builds signed, launches, and pulls `Documents/perf.log`. `engine` = `de_frame()`
CPU per frame; `blit` = the flip + CGImage. **NB: a Debug `-O0` build** (xcodebuild Debug), so the
engine numbers are *pessimistic* vs the `-O2` desktop column — a Release build runs several× faster.
Even so:

| cart | type | engine avg | blit avg | **fps** |
|------|------|-----------|----------|---------|
| `omnichord` | light (the iOS target) | ~5.3ms | ~0.6ms | **59–60** |
| `neonrain` | fill-heavy | ~4.9ms | ~0.65ms | **59–60** |
| `flank` | sprite-heavy | ~4.9ms | ~0.6ms | **59–60** |
| `podracer` | **3D / `tritex`** | **~89ms** | ~0.2ms | **~10** |

Findings:
- **2D holds a locked 59–60fps on the phone** — engine+blit ≈ 5.6ms, ~⅓ of the 16.67ms budget, in an
  *unoptimized* build. The desktop "2D is a non-issue" result survived contact with real phone silicon
  with room to spare; Release only widens the margin.
- **`tritex` is the lone killer, confirmed on-device** — ~89ms/frame → ~10fps. The desktop SW 19.3ms ×
  ~4.6 (phone CPU vs the fast Mac) lands right here. Optimization won't rescue it into budget.
- The blit (flip + CGImage upload) is cheap everywhere (~0.6ms) — the CPU framebuffer path is not the
  bottleneck; the rasterization is.

**Sprite throughput — `bunnymark` on the same iPhone SE 2nd-gen (2026-06-30):** held-at-60fps bunny
count, each bunny one `sspr()` of a 32×32 sprite (a pure CPU-blit stress, no `pal()`):
- **~8k** — Debug `-O0` build
- **~20k** — Release build (the real number; ~2.5× — the `-O0` blit-loop penalty reclaimed)
- **~24k — desktop, software canvas** (the fair, like-for-like baseline): so the phone's 20k is within
  **~17%** of the desktop CPU. Phone-vs-Mac is essentially a wash on this workload.
- ~200k — desktop, but that's the **GPU** (hardware sprite batcher), not like-for-like: the phone has no
  GPU path and rasterizes every sprite on the CPU by design. So the ~10× from 20k→200k is **entirely
  GPU-vs-CPU, not phone-vs-Mac** (confirmed by the 20k-vs-24k software parity above). **Takeaway:** 20k
  CPU sprites at 60fps is
  enormous headroom for the iOS target (touch instruments + modest 2D); a real cart pushing hundreds of
  sprites is nowhere near it. (And: build **Release** for any real device perf number — `CONFIG=Release
  ios/device.sh`; the default Debug build understates by ~2.5× here.)

**Settled.** Both halves measured (desktop + device), both agree. The decision is now an ADR:
[ADR-0024](../decisions/0024-software-canvas-is-canonical-for-2d.md) — **software canvas is canonical
for 2D (ANGLE-free iOS); `tritex`/3D is GPU-only and off the initial iOS target list.**

## ⚠ Known issue — software renderer renders only the first frame (2026-07-01)

Observed running **`sloop`** under the `DE_NO_RAYLIB` software engine (`tools/build-nr.sh`): it paints a
**single stuck first frame** then doesn't update/animate — the GPU/Raylib build is fine. Not yet
diagnosed; unknown whether it's sloop-specific or engine-wide (the software path's main loop / present /
input pump). **To look at:** does it repro on a simpler cart under `build-nr.sh`? If yes → engine-wide
(the `DE_NO_RAYLIB` frame-loop), if no → something sloop does that the software path doesn't drive. Start
by A/B-ing one trivially-animated cart vs sloop under the software engine.

## Built — the platform seam, phase A → D.2 (2026-06-29)

The seam is real and the engine now compiles + renders WITHOUT Raylib. All steps kept desktop
bit-identical (build-all 439/439, canvas-diff omnichord 0px) — every new path is `#ifdef DE_NO_RAYLIB`,
dormant on desktop/web.

- **A — universal `DeColor`** (`runtime/color.h`): the engine owns its color type; `typedef Color
  DeColor` aliases whatever the backend's `Color` is (Raylib's on desktop, the shim's under DE_NO_RAYLIB).
- **`runtime/platform.h`** — the host↔engine contract: `de_init`/`de_frame`/`de_framebuffer`/
  `de_screen_w/h` + `de_audio_render` + touch feed; `DE_RENDERER_SOFTWARE`/`_GPU` (two renderers, one
  seam — software for portable/simple carts, GPU for heavy work).
- **Decode centralized** → `de_image_decode` (one place; Raylib decoder today, stb_image branch is iOS's).
- **Fonts baked** (`tools/bake-fonts.c` → `runtime/fonts_baked.h`): Raylib's own `LoadFontFromImage`
  output (keyed atlas + glyph table) frozen at build time, so the no-Raylib `sw_print` needs no GPU
  readback and no atlas scan.
- **D.1 — `runtime/raylib_compat.h`**: the DE_NO_RAYLIB shim (Raylib types/enums + ~94 prototypes,
  auto-extracted from raylib.h/rlgl.h). `studio.c` compiles with 0 errors sans Raylib.
- **D.2 — it links + renders**: `raylib_compat.c` (no-op GPU/device stubs + real GetTime/GetGlyphIndex/
  GetImageColor/GetCodepointNext), `studio.c` `de_init`/`de_frame` (bind baked fonts → globals, run
  loop_step), and `tools/headless-nr.c` (drive headless, dump `sw_cbuf`). **omnichord renders with NO
  Raylib / NO frameworks**, matching the Raylib build. `sw_cbuf` is bottom-up — the blit must flip
  (the iOS CanvasView too).

**Open follow-ups before iOS:** ~~(1) text-metric diff~~ FIXED — `MeasureTextEx` was a no-op stub
returning {0}, so `text_width()`/centering/clip measured zero (dropped trailing label chars); real impl
mirrors sw_print's advance → headless omnichord now matches Raylib exactly. ~~(2) stb_image branch~~ DONE — `runtime/stb_image.h` (raylib's own copy → byte-identical decode);
`de_image_decode` decodes the sheet into `spritesheet_img`, `de_init` sets `spritesheet.width/height`
(the sprite/map guards + `cols` derive from it; GPU id stays 0), `colorkey()`'s GPU sheet-rebuild is
`#ifndef DE_NO_RAYLIB` (SW keys per-pixel), and `GetRandomValue` got a real LCG. **heroes** (tilemap +
11 sprites) renders **pixel-identical** to Raylib headless. (masseffect's unit sprites render too; its
map grid diverges only because its camera/wave state rides Raylib's exact rng sequence — not a sprite
bug.) ~~(3) audio~~ DONE — `de_audio_render` pumps `sound_callback` (the real mixer); `de_frame`'s
`loop_step` advances the sequencer via `sound_tick`. tb303 rendered headless with NO Raylib is
**byte-identical** to the Raylib `--wav` (correlation 1.00000) once the first-frame dt matches det_mode.

**All three follow-ups closed — the portable engine is complete on desktop:** real `studio.c` + `sound.h`
render graphics (omnichord 2D, heroes tilemap+sprites pixel-identical) AND audio (tb303 byte-identical),
headless, with zero Raylib / zero frameworks. `tools/headless-nr.c` is the proof harness (frame→PPM,
audio→WAV); `tools/build-nr.sh` is the build/run recipe.

**✅ The iOS shell is now built too (Phase 2, 2026-06-29)** — omnichord renders pixel-correct + upright
on the iPhone 15 simulator, CoreAudio pulls the real mixer, UIKit touch drives it. One engine-side gap
surfaced and was filled: the `de_touch_*` seam (platform.h) had no bodies and the no-Raylib input was
all-zero stubs — `raylib_compat.c` now backs `de_touch_*` with a touch-point pool that
`GetTouchPointCount/GetTouchPosition` read. It also synthesizes **mouse from the primary finger**
(`GetMousePosition`/`IsMouseButton*`) so mouse-driven carts play from touch, plus a `de_key_event`
key seam — both with raylib's prev/current edge model (`de_input_endframe()` at frame end). The full
shell wiring (project.yml `SCALE=1`, the `CanvasView` flip, the stereo `de_audio_render` split) lives
in [`ios-plan.md`](ios-plan.md) → "Phase 2".

## The three refactors that unlock iOS (and help web)

1. **A platform seam (`platform.h`).** `studio.c`'s `main()` calls Raylib directly — `InitWindow`,
   `InitAudioDevice`, `GetCharPressed`/mouse, `LoadFontFromImage`/`LoadTexture`, the
   `WindowShouldClose` loop, `GetTime`. Extract that surface — *window, input, audio-device,
   asset-load, timing, present* — behind a small interface with backends: `raylib-desktop`, `web`,
   `ios`. The engine core (update/draw, primitives, mixing) becomes host-agnostic. Highest-leverage
   change; also cleans up the `PLATFORM_WEB` `#ifdef` sprawl.
2. **Commit the software canvas** (gated on the decision above). It's a "Phase 0 probe"
   (`SW_CANVAS_DEFAULT 0`) behind A/B flags with "direction-1 (coverage-vs-DDA line) still open." If
   chosen as canonical: iOS needs no ANGLE, rendering becomes deterministic/portable, and a pile of
   GPU-vs-canvas A/B scaffolding can be deleted.
3. **Pluggable audio output.** `sound.h`'s `sound_callback` is already the core, and `sound_synth_mode`
   (the `--wav` path) proves it runs device-less. Make the *output backend* swappable over that one
   callback: Raylib `AudioStream` (desktop) / CoreAudio (iOS) / Web Audio worklet (web). Same mixer
   everywhere — and exactly what the AUv3 render block wants (spike 7).

Together these turn "real engine on iOS" from surgery into "implement one iOS backend behind the seam."

## General engine health (independent of iOS)

4. **Split the monoliths.** `studio.c` (~4,300 lines) and `sound.h` (~6,850) are huge — and CLAUDE.md
   spends paragraphs on how parallel agents clobber them in the shared tree. Splitting into modules
   (gfx / input / audio-host / assets) *creates* the platform seam above **and** shrinks the
   parallel-edit blast radius that keeps biting.
5. **Settle and prune the A/B flags.** `DE_POLY_FILL`, `DE_DISC_FILL`, `DE_CLAMP_CACHE`,
   `DE_BATCH_PSET`, `DE_BLIT_FAST` — exploration toggles whose fast paths look blessed. Once decided,
   deleting the legacy branches removes real code and footguns.
6. **Asset-loading abstraction.** `LoadFontFromImage`/`LoadTexture` assume Raylib + files at `cwd`. A
   small loader (PNG → CPU buffer for the software canvas; bundle-relative paths) is needed for iOS and
   tidies the desktop/web split.

## Sequencing (if/when we do this)

**2 → 1 → 3**, with **4** happening *as part of* 1:
- Commit the software canvas first (#2) — it removes the hardest dependency (GL).
- The platform seam (#1) then falls out naturally; splitting the monoliths (#4) is how you build it.
- Pluggable audio (#3) last — `sound_callback` already isolates the mixer, so this is the smallest.
- Everything is **gated on the renderer decision** — measure first.

## Assets already in hand

- **Software canvas** — `sw_cbuf`, a CPU RGBA framebuffer (`studio.c`, behind `DE_SOFTWARE_CANVAS`).
- **Device-less audio** — `sound_callback` + `sound_synth_mode` (`sound.h`).
- **The iOS shell** — spikes 0–7 (`ios/`) already built the Path-B host: framebuffer blit, CoreAudio
  callback, touch-capable view, save, StoreKit, App Group, AUv3. Phase 2 = plug the real engine into it.
- **Measurement tools** — `tools/canvas-diff.js` (correctness), `tools/profile-fleet.js` (perf).

## GPU-only feature parity — audited (2026-06-29)

The renderer ADR worried that `pal()`/scale/`smooth_zoom`/`tritex` were all GPU-only and would break
the portable target. Audited against the actual software-canvas code + `canvas-diff` + the no-Raylib
build (`build-nr.sh`); the real picture is narrower than feared:

| feature | status on the software canvas | carts |
|---------|------------------------------|-------|
| **`pal()`** (palette-swap) | ✅ **full parity** — `sw_recolor()` is the CPU twin of `PAL_FS`; `canvas-diff` = **0px** on palettelab/smooch/neonrain/heroes | 37 |
| **scale present filter** | ✅ **non-issue** — no cart uses it; on iOS the *host* (`CanvasView`) does nearest-neighbour scaling | 0 |
| fractional **zoom** (`camera_ex`) | ✅ **works** (orbit renders); only *cosmetically* ≠ GPU (sub-pixel rounding), which doesn't matter with no GPU reference | many |
| **`smooth_zoom`** (offscreen RT) | ⚠️ GPU-only; degrades to **plain zoom** (the fractional-zoom AA just doesn't apply) | `sloop` |
| **camera ROTATION** (`camera_ex` angle≠0) | ✅ **works** — software rotation rasterizer (offscreen world layer → rotate-composite); a 25° probe is **27/64000 px (0.04%)** off the GPU | `hotline`, `sloop`, `coaster`, `worldpointer` |
| **`tritex`/3D** | ❌ GPU-only by perf (~89ms on the phone CPU) — off the initial iOS list ([ADR-0024](../decisions/0024-software-canvas-is-canonical-for-2d.md)) | `podracer`, … |

**Camera rotation — was a freeze, now a rasterizer.** `camera_ex(angle≠0)` first set the sticky
`sw_force_gpu` to fall back to the GPU path — but with no GPU the stubs no-op and `sw_cbuf` never
updated again, so the *whole cart* froze one frame after any rotation (proven: an animated probe was
byte-identical frame 1 vs 12). Now (DE_NO_RAYLIB) the world layer is captured into an offscreen buffer
at zoom+translate, then **rotated about the screen centre into `sw_cbuf`** at the camera() reset /
present boundary — exact for uniform zoom (`screen = R·(worldbuf − centre) + centre`). Every primitive
stays on its fast axis-aligned path (one whole-layer composite, not per-primitive rotation), and HUD
drawn after a `camera()` reset stays un-rotated. Verified 0.04% off the GPU; desktop byte-identical.

**Net:** the only *correctness* gap left for portable 2D is `smooth_zoom`'s antialiasing (→ plain
zoom). `pal()`, scaling, and camera rotation all work; `tritex`/3D stays off-list by perf.

## Open questions

- **One renderer or two behind the seam?** Two keeps the GPU path first-class but re-introduces ANGLE
  on iOS. Probably commit to one.
- **`smooth_zoom` AA on the CPU** — reimplement the fractional-zoom antialiasing on the software canvas
  (supersample the world layer), or accept plain (aliased) zoom on the portable target. Minor; 1 cart.
