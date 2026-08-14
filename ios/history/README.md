# iOS milestone screenshots — the visual changelog

Kept on purpose: one screenshot per spike/milestone as it first worked, so the journey from
"empty toolchain" to a shipping app is visible. When a spike lands, copy its simulator shot here as
`spikeN-<label>.png` (or `YYYY-MM-DD-<label>.png` for later app milestones) and add a row below.

Capture tip: screenshot **~1.5s after launch**, not immediately — `simctl io … screenshot` taken too
soon catches the iOS launch-zoom animation (washed-out, wallpaper showing through). See `../build.sh`.

| Shot | Date | What it first proved |
|---|---|---|
| `spike0-hello-toolchain.png` | 2026-06-29 | The agentic toolchain end-to-end: xcodegen → xcodebuild (no signing) → simulator boot/install/launch. A SwiftUI hello-world. |
| `spike1-canvas-loop.png` | 2026-06-29 | The software-canvas render path: C fills an RGBA framebuffer → CGImage → `CADisplayLink`-driven `de_update(t)` (the inverted loop) → crisp lo-fi pixels on screen. Pink ball animates; no GL/ANGLE. |
| `spike2-audio-vu.png` | 2026-06-29 | The audio path: a C synth fills an `AVAudioSourceNode` render callback (CoreAudio). The green VU bar (top) is driven by the audio thread's RMS — proof the callback is pulled. A C-pentatonic arpeggio plays. |
| `spike4-storekit-gate.png` | 2026-06-29 | StoreKit 2 in-house bridge: the top-right gate dot is driven by the C `Store_IsModuleUnlocked` → Swift `@_cdecl` round-trip. Green = Rebirth unlocked (a headless-test purchase persisted in the sim's StoreKit-test store). Headless XCTest (`xcodebuild test`) proves buy → entitlement, no account/network. |
| `spike3-save-launchcount.png` | 2026-06-29 | Save to the iOS Documents dir: the cyan squares (top-left) count app launches, persisted via `de_save_bytes`. Two squares = relaunched and the count survived. Headless XCTest proves a byte-exact save/load round-trip. |
| `spike5-appgroup.png` | 2026-06-29 | App Group entitlement sharing: the magenta dot (left of the green store gate) lights when the shared App Group suite reports unlocked racks — i.e. the Store mirrored its StoreKit entitlements into the container an AUv3 extension reads. Test proves app-writes/reader-sees. (Cyan squares ~8 = many launches by now.) |
| _(spike 7 — headless, no screenshot)_ | 2026-06-29 | AUv3 extension: the rack reuses the C synth and loads as a real Audio Unit. Proven by `AUHostTests` (our own minimal host finds it via `AVAudioUnitComponentManager`, instantiates + renders offline, peak 0.180) — no host app/device needed. Evidence is the test log, not a screen. |
| `spike8-omnichord.png` | 2026-06-29 | **Phase 2 — the REAL engine on iOS.** omnichord (the actual `studio.c`+`sound.h`, zero Raylib, via `-DDE_NO_RAYLIB`) renders pixel-correct + upright: the OMNICHORD chrome, rainbow strumplate, and full chord grid, all from the engine's software canvas blitted by `CanvasView` (bottom-up `sw_cbuf` flipped). CoreAudio pulls the real mixer (`de_audio_render`); UIKit touch → `de_touch_*` drives the strum. No more stand-in canvas/audio. |

## The spike stand-in sources

`canvas.{c,h}` and `audio.{c,h}` live here too — the stand-in software canvas and stand-in synth
that produced `spike1-canvas-loop.png` and `spike2-audio-vu.png` before the real engine replaced
them (`spike8-omnichord.png`). Kept for the same reason as the screenshots.

They moved out of `ios/Sources/` on 2026-08-14 because sitting there made them a hazard rather than
a record. Two targets excluded them BY NAME, so a future spec globbing `Sources` would have compiled
them silently — and `canvas.h` declares:

```c
const uint8_t* de_framebuffer(void);        // the spike
const uint32_t *de_framebuffer(DeInstance *in);   // runtime/platform.h — the real seam
```

Wrong arity *and* wrong return type. That is undefined behaviour no compiler can see across
translation units, and it is precisely how `face.h`'s stale `extern void de_resize(int, int)`
crashed with `in = 0xa7` (167 — the canvas width the cart had asked for). Nothing here is compiled
by anything; if you want to read how a spike worked, read it. Don't wire it back up.
