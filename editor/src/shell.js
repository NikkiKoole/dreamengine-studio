import { view, setEditorTheme, setErrorLines } from './main.js'
import './sprite-editor.js'
import { getMapBytes, loadMapBytes } from './map-editor.js'
import { studioDocs } from './studioDocs.js'
import { settings, buildSettingsPanel, applyCartSettings } from './settings.js'

let currentCartName = ''  // set when a cart is loaded; used as the game window title

// ── tab switching ─────────────────────────────────────────────
function switchTab(name) {
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'))
  document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'))
  const tab = document.querySelector(`.tab[data-tab="${name}"]`)
  if (tab) tab.classList.add('active')
  const panel = document.getElementById('panel-' + name)
  if (panel) panel.classList.add('active')
  if (name === 'tutorials') {
    const s = document.getElementById('tutorials-search')
    if (s) setTimeout(() => s.focus(), 0)   // panel must be visible before focus takes
  }
}

document.querySelectorAll('.tab').forEach(btn => {
  btn.addEventListener('click', () => {
    if (btn.disabled) return
    switchTab(btn.dataset.tab)
  })
})

document.addEventListener('click', e => {
  const link = e.target.closest('[data-tab]')
  if (link && !link.classList.contains('tab')) switchTab(link.dataset.tab)
})

// ── help panel ───────────────────────────────────────────────
const sections = [
  { title: 'c basics',   titleNL: 'c basics',     keys: ['include', 'define', 'int', 'float', 'bool', 'void', 'static', 'if', 'for', 'return', 'logical', 'equality', 'comment', 'braces', 'semicolon', 'array'] },
  { title: 'callbacks',  titleNL: 'callbacks',    keys: ['init', 'update', 'draw'] },
  { title: 'graphics',   titleNL: 'tekenen',      keys: ['cls', 'colorkey', 'spr', 'sprf', 'sspr', 'spr_rot', 'sspr_ex', 'pget', 'pset', 'print', 'print_centered', 'print_right', 'print_scaled', 'text_width', 'rect', 'rectfill', 'bar', 'circ', 'circfill', 'oval', 'ovalfill', 'tri', 'trifill', 'tritex', 'line', 'camera', 'follow', 'clip', 'pal', 'pal_reset', 'fade', 'shake'] },
  { title: 'input',      titleNL: 'input',        keys: ['btn', 'btnp', 'BTN_UP', 'BTN_DOWN', 'BTN_LEFT', 'BTN_RIGHT', 'BTN_A', 'BTN_B'] },
  { title: 'touch',      titleNL: 'touch',        keys: ['stick_x', 'stick_y', 'touch_count', 'touch_x', 'touch_y', 'tap', 'touch_controls'] },
  { title: 'mouse',      titleNL: 'muis',         keys: ['mouse_x', 'mouse_y', 'mouse_down', 'mouse_pressed', 'mouse_released', 'mouse_wheel', 'MOUSE_LEFT', 'MOUSE_RIGHT', 'MOUSE_MIDDLE'] },
  { title: 'keyboard',   titleNL: 'toetsenbord',  keys: ['key', 'keyp', 'text_input', 'KEY_SPACE', 'KEY_ENTER', 'KEY_BACKSPACE', 'KEY_ESCAPE', 'KEY_TAB', 'KEY_LEFT', 'KEY_RIGHT', 'KEY_UP', 'KEY_DOWN'] },
  { title: 'patterns',   titleNL: 'patronen',     keys: ['fillp', 'fillp_reset', 'FILL_SOLID', 'FILL_CHECKER', 'FILL_DOTS', 'FILL_HLINES', 'FILL_VLINES', 'FILL_DIAG', 'FILL_GRID'] },
  { title: 'map',        titleNL: 'map',          keys: ['map', 'map_scale', 'mget', 'mset', 'MAP_W', 'MAP_H'] },
  { title: 'noise',     titleNL: 'ruis',       keys: ['noise', 'noise2', 'noise3'] },
  { title: 'turtle',   titleNL: 'schildpad',  keys: ['turtle_home', 'turtle_move', 'turtle_turn', 'turtle_face', 'turtle_at', 'pen_down', 'pen_up', 'pen_color', 'pen_size'] },
  { title: 'easing',     titleNL: 'verloop',      keys: ['ease_in', 'ease_out', 'ease_in_out'] },
  { title: 'math',       titleNL: 'wiskunde',     keys: ['abs', 'min', 'max', 'clamp', 'lerp', 'remap', 'distance', 'length', 'fsqrt', 'angle_to', 'dx', 'dy', 'sin_deg', 'cos_deg'] },
  { title: 'collision',  titleNL: 'botsingen',    keys: ['boxes_touch', 'point_in_box', 'circles_touch', 'near', 'touching_map', 'tile_at', 'touching_color', 'bounce_at_edges'] },
  { title: 'animation',  titleNL: 'animatie',     keys: ['anim', 'anim_once', 'blink'] },
  { title: 'strings',    titleNL: 'tekst',        keys: ['str'] },
  { title: 'sound',      titleNL: 'geluid',       keys: ['sfx', 'music', 'note', 'hit', 'instrument', 'instrument_duty', 'instrument_lfo', 'instrument_filter', 'tone', 'chord', 'strum', 'schedule', 'bpm', 'beat', 'beat_pos', 'every', 'euclid', 'chance', 'degree', 'INSTR_SQUARE', 'INSTR_SAW', 'INSTR_TRI', 'INSTR_NOISE', 'INSTR_SINE', 'LFO_PITCH', 'LFO_DUTY', 'LFO_VOLUME', 'LFO_CUTOFF', 'FILTER_OFF', 'FILTER_LOW', 'FILTER_HIGH', 'FILTER_BAND', 'FILTER_NOTCH', 'SCALE_MAJOR', 'SCALE_MINOR', 'SCALE_PENTA', 'SCALE_PENTA_MIN', 'SCALE_BLUES', 'SCALE_CHROMATIC', 'CHORD_MAJ', 'CHORD_MIN', 'CHORD_DIM', 'CHORD_AUG', 'CHORD_MAJ7', 'CHORD_MIN7', 'CHORD_DOM7', 'CHORD_SUS4', 'CHORD_POWER'] },
  { title: 'utility',    titleNL: 'hulpmiddelen', keys: ['rnd', 'rnd_between', 'rnd_float', 'rnd_float_between', 'now', 'dt', 'epoch', 'frame', 'sgn', 'mid', 'timer', 'timer_reset'] },
  { title: 'persistence', titleNL: 'opslaan', keys: ['save', 'load', 'save_bytes', 'load_bytes'] },
  { title: 'debug',      titleNL: 'debuggen',     keys: ['printh', 'watch', 'watch_visible'] },
  { title: 'screen',     titleNL: 'scherm',       keys: ['SCREEN_W', 'SCREEN_H'] },
  { title: 'palette',    titleNL: 'palet',        keys: ['CLR_BLACK', 'CLR_DARK_BLUE', 'CLR_DARK_PURPLE', 'CLR_DARK_GREEN', 'CLR_BROWN', 'CLR_DARK_GREY', 'CLR_LIGHT_GREY', 'CLR_WHITE', 'CLR_RED', 'CLR_ORANGE', 'CLR_YELLOW', 'CLR_GREEN', 'CLR_BLUE', 'CLR_INDIGO', 'CLR_PINK', 'CLR_LIGHT_PEACH', 'CLR_BROWNISH_BLACK', 'CLR_DARKER_BLUE', 'CLR_DARKER_PURPLE', 'CLR_BLUE_GREEN', 'CLR_DARK_BROWN', 'CLR_DARKER_GREY', 'CLR_MEDIUM_GREY', 'CLR_LIGHT_YELLOW', 'CLR_DARK_RED', 'CLR_DARK_ORANGE', 'CLR_LIME_GREEN', 'CLR_MEDIUM_GREEN', 'CLR_TRUE_BLUE', 'CLR_MAUVE', 'CLR_DARK_PEACH', 'CLR_PEACH'] },
]

const introHTML = {
  en: `
  <h1>dreamengine</h1>
  <p>
    A fantasy console you program in C. Write code on the left, hit
    <span class="kbd">▶ run</span>, and a native game window pops open.
    The runtime gives you a 320×200 canvas, a 32-color palette, a sprite editor
    (next tab), a small synth, and the input + drawing API listed below.
    It's aimed at teens and hobbyists who want to learn real programming through
    making games — no malloc, no build system, no boilerplate.
  </p>
  <p>
    The goal is the smallest possible distance between an idea and a thing that
    moves on screen. Inspired by PICO-8, DIV Game Studio, and BlitzMax.
    Coming eventually: a DIV-style process model so each game object can be its
    own coroutine, an iPad runtime, and a sound tracker to match the sprite editor.
  </p>
  <p>
    <b>New to C?</b> A program is a list of instructions. The runtime calls your
    <span class="kbd">update()</span> 60 times a second, then your
    <span class="kbd">draw()</span>, and repeats. You declare variables like
    <span class="kbd">int x = 5;</span> to remember things between frames, and you
    use <span class="kbd">if</span>, <span class="kbd">for</span>, and function
    calls to make things happen. Hover any keyword in the editor for a quick
    explanation — the <b>c basics</b> section below has a short tour of the
    bits you'll meet first.
  </p>
  <p><a class="tutorials-link" data-tab="tutorials">→ browse example carts</a></p>
`,
  nl: `
  <h1>dreamengine</h1>
  <p>
    Een fantasieconsole die je in C programmeert. Schrijf code aan de linkerkant, klik op
    <span class="kbd">▶ run</span>, en er verschijnt een echt spelvenster.
    De runtime geeft je een 320×200 canvas, een palet van 32 kleuren, een sprite-editor
    (volgende tab), een kleine synth, en de input- en teken-API hieronder.
    Bedoeld voor tieners en hobbyisten die echt willen leren programmeren door
    spellen te maken — geen malloc, geen build-systeem, geen boilerplate.
  </p>
  <p>
    Het doel is de kleinst mogelijke afstand tussen een idee en iets dat over het
    scherm beweegt. Geïnspireerd door PICO-8, DIV Game Studio en BlitzMax.
    Komt later nog: een DIV-achtig procesmodel zodat elk spelobject zijn eigen
    coroutine kan zijn, een iPad-runtime, en een sound-tracker die past bij de sprite-editor.
  </p>
  <p>
    <b>Nog niet bekend met C?</b> Een programma is een lijst met instructies.
    De runtime roept jouw <span class="kbd">update()</span> 60 keer per seconde aan,
    daarna jouw <span class="kbd">draw()</span>, en herhaalt dat. Je onthoudt dingen
    tussen frames met variabelen (<span class="kbd">int x = 5;</span>), en je laat
    dingen gebeuren met <span class="kbd">if</span>, <span class="kbd">for</span> en
    functie-aanroepen. Hover over willekeurig welk sleutelwoord in de editor voor
    een korte uitleg — het hoofdstuk <b>c basics</b> hieronder geeft een rondleiding
    langs de bouwstenen die je het eerst tegenkomt.
  </p>
`,
}

const helpPanel = document.getElementById('panel-help')
let helpLang = localStorage.getItem('helpLang') === 'nl' ? 'nl' : 'en'

function renderHelp() {
  helpPanel.innerHTML = ''

  // language toggle
  const toggle = document.createElement('div')
  toggle.className = 'help-lang-toggle'
  ;[['en', 'EN'], ['nl', 'NL']].forEach(([lang, label]) => {
    const btn = document.createElement('button')
    btn.textContent = label
    if (helpLang === lang) btn.classList.add('active')
    btn.addEventListener('click', () => {
      helpLang = lang
      localStorage.setItem('helpLang', lang)
      renderHelp()
    })
    toggle.appendChild(btn)
  })
  helpPanel.appendChild(toggle)

  const intro = document.createElement('div')
  intro.className = 'help-intro'
  intro.innerHTML = introHTML[helpLang]
  helpPanel.appendChild(intro)

  sections.forEach(({ title, titleNL, keys }) => {
    const section = document.createElement('div')
    section.className = 'help-section'
    section.innerHTML = `<div class="help-section-title">${helpLang === 'nl' ? titleNL : title}</div>`
    keys.forEach(key => {
      const entry = studioDocs[key]
      if (!entry) return
      const text = (helpLang === 'nl' && entry.docNL) ? entry.docNL : entry.doc
      const row = document.createElement('div')
      row.className = 'help-entry'
      row.id        = 'help-entry-' + key

      // for CLR_* entries, pull the hex out of the doc and prepend a swatch
      const hexMatch = key.startsWith('CLR_') ? entry.doc.match(/#[0-9a-fA-F]{6}/) : null
      const swatch = hexMatch ? `<span class="color-swatch" style="background:${hexMatch[0]}"></span>` : ''

      row.innerHTML = `
        <div class="help-sig">${swatch}${entry.sig}</div>
        <div class="help-doc">${text.replace(/\n/g, '<br>')}</div>
      `
      section.appendChild(row)
    })
    helpPanel.appendChild(section)
  })
}

renderHelp()

// jump-to-help from the code editor (cmd/ctrl-click on a documented word)
window.addEventListener('help-jump', (e) => {
  const key = e.detail.key
  const helpTab = document.querySelector('.tab[data-tab="help"]')
  if (helpTab) helpTab.click()
  // wait one frame for the panel to become visible before scrolling
  requestAnimationFrame(() => {
    const el = document.getElementById('help-entry-' + key)
    if (!el) return
    el.scrollIntoView({ behavior: 'instant', block: 'center' })
    el.classList.add('help-flash')
    setTimeout(() => el.classList.remove('help-flash'), 1200)
  })
})

// ── tutorials panel ──────────────────────────────────────────
const tutorialsPanel = document.getElementById('panel-tutorials')

async function loadCartFromUrl(url) {
  const res = await fetch(url)
  const buf = await res.arrayBuffer()
  const cart = await window.studio.loadCartBuffer(new Uint8Array(buf))
  if (cart && cart.ok) {
    applyCart(cart)
    switchTab('code')
  }
}

// fuzzy subsequence match: 0 = no match, higher = better. rewards contiguous
// runs and matches at word boundaries, so "snk" → "snake", "brk" → "breakout".
function fuzzyScore(needle, haystack) {
  needle = needle.toLowerCase()
  haystack = haystack.toLowerCase()
  let score = 0, from = 0, prev = -2, run = 0
  for (const ch of needle) {
    let found = -1
    for (let i = from; i < haystack.length; i++) { if (haystack[i] === ch) { found = i; break } }
    if (found === -1) return 0
    if (found === prev + 1) { run++; score += 6 + run } else { run = 0; score += 1 }
    if (found === 0 || /[\s\-_.0-9]/.test(haystack[found - 1])) score += 8   // word start
    prev = found; from = found + 1
  }
  return score
}

// positions in `haystack` that the subsequence `needle` matched, or null. same
// greedy walk as fuzzyScore, so the highlight lines up with what was scored.
function fuzzyMatchIndices(needle, haystack) {
  const n = needle.toLowerCase(), h = haystack.toLowerCase(), idx = []
  let from = 0
  for (const ch of n) {
    let found = -1
    for (let i = from; i < h.length; i++) { if (h[i] === ch) { found = i; break } }
    if (found === -1) return null
    idx.push(found); from = found + 1
  }
  return idx
}

function escapeHtml(s) {
  return s.replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]))
}

// contiguous substring match for long prose (descriptions) — fuzzy subsequence
// looks like confetti across a paragraph, so descriptions match literally only.
// returns all non-overlapping occurrence positions, or null. ignores 1-char queries.
function substringIndices(needle, haystack) {
  const n = needle.toLowerCase(), h = haystack.toLowerCase()
  if (n.length < 2) return null
  const idx = []
  let from = 0, pos
  while ((pos = h.indexOf(n, from)) !== -1) {
    for (let i = 0; i < n.length; i++) idx.push(pos + i)
    from = pos + n.length
  }
  return idx.length ? idx : null
}

function substringScore(needle, haystack) {
  const idx = substringIndices(needle, haystack)
  if (!idx) return 0
  const pos = idx[0]
  let s = 10
  if (pos === 0 || /[\s\-_.]/.test(haystack[pos - 1])) s += 8   // match starts a word
  s += Math.max(0, 12 - pos) * 0.2                              // earlier in the blurb = better
  return s
}

// wrap the matched positions in <mark>, escaping everything else. contiguous
// matched runs collapse into one <mark>.
function markRanges(text, indices) {
  const set = new Set(indices)
  let out = '', i = 0
  while (i < text.length) {
    const on = set.has(i)
    let j = i
    while (j < text.length && set.has(j) === on) j++
    const chunk = escapeHtml(text.slice(i, j))
    out += on ? `<mark>${chunk}</mark>` : chunk
    i = j
  }
  return out
}

async function buildTutorialsPanel() {
  tutorialsPanel.innerHTML = ''

  let index
  try {
    const res = await fetch('/carts/index.json')
    index = await res.json()
  } catch {
    tutorialsPanel.innerHTML = '<p class="tutorials-empty">No tutorials found.</p>'
    return
  }

  const search = document.createElement('input')
  search.id = 'tutorials-search'
  search.className = 'tutorials-search'
  search.type = 'search'
  search.placeholder = 'search carts…'
  search.autocomplete = 'off'
  tutorialsPanel.appendChild(search)

  const grid = document.createElement('div')
  grid.className = 'tutorials-grid'
  tutorialsPanel.appendChild(grid)

  const noMatch = document.createElement('p')
  noMatch.className = 'tutorials-empty'
  noMatch.textContent = 'no carts match'
  noMatch.style.display = 'none'
  tutorialsPanel.appendChild(noMatch)

  const items = index.map(({ title, description, file }, idx) => {
    const card = document.createElement('div')
    card.className = 'tutorial-card'

    const url = `/carts/${file}`

    const img = document.createElement('img')
    img.className = 'tutorial-thumb'
    img.src = url
    img.alt = title
    img.onerror = () => { img.style.display = 'none'; card.classList.add('no-thumb') }

    const info = document.createElement('div')
    info.className = 'tutorial-info'
    const titleEl = document.createElement('div')
    titleEl.className = 'tutorial-title'
    titleEl.textContent = title
    const descEl = document.createElement('div')
    descEl.className = 'tutorial-desc'
    descEl.textContent = description
    info.appendChild(titleEl)
    info.appendChild(descEl)

    card.appendChild(img)
    card.appendChild(info)

    if (window.studio) {
      card.addEventListener('click', () => { currentCartName = title; loadCartFromUrl(url) })
    } else {
      card.classList.add('tutorial-card-disabled')
      card.title = 'run requires the desktop app'
    }

    grid.appendChild(card)
    return { card, titleEl, descEl, idx, title: title || '', desc: description || '', name: String(file).replace(/\.cart\.png$/i, '') }
  })

  function applyFilter() {
    const q = search.value.trim()
    if (!q) {                                    // empty → show all in original order, plain text
      items.forEach(it => {
        it.card.style.display = ''
        it.titleEl.textContent = it.title
        it.descEl.textContent = it.desc
        grid.appendChild(it.card)
      })
      noMatch.style.display = 'none'
      return
    }
    // title matches outrank filename, which outrank description blurb
    const scored = items.map(it => ({
      it, s: Math.max(fuzzyScore(q, it.title) * 3, fuzzyScore(q, it.name) * 2, substringScore(q, it.desc)),
    }))
    scored.forEach(({ it, s }) => {
      it.card.style.display = s > 0 ? '' : 'none'
      if (s > 0) {                               // title: fuzzy (short id); description: literal substring only
        const ti = fuzzyMatchIndices(q, it.title)
        it.titleEl.innerHTML = ti ? markRanges(it.title, ti) : escapeHtml(it.title)
        const di = substringIndices(q, it.desc)
        it.descEl.innerHTML = di ? markRanges(it.desc, di) : escapeHtml(it.desc)
      }
    })
    const hits = scored.filter(x => x.s > 0).sort((a, b) => b.s - a.s)
    hits.forEach(x => grid.appendChild(x.it.card))   // re-order visible cards best-first
    noMatch.style.display = hits.length ? 'none' : ''
  }

  search.addEventListener('input', applyFilter)
  search.addEventListener('keydown', e => { if (e.key === 'Escape') { search.value = ''; applyFilter() } })
}

buildTutorialsPanel()

// ── settings panel ───────────────────────────────────────────
buildSettingsPanel(document.getElementById('panel-settings'))

// ── day/night theme ──────────────────────────────────────────
const themeBtn = document.getElementById('theme-btn')
function applyTheme(mode) {
  document.documentElement.classList.toggle('day', mode === 'day')
  themeBtn.textContent = mode === 'day' ? '☀' : '☾'
  themeBtn.title = mode === 'day' ? 'switch to night' : 'switch to day'
  setEditorTheme(mode)
}
applyTheme(localStorage.getItem('theme') || 'night')
themeBtn.addEventListener('click', () => {
  const next = document.documentElement.classList.contains('day') ? 'night' : 'day'
  localStorage.setItem('theme', next)
  applyTheme(next)
})

// ── run button ────────────────────────────────────────────────
const runBtn  = document.getElementById('run-btn')
const buildLog = document.getElementById('build-log')

runBtn.addEventListener('click', async () => {
  if (!window.studio) {
    showLog({ ok: false, cmd: null, output: 'run requires the desktop app  (npm start)' })
    return
  }

  const code = view.state.doc.toString()
  setErrorLines([])
  runBtn.textContent = '⏳ compiling…'
  runBtn.disabled = true
  buildLog.style.display = 'none'
  rlogClear()

  // export sprite sheet from editor canvas before compiling
  const tilemapCanvas = document.querySelector('#tilemap-canvas')
  if (tilemapCanvas) {
    await window.studio.saveSprites(tilemapCanvas.toDataURL('image/png'))
  }

  // export the current map as raw bytes
  await window.studio.saveMap(getMapBytes())

  const result = await window.studio.run(code, { ...settings, cartName: currentCartName })

  runBtn.textContent = '▶ run'
  runBtn.disabled = false

  showLog(result)
})

// ── cart save / load ──────────────────────────────────────────
const saveCartBtn = document.getElementById('save-cart-btn')
const loadCartBtn = document.getElementById('load-cart-btn')

saveCartBtn.addEventListener('click', async () => {
  if (!window.studio) return
  const tilemapCanvas = document.querySelector('#tilemap-canvas')
  const spritesDataUrl = tilemapCanvas ? tilemapCanvas.toDataURL('image/png') : null
  const mapBytes = getMapBytes()
  let mapBase64 = ''
  if (mapBytes) {
    let bin = ''
    for (let i = 0; i < mapBytes.length; i++) bin += String.fromCharCode(mapBytes[i])
    mapBase64 = btoa(bin)
  }
  const cartSettings = {
    screenW: settings.screenW, screenH: settings.screenH, scale: settings.scale,
    cellW: settings.cellW, cellH: settings.cellH, mapW: settings.mapW, mapH: settings.mapH,
  }
  await window.studio.saveCart({ source: view.state.doc.toString(), spritesDataUrl, mapBase64, settings: cartSettings })
})

const toast = document.getElementById('toast')
let toastTimer = null
function showToast(msg) {
  toast.textContent = msg
  toast.classList.add('visible')
  clearTimeout(toastTimer)
  toastTimer = setTimeout(() => toast.classList.remove('visible'), 2000)
}

function applyCart(cart) {
  // run the cart at the config it was authored for (or safe defaults if it
  // carries none) — not whatever globals the editor currently holds
  applyCartSettings(cart.settings)
  buildSettingsPanel(document.getElementById('panel-settings'))
  view.dispatch(view.state.update({
    changes: { from: 0, to: view.state.doc.length, insert: cart.source },
  }))
  if (cart.spritesDataUrl) {
    window.dispatchEvent(new CustomEvent('de:load-sprites', { detail: cart.spritesDataUrl }))
  }
  if (cart.mapBase64) {
    const bin = atob(cart.mapBase64)
    const bytes = new Uint8Array(bin.length)
    for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i)
    loadMapBytes(bytes)
  }
  showToast('cart loaded')
}

loadCartBtn.addEventListener('click', async () => {
  if (!window.studio) return
  const cart = await window.studio.loadCart()
  if (cart && cart.ok) { if (cart.name) currentCartName = cart.name; applyCart(cart) }
})

// ── build for web ─────────────────────────────────────────────
const buildWebBtn = document.getElementById('build-web-btn')

buildWebBtn.addEventListener('click', async () => {
  if (!window.studio) return
  const tilemapCanvas = document.querySelector('#tilemap-canvas')
  if (tilemapCanvas) await window.studio.saveSprites(tilemapCanvas.toDataURL('image/png'))
  await window.studio.saveMap(getMapBytes())

  buildWebBtn.textContent = 'building…'
  buildWebBtn.disabled = true
  rlogClear()   // open the runtime log panel for step-by-step output

  const code = view.state.doc.toString()
  const result = await window.studio.buildWeb(code, settings)

  buildWebBtn.textContent = 'build for web'
  buildWebBtn.disabled = false

  if (result.ok) {
    showToast(`opening ${result.url}`)
  } else {
    showLog(result)
  }
})

// ── drag-and-drop cart loading ────────────────────────────────
document.addEventListener('dragover', e => e.preventDefault())
document.addEventListener('drop', async e => {
  e.preventDefault()
  const file = e.dataTransfer.files[0]
  if (!file || !file.name.endsWith('.png')) return
  const filePath = window.studio.getFilePath(file)
  const cart = await window.studio.loadCartFile(filePath)
  if (cart && cart.ok) { if (cart.name) currentCartName = cart.name; applyCart(cart) }
})

let hideTimer = null

function showLog(result) {
  clearTimeout(hideTimer)

  buildLog.style.display = 'block'
  buildLog.innerHTML = ''

  // close button
  const close = document.createElement('button')
  close.className = 'build-close'
  close.textContent = '×'
  close.addEventListener('click', () => {
    clearTimeout(hideTimer)
    buildLog.style.display = 'none'
  })
  buildLog.appendChild(close)

  if (result.cmd) {
    const cmdLine = document.createElement('div')
    cmdLine.className = 'build-cmd'
    cmdLine.textContent = '$ ' + result.cmd
    buildLog.appendChild(cmdLine)
  }

  if (result.ok) {
    const ok = document.createElement('div')
    ok.className = 'build-ok'
    ok.textContent = `✓ compiled → ${result.bin}`
    buildLog.appendChild(ok)

    const src = document.createElement('div')
    src.className = 'build-meta'
    src.textContent = `  source   → ${result.src}`
    buildLog.appendChild(src)

    // auto-hide after 3s on success
    hideTimer = setTimeout(() => { buildLog.style.display = 'none' }, 3000)
  }

  if (result.output) {
    const out = document.createElement('div')
    out.className = result.ok ? 'build-warn' : 'build-error'
    out.textContent = result.output
    buildLog.appendChild(out)

    if (!result.ok) {
      // parse cart.c:LINE:COL: error/note/warning — mark error lines in the editor
      const lines = []
      for (const m of result.output.matchAll(/cart\.c:(\d+):/g)) {
        const n = parseInt(m[1])
        if (!lines.includes(n)) lines.push(n)
      }
      if (lines.length) setErrorLines(lines)
    }
  }
}

// ── runtime log panel (printh() output + exit/crash banner) ───
// Docked at the bottom of the window. Auto-opens on output, the ×
// collapses it, and it clears on every ▶ run.
const runtimeLog = document.getElementById('runtime-log')
const RLOG_MAX_LINES = 1000
let rlogBody = null
let rlogPartial = ''       // holds a trailing line fragment until its newline arrives
let rlogHadOutput = false  // did the cart print anything this run? (drives auto-hide)
let rlogHideTimer = null

;(function buildRuntimeLog() {
  const head = document.createElement('div')
  head.className = 'rlog-head'

  const title = document.createElement('span')
  title.className = 'rlog-title'
  title.textContent = 'runtime log'

  const clearBtn = document.createElement('button')
  clearBtn.textContent = 'clear'
  clearBtn.addEventListener('click', rlogClear)

  const closeBtn = document.createElement('button')
  closeBtn.textContent = '×'
  closeBtn.title = 'collapse (reopens on new output)'
  closeBtn.addEventListener('click', () => runtimeLog.classList.remove('open'))

  head.append(title, clearBtn, closeBtn)

  rlogBody = document.createElement('div')
  rlogBody.className = 'rlog-body'

  runtimeLog.append(head, rlogBody)
})()

// strip ANSI escape sequences (raylib / terminal coloring)
function stripAnsi(s) {
  return s.replace(/\x1b\[[0-9;?]*[ -/]*[@-~]/g, '')
}

function rlogClear() {
  if (rlogBody) rlogBody.innerHTML = ''
  rlogPartial = ''
  rlogHadOutput = false
  clearTimeout(rlogHideTimer)
}

function rlogAddLine(text, cls) {
  const line = document.createElement('div')
  if (cls) line.className = cls
  line.textContent = text
  rlogBody.appendChild(line)
  while (rlogBody.childElementCount > RLOG_MAX_LINES) {
    rlogBody.removeChild(rlogBody.firstChild)
  }
  rlogBody.scrollTop = rlogBody.scrollHeight
}

function rlogAppend(chunk) {
  clearTimeout(rlogHideTimer)            // real output → keep the panel open
  rlogHadOutput = true
  runtimeLog.classList.add('open')
  const text = rlogPartial + stripAnsi(chunk)
  const parts = text.split('\n')
  rlogPartial = parts.pop()              // last element is an incomplete line
  for (const p of parts) rlogAddLine(p)
}

function rlogExit(info) {
  const { code, signal } = info || {}
  if (rlogPartial) { rlogAddLine(rlogPartial); rlogPartial = '' }
  runtimeLog.classList.add('open')
  const cleanExit = !signal && (code === 0 || code == null)
  if (signal) {
    rlogAddLine(`─── cart crashed (${signal}) ───`, 'rlog-crash')
  } else if (cleanExit) {
    rlogAddLine('─── cart exited (code 0) ───', 'rlog-exit')
  } else {
    rlogAddLine(`─── cart exited (code ${code}) ───`, 'rlog-crash')
  }
  // nothing to read (clean exit, no printh output) → auto-hide like the build log
  if (cleanExit && !rlogHadOutput) {
    clearTimeout(rlogHideTimer)
    rlogHideTimer = setTimeout(() => runtimeLog.classList.remove('open'), 3000)
  }
}

if (window.studio?.onLog) {
  window.studio.onLog(rlogAppend)
  window.studio.onExit(rlogExit)
}

