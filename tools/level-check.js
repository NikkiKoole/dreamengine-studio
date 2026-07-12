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
    lastFrame = f
    if (gate === 1 && emidi > 0) {
      if (cur && cur.eng === eng && cur.midi === emidi) cur.f1 = f
      else { if (cur) notes.push(cur); cur = { eng, midi: emidi, f0: f, f1: f } }
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
  runPlay('tunecheck', 3400, wav, trace)   // 13 engines × 4 pitches × 62 frames ≈ 3224; over-run is harmless
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
      eng: nt.eng, engineName: ENGINE_NAMES[nt.eng] || `INSTR ${nt.eng}`,
      midi: nt.midi, note: midiName(nt.midi),
      peakDb: +lv.peakDb.toFixed(2), rmsDb: +lv.rmsDb.toFixed(2), crestDb: +lv.crestDb.toFixed(2),
    })
  }
  return { results, sr }
}

// ── baseline ─────────────────────────────────────────────────────────────────
const key = (r) => `${r.eng}:${r.midi}`
function loadBaseline() {
  if (!fs.existsSync(BASELINE)) return null
  const j = JSON.parse(fs.readFileSync(BASELINE, 'utf8'))
  const map = new Map()
  for (const n of j.notes) map.set(`${n.eng}:${n.midi}`, n)
  return map
}
function saveBaseline(results) {
  const out = {
    note: 'golden engine levels from tools/carts/tunecheck.c — regenerate with `node tools/level-check.js --save`',
    generated: 'commit this file; level-check --quiet diffs against it (the regression gate)',
    thresholdDb: { warn: WARN_DB, bad: BAD_DB },
    notes: results.map(r => ({ eng: r.eng, midi: r.midi, peakDb: r.peakDb, rmsDb: r.rmsDb })),
  }
  fs.writeFileSync(BASELINE, JSON.stringify(out, null, 2) + '\n')
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
  const { results, sr } = analyzeRender(wav, trace)
  if (!opts.keep) fs.rmSync(dir, { recursive: true, force: true })
  if (opts.save) { saveBaseline(results); return { results, bad: 0 } }
  const baseline = loadBaseline()
  const { medPeak } = assess(results, baseline)
  if (opts.json) { console.log(JSON.stringify(results, null, 2)); }
  else printResults(results, sr, medPeak, !!baseline)
  const bad = results.filter(r => r.sev === 'bad').length
  return { results, bad }
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

try {
  if (positional.length && positional[0].endsWith('.wav')) {
    single(positional[0], json)
  } else {
    const { bad } = run({ json, keep, save })
    if (quiet) process.exit(bad ? 1 : 0)
  }
} catch (e) { console.error('level-check:', e.message); process.exit(2) }
