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
  const rx = /X\(\s*([^,]+?)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*,([^,]*),/g;
  let m;
  while ((m = rx.exec(body))) rows.push({ type: m[1].trim(), name: m[2], dims: (m[3] || '').trim() });
  return rows;
}

// split on `sep` at bracket/paren depth 0, so `float a[X], b[Y]` splits and `a[1,2]` would not
function splitTop(s, sep) {
  const out = []; let d = 0, cur = '';
  for (const ch of s) {
    if (ch === '[' || ch === '(') d++;
    else if (ch === ']' || ch === ')') d--;
    if (ch === sep && d === 0) { out.push(cur); cur = ''; } else cur += ch;
  }
  if (cur.trim()) out.push(cur);
  return out;
}

// ── parse a `typedef struct { ... } Name;` into ORDERED members ───────────────────────────────────
// Order and array dimensions are part of the identity: this feeds the layout snapshot, where a
// reorder or a resized array is the defect being hunted.
// ⚠ Anything it cannot parse with certainty is COUNTED AND REPORTED, never silently dropped — a
// member missing from the snapshot is a member whose reorder nobody would notice, which is the exact
// failure this file exists to prevent. (Same discipline as tools/ctx-gen.js.)
function structMembers(src, typeName) {
  const close = src.search(new RegExp(`\\}\\s*${typeName}\\s*;`));
  if (close < 0) return null;
  const open = src.lastIndexOf('{', close);
  if (open < 0) return null;
  // ⚠ STRIP COMMENTS BEFORE SPLITTING ON ';'. A trailing comment may itself contain a semicolon —
  // acidcandy has `int cur;  // last active/loaded named slot (UI highlight); -1 = none` — and
  // splitting first cuts inside the comment, so the next real member arrives glued to prose and
  // fails to parse. Both SaveBank and SaveBlob hit this; the tool reported them as unparsed rather
  // than dropping them, which is how it was found.
  const body = src.slice(open + 1, close)
                  .replace(/\/\*[\s\S]*?\*\//g, '')
                  .replace(/\/\/[^\n]*/g, '');
  const out = []; const unparsed = []; const conditional = [];
  for (const raw of body.split(';')) {
    const ln = raw.replace(/\s+/g, ' ').trim();
    if (!ln) continue;
    // A PREPROCESSOR CONDITIONAL is a different problem from an unreadable line, and worth saying so:
    // the struct has more than one shape, so a snapshot can only describe ONE of them. Reported rather
    // than resolved — picking a branch would silently protect one variant and leave the other open.
    if (ln.includes('#')) { conditional.push(ln); continue; }
    // leading type words (`unsigned char`, `struct Foo`, `const char`), then one or more declarators
    const m = /^((?:[A-Za-z_]\w*)(?:\s+[A-Za-z_]\w*)*)\s+(.+)$/.exec(ln);
    if (!m) { unparsed.push(ln); continue; }
    const base = m[1];
    for (const d of splitTop(m[2], ',')) {
      const dm = /^([*\s]*)([A-Za-z_]\w*)\s*((?:\[[^\]]*\])*)$/.exec(d.trim());
      if (!dm) { unparsed.push(ln); continue; }
      out.push({ type: (base + ' ' + dm[1].replace(/\s+/g, '')).trim(), name: dm[2], dims: dm[3] || '' });
    }
  }
  out.unparsed = unparsed;
  out.conditional = conditional;
  return out;
}

/* Every user-defined struct REACHABLE from a saved slice, in declaration order.
 *
 * WHY RECURSIVE: a top-level snapshot of `CartState` would have missed the thing most worth
 * protecting. `SaveBlob`/`SaveBank`/`P303`/`P808`/`P909` are NESTED inside it, they are the bulk of
 * the saved bytes (the autosave plus six song slots), and they are where the comma-declarator lines
 * live. Reordering a field in one of those changes the layout exactly as much as reordering a
 * top-level one, and the fingerprint cannot see either, because the SIZE does not move. */
function reachableLayout(src, rootType, rows) {
  const layout = {}; const problems = [];
  const seen = new Set();
  const visit = (typeName, members) => {
    if (!members || seen.has(typeName)) return;
    seen.add(typeName);
    layout[typeName] = members.map(r => `${r.type}${r.dims} ${r.name}`);
    if (members.unparsed && members.unparsed.length)
      problems.push({ typeName, unparsed: members.unparsed });
    if (members.conditional && members.conditional.length)
      problems.push({ typeName, conditional: members.conditional });
    for (const r of members) {
      // strip qualifiers/pointers to get a bare type name, then see if this file declares it
      const bare = r.type.replace(/\b(const|volatile|unsigned|signed|struct)\b/g, '')
                         .replace(/[*]/g, '').trim().split(/\s+/).pop();
      if (!bare || /^(int|float|double|char|short|long|void|bool|size_t|int8_t|uint8_t|int16_t|uint16_t|int32_t|uint32_t|int64_t|uint64_t)$/.test(bare)) continue;
      if (seen.has(bare)) continue;
      const nested = structMembers(src, bare);
      if (nested) visit(bare, nested);
    }
  };
  visit(rootType, rows);
  return { layout, problems };
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
  const layouts = {};   // "rel:Type" → ordered ["type[dims] name", …]
  const rel = path.relative(ROOT, file);

  const record = (sliceName, rows) => {
    const { layout, problems } = reachableLayout(src, sliceName, rows);
    for (const [t, rowsOut] of Object.entries(layout)) layouts[`${rel}:${t}`] = rowsOut;
    for (const p of problems) {
      if (p.conditional)
        findings.push({ level: 'advisory', where: rel, slice: p.typeName, member: '(conditional)', type: '',
          why: `${p.typeName} has members inside a PREPROCESSOR CONDITIONAL, so it has more than one shape and the snapshot describes only the default one — a reorder in the other variant would NOT be caught: ${p.conditional.slice(0, 2).join(' | ')}` });
      else
        findings.push({ level: 'advisory', where: rel, slice: p.typeName, member: '(unparsed)', type: '',
          why: `${p.unparsed.length} member line(s) in ${p.typeName} could not be parsed, so a reorder involving them would NOT be caught: ${p.unparsed.slice(0, 2).join(' | ')}` });
    }
  };

  // shape 1: DE_CTX_BLOCK_SAVED(lc, Uc, LIST)
  const rx1 = /DE_CTX_BLOCK_SAVED\(\s*([A-Za-z_]\w*)\s*,\s*([A-Za-z_]\w*)\s*,\s*([A-Za-z_]\w*)\s*\)/g;
  let m;
  while ((m = rx1.exec(src))) {
    const rows = xlistRows(src, m[3]);
    if (!rows) { findings.push({ level: 'advisory', where: rel, slice: m[3], member: '(list)', type: '',
      why: `DE_CTX_BLOCK_SAVED names ${m[3]} but no "#define ${m[3]}(X)" was found here — cannot check it` }); continue; }
    findings.push(...judge(rows, rel, m[3]));
    layouts[`${rel}:${m[3]}`] = rows.map(r => `${r.type}${r.dims} ${r.name}`);
  }

  // shape 2: a hand-rolled accessor — de_state_for_saved(&key, sizeof(Type))
  const rx2 = /de_state_for_saved\s*\(\s*&[A-Za-z_]\w*\s*,\s*(?:\(int\)\s*)?sizeof\(\s*([A-Za-z_]\w*)\s*\)/g;
  while ((m = rx2.exec(src))) {
    const mem = structMembers(src, m[1]);
    if (!mem) { findings.push({ level: 'advisory', where: rel, slice: m[1], member: '(struct)', type: '',
      why: `de_state_for_saved saves ${m[1]} but its "typedef struct { … } ${m[1]};" was not found here — cannot check it` }); continue; }
    findings.push(...judge(mem, rel, m[1]));
    record(m[1], mem);
  }
  return { findings, layouts };
}

function scanDirs(dirs) {
  const findings = []; const layouts = {};
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
      const r = scanFile(file, fs.readFileSync(file, 'utf8'));
      findings.push(...r.findings);
      Object.assign(layouts, r.layouts);
    }
  }
  return { findings, layouts };
}

/* ── THE LAYOUT SNAPSHOT: enforce APPEND-ONLY ──────────────────────────────────────────────────────
 *
 * WHY A SNAPSHOT AND NOT A RULE IN THE CODE. A saved blob is matched back to its slice by
 * (index, SIZE) — see de_ss_fingerprint. So the engine catches a slice that GREW or SHRANK, and is
 * structurally blind to a REORDER, which keeps the size identical: the fingerprint matches, the blob
 * is accepted, and every value lands in the wrong field. Silently. Nothing at runtime can see it,
 * because nothing at runtime knows what the fields USED to mean. Only a committed record of the old
 * order does.
 *
 * The allowed change is APPEND. That is not arbitrary: a slice starts life as a copy of the cart's
 * compile-time template, so a shorter saved blob can be restored as a prefix with new fields left at
 * their defaults (the migration route in docs/design/engine-instance-seam.md). Appending keeps that
 * true; anything else breaks it.
 */
const SNAP = path.join(ROOT, 'tools/saved-state-layout.json');

function diffLayout(was, now) {
  for (let i = 0; i < was.length; i++) {
    if (i >= now.length)
      return { kind: 'REMOVED', at: i, was: was[i], now: '(gone)' };
    if (was[i] !== now[i]) {
      // reorder vs retype/rename: does the old entry still exist further along?
      const moved = now.indexOf(was[i]);
      return { kind: moved >= 0 ? 'REORDERED' : 'CHANGED', at: i, was: was[i], now: now[i],
               movedTo: moved >= 0 ? moved : undefined };
    }
  }
  return now.length > was.length ? { kind: 'appended', n: now.length - was.length } : null;
}

function checkLayouts(layouts) {
  if (!fs.existsSync(SNAP))
    return [{ level: 'advisory', where: 'tools/saved-state-layout.json', slice: '(snapshot)', member: '', type: '',
              why: 'no committed layout snapshot — run `node tools/lint-saved-state.js --bless-layout` to record one, or a reorder can never be caught' }];
  const snap = JSON.parse(fs.readFileSync(SNAP, 'utf8'));
  const out = [];
  for (const [key, now] of Object.entries(layouts)) {
    const was = snap.slices[key];
    if (!was) {
      out.push({ level: 'advisory', where: key.split(':')[0], slice: key.split(':')[1], member: '(new)', type: '',
                 why: 'a saved layout not in the snapshot — bless it with --bless-layout' });
      continue;
    }
    const d = diffLayout(was, now);
    if (!d || d.kind === 'appended') continue;   // identical, or the one allowed change
    out.push({ level: 'error', where: key.split(':')[0], slice: key.split(':')[1],
               member: `field ${d.at}`, type: '',
               why: d.kind === 'REORDERED'
                 ? `field ${d.at} was "${d.was}" and is now "${d.now}" — the old field moved to ${d.movedTo}. A REORDER keeps the struct the same SIZE, so the engine's fingerprint still matches and every restored value lands in the WRONG FIELD, silently. Append instead, or migrate deliberately and re-bless.`
                 : `field ${d.at} was "${d.was}" and is now "${d.now}" — a saved field was retyped, renamed or removed. Old blobs will restore into it wrongly. Append instead, or migrate deliberately and re-bless.` });
  }
  for (const key of Object.keys(snap.slices || {}))
    if (!layouts[key])
      out.push({ level: 'advisory', where: key.split(':')[0], slice: key.split(':')[1], member: '(vanished)', type: '',
                 why: 'the snapshot records this saved layout but the scan no longer finds it — either it stopped being saved (re-bless) or the pattern that finds it has rotted' });
  return out;
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
  const sf = (n, src) => scanFile(n, src).findings;
  let f = sf('/x/saved.h', savedListPtr);
  t('a POINTER in a SAVED x-list is an error', f.some(x => x.level === 'error' && x.member === 'foo_evt'));
  t('a handle-NAMED int in a SAVED x-list is advisory', f.some(x => x.level === 'advisory' && x.member === 'foo_handle'));
  t('a plain int is not reported', !f.some(x => x.member === 'foo_n'));

  // THE OTHER DIRECTION: the identical list on the SCRATCH macro must be silent. If this ever starts
  // reporting, the tool has stopped distinguishing the two and every finding above is noise.
  f = sf('/x/scratch.h', savedListPtr.replace('DE_CTX_BLOCK_SAVED', 'DE_CTX_BLOCK'));
  t('the SAME list on DE_CTX_BLOCK (scratch) is silent', f.length === 0);

  const savedStruct = `
typedef struct { int knob; float *buf; int solo_h; int inited; } MyKnobs;
static MyKnobs *m_(void) { return (MyKnobs *)de_state_for_saved(&k_, (int)sizeof(MyKnobs)); }
`;
  f = sf('/x/cart.h', savedStruct);
  t('a POINTER in a hand-rolled saved struct is an error', f.some(x => x.level === 'error' && x.member === 'buf'));
  t('a _h-suffixed int in a saved struct is advisory', f.some(x => x.level === 'advisory' && x.member === 'solo_h'));
  t('and the plain members are not reported', !f.some(x => x.member === 'knob' || x.member === 'inited'));

  f = sf('/x/scratch2.h', savedStruct.replace('de_state_for_saved', 'de_state_for'));
  t('the SAME struct on de_state_for (scratch) is silent', f.length === 0);

  // a saved slice whose list/struct cannot be found must SAY SO rather than pass quietly
  f = sf('/x/orphan.h', 'DE_CTX_BLOCK_SAVED(bar, Bar, BAR_STATE)\n');
  t('an unresolvable saved list is reported, not skipped silently',
    f.length === 1 && f[0].level === 'advisory' && /cannot check/.test(f[0].why));

  f = sf('/x/clean.h', `
#define OK_STATE(X)        \\
    X(int,   ok_a, ,     1) \\
    X(float, ok_b, [8], {0})
DE_CTX_BLOCK_SAVED(ok, Ok, OK_STATE)
`);
  t('a clean saved list reports nothing', f.length === 0);

  // ── the LAYOUT half: the reorder the engine's fingerprint structurally cannot see ───────────────
  const L = ['int a', 'float b', 'int c'];
  t('an identical layout is silent', diffLayout(L, ['int a', 'float b', 'int c']) === null);
  t('APPENDING is the one allowed change',
    (diffLayout(L, [...L, 'int d']) || {}).kind === 'appended');
  const swap = diffLayout(L, ['float b', 'int a', 'int c']);
  t('a REORDER is caught', swap && swap.kind === 'REORDERED' && swap.at === 0);
  t('and it says where the field went', swap && swap.movedTo === 1);
  t('a RETYPE is caught', (diffLayout(L, ['long a', 'float b', 'int c']) || {}).kind === 'CHANGED');
  t('a RENAME is caught', (diffLayout(L, ['int z', 'float b', 'int c']) || {}).kind === 'CHANGED');
  t('a REMOVAL is caught', (diffLayout(L, ['int a', 'float b']) || {}).kind === 'REMOVED');
  t('a RESIZED ARRAY is caught (dims are part of the identity)',
    (diffLayout(['int a[4]'], ['int a[8]']) || {}).kind === 'CHANGED');
  // an INSERT in the middle must not be mistaken for an append
  t('an INSERT in the middle is NOT treated as an append',
    (diffLayout(L, ['int a', 'int NEW', 'float b', 'int c']) || {}).kind === 'REORDERED');

  // NESTED types must be captured, or the snapshot protects the shallow half only — SaveBlob and the
  // pattern structs live inside acidcandy's CartState and carry most of the saved bytes.
  const nested = `
typedef struct { int g[4], h[4]; } Inner;
typedef struct { int knob; Inner blob; } Outer;
static Outer *o_(void) { return (Outer *)de_state_for_saved(&k_, (int)sizeof(Outer)); }
`;
  const nl = scanFile('/x/n.h', nested).layouts;
  // look the slices up by TYPE suffix: the key is prefixed with a repo-relative path, which for a
  // synthetic absolute path comes out as "../x/n.h" rather than the string passed in.
  const bySuffix = (s) => nl[Object.keys(nl).find(k => k.endsWith(':' + s))];
  t('a nested struct reached from a saved slice is snapshotted too', !!bySuffix('Inner'));
  t('and its COMMA-declared members are both captured',
    (bySuffix('Inner') || []).length === 2 && bySuffix('Inner')[1] === 'int[4] h');
  t('the outer layout is captured in order',
    (bySuffix('Outer') || []).join('|') === 'int knob|Inner blob');

  // a member line the parser cannot read must be REPORTED, never silently dropped: a field missing
  // from the snapshot is a field whose reorder nobody would notice.
  const weird = `
typedef struct { int ok; int (*fn)(void); } Odd;
static Odd *d_(void) { return (Odd *)de_state_for_saved(&k_, (int)sizeof(Odd)); }
`;
  const wf = scanFile('/x/w.h', weird).findings;
  t('an unparsable member line is reported, not dropped',
    wf.some(x => x.member === '(unparsed)' && /would NOT be caught/.test(x.why)));

  console.log(`lint-saved-state --selfcheck: ${pass}/${pass + fail} known answers correct`);
  return fail ? 1 : 0;
}

// ── main ─────────────────────────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2);
if (argv.includes('--selfcheck')) process.exit(selfcheck());

const scan = scanDirs(['runtime', 'tools/carts', 'tools/state-check']);
const layouts = scan.layouts;

if (argv.includes('--bless-layout')) {
  const data = { version: 1, note: 'APPEND-ONLY record of every saved state layout. Regenerate ONLY with a deliberate migration: a reorder/retype keeps the struct size, so the engine accepts old blobs and restores every value into the wrong field. See docs/design/engine-instance-seam.md.', slices: layouts };
  fs.writeFileSync(SNAP, JSON.stringify(data, null, 1));
  console.log(`wrote ${path.relative(ROOT, SNAP)} — ${Object.keys(layouts).length} saved layout(s):`);
  for (const k of Object.keys(layouts)) console.log(`  ${k}  (${layouts[k].length} fields)`);
  process.exit(0);
}

const findings = [...scan.findings, ...checkLayouts(layouts)];
const errors = findings.filter(f => f.level === 'error');
const advisory = findings.filter(f => f.level === 'advisory');

if (argv.includes('--json')) {
  console.log(JSON.stringify({ errors, advisory }, null, 2));
} else if (!findings.length) {
  console.log(`SAVED STATE: ok — no pointers or handle-shaped members, and all ${Object.keys(layouts).length} saved layouts match the committed snapshot (append-only)`);
} else {
  for (const f of errors)   console.log(`  ✗ ${f.where}  ${f.slice}.${f.member}  (${f.type})\n      ${f.why}`);
  for (const f of advisory) console.log(`  ⚠ ${f.where}  ${f.slice}.${f.member}  (${f.type})\n      ${f.why}`);
  console.log(`\n${errors.length} error(s) · ${advisory.length} advisory` +
    (advisory.length ? '\n  ⚠ advisory = a NAME that looks like a handle. No static check can tell an\n' +
                       '    `int handle` from any other int, so this is a question, not a verdict.' : ''));
}
if (argv.includes('--strict') && errors.length) process.exit(1);
