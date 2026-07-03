#!/usr/bin/env node
// handoff.js — the ACTIVE-LANE tracker for docs/HANDOFF.md. Keeps the handoff RECENT and the
// reliable place to resume complex in-flight work, via the two-door pattern from
// docs/design/driftable-docs.md (front door primes going in, back door catches what slipped):
//
//   node tools/handoff.js          # FRONT DOOR — list the active ▶ lanes + age (wired into orient)
//   node tools/handoff.js --check  # BACK DOOR  — flag lanes >2wk old or with a broken doc link
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

const lanes = []
for (let i = 0; i < lines.length; i++) {
  const m = lines[i].match(/^>\s*\*\*▶\s*ACTIVE THREAD\s*\((\d{4}-\d{2}-\d{2})\)\s*[—-]+\s*(.+?)\.?\*\*/)
  if (!m) continue
  let j = i, block = []
  while (j < lines.length && /^\s*>/.test(lines[j])) { block.push(lines[j]); j++ }
  const body = block.join('\n')
  const links = [...new Set([...body.matchAll(/\]\(([^)\s]+\.md)[^)]*\)/g)].map(x => x[1]))]
  const broken = links.filter(l => !fs.existsSync(path.join(DOCS, l.replace(/#.*$/, ''))))
  lanes.push({ date: m[1], title: m[2].trim(), age: ageOf(m[1]), links, broken, line: i + 1 })
  i = j
}

if (asJson) { console.log(JSON.stringify({ lanes, staleDays: STALE_DAYS }, null, 2)); process.exit(0) }

const tty = process.stdout.isTTY
const b = s => tty ? `\x1b[1m${s}\x1b[0m` : s
const dim = s => tty ? `\x1b[2m${s}\x1b[0m` : s
const warn = s => tty ? `\x1b[33m${s}\x1b[0m` : s
const isStale = l => (l.age != null && l.age > STALE_DAYS) || l.broken.length > 0

if (!lanes.length) {
  console.log('no active lanes in docs/HANDOFF.md — add a `▶ ACTIVE THREAD (date) — title.` callout when you start complex work')
  process.exit(0)
}

if (check) {
  const bad = lanes.filter(isStale)
  if (!bad.length) { console.log(`handoff: ${lanes.length} active lane(s), all fresh (≤${STALE_DAYS}d, links resolve)`); process.exit(0) }
  console.log(b(`HANDOFF LANES (advisory) — refresh the date, or prune if shipped (→ STATUS.md):`))
  for (const l of bad) {
    const why = [l.age != null && l.age > STALE_DAYS ? `${l.age}d stale` : null, l.broken.length ? `broken link: ${l.broken.join(', ')}` : null].filter(Boolean).join(' · ')
    console.log(`  ${warn('⚠')} ${l.title}  ${dim('(' + l.date + ')')}  ${why}`)
  }
  process.exit(1)
}

// front door — list active lanes + age
console.log(b('ACTIVE LANES') + dim('  (docs/HANDOFF.md — resume complex work here)'))
for (const l of lanes) {
  const age = l.age == null ? '?' : l.age === 0 ? 'today' : `${l.age}d`
  const tag = isStale(l) ? warn(`⚠ ${age}${l.broken.length ? ' · broken link' : ' stale'}`) : dim(age)
  console.log(`  · ${l.title}  ${tag}`)
}
console.log(dim('  → open docs/HANDOFF.md for the Resume-at pointers'))
