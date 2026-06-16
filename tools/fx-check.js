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
}

// thresholds
const WARN_DB = 1.5, BAD_DB = 4.0    // regression drift vs baseline (dB on peak or rms)
const SILENT_DBFS = -55              // gated window quieter than this = NaN-collapse / dead bus
const DC_LIMIT    = 0.03             // |mean| above this = a DC runaway (an effect leaking offset)
const CLIP_WARN   = 0.5              // >50% of samples pinned near full-scale = limiter fully engaged
const NOOP_DB     = 0.1              // within this of DRY on BOTH peak and rms = effect not moving the signal

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
  runPlay('fxcheck', 19 * 84 + 200, wav, trace)   // 19 tests × (56+28) frames + slack
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
  return { results, sr }
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
  const { results, sr } = analyzeRender(wav, trace)
  if (!opts.keep) fs.rmSync(dir, { recursive: true, force: true })
  const baseline = opts.save ? null : loadBaseline()
  assess(results, baseline)   // with null baseline this computes the intrinsic sev to record
  if (opts.save) { saveBaseline(results); return { results, bad: 0 } }
  if (opts.json) console.log(JSON.stringify(results, null, 2))
  else printResults(results, sr, !!baseline)
  // gate: vs a baseline, fail on regressions only; with no baseline, fail on any intrinsic bad
  const bad = baseline ? results.filter(r => r.regressed).length
                       : results.filter(r => r.sev === 'bad').length
  return { results, bad }
}

// ── cli ────────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2)
const opts = {
  json: argv.includes('--json'), keep: argv.includes('--keep'),
  quiet: argv.includes('--quiet'), save: argv.includes('--save'),
}
try {
  const { bad } = run(opts)
  if (opts.quiet) process.exit(bad ? 1 : 0)
} catch (e) { console.error('fx-check:', e.message); process.exit(2) }
