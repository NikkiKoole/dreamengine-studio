#!/usr/bin/env node
// gate-controls.js — which of our GATES can prove they are able to fail?
//
// A gate that has never been seen to go red is indistinguishable from one that has gone blind, and
// nothing in the repo will ever tell you which. This tool makes that property visible: it finds the
// tools that can FAIL a build, and reports which of them carry evidence they can — a known-answer
// self-test, or a negative control.
//
// WHY (docs/guides/checks-and-oracles.md → "The OTHER way a green check lies"): on 2026-08-13 three
// separate assertions were green while the thing they named was broken, each for a different reason
// — one measured the wrong axis, one guarded code that never ran, one passed by winning a race.
// None was a checker BUG, so no known-answer fixture would have caught them; all three were caught
// by a control. The existing `--selfcheck` discipline spread from 4 audio tools to 15 only once
// repo-doctor started counting them, which is the entire theory behind this file.
//
// ADVISORY, and deliberately so. A missing control is a QUESTION ("what would you break to make
// this go red?"), not a defect: plenty of gates fail loudly and self-describingly and need nothing.
// It never exits nonzero except on --selfcheck. It also cannot tell you a gate is GOOD — only that
// nothing in it has ever been shown to fail.
//
//   node tools/gate-controls.js              # the report
//   node tools/gate-controls.js --list       # just the uncovered gates, one per line
//   node tools/gate-controls.js --json
//   node tools/gate-controls.js --quiet      # counts only (repo-doctor)
//   node tools/gate-controls.js --selfcheck  # known answers — this tool judges, so it is judged
//
// ⚠ This tool is itself the shape it measures, so its own self-test carries the negative cases that
// matter more than the positive ones: a NON-gate must not be listed, and a gate whose only "control"
// is the WORD "control" in prose must not count as covered.

const fs   = require('fs');
const path = require('path');
const ROOT = path.resolve(__dirname, '..');

// ── what counts as a GATE ──
// Something that can fail a build: it exits nonzero, or prints a PASS/FAIL verdict. Scanning for
// the **ability to fail** rather than for a name pattern is deliberate — "check" in a filename is
// neither necessary (spec.js, build-all.js) nor sufficient (canvas-diff is a reporter by default).
const GATE_SIGNALS = [
  /process\.exit\(\s*1\s*\)/,          // js: hard fail
  /process\.exitCode\s*=\s*1/,         // js: soft fail
  /\bexit\s+1\b/,                      // sh: hard fail
];
const VERDICT_SIGNALS = [/\bFAIL\b/, /\bPASS\b/];

// ── what counts as EVIDENCE the gate can go red ──
// Two accepted forms, and both must be STRUCTURAL (a flag the tool implements), never prose. An
// early draft accepted the word "control" anywhere in the file, which passed on a comment saying
// "no control here" — the exact class of mistake this tool exists to find.
const CONTROL_SIGNALS = [
  { re: /--selfcheck/,          kind: 'selfcheck'  },  // known-answer fixture
  { re: /--check\b[\s\S]{0,80}self-?test/i, kind: 'selftest' },
  { re: /-bypass\b/,            kind: 'bypass'     },  // rebuild without the safety; must FAIL
  { re: /NEGATIVE CONTROL/i,    kind: 'negctl'     },  // an in-gate control, shouted so it is greppable
];

// …but "exits nonzero" alone over-reports badly, because a tool that exits 1 on BAD USAGE
// (`api.js` with no match, `cart-info.js` with no such cart) looks identical to one that exits 1 on
// a VERDICT. The first draft listed 60 "uncovered gates" including half the build-* generators —
// the same shape as stale-doc-check's first tier, which ran at 47 findings and 0 true positives.
// So a gate must ALSO judge: print a verdict, offer a CI flag, or be named like an oracle.
const JUDGE_SIGNALS = [
  /--quiet/,                                   // the repo's CI convention
  /--selfcheck/,                               // carrying a known-answer fixture IS judging
  /\bPASS\b/, /\bFAIL\b/,
  /--check\b/,                                 // the staleness-gate convention (build-*.js --check)
  /--strict\b/,
];
const JUDGE_NAME = /(^|\/)(lint-|check-|repo-doctor|spec)|-check\.(js|sh)$|-check\//;

function scan(file) {
  let src;
  try { src = fs.readFileSync(file, 'utf8'); } catch { return null; }
  const rel = path.relative(ROOT, file);
  const canFail = GATE_SIGNALS.some(re => re.test(src)) || VERDICT_SIGNALS.every(re => re.test(src));
  if (!canFail) return null;
  const judges = JUDGE_SIGNALS.some(re => re.test(src)) || JUDGE_NAME.test(rel);
  const controls = CONTROL_SIGNALS.filter(c => c.re.test(src)).map(c => c.kind);
  return { file: rel, controls, judges };
}

function collect(dir) {
  const out = [];
  const walk = (d, depth) => {
    let ents;
    try { ents = fs.readdirSync(d, { withFileTypes: true }); } catch { return }
    for (const e of ents) {
      const p = path.join(d, e.name);
      // one level of subdirectory: tools/<name>-check/run.sh is the live-behaviour gate shape
      if (e.isDirectory()) { if (depth < 1 && !/node_modules|fixtures|carts|clips/.test(e.name)) walk(p, depth + 1); continue }
      if (!/\.(js|sh)$/.test(e.name)) continue;
      if (e.name === 'gate-controls.js') continue;      // don't grade yourself
      const r = scan(p);
      if (r) out.push(r);
    }
  };
  walk(dir, 0);
  return out.sort((a, b) => a.file.localeCompare(b.file));
}

// ── self-test: synthetic files with known answers ──
function selfcheck() {
  const tmp = path.join(ROOT, 'build', '.gate-controls-selfcheck');
  fs.rmSync(tmp, { recursive: true, force: true });
  fs.mkdirSync(tmp, { recursive: true });
  const W = (n, s) => fs.writeFileSync(path.join(tmp, n), s);

  W('a-gate-selfcheck.js', 'if (bad) process.exit(1)\nif (args["--selfcheck"]) runFixture()\n');
  W('b-gate-bypass.sh',    'echo PASS\nexit 1\n# -bypass rebuilds without the safety\n');
  W('c-gate-negctl.sh',    'echo "FAIL"\nexit 1\n# NEGATIVE CONTROL: must publish nothing\n');
  W('d-gate-judging.js',   'if (bad) process.exit(1)\nif (a["--quiet"]) terse()\n');   // judges, no control
  W('e-gate-verdict.sh',   'echo PASS; echo FAIL\n');                       // verdict-only, no exit 1
  W('f-not-a-gate.js',     'console.log("a reporter: no exit, no verdict")\n');
  W('g-prose-only.js',     'if (bad) process.exit(1)\nif (a["--quiet"]) t()\n// there is no control here yet\n');
  W('h-nonsource.txt',     'process.exit(1) --selfcheck\n');                // not .js/.sh
  W('i-usage-exit.js',     'if (!argv[0]) { console.error("usage: ..."); process.exit(1) }\n');  // exits, never judges

  const got  = collect(tmp);
  const by   = Object.fromEntries(got.map(g => [path.basename(g.file), g.controls]));
  const jdg  = Object.fromEntries(got.map(g => [path.basename(g.file), g.judges]));
  const T = [];
  const t = (name, cond) => T.push({ name, ok: !!cond });

  t('exit(1) js is a gate',                    !!by['a-gate-selfcheck.js']);
  t('--selfcheck counts as a control',         (by['a-gate-selfcheck.js'] || []).includes('selfcheck'));
  t('-bypass counts as a control',             (by['b-gate-bypass.sh'] || []).includes('bypass'));
  t('NEGATIVE CONTROL counts as a control',    (by['c-gate-negctl.sh'] || []).includes('negctl'));
  t('a judging gate with nothing is uncovered', by['d-gate-judging.js'] && by['d-gate-judging.js'].length === 0);
  t('  ...and it IS counted as judging',       jdg['d-gate-judging.js'] === true);
  t('PASS+FAIL alone makes it a gate',         !!by['e-gate-verdict.sh']);
  // the negative cases — the ones that matter, per rule 4 of "Self-test the checker"
  t('a NON-gate is not listed at all',         by['f-not-a-gate.js'] === undefined);
  t('prose "no control here" does NOT count',  by['g-prose-only.js'] && by['g-prose-only.js'].length === 0);
  t('a non-source file is ignored',            by['h-nonsource.txt'] === undefined);
  t('exit-on-USAGE is NOT counted as judging', jdg['i-usage-exit.js'] === false);

  fs.rmSync(tmp, { recursive: true, force: true });
  const bad = T.filter(x => !x.ok);
  for (const x of T) console.log(`  ${x.ok ? '\x1b[32m✓\x1b[0m' : '\x1b[31m✗\x1b[0m'} \x1b[90m${x.name}\x1b[0m`);
  console.log(`gate-controls --selfcheck: ${T.length - bad.length}/${T.length} known answers correct`);
  process.exit(bad.length ? 1 : 0);
}

// ── main ──
const argv = process.argv.slice(2);
if (argv.includes('--selfcheck')) selfcheck();

const all      = collect(path.join(ROOT, 'tools'));
const gates    = all.filter(g => g.judges);
const notJudge = all.filter(g => !g.judges);          // exits nonzero, but on bad USAGE — not a verdict
const covered  = gates.filter(g => g.controls.length);
const bare     = gates.filter(g => !g.controls.length);

if (argv.includes('--json')) {
  console.log(JSON.stringify({ total: gates.length, covered: covered.length,
    bare: bare.map(b => b.file), excludedNotJudging: notJudge.map(b => b.file) }, null, 2));
  process.exit(0);
}
if (argv.includes('--list')) { bare.forEach(b => console.log(b.file)); process.exit(0) }
// rule 2 of "Self-test the checker": never suppress silently — always be able to LIST what was dropped.
if (argv.includes('--excluded')) { notJudge.forEach(b => console.log(b.file)); process.exit(0) }
if (argv.includes('--quiet')) {
  console.log(`${covered.length}/${gates.length} gates carry a self-test or negative control · ${bare.length} with neither`);
  process.exit(0);
}

console.log(`\nGATE CONTROLS — can each gate prove it is able to FAIL?`);
console.log(`(advisory: a missing control is a question, not a defect — docs/guides/checks-and-oracles.md)\n`);
console.log(`  \x1b[32m${covered.length}\x1b[0m of ${gates.length} gates carry a self-test or a negative control`);
const kinds = {};
covered.forEach(c => c.controls.forEach(k => kinds[k] = (kinds[k] || 0) + 1));
console.log(`  by kind: ${Object.entries(kinds).map(([k, n]) => `${k} ${n}`).join(' · ') || '(none)'}\n`);
console.log(`  \x1b[33m${bare.length}\x1b[0m with neither — for each, ask: what would I break to make this go red?\n`);
for (const b of bare) console.log(`    ${b.file}`);
console.log(`\n  the ones that most need one: PASS is the steady state and failure is SILENT`);
console.log(`  (timing, threading, guards that suppress behaviour). Loud failures need nothing.\n`);
