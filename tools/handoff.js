#!/usr/bin/env node
// handoff.js — the ACTIVE-LANE tracker for docs/HANDOFF.md. Keeps the handoff RECENT and the
// reliable place to resume complex in-flight work, via the two-door pattern from
// docs/design/driftable-docs.md (front door primes going in, back door catches what slipped):
//
//   node tools/handoff.js          # FRONT DOOR — list the active ▶ lanes + age (wired into orient)
//   node tools/handoff.js --check  # BACK DOOR  — flag lanes >2wk old, a lane whose BODY WAS EDITED
//                                  #   AFTER its date, a MISSING or UNANCHORED
//                                  #   Resume-at, a broken doc link, or a
//                                    broken #anchor (a Resume-at pointing at a section that no
//                                    longer exists — the precise "the doc moved under the pointer"
//                                    drift; write Resume-ats as `[text](doc.md#section)` so it bites)
//   node tools/handoff.js --json   # machine-readable
//
// A "lane" is a `> **▶ ACTIVE THREAD (YYYY-MM-DD) — <title>.**` callout in docs/HANDOFF.md. The
// rule (stated in HANDOFF.md's header): refresh a lane's date when you touch it, prune it when it
// ships (its detail lives in STATUS.md + the doc's pick-up point). This tool makes a forgotten
// stale lane SURFACE instead of rotting silently — the exact failure that bit the old handoff.
//
// ⚠ THE AGE CHECK IS BLIND TO THE COMMONEST DRIFT, which is why `edited` exists. Age asks "has
// anyone touched this lane lately", and a lane nobody touches goes stale loudly. But the failure
// that actually bit (2026-08-17) is the OPPOSITE: two commits wrote a whole app submission into a
// lane's body and neither bumped its header date, so the front door aged today's work at 2d and
// the back door said nothing — the lane read FRESHER than the thing it described, and a reader
// choosing what to resume was told the newest work was two days cold. So the second question is
// "is this lane's date at least as new as its BODY", answered from `git blame` over the lane's
// line range (one blame for the whole file, bucketed per lane; uncommitted lines count as today,
// deliberately, so it fires while you can still fix it rather than after you commit).
// DE_HANDOFF_EDITED = {"<title substring>":"YYYY-MM-DD"} injects those dates for --selfcheck,
// which runs against a fixture in a temp dir that git cannot blame.
// ⚠ Its known false positive: a MASS edit (a reflow, a file-wide audit) touches many lane bodies
// without any of them being new work, and every one then reads as edited-after-its-date. That is
// why this is advisory and why the finding names the edit DATE — you can recognise a sweep by
// seeing the same date across unrelated lanes. The response is the same either way: bump if the
// lane is live, prune if it shipped. Do NOT bump a date merely to silence this; a lane that is
// genuinely stale should keep saying so.
'use strict'

const fs = require('fs')
const path = require('path')
const ROOT = path.resolve(__dirname, '..')
const DOCS = path.join(ROOT, 'docs')
// DE_HANDOFF_FILE aims the parse at a fixture instead of docs/HANDOFF.md — used by --selfcheck.
// DOCS stays the real docs/ so link + anchor targets resolve exactly as in production.
const HANDOFF = process.env.DE_HANDOFF_FILE || path.join(DOCS, 'HANDOFF.md')
const STALE_DAYS = 14

const argv = process.argv.slice(2)
const asJson = argv.includes('--json')
const check = argv.includes('--check')

// ── --selfcheck: assert the CHECKER against known answers ────────────────────
// See docs/guides/checks-and-oracles.md "Self-test the checker". Every judgement here is a
// heuristic over hand-written prose, and this tool had THREE false positives on the day it was
// tightened — each is a regression guard in the fixture now. The fixture TEMPLATES its dates
// (__TODAY__ / __ANCIENT__) so a "fresh lane" expectation cannot rot into a stale one tomorrow.
if (process.argv.slice(2).includes('--selfcheck')) {
  const os = require('os')
  const src = path.join(__dirname, 'fixtures', 'handoff', 'HANDOFF.md')
  const iso = (d) => new Date(d).toISOString().slice(0, 10)
  const today = iso(Date.now())
  const recent = iso(Date.now() - 5 * 86400000)     // inside the 14-day bar ON PURPOSE, see below
  const ancient = iso(Date.now() - 90 * 86400000)
  const tmp = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'de-handoff-')), 'HANDOFF.md')
  fs.writeFileSync(tmp, fs.readFileSync(src, 'utf8')
    .replace(/__TODAY__/g, today).replace(/__RECENT__/g, recent).replace(/__ANCIENT__/g, ancient))

  // git cannot blame a temp file, so inject the "last edited" answers the fixture is asserting
  // against. BOTH DIRECTIONS are injected: one lane edited after its date (must report) and one
  // edited before it (must NOT) — without the second, a check that flagged every lane carrying
  // any edit history at all would pass this fixture.
  const edited = JSON.stringify({ 'edited after its date': today, 'edited before its date': ancient })

  let raw
  try {
    raw = require('child_process').execFileSync(process.execPath, [__filename, '--json'],
      { env: { ...process.env, DE_HANDOFF_FILE: tmp, DE_HANDOFF_EDITED: edited }, encoding: 'utf8', maxBuffer: 1 << 26 })
  } catch (e) { raw = e.stdout }
  const g = JSON.parse(raw)
  const lane = (frag) => g.lanes.find(l => l.title.toLowerCase().includes(frag.toLowerCase()))
  const clean = (l) => l && !l.broken.length && !l.brokenAnchors.length && !l.noResume &&
                       !l.unanchored && !l.editedAfter && !(l.age != null && l.age > g.staleDays)
  const T = []
  const t = (n, ok) => T.push({ n, ok })

  t('all 12 lanes parsed', g.lanes.length === 12)
  t('a clean lane → no finding', clean(lane('a clean lane')))
  t('an old lane → stale', (lane('a stale lane')?.age ?? 0) > g.staleDays)
  t('a broken doc link → reported', lane('broken doc link')?.broken.length === 1)
  t('a broken #anchor → reported', lane('broken section anchor')?.brokenAnchors.length === 1)
  t('no Resume-at at all → reported', lane('no pick-up point')?.noResume === true)
  t('Resume-at with no anchor → unanchored', lane('has no anchor')?.unanchored === true)
  t('anchor a few lines BELOW the label → clean  [regression guard]', clean(lane('below the label')))
  t('label mid-bold → clean  [regression guard]', clean(lane('label mid-bold')))
  t('a date qualified with prose still parses  [regression guard]', !!lane('qualified date'))
  t('a drifted lowercase spelling → clean  [regression guard]', clean(lane('lowercase spelling')))
  t('body edited AFTER its date → reported', !!lane('edited after its date')?.editedAfter)
  t('…and that lane is NOT flagged by age  [the age check is blind to it]',
    (lane('edited after its date')?.age ?? 99) <= g.staleDays)
  t('body edited BEFORE its date → clean  [negative control]', clean(lane('edited before its date')))

  const bad = T.filter(x => !x.ok)
  for (const x of T) console.log(`  ${x.ok ? '\x1b[32m✓\x1b[0m' : '\x1b[31m✗\x1b[0m'} ${x.n}`)
  console.log(bad.length
    ? `\x1b[31mhandoff --selfcheck FAILED\x1b[0m — ${bad.length} of ${T.length} expectations broken`
    : `handoff --selfcheck: ${T.length}/${T.length} known answers correct`)
  process.exit(bad.length ? 1 : 0)
}

if (!fs.existsSync(HANDOFF)) { console.error('no docs/HANDOFF.md'); process.exit(2) }
const lines = fs.readFileSync(HANDOFF, 'utf8').split('\n')

const today = new Date()   // real date is fine in a plain tool (unlike a workflow script)
const ageOf = d => { const t = Date.parse(d + 'T00:00:00Z'); return Number.isFinite(t) ? Math.floor((today - t) / 86400000) : null }

// GitHub-flavoured heading slug, then collapsed to a canonical form so the anchor check tolerates
// single-vs-double-hyphen renderer differences (— removal etc.) — we're detecting "the section is
// GONE", not policing exact punctuation. Same normalize is applied to both sides.
const canon = s => s.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '')
// …and the GITHUB form, which DROPS punctuation instead of hyphenating it. The two disagree exactly where
// a heading contains an apostrophe or a dotted number: "What's still open" → canon `what-s-…` but GitHub
// `whats-…`; "2.3(a) …" → canon `2-3-a-…` but GitHub `23a-…`. Whichever one you write, the other looks
// broken, and a link that renders correctly on GitHub was being reported as a dead anchor. Accept BOTH
// (additive — it cannot make a currently-passing link fail). Found 2026-07-30 by a handoff audit.
const ghSlug = s => s.toLowerCase().replace(/[^\w\s-]/g, '').trim().replace(/\s+/g, '-')
const anchorCache = new Map()
function docAnchors(relPath) {          // relPath = 'design/foo.md' (no #), DOCS-relative
  if (anchorCache.has(relPath)) return anchorCache.get(relPath)
  const abs = path.join(DOCS, relPath)
  let set = new Set()
  try {
    for (const ln of fs.readFileSync(abs, 'utf8').split('\n')) {
      const h = ln.match(/^#{1,6}\s+(.+?)\s*#*\s*$/)
      if (h) { set.add(canon(h[1])); set.add(ghSlug(h[1])) }
    }
  } catch { set = null }                 // unreadable → caller already flags the file as broken
  anchorCache.set(relPath, set)
  return set
}

// ── when was each LINE of the handoff last edited? ───────────────────────────
// One `git blame --line-porcelain` for the whole file, bucketed per lane below. Returns an array
// indexed by 0-based line → 'YYYY-MM-DD', or null when git cannot answer (no repo, a fixture in a
// temp dir, git missing). null is NOT a finding: an unanswerable question must stay silent rather
// than flag every lane, since this check's failure mode is a false positive on every single one.
// Uncommitted lines blame as all-zero hash with the CURRENT time, which is what we want — an
// unbumped date is caught while it is still a working-tree edit.
function blameDates () {
  const inj = process.env.DE_HANDOFF_EDITED
  if (inj) return { injected: JSON.parse(inj) }
  try {
    const out = require('child_process').execFileSync(
      'git', ['blame', '--line-porcelain', '--', HANDOFF],
      { cwd: ROOT, encoding: 'utf8', maxBuffer: 1 << 28, stdio: ['ignore', 'pipe', 'ignore'] })
    const dates = []
    for (const ln of out.split('\n')) {
      const m = ln.match(/^committer-time (\d+)$/)
      if (m) dates.push(new Date(Number(m[1]) * 1000).toISOString().slice(0, 10))
    }
    return dates.length ? { dates } : null
  } catch { return null }
}
const BLAME = blameDates()

// the newest edit anywhere in [from,to) — 0-based, `to` exclusive
function editedIn (from, to, title) {
  if (!BLAME) return null
  if (BLAME.injected) {
    const hit = Object.entries(BLAME.injected).find(([k]) => title.toLowerCase().includes(k.toLowerCase()))
    return hit ? hit[1] : null
  }
  let newest = null
  for (let k = from; k < to && k < BLAME.dates.length; k++) {
    if (!newest || BLAME.dates[k] > newest) newest = BLAME.dates[k]
  }
  return newest
}

const lanes = []
for (let i = 0; i < lines.length; i++) {
  // ⚠ THE DATE IS MATCHED LENIENTLY ON PURPOSE. This used to require `(YYYY-MM-DD)` exactly, so the
  // harmony-brain lane — dated `(2026-07-20, later the same day)` — parsed as NO LANE AT ALL: invisible to
  // the front door, uncounted, and permanently exempt from the staleness check whose entire job is to make
  // a forgotten lane surface instead of rotting. A hand-written note qualifying the date is a reasonable
  // thing for a human to write, so accept it and keep the date.  (found 2026-07-30 by a handoff audit)
  const m = lines[i].match(/^>\s*\*\*▶\s*ACTIVE THREAD\s*\((\d{4}-\d{2}-\d{2})[^)]*\)\s*[—-]+\s*(.+?)\.?\*\*/)
  if (!m) continue
  let j = i, block = []
  while (j < lines.length && /^\s*>/.test(lines[j])) { block.push(lines[j]); j++ }
  const body = block.join('\n')
  // capture the full link target (file + optional #anchor), de-duped
  const targets = [...new Set([...body.matchAll(/\]\(([^)\s]+\.md(?:#[^)\s]*)?)[^)]*\)/g)].map(x => x[1]))]
  const broken = [], brokenAnchors = []
  for (const t of targets) {
    const [file, anchor] = t.split('#')
    if (!fs.existsSync(path.join(DOCS, file))) { broken.push(file); continue }
    if (anchor) {
      const anchors = docAnchors(file)
      const a = decodeURIComponent(anchor)
      if (anchors && !anchors.has(canon(a)) && !anchors.has(ghSlug(a))) brokenAnchors.push(`${file}#${anchor}`)
    }
  }
  // ⚠ A Resume-at with NO #anchor made the anchor check INERT for that lane — it reported clean because
  // it had nothing to check. ~a third of the lanes had drifted to a bare doc link or plain prose, so the
  // back door was guarding far less than it appeared to. Flag the absence, not just the breakage.
  // Accept any of the drifted spellings when looking for one (Resume-at / Resume at / resume at).
  // Scope the anchor search from the Resume-at line to the END OF THE LANE, not just that one line: a
  // multi-line Resume-at (a numbered queue, say) legitimately carries its anchor a few lines below the
  // label, and requiring it on the label line reported those as unanchored.
  // Match the label ANYWHERE in the line, not only right after `**`: lanes legitimately write it mid-bold
  // ("**Status + what's-left — Resume at**"), and requiring the prefix reported those as having none. The
  // drifted spellings (Resume-at / Resume at / resume at) are all accepted on purpose — normalising the
  // prose is a doc job, and a checker that only sees one spelling is worse than useless.
  const rIdx = block.findIndex(l => /Resume[- ]at/i.test(l))
  const noResume = rIdx < 0
  const unanchored = rIdx >= 0 &&
    !/\]\([^)\s]+\.md#[^)\s]*\)/.test(block.slice(rIdx).join('\n'))
  const title = m[2].trim()
  // Compare STRINGS, not parsed dates: both sides are ISO 'YYYY-MM-DD', which sorts correctly as
  // text, and this way a lane dated the same day as its last edit is equal (not "one is 3 hours
  // newer") — the check is about the DAY somebody wrote, not about clock skew inside it.
  // ⚠ i + 1, NOT i: the header line holds the DATE, so including it means bumping the date is
  // itself an edit that makes the body look newer than the date you just set. The check then
  // re-reports the lane you just fixed, forever. Same reason a rebase (which re-authors every line
  // it touches) must not be able to flag a lane through its header alone. The BODY is i+1..j.
  const edited = editedIn(i + 1, j, title)
  const editedAfter = edited && edited > m[1] ? edited : null
  lanes.push({ date: m[1], title, age: ageOf(m[1]), links: targets, broken, brokenAnchors,
               noResume, unanchored, edited, editedAfter, line: i + 1 })
  i = j
}

if (asJson) { console.log(JSON.stringify({ lanes, staleDays: STALE_DAYS }, null, 2)); process.exit(0) }

const tty = process.stdout.isTTY
const b = s => tty ? `\x1b[1m${s}\x1b[0m` : s
const dim = s => tty ? `\x1b[2m${s}\x1b[0m` : s
const warn = s => tty ? `\x1b[33m${s}\x1b[0m` : s
const isStale = l => (l.age != null && l.age > STALE_DAYS) || l.broken.length > 0 || l.brokenAnchors.length > 0 || l.noResume || l.unanchored || !!l.editedAfter

if (!lanes.length) {
  console.log('no active lanes in docs/HANDOFF.md — add a `▶ ACTIVE THREAD (date) — title.` callout when you start complex work')
  process.exit(0)
}

if (check) {
  const bad = lanes.filter(isStale)
  if (!bad.length) { console.log(`handoff: ${lanes.length} active lane(s), all fresh (≤${STALE_DAYS}d, links resolve)`); process.exit(0) }
  console.log(b(`HANDOFF LANES (advisory) — refresh the date, or prune if shipped (→ STATUS.md):`))
  for (const l of bad) {
    const why = [
      l.age != null && l.age > STALE_DAYS ? `${l.age}d stale` : null,
      l.broken.length ? `broken link: ${l.broken.join(', ')}` : null,
      l.brokenAnchors.length ? `broken #section: ${l.brokenAnchors.join(', ')}` : null,
      l.noResume   ? 'NO Resume-at (a lane with no pick-up point cannot be resumed)' : null,
      l.unanchored ? 'Resume-at has no #anchor (so the anchor check is INERT for this lane)' : null,
      l.editedAfter ? `body edited ${l.editedAfter}, AFTER its ${l.date} date (bump it — the lane reads fresher than the work)` : null,
    ].filter(Boolean).join(' · ')
    console.log(`  ${warn('⚠')} ${l.title}  ${dim('(' + l.date + ')')}  ${why}`)
  }
  process.exit(1)
}

// front door — list active lanes + age
console.log(b('ACTIVE LANES') + dim('  (docs/HANDOFF.md — resume complex work here)'))
for (const l of lanes) {
  const age = l.age == null ? '?' : l.age === 0 ? 'today' : `${l.age}d`
  const flag = l.broken.length ? ' · broken link' : l.brokenAnchors.length ? ' · broken #section'
             : l.noResume ? ' · no Resume-at' : l.unanchored ? ' · Resume-at unanchored'
             : l.editedAfter ? ` · edited ${l.editedAfter}, date not bumped` : ' stale'
  const tag = isStale(l) ? warn(`⚠ ${age}${flag}`) : dim(age)
  // print the LINE so the front door IS the index — a hand-maintained one drifted for 3 lanes / 4 days
  console.log(`  · ${dim('L' + String(l.line).padEnd(4))} ${l.title}  ${tag}`)
}
console.log(dim('  → open docs/HANDOFF.md for the Resume-at pointers'))
