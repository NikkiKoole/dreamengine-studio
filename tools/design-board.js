#!/usr/bin/env node
// ============================================================================
// design-board.js — ONE overview of every feature/design in the repo and what
// phase it's in. The whole-repo board that build-field-notes.js is for the
// research journal: scan docs/design + docs/decisions (+ top-level docs with a
// status), read each doc's self-declared STATUS line, normalize it to a
// lifecycle phase (doc-status.js owns that vocabulary), and group.
//
//   node tools/design-board.js            # the board (ready → building → … → shipped)
//   node tools/design-board.js ready      # just one phase
//   node tools/design-board.js --full     # don't clip the status text
//   node tools/design-board.js --lint     # docs with NO status / off-vocabulary
//                                            status (exit 1 if any) — the gate
//
// THE FRICTION THIS KILLS. A fully-designed, ready-to-build feature can sit
// unstarted in docs/design/ and nobody knows — touch-controls.md's complete
// 7-step plan was found by accident; road-program-state.md sits at 0/14 with no
// status line at all. STATUS.md and action-plan.md are hand-maintained and
// drift. This derives the board straight from the docs, so the answer to "what's
// designed and waiting?" is one command, always current. The headline bucket is
// READY TO BUILD — specced work with nobody on it.
//
// It does NOT track per-cart polish (that's cart-todos.js over de:meta.todo[])
// or field-note lifecycle (build-field-notes.js). This is design docs + ADRs.
// ============================================================================

const fs = require("fs");
const path = require("path");
const { extractStatus, planProgress, statusDate, PHASES, classifyStatus } = require("./doc-status.js");

const ROOT = path.resolve(__dirname, "..");
const DOCS = path.join(ROOT, "docs");

// ---- args ----
const argv = process.argv.slice(2);
const full = argv.includes("--full");
const lint = argv.includes("--lint") || argv.includes("--check");
const phaseFilter = argv.find(a => !a.startsWith("--"));   // optional: ready/building/…

// ---- styling ----
const tty = process.stdout.isTTY;
const bold = s => (tty ? `\x1b[1m${s}\x1b[0m` : s);
const dim = s => (tty ? `\x1b[2m${s}\x1b[0m` : s);
const head = s => "\n" + bold(s) + "\n" + dim("─".repeat(s.length));
const clip = (s, n) => (s && s.length > n ? s.slice(0, n - 1) + "…" : s || "");

// ---- collect docs: design/ + decisions/ (always), top-level docs/ (if marked) ----
const targets = [];
function add(dir) {
  let ents; try { ents = fs.readdirSync(dir, { withFileTypes: true }); } catch { return; }
  for (const e of ents) if (e.isFile() && e.name.endsWith(".md")) targets.push(path.join(dir, e.name));
}
add(path.join(DOCS, "design"));
add(path.join(DOCS, "decisions"));
// top-level docs/ that carry a status (instrument-bank-plan, tuning-handoff, …);
// skip the hubs (no lifecycle) so they don't clutter.
const HUBS = new Set(["STATUS", "README", "VISION", "glossary", "history", "cart-compendium"]);
for (const e of fs.readdirSync(DOCS, { withFileTypes: true })) {
  if (!e.isFile() || !e.name.endsWith(".md")) continue;
  if (HUBS.has(path.basename(e.name, ".md"))) continue;
  const f = path.join(DOCS, e.name);
  if (extractStatus(fs.readFileSync(f, "utf8").split("\n"))) targets.push(f);
}

// ---- read each ----
const isADR = f => f.includes(`${path.sep}decisions${path.sep}`);
const entries = targets.map(f => {
  const text = fs.readFileSync(f, "utf8");
  const lines = text.split("\n");
  const status = extractStatus(lines);
  const titleM = text.match(/^#\s+(.+)$/m);
  return {
    rel: path.relative(ROOT, f),
    title: titleM ? titleM[1].replace(/`/g, "").trim() : path.basename(f, ".md"),
    status,
    phase: classifyStatus(status),
    plan: planProgress(text),
    date: statusDate(status),
    adr: isADR(f),
  };
});

// ---- lint mode: unmarked + off-vocabulary ----
if (lint) {
  const unmarked = entries.filter(e => !e.adr && e.phase === null);
  const offvocab = entries.filter(e => e.phase === "other");
  if (unmarked.length) {
    console.log(bold(`NO STATUS LINE (${unmarked.length}) — add one (idea/exploring/designed/building/shipped/cut):`));
    for (const e of unmarked.sort((a, b) => a.rel.localeCompare(b.rel))) console.log("  " + e.rel + dim("  " + e.title));
    console.log("");
  }
  if (offvocab.length) {
    console.log(bold(`OFF-VOCABULARY STATUS (${offvocab.length}) — declares a status that matches no phase:`));
    for (const e of offvocab) console.log("  " + e.rel + dim("  → " + clip(e.status, 60)));
    console.log("");
  }
  if (!unmarked.length && !offvocab.length) console.log("every design doc declares a recognized status. ✓");
  console.log(dim(`${entries.length} docs scanned`));
  process.exit(unmarked.length ? 1 : 0);
}

// ---- board mode ----
// design-doc lifecycle, the order we want to READ it (actionable first)
const ORDER = ["ready", "building", "exploring", "other", "shipped", "cut"];
const LABEL = Object.fromEntries(PHASES.map(p => [p.key, p.label]));
LABEL.other = "HAS STATUS, UNCLASSIFIED";

const board = entries.filter(e => !e.adr);
const adrs = entries.filter(e => e.adr);

function line(e) {
  const date = e.date ? dim(` ${e.date}`) : "";
  const plan = e.plan ? dim(` [${e.plan.done}/${e.plan.total}]`) : "";
  const tail = full && e.status ? "\n      " + dim(clip(e.status, 200)) : "";
  return "  " + bold(clip(e.title, 52)) + plan + date + dim("  " + e.rel.replace(/^docs\//, "")) + tail;
}

// summary headline
const SHORT = { ready: "ready", building: "building", exploring: "exploring", other: "unclassified", shipped: "shipped", cut: "cut" };
const counts = ORDER.map(k => {
  const n = board.filter(e => e.phase === k).length;
  return n ? `${n} ${SHORT[k]}` : null;
}).filter(Boolean);
const unmarked = board.filter(e => e.phase === null).length;

console.log(bold("DESIGN BOARD") + dim(`   ${board.length} design docs · ${adrs.length} ADRs · ${unmarked} unmarked`));
console.log(dim("  " + counts.join(" · ")));

for (const k of ORDER) {
  if (phaseFilter && k !== phaseFilter) continue;
  const group = board.filter(e => e.phase === k);
  if (!group.length) continue;
  // newest-first within a phase (what moved recently); undated sink to the end
  group.sort((a, b) => (b.date || "").localeCompare(a.date || "") || a.title.localeCompare(b.title));
  console.log(head(`${LABEL[k]}  (${group.length})`));
  for (const e of group) console.log(line(e));
}

// ADRs: compact footer (settled decisions, not work-in-flight)
if (!phaseFilter) {
  const accepted = adrs.filter(e => e.phase === "accepted" || /accept/i.test(e.status || "")).length;
  const other = adrs.length - accepted;
  console.log(head(`DECISIONS (ADRs) — ${adrs.length}`));
  console.log(dim(`  ${accepted} accepted · ${other} proposed/other`) + dim("   (docs/decisions/ — see decisions/README.md)"));
}

if (unmarked && !phaseFilter)
  console.log(dim(`\n${unmarked} design docs declare no status — run \`node tools/design-board.js --lint\` to list them.`));
