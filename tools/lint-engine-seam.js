#!/usr/bin/env node
/* lint-engine-seam.js — the HOST↔ENGINE boundary, where three real bugs shipped in one day.
 *
 *   node tools/lint-engine-seam.js            report
 *   node tools/lint-engine-seam.js --quiet     PASS/FAIL only (CI)
 *   node tools/lint-engine-seam.js --strict    exit nonzero on any finding (repo-doctor)
 *   node tools/lint-engine-seam.js --selfcheck known-answer fixture
 *
 * WHY THIS EXISTS. The per-instance refactor (docs/design/engine-context.md) changed what host code
 * MEANS without changing what it SAYS, and every gate stayed green while the plug-in flickered and
 * the iOS app played silence. All three shapes below are invisible at the call site — that is
 * precisely what makes them expensive — and none of them is a broken link, a failed compile, or a
 * changed render, so no existing check could see them.
 *
 * A. TWO ENGINES IN ONE HOST.  `de_instance_create` used to return the same SINGLETON on every call,
 *    so it did not matter which component asked for the engine. It ALLOCATES now, and every extra
 *    caller is a separate rack. Three components were quietly doing it: the AUv3 panel booted its
 *    own engine (the maker's flickering), AudioEngine rendered an engine nobody touched (total
 *    silence on device), and GameHost booted a third purely to ask two questions — on the strength
 *    of a comment reading "de_instance_create is idempotent", true when written.
 *    THE RULE: one engine per rack, created by whoever OWNS the rack, PASSED to everything else.
 *    A file may create one only if it says so, with a `de:engine-owner` marker in a comment.
 *
 * B. A SEAM FUNCTION THAT IGNORES ITS HANDLE.  Six found in one day — de_resize, de_copy_frame,
 *    de_set_save_dir, de_framebuffer, de_screen_w, de_screen_h. Each took a `DeInstance *`, dropped
 *    it with `(void)in`, and read the thread-local instead, which on the HOST's thread names the
 *    DEFAULT engine. It never fails loudly: the function compiles, returns plausible values, and
 *    silently drives the wrong rack — and the handle sitting in the signature makes it READ as done.
 *    A legitimate one (de_is_resizable reads a compile-time flag) says so with `seam-lint-ignore`.
 *    ⚠ It scans EVERY engine .c, not just studio.c. While it read one file, the four input-ring
 *    entry points in raylib_compat.c were outside its view — a check's blind spot is not a pass.
 *
 * C. A REDECLARED ENGINE FUNCTION WITH THE WRONG SIGNATURE.  `face.h` and 7 carts declared
 *    `extern void de_resize(int, int)` against a function that had taken a `DeInstance *` for weeks.
 *    That is undefined behaviour the compiler CANNOT see across translation units, and it surfaced
 *    as a crash with `in = 0xa7` — which was 167, the canvas width the cart had asked for.
 *    THE RULE: if a cart needs an extern for an engine function, THE SEAM IS MISSING AN API.
 *    ⚠ `extern` is not part of the defect — a bare prototype is the same UB, and `ios/` is where
 *    they live (a bridging header IS a hand-copied seam). Checking only `extern`, only under
 *    tools/carts + runtime, missed `ios/Sources/canvas.h`'s `const uint8_t *de_framebuffer(void)`
 *    against the engine's `const uint32_t *de_framebuffer(DeInstance *)` — wrong arity AND wrong
 *    return type, sitting in the tree the whole time this check was green.
 *
 * D. A SEAM FUNCTION THAT NEVER TOOK A HANDLE AT ALL.  B's blind twin, and structurally invisible
 *    to it: B looks for a handle being DISCARDED, so an entry point that never had one is not a
 *    finding — it is not even a candidate. `de_sync_position` was exactly this, and the header
 *    still records what it cost ("the FIRST instance swallowed the START edge and every other one
 *    joined mid-flow and stayed silent — two DAW tracks, one playing"). Its surviving twins are the
 *    MIDI trio. THE RULE: everything Swift can call names its engine, or says in a `seam-lint-ignore`
 *    comment why the state behind it is legitimately process-wide (the mic: one capture device).
 *
 * Everything it reports is a real defect shape with a real incident behind it; there are no style
 * findings here. `--selfcheck` carries a fixture for each check IN BOTH DIRECTIONS, because a lint
 * whose pattern has rotted prints the same green as a healthy repo.
 */
const fs = require('fs');
const path = require('path');
const ROOT = path.resolve(__dirname, '..');

const ARGV = process.argv.slice(2);
const QUIET = ARGV.includes('--quiet');
const STRICT = ARGV.includes('--strict');

/* ─────────────────────────────────────────────────────────────────── helpers ── */

function walk(dir, exts, out = []) {
  let ents = [];
  try { ents = fs.readdirSync(dir, { withFileTypes: true }); } catch (_) { return out; }
  for (const e of ents) {
    // ⚠ `build-mac` too, not just `build`. Xcode COPIES engine.h into the built framework, so the
    // products tree holds a frozen snapshot of an older seam — 14 arity "mismatches" that are just
    // last month's header. A lint that reports build output teaches people to ignore it.
    if (/^build(-|$)/.test(e.name) || e.name === 'gen' || e.name === 'node_modules' || e.name.startsWith('.')) continue;
    const p = path.join(dir, e.name);
    if (e.isDirectory()) walk(p, exts, out);
    else if (exts.some(x => e.name.endsWith(x))) out.push(p);
  }
  return out;
}
const rel = p => path.relative(ROOT, p);

// The comment block attached to the declaration at `i`, however long it is. A FIXED lookback was
// wrong in the direction that matters: both waivers here demand a REASON, reasons run to several
// lines, and a 4-line window silently failed to see one — the waiver looked ignored and the finding
// looked unwaivable. Walk back over contiguous comment/blank lines instead and stop at real code,
// so the block cannot reach past the previous declaration to borrow its waiver.
function commentBlockAbove(lines, i) {
  let j = i - 1;
  while (j >= 0 && /^\s*(\/\/|\*|\/\*|$)/.test(lines[j])) j--;
  return lines.slice(j + 1, i).join('\n');
}
// strip // and /* */ so a rule never fires on prose ABOUT the rule — this file's own header would
// otherwise trip every check it defines.
function decomment(src) {
  return src.replace(/\/\*[\s\S]*?\*\//g, m => m.replace(/[^\n]/g, ' '))
            .replace(/\/\/[^\n]*/g, '');
}

/* ─────────────────────────────────────────────── A. one engine per host ─────── */

// A file may create an engine only if it declares itself the owner. A marker beats an allowlist:
// it lives next to the code, a new host cannot forget to be added to it, and deleting the create
// without deleting the marker is harmless.
const OWNER_MARK = 'de:engine-owner';

function checkEngineOwners(files) {
  const out = [];
  for (const f of files) {
    const raw = fs.readFileSync(f, 'utf8');
    const src = decomment(raw);
    const hits = [...src.matchAll(/de_instance_create\s*\(/g)];
    if (!hits.length) continue;
    if (raw.includes(OWNER_MARK)) {
      // Declared owner — but it must create exactly ONE, unless it says otherwise. A second call
      // inside one owner is the same bug wearing a permission slip. `de:engine-owner multi` is for
      // the multi-instance PROBES, whose entire job is running several engines at once.
      if (hits.length > 1 && !/de:engine-owner\s+multi/.test(raw)) {
        out.push({ check: 'A', file: rel(f), line: lineOf(src, hits[1].index),
          msg: `creates ${hits.length} engines — an owner owns ONE. Extra racks render and sound independently, which is invisible until something goes quiet. Say "${OWNER_MARK} multi" if several is the point (an instance probe).` });
      }
      continue;
    }
    out.push({ check: 'A', file: rel(f), line: lineOf(src, hits[0].index),
      msg: `calls de_instance_create but is not a declared engine owner. One engine per rack, created by its owner and PASSED here — or add a "${OWNER_MARK}" comment if this file really does own one.` });
  }
  return out;
}
const lineOf = (src, idx) => src.slice(0, idx).split('\n').length;

/* ────────────────────────────────────────── B. seam ignores its handle ─────── */

function checkSeamHandles(file) {
  const out = [];
  let raw;
  try { raw = fs.readFileSync(file, 'utf8'); } catch (_) { return out; }
  const lines = raw.split('\n');
  for (let i = 0; i < lines.length; i++) {
    const L = lines[i];
    // a definition (not a prototype: it must open a brace) taking a DeInstance handle
    if (!/\bDeInstance\s*\*\s*in\b/.test(L) || !/\{/.test(L)) continue;
    // ⚠ THE BODY ENDS AT ITS CLOSING BRACE, not N lines later. A fixed lookahead window ran past the
    // end of a CORRECT function into the next one's `(void)in` and reported the good function — and a
    // gate that cries wolf is one people learn to skip, which is worse than not having it.
    let body = '', depth = 0;
    for (let j = i; j < lines.length; j++) {
      body += lines[j] + '\n';
      for (const c of lines[j]) { if (c === '{') depth++; else if (c === '}') depth--; }
      if (depth <= 0) break;
    }
    if (!/\(\s*void\s*\)\s*in\s*;/.test(body)) continue;
    // the waiver may sit in the body, or anywhere in the comment block above it
    if (/seam-lint-ignore/.test(body) || /seam-lint-ignore/.test(commentBlockAbove(lines, i))) continue;
    const name = (L.match(/([A-Za-z_]\w*)\s*\(\s*DeInstance/) || [])[1] || '?';
    out.push({ check: 'B', file: rel(file), line: i + 1,
      msg: `${name}() takes a DeInstance* and discards it with (void)in — it will silently act on whichever engine the CURRENT THREAD last entered, which on a host thread is the default one. Use the handle, or mark it "seam-lint-ignore" with the reason.` });
  }
  return out;
}

/* ───────────────────────────────────── C. cart-declared engine externs ─────── */

// The seam's real signatures, read from the header rather than hardcoded so this cannot drift.
function seamArity() {
  const m = new Map();
  for (const h of ['runtime/platform.h', 'runtime/studio.h']) {
    let src;
    try { src = decomment(fs.readFileSync(path.join(ROOT, h), 'utf8')); } catch (_) { continue; }
    for (const d of src.matchAll(/\b([A-Za-z_]\w*)\s*\(([^;{)]*)\)\s*;/g)) {
      const [, name, args] = d;
      if (!/^de_/.test(name)) continue;
      if (!m.has(name)) m.set(name, argc(args));
    }
  }
  return m;
}
const argc = a => (a.trim() === '' || a.trim() === 'void') ? 0 : a.split(',').length;

// `extern` is OPTIONAL here on purpose — a bare prototype in a header is the identical UB, and it is
// the shape the ios/ bridging headers use. The seam headers themselves are excluded by the caller:
// they are the definition this is measured against, not a redeclaration of it.
function checkCartExterns(files, arity) {
  const out = [];
  for (const f of files) {
    const src = decomment(fs.readFileSync(f, 'utf8'));
    for (const d of src.matchAll(/(?:^|\n)\s*(?:extern\s+)?[A-Za-z_][\w\s*]*?\b(de_\w+)\s*\(([^;)]*)\)\s*;/g)) {
      const [, name, args] = d;
      const real = arity.get(name);
      if (real === undefined) continue;                 // not a seam function we can check
      const got = argc(args);
      if (got === real) continue;                       // matches — still hand-rolled, but not UB
      // ⚠ line from the NAME, not the match start: the leading `\s*` swallows the blank lines a
      // decommented doc block leaves behind, so d.index lands on the PREVIOUS declaration.
      out.push({ check: 'C', file: rel(f), line: lineOf(src, d.index + d[0].indexOf(name)),
        msg: `declares its own "${name}(${got} arg${got === 1 ? '' : 's'})" but the engine defines ${real} — UNDEFINED BEHAVIOUR no compiler sees across translation units. If a cart needs this, the seam is missing an API (that is how canvas_resize was born).` });
    }
  }
  return out;
}

/* ────────────────────────────────── D. a seam function with no handle ───────── */

// Every de_* Swift can call must name the engine it means. The waiver is a MARKER next to the
// declaration, not a list in here — same reason as check A: a new entry point cannot forget to be
// added to a list that does not exist, and the reason stays where the reader is.
function checkHandleless(file, arity) {
  const out = [];
  let raw;
  try { raw = fs.readFileSync(file, 'utf8'); } catch (_) { return out; }
  const lines = raw.split('\n');
  const src = decomment(raw);
  for (const d of src.matchAll(/(?:^|\n)\s*[A-Za-z_][\w\s*]*?\b(de_\w+)\s*\(([^;)]*)\)\s*;/g)) {
    const [, name, args] = d;
    if (/DeInstance\s*\*/.test(args)) continue;              // names its instance — the whole point
    if (name === 'de_instance_create') continue;             // it RETURNS the handle; it cannot take one
    const line = lineOf(src, d.index + d[0].indexOf(name));   // the NAME's line — see check C
    // the reason may sit on the declaration itself or anywhere in its comment block
    if (/seam-lint-ignore/.test(lines[line - 1] || '') ||
        /seam-lint-ignore/.test(commentBlockAbove(lines, line - 1))) continue;
    out.push({ check: 'D', file: rel(file), line,
      msg: `${name}() is callable from Swift but names no engine — with more than one instance in the process it acts on whichever the calling thread last entered. That is what made de_sync_position drop the START edge for every rack but the first. Give it a DeInstance*, or mark it "seam-lint-ignore" with why its state is legitimately process-wide.` });
  }
  return out;
}

/* ─────────────────────────────────────────────────────────────────── report ── */

function run() {
  // ios/history/ is an ARCHIVE, not source: the spike stand-ins that produced the milestone
  // screenshots, compiled by nothing, carrying a README that says so. Their signatures deliberately
  // disagree with today's seam — that IS the history. Excluded by name rather than by a rule about
  // directory names, so adding another archive stays a decision somebody makes on purpose.
  const ARCHIVED = p => rel(p).startsWith('ios/history/');
  // ⚠ android/ IS IN SCOPE. It was not, and the entire port silently rotted: every declaration in
  // its hand-copied engine.h lost touch with the seam, `de_init(DeRenderer)` outlived its own
  // deletion, and main.c called all of it — a port that could not link, reported by nothing,
  // under a header whose comment said "byte-for-byte the same contract as ios/Sources/engine.h".
  const hostFiles = [...walk(path.join(ROOT, 'ios'), ['.swift']),
                     ...walk(path.join(ROOT, 'android'), ['.c']),
                     ...walk(path.join(ROOT, 'tools'), ['.c'])].filter(f => !ARCHIVED(f));
  // The seam headers are the DEFINITION check C measures against, so they cannot be redeclarations
  // of themselves. Everything else that declares a de_* is fair game.
  const SEAM_HEADERS = ['runtime/platform.h', 'runtime/studio.h'];
  const cartFiles = [...walk(path.join(ROOT, 'tools', 'carts'), ['.c']),
                     ...walk(path.join(ROOT, 'runtime'), ['.h']),
                     ...walk(path.join(ROOT, 'ios'), ['.h']),
                     ...walk(path.join(ROOT, 'android'), ['.h'])]
                    .filter(f => !SEAM_HEADERS.includes(rel(f)) && !ARCHIVED(f));
  const arity = seamArity();
  const findings = [
    ...checkEngineOwners(hostFiles),
    // every engine .c, not just studio.c — raylib_compat.c holds the whole input seam
    ...walk(path.join(ROOT, 'runtime'), ['.c']).flatMap(checkSeamHandles),
    ...checkCartExterns(cartFiles, arity),
    ...checkHandleless(path.join(ROOT, 'ios', 'Sources', 'engine.h'), arity),
  ];

  if (QUIET) {
    console.log(findings.length ? `engine seam: ${findings.length} finding(s)` : 'engine seam: ok — one engine per host, every seam uses its handle, no cart-declared externs');
    return findings;
  }
  const TITLES = {
    A: 'TWO ENGINES IN ONE HOST — de_instance_create allocates now; it used to be a singleton',
    B: 'A SEAM FUNCTION IGNORES ITS HANDLE — it will act on the wrong engine, silently',
    C: 'AN ENGINE FUNCTION IS REDECLARED WITH THE WRONG SIGNATURE — UB across translation units',
    D: 'A SEAM FUNCTION NAMES NO ENGINE — process-wide state behind a per-instance boundary',
  };
  if (!findings.length) {
    console.log('\nENGINE SEAM: ok\n  one engine per host · every seam uses its handle · no cart-declared engine externs\n');
    return findings;
  }
  console.log(`\nENGINE SEAM — ${findings.length} finding(s)\n`);
  for (const k of ['A', 'B', 'C', 'D']) {
    const g = findings.filter(f => f.check === k);
    if (!g.length) continue;
    console.log(`  ${TITLES[k]}`);
    for (const f of g) console.log(`    ${f.file}:${f.line}  ${f.msg}`);
    console.log('');
  }
  return findings;
}

/* ──────────────────────────────────────────────────────────────── selfcheck ── */

function selfcheck() {
  let pass = 0, fail = 0;
  const t = (name, got) => { if (got) { pass++; console.log(`  \x1b[32m✓\x1b[0m ${name}`); } else { fail++; console.log(`  \x1b[31m✗\x1b[0m ${name}`); } };
  const tmp = fs.mkdtempSync(path.join(require('os').tmpdir(), 'seamlint-'));
  const w = (n, s) => { const p = path.join(tmp, n); fs.writeFileSync(p, s); return p; };

  // ── A, both directions
  const bad = w('bad.swift', 'let e = de_instance_create(DE_RENDERER_SOFTWARE)\n');
  const good = w('good.swift', '// de:engine-owner — this view owns the rack\nlet e = de_instance_create(DE_RENDERER_SOFTWARE)\n');
  const twice = w('twice.swift', '// de:engine-owner\nlet a = de_instance_create(X)\nlet b = de_instance_create(X)\n');
  const none = w('none.swift', 'de_frame(engine, t)\n');
  t('A: an undeclared creator IS caught',      checkEngineOwners([bad]).length === 1);
  t('A: a declared owner is not',              checkEngineOwners([good]).length === 0);
  t('A: an owner creating TWICE is caught',    checkEngineOwners([twice]).length === 1);
  t('A: a file that only USES an engine is not', checkEngineOwners([none]).length === 0);
  // the prose-immunity that makes this file lintable by itself
  const prose = w('prose.swift', '// never call de_instance_create( here\nde_frame(e, t)\n');
  t('A: a MENTION in a comment is not a call', checkEngineOwners([prose]).length === 0);

  // ── B, both directions
  const seamBad = w('b1.c', 'int de_screen_w(DeInstance *in) { (void)in; return de_sw; }\n');
  const seamOk  = w('b2.c', 'int de_screen_w(DeInstance *in) { return de_vid_of(in)->de_sw; }\n');
  const waived  = w('b3.c', 'int de_is_resizable(DeInstance *in) { (void)in; return de_reflow; }  // seam-lint-ignore: compile-time flag\n');
  const proto   = w('b4.c', 'void de_resize(DeInstance *in, int w, int h);\n');
  t('B: a discarded handle IS caught',         checkSeamHandles(seamBad).length === 1);
  t('B: a used handle is not',                 checkSeamHandles(seamOk).length === 0);
  t('B: a waived one is not',                  checkSeamHandles(waived).length === 0);
  t('B: a PROTOTYPE is not a definition',      checkSeamHandles(proto).length === 0);

  // ── C, both directions
  const ar = new Map([['de_resize', 3], ['de_frame', 2]]);
  const cBad = w('c1.c', 'extern void de_resize(int w, int h);\n');
  const cOk  = w('c2.c', 'extern void de_resize(DeInstance *in, int w, int h);\n');
  const cUnk = w('c3.c', 'extern void de_mystery(int a);\n');
  t('C: an arity MISMATCH is caught',          checkCartExterns([cBad], ar).length === 1);
  t('C: a matching extern is not',             checkCartExterns([cOk], ar).length === 0);
  t('C: an unknown function is not guessed at', checkCartExterns([cUnk], ar).length === 0);
  // `extern` was never the defect — a bare prototype is identical UB, and it is the shape the ios/
  // bridging headers use. This is the assertion that would have caught ios/Sources/canvas.h.
  const cProto = w('c4.h', 'const uint8_t* de_resize(int w, int h);\n');
  const cPrOk  = w('c5.h', 'void de_resize(DeInstance *in, int w, int h);\n');
  t('C: a BARE PROTOTYPE mismatch is caught too', checkCartExterns([cProto], ar).length === 1);
  t('C: a bare prototype that matches is not',    checkCartExterns([cPrOk], ar).length === 0);
  // the line must point at the DECLARATION, not at whatever preceded a decommented doc block —
  // an off-by-six here sends the reader to the wrong function and the finding reads as a false one
  const cLine = w('c6.h', 'void de_frame(DeInstance *in, double t);\n\n// a doc block\n// on two lines\nvoid de_resize(int w, int h);\n');
  t('C: the reported line is the declaration\'s', checkCartExterns([cLine], ar)[0].line === 5);

  // ── D, both directions
  const dBad   = w('d1.h', 'void de_midi_event(int type, int note, int vel);\n');
  const dOk    = w('d2.h', 'void de_sync_position(DeInstance *in, double b);\n');
  const dWaive = w('d3.h', '// seam-lint-ignore: one capture device per process\nint de_mic_wanted(void);\n');
  const dCreate= w('d4.h', 'DeInstance *de_instance_create(DeRenderer r);\n');
  t('D: a handle-less seam fn IS caught',      checkHandleless(dBad, ar).length === 1);
  t('D: one that names its instance is not',   checkHandleless(dOk, ar).length === 0);
  t('D: a waived one is not',                  checkHandleless(dWaive, ar).length === 0);
  t('D: de_instance_create RETURNS the handle', checkHandleless(dCreate, ar).length === 0);
  // D is B's blind twin: B only sees a handle being DISCARDED, so it is structurally incapable of
  // reporting a function that never had one. Assert that gap explicitly, or D looks redundant and
  // someone deletes it.
  t('D: check B is blind to this shape (why D exists)', checkSeamHandles(dBad).length === 0);

  // the arity table must actually be populated from the header, or C passes vacuously forever
  const real = seamArity();
  t('C: the seam table is read from the header', real.get('de_resize') === 3 && real.get('de_frame') === 2);
  t('C: and it found the whole seam',           real.size >= 10);

  fs.rmSync(tmp, { recursive: true, force: true });
  console.log(`lint-engine-seam --selfcheck: ${pass}/${pass + fail} known answers correct`);
  process.exit(fail ? 1 : 0);
}

if (ARGV.includes('--selfcheck')) selfcheck();
else {
  const f = run();
  process.exit(STRICT && f.length ? 1 : 0);
}
