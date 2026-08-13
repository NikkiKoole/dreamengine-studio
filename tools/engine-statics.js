#!/usr/bin/env node
/*
 * engine-statics.js — how much PROCESS-GLOBAL MUTABLE STATE does the engine still hold?
 *
 * The measurement the per-instance-context refactor is sized and paced by (docs/HANDOFF.md →
 * the AUv3 lane, docs/design/ios-plan.md). An AUv3 puts every instance of a plug-in in ONE
 * process, so every mutable file-scope static in the engine is shared by every instance —
 * which is why two GarageBand tracks fight over one rack. This counts them, per file, and
 * reports the two facts that decide how hard the fix is:
 *
 *   · the NON-ZERO initialisers   — the only hand work (zero/NULL come free from a calloc)
 *   · the #define COLLISIONS      — a static whose name is reused as a local or a parameter
 *                                   somewhere in the translation unit, which is the one thing
 *                                   that makes `#define name (ctx->name)` miserable
 *
 * ⚠ WHY THIS IS A TOOL AND NOT A GREP. The figure this replaces (`91` statics in sound.h) came
 * from `grep -E '^static [^(]*;$'`, which requires the declaration to END at the semicolon —
 * so it silently dropped every declaration carrying a trailing `// comment`, i.e. most of them.
 * It undercounted by ~2.8x and nearly sized the refactor out of existence. This asks the
 * COMPILER instead (clang's own AST), so it cannot miss a declaration shape.
 *
 * USAGE
 *   node tools/engine-statics.js              the table + the non-zero initialisers + collisions
 *   node tools/engine-statics.js --quiet       one line (the repo-doctor / progress-meter row)
 *   node tools/engine-statics.js --json        machine-readable
 *   node tools/engine-statics.js --check       self-test: known answers on a fixture TU
 *   node tools/engine-statics.js --file <f>    just one engine file
 *
 * As the refactor lands, every number here should fall toward zero. A file at 0 is a file whose
 * state is per-instance; the job is done when the engine files are all 0.
 *
 * COST: it compiles studio.c with -fsyntax-only and dumps its AST (~10s, a few hundred MB
 * streamed, never written to disk). It does NOT need Raylib (it builds the DE_NO_RAYLIB path).
 */
'use strict';

const { spawn, execSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');

// The engine files that hold state. sound.h + sync.h + mic.h + midi_output.h are only ever
// compiled INSIDE studio.c, which is why one TU covers them all.
const ENGINE_FILES = [
  'runtime/sound.h',
  'runtime/studio.c',
  'runtime/sync.h',
  'runtime/mic.h',
  'runtime/midi_output.h',
];

const CFLAGS = [
  '-I', 'runtime', '-I', 'build',
  '-DDE_NO_RAYLIB',
  '-DSCREEN_W=320', '-DSCREEN_H=200', '-DSCALE=2',
  '-DMAP_W=128', '-DMAP_H=64', '-DCELL_W=16', '-DCELL_H=16',
];

/* ---------------------------------------------------------------- AST parsing */

// A clang -ast-dump line looks like:
//   |-VarDecl 0x… <line:451:1, col:30> col:22 used wavcap_buf 'float *' static cinit
// The location block only names the FILE when it changes, so we carry the last one forward.
function parseDump(text) {
  const rows = [];
  let curFile = '?';
  let curLine = 0;
  for (const L of text.split('\n')) {
    if (!/(VarDecl|FunctionDecl)/.test(L)) continue;
    const loc = L.match(/<([^>]*)>/);
    if (loc) {
      const first = loc[1].split(',')[0];
      const m = first.match(/^(.*?):(\d+):(\d+)$/);
      if (m && m[1] !== 'line' && m[1] !== 'col') { curFile = m[1]; curLine = +m[2]; }
      else { const lm = first.match(/^line:(\d+)/); if (lm) curLine = +lm[1]; }
    }
    const q = L.indexOf("'");
    if (q < 0) continue;
    const name = (L.slice(0, q).trim().match(/([A-Za-z_][A-Za-z0-9_]*)$/) || [])[1];
    if (!name) continue;
    const type = (L.slice(q + 1).match(/^((?:[^']|'')*)'/) || [])[1] || '';
    const tail = L.slice(q);
    const indent = L.match(/^[^A-Za-z]*/)[0];
    rows.push({
      file: relative(curFile), line: curLine, name, type,
      kind: /ParmVarDecl/.test(L) ? 'parm' : /FunctionDecl/.test(L) ? 'fn' : 'var',
      // a top-level decl starts the line; anything nested is indented with `| ` runs
      topLevel: /^[|`]-/.test(indent + L.trim()[0] === undefined ? L : L) && /^[|`]-/.test(L),
      isStatic: /\bstatic\b/.test(tail),
      isExtern: /\bextern\b/.test(tail),
      isConst: /\bconst\b/.test(type),
      hasInit: /\bcinit\b/.test(L),
    });
  }
  return rows;
}

function relative(f) {
  if (!f || f === '?') return f;
  const abs = path.isAbsolute(f) ? f : path.resolve(ROOT, f);
  const rel = path.relative(ROOT, abs);
  return rel.startsWith('..') ? f : rel;
}

function dumpAst(cb) {
  const args = ['-Xclang', '-ast-dump', '-fno-color-diagnostics', '-fsyntax-only',
                'runtime/studio.c', ...CFLAGS];
  const p = spawn('clang', args, { cwd: ROOT });
  let buf = '';
  let kept = '';
  p.stdout.setEncoding('utf8');
  p.stdout.on('data', (chunk) => {
    // stream: keep only decl lines, so the ~500 MB dump never lands in memory whole
    buf += chunk;
    const lines = buf.split('\n');
    buf = lines.pop();
    for (const L of lines) if (/(VarDecl|FunctionDecl)/.test(L)) kept += L + '\n';
  });
  p.on('close', (code) => {
    if (buf && /(VarDecl|FunctionDecl)/.test(buf)) kept += buf + '\n';
    if (!kept) { console.error('engine-statics: clang produced no AST (exit ' + code + ')'); process.exit(2); }
    cb(kept);
  });
}

/* ------------------------------------------------- initialiser values (source) */

// Is the initialiser a plain zero? Those come free from a calloc; everything else is hand work.
const ZERO = /^(0|0\.0f?|0u|0L|0x0u?|NULL|false|\{0\}|\{\{0\}\}|\{\}|"")$/;

// One declaration can declare SEVERAL names — `static float drop_lpL = 0.0f, drop_lpR = 0.0f;`
// — and they can have different initialisers, so the value has to be found per NAME rather than
// by taking everything after the first `=`. (Doing the naive thing scored every declarator on
// such a line as non-zero and inflated the hand-work figure by a third.)
function initialiserValue(file, line, name) {
  const src = readSource(file);
  if (!src || !src[line - 1]) return null;
  let text = src[line - 1].replace(/\/\/.*$/, '');
  let i = line - 1;
  while (!/;/.test(text) && i + 1 < src.length) { i++; text += ' ' + src[i].replace(/\/\/.*$/, ''); }
  text = text.replace(/;[\s\S]*$/, '');

  // split on top-level commas (not inside braces, parens or brackets)
  const parts = [];
  let depth = 0, start = 0;
  for (let j = 0; j < text.length; j++) {
    const c = text[j];
    if (c === '{' || c === '(' || c === '[') depth++;
    else if (c === '}' || c === ')' || c === ']') depth--;
    else if (c === ',' && depth === 0) { parts.push(text.slice(start, j)); start = j + 1; }
  }
  parts.push(text.slice(start));

  const mine = parts.find(p => new RegExp('\\b' + name + '\\b').test(p));
  if (mine === undefined) return null;
  const eq = mine.indexOf('=');
  if (eq < 0) return null;                       // this declarator has no initialiser
  return mine.slice(eq + 1).replace(/\s+/g, '');
}

const _srcCache = {};
function readSource(file) {
  if (!(file in _srcCache)) {
    const p = path.resolve(ROOT, file);
    _srcCache[file] = fs.existsSync(p) ? fs.readFileSync(p, 'utf8').split('\n') : null;
  }
  return _srcCache[file];
}

/* ------------------------------------------------------------------- analysis */

function analyze(rows, only) {
  const files = only ? [only] : ENGINE_FILES;
  const inScope = (r) => files.includes(r.file);

  const statics = rows.filter(r => r.kind === 'var' && r.topLevel && r.isStatic && !r.isConst && inScope(r));
  // de-dupe: clang prints a decl once, but a name can legitimately appear twice across files
  const seen = new Set();
  const uniq = statics.filter(r => { const k = r.file + ':' + r.name; if (seen.has(k)) return false; seen.add(k); return true; });

  const localStatics = rows.filter(r => r.kind === 'var' && !r.topLevel && r.isStatic && !r.isConst && inScope(r));

  const names = new Set(uniq.map(r => r.name));
  // a collision = the same identifier used as a local var or a parameter ANYWHERE in the TU
  const collisions = {};
  for (const r of rows) {
    if (r.kind === 'fn') continue;
    if (r.topLevel && r.kind === 'var') continue;
    if (!names.has(r.name)) continue;
    (collisions[r.name] = collisions[r.name] || []).push(r.file);
  }

  const perFile = {};
  for (const f of files) perFile[f] = { total: 0, nonZeroInit: 0, zeroOrNone: 0, localStatics: 0, nonZero: [] };
  for (const r of uniq) {
    const e = perFile[r.file]; if (!e) continue;
    e.total++;
    const v = initialiserValue(r.file, r.line, r.name);
    if (v === null || ZERO.test(v) || /^\{0[,0\s]*\}$/.test(v)) e.zeroOrNone++;
    else { e.nonZeroInit++; e.nonZero.push({ line: r.line, name: r.name, type: r.type, value: v.length > 44 ? v.slice(0, 44) + '…' : v }); }
  }
  for (const r of localStatics) if (perFile[r.file]) perFile[r.file].localStatics++;

  return { perFile, collisions, files };
}

/* ---------------------------------------------------------------------- output */

function report(res, opts) {
  const { perFile, collisions, files } = res;
  const tot = (k) => files.reduce((s, f) => s + perFile[f][k], 0);

  if (opts.json) { console.log(JSON.stringify(res, null, 1)); return; }

  if (opts.quiet) {
    const c = Object.keys(collisions).length;
    console.log(`engine statics: ${tot('total')} mutable file-scope · ${tot('nonZeroInit')} non-zero initialisers · ` +
                `${tot('localStatics')} function-local · ${c} #define collision${c === 1 ? '' : 's'}`);
    return;
  }

  console.log('PROCESS-GLOBAL MUTABLE STATE IN THE ENGINE  (clang AST — every declaration shape, comments and all)\n');
  const w = Math.max(...files.map(f => f.length));
  console.log('  ' + 'file'.padEnd(w) + '   statics   non-zero init   fn-local');
  console.log('  ' + '-'.repeat(w) + '   -------   -------------   --------');
  for (const f of files) {
    const e = perFile[f];
    console.log('  ' + f.padEnd(w) + '   ' + String(e.total).padStart(7) + '   ' +
                String(e.nonZeroInit).padStart(13) + '   ' + String(e.localStatics).padStart(8));
  }
  console.log('  ' + 'TOTAL'.padEnd(w) + '   ' + String(tot('total')).padStart(7) + '   ' +
              String(tot('nonZeroInit')).padStart(13) + '   ' + String(tot('localStatics')).padStart(8));

  console.log('\n  statics            → one member each in the per-instance context struct');
  console.log('  non-zero init      → the hand work: these become an init function');
  console.log('  fn-local           → NOT fixed by a #define; the declaration itself has to move');

  const cn = Object.keys(collisions);
  console.log('\n#define COLLISIONS  (a static name also used as a local or a parameter in the TU)');
  if (!cn.length) console.log('  none — `#define name (ctx->name)` is safe for every static above.');
  else for (const n of cn) console.log('  ' + n.padEnd(24) + [...new Set(collisions[n])].join(' '));

  if (opts.verbose !== false) {
    console.log('\nNON-ZERO INITIALISERS  (the init function this refactor has to write)');
    for (const f of files) {
      const e = perFile[f];
      if (!e.nonZero.length) continue;
      console.log('  ' + f);
      for (const v of e.nonZero) console.log('    ' + String(v.line).padStart(5) + '  ' + v.name.padEnd(24) + ' = ' + v.value);
    }
  }
}

/* ------------------------------------------------------------------ self-test */

function selfCheck() {
  // A known-answer fixture: a TU whose every declaration shape is one this tool has to get
  // right, INCLUDING the ones the grep it replaces got wrong (trailing comments, multi-line
  // declarations, an initialiser containing a brace, a function-local static, a shadowed name).
  const dir = fs.mkdtempSync(path.join(require('os').tmpdir(), 'engstat-'));
  const f = path.join(dir, 'fixture.c');
  fs.writeFileSync(f, `
static int plain;                          // counted: no initialiser
static int commented = 3;                  // counted: NON-ZERO, and the old grep MISSED this shape
static int zeroed = 0;                     // counted: zero initialiser, free from a calloc
static float multi_a = 1.0f, multi_b = 0.0f;
static const int constant = 7;             // NOT counted: const is not mutable state
extern int external;                       // NOT counted: not ours
static char buf[8] = {0};                  // counted: brace-zero is still zero
static int  wrapped =
    5;                                     // counted: NON-ZERO across two lines
static void fn(int shadow) {               // 'shadow' collides with the static below
    static int fn_local = 2;               // function-local static
    (void)shadow; (void)fn_local;
}
static int shadow = 0;
int main(void){ fn(plain+commented+zeroed+wrapped+buf[0]+constant+shadow+(int)multi_a+(int)multi_b); return 0; }
`);
  const out = execSync(`clang -Xclang -ast-dump -fno-color-diagnostics -fsyntax-only ${JSON.stringify(f)} 2>/dev/null || true`,
                       { maxBuffer: 1 << 28 }).toString();
  const rows = parseDump(out).map(r => ({ ...r, file: r.file.replace(/.*\//, '') }));
  const saveRoot = ENGINE_FILES.slice();
  ENGINE_FILES.length = 0; ENGINE_FILES.push('fixture.c');
  _srcCache['fixture.c'] = fs.readFileSync(f, 'utf8').split('\n');
  const res = analyze(rows, null);
  ENGINE_FILES.length = 0; ENGINE_FILES.push(...saveRoot);

  const e = res.perFile['fixture.c'];
  const checks = [
    ['counts the plain static',                 () => e.total >= 1],
    ['counts a static with a trailing comment', () => !!e.nonZero.find(v => v.name === 'commented')],
    ['the old grep shape is the one it fixes',  () => e.nonZero.find(v => v.name === 'commented').value === '3'],
    ['zero initialiser is not hand work',       () => !e.nonZero.find(v => v.name === 'zeroed')],
    ['brace-zero is not hand work',             () => !e.nonZero.find(v => v.name === 'buf')],
    ['a multi-line initialiser is caught',      () => !!e.nonZero.find(v => v.name === 'wrapped')],
    // the multi-declarator trap: one line, two names, only ONE of them non-zero
    ['multi-declarator: non-zero half caught',  () => e.nonZero.find(v => v.name === 'multi_a').value === '1.0f'],
    ['multi-declarator: zero half NOT counted', () => !e.nonZero.find(v => v.name === 'multi_b')],
    ['const is excluded',                       () => e.total === 8 || !JSON.stringify(e).includes('constant')],
    ['extern is excluded',                      () => !JSON.stringify(e).includes('external')],
    ['function-local static is separated',      () => e.localStatics === 1],
    ['a shadowing parameter is a collision',    () => 'shadow' in res.collisions],
    ['a non-shadowed static is not',            () => !('plain' in res.collisions)],
  ];
  let pass = 0;
  for (const [name, fn] of checks) {
    let ok = false; try { ok = !!fn(); } catch (_) { ok = false; }
    if (ok) pass++; else console.log('  ✗ ' + name);
  }
  fs.rmSync(dir, { recursive: true, force: true });
  console.log(`engine-statics --check: ${pass}/${checks.length} known answers correct`);
  process.exit(pass === checks.length ? 0 : 1);
}

/* ---------------------------------------------------------------------- main */

const argv = process.argv.slice(2);
const opts = {
  quiet: argv.includes('--quiet'),
  json: argv.includes('--json'),
  verbose: !argv.includes('--quiet'),
};
if (argv.includes('--check')) { selfCheck(); }
else {
  const fi = argv.indexOf('--file');
  const only = fi >= 0 ? relative(argv[fi + 1]) : null;
  dumpAst((text) => report(analyze(parseDump(text), only), opts));
}
