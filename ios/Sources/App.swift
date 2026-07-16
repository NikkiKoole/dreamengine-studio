import UIKit

// Phase 2 — the real dreamengine (a cart) hosted on iOS. GameViewController owns CanvasView, which
// owns the engine lifecycle (de_init() then AudioEngine.start()).
//
// CLASSIC UIKit lifecycle (not SwiftUI's App/WindowGroup): GameViewController is the REAL root view
// controller, because that's the only way iOS reliably consults its
// preferredScreenEdgesDeferringSystemGestures / prefersHomeIndicatorAutoHidden /
// supportedInterfaceOrientations. Under SwiftUI, UIHostingController did NOT forward those to a
// representable child, so top-edge nav taps stayed dead in landscape. There's no
// UIApplicationSceneManifest in the generated Info.plist, so the legacy app-delegate window
// lifecycle applies. See docs/design/device-adaptive-layout.md.
@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
    var window: UIWindow?
    // Orientation lock, consulted by UIKit on every rotation. GameViewController sets it from the
    // cart (resizable → free; fixed → its aspect) once de_init() has run.
    static var orientationLock: UIInterfaceOrientationMask = .landscape

    func application(_ application: UIApplication,
                     didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]? = nil) -> Bool {
        Store_Init()                                   // spike 4 — load products + entitlements
        let win = UIWindow(frame: UIScreen.main.bounds)
        win.rootViewController = GameViewController()
        win.makeKeyAndVisible()
        window = win
        return true
    }

    func application(_ application: UIApplication,
                     supportedInterfaceOrientationsFor window: UIWindow?) -> UIInterfaceOrientationMask {
        AppDelegate.orientationLock
    }
}
