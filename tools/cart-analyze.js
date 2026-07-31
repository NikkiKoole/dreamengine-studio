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
//   node tools/cart-analyze.js --selfcheck   assert the CHECKER (known-answer fixture, 23)
const fs = require('fs');
const path = require('path');

// Overridable so --selfcheck can analyze a tiny FIXTURE shelf (tools/fixtures/cart-analyze/).
// CART_EXT lets fixture carts be `.c.txt`: never compiled, and a real `.c` gets indexed by clangd
// and read as a cart by anything globbing for sources.
const dir = process.env.DE_ANALYZE_CARTS_DIR || path.join(__dirname, 'carts');
const CART_EXT = process.env.DE_ANALYZE_CART_EXT || '.c';
const files = fs.readdirSync(dir).filter(f => f.endsWith(CART_EXT)).sort();

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

  return { file: file.slice(0, -CART_EXT.length), loc, funcs, globals, arrays, usesState, probe, upd, drw, reactive, verdict, score: +score.toFixed(1) };
}

// ---- --selfcheck: assert the CHECKER against known answers ------------------------
// See docs/guides/checks-and-oracles.md "Self-test the checker". Re-runs the tool as a child with
// DE_ANALYZE_* pointed at tools/fixtures/cart-analyze/, one fixture cart per verdict and per
// counting rule, and asserts what it must say.
//
// WHY. The verdict is a FALL-THROUGH CHAIN and its ORDER is the judgement: `simple` is tested
// first, so a tiny widget cart is `simple` and not `reactive` — reorder it and the answer flips
// for a whole class of carts. Underneath, every metric is a line regex over decommented source,
// and two of those rules exist purely to stop the score inflating: commented-out scratch code
// must not count, and `static const` tables are DATA, not the cart's mutable state. Both are
// silent when wrong: nothing breaks, carts just rank in the wrong order and the spec backlog
// points somewhere useless.
if (process.argv.includes('--selfcheck')) {
  const cp = require('child_process');
  const FX = path.join(__dirname, 'fixtures', 'cart-analyze');
  let raw;
  try {
    raw = cp.execFileSync(process.execPath, [__filename, '--json'], {
      env: { ...process.env,
             DE_ANALYZE_CARTS_DIR: path.join(FX, 'carts'),
             DE_ANALYZE_CART_EXT:  '.c.txt' },
      encoding: 'utf8', maxBuffer: 1 << 24,
    });
  } catch (e) { raw = e.stdout; }
  const rows = JSON.parse(raw);
  const R = {};
  for (const r of rows) R[r.file] = r;

  const T = [];
  const t = (n, ok) => T.push({ n, ok });
  const v = (c) => (R[c] || {}).verdict;

  t('parsed the fixture shelf  [blind-pass guard]', rows.length === 11 && !!R.tiny);

  // ── the five verdicts, each on a cart built to land in exactly one branch
  t('verdict: small + few globals + light update → simple  [no spec needed]', v('tiny') === 'simple');
  // statey carries 5 globals and no arrays SO THAT the second stateful branch cannot also claim
  // it — otherwise deleting this branch changes nothing and the assertion is vacuous.
  t('verdict: de_state() → stateful  [step()+expect spec]',
    v('statey') === 'stateful' && R.statey.globals === 5 && R.statey.arrays === 0);
  t('verdict: globals+arrays+update-heavy → stateful, without de_state  [the 2nd branch]',
    v('bigstate') === 'stateful' && R.bigstate.usesState === false);
  t('verdict: draw-dominant → procedural  [golden / frame()+probe spec]', v('proc') === 'procedural');
  t('verdict: widget/audio machinery → reactive  [light spec or none]', v('react') === 'reactive');
  t('verdict: everything else → mixed  [the fall-through]', v('mixed') === 'mixed');
  t('verdict: draw-leaning but under the drw>=60 floor stays mixed  [threshold guard]',
    v('drawish') === 'mixed' && R.drawish.drw > R.drawish.upd * 1.8 && R.drawish.drw < 60);

  // ── CHAIN ORDER. This is the judgement, not a detail: the same cart gets a different answer
  //    depending on which branch is asked first.
  t('chain: a TINY cart that is also reactive reads simple, not reactive  [order matters]',
    v('tinyreactive') === 'simple' && R.tinyreactive.reactive === true);

  // ── decomment(): commented-out code must not inflate any metric
  t('decomment: commented-out statics are not globals',
    R.commented.globals === 1 && R.commented.arrays === 0);
  t('decomment: a commented-out function is not counted', R.commented.funcs === 0);
  t('decomment: ui_button/note_on inside a comment do not make a cart reactive',
    R.commented.reactive === false);
  t('decomment: an S-> inside a comment does not make a cart stateful',
    R.commented.usesState === false && v('commented') === 'simple');

  // ── static const is DATA, not state
  t('globals: `static const` tables are excluded  [5 tables + a scalar, still 2 globals]',
    R.conster.globals === 2);
  t('globals: ...and `static inline` helpers too', R.conster.globals === 2 && R.conster.funcs === 1);
  t('globals: but a mutable `static` ARRAY is counted, in both globals and arrays',
    R.conster.arrays === 1);

  // ── update() vs draw() are measured apart — the whole basis of stateful-vs-procedural
  t('bodies: update() and draw() line counts are independent',
    R.proc.drw === 64 && R.proc.upd === 1 && R.react.upd === 26 && R.react.drw === 6);

  // ── the SCORE formula, recomputed from the reported metrics. This pins the weights AND both
  //    dampeners at once; a changed constant fails here even if every verdict still agrees.
  const expect = (r) => {
    let s = r.loc * 0.01 + r.funcs * 0.4 + r.globals * 0.8 + r.arrays * 1.5 + r.upd * 0.05 +
            (r.usesState ? 4 : 0);
    if (r.verdict === 'simple')   s *= 0.2;
    if (r.verdict === 'reactive') s *= 0.5;
    if (r.probe) s += 2;
    return +s.toFixed(1);
  };
  t('score: every row matches the documented formula', rows.every(r => r.score === expect(r)));
  t('score: `simple` is dampened x0.2  [not a spec candidate]',
    Math.abs(R.tiny.score - +(1.69 * 0.2).toFixed(1)) < 0.06);
  t('score: `reactive` is dampened x0.5', Math.abs(R.react.score - +(6.51 * 0.5).toFixed(1)) < 0.06);
  t('score: a probe fn adds exactly +2  [same cart shape, one extra fn]',
    R.probey.probe === true && Math.abs((R.probey.score - R.mixed.score) - 2.01) < 0.02);
  t('score: results are ranked by score, descending',
    rows.every((r, i) => i === 0 || rows[i - 1].score >= r.score));
  t('score: the de_state() +4 bonus puts that cart on top; the bottom rows are simple',
    rows[0].file === 'statey' && rows[rows.length - 1].verdict === 'simple');

  const failed = T.filter(x => !x.ok);
  for (const x of T) console.log(`  ${x.ok ? '\x1b[32m✓\x1b[0m' : '\x1b[31m✗\x1b[0m'} ${x.n}`);
  console.log(failed.length
    ? `\x1b[31mcart-analyze --selfcheck FAILED\x1b[0m — ${failed.length} of ${T.length} expectations broken`
    : `cart-analyze --selfcheck: ${T.length}/${T.length} known answers correct`);
  process.exit(failed.length ? 1 : 0);
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
