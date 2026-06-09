#!/usr/bin/env node
// wav-analyze.js — tier-1 signal metrics for engine WAVs (audio-notes §16).
// No dependencies; reads the 16-bit PCM mono WAVs the runtime writes
// (.bake/wav_request live capture, or the --wav deterministic render).
//
//   node tools/wav-analyze.js <file.wav>            human-readable report
//   node tools/wav-analyze.js <file.wav> --json     machine-readable
//   node tools/wav-analyze.js <a.wav> <b.wav>       compare two renders
//
// Metrics: duration, peak / RMS / crest (dBFS), clipped-sample count and
// runs (|s| at full scale), DC offset, per-second RMS envelope.

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

const args = process.argv.slice(2).filter(a => a !== '--json')
const json = process.argv.includes('--json')
if (args.length === 0) { console.error('usage: node tools/wav-analyze.js <file.wav> [b.wav] [--json]'); process.exit(1) }

if (args.length === 2) {
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
