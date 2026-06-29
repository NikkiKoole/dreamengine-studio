import Foundation

// The iOS sandbox Documents dir, handed to C as a stable C string (cached once).
private var cachedDocPath: UnsafeMutablePointer<CChar>?

@_cdecl("de_documents_path")
public func de_documents_path() -> UnsafePointer<CChar>? {
    if let c = cachedDocPath { return UnsafePointer(c) }
    let dir = FileManager.default
        .urls(for: .documentDirectory, in: .userDomainMask).first?.path ?? "."
    cachedDocPath = strdup(dir)            // intentionally lives for the app's lifetime
    return UnsafePointer(cachedDocPath)
}

// Swift-side convenience over the C save API (used by tests; handy for future SwiftUI).
enum DocStore {
    @discardableResult
    static func save(_ name: String, _ bytes: [UInt8]) -> Int {
        bytes.withUnsafeBytes { Int(de_save_bytes(name, $0.baseAddress, Int32($0.count))) }
    }
    static func load(_ name: String, max: Int = 4096) -> [UInt8]? {
        var buf = [UInt8](repeating: 0, count: max)
        let n = buf.withUnsafeMutableBytes { Int(de_load_bytes(name, $0.baseAddress, Int32($0.count))) }
        return n >= 0 ? Array(buf.prefix(n)) : nil
    }
}
