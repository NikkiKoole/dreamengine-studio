#!/usr/bin/env node
// formant-check.js — the VOICE oracle for auto-tune / pitch-shift work (docs/design/transparent-autotune.md).
//
// Reads a WAV region and reports two things that a pitch shift must move INDEPENDENTLY:
//   • f0  — the PITCH, via autocorrelation, sampled at 8 sub-windows → a mean + a WOBBLE number
//           (max-min); a corrected take should sit on the target note with a near-zero wobble.
//   • F1/F2/F3 — the FORMANTS (the vowel/identity), via a Welch-averaged, heavily-smoothed spectral
//           envelope (the harmonic comb blurred out), reported as the strongest peak in each band.
//
// The formant-preservation test: shift/correct the pitch and the formant peaks should HOLD (a naive
// resample scales them with the pitch → the chipmunk). This is the gate born from the mictune PSOLA
// spike; sample_autotune() is measured against it. Analysis tool (prints numbers) — pair it with an
// A/B render, e.g. play.js --wav a raw region vs a tuned region, like wav-envelope / harmonic-spec.
//
// usage: node tools/formant-check.js <file.wav> <from_seconds> <to_seconds>
//   node tools/formant-check.js take.wav 2.5 4.0   → f0 116.5Hz (wobble 0.3Hz) | F1 711 F2 1152 F3 2466
//
// No deps. Bands assume a low male-ish 'ah' (~110 Hz) test source; widen them for other voices.

const fs = require('fs');
const [file, fromS, toS] = [process.argv[2], parseFloat(process.argv[3]), parseFloat(process.argv[4])];
if (!file || Number.isNaN(fromS) || Number.isNaN(toS)) {
  console.error('usage: node tools/formant-check.js <file.wav> <from_seconds> <to_seconds>');
  process.exit(2);
}

function readWav(path) {
  const b = fs.readFileSync(path);
  let p = 12, ch = 1, bits = 16, rate = 44100, dataOff = 0, dataLen = 0;
  while (p < b.length) {
    const id = b.toString('ascii', p, p + 4), sz = b.readUInt32LE(p + 4);
    if (id === 'fmt ') { ch = b.readUInt16LE(p + 10); rate = b.readUInt32LE(p + 12); bits = b.readUInt16LE(p + 22); }
    else if (id === 'data') { dataOff = p + 8; dataLen = sz; break; }
    p += 8 + sz + (sz & 1);
  }
  const bytes = bits / 8, frames = Math.floor(dataLen / (bytes * ch));
  const out = new Float32Array(frames);
  for (let i = 0; i < frames; i++) {            // mono-ize (average channels)
    let s = 0;
    for (let c = 0; c < ch; c++) {
      const off = dataOff + (i * ch + c) * bytes;
      s += bits === 16 ? b.readInt16LE(off) / 32768 : b.readFloatLE(off);
    }
    out[i] = s / ch;
  }
  return { data: out, rate };
}

// iterative radix-2 FFT (in-place)
function fft(re, im) {
  const n = re.length;
  for (let i = 1, j = 0; i < n; i++) {
    let bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) { [re[i], re[j]] = [re[j], re[i]]; [im[i], im[j]] = [im[j], im[i]]; }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const ang = -2 * Math.PI / len, wr = Math.cos(ang), wi = Math.sin(ang);
    for (let i = 0; i < n; i += len) {
      let cr = 1, ci = 0;
      for (let k = 0; k < len / 2; k++) {
        const ur = re[i + k], ui = im[i + k];
        const vr = re[i + k + len / 2] * cr - im[i + k + len / 2] * ci;
        const vi = re[i + k + len / 2] * ci + im[i + k + len / 2] * cr;
        re[i + k] = ur + vr; im[i + k] = ui + vi;
        re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi;
        [cr, ci] = [cr * wr - ci * wi, cr * wi + ci * wr];
      }
    }
  }
}

const { data, rate } = readWav(file);
const N = 4096, hop = 2048;
const from = Math.floor(fromS * rate), to = Math.floor(toS * rate);
if (to - from < N) { console.error('region too short (need ≥ ~0.1s)'); process.exit(2); }

// Welch-averaged magnitude spectrum
const mag = new Float64Array(N / 2);
let wins = 0;
for (let start = from; start + N <= to; start += hop) {
  const re = new Float64Array(N), im = new Float64Array(N);
  for (let i = 0; i < N; i++) re[i] = data[start + i] * (0.5 - 0.5 * Math.cos(2 * Math.PI * i / (N - 1))); // Hann
  fft(re, im);
  for (let k = 0; k < N / 2; k++) mag[k] += Math.hypot(re[k], im[k]);
  wins++;
}
for (let k = 0; k < N / 2; k++) mag[k] /= wins;

// f0 via autocorrelation, sampled across the region → mean + wobble (de-wobble proof)
function f0at(base, W) {
  const lagMin = Math.floor(rate / 500), lagMax = Math.floor(rate / 80);
  let best = 0, bestLag = lagMin;
  for (let lag = lagMin; lag <= lagMax; lag++) {
    let s = 0, e0 = 0, e1 = 0;
    for (let i = 0; i < W; i++) { const a = data[base + i], b2 = data[base + i + lag]; s += a * b2; e0 += a * a; e1 += b2 * b2; }
    const nc = s / (Math.sqrt(e0 * e1) + 1e-9);
    if (nc > best) { best = nc; bestLag = lag; }
  }
  return rate / bestLag;
}
let f0 = 0, f0lo = 1e9, f0hi = 0;
{
  const W = 4096, span = to - from - W, K = 8;
  let sum = 0;
  for (let s = 0; s < K; s++) { const v = f0at(from + Math.floor(span * s / (K - 1)), W); sum += v; if (v < f0lo) f0lo = v; if (v > f0hi) f0hi = v; }
  f0 = sum / K;
}

// spectral ENVELOPE: heavy moving-average blurs the harmonic comb → the formant peaks show through
const binHz = rate / N, sm = 13;
const env = new Float64Array(N / 2);
for (let k = 0; k < N / 2; k++) {
  let s = 0, c = 0;
  for (let d = -sm; d <= sm; d++) { const kk = k + d; if (kk >= 0 && kk < N / 2) { s += mag[kk]; c++; } }
  env[k] = 20 * Math.log10(s / c + 1e-9);
}
// strongest envelope peak in each formant band (F1/F2/F3)
const bands = [[500, 950], [950, 1550], [1700, 3800]];
const fmt = bands.map(([lo, hi]) => {
  let best = -1e9, bhz = 0;
  for (let k = Math.floor(lo / binHz); k <= Math.ceil(hi / binHz); k++) if (env[k] > best) { best = env[k]; bhz = Math.round(k * binHz); }
  return bhz;
});

const wob = Math.round((f0hi - f0lo) * 10) / 10;
console.log(`f0 ${f0.toFixed(1)}Hz (range ${f0lo.toFixed(1)}-${f0hi.toFixed(1)}, wobble ${wob}Hz) | formants F1 ${fmt[0]}Hz F2 ${fmt[1]}Hz F3 ${fmt[2]}Hz`);
