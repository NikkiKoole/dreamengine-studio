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

## 8. "I bought Pro on my iPhone — why is it not in Ableton Live?"

The whole point of one purchase is that it travels. It does not travel by itself. Four links, and
as of 2026-08-19 **three of them are broken**, each silently.

| # | Link | State |
|---|---|---|
| 1 | **Universal Purchase**: one App Store record covering iOS + macOS, so one non-consumable is one purchase everywhere | ❌ the Mac carrier is a DIFFERENT bundle id |
| 2 | **The App Group entitlement** actually present, on the app AND the extension, on BOTH platforms | ❌ declared by NO built target, on either platform (§9 renamed it while that is still free) |
| 3 | **The group registered** for the team so automatic provisioning can sign it | ❌ never done (that is why link 2 was removed) |
| 4 | **The host can load it**: Live has supported AUv3 since 11.3, Apple Silicon only, instruments and audio effects but NOT MIDI effects | ✅ nothing to do, but it bounds who can be sold to |

**Link 1 — Universal Purchase needs the SAME bundle id.** `ios/au-identity.sh`'s `au_carrier_load`
derives the Mac carrier as `CARRIER_APP_ID="$base.mac"`, so *Tiny Pedalboard* would ship as
`com.mipolai.tinypedalboard.mac`: **a separate App Store record and a separate purchase**, forever,
because a bundle id cannot be changed after it ships. That `.mac` suffix is right for what it was
built for (a local dev carrier that exists only to register the extension with the system) and
wrong for a store build. The two roles now have two constants, and `CARRIER_STORE_APP_ID` is the
manifest's `bundleId` unchanged.
⚠ **This also forces a distribution decision.** `tools/mac-app.sh` produces a Developer ID
notarized `.app` outside the store, and that route **cannot do Universal Purchase or App Store IAP
at all** — it would need its own licensing and its own entitlement bridge. "Buy on iPhone, works in
Live" requires the **Mac App Store**.

**Link 2 — the App Group is declared nowhere, so the entitlement cannot reach the plug-in.**
`ios/project.yml` carries a comment recording that it was deliberately removed:

> *the app-group entitlement is re-added once the group is registered for automatic provisioning.
> It blocks plain device signing until then, and the AppGroup UserDefaults suite works without it.*

That last clause is the trap. `UserDefaults(suiteName:)` returns a usable suite with no entitlement,
so everything looks fine **inside one process** — which is exactly what a simulator spike tests. It
is not a shared container, so it does not cross the app/appex boundary, which is the only thing an
App Group is for. `ios/Mac.entitlements` and `ios/MacAU.entitlements` declare only the sandbox, and
`ios/TinyjamHello.entitlements` (which does declare the group) is an **orphan referenced by no
target and no spec**. `AppGroup.containerAvailable` was written to detect precisely this and is
presumably nil everywhere; nothing calls it.

The failure this produces is at least the safe direction: `AppGroup.isUnlocked` fails **closed**, so
a Mac buyer is locked out rather than everyone being let in. But locked out is still the bug.

**What only the maker can do**: register `group.com.tinyjam` for the team in the developer portal
(and confirm the macOS naming rule — the `$(TeamIdentifierPrefix)` form is a known "works on iOS,
nil on macOS" trap), then the entitlement goes back on four targets: iOS app, iOS appex, Mac app,
Mac appex. It is deliberately NOT re-added here, because re-adding it before the portal step blocks
plain device signing and would break the working dev loop.

**Sequence, once the portal step is done**: entitlement back on all four targets → confirm
`AppGroup.containerAvailable` is non-nil on a signed build of each → Mac carrier switched to
`CARRIER_STORE_APP_ID` for the store build → Universal Purchase on the app record → buy on one
device, restore on the other, and check the plug-in in Live rather than only in GarageBand.

**One nuance on MIDI out in Live.** The known limitation is that an AU plug-in cannot route MIDI
out in Live (that needs the VST version), and AUv3 MIDI effects are unsupported there. It may not
apply to us: `runtime/midi_output.h` publishes a **CoreMIDI virtual source**, which a DAW sees as an
ordinary MIDI input device rather than as a plug-in port. Unverified, and the open question is
whether a sandboxed appex may create a virtual source at all. `tools/midi-check/` is
macOS-desktop-only, so nothing covers it.

## 9. One App Group for the whole studio (2026-08-19)

The plan is many small apps, each free with its own one-time Pro unlock. The group was
`group.com.tinyjam`, which named the **umbrella app** rather than the studio and matched none of the
shipping bundle ids (`com.mipolai.*`). It is now **`group.com.mipolai.shared`**, renamed while it
costs nothing: nothing has shipped with it and no built target declares it (§8 link 2).

**One shared group, not one per app.** Both shapes do today's job equally well, so the choice was
made on what each forecloses:

- **StoreKit is per-app.** `Transaction.currentEntitlements` in Tiny Pedalboard cannot see a
  purchase made in Tiny Acid Jam, ever. A shared App Group is the **only on-device way** one app
  learns what another owns.
- So a studio-wide pass ("own three, get the rest"), a cross-app upgrade discount, or a "you already
  own X" cross-promo stays possible with one group and needs a server with N. One portal
  registration instead of one per app, too.
- Nothing is given up: an app still only asks about **its own** product id.

**Two consequences, both handled, because sharing a container is not free:**

1. **Two apps could clobber each other.** Each app now writes only its own slot
   (`unlockedIDs.<its bundle id>`) and readers take the **union**. A single flat key meant whichever
   app refreshed last erased the others. The union is also what lets an extension read without
   knowing its container's bundle id.
2. **The catch-all had to be SCOPED, and this one is a real bug the rename would otherwise have
   introduced.** The rule was "a product id ending `.masterpass` unlocks everything". With one
   shared container that means Tiny Jam's **rack** pass would unlock **Tiny Pedalboard's Pro**, free,
   for anyone who owned it. It is now suffix **plus prefix**: a pass covers only ids beginning with
   its own prefix. Defined once in `AppGroup.grants` and used by both `Store.swift` and the
   extension path, so the app and the plug-in cannot drift. A deliberate studio-wide pass would be a
   **new rule there**, never an accident of naming.

⚠ A studio pass is NOT built and is not implied by the shared group; the group only keeps the door
open. Pricing a catalog is parked behind ADR-0035's five-app trigger.

## See also
[ADR-0035](../decisions/0035-free-with-one-pro-unlock.md) (the model) ·
[`product-notes-followup.md`](product-notes-followup.md) §3, §7 ·
[`product-notes.md`](product-notes.md) (the thesis and the §🚩 trademark rule) ·
[ADR-0026](../decisions/0026-store-pipeline-in-house-not-fastlane.md) (`asc-push.js`) ·
[`auv3-plugin-types.md`](auv3-plugin-types.md) (the `aumf` open items Pro's AUv3 leg depends on)
