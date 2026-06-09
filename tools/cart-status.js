#!/usr/bin/env node
// Cart bake/publish status — what's out of date between a cart's source, its baked
// thumbnail, and its published web build. The companion to lint-carts.js: that one
// validates the registry (tags, files exist); THIS one answers "what do I need to
// rebuild / rebake / publish?" after a round of cart edits.
//
//   node tools/cart-status.js          # the report (groups below)
//   node tools/cart-status.js --quiet  # only print problems; silent + exit 0 if all clean
//   node tools/cart-status.js --json    # machine-readable
//
// Three checks (a cart = an <name>.cart.png in editor/public/carts/):
//   1. NEEDS REBAKE   — the thumbnail's embedded de:source chunk != the current
//                       tools/carts/<name>.c. The editor loads the EMBEDDED source, so a
//                       stale embed means the cart ignores code changes. Fix:
//                         node tools/make-cart.js tools/carts/<name>.c editor/public/carts/<name>.cart.png
//                         node tools/make-cart.js --run editor/public/carts/<name>.cart.png
//   2. NOT PUBLISHED  — no site/<name>/ web build. Fix: node tools/publish-cart.sh <name>
//   3. STALE PUBLISHED— site/<name>/ exists but a commit touched the cart's source/thumbnail
//                       AFTER the last commit that touched its site/ build (git-time compare).
//                       Fix: re-run publish-cart.sh <name>.
//
// Exit code: 0 if nothing pending (or no --quiet), 1 under --quiet when any cart is pending.

const fs = require("fs");
const path = require("path");
const zlib = require("zlib");
const { execSync } = require("child_process");

const ROOT = path.resolve(__dirname, "..");
const CARTS_DIR = path.join(ROOT, "editor/public/carts");
const SRC_DIR = path.join(ROOT, "tools/carts");
const SITE_DIR = path.join(ROOT, "site");

const args = process.argv.slice(2);
const QUIET = args.includes("--quiet");
const JSON_OUT = args.includes("--json");

// --- read the de:source zTXt chunk embedded in a .cart.png ---
function embeddedSource(pngPath) {
  const b = fs.readFileSync(pngPath);
  let o = 8; // skip PNG signature
  while (o + 12 <= b.length) {
    const len = b.readUInt32BE(o);
    const type = b.toString("ascii", o + 4, o + 8);
    if (type === "zTXt") {
      const data = b.slice(o + 8, o + 8 + len);
      const z = data.indexOf(0);
      if (z >= 0 && data.toString("ascii", 0, z) === "de:source") {
        try { return zlib.inflateSync(data.slice(z + 2)).toString("utf8"); }
        catch (e) { return null; }
      }
    }
    o += 12 + len; // length(4) + type(4) + data + crc(4)
  }
  return null;
}

// --- build a path -> latest-commit-unixtime map in ONE git call (for stale-published) ---
function gitTimeMap() {
  const map = new Map();
  let out;
  try {
    out = execSync(
      "git log --pretty=format:'C%ct' --name-only -- tools/carts editor/public/carts site",
      { cwd: ROOT, encoding: "utf8", maxBuffer: 64 * 1024 * 1024 }
    );
  } catch (e) { return map; } // not a git repo / no history → skip stale check
  let t = 0;
  for (const line of out.split("\n")) {
    if (line.startsWith("C")) { t = parseInt(line.slice(1), 10) || 0; continue; }
    if (!line.trim()) continue;
    if (!map.has(line)) map.set(line, t); // log is newest-first → first seen = latest
  }
  return map;
}

// site/<name>/ is a directory of files; its "build time" = newest commit touching any of them
function siteTime(timeMap, name) {
  let t = 0;
  const prefix = `site/${name}/`;
  for (const [p, ts] of timeMap) if (p.startsWith(prefix) && ts > t) t = ts;
  return t;
}

const needRebake = [], notPublished = [], stalePublished = [], noEmbed = [], orphanSite = [];

const pngs = fs.readdirSync(CARTS_DIR).filter(f => f.endsWith(".cart.png")).sort();
const timeMap = gitTimeMap();

for (const png of pngs) {
  const name = png.replace(".cart.png", "");
  const srcPath = path.join(SRC_DIR, name + ".c");
  const hasSrc = fs.existsSync(srcPath);
  const published = fs.existsSync(path.join(SITE_DIR, name));

  // 1. rebake check (only meaningful when there's a .c to compare against)
  if (hasSrc) {
    const emb = embeddedSource(path.join(CARTS_DIR, png));
    if (emb === null) noEmbed.push(name);
    else if (emb.trim() !== fs.readFileSync(srcPath, "utf8").trim()) needRebake.push(name);
  }

  // 2. publish check
  if (!published) { notPublished.push(name); continue; }

  // 3. stale-published check (git-time): source/thumbnail newer than the site build
  const srcT = Math.max(
    timeMap.get(`tools/carts/${name}.c`) || 0,
    timeMap.get(`tools/carts/${name}.cart.js`) || 0,
    timeMap.get(`editor/public/carts/${name}.cart.png`) || 0
  );
  const siteT = siteTime(timeMap, name);
  if (srcT > 0 && siteT > 0 && srcT > siteT) stalePublished.push(name);
}

// site dirs with no matching .cart.png (a removed/renamed cart left behind)
if (fs.existsSync(SITE_DIR)) {
  for (const d of fs.readdirSync(SITE_DIR)) {
    if (fs.statSync(path.join(SITE_DIR, d)).isDirectory() &&
        !fs.existsSync(path.join(CARTS_DIR, d + ".cart.png"))) orphanSite.push(d);
  }
}

if (JSON_OUT) {
  console.log(JSON.stringify({ needRebake, notPublished, stalePublished, noEmbed, orphanSite }, null, 2));
  process.exit(0);
}

const pending = needRebake.length + notPublished.length + stalePublished.length;
const list = (arr) => arr.length ? arr.map(n => "  " + n).join("\n") : "  (none)";

if (!QUIET || needRebake.length)
  console.log(`\nNEEDS REBAKE — embedded source != tools/carts/<name>.c  (${needRebake.length})\n${list(needRebake)}`);
if (!QUIET || notPublished.length)
  console.log(`\nNOT PUBLISHED — no site/<name>/ build  (${notPublished.length})\n${list(notPublished)}`);
if (!QUIET || stalePublished.length)
  console.log(`\nSTALE PUBLISHED — source changed after the site build  (${stalePublished.length})\n${list(stalePublished)}`);
if (noEmbed.length)
  console.log(`\nNOTE — .cart.png with no de:source chunk  (${noEmbed.length})\n${list(noEmbed)}`);
if (orphanSite.length)
  console.log(`\nNOTE — site/<name>/ with no matching cart (renamed/removed?)  (${orphanSite.length})\n${list(orphanSite)}`);

if (!QUIET) {
  console.log(`\n${pngs.length} carts · ${needRebake.length} need rebake · ${notPublished.length} unpublished · ${stalePublished.length} stale-published`);
  if (pending === 0) console.log("all carts baked and published — nothing pending.");
}
process.exit(QUIET && pending ? 1 : 0);
