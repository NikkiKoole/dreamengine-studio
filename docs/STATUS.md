# STATUS — what's shipped, what's open, what's decided-against

> **Single source of truth for project status.** The design docs
> ([`design/api-notes.md`](design/api-notes.md), [`design/audio-notes.md`](design/audio-notes.md), [`VISION.md`](VISION.md))
> hold the *rationale and proposed signatures*; this file is the *ledger* — the one place
> that says whether a thing is shipped, open, or cut. When status changes, update it
> **here**, then fix the prose in the relevant design doc. If a design doc and this file
> disagree, this file wins.

_Last updated: 2026-07-06 (**LOCKSTEP NETPLAY, RUNG 5A STEPS 1–3 — BROWSER NETPLAY LIVE-VERIFIED (two real browsers, a relay match on the home wifi; peer-left banner on the survivor's screen): two tabs (or an iPad + a laptop) meet in a room via `tools/net-relay.js` (zero-dep WS relay, blind byte-forwarding, `--serve` = cart + relay from one wifi box printing the exact two-device URL; lobby v0 = `?room=X` in the URL) over an `EM_JS` WebSocket arm in net.h with an async pre-init handshake (ROLE→HELLO→WELCOME{seed}); the live two-tab pick is the one remaining test. `node tools/net-check.js` = the one-command gate (echo mirror + netdemo pair + relay wire-protocol, all PASS). Underneath it, STEP 1: `net.h` split into `DE_NET_CORE` (rings/packets/barrier, no sockets — native AND web) + `DE_NET_BUILD` (UDP + flags + lobby, native-only as before), all I/O through the new `net_transport_send/pump` seam where step 2's WebSocket arm lands; non-blocking `net_frame_try_sync()` barrier (web stalls the tick — a browser can't block; native wraps it blocking, netdemo-identical); det clock unified for web; and an ECHO loopback fake-peer transport proves remote-input injection on both targets (`--net-echo` / `-DDE_NET_ECHO_DEFAULT`). Gates: netdemo LOCKSTEP OK, echo p2y==p1y ×300, build-all 466/466, emcc green. LAN rungs 1–3 untouched — web is a second transport NEXT to UDP. Full ledger entry below.**) Previous headline: (**LOCKSTEP NETPLAY — RUNGS 1 & 2 SHIPPED: two native builds play the SAME cart over UDP (localhost or LAN by IP), input-lockstep, carts stay 100% network-unaware — `btn(player,…)` now means "which machine": all local input (either keymap + touch) is MY player, the peer's byte comes off the wire. `runtime/net.h` (~250 lines, runtime-flag-gated like the harness: `--net-host` / `--net-join <ip>` / `--net-port`), host seeds the joiner in the handshake, NET_DELAY=3 frames input latency, GGPO-style redundant packets, BYE/timeout exits. Demo + deterministic gate: `play.js <cart> netdemo` spawns a host+joiner pair side by side and diffs their per-frame traces (LOCKSTEP OK / DESYNC, checks-and-oracles has the one-liner; netcheck clips parked at tools/clips/pong/). RUNG 2 (LAN by IP, same day): a **🌐 multiplayer button** in the editor drives host / join-by-IP (shell.js popover; main.cjs adds the `--net-*` flags), and the host resolves + shows its LAN IPv4 (`net_local_ipv4()` via `getifaddrs()`) so the joiner knows what to type — the "click host → get an address" UX, no NAT, no servers. Rung ladder + what's next: design/multiplayer-research.md.**) Same day: **TINYJAM RACK #2 SHIPPED — `yachtrack` "session desk": the yacht radio opened up — the first CHORD-FIRST rack (the chord chart is the star: mu-vocab chord row + MU-IFY/SUS-MELT, form row + gear change, drum skeleton lanes w/ session/Purdie/CR-78 drummer chairs, the 32-step sax hook cell — while bass runs / comp anticipation / ghosts / swing stay session PLAYERS behind feel knobs: the chart+interpreters model) and the first RADIO→RACK SEED HANDOFF, proven — the generator is yacht's `new_song` VERBATIM, gated by a 44-pair golden songsum corpus in `spec()` (48 asserts). Design record: design/yacht-rack.md, design→shipped in one day.** Previous headline: **THE FIRST TINYJAM RACK SHIPPED — `acidrack`: the ReBirth RB-338 (2×303 + full 909 + curated 808) as one cart with per-device FX, banks+song chain, and a SEEDED GENERATOR (8-hex song code → a whole arranged acid track, WAV-exportable). Rode in with engine work: `FILTER_DIODE`, a real TB-303 diode-ladder lowpass (~18dB/oct, bass-draining resonance, in-loop saturation — audio-notes §25) + `tools/filter-spec.js`, the filter-response oracle; tb303.c upgraded to it. Design record: design/rebirth-classic.md, design→shipped in one day.**_

---

## Shipped ✓

**Tooling & environment**
- Code editor (CodeMirror 6, C syntax, autocomplete + hover + Cmd-click-to-help +
  cmd-click an `#include "x.h"` filename → opens the read-only engine source in the
  docs tab's "engine source" group), sprite editor, map editor — all in one
  PICO-8-style window.
- ▶ run (clang → native Raylib window), inline clang error markers.
- Cart format — `.cart.png` with source/sprites/map in zTXt chunks; save, load,
  drag-drop. Carts carry their own settings (screen/scale/cell/map).
- Tutorials gallery — 263 registered carts (tutorials, games, toys, instruments, probes);
  all of them also playable on the web gallery (<https://nikkikoole.github.io/dreamengine/>,
  sortable by date-added/title/mobile-readiness, day/night, description toggle).
- **`sprite-draw.js` post-processing batch + `foundry.cart.png`** (2026-06-04) — five new ops for programmatic `.cart.js` sprites: `shade()` (auto light/shadow via the curated `RAMP_DARKER`/`RAMP_LIGHTER` palette LUTs — *the* "one step darker/lighter" table for the fixed palette), `rotate()`/`rotations()` (baked headings, still post-processable unlike runtime `spr_rot`), `scale2x()` (EPX: sketch 16×16, bake 32×32), `replace()`/`clone()` (bake-time variants); `split()` now grid-splits a 32×32 into 4 slots as its comment always claimed. Showcase: **SPRITE FOUNDRY** — "watch the code draw": `foundry.cart.js` snapshots the canvas into the next slot after every drawing step, and the cart plays each subject's time-lapse back with the code line per frame (dragon → `shade()`, ship → `mirror()`+`rotations()`, boss → `noise()`/`replace()`/`scale2x()`). Tutorial 15 (animation phase) also rebuilt on the library: its 6-frame walk cycle is one parametric `walker(t)` sampled over the stride. See [`guides/cart-authoring.md`](guides/cart-authoring.md) → "sprite-draw.js".
- **`ragdoll.cart.png`** — Verlet physics sandbox. Up to 50 stick-figure ragdolls hop autonomously across the screen, bounce off each other and off rolling balls. Grab + throw with mouse (whole-body drag), right-click to spawn balls, C to spawn characters, R to reset. Key techniques: Störmer-Verlet integration, distance constraints (12 sticks/character, 8 iterations), position springs chained bottom-up (feet → knees → hip → chest → head), angular springs per bone with 90° guard (cross-product direction inverts past 90°), per-character standing/ragdoll timer that only recovers when feet are on the floor, hop impulse that immediately releases the foot pin so it isn't erased, broad-phase character collision (hip-to-hip > 40px early-out then 12×12 point pairs with velocity impulse). Debug session used the play.js harness + DE_TRACE watches to trace `rtimer`, `whop`, `hip_y`, `knee_y` and diagnose: rest lengths mismatched standing pose (hip-knee was 9, actual √73 ≈ 8.54 → knee pushed to floor); hop velocity erased by foot pre-pin each frame (fixed by setting `rtimer=0` inside `hop_tick` and re-reading `sk`). See `tools/carts/ragdoll.c`.
- Web build — "Build for web" (emscripten → `cart.html/js/wasm`, local server on 8765).
- **Store / press / share toolchain + the editor share panel** (2026-07-03) — a full in-house
  App-Store-prep suite, all FREE (no account, no subscription). Tools: `aso-research` (keyword
  landscape, clickable App Store links), `aso-compose` (pack the 100-char field), `aso-lint`
  (char limits / waste / cross-field repeats), `store-shots` (device screenshots — composite-
  not-stretch), `store-contact` (hero-frame contact sheet), `press-kit` (a presskit()-style page
  from cart data). Editor surface: a topbar **⇪ Share** popover (per-cart exports, grouped by
  audience) + an **Apps** tab (app-less ASO lab · apps list from `apps/*/app.json` · per-app
  actions: 📸 screenshots → 📄 press kit, 🍎/📱 Mac/iOS builds, 📝 worksheet / 🔎 research / 💡 suggest / 🧩 compose / 🔬 analyze / 📊 score / ✅ lint / 🪞 check the app's
  `listing`). The app manifest gained a `listing` block (title/subtitle/keywords) as the
  machine-readable home. Design + rationale: [`design/store-agents.md`](design/store-agents.md),
  [`design/share-panel.md`](design/share-panel.md), [`design/press-kit.md`](design/press-kit.md);
  Tiny Jam's listing: [`marketing/tinyjam/app-store-listing.md`](marketing/tinyjam/app-store-listing.md).
  Still open (before a real submission): per-locale copy, the ASC upload/TestFlight step
  (ADR-0026), and the Search-Term-Rank popularity column (Apple beta).
- **Live (libtcc) backend + hot reload** — a "run mode" toggle (settings) switches ▶ run from the clang static build to a persistent `-DDE_TCC` host that JIT-compiles the cart in-process via vendored `runtime/libtcc/`. Editing the code auto-reloads it (debounced, no Run press) without restarting the window; compile errors mark the line and keep the last good cart running. State survives reloads via **`de_state()`** — promoted to a first-class `studio.h` API and fronted by the starter cart's friendly `STATE { ... }; / S->field` sugar (clickable to help). arm64-macOS only; sprite/screen changes relaunch. Full record + rationale: [`design/cart-as-script.md`](design/cart-as-script.md).
- 5-tab navbar (code · pixels · carts · docs · settings); in-app docs viewer renders
  this `docs/` set (with cross-links) in the Docs tab.
- Day/night theming, debug overlay (`watch`/`printh`/crash capture).
- **Live inspection** — on-demand screenshot + state snapshot while a cart runs. Write the desired output path into `build/.bake/screenshot_request` or `build/.bake/state_request`; the game captures the current frame on its next tick and deletes the request file as the handshake (gone = fresh, ready to read). Lets you bracket a specific moment: capture before + capture after = instant diff without a filmstrip. Works alongside `play.js run --headless` and any other harness mode. See [`guides/debug-harness.md` → Live inspection](guides/debug-harness.md).
- **Profiler** — one-click `⏱ profile` button (hidden behind a settings toggle). Compiles a profiling build (`-O1 -fno-inline -DDE_PROFILE`), runs the cart ~4s, and reports into the build log: frame CPU budget (ms vs the 16.6ms 60fps target), hottest functions **with the call paths that reach them** (macOS `sample` call-graph attribution, rolled up to `studio.h` primitives), and exact per-frame draw-call counts (in-engine `PROF` counters, re-entrancy-guarded). Behind the scenes — no Instruments GUI. macOS-only (uses the `sample` CLI). **`PROF` counters + frame timing are now always on in native builds** (not just `-DDE_PROFILE`) — `perf.json` is written every 30 frames in any normal run; snapshot it on demand with `profiler_request` (same trigger-file pattern as `screenshot_request`). `-DDE_RELEASE` strips all overhead (new settings toggle, see below). See [`guides/profiler.md`](guides/profiler.md).
- **Release build mode** — settings → run mode → "build mode" select. `normal` (default): profiler counters + `harness_inspect` polling on, `-Os`. `release`: `-O2 -DDE_RELEASE` — strips `PROF()`, timing measurement, and all per-frame trigger-file probes. For when you want to benchmark or ship without any instrumentation overhead.
- **Per-cart save folders** — `save()`/`save_int()`/`save_bytes()` files (`cart.sav`/`cart.kv`/`cart.blob`) live in `build/saves/<cart>/`, not one shared set in `build/`. Runtime takes `--save-dir DIR` (any native build, default cwd); the editor slugs `cartName` and `play.js` uses the cart's file stem, so editor saves and harness saves are separate folders per cart — a scripted test run can't clobber a real hiscore. Web build unchanged (no argv). See [`guides/debug-harness.md`](guides/debug-harness.md) flags table.
- **`monstermix.cart.png`** — the `sprite-draw.js` `stamp()` showcase cart. The `.cart.js` draws 9 parts (3 heads, 3 bodies, 3 legs, `mirror()`ed) and `stamp()`-composites all 27 combos into slots at bake time; magic `pal()` indices 28/29 recolor them into 4 schemes at draw time — 108 monsters from 9 parts. Also exercises `split()` (32-wide machine), concave `polyfill` (star), `noise()` tiles, `outlined()` with a custom outline color. Gameplay: assemble the customer's order, piston-stamp it (squash + dust + shake), combo chords climb with the streak. See `tools/carts/monstermix.c` / `.cart.js`.
- **`tools/font-bake.js` + `fontbake.cart.png`** (2026-06-05) — real-TTF text as sprites, at build time. Parses a TTF (vendored `tools/vendor/opentype.cjs`), flattens the glyph outlines and scanline-fills them (nonzero winding, 3×3 supersampled, optional darker AA-edge color) into sprite-draw 2D canvases — so any Google Font can be a cart's title with zero runtime font code. `measure()` for fitting a slot budget; border/shadow are plain sprite-draw composition (`outlined()`, offset-stamped recolored clone). Fonts live in `tools/fonts/` (Bungee + OFL included; new ones are one curl from github.com/google/fonts). Showcase: **font bake** — words baked centered into fixed slot-rects so the C side `sspr()`s constant sheet regions; title waves in 4px strips, `pal()`-recolors live (fill + AA edge remapped together — swapping only the fill leaves a clashing rim). Same-day follow-up: **`bakeBanner` promoted into the library** (fit + center + outline + shadow → ready tiles; second customer) and **high noon cart** (Smokum) — a quick-draw reaction duel where the baked words ARE the game (DRAW! signal, DEAD/EARLY!/YOU WIN verdicts), five words packed to exactly 64 slots; full championship/death/early paths verified via scripted play.js runs. Hard-won rules now in the guide: `colorkey(0)` in `init()` is mandatory (no default transparent color — banners drag an opaque black slot-rect without it), and every word needs two slot-rows (one row = ~11px glyphs after outline trim, too thin at 2x). See [`guides/cart-authoring.md`](guides/cart-authoring.md) → "font-bake.js".
- **THE BAND panel** (2026-06-05) — every chaired radio station gets a live timbre-audition overlay: press **B**, click a chair row (or press its number) to cycle that chair's instrument candidates mid-song. The G-key A/B pattern generalized: `runtime/radio.h` owns the registry + input + draw (`rad_chair` / `rad_band_input` / `rad_band_panel`), the cart applies each swap in its own `apply_chair()` — the toolkit never calls back in and never touches `rad_srnd`, so pinned seeds stay byte-identical. Picked chairs re-assert after `new_song`'s per-song rolls; chairs left at sel 0 keep the shipped sound and roll. Chaired so far: **cocktail** (solo chair hands improv.h's chorus to piano/vibes/guitar), **tango** (the three orquestas: troilo/d'arienzo/pugliese reed tables, arco/pizzicato violins, felt/dark piano), **yacht** (dx tine/rhodes/clavinet ep, three basses, three leads, pad), **roadhouse** (VOX/Gibson drawbar tables, rhodes/upright piano bass, guitar), **exotica** (vibes/marimba/denny-piano, fm-glass/celesta). Candidates sourced from [`design/radio-instrument-options.md`](design/radio-instrument-options.md) — ten stations there still unchaired.
- **`tools/sprite-draw.js`** — shared programmatic sprite-authoring library for `.cart.js` files. Exports a 2D pixel-canvas API aligned with the C drawing API names: `blank`, `pixel`, `rectfill`, `rrectfill`, `line`, `circlefill`, `ovalfill`, `trifill`, `polyfill`, `ngonfill`, `noise`, `outlined`, `mirror`, `stamp`, `flat`, `split`, `OUT`. All 14 programmatic `.cart.js` files `require('../sprite-draw.js')`. Three `.cart.js` authoring styles documented: (1) settings-only, (2) ASCII art with `DEFAULT_CHAR_MAP` (palette 0–15), (3) programmatic arrays via sprite-draw (palette 0–31, geometry). See `tools/sprite-draw.js` and `CLAUDE.md` → project structure.
- **Lockstep netplay, rung 1** (2026-07-02) — two native builds play the same 2-player cart over UDP (localhost or LAN by IP) with carts staying 100% network-unaware: under netplay `btn(player,…)` means "which machine" (host = player 0, joiner = player 1; all local input — either keymap, touch — is *my* player). `runtime/net.h`, runtime-flag-gated like the debug harness (`--net-host` / `--net-join <ip>` / `--net-port <n>`; a normal run touches none of it). Net implies `--det`; the host's seed rides the handshake so both sims roll the same `rnd()` stream. NET_DELAY=3 frames of input latency, GGPO-style redundant input packets (a dropped datagram never stalls), BYE on quit + a 10s barrier timeout (peer crash ≠ hang). **v1 syncs `btn()` only** — carts reading `key()`/`mouse_*()` in `update()` desync. Pause is disabled under net (would stall the peer). Demo + gate: **`play.js <cart> netdemo`** spawns a host+joiner pair side by side (optional per-side scripts) and diffs their per-frame traces — `LOCKSTEP OK` / `DESYNC` verdict, exits nonzero on desync; the blessed check lives in [`guides/checks-and-oracles.md`](guides/checks-and-oracles.md) with committed netcheck clips at `tools/clips/pong/`. Design + the rung ladder (LAN discovery, determinism proof, internet/browser): [`design/multiplayer-research.md`](design/multiplayer-research.md).
- **Multiplayer site UI — "play together" + the room bar** (2026-07-06, rung 5a step 4 v0.5) — multiplayer stops being invisible on the published site: the gallery gives 2-player carts a **👥 play together** button (mints a room code, opens the cart in it, `&relay=` auto-appended off the Render domain), and the cart page (both shells) gets a **room bar** — in a room: the code + a *copy invite link* button (the page URL is the invite); not in one: a *play together* offer, gated by a `players` field build-site.js stamps into each cart's `manifest.json`, derived at build time from the cart SOURCE's `de_players()` (same fn that gates the native lobby — zero hand-maintained metadata). Editor build-web previews unaffected (no manifest → bar stays hidden). Render-domain gallery shows it after the next manual deploy (`autoDeploy: false` is deliberate). Design: [`design/multiplayer-research.md`](design/multiplayer-research.md) §5a step 4.
- **The repo split: private-ready code repo, public site repo** (2026-07-06) — the published gallery moved out: code repo renamed **`dreamengine-studio`**, the 356MB `site/` (which had been re-committed 105 times into code history) became its own checkout of the new public **`NikkiKoole/dreamengine`** repo — same `nikkikoole.github.io/dreamengine/` URL because the *public* repo inherited the name. Branch-based Pages (no workflow; `pages.yml` retired), `site/` gitignored in the code repo, `publish-cart.sh` reworked into two legs (commit+push inside site/'s own repo; a by-pathspec write-back of published cart sources here) with a guard that prints the re-clone one-liner for machines where site/ is missing. **The code repo can now flip private with zero further work.** Other machines: the first pull past the split deletes `site/` → `git clone git@github.com:NikkiKoole/dreamengine.git site`. Design record: [`design/sharing-channels.md`](design/sharing-channels.md) §"Parked: private code repo".
- **Netplay relay: Render blueprint + cart-scoped rooms** (2026-07-06; **DEPLOYED + wss-verified same day** — `https://dreamengine-relay.onrender.com` is live, serves the gallery, and a two-client wss probe confirmed seating + verbatim forwarding through Render's proxy; a complete shareable match link is `https://dreamengine-relay.onrender.com/pong/?room=play`) — the internet-relay decision is made and committed: a **`render.yaml`** blueprint deploys the relay on Render's free tier (**moved same day into the public SITE repo after the repo split** — `site/render.yaml` + a publish-synced `site/net-relay.js` copy, so Render connects to `NikkiKoole/dreamengine` only and the code repo stays unshared; the service also `--serve`s the gallery, so games run off the Render domain with no `?relay=` param) (Frankfurt, wss:// handed out, build step overridden so the root `sharp` dep isn't compiled for nothing; bare-URL GET answers 200 = the cold-start warm-up). And **rooms are now cart-scoped by construction**: the web transport prepends the cart's URL path segment to the code (`?room=play` in pong → room `pong-play`), so ONE shared relay hosts every cart with the same friendly codes and can never cross-pair two different games — the relay stays blind. Walkthrough: [`design/multiplayer-research.md`](design/multiplayer-research.md) §"Hosting beyond the LAN".
- **Lockstep netplay, rung 5a steps 2+3 — WebSocket transport + the relay** (2026-07-05; **LIVE-VERIFIED 2026-07-06** — two real browsers played a relay match on the home wifi; a leaving peer now shows an on-screen "PLAYER 2 LEFT — PLAYING SOLO" banner on the survivor's side, `net_notice` in net.h + `draw_net_notice()` in studio.c. Hosting beyond the LAN — the github.io cart + a `wss://` relay story — is written up in [`design/multiplayer-research.md`](design/multiplayer-research.md) §"Hosting beyond the LAN"). Step 2: the web transport arm in `net.h` — an `EM_JS` WebSocket shim (`de_ws_begin/state/send/recv`; **no strings cross the C/JS boundary** — JS reads `?room=`/`?relay=` from the page URL itself) + `net_web_poll()`, the ASYNC twin of `net_handshake` driven by the pre-init click screen (ROLE → HELLO → WELCOME{seed} through the relay, seed before `init()`, exactly the lobby's ordering); mid-game `net_ws_pump` handles INPUT/BYE, a dead socket = BYE, web drops to solo instead of exiting the tab. New `NET_PKT_ROLE` — the one relay-originated packet (first in room = host). Step 3: **`tools/net-relay.js`** — zero-dependency Node (hand-rolled RFC 6455): rooms of 2 by code, blind binary forwarding (never parses game packets — one relay serves every cart forever), BYE synthesis on disconnect, **`--serve <dir>`** = the one-wifi-box setup (static cart + relay in one process, prints the exact two-device URL), `--check` self-test. Lobby v0 = the URL itself (`?room=X`). Plus **`tools/net-check.js`**, the one-command lockstep gate (echo mirror + netdemo pair + relay wire-protocol sim — all PASS), and `--net-echo` passthrough in play.js. Try it: `node tools/build-site.js pong && node tools/net-relay.js --serve site/pong` → open the printed URL on two devices. Design: [`design/multiplayer-research.md`](design/multiplayer-research.md) §5a.
- **Lockstep netplay, rung 5a step 1 — the lockstep core compiles on WEB** (2026-07-05) — the "one lockstep core, two transports" split from [`design/multiplayer-research.md`](design/multiplayer-research.md) §5a, the gating work for browser multiplayer. `runtime/net.h` is now two tiers: **`DE_NET_CORE`** (input rings, `NET_PKT_*`, frame barrier — no sockets anywhere, compiles native AND web) and **`DE_NET_BUILD`** (UDP transport + `--net-*` flags + handshake/lobby, native-only exactly as before); every send/receive goes through the new `net_transport_send`/`net_transport_pump` seam, where step 2's WebSocket arm will land and nowhere else. New **non-blocking barrier `net_frame_try_sync()`**: web `loop_step` stalls the whole tick (no `sound_tick`, no update/draw, no clock advance) while the peer's byte is missing — a browser main thread can't block or WS messages never arrive; the native `net_frame_sync()` is now a blocking wrapper around it. `det_mode`/`det_clock`/`clk()` moved out of the native-only harness block so web runs the fixed-step deterministic sim under net. Proven end-to-end by the new **echo transport** (a loopback fake peer): `--net-echo` (native) / `-DDE_NET_ECHO_DEFAULT` (web) — P2 mirrors P1 through the real pack→send→ring→barrier→`btn(1)` path. Gates: netdemo `LOCKSTEP OK` ×300 traced frames (per-side scripts), echo trace `p2y==p1y` ×300, `build-all` 466/466, emcc compiles the core bare + echo-enabled. **LAN netplay (rungs 1–3) is unchanged — the web path is a second transport NEXT to UDP, not a replacement.** Next: step 2, the WebSocket transport + ~100-line relay.
- **Lockstep netplay, rung 3 — "Open to LAN" auto-discovery** (2026-07-02) — the joiner **finds the host with no typing**. The host broadcasts a small ANNOUNCE datagram to the subnet (`255.255.255.255:33446`) every ~1s while waiting (`net_announce()`, game socket + `SO_BROADCAST`); the lobby's Join screen listens (`net_discover_begin/poll/end` — a `SO_REUSEADDR` socket) and auto-fills the discovered IP ("found a game at 192.168.x.x — ENTER to join"), manual entry kept as fallback. Cross-platform (Winsock too). Verified: the announce is received on-box (sniffed `DN`+type 5+port 33445) and it cross-compiles for Windows; the full two-machine pick is a live test. Minecraft-style, no servers. Design: [`design/multiplayer-research.md`](design/multiplayer-research.md).
- **Lockstep netplay, rung 2.5 — in-game lobby** (2026-07-02) — an engine-owned **Host / Join / Solo boot menu** (`net_lobby_menu()` in `runtime/studio.c`, gated by `--net-lobby` or the compile-time `DE_NET_LOBBY_DEFAULT`) so a **standalone build with no editor** can start netplay with no CLI flags — the "send a friend an .exe" case. Reorders the net startup: the lobby draws after fonts load but before the cart's `init()` (the host's rnd() seed must reach the joiner first), so the handshake now runs with the window open, drawing a `HOSTING at <ip>` / `connecting…` status frame (this also fixes rung 2's "host waits with no window" rough edge). Join screen has an in-window IP text-entry. The direct `--net-host`/`--net-join` path (editor 🌐 button / CLI / netdemo) is unchanged. This is the design doc's "the shell owns host/join" — except for a standalone the *engine* is the shell. **Also (same day):** a **`MULTIPLAYER` item in the pause menu** that **self-restarts** the binary into the lobby (`net_restart_into_lobby()`, reusing the RESTART item's `execv(restart_argv)`) — so a player double-clicks the exe, plays solo, then pauses → MULTIPLAYER → lands in Host/Join/Solo; two launches on one Mac each do this to play locally. **And `net.h` is ported to Winsock** (winsock2-before-windows.h via `NOGDI`/`NOUSER`/`NOMINMAX`; `getifaddrs`→UDP-connect trick; `de_mkdir`/`SIGBUS` gaps fixed; `-lws2_32`), so netplay carts now **cross-compile to a real Windows `.exe`** — compile-verified on the dev box; Windows *runtime* test pending a real machine. Next: an "export exe" button, then rung 3 (multicast auto-discovery). Design: [`design/multiplayer-research.md`](design/multiplayer-research.md).
- **Lockstep netplay, rung 2 — LAN by IP** (2026-07-02) — the shipped path from "CLI-flag capability" to "playable from the editor". A **🌐 multiplayer button** next to ▶ opens a host / join-by-IP popover (`editor/src/shell.js`); `editor/electron/main.cjs` adds the `--net-*` flags to the run spawn and shows the host's LAN IPv4 (via `os.networkInterfaces()`), while the native binary resolves + prints it too (`net_local_ipv4()` in `runtime/net.h`, `getifaddrs()`, prefers a 192.168/10 private address). Host on one Mac, read the shown IP, type it into Join on the other — the wished-for "click host → get an address" UX for the home/classroom case, no NAT, no servers. **Deviation from the plan:** the address surfaces in the editor UI + console, not an in-window overlay — `net_handshake()` blocks *before* `InitWindow`, so there's no window to draw on during the host's wait; the shell (where the button is) is the better surface anyway. Known rough edge: a host with no joiner waits with no window until someone connects (or the editor quits). Next: rung 3 (UDP-multicast "Open to LAN" auto-discovery). Design: [`design/multiplayer-research.md`](design/multiplayer-research.md).

**API surface** — ~125 functions + ~90 constants in `runtime/studio.h`.
For the full grouped inventory see [`design/api-notes.md` → "What dreamengine has today"](design/api-notes.md).
Recently landed and worth calling out:
- Juice batch: `pal`/`pal_reset`, `fade`, `shake`, `print_scaled`, `text_width`.
- **Font system:** `font(FONT_NORMAL/FONT_SMALL/FONT_TINY)` state setter; two extra fonts baked (`font4x6.png` ~64 chars/320px, `font3x5.png` ~80 chars/320px); `print_shadow`, `print_outline`; all `print*` functions now return x-after-last-char for chaining and overflow detection. Demo: `fonts.cart.png`. See [`design/font-rendering.md`](design/font-rendering.md).
- Sprite transforms: `spr_rot`, `sspr_ex` (rotation/scale/flip around a pivot).
- **`sget(sx,sy)`** — palette index of a *spritesheet* pixel (companion to `pget`, which reads the canvas). Lets carts treat sprites as data: paint a level in the sprite editor (1 pixel = 1 block, color = type) and read it back at runtime, or build lookup tables. Shipped with two paired platformer tutorial carts — **`platform-rects`** (a pixel-perfect AABB mover: per-axis resolution + sub-pixel position, coyote time, jump buffering, variable jump height, one-way platforms; level as a hard-coded `Box[]`) and **`platform-paint`** (same mover, level read from a painted sprite via `sget`). Same engine, two level sources — the "level as code vs level as data" teaching pair from [`design/tutorial-curriculum.md`](design/tutorial-curriculum.md).
- Fill patterns: `fillp`/`fillp_reset` + `FILL_*` (PICO-8-style fillp).
- Shapes/helpers: `oval`/`ovalfill`, `bar`, `blink`, `fsqrt`.
- Pseudo-3D: `tritex` (affine texture-mapped triangle; used by `mode7`/`raycaster`/`cube3d`/`cityview`).
- **`cityview`** — GTA1-meets-Zelda pseudo-3D city bench: parallel-oblique projection (height goes
  straight up-screen), four building view modes, `tritex` wall textures, and drivable raised-highway
  flyovers (ramp/curve/spiral/stack) you climb while the camera rises. Folded in the former `overpass`
  experiment. Proves the projected-primitive helpers ([decision 0021](decisions/0021-road-geometry-in-2d-sandbox-render-is-an-adapter.md),
  mechanism in [design/pseudo-3d-city.md](design/pseudo-3d-city.md)). Next: pipe `roadlab`'s real `z(s)` deck through its projector.
- 3D leaf-helpers: `V3` + `rot3`/`project3`/`zsort`/`quadfill` — the rotate→project→sort→fill
  pipeline the solid-3D carts re-derived by hand. `cube3d`/`solid3d`/`textured3d`/`flyover`
  refactored onto them. [decision 0009](decisions/0009-small-3d-leaf-helpers.md).
- **`fade()` is now immediate-mode** — the runtime zeroes it each frame, so a cart re-asserts
  `fade()` only on the frames it wants the screen dimmed and never calls `fade(0)` by hand. Fixed
  the same stuck-dim bug in **27 carts** at once (conditional overlay fade that never cleared on
  exit). `camera`/`pal`/`fillp` remain sticky setters. [decision 0010](decisions/0010-fade-is-immediate-mode.md).
- **Removed:** turtle graphics (`turtle_*`/`pen_*`) — one user, trivially a cart; see Cut
  below + [decision 0008](decisions/0008-cut-turtle-graphics-api.md).
- Input: full mouse (`mouse_x/y/down/pressed/released/wheel`), keyboard
  (`key`/`keyp`/`text_input`).
- Time/persistence: `dt`, `epoch`, `save_bytes`/`load_bytes`.
- **`fps()`** — measured frames-per-second right now, `int`, smoothed over the last second
  (wraps Raylib `GetFPS()`). 60 = smooth; lower = the cart is too slow this frame. Independent
  of `dt()`/`det_mode`: even when the sim's `frame_dt` is pinned for deterministic replay,
  `fps()` still reports *real* render throughput, so scripted/headless runs stay reproducible
  while showing true speed. Intended as the read-out for the triangle-perf work (open item 14):
  `watch("fps", "%d", fps())` on a haze-heavy `podracer` frame, before/after a change. *(A
  dedicated profiling setup is in progress separately.)*

**Code-first sound** — 8-voice synth; `note`/`hit`/`chord`/`strum`/`tone`/`degree`,
`bpm`/`beat`, `every`/`euclid`/`chance`, `schedule`, `schedule_hit` (delay **+** duration —
sample-accurate sub-frame sfx/arp steps). (The `sfx` bank plays built-in demo data only —
see "Open" below. **`music()` is CUT** as of 2026-06-04 — zero cart users, patterns were
never cart-authorable, the generative beat-clock path won;
[decision 0013](decisions/0013-cut-music-api.md).)
- **Modulation envelopes** — `instrument_env()`/`note_env()`: 2 routable one-shot AD
  envelopes per slot (`ENV_CUTOFF` = the pluck "pew", `ENV_PITCH` = drum punch/zap,
  `ENV_DUTY`), bipolar amount, exp decay — the second EG (audio-notes §11). Demo carts:
  `filter env`, `pitch env`; wired into `modrack` (onboard fenv/penv + a VCA `a` jack) and
  `dream synth` (AMP/FILTER/PITCH envelope tabs).
- **Drawable waveforms** — `wave_set()` + `INSTR_USER0..3`: four 64-sample single-cycle
  tables you can draw and play like any wave; live-morphable (a ringing note changes as the
  table is rewritten). Demo cart: `wave editor` (draw the cycle, SPACE-drone + live morph,
  seed shapes, exports `wave_set()` code). The cart-authorable half of
  [`design/instrument-engines.md`](design/instrument-engines.md) §8.4.
- **Sound-tool carts (draw → export-as-code)** — `sfx editor` (paint 32 steps),
  `sfx generator` (sfxp/bfxr-style: 17 sliders + RANDOM/MUTATE + sfxr category buttons),
  `wave editor`. All export paste-ready C (a data array + a tiny player) — the
  decision-0003 answer to sfx authoring, zero engine banks needed.
- **Sound tripwire + `soundcheck` self-test** — the engine counts dropped requests
  (ring buffer + delayed pen) and printh-screams `[sound] WARNING … DROPPED` when sound
  calls are lost (the silent class: defines that never land, notes that never play).
  `soundcheck` cart = worst-case burst + full-API walk; run after touching `sound.h`:
  `node tools/play.js soundcheck script /dev/null --headless --frames 900 | grep "\[sound\]"`
  — silence = PASS. See [`guides/debug-harness.md`](guides/debug-harness.md) → "Sound tripwire".
- **Instrument synth** — `instr` is now an instrument slot (0–4 = the raw waves,
  unchanged; 5–15 cart-defined). Four expressive axes bundled per slot, all on the raw
  waveforms: `instrument()` (per-voice **ADSR**), `instrument_duty()` (pulse width),
  `instrument_lfo()` (**3 LFOs**/slot → vibrato/PWM/tremolo/wah), `instrument_filter()`
  (resonant SVF: low/high/band/notch). Demo carts: `instruments`, `lfo`, `filter`, and
  `dream synth` (a playable Moog-style patch panel + keyboard). See
  [`design/audio-notes.md`](design/audio-notes.md) §10.
- **Held notes** — `note_on()→handle`/`note_off()` plus live setters
  `note_pitch`/`note_vol`/`note_cutoff`/`note_res`/`note_duty`/`note_lfo`/`note_filter`/`note_glide`
  (+ `note_off_all`). A sustained voice
  you drive frame-by-frame, the opposite of fire-and-forget `note()`: hold-to-sustain,
  engine revs, sirens, theremins. Handles are `index + generation` so a stale handle
  safely no-ops; per-param slew smooths live writes (no zipper). `note()`/`hit()` keep
  their fixed-duration behavior. Demo cart: `held notes` (a theremin); `dream synth`
  retrofitted onto them (real hold-to-sustain + live filter sweep). See
  [`design/held-notes.md`](design/held-notes.md).
- **Stereo + panning** — the audio path is stereo. `instrument_pan(slot, pan)` (per-slot
  position, inherited at note-on), `note_pan(handle, pan)` (live, slewed — positional audio:
  map a sound to where it is on screen), and `LFO_PAN` (auto-pan as an LFO destination).
  `pan` is -1 left .. 0 center .. +1 right. **Linear pan law, center unchanged**: a centered
  voice is byte-for-byte the old mono mix, so existing carts are unaffected. Master soft-clip
  and the steal-declick tail are stereo-correct (peak-gain clip preserves the pan ratio).
  Echo stays a mono bus in v1; ping-pong + the effects layer's stereo width are next (§8.10).
  Demo cart: `pan`. Full design + the build-order + bite checklist: [`design/stereo.md`](design/stereo.md).
- **Input release edges** — `keyr(k)` / `btnr(player, button)`, the falling-edge partners
  to `keyp`/`btnp` (true on the frame a key/button is released). Needed for hold gestures
  (note_on on press, note_off on release).

---

## Open — prioritized

Ordered by leverage. Section refs point at the design doc that specs each item.
Which *carts* are probing these questions (and every verdict so far) →
[`design/probe-carts.md`](design/probe-carts.md); probe carts carry `"probe"` in
their `kind[]` tags.

1. **Cart-pattern helpers** — `hud()` and `game_over_screen()` cut (see Decided-against).
   - **AABB collision already SHIPPED as `boxes_touch()`** (+ `point_in_box`, `circles_touch`,
     `near`, `touching_map`, `tile_at`, `touching_color` — the full collision section;
     `bounce_at_edges` later cut for zero adoption, [decision 0014](decisions/0014-cut-unused-convenience-helpers.md)).
     *Not* a missing primitive. Open question is **discoverability, not API**: a rough
     grep finds ~30 carts still hand-rolling inline rectangle overlap even though `boxes_touch`
     exists and 15 carts use it — so the lever is teaching (a collision tutorial / converting
     example carts), and *maybe* a more-guessable alias name, not a new function. Adding a bare
     `overlap()` synonym would just grow the already-large surface (see VISION's prune note).
   - `explode()` / particle system is a **research topic** before any build: a no-param
     `explode()` risks making all carts look identical (same concern that killed `hud()`).
     Needs design work on color, shape, lifetime, and movement params first.
     See particle survey + open questions in [`design/api-notes.md`](design/api-notes.md) §C.
2. ~~**2D geometry helpers**~~ — **SHIPPED** as `ngon`/`ngonfill`, `star`/`starfill`, `poly`/`polyfill`, `thickline`, `rrect`/`rrectfill`, `vgradient`/`hgradient` (+ outline siblings `arcoutline`/`ringoutline`/`thicklineoutline` so every filled shape has a matching boundary-ring outline). Demo: `shapes.cart.png`. See [`design/geometry-helpers.md`](design/geometry-helpers.md).
   - *Parked thought (not a build item):* true smooth color interpolation (`lerp_color`/`rgb`) — splits the color model; needs its own ADR. Gradients are dithered.
   - ~~**BUG: `vgradient` renders INVERTED.**~~ **FIXED 2026-06-04** — swapped color roles in `gradient_band()` (`fillp(pat, cb); rectfill(..., ca)`). Thumbnails re-baked for trafficjam, loveparade, loopstation, shapes.
3. **Events** — `broadcast(msg_id)` / `received(msg_id)`. Confirmed demand (independently
   surfaced by the brainstorm review). Touches main-loop drain semantics.
   [`design/api-notes.md`](design/api-notes.md) §11.
4. **Pause** — runtime-owned pause overlay. (`fps()` SHIPPED — see Shipped above.)
   [`design/api-notes.md`](design/api-notes.md) §16.

   **First pass — SHIPPED** (P/ENTER opens, ESC resumes, freezes `update()`,
   mutes sound, Continue/Restart via `execv`, `paused()` API).
   **2026-06-05 hardening** (sh101's full keyboard collided with P):
   - **Key claiming** — a key the cart reads via `key()/keyp()/keyr()` is
     claimed and skipped by the pause hotkey; claims reset on libtcc hot-reload.
     A full-keyboard cart keeps P; ENTER still pauses everything.
   - `-DPAUSE_KEY` (settings → controls) is honored now — the check was
     hardcoded `KEY_P`, so the rebind never worked.
   - ENTER can actually open the overlay — the menu used to consume the same
     press as Continue in the same frame, net no-op.

   **Deferred — Options submenu (document now, build later):**
   Matches PICO-8's pause → Options screen:
   - Sound: ON/OFF
   - Volume: slider/bar
   - Fullscreen: OFF/ON (one `ToggleFullscreen()` call)
   - Controls: read-only display of current P1/P2 key bindings (rebinding stays in
     the editor settings tab)
   - Back

   **Further deferred:**
   - **Per-player pause key** — currently one shared `PAUSE_KEY` for both players.
     When gamepad support lands, each player should have their own pause button
     (P0_PAUSE_KEY / P1_PAUSE_KEY, same `-D` flag pattern as the other bindings).
     The architecture already supports it — just not exposed in the UI yet.
   - **`menuitem(index, label, callback)`** — let carts add custom rows ("restart
     level", "toggle music", "easy mode") to the pause menu. Zero layout work for
     the cart; ~30 carts currently hand-roll their own options screens.
5. **Sound expansion** — _instrument bank (ADSR/duty/LFO/filter) and **held notes**
   (`note_on`/`note_off` + live setters + slew) now SHIPPED, see above._
   **NEXT (decided 2026-06-03, built BEFORE the engines): modulation envelopes** — a second/third
   envelope generator routed to a destination, the one-shot twin of the routable LFO. One call
   `instrument_env(slot, which, dest, attack_ms, decay_ms, amount)` (+ live `note_env`), **2 dests
   to ship** — `ENV_CUTOFF` (the pluck "pew/dwow") and `ENV_PITCH` (drum punch / attack snap / zap;
   `ENV_DUTY` optional) — **2 env slots** so both run at once, **AD shape, exp decay**. Deliberately
   no `ENV_VOLUME` (= the amp ADSR). This was the missing second EG; doing it first makes every
   engine + raw wave better and frees the pluck `morph` knob to be *inharmonicity* instead of
   filter-decay. Demo carts: `pitchenv`, `filterenv`. Spec: [`design/audio-notes.md`](design/audio-notes.md) **§11**.
   Then, behind it: the **navkit rich-instrument port** (rich non-chiptune voices as `INSTR_*`
   engines, each played behind a tiny fixed 3-macro API: harmonics/timbre/morph, §8.1.1 — all
   §8.x refs here = [`design/instrument-engines.md`](design/instrument-engines.md), split out of
   audio-notes 2026-06-07, numbering preserved).
   The bite order (instrument-engines §8 status note + §8.8 port sketch):
     1. ~~**`INSTR_PLUCK`** (Karplus-Strong)~~ **SHIPPED 2026-06-05** — per-voice KS buffer (§8.2)
        made concrete, full macro surface, `pluck` showcase cart. Station retrofit (jangle/bossa)
        is the open follow-up.
     2. ~~**`INSTR_MALLET`** — buffer-free celesta / music-box voice.~~ **SHIPPED 2026-06-05** —
        4 modal sines + strike click, no buffer, `mallet` showcase cart (5 hardware presets =
        baked macro positions). First full walk of the §8.8.2 engine-shipping playbook. Open
        tail: macro taste-tuning + the lowend vibraphone retrofit.
     3. ~~**`INSTR_FM`** — 2-op + feedback, buffer-free (§12 gap **2a** only — the self-contained
        engine, NOT the deferred Juno second-osc plumbing, gap 2b). Promoted ahead of the organ
        2026-06-05: epiano/bell demand is already on the dial.~~ **SHIPPED 2026-06-05** —
        snapped ratio table, in-note brightness decay (the DX strike), feedback morph; `fm`
        showcase cart (epiano/bell/bass/brass/clang presets + a live formula scope). Design:
        instrument-engines §8.8.3. The two named risks both resolved same-day (§8.8.3
        post-ship findings): epiano tine added → nameplate test PASSED; brass fixed by
        making brightness follow the amp attack. Open tail: plain taste-tuning + the
        citypop/lowend epiano retrofits.
     4. ~~**`INSTR_ORGAN`**~~ **SHIPPED + PUBLISHED 2026-06-07** — Hammond tonewheel, 9 drawbar
        additive, `organ` showcase cart (full design instrument-engines §8.8.4). Post-ship: a
        drive-fizz fix (pre-drive HF rolloff) and the shared per-voice **DC blocker** on the drive
        path both landed. Leslie stays a per-voice recipe / future bus item (0015, §8.10).
     5. ~~**`INSTR_EPIANO`**~~ **SHIPPED + PUBLISHED 2026-06-08** — Rhodes/Wurli/Clav, 12-mode modal
        bank + pickup nonlinearity, `epiano` showcase (§8.8.5). Post-ship tuning (by ear + the
        navkit-render A/B, tools/navkit-render.c): **timbre** given a hammer-hardness tilt so it bites
        on all 3, **bark** folds in drive (clean→growl), clav has a fast filter-env quack. **Rhodes
        rebuilt from MEASURED spectra (2026-06-08, second pass)** — the by-ear body/bell split (old
        `RHO_*` consts) still left a loud sustained inharmonic 4.2× partial that read as an "untuned
        bell"; FFT of our own render confirmed it. Replaced `RAT[0]` with the real structure
        (harmonic body 1-6 + sparse fast inharmonic tine bells 6.27×/17.55×/34.4×) + per-mode `DEC_R`
        + a 2nd-harmonic "voicing" crossfade (Shear 2011 / Münster & Pfeifle 2020), FFT-verified.
        Added a **tremolo** slider (suitcase/Wurli amp wobble). **Wurli** then got its own A/B pass
        (2026-06-09): boosted the OCTAVE partials (2,4,8,16) over the reedy 3rd — the 200A's
        fuller/punchier tone (Reed200 spectral-model crib), FFT-checked + ear-confirmed. **Clav** is
        still the navkit crib (a near-harmonic struck string — plausible, but not reference-validated
        like Rhodes/Wurli; candidate for a future pass). The per-voice **wah is provisional** (flagged TEMP! in-cart)
        — the real auto-wah is a bus effect (§8.10). 🅿️ see PARKED below.
     7. ~~**`INSTR_MEMBRANE`**~~ **SHIPPED 2026-06-08** — struck drumhead, 6 modal sines, buffer-free,
        `tabla` showcase (5 kit presets + drumhead viz). Ported from navkit's `processMembraneOscillator`
        with its live circular-membrane strike-position weighting (timbre) and monotonic pitch-bend chirp
        (morph); the one deviation — harmonics also crossfades the *ratios* tuned-harmonic↔Bessel (tabla
        really is harmonic), not just the amps. Hand percussion the 808/909 sine+pitch-env can't reach
        (instrument-engines §8.5 step 8). (PD shipped separately, item 6.)
     ~~Next: reed → pipe → buffered piano/guitar pair → bowed~~ **ALL SHIPPED 2026-06-09** — the full
     wind + string families are done (`INSTR_BOWED` = violin/cello, arco *and* pizzicato from one
     waveguide; the modeled string family pluck→guitar→piano→bowed is complete). **And `INSTR_BRASS`
     (29) SHIPPED 2026-06-10** — the lip-reed brass family (trumpet→tuba), the LAST engine-blocked
     instrument; STEP-0 found the literal lip mass-spring won't self-oscillate, so it's reed's
     pressure valve + a dynamics-swept brass formant (the blatty "blaaat"); showcase = the `brass`
     cart's trombone slide (§8.8.10). Now **no instrument is engine-blocked.** Next:
     **formant + effects layer.** Additive stays deferred
     (`INSTR_SINE` + FM + MALLET cover its near corners; the MT70 family is its first named customer).
   The **MT70 finding, corrected 2026-06-07** (the 2026-06-03 "all one pure sine" verdict was a
   verification artifact — the songs' `osc2*` fields sit ~50 lines into each block and are
   non-zero): the presets are **2–3 mixed sines with per-partial decay + click** — small
   additive. Conclusion unchanged, better reasons: *no engine port* — the struck half is
   `INSTR_MALLET` territory, exact = 2-slot voice stacking, single-voice ≈ SCW + closing
   `ENV_CUTOFF`; ships as demo/preset carts (instrument-engines §8.9 corrected note).
   Also still open: zero-setup **preset instruments** (`INSTR_PLUCK`/`PAD`/…) and cart-side
   **SFX authoring** (the sfx bank is hardcoded today; *pattern* authoring is settled-no —
   `music()` cut, [decision 0013](decisions/0013-cut-music-api.md)) — *direction 2026-06-04: prototype
   as a PICO-8-style editor CART first (draw the pitch contour, toggle per step), zero new
   engine API — hit()/schedule() + beat clock for playback, save_bytes for persistence,
   export-as-C-code into games; `sfx_def()` only if the prototype proves the engine should
   own banks. See audio-notes §5.6 direction note.*
   ⚠️ The port touches `runtime/sound.h`/`studio.c` — shared with the live/libtcc runtime work;
   sync before starting. [`design/audio-notes.md`](design/audio-notes.md) §5–8, [`design/held-notes.md`](design/held-notes.md).
   🅿️ **PARKED — revisit when the effects-bus layer (instrument-engines §8.10) lands:** the
   per-voice wah (epiano AUTO/TOUCH flavours) and the **envelope follower** (`instrument_follow`/
   `note_follow`) are *interim* — the realistic "woah woah" auto-wah is a BUS effect and will
   likely replace them; the follower's real home is bus-level. Kept (may be handy) but flagged so
   we don't build more on them. When §8.10 is built: reassess whether to fold these into the bus
   wah or remove. Full context: [`design/instrument-engines.md`](design/instrument-engines.md) → §8.10.1 PARKED.
6. **Sprite flags** — `fget`/`fset` (per-sprite 8-bit flags; 256 bytes). Pairs with an
   8-checkbox row in the sprite editor. [`design/api-notes.md`](design/api-notes.md) 2026-05-30 review.
7. **Gamepad** — `gp_axis(slot, axis)`, `gp_present(slot)`, internal `btn()` augment.
   [`design/api-notes.md`](design/api-notes.md) §15. *Groundwork landed via touch controls
   (2026-07-01):* `touch_layout(mode, n_buttons)` is the same "declare this cart's logical
   controls once, feed every input source" idea this item wants, and the button vocabulary grew
   from A/B-only to `BTN_A/B/X/Y` (numbered to append further) specifically so a real pad's face
   buttons have somewhere to land — see [`design/touch-controls.md`](design/touch-controls.md)
   "Synergy with the gamepad item."
8. **Strudel extras / Dilla groove timing** — `pitch`, `sometimes`/`often`/`rarely`,
   `arp`, `stutter`, `palindrome`, `off_beat`; `groove` + `groove_swing/jitter/push`,
   `tick`/`PPQ`. [`design/api-notes.md`](design/api-notes.md) §13–14.
9. **Per-game polish pass** — title/game-over screens, hi-scores, sound on every event,
   juice, difficulty curves, readable HUDs. (Reframed as a reference idea bank, not an
   active backlog — see [`POLISH_TODO.md`](POLISH_TODO.md).)
10. ~~**Browser URL-sharing**~~ — **SHIPPED** (2026-06-05, eleventh pass): the whole catalog
   is playable at <https://nikkikoole.github.io/dreamengine/> (263 carts), publish is one
   command (`tools/publish-cart.sh`) or the editor's 🚀 button. The guide's "what's missing
   is a URL" is resolved — [`guides/sharing.md`](guides/sharing.md) carries the SHIPPED
   banner. *(Struck 2026-06-07 — the item lagged the eleventh-pass ship by two days.)*
11. **iPad runtime** — touch is wired in the runtime; needs a build path. [`VISION.md`](VISION.md).
    *Product lens (2026-06-07):* if the tinyjam racks become a paid product, this item is
    the cash register — iOS is where music-app buyers live. Deliberately **waits for
    evidence** (a rack holding people's attention on the free web gallery) per the
    sketch-first decision in [`design/product-notes.md`](design/product-notes.md).
    *Update 2026-07-03:* the build path largely exists (8/9 [`design/ios-plan.md`](design/ios-plan.md)
    spikes ✅); the live map of what's still missing to the store is
    [`design/sharing-channels.md`](design/sharing-channels.md) §Channel B (product decision,
    palette, submission pipeline — the latter decided in-house, not Fastlane:
    [ADR-0026](decisions/0026-store-pipeline-in-house-not-fastlane.md)).
12. **Sound tracker UI** — open question; depends on whether code-first sound proves
    sufficient. *Direction 2026-06-04: leaning PICO-8-style, prototyped as a CART with
    zero new engine API (see #5 + audio-notes §5.6) — the cheap way to find out if the
    editor earns a place before any engine-side bank API exists.*
    [`VISION.md`](VISION.md), [`design/audio-notes.md`](design/audio-notes.md) §5.6, §9.

13. **Baked rotation atlas** *(exploratory — full design note, not yet started)* — an
    offscreen-canvas primitive (`make_canvas`/`target`/`blit`) plus pre-rotated sprite/shape
    "stamps" so bodies are fast translate-blits instead of per-frame rasterization (for
    crowds, rich shapes, low-end). Centerline/pivot model, `pal()` recolor for free color
    variety, parts capped at 16px (native slot size). The path to scaling the `bones`
    animator past realtime drawing. [`design/baked-rotation-atlas.md`](design/baked-rotation-atlas.md).
    - *Reframe (from the Picotron manual):* Picotron collapses "sprite sheet," "offscreen
      canvas," and "raw memory" into ONE primitive — `userdata(type,w,h)`, a typed 2D buffer
      that sprites/map/screen all are. Suggests the primitive here should be **general**, not
      rotation-specific: a draw target you can render into, read/write per-pixel (`sset`/`sget`),
      and stamp — the offscreen canvas, the rotation cache, and runtime sprite editing are then
      the *same feature*, not three. Worth designing the buffer primitive first; the rotation
      atlas becomes one use of it. (Still index-only — no RGB/alpha, unlike Picotron's userdata.)
14. **Rasterization consistency** *(SHIPPED — every filled primitive on one coverage path)* —
    every filled primitive now shares one pixel-center coverage definition, so the outline is
    exactly the boundary of the fill (no rasterizer drift, dither = solid path):
    `rect`, `circ`/`oval`/`rrect` via `disc_inside`/`ellipse_inside`/`rrect_inside`;
    `tri`/`trifill`, `quadfill`, `ngon`/`star`/`poly` via even-odd `poly_inside` (concave-safe,
    winding-independent; `trifill_pat` deleted); `arc`/`arcfill`/`ring` via `sector_fill` (same
    pixel-centre disc); `thickline` via a capsule coverage (was `quadfill`+caps — the test found
    a 1px seam crack from a `w*0.5` body vs `w/2` cap mismatch). `trifill` is now CPU per-pixel —
    3D carts (`solid3d`/`cube3d`) smoke-tested OK; solid3d's face hairlines gone.
    Detector rewritten to a global invariant (outline == boundary of `fill ∪ outline`): catches
    a 1px offset at any angle, never false-flags sharp tips; verified to have teeth (GPU tris →
    282). Open strokes verified by a 4th-page equivalence self-test (ring==annulus, sector-tiling
    ==disc, thickline solid). Verified: all 11 marker states (3 pages × 4) + 3 equivalence checks
    = 0.
    **Still open (verification, not design):** ~~perf of CPU `trifill` vs old GPU~~ **measured
    (2026-06-01, `podracer`): the cost is real.** CPU `trifill` froze podracer when its haze
    spammed ~190 large software-filled tris/frame; fixed cart-side by moving bulk fills to GPU
    (`rectfill`/`line`). **Off-screen bbox clamp — SHIPPED (2026-06-02).** `poly_fill_cov`/
    `poly_stroke_cov` now intersect their scan box with the on-screen region before scanning,
    mapped through the camera (`GetScreenToWorld2D` on the four screen corners → conservative
    AABB, so translate/zoom/rotation are all correct and the image is byte-identical). Huge
    off-screen tris no longer iterate their full bbox doing point-in-poly tests on cells that
    plot nothing. Verified output-identical: `raster_test` reports `mismatches:"0"` on all 46
    analyse frames + `eq total=0`.
    **It's a performance-cliff guard, not a general speedup** — the cost of a software polygon
    now scales with its *visible* area, not its *total* area. The effect tracks how far a
    poly spills off-screen, so it ranges from huge to nil (all measured with the profiler):
    - `trifill_stress` (synthetic: 12 thin spokes reaching ~1100px off-screen) — **46.7 →
      2.7ms/frame (~17×), ~21fps → 60fps.** This is the worst-case win, not a typical one.
    - `solid3d` (real 3D, model fits the screen so faces only spill a little) — 3.18 → 3.02ms
      avg (~5%), 4.6 → 3.9ms **peak (~15%)**. Modest; the gain is on the frames a face is
      largest.
    - `podracer` — **no effect** (0.81 → 0.75ms, noise): it draws zero software polys (haze
      already on GPU `tritex`/`line`/`rectfill`), so there's nothing on this path to clamp.
    Takeaway: existing well-behaved carts don't get visibly faster; the value is that a future
    cart flying the camera into a `quadfill`/`trifill` surface degrades gracefully instead of
    freezing (the cliff `podracer`'s author had to hand-work-around). *Per-call overhead* is 4
    `GetScreenToWorld2D` (a matrix inverse) — negligible at observed call counts (solid3d got
    faster, not slower); if a cart ever draws thousands of tiny on-screen polys/frame, cache
    the clamp box once per frame (invalidate in `camera()`/`camera_ex()`) instead of per-call.
    Still open: web GL ES confirmation (`pget` disabled on web); an ADR for the GPU→CPU
    `tri`/`trifill` + `thickline` behaviour change.
    **Regression test:** `tools/carts/raster_test.c` + `tools/raster_test.script` —
    drag `editor/public/carts/raster_test.cart.png` into the editor (Z outline, X dither,
    C cycle 4 pages, SPACE analyse / run equiv), or run headless:
    `node tools/play.js raster_test script tools/raster_test.script --headless --trace build/raster_trace.jsonl --frames 60`
    then check every `fs=2` frame reports `mismatches:"0"` and the `eq` line shows `total=0`.
    **Perf-regression test** (the bbox clamp): `tools/carts/trifill_stress.c` — a pinwheel of
    thin off-screen tris. In the editor it should hold 60fps even with reach cranked to max; if
    the clamp regresses, pushing reach tanks the fps. It runs `raster_test` for correctness and
    the ⏱ profiler for the budget (was ~46.7ms unclamped, ~2.7ms clamped at the defaults).
    [`design/rasterization-consistency.md`](design/rasterization-consistency.md).
15. ~~**Tiny fonts**~~ — **SHIPPED** as `font(FONT_SMALL/FONT_TINY)`. See Shipped above.
16. **Packaging & public distribution** *(not started)* — dreamengine only runs as a dev
    build today. A dev-only icon + app name stopgap landed this session (the running app was
    a generic "Electron"); real packaging (bundler, `.icns`, code-signing/notarization, load
    the built renderer instead of `localhost:5173`) is unaddressed. The actual blocker isn't
    Electron — it's that the ▶ run model shells out to a developer's `clang` + Homebrew raylib,
    which a consumer machine doesn't have; web/wasm is the likely public path. Full breakdown:
    [`design/packaging-distribution.md`](design/packaging-distribution.md). Related: browser
    URL-sharing (item 10), iPad runtime (item 11).

17. **Frame-spanning sequence scripts** *(idea — from the Picotron API comparison; needs design)* —
    the *useful half* of Lua's `yield`/coroutines: write time-based logic (cutscenes, scripted
    AI, juice sequences, dialog) as **linear top-to-bottom code** — `walk_to(100); wait(30);
    say("hi"); wait(60); …` — instead of a `switch(state)` shredded across `update()` calls.
    C has no native coroutines, but the pattern is emulable with **protothreads** (Dunkels'
    switch/case macro): stackless, so locals that must survive a `wait` go in `de_state()`.
    **Distinct from the cut "process / coroutine model"** below — that was a full DIV-style
    Level-2 *execution model* (every entity its own process); this is a *small optional helper*
    for sequencing, not a new way to structure carts. Open question is whether a macro trick fits
    dreamengine's "readable C" ethos, or whether it's better shipped as a documented example cart
    than as core API. Worth prototyping one `sequence`/`wait` helper to feel the ergonomics.

18. **Blend tables** *(idea — from the Picotron manual; the substantive capability gap)* —
    index-only translucency/fog/tinting/additive via a precomputed lookup `result = blend[src][dst]`,
    staying entirely in the 32-color palette (**no RGB, no real alpha** — just a table that says
    "drawing color `a` over existing color `b` resolves to `c`"). Unlocks the things carts fake
    with `fillp` dither today: translucent water/glass, fog, additive glows, drop shadows. This is
    a real *capability* dreamengine lacks — `pal()` swaps and `fillp` are the closest, neither
    blends with what's already on screen. Deliberately framed as a lookup table so it does **not**
    trip the "splits the color model" concern flagged on the `lerp_color`/`rgb` parked thought
    (item 2) — the output is always a palette index. Picotron pairs this with stencil clipping;
    that's a separate, lower-value follow-on. **Design note now exists →
    [`design/blend-tables.md`](design/blend-tables.md)**, and the concept is **validated in
    cart-space**: the `blend lab` tech-demo (`tools/carts/blendlab.c`, 2026-06-04) builds
    AVG/ADD/MUL tables and blends per-pixel against a cart-owned scene fn, zero engine API.
    Verdict: the look works (additive glow / glass / fog all read correctly, in-palette), and
    the engine crux is identified — dst must be read from the *in-progress* frame (a `pget`
    last-frame read feeds back and blooms; demonstrated by the cart's `P` mode). Candidate
    implementation: shader + per-scope canvas snapshot, the decision-0007 lane. Next step: ADR —
    **after the palette decision**: blend tables are computed *from* the palette, and the default
    palette (lifted verbatim from PICO-8) should become our own / settable first, or #18 bakes the
    borrowed palette one layer deeper. See [`design/palette-and-color.md`](design/palette-and-color.md)
    (Picotron findings + three-layer plan: new default → `palette_set` + `de:palette` chunk →
    tables-from-active-palette).

19. **Per-cell parameter locks in the cr-78 cart** *(cart-space idea, zero engine API — parked
    2026-06-05)* — Elektron-style p-locks for `tools/carts/cr78.c`: each lit step optionally
    carries a pitch offset (melodic bongos/congas/metal riffs, TR-727 style) and a cutoff
    override (hats opening across the bar). Historically cheeky on purpose: CR-78 voices (1978)
    + the cart's swing knob (LM-1, 1980) + p-locks (Machinedrum, 2001) in one box. Pitch is
    trivial (`fire()` already passes midi). **The known crux is the filter**: one-shot cutoff
    lives on the instrument slot and scheduled notes snapshot their slot at fire time
    ([`design/audio-notes.md`](design/audio-notes.md) §2.2) — per-step cutoff therefore needs
    the sfx-editor **rotating scratch-slot pattern** (cr78 uses slots 9–24; 25–31 free, pool of
    2-3 suffices at one-step lookahead). UI sketch in the cart header: hover+wheel = pitch,
    C+wheel = cutoff, notch markers on the 9×7px cells. Full design notes at the top of
    `tools/carts/cr78.c`.

20. ~~**TB-303 bassline cart**~~ — **SHIPPED same-day 2026-06-05** as `tools/carts/tb303.c` /
    `tb303.cart.png` ("parked for another time" lasted about an hour — user asked for it).
    Exactly as sketched: one `note_on()` voice, `note_glide(60)` slides that don't refire the
    filter envelope (authentic to the circuit), accent = vol 7 + env amount × ACC knob,
    staccato gate at 70% of step, five mouse-draggable knobs with CUT/RES applied live to the
    ringing voice (`note_cutoff`/`note_res`), saw/square switch, mouse piano roll with
    OCT/ACC/SLD flag rows, and an N-key random acid-line generator (root-heavy minor
    pentatonic). The classic-machine family is now cr78 + tr808 + tb303.

21. **More classic boxes — the museum shortlist** *(cart-space, zero engine API — parked
    2026-06-05; the family so far is cr78 + tr808 + tb303 + sh101 + tr909, all in
    `tools/carts/`)*. Curated by
    API fit; the curatorial line is **analog-circuit machines only** — sample/tape boxes
    (LinnDrum, DMX, SP-1200, Mellotron) would be caricatures since the engine has no sample
    playback. Ranked:
    - ~~**TR-606 Drumatix (1981)**~~ — **SHIPPED 2026-07-06** as `tools/carts/tr606.c` /
      `tr606.cart.png` ("an afternoon" held). Metal bank at the circuit-analysis frequencies
      (246.4/308/367/418.2/440.4/627.2 Hz, the beating 418+440 pair), kick as the schematic's
      DOUBLE twin-T with a TONE crossfade, single-mode snare, tempo-linked open-hat decay +
      the shut-off circuit as `instrument_choke`. One measured infidelity, documented in the
      cart header: the metal paths use highpasses, not the schematic's 7100/3440Hz bandpasses —
      with 2 bank members where the circuit sums 6, a narrow band starves between the sparse
      odd harmonics and a wide one leaks fundamentals (Goertzel-probed). Stock-austere panel
      (accent only) with the mod culture as a MODS toggle. The Roland wing is now complete.
    - ~~**TR-909 (1983)**~~ — **SHIPPED 2026-06-05** as `tools/carts/tr909.c` / `tr909.cart.png`.
      Analog kick/snare/toms/rim/clap as assessed (house kick = +30st/35ms swept sine + a click
      layer on the ATTACK knob). The ROM-sample hats/cymbals got a BETTER workaround than the
      predicted SFX-editor one: **`INSTR_FM` on the 3.5 inharmonic clang detent** (harmonics
      0.55, feedback cranked) through a highpass whose cutoff starts ~5kHz low and rises via a
      **negative `ENV_CUTOFF` amount** — the fast-closing sizzle of a sampled hat, synthesized.
      (Same-day instrument-engines §8.8 insight applied: inharmonicity reads as metal; the engine clamps cutoff
      after env addition, so negative amounts are safe.) Swing-knob saga complete: the 909
      shuffle is period-correct at last (even 16ths drag — audio-notes §14 still carries the
      pre-build assessment). Six presets (Good Life, The Bells, Energy Flash, Hardfloor,
      Revolution 909, Gabber); closed hat chokes open via `instrument_choke`. **The
      classic-machine family (cr78 + tr808 + tb303 + sh101 + tr909) now covers the full
      ReBirth RB-338 rack** (303 + 808 + 909). Same-day follow-up: **FLAM shipped** after
      all ("the one panel feature not modeled" lasted one message) — and grew into a
      **stroke cycle**: right-click cycles plain → flam (1 grace, 22ms early) → drag
      (2 graces) → ratchet (4 even hits chopped across the step — post-909, but the
      techno fill); `'f'`/`'d'`/`'r'` in preset rows, cells draw their strokes as ticks,
      Hardfloor flams its claps + ends the hat row on a ratchet, Gabber drags its snare. Plus a deliberate impurity by ear-feedback: a
      **metal-filter XY pad** (cut × resonance, `instrument_filter` re-aimed live across
      all five FM/noise metal slots) because the FM hats landed bright and hissy —
      defaults nudged fuller than the first build. Sound tripwire: PASS (700 headless
      frames on Hardfloor incl. flams + pad clicks, no drops).
    - **EKO ComputeRhythm (1972)** — Jarre's punch-card programmable box; UI gimmick = draw the
      punch card. Pre-dates the CR-78's "first programmable" claim by six years.
    - **Wurlitzer Sideman (1959)** — the FIRST rhythm machine: tubes + rotating contact wheel;
      UI = a circular playhead instead of left-to-right. Oldest piece by two decades.
    - **Casio VL-1 (1979)** — "Da Da Da"; calculator keys + the 8-digit ADSR number code you
      literally type to design a sound.
    - **Maestro Rhythm King (1970)** — Sly Stone's funk preset box; simpler/weirder than CR-78.
    - **Stylophone (1968)** — mouse-as-stylus, ~20 lines, instant Bowie.

    **Pre-Roland wing — Raymond Scott** (Manhattan Research Inc., the basement where all of
    this started; Bob Moog sold him circuits in the '50s):
    - **Circle Machine (~1959)** — strongest candidate of the whole list: a RING of bulbs, each
      with a brightness knob, swept by a rotating photocell arm — a step sequencer a decade
      before the word. Cart: circular sequencer, drag bulb brightness (= pitch/volume per
      step), rotating playhead instead of left-to-right, rotation speed = tempo. `euclid()`
      would feel period-appropriate. Visually unlike every other music cart in the studio.
    - **Clavivox (1956)** — keyboard theremin with portamento; the great-grandfather of the
      tb303 cart's `note_glide`. Could be a small mouse-played instrument cart.
    - **Electronium (1959-70s)** — not an instrument, a collaborative composition machine you
      NUDGE ("faster", "more like that"); Motown bought one and hired Scott as electronic R&D
      director. Already has descendants here: the bossa/ambient/jangle radio carts. A cart
      that adds the nudge-interface to a generative engine would close the circle.

> `tritex` (affine textured triangle) shipped in session 8 — it was Open here; now in the API.

**Smaller open items (no design doc yet):** looping ambience (`drone`)/`volume`/mute. Noted
in [`POLISH_TODO.md`](POLISH_TODO.md).

**Noise is value-noise + the seeding idiom** *(new 2026-06-09, surfaced building the
`procplaces` procgen testbed)*. The engine's `noise`/`noise2`/`noise3` (`studio.c:2911`) is
**value noise, not Perlin** — it hashes lattice *values* and smoothstep-interpolates them
(Perlin interpolates *gradients*), so it has faint axis-aligned grid artifacts at low
frequency. Fine for terrain/density/fog; worth knowing before leaning on it for anything
needing isotropy. Crucially it **is seedable today**, even though `noise`/`noise2` take no
seed: `noise2(x,y)` == `noise3(x,y,0)`, so **`noise3(x, y, (float)seed)` with an INTEGER seed
is a fully-independent, repeatable field** (the z-slice folds through `noise_hash`
independently; an integer z = no interpolation bleed). The `procplaces` cart uses exactly
this (both its generators) and verifies byte-identical re-renders. **Open question:** add a named seeded
helper (`noise2_seeded(x,y,seed)`?) and/or document the `noise3`-as-seed idiom + the
value-vs-Perlin caveat in `studioDocs.js`, so the next author doesn't conclude "can't seed it."

**`sprite-draw.js` next steps:**
- ~~Gap audit~~ **DONE** — `ovalfill`, `rrectfill`, `ngonfill`, `polyfill`, `noise` added (2026-06-04).
- **Remaining Tier 2** — `starfill`, `arcfill`, `thickline`, `vgradient`/`hgradient`, `bezier`. Lower priority; current set covers most sprites.
- **JS-only extras** — `hflip`/`vflip`, `rotate90` (reuse one sprite in 4 orientations). Also: migrate terrain-tile carts (cannonfodder, druglord, heroes, hotline, etc.) to use `noise()` instead of their copy-pasted `(x*a + y*b) % m` patterns.
- **Stress-test cart** — a cart exercising all primitives as a visual reference + regression guard.

23. **The sprite story — two sprite sources of truth** *(new 2026-06-06, surfaced by the
    editor publish button but it already bites without it)*. A cart's sprites can come from
    (a) the **sprite editor's canvas** (exported as `build/sprites.png` on every run — what
    you see is what ships) or (b) a **`.cart.js` generator** (ASCII art / sprite-draw.js
    programs, rebuilt by `make-cart.js`/`build-site.js` at bake time). They do not know
    about each other: repaint a generator-cart's sprites in the editor and your pixels ship
    in that build but the generator still owns the repo truth — the next CLI bake silently
    reverts them. Same applies to plain sprite touch-ups: there is no path from editor
    pixels back to `tools/carts/<name>.cart.js`. Options to explore: a pixels→`.cart.js`
    exporter (slot arrays, lossless), an explicit per-cart marker for which source owns
    sprites, or a publish-time conflict warning (the editor's publish log already warns
    when a `.cart.js` exists). A fourth option — a **patch/overlay layer** (diff your
    hand-edits against the generator output, store the diff, apply it after each bake;
    fingerprint-stale patches discard, blessed ones promote into source) — is the
    human-in-the-loop answer; all four are weighed in
    [`design/editor-cart-workflow.md`](design/editor-cart-workflow.md) §Gap 2 (options
    A–D). Until one ships: generator carts should get sprite changes in the generator,
    hand-drawn carts in the editor — never both.
22. **Mobile-web readiness** *(new 2026-06-05, born from the live gallery + first
    real-device session)* — desktop-authored carts meet phones now. Shipped from
    the worklist (both 2026-06-05): ~~`tools/mobile-lint.js`~~ static checker
    (verdicts rank by *best* phone-usable input path; first `--site` run over 50
    carts: 3 touch-ready, 34 tap-as-mouse, 5 fixable, 2 keyboard-only, 6 no-input)
    and ~~gallery input badges~~ (`build-site.js` requires `lint()` and stamps a
    colored chip per card — 🟢 Mobile Ready / 🟡 Mostly Playable / 🟠 Rough on
    Mobile / 🔴 Desktop Only; strict: any dead input path demotes, hover/wheel
    cores rank rough, and a hand-tested `"mobile":` field in index.json
    overrides the lint; `fixable` shows as desktop-only until touchControls
    lands). Still
    open: a per-cart `fit` setting (letterbox / stretch / integer-scale) flowing
    `.cart.js` → `de:settings` → shell (radios are the first `"stretch"`
    customers), the **device-facts trio** (`touch_available()` grown into
    `touch_available`/`device_class`/`touch_ceiling` — researched 2026-06-06
    incl. the iPad-pretends-to-be-a-Mac detection traps and the
    ceiling-safeguard pattern against the iPhone 6th-finger mass-cancel;
    **`touch_ceiling()` SHIPPED same day** — shell computes `Module.deTouchCeiling`,
    EM_JS read, 4-place wiring, touchpiano header prints "max N fingers";
    `touch_available`/`device_class` still open, design in mobile-web-notes §5),
    and the formal touchlab-on-iPhone
    checklist run (>5-touch assumptions — iPhone Safari caps at ~5 simultaneous
    touches, found on-device; tiny tap targets). Full design + device findings:
    [`design/mobile-web-notes.md`](design/mobile-web-notes.md).

24. ~~**Web phantom touch point**~~ — **CLOSED 2026-06-06, same day as filed**: root-caused,
    fix BUILT & DEVICE-PASSED (touchlab/multitouch/touchpiano via the live gallery), and the
    rebuild tail cleared by the whole-catalog publish (all 263 carts carry the fixed engine).
    **Same-day sequel (iPad play session): tap-as-mouse death** — taps stop registering in
    mouse-driven carts until reload; emscripten GLFW's `primaryTouchId` latch sticks when iOS
    drops a `touchcancel` (WebKit 153064). Fixed with the same medicine: on web, once a real
    touch is seen the mouse is synthesized from the touch mirror (`web_tm_*` in `studio.c`),
    GLFW's emulated mouse never read again. Touch-notes **device finding #6**.
    Spawned **open item 27** (web debug overlay) — three on-device mysteries in one iPad
    afternoon with zero cable-free visibility.
    Kept for the record:
    on web builds a lifted finger's contact can stay in the touch list (most reliably when
    two fingers release at once); native is clean. Cause: emscripten#4679 (`wontfix`,
    touchend conflates remaining+lifted touches) stacked on raylib's
    one-removal-per-touchend (5.5 *and* master). Fix: own the touch truth on web — a JS
    mirror rebuilt from spec-correct `event.touches` in `web_shell.html`, read by
    `poll_virtual_touches()` via EM_JS; rebuild-don't-decrement is immune by construction.
    Acceptance: touchlab two-finger simultaneous-release drumming → `touch_count()` hits 0
    every time. Then the gestures.h follow-ups (two-finger **pan vs pinch** classifier +
    id-keyed pinch pair — also why rgestures reads any 2-finger drag as pinch/zoom).
    Root-cause chain, issue links, full plan: [`design/touch-notes.md`](design/touch-notes.md)
    **§7–8**. Blocks the capture model of item 25 on web.

25. **`ui.h` — cross-input widget kit** — **v1 SHIPPED 2026-06-07** *(designed 2026-06-06;
    the item-24 web blocker closed in between)*. `runtime/ui.h` (gestures.h pattern, zero
    engine API): **button / slider / knob** for mouse + touch + keyboard/gamepad at once —
    per-contact capture table (two fingers = two knobs), value-address identity (buttons:
    rect-only — sfxgen's dynamic `str()` label broke pointer identity), hit-pad inflation
    to mobile-lint's ≥24px with **deferred press resolution** (presses collected in
    `ui_begin`, routed at `ui_end`; a visual-rect hit beats an inflated one, so 17 sliders
    at 9px pitch still route correctly), opt-in marching **focus ring** (arrows traverse,
    LEFT/RIGHT adjust with hold-accel, A activates — written on `btn()` so it inherits
    gamepad the day item 7 lands), and `ui_grabbed`/`ui_released` events timed so a cart
    can snapshot undo state before the press-jump lands. Shipped per the second-customer
    rule: **`uikit`** showcase/probe cart (knobs play a synth voice, sliders drive a ball
    pit, FOCUS toggles keyboard driving) + the **`sfxgen` retrofit** (17 sliders + 11
    buttons → widgets, hand-rolled drag machine deleted; behavior-faithful incl. undo —
    scripted-replay verified). mobile-lint now **inlines runtime/ library headers** before
    scanning, so all-ui.h carts rank touch-ready by construction (the notes' §5.4
    contract). **Cut from v1: panel + drag-from** — the per-widget second-customer rule
    found their named customers speculative; they wait for a real cart that wants them.
    **Still open:** the on-device probe run (two-knobs-at-once, fat fingers, 5-touch
    ceiling) and further retrofits (modrack knob rows, sfxed, wave editor). Design + §7
    build learnings: [`design/ui-widgets-notes.md`](design/ui-widgets-notes.md).

27. **Web debug overlay** *(designed 2026-06-06; **v1 SHIPPED 2026-06-07**)* — cable-free
    on-device visibility for wasm carts: `?debug=1` or **triple-tap the top-left corner**
    overlays live touch rings straight from `Module.deTouches` (a ring that stays after
    lifting = phantom; rings the game ignores = bug is past the touch layer), a
    printh/console mirror, `window.onerror` red lines, fps + the device's touch ceiling.
    Built per the §6d architecture rule (shell tweaks cost a 263-cart rebuild — learned
    twice on 2026-06-06): the shell bakes only a ~25-line loader; ALL overlay logic lives
    in one site-root `debug-overlay.js` (source `runtime/debug-overlay.js`, copied by
    `build-site.js`) — future overlay iteration is a one-file republish, zero rebuilds.
    **Still open (v2):** the cart's `watch()` values pushed per frame via EM_JS so the
    native watch-workflow works on a phone; the `web_tm_*` mouse-synth state readout.
    Zero-code alternative for deep dives: iPad + cable + Mac Safari remote Web Inspector.
    Design: [`design/mobile-web-notes.md`](design/mobile-web-notes.md) §6d.

26. **Editor hand-editing workflow** *(new 2026-06-06 — explored, sliced)* — three gaps when
    a human edits carts in the editor instead of via `tools/carts/` + CLI: (a) **no
    save-in-place** — every save is a Save-As dialog (`currentCartFile` only feeds the
    publish slug); fix = track the loaded cart's origin path, Cmd-S writes back (gallery
    carts excluded — shared registry + build products), keep the existing thumbnail. (b) the
    **sprite story** = item 23; lean recorded: ownership marker (`spriteSource:
    "editor"|"generator"`) now, lossless pixels→`.cart.js` exporter for hand-drawn carts
    later. (c) **gallery metadata**: descriptions CSS-clamped to 3 lines with no way to read
    the rest, and no UI to author `index.json` entries — fix slices: full-description detail
    view (no hazard), then a metadata form that emits a paste-ready index.json entry
    (registration deliberately stays a CLI act — the shared-registry rule holds); the
    self-describing `de:meta` chunk + generated index is parked as a direction. Proposals +
    priority table: [`design/editor-cart-workflow.md`](design/editor-cart-workflow.md).

28. **Library headers hard to find inside the editor** *(new 2026-06-07, surfaced by the
    ui.h ship)* — **slice (a) SHIPPED same day, and the move it implied got made too.**
    The fix turned out cleaner than the original sketch: rather than bolt a "library
    headers" list onto the API reference, the **read-only engine-source viewer moved out
    of its code-tab overlay and into the docs tab** as a dedicated **"engine source"
    sidebar group** (`studio.h` · `ui.h` · `gestures.h` · `improv.h` · `radio.h` ·
    `sound.h` · `studio.c`) — one viewer, two entry points: browse the group, or
    cmd-click an `#include "x.h"` in your cart and it switches to the docs tab and opens
    the same view. This **deleted** the code-tab overlay (`#viewer-overlay` HTML/CSS, the
    ✕/Esc close path, and `outline.js`'s `setOutlineOverride` prototype-scan machinery —
    the cart outline no longer has to yield to a previewed file). `editor/src/navigate.js`
    rewritten to a `showEngineFileIn(container, file)` mounter the docs tab calls; the
    code tab is now always, only your cart. (Reasoning for the move: the headers
    *belong* with the docs, not floating over your code; the docs tab already had the
    sidebar + chrome, so the viewer cost ~5 lines of new wiring and removed an overlay.)
    **Still open:** (b) autocomplete offering `ui.h`/`gestures.h`/… inside an
    `#include "` quote; (c) *maybe* the starter cart mentioning the lane in a comment.
    Function-level autocomplete/hover for header symbols stays deliberately out — keyed
    to `studioDocs.js` and the four-places contract, which is engine-API surface only;
    the header's top-comment manual is the doc, by the lane's contract.
    *(Editor change — needs a dev-server restart + a human visual pass to confirm the
    viewer renders and the day/night theme follows; built and bundle-verified, not yet
    eyeballed live.)*

29. **Sub-pixel camera — thin features shimmer when panning at fractional zoom** *(new
    2026-06-09, surfaced by procplaces' zoomable world)* — `camera_ex(int x, int y, float
    zoom, float angle)` takes **integer** world coordinates. At a fractional zoom (e.g.
    0.55×) a smoothly-moving camera can only snap to whole world units, and 1px features
    (road curbs, dashes, grid lines) land on different fractional screen pixels each frame
    as the world scrolls — so they crawl/shimmer while panning. Static, they're fine. Same
    *class* of problem as sloop's bright-curb jitter (commit fe5553f), but that was a
    velocity-lead wobble (fixed cart-side by low-passing the lead); this is the deeper
    integer-camera limitation underneath. **Cart-side workaround in place** (procplaces):
    suppress thin markings below ~0.55× zoom — wide solid fills are pixel-stable, and the
    detail is invisible zoomed out anyway. **A real fix is engine-level:** give the camera
    a sub-pixel float position and snap world→screen to the zoom pixel grid (so `zoom *
    cam` stays integer), or offer a `camera_exf(float x, float y, …)`. Until then any
    cart drawing thin lines in a zoomable/pannable world will shimmer at non-integer zoom.
    Probably belongs with the rasterization work — [`design/rasterization-consistency.md`](design/rasterization-consistency.md).

30. **Docs/context hygiene — shrink what every agent loads** *(new 2026-06-10, surfaced
    while the per-agent context cost started "being felt in more agents")*. The docs are
    tight and high-value; the problem is *load shape*, not volume. Diagnosis + the one move
    already taken:
    - ✅ **DONE — CLAUDE.md is now a router, not a reference.** The 197-line "Game feel"
      essay (~31% of the file) was always-loaded into *every* agent every session despite
      being task-specific. Extracted to [`guides/game-feel.md`](guides/game-feel.md), left a
      ~10-line pointer. CLAUDE.md 628 → 443 lines. **Next candidate:** the live-inspection
      recipe in CLAUDE.md's "Debugging carts" section largely duplicates
      [`guides/debug-harness.md`](guides/debug-harness.md) — could become a pointer too.
    - **Add a `stereo.md`-style "STATE + next bite" header to the big design docs.**
      `instrument-engines.md` (1,455 lines) and `audio-notes.md` (1,238) force a large read
      to orient; a 20–30 line top block (current state, ✅ shipped, next concrete step) lets
      an agent orient cheaply — the pattern that let stereo.md convey the whole stereo+effects
      gate in one small read. Split past ~1,500 lines, keeping stable §-anchors (lint-docs
      already guards refs).
    - **Consolidate layered corrections.** ✅ **DONE for the effects decision (2026-06-10).**
      It was the worst offender — 4 reads for 1 truth (0015 + its appended Correction +
      `instrument-engines.md` §8.10 + its banner). Folded the wah correction into 0015's "Why"
      lead (now correct in place; Correction shrunk to a dated tail) and trimmed the §8.10
      banner's redundant preamble. **General pattern stays open** but explicitly *not a project*:
      fold corrections into the primary text + push superseded rationale to a clearly-marked
      tail, opportunistically, never as a 154-file sweep. Append-only is honest but taxes every
      future reader — fix a doc's lead when it's outright *wrong*, not just append-decorated.
    - **Keep favoring queryable state over prose** — `cart-status.js` / `lint-docs.js` /
      `lint-carts.js` let an agent *ask* "what's stale / do refs resolve" instead of reading
      everything. That's the right direction; more of it beats more prose.

31. **Engine tuning — some modeled engines play out of tune** *(new 2026-06-10, found by
    the new `tune-check.js`)*. Run `node tools/tune-check.js` for the live per-engine cents
    report (SINE is the 0¢ control); full first-run audit + the *why* in
    [`design/audio-notes.md`](design/audio-notes.md) §18. **The to-do list, worst first:**
    - ~~**`INSTR_PIPE` (flute) — the bad one.**~~ **FIXED 2026-06-11.** Was an octave low and
      progressively flat (A2 −13¢ → A5 −159¢): the bore was sized a full wavelength but the
      inverting open-end reflection resonates at SR/(2·delay), so it rang an octave down, and the
      uncompensated jet+filter loop delay added the flatness. Fix in `sound_pipe_start`: half-
      wavelength bore minus a loop-delay term **derived from the note-on jet length** (`1.69 +
      0.308·jetLen`), sized with the bowed-string fractional-read trick to kill integer
      quantization. The jet-length term is the key: the embouchure macro (morph) sets the jet, and
      a constant left morph≠0 sharp by up to a semitone — deriving it keeps the flute in tune
      across the whole embouchure range. **Now in tune within ~±3¢ from C4 up to ~E6 at typical
      embouchure** (verified at morph 0.70, the showcase recipe; robust across seeds). First
      customer: `air.c`'s Cherry flute register reopened 67–83 → 64–86.
    - **Hollow presets (recorder/breathy/pan-pipe) — FIXED through A5, 2026-06-16 (commit 97a794e).**
      The jet loop-delay `1.69+0.308·jetLen` under-compensated at long jets (morph ≲ 0.5) → flat to
      ~−56¢ by G5. Added a clamped-linear jet-delay correction past jetLen 5 (measured need
      SATURATES at ~+0.8 sample), zero at jetLen ≤ 5 so flute/piccolo are byte-identical. All 5
      presets in tune through A5; morph-0 extreme improved (A5 −84¢→−32¢). audio-notes §18 #8.
    - **Residual (minor): the morph≈0 / hollow TOP OCTAVE (above ~A5) still mode-flips** — at a
      ~20-sample bore the jet ≈ the bore, so the oscillator sits on the overblow edge and flips mode
      (the `tune-check.js` default sweep, morph 0, still flags PIPE A5 — now −32¢, was −84¢). Any
      real recipe stays ≤ A5 here. Fully closing it needs a jet-length re-voicing (jet ∝ bore).
    - ~~**`INSTR_PLUCK` / `INSTR_REED` / `INSTR_BRASS` — flatten at the top** (A5 −17 to −25¢).~~
      **FIXED 2026-06-16 (commit e458af1).** The "integer-sample delay-length quantization, fix =
      fractional read tap" diagnosis was **wrong** — the reads already interpolate. Real fix:
      REED/BRASS sized the note-on bore from a truncated integer delay (→ sharp high notes, dense
      sweep showed BRASS C#6 +64.5¢) → use the true fractional delay as init reference; plus subtract
      the bell-LP loop group delay `(1−lpCoeff)/lpCoeff` (BRASS ×0.5, REED ×1.0). PLUCK: −0.5 on the
      tap for the Karplus averaging's exact half-sample delay. All in tune now — audio-notes §18 #7.
    - **In tune, no action:** SINE/MALLET/EPIANO/PD/PIANO/GUITAR/FM, and **BOWED** (≤ +3¢ —
      whatever's off about the bowed voice, it is *not* pitch; though its default bow PRESSURE
      wants a bump — tuning-handoff.md → NEXT). **`INSTR_ORGAN`** reads an octave low but is in
      tune (+3–7¢) — that's the 16′ sub-octave drawbar, expected.

32. **Split `runtime/sound.h` per-engine to cut the parallel-agent collision surface** *(new
    2026-06-11, surfaced when a parallel commit silently clobbered a PIPE tuning fix)*. `sound.h`
    is one ~4300-line file every audio change touches, edited by several agents in one shared
    working tree — so a stale full-file edit committed over a neighbour's change silently reverts
    it (no git conflict; "different content" only). Two clobbers happened this session: a
    build-breaking half-finished refactor, and a PIPE `loopDelay` reverted to an older line (still
    compiled, just out of tune). **Cheap guards already shipped** — the `.githooks/pre-commit`
    compile-gate + the CLAUDE.md "Hot shared source files" protocol (no full-file `Write`,
    re-Read before edit, grep HEAD after commit, `tune-check.js --quiet` for engine edits).
    **The structural fix (this item):** carve each engine's `sound_<eng>_start`/`_sample` pair
    (and the FX processors) into their own `#include`d headers — `runtime/engines/pipe.h`,
    `brass.h`, …, `runtime/fx.h` — leaving only the shared `Voice` struct, the dispatch switch
    (`sound_engine_sample`), the mixer callback and the public API in `sound.h`. Then an agent
    voicing the flute edits `engines/pipe.h` only; a brass agent edits `brass.h` — engine work
    stops colliding. Notes: it's a **pure textual move, zero runtime change** (all stays
    `static`/`inline` in the one TU `studio.c` includes), so verify **byte-identical** after
    (`soundcheck` + a cart or two rendered to WAV, `wav-analyze.js` "bytes identical" + unchanged
    `tune-check.js`). The `Voice` struct stays shared (every engine's state lives in it), so adding
    a *new* engine still touches the struct + dispatch — collision surface drops a lot, not to
    zero. The refactor is itself one massive `sound.h` commit, so it only lands cleanly in a
    **quiet window** (freeze other audio agents, split, verify, unfreeze) — else it becomes the
    very clobber it prevents. High-effort; only worth it if you keep running several audio agents
    at once. Until then the cheap guards hold the line.

33. **Instrument bank + orchestra tuner** *(new 2026-06-11, **PARKED behind a prerequisite** —
    full spec in [`instrument-bank-plan.md`](instrument-bank-plan.md))*. Two complementary things
    on one source of truth: a machine-readable **registry** of the named dependable voices
    (engine + macros + register + tuning verdict) that carts `#include` instead of copy-pasting
    floats out of [`guides/instrument-recipes.md`](guides/instrument-recipes.md), and an
    **orchestra-tuner cart** — the audible/visual *audition* counterpart to the headless
    `tune-check.js` gate (plays a voice against a sine reference so you *hear* the beating, shows
    the baked cents needle so you *see* the verdict). Architecture: `tools/presets.json` →
    `gen-presets.js → runtime/presets.h` (mirrors `gen-tcc-symbols.js`). **Groundwork done:** a
    doc↔cart reconciliation audit found **zero float drift** (the doc is a clean seed) and pinned
    the vanilla anchor per family; PIPE is the only tuning hazard (only `pipe/flute` m0.70 safe).
    **Why parked:** the per-voice control surface beyond `h/t/m` isn't unified — `eng_tune` is
    EXPERIMENTAL ("not a final API"), VOICE uses bespoke `voice_nasal`/`voice_consonant`. **Land a
    clean 4th-axis/aux-param API first**, else the preset schema can't capture bowed-pizz /
    guitar-fundamental / voice-nasal. Locked decisions, schema, and build order live in the plan
    doc. *(The audit's "radio voices missing from the recipe docs" subtask was a false positive —
    resolved on inspection: they're all present and value-accurate in `radio-voices.md` +
    `instrument-presets.md`, where radio voices belong; the audit checked `instrument-recipes.md`.)*

34. **`line()` draws axis-aligned lines as unreliable `GL_LINES`** *(new 2026-06-11, found via the
    `raycaster` cart)*. `line()` in `runtime/studio.c` is a bare Raylib `DrawLine` → GPU `GL_LINES`.
    For perfectly **vertical** (and horizontal) lines at integer coords, the GL diamond-exit rule
    darkens/drops individual columns — a raycaster (one vertical line per screen column) shows it as
    regular dark vertical bars. GPU/driver-dependent, so it can surface without any code change.
    **Worked around per-cart** in `raycaster.c` (vertical wall slices now use `rectfill(x, y0, 1, h, …)`
    instead of `line`), but the **root-cause engine fix is deferred**: special-case axis-aligned lines
    inside `line()` to draw as a filled rect (`DrawRectangle`) — fixes every cart drawing vlines/hlines,
    but touches the hot shared `studio.c` so it needs a compile-gate + a look at any cart relying on
    the current 1px GL behavior. Deferred deliberately (cart fix unblocked the regression).
35. **CPU-shader demo polish** *(new 2026-06-12, after the shadelab → caustics → raymarch trilogy
    + `shadermath.h` shipped)*. Two parked ideas, both fully spec'd in
    [`design/cpu-shaders.md`](design/cpu-shaders.md): **#3 make the per-pixel cost visible** — a
    live evals/ms/FPS HUD line on the shader carts so dropping `ps` 3→1 viscerally shows why GPUs
    exist (cheap; profiler data already exists); **#5 a true offscreen buffer** for proper
    multipass/ping-pong (blur, bloom, clean previous-state sampling) — lower priority and an
    *engine* change (a `RenderTexture2D` carts can draw into + sample), since the feedback shader
    already fakes ~80% of the intuition on the live canvas.

36. **✓ SHIPPED 2026-06-15 — modrack MACRO now exposes all 14 modeled engines** *(Option B taken:
    `SOUND_INSTR_SLOTS` 32→48; the 8 new engines MEMBRANE/REED/PIPE/VOICE/GUITAR/PIANO/BOWED/BRASS
    got dedicated slots 32–39, `eng` knob 0..13. Bandito reworked to MEMBRANE bongos; new Chamber
    preset (BOWED). Commits `5db2327` engine, `de5c36f` cart.)* Original investigation below.
    *(2026-06-15, investigation + decision)*. The engine ships **14 modeled instruments all on the same
    Mutable-style harmonics/timbre/morph 3-macro interface** (`INSTR_PLUCK/MALLET/FM/ORGAN/EPIANO/PD`
    — the 6 modrack's MACRO `eng` knob already offers — plus **8 not reachable from modrack**:
    `MEMBRANE` (tabla/conga/**bongo**/djembe), `REED` (clarinet/sax), `PIPE` (flute), `VOICE`
    (formant sing/speak), `GUITAR` (string+body), `PIANO` (stiff-string grand/harpsichord),
    `BOWED` (violin/cello), `BRASS` (trumpet→tuba)). They'd drop straight into MACRO's existing
    knobs + CV inlets (same 3 macros, all CV-modulatable). Bonus: `MEMBRANE` would give the
    **Bandito** preset *real* bongos instead of the MALLET stand-in.

    **The blocker is `SOUND_INSTR_SLOTS = 32` (slots 0–31 all used).** modrack gives each MACRO
    engine a dedicated slot, and there are no free slots. The allocation:
    - **0–4** — the engine's raw built-in waves (reserved).
    - **5–22 (18 slots) — the VOICE module's `wav` knob.** 9 waveforms (`saw/sqr/tri/sin/noi` +
      4 drawn user tables `org/vox/bel/fld`, baked via `wave_set()`) × **2 envelope banks**:
      5–13 = normal envelope (own decay), 14–22 = *flat* envelope (full sustain). VOICE plays the
      flat bank when its `a`/amp jack is patched, so an external ENV is a true VCA instead of
      fighting the slot's baked decay (`slot = (amp_cv?14:5) + wav`). The biggest tenant.
    - **23–25** — MACRO engines PLUCK/MALLET/FM.
    - **26–28** — DRUM kick/snare/hat.
    - **29–31** — MACRO engines ORGAN/EPIANO/PD.

    **DECISION (2026-06-15): wait until `sound.h` is clean, then do Option B (bump the slot count).**
    Option B is the cleaner fix and its only real cost is trivial — it was the hot-file timing, not
    memory, that made us hesitate. Park the whole expansion until `sound.h` is quiet (and ideally
    until the per-engine split #32 lands, so we bump the count once, in the right place).

    Two paths to the other 8 engines:
    - **A — reconfigure-on-change (cart-only):** make the six MACRO slots a *pool*; re-init a MACRO's
      slot to the chosen engine when its `eng` knob changes (set-and-hold — a rare deliberate action).
      All 14 selectable, ≤6 sounding at once across the patch (generous). Needs a per-engine config
      table (engine + sustain/release), pool assignment by MACRO-module order, `FMT_ENGINE` grown to
      14 labels, `eng` range 0..13. Fully unblocked *now* — the fallback if we want it before B.
    - **B — bump `SOUND_INSTR_SLOTS`** (e.g. 32→44) so each engine gets a permanent slot (all 14
      simultaneous). **The chosen path, deferred until `sound.h` is clean.** The `.bss` cost is
      negligible: `instr_bank[SOUND_INSTR_SLOTS]` is the *only* slot-sized array and `sizeof(Instrument)
      ≈ 200 bytes` (a flat ~50-field struct, no buffers), so 32→44 adds **~2.4 KB** (32 slots = 6.4 KB
      total) — and `.bss` is **0 download** (zero-filled at launch; wasm: a hair more initial memory).
      No per-cart buffers, no extra voice state. The genuine blockers are timing, not size: it's a
      `sound.h` edit (hot file — clobber risk while another agent's in there) and it collides with the
      per-engine `sound.h` split (#32). Both dissolve once `sound.h` is quiet.

    Engine macro reference (per-engine meaning of each macro) lives in the `INSTR_*` comments in
    [`runtime/studio.h`](../runtime/studio.h) and [`design/instrument-engines.md`](design/instrument-engines.md).

37. **✓ SHIPPED 2026-06-15 — polyphony `SOUND_VOICES` 16 → 32** *(+ `SOUND_HANDLE_BITS` 4→5;
    commit `5db2327`, batched with #36's slot bump. Verified soundcheck + tripwire + tune-check.)*
    Original note below. *(2026-06-15)*.
    16 voices starves on rich patches: the long-ringing modal/Karplus engines (PLUCK/MALLET/PIANO/
    GUITAR/MEMBRANE) hold voices through their release, so chords + fast passages + sustained tails
    overrun the pool. Precedent: the 8→16 flip (audio-notes §15).
    - **Forces a coupled edit:** `SOUND_HANDLE_BITS` 4 → 5. The note-handle's voice-index field is 4
      bits (holds 0–15); 32 needs 5 (0–31), and a `_Static_assert` refuses to compile until it's
      bumped. Mechanically safe — handles are opaque ints to carts, and the encoding just splits at a
      different bit (gen still fits the int after the shift). Grep first for any hardcoded `& 15` /
      `>> 4` handle math that doesn't go through `SOUND_HANDLE_MASK`/`_BITS`.
    - **RAM cost ≈ +150 KB `.bss`** — far bigger than the slot bump because `sizeof(Voice) ≈ 9–10 KB`,
      dominated by **two 1024-float Karplus delay lines** (`ks_buf` + `pn_ks2`, 4 KB each, present in
      every voice regardless of engine). 16→32 doubles the ~150 KB pool. Still **0 download**
      (zero-filled at launch; wasm: ~3 extra 64 KB memory pages).
    - **The real cost is CPU, not RAM:** every *active* voice is processed per-sample, so 32 doubles
      worst-case audio-thread load — and the hungry engines are exactly the ones you'd max out. Idle
      voices cost nothing (pay-for-use), so it raises the ceiling rather than the floor; watch the
      audio budget on wasm / weak hardware. 24 is a CPU-cautious middle ground, but **32 is clean**
      (fits the 5-bit handle field exactly).
    - Same `sound.h` timing as #36 — land both `#define` bumps (+ #32's split if it's ready) in one
      compile-gated + tripwire'd + tune-checked change.

38. **Boutique-effects leftovers (low priority)** *(2026-06-15)* — the boutique-pedal arc is essentially
    done: **shipped** grains-pitch, the modulation kit (`mod_randwalk`/`mod_sh`/`mod_optical`), Univibe,
    dropout, the `fx_order` 16→32 packing widen, Shallow Water, amp-noise, the noise gate, and the trophy
    **Shimmer** (octave-up bus pitch-shifter) — see [`design/boutique-pedals-roadmap.md`](design/boutique-pedals-roadmap.md)
    + `audio-notes §17` #19–#27. What's left is small/optional:
    - ✅ **MOOD varispeed — DONE 2026-06-15.** `varispeed(speed)` (navkit `half_speed` port): tape
      playback-speed dive/bend of the whole mix, slewed (tape inertia), exact octave at 0.5. Master-stage,
      byte-identical at 1.0. Showcase `varispeed`. **The original boutique-pedal lists are now done.**
      (`tape_stop`/`rewind` triggered-gesture cousins remain unported; the swept varispeed covers the dive.)
    - ✅ **Shimmer in the pedalboard — DONE 2026-06-15.** Added as a SHIMMER macro pedal (kind -3) driving
      master `shimmer()` — an output-stage ambience (like a reverb at the end of the rig; chain position
      cosmetic). Cart-side only (no engine); default board byte-identical. A *reorderable* per-bus
      `FX_SHIMMER` insert is still possible later but wasn't needed for "hear it in the rig."
    - **Engineering nits** (may never matter — measure first): `fast_tanh` Padé approximation for the
      soft-clip *only if* it shows up hot in the profiler; an **AC128 transfer-curve LUT** for a *named*
      vintage-germanium fuzz voicing beyond `DRIVE_ASYM`; exposing the Shimmer pitch-shifter as a
      standalone bus effect. Full detail: the roadmap's "Side quests" + "Follow-ups" sections.
    - **King Tubby multi-head dub delay** (`dub_loop.h` port) — dub *character* is already cart-side
      (`echo`+`tape`, the `dub` cart); a faithful port adds the **multi-head** taps + integrated
      degradation, which needs `sound.h` (echo is single-tap). Sized in
      [`navkit-porting-handoff.md`](guides/navkit-porting-handoff.md) → queue; gate on 0015 first.

39. **Unify LFO shape (it's a patchwork)** — ✓ **SHIPPED 2026-06-15.** One `LFO_SHAPE_*` enum
    (SINE/SQUARE/TRI/SAW/RAMP/OPTICAL/SH/RANDOM) now drives every LFO site through a single
    `lfo_wave`/`lfo_eval` dispatcher: voice LFOs (new `lfo_shape(slot,which,shape)` +
    `note_lfo_shape(handle,…)` setters — non-breaking, the 72 `instrument_lfo` carts unchanged),
    `tremolo`/`autopan` (now take any shape; `TREM_*` are aliases), and `fx_lfo` (gained a `shape`
    arg). Stateful shapes (S&H/random) embed a deterministically-seeded `ModState` per LFO instance
    (`--det` byte-reproducible). SINE stays byte-identical (dispatcher returns the same `sinf`); the
    voice path is skipped entirely at `depth==0`. **Showcase: `lfoshapes`** (8 shapes, live, on pitch
    or cutoff). **Forward-compat left in place:** promoting `shape` into the `instrument_lfo` signature
    later is purely additive (storage/dispatcher/request already shape-aware — see the code comment on
    `instrument_lfo`). **Adopted by existing carts (2026-06-15):** `sh101` now drives its square + S&H LFO
    waveforms through the engine (sample-accurate, replacing a frame-rate software LFO; only NOISE stays
    software — no engine white-noise shape); `22-filter` gained an `S` shape selector across all four
    filters; and ten game/ambient beds whose cutoff LFO was a mechanical sine now use `LFO_SHAPE_RANDOM`
    for organic drift (`hotline`, `neonrain`, `masseffect`, `podracer`, `dune`, `dwarffort`,
    `dungeonkeeper`, `turfwar`, `wildfire`, `zoo` — wind/fire/cavern/engine-and-animal roars).
    Round two (2026-06-15): `pedalboard`'s TREMOLO + AUTOPAN pedals expose all 8 shapes on their WAV
    knob; `modrack`'s `MOD_LFO` gained a `shp` knob (sin/sqr/tri/saw/rmp/opt — S&H stays the `MOD_SH`
    module). To make "reuse the dispatcher" literal, a public **`lfo_value(shape, phase)`** now exposes
    the engine's stateless shape math so carts compute shaped CV / draw waveforms without re-rolling it
    (modrack's `MOD_LFO` uses it; the hand-rolled mirrors in `lfoshapes`/`22-filter` are candidates to DRY).
    Original context (the three disconnected places this unified) below for the record:
    - the main LFO system (`instrument_lfo`/`note_lfo`, driving `LFO_PITCH`/`CUTOFF`/macro dests) is
      **sine-only** — no shape param (`sinf(lfo_phase·2π)` in `sound.h`); the one place shape would be
      most expressive has none.
    - `tremolo`/`autopan` have an **ad-hoc** `shape` arg (`TREM_SINE`/`SQUARE`/`TRI`) — 3 shapes, only
      those two effects, behind a tremolo-named enum.
    - the modulation kit (#item B) already has **more** shapes as internal helpers — `mod_optical`
      (asymmetric bulb ramp), `mod_randwalk` (filtered random), `mod_sh` (sample-&-hold) — baked into
      specific effects (univibe = optical), not selectable anywhere.
    - **`fx_lfo` (ADR 0018) shipped 2026-06-15 — also sine-only** (no `shape` arg in the signature), so
      it's now the prime *consumer* waiting on this: a unified shape vocabulary should thread through it too.
    **The fix:** a unified `LFO_SHAPE_*` enum (SINE/TRI/SQUARE/SAW/RAMP/S&H/RANDOM/OPTICAL) accepted by
    `instrument_lfo`/`note_lfo`/`fx_lfo`, folding the `TREM_*` shapes into one vocabulary. **Most of the
    DSP already exists** (`mod_sh`/`mod_randwalk`/`mod_optical`; tri/square/saw are trivial) — it's a small
    `lfo_shape(phase, shape)` dispatcher + threading a `shape` arg through the LFO generator. Square-on-
    cutoff = stepped filter; S&H-on-pitch = random-step arp; etc. Touches `sound.h`; natural sibling to the
    shipped `fx_mod`/`fx_lfo` work in [0018](decisions/0018-effects-keep-params-but-become-modulatable.md).

39b. **`fx_mod` deferred targets — reverb/delay sends + wah** *(2026-06-15)* — `fx_mod`/`fx_lfo` shipped
    with 7 targets ([0018](decisions/0018-effects-keep-params-but-become-modulatable.md) "Shipped"). The
    ADR sketch also listed `FXMOD_REVERB_SEND`/`_DELAY_SEND`/`_WAH`, **dropped from v1 because each needs a
    NEW param before it can be a target**, not just an enum value: reverb/echo sends are per-*voice*
    (`v->rvb`/`v->eko`), so there's no bus-level return-gain to ride — add one first; wah is envelope/LFO-
    driven with no manual position — add a manual-sweep mode first. The `FXMOD_*` enum leaves room to append
    them (7+, no renumber). Small, additive `sound.h` work; do it when a cart actually wants these swept.

40. **Spatial audio v3 — acoustic zones** *(2026-06-15)* — v1 (per-voice) + v2 (emitter buses) SHIPPED;
    the remaining layer is environment: **inside/outside reverb zones, occlusion (a wall between you and
    the source → muffled, low-passed), and material absorption (carpet vs tile)**. The encouraging part:
    the DSP already exists — occlusion = a low-pass driven by "how blocked" (`FILTER_LOW`/`note_cutoff`),
    zones = reverb size/send by which zone the listener is in, materials = an EQ/decay tweak. So it's
    mostly **cart-side zone logic feeding existing knobs**, with maybe a thin convenience helper — *more
    game logic than engine DSP*. Full spec + the v1/v2 build record: [`design/spatial.md`](design/spatial.md)
    → "v3 — acoustic zones". Free-field (direct path only) until built.
    - **PROBE SHIPPED 2026-06-15 — `acoustics` cart** (`"probe"` kind): a top-down walker over
      tile/carpet/wood/grass rooms + a wall/doorway, occluding two emitters via a listener→source
      raycast. Confirmed the mechanic is cart-side over existing knobs (`note_cutoff` for the muffle
      is great — Hz + slewed). **Two real engine asks dropped out:** (a) **`note_gain(handle, float)`** —
      `note_vol`'s 0..7 is too coarse for smooth occlusion attenuation (audible level steps); (b) **a
      rideable / crossfadable zone reverb** — set-and-hold means you can't slew `reverb()` per frame, so
      crossing rooms jumps abruptly. Build plan: a cart-land **`acoustics.h`** helper (raycast occlusion
      + zone/material tables → the knobs) + those two engine conveniences. A tilemap **line-of-sight**
      primitive is an optional bonus (generalizes to vision/stealth). Findings in the cart header.
    - **Engine ask (a) SHIPPED 2026-06-15** (commit `5184a88`): **float `note_vol` + `note_res`** — both
      now `float`, ranges kept (0..7 / 0..15), input quantization dropped → byte-identical for every existing
      caller (int literals promote to the same float). Superseded the earlier separate-`note_gain` sketch.
      **Engine ask (b) still DESIGNED** (ready to build when `sound.h` is free) in
      [`design/spatial.md`](design/spatial.md) → "Designed solutions": rideable reverb = slew the tank's `fb`/`damp` toward targets, gated
      on a new `reverb_glide(ms)` (snap by default → byte-identical; `reverb()` re-call is already cheap —
      just no slew). Two-tank crossfade noted as the v3.1 higher-fidelity fallback.
    - **Floating `note_vol` cleared the second-customer bar independent of v3 — SHIPPED 2026-06-15**
      (commit `5184a88`; was overdue, not speculative — cart survey 2026-06-15). The live per-note surface is float *everywhere* (`note_cutoff` Hz,
      `note_pitch`/`duty`/`pan`/`drive`/sends, the macros) **except two integers: `note_vol` (0..7) and
      `note_res` (0..15)** — the stragglers from before the float-everything convention. **8 carts already
      compute a continuous level then crush it through `note_vol`** via `(int)(level*7+0.5f)` in a per-frame
      loop. Tier 1 (the level *is* the gesture): **`martenot`** (the *touche d'intensité* souffle, quantized
      to ~6!), **`glassharmonica`** (a lerp'd swell the code calls "the glass-harmonica swell"), **`bowed`**
      (bow energy), **`musicalsaw`** (bow speed), **`mouthharp`** (breath). Tier 2: **`modrack`** AMP CV (a
      modular VCA on a smooth CV → 8 steps). Tier 3 (modest): `trafficjam`/`trackgen` engine-by-speed.
      Retrofit = *opportunistically* drop the `(int)(…*7)` for `note_vol(h, level*7)` (no forced migration —
      old call sites stay valid). **Sibling: `note_res` float** (0..15 → continuous) — smaller (16 steps,
      subtler), 2 customers (**`modrack`** RES CV + **`brass`** mute sweep). One-shot velocity
      (`note`/`hit`/`tone` vol 0..7) stays int — transients don't perceptibly step. **Worth its own STATUS
      item** given it's justified beyond spatial — the cleanest, lowest-risk audio change on the board.

41. **Waveguide engines can now bend pitch DOWN** *(2026-06-16)* — full orientation +
    resume steps + measurement workflow: [`design/waveguide-bend-handoff.md`](design/waveguide-bend-handoff.md).
    ✓ **SHIPPED** for **BRASS / REED /
    PIPE / BOWED** (commits `8dfd12a` brass, `d7e6957` reed/pipe/bowed). The bug: each engine sized its
    delay line exactly at the note-on pitch and clamped the read length to it, so a held note could only
    bend UP — downward `note_glide`/`note_pitch`/pitch-env and the lower half of vibrato all clamped at
    the note-on pitch (the trombone SLIDE only slid up; bass lines had to re-trigger lower). Fix: size the
    bore/string ×2.5 (~16 semitones of down-room, capped by `ks_buf`) and pick the init-freq reference so
    the note-on read length is unchanged → tuning byte-identical (reed/brass) or within ~1–3¢ (pipe at
    usable embouchure / bowed's chaotic stick-slip); only the clamp ceiling rises. Verified against the
    pristine baseline; pizzicato still plucks; dc-check + tune-check clean. **PLUCK/GUITAR already had
    ~1 octave of down-room (2× headroom); the simple oscillators (SINE/TRI/SAW/SQUARE/FM/PD) bend freely.**
    - **Caveat — BOWED low register is buffer-capped.** Full wavelength is 2× a half-wave engine, so at
      the bottom of the range the buffer is already at `SOUND_KS_MAX` (1024) and there's no room to
      lengthen. Measured on the bass: down-bend works from ~**E2 up**, but **E1 (the open low-E, ~41 Hz)
      can't bend down at all**. Raising the cap (a bigger `ks_buf`, more RAM/voice) or a coarser low-note
      bore would extend it — only worth it if a cart needs sub-E2 portamento.
    - **TODO — revisit the carts built AROUND the old limit:** `upright.c` (the upright bass, `INSTR_BOWED`)
      hard-codes an **up-only** pull-bend (`fabsf(dpx)` → always `+vbend`) and uses fret-walk
      re-articulation for downward motion *because the engine couldn't bend down* (its description even
      says "the waveguide string bends UP cleanly but can't be bent below its pitch (verified)"). Now it
      can: make the pull-bend **signed** (pull down → smooth flatten) above ~E2, keep the fret-walk as the
      fallback at the very bottom + as the deliberate walking-bass articulation. `pdbass.c` was spun off
      *only* to get a two-way slide (`INSTR_PD` oscillator) — still valid as the "buzzy CZ" variant, but
      the upright itself no longer needs the workaround. Update both carts' descriptions when revisited.

42. **Audio TEST-COVERAGE blind spots** *(2026-06-16, found in a "what isn't tested?" audit)* — the
    engines are well-covered (`tune-check.js` = pitch, `dc-check.js` = DC, the soundcheck tripwire =
    dropped requests, `build-all.js` = cart-vs-API rot) but whole *categories* have no automated gate.
    Ranked by leverage; full reasoning in [`design/audio-notes.md`](design/audio-notes.md) §20.
    - ✅ **Loudness regression gate — SHIPPED `tools/level-check.js`.** The twin of tune-check for
      *level*: renders the same `tunecheck` sweep (`--det`, deterministic) and measures each note's
      peak/RMS/crest in dBFS against a committed golden baseline (`tools/level-baseline.json`,
      re-bless with `--save`). `--quiet` = CI gate (exit 1 on > **4 dB** drift, or new silence/clip).
      Catches the silent regression a compile + tune-check + dc-check all miss — an engine that got
      louder/quieter, or one now slamming the master limiter (crest collapse, dynamics squashed).
      Also flags absolutes with no baseline (SILENT / HOT-on-its-own / loudness-outlier-vs-library).
      **First-run finding (now FIXED):** the library was uneven — BOWED peaked −1.2 dBFS (+12.8 dB over
      the median; two sustained bowed notes clipped the limiter) and BRASS A2 +9 dB, while most engines
      sit −14 to −18. **Trimmed the per-engine output makeup** (`sound.h`: BOWED `dc*3.0→0.7`, −12.6 dB;
      BRASS `(2.7+dark*0.7)→(0.93+dark*0.24)`, −9.3 dB). Now BOWED −13.8…−20, BRASS −14…−15.7, both at
      the ~−15.5 median; no HOT/outlier flags. Pitch unchanged (gain is amplitude-only — dc/tune clean),
      trumpet↔tuba ratio preserved. Baseline re-blessed (`level-baseline.json`); `--quiet` green.
    - ✅ **Effect STABILITY gate — SHIPPED `tools/fx-check.js`** (+ harness cart `fxcheck.c`,
      baseline `tools/fx-baseline.json`). Renders a loud sustained chord into the master bus and, one
      effect at a time, sets THAT effect at its documented EXTREME (echo fb `1.1`, flanger/phaser
      `±0.95`, filter res `0.99`, etc.), then asserts the output stays finite/bounded: no
      collapse-to-silence (a NaN through a feedback loop reads as silence in 16-bit), no DC runaway
      (mean over the full window, *and* both halves must agree in sign — separates true DC from a
      sub-sonic resonant wobble), no permanent limiter-pinning, and that it moves the signal off the
      DRY reference (a dead/unwired effect). Baseline records the intrinsic per-effect state so
      `--quiet` flags only *regressions* (got worse / drifted >4 dB) — known extremes stay green.
      Stability gate, not a character gate (whether it SOUNDS good is still by ear). **Findings —
      two real latent bugs at the extremes, both now FIXED:** the **phaser** (fb 0.95, 8 stages)
      accumulated **−0.13 persistent DC** and the **echo** (fb 1.1 runaway) **−0.04** — both far past
      `dc-check`'s ~1e-4 clean tolerance. Cause was exactly the failure mode `dc-check.js`'s header
      warns about (no master DC blocker → every asymmetric/feedback stage must block its own): the
      phaser allpass cascade and echo `tanh` loop pass/inject DC that high feedback accumulates
      ~1/(1-fb)×. **Fixed** (a one-pole DC blocker, R=0.999 / ~7 Hz, on each feedback tap — the idiom
      the `drive` effect already uses, `drv_dc_*`): phaser −0.13→−0.007, echo −0.04→+0.002, audio
      untouched (corner is sub-sonic). Verified compile-gate + tripwire + dc/tune/level/fx all green,
      build-all 390/390.
    - ✅ **Effect STACKING — now covered** (fxcheck.c tests 13–18). Six master-bus chains via
      `fx_order()`: a lo-fi mastering chain (drive→eq→crush→tape), two resonant combs in series
      (flanger→phaser), two feedback delays (echo+reverb), an A/B order swap (drive→reverb vs
      reverb→drive — proves ordering is audible *and* both stay bounded), and an 8-deep kitchen sink.
      All stay bounded (the worst is "limiter pinned" at the deliberate extreme — expected, baselined).
      **Incidental find + fix:** the two-combs stack exposed that the **flanger** also accumulated DC
      at high feedback (−0.03) — the single-effect test had masked it (at fb 0.95 its DC *oscillated* →
      classed as wobble; in series it settled). Same missing-DC-blocker bug as phaser/echo; fixed with
      the same one-pole idiom (flanger −0.046→−0.002). So **all three feedback combs (phaser/echo/
      flanger) are now DC-blocked.** Verified: compile-gate + tripwire + dc/level/fx all green,
      build-all 390/390.
    - **The web/wasm audio path is verified only by ear — SCOPED + a CONFIRMED-on-paper pitch bug.**
      Every gate above runs the NATIVE build; nothing checks the wasm build emits the *same samples*.
      Scoping + phasing + the source dig: [`design/web-audio-parity.md`](design/web-audio-parity.md).
      **CONFIRMED from source (2026-06-17): the WORKLET backend (desktop default) plays ~+147¢ sharp on
      any non-44.1k device.** `sound_worklet_init()` calls `emscripten_create_audio_context(0)` (NULL
      opts → `new AudioContext()` → device default SR, usually 48000) and `sound_aw_process` fills the
      128-sample output with NO resampler from 44100-synthesized audio → 48000/44100 = +146.7¢ + 8.8%
      fast. The **plain** backend resamples (`LoadAudioStream(44100)` via miniaudio) and is correct.
      Device-dependent, so it ships unnoticed: macOS built-in output is often 44.1k (sounds fine on the
      owner's Mac), most 48k devices (Windows/Linux/DACs/Bluetooth) are sharp. **✅ FIX APPLIED** (`sound.h`
      `sound_worklet_init`): the worklet context is now forced to `SOUND_SAMPLE_RATE` via
      `EmscriptenWebAudioCreateAttributes` (browser resamples to hardware → matches native + the plain
      backend). Compiles clean native + emcc-worklet. **Two follow-ups:** (1) on-device confirm (verified by
      source reading, not yet a browser) + a listen to the resample quality; (2) **republish** — shipped
      `site/` carts keep the old `(0)` worklet until a web rebuild.
      **✅ Phase 1 codegen-parity gate SHIPPED — `tools/web-audio-check.js`** (+ `web-audio-host.c` + a
      raylib shim): compiles the engine clang-native vs emcc-wasm, renders each engine solo, compares. **The
      math is faithful: 15/16 engines sample-identical (native↔wasm diff 75–120 dB below signal; TRI
      byte-exact; the rest libm/FMA ULP noise — inaudible).** Lone exception **BOWED** — chaotic stick-slip
      friction, so a 1-ULP diff diverges to a different micro-waveform, but the two renders' RMS **levels match
      to 0.06 dB** (same note, just micro-phase). Two-tier gate (sample parity, perceptual-level fallback for
      chaotic engines); `--quiet` CI. Net: the only real web bug was the SR (fixed); the codegen is clean.
    - ✅ **Set-and-hold footgun — now lintable: SHIPPED `tools/lint-fx-frame.js`.** Static check (no
      render) that flags an UNCONDITIONAL per-frame call to a buffer-rebuilding effect
      (`crush`/`tape`/`eq`/`chorus`/`reverb`/`flanger`/`phaser`/…) in `update()`/`draw()` — the silent-
      stutter footgun. Calls inside an `if`/`?:` guard pass; `filter()`/`varispeed()`/`note_*` are
      excluded (built to ride live); waive with `// fx-lint-ignore`; `--quiet` = CI gate. **Audit came
      back CLEAN across 390 carts** — the codebase is disciplined here (the lesson stuck), so this is a
      forward regression guard, not a cleanup. Limitation: only inspects `update()`/`draw()` directly,
      not helper-routed calls (the groovebox `apply_fx()` pattern — which is the correct structure
      anyway).
    - ✅ **Soak gate + denormal guard — SHIPPED.** `tools/soak-check.js` (+ harness `soak.c`) renders
      ~64s of stress/idle cycles (dense notes through a big reverb+echo tail, then silence) and asserts
      the long-run failures a short test can't see: stress level STABLE across all 24 cycles (no slow
      drift, no progressive voice-pool starvation from a leak), idle tails DECAY ≥12 dB below stress (no
      stuck/leaked voice ringing), the idle floor doesn't CLIMB run-long (no energy/DC accumulation), and
      nothing blows up. Decay-relative thresholds (not an absolute silence floor), `--quiet` CI gate.
      First run clean (24 cycles within ~1.5 dB). **Denormal flush-to-zero** added to `sound.h`
      (`sound_set_denormal_ftz()` at the top of `sound_callback`): a long reverb/echo feedback tail
      decays into the denormal range where FP ops run 10–100× slower → audio-thread CPU spikes (stutter)
      on some CPUs, invisible in the output. FTZ+DAZ on x86, FPCR FZ on arm64, no-op on wasm. Output is
      byte-unchanged (denormals are far below 16-bit), so the level/fx/dc baselines are untouched. The
      soak proves the tails decay (the audible side); FTZ covers the CPU side (quantifying *that* needs
      audio-thread timing — a follow-up, the profiler only sees the main thread).
    - **Micro-bug spotted in the same pass:** `amp_noise_process` + `varispeed_process` run *after* the
      master soft-clip (`sound.h:5509–5510`), and the device output has no final clamp (only the WAV
      writer does, `sound.h:372`) — so varispeed's interpolation can push the device signal slightly
      past ±1.0 → a hard driver clip. Tiny, but a real seam.

43. **VISUAL test-coverage blind spot — a golden-pixel-diff harness** *(2026-06-23, from the streetlab corner work)*.
    The `spec()` harness (`tools/spec.js`) tests **pure functions and state** — it pinned every geometric *quantity*
    in streetlab (`curb_return` tangency, `kerb_start`, `round_flare`, lane offsets; 70 assertions). But it can't
    test the **rendered pixels**: those are the output of `polyfill`/`line` scan-converting onto the framebuffer —
    engine-level rasterisation, not a pure function of the cart's inputs. The concrete case that exposed this: on a
    symmetric 4-way the four corner kerbs can land ≤1px apart in *staircase arrangement* (arc rasterisation on an
    even-width grid). Geometry is symmetric and spec'd; the difference is pure quantisation. To actually catch/fix
    that at the pixel level you'd need a **golden-image / pixel-diff** test — render a cart headless (the `--dump`
    path already exists, used for clips), compare regions (e.g. the four corner quadrants) pixel-for-pixel against a
    committed golden, or assert a symmetry invariant (corner A == rot180(corner B)). Visual-regression style, the
    twin of the audio `level-check`/`fx-check` baselines but for the framebuffer. We have `ui-audit.js` (draw-box
    *bounds* analysis from the DE_TRACE box log) but **no pixel-level diff**. Note we already keep some pixel-exact
    test/probe carts + deterministic `--dump`, so the rendering side is the missing piece, not the capture.
    **RESOLVED (2026-06-23) — harness built + the floor fixed.** The harness shipped as
    `tools/mirror-diff.js` (renders headless, reflects the framebuffer about cx/cy, counts mismatched
    mirror-pairs — the symmetry-invariant flavour; a committed-golden flavour is still future, see
    [`design/software-canvas.md`](design/software-canvas.md)'s determinism note). Using it, the streetlab
    floor was diagnosed and FIXED: the corner **fill** (`polyfill`) was *already* mirror-symmetric (it's
    CPU-decided); the whole floor was the kerb **stroke** (`line()` → GPU `DrawLine`, direction-dependent).
    A symmetric software line (`sline`) does NOT fix it — it *regresses* the kerb 7→27 because fills are
    point-mirrored (about cx=160) and 1px strokes are cell-mirrored (sum 319 vs 320) — an even-grid snap
    conflict. The fix that shipped is **`mirror_blit`** in `streetlab.c` (reflect the rendered junction-core
    pixels; guarded to the symmetric 4-way): kerb **7→0**. Full writeup +`sline` recipe:
    [`design/streetlab-corner-symmetry-plan.md`](design/streetlab-corner-symmetry-plan.md). Probes: `arcsym`
    (blit principle), `linesym`/`axissym` (primitive symmetry, any axis). **API candidates (not yet
    promoted):** `sline` is the reflection-symmetric CPU line the software canvas needs (the only
    *axis-aligned* primitive still letting GL pick pixels — the rest of that surface is the rotation/
    texture family: `rectfill_rot`/`spr_rot`/`sspr_ex`/`tritex`, see `design/software-canvas.md`);
    `mirror_blit` could become a reusable engine helper. Rule of
    thumb still holds for the un-fixed cases (skew/T/free-right): **spec the geometry, eyeball the pixels.**

44. **Tool-output hygiene — one predictable home for tool artifacts** *(2026-06-25, surfaced from the
    [cart-os](design/cart-os.md) discussion but independent of it)*. Tool outputs are scattered across
    ad-hoc locations with no naming convention: `build/.bake/<name>/screenshot.png`, `build/screenshot.png`,
    `--dump <dir>`, `--trace <f>`, `build/.bake/*_request` (the live-inspection mailbox). The concrete
    papercut: `build/.bake/<name>/screenshot.png` is the fresh baked frame, while **`build/screenshot.png`
    drifts** (it belongs to the running editor) — enough of a trap that CLAUDE.md carries it as an explicit
    "NEVER read this one" gotcha. That's a recurring "which file is the fresh one" mistake, and it's pure
    convention, not a missing capability. **Lever is naming/layout discipline, not a new system** — a single
    documented namespace (e.g. everything tool-emitted under `build/out/<tool>/…`, the editor's own scratch
    kept clearly separate) would retire the gotcha class. Explicitly **NOT** the cart-os shared-FS idea: the
    dev tools already compose fine through files; this is just tidying where they land. Low leverage, no rush.

45. **Profile the *running* cart at its current state (attach, don't cold-respawn)** *(2026-06-25)*.
    Today the editor's Profile button (`studio:profile`, `editor/electron/main.cjs`) compiles a fresh
    `-DDE_PROFILE` build, spawns a **brand-new** instance, waits 1s, runs `/usr/bin/sample <pid> <seconds>`
    (default 4), then kills it — so it always profiles a **cold** cart from startup. If you've played the cart
    into some state (minutes in), that state never exists in the thing being measured. The ask: play into a
    state, *then* click profile and capture **that moment**. Two routes, one nearly free:
    - **Cheap (mechanism already exists).** A running cart already watches `.bake/profiler_request`
      (`runtime/studio.c:1288`) and on demand dumps `{frames,workMsAvg,workMsMax,frameMsAvg,calls[]}` for its
      *recent* frames — live, no respawn, any native build (the live-inspection mailbox, see
      [debug-harness.md](guides/debug-harness.md) → "Live inspection"). Wire the Profile button to drop that
      file into the *already-running* cart (from a normal Run) and read it back. Caveat: this is the runtime's
      self-instrumented per-function work view (`calls[]`/`work[]`), **not** `sample`'s full symbol call-tree —
      a coarser, different lens.
    - **Full call-tree at current state.** `/usr/bin/sample <pid>` works on any running process, so it could
      sample the cart you're already playing — but the editor must **know the running cart's PID**, and it
      doesn't: the native Run spawns `proc` as a throwaway local `const` (`main.cjs:729`), no handle kept.
    - **Connection.** Both this and "open two carts at once" need the SAME missing plumbing — **the kernel
      keeping a handle on running cart processes** instead of fire-and-forget. A process handle/table in
      `main.cjs` unlocks attach-profiling, multi-cart, and more direct live-inspection at once. See
      [cart-os.md](design/cart-os.md) → "Why you can't open two carts today."
46. ~~**Editor cart-browser doesn't surface the new metadata facets yet**~~ **FIXED 2026-06-29** *(from the de:meta migration)*.
    Carts now own rich metadata in a `de:meta` block → generated `index.json` (see
    [`design/cart-metadata.md`](design/cart-metadata.md)); the editor's cart browser now surfaces it (`editor/src/shell.js`):
    - `CART_GENRE_ORDER` now **mirrors the canonical `GENRES` vocabulary** in `tools/lint-carts.js` (with a sync
      comment), so all genres get an ordered filter chip — not just the flagged `tycoon`/`tactics`/`word`/`4x` but
      also `maze`/`space`/`lab`/`dating`, which were silently dropped too.
    - the derived **`orientation`** facet (`portrait`/`square`, emitted by `build-cart-index.js`) now has chips
      ("📱 portrait" / "⬛ square", new `orientation` filter axis), surfacing the mobile-shaped carts.
47. **✓ MOSTLY DONE 2026-06-29 — the engine runs WITHOUT Raylib (the `DE_NO_RAYLIB` platform seam)**, the
    foundation for iOS/Switch (iOS Phase 2 / Path B). The real `studio.c` + `sound.h` compile, render, and
    sound with zero Raylib / zero frameworks, verified headless on desktop: omnichord (2D) + **heroes**
    (tilemap+sprites) **pixel-identical** to Raylib; **tb303** audio **byte-identical** to the Raylib `--wav`.
    Built: `runtime/platform.h` (host↔engine seam) · `color.h` (universal `DeColor`) · `raylib_compat.{h,c}`
    (no-Raylib shim: types/enums + ~94 stubbed prototypes, a few real — text metrics, rng, decode) · baked
    ROM fonts (`tools/bake-fonts.c` → `fonts_baked.h`) · vendored `stb_image.h` sprite decode · `de_init`/
    `de_frame`/`de_framebuffer`/`de_audio_render` · `tools/headless-nr.c` (proof harness: frame→PPM, audio→WAV).
    Decision settled: **two renderers behind one seam** (software now, GPU/Metal later). Kept desktop
    bit-identical throughout (build-all, canvas-diff 0px) — every path is `#ifdef DE_NO_RAYLIB`. Full record:
    [`design/engine-portability.md`](design/engine-portability.md).
48. **✓ DONE 2026-06-29 — the REAL engine renders + sounds on iOS (Phase 2 / spike 8).** omnichord (real
    `studio.c`+`sound.h`, zero Raylib) renders **pixel-correct and upright** on the iPhone 15 simulator
    (`ios/history/spike8-omnichord.png`), CoreAudio pulls the real mixer, and UIKit touch drives it (a
    desktop strum through the same `de_touch_*` path goes silent→0.374 peak). Wiring: `ios/project.yml`
    compiles `studio.c`+`raylib_compat.c`+`build/cart.c` with `-DDE_NO_RAYLIB`/`SCALE=1`; `ios/build.sh`
    regenerates the cart via play.js (the "swap a cart" loop, extended to iOS); `CanvasView` flips the
    bottom-up `sw_cbuf` + maps touches to framebuffer px → `de_touch_*` (newly given bodies in
    `raylib_compat.c`); `AudioEngine` splits `de_audio_render`'s interleaved stereo. `tools/build-nr.sh`
    is the desktop recipe. **The AUv3 extension is now a playable instrument rack** hosting the real
    engine: its render block parses host MIDI → `de_midi_event()`, sample-clocks `de_frame()` (the
    cart's keybed plays the notes), and pulls `de_audio_render()`. `AUHostTests` proves it offline —
    silent with no MIDI (peak 0.000), then a host note-on → peak 0.106. Engine seam: `midi_input.h`
    gates CoreMIDI to desktop and exposes `de_midi_event`/`de_midi_bend` (portable host-feed, like the
    web bridge). **The renderer decision is settled** — on-device FPS measured on an iPhone SE 2nd-gen
    (`ios/measure-device.sh`): 2D holds 59–60fps (~5.6ms, ~⅓ budget), `tritex`/3D ~89ms→~10fps →
    [ADR-0024](decisions/0024-software-canvas-is-canonical-for-2d.md) (software canvas canonical for 2D,
    `tritex`/3D GPU-only). **GPU-parity audited + closed:** `pal()` (0px) and scaling were never gaps,
    and **camera rotation now works on the software canvas** (offscreen world layer → rotate-composite;
    a 25° probe is 0.04% off the GPU), so the rotation carts render on iOS. Only `smooth_zoom`'s AA
    degrades (→ plain zoom). **Input wired:** touch→mouse synthesis (the primary finger drives
    `GetMousePosition`/`IsMouseButton*`, so mouse carts play from touch + the harness can drive them)
    + a `de_key_event` key seam — proven by injecting a tap into `hotline` (mouse_pressed → gameplay).
    Open follow-ups (incl. the recommended next) in [`design/ios-plan.md`](design/ios-plan.md) →
    "Phase 2". Full record there too.

49. **✓ SHIPPED 2026-07-03 — `de_switch_cart()` + the per-cart sound context** (umbrella-app
    build-ladder rung 1, [`design/share-panel.md`](design/share-panel.md) §ladder). Multi-cart
    bundles switch whole sound worlds — instrument slots, bus FX, wave tables, bpm — per context
    (0–7), so racks keep their natural slot numbers (the spike's slot-offset wrappers are deleted).
    Mechanism: a per-context **config-request log** replayed over `sound_reset_state()` (not a
    field-by-field snapshot — the master-bus surface is ~40 effect families; a log covers every
    future effect automatically). Ear-verified by the maker (both regressions on this seam were
    heard, not measured: the slot collision, then tempo jumps from `bpm()`'s queue-bypassing
    direct write — fixed as queued `SR_BPM`). **The design rule that fell out is
    [ADR-0027](decisions/0027-sound-state-flows-through-the-request-queue.md): cart-facing sound
    APIs must never write engine state directly — the queue is what makes the context log
    complete.** Deterministic oracle: `tools/bundle-spike/proof-sound.sh` (round-trip corr 1.0000,
    cross-context 0.004). Known not-covered (later rungs): per-cart save dirs (racks share
    `cart.sav` → the loser falls back to its demo song), sprite sheets, `de_state`, and the
    video set-and-hold twins (`pal`/`fillp`/`font`/…).
    **Rung 2 shipped the same day:** `apps/tinyjam/app.json` (the decided `apps/<name>/` home,
    per [ADR-0026](decisions/0026-store-pipeline-in-house-not-fastlane.md)'s layout) +
    `tools/build-app.js` — a manifest builds the multi-cart binary; adding a rack = one line.
    Verified to N=3 (groovebox/epiano/mellotron ad-hoc app). Side-fix that fell out: live-inspection
    requests can target one process (`pid:<n>` line — two headless bundles raced for the shared
    `.bake` request files and served the wrong app's frame; debug-harness.md §Live inspection).
    **Rung 3 shipped the same day too — the launcher cart:** `tools/carts/tinyjam-menu.c`
    (a real registered cart; standalone = demo roster) + a `launcher` manifest field.
    `build-app.js` generates `app_roster.h` from each rack's `de:meta` (title + summary,
    folded to ASCII — bitmap fonts render em-dashes as `?`), the launcher boots first in
    its own ctx slot, and the shim (not the engine — zero new API) implements
    `app_launch(i)`/`app_current()`; TAB now toggles rack ↔ overview instead of
    blind-cycling. Verified headless (menu → acid 136 → yacht 102 → menu + resume marker)
    and via a play.js keyboard script on the standalone menu. Menu look/feel remains the
    maker's call. **Rung 4 shipped the same day — the ladder is complete:** `build-app.js`
    grew a `--mac` flag that hands the linked bundle to `mac-app.sh` with `--name`/`--id`
    from the manifest, so "which app?" is answered once by the manifest name all the way to
    the `.app`. `node tools/build-app.js tinyjam --mac` → `build/Tinyjam.app`, notarytool
    `Accepted` + stapled + `spctl` `accepted / Notarized Developer ID` — opens on any Mac
    with a double-click (`--no-notarize` for a quick local sign). Per-app icon parked
    (shared dreamengine icon). No in-editor "export app" button yet — apps live in `apps/`,
    not the carts panel, so that needs an Apps picker (a later UI rung on this CLI).
    Differing-resolution racks stay parked (next-spike #3; paths A/B/C scoped in the doc —
    B is the tractable per-cart RenderTexture step, C the eventual responsive-layout home).
    **Cross-cart bleed fixed (2026-07-03) — the video twin of ADR-0027:** `de_switch_cart`
    was sound-only despite its name, so an outgoing cart's set-and-hold VIDEO state
    (pal/fillp/font/camera) AND its sprite sheet leaked into the next cart. Now it's an
    umbrella: `de_sound_switch_cart` (renamed sound half) + `de_gfx_reset()` (video state →
    defaults, reset-only — every cart re-sets modes in draw, and video has no config queue
    to replay like sound) + `de_sheet_select(ctx)` (per-cart sheets, baked by build-app.js).
    Proven by the **bleedtest** rig (bleedred↔bleedblue: pre-fix blue drew red text +
    hatched bar + red critters; post-fix clean both ways). Deferred: per-cart maps + save
    dirs (same pattern).
    **iOS multi-cart build (Spike A) — DONE (2026-07-03):** `build-app.js --ios` stages the
    multi-cart set into `ios/gen/app` (per-cart wrapper `.c` files that `#define` the entry
    renames then inline the source — so Xcode's one-defines-set directory build needs no
    per-file flags — plus the shim, `app_roster.h`, baked data, `gen/app.dims`); `project.yml`
    sources the `gen/app` DIRECTORY; `ios/device.sh`/`build.sh` gain `APP=<manifest>` mode.
    **Tinyjam's launcher + racks render on the iOS simulator** (Tiny Jam menu, acid rack /
    session desk from de:meta, `>` cursor, footer). Same `DE_NO_RAYLIB` engine as single-cart
    device builds. Serialize iOS builds (shared `gen/app` + one xcodeproj); `simctl` shots need
    ~a few seconds settle. **On-device: Tinyjam runs on a real iPhone** (`device.sh APP=tinyjam`,
    maker-confirmed 2026-07-03). **Touch back-to-launcher: hold-to-home** (device-confirmed) —
    hold the top-left corner ~0.3s → overview; a big fat-finger hit-pad (a tiny tap chip was
    unhittable on-device), in racks only. Polished nav-bar via a `de_safe_top()` inset (a real
    reflow of the 240px-full racks) is the deferred, maker-driven redesign. Design + gotchas: [`design/share-panel.md`](design/share-panel.md) next-spike #5.
    **IAP in the multi-cart app (Spike B) — BUILT + sim-tested (2026-07-03):** `apps/tinyjam/app.json`
    `iap.products` (price/name/desc/`unlocks[]`) is the single source of truth; `build-app.js --ios`
    GENERATES `Tinyjam.storekit` from it + threads product/price + an `APP_MASTERPASS` "unlock all"
    into `app_roster.h`. The launcher shows a locked rack's price, taps buy it, owned racks open, and
    an "unlock all — $5" row buys the master pass — cross-platform via WEAK `Store_*` stubs (free on
    Mac/editor). Shelf now: acid $2.99 / session desk free / epiano $2.99 / unlock-all $5. Gotchas +
    full record: [`design/ios-plan.md`](design/ios-plan.md) ("IAP in the multi-cart app").
    **STILL TO TACKLE (the umbrella-app backlog, all deferred here):**
    (1) **Device IAP testing** — the sim path is local-only (`SKTestSession`, sim-only); real device
    testing needs App Store Connect **sandbox** testers (the pre-ship validation).
    (2) **Multi-resolution racks / device-adaptive layout** (next-spike #3) — carts must currently
    share the app's dims; epiano/omnichord had to be bumped 200→240 to join. **Now BUILDING:
    [`design/device-adaptive-layout.md`](design/device-adaptive-layout.md)** — live-resizable +
    physically-sized `SCREEN_W/H` so one cart is beautiful on iPhone AND iPad, both orientations; also
    unblocks honest full-bleed store screenshots/videos. Progress (2026-07-03):
    • **Phase 0 DONE** — the layout model is proven in cart-land across the whole shape space:
      `respond` (primitives), `rackfit` (finger-sized emergent reflow), `acidfit` (dense rack +
      progressive-disclosure accordion), `otafit` (drag-ribbons + a *measured* orientation-lock case).
      Findings: query the **finger-ratio** not `device_class` (iPad mini ≠ Pro); disclosure **mode** is
      per-shape (inline / accordion / tabs); orientation may be **locked** per rack; overrides are
      plain-C + `lay_at`=position:absolute; a future `layout-check` oracle can check all of it.
    • **`runtime/lay.h` SHIPPED** — the immediate-mode layout vocabulary (`split/at/cell/grid/wrap/
      aspect/fluid/pad`), a cart-land library header usable by any cart today (registered in the
      cart-authoring library-headers table + `checks-and-oracles.md`).
    • **Phase 1 DONE** — runtime `de_sw`/`de_sh`, per-cart `-DDE_RESIZABLE` opt-in (`de_reflow`), cart
      API `screen_w()`/`screen_h()`, editor ▶-run wiring (`de:meta "resizable"`) + `play.js --resize`
      sweep / `--show-size` overlay. Byte-identical for fixed carts.
    • **Growable framebuffer DONE (2026-07-04)** — the fb reallocs to the active size (`de_ensure_fb`/
      `de_set_canvas`, GPU + SW + smooth), so a resizable cart fills ANY size (not capped at compile max).
    • **Phase 2 DONE (2026-07-04)** — iOS: `de_resize`/`de_is_resizable`/`de_set_safe_area` seam +
      `CanvasView` reflows to the device; a resizable cart FILLS iPhone SE / 15 / iPad Pro 12.9, dodges
      the notch (`safe_rect()`), and reflows on rotate — all verified on the sim. `RESIZABLE=1 ./build.sh`.
    • **Resume at Phase 3** — per-rack density arrangements (the media-query-like adaptation: more
      controls on iPad, collapsed/tabbed on iPhone per `acidfit`'s disclosure model). Engine foundation done.
    (Supersedes the per-cart-RenderTexture / `de_safe_top()` sketch in share-panel.md #3 paths A/B/C.)
    (3) **Polished back-to-launcher** — the `de_safe_top()` nav-bar reflow (hold-to-home is the temp).
    (4) ~~**In-editor "export app" button**~~ — **DONE (2026-07-03).** The **Apps view** (`shell.js`
    `renderAppsList`, IPC `studio:list-apps`/`build-app`/`app-shots`/`press-kit`/`aso-app` in
    `main.cjs`) lists `apps/*/app.json`; click a card → per-app actions grouped **give / site / sell**:
    🍎 Mac app · 📱 iOS app (→ `build-app.js --mac`/`--ios`, reveals the built `.app`) · 📸 screenshots
    (→ `store-shots` into `apps/<name>/screenshots`) · 📄 press kit · 🔎 research keywords
    (seeds `aso-research` from the app's carts' `de:meta` teaches/titles into the research box) ·
    💡 suggest keywords (`aso-suggest` — Google-autocomplete demand proxy; words → compose,
    phrases → website/press SEO) · ✅ lint listing · 🧩 compose
    keywords (the last two read the manifest's `listing` block). Still open: the App Store *upload*
    rung (v3 — TestFlight button over an in-house App Store Connect client, ADR-0026).
    (5) **Before a real release:** verify `StoreKitTest` is fully stripped from device/release builds
    (currently `[sdk=iphonesimulator*]`-only + `#if targetEnvironment(simulator)`), and the App Store
    Connect product/price reconciliation (ADR-0026 store pipeline). Per-cart maps + save dirs also
    still deferred (same pattern as sheets).

---

## Decided-against / deferred ✗

These were considered and **cut** — kept here so the decision isn't relitigated.
Rationale lives in [`design/api-notes.md`](design/api-notes.md)'s "What to defer or skip" and the 2026-05-30 review.

- **A cart-making PLATFORM (outside authors)** — creator accounts, cart upload/sharing from
  strangers, a browser IDE for others, a public compile-strangers'-C server. **Cut**
  (2026-06-22, [decision 0020](decisions/0020-in-house-tool-curated-showcase.md)): dreamengine
  is an in-house tool; the public surface is a **curated showcase** people view (clips) and
  play (wasm), not contribute to. Resolves the old "Sharing the work" open question. The
  tinyjam racks *product* line ([`design/product-notes.md`](design/product-notes.md)) is a separate
  question, unaffected.
- **Process / coroutine model (DIV-style)** — the would-be "Level-2" learning model.
  Every shipped cart works cleanly with plain typed static pools, so it's weeks
  of coroutine/transformer machinery for a model we don't need. [`VISION.md`](VISION.md).
- **Engine-owned entity system** (God-struct / `SELF` / `val[16]` / ECS) — per-game
  typed pools with *named* fields beat all of them for a learn-C console.
- **MMF movement/qualifier engines** (`move_platform`, `move_8dir`) — removes the lesson.
- **`move_and_collide`** (resolved tile push-out) — low demand; only `platform.c` does
  the full pattern, and `zelda`/`gta` test against their own data, not `mget`.
- **DS structures** (lists/maps/grids), **memory arenas**, **PS1 z-sort/ordering table**,
  **tools-as-carts / VFS / fantasy-OS / peek-poke**, a **3D *engine*** (scene graph / mat4
  stack / z-buffer / per-pixel depth) — out of scope. *(The small 3D leaf-helpers
  `rot3`/`project3`/`zsort`/`quadfill` + `V3` ARE shipped — see below and [decision 0009].)*
- **Particle systems** and **pathfinding** — ship as *library carts* (seeds: `astar.c`,
  `boids.c`), not engine surface.
- **Pixel-perfect sprite collision** — eventually maybe; AABB covers 95% first.
- **Turtle graphics API** (`turtle_*`/`pen_*`) — shipped, then **removed 2026-06-01**: only
  `16-spirograph.c` used it, and a turtle is just `dx`/`dy` + `line()`, so it lives in the
  cart now. [decision 0008](decisions/0008-cut-turtle-graphics-api.md).
