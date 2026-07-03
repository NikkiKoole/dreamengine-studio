# Sharing channels — the map of every way a cart reaches someone

STATUS: EXPLORING / stock-take (2026-07-03). Consolidates the sharing/exporting thinking
that lived in six docs into one map, per the maker: *"essentially there are 2 real ways I
want to share something with the world — a little itch.io-like site on my own domain, and
a real flow for putting an app IN THE APP STORE — and the third way is just friends and
family, giving them a file."* Plus a cross-cutting wish: **every sharing action
triggerable from the editor** (making a gif from a cart's saved scripts is a sharing
action too).

This doc is the **map, not the owner** — each channel's detail stays in the doc that owns
it. The how-to for what ships today: [`../guides/exporting.md`](../guides/exporting.md).

## The three channels

| channel | audience | unit shipped | governing decision |
|---|---|---|---|
| **A · the web showcase** | the world (view + play) | wasm + clips on our site | [ADR-0020](../decisions/0020-in-house-tool-curated-showcase.md): curated showcase, not a platform |
| **B · the App Store** | paying strangers | a finished app (cart or collection), precompiled | [ADR-0023](../decisions/0023-ship-carts-as-apps-not-the-editor.md): ship carts as apps, never the editor |
| **C · friends & family** | trusted people | a file (Mac .app / Windows .exe) | — (shipped, `guides/exporting.md` #1–2) |

## Channel A — the web showcase ("itch.io-like, on my own domain")

**Ships today:** the GitHub Pages gallery (<https://nikkikoole.github.io/dreamengine/>) —
wasm builds via `build-site.js` / `publish-cart.sh` / `publish-all.js`, the editor's 🚀
button, staleness via `cart-status.js`. Detail: [`../guides/sharing.md`](../guides/sharing.md).

**Built but not wired in:** the clips pipeline. `make-gif.js` + committed input tracks
(`tools/clips/<cart>/NN-label.*`) already mint deterministic webm/gifs, and
`editor/public/clips/` holds real clips for ~10 carts — but `build-site.js` doesn't use
them: **the gallery is still a thumbnail file-listing, not the ADR-0020 "carts in motion"
showcase.** (The history page consumes clips; the gallery doesn't.)

**Missing, in rough order of leverage:**
1. **Curation mechanism** — the `status: showcase` flag sketched in
   [`cart-metadata.md`](cart-metadata.md) ("seeds the real featuring system", not yet
   built). ADR-0020 called this "the first concrete task this decision creates."
   Front page = the chosen 20–40, not all ~400.
2. **Clips in the gallery** — a card shows the clip, click to play the wasm.
   [`attract-mode.md`](attract-mode.md)'s web embed (clip → "press start" → play) is the
   polished top of this ladder.
3. **The own domain** — mechanically trivial on GH Pages (a CNAME file + DNS); the real
   work is picking the name. Undecided, undocumented until now.
4. **itch.io as an *additional* target** — never cut (ADR-0020 rejected itch as *the*
   venue, not as *a* mirror). The manual flow works today (zip the three build files,
   upload as HTML5 — `sharing.md` §itch.io); an `itch-publish` step via butler (itch's
   CLI) would make it a one-command mirror of the same wasm build. Cheap, parked until
   the own-site showcase is worth mirroring.
5. **A press kit** — a `/press/<app>/` page for journalists/streamers, generated from cart
   data (a self-hosted presskit()-style page). Specced in [`press-kit.md`](press-kit.md);
   reuses the `store-shots.js`/`make-gif.js` assets from Channel B's store work.

## Channel B — the App Store (the real product flow)

**Decided:** ADR-0023 (precompiled apps, never the editor), product strategy in
[`product-notes.md`](product-notes.md) / [`product-notes-followup.md`](product-notes-followup.md)
(pricing/master-pass/AUv3 catalog/Fastlane), Tinyjam racks as the product line
([`tinyjam-racks.md`](tinyjam-racks.md) — `acidrack` + `yachtrack` exist).

**Proven** ([`ios-plan.md`](ios-plan.md) — 8 of 9 spikes ✅): toolchain,
[`software-canvas`](software-canvas.md) render, CoreAudio, saves, StoreKit 2 (local),
App Groups, signed device deploy
(`ios/device.sh` — that's guides/exporting.md #3, the *dev* flow), AUv3 hosting the real
engine with host MIDI, and a real cart (omnichord) pixel-correct on iOS. Only CloudKit
sync (spike 6) and a real-host AUv3 check remain open.

**Missing between "proven" and "in the store":**
1. **The submission pipeline** — Apple Distribution cert + App Store Connect + TestFlight
   → review; **decided** ([ADR-0026](../decisions/0026-store-pipeline-in-house-not-fastlane.md)):
   an in-house App Store Connect API tool, not Fastlane — only its metadata folder layout
   is kept; store assets/copy derive from the cart, drift-checked. (The
   paid developer account now exists — 2026-07-03, it powers the signed Mac exports.)
2. **The palette rule** — [`palette-and-color.md`](palette-and-color.md): **no paid app
   ships on the borrowed PICO-8 palette**; an original palette before the first paid
   submission.
3. **The product decision** — *which app first?* (a rack? a game? free or paid?) — the
   un-park trigger for most of product-notes' parked items.
4. Store assets (screenshots, description, privacy labels) — unstarted, cheap once 3 is
   decided.

## Channel C — friends & family (a file)

**Shipped** (2026-07-03): signed + notarized Mac `.app` (zip, double-click, no Gatekeeper
warning, shared icon, cart-named window) and the unsigned Windows `.exe`. Both boot the
multiplayer lobby. Detail: `guides/exporting.md` #1–2, `tools/mac-app.sh`.

**Open:** Windows code-signing (paid cert, no free path — parked); nothing else blocking.

## Cross-cutting — the editor as the trigger surface

The wish: every sharing action a button, not a CLI incantation. Where that stands:

| action | editor today | CLI today |
|---|---|---|
| export Mac / Windows / iPhone / web preview | ✅ buttons | `mac-app.sh`, `build-nr.sh`, `ios/device.sh` |
| publish wasm to the site | ✅ 🚀 (settings-gated) | `publish-cart.sh`, `publish-all.js` |
| **make a gif/clip from a saved recipe** | ❌ | `make-gif.js --recipe NN-label` |
| batch publish / staleness sweep | ❌ | `publish-all.js`, `cart-status.js` |
| push to itch.io | ❌ | ❌ (manual zip+upload) |
| song-as-code sharing | in-cart (rack song codes) | [`song-codec.md`](song-codec.md), designed/parked |

The natural shape is **one "share" panel** in the editor gathering these (the existing
buttons + "🎬 make clip" over the cart's committed recipes + later itch/batch), rather
than more loose toolbar buttons. Now being planned — [`share-panel.md`](share-panel.md)
(incl. the **app manifest**: the multi-cart unit Tinyjam ships as).

## Open decisions (the ones only the maker can make)

1. The domain name (channel A #3).
2. Which app goes to the store first, and its price shape (channel B #3).
3. Whether the share panel is worth an editor-chrome pass now or after the gallery
   showcase work (cross-cutting).
