// wav-envelope.js — print a fine-grained AMPLITUDE + BRIGHTNESS envelope of a mono 16-bit WAV.
//
// Companion to tools/wav-analyze.js (which gives peak/RMS/crest/clipping + a coarse per-SECOND
// RMS). This one shows the shape over time at 100ms resolution, with THREE curves:
//   • amplitude  — peak per window (the note's loudness contour)
//   • brightness — the HF/total energy ratio per window (a cheap, LEVEL-INDEPENDENT brightness
//                  proxy: how much of the window's energy is high-frequency). Plotting both is
//                  what cracked the Rhodes "ringy" bug and the funky-clav wah (2026-06-08): a
//                  Rhodes/clav should show brightness DROPPING FASTER than amplitude.
//   • centroid   — the SPECTRAL CENTROID in Hz (the "center of mass" of the magnitude spectrum
//                  via a per-window FFT). The standard, calibrated brightness number — more
//                  interpretable than the HF/total proxy for A/B-ing a crush / filter / EQ /
//                  chorus, or an engine port vs navkit. Higher Hz = brighter.
//
//   node tools/wav-envelope.js <file.wav> [windowMs=100] [--from S] [--to S]
//
// --from/--to (seconds) restrict analysis to a REGION — essential when a file has an unrelated
// prefix (e.g. a recording phase before the part you care about): a whole-file centroid there is
// meaningless, a region centroid is the A/B number. The brightness + centroid columns are
// ABSOLUTE (comparable ACROSS files); the amplitude column is normalized to THIS file's peak
// (contour). The summary line prints peak dBFS + the mean HF/total ratio + the region centroid Hz.
//
// CAVEAT: both brightness proxies conflate a resonant filter PEAK with perceptual brightness — a
// loud resonant peak reads as "bright" even when it isn't. Good for decay/transient shape and
// rough A/Bs; trust your ears for resonant-filter timbre. (Pairs with tools/navkit-render.c.)

const fs = require('fs');

// tiny in-place iterative radix-2 FFT (Cooley–Tukey), n must be a power of two
function fft(re, im) {
  const n = re.length;
  for (let i = 1, j = 0; i < n; i++) {
    let bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) { const tr = re[i]; re[i] = re[j]; re[j] = tr; const ti = im[i]; im[i] = im[j]; im[j] = ti; }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const ang = -2 * Math.PI / len, wr = Math.cos(ang), wi = Math.sin(ang);
    for (let i = 0; i < n; i += len) {
      let cwr = 1, cwi = 0;
      for (let k = 0; k < len / 2; k++) {
        const a = i + k, b = a + len / 2;
        const vr = re[b] * cwr - im[b] * cwi, vi = re[b] * cwi + im[b] * cwr;
        re[b] = re[a] - vr; im[b] = im[a] - vi;
        re[a] += vr;        im[a] += vi;
        const ncwr = cwr * wr - cwi * wi; cwi = cwr * wi + cwi * wr; cwr = ncwr;
      }
    }
  }
}

// spectral centroid (Hz) of a window: Hann-windowed magnitude spectrum, returns {num, den}
// (accumulate across windows for a region centroid = ΣnumΣ/Σden). Skips DC (k=0).
function centroidParts(s, i0, i1, sr) {
  let n = 1; while (n < i1 - i0) n <<= 1;
  if (n < 2) return { num: 0, den: 0 };
  const re = new Float64Array(n), im = new Float64Array(n), W = i1 - i0;
  for (let k = 0; k < W; k++) {
    const w = 0.5 - 0.5 * Math.cos(2 * Math.PI * k / (W - 1 || 1));   // Hann
    re[k] = s[i0 + k] * w;
  }
  fft(re, im);
  let num = 0, den = 0;
  for (let k = 1; k < n / 2; k++) {
    const mag = Math.hypot(re[k], im[k]), f = k * sr / n;
    num += f * mag; den += mag;
  }
  return { num, den };
}

function readWav(f) {
  const b = fs.readFileSync(f);
  let off = 12, data = null, sr = 44100, ch = 1;
  while (off + 8 <= b.length) {
    const id = b.toString('ascii', off, off + 4), len = b.readUInt32LE(off + 4);
    if (id === 'fmt ') { sr = b.readUInt32LE(off + 12); ch = b.readUInt16LE(off + 10); }
    if (id === 'data') data = { off: off + 8, len };
    off += 8 + len + (len & 1);
  }
  if (!data) throw new Error(`${f}: no data chunk`);
  // stereo (stereo.md): downmix L/R to mono — equals the old mono while voices are centered.
  const n = (data.len / 2 / ch) | 0, s = new Float32Array(n);
  for (let i = 0; i < n; i++)
    s[i] = ch === 1 ? b.readInt16LE(data.off + i * 2) / 32768
                    : (b.readInt16LE(data.off + i * 4) + b.readInt16LE(data.off + i * 4 + 2)) / 65536;
  return { sr, s };
}

const argv = process.argv.slice(2);
const flag = (name) => { const i = argv.indexOf(name); return i >= 0 ? parseFloat(argv[i + 1]) : null; };
const fromS = flag('--from'), toS = flag('--to');
const pos = argv.filter((a, i) => a[0] !== '-' && !(i > 0 && argv[i - 1][0] === '-'));  // non-flag args
const file = pos[0];
if (!file) { console.error('usage: node tools/wav-envelope.js <file.wav> [windowMs] [--from S] [--to S]'); process.exit(1); }
const winMs = pos[1] ? parseFloat(pos[1]) : 100;
const { sr, s } = readWav(file);
const W = Math.max(1, (sr * winMs / 1000) | 0);

// region [lo,hi) in samples (default whole file) — --from/--to are seconds
const lo = Math.max(0, Math.min(s.length, fromS != null ? Math.round(fromS * sr) : 0));
const hi = Math.max(lo, Math.min(s.length, toS != null ? Math.round(toS * sr) : s.length));

let pmax = 0, gTot = 0, gHf = 0, cNum = 0, cDen = 0;   // global peak + energies + centroid parts
const pk = [], br = [], ct = [];        // per-window peak, HF/total ratio, spectral centroid (Hz)
for (let i = lo; i < hi; i += W) {
  const j1 = Math.min(i + W, hi);
  let p = 0, tot = 0, hf = 0;
  for (let j = i; j < j1; j++) {
    const v = s[j]; p = Math.max(p, Math.abs(v));
    tot += v * v;
    if (j > i) { const d = v - s[j - 1]; hf += d * d; }
  }
  const c = centroidParts(s, i, j1, sr);
  pk.push(p); br.push(tot > 1e-12 ? hf / tot : 0); ct.push(c.den > 1e-9 ? c.num / c.den : 0);
  pmax = Math.max(pmax, p); gTot += tot; gHf += hf; cNum += c.num; cDen += c.den;
}
const db = (x) => x > 0 ? (20 * Math.log10(x)).toFixed(1) : '-inf';
const meanBr = gTot > 1e-12 ? gHf / gTot : 0;
const meanCentroid = cDen > 1e-9 ? cNum / cDen : 0;         // region spectral centroid (Hz) — the A/B number
const brmax = br.reduce((m, x) => Math.max(m, x), 0);   // for the within-file shape bar

const region = (fromS != null || toS != null) ? `  region ${(lo / sr).toFixed(2)}–${(hi / sr).toFixed(2)}s` : '';
console.log(`${file}  (${winMs}ms windows${region})`);
console.log(`  peak ${db(pmax)} dBFS · brightness(HF/total) mean ${meanBr.toFixed(3)} · centroid ${meanCentroid.toFixed(0)} Hz  ← absolute, compare across files`);
console.log('   t    AMPLITUDE (norm shape)       BRIGHTNESS (abs # · norm bar)   CENTROID');
for (let i = 0; i < pk.length; i++) {
  const a = pmax > 0 ? pk[i] / pmax : 0;          // amplitude: per-file normalized (contour)
  const bb = brmax > 0 ? br[i] / brmax : 0;        // brightness BAR: per-file normalized (contour)
  const t = (lo / sr + i * winMs / 1000).toFixed(2) + 's';
  console.log(t.padStart(6) + ' ' + a.toFixed(2) + ' ' + '#'.repeat(Math.round(a * 18)).padEnd(19) +
              ' ' + br[i].toFixed(3) + ' ' + '#'.repeat(Math.round(bb * 18)).padEnd(19) +
              ' ' + Math.round(ct[i]) + 'Hz');   // numbers absolute, bar contour
}
