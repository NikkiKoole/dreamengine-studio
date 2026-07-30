#!/usr/bin/env node
// ============================================================================
// stale-doc-check.js — flag docs that MENTION something that changed AFTER the
// doc was last touched. The cheap, zero-upkeep freshness nudge.
//
//   node tools/stale-doc-check.js               # full report (whole docs/ corpus)
//   node tools/stale-doc-check.js tools-we-need # SCOPED: only docs whose path matches
//   node tools/stale-doc-check.js --docs         # also expand the doc→doc churn tier
//   node tools/stale-doc-check.js --all          # list the suppressed proposal/other-repo refs
//   node tools/stale-doc-check.js --days 30     # grace: ignore changes <30d after the doc
//   node tools/stale-doc-check.js --json         # machine-readable  (--strict = exit 1 if any)
//
// THE FRICTION THIS KILLS. A doc says "build-context does X" or "see road-check".
// The tool then changes — new flags, renamed, reworked — and the doc silently drifts.
// Nothing connects the prose mention to the file it names, so the drift is invisible
// until a human re-reads the doc and notices. (This tool was itself the last-open item
// on docs/design/tools-we-need.md, which had drifted exactly this way: it listed
// build-context/build-field-notes/build-cart-index as "ideas" long after they shipped.)
//
// THE SIGNAL (one mechanical comparison, no maintained dependency graph):
//   doc mentions entity E (a tool or another doc, by hyphenated basename in prose)
//   AND E's last-commit date is NEWER than the doc's own last-commit date
//   → the doc may describe a stale version of E. Flag it for a human re-read.
//
// This is a NUDGE, not a proof — a mention is not a dependency. It's deliberately
// cheap: reconciling (and committing) the doc resets its clock, so a flag clears the
// moment you actually look. Advisory; always exits 0 unless --strict.
//
// THREE TIERS, by confidence:
//   0. BROKEN REFERENCES (real issues, shown first) — not "something changed" but "this
//      claim is FALSE now": a doc cites a code PATH that doesn't exist, or a `tool --flag`
//      the tool has no such flag literal for. Falsifiable, so worth fixing on sight. Dead
//      flags are near-certain bugs (they caught `play.js --det` — a cart-binary flag that
//      play.js never forwards — cited as runnable in a guide); missing paths are noisier
//      (a design doc naming `runtime/roadkit.h` may be proposing a not-yet-built file, so
//      those rank below flags and below guides). Placeholder paths (x.js, XX-name.c) and
//      lines with proposal cues ("or a --foo", "would", "future") are filtered out.
//      CAVEAT — a dead flag is NOT always a bug: the cue filter misses PLANNED forward-refs
//      framed as roadmap ("open follow-up: `youtube-push --dress`", a spike-ladder "→ 7
//      (`build-app.js --android`)"). Before "fixing" one, read the surrounding doc — a flag
//      that names not-yet-built work is correct prose, not drift. And a missing path is matched
//      by its `tools/`|`runtime/` SUBSTRING anywhere on the line, so PREFIXING it (e.g.
//      `~/Projects/navkit/soundsystem/tools/foo.c` for an external ref) does NOT clear the flag —
//      only a path that resolves, or wording that drops the path pattern, will.
//   1. TOOL DRIFT (shown in full) — a doc describes a TOOL whose code changed after it.
//      A nudge, not a proof: behavior/flags the prose describes MAY now be wrong.
//   2. DOC CHURN (collapsed to a count; --docs expands) — a doc names another DOC edited
//      later. Mostly ordinary churn in an active corpus, so it's a standing backlog, not
//      a to-do list. Link integrity is lint-docs.js's job; whether the link SHOULD exist
//      is lint-xrefs.js's.
//
// SCOPE OF ENTITIES. Only unambiguous names are scanned: hyphenated basenames with an
// alpha segment ≥4 chars (build-context, road-check, touch-controls, audio-notes).
// Single-word tool/cart names (spec, run, play, boom, juice) are skipped on purpose —
// they'd fire on ordinary prose. Fenced code blocks are excluded. Same philosophy as
// lint-xrefs.js (its companion: that finds links that SHOULD exist; this finds prose
// that may have gone stale).
//
// DATES. A doc's date is its leading `updated:` frontmatter if present, else its
// last git commit date. Entity date = last git commit date. Day granularity (git %cs).
// ============================================================================

const fs = require("fs");
const path = require("path");
const { execFileSync } = require("child_process");

const ROOT = path.resolve(__dirname, "..");

// ── --selfcheck: assert the CHECKER against known answers ────────────────────
// See docs/guides/checks-and-oracles.md "Self-test the checker". This tier judged 47 references and
// got all 47 wrong; the fixture pins the four verdicts that judgement rests on, so the next person to
// tune a cue cannot silently reintroduce the noise.
if (process.argv.slice(2).includes("--selfcheck")) {
  const fx = path.join(__dirname, "fixtures", "stale-doc-check", "docs");
  let raw;
  try {
    raw = require("child_process").execFileSync(process.execPath, [__filename, "--json"],
      { env: { ...process.env, DE_DOCS_DIR: fx }, encoding: "utf8", maxBuffer: 1 << 26 });
  } catch (e) { raw = e.stdout; }
  const g = JSON.parse(raw);
  const brokenRefs = g.broken.map(b => b.ref);
  const sup = (why) => g.suppressed.filter(x => x.why === why).map(x => x.ref);
  const T = [];
  const t = (n, ok) => T.push({ n, ok });

  t("a path that once existed and is GONE → broken", brokenRefs.includes("runtime/font16x16_data.h"));
  t("a dead --flag on a real tool → broken", brokenRefs.some(r => /--zzznotaflag/.test(r)));
  t("a path that EXISTS → silent", !brokenRefs.includes("tools/play.js") && !sup("proposal").includes("tools/play.js"));
  t("a real --flag on a real tool → silent", !brokenRefs.some(r => /--headless/.test(r)));
  t("a NEVER-existed path → suppressed as proposal  [regression guard]",
    sup("proposal").includes("runtime/engines/zzznever.h") && sup("proposal").includes("tools/zzz-gen.js"));
  t("another repo's path → suppressed as foreign  [regression guard]",
    sup("foreign").includes("tools/preset_audition.c"));
  t("exactly 2 broken, no more  [noise guard]", g.broken.length === 2);

  const bad = T.filter(x => !x.ok);
  for (const x of T) console.log(`  ${x.ok ? "\x1b[32m✓\x1b[0m" : "\x1b[31m✗\x1b[0m"} ${x.n}`);
  console.log(bad.length
    ? `\x1b[31mstale-doc-check --selfcheck FAILED\x1b[0m — ${bad.length} of ${T.length} expectations broken`
    : `stale-doc-check --selfcheck: ${T.length}/${T.length} known answers correct`);
  process.exit(bad.length ? 1 : 0);
}
// DE_DOCS_DIR aims the scan at a fixture instead of docs/ — used by --selfcheck. ROOT stays the real
// repo on purpose, so the fixture is adjudicated against REAL srcExists + git history, exactly as in
// production (an untracked fixture doc has no git date, so line 240 skips it for the mtime tiers).
const DOCS = process.env.DE_DOCS_DIR ? path.resolve(process.env.DE_DOCS_DIR) : path.join(ROOT, "docs");
const TOOLS = path.join(ROOT, "tools");

// ---- args ----
const argv = process.argv.slice(2);
const scope = argv.find(a => !a.startsWith("--")) || "";
const json = argv.includes("--json");
const strict = argv.includes("--strict");
const daysIdx = argv.indexOf("--days");
const graceDays = daysIdx >= 0 ? parseInt(argv[daysIdx + 1], 10) || 0 : 0;
const showAll = argv.includes("--all"); // list the suppressed proposal/foreign refs too
const showDocs = argv.includes("--docs"); // expand the noisy doc→doc churn tier (default: count only)
const driftable = argv.includes("--driftable");
const selfcheck = argv.includes("--selfcheck"); // curated registry mode (see below); ignores the heuristic tiers
const touchesScope = (...rels) => !scope || rels.some(r => r.toLowerCase().includes(scope.toLowerCase()));

// ---- git last-commit date for every tracked path, in ONE pass ----
// git log walks newest→oldest; the FIRST date we see a path under is its last change.
const gitDate = new Map();
{
  const out = execFileSync("git", ["-C", ROOT, "log", "--format=%cs", "--name-only", "--no-renames"],
    { encoding: "utf8", maxBuffer: 128 * 1024 * 1024 });
  let cur = null;
  for (const line of out.split("\n")) {
    if (/^\d{4}-\d{2}-\d{2}$/.test(line)) { cur = line; continue; }
    if (!line || !cur) continue;
    if (!gitDate.has(line)) gitDate.set(line, cur);
  }
}
const dateOf = rel => gitDate.get(rel) || null; // YYYY-MM-DD, lexically = chronologically comparable
const daysBetween = (a, b) => Math.round((Date.parse(b) - Date.parse(a)) / 86400000);

// ---- collect docs (skip archive/ — staleness there is the point) ----
function walk(dir, out = []) {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) { if (e.name === "archive") continue; walk(p, out); }
    else if (e.name.endsWith(".md")) out.push(p);
  }
  return out;
}
const docFiles = walk(DOCS);

// ============================================================================
// --driftable — the CURATED registry (opposite of the heuristic tiers below).
// A doc that freezes a tool's output DECLARES it, in an invisible marker:
//
//   <!-- de:driftable cmd="node tools/api-usage.js --gaps" as-of="2026-06-22" -->
//   (optional: inputs="runtime/studio.h,tools/carts" — else defaults, see below)
//
// This mode does two things: prints the OVERVIEW (which docs are driftable, off
// which command, snapshotted when), and CHECKS each by comparing `as-of` against
// the newest last-commit date among that command's INPUTS. Inputs moved after the
// snapshot → "likely drifted, re-run and eyeball". No fuzzy inference, no auto-
// edit — a human declares the dependency and a human decides. Reuses the git-date
// engine above. Default inputs when none declared: the tool's own script + the
// cart shelf (`tools/carts`), the data almost all these tools read.
// ============================================================================
if (driftable) {
  const tty0 = process.stdout.isTTY;
  const b0 = s => (tty0 ? `\x1b[1m${s}\x1b[0m` : s);
  const d0 = s => (tty0 ? `\x1b[2m${s}\x1b[0m` : s);
  // newest last-commit date at/under a path (file → itself; dir → max over its files)
  const newestUnder = rel => {
    if (gitDate.has(rel)) return gitDate.get(rel);
    let best = null;
    const pref = rel.endsWith("/") ? rel : rel + "/";
    for (const [p, dt] of gitDate) if (p.startsWith(pref) && (!best || dt > best)) best = dt;
    return best;
  };
  // the WATCH vocabulary — what kind of thing in the doc can drift. Only `numbers`
  // is tool-verifiable (via cmd+as-of); the rest are PRIME-ONLY (honest "watch this"
  // metadata orient/build-context surface — no tool pretends to verify them). This
  // tool OWNS the vocab (like lint-carts owns tags). See docs/design/driftable-docs.md.
  const WATCH_VOCAB = { numbers: "tool count/table (verifiable)", checklist: "done/undone task state",
    carts: "a claim counting/enumerating carts", decisions: "an open/proposed choice that may get settled" };
  const badWatch = [];
  const entries = [];
  const MARK = /<!--\s*de:driftable\s+([^>]*?)-->/;
  // Also scan generated app SEO worksheets (apps/<name>/seo-brief.md) — driftable mode only,
  // so the heuristic mtime tiers below stay a pure docs/ report. aso-brief.js emits the marker.
  const APPS = path.join(ROOT, "apps");
  const appBriefs = fs.existsSync(APPS)
    ? fs.readdirSync(APPS, { withFileTypes: true }).filter(e => e.isDirectory())
        .map(e => path.join(APPS, e.name, "seo-brief.md")).filter(p => fs.existsSync(p))
    : [];
  for (const f of [...docFiles, ...appBriefs]) {
    const rel = path.relative(ROOT, f);
    if (!touchesScope(rel)) continue;
    let inFence = false;
    for (const line of fs.readFileSync(f, "utf8").split("\n")) {
      if (/^\s*```/.test(line)) { inFence = !inFence; continue; } // skip example markers in code fences
      if (inFence) continue;
      const m = line.match(MARK);
      if (!m) continue;
      const attrs = m[1];
      const get = k => (attrs.match(new RegExp(`${k}="([^"]*)"`)) || [])[1] || null;
      const cmd = get("cmd"), asOf = get("as-of"), inputsAttr = get("inputs");
      const watch = (get("watch") || (cmd ? "numbers" : "")).split(",").map(s => s.trim()).filter(Boolean);
      for (const w of watch) if (!WATCH_VOCAB[w]) badWatch.push({ rel, w });
      // resolve inputs: declared list, else [tool-script, tools/carts]
      let inputs = inputsAttr ? inputsAttr.split(",").map(s => s.trim()).filter(Boolean) : [];
      const script = cmd && (cmd.match(/tools\/[\w.-]+\.(?:js|sh|cjs)/) || [])[0];
      if (!inputsAttr) { if (script) inputs.push(script); inputs.push("tools/carts"); }
      const inputDates = inputs.map(p => ({ p, dt: newestUnder(p) })).filter(x => x.dt);
      const newest = inputDates.reduce((a, x) => (!a || x.dt > a.dt ? x : a), null);
      const drifted = !!(asOf && newest && newest.dt > asOf);
      entries.push({ rel, watch, cmd, asOf, inputs, newest, drifted, lag: drifted ? daysBetween(asOf, newest.dt) : 0 });
    }
  }
  if (json) { console.log(JSON.stringify({ driftable: entries }, null, 2)); process.exitCode = strict && entries.some(e => e.drifted) ? 1 : 0; return; }
  if (!entries.length) {
    console.log("no `de:driftable` docs registered" + (scope ? ` matching "${scope}"` : "") +
      ".\n  mark one with:  " + d0(`<!-- de:driftable cmd="node tools/foo.js" as-of="YYYY-MM-DD" -->`));
    process.exit(0);
  }
  const drift = entries.filter(e => e.drifted), fresh = entries.filter(e => !e.drifted);
  console.log(b0(`DRIFTABLE DOCS (${entries.length}) — hand-declared, curated (not inferred):\n`));
  for (const e of [...drift, ...fresh]) {
    // numbers is the only verifiable kind; prime-only kinds just get named
    const primeKinds = e.watch.filter(w => w !== "numbers");
    const tag = e.watch.includes("numbers") || e.cmd
      ? (e.drifted ? b0("⚠ LIKELY DRIFTED") : (e.asOf ? "✓ fresh" : d0("· no as-of, can't check")))
      : d0("· prime-only (not tool-verifiable)");
    console.log(`  ${b0(e.rel)}  ${tag}` + (e.watch.length ? d0(`   watch: ${e.watch.join(", ")}`) : ""));
    if (e.cmd || e.asOf) console.log(d0(`      cmd: ${e.cmd || "(none)"}   snapshot: ${e.asOf || "(undated)"}`));
    if (e.newest && (e.watch.includes("numbers") || e.cmd)) console.log(d0(`      inputs newest: ${e.newest.p} @ ${e.newest.dt}` +
      (e.drifted ? `  → ${e.lag}d after snapshot; re-run cmd + eyeball` : "")));
    if (primeKinds.length) console.log(d0(`      prime-only: ${primeKinds.join(", ")} — orient nudges; no auto-check`));
  }
  if (badWatch.length) {
    console.log("\n" + b0(`unknown watch kind(s) — not in the vocab {${Object.keys(WATCH_VOCAB).join(", ")}}:`));
    for (const x of badWatch) console.log(d0(`  ${x.rel}: "${x.w}"`));
  }
  console.log(d0(`\n${entries.length} registered · ${drift.length} likely drifted · ${fresh.length} fresh · curated (declared, not inferred)`));
  process.exit(strict && drift.length ? 1 : 0);
}

// ---- entity universe: hyphenated basenames of tools + docs ----
// Rule (matches lint-xrefs): contains "-" AND has an alpha segment ≥4 chars.
const hyphenated = b => b.includes("-") && b.split("-").some(seg => /^[a-z]{4,}$/i.test(seg));
const entities = new Map(); // name -> { rel, kind }
// tools: tools/*.js and tools/*.sh (not subdirs — carts/ single-word names over-fire)
for (const e of fs.readdirSync(TOOLS, { withFileTypes: true })) {
  if (!e.isFile()) continue;
  const m = e.name.match(/^(.+)\.(js|sh)$/);
  if (!m || !hyphenated(m[1])) continue;
  if (!entities.has(m[1])) entities.set(m[1], { rel: path.relative(ROOT, path.join(TOOLS, e.name)), kind: "tool" });
}
// docs
for (const f of docFiles) {
  const b = path.basename(f, ".md");
  if (!hyphenated(b)) continue;
  if (!entities.has(b)) entities.set(b, { rel: path.relative(ROOT, f), kind: "doc" });
}

// ---- per-doc scan ----
const esc = s => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
const findings = []; // { doc, docDate, docDateSrc, name, kind, entRel, entDate, lag, ln, text }

for (const f of docFiles) {
  const rel = path.relative(ROOT, f);
  const text = fs.readFileSync(f, "utf8");
  const lines = text.split("\n");

  // doc date: leading `updated:` frontmatter, else git
  let docDate = null, docDateSrc = "git";
  const fm = text.match(/^---\n([\s\S]*?)\n---/);
  if (fm) {
    const u = fm[1].match(/^updated:\s*(\d{4}-\d{2}-\d{2})/m);
    if (u) { docDate = u[1]; docDateSrc = "frontmatter"; }
  }
  if (!docDate) docDate = dateOf(rel);
  if (!docDate) continue; // untracked / unknown — nothing to compare against

  // mask fenced lines
  const fenced = [];
  let inFence = false;
  for (const l of lines) { if (/^\s*```/.test(l)) inFence = !inFence; fenced.push(inFence); }

  const selfBase = path.basename(f, ".md");
  for (const [name, ent] of entities) {
    if (name === selfBase) continue;            // a doc naming itself isn't stale-vs-self
    if (ent.rel === rel) continue;
    const entDate = dateOf(ent.rel);
    if (!entDate) continue;
    const lag = daysBetween(docDate, entDate);  // entity newer than doc by this many days
    if (lag <= graceDays) continue;             // entity older/same (or within grace) → fine
    // find first non-fenced mention
    const re = new RegExp(`(?<![\\w/.(-])${esc(name)}(?:\\.(?:js|sh|md|c))?(?![\\w-])`);
    for (let i = 0; i < lines.length; i++) {
      if (fenced[i]) continue;
      if (re.test(lines[i])) {
        findings.push({ doc: rel, docDate, docDateSrc, name, kind: ent.kind, entRel: ent.rel, entDate, lag, ln: i + 1, text: lines[i].trim() });
        break;
      }
    }
  }
}

const shown = findings.filter(x => touchesScope(x.doc, x.entRel));

// ============================================================================
// BROKEN REFERENCES — the high-confidence tier. Not "something changed" but
// "this claim is false NOW": a doc cites a code path that doesn't exist, or a
// `tool --flag` the tool doesn't have. Falsifiable, so worth fixing on sight.
// (The mtime tiers above are nudges; this tier is real issues.)
// ============================================================================

// index existing source files by relpath (RECURSIVELY — nested files like
// data-tools/roadview/osm-roads.js or tools/det-probes/run.sh are real; a flat
// listing would falsely flag them dead).
const SRC_DIRS = ["tools", "runtime", "editor/src", "editor/electron", "data-tools"];
const srcExists = new Set();       // relpaths that exist
const toolSource = new Map();      // tool basename -> source text (for flag checks; top-level tools/ only)
function indexDir(rel) {
  let ents; try { ents = fs.readdirSync(path.join(ROOT, rel), { withFileTypes: true }); } catch { return; }
  for (const e of ents) {
    const child = `${rel}/${e.name}`;
    if (e.isDirectory()) { if (e.name === "node_modules") continue; indexDir(child); }
    else srcExists.add(child);
  }
}
for (const d of SRC_DIRS) indexDir(d);
// flags belong to CLI tools that live at tools/ top level — index their source by basename
for (const e of fs.readdirSync(TOOLS, { withFileTypes: true })) {
  if (!e.isFile()) continue;
  const m = e.name.match(/^(.+)\.(js|sh|cjs)$/);
  if (m && !toolSource.has(m[1])) toolSource.set(m[1], fs.readFileSync(path.join(TOOLS, e.name), "utf8"));
}

// placeholder paths that are meant to be filled in, not real files
const isPlaceholder = p => /(?:^|\/)(?:x|foo|bar|baz|name|my)\.(?:js|c|h|sh)$/i.test(p) ||
  /XX|<[a-z]|NN-|\bname\.(?:c|cart)/i.test(p);
// proposal cues: the doc is sketching a flag/file that doesn't exist YET, not claiming it does.
//
// TRIAGED 2026-07-30. This tier is the one CLAUDE.md tells agents to TRUST ("real issues"), and all
// 47 of its findings were false positives — a 0% true-positive rate. Two causes, both fixed here:
//
//   1. `proposalCue` was only ever applied to FLAGS. The path loop ran BEFORE the check, so a design
//      doc naming the file it wants CREATED was reported as a broken reference — which is the entire
//      point of a design doc. 35 of the 47. The cue list below grew the vocabulary this repo actually
//      uses for that ("extract to", "graduate to", "carve into", "does not exist", "drop this in",
//      the `gen-x.js → runtime/y.h` generation arrow).
//   2. External-repo paths. `PATH_RE` matches on a \b, which fires *after* a slash, so
//      `~/Projects/navkit/soundsystem/tools/preset_audition.c` matched as `tools/preset_audition.c`
//      and was looked up in THIS repo. 12 of the 47 — including two in `other-projects.md`, a doc
//      that pre-emptively says "Paths below are absolute filesystem paths … so the doc linters don't
//      try to resolve them". It tried anyway.
//
// Suppressed findings stay INSPECTABLE (`--all`) and their count is always printed. A check that
// silently drops things is how this tier would rot in the other direction.
const proposalCue = l => /\b(or a|would|could|propos|future|someday|todo|maybe|might|imagine|instead of|we('d| would)|a `?--)\b/i.test(l)
  || /\b(does ?n[o']t exist|not built|no such|prerequisite: ?build|extract(ion)? to|extract to|graduate (the )?\w+ )/i.test(l)
  || /\b(carve|drop this in|sibling to|endgame is|its own tool|first tool|waits for|will (be|emit|produce)|to be (built|created|added)|planned)\b/i.test(l)
  || /\b[a-z0-9-]+\.(?:js|sh)\s*→\s*(?:runtime|tools)\//i.test(l);   // a generation arrow names its OUTPUT

// THE DISCRIMINATOR that made this tier honest. A path can only be a BROKEN reference if the file
// once existed and is now gone (renamed, deleted, moved) — that is the bug: prose left pointing at a
// file that moved out from under it. A path that has NEVER existed in this repo's history cannot be
// that; it is a doc naming something it wants BUILT, which is what a design doc is for.
//
// Measured 2026-07-30: of the 47 findings this tier reported, ZERO had ever existed. All 47 were
// proposals or other repos' paths, i.e. a 0% true-positive rate on the tier CLAUDE.md tells agents to
// trust. Line-level "proposal cue" regexes could not fix that, because the cue is usually in the
// surrounding paragraph or the section heading, not the line with the path on it.
//
// Cost is one `git log --all --diff-filter=A` (~1.8s, 5k paths), so it runs LAZILY — only when there
// is at least one candidate to adjudicate — and degrades to the cue heuristics if git is unavailable.
let _everExisted = null;
const everExisted = (p) => {
  if (_everExisted === null) {
    try {
      _everExisted = new Set(require("child_process")
        .execFileSync("git", ["log", "--all", "--pretty=format:", "--name-only", "--diff-filter=AR"],
                      { cwd: ROOT, encoding: "utf8", maxBuffer: 1 << 28 })
        .split("\n").map(x => x.trim()).filter(Boolean));
    } catch { _everExisted = new Set(); }
  }
  return _everExisted.size === 0 ? null : _everExisted.has(p);   // null = unknown, fall back to cues
};

// a path that belongs to ANOTHER repo on this machine, not to dreamengine
const FOREIGN_DOC = /(?:^|\/)other-projects\.md$/;                       // self-declared outward-pointing hub
const foreignCue = (l, at) => {
  const before = l.slice(Math.max(0, at - 60), at);
  if (/[~/]$/.test(before)) return true;                                   // …/navkit/soundsystem/tools/x.c
  return /(navkit|soundsystem|\/Users\/|~\/Projects\/|nikkikoole\.github\.io)/i.test(before);
};

const PATH_RE = /\b((?:tools|runtime|editor\/src|editor\/electron|data-tools|det-probes)\/[A-Za-z0-9_./-]+\.(?:js|cjs|c|h|sh))\b/g;
// tool basename (real, from toolSource) optionally .js/.sh — each --flag is bound to the
// NEAREST preceding real tool within a short window. (A single left-to-right regex paired
// the flag with the EARLIEST tool on the line instead: "mirror-diff 68=68, road-check --all"
// blamed mirror-diff for road-check's flag — a false dead-flag, caught 2026-07-10.)
const TOOLNAME_RE = /\b([a-z][a-z0-9-]{2,})(?:\.(?:js|sh))?\b/g;
const FLAGONLY_RE = /--[a-z][a-z0-9-]{2,}/g;

const broken = [];   // { doc, ln, kind:'path'|'flag', ref, text }
const suppressed = []; // same shape + why:'proposal'|'foreign' — visible via --all, always counted
for (const f of docFiles) {
  const rel = path.relative(ROOT, f);
  if (!touchesScope(rel)) continue;
  const lines = fs.readFileSync(f, "utf8").split("\n");
  const seen = new Set();
  lines.forEach((l, i) => {
    let m;
    const lineIsProposal = proposalCue(l);
    PATH_RE.lastIndex = 0;
    while ((m = PATH_RE.exec(l))) {
      const p = m[1];
      if (srcExists.has(p) || isPlaceholder(p)) continue;
      const k = "p:" + p + ":" + i; if (seen.has(k)) continue; seen.add(k); // per-LINE, so a fixer sees every occurrence
      const rec = { doc: rel, ln: i + 1, kind: "path", ref: p, text: l.trim() };
      let why = null;
      if (FOREIGN_DOC.test(rel) || foreignCue(l, m.index)) why = "foreign";
      else {
        const ever = everExisted(p);
        // never in history → aspirational, whatever the prose says. Once in history → a real broken
        // reference EVEN IF the line reads like a proposal (that is prose left behind by a rename).
        if (ever === false) why = "proposal";
        else if (ever === null && lineIsProposal) why = "proposal";
        else if (ever === true) rec.wasRemoved = true;
      }
      if (why) suppressed.push({ ...rec, why }); else broken.push(rec);
    }
    if (lineIsProposal) return; // a line sketching a possible flag isn't a false claim
    const toolsOnLine = [];
    TOOLNAME_RE.lastIndex = 0;
    while ((m = TOOLNAME_RE.exec(l)))
      if (toolSource.has(m[1])) toolsOnLine.push({ name: m[1], end: m.index + m[0].length });
    FLAGONLY_RE.lastIndex = 0;
    while ((m = FLAGONLY_RE.exec(l))) {
      const flag = m[0], at = m.index;
      const owner = toolsOnLine
        .filter(t => t.end <= at && at - t.end <= 25 && !/[`]/.test(l.slice(t.end, at)))
        .pop(); // nearest preceding real tool wins
      if (!owner || toolSource.get(owner.name).includes(flag)) continue;
      const k = "f:" + owner.name + flag + ":" + i; if (seen.has(k)) continue; seen.add(k);
      const rec = { doc: rel, ln: i + 1, kind: "flag", ref: `${owner.name} ${flag}`, text: l.trim() };
      // a roadmap rung ("→ 7 (`build-app.js --android` …)") or a named follow-up is a plan, not a claim
      if (/→\s*\d|open follow-?up|rung \d|\bstaging path\b/i.test(l)) suppressed.push({ ...rec, why: "proposal" });
      else broken.push(rec);
    }
  });
}

// ---- report ----
if (json) {
  console.log(JSON.stringify({ graceDays, scope: scope || null, broken, suppressed, findings: shown }, null, 2));
  // NOT process.exit() here: on a PIPE, stdout is async, and exiting truncates a large payload
  // mid-object — this report is ~65 KB, so `--json | jq` was failing to parse for anyone who tried.
  // Setting exitCode lets node flush and exit naturally.
  process.exitCode = strict && (broken.length || shown.length) ? 1 : 0;
  return;
  process.exit(strict && (broken.length || shown.length) ? 1 : 0);
}

const tty = process.stdout.isTTY;
const bold = s => (tty ? `\x1b[1m${s}\x1b[0m` : s);
const dim = s => (tty ? `\x1b[2m${s}\x1b[0m` : s);
const clip = (s, n) => (s.length > n ? s.slice(0, n - 1) + "…" : s);

if (scope) console.log(dim(`scoped to paths matching "${scope}"\n`));

// TWO TIERS, by confidence:
//   A. TOOL DRIFT (high signal) — a doc describes a tool whose code changed after it.
//      Prose about behavior/flags may now be wrong. Shown in full.
//   B. DOC CHURN (review) — a doc names another doc that was edited later. Mostly
//      normal churn in an active corpus (a live backlog like lint-xrefs's), so it's
//      collapsed to a count by default; --docs expands it.
const toolTier = shown.filter(x => x.kind === "tool");
const docTier = shown.filter(x => x.kind === "doc");

function printTier(items, heading) {
  const byDoc = new Map();
  for (const x of items) (byDoc.get(x.doc) || byDoc.set(x.doc, []).get(x.doc)).push(x);
  const docsSorted = [...byDoc.entries()].sort((a, b) =>
    b[1].length - a[1].length || Math.max(...b[1].map(x => x.lag)) - Math.max(...a[1].map(x => x.lag)));
  console.log(bold(heading));
  for (const [doc, list] of docsSorted) {
    const d0 = list[0];
    console.log(`\n  ${bold(doc)} ${dim(`(last ${d0.docDateSrc === "frontmatter" ? "updated" : "commit"} ${d0.docDate})`)}`);
    for (const x of list.sort((a, b) => b.lag - a.lag)) {
      console.log(`    ${x.name} ${dim(`(${x.kind})`)} changed ${x.entDate} ` + dim(`— ${x.lag}d newer · ${doc}:${x.ln}`));
      console.log(dim(`      ${clip(x.text, 96)}`));
    }
  }
  console.log("");
}

// TOP TIER: broken references — real, falsifiable issues. Guides/root docs first
// (a dead ref in a how-to breaks a reader), then design/ (more likely a sketch).
if (broken.length) {
  // rank: docs with a dead FLAG first (flags are high-precision real bugs; paths can
  // be planned-not-built), then guides/root docs before design/ (a how-to must work).
  const hasFlag = list => list.some(b => b.kind === "flag");
  const weight = d => (/^docs\/(guides|[^/]+\.md)/.test(d) ? 0 : 1);
  const byDoc = new Map();
  for (const b of broken) (byDoc.get(b.doc) || byDoc.set(b.doc, []).get(b.doc)).push(b);
  const sorted = [...byDoc.entries()].sort((a, b) =>
    (hasFlag(b[1]) - hasFlag(a[1])) || (weight(a[0]) - weight(b[0])) || b[1].length - a[1].length);
  const nf = broken.filter(b => b.kind === "flag").length, np = broken.length - nf;
  console.log(bold(`BROKEN REFERENCES (${nf} dead flag${nf !== 1 ? "s" : ""}, ${np} missing path${np !== 1 ? "s" : ""}) — cited as EXISTING but not present now:`));
  console.log(dim(`  (each once EXISTED in git history and is now gone — prose left pointing at a renamed/deleted file.`));
  console.log(dim(`   never-built proposals and other repos' paths are suppressed; see the count below)`));
  for (const [doc, list] of sorted) {
    console.log(`\n  ${bold(doc)}`);
    for (const b of list.sort((x, y) => (x.kind === "flag" ? 0 : 1) - (y.kind === "flag" ? 0 : 1) || x.ln - y.ln)) {
      console.log(`    ${b.kind === "path" ? "missing path" : bold("dead flag")}: ${bold(b.ref)} ` + dim(`· ${doc}:${b.ln}`));
      console.log(dim(`      ${clip(b.text, 96)}`));
    }
  }
  console.log("");
}

// Suppression is always visible: a check that silently drops findings rots the other way.
if (suppressed.length) {
  const np = suppressed.filter(x => x.why === "proposal").length;
  const nfgn = suppressed.length - np;
  console.log(dim(`suppressed ${suppressed.length} reference(s) as not-a-claim: `
    + `${np} proposal/not-yet-built · ${nfgn} another repo's path`)
    + (showAll ? "" : dim("   → --all to list them")));
  if (showAll) for (const x of suppressed.sort((a, b) => a.doc.localeCompare(b.doc) || a.ln - b.ln))
    console.log(dim(`    [${x.why}] ${x.ref} · ${x.doc}:${x.ln}\n      ${clip(x.text, 92)}`));
  console.log("");
}

// MTIME TIERS: nudges, not proven issues.
if (toolTier.length) {
  printTier(toolTier, `TOOL DRIFT (${toolTier.length}) — doc describes a tool whose code changed after it:`);
}
if (docTier.length) {
  if (showDocs) printTier(docTier, `DOC CHURN (${docTier.length}) — doc names another doc edited later (review):`);
  else console.log(dim(`DOC CHURN (${docTier.length}) — doc→doc mentions edited later; run with --docs to expand.\n`));
}

if (!broken.length && !shown.length)
  console.log("no broken references or possibly-stale mentions found" + (scope ? ` for "${scope}"` : "") + ".");

console.log(dim(`${docFiles.length} docs · ${entities.size} entities · ` +
  `${bold(broken.length + " broken")} · ${toolTier.length} tool-drift · ${docTier.length} doc-churn` +
  `${scope ? ` (of ${findings.length} mtime-total)` : ""}` +
  (graceDays ? ` · grace ${graceDays}d` : "") + " · advisory"));

process.exit(strict && shown.length ? 1 : 0);
