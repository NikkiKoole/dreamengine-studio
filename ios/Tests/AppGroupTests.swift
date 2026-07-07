import XCTest
@testable import TinyjamHello

// Proves the entitlement-sharing mechanism: the main app writes unlocked racks into the
// App Group suite; a separate reader (the AUv3 extension, here the test) sees them.
final class AppGroupTests: XCTestCase {
    func testAppWritesExtensionReads() {
        AppGroup.setUnlocked(["com.mipolai.tinyjam.acidrack", "com.mipolai.tinyjam.masterpass"])
        let seen = AppGroup.unlocked()
        XCTAssertTrue(seen.contains("com.mipolai.tinyjam.acidrack"))
        XCTAssertTrue(seen.contains("com.mipolai.tinyjam.masterpass"))
        XCTAssertFalse(seen.contains("com.tinyjam.funk"))
    }

    func testContainerDiagnostic() {
        // Informational only: true on a signed build with the app-group entitlement, may be
        // false on a bare simulator build. The suite path above works regardless.
        NSLog("[appgroup] shared container available = %@", "\(AppGroup.containerAvailable)")
    }
}
