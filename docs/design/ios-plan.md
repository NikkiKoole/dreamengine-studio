# iOS build plan — the spike ladder + the reusable scaffold

STATUS: IN PROGRESS (2026-06-29). Spike 0 (toolchain) ✅ done. This is the *execution*
companion to the strategy docs — it tracks what's actually been built and proven, vs the
parked product thinking in those:

- **What & why we ship** → [`../decisions/0023-ship-carts-as-apps-not-the-editor.md`](../decisions/0023-ship-carts-as-apps-not-the-editor.md):
  finished apps (a cart, or a curated collection), **precompiled native on a dev box**; never the editor.
- **Product strategy** (monetization, AUv3 catalog, agentic store pipeline — in-house, not
  Fastlane, per [ADR-0026](../decisions/0026-store-pipeline-in-house-not-fastlane.md) —
  StoreKit 2) → [`product-notes-followup.md`](product-notes-followup.md) §2–§7.
- **The music product** the apps host → [`tinyjam-racks.md`](tinyjam-racks.md) (Tinyjam racks).
- **Where iOS sits among all the ways a cart reaches someone** → [`sharing-channels.md`](sharing-channels.md)
  (channel B — incl. what's still missing between "proven" and "in the store").

This doc owns the *how-do-we-actually-build-it* ladder. The live scaffold lives in the repo at
[`../../ios/`](../../ios/) (its `README.md` is the operational how-to; this is the rationale + roadmap).

## Decisions settled (2026-06-29)

- **Project generator: xcodegen.** A 7-line `project.yml` is the only hand-edited project file;
  `ios/build.sh` regenerates the `.xcodeproj` (never committed, never opened to manage files). Chosen
  over `cmake -G Xcode` for being lighter when juggling app + AUv3-extension targets, and the iOS
  community default for terminal/agentic work.
- **First target: a dead-simple hello-world** (proves the toolchain before any rack). ✅ done.
- **Render path: software canvas → on-screen texture, initially.** The cart draws into a CPU
  framebuffer (the existing `det-probes/` software rasterizer, already bit-identical across
  arm64/x86/wasm); iOS just uploads it as a texture and draws one fullscreen quad. **No GL, no ANGLE,
  no raylib-on-iOS** — which dodges the single biggest porting risk and fits the own-the-stack ethos.
  - **HW alternative, investigated and deferred:** [`ghera/raylib-iOS`](https://github.com/ghera/raylib-iOS)
    is a live (monthly-synced, last release 2026-06-28) fork on raylib 5.5/6.0 with prebuilt
    Apple-Silicon ANGLE, an Xcode template, and the `ios_ready/update/destroy` callback loop already
    solved — plus an in-flight *official* iOS PR ([raylib #5881](https://github.com/raysan5/raylib/pull/5881),
    building on [#3880](https://github.com/raysan5/raylib/pull/3880)). It's more turnkey than the
    brainstorm assumed, but pulls a large ANGLE/Chromium dependency and self-labels "experimental, not
    production-ready." **Revisit if** a rack needs full GL fidelity or the SW blit can't hold 60fps.
  - This supersedes the "spike 1 = render bake-off" plan: we go SW first to keep momentum; the bake-off
    becomes a later contingency, not a blocker.

## The spike ladder

Each spike is a small throwaway that kills one unknown. Riskiest cheap-thing first. Spikes 0–6 run
**simulator-only: zero code-signing, zero Apple-account interaction** — all free. Only spike 7
(AUv3, which needs a host) forces a physical device + signing.

| # | Spike | Proves | Needs | Status |
|---|---|---|---|---|
| 0 | xcodegen + hello-world → simulator (the whole agentic loop) | toolchain works end-to-end | sim | ✅ **done** (`ios/`) |
| 1 | software-canvas framebuffer → on-screen, driven by the iOS callback loop | the render path + loop inversion | sim | ✅ **done** — C RGBA buffer → CGImage → `CADisplayLink`; see `ios/history/spike1-canvas-loop.png` |
| 2 | audio: a C synth filling a CoreAudio render callback (`AVAudioSourceNode`) | the audio path | sim | ✅ **done** — stand-in arpeggio; VU meter proves callback pulled; `sound.h` swap-in is the follow-up. See `ios/history/spike2-audio-vu.png` |
| 3 | save: a `save_bytes` blob in the Documents dir via a Swift path bridge | the save layer | sim | ✅ **done** — `save.{h,c}` (portable stdio) + `de_documents_path` (`@_cdecl`); launch counter persists across relaunch; headless round-trip test. See `ios/history/spike3-save-launchcount.png` |
| 4 | StoreKit 2 + a local `.storekit` config (StoreKitTest): buy / entitlements, queried from C | the IAP model, no account/network | sim | ✅ **done** — in-house bridge (`Store.swift` + `tinyjam_store.h` via `@_cdecl`); headless XCTest buys → unlocks (master pass unlocks all). See `ios/history/spike4-storekit-gate.png` |
| 5 | App Group: app writes unlocked racks, a reader sees them via the shared suite | entitlement sharing for AUv3 | sim | ✅ **done** — `AppGroup.swift` (UserDefaults suite); Store mirrors entitlements in; test proves write→read. Entitlement wired in `project.yml`; the *true cross-process container* needs signing (lands with the spike-7 extension). See `ios/history/spike5-appgroup.png` |
| 6 | CloudKit sync of a saved tinyjam across devices (native-only nicety) | free cross-device sync | sim | — |
| 6.5 | **standalone app runs on a real iPhone** (signed) | signing + device deploy | **device** | ✅ **done** — iPhone (iOS 15.4.1), maker confirmed running. `ios/device.sh` |
| 7 | AUv3 extension makes sound, hosted | the killer feature | sim | ✅ **done** — extension reuses the C synth (`AU/TinyjamAU.swift`); our OWN host test finds it via `AVAudioUnitComponentManager`, instantiates + renders it offline (peak 0.180). No GarageBand/AUM/device needed. Real-host (AUM/GB) confirmation deferred to whenever a host is installed. |
| 8 | **the REAL engine (a cart) renders + sounds on iOS** (Phase 2) | the whole point | sim | ✅ **done** — omnichord (real `studio.c`+`sound.h`, zero Raylib) renders pixel-correct + upright on the iPhone 15 sim (`history/spike8-omnichord.png`); CoreAudio pulls the real mixer; UIKit touch drives it. See "Phase 2" below. |
| 8.5 | **a MULTI-CART app (the standalone Tinyjam) renders on iOS** | the umbrella app on a phone | sim | ✅ **done (2026-07-03)** — `build-app.js --ios` stages the dispatcher shim + per-cart wrappers into `ios/gen/app`; `project.yml` sources that directory; `ios/build.sh`/`device.sh` `APP=<manifest>`. Tinyjam's launcher (acid rack / session desk from de:meta, `>` cursor, footer) renders on the iPhone 15 sim, and **on a real iPhone** (`device.sh APP=tinyjam`, maker-confirmed). Touch back-to-launcher = temporary **hold-to-home** (hold the top-left corner ~0.3s; shim-drawn fat-finger pad, in racks only, device-confirmed). share-panel.md "Spike A" / next-spike #5. |
| 9 | **two AU instances render at once** in the spike-7 own host — one of each extension AND two of the same | AUv3 concurrency reality | sim | — |

**Why spike 9 matters (the multi-rack mixer question, 2026-07-03):** the Tinyjam plan is
one extension **per rack** — and *different* racks on different host tracks (acid + yacht
in GarageBand's mixer) each get **their own extension process**, own engine singleton,
mixed by the host: that Just Works and is the product's headline trick. The caveat is
**two instances of the SAME rack**: on iOS all instances of one extension share ONE
extension process, and our engine is a per-process singleton (global mixer, one slot
bank — the same wall the desktop `tools/bundle-spike/` hit). Two same-rack tracks would
trample each other's sound state. v1 answer: one instance per rack per session (hosts
tolerate this; some plugins ship that way), stated in the product copy. The real fix —
an engine instance-context refactor (every `sound.h` static behind a context pointer) —
is bigger than the umbrella app's cart-context swap and stays parked until a user
actually asks for two of the same rack.

Spike 1 mechanism shipped: `ios/Sources/canvas.{h,c}` (a stand-in software canvas — a few primitives
into an RGBA8888 buffer) + `CanvasView.swift` (CGImage from the buffer, `layer.magnificationFilter =
.nearest` for crisp pixels, `resizeAspect` letterboxing) driven by a `CADisplayLink` calling
`de_update(t)`. The `CGImage` path is the quick-proof; the 60fps-correct upgrade is a Metal
`CAMetalLayer` + one textured quad, deferred until a real cart needs the headroom. The bridging header
(`Sources/Bridging-Header.h`, wired via `SWIFT_OBJC_BRIDGING_HEADER` in `project.yml`) is how Swift
sees the C API. Gotcha learned: screenshot ~1.5s after launch or you catch the launch-zoom animation.

## Phase 2 — running the REAL engine on iOS (omnichord, not the stand-in)

> **STATUS 2026-06-29: ✅ DONE — the real engine renders + sounds on iOS.** omnichord (the real
> `studio.c` + `sound.h`, zero Raylib) renders **pixel-correct and upright** on the iPhone 15
> simulator (`ios/history/spike8-omnichord.png`), CoreAudio pulls the real mixer, and UIKit touch
> drives it (a desktop strum through the same `de_touch_*` path goes silent→0.374 peak). The fork
> below resolved **Path B**; the renderer decision settled "two renderers, one seam" (software now,
> GPU/Metal later). Engine-side foundation: [`engine-portability.md`](engine-portability.md).
>
> **What the iOS shell turned out to need** (all in the two Phase-2 commits):
> - `runtime/raylib_compat.c`: the `de_touch_*` seam now has bodies (a touch-point pool that
>   `GetTouchPointCount/GetTouchPosition` read) — the no-Raylib input was all-zero stubs before.
>   `de_open_path`'s `system()` is `#ifdef`'d out under `DE_NO_RAYLIB` (unavailable on iOS).
> - `ios/project.yml`: app target compiles `studio.c` + `raylib_compat.c` + `build/cart.c` with
>   `-DDE_NO_RAYLIB`, **`SCALE=1`** (so touches map 1:1 to framebuffer px), omnichord's screen/map
>   dims; `HEADER_SEARCH_PATHS` at `runtime/` + `build/`; stand-in `canvas/audio` excluded.
> - `ios/build.sh`: regenerates `build/{cart.c,sprites_data.h,map_data.h}` via `play.js` first —
>   the **"swap a cart" loop, extended to iOS**: `CART=<name> ./build.sh`.
> - `ios/Sources/`: `engine.h` (standalone seam decls for the bridging header), `CanvasView` (de_init/
>   de_frame/de_framebuffer; flips bottom-up `sw_cbuf`; UIKit touch → framebuffer px → `de_touch_*`),
>   `AudioEngine` (stereo `AVAudioSourceNode` splitting `de_audio_render`'s interleaved L/R).
> - `tools/build-nr.sh`: the desktop DE_NO_RAYLIB build/run recipe (the reference the project.yml mirrors).
>
> **The AUv3 hosts the real engine AND plays host MIDI (2026-06-29) — a real instrument rack.**
> `AU/TinyjamAU.swift` runs the same engine as the app. Each render block, in order: (1) parse the
> host's realtime MIDI event list (note-on/off + bend) → `de_midi_event()`; (2) **sample-clock**
> `de_frame()` (one 60Hz tick per 735 rendered samples) — the cart's keybed drains the MIDI ring
> (`keybed_update → midi_get → note_on/off`) and plays; (3) pull `de_audio_render()`. Sample-clocking
> (not a wall-clock timer) keeps it correct under a host's offline/faster-than-real-time render, and
> all three on the one audio thread means the MIDI ring needs no extra locking. The app and the AUv3
> host DIFFERENT carts, so `build.sh` stages each into its own dir (`gen/app` = omnichord, `gen/au` =
> **epiano** — a keybed instrument, silent until played). Engine seam: `midi_input.h` gates the
> CoreMIDI device backend to desktop (`!DE_NO_RAYLIB`) and exposes `de_midi_event`/`de_midi_bend` as
> the portable host-feed (same model as the web bridge). Verified: `AUHostTests` renders the
> out-of-process AUv3 offline — silent with no MIDI (peak 0.000), then a host note-on → **peak 0.106**.
>
> **Renderer FPS measured on-device + the ADR is written (2026-06-29).** The real iOS software-canvas
> app, profiled on a physical **iPhone SE 2nd-gen** via `ios/measure-device.sh`: 2D carts (omnichord/
> neonrain/flank) hold a locked **59–60fps** (~5.6ms engine+blit, ~⅓ of the 16.67ms budget, even in a
> Debug `-O0` build); `tritex`/3D (`podracer`) is **~89ms → ~10fps**. Both halves (desktop + device)
> agree → [ADR-0024](../decisions/0024-software-canvas-is-canonical-for-2d.md): **software canvas
> canonical for 2D (ANGLE-free iOS), `tritex`/3D GPU-only + off the initial iOS list.**
>
> **GPU-parity audited + closed (2026-06-29):** `pal()` (0px), scaling (host-side), and **camera
> rotation** (software rotation rasterizer — a 25° probe is 0.04% off the GPU) all work on the software
> canvas; the only remaining gap is `smooth_zoom`'s antialiasing (→ plain zoom, 1 cart). So the
> rotation carts (`hotline`/`sloop`/`coaster`/`worldpointer`) now render correctly on iOS. Full table:
> [`engine-portability.md`](engine-portability.md) §"GPU-only feature parity — audited".
>
> **Input wired (2026-06-29):** the no-GPU build was mouse/key zero-stubs, so mouse-driven carts got
> no input on iOS. Now the **primary finger drives the mouse API** (`GetMousePosition`/`IsMouseButton*`
> — `raylib_compat.c`), the way a browser synthesizes mouse from touch, so mouse carts play from touch
> AND the headless harness can drive them by injecting `de_touch_*`. A **key seam** (`de_key_event`)
> feeds `IsKeyDown/Pressed/Released` (the harness today; an on-screen keyboard later). Proven: a tap
> injected into `hotline` headless triggers `mouse_pressed → start_game →` rotated gameplay.
>
> **Open follow-ups:** (1) **Virtual gamepad/keyboard in the iOS shell — the recommended next** — the
> full design + 7-step plan lives in [`touch-controls.md`](touch-controls.md) (this is its Phase 4/5);
> see also [`action-plan.md`](action-plan.md) "Touch-input". Raw `key()` carts (WASD movement, etc.) have no
> touch equivalent. The engine already has an on-screen d-pad/buttons (`show_touch_ui`/`TOUCH_CONTROLS`)
> and the `de_key_event` seam is ready — wire them into the iOS shell so gamepad + key carts become
> playable on the phone. (2) MIDI CC → cart knobs (the engine's MIDI is note+bend only today). (3)
> `smooth_zoom` AA on the CPU, or accept plain zoom. (4) a Metal GPU backend if a `tritex`/3D cart needs iOS.

Spikes 0–7 proved the iOS *shell* with stand-in `canvas.c`/`audio.c`. Phase 2 plugs the real
`studio.c` + `sound.h` + a cart (`omnichord` is the target) into it. Scoping (2026-06-29):

**What helps us:**
- `studio.c` already has a **software canvas** — `sw_cbuf[SCREEN_W*SCREEN_H]`, a CPU RGBA framebuffer
  (`DE_SOFTWARE_CANVAS=on` / `SW_CANVAS_DEFAULT`). Primitives can rasterize on the CPU, no GL. This is
  the same shape as our spike-1 blit.
- `sound.h` has **`sound_synth_mode`** (the `--wav` path): the main thread pumps `sound_callback` to
  fill a buffer with NO audio device. That's the exact hook to feed CoreAudio / the AUv3 render block
  with the real mixer — same shape as our spike-2 path.

**What's in the way:** `studio.c` (4287 lines) `main()` is deeply Raylib-coupled — `InitWindow`,
`InitAudioDevice`, `GetCharPressed`/mouse, `LoadFontFromImage`/`LoadTexture`, the `WindowShouldClose`
loop, `GetTime`. Raylib owns window/input/audio-device/assets/timing; the software canvas only
replaces primitive *drawing*.

### The fork

- **Path A — raylib-iOS (ghera/ANGLE).** Build `studio.c` against the fork; Raylib provides
  window/GL(→Metal)/input/audio/assets, we only invert the loop + map touch. Most faithful, least
  engine surgery; but adds the large ANGLE dependency on an "experimental" fork, and **discards the
  shell our 8 spikes built**.
- **Path B — host-less (reuse our spike shell).** Render via `sw_cbuf` → our Metal/CGImage blit
  (spike 1), audio via `sound_callback` synth-mode → CoreAudio (spike 2), input from UIKit touches,
  save via spike 3, IAP/AUv3 from spikes 4/5/7. The work is **decoupling `studio.c` from Raylib** for
  window/input/audio-device/asset-loading/timing — real surgery in a file not written to be host-less,
  but it reuses everything we've already proven and keeps the own-the-stack ethos (no ANGLE).

**Lean: Path B** — our spikes *are* the Path-B shell; this becomes "plug the real engine into the shell
we built," and the AUv3 render block calling the real `sound_callback` is the headline. First milestone:
compile `studio.c` for iOS with Raylib stubbed/removed, render one cart's `draw()` into `sw_cbuf`, blit
it, and pump `sound_callback` to CoreAudio — then wire touch.

**The maker can change the engine** (confirmed 2026-06-29) — so Path B is a *clean platform seam*, not
a workaround. The full refactor survey + the load-bearing **software-canvas-vs-GPU renderer decision**
(gated on fps measurement) live in [`engine-portability.md`](engine-portability.md). That decision
gates Path B and must be settled (by measuring, on desktop AND device) before the real-engine port.

Deferred and noted by the maker: **fluid/responsive layout** (a CSS-like positioning experiment in one
cart) — Phase 2 just letterboxes the fixed canvas as the spikes do; revisit responsive later.

## The reusable loop (what "add an app" becomes)

`ios/` is a toolkit, not a per-app project. Today it's the hello-world; the durable shape is:
`project.yml` (targets) + `Sources/` (the Swift/Obj-C shell + the cart's C) + `build.sh` (one command:
generate → build → boot → install → launch → screenshot). Adding an app later ≈ swap the cart C,
append a target + module id, re-run `build.sh` — the same "swap a `.c`" loop the rest of dreamengine
already uses, extended to iOS.

## Open / next

- **Spike 1**: pick the on-screen-texture mechanism (Metal `CAMetalLayer` + one textured quad is the
  60fps-correct choice; a `CGImage`/`UIImageView` updated by `CADisplayLink` is the 20-minute version
  to prove the pixels first, then upgrade). Wire the `ios_ready/update/destroy` callbacks around it.
- Then spikes 2–4 (audio, save, StoreKit-local) — all still simulator-free-tier.
- `ios/` is a new top-level dir; if it grows, give it a one-line entry in the `docs/README.md` layout
  tree (and CLAUDE.md project-structure) so it stays discoverable.

Gotchas captured (so nobody re-hits them):
- **Swift is 5.9.2** (Xcode 15.1) — no `nonisolated(unsafe)` (that's 5.10+). Plain `static var` for
  the C-readable entitlement cache.
- **`Tinyjam.storekit` currently ships in the app bundle** (so `SKTestSession` finds it via the main
  bundle). Harmless for the spike; **exclude it from release builds** before shipping.
- **StoreKit-test purchases persist per-simulator** across plain launches — handy (the in-app gate
  shows unlocked) but reset with `xcrun simctl` / `session.clearTransactions()` if you want a clean
  locked state.
- **`simctl launch` does NOT apply a scheme's StoreKit config** — that's why the headless proof is an
  XCTest using `SKTestSession(configurationFileNamed:)`, not a plain app launch. (A scheme
  `run: storeKitConfiguration:` only helps Xcode's Run button, not `simctl`/`ios-deploy` — so
  interactive local testing needs another route: bundle the `.storekit` + an in-app `SKTestSession`,
  or verify via TestFlight sandbox.)
- **IAP in the multi-cart app — DEFERRED (2026-07-03, "later story").** Spike 4 proved the bridge
  *headlessly*; nothing gates in the REAL app path yet (the only `Store_IsModuleUnlocked` caller is
  `canvas.c`, the excluded spike stand-in). The next increment, when it resumes: (1) `apps/tinyjam/app.json`
  grows an `iap.products` block (id/price/name/desc/`unlocks[]`) as the single source of truth;
  (2) `build-app.js --ios` GENERATES `Tinyjam.storekit` from it + threads each rack's productId +
  lock state into `app_roster.h`; (3) the launcher (`tinyjam-menu.c`) shows locked racks (price/🔒),
  taps fire `Store_Purchase`, owned racks launch — cross-platform via a **weak** `Store_*` symbol
  (real on iOS, absent → "free/owned" on Mac/editor, so standalone carts are unaffected). Real prices
  still come from App Store Connect (ADR-0026); the manifest declares intent.
- **AUv3 (spike 7):** `.loadInProcess` is **macOS-only** — on iOS AUv3 always loads out-of-process;
  instantiate with `options: []`. The instrument self-plays (the arpeggio isn't gated on MIDI), so it
  renders sound with no note input — handy for an automated render test. The extension is embedded via
  an `embed: true` app dependency; its `AudioComponents` registration lives in the xcodegen `info:`
  block (generated `AU/Info.plist`, gitignored). Re-adding the app-group entitlement (so the extension
  reads StoreKit unlocks) is the remaining wiring for a *real* host + signed build.
- **Device deploy (spike 6.5):** `devicectl`/CoreDevice does NOT see the iOS-15 iPhone ("No devices
  found" though `xctrace` lists it) — use **`ios-deploy`** (classic protocol, `brew install ios-deploy`)
  via `ios/device.sh`. The signing cert is minted on the first signed `xcodebuild` (after adding the
  Apple ID in Xcode → Settings → Accounts); team `JH2ZCZH58D`. The **app-group entitlement was removed
  from `project.yml`** to let plain device signing succeed (automatic provisioning didn't include the
  group) — re-add it in spike 7 once the group is registered. iOS 15.4.1 needs **no Developer Mode**
  (that's iOS 16+).
- **Fresh-machine setup: Xcode 15's base download does NOT include the iOS device platform.** The
  `.xip` is only ~2.9GB; `xcode-select` + `xcodebuild -runFirstLaunch` still leave device builds
  failing with `xcodebuild: error: … iOS X.Y is not installed. … download and install the platform`
  (the `generic/platform=iOS` destination is ineligible) — even though `-showsdks` lists the SDK. Fix:
  `xcodebuild -downloadPlatform iOS` (or Xcode → Settings → Platforms → iOS → Get), ~7GB. On an older
  **Intel** Mac the subsequent "Verifying iOS X.simruntime…" install step is *very* slow (can look
  like hours) — let it finish in the background before re-running `device.sh`. The signing cert won't
  mint until the build gets past this (it dies before the signing phase, so `find-identity` stays
  empty — not a signing problem, a missing-platform one).
- **Old simulators don't exist under old/new Xcode mismatches.** `build.sh` defaults `DEVICE="iPhone
  15"`, which is absent on e.g. Xcode 13 — override with `DEVICE="iPhone 13" ./build.sh` (or any name
  from `xcrun simctl list devices available`).
