#!/usr/bin/env node
// ============================================================================
// lint-xrefs.js — find docs that SHOULD cross-link but don't.
//
//   node tools/lint-xrefs.js            # full report (whole docs/ corpus)
//   node tools/lint-xrefs.js touch      # SCOPED: only findings touching a doc whose
//                                          path matches "touch" (pairs with topic-brief:
//                                          find the feature's docs, then check links AMONG them)
//
// THE FRICTION THIS KILLS. lint-docs.js checks that EXISTING links RESOLVE. This
// is the opposite gap: a link that SHOULD exist and doesn't, so a reader who
// starts in doc A never discovers doc B — even though B is the real home of what
// A is talking about. The case that motivated it: ios-plan.md discusses the
// virtual gamepad for paragraphs and never points at touch-controls.md (the
// actual 7-step plan), while touch-controls.md DOES link ios-plan.md. The link
// is one-directional, so a code-/iOS-first reader misses the design doc entirely.
//
// Two mechanical signals, reported in confidence tiers:
//
//   A. UNLINKED MENTIONS (high confidence, usually actionable)
//      Doc A writes another doc's hyphenated basename as bare prose
//      (e.g. "touch-controls", "audio-notes", "engine-portability") but has NO
//      markdown link to that doc anywhere in the file. The named doc should be
//      clickable. Fenced code blocks are excluded (a path in a shell example is
//      fine unlinked). Hyphen-only basenames keep this unambiguous (single-word
//      names like "spec"/"run" would over-fire and are skipped).
//
//   B. MISSING BACKLINKS (review tier)
//      A links B but B doesn't link A — an asymmetric reference. Often fine, but
//      it's exactly the ios-plan ↔ touch-controls case. HUB docs (STATUS, README,
//      VISION, history, the field-notes index) are exempt as the un-backlinked
//      side — they legitimately point outward without every doc pointing back.
//
// Companion to lint-docs.js (broken links / §-refs / tool-index). Scope: docs/
// (archive/ skipped — staleness there is the point). ADVISORY — always exits 0;
// the corpus carries a standing backlog, so scope to a feature to act on it.
// ============================================================================

const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "..");
const DOCS = path.join(ROOT, "docs");
const scope = process.argv.slice(2).find(a => !a.startsWith("--")) || "";
const touchesScope = (...rels) => !scope || rels.some(r => r.toLowerCase().includes(scope.toLowerCase()));

// hub docs: point outward by design; don't demand a backlink TO them.
const HUBS = new Set(["STATUS", "README", "VISION", "history", "FIELD-NOTES", "glossary"]);

// ---- collect docs ----
function walk(dir, out = []) {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) { if (e.name === "archive") continue; walk(p, out); }
    else if (e.name.endsWith(".md")) out.push(p);
  }
  return out;
}
const files = walk(DOCS);

// basename → abs path (basenames collide rarely in docs/; keep the first, warn-free)
const byBase = new Map();
for (const f of files) {
  const b = path.basename(f, ".md");
  if (!byBase.has(b)) byBase.set(b, f);
}

// candidate target basenames: hyphenated, with an alpha segment ≥4 — unambiguous
// doc references (touch-controls, ios-plan, engine-portability, …). This skips
// numeric ADR names and short common words on purpose.
const targetBases = [...byBase.keys()].filter(b =>
  b.includes("-") && b.split("-").some(seg => /^[a-z]{4,}$/i.test(seg)));

// ---- per-file: text, link targets (abs), fenced-mask ----
const esc = s => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
const info = new Map(); // abs -> { rel, text, lines, links:Set<abs>, fenced:bool[] }
for (const f of files) {
  const text = fs.readFileSync(f, "utf8");
  const lines = text.split("\n");
  const links = new Set();
  const dir = path.dirname(f);
  for (const m of text.matchAll(/\]\(([^)\s]+?\.md)(?:#[^)\s]*)?\)/g)) {
    if (/^(https?:|mailto:)/.test(m[1])) continue;
    links.add(path.resolve(dir, m[1]));
  }
  // mark fenced lines so a path inside a code block isn't a "mention"
  const fenced = [];
  let inFence = false;
  for (const l of lines) { if (/^\s*```/.test(l)) inFence = !inFence; fenced.push(inFence); }
  info.set(f, { rel: path.relative(ROOT, f), text, lines, links, fenced });
}

// ============================================================================
// A. unlinked mentions
// ============================================================================
const unlinked = []; // { rel, base, target, ln, text }
for (const f of files) {
  const me = info.get(f);
  const selfBase = path.basename(f, ".md");
  for (const base of targetBases) {
    if (base === selfBase) continue;
    const targetAbs = byBase.get(base);
    if (me.links.has(targetAbs)) continue;          // already linked somewhere → fine
    // word-ish match, optional .md, not preceded by a word/path/link char
    const re = new RegExp(`(?<![\\w/.(-])${esc(base)}(?:\\.md)?(?![\\w-])`, "g");
    for (let i = 0; i < me.lines.length; i++) {
      if (me.fenced[i]) continue;
      if (re.test(me.lines[i])) {
        unlinked.push({ rel: me.rel, base, target: path.relative(ROOT, targetAbs), ln: i + 1, text: me.lines[i].trim() });
        break;                                       // one example line per (file,target) is enough
      }
    }
  }
}

// ============================================================================
// B. missing backlinks (asymmetric links)
// ============================================================================
const missingBack = []; // { from (links out), to (no backlink) }
for (const a of files) {
  const A = info.get(a);
  for (const b of A.links) {
    if (!info.has(b)) continue;                      // unresolved link → lint-docs' job
    if (a === b) continue;
    const B = info.get(b);
    if (B.links.has(a)) continue;                    // mutual → fine
    const aBase = path.basename(a, ".md");
    if (HUBS.has(aBase)) continue;                   // don't demand a backlink to a hub
    missingBack.push({ from: A.rel, to: B.rel, fromBase: aBase });
  }
}

// ============================================================================
// report
// ============================================================================
const tty = process.stdout.isTTY;
const bold = s => (tty ? `\x1b[1m${s}\x1b[0m` : s);
const dim = s => (tty ? `\x1b[2m${s}\x1b[0m` : s);
const clip = (s, n) => (s.length > n ? s.slice(0, n - 1) + "…" : s);

const unlinkedShown = unlinked.filter(u => touchesScope(u.rel, u.target));
const backShown = missingBack.filter(m => touchesScope(m.to, m.from));

if (scope) console.log(dim(`scoped to docs matching "${scope}"\n`));

if (unlinkedShown.length) {
  console.log(bold(`UNLINKED MENTIONS (${unlinkedShown.length}) — a named doc that isn't clickable:`));
  for (const u of unlinkedShown) {
    console.log(`  ${u.rel}:${u.ln}  names ${bold(u.base)} ` + dim(`→ link ${u.target}`));
    console.log(dim(`    ${clip(u.text, 100)}`));
  }
  console.log("");
}

if (backShown.length) {
  console.log(bold(`MISSING BACKLINKS (${backShown.length}) — A links B, B doesn't link back (review):`));
  // group by the doc that's missing the backlink
  const byTo = new Map();
  for (const m of backShown) { (byTo.get(m.to) || byTo.set(m.to, []).get(m.to)).push(m.from); }
  for (const [to, froms] of [...byTo.entries()].sort((x, y) => y[1].length - x[1].length)) {
    console.log(`  ${bold(to)} ` + dim(`has no link back to: `) + froms.join(", "));
  }
  console.log("");
}

if (!unlinkedShown.length && !backShown.length) console.log("no missing cross-references found" + (scope ? ` for "${scope}"` : "") + ".");
console.log(dim(`${files.length} docs · ${targetBases.length} linkable basenames · ` +
  `${unlinkedShown.length}/${unlinked.length} unlinked mentions · ${backShown.length}/${missingBack.length} missing backlinks${scope ? " (shown/total)" : ""}`));

