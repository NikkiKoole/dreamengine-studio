#!/usr/bin/env node
// cart-index.js — a COMPUTED technique index for the cart library. Answers two
// questions nothing else does, with zero manual tagging (so it never rots):
//
//   1. "What cart should I read to learn X?"  (exemplar picker — agents building carts)
//   2. "What does the whole shelf look like?"  (coverage clusters, rare exemplars)
//
// It derives each cart's techniques purely from its source: which cart-land library
// headers it #includes (ui.h/keybed.h/radio.h/…) and which studio.h API calls it
// actually makes, bucketed into technique FAMILIES and ranked by call intensity.
//
// It also CLASSIFIES every .c under tools/carts/ into:
//   registered  — a gallery cart (in editor/public/carts/index.json)
//   harness     — unregistered but wired to a check tool (soak, tunecheck, fxcheck…)
//   probe       — a self-labeled single-effect verification cart (header says so)
//   orphan      — unregistered, no tool ref, no self-label  → the only thing to review
//
// Usage:
//   node tools/cart-index.js                 summary + exemplar picker
//   node tools/cart-index.js --coverage      shelf-coverage table (saturation %)
//   node tools/cart-index.js --exemplars     top carts per technique (default section)
//   node tools/cart-index.js --probes        the verification-probe shelf
//   node tools/cart-index.js --probes --md   …as a markdown table (paste into docs)
//   node tools/cart-index.js --cart <name>   one cart's technique breakdown
//   node tools/cart-index.js --classify      list every cart's class (flags orphans)
//   node tools/cart-index.js --json          everything, machine-readable
//   --top <N>   exemplars per technique (default 3)

const fs = require('fs');
const path = require('path');
const ROOT = path.resolve(__dirname, '..');
const CARTS = path.join(ROOT, 'tools/carts');
const TOOLS = path.join(ROOT, 'tools');

// ── registry ──────────────────────────────────────────────────────────────────
const idx = require(path.join(ROOT, 'editor/public/carts/index.json'));
const REG = Array.isArray(idx) ? idx : Object.values(idx)[0];
const meta = Object.fromEntries(REG.map(c => [c.file.replace('.cart.png', ''), c]));
const registered = new Set(Object.keys(meta));

// ── API surface from studio.h ───────────────────────────────────────────────
const hdr = fs.readFileSync(path.join(ROOT, 'runtime/studio.h'), 'utf8');
const API = new Set();
for (const m of hdr.matchAll(/^[A-Za-z_][\w \*]*?\b([a-z_][a-z0-9_]*)\s*\([^)]*\)\s*;/gm)) API.add(m[1]);

// ── library header → capability tag ─────────────────────────────────────────
const HEADER_TAG = {
  'ui.h': 'widgets (ui.h)', 'keybed.h': 'chromatic-keybed (keybed.h)',
  'radio.h': 'radio-station (radio.h)', 'solo.h': 'solo-strip (solo.h)',
  'pointer.h': 'multi-finger pointer (pointer.h)', 'gestures.h': 'swipe/pinch (gestures.h)',
  'improv.h': 'auto-improv (improv.h)',
};

// ── API call → technique family (prefix match; longest key wins) ─────────────
const FAMILIES = [
  ['input: keyboard',       ['key', 'keyp', 'keyr', 'text_input']],
  ['input: gamepad/dpad',   ['btn', 'btnp', 'btnr', 'stick_x', 'stick_y']],
  ['input: touch',          ['touch_', 'tap', 'tapp', 'tapr']],
  ['input: mouse',          ['mouse_']],
  ['input: MIDI',           ['midi_']],
  ['draw: primitives',      ['line', 'pset', 'rect', 'rectfill', 'circ', 'circfill', 'oval',
                             'ovalfill', 'tri', 'trifill', 'poly', 'polyfill', 'ngon', 'star',
                             'starfill', 'arc', 'ring', 'rrect', 'rrectfill', 'thickline', 'bar',
                             'bezier', 'gradient', 'vgradient', 'hgradient']],
  ['draw: dithered fills',  ['fillp']],
  ['draw: sprites',         ['spr', 'sprf', 'sspr', 'colorkey']],
  ['draw: sprite transform',['spr_rot', 'sspr_ex', 'rectfill_rot', 'print_rot']],
  ['draw: text/fonts',      ['font', 'print', 'text_width']],
  ['camera & clip',         ['camera', 'smooth_zoom', 'follow', 'clip', 'zoom_rect']],
  ['palette & screen fx',   ['pal', 'fade', 'shake']],
  ['tilemap',               ['mget', 'mset', 'map', 'map_scale', 'tile_at', 'touching_map']],
  ['3D / projection',       ['tritex', 'rot3', 'project3', 'zsort', 'quadfill']],
  ['sound: notes/play',     ['note', 'note_on', 'note_off', 'sfx', 'hit', 'chord', 'strum', 'tone']],
  ['sound: synth config',   ['instrument', 'wave_set', 'note_pitch', 'note_vol', 'note_cutoff',
                             'note_res', 'note_lfo', 'note_env', 'lfo_', 'voice_']],
  ['sound: effects',        ['echo', 'reverb', 'chorus', 'flanger', 'tape', 'wah', 'crush', 'eq',
                             'formant', 'tremolo', 'autopan', 'ringmod', 'phaser', 'univibe',
                             'shimmer', 'varispeed', 'gate', 'filter', 'leslie', 'drive', 'grains',
                             'sidechain', 'glue', 'fx_', 'dropout', 'shallow', 'amp_noise']],
  ['sound: spatial',        ['listener', 'spatial_', 'note_pos', 'note_motion', 'instrument_pos', 'pan_law']],
  ['sequencing & timing',   ['bpm', 'beat', 'every', 'euclid', 'chance', 'degree', 'schedule', 'timer']],
  ['math & easing',         ['ease_', 'lerp', 'remap', 'clamp', 'mid', 'sgn', 'angle_to', 'distance',
                             'sin_deg', 'cos_deg', 'dx', 'dy']],
  ['procgen: noise/random', ['noise', 'noise2', 'noise3', 'rnd', 'rnd_between', 'rnd_float']],
  ['collision',             ['boxes_touch', 'point_in_box', 'circles_touch', 'near', 'touching_color', 'tile_at']],
  ['persistence (save)',    ['save', 'load', 'save_int', 'save_bytes']],
];
function familyOf(call) {
  let best = null, bestLen = -1;
  for (const [fam, keys] of FAMILIES)
    for (const k of keys)
      if ((call === k || call.startsWith(k)) && k.length > bestLen) { best = fam; bestLen = k.length; }
  return best;
}

// ── TEACHES vocabulary (CONCEPTUAL techniques the API calls can't reveal) ────
// The mechanical axis (which primitives/input/effects) is computed from calls; this
// is the other axis — the named idea a reader comes to a cart to learn. SEED list,
// grown like lint-carts.js's KINDS: add a tag here when a cart genuinely needs one
// (a tag not listed is a soft --lint warning, catching typos + prompting a real add).
const TEACHES_VOCAB = new Set([
  // procgen / world
  'gene-based-procgen', 'wave-function-collapse', 'marching-squares', 'autotiling',
  'noise-terrain', 'maze-generation', 'dungeon-generation', 'l-system', 'cellular-automata',
  // agents / ai
  'state-machine', 'finite-state-ai', 'pathfinding', 'flocking', 'car-following',
  'steering-behaviors', 'traffic-sim',
  // physics
  'verlet-integration', 'spring-damper', 'rigid-body', 'particle-system', 'fluid-sim', 'soft-body',
  // rendering
  'raycasting', 'mode7', 'parallax', 'camera-follow', 'dithering-gradient', 'palette-cycling',
  'software-rasterizer', 'procedural-mesh', 'sprite-stacking', 'no-sprite-vehicles',
  // game structure
  'title-play-gameover-loop', 'tilemap-collision', 'inventory-system', 'dialogue-tree',
  'turn-based-loop', 'grid-movement', 'save-load-persistence', 'screen-shake-juice',
  // audio
  'subtractive-synth', 'granular-synth', 'waveguide-synth', 'fm-synth', 'additive-synth',
  'step-sequencer', 'euclidean-rhythm', 'adsr-envelope', 'generative-melody', 'chord-voicing',
]);

// ── classification ────────────────────────────────────────────────────────────
const PROBE_RE = /verification cart|throwaway|source-only|not registered|unregistered|stress probe|\bprobe\b.{0,40}not a game/i;
// blob of everything that WIRES a cart by name: all tools/*.js (check tools render
// these headless). An unregistered cart wired there is infrastructure, not an orphan.
// NOTE: deliberately NOT docs/ — every cart has a design doc that names it, so a doc
// mention can't distinguish wired infra from a finished-but-unregistered cart.
const refBlob = fs.readdirSync(TOOLS).filter(f => f.endsWith('.js') && f !== 'cart-index.js')
  .map(f => fs.readFileSync(path.join(TOOLS, f), 'utf8')).join('\n');
function classify(name, head) {
  if (registered.has(name)) return 'registered';
  if (PROBE_RE.test(head)) return 'probe';
  if (new RegExp('\\b' + name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + '\\b').test(refBlob)) return 'harness';
  return 'orphan';
}

function analyze(name) {
  const src = fs.readFileSync(path.join(CARTS, name + '.c'), 'utf8');
  const head = src.split('\n').slice(0, 6).join('\n');
  const headers = new Set();
  for (const m of src.matchAll(/#include\s+"([^"]+)"/g))
    if (HEADER_TAG[m[1]]) headers.add(HEADER_TAG[m[1]]);
  const fams = {};
  for (const n of API) {
    const hits = (src.match(new RegExp('\\b' + n + '\\s*\\(', 'g')) || []).length;
    if (!hits) continue;
    const fam = familyOf(n);
    if (!fam) continue;
    (fams[fam] = fams[fam] || { calls: [], total: 0 }).calls.push([n, hits]);
    fams[fam].total += hits;
  }
  // control-build flag(s): #ifdef/#ifndef macros = the A/B knob for a probe
  const flags = [...new Set([...src.matchAll(/#if(?:n?def)\s+([A-Z_][A-Z0-9_]*)/g)].map(m => m[1]))]
    .filter(f => f !== '__cplusplus');
  const purpose = (src.match(/^\/\/\s*(.+)$/m) || [, ''])[1].replace(/^\S+\s*[—-]\s*/, '');
  // author-tagged conceptual techniques + lineage (the cart-header convention)
  const teaches = ((src.match(/^\/\/\s*TEACHES:\s*(.+)$/m) || [, ''])[1])
    .split(',').map(s => s.trim()).filter(Boolean);
  const lineage = (src.match(/^\/\/\s*LINEAGE:\s*(.+)$/m) || [, ''])[1].trim();
  return { headers: [...headers], fams, klass: classify(name, head), flags, purpose, head, teaches, lineage };
}

// ── gather all carts ────────────────────────────────────────────────────────
const all = fs.readdirSync(CARTS).filter(f => f.endsWith('.c')).map(f => f.replace('.c', '')).sort();
const data = {};
for (const n of all) { try { data[n] = analyze(n); } catch {} }
const names = Object.keys(data);

const byClass = { registered: [], harness: [], probe: [], orphan: [] };
for (const n of names) byClass[data[n].klass].push(n);

// reverse + intensity (mechanical axis) + concepts (author-tagged axis) + lineage,
// built ONLY from registered carts (the learnable gallery)
const reverse = {}, intensity = {}, concepts = {}, lineages = [];
const learnable = byClass.registered;
for (const n of learnable) {
  const { headers, fams, teaches, lineage } = data[n];
  headers.forEach(h => { (reverse[h] = reverse[h] || []).push(n); (intensity[h] = intensity[h] || []).push([n, 99]); });
  for (const [fam, d] of Object.entries(fams)) {
    (reverse[fam] = reverse[fam] || []).push(n);
    (intensity[fam] = intensity[fam] || []).push([n, d.total]);
  }
  teaches.forEach(t => (concepts[t] = concepts[t] || []).push(n));
  if (lineage) lineages.push([n, lineage]);
}
const conceptRows = Object.entries(concepts).sort((a, b) => b[1].length - a[1].length);
const taggedCount = learnable.filter(n => data[n].teaches.length).length;
const NL = learnable.length;
const SAT = 0.70;
const rows = Object.entries(reverse).sort((a, b) => b[1].length - a[1].length);

// ── flags ──────────────────────────────────────────────────────────────────
const A = process.argv.slice(2);
const has = f => A.includes(f);
const TOP = (() => { const i = A.indexOf('--top'); return i >= 0 ? +A[i + 1] : 3; })();

// ── --json ────────────────────────────────────────────────────────────────
if (has('--json')) {
  const out = { counts: Object.fromEntries(Object.entries(byClass).map(([k, v]) => [k, v.length])),
    classes: byClass, coverage: {}, exemplars: {}, concepts: {}, lineage: {}, probes: [] };
  for (const [tech, carts] of rows) {
    out.coverage[tech] = { carts: carts.length, pct: +(carts.length / NL * 100).toFixed(1) };
    if (carts.length / NL < SAT)
      out.exemplars[tech] = intensity[tech].sort((a, b) => b[1] - a[1]).slice(0, TOP).map(([c, t]) => ({ cart: c, intensity: t }));
  }
  for (const [tag, carts] of conceptRows) out.concepts[tag] = carts;
  for (const [n, l] of lineages) out.lineage[n] = l;
  out.probes = byClass.probe.map(n => ({ cart: n, purpose: data[n].purpose, control: data[n].flags }));
  console.log(JSON.stringify(out, null, 2));
  return;
}

// ── --cart <name> ────────────────────────────────────────────────────────────
if (has('--cart')) {
  const n = A[A.indexOf('--cart') + 1];
  const d = data[n];
  if (!d) { console.error('no such cart: ' + n); process.exit(1); }
  const m = meta[n] || {};
  console.log(`\n▌ ${n}   [${d.klass}${m.kind ? ' · ' + m.kind.join(', ') : ''}${m.genre ? ' · ' + m.genre : ''}]`);
  if (d.purpose) console.log('  ' + d.purpose);
  if (d.teaches.length) console.log('  teaches: ' + d.teaches.join(', '));
  if (d.lineage) console.log('  lineage: ' + d.lineage);
  if (d.headers.length) console.log('  building blocks: ' + d.headers.join(', '));
  for (const [fam, x] of Object.entries(d.fams).sort((a, b) => b[1].total - a[1].total)) {
    const top = x.calls.sort((a, b) => b[1] - a[1]).slice(0, 5).map(c => c[0]).join(' ');
    console.log(`  • ${fam.padEnd(24)} ${String(x.total).padStart(3)}×  (${top})`);
  }
  console.log();
  return;
}

// ── --classify ────────────────────────────────────────────────────────────
if (has('--classify')) {
  console.log(`\nCART CLASSIFICATION — ${names.length} sources\n`);
  for (const k of ['registered', 'harness', 'probe', 'orphan'])
    console.log(`  ${k.padEnd(12)} ${byClass[k].length}`);
  if (byClass.orphan.length) {
    console.log('\n⚠ ORPHANS (unregistered, no tool ref, no self-label — review):');
    byClass.orphan.forEach(n => console.log('   ' + n));
  } else console.log('\n✓ no orphans — every unregistered cart is a wired harness or self-labeled probe');
  console.log();
  return;
}

// ── --probes ────────────────────────────────────────────────────────────────
if (has('--probes')) {
  const probes = byClass.probe.sort();
  if (has('--md')) {
    console.log('| Cart | Verifies | Control build |');
    console.log('|---|---|---|');
    for (const n of probes) {
      const d = data[n];
      console.log(`| \`${n}\` | ${d.purpose || '—'} | ${d.flags.length ? '`-D' + d.flags.join('` / `-D') + '`' : '—'} |`);
    }
  } else {
    console.log(`\nVERIFICATION-PROBE SHELF — ${probes.length} self-labeled single-effect probes\n`);
    for (const n of probes) {
      const d = data[n];
      console.log(`  ${n}${d.flags.length ? '  [control: -D' + d.flags.join(' / -D') + ']' : ''}`);
      console.log(`     ${d.purpose}`);
    }
    console.log();
  }
  return;
}

// ── --lineage ─────────────────────────────────────────────────────────────
if (has('--lineage')) {
  console.log(`\nLINEAGE — ${lineages.length} carts carry a LINEAGE: line (what-descends-from-what)\n`);
  for (const [n, l] of lineages.sort()) console.log(`  ${n.padEnd(18)} ${l}`);
  if (!lineages.length) console.log('  (none yet — add `// LINEAGE: …` to cart docblocks)');
  console.log();
  return;
}

// ── --lint (soft; for CI as a WARNING, never blocks) ────────────────────────
if (has('--lint')) {
  const missing = learnable.filter(n => !data[n].teaches.length);
  const bad = [];
  for (const n of learnable)
    for (const t of data[n].teaches)
      if (!TEACHES_VOCAB.has(t)) bad.push([n, t]);
  console.log(`\nTEACHES LINT — ${taggedCount}/${NL} registered carts tagged\n`);
  if (bad.length) {
    console.log('⚠ tags not in the seed vocabulary (typo, or add to TEACHES_VOCAB):');
    bad.forEach(([n, t]) => console.log(`   ${n}: ${t}`));
  } else console.log('✓ all TEACHES tags are in the vocabulary');
  console.log(`\nℹ ${missing.length} registered carts have no TEACHES: line yet` +
    (missing.length ? ` (first 10: ${missing.slice(0, 10).join(', ')}…)` : ''));
  console.log('  (soft — the back catalogue fills in over time; new carts add it at authoring)\n');
  return;
}

// ── --coverage ────────────────────────────────────────────────────────────
if (has('--coverage')) {
  console.log(`\nSHELF COVERAGE — ${NL} registered carts · technique → #carts (saturation %)\n`);
  for (const [tech, carts] of rows) {
    const pct = Math.round(carts.length / NL * 100);
    const bar = '█'.repeat(Math.round(pct / 3));
    let note = '';
    if (carts.length / NL >= SAT) note = '  ⚠ ubiquitous';
    else if (carts.length <= 3) note = '  ◂ rare: ' + carts.join(', ');
    console.log(`  ${tech.padEnd(26)} ${String(carts.length).padStart(3)} ${String(pct + '%').padStart(4)} ${bar}${note}`);
  }
  console.log();
  return;
}

// ── default / --exemplars ────────────────────────────────────────────────────
console.log(`\nCART INDEX — ${names.length} sources: ${byClass.registered.length} registered · ` +
  `${byClass.harness.length} harness · ${byClass.probe.length} probe · ${byClass.orphan.length} orphan`);
if (byClass.orphan.length) console.log('  ⚠ orphans: ' + byClass.orphan.join(', ') + '   (node tools/cart-index.js --classify)');
console.log(`\nEXEMPLAR PICKER — best ${TOP} carts by intensity per technique (ubiquitous families hidden)\n`);
for (const [tech, carts] of rows) {
  if (carts.length / NL >= SAT) continue;
  const top = intensity[tech].sort((a, b) => b[1] - a[1]).slice(0, TOP)
    .map(([c, t]) => `${c}${t < 99 ? ' (' + t + '×)' : ''}`).join(', ');
  console.log(`  ${tech.padEnd(26)} ${top}`);
}
console.log(`\nCONCEPTS — author-tagged techniques (${taggedCount}/${NL} carts carry TEACHES:)\n`);
if (conceptRows.length) {
  for (const [tag, carts] of conceptRows) console.log(`  ${tag.padEnd(26)} ${carts.join(', ')}`);
} else {
  console.log('  (none yet — the cart-header convention is new; tags accrue as carts add `// TEACHES:`)');
}
console.log('\n  more: --coverage  --concepts via --json  --lineage  --probes  --classify  --lint  --cart <name>  --top <N>\n');
