#!/usr/bin/env node
// ============================================================================
// cart-info.js — orient on ONE cart in a single command.
//
//   node tools/cart-info.js <name|path> [--source] [--quiet]
//   node tools/cart-info.js boom
//
// Prints everything you need to safely understand + modify an existing cart,
// pulling together the three places its knowledge is scattered:
//
//   1. SOURCE      tools/carts/<name>.c  (and a .cart.js config, if any)
//   2. EMBED       the .cart.png's zTXt chunks — the bytes the EDITOR actually
//                  loads: de:source (the C the editor opens), de:settings
//                  (screen/scale/cell/map dims), de:sprites, de:map.
//   3. REGISTRY    editor/public/carts/index.json — title, kind, teaches, lineage.
//
// Headline checks:
//   • SCREEN/GRID  — screenW×H, scale, and the derived GW×GH at the cart's CELL
//                    (#define CELL in the source). The grid dims you need before
//                    laying out a default scene.
//   • SOURCE DRIFT — does de:source match tools/carts/<name>.c? If not, the
//                    editor is loading STALE code: you edited the .c but skipped
//                    `node tools/make-cart.js <src> <png>` to re-embed it. This
//                    is the "the cart ignores my changes" footgun, caught.
//
// Flags:
//   --source   also dump the embedded de:source to stdout (to eyeball what the
//              editor loads vs the .c on disk).
//   --quiet    print nothing; exit 1 iff de:source has drifted (CI / pre-commit).
//   --dims     print shell-sourceable DE_* dims from de:settings (screen/cell/map) and
//              exit; exit 3 if the cart has no settings chunk. The iOS build (ios/build.sh,
//              ios/device.sh) sources this so a single cart builds at its own size without
//              hand-passing DE_SCREEN_W etc.
//
// ── the .cart.png embed format (the "extraction trick", documented) ──────────
// A .cart.png is a normal PNG (the visible image is the thumbnail) carrying
// extra zTXt chunks keyed "de:<name>". Each zTXt is: keyword + NUL + 1-byte
// compression-method + zlib-DEFLATEd payload. So to read one: find the NUL,
// take everything from NUL+2, zlib.inflateSync it. (Same reader as
// cart-status.js / make-cart.js — kept in lockstep here.)
// ============================================================================

const fs = require("fs");
const path = require("path");
const zlib = require("zlib");

const ROOT = path.resolve(__dirname, "..");
const CARTS_DIR = path.join(ROOT, "editor/public/carts");
const SRC_DIR = path.join(ROOT, "tools/carts");
const INDEX = path.join(CARTS_DIR, "index.json");

const args = process.argv.slice(2);
const QUIET = args.includes("--quiet");
const DUMP_SOURCE = args.includes("--source");
const DIMS = args.includes("--dims");
const nameArg = args.find(a => !a.startsWith("--"));

if (!nameArg) {
  console.error("usage: node tools/cart-info.js <name|path> [--source] [--quiet]");
  process.exit(2);
}

// resolve "boom", "boom.cart.png", or any path to a base name
const base = path.basename(nameArg).replace(/\.cart\.png$/, "").replace(/\.c$/, "");
const pngPath = path.join(CARTS_DIR, base + ".cart.png");
const srcPath = path.join(SRC_DIR, base + ".c");
const cfgPath = path.join(SRC_DIR, base + ".cart.js");

if (!fs.existsSync(pngPath)) {
  console.error(`no cart png at ${path.relative(ROOT, pngPath)}`);
  process.exit(2);
}

// --- read every de:* zTXt chunk into { key: inflatedString } ---
function embeddedChunks(pngPath) {
  const b = fs.readFileSync(pngPath);
  const out = {};
  let o = 8; // skip PNG signature
  while (o + 12 <= b.length) {
    const len = b.readUInt32BE(o);
    const type = b.toString("ascii", o + 4, o + 8);
    if (type === "zTXt") {
      const data = b.slice(o + 8, o + 8 + len);
      const z = data.indexOf(0);
      const key = z >= 0 ? data.toString("latin1", 0, z) : "";
      if (key.startsWith("de:")) {
        try { out[key.slice(3)] = zlib.inflateSync(data.slice(z + 2)).toString("utf8"); }
        catch (e) { out[key.slice(3)] = null; }
      }
    }
    o += 12 + len;
  }
  return out;
}

const chunks = embeddedChunks(pngPath);
const embSource = chunks.source ?? null;
const srcText = fs.existsSync(srcPath) ? fs.readFileSync(srcPath, "utf8") : null;
const drifted = embSource !== null && srcText !== null && embSource.trim() !== srcText.trim();

// --quiet is the CI gate: silent, exit 1 on drift
if (QUIET) process.exit(drifted ? 1 : 0);

if (DUMP_SOURCE) {
  process.stdout.write(embSource ?? "(no de:source chunk)\n");
  process.exit(0);
}

// --dims: shell-sourceable DE_* dims from de:settings, so a build can derive a cart's
// screen/cell/map size instead of hand-passing it (the iOS single-cart build sources this).
// Exit 3 (not 2) when there's no settings chunk → the caller can keep its own defaults.
if (DIMS) {
  let s = null;
  try { s = chunks.settings ? JSON.parse(chunks.settings) : null; } catch (e) {}
  if (!s || !s.screenW || !s.screenH) process.exit(3);
  process.stdout.write(
    `DE_SCREEN_W=${s.screenW}\nDE_SCREEN_H=${s.screenH}\n` +
    `DE_MAP_W=${s.mapW || 128}\nDE_MAP_H=${s.mapH || 64}\n` +
    `DE_CELL_W=${s.cellW || 16}\nDE_CELL_H=${s.cellH || 16}\n`);
  process.exit(0);
}

// --- pretty print ---------------------------------------------------------
const C = { dim: "\x1b[2m", b: "\x1b[1m", y: "\x1b[33m", r: "\x1b[31m", g: "\x1b[32m", x: "\x1b[0m" };
const head = (s) => console.log(`\n${C.b}${s}${C.x}`);
const row = (k, v) => console.log(`  ${k.padEnd(14)} ${v}`);

console.log(`${C.b}cart: ${base}${C.x}`);

// SOURCE
head("SOURCE");
row("source .c", srcText !== null
  ? path.relative(ROOT, srcPath)
  : `${C.y}(none — tools/carts/${base}.c missing; png-only cart)${C.x}`);
if (fs.existsSync(cfgPath)) row("config", path.relative(ROOT, cfgPath) + " (sprites/map)");

// EMBED — what the editor actually loads
head("EMBED (what the editor loads)");
const keys = Object.keys(chunks);
row("chunks", keys.length ? keys.map(k => "de:" + k).join(", ") : `${C.y}none${C.x}`);
if (drifted) {
  row("source drift", `${C.r}STALE — de:source != tools/carts/${base}.c${C.x}`);
  console.log(`  ${C.r}↳ the editor loads OLD code. Re-embed:${C.x}`);
  console.log(`  ${C.dim}    node tools/make-cart.js tools/carts/${base}.c ${path.relative(ROOT, pngPath)}${C.x}`);
} else if (embSource === null) {
  row("source drift", `${C.y}no de:source chunk embedded${C.x}`);
} else if (srcText !== null) {
  row("source drift", `${C.g}in sync with the .c on disk${C.x}`);
}

// SCREEN / GRID
head("SCREEN / GRID");
let settings = null;
try { settings = chunks.settings ? JSON.parse(chunks.settings) : null; } catch (e) {}
if (settings) {
  const { screenW, screenH, scale, cellW, cellH, mapW, mapH, renderEvery } = settings;
  row("screen", `${screenW}x${screenH}  (window ${screenW * (scale || 1)}x${screenH * (scale || 1)} @ ${scale}x)`);
  row("sprite cell", `${cellW}x${cellH}   tilemap ${mapW}x${mapH}`);
  if (renderEvery && renderEvery !== 1) row("renderEvery", renderEvery);
  // derive the cart's own pixel grid from its #define CELL, if present
  const m = srcText && srcText.match(/#define\s+CELL\s+(\d+)/);
  if (m && screenW && screenH) {
    const cell = parseInt(m[1], 10);
    row("CELL grid", `CELL=${cell}  →  GW×GH = ${Math.floor(screenW / cell)}×${Math.floor(screenH / cell)} cells`);
  }
} else {
  row("settings", `${C.y}no de:settings chunk (cart uses editor defaults)${C.x}`);
}

// REGISTRY
head("REGISTRY (index.json)");
let entry = null;
try {
  const idx = JSON.parse(fs.readFileSync(INDEX, "utf8"));
  const carts = Array.isArray(idx) ? idx : (idx.carts || []);
  entry = carts.find(c => (c.file || "").replace(/\.cart\.png$/, "") === base || c.title === base);
} catch (e) {}
if (entry) {
  if (entry.kind) row("kind", entry.kind.join(", "));
  if (entry.genre) row("genre", Array.isArray(entry.genre) ? entry.genre.join(", ") : entry.genre);
  if (entry.teaches) row("teaches", entry.teaches.join(", "));
  if (entry.lineage) row("lineage", entry.lineage);
  if (entry.description) {
    const d = entry.description;
    row("description", (d.length > 220 ? d.slice(0, 217) + "…" : d));
  }
} else {
  row("entry", `${C.y}NOT registered in index.json — run node tools/lint-carts.js${C.x}`);
}

console.log(`\n${C.dim}staleness/publish state → node tools/cart-status.js${C.x}`);
