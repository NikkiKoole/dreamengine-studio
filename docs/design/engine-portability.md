# Engine portability — the platform seam + the renderer decision

STATUS: OPEN / survey (2026-06-29; desktop FPS half measured — see "Measured" below). This captures
the refactors that would make the engine cleanly portable (iOS first, but they help web too) and the
**one undecided decision** everything hangs on: **software canvas vs GPU as the canonical renderer** —
gated on FPS measurement (desktop done; device still owed before the ADR).

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

**Still owed:** the **device** measurement (iPhone, per the plan) — necessary before this is settled
as an ADR.

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

## Open questions

- **The renderer decision** — software canvas vs GPU (measure fps; desktop **and** device). The gate.
- **GPU-only feature parity** — reimplement `pal()`/scale/`smooth_zoom`/`tritex` on CPU, or drop/limit
  them for the portable target?
- **One renderer or two behind the seam?** Two keeps the GPU path first-class but re-introduces ANGLE
  on iOS. Probably commit to one.
