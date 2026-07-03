# The share panel — one surface for every sharing action, and the "app" unit

STATUS: EXPLORING (2026-07-03) — **seam-#1 spike PASSED same day** (two racks, one
binary, zero cart edits — `tools/bundle-spike/`, §spike result below). Plans the
cross-cutting row of [`sharing-channels.md`](sharing-channels.md): every sharing action
triggerable from the editor, no Xcode ever. Two design commitments and one new concept
fall out below.

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
The real fix is a manifest-generator choice: **emit these wrappers per cart**
(mechanical — the slot-taking API list is closed), or grow the engine a per-cart slot
bank behind `de_switch_cart`. Slot budget is the constraint either way: a manifest's
carts must fit 43 cart-defined slots together (acid 25 + yacht 10 = fine).

Deliberately NOT covered — the next spikes, in order of need:
1. **Sprite carts** — per-slug staged sheets + a runtime sheet-swap on switch (the
   engine already swaps the spritesheet texture in the `pal()` path, so the mechanism
   exists; it needs a seam like `de_sheet_select(slug)`).
2. **`de_state` carts** — `-Dde_state=<slug>_de_state` + per-slug slab wrappers in the
   shim (sketched, trivial, unexercised).
3. **Differing screen dims** — `SCREEN_W/H` are compile-time; a manifest picks one size
   and its carts must match (or the engine grows letterboxing). Both racks are 320×240,
   so Tinyjam dodges this.
4. **Master-bus FX bleed** — effects are set-and-hold, so the incoming cart inherits
   the outgoing cart's *master* bus config (delay time/feedback, master filter) until
   it touches a knob. Slot partitioning fixes the per-slot sounds; the shared master
   bus remains; a `de_switch_cart` API would want a bus snapshot/restore.
5. **Per-cart save dirs** and the **launcher cart** + real `de_switch_cart()` (the
   spike's shim hardcodes the pair; the manifest generates it).

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
