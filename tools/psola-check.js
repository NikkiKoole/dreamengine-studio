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
// ⚠ WHAT THIS READS ON HEALTHY AUDIO, measured and pinned in --selfcheck, so none of it reads as a
// fresh scare or (worse) as proof the detector is alive:
//   · SPLICE reads exactly `0 / 0.0000` on ALL FOUR takes of a good render. A live detector and a
//     DEAD one print the same four rows — which is the whole argument for the fixture, and it is
//     sharper here than in any sibling gate: the three detectors are deliberately blind to each
//     other's defects, so one of them dying is invisible BY CONSTRUCTION. The other two carry on
//     printing plausible numbers and the tool still says "no artifact regression".
//   · SPLICE's `count` is not a density measure. The 2 ms debounce advances on every trip, so an
//     artifact that is CONTINUOUS collapses to ONE event however long it runs (measured: a 0.8 s
//     zero-order-hold staircase counts 1, same as a single pop). `worst` is what separates them.
//     Real grain splices sit ~4.5 ms apart at these pitches, above the gate — which is how the
//     down-shift bug counted 177 rather than 1.
//   · PERIODICITY's `p95` stays at 0.0000 for a localised break; only `worst` moves. The verdict
//     compares `worst` for that reason.
//
// usage:
//   node tools/psola-check.js                 render voxshift + report all three detectors per take
//   node tools/psola-check.js --quiet         exit 1 if any take regresses vs the blessed baseline (CI)
//   node tools/psola-check.js --save          re-bless the current numbers as the baseline
//   node tools/psola-check.js --wav <f>       analyse an existing render instead of rendering
//   node tools/psola-check.js --selfcheck     known answers for the three DETECTORS, on audio it
//                                             synthesises (no cart, no engine, no WAV on disk)
//   node tools/psola-check.js --selfcheck --measure   dump what the probe signals actually read —
//                                             run this BEFORE changing any known answer below
//   node tools/psola-check.js --baseline <f>  use another golden file (the fixture's, mainly)
//
// THE BAR IS THE BLESSED BASELINE (tools/psola-check.json, committed), take by take. RAW is the same
// captured voice UNPROCESSED and it is in the sweep for two reasons: it is the row that shows the
// CAPTURE itself has not moved (if the reference drifts, every "no worse than blessed" below it is
// measured against a moved thing — fx-check's DRY lesson), and its `xRAW` column gives the processed
// numbers a scale. ⚠ It is NOT a threshold, and the header used to claim it was — "the bar is no
// worse than RAW". Nothing ever implemented that, and it is not what the numbers say: the processed
// takes measure 1.50x–1.92x RAW on a clean render. Held to it, this gate would be red today.
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
// --baseline exists so --selfcheck can exercise the save/refuse/missing paths for real without ever
// touching the committed golden file.
const BASELINE = argv.includes('--baseline')
  ? path.resolve(argv[argv.indexOf('--baseline') + 1])
  : path.join(__dirname, 'psola-check.json')

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
  return { count, worst: +worst.toFixed(4), n: d.length, med: +med.toFixed(6) }
}

// ── detector 2: PERIODICITY — deviation from the take's OWN previous period ──────────────────────
// `voiced`/`windows` are the LIVENESS pair, and they are not decoration: an unvoiced window is
// skipped, so a take that went silent produces NO errors and scores a perfect {p95:0, worst:0} —
// better than a clean one. The verdict reads `voiced`; without it this detector's best possible
// output and its "I measured nothing" output are the same two numbers. (Vacuity, checks-and-oracles.)
function periodicity (x, a, b, rate) {
  const win = Math.round(rate * 0.03)
  const err = []
  let windows = 0, voiced = 0
  for (let w = a; w + win < b; w += win) {
    windows++
    const { T, conf } = periodAt(x, w, Math.min(w + win, b), rate)
    if (conf < 0.5) continue                        // unvoiced: the measure is meaningless
    voiced++
    let rms = 0
    for (let i = w; i < w + win && i < b; i++) rms += x[i] * x[i]
    rms = Math.sqrt(rms / win) + 1e-9
    for (let i = w; i < w + win && i < b; i++) {
      if (i - T < a) continue
      err.push(Math.abs(x[i] - x[i - T]) / rms)
    }
  }
  if (!err.length) return { p95: 0, worst: 0, windows, voiced }
  const s = [...err].sort((p, q) => p - q)
  return {
    p95: +(s[Math.floor(s.length * 0.95)]).toFixed(4),
    worst: +(s[s.length - 1]).toFixed(3),
    windows, voiced,
  }
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
  if (!f.length) return { f0: 0, spread: 0, ratio: 0, windows: n, voiced: 0 }
  const mean = f.reduce((p, q) => p + q, 0) / f.length
  return {
    f0: +mean.toFixed(1),
    spread: +(Math.max(...f) - Math.min(...f)).toFixed(1),
    ratio: +(mean / expect).toFixed(3),
    windows: n, voiced: f.length,
  }
}

// ── the VERDICT, as one function ────────────────────────────────────────────────────────────────
// Extracted so --selfcheck exercises THE tolerance arithmetic rather than a copy of it. (canvas-diff
// shipped a fixture that re-implemented its own predicate locally; mutating the real one left every
// assertion green. One named function, called from both paths.)
//
// Three tiers, and the distinction matters:
//   LIVENESS — did the measurement happen at all? Absolute, needs no baseline. Everything else here
//              is phrased as "no bad thing was found", which is trivially true of a region nobody
//              looked at: an unvoiced take scores a PERFECT periodicity {p95:0, worst:0} and zero
//              splices. Only this tier can tell a clean take from no take.
//   ABSOLUTE — f0 must sit on its note. Needs no baseline, so it survives a missing one.
//   RELATIVE — splice/periodicity may not get materially worse than blessed.
function verdict (now, base, takes) {
  const fails = []
  for (const t of (takes || TAKES)) {
    const c = now[t.name], b = base && base[t.name]
    if (!c) { fails.push(`${t.name}: no measurement in this run`); continue }

    // LIVENESS. A region that is silent, unvoiced, or simply not where TAKES says it is measures
    // nothing and passes every other check below it.
    if (c.periodicity.voiced === 0) {
      fails.push(`${t.name}: periodicity measured 0 of ${c.periodicity.windows} windows — NOTHING WAS MEASURED (silent, unvoiced, or the region moved)`)
    } else if (c.periodicity.voiced < c.periodicity.windows * 0.5) {
      fails.push(`${t.name}: periodicity measured only ${c.periodicity.voiced} of ${c.periodicity.windows} windows — the take is mostly unvoiced`)
    }
    if (c.doubling.voiced === 0) {
      fails.push(`${t.name}: f0 unreadable in all ${c.doubling.windows} sub-windows — NOTHING WAS MEASURED`)
    }

    // ABSOLUTE.
    if (c.doubling.voiced > 0) {
      if (c.doubling.ratio < 0.75 || c.doubling.ratio > 1.25) {
        fails.push(`${t.name}: f0 ${c.doubling.f0}Hz is ${c.doubling.ratio}x the expected ${t.f0}Hz` +
          (c.doubling.ratio < 0.75 ? ' — PERIOD DOUBLING' : ''))
      }
      if (c.doubling.spread > t.f0 * 0.25) {
        fails.push(`${t.name}: f0 spread ${c.doubling.spread}Hz across sub-windows (flipping — an alternating source epoch)`)
      }
    }

    // RELATIVE.
    if (b) {
      if (c.splice.count > Math.max(b.splice.count * 2, b.splice.count + 8)) {
        fails.push(`${t.name}: ${c.splice.count} splices vs ${b.splice.count} blessed`)
      }
      if (c.periodicity.worst > b.periodicity.worst * 1.5 + 0.1) {
        fails.push(`${t.name}: periodicity worst ${c.periodicity.worst} vs ${b.periodicity.worst} blessed`)
      }
    }
  }
  return fails
}

function measureAll (x, rate, wav) {
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
  return now
}

// ── run ─────────────────────────────────────────────────────────────────────────────────────────
if (argv.includes('--selfcheck')) process.exit(selfcheck())

const wav = wavArg || render()
const { x, rate } = readWav(wav)
const now = measureAll(x, rate, wav)

const base = fs.existsSync(BASELINE) ? JSON.parse(fs.readFileSync(BASELINE, 'utf8')).takes : null

if (save) {
  // Refuse to bless while an ABSOLUTE check is red: a period-doubled take frozen into the golden
  // file stops looking like a fault, and every later run is then measured against the defect.
  // (level-check learned this the same way.)
  const bad = verdict(now, null)
  if (bad.length) {
    console.error(`psola-check: REFUSING to bless — ${bad.length} absolute check(s) red:`)
    for (const f of bad) console.error(`  ✗ ${f}`)
    console.error('fix the render (or the TAKES regions) first; --save freezes a fault into the baseline.')
    process.exit(1)
  }
  fs.writeFileSync(BASELINE, JSON.stringify({ note: 'blessed psola-check baseline — see tools/psola-check.js', takes: now }, null, 2) + '\n')
  console.log(`blessed → ${path.relative(ROOT, BASELINE)}`)
  process.exit(0)
}

// A MISSING baseline used to silently drop the two relative checks and still exit 0 — loudly in a
// normal run, but --quiet prints nothing at all, so CI would have gone green on half a gate.
if (!base) {
  console.error(`psola-check: no baseline at ${path.relative(ROOT, BASELINE)} — the splice and periodicity checks cannot run.`)
  console.error('run with --save to bless the current numbers.')
  process.exit(2)
}

const control = now.RAW
const fails = verdict(now, base)

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
  console.log('\nRAW is the unprocessed control — it shows the CAPTURE has not moved, and gives xRAW its scale.')
  console.log('The bar each take is held to is the blessed baseline, not RAW (measured: 1.5x-1.9x RAW is normal).')
  if (!base) console.log('no baseline yet — run with --save to bless these numbers.')
}

if (fails.length) {
  console.error((quiet ? '' : '\n') + `psola-check FAIL (${fails.length}):`)
  for (const f of fails) console.error(`  ✗ ${f}`)
  process.exit(1)
}
if (!quiet) console.log('\n✓ no artifact regression')
process.exit(0)

// ── --selfcheck: KNOWN ANSWERS FOR THE THREE DETECTORS ──────────────────────────────────────────
// This gate's failure mode is not a false alarm, it is going BLIND — and uniquely so, because its
// three detectors are deliberately blind to each other's defects, so ONE of them dying is invisible
// by construction: the other two keep printing plausible numbers. (Measured on a live render: SPLICE
// reads exactly `0 / 0.0000` on all four takes, which is indistinguishable from a dead detector.)
//
// So: synthesise signals carrying ONE specific artifact each, whose answer is known by construction,
// and assert BOTH directions — the detector that owns the artifact fires, AND the two that are
// documented blind to it stay quiet. Those blindness pairs are the tool's central claim; before this
// fixture nothing checked them, and every number below was MEASURED before it was written down.
function scWriteWav (file, x, sr) {
  const n = x.length, b = Buffer.alloc(44 + n * 2)
  b.write('RIFF', 0); b.writeUInt32LE(36 + n * 2, 4); b.write('WAVE', 8)
  b.write('fmt ', 12); b.writeUInt32LE(16, 16); b.writeUInt16LE(1, 20); b.writeUInt16LE(1, 22)
  b.writeUInt32LE(sr, 24); b.writeUInt32LE(sr * 2, 28); b.writeUInt16LE(2, 32); b.writeUInt16LE(16, 34)
  b.write('data', 36); b.writeUInt32LE(n * 2, 40)
  for (let i = 0; i < n; i++) b.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(x[i] * 32767))), 44 + i * 2)
  fs.writeFileSync(file, b)
}

function selfcheck () {
  const os = require('os')
  const SR = 44100
  // 220.5 Hz at 44.1k is a period of EXACTLY 200 samples, so a period-doubling or a phase break can
  // be placed on an exact sample and its size is known by arithmetic rather than by measurement.
  const P = 200, F = SR / P                       // 220.5 Hz
  const N = SR                                    // 1 s per probe signal
  const A = Math.round(0.1 * SR), B = Math.round(0.9 * SR)   // the analysed region
  const K = Math.round(0.5 * SR)                  // artifacts land at exactly t = 0.500 s
  const Z = 22000                                 // …and Z is the nearest exact ZERO CROSSING (a
                                                  // multiple of P), where a phase break makes no step
  const measureMode = argv.includes('--measure')

  const gen = (f) => { const x = new Float32Array(N); for (let i = 0; i < N; i++) x[i] = f(i); return x }
  const sine = (i, period) => 0.6 * Math.sin(2 * Math.PI * i / (period || P))
  // Seeded, because a fixture that varies run to run is a fixture that flakes. (Plain Math.random()
  // was in the first draft of this file.)
  let seed = 1
  const rnd = () => { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed / 0x7fffffff }

  // ── the probe signals, one artifact each ──
  const S = {
    // clean: the reference every "stays quiet" answer is read against.
    clean: gen(i => sine(i)),
    // a butt-joined grain boundary: the waveform steps by EXACTLY 0.3 at K.
    dcstep: gen(i => sine(i) + (i >= K ? 0.3 : 0)),
    // an EQUAL-AMPLITUDE phase break: the polarity inverts at Z, which is an exact ZERO CROSSING, so
    // the value is continuous (SPLICE has no step to see) while every later period is the negative
    // of the one before the break. The header's claim that SPLICE "scored the snapped and up takes at
    // exactly 0 while they were audibly glitching".
    // ⚠ The first draft used a TIME REVERSAL here and all three detectors read 0 — correctly: a
    // reversed sine is still a sine at the same frequency, so that signal had no artifact in it at
    // all. Mis-synthesise and the fixture pins itself instead of the detector.
    phasebreak: gen(i => (i >= Z ? -1 : 1) * sine(i)),
    // a ZERO-ORDER-HOLD staircase from a truncated fractional read. 4 divides the 200-sample period,
    // so it repeats IDENTICALLY every period — PERIODICITY is blind to it by construction.
    staircase: gen(i => sine(4 * Math.floor(i / 4))),
    // PERIOD DOUBLING: alternate periods at 0.7 amplitude. The switch lands on a zero crossing, so
    // there is no step either — and the signal is still perfectly periodic, at 2P. Both of the other
    // detectors are blind BY CONSTRUCTION; only the f0 reading sees it.
    doubled: gen(i => sine(i) * (Math.floor(i / P) % 2 ? 0.7 : 1.0)),
    // an epoch choice that FLIPS halfway: P for the first half, 2P for the second.
    flip: gen(i => i < K ? sine(i) : sine(i, 2 * P)),
    // two DC steps 1 ms apart, and two 10 ms apart — the debounce, from both sides.
    twoclose: gen(i => sine(i) + (i >= Z ? 0.3 : 0) + (i >= Z + 44 ? 0.3 : 0)),
    twofar: gen(i => sine(i) + (i >= Z ? 0.3 : 0) + (i >= Z + 441 ? 0.3 : 0)),
    // the two vacuity cases: nothing to measure at all.
    silence: gen(() => 0),
    noise: gen(() => 0.6 * (2 * rnd() - 1)),
  }

  const sp = (k) => splice(S[k], A, B, SR)
  const pe = (k) => periodicity(S[k], A, B, SR)
  const db = (k, expect) => doubling(S[k], A, B, SR, expect || F)

  if (measureMode) {
    console.log('MEASURED (before any of it was asserted)\n')
    for (const k of Object.keys(S)) {
      const s = sp(k), p = pe(k), d = db(k)
      console.log(`${k.padEnd(11)} splice{n:${String(s.count).padStart(4)} worst:${s.worst.toFixed(4)} med:${s.med.toFixed(6)}}  ` +
        `per{p95:${p.p95.toFixed(4)} worst:${String(p.worst).padStart(7)} voiced:${p.voiced}/${p.windows}}  ` +
        `dbl{f0:${d.f0} spread:${d.spread} ratio:${d.ratio} voiced:${d.voiced}/${d.windows}}`)
    }
    return 0
  }

  let pass = 0, fail = 0
  const ok = (name, cond, got) => {
    if (cond) { pass++; console.log(`  ✓ ${name}`) }
    else { fail++; console.log(`  ✗ ${name}   got: ${got}`) }
  }

  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'psolacheck-'))
  try {
    console.log('psola-check --selfcheck — known answers for the three detectors (audio is synthesised here)\n')

    console.log('THE CLEAN REFERENCE IS CLEAN (or nothing below means anything)')
    ok('a pure sine trips no splice', sp('clean').count === 0, sp('clean').count)
    ok('a pure sine is periodic to within 1% of its own rms', pe('clean').p95 < 0.01, pe('clean').p95)
    ok('a pure sine reads its own f0', Math.abs(db('clean').ratio - 1) < 0.005, db('clean').ratio)
    ok('  …with zero spread across sub-windows', db('clean').spread === 0, db('clean').spread)
    ok('every window of it is voiced', pe('clean').voiced === pe('clean').windows,
       `${pe('clean').voiced}/${pe('clean').windows}`)

    console.log('\nDETECTOR 1 — SPLICE catches a value STEP, and reports its true size')
    ok('a 0.3 DC step is caught', sp('dcstep').count >= 1, sp('dcstep').count)
    ok('  …and `worst` IS the step (0.3 by construction, ±the local slope)',
       Math.abs(sp('dcstep').worst - 0.3) < 0.02, sp('dcstep').worst)
    ok('a zero-order-hold STAIRCASE is caught (the truncated fractional read)',
       sp('staircase').count >= 1, sp('staircase').count)
    ok('  …at the step size the hold implies (0.6·2π·4/200 = 0.0754)',
       Math.abs(sp('staircase').worst - 0.0754) < 0.002, sp('staircase').worst)
    ok('  …while PERIODICITY is blind — 4 divides the period, so it repeats identically',
       pe('staircase').worst === 0, pe('staircase').worst)

    console.log('\nTHE 2 ms DEBOUNCE, FROM BOTH SIDES')
    ok('two steps 1 ms apart report as ONE event', sp('twoclose').count === 1, sp('twoclose').count)
    ok('two steps 10 ms apart stay TWO events', sp('twofar').count === 2, sp('twofar').count)
    // ⚠ PINNED, NOT FIXED. `last` advances on every trip regardless of the gate, so an artifact that
    // is CONTINUOUS collapses to a single event however long it runs: the staircase above steps every
    // 4 samples for 0.8 s and counts 1, the same as one isolated pop. `worst` still separates them
    // (0.075 vs 0.300), and real PSOLA grain splices sit ~4.5 ms apart at these pitches — above the
    // gate — which is how the down-shift bug counted 177. Widening it would change what the gate
    // ACCEPTS, so it is recorded here as a property rather than quietly adjusted.
    ok('a CONTINUOUS train collapses to one event — count is not a density measure',
       sp('staircase').count === 1 && sp('staircase').n > 30000,
       `${sp('staircase').count} over ${sp('staircase').n} samples`)

    console.log('\nDETECTOR 2 — PERIODICITY catches an EQUAL-AMPLITUDE phase break')
    ok('a polarity flip on a zero crossing is caught', pe('phasebreak').worst > 0.5, pe('phasebreak').worst)
    ok('  …and SPLICE is blind to it — no value step to see (the documented pair)',
       sp('phasebreak').count === 0, sp('phasebreak').count)
    ok('  …and DOUBLING is blind too — the frequency never changed',
       Math.abs(db('phasebreak').ratio - 1) < 0.005, db('phasebreak').ratio)

    console.log('\nDETECTOR 3 — DOUBLING catches what BOTH the others score as clean')
    const dd = db('doubled')
    ok('a period-doubled take reads HALF its expected f0', Math.abs(dd.ratio - 0.5) < 0.02, dd.ratio)
    ok('  …which the verdict names PERIOD DOUBLING',
       verdict({ T: { splice: sp('doubled'), periodicity: pe('doubled'), doubling: dd } }, null,
         [{ name: 'T', f0: F }]).some(f => /PERIOD DOUBLING/.test(f)), 'no such fail')
    ok('  …while SPLICE stays at zero (the amplitude switch is on a zero crossing)',
       sp('doubled').count === 0, sp('doubled').count)
    ok('  …and PERIODICITY scores it CLEANER THAN THE CLEAN SINE — it is still perfectly periodic',
       pe('doubled').p95 <= pe('clean').p95, `${pe('doubled').p95} vs ${pe('clean').p95}`)
    const fl = db('flip')
    ok('an epoch that FLIPS halfway shows up as f0 SPREAD', fl.spread > F * 0.25, fl.spread)
    ok('  …and the verdict names it flipping',
       verdict({ T: { splice: sp('flip'), periodicity: pe('flip'), doubling: fl } }, null,
         [{ name: 'T', f0: F }]).some(f => /flipping/.test(f)), 'no such fail')

    // A hand-built measurement, so each tier can be pushed off its edge one field at a time.
    const mk = (o) => ({ T: {
      splice: Object.assign({ count: 0, worst: 0, n: 1e5, med: 1e-4 }, o.splice),
      periodicity: Object.assign({ p95: 0.2, worst: 0.8, windows: 26, voiced: 26 }, o.periodicity),
      doubling: Object.assign({ f0: F, spread: 0, ratio: 1.0, windows: 8, voiced: 8 }, o.doubling),
    } })
    const TK = [{ name: 'T', f0: F }]
    const v = (cur, bas) => verdict(mk(cur), bas ? mk(bas) : null, TK)

    console.log('\nVACUITY — "no artifact found" must not be how a DEAD MEASUREMENT reads')
    ok('silence trips no splice (this is the hole, not the fix)', sp('silence').count === 0, sp('silence').count)
    ok('silence scores a PERFECT periodicity — better than the clean sine',
       pe('silence').p95 === 0 && pe('silence').p95 <= pe('clean').p95, pe('silence').p95)
    ok('  …but voiced=0 records that nothing was measured', pe('silence').voiced === 0, pe('silence').voiced)
    ok('  …and the verdict REFUSES it', verdict(
       { T: { splice: sp('silence'), periodicity: pe('silence'), doubling: db('silence') } }, null,
       [{ name: 'T', f0: F }]).length > 0, 'no such fail')
    ok('an UNVOICED (noise) take is refused too', verdict(
       { T: { splice: sp('noise'), periodicity: pe('noise'), doubling: db('noise') } }, null,
       [{ name: 'T', f0: F }]).length > 0, 'no such fail')
    // ⚠ EACH LIVENESS CHECK SEPARATELY. The first draft asserted only that *some* fail matched
    // /NOTHING WAS MEASURED/, and BOTH of these mutations survived green: silence trips both checks,
    // so either one alone satisfied the assertion and deleting the other changed nothing. A control
    // has to be mutated too, or it is decoration. (checks-and-oracles.md, level-check.)
    ok('the PERIODICITY liveness check fires on its own',
       v({ periodicity: { voiced: 0 } }, {}).some(f => /periodicity measured 0 of 26/.test(f)),
       v({ periodicity: { voiced: 0 } }, {}).join('; ') || 'nothing fired')
    ok('the DOUBLING liveness check fires on its own',
       v({ doubling: { voiced: 0 } }, {}).some(f => /f0 unreadable in all 8/.test(f)),
       v({ doubling: { voiced: 0 } }, {}).join('; ') || 'nothing fired')
    ok('a MOSTLY-unvoiced take is refused as well (half the windows is the floor)',
       v({ periodicity: { voiced: 12 } }, {}).some(f => /mostly unvoiced/.test(f)),
       v({ periodicity: { voiced: 12 } }, {}).join('; ') || 'nothing fired')
    ok('  …and 13 of 26 is accepted', v({ periodicity: { voiced: 13 } }, {}).length === 0,
       v({ periodicity: { voiced: 13 } }, {}).join('; '))
    // The f0 checks are SKIPPED when nothing was read, so a dead take reports "nothing was measured"
    // rather than the nonsense "f0 0Hz is 0x expected" on top of it.
    ok('a dead take reports the liveness failure and NOT a bogus f0 ratio',
       v({ doubling: { voiced: 0, ratio: 0, f0: 0 } }, {}).length === 1,
       v({ doubling: { voiced: 0, ratio: 0, f0: 0 } }, {}).join('; '))

    console.log('\nTHE VERDICT TOLERANCES, ON BOTH SIDES OF EACH EDGE')
    ok('a clean take against a clean baseline passes', v({}, {}).length === 0, v({}, {}).join('; '))
    ok('splice: 8 more than blessed(0) PASSES', v({ splice: { count: 8 } }, {}).length === 0,
       v({ splice: { count: 8 } }, {}).join('; '))
    ok('splice: 9 more than blessed(0) FAILS', v({ splice: { count: 9 } }, {}).length === 1,
       v({ splice: { count: 9 } }, {}).join('; '))
    ok('splice: blessed(10) tolerates 20 (the 2x arm)',
       v({ splice: { count: 20 } }, { splice: { count: 10 } }).length === 0, 'failed')
    ok('splice: blessed(10) refuses 21', v({ splice: { count: 21 } }, { splice: { count: 10 } }).length === 1, 'passed')
    ok('periodicity: 1.5x+0.1 of blessed PASSES at the edge',
       v({ periodicity: { worst: 1.3 } }, { periodicity: { worst: 0.8 } }).length === 0, 'failed')
    ok('periodicity: just past the edge FAILS',
       v({ periodicity: { worst: 1.31 } }, { periodicity: { worst: 0.8 } }).length === 1, 'passed')
    ok('f0 1.25x expected is refused', v({ doubling: { ratio: 1.26 } }, {}).length === 1, 'passed')
    ok('f0 within 25% is accepted', v({ doubling: { ratio: 1.24 } }, {}).length === 0, 'failed')
    ok('a take missing from the run is refused, not skipped',
       verdict({}, null, TK).some(f => /no measurement/.test(f)), 'no such fail')
    ok('WITHOUT a baseline the two RELATIVE checks cannot fire (which is why the CLI refuses)',
       v({ splice: { count: 900 }, periodicity: { worst: 99 } }, null).length === 0, 'something fired')
    ok('  …while the ABSOLUTE ones still do', v({ doubling: { ratio: 0.5 } }, null).length === 1, 'did not fire')

    console.log('\nEND TO END — the WAV reader, the regions, and the exit codes')
    // A synthetic stand-in for a voxshift render: each TAKES region filled with its own note, long
    // enough that the region slicing is exercised for real rather than mocked.
    const total = Math.ceil(TAKES[TAKES.length - 1].to * SR) + SR
    const fake = (bad) => {
      const x = new Float32Array(total)
      for (const t of TAKES) {
        const a = Math.round(t.from * SR), b = Math.round(t.to * SR), per = SR / t.f0
        for (let i = a; i < b; i++) {
          x[i] = 0.6 * Math.sin(2 * Math.PI * (i - a) / per) *
            (bad && t.name === bad && Math.floor((i - a) / per) % 2 ? 0.7 : 1.0)
        }
      }
      return x
    }
    const good = path.join(dir, 'good.wav'); scWriteWav(good, fake(null), SR)
    const bad = path.join(dir, 'bad.wav');   scWriteWav(bad, fake('UP+12'), SR)
    const shortw = path.join(dir, 'short.wav'); scWriteWav(shortw, fake(null).slice(0, SR), SR)
    const bl = path.join(dir, 'base.json')
    const run = (args) => spawnSync(process.execPath, [__filename].concat(args),
      { cwd: ROOT, encoding: 'utf8' })

    ok('--save writes a baseline and exits 0',
       run(['--wav', good, '--save', '--baseline', bl]).status === 0 && fs.existsSync(bl), 'no baseline')
    ok('a clean render passes against it', run(['--wav', good, '--quiet', '--baseline', bl]).status === 0,
       run(['--wav', good, '--quiet', '--baseline', bl]).stderr)
    const badRun = run(['--wav', bad, '--quiet', '--baseline', bl])
    ok('a PERIOD-DOUBLED take exits 1', badRun.status === 1, badRun.status)
    ok('  …naming the take and the defect', /UP\+12.*PERIOD DOUBLING/.test(badRun.stderr), badRun.stderr.trim())
    const noBase = run(['--wav', good, '--quiet', '--baseline', path.join(dir, 'nope.json')])
    ok('a MISSING baseline is refused (exit 2), not silently half-run', noBase.status === 2, noBase.status)
    ok('  …and it says so even under --quiet', /no baseline/.test(noBase.stderr), noBase.stderr.trim())
    const saveBad = run(['--wav', bad, '--save', '--baseline', path.join(dir, 'b2.json')])
    ok('--save REFUSES to bless a render with a red absolute check', saveBad.status === 1, saveBad.status)
    ok('  …and writes no baseline file', !fs.existsSync(path.join(dir, 'b2.json')), 'wrote one')
    ok('a WAV too short for the regions exits 2, not 0',
       run(['--wav', shortw, '--quiet', '--baseline', bl]).status === 2,
       run(['--wav', shortw, '--quiet', '--baseline', bl]).status)

    console.log(`\n${fail === 0 ? '✓' : '✗'} ${pass}/${pass + fail} known answers correct`)
    return fail === 0 ? 0 : 1
  } finally { fs.rmSync(dir, { recursive: true, force: true }) }
}
