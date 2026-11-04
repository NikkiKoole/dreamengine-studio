#!/usr/bin/env node
// oneshot-compare.js — measure a ONE-SHOT (a drum hit, a cymbal stroke, a pluck) and hold it against a
// REAL recording of the same sound. The method that retuned cr78, the INSTR_SCRAPE guiro and drumhand's
// kit (2026-11-04), made a tool so the next voice does not start from scratch scripts.
//
//   node tools/oneshot-compare.js <file> [--at s] [--len s]           measure one hit
//   node tools/oneshot-compare.js <ref> <ours> [--at s] [--len s]     ref vs ours, side by side
//        (--at/--len cut OURS out of a longer render; --ref-at/--ref-len cut the reference)
//   node tools/oneshot-compare.js --render <cart> --spec <hits.json> [--define A,B] [--frames n]
//        render tools/carts/<cart>.c deterministic + headless (play.js), then compare every hit in the
//        spec to its reference. spec = [{ "label", "at", "len", "ref", "refAt"?, "refLen"? }, ...];
//        a relative "ref" resolves against the spec's own folder, then build/refs/
//   --rel <label>   express each hit's level relative to that hit, in ours AND in the references (the
//                   cr78 method: a real kit recorded at one gain gives each voice's balance vs the kick)
//   --json          machine-readable · --selfcheck = known answers on synthetic signals, no ffmpeg
//
// WHAT IT MEASURES, and why each one (each found a real defect):
//   peak / rms       dBFS. Alone they say little; relative to a reference hit (--rel) they are the mix
//   band 10/50/90    the frequencies under which 10/50/90% of the ENERGY sits, over the first 400 ms.
//                    Found every cymbal hissing at 15-20 kHz (a real hat stops ~14), and the dark
//                    "wash" ride that made drumhand's ride a drone (0.4-2 kHz vs a ping ride's 0.6-12)
//   peaks            the strongest spectral peaks, freq@dB below the top. Found the kit drums' missing
//                    second mode (a real kick: 54 Hz + 118 at -3.5 dB; ours had it 20 dB down)
//   flatness         0 = pure tone .. 1 = white noise (100 Hz-16 kHz). Found the hiss in "hiss"
//   pitch 0/10/40ms  the strongest bin 30 Hz-3 kHz at onset, +10 and +40 ms: a pitch drop, or none
//   ->-20dB / len    time from the peak to -20 dB, and onset to the last moment above -40 dB
//   early curve      the envelope in 5 ms steps for 150 ms, dB re the peak. Found the missing STICK
//                    HIT on the ride: a real one falls 6-7 dB in 40 ms, ours 1 dB
// Everything is read from the ONSET (first 5 ms window above -30 dB of the peak), never the cut's start,
// so a hit sitting 300 ms into its window measures the same as one starting at 0.
//
// ⚠ It MEASURES, it does not judge: a sound can match every number and still be wrong to the ear (the
// drumhand ride matched a dark reference and read as "a train locomotive"). Pick the reference by ear
// first; then use this to get there. Files are decoded through ffmpeg (any format, any rate, mono 44.1k).
'use strict'
const fs = require('fs'), path = require('path'), { execFileSync, spawnSync } = require('child_process')
const ROOT = path.resolve(__dirname, '..'), SR = 44100

function load(file, start, len) {
  const a = ['-v', 'error']
  if (start != null) a.push('-ss', String(start))
  if (len != null) a.push('-t', String(len))
  a.push('-i', file, '-ac', '1', '-ar', String(SR), '-f', 'f32le', '-')
  const b = execFileSync('ffmpeg', a, { maxBuffer: 1 << 28 })
  return new Float32Array(b.buffer, b.byteOffset, b.length / 4)
}

function fft(re, im) {
  const n = re.length
  for (let i = 1, j = 0; i < n; i++) {
    let bit = n >> 1
    for (; j & bit; bit >>= 1) j ^= bit
    j ^= bit
    if (i < j) { [re[i], re[j]] = [re[j], re[i]];[im[i], im[j]] = [im[j], im[i]] }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const ang = -2 * Math.PI / len, wr = Math.cos(ang), wi = Math.sin(ang)
    for (let i = 0; i < n; i += len) {
      let cr = 1, ci = 0
      for (let k = 0; k < len / 2; k++) {
        const ar = re[i + k + len / 2] * cr - im[i + k + len / 2] * ci, ai = re[i + k + len / 2] * ci + im[i + k + len / 2] * cr
        re[i + k + len / 2] = re[i + k] - ar; im[i + k + len / 2] = im[i + k] - ai
        re[i + k] += ar; im[i + k] += ai
        const t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t
      }
    }
  }
}
function power(x, a, w, N) {   // Hann-windowed power spectrum of x[a..a+w), zero-padded to N
  const re = new Float64Array(N), im = new Float64Array(N)
  for (let i = 0; i < w && a + i < x.length; i++) re[i] = x[a + i] * (0.5 - 0.5 * Math.cos(2 * Math.PI * i / (w - 1)))
  fft(re, im)
  const p = new Float64Array(N / 2)
  for (let i = 0; i < N / 2; i++) p[i] = re[i] * re[i] + im[i] * im[i]
  return p
}
const dB = v => 20 * Math.log10(Math.max(v, 1e-9))

function analyze(x) {
  const hop = Math.round(SR * 0.005), env = []
  for (let i = 0; i + hop <= x.length; i += hop) {
    let s = 0; for (let k = 0; k < hop; k++) s += x[i + k] * x[i + k]
    env.push(Math.sqrt(s / hop))
  }
  let pk = 0, pki = 0
  env.forEach((v, i) => { if (v > pk) { pk = v; pki = i } })
  if (pk < 1e-6) return { silent: true }
  let on = 0; while (on < env.length && env[on] < pk * 0.03) on++                 // -30 dB onset
  let end = env.length - 1; while (end > on && env[end] < pk * 0.01) end--          // last above -40 dB
  let d20 = pki; while (d20 < env.length && env[d20] > pk * 0.1) d20++             // peak -> -20 dB
  let peakSample = 0, sum = 0; for (const v of x) { peakSample = Math.max(peakSample, Math.abs(v)); sum += v * v }
  const o = on * hop, lenS = (end - on) * hop
  // spectrum over the sounding part, max 400 ms
  const W = 2048, N = 4096, span = Math.min(lenS, SR * 0.4), acc = new Float64Array(N / 2)
  let nf = 0
  for (let a = o; a < o + Math.max(span - W, 1); a += W / 2) { power(x, a, W, N).forEach((v, i) => acc[i] += v); nf++ }
  if (!nf) power(x, o, W, N).forEach((v, i) => acc[i] += v)
  const bin = SR / N
  let tot = 0; for (let i = 1; i < N / 2; i++) tot += acc[i]
  const q = [0.1, 0.5, 0.9], band = []; let run = 0, qi = 0, cen = 0
  for (let i = 1; i < N / 2; i++) {
    run += acc[i]; cen += acc[i] * i * bin
    while (qi < 3 && run >= q[qi] * tot) { band.push(Math.round(i * bin)); qi++ }
  }
  let lg = 0, ar = 0, n = 0
  for (let i = Math.ceil(100 / bin); i < 16000 / bin; i++) { lg += Math.log(acc[i] + 1e-20); ar += acc[i]; n++ }
  const flatness = Math.exp(lg / n) / (ar / n)
  let mx = 0; for (let i = 1; i < N / 2; i++) mx = Math.max(mx, acc[i])
  const peaks = []
  for (let i = Math.ceil(30 / bin); i < 18000 / bin; i++) {
    if (acc[i] < acc[i - 1] || acc[i] < acc[i + 1]) continue
    let ok = true; for (let k = -3; k <= 3; k++) if (k && acc[i + k] > acc[i]) ok = false
    if (!ok) continue
    let lo = Infinity; for (let k = -12; k <= 12; k++) if (k && acc[i + k] != null) lo = Math.min(lo, acc[i + k])
    if (10 * Math.log10(acc[i] / lo) > 6) peaks.push({ hz: Math.round(i * bin), db: +(10 * Math.log10(acc[i] / mx)).toFixed(1) })
  }
  peaks.sort((a, b) => b.db - a.db)
  const pitchAt = t => {
    const a = o + Math.round(t * SR); if (a + 1024 > x.length) return null
    const p = power(x, a, 1024, 16384), b = SR / 16384; let bi = 0, bv = 0
    for (let i = Math.ceil(30 / b); i < 3000 / b; i++) if (p[i] > bv) { bv = p[i]; bi = i }
    return Math.round(bi * b)
  }
  const early = []
  for (let i = on; i < on + 30 && i < env.length; i++) early.push(+(20 * Math.log10(Math.max(env[i], 1e-9) / pk)).toFixed(1))
  return {
    peak_dBFS: +dB(peakSample).toFixed(1), rms_dBFS: +dB(Math.sqrt(sum / x.length)).toFixed(1),
    attack_ms: (pki - on) * 5, to_m20_ms: (d20 - pki) * 5, len_ms: Math.round(lenS / SR * 1000),
    band, centroid: Math.round(cen / tot), flatness: +flatness.toFixed(3),
    peaks: peaks.slice(0, 4), pitch: [pitchAt(0), pitchAt(0.01), pitchAt(0.04)], early,
  }
}

// ── reporting ──────────────────────────────────────────────────────────────────────────────────────
const fmtPk = p => p.map(k => `${k.hz}@${k.db}`).join(' ')
function line(tag, m, rel) {
  if (m.silent) return `  ${tag.padEnd(5)} SILENT (nothing above -120 dB)`
  const lvl = rel != null ? `  rel ${(m.peak_dBFS - rel >= 0 ? '+' : '') + (m.peak_dBFS - rel).toFixed(1)} dB` : `  pk ${m.peak_dBFS}`
  return `  ${tag.padEnd(5)}${lvl}  band ${m.band.join('/').padEnd(17)} flat ${m.flatness.toFixed(2)}  ->-20dB ${String(m.to_m20_ms).padStart(4)} ms  len ${String(m.len_ms).padStart(4)}  peaks ${fmtPk(m.peaks)}  pitch ${m.pitch.join('/')}`
}
function curve(tag, m) {
  if (m.silent) return ''
  return `  ${tag.padEnd(5)}early ${m.early.slice(0, 30).filter((_, i) => i % 2 === 0).map(v => String(Math.round(v)).padStart(4)).join('')}   (dB re peak, 10 ms steps)`
}
// the deltas worth reading first: band points as % off, decay ratio, flatness
function deltas(r, o) {
  if (r.silent || o.silent) return '  Δ     (one side silent)'
  const pct = (a, b) => `${a >= b ? '+' : ''}${Math.round((a / b - 1) * 100)}%`
  return `  Δ     band ${o.band.map((v, i) => pct(v, r.band[i])).join(' / ')}   ->-20dB ${pct(Math.max(o.to_m20_ms, 1), Math.max(r.to_m20_ms, 1))}   flat ${(o.flatness - r.flatness >= 0 ? '+' : '') + (o.flatness - r.flatness).toFixed(2)}`
}

function resolveRef(ref, base) {
  for (const c of [path.resolve(base, ref), path.join(ROOT, 'build/refs', ref), path.join(ROOT, ref)]) if (fs.existsSync(c)) return c
  throw new Error(`reference not found: ${ref}`)
}

function main(argv) {
  const opt = (k, d) => { const i = argv.indexOf(k); return i >= 0 ? argv[i + 1] : d }
  const json = argv.includes('--json')
  if (argv.includes('--selfcheck')) return selfcheck()
  const render = opt('--render'), rel = opt('--rel')
  if (render) {
    const specPath = opt('--spec'); if (!specPath) throw new Error('--render needs --spec <hits.json>')
    const spec = JSON.parse(fs.readFileSync(specPath, 'utf8')), base = path.dirname(path.resolve(specPath))
    const end = Math.max(...spec.map(h => h.at + h.len))
    const frames = +opt('--frames', Math.ceil((end + 0.5) * 60))
    const wav = path.join(ROOT, 'build', `oneshot-${render}.wav`)
    const env = { ...process.env }; if (opt('--define')) env.DE_DEFINES = opt('--define')
    const r = spawnSync('node', [path.join(ROOT, 'tools/play.js'), render, 'run', '--det', '--headless', '--frames', String(frames), '--wav', wav], { env, encoding: 'utf8' })
    if (r.status !== 0 || !fs.existsSync(wav)) throw new Error(`render failed:\n${r.stdout}${r.stderr}`)
    const rows = spec.map(h => ({ ...h, ours: analyze(load(wav, h.at, h.len)), real: analyze(load(resolveRef(h.ref, base), h.refAt, h.refLen)) }))
    const R = rel ? rows.find(h => h.label === rel) : null
    if (rel && !R) throw new Error(`--rel ${rel}: no hit with that label`)
    if (json) return console.log(JSON.stringify(rows, null, 1))
    for (const h of rows) {
      console.log(`\n${h.label}  (ref ${path.basename(h.ref)})`)
      console.log(line('real', h.real, R && !R.real.silent ? R.real.peak_dBFS : null))
      console.log(line('ours', h.ours, R && !R.ours.silent ? R.ours.peak_dBFS : null))
      console.log(deltas(h.real, h.ours))
    }
    return
  }
  const files = argv.filter((a, i) => !a.startsWith('--') && !(i > 0 && argv[i - 1].startsWith('--') && !['--json'].includes(argv[i - 1])))
  if (!files.length) { console.error('usage: see the header of tools/oneshot-compare.js'); process.exit(2) }
  const num = k => opt(k) != null ? +opt(k) : null
  if (files.length === 1) {
    const m = analyze(load(files[0], num('--at'), num('--len')))
    if (json) return console.log(JSON.stringify(m))
    console.log(path.basename(files[0])); console.log(line('hit', m)); console.log(curve('hit', m)); return
  }
  const r = analyze(load(files[0], num('--ref-at'), num('--ref-len'))), o = analyze(load(files[1], num('--at'), num('--len')))
  if (json) return console.log(JSON.stringify({ ref: r, ours: o }))
  console.log(`ref  ${path.basename(files[0])}\nours ${path.basename(files[1])}`)
  console.log(line('real', r)); console.log(line('ours', o)); console.log(deltas(r, o))
  console.log(curve('real', r)); console.log(curve('ours', o))
}

// ── selfcheck: known answers on synthetic signals (no ffmpeg, no cart) ────────────────────────────
function selfcheck() {
  let pass = 0, fail = 0
  const ok = (cond, what) => { if (cond) pass++; else { fail++; console.log(`  ✗ ${what}`) } }
  const near = (v, want, tol) => Math.abs(v - want) <= tol
  const sig = (sec, fn, pre = 0) => { const n = Math.round(sec * SR), x = new Float32Array(n), p0 = Math.round(pre * SR); for (let i = p0; i < n; i++) x[i] = fn((i - p0) / SR); return x }
  let seed = 12345; const rnd = () => { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed / 0x7fffffff * 2 - 1 }
  // 1. a decaying 440 Hz sine: pitch, band, a pure tone, the top peak
  const tau = 0.05, s440 = sig(1, t => 0.8 * Math.exp(-t / tau) * Math.sin(2 * Math.PI * 440 * t))
  const a = analyze(s440)
  ok(near(a.pitch[0], 440, 6), `sine pitch 440 (got ${a.pitch[0]})`)
  ok(near(a.band[1], 440, 25), `sine band median ~440 (got ${a.band[1]})`)
  ok(a.flatness < 0.02, `sine flatness < 0.02 (got ${a.flatness})`)
  ok(a.peaks.length && near(a.peaks[0].hz, 440, 12), `sine top peak ~440 (got ${a.peaks[0] && a.peaks[0].hz})`)
  // 2. decay: an exp(-t/tau) envelope crosses -20 dB at tau*ln(10) = 115 ms (5 ms resolution)
  ok(near(a.to_m20_ms, 115, 10), `-20 dB at ~115 ms for tau 50 ms (got ${a.to_m20_ms})`)
  // 3. the onset, not the window's start: the same hit 300 ms into its window measures the same
  const late = analyze(sig(1.3, t => 0.8 * Math.exp(-t / tau) * Math.sin(2 * Math.PI * 440 * t), 0.3))
  // (len + the early curve are counted FROM the onset; -20dB and the band alone cannot see a broken
  // onset, since silence before a hit adds no energy and the peak is found either way: mutation-tested)
  ok(late.to_m20_ms === a.to_m20_ms && late.band[1] === a.band[1] && Math.abs(late.len_ms - a.len_ms) <= 15 && Math.abs(late.early[2] - a.early[2]) <= 1.5,   // ±window phase; a broken onset is off by the whole 300 ms
     `a late onset measures the same (-20dB ${late.to_m20_ms}/${a.to_m20_ms}, len ${late.len_ms}/${a.len_ms}, early ${late.early[2]}/${a.early[2]})`)
  // 4. white noise: flat, energy spread evenly 0..22 kHz (10/50/90% at ~2.2/11/19.8 kHz)
  const wn = analyze(sig(0.6, () => 0.5 * rnd()))
  ok(wn.flatness > 0.8, `white noise flatness > 0.8 (got ${wn.flatness})`)
  ok(near(wn.band[0], 2205, 400) && near(wn.band[1], 11025, 900) && near(wn.band[2], 19845, 900), `white noise band ~2.2/11/19.8 kHz (got ${wn.band.join('/')})`)
  // 5. two partials: both found, the weaker one's level read right (-12 dB)
  const two = analyze(sig(0.8, t => Math.exp(-t / 0.2) * (0.6 * Math.sin(2 * Math.PI * 300 * t) + 0.15 * Math.sin(2 * Math.PI * 3000 * t))))
  const p3k = two.peaks.find(p => near(p.hz, 3000, 15))
  ok(two.peaks.length >= 2 && near(two.peaks[0].hz, 300, 12), `two partials: top at 300 (got ${fmtPk(two.peaks)})`)
  ok(p3k && near(p3k.db, -12, 1.5), `two partials: 3 kHz at -12 dB (got ${p3k ? p3k.db : 'none'})`)
  // 5b. a partial only modestly above a noise floor (a cymbal partial in its wash) is still a PEAK.
  // Every other test tone stands 60+ dB clear, which let a 10x-too-strict prominence rule pass (mutation)
  const wash = analyze(sig(0.6, t => 0.3 * rnd() + 0.03 * Math.sin(2 * Math.PI * 5000 * t)))
  ok(wash.peaks.some(p => near(p.hz, 5000, 15)), `a 5 kHz partial in a noise wash is found (got ${fmtPk(wash.peaks)})`)
  // 6. the early curve: a steady tone stays at 0, a fast decay falls (the ride's missing stick hit)
  const steady = analyze(sig(0.4, t => 0.5 * Math.sin(2 * Math.PI * 1000 * t)))
  ok(steady.early.slice(0, 20).every(v => v > -1), `a steady tone's early curve stays at 0 dB`)
  ok(a.early[8] < -5, `a 50 ms decay is below -5 dB 40 ms in (got ${a.early[8]})`)
  // 7. level: half amplitude reads -6 dB
  const half = analyze(sig(1, t => 0.4 * Math.exp(-t / tau) * Math.sin(2 * Math.PI * 440 * t)))
  ok(near(a.peak_dBFS - half.peak_dBFS, 6.02, 0.2), `half amplitude = -6 dB (got ${(a.peak_dBFS - half.peak_dBFS).toFixed(2)})`)
  // 8. NEGATIVE CONTROL: silence must read SILENT, not as a hit with numbers
  ok(analyze(new Float32Array(SR)).silent === true, `silence reads SILENT`)
  console.log(`oneshot-compare selfcheck: ${fail ? 'FAIL' : 'PASS'} (${pass}/${pass + fail})`)
  process.exit(fail ? 1 : 0)
}

if (require.main === module) {
  try { main(process.argv.slice(2)) } catch (e) { console.error(`oneshot-compare: ${e.message}`); process.exit(1) }
}
module.exports = { analyze, load }
