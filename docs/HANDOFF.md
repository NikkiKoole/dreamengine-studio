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

_Last updated: 2026-07-05 (two lanes: device-adaptive-layout Phase 3 re-planned after the maker's device test · store/ASO + app-trailer toolkit)_

---

## Where we are right now

**Two lanes are active in parallel right now** (different areas — pick the one you're resuming):
(1) **device-adaptive layout** and (2) **store / ASO + the app-trailer builder**. Both are below;
neither is "the" thread. Shipped/open ledger for both: [`STATUS.md`](STATUS.md) + the design board.

> **▶ ACTIVE THREAD (2026-07-05) — device-adaptive layout.** Make `SCREEN_W/H`
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
> → re-land acidrack (R5). **Waiting on the maker: the acidrack compact-strip sketch** (which controls
> earn the middle state per machine). Hot files: `tools/carts/acidrack.c`, `runtime/lay.h` (+ new
> `runtime/disclose.h`). Ledger: [`STATUS.md`](STATUS.md) #2.

> **▶ ACTIVE THREAD (2026-07-03) — store / ASO + the app-trailer builder.**
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

> _(The session-8/9 narrative below is stale — kept for history. For current
> shipped/open state trust [`STATUS.md`](STATUS.md) + the design board, not this list.)_

> Recent sessions (8–9): the four-axis **instrument synth** shipped
> (`instrument`/`instrument_duty`/`instrument_lfo`/`instrument_filter`) plus `tritex`;
> the editor navbar was consolidated to **5 tabs** (code · pixels · carts · docs ·
> settings); and the **Docs tab now renders this `docs/` set in-app** (a Vite middleware
> serves `docs/` + a markdown viewer). Full shipped/open ledger: [`STATUS.md`](STATUS.md).

- **Debug tools shipped** (`printh` / `watch` / `watch_visible` / crash capture).
  Design notes archived at [`docs/archive/debug-printh-watch.md`](./archive/debug-printh-watch.md).
- **API expansion pass 1 shipped** — math, collision, animation, strings, timer.
  See [`design/api-notes.md`](design/api-notes.md) for what's done and what's still open (passes 2–3).
- **Cart format shipped** — `.cart.png` files embed source + sprites + map as `zTXt`
  PNG chunks. The visible image is a screenshot of the game (saved automatically on
  exit). See below for full details.
- **Tutorials page shipped** — 15 tutorial carts in `editor/public/carts/`, a gallery
  panel in the editor, and a cart authoring toolchain in `tools/`. See
  [`guides/cart-authoring.md`](guides/cart-authoring.md) for the full workflow.
- **API pass 2 shipped** — `follow()`, `save()`/`load()`, `noise()`/`noise2()`/`noise3()`,
  inline error markers in the CodeMirror editor.
- **API pass 3 shipped** — `ease_in/out/in_out`, `rnd_between`/`rnd_float`/`rnd_float_between`,
  `print_centered`/`print_right`, `frame()`. 14 tutorial carts total.
- **`tri()` / `trifill()` added** — triangle border and filled triangle.
- **`rect()` corner overshoot fixed** — was using `DrawRectangleLines` then `DrawLine` (both add line caps); now uses four 1px `DrawRectangle` slices. Pixel-perfect corners.
- **`init()` callback added** — called once after the window opens, before the first `update()`. Weak stub like `update()`. Replaces the `static bool` workaround for one-time setup (`colorkey`, map building, etc.).
- **`anim()` phase offset added** — signature is now `anim(n_frames, fps, phase)` where `phase` 0..1 shifts the cycle start. Use `(float)i/count` to stagger multiple entities.
- **Tutorial cart 15** — walk cycle with sprites, phase 0 vs staggered, uses `init()` + `colorkey()` + `anim()`.
- **Turtle graphics API + cart 16** — `turtle_home/move/turn/face/at`, `pen_down/up/color/size`. Spirograph tutorial cart.
- **Web build shipped** ✓ — "Build for web" button in the cart tab. Compiles with emscripten to `build/cart.html + cart.js + cart.wasm`, starts a local Node server on port 8765, opens in the default browser automatically. Sound works. See below for full details.

---

## Next on the todo list (by impact)

1. **Remaining pass 2 API** — `broadcast(msg_id)` / `received(msg_id)` event bus
   (touches main-loop drain semantics — small but architectural). Strudel extras:
   `pitch("c4")`, `sometimes`/`often`/`rarely`, `arp`, `stutter`, `palindrome`,
   `off_beat`. Dilla timing: `groove` + `groove_swing/jitter/push`.
2. **Browser sharing / hosting** — web build exists; next step is a URL you can send
   someone (itch.io upload, or a hosted service that stores the wasm).
3. **Sound tracker UI** — the sound tab is disabled; code-first sound works but a
   tracker would let you design sfx/music in the editor.
4. **Pixel-perfect sprite collision** — walk the sprite alpha; AABB covers 95% of cases
   but this would be the next collision improvement.

> The DIV-style **process/coroutine model** was cut from the roadmap — the cart
> corpus shows the plain typed-pool style is enough; see VISION.md for the rationale.

---

## Web build

### Setup on a new machine

Only one thing to install — everything else is in the repo:

```bash
brew install emscripten   # macOS/Linux via Homebrew
```

Then the "Build for web" button in the editor cart tab just works.
`runtime/raylib-web/` is committed (Raylib 5.5 built from source with
emscripten 5.0.7) so no separate download or rebuild is needed.

### How it works

1. Click **"Build for web"** in the cart tab
2. Editor saves sprites + map, opens the runtime log showing progress
3. `emcc` compiles `cart.c + studio.c` → `build/cart.html + cart.js + cart.wasm` (~10s)
4. A Node HTTP server starts on port 8765 (or reuses the existing one)
5. The cart opens in the default browser automatically

The built files are in `build/` — zip `cart.html + cart.js + cart.wasm` and upload
to itch.io to share publicly.

### Web-specific behaviour

- **"click to start" screen** — shown before the game loop starts. The click
  satisfies Chrome's autoplay restriction; `InitAudioDevice()`, `sound_init()`,
  and `init()` all run inside the click handler so the AudioContext is created
  after a real user gesture. Sound works fully after clicking.
- **`pget()` works on web** (opt-in) — the canvas read-back is opt-in via
  `enable_pget(true)` (off by default both platforms; 2026-06-11). On web, studio.c does
  its own `glReadPixels` on the canvas FBO instead of `LoadImageFromTexture` (which runs an
  ES3-only format probe that spams a cosmetic WebGL1 `INVALID_ENUM`). Validated desktop
  Chrome + iOS Safari — correct pixels, clean console. Carts already in the gallery need a
  republish to pick it up. Details: [mobile-web-notes](design/mobile-web-notes.md) §5c.
- **`save()`/`load()` don't persist** — emscripten's virtual filesystem is
  in-memory only; data is lost on page reload. Fix later with localStorage.
- **ScriptProcessorNode deprecation warning** — cosmetic, harmless. Raylib uses
  miniaudio which hasn't switched to AudioWorklet yet. Sound still works.

### emcc flags and why

```
-s USE_GLFW=3              # GLFW canvas input (required for Raylib)
-s TOTAL_MEMORY=67108864   # fixed 64MB heap — ALLOW_MEMORY_GROWTH invalidates
                           # the HEAPF32 TypedArray view used by the audio callback
-s EXPORTED_RUNTIME_METHODS=ccall,HEAPF32
                           # emscripten 5.x no longer exports these on Module by
                           # default; miniaudio's JS onaudioprocess uses both
```

### How the Raylib web library was built

The pre-compiled `raylib-5.5_webassembly.zip` from GitHub was built with an old
emscripten and ships miniaudio 0.11.21 (broken ScriptProcessorNode). We built from
source instead:

```bash
git clone https://github.com/raysan5/raylib.git --branch 5.5 --depth 1 /tmp/raylib-src
cd /tmp/raylib-src
emcmake cmake -S . -B build-web -DPLATFORM=Web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j4
# outputs: build-web/raylib/libraylib.a
```

Result is committed at `runtime/raylib-web/lib/libraylib.a`. WASM bitcode is
architecture-independent so the same file works on any machine with emscripten.
If you ever need to rebuild it (e.g. for a new emscripten major version), run the
above and replace the file.

---

## Cart format

Carts are `.cart.png` files — a PNG whose visible image is the last game screenshot,
with data embedded as compressed `zTXt` chunks:

| Chunk key | Content |
|---|---|
| `de:source` | C source (cart.c text) |
| `de:sprites` | sprite sheet as a PNG data URL |
| `de:map` | map bytes as base64 |

**Code:** chunk helpers live in `editor/electron/main.cjs` (functions `embedCartChunks`,
`extractCartChunks`, `makeZtxtChunk`, `crc32`). Same logic is duplicated in
`tools/make-cart.js` (standalone, no Electron dependency).

**Screenshot:** `studio.c` saves `build/screenshot.png` (3 frames, vertically flipped
corrected) on every clean exit. The save-cart handler in `main.cjs` prefers
`screenshot.png` over `sprites.png` as the base image.

**`--screenshot` mode:** pass `--screenshot` to the compiled cart binary to render
3 frames and exit automatically (used by `tools/make-cart.js --run`).

**IPC surface** (`preload.cjs` → `main.cjs`):
- `studio.saveCart(payload)` — opens save dialog, writes `.cart.png`
- `studio.loadCart()` — opens file picker
- `studio.loadCartFile(path)` — load from a known filesystem path
- `studio.loadCartBuffer(bytes)` — load from raw bytes (used by tutorials panel)
- `studio.getFilePath(file)` — `webUtils.getPathForFile` (Electron 32+ required)

**Drag-and-drop:** dropping a `.png` onto the editor window loads it as a cart.

---

## Tutorial cart toolchain

Full docs in [`guides/cart-authoring.md`](guides/cart-authoring.md). Quick reference:

```bash
# create cart (with optional sprites/map from XX-name.cart.js config)
node tools/make-cart.js tools/carts/XX-name.c editor/public/carts/XX-name.cart.png

# compile + run 3 frames + bake screenshot into cart
node tools/make-cart.js --run editor/public/carts/XX-name.cart.png

# just update the screenshot of an existing cart
node tools/make-cart.js --update <cart.png> <screenshot.png>
```

Cart sources live in `tools/carts/XX-name.c`. Config files (sprites + map) live in
`tools/carts/XX-name.cart.js`. Finished carts go in `editor/public/carts/`.
`editor/public/carts/index.json` is the metadata list the tutorials panel reads.

20 numbered tutorial carts are shipped (01-hello through 19-breakout, plus
05b-colorkey), alongside dozens of example game carts (~100 carts total).
- 11-noise: two-layer scrolling terrain + twinkling stars via `noise2()`
- 12-hiscore: button-mashing game with `save()`/`load()` high score persistence
- 13-easing: four dots on tracks showing all three easing curves vs linear
- 14-hud: catch-the-shrinking-star game using `print_centered`, `print_right`, `rnd_between`
- 15-anim → 19-breakout: walk-cycle animation, turtle-graphics spirograph/hypotrochoid,
  and the first two arcade ports (invaders, breakout)

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
- **`follow()`** is now implemented. Cart 10 uses it correctly.
- **`save()`/`load()`** write `cart.sav`/`cart.kv`/`cart.blob` into a per-cart folder:
  the editor and `play.js` pass `--save-dir saves/<cart>`, so saves live under
  `build/saves/<cart>/` (2026-06-04; previously all carts shared `build/cart.sav`).
- **Inline error markers** — red gutter dot + red line background on clang error lines.
  Cleared on next compile. Pattern matched: `cart.c:LINE:` in stderr output.
- **`frame()`** increments in the main loop after `update()`, declared static at the
  top of `studio.c` alongside the other static state.
- **`print_centered`/`print_right`** use `strlen(text) * 8` for width — the dos_8x8
  font is exactly 8px per character with 0 spacing (`DrawTextEx` size=8, spacing=0).
- **`rnd_float()`** uses `rand()` from `<stdlib.h>` (added to studio.c includes).
  All carts share the same `rand()` seed — not seeded per-cart; same sequence every run.
- **CLAUDE_CODE_TMPDIR** fills up occasionally. Workaround: redirect compile output
  to `build/compile-test.log` and read it back with the Read tool.
- **`trifill()` winding order** — Raylib's `DrawTriangle` needs counter-clockwise winding
  in Y-down screen coords. In Y-down space, cross product > 0 means clockwise visually
  (opposite of math convention), so swap when `cross > 0`.
- **`init()` fires after window + sprites are fully loaded** — safe to call `colorkey()`,
  `mset()`, etc. It does NOT run during `--screenshot` mode's 3-frame early exit, but
  that's fine since screenshot mode still calls it once before the loop.
- **`anim()` breaking change** — added required `phase` param. No existing cart was using
  `anim()` when the change was made, so nothing broke.
- **`colorkey(color)`** is fully shipped — studio.c keeps a clean copy of the original
  spritesheet as `spritesheet_img` so `colorkey(-1)` correctly restores full opacity.
  Has a tutorial cart at `05b-colorkey.cart.png`.
- **Raylib auto-detected:** `/opt/homebrew/opt/raylib` (Apple Silicon) or
  `/usr/local/opt/raylib` (Intel). Both `main.cjs` and `tools/make-cart.js` do this.
- **Web build server stays alive** — `startWebServer()` in `main.cjs` keeps one
  Node HTTP server on port 8765 alive for the editor session. It reuses the same
  instance on subsequent builds (doesn't restart). If something is already on 8765
  externally, kill it: `kill $(lsof -ti:8765)`.
- **CLAUDE_CODE_TMPDIR full** — emcc output gets lost when the tmp partition fills.
  Workaround: redirect to `/tmp/emcc.log` directly: `emcc ... >/tmp/emcc.log 2>&1`.
- **Web build emcc output goes to runtime log** — progress messages (`generating
  headers…`, `running emcc…`, `✓ done`) appear in the runtime log panel (same one
  used for `printh()` output). Errors show in the build log panel.

## Working preferences observed

- **Respect day/night theming** — use CSS vars (`--bg`, `--bg2`, `--fg`, `--fg-dim`,
  `--border`, `--accent`, `--font`), never hardcode panel colors.
- **Panels auto-hide when empty** (build log 3s timer, runtime log on clean exit).
- **Optimize for beginner legibility** — visible mistakes are a first-class goal.
- Commits go **direct to `master`** (solo repo).
