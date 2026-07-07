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

_Last updated: 2026-07-07 (seven lanes: worldgen ladder — rungs 0–3 SHIPPED in one day, rung 4 next · multiplayer — rung 5b WebRTC P2P BUILT + published (pong live on github.io; ~12ms direct over wifi, relay = signaling only); step 5 (adaptive NET_DELAY) + step 7 (TURN) PARKED · device-adaptive-layout — the acidwire WIREFRAME CART is built + INTERACTIVE + runs on the iOS sim (dual-mode desktop/device, 4 states incl. focus, touch+mouse, real 303 piano-roll + drum grids, iPad 2×2 grid both orientations, finger-honest); guide interactive-wireframes.md; next = play on glass → narrow-303 input model → R5 re-land into acidrack.c · store/ASO — the ASC upload tool BUILT (`tools/asc-push.js`, ADR-0026): keywords + screenshots pushed live, all 3 IAPs `READY_TO_SUBMIT`; product ids renamed to `com.mipolai.tinyjam.*` (sim-reverified) · editor media — record/replay shipped, the panel deferred · responsive instrument UI — the playbook + ADR-0028 + the epianofit mock shipped; epiano brief re-scoped to the FAITHFUL piano; the scale-grid SHIPPED as the `scalegrid` cart (device-tested, spec 71/0), open step = extract it into a `grid.h` library then wire epiano's editor-swap · leads — the marketeer tool + ledger BUILT (`tools/leads.js`, 18 tribes): cart→tribe→venues, taxonomy being filled cart-by-cart, editor Apps-page surface next → resume `design/leads-marketeer.md`)_

---

## Where we are right now

**Seven lanes are active in parallel right now** (different areas — pick the one you're resuming):
(1) **the worldgen ladder** (realistic procedural roadgen), (2) **multiplayer — WebRTC P2P (rung
5b)**, (3) **device-adaptive layout**, (4) **store / ASO + the app-trailer builder**, (5) **editor
media (record/replay + where it lands)**, (6) **responsive instrument UI + the scale-grid**, and
(7) **leads — the local marketeer** (find venues to post + track outreach). All below; none is
"the" thread. Shipped/open ledger for all: [`STATUS.md`](STATUS.md) + the design board.

> **▶ ACTIVE THREAD (2026-07-06) — the worldgen ladder (realistic roadgen).** One day, four rungs
> of [`design/worldgen-plan.md`](design/worldgen-plan.md)'s ladder shipped, each gated + committed
> with a deterministic clip:
> **Rung 0** `tools/sndi-check.js` (the street-network metric oracle + the real-city target table).
> **Rung 1** = Track-A A1: **`runtime/worldnet.h`** — roadnet2's world core extracted to a shared
> library header (terrain + ranked lattice + spline links + the unified `wn_road_at()` nearest-edge
> query over an edge cache + spatial hash, edge-type field road/rail/water pinned); **sloop's N key
> drives the spine** behind its `road_at()` seam (grip 1.0/0.55; `spec.js sloop` 25/0; clips
> `roadnet2/01-rung1-onoff` + `sloop/04-rn2-spine`).
> **Rungs 2+3** in the new **`citygrow` bench cart**: the population-density field (settlement
> presence/rank/size/extent from D; O = the field-vs-hash A/B; world bound implemented at 500 km —
> **maker feel-check pending**) and per-city **tensor-field arterials** (T = city view, X = export
> sndi JSON; first generated-vs-real numbers: mean degree 2.65/2.95 vs Amersfoort 2.71; clips
> `citygrow/01-field-vs-hash` + `02-city-arterials`).
> **Resume-at: [`design/worldgen-plan.md`](design/worldgen-plan.md) → "Rung 4 pick-up" note under
> the rung ladder** (district polygons from the arterial graph's planar faces → generalize
> streetlab's five pattern generators to fill a district polygon → stitch under the continuity
> tenet). Hot files: `tools/carts/citygrow.c` (the bench — rungs 2–5 live here),
> `runtime/worldnet.h` (shared — targeted edits only, roadnet2 + sloop include it),
> `tools/carts/roadnet2.c`, `tools/carts/sloop.c` (its rn2 block). Gates to keep green:
> `spec.js sloop` 25/0 · the three committed clips replay byte-identical · `sndi-check
> build/citygrow-graph.json` after any generator change.

> **▶ ACTIVE THREAD (2026-07-07) — responsive instrument UI: playbook, epiano, scale-grid.**
> A research question ("what's the best responsive UI for a music cart?") turned into
> reusable process + two live design docs + a clearly-scoped new feature to build. **What shipped
> (docs/tools, all committed):**
> - [`design/acidrack-ui-research.md`](design/acidrack-ui-research.md) — external survey of the
>   303/909/808 + best clones + the touch/density numbers (48px floor, band table).
> - [`guides/responsive-instrument-ui.md`](guides/responsive-instrument-ui.md) — the reusable
>   **playbook**: sound→inventory→steal-IA→tier→**brief**→prototype→sweep→hands→ship, with the
>   field-note-018 traps baked in as guards.
> - [`decisions/0028-sensible-defaults-optional-tweaks.md`](decisions/0028-sensible-defaults-optional-tweaks.md)
>   — the rule: pick the stranger-legible default, ship it, leave a **seam**; don't agonize, don't
>   over-configure. Wired into design-system §5 + the playbook.
> - `tools/carts/epianofit.c` — the step-4 layout **MOCK** (no audio): device-fit + finger unit +
>   disclosure across all shapes. Keys: `1-5` lock device / `0` auto / `m` machine / `f` fx / `s`
>   scale / `r` key / `i` iso-layout / `g` force piano-or-grid / `n` native full-bleed.
> - [`design/epiano-layout-brief.md`](design/epiano-layout-brief.md) — **re-scoped** to the FAITHFUL
>   epiano (the classic `keybed.h` piano that scales with width + a disclosing sound panel).
> - [`design/scale-grid.md`](design/scale-grid.md) — the scale-locked isomorphic pad grid **split
>   out** as its own feature (a *general* note surface, not epiano's soul — the maker wants the piano
>   kept AND the grid, eventually).
> - **SHIPPED (2026-07-07): the `scalegrid` cart** (`tools/carts/scalegrid.c`) — the playable,
>   sound-bearing showcase, device-tested on multitouch, pinned by a **71-assertion `spec()`**. 11
>   scales (incl. blues + the SoundForest "FOREST" voicing), ROW = OCT↔4TH toggle, SQR↔HEX packing
>   (equidistant-neighbour Tonnetz grid, nearest-centre hit-test, pixel-correct regular hexagons),
>   fill-both-dims finger-first sizing + a SIZE cycle, and a VOICE cycle (PD/EPIANO/MALLET/ORGAN/PLUCK).
>   No-gap lattice proven across all scales × both modes. The maker's verdict on glass: "a very nice
>   musical toy."
>
> **Resume-at: the scale-grid "where does it live" question (scale-grid.md §3) is ANSWERED B→C** —
> built as its own cart first, grid maths kept in self-contained pure fns (`compute_grid`/`pad_midi`/
> `pad_center`/`pad_at`/`hex_verts`). **The one open step: extract those into a `grid.h` library**
> (twin of `keybed.h`, reuse `solo.h`'s scale-lock) so the whole shelf reuses it — then wire epiano's
> optional **editor-swap** to it. Separately, epiano's faithful Phase-3 (piano scales with width) per
> its brief. **Both, eventually — the grid does not replace the beloved piano.**
>
> **Hot files:** `tools/carts/scalegrid.c` (the shipped grid — extract from here), `runtime/solo.h`
> (scale-lock to reuse for grid.h); `tools/carts/epianofit.c` (the earlier silent layout mock, still
> the epiano-brief reference). Gate: `node tools/spec.js scalegrid` (71/0).

> **▶ ACTIVE THREAD (2026-07-07) — multiplayer: WebRTC P2P (rung 5b) — SHIPPED + PUBLISHED (follow-ups parked).**
> Steps 1–4 shipped (commit `05a5dc76`): the WebRTC DataChannel is now the WEB game
> transport (`de_rtc_*` EM_JS shim in `runtime/net.h`), the relay reused **unchanged**
> as signaling only. Play-tested Mac↔iPhone over wifi at LAN speed — the rung-5a
> problem (3 fps + freezes from tromboning through the Render relay at ~330 ms) is
> gone. Signaling + the seed handshake ride the channel; everything above the
> `net_transport_*` seam is untouched. The spike's two potholes are baked in
> (joiner-announces-first; binary signaling told from `ROLE` by the `DN` magic).
> **Jitter fix so far = a blunt one:** the phone's ~70 ms wifi radio-sleep spikes
> stalled the old 3-frame/50 ms cushion (a 1-frame hitch every 1–2 s), so `NET_DELAY`
> is bumped to a **fixed 10 frames (~165 ms)** — feels good, at the cost of input lag
> on clean links. **Pairing UI:** a Host/Join split (gallery + in-cart bar); Join via
> native `prompt()` because an inline `<input>` is blocked by the running cart's key
> handlers on iOS. **Resume-at:** [`design/multiplayer-research.md`](design/multiplayer-research.md)
> §"rung 5b" step table. **Published 2026-07-07** — pong is live on github.io (it's
> the only netplay cart, so that's the whole rollout; the Render relay needed no
> redeploy — it's signaling only now). **PARKED follow-ups** (not being worked):
> **step 5 (adaptive `NET_DELAY`)** — the fixed 10-frame cushion feels good but adds
> lag on clean links; adaptive sizes it to live jitter to claw that back (the
> maker's-call "when we want to sand off the floatiness"). **step 7 (TURN)** — for the
> un-punchable ~10–20% (today they see "connection failed - reload"); needs a free
> Cloudflare/Metered account. Hot file if resumed: `runtime/net.h` (targeted `Edit`s,
> shared). Gate: `node tools/net-check.js`. Local play-test: `node tools/net-relay.js --serve site`.

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
> callback, do NOT re-try the callback route; iOS rotation is fine.
> **Update 2026-07-07 — the WIREFRAME CART is built and mature: `tools/carts/acidwire.c`** (chosen
> over an HTML mock — a cart has no translation gap, IS the production path, field-018). It's a full,
> **interactive** device-matrix wireframe + an exemplar (guide:
> [`guides/interactive-wireframes.md`](guides/interactive-wireframes.md)). What it has:
> **dual-mode** — desktop = fake-device flipper (flip all 11 [`device-matrix.md`](design/device-matrix.md)
> shapes, tap label); **device (`-DDE_RESIZABLE`)** = classifies the REAL screen + safe_rect and fills
> it (RUNS ON THE iOS SIM: `cd ios && RESIZABLE=1 CART=acidwire ./build.sh`; physical:
> `./device.sh` — device.sh gained `RESIZABLE=` this session). **Four states** folded/compact/
> **expanded**/**focus** (fullscreen, name = back). **Touch+mouse** click layer (`clicked(box)`): tap
> strip name to open→focus, M mute, pattern chip select, tab name/M, play/stop. **Per-instrument**
> mute + a **flat 6 patterns** (device-adaptive: iPad boxed PAT panel, phone header row). **Real
> editors**: 303 = pitch piano-roll + slide/accent lanes + knobs; drums = full voices×steps grid
> (factual 909/808 voice names). **Arrangements**: phone tall = expand+compact+fold; phone landscape =
> tabs (per-tab M); **iPad BOTH orientations = 2×2 machine grid + master bar** (kills the portrait
> void). **Finger-honest**: `FU`=22 logical px, a `g` finger-grid overlay; header made finger-tall.
> GOTCHA banked: raylib letter keycodes are UPPERCASE (`keyp('W')` not `'w'`). Brief kept current:
> [`design/acidrack-layout-brief.md`](design/acidrack-layout-brief.md).
> **Resume at:** play acidwire on GLASS, then (a) the **narrow-303 input model** (4-row vs 2-row+gesture
> — a touch-ergonomics call to make on device, brief §3), (b) dense adjacent rows on the smallest
> phones (11-voice selector / 16-step columns → paging or prev/next), then (c) **R5**: graduate this
> layout into the real `acidrack.c` (with audio). Parked: landscape side-notch inset + background-audio
> policy (keep-playing vs pause-on-Home).

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
> **Update 2026-07-07 — the ASC upload tool is BUILT: `tools/asc-push.js`** (the store track's
> "one big unbuilt piece", ADR-0026). In-house against the App Store Connect API, zero deps, proven
> LIVE against Tiny Jam: **keywords + app screenshots pushed**, and **all 3 IAPs created →
> localized → priced → availability → review-shot → `READY_TO_SUBMIT`**. `--metadata`/
> `--screenshots`/`--iap`/`--dry-run`/`--check`. Auth in `~/.appstoreconnect/` (`.p8` + `config.json`,
> never git; `*.p8` gitignored). **Also this session:** the IAP product ids were renamed to the
> bundle-nested scheme **`com.mipolai.tinyjam.{acidrack,epiano,masterpass}`** (was `com.tinyjam.*`;
> rebirth→acidrack) across `app.json` + `Store.swift`/`canvas.c`/`Tinyjam.storekit` + the two iOS
> tests, and the `.storekit` was resynced to the manifest (dropped a phantom "funk", fixed the master
> pass $19.99→$5.00) — **purchase flow re-verified on the iPhone 16 (18.4) sim**. **Resume at:** the
> credentials are set up (Key `Z5DTR9TFW2`); next store moves are per-locale `metadata/<locale>/`
> folders + an editor button for `asc-push`, then the maker-gated submission. Snapshot in
> [`design/store-agents.md`](design/store-agents.md) §"ASC upload + TestFlight tool".

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

> **▶ ACTIVE THREAD (2026-07-07) — leads: the local marketeer (demand generation).** A new
> tool that answers "where do I post about this cart?" — the generation twin of the `aso-*`
> capture tools. **What shipped (committed):** `tools/leads.js` (7 commands: `match` cart→tribe→
> venues · `discover` venue-hunt links + Google-autocomplete signals · `draft` a gift-first post
> scaffold from the cart's own words · `track` outreach log · `audit` whole-catalogue coverage,
> free/local · `list` · `--check`) + `tools/leads-ledger.json` (committed, hand-editable; **18
> tribes**/9 cross-cutting, seeded from tinyjam-marketing §3.9). The model is **buckets**: a tribe =
> tags + venues + hook; carts auto-match on `de:meta`; a `domain` (music/game/any) pre-filter keeps
> games off music-press venues. Reddit's free API is dead (403) — discover uses free Google
> autocomplete + search-url launchers.
> **Update 2026-07-07 — the MUSIC TAXONOMY is substantially filled: 18 → 32 tribes, music coverage
> 62% → 90%.** The big idea (from a maker insight): **the tribe is the SCENE, not the FORMAT.** The
> genre-radio carts each homage a specific artist (jingle=Mac DeMarco, eno=Brian Eno, afrobeat=Fela,
> house=Daft Punk) → scene tribes keyed on the identity word, with `generative` (r/generative·lines·
> Disquiet) as a cross-domain FORMAT amplifier. Added: ambient/citypop/afrobeat/frenchhouse/
> indie-jangle/microtonal/generative, `piano`+`vintage-poly` (homed the piano/juno misfiles),
> physical-modeling/guitar/world-folk (acoustic cluster), novelty-toy/vocal-synth. Fixed the `moog`
> subtractive over-match (31→14) and the `drone`/`vowel`/`vocoder` generic-adjective noise (tag on
> identity, never technique). Model refinement written into `leads-marketeer.md`.
> **PARKED (maker's call):** the 4 weak-room scenes (satie/bossa/mariachi/tango — stay on the
> generative amplifier) + games buckets (GTM: web-gallery-only; `arcade` is the one game tribe).
> **Resume-at: [`design/leads-marketeer.md`](design/leads-marketeer.md) → open-question #4** — the ONE
> remaining build task: the editor **Apps-page** surface (a cart/app card: tribes + venues + draftable
> post + free-form discover box; mirrors `aso-score`'s 📊 glance). Tool + 90% taxonomy are ready to
> surface. Hot files: `tools/leads-ledger.json` (hand-edit to add venues), `tools/leads.js`. Gate:
> `node tools/leads.js --check`.

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
