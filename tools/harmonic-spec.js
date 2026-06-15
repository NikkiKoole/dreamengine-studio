#!/usr/bin/env node
// harmonic-spec.js — quick harmonic-series + high-frequency-rolloff readout for an engine WAV.
// Throwaway analysis tool for the brass-realism work (docs/design/brass-realism-handoff.md).
// Usage: node tools/harmonic-spec.js <wav> <fundamentalHz> [--n 24]
// Prints each harmonic's level in dB relative to the fundamental, plus the band energy above 4kHz
// (the "shock-wave blat" cue) and the highest harmonic still within 20 dB of the fundamental.
const fs = require('fs');

function readWav(path) {
  const b = fs.readFileSync(path);
  // minimal PCM16 / float32 WAV reader
  let p = 12, fmt = null, dataOff = 0, dataLen = 0;
  while (p + 8 <= b.length) {
    const id = b.toString('ascii', p, p + 4);
    const sz = b.readUInt32LE(p + 4);
    if (id === 'fmt ') fmt = { audioFormat: b.readUInt16LE(p + 8), channels: b.readUInt16LE(p + 10),
                               rate: b.readUInt32LE(p + 12), bits: b.readUInt16LE(p + 22) };
    else if (id === 'data') { dataOff = p + 8; dataLen = sz; }
    p += 8 + sz + (sz & 1);
  }
  if (!fmt || !dataOff) throw new Error('bad wav');
  const n = Math.floor(dataLen / (fmt.bits / 8) / fmt.channels);
  const out = new Float64Array(n);
  for (let i = 0; i < n; i++) {
    const off = dataOff + i * fmt.channels * (fmt.bits / 8);   // channel 0 only
    if (fmt.audioFormat === 3 && fmt.bits === 32) out[i] = b.readFloatLE(off);
    else if (fmt.bits === 16) out[i] = b.readInt16LE(off) / 32768;
    else if (fmt.bits === 32) out[i] = b.readInt32LE(off) / 2147483648;
  }
  return { samples: out, rate: fmt.rate };
}

// Goertzel magnitude at frequency f over a Hann-windowed block.
function goertzel(x, start, len, f, rate) {
  const k = f / rate;
  const w = 2 * Math.PI * k;
  const cw = Math.cos(w), sw = Math.sin(w);
  const coeff = 2 * cw;
  let s0 = 0, s1 = 0, s2 = 0;
  for (let i = 0; i < len; i++) {
    const win = 0.5 - 0.5 * Math.cos((2 * Math.PI * i) / (len - 1));
    s0 = win * x[start + i] + coeff * s1 - s2;
    s2 = s1; s1 = s0;
  }
  const re = s1 - s2 * cw, im = s2 * sw;
  return Math.sqrt(re * re + im * im) / (len / 2);
}

const [wav, f0s, ...rest] = process.argv.slice(2);
if (!wav || !f0s) { console.error('usage: harmonic-spec.js <wav> <fundamentalHz> [--n 24]'); process.exit(1); }
const f0 = parseFloat(f0s);
const N = rest.includes('--n') ? parseInt(rest[rest.indexOf('--n') + 1]) : 24;

const { samples, rate } = readWav(wav);
// analyze a sustained middle window (skip attack, avoid release)
const start = Math.floor(samples.length * 0.35);
const len = Math.min(16384, samples.length - start - 1);

const mags = [];
for (let h = 1; h <= N; h++) {
  const f = f0 * h;
  if (f > rate / 2 - 200) { mags.push(null); continue; }
  mags.push(goertzel(samples, start, len, f, rate));
}
const f1 = mags[0] || 1e-9;
console.log(`fundamental ${f0.toFixed(1)} Hz · rate ${rate} · window ${len} samp`);
console.log('harmonic   freq      dB(rel f1)');
let lastWithin20 = 1;
for (let h = 1; h <= N; h++) {
  const m = mags[h - 1];
  if (m == null) { console.log(`  h${String(h).padEnd(3)}    >Nyquist`); continue; }
  const db = 20 * Math.log10((m + 1e-12) / f1);
  if (db > -20) lastWithin20 = h;
  const bar = '#'.repeat(Math.max(0, Math.round((db + 60) / 3)));
  console.log(`  h${String(h).padEnd(3)} ${String((f0 * h).toFixed(0)).padStart(6)}Hz  ${db.toFixed(1).padStart(7)}  ${bar}`);
}
// high-band energy (>4kHz) relative to total — the brass "brightness" metric
let totE = 0, hiE = 0;
for (let h = 1; h <= N; h++) {
  const m = mags[h - 1]; if (m == null) continue;
  totE += m * m;
  if (f0 * h > 4000) hiE += m * m;
}
console.log(`\nhighest harmonic within 20dB of f1:  h${lastWithin20}  (~${(f0 * lastWithin20 / 1000).toFixed(1)} kHz)`);
console.log(`energy >4kHz / total:  ${(100 * hiE / (totE + 1e-12)).toFixed(1)}%   (brass wants this HIGH at forte)`);
