#!/usr/bin/env node
// aso-score.js — SCORE an App Store listing (title/subtitle/keywords) and A/B a TWEAK against the
// committed one, in the terminal, for the tweak → score → tweak loop with the agent. Sub-scores —
//   · BUDGET      — are you using the char space? (title/subtitle ≤30, keyword field ≤100)
//   · HYGIENE     — the aso-lint rules as penalties: over-limit, comma-spaces, stopwords,
//                   multi-word entries, cross-field repeats (a word ranks once — repeating is waste)
//   · REACH       — coverage of the demand WORDS from apps/<name>/seo-brief.md (if present)
//   · WINNABILITY — (--deep, hits the network) per-keyword difficulty from aso-research: a term you
//                   can't rank for is a bad bet even if it's popular. winnable = 100 − difficulty.
//                   Without this, REACH alone would REWARD adding a crowded head term you'll never
//                   rank for — winnability is what makes a tweak MEANINGFUL, not just longer.
// It prints the numbers AND the evidence (waste items, missed words, per-term difficulty) — never a
// magic grade. The SCRIPT scores; the agent + maker own the taste (RELEVANCE, does it read human).
// Detailed field report → aso-lint.js. docs/design/store-agents.md §"palette + mirror".
//
//   node tools/aso-score.js tinyjam
//   node tools/aso-score.js tinyjam --subtitle "Make beats with pocket synths"     # A/B a tweak
//   node tools/aso-score.js tinyjam --keywords "groovebox,drum,machine,sampler,beat,synth,bass"
//   node tools/aso-score.js tinyjam --deep                    # + winnability (per-keyword difficulty)
//   node tools/aso-score.js tinyjam --deep --keywords "…" --json
'use strict'

const fs = require('fs')
const path = require('path')
const { spawnSync } = require('child_process')
const ROOT = path.join(__dirname, '..')

const LIMITS = { title: 30, subtitle: 30, keywords: 100 }
// Apple ignores these in the keyword field (undocumented; the practical set, shared with aso-lint).
const STOP = new Set(('a an and the or of to for in on at with is are be your you my me it this that '
  + 'app apps free new by from can').split(' '))

// ── args ─────────────────────────────────────────────────────────────────────
const USAGE = 'usage: node tools/aso-score.js <app> [--title …] [--subtitle …] [--keywords "a,b,c"] [--deep] [--country cc] [--json]'
function die(msg) { console.error(msg + '\n' + USAGE); process.exit(2) }
const argv = process.argv.slice(2)
let app = null, asJson = false, deep = false, country = 'us'
const over = {}
for (let i = 0; i < argv.length; i++) {
  const a = argv[i]
  if (a === '--title' || a === '--subtitle' || a === '--keywords') over[a.slice(2)] = argv[++i] ?? ''
  else if (a === '--deep') deep = true
  else if (a === '--country') country = (argv[++i] || 'us').toLowerCase()
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
const kwEntriesOf = L => L.keywords.split(',').map(s => s.trim()).filter(Boolean)

// WINNABILITY needs per-keyword difficulty — one aso-research call over the UNION of both
// listings' keyword entries (deduped), so committed + tweak share the fetch.
let diffMap = null
if (deep) {
  const terms = [...new Set([...kwEntriesOf(listingBase), ...kwEntriesOf(listingTweak)])]
  if (terms.length) {
    process.stderr.write(`aso-score: --deep → aso-research on ${terms.length} keyword terms…\n`)
    const r = spawnSync('node', [path.join(ROOT, 'tools/aso-research.js'), '--json', '--country', country, ...terms],
      { cwd: ROOT, encoding: 'utf8', maxBuffer: 32 * 1024 * 1024 })
    if (r.status === 0) {
      try {
        diffMap = new Map()
        for (const t of (JSON.parse(r.stdout).terms || [])) if (!t.error) diffMap.set(t.term.toLowerCase(), { score: t.score, band: t.band })
      } catch { diffMap = null }
    }
    if (!diffMap) process.stderr.write('aso-score: research failed — scoring without winnability\n')
  }
}

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

  // WINNABILITY — per-keyword difficulty (deep mode); winnable = 100 − difficulty. Averaged over
  // the keyword entries that got a difficulty. Low-difficulty terms = a new app can actually rank.
  let winnability = null
  if (diffMap) {
    const rated = kwEntries.map(e => ({ e, d: diffMap.get(e.toLowerCase()) })).filter(x => x.d)
    ev.terms = rated.map(x => ({ term: x.e, score: x.d.score, band: x.d.band })).sort((a, b) => b.score - a.score)
    if (rated.length) winnability = clamp(rated.reduce((a, x) => a + (100 - x.d.score), 0) / rated.length)
  }

  // OVERALL — weighted mean over the dims present (hygiene/reach/winnability weight 3, budget 1);
  // budget is the weakest signal (filling space with bad words is worse than empty space).
  const dims = [['hygiene', hygiene, 3], ['reach', reach, 3], ['winnability', winnability, 3], ['budget', budget, 1]]
    .filter(d => d[1] != null)
  const W = dims.reduce((a, d) => a + d[2], 0)
  const overall = clamp(dims.reduce((a, d) => a + d[1] * d[2], 0) / W)
  return { overall, budget, hygiene, reach, winnability, ev }
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
function scoreRow(label, k) {
  if (sBase[k] == null) return   // dim hint printed separately below
  console.log(`  ${label.padEnd(11)} ${String(sBase[k]).padStart(3)}` +
    (tweaked ? `   →  ${String(sTweak[k]).padStart(3)}  ${delta(sBase[k], sTweak[k])}` : ''))
}

console.log(`\naso-score · ${manifest.name || app}` + (tweaked ? `   ${dim('committed → tweaked')}` : '') + '\n')
console.log(b(tweaked ? 'COMMITTED listing' : 'listing'))
fieldLines(listingBase)
if (tweaked) { console.log('\n' + b('TWEAKED listing')); fieldLines(listingTweak) }

console.log('\n' + b('score') + dim('   (hygiene · reach · winnability weighted 3, budget 1)'))
scoreRow('overall', 'overall'); scoreRow('reach', 'reach'); scoreRow('winnability', 'winnability')
scoreRow('hygiene', 'hygiene'); scoreRow('budget', 'budget')
if (sBase.reach == null) console.log(dim('  reach       n/a — run aso-brief for the demand-word coverage'))
if (sBase.winnability == null) console.log(dim('  winnability n/a — run with --deep for per-keyword difficulty (hits the network)'))

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
if (E.terms && E.terms.length) {
  const hard = E.terms.filter(t => t.band === 'HARD'), easy = E.terms.filter(t => t.band === 'EASY')
  if (hard.length) console.log(`  ⚠ HARD keywords (crowded — a new app won't rank; drop unless core): ${hard.map(t => `${t.term}·${t.score}`).join(', ')}`)
  if (easy.length) console.log(`  ✓ EASY keywords (winnable for a fresh app — lean here): ${easy.map(t => t.term).join(', ')}`)
  console.log(dim(`  per-keyword difficulty: ${E.terms.map(t => `${t.term} ${t.band}·${t.score}`).join('  ')}`))
}
if (!E.over.length && !E.repeats.length && !E.stops.length && !E.multi.length && !E.commaSpaces) console.log('  ✓ no hygiene waste')
console.log(dim(`\n  taste is the agent's + yours: relevance & does-it-read-human aren't scored here.`))
console.log(dim(`  detail → aso-lint · difficulty vs competition → aso-research / 🔬 analyze\n`))
process.exit((sTweak || sBase).ev.over.length ? 1 : 0)
