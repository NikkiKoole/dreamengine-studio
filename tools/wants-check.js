#!/usr/bin/env node
// ============================================================================
// wants-check.js — what a "wants" doc is ACTUALLY still waiting on.
//
//   node tools/wants-check.js                 # every wants doc
//   node tools/wants-check.js afrobeat        # one (substring match on the slug)
//   node tools/wants-check.js --todo          # only the actionable rows (shipped-but-unwired)
//   node tools/wants-check.js --quiet         # CI: print only when something is actionable
//   node tools/wants-check.js --strict        # exit 1 if any want shipped and is unwired
//   node tools/wants-check.js --json
//   node tools/wants-check.js --selfcheck     # assert the CHECKER against known answers
//
// THE FRICTION THIS KILLS. A wants doc records what a cart couldn't have when it was
// written ("the effects bus doesn't exist yet"). Then the bus ships, and nothing tells
// the doc. On 2026-07-30 all four *-effects-wants.md docs were still describing
// engine gaps that had been closed for six weeks; afrobeat's STATUS line still read
// "blocked on the effects bus" when every effect it voted for had landed. An agent
// reading that doc believes it and hand-rolls a workaround for a shipped effect.
//
// WHY NOT lint-capability-claims.js. That tool reads PROSE, and prose has a recall
// ceiling: it missed afrobeat entirely because this repo denies a capability by
// SCHEDULING it ("still open: wah, tape, leslie"), and no regex set over English ever
// converges. This tool never reads prose. It reads the table column the genre already
// writes — `unblocked by` — which IS the dependency list, in structured form. Same
// question, but asked of data instead of sentences.
//
// THE THREE FACTS. A row is actionable only when all three hold, which is what makes
// the output a work-list rather than a nag:
//   1. the doc declares the want blocked on capability X   (the `unblocked by` column)
//   2. X ships                                             (a studio.h proof symbol)
//   3. the cart makes no call that would use it            (the roster's `wire` symbols)
// Miss #2 and the doc is telling the truth (⧗ STILL BLOCKED). Miss #3 and the work is
// already done (✓ WIRED) and the row disappears on its own — nothing to clear by hand.
//
// It also cross-checks the doc's own STATUS line: a doc parked in exploring/building
// whose every want now ships is stale by definition, no phrasing involved.
//
// Roster (capability → proof symbol → wire symbols): tools/capability-roster.js, shared
// with lint-capability-claims.js so the two can't drift apart.
// ============================================================================

const fs = require("fs");
const path = require("path");
const { CAPS, shipsIn, wireSymbols } = require("./capability-roster");
const { extractStatus, classifyStatus } = require("./doc-status");

const ROOT = path.resolve(__dirname, "..");
const args = process.argv.slice(2);
const has = (f) => args.includes(f);

// ── --selfcheck: assert the CHECKER against known answers ────────────────────
// Same reason as lint-capability-claims': this tool JUDGES, so it rots in both
// directions, and a broken parser prints a confident empty list. The fixture pins
// one doc with one want of each of the three outcomes.
if (has("--selfcheck")) {
  const cp = require("child_process");
  const fx = path.join(__dirname, "fixtures", "wants-check");
  let raw;
  try {
    raw = cp.execFileSync(process.execPath, [__filename, "--json"], {
      env: { ...process.env,
             DE_WANTS_STUDIO_H: path.join(fx, "studio.h.txt"),
             DE_WANTS_DOCS_DIR: path.join(fx, "docs"),
             DE_WANTS_CARTS_DIR: path.join(fx, "carts") },
      encoding: "utf8", maxBuffer: 1 << 26,
    });
  } catch (e) { raw = e.stdout; }
  const g = JSON.parse(raw);
  const doc = g.docs.find(d => d.slug === "demo") || { wants: [] };
  const stale = g.docs.find(d => d.slug === "stale") || {};
  const want = (cap) => doc.wants.find(w => w.cap === cap);
  const T = [];
  const t = (n, ok) => T.push({ n, ok });

  t("the unblocked-by COLUMN is found and parsed  [broken-parse guard]", doc.wants.length === 4);
  t("a want whose capability ships but the cart never calls → UNWIRED",
    want("leslie") && want("leslie").state === "unwired");
  t("a want the cart DOES call → WIRED, and drops off the work-list",
    want("chorus") && want("chorus").state === "wired");
  t("a want whose capability is NOT in studio.h → STILL BLOCKED (the doc is right)",
    want("shimmer") && want("shimmer").state === "blocked");
  t("an alternate wire symbol counts as wired  [compression = glue|multiband|sidechain]",
    want("compression") && want("compression").state === "wired");
  t("'shimmer reverb' is ONE want, not shimmer + reverb  [substring shadowing]",
    !want("reverb"));
  t("the cart is resolved from the doc's filename stem",
    doc.cart && doc.cart.endsWith("demo.c"));
  // The bug that shipped once: a cart's OWN de:meta todo naming wah()/leslie() made the
  // tool report them wired. A doc ABOUT the gap read as the gap being closed.
  t("a call named only in a COMMENT or de:meta does NOT count as wired",
    want("leslie") && want("leslie").via.length === 0);
  t("a doc with a genuinely-blocked want is NOT called status-stale",
    doc.statusStale === false);
  t("...but one whose every want ships IS  [the C cross-check]",
    stale.statusStale === true);
  t("--strict exits nonzero while any want is shipped-but-unwired", g.actionable > 0);

  const bad = T.filter(x => !x.ok);
  for (const x of T) console.log(`  ${x.ok ? "\x1b[32m✓\x1b[0m" : "\x1b[31m✗\x1b[0m"} ${x.n}`);
  console.log(bad.length
    ? `\x1b[31mwants-check --selfcheck FAILED\x1b[0m — ${bad.length} of ${T.length} expectations broken`
    : `wants-check --selfcheck: ${T.length}/${T.length} known answers correct`);
  process.exit(bad.length ? 1 : 0);
}

const STUDIO = process.env.DE_WANTS_STUDIO_H || path.join(ROOT, "runtime", "studio.h");
const DOCS   = process.env.DE_WANTS_DOCS_DIR || path.join(ROOT, "docs", "design");
const CARTS  = process.env.DE_WANTS_CARTS_DIR || path.join(ROOT, "tools", "carts");
const scope  = args.find(a => !a.startsWith("--")) || "";

const studioSrc = fs.readFileSync(STUDIO, "utf8");
const roster = CAPS.filter(c => shipsIn(studioSrc, c.proof));
const rosterCaps = new Set(roster.map(c => c.cap));

// Match a capability by the words PROSE uses. Unlike lint-capability-claims we do NOT
// demand a qualifier for the ambiguous words: this text is already known to be a
// dependency declaration (it is the `unblocked by` cell), so "tape" means the effect.
// That is the whole dividend of reading structure instead of sentences.
const esc = (w) => w.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
function capsIn(text) {
  const hits = [];
  for (const c of CAPS) {
    const re = new RegExp(`\\b(?:${c.words.map(esc).join("|")})\\b`, "i");
    const m = text.match(re);
    if (m) hits.push({ c, word: m[0].toLowerCase() });
  }
  // Drop a capability whose matched word is CONTAINED in another's: the cell
  // "formant filter / vocoder" names two capabilities, not three — "filter" is a
  // fragment of "formant filter", and reporting it is noise.
  return hits
    .filter(h => !hits.some(o => o !== h && o.word.includes(h.word) && o.word !== h.word))
    .map(h => h.c);
}

// Wiring is a question about CODE, so comments and the de:meta block must go first.
// Not optional: a cart's own de:meta todo saying `wah() + instrument_wah for the
// Gentleman chop` made this tool report afrobeat as having wired wah, leslie and tape,
// none of which it calls. A doc ABOUT the gap read as the gap being closed.
const stripComments = (s) => s.replace(/\/\*[\s\S]*?\*\//g, " ").replace(/\/\/[^\n]*/g, " ");

// ── B: the `unblocked by` column IS the dependency list ──────────────────────
// A markdown table row is `| a | b | c |`. Find the header cell that says
// "unblocked by", remember its index, and read that cell from every row after.
function parseWants(src) {
  const lines = src.split("\n");
  const wants = [];
  let col = -1, inTable = false;
  for (const line of lines) {
    if (!/^\s*\|/.test(line)) { inTable = false; col = -1; continue; }
    const cells = line.split("|").slice(1, -1).map(s => s.trim());
    if (col < 0) {
      const i = cells.findIndex(c => /unblocked\s+by/i.test(c));
      if (i >= 0) { col = i; inTable = true; }
      continue;
    }
    if (/^-+$/.test(cells[0]?.replace(/[:\s]/g, "") || "")) continue;  // the |---| rule
    if (!inTable || cells.length <= col) continue;
    const dep = cells[col], label = (cells[0] || "").replace(/\*\*/g, "").trim();
    for (const c of capsIn(dep)) wants.push({ cap: c.cap, label, dep });
  }
  // one row per capability, keeping the first want that asked for it
  const seen = new Set();
  return wants.filter(w => (seen.has(w.cap) ? false : seen.add(w.cap)));
}

// ── D: does the CART actually call it? ───────────────────────────────────────
function wiredIn(cartSrc, cap) {
  const c = CAPS.find(x => x.cap === cap);
  return wireSymbols(c).filter(sym => new RegExp(`\\b${esc(sym)}\\s*\\(`).test(cartSrc));
}

const docs = [];
for (const name of fs.readdirSync(DOCS).sort()) {
  const m = name.match(/^(.+?)-(?:effects-)?wants\.md$/);
  if (!m) continue;
  const slug = m[1];
  if (scope && !slug.includes(scope.toLowerCase()) && !name.includes(scope)) continue;
  const file = path.join(DOCS, name);
  const src = fs.readFileSync(file, "utf8");

  const cartPath = path.join(CARTS, `${slug}.c`);
  const cartSrc = fs.existsSync(cartPath) ? stripComments(fs.readFileSync(cartPath, "utf8")) : null;

  const wants = parseWants(src).map(w => {
    if (!rosterCaps.has(w.cap)) return { ...w, state: "blocked", via: [] };
    if (cartSrc === null) return { ...w, state: "unknown", via: [] };
    const via = wiredIn(cartSrc, w.cap);
    return { ...w, state: via.length ? "wired" : "unwired", via };
  });

  // ── C: the doc's own STATUS vs reality (free, once B gives the dependency list)
  const status = extractStatus(src.split("\n")) || "";
  const phase = classifyStatus(status);
  const allShip = wants.length > 0 && wants.every(w => w.state !== "blocked");
  const statusStale = allShip && ["exploring", "building"].includes(phase);

  docs.push({
    slug, file: path.relative(ROOT, file),
    cart: cartSrc === null ? null : path.relative(ROOT, cartPath),
    phase, statusStale, wants,
  });
}

const actionable = docs.reduce((a, d) => a + d.wants.filter(w => w.state === "unwired").length, 0);

if (has("--json")) {
  console.log(JSON.stringify({ docs, actionable }, null, 2));
  process.exit(0);
}

if (!docs.length) {
  console.log(`wants-check: no *-wants.md docs matched${scope ? ` "${scope}"` : ""}.`);
  process.exit(0);
}
if (has("--quiet") && !actionable) process.exit(0);

const ICON = { unwired: "\x1b[33m✗\x1b[0m", wired: "\x1b[32m✓\x1b[0m",
               blocked: "\x1b[90m⧗\x1b[0m", unknown: "\x1b[90m?\x1b[0m" };
const STATE = { unwired: "SHIPPED, UNWIRED", wired: "WIRED           ",
                blocked: "STILL BLOCKED   ", unknown: "NO CART         " };

console.log(`\x1b[1mWANTS DOCS\x1b[0m — what each one is really waiting on`);
for (const d of docs) {
  const only = has("--todo");
  const rows = only ? d.wants.filter(w => w.state === "unwired") : d.wants;
  if (only && !rows.length) continue;
  console.log(`\n  \x1b[1m${d.slug}\x1b[0m  ${d.file}  [${d.phase}]` +
              (d.cart ? `\n  cart: ${d.cart}` : `\n  cart: (none found)`));
  for (const w of rows) {
    const via = w.via.length ? `  via ${w.via.map(s => s + "()").join(" / ")}` : "";
    console.log(`    ${ICON[w.state]} ${STATE[w.state]}  ${w.cap.padEnd(12)}${via}` +
                (w.label ? `\n        want: ${w.label.slice(0, 88)}` : ""));
  }
  const un = d.wants.filter(w => w.state === "unwired").length;
  const bl = d.wants.filter(w => w.state === "blocked").length;
  if (!only) {
    console.log(`    → ${d.wants.length} want(s): ${un} shipped-but-unwired, ` +
                `${d.wants.length - un - bl} wired, ${bl} genuinely blocked.` +
                (bl === 0 ? "  Nothing here is blocked; this is a work-list." : ""));
  }
  if (d.statusStale) {
    console.log(`    \x1b[33m⚠ STATUS is "${d.phase}" but every want ships\x1b[0m — the status line is stale.`);
  }
}
console.log(`\n  ${docs.length} doc(s) · ${actionable} shipped-but-unwired want(s).` +
            `\n  A row clears itself when the cart wires it — nothing to tick off by hand.`);
process.exit(has("--strict") && actionable ? 1 : 0);
