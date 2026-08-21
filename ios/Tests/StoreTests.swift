import XCTest
import StoreKitTest
@testable import TinyjamHello

// Headless proof of the IAP model — no Apple account, no network. SKTestSession loads the
// local Tinyjam.storekit config; we buy a product and assert the entitlement flips.
// Each test resets to a clean slate INSIDE the awaited body (StoreKit-test transactions
// persist per-simulator and the Store singleton is shared across tests, so a sync setUp
// clear isn't enough — clear + refresh must be awaited before asserting).
final class StoreTests: XCTestCase {
    var session: SKTestSession!

    override func setUpWithError() throws {
        session = try SKTestSession(configurationFileNamed: "Tinyjam")
        session.disableDialogs = true     // auto-approve the purchase sheet
    }

    private func cleanSlate() async {
        await Store.shared.start()         // load products
        session.clearTransactions()        // wipe any persisted purchases
        await Store.shared.refresh()        // resync the entitlement cache to the cleared state
    }

    func testPurchaseUnlocksModule() async throws {
        await cleanSlate()
        XCTAssertFalse(Store.isUnlocked("com.mipolai.tinyjam.acidrack"), "should start locked")

        await Store.shared.purchase("com.mipolai.tinyjam.acidrack")
        XCTAssertTrue(Store.isUnlocked("com.mipolai.tinyjam.acidrack"), "should unlock after purchase")
        XCTAssertFalse(Store.isUnlocked("com.tinyjam.funk"), "other racks stay locked")
    }

    func testMasterPassUnlocksEverything() async throws {
        await cleanSlate()
        await Store.shared.purchase("com.mipolai.tinyjam.masterpass")
        XCTAssertTrue(Store.isUnlocked("com.tinyjam.funk"), "master pass unlocks all")
        XCTAssertTrue(Store.isUnlocked("com.mipolai.tinyjam.acidrack"), "master pass unlocks all")
    }

    // ── the catch-all rule, and the bug the SHARED app group would otherwise have created ──────
    // It used to be the literal id "com.mipolai.tinyjam.masterpass" (Tiny Jam's own bundle prefix),
    // so any other app's pass unlocked nothing. The rule is now "ends .masterpass, SCOPED BY
    // PREFIX", and the scope is load-bearing: every app shares one container, so a plain suffix
    // test would let Tiny Jam's rack pass unlock Tiny Pedalboard's Pro for free.
    func testMasterPassUnlocksItsOwnAppOnly() throws {
        let owned: Set<String> = ["com.mipolai.tinyjam.masterpass"]
        XCTAssertTrue(AppGroup.grants(owned, "com.mipolai.tinyjam.acidrack"),
                      "the pass covers its own app's products")
        XCTAssertFalse(AppGroup.grants(owned, "com.mipolai.tinypedalboard.pro"),
                       "…and NOT another app's Pro, or one shared container gives the studio away")
        XCTAssertTrue(AppGroup.grants(["com.example.other.masterpass"], "com.example.other.thing"),
                      "the rule is the suffix + prefix, never a hardcoded bundle id")
        XCTAssertTrue(AppGroup.grants([], ""), "an app declaring no product has nothing to gate")
        XCTAssertFalse(AppGroup.grants([], "com.mipolai.tinypedalboard.pro"), "empty must fail CLOSED")
    }

    // Two apps share one container, so each writes its OWN slot and readers take the union. A flat
    // single key would mean whichever app refreshed last erased the other's entitlement.
    func testTwoAppsInOneGroupDoNotClobberEachOther() throws {
        let d = AppGroup.defaults
        d.removeObject(forKey: "unlockedIDs.com.mipolai.tinyacidjam")
        AppGroup.setUnlocked(["com.mipolai.tinypedalboard.pro"])          // this app's slot
        d.set(["com.mipolai.tinyacidjam.pro"], forKey: "unlockedIDs.com.mipolai.tinyacidjam")
        XCTAssertTrue(AppGroup.unlocked().isSuperset(of:
            ["com.mipolai.tinypedalboard.pro", "com.mipolai.tinyacidjam.pro"]),
            "the union must hold both apps' purchases")
        AppGroup.setUnlocked([])                                          // clearing ours…
        XCTAssertTrue(AppGroup.unlocked().contains("com.mipolai.tinyacidjam.pro"),
                      "…must not clear the other app's")
        d.removeObject(forKey: "unlockedIDs.com.mipolai.tinyacidjam")
    }

    // ── the Pro (feature-axis) unlock, and the App Group an extension reads ──────────────────
    // An AUv3 runs in another process with no StoreKit, so the App Group is its only truth. These
    // assert the READ side that ios/Sources/Entitlements.swift uses under AU_EXT.
    func testAppGroupIsTheExtensionsAnswerAndFailsClosed() async throws {
        AppGroup.setUnlocked([])
        XCTAssertFalse(AppGroup.isUnlocked("com.mipolai.tinypedalboard.pro"),
                       "an empty App Group must read LOCKED — failing open here gives Pro away in every host")
        AppGroup.setUnlocked(["com.mipolai.tinypedalboard.pro"])
        XCTAssertTrue(AppGroup.isUnlocked("com.mipolai.tinypedalboard.pro"))
        XCTAssertFalse(AppGroup.isUnlocked("com.mipolai.tinyacidjam.pro"),
                       "a different app's Pro must not unlock this one")
        AppGroup.setUnlocked([])
    }

    // NEGATIVE CONTROL for the test above: if setUnlocked/unlocked did not actually round-trip
    // through the suite, both directions of it would pass on a permanently-empty set.
    func testAppGroupRoundTrips() throws {
        AppGroup.setUnlocked(["a.b.c", "d.e.f"])
        XCTAssertEqual(AppGroup.unlocked(), ["a.b.c", "d.e.f"])
        AppGroup.setUnlocked([])
        XCTAssertEqual(AppGroup.unlocked(), [])
    }

    // A purchase must MIRROR into the App Group, or the app is entitled and the plug-in is not.
    func testPurchaseMirrorsIntoTheAppGroup() async throws {
        await cleanSlate()
        AppGroup.setUnlocked([])
        await Store.shared.purchase("com.mipolai.tinyjam.acidrack")
        XCTAssertTrue(AppGroup.unlocked().contains("com.mipolai.tinyjam.acidrack"),
                      "refresh() must mirror entitlements to the App Group for the extension")
    }

    // The C bridge is what a cart actually calls (runtime/pro.h). Empty id = nothing to gate.
    func testEntitlementsBridge() async throws {
        await cleanSlate()
        XCTAssertTrue(Entitlements.isUnlocked(""), "no product declared → nothing to gate")
        XCTAssertFalse(Entitlements.isUnlocked("com.mipolai.tinyjam.acidrack"))
        await Store.shared.purchase("com.mipolai.tinyjam.acidrack")
        XCTAssertTrue(Entitlements.isUnlocked("com.mipolai.tinyjam.acidrack"))
    }
}
