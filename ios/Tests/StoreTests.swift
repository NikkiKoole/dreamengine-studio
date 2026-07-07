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
}
