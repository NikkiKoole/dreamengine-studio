#!/usr/bin/env node
// osm-roads.js — EXPERIMENTAL: fetch real road geometry and emit the "vector features"
// JSON that a data-driven cart loads at RUNTIME (via runtime/json.h + de_data_path()).
//
// This is the road sibling of data-tools/fmltools/fml2cart.js — but where fml2cart BAKES C source
// per dataset, this emits a plain .json the ONE cart (roadview) reads with --data, so you
// swap cities by swapping files, never regenerating a cart. See docs/design/external-data-carts.md.
//
// USAGE
//   node data-tools/roadview/osm-roads.js --bbox S,W,N,E   [--out f] [--simplify M] [--name N] [--json]
//   node data-tools/roadview/osm-roads.js --place "Delft"  [--out f] [--json]         (geocode → bbox via Nominatim)
//   node data-tools/roadview/osm-roads.js --demo           [--out f] [--json]         (synthetic, NO network — for testing)
//   node data-tools/roadview/osm-roads.js --convert f.json [--out f.rvb]              (pack an existing JSON → .rvb, no network)
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
//   class ∈ line:  highway arterial secondary tertiary road service cycleway footway track
//                  canal coast rail other_line
//          area:  water building green sand residential commercial industrial farm parking other_area
//          point: tree
//   "sub" carries a refining tag: the OSM building category on buildings (for a downstream
//   consumer that extrudes them), the defining "key=value" on other_area/other_line —
//   which the cart HASHES into a muted colour+dither so the uncategorised long tail stays
//   visually distinct without a dedicated palette slot each — and, on LINE ROADS, a
//   bridge/tunnel code "B<layer>"/"T<layer>" (absent = at grade) so a consumer can raise
//   bridge decks / dash tunnels and grade-dispatch can route the crossing.
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
                  parking: 13, sand: 14, rail: 15, coast: 16,
                  // appended — keep in lockstep with the K_* enum in roadview.c (APPEND ONLY,
                  // never reorder: the .rvb encodes each feature's kind as this index).
                  secondary: 17, tertiary: 18, service: 19, cycleway: 20, footway: 21,
                  other_area: 22, other_line: 23 };
// line-road kinds that can be a bridge/tunnel (carried in `sub` as "B<layer>"/"T<layer>" — see below).
const BRIDGEABLE = new Set(['highway','arterial','road','track','secondary','tertiary','service','cycleway','footway','rail']);
const SIMPLIFY = parseFloat(opt('--simplify', '8'));
const NAME = opt('--name', null);
const DEM = has('--dem') ? (parseInt(opt('--dem', '96'), 10) || 96) : 0;   // GxG terrain heightfield (opt-in)

// ---- terrain elevation: sample a GxG heightfield over the bbox (opentopodata srtm30m) ----
// citydrive drapes its flat geometry over this so a hilly city (SF, Lisbon…) renders 3D relief.
// Stored compactly in the .rvb (RVB3): a grid of metres, not a z per point. Opt-in (--dem) — it's
// slow (public API is 1 call/sec) so flat-city fetches stay fast. roadview is 2D and ignores it.
// Sample over the FEATURE extent (doc-local metre frame: origin ox,oy, size Wm×Hm), inverse-
// projecting each grid point to lon/lat. This makes the grid map 1:1 onto doc.bbox in the cart,
// so it stays aligned even when the OSM feature bbox balloons past the query area.
async function fetchDEM(ox, oy, Wm, Hm, G) {
  console.log(`fetching ${G}x${G} terrain grid over the feature extent (opentopodata srtm30m)…`);
  const pts = [];
  for (let i = 0; i < G; i++) for (let j = 0; j < G; j++) {        // row i = south→north, col j = west→east
    const [lon, lat] = invmerc(ox + (Wm) * j / (G - 1), oy + (Hm) * i / (G - 1));
    pts.push([lat, lon]);
  }
  const z = new Array(G * G).fill(0);
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  for (let b = 0; b < pts.length; b += 100) {
    const batch = pts.slice(b, b + 100);
    const locs = batch.map(([la, lo]) => `${la.toFixed(6)},${lo.toFixed(6)}`).join('|');
    let ok = false;
    for (let attempt = 0; attempt < 4 && !ok; attempt++) {
      try {
        const r = await fetch(`https://api.opentopodata.org/v1/srtm30m?locations=${locs}`);
        if (!r.ok) { await sleep(2000); continue; }
        const jj = await r.json();
        if (jj.status !== 'OK' || !jj.results) { await sleep(2000); continue; }
        jj.results.forEach((res, k) => { z[b + k] = res.elevation == null ? 0 : res.elevation; });
        ok = true;
      } catch (e) { await sleep(2000); }
    }
    if (!ok) throw new Error('opentopodata fetch failed');
    process.stdout.write(`\r  terrain ${Math.min(b + 100, pts.length)}/${pts.length}`);
    await sleep(1100);                                              // public API rate limit: ~1 call/sec
  }
  process.stdout.write('\n');
  const min = Math.min(...z), max = Math.max(...z);
  console.log(`  relief ${(max - min).toFixed(0)} m (${min.toFixed(0)}–${max.toFixed(0)} m)`);
  return { g: G, z: z.map((v) => v - min) };                       // relative metres (lowest point = 0)
}

// a synthetic smooth-hills heightfield for the offline --demo (so the demo shows terrain too).
function demoHeightfield(Wm, Hm, G) {
  const z = [];
  for (let i = 0; i < G; i++) for (let j = 0; j < G; j++) {
    const x = j / (G - 1) * Wm, y = i / (G - 1) * Hm;
    const h1 = 42 * Math.exp(-(((x - 0.36 * Wm) ** 2 + (y - 0.55 * Hm) ** 2) / (2 * 700 ** 2)));
    const h2 = 30 * Math.exp(-(((x - 0.68 * Wm) ** 2 + (y - 0.40 * Hm) ** 2) / (2 * 520 ** 2)));
    z.push(h1 + h2);
  }
  return { g: G, z };
}

// ---- web mercator: lon/lat (deg) → metres (and back) ----
const R = 6378137;
function merc(lon, lat) {
  return [R * lon * Math.PI / 180,
          R * Math.log(Math.tan(Math.PI / 4 + lat * Math.PI / 360))];
}
function invmerc(x, y) {                          // metres → lon/lat (deg)
  return [x / R * 180 / Math.PI,
          (2 * Math.atan(Math.exp(y / R)) - Math.PI / 2) * 180 / Math.PI];
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
  // the FULL road ladder — every density gets its own painted class (not collapsed): big
  // motorways down to bike paths and footpaths. Order = importance (first match wins).
  const hw = t.highway || '';
  if (/^(motorway|trunk)(_link)?$/.test(hw))        return 'highway';
  if (/^primary(_link)?$/.test(hw))                 return 'arterial';
  if (/^secondary(_link)?$/.test(hw))               return 'secondary';
  if (/^tertiary(_link)?$/.test(hw))                return 'tertiary';
  if (/^(residential|unclassified|living_street|road)$/.test(hw)) return 'road';   // ordinary streets
  if (hw === 'service')                             return 'service';
  if (hw === 'cycleway')                            return 'cycleway';              // bike path
  if (/^(footway|path|steps|bridleway|pedestrian|corridor)$/.test(hw)) return 'footway';
  if (hw === 'track')                               return 'track';
  return null;   // everything else → caller maps to other_area / other_line (the hashed understory)
}

// the single most salient key=value of an UNCATEGORISED way — what the cart hashes into a
// muted textured fill so the long tail of OSM categories stays distinct without a dedicated
// colour each. Priority picks the tag that best describes the patch of land.
function definingTag(t) {
  const keys = ['aeroway', 'landuse', 'leisure', 'amenity', 'man_made', 'natural',
                'tourism', 'historic', 'military', 'power', 'barrier', 'waterway', 'railway'];
  for (const k of keys) if (t[k]) return `${k}=${t[k]}`;
  return 'other';
}

// OSM surface → a single render code: a=smooth (asphalt/concrete), p=brick/klinker (paving_stones/
// sett/cobblestone — most of a Dutch centre), g=unpaved (gravel/ground/sand), w=wood, o=other.
function surfaceCode(s) {
  if (!s) return '';
  if (/^(asphalt|concrete|concrete:plates|paved|chipseal|metal|tartan|acrylic)$/.test(s)) return 'a';
  if (/(paving_stones|sett|cobblestone|unhewn_cobblestone|bricks?|paving_stone)/.test(s)) return 'p';
  if (/(gravel|fine_gravel|compacted|ground|dirt|earth|sand|grass|mud|pebblestone|rock|clay)/.test(s)) return 'g';
  if (/wood/.test(s)) return 'w';
  return 'o';
}
// Pack a road's useful OSM attributes into the compact `sub` token string — ';'-delimited, each token's
// FIRST char is its key. The bridge/tunnel token stays first for backward-compat (parse_deck reads sub[0]).
// Tokens: B<L>/T<L> bridge/tunnel · O oneway · L<n> lanes · W b|l|r|n sidewalk · U a|p|g|w|o surface ·
// M<n> maxspeed(km/h) · X<n> width(m) · C b|l|r on-road cycleway · R roundabout. Consumers read only what they use.
function roadSub(tags, bridgeTok) {
  const t = []; if (bridgeTok) t.push(bridgeTok);
  if (tags.oneway === 'yes' || tags.oneway === '-1') t.push('O');
  const ln = parseInt(tags.lanes, 10); if (Number.isFinite(ln) && ln > 0 && ln <= 12) t.push('L' + ln);
  const sw = tags.sidewalk || tags['sidewalk:both'];
  if (sw === 'both') t.push('Wb'); else if (sw === 'left') t.push('Wl');
  else if (sw === 'right') t.push('Wr'); else if (sw === 'no' || sw === 'none') t.push('Wn');
  const u = surfaceCode(tags.surface); if (u) t.push('U' + u);
  const ms = parseInt(tags.maxspeed, 10); if (Number.isFinite(ms) && ms > 0 && ms <= 130) t.push('M' + ms);
  const wd = parseFloat(tags.width); if (Number.isFinite(wd) && wd > 0 && wd <= 60) t.push('X' + Math.round(wd));  // carriageway width (m), authoritative but rare
  const lane = (v) => v && v !== 'no' && v !== 'separate' && v !== 'none';           // an on-road bike lane?
  const cwL = lane(tags['cycleway:left']), cwR = lane(tags['cycleway:right']);
  if (lane(tags.cycleway) || lane(tags['cycleway:both']) || (cwL && cwR)) t.push('Cb');
  else if (cwL) t.push('Cl'); else if (cwR) t.push('Cr');
  if (tags.junction === 'roundabout' || tags.junction === 'circular') t.push('R');
  return t.join(';');
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

// Assemble multipolygon member ways into closed rings. OSM members arrive unordered and in
// either direction, so we greedily join segments that share an endpoint (matched on raw
// lon/lat — shared OSM nodes have identical coords) until each ring closes. Returns an array
// of point-rings. (Stage 1: callers pass only the `outer` members; inner-ring holes TODO.)
function assembleRings(segments) {
  const eq = (a, b) => a[0] === b[0] && a[1] === b[1];
  const segs = segments.filter(s => s.length >= 2).map(s => s.slice());
  const rings = [];
  while (segs.length) {
    let ring = segs.shift();
    let joined = true;
    while (joined && !eq(ring[0], ring[ring.length - 1])) {
      joined = false;
      for (let i = 0; i < segs.length; i++) {
        const s = segs[i], head = ring[0], tail = ring[ring.length - 1];
        if      (eq(tail, s[0]))              ring = ring.concat(s.slice(1));
        else if (eq(tail, s[s.length - 1]))   ring = ring.concat(s.slice(0, -1).reverse());
        else if (eq(head, s[s.length - 1]))   ring = s.slice(0, -1).concat(ring);
        else if (eq(head, s[0]))              ring = s.slice(1).reverse().concat(ring);
        else continue;
        segs.splice(i, 1); joined = true; break;
      }
    }
    rings.push(ring);                        // closed, or best-effort if the data has gaps
  }
  return rings;
}

// build the document from an array of {kind, name, pts:[[x,y],...]} in raw metres
// a building's height in METRES, cheapest source first. OSM `height` (already
// metres) wins; else `building:levels` × ~3 m/storey; else 0 = unknown (the cart
// applies its own fallback). Coverage varies wildly by region — many buildings
// carry neither tag. (3DBAG would give authoritative NL heights — a later tool.)
function buildingHeight(tags) {
  const h = parseFloat(tags.height);                       // e.g. "12.5" or "12 m"
  if (isFinite(h) && h > 0) return h;
  const lv = parseFloat(tags['building:levels']);
  if (isFinite(lv) && lv > 0) return lv * 3;
  return 0;
}

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
    if (w.h) feat.h = w.h;                         // building height in metres (citydrive extrudes it; roadview ignores)
    features.push(feat);
  }
  return {
    source, name,
    bbox: [0, 0, round(maxx - minx), round(maxy - miny)],
    ox: minx, oy: miny,                          // mercator-metre origin of the local frame (for DEM alignment)
    features,
  };
}
const round = (v) => Math.round(v * 10) / 10;

// pack a doc into the binary .rvb the cart fast-loads. Layout (all multi-byte LE):
//   magic "RVB2" | int32 nfeat | int32 namelen | name bytes | float32 bbox[4]
//   per feature: int32 kind | float32 h | int32 sublen | sub bytes | int32 npts | float32 pts[npts*2]
// h = building height in metres (0 = unknown/non-building). RVB1 (no h) still loads in the
// cart — the reader switches layout on the magic's version byte.
function writeBin(doc, outPath) {
  const nameBuf = Buffer.from(doc.name || '', 'utf8');
  const subBufs = doc.features.map(f => Buffer.from(f.sub || '', 'utf8'));
  const hf = doc.heightfield;                                  // {g, z:[g*g]} or undefined
  let size = 4 + 4 + 4 + nameBuf.length + 16;
  doc.features.forEach((f, i) => { size += 4 + 4 + 4 + subBufs[i].length + 4 + f.pts.length * 4; });  // +4: float h
  if (hf) size += 4 + hf.g * hf.g * 4;                         // trailing: int32 G + float32 grid[G*G]
  const buf = Buffer.alloc(size);
  let o = 0;
  o += buf.write(hf ? 'RVB3' : 'RVB2', o, 'latin1');
  buf.writeInt32LE(doc.features.length, o); o += 4;
  buf.writeInt32LE(nameBuf.length, o); o += 4;
  o += nameBuf.copy(buf, o);
  for (const v of doc.bbox) { buf.writeFloatLE(v, o); o += 4; }
  doc.features.forEach((f, i) => {
    buf.writeInt32LE(KIND_IX[f.kind] ?? KIND_IX.road, o); o += 4;
    buf.writeFloatLE(f.h || 0, o); o += 4;                  // building height in metres (0 = unknown)
    buf.writeInt32LE(subBufs[i].length, o); o += 4;
    o += subBufs[i].copy(buf, o);
    buf.writeInt32LE(f.pts.length >> 1, o); o += 4;
    for (let k = 0; k < f.pts.length; k++) { buf.writeFloatLE(f.pts[k], o); o += 4; }
  });
  if (hf) { buf.writeInt32LE(hf.g, o); o += 4; for (const v of hf.z) { buf.writeFloatLE(v, o); o += 4; } }
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
  // the rest of the road ladder: secondary + tertiary feeders, service alleys, a red bike
  // path and a footpath — so the demo exercises every painted road class offline.
  ways.push({ kind: 'secondary', name: 'Stationsweg', pts: wig(16, 400, 1250, W - 400, 1450, 100) });
  ways.push({ kind: 'tertiary',  name: 'Kerkstraat',  pts: wig(14, 1250, 650, 1650, H - 500, 90) });
  ways.push({ kind: 'service',   name: '', pts: wig(8, 2300, 1150, 2300, 1700, 18) });
  ways.push({ kind: 'service',   name: '', pts: wig(8, 2500, 1180, 2950, 1180, 18) });
  ways.push({ kind: 'cycleway',  name: 'Fietspad', pts: wig(20, 200, 1550, W - 200, 1750, 120) });
  ways.push({ kind: 'footway',   name: '', pts: wig(16, 3300, 700, 3300, 1340, 50) });
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
  // uncategorised land → the muted hashed understory (a quarry, school grounds, a stadium),
  // plus an "other" line (a pier) — each gets a stable colour+dither from its defining tag.
  rect('other_area',   200, 2200,  700, 600, 'landuse=quarry');
  rect('other_area',  2500,  280,  640, 520, 'amenity=school');
  rect('other_area',  3450, 2050,  520, 520, 'leisure=stadium');
  ways.push({ kind: 'other_line', name: '', sub: 'man_made=pier', pts: wig(6, 980, 720, 1500, 700, 12) });
  // a rail line cutting NW→SE across town (drawn dashed)
  ways.push({ kind: 'rail', name: 'Spoorlijn', pts: wig(20, -100, 2400, W + 100, 400, 120) });
  // a scatter of building footprints in the downtown blocks (closed rects = areas)
  const btypes = ['house', 'apartments', 'commercial', 'retail', 'industrial'];
  for (let bx = 1400; bx < 3300; bx += 64) {                  // a DENSE grid of house/block-sized lots
    for (let by = 1000; by < 2400; by += 58) {                // (small enough to read as a town in citydrive's 3D)
      if (!inDisc(bx, by) || rnd() < 0.30) continue;          // gaps = streets/yards
      const w = 16 + rnd() * 26, h = 16 + rnd() * 26;         // ~16–42 m footprints
      const ox = bx + (rnd() - 0.5) * 16, oy = by + (rnd() - 0.5) * 16;
      ways.push({ kind: 'building', name: '', sub: btypes[(s >> 4) % btypes.length],
                  h: 6 + rnd() * 30,                          // synthetic storey heights so demo shows 3D
                  pts: [[ox, oy], [ox + w, oy], [ox + w, oy + h], [ox, oy + h], [ox, oy]] });
    }
  }
  const doc = buildDoc('demo', NAME || 'demo', ways);              // → data/demo.rvb (the cart's default file)
  doc.heightfield = demoHeightfield(doc.bbox[2], doc.bbox[3], 64); // synthetic hills so the demo shows terrain too
  write(doc);
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
            `way["building"]${bb};` +
            // the area families — fetched WHOLESALE now: classifyWay keeps the curated classes
            // (water/green/sand/zoning/parking) and routes everything else to the hashed "other"
            // understory, so nothing is silently dropped.
            `way["natural"]${bb};` +              // water/coast/beach/sand/wood/scrub/grassland curated; rest → other
            `way["landuse"]${bb};` +              // zoning/green/farm curated; quarry/military/religious/… → other
            `way["leisure"]${bb};` +              // park/garden/pitch/… curated; stadium/marina/track/… → other
            `way["amenity"]${bb};` +              // parking curated; school/hospital/university/… → other
            `way["man_made"]${bb};` +             // works → industrial; pier/tower/bridge/… → other
            `way["aeroway"]${bb};` +              // runways/taxiways/aprons → other
            // multipolygon RELATIONS — big lakes/rivers/forests/parks are modelled as relations
            // whose member ways are just boundary arcs; we assemble their outer rings below so
            // they fill as proper areas (a thin way alone renders as a hollow sliver).
            `relation["natural"]${bb};` +
            `relation["landuse"]${bb};` +
            `relation["leisure"]${bb};` +
            `relation["waterway"="riverbank"]${bb};` +
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
  const els = j.elements || [];
  // multipolygon relations carry the area tag; their member ways are just boundary arcs (often
  // untagged). Collect member way-ids so we skip them as standalone ways (a lone arc fills as a
  // hollow sliver — the "grey middle" bug), then assemble the relations into proper rings below.
  const rels = els.filter(e => e.type === 'relation' && (e.tags || {}).type === 'multipolygon');
  const memberWayIds = new Set();
  for (const r of rels) for (const m of r.members || []) if (m.type === 'way') memberWayIds.add(m.ref);

  const ways = [];
  for (const el of els) {
    if (el.type === 'node') {                          // individual trees = point nodes
      if (el.tags?.natural === 'tree' && el.lat != null)
        ways.push({ kind: 'tree', name: '', pts: [merc(el.lon, el.lat)] });
      continue;
    }
    if (el.type !== 'way' || !el.geometry) continue;
    if (memberWayIds.has(el.id)) continue;             // drawn as part of its relation instead
    const tags = el.tags || {};
    const g = el.geometry;
    let kind = classifyWay(tags);
    let sub = kind === 'building' ? (tags.building !== 'yes' ? tags.building : '') : '';
    // Line roads carry their useful OSM attributes in `sub` as a ';'-delimited token string (roadSub):
    // bridge/tunnel (deck), surface, sidewalk, oneway, lanes, maxspeed, on-road cycleway, roundabout.
    // Backward-compatible: the bridge/tunnel token stays FIRST so parse_deck (reads sub[0]) is unchanged,
    // and any reader that ignores `sub` still loads. Bridge code: "B<layer>"/"T<layer>"; absent = at grade.
    if (BRIDGEABLE.has(kind)) {
      const br = tags.bridge && tags.bridge !== 'no';
      const tu = !br && tags.tunnel && tags.tunnel !== 'no';
      let bridgeTok = '';
      if (br || tu) { const L = parseInt(tags.layer, 10); bridgeTok = (br ? 'B' : 'T') + (Number.isFinite(L) ? L : (br ? 1 : -1)); }
      sub = roadSub(tags, bridgeTok);
    }
    const h = kind === 'building' ? buildingHeight(tags) : 0;   // metres, for the pseudo-3D extrude (citydrive)
    if (!kind) {                                       // uncategorised → the hashed "other" understory
      const closed = g.length > 3 && g[0].lon === g[g.length - 1].lon && g[0].lat === g[g.length - 1].lat;
      kind = closed ? 'other_area' : 'other_line';     // closed ring fills, open way strokes
      sub = definingTag(tags);                          // cart hashes this → colour + dither pattern
    }
    ways.push({ kind, name: tags.name || '', sub, h, pts: g.map(p => merc(p.lon, p.lat)) });
  }

  // assemble each multipolygon's OUTER rings into filled area features (Stage 1: outer only —
  // inner-ring holes / islands are a later refinement). This is what fills big lakes/rivers.
  let relRings = 0;
  for (const r of rels) {
    const tags = r.tags || {};
    let kind = classifyWay(tags), sub = '';
    if (!kind) { kind = 'other_area'; sub = definingTag(tags); }   // a multipolygon is always an area
    const outers = (r.members || [])
      .filter(m => m.type === 'way' && (m.role === 'outer' || m.role === '') && Array.isArray(m.geometry))
      .map(m => m.geometry.map(p => [p.lon, p.lat]));
    if (!outers.length) continue;
    for (const ring of assembleRings(outers)) {
      if (ring.length < 4) continue;
      ways.push({ kind, name: tags.name || '', sub, pts: ring.map(([lon, lat]) => merc(lon, lat)) });
      relRings++;
    }
  }
  if (relRings) console.log(`  assembled ${relRings} ring(s) from ${rels.length} multipolygon relation(s)`);
  if (!ways.length) throw new Error('no roads in that bbox');
  const doc = buildDoc('overpass', name || `${S},${W},${N},${E}`, ways);
  if (DEM) { try { doc.heightfield = await fetchDEM(doc.ox, doc.oy, doc.bbox[2], doc.bbox[3], DEM); }
             catch (e) { console.log(`  terrain skipped (${e.message}) — flat`); } }
  write(doc);
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
