#!/usr/bin/env node
// build-site.js — build playable web (wasm) versions of carts + a gallery page
// into site/, ready for GitHub Pages (https://nikkikoole.github.io/dreamengine/).
//
//   node tools/build-site.js <name> [<name>...]   build cart(s), refresh gallery
//   node tools/build-site.js --gallery            regenerate site/index.html only
//   options: --force   rebuild even if outputs look fresh
//
// Each cart becomes site/<name>/index.html (+ .js/.wasm, fully self-contained:
// sprites/map/font are compiled in) plus its .cart.png as thumbnail. The gallery
// lists every cart that has a built site/<name>/ — so the page grows as you
// build more carts. Compilation mirrors the editor's "Build for web"
// (editor/electron/main.cjs studio:build-web): emcc + runtime/raylib-web +
// runtime/web_shell.html. Per-cart workdirs live in build/.site/<name>/ so this
// never touches the shared build/ files another agent may be using.

const fs   = require('fs')
const path = require('path')
const { execFileSync } = require('child_process')
const mk   = require('./make-cart.js')

const ROOT       = path.join(__dirname, '..')
const RUNTIME    = path.join(ROOT, 'runtime')
const RAYLIB_WEB = path.join(RUNTIME, 'raylib-web')
const CARTS_DIR  = path.join(ROOT, 'tools', 'carts')
const PNG_DIR    = path.join(ROOT, 'editor', 'public', 'carts')
const INDEX_JSON = path.join(PNG_DIR, 'index.json')
const SITE_DIR   = path.join(ROOT, 'site')

// ── tiny helpers ──────────────────────────────────────────────
function cHeader(symbol, buf) {
  // xxd -i equivalent, no external dependency
  if (buf.length === 0)
    return `static const unsigned char ${symbol}[] = {0};\nstatic const unsigned int  ${symbol}_LEN = 0;\n`
  const lines = []
  for (let i = 0; i < buf.length; i += 12)
    lines.push('  ' + [...buf.slice(i, i + 12)].map(b => '0x' + b.toString(16).padStart(2, '0')).join(', '))
  return `static const unsigned char ${symbol}[] = {\n${lines.join(',\n')}\n};\n` +
         `static const unsigned int  ${symbol}_LEN = ${buf.length};\n`
}

function newestMtime(files) {
  let t = 0
  for (const f of files) {
    try { t = Math.max(t, fs.statSync(f).mtimeMs) } catch {}
  }
  return t
}

function esc(s) {
  return String(s ?? '').replace(/&/g, '&amp;').replace(/</g, '&lt;')
                        .replace(/>/g, '&gt;').replace(/"/g, '&quot;')
}

// ── build one cart to site/<name>/ ────────────────────────────
function buildCart(name, { force = false } = {}) {
  const srcC = path.join(CARTS_DIR, `${name}.c`)
  if (!fs.existsSync(srcC)) { console.error(`✗ ${name}: no ${srcC}`); return false }

  const outDir  = path.join(SITE_DIR, name)
  const outHtml = path.join(outDir, 'index.html')
  const cfgFile = srcC.replace(/\.c$/, '.cart.js')
  const inputs  = [srcC, cfgFile,
    path.join(RUNTIME, 'studio.c'), path.join(RUNTIME, 'studio.h'),
    path.join(RUNTIME, 'web_shell.html')]
  if (!force && fs.existsSync(outHtml) &&
      newestMtime([path.join(outDir, 'index.wasm')]) > newestMtime(inputs)) {
    console.log(`· ${name}: up to date`)
    return true
  }

  const cfg = mk.loadConfig(srcC)
  const SW = cfg.screenW ?? 320, SH = cfg.screenH ?? 200, SC = cfg.scale ?? 4
  const CW = cfg.cellW ?? 16, CH = cfg.cellH ?? 16
  const MW = cfg.mapW ?? 128, MH = cfg.mapH ?? 64

  // per-cart workdir: only the generated asset headers live here
  const work = path.join(ROOT, 'build', '.site', name)
  fs.mkdirSync(work, { recursive: true })
  fs.mkdirSync(outDir, { recursive: true })

  const spritesBuf = cfg.sprites ? mk.buildSpriteSheet(cfg.sprites, cfg.charMap) : Buffer.alloc(0)
  fs.writeFileSync(path.join(work, 'sprites_data.h'), cHeader('SPRITES_DATA', spritesBuf))
  const mapBytes = cfg.map ? mk.buildMap(cfg.map.layout || cfg.map, cfg.map.tiles, MW, MH) : new Uint8Array(0)
  fs.writeFileSync(path.join(work, 'map_data.h'), cHeader('MAP_DATA', Buffer.from(mapBytes)))

  const args = [
    srcC, path.join(RUNTIME, 'studio.c'),
    '-I', RUNTIME, '-I', work, '-I', path.join(RAYLIB_WEB, 'include'),
    '-DPLATFORM_WEB',
    `-DSCREEN_W=${SW}`, `-DSCREEN_H=${SH}`, `-DSCALE=${SC}`,
    `-DMAP_W=${MW}`, `-DMAP_H=${MH}`, `-DCELL_W=${CW}`, `-DCELL_H=${CH}`,
    `-DTOUCH_CONTROLS_DEFAULT=${cfg.touchControls ? 1 : 0}`,
    '-Os', '-fno-delete-null-pointer-checks',
    path.join(RAYLIB_WEB, 'lib', 'libraylib.a'),
    '-s', 'USE_GLFW=3',
    '-s', 'TOTAL_MEMORY=67108864',
    '-s', 'EXPORTED_RUNTIME_METHODS=ccall,HEAPF32',
    '--shell-file', path.join(RUNTIME, 'web_shell.html'),
    '-o', outHtml,
  ]

  process.stdout.write(`⚙ ${name}: emcc… `)
  const t0 = Date.now()
  try {
    execFileSync('emcc', args, { stdio: ['ignore', 'ignore', 'pipe'], timeout: 180000 })
  } catch (e) {
    console.log('FAILED')
    console.error(String(e.stderr || e.message).trim())
    return false
  }

  // per-cart page title + thumbnail (the .cart.png doubles as a shareable cart)
  const meta = cartMeta(name)
  if (meta) {
    const html = fs.readFileSync(outHtml, 'utf8')
      .replace('<title>dreamengine</title>', `<title>${esc(meta.title)} — dreamengine</title>`)
    fs.writeFileSync(outHtml, html)
  }
  const png = path.join(PNG_DIR, `${name}.cart.png`)
  if (fs.existsSync(png)) fs.copyFileSync(png, path.join(outDir, 'cart.png'))

  const kb = Math.round(fs.statSync(path.join(outDir, 'index.wasm')).size / 1024)
  console.log(`ok (${((Date.now() - t0) / 1000).toFixed(1)}s, wasm ${kb} KB)`)
  return true
}

// ── gallery page ──────────────────────────────────────────────
function cartIndex() {
  const raw = JSON.parse(fs.readFileSync(INDEX_JSON, 'utf8'))
  return Array.isArray(raw) ? raw : (raw.carts || [])
}

function cartMeta(name) {
  return cartIndex().find(c => c.file === `${name}.cart.png`)
}

function buildGallery() {
  if (!fs.existsSync(SITE_DIR)) fs.mkdirSync(SITE_DIR, { recursive: true })
  const built = fs.readdirSync(SITE_DIR, { withFileTypes: true })
    .filter(d => d.isDirectory() && fs.existsSync(path.join(SITE_DIR, d.name, 'index.html')))
    .map(d => d.name)

  const entries = []
  for (const name of built) {
    const meta = cartMeta(name) || { title: name, description: '' }
    entries.push({ name, ...meta })
  }
  entries.sort((a, b) => a.title.localeCompare(b.title))

  const cards = entries.map(e => `
    <a class="card" href="${e.name}/">
      <img src="${e.name}/cart.png" alt="${esc(e.title)}" loading="lazy">
      <div class="body">
        <h2>${esc(e.title)}${e.genre ? ` <span class="tag">${esc(e.genre)}</span>` : ''}</h2>
        <p>${esc(e.description)}</p>
      </div>
    </a>`).join('\n')

  fs.writeFileSync(path.join(SITE_DIR, 'index.html'), `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>dreamengine — playable carts</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body { background: #1b1c1f; color: #e8e6e3; font-family: ui-monospace, Menlo, monospace; padding: 24px; }
  header { max-width: 960px; margin: 0 auto 24px; }
  h1 { font-size: 22px; } h1 span { color: #ff6c24; }
  header p { color: #9a948c; margin-top: 6px; font-size: 13px; }
  header a { color: #06b5e0; }
  .grid { max-width: 960px; margin: 0 auto; display: grid; gap: 16px;
          grid-template-columns: repeat(auto-fill, minmax(220px, 1fr)); }
  .card { background: #25262b; border: 1px solid #34353b; border-radius: 8px;
          overflow: hidden; text-decoration: none; color: inherit; display: block; }
  .card:hover { border-color: #ff6c24; }
  .card img { width: 100%; aspect-ratio: 8/5; object-fit: cover; image-rendering: pixelated; display: block; }
  .body { padding: 10px 12px 12px; }
  h2 { font-size: 14px; } .tag { color: #ffa300; font-size: 11px; font-weight: normal; }
  .body p { color: #9a948c; font-size: 12px; margin-top: 4px; }
  footer { max-width: 960px; margin: 32px auto 0; color: #5f5a54; font-size: 12px; }
</style>
</head>
<body>
<header>
  <h1><span>▶</span> dreamengine</h1>
  <p>Little games and toys made with <a href="https://github.com/NikkiKoole/dreamengine">dreamengine</a>,
     a fantasy console where you write C and hit run. Click a cart to play it in your browser.</p>
</header>
<div class="grid">
${cards}
</div>
<footer>${entries.length} cart${entries.length === 1 ? '' : 's'} · arrows + Z/X to play (most carts)</footer>
</body>
</html>
`)
  console.log(`✓ gallery: ${entries.length} cart(s) → site/index.html`)
}

// ── main ──────────────────────────────────────────────────────
const argv  = process.argv.slice(2)
const force = argv.includes('--force')
const names = argv.filter(a => !a.startsWith('--'))

if (!argv.includes('--gallery') && names.length === 0) {
  console.log('usage: node tools/build-site.js <name> [<name>...] [--force]')
  console.log('       node tools/build-site.js --gallery')
  process.exit(1)
}

let ok = true
for (const name of names) ok = buildCart(name, { force }) && ok
buildGallery()
process.exit(ok ? 0 : 1)
