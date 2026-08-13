import CoreAudioKit

// The AUv3's VIEW (phase 3 of the AU arc). Until this existed the plug-in worked but a host showed
// its own generic slider panel, which for a groovebox is the same as not working: the whole product
// IS the panel.
//
// It is also the extension's PRINCIPAL CLASS now, which is the part that is easy to get wrong. An
// audio-only AUv3 has NSExtensionPointIdentifier `com.apple.AudioUnit` and any NSObject factory; one
// with a view must declare `com.apple.AudioUnit-UI` and hand the system a class that is BOTH an
// AUViewController and the AUAudioUnitFactory. So this replaces TinyjamAUFactory rather than sitting
// beside it — two factories would leave the host free to instantiate the AU through the one with no
// view, and the panel would silently stay generic.
//
// WHAT THIS DELIBERATELY DOES NOT DO: tick the engine. The render block in TinyjamAU.swift owns the
// frame, sample-clocked at one per 735 rendered samples, because that is what keeps the sequencer on
// the host's grid and correct through an offline bounce, where no view is even on screen. So the view
// is a pure blitter over a snapshot (`CanvasView(hosted: true)` → de_copy_frame). The engine draws on
// the audio thread; we read a published copy here on main. See "THE HOST/ENGINE THREAD SPLIT" in
// studio.c, and tools/present-race-check for the gate on it.
public final class TinyjamAUViewController: AUViewController, AUAudioUnitFactory {
    private var au: TinyjamAU?
    private var canvas: CanvasView?

    // The host may call this BEFORE or AFTER the view loads, and both orders happen in the wild
    // (GarageBand differs from auval). Neither needs the other: the view shows nothing until a frame
    // is published, and the AU renders whether or not anyone is looking.
    public func createAudioUnit(with componentDescription: AudioComponentDescription) throws -> AUAudioUnit {
        let a = try TinyjamAU(componentDescription: componentDescription, options: [])
        au = a
        connectPanel()          // the view may already be up — see connectPanel()
        return a
    }

    // ── connect the panel to the engine that is actually making sound ──────────────────────────
    // Called from BOTH createAudioUnit and viewDidLoad, because the host may do those in either
    // order and neither one alone is enough. The first version probed only in viewDidLoad behind
    // `if let a = au` — and GarageBand calls createAudioUnit LAST, so `au` was nil, the block was
    // skipped, AND the "no channel" fallback log sat inside the same `if`, so it could not report
    // its own failure. Result: no line at all in a real host, which reads as "the check did not run"
    // and is indistinguishable from "the check found nothing". A diagnostic that can be silently
    // skipped is not a diagnostic. It now runs on whichever call completes the pair, once.
    private var panelConnected = false
    private func connectPanel() {
        guard !panelConnected, let a = au, let c = canvas else { return }
        panelConnected = true
        let mypid = Int(ProcessInfo.processInfo.processIdentifier)
        guard #available(macCatalyst 16.0, iOS 16.0, macOS 13.0, *) else {
            NSLog("[tinyjam] PANEL no channel — AUMessageChannel needs 16.0+ · this UI process pid %d", mypid)
            return
        }
        let ch = a.messageChannel(for: "com.tinyjam.canvas")
        guard let call = ch.callAudioUnit, !call(["op": "nonce"]).isEmpty else {
            NSLog("[tinyjam] PANEL no channel — host wired none; blitting THIS process's own engine · pid %d", mypid)
            return
        }
        let who = call(["op": "nonce"])
        let theirpid = (who["pid"] as? Int) ?? -1
        let connected = theirpid > 0 && theirpid != mypid
        NSLog("[tinyjam] PANEL %@ — channel engine pid %d nonce %@ · this UI process pid %d",
              connected ? "CONNECTED to another process (the audio one)"
                        : "TALKING TO ITSELF (same process = still the wrong engine)",
              theirpid, String(describing: who["nonce"] ?? "?"), mypid)
        if connected {
            c.remoteFrame = {
                let r = call(["op": "frame"])
                guard let px = r["px"] as? Data,
                      let w = r["w"] as? Int, let h = r["h"] as? Int else { return nil }
                return (px: px, w: w, h: h)
            }
        }
    }

    public override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .black
        // A size the host uses for its initial window. The cart reflows to whatever it is given
        // (de_is_resizable), so this is a preference and not a constraint — 2x the cart's own canvas
        // keeps the pixels honest at 1:2 without demanding a huge window.
        preferredContentSize = CGSize(width: 640, height: 400)
        let c = CanvasView(frame: view.bounds, hosted: true)

        c.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        view.addSubview(c)
        canvas = c
        connectPanel()          // the AU may already exist — see connectPanel()
    }
}
