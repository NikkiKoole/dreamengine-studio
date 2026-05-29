import { view, setEditorTheme, setErrorLines } from './main.js'
import './sprite-editor.js'
import { getMapBytes, loadMapBytes } from './map-editor.js'
import { studioDocs } from './studioDocs.js'
import { settings, buildSettingsPanel } from './settings.js'

// ── tab switching ─────────────────────────────────────────────
function switchTab(name) {
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'))
  document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'))
  const tab = document.querySelector(`.tab[data-tab="${name}"]`)
  if (tab) tab.classList.add('active')
  const panel = document.getElementById('panel-' + name)
  if (panel) panel.classList.add('active')
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
  { title: 'callbacks',  titleNL: 'callbacks',    keys: ['update', 'draw'] },
  { title: 'graphics',   titleNL: 'tekenen',      keys: ['cls', 'colorkey', 'spr', 'sprf', 'sspr', 'pget', 'pset', 'print', 'print_centered', 'print_right', 'rect', 'rectfill', 'circ', 'circfill', 'tri', 'trifill', 'line', 'camera', 'follow', 'clip'] },
  { title: 'input',      titleNL: 'input',        keys: ['btn', 'btnp', 'BTN_UP', 'BTN_DOWN', 'BTN_LEFT', 'BTN_RIGHT', 'BTN_A', 'BTN_B'] },
  { title: 'touch',      titleNL: 'touch',        keys: ['stick_x', 'stick_y', 'touch_count', 'touch_x', 'touch_y', 'tap', 'touch_controls'] },
  { title: 'map',        titleNL: 'map',          keys: ['map', 'map_scale', 'mget', 'mset', 'MAP_W', 'MAP_H'] },
  { title: 'noise',     titleNL: 'ruis',       keys: ['noise', 'noise2', 'noise3'] },
  { title: 'easing',     titleNL: 'verloop',      keys: ['ease_in', 'ease_out', 'ease_in_out'] },
  { title: 'math',       titleNL: 'wiskunde',     keys: ['abs', 'min', 'max', 'clamp', 'lerp', 'remap', 'distance', 'length', 'angle_to', 'dx', 'dy', 'sin_deg', 'cos_deg'] },
  { title: 'collision',  titleNL: 'botsingen',    keys: ['boxes_touch', 'point_in_box', 'circles_touch', 'near', 'touching_map', 'tile_at', 'touching_color', 'bounce_at_edges'] },
  { title: 'animation',  titleNL: 'animatie',     keys: ['anim', 'anim_once'] },
  { title: 'strings',    titleNL: 'tekst',        keys: ['str'] },
  { title: 'sound',      titleNL: 'geluid',       keys: ['sfx', 'music', 'note', 'hit', 'tone', 'chord', 'strum', 'schedule', 'bpm', 'beat', 'beat_pos', 'every', 'euclid', 'chance', 'degree', 'INSTR_SQUARE', 'INSTR_SAW', 'INSTR_TRI', 'INSTR_NOISE', 'INSTR_SINE', 'SCALE_MAJOR', 'SCALE_MINOR', 'SCALE_PENTA', 'SCALE_PENTA_MIN', 'SCALE_BLUES', 'SCALE_CHROMATIC', 'CHORD_MAJ', 'CHORD_MIN', 'CHORD_DIM', 'CHORD_AUG', 'CHORD_MAJ7', 'CHORD_MIN7', 'CHORD_DOM7', 'CHORD_SUS4', 'CHORD_POWER'] },
  { title: 'utility',    titleNL: 'hulpmiddelen', keys: ['rnd', 'rnd_between', 'rnd_float', 'rnd_float_between', 'now', 'frame', 'sgn', 'mid', 'timer', 'timer_reset'] },
  { title: 'persistence', titleNL: 'opslaan', keys: ['save', 'load'] },
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

  const grid = document.createElement('div')
  grid.className = 'tutorials-grid'

  index.forEach(({ title, description, file }) => {
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
    info.innerHTML = `<div class="tutorial-title">${title}</div><div class="tutorial-desc">${description}</div>`

    card.appendChild(img)
    card.appendChild(info)

    if (window.studio) {
      card.addEventListener('click', () => loadCartFromUrl(url))
    } else {
      card.classList.add('tutorial-card-disabled')
      card.title = 'run requires the desktop app'
    }

    grid.appendChild(card)
  })

  tutorialsPanel.appendChild(grid)
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

  const result = await window.studio.run(code, settings)

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
  await window.studio.saveCart({ source: view.state.doc.toString(), spritesDataUrl, mapBase64 })
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
  if (cart && cart.ok) applyCart(cart)
})

// ── drag-and-drop cart loading ────────────────────────────────
document.addEventListener('dragover', e => e.preventDefault())
document.addEventListener('drop', async e => {
  e.preventDefault()
  const file = e.dataTransfer.files[0]
  if (!file || !file.name.endsWith('.png')) return
  const filePath = window.studio.getFilePath(file)
  const cart = await window.studio.loadCartFile(filePath)
  if (cart && cart.ok) applyCart(cart)
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

