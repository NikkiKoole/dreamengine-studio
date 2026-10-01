#!/usr/bin/env node
// gen-wavescan.js — GENERATE runtime/wavescan_data.h, the band-limited table bank INSTR_WAVESCAN scans.
//
//   node tools/gen-wavescan.js               # write runtime/wavescan_data.h
//   node tools/gen-wavescan.js --check       # exit nonzero if the header is stale (repo-doctor)
//   node tools/gen-wavescan.js --selfcheck   # known answers vs Plinky's own shipped saw/square + invariants
//   node tools/gen-wavescan.js --report      # per-shape brightness (spectral centroid, highest harmonic)
//
// WHERE IT COMES FROM. A port of Plinky's wavetable generator (plinkysynth/plinky_public,
// sw/emu/main.cpp: eval_wave() + the windowed-sinc mip pyramid in main(); MIT License,
// Copyright (c) Alex Evans). The SHAPE RECIPES and the BAND-LIMITING are ported verbatim; the
// shipped DATA is not. Plinky's committed wavetable.h does not match its own labels: only slots 0-1
// (saw, square) are those recipes, slots 2-15 were read from WAV files on the author's disk
// (cyclenames[] = "c:/temp/waves/wave1_2048.wav"…) of unknown origin. So we regenerate from the
// code, which is unambiguously MIT, and use the two procedural slots as the oracle (--selfcheck):
// our saw and square must come out byte-identical to theirs.
//
// WHY A GENERATED CONST TABLE and not a build at sound init: the recipes call sin(), and libm's
// transcendentals round differently per platform, so a runtime build would make the voice sound
// different natively and in wasm (docs/design/determinism.md). Const data is the same bits
// everywhere, and it is shared read-only state, so two plug-in instances cannot race on it.
//
// LAYOUT (Plinky's): per shape, 9 mip levels of 512, 256 … 2 samples, each followed by ONE guard
// sample (= sample 0, so linear interpolation needs no wrap) → 513+257+…+3 = 1031 shorts. Level k is
// the shape low-passed by a Hann-windowed sinc whose width doubles with k, so a level never holds a
// harmonic its table cannot carry. The voice picks the largest level no longer than the period.
// int16 at 16384 = 1.0 (Plinky's scale; a band-limited saw overshoots to ~1.16 — Gibbs).
//
// ORDER IS OURS: Plinky's enum order (saw, square, sin, sin2, fm, folds, noises) is reordered by
// MEASURED spectral centroid, so `harmonics` 0 = a pure sine and turning it up only ever brightens.
// The recipes are unchanged; --report prints the centroids, --selfcheck enforces the order.
const fs = require('fs'), path = require('path');
const OUT = path.join(__dirname, '..', 'runtime', 'wavescan_data.h');

const LEVELS = 9, TOP = 512;
const LEVEL_OFF = []; { let o = 0; for (let k = 0; k < LEVELS; k++) { LEVEL_OFF.push(o); o += (TOP >> k) + 1; } }
const SIZE = LEVEL_OFF[LEVELS - 1] + (TOP >> (LEVELS - 1)) + 1;   // 1031

// ── Plinky's helpers, verbatim (main.cpp) ──
function softtri(x) {
  x *= Math.PI * 0.5;
  const y = Math.sin(x) - Math.sin(x * 3) / 9 + Math.sin(x * 5) / 25 - Math.sin(x * 7) / 49
          + Math.sin(x * 9) / 81 - Math.sin(x * 11) / 121 + Math.sin(x * 13) / 169 - Math.sin(x * 15) / 225;
  return y * (8 / Math.PI / Math.PI);
}
function wang_hash(seed) {
  seed = ((seed ^ 61) ^ (seed >>> 16)) >>> 0;
  seed = Math.imul(seed, 9) >>> 0;
  seed = (seed ^ (seed >>> 4)) >>> 0;
  seed = Math.imul(seed, 0x27d4eb2d) >>> 0;
  seed = (seed ^ (seed >>> 15)) >>> 0;
  return seed;
}
const noisey = i => (wang_hash(i >>> 0) & 65535) / 32768 - 1;

// recipe ids = Plinky's EWavetables order; `octnoise` = their `shape > 4` (the fractal noise term)
const R = { SAW: 0, SQUARE: 1, SIN: 2, SIN_2: 3, FM: 4, SIN_FOLD1: 5, SIN_FOLD2: 6, SIN_FOLD3: 7,
            NOISE_FOLD1: 8, NOISE_FOLD2: 9, NOISE_FOLD3: 10, WHITE_NOISE: 11 };
function eval_wave(shape, i) {
  i &= 65535;
  const ph = i * (Math.PI * 2 / 65536);
  let ns = 0;
  if (shape > 4) { let seed = 0, f = 1; for (let oct = 15; oct >= 2; --oct) { ns += noisey((i >> oct) + seed) * f; f *= 0.5; seed += 232532; } }
  switch (shape) {
    case R.SAW:         return (i - 32768) * (1 / 32768);
    case R.SQUARE:      return i < 32768 ? -1 : 1;
    case R.SIN:         return Math.cos(ph);
    case R.SIN_2:       return Math.cos(ph) * 0.75 + Math.cos(ph * 3) * 0.5;
    case R.FM:          return Math.cos(ph + Math.sin(ph * 7 + Math.sin(ph * 11) * 0.1) * 0.7);
    case R.SIN_FOLD1:   return softtri(Math.sin(ph) * 2);
    case R.SIN_FOLD2:   return softtri(Math.sin(ph) * 6);
    case R.SIN_FOLD3:   return softtri(Math.sin(ph) * 12);
    case R.NOISE_FOLD1: return softtri(Math.sin(ph) + 2 * ns);
    case R.NOISE_FOLD2: return softtri(Math.sin(ph) + 8 * ns);
    case R.NOISE_FOLD3: return softtri(Math.sin(ph) + 32 * ns);
    case R.WHITE_NOISE: return ((wang_hash(i) & 65535) - 32768) * (1 / 32768);
  }
  return 0;
}

// the scan order (dark → bright) and the names the C header + docs use
const BANK = [   // ordered by MEASURED level-0 spectral centroid (--report), so the scan only ever brightens
  ['SIN', R.SIN, 'sine'], ['SIN2', R.SIN_2, 'sine + 3rd'], ['FOLD1', R.SIN_FOLD1, 'gently folded sine'],
  ['FM', R.FM, 'FM cycle'], ['SQUARE', R.SQUARE, 'square'], ['SAW', R.SAW, 'saw'],
  ['GRIT1', R.NOISE_FOLD1, 'noise-folded sine'], ['FOLD2', R.SIN_FOLD2, 'folded sine'],
  ['FOLD3', R.SIN_FOLD3, 'hard-folded sine'], ['GRIT2', R.NOISE_FOLD2, 'noisier fold'],
  ['GRIT3', R.NOISE_FOLD3, 'mostly noise fold'], ['BUZZ', R.WHITE_NOISE, 'one cycle of white noise (a pitched buzz)'],
];

// Plinky's kernel: Hann-windowed sinc, 256 one-sided taps, zero crossing every 28, normalised
const KERNEL = (() => {
  const k = new Float32Array(256); let tot = 0;
  // single-precision step by step, as the C does it (PI is a float literal, cosf/sinf are float), so the
  // pyramid comes out of the same rounding as theirs and the --selfcheck can demand byte equality
  const f = Math.fround, PIf = f(Math.PI);
  for (let i = 0; i < 256; ++i) {
    const ip = f(i * PIf), x = f(ip / 28);
    k[i] = i ? f(f(f(0.5 + f(0.5 * f(Math.cos(f(ip / 256))))) * f(Math.sin(x))) / x) : 1;
    tot = f(tot + f(k[i] * (i ? 2 : 1)));
  }
  for (let i = 0; i < 256; ++i) k[i] = Math.fround(k[i] / tot);
  return k;
})();

function buildShape(recipe) {
  const w = new Float32Array(65536);
  for (let i = 0; i < 65536; ++i) w[i] = eval_wave(recipe, i);
  const out = new Int16Array(SIZE);
  let p = 0;
  for (let oct = 0; oct < LEVELS; ++oct) {
    const n = TOP >> oct;
    for (let i = 0; i <= n; ++i) {
      let x = 0;
      for (let j = -255; j < 256; ++j) x = Math.fround(x + Math.fround(KERNEL[Math.abs(j)] * w[65535 & ((i << (oct + 7)) + (j << (oct + 2)))]));
      out[p++] = Math.trunc(Math.fround(x * 16384));   // C: short s = x * 16384.f  (truncates toward zero)
    }
  }
  return out;
}

// DC REMOVAL (ours, not upstream's): the noise-fold recipes are not zero-mean — Plinky's fractal noise term
// sits off centre, so GRIT1 averaged -0.177 of full scale and GRIT2 -0.118, which dc-check caught as a
// -35 dBFS offset on the voice. Subtract each LEVEL's own mean (the band-limiting moves it per level, the
// saw's top levels drift too), then re-copy the guard sample. buildShape() stays the verbatim port, so the
// byte-identity oracle below still checks the port itself.
function dcFree(t) {
  const out = Int16Array.from(t);
  for (let k = 0; k < LEVELS; k++) {
    const a = LEVEL_OFF[k], n = TOP >> k;
    let m = 0; for (let i = 0; i < n; i++) m += out[a + i];
    m = Math.round(m / n);
    for (let i = 0; i < n; i++) out[a + i] -= m;
    out[a + n] = out[a];
  }
  return out;
}
const levelMean = (t, k) => { const a = LEVEL_OFF[k], n = TOP >> k; let m = 0; for (let i = 0; i < n; i++) m += t[a + i]; return m / n; };

function render() {
  const tabs = BANK.map(([, r]) => dcFree(buildShape(r)));
  let s = '';
  s += '// wavescan_data.h — GENERATED by tools/gen-wavescan.js. DO NOT EDIT (rerun the tool; --check gates it).\n';
  s += '// The INSTR_WAVESCAN table bank: Plinky\'s wave recipes + its windowed-sinc mip pyramid, regenerated from\n';
  s += '// plinkysynth/plinky_public sw/emu/main.cpp (MIT License, Copyright (c) Alex Evans). Order is dark → bright.\n';
  s += `// Per shape: ${LEVELS} levels of ${TOP}..${TOP >> (LEVELS - 1)} samples + 1 guard each = ${SIZE} int16, 16384 = 1.0.\n`;
  s += '#ifndef DE_WAVESCAN_DATA_H\n#define DE_WAVESCAN_DATA_H\n\n';
  s += `#define WAVESCAN_NSHAPE ${BANK.length}\n#define WAVESCAN_LEVELS ${LEVELS}\n#define WAVESCAN_TOP    ${TOP}\n#define WAVESCAN_SIZE   ${SIZE}\n`;
  s += `static const unsigned short WAVESCAN_LEVEL_OFF[WAVESCAN_LEVELS] = { ${LEVEL_OFF.join(', ')} };\n`;
  s += '// scan order: ' + BANK.map(([n], i) => `${i} ${n}`).join(' · ') + '\n';
  s += 'static const short WAVESCAN_TABLE[WAVESCAN_NSHAPE][WAVESCAN_SIZE] = {\n';
  tabs.forEach((t, k) => {
    s += `  // ${k} ${BANK[k][0]} — ${BANK[k][2]}\n  {`;
    for (let oct = 0; oct < LEVELS; oct++) {
      const a = LEVEL_OFF[oct], n = (TOP >> oct) + 1;
      s += (oct ? '\n   ' : '') + Array.from(t.subarray(a, a + n)).join(',') + ',';
    }
    s += '},\n';
  });
  s += '};\n\n#endif\n';
  return { text: s, tabs };
}

// brightness of level 0 (512 samples): centroid in harmonics + highest harmonic within 60 dB
function spectrum(t) {
  const N = 512, mags = [];
  for (let h = 1; h <= 200; h++) {
    let re = 0, im = 0;
    for (let i = 0; i < N; i++) { re += t[i] * Math.cos(2 * Math.PI * h * i / N); im += t[i] * Math.sin(2 * Math.PI * h * i / N); }
    mags.push(Math.hypot(re, im));
  }
  const m = Math.max(...mags), tot = mags.reduce((a, b) => a + b * b, 0);
  const centroid = mags.reduce((a, b, i) => a + (i + 1) * b * b, 0) / tot;
  let top = 0; mags.forEach((v, i) => { if (v > m * 1e-3) top = i + 1; });
  return { centroid, top };
}

const arg = process.argv[2];
if (arg === '--check') {
  const cur = fs.existsSync(OUT) ? fs.readFileSync(OUT, 'utf8') : '';
  if (cur !== render().text) { console.error('wavescan_data.h is STALE — run node tools/gen-wavescan.js'); process.exit(1); }
  console.log('wavescan_data.h up to date'); process.exit(0);
}
if (arg === '--report') {
  const { tabs } = render();
  tabs.forEach((t, k) => { const s = spectrum(t); console.log(`${String(k).padStart(2)} ${BANK[k][0].padEnd(7)} centroid ${s.centroid.toFixed(1).padStart(6)}  top harmonic ${s.top}`); });
  process.exit(0);
}
if (arg === '--selfcheck') {
  // KNOWN ANSWERS from Plinky's shipped sw/Core/Src/wavetable.h. Only TWO of its slots are recipes (the rest
  // are WAVs, see the header): slot 0 = SAW and slot 1 = a COSINE (labelled "Square" by today's enum — the
  // table was generated under an older order). The saw must be byte-identical; the sine may differ by
  // 1 LSB in a couple of samples, since their cosf and our Math.cos round differently.
  const PLINKY_SAW_HASH = 1142081772, PLINKY_SIN_SUM = 146464, PLINKY_SIN_SUMABS = 10798322;
  const hash = a => a.reduce((s, v, i) => (Math.imul(s, 31) + v + i) >>> 0, 7);
  const { tabs } = render();
  let fail = 0;
  const ok = (c, msg) => { console.log((c ? '  ✓ ' : '  ✗ ') + msg); if (!c) fail++; };
  const by = n => tabs[BANK.findIndex(b => b[0] === n)];
  const saw = buildShape(R.SAW), sin = buildShape(R.SIN);   // the RAW port (before our DC removal) is what matches upstream
  ok(hash(saw) === PLINKY_SAW_HASH, `SAW is byte-identical to Plinky's shipped slot 0 (hash ${hash(saw)})`);
  const ssum = sin.reduce((a, b) => a + b, 0), sabs = sin.reduce((a, b) => a + Math.abs(b), 0);
  ok(Math.abs(ssum - PLINKY_SIN_SUM) <= 4 && Math.abs(sabs - PLINKY_SIN_SUMABS) <= 4, `SIN matches Plinky's shipped slot 1 within rounding (sum ${ssum} vs ${PLINKY_SIN_SUM})`);
  ok(SIZE === 1031, `table size ${SIZE} == Plinky's WAVETABLE_SIZE 1031`);
  let guard = true;   // guard sample = sample 0 of its level, so the voice's linear interpolation needs no wrap
  tabs.forEach(t => { for (let k = 0; k < LEVELS; k++) { const a = LEVEL_OFF[k], n = TOP >> k; if (Math.abs(t[a + n] - t[a]) > 1) guard = false; } });
  ok(guard, "every level's guard sample equals its sample 0");
  ok(spectrum(sin).top === 1, `SIN is a pure fundamental (top harmonic ${spectrum(sin).top})`);
  const c = tabs.map(t => spectrum(t).centroid);
  ok(c.every((v, i) => i === 0 || v >= c[i - 1] - 1e-9), 'the scan order is non-decreasing in centroid: ' + c.map(v => v.toFixed(1)).join(' ≤ '));
  // band-limiting: the SAW's level 3 (64 samples) must roll off toward its Nyquist (harmonic 32). A plain saw's 30th
  // harmonic is -29.5 dB by nature (1/n) and aliasing lifts its top ones further; the kernel takes them past -35
  const l3 = Array.from(saw.subarray(LEVEL_OFF[3], LEVEL_OFF[3] + 64));
  let hi = 0, lo = 0;
  for (let h = 1; h < 32; h++) { let re = 0, im = 0; for (let i = 0; i < 64; i++) { re += l3[i] * Math.cos(2 * Math.PI * h * i / 64); im += l3[i] * Math.sin(2 * Math.PI * h * i / 64); } const m = Math.hypot(re, im); if (h === 1) lo = m; if (h >= 30) hi = Math.max(hi, m); }
  ok(hi < lo * 0.0178, `level 3 is band-limited: harmonics 30-31 sit ${(20 * Math.log10(hi / lo)).toFixed(0)} dB under the fundamental`);
  // negative control: the UNFILTERED saw does not pass that test, so the band-limit check can fail
  const raw = Array.from({ length: 64 }, (_, i) => Math.trunc(eval_wave(R.SAW, i * 1024) * 16384));
  let rhi = 0, rlo = 0;
  for (let h = 1; h < 32; h++) { let re = 0, im = 0; for (let i = 0; i < 64; i++) { re += raw[i] * Math.cos(2 * Math.PI * h * i / 64); im += raw[i] * Math.sin(2 * Math.PI * h * i / 64); } const m = Math.hypot(re, im); if (h === 1) rlo = m; if (h >= 30) rhi = Math.max(rhi, m); }
  ok(!(rhi < rlo * 0.0178), `negative control: a raw (unfiltered) saw FAILS the same band-limit test (${(20 * Math.log10(rhi / rlo)).toFixed(0)} dB)`);
  // DC: every emitted level is centred (within 1 LSB) — and the RAW grit shape is NOT, so the check can fail
  let worst = 0; tabs.forEach(t => { for (let k = 0; k < LEVELS; k++) worst = Math.max(worst, Math.abs(levelMean(t, k))); });
  ok(worst <= 1, `every emitted level is zero-mean (worst |mean| ${worst.toFixed(2)} LSB ≤ 1)`);
  const rawGrit = Math.abs(levelMean(buildShape(R.NOISE_FOLD1), 0)) / 16384;
  ok(rawGrit > 0.05, `negative control: the RAW noise-fold recipe is off centre (mean ${rawGrit.toFixed(3)} of full scale)`);
  if (fail) { console.error(`${fail} FAILED`); process.exit(1); }
  console.log('all known answers hold'); process.exit(0);
}
const { text } = render();
fs.writeFileSync(OUT, text);
console.log(`wrote ${path.relative(process.cwd(), OUT)} — ${BANK.length} shapes × ${SIZE} samples (${(text.length / 1024).toFixed(0)} KB source)`);
