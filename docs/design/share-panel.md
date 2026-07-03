# The share panel — one surface for every sharing action, and the "app" unit

STATUS: BUILDING (2026-07-03) — **seam-#1 spike PASSED, then build-ladder rungs 1 AND 2
SHIPPED, all same day**: `de_switch_cart()` + the per-cart sound context (rung 1, ear-verified,
[ADR-0027](../decisions/0027-sound-state-flows-through-the-request-queue.md)) and
`apps/<name>/app.json` + `tools/build-app.js` (rung 2 — adding a rack = one manifest line).
**Fresh context: pick up at §"The umbrella-app build ladder" → rung 3 (launcher cart).** Plans the
cross-cutting row of [`sharing-channels.md`](sharing-channels.md): every sharing action
triggerable from the editor, no Xcode ever. Two design commitments and one new concept
fall out below.

## The two commitments

**1 · No Xcode, no GUI toolchains — ever.** Already the `ios/` ethos (xcodegen +
`build.sh` + `device.sh`; the `.xcodeproj` is generated, never opened) and today's Mac
signing proved it extends to distribution (`mac-app.sh`: codesign → notarize → staple,
all CLI). The App Store rung continues the same way: `xcodebuild -exportArchive` + the
App Store Connect API (in-house, not Fastlane — decided,
[ADR-0026](../decisions/0026-store-pipeline-in-house-not-fastlane.md)). The panel never does
anything a terminal can't.

**2 · Every action is a CLI tool first; the button is a thin trigger that streams its
log.** This is already how every export works (`mac-app.sh`, `publish-cart.sh`,
`device.sh` — main.cjs spawns + streams). New actions land as tools, then get a button.
The panel adds no logic of its own.

## The new concept: the unit of sharing is not always a cart

Every button today operates on *the current cart*. That's right for four of the five
flows — but the Tinyjam product (per [ADR-0023](../decisions/0023-ship-carts-as-apps-not-the-editor.md):
"a cart, or a curated **collection of carts** bundled into one app") makes the shipped
unit a **set**: several racks + a launcher + StoreKit products, as one App Store app.

Proposal: make the unit explicit as an **app manifest** — a small committed file, e.g.
`apps/tinyjam/app.json`:

```json
{
  "name":     "Tinyjam",
  "bundleId": "com.dreamengine.tinyjam",
  "version":  "1.0",
  "icon":     "icon.png",
  "carts":    ["acidrack", "yachtrack"],
  "launcher": "tinyjam-menu",
  "iap":      { "yachtrack": "rack.yacht" }
}
```

- A **single cart is the degenerate case** (a one-cart manifest, or no manifest at all —
  today's flows keep working untouched). Sharing is *usually* one cart; the manifest
  exists for when it isn't.
- **Channel-agnostic**: the same manifest can drive the iOS app (`ios/build.sh
  APP=tinyjam`), a Mac `.app` with the same racks (mac-app.sh already bundles; it would
  gain a multi-cart binary), and later a site page for the collection.
- It's also where per-app identity lives that today has nowhere to go: icon override,
  bundle id, version, IAP mapping (spike 4's StoreKit bridge already gates on product
  ids — the manifest is where they're declared).

## The engine seam a multi-cart binary needs (scoped, not solved)

Verified against the code (2026-07-03): cart internals are `static`, so most of a cart
links cleanly next to another. What actually collides / is singular:

1. **Entry points** — `draw()`/`update()` (+ `spec()`) are global. Fix at bake time:
   compile each cart's TU with `-Ddraw=<slug>_draw -Dupdate=<slug>_update`, generate a
   registry table `{name, draw, update}` the engine switches between.
2. **Staged assets** — `build/sprites_data.h`/`map_data.h` are one-per-build today;
   multi-cart staging needs per-slug files + per-slug symbol/include names, and
   `spr()`/`mget()` need to read the *active* cart's sheet (engine holds one texture —
   swap on cart switch).
3. **`de_state()`** — one blob per process today; needs a per-cart slot (or document
   "switching carts resets state" for v1).
4. **Saves** — `--save-dir saves/<cart>` is per-cart already; the bundle switches dir
   with the active cart.
5. **The launcher** — a menu the player picks a rack from. Simplest honest answer: the
   launcher is *itself a cart* (`launcher` in the manifest) plus one new API to request
   a switch (`de_switch_cart("acidrack")` or similar). No native menu UI.

Each is a small spike in the [`ios-plan.md`](ios-plan.md) spirit: riskiest-cheapest first (1+2 together —
two renamed carts in one desktop binary, switchable by key — proves the whole shape
before any iOS work).

### Spike result (2026-07-03): PASSED — acidrack + yachtrack in one binary

`tools/bundle-spike/` (build.sh + bundle.c + the two proof screenshots). Both cart
sources went in **verbatim — zero edits**; the whole trick is per-TU compile defines
(`-Ddraw=acid_draw -Dupdate=acid_update`, ditto yacht + `spec`) plus a ~30-line
dispatcher shim that owns the real `init`/`update`/`draw` and switches on TAB
(`note_off_all()` as the band panic; `DE_BUNDLE_AUTOSWITCH=<frame>` is the
deterministic headless proof hook). Run it: `tools/bundle-spike/build.sh` →
`build/bundle-spike`.

What came free: **state isolation** (both racks keep all state in file-scope `static`s
— per-TU, untouched across switches; neither uses `de_state`), and macro renames are
safe against comments/strings (the de:meta block survives).

Traps hit: the shim is cart-namespace code too (`frame` is engine API — the
"don't name a variable after a built-in" rule applies); headless runs are
**uncapped** (no vsync), so frame-N proofs need one screenshot per run, not two in one.

**Found the hard way (maker's ears, first build): INSTRUMENT SLOT COLLISION.** The
engine's instrument-slot bank is one global array (48 slots); both racks define their
sounds by slot *number* (acid 5..29, yacht 5..14) — so the last rack to configure a
slot wins and the other rack triggers the WRONG SOUNDS (acid's 909 came back as
yacht's strat). Fixed in the spike by **slot partitioning**: yacht's TU compiles with
its 9 slot-taking calls (`instrument`, the 7 `instrument_*` setters, `schedule_hit`)
renamed to shim wrappers that shift its slots ≥5 up by 25 → yacht plays in 30..39,
zero overlap. Verified via `nm` (yacht.o imports the wrappers, acid.o untouched).
The real fix was an either/or until the maker settled it (2026-07-03): the aim is
**MANY sound apps under one Tinyjam umbrella** — and static slot partitioning can't
scale past ~2 racks (acid alone uses 25 of the 43 cart-definable slots). **Decision:
the engine grows a per-cart sound context behind `de_switch_cart()`** — snapshot the
outgoing cart's slot bank (48 × ~200B ≈ 10KB/cart, trivial) + master-bus FX config,
restore the incoming one's. Only one rack plays at a time, so nothing needs to be
resident twice; the same snapshot/restore kills next-spike #4 (master-bus bleed) for
free, and the same "cart context" umbrella is where the sprite-sheet pointer, save
dir, and `de_state` slab naturally live too. The spike's wrappers stay as the proof
of diagnosis, not the shipping mechanism. (The AUv3 side is untouched by all this —
each rack-as-plugin runs in its own process; the context swap is for the standalone
umbrella app.)

Deliberately NOT covered — the next spikes, in order of need:
1. **Sprite carts** — per-slug staged sheets + a runtime sheet-swap on switch (the
   engine already swaps the spritesheet texture in the `pal()` path, so the mechanism
   exists; it needs a seam like `de_sheet_select(slug)`).
2. **`de_state` carts** — `-Dde_state=<slug>_de_state` + per-slug slab wrappers in the
   shim (sketched, trivial, unexercised).
3. **Differing screen dims** — `SCREEN_W/H` are compile-time; a manifest picks one size
   and its carts must match (or the engine grows letterboxing). Both racks are 320×240,
   so Tinyjam dodges this.
4. ~~**Master-bus FX bleed**~~ — **DONE (2026-07-03), free with ladder rung 1:** the
   config-log replay behind `de_switch_cart()` covers master-bus FX the same as
   per-slot sounds (reset to boot defaults + replay the incoming cart's own config).
5. **Per-cart save dirs** and the **launcher cart** (the spike's shim hardcodes the
   pair; the manifest generates it). `de_switch_cart()` itself shipped with ladder
   rung 1 — what remains here is the dispatch/menu side, not the sound side.

## The panel itself — three rungs

**v1 — regroup + the two missing cheap buttons (no engine work).** One "share" popover
replacing the loose toolbar buttons: the five existing exports, plus **🎬 make clip**
(dropdown of the cart's committed recipes from `tools/clips/<cart>/`, runs
`make-gif.js --recipe`) and **batch publish / staleness** (`publish-all.js -y`,
`cart-status.js` output into the log). All tools exist; this is shell.js + main.cjs
wiring only.

**v2 — the app unit.** The manifest format + the multi-cart desktop spike + `mac-app.sh`
consuming a manifest (a Mac Tinyjam.app with N racks = the cheap full rehearsal of the
App Store shape, shareable to friends immediately via channel C).

**v3 — the store rung.** `ios/build.sh APP=<manifest>` + the upload tool — **decided:
in-house against the App Store Connect API, not Fastlane; only its `metadata/<locale>/`
folder layout is kept**
([ADR-0026](../decisions/0026-store-pipeline-in-house-not-fastlane.md); assets/copy
derive from the cart via `play.js`/`make-gif.js`/`de:meta`, plus a `store-status.js`
drift oracle) — + a "⬆ TestFlight" button in the panel. Gated on channel B's open
product decisions (which app, price, original palette —
[`sharing-channels.md`](sharing-channels.md) §Channel B). The *judgment* layer above this
plumbing (hero-frame selection, copy transcreation, guidelines pre-flight, a whole-page
stranger-legibility audit) is brainstormed in [`store-agents.md`](store-agents.md) —
each split at the script/agent boundary.

## The umbrella-app build ladder — the pick-up point (2026-07-03)

The spike is done and its findings are above. From here to a shippable **Tinyjam.app**,
in order (each rung small, only #1 touches the engine):

1. ~~**`de_switch_cart()` + the cart context**~~ — **DONE (2026-07-03).**
   `de_switch_cart(int ctx)` in studio.h (ctx 0..7). The mechanism is NOT the struct
   snapshot sketched above — mapping the engine found the master-bus FX surface is ~40
   effect families of scattered per-bus statics, so a field-by-field snapshot would rot
   with every new effect. Instead: a **per-context CONFIG-REQUEST LOG**. All set-and-hold
   sound config already funnels through the one SoundReq queue, so the engine records each
   config request at drain (deduped by knob identity — a ridden `filter()` stays one
   entry; events/notes never recorded), and a switch = boot reset (`sound_reset_state()`,
   the libtcc hot-reload clean slate factored out of `sound_init`) + **replay** of the
   target's log through the normal dispatch. Every *future* effect is covered
   automatically. Log overflow trips a `[sound] WARNING` (the soundcheck gate).
   Master-bus FX ride the same log, so next-spike **#4 (master-bus bleed) is dead for
   free**, as predicted. Deterministic oracle: `tools/bundle-spike/proof-sound.sh` —
   same note/slot before-switch vs after-round-trip **corr 1.0000**, vs the other
   context's sound **corr 0.004**. The spike's yacht slot-offset wrappers are deleted
   (git history keeps the diagnosis); both racks play their natural slot numbers.
   **Maker's-ears regression, same day: tempo jumped on TAB.** `bpm()` was the ONE
   config call that bypassed the queue (a direct global write) — the incoming cart's
   main-thread `bpm()` landed *before* the audio thread snapshotted the outgoing one's,
   polluting it. Fix: `bpm()` is now a queued request (`SR_BPM`) like all config, so it
   records/replays with everything else and the by-hand snapshot is gone. **The rule
   this hardened: a cart-facing sound API must NEVER write engine state directly — the
   queue is what makes the context log complete.** Verified: acid 136 → yacht 102 →
   acid 136 (on-screen bpm labels, headless autoswitch round trip).
   Still TODO from the original sketch, for later rungs: sprite-sheet pointer, save
   dir, `de_state` slab joining the context — plus the **video twin** of this leak
   family someday: `pal()`/`palt()`/`fillp()`/`font()`/`camera()`/`clip()` are
   set-and-hold too and currently bleed across a switch (racks redraw their own state
   every frame, so it doesn't bite Tinyjam yet).
2. ~~**Manifest + generator**~~ — **DONE (2026-07-03).** `apps/tinyjam/app.json` (the
   decided `apps/<name>/` home) + `tools/build-app.js`: stages assets, compiles each
   cart TU with the `-D<entry>=<slug>_<entry>` renames, detects optional entry points
   via `nm` (no hand-written stubs — yacht's missing `init()` is found, not known),
   generates the dispatcher shim (TAB cycles N carts, each switch = `de_switch_cart(ctx)`),
   links → `build/<name>`. **Adding a rack = one manifest line.** Guards: cart count vs
   `SOUND_CART_CTX` parsed live from sound.h (no drift), screen/grid dims must match
   across carts (next-spike #3 honored), first-cart-with-sprites staging warns when
   several carts bring sheets (next-spike #1 still open). Verified: tinyjam (acid 136 →
   yacht 102 → acid 136 restored) and an ad-hoc THREE-cart app (groovebox → epiano →
   mellotron → groovebox, pattern + 123bpm intact) — N>2 works. `launcher`/`iap`/`icon`
   manifest fields are accepted-but-parked for rungs 3/4/iOS. The hand-written
   `bundle-spike/` stays as the minimal reference + proof-sound.sh home.
3. **Launcher cart** — the menu screen (fed by each rack's `de:meta`), replacing TAB.
4. **`mac-app.sh` consumes the manifest** → a signed, notarized Tinyjam.app with the
   full roster: channel C's rehearsal of the App Store shape. iOS then reuses
   everything ([`ios-plan.md`](ios-plan.md); AUv3 concurrency = its spike 9).

## Open questions (maker's call)

1. ~~Manifest home~~ — **DECIDED (2026-07-03, maker): `apps/<name>/` dir.** Not just
   preference — [ADR-0026](../decisions/0026-store-pipeline-in-house-not-fastlane.md)'s
   convention ("copy and assets live as versioned files *next to the app manifest*",
   Fastlane's `metadata/<locale>/` + `screenshots/<locale>/` layout kept verbatim)
   requires a directory per app; a flat json couldn't hold the tree. Shape:
   `apps/tinyjam/{app.json, icon.png, metadata/<locale>/*.txt, screenshots/<locale>/,
   previews/<locale>/}` — screenshots/previews derive from committed clip recipes
   (play.js / make-gif.js), copy drafts from de:meta, and `store-status.js` diffs this
   one directory against the live listing.
2. Launcher-as-cart: good with the one-new-API cost (`de_switch_cart`)? It keeps the
   menu in cart-land (lo-fi, ours) instead of native chrome. With MANY racks under the
   umbrella (the maker's stated aim) this is a real menu screen, not a TAB toggle —
   each cart's `de:meta` (title/description) can feed it via a generated header.
   *(The sound half of `de_switch_cart` already shipped with ladder rung 1 — the
   remaining cost is only the dispatch/registry side.)*
3. v1 panel placement: popover off one "share" toolbar button, or a tab like settings?
