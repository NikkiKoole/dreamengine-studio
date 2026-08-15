#!/usr/bin/env node
// dc-check.js — does each synth engine output a clean (DC-free) signal? tune-check.js measures
// PITCH; this measures the DC OFFSET. It renders tools/carts/tunecheck.c (a sweep of every engine
// across four octaves), reads the trace to segment per-note, and reports the mean (DC) of each
// note's sustained body. SINE is the control — a pure tone is mathematically DC-free.
//
//   node tools/dc-check.js              render the sweep + full report
//   node tools/dc-check.js --json       machine-readable
//   node tools/dc-check.js --keep       keep the rendered WAV/trace (build/.dc/)
//   node tools/dc-check.js --quiet      exit 1 if any engine carries DC past tolerance (CI gate)
//   node tools/dc-check.js --selfcheck  known answers for the MEASUREMENT on synthetic signals
//                                       (renders no cart; 16 assertions, mutation-tested)
//
// WHY this exists: dreamengine removes DC at the SOURCE (each engine blocks its own — the blown
// models reed/pipe/brass carry a large DC from steady mouth pressure; the asymmetric nonlinearities
// epiano/guitar/the drive effect/brass inject it too). There is deliberately NO master DC blocker
// (it would eat headroom against the master soft-clip, click on note-on under the amp envelope, and
// break byte-reproducibility for old carts — see the master mix in runtime/sound.h). That source-
// blocking scheme rests on author discipline: every new engine / asymmetric shaper must remember its
// own blocker. THIS is the backstop — if someone forgets, a sustained note's mean drifts off zero and
// this fails, without touching the audio path. (It bit BRASS once: an asymmetric brassiness shaper
// injected −0.02 DC; the fix was a per-voice output blocker.) Pairs with tune-check.js / soundcheck.

const fs = require('fs')
const path = require('path')
const { spawnSync } = require('child_process')

const ROOT = path.resolve(__dirname, '..')

// thresholds (linear, |mean| of the sustained note body). A clean engine sits near 1e-4 (≈−80 dBFS,
// the render's quantization floor). Flag a whisker above that; scream where DC starts eating
// headroom / thumping on note-on. Calibrated so every currently-shipping engine passes (see header).
const WARN_DC = 0.004   // ≈ −48 dBFS — small but real offset; a blocker is drifting or absent
const BAD_DC  = 0.015   // ≈ −36 dBFS — clearly audible headroom loss / note-on thump
// ⚠ THESE TWO ARE NOW VERY LOOSE and were deliberately left alone. They were calibrated against the
// PLAIN-mean readings, whose floor was the window artifact described at measureDC (WARN_DC 0.004 vs
// an A2 truncation residual of 0.004134 — the threshold was sized to clear the artifact, not the
// engines). With the Hann mean every shipping engine sits at or below 7.2e-5, a 55x margin, so they
// could be tightened roughly 4x and still keep an order of magnitude. That is a change to what the
// gate ACCEPTS, which is the maker's call, not a mechanical follow-on — so it is written down here
// rather than done quietly.

// ── THE CONTROL, HELD TO A CONTROL'S BOUND ───────────────────────────────────
// SINE is DC-free by construction, so its reading measures the MEASUREMENT. It used to read
// -0.000473 (-67 dBFS) — DIRTIER than GUITAR at -103 — because the plain mean was reporting window
// truncation. With that gone it reads exactly 0.000000, so a real bound is finally meaningful.
// Anything past this and no engine number in the run can be trusted.
const CONTROL_ENGINE = 4
const CONTROL_DC     = 0.00005   // ≈ −86 dBFS; the blessed reading is 0.000000

// engine id → label, mirrors the INSTR_* block in runtime/studio.h (kept in sync with tune-check.js)
const ENGINE_NAMES = {
  0: 'SQUARE', 1: 'SAW', 2: 'TRI', 3: 'NOISE', 4: 'SINE (control)',
  16: 'PLUCK  karplus-strong', 17: 'MALLET struck bar', 18: 'FM 2-op',
  19: 'ORGAN tonewheel', 20: 'EPIANO rhodes/wurli', 21: 'PD casio-cz',
  22: 'MEMBRANE drum', 23: 'REED clarinet/sax', 24: 'VOICE formant',
  25: 'PIPE flute', 26: 'GUITAR plucked+body', 27: 'PIANO stiff-string',
  28: 'BOWED violin/cello', 29: 'BRASS lip-reed',
}

const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
const midiName = (m) => `${NOTE_NAMES[m % 12]}${Math.floor(m / 12) - 1}`
const dbfs = (lin) => lin <= 0 ? '-inf' : `${(20 * Math.log10(lin)).toFixed(0)} dBFS`

// ── WAV (16-bit PCM, mono or stereo→mono) — identical reader to tune-check.js ────────────────
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

// ── trace → note windows — identical segmentation to tune-check.js (index by FRAME, not `t`) ──
function noteWindows(traceFile) {
  const lines = fs.readFileSync(traceFile, 'utf8').trim().split('\n')
  const notes = []
  let cur = null, lastFrame = 0
  for (const ln of lines) {
    let row; try { row = JSON.parse(ln) } catch { continue }
    if (row.vev !== undefined) continue   // skip voice-trace events (-DDE_TRACE): they share the trace JSONL but carry no gate/window info (would close a note window early)
    const w = row.w || {}
    const gate = +w.gate, eng = +w.eng, emidi = +w.emidi, f = row.f
    lastFrame = f
    if (gate === 1 && emidi > 0) {
      if (cur && cur.eng === eng && cur.midi === emidi) cur.f1 = f
      else { if (cur) notes.push(cur); cur = { eng, midi: emidi, f0: f, f1: f } }
    } else if (cur) { notes.push(cur); cur = null }
  }
  if (cur) notes.push(cur)
  return { notes, lastFrame }
}

// ── render the sweep (same cart + frame budget as tune-check.js) ──────────────────────────────
function renderSweep() {
  const dir = path.join(ROOT, 'build', '.dc')
  fs.mkdirSync(dir, { recursive: true })
  const wav = path.join(dir, 'sweep.wav'), trace = path.join(dir, 'sweep.trace.jsonl')
  const r = spawnSync('node',
    [path.join('tools', 'play.js'), 'tunecheck', 'run', '--headless',
     '--frames', '3400', '--trace', trace, '--wav', wav],
    { cwd: ROOT, encoding: 'utf8' })
  if (r.status !== 0) {
    process.stderr.write((r.stdout || '') + (r.stderr || ''))
    throw new Error('render failed (play.js tunecheck)')
  }
  return { wav, trace, dir }
}

// ── analysis: mean (DC) + rms of each note's sustained body ───────────────────────────────────
function verdict(dc) {
  const a = Math.abs(dc)
  return a > BAD_DC ? 'DC OFFSET' : a > WARN_DC ? 'off' : 'ok'
}
const mark = (v) => v === 'DC OFFSET' ? '✗' : v === 'off' ? '⚠' : '·'

// The measurement itself, lifted out of analyze() so --selfcheck can put known signals through the
// SAME code the gate uses. Returns null where there is nothing to judge — too short, or silent.
//
// ⚠ THE MEAN IS HANN-WEIGHTED, AND THAT IS NOT A REFINEMENT — a plain mean measured this gate's own
// noise floor rather than the engine's DC. A rectangular window over a non-integer number of cycles
// leaves a residual bounded by A/(pi*cycles): worst at a HALF-integer cycle count and scaling as
// 1/f, so it is ~40x larger at A2 than at A5. Measured, amplitude 0.5: a PURE sine over 38.5 cycles
// (110 Hz, 0.35 s) has a plain mean of +0.004134 — which is essentially WARN_DC, so the threshold
// had been calibrated to clear an artifact. It also showed in the report: the SINE control read
// -67 dBFS while GUITAR read -103, and nearly every engine's worst note was A2, the lowest.
// Worse, the artifact ADDS to real DC rather than just limiting sensitivity: a true 0.010 offset at
// 110 Hz measured +0.014134, a 41% overestimate.
// A Hann taper goes to zero at both ends, so the edge discontinuity that causes all of this simply
// is not there. Same signals: pure sine -0.0000028 (1500x cleaner) and a true 0.010 offset reads
// +0.009997. It needs no knowledge of the note's frequency, which trimming to whole cycles would.
// Pinned in --selfcheck, both directions: the artifact must stay gone AND real DC must survive.
function measureDC(s, a, b) {
  const N = Math.min(b, s.length) - a
  if (N < 64) return null                                  // too short to trust
  let wsum = 0, dsum = 0, sq = 0, n = 0
  for (let i = 0; i < N; i++) {
    const v = s[a + i]
    const w = 0.5 - 0.5 * Math.cos(2 * Math.PI * i / (N - 1))
    wsum += w; dsum += w * v; sq += v * v; n++
  }
  const dc = wsum > 0 ? dsum / wsum : 0, rms = Math.sqrt(sq / n)
  if (rms < 0.0015) return null                            // effectively silent — no signal to judge
  return { dc, rms, n }
}

function analyze(wav, trace) {
  const { sr, s } = readWavMono(wav)
  const { notes, lastFrame } = noteWindows(trace)
  if (!notes.length) throw new Error('no gated notes found in trace — did the cart build?')
  const spf = s.length / (lastFrame + 1)   // samples per frame (audio is frame-locked)
  const results = []
  for (const nt of notes) {
    // measure the sustained body (12%..88%), skipping the onset transient and the release tail —
    // exactly the window tune-check.js uses, so a settling attack DC isn't counted.
    const a = Math.floor(nt.f0 * spf + (nt.f1 - nt.f0) * spf * 0.12)
    const b = Math.floor(nt.f0 * spf + (nt.f1 - nt.f0) * spf * 0.88)
    const m = measureDC(s, a, b)
    if (!m) continue
    const { dc, rms } = m
    results.push({
      engine: nt.eng, engineName: ENGINE_NAMES[nt.eng] || `INSTR ${nt.eng}`,
      midi: nt.midi, note: midiName(nt.midi),
      dc: +dc.toFixed(6), rms: +rms.toFixed(4), verdict: verdict(dc),
    })
  }
  return { results, sr }
}

// worst |dc| per engine — one bad note condemns the engine (a forgotten blocker hits every note)
function perEngine(results) {
  const by = new Map()
  for (const r of results) {
    const cur = by.get(r.engine)
    if (!cur || Math.abs(r.dc) > Math.abs(cur.dc)) by.set(r.engine, r)
  }
  return [...by.values()].sort((a, b) => a.engine - b.engine)
}

function printResults(results, sr) {
  const eng = perEngine(results)
  console.log(`DC offset check — ${results.length} notes @ ${sr}Hz   (warn >${WARN_DC}, bad >${BAD_DC})\n`)
  for (const r of eng) {
    const sign = r.dc >= 0 ? '+' : ''
    console.log(`  ${mark(r.verdict)} ${r.engineName.padEnd(22)} dc ${sign}${r.dc.toFixed(6)}  (${dbfs(Math.abs(r.dc))})   worst @ ${r.note}`)
  }
  const bad = eng.filter(r => r.verdict !== 'ok').sort((a, b) => Math.abs(b.dc) - Math.abs(a.dc))
  console.log()
  if (!bad.length) console.log('✓ every engine is DC-clean (source-blocking holding)')
  else {
    console.log(`${bad.length} engine(s) carrying DC (worst first):`)
    for (const r of bad)
      console.log(`  ${mark(r.verdict)} ${r.engineName} dc ${r.dc >= 0 ? '+' : ''}${r.dc} (${dbfs(Math.abs(r.dc))}) — add/repair an OUTPUT DC blocker in its sound.h voice`)
  }
}

// ── cli ──────────────────────────────────────────────────────────────────────
// ── --selfcheck: KNOWN ANSWERS FOR THE MEASUREMENT ───────────────────────────
// Renders no cart. measureDC is pure, so it is judged on signals whose DC is known by construction.
// Both directions, because the two failure modes are opposite: the window artifact must stay gone
// AND a real offset must still be reported at full size. A measurement that reports 0 for
// everything passes a one-sided test and is exactly how this gate would go blind.
function selfcheck() {
  const SR = 44100
  let pass = 0, fail = 0
  const ok = (name, cond, got) => {
    if (cond) { pass++; console.log(`  ✓ ${name}`) } else { fail++; console.log(`  ✗ ${name}   got: ${got}`) }
  }
  // 110 Hz over 0.35 s = 38.5 cycles: the WORST case for a rectangular window (a half-integer count)
  const gen = (secs, f) => { const n = Math.floor(SR * secs), x = new Float64Array(n)
    for (let i = 0; i < n; i++) x[i] = f(i, n); return x }
  const sine = (amp, hz) => (i) => amp * Math.sin(2 * Math.PI * hz * i / SR)
  const plainMean = (x) => { let t = 0; for (let i = 0; i < x.length; i++) t += x[i]; return t / x.length }
  const dcOf = (x) => { const m = measureDC(x, 0, x.length); return m ? m.dc : null }

  console.log('dc-check --selfcheck — known answers for the measurement (no cart is rendered)\n')

  console.log('THE WINDOW ARTIFACT IS GONE')
  const pure = gen(0.35, sine(0.5, 110))                      // 38.5 cycles — worst case
  const pd = dcOf(pure), pp = plainMean(pure)
  ok('a DC-free sine over a half-integer cycle count reads ~0', Math.abs(pd) < 1e-5, pd)
  ok('  …and a PLAIN mean of the same signal would NOT (this is the bug being prevented)',
     Math.abs(pp) > 1e-3, pp)
  ok('  …the Hann mean is >100x cleaner than the plain one', Math.abs(pp) / Math.abs(pd || 1e-12) > 100,
     `${(Math.abs(pp) / Math.abs(pd || 1e-12)).toFixed(0)}x`)
  ok('a DECAYING sine still reads ~0 (an envelope is not DC)',
     Math.abs(dcOf(gen(0.35, (i, n) => Math.exp(-3 * i / n) * Math.sin(2 * Math.PI * 220 * i / SR)))) < 1e-4,
     dcOf(gen(0.35, (i, n) => Math.exp(-3 * i / n) * Math.sin(2 * Math.PI * 220 * i / SR))))

  console.log('\nREAL DC SURVIVES AT FULL SIZE — the direction a blind measurement fails')
  const off = (v) => dcOf(gen(0.35, (i) => 0.5 * Math.sin(2 * Math.PI * 110 * i / SR) + v))
  ok('an injected +0.010 offset reads +0.010 (within 1%)', Math.abs(off(0.010) - 0.010) < 0.0001, off(0.010))
  ok('an injected -0.030 offset reads -0.030 (within 1%)', Math.abs(off(-0.030) + 0.030) < 0.0003, off(-0.030))
  // a 25% duty square has an exactly known mean: amp*(0.25 - 0.75) = -0.5*amp
  const duty = gen(0.35, (i) => (((110 * i / SR) % 1) < 0.25 ? 0.6 : -0.6))
  ok('a 25%-duty square reads its arithmetic mean of -0.30', Math.abs(dcOf(duty) + 0.30) < 0.005, dcOf(duty))

  console.log('\nTHE VERDICT BANDS, AND THE GATE CAN GO RED')
  ok('a clean tone is ok', verdict(0) === 'ok', verdict(0))
  ok('past WARN_DC it is off', verdict(WARN_DC * 1.5) === 'off', verdict(WARN_DC * 1.5))
  ok('past BAD_DC it is DC OFFSET', verdict(BAD_DC * 1.5) === 'DC OFFSET', verdict(BAD_DC * 1.5))
  ok('an injected 0.05 offset is judged DC OFFSET', verdict(off(0.05)) === 'DC OFFSET', verdict(off(0.05)))
  ok('the CONTROL bound is far tighter than the engine bound', CONTROL_DC < WARN_DC / 10,
     `${CONTROL_DC} vs ${WARN_DC}`)
  ok('  …and the old artifact (0.004134) would have tripped it', 0.004134 > CONTROL_DC, CONTROL_DC)

  console.log('\nIT REFUSES TO JUDGE WHAT IT CANNOT')
  ok('digital silence yields no measurement (rms guard)', dcOf(gen(0.35, () => 0)) === null, dcOf(gen(0.35, () => 0)))
  ok('a too-short window yields no measurement', measureDC(new Float64Array(32), 0, 32) === null, 'not null')
  ok('a NEAR-silent signal is skipped rather than divided by',
     dcOf(gen(0.35, sine(0.0005, 220))) === null, dcOf(gen(0.35, sine(0.0005, 220))))

  console.log(`\n${fail === 0 ? '✓' : '✗'} ${pass}/${pass + fail} known answers correct`)
  return fail === 0 ? 0 : 1
}

const argv = process.argv.slice(2)
const json = argv.includes('--json')
const keep = argv.includes('--keep')
const quiet = argv.includes('--quiet')
if (argv.includes('--selfcheck')) process.exit(selfcheck())

try {
  const { wav, trace, dir } = renderSweep()
  const { results, sr } = analyze(wav, trace)
  if (!keep) fs.rmSync(dir, { recursive: true, force: true })
  if (json) console.log(JSON.stringify(perEngine(results), null, 2))
  else if (!quiet) printResults(results, sr)
  if (quiet) {
    // The control first, and reported differently: if SINE has drifted the engine numbers below it
    // are meaningless, and "N engines carry DC" would send the next person after a missing blocker
    // that is not missing.
    const ctl = results.filter(r => r.engine === CONTROL_ENGINE)
    if (!ctl.length) {
      console.error(`dc-check: the SINE CONTROL DID NOT RUN — no engine ${CONTROL_ENGINE} rows in the sweep.`)
      console.error('  Without it this gate cannot tell a DC-carrying engine from a broken measurement. Refusing to pass.')
      process.exit(1)
    }
    const ctlBad = ctl.filter(r => Math.abs(r.dc) > CONTROL_DC)
    if (ctlBad.length) {
      console.error('dc-check: THE MEASUREMENT IS OFF, NOT THE ENGINE — the SINE control read ' +
                    ctlBad.map(r => `${r.note} ${r.dc}`).join(', ') +
                    ` (a DC-free tone must sit inside ±${CONTROL_DC}).`)
      console.error('  Every engine number in this run is suspect. Start with `node tools/dc-check.js --selfcheck`.')
      process.exit(1)
    }
    const bad = perEngine(results).filter(r => r.verdict !== 'ok')
    if (bad.length) console.error(`dc-check: ${bad.length} engine(s) carry DC: ${bad.map(r => r.engineName.split(' ')[0]).join(', ')}`)
    process.exit(bad.length ? 1 : 0)
  }
} catch (e) { console.error('dc-check:', e.message); process.exit(2) }
