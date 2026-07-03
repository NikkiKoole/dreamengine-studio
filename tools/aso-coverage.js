#!/usr/bin/env node
// aso-coverage.js — the MIRROR to aso-brief.js: read your WRITTEN press.md + App Store listing
// and report (a) COVERAGE of the demand terms the brief surfaced and (b) keyword STUFFING in the
// prose. It never rewrites a word — it confirms the ADR-0022 two-part bar on the press page:
// DISCOVERABLE (the terms landed) AND LEGIBLE-TO-A-STRANGER (it still reads like a person wrote
// it, not keyword soup). Design: docs/design/store-agents.md §"palette + mirror".
//
//   node tools/aso-coverage.js tinyjam
//   node tools/aso-coverage.js tinyjam --max 4     # stricter stuffing threshold (default 5)
//
// Reads apps/<name>/{seo-brief.md (its trailing <!-- aso-coverage --> data block), press.md,
// app.json listing}. Reports:
//   · PHRASE coverage — which website/press phrases your prose naturally hits; high-value misses
//   · WORD coverage — which store words appear anywhere in title/subtitle/keywords/prose
//   · STUFFING — any content word over-repeated in the prose body (reads spammy to Google/Apple)
// Coverage gaps are ADVISORY (you may skip a term on purpose). Exit 1 only on stuffing (CI-friendly)
// — the mirror fails you for sounding robotic, not for leaving a keyword on the table.
'use strict'

const fs = require('fs')
const path = require('path')

const ROOT = path.join(__dirname, '..')
const STOP = new Set(('the a an and or of to for in on at with your you my me it is are be as by '
  + 'from this that these those they them we us he she it its our so if then than not no more most '
  + 'app apps free new one all can into out up down over about vs whether what which how')
  .split(' '))

// ── args ─────────────────────────────────────────────────────────────────────
const USAGE = 'usage: node tools/aso-coverage.js <app> [--max N]'
function die(msg) { console.error(msg + '\n' + USAGE); process.exit(2) }
const argv = process.argv.slice(2)
let app = null, stuffMax = 5
for (let i = 0; i < argv.length; i++) {
  const a = argv[i]
  if (a === '--max') stuffMax = parseInt(argv[++i], 10) || 5
  else if (a.startsWith('-')) die(`unknown flag: ${a}`)
  else if (!app) app = a
  else die(`unexpected arg: ${a}`)
}
if (!app) die('which app? e.g. node tools/aso-coverage.js tinyjam')

const appDir = path.join(ROOT, 'apps', app)
const briefPath = path.join(appDir, 'seo-brief.md')
if (!fs.existsSync(briefPath)) die(`no worksheet at apps/${app}/seo-brief.md — run: node tools/aso-brief.js ${app}`)

// ── read the brief's hidden contract ───────────────────────────────────────────
const briefText = fs.readFileSync(briefPath, 'utf8')
const m = briefText.match(/<!--\s*aso-coverage\s*([\s\S]*?)-->/)
if (!m) die(`apps/${app}/seo-brief.md has no <!-- aso-coverage --> data block — regenerate with aso-brief.js`)
let data
try { data = JSON.parse(m[1].trim()) } catch { die('the aso-coverage data block is not valid JSON — regenerate') }
const phrases = data.phrases || [], words = data.words || []

// ── read what you WROTE ────────────────────────────────────────────────────────
const manifest = JSON.parse(fs.readFileSync(path.join(appDir, 'app.json'), 'utf8'))
const listing = manifest.listing || {}
const pressPath = path.join(appDir, 'press.md')
let prose = ''
if (fs.existsSync(pressPath)) {
  // strip a leading --- frontmatter --- block; keep the markdown body
  prose = fs.readFileSync(pressPath, 'utf8').replace(/^---\n[\s\S]*?\n---\n/, '')
} else {
  console.error(`(note: no apps/${app}/press.md — checking listing fields only)`)
}
const listingProse = `${listing.title || ''} ${listing.subtitle || ''}`
const keywordField = listing.keywords || ''

const norm = s => ` ${String(s).toLowerCase().replace(/[^a-z0-9]+/g, ' ').replace(/\s+/g, ' ').trim()} `
const proseN = norm(`${prose} ${listingProse}`)          // where PHRASES should live (Google/human)
const anyN = norm(`${prose} ${listingProse} ${keywordField}`)   // anywhere indexed (WORD coverage)
const hasWord = (hay, w) => hay.includes(` ${w} `)
const sigWords = phrase => phrase.toLowerCase().split(/[^a-z0-9]+/).filter(w => w.length >= 3 && !STOP.has(w))

// ── coverage ───────────────────────────────────────────────────────────────────
const phraseHits = [], phraseMiss = []
for (const p of phrases) {
  const sig = sigWords(p)
  const exact = proseN.includes(norm(p).trim())
  const covered = exact || (sig.length > 0 && sig.every(w => hasWord(proseN, w)))
  ; (covered ? phraseHits : phraseMiss).push({ p, exact })
}
const wordHits = [], wordMiss = []
for (const w of words) (hasWord(anyN, w) ? wordHits : wordMiss).push(w)

// ── stuffing (naturalness of the PROSE body) ────────────────────────────────────
const counts = new Map()
if (prose.trim()) for (const w of prose.toLowerCase().split(/[^a-z0-9]+/)) {
  if (w.length < 3 || STOP.has(w)) continue
  counts.set(w, (counts.get(w) || 0) + 1)
}
const overused = [...counts.entries()].filter(([, c]) => c > stuffMax).sort((a, b) => b[1] - a[1])

// ── report ───────────────────────────────────────────────────────────────────
const pct = (a, b) => b ? Math.round(100 * a / b) : 0
console.log(`\naso-coverage · ${manifest.name || app} · vs seo-brief.md (${data.generated || '?'})\n`)

console.log(`━━ website/press PHRASES — ${phraseHits.length}/${phrases.length} covered (${pct(phraseHits.length, phrases.length)}%)`)
if (phraseMiss.length) {
  console.log(`   missed (high-value first — work in the ones that fit, in your voice):`)
  for (const { p } of phraseMiss.slice(0, 12)) console.log(`     ○ ${p}`)
  if (phraseMiss.length > 12) console.log(`     … +${phraseMiss.length - 12} more`)
} else console.log(`   ✓ all covered`)
console.log()

console.log(`━━ App Store WORDS — ${wordHits.length}/${words.length} present somewhere (${pct(wordHits.length, words.length)}%)`)
if (wordMiss.length) console.log(`   not yet used: ${wordMiss.join(', ')}`)
if (keywordField) console.log(`   (keyword field is ${keywordField.length}/100 chars — pack more with aso-compose)`)
console.log()

let fail = false
console.log(`━━ STUFFING — does the prose still read human?`)
if (!prose.trim()) console.log(`   (no press.md prose to check)`)
else if (!overused.length) console.log(`   ✓ no word repeated more than ${stuffMax}× — reads natural`)
else {
  fail = true
  console.log(`   ⚠ over-repeated (>${stuffMax}× in the description — reads like keyword stuffing):`)
  for (const [w, c] of overused) console.log(`     ${c}×  ${w}`)
  console.log(`   → rephrase so no keyword is hammered; you rank on ONE mention, not ten.`)
}
console.log()

console.log(fail
  ? `verdict: ✗ prose reads stuffed — fix the repeats above (discoverable, but not legible).`
  : `verdict: ✓ reads human · ${pct(phraseHits.length, phrases.length)}% phrase / ${pct(wordHits.length, words.length)}% word coverage (gaps are your call).`)
console.log()
process.exit(fail ? 1 : 0)
