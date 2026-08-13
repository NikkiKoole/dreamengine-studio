import Foundation
import TinyjamAUKernel

// The app extension's ONLY source file. Everything that used to live in the appex — the audio unit,
// the view controller, the canvas, the whole C engine — moved into the TinyjamAUKernel framework so
// the system can load it IN-PROCESS (see AU/TinyjamAUKernel.h for why that is the point). The appex
// is now a carrier for Info-mac.plist and a holder of the link to that framework.
//
// This anchor is not ceremony. An extension target with no sources produces no executable, and an
// `import` with no symbol used can be dropped by the linker (`-dead_strip_dylibs`), which would leave
// the appex without the LC_LOAD_DYLIB that makes the framework load at all — and the failure mode is
// the nastiest kind: the extension launches fine and then `NSClassFromString` cannot find the
// principal class named in the plist. So touch a real class and keep the reference.
@objc(TinyjamAUExtensionAnchor)
final class TinyjamAUExtensionAnchor: NSObject {
    static let principal: AnyClass = TinyjamAUViewController.self
}
