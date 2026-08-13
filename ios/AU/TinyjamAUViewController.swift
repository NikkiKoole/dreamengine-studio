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
        // HAND THE VIEW THE AUDIO UNIT'S ENGINE. The panel must show the engine that makes the
        // sound, and a hosted CanvasView deliberately creates none of its own — a UI extension runs
        // in its own process, so creating one there boots a second engine inside the view process.
        c.engine = a.engine
        // Read the verdict NOW and again later. Not politeness: the only orphan signal available is
        // "nothing in this process has rendered", and a host whose transport is stopped looks
        // identical at the moment a panel opens. Press play and the ambiguity resolves itself — which
        // it can only do if somebody looks twice. The old single-shot line could not tell those apart
        // and did not try.
        // `canvas.remoteFrame` is deliberately left nil, i.e. the panel blits its own process's engine.
        // It used to be wired inside `if connected`, a branch that could never be taken; leaving that
        // in would read as a live cross-process pixel path when it is dead code. The frame transport
        // itself is written and measured (0.32ms for a real 160x100 frame) and stays available — what
        // is missing is any API that hands this view controller the RENDERING instance's channel.
        reportAudibility("on open")
        for delay in [2.0, 8.0, 20.0] {
            DispatchQueue.main.asyncAfter(deadline: .now() + delay) { [weak self] in
                self?.reportAudibility(String(format: "+%.0fs", delay))
            }
        }
    }

    // ── the ONE line to read in a host's Console, and it can now go red ─────────────────────────
    // Superseding a diagnostic that could not: it compared the message channel's pid against this
    // process's pid, and the channel is fetched from our OWN local audio unit, so those two were
    // equal by construction and the "connected" branch was unreachable. See the long note at
    // "WHICH INSTANCE IS THE AUDIBLE ONE" in TinyjamAU.swift — the GarageBand reading that closed
    // both bridging routes was that unreachable branch's default, not a measurement.
    private var lastVerdict = ""
    private func reportAudibility(_ when: String) {
        guard let a = au else { return }
        let r = a.audibilityReport
        let verdict: String
        if r.rendering == 0 {
            verdict = "NO AUDIO HAS RENDERED IN THIS PROCESS — either the host is stopped (press play and read the next line) or this panel is in a different process from the DSP, which is the orphan"
        } else if r.rendering == r.mine {
            verdict = "CONNECTED — this panel's own audio unit is the one being rendered"
        } else {
            verdict = "CONNECTED through the shared per-process engine — instance \(r.rendering) renders, this panel holds \(r.mine); one engine per process, so the picture is the audible one"
        }
        guard verdict != lastVerdict else { return }        // only when it CHANGES, so play/stop reads clean
        lastVerdict = verdict
        NSLog("[tinyjam] PANEL %@ · %@ · %d instance(s) in this process · pid %d",
              verdict, when, Int(r.instances),
              Int(ProcessInfo.processInfo.processIdentifier))
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
