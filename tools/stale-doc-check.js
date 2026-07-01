#!/usr/bin/env node
// ============================================================================
// stale-doc-check.js — flag docs that MENTION something that changed AFTER the
// doc was last touched. The cheap, zero-upkeep freshness nudge.
//
//   node tools/stale-doc-check.js               # full report (whole docs/ corpus)
//   node tools/stale-doc-check.js tools-we-need # SCOPED: only docs whose path matches
//   node tools/stale-doc-check.js --docs         # also expand the doc→doc churn tier
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
const DOCS = path.join(ROOT, "docs");
const TOOLS = path.join(ROOT, "tools");

// ---- args ----
const argv = process.argv.slice(2);
const scope = argv.find(a => !a.startsWith("--")) || "";
const json = argv.includes("--json");
const strict = argv.includes("--strict");
const daysIdx = argv.indexOf("--days");
const graceDays = daysIdx >= 0 ? parseInt(argv[daysIdx + 1], 10) || 0 : 0;
const showDocs = argv.includes("--docs"); // expand the noisy doc→doc churn tier (default: count only)
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
// proposal cues: the doc is sketching a flag/file that doesn't exist YET, not claiming it does
const proposalCue = l => /\b(or a|would|could|propos|future|someday|todo|maybe|might|imagine|instead of|we('d| would)|a `?--)\b/i.test(l);

const PATH_RE = /\b((?:tools|runtime|editor\/src|editor\/electron|data-tools|det-probes)\/[A-Za-z0-9_./-]+\.(?:js|cjs|c|h|sh))\b/g;
// tool basename (real, from toolSource) optionally .js/.sh, then a --flag within a short window
const FLAG_RE = /\b([a-z][a-z0-9-]{2,})(?:\.(?:js|sh))?\b[^`\n]{0,25}?(--[a-z][a-z0-9-]{2,})/g;

const broken = []; // { doc, ln, kind:'path'|'flag', ref, text }
for (const f of docFiles) {
  const rel = path.relative(ROOT, f);
  if (!touchesScope(rel)) continue;
  const lines = fs.readFileSync(f, "utf8").split("\n");
  const seen = new Set();
  lines.forEach((l, i) => {
    let m;
    PATH_RE.lastIndex = 0;
    while ((m = PATH_RE.exec(l))) {
      const p = m[1];
      if (srcExists.has(p) || isPlaceholder(p)) continue;
      const k = "p:" + p + ":" + i; if (seen.has(k)) continue; seen.add(k); // per-LINE, so a fixer sees every occurrence
      broken.push({ doc: rel, ln: i + 1, kind: "path", ref: p, text: l.trim() });
    }
    if (proposalCue(l)) return; // a line sketching a possible flag isn't a false claim
    FLAG_RE.lastIndex = 0;
    while ((m = FLAG_RE.exec(l))) {
      const tool = m[1], flag = m[2];
      const src = toolSource.get(tool);
      if (!src || src.includes(flag)) continue;
      const k = "f:" + tool + flag + ":" + i; if (seen.has(k)) continue; seen.add(k);
      broken.push({ doc: rel, ln: i + 1, kind: "flag", ref: `${tool} ${flag}`, text: l.trim() });
    }
  });
}

// ---- report ----
if (json) {
  console.log(JSON.stringify({ graceDays, scope: scope || null, broken, findings: shown }, null, 2));
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
  console.log(bold(`BROKEN REFERENCES (${nf} dead flag${nf !== 1 ? "s" : ""}, ${np} missing path${np !== 1 ? "s" : ""}) — cited but not present now:`));
  console.log(dim(`  (dead flags = near-certain doc bugs; missing paths may be planned-not-built files, esp. in design/)`));
  for (const [doc, list] of sorted) {
    console.log(`\n  ${bold(doc)}`);
    for (const b of list.sort((x, y) => (x.kind === "flag" ? 0 : 1) - (y.kind === "flag" ? 0 : 1) || x.ln - y.ln)) {
      console.log(`    ${b.kind === "path" ? "missing path" : bold("dead flag")}: ${bold(b.ref)} ` + dim(`· ${doc}:${b.ln}`));
      console.log(dim(`      ${clip(b.text, 96)}`));
    }
  }
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
