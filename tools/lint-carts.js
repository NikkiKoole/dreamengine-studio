#!/usr/bin/env node
// Lint the cart registry. Each cart owns a `de:meta` block in its tools/carts/<name>.c
// (the source of truth); editor/public/carts/index.json is GENERATED from those blocks
// by build-cart-index.js. This validator owns the VOCABULARY + rules, checks every
// de:meta, and asserts index.json is in sync with a fresh generate. Run after editing a
// cart's de:meta or baking:
//
//   node tools/lint-carts.js
//
// Rules:
//   - every de:meta needs title, a non-empty kind[], teaches[] (may be []), description
//   - kind from KINDS; genre (if present) from GENRES; kind "game" REQUIRES a genre
//   - status (if present) from STATUSES; homage (if present) a non-empty string
//   - teaches values from the controlled vocab (tools/teaches-vocab.js)
//   - description is a string OR { summary?, detail?, controls? } with ≥1 string part
//   - todo (if present) is a non-empty string[] — the per-cart polish punch-list; authoring-only,
//     NOT emitted to index.json, read by cart-todos.js
//   - every de:meta cart has a baked .cart.png; every .cart.png has a de:meta cart
//   - editor/public/carts/index.json equals a fresh build-cart-index generate
//
// Growing a vocabulary is fine and expected — add the value here (or in teaches-vocab.js)
// in the same commit as the first cart that uses it.

const fs = require("fs");
const path = require("path");
const { readMeta, inSync, CARTS } = require("./build-cart-index.js");

const KINDS = [
  "tutorial",   // numbered on-ramp or teaches one concept
  "game",       // complete playable game (requires genre)
  "tech-demo",  // shows off an engine/toolkit capability
  "instrument", // makes sound/music interactively
  "toy",        // fun to poke at, no goal
  "tool",       // produces something you take elsewhere (sfx editor...)
  "probe",      // built to answer an API question (see docs/design/probe-carts.md)
  "generative", // the cart's output is generated art/music itself
];

const GENRES = [
  "arcade", "shooter", "platformer", "fighting", "puzzle", "racing",
  "sports", "strategy", "rpg", "adventure", "simulation", "sandbox",
  "tabletop", "maze", "space", "lab", "dating",
  // finer buckets carved out of the broad ones (2026-06):
  "tycoon",   // economy/business/empire management (transport, tradewinds, beltworks…)
  "tactics",  // turn-based squad/artillery combat (advancewars, xcom, worms…)
  "word",     // word games (boggle, hangman, wordle)
  "4x",       // explore/expand/exploit/exterminate (civ, fourx, orion)
];

// cart lifecycle (de:meta.status) — seeds the curated/featuring views (003-curation)
const STATUSES = ["active", "showcase", "retired", "archive", "hidden"];

// teaches[] is a CONTROLLED vocabulary like kind/genre: an off-list tag is a hard error,
// so adding one is a deliberate edit to tools/teaches-vocab.js. lineage is free prose.
const TEACHES = new Set(require("./teaches-vocab.js").TEACHES_VOCAB);

const ROOT = path.join(__dirname, "..");
const CARTS_DIR = path.join(ROOT, "editor", "public", "carts");

const errors = [];
const metaCarts = new Set(); // <name>.cart.png for every cart that has a de:meta

for (const f of fs.readdirSync(CARTS).sort()) {
  if (!f.endsWith(".c")) continue;
  const name = f.slice(0, -2);
  const src = fs.readFileSync(path.join(CARTS, f), "utf8");
  let m;
  try { m = readMeta(src, name); }
  catch (e) { errors.push(`${name}.c: ${e.message}`); continue; }
  if (!m) continue; // no de:meta = not a gallery cart (test/probe carts) — fine

  const png = `${name}.cart.png`;
  metaCarts.add(png);
  const at = `${name}.c de:meta`;

  if (!m.title || typeof m.title !== "string") errors.push(`${at}: missing title`);
  // slug — the canonical <name>, the anchor from a .cart.png back to tools/carts/<name>.c
  // (design/editor-cart-workflow.md Gap 1b). Optional-while-we-backfill; when present it MUST
  // equal the filename stem, so the PNG→source link can't silently drift. Backfill: node
  // tools/backfill-slug.js. Once every cart carries it, flip this to required.
  if ("slug" in m) {
    if (typeof m.slug !== "string" || !m.slug)
      errors.push(`${at}: slug must be a non-empty string`);
    else if (m.slug !== name)
      errors.push(`${at}: slug '${m.slug}' must equal the filename stem '${name}' (fix the slug, or rename the .c)`);
  }
  if ("status" in m && !STATUSES.includes(m.status))
    errors.push(`${at}: bad status '${m.status}' — pick from: ${STATUSES.join(", ")}`);
  if (typeof m.created !== "string" || !/^\d{4}-\d{2}-\d{2}$/.test(m.created))
    errors.push(`${at}: missing/invalid created (YYYY-MM-DD) — find it with: git log --follow --diff-filter=A --format=%cd --date=short -- tools/carts/${name}.c | tail -1`);

  if (!Array.isArray(m.kind) || m.kind.length === 0)
    errors.push(`${at}: missing kind[] — pick from: ${KINDS.join(", ")}`);
  else for (const k of m.kind)
    if (!KINDS.includes(k)) errors.push(`${at}: bad kind '${k}'`);

  if (m.genre != null && !GENRES.includes(m.genre)) errors.push(`${at}: bad genre '${m.genre}'`);
  if (Array.isArray(m.kind) && m.kind.includes("game") && m.genre == null)
    errors.push(`${at}: kind 'game' requires a genre — pick from: ${GENRES.join(", ")}`);
  if ("homage" in m && (typeof m.homage !== "string" || !m.homage))
    errors.push(`${at}: homage must be a non-empty string`);
  if ("lineage" in m && typeof m.lineage !== "string") errors.push(`${at}: lineage must be a string`);

  // resizable — optional opt-in (device-adaptive-layout.md Phase 1b). true → the editor ▶-run,
  // build-app and the CLI compile the cart with -DDE_RESIZABLE (a live-reflowing window; read
  // screen_w()/screen_h(), lay out with lay.h). Omit or false = fixed canvas (every existing cart).
  if ("resizable" in m && typeof m.resizable !== "boolean")
    errors.push(`${at}: resizable must be a boolean (omit it for a fixed-canvas cart)`);

  // todo[] — optional per-cart polish punch-list (NOT emitted to index.json; read by cart-todos.js).
  // If present it must be a non-empty array of non-empty strings (drop the field when nothing's left).
  if ("todo" in m) {
    if (!Array.isArray(m.todo) || m.todo.length === 0)
      errors.push(`${at}: todo must be a non-empty string[] (remove the field if there's nothing to do)`);
    else for (const t of m.todo)
      if (typeof t !== "string" || !t.trim()) errors.push(`${at}: todo entries must be non-empty strings`);
  }

  if (!Array.isArray(m.teaches))
    errors.push(`${at}: missing teaches[] — use [] if nothing conceptually distinctive`);
  else for (const t of m.teaches)
    if (!TEACHES.has(t)) errors.push(`${at}: teaches '${t}' not in the vocabulary — reuse one, or add it deliberately to tools/teaches-vocab.js`);

  // description: string, or { summary?, detail?, controls? } with ≥1 string part
  const d = m.description;
  const okString = typeof d === "string" && d.length > 0;
  const okParts = d && typeof d === "object" &&
    ["summary", "detail", "controls"].some(k => typeof d[k] === "string" && d[k].length > 0);
  if (!okString && !okParts)
    errors.push(`${at}: description must be a non-empty string or { summary/detail/controls }`);

  if (!fs.existsSync(path.join(CARTS_DIR, png)))
    errors.push(`${at}: has de:meta but no baked ${png} — run: node tools/make-cart.js ${path.relative(ROOT, path.join(CARTS, f))} ${path.relative(ROOT, path.join(CARTS_DIR, png))}`);
}

// a .cart.png with no de:meta cart can never be (re)generated into the index
for (const f of fs.readdirSync(CARTS_DIR))
  if (f.endsWith(".cart.png") && !metaCarts.has(f))
    errors.push(`${f}: on disk but its tools/carts/${f.replace(/\.cart\.png$/, ".c")} has no de:meta block (add one to register it)`);

// the generated index.json must match what the de:meta blocks produce right now
if (!inSync())
  errors.push("editor/public/carts/index.json is STALE vs the de:meta blocks — run: node tools/build-cart-index.js");

if (errors.length) {
  for (const e of errors) console.error("  ✗ " + e);
  console.error(`\n${errors.length} problem(s) across ${metaCarts.size} registered carts.`);
  process.exit(1);
}
console.log(`ok — ${metaCarts.size} carts, all de:meta valid and index.json in sync.`);
