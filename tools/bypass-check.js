#!/usr/bin/env node
// bypass-check.js — the RECONVERGENCE oracle: when you switch an effect OUT, does the mix come back
// BIT-EXACT, and how long does it take? Nothing else in the repo asserts that.
//
//   node tools/bypass-check.js                 render the outboard rack + report all four stages
//   node tools/bypass-check.js --quiet          PASS/FAIL per stage, exit 1 on any fail (CI)
//   node tools/bypass-check.js --stage PLATE    just one stage
//   node tools/bypass-check.js --selfcheck      known answers for the ANALYSER, on WAVs it
//                                              synthesises (no cart, no engine, nothing rendered)
//   node tools/bypass-check.js --selfcheck --measure   dump what the fixtures actually read
//   node tools/bypass-check.js --json           machine-readable
//   node tools/bypass-check.js --keep           keep the rendered WAVs and print their paths
//   node tools/bypass-check.js --frames <n>     render length (default 720 = 12.0 s; the PLATE tail
//                                              needs room, and the gate says so when it runs out)
//   node tools/bypass-check.js --switch-at <n>  the frame the stage is switched OUT (default 90 = 1.5 s)
//   node tools/bypass-check.js --gap <n>        frames the stage stays out before coming back IN,
//                                              for the IN direction (default 45 = 0.75 s)
//   node tools/bypass-check.js --direction out|in|both   which reconvergence to measure (default both)
//
// WHY A WHOLE-FILE SHA IS THE WRONG TOOL (the insight this gate exists to preserve). A stage that is
// IN changes the audio: that is its job. So `sha(A) != sha(B)` says only "differs", which is true of
// every working effect and tells you nothing about the bypass. The number that carries the meaning is
// the **LAST DIFFERING SAMPLE**. Two renders that reconverge have a last difference; two renders that
// do not, do not. Everything below is built around locating it:
//
//   BASELINE   the rack switched fully out early, and left out.
//   STAGE      the identical run with ONE stage switched back IN, then switched OUT at frame N.
//
// They must differ while the stage is in (that is the LIVENESS half) and be byte-identical from the
// switch onward (that is the BYPASS half). Reconvergence delay = last differing sample - the switch.
//
// TWO DIRECTIONS, and they are different questions with different answers. Switching a stage OUT and
// switching it back IN both have to reconverge, but only the OUT direction can be bit-exact at the
// switch: a stage that RE-ENGAGES starts from empty state, and the reference run's state is however
// full the music has made it. So it converges at the rate of that stage's own memory. Measured on
// the outboard rack: IRON, a memoryless waveshaper, returns on the switching sample in BOTH
// directions; the console EQ takes hundreds of ms coming back IN, because its low band's corner is
// 80 Hz and one period there is 12.5 ms. That number is a property of the STAGE, not of the switch,
// which is the whole argument for a per-stage window instead of a boolean.
//
//   --direction out   OUT only: baseline = the stage never in.  Variant = in, then OUT at frame N.
//   --direction in    IN only:  baseline = the stage in throughout. Variant = out for a gap, then
//                     back IN. Reconvergence is measured from the moment it comes back.
//   --direction both  (default) both, reported as two rows per stage.
//
// It was a SECOND consumer of outboard.h (the `sideman` organ cabinet) that surfaced the IN
// direction: 0.304 s to reconverge on an EQ+IRON pair, with the plate parked out so nothing could be
// blamed on a tail. Until then the doc's table read as covering both and covered one.
//
// ⚠ THE FAILURE MODE THIS GATE IS BUILT AGAINST IS A VACUOUS PASS. Two renders that are identical
// EVERYWHERE also have "no difference after the switch", and a naive reading of that is a perfect
// score. It is the commonest way an A/B harness lies: the toggle never reached the DSP, the key was
// never claimed, the stage's null and its "in" values happen to be the same. tools/ab-render.js hit
// exactly this and exits 2 on byte-identical variants. So a stage here must EARN its pass:
//
//   1. the two renders must differ over the window where the stage is in, and
//   2. that difference must be AUDIBLE (peak diff above --floor, default -60 dBFS), not a rounding
//      wobble a couple of LSBs wide, and
//   3. the difference must START in the window (a run that only differs AFTER the switch is measuring
//      something other than the stage).
//
//   Any of those failing reports INCONCLUSIVE and exits nonzero. Inconclusive is not a pass.
//
// A THIRD RENDER IS A CONTROL, not a cost: the baseline is rendered TWICE and the two must be
// byte-identical. Without it, "the renders differ" could be measuring engine nondeterminism, and
// every number below would be noise. (`--no-det-control` skips it; do not, in CI.)
//
// TOLERANCE IS A TIME, NOT A BOOLEAN. Three of the outboard stages return on the SAME SAMPLE the
// switch flips, because their nulls are exact (eq 0/0/0, drive amount 0, glue amount 0). The PLATE
// cannot: its bypass sets the SENDS to zero and a reverb tail is real, so the tank keeps ringing what
// it was already holding. That is correct behaviour, not a leak, and a gate that called it a failure
// would be wrong. So each stage carries its own allowed reconvergence window in ms.
//
// EXTENDING IT TO ANOTHER CART: add a RACKS entry. A rack is a cart plus, per stage, the KEY that
// toggles it and the window it is allowed. Keyboard, deliberately: a mouse tap is an absolute canvas
// coordinate that does not survive a relayout, a key is position-free (resolution-portable-input.md).
//
// ⚠ AND ONE HAZARD THAT IS SPECIFIC TO THIS REPO: play.js recompiles per render, and several agents
// edit runtime/ on one shared working tree. If sound.h lands a change BETWEEN two of these renders,
// the two WAVs come from two different engines and every number below is meaningless. It happened on
// this tool's second day: the report came back four-stages-red with "the runs are not the same run",
// which is true and blames the wrong thing. So the engine sources are fingerprinted before the first
// render and after the last, and a move is reported as a MOVE, not as a finding (refactor-guard.js
// does the same with HEAD, for the same reason).
//
// Design + the measurements: docs/design/analog-outboard-chain.md §4.

const fs = require('fs')
const os = require('os')
const path = require('path')
const crypto = require('crypto')
const { spawnSync } = require('child_process')

const ROOT = path.resolve(__dirname, '..')
const SR = 44100
const FPS = 60
const SPF = SR / FPS            // 735 samples per frame — studio.c's wav_stream_pump()

// ── the racks ────────────────────────────────────────────────────────────────
// Per stage: the key that toggles it, and the reconvergence window it is ALLOWED, with the reason.
// A window of 0 means "the same sample the switch flips" and is the strong claim.
const RACKS = {
  outboard: {
    cart: 'outboard',
    // the keys that switch every stage out, pressed in BOTH renders, so both share one boot
    // transient and the only difference left is the stage under test.
    allOff: ['1', '2', '3', '4'],
    stages: [
      // Two criteria per stage, because OUT and IN are different questions (see the header). Each
      // strict row carries a residual CEILING as well as a time, so a loosened window cannot hide a
      // leak: a stage that failed to null at all is tens of dB above these ceilings and goes red.
      // EVERY NUMBER BELOW IS MEASURED, on this cart's GROOVE programme at ROOM headroom.
      { name: 'EQ', key: '1',
        out: { tolMs: 0, residDb: -Infinity,
               why: 'eq_inst(0) nulled to 0/0/0, and nothing upstream of it moved, so the null is exact' },
        // MEASURED 0.0 ms, and this REFUTED the obvious hypothesis. The console EQ is two cascaded
        // one-poles and the low one sits at 80 Hz, so "a re-engaging filter settles over hundreds of
        // ms" is the natural guess — and it is wrong here, because eq_process's STATE is driven by its
        // INPUT and its gains only scale the three bands it already split. eq_inst(0) is first in the
        // chain, so its input is identical in both runs and its state never diverges. A filter's
        // settling time only shows up when something UPSTREAM of it changed.
        in:  { tolMs: 0, residDb: -Infinity,
               why: 'the crossover state is driven by its INPUT, which is identical, so re-engaging is exact too' } },
      { name: 'IRON', key: '2',
        out: { tolMs: 0, residDb: -Infinity,
               why: 'drive_insert(0) returns before it touches the sample, so the dry path is untouched' },
        // MEASURED 320.7 ms, and this is the stage that is NOT memoryless, which is the opposite of
        // what "a waveshaper" suggests. drive_process runs a DC BLOCKER on the wet path (asym
        // clipping is one-sided, so it has to), a one-pole highpass at R = 0.999 ≈ 7 Hz. The `dr <=
        // 0.001f` early-out returns BEFORE that filter, so its state FREEZES while the stage is out
        // and discharges when it comes back. The decay was measured at 0.6433 per 10 ms, which is
        // 0.999^441 to four figures: the mechanism is not inferred, it is identified. Arguably right
        // (a real pedal's coupling cap holds charge too); the only thing that was wrong was the CLAIM
        // that the stage has no memory. 500 ms = the measurement plus room for other material.
        in:  { tolMs: 500, residDb: 0,
               why: "the wet path's 7 Hz DC blocker froze while the stage was out and discharges over ~300 ms" } },
      // MEASURED 2026-08-19, and the OUT row is the finding this gate was built to catch. `glue`
      // amount 0 clears sc.used, so the comp itself IS exact. The residual comes from the stage's
      // OTHER two parts: eq_inst(1) reconstructs its input as lo + mid + (hi - mid), which is an
      // ALGEBRAIC null and not a float-exact one, and the stage's own `dirt` changes what reaches
      // that EQ. So its retained one-pole state diverges between the two runs and the rounding
      // differs until the states reconverge. Proven by setting the ratio's dirt to 0: the residual
      // vanishes entirely. Hence a window AND a ceiling: 25 ms is the observed decay and -85 dBFS is
      // one LSB, so a stage that did not actually null is still caught.
      // ⚠ THE ALLOWANCE STAYS EVEN THOUGH THIS ROW NOW READS 0.0 ms. The residual lives AT the 16-bit
      // quantisation boundary, so whether it produces a differing SAMPLE depends on the material: the
      // first programme this was measured on gave 2 samples at -90.31 dBFS (exactly 1 LSB) 17 ms
      // after the switch, the retuned one gives none. Tightening the row to 0 because today's loop
      // happens to round the other way would make it fail on a mix change, for no defect.
      { name: 'COMP', key: '3',
        out: { tolMs: 25, residDb: -85,
               why: 'glue amount 0 clears sc.used (exact), but a nulled eq_inst is an ALGEBRAIC null: <=1 LSB for ~17 ms' },
        // The SETTLE row, and the reason that criterion exists. `glue` learns its makeup from a
        // ~1.5 s one-pole average, so a re-engaging comp needs ln(2^15)·1.5 s ≈ 16 s for that
        // averager alone to converge to within one LSB. MEASURED: bit-exact after 20.5 s, below
        // -60 dBFS after 2.66 s. Gating the first number would be gating an averager's float
        // precision; the second is the audible question. Both are printed.
        in:  { settleMs: 3500, settleDb: -60,
               why: "glue's makeup average is a ~1.5 s one-pole, so bit-exact takes ~20 s by construction" } },
      { name: 'PLATE', key: '4',
        // MEASURED 3588 ms out / 3970 ms in at plate_amt 0.55; below -60 dBFS in 651 ms / 562 ms,
        // which is the number that matters for whether you can HEAR a clean A/B with the plate in.
        out: { tolMs: 6000, residDb: 0,
               why: 'the sends go to 0 but the TANK keeps ringing what it already holds: a reverb tail is real' },
        in:  { tolMs: 6000, residDb: 0,
               why: "the tank starved during the gap and refills at the tail's own rate" } },
    ],
  },

  // The SECOND consumer of outboard.h, and the reason it is worth gating rather than measuring by
  // hand once: `sideman` (the Wurlitzer Side Man's organ cabinet) uses the same table as a SUBSET.
  // It pins EQ and IRON together as one CABINET switch and leaves COMP out entirely, because a 1959
  // organ amplifier had no bus compressor, and its programme is percussion rather than a groove with
  // a bassline. Same table, different material, different subset: if a stage's reconvergence is a
  // property of the STAGE, these rows must agree with the outboard rows above. It was this cart's
  // hand-measured 0.304 s that surfaced the IN direction in the first place.
  sideman: {
    cart: 'sideman',
    allOff: ['C', 'V'],          // the cart boots with both in circuit, so one press each parks them
    stages: [
      // EQ and IRON arrive together here, so this row is the SUM of the outboard EQ and IRON rows:
      // 0 ms out (both null exactly) and IRON's frozen DC blocker on the way back in.
      { name: 'CABINET', key: 'C',
        out: { tolMs: 0, residDb: -Infinity,
               why: 'eq_inst(0) nulled to 0/0/0 and drive_insert(0) returns before touching the sample' },
        in:  { tolMs: 500, residDb: 0,
               why: "IRON's wet-path 7 Hz DC blocker froze while the stage was out and discharges over ~300 ms" } },
      { name: 'TANK', key: 'V',
        out: { tolMs: 6000, residDb: 0,
               why: 'the sends go to 0 but the tank keeps ringing what it already holds' },
        in:  { tolMs: 6000, residDb: 0,
               why: "the tank starved during the gap and refills at the tail's own rate" } },
    ],
  },
}

// ── the engine fingerprint ───────────────────────────────────────────────────
// Every source a render depends on. Hashed before the first render and after the last: if it moved,
// the WAVs came from two different builds.
const ENGINE_FILES = ['runtime/sound.h', 'runtime/studio.c', 'runtime/studio.h', 'runtime/outboard.h']
function engineFingerprint (cart) {
  const h = crypto.createHash('sha256')
  for (const f of ENGINE_FILES.concat([`tools/carts/${cart}.c`])) {
    const p = path.join(ROOT, f)
    h.update(f).update(fs.existsSync(p) ? fs.readFileSync(p) : Buffer.alloc(0))
  }
  return h.digest('hex').slice(0, 16)
}

// ── WAV (16-bit PCM), read as raw interleaved int16 ──────────────────────────
// Interleaved on purpose: the claim under test is BIT-EXACT, so the unit is the sample as written,
// not a downmix. (Every other audio gate here averages L and R at the door, which would hide a
// difference that lives on one side only.)
function readWavPcm (file) {
  const b = fs.readFileSync(file)
  if (b.toString('ascii', 0, 4) !== 'RIFF' || b.toString('ascii', 8, 12) !== 'WAVE')
    throw new Error(`${file}: not a WAV`)
  let off = 12, fmt = null, data = null
  while (off + 8 <= b.length) {
    const id = b.toString('ascii', off, off + 4)
    const len = b.readUInt32LE(off + 4)
    if (id === 'fmt ') fmt = { tag: b.readUInt16LE(off + 8), ch: b.readUInt16LE(off + 10),
                               sr: b.readUInt32LE(off + 12), bits: b.readUInt16LE(off + 22) }
    if (id === 'data') data = { off: off + 8, len }
    off += 8 + len + (len & 1)
  }
  if (!fmt || !data) throw new Error(`${file}: missing fmt/data chunk`)
  if (fmt.tag !== 1 || fmt.bits !== 16) throw new Error(`${file}: expected 16-bit PCM`)
  const n = Math.floor(data.len / 2)
  const s = new Int16Array(n)
  for (let i = 0; i < n; i++) s[i] = b.readInt16LE(data.off + i * 2)
  return { sr: fmt.sr, ch: fmt.ch, s, n }
}

const dbfs = v => v <= 0 ? -Infinity : 20 * Math.log10(v / 32768)
const fmtDb = v => v === -Infinity ? '   -inf' : (v >= 0 ? '+' : '') + v.toFixed(2)
// sub-millisecond precision matters: a tol-0 stage that misses by ONE sample is 0.023 ms,
// and a formatter that rounds that to "0.0 ms" prints a failure that reads like a pass.
const fmtMs = v => (v !== 0 && v < 1 ? v.toFixed(4) : v.toFixed(1)) + ' ms'

// ── the analyser ─────────────────────────────────────────────────────────────
// Pure: takes two decoded renders + the frame the stage was switched out, returns the numbers. No
// cart, no files, no judgement — which is what lets --selfcheck feed it constructed answers.
//
// `inFromFrame` is the frame the stage was switched back IN. Everything before it is shared prelude
// and must be identical in both renders; a difference there means the two runs are not the same run,
// which is a harness bug, not a finding, and is reported as such.
function analyse (base, stage, inFromFrame, switchFrame) {
  if (base.sr !== stage.sr) throw new Error(`sample-rate mismatch: ${base.sr} vs ${stage.sr}`)
  if (base.ch !== stage.ch) throw new Error(`channel-count mismatch: ${base.ch} vs ${stage.ch}`)
  if (base.n !== stage.n) throw new Error(`length mismatch: ${base.n} vs ${stage.n} samples ` +
    `(the two renders must be the same length, or "reconverged" only means "ran out of file")`)
  const perFrame = (base.sr / FPS) * base.ch      // interleaved samples per video frame
  const iIn = Math.round(inFromFrame * perFrame)
  const iSw = Math.round(switchFrame * perFrame)
  // interleaved index → seconds. Divide by the channel count FIRST: an interleaved step is half a
  // sample period in stereo, and a delay reported in half-samples is a number nobody can check.
  const t = i => Math.floor(i / base.ch) / base.sr
  const oneSample = 1 / base.sr
  // The switch is applied in the frame's update(), so sample iSw is the FIRST one rendered with the
  // stage OUT. A difference AT iSw therefore means it did NOT come back on the switching sample, and
  // must read as a delay rather than as zero — otherwise the tol-0 boundary is a sample wide and the
  // strong claim ("bit-exact at the switch") is not actually being tested. --selfcheck pins this.
  const delayMs = last => last < iSw ? 0 : (t(last) - t(iSw) + oneSample) * 1000

  let lastDiff = -1, firstDiff = -1
  let preludeDiff = -1                            // a difference BEFORE the stage went in
  let inCount = 0, inPeak = 0, inSq = 0, inTot = 0
  let postCount = 0, postPeak = 0
  // the decay trail: when does the residual fall below a series of floors and stay there
  const floors = [-40, -60, -80]
  const floorLast = floors.map(() => -1)

  for (let i = 0; i < base.n; i++) {
    const d = base.s[i] - stage.s[i]
    if (d !== 0) {
      if (firstDiff < 0) firstDiff = i
      lastDiff = i
      if (i < iIn && preludeDiff < 0) preludeDiff = i
      const a = d < 0 ? -d : d
      for (let f = 0; f < floors.length; f++) if (dbfs(a) > floors[f]) floorLast[f] = i
      if (i >= iSw) { postCount++; if (a > postPeak) postPeak = a }
    }
    if (i >= iIn && i < iSw) {
      inTot++
      if (d !== 0) {
        inCount++
        const a = d < 0 ? -d : d
        if (a > inPeak) inPeak = a
        inSq += d * d
      }
    }
  }
  return {
    sr: base.sr, ch: base.ch, n: base.n, durS: base.n / (base.sr * base.ch),
    inFromFrame, switchFrame, inFromS: t(iIn), switchS: t(iSw),
    // liveness: did the stage do anything AUDIBLE while it was in?
    inDiffFrac: inTot ? inCount / inTot : 0,
    inPeakDb: dbfs(inPeak),
    inRmsDb: inCount ? dbfs(Math.sqrt(inSq / Math.max(1, inTot))) : -Infinity,
    firstDiffS: firstDiff < 0 ? null : t(firstDiff),
    preludeDiffS: preludeDiff < 0 ? null : t(preludeDiff),
    // the headline: the last differing sample, and how long after the switch that is
    lastDiffS: lastDiff < 0 ? null : t(lastDiff),
    reconvergeMs: delayMs(lastDiff),
    postCount, postPeakDb: dbfs(postPeak),
    // how much file is LEFT after the last difference. A tail still running when the render stops
    // "reconverged" only in the sense that it ran out of samples — see judge().
    tailRoomMs: lastDiff < 0 ? Infinity : (t(base.n - 1) - t(lastDiff)) * 1000,
    // how the residual decays after the switch: the honest shape of a reverb tail
    // `ever: false` means the residual NEVER reached that floor. Printing "falls below -40 dB at
    // 0.0 ms" for a residual that never got near -40 dB reads like a fast decay from something loud.
    decay: floors.map((f, k) => ({ floorDb: f, ever: floorLast[k] >= 0, afterMs: delayMs(floorLast[k]) })),
    identical: lastDiff < 0,
  }
}

// ── the verdict ──────────────────────────────────────────────────────────────
// Three outcomes, and the middle one is the point of the tool:
//   PASS          it reconverged inside its allowed window, and it was doing something first
//   FAIL          it did not reconverge inside the window (the bypass is not exact)
//   INCONCLUSIVE  the run cannot answer: the stage did nothing measurable while in
function judge (a, tol, floorDb, endGuardMs = 100) {
  const reasons = []
  const strict = tol.settleMs === undefined
  if (a.preludeDiffS !== null)
    return { verdict: 'ERROR', reasons: [`the two renders already differ at ${a.preludeDiffS.toFixed(3)}s, ` +
      `before the window under test opens at ${a.inFromS.toFixed(3)}s — the runs are not the same run`] }
  if (a.identical)
    return { verdict: 'INCONCLUSIVE', reasons: ['the two renders are BYTE-IDENTICAL everywhere: the ' +
      'stage never reached the DSP (or its "in" values equal its null), so "reconverged" is vacuous'] }
  if (a.inDiffFrac < 0.01)
    reasons.push(`only ${(a.inDiffFrac * 100).toFixed(3)}% of samples differ across the window ` +
      `(want > 1%): the stage is doing essentially nothing, so the claim is untested`)
  if (!(a.inPeakDb > floorDb))
    reasons.push(`the difference across the window peaks at ${fmtDb(a.inPeakDb)} dBFS, at or below the ` +
      `${floorDb} dBFS floor: that is a rounding wobble, not an effect`)
  if (a.firstDiffS !== null && a.firstDiffS >= a.switchS)
    reasons.push(`the renders first differ at ${a.firstDiffS.toFixed(3)}s, AT OR AFTER the switch ` +
      `(${a.switchS.toFixed(3)}s): whatever moved, it was not this stage`)
  if (reasons.length) return { verdict: 'INCONCLUSIVE', reasons }

  // ── criterion A: BIT-EXACT inside a window (the strong claim, and the default) ──
  if (strict) {
    // The CEILING is judged first, deliberately before the ran-out-of-file guard: a residual this
    // far above the floor is conclusive evidence of a leak whenever the file happened to end, and
    // calling that "inconclusive" would file a real defect under "rerun it longer".
    if (a.postPeakDb > tol.residDb)
      return { verdict: 'FAIL', reasons: [`the residual after the switch peaks at ${fmtDb(a.postPeakDb)} dBFS, ` +
        `above the ${tol.residDb === -Infinity ? 'bit-exact (no residual at all)' : tol.residDb + ' dBFS'} ` +
        `this stage allows: something is still leaking through the null, not just rounding`] }
    // THE OTHER VACUOUS PASS, and subtler than byte-identical: a difference still going when the
    // render STOPS has a "last differing sample" too, and it sits at the end of the file. Reporting
    // that as a reconvergence is reporting --frames. It bit this tool on its first real run: the
    // PLATE "reconverged at 2997.6 ms" in a 3000 ms post-switch window.
    if (a.tailRoomMs < endGuardMs)
      return { verdict: 'INCONCLUSIVE', reasons: [`the last difference is only ${fmtMs(a.tailRoomMs)} ` +
        `before the END of the render: the two runs had not reconverged when the file ran out, so the ` +
        `number is the render length, not a reconvergence. Raise --frames`] }
    if (a.reconvergeMs > tol.tolMs)
      return { verdict: 'FAIL', reasons: [`last difference ${fmtMs(a.reconvergeMs)} after the switch, ` +
        `outside the ${fmtMs(tol.tolMs)} allowed: the bypass is not returning to bit-exact` +
        (tol.tolMs === 0 ? ' on the switching sample' : '')] }
    return { verdict: 'PASS', reasons: [] }
  }

  // ── criterion B: SETTLES BELOW A LEVEL inside a window ──
  // For a stage holding a slowly LEARNED average. `glue`'s makeup averages over ~1.5 s, so a
  // re-engaging comp needs ln(2^15)·1.5 s ≈ 16 s just for that one-pole to converge to within one
  // LSB: measured, 20.5 s. Holding that to "bit-exact" would gate the FLOAT PRECISION of an
  // averager, not the bypass — a 25 s window is not a strict gate, it is a number nobody can read.
  // So this criterion asks the audible question instead, and the report still prints the bit-exact
  // number beside it so the weaker claim is never mistaken for the strong one.
  const d = a.decay.find(x => x.floorDb === tol.settleDb)
  if (!d) return { verdict: 'ERROR',
    reasons: [`settleDb ${tol.settleDb} is not one of the measured floors ` +
              `(${a.decay.map(x => x.floorDb).join(', ')})`] }
  // still above the floor when the file ran out: cannot answer, ask for a longer render
  if (d.ever && (a.durS * 1000 - (a.switchS * 1000 + d.afterMs)) < endGuardMs)
    return { verdict: 'INCONCLUSIVE', reasons: [`the residual was still above ${tol.settleDb} dBFS ` +
      `${fmtMs(d.afterMs)} after the switch, which is the END of the render: raise --frames`] }
  if (d.afterMs > tol.settleMs)
    return { verdict: 'FAIL', reasons: [`the residual took ${fmtMs(d.afterMs)} to fall below ` +
      `${tol.settleDb} dBFS, outside the ${fmtMs(tol.settleMs)} allowed`] }
  return { verdict: 'PASS', reasons: [] }
}

// one line naming which criterion a row was held to, so a SETTLE pass is never read as a bit-exact
// one. The bit-exact number is printed either way.
function criterionOf (tol) {
  return tol.settleMs === undefined
    ? `bit-exact within ${fmtMs(tol.tolMs)}` +
      (tol.residDb === -Infinity ? ' (no residual at all)' : `, residual at most ${tol.residDb} dBFS`)
    : `residual below ${tol.settleDb} dBFS within ${fmtMs(tol.settleMs)} (NOT bit-exact: see below)`
}

// ── rendering ────────────────────────────────────────────────────────────────
// A script is written per render. The two scripts share their whole prelude, so the only difference
// between the two runs is the one stage's toggle. `press`/`release` on the keyboard path:
// `down <frame> <key>` + `up <frame+1> <key>` is one keyp().
function scriptFor (rack, key, taps) {
  const lines = ['# generated by tools/bypass-check.js — do not commit']
  let f = 2
  for (const k of rack.allOff) { lines.push(`down ${f} ${k}`, `up ${f + 1} ${k}`); f += 2 }
  for (const t of taps) lines.push(`down ${t} ${key}`, `up ${t + 1} ${key}`)
  return lines.join('\n') + '\n'
}

// The four scripts per stage, and which two of them each direction compares. The KEY property, and
// the reason the taps are laid out this way: within a direction, the reference and the variant tap
// the same key at the same frames right up to the moment under test, so the only thing left that can
// differ is the thing being measured.
//
//   OUT   ref = never in                 var = IN at t0,  OUT at t1        measured from t1
//   IN    ref = IN at t0, stays in       var = IN at t0, OUT at t1, IN t2   measured from t2
//
// (`allOff` runs in every one of them, so all four also share one identical boot transient.)
function planFor (dir, t0, t1, t2) {
  return dir === 'out'
    ? { refTaps: [],   varTaps: [t0, t1],     from: t0, at: t1 }
    : { refTaps: [t0], varTaps: [t0, t1, t2], from: t1, at: t2 }
}

// `fp` is the engine fingerprint taken before the first render. It is re-checked after EVERY render
// rather than only at the end: a parallel agent's patch-and-restore probe (ab-render.js does exactly
// that) flips runtime/sound.h back and forth, and a whole 70-second sweep whose output is meaningless
// is a worse answer than an abort on render 2 that names the cause.
function render (rack, scriptText, wav, frames, dir, tag, verbose, fp) {
  const sf = path.join(dir, `${tag}.script`)
  fs.writeFileSync(sf, scriptText)
  const args = [path.join(ROOT, 'tools/play.js'), rack.cart, 'script', sf,
                '--headless', '--frames', String(frames), '--seed', '1', '--wav', wav]
  if (verbose) console.log('  $ node ' + args.map(a => path.relative(ROOT, a) || a).join(' '))
  const r = spawnSync('node', args, { cwd: ROOT, encoding: 'utf8' })
  if (r.status !== 0) {
    console.error(r.stdout || '')
    console.error(r.stderr || '')
    throw new Error(`play.js failed for ${tag} (exit ${r.status})`)
  }
  if (!fs.existsSync(wav)) throw new Error(`no WAV written for ${tag}`)
  if (fp && engineFingerprint(rack.cart) !== fp) throw new Error(engineMovedMsg(fp, tag))
  return wav
}

function engineMovedMsg (fp, tag) {
  return 'THE ENGINE MOVED, not the bypass.\n' +
    `  One of ${ENGINE_FILES.join(', ')} or the cart changed while\n` +
    `  render "${tag}" was running (fingerprint was ${fp}), so the renders being compared came from\n` +
    '  two different builds and every number from them would be meaningless. A parallel agent editing\n' +
    '  runtime/ does this, and so does any patch-and-restore probe (ab-render.js). Rerun on a quiet tree.'
}

// ── selfcheck: known answers for the ANALYSER, built by hand ─────────────────
// A gate nobody has seen go red is indistinguishable from one gone blind (gate-controls.js), and
// this one's red is the interesting half: three of its four outcomes are failures. Every fixture is
// a WAV PAIR whose reconvergence point was CONSTRUCTED, so the answer is known before the tool runs.
// It renders nothing and needs no engine.
function fixtures () {
  const sr = SR, ch = 2, frames = 300
  const n = frames * SPF * ch
  const IN = 60, SW = 180                       // the stage is in over frames 60..180
  const iIn = IN * SPF * ch, iSw = SW * SPF * ch
  const tone = i => Math.round(Math.sin(2 * Math.PI * 220 * (i / ch) / sr) * 9000)

  // the shared baseline: the same programme in both renders
  const mk = () => { const a = new Int16Array(n); for (let i = 0; i < n; i++) a[i] = tone(i); return a }

  const cases = []
  const add = (label, mutate, want, opts = {}) => {
    const base = mk(), stg = mk()
    mutate(stg, base)
    cases.push({ label, base, stg, want, tolMs: opts.tolMs ?? 0, floorDb: opts.floorDb ?? -60,
                 // default 0 dBFS = "a residual of any level is allowed, judge it on TIME alone",
                 // so each fixture tests one thing. The ceiling gets its own cases below.
                 tol: opts.tol ?? { tolMs: opts.tolMs ?? 0, residDb: opts.tolResidDb ?? 0 },
                 inFrame: opts.inFrame ?? IN, switchFrame: opts.switchFrame ?? SW })
  }
  // a plain audible offset while the stage is in — what a working stage looks like
  const inWindow = (a, amp = 3000) => { for (let i = iIn; i < iSw; i++) a[i] += (i & 1) ? amp : -amp }

  // 1. the clean bypass: differs while in, byte-identical from the switching sample on
  add('clean bypass: last diff = the sample before the switch', a => inWindow(a), 'PASS')
  // 2. a tail: the residual decays away after the switch
  add('a 300 ms decaying tail after the switch, tol 0', a => {
    inWindow(a)
    for (let i = iSw; i < iSw + Math.round(0.300 * sr) * ch; i++) {
      const k = (i - iSw) / (Math.round(0.300 * sr) * ch)
      a[i] += Math.round((1 - k) * 3000) * ((i & 1) ? 1 : -1)
    }
  }, 'FAIL')
  // 2b. the SAME pair, judged with a window that allows it — the tolerance-is-a-time case
  add('the same 300 ms tail, tol 500 ms (the PLATE case)', a => {
    inWindow(a)
    for (let i = iSw; i < iSw + Math.round(0.300 * sr) * ch; i++) {
      const k = (i - iSw) / (Math.round(0.300 * sr) * ch)
      a[i] += Math.round((1 - k) * 3000) * ((i & 1) ? 1 : -1)
    }
  }, 'PASS', { tolMs: 500 })
  // 3. the fx_order bug shape: a 1.2 s trailing divergence, ending WELL inside the file so the
  //    verdict is about the bypass and not about the render length
  add('a 1.2 s trailing divergence that ends inside the file', a => {
    inWindow(a)
    for (let i = iSw; i < iSw + Math.round(1.2 * sr) * ch; i++) a[i] += (i & 1) ? 900 : -900
  }, 'FAIL', { tolMs: 500 })
  // 3b. THE OTHER VACUOUS PASS: still differing when the render stops. "Last differing sample" is
  //     then the last sample of the file, and reporting it as a reconvergence reports --frames.
  //     This is the shape the PLATE actually produced on this gate's first real run.
  add('still differing when the render ENDS (the number is the render length)', a => {
    inWindow(a)
    for (let i = iSw; i < n; i++) a[i] += (i & 1) ? 900 : -900
  }, 'INCONCLUSIVE', { tolMs: 6000 })
  // 4. THE VACUOUS PASS. Byte-identical everywhere reads as a perfect bypass to any naive
  //    "no difference after the switch" test. It must not pass.
  add('byte-identical everywhere (the toggle never reached the DSP)', () => {}, 'INCONCLUSIVE')
  // 5. differs only AFTER the switch: something moved, but not this stage being in
  add('differs only AFTER the switch (measuring the wrong thing)', a => {
    for (let i = iSw; i < iSw + 20000; i++) a[i] += (i & 1) ? 3000 : -3000
  }, 'INCONCLUSIVE')
  // 6. a ±1 LSB wobble while in: technically different, nowhere near audible
  add('a 1-LSB wobble while IN (below the audible floor)', a => {
    for (let i = iIn; i < iSw; i++) a[i] += (i & 1) ? 1 : -1
  }, 'INCONCLUSIVE')
  // 7. a difference in only 0.2% of the in-window samples: present, but the stage is inert
  add('an audible blip in 0.2% of the in-window (the stage is inert)', a => {
    const span = iSw - iIn, k = Math.floor(span * 0.002)
    for (let i = iIn; i < iIn + k; i++) a[i] += (i & 1) ? 3000 : -3000
  }, 'INCONCLUSIVE')
  // 8. ONE interleaved sample late, tol 0 — the boundary must be sharp, not sloppy
  add('reconverges ONE sample late, tol 0 (the boundary is sharp)', a => {
    inWindow(a); a[iSw] += 3000
  }, 'FAIL')
  // 9. the prelude differs: the two runs are not the same run (a harness bug, not a finding)
  add('the renders differ BEFORE the stage went in (harness bug)', a => {
    inWindow(a); a[100] += 3000
  }, 'ERROR')
  // 9b. THE RESIDUAL CEILING. Same 5 ms window either way; what separates them is HOW LOUD the
  //     residual is. A 1-LSB rounding tail is allowed, a -30 dBFS one is a stage still leaking.
  const shortTail = (amp) => (a) => {
    inWindow(a)
    for (let i = iSw; i < iSw + Math.round(0.005 * sr) * ch; i++) a[i] += (i & 1) ? amp : -amp
  }
  add('a 1-LSB residual inside the window, ceiling -85 dBFS', shortTail(1), 'PASS',
      { tolMs: 25, tolResidDb: -85 })
  add('a -30 dBFS residual inside the window, ceiling -85 dBFS (still leaking)', shortTail(1000), 'FAIL',
      { tolMs: 25, tolResidDb: -85 })
  add('the same -30 dBFS residual with NO ceiling set: judged on time, so it passes', shortTail(1000), 'PASS',
      { tolMs: 25, tolResidDb: 0 })
  // 9c. a LOUD residual that also runs off the end of the file. Both guards match; the CEILING one
  //     must win, because "rerun it longer" is the wrong filing for a stage that is plainly leaking.
  add('a loud residual running to the END of the file (ceiling wins over "rerun longer")', a => {
    inWindow(a)
    for (let i = iSw; i < n; i++) a[i] += (i & 1) ? 900 : -900
  }, 'FAIL', { tolMs: 6000, tolResidDb: -85 })
  // 9d. THE SETTLE CRITERION. Same pair judged three ways: bit-exact (fails, it has a long tail),
  //     settles below -60 dBFS in time (passes), settles below -80 dBFS in that time (fails, it is
  //     still above -80 dB then). A criterion that cannot distinguish those two floors is not a
  //     criterion, and the two rows differ ONLY in the floor.
  //     The tail: 3000 counts down to 1 over 800 ms, so it crosses -60 dBFS (33 counts) at ~590 ms
  //     and -80 dBFS (3 counts) at ~760 ms.
  const longTail = (a) => {
    inWindow(a)
    const len = Math.round(0.800 * sr) * ch
    for (let i = iSw; i < iSw + len; i++) {
      const k = (i - iSw) / len
      a[i] += Math.max(1, Math.round(3000 * Math.pow(0.001, k))) * ((i & 1) ? 1 : -1)
    }
  }
  add('an 800 ms decaying tail, judged BIT-EXACT within 100 ms', longTail, 'FAIL', { tolMs: 100 })
  add('the same tail, judged SETTLES below -60 dBFS within 700 ms',
      longTail, 'PASS', { tol: { settleMs: 700, settleDb: -60 } })
  add('the same tail, judged SETTLES below -80 dBFS within 700 ms (it does not)',
      longTail, 'FAIL', { tol: { settleMs: 700, settleDb: -80 } })
  // 9e. a SETTLE row whose residual is still above the floor when the render stops must be
  //     INCONCLUSIVE, exactly like the strict row's ran-out-of-file guard. Same trap, other branch.
  add('a SETTLE row still above its floor when the render ENDS', a => {
    inWindow(a)
    for (let i = iSw; i < n; i++) a[i] += (i & 1) ? 900 : -900
  }, 'INCONCLUSIVE', { tol: { settleMs: 6000, settleDb: -60 } })
  // 9f. a SETTLE row whose residual NEVER reaches the floor at all: nothing to wait for, so it
  //     passes at once. Without this the `ever: false` branch is untested and would read as 0 ms
  //     either way, which is the same number a dead detector prints.
  add('a SETTLE row whose residual never reaches the floor (passes at once)', a => {
    inWindow(a)
    for (let i = iSw; i < iSw + 20000; i++) a[i] += (i & 1) ? 2 : -2
  }, 'PASS', { tol: { settleMs: 1, settleDb: -60 } })
  // 10. exactly at the tolerance boundary: 100 ms tail, 100 ms window
  add('a tail exactly at the allowed window (must PASS, not fail by a rounding)', a => {
    inWindow(a)
    const last = iSw + Math.round(0.100 * sr) * ch
    for (let i = iSw; i <= last; i++) a[i] += (i & 1) ? 2000 : -2000
  }, 'PASS', { tolMs: 100.1 })
  return { cases, sr, ch, n }
}

function selfcheck (measure) {
  const { cases, sr, ch } = fixtures()
  console.log('bypass-check --selfcheck  (synthetic WAV pairs with CONSTRUCTED reconvergence points)\n')
  let fail = 0
  for (const c of cases) {
    const base = { sr, ch, s: c.base, n: c.base.length }
    const stg = { sr, ch, s: c.stg, n: c.stg.length }
    let got, a = null
    try {
      a = analyse(base, stg, c.inFrame, c.switchFrame)
      got = judge(a, c.tol, c.floorDb).verdict
    } catch (e) { got = 'ERROR'; a = null }
    // the prelude case is reported by judge(), not thrown — both spellings land on ERROR
    if (a) { const j = judge(a, c.tol, c.floorDb); got = j.verdict }
    const ok = got === c.want
    if (!ok) fail++
    console.log(`  ${ok ? '✓' : '✗'} ${c.label.padEnd(62)} ${got.padEnd(13)}` +
                (ok ? '' : `← WANT ${c.want}`))
    if (measure && a) console.log(`        in: ${(a.inDiffFrac * 100).toFixed(2)}% @ ${fmtDb(a.inPeakDb)} dBFS  ` +
      `last diff ${a.lastDiffS === null ? 'none' : a.lastDiffS.toFixed(4) + 's'}  ` +
      `reconverge ${fmtMs(a.reconvergeMs)}  post ${a.postCount} samples`)
  }
  // the analyser's own arithmetic, checked against numbers computed by hand rather than by itself
  console.log('')
  {
    const { sr, ch } = fixtures()
    const n = 300 * SPF * ch
    const b = new Int16Array(n), s = new Int16Array(n)
    for (let i = 0; i < n; i++) { b[i] = 1000; s[i] = 1000 }
    const iSw = 180 * SPF * ch
    s[iSw + 441 * ch] = 4000            // exactly 10 ms after the switch (441 samples @ 44.1k)
    for (let i = 60 * SPF * ch; i < iSw; i++) s[i] = 5000
    const a = analyse({ sr, ch, s: b, n }, { sr, ch, s, n }, 60, 180)
    const checks = [
      // 441 samples past the switch, plus the switching sample itself = 442/44100 s
      ['reconvergence delay reads 10.023 ms', Math.abs(a.reconvergeMs - 442 / 44100 * 1000) < 1e-9, a.reconvergeMs.toFixed(4)],
      ['the in-window difference is 100% of samples', Math.abs(a.inDiffFrac - 1) < 1e-9, a.inDiffFrac.toFixed(4)],
      ['peak in-window diff of 4000 reads -18.29 dBFS', Math.abs(a.inPeakDb - (20 * Math.log10(4000 / 32768))) < 1e-9, fmtDb(a.inPeakDb)],
      ['exactly 1 sample differs after the switch', a.postCount === 1, String(a.postCount)],
      ['duration of a 300-frame render reads 5.000 s', Math.abs(a.durS - 5) < 1e-9, a.durS.toFixed(4)],
    ]
    for (const [label, ok, got] of checks) {
      if (!ok) fail++
      console.log(`  ${ok ? '✓' : '✗'} ${label.padEnd(62)} ${got}`)
    }
  }
  // and the guards that must THROW rather than quietly compare a prefix
  console.log('')
  for (const [label, mut] of [
    ['a length mismatch is an ERROR, not a "reconvergence"', o => { o.n -= 2; o.s = o.s.slice(0, o.n) }],
    ['a sample-rate mismatch is an ERROR', o => { o.sr = 48000 }],
    ['a channel-count mismatch is an ERROR', o => { o.ch = 1 }],
  ]) {
    const n = 600 * ch
    const b = { sr, ch, s: new Int16Array(n), n }
    const s = { sr, ch, s: new Int16Array(n), n }
    mut(s)
    let threw = false
    try { analyse(b, s, 0, 10) } catch { threw = true }
    if (!threw) fail++
    console.log(`  ${threw ? '✓' : '✗'} ${label}`)
  }
  console.log(fail ? `\n✗ ${fail} self-test failure(s) — do NOT trust this tool's output`
                   : '\n✓ self-test clean')
  return fail ? 1 : 0
}

// ── CLI ──────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2)
const has = f => argv.includes(f)
const opt = (f, d) => { const i = argv.indexOf(f); return i >= 0 && i + 1 < argv.length ? argv[i + 1] : d }

if (has('--selfcheck') || has('--check')) process.exit(selfcheck(has('--measure')))

const quiet = has('--quiet')
const asJson = has('--json')
const keep = has('--keep')
const verbose = has('--verbose')
const frames = parseInt(opt('--frames', '720'), 10)
const t1 = parseInt(opt('--switch-at', '90'), 10)      // the stage goes OUT here
const t0 = parseInt(opt('--in-at', '20'), 10)          // …and came IN here
const gap = parseInt(opt('--gap', '45'), 10)           // …and comes back IN `gap` frames later
const t2 = t1 + gap
const floorDb = parseFloat(opt('--floor', '-60'))
const only = opt('--stage', null)
const rackName = opt('--rack', 'outboard')
const dirArg = (opt('--direction', 'both') || '').toLowerCase()
const detControl = !has('--no-det-control')

const DIR_LABEL = { out: 'OUT (switched out, against a run it was never in)',
                    in:  'IN  (switched back in, against a run it never left)' }
if (!['out', 'in', 'both'].includes(dirArg)) {
  console.error(`bypass-check: --direction must be out | in | both (got "${dirArg}")`)
  process.exit(2)
}
const dirs = dirArg === 'both' ? ['out', 'in'] : [dirArg]

const rack = RACKS[rackName]
if (!rack) {
  console.error(`bypass-check: no rack "${rackName}" (have: ${Object.keys(RACKS).join(', ')})`)
  process.exit(2)
}
if (!(t0 > 4 && t1 > t0 + 10 && gap > 10 && frames > t2 + 30)) {
  console.error('bypass-check: need 4 < --in-at < --switch-at, --gap > 10, and --frames past')
  console.error('  --switch-at + --gap with room for the tail. Got ' +
                `in-at ${t0}, switch-at ${t1}, gap ${gap}, frames ${frames}`)
  process.exit(2)
}
const stages = rack.stages.filter(s => !only || s.name.toUpperCase() === only.toUpperCase())
if (!stages.length) {
  console.error(`bypass-check: no stage "${only}" in rack ${rackName} ` +
                `(have: ${rack.stages.map(s => s.name).join(', ')})`)
  process.exit(2)
}

const tmp = keep ? fs.mkdtempSync(path.join(ROOT, 'build', 'bypass-'))
                 : fs.mkdtempSync(path.join(os.tmpdir(), 'bypass-'))
let exit = 0
const out = []
// One decoded render per (stage, taps) shape, cached: the OUT reference is the SAME run for every
// stage, so four stages in two directions is 9 renders, not 16.
const cache = new Map()

try {
  if (!quiet) {
    console.log(`bypass-check: ${rack.cart} — ${frames} frames (${(frames / FPS).toFixed(2)} s), ` +
      `stage IN at ${t0} (${(t0 / FPS).toFixed(3)} s), OUT at ${t1} (${(t1 / FPS).toFixed(3)} s), ` +
      `back IN at ${t2} (${(t2 / FPS).toFixed(3)} s)`)
    console.log(`  directions: ${dirs.join(' + ')}\n`)
  }

  const fp0 = engineFingerprint(rack.cart)

  // THE DETERMINISM CONTROL, first, because it decides whether anything below means anything.
  // Identical script twice: identical bytes, or the comparison is measuring the engine wobbling.
  // It uses the OUT baseline's script so the render is one the run needs anyway.
  const outRefScript = scriptFor(rack, rack.stages[0].key, [])
  const ctlA = readWavPcm(render(rack, outRefScript, path.join(tmp, 'control-a.wav'),
                                 frames, tmp, 'control-a', verbose, fp0))
  if (detControl) {
    const ctlB = readWavPcm(render(rack, outRefScript, path.join(tmp, 'control-b.wav'),
                                   frames, tmp, 'control-b', verbose, fp0))
    let same = ctlA.n === ctlB.n
    if (same) for (let i = 0; i < ctlA.n; i++) if (ctlA.s[i] !== ctlB.s[i]) { same = false; break }
    if (!same) {
      console.error('✗ CONTROL FAILED: two renders of the IDENTICAL script differ. The run is not')
      console.error('  deterministic, so no reconvergence number from it means anything. Check that')
      console.error('  --seed is passed and the cart reads no wall-clock/RNG outside de_state().')
      exit = 1
      throw new Error('determinism control failed')
    }
    if (!quiet) console.log('  control: two renders of the identical script are byte-identical ✓\n')
  }

  for (const st of stages) {
    for (const dir of dirs) {
      const plan = planFor(dir, t0, t1, t2)
      const tol = st[dir]
      // the reference: "never in" for OUT (the taps list is empty, so it is the control render we
      // already have) and "in and left in" for IN
      let ref
      if (dir === 'out') ref = ctlA
      else {
        const key = `ref-in-${st.key}`
        if (!cache.has(key))
          cache.set(key, readWavPcm(render(rack, scriptFor(rack, st.key, plan.refTaps),
            path.join(tmp, `ref-in-${st.name.toLowerCase()}.wav`), frames, tmp,
            `ref-in-${st.name}`, verbose, fp0)))
        ref = cache.get(key)
      }
      const varKey = `var-${dir}-${st.key}`
      if (!cache.has(varKey))
        cache.set(varKey, readWavPcm(render(rack, scriptFor(rack, st.key, plan.varTaps),
          path.join(tmp, `${dir}-${st.name.toLowerCase()}.wav`), frames, tmp,
          `${dir}-${st.name}`, verbose, fp0)))
      const variant = cache.get(varKey)

      const a = analyse(ref, variant, plan.from, plan.at)
      const j = judge(a, tol, floorDb)
      if (j.verdict !== 'PASS') exit = 1
      out.push({ stage: st.name, direction: dir, criterion: criterionOf(tol), ...tol, ...a, ...j })
      const tag = `${st.name} ${dir.toUpperCase()}`
      if (quiet) {
        console.log(`${j.verdict.padEnd(12)} ${tag.padEnd(11)} bit-exact after ${fmtMs(a.reconvergeMs).padStart(11)} ` +
                    `· ${criterionOf(tol)}${j.reasons.length ? '  — ' + j.reasons[0] : ''}`)
      } else if (!asJson) {
        console.log(`${j.verdict === 'PASS' ? '✓' : '✗'} ${tag}   ${DIR_LABEL[dir]}`)
        console.log(`    held to      ${criterionOf(tol)}`)
        console.log(`    expected     ${tol.why}`)
        console.log(`    differing    ${(a.inDiffFrac * 100).toFixed(2)}% of samples over the ` +
                    `${a.inFromS.toFixed(3)}..${a.switchS.toFixed(3)} s window, ` +
                    `peak ${fmtDb(a.inPeakDb)} dBFS`)
        console.log(`    last diff    ${a.lastDiffS === null ? 'none' : a.lastDiffS.toFixed(4) + ' s'}` +
                    `   the switch under test is at ${a.switchS.toFixed(4)} s`)
        // never print a bit-exact figure that is really the render length without saying so
        console.log(`    bit-exact    ${a.tailRoomMs < 100 ? '>' : ''}${fmtMs(a.reconvergeMs)} after it   ` +
                    `${a.postCount} sample(s) differ after it` +
                    (a.tailRoomMs < 100 ? '   (still differing when the render ENDED, so this is a floor)' : ''))
        if (a.postCount) console.log(`    residual     peak ${fmtDb(a.postPeakDb)} dBFS; ` +
          a.decay.map(d => d.ever ? `below ${d.floorDb} dB after ${fmtMs(d.afterMs)}`
                                  : `never above ${d.floorDb} dB`).join(', ') +
          `   (${fmtMs(a.tailRoomMs)} of render left)`)
        console.log(`    ${j.verdict}${j.reasons.length ? ': ' + j.reasons.join('; ') : ''}\n`)
      }
    }
  }

  if (engineFingerprint(rack.cart) !== fp0) {   // belt and braces: the per-render check should have caught it
    console.error('\n⚠ ' + engineMovedMsg(fp0, 'the sweep'))
    exit = 2      // NOT process.exit(): that skips the finally below and leaks the temp renders
  }
  if (asJson) console.log(JSON.stringify(out, null, 2))
  if (keep) console.log(`\nrenders kept in ${path.relative(ROOT, tmp)}/`)
} catch (e) {
  if (e.message !== 'determinism control failed') { console.error('\n✗ ' + e.message); exit = 2 }
} finally {
  if (!keep) fs.rmSync(tmp, { recursive: true, force: true })
}
process.exit(exit)
