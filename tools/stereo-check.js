#!/usr/bin/env node
// stereo-check.js — the STEREO oracle: what the other audio gates are structurally blind to.
//
//   node tools/stereo-check.js <file.wav> [more.wav ...]   report; multiple files = A/B table
//   node tools/stereo-check.js <file.wav> --expect <kind>  PASS/FAIL gate (exit 1 on fail)
//   node tools/stereo-check.js --check                     self-test against synthetic signals
//
//   --expect <kind>   mono | wide | autopan   what this render SHOULD be (see "kinds" below)
//   --rate <hz>       with --expect autopan: the LFO rate the cart was told to use (±20% tolerance)
//   --quiet           one PASS/FAIL line per file, for CI
//   --window <ms>     pan-trace bucket size (default 50)
//   --json            machine-readable
//
// WHY THIS EXISTS. Every other audio gate in this repo reads a WAV through a `readWavMono()` that
// averages the channels on the way in:
//     else s[i] = (b.readInt16LE(...) + b.readInt16LE(... + 2)) / 65536
// That one line is in level-check, tune-check, fx-check, dc-check, click-check, harmonic-spec,
// formant-check, soak-check, wav-analyze, wav-envelope and the rest. The render is genuinely
// stereo (studio.c's wav_stream_open writes 2ch interleaved), so the information is THERE and
// then thrown away at the door. Consequence: autopan, pan_law, the stereo-linked master soft-clip
// and chorus/flanger width have had ZERO coverage — a change could invert, collapse or silence
// any of them and every gate in the repo would still pass. Found 2026-07-30 chasing a player
// report that autopan "doesn't seem to do much", which no existing tool could confirm or refute.
//
// A mono downmix is not just lossy here, it is actively BLIND to the failure mode: an antiphase
// auto-pan has gL+gR ≈ constant, so summing to mono removes the effect almost perfectly. The
// louder the panning, the more completely the mono gate cannot see it.
//
// WHAT IT MEASURES (per file, and over time):
//   corr      Pearson correlation of L against R. +1 identical (mono), 0 uncorrelated, -1 antiphase.
//   width     side/mid RMS ratio. 0 = pure mono, ~1 = as much difference as sum, >1 = very wide.
//   balance   dB offset of R relative to L over the whole file (a static lean).
//   pan trace pan position per window, so a MOVING image is visible as motion rather than as an
//             average — an autopan sweeping hard L↔R and a dead-centre mono file have the SAME
//             mean pan. Reported as excursion (how far it travels) + the dominant rate.
//   monosum   dB change when folded to mono: how much cancels on a mono speaker.
//
// KINDS for --expect:
//   mono          corr > 0.999 and width < 0.01     centred; the byte-identical baseline
//   wide          width > 0.05                      the image is OFF-CENTRE. Deliberately does not
//                                                   gate on corr: a mono source panned hard left is
//                                                   one signal at two gains, so corr stays 1.0.
//   decorrelated  corr < 0.98 and width > 0.05      the sides carry DIFFERENT content (chorus,
//                                                   flanger spread, a real stereo reverb)
//   autopan       excursion > 0.30 + a rate in       the image MOVES; --rate checks how fast
//                 0.1..20 Hz
//
// NOT a loudness/quality tool — pair it with level-check (levels) and fx-check (effect presence).
// Reads the same 16-bit PCM WAV the rest of the gates read; refuses a mono FILE with a clear
// message, because "1 channel" is a render-side problem this tool cannot diagnose.

const fs = require('fs')

// ── WAV (16-bit PCM) — keeps the channels APART, unlike every other reader here ──
function readWavStereo(file) {
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
  if (fmt.ch !== 2) throw new Error(`${file}: ${fmt.ch} channel(s) — stereo-check needs a 2ch render.\n` +
    `  play.js --wav writes stereo; a 1ch file came from the live-capture path (.bake/wav_request),\n` +
    `  which downmixes by design. Re-render with: node tools/play.js <cart> ... --wav out.wav`)
  const n = Math.floor(data.len / 4)
  const L = new Float64Array(n), R = new Float64Array(n)
  for (let i = 0; i < n; i++) {
    L[i] = b.readInt16LE(data.off + i * 4) / 32768
    R[i] = b.readInt16LE(data.off + i * 4 + 2) / 32768
  }
  return { sr: fmt.sr, L, R, n }
}

const rms = (a, i0, i1) => { let s = 0; for (let i = i0; i < i1; i++) s += a[i] * a[i]
                             return Math.sqrt(s / Math.max(1, i1 - i0)) }
// loop, NOT Math.max(...arr): spreading a multi-second buffer overflows the call stack
const peakAbs = a => { let p = 0; for (let i = 0; i < a.length; i++) { const v = a[i] < 0 ? -a[i] : a[i]; if (v > p) p = v } return p }
const minOf = a => { let m = Infinity; for (const v of a) if (v < m) m = v; return m === Infinity ? 0 : m }
const maxOf = a => { let m = -Infinity; for (const v of a) if (v > m) m = v; return m === -Infinity ? 0 : m }
const dbfs = v => v <= 1e-12 ? -Infinity : 20 * Math.log10(v)
const fmtDb = v => (v === -Infinity ? '  -inf' : (v >= 0 ? '+' : '') + v.toFixed(2))

// Pearson correlation. The headline number: +1 mono, -1 antiphase.
function correlation(L, R, n) {
  let sL = 0, sR = 0
  for (let i = 0; i < n; i++) { sL += L[i]; sR += R[i] }
  const mL = sL / n, mR = sR / n
  let num = 0, dL = 0, dR = 0
  for (let i = 0; i < n; i++) {
    const a = L[i] - mL, b = R[i] - mR
    num += a * b; dL += a * a; dR += b * b
  }
  const den = Math.sqrt(dL * dR)
  return den < 1e-20 ? 1 : num / den      // two silent channels are trivially "identical"
}

// Per-window pan position in [-1,+1] from the L/R energy split. Energy, not amplitude, so a
// window is judged by how loud each side is rather than by instantaneous sample values.
function panTrace(L, R, n, sr, windowMs) {
  const w = Math.max(1, Math.floor(sr * windowMs / 1000))
  const out = []
  for (let i = 0; i + w <= n; i += w) {
    const eL = rms(L, i, i + w), eR = rms(R, i, i + w)
    const tot = eL + eR
    if (tot < 1e-5) continue                        // skip silence: pan is undefined there
    out.push({ t: i / sr, pan: (eR - eL) / tot })
  }
  return out
}

// Dominant rate of the pan trace, by counting zero-crossings of the DEMEANED trace. Cheap and
// robust for an LFO; a full FFT would be overkill and would need windowing decisions.
function panRate(trace, windowMs) {
  if (trace.length < 4) return null
  const mean = trace.reduce((a, p) => a + p.pan, 0) / trace.length
  let crossings = 0
  for (let i = 1; i < trace.length; i++) {
    const a = trace[i - 1].pan - mean, b = trace[i].pan - mean
    if (a === 0) continue
    if ((a < 0) !== (b < 0)) crossings++
  }
  const durS = (trace[trace.length - 1].t - trace[0].t) + windowMs / 1000
  if (durS <= 0 || crossings < 2) return null
  return crossings / (2 * durS)                      // two crossings per cycle
}

function analyze(file, windowMs) {
  const { sr, L, R, n } = readWavStereo(file)
  const rL = rms(L, 0, n), rR = rms(R, 0, n)
  // mid/side: the classic width measure. side==0 means the channels are the same signal.
  const mid = new Float64Array(n), side = new Float64Array(n)
  for (let i = 0; i < n; i++) { mid[i] = (L[i] + R[i]) / 2; side[i] = (L[i] - R[i]) / 2 }
  const rMid = rms(mid, 0, n), rSide = rms(side, 0, n)
  const trace = panTrace(L, R, n, sr, windowMs)
  const pans = trace.map(p => p.pan)
  const panMin = minOf(pans)
  const panMax = maxOf(pans)
  let identical = true
  for (let i = 0; i < n; i++) if (L[i] !== R[i]) { identical = false; break }
  return {
    file, sr, n, durS: n / sr, identical,
    corr:      correlation(L, R, n),
    width:     rMid < 1e-12 ? 0 : rSide / rMid,
    balanceDb: dbfs(rR) - dbfs(rL),
    peakLDb:   dbfs(peakAbs(L)),
    peakRDb:   dbfs(peakAbs(R)),
    monosumDb: dbfs(rMid) - dbfs(Math.max(rL, rR)),   // how much dies when folded to mono
    panMin, panMax, panExcursion: panMax - panMin,
    panRateHz: panRate(trace, windowMs),
    windows:   trace.length,
  }
}

// ── expectations ────────────────────────────────────────────────────────────
function judge(a, kind, wantRate) {
  const reasons = []
  let pass = true
  const need = (ok, msg) => { if (!ok) { pass = false; reasons.push(msg) } }
  if (kind === 'mono') {
    need(a.corr > 0.999, `corr ${a.corr.toFixed(4)} ≤ 0.999 (channels differ)`)
    need(a.width < 0.01, `width ${a.width.toFixed(4)} ≥ 0.01 (side energy present)`)
  } else if (kind === 'wide') {
    // WIDTH only, deliberately NOT correlation. A mono source panned hard left is the same signal
    // at two gains, so corr stays exactly 1.0 while the image is as off-centre as it can get.
    // Correlation measures DECORRELATION (different content per side, e.g. chorus/reverb spread);
    // width measures how far the image departs from centre. Gating "wide" on corr rejected a
    // hard-panned signal as mono, which is how the self-test caught it.
    need(a.width > 0.05, `width ${a.width.toFixed(4)} ≤ 0.05 (image is essentially mono)`)
  } else if (kind === 'decorrelated') {
    need(a.corr < 0.98, `corr ${a.corr.toFixed(4)} ≥ 0.98 (channels carry the same signal)`)
    need(a.width > 0.05, `width ${a.width.toFixed(4)} ≤ 0.05 (no side energy)`)
  } else if (kind === 'autopan') {
    need(a.panExcursion > 0.30,
         `pan excursion ${a.panExcursion.toFixed(3)} ≤ 0.30 (the image barely moves)`)
    need(a.panRateHz !== null && a.panRateHz >= 0.1 && a.panRateHz <= 20,
         `no LFO rate detected in 0.1..20 Hz (got ${a.panRateHz === null ? 'none' : a.panRateHz.toFixed(2) + ' Hz'})`)
    if (wantRate && a.panRateHz !== null) {
      const lo = wantRate * 0.8, hi = wantRate * 1.2
      need(a.panRateHz >= lo && a.panRateHz <= hi,
           `rate ${a.panRateHz.toFixed(2)} Hz outside ±20% of the requested ${wantRate} Hz`)
    }
  } else {
    throw new Error(`unknown --expect kind "${kind}" (want: mono | wide | decorrelated | autopan)`)
  }
  return { pass, reasons }
}

function report(a) {
  console.log(`\n${a.file}`)
  console.log(`  ${a.durS.toFixed(2)}s · ${a.sr} Hz · ${a.windows} pan windows`)
  if (a.identical) console.log(`  ⚠ L and R are BYTE-IDENTICAL — the render is centred mono in a stereo container`)
  console.log(`  corr      ${a.corr.toFixed(4)}   ${a.corr > 0.999 ? '(mono / centred)' :
                                                    a.corr < -0.5 ? '(largely ANTIPHASE — check polarity)' :
                                                    a.corr < 0.98 ? '(genuine stereo)' : '(near-mono)'}`)
  console.log(`  width     ${a.width.toFixed(4)}   side/mid RMS`)
  console.log(`  balance   ${fmtDb(a.balanceDb)} dB  R relative to L`)
  console.log(`  peak L/R  ${fmtDb(a.peakLDb)} / ${fmtDb(a.peakRDb)} dBFS`)
  console.log(`  mono fold ${fmtDb(a.monosumDb)} dB  change when summed to mono`)
  console.log(`  pan       ${a.panMin >= 0 ? ' ' : ''}${a.panMin.toFixed(3)} .. ${a.panMax >= 0 ? ' ' : ''}${a.panMax.toFixed(3)}` +
              `   excursion ${a.panExcursion.toFixed(3)}` +
              (a.panRateHz !== null ? `   ~${a.panRateHz.toFixed(2)} Hz` : `   (no periodic motion)`))
}

function table(rows) {
  console.log('')
  console.log('file'.padEnd(28) + 'corr'.padEnd(9) + 'width'.padEnd(9) + 'bal dB'.padEnd(9) +
              'pan excur'.padEnd(11) + 'rate')
  console.log('-'.repeat(74))
  for (const a of rows) {
    const short = a.file.split('/').pop()
    console.log(short.slice(0, 27).padEnd(28) +
      a.corr.toFixed(4).padEnd(9) +
      a.width.toFixed(4).padEnd(9) +
      fmtDb(a.balanceDb).padEnd(9) +
      a.panExcursion.toFixed(3).padEnd(11) +
      (a.panRateHz !== null ? a.panRateHz.toFixed(2) + ' Hz' : '-'))
  }
}

// ── self-test ───────────────────────────────────────────────────────────────
// A broken analyser and a mono signal print the same thing, so the tool must prove it can tell
// them apart before any null result from it is worth believing (same rule as inharm-spec --check).
function writeWav(path, L, R, sr) {
  const n = L.length, dataBytes = n * 4, buf = Buffer.alloc(44 + dataBytes)
  buf.write('RIFF', 0); buf.writeUInt32LE(36 + dataBytes, 4); buf.write('WAVE', 8)
  buf.write('fmt ', 12); buf.writeUInt32LE(16, 16); buf.writeUInt16LE(1, 20)
  buf.writeUInt16LE(2, 22); buf.writeUInt32LE(sr, 24); buf.writeUInt32LE(sr * 4, 28)
  buf.writeUInt16LE(4, 32); buf.writeUInt16LE(16, 34)
  buf.write('data', 36); buf.writeUInt32LE(dataBytes, 40)
  for (let i = 0; i < n; i++) {
    buf.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(L[i] * 32767))), 44 + i * 4)
    buf.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(R[i] * 32767))), 44 + i * 4 + 2)
  }
  fs.writeFileSync(path, buf)
}

function selfTest() {
  const os = require('os'), path = require('path')
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'stereocheck-'))
  const sr = 44100, dur = 4, n = sr * dur
  const tone = i => Math.sin(2 * Math.PI * 220 * i / sr) * 0.5
  const cases = []

  // 1. dead centre — the byte-identical baseline every non-panning cart should produce
  {
    const L = new Float64Array(n), R = new Float64Array(n)
    for (let i = 0; i < n; i++) { L[i] = R[i] = tone(i) }
    const f = path.join(dir, 'mono.wav'); writeWav(f, L, R, sr)
    cases.push({ f, kind: 'mono', want: true, label: 'centred mono → expect mono' })
    cases.push({ f, kind: 'autopan', want: false, label: 'centred mono → expect autopan (must FAIL)' })
  }
  // 2. hard static pan — a fixed off-centre image
  {
    const L = new Float64Array(n), R = new Float64Array(n)
    for (let i = 0; i < n; i++) { L[i] = tone(i) * 1.0; R[i] = tone(i) * 0.2 }
    const f = path.join(dir, 'wide.wav'); writeWav(f, L, R, sr)
    cases.push({ f, kind: 'wide', want: true, label: 'hard-left static (mono src) → expect wide' })
    cases.push({ f, kind: 'decorrelated', want: false, label: 'hard-left static (mono src) → expect decorrelated (must FAIL: corr is 1.0)' })
    cases.push({ f, kind: 'mono', want: false, label: 'hard-left static → expect mono (must FAIL)' })
    cases.push({ f, kind: 'autopan', want: false, label: 'hard-left STATIC → expect autopan (must FAIL: it does not move)' })
  }
  // 3. autopan at a known rate, built the way pan_process builds it (antiphase gains)
  for (const rate of [1.5, 4.5]) {
    const L = new Float64Array(n), R = new Float64Array(n), depth = 0.7
    for (let i = 0; i < n; i++) {
      const mod = 0.5 + 0.5 * Math.sin(2 * Math.PI * rate * i / sr)
      L[i] = tone(i) * (1 - depth * (1 - mod))
      R[i] = tone(i) * (1 - depth * mod)
    }
    const f = path.join(dir, `autopan${rate}.wav`); writeWav(f, L, R, sr)
    cases.push({ f, kind: 'autopan', want: true, rate, label: `autopan @ ${rate} Hz → expect autopan + rate` })
    cases.push({ f, kind: 'mono', want: false, label: `autopan @ ${rate} Hz → expect mono (must FAIL)` })
  }
  // 4. the blindness proof: the SAME autopan file, summed to mono the way every other gate reads it
  {
    const L = new Float64Array(n), R = new Float64Array(n), depth = 0.7, rate = 4.5
    for (let i = 0; i < n; i++) {
      const mod = 0.5 + 0.5 * Math.sin(2 * Math.PI * rate * i / sr)
      const m = (tone(i) * (1 - depth * (1 - mod)) + tone(i) * (1 - depth * mod)) / 2
      L[i] = R[i] = m
    }
    const f = path.join(dir, 'autopan-downmixed.wav'); writeWav(f, L, R, sr)
    cases.push({ f, kind: 'mono', want: true,
                 label: 'autopan AFTER a mono downmix → reads as mono (this is the blind spot)' })
  }

  let fail = 0
  console.log('stereo-check --check  (synthetic signals with known answers)\n')
  for (const c of cases) {
    const a = analyze(c.f, 50)
    const { pass } = judge(a, c.kind, c.rate)
    const ok = pass === c.want
    if (!ok) fail++
    console.log(`  ${ok ? '✓' : '✗'} ${c.label.padEnd(62)} ${pass ? 'PASS' : 'FAIL'}${ok ? '' : '   ← WRONG'}`)
  }
  // rate accuracy, reported separately: a detector that fires on anything is not a detector
  console.log('')
  for (const rate of [1.5, 4.5]) {
    const a = analyze(path.join(dir, `autopan${rate}.wav`), 50)
    const err = Math.abs(a.panRateHz - rate) / rate * 100
    const ok = err < 20
    if (!ok) fail++
    console.log(`  ${ok ? '✓' : '✗'} rate detection: asked ${rate} Hz, measured ${a.panRateHz.toFixed(2)} Hz (${err.toFixed(1)}% off)`)
  }
  fs.rmSync(dir, { recursive: true, force: true })
  console.log(fail ? `\n✗ ${fail} self-test failure(s) — do NOT trust this tool's output` : '\n✓ self-test clean')
  return fail ? 1 : 0
}

// ── CLI ─────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2)
const flag = f => argv.includes(f)
const opt = (f, d) => { const i = argv.indexOf(f); return i >= 0 && i + 1 < argv.length ? argv[i + 1] : d }

if (flag('--check')) process.exit(selfTest())

const files = argv.filter((a, i) => !a.startsWith('--') &&
  !(i > 0 && ['--expect', '--rate', '--window'].includes(argv[i - 1])))
if (!files.length) {
  console.error('usage: node tools/stereo-check.js <file.wav> [more.wav ...] [--expect mono|wide|decorrelated|autopan] [--rate <hz>] [--quiet] [--json]')
  console.error('       node tools/stereo-check.js --check      self-test')
  process.exit(2)
}

const windowMs = parseFloat(opt('--window', '50'))
const expect = opt('--expect', null)
const wantRate = opt('--rate', null) ? parseFloat(opt('--rate')) : null
const quiet = flag('--quiet'), asJson = flag('--json')

let exit = 0
const rows = []
for (const f of files) {
  let a
  try { a = analyze(f, windowMs) }
  catch (e) { console.error(`✗ ${e.message}`); exit = 2; continue }
  rows.push(a)
  if (expect) {
    const { pass, reasons } = judge(a, expect, wantRate)
    if (!pass) exit = 1
    if (quiet) console.log(`${pass ? 'PASS' : 'FAIL'}  ${f}  (expect ${expect})${pass ? '' : '  — ' + reasons.join('; ')}`)
    else {
      report(a)
      console.log(`  ${pass ? '✓ PASS' : '✗ FAIL'} — expected ${expect}`)
      for (const r of reasons) console.log(`      ${r}`)
    }
  } else if (!quiet && !asJson) report(a)
}
if (asJson) console.log(JSON.stringify(expect ? rows.map(a => ({ ...a, ...judge(a, expect, wantRate) })) : rows, null, 2))
else if (rows.length > 1 && !quiet) table(rows)
process.exit(exit)
