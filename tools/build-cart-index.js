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
//     "status": "active",                 // active|showcase|retired|archive|hidden (not emitted yet)
//     "kind": ["toy","tech-demo"],
//     "genre": null,                       // omitted from index.json when null/absent
//     "homage": null,
//     "teaches": ["dithering-gradient"],
//     "lineage": "…",                      // omitted when absent
//     "description": "…"  OR  { "summary":"…", "detail":"…", "controls":"…" }
//   }
//   de:meta */
//
// `description` may be a plain string (legacy/migrated) or an object of parts; parts are
// flattened to one string for index.json (the gallery is unchanged) until a consumer
// renders them directly. `file` is DERIVED (<name>.cart.png), never stored.
// Reserved, ignored-until-a-consumer-exists: inputs, outputs, see_also, notes.

const fs = require("fs");
const path = require("path");

const CARTS = path.join(__dirname, "carts");
const OUT = path.join(__dirname, "..", "editor", "public", "carts", "index.json");

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
  e.kind = meta.kind;
  e.teaches = meta.teaches || [];
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

const out = generate();

if (process.argv.includes("--check")) {
  const cur = fs.existsSync(OUT) ? fs.readFileSync(OUT, "utf8") : "";
  if (cur !== out) {
    console.error("index.json is STALE — run: node tools/build-cart-index.js");
    process.exit(1);
  }
  console.log("index.json is up to date.");
} else {
  fs.writeFileSync(OUT, out);
  const n = JSON.parse(out).length;
  console.log(`wrote ${path.relative(path.join(__dirname, ".."), OUT)} — ${n} carts`);
}
