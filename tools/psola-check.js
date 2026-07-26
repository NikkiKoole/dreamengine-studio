#!/usr/bin/env node
// psola-check.js — the ARTIFACT oracle for the PSOLA pitch engine (sample_autotune / sample_shift /
// autotune_mic / harmonize_mic). The "does it CLICK" gate, as opposed to formant-check.js's "is it in
// TUNE" gate. Born 2026-07-26 the hard way: every existing gate was green, the pitch numbers were
// exact, and the maker could still hear popping. Full story: docs/design/contemporary-rebirth.md
// §"Rung B, same-day postscript".
//
// WHY THREE DETECTORS. This is the whole point of the tool — each one is blind to a defect the others
// catch, and using any single one of them actively misleads. All three were established by finding a
// real bug the others scored as clean:
//
//   1. SPLICE (first difference).  A butt-joined grain boundary steps the waveform discontinuously.
//      Caught the down-shift zero-overlap bug (177 events, a step 95% of peak) and the zero-order-hold
//      staircase from a truncated fractional read. BLIND TO: a phase break that happens at equal
//      amplitude — it scored the snapped and up takes at exactly 0 while they were audibly glitching.
//
//   2. PERIODICITY (r[n] = x[n] - x[n-T], normalised, vs the RAW take as CONTROL).  A splice that
//      preserves amplitude still breaks period-to-period self-similarity. Ranked the +12 take WORST of
//      four — the take detector 1 called clean. BLIND TO: the staircase (it repeats identically every
//      period), and, far more dangerously, to PERIOD DOUBLING (see 3).
//
//   3. DOUBLING (f0 vs expected).  A period-doubled take — alternate pulses differing, from a source
//      epoch that alternates — is STILL PERFECTLY PERIODIC, so detector 2 scores it as an IMPROVEMENT.
//      Measured: a change that doubled the snapped take (f0 220.4 → 178.8, flipping 110–220) scored 2x
//      BETTER on periodicity while sounding worse. Only the f0 reading sees it. This detector exists
//      because trusting detector 2 alone shipped a regression.
//
// A metric that improves while the maker's report gets worse is measuring the wrong thing. Hence: the
// tool fails if ANY detector fails, and never averages them into one score.
//
// usage:
//   node tools/psola-check.js                 render voxshift + report all three detectors per take
//   node tools/psola-check.js --quiet         exit 1 if any take regresses vs the blessed baseline (CI)
//   node tools/psola-check.js --save          re-bless the current numbers as the baseline
//   node tools/psola-check.js --wav <f>       analyse an existing render instead of rendering
//
// Baseline lives in tools/psola-check.json (committed). RAW is the control in every comparison: it is
// the same captured voice UNPROCESSED, so "no worse than RAW" is the honest bar, not "zero".
//
// No deps. The source cart is voxshift, whose four takes (raw / snapped / +12 / -12 of ONE captured
// INSTR_VOICE take) are exactly the A/B any at_psola_slot edit needs, and which needs no mic and
// replays deterministically.

const fs = require('fs')
const path = require('path')
const { spawnSync } = require('child_process')

const argv = process.argv.slice(2)
const quiet = argv.includes('--quiet')
const save = argv.includes('--save')
const wavArg = argv.includes('--wav') ? argv[argv.indexOf('--wav') + 1] : null
const ROOT = path.resolve(__dirname, '..')
const BASELINE = path.join(__dirname, 'psola-check.json')

// voxshift auditions the four takes in order after a ~1.6s capture. Regions are stable because the
// cart is deterministic (record_arm/record_grab off its own INSTR_VOICE, no mic).
const TAKES = [
  { name: 'RAW',      from: 1.85, to: 3.00, f0: 220.5, control: true },
  { name: 'SNAPPED',  from: 3.35, to: 4.50, f0: 220.4 },
  { name: 'UP+12',    from: 4.85, to: 6.00, f0: 440.5 },
  { name: 'DOWN-12',  from: 6.35, to: 7.50, f0: 110.25 },
]

function readWav (p) {
  const b = fs.readFileSync(p)
  let q = 12, ch = 1, bits = 16, rate = 44100, dOff = 0, dLen = 0
  while (q < b.length) {
    const id = b.toString('ascii', q, q + 4), sz = b.readUInt32LE(q + 4)
    if (id === 'fmt ') { ch = b.readUInt16LE(q + 10); rate = b.readUInt32LE(q + 12); bits = b.readUInt16LE(q + 22) }
    else if (id === 'data') { dOff = q + 8; dLen = sz; break }
    q += 8 + sz + (sz & 1)
  }
  const bytes = bits / 8, frames = Math.floor(dLen / (bytes * ch))
  const x = new Float32Array(frames)
  for (let i = 0; i < frames; i++) {
    let s = 0
    for (let c = 0; c < ch; c++) {
      const o = dOff + (i * ch + c) * bytes
      s += bits === 16 ? b.readInt16LE(o) / 32768 : b.readFloatLE(o)
    }
    x[i] = s / ch
  }
  return { x, rate }
}

function render () {
  const out = path.join(ROOT, 'build', 'psola-check.wav')
  const r = spawnSync('node',
    [path.join('tools', 'play.js'), 'voxshift', 'script', '/dev/null',
      '--headless', '--frames', '600', '--wav', out],
    { cwd: ROOT, encoding: 'utf8' })
  if (r.status !== 0 || !fs.existsSync(out)) {
    console.error('render failed (play.js voxshift)\n' + (r.stderr || r.stdout || ''))
    process.exit(2)
  }
  return out
}

// ── shared: local period by normalised autocorrelation ──────────────────────────────────────────
function periodAt (x, a, b, rate) {
  const lo = Math.floor(rate / 500), hi = Math.floor(rate / 70)
  let best = -1, bl = lo
  for (let lag = lo; lag <= hi; lag++) {
    let num = 0, d1 = 0, d2 = 0
    for (let i = a; i + lag < b; i++) { num += x[i] * x[i + lag]; d1 += x[i] * x[i]; d2 += x[i + lag] * x[i + lag] }
    const c = num / (Math.sqrt(d1 * d2) + 1e-12)
    if (c > best) { best = c; bl = lag }
  }
  return { T: bl, conf: best }
}

// ── detector 1: SPLICE — first-difference steps far above the local slope ────────────────────────
function splice (x, a, b, rate) {
  const d = []
  for (let i = a + 1; i < b; i++) d.push(Math.abs(x[i] - x[i - 1]))
  const sorted = [...d].sort((p, q) => p - q)
  const med = sorted[Math.floor(sorted.length / 2)] || 1e-9
  let count = 0, worst = 0, last = -1e9
  for (let i = 0; i < d.length; i++) {
    if (d[i] > med * 8 && d[i] > 0.01) {            // 8x the median slope AND >1% full scale
      if (i - last > rate * 0.002) count++          // debounce 2ms: one pop counts once
      last = i
      if (d[i] > worst) worst = d[i]
    }
  }
  return { count, worst: +worst.toFixed(4) }
}

// ── detector 2: PERIODICITY — deviation from the take's OWN previous period ──────────────────────
function periodicity (x, a, b, rate) {
  const win = Math.round(rate * 0.03)
  const err = []
  for (let w = a; w + win < b; w += win) {
    const { T, conf } = periodAt(x, w, Math.min(w + win, b), rate)
    if (conf < 0.5) continue                        // unvoiced: the measure is meaningless
    let rms = 0
    for (let i = w; i < w + win && i < b; i++) rms += x[i] * x[i]
    rms = Math.sqrt(rms / win) + 1e-9
    for (let i = w; i < w + win && i < b; i++) {
      if (i - T < a) continue
      err.push(Math.abs(x[i] - x[i - T]) / rms)
    }
  }
  if (!err.length) return { p95: 0, worst: 0 }
  const s = [...err].sort((p, q) => p - q)
  return { p95: +(s[Math.floor(s.length * 0.95)]).toFixed(4), worst: +(s[s.length - 1]).toFixed(3) }
}

// ── detector 3: DOUBLING — f0 across sub-windows vs the expected note ───────────────────────────
// A period-doubled take reads at (or flips to) HALF the expected f0. Reported as the measured mean,
// the spread across sub-windows, and the ratio to expected — a ratio near 0.5 is doubling, and the
// spread catches a take that flips between the two (which is what a flip-flopping epoch choice does).
function doubling (x, a, b, rate, expect) {
  const n = 8, step = Math.floor((b - a) / n), f = []
  for (let k = 0; k < n; k++) {
    const s = a + k * step
    const { T, conf } = periodAt(x, s, s + step, rate)
    if (conf < 0.5) continue
    f.push(rate / T)
  }
  if (!f.length) return { f0: 0, spread: 0, ratio: 0 }
  const mean = f.reduce((p, q) => p + q, 0) / f.length
  return {
    f0: +mean.toFixed(1),
    spread: +(Math.max(...f) - Math.min(...f)).toFixed(1),
    ratio: +(mean / expect).toFixed(3),
  }
}

// ── run ─────────────────────────────────────────────────────────────────────────────────────────
const wav = wavArg || render()
const { x, rate } = readWav(wav)
const now = {}
for (const t of TAKES) {
  const a = Math.round(t.from * rate), b = Math.round(t.to * rate)
  if (b > x.length) { console.error(`region ${t.name} past end of ${wav}`); process.exit(2) }
  now[t.name] = {
    splice: splice(x, a, b, rate),
    periodicity: periodicity(x, a, b, rate),
    doubling: doubling(x, a, b, rate, t.f0),
  }
}

if (save) {
  fs.writeFileSync(BASELINE, JSON.stringify({ note: 'blessed psola-check baseline — see tools/psola-check.js', takes: now }, null, 2) + '\n')
  console.log(`blessed → ${path.relative(ROOT, BASELINE)}`)
  process.exit(0)
}

const base = fs.existsSync(BASELINE) ? JSON.parse(fs.readFileSync(BASELINE, 'utf8')).takes : null
const control = now.RAW
const fails = []

// Tolerances: splice/periodicity may not get materially WORSE than blessed; f0 must stay on its note.
for (const t of TAKES) {
  const c = now[t.name], b = base && base[t.name]
  if (c.doubling.ratio < 0.75 || c.doubling.ratio > 1.25) {
    fails.push(`${t.name}: f0 ${c.doubling.f0}Hz is ${c.doubling.ratio}x the expected ${t.f0}Hz` +
      (c.doubling.ratio < 0.75 ? ' — PERIOD DOUBLING' : ''))
  }
  if (c.doubling.spread > t.f0 * 0.25) {
    fails.push(`${t.name}: f0 spread ${c.doubling.spread}Hz across sub-windows (flipping — an alternating source epoch)`)
  }
  if (b) {
    if (c.splice.count > Math.max(b.splice.count * 2, b.splice.count + 8)) {
      fails.push(`${t.name}: ${c.splice.count} splices vs ${b.splice.count} blessed`)
    }
    if (c.periodicity.worst > b.periodicity.worst * 1.5 + 0.1) {
      fails.push(`${t.name}: periodicity worst ${c.periodicity.worst} vs ${b.periodicity.worst} blessed`)
    }
  }
}

if (!quiet) {
  console.log(`psola-check — ${path.relative(ROOT, wav)}\n`)
  console.log('take       SPLICE           PERIODICITY (vs RAW control)      DOUBLING')
  console.log('           n    worst       p95      worst    xRAW           f0        spread  xexpect')
  for (const t of TAKES) {
    const c = now[t.name]
    const xraw = control.periodicity.p95 ? (c.periodicity.p95 / control.periodicity.p95).toFixed(2) : '-'
    console.log(
      `${t.name.padEnd(10)} ${String(c.splice.count).padStart(4)} ${c.splice.worst.toFixed(4).padStart(7)}   ` +
      `${c.periodicity.p95.toFixed(4).padStart(7)} ${c.periodicity.worst.toFixed(3).padStart(8)} ${(xraw + 'x').padStart(8)}` +
      `        ${c.doubling.f0.toFixed(1).padStart(7)} ${c.doubling.spread.toFixed(1).padStart(7)} ${c.doubling.ratio.toFixed(3).padStart(8)}`
    )
  }
  console.log('\nRAW is the CONTROL (same voice, unprocessed) — the bar is "no worse than RAW", not zero.')
  if (!base) console.log('no baseline yet — run with --save to bless these numbers.')
}

if (fails.length) {
  console.error((quiet ? '' : '\n') + `psola-check FAIL (${fails.length}):`)
  for (const f of fails) console.error(`  ✗ ${f}`)
  process.exit(1)
}
if (!quiet) console.log('\n✓ no artifact regression')
process.exit(0)
