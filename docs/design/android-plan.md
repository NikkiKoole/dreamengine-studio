# Android build plan — the spike ladder + the host shell

STATUS: BUILDING (2026-07-16) — spikes 0–3 ✅ **done**: the real engine cross-compiles with the NDK
and a `NativeActivity` host (in `android/`) renders it **pixel-perfect** on an arm64 emulator, with
AAudio pulling the mixer and touch driving the cart — all frameworkless (`-DDE_NO_RAYLIB`), zero
engine surgery. `android/build.sh` is the one-command loop (`CART=<name> ./build.sh`). What remains
is save (4), Gradle-signed packaging → `.aab` (5), Play Billing (6), and the multi-cart app (7).
This doc is the execution companion — the Android twin of [`ios-plan.md`](ios-plan.md). It owns the
*how-do-we-build-it* ladder; the strategy lives in the shared docs:

- **What & why we ship** → [`../decisions/0023-ship-carts-as-apps-not-the-editor.md`](../decisions/0023-ship-carts-as-apps-not-the-editor.md):
  finished apps (a cart, or a curated collection), precompiled on a dev box; never the editor. Same on Android.
- **The platform seam** the whole plan rests on → [`engine-portability.md`](engine-portability.md)
  ("adding a target later is *implement one backend behind the seam*, not surgery on studio.c").
- **The iOS build that Android mirrors** → [`ios-plan.md`](ios-plan.md) + the live scaffold in
  [`../../ios/`](../../ios/). Android's `build.sh`/`project.yml`/`Sources/` all have iOS twins to copy.
- **Store pipeline** (in-house, not a third-party SDK, per [ADR-0026](../decisions/0026-store-pipeline-in-house-not-fastlane.md)) →
  [`store-agents.md`](store-agents.md). The iOS side is `asc-push.js` → App Store Connect; the
  Android twin is a Play Developer API pusher (below).
- **Where Android sits among the ways a cart reaches someone** → [`sharing-channels.md`](sharing-channels.md)
  (channel B, the App Store — Google Play is the second storefront on that channel).

The live scaffold will land at `android/` (a new top-level dir, gitignoring the generated Gradle
project + `build/` exactly as `ios/` gitignores the `.xcodeproj`).

## Why this is mostly done already

dreamengine's runtime is a single C core (`runtime/studio.c` + `sound.h` + a compiled cart) driven
through **one seam**, `runtime/platform.h`. Compiled with `-DDE_NO_RAYLIB` it needs **only libc +
libm** — no Raylib, no GPU library, no OS frameworks. That path is:

- **proven on iOS** — `ios/` ships it (CanvasView blits `de_framebuffer()`, AudioEngine pulls
  `de_audio_render()`, UIKit touch → `de_touch_*`), and
- **proven on desktop headless** — [`tools/build-nr.sh`](../../tools/build-nr.sh) +
  `tools/headless-nr.c` drive the identical seam with plain `clang … -lm`, no window.

Android's NDK toolchain is that same clang with an Android target triple. So the **entire
Homebrew-Raylib dependency is already absent** from the path Android uses. The Raylib-linked builds
— native desktop in [`build-app.js`](../../tools/build-app.js) and wasm in
[`build-site.js`](../../tools/build-site.js) — are the only ones that *don't* carry over, and
Android doesn't want them.

**The host seam Android must implement** (from `ios/Sources/engine.h`, mirroring `platform.h`):

| Concern | Contract | iOS host that implements it |
|---|---|---|
| Render | `de_frame(t)` → `de_framebuffer()` = RGBA8888, **bottom-up** (blit must flip), `de_screen_w/h()` | `CanvasView.swift` |
| Audio | `de_audio_render(out, frames)` = stereo interleaved float @ 44100, on the audio thread | `AudioEngine.swift` |
| Touch | `de_touch_begin/moved/ended(id, x, y)` in framebuffer px (SCALE=1) | `CanvasView.swift` |
| Layout | `de_resize()`, `de_set_safe_area()`, `de_set_backing_scale()`, `de_is_resizable()` | `CanvasView.swift` `layoutSubviews` |
| MIDI (later) | `de_midi_event()`, `de_midi_bend()` | `AU/TinyjamAU.swift` |
| Save | `save.{h,c}` (portable stdio) + a host-provided documents path | `save.c` + `Save.swift` |
| Purchases | `Store_IsModuleUnlocked(id)` / `Store_Purchase(id)` (`tinyjam_store.h`) | `Store.swift` (StoreKit 2) |

## The fork — recommended stack

The engine path is settled; the **host stack** is the real decision. Recommendation, each choice
mapped to its iOS twin:

- **Shell: NDK + `NativeActivity` (spike 1+), `GameActivity` migration later.** The spike shipped on
  **NativeActivity + `native_app_glue`** — pure C, the glue is bundled in the NDK
  (`$NDK/sources/android/native_app_glue`), zero androidx/AAR/Kotlin, `android:hasCode="false"`. That
  was the fastest kill of the render/audio/touch unknowns (spikes 1–3 done in one sitting). **Migrate to
  `GameActivity`** (`androidx.games.activity`) when billing/text-input land — it needs a Kotlin Activity
  subclass + the AAR anyway, and there is *always* a JVM side on Android because Play Billing is a Java
  library. So the endpoint keeps a small Kotlin surface (lifecycle + billing); the spike just didn't need
  it yet.
- **Render: GLES2 texture-quad.** Upload the RGBA framebuffer as a texture each frame, draw one
  fullscreen quad, flip + nearest-filter in the shader → free GPU integer-upscale of the ~320×200
  canvas, crisp chunky pixels. (A pure `ANativeWindow_lock` + `memcpy` software blit is even simpler
  and more on-ethos, but you'd upscale on the CPU or let the window stretch — start with GLES.)
  This is the twin of iOS's `CGImage`→`layer.contents` (and the future Metal quad).
- **Audio: AAudio.** A **pure-C** API (no C++ dependency, unlike Oboe), min SDK 26 (Android 8.0,
  2017) — a fine floor. Its render callback pulls `de_audio_render()` / `sound_callback()` directly,
  the exact shape of the CoreAudio `AVAudioSourceNode`. (`sound.h` is portable C + `<math.h>`; its
  only platform-specific line is an x86 SSE denormal-flush that is ARM-safe.)
- **Input: `MotionEvent` → `de_touch_*`.** GameActivity delivers motion events natively (or Kotlin
  `dispatchTouchEvent` → JNI). Map through the aspect-fit rect into framebuffer px, same as iOS.
- **Packaging: Gradle, project generated + gitignored.** The twin of xcodegen's `project.yml` — a
  small committed template that a script expands into a full Gradle project, built to an **`.aab`**
  (Android App Bundle, Play's required upload format) via `./gradlew bundleRelease`.
- **Billing: Google Play Billing Library** (Kotlin) feeding the *same* `Store_*` C gate over JNI.
  The `tinyjam_store.h` seam already exists; only the implementation swaps (StoreKit 2 → BillingClient).

## Toolchain (installed 2026-07-16, GUI-free via Homebrew)

All installed **without sudo** — JDK via the keg-only formula (not the `.pkg` cask), SDK bits via
`sdkmanager` into a Homebrew-owned prefix. Env the build scripts need:

```sh
export JAVA_HOME=/opt/homebrew/opt/openjdk@17
export ANDROID_HOME=/opt/homebrew/share/android-commandlinetools
export PATH="$JAVA_HOME/bin:$ANDROID_HOME/platform-tools:$PATH"
```

Installed: `openjdk@17` (17.0.19) · `android-commandlinetools` (`sdkmanager` 20.0) · `platform-tools`
(`adb` 1.0.41) · `platforms;android-35` · `build-tools;35.0.0` · `ndk;28.2.13676358` (r28c, clang
19.0.1) · `cmake;3.22.1`. The NDK target compiler for arm64 is
`$ANDROID_HOME/ndk/28.2.13676358/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android26-clang`
(the `darwin-x86_64` prebuilt runs under Rosetta on Apple Silicon — normal).

Also installed: `emulator` + `system-images;android-35;google_apis;arm64-v8a` + an AVD named
**`de_test`** (Pixel 6, arm64). Boot it headless with
`emulator -avd de_test -no-window -no-audio -no-snapshot -gpu swiftshader_indirect &`, wait for
`sys.boot_completed`, then `adb`. (A plugged-in USB device with developer mode works too, no download.)

**Still needed for the host shell (spike 1+):** the Gradle/AGP layer isn't set up yet — a
`build.gradle`/AGP + a `GameActivity` template + the GLES2/AAudio JNI shell. That's the next build,
not a download.

## The spike ladder

Each spike kills one unknown; riskiest cheap-thing first. Spikes 0–5 run on an **emulator or a
plain USB device with `adb install` — no Play account, no signing hassle, all free** (the Android
analog of iOS's "simulator-only" tier). Only spike 6 (Play Billing) forces a Play Console account +
a real signed upload.

| # | Spike | Proves | Needs | Status |
|---|---|---|---|---|
| 0 | NDK cross-compile `build-nr.sh`'s source list → a `.so` per ABI, run headless (or via `headless-nr.c`) on an emulator | the toolchain: engine builds + runs with the Android clang | emu | ✅ **DONE (2026-07-16)** — the real engine (`studio.c`+`raylib_compat.c`+`sound.h`+omnichord, `-DDE_NO_RAYLIB`) cross-compiles clean with NDK r28c clang to a valid `ELF … ARM aarch64 … interpreter /system/bin/linker64` binary, zero warnings, and **RUNS on an arm64 emulator**: `adb push` + `headless-nr-arm64 90 nr.ppm nr.wav` → omnichord renders **pixel-perfect** (320×200 PPM, full UI) + the mixer produces 132300 stereo samples (silent with no touch input, correct — omnichord plays only when strummed). No external asset files needed (fonts/sprites are baked under `DE_NO_RAYLIB`). Proof frame: `build/android/out/nr.png`. |
| 1 | NativeActivity + GLES2 quad shows `de_framebuffer()` on screen, driven by the native_app_glue loop | render path + loop inversion (Android owns the loop) | emu | ✅ **DONE (2026-07-16)** — omnichord renders **pixel-perfect, upright, letterboxed** on the arm64 emulator (`build/android/out/omnichord-final.png`). Scaffold lives in `android/` (`build.sh` = the one-command loop). Went NativeActivity (pure C, glue bundled in NDK), not GameActivity — see note below. |
| 2 | AAudio callback pulls `de_audio_render()` — a cart's sound plays | the audio path | emu | ✅ **DONE (2026-07-16)** — AAudio stream (44100/stereo/float, low-latency) pulls `de_audio_render` on its callback thread; logged `AAudio started: 44100 Hz, 2 ch`. (Emulator ran `-no-audio`; wired + opening cleanly — real device confirms loudness later.) |
| 3 | `MotionEvent` → `de_touch_*` — touch drives the cart | input | emu | ✅ **DONE (2026-07-16)** — an injected `adb input tap` flows through `de_touch_*`; omnichord responded (header → "C m7", the m7 modifier + Cm/C7 lit). Multi-pointer handled via pointer-id iteration. |
| 4 | `save.c` round-trips a blob to internal storage (host provides the path) | the save layer | emu | — |
| 5 | `android/build.sh` builds a signed **debug** `.aab`/`.apk`, `adb install`s + launches; screenshot | packaging + the one-command agentic loop | device | — |
| 6 | Play Billing → the `Store_*` gate: buy a rack → entitlement unlocks (license testing, no real charge) | the IAP model on Play | Play acct | — |
| 7 | a MULTI-CART app (`apps/<name>/app.json`) → Android app via a `build-app.js --android` staging path | the umbrella app on a phone | device | — |

**Product shape** (recommendation): same as iOS per ADR-0023 — ship individual carts and multi-cart
*apps* to Play, never the editor. Spike 7 is the twin of iOS spike 8.5 (`build-app.js --ios` stages
a dispatcher + per-cart wrappers into `ios/gen/app`; `--android` stages the same into the generated
Gradle project and lets NDK compile).

## Editor export button (shipped 2026-07-16)

The editor's **⇪ share** popover ("Give it to someone") now has a **🤖 export .apk** action next to
🍎 export mac / 🖥 export .exe / 📱 to iPhone. It builds the **live buffer** into a sideloadable
debug `.apk` (arm64 + arm32) — no Play account, no device, no signing setup — and reveals it in
Finder to send to anyone; they enable "install unknown apps" and tap it. Wiring mirrors the Windows
export path exactly: `shell.js` (`export-android-btn`) → `preload.cjs` (`exportAndroid`) →
`main.cjs` (`studio:export-android`: `prepareCart` → save dialog → `android/build.sh` in **EDITOR
mode** → copy + reveal). `build.sh EDITOR=1` skips `play.js` and builds `build/{cart.c,…}` (what
`prepareCart` wrote) with dims from `DE_*` env. (Share popover is opt-in: settings → show share.)

## The reusable loop (what "add an app" becomes)

Like `ios/`, `android/` is a toolkit, not a per-app project: a committed Gradle template +
`Sources/` (the Kotlin/C shell) + `build.sh` (one command: regenerate the cart's `build/{cart.c,…}`
via `play.js` → NDK-compile per ABI → stage into Gradle → `bundleRelease` → `adb install` →
screenshot). Adding an app ≈ swap the cart C, append a product id, re-run — the same "swap a `.c`"
loop, extended to Android.

## Anticipated gotchas (from the iOS scars + Android specifics)

- **The entitlement-read heap race applies verbatim.** iOS hit nano-zone heap corruption because a
  background StoreKit `Task` mutated a `Set` the C loop read every frame (`ios/README.md` gotcha #1).
  The Play BillingClient runs its listeners on a background thread too — so the JNI bridge the C gate
  reads every frame **must be a lock-guarded snapshot or a primitive**, never a live shared JVM
  object read across the fence.
- **Bottom-up framebuffer — do NOT flip in the GLES path (hit 2026-07-16).** `sw_cbuf` is bottom-up,
  but GLES texel (0,0) already maps to uv (0,0), so a straight quad (uv = aUV, no `1.0 - v`) shows the
  image upright. Adding the "obvious" flip renders it upside-down. (iOS flips because it builds a
  top-down array via a mirrored `CGContext` — a different path; don't copy that flip into the shader.)
- **Never point the build at the shared `build/cart.c` (hit 2026-07-16).** A sibling process (the
  editor, another agent) overwrote `build/cart.c` between `play.js` and the CMake build → the APK shipped
  the *wrong cart* (voxroll instead of omnichord). Fix, mirroring `ios/gen/app`: `android/build.sh`
  regenerates and **immediately** copies `cart.c`+`sprites_data.h`+`map_data.h` into a private
  `app/src/main/cpp/gen/` (gitignored); CMake reads `DE_GEN`, never `build/`.
- **`SCALE=1` on the device build** so touches map 1:1 to framebuffer px (iOS learned this in Phase 2);
  the chunky-pixel look comes from a `pixelChunk`-style logical-px divisor in the host, not from SCALE.
- **AAudio buffer sizing / underruns** — set the buffer to ~4× the burst (`AAudioStream_setFramesPerBurst`
  × 4) so scheduler jitter doesn't crackle; recover a genuinely disconnected stream via the
  **error callback** (`setErrorCallback` → flag → reopen on the main thread), NOT by polling.
- **Audio is DECOUPLED from the window/orientation lifecycle (hit hard 2026-07-16).** Start the
  AAudio stream ONCE (in `android_main`) and leave it running through rotation. Do NOT stop/restart
  it on `TERM_WINDOW`/`INIT_WINDOW`/`CONFIG_CHANGED` — only the EGL render surface follows the window.
  Reopening audio on rotation churns the stream (we saw 4 stop/starts in 4s, incl. one from the
  activity self-recreating at startup) and **breaks the emulator's fragile audio route**. On a real
  device audio survives rotation natively; the only legitimate reopen trigger is a real disconnect.
- **The Apple-Silicon emulator's audio-to-host output is UNRELIABLE — ROOT-CAUSED 2026-07-16.** Same
  clean build played on one cold boot, silent on others, while the engine generated identical samples
  every time (0.3–0.8 peak via a temporary RMS probe). Cause: **third-party virtual audio devices
  (BlackHole, Soundflower, Loopback, eqMac, Multi-Output devices) conflict with the emulator grabbing
  the host audio route at launch** (widely reported; e.g. eqMac2 #41). This box has BlackHole + a
  Multi-Output device. **The working recipe on the emulator:**
  1. **Device nudge after launch** — flip the macOS default output to another device and back
     (`SwitchAudioSource -s <other>; SwitchAudioSource -s <speakers>`); forces macOS to re-hand the
     route to the emulator. Fixes the startup silence.
  2. **Lock orientation** (`android:screenOrientation="landscape"`) — rotation makes the emulator
     re-drop the route, and the *app can't nudge the host's audio device itself*. Locking sidesteps it,
     and it's the right product default for these fixed-size landscape carts anyway.
  3. **Deep audio buffer** — `setFramesPerDataCallback(512)` + `setBufferCapacityInFrames(8192)` +
     buffer size 8× burst kills the underrun crackle (the emulator's jittery scheduler). ~20ms latency;
     tune down on device.
  None of this exists on a real device (audio survives rotation, no route-grab conflict). Capturing the
  host output to verify objectively (BlackHole loopback + ffmpeg) is walled off by macOS Microphone TCC
  permission (the `claude`/terminal process lacks it; granting needs an app restart). **Reliable audio
  validation = a physical device** (or the desktop `build-nr.sh` WAV render — same `de_audio_render`
  mixer). Render + touch are rock-solid on the emulator.
- **Play's `.aab` + signing:** Play requires an App Bundle and **Play App Signing** (Google holds
  the app key; you upload with an upload key). The push tool must target the **Google Play Developer
  Publishing API** (the `asc-push.js` twin) — release tracks (internal → closed → production),
  listing metadata, and IAP products defined in the Play Console.
- **ABIs:** ship at least `arm64-v8a` (all modern devices); add `armeabi-v7a` for old phones and
  `x86_64` for the emulator. Each is one NDK-clang pass over the same source list.
- **Min SDK 26** (AAudio) drops pre-2017 devices — confirm that's acceptable before locking it in.

## Open decisions for the maker

1. **Scope of v1** — same product shape as iOS (carts + multi-cart apps to Play), or one flagship
   cart first as a proof? (Recommendation: same shape, spike-laddered.)
2. **Billing now or later** — spikes 0–5 give a playable APK on a device with no Play account; defer
   spike 6 until a store push is actually wanted?
3. **Store metadata reuse** — Google Play *does* index the full description for search (unlike
   Apple's title/subtitle/keywords-only). The `aso-*` tooling and `apps/<name>/press.md` may need a
   Play-flavoured brief. Parkable until spike 6/7.

## Next

Spike 0: install the NDK, and cross-compile `tools/build-nr.sh`'s exact source list
(`headless-nr.c` + `studio.c` + `raylib_compat.c` + `build/cart.c`, `-DDE_NO_RAYLIB … -lm`) with
`--target=aarch64-linux-android<API>` into a `.so`; run it on an emulator to prove the engine builds
and ticks under the Android clang. That's the cheapest kill of the biggest remaining unknown.

When `android/` lands, give it a one-line entry in [`../README.md`](../README.md)'s layout tree and
in CLAUDE.md's project-structure block so it stays discoverable (the same housekeeping `ios/` got).
