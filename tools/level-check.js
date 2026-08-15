#!/usr/bin/env node
// level-check.js — does each synth engine play at the RIGHT LEVEL? tune-check.js measures
// PITCH; this measures LOUDNESS. It renders the SAME sweep cart (tools/carts/tunecheck.c —
// every pitched engine across four octaves of A, gated, with a watch()-driven ground-truth
// trace), reads the WAV, and measures each note's peak / RMS / crest in dBFS.
//
//   node tools/level-check.js            render the sweep + full per-engine level report
//   node tools/level-check.js --save     render + WRITE the golden baseline (tools/level-baseline.json)
//   node tools/level-check.js --quiet    CI gate: exit 1 on any regression vs baseline (or new silence/clip)
//   node tools/level-check.js --json     machine-readable
//   node tools/level-check.js --keep     keep the rendered WAV/trace (build/.level/)
//   node tools/level-check.js <file.wav> measure ONE wav (peak / RMS / crest)
//   node tools/level-check.js --selfcheck  known answers for the MEASUREMENT (renders no cart)
//
// WHY a baseline (unlike tune-check): pitch has a mathematically exact target (A440), so a
// note is right or wrong on its own. LEVEL has no absolute truth — the question is whether a
// voice DRIFTED from where it was. So the gate compares against a committed golden render:
// after a sound.h edit, `--quiet` flags any (engine,note) whose level moved more than the
// threshold. Re-bless intended changes with `--save`. This catches the silent regression a
// compile + tune-check + dc-check all miss: an engine that got louder/quieter, or one now
// slamming the master limiter (crest collapse) so its dynamics are squashed — none of which
// changes whether it compiles or what pitch it plays. Render is deterministic (--det).
//
// Three absolute checks need no baseline (a fresh engine with no golden entry still gets these):
//   • SILENT  — peak below the noise floor (a broken/mis-wired voice that makes no sound)
//   • HOT     — a single sustained note peaking near full-scale (one voice alone is too loud;
//               a chord of these will live in the limiter)
//   • crest   — peak/RMS ratio; a very low crest on a sustained tone = limiter squash / no headroom
//
// THE CONTROLS — "level has no absolute truth" is true of LEVEL and false of SHAPE and GEOMETRY.
// That gap is where this gate's five free controls live; none of them needs a golden file. They run
// before --save as well as before a verdict, because blessing a baseline from a broken measurement
// freezes the fault into the golden file, after which it no longer looks like a fault.
//   Structural, from the sweep's own geometry:
//   1. a frame is exactly sr/60 samples (the render is frame-locked), so every note window is
//      derived from that one number and a wrong one silently reads the wrong samples;
//   2. every note is gated for the same number of frames, so an odd one out ran into the end;
//   3. the render outlasts the sweep by at least one note period — a note that is never rendered
//      leaves nothing behind to notice, which is how the differential pass went missing for weeks.
//   Arithmetic, from the control engine:
//   4. SINE's CREST is 20·log10(√2) = 3.0103 dB whatever the level. Measured 3.0110.
//   5. SINE's PEAK is identical at every pitch — one velocity, one waveform, four octaves, spread
//      0.0000 dB. Anything frequency-dependent leaking into the level path shows up here.
//
// WHAT THEY ARE BLIND TO, measured rather than assumed — because a control believed to cover more
// than it does is worse than none:
//   • crest is a RATIO, so a uniform gain error cancels out of it exactly. A WAV reader with the
//     wrong scale factor passes 4 and 5. That class is the baseline's, and only the baseline's.
//   • the two SINE controls do NOT catch a sliding window, though it is tempting to think they must.
//     Mutation-tested: a 2% samples-per-frame error left SINE reading crest 3.0 dB, peak -14.0 dBFS
//     and ZERO drift while it wrecked eleven other engines — because SINE is the FIRST entry in the
//     sweep, where the accumulated offset is a couple of frames, and a stationary sine is the least
//     window-sensitive signal there is. Control 1 is what actually catches that, and it exists
//     because the SINE pair was tested against the failure it was supposed to cover and did nothing.

const fs = require('fs')
const path = require('path')
const { spawnSync } = require('child_process')

const ROOT = path.resolve(__dirname, '..')
const BASELINE = path.join(ROOT, 'tools', 'level-baseline.json')

// thresholds (dB). a few tenths is render noise; flag a comma's worth, scream past a quarter.
const WARN_DB = 1.5    // level drift vs baseline — worth a look
const BAD_DB  = 4.0    // level drift vs baseline — almost certainly a regression
// absolute (baseline-free) flags
const SILENT_DBFS = -60   // peak below this = no sound (broken voice)
const HOT_DBFS    = -2.0  // a single note peaking above this = too hot on its own (two clip)
const SQUASH_CREST_DB = 6.0   // crest below this AND a hot peak = the master limiter is squashing it
const SQUASH_PEAK_DBFS = -3.0 // squash only counts when the peak is near the limiter knee (~-1.9 dBFS)
// per-engine loudness-consistency: flag a voice whose level sits this far off the library median
const OUTLIER_DB = 9.0
// ── the controls (see the header). Bounds are ~50x the observed deviation, which is not slack:
// these exist to catch a measurement that has come apart, and a measurement that has come apart
// misses by dB, not by thousandths. A tight bound here would only buy false alarms.
const CONTROL_ENGINE = 4          // INSTR_SINE
const CONTROL_CREST_DB = 3.0103   // 20*log10(sqrt(2)) — a sine's crest, exactly
const CONTROL_CREST_TOL = 0.05    // measured deviation from theory: 0.0009 dB
const CONTROL_PITCH_SPREAD = 0.05 // max-min SINE peak across the four octaves; measured 0.0000 dB

// engine id → label, mirrors the INSTR_* block in runtime/studio.h (same map as tune-check.js)
const ENGINE_NAMES = {
  0: 'SQUARE', 1: 'SAW', 2: 'TRI', 3: 'NOISE', 4: 'SINE (control)',
  16: 'PLUCK  karplus-strong', 17: 'MALLET struck bar', 18: 'FM 2-op',
  19: 'ORGAN tonewheel', 20: 'EPIANO rhodes/wurli', 21: 'PD casio-cz',
  22: 'MEMBRANE drum', 23: 'REED clarinet/sax', 24: 'VOICE formant',
  25: 'PIPE flute', 26: 'GUITAR plucked+body', 27: 'PIANO stiff-string',
  28: 'BOWED violin/cello', 29: 'BRASS lip-reed',
}
// engines that DECAY within the gate (so a low crest / dying tail is expected, not a fault)
const DECAYING = new Set([16, 17, 18, 20, 26, 27])

const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
const midiName = (m) => `${NOTE_NAMES[m % 12]}${Math.floor(m / 12) - 1}`
const dbfs = (x) => x <= 0 ? -Infinity : 20 * Math.log10(x)
const fmtDb = (d) => d === -Infinity ? '  -inf' : `${d >= 0 ? '+' : ''}${d.toFixed(1)}`.padStart(6)

// ── WAV (16-bit PCM, mono or stereo→mono) — same reader as tune-check.js ─────
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

// peak (full window) + RMS (steady middle, skipping the attack transient and the dying tail)
function measureLevel(s, a, b) {
  let peak = 0
  for (let i = a; i < b; i++) { const v = Math.abs(s[i]); if (v > peak) peak = v }
  const span = b - a
  const m0 = Math.floor(a + span * 0.12), m1 = Math.floor(a + span * 0.88)
  let sum = 0, n = 0
  for (let i = m0; i < m1; i++) { sum += s[i] * s[i]; n++ }
  const rms = n ? Math.sqrt(sum / n) : 0
  return { peakDb: dbfs(peak), rmsDb: dbfs(rms), crestDb: dbfs(peak) - dbfs(rms) }
}

// ── trace → note windows (same contract as tune-check.js: eng/emidi/gate) ────
function noteWindows(traceFile) {
  const lines = fs.readFileSync(traceFile, 'utf8').trim().split('\n')
  const notes = []
  let cur = null, lastFrame = 0
  for (const ln of lines) {
    let row; try { row = JSON.parse(ln) } catch { continue }
    if (row.vev !== undefined) continue   // skip voice-trace events (-DDE_TRACE): they share the trace JSONL but carry no gate/window info (would close a note window early)
    const w = row.w || {}
    const gate = +w.gate, eng = +w.eng, emidi = +w.emidi, f = row.f
    // THE SWEEP PLAYS PIANO TWICE. Its last entry is the same INSTR_* id with MODE_PIANO_STRETCH
    // forced off (tunecheck.c's differential pass), so `eng` alone cannot tell the two apart —
    // which is why the cart publishes `et`. This file used to ignore it, and the cost was not
    // theoretical: both passes collapsed onto the baseline key `27:45`, last-write-won, and the
    // NORMAL pass was silently diffed against the STRETCH-OFF one. That reported a standing
    // phantom drift of +0.5 / +0.7 dB peak and 1.2 dB rms on PIANO — 82% of the warn budget spent
    // on a difference between two renders, with the real drift hidden underneath it.
    const et = w.et === undefined ? 0 : +w.et
    lastFrame = f
    if (gate === 1 && emidi > 0) {
      if (cur && cur.eng === eng && cur.midi === emidi && cur.et === et) cur.f1 = f
      else { if (cur) notes.push(cur); cur = { eng, midi: emidi, et, f0: f, f1: f } }
    } else if (cur) { notes.push(cur); cur = null }
  }
  if (cur) notes.push(cur)
  return { notes, lastFrame }
}

function runPlay(cart, frames, wav, trace) {
  // --det makes the render byte-reproducible, so the golden baseline is stable across runs.
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
  const dir = path.join(ROOT, 'build', '.level')
  fs.mkdirSync(dir, { recursive: true })
  const wav = path.join(dir, 'sweep.wav'), trace = path.join(dir, 'sweep.trace.jsonl')
  // 14 sweep entries (13 engines + PIANO's stretch-off differential pass) × 4 pitches × 62 frames
  // = 3472. Over-run is harmless — the analyzer is trace-driven — but UNDER-run silently truncates
  // the LAST entries, and the last entry is the differential pass. This said 3400 with a comment
  // that counted 13 entries, so the sweep had been quietly stopping one note short of the end since
  // the pass was added: the ET A5 note was never rendered and never missed. Same fix, same number,
  // same reasoning as tune-check.js — the two renderSweep()s must not drift apart again.
  runPlay('tunecheck', 3700, wav, trace)
  return { wav, trace, dir }
}

function analyzeRender(wav, trace) {
  const { sr, s } = readWavMono(wav)
  const { notes, lastFrame } = noteWindows(trace)
  if (!notes.length) throw new Error('no gated notes found in trace — did the cart build?')
  const spf = s.length / (lastFrame + 1)   // samples per frame (audio is frame-locked)
  const results = []
  for (const nt of notes) {
    const a = Math.floor(nt.f0 * spf), b = Math.floor(nt.f1 * spf)
    const lv = measureLevel(s, a, b)
    results.push({
      eng: nt.eng, et: nt.et,
      // the differential pass gets its own label, or the report prints the same engine heading
      // twice with no way to tell which block is which
      engineName: (ENGINE_NAMES[nt.eng] || `INSTR ${nt.eng}`) + (nt.et ? '  · stretch-off differential' : ''),
      midi: nt.midi, note: midiName(nt.midi),
      peakDb: +lv.peakDb.toFixed(2), rmsDb: +lv.rmsDb.toFixed(2), crestDb: +lv.crestDb.toFixed(2),
    })
  }
  return { results, sr, meta: { sr, spf, notes, lastFrame } }
}

// ── baseline ─────────────────────────────────────────────────────────────────
// the `:et` suffix only appears on the differential pass, so every ordinary key is unchanged from
// the pre-fix baseline and re-blessing does not churn 52 untouched rows
const key = (r) => `${r.eng}:${r.midi}${r.et ? ':et' : ''}`
function loadBaseline() {
  if (!fs.existsSync(BASELINE)) return null
  const j = JSON.parse(fs.readFileSync(BASELINE, 'utf8'))
  const map = new Map()
  for (const n of j.notes) map.set(key(n), n)
  return map
}
function saveBaseline(results) {
  const out = {
    note: 'golden engine levels from tools/carts/tunecheck.c — regenerate with `node tools/level-check.js --save`',
    generated: 'commit this file; level-check --quiet diffs against it (the regression gate)',
    thresholdDb: { warn: WARN_DB, bad: BAD_DB },
    notes: results.map(r => ({ eng: r.eng, midi: r.midi, et: r.et || 0, peakDb: r.peakDb, rmsDb: r.rmsDb })),
  }
  fs.writeFileSync(BASELINE, JSON.stringify(out, null, 2) + '\n')
  const keys = new Set(results.map(key))
  if (keys.size !== results.length)   // the ET collision, generalised: a golden file cannot hold
    console.log(`  ⚠ ${results.length - keys.size} DUPLICATE key(s) — the baseline keeps the last`)
  console.log(`✓ wrote baseline for ${results.length} notes → ${path.relative(ROOT, BASELINE)}`)
}

// ── verdicts ───────────────────────────────────────────────────────────────
// each note collects zero or more issues; the worst severity drives its mark.
function assess(results, baseline) {
  // library median peak, for the loudness-consistency (outlier) check
  const peaks = results.map(r => r.peakDb).filter(d => d > SILENT_DBFS).sort((a, b) => a - b)
  const medPeak = peaks.length ? peaks[Math.floor(peaks.length / 2)] : -Infinity
  for (const r of results) {
    r.issues = []; r.sev = 'ok'
    const bump = (sev) => { if (sev === 'bad' || (sev === 'warn' && r.sev === 'ok')) r.sev = sev }
    // absolute: silence
    if (r.peakDb < SILENT_DBFS) { r.issues.push('SILENT (no output)'); bump('bad') }
    else {
      // absolute: too hot on its own
      if (r.peakDb > HOT_DBFS) { r.issues.push(`HOT ${fmtDb(r.peakDb).trim()} dBFS`); bump('warn') }
      // absolute: limiter squash — a HOT peak (near the knee) whose crest has also collapsed.
      // a low crest on a quiet tone is just a dense waveform (REED < a sine), not squash.
      if (!DECAYING.has(r.eng) && r.peakDb > SQUASH_PEAK_DBFS && r.crestDb < SQUASH_CREST_DB)
        { r.issues.push(`limiter squash (peak ${r.peakDb.toFixed(1)} dBFS, crest ${r.crestDb.toFixed(1)} dB)`); bump('warn') }
      // absolute: loudness outlier vs the rest of the library
      const off = r.peakDb - medPeak
      if (Math.abs(off) > OUTLIER_DB) { r.issues.push(`${off > 0 ? '+' : ''}${off.toFixed(1)} dB vs library median`); bump('warn') }
    }
    // regression vs baseline
    if (baseline) {
      const base = baseline.get(key(r))
      if (!base) { r.issues.push('new (no baseline)'); }
      else {
        const dP = r.peakDb - base.peakDb, dR = r.rmsDb - base.rmsDb
        r.dPeak = +dP.toFixed(2); r.dRms = +dR.toFixed(2)
        const worst = Math.max(Math.abs(dP), Math.abs(dR))
        if (worst > BAD_DB)  { r.issues.push(`DRIFT peak ${fmtDb(dP).trim()} / rms ${fmtDb(dR).trim()} dB vs baseline`); bump('bad') }
        else if (worst > WARN_DB) { r.issues.push(`drift peak ${fmtDb(dP).trim()} / rms ${fmtDb(dR).trim()} dB vs baseline`); bump('warn') }
      }
    }
  }
  return { medPeak }
}

// ── the controls ─────────────────────────────────────────────────────────────
// Read the header for what these are and what they cannot see. Returns [] when the measurement is
// trustworthy; anything in the list means the numbers above it are void, not that a voice is wrong.
function controlCheck(results, meta) {
  const bad = []
  // ── the STRUCTURAL half: three things the sweep's own geometry fixes, with no golden file ──
  if (meta) {
    // (1) the engine is frame-locked at 60fps, so a frame is exactly sr/60 samples. Every note
    // window is derived from this number, so if it is wrong every window in the report is reading
    // the wrong samples — which is the failure the two SINE controls below CANNOT see (see header).
    const want = meta.sr / 60
    if (Math.abs(meta.spf - want) > 0.5)
      bad.push(`${meta.spf.toFixed(1)} samples per frame, but a 60fps render at ${meta.sr}Hz is ${want}`)
    // (2) the sweep gates every note for the same number of frames. An odd one out means a window
    // ran into the end of the render, so that note was measured on a truncated signal.
    const lens = new Set(meta.notes.map(n => n.f1 - n.f0))
    if (lens.size > 1) bad.push(`note windows are not all the same length (${[...lens].sort((a, b) => a - b).join(', ')} frames)`)
    // (3) …and the render must outlast the sweep. If it stops within a note period of the last
    // note, later entries were never rendered at all — which is exactly how the differential pass
    // went missing for weeks. A note that never plays leaves NO trace of itself to notice.
    const last = meta.notes[meta.notes.length - 1]
    const period = meta.notes.length > 1 ? meta.notes[1].f0 - meta.notes[0].f0 : 62
    if (meta.lastFrame - last.f1 < period)
      bad.push(`the render ends ${meta.lastFrame - last.f1} frames after the last note (< one ${period}-frame period): the sweep is TRUNCATED`)
  }
  // ── the SINE half: the waveform's own arithmetic ──
  const sine = results.filter(r => r.eng === CONTROL_ENGINE && !r.et)
  if (!sine.length) return bad.concat(['the SINE control never played — the sweep did not run as expected'])
  for (const r of sine) {
    const off = Math.abs(r.crestDb - CONTROL_CREST_DB)
    if (off > CONTROL_CREST_TOL)
      bad.push(`SINE ${r.note} crest ${r.crestDb.toFixed(3)} dB, a sine's is ${CONTROL_CREST_DB} (off by ${off.toFixed(3)})`)
  }
  const peaks = sine.map(r => r.peakDb)
  const spread = Math.max(...peaks) - Math.min(...peaks)
  if (spread > CONTROL_PITCH_SPREAD)
    bad.push(`SINE peak varies ${spread.toFixed(3)} dB across four octaves; one velocity on one waveform must not`)
  return bad
}

const mark = (sev) => sev === 'bad' ? '✗' : sev === 'warn' ? '⚠' : '·'

function printResults(results, sr, medPeak, hasBaseline) {
  console.log(`level sweep — ${results.length} notes @ ${sr}Hz   library median peak ${medPeak.toFixed(1)} dBFS`
    + `   (warn >${WARN_DB} dB drift, bad >${BAD_DB} dB)${hasBaseline ? '' : '   ⚠ NO BASELINE — run --save'}\n`)
  let lastEng = null
  for (const r of results) {
    if (r.eng !== lastEng) { console.log(`${r.engineName}  (id ${r.eng})`); lastEng = r.eng }
    const drift = r.dPeak === undefined ? '' : `   Δpk ${fmtDb(r.dPeak)} Δrms ${fmtDb(r.dRms)}`
    const note = r.issues.length ? `   ${r.issues.join('; ')}` : ''
    console.log(`  ${mark(r.sev)} ${r.note.padEnd(3)}  peak ${fmtDb(r.peakDb)}  rms ${fmtDb(r.rmsDb)}  crest ${r.crestDb.toFixed(1).padStart(5)} dB${drift}${note}`)
  }
  const flagged = results.filter(r => r.sev !== 'ok').sort((a, b) => (a.sev === b.sev ? 0 : a.sev === 'bad' ? -1 : 1))
  console.log()
  if (!flagged.length) console.log('✓ every engine is within level tolerance')
  else {
    console.log(`${flagged.length} note(s) flagged (worst first):`)
    for (const r of flagged) console.log(`  ${mark(r.sev)} ${r.engineName} ${r.note}  —  ${r.issues.join('; ')}`)
  }
}

function run(opts) {
  const { wav, trace, dir } = renderSweep(opts.keep)
  const { results, sr, meta } = analyzeRender(wav, trace)
  if (!opts.keep) fs.rmSync(dir, { recursive: true, force: true })
  // ⚠ the controls run BEFORE --save too: blessing a baseline from a broken measurement would
  // freeze the fault into the golden file, where it stops looking like a fault at all
  const preControl = controlCheck(results, meta)
  if (opts.save) {
    if (preControl.length) {
      console.error('✗ REFUSING TO BLESS — the measurement is off, not the engines:')
      for (const c of preControl) console.error(`    ${c}`)
      return { results, control: preControl, bad: preControl.length }
    }
    saveBaseline(results); return { results, control: [], bad: 0 }
  }
  const baseline = loadBaseline()
  const { medPeak } = assess(results, baseline)
  const control = preControl
  if (opts.json) { console.log(JSON.stringify({ control, notes: results }, null, 2)); }
  else {
    printResults(results, sr, medPeak, !!baseline)
    if (control.length) {
      console.error('\n✗ THE MEASUREMENT IS OFF, NOT THE ENGINES:')
      for (const c of control) console.error(`    ${c}`)
      console.error('  Every level above is suspect. Start with `node tools/level-check.js --selfcheck`.')
    }
  }
  const bad = results.filter(r => r.sev === 'bad').length + control.length
  return { results, control, bad }
}

// ── --selfcheck: KNOWN ANSWERS FOR THE MEASUREMENT ───────────────────────────
// Renders no cart. Every number below is arithmetic or was measured before it was asserted, and
// the suite is mutation-tested: breaking measureLevel's RMS slice, dropping `et` from the window
// key, and widening the control tolerance each turn a documented set of these red.
function selfcheck() {
  const SR = 44100
  let pass = 0, fail = 0
  const ok = (name, cond, got) => {
    if (cond) { pass++; console.log(`  ✓ ${name}`) } else { fail++; console.log(`  ✗ ${name}   got: ${got}`) }
  }
  const gen = (secs, f) => { const n = Math.floor(SR * secs), x = new Float64Array(n)
    for (let i = 0; i < n; i++) x[i] = f(i, n); return x }
  const sine = (amp, hz) => (i) => amp * Math.sin(2 * Math.PI * hz * i / SR)
  const lv = (x) => measureLevel(x, 0, x.length)

  console.log('level-check --selfcheck — known answers for the measurement (no cart is rendered)\n')

  console.log('THE ARITHMETIC')
  // 441 Hz at 44100 = a 100-sample period, so the peak sample lands exactly on the crest
  const s5 = lv(gen(0.5, sine(0.5, 441)))
  ok('a 0.5-amplitude sine peaks at -6.02 dBFS', Math.abs(s5.peakDb + 6.0206) < 0.001, s5.peakDb)
  ok('  …its rms is -9.03 dBFS (peak/sqrt2)', Math.abs(s5.rmsDb + 9.0329) < 0.005, s5.rmsDb)
  ok('  …so its crest is 3.01 dB', Math.abs(s5.crestDb - 3.0103) < 0.01, s5.crestDb)
  const s10 = lv(gen(0.5, sine(1.0, 441)))
  ok('doubling the amplitude adds exactly 6.02 dB to peak',
     Math.abs((s10.peakDb - s5.peakDb) - 6.0206) < 0.001, s10.peakDb - s5.peakDb)
  ok('  …and leaves crest untouched (it is a ratio)',
     Math.abs(s10.crestDb - s5.crestDb) < 1e-9, s10.crestDb - s5.crestDb)
  const sq = lv(gen(0.5, (i) => ((441 * i / SR) % 1) < 0.5 ? 0.5 : -0.5))
  ok('a square wave has crest 0 dB (peak == rms)', Math.abs(sq.crestDb) < 1e-9, sq.crestDb)

  console.log('\nTHE RMS SLICE SKIPS THE ATTACK — which is the whole point of measuring both')
  const spike = lv(gen(0.5, (i, n) => i === Math.floor(n * 0.02) ? 0.9 : 0.01 * Math.sin(2 * Math.PI * 441 * i / SR)))
  ok('a spike inside the first 12% is seen by PEAK', spike.peakDb > -1.0, spike.peakDb)
  ok('  …and NOT by rms, which reads the quiet body', spike.rmsDb < -40, spike.rmsDb)
  // the pair above passes even if the slice is removed entirely (one loud sample barely moves an
  // rms over 22050), which mutation-testing exposed. THIS pins the 12/88 boundaries themselves:
  // energy only in the outer edges, silence in the middle, so the slice reads exactly nothing.
  const edges = lv(gen(0.5, (i, n) => (i < n * 0.11 || i > n * 0.89) ? 0.7 * Math.sin(2 * Math.PI * 441 * i / SR) : 0))
  ok('energy ONLY outside the 12–88% slice reads peak loud and rms at -inf',
     edges.peakDb > -4 && edges.rmsDb === -Infinity, `${edges.peakDb} / ${edges.rmsDb}`)
  const dec = lv(gen(0.5, (i, n) => Math.exp(-6 * i / n) * 0.8 * Math.sin(2 * Math.PI * 441 * i / SR)))
  ok('a decaying note therefore shows a large crest (~18.8 dB), and is not a fault',
     dec.crestDb > 15 && dec.crestDb < 22, dec.crestDb)
  ok('  …which is why the DECAYING set exists', DECAYING.has(16) && DECAYING.has(27) && !DECAYING.has(4),
     [...DECAYING].join(','))

  console.log('\nIT REPORTS SILENCE AS SILENCE')
  const sil = lv(gen(0.1, () => 0))
  ok('digital silence peaks at -inf', sil.peakDb === -Infinity, sil.peakDb)
  // pinned as a CHARACTERISTIC, not endorsed: -Inf minus -Inf is NaN. assess() reaches the SILENT
  // branch first and never prints a crest for such a note, so this never surfaces — but a future
  // caller reading crestDb directly needs to know it is not a number.
  ok('  …and its crest is NaN (-inf minus -inf), which SILENT catches before anyone reads it',
     Number.isNaN(sil.crestDb), sil.crestDb)

  console.log('\nTHE DIFFERENTIAL PASS IS A SEPARATE NOTE — the defect this file shipped with')
  const os = require('os')
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'levelcheck-selfcheck-'))
  const traceOf = (rows) => { const f = path.join(dir, `t${rows.length}-${Math.abs(rows[0].w.et)}.jsonl`)
    fs.writeFileSync(f, rows.map(r => JSON.stringify(r)).join('\n') + '\n'); return f }
  const row = (f, eng, midi, gate, et) => ({ f, w: { eng, emidi: midi, gate, et } })
  // PIANO A2 twice back to back, stretch on then off — exactly tunecheck.c's shape
  const both = traceOf([...Array(4)].map((_, i) => row(i, 27, 45, 1, 0))
    .concat([row(4, 27, 45, 0, 0)])
    .concat([...Array(4)].map((_, i) => row(5 + i, 27, 45, 1, 1))))
  const w = noteWindows(both).notes
  ok('two PIANO A2 runs with different `et` are TWO windows', w.length === 2, w.length)
  ok('  …and they get DIFFERENT baseline keys', key(w[0]) !== key(w[1]), `${key(w[0])} vs ${key(w[1])}`)
  ok('  …the normal pass keeps its old key, so re-blessing does not churn 52 rows',
     key({ eng: 27, midi: 45, et: 0 }) === '27:45', key({ eng: 27, midi: 45, et: 0 }))
  // the regression this guards: keyed without `et` both rows collapse and last-write-wins
  ok('  …keyed WITHOUT et they would collide (the shipped bug)',
     `${w[0].eng}:${w[0].midi}` === `${w[1].eng}:${w[1].midi}`, 'keys differed')
  const noEt = traceOf([...Array(3)].map((_, i) => ({ f: i, w: { eng: 4, emidi: 69, gate: 1 } })))
  ok('a trace with no `et` field at all is treated as the normal pass',
     noteWindows(noEt).notes[0].et === 0, noteWindows(noEt).notes[0].et)
  const split = traceOf([row(0, 4, 69, 1, 0), row(1, 4, 69, 0, 0), row(2, 4, 69, 1, 0)])
  ok('a gate drop splits one engine+pitch into two windows', noteWindows(split).notes.length === 2,
     noteWindows(split).notes.length)
  const vev = traceOf([row(0, 4, 69, 1, 0), { f: 1, vev: 'on', w: {} }, row(2, 4, 69, 1, 0)])
  ok('voice-trace (vev) rows are skipped, not read as a gate drop', noteWindows(vev).notes.length === 1,
     noteWindows(vev).notes.length)
  fs.rmSync(dir, { recursive: true, force: true })

  console.log('\nTHE STRUCTURAL CONTROLS — the half that catches a sliding window')
  // the real sweep's geometry: 56 notes, gated 47 frames each, one every 62, render to 3699
  const sweepMeta = (o = {}) => {
    const n = o.count || 56
    const notes = [...Array(n)].map((_, i) => ({ f0: i * 62, f1: i * 62 + (o.shortLast && i === n - 1 ? 30 : 47) }))
    return { sr: 44100, spf: 735, notes, lastFrame: o.lastFrame === undefined ? 3699 : o.lastFrame, ...o.over }
  }
  const okSine = [{ eng: 4, et: 0, note: 'A2', peakDb: -14, rmsDb: -17.01, crestDb: 3.011 }]
  const structural = (m) => controlCheck(okSine, m)
  ok('a healthy sweep passes every structural control', structural(sweepMeta()).length === 0,
     structural(sweepMeta()))
  ok('samples-per-frame off by 2% is caught (the one the SINE pair misses)',
     structural(sweepMeta({ over: { spf: 749.7 } })).some(m => m.includes('samples per frame')), 'silent')
  ok('  …and 735 at 44100Hz is what a 60fps render must be',
     structural(sweepMeta({ over: { spf: 735.4 } })).length === 0, 'flagged a rounding-sized offset')
  ok('a note window shorter than its neighbours is caught',
     structural(sweepMeta({ shortLast: true })).some(m => m.includes('same length')), 'silent')
  ok('a render that stops right after the last note is TRUNCATED',
     structural(sweepMeta({ lastFrame: 56 * 62 - 15 })).some(m => m.includes('TRUNCATED')), 'silent')
  ok('  …which is the defect that hid the differential pass, and it now cannot come back quietly',
     structural(sweepMeta({ count: 55, lastFrame: 3399 })).some(m => m.includes('TRUNCATED')), 'silent')

  console.log('\nTHE SINE CONTROLS, BOTH DIRECTIONS')
  const sineSet = (crest, peaks) => peaks.map((p, i) => ({
    eng: CONTROL_ENGINE, et: 0, midi: 45 + i * 12, note: `A${2 + i}`, peakDb: p, rmsDb: p - crest, crestDb: crest }))
  ok('a real SINE control passes', controlCheck(sineSet(3.011, [-13.98, -13.98, -13.98, -13.98])).length === 0,
     controlCheck(sineSet(3.011, [-13.98, -13.98, -13.98, -13.98])))
  ok('a crest 1 dB off is caught', controlCheck(sineSet(4.0, [-14, -14, -14, -14])).length > 0, 'silent')
  ok('  …and so is one 0.2 dB off (the bound is 0.05)',
     controlCheck(sineSet(3.21, [-14, -14, -14, -14])).length > 0, 'silent')
  ok('a pitch-DEPENDENT peak is caught with no baseline involved',
     controlCheck(sineSet(3.011, [-14.0, -14.3, -15.1, -16.4])).some(m => m.includes('four octaves')), 'silent')
  ok('  …while a uniform level change is NOT (that is the baseline\'s job, said honestly)',
     controlCheck(sineSet(3.011, [-20, -20, -20, -20])).length === 0, 'flagged')
  ok('a sweep with no SINE at all fails loudly rather than passing empty',
     controlCheck([{ eng: 27, et: 0, peakDb: -18, crestDb: 20 }]).length === 1, 'silent')
  ok('the control bound is far tighter than the drift thresholds it sits beside',
     CONTROL_CREST_TOL < WARN_DB / 10, `${CONTROL_CREST_TOL} vs ${WARN_DB}`)

  console.log('\nTHE VERDICTS, INCLUDING THE TWO THAT HAVE NEVER FIRED ON A REAL RUN')
  const note = (o) => ({ eng: 19, et: 0, midi: 69, note: 'A4', peakDb: -18, rmsDb: -24, crestDb: 6, ...o })
  const sev = (o, base) => { const rs = [note(o)]; assess(rs, base || null); return rs[0] }
  ok('a normal note is ok', sev({}).sev === 'ok', sev({}).issues)
  ok('a note under the noise floor is SILENT and bad', sev({ peakDb: -80 }).sev === 'bad', sev({ peakDb: -80 }).issues)
  ok('a note peaking above -2 dBFS is HOT and warns', sev({ peakDb: -1.0 }).issues.some(i => i.startsWith('HOT')), 'silent')
  ok('a hot peak with a collapsed crest is limiter squash',
     sev({ peakDb: -2.5, rmsDb: -6.0, crestDb: 3.5 }).issues.some(i => i.includes('squash')), 'silent')
  ok('  …but the same numbers on a DECAYING engine are not (it rings down by design)',
     !sev({ eng: 16, peakDb: -2.5, rmsDb: -6.0, crestDb: 3.5 }).issues.some(i => i.includes('squash')), 'flagged')
  ok('  …and a low crest on a QUIET tone is not squash either (a dense waveform)',
     !sev({ peakDb: -20, crestDb: 2.0 }).issues.some(i => i.includes('squash')), 'flagged')
  // outlier is measured against the median of the batch, so it needs more than one note
  const batch = [note({ peakDb: -18 }), note({ midi: 45, peakDb: -18 }), note({ midi: 57, peakDb: -4.0 })]
  assess(batch, null)
  ok('a voice far off the library median is an outlier',
     batch[2].issues.some(i => i.includes('library median')), batch[2].issues)
  const base = new Map([['19:69', { eng: 19, midi: 69, peakDb: -18, rmsDb: -24 }]])
  ok('drift under the warn threshold stays ok', sev({ peakDb: -19.0 }, base).sev === 'ok', sev({ peakDb: -19.0 }, base).issues)
  ok('drift past the warn threshold warns', sev({ peakDb: -20.0 }, base).sev === 'warn', sev({ peakDb: -20.0 }, base).issues)
  ok('drift past the bad threshold is bad', sev({ peakDb: -24.0 }, base).sev === 'bad', sev({ peakDb: -24.0 }, base).issues)
  ok('a note with no baseline entry says so and does NOT fail the gate',
     sev({ eng: 99 }, base).sev === 'ok' && sev({ eng: 99 }, base).issues.includes('new (no baseline)'),
     sev({ eng: 99 }, base).issues)

  console.log(`\n${fail ? '✗' : '✓'} ${pass} passed, ${fail} failed`)
  return fail ? 1 : 0
}

// ── single-WAV mode ───────────────────────────────────────────────────────────
function single(file, json) {
  const { sr, s } = readWavMono(file)
  const lv = measureLevel(s, 0, s.length)
  const out = { file, sr, peakDb: +lv.peakDb.toFixed(2), rmsDb: +lv.rmsDb.toFixed(2), crestDb: +lv.crestDb.toFixed(2) }
  if (json) console.log(JSON.stringify(out, null, 2))
  else console.log(`${file} @ ${sr}Hz\n  peak ${fmtDb(out.peakDb)} dBFS   rms ${fmtDb(out.rmsDb)} dBFS   crest ${out.crestDb.toFixed(1)} dB`)
  return out
}

// ── cli ────────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2)
const json = argv.includes('--json')
const keep = argv.includes('--keep')
const quiet = argv.includes('--quiet')
const save = argv.includes('--save')
const positional = argv.filter(a => !a.startsWith('--'))

if (argv.includes('--selfcheck')) process.exit(selfcheck())

try {
  if (positional.length && positional[0].endsWith('.wav')) {
    single(positional[0], json)
  } else {
    const { bad } = run({ json, keep, save })
    if (quiet) process.exit(bad ? 1 : 0)
  }
} catch (e) { console.error('level-check:', e.message); process.exit(2) }
