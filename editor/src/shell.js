import { view, setEditorTheme, setErrorLines, onDocChange } from './main.js'
import { initOutline, refreshOutline } from './outline.js'
import { ENGINE_SOURCES, showEngineFileIn } from './navigate.js'
import './sprite-editor.js'
import { getMapBytes, loadMapBytes } from './map-editor.js'
import { studioDocs } from './studioDocs.js'
import { settings, buildSettingsPanel, applyCartSettings } from './settings.js'
import { marked } from 'marked'

let currentCartName = ''  // set when a cart is loaded; used as the game window title

// the loaded cart's name lives OUTSIDE the editor chrome: in the macOS menu bar
// (as a top-level menu — the window's titlebar is hidden) and in document.title
// (dock / Mission Control / browser tab). Glance up to see what you're editing.
let currentCartFile = ''   // the cart's FILE slug (zoo, loveparade) — publish uses
                           // this, never the display title ("pixel zoo" ≠ zoo.cart.png)
function setCartName(name) {
  currentCartName = name || ''
  document.title = currentCartName ? `dreamengine — ${currentCartName}` : 'dreamengine'
  window.studio?.setCartName?.(currentCartName)
}

// ── tab switching ─────────────────────────────────────────────
// The "pixels" tab hosts both the sprite editor (#panel-sprites) and the map
// editor (#panel-map); a sub-toggle picks which one is visible. The two panels
// keep their own ids/markup so sprite-editor.js / map-editor.js are unchanged.
let pixMode = 'sprites'

function showPixels() {
  document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'))
  const sub = document.getElementById(pixMode === 'map' ? 'panel-map' : 'panel-sprites')
  if (sub) sub.classList.add('active')
  document.querySelectorAll('.pix-toggle [data-pix]').forEach(b =>
    b.classList.toggle('active', b.dataset.pix === pixMode))
  // entering the map view: let the map editor refresh from the (maybe-changed) spritesheet
  if (pixMode === 'map') window.dispatchEvent(new CustomEvent('de:show-map'))
}

function switchTab(name) {
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'))
  document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'))
  const tab = document.querySelector(`.tab[data-tab="${name}"]`)
  if (tab) tab.classList.add('active')
  if (name === 'pixels') {
    showPixels()
  } else {
    const panel = document.getElementById('panel-' + name)
    if (panel) panel.classList.add('active')
  }
  // keep the outline fresh when returning to the code tab (edits made while on
  // another tab still fire onDocChange, but this also covers the first show)
  if (name === 'code') refreshOutline()
  if (name === 'tutorials') {
    // rebuild on every open so a freshly-added cart shows up without a reload
    buildTutorialsPanel().then(() => {
      const s = document.getElementById('tutorials-search')
      if (s) s.focus()   // panel is visible by now, so focus takes
    })
  }
}

// sub-toggle inside the Pixels tab: switch between sprite and map editing
document.querySelectorAll('.pix-toggle [data-pix]').forEach(btn => {
  btn.addEventListener('click', () => { pixMode = btn.dataset.pix; showPixels() })
})

document.querySelectorAll('.tab').forEach(btn => {
  btn.addEventListener('click', () => {
    if (btn.disabled) return
    switchTab(btn.dataset.tab)
  })
})

// mount the outline sidebar (lists the cart's functions; click to jump). It
// subscribes to onDocChange itself, so it stays live as you type.
initOutline()

document.addEventListener('click', e => {
  const link = e.target.closest('[data-tab]')
  if (link && !link.classList.contains('tab')) switchTab(link.dataset.tab)
})

// ── help panel ───────────────────────────────────────────────
const sections = [
  { title: 'c basics',     keys: ['include', 'define', 'int', 'float', 'bool', 'void', 'static', 'if', 'for', 'return', 'logical', 'equality', 'comment', 'braces', 'semicolon', 'array'] },
  { title: 'callbacks',    keys: ['init', 'update', 'draw'] },
  { title: 'graphics',      keys: ['cls', 'colorkey', 'spr', 'sprf', 'sspr', 'spr_rot', 'sspr_ex', 'pget', 'pset', 'sget', 'font', 'FONT_NORMAL', 'FONT_SMALL', 'FONT_TINY', 'print', 'print_centered', 'print_right', 'print_scaled', 'print_outline', 'text_width', 'rect', 'rectfill', 'bar', 'circ', 'circfill', 'oval', 'ovalfill', 'arc', 'arcfill', 'arcoutline', 'ring', 'ringoutline', 'tri', 'trifill', 'tritex', 'line', 'bezier', 'camera', 'camera_ex', 'follow', 'clip', 'pal', 'pal_reset', 'fade', 'shake'] },
  { title: 'shapes',       keys: ['ngon', 'ngonfill', 'star', 'starfill', 'poly', 'polyfill', 'thickline', 'thicklineoutline', 'rrect', 'rrectfill', 'gradient', 'vgradient', 'hgradient'] },
  { title: 'input',        keys: ['btn', 'btnp', 'btnr', 'BTN_UP', 'BTN_DOWN', 'BTN_LEFT', 'BTN_RIGHT', 'BTN_A', 'BTN_B'] },
  { title: 'touch',        keys: ['stick_x', 'stick_y', 'touch_count', 'touch_x', 'touch_y', 'touch_id', 'tap', 'tapp', 'touch_ended_count', 'touch_ended_id', 'touch_ended_x', 'touch_ended_y', 'tapr', 'touch_controls', 'touch_ceiling'] },
  { title: 'mouse',         keys: ['mouse_x', 'mouse_y', 'mouse_world_x', 'mouse_world_y', 'mouse_down', 'mouse_pressed', 'mouse_released', 'mouse_wheel', 'MOUSE_LEFT', 'MOUSE_RIGHT', 'MOUSE_MIDDLE'] },
  { title: 'keyboard',  keys: ['key', 'keyp', 'keyr', 'text_input', 'KEY_SPACE', 'KEY_ENTER', 'KEY_BACKSPACE', 'KEY_ESCAPE', 'KEY_TAB', 'KEY_LEFT', 'KEY_RIGHT', 'KEY_UP', 'KEY_DOWN'] },
  { title: 'patterns',     keys: ['fillp', 'fillp_reset', 'fillp_anchor', 'FILL_SOLID', 'FILL_CHECKER', 'FILL_DOTS', 'FILL_HLINES', 'FILL_VLINES', 'FILL_DIAG', 'FILL_GRID'] },
  { title: 'map',          keys: ['map', 'map_scale', 'mget', 'mset', 'MAP_W', 'MAP_H'] },
  { title: 'noise',       keys: ['noise', 'noise2', 'noise3'] },
  { title: '3d',           keys: ['V3', 'rot3', 'project3', 'zsort', 'quadfill'] },
  { title: 'easing',      keys: ['ease_in', 'ease_out', 'ease_in_out'] },
  { title: 'math',     keys: ['abs', 'min', 'max', 'clamp', 'lerp', 'remap', 'distance', 'length', 'fsqrt', 'angle_to', 'dx', 'dy', 'sin_deg', 'cos_deg'] },
  { title: 'collision',    keys: ['boxes_touch', 'point_in_box', 'circles_touch', 'near', 'touching_map', 'tile_at', 'touching_color'] },
  { title: 'animation',     keys: ['anim', 'blink'] },
  { title: 'strings',        keys: ['str'] },
  { title: 'sound',       keys: ['sfx', 'note', 'hit', 'note_on', 'note_off', 'note_pitch', 'note_vol', 'note_cutoff', 'note_duty', 'note_res', 'note_lfo', 'note_env', 'note_harmonics', 'note_timbre', 'note_morph', 'note_drive', 'note_echo', 'note_filter', 'note_glide', 'note_off_all', 'instrument', 'instrument_duty', 'instrument_lfo', 'instrument_filter', 'instrument_env', 'instrument_harmonics', 'instrument_timbre', 'instrument_morph', 'instrument_tune', 'instrument_drive', 'instrument_echo', 'instrument_choke', 'echo', 'wave_set', 'tone', 'chord', 'strum', 'schedule', 'schedule_hit', 'bpm', 'beat', 'beat_pos', 'every', 'euclid', 'chance', 'degree', 'INSTR_SQUARE', 'INSTR_SAW', 'INSTR_TRI', 'INSTR_NOISE', 'INSTR_SINE', 'INSTR_USER0', 'INSTR_USER1', 'INSTR_USER2', 'INSTR_USER3', 'INSTR_PLUCK', 'INSTR_MALLET', 'INSTR_FM', 'INSTR_ORGAN', 'LFO_PITCH', 'LFO_DUTY', 'LFO_VOLUME', 'LFO_CUTOFF', 'ENV_CUTOFF', 'ENV_PITCH', 'ENV_DUTY', 'FILTER_OFF', 'FILTER_LOW', 'FILTER_HIGH', 'FILTER_BAND', 'FILTER_NOTCH', 'SCALE_MAJOR', 'SCALE_MINOR', 'SCALE_PENTA', 'SCALE_PENTA_MIN', 'SCALE_BLUES', 'SCALE_CHROMATIC', 'CHORD_MAJ', 'CHORD_MIN', 'CHORD_DIM', 'CHORD_AUG', 'CHORD_MAJ7', 'CHORD_MIN7', 'CHORD_DOM7', 'CHORD_SUS4', 'CHORD_POWER'] },
  { title: 'utility', keys: ['rnd', 'rnd_between', 'rnd_float', 'rnd_float_between', 'now', 'dt', 'epoch', 'frame', 'fps', 'sgn', 'mid', 'timer', 'timer_reset'] },
  { title: 'persistence', keys: ['save', 'load', 'save_int', 'load_int', 'save_bytes', 'load_bytes', 'de_state', 'STATE', 'S', 'GameState'] },
  { title: 'debug',     keys: ['printh', 'watch', 'watch_visible', 'paused'] },
  { title: 'screen',       keys: ['SCREEN_W', 'SCREEN_H'] },
  { title: 'palette',        keys: ['CLR_BLACK', 'CLR_DARK_BLUE', 'CLR_DARK_PURPLE', 'CLR_DARK_GREEN', 'CLR_BROWN', 'CLR_DARK_GREY', 'CLR_LIGHT_GREY', 'CLR_WHITE', 'CLR_RED', 'CLR_ORANGE', 'CLR_YELLOW', 'CLR_GREEN', 'CLR_BLUE', 'CLR_INDIGO', 'CLR_PINK', 'CLR_LIGHT_PEACH', 'CLR_BROWNISH_BLACK', 'CLR_DARKER_BLUE', 'CLR_DARKER_PURPLE', 'CLR_BLUE_GREEN', 'CLR_DARK_BROWN', 'CLR_DARKER_GREY', 'CLR_MEDIUM_GREY', 'CLR_LIGHT_YELLOW', 'CLR_DARK_RED', 'CLR_DARK_ORANGE', 'CLR_LIME_GREEN', 'CLR_MEDIUM_GREEN', 'CLR_TRUE_BLUE', 'CLR_MAUVE', 'CLR_DARK_PEACH', 'CLR_PEACH'] },
  { title: 'experimental — testing only', keys: ['palette_hex'] },
]

const introHTML = `
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
    Coming eventually: an iPad runtime and a sound tracker to match the sprite editor.
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
`

// The Docs tab is a two-pane wiki: a sidebar (API reference + the docs/ tree) and a
// content pane that renders either the API reference or a markdown doc from docs/.
const helpPanel = document.getElementById('panel-help')
let docsSidebar          // left-hand nav
let docsContent          // right-hand content pane
let currentDocPath = ''  // relative path of the markdown doc shown ('' = API reference)
let docsSidebarCollapsed = localStorage.getItem('docsSidebar') === 'closed'

// — find-in-docs (Ctrl/Cmd+F) — highlights matches in whatever the content pane shows —
let findBar, findInput, findCount   // the find UI (built in buildDocsTab)
let findMatches = []                // the <mark> elements for the current query
let findIndex = -1                  // which match is the "current" one
let findQuery = ''

function clearFind() {
  if (!docsContent) return
  docsContent.querySelectorAll('mark.docs-find-hit').forEach(m => {
    m.replaceWith(document.createTextNode(m.textContent))
  })
  docsContent.normalize()
  findMatches = []
  findIndex = -1
}

function updateFindCount() {
  if (!findCount) return
  if (!findQuery) { findCount.textContent = '' }
  else findCount.textContent = findMatches.length ? `${findIndex + 1}/${findMatches.length}` : 'no results'
}

function highlightCurrent(scroll) {
  findMatches.forEach((m, i) => m.classList.toggle('current', i === findIndex))
  if (scroll && findIndex >= 0) findMatches[findIndex].scrollIntoView({ behavior: 'instant', block: 'center' })
  updateFindCount()
}

function runFind(query) {
  clearFind()
  findQuery = query
  if (!query) { updateFindCount(); return }
  // the engine-source view is a CodeMirror instance — injecting <mark>s would
  // corrupt its managed DOM, so find-in-docs sits this one out
  if (currentDocPath.startsWith('engine:')) {
    if (findCount) findCount.textContent = 'n/a in source view'
    return
  }
  const q = query.toLowerCase()
  const walker = document.createTreeWalker(docsContent, NodeFilter.SHOW_TEXT, {
    acceptNode: node => node.nodeValue.trim() ? NodeFilter.FILTER_ACCEPT : NodeFilter.FILTER_REJECT,
  })
  const nodes = []
  for (let n = walker.nextNode(); n; n = walker.nextNode()) nodes.push(n)

  nodes.forEach(node => {
    const text = node.nodeValue
    const lower = text.toLowerCase()
    if (lower.indexOf(q) === -1) return
    const frag = document.createDocumentFragment()
    let last = 0, idx = lower.indexOf(q)
    while (idx !== -1) {
      if (idx > last) frag.appendChild(document.createTextNode(text.slice(last, idx)))
      const mark = document.createElement('mark')
      mark.className = 'docs-find-hit'
      mark.textContent = text.slice(idx, idx + q.length)
      frag.appendChild(mark)
      last = idx + q.length
      idx = lower.indexOf(q, last)
    }
    if (last < text.length) frag.appendChild(document.createTextNode(text.slice(last)))
    node.parentNode.replaceChild(frag, node)
  })

  findMatches = Array.from(docsContent.querySelectorAll('mark.docs-find-hit'))
  findIndex = findMatches.length ? 0 : -1
  highlightCurrent(true)
}

function gotoFind(delta) {
  if (!findMatches.length) return
  findIndex = (findIndex + delta + findMatches.length) % findMatches.length
  highlightCurrent(true)
}

function openFind() {
  if (!findBar) return
  findBar.classList.add('open')
  findInput.focus()
  findInput.select()
  if (findInput.value) runFind(findInput.value)
}

function closeFind() {
  if (!findBar) return
  findBar.classList.remove('open')
  clearFind()
  findQuery = ''
}

// re-run the active query after the content pane is re-rendered (navigation, lang switch)
function reapplyFind() {
  if (findBar?.classList.contains('open') && findQuery) runFind(findQuery)
}

function setActiveNav(key) {
  if (!docsSidebar) return
  docsSidebar.querySelectorAll('[data-docnav]').forEach(el =>
    el.classList.toggle('active', el.dataset.docnav === key))
}

// — the studioDocs-driven API reference (the function/constant list) —
function renderApiReference() {
  currentDocPath = ''
  setActiveNav('api')
  docsContent.innerHTML = ''

  const intro = document.createElement('div')
  intro.className = 'help-intro'
  intro.innerHTML = introHTML
  docsContent.appendChild(intro)

  // section-title → DOM id, so the table of contents can scroll to each section
  const sectionId = title => 'help-section-' + title.replace(/[^a-z0-9]+/gi, '-')

  // table of contents — a clickable list of every section, sits above c basics
  const toc = document.createElement('nav')
  toc.className = 'help-toc'
  toc.innerHTML = `<div class="help-toc-title">contents</div>`
  const tocList = document.createElement('div')
  tocList.className = 'help-toc-list'
  sections.forEach(({ title }) => {
    const link = document.createElement('a')
    link.className = 'help-toc-link'
    link.href = '#'
    link.textContent = title
    link.addEventListener('click', e => {
      e.preventDefault()
      document.getElementById(sectionId(title))?.scrollIntoView({ behavior: 'instant', block: 'start' })
    })
    tocList.appendChild(link)
  })
  toc.appendChild(tocList)
  docsContent.appendChild(toc)

  sections.forEach(({ title, keys }) => {
    const section = document.createElement('div')
    section.className = 'help-section'
    section.id = sectionId(title)
    section.innerHTML = `<div class="help-section-title">${title}</div>`
    keys.forEach(key => {
      const entry = studioDocs[key]
      if (!entry) return
      const text = entry.doc
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
    docsContent.appendChild(section)
  })

  reapplyFind()
}

// — render an engine source file (runtime/*.h|c) into the content pane —
// read-only CodeMirror viewer from navigate.js; cmd-click inside it chains
// to other headers and jumps documented symbols to the API reference
async function showEngineSource(file) {
  currentDocPath = 'engine:' + file
  setActiveNav('engine:' + file)
  docsContent.innerHTML = ''
  const head = document.createElement('div')
  head.className = 'engine-src-head'
  head.textContent = `runtime/${file} — read-only`
  docsContent.appendChild(head)
  const wrap = document.createElement('div')
  wrap.className = 'engine-src'
  docsContent.appendChild(wrap)
  await showEngineFileIn(wrap, file)
  docsContent.scrollTop = 0
}

// cmd-click on an #include "x.h" in the cart (or a chained click inside the
// viewer) lands here — switch to the docs tab and show the file
window.addEventListener('engine-source', (e) => {
  const helpTab = document.querySelector('.tab[data-tab="help"]')
  if (helpTab && !helpTab.classList.contains('active')) helpTab.click()
  showEngineSource(e.detail.file)
})

// — render a markdown doc from docs/ into the content pane —
async function showDoc(relPath) {
  currentDocPath = relPath
  setActiveNav(relPath)
  let md
  try {
    md = await (await fetch('/docs/' + relPath)).text()
  } catch {
    docsContent.innerHTML = `<p class="docs-empty">couldn’t load ${relPath}</p>`
    return
  }
  docsContent.innerHTML = `<div class="docs-md">${marked.parse(md)}</div>`
  docsContent.scrollTop = 0
  reapplyFind()
}

// resolve a relative markdown link against the doc it appears in (doc-root-relative)
function resolveDocPath(from, href) {
  const hrefPath = href.split('#')[0]
  if (!hrefPath) return null
  const baseDir = from.includes('/') ? from.slice(0, from.lastIndexOf('/')) : ''
  const parts = baseDir ? baseDir.split('/') : []
  for (const seg of hrefPath.split('/')) {
    if (seg === '..') parts.pop()
    else if (seg !== '.' && seg !== '') parts.push(seg)
  }
  return parts.join('/')
}

const prettyDocLabel = p => p.split('/').pop().replace(/\.md$/i, '').replace(/[-_]/g, ' ')

function docNavItem(label, key, onClick) {
  const el = document.createElement('button')
  el.className = 'docs-nav-item'
  el.textContent = label
  el.dataset.docnav = key
  el.addEventListener('click', onClick)
  return el
}

async function buildDocsSidebar() {
  docsSidebar.innerHTML = ''
  docsSidebar.appendChild(docNavItem('API reference', 'api', () => renderApiReference()))

  // the engine's own C files, readable right here (cmd-click an #include in
  // your cart jumps to the same view) — studio.h first, then the cart-land
  // library headers, then internals
  const engGrp = document.createElement('div')
  engGrp.className = 'docs-nav-group'
  engGrp.innerHTML = `<div class="docs-nav-head">engine source</div>`
  ENGINE_SOURCES.forEach(f =>
    engGrp.appendChild(docNavItem(f, 'engine:' + f, () => showEngineSource(f))))
  docsSidebar.appendChild(engGrp)

  let files = []
  try { files = await (await fetch('/docs-list.json')).json() } catch {}

  // top-level docs first (curated order), then one group per folder
  const ORDER = ['README.md', 'VISION.md', 'STATUS.md', 'POLISH_TODO.md', 'HANDOFF.md']
  const rank = f => (ORDER.indexOf(f) + 1) || 99
  const top = files.filter(f => !f.includes('/')).sort((a, b) => rank(a) - rank(b) || a.localeCompare(b))
  const folders = {}
  files.filter(f => f.includes('/')).forEach(f => {
    const dir = f.slice(0, f.indexOf('/'))
    ;(folders[dir] ||= []).push(f)
  })

  const addGroup = (head, list) => {
    const grp = document.createElement('div')
    grp.className = 'docs-nav-group'
    grp.innerHTML = `<div class="docs-nav-head">${head}</div>`
    list.forEach(f => grp.appendChild(docNavItem(prettyDocLabel(f), f, () => showDoc(f))))
    docsSidebar.appendChild(grp)
  }
  if (top.length) addGroup('guide', top)
  Object.keys(folders).sort().forEach(dir => addGroup(dir, folders[dir].sort()))
}

async function buildDocsTab() {
  helpPanel.classList.add('docs-tab')
  helpPanel.innerHTML = ''
  docsSidebar = document.createElement('div'); docsSidebar.id = 'docs-sidebar'
  docsContent = document.createElement('div'); docsContent.id = 'docs-content'
  helpPanel.appendChild(docsSidebar)
  helpPanel.appendChild(docsContent)

  // collapse / expand the sidebar (button stays visible in both states; state persists)
  const toggleBtn = document.createElement('button')
  toggleBtn.id = 'docs-toggle'
  toggleBtn.title = 'show / hide sidebar'
  const syncToggle = () => {
    helpPanel.classList.toggle('sidebar-collapsed', docsSidebarCollapsed)
    toggleBtn.textContent = docsSidebarCollapsed ? '☰' : '⟨'
  }
  toggleBtn.addEventListener('click', () => {
    docsSidebarCollapsed = !docsSidebarCollapsed
    localStorage.setItem('docsSidebar', docsSidebarCollapsed ? 'closed' : 'open')
    syncToggle()
  })
  helpPanel.appendChild(toggleBtn)
  syncToggle()

  // find-in-docs bar (Ctrl/Cmd+F) — floats over the content pane, searches whatever it shows
  findBar = document.createElement('div')
  findBar.id = 'docs-find'
  findBar.innerHTML = `
    <input type="text" id="docs-find-input" placeholder="find in docs…" spellcheck="false" />
    <span id="docs-find-count"></span>
    <button id="docs-find-prev" title="previous (Shift+Enter)">↑</button>
    <button id="docs-find-next" title="next (Enter)">↓</button>
    <button id="docs-find-close" title="close (Esc)">✕</button>
  `
  helpPanel.appendChild(findBar)
  findInput = findBar.querySelector('#docs-find-input')
  findCount = findBar.querySelector('#docs-find-count')
  findInput.addEventListener('input', () => runFind(findInput.value))
  findInput.addEventListener('keydown', e => {
    if (e.key === 'Enter') { e.preventDefault(); gotoFind(e.shiftKey ? -1 : 1) }
    else if (e.key === 'Escape') { e.preventDefault(); closeFind() }
  })
  findBar.querySelector('#docs-find-prev').addEventListener('click', () => gotoFind(-1))
  findBar.querySelector('#docs-find-next').addEventListener('click', () => gotoFind(1))
  findBar.querySelector('#docs-find-close').addEventListener('click', () => closeFind())

  // relative .md links inside a rendered doc navigate within the pane; external/anchor links pass through
  docsContent.addEventListener('click', e => {
    const a = e.target.closest('a')
    if (!a) return
    const href = a.getAttribute('href') || ''
    if (!href || href.startsWith('#') || /^[a-z]+:/i.test(href)) return
    e.preventDefault()
    const resolved = resolveDocPath(currentDocPath, href)
    if (resolved) showDoc(resolved)
  })

  await buildDocsSidebar()
  renderApiReference()   // default view
}

buildDocsTab()

// jump-to-help from the code editor (cmd/ctrl-click on a documented word)
window.addEventListener('help-jump', (e) => {
  const key = e.detail.key
  const helpTab = document.querySelector('.tab[data-tab="help"]')
  if (helpTab) helpTab.click()
  renderApiReference()   // ensure the API list (not a doc) is the visible pane content
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

// Parse a cart PNG's zTXt chunks in the browser (no Electron needed).
// PNG zTXt uses zlib (RFC 1950) compression — DecompressionStream('deflate') handles it.
async function extractCartChunksBrowser(bytes) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength)
  const latin1 = new TextDecoder('latin1')
  const utf8   = new TextDecoder('utf-8')
  const result = {}
  let offset = 8
  while (offset + 12 <= bytes.length) {
    const len  = view.getUint32(offset, false)
    const type = latin1.decode(bytes.subarray(offset + 4, offset + 8))
    const data = bytes.subarray(offset + 8, offset + 8 + len)
    if (type === 'zTXt') {
      const nullIdx = data.indexOf(0)
      if (nullIdx !== -1) {
        const key = latin1.decode(data.subarray(0, nullIdx))
        if (key.startsWith('de:')) {
          try {
            const compressed = data.subarray(nullIdx + 2)
            const ds = new DecompressionStream('deflate')
            const writer = ds.writable.getWriter()
            const reader = ds.readable.getReader()
            writer.write(compressed); writer.close()
            const chunks = []
            for (;;) { const { done, value } = await reader.read(); if (done) break; chunks.push(value) }
            const out = new Uint8Array(chunks.reduce((s, c) => s + c.length, 0))
            let pos = 0; for (const c of chunks) { out.set(c, pos); pos += c.length }
            result[key.slice(3)] = utf8.decode(out)
          } catch {}
        }
      }
    }
    offset += 12 + len
    if (type === 'IEND') break
  }
  return result
}

async function loadCartFromUrl(url) {
  const res = await fetch(url)
  const bytes = new Uint8Array(await res.arrayBuffer())
  let cart
  if (window.studio) {
    cart = await window.studio.loadCartBuffer(bytes)
  } else {
    const chunks = await extractCartChunksBrowser(bytes)
    if (!chunks.source) return
    let settings = null
    try { settings = chunks.settings ? JSON.parse(chunks.settings) : null } catch {}
    cart = { ok: true, source: chunks.source, spritesDataUrl: chunks.sprites || null, mapBase64: chunks.map || null, settings }
  }
  if (cart && cart.ok) { applyCart(cart); switchTab('code') }
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

// faceted cart filters (see editor/public/carts/index.json). order is curated,
// not alphabetical. state is module-level so it survives the panel rebuild-on-open.
const CART_KIND_ORDER  = ['tutorial', 'game', 'tech-demo', 'instrument', 'toy', 'tool', 'probe']
const CART_GENRE_ORDER = ['arcade', 'shooter', 'platformer', 'fighting', 'puzzle', 'racing',
                          'sports', 'strategy', 'rpg', 'adventure', 'simulation', 'sandbox', 'tabletop']
let cartFilter = null   // null = all; else { axis: 'kind'|'genre', value } — flat single-select

async function buildTutorialsPanel() {
  const body = tutorialsPanel.querySelector('#tutorials-body')
  if (!body) return
  // preserve scroll + search term across reopen/rebuild
  const prevScroll = tutorialsPanel.scrollTop
  const prevSearch = body.querySelector('#tutorials-search')?.value || ''
  body.innerHTML = ''

  let index
  try {
    const res = await fetch('/carts/index.json')
    index = await res.json()
  } catch {
    body.innerHTML = '<p class="tutorials-empty">No carts found.</p>'
    return
  }

  const search = document.createElement('input')
  search.id = 'tutorials-search'
  search.className = 'tutorials-search'
  search.type = 'search'
  search.placeholder = 'search carts…'
  search.autocomplete = 'off'
  body.appendChild(search)

  // ── filter chips: one flat list (kinds + genres), single-select ──
  const kindCounts = {}, genreCounts = {}
  index.forEach(e => {
    (e.kind || []).forEach(k => { kindCounts[k] = (kindCounts[k] || 0) + 1 })
    if (e.genre) genreCounts[e.genre] = (genreCounts[e.genre] || 0) + 1
  })

  const filters = document.createElement('div')
  filters.className = 'tutorials-filters'
  const chipRow = document.createElement('div')
  chipRow.className = 'cart-chip-row'
  filters.appendChild(chipRow)
  body.appendChild(filters)

  const chipOn = (axis, value) => cartFilter && cartFilter.axis === axis && cartFilter.value === value
  function makeChip(label, value, count, axis) {
    const b = document.createElement('button')
    b.className = 'cart-chip'
    b.dataset.axis = axis || ''
    b.dataset.value = value == null ? '' : value
    b.innerHTML = `${escapeHtml(label)}${count != null ? ` <span class="cart-chip-n">${count}</span>` : ''}`
    b.addEventListener('click', () => {
      cartFilter = (value == null || chipOn(axis, value)) ? null : { axis, value }   // toggle / clear
      syncChips(); applyFilter()
    })
    chipRow.appendChild(b)
  }

  // kinds first, then genres — same flat row, no visual divide
  makeChip('all', null, index.length, null)
  CART_KIND_ORDER.forEach(k => { if (kindCounts[k]) makeChip(k, k, kindCounts[k], 'kind') })
  CART_GENRE_ORDER.forEach(g => { if (genreCounts[g]) makeChip(g, g, genreCounts[g], 'genre') })

  function syncChips() {
    chipRow.querySelectorAll('.cart-chip').forEach(b => {
      const axis = b.dataset.axis || null, value = b.dataset.value || null
      b.classList.toggle('active', value == null ? cartFilter == null : chipOn(axis, value))
    })
  }

  function passesChips(it) {
    if (!cartFilter) return true
    return cartFilter.axis === 'kind' ? it.kind.includes(cartFilter.value)
                                      : it.genre === cartFilter.value
  }

  const grid = document.createElement('div')
  grid.className = 'tutorials-grid'
  body.appendChild(grid)

  const noMatch = document.createElement('p')
  noMatch.className = 'tutorials-empty'
  noMatch.textContent = 'no carts match'
  noMatch.style.display = 'none'
  body.appendChild(noMatch)

  const items = index.map(({ title, description, file, kind, genre }, idx) => {
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

    card.addEventListener('click', () => { setCartName(title); currentCartFile = String(file).replace(/\.cart\.png$/i, ''); loadCartFromUrl(url) })

    grid.appendChild(card)
    return { card, titleEl, descEl, idx, title: title || '', desc: description || '',
             name: String(file).replace(/\.cart\.png$/i, ''), kind: kind || [], genre: genre || null }
  })

  function applyFilter() {
    const q = search.value.trim()
    const pool = items.filter(passesChips)
    items.forEach(it => { it.card.style.display = 'none' })   // hide all; the branches re-show the pool
    if (!q) {                                    // no text → show chip-passing in original order, plain text
      pool.forEach(it => {
        it.card.style.display = ''
        it.titleEl.textContent = it.title
        it.descEl.textContent = it.desc
        grid.appendChild(it.card)
      })
      noMatch.style.display = pool.length ? 'none' : ''
      return
    }
    // title matches outrank filename, which outrank description blurb
    const scored = pool.map(it => ({
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

  // restore prior search term + chip state + scroll position so reopening feels seamless
  if (prevSearch) search.value = prevSearch
  syncChips()
  applyFilter()
  tutorialsPanel.scrollTop = prevScroll
}
// (no eager build — switchTab('tutorials') rebuilds the panel each time it's opened)

// ── day/night theme (toggle lives in the Settings panel) ──────
let themeMode = localStorage.getItem('theme') || 'night'
function applyTheme(mode) {
  themeMode = mode
  document.documentElement.classList.toggle('day', mode === 'day')
  localStorage.setItem('theme', mode)
  setEditorTheme(mode)
  const btn = document.getElementById('theme-btn')
  if (btn) {
    btn.textContent = mode === 'day' ? '☀ day' : '☾ night'
    btn.title = mode === 'day' ? 'switch to night' : 'switch to day'
  }
}

// appended after every settings-panel (re)build so it survives rebuilds
function mountSettingsExtras(panel) {
  if (!panel) return
  const section = document.createElement('div')
  section.className = 'settings-section'
  section.innerHTML = '<div class="help-section-title">appearance</div>'
  const row = document.createElement('div')
  row.className = 'settings-row'
  const label = document.createElement('span')
  label.className = 'settings-field-label'
  label.textContent = 'theme'
  const btn = document.createElement('button')
  btn.id = 'theme-btn'
  btn.addEventListener('click', () => applyTheme(themeMode === 'day' ? 'night' : 'day'))
  row.appendChild(label)
  row.appendChild(btn)
  section.appendChild(row)
  panel.appendChild(section)
}

// ── settings panel ───────────────────────────────────────────
function rebuildSettings() {
  const el = document.getElementById('panel-settings')
  buildSettingsPanel(el)
  mountSettingsExtras(el)
  applyTheme(themeMode)   // sync the freshly-created toggle's label
}
rebuildSettings()

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
  liveHostRunning = !!(result && result.live)   // a live window is now open → enable auto-reload

  runBtn.textContent = '▶ run'
  runBtn.disabled = false

  showLog(result)
})

// ── live auto-reload ──────────────────────────────────────────
// While a live (libtcc) window is open, rewrite cart.c on a debounce as the user types;
// the host's file-watch hot-reloads it. No-op otherwise, so it costs nothing in native
// mode or before the first ▶ run.
let liveHostRunning = false
let liveWriteTimer = null
onDocChange(() => {
  if (settings.backend !== 'live' || !liveHostRunning) return
  if (typeof window.studio?.liveWrite !== 'function') return   // stale preload → needs npm start
  clearTimeout(liveWriteTimer)
  liveWriteTimer = setTimeout(() => window.studio.liveWrite(view.state.doc.toString()), 350)
})

// ── profile button (hidden unless enabled in settings) ─────────
const profileBtn = document.getElementById('profile-btn')
if (profileBtn) {
  if (settings.showProfiler) profileBtn.style.display = ''

  profileBtn.addEventListener('click', async () => {
    if (!window.studio) {
      showLog({ ok: false, profile: true, output: 'profiling requires the desktop app  (npm start)' })
      return
    }
    // profile() is added in preload.cjs — which only loads on a full Electron
    // restart, not a Vite hot-reload. A stale preload would throw and wedge the
    // button, so fail loud instead.
    if (typeof window.studio.profile !== 'function') {
      showLog({ ok: false, profile: true, output: 'restart the app (Ctrl-C, then `npm start`) to enable profiling' })
      return
    }

    const code = view.state.doc.toString()
    setErrorLines([])
    profileBtn.textContent = '⏱ profiling…'
    profileBtn.disabled = true
    runBtn.disabled = true
    buildLog.style.display = 'none'
    rlogClear()

    try {
      const tilemapCanvas = document.querySelector('#tilemap-canvas')
      if (tilemapCanvas) await window.studio.saveSprites(tilemapCanvas.toDataURL('image/png'))
      await window.studio.saveMap(getMapBytes())

      const result = await window.studio.profile(code, { ...settings, cartName: currentCartName })
      showLog(result)
    } catch (e) {
      showLog({ ok: false, profile: true, output: 'profiling failed: ' + (e?.message || e) })
    } finally {
      profileBtn.textContent = '⏱ profile'
      profileBtn.disabled = false
      runBtn.disabled = false
    }
  })
}

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
  const saved = await window.studio.saveCart({ source: view.state.doc.toString(), spritesDataUrl, mapBase64, settings: cartSettings })
  // adopt the saved filename as the cart name (shown in the window title)
  if (saved && saved.ok && saved.filePath) {
    const base = saved.filePath.split('/').pop().replace(/\.cart\.png$/, '').replace(/\.png$/, '')
    if (base) { setCartName(base); currentCartFile = base }
  }
})

// Cmd/Ctrl+S → save cart, from anywhere (capture phase, so CodeMirror doesn't eat it)
window.addEventListener('keydown', e => {
  if ((e.metaKey || e.ctrlKey) && (e.key === 's' || e.key === 'S')) {
    e.preventDefault()
    saveCartBtn.click()
  }
  // Cmd/Ctrl+F → find-in-docs, but only while the Docs tab is showing
  if ((e.metaKey || e.ctrlKey) && (e.key === 'f' || e.key === 'F') && helpPanel.classList.contains('active')) {
    e.preventDefault()
    openFind()
  }
}, true)

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
  rebuildSettings()
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
  refreshOutline()   // doc was replaced wholesale — re-list immediately, no debounce wait
  showToast('cart loaded')
}

loadCartBtn.addEventListener('click', async () => {
  if (!window.studio) return
  const cart = await window.studio.loadCart()
  if (cart && cart.ok) { if (cart.name) { setCartName(cart.name); currentCartFile = cart.name } applyCart(cart) }
})

// ── build for web ─────────────────────────────────────────────
const buildWebBtn = document.getElementById('build-web-btn')

buildWebBtn.addEventListener('click', async () => {
  if (!window.studio) return
  const tilemapCanvas = document.querySelector('#tilemap-canvas')
  if (tilemapCanvas) await window.studio.saveSprites(tilemapCanvas.toDataURL('image/png'))
  await window.studio.saveMap(getMapBytes())

  const stopDots = busyDots(buildWebBtn, 'building', 'build for web')
  buildWebBtn.disabled = true
  rlogClear()   // open the runtime log panel for step-by-step output

  const code = view.state.doc.toString()
  const result = await window.studio.buildWeb(code, settings)

  stopDots()
  buildWebBtn.disabled = false

  if (result.ok) {
    showToast(`opening ${result.url}`)
  } else {
    showLog(result)
  }
})

// busy-dots: "⌛ label", "⌛ label.", "⌛ label.." … while a slow build runs.
// returns a stop() that restores the button's resting text.
function busyDots(btn, label, resting) {
  let n = 0
  btn.textContent = `⌛ ${label}`
  const tick = setInterval(() => {
    n = (n + 1) % 4
    btn.textContent = `⌛ ${label}${'.'.repeat(n)}`
  }, 350)
  return () => { clearInterval(tick); btn.textContent = resting }
}

// ── publish to site (gated by settings → publish toggle) ─────
const publishBtn = document.getElementById('publish-btn')
if (settings.showPublish) publishBtn.style.display = ''

publishBtn.addEventListener('click', async () => {
  if (!window.studio) return
  // a Vite hot-reload shows this button before Electron has the new backend —
  // preload/main only update on a full restart
  if (!window.studio.publish) { showToast('restart the editor first (run `make`) — publish backend not loaded'); return }
  const slug = (currentCartFile || currentCartName || '').toLowerCase().replace(/\.cart\.png$/, '').replace(/[^a-z0-9_-]+/g, '')
  if (!slug) { showToast('the cart needs a name — load one, or "save cart" first'); return }
  if (!confirm(`Publish "${slug}" to the PUBLIC site?\n\nCommits to github.com/NikkiKoole/dreamengine (site/ + tools/carts/${slug}.c) and pushes to master.\n\nLive at: nikkikoole.github.io/dreamengine/${slug}/`)) return

  const stopDots = busyDots(publishBtn, 'publishing', '🚀 publish to site')
  publishBtn.disabled = true
  try {
    const tilemapCanvas = document.querySelector('#tilemap-canvas')
    if (tilemapCanvas) await window.studio.saveSprites(tilemapCanvas.toDataURL('image/png'))
    await window.studio.saveMap(getMapBytes())
    rlogClear()

    const code = view.state.doc.toString()
    const result = await window.studio.publish(code, { ...settings, cartName: slug })

    if (result.ok) showToast(`live in ~1 min: ${result.url}`)
    else showLog(result)
  } catch (e) {
    showToast(`publish failed: ${e.message}`)
  } finally {
    stopDots()
    publishBtn.disabled = false
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
  if (cart && cart.ok) { if (cart.name) { setCartName(cart.name); currentCartFile = cart.name } applyCart(cart) }
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

  if (result.profile) { renderProfile(result); return }

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

// render profiler results into the build log:
//   C — frame budget (ms/frame vs the 60fps budget)
//   A — hottest functions + the call paths that reach them (from sampling)
//   D — draw calls per frame (exact, in-engine counts)
function renderProfile(result) {
  if (!result.ok) {
    const err = document.createElement('div')
    err.className = 'build-error'
    err.textContent = result.output || 'profiling failed'
    buildLog.appendChild(err)
    return
  }

  const addLine = (text, cls = 'build-meta') => {
    const el = document.createElement('div')
    el.className = cls
    el.textContent = text
    buildLog.appendChild(el)
  }

  const { hotspots, perf, seconds } = result

  const head = document.createElement('div')
  head.className = 'build-ok'
  head.textContent = `⏱ profiled ${seconds}s`
  buildLog.appendChild(head)

  // ── C — frame budget ───────────────────────────────────────────
  if (perf) {
    const fps = Math.round(perf.fps)
    const verdict = fps >= 58 ? 'smooth 60fps' : fps >= 50 ? `~${fps}fps` : `dropping to ~${fps}fps`
    addLine(`  CPU ${perf.workMedian.toFixed(1)}ms/frame typical · ${perf.workP95.toFixed(1)}ms p95`
          + ` · ${Math.round(perf.budgetPct)}% of the 16.6ms budget · ${verdict}`,
            fps >= 58 ? 'build-ok' : 'build-warn')
  }

  // ── A — hottest functions + call paths ─────────────────────────
  const { total, cartSamples, leaves } = hotspots
  const wallPct = total ? Math.round((cartSamples / total) * 100) : 0
  addLine('')
  addLine(`hottest functions in your update()/draw()  (${cartSamples}/${total} samples, ~${wallPct}% of wall; rest = vsync/system)`)
  if (cartSamples > 0 && cartSamples < 80)
    addLine('  ⚠ low sample count — cart is mostly idle, so this ranking is rough', 'build-warn')
  if (!leaves || !leaves.length) {
    addLine('  (nothing hot — the cart was idle / waiting on vsync the whole time)')
  } else {
    leaves.slice(0, 8).forEach(leaf => {
      addLine(`  ${leaf.pct.toFixed(1).padStart(5)}%  ${String(leaf.samples).padStart(4)}  ${leaf.symbol}`)
      leaf.paths.forEach(p =>
        addLine(`            ${String(p.samples).padStart(4)}  ← ${p.via}`, 'build-meta build-profile-path'))
    })
  }

  // ── D — draw calls per frame ───────────────────────────────────
  if (perf && perf.calls && perf.calls.length) {
    addLine('')
    addLine('draw calls per frame  (your direct calls; nested ones counted once)')
    perf.calls.slice(0, 12).forEach(c => {
      const n = c.perFrame >= 10 ? String(Math.round(c.perFrame)) : c.perFrame.toFixed(1)
      addLine(`  ${n.padStart(7)}  ${c.name}`)
    })
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
  // linkify https URLs — click opens the system browser (not the editor window)
  const re = /https?:\/\/[^\s)]+/g
  let last = 0, m
  while ((m = re.exec(text)) !== null) {
    const url = m[0]
    line.appendChild(document.createTextNode(text.slice(last, m.index)))
    const a = document.createElement('a')
    a.href = url
    a.textContent = url
    a.addEventListener('click', e => {
      e.preventDefault()
      if (window.studio?.openExternal) window.studio.openExternal(url)
      else window.open(url, '_blank')
    })
    line.appendChild(a)
    last = m.index + url.length
  }
  line.appendChild(document.createTextNode(text.slice(last)))
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

// Live (libtcc) diagnostics stream through the log (not a run-result), as
// `[tcc] …/cart.c:LINE: error: …`. Mirror the native path: mark the offending line(s)
// in the editor, and clear them when a recompile succeeds. cart.c is the editor code
// verbatim, so its line numbers match the editor's.
function scanLiveDiagnostics(text) {
  const errs = []
  for (const m of text.matchAll(/\[tcc\][^\n]*cart\.c:(\d+):[^\n]*error/g)) errs.push(parseInt(m[1]))
  if (errs.length) setErrorLines([...new Set(errs)])
  else if (/\[tcc\] (compiled|hot-reloaded)/.test(text)) setErrorLines([])   // clean recompile → clear
}

if (window.studio?.onLog) {
  window.studio.onLog(rlogAppend)
  window.studio.onLog(scanLiveDiagnostics)
  window.studio.onExit(rlogExit)
  window.studio.onExit(() => { liveHostRunning = false })   // live window closed → stop auto-reload
}

// ── welcome cart ──────────────────────────────────────────────
// On startup, load the configured welcome cart (settings tab → startup).
// Works in both Electron and the browser — loadCartFromUrl has a browser-side
// fallback for parsing carts; only ▶ run requires Electron.
const EMPTY_TEMPLATE =
`// dreamengine — write c, make things!

#include "studio.h"

// S holds your game's state — and it survives a live reload, so you can
// edit the code while it runs and S keeps its values. Add fields here,
// then use them as S->name.
#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))
STATE {
    int x;
};

void update() {
    S->x = (S->x + 1) % SCREEN_W;   // slide across, wrap around
}

void draw() {
    cls(CLR_DARK_BLUE);
    print("hello!", 10, 10, CLR_PEACH);
    rectfill(S->x, 40, 8, 8, CLR_PEACH);
}
`

window.addEventListener('load', () => {
  if (settings.welcomeCart === 'empty') {
    view.dispatch({ changes: { from: 0, to: view.state.doc.length, insert: EMPTY_TEMPLATE } })
  } else {
    setCartName('pixel zoo')
    currentCartFile = 'zoo'
    loadCartFromUrl('/carts/zoo.cart.png').catch(() => {})
  }
})

