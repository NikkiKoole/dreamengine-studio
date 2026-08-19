import Foundation

// The shared store an AUv3 extension reads to learn what's unlocked. Extensions run in a separate
// sandboxed process and cannot query StoreKit, so the main app writes entitlements into the App
// Group (a UserDefaults suite) and each extension reads them on load (product-notes-followup.md §3).
//
// ── ONE GROUP FOR THE WHOLE STUDIO, not one per app ──────────────────────────────────────────
// The plan is many small apps, each free with its own one-time "Pro" unlock (ADR-0035). Either
// shape works for the job this group does today (carry one app's entitlement to its own
// extension), so the choice was made on what it does NOT foreclose:
//
//   · StoreKit is per-app. `Transaction.currentEntitlements` in Tiny Pedalboard cannot see a
//     purchase made in Tiny Acid Jam, ever. A shared App Group is the ONLY on-device way one app
//     learns what another owns.
//   · So a future studio-wide pass ("own three, get the rest"), a cross-app upgrade discount, or
//     any "you already own X" cross-promo is possible with one group and impossible with N,
//     without adding a server. One registration in the portal instead of one per app, too.
//
// It was `group.com.tinyjam`, which named the umbrella app rather than the studio and matched
// none of the shipping bundle ids (`com.mipolai.*`). Renamed while it is still free to do so:
// nothing has shipped with it, and no built target declares it yet.
//
// ⚠ SHARING THE CONTAINER MEANS TWO APPS CAN CLOBBER EACH OTHER, so the layout guards against it:
// each app writes ONLY its own key (`unlockedIDs.<its bundle id>`) and readers take the UNION. A
// flat single key would mean whichever app refreshed last erased the others.
enum AppGroup {
    static let id = "group.com.mipolai.shared"
    static var defaults: UserDefaults { UserDefaults(suiteName: id) ?? .standard }

    private static let prefix = "unlockedIDs."
    // Only the APP ever writes, so Bundle.main here is the app (an extension's bundle id is its
    // own, which is exactly why the read side takes the union instead of guessing the container's).
    private static var ownKey: String { prefix + (Bundle.main.bundleIdentifier ?? "unknown") }

    static func setUnlocked(_ ids: Set<String>) {        // main app writes, its slot only
        defaults.set(Array(ids), forKey: ownKey)
    }

    // Every app's slot, unioned. An extension reads this: it cannot know its container's bundle id
    // cheaply, and it does not need to, because isUnlocked() asks about ONE product id and only the
    // owning app could have put that id here.
    static func unlocked() -> Set<String> {
        var all = Set<String>()
        for (k, v) in defaults.dictionaryRepresentation() where k.hasPrefix(prefix) {
            if let ids = v as? [String] { all.formUnion(ids) }
        }
        return all
    }

    // THE ONE ENTITLEMENT RULE, shared with Store.swift so the app and the extension cannot drift.
    //
    // The catch-all is a product id ending `.masterpass`, and it is SCOPED BY PREFIX: Tiny Jam's
    // `com.mipolai.tinyjam.masterpass` unlocks `com.mipolai.tinyjam.acidrack` and must NOT unlock
    // `com.mipolai.tinypedalboard.pro`. Without the scope, one shared container plus a plain suffix
    // test would hand every app's Pro to anyone holding any pass — a bug the per-app group could
    // not have had, introduced by the union above, so the two changes belong together.
    // (A deliberate studio-wide pass would be a NEW rule here, not an accident of naming.)
    static func grants(_ owned: Set<String>, _ id: String) -> Bool {
        if id.isEmpty { return true }                    // nothing declared → nothing to gate
        if owned.contains(id) { return true }
        return owned.contains { pass in
            guard pass.hasSuffix(".masterpass") else { return false }
            return id.hasPrefix(String(pass.dropLast(".masterpass".count)))
        }
    }

    // The entitlement question answered from the shared container alone — what an AUv3 extension
    // uses, since it has no StoreKit and no .storekit config.
    // ⚠ Fails CLOSED on purpose: an empty or absent suite means "not entitled", never "go on".
    static func isUnlocked(_ id: String) -> Bool { grants(unlocked(), id) }

    // Diagnostic, and the ONE thing that distinguishes a working group from a convincing fake:
    // UserDefaults(suiteName:) hands back a usable suite with NO entitlement, so writes and reads
    // succeed inside one process and never cross to the extension. nil here means the entitlement
    // is missing, which is the state every target is in until the group is registered for
    // automatic provisioning (docs/design/pro-unlock.md §8).
    static var containerAvailable: Bool {
        FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: id) != nil
    }
}

// C indicator hook — how many racks the App Group reports unlocked (for the canvas dot).
@_cdecl("AppGroup_UnlockedCount")
public func AppGroup_UnlockedCount() -> Int32 { Int32(AppGroup.unlocked().count) }
