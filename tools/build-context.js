#!/usr/bin/env node
// ============================================================================
// build-context.js — assemble a task-specific READING BRIEFING for one cart.
//
//   node tools/build-context.js <name>          # cart:<name> is the only subject (for now)
//   node tools/build-context.js roadview
//   node tools/build-context.js cart:roadview   # explicit subject prefix, same thing
//
// THE FRICTION THIS KILLS. "I want to work on cart X — what's the context?" The
// answer is scattered: the cart's own de:meta (title/lineage/todos), and — the
// part that bites — PROSE DOCS that reference the cart but AREN'T NAMED AFTER IT
// (roadview's whole todo list lives in docs/design/external-data-carts.md). You
// can't find that by guessing filenames; you have to grep, then read, then
// notice. This tool does that sweep for you and shows the matching LINE so you
// see *why* each doc is relevant before opening it.
//
// This is the cart-only terminal slice of the planned `build-context` tool
// (docs/design/action-plan.md Tier 1; docs/field-notes/tools-we-need.md #1;
// rationale in docs/field-notes/002-context-assembly.md). This does cart:<name>
// only. For a cross-cutting FEATURE/theme that spans many carts + docs + code
// (the topic:<x> subject sketched there), use its HORIZONTAL twin
// `tools/topic-brief.js "touch controls" gamepad`. api:<x>/task:<x> still unbuilt.
//
// IT DOES NOT re-check integrity or publish state — that's cart-info.js (source
// /embed/registry drift) and cart-status.js (rebake/publish). This tool only
// GATHERS reading material; it points at those two at the end.
//
// WHAT IT ASSEMBLES (all from data that already exists):
//   1. de:meta      title · kind/genre · status · created, the description, controls
//   2. lineage/homage  the prose "where this came from" fields
//   3. todo[]       the cart's own polish punch-list (cf. cart-todos.js)
//   4. related carts   forward (carts THIS cart's meta names) + reverse (carts
//                      whose meta names THIS one) — a cheap prose-mention graph
//   5. docs & notes the docs/ + data-tools/ files that mention the cart, each
//                   with its first matching line(s) — the headline feature
//   6. source       tools/carts/<name>.c (+ .cart.js) and data-tools that feed it
// ============================================================================

const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "..");
const SRC_DIR = path.join(ROOT, "tools/carts");
const CARTS_DIR = path.join(ROOT, "editor/public/carts");
const { readMeta, flattenDesc } = require("./build-cart-index.js");

// ---- args ----
const args = process.argv.slice(2);
const raw = args.find(a => !a.startsWith("--"));
if (!raw) {
  console.error("usage: node tools/build-context.js <cart-name>");
  process.exit(2);
}
// accept "roadview" or "cart:roadview"; reject other subjects for now (planned, not built)
const m = raw.match(/^(?:(\w+):)?(.+)$/);
const subject = m[1] || "cart";
const name = m[2];
if (subject !== "cart") {
  console.error(`only cart:<name> is supported yet — '${subject}:' subjects are planned (see header). Try: build-context cart:${name}`);
  process.exit(2);
}

// ---- styling (plain text unless a TTY) ----
const tty = process.stdout.isTTY;
const bold = s => (tty ? `\x1b[1m${s}\x1b[0m` : s);
const dim = s => (tty ? `\x1b[2m${s}\x1b[0m` : s);
const head = s => "\n" + bold(s) + "\n" + dim("─".repeat(s.length));

// ---- resolve the cart ----
const cPath = path.join(SRC_DIR, name + ".c");
if (!fs.existsSync(cPath)) {
  console.error(`no cart source at ${path.relative(ROOT, cPath)} — is the name right? (try: ls tools/carts/)`);
  process.exit(1);
}
let meta;
try {
  meta = readMeta(fs.readFileSync(cPath, "utf8"), name);
} catch (e) {
  console.error(`couldn't parse de:meta — ${e.message}`);
  process.exit(1);
}
if (!meta) {
  console.error(`${name} has no de:meta block (test/probe cart?) — nothing to assemble.`);
  process.exit(1);
}

// the full set of cart names, for the mention-graph
const allCarts = fs.readdirSync(SRC_DIR)
  .filter(f => f.endsWith(".c"))
  .map(f => f.slice(0, -2));

// Cart names that are ALSO everyday words: in free-prose lineage/description they
// match in their English sense ("any town: a different city", "WASD pan"), not as a
// cart reference — so they're excluded from the prose mention-graph below (NOT from
// the docs grep, which shows the line so you can judge a stray hit yourself). Names
// like puyo/xcom/cr78/moog are distinctive and stay in.
const COMMON_WORD_CARTS = new Set([
  "air", "boom", "city", "doom", "dub", "dune", "eq", "farm", "fire", "fm", "lofi",
  "lurk", "maze", "pan", "pet", "pipe", "pong", "pool", "reed", "rope", "sand", "say",
  "sims", "soak", "sort", "vox", "zoo",
]);
// candidate other-cart names for the graph: distinctive enough to be a real reference
const graphCarts = allCarts.filter(c => c.length >= 3 && !COMMON_WORD_CARTS.has(c));

// word-boundary, case-insensitive matcher for a token (escapes regex specials)
const esc = s => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
const wordRe = tok => new RegExp(`(?<![A-Za-z0-9_])${esc(tok)}(?![A-Za-z0-9_])`, "i");

// ============================================================================
// 1–3. de:meta headline, description, lineage, todos
// ============================================================================
const kinds = [].concat(meta.kind || []).join(", ");
const tags = [kinds, meta.genre].filter(Boolean).join(" · ");
const teaches = (meta.teaches || []).join(", ");

let out = [];
out.push(bold(`cart: ${meta.title || name}`) +
  (tags ? `   ${dim(tags)}` : "") +
  (meta.status ? `   ${dim("[" + meta.status + "]")}` : "") +
  (meta.created ? `   ${dim(meta.created)}` : ""));
if (teaches) out.push(dim(`teaches: ${teaches}`));

const desc = meta.description;
out.push(head("WHAT IT IS"));
if (typeof desc === "string") {
  out.push(wrap(desc));
} else if (desc && typeof desc === "object") {
  if (desc.summary) out.push(wrap(desc.summary));
  if (desc.detail) out.push("\n" + wrap(desc.detail));
  if (desc.controls) out.push("\n" + dim("controls: ") + wrap(desc.controls).trimStart());
} else {
  out.push(dim("(no description)"));
}

if (meta.lineage || meta.homage) {
  out.push(head("LINEAGE"));
  if (meta.lineage) out.push(wrap(meta.lineage));
  if (meta.homage) out.push((meta.lineage ? "\n" : "") + dim("homage: ") + wrap(meta.homage).trimStart());
}

out.push(head("TODO"));
const todos = meta.todo || [];
if (todos.length) todos.forEach(t => out.push("  ☐ " + wrap(t, 4).trimStart()));
else out.push(dim("  no open todos in this cart's de:meta."));

// ============================================================================
// 4. related carts — forward (named by this meta) + reverse (this name in theirs)
// ============================================================================
const metaText = [
  meta.lineage, meta.homage, flattenDesc(meta.description),
].filter(Boolean).join("\n");

const forward = graphCarts.filter(c => c !== name && wordRe(c).test(metaText));

const reverse = [];
for (const c of graphCarts) {
  if (c === name) continue;
  let cm;
  try { cm = readMeta(fs.readFileSync(path.join(SRC_DIR, c + ".c"), "utf8"), c); }
  catch { continue; }
  if (!cm) continue;
  const t = [cm.lineage, cm.homage, flattenDesc(cm.description)].filter(Boolean).join("\n");
  if (wordRe(name).test(t)) reverse.push(c);
}

if (forward.length || reverse.length) {
  out.push(head("RELATED CARTS"));
  if (forward.length) out.push("  this cart's notes mention:  " + forward.join(", "));
  if (reverse.length) out.push("  carts that mention this one: " + reverse.join(", "));
}

// ============================================================================
// 5. docs & notes that reference this cart — the headline feature
// ============================================================================
const SCAN = [
  { dir: path.join(ROOT, "docs"), exts: [".md"] },
  { dir: path.join(ROOT, "data-tools"), exts: [".md", ".js", ".sh"] },
];
// generated / derived files: a hit there is noise, not signal
const SKIP_RE = /(cart-compendium\.html|history\.html|FIELD-NOTES\.md|index\.json)$/;

function walk(dir, exts, acc) {
  let ents;
  try { ents = fs.readdirSync(dir, { withFileTypes: true }); } catch { return; }
  for (const e of ents) {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) {
      if (e.name === "node_modules" || e.name === "cache" || e.name === ".git") continue;
      walk(p, exts, acc);
    } else if (exts.includes(path.extname(e.name)) && !SKIP_RE.test(e.name)) {
      acc.push(p);
    }
  }
}

const files = [];
for (const s of SCAN) walk(s.dir, s.exts, files);

const re = wordRe(name);
const hits = [];
for (const f of files) {
  // don't report the cart's own source/config as "a doc that mentions it"
  if (f === cPath) continue;
  let lines;
  try { lines = fs.readFileSync(f, "utf8").split("\n"); } catch { continue; }
  const matched = [];
  for (let i = 0; i < lines.length; i++) if (re.test(lines[i])) matched.push([i + 1, lines[i].trim()]);
  if (matched.length) hits.push({ file: path.relative(ROOT, f), matched });
}
// most-relevant first: more mentions = more likely the cart's real home
hits.sort((a, b) => b.matched.length - a.matched.length);

out.push(head("DOCS & NOTES THAT MENTION IT"));
if (!hits.length) {
  out.push(dim("  none — this cart isn't referenced in any prose doc yet."));
} else {
  for (const h of hits) {
    out.push("  " + bold(h.file) + dim(`  (${h.matched.length})`));
    for (const [ln, text] of h.matched.slice(0, 3)) {
      out.push(dim(`    ${ln}: `) + clip(text, 96));
    }
    if (h.matched.length > 3) out.push(dim(`    … +${h.matched.length - 3} more`));
  }
}

// ============================================================================
// 6. source files + data-tools that feed it
// ============================================================================
out.push(head("SOURCE"));
out.push("  " + path.relative(ROOT, cPath));
const jsCfg = path.join(SRC_DIR, name + ".cart.js");
if (fs.existsSync(jsCfg)) out.push("  " + path.relative(ROOT, jsCfg) + dim("   (sprite/map config)"));
const png = path.join(CARTS_DIR, name + ".cart.png");
if (fs.existsSync(png)) out.push("  " + path.relative(ROOT, png) + dim("   (baked: thumbnail + embedded source)"));
// data-tools subfolders that name this cart (in their .js/.md) feed it
const dtHits = hits.filter(h => h.file.startsWith("data-tools/"))
  .map(h => h.file.split("/").slice(0, 2).join("/"));
const dtFeed = [...new Set(dtHits)];
if (dtFeed.length) out.push(dim("  fed by: ") + dtFeed.join(", "));

// ============================================================================
// next steps — the integrity/publish tools this one deliberately doesn't do
// ============================================================================
out.push(head("NEXT"));
out.push(dim("  this + the source map in 1: ") + `node tools/orient.js ${name}`);
out.push(dim("  a cross-cart FEATURE, not 1 cart:") + ` node tools/topic-brief.js "<theme>"`);
out.push(dim("  map the SOURCE (shapes/fns):") + ` node tools/cart-outline.js ${name}`);
out.push(dim("  one function's full body:  ") + ` node tools/cart-outline.js ${name} --fn <name>`);
out.push(dim("  integrity (source drift):  ") + `node tools/cart-info.js ${name}`);
out.push(dim("  publish / rebake state:    ") + `node tools/cart-status.js`);
out.push(dim("  which oracle to run:       ") + "docs/guides/checks-and-oracles.md");

console.log(out.join("\n"));

// ---- helpers ----
function wrap(s, indent = 2, width = 92) {
  const pad = " ".repeat(indent);
  const words = String(s).split(/\s+/);
  let line = pad, lines = [];
  for (const w of words) {
    if ((line + " " + w).length > width && line.trim()) { lines.push(line); line = pad + w; }
    else line += (line === pad ? "" : " ") + w;
  }
  if (line.trim()) lines.push(line);
  return lines.join("\n");
}
function clip(s, n) { return s.length > n ? s.slice(0, n - 1) + "…" : s; }
