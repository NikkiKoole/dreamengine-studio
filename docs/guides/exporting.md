# Exporting a cart — the five ways out

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
multitouch test loop). Nothing is published. Needs `brew install emscripten` +
`runtime/raylib-web/` (the raylib webassembly release unzipped there).
