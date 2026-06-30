#!/usr/bin/env node
// ============================================================================
// orient.js — go cold on a cart in ONE call: its EXTERNAL context (docs/notes
// that mention it) THEN its OWN source map. The two-in-one front door.
//
//   node tools/orient.js <name>          # e.g. sloop, or cart:sloop
//   node tools/orient.js sloop --full    # pass flags straight through to the outline
//
// It just runs build-context.js then cart-outline.js back to back — the pair
// you almost always want when picking up an unfamiliar cart, without the second
// round trip. Each is its own tool (run them solo when you only want one half);
// this is the convenience composite. Flags after the name go to BOTH children
// (cart-outline reads --full / --fn / --json; build-context ignores the rest).
//
// This is the VERTICAL front door (one cart, deep). For a HORIZONTAL theme that
// spans many carts + docs + code (a feature like the iOS virtual gamepad), reach
// for `tools/topic-brief.js "<theme>"` instead.
//
// Plain node, CommonJS.
// ============================================================================

const { spawnSync } = require("child_process");
const path = require("path");

const ROOT = path.resolve(__dirname, "..");
const args = process.argv.slice(2);
const raw = args.find(a => !a.startsWith("--"));
if (!raw) {
  console.error("usage: node tools/orient.js <cart-name> [--full] [--fn <name>]");
  process.exit(2);
}
const passthru = args.filter(a => a !== raw);

function run(tool, toolArgs) {
  const r = spawnSync("node", [path.join(ROOT, "tools", tool), ...toolArgs], { stdio: "inherit" });
  return r.status === null ? 1 : r.status;
}

// --fn is a targeted drill, not a cold orient — skip the external context dump
// and just hand the request to the outline (so `orient x --fn y` ≈ that slice).
if (passthru.includes("--fn")) process.exit(run("cart-outline.js", [raw, ...passthru]));

// EXTERNAL context first (the why / the neighbours), then the SOURCE map (the what).
const a = run("build-context.js", [raw]);
process.stdout.write("\n");
const b = run("cart-outline.js", [raw, ...passthru]);

// surface a real failure (bad cart name, etc.) without masking the other half's output
process.exit(a || b);
