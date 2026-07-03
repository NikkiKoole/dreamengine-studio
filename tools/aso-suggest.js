#!/usr/bin/env node
// aso-suggest.js — free App Store DEMAND proxy + long-tail keyword discovery from Google
// Autocomplete (suggestqueries.google.com — no key, no auth, no account). The demand-side
// companion to aso-research.js (which measures the COMPETITION side), and the free stand-in
// for Apple's Search Term Rank popularity column until that beta reaches the account.
// See docs/design/store-agents.md §ASO ("some other free trick").
//
//   node tools/aso-suggest.js "drum machine" "groovebox"
//   node tools/aso-suggest.js --country nl "drummachine"
//   node tools/aso-suggest.js --json "beat maker" > snapshot.json     # machine-readable
//   node tools/aso-suggest.js --quick "sampler"                       # skip the a-z soup
//
// Flags:
//   --country <cc>   storefront/locale hint (gl+hl, default us) — nudges Google's results
//   --quick          bare seeds only (1 request each) instead of the a-z expansion
//   --json           emit a dated JSON snapshot instead of the human table
//
// HOW: for each seed it queries Google autocomplete for the bare seed AND seed + " " + a..z
// (the "alphabet soup" trick every pre-paid-tool keyword researcher used). Google orders its
// suggestions ~by query popularity, so a phrase's best POSITION plus how many expansions it
// shows up in = a DEMAND proxy. This is GOOGLE / cross-platform search intent, NOT App Store
// search volume — correlated, not identical (same honest-proxy posture as aso-research's
// difficulty score; absolute volume stays the paid tools' moat — store-agents.md §ASO).
//
// TIP: seed with CATEGORY terms ("drum machine", "step sequencer"), NOT a brand/product name
// — Google intent for a product name is whatever ELSE shares it (seeding "groovebox" surfaces a
// UK music festival, not the app). The editor's app-scoped 💡 seeds from each cart's de:meta
// `teaches` tags for exactly this reason.
//
// OUTPUT: the long-tail PHRASES real people type (ranked by the demand proxy) + a
// priority-ordered candidate WORD pool (phrases split, stopwords/seeds/platform-words
// dropped, demand-weighted) ready to paste straight into `aso-compose.js --candidates`.
'use strict'

const COUNTRY_DEFAULT = 'us'
const ALPHABET = 'abcdefghijklmnopqrstuvwxyz'.split('')
const REQ_SLEEP_MS = 90         // be polite to the endpoint
const POS_CAP = 10              // positions past this don't add popularity weight

// ── args ─────────────────────────────────────────────────────────────────────
const USAGE = 'usage: node tools/aso-suggest.js [--country cc] [--quick] [--json] "term" ...\n'
  + '  e.g. node tools/aso-suggest.js --country nl "poppen maken" "marionet"'
function die(msg) { console.error(msg + '\n' + USAGE); process.exit(1) }

const argv = process.argv.slice(2)
let country = COUNTRY_DEFAULT, quick = false, asJson = false
const seeds = []
for (let i = 0; i < argv.length; i++) {
  let a = argv[i]
  let inlineVal = null
  const eq = a.indexOf('=')
  if (a.startsWith('--') && eq !== -1) { inlineVal = a.slice(eq + 1); a = a.slice(0, eq) }
  const takeVal = () => inlineVal !== null ? inlineVal
    : (i + 1 < argv.length && !argv[i + 1].startsWith('-')) ? argv[++i]
    : die(`flag ${a} needs a value`)

  if (a === '--country') country = takeVal()
  else if (a === '--quick') quick = true
  else if (a === '--json') asJson = true
  else if (a.startsWith('-')) die(`unknown flag: ${argv[i]}`)
  else seeds.push(argv[i])
}
country = country.toLowerCase()
if (!/^[a-z]{2}$/.test(country)) die(`--country wants a 2-letter code (us, nl, de, …), got "${country}"`)
if (!seeds.length) die('no seed terms given — pass at least one, e.g. "drum machine"')

// Words that are noise as App-Store KEYWORD candidates: function words, App-Store fluff, the
// platform/download words Google autocomplete loves ("... app free download for windows"), and
// the WEB/GAMING/BRAND intent that leaks in because this is Google, not the App Store (xbox,
// minecraft, chrome, youtube, wikipedia, meaning …). Category NOUNS (synth, sampler,
// marionette, beat, jam) are deliberately absent, so they survive + rank.
const STOP = new Set(('the a an and or of to for in on at with your you my me it is are be as by '
  + 'from app apps free pro plus lite hd new best top maker make making play online download '
  + 'windows android ios ipad iphone ipod mac macbook pc apk mod version review reviews how what '
  + 'which vs like without under over near cheap paid buy get use using not no more most site com '
  // web / gaming / brand intent that is NOT App-Store demand (Google-specific leakage):
  + 'xbox playstation ps4 ps5 nintendo switch minecraft roblox fortnite steam '
  + 'youtube tiktok instagram facebook twitter spotify netflix google chrome safari firefox edge '
  + 'browser website web internet wiki wikipedia reddit amazon walmart ebay laptop computer '
  + 'desktop meaning definition synonym synonyms salary jobs job quiz crossword near unblocked '
  + 'sign signin signup login logout account easy quick simple hard tutorial tutorials guide '
  + 'guides guys quote quotes')
  .split(' '))
const seedWords = new Set(seeds.join(' ').toLowerCase().split(/[^a-z0-9]+/).filter(Boolean))

const sleep = ms => new Promise(r => setTimeout(r, ms))

async function suggest(q) {
  const url = 'https://suggestqueries.google.com/complete/search'
    + `?client=firefox&hl=${country}&gl=${country}&q=${encodeURIComponent(q)}`
  const r = await fetch(url, { headers: { 'User-Agent': 'dreamengine-aso/0.1' } })
  if (!r.ok) throw new Error(`HTTP ${r.status}`)
  const j = JSON.parse(await r.text())          // [query, [suggestions...], [], {subtypes}]
  return Array.isArray(j[1]) ? j[1] : []
}

// A phrase's demand weight from one appearance at list position `pos` (0 = top = most popular).
const posWeight = pos => Math.max(0, POS_CAP - Math.min(pos, POS_CAP))

;(async () => {
  const stamp = new Date().toISOString().slice(0, 10)
  const perSeed = []
  const wordScore = new Map()          // candidate word -> summed demand weight (across all seeds)

  for (const seed of seeds) {
    // "<seed> app" biases Google toward app-context suggestions; the a-z soup harvests the
    // long tail. --quick keeps just the two cheap app-framed probes.
    const queries = quick ? [seed, `${seed} app`]
      : [seed, `${seed} app`, ...ALPHABET.map(c => `${seed} ${c}`)]
    const phrases = new Map()          // phrase -> { freq, bestPos }
    let warn = null, done = 0
    for (const q of queries) {
      let hits
      try { hits = await suggest(q) }
      catch (e) { warn = `partial — Google returned ${e.message} after ${done} queries`; break }
      done++
      hits.forEach((phrase, pos) => {
        const p = String(phrase).toLowerCase().trim()
        if (!p || p === seed.toLowerCase()) return         // the seed itself isn't a discovery
        const e = phrases.get(p) || { freq: 0, bestPos: 99 }
        e.freq++; e.bestPos = Math.min(e.bestPos, pos)
        phrases.set(p, e)
      })
      await sleep(REQ_SLEEP_MS)
    }
    // score each phrase: broad appearance (freq) + how high it ever ranked (position)
    const ranked = [...phrases.entries()]
      .map(([phrase, e]) => ({ phrase, demand: e.freq + posWeight(e.bestPos) * 2, freq: e.freq, pos: e.bestPos }))
      .sort((a, b) => b.demand - a.demand)
    // feed the candidate WORD pool: each qualifying word inherits its phrase's demand
    for (const { phrase, demand } of ranked) {
      for (const w of phrase.split(/[^a-z0-9]+/)) {
        if (w.length < 3 || STOP.has(w) || seedWords.has(w)) continue
        wordScore.set(w, (wordScore.get(w) || 0) + demand)
      }
    }
    perSeed.push({ seed, queries: done, phrases: ranked.length, warn, top: ranked.slice(0, 12) })
  }

  // TWO outputs for TWO different search engines (they rank differently — see the header note
  // and docs/design/store-agents.md §ASO "phrases vs words"):
  //  · candidates = priority-ordered single WORDS → the App Store keyword field (Apple
  //    auto-combines singles + ignores stopwords), fed straight to aso-compose --candidates.
  const mined = [...wordScore.entries()].sort((a, b) => b[1] - a[1]).slice(0, 30)
    .map(([word, score]) => [word, Math.round(score)])
  const candidates = mined.map(([w]) => w)
  //  · phrases = the natural-language QUERIES people actually type → your WEBSITE / press-kit
  //    SEO (Google is a phrase/semantic engine: meta description, headings, copy), NOT the
  //    App Store field. Flattened + demand-ranked + deduped across seeds.
  const phraseAgg = new Map()
  for (const s of perSeed) for (const t of s.top) {
    const e = phraseAgg.get(t.phrase)
    if (!e || t.demand > e.demand) phraseAgg.set(t.phrase, t)
  }
  const phrases = [...phraseAgg.values()].sort((a, b) => b.demand - a.demand).slice(0, 20)

  if (asJson) {
    console.log(JSON.stringify({ date: stamp, country, source: 'google-autocomplete',
      seeds: perSeed, mined, candidates, phrases }, null, 2))
    return
  }

  console.log(`\nASO suggest · google-autocomplete · country=${country} · ${stamp}`
    + (quick ? ' · quick (bare seeds)' : '') + '\n')
  for (const s of perSeed) {
    console.log(`━━ "${s.seed}"  (${s.queries} queries · ${s.phrases} phrases`
      + (s.warn ? ` · ${s.warn}` : '') + ')')
    console.log('   ' + s.top.map(t => `${t.phrase}·${t.demand}`).join('  '))
    console.log()
  }
  console.log(`━━ for the APP STORE keyword field — single WORDS (Apple auto-combines them)`)
  console.log('   ' + mined.map(([w, c]) => `${w}·${c}`).join('  '))
  console.log(`   copy → ${candidates.join(',')}\n`)
  console.log(`━━ for your WEBSITE / PRESS KIT — natural-language PHRASES people google`)
  console.log(`   (meta description, headings, copy — Google ranks phrases, not word-soup)`)
  console.log('   ' + phrases.map(p => `${p.phrase}·${p.demand}`).join('  ') + '\n')
  console.log(`(demand is a RELATIVE proxy from Google autocomplete — cross-platform search`)
  console.log(` intent ordered by popularity, NOT App Store volume. The real 1–100 arrives`)
  console.log(` with Apple's Search Term Rank report. — docs/design/store-agents.md §ASO)\n`)
})()
