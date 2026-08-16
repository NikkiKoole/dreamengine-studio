#!/usr/bin/env node
// api-usage.js — cross-check studio.h API usage across all carts, and the
// "four places" doc coverage (studio.h ↔ studioDocs.js ↔ shell.js).
//
//   node tools/api-usage.js            # full table, least-used first
//   node tools/api-usage.js --unused   # only never/rarely used (≤1 cart, nothing elsewhere)
//   node tools/api-usage.js --where de_state_for   # WHO calls it, outside tools/carts
//
// Counting is textual (word-boundary + open paren) — good enough because cart code shares one
// namespace with the API. Findings snapshot + interpretation: docs/design/api-usage-audit.md
//
// TWO CORPORA, because one was a trap. The `carts` column is tools/carts/*.c, the headline
// number. `elsewhere` is every OTHER first-party call site: the cart-land shelf (runtime/*.h per
// tools/cart-land-headers.js), the generated per-cart contexts (runtime/<slug>_state.h), one
// cart's private modules (runtime/lockup|tenement|isoroom), and the generated multi-cart app
// dispatcher (tools/build-app.js). Until 2026-08 only the first was
// scanned, so an API called from a SHARED HEADER read as a flat zero — and four of the ten zeros
// were exactly that (de_state_for + de_state_for_saved via cart_ctx.h/ui.h/…, instrument_bandlimit
// via acid303.h, de_switch_cart from the app shim). That is not a cosmetic miscount: this table's
// standing advice is "ship a cart for it or cut it", so a false zero reads as an instruction to
// cut the per-instance-state seam the AUv3 work is built on. A zero in BOTH columns now means
// nobody calls it anywhere.
//
// Engine internals are deliberately NOT a corpus: sound.h/studio.c DEFINE these functions, so
// scanning them would score every name as used and destroy the signal the table exists for.
//
// What the textual match still cannot see, in BOTH columns: a name that is not followed by `(`.
// cart_ctx.h passes `de_state_for` to DE_CTX_BLOCK as a macro ARGUMENT, so every cart using that
// macro is a transitive consumer none of these numbers count. Matching bare names instead would
// pick up the prose (three headers mention de_state_for in comments), which is worse — a
// documented undercount beats a silent overcount. Read `elsewhere` as a floor, not a census.

'use strict';
const fs = require('fs');
const path = require('path');
const { shelf, privateModuleDirs, generatedCartContexts } = require('./cart-land-headers');

const ROOT = path.join(__dirname, '..');
const argv = process.argv.slice(2);
const onlyUnused = argv.includes('--unused');
const selfcheck = argv.includes('--selfcheck');
const whereIdx = argv.indexOf('--where');
const whereFn = whereIdx !== -1 ? argv[whereIdx + 1] : null;
if (whereIdx !== -1 && !whereFn) {
  console.error('usage: node tools/api-usage.js --where <function>');
  process.exit(1);
}

// ── 1. API surface from studio.h ────────────────────────────────────────────
const hdr = fs.readFileSync(path.join(ROOT, 'runtime/studio.h'), 'utf8');
const names = new Set();
for (const m of hdr.matchAll(/^[A-Za-z_][\w \*]*?\b([a-z_][a-z0-9_]*)\s*\([^)]*\)\s*;/gm)) {
  names.add(m[1]);
}

// ── 2. usage counts across carts + the other first-party call sites ─────────
// Strip comments and string literals BEFORE matching, or the scan counts prose. This is not a
// rounding error: 2306 of the raw matches across the table were comments, and two functions were
// scored as used purely by text SAYING they are not — isoroom's `// NOT `paused` — studio.h
// already has a paused() built-in`, and a voxshift docblock noting harmonize_mic has no home cart.
// Headers are worse than carts (cart-dupes.js measured 56% of runtime/*.h "vocabulary" as prose),
// and headers are the corpus this tool just gained. Same routine as cart-dupes.js's decomment();
// kept local rather than shared because that one is a CLI, not a lib.
function decomment(src) {
  let out = '', i = 0; const n = src.length;
  while (i < n) {
    const c = src[i], d = src[i + 1];
    if (c === '/' && d === '*') { i += 2; while (i < n && !(src[i] === '*' && src[i + 1] === '/')) { if (src[i] === '\n') out += '\n'; i++; } i += 2; continue; }
    if (c === '/' && d === '/') { while (i < n && src[i] !== '\n') i++; continue; }
    if (c === '"' || c === '\'') { const q = c; out += ' '; i++; while (i < n && src[i] !== q) { if (src[i] === '\\') i++; if (src[i] === '\n') out += '\n'; i++; } i++; continue; }
    out += c; i++;
  }
  return out;
}
const countIn = (src, n) => (src.match(new RegExp('\\b' + n + '\\s*\\(', 'g')) || []).length;

if (selfcheck) { runSelfcheck(); return; }   // pure rules — asserted without reading the repo

const cartsDir = path.join(ROOT, 'tools/carts');
const carts = fs.readdirSync(cartsDir).filter((f) => f.endsWith('.c'));
const counts = {};
for (const n of names) counts[n] = { carts: 0, calls: 0, other: 0, where: [] };
for (const f of carts) {
  const src = decomment(fs.readFileSync(path.join(cartsDir, f), 'utf8'));
  for (const n of names) {
    const c = countIn(src, n);
    if (c) { counts[n].carts++; counts[n].calls += c; }
  }
}

// the `elsewhere` corpus: shared shelf headers, one cart's private modules, the generated app shim
const otherFiles = [
  ...shelf().map((h) => 'runtime/' + h),
  ...generatedCartContexts().map((h) => 'runtime/' + h),
  ...privateModuleDirs().flatMap((d) =>
    fs.readdirSync(path.join(ROOT, 'runtime', d))
      .filter((f) => f.endsWith('.h') || f.endsWith('.c'))
      .map((f) => `runtime/${d}/${f}`)),
  'tools/build-app.js',   // the generated multi-cart dispatcher lives in a template string here
].filter((rel) => fs.existsSync(path.join(ROOT, rel)));

for (const rel of otherFiles) {
  const src = decomment(fs.readFileSync(path.join(ROOT, rel), 'utf8'));
  for (const n of names) {
    const c = countIn(src, n);
    if (c) { counts[n].other += c; counts[n].where.push(`${rel} (${c})`); }
  }
}

// ── --where: who calls ONE function, outside tools/carts ────────────────────
if (whereFn) {
  if (!counts[whereFn]) {
    console.error(`not a studio.h function: ${whereFn}`);
    process.exit(1);
  }
  const c = counts[whereFn];
  console.log(`${whereFn} — ${c.carts} cart(s) / ${c.calls} call(s) in tools/carts`);
  if (!c.where.length) console.log('  elsewhere: nothing');
  else {
    console.log(`  elsewhere (${c.other} call(s)):`);
    for (const w of c.where) console.log('    ' + w);
  }
  return;
}

const sorted = Object.entries(counts).sort(
  (a, b) => a[1].carts - b[1].carts || a[1].other - b[1].other || a[1].calls - b[1].calls
);
// "unused" now means unused ANYWHERE — a header-only call site is a real consumer.
const rows = onlyUnused ? sorted.filter(([, c]) => c.carts <= 1 && c.other === 0) : sorted;

console.log('API function'.padEnd(22) + 'carts'.padStart(6) + 'calls'.padStart(7) + 'elsewhere'.padStart(11));
for (const [n, c] of rows) {
  console.log(n.padEnd(22) + String(c.carts).padStart(6) + String(c.calls).padStart(7)
    + String(c.other || '').padStart(11));
}
console.log(`\n${names.size} API functions, ${carts.length} carts + ${otherFiles.length} other first-party files scanned`);
console.log(`elsewhere = cart-land shelf + generated cart contexts + private cart modules + the app shim; --where <fn> names them.`);
const falseZeros = sorted.filter(([, c]) => c.carts === 0 && c.other > 0).map(([n]) => n);
if (falseZeros.length)
  console.log(`0 carts but called elsewhere (NOT dead weight): ${falseZeros.join(', ')}`);

// ── 3. four-places coverage: studioDocs.js + shell.js ───────────────────────
const docsSrc = fs.readFileSync(path.join(ROOT, 'editor/src/studioDocs.js'), 'utf8');
const docKeys = new Set();
for (const m of docsSrc.matchAll(/^\s{2}([A-Za-z_][A-Za-z0-9_]*)\s*:\s*\{/gm)) docKeys.add(m[1]);

const shellSrc = fs.readFileSync(path.join(ROOT, 'editor/src/shell.js'), 'utf8');
const shellKeys = new Set();
for (const m of shellSrc.matchAll(/keys:\s*\[([^\]]*)\]/g)) {
  for (const k of m[1].matchAll(/['"]([A-Za-z_][A-Za-z0-9_]*)['"]/g)) shellKeys.add(k[1]);
}

const missDocs = [...names].filter((n) => !docKeys.has(n));
const missShell = [...names].filter((n) => !shellKeys.has(n));
console.log('\nIn studio.h but missing from studioDocs.js: ' + (missDocs.join(', ') || '(none)'));
console.log('In studio.h but missing from shell.js keys: ' + (missShell.join(', ') || '(none)'));

// ── --selfcheck: known answers for the counting rules ───────────────────────
// Both bugs this fixture pins were SILENT: the table stayed plausible while the numbers were
// wrong. #1, the scan read tools/carts only, so an API called from a shared header scored 0 and
// read as dead weight. #2, it matched raw source, so comments and string literals counted as
// calls — including two carts whose only "use" is prose saying the function is NOT used. A
// counting tool whose failure mode is a believable number needs its rules asserted, not eyeballed.
function runSelfcheck() {
  let pass = 0, fail = 0;
  const ok = (cond, label) => { cond ? pass++ : fail++; console.log(`  ${cond ? '\x1b[32m✓\x1b[0m' : '\x1b[31m✗\x1b[0m'} ${label}`); };
  const n = (src, name, want, label) => ok(countIn(decomment(src), name) === want,
    `${label}  [want ${want}, got ${countIn(decomment(src), name)}]`);

  console.log('counting: a call counts, prose does not');
  n('paused();', 'paused', 1, 'a plain call');
  n('  x = paused() ? 1 : 0;', 'paused', 1, 'a call mid-expression');
  n('paused(); paused();', 'paused', 2, 'two calls on one line');
  n('// call paused() first\n', 'paused', 0, 'a // comment does NOT count');
  n('/* paused() is a thing */', 'paused', 0, 'a /* */ comment does NOT count');
  n('printh("paused() has no home");', 'paused', 0, 'a string literal does NOT count');
  n('/* a\n * paused()\n */\nx;', 'paused', 0, 'a multi-line comment does NOT count');

  console.log('\nthe two real miscounts that shipped (regression guards)');
  n('static int walk_paused = 0;  // NOT `paused` — studio.h already has a paused() built-in',
    'paused', 0, 'isoroom: a comment WARNING against the name scored as a use');
  n('    "Once harmonize_mic() has a home cart, cross-link it here.",',
    'harmonize_mic', 0, 'voxshift: a docblock string scored as a use');

  console.log('\nword boundaries + the documented undercount');
  n('boxrig_draw(a);', 'draw', 0, 'a suffixed name is NOT the API fn');
  n('de_draw(a);', 'draw', 0, 'a prefixed name is NOT the API fn');
  n('draw (a);', 'draw', 1, 'whitespace before the paren still counts');
  n('DE_CTX_BLOCK(lc, Uc, LIST, de_state_for)', 'de_state_for', 0,
    'a macro ARGUMENT does not count — the known, documented undercount');

  console.log('\njs template literals survive (the app shim keeps its call)');
  n('const c = `\n  de_switch_cart(i);  // engine swaps the sound world\n`;', 'de_switch_cart', 1,
    'generated C inside a backtick template is still code');
  n('// de_switch_cart(ctx) swaps the rack\n', 'de_switch_cart', 0,
    'a JS comment about it is not');

  console.log('\ncorpus split (tools/cart-land-headers.js)');
  const sh = shelf();
  ok(sh.includes('acid303.h'), 'shelf includes acid303.h — the instrument_bandlimit call site');
  ok(sh.includes('ui.h'), 'shelf includes ui.h — the de_state_for call site');
  ok(!sh.includes('studio.h'), 'shelf EXCLUDES studio.h — declarations are not calls');
  ok(!sh.includes('sound.h'), 'shelf EXCLUDES sound.h — definitions would score every name used');
  ok(!sh.some((h) => /_state\.h$/.test(h)), 'shelf EXCLUDES generated _state.h (no doc row owed)');
  ok(generatedCartContexts().includes('acidcandy_state.h'),
    '…but the call-site corpus KEEPS it — the only de_state_for_saved call in the repo');

  console.log(`\napi-usage --selfcheck: ${pass}/${pass + fail} known answers correct`);
  process.exit(fail ? 1 : 0);
}
