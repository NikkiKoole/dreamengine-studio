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
| 1 | [software-canvas](software-canvas.md) framebuffer → on-screen, driven by the iOS callback loop | the render path + loop inversion | sim | ✅ **done** — C RGBA buffer → CGImage → `CADisplayLink`; see `ios/history/spike1-canvas-loop.png` |
| 2 | audio: a C synth filling a CoreAudio render callback (`AVAudioSourceNode`) | the audio path | sim | ✅ **done** — stand-in arpeggio; VU meter proves callback pulled; `sound.h` swap-in is the follow-up. See `ios/history/spike2-audio-vu.png` |
| 3 | save: a `save_bytes` blob in the Documents dir via a Swift path bridge | the save layer | sim | ✅ **done** — `save.{h,c}` (portable stdio) + `de_documents_path` (`@_cdecl`); launch counter persists across relaunch; headless round-trip test. See `ios/history/spike3-save-launchcount.png` |
| 4 | StoreKit 2 + a local `.storekit` config (StoreKitTest): buy / entitlements, queried from C | the IAP model, no account/network | sim | ✅ **done** — in-house bridge (`Store.swift` + `tinyjam_store.h` via `@_cdecl`); headless XCTest buys → unlocks (master pass unlocks all). See `ios/history/spike4-storekit-gate.png` |
| 5 | App Group: app writes unlocked racks, a reader sees them via the shared suite | entitlement sharing for AUv3 | sim | ✅ **done** — `AppGroup.swift` (UserDefaults suite); Store mirrors entitlements in; test proves write→read. Entitlement wired in `project.yml`; the *true cross-process container* needs signing (lands with the spike-7 extension). See `ios/history/spike5-appgroup.png` |
| 6 | CloudKit sync of a saved tinyjam across devices (native-only nicety) | free cross-device sync | sim | — |
| 6.5 | **standalone app runs on a real iPhone** (signed) | signing + device deploy | **device** | ✅ **done** — iPhone (iOS 15.4.1), maker confirmed running. `ios/device.sh` |
| 7 | AUv3 extension makes sound, hosted | the killer feature | sim | ✅ **done** — extension reuses the C synth (`AU/TinyjamAU.swift`); our OWN host test finds it via `AVAudioUnitComponentManager`, instantiates + renders it offline (peak 0.180). No GarageBand/AUM/device needed. Real-host (AUM/GB) confirmation deferred to whenever a host is installed. |
| 8 | **the REAL engine (a cart) renders + sounds on iOS** (Phase 2) | the whole point | sim | ✅ **done** — omnichord (real `studio.c`+`sound.h`, zero Raylib) renders pixel-correct + upright on the iPhone 15 sim (`history/spike8-omnichord.png`); CoreAudio pulls the real mixer; UIKit touch drives it. See "Phase 2" below. |
| 8.5 | **a MULTI-CART app (the standalone Tinyjam) renders on iOS** | the umbrella app on a phone | sim | ✅ **done (2026-07-03)** — `build-app.js --ios` stages the dispatcher shim + per-cart wrappers into `ios/gen/app`; `project.yml` sources that directory; `ios/build.sh`/`device.sh` `APP=<manifest>`. Tinyjam's launcher (acid rack / session desk from de:meta, `>` cursor, footer) renders on the iPhone 15 sim, and **on a real iPhone** (`device.sh APP=tinyjam`, maker-confirmed). Touch back-to-launcher = temporary **hold-to-home** (hold the top-left corner ~0.3s; shim-drawn fat-finger pad, in racks only, device-confirmed). [share-panel.md](share-panel.md) "Spike A" / next-spike #5. |
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

> **UPDATE 2026-08-13 — this got worse, and it is no longer only about two of the same rack.** A live
> sample of a wedged GarageBand found the UI and the audio in **different processes**, with THREE
> engines in the UI one. So "one engine per process" is not merely a musical limitation now; it breaks
> a plug-in whose VIEW renders the engine's framebuffer, because the panel is drawn by a different
> engine than the one you hear. The instance-context refactor named above is one of the four ways out.
> Read [The out-of-process wall](#the-out-of-process-wall-the-open-fork-2026-08-13) before planning
> anything here.

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

## macOS: hosting the AUv3 in GarageBand and Logic

Phase 1 of "run our plug-in on the Mac". Driven by **`ios/mac.sh`** off **`ios/project-mac.yml`** —
a SEPARATE xcodegen spec producing `TinyjamMac.xcodeproj`, so it cannot clobber the iOS project that
another agent may be building in this shared tree.

**Mac Catalyst, not a native macOS target**, because all of our UI is UIKit
(`Sources/CanvasView.swift`) and the AU extension already exists: Catalyst reuses both, a native
AppKit target would mean a second host view to maintain forever.

**Status: WORKING in GarageBand on macOS (confirmed by the maker, 2026-08-12).** `zsh ios/mac.sh`
builds, signs, installs, registers, and `auval -v aumu tacj Mpla` reports **AU VALIDATION
SUCCEEDED**. In GarageBand the plug-in loads on a software-instrument track, plays, **stops when the
host stops, and follows the host's tempo** (phase 2, below). What it does NOT have is a view: the
host shows its generic panel, which is phase 3 and the bulk of the remaining work.

Getting from "builds" to "loads" was three separate gates, each of which reported a different
symptom, and none of which was a code bug:

| symptom | cause |
|---|---|
| registers nowhere; `pluginkit` lists nothing; `codesign` says `Signature=adhoc` / `TeamIdentifier=not set` | **macOS will not register an app extension from an ad-hoc-signed app.** Ad-hoc was chosen to sidestep provisioning and that was the wrong trade: it builds happily and the system silently ignores the plug-in. Fixed with `DEVELOPMENT_TEAM` + `CODE_SIGN_STYLE=Automatic` |
| registers, appears in GarageBand's instrument list, then fails to open with an orange **!** and NO crash report — `OpenAComponent: result 4` | **an app extension only loads if it is SANDBOXED.** The only entitlement present was `com.apple.security.get-task-allow`, a debug flag. Fixed by adding `com.apple.security.app-sandbox` to the extension AND its carrier app |
| `OpenAComponent: result 4097` (0x1001 = the XPC "couldn't launch the helper" error), now WITH a crash report: `EXC_BREAKPOINT` in `libsystem_secinit` `_libsecinit_appsandbox`, during dyld initializers | **`com.apple.security.inherit` must NOT be set on an app extension.** It is for XPC *services* that adopt their parent's sandbox; an extension gets its own, so `inherit` makes the sandbox invalid and the process traps before `main`. That is what 4097 looks like from the host's side |

Lesson worth keeping: for a plug-in that registers but won't open, the useful evidence is
`codesign -d --entitlements -` on the `.appex` and `~/Library/Logs/DiagnosticReports/`. The absence
of a crash report is itself a signal — it means refused-at-load rather than crashing.

### Four platform findings, all fixed, all of which would bite anyone repeating this

1. **`echo()` and `filter()` collide with curses on the macOS SDK.** `studio.c` includes
   `<sys/stat.h>`, which imports the Darwin **Clang module**, and on macOS that module includes
   `curses.h`, which declares `int echo(void)` and `int filter(void)`. Both are `studio.h` public
   API. iOS never hit it: there is no curses in the iOS SDK. Renaming ours is not an option (it is
   in hundreds of carts), so the spec sets **`OTHER_CFLAGS: [-fno-modules]`** — textual includes
   never reach `curses.h`. C only; Swift keeps modules because it needs them
   (`CLANG_ENABLE_MODULES: NO` breaks Swift's AVFoundation import).
2. **`UnsafeMutableAudioBufferListPointer` needs an explicit `import CoreAudio` under Catalyst.**
   `import AVFoundation` re-exports it on iOS but not on Catalyst. Added to `AU/TinyjamAU.swift` and
   `Sources/AudioEngine.swift`; both still type-check against the **iOS** SDK, so the iOS build is
   unaffected. Found by grepping both SDKs' `.swiftinterface` files rather than guessing an import
   per two-minute build — worth copying as a technique.
3. **Don't borrow the iOS staging dirs.** Using `gen/app` gave `ld: 3 duplicate symbols`, because it
   legitimately holds a MULTI-cart staging (`app_main.c` + per-cart TUs) that a lone `cart.c`
   collides with — and it is shared with other agents' builds. The Mac spec stages into its own
   `gen/mac` + `gen/macau`.
4. **zsh aborts on an empty glob.** `rm -f "$d"/*.c` killed the script on the first run, with the
   directory freshly created; `find -delete` is shell-agnostic. (The `zsh` word-splitting note in
   CLAUDE.md's "Key things to know" has the same root cause: this repo's scripts are run with zsh.)

### Still ahead, in order

| | |
|---|---|
| **register it** | sign properly (above), then `auval -v aumu tacj Mpla` — Apple's own validator, which exercises the plug-in harder than a DAW and is the gate before GarageBand is ever opened |
| ~~**host transport**~~ | ✅ **DONE 2026-08-12.** The render block reads `musicalContextBlock` + `transportStateBlock` → `de_sync_position()` → `sync.h`. **The cart needed ZERO changes** — acidcandy already derived its step counter from `sync_beats()` and surrendered its transport on `sync_transport()`, from the MIDI-clock work; a host is just the ABSOLUTE half of the same seam, so there is nothing to measure. Two traps: capture `Unmanaged.passUnretained(self)` rather than the host blocks (the host assigns them AFTER fetching the render block, so capturing them gets nil forever), and handle a host that supplies neither (push nothing, free-run — the path `auval` exercises). **Gated** by `ios/au-transport-check.swift` (run by `mac.sh`): a ~130-line AUv3 HOST that sets both blocks, advances its fake playhead from the RENDERED SAMPLE COUNT (so it is deterministic), and asserts the rack plays while moving · fires the same notes over the same 8 BEATS at 90 and 180 BPM · goes silent when the host stops. `--free` is the negative control (no blocks installed → ratio ~0.5 instead of ~0.87, and it keeps playing through the stop), because a gate that cannot fail is decoration. Two earlier onset detectors had to be thrown away first, both TEMPO-SENSITIVE and so unable to measure tempo: a fixed quiet-gate re-arm that the reverb tails defeated, then an adaptive threshold whose 57ms average never settled between notes at 180 BPM. The one that works compares the envelope against itself a FIXED 6ms earlier |
| ~~**the plug-in view**~~ | ✅ **WIRED 2026-08-12** — `AU/TinyjamAUViewController.swift` (an `AUViewController` that is also the `AUAudioUnitFactory`) hosting the same `Sources/CanvasView.swift` the app uses, via a new `hosted: true` mode. See "the plug-in view" below for the three seams it needed and the one thing still unverified |
| **multi-instance** | spike 9. **Worse on macOS than iOS**: `.loadInProcess` is macOS-only, so a Mac host may load several instances in ONE process, where they would share `studio.c`'s file-scope globals. iOS gets one-per-process for free |

### The sample rate: the plug-in was rate-blind, and now converts

**The engine is compile-time 44.1 kHz** (`SOUND_SAMPLE_RATE` in `sound.h` sizes delay lines and
envelopes; a frame is 735 samples). The standalone app gets away with it by feeding an
`AVAudioEngine` source node that converts to the device rate — **an AUv3 has no such buffer, the host
calls us at the host's rate**, and a host follows its audio INTERFACE, so 48k is common even though
GarageBand, Logic and Live all default to 44.1.

Three facts, and the middle one is where a first attempt went wrong:

| | |
|---|---|
| does the host actually move our bus? | **yes.** `AVAudioEngine` connected at 48 kHz and the plug-in's own output bus reports 48000. Nothing rejects it and nothing converts for us; `auval` renders us at 192k, 96k, 48k, 22050 and 11025 |
| what broke | **pitch, and every envelope / delay / LFO time**, by the rate ratio: an oscillator adding a fixed phase increment per sample runs `hostRate/44100` too fast. This is certain from the code, not from a measurement — see the retraction below |
| what did NOT break | **the sequencer.** All three transport checks PASS at 48 kHz (`--rate 48000`): the notes stay on the host's grid, because acidcandy derives its step from `sync_beats()` and a host states its playhead ABSOLUTELY. The rate never enters that path |

That split is why the fix is **a converter in the render block, not an engine refactor**: the defect
was confined to the sound, never to sync. The engine still runs at 44.1k in 735-sample frames (every
audio gate in the repo assumes it, and `demath.h`'s bit-determinism depends on it) and
**`ios/AU/RateConvert.swift`** converts on the way out: 4-point Catmull-Rom, plus a four-pole
anti-alias cascade at 0.45·host engaged only when the host is BELOW our rate. At exactly 44100 the
old code path runs untouched and bit-identical, so the common case pays nothing and every existing
gate keeps its meaning. Gate: **`ios/rate-convert-check.swift`** (in `mac.sh`), a 220 Hz sine through
the real struct — 220.000 Hz at every rate, level held, the 11025 case rejecting a 15 kHz tone
instead of folding it down to 3.9 kHz, and a nonsense rate falling back to passthrough rather than
hanging the audio thread in a pull loop that never terminates.

### The out-of-process wall: the open fork (2026-08-13)

⚠ **Read this before doing any more AUv3 view work.** The plug-in now plays in GarageBand and survives a
full song, but its panel is not honestly connected to its sound, and no amount of patching the current
design fixes that.

**What a live sample found** while the maker's session was wedged (`sample <pid> 2 -file out.txt`, and
the *thread names* are the tell):

| process | what it had | what it means |
|---|---|---|
| pid A | view work, **no** `AUOOPRenderingServer`, **three** `dreamengine.frame` threads | a UI-only process running THREE engines |
| pid B | view work **and** `AUOOPRenderingServer`, one worker | the process actually making sound |

GarageBand runs the UI and the audio in **different processes**, and our view controller created a
fresh `TinyjamAU` — `de_init`, globals, worker and all — every time a panel opened. So the panel being
clicked was drawn by an engine that was not the one you hear, and three of them were writing the same
file-scope globals. That is the bar-33 wedge, and it is why the rack's own play button did nothing.

**Fixed now:** booting is idempotent — one `de_init` and one shared frame worker per process
(`TinyjamAU.bootEngineOnce`). Two instances in one process share one rack: wrong for two tracks, but
honestly wrong instead of corrupt, and it is what the globals have always meant.

**Not fixed, and not fixable within this design:** a view that blits the engine's framebuffer requires
the view and the engine to be the SAME instance in the SAME process. AUv3 does not promise that, and
GarageBand does not provide it. "multi-instance, spike 9" was filed as a limitation on the assumption
that each instance gets its own process; that assumption is simply false.

**Four ways out, in rough order of cost.** This is a genuine fork and it wants a decision, not a
default:

1. **Ship the standalone app; park the plug-in.** The app works, is signed, and is the product
   `apps/tinyacidjam` describes. The AUv3 was always the ambitious end of the arc. Parking it
   deliberately is a legitimate answer, and the cheapest one.
2. **Per-instance engine state** — the honest engineering fix: `studio.c`/`sound.h` file-scope globals
   become a context struct threaded through the engine. Large, touches everything, and pays for itself
   beyond the plug-in (two tracks = two racks, and the umbrella-app `de_switch_cart` seam gets simpler).
3. **A parameter-bound UI** — expose the rack as AU parameters and rebuild the panel from controls the
   system proxies across processes. This is how every commercial AUv3 does it, and it means giving up
   the pixel canvas *in the plug-in* (the app keeps it).
4. **Ship pixels across the boundary** — keep the canvas, send the framebuffer out and input back over
   XPC/shared memory. Keeps the look; adds a realtime transport problem and a lot of new surface.

**Do not start 2, 3 or 4 without deciding which product the plug-in is for.** They are different
plug-ins.

#### ↩ UNPARKED 2026-08-13 (later the same day): the panel was never orphaned, and the diagnostic that said it was could not have said anything else

**The finding this whole fork rested on does not survive.** The panel is attached to the audio unit
that renders — measured, and now gated. What follows supersedes the ⛔ PARKED section below; that
section's *observations* are still accurate, its *conclusion* is not.

**1. The `AudioComponentBundle` / `factoryFunction` lead: RIGHT about the mechanism, and it hits a
platform wall. Implemented, REVERTED, and the wall is now named.** The AU's code was factored into a
real framework (`TinyjamAUKernel`) with both keys declared as Apple's samples declare them. Then
GarageBand refused to open the plug-in at all — orange (!), the failure mode the entitlements bug used
to produce — and said exactly why:

```
GarageBand: Error loading …/TinyjamAUKernel.framework/TinyjamAUKernel (107):
  dlopen(…): incompatible platform (have 'MacCatalyst', need 'macOS')
```

**So a host DOES dlopen the bundle those two keys name — the lead's mechanism was correct.** It fails
because our framework is a **Mac Catalyst** binary and GarageBand is a native **macOS** process, and a
native process cannot dlopen Catalyst code. That is the real wall, and it is a much sharper statement
than "the panel is orphaned":

> **In-process AUv3 loading requires the AU's code bundle to be native macOS. Ours is Catalyst,
> because every pixel of our UI is UIKit (`Sources/CanvasView.swift`) — the deliberate choice at the
> top of `project-mac.yml`. Those two facts are incompatible.** Getting in-process loading therefore
> costs an AppKit canvas view, i.e. exactly the "second host view to maintain forever" that Catalyst
> was chosen to avoid. That is a product decision, not a cleanup.

⚠ **And note how the first reading of this went wrong, because it is the session's second instance of
the same mistake.** A probe measured `.loadInProcess` and saw the engine answer from another pid,
before *and* after the framework — and that was written up as "in-process loading is denied, an
`.appex` is not dlopen-able, the lead is closed". Wrong: in-process *was attempted* and failed on the
platform mismatch, after which a well-behaved host silently falls back to out-of-process. The probe
watched the **outcome** (which pid answered) and never looked at the **attempt** (was there a dlopen
error). GarageBand simply doesn't fall back, so it was the only observer that showed the truth.
**When a mechanism is expected to engage and the outcome looks unchanged, look for the mechanism's
own error before concluding it cannot engage.**

**REVERTED** (`ios/project-mac.yml` is back to the appex compiling the AU directly): a packaging that
no DAW can load is not "correct packaging", whatever Apple's samples do. **The failure was invisible
to all six gates** — they are hosted by a native macOS binary that falls back silently, so every one
of them passed on a plug-in that GarageBand could not open. That is the sharpest gate lesson of the
stretch: *the gates covered the AU, and nothing covered whether a DAW can load it.*

**2. The measurement that closed both routes was a tautology.** The old `[tinyjam] PANEL …` line asked
the view controller's own local `TinyjamAU` for a message-channel nonce and compared the pid in the
reply against the view controller's own pid — then called a match *"talking to itself, still the wrong
engine"*. Those two pids are equal **by construction**: the channel is fetched from a local object, so
the call never leaves the process and can only ever report that process. **The "connected" branch was
unreachable.** `PANEL TALKING TO ITSELF` was not a reading, it was the only string the code could
print.

The parameter probe fails the same way in the other direction. `PARAM writing 0.75 from UI pid 98759`
→ `PARAM observed … in pid 98759` is *exactly what a correctly connected AUv3 does*: if the view
controller's AU is the rendering AU, and both are in the extension process, then of course the write is
observed there. Both rows of the PARKED table are equally consistent with a panel that works.

**3. What is actually true, measured with a diagnostic that can go red.** The extension now stamps
which instance last rendered audio (`TinyjamAU.audibilityReport` — a per-instance id plus a
process-global "rendered by", written on the audio thread through a plain pointer) and the view
controller reports a verdict against it, re-reading at +2s/+8s/+20s because a panel opens *before* a
stopped host renders anything. Run against our own host:

```
TinyjamMacAU[3901]  PANEL NO AUDIO HAS RENDERED IN THIS PROCESS …  · on open · 1 instance(s) · pid 3901
PanelProbe[3899]    HOST's audio unit → engine pid 3901
TinyjamMacAU[3901]  PANEL CONNECTED — this panel's own audio unit is the one being rendered · +2s · pid 3901
```

The view controller loads **in the same process as the host's audio unit**, and the instance it holds
is the one being rendered. This is AUv3 working as designed: the system uses our view controller as
the factory, so the AU it makes *is* the host's AU, and `requestViewController` hands back that same
controller.

Gated by **`ios/au-transport-check --panel`**, wired into `mac.sh` as the sixth gate. It reads the
extension's verdict back out of the unified log — the same line a person reads in Console — and
**requires both verdicts in one run**, which is its control: a run showing only `CONNECTED` would be a
reporter stuck on a branch, the precise failure being corrected here.

**4. ✅ CONFIRMED IN GARAGEBAND (the maker, 2026-08-13) — (A) is dead and (B) is now measured.**
One track, panel open, press play:

```
TinyjamMacAU[6127]  PANEL NO AUDIO HAS RENDERED IN THIS PROCESS …            · on open · 1 instance(s)
TinyjamMacAU[6127]  PANEL CONNECTED — this panel's own audio unit is the one being rendered · +8s
```

**The panel in GarageBand is attached to the audio unit that renders.** The out-of-process wall, as
characterised, does not exist; the whole four-way fork it created (park / per-instance state / a
parameter-bound UI / ship pixels over XPC) was answering a question that was never open. Nothing needs
to be built to connect the panel.

Then a SECOND track with the same plug-in — the "it goes super weird" case, reproduced and explained
in three lines:

```
TinyjamMacAU[6127]  PANEL CONNECTED through the shared per-process engine — instance 1 renders,
                    this panel holds 2 … · on open · 2 instance(s) in this process · pid 6127
TinyjamMacAU[6127]  PANEL CONNECTED — this panel's own audio unit is the one being rendered · +2s · 2 instance(s)
TinyjamMacAU[6127]  PANEL CONNECTED through the shared per-process engine — instance 1 renders … · +8s · 2 instance(s)
```

**Same pid.** Two tracks are two audio units in ONE extension process, and the "which instance
renders" stamp **flips between 1 and 2** across re-reads — both render blocks are running, so both are
pushing `de_sync_position` into the one process-global engine and both are signalling the one frame
worker. The rack is being driven twice per host buffer by two transports. That is the whole of the
"weird sound", stated as a mechanism.

It also closes off the cheap hope: there is no host behaviour that gives each track its own process, so
**per-instance engine state is the only real fix** — which is also what the platform expects, since an
AUv3 is designed to be instantiated many times and Apple's own samples keep DSP state in a per-instance
kernel object. (The scary figures this section originally carried — "~204 statics … acidcandy ~120" —
were never measured. See "the numbers, finally measured" below: 91 + 109 mutable vars with 34 non-zero
initialisers between them, and acidcandy has 20, not 120.) One much smaller mitigation is available and is a PRODUCT decision,
not a cleanup: **elect one instance to drive transport and the frame**, and the second track becomes a
second window onto the *same* rack — still wrong for two tracks, but coherent instead of garbled, and
an honest limitation a buyer can be told about.

#### ✅ AND PER-INSTANCE STATE IS CHEAP AFTER ALL — `tools/engine-dylib-spike` PASSES (2026-08-13)

Asked at the right scale ("imagine I want to make 20 audio apps") the answer changes: a hack costs you
20×, an engine fix costs 1×. So the refactor is what you want — and this section originally called it
UNLANDABLE, on the strength of numbers nobody had run. **That was wrong; see "the numbers, finally
measured" below. The struct refactor is THE route.** What follows is still worth having, because it is a
measured FALLBACK and its test harness is the refactor's oracle.

**dyld keys loaded images by FILE, not by symbol.** Two *copies* of one dylib are two images with two
data segments, so every file-scope static duplicates — all 146 in `sound.h`, all 58 in `studio.c`, and
every cart's own — with **zero changes to any of them**. The engine seam is already exactly the right
interface: `ios/Sources/engine.h`, ~20 C functions.

Measured on the REAL engine (`bash tools/engine-dylib-spike/run.sh`, `acidcandy` twice + `epiano`):

```
▸ two COPIES of one engine dylib, driven with different transport
  ✓ the two loads are distinct dyld images                     handles …3510 vs …39c0
  ✓ THE POINT: their frames DIFFER, so their state is independent
  ✓ each engine hears its own transport   A peak 0.0000 (host stopped) vs B peak 0.6128 (playing)
▸ NEGATIVE CONTROL: the same path twice (must SHARE state)
  ✓ dyld hands back the SAME image for one path                …3510 and …3510
  ✓ and therefore ONE engine: both reads are byte-identical
▸ BONUS: a SECOND CART in the same process
  ✓ the other cart runs alongside, drawing its own frame        64000 px (320x200)
RSS: 9 MB with two engines up (peak 15 MB)
```

The audio row is the strongest one: A was handed a **stopped** transport and B a **playing** one, and a
shared engine could not have produced both. The negative control matters as much — the same path twice
comes back byte-identical, reproducing today's defect on purpose, so "the frames differed" cannot be the
probe simply being unable to see sharing.

**The shipping shape: K pre-built, pre-signed copies in the bundle** (`engine1.dylib`…`engineK.dylib`),
each instance `dlopen`s the next free one, instance K+1 refuses politely instead of garbling. Shipping
the copies rather than copying at runtime removes the one serious unknown — whether a sandboxed,
hardened-runtime appex may `dlopen` a file it just wrote.

**Bonus worth noticing:** the second cart ran alongside the first at its own canvas size. `de_switch_cart`
is a config-log replay that swaps one cart at a time; this is two carts *alive simultaneously*, which is
a capability the app side has never had.

**Still not covered — each needs its own step before this is a plan, not a hope:**

| unknown | why it matters |
|---|---|
| ~~`dlopen` from inside the sandboxed `.appex`~~ | **✅ ANSWERED — YES. See below.** |
| the Swift-side frame worker | one `static` per process today (`TinyjamAU.worker`); needs one per instance |
| K CoreMIDI virtual sources | `midi_output.h` publishes by NAME; K instances would collide |
| K instances, one `cart.blob` | `save_bytes` per cart; `de_set_save_dir` already exists to scope it |

⚠ **And the spike found a break at HEAD on its way in:** `tools/build-nr.sh` no longer linked. Today's
`midi_output.h` put CoreMIDI in `studio.c` and — unlike `midi_input.h`, which gates its backend off
under `DE_NO_RAYLIB` because a portable host feeds MIDI *in* — output deliberately wants that same call
on macOS and iOS, so it is not gated and the script's "ZERO frameworks (only libc + libm)" link line
broke. Fixed (link `CoreMIDI` + `CoreFoundation`, header corrected). It sat red for hours because
nothing runs that script in CI, which is the same shape as the six gates that passed on a plug-in
GarageBand could not open: **the seams that only humans exercise are the ones that rot.**

#### ▶ THE ROUTE: a context struct — the numbers, finally measured (2026-08-13)

The refactor was called *unlandable* twice in this document on the strength of "~204 statics", a figure
nobody ran a command against. Run:

```
grep -E '^static [^(]*;$' <file> | grep -vE '\(|const'      # mutable file-scope vars (a LOWER bound)
```

| | mutable file-scope vars | of those, NON-ZERO initialisers |
|---|---|---|
| `runtime/sound.h` | 91 | **14** |
| `runtime/studio.c` | 109 | **20** |
| `acidcandy` | **20** (29 total, 9 const) | — |

**Three things that figure hid, each of which changes the decision:**

**1. The call sites do not change.** The C idiom is one struct plus `#define name (ctx->name)` per member,
so every existing reference keeps compiling untouched. "204 statics" reads as 204 rewrites; the actual
hand work is the **34 non-zero initialisers**, which become an init function. Everything else is zero or
NULL and comes free from a calloc.

**2. The determinism gates are the SAFETY NET, not a cost.** This document listed them as a reason NOT to
refactor. Backwards: a pure state move MUST produce byte-identical output, and this repo can prove it —
`tune-check --quiet`, the golden WAVs, `canvas-diff --bytecheck`, `spec.js`, `det-probes/run.sh`. A
semantic slip shows up as a changed sha. Very few refactors get an oracle this strong. **A non-identical
render is a bug in the refactor, never "close enough".**

**3. Carts come along for free if they use `de_state()`.** *"A zero-filled block of bytes that the engine
owns — put your whole cart state in it"*, already the documented idiom (`STATE {…}` / `S->x`, and it
survives a hot-reload). Put that block inside the engine context and a cart's state duplicates with it —
**one mechanism covers engine and cart.** The claim that 553 carts each needed treatment was doubly wrong:
only carts you actually host in an AUv3 matter, and acidcandy has 20 mutable statics, not 120.

**Order:** `sound.h` alone first (self-contained, strongest oracle, clean bail-out) → `studio.c` →
acidcandy's statics into `STATE` → thread the context through `engine.h` → the three per-instance items
that are NOT C globals (Swift frame worker, CoreMIDI source name, `cart.blob`) → gate with
`tools/engine-dylib-spike/probe.c`, whose assertions and negative control port unchanged once `dlopen` is
swapped for the context call.

**Risks:** `#define` collisions with same-named locals (mechanical to find, do it first), and scheduling —
these are the two files parallel agents share, so CLAUDE.md's hot-files rule applies in full.

**Why not the dylib route, which is also proven?** It is a workaround for an engine that should not be a
singleton: K is a hard cap, memory multiplies (~4.5 MB/engine), it rests on dyld image-dedup behaviour
rather than a documented contract, and it is unusual enough to draw a review question. Keep it as the
fallback — its spike and oracle are already written.

#### ✅ THE SANDBOX QUESTION IS ANSWERED — YES, and runtime copying is CLOSED (2026-08-13)

The killer unknown, measured in the real plug-in: a temporary dylib (`AU/sandboxprobe.c`, a counter)
embedded in the appex's `Frameworks/`, `dlopen`ed from `bootEngineOnce`, logged, then **removed again**
— a `dlopen` has no business in a shipping AU's boot path once it has answered.

```
[tinyjam] SBPROBE bundled dylib LOADED under the sandbox — counter 3 (expect 3)
```

**A sandboxed, hardened-runtime app extension CAN dlopen a signed dylib shipped in its own bundle.**
That is the route to per-instance engine state, and it is open.

**The other half is firmly closed, and for a better reason than failure.** The probe also copied itself
into the container and tried to load *that*:

```
[tinyjam] SBPROBE container copy dlopen REFUSED —
  code signature … not valid for use in process: library load disallowed by system policy
```

and — the part that actually decides it — macOS threw **user-visible Gatekeeper dialogs** at the maker:
*"sbprobe-copy.dylib can't be opened because Apple cannot check it for malicious software"*, then
*"is damaged and can't be opened. You should move it to the Bin."* A plug-in that pops a malware
warning is dead on arrival, whatever the API allows. **So: PRE-SHIP K signed copies in the bundle.**
Not as risk-avoidance — as the only shape that does not accuse the user's DAW of running malware.

⚠ **Two traps this run set, both the day's recurring shape.**

**(1) The probe's own verdict line was INVERTED, and it printed the wrong answer.** It compared the two
images' counters and reported `b == a` as *"SAME image"*. It is the opposite: `open()` calls the counter
three times, so **two independent images both return 3**, while one shared image returns 3 then 6. The
one run where the copy did load (after the maker allowed it in Privacy & Security) printed
`counter 3 … SAME image (unexpected)` when 3 was proof of exactly the independence being tested. Fixed
before removal. The lesson is the day's: *a probe's interpretation needs a control as much as its
measurement does* — a two-state readout where both states are plausible is a coin toss with a comment.

**(2) That allowed load must NOT be read as "runtime copying works".** It worked because a human
clicked Allow Anyway for one file on one machine. Nothing shipped can rely on that, and a green reading
obtained by granting a permission is not a measurement of the default. Recorded because it is precisely
the mistake made twice earlier today — reading an outcome without asking what produced it.

✅ **And that second one is now gated: `au-transport-check --loadable`, the FIRST check in `mac.sh`.**
It instantiates nothing — it reads what the extension DECLARES and checks the declaration is honest: if
`AudioComponentBundle` names a bundle other than the appex itself, that bundle must be dlopen-able by a
NATIVE macOS process, because that is precisely what a host does. No other mode in that file can see the
class, because they all instantiate through AVAudioUnit, **which silently falls back to out-of-process
when in-process loading fails**. It carries a control (Catalyst code must fail: *"wrong platform to load
into process"*) and was exercised RED against a hand-broken copy of the app — that red came back on
"code signature invalid" rather than the platform, since copying a binary out of a signed bundle breaks
its signature, so the gate demonstrably catches the SIGNATURE class too. 7 gates, 31 assertions, green.

Incidentally all three verdict branches have now been observed in the wild, which is the strongest form
of the control the `--panel` gate asserts synthetically.

**5. Wrong turn #12, and it is the generic one.** *A diagnostic whose other branch is unreachable is
not a diagnostic* — the same defect as a gate that cannot go red (`tools/gate-controls.js` exists for
this class), except a diagnostic gets *believed* rather than checked, and this one redirected a day of
work and got written into three documents as fact. The tell was available for free: the two numbers
being compared came from the same object.

**6. And a shell trap that cost the same conclusion twice in one afternoon.** `log` is a **zsh
builtin**. `log show --predicate …` from the Bash tool prints `log: too many arguments` on *stderr* and
nothing on stdout — so with stderr redirected it reads exactly like "the extension logged nothing",
which is what made the panel look like it never loaded. Always `/usr/bin/log`.

#### ⛔ superseded (see the section above) — PARKED 2026-08-13: BOTH routes measured, both closed AS CONFIGURED — and the wrong turns

**Outcome: the plug-in is parked.** It works as a sound source (plays, follows host tempo and
transport, stops, restarts, survives a loop, converts sample rates; five gates cover it). It does not
work as an *instrument you can play*: the panel is disconnected in both directions — it shows a
different engine than you hear, and touches on it drive that same wrong engine.

**What was measured, in GarageBand, which is the only place that counts.** Two mechanisms, one
result each, both from a clean restart:

| route | result |
|---|---|
| `AUMessageChannel` from the view controller's AU | `PANEL TALKING TO ITSELF — channel engine pid 98759 · this UI process pid 98759` |
| `AUParameterTree` (Apple's own supported route) | `PARAM writing 0.75 from UI pid 98759` → `PARAM observed … in pid 98759` |

Same pid both times. The view controller's `TinyjamAU` is an orphan: nothing it writes reaches the
rendering instance, and nothing the rendering instance holds is readable from it.

⚠ **Do NOT read that as "AUv3 cannot do this."** Commercial AUv3s have working out-of-process UIs
through exactly the parameter mechanism that just failed here, so the suspect is **our
configuration**, not the platform. The most likely candidate is that our view controller is both the
principal class and the factory (done deliberately — two factories would let a host instantiate the
one without a view), leaving the UI process with an AU nobody connected.

**How to resume, and it is deliberately NOT more instrumentation on our own code** — the last three
probes all told us the same thing:

1. **Diff against a KNOWN-WORKING AUv3** — Apple's `AUv3FilterDemo`, or [bradhowes/LPF](https://github.com/bradhowes/LPF).
   Run the same pid probe on *that* first, so "what a working one looks like" is established before
   changing ours.
2. **Test on iPad.** Everything above is Mac Catalyst, where audio runs in `AUOOPRenderingServer`.
   iOS has historically hosted the audio unit and the view in ONE process, which could make the whole
   problem vanish on the platform the product actually ships to. The `[tinyjam] PANEL …` line is
   permanent, so this is a ten-minute check.

**What is kept, and why.** `TinyjamCanvasChannel` (echo/nonce/frame) and `CanvasView.remoteFrame` stay
and are inert — `remoteFrame` is nil, so the view runs exactly the old path, and all five `mac.sh`
gates pass with them in. They earn their place as the DIAGNOSTIC: `[tinyjam] PANEL …` answers "is the
panel connected?" in one line of Console, in every case, where before it took `sample`ing a wedged
host and reading thread names. The probe PARAMETER was removed — a stray "Bridge Probe" shows in every
host's automation list, and this app is on the store.

##### ▶ THE LEAD TO RESUME ON: our extension may never be loadable IN-PROCESS

Found by diffing our extension declaration against [bradhowes/LPF](https://github.com/bradhowes/LPF)
(a working AUv3 the author tests in GarageBand and Logic on macOS). We match on the parts already
checked — `NSExtensionPointIdentifier: com.apple.AudioUnit-UI`, principal class = the view controller,
`sandboxSafe: true`. **Two keys differ:**

| key | LPF | ours (`ios/project-mac.yml`) |
|---|---|---|
| `AudioComponentBundle` | `com.braysoftware.SimplyLowPass.framework` — a **separate framework** holding the AU code | `$(PRODUCT_BUNDLE_IDENTIFIER)` — the extension itself |
| `factoryFunction` | declared, names the factory | **absent** |

Both keys exist so the system can **load the audio unit's code as a bundle into another process**,
which together with `sandboxSafe` is what permits **in-process instantiation**. In-process is exactly
the case where `createAudioUnit` runs once and `requestViewController` hands back a view controller
holding *that same instance*.

That would explain every symptom at once: with no loadable component bundle and no factory function
the system has no option but out-of-process, the UI extension separately instantiates our principal
class, and the view controller ends up with an AU that bridges nothing — not the message channel, not
the parameter tree. It also fits LPF's HOST code, which calls `requestViewController` and **never sets
an `audioUnit` property on the returned controller**, because with a correctly-declared component it
does not need to (`AUv3Support/Sources/AUv3Support/Host/AudioUnitLoader.swift`, `wireAudioUnit`).

⚠ **Confidence: a well-supported hypothesis, NOT a measured fact.** It explains all the evidence and
matches a known-working plug-in's structure — but four confident claims were wrong earlier the same
day (see the table below), so treat it as the next thing to TEST.

**The test:** factor the AU code into a framework, add `AudioComponentBundle` + `factoryFunction`,
rebuild, reload in GarageBand, read the one `[tinyjam] PANEL …` line. Ordinary build configuration
rather than research, and the diagnostic is already permanent.

##### The wrong turns, written down because they cost the afternoon

Kept in full: every one of these looked like progress at the time, and the pattern across them is
more useful than any single fix.

| # | what was believed | what was true | the lesson |
|---|---|---|---|
| 1 | "`au-transport-check` runs IN-process, so every AU gate tests the wrong topology" — stated confidently, put in a commit message | macOS loads AUv3 app extensions **out-of-process regardless** of the `options` flag. The comment in that file was accurate all along | Caught by the spike's own `--in-process` control. The control existed only because of an unrelated habit, and it caught its author within minutes |
| 2 | The channel returned empty replies because the channel object **was not retained** | Retaining changed nothing | A plausible mechanism is not a diagnosis. Cost a full rebuild cycle |
| 3 | — | The real cause: Swift imports `callAudioUnit` as an **optional ObjC protocol METHOD**. Declaring it as a stored closure property type-checks, compiles clean, and registers **no selector** | The host proxy then finds nothing and returns empty — indistinguishable from "no channel exists" |
| 4 | "The fork is decided — ship pixels, it's just plumbing" | The spike measured **HOST → audio-AU**. The panel needs **UI-extension → audio-AU**, a hop that does not exist | **A measurement can be perfectly sound and still be of the wrong thing.** The 0.32ms frame numbers are real and were answering a question nobody asked |
| 5 | Told the maker to compare the panel's nonce against a host-side spike run | The nonce is per **process**; two host sessions always differ, so "different" would have looked like proof no matter what was true | The check was fine and the PROCEDURE around it measured nothing |
| 6 | The panel diagnostic would report its own failure | It sat in `viewDidLoad` behind `if let au`, and GarageBand calls `createAudioUnit` **last** — so `au` was nil, the block was skipped, and the fallback log was **inside the same `if`**. No line at all in a real host | A diagnostic that can be silently skipped is not a diagnostic |
| 7 | A canvas frame is ~150KB (1 byte per palette index) | `de_copy_frame` publishes RGBA `UInt32` — **4 bytes/px**. This cart is 160×100 = 64KB | Wrong by 4×, and it happened to land in the safe direction rather than by judgement |
| 8 | `implementorValueProvider = { param in param.value }` | That getter **calls the provider** — infinite recursion. `auval` failed instantly with `OpenAComponent: result: 4099` | The gate caught it at the moment of introduction; otherwise the first symptom is "won't load in a DAW" |
| 9 | The first live-frame reply was a black picture | It was `0x0`, no pixels at all — the spike had never made the plug-in **render**, and `de_frame` runs from the render block | The message conflated "no frame" with "black frame" and sent the reader to the wrong end. Now three sentences for three failures |
| 10 | `swiftc -parse` proved the AU change compiled | `-parse` is syntax only. It accepted `CallAudioUnitBlock`, a type that does not exist in this SDK | `-typecheck` with the right target and SDK is the actual check |
| 11 | "0 errors" from `grep -c "error:"` on the build log | `mac.sh` pipes xcodebuild through `| tail -6`, so the diagnostics were already cut. Two rebuild cycles were spent blind | A count over a filtered log is not a count. (Also: `zsh mac.sh \| tail` makes `$?` the exit of `tail`, not the script) |

**The thread running through all of it:** eleven separate times, something green, plausible, or
confidently stated was not measuring what it claimed. Six were caught by a control, a negative case,
or a plausibility assertion — and the ones that were not caught (4, 5, 7) are the ones that cost the
most. That is the same lesson as
[`checks-and-oracles.md` → "The OTHER way a green check lies"](../guides/checks-and-oracles.md#the-other-way-a-green-check-lies-it-was-never-measuring-the-thing),
learned again at a different altitude: not just in assertions, but in procedures, messages, estimates
and build scripts.

#### the transport numbers, which remain true (`ios/au-msgchannel-spike.swift`)

Measured in GarageBand, which is the only place that counts:

```
[tinyjam] PANEL TALKING TO ITSELF (same process = still the wrong engine)
          — channel engine pid 96523 nonce 62414 · this UI process pid 96523
```

The channel taken from the view controller's own AU **loops back to the local instance**. The UI
extension constructs its own `TinyjamAU` (`TinyjamAUViewController.createAudioUnit`) and there is no
API to hand it the *rendering* one. Apple's model is that UI and DSP talk through the **parameter
tree** — which is exactly why every commercial AUv3 is built that way, and it is not a detail we can
route around.

**This overturns the "ship pixels" call made earlier the same day, and the failure is worth naming:
the spike measured the HOST → audio-AU hop and I read it as settling the fork.** The panel needs
UI-extension → audio-AU, a different hop that does not exist. The transport numbers below are real
and still true; they were simply answering a question nobody was asking. A measurement can be
perfectly sound and still be of the wrong thing.

It also explains the free-running playhead the maker noticed: the panel's own engine never receives
host transport, so it boots `playing = 1` and runs on its own while the engine you can hear sits
stopped. That symptom WAS the disconnect, visible without any tooling.

**What survives, and is kept:** `TinyjamCanvasChannel` (echo/nonce/frame) and
`CanvasView.remoteFrame` are inert — `remoteFrame` stays nil, so the view uses exactly the old path
and all five `mac.sh` gates pass with them in. They are kept for the DIAGNOSTIC: the
`[tinyjam] PANEL …` line now answers "is the panel connected?" in one line of Console, in every case
(connected · talking to itself · no channel · OS too old), where before it took sampling a wedged
host. If a future host or OS does bridge the two, the frame path is already written and measured.

**So the live fork is now:** park the plug-in (1) · a parameter-bound UI (3, the only supported
route, and it costs the pixel canvas in the plug-in) · or a hybrid that keeps the canvas by running
the UI-process engine and syncing it to the audio one through the parameter tree — which carries the
determinism-drift risk where the panel silently shows a *different* rack.

#### MEASURED 2026-08-13: the transport itself is fast (`ios/au-msgchannel-spike.swift`)

The cost that made option 4 read as "a lot of new surface" was guessed, so it got measured. An
`AUMessageChannel` round trip, **out-of-process** (`isLoadedInProcess` asserted false), 60 reps:

| payload | 64 B | 1 KB | 64 KB | **150 KB** (a full canvas frame) | 256 KB |
|---|---|---|---|---|---|
| rtt avg | 0.145 ms | 0.128 ms | 0.272 ms | **0.424 ms** | 0.668 ms |

A whole framebuffer crosses in **0.42 ms** — roughly 2300 fps against the 20–30 a panel needs. The
canvas can cross the boundary with ~100× headroom.

**It also settles the state-vs-pixels question against state.** Payload grows 2400× while latency
grows ~3×: the channel is dominated by fixed per-call overhead, not bandwidth. So mirroring the cart
in the UI process netplay-style (ship inputs, re-render locally) buys nothing measurable, and it
would buy a determinism contract across two processes where any drift silently draws the *wrong*
rack. **Ship pixels.**

Two things this does not settle: the numbers are an ECHO with no engine work behind them (real use
adds a `de_copy_frame()` on one side and a blit on the other, both of which already exist and are
TSan-gated), and it is Mac Catalyst — iPad is where the app lives. Also `AUMessageChannel` is
Catalyst/iOS **16+**, an OS floor on the plug-in, not on the app.

⚠ Three claims made confidently during that spike were wrong and are corrected in its header:
AUv3 extensions load out-of-process on macOS **regardless** of the `options` flag (so
`au-transport-check` was never running the wrong topology); the empty replies were not a retain bug;
the real cause is that Swift imports `callAudioUnit` as an optional ObjC **method**, so declaring it
as a stored closure property compiles clean and registers no selector.

### The first real play-test, and the two things it found (2026-08-13)

The maker put the plug-in on a track in GarageBand and played it. Two findings, and **the one that
looked most alarming was not a bug in our code at all**.

**1. A SAVED CART STATE CAN BOOT THE PLUG-IN SILENT, and there is no way to reset it.** After the
session, the plug-in rendered pure silence — every gate, every build, including builds that had
passed an hour earlier. That is what makes it worth writing down: it presents exactly like a code
regression, and it survives a rebuild, so a bisect chases ghosts. The cause is
`~/Library/Containers/com.tinyjam.mac.AU/Data/cart.blob`: acidcandy persists its pattern banks with
`save_bytes`, the plug-in has its own container, and whatever state the last session left is restored
on every launch. Move the blob aside and the rack plays again (0 onsets → 124, confirmed both ways).

The product consequence is bigger than the debugging one: **a user can put the rack into a silent
state and has no recourse.** No reset, no visible indication that a saved state is even in play.
Open work: a "reset rack" affordance, and/or a plug-in that does not persist to a shared container
without the host's state mechanism (`fullState`, which is how a DAW expects to save per-instance
settings — a plug-in with ONE global saved state is wrong for a host that has many instances anyway).

**2. The frame ran on the audio thread, which is a deadline we cannot meet.** The sample taken while
GarageBand was wedged shows the render thread inside `de_frame → draw → line → sw_pset →
blend_nearest`: the cart's entire software-rasterised UI, on the render thread. Measured on acidcandy:

| build | de_frame avg | peak | 512-buf (11.6 ms) | 128-buf (2.9 ms) | 64-buf (1.45 ms) |
|---|---|---|---|---|---|
| `-O2` | 0.13–0.25 ms | 0.83 ms | ok | ok | ok |
| `-O0` (what `mac.sh` shipped) | 0.40–0.94 ms | 2.08 ms | ok | ok | **MISS** |

So it fits a normal buffer and misses a small one. Two fixes: the frame moved to a **worker thread**
(the render block signals it and never waits; offline renders still run inline, where exactness beats
latency and there is no deadline), and `mac.sh` now builds **Release** — it built Debug until today,
which for a plug-in that software-rasterises a UI is a 3–4× tax on the hot path. The worker also
moves every allocation the frame can make (the framebuffer realloc on resize, the present buffer's
growth) off the audio thread, which was the last realtime sin in the render block.

⚠ **Not proven to be the hang.** The wedged transport was not reproducible from the sampled state:
both processes were idle and healthy by the time they were inspected, our view was blitting, our audio
was rendering. The deadline hazard is real and measured and now gone, but nobody has shown it caused
GarageBand's transport to stop. Treat that as open until a play-test says otherwise.

Two smaller things the same session surfaced: **hovering does not move the cursor** (the view feeds
`de_touch_*` from UIKit touches, and a Catalyst mouse-move is not a touch, so the cart's pixel cursor
only jumps on click — needs a hover seam), and **`build/` is shared between agents**, which
contaminated this very investigation twice (a `headless-nr` rebuilt for another cart made "the
software path is silent" look like an engine regression).

### The plug-in view (phase 3)

`AU/TinyjamAUViewController.swift`. Wired and gated 2026-08-12; **not yet judged by eye in a DAW**,
which is the honest remaining step — a gate can prove a host receives our view, not that the picture
is right.

**The plumbing that is easy to get wrong, because none of it fails loudly.** An audio-only AUv3
declares `NSExtensionPointIdentifier = com.apple.AudioUnit` with any NSObject factory. One with a view
must declare **`com.apple.AudioUnit-UI`** and hand the system a principal class that is *both* an
`AUViewController` and the `AUAudioUnitFactory`. Get it wrong and the plug-in still builds, still
registers, still passes `auval`, still plays — every DAW just quietly shows its own generic sliders,
which for a groovebox is the same as not working. So the view controller **replaces**
`TinyjamAUFactory` rather than sitting beside it: two factories would let a host instantiate through
the one with no view.

**The view never ticks the engine.** That settles the "tick-ownership split" this plan used to list as
open work: the render block keeps the frame, sample-clocked at one per 735 samples, because that is
what keeps the sequencer on the host's grid *and* correct through an offline bounce, where no view is
even on screen. `CanvasView(hosted: true)` therefore skips `de_init`, skips CoreAudio, skips
`de_frame`, and only blits. `AU_EXT` keeps the app's `AVAudioEngine`/mic plumbing out of the extension.

**Three engine seams it needed**, all in [`audio-threading.md`](audio-threading.md): the input event
ring, the deferred resize, and `de_copy_frame()`'s frame snapshot. The common cause is that a plug-in
inverts which thread is which, so every host call that used to touch engine state directly is now a
race — and two of the three were crashes rather than glitches.

**Two traps worth remembering**, both of which misdirected:

- `AVAudioUnit.instantiate` began "failing" the moment the extension gained a view, with a message
  blaming registration. A `-UI` extension does part of its loading on the **main queue**, and the
  checker was blocking the main thread on a semaphore. `auval`, which has a real run loop, loaded the
  same plug-in happily. Pump the run loop instead.
- `requestViewControllerWithCompletionHandler` is declared in **CoreAudioKit**, as a category on
  `AUAudioUnit` — not in AudioToolbox. Without that import the compiler just says `AUAudioUnit` "has
  no member", which reads like an OS-version problem. Found by grepping the SDK headers, the same
  technique that settled the `import CoreAudio` question under Catalyst.

Gate: `ios/au-transport-check --view` (run by `mac.sh`) — the host is handed an
`AUAudioUnitRemoteViewController`, and its view loads and reports a size (492×308).

#### Retracted: the "+147 cents at 48k" measurement

Worth keeping, because the failure is the generic one and it fooled a whole round of work: **a broken
analyser and the real defect print the same number.** The first gate measured pitch out of the running
plug-in, comparing the same 8 bars at 44.1k and 48k via per-window zero crossings of the low band. At
~50 Hz a 2048-sample window holds 4 or 5 crossings, so the estimate was quantized to a grid whose
positions are `crossings / 2 / windowSeconds` — and those positions move with the sample rate. Five
crossings in 46.4 ms reads 53.83 Hz; five in 42.7 ms reads 58.59 Hz; the ratio is `48000/44100`
exactly. So it printed "+147 cents, matching the predicted 146.6" and looked like a confirmed
diagnosis, when **any** signal would have produced that figure. It even carried an A/A null that
passed at 0 cents, which certified nothing: a same-rate null is blind to a rate-dependent estimator.

Two things caught it, both cheap and both skipped the first time round:

1. **A cross-rate control.** Dump a 44.1k render, convert it with a known-good converter
   (`afconvert -f WAVE -d LEI16@48000`), re-measure. Same music, same pitch, and the estimator moved
   53.83 → 46.88. It was reading the rate, not the instrument.
2. **A reference signal with an exact answer.** A cart with per-step drum probability and noise voices
   does not render the same audio twice (a same-rate A/A correlated at **0.045**), so it can never be
   the reference for a converter. A sine can. That is the whole reason the pitch oracle is a unit gate
   on `RateConvert` and not a statistic over the rack.

The independent end-to-end evidence that the fix landed is in the transport gate's own onset counts:
at 48k, **86 onsets before the fix vs 128 after**, against 124 at 44.1k, while the 44.1k numbers never
moved (124/108, byte-identical across both builds). Agreement went from 31% off to 3% off.

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
- **IAP in the multi-cart app — BUILT + sim-tested (2026-07-03).** `apps/tinyjam/app.json` carries an
  `iap.products` block (id/price/name/desc/`unlocks[]`) as the single source of truth; `build-app.js
  --ios` GENERATES `Tinyjam.storekit` from it AND threads each rack's productId + lock state (+ an
  `APP_MASTERPASS_*` "unlock all" offer) into `app_roster.h`. The launcher (`tinyjam-menu.c`) shows
  locked racks with a price, taps fire `Store_Purchase`, owned racks launch, and there's an "unlock
  all — $N" master-pass row — cross-platform via **weak `Store_*` stub definitions** (real on iOS,
  free/owned on Mac/editor so standalone carts are unaffected; `weak_import`/undefined-weak-*reference*
  does NOT link on the current Darwin ld — a weak *definition* does). Real prices still come from App
  Store Connect (ADR-0026); the manifest declares intent. **Gotchas hit, all real:**
  - **StoreKitTest is SIMULATOR-ONLY.** Linking `StoreKitTest.framework` into the app pulls in
    `XCTest.framework`, which isn't present in a plain app launch → dyld `SIGABRT` at startup (worse on
    device). Link it + the platform Developer-frameworks rpath ONLY for `[sdk=iphonesimulator*]`, and
    gate the Swift (`import StoreKitTest`, `SKTestSession`, the reset/testing `@_cdecl`s) behind `#if
    targetEnvironment(simulator)`. Device builds stay clean; device IAP testing = ASC **sandbox** (later).
  - **Local testing needs an in-app `SKTestSession`** (created at `Store_Init`), because a scheme's
    `storeKitConfiguration` only applies to Xcode's Run button, not `simctl`/`ios-deploy`. Set
    `session.disableDialogs = true` so a test buy completes instantly (no sheet) — a visible sheet
    passes the dismiss-tap through to the launcher row and re-buys ("popup won't close"); an in-flight
    guard in `purchase()` also stops multi-tap stacking while StoreKit loads.
  - **Product ids MUST be manifest-driven, never hardcoded.** `Store.swift` had a stale hardcoded
    `ids = [rebirth, funk, masterpass]`; adding `epiano` to the manifest → its product never loaded →
    `purchase()` found no product → the rack couldn't unlock (silent). Fix: `Store.configuredIDs()`
    reads `productID`s from the bundled generated `Tinyjam.storekit`, so any manifest product just works.
  - **A rack joining a bundle must match the app's dims** (build-app.js enforces one size — next-spike
    #3). omnichord/epiano are 320×200; adding epiano needed a `.cart.js` bumping it to 320×240 (which
    also changes the *standalone* cart). The real fix is the deferred multi-resolution support.
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
