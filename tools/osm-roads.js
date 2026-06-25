#!/usr/bin/env node
// osm-roads.js — EXPERIMENTAL: fetch real road geometry and emit the "vector features"
// JSON that a data-driven cart loads at RUNTIME (via runtime/json.h + de_data_path()).
//
// This is the road sibling of fmltools/fml2cart.js — but where fml2cart BAKES C source
// per dataset, this emits a plain .json the ONE cart (roadview) reads with --data, so you
// swap cities by swapping files, never regenerating a cart. See docs/design/external-data-carts.md.
//
// USAGE
//   node tools/osm-roads.js --bbox S,W,N,E   [--out f] [--simplify M] [--name N] [--json]
//   node tools/osm-roads.js --place "Delft"  [--out f] [--json]         (geocode → bbox via Nominatim)
//   node tools/osm-roads.js --demo           [--out f] [--json]         (synthetic, NO network — for testing)
//   node tools/osm-roads.js --convert f.json [--out f.rvb]              (pack an existing JSON → .rvb, no network)
//
//   --bbox      south,west,north,east in degrees (e.g. 52.00,4.34,52.02,4.38)
//   --place     a place name; Nominatim resolves it to a bounding box
//   --simplify  drop points closer than M metres to the last kept one (default 8; broad strokes)
//   --out       output path (default data/<slug>.rvb — the folder roadview's OPEN button reveals)
//   --json      ALSO emit a .json sibling (human-readable IR; for inspection / downstream handoff)
//   --convert   read an existing .json and (re)write its .rvb, without re-querying Overpass
//
// Default output is a packed BINARY (.rvb) the cart loads ~instantly — real cities are tens of MB
// of JSON and tokenising that text (jsmn + strtod per coordinate) is the load bottleneck. The cart
// auto-detects format by magic bytes, so it still reads .json (drag-drop accepts either).
//
// SCHEMA (the shared "vector features" IR — floorplans could emit the same shape):
//   { "source", "name",
//     "bbox":  [minx, miny, maxx, maxy],          // local metres, origin at SW corner, Y north-up
//     "features": [ { "kind": <class>, "name": "...",
//                     "sub": "...",                   // refining OSM tag (building=* type); omitted if none
//                     "pts": [x0,y0, x1,y1, ...] } ] }  // pts in the same local-metre frame
//   class ∈ line:  highway arterial road track canal coast rail
//          area:  water building green sand residential commercial industrial farm parking
//          point: tree
//   "sub" lets a downstream consumer (e.g. one that extrudes individual buildings) keep the
//   OSM building category without re-querying; the roadview cart ignores it.
//
// The cart fits bbox→screen and flips Y for the screen's downward axis. Coordinates are
// already PROJECTED (web-mercator metres) so the cart needs no geo math.

const fs = require('fs');

// ---- args ----
const argv = process.argv.slice(2);
const opt = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const has = (n) => argv.includes(n);
let OUT = opt('--out', null);                  // default: data/<slug>.rvb (computed in write)
const WANT_JSON = has('--json');               // also emit the human-readable .json sibling
const slug = (s) => String(s || 'roads').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '') || 'roads';

// kind name → numeric index for the binary format. MUST match the K_* enum order in
// tools/carts/roadview.c (kind_of there is the JSON twin). Append only; never reorder.
const KIND_IX = { highway: 0, arterial: 1, road: 2, track: 3, water: 4, canal: 5, building: 6,
                  green: 7, tree: 8, residential: 9, commercial: 10, industrial: 11, farm: 12,
                  parking: 13, sand: 14, rail: 15, coast: 16 };
const SIMPLIFY = parseFloat(opt('--simplify', '8'));
const NAME = opt('--name', null);

// ---- web mercator: lon/lat (deg) → metres ----
const R = 6378137;
function merc(lon, lat) {
  return [R * lon * Math.PI / 180,
          R * Math.log(Math.tan(Math.PI / 4 + lat * Math.PI / 360))];
}

// OSM tags → our broad classes (null = drop). Area kinds (filled): water, building,
// green, sand, and the landuse "zoning" blocks (residential/commercial/industrial/
// farm/parking). Line kinds: canal, coast, rail, and the road classes. Order matters
// (first match wins): water and building are tested first so a riverbank polygon or a
// footprint wins over any stray landuse/highway tag on the same way.
function classifyWay(t) {
  if (t.natural === 'water' || t.water || t.waterway === 'riverbank' ||
      t.landuse === 'reservoir' || t.landuse === 'basin') return 'water';   // area, filled
  if (t.building && t.building !== 'no') return 'building';                  // area, filled (footprint)
  // green ground: parks/woods/grass + cemeteries, allotments, sports, orchards
  if (t.natural === 'wood' || t.natural === 'scrub' || t.natural === 'grassland' ||
      t.landuse === 'forest' || t.landuse === 'grass' || t.landuse === 'meadow' ||
      t.landuse === 'cemetery' || t.landuse === 'allotments' || t.landuse === 'orchard' ||
      t.landuse === 'vineyard' || t.landuse === 'recreation_ground' || t.landuse === 'village_green' ||
      t.leisure === 'park' || t.leisure === 'garden' || t.leisure === 'nature_reserve' ||
      t.leisure === 'pitch' || t.leisure === 'golf_course' || t.leisure === 'recreation_ground')
    return 'green';                                                         // area, filled (park/wood)
  if (t.natural === 'beach' || t.natural === 'sand') return 'sand';         // area, filled
  // SimCity-style zoning blocks (area fills): residential=green, commercial=blue, industrial=yellow
  if (t.landuse === 'residential') return 'residential';
  if (t.landuse === 'commercial' || t.landuse === 'retail') return 'commercial';
  if (t.landuse === 'industrial' || t.landuse === 'warehouse' || t.landuse === 'port' ||
      t.man_made === 'works') return 'industrial';
  if (t.landuse === 'farmland' || t.landuse === 'farmyard' || t.landuse === 'farm') return 'farm';
  if (t.amenity === 'parking' || t.landuse === 'garages') return 'parking'; // area, filled
  if (t.natural === 'coastline') return 'coast';                            // line
  if (/^(rail|light_rail|tram|subway|narrow_gauge|monorail)$/.test(t.railway || '')) return 'rail';  // line, dashed
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
    if (w.kind !== 'building' && w.kind !== 'tree') local = simplify(local, SIMPLIFY);
    if (local.length < 1 || (local.length < 2 && w.kind !== 'tree')) continue;  // trees are single points
    const flat = [];
    for (const [x, y] of local) flat.push(round(x), round(y));
    const feat = { kind: w.kind, name: w.name || '', pts: flat };
    if (w.sub) feat.sub = w.sub;                  // refining tag (building type) for downstream consumers
    features.push(feat);
  }
  return {
    source, name,
    bbox: [0, 0, round(maxx - minx), round(maxy - miny)],
    features,
  };
}
const round = (v) => Math.round(v * 10) / 10;

// pack a doc into the binary .rvb the cart fast-loads. Layout (all multi-byte LE):
//   magic "RVB1" | int32 nfeat | int32 namelen | name bytes | float32 bbox[4]
//   per feature: int32 kind | int32 sublen | sub bytes | int32 npts | float32 pts[npts*2]
function writeBin(doc, outPath) {
  const nameBuf = Buffer.from(doc.name || '', 'utf8');
  const subBufs = doc.features.map(f => Buffer.from(f.sub || '', 'utf8'));
  let size = 4 + 4 + 4 + nameBuf.length + 16;
  doc.features.forEach((f, i) => { size += 4 + 4 + subBufs[i].length + 4 + f.pts.length * 4; });
  const buf = Buffer.alloc(size);
  let o = 0;
  o += buf.write('RVB1', o, 'latin1');
  buf.writeInt32LE(doc.features.length, o); o += 4;
  buf.writeInt32LE(nameBuf.length, o); o += 4;
  o += nameBuf.copy(buf, o);
  for (const v of doc.bbox) { buf.writeFloatLE(v, o); o += 4; }
  doc.features.forEach((f, i) => {
    buf.writeInt32LE(KIND_IX[f.kind] ?? KIND_IX.road, o); o += 4;
    buf.writeInt32LE(subBufs[i].length, o); o += 4;
    o += subBufs[i].copy(buf, o);
    buf.writeInt32LE(f.pts.length >> 1, o); o += 4;
    for (let k = 0; k < f.pts.length; k++) { buf.writeFloatLE(f.pts[k], o); o += 4; }
  });
  fs.writeFileSync(outPath, buf);
}

function write(doc) {
  fs.mkdirSync('data', { recursive: true });
  const base = (OUT || `data/${slug(doc.name)}`).replace(/\.(rvb|json)$/i, '');
  const binPath = base + '.rvb';
  writeBin(doc, binPath);
  let extra = '';
  if (WANT_JSON || (OUT && /\.json$/i.test(OUT))) { fs.writeFileSync(base + '.json', JSON.stringify(doc)); extra = `  (+ ${base}.json)`; }
  const npts = doc.features.reduce((a, f) => a + f.pts.length / 2, 0);
  const byKind = {};
  for (const f of doc.features) byKind[f.kind] = (byKind[f.kind] || 0) + 1;
  console.log(`wrote ${binPath}${extra}`);
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
  // a park (green area) in the NE, with a scatter of individual trees in it
  const park = [];
  for (let i = 0; i <= 22; i++) {
    const a = (i / 22) * Math.PI * 2;
    park.push([3300 + Math.cos(a) * 480, 1000 + Math.sin(a) * 360 + (rnd() - 0.5) * 40]);
  }
  ways.push({ kind: 'green', name: 'Stadspark', pts: park });
  for (let i = 0; i < 80; i++) {
    const tx = 3300 + (rnd() - 0.5) * 820, ty = 1000 + (rnd() - 0.5) * 620;
    if (((tx - 3300) / 480) ** 2 + ((ty - 1000) / 360) ** 2 < 0.9)
      ways.push({ kind: 'tree', name: '', pts: [[tx, ty]] });
  }
  // street trees lining the central avenue
  for (let x = 600; x < W - 400; x += 90) ways.push({ kind: 'tree', name: '', pts: [[x, H / 2 - 70]] });
  // SimCity-style zoning blocks (closed rects = areas) — drawn UNDER the buildings/roads.
  const rect = (kind, x, y, w, h, sub) =>
    ways.push({ kind, name: '', ...(sub ? { sub } : {}),
                pts: [[x, y], [x + w, y], [x + w, y + h], [x, y + h], [x, y]] });
  rect('residential', 1500, 1850, 1500, 600);   // green zone (housing) S of centre
  rect('residential',  900, 1100,  700, 700);
  rect('commercial',  2300, 1100,  900, 600);    // blue zone (shops) downtown
  rect('industrial',  3000, 2000, 1000, 800);    // yellow zone (works) SE edge
  rect('farm',          80,  200, 1000, 700);    // brown field, SW corner
  rect('farm',        3300,  100,  900, 600);    // brown field, NE corner
  rect('parking',     2250, 1720,  220, 150);    // a lot beside the commercial zone
  // a rail line cutting NW→SE across town (drawn dashed)
  ways.push({ kind: 'rail', name: 'Spoorlijn', pts: wig(20, -100, 2400, W + 100, 400, 120) });
  // a scatter of building footprints in the downtown blocks (closed rects = areas)
  const btypes = ['house', 'apartments', 'commercial', 'retail', 'industrial'];
  for (let bx = 1500; bx < 3200; bx += 240) {
    for (let by = 1100; by < 2300; by += 200) {
      if (!inDisc(bx, by) || rnd() < 0.25) continue;        // gaps = streets/yards
      const w = 70 + rnd() * 90, h = 60 + rnd() * 80;        // 6–16 m footprints
      const ox = bx + (rnd() - 0.5) * 40, oy = by + (rnd() - 0.5) * 40;
      ways.push({ kind: 'building', name: '', sub: btypes[(s >> 4) % btypes.length],
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
  const q = `[out:json][timeout:120];(` +
            `way["highway"]${bb};` +
            `way["railway"~"^(rail|light_rail|tram|subway|narrow_gauge|monorail)$"]${bb};` +
            `way["waterway"~"^(river|canal|stream|drain|ditch|riverbank)$"]${bb};` +
            `way["natural"~"^(water|coastline|beach|sand)$"]${bb};` +
            `way["building"]${bb};` +
            `way["natural"~"^(wood|scrub|grassland)$"]${bb};` +
            `way["landuse"]${bb};` +                                  // all landuse → classifyWay keeps zoning/green, drops the rest
            `way["leisure"~"^(park|garden|nature_reserve|pitch|golf_course|recreation_ground)$"]${bb};` +
            `way["amenity"="parking"]${bb};` +
            `node["natural"="tree"]${bb};` +
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
    if (el.type === 'node') {                          // individual trees = point nodes
      if (el.tags?.natural === 'tree' && el.lat != null)
        ways.push({ kind: 'tree', name: '', pts: [merc(el.lon, el.lat)] });
      continue;
    }
    if (el.type !== 'way' || !el.geometry) continue;
    const tags = el.tags || {};
    const kind = classifyWay(tags);
    if (!kind) continue;
    ways.push({ kind, name: tags.name || '',
                sub: kind === 'building' ? (tags.building !== 'yes' ? tags.building : '') : '',
                pts: el.geometry.map(g => merc(g.lon, g.lat)) });
  }
  if (!ways.length) throw new Error('no roads in that bbox');
  write(buildDoc('overpass', name || `${S},${W},${N},${E}`, ways));
}

(async () => {
  try {
    if (opt('--convert', null)) {                          // pack an existing .json → .rvb (no network)
      const inPath = opt('--convert', null);
      const doc = JSON.parse(fs.readFileSync(inPath, 'utf8'));
      const outPath = OUT || inPath.replace(/\.json$/i, '.rvb');
      writeBin(doc, outPath);
      const npts = doc.features.reduce((a, f) => a + f.pts.length / 2, 0);
      console.log(`converted ${inPath} -> ${outPath}  (${doc.features.length} ways, ${npts} points)`);
      return;
    }
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
