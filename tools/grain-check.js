#!/usr/bin/env node
// grain-check.js — the INSTR_GRAIN gate: render the `grainprobe` cart and assert what the voice promises.
//
//   node tools/grain-check.js            render + report, exit 1 on any failure
//   node tools/grain-check.js --quiet    PASS/FAIL line only
//   node tools/grain-check.js --keep     keep build/.grain/probe.wav
//
// The probe (tools/carts/grainprobe.c) loads a 0.5 s 200 → 800 Hz sine SWEEP as the sample, because a
// sweep's pitch IS its position: a steady tone cannot tell a moving playhead from a stopped one. Five
// tests, 105 frames apart from frame 30: T0 = INSTR_SAMPLE looping (the reference), T1 = INSTR_GRAIN at
// 1× on the root, T2 = frozen mid-sweep, T3 = an octave up at 1×, T4 = scatter + detune.
//
// What each assertion catches (docs/design/plinky-harvest.md §4.1):
//   T1 ≈ T0 (correlation ≥ 0.99)     the crossfade chain is transparent at 1× — a broken fade, a wrong
//                                    playhead rate or a mis-centred grain all decorrelate it
//   T2 holds still                   SPEED 0 really freezes — its pitch spread must be a fraction of T1's
//                                    (T1's spread is the CONTROL: a sweep that stopped sweeping in T1 too
//                                    would make "T2 is still" meaningless, so T1 must move)
//   T3 / T1 = 2.00 ± 0.03            pitch moved, timing did not: without grain centring this read 2.3
//   T4 finite + audible              scatter/detune cannot blow up or go silent
const fs = require('fs'), path = require('path'), { spawnSync } = require('child_process');
const ROOT = path.resolve(__dirname, '..');
const quiet = process.argv.includes('--quiet'), keep = process.argv.includes('--keep');
const FRAMES = 560, dir = path.join(ROOT, 'build', '.grain'), wav = path.join(dir, 'probe.wav');
fs.mkdirSync(dir, { recursive: true });
const r = spawnSync('node', ['tools/play.js', 'grainprobe', 'script', '/dev/null', '--headless', '--frames', String(FRAMES), '--wav', wav],
  { cwd: ROOT, encoding: 'utf8' });
if (r.status !== 0 || !fs.existsSync(wav)) { process.stderr.write((r.stdout || '') + (r.stderr || '')); console.error('grain-check: render failed'); process.exit(1); }

const b = fs.readFileSync(wav); let o = 12, ch = 2, sr = 44100, data;
while (o < b.length) { const id = b.toString('ascii', o, o + 4), sz = b.readUInt32LE(o + 4);
  if (id === 'fmt ') { ch = b.readUInt16LE(o + 10); sr = b.readUInt32LE(o + 12); } if (id === 'data') data = b.subarray(o + 8, o + 8 + sz); o += 8 + sz + (sz & 1); }
const n = data.length / 2 / ch, x = new Float64Array(n);
for (let i = 0; i < n; i++) { let s = 0; for (let c = 0; c < ch; c++) s += data.readInt16LE((i * ch + c) * 2); x[i] = s / ch / 32768; }
const spf = n / FRAMES, start = t => Math.round((30 + t * 105) * spf);
function track(t) {        // zero-crossing pitch in 10 ms windows across 1 s of the held note (skipping the attack)
  const out = [], s0 = start(t) + Math.round(0.03 * sr), w = Math.round(sr / 100);
  for (let k = 0; k < 100; k++) { let zc = 0; for (let i = s0 + k * w + 1; i < s0 + (k + 1) * w; i++) if ((x[i - 1] < 0) !== (x[i] < 0)) zc++; out.push(zc * 50); }
  return out;
}
const T = [0, 1, 2, 3, 4].map(track);
const sd = a => { const m = a.reduce((p, q) => p + q) / a.length; return Math.sqrt(a.reduce((p, q) => p + (q - m) ** 2, 0) / a.length); };
let num = 0, da = 0, db = 0; const a0 = start(0) + 1323, b0 = start(1) + 1323;
for (let i = 0; i < sr; i++) { num += x[a0 + i] * x[b0 + i]; da += x[a0 + i] ** 2; db += x[b0 + i] ** 2; }
const corr = num / Math.sqrt(da * db), rms = Math.sqrt(db / da);
const ratio = T[3].map((v, i) => v / T[1][i]).sort((p, q) => p - q)[50];
let fin = true, peak = 0; for (let i = start(4); i < start(4) + Math.round(1.5 * sr); i++) { if (!isFinite(x[i])) fin = false; peak = Math.max(peak, Math.abs(x[i])); }

const checks = [
  [corr >= 0.99 && Math.abs(rms - 1) < 0.02, `T1 grain 1× matches T0 sample loop: correlation ${corr.toFixed(4)}, rms ratio ${rms.toFixed(3)}`],
  [sd(T[1]) > 100, `control: T1 really sweeps (pitch sd ${sd(T[1]).toFixed(0)} Hz > 100)`],
  [sd(T[2]) < 0.35 * sd(T[1]), `T2 frozen holds still: pitch sd ${sd(T[2]).toFixed(0)} Hz < 0.35 × T1's`],
  [Math.abs(ratio - 2) < 0.03, `T3 an octave up: pitch ratio to T1 ${ratio.toFixed(3)} (want 2.00 ± 0.03)`],
  [fin && peak > 0.01, `T4 scatter + detune: finite, peak ${peak.toFixed(3)}`],
];
const fails = checks.filter(c => !c[0]).length;
if (!quiet) checks.forEach(([ok, m]) => console.log((ok ? '  ✓ ' : '  ✗ ') + m));
console.log(fails ? `grain-check: FAIL (${fails}/${checks.length})` : `grain-check: PASS (${checks.length}/${checks.length})`);
if (!keep) fs.rmSync(dir, { recursive: true, force: true });
process.exit(fails ? 1 : 0);
