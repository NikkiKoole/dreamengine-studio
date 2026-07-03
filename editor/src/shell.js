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
let currentCartPath = ''   // absolute on-disk ORIGIN path, only for carts loaded
                           // from a real file (dialog / drag-drop). '' for gallery
                           // carts + fresh carts → Cmd-S falls through to Save As.
                           // Set this ONLY from a load/save handler's `origin`.
let currentCartThumb = ''  // the loaded cart's .cart.png URL (gallery loads) — the share header thumbnail
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
  // leaving the current view dismisses any open build/profile report
  clearTimeout(hideTimer)
  buildLog.style.display = 'none'
  captureHistoryScroll()   // grab the history iframe's scroll WHILE it's still visible
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
  // returning to the docs tab after a cart load: the panel was display:none'd,
  // which resets the still-mounted history iframe's scroll to 0 without reloading
  // it — so re-restore from the position the page saved in sessionStorage.
  if (name === 'help') restoreHistoryScroll()
  if (name === 'tutorials') {
    // rebuild on every open so a freshly-added cart shows up without a reload
    buildTutorialsPanel().then(() => {
      const s = document.getElementById('tutorials-search')
      if (s) s.focus()   // panel is visible by now, so focus takes
    })
  }
}

// Scroll-position memory for the history iframe across tab switches. Switching
// tabs display:none's the Docs panel, which zeroes the (still-mounted) iframe's
// scroll — so we snapshot scrollY while it's visible and re-apply it on return.
// A plain variable + direct same-origin scrollTo: no sessionStorage (Chromium
// can partition the parent's separately from the iframe's) and no message timing.
let lastHistoryScroll = 0
function historyFrame() {
  const helpActive = document.getElementById('panel-help')?.classList.contains('active')
  const frame = typeof docsContent !== 'undefined' && docsContent && docsContent.querySelector('.history-frame')
  return { frame, helpActive }
}
function captureHistoryScroll() {
  const { frame, helpActive } = historyFrame()
  if (!frame || !helpActive) return        // only capture while it's actually on screen
  // capture even 0 — the panel being visible means this scrollY is real (e.g. you
  // just hit back-to-top); the helpActive guard already excludes the bogus 0 a
  // hidden (display:none) panel reports.
  try { const y = frame.contentWindow.scrollY; if (Number.isFinite(y)) lastHistoryScroll = y } catch {}
}
function restoreHistoryScroll() {
  const { frame } = historyFrame()
  if (!frame || !lastHistoryScroll) return
  const y = lastHistoryScroll
  let n = 0
  const tick = () => {                      // retry ~400ms in case layout settles late
    try { frame.contentWindow.scrollTo(0, y) } catch {}
    if (++n < 8) setTimeout(tick, 50)
  }
  requestAnimationFrame(tick)
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
  { title: 'graphics',      keys: ['cls', 'colorkey', 'spr', 'sprf', 'sspr', 'spr_rot', 'sspr_ex', 'pget', 'pget_rgb', 'enable_pget', 'pset', 'pset_rgb', 'sget', 'font', 'FONT_NORMAL', 'FONT_SMALL', 'FONT_TINY', 'FONT_COMIC', 'FONT_THIN', 'print', 'print_centered', 'print_right', 'print_scaled', 'print_rot', 'print_rot_scaled', 'print_outline', 'text_width', 'rect', 'rectfill', 'rectfill_rgb', 'rectfill_rot', 'bar', 'circ', 'circfill', 'oval', 'ovalfill', 'arc', 'arcfill', 'arcoutline', 'ring', 'ringoutline', 'tri', 'trifill', 'tritex', 'line', 'bezier', 'camera', 'camera_ex', 'smooth_zoom', 'follow', 'clip', 'zoom_rect', 'pal', 'pal_reset', 'fade', 'shake'] },
  { title: 'shapes',       keys: ['ngon', 'ngonfill', 'star', 'starfill', 'poly', 'polyfill', 'thickline', 'thicklineoutline', 'rrect', 'rrectfill', 'gradient', 'vgradient', 'hgradient'] },
  { title: 'input',        keys: ['btn', 'btnp', 'btnr', 'de_players', 'BTN_UP', 'BTN_DOWN', 'BTN_LEFT', 'BTN_RIGHT', 'BTN_A', 'BTN_B', 'BTN_X', 'BTN_Y'] },
  { title: 'touch',        keys: ['stick_x', 'stick_y', 'touch_count', 'touch_x', 'touch_y', 'touch_id', 'tap', 'tapp', 'touch_ended_count', 'touch_ended_id', 'touch_ended_x', 'touch_ended_y', 'tapr', 'touch_controls', 'touch_layout', 'TOUCH_ANALOG', 'TOUCH_ANALOG_FIX', 'TOUCH_DPAD4', 'TOUCH_DPAD8', 'touch_ceiling', 'touch_layout_mode', 'TOUCH_LAYOUT_OVERLAY', 'TOUCH_LAYOUT_DECK', 'TOUCH_LAYOUT_RAILS', 'touch_ctrl_scale', 'touch_debug'] },
  { title: 'mouse',         keys: ['mouse_x', 'mouse_y', 'mouse_world_x', 'mouse_world_y', 'mouse_down', 'mouse_pressed', 'mouse_released', 'mouse_wheel', 'MOUSE_LEFT', 'MOUSE_RIGHT', 'MOUSE_MIDDLE', 'mouse_cursor', 'CURSOR_DEFAULT', 'CURSOR_HAND', 'CURSOR_CROSSHAIR', 'CURSOR_MOVE', 'CURSOR_TEXT', 'CURSOR_NO', 'mouse_hide', 'mouse_show'] },
  { title: 'keyboard',  keys: ['key', 'keyp', 'keyr', 'text_input', 'KEY_SPACE', 'KEY_ENTER', 'KEY_BACKSPACE', 'KEY_ESCAPE', 'KEY_TAB', 'KEY_LEFT', 'KEY_RIGHT', 'KEY_UP', 'KEY_DOWN'] },
  { title: 'midi',         keys: ['midi_get', 'midi_held', 'midi_bend', 'midi_present', 'midi_name'] },
  { title: 'patterns',     keys: ['fillp', 'fillp_reset', 'fillp_anchor', 'FILL_SOLID', 'FILL_CHECKER', 'FILL_DOTS', 'FILL_HLINES', 'FILL_VLINES', 'FILL_DIAG', 'FILL_GRID'] },
  { title: 'map',          keys: ['map', 'map_scale', 'mget', 'mset', 'MAP_W', 'MAP_H'] },
  { title: 'noise',       keys: ['noise', 'noise2', 'noise3'] },
  { title: '3d',           keys: ['V3', 'rot3', 'project3', 'zsort', 'quadfill'] },
  { title: 'easing',      keys: ['ease_in', 'ease_out', 'ease_in_out', 'ease_back'] },
  { title: 'math',     keys: ['abs', 'min', 'max', 'clamp', 'lerp', 'remap', 'distance', 'length', 'fsqrt', 'angle_to', 'dx', 'dy', 'sin_deg', 'cos_deg'] },
  { title: 'collision',    keys: ['boxes_touch', 'point_in_box', 'circles_touch', 'near', 'touching_map', 'tile_at', 'touching_color'] },
  { title: 'animation',     keys: ['anim', 'blink'] },
  { title: 'strings',        keys: ['str'] },
  { title: 'sound',       keys: ['sfx', 'note', 'hit', 'note_on', 'note_off', 'note_pitch', 'note_vol', 'note_cutoff', 'note_duty', 'note_pan', 'note_res', 'note_lfo', 'note_lfo_shape', 'note_env', 'note_follow', 'note_harmonics', 'note_timbre', 'note_morph', 'voice_nasal', 'voice_consonant', 'voice_coda', 'instrument_mode', 'note_aux', 'note_drive', 'note_drive_mode', 'instrument_sync', 'note_sync', 'note_echo', 'note_reverb', 'note_filter', 'note_glide', 'note_off_all', 'instrument', 'instrument_duty', 'instrument_pan', 'pan_law', 'listener', 'listener_vel', 'spatial_model', 'spatial_speed', 'note_pos', 'note_motion', 'hit_at', 'instrument_pos', 'instrument_motion', 'instrument_lfo', 'lfo_shape', 'lfo_value', 'scope_read', 'instrument_filter', 'instrument_env', 'instrument_follow', 'instrument_harmonics', 'instrument_timbre', 'instrument_morph', 'instrument_tune', 'instrument_drive', 'instrument_drive_mode', 'instrument_echo', 'instrument_reverb', 'instrument_reverb_bus', 'reverb_bus_fx', 'reverb_insert', 'echo_insert', 'drive_insert', 'drive_insert_inst', 'grains', 'instrument_grains', 'grains_freeze', 'instrument_grains_freeze', 'grains_pitch', 'instrument_grains_pitch', 'instrument_chorus', 'instrument_flanger', 'instrument_tape', 'instrument_wah', 'instrument_wah_lfo', 'instrument_crush', 'instrument_eq', 'instrument_formant', 'instrument_tremolo', 'instrument_autopan', 'instrument_ringmod', 'instrument_phaser', 'instrument_univibe', 'instrument_leslie', 'instrument_choke', 'echo', 'reverb', 'reverb_bus', 'chorus', 'flanger', 'tape', 'tape_inst', 'wah', 'wah_lfo', 'crush', 'crush_inst', 'eq', 'eq_inst', 'formant', 'filter', 'filter_inst', 'tremolo', 'autopan', 'ringmod', 'phaser', 'univibe', 'leslie', 'dropout', 'shallow', 'instrument_shallow', 'amp_noise', 'gate', 'instrument_gate', 'shimmer', 'instrument_shimmer', 'varispeed', 'fx_mod', 'instrument_fx_mod', 'fx_lfo', 'fx_order', 'sidechain', 'sidechain_key', 'glue', 'wave_set', 'tone', 'chord', 'strum', 'strum_notes', 'schedule', 'schedule_hit', 'bpm', 'beat', 'beat_pos', 'every', 'euclid', 'chance', 'degree', 'INSTR_SQUARE', 'INSTR_SAW', 'INSTR_TRI', 'INSTR_NOISE', 'INSTR_SINE', 'INSTR_USER0', 'INSTR_USER1', 'INSTR_USER2', 'INSTR_USER3', 'INSTR_PLUCK', 'INSTR_MALLET', 'INSTR_FM', 'INSTR_ORGAN', 'INSTR_EPIANO', 'INSTR_PD', 'INSTR_MEMBRANE', 'INSTR_REED', 'INSTR_PIPE', 'INSTR_VOICE', 'INSTR_GUITAR', 'INSTR_PIANO', 'INSTR_BOWED', 'INSTR_BRASS', 'LFO_PITCH', 'LFO_DUTY', 'LFO_VOLUME', 'LFO_CUTOFF', 'LFO_HARMONICS', 'LFO_TIMBRE', 'LFO_MORPH', 'LFO_PAN', 'PAN_LINEAR', 'PAN_POWER', 'ENV_CUTOFF', 'ENV_PITCH', 'ENV_DUTY', 'ENV_HARMONICS', 'ENV_TIMBRE', 'ENV_MORPH', 'FILTER_OFF', 'FILTER_LOW', 'FILTER_HIGH', 'FILTER_BAND', 'FILTER_NOTCH', 'FILTER_LADDER', 'FILTER_STEINER', 'FILTER_STEINER_HP', 'FILTER_STEINER_BP', 'FILTER_STEINER_NF', 'FILTER_DIODE', 'DRIVE_SOFT', 'DRIVE_HARD', 'DRIVE_FOLD', 'DRIVE_ASYM', 'LFO_SHAPE_SINE', 'LFO_SHAPE_SQUARE', 'LFO_SHAPE_TRI', 'LFO_SHAPE_SAW', 'LFO_SHAPE_RAMP', 'LFO_SHAPE_OPTICAL', 'LFO_SHAPE_SH', 'LFO_SHAPE_RANDOM', 'TREM_SINE', 'TREM_SQUARE', 'TREM_TRI', 'MODE_STRING_WEIGHT', 'MODE_STRING_CLICK', 'MODE_BOW_PIZZ', 'FX_TREM', 'FX_WAH', 'FX_CHORUS', 'FX_PHASER', 'FX_FLANGER', 'FX_TAPE', 'FX_EQ', 'FX_CRUSH', 'FX_REVERB', 'FX_FORMANT', 'FX_FILTER', 'FX_PAN', 'FX_RINGMOD', 'FX_ECHO', 'FX_GRAINS', 'FX_DRIVE', 'FX_SHALLOW', 'FX_GATE', 'FX_INST', 'FXMOD_FILTER_CUT', 'FXMOD_FILTER_RES', 'FXMOD_DRIVE', 'FXMOD_TREM_DEPTH', 'FXMOD_PAN_DEPTH', 'FXMOD_GRAINS_MIX', 'FXMOD_SHIMMER_MIX', 'LESLIE_STOP', 'LESLIE_SLOW', 'LESLIE_FAST', 'SCALE_MAJOR', 'SCALE_MINOR', 'SCALE_PENTA', 'SCALE_PENTA_MIN', 'SCALE_BLUES', 'SCALE_CHROMATIC', 'CHORD_MAJ', 'CHORD_MIN', 'CHORD_DIM', 'CHORD_AUG', 'CHORD_MAJ7', 'CHORD_MIN7', 'CHORD_DOM7', 'CHORD_SUS4', 'CHORD_POWER'] },
  { title: 'utility', keys: ['rnd', 'rnd_between', 'rnd_float', 'rnd_float_between', 'now', 'dt', 'epoch', 'frame', 'fps', 'sgn', 'mid', 'timer', 'timer_reset'] },
  { title: 'persistence', keys: ['save', 'load', 'save_int', 'load_int', 'save_bytes', 'load_bytes', 'de_state', 'STATE', 'S', 'GameState'] },
  { title: 'external data', keys: ['de_data_path', 'de_dropped_file', 'de_open_path', 'de_switch_cart'] },
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
let docsSidebar          // left-hand nav (search box + results + nav tree)
let docsNav              // the nav-tree container inside the sidebar (hidden while searching)
let docsSearchInput      // the unified "search all docs" box
let docsSearchResults    // ranked-results container (shown while searching)
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
  // multi-term: each whitespace-separated word is highlighted (a single word
  // behaves exactly as before). Lets "keybed midi" light up both, and lets a
  // search-result jump land on any of the query's words.
  const terms = query.toLowerCase().split(/\s+/).filter(Boolean)
  const walker = document.createTreeWalker(docsContent, NodeFilter.SHOW_TEXT, {
    acceptNode: node => node.nodeValue.trim() ? NodeFilter.FILTER_ACCEPT : NodeFilter.FILTER_REJECT,
  })
  const nodes = []
  for (let n = walker.nextNode(); n; n = walker.nextNode()) nodes.push(n)

  nodes.forEach(node => {
    const text = node.nodeValue
    const lower = text.toLowerCase()
    // collect every term's match ranges, then merge overlaps so adjacent/overlapping
    // hits don't nest <mark>s
    let ranges = []
    for (const t of terms) {
      let idx = lower.indexOf(t)
      while (idx !== -1) { ranges.push([idx, idx + t.length]); idx = lower.indexOf(t, idx + t.length) }
    }
    if (!ranges.length) return
    ranges.sort((a, b) => a[0] - b[0])
    const merged = []
    for (const r of ranges) {
      const last = merged[merged.length - 1]
      if (last && r[0] <= last[1]) last[1] = Math.max(last[1], r[1])
      else merged.push(r.slice())
    }
    const frag = document.createDocumentFragment()
    let last = 0
    for (const [s, e] of merged) {
      if (s > last) frag.appendChild(document.createTextNode(text.slice(last, s)))
      const mark = document.createElement('mark')
      mark.className = 'docs-find-hit'
      mark.textContent = text.slice(s, e)
      frag.appendChild(mark)
      last = e
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

// Break a function signature across lines when it takes any params, so the long
// descriptions get a narrower left column. Zero-param sigs and constants
// (#define …, which may carry parens in the macro body) stay on one line.
//   void reverb(float size, float damping)  ->  void reverb(
//                                                  float size,
//                                                  float damping
//                                                )
function formatSig(sig) {
  if (sig.startsWith('#define')) return sig        // macro bodies aren't param lists
  const open = sig.indexOf('(')
  const close = sig.lastIndexOf(')')
  if (open < 0 || close < open) return sig
  const inner = sig.slice(open + 1, close).trim()
  if (!inner || inner === 'void') return sig
  const params = inner.split(',').map(p => p.trim())
  const prefix = sig.slice(0, open + 1)            // "void reverb("
  const suffix = sig.slice(close)                  // ")" (+ anything trailing)
  const lines = params.map(p => '&nbsp;&nbsp;' + p).join(',<br>')
  return `${prefix}<br>${lines}<br>${suffix}`
}

// — the studioDocs-driven API reference (the function/constant list) —
function renderApiReference() {
  currentDocPath = ''
  setActiveNav('api')
  docsContent.classList.remove('docs-history')
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
        <div class="help-sig">${swatch}${formatSig(entry.sig)}</div>
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
  docsContent.classList.remove('docs-history')
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

// — the generated project history (docs/history.html) in an iframe —
// it's a dedicated styled page; doc links inside it postMessage back here
// (see the 'message' listener below) so they open in this same docs pane.
// The page persists its own scroll in sessionStorage (it knows when its layout
// has settled), so returning to ★ history after clicking an ADR/cart lands you
// back where you were — no back button needed.
function showHistory() {
  currentDocPath = 'history'
  setActiveNav('history')
  docsContent.classList.add('docs-history')
  docsContent.innerHTML = `<iframe class="history-frame" src="/docs/history.html" title="project history" onload="this.classList.add('loaded')"></iframe>`
  docsContent.scrollTop = 0
}

// — the generated cart technique compendium (docs/cart-compendium.html) in an
// iframe — what each cart teaches + its lineage. Clicking a cart name inside it
// postMessages back here (the 'message' listener below) to load that cart.
function showCompendium() {
  currentDocPath = 'compendium'
  setActiveNav('compendium')
  docsContent.classList.add('docs-history')
  docsContent.innerHTML = `<iframe class="compendium-frame" src="/docs/cart-compendium.html" title="cart technique compendium" onload="this.classList.add('loaded')"></iframe>`
  docsContent.scrollTop = 0
}

// — the generated design board (docs/design-board.html) in an iframe — every
// design doc + ADR by lifecycle phase. Clicking a card postMessages back here
// (the 'message' listener above) to open that doc's markdown.
function showDesignBoard() {
  currentDocPath = 'designboard'
  setActiveNav('designboard')
  docsContent.classList.add('docs-history')
  docsContent.innerHTML = `<iframe class="compendium-frame" src="/docs/design-board.html" title="design board" onload="this.classList.add('loaded')"></iframe>`
  docsContent.scrollTop = 0
}

// — the generated Reflections page (docs/reflections.html) in an iframe — the
// research journal by lifecycle. Clicking a note postMessages back here to open
// its markdown.
function showReflections() {
  currentDocPath = 'reflections'
  setActiveNav('reflections')
  docsContent.classList.add('docs-history')
  docsContent.innerHTML = `<iframe class="compendium-frame" src="/docs/reflections.html" title="reflections" onload="this.classList.add('loaded')"></iframe>`
  docsContent.scrollTop = 0
}

// — the generated Band Briefs page (docs/band-briefs.html) in an iframe — every
// blind band brief paired with the radio cart it became. Clicking a card loads
// that station; "read brief →" postMessages back here to open its markdown.
function showBandBriefs() {
  currentDocPath = 'bandbriefs'
  setActiveNav('bandbriefs')
  docsContent.classList.add('docs-history')
  docsContent.innerHTML = `<iframe class="compendium-frame" src="/docs/band-briefs.html" title="band briefs" onload="this.classList.add('loaded')"></iframe>`
  docsContent.scrollTop = 0
}

// the history iframe asks us to open a docs/ markdown file, or load a cart, when
// something inside it is clicked
window.addEventListener('message', (e) => {
  const m = e.data
  if (!m) return
  if (m.type === 'open-doc' && typeof m.path === 'string') {
    if (!/^[\w./-]+\.md$/.test(m.path)) return   // docs-relative .md only
    const helpTab = document.querySelector('.tab[data-tab="help"]')
    if (helpTab && !helpTab.classList.contains('active')) helpTab.click()
    showDoc(m.path)
  } else if (m.type === 'load-cart' && typeof m.file === 'string' && /^[\w-]+\.cart\.png$/.test(m.file)) {
    if (m.title) setCartName(m.title)
    currentCartFile = m.file.replace(/\.cart\.png$/i, '')
    currentCartPath = ''   // gallery cart — no save-in-place origin
    loadCartFromUrl('/carts/' + m.file)   // applies the cart + switches to the code tab
  }
})

// — render a markdown doc from docs/ into the content pane —
async function showDoc(relPath) {
  currentDocPath = relPath
  setActiveNav(relPath)
  docsContent.classList.remove('docs-history')
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

// a collapsible sidebar group: clicking its head toggles the group open/closed,
// state persisted per-head (so closing "archive"/"field-notes" etc. sticks). The
// folders grow without limit, so every group is an accordion.
function navGroup(head) {
  const grp = document.createElement('div')
  grp.className = 'docs-nav-group'
  const h = document.createElement('div')
  h.className = 'docs-nav-head'
  h.textContent = head
  const key = 'docsGroup:' + head
  if (localStorage.getItem(key) === 'closed') grp.classList.add('collapsed')
  h.addEventListener('click', () => {
    const closed = grp.classList.toggle('collapsed')
    localStorage.setItem(key, closed ? 'closed' : 'open')
  })
  grp.appendChild(h)
  return grp
}

// ── unified docs search (all docs, ranked; current doc boosted) ──────────────
const escHtml = s => s.replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]))
// highlight every query word inside a snippet, in one escaped pass
function hlSnippet(text, terms) {
  const esc = escHtml(text)
  const pat = terms.filter(Boolean).map(t => t.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')).join('|')
  return pat ? esc.replace(new RegExp('(' + pat + ')', 'ig'), '<mark>$1</mark>') : esc
}

let docsSearchSeq = 0   // guards against out-of-order async responses
async function runDocsSearch(query) {
  const q = query.trim()
  if (!q) { clearDocsSearch(); return }
  const seq = ++docsSearchSeq
  let results = []
  try { results = await (await fetch('/docs-search?q=' + encodeURIComponent(q))).json() } catch {}
  if (seq !== docsSearchSeq) return   // a newer query already fired
  renderDocsSearch(q, results)
}

function clearDocsSearch() {
  docsSearchSeq++
  docsSearchResults.innerHTML = ''
  docsSidebar.classList.remove('searching')
}

function renderDocsSearch(query, results) {
  docsSidebar.classList.add('searching')   // CSS hides the nav tree, shows results
  const terms = query.toLowerCase().split(/\s+/).filter(Boolean)
  // boost the currently-open doc to the top (it behaves like find-in-this)
  results.sort((a, b) => (b.path === currentDocPath) - (a.path === currentDocPath))

  if (!results.length) {
    docsSearchResults.innerHTML = `<div class="docs-search-empty">no matches for “${escHtml(query)}”</div>`
    return
  }
  docsSearchResults.innerHTML = ''
  for (const r of results) {
    const current = r.path === currentDocPath
    const card = document.createElement('div')
    card.className = 'docs-result' + (current ? ' current' : '')
    // current doc shows all its hits (in document order, so click→jump maps 1:1);
    // other docs show just the best line as a preview
    const snips = current ? r.lines.slice().sort((a, b) => a.n - b.n) : r.lines.slice(0, 1)
    const tag = current ? `<span class="docs-result-tag">this doc</span>` : ''
    card.innerHTML =
      `<div class="docs-result-head"><span class="docs-result-title">${escHtml(r.title)}</span>` +
      `<span class="docs-result-group">${escHtml(r.group)}</span>${tag}</div>` +
      `<div class="docs-result-snips">` +
      snips.map((s, i) => `<button class="docs-result-snip" data-occ="${current ? i : 0}">${hlSnippet(s.text, terms)}</button>`).join('') +
      `</div>`
    card.querySelector('.docs-result-head').addEventListener('click', () => openSearchHit(r.path, query, 0))
    card.querySelectorAll('.docs-result-snip').forEach(btn =>
      btn.addEventListener('click', () => openSearchHit(r.path, query, +btn.dataset.occ)))
    docsSearchResults.appendChild(card)
  }
}

// open a result and land on the matching text: render the doc, then reuse the
// find machinery to highlight every query word and scroll to the chosen hit.
// We do NOT open the Cmd+F bar here — the highlights/scroll work on their own;
// popping the find panel on every result click is noise (the search box is the UI).
async function openSearchHit(path, query, occ) {
  const land = () => {
    runFind(query)
    if (findMatches.length) { findIndex = Math.min(occ || 0, findMatches.length - 1); highlightCurrent(true) }
  }
  if (path !== currentDocPath) await showDoc(path)
  land()
}

async function buildDocsSidebar() {
  docsNav.innerHTML = ''
  docsNav.appendChild(docNavItem('★ API', 'api', () => renderApiReference()))
  docsNav.appendChild(docNavItem('★ history', 'history', () => showHistory()))
  docsNav.appendChild(docNavItem('★ compendium', 'compendium', () => showCompendium()))
  docsNav.appendChild(docNavItem('★ todo', 'designboard', () => showDesignBoard()))
  docsNav.appendChild(docNavItem('★ reflections', 'reflections', () => showReflections()))
  docsNav.appendChild(docNavItem('★ briefs', 'bandbriefs', () => showBandBriefs()))

  // the engine's own C files, readable right here (cmd-click an #include in
  // your cart jumps to the same view) — studio.h first, then the cart-land
  // library headers, then internals
  const engGrp = navGroup('engine source')
  ENGINE_SOURCES.forEach(f =>
    engGrp.appendChild(docNavItem(f, 'engine:' + f, () => showEngineSource(f))))
  docsNav.appendChild(engGrp)

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
    const grp = navGroup(head)
    list.forEach(f => grp.appendChild(docNavItem(prettyDocLabel(f), f, () => showDoc(f))))
    docsNav.appendChild(grp)
  }
  if (top.length) addGroup('guide', top)
  Object.keys(folders).sort().forEach(dir => addGroup(dir, folders[dir].sort()))
}

async function buildDocsTab() {
  helpPanel.classList.add('docs-tab')
  helpPanel.innerHTML = ''
  docsSidebar = document.createElement('div'); docsSidebar.id = 'docs-sidebar'
  docsContent = document.createElement('div'); docsContent.id = 'docs-content'
  // sidebar = a unified "search all docs" box, the ranked-results pane (shown
  // while typing), and the nav tree (hidden while searching)
  docsSidebar.innerHTML = `
    <div id="docs-search-box"><input id="docs-search-input" type="text" placeholder="search all docs…" spellcheck="false" /></div>
    <div id="docs-search-results"></div>
    <div id="docs-nav"></div>`
  docsSearchInput = docsSidebar.querySelector('#docs-search-input')
  docsSearchResults = docsSidebar.querySelector('#docs-search-results')
  docsNav = docsSidebar.querySelector('#docs-nav')
  let searchTimer
  docsSearchInput.addEventListener('input', () => {
    clearTimeout(searchTimer)
    const v = docsSearchInput.value
    searchTimer = setTimeout(() => runDocsSearch(v), 150)
  })
  docsSearchInput.addEventListener('keydown', e => {
    if (e.key === 'Escape') { e.preventDefault(); docsSearchInput.value = ''; clearDocsSearch() }
  })
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
  currentCartThumb = url   // the .cart.png thumbnail, shown in the share popover header
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

// faceted cart filters (see editor/public/carts/index.json). index.json is generated
// from per-cart de:meta (build-cart-index.js) in alphabetical order — not curated — so
// the default sort is 'updated'. state is module-level so it survives the panel rebuild.
const CART_KIND_ORDER  = ['tutorial', 'game', 'tech-demo', 'instrument', 'toy', 'tool', 'probe']
// mirror the canonical GENRES vocabulary in tools/lint-carts.js — any genre missing here
// silently drops its filter chip. Keep in sync when a genre is added there.
const CART_GENRE_ORDER = ['arcade', 'shooter', 'platformer', 'fighting', 'puzzle', 'racing',
                          'sports', 'strategy', 'rpg', 'adventure', 'simulation', 'sandbox',
                          'tabletop', 'maze', 'space', 'lab', 'dating',
                          'tycoon', 'tactics', 'word', '4x']
// orientation facet (derived from screen dims by build-cart-index.js; landscape = absent).
// label + icon per value; order = chip order.
const CART_ORIENTATION_ORDER = [['portrait', '📱 portrait'], ['square', '⬛ square']]
let cartFilter = null   // null = all; else { axis: 'kind'|'genre', value } — flat single-select
let cartSort = localStorage.getItem('cartSort') || 'updated'   // 'updated' (default) | 'title' | 'newest' | 'oldest'
if (cartSort === 'featured') cartSort = 'updated'   // retired: index.json array order is generated (alphabetical), no longer curated
// { file: { added, updated } } ISO dates (git history, served by vite). Seeded from
// localStorage so the panel renders instantly on open; refreshed in the background (see
// buildTutorialsPanel) so a freshly-committed rebake still surfaces without a restart.
let cartDates = null
try { cartDates = JSON.parse(localStorage.getItem('cartDates') || 'null') } catch {}
let cartDatesBuild = 0      // bumped per panel build; the async refresh bails if it changed

async function buildTutorialsPanel() {
  const body = tutorialsPanel.querySelector('#tutorials-body')
  if (!body) return
  const myBuild = ++cartDatesBuild
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

  // cart dates: render now with the cached map (localStorage / prior fetch); a live git
  // refresh runs in the background below, after the grid is built (powers Newest/Oldest +
  // Recently-updated). No await here = opening the tab never blocks on git.
  if (!cartDates) cartDates = {}

  const search = document.createElement('input')
  search.id = 'tutorials-search'
  search.className = 'tutorials-search'
  search.type = 'search'
  search.placeholder = 'search carts…'
  search.autocomplete = 'off'
  body.appendChild(search)

  // ── sort control: curated order, title, or git date (added / last-updated) ──
  const sortSel = document.createElement('select')
  sortSel.className = 'tutorials-sort'
  sortSel.title = 'sort carts'
  for (const [value, label] of [
    ['updated', 'recently updated'], ['title', 'title A–Z'],
    ['newest', 'newest'], ['oldest', 'oldest'],
  ]) {
    const opt = document.createElement('option')
    opt.value = value; opt.textContent = label
    sortSel.appendChild(opt)
  }
  sortSel.value = cartSort
  sortSel.addEventListener('change', () => { cartSort = sortSel.value; localStorage.setItem('cartSort', cartSort); applyFilter() })
  body.appendChild(sortSel)

  // ── filter chips: one flat list (kinds + genres), single-select ──
  const kindCounts = {}, genreCounts = {}, orientCounts = {}
  index.forEach(e => {
    (e.kind || []).forEach(k => { kindCounts[k] = (kindCounts[k] || 0) + 1 })
    if (e.genre) genreCounts[e.genre] = (genreCounts[e.genre] || 0) + 1
    if (e.orientation) orientCounts[e.orientation] = (orientCounts[e.orientation] || 0) + 1
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

  // kinds first, then genres, then orientation — same flat row, no visual divide
  makeChip('all', null, index.length, null)
  CART_KIND_ORDER.forEach(k => { if (kindCounts[k]) makeChip(k, k, kindCounts[k], 'kind') })
  CART_GENRE_ORDER.forEach(g => { if (genreCounts[g]) makeChip(g, g, genreCounts[g], 'genre') })
  CART_ORIENTATION_ORDER.forEach(([v, label]) => { if (orientCounts[v]) makeChip(label, v, orientCounts[v], 'orientation') })

  function syncChips() {
    chipRow.querySelectorAll('.cart-chip').forEach(b => {
      const axis = b.dataset.axis || null, value = b.dataset.value || null
      b.classList.toggle('active', value == null ? cartFilter == null : chipOn(axis, value))
    })
  }

  function passesChips(it) {
    if (!cartFilter) return true
    if (cartFilter.axis === 'kind') return it.kind.includes(cartFilter.value)
    if (cartFilter.axis === 'orientation') return it.orientation === cartFilter.value
    return it.genre === cartFilter.value
  }

  const grid = document.createElement('div')
  grid.className = 'tutorials-grid'
  body.appendChild(grid)

  const noMatch = document.createElement('p')
  noMatch.className = 'tutorials-empty'
  noMatch.textContent = 'no carts match'
  noMatch.style.display = 'none'
  body.appendChild(noMatch)

  const items = index.map(({ title, description, file, kind, genre, orientation }, idx) => {
    const card = document.createElement('div')
    card.className = 'tutorial-card'

    const url = `/carts/${file}`

    const img = document.createElement('img')
    img.className = 'tutorial-thumb'
    img.src = url
    img.alt = title
    img.onload = () => img.classList.add('loaded')   // fade in once decoded (no black-rect flash)
    if (img.complete) img.classList.add('loaded')    // cached: already decoded, skip the fade-in
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

    card.addEventListener('click', () => { setCartName(title); currentCartFile = String(file).replace(/\.cart\.png$/i, ''); currentCartPath = ''; loadCartFromUrl(url) })

    grid.appendChild(card)
    const d = (cartDates && cartDates[file]) || {}
    return { card, titleEl, descEl, idx, title: title || '', desc: description || '', file,
             name: String(file).replace(/\.cart\.png$/i, ''), kind: kind || [], genre: genre || null,
             orientation: orientation || null,
             added: d.added || '', updated: d.updated || d.added || '' }
  })

  // sort comparators for the no-search branch ('featured' keeps index.json order)
  function sortPool(pool) {
    if (cartSort === 'title') return pool.slice().sort((a, b) => a.title.localeCompare(b.title))
    if (cartSort === 'newest') return pool.slice().sort((a, b) => (b.added || '').localeCompare(a.added || ''))
    if (cartSort === 'oldest') return pool.slice().sort((a, b) => (a.added || '').localeCompare(b.added || ''))
    if (cartSort === 'updated') return pool.slice().sort((a, b) => (b.updated || '').localeCompare(a.updated || ''))
    return pool   // fallback: generated (alphabetical) order
  }

  function applyFilter() {
    const q = search.value.trim()
    const pool = items.filter(passesChips)
    items.forEach(it => { it.card.style.display = 'none' })   // hide all; the branches re-show the pool
    if (!q) {                                    // no text → show chip-passing in sort order, plain text
      sortPool(pool).forEach(it => {
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

  // live git refresh, off the critical path: pull fresh dates, and only if they actually
  // changed, cache them + patch the items' dates + re-sort. Bails if the panel was rebuilt
  // or closed while the fetch was in flight (stale closure).
  ;(async () => {
    let fresh
    try { fresh = await (await fetch('/carts/dates.json')).json() } catch { return }
    if (myBuild !== cartDatesBuild) return
    if (JSON.stringify(fresh) === JSON.stringify(cartDates)) return
    cartDates = fresh
    try { localStorage.setItem('cartDates', JSON.stringify(fresh)) } catch {}
    items.forEach(it => {
      const d = cartDates[it.file] || {}
      it.added = d.added || ''
      it.updated = d.updated || d.added || ''
    })
    applyFilter()
  })()
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

// build + run the current buffer. `net` is null for a normal run, or
// { mode:'host' } / { mode:'join', ip } for lockstep netplay (rung 2 — see the
// 🌐 button below). Shared so run / host / join take the identical build path.
async function runCart(net) {
  if (!window.studio) {
    showLog({ ok: false, cmd: null, output: 'run requires the desktop app  (npm start)' })
    return
  }

  const code = view.state.doc.toString()
  setErrorLines([])
  runBtn.textContent = '⏳ compiling…'
  runBtn.disabled = true
  netBtn && (netBtn.disabled = true)
  buildLog.style.display = 'none'
  rlogClear()

  // export sprite sheet from editor canvas before compiling
  const tilemapCanvas = document.querySelector('#tilemap-canvas')
  if (tilemapCanvas) {
    await window.studio.saveSprites(tilemapCanvas.toDataURL('image/png'))
  }

  // export the current map as raw bytes
  await window.studio.saveMap(getMapBytes())

  const result = await window.studio.run(code, { ...settings, cartName: currentCartName, net: net || undefined })
  liveHostRunning = !!(result && result.live)   // a live window is now open → enable auto-reload

  runBtn.textContent = '▶ run'
  runBtn.disabled = false
  netBtn && (netBtn.disabled = false)

  // host: surface the address the joiner must type (also printed in the runtime log)
  if (result && result.ok && net && net.mode === 'host' && result.netIp) {
    showToast(`🌐 hosting at ${result.netIp} — on the other machine click 🌐 → Join and type that IP`, 8000)
  }

  showLog(result)
}

runBtn.addEventListener('click', () => runCart(null))

// ── multiplayer (netplay) button — rung 2, LAN by IP ──────────
// One machine hosts (P0), another on the same wifi joins by the host's IP (P1).
// docs/design/multiplayer-research.md. Native run backend only (browsers have no
// UDP; the live host owns its own loop, so the lockstep barrier doesn't fit yet).
const netBtn = document.getElementById('net-btn')
if (settings.showNetplay && netBtn) netBtn.style.display = ''   // hidden by default; opt-in via settings → multiplayer
let netMenu = null
function closeNetMenu() {
  if (!netMenu) return
  netMenu.remove(); netMenu = null
  document.removeEventListener('mousedown', netMenuOutside)
}
function netMenuOutside(e) {
  if (netMenu && !netMenu.contains(e.target) && e.target !== netBtn) closeNetMenu()
}
netBtn?.addEventListener('click', () => {
  if (!window.studio) { showLog({ ok:false, cmd:null, output:'multiplayer requires the desktop app  (npm start)' }); return }
  if (settings.backend === 'live') { showToast('multiplayer needs the native run backend — turn off “live” in settings', 4000); return }
  if (netMenu) { closeNetMenu(); return }

  netMenu = document.createElement('div')
  netMenu.id = 'net-menu'
  netMenu.innerHTML = `
    <button id="net-host">🖥 Host on this machine  ·  you are P0</button>
    <div class="net-row">
      <input id="net-ip" type="text" placeholder="host IP, e.g. 192.168.1.23" spellcheck="false" />
      <button id="net-join">Join · P1</button>
    </div>
    <div class="net-hint">Same wifi, no servers. The host shows its IP when it starts — type it here.</div>`
  document.body.appendChild(netMenu)
  const r = netBtn.getBoundingClientRect()
  netMenu.style.top   = `${r.bottom + 4}px`
  netMenu.style.right = `${window.innerWidth - r.right}px`

  const ipInput = netMenu.querySelector('#net-ip')
  const doJoin = () => {
    const ip = ipInput.value.trim()
    if (!ip) { ipInput.focus(); return }
    closeNetMenu(); runCart({ mode: 'join', ip })
  }
  netMenu.querySelector('#net-host').onclick = () => { closeNetMenu(); runCart({ mode: 'host' }) }
  netMenu.querySelector('#net-join').onclick = doJoin
  ipInput.addEventListener('keydown', e => { if (e.key === 'Enter') doJoin() })
  ipInput.focus()
  setTimeout(() => document.addEventListener('mousedown', netMenuOutside), 0)
})

// ── share popover (current cart) — one surface, actions grouped by audience ────
// Mirrors the net-menu pattern; the buttons inside keep their existing handlers
// (bound by id), so this is show / position / close only. docs/design/share-panel.md.
const shareBtn  = document.getElementById('share-btn')
const shareMenu = document.getElementById('share-menu')
let shareOpen = false
function closeShareMenu() {
  if (!shareOpen) return
  shareMenu.hidden = true; shareOpen = false
  document.removeEventListener('mousedown', shareOutside)
}
function shareOutside(e) {
  if (shareOpen && !shareMenu.contains(e.target) && e.target !== shareBtn) closeShareMenu()
}
shareBtn?.addEventListener('click', () => {
  if (shareOpen) { closeShareMenu(); return }
  // header: which cart am I sharing?
  const nameEl = document.getElementById('share-cart-name')
  const thumbEl = document.getElementById('share-cart-thumb')
  if (nameEl) nameEl.textContent = currentCartName || 'untitled cart'
  if (thumbEl) {
    if (currentCartThumb) { thumbEl.src = currentCartThumb; thumbEl.style.display = '' }
    else thumbEl.style.display = 'none'
  }
  shareMenu.hidden = false; shareOpen = true
  const r = shareBtn.getBoundingClientRect()
  shareMenu.style.top   = `${r.bottom + 4}px`
  shareMenu.style.right = `${window.innerWidth - r.right}px`
  setTimeout(() => document.addEventListener('mousedown', shareOutside), 0)
})
// starting an action closes the popover so the runtime log (each tool streams to it) shows
shareMenu?.addEventListener('click', e => { if (e.target.closest('button')) closeShareMenu() })

// ── Apps view: research lab (app-less) + apps list ────────────
// The standalone tools (aso-research) run with NO app selected — a research sandbox;
// app-scoped actions (build-app/press-kit) come with a selection in a later slice.
const asoRun = document.getElementById('aso-run')
const asoOut = document.getElementById('aso-out')
window.studio?.onAsoLog?.(s => { if (asoOut) asoOut.textContent += stripAnsi(s) })   // stripAnsi() defined below (hoisted)
asoRun?.addEventListener('click', async () => {
  if (!window.studio?.asoResearch) { showToast('research requires the desktop app  (npm start)', 3000); return }
  const termsEl = document.getElementById('aso-terms')
  const terms = termsEl.value
  const cc = document.getElementById('aso-cc').value || 'us'
  if (!terms.trim()) { termsEl.focus(); return }
  asoOut.textContent = ''
  const stop = busyDots(asoRun, 'researching', 'research')
  asoRun.disabled = true
  await window.studio.asoResearch(terms, cc)
  stop(); asoRun.disabled = false
})
document.getElementById('aso-terms')?.addEventListener('keydown', e => { if (e.key === 'Enter') asoRun?.click() })

async function renderAppsList() {
  const el = document.getElementById('apps-list')
  if (!el || !window.studio?.listApps) return
  const { apps } = await window.studio.listApps()
  if (!apps || !apps.length) { el.innerHTML = '<div class="apps-empty">no apps yet — an app is <code>apps/&lt;name&gt;/app.json</code></div>'; return }
  el.innerHTML = ''
  for (const a of apps) {
    const card = document.createElement('div')
    card.className = 'app-card'
    const meta = [a.carts.join(', '), a.iap ? `${a.iap} IAP` : ''].filter(Boolean).join('  ·  ')
    card.innerHTML = `<div class="app-name"></div><div class="app-meta"></div>`
    card.querySelector('.app-name').textContent = a.name
    card.querySelector('.app-meta').textContent = meta
    el.appendChild(card)
  }
}
document.querySelector('.tab[data-tab="apps"]')?.addEventListener('click', renderAppsList)

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

// Save the open cart. `forceDialog` (Save As) always prompts; otherwise we
// save-in-place when we have a real on-disk origin, else fall through to the
// dialog (fresh / gallery carts have no origin to overwrite).
async function saveCart(forceDialog) {
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
  const targetPath = (!forceDialog && currentCartPath) ? currentCartPath : undefined
  const saved = await window.studio.saveCart({ source: view.state.doc.toString(), spritesDataUrl, mapBase64, settings: cartSettings, targetPath })
  if (!saved || !saved.ok) {
    if (saved && saved.error) showToast(`save failed: ${saved.error}`)
    return
  }
  // remember the origin for next time (null when saved into the gallery dir)
  currentCartPath = saved.origin || ''
  // adopt the saved filename as the cart name (shown in the window title)
  if (saved.filePath) {
    const base = saved.filePath.split('/').pop().replace(/\.cart\.png$/, '').replace(/\.png$/, '')
    if (base) { setCartName(base); currentCartFile = base }
  }
  showToast(saved.inPlace ? `saved → ${currentCartFile}.cart.png` : 'cart saved')
}

saveCartBtn.addEventListener('click', () => saveCart(false))

// Cmd/Ctrl+S → save (in-place if we have an origin); Shift adds → Save As.
// Capture phase, so CodeMirror doesn't eat it.
window.addEventListener('keydown', e => {
  if ((e.metaKey || e.ctrlKey) && (e.key === 's' || e.key === 'S')) {
    e.preventDefault()
    saveCart(e.shiftKey)
  }
  // Cmd/Ctrl+F → find-in-docs, but only while the Docs tab is showing
  if ((e.metaKey || e.ctrlKey) && (e.key === 'f' || e.key === 'F') && helpPanel.classList.contains('active')) {
    e.preventDefault()
    openFind()
  }
}, true)

const toast = document.getElementById('toast')
let toastTimer = null
function showToast(msg, ms = 2000) {
  toast.textContent = msg
  toast.classList.add('visible')
  clearTimeout(toastTimer)
  toastTimer = setTimeout(() => toast.classList.remove('visible'), ms)
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
  if (cart && cart.ok) { if (cart.name) { setCartName(cart.name); currentCartFile = cart.name } currentCartPath = cart.origin || ''; applyCart(cart) }
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

// ── export a standalone Windows .exe (send-to-a-friend) ────────
const exportWinBtn = document.getElementById('export-win-btn')

exportWinBtn?.addEventListener('click', async () => {
  if (!window.studio?.exportWin) { showToast('export requires the desktop app (npm start)', 3000); return }
  const tilemapCanvas = document.querySelector('#tilemap-canvas')
  if (tilemapCanvas) await window.studio.saveSprites(tilemapCanvas.toDataURL('image/png'))
  await window.studio.saveMap(getMapBytes())

  const stopDots = busyDots(exportWinBtn, 'exporting', '🖥 export .exe')
  exportWinBtn.disabled = true
  rlogClear()   // open the runtime log panel for build output

  const code = view.state.doc.toString()
  const result = await window.studio.exportWin(code, { ...settings, cartName: currentCartName })

  stopDots()
  exportWinBtn.disabled = false

  if (result.ok) showToast('✓ .exe exported — revealed in Finder', 5000)
  else showLog(result)
})

// ── export a standalone Mac binary (send-to-another-Mac) ───────
const exportMacBtn = document.getElementById('export-mac-btn')

exportMacBtn?.addEventListener('click', async () => {
  if (!window.studio?.exportMac) { showToast('export requires the desktop app (npm start)', 3000); return }
  const tilemapCanvas = document.querySelector('#tilemap-canvas')
  if (tilemapCanvas) await window.studio.saveSprites(tilemapCanvas.toDataURL('image/png'))
  await window.studio.saveMap(getMapBytes())

  const stopDots = busyDots(exportMacBtn, 'exporting', '🍎 export mac')
  exportMacBtn.disabled = true
  rlogClear()

  const code = view.state.doc.toString()
  const result = await window.studio.exportMac(code, { ...settings, cartName: currentCartName })

  stopDots()
  exportMacBtn.disabled = false

  if (result.ok) showToast(result.msg || '✓ Mac export done — revealed in Finder', 5000)
  else showLog(result)
})

// ── deploy to iPhone (signed device build of the live buffer) ──
const deployIosBtn = document.getElementById('deploy-ios-btn')

deployIosBtn?.addEventListener('click', async () => {
  if (!window.studio?.deployIos) return   // Electron-only (browser tab can't spawn the build)
  const tilemapCanvas = document.querySelector('#tilemap-canvas')
  if (tilemapCanvas) await window.studio.saveSprites(tilemapCanvas.toDataURL('image/png'))
  await window.studio.saveMap(getMapBytes())

  const stopDots = busyDots(deployIosBtn, 'deploying', '\u{1F4F1} to iPhone')
  deployIosBtn.disabled = true
  rlogClear()   // open the runtime log panel for the device-build progress

  const code = view.state.doc.toString()
  const result = await window.studio.deployIos(code, { ...settings, cartName: currentCartName })

  stopDots()
  deployIosBtn.disabled = false

  if (result.ok) showToast('running on iPhone')
  else showLog(result)
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

// ── publish to site ─────
// Always visible in the Share popover's "Put it on my site" section (it streams its log
// and is confirm-by-clicking, so no settings gate needed here).
const publishBtn = document.getElementById('publish-btn')

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
  if (cart && cart.ok) { if (cart.name) { setCartName(cart.name); currentCartFile = cart.name } currentCartPath = cart.origin || ''; applyCart(cart) }
})

let hideTimer = null

function showLog(result) {
  clearTimeout(hideTimer)

  buildLog.style.display = 'block'
  buildLog.innerHTML = ''

  // pinned action group (copy + close) — stays top-right while the report scrolls
  const actions = document.createElement('div')
  actions.className = 'build-actions'

  const copy = document.createElement('button')
  copy.className = 'build-action'
  copy.textContent = '⧉ copy'
  copy.title = 'copy this report to the clipboard'
  copy.addEventListener('click', async () => {
    // every report line is a <div>; the action buttons are <button>, so this
    // grabs the report text only (no glyphs from these controls)
    const text = [...buildLog.querySelectorAll('div')].map(d => d.textContent).join('\n')
    try {
      await navigator.clipboard.writeText(text)
      copy.textContent = '✓ copied'
      setTimeout(() => { copy.textContent = '⧉ copy' }, 1200)
    } catch {
      copy.textContent = '✗ failed'
      setTimeout(() => { copy.textContent = '⧉ copy' }, 1200)
    }
  })
  actions.appendChild(copy)

  const close = document.createElement('button')
  close.className = 'build-action build-close'
  close.textContent = '×'
  close.title = 'close'
  close.addEventListener('click', () => {
    clearTimeout(hideTimer)
    buildLog.style.display = 'none'
  })
  actions.appendChild(close)
  buildLog.appendChild(actions)

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
  const who = currentCartName ? `“${currentCartName}”` : 'cart'
  head.textContent = `⏱ profiled ${who} · ${seconds}s`
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
    currentCartPath = ''   // default gallery cart — no save-in-place origin
    loadCartFromUrl('/carts/zoo.cart.png').catch(() => {})
  }
})

