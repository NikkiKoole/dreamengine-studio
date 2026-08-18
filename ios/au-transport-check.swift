// au-transport-check.swift — the headless gate for AUv3 HOST TRANSPORT (phase 2 of the AU arc),
// and for the SAMPLE RATE the host renders us at (the hazard ios-plan.md recorded but never exercised).
//
// WHY THIS EXISTS: everything about host sync was verified by a person pressing buttons in
// GarageBand. That worked, but it means touching runtime/sync.h or the render block again has no
// safety net short of asking the owner to go press buttons. This is that net, and it needs no DAW
// and no device: it IS a host. auval cannot cover it — auval never SETS musicalContextBlock, so it
// only ever exercises the "host supplies no transport" path.
//
//   swiftc -O -o au-transport-check au-transport-check.swift -framework AVFoundation -framework CoreAudioKit
//   ./au-transport-check            # the transport checks at 44.1k; exits 0 = PASS
//   ./au-transport-check --free     # NEGATIVE CONTROL: no transport blocks, the tempo check must fail
//   ./au-transport-check --rate 48000   # the same transport checks, host rendering at 48k
//   ./au-transport-check --wav /tmp/x   # also dump what it rendered → /tmp/x-<rate>.wav, to LISTEN
//   ./au-transport-check --loadable   # CAN A DAW LOAD OUR CODE AT ALL? (read its note first)
//   ./au-transport-check --view      # is the UI extension wired? (the picture still needs eyes)
//   ./au-transport-check --panel     # is the panel attached to the unit that RENDERS? (see --panel below)
//   ./au-transport-check --realtime  # pace to the wall clock: exercises the FRAME WORKER path
//
// Requires the plug-in registered (zsh ios/mac.sh). Run from ios/; mac.sh runs it for you.
//
// WHAT IT ASSERTS, and each maps to something a user actually reported or relied on:
//   1. with the host transport MOVING, the rack plays                  (it made sound in GarageBand)
//   2. at double tempo it fires roughly twice as many notes            ("the tempo follows")
//   3. with the host STOPPED, it goes quiet                            ("when i press stop it doesnt
//                                                                        stop" — the bug this fixed)
// The fake host advances its beat position from the RENDERED SAMPLE COUNT, not a wall clock, so the
// whole run is deterministic and independent of machine speed. --rate runs all three at another host
// rate, which guards a property worth guarding: the sequencer is RATE-IMMUNE, because the step comes
// from sync_beats() and a host states its playhead absolutely.
//
// ── why there is no pitch check in this file, and where the pitch oracle actually lives ───────────
// There WAS one, and it was wrong in a way worth keeping written down, because the wrongness is the
// generic trap: A BROKEN ANALYSER AND THE REAL DEFECT PRINT THE SAME NUMBER.
//
// It rendered the same 8 bars at 44.1k and at 48k and compared the pitch of the low band, estimated
// from per-window zero crossings. At ~50 Hz a 2048-sample window holds only 4 or 5 crossings, so the
// reading was quantized to a coarse grid — and the grid's POSITIONS are (crossings / 2 / window
// seconds), so they move with the sample rate. 5 crossings in a 46.4 ms window reads 53.83 Hz; 5 in a
// 42.7 ms window reads 58.59 Hz. Their ratio is 48000/44100 EXACTLY. So the tool reported "+147 cents
// sharp at 48k, exactly the predicted 1200·log2(48000/44100)" — a number that looked like a
// confirmed diagnosis and was in fact the sample-rate ratio wearing a costume. ANY signal would have
// produced it. The check even carried an A/A null, which passed at 0 cents: a same-rate null cannot
// see a rate-dependent estimator, so it certified nothing.
//
// What caught it was the control that should have existed first: resample a 44.1k render to 48k with a
// KNOWN-GOOD converter (afconvert -f WAVE -d LEI16@48000) and re-measure. Same music, same pitch, and
// the estimator moved 53.83 → 46.88. That is a measurement of the rate, not of the instrument.
//
// So the pitch oracle is ios/rate-convert-check.swift instead: a 220 Hz SINE through the real
// AU/RateConvert.swift, where the answer is 220.000 Hz at every rate and there is nothing to argue
// with. The lesson generalises past this file — a statistic over a musical mix from a cart with
// per-step probability and noise drums is not a reference signal, and picking the reference signal is
// most of the work in an oracle.

import AVFoundation
import AppKit
// CoreAudioKit, not AudioToolbox: requestViewControllerWithCompletionHandler is declared there, as a
// CATEGORY on AUAudioUnit (CoreAudioKit/AUViewController.h). Without this import the method simply does
// not exist and the compiler says AUAudioUnit "has no member" — found by grepping the SDK headers
// rather than guessing an import per build, the same technique that settled the CoreAudio one.
import CoreAudioKit

// ── the component we built (must match the derived spec's AudioComponents entry) ──
//
// ⚠ FROM THE ENVIRONMENT, because the identity is DERIVED PER APP (ios/au-identity.sh) and these
// two codes were hardcoded to Tiny Acid Jam's. That is not merely a stale default: with two of our
// plug-ins registered on one machine, every gate below would instantiate `tacj`/`Mpla` and PASS
// while the plug-in you had just built and were reasoning about was never loaded at all. A gate
// that reports on a different binary than the one under test is worse than no gate. mac.sh exports
// these; the defaults keep a bare `./au-transport-check` working as documented.
func fourCC(_ s: String) -> OSType { s.utf8.reduce(0) { ($0 << 8) | OSType($1) } }
func envCode(_ key: String, _ fallback: String) -> String {
    guard let v = ProcessInfo.processInfo.environment[key], v.count == 4 else { return fallback }
    return v
}
// The CARRIER is per-app too (its bundle id and product name are derived from the manifest, so two
// of our plug-ins can be registered at once — see ios/au-identity.sh). Both of these used to be
// hardcoded to the one shared `TinyjamMac` carrier, which is the same failure mode as the component
// codes above: the gate would inspect whichever carrier happened to be installed rather than the one
// just built, and pass or fail on a bundle nobody was asking about.
func envStr(_ key: String, _ fallback: String) -> String {
    guard let v = ProcessInfo.processInfo.environment[key], !v.isEmpty else { return fallback }
    return v
}
let AU_SUBTYPE = envCode("AU_SUBTYPE", "tacj")
let AU_MANUF   = envCode("AU_MANUF",   "Mpla")
// The TYPE is part of the triple too, so a plug-in declared `aumf` is simply not found by a search
// for `aumu` — the gate would report "is it registered?" about a plug-in sitting right there.
let AU_TYPE    = envCode("AU_TYPE",    "aumu")
let desc = AudioComponentDescription(componentType: fourCC(AU_TYPE),
                                     componentSubType: fourCC(AU_SUBTYPE),
                                     componentManufacturer: fourCC(AU_MANUF),
                                     componentFlags: 0, componentFlagsMask: 0)

let ENGINE_SR = 44100.0   // what the engine is COMPILED for; the baseline every rate is compared to

// ── args ──
let argv = CommandLine.arguments
func flagValue(_ flag: String, _ fallback: Double) -> Double {
    guard let i = argv.firstIndex(of: flag), i + 1 < argv.count, let v = Double(argv[i + 1]) else { return fallback }
    return v
}
let freeRun  = argv.contains("--free")
// The plug-in runs the cart's frame on a WORKER thread in realtime, and INLINE when the host declares
// it is rendering OFFLINE (a bounce), where exactness beats latency. AVAudioEngine's manual-rendering
// mode sets isRenderingOffline for us, so the default run here takes the inline path and stays
// deterministic. `--realtime` paces the loop to the wall clock instead, which leaves the WORKER path
// engaged — the arrangement a live DAW actually uses, and worth running after touching either.
let realtime = argv.contains("--realtime")
let hostRate = flagValue("--rate", ENGINE_SR)
// --wav <prefix> dumps what was rendered → <prefix>-<rate>.wav. Not a check, a listening aid: the
// fastest way to answer "does 48k sound in tune" is still to play the file.
let wavPrefix: String? = {
    guard let i = argv.firstIndex(of: "--wav"), i + 1 < argv.count else { return nil }
    return argv[i + 1]
}()

// ═══ --loadable: CAN A HOST LOAD OUR CODE AT ALL? ════════════════════════════════════════════════
// The gate that was missing on 2026-08-13, when SIX green gates coexisted with a plug-in GarageBand
// refused to open (orange !). The AU code had been factored into a framework and `AudioComponentBundle`
// pointed at it — correct per Apple's samples — but the framework is a Mac Catalyst binary and
// GarageBand is a native macOS process, so the host's dlopen failed:
//
//   dlopen(…TinyjamAUKernel): incompatible platform (have 'MacCatalyst', need 'macOS')
//
// NOTHING ELSE HERE CAN SEE THAT, and the reason is worth stating: every other mode in this file
// instantiates through AVAudioUnit, which SILENTLY FALLS BACK to out-of-process when in-process
// loading fails. So the plug-in kept working for us and died in the DAW. A DAW does not fall back.
//
// So this mode does not instantiate anything. It reads what the extension DECLARES and checks the
// declaration is honest: if `AudioComponentBundle` names a bundle other than the appex itself, that
// bundle must be dlopen-able by a native macOS process, because that is precisely what a host does.
//
// It runs BEFORE the instantiation below on purpose — the broken case is the one where instantiation
// is the thing that fails, and a gate that needs a working plug-in to report a broken plug-in is no
// gate at all.
if argv.contains("--loadable") {
    var bad = 0
    func t(_ name: String, _ ok: Bool, _ detail: String) {
        print("  \(ok ? "✓" : "✗") \(name)  — \(detail)"); if !ok { bad += 1 }
    }
    let appPath = { () -> String in
        if let i = argv.firstIndex(of: "--app"), i + 1 < argv.count { return argv[i + 1] }
        return (envStr("AU_CARRIER_APP", "~/Applications/TinyjamMac.app") as NSString).expandingTildeInPath
    }()
    print("▸ can a native macOS host load our code? (\(appPath))")
    let plugIns = appPath + "/Contents/PlugIns"
    let appexes = (try? FileManager.default.contentsOfDirectory(atPath: plugIns))?.filter { $0.hasSuffix(".appex") } ?? []
    t("the app carries an app-extension", !appexes.isEmpty,
      appexes.isEmpty ? "no .appex in \(plugIns) — run: zsh ios/mac.sh" : appexes.joined(separator: ", "))

    for ax in appexes {
        let axPath = plugIns + "/" + ax
        guard let info = NSDictionary(contentsOfFile: axPath + "/Contents/Info.plist") else {
            t("\(ax): Info.plist is readable", false, "could not read it"); continue
        }
        let ownID = info["CFBundleIdentifier"] as? String ?? "?"
        let attrs = (info["NSExtension"] as? NSDictionary)?["NSExtensionAttributes"] as? NSDictionary
        let declared = attrs?["AudioComponentBundle"] as? String
        t("\(ax) declares AudioComponentBundle", declared != nil,
          declared ?? "MISSING — a host has no way to find the AU's code")

        guard let want = declared else { continue }
        if want == ownID {
            // Today's shape. The code is the appex, which the system launches as its own process; no
            // host ever dlopens it, so there is nothing here that can fail the way the framework did.
            t("\(ax): the AU code is the appex itself", true,
              "\(want) — hosts load it OUT of process; no dlopen for a host to get wrong")
            continue
        }
        // A SEPARATE bundle is named. This is the shape that broke: it only works if a native host can
        // actually load it. Search where the system would — the appex's Frameworks, then the app's.
        var found: String? = nil
        for dir in [axPath + "/Contents/Frameworks", appPath + "/Contents/Frameworks"] {
            for e in (try? FileManager.default.contentsOfDirectory(atPath: dir)) ?? [] {
                let p = dir + "/" + e
                let ids = [p + "/Resources/Info.plist", p + "/Info.plist"]        // versioned, then shallow
                for ip in ids where NSDictionary(contentsOfFile: ip)?["CFBundleIdentifier"] as? String == want {
                    found = p
                }
            }
        }
        t("\(ax): the named bundle \(want) is present", found != nil,
          found ?? "not found in the appex's or the app's Frameworks/")
        guard let fw = found, let b = Bundle(path: fw), let exe = b.executableURL?.path else { continue }
        let h = dlopen(exe, RTLD_NOW | RTLD_LOCAL)
        t("\(ax): and a NATIVE macOS process can dlopen it — what a host does", h != nil,
          h != nil ? "loaded \((exe as NSString).lastPathComponent)"
                   : "dlopen FAILED: \(String(cString: dlerror()))")
        if h != nil { dlclose(h) }
    }

    // ── THE CONTROL, and this gate needs one badly: every assertion above passes VACUOUSLY in the
    // shape we ship today (nothing separate is declared, so nothing is dlopened). Without a case that
    // must fail, a green run here would be indistinguishable from a check that has gone blind.
    // Catalyst code is the thing a native host cannot load, and the appex's own executable IS Catalyst,
    // so loading it must fail. (It fails on /System/iOSSupport rather than the platform triple —
    // different message, same wall.)
    //
    // ⚠ The dlopen assertion above was ALSO exercised red, against a hand-broken copy of the app that
    // declared a Catalyst framework as its code bundle — and it failed on "code signature invalid"
    // rather than the platform, because copying a binary out of a signed bundle breaks its signature.
    // Both are legitimate reds (a DAW refuses either), and it is worth knowing this gate catches the
    // signature class too — a nested code bundle whose signature does not survive packaging is an
    // ordinary shipping bug. What has NOT been demonstrated red is the platform message specifically;
    // the real GarageBand failure of 2026-08-13 is the record for that.
    print("▸ CONTROL: Catalyst code must NOT load into this native process")
    if let ax = appexes.first {
        let exe = plugIns + "/" + ax + "/Contents/MacOS/" + (ax as NSString).deletingPathExtension
        let h = dlopen(exe, RTLD_NOW | RTLD_LOCAL)
        t("dlopen of the Catalyst appex binary fails, so the check above can go red", h == nil,
          h == nil ? String(cString: dlerror()).components(separatedBy: ": ").suffix(2).joined(separator: ": ")
                   : "IT LOADED — this probe cannot detect an unloadable bundle, treat every ✓ above as unproven")
        if h != nil { dlclose(h) }
    }

    print(bad == 0 ? "\nPASS — nothing is declared that a host cannot load."
                   : "\n\(bad) check(s) FAILED — a DAW will refuse this plug-in even if every other gate is green.")
    exit(bad == 0 ? 0 : 1)
}

// WIPE THE PLUG-IN'S SAVED CART STATE FIRST. acidcandy persists its banks with save_bytes, and an
// app-extension has its own container, so without this every run boots from whatever the LAST run
// left — the onset counts drift between runs and a gate that drifts is not a gate. It is also how a
// silent state got stuck to the plug-in during the maker's play-test: worth knowing the path.
let blob = ("~/Library/Containers/\(envStr("AU_APPEX_ID", "com.tinyjam.mac.AU"))/Data/cart.blob" as NSString).expandingTildeInPath
try? FileManager.default.removeItem(atPath: blob)

// ── instantiate the plug-in (out of process, as a real host does) ──
// PUMP THE MAIN RUN LOOP instead of blocking on a semaphore. This used to be a semaphore wait, and it
// worked right up until the extension gained a VIEW: a `com.apple.AudioUnit-UI` extension does part of
// its loading on the main queue, so blocking the main thread here deadlocks and instantiation "fails"
// after the timeout with a message blaming registration — while auval, which has a real run loop,
// loads the same plug-in happily. That misdirection cost a confused minute; worth the comment.
var au: AVAudioUnit?
var instantiated = false
AVAudioUnit.instantiate(with: desc, options: []) { u, _ in au = u; instantiated = true }
let deadline = Date().addingTimeInterval(20)
while !instantiated, Date() < deadline {
    RunLoop.main.run(mode: .default, before: Date().addingTimeInterval(0.05))
}
guard let avAU = au else {
    print("✗ could not instantiate \(AU_TYPE)/\(AU_SUBTYPE)/\(AU_MANUF) — is it registered? run: zsh ios/mac.sh")
    exit(1)
}


// ── THE FAKE HOST. These two blocks are the entire point: they are what a DAW supplies and what
//    auval never does. `hostBeat` is advanced by the render loop from samples rendered. ──
var hostTempo = 90.0
var hostBeat  = 0.0
var hostMoving = true
var hostSR = ENGINE_SR        // the rate the CURRENT rig renders at; the blocks report positions in it

// NEGATIVE CONTROL: `--free` skips installing the blocks, so the plug-in gets no transport and
// free-runs on its own clock. The tempo check must then FAIL — a cart on its own clock fires notes in
// proportion to TIME, so the same 8 host beats at double tempo (half the wall time) yield about HALF
// the notes, i.e. a ratio near 0.5. Run it that way once after touching this file: a gate that cannot
// fail is decoration, and every other checker in this repo carries the same kind of known answer.
if freeRun { print("  (--free: installing NO transport blocks; the tempo check SHOULD fail)") }

if !freeRun { avAU.auAudioUnit.musicalContextBlock = { tempo, tsNum, tsDen, beat, offset, downbeat in
    tempo?.pointee = hostTempo
    tsNum?.pointee = 4; tsDen?.pointee = 4
    beat?.pointee = hostBeat
    offset?.pointee = 0
    downbeat?.pointee = (hostBeat / 4.0).rounded(.down) * 4.0
    return true
} }
if !freeRun { avAU.auAudioUnit.transportStateBlock = { flags, samplePos, cycleStart, cycleEnd in
    flags?.pointee = hostMoving ? [.moving] : []
    samplePos?.pointee = hostBeat / hostTempo * 60.0 * hostSR
    cycleStart?.pointee = 0; cycleEnd?.pointee = 0
    return true
} }

// ── one offline render rig at one sample rate ────────────────────────────────────────────────────
// A rig owns an AVAudioEngine and takes the AU INSTANCE, so a run can tear one rig down and build
// another at a different rate around the same plug-in, the way a host switching device rate does.
final class Rig {
    let sr: Double
    private let engine = AVAudioEngine()
    private let au: AVAudioUnit
    private let buf: AVAudioPCMBuffer
    // onset detector state (per rig: a fresh rate means a fresh lookback ring)
    private let look: Int
    private var envRing: [Float]
    private var ringIdx = 0
    private var env: Float = 0
    private var sinceOnset = 99999

    init(au: AVAudioUnit, sr: Double) throws {
        self.sr = sr; self.au = au
        let fmt = AVAudioFormat(standardFormatWithSampleRate: sr, channels: 2)!
        look = Int(0.006 * sr)
        envRing = [Float](repeating: 0, count: look)
        engine.attach(au)
        engine.connect(au, to: engine.mainMixerNode, format: fmt)
        try engine.enableManualRenderingMode(.offline, format: fmt, maximumFrameCount: 4096)
        try engine.start()
        buf = AVAudioPCMBuffer(pcmFormat: engine.manualRenderingFormat, frameCapacity: 4096)!
        hostSR = sr
    }

    // What the PLUG-IN's own output bus ended up at, which is the fact --rate exists to establish:
    // if AVAudioEngine had quietly kept the AU at 44.1k and converted downstream, a run at "48k"
    // would be exercising AVFoundation's converter and not ours. Assert, never assume.
    var auRate: Double { au.auAudioUnit.outputBusses[0].format.sampleRate }

    // Count note ONSETS with an ADAPTIVE threshold, and count them over a span of BEATS rather than
    // seconds. Both choices were forced by a first version that failed honestly:
    //   · a fixed "re-arm once the signal drops below 0.01" rule counted 6 onsets in 4s where there
    //     were ~24, because the reverb and delay tails never drop that low — and it got WORSE at higher
    //     tempo (denser notes, fewer gaps), which reads exactly like "the tempo isn't followed" when
    //     nothing of the sort is happening. A detector whose sensitivity depends on the thing under
    //     test is worthless.
    //   · comparing equal TIME at two tempos also compares different musical spans. Equal BEATS is the
    //     invariant that actually means "it followed": the same bars should fire the same notes at any
    //     tempo, so the two counts should MATCH rather than differ by the tempo ratio.
    // An ENVELOPE RISE, compared against the envelope a fixed 6ms earlier. Both properties matter:
    // smoothing |x| throws away waveform detail (a 303 saw's flyback is a huge per-sample delta and would
    // swamp any first-difference detector — the same trap click-check.js documents), and a FIXED lookback
    // makes the test rate-independent, which the previous two attempts were not.
    // Set to dump what this rig rendered, at its OWN rate, for any tool (or ear) outside this file.
    // This is how the bogus pitch estimator was caught: dump, convert with afconvert, re-measure.
    var wav: AVAudioFile?

    func render(beats: Double) -> (peak: Float, onsets: Int) {
        var peak: Float = 0, onsets = 0
        var hpPrev: Float = 0                               // one-pole high-pass state (see below)
        let until = hostBeat + beats
        while hostBeat < until {
            guard (try? engine.renderOffline(2048, to: buf)) == .success else { break }
            let n = Int(buf.frameLength)
            // --realtime: hand the wall clock back, so the frame worker gets to run between buffers
            // exactly as it would under a live host. Without this the CPU renders minutes of audio a
            // second and no worker on earth keeps up.
            if realtime { usleep(useconds_t(Double(n) / sr * 1_000_000.0)) }
            if let w = wav { try? w.write(from: buf) }
            if let ch = buf.floatChannelData {
                for i in 0..<n {
                    let raw = ch[0][i]
                    let a = abs(raw)
                    peak = max(peak, a)
                    // ⚠ THE ENVELOPE RUNS ON A HIGH-PASSED SIGNAL, and the reason is the same failure
                    // this detector was already rebuilt for once (see above), returning by another
                    // door. The rise is measured RELATIVE to the envelope 6ms ago, so anything
                    // SUSTAINED raises the floor that a transient has to clear. Drums alone have no
                    // sustain and it worked; the day acidcandy started booting with an audible legato
                    // 303, the bassline lifted the floor, masked drum onsets, and did it MORE at 180
                    // BPM where the line is denser — dropping the ratio 0.87 → 0.69 and reading as
                    // "the sequencer ignores the host tempo" on a sequencer measured at exactly 4.00
                    // steps/beat at BOTH tempos. A one-pole difference keeps percussive transients
                    // and throws away the sustained tone, so the detector stops depending on which
                    // voices happen to be unmuted.
                    let hp = abs(raw - hpPrev); hpPrev = raw
                    env += (hp - env) * 0.02                // ~3ms envelope follower, on the transients
                    let then = envRing[ringIdx]             // the envelope 6ms ago
                    envRing[ringIdx] = env; ringIdx = (ringIdx + 1) % look
                    sinceOnset += 1
                    // ⚠ DO NOT LOWER 0.02 TO "COUNT MORE ONSETS". It was tried, and it made the measurement WORSE in
                    // the most instructive way: at 0.004 the count went 16 → 58 at 90 BPM but only 16 → 30 at
                    // 180, so the ratio collapsed to 0.52 — the exact signature of a sequencer ignoring the
                    // host. Nothing had changed about the sequencer. Eight beats at 180 BPM is HALF THE
                    // SECONDS of eight beats at 90, so the extra events the lower floor admitted were
                    // proportional to WALL TIME, not to beats: reverb and delay tails, which is precisely what
                    // this detector was rebuilt once already to stop counting. A strict floor counting few
                    // real events beats a loose one counting many tails; 16 is EXACT and reproducible run to
                    // run, so its 2x margin over the liveness floor below is solid rather than lucky.
                    if env > then * 1.8 + 0.02 && sinceOnset > Int(0.02 * sr) { onsets += 1; sinceOnset = 0 }
                }
            }
            hostBeat += Double(n) / sr * (hostTempo / 60.0)   // the host's playhead advances with audio
        }
        return (peak, onsets)
    }

    func teardown() { engine.stop(); engine.detach(au) }   // detach: an AU node lives in one engine
}

// ── check plumbing ──
var failures = 0
func check(_ name: String, _ ok: Bool, _ detail: String) {
    print("  \(ok ? "✓" : "✗") \(name)  — \(detail)")
    if !ok { failures += 1 }
}
func cents(_ ratio: Double) -> Double { 1200.0 * log2(ratio) }

// ═══ --view: can a host actually GET our plug-in view? ═══════════════════════════════════════════
// The narrow question this can answer headlessly: is the UI extension wired up — `com.apple.AudioUnit-UI`
// as the extension point, an AUViewController that is also the AUAudioUnitFactory as the principal
// class, and the view actually loading when asked. Get any of that wrong and the plug-in still passes
// auval and still plays; the host just quietly shows its own generic sliders forever, which is the
// failure this mode exists to catch early.
// What it CANNOT answer is whether the picture looks right — that needs eyes in a DAW. The pixel path
// itself (de_copy_frame → the blit) is gated separately by tools/present-race-check.
if argv.contains("--view") {
    print("▸ plug-in view: can a host obtain it?")
    var vc: NSViewController?
    var answered = false
    avAU.auAudioUnit.requestViewController { controller in vc = controller; answered = true }
    let vdl = Date().addingTimeInterval(15)
    while !answered, Date() < vdl { RunLoop.main.run(mode: .default, before: Date().addingTimeInterval(0.05)) }
    check("the host is handed a view controller", answered && vc != nil,
          answered ? "got \(vc.map { String(describing: type(of: $0)) } ?? "nil — the host would fall back to generic sliders")"
                   : "requestViewController never answered within 15s")
    if let v = vc {
        // Reading `view` forces the controller to load, which is where our CanvasView is built and its
        // display link starts. If that fails or the view is empty, a DAW gets a blank panel.
        let loaded = v.view.frame
        let size = v.preferredContentSize
        check("its view loads and asks for a size", size.width > 0 && size.height > 0,
              "view frame \(Int(loaded.width))x\(Int(loaded.height)), preferredContentSize \(Int(size.width))x\(Int(size.height))")
    }
    print(failures == 0 ? "\nPASS — the UI extension is wired; LOOK at it in a DAW to judge the picture."
                        : "\n\(failures) check(s) FAILED")
    exit(failures == 0 ? 0 : 1)
}

// ═══ --panel: is the PANEL looking at the engine you can HEAR? ═══════════════════════════════════
// The gate that should have existed before anyone concluded the panel was orphaned. --view above
// proves a host can OBTAIN the view; this proves the view is attached to the RENDERING audio unit,
// which is a different question and the one the whole "out-of-process wall" fork turned on.
//
// It cannot ask the extension directly — there is no API for "which instance renders" across the
// boundary, and inventing one was the trap: the previous diagnostic asked the view controller's OWN
// local audio unit for its pid and compared it against the view controller's own pid, two numbers
// that are equal by construction. So instead the extension stamps the renderer's identity in its own
// process (TinyjamAU.audibilityReport) and NSLogs a verdict, and this reads that verdict back out of
// the unified log — the same line a person reads in Console, now asserted.
//
// THE CONTROL IS BUILT IN, and it is why the check wants BOTH lines: the panel opens before the host
// renders anything, so it must first report the orphan-shaped verdict and only then flip to
// CONNECTED. A run that shows only one of the two is a reporter stuck on one branch, which is exactly
// the failure being corrected here.
// ═══ --state: SESSION STATE — does fullState carry the rack OUT to the host? ══════════════════════
// The engine half of session state is gated by tools/state-check/run.sh (20 assertions, four negative
// controls) — but that runs the desktop DE_NO_RAYLIB build, so it never sees the twelve Swift lines in
// TinyjamAU's fullState, and it cannot see the thing those lines have to survive: a host does not keep
// the dictionary in memory, it WRITES IT INTO THE PROJECT FILE as a property list. Anything in there
// that is not plist-representable is dropped SILENTLY. So a plug-in can pass every in-memory test and
// still forget everything the first time a real project is saved and reopened.
//
// This mode covers exactly that seam: the key reaches the host, it is our own format, super's keys are
// not stripped, it survives plist serialisation byte-for-byte, and setting it back — valid or corrupt —
// leaves a plug-in that still plays.
if argv.contains("--state") {
    print("▸ session state: does fullState carry the rack, and survive what a DAW does with it?")
    // SAME REASON AS --panel AND THE TRANSPORT GATE, which already skip here: this rig gives an
    // effect no input, so it renders silence BY CONSTRUCTION. Every assertion below is downstream
    // of a booted rack, so on an effect they went red together (6 of them) while saying nothing
    // about session state at all. That is worse than no coverage: it is 6 false alarms sitting in
    // front of whoever is diagnosing a REAL silence, and it trains you to ignore this gate.
    // ⚠ WHAT THIS SKIP COSTS, said out loud so it is not mistaken for coverage: an EFFECT's
    // fullState is now gated ONLY by tools/state-check/run.sh, which is the engine half and cannot
    // see the plist round trip. The proper fix is the same ~10 lines named at --panel (give Rig an
    // input source for an effect), and it fixes all three gates at once.
    if AU_TYPE == "aumf" || AU_TYPE == "aufx" {
        print("  SKIPPED — \(AU_TYPE) is an EFFECT and this rig gives it no input, so it never renders.")
        print("  Every check here needs a booted rack, so they would fail together and mean nothing.")
        print("  ⚠ An effect's session state is therefore covered ONLY by tools/state-check/run.sh")
        print("    (the engine half) — the plist round trip is UNGATED for this type. See --panel.")
        exit(0)
    }
    let KEY = "dreamengineRack"

    // Frames have to have RUN first: a cart's saved slices register on first access, so reading
    // fullState straight after instantiation would measure a rack that has not booted its state yet.
    let rig = try! Rig(au: avAU, sr: ENGINE_SR)
    let before = rig.render(beats: 4)
    check("the plug-in is rendering before we ask it to save", before.peak > 0.01,
          "peak \(String(format: "%.3f", before.peak)) — a silent AU makes every check below vacuous")

    guard let state = avAU.auAudioUnit.fullState else {
        check("fullState returns a dictionary", false, "it returned nil")
        print("\n\(failures) check(s) FAILED — the host cannot save this plug-in at all.")
        exit(1)
    }
    let data = state[KEY] as? Data
    check("fullState carries our rack blob", (data?.count ?? 0) > 0,
          data == nil ? "no \"\(KEY)\" key — the engine half never reached the host"
                      : "\(data!.count) bytes under \"\(KEY)\"")

    // It must be OUR container, not an empty placeholder that happens to be non-nil.
    let magic: String? = (data?.count ?? 0) >= 4 ? String(bytes: data!.prefix(4), encoding: .ascii) : nil
    check("and it is our compressed container", magic == "DEZ1",
          "magic = \(magic ?? "none") (the AU packs the engine's DES1 blob as DEZ1)")

    // The whole reason compression is here: the engine's blob is ~589 KB and ~99.5% zero bytes.
    // Assert the SAVING, not just the format — a container that compressed nothing would still
    // carry the right magic and quietly put half a megabyte per track in the project file.
    var rawLen = 0
    if let d = data, d.count >= 8 {
        var n: UInt32 = 0
        _ = withUnsafeMutableBytes(of: &n) { d.subdata(in: 4..<8).copyBytes(to: $0) }
        rawLen = Int(UInt32(littleEndian: n))
    }
    let stored = data?.count ?? 0
    check("and it actually compressed", rawLen > 0 && stored * 10 < rawLen,
          "\(stored) bytes stored for a \(rawLen)-byte rack" +
          (rawLen > 0 ? " (\(String(format: "%.0f", Double(rawLen) / Double(max(stored, 1))))× smaller)" : ""))

    // and what is inside must be the engine's own format
    let inner: Data? = {
        guard let d = data, d.count > 8, magic == "DEZ1" else { return nil }
        return try? (Data(d.dropFirst(8)) as NSData).decompressed(using: .zlib) as Data
    }()
    check("the packed payload inflates to the engine's DES1 blob",
          inner != nil && inner!.count == rawLen && String(bytes: inner!.prefix(4), encoding: .ascii) == "DES1",
          inner == nil ? "could not inflate it" : "\(inner!.count) bytes, magic \(String(bytes: inner!.prefix(4), encoding: .ascii) ?? "?")")

    // super's keys are what let the host re-instantiate us at all. Returning only ours would strip them.
    check("super's fullState keys are preserved", state.count > 1,
          "\(state.count) key(s): \(state.keys.map { "\($0)" }.sorted().joined(separator: ", "))")

    // ⚠ THE ONE THAT MATTERS MOST — see the block comment above.
    var plistOK = false, sameAfterPlist = false
    do {
        let ser = try PropertyListSerialization.data(fromPropertyList: state, format: .binary, options: 0)
        let back = try PropertyListSerialization.propertyList(from: ser, options: [], format: nil) as? [String: Any]
        plistOK = back != nil
        sameAfterPlist = (back?[KEY] as? Data) == data
    } catch { plistOK = false }
    check("the whole dictionary survives a property-list round trip", plistOK,
          plistOK ? "serialised to binary plist and read back"
                  : "NOT plist-representable — a DAW would drop this silently on project save")
    check("and our blob comes back byte-identical", sameAfterPlist,
          sameAfterPlist ? "\(data?.count ?? 0) bytes unchanged" : "the blob changed or vanished in the plist")

    // Setting it back must be accepted and must leave a plug-in that still plays.
    avAU.auAudioUnit.fullState = state
    let after = rig.render(beats: 4)
    check("the plug-in still renders after a restore", after.peak > 0.01,
          "peak \(String(format: "%.3f", after.peak)) over 4 beats")

    // ROUND-TRIP EQUALITY, which is what upgrades the check above from "did not crash" to "kept the
    // rack". Save again and compare the ENGINE-level bytes: a restore that had silently fallen back to
    // defaults, or a container that mangled/truncated/reordered anything, would differ wholesale.
    // ⚠ Not asserted as EXACT. A few live counters (per-voice trigger levels, the rolling autosave)
    // move every frame and a frame has to run for the restore to apply at all, so a handful of bytes
    // legitimately differ. The discriminating question is 99% vs 50%, not 100% vs 99.9%.
    if let before = inner,
       let again = (avAU.auAudioUnit.fullState?[KEY] as? Data).flatMap({ d -> Data? in
           guard d.count > 8, d.prefix(4).elementsEqual(Data("DEZ1".utf8)) else { return d }
           return try? (Data(d.dropFirst(8)) as NSData).decompressed(using: .zlib) as Data
       }) {
        var differing = 0
        if again.count == before.count {
            before.withUnsafeBytes { a in again.withUnsafeBytes { b in
                let pa = a.bindMemory(to: UInt8.self), pb = b.bindMemory(to: UInt8.self)
                for i in 0..<pa.count where pa[i] != pb[i] { differing += 1 }
            }}
        }
        let pct = before.count > 0 ? 100.0 * Double(differing) / Double(before.count) : 100.0
        check("re-saving the restored rack reproduces it",
              again.count == before.count && pct < 1.0,
              again.count == before.count
                ? "\(differing) of \(before.count) bytes differ (\(String(format: "%.3f", pct))%) — live counters only"
                : "length changed: \(before.count) → \(again.count)")
    }

    // BACKWARDS COMPATIBILITY: projects saved before compression landed hold a RAW DES1 blob, and the
    // maker has some. The setter must recognise those by their magic and pass them straight through.
    // ⚠ What this proves is that a legacy blob is not mistaken for a compressed one and does not wedge
    // the plug-in — not that the engine accepted it, which needs an observable the host does not have.
    if let raw = inner {
        var legacy = state
        legacy[KEY] = raw
        avAU.auAudioUnit.fullState = legacy
        let afterLegacy = rig.render(beats: 4)
        check("a LEGACY uncompressed DES1 blob still loads", afterLegacy.peak > 0.01,
              "peak \(String(format: "%.3f", afterLegacy.peak)) — old projects keep working")
    }

    // TWO NEGATIVE CONTROLS, because compression added a SECOND way to reject a blob and they fail in
    // different layers. Neither may wedge the plug-in: if "refused" silenced the rack then refusing
    // would not be the safe outcome the engine advertises, and the checks above would prove little.
    //
    // (a) the CONTAINER is damaged → the AU's own unpack rejects it before the engine sees anything.
    var badZip = state
    if var d = data, d.count > 12 { d[12] ^= 0xFF; badZip[KEY] = d }   // into the deflate stream
    avAU.auAudioUnit.fullState = badZip
    let afterBadZip = rig.render(beats: 4)
    check("a damaged CONTAINER is rejected WITHOUT wedging the plug-in", afterBadZip.peak > 0.01,
          "peak \(String(format: "%.3f", afterBadZip.peak)) after mangling the deflate stream")

    // (b) the container is fine but the ENGINE's layout fingerprint disagrees (a different build).
    // Corrupt the INNER blob and repack, so the rejection has to happen in de_load_state.
    if var raw = inner, raw.count > 8 {
        raw[raw.startIndex + 8] ^= 0xFF                                // the fingerprint word
        var badInner = state
        badInner[KEY] = raw                                            // raw path: the engine judges it
        avAU.auAudioUnit.fullState = badInner
        let afterBadInner = rig.render(beats: 4)
        check("a blob from another BUILD is refused WITHOUT wedging the plug-in", afterBadInner.peak > 0.01,
              "peak \(String(format: "%.3f", afterBadInner.peak)) after mangling the layout fingerprint")
    }

    print(failures == 0 ? "\nPASS — fullState reaches the host, survives a project save, and refuses safely."
                        : "\n\(failures) check(s) FAILED — a saved project would not restore this rack.")
    exit(failures == 0 ? 0 : 1)
}

// ═══ --wheel: DOES A HOST CONTINUOUS CONTROL REACH THE RACK? ══════════════════════════════════════
// GarageBand draws a mod wheel and a pitch bend above its keyboard, on macOS and iPadOS, and until
// 2026-08-14 neither did anything: the AU's event switch handled only 0x90/0x80/0xE0, so NO CC reached
// the engine at all — which also means no DAW automation lane did. This gate covers the wired path:
// CC1 → the master DJ filter, applied to the whole rack.
//
// It asserts a CHANGE and a RECOVERY, with a no-op control first, because "the audio differs" is
// worthless if two untouched renders of a self-running rack already differ by as much.
if argv.contains("--wheel") {
    print("▸ mod wheel: does CC1 reach the rack and move the master filter?")
    // Effects render silence in this rig (see --panel), and this gate's ENTIRE subject is whether
    // the mix changes. Worse than useless on an effect: its no-op CONTROL compares two silences,
    // which agree perfectly, so the control PASSED at 0.000 vs 0.000 while the two real checks
    // failed. A control that a dead rig satisfies is not a control.
    if AU_TYPE == "aumf" || AU_TYPE == "aufx" {
        print("  SKIPPED — \(AU_TYPE) is an EFFECT and this rig gives it no input, so it never renders.")
        print("  ⚠ CC1 is therefore UNGATED for this type. Fix is the input source named at --panel.")
        exit(0)
    }
    let rig = try! Rig(au: avAU, sr: ENGINE_SR)
    let sched = avAU.auAudioUnit.scheduleMIDIEventBlock
    check("the AU exposes a MIDI input path", sched != nil,
          sched == nil ? "no scheduleMIDIEventBlock — nothing below can work" : "scheduleMIDIEventBlock present")

    func send(_ bytes: [UInt8]) { sched?(AUEventSampleTimeImmediate, 0, bytes.count, bytes) }

    _ = rig.render(beats: 4)                       // settle
    let a = rig.render(beats: 8)
    let b = rig.render(beats: 8)                   // NO-OP CONTROL: nothing sent between these two
    let noopOn = abs(Double(a.onsets - b.onsets))
    let noopPk = abs(Double(a.peak - b.peak))
    check("two untouched renders are comparable", noopOn <= Double(max(a.onsets, 4)) * 0.35,
          "onsets \(a.onsets) vs \(b.onsets), peak \(String(format: "%.3f", a.peak)) vs \(String(format: "%.3f", b.peak)) — the floor any real change has to clear")

    send([0xB0, 1, 127])                           // mod wheel fully up = filter fully closed
    let closed = rig.render(beats: 8)
    // MEASURED, and not what I first assumed: PEAK is the discriminator (0.711 → 0.249, a 2.9× drop as
    // the filter shuts), while ONSETS barely move (123 → 134) and can even rise — a lowpass reshapes
    // the envelopes the detector triggers on rather than removing events. The `||` keeps the check
    // honest for either outcome; the comment is here so nobody "fixes" it back to an onsets-only test.
    check("CC1 up CLOSES the master filter",
          Double(closed.onsets) < Double(a.onsets) * 0.6 || closed.peak < a.peak * 0.6,
          "onsets \(a.onsets) → \(closed.onsets), peak \(String(format: "%.3f", a.peak)) → \(String(format: "%.3f", closed.peak))")

    send([0xB0, 1, 0])                             // …and back to rest
    let reopened = rig.render(beats: 8)
    check("letting it back to 0 hands the knob back",
          Double(reopened.onsets) > Double(closed.onsets) * 1.4 || reopened.peak > closed.peak * 1.4,
          "onsets \(closed.onsets) → \(reopened.onsets), peak \(String(format: "%.3f", closed.peak)) → \(String(format: "%.3f", reopened.peak))")

    // A bend is NOT asserted here, and the reason is worth stating rather than leaving as a silent
    // gap: BOTH 303 LINES ARE MUTED AT BOOT by design ("bring it in on record"), so a default render
    // is drums only and a bend on the acid lines has nothing to move. Verifying it needs the lines
    // unmuted plus a PITCH oracle, which this file deliberately does not have (see its header).
    print(failures == 0 ? "\nPASS — a host continuous control reaches the rack and moves the whole mix."
                        : "\n\(failures) check(s) FAILED — the host's mod wheel does not reach the rack.")
    exit(failures == 0 ? 0 : 1)
}

// ═══ --params: DOES THE HOST SEE, DRIVE AND READ BACK THE RACK'S KNOBS? ═══════════════════════════
// Until 2026-08-15 this unit exposed NO parameterTree at all, so a host saw zero parameters: nothing
// automatable, nothing recordable, an empty lane menu. (It is also why the mod wheel above had to be
// hand-mapped to the master filter — a workaround for having none.) The tree is now built from what
// the CART declared via param_bind, so this gate is the only place that proves the whole chain
// end to end through a REAL out-of-process plug-in: cart → engine table → seam → AUParameterTree.
if argv.contains("--params") {
    print("▸ parameters: can a DAW see, ride and read back the rack's knobs?")
    let au = avAU.auAudioUnit
    let tree = au.parameterTree
    check("the AU exposes a parameter tree", tree != nil,
          tree == nil ? "parameterTree is nil — a host sees no parameters at all" : "parameterTree present")
    let all = tree?.allParameters ?? []
    check("and it has parameters in it", all.count > 0, "\(all.count) parameter(s)")
    // NAMED, not just counted: an empty-named or zero-address tree would satisfy a count and be
    // useless in a host's menu, which is exactly how this would fail quietly.
    let named = all.filter { !$0.displayName.isEmpty && $0.address != 0 }
    check("every parameter has a name and a non-zero address", named.count == all.count,
          "\(named.count)/\(all.count) usable — e.g. " + all.prefix(4).map { "\($0.address):\($0.displayName)" }.joined(separator: " "))

    // addr 1 = the master DJ filter (P_M_FLT in acidcandy). Same target the mod wheel rides, chosen
    // for the same reason: it is the one control whose effect is unmissable in a whole-rack mix.
    guard let flt = tree?.parameter(withAddress: 1) else {
        check("parameter 1 (the master filter) is in the tree", false, "not found — the rest cannot run")
        print("\n\(failures) check(s) FAILED"); exit(1)
    }
    // The tree checks above are type-agnostic and have run. Everything below asks whether a write
    // MOVES THE MIX, which an effect fed no input cannot answer (see --panel). Stopping here keeps
    // the half that means something instead of failing the whole gate.
    if AU_TYPE == "aumf" || AU_TYPE == "aufx" {
        print("  ⋯ the render half SKIPPED — \(AU_TYPE) is an EFFECT and this rig gives it no input.")
        print("  ⚠ So the tree is gated for this type and REACHING THE DSP is not.")
        print("\n\(failures) check(s) FAILED"); if failures == 0 { print("PASS (tree only)") }
        exit(failures == 0 ? 0 : 1)
    }
    let rig = try! Rig(au: avAU, sr: ENGINE_SR)
    _ = rig.render(beats: 4)                          // settle
    let a = rig.render(beats: 8)
    let b = rig.render(beats: 8)                      // NO-OP CONTROL, as in --wheel
    check("two untouched renders are comparable",
          abs(Double(a.onsets - b.onsets)) <= Double(max(a.onsets, 4)) * 0.35,
          "onsets \(a.onsets) vs \(b.onsets), peak \(String(format: "%.3f", a.peak)) vs \(String(format: "%.3f", b.peak)) — the floor")

    let before = flt.value
    flt.value = 0.02                                  // a host WRITE — closes the filter
    let closed = rig.render(beats: 8)
    check("a host write moves the mix",
          Double(closed.onsets) < Double(a.onsets) * 0.6 || closed.peak < a.peak * 0.6,
          "FLT \(before) → 0.02 · onsets \(a.onsets) → \(closed.onsets), peak \(String(format: "%.3f", a.peak)) → \(String(format: "%.3f", closed.peak))")
    // READ BACK through implementorValueProvider, which is what a host uses to draw its own generic
    // view and to know where an automation lane starts. A tree that only WRITES looks fine until a
    // host reopens the project and shows every knob at its default.
    // READ-BACK. Not a nicety: a host reads a parameter straight back after writing it, IN THE SAME
    // CALL, to populate the cache that every later read is served from. While de_param_set only
    // QUEUED, that read-back landed before the frame drain and honestly reported the OLD slot — so a
    // host cached the pre-write value and showed it forever, and a reopened project displayed every
    // knob where it used to be. Fixed by de_param_set recording its intent (param_ctx.h → `want`).
    check("and the host reads the new value back", abs(flt.value - 0.02) < 0.001,
          "provider returned \(flt.value)")
    // …and a parameter NOBODY wrote must still read its LIVE value, so the fix cannot degenerate into
    // "echo back whatever was last set". addr 10 = 303a CUT, untouched, cart default 0.55.
    check("…while an untouched parameter still reads its real value",
          abs((tree?.parameter(withAddress: 10)?.value ?? -1) - 0.55) < 0.001,
          "addr 10 = \(tree?.parameter(withAddress: 10)?.value ?? -1)")

    flt.value = before                                // hand it back
    let reopened = rig.render(beats: 8)
    check("restoring the value restores the mix",
          Double(reopened.onsets) > Double(closed.onsets) * 1.4 || reopened.peak > closed.peak * 1.4,
          "onsets \(closed.onsets) → \(reopened.onsets), peak \(String(format: "%.3f", closed.peak)) → \(String(format: "%.3f", reopened.peak))")

    print(failures == 0 ? "\nPASS — the rack's knobs are visible, automatable and readable from a host."
                        : "\n\(failures) check(s) FAILED — a DAW cannot automate this rack.")
    exit(failures == 0 ? 0 : 1)
}

if argv.contains("--panel") {
    print("▸ panel: is it attached to the audio unit that RENDERS?")
    // Same limitation as the transport gate, for the same reason, and it has to be said out loud
    // here too: this check waits for the panel to report CONNECTED, which the extension only does
    // once it observes audio RENDERING. The rig feeds an effect's input nothing, so an effect never
    // renders and never reports — "never reported CONNECTED" then reads as a broken panel when the
    // truth is that nothing asked it to draw a frame.
    // ▶ THE PROPER FIX, deliberately not taken yet: give Rig an input SOURCE when the unit is an
    // effect (attach an AVAudioSourceNode and connect it INTO the AU), which would make this gate and
    // the transport one work for both types. It is ~10 lines, and the reason to be careful is that the
    // instrument path must stay byte-identical — a gate that currently passes must not be blunted to
    // make a new case work. Until then, refusing is honest and passing-by-silence is not.
    if AU_TYPE == "aumf" || AU_TYPE == "aufx" {
        print("  SKIPPED — \(AU_TYPE) is an EFFECT and this rig gives it no input, so it never renders.")
        print("  --view still applies (and covers that a host can obtain the view at all).")
        print("  Judge the picture in a DAW; `auval -v \(AU_TYPE) \(AU_SUBTYPE) \(AU_MANUF)` covers the audio.")
        exit(0)
    }
    // WHICH PROCESS is the audio in? The channel answers from wherever the AU's code actually lives.
    var enginePid = -1
    if #available(macOS 13.0, macCatalyst 16.0, *) {
        let ch = avAU.auAudioUnit.messageChannel(for: "com.tinyjam.canvas")
        if let call = ch.callAudioUnit { enginePid = (call(["op": "nonce"])["pid"] as? Int) ?? -1 }
    }
    check("the host can locate the process the audio unit lives in", enginePid > 0,
          enginePid > 0 ? "audio unit code is in pid \(enginePid)" : "no message channel — cannot tell")

    var vc: NSViewController?
    var answered = false
    avAU.auAudioUnit.requestViewController { c in vc = c; answered = true }
    let pdl = Date().addingTimeInterval(15)
    while !answered, Date() < pdl { RunLoop.main.run(mode: .default, before: Date().addingTimeInterval(0.05)) }
    if let v = vc { _ = v.view.frame }          // forces the real view controller to load and report

    // Render for real, so ONE instance stamps itself as the audible one. Without this every verdict
    // stays at "nothing has rendered", which is indistinguishable from the orphan — deliberately, and
    // it is the reason this mode renders instead of just opening a view.
    let prig = try! Rig(au: avAU, sr: ENGINE_SR)
    hostMoving = true; hostTempo = 120
    _ = prig.render(beats: 4.0)
    prig.teardown()
    // Give the extension's own +2s/+8s re-reads time to fire, then let the log daemon settle.
    let wait = Date().addingTimeInterval(10)
    while Date() < wait { RunLoop.main.run(mode: .default, before: Date().addingTimeInterval(0.1)) }

    func panelLines() -> [String] {
        let p = Process()
        // /usr/bin/log by absolute path: `log` is a zsh BUILTIN, and calling it by name from a shell
        // gets "too many arguments" on stderr and nothing on stdout — which reads as "the extension
        // logged nothing" and cost a wrong conclusion twice in one afternoon.
        p.executableURL = URL(fileURLWithPath: "/usr/bin/log")
        p.arguments = ["show", "--last", "120s", "--style", "compact",
                       "--predicate", "eventMessage CONTAINS \"[tinyjam] PANEL\""]
        let out = Pipe(); p.standardOutput = out; p.standardError = Pipe()
        do { try p.run() } catch { return [] }
        let d = out.fileHandleForReading.readDataToEndOfFile()
        p.waitUntilExit()
        return (String(data: d, encoding: .utf8) ?? "")
            .split(separator: "\n").map(String.init)
            .filter { $0.contains("pid \(enginePid)") }     // THIS run's audio process, not an old one
    }
    let lines = panelLines()
    let connected = lines.filter { $0.contains("PANEL CONNECTED") }
    let notYet   = lines.filter { $0.contains("NO AUDIO HAS RENDERED") }

    check("the panel reports from the same process as the audio unit", !lines.isEmpty,
          lines.isEmpty ? "no [tinyjam] PANEL line mentioning pid \(enginePid) — the view loaded somewhere else, or never loaded"
                        : "\(lines.count) verdict line(s) from pid \(enginePid)")
    check("its verdict is CONNECTED once audio is rendering", !connected.isEmpty,
          connected.last?.components(separatedBy: "PANEL ").last ?? "never reported CONNECTED")
    check("and it reported the OTHER verdict first (so the check can go red)", !notYet.isEmpty,
          notYet.isEmpty ? "only ever printed one verdict — the reporter may be stuck on a branch"
                         : "saw the pre-render verdict too, \(notYet.count) time(s)")

    print(failures == 0 ? "\nPASS — the panel is attached to the audio unit that renders."
                        : "\n\(failures) check(s) FAILED — the panel may be driving an engine nobody hears.")
    exit(failures == 0 ? 0 : 1)
}

// ═══ the three transport checks ══════════════════════════════════════════════════════════════════
//
// ⚠ INSTRUMENT-ONLY, AND IT HAS TO SAY SO. This rig attaches the AU and connects it to the main
// mixer (Rig.init) and feeds its INPUT nothing, which is the whole truth for an instrument and
// useless for an EFFECT: AVAudioEngine will not run an effect with no source, so every check below
// reads 0 onsets / peak 0.000. That is not a defect in the plug-in — it is this gate being the wrong
// host — but five red lines are indistinguishable from a broken rack, and worse, one of them ("STOPS
// when the host stops") goes GREEN because silence satisfies it. A gate that fails for a reason
// unrelated to what it names is worse than no gate, so refuse the question rather than answer it
// wrongly. `auval` is the right tool here and already knows how: for an effect it feeds real input at
// several block sizes and sample rates. Verified on `aumf tpdl Mpla` — AU VALIDATION SUCCEEDED.
if AU_TYPE == "aumf" || AU_TYPE == "aufx" {
    print("▸ host-transport gate: SKIPPED — \(AU_TYPE) is an EFFECT, and this rig hosts INSTRUMENTS.")
    print("  It connects the AU to the mixer and feeds its input nothing, so an effect renders silence")
    print("  and every check below would fail for a reason that has nothing to do with the plug-in.")
    print("  Use Apple's validator, which feeds an effect real input:")
    print("      auval -v \(AU_TYPE) \(AU_SUBTYPE) \(AU_MANUF)")
    print("  The type-agnostic gates DO apply to an effect and are worth running: --loadable --view")
    print("  --panel --state --wheel --params.")
    exit(0)
}
if hostRate != ENGINE_SR { print("▸ host rendering at \(Int(hostRate)) Hz (engine is compiled for \(Int(ENGINE_SR)))") }
let rig = try! Rig(au: avAU, sr: hostRate)
if let p = wavPrefix {
    let url = URL(fileURLWithPath: "\(p)-\(Int(hostRate)).wav")
    try? FileManager.default.removeItem(at: url)
    rig.wav = try? AVAudioFile(forWriting: url,
                               settings: [AVFormatIDKey: kAudioFormatLinearPCM,
                                          AVSampleRateKey: hostRate,
                                          AVNumberOfChannelsKey: 2,
                                          AVLinearPCMBitDepthKey: 16,
                                          AVLinearPCMIsFloatKey: false])
    print("▸ dumping → \(url.path)")
}
if hostRate != ENGINE_SR {
    check("the host really moved the plug-in's own bus to \(Int(hostRate)) Hz",
          abs(rig.auRate - hostRate) < 1.0, "AU output bus reports \(Int(rig.auRate)) Hz")
}

_ = rig.render(beats: 1.0)                     // let the rack settle / the first notes land
hostTempo = 90;  let atSlow = rig.render(beats: 8.0)
hostTempo = 180; let atFast = rig.render(beats: 8.0)

check("plays while the host transport is MOVING", atSlow.onsets >= 8,
      "\(atSlow.onsets) onsets over 8 beats at 90 BPM, peak \(String(format: "%.3f", atSlow.peak))")

// The real assertion: the SAME 8 beats fire the same notes at either tempo, i.e. the sequencer is
// on the HOST's grid. If it ran on its own clock instead, doubling the host tempo would leave the
// note count flat in TIME and so halve it per beat.
let ratio = atSlow.onsets > 0 ? Double(atFast.onsets) / Double(atSlow.onsets) : 0
check("the same 8 beats fire the same notes at 2x tempo", ratio > 0.7 && ratio < 1.4,
      "\(atFast.onsets) onsets at 180 vs \(atSlow.onsets) at 90 → ratio \(String(format: "%.2f", ratio))")

hostMoving = false
_ = rig.render(beats: 3.0)                     // release + reverb/delay tails decay
let stopped = rig.render(beats: 6.0)

// ── 4. IT COMES BACK. ───────────────────────────────────────────────────────────────────────────
// Added after the maker's play-test wedged TWICE at bar 33 — which is where a host reaches the end of
// a 32-bar section and STOPS. Stopping was already covered; STARTING AGAIN was not, and "it paused
// and neither GarageBand's play button nor the rack's own would bring it back" is a far worse bug
// than never stopping. A stop is a state you must be able to leave.
check("STOPS when the host stops", freeRun ? stopped.onsets > 0 : (stopped.onsets == 0 && stopped.peak < 0.05),
      "\(stopped.onsets) onsets, peak \(String(format: "%.4f", stopped.peak)) after a 3-beat settle"
      + (freeRun ? "  (--free: inverted — it SHOULD keep playing)" : ""))

hostMoving = true                              // the host presses PLAY again
let restarted = rig.render(beats: 8.0)
check("STARTS AGAIN when the host restarts", restarted.onsets >= 8,
      "\(restarted.onsets) onsets over 8 beats after the host resumed, peak \(String(format: "%.3f", restarted.peak))")

// ── 5+6: THE PLAYHEAD JUMPS. ────────────────────────────────────────────────────────────────────
// Both added chasing a wedge the maker hit at bar 33.1 — twice, and again after the first fix. 33.1
// is beat 128, which is where a 32-bar CYCLE region ends: the host does not merely stop there, it
// JUMPS THE PLAYHEAD BACKWARDS. Every check above moves the playhead forwards only, so a cart that
// mishandles a backward jump would pass all of them and still die in the first bar of real use.
//   5. LOOP: jump back mid-flight, the way a cycle does, and keep playing.
//   6. REWIND then RESTART: the other half — stop, return to the top, press play.
hostBeat = 128.0                                // where the maker's wedge happened
hostMoving = true
_ = rig.render(beats: 2.0)
hostBeat = 0.0                                  // the cycle wraps: 128 beats BACKWARDS, still playing
let looped = rig.render(beats: 8.0)
check("keeps playing when the host LOOPS the playhead backwards", looped.onsets >= 8,
      "\(looped.onsets) onsets over the 8 beats after a 128-beat jump back, peak \(String(format: "%.3f", looped.peak))")

hostMoving = false
_ = rig.render(beats: 4.0)                      // stop and let the tails die
hostBeat = 0.0                                  // rewind to the top, as pressing stop-then-play does
hostMoving = true
let afterRewind = rig.render(beats: 8.0)
check("plays after a STOP + REWIND + play", afterRewind.onsets >= 8,
      "\(afterRewind.onsets) onsets over 8 beats from the top, peak \(String(format: "%.3f", afterRewind.peak))")

rig.teardown()
print(failures == 0 ? "\nPASS — the plug-in follows host transport."
                    : "\n\(failures) check(s) FAILED")
exit(failures == 0 ? 0 : 1)
