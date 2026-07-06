# Handoff / working notes

> Portable context for picking dreamengine up on another machine or in a fresh
> session. This is the stuff that isn't obvious from the code or git log. Keep it
> short; prune what goes stale.
>
> **For "what's shipped vs. open vs. cut" see [`STATUS.md`](STATUS.md)** — the single
> status ledger. This file is the running narrative + environment gotchas.

**How this file stays useful (the system).** The `## Where we are right now` section is
**ACTIVE LANES only** — one dated `▶ ACTIVE THREAD` callout per complex in-flight effort, each with
(1) what shipped, (2) a **Resume-at** pointer to the owning doc's pick-up point, (3) any hot files
to avoid colliding on. Rules that keep it honest: **refresh your lane's date whenever you touch it;
prune a lane the moment it ships or goes quiet** (its detail already lives in `STATUS.md` + the
doc's pick-up point — don't duplicate, point). A lane dated **>2 weeks** old is presumed stale —
verify or prune. Everything below the lanes is history; trust `STATUS.md` + the design board over it.
**Tooling keeps this honest** (`tools/handoff.js`, the driftable two-door pattern): `node
tools/handoff.js` lists the active lanes + age (and it's the first thing `orient` prints — the
front door); `node tools/handoff.js --check` flags a lane >2wk old or with a broken link (surfaced
by `cart-status.js` — the back door). So a forgotten stale lane *surfaces* instead of rotting.

_Last updated: 2026-07-06 (four lanes: multiplayer — rung 5b WebRTC P2P SPIKED (Mac↔iPhone ~12ms direct over wifi), implementation not started · device-adaptive-layout — resizable-app PLUMBING landed (Tiny Jam fills the device: K=2 pixel-chunk + safe-area + reflow-aware menu, be7b2cad); next = the HTML wireframe of the 3 shapes → R5 acidrack redesign · store/ASO — buy-screen crash FIXED (Store.unlockedIDs race, 07690c9b) + Tiny Jam name reserved on ASC + testflight.sh · editor media — record/replay shipped, the panel deferred)_

---

## Where we are right now

**Four lanes are active in parallel right now** (different areas — pick the one you're resuming):
(1) **multiplayer — WebRTC P2P (rung 5b)**, (2) **device-adaptive layout**, (3) **store / ASO +
the app-trailer builder**, and (4) **editor media (record/replay + where it lands)**. All below;
none is "the" thread. Shipped/open ledger for all: [`STATUS.md`](STATUS.md) + the design board.

> **▶ ACTIVE THREAD (2026-07-06) — multiplayer: WebRTC P2P (rung 5b).** Kicked off by
> the maker play-testing rung 5a with his son (Mac ↔ Windows, both browsers): it
> "worked" but ran ~3 fps with 0.3 fps freezes. **Diagnosis:** the published
> gallery signals through the **Render relay**, so even on one wifi every input
> tromboned out to the internet and back (~330 ms RTT) — and fixed-delay lockstep
> (`NET_DELAY=3`, ~50 ms) collapses to one sim-step per round-trip once the cushion
> drains; the 0.3 fps freezes were TCP head-of-line blocking through the WS relay.
> A github.io-hosted gallery **structurally can't** be LAN-fast with a relay (static
> host + https ⇒ needs a public `wss://` box). **The fix is WebRTC P2P.**
> **SPIKED + PROVEN tonight:** `tools/webrtc-spike/index.html` (committed, reusable
> connectivity probe; loopback mode + relay mode) opened a real `RTCDataChannel`
> **Mac ↔ iPhone/Safari across the home wifi at ~12 ms** (vs ~330 ms via Render — a
> ~25× win), signaling through the **existing `net-relay.js` unchanged**. Two
> handshake potholes found + fixed, both carrying into the real design: (1)
> offer-before-peer race → **joiner announces first** (mirrors the WS `HELLO`); (2)
> the relay re-frames all forwards as **binary** (`wsEncode` opcode 2) → send
> signaling as binary, distinguish from `ROLE` by the `DN` magic. Bonus proven:
> DataChannel `{ordered:false,maxRetransmits:0}` (UDP-like) kills the TCP-freeze
> mode; Safari-on-iOS connects even over http. **Key insight:** browser↔browser
> needs **NO `libdatachannel`** (browsers have WebRTC built in) — it's a thin
> `EM_JS` shim parallel to the shipped `de_ws_*`, plugged into the
> `net_transport_send`/`pump` seam as the 3rd arm. **Resume-at:**
> [`design/multiplayer-research.md`](design/multiplayer-research.md) §"Scoped plan —
> rung 5b" — the 7-step table (start: step 1 `de_rtc_*` shim + step 2 signaling).
> Implementation NOT started; the spike settled the unknowns. Measured jitter (12 ms
> base, 70 ms phone-wifi spikes > the 50 ms fixed cushion) is the case for **adaptive
> `NET_DELAY`** (step 5). Hot file when building: `runtime/net.h` (targeted `Edit`s,
> shared). Gate: `node tools/net-check.js`.

> **▶ ACTIVE THREAD (2026-07-06) — device-adaptive layout.** Make `SCREEN_W/H`
> live-resizable + physically-sized so one cart reflows beautifully to iPhone AND
> iPad. **Phase 0 done** (model proven in cart-land — `respond`/`rackfit`/`acidfit`/
> `otafit`; `runtime/lay.h` **shipped**) and **Phase 1 (a+b+c) DONE (2026-07-03)** — the
> engine reflows live on desktop: `de_sw`/`de_sh` runtime globals, a per-cart
> `-DDE_RESIZABLE` opt-in (`de_reflow`), the cart API `screen_w()`/`screen_h()`, and `respond.c`
> flipped + verified reflowing. **Plus a GROWABLE framebuffer (B, 2026-07-04):** `sw_cbuf` + GPU
> `canvas`/`canvas_snap`/smooth are heap + grow-only (`de_ensure_fb`/`de_grow_gpu`/`de_set_canvas`,
> stride = runtime `fb_w`, cap `DE_MAX_DIM` 4096) — a resizable cart fills **any** window; the earlier
> side-bands (enlarge) + top-pinned drift (narrow) were the old fixed-max ceiling, now gone. **Editor
> ▶-run** passes `-DDE_RESIZABLE` from `de:meta "resizable": true`; **`play.js`** does too + a
> **`--resize "WxH,…"` sweep** (scripted resize→look filmstrip). Byte-identical for fixed carts
> (SHA-verified on `drawall`, all 465 compile). **Plus Phase 2 iOS FILL DONE (2026-07-04):** seam
> `de_resize`/`de_is_resizable` (`platform.h`/`ios/Sources/engine.h`/`studio.c`) + `CanvasView.swift`
> reads dims LIVE and calls `de_resize(bounds points)` from `layoutSubviews` for a resizable cart →
> `respond` fills iPhone SE / 15 / iPad Pro 12.9 on the sim (build: `RESIZABLE=1 CART=respond DEVICE=…
> ./build.sh`). **Safe-area + rotation DONE too:** `de_set_safe_area`/`safe_rect()` (controls dodge the
> notch), landscape allowed in Info.plist + `layoutSubviews`→`de_resize` reflows on rotate (both
> confirmed on the sim). So **Phase 2 is fully done — the engine foundation (fill/safe-area/rotation/
> growable fb) carries device-adaptive layout.** **GOTCHA:** `build-all` misses the DE_NO_RAYLIB path —
> run `build-nr.sh` after touching studio.c (the `--resize`/overlay work broke iOS via missing
> `raylib_compat` stubs; fixed). **Phase 3 RE-PLANNED (2026-07-05):** the first acidrack pass reflowed
> without redesigning (hand-rolled px math, no `lay.h`/finger unit/disclosure, no iPad arrangement) —
> caught by the maker's device test; retrospective = field note
> [018](field-notes/018-passing-the-gates-felt-like-done.md). **Next agent: the revised plan** —
> §"Phase 3 — revised plan" in [`design/device-adaptive-layout.md`](design/device-adaptive-layout.md):
> per-rack layout brief (R1) → `runtime/disclose.h` (R2, the keystone — three-state strips
> folded/compact/expanded from the ReBirth study) → `finger_px()` (R3) → ui-audit judgment layer (R4)
> → re-land acidrack (R5). **The acidrack brief is STARTED:
> [`design/acidrack-layout-brief.md`](design/acidrack-layout-brief.md) — resume THERE; its §2
> compact-strip taste calls (which controls earn the middle state per machine) wait on the maker,
> everything downstream (footprints §5 → disclose.h → re-land) follows from that table.** Hot files:
> `tools/carts/acidrack.c`, `runtime/lay.h` (+ new `runtime/disclose.h`). Ledger: [`STATUS.md`](STATUS.md) #2.
> **Update 2026-07-06 — resizable-app PLUMBING landed (commit `be7b2cad`), before R5's redesign.** The
> Tiny Jam *app* now reflows to fill the device on the sim (was 320×240 letterboxed — resizable only
> existed for single-cart builds): `RESIZABLE=1` on `ios/build.sh`'s `APP=` path, the launcher menu
> made reflow-aware (`safe_rect()` + centered column), the app home-chip moved inside the safe area
> (was stuck under the notch → couldn't reach the overview), acidrack's transport + chain row inset by
> `safe_rect()`. **Device matrix committed** as the design baseline (`design/acidrack-device-matrix.png`
> + regen recipe in the brief §7). Three findings, all written up in
> [`design/device-adaptive-layout.md`](design/device-adaptive-layout.md) §"2026-07-06": (1) **pixel
> chunk K** (`CanvasView.swift pixelChunk`, =2) — reflow to `points/K` logical px or you get hi-res
> tiny pixels + sub-finger controls; K=3 overflows the font; (2) `de_reflow` is **binary-wide** so
> yachtrack/epiano render in the top band (per-cart reflow = backlog); (3) **SEAM (backed out):**
> desktop live-resize freezes the transport — macOS modal loop blocks the main thread, GLFW fires no
> callback, do NOT re-try the callback route; iOS rotation is fine. **Resume at: the interactive HTML
> WIREFRAME of the 3 shapes (tall/short-wide/roomy at K=2 logical sizes) with folded/compact/expanded
> toggles** — the vehicle to make §2's compact-strip taste calls before touching acidrack's C (maker's
> call), then R5. Parked decisions: landscape side-notch inset (acidrack insets top/bottom only) +
> background-audio policy (keep-playing vs pause-on-Home).

> **▶ ACTIVE THREAD (2026-07-06) — store / ASO + the app-trailer builder.**
> **Buy-screen crash FIXED (2026-07-06, commit `07690c9b`):** the "instant, random" abort on the
> Tiny Jam menu/purchase screen was a **data race** — `Store.unlockedIDs` (a Swift `Set`) read by the
> C entitlement gate every frame while a StoreKit `Task` reassigned it → nano-heap corruption surfacing
> later at an unrelated `malloc`. Never reproduced off-device (desktop stubs `Store_*`). Now
> `NSLock`-guarded. Full lesson in `ios/README.md` §Gotchas — any per-frame `@_cdecl` bridge must be a
> lock-guarded snapshot, never a bare Swift collection.
> **Store-identity day (2026-07-06), all committed:** the App Store name **"Tiny Jam: Pocket
> Music Toys" is RESERVED** on App Store Connect (record created, not public); shipping bundle id
> is **`com.mipolai.tinyjam`** (registered in the dev portal; `apps/tinyjam/app.json` updated —
> the `com.tinyjam.hello` in `ios/project.yml` is dev-loop-only, see the comment there); the
> manifest **`icon` key is live** (`build-app.js --ios` → single-size asset catalog, sim-verified
> in `Assets.car`); **`ios/testflight.sh` RAN TO COMPLETION (2026-07-06)** on the upgraded box
> (macOS 26.5 + Xcode 26.6 at `/Applications/Xcode26_6.app`): **v0.1 build 202607061929 uploaded
> to App Store Connect** (cloud-signed Release, name reservation cemented) — next store step is
> ASC → TestFlight once it clears Processing. Toolchain wobbles found + fixed on the way:
> (1) `open -a Simulator` launches the STALE Xcode 15.1 copy in ~/Downloads and dyld-crashes —
> open Xcode26_6's Simulator.app by path; (2) the **iOS 26 sim runtime killed in-app
> `SKTestSession`** (needs a real XCTest run context now, not just XCTest loaded — dlopen tricks
> don't help; the 17.2 runtime was auto-deleted in the upgrade) — `Store.swift` now skips local
> IAP testing gracefully (gated to iOS 26+); **the sim purchase dev-loop lives on an 18.x
> runtime device** — iOS 18.4 runtime installed + `DEVICE="iPhone 16 (18.4)" ./build.sh`
> VERIFIED purchases working (2026-07-06). Device IAP testing still waits on ASC IAP records
> (Monetization → In-App Purchases; the bundled .storekit only covers the sim).
> (A separate lane from the one above.) A big session. Shipped, all committed to `master`
> (local — **push to sync other machines**):
> - **The free ASO keyword loop** (CLI + Apps tab): `aso-research` (now mines competitor
>   *descriptions*, doc-frequency ranked) · `aso-suggest` (free Google-autocomplete demand proxy) ·
>   `aso-compose` · `aso-lint` · **`aso-brief`** (palette — a committed, drift-tracked
>   `seo-brief.md`) · **`aso-coverage`** (mirror — coverage + stuffing) · **`aso-score`** (terminal
>   scoreboard + A/B tweak, `--deep` = winnability). Loop: research/suggest → brief → *you write* →
>   coverage → compose/lint/score; **no step writes prose**.
> - **Apps-view surface:** the sell row (📝🔎💡🧩🔬📊✅🪞) + IAP copy (char badges) + clickable
>   keyword "keys" + all-keys→research + load-into-all-tools.
> - **Strategy reframe:** [`design/demand-generation.md`](design/demand-generation.md) — capture
>   (ASO, the tail) vs generation (video/tribe, the wave); grab a **tribe**, not the world.
> - **The trailer builder** ([`design/trailer-builder.md`](design/trailer-builder.md)): backbone
>   `tools/build-app-reel.js` (manifest carts → one reel; Tiny Jam = 3-rack) **+ editor v1 (A)** —
>   the Apps-card **🎞 trailer** section, a **non-destructive** click-to-edit timeline (library, ◀▶
>   reorder, transition-at-join, Build → bake+compose → preview; edits only the `.reel`).
>
> **Resume at:** the trailer's **trim + speed** slice (a `compose-clips` `setpts`/`trim` bump +
> `.reel` line syntax + block fields — fixes "the 34s reel is too long"), then the maker-gated store
> track (submission gates → ASC upload tool). **Full snapshot + next:** the pick-up point in
> [`design/store-agents.md`](design/store-agents.md). Orient: `node tools/topic-brief.js "aso"
> "trailer" "demand"`.
> **Editor note:** this lane changed `editor/electron/main.cjs` + `preload.cjs` (new IPCs:
> aso-score, app-clips, build-reel, app-seeds, aso-suggest/brief/coverage) — **restart Electron
> (`make`) to light them up**; `shell.js`/CSS/`index.html` hot-reload. All CLI tools work now.

> **▶ ACTIVE THREAD (2026-07-05) — editor media (record / replay + where it lands).**
> (Adjacent to the trailer lane above; shares its editor files.) **Shipped + committed
> (`a097e9df`):** the editor can **record** your play as a `.rec` input track (opt-in `● rec`
> button, settings → record → `tools/clips/<cart>/NN-take.rec`) and **replay** one (drop a `.rec`
> on the window → `studio:replay` runs it against the OPEN cart; warns if the take's folder names a
> different cart). Slug bug fixed on BOTH load paths (`eafbdc08`): a dropped `.cart.png` and the
> file-dialog "load cart" button now set `currentCartFile` from the FILE slug, not the display title
> (which mis-filed records into `squishy-lines/`).
> The `studio:replay` **plumbing is the durable piece**; the drop-anywhere trigger is a rootless
> stopgap (no media home exists yet).
> **Deliberately NOT built (maker's call, "not yet"):** the media home itself. Pulling that thread
> surfaced an IA seam, captured in
> [`design/editor-scopes-and-facets.md`](design/editor-scopes-and-facets.md): media is a **facet**
> at both **cart AND app** scope (the trailer builder is already the app-scoped half), not a new
> sibling of code/sprites/map. **Resume at:** that doc's three open questions (own per-cart panel vs
> a section in the share popover · how the app-scoped trailer twin relates · whether the editor
> grows an explicit cart|app scope switch) — plan against `share-panel.md`'s 🎬 make-clip button
> (the cart-media panel under another name) + [`design/cart-clips.md`](design/cart-clips.md)
> (storage layout). Hot files: `editor/electron/main.cjs` + `preload.cjs` + `src/shell.js` now ALSO
> carry record/replay — **shared with the trailer lane, and main/preload need an Electron restart.**

## History & reference (pruned 2026-07-05)

The old session narratives, shipped-feature bullets, and todo list that lived here are ledgered
elsewhere — trust those homes, not a handoff file:

- **Shipped / open / cut** → [`STATUS.md`](STATUS.md) + the design board (`design-board.html`).
  Backlog → the board's READY-TO-BUILD column (`node tools/design-board.js`); the old todo items
  all live on it or in [`design/api-notes.md`](design/api-notes.md).
- **Web build deep reference** (web-specific behaviour, emcc flags + why, how `runtime/raylib-web/`
  was built from source) → moved to [`guides/exporting.md`](guides/exporting.md) §5.
- **Cart format** (`.cart.png` zTXt chunks) → [`design/cart-metadata.md`](design/cart-metadata.md) +
  [`guides/cart-authoring.md`](guides/cart-authoring.md). Editor internals worth remembering: chunk
  helpers are `embedCartChunks`/`extractCartChunks`/`makeZtxtChunk`/`crc32` in
  `editor/electron/main.cjs`, duplicated standalone in `tools/make-cart.js`; the
  `preload.cjs` IPC surface is `studio.saveCart/loadCart/loadCartFile/loadCartBuffer/getFilePath`
  (Electron 32+ for the last); dropping a `.png` on the window loads it as a cart; `--screenshot`
  on a cart binary renders 3 frames and exits.
- **Cart authoring quick reference** → [`guides/cart-authoring.md`](guides/cart-authoring.md)
  (and CLAUDE.md's "Adding a cart" steps).
- Debug-tools design notes archive → [`archive/debug-printh-watch.md`](./archive/debug-printh-watch.md).

---

## Gotchas / environment facts

- **Display asleep = every play.js/make-cart run SEGFAULTS in `rlglInit`** (signal 11, empty
  trace) — Raylib needs a live GL context even `--headless`, so late-night unattended renders
  suddenly "break the engine" when the screen locks. It's not your edit. Fix:
  `caffeinate -du node tools/play.js …` (wakes + holds the display). Discovered mid
  filter-spike 2026-07-02 (audio-notes §25), took out a parallel agent's runs too.

- **`main.cjs` / `preload.cjs` changes need an Electron restart** (`npm start`);
  Vite hot-reloads everything else.
- **`▶ run` only works in Electron** (it spawns clang); the browser tab edits but can't run.
- Use **Node 22** (`nvm use 22`) before any npm command.
- **`--screenshot` mode** opens a real window briefly (Raylib needs a display).
  3 frames is enough for static carts; carts that randomise initial state on frame 1
  will look fine; carts that need user input obviously won't show gameplay.
- **arm64 integer divide-by-zero does NOT trap** (returns 0) — SIGFPE won't fire on
  Apple Silicon. Use a `volatile` null read for reliable test crashes.
- **`save()`/`load()`** write `cart.sav`/`cart.kv`/`cart.blob` into a per-cart folder:
  the editor and `play.js` pass `--save-dir saves/<cart>`, so saves live under
  `build/saves/<cart>/` (2026-06-04; previously all carts shared `build/cart.sav`).
- **`print_centered`/`print_right`** use `strlen(text) * 8` for width — the dos_8x8
  font is exactly 8px per character with 0 spacing (`DrawTextEx` size=8, spacing=0).
- **`rnd_float()`** uses `rand()` from `<stdlib.h>` (added to studio.c includes).
  All carts share the same `rand()` seed — not seeded per-cart; same sequence every run.
- **CLAUDE_CODE_TMPDIR fills up occasionally** — compile/emcc output gets lost when the
  tmp partition fills. Workaround: redirect to a real file and read it back —
  `> build/compile-test.log` for clang, `emcc ... >/tmp/emcc.log 2>&1` for web builds.
- **`trifill()` winding order** — Raylib's `DrawTriangle` needs counter-clockwise winding
  in Y-down screen coords. In Y-down space, cross product > 0 means clockwise visually
  (opposite of math convention), so swap when `cross > 0`.
- **`init()` fires after window + sprites are fully loaded** — safe to call `colorkey()`,
  `mset()`, etc. It does NOT run during `--screenshot` mode's 3-frame early exit, but
  that's fine since screenshot mode still calls it once before the loop.
- **Raylib auto-detected:** `/opt/homebrew/opt/raylib` (Apple Silicon) or
  `/usr/local/opt/raylib` (Intel). Both `main.cjs` and `tools/make-cart.js` do this.
- **Web build server stays alive** — `startWebServer()` in `main.cjs` keeps one
  Node HTTP server on port 8765 alive for the editor session. It reuses the same
  instance on subsequent builds (doesn't restart). If something is already on 8765
  externally, kill it: `kill $(lsof -ti:8765)`. emcc progress goes to the runtime log
  panel; errors to the build log panel.

## Working preferences observed

- **Respect day/night theming** — use CSS vars (`--bg`, `--bg2`, `--fg`, `--fg-dim`,
  `--border`, `--accent`, `--font`), never hardcode panel colors.
- **Panels auto-hide when empty** (build log 3s timer, runtime log on clean exit).
- **Optimize for beginner legibility** — visible mistakes are a first-class goal.
- Commits go **direct to `master`** (solo repo).
