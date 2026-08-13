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
        return a
    }

    public override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .black
        // A size the host uses for its initial window. The cart reflows to whatever it is given
        // (de_is_resizable), so this is a preference and not a constraint — 2x the cart's own canvas
        // keeps the pixels honest at 1:2 without demanding a huge window.
        preferredContentSize = CGSize(width: 640, height: 400)
        let c = CanvasView(frame: view.bounds, hosted: true)

        // ── connect the panel to the engine that is actually making sound ───────────────────────
        // In an out-of-process host, createAudioUnit above builds a TinyjamAU IN THIS (UI) process,
        // and blitting its framebuffer is the disconnect: a panel drawn by an engine nobody hears.
        // If a message channel is reachable, pull frames through it instead.
        //
        // ⚠ OPEN, and stated plainly because it is the one thing this is not yet known to fix:
        // `au` here is the instance THIS process created. Whether a channel taken from it reaches
        // the RENDERING instance depends on the host wiring the view controller to its own AU proxy
        // — some do, and AUv3 does not oblige them to. The nonce op exists to settle that in a real
        // host rather than by argument: if the panel's nonce differs from the one a host-side spike
        // reports, this is still two engines and the parameter-bound route (option 3) is the answer.
        if #available(macCatalyst 16.0, iOS 16.0, macOS 13.0, *), let a = au {
            let ch = a.messageChannel(for: "com.tinyjam.canvas")
            if let call = ch.callAudioUnit, !call(["op": "nonce"]).isEmpty {
                let who = call(["op": "nonce"])
                NSLog("[tinyjam] panel channel live — engine nonce %@ pid %@",
                      String(describing: who["nonce"] ?? "?"), String(describing: who["pid"] ?? "?"))
                c.remoteFrame = {
                    let r = call(["op": "frame"])
                    guard let px = r["px"] as? Data,
                          let w = r["w"] as? Int, let h = r["h"] as? Int else { return nil }
                    return (px: px, w: w, h: h)
                }
            } else {
                NSLog("[tinyjam] no panel channel — falling back to this process's own engine")
            }
        }
        // The panel must live even when the host is stopped and pulling no audio — otherwise the
        // engine never ticks, the picture freezes and the rack's own controls stop responding.
        c.onDisplayTick = { [weak self] in self?.au?.uiTick() }
        c.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        view.addSubview(c)
        canvas = c
    }
}
