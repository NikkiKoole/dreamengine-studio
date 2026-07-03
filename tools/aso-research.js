#!/usr/bin/env node
// aso-research.js — DIY App Store keyword research from FREE, public data (v0.1).
// The unblocked leg of docs/design/store-agents.md §3: no App Store Connect account, no
// paid ASO subscription, no Search Term Rank report needed. Uses only the public iTunes
// Search API (itunes.apple.com/search — no key, no auth).
//
//   node tools/aso-research.js "puppet maker" "puppet" "marionette"
//   node tools/aso-research.js --country nl "poppen maken"
//   node tools/aso-research.js --app "Mipo Puppet Maker" "puppet maker" "marionette"
//   node tools/aso-research.js --json "puppet" > snapshot.json      # machine-readable
//
// Flags:
//   --country <cc>   storefront (default us)
//   --app "<name>"   flag where YOUR app ranks for each term (substring match on title)
//   --json           emit a dated JSON snapshot instead of the human table
//
// For each seed term it reports:
//   - a DIFFICULTY proxy (0-100): how crowded the term is × how strong the incumbents are.
//     RELATIVE, not absolute search volume — that needs Apple's Search Term Rank report
//     (App Store Connect → Analytics → Insights, still a phased beta as of 2026-07). This
//     tool is the half that works today; the report becomes the popularity COLUMN later.
//   - the top competitors by rating count (incumbent strength)
//   - your app's rank for the term, if --app is given
// Then, pooled across all seeds: MINED keyword candidates — title words the ranking apps
// use that you didn't seed (the "keywords you're missing" leg).
//
// WHY a proxy and not real volume: absolute volume is the paid tools' one true moat (user
// panels + embedded-SDK telemetry — surveillance-scale assets). See store-agents.md §ASO.
'use strict'

const COUNTRY_DEFAULT = 'us'
const LIMIT = 200

// ── args ───────────────────────────────────────────────────────────────────────
const USAGE = 'usage: node tools/aso-research.js [--country cc] [--app "Name"] [--json] "term" ...\n'
  + '  e.g. node tools/aso-research.js --country nl --app "Mipo" "poppen maken" "marionet"'
function die(msg) { console.error(msg + '\n' + USAGE); process.exit(1) }

const argv = process.argv.slice(2)
let country = COUNTRY_DEFAULT, myApp = null, asJson = false
const seeds = []
for (let i = 0; i < argv.length; i++) {
  let a = argv[i]
  // support --flag=value as well as --flag value
  let inlineVal = null
  const eq = a.indexOf('=')
  if (a.startsWith('--') && eq !== -1) { inlineVal = a.slice(eq + 1); a = a.slice(0, eq) }
  const takeVal = () => inlineVal !== null ? inlineVal
    : (i + 1 < argv.length && !argv[i + 1].startsWith('-')) ? argv[++i]
    : die(`flag ${a} needs a value`)

  if (a === '--country') country = takeVal()
  else if (a === '--app') myApp = takeVal()
  else if (a === '--json') asJson = true
  else if (a.startsWith('-')) die(`unknown flag: ${argv[i]}  (country is set with "--country nl", not "--nl")`)
  else seeds.push(argv[i])
}
country = country.toLowerCase()
if (!/^[a-z]{2}$/.test(country)) die(`--country wants a 2-letter code (us, nl, de, …), got "${country}"`)
if (!seeds.length) die('no search terms given — pass at least one, e.g. "poppen maken"')

// App Store CATEGORY labels — noise when mining competitor TITLES for keywords, so drop them.
const GENRE_WORDS = new Set(('games game entertainment education educational casual simulation '
  + 'puzzle family action adventure roleplaying board sports strategy music photo video graphics '
  + 'design social networking utilities productivity lifestyle kids trivia arcade word racing')
  .split(' '))
const STOP = new Set(('the a an and or of to for in on at with your you my me it is are app apps '
  + 'free pro plus lite hd new best top maker make making play')
  .split(' '))
const seedWords = new Set(seeds.join(' ').toLowerCase().split(/[^a-z0-9]+/).filter(Boolean))

const sleep = ms => new Promise(r => setTimeout(r, ms))
const fmt = x => x >= 1e6 ? (x / 1e6).toFixed(1) + 'M' : x >= 1e3 ? (x / 1e3).toFixed(1) + 'k' : String(x)

async function searchTerm(term) {
  const url = `https://itunes.apple.com/search?term=${encodeURIComponent(term)}`
    + `&entity=software&country=${country}&limit=${LIMIT}`
  const r = await fetch(url, { headers: { 'User-Agent': 'dreamengine-aso/0.1' } })
  if (!r.ok) throw new Error(`HTTP ${r.status}`)
  return (await r.json()).results || []
}

function difficulty(results) {
  const n = results.length
  const top = results.slice(0, 10).map(a => a.userRatingCount || 0).sort((a, b) => b - a)
  const med = top.length ? top[Math.floor(top.length / 2)] : 0
  const sat = Math.min(1, n / LIMIT)                       // saturation: is the SERP full?
  const strength = Math.min(1, Math.log10(med + 1) / 6)   // ~1M ratings → 1.0
  const score = Math.round((0.4 * sat + 0.6 * strength) * 100)
  const band = score >= 66 ? 'HARD' : score >= 33 ? 'MEDIUM' : 'EASY'
  return { n, med, score, band }
}

const pool = new Map()
function mineTitles(results) {
  for (const a of results.slice(0, 50)) {
    for (const w of (a.trackName || '').toLowerCase().split(/[^a-z0-9]+/)) {
      if (w.length < 3 || STOP.has(w) || GENRE_WORDS.has(w) || seedWords.has(w)) continue
      pool.set(w, (pool.get(w) || 0) + 1)
    }
  }
}
function genreMix(results) {
  const g = new Map()
  for (const a of results.slice(0, 20)) {
    const k = a.primaryGenreName || '?'
    g.set(k, (g.get(k) || 0) + 1)
  }
  return [...g.entries()].sort((a, b) => b[1] - a[1])
}
function myRank(results) {
  if (!myApp) return null
  const needle = myApp.toLowerCase()
  const i = results.findIndex(a => (a.trackName || '').toLowerCase().includes(needle))
  return i < 0 ? null : i + 1
}

;(async () => {
  const stamp = new Date().toISOString().slice(0, 10)
  const perTerm = []
  for (const term of seeds) {
    let results
    try { results = await searchTerm(term) }
    catch (e) { perTerm.push({ term, error: e.message }); continue }
    mineTitles(results)
    perTerm.push({
      term,
      ...difficulty(results),
      myRank: myRank(results),
      genres: genreMix(results),
      top: results.slice(0, 5).map(a => ({
        name: a.trackName, ratings: a.userRatingCount || 0,
        stars: a.averageUserRating || 0, genre: a.primaryGenreName,
      })),
    })
    await sleep(400)   // be polite to the API
  }
  const mined = [...pool.entries()].sort((a, b) => b[1] - a[1]).slice(0, 30)

  if (asJson) {
    console.log(JSON.stringify({ date: stamp, country, app: myApp, terms: perTerm, mined }, null, 2))
    return
  }

  console.log(`\nASO research · country=${country} · ${stamp}${myApp ? ` · app="${myApp}"` : ''}\n`)
  for (const t of perTerm) {
    if (t.error) { console.log(`━━ "${t.term}"  — fetch failed: ${t.error}\n`); continue }
    console.log(`━━ "${t.term}"`)
    console.log(`   difficulty ${t.score}/100 [${t.band}]  ·  ${t.n} apps rank`
      + `  ·  top-10 median ratings ${fmt(t.med)}`
      + (t.myRank ? `  ·  YOU rank #${t.myRank}` : (myApp ? `  ·  you: not in top ${LIMIT}` : '')))
    console.log(`   genres: ${t.genres.slice(0, 3).map(([k, c]) => `${k}·${c}`).join(' ')}`)
    for (const a of t.top) {
      console.log(`     ${fmt(a.ratings).padStart(6)} ★${a.stars.toFixed(1)}  ${a.name}`
        + (a.genre ? `  (${a.genre})` : ''))
    }
    console.log()
  }
  console.log(`━━ mined keyword candidates (title words the ranking apps use, not in your seeds)`)
  console.log('   ' + mined.map(([w, c]) => `${w}·${c}`).join('  '))
  console.log(`\n(difficulty is a RELATIVE proxy — crowding × incumbent strength — not absolute`)
  console.log(` search volume. That column arrives with Apple's Search Term Rank report.)\n`)
})()
