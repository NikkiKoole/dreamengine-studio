#!/usr/bin/env node
// ============================================================================
// repo-doctor.js — the ONE aggregated health strip over every meta-check.
//
//   node tools/repo-doctor.js          # summary: one ✓/⚠/✗ line per check + a verdict
//   node tools/repo-doctor.js --full   # + dump the full output of every non-clean check
//   node tools/repo-doctor.js --quiet  # CI/hook mode: print only failing GATES, exit 1 if any
//
// THE FRICTION THIS KILLS. The repo has a dozen self-audit tools (lint-docs,
// lint-xrefs, design-board --lint, stale-doc-check, handoff --check, the
// build-* --check staleness gates…) and each requires REMEMBERING a command —
// so in practice they run never, and drift accumulates invisibly (a 2026-07-10
// audit found reflections.html stale, 42 unmarked design docs, and 3 drifted
// driftable snapshots, all flagged by tools nobody had run). This tool runs
// them all, in parallel, and prints one glanceable strip. It is the "check at
// a moment you already visit" answer: bare `orient` embeds the summary, so
// every cold session start sees the strip without asking for it.
//
// TWO TIERS (the balancing act, deliberately):
//   GATES     — a ✗ means a generated artifact or reference is actually broken/
//               stale (lint-docs, lint-carts, the build-* --check family).
//               These fail --quiet.
//   ADVISORY  — a ⚠ is hygiene/backlog, never blocks (design-board --lint's
//               unmarked-docs backlog, stale-doc-check's nudge tiers, missing
//               backlinks, stale handoff lanes, drifted driftable snapshots).
//               design-board --lint graduates to a GATE if its backlog ever
//               reaches zero — until then a hard gate would just train everyone
//               to ignore red.
//
// Token/size contract: the SUMMARY is one line per check (counts, never
// listings) — safe to embed in orient. The listings live behind --full, or in
// each tool run directly (every row IS its own tool; run that tool to fix).
// What we deliberately DON'T gate at all: docs/design/driftable-docs.md
// → "what we deliberately don't gate".
//
// Plain node, CommonJS, no deps. Children run in parallel; ~3s total.
// ============================================================================

const { exec } = require("child_process");
const path = require("path");

const ROOT = path.resolve(__dirname, "..");
const FULL = process.argv.includes("--full");
const QUIET = process.argv.includes("--quiet");

// count-extracting helpers for advisory ⚠ decisions (exit code alone isn't enough:
// stale-doc-check/lint-xrefs always exit 0 but may report real findings)
const num = (re) => (out) => { const m = out.match(re); return m ? Number(m[1]) : 0; };

const CHECKS = [
  // --- GATES: broken artifact / broken reference ---
  { name: "lint-docs",    tool: "lint-docs.js",          args: [],              gate: true },
  { name: "lint-carts",   tool: "lint-carts.js",         args: [],              gate: true },
  { name: "compendium",   tool: "build-compendium.js",   args: ["--check"],     gate: true },
  { name: "design board", tool: "build-design-board.js", args: ["--check"],     gate: true },
  { name: "band briefs",  tool: "build-band-briefs.js",  args: ["--check"],     gate: true },
  { name: "field notes",  tool: "build-field-notes.js",  args: ["--check"],     gate: true },
  { name: "reflections",  tool: "build-reflections.js",  args: ["--check"],     gate: true },
  { name: "cart index",   tool: "build-cart-index.js",   args: ["--check"],     gate: true },
  { name: "history",      tool: "build-history.js",      args: ["--check"],     gate: true },
  // --- ADVISORY: hygiene / backlog / nudges ---
  { name: "doc statuses", tool: "design-board.js",       args: ["--lint"] },
  { name: "handoff",      tool: "handoff.js",            args: ["--check"] },
  { name: "driftable",    tool: "stale-doc-check.js",    args: ["--driftable"], warn: num(/(\d+) likely drifted/) },
  { name: "doc freshness",tool: "stale-doc-check.js",    args: [],              warn: num(/(\d+) broken/) },
  { name: "xrefs",        tool: "lint-xrefs.js",         args: [],              warn: num(/(\d+)\/\d+ missing backlinks/) },
];

// For a clean check the tool's own final summary line is the row. For a non-clean
// one prefer its count-headers ("ERRORS (1)", "NO STATUS LINE (42)") or STALE line —
// the last line of a failing linter is usually just its stats footer.
function pickSummary(out, status) {
  const lines = out.split("\n").map((l) => l.trim()).filter(Boolean);
  if (status !== "ok") {
    const hits = lines
      .filter((l) => /STALE|^[A-Z][A-Z0-9 -]+\(\d/.test(l))
      .map((l) => (l.match(/^([A-Z][A-Z0-9 -]+\([^)]*\))/) || [null, l])[1])
      .slice(0, 3);
    if (hits.length) return hits.join(" · ").slice(0, 110);
  }
  return (lines[lines.length - 1] || "(no output)").slice(0, 110);
}

function run(check) {
  return new Promise((resolve) => {
    exec(
      `node ${JSON.stringify(path.join(ROOT, "tools", check.tool))} ${check.args.join(" ")}`,
      { cwd: ROOT, timeout: 120000, maxBuffer: 16 * 1024 * 1024 },
      (err, stdout, stderr) => {
        const out = `${stdout || ""}${stderr || ""}`;
        const code = err ? (typeof err.code === "number" ? err.code : 1) : 0;
        const status =
          code !== 0 ? (check.gate ? "fail" : "warn")
          : check.warn && check.warn(out) > 0 ? "warn"
          : "ok";
        resolve({ ...check, code, out, summary: pickSummary(out, status), status });
      }
    );
  });
}

(async () => {
  const results = await Promise.all(CHECKS.map(run));
  const fails = results.filter((r) => r.status === "fail");
  const warns = results.filter((r) => r.status === "warn");

  if (QUIET) {
    for (const r of fails) console.error(`✗ ${r.name} — ${r.summary}`);
    process.exit(fails.length ? 1 : 0);
  }

  const ICON = { ok: "✓", warn: "⚠", fail: "✗" };
  console.log("REPO DOCTOR (each row is its own tool — run it directly to fix; --full for detail)");
  for (const r of results) {
    const cmd = `${r.tool}${r.args.length ? " " + r.args.join(" ") : ""}`;
    console.log(`  ${ICON[r.status]} ${r.name.padEnd(13)} ${r.summary}${r.status === "ok" ? "" : `   [${cmd}]`}`);
  }
  console.log(
    `${results.length} checks · ${results.length - fails.length - warns.length} clean · ` +
    `${warns.length} advisory · ${fails.length} failing` +
    (fails.length ? "  ← fix the ✗ rows (generated artifact or reference is actually stale/broken)" : "")
  );

  if (FULL) {
    for (const r of results.filter((x) => x.status !== "ok")) {
      console.log(`\n───── ${r.name} (${r.tool} ${r.args.join(" ")}) — exit ${r.code} ─────`);
      console.log(r.out.trimEnd());
    }
  }
  process.exit(0);
})();
