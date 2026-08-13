// au-msgchannel-spike.swift — CAN WE TALK ACROSS THE AUv3 PROCESS BOUNDARY, AND HOW FAST?
//
// The one question blocking the plug-in fork (docs/design/ios-plan.md → "The out-of-process wall").
// GarageBand runs our UI and our audio in DIFFERENT PROCESSES, so a view that blits the engine's
// framebuffer is drawing an engine nobody can hear. Two of the four ways out need a channel between
// those processes, and they need very different things from it:
//
//   ship PIXELS  (option 4) — ~150 KB a frame at 20-30 fps. Keeps the pixel canvas, which is the
//                             thing that makes this rack ours. Needs THROUGHPUT.
//   ship STATE   (the architectural alternative) — a few hundred bytes a frame, with the UI-process
//                             engine RE-RENDERING locally, netplay-style. Needs LOW LATENCY.
//
// This spike measures both on the real API (AUMessageChannel), so the fork gets decided on numbers
// instead of on estimates. It changes no engine code.
//
//   xcrun swiftc -O -o au-msgchannel-spike au-msgchannel-spike.swift -framework AVFoundation -framework CoreAudioKit
//   ./au-msgchannel-spike                # the measurement
//   ./au-msgchannel-spike --in-process   # the CONTROL — see below
//
// ⚠ THE CONTROL IS THE POINT — and it immediately caught the author, which is the best argument for
// keeping it. The first version of this header claimed that ios/au-transport-check.swift was running
// IN-process because it passes `options: []`, and therefore that every AU gate we own exercised the
// wrong topology. MEASURED: FALSE. On macOS an AUv3 app extension loads OUT-of-process regardless of
// that flag — `--in-process` here reports "loaded OUT-of-process (asked for in)" and says outright
// that it is isolating nothing. au-transport-check's comment was accurate all along.
// What remains true, and is why the check stays:
//   1. we pass .loadOutOfProcess explicitly (harmless where it is already the default, correct where
//      it is not — iOS and future macOS are not obliged to agree), and
//   2. we ASSERT isLoadedInProcess == false before believing any number, so a platform that DOES
//      honour in-process loading cannot hand us flattering numbers that say nothing about a DAW.

import AVFoundation
import AudioToolbox
import CoreAudioKit
import Foundation

let args = CommandLine.arguments
let wantInProcess = args.contains("--in-process")

func die(_ s: String) -> Never { print("✗ \(s)"); exit(1) }

// ── find the plug-in (same identifiers ios/mac.sh registers) ──
var desc = AudioComponentDescription()
desc.componentType         = kAudioUnitType_MusicDevice
desc.componentSubType      = 0x7461636A   // 'tacj'
desc.componentManufacturer = 0x4D706C61   // 'Mpla'
desc.componentFlags        = 0
desc.componentFlagsMask    = 0

var au: AVAudioUnit?
var failed: Error?
var answered = false

// AudioComponentInstantiationOptions.loadOutOfProcess is what a real host uses; [] loads it in the
// caller. This is the ONE line the whole measurement hangs on.
let opts: AudioComponentInstantiationOptions = wantInProcess ? [] : [.loadOutOfProcess]

AVAudioUnit.instantiate(with: desc, options: opts) { u, e in au = u; failed = e; answered = true }

// Pump the run loop rather than blocking: a -UI extension does part of its loading on the MAIN
// QUEUE, and a semaphore wait here deadlocks and then blames registration (ios-plan.md records that
// trap costing a round of work).
let deadline = Date().addingTimeInterval(20)
while !answered, Date() < deadline { RunLoop.current.run(mode: .default, before: Date().addingTimeInterval(0.05)) }

guard let avAU = au else { die("could not instantiate aumu/tacj/Mpla (\(failed?.localizedDescription ?? "timeout")) — run: zsh ios/mac.sh") }
let unit = avAU.auAudioUnit

// ── the control: are we ACTUALLY where we think we are? ──
let inProc = unit.isLoadedInProcess
print("loaded \(inProc ? "IN-process" : "OUT-of-process")  (asked for \(wantInProcess ? "in" : "out"))")
if !wantInProcess && inProc {
    die("asked for out-of-process and got in-process — every number below would be meaningless")
}
if wantInProcess && !inProc {
    print("  note: asked for in-process and got out-of-process; the control is not isolating anything")
}

// ── the channel ──
// AUMessageChannel is AUv3's own cross-process pipe. A plug-in opts in by answering
// messageChannel(for:) with a name it publishes; if ours does not implement it yet, that is the
// FIRST piece of work and this spike says so plainly rather than crashing.
let channelName = "com.tinyjam.canvas"
let ch = unit.messageChannel(for: channelName)

// messageChannel(for:) is NON-optional — AUAudioUnit's base implementation always hands back an
// object, so "did the plug-in actually implement this?" cannot be answered by a nil check. Probe it:
// ask for an echo of a marker and see whether anything comes back. A base/no-op channel answers with
// nothing, which is exactly the not-implemented-yet case and deserves a plain sentence, not a crash.
guard let call = ch.callAudioUnit else {
    print("\n  the channel exposes no callAudioUnit block — nothing implemented on the plug-in side yet.")
    exit(2)
}
let probe = call(["op": "echo", "marker": 0xAC1D] as [AnyHashable: Any])
if probe.isEmpty {
    print("""

    the plug-in answers nothing on channel "\(channelName)" — it has not implemented one yet.
      THAT IS THE FIRST BUILD STEP, and it is small: TinyjamAU overrides messageChannel(for:) to
      return an object conforming to AUMessageChannel whose callAudioUnit(_:) echoes the payload
      back. (ios/AU/TinyjamAU.swift). Re-run and this spike prints the table for real.
    """)
    exit(2)
}

// ── measure: round-trip latency by payload size ──
// Sizes chosen to bracket the two options: 64B/1KB answer "can we ship STATE/inputs cheaply", and
// 64KB/150KB/256KB answer "can we ship PIXELS" (492×308 ≈ 151k px ≈ 150KB at one byte per palette
// index, which is what our canvas actually is).
let sizes = [64, 1024, 16 * 1024, 64 * 1024, 151_536, 256 * 1024]
let reps  = 60

// NB: %s in String(format:) takes a C string — handing it a Swift String is undefined and
// segfaults. Interpolation + padding instead.
func pad(_ s: String, _ n: Int) -> String { s.count >= n ? s : s + String(repeating: " ", count: n - s.count) }
func rpad(_ s: String, _ n: Int) -> String { s.count >= n ? s : String(repeating: " ", count: n - s.count) + s }
print("\n  " + pad("payload", 11) + rpad("rtt avg", 11) + rpad("rtt max", 11) + rpad("max fps", 10))
print("  " + String(repeating: "─", count: 44))

var pixelsOK = false, stateOK = false
for size in sizes {
    let blob = Data(count: size)
    var best = Double.greatestFiniteMagnitude, worst = 0.0, total = 0.0
    for _ in 0..<reps {
        let t0 = CFAbsoluteTimeGetCurrent()
        _ = call(["op": "echo", "payload": blob] as [AnyHashable: Any])
        let dt = (CFAbsoluteTimeGetCurrent() - t0) * 1000.0     // ms
        total += dt; best = min(best, dt); worst = max(worst, dt)
    }
    let avg = total / Double(reps)
    let fps = avg > 0 ? 1000.0 / avg : 0
    print("  " + pad("\(size)B", 11)
        + rpad(String(format: "%.3fms", avg), 11)
        + rpad(String(format: "%.3fms", worst), 11)
        + rpad(String(format: "%.0f", fps), 10))
    if size >= 151_536 && fps >= 20 { pixelsOK = true }
    if size <= 1024   && fps >= 60 { stateOK = true }
}

// ── THE REAL THING: a live frame out of the audio process ──────────────────────────────────────
// Everything above is an echo — a payload the plug-in bounces without touching the engine. That
// measures the pipe and nothing else. This asks for an ACTUAL frame, so the number includes
// de_copy_frame's seqlock read and the Data copy, which is what a real panel would pay.
//
// It also checks the frame is PLAUSIBLE, because "it returned bytes" is not the same as "it returned
// a picture": non-zero dimensions, a byte count that matches w*h*4, and — the one that matters — some
// NON-ZERO pixels. An all-black frame would satisfy every other check while proving the UI process is
// still looking at a dead engine, which is the exact bug this whole exercise is about.
// The plug-in must actually RENDER before it has a frame to give: de_frame runs from the render
// block, so an AU that has never rendered has published nothing and de_copy_frame honestly reports
// 0x0. The first run of this spike hit exactly that and the message blamed a black picture, which
// was wrong twice over — there were no pixels at all, and the AU was simply idle. So drive it like a
// host first (the au-transport-check rig: manual offline rendering), then ask.
let renderFmt = AVAudioFormat(standardFormatWithSampleRate: 44100, channels: 2)!
let hostEngine = AVAudioEngine()
hostEngine.attach(avAU)
hostEngine.connect(avAU, to: hostEngine.mainMixerNode, format: renderFmt)
var rendered = 0
do {
    try hostEngine.enableManualRenderingMode(.offline, format: renderFmt, maximumFrameCount: 4096)
    try hostEngine.start()
    if let rbuf = AVAudioPCMBuffer(pcmFormat: hostEngine.manualRenderingFormat, frameCapacity: 4096) {
        for _ in 0..<40 {                                   // ~1.9s at 2048 frames a go
            guard (try? hostEngine.renderOffline(2048, to: rbuf)) == .success else { break }
            rendered += 2048
        }
    }
    print("\n  drove the plug-in for \(rendered) frames of audio so it has something to show")
} catch {
    print("\n  ⚠ could not drive the plug-in (\(error.localizedDescription)) — a frame may not exist yet")
}

// WHICH ENGINE ANSWERED. Printed loudly because it is the number the maker compares against the
// one the PANEL logs in a DAW: same nonce = the panel is connected to the engine making sound;
// different = still two engines, and this whole route is closed.
do {
    let who = call(["op": "nonce"] as [AnyHashable: Any])
    print("\n  ENGINE NONCE (host side): \(who["nonce"] ?? "?")   pid \(who["pid"] ?? "?")")
    print("  ↳ compare with the panel's Console line in a DAW; SAME = connected, DIFFERENT = two engines")
}

print("\n  A LIVE FRAME (not an echo — includes de_copy_frame + the copy)")
var frameOK = false
var fw = 0, fh = 0, fbytes = 0, nonzero = 0
var favg = 0.0
do {
    let warm = call(["op": "frame"] as [AnyHashable: Any])
    if warm.isEmpty {
        print("    the plug-in returned nothing for op=frame — the frame path is not wired yet")
    } else {
        fw = (warm["w"] as? Int) ?? 0
        fh = (warm["h"] as? Int) ?? 0
        if let px = warm["px"] as? Data {
            fbytes = px.count
            nonzero = px.withUnsafeBytes { raw -> Int in
                let u = raw.bindMemory(to: UInt32.self)
                var n = 0
                for v in u where (v & 0x00FF_FFFF) != 0 { n += 1 }
                return n
            }
        }
        var total = 0.0
        let reps2 = 60
        for _ in 0..<reps2 {
            let t0 = CFAbsoluteTimeGetCurrent()
            _ = call(["op": "frame"] as [AnyHashable: Any])
            total += (CFAbsoluteTimeGetCurrent() - t0) * 1000.0
        }
        favg = total / Double(reps2)
        let expect = fw * fh * 4
        let sane = fw > 0 && fh > 0 && fbytes == expect && nonzero > 0
        frameOK = sane && favg > 0
        print("    \(fw)x\(fh)  \(fbytes) bytes  (expect \(expect))  non-zero px \(nonzero)")
        print("    rtt " + String(format: "%.3fms", favg) + "  →  " + String(format: "%.0f", favg > 0 ? 1000/favg : 0) + " fps ceiling")
        // THREE distinct failures, three distinct sentences. The first version printed "every pixel
        // is black" for a 0x0 reply, which is not a black frame — it is no frame — and it sent the
        // reader looking at the wrong end. Conflating failure modes in a message is the same mistake
        // as conflating them in a check.
        if fw == 0 || fh == 0 { print("    ✗ NO FRAME PUBLISHED (0x0) — the engine has not rendered one yet, or is not booted") }
        else if fbytes != expect { print("    ✗ byte count \(fbytes) != w*h*4 (\(expect)) — the payload is not a whole frame") }
        else if nonzero == 0 { print("    ✗ EVERY PIXEL IS BLACK — a full frame crossed, but no picture did") }
    }
}

// ── the verdict, stated as what it decides rather than pass/fail ──
print("""

  READ IT LIKE THIS
    pixels (option 4)  \(pixelsOK ? "VIABLE — a 150KB frame sustains 20fps+, the canvas can cross the boundary"
                                  : "NOT viable at 150KB — either downscale/compress the frame, or ship STATE instead")
    live frame         \(frameOK ? "REAL — a genuine engine frame crossed, non-black, at the rate above"
                                 : "NOT PROVEN — see the frame block above")
    state  (netplay)   \(stateOK ? "VIABLE — small messages are cheap, so mirroring inputs and re-rendering wins on bandwidth"
                                 : "small messages are NOT cheap either — the channel itself is the bottleneck")

  Numbers from an IN-process run mean nothing about GarageBand. Compare with --in-process:
  if the two tables look the same, .loadOutOfProcess is not doing what this spike assumes.
""")
