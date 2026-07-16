import SwiftUI
import UIKit

// Phase 2 — the real dreamengine (a cart) hosted on iOS via CanvasView (inside GameViewController).
// CanvasView owns the lifecycle: de_init() then AudioEngine.start() (so sound_init runs before the
// audio thread pulls de_audio_render). App just loads StoreKit entitlements.

// The app delegate's ONLY job (bridged into SwiftUI via @UIApplicationDelegateAdaptor) is to answer
// the interface-orientation query. The Info.plist lists all four orientations because iPad App Store
// validation requires the full set (project.yml) — this narrows the RUNTIME set to the active cart's
// aspect, so a landscape cart (e.g. acidcandy, 160x100) locks to landscape. GameViewController sets
// `orientationLock` from the canvas size once de_init() has run.
// See docs/design/device-adaptive-layout.md (orientation lock).
final class AppDelegate: NSObject, UIApplicationDelegate {
    static var orientationLock: UIInterfaceOrientationMask = .landscape   // default suits landscape carts; refined per-cart
    func application(_ application: UIApplication,
                     supportedInterfaceOrientationsFor window: UIWindow?) -> UIInterfaceOrientationMask {
        AppDelegate.orientationLock
    }
}

@main
struct TinyjamHelloApp: App {
    @UIApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    var body: some Scene {
        WindowGroup { ContentView() }
    }
}

struct ContentView: View {
    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            GameHost().ignoresSafeArea()       // the engine, in a UIViewController (edge-gesture + orientation control)
        }
        .onAppear {
            Store_Init()                        // spike 4 — load products + entitlements
        }
    }
}
