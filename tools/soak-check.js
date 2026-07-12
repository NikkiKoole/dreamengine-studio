#!/usr/bin/env node
// soak-check.js — the engine's LONG-RUN stability gate. tune/level/fx-check each render a few
// seconds; this renders ~64s of tools/carts/soak.c — cycles of dense note-firing through a big
// reverb+echo tail (STRESS) and silence (IDLE) — and asserts the things only a long run reveals:
//
//   • STRESS level stays STABLE across all cycles — late cycles ≈ early ones. A slow gain drift,
//     or a progressive VOICE LEAK starving the pool, shows as the stress level creeping cycle to cycle.
//   • IDLE DECAYS well below stress — the tails die; a leaked voice or a runaway feedback loop would
//     keep an idle window ringing instead.
//   • No cross-cycle ENERGY ACCUMULATION — the idle-tail floor doesn't climb run-long (a feedback
//     loop slowly piling up energy / DC).
//   • No blowup or NaN-collapse — every stress window still makes sound; peak stays bounded.
//
//   node tools/soak-check.js            render the long run + per-cycle stability report
//   node tools/soak-check.js --quiet    CI gate: exit 1 on drift / leak / accumulation / blowup
//   node tools/soak-check.js --json     machine-readable
//   node tools/soak-check.js --keep     keep the render (build/.soak/)
//
// Assertions are DECAY-RELATIVE (not an absolute silence floor) so they don't depend on exactly
// how long an aggressive tail takes to die. Pairs with the denormal flush-to-zero in sound.h: this
// proves the tails decay (the audible side); FTZ handles the audio-thread CPU side of that decay
// passing through the denormal range. Render is deterministic (--det).

const fs = require('fs')
const path = require('path')
const { spawnSync } = require('child_process')

const ROOT = path.resolve(__dirname, '..')

// thresholds (dB)
const STRESS_DRIFT_DB = 4.0    // spread of per-cycle stress RMS — more = drift or voice-pool starvation
const DECAY_MARGIN_DB = 12.0   // idle-late must sit at least this far below the same cycle's stress (tail is dying; healthy run decays 15-18, a stuck/leaked voice ~0-5)
const ACCUM_DB        = 4.0    // idle-late floor must not climb more than this across the run (no energy pile-up)
const ALIVE_DBFS      = -45.0  // a stress window quieter than this = the engine went silent (crash / total starvation)
const BLOWUP_DBFS     = -0.2   // sustained peak above this for a whole stress window = something runaway (limiter pinned)

const dbfs = (x) => x <= 0 ? -Infinity : 20 * Math.log10(x)
const fmtDb = (d) => d === -Infinity ? ' -inf' : `${d >= 0 ? '+' : ''}${d.toFixed(1)}`.padStart(6)

// ── WAV reader (same as the other audio gates) ───────────────────────────────
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

const peakOf = (s, a, b) => { let p = 0; for (let i = a; i < b; i++) { const v = Math.abs(s[i]); if (v > p) p = v } return p }
const rmsOf  = (s, a, b) => { let q = 0, n = 0; for (let i = a; i < b; i++) { q += s[i] * s[i]; n++ } return n ? Math.sqrt(q / n) : 0 }

// ── trace → (cycle, phase) windows ───────────────────────────────────────────
function windows(traceFile) {
  const lines = fs.readFileSync(traceFile, 'utf8').trim().split('\n')
  const wins = []
  let cur = null, lastFrame = 0
  for (const ln of lines) {
    let row; try { row = JSON.parse(ln) } catch { continue }
    if (row.vev !== undefined) continue   // skip voice-trace events (-DDE_TRACE): they share the trace JSONL but carry no cyc/phase window info (would split a window)
    const w = row.w || {}
    const cyc = +w.cyc, phase = +w.phase, f = row.f
    lastFrame = f
    if (cur && cur.cyc === cyc && cur.phase === phase) cur.f1 = f
    else { if (cur) wins.push(cur); cur = { cyc, phase, f0: f, f1: f } }
  }
  if (cur) wins.push(cur)
  return { wins, lastFrame }
}

function runPlay(frames, wav, trace) {
  const r = spawnSync('node',
    [path.join('tools', 'play.js'), 'soak', 'run', '--headless', '--det',
     '--frames', String(frames), '--trace', trace, '--wav', wav],
    { cwd: ROOT, encoding: 'utf8' })
  if (r.status !== 0) { process.stderr.write((r.stdout || '') + (r.stderr || '')); throw new Error('render failed (play.js soak)') }
}

function analyze() {
  const dir = path.join(ROOT, 'build', '.soak')
  fs.mkdirSync(dir, { recursive: true })
  const wav = path.join(dir, 'soak.wav'), trace = path.join(dir, 'soak.trace.jsonl')
  runPlay(24 * 160 + 300, wav, trace)   // soak.c: 24 cycles × (60 stress + 100 idle) + slack
  const { sr, s } = readWavMono(wav)
  const { wins, lastFrame } = windows(trace)
  const spf = s.length / (lastFrame + 1)
  // collect per-cycle stress + idle-late measurements
  const cycles = {}
  for (const w of wins) {
    if (w.cyc < 0) continue
    const a = Math.floor(w.f0 * spf), b = Math.floor(w.f1 * spf), span = b - a
    cycles[w.cyc] = cycles[w.cyc] || { cyc: w.cyc }
    if (w.phase === 1) {                                  // stress: measure the body
      cycles[w.cyc].stressRms = dbfs(rmsOf(s, Math.floor(a + span * 0.2), Math.floor(a + span * 0.9)))
      cycles[w.cyc].stressPeak = dbfs(peakOf(s, a, b))
    } else {                                              // idle: measure the LATE part (tail decayed)
      cycles[w.cyc].idleLateRms = dbfs(rmsOf(s, Math.floor(a + span * 0.55), Math.floor(a + span * 0.95)))
    }
  }
  const rows = Object.values(cycles).filter(c => c.stressRms !== undefined).sort((a, b) => a.cyc - b.cyc)
  return { dir, wav, trace, sr, rows }
}

function assess(rows) {
  const issues = []
  const stressRms = rows.map(r => r.stressRms)
  const idleLate  = rows.filter(r => r.idleLateRms !== undefined).map(r => r.idleLateRms)
  const spread = Math.max(...stressRms) - Math.min(...stressRms)
  // 1) stress level stable across cycles (drift / progressive starvation)
  if (spread > STRESS_DRIFT_DB) issues.push({ sev: 'bad', msg: `stress level drifts ${spread.toFixed(1)} dB across cycles (>${STRESS_DRIFT_DB}) — gain drift or voice-pool starvation` })
  // 2) engine still alive every cycle (no crash / total starvation)
  const dead = rows.filter(r => r.stressRms < ALIVE_DBFS)
  if (dead.length) issues.push({ sev: 'bad', msg: `${dead.length} stress cycle(s) went silent (< ${ALIVE_DBFS} dBFS) — crash or total voice starvation (first: cycle ${dead[0].cyc})` })
  // 3) idle decays below stress each cycle (tails die, no stuck/leaked voice ringing)
  const noDecay = rows.filter(r => r.idleLateRms !== undefined && r.idleLateRms > r.stressRms - DECAY_MARGIN_DB)
  if (noDecay.length) issues.push({ sev: 'bad', msg: `${noDecay.length} cycle(s) idle tail did not decay ≥ ${DECAY_MARGIN_DB} dB below stress — leaked voice or runaway tail (first: cycle ${noDecay[0].cyc})` })
  // 4) idle floor not climbing across the run (energy / DC accumulation)
  if (idleLate.length >= 2) {
    const climb = idleLate[idleLate.length - 1] - Math.min(...idleLate)
    if (climb > ACCUM_DB) issues.push({ sev: 'bad', msg: `idle-tail floor climbs ${climb.toFixed(1)} dB over the run (>${ACCUM_DB}) — energy/DC accumulating in a feedback loop` })
  }
  // 5) no runaway pinning the limiter for a whole stress window
  const pinned = rows.filter(r => r.stressPeak > BLOWUP_DBFS)
  if (pinned.length) issues.push({ sev: 'warn', msg: `${pinned.length} stress cycle(s) peak near full-scale (> ${BLOWUP_DBFS} dBFS) — dense, leaning on the limiter` })
  return issues
}

function printReport(rows, sr, issues) {
  console.log(`soak — ${rows.length} cycles @ ${sr}Hz   (drift <${STRESS_DRIFT_DB}dB · decay ≥${DECAY_MARGIN_DB}dB · accum <${ACCUM_DB}dB)\n`)
  console.log('  cyc   stress rms   stress peak   idle-late rms   decay')
  for (const r of rows) {
    const decay = r.idleLateRms !== undefined ? (r.stressRms - r.idleLateRms) : null
    console.log(`  ${String(r.cyc).padStart(3)}     ${fmtDb(r.stressRms)}      ${fmtDb(r.stressPeak)}       ${r.idleLateRms !== undefined ? fmtDb(r.idleLateRms) : '   —'}     ${decay !== null ? decay.toFixed(1) + 'dB' : ''}`)
  }
  console.log()
  if (!issues.length) { console.log('✓ stable over the long run — no drift, leak, accumulation, or blowup'); return }
  for (const it of issues) console.log(`  ${it.sev === 'bad' ? '✗' : '⚠'} ${it.msg}`)
}

// ── run ──────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2)
const json = argv.includes('--json'), quiet = argv.includes('--quiet'), keep = argv.includes('--keep')
try {
  const { dir, sr, rows } = analyze()
  const issues = assess(rows)
  if (!keep) fs.rmSync(dir, { recursive: true, force: true })
  if (json) console.log(JSON.stringify({ rows, issues }, null, 2))
  else printReport(rows, sr, issues)
  const bad = issues.filter(i => i.sev === 'bad').length
  if (quiet) process.exit(bad ? 1 : 0)
} catch (e) { console.error('soak-check:', e.message); process.exit(2) }
