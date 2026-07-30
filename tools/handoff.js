#!/usr/bin/env node
// handoff.js — the ACTIVE-LANE tracker for docs/HANDOFF.md. Keeps the handoff RECENT and the
// reliable place to resume complex in-flight work, via the two-door pattern from
// docs/design/driftable-docs.md (front door primes going in, back door catches what slipped):
//
//   node tools/handoff.js          # FRONT DOOR — list the active ▶ lanes + age (wired into orient)
//   node tools/handoff.js --check  # BACK DOOR  — flag lanes >2wk old, a MISSING or UNANCHORED
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
'use strict'

const fs = require('fs')
const path = require('path')
const ROOT = path.resolve(__dirname, '..')
const DOCS = path.join(ROOT, 'docs')
const HANDOFF = path.join(DOCS, 'HANDOFF.md')
const STALE_DAYS = 14

const argv = process.argv.slice(2)
const asJson = argv.includes('--json')
const check = argv.includes('--check')

if (!fs.existsSync(HANDOFF)) { console.error('no docs/HANDOFF.md'); process.exit(2) }
const lines = fs.readFileSync(HANDOFF, 'utf8').split('\n')

const today = new Date()   // real date is fine in a plain tool (unlike a workflow script)
const ageOf = d => { const t = Date.parse(d + 'T00:00:00Z'); return Number.isFinite(t) ? Math.floor((today - t) / 86400000) : null }

// GitHub-flavoured heading slug, then collapsed to a canonical form so the anchor check tolerates
// single-vs-double-hyphen renderer differences (— removal etc.) — we're detecting "the section is
// GONE", not policing exact punctuation. Same normalize is applied to both sides.
const canon = s => s.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '')
const anchorCache = new Map()
function docAnchors(relPath) {          // relPath = 'design/foo.md' (no #), DOCS-relative
  if (anchorCache.has(relPath)) return anchorCache.get(relPath)
  const abs = path.join(DOCS, relPath)
  let set = new Set()
  try {
    for (const ln of fs.readFileSync(abs, 'utf8').split('\n')) {
      const h = ln.match(/^#{1,6}\s+(.+?)\s*#*\s*$/)
      if (h) set.add(canon(h[1]))
    }
  } catch { set = null }                 // unreadable → caller already flags the file as broken
  anchorCache.set(relPath, set)
  return set
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
      if (anchors && !anchors.has(canon(decodeURIComponent(anchor)))) brokenAnchors.push(`${file}#${anchor}`)
    }
  }
  // ⚠ A Resume-at with NO #anchor made the anchor check INERT for that lane — it reported clean because
  // it had nothing to check. ~a third of the lanes had drifted to a bare doc link or plain prose, so the
  // back door was guarding far less than it appeared to. Flag the absence, not just the breakage.
  // Accept any of the drifted spellings when looking for one (Resume-at / Resume at / resume at).
  const resumeLine = block.find(l => /\*\*\s*Resume[- ]at/i.test(l))
  const noResume = !resumeLine
  const unanchored = !!resumeLine && !/\]\([^)\s]+\.md#[^)\s]*\)/.test(resumeLine)
  lanes.push({ date: m[1], title: m[2].trim(), age: ageOf(m[1]), links: targets, broken, brokenAnchors,
               noResume, unanchored, line: i + 1 })
  i = j
}

if (asJson) { console.log(JSON.stringify({ lanes, staleDays: STALE_DAYS }, null, 2)); process.exit(0) }

const tty = process.stdout.isTTY
const b = s => tty ? `\x1b[1m${s}\x1b[0m` : s
const dim = s => tty ? `\x1b[2m${s}\x1b[0m` : s
const warn = s => tty ? `\x1b[33m${s}\x1b[0m` : s
const isStale = l => (l.age != null && l.age > STALE_DAYS) || l.broken.length > 0 || l.brokenAnchors.length > 0 || l.noResume || l.unanchored

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
             : l.noResume ? ' · no Resume-at' : l.unanchored ? ' · Resume-at unanchored' : ' stale'
  const tag = isStale(l) ? warn(`⚠ ${age}${flag}`) : dim(age)
  // print the LINE so the front door IS the index — a hand-maintained one drifted for 3 lanes / 4 days
  console.log(`  · ${dim('L' + String(l.line).padEnd(4))} ${l.title}  ${tag}`)
}
console.log(dim('  → open docs/HANDOFF.md for the Resume-at pointers'))
