#!/usr/bin/env node
// build-history.js — generate docs/history.html, a dedicated readable timeline
// of how the project grew. STRUCTURE comes from docs/history-spine.json (eras,
// subsystems, importance tiers, marked days, the editorial seam); every DATE,
// commit, ADR and cart-birth fact is derived from git + index.json so the page
// stays honest with the repo. Surfaced in the editor's Docs tab (a "history"
// sidebar item loads it in an iframe; doc links postMessage the parent to open
// the rendered markdown).
//
//   node tools/build-history.js            # regenerate docs/history.html
//
// v1 scope = the spine's window (week 1). Re-run after editing the spine.

const fs = require('fs')
const path = require('path')
const { execFileSync } = require('child_process')

const REPO = path.resolve(__dirname, '..')
const SPINE = path.join(REPO, 'docs', 'history-spine.json')
const INDEX = path.join(REPO, 'editor', 'public', 'carts', 'index.json')
const OUT = path.join(REPO, 'docs', 'history.html')

const git = (args) =>
  execFileSync('git', args, { cwd: REPO, encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 })

// ---- load structure ----
const spine = JSON.parse(fs.readFileSync(SPINE, 'utf8'))
const carts = JSON.parse(fs.readFileSync(INDEX, 'utf8'))
const cartTitle = {}, cartDesc = {}, cartTeaches = {}, cartLineage = {}
for (const c of carts) if (c.file) {
  cartTitle[c.file] = c.title || c.file
  cartDesc[c.file] = c.description || ''
  cartTeaches[c.file] = Array.isArray(c.teaches) ? c.teaches : []
  cartLineage[c.file] = c.lineage || ''
}

const { from, to } = spine.window
// git --until is exclusive of the day boundary; bump one day past `to`
const untilExclusive = (() => {
  const d = new Date(to + 'T00:00:00Z'); d.setUTCDate(d.getUTCDate() + 1)
  return d.toISOString().slice(0, 10)
})()
const SINCE = `${from} 00:00`, UNTIL = `${untilExclusive} 00:00`

const dayOf = (iso) => iso.slice(0, 10)
const inWindow = (day) => day >= from && day <= to

// ---- gather commits in window (oldest first) ----
// record-separated (RS between commits, SEP between fields) so commit BODIES —
// which contain newlines — survive the parse. The body feeds milestone hovers.
const SEP = '\x1f', RS = '\x1e'
const cleanBody = (b) => (b || '')
  .replace(/^\s*Co-Authored-By:.*$/gim, '')        // drop the trailer
  .replace(/\r/g, '').replace(/\n{2,}/g, '\n').trim()
// turn a raw commit body into a friendlier blurb: drop the bullet markers and
// the "filename.ext:" file-scaffolding, flow it into one line, cap it. (A real
// prose summary needs a human — that's the optional authored `note`; this is the
// derived fallback so the hover reads like a sentence, not a git log.)
const summarizeBody = (b) => (b || '').split('\n')
  .map((l) => l.replace(/^[-*]\s*/, '').replace(/^[A-Za-z0-9_.\/-]+\.(?:c|h|js|cjs|json|html|css|md|sh):\s*/i, ''))
  .filter(Boolean).join(' · ').replace(/`/g, '').replace(/\s+/g, ' ').trim().slice(0, 240)
const rawCommits = git([
  'log', '--reverse', `--format=%H${SEP}%cI${SEP}%s${SEP}%b${RS}`,
  '--since', SINCE, '--until', UNTIL,
]).split(RS).map((r) => r.replace(/^\n/, '')).filter(Boolean).map((rec) => {
  const [hash, iso, subject, ...rest] = rec.split(SEP)
  return { hash, iso, day: dayOf(iso), subject, body: cleanBody(rest.join(SEP)) }
})

// ---- ADRs added in window ----
function addedInWindow(glob) {
  // newest-first; first sighting of each path is its most recent touch, but with
  // --diff-filter=A each path appears once at its add commit.
  const out = git(['log', '--diff-filter=A', '--format=C%cI', '--name-only', '--', glob])
  const res = []
  let day = null
  for (const line of out.split('\n')) {
    if (line.startsWith('C')) { day = dayOf(line.slice(1)); continue }
    const f = line.trim()
    if (!f || !day) continue
    // skip ghosts: a path ADDED in-window but since deleted (e.g. a cart folded
    // into another, like overpass→cityview) — it has no file to link or thumbnail.
    if (!fs.existsSync(path.join(REPO, f))) continue
    if (inWindow(day)) res.push({ day, file: f })
  }
  return res
}
const adrs = addedInWindow('docs/decisions/*.md')
  .filter((a) => /\/\d{4}-/.test(a.file))
  .map((a) => ({
    day: a.day,
    rel: a.file.replace(/^docs\//, ''),
    name: path.basename(a.file),
    label: path.basename(a.file).replace(/\.md$/, '').replace(/-/g, ' '),
  }))
  .sort((x, y) => x.name.localeCompare(y.name))

// ---- carts born in window ----
const cartsBorn = addedInWindow('editor/public/carts/*.cart.png')
  .map((c) => {
    const file = path.basename(c.file)
    return { day: c.day, file, title: cartTitle[file] || file }
  })

// ---- design/guide docs born in window (for "docs written" per era) ----
// exclude the per-game cart-specs/ batch (the editor's docs sidebar hides them
// too) — they're build briefs, not design notes, and there are ~70 of them.
const docsBorn = [
  ...addedInWindow('docs/design/*.md'),
  ...addedInWindow('docs/guides/*.md'),
].filter((d) => !d.file.includes('cart-specs'))
  .map((d) => ({
    day: d.day,
    rel: d.file.replace(/^docs\//, ''),
    name: path.basename(d.file).replace(/\.md$/, ''),
  }))

// ---- per-day commit counts ----
const dayCount = {}
for (const c of rawCommits) dayCount[c.day] = (dayCount[c.day] || 0) + 1

// ---- the "hero" cart per era: whichever cart was committed-to MOST during the
// era's window (the thing being worked hardest right then) — its thumbnail goes
// in as a magazine pull-image, inlined as a data URI so the page is self-contained.
const CARTS_DIR = path.join(REPO, 'editor', 'public', 'carts')
function heroByEra() {
  // count SOURCE (.c) commits only — a cart's .cart.png gets bulk-rebaked/published
  // in sweeps that touch dozens at once, which would inflate the "worked on" signal.
  const out = git(['log', '--format=C%cI', '--name-only',
    '--since', SINCE, '--until', UNTIL, '--', 'tools/carts'])
  const tally = {}            // eraId -> { cartName -> commit count }
  let era = null, seen = null
  for (const line of out.split('\n')) {
    if (line.startsWith('C')) {
      const e = eraOf(dayOf(line.slice(1))); era = e ? e.id : null; seen = new Set(); continue
    }
    if (!era) continue
    const m = line.match(/tools\/carts\/([A-Za-z0-9_-]+)\.c$/)
    if (!m || seen.has(m[1])) continue   // count a cart once per commit
    seen.add(m[1])
    const t = (tally[era] ||= {}); t[m[1]] = (t[m[1]] || 0) + 1
  }
  const heroes = {}
  for (const eid of Object.keys(tally)) {
    let best = null
    for (const [name, n] of Object.entries(tally[eid])) {
      if (!fs.existsSync(path.join(CARTS_DIR, name + '.cart.png'))) continue
      if (!best || n > best.n) best = { name, n }
    }
    if (best && best.n >= 2) {
      const b64 = fs.readFileSync(path.join(CARTS_DIR, best.name + '.cart.png')).toString('base64')
      heroes[eid] = { name: best.name, commits: best.n,
        title: cartTitle[best.name + '.cart.png'] || best.name,
        lineage: cartLineage[best.name + '.cart.png'] || '',
        dataUri: 'data:image/png;base64,' + b64 }
    }
  }
  return heroes
}
const heroes = heroByEra()

// ---- doc-content mining --------------------------------------------------
// The page used to read docs as names+dates only; this mines their CONTENT so
// design notes can enrich the story automatically. Two sources:
//  · docs/README.md — the curated map: every design/guide doc has a hand-written
//    one-liner (often STATUS-tagged). We reuse those as friendly summaries so
//    nothing is re-authored — keep README current and the page follows.
//  · each doc's body — scanned for the carts that reference it (≈ "carts that
//    grew from this note"), whitelisted against real cart names (backtick refs
//    in docs are mostly API symbols, not carts).
const readDoc = (rel) => { try { return fs.readFileSync(path.join(REPO, rel), 'utf8') } catch (e) { return '' } }

function parseDocMap() {
  const map = {}
  const txt = readDoc('docs/README.md')
  // tree lines like:  │   ├── second-north-star.md  REFLECTION (...): ...
  const re = /^[│\s]*[├└]──\s+([A-Za-z0-9_.-]+\.md)\s{2,}(.+?)\s*$/
  for (const line of txt.split('\n')) {
    const m = line.match(re)
    if (!m) continue
    const desc = m[2].trim()
    // leading status tag = ALL-CAPS word(s) (or a ★) before the first :/(/—
    let status = null
    const sm = desc.replace(/^★\s*/, '').match(/^([A-Z][A-Z0-9 ]{2,}?)(?=[:(—-]|\s|$)/)
    if (sm && sm[1].trim().length >= 3) status = sm[1].trim()
    map[m[1]] = { desc, status }
  }
  return map
}
const docMap = parseDocMap()

// cart-name index — match doc text against REAL cart names only
const cartNameToFile = {}
for (const f of Object.keys(cartTitle)) cartNameToFile[f.replace(/\.cart\.png$/, '')] = f
const cartNames = Object.keys(cartNameToFile)
const cartNameRe = cartNames.length
  ? new RegExp('(`?)\\b(' + cartNames.slice().sort((a, b) => b.length - a.length)
      .map((n) => n.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')).join('|') + ')(\\.c)?\\b(`?)', 'g')
  : null
const hasThumb = (name) => fs.existsSync(path.join(CARTS_DIR, name + '.cart.png'))

// carts a blob references — counted ONLY on a strong signal: a `.c` suffix or a
// `backtick`-wrapped name (how docs actually cite carts). Bare prose words are
// ignored, so common-word cart names (needs/house/patterns/effects…) don't
// false-fire. Returns names ranked by weight, only carts that have a thumbnail.
function cartsReferencedIn(text, selfName) {
  if (!cartNameRe || !text) return []
  const counts = {}
  let m; cartNameRe.lastIndex = 0
  while ((m = cartNameRe.exec(text))) {
    const tick = m[1] === '`' || m[4] === '`', name = m[2], dotC = !!m[3]
    if (name === selfName || !(dotC || tick)) continue
    if (!hasThumb(name)) continue
    counts[name] = (counts[name] || 0) + (dotC ? 2 : 1) + (tick ? 1 : 0)
  }
  return Object.entries(counts).sort((a, b) => b[1] - a[1]).map(([n]) => n)
}

// enrich ADRs: real H1 title + a friendly first-paragraph, and flag the CUT ones
// (the "roads not taken" — deliberate negative space the page otherwise omits)
for (const a of adrs) {
  const txt = readDoc('docs/' + a.rel)
  const h1 = txt.match(/^#\s+(.+)$/m)
  a.title = h1 ? h1[1].replace(/^\d+\s*[—-]\s*/, '').replace(/`/g, '').trim() : a.label
  const ctx = (txt.split(/^##\s+/m)[1] || '').replace(/^[^\n]*\n/, '')   // body after "## Context"
  a.summary = (ctx.split(/\n\s*\n/)[0] || '').replace(/\s+/g, ' ').replace(/[`*\[\]]/g, '').trim().slice(0, 240)
  a.cut = /\bcut\b/i.test(a.name) || /^cut\b/i.test(a.title)
  a.voiced = (spine.decisionNotes || {})[a.name.replace(/\.md$/, '')] || ''
}

// enrich docs-born: README one-liner + the carts that grew from each
for (const d of docsBorn) {
  const meta = docMap[d.name + '.md'] || {}
  d.desc = meta.desc || ''
  d.status = meta.status || ''
  d.relatedCarts = cartsReferencedIn((d.desc || '') + '\n' + readDoc('docs/' + d.rel), d.name).slice(0, 8)
}

// tools built in window — the toolsmith trail (make-cart → the linters → the web
// build → the audio-analysis suite). Description = the file's first header comment
// after any shebang. Excludes *.cart.js configs and tools/carts/. All from git, so
// the row self-refreshes as tools land.
const toolDesc = (rel) => {
  for (const line of readDoc(rel).split('\n').slice(0, 8)) {
    const m = line.match(/^\s*(?:\/\/|#)\s?(.+)$/)
    if (!m || m[1].startsWith('!')) continue          // skip the shebang (#!)
    return m[1].replace(/^[a-z0-9_.-]+\.(?:js|sh)\s*[—-]\s*/i, '').replace(/`/g, '').trim().slice(0, 130)
  }
  return ''
}
const toolsBorn = addedInWindow('tools/*.js').concat(addedInWindow('tools/*.sh'))
  .filter((t) => /^tools\/[^/]+\.(js|sh)$/.test(t.file) && !/\.cart\.js$/.test(t.file))
  .map((t) => ({ day: t.day, name: path.basename(t.file), desc: toolDesc(t.file) }))

// ---- subsystem tagging ----
const subMatch = spine.subsystems.map((s) => ({
  id: s.id,
  needles: (s.match || []).map((m) => m.toLowerCase()),
}))
function subsystemsFor(subject) {
  const s = subject.toLowerCase()
  const hits = []
  for (const sm of subMatch) if (sm.needles.some((n) => s.includes(n))) hits.push(sm.id)
  return hits
}
for (const c of rawCommits) c.subs = subsystemsFor(c.subject)

// ---- assign commits to eras (by day) ----
function eraOf(day) { return spine.eras.find((e) => day >= e.from && day <= e.to) }
for (const c of rawCommits) { const e = eraOf(c.day); c.era = e ? e.id : null }

// ---- resolve milestones to dates (first matching commit) ----
const milestones = spine.milestones.map((m) => {
  const needle = m.match.toLowerCase()
  const hit = rawCommits.find((c) => c.subject.toLowerCase().includes(needle))
  // the hover blurb: an authored `note` (voiced) wins; else the commit's own
  // subject as the friendly headline + a cleaned-up summary of its body. All
  // self-refreshing from git unless a note is written.
  const gist = m.note || (hit ? hit.subject : '')
  const detail = m.note ? '' : (hit ? summarizeBody(hit.body) : '')
  return { ...m, day: hit ? hit.day : null, matched: !!hit, gist, detail }
})
const missing = milestones.filter((m) => !m.matched)
if (missing.length) {
  console.warn('[build-history] WARNING — milestone matchers with no commit:')
  for (const m of missing) console.warn(`    "${m.title}"  (match: "${m.match}")`)
}

// ---- assemble per-era data ----
const eras = spine.eras.map((e) => {
  const days = []
  for (let d = new Date(e.from + 'T00:00:00Z'); dayOf(d.toISOString()) <= e.to; d.setUTCDate(d.getUTCDate() + 1)) {
    const day = dayOf(d.toISOString())
    days.push({
      day,
      count: dayCount[day] || 0,
      marked: spine.markedDays && spine.markedDays[day] ? spine.markedDays[day] : null,
      dense: (dayCount[day] || 0) >= (spine.denseDayThreshold || 9999),
      commits: rawCommits.filter((c) => c.day === day),
    })
  }
  const eraDocs = docsBorn.filter((d) => eraOf(d.day) && eraOf(d.day).id === e.id)

  // spotlight: a design note to feature beside the hero cart — spine override
  // (e.spotlightDoc), else the era's doc that grew the most carts. Inline up to
  // 4 related cart thumbs (data URIs, like the hero) so the page stays self-contained.
  let pick = null
  if (e.spotlightDoc) pick = eraDocs.find((d) => d.name === e.spotlightDoc.replace(/\.md$/, ''))
  if (!pick) {
    pick = eraDocs.filter((d) => d.relatedCarts.length)
      .sort((a, b) => b.relatedCarts.length - a.relatedCarts.length || b.desc.length - a.desc.length)[0] || null
  }
  let spotlight = null
  if (pick) {
    const carts = pick.relatedCarts.slice(0, 4).map((name) => {
      const file = name + '.cart.png'
      if (!hasThumb(name)) return null
      return { name, file, title: cartTitle[file] || name, lineage: cartLineage[file] || '',
        dataUri: 'data:image/png;base64,' + fs.readFileSync(path.join(CARTS_DIR, file)).toString('base64') }
    }).filter(Boolean)
    spotlight = { rel: pick.rel, name: pick.name, status: pick.status, desc: pick.desc,
      carts, relatedCount: pick.relatedCarts.length }
  }

  return {
    ...e,
    days,
    commitCount: rawCommits.filter((c) => c.era === e.id).length,
    milestones: milestones.filter((m) => m.era === e.id),
    adrs: adrs.filter((a) => inWindow(a.day) && eraOf(a.day) && eraOf(a.day).id === e.id),
    docs: eraDocs,
    cartsBorn: cartsBorn.filter((c) => eraOf(c.day) && eraOf(c.day).id === e.id),
    toolsBorn: toolsBorn.filter((t) => eraOf(t.day) && eraOf(t.day).id === e.id),
    hero: heroes[e.id] || null,
    spotlight,
  }
})

// title + description (from index.json) for every cart born in-window — feeds
// the hover card on the "carts born" lists
const cartMeta = {}
for (const c of cartsBorn) if (!cartMeta[c.file]) cartMeta[c.file] = {
  title: c.title, desc: cartDesc[c.file] || '',
  lineage: cartLineage[c.file] || '', teaches: cartTeaches[c.file] || [],
}

// ---- research threads: topics that spawned a trail of docs/handoffs/carts ----
// declared by topic in the spine (like subsystems); members are auto-collected
// by filename match, the carts unioned from the per-doc strong-signal scan.
const inlineThumb = (name) => {
  const file = name + '.cart.png'
  if (!hasThumb(name)) return null
  return { name, file, title: cartTitle[file] || name, lineage: cartLineage[file] || '',
    dataUri: 'data:image/png;base64,' + fs.readFileSync(path.join(CARTS_DIR, file)).toString('base64') }
}
const threads = ((spine.threads && spine.threads.items) || []).map((t) => {
  const terms = (t.match || []).map((s) => s.toLowerCase())
  const matches = (name) => terms.some((term) => name.toLowerCase().includes(term))
  const memDocs = docsBorn.filter((d) => matches(d.name))
  const memAdrs = adrs.filter((a) => matches(a.name))
  const notes = memDocs.filter((d) => !/handoff/i.test(d.name))
  const handoffs = memDocs.filter((d) => /handoff/i.test(d.name))
  // carts: union of each member doc's related carts, ranked by how many docs cite each
  const cc = {}
  memDocs.forEach((d) => (d.relatedCarts || []).forEach((n) => { cc[n] = (cc[n] || 0) + 1 }))
  const ranked = Object.entries(cc).sort((a, b) => b[1] - a[1]).map(([n]) => n)
  const carts = ranked.slice(0, 6).map(inlineThumb).filter(Boolean)
  const days = [...memDocs.map((d) => d.day), ...memAdrs.map((a) => a.day)].filter(Boolean).sort()
  const lite = (d) => ({ name: d.name, rel: d.rel, desc: d.desc || '' })
  return {
    id: t.id, title: t.title, blurb: t.blurb || '', seed: t.seed || null,
    notes: notes.map(lite), handoffs: handoffs.map(lite),
    decisions: memAdrs.map((a) => ({ name: a.name, rel: a.rel, title: a.title, cut: a.cut })),
    carts, from: days[0] || null, to: days[days.length - 1] || null,
    docCount: memDocs.length + memAdrs.length, cartCount: ranked.length,
  }
}).filter((t) => t.docCount)

// group eras into weeks (by date) + per-week growth stats — the TOC reads off this
const weeks = (spine.weeks || []).map((w) => ({
  ...w,
  commits: rawCommits.filter((c) => c.day >= w.from && c.day <= w.to).length,
  cartsBorn: cartsBorn.filter((c) => c.day >= w.from && c.day <= w.to).length,
  eras: eras.filter((e) => e.from >= w.from && e.from <= w.to),
})).filter((w) => w.eras.length)

// ---- knowledge loop: each field note's insight → the artifact it became (its Outcome) ----
let knowledgeLoop = []
try {
  const { collectNotes } = require('./build-field-notes.js')
  const FN = path.join(REPO, 'docs/field-notes')
  for (const n of collectNotes().journal) {
    const txt = fs.readFileSync(path.join(FN, n.file), 'utf8')
    const m = txt.match(/##\s*Outcome[^\n]*\n+([\s\S]*?)(?:\n#{1,6}\s|$)/)
    const outcome = m ? m[1].replace(/\s+/g, ' ').trim() : ''
    if (outcome) knowledgeLoop.push({ num: n.num, title: n.title, status: n.status, rel: 'field-notes/' + n.file, outcome })
  }
} catch (e) { /* journal optional */ }

// ---- negative space: the decisions that CUT or DEFERRED something ----
const negativeSpace = adrs
  .filter(a => /\b(cut|defer|deprecat|superseded)\b/i.test(a.name + ' ' + (a.title || '')))
  .map(a => ({ name: a.name, title: a.title, rel: a.rel }))

// ---- design lifecycle: phase counts (designed → shipped) ----
let designPhases = null
try {
  const { collectBoard } = require('./design-board.js')
  designPhases = {}
  for (const e of collectBoard().board) if (e.phase) designPhases[e.phase] = (designPhases[e.phase] || 0) + 1
} catch (e) { /* board optional */ }

const data = {
  title: spine.title,
  subtitle: spine.subtitle,
  window: spine.window,
  generatedFrom: { commits: rawCommits.length, adrs: adrs.length, cartsBorn: cartsBorn.length },
  subsystems: spine.subsystems,
  cartMeta,
  weeks,
  threads,
  observations: spine.observations || null,
  knowledgeLoop,
  negativeSpace,
  designPhases,
}

// ---- render HTML (CSS/JS consts are defined below; write at end of module) ----

// =====================================================================

function esc(s) {
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
}

function renderHtml(d) {
  const json = JSON.stringify(d).replace(/</g, '\\u003c')
  return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${esc(d.title)}</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Archivo+Black&family=Space+Grotesk:wght@400;500;700&display=swap" rel="stylesheet">
<style>${CSS}</style>
</head>
<body>
<div id="root"></div>
<script>const DATA = ${json};</script>
<script>${JS}</script>
</body>
</html>
`
}

const CSS = `
/* neo-brutalism: thick black edges, hard offset shadows, flat bold colour,
   chunky display type — tuned to the editor's dark palette + pink accent,
   with orange + yellow carrying the importance tiers. */
:root{
  --bg:#1b1c1f; --panel:#22232a; --panel2:#2a2c34; --ink:#e7e9ee; --dim:#9094a2;
  --edge:#000; --pink:#ff6fb5; --orange:#ff7a2f; --yellow:#ffd23f;
  --on:#141414;                         /* text on bright fills */
  --sh:6px 6px 0 var(--edge);           /* the signature hard shadow */
  --sh-sm:4px 4px 0 var(--edge);
  --disp:"Archivo Black",Impact,system-ui,sans-serif;
  --body:"Space Grotesk",ui-sans-serif,system-ui,sans-serif;
  --mono:ui-monospace,"SF Mono",Menlo,monospace;
}
*{box-sizing:border-box}
html,body{margin:0;background:var(--bg);color:var(--ink);font-family:var(--body);
  font-size:14px;line-height:1.55;overflow-x:hidden;
  background-image:radial-gradient(var(--panel2) 1px,transparent 1px);
  background-size:22px 22px}
a{color:var(--ink);text-decoration:none}
#root{max-width:1000px;margin:0 auto;padding:0 24px 140px}

header.top{padding:44px 0 14px}
header.top h1{font-family:var(--disp);margin:0 0 12px;font-size:46px;line-height:.98;
  letter-spacing:-1px;text-transform:uppercase;color:var(--ink);
  text-shadow:4px 4px 0 var(--pink)}
header.top .sub{color:var(--ink);font-size:15px;max-width:680px;font-weight:500}
header.top .meta{display:inline-block;color:var(--on);background:var(--yellow);
  border:3px solid var(--edge);box-shadow:var(--sh-sm);font-size:12px;
  margin-top:16px;padding:6px 12px;font-weight:700;font-family:var(--mono);
  transform:rotate(var(--rot,0deg))}
header.top .meta b{font-family:var(--disp);font-weight:400}

.legend{display:flex;flex-wrap:wrap;gap:8px;margin:22px 0 4px;align-items:center}
.legend .lbl{color:var(--dim);font-size:12px;margin-right:2px;text-transform:uppercase;
  letter-spacing:.5px;font-weight:700}
.chip{display:inline-flex;align-items:center;gap:6px;background:var(--panel);
  border:2px solid var(--edge);box-shadow:2px 2px 0 var(--edge);padding:3px 10px;
  font-size:12px;font-weight:600}
.chip .dot{width:9px;height:9px;border:2px solid var(--edge)}
.tier-foundational .dot,.dot.foundational{background:var(--pink)}
.tier-mid .dot,.dot.mid{background:var(--orange)}
.tier-small .dot,.dot.small{background:var(--yellow)}

nav.ribbon{position:sticky;top:0;z-index:5;background:var(--bg);
  border-bottom:3px solid var(--edge);
  display:flex;flex-wrap:wrap;gap:10px;padding:14px 0;margin:16px 0 8px}
nav.ribbon a{display:inline-flex;align-items:center;gap:8px;background:var(--panel);
  border:3px solid var(--edge);box-shadow:var(--sh-sm);padding:7px 13px;
  color:var(--ink);font-size:13px;font-weight:700;transition:transform .08s,box-shadow .08s}
nav.ribbon a:hover{transform:translate(-2px,-2px);box-shadow:6px 6px 0 var(--pink)}
nav.ribbon a:active{transform:translate(2px,2px);box-shadow:1px 1px 0 var(--edge)}
nav.ribbon a .n{background:var(--pink);color:var(--on);border:2px solid var(--edge);
  padding:0 6px;font-family:var(--mono);font-size:11px;font-weight:700}

/* table of contents — the weeks listed up front, each with its growth bar +
   chapter chips (the chips reuse the ribbon look). The grouping IS the TOC. */
nav.toc{margin:18px 0 8px;border:3px solid var(--edge);box-shadow:var(--sh);background:var(--panel);padding:14px 16px 16px}
nav.toc .toc-title{font-family:var(--disp);text-transform:uppercase;font-size:13px;margin-bottom:12px;color:var(--ink)}
.toc-week{padding:14px 0;border-top:3px dashed var(--panel2)}
.toc-week:first-of-type{border-top:none;padding-top:2px}
.tw-head{display:inline-flex;align-items:baseline;gap:12px;flex-wrap:wrap;text-decoration:none}
.tw-label{font-family:var(--disp);font-size:20px;text-transform:uppercase;color:var(--on);
  border:2px solid var(--edge);box-shadow:2px 2px 0 var(--edge);padding:2px 11px;line-height:1.15}
.tw-tag{font-style:italic;color:var(--ink);font-size:13.5px}
.tw-bar{margin:10px 0 6px;height:14px;background:var(--bg);border:2px solid var(--edge);max-width:520px}
.tw-bar span{display:block;height:100%;background:repeating-linear-gradient(45deg,var(--pink) 0 8px,var(--orange) 8px 16px)}
.tw-stats{font-family:var(--mono);font-size:11.5px;color:var(--dim);margin-bottom:10px}
.tw-stats b{color:var(--ink)}
.tw-eras{display:flex;flex-wrap:wrap;gap:9px}
.tw-eras a{display:inline-flex;align-items:center;gap:8px;background:var(--bg);border:3px solid var(--edge);
  box-shadow:var(--sh-sm);padding:6px 12px;color:var(--ink);font-size:13px;font-weight:700;
  transition:transform .08s,box-shadow .08s}
.tw-eras a:hover{transform:translate(-2px,-2px);box-shadow:6px 6px 0 var(--pink)}
.tw-eras a:active{transform:translate(2px,2px);box-shadow:1px 1px 0 var(--edge)}
.tw-eras a .ec{background:var(--pink);color:var(--on);border:2px solid var(--edge);
  padding:0 6px;font-family:var(--mono);font-size:11px;font-weight:700}

/* week band — separates the weeks as you scroll the body */
section.week-band{margin:54px 0 0;scroll-margin-top:16px}
.wb-head{display:flex;align-items:baseline;gap:14px;flex-wrap:wrap}
.wb-head h2{font-family:var(--disp);font-size:40px;margin:0;text-transform:uppercase;color:var(--ink);letter-spacing:-1px}
.wb-range{font-family:var(--mono);color:var(--dim);font-size:13px;font-weight:700}
.wb-tag{font-style:italic;color:var(--ink);font-size:15px}
.wb-bar{margin:14px 0 8px;height:18px;background:var(--panel);border:3px solid var(--edge);box-shadow:var(--sh-sm);max-width:600px}
.wb-bar span{display:block;height:100%;background:repeating-linear-gradient(45deg,var(--pink) 0 10px,var(--orange) 10px 20px)}
.wb-stats{font-family:var(--mono);font-size:12px;color:var(--dim)}
.wb-stats b{color:var(--ink)}

section.era{padding:40px 0 8px;margin-top:24px;scroll-margin-top:78px}
.era-head{display:flex;align-items:center;gap:14px;flex-wrap:wrap}
.era-head h2{font-family:var(--disp);margin:0;font-size:27px;text-transform:uppercase;
  letter-spacing:-.5px;background:var(--pink);color:var(--on);
  border:3px solid var(--edge);box-shadow:var(--sh);padding:6px 14px;line-height:1.05}
.era-head .dates{font-family:var(--mono);color:var(--ink);font-size:13px;font-weight:700}
.era-head .cc{margin-left:auto;font-family:var(--mono);color:var(--on);
  background:var(--ink);padding:3px 9px;border:2px solid var(--edge);font-size:11px;font-weight:700}
.blurb{color:var(--ink);margin:16px 0 4px;max-width:780px;font-size:14.5px;font-weight:500}
.editorial{border:3px solid var(--edge);border-left-width:10px;border-left-color:var(--pink);
  box-shadow:var(--sh-sm);background:var(--panel);padding:12px 16px;margin:14px 0;
  color:var(--ink);font-style:italic;max-width:780px;transform:rotate(var(--rot,0deg))}

.era-intro{display:flex;gap:26px;align-items:flex-start;margin-top:16px;flex-wrap:wrap}
.intro-left{flex:1 1 320px;min-width:300px}
.era-intro .blurb{margin-top:0}
figure.hero{flex:0 0 224px;margin:0;background:var(--panel);border:3px solid var(--edge);
  box-shadow:var(--sh);padding:9px;transform:rotate(1.4deg);transition:transform .12s;cursor:pointer}
figure.hero:hover{transform:rotate(0deg) translate(-2px,-2px)}
figure.hero img{display:block;width:100%;image-rendering:pixelated;border:2px solid var(--edge);background:#000}
figure.hero figcaption{margin-top:9px;line-height:1.25}
figure.hero .hk{display:inline-block;background:var(--orange);color:var(--on);
  border:2px solid var(--edge);font-family:var(--mono);font-size:9.5px;font-weight:700;
  text-transform:uppercase;letter-spacing:.5px;padding:1px 6px;margin-bottom:5px}
figure.hero b{display:block;font-family:var(--disp);font-size:14px;text-transform:uppercase;letter-spacing:-.3px}
figure.hero .hc{font-family:var(--mono);color:var(--dim);font-size:10.5px}
figure.hero .hl{margin-top:6px;font-size:11px;line-height:1.4;color:var(--ink);
  font-style:italic;font-weight:500;
  overflow:hidden;display:-webkit-box;-webkit-box-orient:vertical;-webkit-line-clamp:6}

.mstones{display:grid;grid-template-columns:repeat(auto-fill,minmax(240px,1fr));gap:16px;margin:22px 0}
.ms{background:var(--panel);border:3px solid var(--edge);box-shadow:var(--sh);padding:13px 15px;
  transform:rotate(var(--rot,0deg));transition:transform .1s,box-shadow .1s}
.ms:hover{transform:rotate(0deg) translate(-2px,-2px);box-shadow:8px 8px 0 var(--edge)}
.ms.foundational{background:var(--pink);color:var(--on);grid-column:span 2}
.ms.mid{border-top:9px solid var(--orange)}
.ms.small{border-top:9px solid var(--yellow)}
.ms .date{font-family:var(--mono);font-size:11px;font-weight:700;opacity:.75;font-variant-numeric:tabular-nums}
.ms .t{font-weight:700;margin:4px 0 3px;font-size:15px;line-height:1.2}
.ms.foundational .t{font-family:var(--disp);font-weight:400;font-size:17px;text-transform:uppercase;letter-spacing:-.3px}
.ms .sx{font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:.4px;opacity:.7}
.ms.foundational .sx{opacity:.85}
.ms .nomatch{color:var(--orange);font-size:11px;font-weight:700}
.ms[data-gist]{cursor:help}
@media(max-width:560px){.ms.foundational{grid-column:span 1}}

/* milestone hover popup — a friendly blurb about what landed */
.ms-pop{position:fixed;left:0;top:0;z-index:55;pointer-events:none;display:none;max-width:340px;
  background:var(--panel);border:3px solid var(--edge);box-shadow:var(--sh);padding:10px 12px;transform:rotate(-1deg)}
.ms-pop.show{display:block}
.ms-pop .msp-s{font-weight:700;font-size:12.5px;line-height:1.4;color:var(--ink)}
.ms-pop .msp-b{margin-top:7px;font-size:11px;line-height:1.5;color:var(--dim)}

.row{margin:18px 0}
.row > .h{font-family:var(--disp);font-size:13px;text-transform:uppercase;letter-spacing:0;
  color:var(--ink);margin-bottom:9px}
.tags{display:flex;flex-wrap:wrap;gap:8px}
.tag{background:var(--panel);border:2px solid var(--edge);box-shadow:2px 2px 0 var(--edge);
  padding:3px 10px;font-size:12px;font-weight:600;cursor:pointer;transition:transform .08s,box-shadow .08s}
.tag:hover{transform:translate(-1px,-1px);box-shadow:3px 3px 0 var(--yellow)}
.tag:active{transform:translate(2px,2px);box-shadow:0 0 0 var(--edge)}

details{background:var(--panel);border:3px solid var(--edge);box-shadow:var(--sh-sm);margin:16px 0;overflow:hidden;
  transform:rotate(var(--rot,0deg))}
details > summary{cursor:pointer;padding:11px 15px;list-style:none;font-size:13px;font-weight:700;
  display:flex;align-items:center;gap:10px;user-select:none;text-transform:uppercase;letter-spacing:.3px}
details > summary::-webkit-details-marker{display:none}
details > summary:hover{background:var(--panel2)}
details[open] > summary{border-bottom:3px solid var(--edge)}
details > summary .caret{transition:transform .12s;color:var(--pink)}
details[open] > summary .caret{transform:rotate(90deg)}
details > summary .grow{margin-left:auto;font-family:var(--mono);color:var(--on);
  background:var(--yellow);border:2px solid var(--edge);padding:1px 8px;font-size:11px;letter-spacing:0}
.detail-body{padding:6px 15px 15px}

.day{padding:12px 0;border-top:3px dashed var(--panel2)}
.day:first-child{border-top:none}
.day .dh{display:flex;align-items:center;gap:10px;font-size:13px}
.day .dd{font-family:var(--disp);font-size:15px}
.day .dc{font-family:var(--mono);color:var(--dim);font-size:12px}
.badge{font-size:11px;font-weight:700;padding:1px 8px;border:2px solid var(--edge);color:var(--on)}
.badge.dense{background:var(--orange)}
.badge.marked{background:var(--pink)}
.day .note{color:var(--ink);font-size:12.5px;font-style:italic;margin:5px 0 7px;
  border-left:6px solid var(--pink);padding-left:10px;font-weight:500}
ul.commits{margin:7px 0 0;padding:0;list-style:none}
ul.commits li{padding:2px 0 2px 18px;font-size:12.5px;color:var(--ink);position:relative}
ul.commits li:before{content:"▪";position:absolute;left:2px;color:var(--pink)}
ul.commits li .sx{color:var(--dim);font-size:11px;font-family:var(--mono)}

ul.carts{margin:7px 0 0;padding:0;list-style:none;columns:2;column-gap:28px}
ul.carts li{font-size:12.5px;padding:2px 0;break-inside:avoid}
ul.carts li[data-cart]{cursor:pointer}
ul.carts li[data-cart]:hover{color:var(--pink)}
ul.carts li .f{color:var(--dim);font-family:var(--mono);font-size:11px}
@media(max-width:560px){ul.carts{columns:1}}

.cart-preview{position:fixed;left:0;top:0;z-index:50;pointer-events:none;display:none;width:248px;
  background:var(--panel);border:3px solid var(--edge);box-shadow:var(--sh);padding:6px;transform:rotate(-1.6deg)}
.cart-preview.show{display:block}
.cart-preview img{display:block;width:100%;image-rendering:pixelated;border:2px solid var(--edge);background:#000}
.cart-preview img[src=""],.cart-preview img:not([src]){display:none}
.cart-preview .cp-meta{padding:7px 3px 2px}
.cart-preview .cp-t{display:block;font-family:var(--disp);font-size:13px;text-transform:uppercase;letter-spacing:-.2px;margin-bottom:4px}
.cart-preview .cp-d{display:block;font-size:11.5px;line-height:1.4;color:var(--dim);
  overflow:hidden;display:-webkit-box;-webkit-box-orient:vertical;-webkit-line-clamp:7}
/* lineage — what the cart descends from, italic; sits above the description */
.cart-preview .cp-lin{display:block;font-size:11px;line-height:1.4;color:var(--ink);
  font-style:italic;font-weight:500;margin-bottom:5px;
  overflow:hidden;-webkit-box-orient:vertical;-webkit-line-clamp:4;display:-webkit-box}
/* teaches — controlled-vocabulary technique chips */
.cart-preview .cp-teach{margin-top:7px;display:flex;flex-wrap:wrap;gap:4px}
.cart-preview .cp-teach i{font-style:normal;font-family:var(--mono);font-size:9.5px;font-weight:600;
  background:var(--bg);border:2px solid var(--edge);color:var(--ink);padding:1px 5px}

.totop{position:fixed;right:18px;bottom:18px;z-index:60;display:none;width:44px;height:44px;
  background:var(--pink);color:var(--on);border:3px solid var(--edge);box-shadow:var(--sh-sm);
  font-family:var(--disp);font-size:20px;line-height:1;cursor:pointer;
  transition:transform .08s,box-shadow .08s}
.totop.show{display:block}
.totop:hover{transform:translate(-2px,-2px);box-shadow:6px 6px 0 var(--edge)}
.totop:active{transform:translate(2px,2px);box-shadow:1px 1px 0 var(--edge)}
/* design-note spotlight — a foldout sibling to the hero: yellow accent edge,
   a STATUS badge, and the carts that grew from the note as little thumbnails. */
details.docspot{border-left-width:10px;border-left-color:var(--yellow);max-width:840px}
details.docspot > summary{gap:9px;flex-wrap:wrap}
.docspot .dsb{background:var(--yellow);color:var(--on);border:2px solid var(--edge);
  font-family:var(--mono);font-size:10px;font-weight:700;text-transform:uppercase;letter-spacing:.5px;padding:2px 7px}
.docspot .dst{font-family:var(--disp);font-weight:400;text-transform:uppercase;letter-spacing:-.2px;font-size:15px}
.ds-desc{color:var(--ink);font-size:13px;line-height:1.5;margin:4px 0 12px}
.ds-carts{display:flex;flex-wrap:wrap;gap:12px;margin:8px 0 14px}
figure.ds-cart{margin:0;width:120px;background:var(--bg);border:3px solid var(--edge);box-shadow:var(--sh-sm);
  padding:6px;cursor:pointer;transition:transform .1s,box-shadow .1s}
figure.ds-cart:hover{transform:translate(-2px,-2px);box-shadow:6px 6px 0 var(--pink)}
figure.ds-cart img{display:block;width:100%;height:74px;object-fit:cover;image-rendering:pixelated;border:2px solid var(--edge);background:#000}
figure.ds-cart figcaption{margin-top:6px;font-size:10.5px;font-weight:700;line-height:1.2;text-align:center}
a.ds-open{display:inline-block;font-family:var(--mono);font-size:11.5px;font-weight:700;color:var(--on);
  background:var(--pink);border:2px solid var(--edge);box-shadow:2px 2px 0 var(--edge);padding:3px 10px;
  transition:transform .08s,box-shadow .08s}
a.ds-open:hover{transform:translate(-1px,-1px);box-shadow:4px 4px 0 var(--edge)}

/* roads not taken — the cut decisions, dashed + struck */
.row.roads > .h{color:var(--orange)}
.tag.cut{border-style:dashed;border-color:var(--orange)}
.tag.cut .x{color:var(--orange);font-weight:700;margin-right:5px}
.tag.grew{border-color:var(--yellow)}

/* tools built — the toolsmith trail (green accent; hover a chip for its purpose) */
.row.tools > .h{color:#9ee37d}
.tag.tool{border-color:#9ee37d;font-family:var(--mono);font-size:11px;cursor:help}
.tag.tool:hover{box-shadow:3px 3px 0 #9ee37d}

/* open threads — handoff docs, work parked mid-thread (blue accent) */
.row.threads > .h{color:#7ee0ff}
.tag.handoff{border-color:#7ee0ff;border-style:dashed}
.tag.handoff:hover{box-shadow:3px 3px 0 #7ee0ff}

/* roads not taken — the cut decisions as cards, with a voiced line */
.roadcards{display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:14px}
.roadcard{background:var(--panel);border:3px solid var(--edge);border-top:9px solid var(--orange);
  box-shadow:var(--sh-sm);padding:12px 14px;cursor:pointer;transition:transform .1s,box-shadow .1s}
.roadcard:hover{transform:translate(-2px,-2px);box-shadow:7px 7px 0 var(--edge)}
.roadcard .rc-t{font-weight:700;font-size:13.5px;line-height:1.2;color:var(--orange)}
.roadcard .rc-v{margin:8px 0 0;font-size:12.5px;line-height:1.5;color:var(--ink)}
.roadcard .rc-v.dim{color:var(--dim);font-family:var(--mono);font-size:11px}
.roadcard .rc-d{margin-top:9px;font-family:var(--mono);font-size:10.5px;color:var(--dim);font-variant-numeric:tabular-nums}

/* research threads — a band of long investigations (blue capstone) */
section.threadsband{margin:64px 0 0;padding-top:36px;border-top:6px solid var(--edge);scroll-margin-top:16px}
section.threadsband .rs-head h2{background:#7ee0ff}
details.thread{border-left-width:10px;border-left-color:#7ee0ff;max-width:920px;scroll-margin-top:16px}
details.thread > summary .dst{font-family:var(--disp);font-weight:400;text-transform:uppercase;letter-spacing:-.2px;font-size:17px}
.thread-blurb{color:var(--ink);font-size:14px;line-height:1.6;max-width:760px;margin:6px 0 10px}
.thread-span{font-family:var(--mono);font-size:11px;color:var(--dim);margin:0 0 14px;
  border-bottom:2px dashed var(--panel2);padding-bottom:12px}

/* retrospect — the cross-week capstone. Pink-band heading like an era, but a
   numbered card grid for the observations + a foldout for the refresh recipe. */
section.retrospect{margin:64px 0 0;padding-top:36px;border-top:6px solid var(--edge);scroll-margin-top:16px}
.rs-head{display:flex;align-items:center;gap:14px;flex-wrap:wrap}
.rs-head h2{font-family:var(--disp);margin:0;font-size:30px;text-transform:uppercase;letter-spacing:-.5px;
  background:var(--pink);color:var(--on);border:3px solid var(--edge);box-shadow:var(--sh);padding:6px 14px;line-height:1.05}
.rs-head .dates{font-family:var(--mono);color:var(--ink);font-size:13px;font-weight:700}
.rs-lede{color:var(--ink);margin:16px 0 4px;max-width:780px;font-size:14.5px;font-weight:500}
.rs-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:18px;margin:22px 0}
.rs{position:relative;background:var(--panel);border:3px solid var(--edge);box-shadow:var(--sh);
  padding:16px 16px 14px;transform:rotate(var(--rot,0deg));transition:transform .1s,box-shadow .1s}
.rs:hover{transform:rotate(0deg) translate(-2px,-2px);box-shadow:8px 8px 0 var(--edge)}
.rs-n{position:absolute;top:-14px;right:-10px;font-family:var(--disp);font-size:26px;color:var(--on);
  background:var(--yellow);border:3px solid var(--edge);box-shadow:2px 2px 0 var(--edge);padding:1px 9px;line-height:1.1}
.rs h3{font-family:var(--disp);font-weight:400;text-transform:uppercase;letter-spacing:-.3px;
  font-size:16px;margin:0 40px 8px 0;line-height:1.12}
.rs-body{color:var(--ink);font-size:13px;line-height:1.5;margin:0}
.rs-ev{margin-top:11px;padding-top:9px;border-top:2px dashed var(--panel2);font-family:var(--mono);
  font-size:11px;color:var(--dim);line-height:1.45}
.rs-ev b{color:var(--orange);text-transform:uppercase;letter-spacing:.4px;margin-right:5px}
details.rs-recipe{margin-top:8px}
details.rs-recipe .rs-body{font-style:italic;max-width:820px}
footer{color:var(--dim);font-size:12px;margin-top:60px;border-top:3px solid var(--edge);padding-top:18px}
footer a{color:var(--pink);font-weight:700}
/* knowledge loop — field-note insight → the artifact it became */
section.knowloop{margin-top:60px}
.kl-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(330px,1fr));gap:14px;margin-top:14px}
.kl{background:var(--panel);border:3px solid var(--edge);box-shadow:var(--sh-sm);padding:12px 14px;border-left:7px solid var(--dim)}
.kl.inc{border-left-color:#5fd07a}
.kl-h{font-weight:700;margin-bottom:6px}
.kl-h a{color:var(--ink)}
.kl-h a:hover{color:var(--pink)}
.kl-badge{font-family:var(--mono);font-size:10px;text-transform:uppercase;letter-spacing:.5px;color:var(--on);background:var(--dim);padding:1px 6px;border:2px solid var(--edge);margin-right:6px}
.kl.inc .kl-badge{background:#5fd07a}
.kl-out{color:var(--dim);font-size:12.5px;line-height:1.5;margin:0}
.ns-list{display:flex;flex-wrap:wrap;gap:8px;margin-top:6px}
.ns-list .tag{cursor:pointer}
`

const JS = `
const $ = (t, c, h) => { const e = document.createElement(t); if (c) e.className = c; if (h != null) e.innerHTML = h; return e }
const esc = s => String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;')
const subTitle = {}; DATA.subsystems.forEach(s => subTitle[s.id] = s.title)
const subTier  = {}; DATA.subsystems.forEach(s => subTier[s.id]  = s.tier)

// open a docs/ markdown file in the parent editor's Docs tab (postMessage),
// falling back to the raw route if we're not embedded.
function openDoc(rel){
  if (window.parent && window.parent !== window){
    window.parent.postMessage({ type:'open-doc', path: rel }, '*')
  } else {
    window.open('/docs/' + rel, '_blank')
  }
}

// ask the parent editor to load a cart (only works embedded in the editor)
function loadCart(file, title){
  if (window.parent && window.parent !== window){
    window.parent.postMessage({ type:'load-cart', file, title }, '*')
  } else {
    window.open('/carts/' + file, '_blank')
  }
}

function render(){
  const root = document.getElementById('root')

  const head = $('header','top')
  head.appendChild($('h1', null, esc(DATA.title)))
  head.appendChild($('div','sub', esc(DATA.subtitle)))
  const g = DATA.generatedFrom
  head.appendChild($('div','meta',
    '<b>'+DATA.window.label+'</b> · '+DATA.window.from+' → '+DATA.window.to+
    ' · derived from <b>'+g.commits+'</b> commits, <b>'+g.adrs+'</b> decisions, <b>'+g.cartsBorn+'</b> carts born'))
  root.appendChild(head)

  // subsystem legend
  const leg = $('div','legend')
  leg.appendChild($('span','lbl','subsystems —'))
  DATA.subsystems.forEach(s => {
    leg.appendChild($('span','chip tier-'+s.tier,
      '<span class="dot"></span>'+esc(s.title)))
  })
  root.appendChild(leg)
  const tl = $('div','legend')
  tl.appendChild($('span','lbl','tiers —'))
  ;[['foundational','foundational'],['mid','notable'],['small','everyday']].forEach(([k,lbl])=>
    tl.appendChild($('span','chip','<span class="dot '+k+'"></span>'+lbl)))
  root.appendChild(tl)

  // table of contents — one block per WEEK with its growth (commits/carts) + a
  // bar sized against the busiest week, then its chapters as jump links
  const maxC = Math.max(1, ...DATA.weeks.map(w => w.commits))
  const toc = $('nav','toc')
  toc.appendChild($('div','toc-title','Contents — week by week'))
  DATA.weeks.forEach((w, wi) => {
    const tw = $('div','toc-week')
    const head = $('a','tw-head'); head.href = '#'+w.id
    const lab = $('span','tw-label', esc(w.label)); lab.style.background = ERA_COLORS[wi % ERA_COLORS.length]
    head.appendChild(lab)
    if (w.tagline) head.appendChild($('span','tw-tag', esc(w.tagline)))
    tw.appendChild(head)
    const bar = $('div','tw-bar'); bar.innerHTML = '<span style="width:'+Math.round(w.commits / maxC * 100)+'%"></span>'
    tw.appendChild(bar)
    tw.appendChild($('div','tw-stats','<b>'+w.commits+'</b> commits · <b>'+w.cartsBorn+'</b> carts born · <b>'+w.eras.length+'</b> chapters'))
    const eras = $('div','tw-eras')
    w.eras.forEach(e => {
      const a = $('a', null, esc(e.title)+' <span class="ec">'+e.commitCount+'</span>')
      a.href = '#era-'+e.id
      eras.appendChild(a)
    })
    tw.appendChild(eras)
    toc.appendChild(tw)
  })
  if (DATA.threads && DATA.threads.length){
    const tw = $('div','toc-week')
    const head = $('a','tw-head'); head.href = '#threads'
    const lab = $('span','tw-label', '⛓'); lab.style.background = '#7ee0ff'
    head.appendChild(lab)
    head.appendChild($('span','tw-tag', 'Research threads'))
    tw.appendChild(head)
    tw.appendChild($('div','tw-stats', '<b>'+DATA.threads.length+'</b> long investigations · docs, handoffs & the carts they grew'))
    const links = $('div','tw-eras')
    DATA.threads.forEach(t => { const a = $('a', null, esc(t.title)+' <span class="ec">'+t.cartCount+'</span>'); a.href = '#thread-'+t.id; links.appendChild(a) })
    tw.appendChild(links)
    toc.appendChild(tw)
  }
  if (DATA.observations && (DATA.observations.items || []).length){
    const tw = $('div','toc-week')
    const head = $('a','tw-head'); head.href = '#retrospect'
    const lab = $('span','tw-label', '★'); lab.style.background = 'var(--pink)'
    head.appendChild(lab)
    head.appendChild($('span','tw-tag', esc(DATA.observations.title || 'Retrospect')))
    tw.appendChild(head)
    tw.appendChild($('div','tw-stats', '<b>'+DATA.observations.items.length+'</b> cross-week observations · read off the finished arc'))
    toc.appendChild(tw)
  }
  root.appendChild(toc)

  // body — a week band, then that week's eras (era colours cycle continuously)
  let ei = 0
  DATA.weeks.forEach((w, wi) => {
    root.appendChild(renderWeekBand(w, maxC, wi))
    w.eras.forEach(e => root.appendChild(renderEra(e, ei++)))
  })

  // research threads — topics that spawned a trail of docs/handoffs/carts
  if (DATA.threads && DATA.threads.length) root.appendChild(renderThreads(DATA.threads))

  // the retrospect capstone — cross-week observations, read off the finished arc
  if (DATA.observations && (DATA.observations.items || []).length) root.appendChild(renderObservations(DATA.observations))

  // the knowledge loop — each field-note insight mapped to the artifact it became
  const klSec = renderKnowledgeLoop(); if (klSec) root.appendChild(klSec)

  const f = $('footer', null,
    'Generated by <code>tools/build-history.js</code> from <code>docs/history-spine.json</code> + git. '+
    'Structure is hand-authored; dates and lists are derived. Editorial prose follows '+
    '<a href="#" data-doc="guides/voice.md">guides/voice.md</a>.')
  root.appendChild(f)

  // doc links + click-a-cart-to-load
  document.addEventListener('click', ev => {
    const t = ev.target.closest('[data-doc]')
    if (t){ ev.preventDefault(); openDoc(t.getAttribute('data-doc')); return }
    const cart = ev.target.closest('[data-cart]')   // a carts-born entry OR a hero figure
    if (cart){ ev.preventDefault(); loadCart(cart.dataset.cart, cart.dataset.title || '') }
  })

  // give the panels a small, STABLE, hand-placed tilt (deterministic per index,
  // so the look doesn't churn between regenerations / reloads)
  let ti = 0
  document.querySelectorAll('.ms, details, .editorial, .rs, header.top .meta').forEach(el => {
    const r = Math.sin((ti + 1) * 99.13) * 1e4, f = r - Math.floor(r)  // 0..1
    el.style.setProperty('--rot', ((f - 0.5) * 2.2).toFixed(2) + 'deg') // ~±1.1°
    ti++
  })

  // hover a "carts born" entry → a card with its thumbnail (loaded on demand
  // from /carts/, one shared image so we never fetch 200 up front) + the cart's
  // description from index.json. Only resolves inside the editor dev server.
  const meta = DATA.cartMeta || {}
  const prev = $('div','cart-preview')
  prev.innerHTML = '<img alt=""><div class="cp-meta"><b class="cp-t"></b><span class="cp-lin"></span><span class="cp-d"></span><span class="cp-teach"></span></div>'
  document.body.appendChild(prev)
  const pimg = prev.querySelector('img'), pT = prev.querySelector('.cp-t')
  const pD = prev.querySelector('.cp-d'), pLin = prev.querySelector('.cp-lin'), pTeach = prev.querySelector('.cp-teach')
  let curCart = ''
  document.addEventListener('mouseover', ev => {
    const li = ev.target.closest('li[data-cart]'); if (!li) return
    const f = li.dataset.cart
    if (f !== curCart){
      curCart = f; pimg.src = '/carts/' + f
      const m = meta[f] || {}
      pT.textContent = m.title || li.dataset.title || ''
      pD.textContent = m.desc || ''
      pD.style.display = m.desc ? '' : 'none'
      // lineage — what the cart descends from / what's new (the authored one-liner)
      pLin.textContent = m.lineage || ''
      pLin.style.display = m.lineage ? '' : 'none'
      // teaches — the controlled-vocabulary technique tags, as little chips
      pTeach.innerHTML = (m.teaches || []).map(t => '<i>'+esc(t)+'</i>').join('')
      pTeach.style.display = (m.teaches && m.teaches.length) ? '' : 'none'
    }
    prev.classList.add('show')
  })
  document.addEventListener('mouseout', ev => {
    const li = ev.target.closest('li[data-cart]'); if (!li) return
    const to = ev.relatedTarget && ev.relatedTarget.closest && ev.relatedTarget.closest('li[data-cart]')
    if (!to) prev.classList.remove('show')
  })
  document.addEventListener('mousemove', ev => {
    if (!prev.classList.contains('show')) return
    const pad = 18, w = prev.offsetWidth || 190, h = prev.offsetHeight || 140
    let x = ev.clientX + pad, y = ev.clientY + pad
    if (x + w > innerWidth)  x = ev.clientX - w - pad
    if (y + h > innerHeight) y = ev.clientY - h - pad
    prev.style.left = x + 'px'; prev.style.top = Math.max(8, y) + 'px'
  })

  // hover a milestone card → a friendly blurb (an authored note, or the commit's
  // own words cleaned into a sentence). Reads like prose, not a git log.
  const pop = $('div','ms-pop')
  pop.innerHTML = '<div class="msp-s"></div><div class="msp-b"></div>'
  document.body.appendChild(pop)
  const pS = pop.querySelector('.msp-s'), pB = pop.querySelector('.msp-b')
  document.addEventListener('mouseover', ev => {
    const c = ev.target.closest('.ms[data-gist]'); if (!c) return
    pS.textContent = c.dataset.gist
    pB.textContent = c.dataset.detail || ''; pB.style.display = c.dataset.detail ? '' : 'none'
    pop.classList.add('show')
  })
  document.addEventListener('mouseout', ev => {
    const c = ev.target.closest('.ms[data-gist]'); if (!c) return
    const to = ev.relatedTarget && ev.relatedTarget.closest && ev.relatedTarget.closest('.ms[data-gist]')
    if (to !== c) pop.classList.remove('show')
  })
  document.addEventListener('mousemove', ev => {
    if (!pop.classList.contains('show')) return
    const pad = 18, w = pop.offsetWidth || 300, h = pop.offsetHeight || 120
    let x = ev.clientX + pad, y = ev.clientY + pad
    if (x + w > innerWidth)  x = ev.clientX - w - pad
    if (y + h > innerHeight) y = ev.clientY - h - pad
    pop.style.left = x + 'px'; pop.style.top = Math.max(8, y) + 'px'
  })

  // a tiny fixed "back to top" button, shown once you've scrolled down a bit
  const toTop = $('button','totop','↑'); toTop.title = 'back to top'
  document.body.appendChild(toTop)
  toTop.addEventListener('click', () => window.scrollTo({ top: 0, behavior: 'smooth' }))

  // persist + restore our own scroll. Two return paths, both handled here:
  //  · doc link → the iframe RELOADS → we restore on load (rAF + fonts.ready)
  //  · cart load → the iframe stays mounted but the host display:none's it
  //    (which zeroes scroll); the host posts 'restore-scroll' when the Docs tab
  //    is re-shown, and WE read back our own sessionStorage (the host can't —
  //    storage may be partitioned).
  const KEY = 'de-history-scroll'
  const getSaved = () => { try { return +sessionStorage.getItem(KEY) || 0 } catch (e) { return 0 } }
  const restoreScroll = () => { const y = getSaved(); if (y) requestAnimationFrame(() => requestAnimationFrame(() => window.scrollTo(0, y))) }
  restoreScroll()
  if (document.fonts && document.fonts.ready) document.fonts.ready.then(() => { const y = getSaved(); if (y) window.scrollTo(0, y) })
  window.addEventListener('message', e => { if (e.data && e.data.type === 'restore-scroll') restoreScroll() })
  let sraf = 0
  window.addEventListener('scroll', () => {
    toTop.classList.toggle('show', window.scrollY > 400)
    if (sraf) return
    sraf = requestAnimationFrame(() => { sraf = 0; try { sessionStorage.setItem(KEY, String(Math.round(window.scrollY))) } catch (e) {} })
  }, { passive: true })
}

const ERA_COLORS = ['var(--yellow)','var(--orange)','var(--pink)','#7ee0ff']

function renderWeekBand(w, maxC, wi){
  const s = $('section','week-band'); s.id = w.id
  const h = $('div','wb-head')
  const h2 = $('h2', null, esc(w.label)); h2.style.textShadow = '3px 3px 0 ' + ERA_COLORS[wi % ERA_COLORS.length]
  h.appendChild(h2)
  h.appendChild($('span','wb-range', w.from + ' → ' + w.to))
  if (w.tagline) h.appendChild($('span','wb-tag', esc(w.tagline)))
  s.appendChild(h)
  const bar = $('div','wb-bar'); bar.innerHTML = '<span style="width:' + Math.round(w.commits / maxC * 100) + '%"></span>'
  s.appendChild(bar)
  s.appendChild($('div','wb-stats', '<b>' + w.commits + '</b> commits · <b>' + w.cartsBorn + '</b> carts born · <b>' + w.eras.length + '</b> chapters'))
  return s
}

function renderEra(e, i){
  const s = $('section','era'); s.id = 'era-'+e.id
  const h = $('div','era-head')
  const h2 = $('h2', null, esc(e.title))
  h2.style.background = ERA_COLORS[i % ERA_COLORS.length]   // cycle colours across all weeks
  h.appendChild(h2)
  const dr = e.from === e.to ? e.from : e.from+' → '+e.to
  h.appendChild($('span','dates', dr))
  h.appendChild($('span','cc', e.commitCount+' commits'))
  s.appendChild(h)

  // intro: blurb + editorial on the left, the era's most worked-on cart as a
  // magazine pull-image on the right
  const intro = $('div','era-intro')
  const left = $('div','intro-left')
  if (e.blurb) left.appendChild($('div','blurb', esc(e.blurb)))
  if (e.editorial) left.appendChild($('div','editorial', esc(e.editorial)))
  intro.appendChild(left)
  if (e.hero){
    const fig = $('figure','hero')
    fig.dataset.cart = e.hero.name + '.cart.png'   // click → load this cart
    fig.dataset.title = e.hero.title
    fig.title = e.hero.lineage ? e.hero.title + ' — ' + e.hero.lineage : 'open ' + e.hero.title
    // size the thumb by how hard the cart was worked: ~165px → 320px, maxing
    // out around 40 commits in a stretch
    const W = Math.round(165 + Math.min(1, e.hero.commits / 40) * 155)
    fig.style.flexBasis = W + 'px'; fig.style.width = W + 'px'
    fig.innerHTML =
      '<img src="'+e.hero.dataUri+'" alt="'+esc(e.hero.title)+'">'+
      '<figcaption><span class="hk">most worked-on</span>'+
      '<b>'+esc(e.hero.title)+'</b>'+
      '<span class="hc">'+e.hero.commits+' commits this stretch</span>'+
      (e.hero.lineage ? '<span class="hl">'+esc(e.hero.lineage)+'</span>' : '')+
      '</figcaption>'
    intro.appendChild(fig)
  }
  s.appendChild(intro)

  // design-note spotlight — the era's most cart-spawning design doc, foldout
  if (e.spotlight) s.appendChild(renderSpotlight(e.spotlight))

  // milestones
  if (e.milestones.length){
    const grid = $('div','mstones')
    e.milestones.forEach(m => {
      const c = $('div','ms '+m.tier)
      c.appendChild($('div','date', m.day || ''))
      c.appendChild($('div','t', esc(m.title)))
      c.appendChild($('div','sx', m.subsystem ? esc(subTitle[m.subsystem]||m.subsystem) : ''))
      if (!m.matched) c.appendChild($('div','nomatch','⚠ no matching commit found'))
      // hover → a friendly blurb (authored note, or the commit's own words cleaned up)
      if (m.gist){ c.dataset.gist = m.gist; c.dataset.detail = m.detail || '' }
      grid.appendChild(c)
    })
    s.appendChild(grid)
  }

  // ADRs — kept decisions, then the CUT ones as a distinct "roads not taken" row
  const kept = e.adrs.filter(a => !a.cut), cut = e.adrs.filter(a => a.cut)
  if (kept.length){
    const r = $('div','row'); r.appendChild($('div','h','Decisions landed'))
    const tags = $('div','tags')
    kept.forEach(a => {
      const t = $('span','tag', esc(a.label))
      t.setAttribute('data-doc', a.rel)
      if (a.summary) t.title = a.summary
      tags.appendChild(t)
    })
    r.appendChild(tags); s.appendChild(r)
  }
  if (cut.length){
    const r = $('div','row roads'); r.appendChild($('div','h','✗ Roads not taken'))
    const grid = $('div','roadcards')
    cut.forEach(a => {
      const c = $('article','roadcard'); c.setAttribute('data-doc', a.rel); c.title = 'open ' + a.label
      c.appendChild($('div','rc-t', '✗ '+esc(a.title || a.label)))
      if (a.voiced) c.appendChild($('p','rc-v', esc(a.voiced)))
      else if (a.summary) c.appendChild($('p','rc-v dim', esc(a.summary)))
      if (a.day) c.appendChild($('div','rc-d', a.day))
      grid.appendChild(c)
    })
    r.appendChild(grid); s.appendChild(r)
  }

  // design docs written — split handoffs out as their own "open threads" row
  // (handoffs = work parked mid-thread; the concrete form of a long thread)
  const handoffs = e.docs.filter(d => /handoff/i.test(d.name))
  const notes = e.docs.filter(d => !/handoff/i.test(d.name))
  const docTag = (dd, cls) => {
    const t = $('span', cls, esc(dd.name))
    t.setAttribute('data-doc', dd.rel)
    if (dd.desc) t.title = dd.desc
    return t
  }
  if (notes.length){
    const r = $('div','row'); r.appendChild($('div','h','Design notes written ('+notes.length+')'))
    const tags = $('div','tags')
    notes.sort((a,b)=>a.name.localeCompare(b.name)).forEach(dd =>
      tags.appendChild(docTag(dd, 'tag'+(dd.relatedCarts && dd.relatedCarts.length ? ' grew' : ''))))
    r.appendChild(tags); s.appendChild(r)
  }
  // tools built — the toolsmith trail (hover a chip for what it does)
  if (e.toolsBorn && e.toolsBorn.length){
    const r = $('div','row tools'); r.appendChild($('div','h','🔧 Tools built ('+e.toolsBorn.length+')'))
    const tags = $('div','tags')
    e.toolsBorn.slice().sort((a,b)=>a.name.localeCompare(b.name)).forEach(t => {
      const tag = $('span','tag tool', esc(t.name))
      if (t.desc) tag.title = t.desc
      tags.appendChild(tag)
    })
    r.appendChild(tags); s.appendChild(r)
  }
  if (handoffs.length){
    const r = $('div','row threads'); r.appendChild($('div','h','⛏ Open threads — handoffs ('+handoffs.length+')'))
    const tags = $('div','tags')
    handoffs.sort((a,b)=>a.name.localeCompare(b.name)).forEach(dd =>
      tags.appendChild(docTag(dd, 'tag handoff')))
    r.appendChild(tags); s.appendChild(r)
  }

  // carts born
  if (e.cartsBorn.length){
    const det = $('details')
    const sum = $('summary', null,
      '<span class="caret">▸</span> Carts born <span class="grow">'+e.cartsBorn.length+' cartridges</span>')
    det.appendChild(sum)
    const body = $('div','detail-body')
    const ul = $('ul','carts')
    e.cartsBorn.slice().sort((a,b)=>a.title.localeCompare(b.title)).forEach(c => {
      const li = $('li', null, esc(c.title)+' <span class="f">'+esc(c.file.replace(/\\.cart\\.png$/,''))+'</span>')
      li.dataset.cart = c.file    // hover → thumbnail; click → load the cart (both via /carts/)
      li.dataset.title = c.title
      ul.appendChild(li)
    })
    body.appendChild(ul); det.appendChild(body); s.appendChild(det)
  }

  // day-by-day beat (collapsible)
  const det = $('details')
  const markedN = e.days.filter(d=>d.marked).length
  const sum = $('summary', null,
    '<span class="caret">▸</span> The day-by-day beat'+
    '<span class="grow">'+e.days.length+' days'+(markedN?' · '+markedN+' marked':'')+'</span>')
  det.appendChild(sum)
  const body = $('div','detail-body')
  e.days.forEach(day => {
    const dv = $('div','day')
    const dh = $('div','dh')
    dh.appendChild($('span','dd', day.day))
    dh.appendChild($('span','dc', day.count+' commits'))
    if (day.marked) dh.appendChild($('span','badge marked','★ marked'))
    if (day.dense)  dh.appendChild($('span','badge dense','🔥 dense'))
    dv.appendChild(dh)
    if (day.marked) dv.appendChild($('div','note', esc(day.marked)))
    const ul = $('ul','commits')
    day.commits.forEach(c => {
      const sx = c.subs.length ? ' <span class="sx">['+c.subs.map(id=>esc(subTitle[id]||id)).join(', ')+']</span>' : ''
      ul.appendChild($('li', null, esc(c.subject)+sx))
    })
    dv.appendChild(ul)
    body.appendChild(dv)
  })
  det.appendChild(body); s.appendChild(det)

  return s
}

const prettyName = s => String(s).replace(/-/g, ' ')

// the design-note spotlight — a foldout sibling to the hero cart: the era's
// most cart-spawning design doc, its friendly one-liner + the carts that grew
// from it (click a thumb to load the cart, or open the full note).
function renderSpotlight(sp){
  const det = $('details','docspot')
  const badge = '<span class="dsb">'+esc(sp.status || 'design note')+'</span>'
  const grew = sp.relatedCount ? '<span class="grow">'+sp.relatedCount+' cart'+(sp.relatedCount===1?'':'s')+' grew from this</span>' : ''
  det.appendChild($('summary', null,
    '<span class="caret">▸</span>'+badge+'<b class="dst">'+esc(prettyName(sp.name))+'</b>'+grew))
  const body = $('div','detail-body')
  if (sp.desc) body.appendChild($('p','ds-desc', esc(sp.desc)))
  if (sp.carts && sp.carts.length){
    const row = $('div','ds-carts')
    sp.carts.forEach(c => {
      const fig = $('figure','ds-cart')
      fig.dataset.cart = c.file; fig.dataset.title = c.title
      fig.title = c.lineage ? c.title + ' — ' + c.lineage : 'open ' + c.title
      fig.innerHTML = '<img src="'+c.dataUri+'" alt="'+esc(c.title)+'"><figcaption>'+esc(c.title)+'</figcaption>'
      row.appendChild(fig)
    })
    body.appendChild(row)
  }
  const a = $('a','ds-open','open the full note →'); a.href = '#'; a.setAttribute('data-doc', sp.rel)
  body.appendChild(a)
  det.appendChild(body)
  return det
}

// research threads — a band of long investigations. Each is a foldout: voiced
// blurb, the cross-referenced docs (notes / handoffs / decisions) as chips, and
// the carts that grew from the thread as thumbnails.
function renderThreads(threads){
  const sec = $('section','threadsband'); sec.id = 'threads'
  sec.appendChild($('div','rs-head', '<h2>Research threads</h2>'))
  sec.appendChild($('div','rs-lede',
    'Topics that spawned a trail of docs, handoffs and carts — the long investigations. '+
    'Each links back to its notes; the carts are what grew from the thinking.'))
  threads.forEach(t => sec.appendChild(renderThread(t)))
  return sec
}

function renderThread(t){
  const det = $('details','thread'); det.id = 'thread-'+t.id
  const span = t.from ? (t.from + (t.to && t.to !== t.from ? ' → '+t.to : '')) : ''
  det.appendChild($('summary', null,
    '<span class="caret">▸</span><b class="dst">'+esc(t.title)+'</b>'+
    '<span class="grow">'+t.docCount+' docs · '+t.cartCount+' carts</span>'))
  const body = $('div','detail-body')
  if (t.blurb) body.appendChild($('p','thread-blurb', esc(t.blurb)))
  if (span) body.appendChild($('div','thread-span', span + (t.seed ? ' · seeded by '+esc(t.seed) : '')))

  const chips = (label, arr, cls) => {
    if (!arr.length) return
    const r = $('div','row'+(cls ? ' '+cls : '')); r.appendChild($('div','h', label+' ('+arr.length+')'))
    const tags = $('div','tags')
    arr.forEach(d => {
      const tag = $('span','tag'+(cls === 'threads' ? ' handoff' : ''), esc(d.name || d.title))
      tag.setAttribute('data-doc', d.rel)
      if (d.desc) tag.title = d.desc
      tags.appendChild(tag)
    })
    r.appendChild(tags); body.appendChild(r)
  }
  chips('Notes', t.notes)
  chips('Handoffs', t.handoffs, 'threads')
  if (t.decisions.length){
    const r = $('div','row'); r.appendChild($('div','h','Decisions'))
    const tags = $('div','tags')
    t.decisions.forEach(d => {
      const tag = $('span','tag'+(d.cut ? ' cut' : ''), (d.cut ? '<span class="x">✗</span>' : '')+esc(d.title))
      tag.setAttribute('data-doc', d.rel)
      tags.appendChild(tag)
    })
    r.appendChild(tags); body.appendChild(r)
  }
  if (t.carts.length){
    const r = $('div','row'); r.appendChild($('div','h','Carts that grew from it'))
    const row = $('div','ds-carts')
    t.carts.forEach(c => {
      const fig = $('figure','ds-cart')
      fig.dataset.cart = c.file; fig.dataset.title = c.title
      fig.title = c.lineage ? c.title + ' — ' + c.lineage : 'open ' + c.title
      fig.innerHTML = '<img src="'+c.dataUri+'" alt="'+esc(c.title)+'"><figcaption>'+esc(c.title)+'</figcaption>'
      row.appendChild(fig)
    })
    r.appendChild(row); body.appendChild(r)
  }
  det.appendChild(body)
  return det
}

function renderObservations(o){
  const s = $('section','retrospect'); s.id = 'retrospect'
  const h = $('div','rs-head')
  h.appendChild($('h2', null, esc(o.title || 'Retrospect')))
  if (o.asOf) h.appendChild($('span','dates', 'as of ' + o.asOf))
  s.appendChild(h)
  s.appendChild($('div','rs-lede',
    'Conclusions that only surface from reading the whole window back at once — not legible commit-by-commit. '+
    'Each is grounded in a number the page can re-derive.'))

  const grid = $('div','rs-grid')
  o.items.forEach((it, i) => {
    const c = $('article','rs')
    c.appendChild($('div','rs-n', String(i + 1).padStart(2, '0')))
    c.appendChild($('h3', null, esc(it.title)))
    if (it.body) c.appendChild($('p','rs-body', esc(it.body)))
    if (it.evidence) c.appendChild($('div','rs-ev', '<b>evidence</b> ' + esc(it.evidence)))
    grid.appendChild(c)
  })
  s.appendChild(grid)

  if (o.recipe){
    const det = $('details','rs-recipe')
    det.appendChild($('summary', null, '<span class="caret">▸</span> How these were got — the recipe, to run again in due time'))
    const body = $('div','detail-body')
    body.appendChild($('p','rs-body', esc(o.recipe)))
    det.appendChild(body); s.appendChild(det)
  }
  return s
}

function renderKnowledgeLoop(){
  const kl = DATA.knowledgeLoop || [], ns = DATA.negativeSpace || [], dp = DATA.designPhases || null
  if (!kl.length && !ns.length) return null
  const s = $('section','knowloop'); s.id = 'knowledge'
  const h = $('div','rs-head'); h.appendChild($('h2', null, 'What we learned → what we built')); s.appendChild(h)
  const phLine = dp ? (' Designs now stand at ' + Object.entries(dp).map(([k,v]) => v + ' ' + k).join(' · ') + '.') : ''
  s.appendChild($('div','rs-lede', "The research journal mapped to the artifacts each insight became — read off the field notes' Outcome sections, not hand-written here." + phLine))
  const order = { 'Incorporated':0, 'Working Theory':1 }
  const sorted = kl.slice().sort((a,b) => (order[a.status] ?? 9) - (order[b.status] ?? 9) || (a.num - b.num))
  const grid = $('div','kl-grid')
  sorted.forEach(n => {
    const c = $('article','kl' + (n.status === 'Incorporated' ? ' inc' : ''))
    c.appendChild($('div','kl-h', '<span class="kl-badge">' + esc(n.status) + '</span> <a href="#" data-doc="' + esc(n.rel) + '">' + esc(n.title) + '</a>'))
    c.appendChild($('p','kl-out', esc(n.outcome)))
    grid.appendChild(c)
  })
  s.appendChild(grid)
  if (ns.length){
    const det = $('details','rs-recipe')
    det.appendChild($('summary', null, '<span class="caret">▸</span> The negative space — ' + ns.length + ' decisions to cut or defer (features removed to keep the lesson honest)'))
    const body = $('div','detail-body'); const list = $('div','ns-list')
    ns.forEach(a => { const t = $('span','tag cut', '<span class="x">✗</span>' + esc(a.title)); t.setAttribute('data-doc', a.rel); list.appendChild(t) })
    body.appendChild(list); det.appendChild(body); s.appendChild(det)
  }
  return s
}

render()
`

// ---- write ----
fs.writeFileSync(OUT, renderHtml(data))
console.log(`[build-history] wrote ${path.relative(REPO, OUT)}`)
console.log(`  ${data.window.label}: ${data.weeks.length} weeks, ${data.weeks.reduce((n, w) => n + w.eras.length, 0)} eras, ${rawCommits.length} commits, ${adrs.length} ADRs, ${cartsBorn.length} carts born`)
