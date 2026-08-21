import UIKit
import AVFoundation

// The host half of export_audio() — ADR-0035's "Pro is the paths that carry audio OUT of the app".
// The engine records the take and writes a WAV into the cart's save folder; on a phone that folder
// is inside the app container, where no human can reach it. This is the part that gets it out.
//
// TWO JOBS, and the split is deliberate. The ENGINE only ever writes WAV, because AAC needs a
// platform encoder and studio.c has no business linking one. So EXPORT_M4A arrives here as a
// REQUEST, and this file is what makes it true.
//
// ⚠ APP TARGET ONLY, on purpose. The AU targets do not compile this file, so inside an AUv3
// `de_export_ready` stays the weak no-op in studio.c: the take is written and nothing is offered.
// That is the right behaviour, not an omission — an audio-unit extension is embedded in someone
// else's window, presenting a share sheet from it is unreliable, and a person exporting from inside
// a DAW is asking the DAW to record, not us. (Same reasoning as Store_Purchase being inert there.)
enum Export {
    // EXPORT_WAV / EXPORT_M4A in studio.h. Kept as raw ints because the bridging header exposes the
    // FUNCTION, not the #defines, and inventing a Swift enum that silently disagreed with the C
    // constants is the kind of drift this repo has been bitten by.
    static let FORMAT_WAV = 0
    static let FORMAT_M4A = 1

    // ⚠ MAY BE CALLED FROM ANY THREAD, and there are TWO of them: de_frame runs on the audio
    // thread inside an AUv3, and the transcode completion below lands on an arbitrary queue. Neither
    // this function nor transcodeToM4A touches UIKit — the main-thread hop lives in share(), at the
    // one place UI actually starts, so no caller can forget it.
    static func ready(path: String, format: Int) {
        guard FileManager.default.fileExists(atPath: path) else {
            NSLog("[export] engine reported %@ but it is not on disk", path); return
        }
        let url = URL(fileURLWithPath: path)
        if format == FORMAT_M4A {
            transcodeToM4A(url) { out in share(out ?? url) }   // fall back to the WAV rather than nothing
        } else {
            share(url)
        }
    }

    // WAV → M4A. AVAssetExportSession with the AppleM4A preset is the boring, reliable path for
    // audio-only; AVAudioConverter would mean owning buffer plumbing for no gain.
    private static func transcodeToM4A(_ src: URL, done: @escaping (URL?) -> Void) {
        let out = src.deletingPathExtension().appendingPathExtension("m4a")
        try? FileManager.default.removeItem(at: out)   // an export session REFUSES to overwrite
        let asset = AVURLAsset(url: src)
        guard let s = AVAssetExportSession(asset: asset, presetName: AVAssetExportPresetAppleM4A) else {
            NSLog("[export] no M4A export session — sharing the WAV instead"); done(nil); return
        }
        s.outputURL = out
        s.outputFileType = .m4a
        s.exportAsynchronously {
            if s.status == .completed { done(out) }
            else {
                NSLog("[export] M4A transcode failed (%@) — sharing the WAV instead",
                      s.error.map { "\($0)" } ?? "unknown")
                done(nil)
            }
        }
    }

    // THE ONE PLACE THE MAIN-THREAD RULE LIVES. It is enforced here rather than at the call sites
    // because the first version hopped to main in ready() and then hopped straight back OFF it:
    // AVAssetExportSession.exportAsynchronously calls back on an ARBITRARY queue, so the sheet was
    // presented from a background thread and UIKit killed the app outright —
    // NSInternalInconsistencyException, "Modifications to the layout engine must not be performed
    // from a background thread". Not a warning, a termination, on the happy path. Guaranteeing it
    // at the UI boundary means a future caller cannot reintroduce it.
    private static func share(_ url: URL) {
        guard Thread.isMainThread else { DispatchQueue.main.async { share(url) }; return }
        guard let top = topViewController() else {
            NSLog("[export] nothing to present from — the file is at %@", url.path); return
        }
        let vc = UIActivityViewController(activityItems: [url], applicationActivities: nil)
        // ⚠ NOT OPTIONAL ON iPad OR MAC. A UIActivityViewController is a POPOVER there, and
        // presenting one with no anchor raises NSGenericException and takes the app down. The
        // centre of the presenting view is the honest anchor when the tap came from a canvas
        // widget the host knows nothing about.
        if let pop = vc.popoverPresentationController {
            pop.sourceView = top.view
            pop.sourceRect = CGRect(x: top.view.bounds.midX, y: top.view.bounds.midY, width: 1, height: 1)
            pop.permittedArrowDirections = []
        }
        top.present(vc, animated: true)
    }

    // The frontmost controller, walking presented/nav/tab chains. Reading the window off the
    // connected scenes rather than the deprecated UIApplication.windows, and taking the KEY window
    // rather than the first, or a passing system window can win.
    private static func topViewController() -> UIViewController? {
        let key = UIApplication.shared.connectedScenes
            .compactMap { $0 as? UIWindowScene }
            .flatMap { $0.windows }
            .first { $0.isKeyWindow }
        var vc = key?.rootViewController
        while true {
            if let p = vc?.presentedViewController { vc = p; continue }
            if let n = vc as? UINavigationController, let v = n.visibleViewController { vc = v; continue }
            if let t = vc as? UITabBarController, let v = t.selectedViewController { vc = v; continue }
            return vc
        }
    }
}

// ── C bridge: overrides the WEAK de_export_ready in studio.c ────────────────
@_cdecl("de_export_ready")
public func de_export_ready(_ path: UnsafePointer<CChar>, _ format: Int32) {
    Export.ready(path: String(cString: path), format: Int(format))
}
