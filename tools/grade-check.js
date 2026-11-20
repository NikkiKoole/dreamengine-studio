#!/usr/bin/env node
// grade-check.js — road GRADIENT over 50 m stretches, by road class, for a vector-features .json with a
// heightfield (islandstudio's export: the ground backroads drives on). The gate for "roads climb like
// roads" (docs/design/island-studio.md, "Hairpins and a gradient cap").
//
// USAGE
//   node tools/grade-check.js <file.json> [--where] [--near X,Y,R]
//     --where       list the steepest stretches (with distance to the nearest bridge piece)
//     --near X,Y,R  only roads with a point within R m of (X,Y) (export frame), e.g. a mast service road
//     --profile     use each road's own graded heights ("z", roadmap item 5) instead of the heightfield,
//                   and report junction ends whose heights disagree (must be 0)
//     --selfcheck   a synthetic ramp with known grades; exits 1 on mismatch
//
// DEFINITION (keep stable): resample each road line every 10 m; grade = |dz| / length over each 5-step
// (~50 m) window, z bilinear on the exported heightfield (rows south -> north). Bridge pieces (sub starting
// "B") are skipped: their deck is level, the valley under it isn't. Groups: main = highway + arterial,
// secondary, lane = road, track.
const fs = require('fs');
const argv = process.argv.slice(2);
const opt = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };

const PROFILE = argv.includes('--profile');
function measure(d, near) {
  const hf = d.heightfield, G = hf.g, W = d.bbox[2];
  const z = (x, y) => {
    const j = Math.max(0, Math.min(G - 1.001, x / W * (G - 1))), i = Math.max(0, Math.min(G - 1.001, y / W * (G - 1)));
    const j0 = Math.floor(j), i0 = Math.floor(i), tj = j - j0, ti = i - i0;
    return (hf.z[i0 * G + j0] * (1 - tj) + hf.z[i0 * G + j0 + 1] * tj) * (1 - ti) + (hf.z[(i0 + 1) * G + j0] * (1 - tj) + hf.z[(i0 + 1) * G + j0 + 1] * tj) * ti;
  };
  const GR = { highway: 'main', arterial: 'main', secondary: 'secondary', road: 'lane', track: 'track' };
  const st = {}, worst = [], BR = [];
  for (const f of d.features) if ((f.sub || '').startsWith('B')) for (let i = 0; i < f.pts.length; i += 2) BR.push([f.pts[i], f.pts[i + 1]]);
  for (const f of d.features) {
    const g = GR[f.kind]; if (!g || (!PROFILE && (f.sub || '').startsWith('B'))) continue;   // a deck is level; with --profile its z is the deck
    const p = f.pts;
    if (near) { let ok = false; for (let i = 0; i < p.length; i += 2) if (Math.hypot(p[i] - near[0], p[i + 1] - near[1]) < near[2]) ok = true; if (!ok) continue; }
    const xs = [], ys = [], zs = [], useZ = PROFILE && Array.isArray(f.z);
    if (PROFILE && !useZ) continue;
    for (let i = 2; i < p.length; i += 2) {
      const L = Math.hypot(p[i] - p[i - 2], p[i + 1] - p[i - 1]), n = Math.max(1, Math.ceil(L / 10));
      for (let k = 0; k < n; k++) {
        xs.push(p[i - 2] + (p[i] - p[i - 2]) * k / n); ys.push(p[i - 1] + (p[i + 1] - p[i - 1]) * k / n);
        if (useZ) zs.push(f.z[(i >> 1) - 1] + (f.z[i >> 1] - f.z[(i >> 1) - 1]) * k / n);
      }
    }
    xs.push(p[p.length - 2]); ys.push(p[p.length - 1]); if (useZ) zs.push(f.z[f.z.length - 1]);
    const Z = useZ ? (k) => zs[k] : (k) => z(xs[k], ys[k]);
    const s = st[g] = st[g] || { n: 0, o12: 0, o15: 0, o20: 0, max: 0 };
    for (let k = 5; k < xs.length; k += 5) {
      let L = 0; for (let q = k - 4; q <= k; q++) L += Math.hypot(xs[q] - xs[q - 1], ys[q] - ys[q - 1]);
      if (L < 20) continue;
      const gr = Math.abs(Z(k) - Z(k - 5)) / L;
      s.n++; if (gr > 0.12) s.o12++; if (gr > 0.15) s.o15++; if (gr > 0.20) s.o20++; if (gr > s.max) s.max = gr;
      if (gr > 0.18) worst.push([gr, g, Math.round(xs[k]), Math.round(ys[k])]);
    }
  }
  return { st, worst, BR };
}

function selfcheck() {
  // 1 km x 1 km, z = 0.1 * x (a 10 % ramp eastward); one road east (10 %), one road north (0 %)
  const G = 11, z = []; for (let i = 0; i < G; i++) for (let j = 0; j < G; j++) z.push(0.1 * j * 100);
  const d = { bbox: [0, 0, 1000, 1000], heightfield: { g: G, z }, features: [
    { kind: 'arterial', pts: [100, 500, 900, 500] }, { kind: 'road', pts: [500, 100, 500, 900] },
    { kind: 'track', sub: 'B1', pts: [100, 200, 900, 200] } ] };
  const { st } = measure(d);
  const ok = Math.abs(st.main.max - 0.1) < 0.002 && st.lane.max < 0.002 && !st.track;
  console.log(`${ok ? 'PASS' : 'FAIL'}  east road ${(st.main.max * 100).toFixed(1)}% (want 10), north road ${(st.lane.max * 100).toFixed(1)}% (want 0), bridge skipped ${!st.track}`);
  process.exit(ok ? 0 : 1);
}

if (argv.includes('--selfcheck')) selfcheck();
const file = argv.find((a, i) => !a.startsWith('--') && !(i > 0 && argv[i - 1] === '--near'));
if (!file) { console.error('usage: node tools/grade-check.js <file.json> [--where] [--near X,Y,R]'); process.exit(2); }
const near = opt('--near', null) ? opt('--near', null).split(',').map(Number) : null;
const { st, worst, BR } = measure(JSON.parse(fs.readFileSync(file, 'utf8')), near);
for (const g of ['main', 'secondary', 'lane', 'track']) {
  const s = st[g]; if (!s) continue;
  const pc = (v) => (100 * v / s.n).toFixed(2).padStart(6) + '%';
  console.log(`${g.padEnd(10)} 50 m stretches ${String(s.n).padStart(6)}   >12% ${pc(s.o12)}   >15% ${pc(s.o15)}   >20% ${pc(s.o20)}   max ${(100 * s.max).toFixed(0)}%`);
}
if (PROFILE) {
  const ends = new Map(); let bad = 0;
  for (const f of JSON.parse(fs.readFileSync(file, 'utf8')).features) if (Array.isArray(f.z))
    for (const k of [0, f.z.length - 1]) { const key = f.pts[2 * k] + ',' + f.pts[2 * k + 1]; if (!ends.has(key)) ends.set(key, []); ends.get(key).push(f.z[k]); }
  for (const [, v] of ends) if (Math.max(...v) - Math.min(...v) > 0.11) bad++;
  console.log(`junction ends whose heights disagree: ${bad}`);
}
if (argv.includes('--where')) {
  const nb = (x, y) => { let b = 1e9; for (const [a, c] of BR) b = Math.min(b, Math.hypot(a - x, c - y)); return b; };
  worst.sort((a, b) => b[0] - a[0]);
  for (const w of worst.slice(0, 12)) console.log(`  ${w[1].padEnd(9)} ${(100 * w[0]).toFixed(0)}% @ ${w[2]},${w[3]}  nearest bridge ${Math.round(nb(w[2], w[3]))} m`);
}
