import Foundation
import StoreKit
#if targetEnvironment(simulator)
import StoreKitTest   // local StoreKit testing — SIMULATOR ONLY (pulls in XCTest; absent on device)
#endif

// In-house StoreKit 2 manager. Apple verifies purchases on-device (JWS); no server.
// A nonisolated cache (unlockedIDs) is the bridge the C gate reads synchronously each
// frame; async StoreKit work refreshes it. The @_cdecl funcs are the C ABI surface.
final class Store {
    static let shared = Store()

    // C-readable snapshot of owned product IDs (gate reads this; StoreKit refreshes it).
    // The C gate (Store_IsModuleUnlocked) reads this on the MAIN thread every frame, while
    // refresh() writes it from a background Task. A Swift Set is a refcounted copy-on-write
    // heap object — a read racing the reassignment corrupts its storage refcount (nano-zone
    // heap corruption that surfaces later at a random malloc, e.g. deep in UIKit). So both
    // sides go through this lock; the reader COPIES out under it and touches the Set no more.
    static private let unlockedLock = NSLock()
    static private var _unlockedIDs: Set<String> = []
    static private func setUnlocked(_ ids: Set<String>) {
        unlockedLock.lock(); _unlockedIDs = ids; unlockedLock.unlock()
    }

    // Product ids come from the bundled Tinyjam.storekit (generated from the app manifest by
    // build-app.js) — NOT a hardcoded list, so adding a product to the manifest just works.
    private lazy var ids: [String] = Store.configuredIDs()
    private var products: [String: Product] = [:]
    private var purchasing = Set<String>()   // products with a purchase sheet already in flight
    private var updatesTask: Task<Void, Never>?
    private var promoTask: Task<Void, Never>?

    static func configuredIDs() -> [String] {
        guard let url = Bundle.main.url(forResource: "Tinyjam", withExtension: "storekit"),
              let data = try? Data(contentsOf: url),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let products = json["products"] as? [[String: Any]] else {
            NSLog("[store] no Tinyjam.storekit in bundle — no products loaded"); return []
        }
        return products.compactMap { $0["productID"] as? String }
    }

    func start() async {
        if updatesTask == nil { updatesTask = listen() }
        if #available(iOS 16.4, *), promoTask == nil { promoTask = listenPromoted() }
        await load()
        await refresh()
    }

    func load() async {
        do { for p in try await Product.products(for: ids) { products[p.id] = p } }
        catch { NSLog("[store] load failed: %@", "\(error)") }
    }

    func purchase(_ id: String) async {
        guard let p = products[id] else { NSLog("[store] no such product %@", id); return }
        if purchasing.contains(id) { return }        // a sheet for this product is already up — don't stack (fixes multi-tap while StoreKit loads)
        purchasing.insert(id)
        defer { purchasing.remove(id) }
        do {
            if case .success(let v) = try await p.purchase(), case .verified(let t) = v {
                await t.finish()
                await refresh()
            }
        } catch { NSLog("[store] purchase failed: %@", "\(error)") }
    }

    func refresh() async {
        var owned = Set<String>()
        for await r in Transaction.currentEntitlements {
            if case .verified(let t) = r { owned.insert(t.productID) }
        }
        Store.setUnlocked(owned)
        AppGroup.setUnlocked(owned)   // mirror to the App Group so AUv3 extensions can read it
    }

    static func isUnlocked(_ id: String) -> Bool {
        unlockedLock.lock()
        let ids = _unlockedIDs        // COW snapshot under the lock — the contains() below is race-free
        unlockedLock.unlock()
        return ids.contains(id) || ids.contains("com.mipolai.tinyjam.masterpass")
    }

    private func listen() -> Task<Void, Never> {
        Task.detached {
            for await r in Transaction.updates {
                if case .verified(let t) = r { await t.finish(); await Store.shared.refresh() }
            }
        }
    }

    // A PROMOTED IAP tapped on the App Store product page or in search results delivers a
    // PurchaseIntent — take the user straight into that purchase (asc-push.js --promote sets these
    // up server-side). Without this, tapping a promoted IAP just opens the app to no effect.
    @available(iOS 16.4, *)
    private func listenPromoted() -> Task<Void, Never> {
        Task.detached {
            for await intent in PurchaseIntent.intents {
                await Store.shared.purchase(intent.product.id)
            }
        }
    }
}

#if targetEnvironment(simulator)
// Local StoreKit testing. A scheme's storeKitConfiguration only applies to Xcode's Run
// button — NOT `simctl launch` / `ios-deploy` (how we deploy). Creating an in-app
// SKTestSession from the bundled Tinyjam.storekit activates the local config for ANY
// launch, so purchases work with no App Store Connect. Test purchases persist across
// relaunches; Store_ResetPurchases() wipes them (the launcher's "reset" button).
extension Store {
    static var testSession: SKTestSession?
    static func startTesting() {
        guard testSession == nil else { return }
        // iOS 26 runtime: SKTestSession abort()s (not throws) unless running in a real XCTest
        // context — even loading XCTest by hand doesn't satisfy it. Skip local testing there
        // instead of crashing. Pre-26 runtimes (≤18.x) don't have the requirement and work
        // in-app as always — that's the sim IAP dev loop (create a device on an 18.x runtime).
        if #available(iOS 26.0, *) {
            NSLog("[store] iOS 26 sim runtime — SKTestSession needs a real test run; local StoreKit testing disabled (use an 18.x runtime device for the IAP loop)")
            return
        }
        do {
            let s = try SKTestSession(configurationFileNamed: "Tinyjam")
            s.disableDialogs = true   // local testing: purchases complete instantly, no sheet to
            testSession = s           // fumble/reopen (the real Apple sheet is a device-sandbox test)
        } catch { NSLog("[store] SKTestSession failed: %@", "\(error)") }
    }
}
#endif

// ── C bridge (tinyjam_store.h) ───────────────────────────────────────────────
@_cdecl("Store_Init")
public func Store_Init() {
#if targetEnvironment(simulator)
    Store.startTesting()   // activate the local .storekit before products load
#endif
    Task { await Store.shared.start() }
}

#if targetEnvironment(simulator)   // sim-only → launcher shows the reset button only where testing is live
@_cdecl("Store_TestingAvailable")
public func Store_TestingAvailable() -> Bool { true }

@_cdecl("Store_ResetPurchases")
public func Store_ResetPurchases() {
    Store.testSession?.clearTransactions()          // wipe local test purchases → all locked
    Task { await Store.shared.refresh() }           // refresh the C-readable unlocked cache
}
#endif

@_cdecl("Store_IsModuleUnlocked")
public func Store_IsModuleUnlocked(_ moduleId: UnsafePointer<CChar>) -> Bool {
    Store.isUnlocked(String(cString: moduleId))
}

@_cdecl("Store_Purchase")
public func Store_Purchase(_ moduleId: UnsafePointer<CChar>) {
    let id = String(cString: moduleId)
    Task { await Store.shared.purchase(id) }
}
