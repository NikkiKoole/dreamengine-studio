#!/usr/bin/env node
// web-audio-check.js — does the WASM build's audio match NATIVE? (Web parity, Axis 1.)
// Every other audio gate renders the native build only; this one compiles the SAME engine
// (tools/web-audio-host.c — sound.h with a raylib shim, no graphics) BOTH ways —
//   native: clang -O2          (the reference)
//   wasm:   emcc  -O2 → Node   (NODERAWFS)
// — renders each engine solo with identical deterministic input, and compares the two WAVs.
// It isolates ONE variable: does emscripten's compiled DSP math match native clang (float /
// libm / FMA / codegen determinism)? See docs/design/web-audio-parity.md.
//
//   node tools/web-audio-check.js            per-engine parity report
//   node tools/web-audio-check.js --quiet    CI gate: exit 1 if any engine diverges audibly
//   node tools/web-audio-check.js --json
//   node tools/web-audio-check.js --keep     keep build/.webparity/ (the binaries + WAVs)
//
// TWO-TIER verdict (the 2026-06-17 finding): 10/11 engines reproduce to 13-23 bits — the diff
// sits 75-120 dB below the signal, inaudible. The exception is BOWED: its chaotic stick-slip
// friction has sensitive dependence on initial conditions, so a 1-ULP libm/FMA difference at the
// excitation diverges to a different micro-waveform (~-4 dB) — yet it's the SAME note (pitch +
// level match). So:
//   • Tier 1 SAMPLE parity — diff must sit >= PARITY_FLOOR dB below the signal. Catches a real
//     codegen regression (a non-chaotic engine would jump from -95 dB to audible).
//   • Tier 2 PERCEPTUAL parity — for an engine that fails Tier 1 (chaotic), the two renders' RMS
//     LEVELS must still match within LEVEL_TOL dB (same loudness/pitch, just different phase).
//   A real divergence fails BOTH and is a bug.

const fs = require('fs')
const path = require('path')
const { spawnSync } = require('child_process')

const ROOT = path.resolve(__dirname, '..')
const DIR = path.join(ROOT, 'build', '.webparity')
const HOST = path.join(ROOT, 'tools', 'web-audio-host.c')
const SHIM = path.join(ROOT, 'tools', 'web-audio-shim')
const RUNTIME = path.join(ROOT, 'runtime')

const PARITY_FLOOR = 60.0   // dB: diff this far below signal = sample-parity (inaudible). margin is huge (real is 75-120).
const LEVEL_TOL    = 1.5    // dB: Tier-2 — a chaotic engine's two renders must match in RMS level within this
const FRAMES       = 240

// id → name (mirrors runtime/studio.h INSTR_* / the tune-check map). Pitched/tonal engines only;
// NOISE(3)/MEMBRANE(22) are stochastic/unpitched so sample-diff is meaningless — skipped.
const ENGINES = [
  [4, 'SINE (control)'], [1, 'SAW'], [2, 'TRI'], [18, 'FM'], [19, 'ORGAN'],
  [20, 'EPIANO'], [21, 'PD'], [16, 'PLUCK'], [17, 'MALLET'], [26, 'GUITAR'],
  [27, 'PIANO'], [24, 'VOICE'], [23, 'REED'], [25, 'PIPE'], [29, 'BRASS'], [28, 'BOWED'],
]

function sh(cmd, args) {
  const r = spawnSync(cmd, args, { cwd: ROOT, encoding: 'utf8' })
  if (r.status !== 0) { process.stderr.write((r.stdout || '') + (r.stderr || '')); throw new Error(`${cmd} failed`) }
  return r
}

function build() {
  fs.mkdirSync(DIR, { recursive: true })
  const nativeBin = path.join(DIR, 'host_native')
  const wasmJs = path.join(DIR, 'host.js')
  const inc = ['-I', SHIM, '-I', RUNTIME, HOST]
  sh('clang', ['-O2', ...inc, '-lm', '-o', nativeBin])
  sh('emcc', ['-O2', ...inc, '-o', wasmJs, '-sNODERAWFS=1', '-sEXIT_RUNTIME=1', '-sENVIRONMENT=node'])
  return { nativeBin, wasmJs }
}

function readWavI16(file) {
  const b = fs.readFileSync(file)
  let o = 12, d = null
  while (o + 8 <= b.length) { const id = b.toString('ascii', o, o + 4), len = b.readUInt32LE(o + 4); if (id === 'data') { d = { o: o + 8, len }; break } o += 8 + len + (len & 1) }
  const n = d.len / 2, s = new Int16Array(n)
  for (let i = 0; i < n; i++) s[i] = b.readInt16LE(d.o + i * 2)
  return s
}

const db = (x) => x <= 0 ? -Infinity : 20 * Math.log10(x / 32768)

function compare(a, b) {
  let maxd = 0, ndiff = 0, first = -1, sumsq = 0, sigsq = 0, wsq = 0
  const n = Math.min(a.length, b.length)
  for (let i = 0; i < n; i++) {
    const dd = Math.abs(a[i] - b[i]); if (dd) { ndiff++; if (first < 0) first = i }
    if (dd > maxd) maxd = dd
    sumsq += dd * dd; sigsq += a[i] * a[i]; wsq += b[i] * b[i]
  }
  const rmsd = Math.sqrt(sumsq / n), rmsN = Math.sqrt(sigsq / n), rmsW = Math.sqrt(wsq / n)
  return {
    pctDiff: 100 * ndiff / n, maxLsb: maxd, firstFrame: first < 0 ? -1 : Math.floor(first / 2 / 735),
    diffDb: db(rmsd), nativeDb: db(rmsN), wasmDb: db(rmsW),
    belowSignal: db(rmsd) - db(rmsN),           // how far the diff sits below the native signal
    levelGap: Math.abs(db(rmsN) - db(rmsW)),     // Tier-2: do the two renders match in loudness?
  }
}

function run(opts) {
  const { nativeBin, wasmJs } = build()
  const nWav = path.join(DIR, 'n.wav'), wWav = path.join(DIR, 'w.wav')
  const rows = []
  for (const [id, name] of ENGINES) {
    sh(nativeBin, [nWav, String(FRAMES), String(id)])
    sh('node', [wasmJs, wWav, String(FRAMES), String(id)])
    const c = compare(readWavI16(nWav), readWavI16(wWav))
    let verdict, sev
    if (c.belowSignal <= -PARITY_FLOOR) { verdict = 'sample-parity'; sev = 'ok' }
    else if (c.levelGap <= LEVEL_TOL)   { verdict = 'perceptual (chaotic)'; sev = 'warn' }
    else                                { verdict = 'DIVERGES'; sev = 'bad' }
    rows.push({ id, name, ...c, verdict, sev })
  }
  if (!opts.keep) fs.rmSync(DIR, { recursive: true, force: true })
  return rows
}

function report(rows) {
  console.log(`web audio parity — native (clang) vs wasm (emcc), ${ENGINES.length} engines @ ${FRAMES}f`)
  console.log(`  (sample-parity = diff >${PARITY_FLOOR}dB below signal; else perceptual if RMS levels match <${LEVEL_TOL}dB)\n`)
  const mark = (s) => s === 'bad' ? '✗' : s === 'warn' ? '○' : '·'
  for (const r of rows) {
    const lvl = r.verdict === 'perceptual (chaotic)' ? `  level gap ${r.levelGap.toFixed(2)}dB` : ''
    console.log(`  ${mark(r.sev)} ${r.name.padEnd(16)} diff ${r.belowSignal.toFixed(1).padStart(7)} dB below signal  (max ${String(r.maxLsb).padStart(5)} LSB, ${r.pctDiff.toFixed(1).padStart(5)}%)  ${r.verdict}${lvl}`)
  }
  const bad = rows.filter(r => r.sev === 'bad'), chaotic = rows.filter(r => r.sev === 'warn')
  console.log()
  if (chaotic.length) console.log(`○ ${chaotic.length} engine(s) sample-diverge but match in level (chaotic — expected): ${chaotic.map(r => r.name).join(', ')}`)
  if (!bad.length) console.log('✓ wasm matches native: every engine is sample-identical or perceptually equal')
  else { console.log(`✗ ${bad.length} engine(s) DIVERGE audibly (sample AND level differ) — a real codegen/float bug:`); for (const r of bad) console.log(`    ${r.name}: diff ${r.belowSignal.toFixed(1)}dB below signal, level gap ${r.levelGap.toFixed(2)}dB`) }
  return bad.length
}

const argv = process.argv.slice(2)
const opts = { json: argv.includes('--json'), quiet: argv.includes('--quiet'), keep: argv.includes('--keep') }
try {
  const rows = run(opts)
  if (opts.json) console.log(JSON.stringify(rows, null, 2))
  const bad = opts.json ? rows.filter(r => r.sev === 'bad').length : report(rows)
  if (opts.quiet) process.exit(bad ? 1 : 0)
} catch (e) { console.error('web-audio-check:', e.message); process.exit(2) }
