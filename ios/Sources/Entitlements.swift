import Foundation
#if !AU_EXT
import StoreKit   // only the APP talks to StoreKit; an appex must not link the purchase path
#endif

// THE one C-visible answer to "may this player use the paid thing?", and the ONLY file that
// defines the Store_* bridge (runtime/pro.h is the cart-land face of it). It exists as its own
// file because the answer has TWO sources and a target compiles only one of them:
//
//   · the APP links Store.swift → StoreKit 2 is the truth, and it MIRRORS every refresh into the
//     App Group so extensions can see it.
//   · an AUv3 EXTENSION links neither Store.swift nor StoreKit. An appex is a separate sandboxed
//     process, a host can load it without the container app ever launching, and it cannot show a
//     purchase sheet. Its only truth is what the app last wrote into the App Group.
//
// ⚠ WHY THIS FILE IS NOT OPTIONAL. Before it, no AU target compiled Store.swift OR AppGroup.swift
// (ios/project.yml, project-store.yml, project-mac.yml all list only CanvasView.swift), so a cart
// asking Store_IsModuleUnlocked inside the plug-in linked pro.h's WEAK STUB — which answers TRUE.
// The failure was therefore not "a buyer reads as locked": it was **Pro given away free to
// everyone, in the plug-in, silently**. A weak stub that fails OPEN cannot be the extension's
// answer, so the extension must link a real one. Add this file (and AppGroup.swift) to every
// target that runs a cart.
enum Entitlements {
    static func isUnlocked(_ id: String) -> Bool {
        if id.isEmpty { return true }        // no product declared → nothing to gate
#if AU_EXT
        return AppGroup.isUnlocked(id)       // extension: the App Group is the only truth
#else
        // App: StoreKit is authoritative. The App Group is also consulted so a build that has not
        // finished its first async refresh still answers correctly from the last known state.
        return Store.isUnlocked(id) || AppGroup.isUnlocked(id)
#endif
    }

    // Can this process actually put a purchase sheet on screen? False in an extension, which is
    // why runtime/pro.h exposes pro_can_purchase(): the plug-in draws "open the app to unlock"
    // instead of a button that silently does nothing.
    static var canPurchase: Bool {
#if AU_EXT
        return false
#else
        return true
#endif
    }
}

// ── C bridge (runtime/pro.h → tinyjam_store.h) ───────────────────────────────
@_cdecl("Store_IsModuleUnlocked")
public func Store_IsModuleUnlocked(_ moduleId: UnsafePointer<CChar>) -> Bool {
    Entitlements.isUnlocked(String(cString: moduleId))
}

@_cdecl("Store_CanPurchase")
public func Store_CanPurchase() -> Bool { Entitlements.canPurchase }

#if !AU_EXT
@_cdecl("Store_Purchase")
public func Store_Purchase(_ moduleId: UnsafePointer<CChar>) {
    let id = String(cString: moduleId)
    Task { await Store.shared.purchase(id) }
}

// App Review expects an explicit restore path for a non-consumable, and a real one is needed the
// first time somebody reinstalls. Transaction.currentEntitlements (which refresh() already reads)
// covers the ordinary case; AppStore.sync() is the one that re-authenticates and repairs a device
// whose entitlements never arrived.
@_cdecl("Store_Restore")
public func Store_Restore() {
    Task {
        do { try await AppStore.sync() } catch { NSLog("[store] restore failed: %@", "\(error)") }
        await Store.shared.refresh()
    }
}
#else
// Extension stubs: STRONG definitions, so they win over pro.h's fail-open weak ones. An appex
// cannot purchase or restore; it can only read what the app wrote.
@_cdecl("Store_Purchase")
public func Store_Purchase(_ moduleId: UnsafePointer<CChar>) {
    NSLog("[store] purchase ignored in an extension (%@) — the container app owns the sheet",
          String(cString: moduleId))
}

@_cdecl("Store_Restore")
public func Store_Restore() { NSLog("[store] restore ignored in an extension") }
#endif
