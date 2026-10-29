#!/usr/bin/env node
// scrape-check.js — the INSTR_SCRAPE gate: does the guiro scrape like the guiro, the quijada rattle like
// the quijada? Renders the `scraper` cart with DE_DEFINES=PROBE (one guiro hit, one quijada hit, at fixed
// frames) and measures each against the SAMPLE it replaces (GitHub #45).
//
//   node tools/scrape-check.js              render + report, exit 1 on any failure
//   node tools/scrape-check.js --quiet      PASS/FAIL line only
//   node tools/scrape-check.js --ref <wav>  measure ANY recording the same way (re-derive a target)
//   node tools/scrape-check.js --selfcheck  known answers for the MEASUREMENT, on synthetic signals
//
// WHAT IT MEASURES, per hit (and why each one, since each one caught a wrong first draft of the engine):
//   audible     first to last sample of the 1 ms envelope above 1% of its peak
//   rate        teeth per second = 1000 / the MEDIAN inter-tooth interval (a median, because a rattle's
//               first teeth are chaotic on purpose and must not set the rate)
//   band        the 10 / 50 / 90 % cumulative-energy frequencies: the BODY. A noise-excited body leaked its
//               90% point to 8.2 kHz on the guiro (target 6.9), which an "average frequency" would hide
//   levels      each tooth's envelope peak / the loudest. A scrape's stay up (guiro1: 0.37-1.0); the first
//               engine's scattered to 0.12, because a noise burst through a resonator peaks at random
//   ring        envelope fall to -20 dB after each tooth (the tooth's own length)
//   floor       envelope minimum between teeth: guiro1 -55 dB, quijada -60. Digital silence (-140 dB, the
//               engine before it grew a friction trace; -180 = an exact 0, a trace set below the 16-bit
//               render's last bit) reads as a sequencer, not a scrape
//
// TARGETS were measured 2026-10-29 by THIS tool's --ref from the two samples Puppetmaker 2 shipped, which
// live in Playtime's repo and are NOT copied here (vector-sketch: experiments/playtime/assets/sounds/
// cr78/guiro1.ogg at 48 kHz, mp7-quijada.ogg; decode with `ffmpeg -i x.ogg -ac 1 -ar 44100 x.wav`).
// Tolerances are what "sounds like that instrument" can honestly claim: ±15% on length, ±8% on rate,
// ±10% on each band point. ⚠ They are NOT a statement that the engine matches the samples in every
// detail: the quijada's chaotic first teeth are only checked for EXISTING (a short interval among the
// first four), since their exact pattern is chance.

const fs = require('fs'), path = require('path'), { spawnSync } = require('child_process');
const ROOT = path.resolve(__dirname, '..'), R = 44100;
const argv = process.argv.slice(2), quiet = argv.includes('--quiet');

const TARGET = {
  // every number below is THIS tool's --ref output on the sample. ⚠ Not hand-copied from another script:
  // the first draft took ring/floor from a scratch analyser that found teeth differently (6.4 ms / -46 dB
  // there, 3.0 ms / -55 dB here, same file), so the engine was being held to a number this measure never makes
  guiro:   { audible: 274, rate: 74.7, band: [5469, 5986, 6869], ring: 3.0, floor: -55.4, minLevel: 0.37 },
  quijada: { audible: 498, rate: 22.5, band: [1981, 2627, 3208], ring: 1.7, floor: -60.1, lastLevel: 0.11 },
};
// where PROBE plays them (tools/carts/scraper.c PR_GUIRO / PR_QUIJ, in frames at 60 fps), 10 ms early
// because an audio block does not start exactly on a frame (measured: the first tooth lands ~2 ms early)
const WIN = { guiro: [20 / 60 - 0.01, 1.2], quijada: [80 / 60 - 0.01, 3.3] };

function readWav (p) {
  const b = fs.readFileSync(p); let q = 12, ch = 1, bits = 16, off = 0, len = 0;
  while (q < b.length) {
    const id = b.toString('ascii', q, q + 4), sz = b.readUInt32LE(q + 4);
    if (id === 'fmt ') { ch = b.readUInt16LE(q + 10); bits = b.readUInt16LE(q + 22) } else if (id === 'data') { off = q + 8; len = sz; break }
    q += 8 + sz + (sz & 1);
  }
  const by = bits / 8, N = Math.floor(len / (by * ch)), x = new Float32Array(N);
  for (let i = 0; i < N; i++) { let s = 0; for (let c = 0; c < ch; c++) { const o = off + (i * ch + c) * by; s += bits === 16 ? b.readInt16LE(o) / 32768 : b.readFloatLE(o) } x[i] = s / ch }
  return x;
}

function fft (re, im) {
  const n = re.length;
  for (let i = 1, j = 0; i < n; i++) { let bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) { [re[i], re[j]] = [re[j], re[i]]; [im[i], im[j]] = [im[j], im[i]] } }
  for (let l = 2; l <= n; l <<= 1) {
    const a = -2 * Math.PI / l, wr = Math.cos(a), wi = Math.sin(a);
    for (let i = 0; i < n; i += l) {
      let cr = 1, ci = 0;
      for (let k = 0; k < l / 2; k++) {
        const ur = re[i + k], ui = im[i + k], vr = re[i + k + l / 2] * cr - im[i + k + l / 2] * ci, vi = re[i + k + l / 2] * ci + im[i + k + l / 2] * cr;
        re[i + k] = ur + vr; im[i + k] = ui + vi; re[i + k + l / 2] = ur - vr; im[i + k + l / 2] = ui - vi;
        const t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t;
      }
    }
  }
}

function envelope (x, ms) { const e = new Float32Array(x.length), k = 1 / (ms * 0.001 * R); let m = 0; for (let i = 0; i < x.length; i++) { m += k * (Math.abs(x[i]) - m); e[i] = m } return e }

function measure (x) {
  const env = envelope(x, 1);
  let pk = 0; for (const v of env) pk = Math.max(pk, v);
  if (pk < 1e-5) return null;
  const thr = pk * 0.01; let s0 = 0; while (s0 < env.length && env[s0] < thr) s0++; let s1 = env.length - 1; while (s1 > s0 && env[s1] < thr) s1--;
  // teeth: envelope maxima >= 8% of peak, standing 30% over the trough since the last, >= 4 ms apart
  const ons = []; let trough = 1e9, last = -1e9;
  for (let i = s0 + 1; i < s1 - 1; i++) {
    trough = Math.min(trough, env[i]);
    if (env[i] >= env[i - 1] && env[i] > env[i + 1] && env[i] > 0.08 * pk && env[i] > 1.3 * trough) {
      if (i - last < 0.004 * R) { if (ons.length && env[i] > ons[ons.length - 1][1]) { ons[ons.length - 1] = [i, env[i]]; last = i } continue }
      ons.push([i, env[i]]); last = i; trough = 1e9;
    }
  }
  const ioi = []; for (let j = 1; j < ons.length; j++) ioi.push((ons[j][0] - ons[j - 1][0]) / R * 1000);
  const srt = [...ioi].sort((a, b) => a - b), med = srt.length ? srt[srt.length >> 1] : 0;
  const levels = ons.map(o => o[1] / pk);
  // the body: cumulative energy over the audible part
  const F = 2048, P = new Float64Array(F / 2);
  for (let s = s0; s + F <= s1; s += F / 2) {
    const re = new Float64Array(F), im = new Float64Array(F);
    for (let i = 0; i < F; i++) re[i] = x[s + i] * (0.5 - 0.5 * Math.cos(2 * Math.PI * i / F));
    fft(re, im); for (let q = 1; q < F / 2; q++) P[q] += re[q] * re[q] + im[q] * im[q];
  }
  let tot = 0; for (let q = 1; q < F / 2; q++) tot += P[q];
  const pc = f => { let c = 0; for (let q = 1; q < F / 2; q++) { c += P[q]; if (c >= f * tot) return q * R / F } return R / 2 };
  // per tooth: ring (fall to -20 dB) + floor (minimum before the next), on a sharper 0.3 ms envelope
  const e2 = envelope(x, 0.3), rings = [], floors = [];
  for (let j = 0; j < ons.length; j++) {
    let p = ons[j][0]; const end = j + 1 < ons.length ? ons[j + 1][0] : Math.min(x.length, p + Math.floor(0.08 * R));
    for (let i = p; i < Math.min(end, p + Math.floor(0.002 * R)); i++) if (e2[i] > e2[p]) p = i;
    let t = p; while (t < end && e2[t] > e2[p] * 0.1) t++; if (t < end) rings.push((t - p) / R * 1000);
    let mn = 1e9; for (let i = p; i < end; i++) mn = Math.min(mn, e2[i]); floors.push(20 * Math.log10(mn / e2[p] + 1e-9));
  }
  const median = a => { const s = [...a].sort((u, v) => u - v); return s.length ? s[s.length >> 1] : NaN };
  return {
    audible: Math.round((s1 - s0) / R * 1000), teeth: ons.length, rate: med ? 1000 / med : 0, ioi,
    levels, band: [pc(0.1), pc(0.5), pc(0.9)], ring: median(rings), floor: median(floors.slice(0, -1)),
  };
}

const near = (v, t, tol) => Math.abs(v - t) <= tol * Math.abs(t);
function judge (name, m, T) {
  const out = [], f = (ok, what) => out.push([ok, what]);
  if (!m) { f(false, `${name}: SILENT, nothing to measure`); return out }
  f(m.teeth >= 6, `${name}: ${m.teeth} teeth found (>= 6, else there is nothing scraped)`);
  f(near(m.audible, T.audible, 0.15), `${name}: audible ${m.audible} ms vs ${T.audible} (±15%)`);
  f(near(m.rate, T.rate, 0.08), `${name}: ${m.rate.toFixed(1)} teeth/s vs ${T.rate} (±8%)`);
  ['10', '50', '90'].forEach((p, i) => f(near(m.band[i], T.band[i], 0.10), `${name}: body ${p}% at ${Math.round(m.band[i])} Hz vs ${T.band[i]} (±10%)`));
  f(near(m.ring, T.ring, 0.35), `${name}: tooth ring ${m.ring.toFixed(1)} ms vs ${T.ring} (±35%)`);
  f(m.floor <= T.floor + 12 && m.floor >= T.floor - 15, `${name}: floor between teeth ${m.floor.toFixed(1)} dB vs ${T.floor} (+12/-15 dB: not a hiss, not digital silence)`);
  if (T.minLevel !== undefined) {
    const lo = [...m.levels].sort((a, b) => a - b)[Math.floor(m.levels.length * 0.1)];
    f(lo >= 0.3, `${name}: teeth stay up, 10th-percentile level ${lo.toFixed(2)} (>= 0.3; the sample's lowest is ${T.minLevel})`);
  }
  if (T.lastLevel !== undefined) {
    const imax = m.levels.indexOf(Math.max(...m.levels)), lastLv = m.levels[m.levels.length - 1];
    f(imax <= 3, `${name}: the loudest tooth is among the first four (#${imax + 1}), a rattle swells then dies`);
    f(lastLv <= 0.25, `${name}: the last tooth has died to ${lastLv.toFixed(2)} of the loudest (<= 0.25; the sample: ${T.lastLevel})`);
    f(m.ioi.slice(0, 4).some(v => v < 0.5 * 1000 / T.rate), `${name}: a BOUNCE among the first teeth (an interval under half the spacing: ${m.ioi.slice(0, 4).map(Math.round).join(', ')} ms)`);
  }
  return out;
}

// ── --selfcheck: the measurement on signals whose answers are known by construction ──────────────────
function selfcheck () {
  const synth = (rate, fc, q, secs, opt = {}) => {      // clicks at `rate`/s ringing a resonator at fc
    const n = Math.floor(secs * R), x = new Float32Array(n + Math.floor(0.05 * R)), w = 2 * Math.PI * fc / R, r = Math.exp(-Math.PI * fc / (q * R));
    let y1 = 0, y2 = 0, next = 0, k = 0;
    for (let i = 0; i < x.length; i++) {
      let u = 0;
      if (i < n && i >= next) { u = opt.decay ? Math.exp(-k / opt.decay) : 1; next += R / rate; k++ }
      if (opt.noise) u = i < n ? (Math.sin(i * 12.9898) * 43758.5453 % 1) * 0.3 : 0;
      const y = u + 2 * r * Math.cos(w) * y1 - r * r * y2; y2 = y1; y1 = y; x[i] = y;
    }
    return x;
  };
  const checks = [];
  const t = (ok, what) => checks.push([ok, what]);
  const a = measure(synth(50, 3000, 20, 0.30));
  t(Math.abs(a.rate - 50) < 2, `50 clicks/s reads ${a.rate.toFixed(1)}`);
  t(Math.abs(a.band[1] - 3000) < 250, `a 3 kHz body reads its median at ${Math.round(a.band[1])} Hz`);
  t(Math.abs(a.audible - 300) < 40, `a 0.30 s train reads ${a.audible} ms audible`);
  t(a.teeth >= 13 && a.teeth <= 16, `a 0.30 s train at 50/s counts ${a.teeth} teeth (15 by construction)`);
  const b = measure(synth(20, 2500, 8, 0.45, { decay: 2.5 }));
  t(b.levels[b.levels.length - 1] < 0.1, `a decaying train's last tooth reads ${b.levels[b.levels.length - 1].toFixed(2)} of its first`);
  // NEGATIVE CONTROL: steady noise through the same body has no teeth to find. If the tooth finder counted
  // noise wiggles as teeth, every "rate" above would be measuring nothing
  const c = measure(synth(1, 3000, 20, 0.30, { noise: 1 }));
  t(!c || c.teeth < 4, `steady resonated NOISE finds ${c ? c.teeth : 0} teeth (< 4: the finder is not counting wiggles)`);
  for (const [ok, w] of checks) console.log(`  ${ok ? '✓' : '✗'} ${w}`);
  const bad = checks.filter(c => !c[0]).length;
  console.log(bad ? `scrape-check --selfcheck: ${bad} WRONG` : `scrape-check --selfcheck: ${checks.length}/${checks.length} known answers`);
  return bad ? 1 : 0;
}

if (argv.includes('--selfcheck')) process.exit(selfcheck());
if (argv.includes('--ref')) {
  const m = measure(readWav(argv[argv.indexOf('--ref') + 1]));
  if (!m) { console.log('silent'); process.exit(1) }
  console.log(JSON.stringify({ audible: m.audible, teeth: m.teeth, rate: +m.rate.toFixed(1), band: m.band.map(Math.round), ring: +m.ring.toFixed(1), floor: +m.floor.toFixed(1), levels: m.levels.map(v => +v.toFixed(2)), first_iois: m.ioi.slice(0, 6).map(Math.round) }));
  process.exit(0);
}

const dir = path.join(ROOT, 'build', '.scrape'), wav = path.join(dir, 'probe.wav');
fs.mkdirSync(dir, { recursive: true });
const r = spawnSync('node', ['tools/play.js', 'scraper', 'script', '/dev/null', '--headless', '--frames', '200', '--wav', wav],
  { cwd: ROOT, encoding: 'utf8', env: { ...process.env, DE_DEFINES: 'PROBE' } });
if (r.status !== 0 || !fs.existsSync(wav)) { process.stderr.write((r.stdout || '') + (r.stderr || '')); console.error('scrape-check: render failed'); process.exit(1) }
const x = readWav(wav), results = [];
for (const name of ['guiro', 'quijada']) {
  const [a, b] = WIN[name];
  results.push(...judge(name, measure(x.subarray(Math.floor(a * R), Math.floor(b * R))), TARGET[name]));
}
const bad = results.filter(c => !c[0]);
if (!quiet) for (const [ok, w] of results) console.log(`  ${ok ? '✓' : '✗'} ${w}`);
console.log(bad.length ? `scrape-check: FAIL (${bad.length})${quiet ? ' — ' + bad.map(c => c[1]).join(' | ') : ''}` : `scrape-check: PASS (${results.length} checks)`);
process.exit(bad.length ? 1 : 0);
