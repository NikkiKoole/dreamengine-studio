#!/usr/bin/env node
// osm-junction.js — extract REAL intersections from a .rvb and print each one's incident-way
// bearings + classes. The rung-2 bridge of docs/design/driving-world-program.md: turn an OSM
// road graph into the (legs) input streetlab's junction renderer eats. Reusable — the same
// node→bearings algorithm the in-cart loader will use later (rung 2b).
//
//   node data-tools/roadview/osm-junction.js data/delft-centre.rvb [--arms N] [--top K] [--quant M]
//     --arms N : only print nodes with exactly N incident arms (default: any with >=3)
//     --top  K : print the K best (most-incident) nodes (default 5)
//     --quant M: node-merge grid in metres (shared OSM nodes share coords; default 1.0)
//
// RVB2 layout (data-tools/roadview/osm-roads.js): magic "RVB2"|int32 nfeat|int32 namelen|name|
//   float32 bbox[4]| per feature: int32 kind|float32 h|int32 sublen|sub|int32 npts|float32 pts[npts*2]
// RVB3 = RVB2 + a heightfield trailer we don't need here.
const fs = require('fs');

const args = process.argv.slice(2);
const file = args.find(a => !a.startsWith('--'));
const opt  = (k, d) => { const i = args.indexOf(k); return i >= 0 ? args[i + 1] : d; };
if (!file) { console.error('usage: osm-junction.js <file.rvb> [--arms N] [--top K] [--quant M]'); process.exit(1); }

const ROAD_KINDS = new Set([0, 1, 2, 3, 17, 18, 19]); // highway arterial road track secondary tertiary service
const KIND_NAME  = { 0:'highway', 1:'arterial', 2:'road', 3:'track', 17:'secondary', 18:'tertiary', 19:'service' };
const ARMS  = parseInt(opt('--arms', '0'), 10);   // 0 = any
const TOP   = parseInt(opt('--top', '5'), 10);
const QUANT = parseFloat(opt('--quant', '1.0'));

// ---- parse the .rvb ----
const buf = fs.readFileSync(file);
const magic = buf.toString('latin1', 0, 4);
if (magic.slice(0,3) !== 'RVB' || magic[3] < '1' || magic[3] > '3') { console.error(`not an .rvb (magic="${magic}")`); process.exit(1); }
const hasHeight = magic[3] >= '2';   // RVB2+ adds a float32 height per feature (RVB1 has none)
let o = 4;
const nfeat = buf.readInt32LE(o); o += 4;
const namelen = buf.readInt32LE(o); o += 4;
const name = buf.toString('utf8', o, o + namelen); o += namelen;
o += 16; // skip bbox[4]
const roads = [];
for (let f = 0; f < nfeat; f++) {
  const kind = buf.readInt32LE(o); o += 4;
  if (hasHeight) o += 4;                     // float h (RVB2+ only)
  const sublen = buf.readInt32LE(o); o += 4; o += sublen;
  const npts = buf.readInt32LE(o); o += 4;
  const pts = new Float32Array(npts * 2);
  for (let k = 0; k < npts * 2; k++) { pts[k] = buf.readFloatLE(o); o += 4; }
  if (ROAD_KINDS.has(kind) && npts >= 2) roads.push({ kind, pts, npts });
}

// ---- build the node graph: quantize every vertex; a node = a shared coord. ----
// Each vertex of a road contributes incident direction(s) toward its along-way neighbour(s).
const key = (x, y) => `${Math.round(x / QUANT)},${Math.round(y / QUANT)}`;
const nodes = new Map();  // key -> { x, y, arms:[{brg, kind}] }
const push = (x, y, nx, ny, kind) => {
  const dx = nx - x, dy = ny - y;
  if (dx === 0 && dy === 0) return;
  const brg = (Math.atan2(dy, dx) * 180 / Math.PI + 360) % 360;  // raw bearing; only RELATIVE angles matter
  const k = key(x, y);
  let n = nodes.get(k);
  if (!n) { n = { x, y, arms: [] }; nodes.set(k, n); }
  n.arms.push({ brg, kind });
};
for (const w of roads) {
  const p = w.pts;
  for (let i = 0; i < w.npts; i++) {
    const x = p[i*2], y = p[i*2+1];
    if (i > 0)            push(x, y, p[(i-1)*2], p[(i-1)*2+1], w.kind); // toward previous vertex
    if (i < w.npts - 1)   push(x, y, p[(i+1)*2], p[(i+1)*2+1], w.kind); // toward next vertex
  }
}

// ---- collapse near-collinear arms (a way passing straight through = one street = 2 arms, keep both;
//      but duplicate coincident directions from overlapping ways collapse). Then a junction = >=3 arms. ----
const DEDUP = 8; // degrees: arms within this are the same physical approach
for (const n of nodes.values()) {
  n.arms.sort((a, b) => a.brg - b.brg);
  const merged = [];
  for (const a of n.arms) {
    const last = merged[merged.length - 1];
    if (last && (Math.abs(a.brg - last.brg) < DEDUP)) continue;
    merged.push(a);
  }
  if (merged.length > 1 && Math.abs((merged[0].brg + 360) - merged[merged.length-1].brg) < DEDUP) merged.pop();
  n.arms = merged;
}

let junctions = [...nodes.values()].filter(n => n.arms.length >= 3);
if (ARMS) junctions = junctions.filter(n => n.arms.length === ARMS);
junctions.sort((a, b) => b.arms.length - a.arms.length);

console.log(`# ${name || file}  —  ${roads.length} road ways, ${nodes.size} nodes, ${junctions.length} junctions (>=3 arms${ARMS?`, filtered to ${ARMS}`:''})`);
for (const n of junctions.slice(0, TOP)) {
  const brgs = n.arms.map(a => a.brg.toFixed(0)).join(',');
  const cls  = n.arms.map(a => KIND_NAME[a.kind]).join('/');
  console.log(`${n.arms.length} arms @ ${brgs}   [${cls}]   (at ${n.x.toFixed(0)},${n.y.toFixed(0)} m)`);
  // ready-to-paste streetlab OsmJunc row:
  console.log(`    { "Delft real ${n.arms.length}-way", ${n.arms.length}, { ${n.arms.map(a=>a.brg.toFixed(0)).join(', ')} } },`);
}
