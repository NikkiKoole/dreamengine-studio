# Packaging & distribution — notes

> **Status: not started.** This is an exploratory scratchpad, not a plan of record.
> dreamengine has only ever run as a **dev build** — `npm start` launches Vite + an
> unpackaged Electron (`electron .`) pointed at `localhost:5173`, and the ▶ run button
> shells out to a developer's `clang` + Homebrew `libraylib.a`. Nothing here ships to a
> machine that isn't already a dev box. We're whipping carts, not releases — this doc
> exists so the gap is written down, not so we act on it now. State of any of this lands
> in [`STATUS.md`](../STATUS.md), not here.

_Last updated: 2026-06-02 (session 14 — written after wiring the dev-only app icon + name)._

---

## What we did (the dev-branding stopgap)

While prototyping, the running app showed up as a generic **Electron** icon and the name
**"Electron"** in the Dock and Cmd-Tab switcher. We fixed the *dev* experience only:

- **Icon** — `editor/electron/icon.png`, a solid `#ff6fb5` square (the run-button pink
  from `shell.css`). Applied at runtime via `app.dock.setIcon()` in `main.cjs`
  (macOS-only; `app.dock` is undefined elsewhere).
- **Name (internal)** — `app.setName('dreamengine')` near the top of `main.cjs`. This
  fixes the menu bar and the userData path, but **not** the Cmd-Tab / Dock *label*.
- **Name (Cmd-Tab / Dock label)** — macOS reads `CFBundleName` / `CFBundleDisplayName`
  from the Electron bundle's `Info.plist` *before* our JS runs, so `setName()` can't
  touch it. We patch the plist directly via `editor/scripts/dev-branding.cjs`.

**Why it's a stopgap, not a fix.** The plist lives in `node_modules/electron/dist/…`, so
`npm install` wipes it. The script self-heals this by running as a `postinstall` hook and
again at the top of `npm start` (also exposed as `npm run brand`). That's fine for a dev
machine and wrong for distribution — a shipped app gets its name and icon from the build
config and a real `.icns`, never from a node_modules patch. When we package for real, the
dev-branding script becomes redundant and the icon source-of-truth moves to the builder.

## What real packaging requires (none of this exists yet)

- **A bundler** — `electron-builder` or Electron Forge. Sets `productName`
  (→ proper Cmd-Tab/Dock name without the plist hack), bundles the renderer build
  (`vite build`, currently only used for the unused `build`/`preview` scripts), and emits
  a `.app` / `.dmg` (and `.exe`/AppImage if we ever go cross-platform).
- **A real icon set** — a `.icns` (macOS) / `.ico` (Windows) built from a multi-res
  source, not a single flat PNG. The pink square is a placeholder; a public release wants
  an actual mark.
- **Code signing + notarization (macOS)** — without an Apple Developer ID signature and
  notarization, Gatekeeper blocks the app on other people's machines ("damaged / can't be
  opened"). This is mandatory for any download anyone else runs, and it's the most
  annoying part (certs, entitlements, the notary upload step).
- **Load the bundled renderer, not `localhost:5173`** — `createWindow()` hardcodes
  `win.loadURL('http://localhost:5173')`. A packaged app has no Vite dev server; it must
  `loadFile()` the built `dist/`. This branch (`app.isPackaged`) doesn't exist yet.

## The real blocker: the run model needs a compiler

This is the part that makes dreamengine *not* a normal Electron app to ship. The ▶ run
button (and `make-cart.js`, and `play.js`) all do the same thing: write `cart.c`, then
**invoke the user's `clang`** to compile it against `studio.c` and link a developer's
Homebrew `libraylib.a` + macOS frameworks (see `main.cjs` and CLAUDE.md "How ▶ run
works"). A consumer who downloads a `.dmg` has none of that — no clang, no Homebrew, no
`raylib/include`. So "package the Electron app" is the *easy* 20%; the open question is
how a non-developer ever runs a cart. Rough options, unexplored:

1. **Bundle a toolchain** — ship a pinned clang + a prebuilt `libraylib.a` + the SDK
   headers inside the app, and point the compile command at those instead of system
   paths. Heaviest (hundreds of MB, signing implications for bundled binaries) but keeps
   the "edit C, hit run, native window" promise intact.
2. **Precompile only** — author/build carts on a dev box, ship *binaries* (or `.cart.png`
   + a prebuilt runtime). Loses the "anyone can edit and re-run" loop that's the whole
   point.
3. **Lean on the web/wasm path** — the emscripten "Build for web" output
   (`cart.html/js/wasm`, see [`guides/sharing.md`](../guides/sharing.md)) already runs a
   cart with no local toolchain, in a browser. The unsolved piece there is *editing +
   recompiling* in the browser (no clang in the page) and one-click hosted sharing
   (a *link*, not just a local build) — both listed Open in [`STATUS.md`](../STATUS.md).
   This is probably the most realistic public-consumption path and deserves its own note
   before anyone reaches for native packaging.

The iPad runtime ([`VISION.md`](../VISION.md), Open item 11) hits the same wall from a
different angle — touch is wired, but there's no build path, and iOS can't JIT/spawn a
compiler at all, so it's wasm-or-nothing there.

## Pointers

- Dev branding: `editor/electron/main.cjs` (`app.setName`, `app.dock.setIcon`),
  `editor/electron/icon.png`, `editor/scripts/dev-branding.cjs`, `editor/package.json`
  (`postinstall` / `brand` / `start`).
- Run model: CLAUDE.md "How ▶ run works"; `editor/electron/main.cjs`.
- Web path: [`guides/sharing.md`](../guides/sharing.md); browser URL-sharing is Open in
  [`STATUS.md`](../STATUS.md).
