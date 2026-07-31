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
//               (design-board --lint GRADUATED to a gate 2026-07-10 when its
//               42-doc backlog was cleared — the promised path for any advisory
//               check that reaches zero.)
//
// Token/size contract: the SUMMARY is one line per check (counts, never
// listings) — safe to embed in orient. The listings live behind --full, or in
// each tool run directly (every row IS its own tool; run that tool to fix).
// What we deliberately DON'T gate at all: docs/design/driftable-docs.md
// → "what we deliberately don't gate".
//
// Plain node, CommonJS, no deps. Children run in parallel; ~8s total (was ~3s before the
// `sw canvas` row, which COMPILES AND RENDERS drawall — the only check here that builds anything).
// Two consequences worth knowing: the extra ~5s is compile time and does not shrink with --frames,
// and in a multi-agent repo that row goes ✗ whenever the shared engine is momentarily mid-edit by
// someone else. That is a true signal (the engine does not build) but it reads as a canvas
// regression, so check `node tools/canvas-diff.js drawall --golden` directly before believing it.
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
  // GATED, unlike the ledger row below it: this asserts the CHECKER against a known-answer fixture,
  // so it is deterministic and must always pass. A red ledger row is a backlog; a red selftest row
  // means the tool's findings cannot be believed at all. See checks-and-oracles.md "self-test the
  // checker". More meta-linters should grow a --selfcheck and join this row.
  { name: "selftest: ledger",  tool: "status-check.js",    args: ["--selfcheck"], gate: true },
  { name: "selftest: xrefs",   tool: "lint-xrefs.js",      args: ["--selfcheck"], gate: true },
  { name: "selftest: doc refs",tool: "stale-doc-check.js", args: ["--selfcheck"], gate: true },
  { name: "selftest: links",   tool: "lint-docs.js",       args: ["--selfcheck"], gate: true },
  { name: "selftest: lanes",   tool: "handoff.js",         args: ["--selfcheck"], gate: true },
  { name: "selftest: cap claims", tool: "lint-capability-claims.js", args: ["--selfcheck"], gate: true },
  { name: "selftest: fxicons", tool: "lint-fxicons.js",    args: ["--selfcheck"], gate: true },
  { name: "selftest: aux params", tool: "lint-aux-params.js", args: ["--selfcheck"], gate: true },
  { name: "selftest: carts",   tool: "lint-carts.js",      args: ["--selfcheck"], gate: true },
  { name: "selftest: fx frame", tool: "lint-fx-frame.js",  args: ["--selfcheck"], gate: true },
  { name: "selftest: ui audit", tool: "ui-audit.js",       args: ["--selfcheck"], gate: true },
  { name: "selftest: dupes",   tool: "cart-dupes.js",     args: ["--selfcheck"], gate: true },
  // Joined 2026-07-31 (field note 028): reddit-gaps had printed a confident green verdict table for
  // ten straight rotations while three of its signals were degraded — two of them in the false-
  // NEGATIVE direction, which is self-sealing (no finding means no reason to look). Its --check flag
  // is the older spelling of --selfcheck; both are accepted here.
  { name: "selftest: demand",  tool: "reddit-gaps.js",     args: ["--check"], gate: true },
  // GATED from birth (2026-07-30): mechanical (an FX_* kind either has a glyph or it doesn't),
  // and it was written already AT zero after fixing the two kinds that had shipped without one —
  // so there is no backlog to work down first, unlike the ledger row above.
  { name: "fx glyphs",    tool: "lint-fxicons.js",       args: ["--strict"],    gate: true },
  // GATED from birth for the same reason as fx glyphs above: it was already AT zero (0 findings
  // across 573 carts), so there is no backlog to work down. It had been in NO gate at all, which
  // is why nobody would have noticed if its parser had gone blind — the fixture row above is the
  // other half of that fix. `--strict`, not `--quiet`: quiet prints nothing when clean, which
  // makes the health-strip row read "(no output)".
  { name: "fx per-frame", tool: "lint-fx-frame.js",      args: ["--strict"],    gate: true },
  { name: "status ledger", tool: "status-check.js",       args: ["--check"],     gate: false }, // ADVISORY on purpose: 81 findings on the day it was written (2026-07-30 audit). Gate it once the backlog is worked down, like doc-statuses and xrefs graduated.
  { name: "doc statuses", tool: "design-board.js",       args: ["--lint"],      gate: true }, // GRADUATED 2026-07-10: backlog reached 0 (was 42) — see driftable-docs.md "deliberately don't gate"
  { name: "xrefs",        tool: "lint-xrefs.js",         args: ["--strict"],    gate: true }, // GRADUATED 2026-07-10: both tiers reached 0 (were 58/203) — exempt classes documented in its header
  { name: "icon mask",    tool: "icon-mask.js",          args: ["--check"],     gate: true }, // committed app-icon mask vs Apple's ictool; skips (exit 0) without Xcode
  // The SOFTWARE-CANVAS regression gate. Added 2026-07-30 after it was found RED FOR 20 DAYS: the
  // golden was blessed at 23:30:33 and `drawall: exercise engine blend()` landed at 23:33:28 without
  // re-blessing, and because `--golden` was a manual command nobody ran it while every other gate
  // stayed green. That is exactly the failure this tool exists to stop. Unlike the parity check
  // (GPU vs SW, which needs a 64px budget because GPUs break texel ties differently), this one
  // compares the SW render against a committed PNG and is deterministic everywhere, so it is
  // pixel-exact and a real gate. Costs ~5s and runs in parallel with the rest, so the strip goes
  // ~3.5s -> ~5.2s rather than 8.7s.
  { name: "sw canvas",    tool: "canvas-diff.js",        args: ["drawall", "--golden"], gate: true },
  // --- ADVISORY: hygiene / backlog / nudges ---
  { name: "handoff",      tool: "handoff.js",            args: ["--check"] },
  { name: "driftable",    tool: "stale-doc-check.js",    args: ["--driftable"], warn: num(/(\d+) likely drifted/) },
  // ADVISORY on purpose despite currently sitting at 0: it JUDGES prose with regexes, so a
  // legitimately-historical claim can fire in a way no discriminator sees. The fix is one
  // acknowledgement line, never a code change — so this must not be able to block anyone.
  // Graduate it to a gate (--strict) if it holds at 0 through a few real staleness cycles.
  { name: "cap claims",   tool: "lint-capability-claims.js", args: [], warn: num(/SHIPPED CAPABILITY.*?\((\d+)\)/) },
  { name: "doc freshness",tool: "stale-doc-check.js",    args: [],              warn: num(/(\d+) broken/) },
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
