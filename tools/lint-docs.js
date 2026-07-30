#!/usr/bin/env node
// lint-docs.js — validate cross-references in docs/.
//
// Checks four things, all mechanically certain:
//   1. Relative .md links resolve:  [text](path.md) / [text](../dir/path.md#x)
//      (anchors ignored; http(s)/mailto skipped)
//   2. Doc-qualified §-references point at a numbered heading that exists:
//      "audio-notes §8.9", "[...](instrument-engines.md) §8.8.2", "touch-notes.md → §7"
//      A §-ref binds to the NEAREST doc name BEFORE it on the same line (≤80 chars away).
//   3. Tool-index discoverability: every tools/*.{js,sh} and every non-data tools/ subdir is
//      referenced in CLAUDE.md (the always-in-context index). This is the gate for the recurring
//      "the tool exists but no agent finds it because it's only linked from a deep design doc"
//      class — swcanvas_test, road-check, det-probes were all index-invisible. A new internal-only
//      tool either earns a one-line pointer or gets allowlisted in DATA_SUBDIRS below.
//   4. Cart-land header discoverability: every cart-land `runtime/*.h` (ADR-0006) appears BOTH in
//      CLAUDE.md's runtime/ block and in cart-authoring.md's "Cart-land library headers" table.
//      Same class as 3 — the lists that stop a cart from hand-rolling a capability that exists —
//      and both had silently rotted, in opposite directions. Platform seams and generated font
//      tables are allowlisted in ENGINE_INTERNALS / GENERATED_H below.
//
// §-resolution is by PREFIX: if §8.4 has no exact heading but §8 exists (e.g. a
// "→ moved" stub left by a doc split), that's a SOFT note, not an error — stubs
// are load-bearing redirects and append-only history (STATUS) must not be forced
// to churn. Only a ref whose top-level section is entirely absent is an error.
//
// Deliberately NOT checked (would be false-positive farms):
//   - bare §-refs with no doc name on the line — in split docs these legitimately
//     point at the sibling file (see instrument-engines.md's provenance note).
//     They are counted in the summary for transparency.
//   - docs/archive/ — superseded notes kept for history; staleness is the point.
//   - external URLs.
//
// Run after any doc reorg/split/rename:  node tools/lint-docs.js
// Exit 1 on hard errors (broken links / unresolvable §-refs), 0 otherwise.
//
// Companion (the INVERSE check): `tools/lint-xrefs.js` finds links that SHOULD
// exist but don't — unlinked doc-name mentions + missing backlinks. This one
// checks existing links resolve; that one checks the corpus is well-connected.

const fs = require('fs');
const path = require('path');

const DOCS = path.join(__dirname, '..', 'docs');

// ---------- collect files ----------
function walk(dir, out = []) {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) {
      if (e.name === 'archive') continue;
      walk(p, out);
    } else if (e.name.endsWith('.md')) out.push(p);
  }
  return out;
}
const files = walk(DOCS);

// ---------- heading index: basename (sans .md) -> Set of section numbers ----------
const sections = new Map(); // 'audio-notes' -> Set{'1','2','8','8.1',...}
const prosable = new Set(); // names eligible for prose-mention binding
for (const f of files) {
  const base = path.basename(f, '.md');
  const set = sections.get(base) || new Set();
  for (const line of fs.readFileSync(f, 'utf8').split('\n')) {
    const m = line.match(/^#{1,4}\s+(\d+(?:\.\d+)*)[.\s]/);
    if (m) set.add(m[1]);
  }
  sections.set(base, set);
  // cart-specs are per-game files named after common words (pool, outrun,
  // farm…) — never §-referenced in prose; binding them is pure false positive
  if (!f.includes(`${path.sep}cart-specs${path.sep}`)) prosable.add(base);
}
// doc names worth scanning prose for, longest first so 'instrument-engines'
// wins over any shorter accidental substring. A doc with NO numbered headings
// (STATUS, VISION, game-music…) can never satisfy a §-ref — binding to it is
// always a false positive ("STATUS item 5, the §13 order…" — §13 isn't STATUS's).
const docNames = [...prosable]
  .filter(n => sections.get(n).size > 0)
  .sort((a, b) => b.length - a.length);

const errors = [], notes = [];
let bareRefs = 0, linksChecked = 0, refsChecked = 0, selfResolved = 0;

for (const f of files) {
  const rel = path.relative(path.join(DOCS, '..'), f);
  const dir = path.dirname(f);
  const lines = fs.readFileSync(f, 'utf8').split('\n');
  let inFence = false;

  lines.forEach((line, i) => {
    if (/^\s*```/.test(line)) { inFence = !inFence; return; }
    const where = `${rel}:${i + 1}`;

    // ---- 1. markdown links to .md files (also checked inside fences? no) ----
    if (!inFence) {
      for (const m of line.matchAll(/\]\(([^)\s]+?\.md)(#[^)\s]*)?\)/g)) {
        const target = m[1];
        if (/^(https?:|mailto:)/.test(target)) continue;
        linksChecked++;
        if (!fs.existsSync(path.resolve(dir, target)))
          errors.push(`${where}  broken link: (${target})`);
      }
    }

    // ---- 2. §-references ----
    const refs = [...line.matchAll(/§\s?(\d+(?:\.\d+)*)/g)];
    if (!refs.length) return;
    // positions of doc-name mentions on this line
    const mentions = [];
    for (const name of docNames) {
      let idx = -1;
      while ((idx = line.indexOf(name, idx + 1)) !== -1) {
        // avoid matching inside a longer already-recorded name at same spot
        if (!mentions.some(x => idx >= x.at && idx < x.at + x.name.length))
          mentions.push({ name, at: idx });
      }
    }
    for (const r of refs) {
      const at = r.index;
      // nearest mention strictly before the §, within 80 chars
      const owner = mentions
        .filter(x => x.at < at && at - (x.at + x.name.length) <= 80)
        .sort((a, b) => b.at - a.at)[0];
      if (!owner) { bareRefs++; continue; }
      refsChecked++;
      const have = sections.get(owner.name);
      const num = r[1];
      if (have.has(num)) continue; // exact hit
      // prefix resolution: walk up to the nearest existing parent section
      let parent = num;
      while (parent.includes('.') && !have.has(parent))
        parent = parent.replace(/\.\d+$/, '');
      if (have.has(parent)) {
        notes.push(`${where}  ${owner.name} §${num} resolves only via §${parent} (stub/parent)`);
        continue;
      }
      // ambiguity escape: a doc-name earlier on the line doesn't mean every
      // later §-ref is its — "…[instrument-engines.md]… choke (§12)" means
      // THIS file's §12. If the current file has the section, assume self.
      const self = sections.get(path.basename(f, '.md'));
      if (self && self.has(num)) { selfResolved++; refsChecked--; continue; }
      errors.push(`${where}  ${owner.name} §${num} — no such section (not even a parent)`);
    }
  });
}

// ---------- 3. tool-index discoverability (tools/ ↔ CLAUDE.md) ----------
// Allowlist: subdirs that are DATA/vendor/assets, not tool-code an agent needs to discover.
const DATA_SUBDIRS = new Set(['reels', 'vendor', 'web-audio-shim', 'fonts', '.claude', 'reddit-gaps-cache', 'canvas-golden', 'testdata']);
const TOOLS = path.join(DOCS, '..', 'tools');
const CLAUDE = path.join(DOCS, '..', 'CLAUDE.md');
let toolsChecked = 0;
if (fs.existsSync(CLAUDE) && fs.existsSync(TOOLS)) {
  const claude = fs.readFileSync(CLAUDE, 'utf8');
  for (const e of fs.readdirSync(TOOLS, { withFileTypes: true })) {
    if (e.isFile() && /\.(js|sh)$/.test(e.name)) {
      toolsChecked++;
      if (!claude.includes(e.name))                       // full filename incl. extension = unambiguous
        errors.push(`CLAUDE.md  tool not indexed: tools/${e.name} — add a one-line pointer to the tools list (discoverability gate)`);
    } else if (e.isDirectory() && !DATA_SUBDIRS.has(e.name)) {
      toolsChecked++;
      if (!claude.includes(`${e.name}/`))
        errors.push(`CLAUDE.md  tool dir not indexed: tools/${e.name}/ — add a pointer (or allowlist it in lint-docs DATA_SUBDIRS if it's data/vendor)`);
    }
  }
}

// ---------- 4. cart-land header discoverability (runtime/*.h ↔ CLAUDE.md + the guide) ----------
// Same gate as 3, for the OTHER thing an agent has to find before it hand-rolls one: the
// cart-land library headers (ADR-0006). Two hand-kept lists describe them — CLAUDE.md's
// runtime/ block (the always-in-context pointer) and cart-authoring.md's "Cart-land library
// headers" table (the full contract) — and in 2026-07 both had rotted in OPPOSITE directions:
// CLAUDE.md was missing 7 (ampcab/boxrig/disclose/fxicons/json/morphdrum/shadermath), the table
// was missing 5 (boxrig/cards/citygen/morphdrum/roadkit), so neither was the full set. A hole
// here is how a cart ends up re-hand-rolling a disclosure pass or a second morphing drum voice.
//
// The editor's sidebar list stopped needing a gate — it's DERIVED from the directory now
// (vite /runtime-list.json). These two can't be: each entry has to say WHEN to reach for the
// header, which is writing, not generation. Hence a check instead.
const ENGINE_INTERNALS = new Set([
  'studio.h',            // the public API, not a cart-land library
  'sound.h', 'spec.h',   // engine + harness, documented elsewhere in CLAUDE.md
  'color.h', 'game_rect.h', 'platform.h', 'raylib_compat.h',   // platform seams
  'demath.h',            // engine numerics seam (deterministic transcendentals); not cart-land yet
  'mic.h', 'mic_desktop.h', 'midi_input.h',                    // host input plumbing
  'stb_image.h', 'studio_tcc_symbols.h',                       // vendored / generated
]);
const GENERATED_H = /(_data|_font|_baked)\.h$/;   // baked font tables
const RUNTIME = path.join(DOCS, '..', 'runtime');
const GUIDE = path.join(DOCS, 'guides', 'cart-authoring.md');
let headersChecked = 0;
if (fs.existsSync(CLAUDE) && fs.existsSync(RUNTIME) && fs.existsSync(GUIDE)) {
  const claude = fs.readFileSync(CLAUDE, 'utf8');
  const guide = fs.readFileSync(GUIDE, 'utf8');
  const at = guide.indexOf('Cart-land library headers');
  const table = at === -1 ? '' : guide.slice(at);   // the table + everything after it
  if (at === -1)
    errors.push('docs/guides/cart-authoring.md  the "Cart-land library headers" section is gone — the header table is the full contract half of the gate');
  for (const name of fs.readdirSync(RUNTIME).sort()) {
    if (!name.endsWith('.h') || ENGINE_INTERNALS.has(name) || GENERATED_H.test(name)) continue;
    headersChecked++;
    if (!claude.includes(name))
      errors.push(`CLAUDE.md  cart-land header not indexed: runtime/${name} — add a one-line pointer to the runtime/ block (or allowlist it in lint-docs ENGINE_INTERNALS if it's a platform seam)`);
    if (table && !table.includes(name))
      errors.push(`docs/guides/cart-authoring.md  cart-land header missing from the table: runtime/${name} — add a row (what it gives you · when to reach for it · showcase carts)`);
  }
}

// ---------- report ----------
if (errors.length) {
  console.log(`ERRORS (${errors.length}):`);
  for (const e of errors) console.log('  ' + e);
}
if (notes.length) {
  console.log(`\nsoft notes (${notes.length}) — resolve via a parent/stub heading, fine if intentional:`);
  for (const n of notes) console.log('  ' + n);
}
console.log(`\n${files.length} files · ${linksChecked} md-links checked · ${refsChecked} doc-qualified §-refs checked · ${bareRefs} bare + ${selfResolved} self-resolved §-refs not checked (by design) · ${toolsChecked} tools/dirs indexed in CLAUDE.md · ${headersChecked} cart-land headers indexed in CLAUDE.md + cart-authoring`);
process.exit(errors.length ? 1 : 0);
