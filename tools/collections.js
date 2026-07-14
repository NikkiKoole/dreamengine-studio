#!/usr/bin/env node
// collections.js — the roll-up view over cart COLLECTIONS: the cross-cutting, doc-anchored
// research threads a cart belongs to (docs/field-notes/003-curation.md's "curated collections").
// Each collection is defined ONCE in tools/collections-vocab.js (slug + title + anchor doc +
// blurb); each cart declares its membership in its de:meta `collection[]` (emitted to index.json
// by build-cart-index.js). This tool joins the two into one browsable list — the "show me the
// road stuff / the radio stations" view. A sibling of cart-todos.js: it only READS.
//
//   node tools/collections.js              # every collection: title · doc · N carts + members
//   node tools/collections.js <slug>       # just that collection (e.g. road, radio, tinyjam)
//   node tools/collections.js --counts      # one line per collection: slug  N
//   node tools/collections.js --json        # machine-readable { slug: {title,doc,blurb,carts[]} }
//
// To add a collection: tools/collections-vocab.js (it must point at a doc that exists — lint
// enforces). To add a cart to one: add the slug to that cart's de:meta collection[].

const fs = require("fs");
const path = require("path");
const { COLLECTIONS } = require("./collections-vocab.js");

const ROOT = path.join(__dirname, "..");
const INDEX = path.join(ROOT, "editor", "public", "carts", "index.json");

// { slug: [ {slug,title} sorted ] } from index.json's per-cart collection[]
function membersBySlug() {
  const carts = JSON.parse(fs.readFileSync(INDEX, "utf8"));
  const by = Object.fromEntries(COLLECTIONS.map((c) => [c.slug, []]));
  for (const c of carts) {
    if (!Array.isArray(c.collection)) continue;
    const slug = c.file.replace(/\.cart\.png$/, "");
    for (const col of c.collection)
      if (by[col]) by[col].push({ slug, title: c.title || slug });
  }
  for (const k of Object.keys(by)) by[k].sort((a, b) => a.slug.localeCompare(b.slug));
  return by;
}

module.exports = { membersBySlug };

// ---- CLI ----
if (require.main === module) {
  const args = process.argv.slice(2);
  const only = args.find((a) => !a.startsWith("--")) || null;
  const by = membersBySlug();

  if (args.includes("--json")) {
    const out = {};
    for (const c of COLLECTIONS)
      out[c.slug] = { title: c.title, doc: c.doc, blurb: c.blurb, carts: by[c.slug].map((m) => m.slug) };
    console.log(JSON.stringify(only ? { [only]: out[only] } : out, null, 2));
    process.exit(0);
  }

  if (args.includes("--counts")) {
    for (const c of COLLECTIONS) console.log(`${c.slug.padEnd(14)} ${by[c.slug].length}`);
    process.exit(0);
  }

  const list = only ? COLLECTIONS.filter((c) => c.slug === only) : COLLECTIONS;
  if (only && !list.length) {
    console.error(`no collection '${only}' — known: ${COLLECTIONS.map((c) => c.slug).join(", ")}`);
    process.exit(1);
  }

  for (const c of list) {
    const members = by[c.slug];
    console.log(`\n\x1b[1m${c.title}\x1b[0m  (${c.slug}) — ${members.length} cart(s)`);
    console.log(`  ${c.blurb}`);
    console.log(`  ↳ ${c.doc}`);
    for (const m of members) console.log(`    · ${m.slug}${m.title !== m.slug ? `  — ${m.title}` : ""}`);
  }
  const total = new Set(list.flatMap((c) => by[c.slug].map((m) => m.slug))).size;
  console.log(`\n${list.length} collection(s), ${total} distinct cart(s).`);
}
