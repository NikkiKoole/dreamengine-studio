#!/usr/bin/env node
// road-density.js — road km per km² of land, by class, for a vector-features .json (the IR that
// data-tools/roadview/osm-roads.js writes for real OSM, and that islandstudio exports with X).
// Puts a generated island and a real one on the same footing; optionally renders a true-scale SVG.
//
// USAGE
//   node tools/road-density.js <file.json> [more.json ...] [--lat L] [--area KM2] [--svg out.svg]
//
//   --lat   latitude of an OSM fetch: osm-roads.js stores web-Mercator metres, which stretch
//           lengths by 1/cos(lat) (×1.7 at the Isle of Man). Omit for islandstudio (true metres).
//           One value applies to every file; run files separately if their latitudes differ.
//   --area  land area in km² (for an OSM fetch: from the island's official area). islandstudio
//           exports record "land_km2" in the header, which is used when --area is absent.
//   --svg   also render the first file as a true-scale SVG (20 px/km, dashed 40x40 km frame,
//           roads by class over land use). Convert with rsvg-convert, NOT ImageMagick's built-in
//           SVG reader (it silently stops after the first few thousand elements).
//
// OUTPUT (one line per file):   main / minor / track km per km², where
//   main  = highway + arterial + secondary + tertiary   (OSM trunk/primary/secondary/tertiary)
//   minor = road                                         (unclassified + residential)
//   track = track                                        service roads are printed but not grouped
// Reference values (2026-11-19, docs/design/island-studio.md): Isle of Man 0.92 / 0.83 / 1.03,
// Sado 0.69 / 1.91 / 1.65.
//
// --selfcheck: a synthetic 10 km² file with known lengths; exits 1 on mismatch.

const fs = require('fs');
const argv = process.argv.slice(2);
const opt = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const files = argv.filter((a, i) => !a.startsWith('--') && !(i > 0 && ['--lat', '--area', '--svg'].includes(argv[i - 1])));
const LAT = parseFloat(opt('--lat', '0'));
const AREA = opt('--area', null);
const SVG = opt('--svg', null);

const AREA_KINDS = new Set(['water', 'green', 'farm', 'residential', 'commercial', 'industrial', 'sand', 'other_area', 'building', 'parking']);
const MAIN = ['highway', 'arterial', 'secondary', 'tertiary'];

function measure(d, k) {
  const km = {};
  for (const f of d.features) {
    if (AREA_KINDS.has(f.kind)) continue;
    let L = 0;
    for (let i = 2; i < f.pts.length; i += 2) L += Math.hypot(f.pts[i] - f.pts[i - 2], f.pts[i + 1] - f.pts[i - 1]) * k;
    km[f.kind] = (km[f.kind] || 0) + L / 1000;
  }
  return km;
}
function line(name, km, area) {
  const main = MAIN.reduce((a, c) => a + (km[c] || 0), 0), minor = km.road || 0, track = km.track || 0;
  const f = (v) => (v / area).toFixed(2);
  return `${name.padEnd(28)} land ${String(Math.round(area)).padStart(5)} km2 | main ${f(main)} | minor ${f(minor)} | track ${f(track)}` +
         ` | service ${f(km.service || 0)}   (km: ${main.toFixed(0)} / ${minor.toFixed(0)} / ${track.toFixed(0)})`;
}

function svg(d, k, out, title) {
  const PX = 1 / 50;   // 20 px per km
  const W = Math.max(40000, (d.bbox[2] - d.bbox[0]) * k) * PX + 40, H = Math.max(40000, (d.bbox[3] - d.bbox[1]) * k) * PX + 60;
  const st = { highway: ['#e04030', 3], arterial: ['#f08030', 2.4], secondary: ['#f0b040', 2], tertiary: ['#f0e070', 1.6],
               road: ['#ffffff', 1], service: ['#bbbbbb', 0.5], track: ['#a07850', 0.6], rail: ['#202020', 1.2],
               canal: ['#3070c0', 0.6], coast: ['#103060', 1.5] };
  const area = { water: '#3070c0', green: '#9cc490', farm: '#e8e0b0', residential: '#d09090', commercial: '#d09090',
                 industrial: '#b0a0b0', sand: '#e8d890', other_area: '#d8d8cc' };
  const P = (pts) => { let r = ''; for (let i = 0; i < pts.length; i += 2) r += (pts[i] * k * PX + 20).toFixed(1) + ',' + (H - 40 - pts[i + 1] * k * PX).toFixed(1) + ' '; return r; };
  const s = [`<svg xmlns="http://www.w3.org/2000/svg" width="${W | 0}" height="${H | 0}"><rect width="100%" height="100%" fill="#e8eef0"/>`];
  for (const f of d.features) if (area[f.kind] && f.pts.length > 4) s.push(`<polygon points="${P(f.pts)}" fill="${area[f.kind]}"/>`);
  for (const kind of ['canal', 'coast', 'track', 'service', 'road', 'tertiary', 'secondary', 'arterial', 'highway', 'rail'])
    for (const f of d.features) if (f.kind === kind)
      s.push(`<polyline points="${P(f.pts)}" fill="none" stroke="${st[kind][0]}" stroke-width="${st[kind][1]}"/>`);
  s.push(`<rect x="20" y="${H - 40 - 800}" width="800" height="800" fill="none" stroke="#000" stroke-dasharray="8,6" stroke-width="2"/>`);
  s.push(`<rect x="20" y="${H - 25}" width="200" height="6" fill="#000"/><text x="230" y="${H - 17}" font-size="16" font-family="sans-serif">10 km (dashed square = 40x40 km)</text>`);
  s.push(`<text x="20" y="22" font-size="22" font-family="sans-serif" font-weight="bold">${title}</text></svg>`);
  fs.writeFileSync(out, s.join('\n'));
}

if (argv.includes('--selfcheck')) {
  // 10 km² of land: 2 km of arterial, 3 km of road, 5 km of track (+ an area that must be ignored)
  const d = { bbox: [0, 0, 4000, 4000], land_km2: 10, features: [
    { kind: 'arterial', pts: [0, 0, 2000, 0] }, { kind: 'road', pts: [0, 0, 0, 1500, 0, 3000] },
    { kind: 'track', pts: [0, 0, 3000, 4000] }, { kind: 'residential', pts: [0, 0, 100, 0, 100, 100, 0, 0] } ] };
  const km = measure(d, 1);
  const ok = Math.abs(km.arterial - 2) < 1e-6 && Math.abs(km.road - 3) < 1e-6 && Math.abs(km.track - 5) < 1e-6 && !km.residential;
  const k54 = measure(d, Math.cos(54 * Math.PI / 180)).arterial;
  const ok2 = Math.abs(k54 - 2 * Math.cos(54 * Math.PI / 180)) < 1e-6;
  console.log(ok && ok2 ? 'selfcheck PASS' : 'selfcheck FAIL', JSON.stringify(km));
  process.exit(ok && ok2 ? 0 : 1);
}
if (!files.length) { console.error('usage: node tools/road-density.js <file.json> [...] [--lat L] [--area KM2] [--svg out.svg]'); process.exit(2); }
const k = Math.cos(LAT * Math.PI / 180);
files.forEach((file, n) => {
  const d = JSON.parse(fs.readFileSync(file, 'utf8'));
  const area = AREA ? parseFloat(AREA) : d.land_km2;
  if (!area) { console.error(`${file}: no land area (pass --area KM2)`); process.exit(2); }
  console.log(line(d.name || file, measure(d, k), area));
  if (SVG && n === 0) svg(d, k, SVG, d.name || file);
});
