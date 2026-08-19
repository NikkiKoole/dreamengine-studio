# The Pro unlock — the entitlement seam, and the awkward fact that the features do not exist yet

STATUS: BUILDING / plan (2026-08-19). **Phase 1, the entitlement seam, SHIPPED 2026-08-19 — see §6.** How [ADR-0035](../decisions/0035-free-with-one-pro-unlock.md)'s
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

## 2. Gaps in the entitlement seam (small, each well defined) — ALL FIXED, see §6

1. **The extension had no entitlement source at all, and it fails OPEN.**
   ⚠ **An earlier draft of this line said an appex would read as *locked*. That was wrong, and the
   truth is worse.** None of the three project specs (`ios/project.yml`, `project-store.yml`,
   `project-mac.yml`) listed `Store.swift` OR `AppGroup.swift` in the AU target: each one compiles
   only `Sources/CanvasView.swift` plus the cart. So a gated cart inside the plug-in resolves
   `Store_IsModuleUnlocked` to the **weak stub, which answers `true`** — the plug-in would have
   handed Pro to every host, for free, with nothing to see. `AppGroup.swift` had a `write` side and
   no `read` side, and `AppGroup_UnlockedCount` is only a diagnostic dot. This is the half
   [`product-notes-followup.md`](product-notes-followup.md) §3 warned about, arriving from the
   direction nobody was watching.
2. **The catch-all is hardcoded.** `Store.isUnlocked` ends `|| ids.contains("com.mipolai.tinyjam.masterpass")`.
   Derive the `"*"` product from the manifest instead, or a second app's pass silently does nothing.
3. **No Restore Purchases.** App Review expects an explicit restore path for a non-consumable.
   Needs `Store_Restore` over `AppStore.sync()`, plus a button somewhere a reviewer can find.
4. **No purchase UI outside the launcher.** The Tiny Jam launcher draws the offer rows. A
   single-cart app has no launcher, so `pedalboard` and `acidcandy` each need their own Pro sheet in
   cart-land, and the appex needs a *locked* state that explains itself ("open the app to unlock")
   because **an app extension cannot run a StoreKit purchase sheet.**

5. **A single-cart app generated no IAP at all.** The whole IAP block in `build-app.js` (the
   product model AND the `.storekit` generation Store.swift reads its product ids from) sat inside
   `if (launcher) { … }`, which only an umbrella has. So *Tiny Pedalboard* and *Tiny Acid Jam*
   would have shipped with `Store.configuredIDs()` finding nothing and no purchase possible.
   Found by writing the generator seam, not by reading.

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

**Phase 1 — the entitlement seam. ✅ SHIPPED 2026-08-19, see §6.**

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

## 6. Phase 1 as shipped (2026-08-19)

**[`runtime/pro.h`](../../runtime/pro.h)** — the cart-land face. A cart asks `pro_unlocked()` and
never names a product id; which product at what price arrives through a **generated `app_pro.h`**,
so the same cart is the same cart in a paid app, in the free web gallery and in the editor.
`pro_for_sale()` / `pro_can_purchase()` / `pro_buy()` / `pro_restore()` are the shopfront, and one
**shared Pro sheet** keeps three apps from drawing three paywalls (the `fxicons.h` argument).
**Stateless on purpose**: the `ProSheet` belongs to the cart, so the header needs no
`DE_CTX_STATICS` block and can never land in a saved-state slice. It also owns the ONE copy of the
weak `Store_*` bridge, which `tinyjam-menu.c` used to declare privately.

**[`ios/Sources/Entitlements.swift`](../../ios/Sources/Entitlements.swift)** — the strong answer,
in its own file because the answer has two sources and a target compiles only one: the app links
`Store.swift` (StoreKit is truth, mirrored into the App Group); an extension links neither StoreKit
nor a purchase path and reads the App Group alone. `AppGroup.isUnlocked()` **fails closed**.
`Store_CanPurchase()` is false in an extension, which is why the sheet there says *"open the app to
unlock"* rather than showing a button that does nothing. Added to the AU target in all three specs.

**Two real bugs fixed on the way**, both of which would have shipped silently:
- `Store.isUnlocked` matched the catch-all as the literal id `com.mipolai.tinyjam.masterpass`, i.e.
  Tiny Jam's own bundle prefix, so **any other app's pass unlocked nothing**. The rule is the
  `.masterpass` SUFFIX now (both readers, since an extension has no `.storekit` to consult), and
  `build-app.js` refuses a `"*"` product that is not named that way.
- The IAP block sat inside `if (launcher)`, so a single-cart app generated **no `.storekit` and no
  products at all** (§2.5). Hoisted; the roster stays launcher-only.

**Manifest → header.** The Pro product is the one declaring a non-empty `features` list; `unlocks`
stays for racks, so the feature axis and the content axis cannot collide. `build-app.js` errors on
two Pro products, on a Pro product that also lists `unlocks`, and copies `app_pro.h` into
`ios/gen/au` as well — **the AU is staged by `testflight.sh`/`device.sh`, not by `build-app.js`, so
without that copy the plug-in finds no header, reads "no store here", and fails open again.**

**[`tools/pro-check.js`](../../tools/pro-check.js)** — 18 assertions plus a 5-answer `--selfcheck`,
a gate row in `repo-doctor`. It asserts **both directions** (a strong "no" must lock AND a strong
"yes" must unlock, or a probe that always says locked scores full marks) and, the part that matters,
the **structural** half: every `AU_EXT` target in the git-tracked specs must link the entitlement
source. Four negative controls, including a mutated yml that must go red. Swift-side coverage is in
`ios/Tests/StoreTests.swift` (7 tests now, incl. the App-Group round-trip as its own control).

**Still open in Phase 3**: no cart calls `pro_unlocked()` yet, because there is nothing to gate
until WAV export exists. `Store_Restore` has no button yet. Wiring both is the shopfront step.

## 7. The rule this seam is built around

**`pro.h` fails OPEN, and that is deliberate.** A build with no store must run every feature: the
editor, `play.js`, the desktop binary and the wasm gallery all link the weak stubs and get
`unlocked`. The cost of that choice is that **forgetting to link the real answer is invisible** —
it does not crash, it does not warn, it just gives the product away. So the rule is:

> **Any target that runs a cart AND has a paywall must link `Entitlements.swift` + `AppGroup.swift`.**
> `tools/pro-check.js` is the only thing that can see whether it does. Run it after touching
> `ios/project*.yml`, `runtime/pro.h`, `Store.swift`, or `build-app.js`'s IAP block.

## See also
[ADR-0035](../decisions/0035-free-with-one-pro-unlock.md) (the model) ·
[`product-notes-followup.md`](product-notes-followup.md) §3, §7 ·
[`product-notes.md`](product-notes.md) (the thesis and the §🚩 trademark rule) ·
[ADR-0026](../decisions/0026-store-pipeline-in-house-not-fastlane.md) (`asc-push.js`) ·
[`auv3-plugin-types.md`](auv3-plugin-types.md) (the `aumf` open items Pro's AUv3 leg depends on)
