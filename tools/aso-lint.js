#!/usr/bin/env node
// aso-lint.js — lint App Store metadata FIELDS (the deterministic "script" half of the
// metadata composer in docs/design/store-agents.md §ASO). Offline, no network, no account.
// The agent's job is the TASTE (a title/subtitle that read like a person wrote them); this
// tool does the mechanical part a lint can own: char limits, wasted space, cross-field
// repeats, and coverage of your researched keywords.
//
//   node tools/aso-lint.js --title "Mipo Puppetmaker" \
//     --subtitle "Imagine, Create and Play" \
//     --keywords "ragdoll,sandbox,avatar,puppet,create your own character,for kids" \
//     --research "marionette,puppet pals,craft"        # optional: coverage check
//
// Apple's field rules this encodes:
//   - Title 30, Subtitle 30, Keywords 100 characters (hard caps).
//   - You rank on the UNION of the three fields; a word only needs to appear ONCE — a word
//     in Keywords that's already in Title/Subtitle is wasted.
//   - In the Keyword field: NO spaces after commas (wasted chars), Apple ignores common
//     STOPWORDS (a/the/for/and/your/own…), and Apple AUTO-COMBINES your single words into
//     phrases — so multi-word keyword entries are usually wasted (exception: a brand you're
//     targeting, e.g. "toca boca", which must stay a phrase to match).
'use strict'

const LIMITS = { title: 30, subtitle: 30, keywords: 100 }

// ── args ─────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2)
const opt = { title: '', subtitle: '', keywords: '', research: '' }
for (let i = 0; i < argv.length; i++) {
  const a = argv[i]
  const key = a.replace(/^--/, '')
  if (a.startsWith('--') && key in opt) opt[key] = argv[++i] ?? ''
  else { console.error(`unknown arg: ${a}\nusage: node tools/aso-lint.js --title "…" --subtitle "…" --keywords "a,b,c" [--research "x,y"]`); process.exit(1) }
}
if (!opt.title && !opt.subtitle && !opt.keywords) {
  console.error('usage: node tools/aso-lint.js --title "…" --subtitle "…" --keywords "a,b,c" [--research "x,y"]')
  process.exit(1)
}

// Common App Store stopwords (Apple ignores these; the exact list is undocumented — this is
// the conservative, widely-agreed set). Kept out of char-count advice as "wasted".
const STOP = new Set(('a an and or the for with to in on of at your own is are be this that '
  + 'app apps free get my me you it').split(' '))

const words = s => s.toLowerCase().split(/[^a-z0-9]+/).filter(Boolean)
const ok = '\x1b[32m✓\x1b[0m', warn = '\x1b[33m⚠\x1b[0m', bad = '\x1b[31m✗\x1b[0m'
let issues = 0

console.log('\nASO metadata lint\n')

// ── 1. char limits ──────────────────────────────────────────────────────────
for (const f of ['title', 'subtitle', 'keywords']) {
  const n = opt[f].length, lim = LIMITS[f]
  if (!opt[f]) { console.log(`  ${warn} ${f}: empty`); continue }
  const over = n > lim
  if (over) issues++
  const mark = over ? bad : (n < lim * 0.6 && f !== 'title' ? warn : ok)
  const note = over ? `  OVER by ${n - lim}` : (n < lim * 0.6 && f === 'subtitle'
    ? '  (lots of room — is it earning keywords?)' : '')
  console.log(`  ${mark} ${f.padEnd(9)} ${n}/${lim}${note}`)
}

// ── parse keywords ────────────────────────────────────────────────────────────
const rawEntries = opt.keywords ? opt.keywords.split(',') : []
const entries = rawEntries.map(e => e.trim()).filter(Boolean)
const kwWords = entries.flatMap(words)

// ── 2. keyword-field hygiene ──────────────────────────────────────────────────
if (opt.keywords) {
  console.log('\n  keyword field:')

  // wasted whitespace around commas
  const wasted = rawEntries.reduce((s, e) => s + (e.length - e.trim().length), 0)
  if (wasted > 0) { issues++; console.log(`    ${warn} ${wasted} char(s) wasted on spaces after commas — remove them`) }
  else console.log(`    ${ok} no wasted comma-spaces`)

  // stopwords
  const stops = [...new Set(kwWords.filter(w => STOP.has(w)))]
  if (stops.length) { issues++; console.log(`    ${warn} stopwords Apple ignores (drop them): ${stops.join(', ')}`) }

  // multi-word phrase entries (Apple auto-combines single words)
  const phrases = entries.filter(e => e.includes(' '))
  if (phrases.length) { issues++; console.log(`    ${warn} multi-word entries (Apple auto-combines singles — split unless it's a brand): ${phrases.map(p => `"${p}"`).join(', ')}`) }

  // duplicate words inside the field
  const seen = new Set(), dupes = new Set()
  for (const w of kwWords) { if (seen.has(w)) dupes.add(w); else seen.add(w) }
  if (dupes.size) { issues++; console.log(`    ${warn} duplicate words within keywords: ${[...dupes].join(', ')}`) }
}

// ── 3. cross-field repeats (the big one) ──────────────────────────────────────
const titleW = new Set(words(opt.title)), subW = words(opt.subtitle)
const visible = new Set([...titleW, ...subW])
const repeats = [...new Set(kwWords.filter(w => visible.has(w) && !STOP.has(w)))]
console.log('\n  cross-field:')
// title ↔ subtitle overlap — both are indexed, so a word in both is wasted too
const tsDup = [...new Set(subW.filter(w => titleW.has(w) && !STOP.has(w)))]
if (tsDup.length) { issues++; console.log(`    ${warn} in BOTH title & subtitle (wasted — a word only ranks once): ${tsDup.join(', ')}`) }
if (repeats.length) {
  issues++
  console.log(`    ${warn} in Keywords AND Title/Subtitle (wasted — a word only needs to appear once): ${repeats.join(', ')}`)
} else if (opt.keywords) {
  console.log(`    ${ok} no words repeated across fields`)
}

// ── 4. coverage of researched terms ───────────────────────────────────────────
if (opt.research) {
  const all = new Set([...visible, ...kwWords])
  const terms = opt.research.split(',').map(t => t.trim()).filter(Boolean)
  const missing = terms.filter(t => !words(t).every(w => all.has(w)))
  console.log('\n  coverage:')
  if (missing.length) { issues++; console.log(`    ${warn} researched terms not present anywhere: ${missing.join(', ')}`) }
  else console.log(`    ${ok} all ${terms.length} researched terms covered`)
}

console.log(`\n${issues ? warn + ' ' + issues + ' thing(s) to look at' : ok + ' clean'} — remember the read-aloud test for the visible fields.\n`)
process.exit(issues ? 1 : 0)
