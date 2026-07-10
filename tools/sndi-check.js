#!/usr/bin/env node
// sndi-check.js — the street-network METRIC ORACLE (worldgen-plan.md rung 0).
// Computes the SNDi-style composite (Barrington-Leigh & Millard-Ball 2019, via
// road-hierarchy-notes.md §8.2) over a real OSM city (.rvb) OR a generated graph
// dump (.json), so "the generator is realistic" becomes a number: generated
// districts are done when their profile matches a real city's.
//
//   node tools/sndi-check.js <file.rvb|file.json> [more files...] [options]
//     multiple files print side-by-side — that IS the A/B compare.
//     --compare      exactly 2 files (GENERATED first, real TARGET second): print
//                    each shape metric's Δ vs a tolerance + PASS/FAIL + exit code —
//                    the rung-5 calibration GATE (a loop, not a vibe). --max-fail N
//                    (default 1) shape metrics may miss; --tol-<metric> overrides.
//     --check        self-test on synthetic grid + tree graphs (PASS/FAIL, exit code)
//     --json         machine-readable output (for snapshots / future citygrow HUD)
//     --classes X    rvb road kinds: "drive" (default: highway/arterial/road/
//                    secondary/tertiary — no service, no track) or "all"
//     --merge M      consolidate street nodes within M metres (union-find) —
//                    collapses dual-carriageway twins + roundabout rings into one
//                    logical intersection. Default 15 for .rvb, 0 for .json.
//     --samples N    circuity sample sources (default 25; deterministic PRNG)
//     --quant M      vertex-merge grid in metres (default 1.0, as osm-junction.js)
//
// METRICS (all on the CONTRACTED graph: street nodes = degree != 2, edges =
// polyline chains between them; definitions pinned here, keep them stable):
//   mean degree     2E/N over street nodes (grid ~4.0 · medieval ~3.1 · cul-de-sac ~2.2)
//   deg-1/3/4+      share of street nodes that are dead-ends / T / 4-way+
//   dendricity      LENGTH share of edges that are graph BRIDGES (Tarjan) —
//                   tree-likeness: pure tree = 1, pure grid ~ 0
//   circuity        mean network/straight-line distance over sampled node pairs
//                   (euclid 400–2500 m, Dijkstra, seeded — deterministic)
//   sinuosity       length-weighted mean of edge polyline-length / chord
//   orient entropy  Shannon entropy of edge chord bearings (mod 180°, 36 bins),
//                   normalized 0..1 — a grid detector (grid low, organic ~1)
//   ~block area     hull-bbox area / cycle count (E - V + C, Euler) — a PROXY:
//                   overpasses aren't planarized, so read it comparatively
//   int./km²        street-node density over the bbox area
//
// GENERATED-GRAPH JSON (what citygrow dumps; the one-data-model guard):
//   { "nodes": [[x,y], ...], "edges": [{"a":i, "b":j, "pts":[[x,y],...]?}, ...] }
//   coords in METRES; pts optional (straight edge). Class field ignored for now.
//
// .rvb parse mirrors data-tools/roadview/osm-junction.js (RVB1/2/3).
'use strict';
const fs = require('fs');

const args = process.argv.slice(2);
const files = args.filter(a => !a.startsWith('--') && !/^\d/.test(a));
const opt = (k, d) => { const i = args.indexOf(k); return i >= 0 ? args[i + 1] : d; };
const has = k => args.includes(k);

const KIND_NAME = { 0: 'highway', 1: 'arterial', 2: 'road', 3: 'track', 17: 'secondary', 18: 'tertiary', 19: 'service' };
const DRIVE_KINDS = new Set([0, 1, 2, 17, 18]);           // no service (driveways), no track
const ALL_KINDS   = new Set([0, 1, 2, 3, 17, 18, 19]);
const QUANT = parseFloat(opt('--quant', '1.0'));
const SAMPLES = parseInt(opt('--samples', '25'), 10);

// ---------- deterministic PRNG (no Math.random in this repo's oracles) ----------
function lcg(seed) { let s = seed >>> 0; return () => (s = (s * 1664525 + 1013904223) >>> 0) / 4294967296; }

// ---------- input: .rvb ----------
function loadRvb(file, kinds) {
  const buf = fs.readFileSync(file);
  const magic = buf.toString('latin1', 0, 4);
  if (magic.slice(0, 3) !== 'RVB') throw new Error(`${file}: not an .rvb (magic="${magic}")`);
  const hasHeight = magic[3] >= '2';
  let o = 4;
  const nfeat = buf.readInt32LE(o); o += 4;
  const namelen = buf.readInt32LE(o); o += 4;
  const name = buf.toString('utf8', o, o + namelen); o += namelen;
  o += 16; // bbox[4]
  const ways = [];
  for (let f = 0; f < nfeat; f++) {
    const kind = buf.readInt32LE(o); o += 4;
    if (hasHeight) o += 4;
    const sublen = buf.readInt32LE(o); o += 4; o += sublen;
    const npts = buf.readInt32LE(o); o += 4;
    const pts = new Float32Array(npts * 2);
    for (let k = 0; k < npts * 2; k++) { pts[k] = buf.readFloatLE(o); o += 4; }
    if (kinds.has(kind) && npts >= 2) ways.push(pts);
  }
  return { name: name || file, ways };
}

// ---------- input: generated-graph .json ----------
function loadJson(file) {
  const g = JSON.parse(fs.readFileSync(file, 'utf8'));
  const ways = [];
  for (const e of g.edges) {
    const pts = e.pts && e.pts.length >= 2 ? e.pts
      : [g.nodes[e.a], g.nodes[e.b]];
    const flat = new Float32Array(pts.length * 2);
    pts.forEach((p, i) => { flat[i * 2] = p[0]; flat[i * 2 + 1] = p[1]; });
    ways.push(flat);
  }
  return { name: file.replace(/^.*\//, ''), ways };
}

// ---------- micro graph: quantized vertices, dedup'd unit segments ----------
function microGraph(ways) {
  const id = new Map();                       // quantkey -> node id
  const xs = [], ys = [];
  const nid = (x, y) => {
    const k = `${Math.round(x / QUANT)},${Math.round(y / QUANT)}`;
    let i = id.get(k);
    if (i === undefined) { i = xs.length; id.set(k, i); xs.push(x); ys.push(y); }
    return i;
  };
  const adj = [];                             // id -> Map(nbr -> segLen)
  const link = (a, b, len) => {
    (adj[a] || (adj[a] = new Map())).set(b, len);
    (adj[b] || (adj[b] = new Map())).set(a, len);
  };
  for (const p of ways) {
    let prev = -1;
    for (let i = 0; i < p.length / 2; i++) {
      const cur = nid(p[i * 2], p[i * 2 + 1]);
      if (prev >= 0 && prev !== cur) {
        const dx = xs[cur] - xs[prev], dy = ys[cur] - ys[prev];
        link(prev, cur, Math.hypot(dx, dy));
      }
      prev = cur;
    }
  }
  for (let i = 0; i < xs.length; i++) if (!adj[i]) adj[i] = new Map();
  return { xs, ys, adj };
}

// ---------- contract degree-2 chains -> street graph ----------
function contract(m) {
  const deg = m.adj.map(a => a.size);
  const isStreet = i => deg[i] !== 2;
  const edges = [];                           // {a, b, len, chord}
  const seen = new Set();                     // traversed micro segments, both directions
  const mark = (a, b) => { seen.add(`${a}>${b}`); seen.add(`${b}>${a}`); };
  const walk = (start, first) => {
    let prev = start, cur = first, len = m.adj[start].get(first);
    mark(start, first);
    while (!isStreet(cur)) {
      const [n1, n2] = [...m.adj[cur].keys()];
      const nxt = n1 === prev ? n2 : n1;
      if (nxt === undefined || seen.has(`${cur}>${nxt}`)) break;  // guard odd data
      mark(cur, nxt);
      len += m.adj[cur].get(nxt);
      prev = cur; cur = nxt;
      if (cur === start) break;               // pure ring back to start
    }
    const chord = Math.hypot(m.xs[cur] - m.xs[start], m.ys[cur] - m.ys[start]);
    edges.push({ a: start, b: cur, len, chord });
  };
  for (let i = 0; i < m.adj.length; i++) {
    if (!isStreet(i)) continue;
    for (const nbr of m.adj[i].keys()) if (!seen.has(`${i}>${nbr}`)) walk(i, nbr);
  }
  // isolated pure rings (all deg-2, e.g. a detached roundabout): walk once from any unvisited
  let rings = 0;
  for (let i = 0; i < m.adj.length; i++) {
    if (isStreet(i) || deg[i] === 0) continue;
    const nbr = [...m.adj[i].keys()].find(n => !seen.has(`${i}>${n}`));
    if (nbr !== undefined) { walk(i, nbr); rings++; }
  }
  return { edges, rings, xs: m.xs, ys: m.ys };
}

// ---------- consolidate street nodes within R metres (union-find on grid buckets) ----------
function consolidate(g, R) {
  const nodeIds = new Set();
  for (const e of g.edges) { nodeIds.add(e.a); nodeIds.add(e.b); }
  const ids = [...nodeIds];
  const parent = new Map(ids.map(i => [i, i]));
  const find = i => { while (parent.get(i) !== i) { parent.set(i, parent.get(parent.get(i))); i = parent.get(i); } return i; };
  const union = (a, b) => { a = find(a); b = find(b); if (a !== b) parent.set(b, a); };
  if (R > 0) {
    const bucket = new Map();                 // grid cell -> ids
    const cell = i => `${Math.floor(g.xs[i] / R)},${Math.floor(g.ys[i] / R)}`;
    for (const i of ids) {
      const [cx, cy] = cell(i).split(',').map(Number);
      for (let dx = -1; dx <= 1; dx++) for (let dy = -1; dy <= 1; dy++) {
        const k = `${cx + dx},${cy + dy}`;
        for (const j of bucket.get(k) || []) {
          if (Math.hypot(g.xs[i] - g.xs[j], g.ys[i] - g.ys[j]) <= R) union(i, j);
        }
      }
      const k = cell(i);
      (bucket.get(k) || bucket.set(k, []).get(k)).push(i);
    }
  }
  // re-endpoint edges onto representatives; drop short intra-junction self-loops
  const edges = [];
  for (const e of g.edges) {
    const a = find(e.a), b = find(e.b);
    if (a === b && e.len <= 2 * R + 1) continue;   // roundabout ring segment / dual-cw sliver
    edges.push({ a, b, len: e.len, chord: Math.max(e.chord, a === b ? 0 : 1e-9) });
  }
  return { edges, rings: g.rings, xs: g.xs, ys: g.ys };
}

// ---------- metrics ----------
function metrics(g, name) {
  const deg = new Map();
  const bump = i => deg.set(i, (deg.get(i) || 0) + 1);
  for (const e of g.edges) { bump(e.a); if (e.a !== e.b) bump(e.b); else bump(e.b); }
  const nodes = [...deg.keys()];
  const N = nodes.length, E = g.edges.length;
  if (!N || !E) return { name, empty: true };

  const dshare = k => nodes.filter(i => deg.get(i) === k).length / N;
  const d4plus = nodes.filter(i => deg.get(i) >= 4).length / N;
  const meanDeg = 2 * E / N;

  // sinuosity: length-weighted, skip degenerate chords (self-loops)
  let sinW = 0, sinSum = 0;
  for (const e of g.edges) if (e.chord > 1) { sinSum += (e.len / e.chord) * e.len; sinW += e.len; }
  const sinuosity = sinW ? sinSum / sinW : 1;

  // adjacency over street graph (multi-edges: keep the shortest per pair for Dijkstra)
  const adj = new Map();
  const addAdj = (a, b, len, ei) => {
    let m = adj.get(a); if (!m) adj.set(a, (m = new Map()));
    const cur = m.get(b);
    if (!cur || len < cur.len) m.set(b, { len, ei });
  };
  g.edges.forEach((e, ei) => { if (e.a !== e.b) { addAdj(e.a, e.b, e.len, ei); addAdj(e.b, e.a, e.len, ei); } });

  // connected components (largest for circuity) — iterative BFS
  const comp = new Map(); let ncomp = 0, bigComp = 0, bigSize = 0;
  for (const s of nodes) {
    if (comp.has(s)) continue;
    const q = [s]; comp.set(s, ncomp); let size = 0;
    while (q.length) {
      const v = q.pop(); size++;
      for (const nb of (adj.get(v) || new Map()).keys()) if (!comp.has(nb)) { comp.set(nb, ncomp); q.push(nb); }
    }
    if (size > bigSize) { bigSize = size; bigComp = ncomp; }
    ncomp++;
  }

  // dendricity: Tarjan bridges, iterative (large graphs), length share
  const idx = new Map(), low = new Map();
  let timer = 1, bridgeLen = 0, totLen = 0;
  for (const e of g.edges) totLen += e.len;
  const bridgeEi = new Set();
  for (const root of nodes) {
    if (idx.has(root)) continue;
    const stack = [[root, -1, null, (adj.get(root) || new Map()).entries()]];
    idx.set(root, timer); low.set(root, timer); timer++;
    while (stack.length) {
      const fr = stack[stack.length - 1];
      const nxt = fr[3].next();
      if (nxt.done) {
        stack.pop();
        if (stack.length) {
          const p = stack[stack.length - 1][0];
          low.set(p, Math.min(low.get(p), low.get(fr[0])));
          if (low.get(fr[0]) > idx.get(p)) bridgeEi.add(fr[2]);
        }
        continue;
      }
      const [v, { ei }] = nxt.value;
      if (ei === fr[2]) continue;                          // don't reuse the entry edge
      if (idx.has(v)) { fr[0] !== undefined && low.set(fr[0], Math.min(low.get(fr[0]), idx.get(v))); continue; }
      idx.set(v, timer); low.set(v, timer); timer++;
      stack.push([v, fr[0], ei, (adj.get(v) || new Map()).entries()]);
    }
  }
  for (const ei of bridgeEi) bridgeLen += g.edges[ei].len;
  const dendricity = totLen ? bridgeLen / totLen : 0;

  // circuity: seeded samples in the largest component
  const bigNodes = nodes.filter(i => comp.get(i) === bigComp);
  const rnd = lcg(0xC17C);
  let circSum = 0, circN = 0;
  const srcCount = Math.min(SAMPLES, bigNodes.length);
  for (let s = 0; s < srcCount; s++) {
    const src = bigNodes[Math.floor(rnd() * bigNodes.length)];
    // Dijkstra with 4000 m cutoff (binary-heap-free: array heap is fine at these sizes)
    const dist = new Map([[src, 0]]);
    const heap = [[0, src]];
    while (heap.length) {
      let bi = 0;
      for (let i = 1; i < heap.length; i++) if (heap[i][0] < heap[bi][0]) bi = i;
      const [d, v] = heap.splice(bi, 1)[0];
      if (d !== dist.get(v) || d > 4000) continue;
      for (const [nb, { len }] of adj.get(v) || new Map()) {
        const nd = d + len;
        if (nd < (dist.get(nb) ?? Infinity)) { dist.set(nb, nd); heap.push([nd, nb]); }
      }
    }
    let picked = 0;
    for (let t = 0; t < 60 && picked < 8; t++) {
      const dst = bigNodes[Math.floor(rnd() * bigNodes.length)];
      const eu = Math.hypot(g.xs[dst] - g.xs[src], g.ys[dst] - g.ys[src]);
      if (eu < 400 || eu > 2500 || !dist.has(dst)) continue;
      circSum += dist.get(dst) / eu; circN++; picked++;
    }
  }
  const circuity = circN ? circSum / circN : NaN;

  // orientation entropy: chord bearings mod 180, 36 bins, length-weighted, normalized
  const bins = new Array(36).fill(0);
  for (const e of g.edges) {
    if (e.chord <= 1) continue;
    const brg = ((Math.atan2(g.ys[e.b] - g.ys[e.a], g.xs[e.b] - g.xs[e.a]) * 180 / Math.PI) + 360) % 180;
    bins[Math.floor(brg / 5) % 36] += e.len;
  }
  const bw = bins.reduce((a, b) => a + b, 0);
  let H = 0;
  for (const b of bins) if (b > 0) { const p = b / bw; H -= p * Math.log(p); }
  const orientEntropy = bw ? H / Math.log(36) : NaN;

  // extent + densities + Euler block proxy
  let x0 = 1e30, x1 = -1e30, y0 = 1e30, y1 = -1e30;
  for (const i of nodes) { x0 = Math.min(x0, g.xs[i]); x1 = Math.max(x1, g.xs[i]); y0 = Math.min(y0, g.ys[i]); y1 = Math.max(y1, g.ys[i]); }
  const areaKm2 = ((x1 - x0) * (y1 - y0)) / 1e6;
  const cycles = E - N + ncomp;
  const blockHa = cycles > 0 ? (areaKm2 * 100) / cycles : NaN;

  return {
    name, nodes: N, edges: E, comps: ncomp, netKm: totLen / 1000, areaKm2,
    meanDeg, deg1: dshare(1), deg3: dshare(3), deg4: d4plus,
    dendricity, circuity, sinuosity, orientEntropy,
    intPerKm2: nodes.filter(i => deg.get(i) >= 3).length / areaKm2,
    blockHa,
  };
}

// ---------- synthetic graphs for --check ----------
function synthGrid(n, sp) {
  const nodes = [], edges = [];
  for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) nodes.push([x * sp, y * sp]);
  const id = (x, y) => y * n + x;
  for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) {
    if (x + 1 < n) edges.push({ a: id(x, y), b: id(x + 1, y) });
    if (y + 1 < n) edges.push({ a: id(x, y), b: id(x, y + 1) });
  }
  return { nodes, edges };
}
function synthTree(depth, sp) {
  // full binary tree, positional layout — collision-free by construction
  // (random offsets can collide under quantization and weld a cycle; bit us)
  const nodes = [[0, 0]], edges = [];
  let frontier = [0];
  const span = Math.pow(2, depth) * sp;
  for (let d = 1; d <= depth; d++) {
    const next = [];
    const w = span / Math.pow(2, d);
    frontier.forEach((p, i) => {
      for (let k = 0; k < 2; k++) {
        const slot = i * 2 + k;
        nodes.push([(slot + 0.5) * w - span / 2, d * sp]);
        const c = nodes.length - 1;
        edges.push({ a: p, b: c });
        next.push(c);
      }
    });
    frontier = next;
  }
  return { nodes, edges };
}
function graphFromSynth(s) {
  const ways = s.edges.map(e => {
    const f = new Float32Array(4);
    f[0] = s.nodes[e.a][0]; f[1] = s.nodes[e.a][1];
    f[2] = s.nodes[e.b][0]; f[3] = s.nodes[e.b][1];
    return f;
  });
  return contract(microGraph(ways));
}

function check() {
  const grid = metrics(graphFromSynth(synthGrid(20, 100)), 'grid20');
  const tree = metrics(graphFromSynth(synthTree(9, 120)), 'tree');
  const asserts = [
    ['grid mean degree ~3.8', grid.meanDeg > 3.5 && grid.meanDeg <= 4.01],
    ['grid deg-4 share high', grid.deg4 > 0.7],
    ['grid dendricity ~0', grid.dendricity < 0.05],
    ['grid orient entropy low', grid.orientEntropy < 0.35],
    ['grid circuity 1.1–1.5', grid.circuity > 1.05 && grid.circuity < 1.5],
    ['tree dendricity = 1', tree.dendricity > 0.999],
    ['tree deg-1 share high', tree.deg1 > 0.4],
    ['tree mean degree < 2.6', tree.meanDeg < 2.6],
  ];
  let fail = 0;
  for (const [label, ok] of asserts) {
    console.log(`${ok ? 'ok  ' : 'FAIL'}  ${label}`);
    if (!ok) fail++;
  }
  console.log(fail ? `sndi-check: ${fail} FAILED` : 'sndi-check: PASS');
  process.exit(fail ? 1 : 0);
}

// ---------- output ----------
const ROWS = [
  ['nodes',        m => m.nodes],
  ['edges',        m => m.edges],
  ['network km',   m => m.netKm.toFixed(1)],
  ['area km²',     m => m.areaKm2.toFixed(1)],
  ['mean degree',  m => m.meanDeg.toFixed(2)],
  ['deg-1 (dead-end)', m => (m.deg1 * 100).toFixed(1) + '%'],
  ['deg-3 (T)',    m => (m.deg3 * 100).toFixed(1) + '%'],
  ['deg-4+ (X)',   m => (m.deg4 * 100).toFixed(1) + '%'],
  ['dendricity',   m => m.dendricity.toFixed(3)],
  ['circuity',     m => isNaN(m.circuity) ? '—' : m.circuity.toFixed(3)],
  ['sinuosity',    m => m.sinuosity.toFixed(3)],
  ['orient entropy', m => isNaN(m.orientEntropy) ? '—' : m.orientEntropy.toFixed(3)],
  ['int. / km²',   m => m.intPerKm2.toFixed(1)],
  ['~block ha',    m => isNaN(m.blockHa) ? '—' : m.blockHa.toFixed(2)],
];

// ---------- the rung-5 calibration GATE: generated vs a real target ----------
// The scale-free SHAPE metrics (the SNDi composite) with a "reads as the same city
// family" tolerance each. Extent-dependent rows (nodes/area/int-per-km²/block ha)
// are informational only — a generated district need not match a whole city's size.
const CMP = [
  ['mean degree',    m => m.meanDeg,       0.30, v => v.toFixed(2)],
  ['deg-1 (dead-end) %', m => m.deg1 * 100, 10, v => v.toFixed(1)],
  ['deg-3 (T) %',    m => m.deg3 * 100,     12,   v => v.toFixed(1)],
  ['deg-4+ (X) %',   m => m.deg4 * 100,     10,   v => v.toFixed(1)],
  ['dendricity',     m => m.dendricity,     0.10, v => v.toFixed(3)],
  ['circuity',       m => m.circuity,       0.25, v => v.toFixed(3)],
  ['sinuosity',      m => m.sinuosity,      0.20, v => v.toFixed(3)],
  ['orient entropy', m => m.orientEntropy,  0.20, v => v.toFixed(3)],
];
function compareGate(gen, real) {
  if (gen.empty || real.empty) { console.error('compare: an input graph is empty'); process.exit(1); }
  const slug = s => '--tol-' + s.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
  const W = 20, C = 12;
  console.log(`SNDi calibration gate — GENERATED vs ${real.name}`);
  console.log(''.padEnd(W) + 'generated'.padStart(C) + 'target'.padStart(C) +
              'Δ'.padStart(C) + 'tol'.padStart(C) + '   verdict');
  let fails = 0;
  for (const [label, fn, defTol, fmt] of CMP) {
    const a = fn(gen), b = fn(real);
    const tol = parseFloat(opt(slug(label), String(defTol)));
    const d = Math.abs(a - b), ok = d <= tol;
    if (!ok) fails++;
    console.log(label.padEnd(W) + fmt(a).padStart(C) + fmt(b).padStart(C) +
                d.toFixed(fmt === undefined ? 2 : (defTol < 1 ? 3 : 1)).padStart(C) +
                String(tol).padStart(C) + '   ' + (ok ? 'ok' : 'MISS'));
  }
  const maxFail = parseInt(opt('--max-fail', '1'), 10);
  const pass = fails <= maxFail;
  console.log(`\n${fails}/${CMP.length} shape metrics miss (allowed ${maxFail}) → ${pass ? 'PASS' : 'FAIL'}`);
  process.exit(pass ? 0 : 1);
}

function main() {
  if (has('--check')) return check();
  if (!files.length) {
    console.error('usage: sndi-check.js <file.rvb|file.json> [...] [--check --json --classes drive|all --merge M --samples N]');
    process.exit(1);
  }
  const kinds = opt('--classes', 'drive') === 'all' ? ALL_KINDS : DRIVE_KINDS;
  const results = files.map(f => {
    const isJson = f.endsWith('.json');
    const R = parseFloat(opt('--merge', isJson ? '0' : '15'));
    const src = isJson ? loadJson(f) : loadRvb(f, kinds);
    const g = consolidate(contract(microGraph(src.ways)), R);
    return metrics(g, src.name);
  });
  if (has('--compare')) {
    if (results.length !== 2) { console.error('--compare needs exactly 2 files: GENERATED then real TARGET'); process.exit(1); }
    return compareGate(results[0], results[1]);
  }
  if (has('--json')) { console.log(JSON.stringify(results, null, 2)); return; }
  const W = 18, C = 16;
  console.log(''.padEnd(W) + results.map(r => String(r.name).slice(0, C - 1).padStart(C)).join(''));
  for (const [label, fn] of ROWS) {
    console.log(label.padEnd(W) + results.map(r => (r.empty ? '—' : String(fn(r))).padStart(C)).join(''));
  }
}
main();
