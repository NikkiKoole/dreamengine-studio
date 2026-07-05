# Exporting a cart — the five ways out

> This is the how-to for what ships **today**. The wider picture — the three sharing
> *channels*, what's missing per channel, the editor-share-panel idea — is mapped in
> [`../design/sharing-channels.md`](../design/sharing-channels.md).

One cart, five destinations. All of them are self-contained (sprites/font baked into the
binary/wasm), the desktop two boot into the Host/Join/Solo multiplayer lobby
(`-DDE_NET_LOBBY_DEFAULT`), and every one titles its window with the **cart's name**, not
"dreamengine" (baked in via `-DDE_WINDOW_TITLE` for exports, passed as `--title` for
editor runs).

| # | destination | button | you get |
|---|-------------|--------|---------|
| 1 | another Mac | 🍎 export mac | signed + notarized `.app`, zipped, send-ready |
| 2 | a Windows PC | export .exe | unsigned `.exe` |
| 3 | your iPhone | deploy to iPhone | signed dev build on the plugged-in device |
| 4 | the public site | 🚀 publish to site | a URL anyone can play |
| 5 | a browser tab (unpublished) | build for web | local wasm preview, LAN-reachable |

## 1 · Signed Mac app (🍎 export mac)

Compiles a release binary, then hands it to `tools/mac-app.sh`: `.app` bundle → codesign
(Developer ID + hardened runtime) → notarize (uploads to Apple, ~1–5 min, watch the
runtime log) → staple → saves a zip of the `.app`. The recipient unzips and
double-clicks — no Gatekeeper warning. Every app gets the shared dreamengine icon
(`tools/assets/mac-app.icns`; regenerate with `tools/assets/make-mac-icon.js`, override
per-app with `mac-app.sh --icon`).

Degrades gracefully, checked at export time: notary creds missing → signs only
(recipient right-clicks → Open once); Developer ID cert missing → bare unsigned binary +
a setup hint. **One-time cert + notary setup: `tools/mac-app.sh`'s header.** macOS host
only. CLI twin: `tools/mac-app.sh <binary>` (works on any exported/`build/` binary).

## 2 · Windows .exe (export .exe)

MinGW cross-compile (`brew install mingw-w64` + raylib-win64), `-mwindows` so no console
window. **Unsigned** — recipient clicks "More info → Run anyway" once (Windows
code-signing is a possible later step; needs a paid cert, no free Apple-style dev path).

## 3 · iPhone test build (deploy to iPhone)

Builds + signs the *live editor buffer* for a plugged-in, unlocked iPhone and launches
it (`ios/device.sh` with `EDITOR=1`; ~90s — a deploy button, not a hot-run). One-time
setup (signing cert, ios-deploy, trust the computer): `ios/device.sh`'s header. This is
a test flow, not an App Store export (that ladder: `docs/design/ios-plan.md`).

## 4 · Publish to the site (🚀 publish to site)

The shareable-URL flow: builds wasm into `site/<name>/`, writes the source back to
`tools/carts/<name>.c`, commits + pushes; GitHub Pages serves
<https://nikkikoole.github.io/dreamengine/>. The button is gated behind settings → the
publish toggle. Batch/CLI: `tools/publish-cart.sh <name>`, `node tools/publish-all.js`
(smart batch), `node tools/cart-status.js` (what's stale/unpublished). Full story +
options not taken: [`sharing.md`](sharing.md).

## 5 · Local web preview (build for web)

emcc-compiles to `build/cart.{html,js,wasm}` and serves it from the editor on a local
port bound to `0.0.0.0` — so a phone/iPad on the same LAN can open it too (that's the
multitouch test loop). Nothing is published. Needs `brew install emscripten`;
`runtime/raylib-web/` is committed, so nothing else to install.

### Web-specific behaviour

- **"click to start" screen** — the click satisfies Chrome's autoplay restriction;
  `InitAudioDevice()`, `sound_init()`, and `init()` all run inside the click handler so the
  AudioContext is created after a real user gesture. Sound works fully after clicking.
- **`pget()` works on web** (opt-in via `enable_pget(true)`, off by default) — on web, studio.c
  does its own `glReadPixels` on the canvas FBO instead of `LoadImageFromTexture` (which runs an
  ES3-only format probe that spams a cosmetic WebGL1 `INVALID_ENUM`). Validated desktop Chrome +
  iOS Safari. Details: [`../design/mobile-web-notes.md`](../design/mobile-web-notes.md) §5c.
- **`save()`/`load()` don't persist** — emscripten's virtual filesystem is in-memory only; data
  is lost on page reload. Fix later with localStorage.
- **ScriptProcessorNode deprecation warning** — cosmetic, harmless. Raylib uses miniaudio which
  hasn't switched to AudioWorklet yet. Sound still works.

### emcc flags and why

```
-s USE_GLFW=3              # GLFW canvas input (required for Raylib)
-s TOTAL_MEMORY=67108864   # fixed 64MB heap — ALLOW_MEMORY_GROWTH invalidates
                           # the HEAPF32 TypedArray view used by the audio callback
-s EXPORTED_RUNTIME_METHODS=ccall,HEAPF32
                           # emscripten 5.x no longer exports these on Module by
                           # default; miniaudio's JS onaudioprocess uses both
```

### How `runtime/raylib-web/` was built (from source, NOT the release zip)

The pre-compiled `raylib-5.5_webassembly.zip` from GitHub was built with an old emscripten and
ships miniaudio 0.11.21 (broken ScriptProcessorNode), so the committed lib was built from source:

```bash
git clone https://github.com/raysan5/raylib.git --branch 5.5 --depth 1 /tmp/raylib-src
cd /tmp/raylib-src
emcmake cmake -S . -B build-web -DPLATFORM=Web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j4
# outputs: build-web/raylib/libraylib.a → committed at runtime/raylib-web/lib/libraylib.a
```

WASM bitcode is architecture-independent so the same file works on any machine with emscripten.
If you ever need to rebuild it (e.g. a new emscripten major version), run the above and replace
the file.
