#!/usr/bin/env node
// aso-compose.js — deterministically PACK the App Store keyword field (the mechanical core
// of the metadata composer, docs/design/store-agents.md §ASO). The TASTE — a title &
// subtitle that read like a person wrote them — is the agent's job (the read-aloud test);
// this tool owns the fiddly, error-prone part: given the visible fields + a PRIORITY-ORDERED
// candidate pool, pack the best-fitting <=100-char keyword field with zero waste.
//
//   node tools/aso-compose.js --title "Tinyjams: Pocket Music Toys" \
//     --subtitle "Make music with tiny synths" \
//     --candidates "pocket operator,groovebox,drum machine,sampler,sequencer,beat,bass,studio,loop,mixer,auv3,pads,rhythm,arp,techno,acid,808,chiptune,lofi"
//
// All deterministic:
//   1. Flatten candidates to individual words IN PRIORITY ORDER (Apple auto-combines singles,
//      so "drum machine" -> drum, machine; "pocket operator" -> pocket(+title), operator).
//   2. Drop stopwords, drop any word already in Title/Subtitle (a word only ranks once), dedupe.
//   3. Greedily add words while the comma-joined field stays <= --limit (default 100); report
//      what FIT and what got CUT (so you can re-prioritise), plus per-candidate coverage.
// Output is lint-clean by construction (feed it straight to aso-lint.js to confirm).
'use strict'

const LIMIT_DEFAULT = 100
const argv = process.argv.slice(2)
const opt = { title: '', subtitle: '', candidates: '', limit: String(LIMIT_DEFAULT) }
for (let i = 0; i < argv.length; i++) {
  const k = argv[i].replace(/^--/, '')
  if (argv[i].startsWith('--') && k in opt) opt[k] = argv[++i] ?? ''
  else { console.error(`unknown arg: ${argv[i]}\nusage: node tools/aso-compose.js --title "…" --subtitle "…" --candidates "a,b c,d" [--limit 100]`); process.exit(1) }
}
if (!opt.candidates) { console.error('need --candidates "term one,term two,…" (priority order)'); process.exit(1) }
const LIMIT = parseInt(opt.limit, 10) || LIMIT_DEFAULT

const STOP = new Set(('a an and or the for with to in on of at your own is are be this that '
  + 'app apps free get my me you it').split(' '))
const words = s => s.toLowerCase().split(/[^a-z0-9]+/).filter(Boolean)

const used = new Set([...words(opt.title), ...words(opt.subtitle)])   // a word only ranks once
const entries = opt.candidates.split(',').map(e => e.trim()).filter(Boolean)

// flatten to priority-ordered unique words, tracking why any word is dropped
const picked = [], cutNoFit = []
const seen = new Set()
const dropStop = new Set(), dropUsed = new Set()
const fits = w => ([...picked, w].join(',')).length <= LIMIT

for (const entry of entries) {
  for (const w of words(entry)) {
    if (seen.has(w)) continue
    seen.add(w)
    if (STOP.has(w)) { dropStop.add(w); continue }
    if (used.has(w)) { dropUsed.add(w); continue }
    if (fits(w)) picked.push(w)
    else cutNoFit.push(w)
  }
}

const field = picked.join(',')
const status = e => {
  const ws = words(e).filter(w => !STOP.has(w) && !used.has(w))
  if (!ws.length) return 'skip'                                  // entirely stop/used
  if (ws.every(w => picked.includes(w))) return 'in'
  if (ws.some(w => picked.includes(w))) return 'partial'
  return 'cut'
}
const mark = { in: '\x1b[32m✓\x1b[0m', partial: '\x1b[33m~\x1b[0m', cut: '\x1b[31m✗\x1b[0m', skip: '·' }

console.log(`\nkeyword field  (${field.length}/${LIMIT})`)
console.log('  ' + field + '\n')
console.log('candidate coverage (priority order):')
for (const e of entries) console.log(`  ${mark[status(e)]} ${e}`)
if (dropUsed.size) console.log(`\n  already in title/subtitle (skipped, correctly): ${[...dropUsed].join(', ')}`)
if (dropStop.size) console.log(`  stopwords dropped: ${[...dropStop].join(', ')}`)
if (cutNoFit.length) console.log(`\n  ⚠ didn't fit (raise their priority to include): ${cutNoFit.join(', ')}`)
console.log(`\nnext: paste the field into aso-lint.js to confirm clean; you write the title/subtitle (read-aloud test).\n`)
