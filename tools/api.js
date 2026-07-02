#!/usr/bin/env node
// ============================================================================
// api.js — one-shot studio.h API lookup: print the exact signature + doc for a
// function/constant WITHOUT reading the whole studioDocs.js (the ~500-entry
// source of truth that also drives the editor's autocomplete/hover/help).
//
//   node tools/api.js circfill            # sig + doc for one name
//   node tools/api.js circ spr print      # several at once
//   node tools/api.js note_               # substring → discovery (lists matches)
//
// Exact name wins. No exact hit → substring search across names: one match
// prints in full, several list their signatures (so it doubles as "what's the
// name for…"). Cheaper than reading studioDocs.js or an LSP round trip when you
// just need a signature while writing cart code.
//
// studioDocs.js is an ES module; this CommonJS tool dynamic-imports it. Plain node.
// ============================================================================

const path = require("path");
const { pathToFileURL } = require("url");

const ROOT = path.resolve(__dirname, "..");
const queries = process.argv.slice(2).filter(a => !a.startsWith("--"));
if (!queries.length) {
  console.error("usage: node tools/api.js <name> [<name> ...]   (e.g. circfill, note_, CLR_RED)");
  process.exit(2);
}

const tty = process.stdout.isTTY;
const bold = s => (tty ? `\x1b[1m${s}\x1b[0m` : s);
const dim  = s => (tty ? `\x1b[2m${s}\x1b[0m` : s);

(async () => {
  const url = pathToFileURL(path.join(ROOT, "editor/src/studioDocs.js")).href;
  let studioDocs;
  try { ({ studioDocs } = await import(url)); }
  catch (e) { console.error("could not load editor/src/studioDocs.js: " + e.message); process.exit(1); }

  const keys = Object.keys(studioDocs);
  const out = [];
  let missed = 0;

  function full(name) {
    const e = studioDocs[name];
    out.push(bold(name) + dim("   " + e.sig));
    for (const ln of String(e.doc).split("\n")) out.push("    " + ln);
  }

  for (const q of queries) {
    if (studioDocs[q]) { full(q); out.push(""); continue; }              // exact
    const ql = q.toLowerCase();
    const hits = keys.filter(k => k.toLowerCase().includes(ql));
    if (hits.length === 1) { full(hits[0]); out.push(""); continue; }    // one fuzzy → full
    if (hits.length) {                                                   // many → signatures
      out.push(dim(`${hits.length} match "${q}":`));
      for (const k of hits) out.push("  " + bold(k) + dim("  " + studioDocs[k].sig));
      out.push("");
      continue;
    }
    const docHits = keys.filter(k => (studioDocs[k].doc || "").toLowerCase().includes(ql));
    if (docHits.length) {                                                // name missed, doc text hits
      out.push(dim(`no API entry NAMED "${q}" — but mentioned in the doc of:`));
      for (const k of docHits.slice(0, 12)) out.push("  " + bold(k) + dim("  " + studioDocs[k].sig));
      if (docHits.length > 12) out.push(dim(`  … +${docHits.length - 12} more`));
      out.push("");
      continue;
    }
    out.push(dim(`no API entry matching "${q}".`));                      // nothing
    out.push(dim(`  not an API name? the repo-wide finder is: node tools/topic-brief.js "${q}"  (docs + carts + engine seam)`));
    out.push("");
    missed++;
  }

  console.log(out.join("\n").replace(/\n+$/, ""));
  process.exit(missed && missed === queries.length ? 1 : 0);
})();
