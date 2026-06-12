#!/usr/bin/env node
// fml2cart.js — convert a Floorplanner .fml into level data for the floorwalker cart.
//
//   node fmltools/fml2cart.js <file.fml> [--out tools/carts/floorwalker.c]
//                                      [--scale <mm-per-px>]  (default 25)
//                                      [--floor <i>] [--design <i>]
//                                      [--stdout]   (print the data block instead of splicing)
//
// Walls are arbitrary-angle segments (a->b + thickness). Door openings become
// walkable GAPS (the wall is split into solid spans around them); windows stay
// solid but are emitted separately so the cart can draw them as glass. Areas are
// polygons tinted by room role. Furniture become oriented boxes (center + size +
// rotation) carrying a refid index for the later sprite-bake phase. Coordinates
// are converted mm -> engine pixels and recentred to a small margin.
//
// The generated C is spliced into floorwalker.c between the sentinels:
//   /* >>>FML_DATA */  ...generated...  /* <<<FML_DATA */
// keeping the cart a single self-contained .c (the editor loads embedded source).

const fs = require('fs');
const path = require('path');

// ---- args ----
const argv = process.argv.slice(2);
if (!argv.length || argv[0].startsWith('-')) {
  console.error('usage: node fmltools/fml2cart.js <file.fml> [--out <cart.c>] [--scale N] [--floor i] [--design i] [--stdout]');
  process.exit(1);
}
// units are centimetres (wall thickness ~15-30cm in real files); scale = cm/px.
const opt = { in: argv[0], out: 'tools/carts/floorwalker.c', scale: 8, floor: 0, design: 0, maxfurn: 280, stdout: false };
for (let i = 1; i < argv.length; i++) {
  const a = argv[i];
  if (a === '--out') opt.out = argv[++i];
  else if (a === '--scale') opt.scale = parseFloat(argv[++i]);
  else if (a === '--floor') opt.floor = parseInt(argv[++i], 10);
  else if (a === '--design') opt.design = parseInt(argv[++i], 10);
  else if (a === '--maxfurn') opt.maxfurn = parseFloat(argv[++i]); // mm cap: above this an item is a surface/rug, not furniture
  else if (a === '--stdout') opt.stdout = true;
  else { console.error('unknown arg', a); process.exit(1); }
}

const fml = JSON.parse(fs.readFileSync(opt.in, 'utf8'));
const floor = fml.floors[opt.floor];
const design = floor.designs[opt.design];
const walls = design.walls || [];
const areas = design.areas || [];
const items = design.items || [];

// ---- floor colour ----
// Newer FMLs give each area a real hex `color`; older ones a numeric `role`.
// Prefer the real colour (quantised to the engine palette); fall back to role.
const ROLE_COLOR = { 1: 17, 3: 18, 5: 19, 6: 20, 8: 21, 7: 2, 2: 1, 4: 3 };
const FLOOR_FALLBACK = 16; // darkest brown
const PALETTE = (() => {
  try {
    const j = JSON.parse(fs.readFileSync('editor/public/palettes/pico32.json', 'utf8'));
    return (j.palette || j).map(h => [parseInt(h.slice(1, 3), 16), parseInt(h.slice(3, 5), 16), parseInt(h.slice(5, 7), 16)]);
  } catch { return null; }
})();
function nearestIdx(hex) {
  if (!PALETTE || typeof hex !== 'string' || hex[0] !== '#') return null;
  const r = parseInt(hex.slice(1, 3), 16), g = parseInt(hex.slice(3, 5), 16), b = parseInt(hex.slice(5, 7), 16);
  let best = 0, bd = 1e18;
  for (let i = 0; i < PALETTE.length; i++) {
    const dr = r - PALETTE[i][0], dg = g - PALETTE[i][1], db = b - PALETTE[i][2];
    const d = 0.3 * dr * dr + 0.59 * dg * dg + 0.11 * db * db;
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}
function floorColor(a) {
  const c = nearestIdx(a.color);
  if (c != null) return c;
  return ROLE_COLOR[a.role] ?? FLOOR_FALLBACK;
}

// ---- bounds (from walls + area polys) ----
let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
const eat = (x, y) => { if (x < minX) minX = x; if (y < minY) minY = y; if (x > maxX) maxX = x; if (y > maxY) maxY = y; };
for (const w of walls) { eat(w.a.x, w.a.y); eat(w.b.x, w.b.y); }
for (const a of areas) for (const p of (a.poly || [])) eat(p.x, p.y);
if (!isFinite(minX)) { console.error('no geometry found'); process.exit(1); }

const MARGIN = 24; // px of empty border around the floor
const S = opt.scale;
const PX = (x) => Math.round((x - minX) / S) + MARGIN;
const PY = (y) => Math.round((y - minY) / S) + MARGIN;
const PL = (mm) => Math.max(1, Math.round(mm / S));      // generic length mm -> px
const PT = (mm) => Math.max(3, Math.round(mm / S));      // wall thickness: visible minimum
const W = Math.round((maxX - minX) / S) + MARGIN * 2;
const H = Math.round((maxY - minY) / S) + MARGIN * 2;

// ---- unified asset ref table (furniture + door/window openings) ----
const HEX40 = /^[0-9a-f]{40}$/;
const refs = [];
const refIndex = (r) => { if (!HEX40.test(r)) return -1; let i = refs.indexOf(r); if (i < 0) { i = refs.length; refs.push(r); } return i; };

// ---- walls: split by DOOR openings into solid spans; windows kept + emitted ----
const segs = [];     // {ax,ay,bx,by,thick} solid, collidable
const windows = [];  // {ax,ay,bx,by,thick} glass, collidable + drawn light
const doorways = [];  // {ax,ay,bx,by,thick} just for drawing the threshold (walkable)
const openings = []; // {cx,cy,w,h,rot,ref,type} real door/window sprites (doors non-colliding)

for (const w of walls) {
  const ax = w.a.x, ay = w.a.y, bx = w.b.x, by = w.b.y;
  const Lmm = Math.hypot(bx - ax, by - ay) || 1;
  const thick = PT(w.thickness || 15);
  const wallDeg = Math.atan2(by - ay, bx - ax) * 180 / Math.PI;
  // opening intervals in param t = [0,1]
  const doors = [], wins = [];
  for (const o of (w.openings || [])) {
    const half = (o.width / 2) / Lmm;
    const lo = Math.max(0, o.t - half), hi = Math.min(1, o.t + half);
    (o.type === 'window' ? wins : doors).push([lo, hi]);
    // placement for the real door/window sprite (centred on the opening, along the wall).
    // windows sit thin in the wall; doors get a near-square footprint so the leaf/swing reads.
    openings.push({
      cx: PX(ax + (bx - ax) * o.t), cy: PY(ay + (by - ay) * o.t),
      w: PL(o.width), h: (o.type === 'window' ? thick : PL(o.width)), rot: wallDeg, type: o.type,
      ref: refIndex(o.refid || ''),
    });
  }
  const lerp = (t) => ({ x: ax + (bx - ax) * t, y: ay + (by - ay) * t });
  const emit = (arr, t0, t1) => {
    const p = lerp(t0), q = lerp(t1);
    arr.push({ ax: PX(p.x), ay: PY(p.y), bx: PX(q.x), by: PY(q.y), thick });
  };
  // windows: drawn as glass spans
  for (const [lo, hi] of wins) emit(windows, lo, hi);
  // doorway thresholds: drawn faint
  for (const [lo, hi] of doors) emit(doorways, lo, hi);
  // solid spans = [0,1] minus union of door intervals (windows stay solid -> kept in span)
  const cuts = doors.slice().sort((p, q) => p[0] - q[0]);
  let cur = 0;
  for (const [lo, hi] of cuts) {
    if (lo > cur) emit(segs, cur, lo);
    cur = Math.max(cur, hi);
  }
  if (cur < 1) emit(segs, cur, 1);
}

// ---- areas: polygons, each a distinct colour ----
// Real floor colours all quantise to the same grey, so for a game-representation
// look we give every room its own colour. Deterministic (stable across rebuilds),
// spread so adjacent rooms differ. A curated set of readable mid/dark floor tones.
const AREA_COLORS = [1, 2, 3, 4, 5, 17, 18, 19, 20, 21, 28, 29, 13, 24, 26, 27];
const areaOut = [];
const apts = [];
let ai = 0;
for (const a of areas) {
  const poly = a.poly || [];
  if (poly.length < 3) continue;
  const off = apts.length / 2;
  for (const p of poly) { apts.push(PX(p.x), PY(p.y)); }
  const color = AREA_COLORS[(ai * 7) % AREA_COLORS.length]; // *7 spreads neighbours apart
  areaOut.push({ color, n: poly.length, off,
                 name: (a.customName || a.name || '').replace(/"/g, "'") });
  ai++;
}

// ---- furniture: oriented boxes (filter out giant surface/ground planes) ----
const furn = [];
let skipped = 0;
for (const it of items) {
  // skip surface/rug/ground planes (huge flat footprints) — real furniture rarely
  // exceeds ~2.6m on a side; these big items are the floor-covering layer instead
  if ((it.width > opt.maxfurn) || (it.height > opt.maxfurn)) { skipped++; continue; }
  furn.push({
    cx: PX(it.x), cy: PY(it.y),
    w: PL(it.width), h: PL(it.height),
    rot: it.rotation || 0,
    ref: refIndex(it.refid || ''),
  });
}

// ---- spawn: centre of the largest area, else floor centre ----
let spawn = { x: W / 2, y: H / 2 };
if (areaOut.length) {
  let best = -1;
  for (const a of areaOut) {
    // centroid
    let cx = 0, cy = 0;
    for (let k = 0; k < a.n; k++) { cx += apts[(a.off + k) * 2]; cy += apts[(a.off + k) * 2 + 1]; }
    cx /= a.n; cy /= a.n;
    // rough area via bbox
    let ax0 = Infinity, ay0 = Infinity, ax1 = -Infinity, ay1 = -Infinity;
    for (let k = 0; k < a.n; k++) { const x = apts[(a.off + k) * 2], y = apts[(a.off + k) * 2 + 1]; ax0 = Math.min(ax0, x); ay0 = Math.min(ay0, y); ax1 = Math.max(ax1, x); ay1 = Math.max(ay1, y); }
    const area = (ax1 - ax0) * (ay1 - ay0);
    if (area > best) { best = area; spawn = { x: Math.round(cx), y: Math.round(cy) }; }
  }
}

// ---- emit C ----
const segC = (s) => `{${s.ax},${s.ay},${s.bx},${s.by},${s.thick}}`;
const fmt = (arr, f) => arr.map(f).join(',');
let c = '';
c += `/* >>>FML_DATA   generated by fmltools/fml2cart.js from ${path.basename(opt.in)} @ ${opt.scale}mm/px — do not hand-edit */\n`;
c += `#define LV_W ${W}\n#define LV_H ${H}\n`;
c += `static const float LV_SPAWN_X = ${spawn.x}, LV_SPAWN_Y = ${spawn.y};\n`;
c += `typedef struct { float ax,ay,bx,by,thick; } Seg;\n`;
c += `static const Seg lv_walls[] = {${fmt(segs, segC)}};\n`;
c += `#define N_WALLS ${segs.length}\n`;
c += `static const Seg lv_windows[] = {${windows.length ? fmt(windows, segC) : '{0,0,0,0,0}'}};\n`;
c += `#define N_WINDOWS ${windows.length}\n`;
c += `static const Seg lv_doorways[] = {${doorways.length ? fmt(doorways, segC) : '{0,0,0,0,0}'}};\n`;
c += `#define N_DOORWAYS ${doorways.length}\n`;
c += `typedef struct { float cx,cy,w,h,rot; int ref; } Furn;\n`;
const rotLit = (r) => Number.isInteger(r) ? `${r}` : `${r}f`;
const furnC = (f) => `{${f.cx},${f.cy},${f.w},${f.h},${rotLit(f.rot)},${f.ref}}`;
c += `static const Furn lv_furn[] = {${fmt(furn, furnC)}};\n`;
c += `#define N_FURN ${furn.length}\n`;
c += `static const Furn lv_openings[] = {${openings.length ? fmt(openings, furnC) : '{0,0,0,0,0,-1}'}};\n`;
c += `#define N_OPENINGS ${openings.length}\n`;
c += `typedef struct { int color,n,off; } AreaP;\n`;
c += `static const AreaP lv_areas[] = {${areaOut.length ? fmt(areaOut, a => `{${a.color},${a.n},${a.off}}`) : '{0,0,0}'}};\n`;
c += `#define N_AREAS ${areaOut.length}\n`;
c += `static const float lv_apts[] = {${apts.length ? apts.join(',') : '0'}};\n`;
c += `static const char* lv_refs[] = {${refs.length ? refs.map(r => `"${r}"`).join(',') : '0'}};\n`;
c += `#define N_REFS ${refs.length}\n`;
c += `/* <<<FML_DATA */`;

const summary = `walls:${segs.length} spans, openings:${openings.length} (doors+windows), areas:${areaOut.length}, furniture:${furn.length} (skipped ${skipped} oversize), refids:${refs.length}, world:${W}x${H}px`;

if (opt.stdout) { console.log(c); console.error(summary); process.exit(0); }

// splice into the target .c between sentinels
const outPath = path.resolve(opt.out);
let src = fs.readFileSync(outPath, 'utf8');
const re = /\/\* >>>FML_DATA[\s\S]*?<<<FML_DATA \*\//;
if (!re.test(src)) { console.error(`no FML_DATA sentinels found in ${opt.out}`); process.exit(1); }
src = src.replace(re, c);
fs.writeFileSync(outPath, src);
console.log(`fml2cart: ${summary}\n  -> spliced into ${opt.out}`);
