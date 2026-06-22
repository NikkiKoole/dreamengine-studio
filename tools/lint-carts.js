#!/usr/bin/env node
// Lint the cart registry: editor/public/carts/index.json vs the carts on disk.
//
// Replaces the old tag-carts.js one-shot (which carried its own copy of every
// cart's tags and went stale the moment agents started writing tags inline).
// This is a pure validator — index.json IS the source of truth; this file owns
// only the VOCABULARY and the rules. Run it after registering a cart:
//
//   node tools/lint-carts.js
//
// Rules:
//   - every entry needs title, description, file, and a non-empty kind[]
//   - kind values come from KINDS; genre (if present) from GENRES
//   - kind "game" REQUIRES a genre; for other kinds genre is optional but
//     encouraged when it aids browsing (platformer tutorials, lab carts...)
//   - every entry's .cart.png exists; every .cart.png on disk is registered
//
// Growing the vocabulary is fine and expected — add the value here in the
// same commit as the first cart that uses it.

const fs = require("fs");
const path = require("path");

// "probe" = built to answer an API question (should the engine own X, or can a
// cart?). The tag only marks the role; the QUESTION and VERDICT live in
// docs/design/probe-carts.md — update that ledger when a probe resolves.
const KINDS = [
  "tutorial",   // numbered on-ramp or teaches one concept
  "game",       // complete playable game (requires genre)
  "tech-demo",  // shows off an engine/toolkit capability
  "instrument", // makes sound/music interactively
  "toy",        // fun to poke at, no goal
  "tool",       // produces something you take elsewhere (sfx editor...)
  "probe",      // see note above
  "generative", // the cart's output is generated art/music itself
];

const GENRES = [
  "arcade", "shooter", "platformer", "fighting", "puzzle", "racing",
  "sports", "strategy", "rpg", "adventure", "simulation", "sandbox",
  "tabletop", "maze", "space", "lab", "dating",
];

// teaches[] (the conceptual techniques a cart shows off — the ★ techniques compendium)
// is a CONTROLLED vocabulary, like kind/genre: an off-list tag is a hard error here, so
// adding a new tag is a deliberate, reviewable edit to tools/teaches-vocab.js — not a
// casual side effect of tagging a cart. lineage is free prose. Both optional per entry.
const TEACHES = new Set(require("./teaches-vocab.js").TEACHES_VOCAB);

const ROOT = path.join(__dirname, "..");
const CARTS_DIR = path.join(ROOT, "editor", "public", "carts");
const INDEX = path.join(CARTS_DIR, "index.json");

const index = JSON.parse(fs.readFileSync(INDEX, "utf8"));
const errors = [];

// per-entry checks
const seen = new Set();
for (const e of index) {
  const f = e.file || "(no file)";
  if (!e.title) errors.push(`${f}: missing title`);
  if (!e.description) errors.push(`${f}: missing description`);
  if (!e.file) { errors.push(`entry '${e.title}': missing file`); continue; }
  if (seen.has(f)) errors.push(`${f}: registered twice`);
  seen.add(f);

  if (!Array.isArray(e.kind) || e.kind.length === 0)
    errors.push(`${f}: missing kind[] — pick from: ${KINDS.join(", ")}`);
  else
    for (const k of e.kind)
      if (!KINDS.includes(k)) errors.push(`${f}: bad kind '${k}'`);

  if (e.genre && !GENRES.includes(e.genre)) errors.push(`${f}: bad genre '${e.genre}'`);
  if (Array.isArray(e.kind) && e.kind.includes("game") && !e.genre)
    errors.push(`${f}: kind 'game' requires a genre — pick from: ${GENRES.join(", ")}`);
  if ("homage" in e && (typeof e.homage !== "string" || !e.homage))
    errors.push(`${f}: homage must be a non-empty string`);

  if ("teaches" in e) {
    if (!Array.isArray(e.teaches)) errors.push(`${f}: teaches must be an array`);
    else for (const t of e.teaches)
      if (!TEACHES.has(t)) errors.push(`${f}: teaches '${t}' not in the vocabulary — reuse an existing tag, or add it deliberately to tools/teaches-vocab.js`);
  }
  if ("lineage" in e && typeof e.lineage !== "string")
    errors.push(`${f}: lineage must be a string`);

  if (!fs.existsSync(path.join(CARTS_DIR, f)))
    errors.push(`${f}: registered but no such file in editor/public/carts/`);
}

// orphan carts on disk
for (const f of fs.readdirSync(CARTS_DIR))
  if (f.endsWith(".cart.png") && !seen.has(f))
    errors.push(`${f}: on disk but not registered in index.json`);

if (errors.length) {
  for (const e of errors) console.error("  ✗ " + e);
  console.error(`\n${errors.length} problem(s) across ${index.length} entries.`);
  process.exit(1);
}
console.log(`ok — ${index.length} carts, all tagged and on disk.`);
