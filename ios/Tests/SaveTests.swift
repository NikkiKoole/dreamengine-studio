import XCTest
@testable import TinyjamHello

// Headless proof of the save layer: write a blob to the Documents dir, read it back, byte-exact.
final class SaveTests: XCTestCase {
    func testSaveLoadRoundTrip() {
        let data: [UInt8] = [1, 2, 3, 4, 5, 42, 99, 0, 255]
        let written = DocStore.save("test_blob.bin", data)
        XCTAssertEqual(written, data.count, "should write all bytes")

        let read = DocStore.load("test_blob.bin")
        XCTAssertEqual(read, data, "round-trip must be byte-exact")
    }

    func testLoadMissingReturnsNil() {
        XCTAssertNil(DocStore.load("does_not_exist_\(UUID().uuidString).bin"))
    }
}
