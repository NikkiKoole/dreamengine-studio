# The share panel — one surface for every sharing action, and the "app" unit

STATUS: BUILDING (2026-07-03) — **seam-#1 spike PASSED, then the WHOLE build-ladder (rungs
1–4) SHIPPED, all same day**: `de_switch_cart()` + the per-cart sound context (rung 1,
ear-verified, [ADR-0027](../decisions/0027-sound-state-flows-through-the-request-queue.md)),
`apps/<name>/app.json` + `tools/build-app.js` (rung 2 — adding a rack = one manifest line),
the launcher cart (rung 3 — `tinyjam-menu.c` + the generated `app_roster.h`; Tinyjam
boots into a de:meta-fed menu, TAB toggles rack ↔ overview, zero new engine API), and
`build-app.js --mac` (rung 4 — wraps the bundle via `mac-app.sh`, name+bundleId from the
manifest; **Tinyjam.app built, notarized `Accepted`, stapled, Gatekeeper-clean 2026-07-03**).
Then the **cross-cart bleed fix** (the video twin of ADR-0027: `de_switch_cart` is now a
sound+video+sheet umbrella — see next-spike #1 + rung-1's video-twin note) and **iOS
(Spike A): `build-app.js --ios` stages the multi-cart set for the Xcode build and Tinyjam's
launcher + racks render on the iOS simulator** (`ios/device.sh`/`build.sh` `APP=<name>`), then
**on-device: Tinyjam runs on a real iPhone** (`device.sh APP=tinyjam`, maker-confirmed
2026-07-03), and **hold-to-home** (hold the top-left corner ~0.3s → overview; a shim-drawn
fat-finger pad in racks only) as the temporary touch back-to-launcher.
**Fresh context: the ladder is COMPLETE — a manifest → a double-clickable any-Mac app AND a
multi-cart iOS app on a phone. Next work is the panel itself (§"The panel itself"), the polished
back-to-launcher (a `de_safe_top()` nav-bar the racks reflow to — hold-to-home is the temporary), or the menu's
look/feel (rung 3 sub-question b — maker's call).** Plans the
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
1. ~~**Sprite carts**~~ — **DONE (2026-07-03).** `build-app.js` bakes a sheet PER cart
   (ctx order) into an indexed table (`SPRITES_SHEETS[]` / `SPRITES_MULTI`); `studio.c`
   pre-loads all at init (both the Raylib GPU and the DE_NO_RAYLIB/iOS SW paths) and
   `de_sheet_select(ctx)` swaps the active `spritesheet`/`_img` on switch (cheap struct
   copy). Folded into the `de_switch_cart` umbrella (see the video-twin note under rung 1).
   Proven by the **bleedtest** rig (bleedred↔bleedblue round-trips clean). Per-cart *maps*
   are the same pattern, still deferred.
2. **`de_state` carts** — `-Dde_state=<slug>_de_state` + per-slug slab wrappers in the
   shim (sketched, trivial, unexercised).
3. **Differing screen dims** — `SCREEN_W/H` are compile-time `#define`s (`studio.h`),
   baked into `studio.c` in ~76 places (framebuffer, clip bounds, blit loops) and shared
   by the ONE `-D` value the whole binary compiles with — so "a cart's resolution" is a
   property of the *binary*, not the cart. `build-app.js` hard-rejects a mismatch for that
   reason. Both Tinyjam racks are 320×240, so it dodges this. Three ways out, cheapest
   first (scoped 2026-07-03, deferred — no concrete two-shapes need yet):
   - **A — letterbox at one master size (small, leaky):** window stays one size (max of
     the carts / a manifest field); the dispatcher centres smaller carts with a
     `camera()`+`clip()` border. But a cart still *sees* the master `SCREEN_W`, so only
     resolution-relative carts survive; hardcoded pixel layouts break. Cheap, not honest.
   - **B — per-cart RenderTexture, runtime-sized (medium, the right near-term one):** each
     cart TU already compiles separately, so it also takes its own `-DSCREEN_W/H`; give it
     its own RenderTexture at that size and make `studio.c`'s ~76 internal uses read the
     **active target's** dims at runtime instead of the macro. The dispatcher composites
     each cart's texture into the window (centre/letterbox or scale-to-fit). **Cart code
     changes zero** — it keeps `SCREEN_W` as its own honest constant. Cost is confined to
     `studio.c`'s draw/clip core, fully under the `canvas-diff`/`mirror-diff` gates. Pays
     for itself twice: the same change unblocks iOS varying-device sizes + resizable
     windows.
   - **C — fully runtime-variable screen dims (large, the eventual home):** `SCREEN_W`
     becomes a runtime global and carts author against whatever size they're handed — a
     **responsive-layout** model where a cart *responds* to its viewport (pairs with the
     cart `respond`/reflow direction). Biggest blast radius (touches cart-land
     conventions), so it's a deliberate later project done together — not a bundling
     spike. B is the stepping stone; C is where it lands.
4. ~~**Master-bus FX bleed**~~ — **DONE (2026-07-03), free with ladder rung 1:** the
   config-log replay behind `de_switch_cart()` covers master-bus FX the same as
   per-slot sounds (reset to boot defaults + replay the incoming cart's own config).
5. **Per-cart save dirs** — the one still-open bit of this entry. ~~The launcher
   cart~~ shipped as ladder rung 3 (`tinyjam-menu.c` + generated roster), and
   `de_switch_cart()` with rung 1; racks sharing one `cart.sav` is what remains.

## What the panel is — one system: unit × audience (2026-07-03)

The panel is **one concept — *sharing to the world*** — not a pile of separate export
buttons. It has two axes:

- **Unit** (what you share): a **cart** · an **app** (a manifest bundling carts) · a
  **showcase** (a cross-cart reel/gallery — `compose-clips.js`).
- **Audience** (who you share to = [`sharing-channels.md`](sharing-channels.md)'s channels):
  **a person** (give a file / signed app) · **my site** (web / gallery / press) · **buyers**
  (the App Store).

Every action is a **cell** in that grid — `(cart, person) → signed Mac .app`,
`(app, buyers) → keywords + screenshots + upload`, `(showcase, site) → the reel on the
gallery`. **Prepare vs publish** is the *mode within* a cell: *prepare* (make an asset —
local, safe, re-runnable) vs *publish* (send it outward — confirm-first). Some cells are
empty by nature (you can't sell a *lone cart* on the App Store — that's an app); the
emptiness is the structure.

**Most cells are already built** — the six store/press tools (`aso-research`/`aso-compose`/
`aso-lint`, `store-shots`, `store-contact`, `press-kit`) + `build-app`/`mac-app.sh` +
`make-gif`/`publish-all`. The panel is now mostly **glue over an existing toolbox.**

**What it does to the UI:**
- One reusable **"share surface"**: given a unit, render its non-empty cells, grouped by
  audience (section headers = *Give it to someone · Put it on my site · Sell it*), with
  prepare-actions visually separated from publish-actions, each button running a CLI tool and
  streaming its log (commitment #2).
- **Two mount points now**, because you *reach* the units differently:
  - **Share** — topbar, unit = the *open cart*. Only cart-valid cells show, so the humble
    "hand someone this cart as a signed Mac app, no marketing" path stays one click.
  - **Apps** — a new list reading `apps/*/app.json` (parallel to the carts panel), unit = a
    *manifest*. It **mirrors the carts flow one level up**: the list is not a dead table —
    *click an app → its* share/publish actions appear (the same share surface, fed the app).
    So the whole product ceremony (keywords/screenshots/press/upload) lives **in the Apps
    view, scoped to the selected app**, not in the per-cart popover.

    ```
    CARTS:  carts panel → click a cart → it opens → topbar [Share ▾] acts on it
    APPS:   Apps view   → click an app  → its [share / publish actions] appear
    ```

    The **asymmetry is intentional** (topbar for carts, in-view for apps): a cart is *open* —
    you're editing it — so Share is a topbar action on the current thing; an app isn't
    something you "open and edit", you *browse to it* and act on it there. The Apps view also
    earns its keep beyond listing: **`+ New`** (writes a manifest) and **promote a cart into
    an app** ("add this cart to …") — a small project/releases hub, not a one-row table. This
    is the main new structural piece the model needs.
  - A third **Showcase/Site** mount comes later (cross-cart reels + gallery curation) — don't
    force it into the first two.
- **Cheap to rearrange:** a mis-placed button is ~10 lines of `shell.js` — the *tool* doesn't
  move. So start with Share + Apps and let the grouping teach you.

The three rungs below are the **build order** within this model, not a different design.

## The panel itself — three rungs

**v1 — regroup + the two missing cheap buttons (no engine work).** One "share" popover
replacing the loose toolbar buttons: the five existing exports, plus **🎬 make clip**
(dropdown of the cart's committed recipes from `tools/clips/<cart>/`, runs
`make-gif.js --recipe`) and **batch publish / staleness** (`publish-all.js -y`,
`cart-status.js` output into the log). All tools exist; this is shell.js + main.cjs
wiring only.

> **Status (2026-07-03):** the **Share popover SHIPPED** — the export regroup + a loaded-cart
> thumbnail/name header, actions grouped by audience (`editor/index.html` + `shell.js`/`.css`).
> Still open in v1: **batch publish**, and **🎬 make-clip** — deferred + **gated behind a
> setting** (you don't always want it visible). And make-clip is more than a trigger: it's a
> clip **browser *and* trigger** — see the cart's already-baked clips (`tools/clips/<cart>/`,
> `editor/public/clips/`) *and* mint a new one. It's the *output* end of the interaction
> recorder / live-loop-multitouch authoring in
> [`input-recording-looper.md`](input-recording-looper.md).

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
   **The video twin — DONE (2026-07-03).** `de_switch_cart` was sound-only despite its
   name, so set-and-hold VIDEO state leaked across a switch. Fixed by making it an
   **umbrella**: `de_sound_switch_cart` (the renamed sound half) + `de_gfx_reset()`
   (pal/fillp/font/camera → boot defaults; clip already resets at frame-end, no palt in
   this engine) + `de_sheet_select(ctx)` (per-cart sheets — next-spike #1, also DONE). **Reset-only,
   not record+replay** like sound — every cart re-sets its video modes in `draw()`, so a
   clean slate suffices, and video has no config queue to tap the way sound did (the
   maker's call: full-replay is far more work for tiny payoff). Proven by the **bleedtest**
   rig. Still deferred for later rungs: per-cart save dir, `de_state` slab, per-cart maps.
2. ~~**Manifest + generator**~~ — **DONE (2026-07-03).** `apps/tinyjam/app.json` (the
   decided `apps/<name>/` home) + `tools/build-app.js`: stages assets, compiles each
   cart TU with the `-D<entry>=<slug>_<entry>` renames, detects optional entry points
   via `nm` (no hand-written stubs — yacht's missing `init()` is found, not known),
   generates the dispatcher shim (TAB cycles N carts, each switch = `de_switch_cart(ctx)`),
   links → `build/<name>`. **Adding a rack = one manifest line.** Guards: cart count vs
   `SOUND_CART_CTX` parsed live from sound.h (no drift), screen/grid dims must match
   across carts (next-spike #3 honored), a sprite sheet baked PER cart (next-spike #1,
   swapped on switch by de_sheet_select). Verified: tinyjam (acid 136 →
   yacht 102 → acid 136 restored) and an ad-hoc THREE-cart app (groovebox → epiano →
   mellotron → groovebox, pattern + 123bpm intact) — N>2 works. `launcher`/`iap`/`icon`
   manifest fields are accepted-but-parked for rungs 3/4/iOS. The hand-written
   `bundle-spike/` stays as the minimal reference + proof-sound.sh home.
3. ~~**Launcher cart**~~ — **DONE (2026-07-03), zero new engine API as predicted.**
   `tools/carts/tinyjam-menu.c` — the menu IS a cart (baked + registered like any other;
   standalone it runs against a built-in demo roster, so it stays play.js-drivable).
   `build-app.js` grew the `launcher` manifest field: the launcher TU compiles with
   `-DAPP_BUNDLE` and a generated **`app_roster.h`** (one entry per rack — title +
   description-summary straight from the rack's `de:meta`, so adding a rack auto-adds
   its menu entry), boots first in a **ctx slot of its own** (ctx 0; racks shift to
   1..N and count against `SOUND_CART_CTX`), and picks racks via two shim-implemented
   (not engine) functions: `app_launch(i)` → `de_switch_cart` + dispatch, and
   `app_current()` → roster index of the rack you came from (the menu's green resume
   marker). TAB is sub-question (a)'s v1: **toggles rack ↔ overview** (the blind cycle
   is gone whenever a launcher is present; launcher-less manifests keep it). The list
   scrolls to keep the selection visible, so MANY racks fit. Two traps recorded:
   de:meta prose carries em-dashes/curly quotes the bitmap fonts render as `?` —
   `build-app.js` folds roster strings to ASCII; and headless bundle runs are
   60fps-capped (vsync-off is a play.js-harness thing), so autoswitch proofs are
   wall-clock-timeable. Verified: headless autoswitch round trip (menu → acid 136 →
   yacht 102 → menu + resume marker, live-inspection screenshots) and a play.js script
   driving the standalone menu (DOWN/ENTER picks entry 2). Still open, deliberately:
   (b) the menu's **look/feel is the maker's call** (current look = a humble program
   picker: title rows + summary footer — program-picker / cartridge shelf / radio-dial
   are all on the table; ask before polishing). The **intro-panel seam** (de:meta title
   card doubling as back-to-overview in bundles, pause-key entry) stays the docced
   touch-era upgrade — TAB doesn't exist on phones.
4. ~~**`mac-app.sh` consumes the manifest**~~ — **DONE (2026-07-03), the ladder is
   complete.** `build-app.js` grew a `--mac` flag: after linking the multi-cart binary it
   hands it to `mac-app.sh` with `--name`/`--id` **straight from the manifest** — so "which
   app?" is answered ONCE, by the manifest name, all the way from carts → binary → `.app`.
   `node tools/build-app.js tinyjam --mac` → `build/Tinyjam.app`; `--no-notarize` does a
   quick local-only sign. Per-app icon stays parked (falls back to the shared dreamengine
   icon). **Verified end-to-end:** Tinyjam.app built (320×240, launcher + acid + yacht),
   notarytool `status: Accepted`, ticket stapled, `spctl` → `accepted / source=Notarized
   Developer ID` — opens on ANY Mac with a plain double-click. Cert note: the Developer ID
   is minted 2026-07-03, valid to 2031 (not expiring — see the machine memory). This is
   channel C's rehearsal of the App Store shape. No editor button yet —
   apps live in `apps/`, not the carts panel, so an in-IDE "export app" needs an **Apps**
   picker (a later UI rung on top of this CLI, not a prerequisite).
5. ~~**iOS multi-cart build (Spike A)**~~ — **DONE (2026-07-03), Tinyjam's launcher + racks
   render on the iOS simulator.** `build-app.js --ios` stages the multi-cart set into
   `ios/gen/app` instead of a Raylib link: since Xcode compiles a whole directory under ONE
   preprocessor-defines set, the per-TU `-Ddraw=<slug>_draw` rename can't be a project flag —
   so each cart gets a **wrapper `.c`** that `#define`s the renames then inlines the source
   (same effect, no per-file build settings). Plus the `app_main.c` shim, `app_roster.h`,
   baked data headers, and `gen/app.dims`. `project.yml`'s app target now sources the `gen/app`
   DIRECTORY (works for a single staged cart OR the multi-cart set); `ios/device.sh` /
   `build.sh` gain `APP=<manifest>` mode. The SAME `DE_NO_RAYLIB` software engine that runs
   single carts on device (ios-plan.md spike 8) runs the dispatcher. Gotchas recorded: iOS
   builds must be **serialized** (shared `gen/app` + one xcodeproj — the build-time twin of the
   `.bake` request-file race, which used `pid:` targeting); `simctl` screenshots need a few
   seconds' settle (it grabs a pre-render frame otherwise — perf.log confirms 59fps).
   **On-device: DONE (2026-07-03)** — `device.sh APP=tinyjam` runs Tinyjam on a real iPhone
   (maker-confirmed). **Touch back-to-launcher: HOLD-TO-HOME (device-confirmed 2026-07-03).**
   The shim overlays a small top-left chip in racks only (bundle-with-launcher only, so a
   standalone cart never shows it), but the HIT REGION is a big 48×20 fat-finger pad and it
   needs a ~0.3s HOLD — so it's easy to land yet quick taps on the rack's own corner controls
   never fire it (the first try, a tiny tap chip, was unhittable on-device and sat on acidrack's
   OP button). The chip fills as you hold (feedback); resets camera/clip first; cross-input
   (held finger or mouse). **The polished answer (deferred, maker-driven):** reserve a real
   nav-bar via a **`de_safe_top()` safe-area inset** the racks reflow to — but acidrack fills
   all 240px, so that's a genuine layout redesign of the dense racks, done as its own pass (the
   same inset also handles the iPhone notch/home-indicator later). iOS-as-AUv3 (a rack per
   plugin) stays [`ios-plan.md`](ios-plan.md) spike 9 (AUv3 concurrency).

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
2. ~~Launcher-as-cart~~ — **SETTLED by shipping (2026-07-03, ladder rung 3):** the menu
   is a cart, fed by each rack's `de:meta` via the generated `app_roster.h`, and the
   feared API cost never materialized — `app_launch`/`app_current` live in the
   generated shim, not the engine. What's left of this question is only the menu's
   look/feel (rung 3 sub-question b — maker's call).
3. v1 panel placement: popover off one "share" toolbar button, or a tab like settings?
