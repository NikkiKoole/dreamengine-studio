#!/usr/bin/env node
// wav-analyze.js — tier-1 signal metrics for engine WAVs (audio-notes §16).
// No dependencies; reads the 16-bit PCM mono/stereo WAVs the runtime writes
// (.bake/wav_request live capture, or the --wav deterministic render).
//
//   node tools/wav-analyze.js <file.wav>            human-readable report
//   node tools/wav-analyze.js <file.wav> --json     machine-readable
//   node tools/wav-analyze.js <a.wav> <b.wav>       compare two renders
//   node tools/wav-analyze.js <file.wav> --stereo   per-channel + stereo-field report
//
// Default (mono) metrics: duration, peak / RMS / crest (dBFS), clipped-sample
// count and runs (|s| at full scale), DC offset, per-second RMS envelope. The
// default path downmixes L+R→mono, so it's blind to the stereo field — use
// --stereo for panning/width/effects-bus work (stereo.md).
//
// --stereo metrics (per channel + cross-channel): L/R peak & RMS, balance
// (R−L in dB), inter-channel correlation (+1 mono · 0 wide · <0 anti-phase),
// the MONO-FOLD test (sum to mono, measure power lost — the stereo.md bite-#6
// check that a width trick doesn't comb-cancel on a phone speaker; amplitude
// panning is safe, delay/phase tricks aren't), and a per-second pan trajectory.

const fs = require('fs')

function readWav(file) {
  const b = fs.readFileSync(file)
  if (b.toString('ascii', 0, 4) !== 'RIFF' || b.toString('ascii', 8, 12) !== 'WAVE')
    throw new Error(`${file}: not a WAV`)
  // walk chunks for fmt + data
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
  if (fmt.tag !== 1 || fmt.bits !== 16 || (fmt.ch !== 1 && fmt.ch !== 2))
    throw new Error(`${file}: expected 16-bit PCM mono/stereo, got tag=${fmt.tag} bits=${fmt.bits} ch=${fmt.ch}`)
  // stereo (the runtime went stereo — stereo.md): downmix L/R to mono for the metrics.
  // While voices are centered this equals the old mono sample exactly; the master soft-clip
  // is applied pre-split so neither channel clips independently, so the mean stays representative.
  const ch = fmt.ch
  const n = Math.floor(data.len / 2 / ch)
  const s = new Int16Array(n)
  for (let i = 0; i < n; i++) {
    if (ch === 1) s[i] = b.readInt16LE(data.off + i * 2)
    else s[i] = (b.readInt16LE(data.off + i * 4) + b.readInt16LE(data.off + i * 4 + 2)) >> 1
  }
  return { sr: fmt.sr, s }
}

const db = (x) => x > 0 ? (20 * Math.log10(x)).toFixed(2) : '-inf'

// like readWav but keeps L and R separate (R === L for a mono file), normalized to -1..1
function readWavLR(file) {
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
  if (fmt.tag !== 1 || fmt.bits !== 16 || (fmt.ch !== 1 && fmt.ch !== 2))
    throw new Error(`${file}: expected 16-bit PCM mono/stereo, got tag=${fmt.tag} bits=${fmt.bits} ch=${fmt.ch}`)
  const ch = fmt.ch
  const n = Math.floor(data.len / 2 / ch)
  const L = new Float64Array(n), R = new Float64Array(n)
  for (let i = 0; i < n; i++) {
    L[i] = b.readInt16LE(data.off + i * 2 * ch) / 32768
    R[i] = ch === 2 ? b.readInt16LE(data.off + i * 2 * ch + 2) / 32768 : L[i]
  }
  return { sr: fmt.sr, ch, L, R }
}

function analyzeStereo(file) {
  const { sr, ch, L, R } = readWavLR(file)
  const n = L.length
  let pkL = 0, pkR = 0, sL = 0, sR = 0, sLR = 0, sMono = 0
  for (let i = 0; i < n; i++) {
    const l = L[i], r = R[i]
    if (Math.abs(l) > pkL) pkL = Math.abs(l)
    if (Math.abs(r) > pkR) pkR = Math.abs(r)
    sL += l * l; sR += r * r; sLR += l * r
    const m = (l + r) * 0.5; sMono += m * m
  }
  const rmsL = Math.sqrt(sL / n), rmsR = Math.sqrt(sR / n), rmsMono = Math.sqrt(sMono / n)
  const corr = (sL > 0 && sR > 0) ? sLR / Math.sqrt(sL * sR) : 1
  const balanceDb = (rmsL > 0 && rmsR > 0) ? 20 * Math.log10(rmsR / rmsL) : 0
  const avgCh = Math.sqrt((sL + sR) / (2 * n))                    // = sqrt((rmsL²+rmsR²)/2)
  const foldLoss = (avgCh > 0 && rmsMono > 0) ? 20 * Math.log10(rmsMono / avgCh) : -Infinity
  // fold loss: 0dB = mono-compatible (correlated); ~-3dB = naturally decorrelated/wide (fine,
  // and where amplitude panning lands); < -6dB = power vanishing on mono-fold → a phase/delay
  // width trick that'll comb-cancel on a mono phone speaker (stereo.md bite #6 — investigate).
  const verdict = !(foldLoss > -6) ? 'WARN — phase cancellation on mono-fold (bite #6)'
                : foldLoss < -1 ? 'wide / decorrelated — expected for width' : 'mono-compatible'
  // per-second pan trajectory: -1 (hard L) .. 0 (center) .. +1 (hard R) from L/R energy
  const traj = []
  for (let t = 0; t < n; t += sr) {
    const end = Math.min(t + sr, n); let el = 0, er = 0
    for (let i = t; i < end; i++) { el += L[i] * L[i]; er += R[i] * R[i] }
    const rl = Math.sqrt(el / (end - t)), rr = Math.sqrt(er / (end - t))
    traj.push((rl + rr) > 0 ? (rr - rl) / (rr + rl) : 0)
  }
  return {
    file, sr, channels: ch, mono: ch === 1, samples: n, seconds: +(n / sr).toFixed(3),
    peakLDb: db(pkL), peakRDb: db(pkR), rmsLDb: db(rmsL), rmsRDb: db(rmsR),
    balanceDb: +balanceDb.toFixed(2), correlation: +corr.toFixed(4),
    monoFoldLossDb: foldLoss === -Infinity ? '-inf' : +foldLoss.toFixed(2),
    monoFoldVerdict: verdict,
    panPerSecond: traj.map(p => +p.toFixed(3)),
  }
}

function panBar(p) {                         // -1..+1 → a 21-cell L···|···R meter
  const i = Math.max(0, Math.min(20, Math.round((p + 1) * 10)))
  return '['.padEnd(0) + '·'.repeat(i) + '@' + '·'.repeat(20 - i) + ']'
}

function reportStereo(r) {
  console.log(`${r.file}`)
  console.log(`  ${r.seconds}s  ${r.samples} samples @ ${r.sr}Hz  (${r.mono ? 'MONO file' : 'stereo'})`)
  console.log(`  peak   L ${r.peakLDb}  R ${r.peakRDb} dBFS`)
  console.log(`  rms    L ${r.rmsLDb}  R ${r.rmsRDb} dBFS`)
  console.log(`  balance ${r.balanceDb} dB (R−L; 0 = centered, + = right-heavy)`)
  console.log(`  correlation ${r.correlation} (+1 mono · 0 wide · <0 anti-phase)`)
  console.log(`  mono-fold loss ${r.monoFoldLossDb} dB  →  ${r.monoFoldVerdict}`)
  if (r.mono) { console.log('  (mono file — no stereo field to report)'); return }
  console.log('  pan/sec (L··@··R):')
  r.panPerSecond.forEach((p, i) => console.log(`    ${String(i).padStart(3)}s ${p >= 0 ? '+' : ''}${p.toFixed(2)} ${panBar(p)}`))
}

function analyze(file) {
  const { sr, s } = readWav(file)
  const n = s.length
  let peak = 0, sum2 = 0, sum = 0, clipped = 0, clipRuns = 0, inRun = false
  for (let i = 0; i < n; i++) {
    const v = s[i] / 32768
    const a = Math.abs(v)
    if (a > peak) peak = a
    sum2 += v * v
    sum += v
    const clip = s[i] >= 32766 || s[i] <= -32767   // at/over full scale (16-bit rails)
    if (clip) { clipped++; if (!inRun) { clipRuns++; inRun = true } } else inRun = false
  }
  const rms = Math.sqrt(sum2 / n)
  // per-second RMS envelope
  const env = []
  for (let t = 0; t + sr <= n || (t < n && env.length === 0); t += sr) {
    let e = 0; const end = Math.min(t + sr, n)
    for (let i = t; i < end; i++) { const v = s[i] / 32768; e += v * v }
    env.push(Math.sqrt(e / (end - t)))
  }
  return {
    file, sr, samples: n, seconds: +(n / sr).toFixed(3),
    peak: +peak.toFixed(5), peakDb: db(peak),
    rms: +rms.toFixed(5), rmsDb: db(rms),
    crestDb: peak > 0 && rms > 0 ? +(20 * Math.log10(peak / rms)).toFixed(2) : null,
    dcOffset: +(sum / n).toFixed(6),
    clippedSamples: clipped, clipRuns,
    clippedPct: +((clipped / n) * 100).toFixed(4),
    rmsPerSecond: env.map(e => +e.toFixed(4)),
  }
}

function bar(v, scale = 40) { return '#'.repeat(Math.min(scale, Math.round(v * scale * 2))) }

function report(r) {
  console.log(`${r.file}`)
  console.log(`  ${r.seconds}s  ${r.samples} samples @ ${r.sr}Hz`)
  console.log(`  peak  ${r.peakDb} dBFS (${r.peak})`)
  console.log(`  rms   ${r.rmsDb} dBFS (${r.rms})`)
  console.log(`  crest ${r.crestDb} dB    dc ${r.dcOffset}`)
  console.log(`  clipped ${r.clippedSamples} samples in ${r.clipRuns} runs (${r.clippedPct}%)`)
  console.log('  rms/sec:')
  r.rmsPerSecond.forEach((e, i) => console.log(`    ${String(i).padStart(3)}s ${e.toFixed(3)} ${bar(e)}`))
}

const args = process.argv.slice(2).filter(a => a !== '--json' && a !== '--stereo')
const json = process.argv.includes('--json')
const stereo = process.argv.includes('--stereo')
if (args.length === 0) { console.error('usage: node tools/wav-analyze.js <file.wav> [b.wav] [--json] [--stereo]'); process.exit(1) }

if (stereo) {
  const rs = args.map(analyzeStereo)
  if (json) { console.log(JSON.stringify(rs.length === 1 ? rs[0] : rs, null, 2)); process.exit(0) }
  rs.forEach(r => { reportStereo(r); console.log() })
} else if (args.length === 2) {
  const a = analyze(args[0]), b = analyze(args[1])
  if (json) { console.log(JSON.stringify({ a, b }, null, 2)); process.exit(0) }
  report(a); console.log(); report(b); console.log()
  const identical = a.samples === b.samples &&
    JSON.stringify(a.rmsPerSecond) === JSON.stringify(b.rmsPerSecond) &&
    a.peak === b.peak && a.rms === b.rms && a.clippedSamples === b.clippedSamples
  console.log('compare:')
  console.log(`  peak    ${a.peakDb} -> ${b.peakDb} dBFS`)
  console.log(`  rms     ${a.rmsDb} -> ${b.rmsDb} dBFS`)
  console.log(`  clipped ${a.clippedSamples} -> ${b.clippedSamples} samples`)
  console.log(`  metrics identical: ${identical}`)
  // byte-level check for golden-style comparison
  const ba = fs.readFileSync(args[0]), bb = fs.readFileSync(args[1])
  console.log(`  bytes identical: ${ba.length === bb.length && ba.equals(bb)}`)
} else {
  const r = analyze(args[0])
  if (json) console.log(JSON.stringify(r, null, 2))
  else report(r)
}
