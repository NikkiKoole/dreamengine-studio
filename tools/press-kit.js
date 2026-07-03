#!/usr/bin/env node
// press-kit.js — generate a self-contained presskit()-style press page for an app, from data
// we already own. docs/design/press-kit.md. Mirror the journalist-recognised format; build our
// own (same call as ADR-0026/Fastlane). Channel-A own-domain artifact; reuses store-shots.js /
// make-gif.js assets. No node deps (flat-frontmatter + a tiny markdown renderer, self-contained).
//
//   node tools/press-kit.js tinyjam \
//     --shots build/.shots --trailer docs/media/groovebox.webm --icon apps/tinyjam/icon.png
//
// Reads (all optional except the manifest, degrades gracefully + reports what's missing):
//   apps/<name>/app.json   manifest (required): name, bundleId, version, carts, price, icon
//   apps/<name>/press.md   per-app copy — FLAT frontmatter (releaseDate/price/…) + markdown body
//                          (## Description / ## History / ## Features / ## Quotes / ## Awards)
//   apps/studio.md         studio-wide — frontmatter (developer/basedIn/website/pressContact/
//                          social…) + a body (## About / ## Credits)
// Writes: site/press/<name>/index.html (self-contained) + assets/ + <name>-presskit.zip
'use strict'
const fs = require('fs')
const path = require('path')
const { execFileSync } = require('child_process')
const ROOT = path.join(__dirname, '..')

// ── args ─────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2)
const opt = { shots: 'build/.shots', trailer: '', icon: '', out: '' }
const pos = []
for (let i = 0; i < argv.length; i++) {
  const k = argv[i].replace(/^--/, '')
  if (argv[i].startsWith('--') && k in opt) opt[k] = argv[++i] ?? ''
  else if (argv[i].startsWith('--')) { console.error(`unknown flag ${argv[i]}`); process.exit(1) }
  else pos.push(argv[i])
}
const app = pos[0]
if (!app) { console.error('usage: node tools/press-kit.js <app> [--shots dir] [--trailer f] [--icon f] [--out dir]'); process.exit(1) }

const appDir = path.join(ROOT, 'apps', app)
const manifestPath = path.join(appDir, 'app.json')
if (!fs.existsSync(manifestPath)) { console.error(`no manifest at apps/${app}/app.json`); process.exit(1) }
const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'))

// ── tiny flat-frontmatter + markdown ───────────────────────────────────────────
function loadMd(p) {
  if (!fs.existsSync(p)) return { fm: {}, body: '' }
  let s = fs.readFileSync(p, 'utf8')
  const fm = {}
  const m = s.match(/^---\n([\s\S]*?)\n---\n?/)
  if (m) {
    for (const line of m[1].split('\n')) {
      const kv = line.match(/^([A-Za-z0-9_]+):\s*(.*)$/)
      if (!kv) continue
      let v = kv[2].trim().replace(/^["']|["']$/g, '')
      if (v === 'true') v = true; else if (v === 'false') v = false
      fm[kv[1]] = v
    }
    s = s.slice(m[0].length)
  }
  return { fm, body: s.trim() }
}
const esc = t => String(t).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
const inline = t => esc(t)
  .replace(/\[([^\]]+)\]\(([^)]+)\)/g, '<a href="$2">$1</a>')
  .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
  .replace(/\*([^*]+)\*/g, '<em>$1</em>')
  .replace(/`([^`]+)`/g, '<code>$1</code>')
function md2html(src) {          // supports: ## / ### / paragraphs / - lists / > quotes
  const out = []; let list = null, quote = null
  const flush = () => { if (list) { out.push(`<ul>${list.join('')}</ul>`); list = null }
    if (quote) { out.push(`<blockquote>${quote.join(' ')}</blockquote>`); quote = null } }
  for (const raw of (src || '').split('\n')) {
    const l = raw.trim()
    if (!l) { flush(); continue }
    let h
    if ((h = l.match(/^(#{2,3})\s+(.*)/))) { flush(); out.push(`<h${h[1].length}>${inline(h[2])}</h${h[1].length}>`) }
    else if (l.startsWith('- ')) { if (!list) { flush(); list = [] } list.push(`<li>${inline(l.slice(2))}</li>`) }
    else if (l.startsWith('> ')) { if (!quote) { flush(); quote = [] } quote.push(inline(l.slice(2))) }
    else { flush(); out.push(`<p>${inline(l)}</p>`) }
  }
  flush(); return out.join('\n')
}

const studio = loadMd(path.join(ROOT, 'apps', 'studio.md'))
const press = loadMd(path.join(appDir, 'press.md'))

// ── assets ─────────────────────────────────────────────────────────────────────
const out = path.resolve(ROOT, opt.out || `site/press/${app}`)
const assetsDir = path.join(out, 'assets')
fs.rmSync(out, { recursive: true, force: true })
fs.mkdirSync(assetsDir, { recursive: true })
const copyIn = (src, name) => { const d = path.join(assetsDir, name); fs.copyFileSync(src, d); return `assets/${name}` }

const shots = []
const shotsDir = path.resolve(ROOT, opt.shots)
if (fs.existsSync(shotsDir)) for (const f of fs.readdirSync(shotsDir).filter(f => /\.png$/i.test(f)).sort())
  shots.push(copyIn(path.join(shotsDir, f), f))
let trailer = ''
if (opt.trailer && fs.existsSync(path.resolve(ROOT, opt.trailer)))
  trailer = copyIn(path.resolve(ROOT, opt.trailer), 'trailer' + path.extname(opt.trailer))
let icon = ''
const iconSrc = opt.icon || (manifest.icon && path.join(appDir, manifest.icon))
if (iconSrc && fs.existsSync(path.resolve(ROOT, iconSrc))) icon = copyIn(path.resolve(ROOT, iconSrc), 'icon' + path.extname(iconSrc))

// ── factsheet ────────────────────────────────────────────────────────────────
const S = studio.fm, P = press.fm
const social = ['mastodon', 'youtube', 'x', 'bluesky', 'instagram'].filter(k => S[k])
  .map(k => `<a href="${S[k]}">${k}</a>`).join(' · ')
const factRows = [
  ['Developer', S.developer], ['Based in', S.basedIn],
  ['Release date', P.releaseDate], ['Platforms', 'iOS' + (manifest.mac ? ', macOS' : '')],
  ['Price', P.price], ['Website', S.website && `<a href="${S.website}">${S.website}</a>`],
  ['Press contact', S.pressContact && `<a href="mailto:${S.pressContact}">${S.pressContact}</a>`],
  ['Social', social],
].filter(([, v]) => v).map(([k, v]) => `<tr><th>${k}</th><td>${v}</td></tr>`).join('')

// ── page ─────────────────────────────────────────────────────────────────────
const descFallback = `${manifest.name} — ${(manifest.carts || []).join(', ')}.`
const html = `<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>${esc(manifest.name)} — Press Kit</title><style>
:root{--bg:#141726;--fg:#e8e8ee;--mut:#9aa0b4;--card:#1d2033;--acc:#ff5d8f}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);
font:16px/1.6 -apple-system,system-ui,sans-serif}main{max-width:820px;margin:0 auto;padding:32px 20px 80px}
header{display:flex;gap:20px;align-items:center;margin-bottom:8px}
header img{width:96px;height:96px;border-radius:20px;image-rendering:pixelated}
h1{margin:0;font-size:34px}h2{margin:40px 0 12px;font-size:22px;border-bottom:1px solid #ffffff1a;padding-bottom:6px}
h3{margin:24px 0 8px;font-size:17px}a{color:var(--acc)}
table{border-collapse:collapse;width:100%;background:var(--card);border-radius:12px;overflow:hidden}
th,td{text-align:left;padding:10px 14px;vertical-align:top}th{color:var(--mut);width:150px;font-weight:600}
tr+tr{border-top:1px solid #ffffff10}
.gallery{display:grid;grid-template-columns:repeat(auto-fill,minmax(240px,1fr));gap:12px}
.gallery img{width:100%;border-radius:10px;image-rendering:pixelated;background:#000}
video{width:100%;border-radius:12px;background:#000}
blockquote{margin:12px 0;padding:10px 16px;border-left:3px solid var(--acc);background:var(--card);border-radius:0 8px 8px 0;color:#d7d9e6}
.dl{display:inline-block;margin-top:10px;background:var(--acc);color:#111;padding:10px 18px;border-radius:10px;text-decoration:none;font-weight:700}
footer{color:var(--mut);margin-top:48px;font-size:13px}
</style></head><body><main>
<header>${icon ? `<img src="${icon}" alt="icon">` : ''}<div><h1>${esc(manifest.name)}</h1>
<div style="color:var(--mut)">${esc(P.tagline || descFallback)}</div></div></header>

<h2>Factsheet</h2><table>${factRows}</table>

${press.body ? md2html(press.body) : `<h2>Description</h2><p>${esc(descFallback)}</p>`}

${shots.length ? `<h2>Screenshots</h2><div class="gallery">${shots.map(s => `<a href="${s}"><img src="${s}" alt="screenshot"></a>`).join('')}</div>` : ''}
${trailer ? `<h2>Trailer</h2><video controls src="${trailer}"></video>` : ''}
${P.monetizationPermission ? `<h2>Monetization permission</h2><p>You are free to record, stream, and monetize videos of ${esc(manifest.name)}. No permission needed — go for it.</p>` : ''}
${studio.body ? md2html(studio.body) : ''}
<p><a class="dl" href="${app}-presskit.zip">⬇ Download all assets (.zip)</a></p>
<footer>Press kit for ${esc(manifest.name)} · generated by tools/press-kit.js</footer>
</main></body></html>`
fs.writeFileSync(path.join(out, 'index.html'), html)

// ── zip assets ───────────────────────────────────────────────────────────────
try { execFileSync('zip', ['-rq', `${app}-presskit.zip`, 'assets'], { cwd: out }) } catch {}

// ── report ───────────────────────────────────────────────────────────────────
const miss = []
if (!press.body) miss.push('press.md (copy — using de:meta/manifest fallback)')
if (!studio.fm.developer) miss.push('apps/studio.md (developer/contact/about)')
if (!shots.length) miss.push('screenshots (--shots dir empty)')
if (!trailer) miss.push('trailer (--trailer)')
if (!icon) miss.push('icon (--icon or manifest.icon)')
if (!press.fm.releaseDate) miss.push('releaseDate (press.md frontmatter)')
console.log(`\npress kit → ${path.relative(ROOT, path.join(out, 'index.html'))}`)
console.log(`  ${shots.length} screenshot(s)${trailer ? ' + trailer' : ''}${icon ? ' + icon' : ''}, assets zipped`)
if (miss.length) { console.log('  incomplete (fine for a draft):'); miss.forEach(m => console.log('    · ' + m)) }
else console.log('  ✓ complete')
console.log(`\n  preview:  open ${path.relative(ROOT, path.join(out, 'index.html'))}\n`)
