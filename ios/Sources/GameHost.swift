import UIKit

// A real UIViewController hosting the engine's CanvasView. Three things UIKit only exposes at the
// view-controller level, all needed to make a full-bleed cart play well on a phone:
//
//   • preferredScreenEdgesDeferringSystemGestures — WITHOUT this, near the top/bottom screen edges
//     iOS holds the FIRST touch to watch for its own swipe (Control Center / notifications / the
//     home-indicator swipe), so edge controls don't respond to a first tap. In landscape a cart
//     fills the height, so its top nav row sits right in that band — acidcandy's tabs "didn't
//     respond" for exactly this reason. Deferring hands the app those touches first.
//   • prefersHomeIndicatorAutoHidden — dim the home indicator over the bottom play surface.
//   • the interface-orientation lock — set from the cart's aspect (landscape cart → landscape).
//
// This is a UIViewController (not the iOS-16 SwiftUI modifiers .defersSystemGestures /
// .persistentSystemOverlays) because the app targets iOS 15. UIHostingController forwards the
// childFor… queries to a full-screen representable child, so these overrides are honoured.
// See docs/design/device-adaptive-layout.md.
final class GameViewController: UIViewController {
    // CanvasView's init runs de_init(), so de_screen_w()/de_screen_h() are valid right after.
    private let canvas = CanvasView(frame: .zero)

    override func loadView() {
        view = canvas
        // Derive the orientation policy from the cart itself:
        //   • a RESIZABLE cart (respond/rackfit/acidfit — reflows via de_resize on rotate) is meant to
        //     RESPOND, so leave it free to rotate.
        //   • a FIXED cart can't reflow, so lock it to the aspect of its canvas (landscape cart →
        //     landscape) — else it looks wrong rotated.
        // An explicit per-cart override (a manifest orientation field) would slot in here when a cart
        // needs to contradict this default — see docs/design/device-adaptive-layout.md.
        // ⚠ ASK THE VIEW'S ENGINE — do NOT create one. This used to call de_instance_create() on the
        // strength of "it is idempotent for the process's one instance", which WAS true and is not
        // any more: since the per-instance work every call allocates a real engine (and runs the
        // cart's init). That made this boot a third engine purely to ask two questions, and it is the
        // same shape as the bug that silenced the app — AudioEngine creating its own instance while
        // CanvasView drew another. `canvas` is right there and already owns the one we mean.
        let engine = canvas.engine
        if de_is_resizable(engine) != 0 {
            AppDelegate.orientationLock = .all
        } else {
            let w = Int(de_screen_w(engine)), h = Int(de_screen_h(engine))
            AppDelegate.orientationLock = (w > h) ? .landscape : (h > w ? .portrait : .all)
        }
    }

    override var preferredScreenEdgesDeferringSystemGestures: UIRectEdge { [.top, .bottom] }
    override var prefersHomeIndicatorAutoHidden: Bool { true }
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { AppDelegate.orientationLock }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        // Nudge UIKit to (re-)read the preferences above now that we're in a window, and apply the lock.
        setNeedsUpdateOfHomeIndicatorAutoHidden()
        setNeedsUpdateOfScreenEdgesDeferringSystemGestures()
        if #available(iOS 16.0, *) {
            setNeedsUpdateOfSupportedInterfaceOrientations()
            view.window?.windowScene?.requestGeometryUpdate(.iOS(interfaceOrientations: AppDelegate.orientationLock))
        }
    }
}
