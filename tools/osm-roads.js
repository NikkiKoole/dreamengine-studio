#!/usr/bin/env node
// osm-roads.js — EXPERIMENTAL: fetch real road geometry and emit the "vector features"
// JSON that a data-driven cart loads at RUNTIME (via runtime/json.h + de_data_path()).
//
// This is the road sibling of fmltools/fml2cart.js — but where fml2cart BAKES C source
// per dataset, this emits a plain .json the ONE cart (roadview) reads with --data, so you
// swap cities by swapping files, never regenerating a cart. See docs/design/external-data-carts.md.
//
// USAGE
//   node tools/osm-roads.js --bbox S,W,N,E   [--out f.json] [--simplify M] [--name N]
//   node tools/osm-roads.js --place "Delft"  [--out f.json]            (geocode → bbox via Nominatim)
//   node tools/osm-roads.js --demo           [--out f.json]            (synthetic, NO network — for testing)
//
//   --bbox      south,west,north,east in degrees (e.g. 52.00,4.34,52.02,4.38)
//   --place     a place name; Nominatim resolves it to a bounding box
//   --simplify  drop points closer than M metres to the last kept one (default 8; broad strokes)
//   --out       output path (default data/<slug>.json — the folder roadview's OPEN button reveals)
//
// SCHEMA (the shared "vector features" IR — floorplans could emit the same shape):
//   { "source", "name",
//     "bbox":  [minx, miny, maxx, maxy],          // local metres, origin at SW corner, Y north-up
//     "features": [ { "kind": "highway"|"arterial"|"road"|"track",
//                     "name": "...", "pts": [x0,y0, x1,y1, ...] } ] }   // pts in the same local-metre frame
//
// The cart fits bbox→screen and flips Y for the screen's downward axis. Coordinates are
// already PROJECTED (web-mercator metres) so the cart needs no geo math.

const fs = require('fs');

// ---- args ----
const argv = process.argv.slice(2);
const opt = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const has = (n) => argv.includes(n);
let OUT = opt('--out', null);                  // default: data/<slug>.json (computed in write)
const slug = (s) => String(s || 'roads').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '') || 'roads';
const SIMPLIFY = parseFloat(opt('--simplify', '8'));
const NAME = opt('--name', null);

// ---- web mercator: lon/lat (deg) → metres ----
const R = 6378137;
function merc(lon, lat) {
  return [R * lon * Math.PI / 180,
          R * Math.log(Math.tan(Math.PI / 4 + lat * Math.PI / 360))];
}

// OSM tags → our broad classes (null = drop). "water" = a filled area; everything
// else is a polyline. Water is checked first so a riverbank polygon wins over any
// stray highway tag on the same way.
function classifyWay(t) {
  if (t.natural === 'water' || t.water || t.waterway === 'riverbank' ||
      t.landuse === 'reservoir' || t.landuse === 'basin') return 'water';   // area, filled
  if (t.building && t.building !== 'no') return 'building';                  // area, filled (footprint)
  if (/^(river|canal|stream|drain|ditch)$/.test(t.waterway || '')) return 'canal';  // line
  const hw = t.highway || '';
  if (/^(motorway|trunk)(_link)?$/.test(hw)) return 'highway';
  if (/^(primary|secondary)(_link)?$/.test(hw)) return 'arterial';
  if (/^(tertiary|unclassified|residential|living_street|service|road)(_link)?$/.test(hw)) return 'road';
  if (/^(track|path|footway|cycleway|bridleway|steps|pedestrian)$/.test(hw)) return 'track';
  return null;
}

// radial-distance decimation: keep a point only if it's > eps metres from the last kept.
function simplify(pts, eps) {
  if (pts.length <= 2 || eps <= 0) return pts;
  const out = [pts[0]];
  let [lx, ly] = pts[0];
  for (let i = 1; i < pts.length - 1; i++) {
    const [x, y] = pts[i];
    if (Math.hypot(x - lx, y - ly) >= eps) { out.push(pts[i]); lx = x; ly = y; }
  }
  out.push(pts[pts.length - 1]);
  return out;
}

// build the document from an array of {kind, name, pts:[[x,y],...]} in raw metres
function buildDoc(source, name, ways) {
  let minx = Infinity, miny = Infinity, maxx = -Infinity, maxy = -Infinity;
  for (const w of ways) for (const [x, y] of w.pts) {
    if (x < minx) minx = x; if (y < miny) miny = y;
    if (x > maxx) maxx = x; if (y > maxy) maxy = y;
  }
  const features = [];
  for (const w of ways) {
    let local = w.pts.map(([x, y]) => [x - minx, y - miny]);
    if (w.kind !== 'building') local = simplify(local, SIMPLIFY);  // keep small footprints intact
    if (local.length < 2) continue;
    const flat = [];
    for (const [x, y] of local) flat.push(round(x), round(y));
    features.push({ kind: w.kind, name: w.name || '', pts: flat });
  }
  return {
    source, name,
    bbox: [0, 0, round(maxx - minx), round(maxy - miny)],
    features,
  };
}
const round = (v) => Math.round(v * 10) / 10;

function write(doc) {
  if (!OUT) { fs.mkdirSync('data', { recursive: true }); OUT = `data/${slug(doc.name)}.json`; }
  fs.writeFileSync(OUT, JSON.stringify(doc));
  const npts = doc.features.reduce((a, f) => a + f.pts.length / 2, 0);
  const byKind = {};
  for (const f of doc.features) byKind[f.kind] = (byKind[f.kind] || 0) + 1;
  console.log(`wrote ${OUT}`);
  console.log(`  ${doc.features.length} ways, ${npts} points, bbox ${doc.bbox.join(',')} m`);
  console.log(`  classes: ${Object.entries(byKind).map(([k, n]) => `${k}=${n}`).join('  ')}`);
}

// ---- DEMO: a synthetic town, no network (deterministic) ----
function demo() {
  let s = 1234567;                       // tiny LCG so the layout is repeatable
  const rnd = () => (s = (s * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff;
  const W = 4200, H = 3000;
  const ways = [];
  const wig = (n, x0, y0, x1, y1, amp) => {       // a gently-wiggled line from a→b, n segments
    const pts = [];
    for (let i = 0; i <= n; i++) {
      const t = i / n;
      const x = x0 + (x1 - x0) * t, y = y0 + (y1 - y0) * t;
      const w = Math.sin(t * Math.PI);            // 0 at ends, 1 mid
      pts.push([x + (rnd() - 0.5) * amp * w, y + (rnd() - 0.5) * amp * w]);
    }
    return pts;
  };
  // two highways crossing the town diagonally
  ways.push({ kind: 'highway', name: 'N-North', pts: wig(24, -200, 600, W + 200, 2200, 260) });
  ways.push({ kind: 'highway', name: 'N-West',  pts: wig(24, 700, -200, 2600, H + 200, 260) });
  // a ring arterial
  const ring = [];
  for (let i = 0; i <= 40; i++) {
    const a = (i / 40) * Math.PI * 2;
    ring.push([W / 2 + Math.cos(a) * 1500 + (rnd() - 0.5) * 80,
               H / 2 + Math.sin(a) * 1000 + (rnd() - 0.5) * 80]);
  }
  ways.push({ kind: 'arterial', name: 'Ring', pts: ring });
  // two cross avenues
  ways.push({ kind: 'arterial', name: 'Centraal',  pts: wig(16, 400, H / 2, W - 400, H / 2, 120) });
  ways.push({ kind: 'arterial', name: 'Midden',    pts: wig(16, W / 2, 400, W / 2, H - 400, 120) });
  // a residential grid inside the ring (clip roughly to the disc)
  const inDisc = (x, y) => ((x - W / 2) / 1500) ** 2 + ((y - H / 2) / 1000) ** 2 < 0.92;
  for (let gx = 700; gx < W - 500; gx += 360) {
    const seg = [];
    for (let y = 300; y < H - 300; y += 40) if (inDisc(gx, y)) seg.push([gx + (rnd() - 0.5) * 30, y]);
    if (seg.length > 2) ways.push({ kind: 'road', name: '', pts: seg });
  }
  for (let gy = 500; gy < H - 400; gy += 320) {
    const seg = [];
    for (let x = 400; x < W - 300; x += 40) if (inDisc(x, gy)) seg.push([x, gy + (rnd() - 0.5) * 30]);
    if (seg.length > 2) ways.push({ kind: 'road', name: '', pts: seg });
  }
  // a couple of tracks wandering off into the countryside
  ways.push({ kind: 'track', name: '', pts: wig(18, 300, 2600, -100, 2900, 200) });
  ways.push({ kind: 'track', name: '', pts: wig(18, W - 300, 500, W + 200, 200, 200) });
  // a canal cutting across town, and a lake in the south-west (closed ring = area)
  ways.push({ kind: 'canal', name: 'Singel', pts: wig(22, -100, 1100, W + 100, 1900, 200) });
  const lake = [];
  for (let i = 0; i <= 26; i++) {
    const a = (i / 26) * Math.PI * 2;
    lake.push([1050 + Math.cos(a) * 520, 720 + Math.sin(a) * 360 + (rnd() - 0.5) * 40]);
  }
  ways.push({ kind: 'water', name: 'Vest', pts: lake });
  // a scatter of building footprints in the downtown blocks (closed rects = areas)
  for (let bx = 1500; bx < 3200; bx += 240) {
    for (let by = 1100; by < 2300; by += 200) {
      if (!inDisc(bx, by) || rnd() < 0.25) continue;        // gaps = streets/yards
      const w = 70 + rnd() * 90, h = 60 + rnd() * 80;        // 6–16 m footprints
      const ox = bx + (rnd() - 0.5) * 40, oy = by + (rnd() - 0.5) * 40;
      ways.push({ kind: 'building', name: '',
                  pts: [[ox, oy], [ox + w, oy], [ox + w, oy + h], [ox, oy + h], [ox, oy]] });
    }
  }
  write(buildDoc('demo', NAME || 'demo', ways));     // → data/demo.json (the cart's default file)
}

// ---- Nominatim geocode a place name → [S,W,N,E] ----
async function geocode(place) {
  const url = `https://nominatim.openstreetmap.org/search?format=json&limit=1&q=${encodeURIComponent(place)}`;
  const r = await fetch(url, { headers: { 'User-Agent': 'dreamengine-osm-roads/0.1 (experiment)' } });
  const j = await r.json();
  if (!j.length) throw new Error(`no place found for "${place}"`);
  const bb = j[0].boundingbox.map(Number);   // [S, N, W, E]
  return [bb[0], bb[2], bb[1], bb[3]];        // → S,W,N,E
}

// ---- Overpass fetch for a bbox ----
async function overpass(S, W, N, E, name) {
  const bb = `(${S},${W},${N},${E})`;
  const q = `[out:json][timeout:90];(` +
            `way["highway"]${bb};` +
            `way["waterway"~"^(river|canal|stream|drain|ditch|riverbank)$"]${bb};` +
            `way["natural"="water"]${bb};` +
            `way["landuse"~"^(reservoir|basin)$"]${bb};` +
            `way["building"]${bb};` +
            `);out geom;`;
  console.log(`querying Overpass for bbox ${S},${W},${N},${E} …`);
  // the public instances 504/429 under load — try the mirrors in turn
  const ENDPOINTS = [
    'https://overpass-api.de/api/interpreter',
    'https://overpass.kumi.systems/api/interpreter',
    'https://maps.mail.ru/osm/tools/overpass/api/interpreter',
  ];
  let j = null, lastErr = '';
  for (const ep of ENDPOINTS) {
    try {
      const r = await fetch(ep, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded',
                   'User-Agent': 'dreamengine-osm-roads/0.1 (experiment)' },
        body: 'data=' + encodeURIComponent(q),
      });
      if (!r.ok) { lastErr = `HTTP ${r.status}`; console.log(`  ${ep} → ${lastErr}, trying next…`); continue; }
      j = await r.json();
      break;
    } catch (e) { lastErr = e.message; console.log(`  ${ep} → ${lastErr}, trying next…`); }
  }
  if (!j) throw new Error(`all Overpass mirrors failed (${lastErr})`);
  const ways = [];
  for (const el of j.elements || []) {
    if (el.type !== 'way' || !el.geometry) continue;
    const kind = classifyWay(el.tags || {});
    if (!kind) continue;
    ways.push({ kind, name: el.tags?.name || '',
                pts: el.geometry.map(g => merc(g.lon, g.lat)) });
  }
  if (!ways.length) throw new Error('no roads in that bbox');
  write(buildDoc('overpass', name || `${S},${W},${N},${E}`, ways));
}

(async () => {
  try {
    if (has('--demo')) { demo(); return; }
    if (opt('--place', null)) {
      const place = opt('--place', null);
      const [S, W, N, E] = await geocode(place);
      await overpass(S, W, N, E, NAME || place);
      return;
    }
    const bbox = opt('--bbox', null);
    if (bbox) {
      const [S, W, N, E] = bbox.split(',').map(Number);
      if ([S, W, N, E].some(Number.isNaN)) throw new Error('bad --bbox; want S,W,N,E');
      await overpass(S, W, N, E, NAME);
      return;
    }
    console.error('need one of --demo | --bbox S,W,N,E | --place "name". See the header.');
    process.exit(1);
  } catch (e) {
    console.error('error:', e.message);
    process.exit(1);
  }
})();
