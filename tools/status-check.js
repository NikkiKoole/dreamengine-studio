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
//   · 20 of 53 numbered items in `## Open` were shipped — the backlog advertised ~38% more work
//     than existed. Nine of them ALSO left a real open remainder, buried under the write-up.
//   · the `_Last updated:_` line had reached 9,064 chars and was the ONLY record of three shipped
//     things (`FILTER_DIODE`, `filter-spec.js`, `rebirth-classic.md`) — a headline used as an entry
//   · numbers that doubled or tripled while the prose stood still: "8-voice synth" (32), "~125
//     functions" (373), "263 registered carts" (545), `sound.h:5509` (a file now 8,800 lines long)
//   · a `(see Decided-against)` pointer whose target had never been written
//   · `## Shipped` is a reverse-chronological changelog and a flat capability inventory, interleaved
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

// ── per-line age, from git blame ─────────────────────────────────────────────
// WHY THIS IS CONTEXT AND NOT A CHECK. On 2026-07-30 item 25 claimed ui.h's on-device touch pass was
// still outstanding. It had been answered 52 days earlier — in `touch-notes.md`, the day BEFORE ui.h
// v1 shipped — and the item sat untouched the whole time. Nothing could catch that: "Still open: the
// on-device probe run" is structurally identical to a genuinely open item, so `lint-docs` (links),
// `lint-xrefs` (backlinks) and `stale-doc-check` (does a NAMED thing still exist) are all blind by
// construction. `stale-doc-check` is doubly blind here because it keys on the FILE's mtime, and
// STATUS.md is the most-edited doc in the repo — it never looks stale while individual lines rot.
//
// Age is the only mechanical signal, and it is far too weak to gate on: 34 of the 53 open items were
// untouched for 45+ days, and most of those are parked on purpose. So it is printed as CONTEXT in the
// front door — a reader gets the prior "this line is five months old, verify before trusting it" —
// and never as a finding. Flagging 64% of a backlog trains people to ignore the tool.
const lineAge = (() => {
  try {
    const out = require('child_process')
      .execFileSync('git', ['blame', '--date=short', '-l', '--', 'docs/STATUS.md'],
                    { cwd: ROOT, encoding: 'utf8', maxBuffer: 1 << 26 })
      .split('\n')
    const today = Date.now()
    return out.map(l => {
      const m = l.match(/(\d{4}-\d{2}-\d{2})/)
      if (!m) return null
      return Math.round((today - Date.parse(m[1])) / 86400000)
    })
  } catch { return null }   // no git / shallow clone / dirty — age is a nicety, never required
})()
// youngest line in the entry: how recently ANY of it was revisited
const ageOf = (e) => {
  if (!lineAge) return null
  const a = lineAge.slice(e.line - 1, e.line - 1 + e.len).filter(n => n != null)
  return a.length ? Math.min(...a) : null
}

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
    // An entry ends at the next entry OR at any sub-heading — otherwise the last bullet before a
    // `###` swallows the whole rest of the section and reads as a 43-line monster.
    let j = i + 1
    while (j < end && !startRe.test(lines[j]) && !/^#{2,4} /.test(lines[j])) j++
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
// LENGTH. The first version of this check used a flat 25-line budget, picked before reading the file,
// and it flagged 21 entries. Once the genuine monsters were dealt with (item 52 at 229 lines restating
// a 4,800-line pair of design docs, item 42 at 106 lines of ✅-marked shipped tools, item 31 at 41
// lines of struck root-cause narratives that audio-notes §18 owns), the distribution was: 88 entries,
// median 9, p75 17, p90 28, max 56 — and EVERY remaining entry over 25 lines already linked its owning
// design doc. So length by itself had stopped being a defect signal, and a flat 25 was just telling
// good multi-week write-ups they were too long.
//
// What actually goes wrong is length with NOWHERE ELSE FOR IT TO LIVE — a write-up whose rationale has
// no owning doc, so the ledger has silently become the design record. That is mechanically checkable,
// and it is the real rule now. The flat cap stays as a backstop for genuine monsters.
const LEN_BUDGET = 60          // hard backstop: past this it is a write-up whatever it links
const LEN_SOFT = 25            // past this you must at least link the doc that owns the rationale
const DOC_LINK_RE = /\]\((?:design|guides|decisions|field-notes)\//
const HEADLINE_BUDGET = 900    // chars on the `_Last updated:_ ` line
const problems = []
const P = (kind, line, msg) => problems.push({ kind, line, msg })

// 1. the highest-value check: a DONE marker inside `## Open`.
//
// Scan the first THREE lines of the item, then subtract two context shapes. Both halves of that were
// learned by reading all 22 hits by hand instead of trusting the first version:
//
//   · THREE lines, not the bold title. The file often puts the marker just outside the bold
//     ("**Rasterization consistency** *(SHIPPED — …)*", "**Unify LFO shape** — ✓ **SHIPPED**") or on
//     the next line ("**Sound expansion** — _… now SHIPPED_"). Matching only the title missed five.
//   · MINUS the context shapes, or it cries wolf. Item 1 says "AABB collision already SHIPPED as
//     boxes_touch()" while its own work (teaching discoverability, an `explode()` design) is wide
//     open; item 40 is "spatial audio **v3**" opening "v1 + v2 SHIPPED". Both are open items that
//     MENTION a shipped thing — the inverse of the finding.
//
// And it reports MOVE vs SPLIT, because half of these shipped-but-still-listed items left a real
// remainder (ui.h's on-device probe run, upright.c's up-only bend) that a blind move would bury.
const TAIL_RE = /\*\*(Still open|Deferred|Further deferred|Remaining|PARKED)/i
const CONTEXT_RE = /already\s+(?:\*\*)?(?:SHIPPED|DONE)|v\d[^\n]*\+\s*v\d[^\n]*(?:SHIPPED|DONE)/i
const doneInOpen = open.filter(e => {
  const head = e.body.split('\n').slice(0, 3).join(' ')
  return DONE_RE.test(head.replace(CONTEXT_RE, '')) || struck(e)
})
for (const e of doneInOpen) {
  const tail = TAIL_RE.test(e.body)
  P('done-in-open', e.line, tail
    ? `item ${e.num} is SHIPPED but still carries an open tail — SPLIT it: leave a short open item with just the tail, move the rest to "Shipped ✓". ${e.title}`
    : `item ${e.num} reads as shipped/fixed with no open tail — move it to "Shipped ✓". ${e.title}`)
}
if (doneInOpen.length)
  P('done-in-open', bounds.open.start + 1, `⚠ KEEP EVERY NUMBER when you move these — ~30 "STATUS #N" refs across docs, tune-check.js, sound.h and ~10 carts resolve today. Record the number in the destination entry; never reuse or renumber.`)

// 2. an entry with no date at all — but ONLY in the changelog half of `## Shipped`.
//
// `## Shipped` is really TWO documents. It opens as a reverse-chronological CHANGELOG of dated
// entries, then at the first bolded group heading (`**Tooling & environment**`, `**API surface**`,
// `**Code-first sound**`) it becomes a flat CAPABILITY INVENTORY — "what the engine has today".
// Inventory bullets are things, not events: "Live inspection", "Profiler", "Font system:", `sget()`.
// Demanding a date from those is the check being wrong, not the file, and it produced 20 of the
// original 28 undated findings. A date only means something where the entry records an event.
const invStart = (() => {
  for (let i = bounds.shipped.start + 1; i < bounds.shipped.end; i++)
    if (/^\*\*[A-Z]/.test(lines[i])) return i + 1
  return bounds.shipped.end
})()
const shippedLog = shipped.filter(e => e.line < invStart)
const shippedInv = shipped.filter(e => e.line >= invStart)

for (const e of [...shippedLog, ...cut]) if (!dateOf(e.body))
  P('undated', e.line, `entry has no (YYYY-MM-DD): ${e.title}`)

// 2b. the structural consequence of that split: dated CHANGELOG entries appended into the middle of
// the inventory. One finding, not N — the fix is one decision (move them up), not N edits.
{
  const strays = shippedInv.filter(e => dateOf(e.body))
  if (strays.length)
    P('log-in-inventory', invStart, `${strays.length} dated entries sit INSIDE the capability inventory (below line ${invStart}) rather than in the changelog above it, so "## Shipped" is a changelog and an inventory interleaved. ⚠ This one is a DESIGN DECISION, not a mechanical move: the inventory groups by THEME and the changelog sorts by DATE, so flattening these into date order would scatter a themed run (the netplay rungs are the case in point — and note they are ALREADY fragmented, rung 1 sitting apart from rungs 2-5a which are themselves in reverse order). Pick one: promote them to the changelog, or gather each theme into a named sub-group that is explicitly NOT date-sorted. Lines: ${strays.slice(0, 12).map(e => e.line).join(', ')}${strays.length > 12 ? ', …' : ''}`)
}

// 3. an over-long entry (see the LENGTH note above for why this is not a flat budget)
for (const e of [...shipped, ...open]) {
  if (e.len > LEN_BUDGET)
    P('too-long', e.line, `${e.len} lines — past the ${LEN_BUDGET}-line backstop. Even with a design doc linked, this is a write-up living in a ledger: ${e.title}`)
  else if (e.len > LEN_SOFT && !DOC_LINK_RE.test(e.body))
    P('too-long', e.line, `${e.len} lines with NO link to an owning design doc — the ledger has become the design record for this. Give it a home in docs/design (or an ADR) and leave a pointer: ${e.title}`)
}

// 4. the headline line
const hl = lines.findIndex(l => l.startsWith('_Last updated:'))
if (hl >= 0 && lines[hl].length > HEADLINE_BUDGET)
  P('headline', hl + 1, `_Last updated:_ is ${lines[hl].length} chars (budget ${HEADLINE_BUDGET}) — one date + a sentence + a link; the detail is already in the entry below`)

// 5. Shipped out of reverse-chronological order — changelog half only (see #2: the inventory's
// dates are interleaved by construction, so ordering them is meaningless until 2b is resolved).
{
  const dated = shippedLog.map(e => ({ ...e, d: dateOf(e.body) })).filter(e => e.d)
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
    const a = ageOf(e)
    const tag = a == null ? '      ' : a >= 45 ? warn(String(a) + 'd').padEnd(15) : dim(String(a) + 'd').padEnd(14)
    console.log(`    ${dim('L' + String(e.line).padEnd(5))}${tag}#${String(e.num).padEnd(4)}${e.title}`)
  }
  if (lineAge) console.log(dim('\n  age = days since any line of the item last changed. An old open item is not a bug,'
    + '\n  but it is the prior for "verify this is still true" — item 25 claimed an on-device pass was'
    + '\n  outstanding for 52 days after touch-notes had answered it. Not gateable: 34 of 53 items were'
    + '\n  45d+ stale and most were parked on purpose.'))
  if (doneInOpen.length) console.log(warn(`\n  ⚠ ${doneInOpen.length} listed-open item(s) read as shipped — run --check`))
  console.log(dim('\n  → node tools/status-check.js --check   for ledger drift'))
  process.exit(0)
}

// --check
if (!problems.length) {
  console.log(`status-check: ledger ok — ${shipped.length} shipped · ${open.length} open · ${cut.length} cut`)
  process.exit(0)
}
const order = ['done-in-open', 'dead-pointer', 'numbering', 'headline', 'log-in-inventory', 'too-long', 'undated', 'unsorted']
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
