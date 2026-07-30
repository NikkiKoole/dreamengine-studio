#!/usr/bin/env node
// status-check.js — the front + back door for `docs/STATUS.md`, the repo's shipped/open/cut LEDGER.
//
//   node tools/status-check.js            FRONT DOOR — the index the file does not have
//   node tools/status-check.js --check    BACK DOOR  — exit 1 on ledger drift (CI gate)
//   node tools/status-check.js --json     machine-readable (for orient/build-context)
//
// WHY THIS EXISTS. CLAUDE.md and HANDOFF.md both tell the reader to trust STATUS.md as "the single status
// ledger" — and it was the one doc in the repo held to no standard: `lint-xrefs.js` listed it in HUBS (its
// links unpoliced) and `lint-docs.js` carves it out as "append-only history". A five-lens audit on
// 2026-07-30 found what that permitted:
//
//   · 20 of 53 numbered items in `## Open` were marked SHIPPED/DONE/FIXED/CLOSED — 38% of the backlog
//   · the section header claims "Ordered by leverage"; the order is arrival date
//   · one item (52) was 226 lines, 12% of the file, and CONTRADICTED the file's own first Shipped entry
//   · numbers that had doubled or tripled while the prose stood still: "8-voice synth" (32), "~125
//     functions" (~370), "263 registered carts" (544), "NET_DELAY=3" (10), `sound.h:5509` (6992)
//   · a `(see Decided-against)` pointer whose subject is not in that section
//
// None of it was catchable by a linter, because none of it is a broken link. It is a ledger losing sync
// with the repo it describes — which is exactly what a tool can check and prose cannot.
//
// THE HANDOFF PRECEDENT. `tools/handoff.js` does this for the lane file: a front door that prints the index
// (so no hand-written one exists to drift) and a `--check` back door that fails on staleness. That pattern
// had three holes of its own, all found by running it; the checks here are deliberately conservative for
// the same reason — every one flags a fact about the FILE, never a judgement about the work.
//
// WHAT IT DELIBERATELY DOES NOT DO: renumber. Every `STATUS #N` reference in the repo (docs, `tune-check.js`,
// `sound.h`, ~10 cart sources — ~30 of them) currently resolves. The numbers are load-bearing: move a DONE
// item to Shipped, but keep its number so the inbound references survive.

const fs = require('fs')
const path = require('path')
const ROOT = path.resolve(__dirname, '..')
const FILE = path.join(ROOT, 'docs', 'STATUS.md')

const argv = process.argv.slice(2)
const wantJson = argv.includes('--json')
const wantCheck = argv.includes('--check')

const raw = fs.readFileSync(FILE, 'utf8')
const lines = raw.split('\n')

// ── parse ────────────────────────────────────────────────────────────────────
const SECTIONS = [
  { key: 'shipped', re: /^## Shipped/ },
  { key: 'open',    re: /^## Open/ },
  { key: 'cut',     re: /^## Decided-against/ },
]
const bounds = {}
SECTIONS.forEach((s, i) => {
  const start = lines.findIndex(l => s.re.test(l))
  bounds[s.key] = { start, end: lines.length }
  if (i > 0) bounds[SECTIONS[i - 1].key].end = start
})

// A Shipped/cut entry is `- **…`; an Open item is `N. **…` (or `39b.`). Entries run to the next entry
// at the same level or the end of the section.
function collect(key, startRe) {
  const { start, end } = bounds[key]
  const out = []
  for (let i = start + 1; i < end; i++) {
    const m = lines[i].match(startRe)
    if (!m) continue
    let j = i + 1
    while (j < end && !startRe.test(lines[j])) j++
    const body = lines.slice(i, j).join('\n')
    out.push({ num: m[1] || null, line: i + 1, len: j - i, title: titleOf(lines[i]), body })
    i = j - 1
  }
  return out
}
const titleOf = (l) => {
  const m = l.match(/\*\*(.+?)\*\*/)
  let t = m ? m[1] : l.replace(/^[-\d.b\s]+/, '')
  return t.replace(/`/g, '').slice(0, 68)
}
const dateOf = (body) => { const m = body.match(/(20\d\d-\d\d-\d\d)/); return m ? m[1] : null }

const shipped = collect('shipped', /^- \*\*(?:)/)
const open    = collect('open',    /^(\d+b?)\.\s/)
const cut     = collect('cut',     /^- \*\*(?:)/)

const DONE_RE = /\b(SHIPPED|✅ DONE|✓ SHIPPED|✓ DONE|FIXED|CLOSED|RESOLVED)\b/
// a `~~struck~~` heading is the file's own idiom for "this shipped"
const struck = (e) => /^\s*\d+b?\.\s*~~/.test(e.body.split('\n')[0])

// ── checks ───────────────────────────────────────────────────────────────────
const LEN_BUDGET = 25          // an entry past this is a write-up, not a ledger row
const HEADLINE_BUDGET = 900    // chars on the `_Last updated:_ ` line
const problems = []
const P = (kind, line, msg) => problems.push({ kind, line, msg })

// 1. the highest-value check: a DONE marker inside `## Open`
const doneInOpen = open.filter(e => DONE_RE.test(e.title) || struck(e) ||
  DONE_RE.test(e.body.split('\n').slice(0, 3).join(' ')))
for (const e of doneInOpen)
  P('done-in-open', e.line, `item ${e.num} reads as shipped/fixed — move it to "Shipped ✓" (KEEP its number: ~30 "STATUS #N" refs resolve today): ${e.title}`)

// 2. an entry with no date at all
for (const e of [...shipped, ...cut]) if (!dateOf(e.body))
  P('undated', e.line, `entry has no (YYYY-MM-DD): ${e.title}`)

// 3. an over-long entry — rationale belongs in the owning design doc
for (const e of [...shipped, ...open]) if (e.len > LEN_BUDGET)
  P('too-long', e.line, `${e.len} lines (budget ${LEN_BUDGET}) — move the rationale to the linked design doc: ${e.title}`)

// 4. the headline line
const hl = lines.findIndex(l => l.startsWith('_Last updated:'))
if (hl >= 0 && lines[hl].length > HEADLINE_BUDGET)
  P('headline', hl + 1, `_Last updated:_ is ${lines[hl].length} chars (budget ${HEADLINE_BUDGET}) — one date + a sentence + a link; the detail is already in the entry below`)

// 5. Shipped out of reverse-chronological order
{
  const dated = shipped.map(e => ({ ...e, d: dateOf(e.body) })).filter(e => e.d)
  for (let i = 1; i < dated.length; i++)
    if (dated[i].d > dated[i - 1].d)
      P('unsorted', dated[i].line, `Shipped is reverse-chronological, but ${dated[i].d} follows ${dated[i - 1].d}: ${dated[i].title}`)
}

// 6. numbering inversions (do NOT renumber to fix — reorder the entries)
{
  const seq = open.filter(e => /^\d+$/.test(e.num)).map(e => ({ n: +e.num, line: e.line, t: e.title }))
  for (let i = 1; i < seq.length; i++)
    if (seq[i].n < seq[i - 1].n)
      P('numbering', seq[i].line, `item ${seq[i].n} appears after item ${seq[i - 1].n} — "go to item ${seq[i].n}" fails. Reorder the entries; do NOT renumber`)
}

// 7. a "(see Decided-against)" pointer whose subject is not in that section
{
  const cutText = lines.slice(bounds.cut.start, bounds.cut.end).join('\n')
  for (let i = bounds.open.start; i < bounds.open.end; i++) {
    if (!/see Decided-against/i.test(lines[i])) continue
    const subjects = [...lines[i].matchAll(/`([a-z_][a-z0-9_]*)\(\)`/gi)].map(m => m[1])
    const missing = subjects.filter(s => !cutText.includes(s))
    if (subjects.length && missing.length === subjects.length)
      P('dead-pointer', i + 1, `"see Decided-against" but ${missing.map(s => s + '()').join(', ')} is not in that section`)
  }
}

// ── output ───────────────────────────────────────────────────────────────────
if (wantJson) {
  console.log(JSON.stringify({ counts: { shipped: shipped.length, open: open.length, cut: cut.length,
    openReal: open.length - doneInOpen.length }, problems,
    shipped: shipped.map(e => ({ line: e.line, date: dateOf(e.body), len: e.len, title: e.title })),
    open: open.map(e => ({ num: e.num, line: e.line, len: e.len, title: e.title, done: doneInOpen.includes(e) })) }, null, 2))
  process.exit(wantCheck && problems.length ? 1 : 0)
}

const b = s => `\x1b[1m${s}\x1b[0m`, dim = s => `\x1b[2m${s}\x1b[0m`, warn = s => `\x1b[33m${s}\x1b[0m`

if (!wantCheck) {
  console.log(b('STATUS LEDGER') + dim('  (docs/STATUS.md — the shipped/open/cut record)'))
  console.log(`  shipped ${shipped.length} · open ${open.length - doneInOpen.length} real of ${open.length} listed · cut ${cut.length}\n`)
  console.log(b('  SHIPPED, newest first'))
  for (const e of shipped.slice(0, 12))
    console.log(`    ${dim('L' + String(e.line).padEnd(5))}${(dateOf(e.body) || '        ').padEnd(11)}${e.title}`)
  if (shipped.length > 12) console.log(dim(`    … ${shipped.length - 12} more`))
  console.log('\n' + b('  OPEN') + dim('  (numbers are load-bearing — ~30 "STATUS #N" refs resolve; never renumber)'))
  for (const e of open) {
    if (doneInOpen.includes(e)) continue
    console.log(`    ${dim('L' + String(e.line).padEnd(5))}#${String(e.num).padEnd(4)}${e.title}`)
  }
  if (doneInOpen.length) console.log(warn(`\n  ⚠ ${doneInOpen.length} listed-open item(s) read as shipped — run --check`))
  console.log(dim('\n  → node tools/status-check.js --check   for ledger drift'))
  process.exit(0)
}

// --check
if (!problems.length) {
  console.log(`status-check: ledger ok — ${shipped.length} shipped · ${open.length} open · ${cut.length} cut`)
  process.exit(0)
}
const order = ['done-in-open', 'dead-pointer', 'numbering', 'headline', 'too-long', 'undated', 'unsorted']
// repo-doctor shows the LAST stdout line as this check's one-line summary, so that line must be the
// count — a trailing explanatory note would be displayed instead of the result.
problems.sort((x, y) => order.indexOf(x.kind) - order.indexOf(y.kind) || x.line - y.line)
console.log(b(`STATUS LEDGER DRIFT — ${problems.length} finding(s)`))
let last = null
for (const p of problems) {
  if (p.kind !== last) { console.log('\n' + b(`  ${p.kind}`)); last = p.kind }
  console.log(`    L${String(p.line).padEnd(5)} ${p.msg}`)
}
console.log(dim('\n  none of these is a broken link — that is why lint-docs/lint-xrefs cannot see them.'))
const byKind = order.map(k => [k, problems.filter(p => p.kind === k).length]).filter(([, n]) => n)
console.log(byKind.map(([k, n]) => `${n} ${k}`).join(' · '))
process.exit(1)
