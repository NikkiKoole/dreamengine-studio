#!/usr/bin/env node
// inharm-spec.js — measure where a struck/blown voice's PARTIALS actually sit, in cents, against
// the ideal harmonic series n·f0. The missing third leg of the spectral toolkit:
//
//   harmonic-spec.js  how LOUD each partial is        (levels)
//   filter-spec.js    what the filter does to them    (response)
//   inharm-spec.js    where each partial IS           (frequency — inharmonicity)
//
// Born from Synth Secrets audit §I4 / plan item 2.3 ("level-dependent inharmonicity"), whose claim is
// about partial FREQUENCY moving with amplitude. Nothing we had could see that: harmonic-spec reads a
// partial's level at the frequency you ASSUME it has, and a partial that has drifted 30 cents sharp
// just reads as slightly quieter. Same blind-spot family as the one click-check.js was built for.
//
// The claim it exists to test is shared by FIVE engine families (audit §E8 brass, §H guitar, §I4
// piano, §J8 drums, §K8 flutes), so the WAV mode below is deliberately engine-agnostic.
//
//   node tools/inharm-spec.js                             PIANO velocity × time probe (§I4 evidence)
//   node tools/inharm-spec.js --midi 48 --voicing 0       … at another note / piano voicing (0-5)
//   node tools/inharm-spec.js <wav> --f0 <hz>             measure any WAV (whole file)
//   node tools/inharm-spec.js <wav> --f0 <hz> --from 0.05 --to 0.35     … one region
//   node tools/inharm-spec.js --n 20 --json --keep        partial count / machine-readable / keep files
//   node tools/inharm-spec.js <wav> --f0 <hz> --decay     PER-PARTIAL DECAY RATE (dB/s) instead of cents
//   node tools/inharm-spec.js --check                     SELF-TEST against synthetic known-B spectra
//
// --decay exists because a whole class of confusion lives there. `wav-envelope` gives you the WHOLE
// signal's amplitude curve, which cannot tell "the fundamental is dying faster" from "only the upper
// partials are". That distinction is the difference between an implementation bug in the loop and a
// spectral side-effect. It is what settled the §I4b sustain scare: the owner reported a dispersed piano
// "dying out earlier", and per-partial decay showed EVERY partial including h1 dying ~2x faster, which
// ruled out a spectral explanation and pointed at the test rig — `instrument_tune`'s pitch-shift path,
// which forces per-sample fractional interpolation and so bleeds energy every round trip (audio-notes).
// Reach for it whenever a voice's sustain changes and you need to know WHERE the energy went.
//
// RUN --check BEFORE BELIEVING A NULL RESULT. A tool that reports "no inharmonicity" and a tool that
// is silently broken produce the same table. --check synthesises stiff-string spectra at known B
// (including B = 0) and asserts the fit comes back, so "the engine's partials are harmonic" is a
// measurement rather than a bug. That is not hypothetical: the first PIANO run here returned ~0 and
// the self-test is what turned it from a suspected tool fault into audit finding §I4b.
//
// HOW a partial is located, and why not just an FFT bin: a 0.3 s window gives ~3 Hz bins, which at
// partial 12 of a low note is ~4 cents of quantisation — the same order as the effect being measured.
// So each partial is found by a three-stage Goertzel hunt (coarse on a short sub-window to localise,
// then two refinements on the full window, then parabolic interpolation of the peak), giving well
// under 1 cent on a clean partial. A partial is reported as UNRESOLVED rather than guessed when its
// peak lands on the search-window edge (it has drifted past half the inter-partial spacing and is
// genuinely ambiguous) or when it is more than 50 dB below the fundamental (measuring noise).
//
// B is the stiff-string inharmonicity coefficient in f_n = n·f0·sqrt(1 + B·n²) (Fletcher; the number
// piano technicians quote, ~1e-4 for a middle-register grand). Our dispersion is a cascade of
// first-order allpasses, which only APPROXIMATES that law, so the fit residual is printed next to it:
// a large residual means "the partials moved, but not in the shape a stiff string moves them", which
// is itself a finding. Read the fitted B as a summary, the per-partial cents as the measurement.
//
// CAVEATS. (1) Voicings with a detuned second string (grand/bright/dulcimer) put a doublet under every
// partial; the ratio is constant in cents across n, so it adds a small uniform bias that cancels in an
// A/B and barely touches the B fit. (2) The window must be short enough that the partials do not move
// WITHIN it (that is the very effect under test) but long enough to resolve them — 0.3 s is the
// compromise, and is why the probe reports early and late windows separately instead of one average.
// (3) Deterministic: same engine + same probe = identical bytes, so a saved report diffs cleanly
// across engine changes. Cite it as acceptance evidence when touching dispersion code.

const fs = require('fs')
const path = require('path')
const { spawnSync } = require('child_process')

const ROOT = path.resolve(__dirname, '..')

// ── probe schedule (the §I4 matrix: does inharmonicity move with LEVEL, and does it RELAX?) ──
const VELS = [1, 3, 5, 7]                 // note_on vol — pp … ff
const NF = 132, GAP = 12, PER = NF + GAP  // 2.2 s held note, 0.2 s gap
const WINDOWS = [                         // seconds after note-on
  { name: 'early', a: 0.05, b: 0.35 },
  { name: 'mid',   a: 0.60, b: 0.90 },
  { name: 'late',  a: 1.60, b: 1.90 },
]

// ── WAV reader (filter-spec house pattern) ──────────────────────────────────
function readWavMono(file) {
  const b = fs.readFileSync(file)
  if (b.toString('ascii', 0, 4) !== 'RIFF' || b.toString('ascii', 8, 12) !== 'WAVE')
    throw new Error(`${file}: not a WAV`)
  let off = 12, fmt = null, data = null
  while (off + 8 <= b.length) {
    const id = b.toString('ascii', off, off + 4)
    const len = b.readUInt32LE(off + 4)
    if (id === 'fmt ') fmt = { ch: b.readUInt16LE(off + 10), sr: b.readUInt32LE(off + 12), bits: b.readUInt16LE(off + 22) }
    if (id === 'data') data = { off: off + 8, len }
    off += 8 + len + (len & 1)
  }
  if (!fmt || !data || fmt.bits !== 16) throw new Error(`${file}: expected 16-bit PCM WAV`)
  const ch = fmt.ch, n = Math.floor(data.len / 2 / ch)
  const s = new Float64Array(n)
  for (let i = 0; i < n; i++)
    s[i] = ch === 1 ? b.readInt16LE(data.off + i * 2) / 32768
                    : (b.readInt16LE(data.off + i * 2 * ch) + b.readInt16LE(data.off + i * 2 * ch + 2)) / 65536
  return { sr: fmt.sr, s }
}

// ── Goertzel magnitude at an arbitrary frequency over a Hann-windowed block ──
function mag(x, start, len, f, sr) {
  const w = 2 * Math.PI * f / sr
  const coeff = 2 * Math.cos(w)
  let s1 = 0, s2 = 0
  const twoPiOverN = 2 * Math.PI / (len - 1)
  for (let i = 0; i < len; i++) {
    const win = 0.5 - 0.5 * Math.cos(twoPiOverN * i)
    const s0 = win * x[start + i] + coeff * s1 - s2
    s2 = s1; s1 = s0
  }
  const cw = Math.cos(w), sw = Math.sin(w)
  const re = s1 - s2 * cw, im = s2 * sw
  return Math.sqrt(re * re + im * im) / len
}

// Locate a spectral peak in [fLo, fHi]. Coarse pass on a short sub-window to localise cheaply, then
// two refinements on the FULL window, then parabolic interpolation. Returns null if the peak sits on
// a search edge (drifted past half the inter-partial spacing → genuinely ambiguous, do not guess).
function findPeak(x, start, len, fLo, fHi, sr) {
  const coarseLen = Math.min(len, 4096)
  const N = 200, step = (fHi - fLo) / N
  let bestF = fLo, bestM = -1
  for (let i = 0; i <= N; i++) {
    const f = fLo + i * step
    const m = mag(x, start, coarseLen, f, sr)
    if (m > bestM) { bestM = m; bestF = f }
  }
  if (bestF <= fLo + step || bestF >= fHi - step) return null      // on the edge
  for (const [span, pts] of [[step * 1.5, 30], [step * 0.1, 20]]) {
    const lo = bestF - span, hi = bestF + span, st = (hi - lo) / pts
    bestM = -1
    for (let i = 0; i <= pts; i++) {
      const f = lo + i * st
      const m = mag(x, start, len, f, sr)
      if (m > bestM) { bestM = m; bestF = f }
    }
    // parabolic interpolation on the winning triplet
    const m0 = mag(x, start, len, bestF - st, sr), m1 = bestM, m2 = mag(x, start, len, bestF + st, sr)
    const den = m0 - 2 * m1 + m2
    if (den !== 0) bestF += (0.5 * (m0 - m2) / den) * st
  }
  return { f: bestF, m: bestM }
}

const cents = (f, ref) => 1200 * Math.log2(f / ref)

// Refine a nominal f0 (the engine may stretch it — PIANO_STRETCH_K — so never trust the MIDI freq).
function refineF0(x, start, len, f0nom, sr) {
  const p = findPeak(x, start, len, f0nom * Math.pow(2, -1.5 / 12), f0nom * Math.pow(2, 1.5 / 12), sr)
  return p ? p.f : f0nom
}

// Least-squares B in (f_n/(n·f0))² − 1 = B·n², plus the rms residual in cents (how well the
// stiff-string law actually describes what the engine did).
function fitB(parts, f0) {
  let num = 0, den = 0
  for (const p of parts) {
    if (!p.ok) continue
    const y = Math.pow(p.f / (p.n * f0), 2) - 1
    num += y * p.n * p.n; den += Math.pow(p.n, 4)
  }
  if (den === 0) return { B: 0, resid: 0 }
  const B = num / den
  let ss = 0, k = 0
  for (const p of parts) {
    if (!p.ok) continue
    const pred = p.n * f0 * Math.sqrt(1 + B * p.n * p.n)
    ss += Math.pow(cents(p.f, pred), 2); k++
  }
  return { B, resid: k ? Math.sqrt(ss / k) : 0 }
}

// Measure partials 2..nMax over one window. Two passes: a bounded search fits B from the low
// partials, then B PREDICTS where each higher partial should be so the search can follow a large
// drift instead of clipping it at half the spacing.
function measure(x, start, len, f0nom, nMax, sr) {
  const f0 = refineF0(x, start, len, f0nom, sr)
  const fund = mag(x, start, len, f0, sr)
  const hunt = (n, centreF, spanF) => {
    const p = findPeak(x, start, len, centreF - spanF, centreF + spanF, sr)
    if (!p) return { n, ok: false, why: 'edge' }
    const db = 20 * Math.log10(Math.max(p.m, 1e-12) / Math.max(fund, 1e-12))
    if (db < -50) return { n, ok: false, why: 'quiet', db }
    return { n, ok: true, f: p.f, db, dev: cents(p.f, n * f0) }
  }
  const seed = []
  for (let n = 2; n <= Math.min(8, nMax); n++) seed.push(hunt(n, n * f0, 0.45 * f0))
  const B0 = fitB(seed, f0).B
  const parts = []
  for (let n = 2; n <= nMax; n++) {
    const pred = n * f0 * Math.sqrt(1 + Math.max(B0, 0) * n * n)
    parts.push(hunt(n, pred, 0.30 * f0))
  }
  return { f0, fund, parts, ...fitB(parts, f0) }
}

// ── probe cart (generated; filter-spec RECIPE pattern) ──────────────────────
function renderProbe(o, keep) {
  const dir = path.join(ROOT, 'build', '.inharmspec')
  fs.mkdirSync(dir, { recursive: true })
  const wav = path.join(dir, 'probe.wav'), trace = path.join(dir, 'probe.trace.jsonl')
  const cartName = '_inharmspecgen'
  const cartPath = path.join(ROOT, 'tools', 'carts', `${cartName}.c`)
  const src = `#include "studio.h"
// GENERATED by tools/inharm-spec.js — safe to delete.
#define NF ${NF}
#define PER ${PER}
static const int VEL[] = { ${VELS.join(', ')} };
#define NS ((int)(sizeof(VEL)/sizeof(VEL[0])))
static int fnum = -1, held = -1;
void init(void) {
  instrument(5, INSTR_${o.instr}, 1, 0, 7, 2000);   // sustain 7 + held gate: the VOICE's own decay, not the envelope's
  instrument_harmonics(5, ${o.harm.toFixed(6)}f);
  instrument_timbre(5, ${o.timb.toFixed(6)}f);
  instrument_morph(5, ${o.mor.toFixed(6)}f);
}
void update(void) {
  fnum++;
  int idx = fnum / PER, local = fnum % PER;
  if (idx < NS) {
    if (local == 0) held = note_on(${o.midi}, 5, VEL[idx]);
    else if (local == NF - 1 && held >= 0) { note_off(held); held = -1; }
  }
#ifdef DE_TRACE
  watch("seg", "%d", idx < NS ? idx : -1);
#endif
}
void draw(void) { cls(0); }
`
  fs.writeFileSync(cartPath, src)
  try {
    const frames = VELS.length * PER + 30
    const r = spawnSync('node',
      [path.join('tools', 'play.js'), cartName, 'run', '--headless',
       '--frames', String(frames), '--trace', trace, '--wav', wav],
      { cwd: ROOT, encoding: 'utf8' })
    if (r.status !== 0) {
      process.stderr.write((r.stdout || '') + (r.stderr || ''))
      throw new Error(`render failed (play.js ${cartName})`)
    }
  } finally { if (!keep) fs.rmSync(cartPath, { force: true }) }
  return { wav, trace, dir }
}

function segStartFrames(trace, nSegs) {
  const first = new Array(nSegs).fill(-1)
  for (const line of fs.readFileSync(trace, 'utf8').split('\n')) {
    if (!line.trim()) continue
    let j; try { j = JSON.parse(line) } catch { continue }
    const seg = j.w && Number(j.w.seg)
    if (seg >= 0 && seg < nSegs && first[seg] < 0) first[seg] = j.f
  }
  return first
}

// ── reporting ───────────────────────────────────────────────────────────────
const SHOW = [2, 4, 6, 8, 10, 12, 16, 20]
const f2 = (v, w = 6) => (v >= 0 ? '+' : '') + v.toFixed(1).padStart(w - 1)

function header(nMax) {
  const cols = SHOW.filter(n => n <= nMax)
  return { cols,
    line: '  ' + cols.map(n => `h${n}`.padStart(6)).join('') + '  |' + 'B'.padStart(10) + 'resid'.padStart(8) }
}

function row(m, cols) {
  const byN = new Map(m.parts.map(p => [p.n, p]))
  let s = '  '
  for (const n of cols) {
    const p = byN.get(n)
    s += (p && p.ok ? f2(p.dev) : (p ? (p.why === 'edge' ? '  ?' : '  ·') : '   ').padStart(6)).padStart(6)
  }
  s += '  |' + m.B.toExponential(1).padStart(10) + (m.resid.toFixed(1) + '¢').padStart(8)
  return s
}

// ── per-partial DECAY rate (dB/s), least-squares over a sequence of windows ──────────────────
// Each partial is located ONCE in an early window (in a stiff string it is not at n·f0), then tracked.
function decayRates(x, start, len, f0nom, nMax, sr, t0, t1, win) {
  const f0 = refineF0(x, start, len, f0nom, sr)
  const out = []
  for (let n = 1; n <= nMax; n++) {
    // find this partial's ACTUAL frequency, then fit 20log10(mag) against time
    const p = n === 1 ? { f: f0 } : findPeak(x, start, len, n * f0 - 0.45 * f0, n * f0 + 0.45 * f0, sr)
    if (!p) { out.push({ n, ok: false }); continue }
    const pts = []
    for (let t = t0; t + win <= t1; t += win / 2) {
      const s0 = Math.round(t * sr), L = Math.round(win * sr)
      if (s0 + L > x.length) break
      const m = mag(x, s0, L, p.f, sr)
      if (m > 1e-7) pts.push([t + win / 2, 20 * Math.log10(m)])
    }
    if (pts.length < 4) { out.push({ n, ok: false }); continue }
    const k = pts.length
    const mx = pts.reduce((a, q) => a + q[0], 0) / k, my = pts.reduce((a, q) => a + q[1], 0) / k
    let sxy = 0, sxx = 0
    for (const [t, d] of pts) { sxy += (t - mx) * (d - my); sxx += (t - mx) * (t - mx) }
    out.push({ n, ok: sxx > 0, f: p.f, dbPerSec: sxx > 0 ? sxy / sxx : null })
  }
  return { f0, parts: out }
}

// ── self-test: synthesise a decaying stiff-string spectrum at a KNOWN B and recover it ──────
function selfCheck(nMax) {
  const sr = 44100, f0 = 130.81, dur = 0.30, len = Math.round(dur * sr)
  let fails = 0
  console.log('\ninharm-spec --check   synthetic stiff-string spectra, B recovered from the partials\n')
  console.log('      true B    fitted B    err      h16 true    h16 read    resid')
  for (const Btrue of [0, 1e-5, 1e-4, 5e-4]) {
    const x = new Float64Array(len)
    for (let n = 1; n <= nMax; n++) {
      const fn = n * f0 * Math.sqrt(1 + Btrue * n * n)
      const amp = 1 / n                                        // a plausible struck-string rolloff (1/n, not 1/n²
                                                               // — 1/n² buries h16 under the −50 dB noise gate)
      const decay = 2.0 + n * 0.1                              // higher partials die faster, as in a string
      for (let i = 0; i < len; i++)
        x[i] += amp * Math.exp(-decay * i / sr) * Math.sin(2 * Math.PI * fn * i / sr + n)
    }
    let pk = 0; for (let i = 0; i < len; i++) pk = Math.max(pk, Math.abs(x[i]))
    for (let i = 0; i < len; i++) x[i] = Math.round(x[i] / pk * 32767 * 0.9) / 32768   // 16-bit, as a real WAV
    const m = measure(x, 0, len, f0, nMax, sr)
    const h16true = cents(16 * f0 * Math.sqrt(1 + Btrue * 256), 16 * f0)
    const p16 = m.parts.find(p => p.n === 16)
    const h16read = p16 && p16.ok ? p16.dev : NaN
    // tolerance: B to 15% (or 2e-6 absolute at B=0), and h16 to 2 cents
    const bOK = Math.abs(m.B - Btrue) <= Math.max(0.15 * Btrue, 2e-6)
    const hOK = Number.isFinite(h16read) && Math.abs(h16read - h16true) <= 2.0
    if (!bOK || !hOK) fails++
    console.log(`  ${Btrue.toExponential(0).padStart(10)}  ${m.B.toExponential(2).padStart(10)}` +
      `  ${bOK ? ' ok ' : 'FAIL'}  ${f2(h16true, 10)}¢ ${f2(h16read, 11)}¢  ${m.resid.toFixed(2)}¢ ${hOK ? '' : ' FAIL'}`)
  }
  console.log(fails === 0
    ? '\nPASS — the tool resolves B from 1e-5 upward and reads 0 as 0, so a null result is a real null.\n'
    : `\nFAIL — ${fails} case(s) off; do not trust a measurement until this is green.\n`)
  return fails === 0
}

function main() {
  const argv = process.argv.slice(2)
  const flag = (k, d) => { const i = argv.indexOf(k); return i >= 0 ? argv[i + 1] : d }
  const has = k => argv.includes(k)
  const nMax = parseInt(flag('--n', '20'), 10)
  const json = has('--json'), keep = has('--keep')
  const wavArg = argv.find(a => !a.startsWith('--') && /\.wav$/i.test(a))

  if (has('--check')) { process.exit(selfCheck(Math.max(nMax, 16)) ? 0 : 1) }

  if (wavArg) {                                    // ── WAV mode (engine-agnostic) ──
    const f0nom = parseFloat(flag('--f0', '0'))
    if (!f0nom) { console.error('inharm-spec: --f0 <hz> is required in WAV mode'); process.exit(2) }
    const { sr, s } = readWavMono(wavArg)
    const from = parseFloat(flag('--from', '0')), to = parseFloat(flag('--to', String(s.length / sr)))
    const start = Math.max(0, Math.round(from * sr))
    const len = Math.min(s.length - start, Math.round((to - from) * sr))
    if (len < 2048) { console.error('inharm-spec: region too short (need ≥2048 samples)'); process.exit(2) }

    if (has('--decay')) {                          // per-partial decay rate, not cents
      const t0 = parseFloat(flag('--from', '0.10')), t1 = parseFloat(flag('--to', '1.20'))
      const win = parseFloat(flag('--win', '0.18'))
      // locate the partials in an early window, then fit each one's level over [t0, t1]
      const locStart = Math.round(t0 * sr), locLen = Math.min(Math.round(0.25 * sr), s.length - locStart)
      const d = decayRates(s, locStart, locLen, f0nom, nMax, sr, t0, t1, win)
      if (json) { console.log(JSON.stringify({ wav: wavArg, from: t0, to: t1, ...d }, null, 2)); return }
      console.log(`\nPER-PARTIAL DECAY  ${path.basename(wavArg)}  ${t0}-${t1}s  (f0 ${d.f0.toFixed(2)} Hz)`)
      console.log('more negative = dies sooner. Compare the SAME partial across two takes.\n')
      console.log('  n      freq     dB/s')
      for (const p of d.parts)
        console.log(`  ${String(p.n).padStart(2)}  ${p.ok ? p.f.toFixed(1).padStart(8) : '       ?'}  ` +
          `${p.ok && p.dbPerSec !== null ? p.dbPerSec.toFixed(1).padStart(7) : '      ?'}`)
      return
    }

    const m = measure(s, start, len, f0nom, nMax, sr)
    if (json) { console.log(JSON.stringify({ wav: wavArg, from, to, ...m }, null, 2)); return }
    const h = header(nMax)
    console.log(`\nINHARMONICITY  ${path.basename(wavArg)}  ${from.toFixed(2)}-${to.toFixed(2)}s`)
    console.log(`f0 ${f0nom.toFixed(2)} Hz nominal → ${m.f0.toFixed(2)} measured (${f2(cents(m.f0, f0nom)).trim()}¢)\n`)
    console.log('cents sharp of n·f0' + h.line.slice(19 > h.line.length ? 0 : 0))
    console.log(' '.repeat(19) + h.line)
    console.log(' '.repeat(19) + row(m, h.cols))
    console.log('\n  ? = drifted past half the partial spacing (ambiguous)   · = >50 dB down (noise)')
    return
  }

  // ── probe mode (the §I4 matrix, on any engine) ──
  const instr = (flag('--instr', 'PIANO')).toUpperCase()
  const midi = parseInt(flag('--midi', '48'), 10)
  const voicing = flag('--voicing', null)
  const o = { instr, midi,
    harm: voicing !== null ? parseInt(voicing, 10) / 5.999 : parseFloat(flag('--harm', '0.5')),
    timb: parseFloat(flag('--timb', '0.5')),
    mor:  parseFloat(flag('--mor', instr === 'PIANO' ? '0' : '0.5')) }   // piano: pedal up, no damper slew
  const f0nom = 440 * Math.pow(2, (midi - 69) / 12)
  const { wav, trace, dir } = renderProbe(o, keep)
  const { sr, s } = readWavMono(wav)
  const firsts = segStartFrames(trace, VELS.length)
  const rows = []
  for (let i = 0; i < VELS.length; i++) {
    if (firsts[i] < 0) continue
    const t0 = firsts[i] / 60
    for (const w of WINDOWS) {
      const start = Math.round((t0 + w.a) * sr)
      const len = Math.round((w.b - w.a) * sr)
      if (start + len > s.length) continue
      let rms = 0
      for (let k = 0; k < len; k++) rms += s[start + k] * s[start + k]
      rms = Math.sqrt(rms / len)
      // A decayed window must read as SILENT, not as a table of ambiguous partials — otherwise the
      // top-octave carts (which die inside 0.5 s, audit §I5) look like an inharmonicity finding.
      rows.push(rms < 1e-4
        ? { vel: VELS[i], window: w.name, silent: true, rms, f0: 0, parts: [], B: 0, resid: 0 }
        : { vel: VELS[i], window: w.name, rms, ...measure(s, start, len, f0nom, nMax, sr) })
    }
  }
  if (json) { console.log(JSON.stringify({ instr, midi, ...o, f0nom, rows }, null, 2)); return }

  const h = header(nMax)
  console.log(`\nINHARMONICITY — INSTR_${instr}  midi ${midi} (${f0nom.toFixed(1)} Hz nominal)` +
              `  h${o.harm.toFixed(2)} t${o.timb.toFixed(2)} m${o.mor.toFixed(2)}`)
  console.log('cents sharp of the ideal n·f0, per strike velocity and per window\n')
  console.log('vel  window            f0' + h.line)
  let lastVel = null
  for (const r of rows) {
    if (lastVel !== null && r.vel !== lastVel) console.log('')
    lastVel = r.vel
    if (r.silent) { console.log(`${String(r.vel).padStart(3)}  ${r.window.padEnd(6)}   — silent (rms ${r.rms.toExponential(1)})`); continue }
    console.log(`${String(r.vel).padStart(3)}  ${r.window.padEnd(6)} ${r.f0.toFixed(2).padStart(8)}` + row(r, h.cols))
  }
  console.log('\n  ? = drifted past half the partial spacing (ambiguous)   · = >50 dB down (noise)')
  console.log('  B = fitted stiff-string coefficient, resid = rms departure from that law')
  if (keep) console.log(`\n  kept: ${dir}`)
}

main()
