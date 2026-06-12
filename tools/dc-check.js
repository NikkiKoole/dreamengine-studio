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
    let sum = 0, sq = 0, n = 0
    for (let i = a; i < b && i < s.length; i++) { sum += s[i]; sq += s[i] * s[i]; n++ }
    if (n < 64) continue                                   // too short to trust
    const dc = sum / n, rms = Math.sqrt(sq / n)
    if (rms < 0.0015) continue                             // effectively silent — no signal to judge
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
const argv = process.argv.slice(2)
const json = argv.includes('--json')
const keep = argv.includes('--keep')
const quiet = argv.includes('--quiet')

try {
  const { wav, trace, dir } = renderSweep()
  const { results, sr } = analyze(wav, trace)
  if (!keep) fs.rmSync(dir, { recursive: true, force: true })
  if (json) console.log(JSON.stringify(perEngine(results), null, 2))
  else if (!quiet) printResults(results, sr)
  if (quiet) {
    const bad = perEngine(results).filter(r => r.verdict !== 'ok')
    if (bad.length) console.error(`dc-check: ${bad.length} engine(s) carry DC: ${bad.map(r => r.engineName.split(' ')[0]).join(', ')}`)
    process.exit(bad.length ? 1 : 0)
  }
} catch (e) { console.error('dc-check:', e.message); process.exit(2) }
