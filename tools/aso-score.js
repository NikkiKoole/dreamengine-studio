#!/usr/bin/env node
// aso-score.js — SCORE an App Store listing (title/subtitle/keywords) and A/B a TWEAK against the
// committed one, in the terminal, for the tweak → score → tweak loop with the agent. Local +
// INSTANT (no network): three transparent sub-scores —
//   · BUDGET   — are you using the char space? (title/subtitle ≤30, keyword field ≤100)
//   · HYGIENE  — the aso-lint rules as penalties: over-limit, comma-spaces, stopwords, multi-word
//                entries, cross-field repeats (a word ranks once — repeating it is wasted)
//   · REACH    — coverage of the demand WORDS from apps/<name>/seo-brief.md (if present)
// It prints the numbers AND the evidence (counts, waste items, missed words) — never a magic grade.
// The SCRIPT scores; the agent + maker own the taste (relevance, does it read human). Detailed
// field report → aso-lint.js · per-keyword difficulty vs competition → aso-research.js / 🔬 analyze.
// docs/design/store-agents.md §"palette + mirror".
//
//   node tools/aso-score.js tinyjam
//   node tools/aso-score.js tinyjam --subtitle "Make beats with pocket synths"     # A/B a tweak
//   node tools/aso-score.js tinyjam --keywords "groovebox,drum,machine,sampler,beat,synth,bass"
//   node tools/aso-score.js tinyjam --title "Tiny Jam: Beat & Synth Studio" --json
'use strict'

const fs = require('fs')
const path = require('path')
const ROOT = path.join(__dirname, '..')

const LIMITS = { title: 30, subtitle: 30, keywords: 100 }
// Apple ignores these in the keyword field (undocumented; the practical set, shared with aso-lint).
const STOP = new Set(('a an and the or of to for in on at with is are be your you my me it this that '
  + 'app apps free new by from can').split(' '))

// ── args ─────────────────────────────────────────────────────────────────────
const USAGE = 'usage: node tools/aso-score.js <app> [--title …] [--subtitle …] [--keywords "a,b,c"] [--json]'
function die(msg) { console.error(msg + '\n' + USAGE); process.exit(2) }
const argv = process.argv.slice(2)
let app = null, asJson = false
const over = {}
for (let i = 0; i < argv.length; i++) {
  const a = argv[i]
  if (a === '--title' || a === '--subtitle' || a === '--keywords') over[a.slice(2)] = argv[++i] ?? ''
  else if (a === '--json') asJson = true
  else if (a.startsWith('-')) die(`unknown flag: ${a}`)
  else if (!app) app = a
  else die(`unexpected arg: ${a}`)
}
if (!app) die('which app? e.g. node tools/aso-score.js tinyjam')

const appDir = path.join(ROOT, 'apps', app)
if (!fs.existsSync(path.join(appDir, 'app.json'))) die(`no manifest at apps/${app}/app.json`)
const manifest = JSON.parse(fs.readFileSync(path.join(appDir, 'app.json'), 'utf8'))
const base = manifest.listing || {}
const tweaked = Object.keys(over).length > 0
const listingBase = { title: base.title || '', subtitle: base.subtitle || '', keywords: base.keywords || '' }
const listingTweak = { ...listingBase, ...over }

// ── the demand words from the worksheet (for REACH), if it exists ──────────────
let briefWords = null
const briefPath = path.join(appDir, 'seo-brief.md')
if (fs.existsSync(briefPath)) {
  const m = fs.readFileSync(briefPath, 'utf8').match(/<!--\s*aso-coverage\s*([\s\S]*?)-->/)
  if (m) { try { briefWords = (JSON.parse(m[1].trim()).words || []); } catch {} }
}

// ── scoring (transparent formulas; evidence collected alongside) ───────────────
const clamp = n => Math.max(0, Math.min(100, Math.round(n)))
const wordsOf = s => String(s).toLowerCase().split(/[^a-z0-9]+/).filter(Boolean)

function score(L) {
  const ev = { over: [], commaSpaces: 0, stops: [], multi: [], repeats: [], missed: [], fills: {} }

  // BUDGET — reward using the space; a field OVER its limit is invalid (0 for that field).
  const fill = (v, lim) => { const n = v.length; ev.fills[lim] = n; return n > lim ? 0 : n / lim * 100 }
  for (const [f, lim] of Object.entries(LIMITS)) if (L[f].length > lim) ev.over.push(`${f} ${L[f].length}/${lim}`)
  const budget = clamp((fill(L.title, LIMITS.title) + fill(L.subtitle, LIMITS.subtitle) + fill(L.keywords, LIMITS.keywords)) / 3)

  // HYGIENE — start at 100, subtract aso-lint-style waste.
  let h = 100
  h -= ev.over.length * 30                                  // over-limit is a hard fail
  ev.commaSpaces = (L.keywords.match(/,\s/g) || []).length  // Apple wants no space after commas
  h -= ev.commaSpaces * 1
  const kwEntries = L.keywords.split(',').map(s => s.trim()).filter(Boolean)
  const visible = new Set([...wordsOf(L.title), ...wordsOf(L.subtitle)])
  for (const e of kwEntries) {
    const parts = wordsOf(e)
    if (parts.length > 1) { ev.multi.push(e); h -= 2 }               // Apple auto-combines singles
    for (const w of parts) {
      if (STOP.has(w)) { ev.stops.push(w); h -= 3 }                  // ignored → wasted chars
      else if (visible.has(w)) { ev.repeats.push(w); h -= 4 }        // already ranks via title/subtitle
    }
  }
  const hygiene = clamp(h)

  // REACH — how many demand words from the worksheet appear anywhere in the listing.
  let reach = null
  if (briefWords && briefWords.length) {
    const hay = new Set([...wordsOf(L.title), ...wordsOf(L.subtitle), ...wordsOf(L.keywords)])
    const hit = briefWords.filter(w => hay.has(w))
    ev.missed = briefWords.filter(w => !hay.has(w))
    reach = clamp(100 * hit.length / briefWords.length)
    ev.reachHit = hit.length; ev.reachTot = briefWords.length
  }

  // OVERALL — reach 40 · hygiene 40 · budget 20 (reach unavailable → hygiene 60 / budget 40).
  const overall = reach == null
    ? clamp(hygiene * 0.6 + budget * 0.4)
    : clamp(reach * 0.4 + hygiene * 0.4 + budget * 0.2)
  return { overall, budget, hygiene, reach, ev }
}

const sBase = score(listingBase)
const sTweak = tweaked ? score(listingTweak) : null

// ── output ─────────────────────────────────────────────────────────────────────
if (asJson) {
  console.log(JSON.stringify({ app, committed: { listing: listingBase, ...sBase },
    tweak: tweaked ? { listing: listingTweak, ...sTweak } : null }, null, 2))
  process.exit(0)
}

const tty = process.stdout.isTTY
const b = s => tty ? `\x1b[1m${s}\x1b[0m` : s
const dim = s => tty ? `\x1b[2m${s}\x1b[0m` : s
const delta = (a, c) => { if (a === c) return dim(' ='); const d = c - a; return (d > 0 ? `\x1b[32m▲+${d}` : `\x1b[31m▼${d}`) + (tty ? '\x1b[0m' : '') }

function fieldLines(L) {
  for (const [f, lim] of Object.entries(LIMITS)) {
    const n = L[f].length
    console.log(`  ${f.padEnd(9)} ${String(n).padStart(3)}/${lim}${n > lim ? b(' ⚠ OVER') : ''}  ${dim(L[f] || '—')}`)
  }
}
function scoreRow(label, kBase, kT) {
  const line = `  ${label.padEnd(9)} ${String(sBase[kBase]).padStart(3)}` +
    (tweaked ? `   →  ${String(sTweak[kBase]).padStart(3)}  ${delta(sBase[kBase], sTweak[kBase])}` : '')
  console.log(sBase[kBase] == null ? `  ${label.padEnd(9)} ${dim('n/a (no seo-brief.md — run aso-brief for REACH)')}` : line)
}

console.log(`\naso-score · ${manifest.name || app}` + (tweaked ? `   ${dim('committed → tweaked')}` : '') + '\n')
console.log(b(tweaked ? 'COMMITTED listing' : 'listing'))
fieldLines(listingBase)
if (tweaked) { console.log('\n' + b('TWEAKED listing')); fieldLines(listingTweak) }

console.log('\n' + b('score') + dim('   (reach 40 · hygiene 40 · budget 20)'))
scoreRow('overall', 'overall'); scoreRow('reach', 'reach'); scoreRow('hygiene', 'hygiene'); scoreRow('budget', 'budget')

// evidence from whichever listing we care about most (the tweak if given, else committed)
const E = (sTweak || sBase).ev, L = tweaked ? listingTweak : listingBase
console.log('\n' + b('why') + dim(tweaked ? '  (for the TWEAKED listing)' : ''))
if (E.over.length) console.log(`  ⚠ OVER LIMIT: ${E.over.join(', ')}  — invalid, fix first`)
if (E.repeats.length) console.log(`  ⚠ repeats title/subtitle in keywords (ranks once, wasted): ${[...new Set(E.repeats)].join(', ')}`)
if (E.stops.length) console.log(`  · stopwords in keyword field (Apple ignores): ${[...new Set(E.stops)].join(', ')}`)
if (E.multi.length) console.log(`  · multi-word keyword entries (singles auto-combine): ${E.multi.join(' | ')}`)
if (E.commaSpaces) console.log(`  · ${E.commaSpaces} space(s) after commas in keywords — ${E.commaSpaces} wasted char(s)`)
if (E.reachTot != null) {
  console.log(`  reach: ${E.reachHit}/${E.reachTot} worksheet demand words used` +
    (E.missed.length ? `; missing: ${E.missed.slice(0, 16).join(', ')}${E.missed.length > 16 ? ` …+${E.missed.length - 16}` : ''}` : ''))
}
if (!E.over.length && !E.repeats.length && !E.stops.length && !E.multi.length && !E.commaSpaces) console.log('  ✓ no hygiene waste')
console.log(dim(`\n  taste is the agent's + yours: relevance & does-it-read-human aren't scored here.`))
console.log(dim(`  detail → aso-lint · difficulty vs competition → aso-research / 🔬 analyze\n`))
process.exit((sTweak || sBase).ev.over.length ? 1 : 0)
