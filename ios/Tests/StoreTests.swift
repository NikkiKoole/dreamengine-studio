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

    // The catch-all used to be the literal id "com.mipolai.tinyjam.masterpass", i.e. Tiny Jam's own
    // bundle prefix, so ANY other app's pass unlocked nothing at all and nobody would have found
    // out until a customer did. The rule is the SUFFIX now; this pins it with a foreign prefix.
    func testCatchAllIsMatchedBySuffixNotBundlePrefix() async throws {
        await cleanSlate()
        AppGroup.setUnlocked(["com.example.somethingelse.masterpass"])
        XCTAssertTrue(AppGroup.isUnlocked("com.mipolai.tinypedalboard.pro"),
                      "a pass from any bundle id must still be the catch-all")
        AppGroup.setUnlocked([])
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
