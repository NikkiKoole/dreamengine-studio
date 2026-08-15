#!/usr/bin/env node
// fx-check.js — are the bus EFFECTS stable at their extremes? tune-check.js gates pitch,
// level-check.js gates loudness; this gates EFFECT STABILITY. It renders tools/carts/fxcheck.c
// (a loud sustained chord driven into the master bus, one effect at a time at its DOCUMENTED
// EXTREME — max feedback / resonance / depth) and asserts each effect's output stays FINITE
// and BOUNDED: no collapse to silence (a NaN propagating through a feedback loop reads as
// sudden silence in the 16-bit render), no DC runaway, and it actually moves the signal off
// the DRY reference (a dead/unwired effect). The scary cases it hammers: echo feedback 1.1,
// flanger/phaser feedback ±0.95, filter resonance 0.99 — stability there rested on internal
// tanh guards asserted by COMMENT in sound.h; this asserts it by measurement.
//
//   node tools/fx-check.js            render + per-effect report (peak/rms/dc/clip vs DRY)
//   node tools/fx-check.js --save     render + WRITE the golden baseline (tools/fx-baseline.json)
//   node tools/fx-check.js --quiet    CI gate: exit 1 on instability OR > drift threshold vs baseline
//   node tools/fx-check.js --json     machine-readable
//   node tools/fx-check.js --keep     keep the rendered WAV/trace (build/.fx/)
//   node tools/fx-check.js --selfcheck  known answers for the MEASUREMENT (renders no cart)
//
// THE CONTROLS — every number here is read RELATIVE to something: the note windows the statistics
// are taken in, and the DRY reference each effect is compared against. Neither was checked, and
// both fail silently rather than loudly. Five now run before any verdict AND before --save (see
// controlCheck): a frame is exactly sr/60 samples · every window is the same length · the render
// outlasts the sweep · every roster entry actually rendered · and DRY is present, sounding, not
// clipping and DC-free. What they do NOT cover: whether an effect sounds right, and whether the
// accepted baseline was reasonable when it was blessed.
//
// This is a STABILITY gate, not a character gate — whether an effect SOUNDS right is still by
// ear. It's the DSP twin of build-all.js: it can't tell you the reverb is beautiful, only that
// it didn't blow up, go silent, or do nothing. Render is deterministic (--det).

const fs = require('fs')
const path = require('path')
const { spawnSync } = require('child_process')

const ROOT = path.resolve(__dirname, '..')
const BASELINE = path.join(ROOT, 'tools', 'fx-baseline.json')

// test index → label + the extreme it sets (must match fxcheck.c's fx_enable() switch)
const FX_NAMES = {
  0:  'DRY (reference)',
  1:  'reverb  size 1.0 (endless bright hall)',
  2:  'echo    fb 1.1 — FEEDBACK > 1.0 (runaway)',
  3:  'chorus  5Hz depth 1.0 wet 1.0',
  4:  'flanger fb +0.95 (max jet)',
  5:  'flanger fb -0.95 (through-zero)',
  6:  'tape    wow/flut/sat 1.0',
  7:  'wah_lfo resonance 1.0 (max quack)',
  8:  'crush   1-bit + heavy downsample',
  9:  'eq      +15 dB all bands',
  10: 'tremolo 20Hz square (hard chop)',
  11: 'phaser  fb 0.95, 8 stages',
  12: 'filter  BP cutoff 300 res 0.99 (self-osc)',
  // stacks — multiple effects chained via fx_order() (the compounding / ordering surface)
  13: 'STACK   drive→eq→crush→tape (lo-fi master)',
  14: 'STACK   flanger→phaser (two combs in series)',
  15: 'STACK   echo+reverb (two feedback tails)',
  16: 'STACK   drive→reverb (order A)',
  17: 'STACK   reverb→drive (order B, reversed)',
  18: 'STACK   kitchen sink (8-deep chain)',
  // appended after the stacks on purpose — inserting a case renumbers the committed baseline
  19: 'multiband 3 bands 1.0 + up 1.0 (OTT wall)',
}

// thresholds
const WARN_DB = 1.5, BAD_DB = 4.0    // regression drift vs baseline (dB on peak or rms)
const SILENT_DBFS = -55              // gated window quieter than this = NaN-collapse / dead bus
const DC_LIMIT    = 0.03             // |mean| above this = a DC runaway (an effect leaking offset)
const CLIP_WARN   = 0.5              // >50% of samples pinned near full-scale = limiter fully engaged
const NOOP_DB     = 0.1              // within this of DRY on BOTH peak and rms = effect not moving the signal
// the DRY reference's own DC, with no effect in the path. Measured -0.000167; the bound is ~30x
// that and still 60x tighter than DC_LIMIT, because this one is not judging an effect at all
const CONTROL_DRY_DC = 0.005

const dbfs = (x) => x <= 0 ? -Infinity : 20 * Math.log10(x)
const fmtDb = (d) => d === -Infinity ? '  -inf' : `${d >= 0 ? '+' : ''}${d.toFixed(1)}`.padStart(6)

// ── WAV (16-bit PCM, mono or stereo→mono) — same reader as level-check.js ────
function readWavMono(file) {
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
  const ch = fmt.ch, n = Math.floor(data.len / 2 / ch)
  const s = new Float64Array(n)
  for (let i = 0; i < n; i++) {
    if (ch === 1) s[i] = b.readInt16LE(data.off + i * 2) / 32768
    else s[i] = (b.readInt16LE(data.off + i * 2 * ch) + b.readInt16LE(data.off + i * 2 * ch + 2)) / 65536
  }
  return { sr: fmt.sr, s }
}

// helper: mean of s over [i0,i1)
// ⚠ A PLAIN mean, deliberately, and this was checked rather than assumed. dc-check.js had to move
// to a Hann-weighted mean because a rectangular window over a single low sine leaves a residual of
// A/(π·cycles) — which there was the same size as the threshold. That does NOT transfer here: this
// window is 40k samples of a dense CHORD, thousands of cycles of many frequencies, so the residual
// averages away. Measured across all 20 effects, plain-vs-Hann differs by at most 0.009 (the
// flanger→phaser stack) against a DC_LIMIT of 0.03, and the two largest readings stay large under
// both — they are real asymmetry from the drive stages, not an artifact of the window.
const meanOf = (s, i0, i1) => { let m = 0; for (let i = i0; i < i1; i++) m += s[i]; return i1 > i0 ? m / (i1 - i0) : 0 }

// peak + clip ratio over the full window; rms over the steady middle. DC is measured as the
// mean over the FULL sounding window (longest integration) AND over each half — a true DC
// offset is a persistent bias (both halves agree in sign), whereas a sub-sonic resonant
// oscillation (which a max-feedback comb/allpass produces) averages toward zero and disagrees
// between halves. `dcStable` separates the two so we don't flag a slow wobble as a DC runaway.
function measure(s, a, b) {
  let peak = 0, clipped = 0
  for (let i = a; i < b; i++) { const v = Math.abs(s[i]); if (v > peak) peak = v; if (v > 0.98) clipped++ }
  const span = b - a
  const m0 = Math.floor(a + span * 0.12), m1 = Math.floor(a + span * 0.88)
  let sum = 0, n = 0
  for (let i = m0; i < m1; i++) { sum += s[i] * s[i]; n++ }
  const rms = n ? Math.sqrt(sum / n) : 0
  const mid = Math.floor((a + b) / 2)
  const dc = meanOf(s, a, b), dcH1 = meanOf(s, a, mid), dcH2 = meanOf(s, mid, b)
  const dcStable = Math.sign(dcH1) === Math.sign(dcH2) && Math.min(Math.abs(dcH1), Math.abs(dcH2)) > DC_LIMIT * 0.5
  return { peakDb: dbfs(peak), rmsDb: dbfs(rms), dc, dcStable, clipRatio: span ? clipped / span : 0 }
}

// ── trace → fx windows (contract: watch "fx" = test index, "gate" = sounding) ─
function fxWindows(traceFile) {
  const lines = fs.readFileSync(traceFile, 'utf8').trim().split('\n')
  const wins = []
  let cur = null, lastFrame = 0
  for (const ln of lines) {
    let row; try { row = JSON.parse(ln) } catch { continue }
    if (row.vev !== undefined) continue   // skip voice-trace events (-DDE_TRACE): they share the trace JSONL but carry no gate/fx window info
    const w = row.w || {}
    const gate = +w.gate, fx = +w.fx, f = row.f
    lastFrame = f
    if (gate === 1 && fx >= 0) {
      if (cur && cur.fx === fx) cur.f1 = f
      else { if (cur) wins.push(cur); cur = { fx, f0: f, f1: f } }
    } else if (cur) { wins.push(cur); cur = null }
  }
  if (cur) wins.push(cur)
  return { wins, lastFrame }
}

function runPlay(cart, frames, wav, trace) {
  const r = spawnSync('node',
    [path.join('tools', 'play.js'), cart, 'run', '--headless', '--det',
     '--frames', String(frames), '--trace', trace, '--wav', wav],
    { cwd: ROOT, encoding: 'utf8' })
  if (r.status !== 0) {
    process.stderr.write((r.stdout || '') + (r.stderr || ''))
    throw new Error(`render failed (play.js ${cart})`)
  }
}

function renderSweep(keep) {
  const dir = path.join(ROOT, 'build', '.fx')
  fs.mkdirSync(dir, { recursive: true })
  const wav = path.join(dir, 'fx.wav'), trace = path.join(dir, 'fx.trace.jsonl')
  // DERIVED from the roster, not hardcoded. This said `19 * 84 + 200` under a comment counting 19
  // tests when FX_NAMES already held 20 — the slack absorbed it, so nothing showed. The same shape
  // in level-check.js did NOT get absorbed and silently dropped a note for weeks; a count that has
  // to be edited by hand every time a case is appended is the defect, whether or not it has bitten
  // yet. Control 3 below fails loudly if this is ever short again.
  const tests = Object.keys(FX_NAMES).length
  runPlay('fxcheck', tests * 84 + 200, wav, trace)   // each test: 56 sounding + 28 gap frames
  return { wav, trace, dir }
}

function analyzeRender(wav, trace) {
  const { sr, s } = readWavMono(wav)
  const { wins, lastFrame } = fxWindows(trace)
  if (!wins.length) throw new Error('no gated fx windows in trace — did the cart build?')
  const spf = s.length / (lastFrame + 1)
  const results = wins.map(w => {
    const m = measure(s, Math.floor(w.f0 * spf), Math.floor(w.f1 * spf))
    return { fx: w.fx, name: FX_NAMES[w.fx] || `fx ${w.fx}`,
      peakDb: +m.peakDb.toFixed(2), rmsDb: +m.rmsDb.toFixed(2),
      dc: +m.dc.toFixed(4), dcStable: m.dcStable, clipRatio: +m.clipRatio.toFixed(3) }
  })
  return { results, sr, meta: { sr, spf, wins, lastFrame } }
}

// ── the controls ─────────────────────────────────────────────────────────────
// This gate's whole report is read RELATIVE to two things: the note windows it measures in, and
// the DRY reference every effect is compared against. Neither was ever checked, and both fail
// SILENTLY — a window reading the wrong samples still prints confident dB, and a missing DRY makes
// the no-op check short-circuit to nothing rather than error. Returns [] when the measurement can
// be trusted; anything in the list voids the numbers above it rather than blaming an effect.
//
// The structural three are the same invariants level-check.js carries, for the same reason (both
// derive every window from one samples-per-frame number). The fourth and fifth are this gate's own.
function controlCheck(results, meta) {
  const bad = []
  if (meta) {
    const want = meta.sr / 60           // the render is frame-locked at 60fps
    if (Math.abs(meta.spf - want) > 0.5)
      bad.push(`${meta.spf.toFixed(1)} samples per frame, but a 60fps render at ${meta.sr}Hz is ${want}`)
    const lens = new Set(meta.wins.map(w => w.f1 - w.f0))
    if (lens.size > 1) bad.push(`fx windows are not all the same length (${[...lens].sort((a, b) => a - b).join(', ')} frames)`)
    const last = meta.wins[meta.wins.length - 1]
    const period = meta.wins.length > 1 ? meta.wins[1].f0 - meta.wins[0].f0 : 84
    if (meta.lastFrame - last.f1 < period)
      bad.push(`the render ends ${meta.lastFrame - last.f1} frames after the last effect (< one ${period}-frame period): the sweep is TRUNCATED`)
  }
  // (4) EVERY ROSTER ENTRY MUST HAVE RENDERED. FX_NAMES and fxcheck.c's fx_enable() switch are two
  // hand-maintained lists that must agree, and the file already warns that appending a case
  // renumbers the baseline. A test present in the roster but missing from the trace is not a
  // stability finding — it means the two lists have drifted, and every index after the gap is
  // labelled with the wrong effect's name.
  const seen = new Set(results.map(r => r.fx))
  const missing = Object.keys(FX_NAMES).map(Number).filter(i => !seen.has(i))
  if (missing.length) bad.push(`the roster lists fx ${missing.join(', ')} but the render produced no window for ${missing.length > 1 ? 'them' : 'it'}`)
  const extra = [...seen].filter(i => FX_NAMES[i] === undefined)
  if (extra.length) bad.push(`the render produced fx ${extra.join(', ')}, which the roster does not name`)
  // (5) DRY IS THE REFERENCE, so it gets held to what a reference must be. Every no-op verdict is
  // measured against it, and `dry &&` means a missing one skips that check without a word.
  const dry = results.find(r => r.fx === 0)
  if (!dry) bad.push('there is no DRY window, so nothing was comparing the effects to anything')
  else {
    if (dry.peakDb < SILENT_DBFS) bad.push(`DRY is silent (${dry.peakDb} dBFS) — the source never sounded, so no effect can be judged`)
    if (dry.clipRatio > 0.01) bad.push(`DRY is already clipping (${(dry.clipRatio * 100).toFixed(0)}%) before any effect is applied`)
    // no effect has been applied, so a DRY offset is the source or the measurement, never an fx
    if (Math.abs(dry.dc) > CONTROL_DRY_DC)
      bad.push(`DRY has a DC offset of ${dry.dc.toFixed(4)} with no effect in the path (measured: 0.0002)`)
  }
  return bad
}

// ── baseline ─────────────────────────────────────────────────────────────────
function loadBaseline() {
  if (!fs.existsSync(BASELINE)) return null
  const j = JSON.parse(fs.readFileSync(BASELINE, 'utf8'))
  const map = new Map()
  for (const n of j.fx) map.set(n.fx, n)
  return map
}
function saveBaseline(results) {
  // record the INTRINSIC severity (computed with no baseline) per effect, so a known issue at
  // an extreme (e.g. the phaser/echo DC at max feedback) is the ACCEPTED current state. --quiet
  // then flags only effects that got WORSE than this, or drifted past the dB threshold.
  const out = {
    note: 'golden effect stability from tools/carts/fxcheck.c — regenerate with `node tools/fx-check.js --save`. Records known issues at extremes as the accepted baseline; the gate catches regressions (got worse / drifted).',
    thresholdDb: { warn: WARN_DB, bad: BAD_DB },
    fx: results.map(r => ({ fx: r.fx, peakDb: r.peakDb, rmsDb: r.rmsDb, dc: r.dc, clipRatio: r.clipRatio, sev: r.sev })),
  }
  fs.writeFileSync(BASELINE, JSON.stringify(out, null, 2) + '\n')
  console.log(`✓ wrote baseline for ${results.length} effects → ${path.relative(ROOT, BASELINE)}`)
}

// ── verdicts ─────────────────────────────────────────────────────────────────
function assess(results, baseline) {
  const dry = results.find(r => r.fx === 0)
  for (const r of results) {
    r.issues = []; r.sev = 'ok'
    const bump = (sev) => { if (sev === 'bad' || (sev === 'warn' && r.sev === 'ok')) r.sev = sev }
    // absolute STABILITY checks (no baseline needed)
    if (r.peakDb < SILENT_DBFS) { r.issues.push('SILENT while driven (NaN-collapse / dead bus)'); bump('bad') }
    if (Math.abs(r.dc) > DC_LIMIT && r.dcStable) { r.issues.push(`DC runaway (persistent offset ${r.dc.toFixed(3)})`); bump('bad') }
    else if (Math.abs(r.dc) > DC_LIMIT) { r.issues.push(`sub-sonic wobble (mean ${r.dc.toFixed(3)}, not steady DC)`); bump('warn') }
    if (r.clipRatio > CLIP_WARN) { r.issues.push(`limiter pinned (${(r.clipRatio * 100).toFixed(0)}% full-scale)`); bump('warn') }
    // no-op: a non-dry effect indistinguishable from DRY = not wired / silently broken
    if (r.fx !== 0 && dry && Math.abs(r.peakDb - dry.peakDb) < NOOP_DB && Math.abs(r.rmsDb - dry.rmsDb) < NOOP_DB)
      { r.issues.push('no-op (≈ DRY — effect not moving the signal)'); bump('warn') }
    // regression vs baseline. r.sev (above) is the INTRINSIC state for display; r.regressed is
    // the gate signal — true only if this effect got WORSE than its accepted baseline, so the
    // known extreme-feedback DC stays green while a NEW break (or a drift) fails CI.
    const RANK = { ok: 0, warn: 1, bad: 2 }
    if (baseline) {
      const base = baseline.get(r.fx)
      if (!base) { r.issues.push('new (no baseline)'); r.regressed = r.sev === 'bad' }
      else {
        const dP = r.peakDb - base.peakDb, dR = r.rmsDb - base.rmsDb
        r.dPeak = +dP.toFixed(2); r.dRms = +dR.toFixed(2)
        const worst = Math.max(Math.abs(dP), Math.abs(dR))
        if (worst > BAD_DB)  { r.issues.push(`DRIFT peak ${fmtDb(dP).trim()} / rms ${fmtDb(dR).trim()} dB vs baseline`); bump('bad') }
        else if (worst > WARN_DB) { r.issues.push(`drift peak ${fmtDb(dP).trim()} / rms ${fmtDb(dR).trim()} dB vs baseline`); bump('warn') }
        const worsened = RANK[r.sev] > RANK[base.sev || 'ok']
        if (worsened) r.issues.push(`WORSE than baseline (${base.sev || 'ok'} → ${r.sev})`)
        r.regressed = worst > BAD_DB || worsened
      }
    }
  }
}

const mark = (sev) => sev === 'bad' ? '✗' : sev === 'warn' ? '⚠' : '·'

function printResults(results, sr, hasBaseline) {
  console.log(`fx stability sweep — ${results.length} effects @ ${sr}Hz`
    + `   (silent <${SILENT_DBFS} dBFS · dc >${DC_LIMIT} · clip >${CLIP_WARN * 100}%)`
    + `${hasBaseline ? `   (warn >${WARN_DB} dB drift, bad >${BAD_DB})` : '   ⚠ NO BASELINE — run --save'}\n`)
  for (const r of results) {
    const drift = r.dPeak === undefined ? '' : `   Δpk ${fmtDb(r.dPeak)} Δrms ${fmtDb(r.dRms)}`
    const note = r.issues.length ? `   ${r.issues.join('; ')}` : ''
    console.log(`  ${mark(r.sev)} ${r.name.padEnd(42)} peak ${fmtDb(r.peakDb)}  rms ${fmtDb(r.rmsDb)}  dc ${r.dc >= 0 ? '+' : ''}${r.dc.toFixed(3)}  clip ${(r.clipRatio * 100).toFixed(0).padStart(3)}%${drift}${note}`)
  }
  const flagged = results.filter(r => r.sev !== 'ok').sort((a, b) => (a.sev === b.sev ? 0 : a.sev === 'bad' ? -1 : 1))
  console.log()
  if (!flagged.length) console.log('✓ every effect is stable (finite, bounded, moves the signal)')
  else {
    console.log(`${flagged.length} effect(s) flagged (worst first):`)
    for (const r of flagged) console.log(`  ${mark(r.sev)} ${r.name}  —  ${r.issues.join('; ')}`)
  }
}

function run(opts) {
  const { wav, trace, dir } = renderSweep(opts.keep)
  const { results, sr, meta } = analyzeRender(wav, trace)
  if (!opts.keep) fs.rmSync(dir, { recursive: true, force: true })
  const baseline = opts.save ? null : loadBaseline()
  assess(results, baseline)   // with null baseline this computes the intrinsic sev to record
  // ⚠ the controls gate --save too. This baseline records each effect's ACCEPTED severity, so
  // blessing from a broken measurement does not merely store wrong numbers — it writes the fault
  // in as the accepted state, and every later run compares against it and agrees.
  const control = controlCheck(results, meta)
  if (opts.save) {
    if (control.length) {
      console.error('✗ REFUSING TO BLESS — the measurement is off, not the effects:')
      for (const c of control) console.error(`    ${c}`)
      return { results, control, bad: control.length }
    }
    saveBaseline(results); return { results, control: [], bad: 0 }
  }
  if (opts.json) console.log(JSON.stringify({ control, fx: results }, null, 2))
  else {
    printResults(results, sr, !!baseline)
    if (control.length) {
      console.error('\n✗ THE MEASUREMENT IS OFF, NOT THE EFFECTS:')
      for (const c of control) console.error(`    ${c}`)
      console.error('  Every reading above is suspect. Start with `node tools/fx-check.js --selfcheck`.')
    }
  }
  // gate: vs a baseline, fail on regressions only; with no baseline, fail on any intrinsic bad
  const bad = (baseline ? results.filter(r => r.regressed).length
                        : results.filter(r => r.sev === 'bad').length) + control.length
  return { results, control, bad }
}

// ── --selfcheck: KNOWN ANSWERS FOR THE MEASUREMENT ───────────────────────────
// Renders no cart. Every number is arithmetic or was measured on the real sweep before it was
// asserted here, and the suite is mutation-tested — see the guide's recipe, step 5.
function selfcheck() {
  const SR = 44100
  let pass = 0, fail = 0
  const ok = (name, cond, got) => {
    if (cond) { pass++; console.log(`  ✓ ${name}`) } else { fail++; console.log(`  ✗ ${name}   got: ${got}`) }
  }
  const gen = (secs, f) => { const n = Math.floor(SR * secs), x = new Float64Array(n)
    for (let i = 0; i < n; i++) x[i] = f(i, n); return x }
  const m = (x) => measure(x, 0, x.length)

  console.log('fx-check --selfcheck — known answers for the measurement (no cart is rendered)\n')

  console.log('THE STATISTICS')
  const s5 = m(gen(0.5, (i) => 0.5 * Math.sin(2 * Math.PI * 441 * i / SR)))
  ok('a 0.5-amplitude sine peaks at -6.02 dBFS', Math.abs(s5.peakDb + 6.0206) < 0.001, s5.peakDb)
  ok('  …and reports no clipping at half scale', s5.clipRatio === 0, s5.clipRatio)
  // PINNED, not fixed. A DC-free sine over a rectangular window leaves a residual bounded by
  // A/(π·cycles); this is 220.5 cycles of amplitude 0.5, so 0.000722 — and it measures 0.000722.
  // The first draft of this assertion demanded < 1e-6 and went red, which is how the bound got
  // checked rather than assumed. It is the same artifact that forced dc-check.js onto a Hann
  // window, and the reason it is tolerable HERE is visible in the arithmetic: it shrinks as 1/cycles,
  // and a real fx window is 40k samples of a dense chord against a DC_LIMIT of 0.03.
  const artifact = 0.5 / (Math.PI * 220.5)
  ok(`  …a DC-free sine still reads the window residual, ${artifact.toFixed(6)}, and no more`,
     Math.abs(Math.abs(s5.dc) - artifact) < 1e-5, s5.dc)
  ok('  …which is 40x under DC_LIMIT, so the plain mean is adequate here (dc-check is not)',
     artifact * 40 < DC_LIMIT, `${artifact} vs ${DC_LIMIT}`)
  const dcd = m(gen(0.5, (i) => 0.3 * Math.sin(2 * Math.PI * 441 * i / SR) + 0.2))
  ok('an injected +0.20 offset is measured as +0.20', Math.abs(dcd.dc - 0.2) < 0.001, dcd.dc)
  ok('  …and is judged STABLE (both halves agree in sign)', dcd.dcStable === true, dcd.dcStable)
  // a sub-sonic wobble averages toward zero and disagrees between halves — the distinction the
  // tool exists to make, since a max-feedback comb produces one and it is not a DC fault
  const wob = m(gen(0.5, (i, n) => 0.3 * Math.sin(2 * Math.PI * 441 * i / SR) + 0.25 * Math.sin(2 * Math.PI * i / n)))
  ok('a sub-sonic wobble is NOT stable DC, though its halves are large', wob.dcStable === false, wob.dcStable)
  const clip = m(gen(0.5, (i) => Math.max(-1, Math.min(1, 3.0 * Math.sin(2 * Math.PI * 441 * i / SR)))))
  ok('a hard-clipped sine reports most of its samples pinned', clip.clipRatio > 0.6, clip.clipRatio)
  ok('  …while a sine at half scale reports none', s5.clipRatio === 0, s5.clipRatio)
  const dead = m(gen(0.5, () => 0))
  ok('a collapsed (silent) bus reads -inf, which is what SILENT catches', dead.peakDb === -Infinity, dead.peakDb)

  console.log('\nTHE WINDOWS')
  const os = require('os')
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'fxcheck-selfcheck-'))
  let seq = 0
  const traceOf = (rows) => { const f = path.join(dir, `t${seq++}.jsonl`)
    fs.writeFileSync(f, rows.map(r => JSON.stringify(r)).join('\n') + '\n'); return f }
  const row = (f, fx, gate) => ({ f, w: { fx, gate } })
  const two = traceOf([row(0, 0, 1), row(1, 0, 1), row(2, 0, 0), row(3, 1, 1), row(4, 1, 1)])
  ok('a gap between two effects makes two windows', fxWindows(two).wins.length === 2, fxWindows(two).wins.length)
  ok('  …labelled with their own fx index', fxWindows(two).wins.map(w => w.fx).join(',') === '0,1',
     fxWindows(two).wins.map(w => w.fx).join(','))
  const vev = traceOf([row(0, 3, 1), { f: 1, vev: 'on', w: {} }, row(2, 3, 1)])
  ok('voice-trace (vev) rows are skipped, not read as a gate drop', fxWindows(vev).wins.length === 1,
     fxWindows(vev).wins.length)
  fs.rmSync(dir, { recursive: true, force: true })

  console.log('\nTHE STRUCTURAL CONTROLS')
  // the real sweep's geometry, measured: 20 windows, 55 frames gated, one every 84, render to 1795
  const N = Object.keys(FX_NAMES).length
  const sweepMeta = (o = {}) => {
    const n = o.count === undefined ? N : o.count
    const wins = [...Array(n)].map((_, i) => ({ fx: i, f0: i * 84, f1: i * 84 + (o.shortLast && i === n - 1 ? 20 : 55) }))
    return { sr: 44100, spf: 735, wins, lastFrame: o.lastFrame === undefined ? 1795 : o.lastFrame, ...o.over }
  }
  const fullSet = (o = {}) => [...Array(o.count === undefined ? N : o.count)].map((_, i) => ({
    fx: i, peakDb: i === 0 ? -5.4 : -2.0, rmsDb: i === 0 ? -13.6 : -8.0, dc: -0.0002, clipRatio: 0 }))
  const ctl = (rs, mt) => controlCheck(rs, mt)
  ok('the real sweep geometry passes every control', ctl(fullSet(), sweepMeta()).length === 0,
     ctl(fullSet(), sweepMeta()))
  ok('samples-per-frame off by 2% is caught',
     ctl(fullSet(), sweepMeta({ over: { spf: 749.7 } })).some(c => c.includes('samples per frame')), 'silent')
  ok('a window shorter than its neighbours is caught',
     ctl(fullSet(), sweepMeta({ shortLast: true })).some(c => c.includes('same length')), 'silent')
  ok('a render that stops right after the last effect is TRUNCATED',
     ctl(fullSet(), sweepMeta({ lastFrame: N * 84 - 40 })).some(c => c.includes('TRUNCATED')), 'silent')

  console.log('\nTHE ROSTER CONTROL — the one this gate needs and level-check cannot have')
  ok(`the roster and a full render agree (${N} effects)`, ctl(fullSet(), sweepMeta()).length === 0, 'flagged')
  ok('an effect in the roster with no window is caught (the two lists have drifted)',
     ctl(fullSet({ count: N - 1 }), sweepMeta({ count: N - 1 })).some(c => c.includes('roster lists fx')), 'silent')
  ok('  …and it names the missing index, because every label after a gap is then wrong',
     ctl(fullSet({ count: N - 1 }), sweepMeta({ count: N - 1 })).some(c => c.includes(String(N - 1))), 'unnamed')
  ok('a window the roster does not name is caught too',
     ctl(fullSet().concat([{ fx: 99, peakDb: -2, rmsDb: -8, dc: 0, clipRatio: 0 }]), sweepMeta())
       .some(c => c.includes('does not name')), 'silent')

  console.log('\nTHE DRY CONTROL — every no-op verdict is measured against it')
  const noDry = fullSet().filter(r => r.fx !== 0)
  ok('a sweep with NO dry window fails loudly instead of skipping the no-op check',
     ctl(noDry, null).some(c => c.includes('no DRY window')), 'silent')
  const withDry = (o) => ctl(fullSet().map(r => r.fx === 0 ? { ...r, ...o } : r), null)
  ok('a silent DRY is caught (the source never sounded)',
     withDry({ peakDb: -80 }).some(c => c.includes('DRY is silent')), 'silent')
  ok('a DRY that already clips is caught (before any effect is applied)',
     withDry({ clipRatio: 0.4 }).some(c => c.includes('already clipping')), 'silent')
  ok('a DRY with a DC offset is caught — with no effect in the path it cannot be an fx',
     withDry({ dc: 0.02 }).some(c => c.includes('DC offset')), 'silent')
  ok('  …while the real DRY reading (-0.000167) passes', withDry({ dc: -0.000167 }).length === 0, 'flagged')
  ok('the DRY bound is far tighter than the effect DC limit it sits beside',
     CONTROL_DRY_DC < DC_LIMIT / 5, `${CONTROL_DRY_DC} vs ${DC_LIMIT}`)

  console.log('\nTHE VERDICTS')
  const fx = (o) => ({ fx: 1, name: 'test', peakDb: -6, rmsDb: -12, dc: 0, dcStable: false, clipRatio: 0, ...o })
  const dryRow = fx({ fx: 0, peakDb: -5.4, rmsDb: -13.6 })
  const sev = (o, base) => { const rs = [dryRow, fx(o)]; assess(rs, base || null); return rs[1] }
  ok('a stable effect is ok', sev({}).sev === 'ok', sev({}).issues)
  ok('an effect that went silent while driven is bad',
     sev({ peakDb: -80 }).issues.some(i => i.includes('NaN-collapse')), 'silent')
  ok('a persistent DC offset is a runaway and is bad',
     sev({ dc: 0.05, dcStable: true }).sev === 'bad', sev({ dc: 0.05, dcStable: true }).issues)
  ok('  …but the same magnitude that is NOT steady is only a wobble warning',
     sev({ dc: 0.05, dcStable: false }).sev === 'warn', sev({ dc: 0.05, dcStable: false }).issues)
  ok('an effect pinned against the limiter warns',
     sev({ clipRatio: 0.7 }).issues.some(i => i.includes('limiter pinned')), 'silent')
  // the no-op branch has never fired on a real run — every shipped effect moves the signal
  ok('an effect indistinguishable from DRY is a no-op (never yet seen on a real run)',
     sev({ peakDb: -5.4, rmsDb: -13.6 }).issues.some(i => i.includes('no-op')), 'silent')
  ok('  …and 0.2 dB away from DRY is NOT a no-op',
     !sev({ peakDb: -5.6, rmsDb: -13.8 }).issues.some(i => i.includes('no-op')), 'flagged')

  console.log('\nTHE GATE SIGNAL — an accepted extreme stays green, a NEW break does not')
  const base = new Map([[1, { fx: 1, peakDb: -6, rmsDb: -12, dc: 0, clipRatio: 0, sev: 'warn' }]])
  ok('an effect at its blessed warn state does not regress',
     sev({ clipRatio: 0.7 }, base).regressed === false, sev({ clipRatio: 0.7 }, base).issues)
  ok('  …the same effect turning bad DOES regress',
     sev({ clipRatio: 0.7, dc: 0.05, dcStable: true }, base).regressed === true, 'not flagged')
  ok('a big level drift regresses even with no severity change',
     sev({ peakDb: -12 }, base).regressed === true, sev({ peakDb: -12 }, base).issues)
  ok('  …and a small one does not', sev({ peakDb: -7 }, base).regressed === false, sev({ peakDb: -7 }, base).issues)

  console.log(`\n${fail ? '✗' : '✓'} ${pass} passed, ${fail} failed`)
  return fail ? 1 : 0
}

// ── cli ────────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2)
const opts = {
  json: argv.includes('--json'), keep: argv.includes('--keep'),
  quiet: argv.includes('--quiet'), save: argv.includes('--save'),
}
if (argv.includes('--selfcheck')) process.exit(selfcheck())

try {
  const { bad } = run(opts)
  if (opts.quiet) process.exit(bad ? 1 : 0)
} catch (e) { console.error('fx-check:', e.message); process.exit(2) }
