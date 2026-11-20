#!/usr/bin/env node
// kink-check.js — SHARP KINKS per 100 km of road, by class, for a vector-features .json (the IR that
// data-tools/roadview/osm-roads.js writes for real OSM and islandstudio exports with X). The number
// behind "main roads have teeth and hooks" (docs/design/island-studio.md, "Smooth roads at 1024").
//
// USAGE
//   node tools/kink-check.js <file.json> [more.json ...] [--lat L] [--win M] [--deg D] [--where CLASS]
//
//   --lat    latitude of an OSM fetch (web-Mercator metres stretch by 1/cos(lat)); omit for islandstudio
//   --win    look-behind / look-ahead distance in metres (default 20)
//   --deg    a kink is a turn sharper than this between the two windows (default 60)
//   --where  also list kink positions (local metres) for one group: main | secondary | lane | track
//   --selfcheck  synthetic lines with known kinks; exits 1 on mismatch
//
// DEFINITION (keep stable; numbers in the doc depend on it): walk each polyline; at every vertex at
// least WIN from both ends, compare the direction over the WIN metres before it with the WIN metres
// after it. A turn > DEG degrees is a kink; after one, the next can't start within 2*WIN (one bend =
// one kink). Polyline ends are skipped, so a turn AT a junction (where islandstudio splits roads) is
// not counted. Groups: main = highway + arterial, secondary = secondary + tertiary, lane = road, track.
//
// Reference (2026-11-20): Isle of Man main 7 · secondary 8 · lane 78 · track 71.

const fs = require('fs');
const argv = process.argv.slice(2);
const opt = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const VALOPTS = ['--lat', '--win', '--deg', '--where'];
const files = argv.filter((a, i) => !a.startsWith('--') && !(i > 0 && VALOPTS.includes(argv[i - 1])));
const LAT = parseFloat(opt('--lat', '0'));
const WIN = parseFloat(opt('--win', '20'));
const DEG = parseFloat(opt('--deg', '60'));
const WHERE = opt('--where', null);
const GROUP = { highway: 'main', arterial: 'main', secondary: 'secondary', tertiary: 'secondary', road: 'lane', track: 'track' };
const GROUPS = ['main', 'secondary', 'lane', 'track'];

// point at arc length s along (xs, ys, cum)
function at(xs, ys, cum, s) {
  let lo = 0, hi = cum.length - 1;
  while (hi - lo > 1) { const m = (lo + hi) >> 1; if (cum[m] <= s) lo = m; else hi = m; }
  const L = cum[hi] - cum[lo] || 1, t = (s - cum[lo]) / L;
  return [xs[lo] + (xs[hi] - xs[lo]) * t, ys[lo] + (ys[hi] - ys[lo]) * t];
}

function kinks(pts, k, win, deg) {
  const n = pts.length >> 1, xs = [], ys = [], cum = [0];
  for (let i = 0; i < n; i++) { xs.push(pts[2 * i] * k); ys.push(pts[2 * i + 1] * k); }
  for (let i = 1; i < n; i++) cum.push(cum[i - 1] + Math.hypot(xs[i] - xs[i - 1], ys[i] - ys[i - 1]));
  const len = cum[n - 1], out = [], lim = Math.cos(deg * Math.PI / 180);
  let last = -1e9;
  for (let i = 1; i < n - 1; i++) {
    const s = cum[i];
    if (s < win || s > len - win || s < last + 2 * win) continue;
    const [bx, by] = at(xs, ys, cum, s - win), [fx, fy] = at(xs, ys, cum, s + win);
    const ax = xs[i] - bx, ay = ys[i] - by, cx = fx - xs[i], cy = fy - ys[i];
    const la = Math.hypot(ax, ay), lc = Math.hypot(cx, cy);
    if (la < 1e-6 || lc < 1e-6) continue;
    if ((ax * cx + ay * cy) / (la * lc) < lim) { out.push([xs[i] / k, ys[i] / k]); last = s; }
  }
  return { len, out };
}

function measure(doc, k) {
  const r = {}; for (const g of GROUPS) r[g] = { km: 0, n: 0, at: [] };
  for (const f of doc.features) {
    const g = GROUP[f.kind]; if (!g || !f.pts || f.pts.length < 4) continue;
    const { len, out } = kinks(f.pts, k, WIN, DEG);
    r[g].km += len / 1000; r[g].n += out.length; for (const p of out) r[g].at.push(p);
  }
  return r;
}

function selfcheck() {
  const line = (pts) => ({ kind: 'arterial', pts });
  const straight = []; for (let x = 0; x <= 1000; x += 10) straight.push(x, 0);
  const zig = []; for (let x = 0; x <= 400; x += 10) zig.push(x, 0); zig.push(400, 10); for (let y = 20; y <= 400; y += 10) zig.push(400, y);   // one 90° corner
  const gentle = []; for (let a = 0; a <= 90; a += 1) { const t = a * Math.PI / 180; gentle.push(Math.sin(t) * 500, 500 - Math.cos(t) * 500); }   // r = 500 m arc
  const two = []; for (let x = 0; x <= 300; x += 10) two.push(x, 0); for (let y = 10; y <= 300; y += 10) two.push(300, y); for (let x = 310; x <= 600; x += 10) two.push(x, 300);   // two corners 300 m apart
  const cases = [['straight', straight, 0], ['one corner', zig, 1], ['gentle r=500 arc', gentle, 0], ['two corners', two, 2]];
  let bad = 0;
  for (const [name, pts, want] of cases) {
    const got = measure({ features: [line(pts)] }, 1).main.n;
    console.log(`${got === want ? 'PASS' : 'FAIL'}  ${name}: ${got} kinks (want ${want})`); if (got !== want) bad++;
  }
  process.exit(bad ? 1 : 0);
}

if (argv.includes('--selfcheck')) selfcheck();
if (!files.length) { console.error('usage: node tools/kink-check.js <file.json> [...] [--lat L] [--win M] [--deg D] [--where GROUP]'); process.exit(2); }
const k = LAT ? Math.cos(LAT * Math.PI / 180) : 1;
console.log(`sharp kinks per 100 km (> ${DEG}° within ${WIN} m)     main  second    lane   track`);
for (const f of files) {
  const r = measure(JSON.parse(fs.readFileSync(f, 'utf8')), k);
  const cell = (g) => (r[g].km > 0.5 ? (100 * r[g].n / r[g].km).toFixed(0) : '-').padStart(8);
  console.log(`${f.split('/').pop().padEnd(44)}${GROUPS.map(cell).join('')}`);
  if (WHERE && r[WHERE]) for (const [x, y] of r[WHERE].at) console.log(`  ${x.toFixed(0)},${y.toFixed(0)}`);
}
