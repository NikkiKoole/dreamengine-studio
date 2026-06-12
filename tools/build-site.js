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
const { lint } = require('./mobile-lint.js')

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
function buildCart(name, { force = false, worklet = false } = {}) {
  const srcC = path.join(CARTS_DIR, `${name}.c`)
  if (!fs.existsSync(srcC)) { console.error(`✗ ${name}: no ${srcC}`); return false }

  const outDir  = path.join(SITE_DIR, name)
  const outHtml = path.join(outDir, 'index.html')
  const cfgFile = srcC.replace(/\.c$/, '.cart.js')
  const cfg = mk.loadConfig(srcC)

  // AudioWorklet backend (design/audio-threading.md): AUTO-ON for instrument-kind carts
  // (incl. radios — all 22 are kind:instrument), or any cart with `worklet:true`; OFF with
  // `worklet:false`; forced by --worklet. A worklet cart ships BOTH a worklet build and a
  // ScriptProcessor fallback + a loader that auto-picks (built in the build section below).
  const meta = cartMeta(name)
  const kindWorklet = (meta?.kind || []).includes('instrument')
  const wantWorklet = worklet || (cfg.worklet !== false && (cfg.worklet === true || kindWorklet))

  const inputs = [srcC, cfgFile,
    path.join(RUNTIME, 'studio.c'), path.join(RUNTIME, 'studio.h'), path.join(RUNTIME, 'sound.h'),
    path.join(RUNTIME, wantWorklet ? 'web_shell_worklet.html' : 'web_shell.html')]
  if (wantWorklet) inputs.push(path.join(RUNTIME, 'coi-serviceworker.js'), path.join(RUNTIME, 'audio-worklet-stub.js'))
  const checkWasm = path.join(outDir, wantWorklet ? 'worklet.wasm' : 'index.wasm')
  if (!force && fs.existsSync(outHtml) && newestMtime([checkWasm]) > newestMtime(inputs)) {
    console.log(`· ${name}: up to date`)
    return true
  }

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

  const baseArgs = [
    srcC, path.join(RUNTIME, 'studio.c'),
    '-I', RUNTIME, '-I', work, '-I', path.join(RAYLIB_WEB, 'include'),
    '-DPLATFORM_WEB',
    `-DSCREEN_W=${SW}`, `-DSCREEN_H=${SH}`, `-DSCALE=${SC}`,
    `-DMAP_W=${MW}`, `-DMAP_H=${MH}`, `-DCELL_W=${CW}`, `-DCELL_H=${CH}`,
    `-DTOUCH_CONTROLS_DEFAULT=${cfg.touchControls ? 1 : 0}`,
    `-DRENDER_EVERY=${cfg.renderEvery ?? 1}`,   // present every Nth tick (web heat lever); 1 = every tick
    '-Os', '-fno-delete-null-pointer-checks',
    path.join(RAYLIB_WEB, 'lib', 'libraylib.a'),
    '-s', 'USE_GLFW=3',
    '-s', 'TOTAL_MEMORY=67108864',
    '-s', 'EXPORTED_RUNTIME_METHODS=ccall,HEAPF32',
    '--post-js', path.join(RUNTIME, 'web_midi.js'),   // Web MIDI → midi_get() bridge
  ]
  const workletFlags = [
    '-DDE_AUDIO_WORKLET', '-sAUDIO_WORKLET=1', '-sWASM_WORKERS=1',
    '--js-library', path.join(RUNTIME, 'audio-worklet-stub.js'),
  ]
  const runEmcc = (extra) => {
    try { execFileSync('emcc', [...baseArgs, ...extra], { stdio: ['ignore', 'ignore', 'pipe'], timeout: 180000 }); return true }
    catch (e) { console.log('FAILED'); console.error(String(e.stderr || e.message).trim()); return false }
  }

  process.stdout.write(`⚙ ${name}${wantWorklet ? ' (worklet+plain)' : ''}: emcc… `)
  const t0 = Date.now()
  let buildOk
  if (wantWorklet) {
    buildOk = runEmcc(['-o', path.join(outDir, 'plain.js')])                       // ScriptProcessor fallback
           && runEmcc([...workletFlags, '-o', path.join(outDir, 'worklet.js')])     // AudioWorklet (shared memory)
    if (buildOk) {
      fs.copyFileSync(path.join(RUNTIME, 'web_shell_worklet.html'), outHtml)        // loader + coi + self-heal
      fs.copyFileSync(path.join(RUNTIME, 'coi-serviceworker.js'), path.join(outDir, 'coi-serviceworker.js'))
      // drop stale single-build outputs (the loader uses plain.js/worklet.js, not index.js)
      for (const f of ['index.js', 'index.wasm'])
        { const p = path.join(outDir, f); if (fs.existsSync(p)) fs.unlinkSync(p) }
    }
  } else {
    buildOk = runEmcc(['--shell-file', path.join(RUNTIME, 'web_shell.html'), '-o', outHtml])
  }
  if (!buildOk) return false

  finishCart(name)

  const wasmName = wantWorklet ? 'worklet.wasm' : 'index.wasm'
  const kb = Math.round(fs.statSync(path.join(outDir, wasmName)).size / 1024)
  console.log(`ok (${((Date.now() - t0) / 1000).toFixed(1)}s, wasm ${kb} KB)`)
  return true
}

// ── finish one cart dir: title, web-app manifest + icon, thumbnail ──────────
// Shared by the normal build and by `--finish <name>` (the editor's publish
// button compiles straight into site/<name>/ and then calls this). The manifest
// link is injected here, not in web_shell.html, so the editor's build-web
// preview (no manifest on disk) stays 404-free. Idempotent: the title replace
// no-ops on an already-finished page. Unregistered carts (no index.json entry
// yet) get their bare name as title and a placeholder thumbnail.
function finishCart(name) {
  const outDir  = path.join(SITE_DIR, name)
  const outHtml = path.join(outDir, 'index.html')
  if (!fs.existsSync(outHtml)) { console.error(`✗ ${name}: no ${outHtml} to finish`); return false }
  const meta  = cartMeta(name)
  const title = meta?.title || name
  const html = fs.readFileSync(outHtml, 'utf8')
    .replace('<title>dreamengine</title>',
      `<title>${esc(title)} — dreamengine</title>\n` +
      `  <link rel="manifest" href="manifest.json">\n` +
      `  <link rel="apple-touch-icon" href="cart.png">`)
  fs.writeFileSync(outHtml, html)
  fs.writeFileSync(path.join(outDir, 'manifest.json'), JSON.stringify({
    name: title, short_name: title, display: 'fullscreen',
    background_color: '#1b1c1f', theme_color: '#1b1c1f', start_url: './',
    icons: [{ src: 'cart.png', sizes: '320x200', type: 'image/png' }],
  }, null, 2))
  const png = path.join(PNG_DIR, `${name}.cart.png`)
  if (fs.existsSync(png)) fs.copyFileSync(png, path.join(outDir, 'cart.png'))
  else if (!fs.existsSync(path.join(outDir, 'cart.png')))
    fs.writeFileSync(path.join(outDir, 'cart.png'), mk.makePlaceholderPng())
  return true
}

// ── reshell: re-apply the worklet loader (runtime/web_shell_worklet.html) to every
// already-built worklet cart in site/ WITHOUT recompiling the wasm. Use after editing the
// shell (e.g. the audio-backend chooser / coi gate) to push it to all published carts.
// Reuses finishCart() so each cart keeps its own title/manifest/icon. A worklet cart is
// any site/<name>/ that ships a worklet.js (the chooser's dedicated-thread build).
function reshellWorkletCarts() {
  const dirs = fs.readdirSync(SITE_DIR, { withFileTypes: true })
    .filter(d => d.isDirectory() && fs.existsSync(path.join(SITE_DIR, d.name, 'worklet.js')))
    .map(d => d.name)
  let n = 0
  for (const name of dirs) {
    const outDir = path.join(SITE_DIR, name)
    fs.copyFileSync(path.join(RUNTIME, 'web_shell_worklet.html'), path.join(outDir, 'index.html'))
    fs.copyFileSync(path.join(RUNTIME, 'coi-serviceworker.js'), path.join(outDir, 'coi-serviceworker.js'))
    finishCart(name)   // re-injects the cart's <title>/manifest/icon into the fresh shell
    n++
  }
  console.log(`✓ reshelled ${n} worklet cart(s) from runtime/web_shell_worklet.html`)
  return n
}

// ── gallery page ──────────────────────────────────────────────
function cartIndex() {
  const raw = JSON.parse(fs.readFileSync(INDEX_JSON, 'utf8'))
  return Array.isArray(raw) ? raw : (raw.carts || [])
}

function cartMeta(name) {
  return cartIndex().find(c => c.file === `${name}.cart.png`)
}

// phone-playability badge, four tiers. "ready" is strict: any dead input path
// on a touch screen demotes — if there's a keyboard button to press somewhere,
// it's not perfect. The split below "ready": dead EXTRAS (keys/right-click read
// alongside a working tap path, or watch-only radios — the music plays, the
// key controls don't) = mostly; a dead CORE interaction (hover/wheel — things
// a touch screen simply doesn't have) = rough. "fixable" is a worklist term —
// until touchControls actually lands the cart needs a keyboard, so: desktop.
// A hand-tested `"mobile": "ready"|"mostly"|"rough"|"desktop"` in the cart's
// index.json entry beats the static guess (the lint can't see key-gated title
// screens etc.).
const BADGES = {
  ready:   { cls: 'b-ready',   text: '🟢 Mobile Ready — plays perfectly' },
  mostly:  { cls: 'b-mostly',  text: '🟡 Mostly Playable — works, with rough edges' },
  rough:   { cls: 'b-rough',   text: '🟠 Rough on Mobile — technically runs, expect pain' },
  desktop: { cls: 'b-desktop', text: '🔴 Desktop Only — good luck past the title screen' },
}
const DEAD_EXTRA = /^(also-reads-keys|btn-without-overlay|right\/middle|touch>5)/
const DEAD_CORE  = /^(hover|wheel)/

function mobileTier(name, meta) {
  if (BADGES[meta.mobile]) return meta.mobile        // manual verdict wins
  const r = lint(name)
  if (r.verdict === 'fixable' || r.verdict === 'keyboard-only') return 'desktop'
  if (r.warnings.some(w => DEAD_CORE.test(w)))  return 'rough'
  if (r.verdict === 'no-input')                 return 'mostly'  // watch/listen-only
  if (r.warnings.some(w => DEAD_EXTRA.test(w))) return 'mostly'
  return 'ready'
}

// "date added" = the cart's FIRST commit into the repo (its .cart.png landing
// in editor/public/carts/), read from git history at gallery-build time. This
// survives bulk publishes: redeploying all of site/ at once doesn't flatten
// the timeline, because the date never came from site/ in the first place.
function dateAdded(name) {
  try {
    const out = execFileSync('git',
      ['log', '--diff-filter=A', '--format=%as', '--', `editor/public/carts/${name}.cart.png`],
      { cwd: ROOT, encoding: 'utf8' }).trim()
    return out ? out.split('\n').pop() : ''   // log is newest-first; the A(dd) is last
  } catch (e) { return '' }
}

const TIER_RANK = { ready: 0, mostly: 1, rough: 2, desktop: 3 }

function buildGallery() {
  if (!fs.existsSync(SITE_DIR)) fs.mkdirSync(SITE_DIR, { recursive: true })
  // the on-device debug overlay (mobile-web-notes §6d) — one site-root copy
  // shared by every cart; the shell only bakes a tiny loader for it
  fs.copyFileSync(path.join(RUNTIME, 'debug-overlay.js'), path.join(SITE_DIR, 'debug-overlay.js'))
  const built = fs.readdirSync(SITE_DIR, { withFileTypes: true })
    .filter(d => d.isDirectory() && fs.existsSync(path.join(SITE_DIR, d.name, 'index.html')))
    .map(d => d.name)

  const entries = []
  for (const name of built) {
    const meta = cartMeta(name) || { title: name, description: '' }
    // dual-backend = ships a worklet.js alongside the plain build → responds to the Audio toggle
    const dual = fs.existsSync(path.join(SITE_DIR, name, 'worklet.js'))
    entries.push({ name, added: dateAdded(name), tier: mobileTier(name, meta), dual, ...meta })
  }
  // server-side default order = newest first (the client re-sorts live)
  entries.sort((a, b) => (b.added || '').localeCompare(a.added || '') || a.title.localeCompare(b.title))

  const cards = entries.map(e => {
    const badge = BADGES[e.tier]
    // data-search feeds the live filter: name + title + genre + kind + description
    const hay = [e.name, e.title, e.genre, ...(e.kind || []), e.description]
      .filter(Boolean).join(' ').toLowerCase()
    return `
    <a class="card" href="${e.name}/" data-title="${esc(e.title.toLowerCase())}" data-search="${esc(hay)}" data-added="${e.added}" data-tier="${TIER_RANK[e.tier] ?? 9}">
      <img src="${e.name}/cart.png" alt="${esc(e.title)}" loading="lazy">
      <div class="body">
        <h2>${esc(e.title)}${e.genre ? ` <span class="tag">${esc(e.genre)}</span>` : ''}${e.dual ? ` <span class="dual" title="Ships two audio backends — a dedicated-thread AudioWorklet build and a plain ScriptProcessor build. Responds to the Audio toggle above.">⚡2×audio</span>` : ''}</h2>
        ${badge ? `<div class="badge ${badge.cls}">${badge.text}</div>` : ''}
        <p>${esc(e.description)}</p>
        ${e.added ? `<div class="added">added ${e.added}</div>` : ''}
      </div>
    </a>`
  }).join('\n')

  fs.writeFileSync(path.join(SITE_DIR, 'index.html'), `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>dreamengine — playable carts</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  :root { --bg:#1b1c1f; --card:#25262b; --border:#34353b; --fg:#e8e6e3;
          --dim:#9a948c; --faint:#5f5a54; --accent:#ff6c24; --link:#06b5e0; }
  body.day { --bg:#f2efe9; --card:#ffffff; --border:#d8d4cc; --fg:#26241f;
             --dim:#6b665e; --faint:#a59f95; }
  body { background: var(--bg); color: var(--fg); font-family: ui-monospace, Menlo, monospace; padding: 24px; }
  header { max-width: 960px; margin: 0 auto 24px; }
  h1 { font-size: 22px; } h1 span { color: var(--accent); }
  header p { color: var(--dim); margin-top: 6px; font-size: 13px; }
  header a { color: var(--link); }
  .controls { max-width: 960px; margin: 0 auto 16px; display: flex; gap: 6px; align-items: center;
              flex-wrap: wrap; color: var(--faint); font-size: 12px;
              position: sticky; top: 0; z-index: 5; background: var(--bg); padding: 10px 0; }
  .controls input[type=search] { background: var(--card); border: 1px solid var(--border);
                                 color: var(--fg); font: inherit; padding: 4px 10px;
                                 border-radius: 6px; width: 170px; }
  .controls input[type=search]:focus { border-color: var(--accent); outline: none; }
  #count { color: var(--dim); }
  .controls button { background: var(--card); border: 1px solid var(--border); color: var(--dim);
                     font: inherit; padding: 4px 10px; border-radius: 6px; cursor: pointer; }
  .controls button:hover { border-color: var(--accent); }
  .controls button.on { color: var(--fg); border-color: var(--accent); }
  .controls .sep { margin: 0 4px; }
  .grid { max-width: 960px; margin: 0 auto; display: grid; gap: 16px;
          grid-template-columns: repeat(auto-fill, minmax(220px, 1fr)); }
  .card { background: var(--card); border: 1px solid var(--border); border-radius: 8px;
          overflow: hidden; text-decoration: none; color: inherit; display: block; }
  .card:hover { border-color: var(--accent); }
  .card img { width: 100%; aspect-ratio: 8/5; object-fit: cover; image-rendering: pixelated; display: block; }
  .body { padding: 10px 12px 12px; }
  h2 { font-size: 14px; } .tag { color: #ffa300; font-size: 11px; font-weight: normal; }
  .dual { color: var(--link); font-size: 10px; font-weight: normal; white-space: nowrap; }
  .badge { font-size: 11px; margin-top: 4px; }
  .b-ready { color: #00e436; } .b-mostly { color: #ffa300; }
  .b-rough { color: #ff6c24; } .b-desktop { color: var(--dim); }
  .body p { color: var(--dim); font-size: 12px; margin-top: 4px; }
  body.desc-clamp .body p { display: -webkit-box; -webkit-box-orient: vertical;
                            -webkit-line-clamp: 3; overflow: hidden; }
  body.desc-off .body p { display: none; }
  .added { color: var(--faint); font-size: 10px; margin-top: 6px; }
  body.desc-off .added { display: none; }
  footer { max-width: 960px; margin: 32px auto 0; color: var(--faint); font-size: 12px; }
</style>
</head>
<body class="desc-clamp">
<header>
  <h1><span>▶</span> dreamengine</h1>
  <p>Little games and toys made with <a href="https://github.com/NikkiKoole/dreamengine">dreamengine</a>,
     a fantasy console where you write C and hit run. Click a cart to play it in your browser.</p>
</header>
<div class="controls">
  <input id="search" type="search" placeholder="search… (just type)" autocomplete="off">
  <span id="count"></span>
  <span class="sep">·</span>
  <span>sort:</span>
  <button data-sort="added">newest</button>
  <button data-sort="title">a–z</button>
  <button data-sort="tier">mobile</button>
  <span class="sep">·</span>
  <button id="desc-btn">desc: 3 lines</button>
  <span class="sep">·</span>
  <button id="theme-btn">☀ day</button>
  <span class="sep">·</span>
  <button id="audio-btn" title="Audio backend (applies to every cart you open). Auto = dedicated audio thread on desktop/Android, plain ScriptProcessor on iOS. If a cart is silent, switch to Plain; to try the dedicated thread anyway, pick Worklet.">audio: auto</button>
</div>
<div class="grid">
${cards}
</div>
<footer>${entries.length} cart${entries.length === 1 ? '' : 's'} · arrows + Z/X to play (most carts)</footer>
<script>
(function () {
  var grid = document.querySelector('.grid')

  // ── live search: filters cards on name/title/genre/kind/description.
  // Desktop: just start typing anywhere ('/' also focuses). Esc clears.
  // The query round-trips through #q= so back-nav and shared links keep it.
  var search = document.getElementById('search')
  function applyFilter() {
    var q = search.value.trim().toLowerCase(), n = 0
    ;[].slice.call(grid.children).forEach(function (c) {
      var hit = !q || (c.dataset.search || '').indexOf(q) !== -1
      c.style.display = hit ? '' : 'none'
      if (hit) n++
    })
    document.getElementById('count').textContent = q ? n + '/' + grid.children.length : ''
    try {
      history.replaceState(null, '',
        q ? '#q=' + encodeURIComponent(q) : location.pathname + location.search)
    } catch (e) {}
  }
  search.addEventListener('input', applyFilter)
  search.addEventListener('keydown', function (e) {
    if (e.key === 'Escape') { search.value = ''; applyFilter(); search.blur() }
  })
  document.addEventListener('keydown', function (e) {
    if (e.target === search || e.metaKey || e.ctrlKey || e.altKey) return
    if (e.key === '/') { e.preventDefault(); search.focus(); return }
    if (e.key.length === 1) search.focus()   // the keystroke lands in the box
  })
  var qm = /[#&]q=([^&]*)/.exec(location.hash)
  if (qm) { search.value = decodeURIComponent(qm[1]); applyFilter() }

  function applySort(k) {
    var cards = [].slice.call(grid.children)
    cards.sort(function (a, b) {
      if (k === 'added') return (b.dataset.added || '').localeCompare(a.dataset.added || '') ||
                                a.dataset.title.localeCompare(b.dataset.title)
      if (k === 'tier')  return (+a.dataset.tier) - (+b.dataset.tier) ||
                                a.dataset.title.localeCompare(b.dataset.title)
      return a.dataset.title.localeCompare(b.dataset.title)
    })
    cards.forEach(function (c) { grid.appendChild(c) })
    document.querySelectorAll('[data-sort]').forEach(function (b) {
      b.classList.toggle('on', b.dataset.sort === k)
    })
    try { localStorage.setItem('de-sort', k) } catch (e) {}
  }

  var DESC_ORDER = ['clamp', 'full', 'off']
  var DESC_LABEL = { clamp: 'desc: 3 lines', full: 'desc: full', off: 'desc: off' }
  function applyDesc(m) {
    document.body.classList.remove('desc-clamp', 'desc-full', 'desc-off')
    document.body.classList.add('desc-' + m)
    document.getElementById('desc-btn').textContent = DESC_LABEL[m]
    try { localStorage.setItem('de-desc', m) } catch (e) {}
  }

  function applyTheme(t) {
    document.body.classList.toggle('day', t === 'day')
    document.getElementById('theme-btn').textContent = t === 'day' ? '☾ night' : '☀ day'
    try { localStorage.setItem('de-theme', t) } catch (e) {}
  }

  // audio backend pref (key 'de:audio' — the SAME key every cart's loader shell reads).
  // The gallery has no audio itself; this just stores the choice so the next cart you open
  // honors it. auto = iOS plain / desktop worklet; worklet = force it; plain = force fallback.
  var AUDIO_ORDER = ['auto', 'worklet', 'plain']
  var AUDIO_LABEL = { auto: 'audio: auto', worklet: 'audio: worklet', plain: 'audio: plain' }
  function applyAudio(m) {
    if (AUDIO_ORDER.indexOf(m) === -1) m = 'auto'
    document.getElementById('audio-btn').textContent = AUDIO_LABEL[m]
    document.getElementById('audio-btn').classList.toggle('on', m !== 'auto')
    try { localStorage.setItem('de:audio', m) } catch (e) {}
  }

  document.querySelectorAll('[data-sort]').forEach(function (b) {
    b.addEventListener('click', function () { applySort(b.dataset.sort) })
  })
  document.getElementById('desc-btn').addEventListener('click', function () {
    var cur = (localStorage.getItem('de-desc') || 'clamp')
    applyDesc(DESC_ORDER[(DESC_ORDER.indexOf(cur) + 1) % DESC_ORDER.length])
  })
  document.getElementById('theme-btn').addEventListener('click', function () {
    applyTheme(document.body.classList.contains('day') ? 'night' : 'day')
  })
  document.getElementById('audio-btn').addEventListener('click', function () {
    var cur = 'auto'; try { cur = localStorage.getItem('de:audio') || 'auto' } catch (e) {}
    if (AUDIO_ORDER.indexOf(cur) === -1) cur = 'auto'
    applyAudio(AUDIO_ORDER[(AUDIO_ORDER.indexOf(cur) + 1) % AUDIO_ORDER.length])
  })

  var s = 'added', d = 'clamp', t = 'night', a = 'auto'
  try {
    s = localStorage.getItem('de-sort')  || s
    d = localStorage.getItem('de-desc')  || d
    t = localStorage.getItem('de-theme') || t
    a = localStorage.getItem('de:audio') || a
  } catch (e) {}
  applySort(s); applyDesc(d); applyTheme(t); applyAudio(a)
})()
</script>
</body>
</html>
`)
  console.log(`✓ gallery: ${entries.length} cart(s) → site/index.html`)
}

// ── main ──────────────────────────────────────────────────────
const argv    = process.argv.slice(2)
const force   = argv.includes('--force')
const worklet = argv.includes('--worklet')   // AudioWorklet backend build → site/<name>-worklet/
const names = argv.filter(a => !a.startsWith('--'))

if (!argv.includes('--gallery') && !argv.includes('--reshell') && names.length === 0) {
  console.log('usage: node tools/build-site.js <name> [<name>...] [--force]')
  console.log('       node tools/build-site.js --gallery')
  console.log('       node tools/build-site.js --reshell          (re-apply the worklet loader shell to all built worklet carts, no recompile)')
  console.log('       node tools/build-site.js --finish <name>   (post-process an already-compiled site/<name>/)')
  process.exit(1)
}

let ok = true
if (argv.includes('--reshell')) {
  reshellWorkletCarts()
} else if (argv.includes('--finish')) {
  for (const name of names) ok = finishCart(name) && ok
} else {
  for (const name of names) ok = buildCart(name, { force, worklet }) && ok
}
buildGallery()
process.exit(ok ? 0 : 1)
