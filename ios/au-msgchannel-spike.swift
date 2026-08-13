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

// ── the verdict, stated as what it decides rather than pass/fail ──
print("""

  READ IT LIKE THIS
    pixels (option 4)  \(pixelsOK ? "VIABLE — a 150KB frame sustains 20fps+, the canvas can cross the boundary"
                                  : "NOT viable at 150KB — either downscale/compress the frame, or ship STATE instead")
    state  (netplay)   \(stateOK ? "VIABLE — small messages are cheap, so mirroring inputs and re-rendering wins on bandwidth"
                                 : "small messages are NOT cheap either — the channel itself is the bottleneck")

  Numbers from an IN-process run mean nothing about GarageBand. Compare with --in-process:
  if the two tables look the same, .loadOutOfProcess is not doing what this spike assumes.
""")
