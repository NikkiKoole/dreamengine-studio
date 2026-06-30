#!/usr/bin/env node
// ============================================================================
// topic-brief.js — assemble a feature/THEME dossier across the whole repo.
//
//   node tools/topic-brief.js "touch controls" gamepad joystick
//   node tools/topic-brief.js touch-controls
//   node tools/topic-brief.js audio reverb shimmer
//
// THE FRICTION THIS KILLS. build-context/orient are VERTICAL — one cart, deep.
// They answer "what do I need to work on cart X." But a cross-cutting feature
// (the iOS virtual gamepad, spatial audio, the road renderer) is HORIZONTAL:
// it spans engine code + a design doc + a reference cart + N consumer carts +
// STATUS items + ADRs + a punchlist — and the one design doc that holds the
// real plan is often NOT linked from the doc you started in (ios-plan.md talks
// about the gamepad for paragraphs and never points at touch-controls.md, which
// is the actual 7-step plan). A code-first or single-doc orientation misses it.
// This sweeps the whole corpus for a TERM-SET and assembles the feature picture:
// the design docs (with their STATUS line + plan progress), the ledger items,
// the engine seam, the carts that demo/want it, and the related tools.
//
// This is the `topic:<x>` slice that build-context.js's header sketches as
// planned (docs/design/tools-we-need.md #1 — `build-context roads`). The
// cart:<name> slice stays in build-context.js; this is its theme-keyed sibling.
//
// IT DOES NOT judge cross-link health beyond a one-line nudge — that's
// lint-xrefs.js (docs that should link each other but don't). Pointed at the end.
//
// WHAT IT ASSEMBLES (all from data that already exists):
//   1. DESIGN DOCS & GUIDES   docs/design + docs/guides hits, each with its
//                             STATUS: line + embedded plan progress (☐/☑ counts)
//   2. DECISIONS (ADRs)       docs/decisions hits, with the Status: line
//   3. LEDGER                 STATUS.md / cart-polish-punchlist.md hits (the line)
//   4. FIELD NOTES            docs/field-notes research-journal hits
//   5. ENGINE / CODE          runtime/*.{h,c} + web_shell.html — the seam
//   6. CARTS                  carts whose source/de:meta mention it (probe vs use)
//   7. RELATED TOOLS          tools/ whose header mentions it
// Ranked by how many distinct query terms a file matches, then hit count.
// ============================================================================

const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "..");
const SRC_DIR = path.join(ROOT, "tools/carts");
let readMeta, flattenDesc;
try { ({ readMeta, flattenDesc } = require("./build-cart-index.js")); } catch { /* carts section degrades gracefully */ }
const { extractStatus, planProgress } = require("./doc-status.js");

// ---- args ----
const argv = process.argv.slice(2);
const terms = argv.filter(a => !a.startsWith("--"));
if (!terms.length) {
  console.error('usage: node tools/topic-brief.js <term> [more terms…]   e.g. "touch controls" gamepad joystick');
  process.exit(2);
}

// ---- styling (plain text unless a TTY) ----
const tty = process.stdout.isTTY;
const bold = s => (tty ? `\x1b[1m${s}\x1b[0m` : s);
const dim = s => (tty ? `\x1b[2m${s}\x1b[0m` : s);
const head = s => "\n" + bold(s) + "\n" + dim("─".repeat(s.length));

// ---- term matchers ----
// each term may be multi-word ("touch controls") or hyphenated (touch-controls);
// both match "touch controls" / "touch-controls" / "touch_controls", whole-word.
const esc = s => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
function termRe(term) {
  const words = term.trim().split(/[\s\-_]+/).filter(Boolean).map(esc);
  const body = words.join("[\\s\\-_]+");
  return new RegExp(`(?<![A-Za-z0-9])${body}(?![A-Za-z0-9])`, "i");
}
const matchers = terms.map(t => ({ term: t, re: termRe(t) }));

// score a text blob: how many DISTINCT query terms it contains
function distinctTerms(text) {
  let n = 0;
  for (const m of matchers) if (m.re.test(text)) n++;
  return n;
}
// collect matching lines, richest first (lines hitting the most terms), capped
function matchLines(lines, cap = 3) {
  const out = [];
  for (let i = 0; i < lines.length; i++) {
    let hits = 0;
    for (const m of matchers) if (m.re.test(lines[i])) hits++;
    if (hits) out.push({ ln: i + 1, text: lines[i].trim(), hits });
  }
  out.sort((a, b) => b.hits - a.hits || a.ln - b.ln);
  return { shown: out.slice(0, cap), total: out.length };
}

// ---- corpus walk ----
function walk(dir, exts, acc, skipDirs = new Set()) {
  let ents;
  try { ents = fs.readdirSync(dir, { withFileTypes: true }); } catch { return; }
  for (const e of ents) {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) {
      if (skipDirs.has(e.name)) continue;
      walk(p, exts, acc, skipDirs);
    } else if (exts.includes(path.extname(e.name))) acc.push(p);
  }
}
const SKIP_DIRS = new Set(["node_modules", "cache", ".git", "archive"]);
const SKIP_FILE = /(cart-compendium\.html|history\.html|FIELD-NOTES\.md)$/;

const docFiles = [];
walk(path.join(ROOT, "docs"), [".md"], docFiles, SKIP_DIRS);

// score a file → { rel, terms, lines:{shown,total}, status, plan, text }
function scoreDoc(f) {
  let text;
  try { text = fs.readFileSync(f, "utf8"); } catch { return null; }
  if (SKIP_FILE.test(f)) return null;
  const nterms = distinctTerms(text);
  if (!nterms) return null;
  const lines = text.split("\n");
  return {
    rel: path.relative(ROOT, f),
    terms: nterms,
    lines: matchLines(lines),
    status: extractStatus(lines),
    plan: planProgress(text),
  };
}

const docs = docFiles.map(scoreDoc).filter(Boolean)
  .sort((a, b) => b.terms - a.terms || b.lines.total - a.lines.total);

// bucket docs by home
const inDir = (d, seg) => d.rel.startsWith(`docs/${seg}/`);
const isLedger = d => /docs\/STATUS\.md$/.test(d.rel) || /punchlist/.test(d.rel) || /backlog/.test(d.rel);
const designDocs = docs.filter(d => (inDir(d, "design") || inDir(d, "guides")) && !isLedger(d));
const adrs       = docs.filter(d => inDir(d, "decisions"));
const ledger     = docs.filter(isLedger);
const fieldNotes = docs.filter(d => inDir(d, "field-notes"));
const otherDocs  = docs.filter(d => !designDocs.includes(d) && !adrs.includes(d) &&
                                    !ledger.includes(d) && !fieldNotes.includes(d));

// ---- engine / code (ours only — skip vendored Raylib headers) ----
const VENDOR_RE = /(raylib-web\/|raylib\/include\/|^runtime\/raylib\.h$|^runtime\/raymath\.h$)/;
const codeFiles = [];
walk(path.join(ROOT, "runtime"), [".h", ".c"], codeFiles, SKIP_DIRS);
const webShell = path.join(ROOT, "runtime/web_shell.html");
if (fs.existsSync(webShell)) codeFiles.push(webShell);
const code = codeFiles.filter(f => !VENDOR_RE.test(path.relative(ROOT, f))).map(f => {
  let text; try { text = fs.readFileSync(f, "utf8"); } catch { return null; }
  const nterms = distinctTerms(text);
  if (!nterms) return null;
  return { rel: path.relative(ROOT, f), terms: nterms, lines: matchLines(fs.readFileSync(f, "utf8").split("\n"), 4) };
}).filter(Boolean).sort((a, b) => b.terms - a.terms || b.lines.total - a.lines.total);

// ---- carts (source + de:meta) ----
const cartHits = [];
if (readMeta) {
  for (const f of fs.readdirSync(SRC_DIR).filter(f => f.endsWith(".c"))) {
    const name = f.slice(0, -2);
    let text; try { text = fs.readFileSync(path.join(SRC_DIR, f), "utf8"); } catch { continue; }
    const nterms = distinctTerms(text);
    if (!nterms) continue;
    let meta = null; try { meta = readMeta(text, name); } catch { /* keep, title unknown */ }
    cartHits.push({
      name,
      title: meta && meta.title ? meta.title : name,
      kind: meta ? [].concat(meta.kind || []).join("/") : "",
      terms: nterms,
      lines: matchLines(text.split("\n"), 1),
    });
  }
  cartHits.sort((a, b) => b.terms - a.terms || b.lines.total - a.lines.total);
}

// ---- related tools (header only) ----
const toolHits = [];
for (const f of fs.readdirSync(path.join(ROOT, "tools")).filter(f => /\.(js|sh)$/.test(f))) {
  if (f === "topic-brief.js") continue;
  let text; try { text = fs.readFileSync(path.join(ROOT, "tools", f), "utf8"); } catch { continue; }
  const header = text.split("\n").slice(0, 60).join("\n");
  if (distinctTerms(header)) toolHits.push(f);
}

// ============================================================================
// render
// ============================================================================
const out = [];
out.push(bold(`TOPIC: ${terms.join(" · ")}`) + dim(`   (${docs.length} docs · ${code.length} code · ${cartHits.length} carts)`));

function docBlock(d) {
  let header = "  " + bold(d.rel) + dim(`  (${d.lines.total}× · ${d.terms}/${terms.length} terms)`);
  out.push(header);
  if (d.status) out.push(dim("    status: ") + clip(d.status, 100));
  if (d.plan)   out.push(dim(`    plan:   ${d.plan.done}/${d.plan.total} steps checked off`));
  for (const m of d.lines.shown) out.push(dim(`    ${m.ln}: `) + clip(m.text, 100));
  if (d.lines.total > d.lines.shown.length) out.push(dim(`    … +${d.lines.total - d.lines.shown.length} more`));
}

if (designDocs.length) { out.push(head("DESIGN DOCS & GUIDES")); designDocs.slice(0, 8).forEach(docBlock); }
if (adrs.length)       { out.push(head("DECISIONS (ADRs)"));     adrs.slice(0, 6).forEach(docBlock); }
if (ledger.length)     { out.push(head("LEDGER (status / backlog / punchlist)")); ledger.forEach(docBlock); }
if (fieldNotes.length) { out.push(head("FIELD NOTES")); fieldNotes.slice(0, 6).forEach(docBlock); }
if (otherDocs.length)  { out.push(head("OTHER DOCS"));  otherDocs.slice(0, 6).forEach(docBlock); }

if (code.length) {
  out.push(head("ENGINE / CODE (the seam)"));
  for (const c of code.slice(0, 6)) {
    out.push("  " + bold(c.rel) + dim(`  (${c.lines.total}×)`));
    for (const m of c.lines.shown) out.push(dim(`    ${m.ln}: `) + clip(m.text, 100));
    if (c.lines.total > c.lines.shown.length) out.push(dim(`    … +${c.lines.total - c.lines.shown.length} more`));
  }
}

if (cartHits.length) {
  out.push(head("CARTS THAT MENTION IT"));
  out.push(dim("  (a probe/demo cart is the reference; game/sim carts are consumers)"));
  for (const c of cartHits.slice(0, 12)) {
    const k = c.kind ? dim(` [${c.kind}]`) : "";
    out.push("  " + bold(c.name) + k + dim(`  ${c.title}`));
  }
  if (cartHits.length > 12) out.push(dim(`  … +${cartHits.length - 12} more`));
}

if (toolHits.length) {
  out.push(head("RELATED TOOLS"));
  out.push("  " + toolHits.join(", "));
}

out.push(head("NEXT"));
out.push(dim("  one cart, deep:        ") + "node tools/orient.js <cart>");
out.push(dim("  a studio.h API sig:    ") + "node tools/api.js <name>");
out.push(dim("  docs that should cross-link but don't: ") + "node tools/lint-xrefs.js");
out.push(dim("  which oracle to run:   ") + "docs/guides/checks-and-oracles.md");

console.log(out.join("\n"));

// ---- helpers ----
function clip(s, n) { return s.length > n ? s.slice(0, n - 1) + "…" : s; }
