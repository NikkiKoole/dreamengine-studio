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
// STATUS 2026-07-30: ALL 16 engines are now BIT-IDENTICAL native-vs-wasm (0 LSB, diff -inf dB),
// BOWED included. Two changes got it there, and the A/B showed neither alone is enough:
//   1. runtime/demath.h — deterministic sin/cos/exp/log/pow/tanh, because libm is not spec'd to
//      the bit and Apple's / emscripten's disagree by ~1 ULP.
//   2. the file-scope `#pragma STDC FP_CONTRACT OFF` in runtime/studio.h — native fuses a*b+c
//      into an FMA, wasm has no scalar FMA instruction, so they drifted apart everywhere.
// THE BAR IS NOW 0 LSB. Do NOT read a BOWED divergence as "expected chaos" any more: it would
// mean one of those two regressed. The gate fails on anything less than bit-identical, and
// prints a checklist of what to look at first.
//
// The older, looser tiers are KEPT — not as the pass condition, but because when this does fail
// they say HOW BADLY. "1 LSB on one engine" is a different bug from "BOWED went chaotic", and
// that distinction is the fastest way to the cause. The tiers, worst last:
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
    // Tier 0 is the bar now: identical BITS. The looser tiers below it are kept because they say
    // HOW BADLY determinism broke, which is the first thing you want to know when it does.
    let verdict, sev
    if (c.maxLsb === 0)                 { verdict = 'BIT-IDENTICAL'; sev = 'ok' }
    else if (c.belowSignal <= -PARITY_FLOOR) { verdict = 'near-parity, NOT bit-exact'; sev = 'warn' }
    else if (c.levelGap <= LEVEL_TOL)   { verdict = 'chaotic divergence'; sev = 'bad' }
    else                                { verdict = 'DIVERGES AUDIBLY'; sev = 'bad' }
    rows.push({ id, name, ...c, verdict, sev })
  }
  if (!opts.keep) fs.rmSync(DIR, { recursive: true, force: true })
  return rows
}

function report(rows) {
  console.log(`web audio parity — native (clang) vs wasm (emcc), ${ENGINES.length} engines @ ${FRAMES}f`)
  console.log(`  (the bar is BIT-IDENTICAL: 0 LSB. Looser tiers are reported to say how badly it broke.)\n`)
  const mark = (s) => s === 'bad' ? '✗' : s === 'warn' ? '○' : '·'
  for (const r of rows) {
    const lvl = r.sev === 'bad' ? `  level gap ${r.levelGap.toFixed(2)}dB` : ''
    console.log(`  ${mark(r.sev)} ${r.name.padEnd(16)} diff ${r.belowSignal.toFixed(1).padStart(7)} dB below signal  (max ${String(r.maxLsb).padStart(5)} LSB, ${r.pctDiff.toFixed(1).padStart(5)}%)  ${r.verdict}${lvl}`)
  }
  const off = rows.filter(r => r.sev !== 'ok')
  console.log()
  if (!off.length) { console.log(`✓ all ${rows.length} engines BIT-IDENTICAL native-vs-wasm (0 LSB)`); return 0 }
  console.log(`✗ ${off.length} of ${rows.length} engine(s) are NOT bit-identical — determinism regressed:`)
  for (const r of off) console.log(`    ${r.name.padEnd(16)} max ${r.maxLsb} LSB, ${r.pctDiff.toFixed(1)}% of samples, ${r.belowSignal.toFixed(1)}dB below signal  (${r.verdict})`)
  console.log(`
  This gate demanded only "inaudible" until 2026-07-30, when demath.h + the FP_CONTRACT pragma
  made all 16 engines exact. If it fails now, check in this order:
    1. runtime/studio.h still opens with  #pragma STDC FP_CONTRACT OFF
       (grep -c FP_CONTRACT runtime/studio.h  → 1)
    2. sound.h still calls de_* and not libm
       (node -e '…' sweep, or just: grep -cE "(^|[^A-Za-z0-9_])(sinf|cosf|expf|powf|tanhf)\\(" runtime/sound.h → 0)
    3. no -ffast-math / -ffp-contract=fast crept into a build (the pragma does NOT override it)
    4. bash tools/det-probes/run.sh — if the demath probe also fails, it's the header, not the engine
  Background: docs/design/determinism.md`)
  return off.length
}

const argv = process.argv.slice(2)
const opts = { json: argv.includes('--json'), quiet: argv.includes('--quiet'), keep: argv.includes('--keep') }
try {
  const rows = run(opts)
  if (opts.json) console.log(JSON.stringify(rows, null, 2))
  const bad = opts.json ? rows.filter(r => r.sev === 'bad').length : report(rows)
  if (opts.quiet) process.exit(bad ? 1 : 0)
} catch (e) { console.error('web-audio-check:', e.message); process.exit(2) }
