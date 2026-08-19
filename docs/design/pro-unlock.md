# The Pro unlock — the entitlement seam, and the awkward fact that the features do not exist yet

STATUS: READY TO BUILD / plan (2026-08-19). How [ADR-0035](../decisions/0035-free-with-one-pro-unlock.md)'s
**free + one $4.99 Pro unlock** actually gets built into `apps/pedalboard` and `apps/tinyacidjam`.
Written by walking the code rather than the intent, which turned up the finding that reorders the
whole thing: **the StoreKit half is mostly done and the FEATURES Pro is meant to sell are mostly
missing.** Companion to [`product-notes-followup.md`](product-notes-followup.md) §3 (App Group
entitlements) and §7 (in-house StoreKit, no RevenueCat).

## 1. What already exists, and it is more than expected

`apps/tinyjam` needed per-rack IAP, so the machinery was built once and generalises for free:

| Piece | Where | State |
|---|---|---|
| StoreKit 2 manager | `ios/Sources/Store.swift` | products, purchase, `Transaction.updates`, entitlement cache behind an `NSLock` (a real heap-corruption bug, documented in the file) |
| Promoted-IAP intents | same, `listenPromoted()` | wired, iOS 16.4+, pairs with `asc-push --promote` |
| C bridge | `ios/Sources/tinyjam_store.h` | `Store_Init` / `Store_IsModuleUnlocked` / `Store_Purchase` |
| App Group mirror | `ios/Sources/AppGroup.swift` | app WRITES `unlockedIDs` into `group.com.tinyjam` |
| Local test purchases | `Store.startTesting()` (sim only) | `SKTestSession` from the bundled `.storekit` |
| Product ids | `tools/build-app.js` → `ios/gen/app/Tinyjam.storekit` | **generated from the manifest**, so the two new `.pro` products need no Swift change |
| Test | `ios/Tests/StoreTests.swift` | buy a rack → entitlement unlocks |

**Consequence: adding the Pro PRODUCT is nearly free.** It is already in both manifests
(`com.mipolai.<app>.pro`, $4.99). The work is the four gaps below plus, mostly, the features.

## 2. Gaps in the entitlement seam (small, each well defined)

1. **The extension never READS the App Group.** `AppGroup.swift` writes; nothing reads it for
   gating (`AppGroup_UnlockedCount` is a diagnostic dot). In an appex `Store.shared` never starts,
   so `Store_IsModuleUnlocked` sees an empty cache and answers **locked** for a customer who paid.
   Fix: the C gate consults `AppGroup.unlocked()` as well as the StoreKit cache. This is the half
   [`product-notes-followup.md`](product-notes-followup.md) §3 warned about, and it is the first
   thing a paying customer hits.
2. **The catch-all is hardcoded.** `Store.isUnlocked` ends `|| ids.contains("com.mipolai.tinyjam.masterpass")`.
   Derive the `"*"` product from the manifest instead, or a second app's pass silently does nothing.
3. **No Restore Purchases.** App Review expects an explicit restore path for a non-consumable.
   Needs `Store_Restore` over `AppStore.sync()`, plus a button somewhere a reviewer can find.
4. **No purchase UI outside the launcher.** The Tiny Jam launcher draws the offer rows. A
   single-cart app has no launcher, so `pedalboard` and `acidcandy` each need their own Pro sheet in
   cart-land, and the appex needs a *locked* state that explains itself ("open the app to unlock")
   because **an app extension cannot run a StoreKit purchase sheet.**

Minor: `build-app.js` writes the StoreKit config to a fixed `ios/gen/app/Tinyjam.storekit` for every
app. Regenerated per build, so harmless today, but it is a shared name in a shared path.

## 3. The finding: Pro currently sells almost nothing

ADR-0035 names three paid features. Checked against the code:

| Pro feature | Tiny Pedalboard | Tiny Acid Jam |
|---|---|---|
| **AUv3** | ❌ **not in the app.** `apps/pedalboard/app.json` sets no `auCart`, so the shipping bundle contains no extension (already the `aumf` lane's open item 3) | ✅ ships `aumu tacj Mpla` |
| **WAV export** | ❌ **no user-facing path exists anywhere in the repo** | ❌ same |
| **MIDI in** (a keyboard plugged into the device) | ❌ **not built on iOS.** `runtime/midi_input.h`'s CoreMIDI scan is `#if … && !defined(DE_NO_RAYLIB)` and iOS compiles with `DE_NO_RAYLIB=1`, so MIDI in arrives ONLY through an AUv3 host's render block (`de_midi_event`) | ❌ same |
| **MIDI out** | ◐ likely: `runtime/midi_output.h` is gated only on `__APPLE__ && !PLATFORM_WEB`, so it compiles on iOS. **Unverified on device**, and `project.yml` links no CoreMIDI explicitly (autolinking) | ◐ same |

So on *Tiny Pedalboard* the paywall would today gate **nothing at all**, and on *Tiny Acid Jam* it
would gate the AUv3 plus an unproven MIDI out. **You cannot ship a wall in front of an empty room.**

On WAV export specifically, the capture half IS built and the export half is not:
`record_arm`/`record_grab`/`sample_read` snapshot the master output, and studio.c has a real WAV
writer behind the harness trigger file `.bake/wav_request` (`tools/carts/acidrack.c` and
`yachtrack.c` drive it from a "WAV" button). That is a **debug** path: on iOS it writes into the
sandbox where the player can never reach it. The missing piece is small and specific: **a share
sheet**, plus a cart-facing API instead of a poked file.

## 4. Proposed order

**Phase 1 — the entitlement seam.** The four gaps in §2, plus one new cart-land header so a cart can
ask the question without knowing about StoreKit: `pro_unlocked()` / `pro_offer()`, weak-linked so
desktop and wasm builds compile unchanged. **Open design call: what should `pro_unlocked()` answer
off-iOS?** Recommend **true** (there is no store on desktop or in the wasm gallery, the wall is an
App Store business model rather than a product boundary, and the gallery is a funnel). Gate it the
way `tools/state-check` gates saved state: a probe plus a negative control, since "locked" and
"the seam is dead" look identical from outside.

**Phase 2 — make the features real.** Ranked by value per unit of work:
1. **WAV export**, and it is the one to do first: universally wanted, the same work for both apps,
   the capture half already exists, and it is the most legible thing a stranger can be sold.
   Needs its own design note (cart API, the iOS share sheet, the desktop file path, the length cap).
2. **AUv3 in `pedalboard`**: one manifest line (`auCart`) plus the `aumf` lane's open items.
3. **MIDI out**: verify on device. Possibly already done, in which case it is free.
4. **MIDI in on iOS**: real work (CoreMIDI source scanning inside a `DE_NO_RAYLIB` build).
   Recommend **dropping it from the Pro headline** until it exists, rather than listing a feature
   the app does not have.

**Phase 3 — the shopfront.** The Pro sheet in each cart, the appex's locked state, restore, the
review notes rewrite ([ADR-0035](../decisions/0035-free-with-one-pro-unlock.md)'s Update holds the
drafted paragraph), an IAP review screenshot per product, and only then `asc-push --iap --price`.

## 5. What NOT to do

- **Do not flip either app to Free before Phase 2 lands.** A free download with no wall, followed by
  a version that adds one, is a feature takeaway. That is why `pedalboard` is off sale and
  `tinyacidjam` is unreleased (ADR-0035's 2026-08-19 Update).
- **Do not gate anything else.** The wall is "it leaves the app". Background audio, Ableton Link and
  saving your own patterns stay free, and nothing is degraded.
- **Do not touch Tiny Jam's $4.99 master pass.** Parked behind the five-app trigger.

## See also
[ADR-0035](../decisions/0035-free-with-one-pro-unlock.md) (the model) ·
[`product-notes-followup.md`](product-notes-followup.md) §3, §7 ·
[`product-notes.md`](product-notes.md) (the thesis and the §🚩 trademark rule) ·
[ADR-0026](../decisions/0026-store-pipeline-in-house-not-fastlane.md) (`asc-push.js`) ·
[`auv3-plugin-types.md`](auv3-plugin-types.md) (the `aumf` open items Pro's AUv3 leg depends on)
