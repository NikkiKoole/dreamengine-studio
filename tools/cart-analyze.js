#!/usr/bin/env node
// cart-analyze.js — static complexity + global-state report across all carts.
//
// Answers "which carts warrant a spec(), and which KIND?" by measuring, per cart:
//   loc        non-blank source lines
//   funcs      top-level function definitions
//   globals    file-scope mutable `static` variables (the cart's global state)
//   arrays     of those globals, how many are arrays/structs (accumulating state)
//   state      uses de_state()/STATE/S-> (live-reload-safe persistent state)
//   upd / drw  lines of logic inside update() vs draw()
//   probe      exposes a probe-like fn (point query — good for frame()+expect specs)
//
// Verdict heuristic (see docs/design/spec-harness.md):
//   stateful   real mutable state mutated over frames in update()  → step()+expect spec
//   procedural seed-driven, logic concentrated in draw()           → golden / frame()+probe spec
//   reactive   UI/instrument cart, state is widget/audio config     → light spec or none
//   simple     small / draw-only                                   → no spec
//
// Heuristic, not a compiler. Flags candidates; you judge. CommonJS, plain node.
const fs = require('fs');
const path = require('path');

const dir = path.join(__dirname, 'carts');
const files = fs.readdirSync(dir).filter(f => f.endsWith('.c')).sort();

// strip /* */ and // comments and "string" / 'c' literals so counts aren't fooled
function decomment(src) {
  let out = '', i = 0, n = src.length;
  while (i < n) {
    const c = src[i], d = src[i + 1];
    if (c === '/' && d === '*') { i += 2; while (i < n && !(src[i] === '*' && src[i + 1] === '/')) i++; i += 2; continue; }
    if (c === '/' && d === '/') { while (i < n && src[i] !== '\n') i++; continue; }
    if (c === '"' || c === '\'') { const q = c; out += ' '; i++; while (i < n && src[i] !== q) { if (src[i] === '\\') i++; i++; } i++; continue; }
    out += c; i++;
  }
  return out;
}

// extract the body (brace-balanced) of a top-level definition matching `sig` regex
function bodyOf(src, sigRe) {
  const m = sigRe.exec(src);
  if (!m) return null;
  let i = src.indexOf('{', m.index);
  if (i < 0) return null;
  let depth = 0, start = i;
  for (; i < src.length; i++) {
    if (src[i] === '{') depth++;
    else if (src[i] === '}') { depth--; if (depth === 0) return src.slice(start + 1, i); }
  }
  return null;
}
const nonBlank = s => s ? s.split('\n').filter(l => l.trim()).length : 0;

function analyze(file) {
  const raw = fs.readFileSync(path.join(dir, file), 'utf8');
  const code = decomment(raw);
  const lines = code.split('\n');

  const loc = nonBlank(code);

  // top-level function definitions: a line at column 0 that opens a brace after a )
  // (catches `void draw(void) {`, `static int foo(...) {`, multi-line sigs end with `) {`)
  let funcs = 0;
  for (let i = 0; i < lines.length; i++) {
    const l = lines[i];
    if (/^[A-Za-z_].*\)\s*\{\s*$/.test(l) && /[A-Za-z_]\s*\(/.test(l)) funcs++;
  }

  // file-scope static variables: a `static ...;` at column 0 that is NOT a function
  // def (no `(name` call form) — includes arrays `[`, scalars, structs. const excluded.
  let globals = 0, arrays = 0, constGlobals = 0;
  for (const l of lines) {
    if (!/^static\b/.test(l)) continue;
    if (/\)\s*\{?\s*$/.test(l) && /\w\s*\(/.test(l)) continue;          // function def/proto
    if (/typedef/.test(l)) continue;
    if (!/;/.test(l)) continue;                                          // must be a full decl on the line
    if (/^static\s+(const\b|inline\b)/.test(l)) { constGlobals++; continue; }
    if (/\(/.test(l) && !/\[/.test(l)) continue;                         // fn pointer-ish proto w/o array
    globals++;
    if (/\[/.test(l)) arrays++;
  }

  const usesState = /\bde_state\s*\(|\bSTATE\b|\bS->/.test(code);
  const probe = /\bprobe\b/.test(code) || /\b\w*_probe\s*\(/.test(code);

  const upd = nonBlank(bodyOf(code, /\bvoid\s+update\s*\(\s*(void)?\s*\)/));
  const drw = nonBlank(bodyOf(code, /\bvoid\s+draw\s*\(\s*(void)?\s*\)/));

  // reactive = leans on ui.h / keybed / radio / pointer machinery
  const reactive = /\b(ui_button|ui_slider|ui_knob|note_on|rad_knob|keybed|ptr_)/i.test(code);

  // verdict
  let verdict;
  if (loc < 120 && globals <= 4 && upd < 25) verdict = 'simple';
  else if (usesState || (globals >= 6 && arrays >= 1 && upd >= drw * 0.6 && upd >= 20)) verdict = 'stateful';
  else if (drw > upd * 1.8 && drw >= 60) verdict = 'procedural';
  else if (reactive) verdict = 'reactive';
  else verdict = 'mixed';

  // spec-worthiness score: complexity × has-mutable-state, dampened for simple/reactive
  let score = loc * 0.01 + funcs * 0.4 + globals * 0.8 + arrays * 1.5 + upd * 0.05 + (usesState ? 4 : 0);
  if (verdict === 'simple') score *= 0.2;
  if (verdict === 'reactive') score *= 0.5;
  if (probe) score += 2;

  return { file: file.replace('.c', ''), loc, funcs, globals, arrays, usesState, probe, upd, drw, reactive, verdict, score: +score.toFixed(1) };
}

const rows = files.map(analyze).sort((a, b) => b.score - a.score);

const json = process.argv.includes('--json');
const topN = (() => { const i = process.argv.indexOf('--top'); return i >= 0 ? +process.argv[i + 1] : 30; })();

if (json) { console.log(JSON.stringify(rows, null, 2)); process.exit(0); }

const byVerdict = {};
for (const r of rows) byVerdict[r.verdict] = (byVerdict[r.verdict] || 0) + 1;

console.log(`\n${rows.length} carts analyzed\n`);
console.log('verdict breakdown:');
for (const [k, v] of Object.entries(byVerdict).sort((a, b) => b[1] - a[1]))
  console.log(`  ${k.padEnd(12)} ${v}`);

console.log(`\nspec candidates — top ${topN} by score:\n`);
const pad = (s, n) => String(s).padEnd(n);
console.log('  ' + pad('cart', 22) + pad('verdict', 11) + pad('loc', 6) + pad('fn', 4) + pad('glob', 6) + pad('arr', 5) + pad('upd', 5) + pad('drw', 5) + pad('state', 7) + 'score');
console.log('  ' + '-'.repeat(78));
for (const r of rows.slice(0, topN))
  console.log('  ' + pad(r.file, 22) + pad(r.verdict, 11) + pad(r.loc, 6) + pad(r.funcs, 4) + pad(r.globals, 6) + pad(r.arrays, 5) + pad(r.upd, 5) + pad(r.drw, 5) + pad(r.usesState ? 'yes' : '-', 7) + r.score);
console.log('');
