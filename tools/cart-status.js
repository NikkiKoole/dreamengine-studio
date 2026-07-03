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
// Plus one ADVISORY note (does NOT gate --quiet/CI — engine edits are frequent, you don't
// republish on each one):
//   ENGINE-STALE      — the cart uses an INSTR_ engine AND runtime/sound.h (the synth engine,
//                       where tuning/voice changes live) got a commit NEWER than the cart's
//                       site/ build. The cart's .c is unchanged so checks 1–3 stay silent, but
//                       its PUBLISHED web AUDIO predates the engine change → it sounds outdated
//                       online. Coarse by design (flags every INSTR_ cart when sound.h moves,
//                       even if the specific voice it uses didn't change — over-reporting is
//                       safe here). Fix: republish it, or do a site-wide rebuild (build-site.js).
//
// Exit code: 0 if nothing pending (or no --quiet), 1 under --quiet when any cart is pending
// (checks 1–3 only; ENGINE-STALE is advisory and never sets the exit code).

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
      "git log --pretty=format:'C%ct' --name-only -- tools/carts editor/public/carts site runtime",
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

const needRebake = [], notPublished = [], stalePublished = [], noEmbed = [], orphanSite = [], engineStale = [];

const pngs = fs.readdirSync(CARTS_DIR).filter(f => f.endsWith(".cart.png")).sort();
const timeMap = gitTimeMap();

// the synth engine: newest commit touching any of these = the "engine version" a cart's
// published audio is measured against (sound.h is where the INSTR_ voices + tuning live).
const ENGINE_FILES = ["runtime/sound.h"];
const engineT = ENGINE_FILES.reduce((m, f) => Math.max(m, timeMap.get(f) || 0), 0);
// the MODELED voices (ids 16–29) carry the synth/tuning work in sound.h; raw oscillators
// (SQUARE/SAW/TRI/NOISE/SINE, ids 0–4) are sfx blips, so a cart using only those isn't an
// "instrument" for republish purposes.
const INSTRUMENT_RE = /INSTR_(PLUCK|MALLET|FM|ORGAN|EPIANO|PD|MEMBRANE|REED|VOICE|PIPE|GUITAR|PIANO|BOWED|BRASS)/;

for (const png of pngs) {
  const name = png.replace(".cart.png", "");
  const srcPath = path.join(SRC_DIR, name + ".c");
  const hasSrc = fs.existsSync(srcPath);
  const published = fs.existsSync(path.join(SITE_DIR, name));

  const srcText = hasSrc ? fs.readFileSync(srcPath, "utf8") : null;

  // 1. rebake check (only meaningful when there's a .c to compare against)
  if (hasSrc) {
    const emb = embeddedSource(path.join(CARTS_DIR, png));
    if (emb === null) noEmbed.push(name);
    else if (emb.trim() !== srcText.trim()) needRebake.push(name);
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

  // 4. engine-stale (advisory): an INSTRUMENT cart whose site build predates the latest sound.h
  // commit → its published audio is older than the engine. Skip if already stale-published
  // (a republish fixes both). Scoped to the MODELED engines (ids 16–29) — the raw oscillators
  // (SQUARE/SAW/TRI/SINE/NOISE) are games' sfx blips, not instruments, and don't carry the
  // voice/tuning work that lands in sound.h.
  else if (engineT > 0 && siteT > 0 && engineT > siteT && srcText && INSTRUMENT_RE.test(srcText))
    engineStale.push(name);
}

// site dirs with no matching .cart.png (a removed/renamed cart left behind)
if (fs.existsSync(SITE_DIR)) {
  for (const d of fs.readdirSync(SITE_DIR)) {
    if (fs.statSync(path.join(SITE_DIR, d)).isDirectory() &&
        !fs.existsSync(path.join(CARTS_DIR, d + ".cart.png"))) orphanSite.push(d);
  }
}

// COMPENDIUM-STALE — docs/cart-compendium.html (the ★ techniques page) drifted from the
// tag data (a TEACHES header or teaches.json changed without a regen). One command fixes it.
let compendiumStale = false;
try { execSync(`node ${path.join(__dirname, "build-compendium.js")} --check`, { stdio: "ignore" }); }
catch { compendiumStale = true; }

// DRIFTABLE DOCS (advisory) — docs that hand-declared a `de:driftable` snapshot of a tool's
// output, where the tool's inputs (usually the cart shelf) moved AFTER the snapshot date.
// Cart edits shift those numbers, so this is the place you learn a snapshot doc needs a re-read.
// See tools/stale-doc-check.js --driftable + docs/design/driftable-docs.md.
let driftedDocs = [];
try {
  const out = execSync(`node ${path.join(__dirname, "stale-doc-check.js")} --driftable --json`, { encoding: "utf8" });
  driftedDocs = (JSON.parse(out).driftable || []).filter(e => e.drifted).map(e => `${e.rel}  (snapshot ${e.asOf}, ${e.lag}d stale)`);
} catch { /* advisory — never break cart-status */ }

// HANDOFF LANES (advisory) — the back door of tools/handoff.js: active ▶ lanes in docs/HANDOFF.md
// that are >2wk old or whose doc links broke, i.e. a stale "resume here" that needs refresh/prune.
let staleLanes = [];
try {
  const out = execSync(`node ${path.join(__dirname, "handoff.js")} --json`, { encoding: "utf8" });
  const S = JSON.parse(out).staleDays;
  staleLanes = (JSON.parse(out).lanes || [])
    .filter(l => (l.age != null && l.age > S) || (l.broken && l.broken.length))
    .map(l => `${l.title}  (${l.date}${l.age != null ? `, ${l.age}d` : ""}${l.broken && l.broken.length ? `, broken: ${l.broken.join(", ")}` : ""})`);
} catch { /* advisory — never break cart-status */ }

if (JSON_OUT) {
  console.log(JSON.stringify({ needRebake, notPublished, stalePublished, engineStale, noEmbed, orphanSite, compendiumStale, driftedDocs, staleLanes }, null, 2));
  process.exit(0);
}

const pending = needRebake.length + notPublished.length + stalePublished.length + (compendiumStale ? 1 : 0);
const list = (arr) => arr.length ? arr.map(n => "  " + n).join("\n") : "  (none)";

if (!QUIET || needRebake.length)
  console.log(`\nNEEDS REBAKE — embedded source != tools/carts/<name>.c  (${needRebake.length})\n${list(needRebake)}`);
if (!QUIET || notPublished.length)
  console.log(`\nNOT PUBLISHED — no site/<name>/ build  (${notPublished.length})\n${list(notPublished)}`);
if (!QUIET || stalePublished.length)
  console.log(`\nSTALE PUBLISHED — source changed after the site build  (${stalePublished.length})\n${list(stalePublished)}`);
if (engineStale.length) {
  const shown = engineStale.slice(0, 12);
  const more = engineStale.length - shown.length;
  console.log(`\nENGINE-STALE (advisory) — instrument cart published before the latest runtime/sound.h commit  (${engineStale.length})\n${list(shown)}${more > 0 ? `\n  …and ${more} more (--json for the full list)` : ""}\n  → republish individually, or site-wide: node tools/build-site.js`);
}
if (!QUIET || compendiumStale)
  console.log(`\nCOMPENDIUM — docs/cart-compendium.html (★ techniques)  ${compendiumStale ? "STALE → node tools/build-compendium.js" : "up to date"}`);
if (driftedDocs.length)
  console.log(`\nDRIFTABLE DOCS (advisory) — snapshot docs whose source moved after their as-of date  (${driftedDocs.length})\n${list(driftedDocs)}\n  → re-run the doc's declared cmd + eyeball; details: node tools/stale-doc-check.js --driftable`);
if (staleLanes.length)
  console.log(`\nHANDOFF LANES (advisory) — active ▶ threads that are stale or have a broken link  (${staleLanes.length})\n${list(staleLanes)}\n  → refresh the date in docs/HANDOFF.md or prune it (shipped → STATUS.md); details: node tools/handoff.js --check`);
if (noEmbed.length)
  console.log(`\nNOTE — .cart.png with no de:source chunk  (${noEmbed.length})\n${list(noEmbed)}`);
if (orphanSite.length)
  console.log(`\nNOTE — site/<name>/ with no matching cart (renamed/removed?)  (${orphanSite.length})\n${list(orphanSite)}`);

if (!QUIET) {
  console.log(`\n${pngs.length} carts · ${needRebake.length} need rebake · ${notPublished.length} unpublished · ${stalePublished.length} stale-published · ${engineStale.length} engine-stale`);
  if (pending === 0) console.log("all carts baked and published — nothing pending.");
}
process.exit(QUIET && pending ? 1 : 0);
