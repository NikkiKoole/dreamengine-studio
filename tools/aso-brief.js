#!/usr/bin/env node
// aso-brief.js — generate a committed SEO WORKSHEET (apps/<name>/seo-brief.md) that you write
// your press.md + App Store listing AGAINST. It is a PALETTE, never the painting: it emits
// vocabulary + demand ranking + char budgets, and NEVER any prose. The copy stays 100% human —
// no AI-generated press kits (docs/design/store-agents.md §"palette + mirror"). Its twin is the
// MIRROR, aso-coverage.js, which checks the finished copy against this worksheet.
//
//   node tools/aso-brief.js tinyjam
//   node tools/aso-brief.js tinyjam --country nl
//   node tools/aso-brief.js tinyjam --seeds "drum machine, beat maker"   # override auto-seeds
//   node tools/aso-brief.js tinyjam --quick                              # faster suggest (no a-z soup)
//
// Seeds auto-derive from each cart's de:meta (teaches tags → category terms; titles as fallback)
// via the generated editor/public/carts/index.json — the same seeds the editor's 🔎/💡 use. It
// runs aso-research.js (competition side) + aso-suggest.js (Google demand side) and writes:
//   · char budgets + your current listing, with live counts
//   · STORE WORDS in priority order for the ≤100-char keyword field → feed aso-compose.js
//   · WEBSITE / PRESS PHRASES people actually google → work these into press.md, in your voice
//   · per-seed competition (difficulty band + strongest incumbent) so you know what's winnable
// Writes apps/<name>/seo-brief.md (committed, regenerable). A trailing <!-- aso-coverage ... -->
// block (invisible in rendered markdown) is the contract aso-coverage.js reads back.
'use strict'

const fs = require('fs')
const path = require('path')
const { spawnSync } = require('child_process')

const ROOT = path.join(__dirname, '..')
const SUGGEST_SEED_CAP = 5   // suggest's a-z soup is heavy; bound the seed count for the brief

// ── args ─────────────────────────────────────────────────────────────────────
const USAGE = 'usage: node tools/aso-brief.js <app> [--country cc] [--seeds "a,b"] [--quick]'
function die(msg) { console.error(msg + '\n' + USAGE); process.exit(1) }
const argv = process.argv.slice(2)
let app = null, country = 'us', quick = false, seedOverride = null
for (let i = 0; i < argv.length; i++) {
  const a = argv[i]
  if (a === '--country') country = argv[++i] || 'us'
  else if (a === '--seeds') seedOverride = argv[++i] || ''
  else if (a === '--quick') quick = true
  else if (a.startsWith('-')) die(`unknown flag: ${a}`)
  else if (!app) app = a
  else die(`unexpected arg: ${a}`)
}
if (!app) die('which app? e.g. node tools/aso-brief.js tinyjam')
country = country.toLowerCase()

const appDir = path.join(ROOT, 'apps', app)
const manifestPath = path.join(appDir, 'app.json')
if (!fs.existsSync(manifestPath)) die(`no manifest at apps/${app}/app.json`)
const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'))
const listing = manifest.listing || {}

// ── seeds: from de:meta teaches/titles (via index.json), or --seeds override ───
function deriveSeeds() {
  if (seedOverride != null) return seedOverride.split(',').map(s => s.trim()).filter(Boolean)
  const idx = JSON.parse(fs.readFileSync(path.join(ROOT, 'editor/public/carts/index.json'), 'utf8'))
  const byFile = new Map(idx.map(c => [c.file, c]))
  const norm = s => String(s || '').toLowerCase().replace(/[-_]+/g, ' ').replace(/\s+/g, ' ').trim()
  const teaches = [], titles = []
  for (const cart of (manifest.carts || [])) {
    const e = byFile.get(`${cart}.cart.png`)
    if (!e) continue
    for (const t of (e.teaches || [])) { const n = norm(t); if (n) teaches.push(n) }
    const ti = norm(e.title); if (ti) titles.push(ti)
  }
  const seen = new Set(), out = []
  for (const t of [...teaches, ...titles]) if (!seen.has(t)) { seen.add(t); out.push(t) }
  return out.slice(0, 8)
}
const seeds = deriveSeeds()
if (!seeds.length) die(`no seeds — apps/${app}/app.json has no carts with de:meta, and no --seeds given`)

// ── run the two research tools (JSON), degrade gracefully ──────────────────────
function runJson(script, args) {
  const r = spawnSync('node', [path.join(ROOT, 'tools', script), '--json', ...args],
    { cwd: ROOT, encoding: 'utf8', maxBuffer: 32 * 1024 * 1024 })
  if (r.status !== 0) return { error: (r.stderr || '').trim() || `exit ${r.status}` }
  try { return JSON.parse(r.stdout) } catch { return { error: 'unparseable output' } }
}
console.error(`aso-brief: seeds = ${seeds.join(', ')}`)
console.error(`aso-brief: running aso-research (competition)…`)
const research = runJson('aso-research.js', ['--country', country, ...seeds])
console.error(`aso-brief: running aso-suggest (Google demand)…`)
const sugSeeds = seeds.slice(0, SUGGEST_SEED_CAP)
const suggest = runJson('aso-suggest.js', ['--country', country, ...(quick ? ['--quick'] : []), ...sugSeeds])

// ── merge the store-word pool: rank by presence in BOTH sources ────────────────
// research.mined = words competitors TARGET; suggest.mined = words people SEARCH. A word in
// both is proven demand+relevance → rank it highest. Position-rank each list (scale-independent).
function rankMap(list) { const m = new Map(); (list || []).forEach(([w], i) => m.set(w, (list.length - i))); return m }
const rR = rankMap(research.mined), rS = rankMap(suggest.mined)
const visible = new Set((`${listing.title || ''} ${listing.subtitle || ''}`)
  .toLowerCase().split(/[^a-z0-9]+/).filter(Boolean))   // words already visible rank once already
const words = [...new Set([...rR.keys(), ...rS.keys()])].map(w => ({
  w, score: (rR.get(w) || 0) + (rS.get(w) || 0), both: rR.has(w) && rS.has(w), visible: visible.has(w),
})).sort((a, b) => (Number(b.both) - Number(a.both)) || (b.score - a.score))
const freshWords = words.filter(x => !x.visible).slice(0, 24)
const alreadyVisible = words.filter(x => x.visible).map(x => x.w)
const phrases = (suggest.phrases || []).map(p => p.phrase)

// ── write the worksheet ────────────────────────────────────────────────────────
const stamp = new Date().toISOString().slice(0, 10)
const L = s => String(s || '').length
const fitBar = (v, max) => `${L(v)}/${max}${L(v) > max ? '  ⚠ over' : ''}`
const both = w => words.find(x => x.w === w)?.both ? '★' : ''
const q = s => JSON.stringify(s)

let md = ''
md += `# SEO worksheet — ${manifest.name || app}\n\n`
md += `> **This is a palette, not the page.** Write \`press.md\` and the App Store listing in your\n`
md += `> own voice; reach here for the words the world actually uses. Nothing here is copy — it is\n`
md += `> never rendered into the press page. Regenerate: \`node tools/aso-brief.js ${app}\`. Check your\n`
md += `> finished copy against it: \`node tools/aso-coverage.js ${app}\` (the mirror).\n\n`
md += `_generated ${stamp} · country ${country} · seeds: ${seeds.join(', ')}_\n`
if (research.error) md += `\n> ⚠ research (competition) unavailable: ${research.error}\n`
if (suggest.error) md += `\n> ⚠ suggest (Google demand) unavailable: ${suggest.error}\n`
md += `\n---\n\n`

md += `## Char budgets & your current listing\n\n`
md += `| field | limit | yours | count |\n|---|---|---|---|\n`
md += `| Title | 30 | ${listing.title || '—'} | ${listing.title ? fitBar(listing.title, 30) : '—'} |\n`
md += `| Subtitle | 30 | ${listing.subtitle || '—'} | ${listing.subtitle ? fitBar(listing.subtitle, 30) : '—'} |\n`
md += `| Keywords | 100 | ${listing.keywords || '—'} | ${listing.keywords ? fitBar(listing.keywords, 100) : '—'} |\n\n`
md += `You rank on the UNION of the three — a word only needs to appear once. Title/subtitle read\n`
md += `like a person wrote them; the keyword field is the hidden word-soup.\n\n`

md += `## For the App Store keyword field — WORDS (priority order)\n\n`
md += `Apple auto-combines single words and ignores stopwords, so feed singles. ★ = the word is both\n`
md += `**searched** (Google demand) and **targeted** (a competitor uses it) — the strongest picks.\n\n`
if (freshWords.length) {
  md += freshWords.map(x => `- ${x.both ? '★ ' : ''}${x.w}`).join('\n') + '\n\n'
  md += `Paste into \`aso-compose\`:\n\n\`\`\`\nnode tools/aso-compose.js --title ${q(listing.title || '')} `
    + `--subtitle ${q(listing.subtitle || '')} \\\n  --candidates ${q(freshWords.map(x => x.w).join(','))}\n\`\`\`\n\n`
} else md += `_(no candidate words — research/suggest returned nothing; check the seeds)_\n\n`
if (alreadyVisible.length) md += `Already in your title/subtitle (don't repeat in keywords): ${alreadyVisible.join(', ')}\n\n`

md += `## For your website / press kit — PHRASES people google\n\n`
md += `Google ranks natural-language phrases, not word-soup — so these belong in \`press.md\` prose,\n`
md += `the page \`<title>\`/headings, and the meta description. **Work in the ones that fit, in your\n`
md += `own words** — don't paste them. (This is the demand side; the store field above is where the\n`
md += `bare keywords go.)\n\n`
if (phrases.length) md += phrases.map(p => `- ${p}`).join('\n') + '\n\n'
else md += `_(no phrases — suggest returned nothing; try category-term seeds, not brand names)_\n\n`

md += `## Competition — what's winnable\n\n`
if (research.terms && research.terms.length) {
  md += `| seed | difficulty | strongest incumbent |\n|---|---|---|\n`
  for (const t of research.terms) {
    if (t.error) { md += `| ${t.term} | (fetch failed) | — |\n`; continue }
    const top = (t.top && t.top[0]) ? `${t.top[0].name} (${t.top[0].ratings ? Math.round(t.top[0].ratings / 1000) + 'k' : '—'} ratings)` : '—'
    md += `| ${t.term} | ${t.band} ${t.score}/100 | ${top} |\n`
  }
  md += `\nEASY + relevant + low-authority = where a fresh app wins. HARD = crowded; skip unless core.\n\n`
} else md += `_(no competition data)_\n\n`

md += `---\n_worksheet regenerable; edit \`press.md\`, not this file. Terms drift — re-run before a launch pass._\n`

// drift tracking (docs/design/driftable-docs.md): stale-doc-check --driftable flags this when the
// seeds/manifest/tools move after `as-of`. NB the DOMINANT drift is live Google/App-Store search
// data, which no file-mtime can see — so re-run before a launch pass regardless. Single line (the
// scanner reads line-by-line, skipping code fences).
md += `\n<!-- de:driftable cmd="node tools/aso-brief.js ${app}" as-of="${stamp}" `
  + `inputs="tools/carts,apps/${app}/app.json,tools/aso-brief.js,tools/aso-research.js,tools/aso-suggest.js" watch="numbers" -->\n`

// hidden machine-readable contract for aso-coverage.js (invisible in rendered markdown)
const coverageData = { generated: stamp, country, seeds, phrases,
  words: freshWords.map(x => x.w), visible: alreadyVisible }
md += `\n<!-- aso-coverage\n${JSON.stringify(coverageData)}\n-->\n`

const outPath = path.join(appDir, 'seo-brief.md')
fs.writeFileSync(outPath, md)
console.error(`\n✓ wrote apps/${app}/seo-brief.md  (${freshWords.length} store words · ${phrases.length} phrases)`)
console.error(`  open it beside press.md, then: node tools/aso-coverage.js ${app}`)
