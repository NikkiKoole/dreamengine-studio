# The share panel — one surface for every sharing action, and the "app" unit

STATUS: EXPLORING (2026-07-03). Plans the cross-cutting row of
[`sharing-channels.md`](sharing-channels.md): every sharing action triggerable from the
editor, no Xcode ever. Two design commitments and one new concept fall out below.

## The two commitments

**1 · No Xcode, no GUI toolchains — ever.** Already the `ios/` ethos (xcodegen +
`build.sh` + `device.sh`; the `.xcodeproj` is generated, never opened) and today's Mac
signing proved it extends to distribution (`mac-app.sh`: codesign → notarize → staple,
all CLI). The App Store rung continues the same way: `xcodebuild -exportArchive` + the
App Store Connect API (or Fastlane — parked sketch in
[`product-notes-followup.md`](product-notes-followup.md) §7). The panel never does
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

Each is a small spike in the ios-plan spirit: riskiest-cheapest first (1+2 together —
two renamed carts in one desktop binary, switchable by key — proves the whole shape
before any iOS work).

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

**v3 — the store rung.** `ios/build.sh APP=<manifest>` + the upload tool (App Store
Connect API/Fastlane) + a "⬆ TestFlight" button in the panel. Gated on channel B's open
product decisions (which app, price, original palette —
[`sharing-channels.md`](sharing-channels.md) §Channel B).

## Open questions (maker's call)

1. Manifest home: `apps/<name>/` dir (room for icon/screenshots/store copy) vs a flat
   `tools/apps/<name>.json`?
2. Launcher-as-cart: good with the one-new-API cost (`de_switch_cart`)? It keeps the
   menu in cart-land (lo-fi, ours) instead of native chrome.
3. v1 panel placement: popover off one "share" toolbar button, or a tab like settings?
