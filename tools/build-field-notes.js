#!/usr/bin/env node
// build-field-notes.js — generate field-notes/FIELD-NOTES.md, a navigable index of
// the research journal. A sibling of build-compendium.js / build-cart-index.js: it
// READS every field-notes/*.md and DERIVES the index — never hand-edited, regenerated
// on demand. It honours the journal's append-only rule (README: "never rewrite history;
// write a -revisited note instead") — this tool only reads the notes, never edits them.
//
//   node tools/build-field-notes.js          → writes field-notes/FIELD-NOTES.md
//   node tools/build-field-notes.js --check   → exit 1 if the committed index is stale
//
// What it gives you that a flat directory listing can't:
//   • LIFECYCLE board — notes grouped by status (Hypothesis → Observed → Review →
//     Working Theory → Incorporated → Superseded). Answers "what's open vs what shipped".
//   • TIMELINE — numbered notes in chronological order (the evolution of understanding).
//   • RELATED-NOTE GRAPH — who references whom (from each note's "Related notes" list).
//   • CONCEPTS — tag → notes (from frontmatter `concepts:`).
//   • WORKING MATERIAL — the unnumbered supporting docs, listed but kept distinct.
//   • CONFORMANCE — journal notes that don't yet match standard-header.md, so they can be
//     nudged into shape when next touched (flagged, NEVER rewritten).
//
// Tolerant of the three metadata styles in the wild: the `**Status**`/`**Date**` bold
// block (002, 100…), the YAML-ish `title:`/`## status:` frontmatter (011–013), and the
// bare `Status` label (000, 001). Metadata is read only from each note's HEAD region, so
// example `status:` lines buried deep in a note (about-tags, tools-we-need) aren't misread.

const fs = require('fs');
const path = require('path');
const ROOT = path.resolve(__dirname, '..');
const DIR = path.join(ROOT, 'field-notes');
const OUT = path.join(DIR, 'FIELD-NOTES.md');

// Files that are NOT journal entries — meta/scaffolding, excluded from the index body.
const META_FILES = new Set(['FIELD-NOTES.md', 'README.md', 'REVIEW_CHANGES.md', 'standard-header.md']);

// A few notes have odd/auto-generated titles; give them a human label.
const TITLE_OVERRIDE = {
  '0000000-cgecking-all-carts.md': 'Checking all carts — the cart-polish punch-list',
};

// Numbered files that are really living logs / raw material, not journal observations —
// list them with the working docs so the numbered timeline stays a clean spine.
const FORCE_WORKING = new Set(['0000000-cgecking-all-carts.md']);

// Boilerplate template lines that carry no information — skip them when picking a summary.
const GENERIC = [
  /^this note captures a discovery/i,
  /^this note is a synthesis/i,
  /^it represents our current understanding/i,
  /^future observations may/i,
  /^this document captures/i,
  /^it records our understanding/i,
  /^it represents/i,
  /^later notes may/i,
  /^this note is part of/i,
];
const isGeneric = s => GENERIC.some(re => re.test(s.trim()));

// Canonical lifecycle order + how raw status strings map onto it.
const LIFECYCLE = ['Hypothesis', 'Observed', 'Review', 'Working Theory', 'Incorporated', 'Superseded', 'Unmarked'];
function normStatus(raw) {
  if (!raw) return 'Unmarked';
  const s = raw.trim().toLowerCase();
  if (/hypothesis/.test(s)) return 'Hypothesis';
  if (/working theory/.test(s)) return 'Working Theory';
  if (/observ/.test(s)) return 'Observed';
  if (/review/.test(s)) return 'Review';
  if (/incorporat|accepted|active|shipped|adopted/.test(s)) return 'Incorporated';
  if (/supersed|retired|deprecat/.test(s)) return 'Superseded';
  return 'Unmarked';
}

const STATUS_BADGE = {
  Hypothesis: '🌱', Observed: '🔭', Review: '🔍', 'Working Theory': '🧪',
  Incorporated: '✅', Superseded: '🗄️', Unmarked: '·',
};

// ── parse one note ──────────────────────────────────────────────────────────
function parseNote(file) {
  const raw = fs.readFileSync(path.join(DIR, file), 'utf8');
  const lines = raw.split('\n');
  const head = lines.slice(0, 30);                 // metadata only ever lives at the top

  const m = file.match(/^(\d+)/);
  const num = m ? parseInt(m[1], 10) : null;
  const numbered = m !== null;

  // title: override → frontmatter title: → first # heading (strip "NNN — ")
  let title = TITLE_OVERRIDE[file];
  if (!title) {
    const fmTitle = head.find(l => /^title:\s*\S/.test(l));
    const h1 = lines.find(l => /^#\s+\S/.test(l));
    if (fmTitle && numbered) title = fmTitle.replace(/^title:\s*/, '').trim();
    else if (h1) title = h1.replace(/^#\s+/, '').trim();
    else title = file.replace(/\.md$/, '');
  }
  title = title.replace(/^\d+\s*[—–-]+\s*/, '').trim();   // drop a leading "002 — "

  // status: bold block, "## status:" heading, or bare "Status" label — head only
  let status = null;
  for (let i = 0; i < head.length; i++) {
    const l = head[i];
    let mm;
    if ((mm = l.match(/^##\s*status:\s*(.+)$/i))) { status = mm[1]; break; }
    if ((mm = l.match(/^status:\s*(.+)$/i))) { status = mm[1]; break; }
    if (/^\**status\**\s*$/i.test(l)) {                // label on its own line → value next
      const v = head.slice(i + 1).find(x => x.trim());
      if (v) { status = v.replace(/[*_`]/g, ''); break; }
    }
  }

  // date: bold "Date" block, or first ISO date in the head
  let date = null;
  const dIdx = head.findIndex(l => /^\**date\**\s*$/i.test(l) || /^date:/i.test(l));
  if (dIdx >= 0) {
    const inline = head[dIdx].match(/(\d{4}-\d{2}-\d{2})/);
    date = inline ? inline[1] : (head.slice(dIdx + 1).join(' ').match(/(\d{4}-\d{2}-\d{2})/) || [])[1] || null;
  }
  if (!date) date = (head.join(' ').match(/\b(\d{4}-\d{2}-\d{2})\b/) || [])[1] || null;

  // confidence
  let confidence = null;
  const cIdx = head.findIndex(l => /^\**confidence\**\s*$/i.test(l));
  if (cIdx >= 0) { const v = head.slice(cIdx + 1).find(x => x.trim()); if (v) confidence = v.replace(/[*_`]/g, '').trim(); }

  // summary: frontmatter folded `summary: >`, else first blockquote, else first paragraph
  let summary = null;
  const sIdx = lines.findIndex(l => /^summary:\s*>?\s*$/.test(l) || /^summary:\s*\S/.test(l));
  if (sIdx >= 0 && numbered) {
    const inline = lines[sIdx].replace(/^summary:\s*>?\s*/, '').trim();
    if (inline) summary = inline;
    else {
      const buf = [];
      for (let i = sIdx + 1; i < lines.length; i++) {
        const l = lines[i];
        if (!l.trim()) break;
        if (/^[a-z_]+:/.test(l) || /^[*-]\s/.test(l) || /^#/.test(l)) break;
        buf.push(l.trim());
      }
      summary = buf.join(' ');
    }
  }
  if (!summary) {
    // Prefer the first real body paragraph (under the first "## " section) over the intro
    // blockquote — in this journal the blockquotes are templated fluff and the observation
    // is the real content. Starting at "## " also keeps bold-block metadata values out.
    const secIdx = lines.findIndex(l => /^##\s+\S/.test(l));
    const body = secIdx >= 0 ? lines.slice(secIdx + 1) : lines;
    const p = body.find(l => l.trim() && !/^[#>*\-|]/.test(l) && !/^\**\w+\**\s*$/.test(l) &&
      !/^(title|summary|concepts|status|date|confidence)\s*:/i.test(l) && !/^-{3,}$/.test(l) &&
      !/^\d{4}-\d{2}-\d{2}/.test(l) && !isGeneric(l));
    if (p) summary = p.trim();
  }
  if (!summary) {
    // fallback: a non-generic blockquote (for notes with no "## " section)
    const bq = lines.find(l => /^>\s*\S/.test(l) && !/generated/i.test(l) && !isGeneric(l.replace(/^>\s*/, '')));
    if (bq) summary = bq.replace(/^>\s*/, '').trim();
  }
  if (summary) { summary = summary.replace(/\s+/g, ' '); if (summary.length > 160) summary = summary.slice(0, 157) + '…'; }

  // concepts: frontmatter list
  const concepts = [];
  const cpIdx = head.findIndex(l => /^concepts:\s*$/.test(l));
  if (cpIdx >= 0) {
    for (let i = cpIdx + 1; i < lines.length; i++) {
      const l = lines[i];
      const item = l.match(/^[*-]\s+(.+)$/);
      if (item) concepts.push(item[1].trim());
      else if (l.trim() && !/^\s/.test(l)) break;
    }
  }

  // related: list items under a "## Related notes" heading → note slugs
  const related = [];
  const rIdx = lines.findIndex(l => /^#+\s*related notes/i.test(l));
  if (rIdx >= 0) {
    for (let i = rIdx + 1; i < lines.length; i++) {
      const l = lines[i];
      if (/^#/.test(l)) break;
      const item = l.match(/^[*-]\s+(.+)$/);
      if (item) {
        const slug = item[1].replace(/[`\[\]()]/g, '').trim();
        if (slug && slug !== '...' && slug !== '…') related.push(slug);
      }
    }
  }

  return { file, num, numbered, title, status: normStatus(status), rawStatus: status,
           date, confidence, summary, concepts, related };
}

// ── gather ────────────────────────────────────────────────────────────────
const all = fs.readdirSync(DIR).filter(f => f.endsWith('.md') && !META_FILES.has(f)).sort();
const notes = all.map(parseNote);
const journal = notes.filter(n => n.numbered && !FORCE_WORKING.has(n.file)).sort((a, b) => a.num - b.num);
const working = notes.filter(n => !n.numbered || FORCE_WORKING.has(n.file))
  .sort((a, b) => a.file.localeCompare(b.file));

// ── render ──────────────────────────────────────────────────────────────────
function link(n) { return `[${n.title}](${encodeURI(n.file)})`; }
function numTag(n) { return n.num != null ? `\`${String(n.num).padStart(3, '0')}\`` : ''; }

const L = [];
L.push('# Field Notes — index');
L.push('');
L.push('> **GENERATED** by `tools/build-field-notes.js` — do not hand-edit; re-run after adding or');
L.push('> editing a note (`node tools/build-field-notes.js`). The journal is **append-only** (see');
L.push('> [`README.md`](README.md)): to revise a finding, write a *new* note — never rewrite an old one,');
L.push('> so this index keeps showing the evolution of understanding, not just the latest answer.');
L.push('');
L.push(`*${journal.length} journal notes · ${working.length} working docs · regenerate to refresh.*`);
L.push('');

// Lifecycle board
L.push('## Lifecycle');
L.push('');
L.push('Where each numbered note sits on the path from a hunch to something the repo adopted.');
L.push('');
for (const st of LIFECYCLE) {
  const group = journal.filter(n => n.status === st);
  if (!group.length) continue;
  L.push(`### ${STATUS_BADGE[st]} ${st} (${group.length})`);
  L.push('');
  for (const n of group) {
    L.push(`- ${numTag(n)} ${link(n)}${n.summary ? ` — ${n.summary}` : ''}`);
  }
  L.push('');
}

// Timeline
L.push('## Timeline');
L.push('');
L.push('Numbered notes in order — the spine of the journal.');
L.push('');
const timeline = [...journal].sort((a, b) => {
  if (a.date && b.date && a.date !== b.date) return a.date.localeCompare(b.date);
  return a.num - b.num;
});
for (const n of timeline) {
  L.push(`- ${numTag(n)} ${STATUS_BADGE[n.status]} ${link(n)} *(${n.date || 'undated'})*`);
}
L.push('');

// Related-note graph
const withRel = journal.filter(n => n.related.length);
if (withRel.length) {
  L.push('## Related-note graph');
  L.push('');
  L.push('From each note\'s "Related notes" list — follow a thread of thinking across notes.');
  L.push('');
  for (const n of withRel) {
    L.push(`- ${numTag(n)} **${n.title}** → ${n.related.map(r => `\`${r}\``).join(', ')}`);
  }
  L.push('');
}

// Concepts
const conceptMap = {};
for (const n of journal) for (const c of n.concepts) (conceptMap[c] ||= []).push(n);
const conceptKeys = Object.keys(conceptMap).sort();
if (conceptKeys.length) {
  L.push('## Concepts');
  L.push('');
  for (const c of conceptKeys) {
    L.push(`- **${c}** — ${conceptMap[c].map(n => numTag(n)).join(' ')}`);
  }
  L.push('');
}

// Working material
if (working.length) {
  L.push('## Working material (unnumbered)');
  L.push('');
  L.push('Raw thinking, tool wishlists, handoffs and logs — the soil the numbered notes grow from.');
  L.push('');
  for (const n of working) {
    L.push(`- ${link(n)}${n.summary ? ` — ${n.summary}` : ''}`);
  }
  L.push('');
}

// Conformance
const issues = [];
for (const n of journal) {
  const missing = [];
  if (n.status === 'Unmarked') missing.push('status');
  if (!n.date) missing.push('date');
  if (!n.summary) missing.push('summary');
  if (missing.length) issues.push({ n, missing });
}
if (issues.length) {
  L.push('## Conformance');
  L.push('');
  L.push('Journal notes that don\'t yet match [`standard-header.md`](standard-header.md). Flagged for a');
  L.push('gentle nudge **when next touched** — never a rewrite (append-only).');
  L.push('');
  for (const { n, missing } of issues) {
    L.push(`- ${numTag(n)} ${link(n)} — missing: ${missing.join(', ')}`);
  }
  L.push('');
}

const out = L.join('\n');

if (process.argv.includes('--check')) {
  const cur = fs.existsSync(OUT) ? fs.readFileSync(OUT, 'utf8') : '';
  if (cur !== out) {
    console.error('FIELD-NOTES.md is stale — run `node tools/build-field-notes.js`.');
    process.exit(1);
  }
  console.log('FIELD-NOTES.md is up to date.');
  process.exit(0);
}

fs.writeFileSync(OUT, out);
console.log(`wrote ${path.relative(ROOT, OUT)} — ${journal.length} journal notes, ${working.length} working docs.`);
