#!/usr/bin/env node
// ============================================================================
// orient.js — go cold on a cart in ONE call: its EXTERNAL context (docs/notes
// that mention it) THEN its OWN source map. The two-in-one front door.
//
//   node tools/orient.js <name>          # e.g. sloop, or cart:sloop
//   node tools/orient.js sloop --full    # pass flags straight through to the outline
//   node tools/orient.js                 # BARE = the SESSION front door (repo-level):
//                                          active handoff lanes + the repo-doctor health
//                                          strip + a one-line cart digest. Counts only,
//                                          never listings — deliberately token-small; the
//                                          detail lives in the named tool each row points at.
//                                          Run this when starting cold / resuming a session.
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
const fs = require("fs");

const ROOT = path.resolve(__dirname, "..");

// APP-name → cart-slug alias. An app's name/dir often differs from the cart slug it ships
// (e.g. the "Tiny Acid Jam" app in apps/tinyacidjam/ ships cart slug `acidcandy` — the rename
// deliberately kept the slug for provenance). So `orient tinyacidjam` should point at the cart,
// not dead-end. Returns {app, carts[]} or null.
function resolveApp(name) {
  const key = name.toLowerCase().replace(/[^a-z0-9]/g, "");
  const appsDir = path.join(ROOT, "apps");
  if (!fs.existsSync(appsDir)) return null;
  for (const d of fs.readdirSync(appsDir)) {
    const mf = path.join(appsDir, d, "app.json");
    if (!fs.existsSync(mf)) continue;
    let j; try { j = JSON.parse(fs.readFileSync(mf, "utf8")); } catch { continue; }
    const names = [d, j.name || ""].map(s => s.toLowerCase().replace(/[^a-z0-9]/g, ""));
    if (names.includes(key) && Array.isArray(j.carts) && j.carts.length)
      return { app: j.name || d, carts: j.carts };
  }
  return null;
}
const args = process.argv.slice(2);
const raw = args.find(a => !a.startsWith("--"));

function run(tool, toolArgs) {
  const r = spawnSync("node", [path.join(ROOT, "tools", tool), ...toolArgs], { stdio: "inherit" });
  return r.status === null ? 1 : r.status;
}

// BARE = repo-level session briefing: lanes + health strip + a one-line cart digest.
// Each piece is counts-only so the whole briefing stays a few hundred tokens.
if (!raw) {
  run("handoff.js", []);
  process.stdout.write("\n");
  run("repo-doctor.js", []);
  // cart-status is ~480 lines of listings — distill it to its section headers.
  const r = spawnSync("node", [path.join(ROOT, "tools", "cart-status.js")], { encoding: "utf8" });
  const heads = (r.stdout || "").split("\n").filter(l => /\(\d+\)\s*$/.test(l))
    .map(l => {
      const n = l.match(/\((\d+)\)\s*$/)[1];
      const label = l.split("—")[0].replace(/\(.*$/, "").trim().toLowerCase().replace(/\s+/g, "-");
      return `${n} ${label}`;
    });
  if (heads.length) console.log(`\ncarts: ${heads.join(" · ")}   [cart-status.js for the lists]`);
  console.log("next: orient <cart> · topic-brief \"<theme>\" · design-board ready");
  process.exit(0);
}
const passthru = args.filter(a => a !== raw);

// Resolve an APP name to its cart slug up front (see resolveApp) so every path below orients on
// the real cart. If <raw> isn't a cart source but IS an app, remap + say so.
let name = raw;
const bare = raw.replace(/^cart:/, "");
if (!fs.existsSync(path.join(ROOT, "tools", "carts", `${bare}.c`))) {
  const app = resolveApp(bare);
  if (app) {
    name = app.carts[0];
    console.log(`'${raw}' is the APP "${app.app}" — cart slug${app.carts.length > 1 ? "s" : ""}: ${app.carts.join(", ")}. Orienting on ${name}.\n`);
  }
}

// --fn is a targeted drill, not a cold orient — skip the external context dump
// and just hand the request to the outline (so `orient x --fn y` ≈ that slice).
if (passthru.includes("--fn")) process.exit(run("cart-outline.js", [name, ...passthru]));

// FRONT DOOR: prime the active handoff lanes first — going cold on a cart is exactly when you
// want to know what complex work is in flight (docs/design/driftable-docs.md's two-door pattern,
// pointed at HANDOFF.md). Advisory, never blocks the orient.
run("handoff.js", []);
process.stdout.write("\n");
// EXTERNAL context first (the why / the neighbours), then the SOURCE map (the what).
const a = run("build-context.js", [name]);
process.stdout.write("\n");
const b = run("cart-outline.js", [name, ...passthru]);

// surface a real failure (bad cart name, etc.) without masking the other half's output
process.exit(a || b);
