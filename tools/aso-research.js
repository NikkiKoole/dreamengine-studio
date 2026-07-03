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
// Then, pooled across all seeds: MINED keyword candidates — words the ranking apps use in
// their TITLES and DESCRIPTIONS that you didn't seed (the "keywords you're missing" leg).
// Ranked by DOCUMENT frequency (how many DISTINCT competitors use the word, not raw count —
// so one app repeating a word 30× doesn't win) with title hits weighted heavily, since the
// title is curated keyword real-estate. A ★ marks words a competitor put in its TITLE.
// (The App Store SUBTITLE is NOT exposed by the iTunes Search API — description is the
// recall source; the tagline after a dash in the title is the closest subtitle proxy.)
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
// Stopwords. The base set is enough for TITLES (curated, low-noise). DESCRIPTIONS are prose
// full of marketing fluff, so they get the base set PLUS the marketing set below — otherwise
// document-frequency mining just surfaces "love / easy / beautiful / features" (every app
// says them). Category NOUNS (synth, drum, marionette, ragdoll) are deliberately absent from
// both, so they survive and rank.
const STOP = new Set(('the a an and or of to for in on at with your you my me it is are app apps '
  + 'free pro plus lite hd new best top maker make making play')
  .split(' '))
const MARKETING_STOP = new Set(('be was were been being have has had do does did will would can '
  + 'could should this that these those they them their our we us he she his her its as by from '
  + 'up down out off over under into onto about all any both each more most other some such not '
  + 'only own same so than too very just also then here there when where why how what who which '
  + 'get got use using used want wants need needs like likes love loves made create creating '
  + 'creates plays playing enjoy world enter inspiring packed beautiful beautifully designed '
  + 'design amazing awesome great cool fun easy simple perfect powerful feature features '
  + 'featuring including includes available download downloads try tap touch swipe start let '
  + 'lets time day days way ways help helps everything anything something everyone anyone real '
  + 'live full complete ultimate premium version support supported experience explore discover '
  + 'now today one two first way if you\'ll you\'re we\'ve don\'t re ve ll '
  + 'iphone ipad ipod android device devices mobile phone tablet screen touchscreen os ios '
  + 'http https www com net org email mailto html')
  .split(' '))
const seedWords = new Set(seeds.join(' ').toLowerCase().split(/[^a-z0-9]+/).filter(Boolean))
const DESC_SCAN_CHARS = 600   // mine the PITCH (first paragraphs), not the boilerplate tail

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

// Mined-candidate pool. Per word we track DOCUMENT frequency (distinct apps), split by
// source: titles (t) vs descriptions (d). A word is scored t*4 + d — a title hit is worth
// ~4 description hits — but DISPLAYED as its honest distinct-app count (max 50), i.e. "N of
// the top competitors use this word."
const pool = new Map()   // word -> { t, d }
function bump(word, key) {
  const e = pool.get(word) || { t: 0, d: 0 }
  e[key]++; pool.set(word, e)
}
// unique alnum words in a string, filtered (dedupe within one app = document frequency)
function words(str, extraStop) {
  const seen = new Set()
  for (const w of (str || '').toLowerCase().split(/[^a-z0-9]+/)) {
    if (w.length < 3 || STOP.has(w) || GENRE_WORDS.has(w) || seedWords.has(w)) continue
    if (extraStop && extraStop.has(w)) continue
    seen.add(w)
  }
  return seen
}
function mine(results) {
  for (const a of results.slice(0, 50)) {
    for (const w of words(a.trackName, null)) bump(w, 't')                        // title: light filter
    for (const w of words((a.description || '').slice(0, DESC_SCAN_CHARS), MARKETING_STOP)) bump(w, 'd')
  }
}
// composite rank score (title-weighted) and honest distinct-app count for a pool entry
const poolScore = e => e.t * 4 + e.d
const poolApps  = e => e.t + e.d   // upper bound on distinct apps (a word rarely sits in both a title AND its own desc after seed/stop filtering); close enough for display
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
    mine(results)
    perTerm.push({
      term,
      ...difficulty(results),
      myRank: myRank(results),
      genres: genreMix(results),
      top: results.slice(0, 5).map(a => ({
        name: a.trackName, ratings: a.userRatingCount || 0,
        stars: a.averageUserRating || 0, genre: a.primaryGenreName,
        url: a.trackViewUrl,   // the App Store page — used by the editor to link the name
      })),
    })
    await sleep(400)   // be polite to the API
  }
  // rank by title-weighted composite; keep a rich detail list + a back-compat [[word, appCount]] view
  const ranked = [...pool.entries()]
    .sort((a, b) => poolScore(b[1]) - poolScore(a[1]) || poolApps(b[1]) - poolApps(a[1]))
    .slice(0, 30)
  const minedDetail = ranked.map(([w, e]) => ({ word: w, apps: poolApps(e), title: e.t, desc: e.d }))
  const mined = minedDetail.map(m => [m.word, m.apps])   // editor + old consumers read [[w, count]]

  if (asJson) {
    console.log(JSON.stringify({ date: stamp, country, app: myApp, terms: perTerm, mined, minedDetail }, null, 2))
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
  console.log(`━━ mined keyword candidates (words the ranking apps use, not in your seeds)`)
  console.log(`   ★ = used in a competitor TITLE · ·N = # of the top competitors using it`)
  console.log('   ' + minedDetail.map(m => `${m.title ? '★' : ' '}${m.word}·${m.apps}`).join('  '))
  console.log(`\n(difficulty is a RELATIVE proxy — crowding × incumbent strength — not absolute`)
  console.log(` search volume. That column arrives with Apple's Search Term Rank report.)\n`)
})()
