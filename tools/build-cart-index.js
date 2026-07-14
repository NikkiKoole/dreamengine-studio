#!/usr/bin/env node
// build-cart-index.js — GENERATE editor/public/carts/index.json from each cart's
// own `de:meta` block. The cart is the source of truth; index.json is a derived view.
// Kills the multi-agent merge conflict on the shared index (CLAUDE.md hazard #2):
// editing a cart's tags/description now touches only that cart's .c.
//
//   node tools/build-cart-index.js          # regenerate index.json (writes the file)
//   node tools/build-cart-index.js --check   # exit 1 if index.json is stale (CI/cart-status)
//
// THE RULE: a cart appears in index.json IFF its tools/carts/<name>.c contains a
// `de:meta` block. Test/probe carts (soak, tunecheck, swcanvas_test, …) have none,
// so they stay unregistered automatically — no ignore-list.
//
// de:meta is a C block comment at the top of the .c (so it never affects compilation,
// travels harmlessly inside de:source, and is greppable). It is a SUPERSET of what
// index.json needs — index.json is just the gallery's slice. Shape + rationale:
// docs/design/cart-metadata.md.
//
//   /* de:meta
//   {
//     "title": "pixel zoo",
//     "status": "active",                 // active|showcase|retired|archive|hidden|wip (not emitted yet)
//     "created": "2026-05-30",            // YYYY-MM-DD, the .c's first-add date in git; immutable
//     "kind": ["toy","tech-demo"],
//     "genre": null,                       // omitted from index.json when null/absent
//     "homage": null,
//     "teaches": ["dithering-gradient"],
//     "collection": ["road"],              // doc-anchored threads (collections-vocab.js); omitted when empty
//     "lineage": "…",                      // omitted when absent
//     "description": "…"  OR  { "summary":"…", "detail":"…", "controls":"…" }
//   }
//   de:meta */
//
// `description` may be a plain string (legacy/migrated) or an object of parts; parts are
// flattened to one string for index.json (the gallery is unchanged) until a consumer
// renders them directly. `file` is DERIVED (<name>.cart.png), never stored.
// Reserved, ignored-until-a-consumer-exists: inputs, outputs, see_also, notes.
// Authoring-only, deliberately NOT emitted (no gallery churn): todo[] — the per-cart polish
// punch-list, validated by lint-carts.js and surfaced by cart-todos.js.

const fs = require("fs");
const path = require("path");

const CARTS = path.join(__dirname, "carts");
const PNG_DIR = path.join(__dirname, "..", "editor", "public", "carts");
const OUT = path.join(PNG_DIR, "index.json");

// orientation is DERIVED from the cart's runtime screen dims (de:settings in the .cart.png),
// never hand-tagged — so it can't drift from the actual screen. Default 320x200 landscape is
// omitted (the common case); only portrait/square carts carry the field, for a gallery filter.
function orientationOf(name) {
  try {
    const { extractCartChunks } = require("./make-cart.js");
    const c = extractCartChunks(fs.readFileSync(path.join(PNG_DIR, `${name}.cart.png`)));
    const s = JSON.parse(c.settings || "{}");
    const W = s.screenW ?? 320, H = s.screenH ?? 200;
    if (H > W * 1.1) return "portrait";
    if (Math.abs(W - H) <= W * 0.1) return "square";
  } catch (e) { /* missing/odd png → treat as default landscape */ }
  return null;
}

// extract + parse the de:meta block from a .c; null if absent
function readMeta(src, name) {
  const m = src.match(/\/\*\s*de:meta\s*\n([\s\S]*?)\nde:meta\s*\*\//);
  if (!m) return null;
  try {
    return JSON.parse(m[1]);
  } catch (e) {
    throw new Error(`${name}.c: de:meta block is not valid JSON — ${e.message}`);
  }
}

// parts object -> single index.json description string (string passes through)
function flattenDesc(d) {
  if (typeof d === "string") return d;
  if (d && typeof d === "object") {
    return [d.summary, d.detail, d.controls].filter(Boolean).join(" ");
  }
  return "";
}

// build one index.json entry in canonical key order, omitting absent optionals
function toEntry(meta, name) {
  const e = {};
  e.title = meta.title;
  e.description = flattenDesc(meta.description);
  e.file = `${name}.cart.png`;
  if (meta.created != null) e.created = meta.created;
  const ori = orientationOf(name);
  if (ori) e.orientation = ori;
  e.kind = meta.kind;
  e.teaches = meta.teaches || [];
  if (Array.isArray(meta.collection) && meta.collection.length) e.collection = meta.collection;
  if (meta.lineage != null) e.lineage = meta.lineage;
  if (meta.genre != null) e.genre = meta.genre;
  if (meta.homage != null) e.homage = meta.homage;
  return e;
}

function generate() {
  const entries = [];
  for (const f of fs.readdirSync(CARTS).sort()) {
    if (!f.endsWith(".c")) continue;
    const name = f.slice(0, -2);
    const meta = readMeta(fs.readFileSync(path.join(CARTS, f), "utf8"), name);
    if (!meta) continue; // no de:meta = not a gallery cart (test/probe carts)
    entries.push(toEntry(meta, name));
  }
  entries.sort((a, b) => a.file.localeCompare(b.file)); // deterministic; array order is not curated
  return JSON.stringify(entries, null, 2) + "\n";
}

// write the freshly generated index.json; returns the cart count
function writeIndex() {
  const out = generate();
  fs.writeFileSync(OUT, out);
  return JSON.parse(out).length;
}

// true if the committed index.json already matches a fresh generate
function inSync() {
  const cur = fs.existsSync(OUT) ? fs.readFileSync(OUT, "utf8") : "";
  return cur === generate();
}

module.exports = { generate, writeIndex, inSync, readMeta, flattenDesc, OUT, CARTS };

if (require.main === module) {
  if (process.argv.includes("--check")) {
    if (!inSync()) {
      console.error("index.json is STALE — run: node tools/build-cart-index.js");
      process.exit(1);
    }
    console.log("index.json is up to date.");
  } else {
    const n = writeIndex();
    console.log(`wrote ${path.relative(path.join(__dirname, ".."), OUT)} — ${n} carts`);
  }
}
