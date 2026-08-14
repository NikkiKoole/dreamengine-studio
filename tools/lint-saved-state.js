#!/usr/bin/env node
'use strict';
/*
 * lint-saved-state.js — nothing in a SAVED state slice may be meaningful only to the instance that
 * wrote it.
 *
 * WHY THIS EXISTS. `de_state_for_saved` slices are written into the host's project file and restored
 * into a DIFFERENT process, and possibly a different instance of the same process. A member that
 * names something local to the writer comes back naming nothing:
 *
 *   · a POINTER — the address is from another process's address space. Deterministic to detect, and
 *     this is the rule docs/design/engine-instance-seam.md always intended to impose.
 *   · a LIVE VOICE HANDLE — what note_on() returns. This is the case that matters more and the reason
 *     a pointer-only lint is not enough: a handle is a plain `int`, so it is INVISIBLE to any type
 *     check, yet restoring one points a restored rack at a voice slot in a pool it never allocated
 *     from. keybed.h (kb_handle[128]), solo.h (solo_handle) and radio.h (rad_static_h) all hold them,
 *     which is precisely why all three are SCRATCH.
 *
 * So the two tiers are honest about their own strength. POINTERS are an ERROR: the type says so, and
 * there is no legitimate reason to save one. HANDLES are ADVISORY, matched by NAME, because no static
 * check can tell an `int handle` from any other `int` — the tool raises the question and a human
 * answers it. A tool that pretended otherwise would either miss handles or drown the real findings.
 *
 *   node tools/lint-saved-state.js              the report
 *   node tools/lint-saved-state.js --strict     exit nonzero on an ERROR (repo-doctor)
 *   node tools/lint-saved-state.js --json
 *   node tools/lint-saved-state.js --selfcheck  known answers, BOTH directions
 *
 * BOTH DIRECTIONS is the point of the fixture: this lint's failure mode is not a false positive, it
 * is going quietly blind — if the pattern that finds a SAVED slice ever rots, the tool reports zero
 * findings and prints the same green as a healthy repo. So the fixture asserts that a pointer in a
 * saved list IS caught AND that the same pointer in a SCRATCH list is NOT.
 *
 * Sibling of tools/lint-engine-seam.js (the host boundary) and tools/lint-fx-frame.js. The runtime
 * behaviour it guards is gated by tools/state-check/run.sh.
 */
const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..');
const HANDLE_RX = /(^|_)(handle|handles)$|(^|_)h$|_hn$|(^|_)hnd$/i;

// ── parse an X-list: X(type, name, dims, init) rows inside a #define NAME(X) ... block ────────────
function xlistRows(src, listName) {
  const at = src.indexOf(`#define ${listName}(X)`);
  if (at < 0) return null;
  // the macro body runs to the first line that does not end in a backslash
  let i = at, body = '';
  const lines = src.slice(at).split('\n');
  for (const ln of lines) {
    body += ln + '\n';
    if (!/\\\s*$/.test(ln)) break;
  }
  void i;
  const rows = [];
  const rx = /X\(\s*([^,]+?)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*,/g;
  let m;
  while ((m = rx.exec(body))) rows.push({ type: m[1].trim(), name: m[2] });
  return rows;
}

// ── parse a `typedef struct { ... } Name;` into members ──────────────────────────────────────────
function structMembers(src, typeName) {
  // find the closing `} Name;` then walk back to its opening brace
  const close = src.search(new RegExp(`\\}\\s*${typeName}\\s*;`));
  if (close < 0) return null;
  const open = src.lastIndexOf('{', close);
  if (open < 0) return null;
  const body = src.slice(open + 1, close);
  const out = [];
  for (let raw of body.split(';')) {
    const ln = raw.replace(/\/\/[^\n]*/g, '').replace(/\/\*[\s\S]*?\*\//g, '').trim();
    if (!ln) continue;
    const m = /^([A-Za-z_][A-Za-z0-9_ \t]*?[ \t*]+)([A-Za-z_][A-Za-z0-9_]*)\s*(\[[^\]]*\])?$/.exec(ln);
    if (!m) continue;
    out.push({ type: m[1].trim(), name: m[2] });
  }
  return out;
}

function judge(members, where, slice) {
  const findings = [];
  for (const mem of members) {
    if (/\*/.test(mem.type)) {
      findings.push({ level: 'error', where, slice, member: mem.name, type: mem.type,
        why: 'a POINTER in a saved slice — the address belongs to the process that wrote it' });
    } else if (HANDLE_RX.test(mem.name)) {
      findings.push({ level: 'advisory', where, slice, member: mem.name, type: mem.type,
        why: 'name looks like a LIVE HANDLE — if it is one, it names a voice in another instance\'s pool' });
    }
  }
  return findings;
}

// ── scan one file for saved slices, both declaration shapes ──────────────────────────────────────
function scanFile(file, src) {
  const findings = [];
  const rel = path.relative(ROOT, file);

  // shape 1: DE_CTX_BLOCK_SAVED(lc, Uc, LIST)
  const rx1 = /DE_CTX_BLOCK_SAVED\(\s*([A-Za-z_]\w*)\s*,\s*([A-Za-z_]\w*)\s*,\s*([A-Za-z_]\w*)\s*\)/g;
  let m;
  while ((m = rx1.exec(src))) {
    const rows = xlistRows(src, m[3]);
    if (!rows) { findings.push({ level: 'advisory', where: rel, slice: m[3], member: '(list)', type: '',
      why: `DE_CTX_BLOCK_SAVED names ${m[3]} but no "#define ${m[3]}(X)" was found here — cannot check it` }); continue; }
    findings.push(...judge(rows, rel, m[3]));
  }

  // shape 2: a hand-rolled accessor — de_state_for_saved(&key, sizeof(Type))
  const rx2 = /de_state_for_saved\s*\(\s*&[A-Za-z_]\w*\s*,\s*(?:\(int\)\s*)?sizeof\(\s*([A-Za-z_]\w*)\s*\)/g;
  while ((m = rx2.exec(src))) {
    const mem = structMembers(src, m[1]);
    if (!mem) { findings.push({ level: 'advisory', where: rel, slice: m[1], member: '(struct)', type: '',
      why: `de_state_for_saved saves ${m[1]} but its "typedef struct { … } ${m[1]};" was not found here — cannot check it` }); continue; }
    findings.push(...judge(mem, rel, m[1]));
  }
  return findings;
}

function scanDirs(dirs) {
  const findings = [];
  for (const d of dirs) {
    const abs = path.join(ROOT, d);
    if (!fs.existsSync(abs)) continue;
    for (const f of fs.readdirSync(abs)) {
      if (!/\.(h|c)$/.test(f)) continue;
      // cart_ctx.h DEFINES DE_CTX_BLOCK_SAVED, so its own `#define … DE_CTX_BLOCK_(lc, Uc, LIST, …)`
      // matches the use-site pattern and reports the placeholder `LIST` as an uncheckable slice.
      // Skipping the declaration site is the fix; stripping comments would not help, since this is
      // real code rather than the doc example above it.
      if (f === 'cart_ctx.h') continue;
      const file = path.join(abs, f);
      findings.push(...scanFile(file, fs.readFileSync(file, 'utf8')));
    }
  }
  return findings;
}

// ── selfcheck ────────────────────────────────────────────────────────────────────────────────────
function selfcheck() {
  let pass = 0, fail = 0;
  const t = (name, cond) => { if (cond) { pass++; } else { fail++; console.log(`  ✗ ${name}`); } };

  const savedListPtr = `
#define FOO_STATE(X)            \\
    X(int,    foo_n,   ,      0) \\
    X(void *, foo_evt, [4], {0}) \\
    X(int,    foo_handle, ,   0)
DE_CTX_BLOCK_SAVED(foo, Foo, FOO_STATE)
`;
  let f = scanFile('/x/saved.h', savedListPtr);
  t('a POINTER in a SAVED x-list is an error', f.some(x => x.level === 'error' && x.member === 'foo_evt'));
  t('a handle-NAMED int in a SAVED x-list is advisory', f.some(x => x.level === 'advisory' && x.member === 'foo_handle'));
  t('a plain int is not reported', !f.some(x => x.member === 'foo_n'));

  // THE OTHER DIRECTION: the identical list on the SCRATCH macro must be silent. If this ever starts
  // reporting, the tool has stopped distinguishing the two and every finding above is noise.
  f = scanFile('/x/scratch.h', savedListPtr.replace('DE_CTX_BLOCK_SAVED', 'DE_CTX_BLOCK'));
  t('the SAME list on DE_CTX_BLOCK (scratch) is silent', f.length === 0);

  const savedStruct = `
typedef struct { int knob; float *buf; int solo_h; int inited; } MyKnobs;
static MyKnobs *m_(void) { return (MyKnobs *)de_state_for_saved(&k_, (int)sizeof(MyKnobs)); }
`;
  f = scanFile('/x/cart.h', savedStruct);
  t('a POINTER in a hand-rolled saved struct is an error', f.some(x => x.level === 'error' && x.member === 'buf'));
  t('a _h-suffixed int in a saved struct is advisory', f.some(x => x.level === 'advisory' && x.member === 'solo_h'));
  t('and the plain members are not reported', !f.some(x => x.member === 'knob' || x.member === 'inited'));

  f = scanFile('/x/scratch2.h', savedStruct.replace('de_state_for_saved', 'de_state_for'));
  t('the SAME struct on de_state_for (scratch) is silent', f.length === 0);

  // a saved slice whose list/struct cannot be found must SAY SO rather than pass quietly
  f = scanFile('/x/orphan.h', 'DE_CTX_BLOCK_SAVED(bar, Bar, BAR_STATE)\n');
  t('an unresolvable saved list is reported, not skipped silently',
    f.length === 1 && f[0].level === 'advisory' && /cannot check/.test(f[0].why));

  f = scanFile('/x/clean.h', `
#define OK_STATE(X)        \\
    X(int,   ok_a, ,     1) \\
    X(float, ok_b, [8], {0})
DE_CTX_BLOCK_SAVED(ok, Ok, OK_STATE)
`);
  t('a clean saved list reports nothing', f.length === 0);

  console.log(`lint-saved-state --selfcheck: ${pass}/${pass + fail} known answers correct`);
  return fail ? 1 : 0;
}

// ── main ─────────────────────────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2);
if (argv.includes('--selfcheck')) process.exit(selfcheck());

const findings = scanDirs(['runtime', 'tools/carts', 'tools/state-check']);
const errors = findings.filter(f => f.level === 'error');
const advisory = findings.filter(f => f.level === 'advisory');

if (argv.includes('--json')) {
  console.log(JSON.stringify({ errors, advisory }, null, 2));
} else if (!findings.length) {
  console.log('SAVED STATE: ok — no pointers or handle-shaped members in any de_state_for_saved slice');
} else {
  for (const f of errors)   console.log(`  ✗ ${f.where}  ${f.slice}.${f.member}  (${f.type})\n      ${f.why}`);
  for (const f of advisory) console.log(`  ⚠ ${f.where}  ${f.slice}.${f.member}  (${f.type})\n      ${f.why}`);
  console.log(`\n${errors.length} error(s) · ${advisory.length} advisory` +
    (advisory.length ? '\n  ⚠ advisory = a NAME that looks like a handle. No static check can tell an\n' +
                       '    `int handle` from any other int, so this is a question, not a verdict.' : ''));
}
if (argv.includes('--strict') && errors.length) process.exit(1);
