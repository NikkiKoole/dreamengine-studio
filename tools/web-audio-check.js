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
//   node tools/web-audio-check.js --selfcheck  known answers for the ANALYSER — no clang, no emcc,
//                                             no engine, so it runs anywhere (repo-doctor row)
//   node tools/web-audio-check.js --bypass   the NEGATIVE CONTROL: rebuild the wasm side with
//                                             -ffast-math and require the gate to go RED
//
// ⚠ TWO CONTROLS, BECAUSE ONE CANNOT COVER BOTH HALVES. `--selfcheck` judges the pure analyser
// (readWavI16 · compare · classify) on synthesised signals, and it is structurally UNABLE to tell
// you the comparison reaches the DSP — it never builds anything. `--bypass` is the other half: with
// -ffast-math on one side the two builds MUST disagree, so a green `--bypass` means this gate cannot
// see a real divergence and every clean run it has ever printed means nothing. It needs the
// toolchain, which is why it is not the repo-doctor row.
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
// dBFS: below this the NATIVE reference is silent, i.e. nothing was rendered and every parity verdict
// below is vacuous. Not a tuning knob — it separates "an engine made no sound" from "a quiet engine".
// Measured across all 16: the quietest is GUITAR at -42.7 dBFS, so this leaves ~37 dB of headroom.
const SILENCE_FLOOR = -80.0
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

function build(bypass) {
  fs.mkdirSync(DIR, { recursive: true })
  const nativeBin = path.join(DIR, 'host_native')
  const wasmJs = path.join(DIR, 'host.js')
  const inc = ['-I', SHIM, '-I', RUNTIME, HOST]
  sh('clang', ['-O2', ...inc, '-lm', '-o', nativeBin])
  // --bypass is the NEGATIVE CONTROL: build the wasm side with -ffast-math, which re-enables the
  // contraction the FP_CONTRACT pragma turns off (the pragma does not override the flag — that is
  // checklist item 3 below). The two builds must then DIVERGE, and this gate must go RED. If it
  // stays green, the comparison is not reaching the DSP and every clean run above means nothing.
  sh('emcc', ['-O2', ...(bypass ? ['-ffast-math'] : []), ...inc, '-o', wasmJs,
    '-sNODERAWFS=1', '-sEXIT_RUNTIME=1', '-sENVIRONMENT=node'])
  return { nativeBin, wasmJs }
}

function readWavI16(file) {
  const b = fs.readFileSync(file)
  let o = 12, d = null
  while (o + 8 <= b.length) { const id = b.toString('ascii', o, o + 4), len = b.readUInt32LE(o + 4); if (id === 'data') { d = { o: o + 8, len }; break } o += 8 + len + (len & 1) }
  if (!d) throw new Error(`${path.basename(file)}: no data chunk — not a WAV this tool can read`)
  const n = Math.min(d.len, b.length - d.o) >> 1, s = new Int16Array(n)
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
    n, nNative: a.length, nWasm: b.length,       // LIVENESS — see classify()
  }
}

// ── the VERDICT, as one function ────────────────────────────────────────────────────────────────
// Extracted so --selfcheck exercises THE tier arithmetic rather than a copy of it.
//
// ⚠ TIER -1 IS NOT DECORATION, AND IT IS WHY THIS FUNCTION EXISTS. Every tier below it is a
// statement about DIFFERENCES FOUND BETWEEN TWO RENDERS, and all of them are trivially satisfied
// when there is nothing to compare or nothing to hear. Measured on the old code, all four of these
// returned "BIT-IDENTICAL, ok":
//   · two EMPTY renders                (n = 0; every statistic is NaN, but maxLsb === 0 short-circuits)
//   · an empty wasm render vs a good native one   (Math.min makes n = 0 — a build that produced
//     NOTHING was the strongest possible pass)
//   · two SILENT renders               (if the host stopped making audio, all 16 engines read perfect)
//   · a TRUNCATED render vs a full one (the extra samples are simply never looked at)
// This gate's whole subject is "do these two agree", and two nothings agree perfectly. So: assert
// the measurement happened before believing anything about what it found.
// THE ONE PREDICATE both exit paths use. They did not agree: `--quiet` failed on anything that was
// not 'ok' (via report()), while `--json --quiet` counted only sev 'bad' — so a 1-LSB regression,
// which is 'warn', exited 1 in one mode and 0 in the other, against a header that says the bar is
// BIT-IDENTICAL. Measured under --bypass: 7 of the 16 diverged engines land in 'warn', so that mode
// would have passed a build with real drift in half its engines. (canvas-diff shipped the same
// shape — two comparison paths, one condition, quietly different answers.)
const isOff = (r) => r.sev !== 'ok'

function classify(c) {
  if (!c.n)                        return { verdict: 'NOTHING COMPARED — a render is empty', sev: 'bad' }
  if (c.nNative !== c.nWasm)       return { verdict: `LENGTH MISMATCH — native ${c.nNative} vs wasm ${c.nWasm} samples`, sev: 'bad' }
  if (c.nativeDb < SILENCE_FLOOR)  return { verdict: `REFERENCE IS SILENT (${c.nativeDb.toFixed(1)} dBFS) — nothing was measured`, sev: 'bad' }
  if (c.maxLsb === 0)              return { verdict: 'BIT-IDENTICAL', sev: 'ok' }
  if (c.belowSignal <= -PARITY_FLOOR) return { verdict: 'near-parity, NOT bit-exact', sev: 'warn' }
  if (c.levelGap <= LEVEL_TOL)     return { verdict: 'chaotic divergence', sev: 'bad' }
  return { verdict: 'DIVERGES AUDIBLY', sev: 'bad' }
}

function run(opts) {
  // An empty roster would report "✓ all 0 engines BIT-IDENTICAL" — the same vacuity as tier -1, one
  // level up. (mirror-diff and canvas-diff both shipped this exact pass.)
  if (!ENGINES.length) throw new Error('the ENGINES roster is empty — there is nothing to compare')
  const { nativeBin, wasmJs } = build(opts.bypass)
  const nWav = path.join(DIR, 'n.wav'), wWav = path.join(DIR, 'w.wav')
  const rows = []
  for (const [id, name] of ENGINES) {
    sh(nativeBin, [nWav, String(FRAMES), String(id)])
    sh('node', [wasmJs, wWav, String(FRAMES), String(id)])
    const c = compare(readWavI16(nWav), readWavI16(wWav))
    // Tier 0 is the bar: identical BITS. The looser tiers below it are kept because they say HOW
    // BADLY determinism broke, and tier -1 above it asserts there was anything to compare at all.
    rows.push({ id, name, ...c, ...classify(c) })
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
  const off = rows.filter(isOff)
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

// ── --selfcheck: KNOWN ANSWERS FOR THE ANALYSER ─────────────────────────────────────────────────
// Deliberately TOOLCHAIN-FREE — no clang, no emcc, no engine — so it can sit in repo-doctor on any
// machine. It judges the pure half (readWavI16 · compare · classify) against signals synthesised
// here, whose answers are known by arithmetic. The half it CANNOT reach (do the two builds actually
// differ, does the comparison reach the DSP) is what `--bypass` is for; see the header.
function scWriteWav (file, s, opt) {
  const o = opt || {}
  const n = s.length, body = o.truncateData ? 0 : n * 2
  const b = Buffer.alloc(44 + n * 2)
  b.write('RIFF', 0); b.writeUInt32LE(36 + body, 4); b.write('WAVE', 8)
  b.write('fmt ', 12); b.writeUInt32LE(16, 16); b.writeUInt16LE(1, 20); b.writeUInt16LE(2, 22)
  b.writeUInt32LE(44100, 24); b.writeUInt32LE(44100 * 4, 28); b.writeUInt16LE(4, 32); b.writeUInt16LE(16, 34)
  b.write(o.noData ? 'JUNK' : 'data', 36); b.writeUInt32LE(body, 40)
  for (let i = 0; i < n; i++) b.writeInt16LE(Math.max(-32768, Math.min(32767, s[i])), 44 + i * 2)
  // cutFile: the header still CLAIMS n*2 bytes but the file stops early — a half-written render.
  fs.writeFileSync(file, o.cutFile ? b.subarray(0, o.cutFile) : b)
}

function selfcheck () {
  const os = require('os')
  const N = 44100
  const mk = (f) => { const s = new Int16Array(N); for (let i = 0; i < N; i++) s[i] = f(i); return s }
  // A full-scale-ish sine: rms = A/sqrt(2), so dBFS is known by arithmetic rather than by measurement.
  const A = 16384
  const sig = mk(i => Math.round(A * Math.sin(2 * Math.PI * i / 100)))
  const sigDb = 20 * Math.log10((A / Math.SQRT2) / 32768)     // = -9.03 dBFS

  let pass = 0, fail = 0
  const ok = (name, cond, got) => {
    if (cond) { pass++; console.log(`  ✓ ${name}`) }
    else { fail++; console.log(`  ✗ ${name}   got: ${got}`) }
  }
  const V = (a, b) => classify(compare(a, b)).verdict
  const S = (a, b) => classify(compare(a, b)).sev

  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'webaudio-'))
  try {
    console.log('web-audio-check --selfcheck — known answers for the analyser (no clang, no emcc, no engine)\n')

    console.log('THE ARITHMETIC')
    const c0 = compare(sig, sig)
    ok('a signal against itself is 0 LSB', c0.maxLsb === 0, c0.maxLsb)
    ok(`  …and its level is A/sqrt(2) = ${sigDb.toFixed(2)} dBFS`,
       Math.abs(c0.nativeDb - sigDb) < 0.01, c0.nativeDb.toFixed(3))
    const off1 = mk(i => Math.round(A * Math.sin(2 * Math.PI * i / 100)) + 1)
    const c1 = compare(sig, off1)
    ok('a 1-LSB offset reads maxLsb 1 on 100% of samples',
       c1.maxLsb === 1 && Math.abs(c1.pctDiff - 100) < 0.01, `${c1.maxLsb} / ${c1.pctDiff}%`)
    // The diff is exactly 1 LSB everywhere, so its rms is 1 and belowSignal = -20*log10(A/sqrt2).
    // (Asserted at -78.28 in the first draft, from the wrong expression. -81.28 is the arithmetic.)
    ok(`  …sitting -20*log10(A/sqrt2) = ${(-20 * Math.log10(A / Math.SQRT2)).toFixed(2)} dB below the signal`,
       Math.abs(c1.belowSignal - (-20 * Math.log10(A / Math.SQRT2))) < 0.01, c1.belowSignal.toFixed(2))
    const one = mk(i => Math.round(A * Math.sin(2 * Math.PI * i / 100)) + (i === 5000 ? 300 : 0))
    const cOne = compare(sig, one)
    ok('a SINGLE differing sample is found, and localised to its frame',
       cOne.maxLsb === 300 && cOne.firstFrame === Math.floor(5000 / 2 / 735), `${cOne.maxLsb} @ f${cOne.firstFrame}`)

    console.log('\nTHE TIERS, ON BOTH SIDES OF EACH EDGE')
    ok('0 LSB is BIT-IDENTICAL', V(sig, sig) === 'BIT-IDENTICAL', V(sig, sig))
    ok('a 1-LSB offset is NOT bit-identical…', V(sig, off1) !== 'BIT-IDENTICAL', V(sig, off1))
    ok('  …but is near-parity (78 dB down, past the 60 dB floor)',
       V(sig, off1) === 'near-parity, NOT bit-exact', V(sig, off1))
    ok('  …and is reported as a WARN, not a pass', S(sig, off1) === 'warn', S(sig, off1))
    // Same loudness, different phase = the chaotic case Tier 2 exists to name.
    const shifted = mk(i => Math.round(A * Math.sin(2 * Math.PI * (i + 25) / 100)))
    ok('same level, different waveform → chaotic divergence',
       V(sig, shifted) === 'chaotic divergence', V(sig, shifted))
    const quiet = mk(i => Math.round(A * 0.1 * Math.sin(2 * Math.PI * i / 100)))
    ok('a 20 dB level gap → DIVERGES AUDIBLY', V(sig, quiet) === 'DIVERGES AUDIBLY', V(sig, quiet))
    // The PARITY_FLOOR edge, from both sides, computed rather than guessed.
    const atFloor = (below) => { const amp = (A / Math.SQRT2) * Math.pow(10, below / 20) * Math.SQRT2
      return mk(i => Math.round(A * Math.sin(2 * Math.PI * i / 100) + amp * Math.sin(2 * Math.PI * i / 37))) }
    ok('a diff just BELOW the parity floor is near-parity',
       V(sig, atFloor(-62)) === 'near-parity, NOT bit-exact', V(sig, atFloor(-62)))
    ok('a diff just ABOVE it is not', V(sig, atFloor(-58)) !== 'near-parity, NOT bit-exact', V(sig, atFloor(-58)))

    console.log('\nTIER -1 — VACUITY: two nothings agree perfectly')
    // Every one of these returned "BIT-IDENTICAL, ok" before this tier existed.
    const empty = new Int16Array(0), silence = new Int16Array(N)
    ok('two EMPTY renders are NOT bit-identical', S(empty, empty) === 'bad', V(empty, empty))
    ok('  …they are named as nothing compared', /NOTHING COMPARED/.test(V(empty, empty)), V(empty, empty))
    ok('an EMPTY wasm render vs a good native one is refused', S(sig, empty) === 'bad', V(sig, empty))
    ok('two SILENT renders are refused — the reference has no signal',
       /REFERENCE IS SILENT/.test(V(silence, silence)), V(silence, silence))
    ok('a TRUNCATED render is refused, not silently min-ed',
       /LENGTH MISMATCH/.test(V(sig, sig.slice(0, N - 2))), V(sig, sig.slice(0, N - 2)))
    ok('  …naming both lengths', /44100.*44098/.test(V(sig, sig.slice(0, N - 2))), V(sig, sig.slice(0, N - 2)))
    // The floor must not swallow a real engine: the quietest measured is GUITAR at -42.7 dBFS.
    const guitarish = mk(i => Math.round(A * Math.pow(10, (-42.7 - sigDb) / 20) * Math.sin(2 * Math.PI * i / 100)))
    ok('the silence floor does NOT reject the quietest real engine (GUITAR, -42.7 dBFS)',
       V(guitarish, guitarish) === 'BIT-IDENTICAL', `${compare(guitarish, guitarish).nativeDb.toFixed(1)} dBFS → ${V(guitarish, guitarish)}`)
    // ⚠ THE FLOOR'S VALUE, from both sides, on signals that are QUIET RATHER THAN ZERO. Asserting it
    // with digital silence proves nothing: rms 0 makes db() return -Infinity, which is below ANY
    // finite floor — so `SILENCE_FLOOR = -400` passed every silence assertion here until these two
    // were added. A control has to be able to fail for the reason it names.
    const atDb = (target) => { const amp = Math.SQRT2 * 32768 * Math.pow(10, target / 20)
      return mk(i => Math.round(amp * Math.sin(2 * Math.PI * i / 100))) }
    const below = atDb(SILENCE_FLOOR - 7), above = atDb(SILENCE_FLOOR + 5)
    ok(`a quiet-but-nonzero reference BELOW the floor is rejected (${compare(below, below).nativeDb.toFixed(1)} dBFS)`,
       /REFERENCE IS SILENT/.test(V(below, below)), V(below, below))
    ok(`  …and one just ABOVE it is accepted (${compare(above, above).nativeDb.toFixed(1)} dBFS)`,
       V(above, above) === 'BIT-IDENTICAL', V(above, above))

    console.log('\nTHE WAV READER IS IN THE PATH')
    const p = path.join(dir, 'a.wav'); scWriteWav(p, sig)
    ok('a written WAV reads back sample-for-sample',
       compare(readWavI16(p), sig).maxLsb === 0 && readWavI16(p).length === N, readWavI16(p).length)
    const pj = path.join(dir, 'nodata.wav'); scWriteWav(pj, sig, { noData: true })
    let threw = ''
    try { readWavI16(pj) } catch (e) { threw = e.message }
    ok('a file with no data chunk is REFUSED with a real message (it used to TypeError on null)',
       /no data chunk/.test(threw), threw || 'did not throw')
    const pt = path.join(dir, 'lying.wav'); scWriteWav(pt, sig, { truncateData: true })
    ok('a data chunk claiming 0 bytes reads as 0 samples, which tier -1 then refuses',
       readWavI16(pt).length === 0 && S(readWavI16(pt), readWavI16(pt)) === 'bad', readWavI16(pt).length)
    // A HALF-WRITTEN render: the header claims the full length, the file stops at 100 data bytes.
    // Without the bounds clamp this reads past the buffer and throws RangeError from inside the
    // comparison — a crash where the honest answer is "50 samples, and tier -1 refuses them".
    const pc = path.join(dir, 'cut.wav'); scWriteWav(pc, sig, { cutFile: 44 + 100 })
    let cutN = -1, cutErr = ''
    try { cutN = readWavI16(pc).length } catch (e) { cutErr = e.message }
    ok('a file SHORTER than its data chunk claims reads what is there, and does not throw',
       cutN === 50, cutErr || cutN)
    ok('  …and comparing it against the full render is a LENGTH MISMATCH',
       /LENGTH MISMATCH/.test(V(sig, readWavI16(pc))), V(sig, readWavI16(pc)))

    console.log('\nBOTH EXIT PATHS ASK THE SAME QUESTION')
    // The bar is BIT-IDENTICAL, so every non-'ok' severity must count as a failure in BOTH modes.
    const sevs = [sig, off1, shifted, quiet, empty, silence].map(w => classify(compare(sig, w)))
    ok('a 1-LSB "warn" counts as a failure, not a pass',
       isOff({ sev: 'warn' }) === true, isOff({ sev: 'warn' }))
    ok('  …and only BIT-IDENTICAL is ok', sevs.filter(s => !isOff(s)).length === 1,
       sevs.filter(s => !isOff(s)).map(s => s.verdict).join('; '))

    console.log('\nTHE ROSTER')
    ok('every engine id is unique', new Set(ENGINES.map(e => e[0])).size === ENGINES.length, 'duplicate id')
    ok('the roster is not empty (an empty one would report "all 0 BIT-IDENTICAL")', ENGINES.length > 0, 0)

    console.log(`\n${fail === 0 ? '✓' : '✗'} ${pass}/${pass + fail} known answers correct`)
    return fail === 0 ? 0 : 1
  } finally { fs.rmSync(dir, { recursive: true, force: true }) }
}

const argv = process.argv.slice(2)
if (argv.includes('--selfcheck')) process.exit(selfcheck())
const opts = {
  json: argv.includes('--json'), quiet: argv.includes('--quiet'),
  keep: argv.includes('--keep'), bypass: argv.includes('--bypass'),
}
try {
  const rows = run(opts)
  if (opts.json) console.log(JSON.stringify(rows, null, 2))
  const bad = opts.json ? rows.filter(isOff).length : report(rows)
  if (opts.bypass) {
    // Inverted on purpose: with -ffast-math on one side the builds MUST disagree.
    const ok = bad > 0
    console.log(`\n${ok ? '✓' : '✗'} NEGATIVE CONTROL: ${bad} of ${rows.length} engine(s) diverged under -ffast-math` +
      (ok ? ' — the gate can go red.' : ' — THE GATE IS BLIND: it cannot see a real divergence, so every clean run means nothing.'))
    process.exit(ok ? 0 : 1)
  }
  if (opts.quiet) process.exit(bad ? 1 : 0)
} catch (e) { console.error('web-audio-check:', e.message); process.exit(2) }
