import { view, setEditorTheme, setErrorLines, onDocChange } from './main.js'
import { initOutline, refreshOutline } from './outline.js'
import { ENGINE_SOURCES, showEngineFileIn, renderEngineOutline } from './navigate.js'
import './sprite-editor.js'
import { aseToDataUrl } from './aseprite.js'
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
  { title: 'graphics',      keys: ['cls', 'colorkey', 'spr', 'sprf', 'sspr', 'spr_rot', 'sspr_ex', 'pget', 'pget_rgb', 'enable_pget', 'pset', 'pset_rgb', 'sget', 'font', 'FONT_NORMAL', 'FONT_SMALL', 'FONT_TINY', 'FONT_COMIC', 'FONT_THIN', 'print', 'print_centered', 'print_right', 'hint', 'print_scaled', 'print_rot', 'print_rot_scaled', 'print_outline', 'text_width', 'rect', 'rectfill', 'rectfill_rgb', 'rectfill_rot', 'bar', 'circ', 'circfill', 'oval', 'ovalfill', 'arc', 'arcfill', 'arcoutline', 'ring', 'ringoutline', 'tri', 'trifill', 'tritex', 'line', 'bezier', 'camera', 'camera_ex', 'smooth_zoom', 'follow', 'clip', 'zoom_rect', 'pal', 'pal_reset', 'fade', 'shake'] },
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
  { title: 'sound',       keys: ['sfx', 'note', 'hit', 'note_on', 'note_off', 'note_pitch', 'note_vol', 'note_cutoff', 'note_duty', 'note_pan', 'note_res', 'note_lfo', 'note_lfo_shape', 'note_env', 'note_follow', 'note_harmonics', 'note_timbre', 'note_morph', 'voice_nasal', 'voice_consonant', 'voice_coda', 'instrument_mode', 'note_aux', 'note_drive', 'note_drive_mode', 'instrument_sync', 'note_sync', 'note_echo', 'note_reverb', 'note_filter', 'note_glide', 'note_off_all', 'instrument', 'instrument_duty', 'instrument_pan', 'instrument_level', 'pan_law', 'listener', 'listener_vel', 'spatial_model', 'spatial_speed', 'note_pos', 'note_motion', 'hit_at', 'instrument_pos', 'instrument_motion', 'instrument_lfo', 'lfo_shape', 'lfo_value', 'scope_read', 'scope_read2', 'instrument_filter', 'instrument_env', 'instrument_follow', 'instrument_harmonics', 'instrument_timbre', 'instrument_morph', 'instrument_tune', 'instrument_unison', 'instrument_unison_detune', 'instrument_drive', 'instrument_drive_mode', 'instrument_echo', 'instrument_reverb', 'instrument_reverb_bus', 'reverb_bus_fx', 'reverb_insert', 'echo_insert', 'drive_insert', 'drive_insert_inst', 'grains', 'instrument_grains', 'grains_freeze', 'instrument_grains_freeze', 'grains_pitch', 'instrument_grains_pitch', 'instrument_chorus', 'instrument_flanger', 'instrument_tape', 'instrument_wah', 'instrument_wah_lfo', 'instrument_crush', 'instrument_eq', 'instrument_formant', 'instrument_tremolo', 'instrument_autopan', 'instrument_ringmod', 'instrument_phaser', 'instrument_univibe', 'instrument_leslie', 'instrument_choke', 'echo', 'reverb', 'reverb_bus', 'chorus', 'flanger', 'tape', 'tape_inst', 'wah', 'wah_lfo', 'crush', 'crush_inst', 'eq', 'eq_inst', 'formant', 'filter', 'filter_inst', 'tremolo', 'autopan', 'ringmod', 'phaser', 'univibe', 'leslie', 'dropout', 'shallow', 'instrument_shallow', 'amp_noise', 'gate', 'instrument_gate', 'shimmer', 'instrument_shimmer', 'varispeed', 'fx_mod', 'instrument_fx_mod', 'fx_lfo', 'fx_order', 'sidechain', 'sidechain_key', 'glue', 'wave_set', 'tone', 'chord', 'strum', 'strum_notes', 'schedule', 'schedule_hit', 'bpm', 'beat', 'beat_pos', 'every', 'euclid', 'chance', 'degree', 'INSTR_SQUARE', 'INSTR_SAW', 'INSTR_TRI', 'INSTR_NOISE', 'INSTR_SINE', 'INSTR_USER0', 'INSTR_USER1', 'INSTR_USER2', 'INSTR_USER3', 'INSTR_PLUCK', 'INSTR_MALLET', 'INSTR_FM', 'INSTR_ORGAN', 'INSTR_EPIANO', 'INSTR_PD', 'INSTR_MEMBRANE', 'INSTR_REED', 'INSTR_PIPE', 'INSTR_VOICE', 'INSTR_GUITAR', 'INSTR_PIANO', 'INSTR_BOWED', 'INSTR_BRASS', 'LFO_PITCH', 'LFO_DUTY', 'LFO_VOLUME', 'LFO_CUTOFF', 'LFO_HARMONICS', 'LFO_TIMBRE', 'LFO_MORPH', 'LFO_PAN', 'LFO_DETUNE', 'PAN_LINEAR', 'PAN_POWER', 'ENV_CUTOFF', 'ENV_PITCH', 'ENV_DUTY', 'ENV_HARMONICS', 'ENV_TIMBRE', 'ENV_MORPH', 'ENV_DETUNE', 'FILTER_OFF', 'FILTER_LOW', 'FILTER_HIGH', 'FILTER_BAND', 'FILTER_NOTCH', 'FILTER_LADDER', 'FILTER_STEINER', 'FILTER_STEINER_HP', 'FILTER_STEINER_BP', 'FILTER_STEINER_NF', 'FILTER_DIODE', 'DRIVE_SOFT', 'DRIVE_HARD', 'DRIVE_FOLD', 'DRIVE_ASYM', 'LFO_SHAPE_SINE', 'LFO_SHAPE_SQUARE', 'LFO_SHAPE_TRI', 'LFO_SHAPE_SAW', 'LFO_SHAPE_RAMP', 'LFO_SHAPE_OPTICAL', 'LFO_SHAPE_SH', 'LFO_SHAPE_RANDOM', 'TREM_SINE', 'TREM_SQUARE', 'TREM_TRI', 'MODE_STRING_WEIGHT', 'MODE_STRING_CLICK', 'MODE_BOW_PIZZ', 'FX_TREM', 'FX_WAH', 'FX_CHORUS', 'FX_PHASER', 'FX_FLANGER', 'FX_TAPE', 'FX_EQ', 'FX_CRUSH', 'FX_REVERB', 'FX_FORMANT', 'FX_FILTER', 'FX_PAN', 'FX_RINGMOD', 'FX_ECHO', 'FX_GRAINS', 'FX_DRIVE', 'FX_SHALLOW', 'FX_GATE', 'FX_INST', 'FXMOD_FILTER_CUT', 'FXMOD_FILTER_RES', 'FXMOD_DRIVE', 'FXMOD_TREM_DEPTH', 'FXMOD_PAN_DEPTH', 'FXMOD_GRAINS_MIX', 'FXMOD_SHIMMER_MIX', 'LESLIE_STOP', 'LESLIE_SLOW', 'LESLIE_FAST', 'SCALE_MAJOR', 'SCALE_MINOR', 'SCALE_PENTA', 'SCALE_PENTA_MIN', 'SCALE_BLUES', 'SCALE_CHROMATIC', 'CHORD_MAJ', 'CHORD_MIN', 'CHORD_DIM', 'CHORD_AUG', 'CHORD_MAJ7', 'CHORD_MIN7', 'CHORD_DOM7', 'CHORD_SUS4', 'CHORD_POWER'] },
  { title: 'utility', keys: ['rnd', 'rnd_between', 'rnd_float', 'rnd_float_between', 'now', 'dt', 'epoch', 'frame', 'fps', 'sgn', 'mid', 'timer', 'timer_reset'] },
  { title: 'persistence', keys: ['save', 'load', 'save_int', 'load_int', 'save_bytes', 'load_bytes', 'de_state', 'STATE', 'S', 'GameState'] },
  { title: 'external data', keys: ['de_data_path', 'de_dropped_file', 'de_open_path', 'de_switch_cart'] },
  { title: 'debug',     keys: ['printh', 'watch', 'watch_visible', 'paused'] },
  { title: 'screen',       keys: ['SCREEN_W', 'SCREEN_H', 'screen_w', 'screen_h', 'safe_rect'] },
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
  docsContent.classList.remove('docs-history', 'docs-engine')
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
  docsContent.classList.remove('docs-history', 'docs-engine')
  docsContent.classList.add('docs-engine')   // full-height: editor scrolls internally, outline stays put
  docsContent.innerHTML = ''
  const head = document.createElement('div')
  head.className = 'engine-src-head'
  head.textContent = `runtime/${file} — read-only`
  docsContent.appendChild(head)
  // editor on the left, function outline on the right (mirrors the cart code tab)
  const row = document.createElement('div')
  row.className = 'engine-src-row'
  const wrap = document.createElement('div')
  wrap.className = 'engine-src'
  const aside = document.createElement('aside')
  aside.className = 'engine-outline'
  const ohead = document.createElement('div')
  ohead.className = 'engine-outline-head'
  ohead.textContent = 'outline'
  const olist = document.createElement('div')
  olist.className = 'engine-outline-list'
  aside.append(ohead, olist)
  row.append(wrap, aside)
  docsContent.appendChild(row)
  await showEngineFileIn(wrap, file)
  renderEngineOutline(olist)
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
  docsContent.classList.remove('docs-engine'); docsContent.classList.add('docs-history')
  docsContent.innerHTML = `<iframe class="history-frame" src="/docs/history.html" title="project history" onload="this.classList.add('loaded')"></iframe>`
  docsContent.scrollTop = 0
}

// — the generated cart technique compendium (docs/cart-compendium.html) in an
// iframe — what each cart teaches + its lineage. Clicking a cart name inside it
// postMessages back here (the 'message' listener below) to load that cart.
function showCompendium() {
  currentDocPath = 'compendium'
  setActiveNav('compendium')
  docsContent.classList.remove('docs-engine'); docsContent.classList.add('docs-history')
  docsContent.innerHTML = `<iframe class="compendium-frame" src="/docs/cart-compendium.html" title="cart technique compendium" onload="this.classList.add('loaded')"></iframe>`
  docsContent.scrollTop = 0
}

// — the generated design board (docs/design-board.html) in an iframe — every
// design doc + ADR by lifecycle phase. Clicking a card postMessages back here
// (the 'message' listener above) to open that doc's markdown.
function showDesignBoard() {
  currentDocPath = 'designboard'
  setActiveNav('designboard')
  docsContent.classList.remove('docs-engine'); docsContent.classList.add('docs-history')
  docsContent.innerHTML = `<iframe class="compendium-frame" src="/docs/design-board.html" title="design board" onload="this.classList.add('loaded')"></iframe>`
  docsContent.scrollTop = 0
}

// — the generated Reflections page (docs/reflections.html) in an iframe — the
// research journal by lifecycle. Clicking a note postMessages back here to open
// its markdown.
function showReflections() {
  currentDocPath = 'reflections'
  setActiveNav('reflections')
  docsContent.classList.remove('docs-engine'); docsContent.classList.add('docs-history')
  docsContent.innerHTML = `<iframe class="compendium-frame" src="/docs/reflections.html" title="reflections" onload="this.classList.add('loaded')"></iframe>`
  docsContent.scrollTop = 0
}

// — the generated Band Briefs page (docs/band-briefs.html) in an iframe — every
// blind band brief paired with the radio cart it became. Clicking a card loads
// that station; "read brief →" postMessages back here to open its markdown.
function showBandBriefs() {
  currentDocPath = 'bandbriefs'
  setActiveNav('bandbriefs')
  docsContent.classList.remove('docs-engine'); docsContent.classList.add('docs-history')
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
  docsContent.classList.remove('docs-history', 'docs-engine')
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
    cart = { ok: true, source: chunks.source, spritesDataUrl: chunks.sprites || null, mapBase64: chunks.map || null, settings, spritepatch: chunks.spritepatch || null }
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

// ● record — run the cart and capture your play as an input track. Triggered from the Promote
// tab's ● record a take (#promote-rec) — there's no toolbar/settings button anymore. The take
// runs the whole session; you play, then CLOSE the cart window (Esc) to stop + save it to
// tools/clips/<cart>/NN-take.rec. docs/design/input-recording-looper.md, promote-tab.md §A.
async function recordCart() {
  if (!window.studio?.record) { showLog({ ok: false, cmd: null, output: 'recording requires the desktop app  (npm start)' }); return }
  const code = view.state.doc.toString()
  setErrorLines([])
  const recBtn = document.getElementById('promote-rec')
  if (recBtn) { recBtn.textContent = '⏳ recording…'; recBtn.disabled = true }
  runBtn.disabled = true
  const tilemapCanvas = document.querySelector('#tilemap-canvas')
  if (tilemapCanvas) await window.studio.saveSprites(tilemapCanvas.toDataURL('image/png'))
  await window.studio.saveMap(getMapBytes())
  const result = await window.studio.record(code, { ...settings, cartName: currentCartName, cartFile: currentCartFile })
  if (recBtn) { recBtn.textContent = '● record a take'; recBtn.disabled = false }
  runBtn.disabled = false
  if (result && !result.ok) { showLog(result); return }
  showToast('● recording — play, then close the window (Esc) to save the take', 6000)
}
// the exact path also prints into the runtime log (persistent); the toast reveals it in Finder on click
window.studio?.onRecorded?.(info => {
  if (info?.ok) {
    showToast(`✓ take saved → ${info.rel}  ·  find it under promote`, 12000, () => window.studio.revealPath?.(info.abs || info.rel))
    // the new take should pop into the Promote takes list without a manual refresh — re-render if
    // that panel is showing (renderPromote is a hoisted fn declaration, so it's live by now).
    if (document.getElementById('panel-promote')?.classList.contains('active')) renderPromote()
  }
  else if (info?.empty) showToast('no input captured — nothing saved', 3500)
  else showToast('recording failed: ' + (info?.error || '?'), 4000)
})

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
if (settings.showShare && shareBtn) shareBtn.style.display = ''   // hidden by default; opt-in via settings → share
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
const asoResults = document.getElementById('aso-results')
const fmtRatings = x => x >= 1e6 ? (x / 1e6).toFixed(1) + 'M' : x >= 1e3 ? (x / 1e3).toFixed(1) + 'k' : String(x)
// escHtml() is defined elsewhere in shell.js (hoisted / module scope). researchHtml/suggestHtml
// are container-agnostic string builders so BOTH the Apps ASO lab (asoResults) and the shared
// keyword-research popup (#kw-results) can render the same results.
function researchHtml(data) {
  let h = ''
  for (const t of (data.terms || [])) {
    if (t.error) { h += `<div class="rs-term"><b>"${escHtml(t.term)}"</b> — ${escHtml(t.error)}</div>`; continue }
    h += `<div class="rs-term"><div class="rs-head"><b>"${escHtml(t.term)}"</b> `
      + `<span class="rs-badge rs-${escHtml(t.band).toLowerCase()}">${escHtml(t.band)} ${t.score}</span> `
      + `<span class="rs-dim">· ${t.n} apps · med ${fmtRatings(t.med)}${t.myRank ? ' · YOU #' + t.myRank : ''}</span></div>`
    for (const a of (t.top || [])) {
      const name = escHtml(a.name || '')
      const nm = a.url ? `<a href="#" data-url="${escHtml(a.url)}">${name}</a>` : name
      h += `<div class="rs-app"><span class="rs-r">${fmtRatings(a.ratings || 0)}</span> `
        + `<span class="rs-dim">★${(a.stars || 0).toFixed(1)}</span> ${nm} <span class="rs-g">${escHtml(a.genre || '')}</span></div>`
    }
    h += `</div>`
  }
  if (data.mined && data.mined.length) {
    h += `<div class="rs-mined"><span class="rs-dim">mined (click to research):</span> `
      + data.mined.map(([w, c]) => `<a href="#" class="rs-chip" data-term="${escHtml(w)}">${escHtml(w)}·${c}</a>`).join(' ')
      + `</div>`
  }
  return h
}
function renderResearch(data) { if (asoResults) asoResults.innerHTML = researchHtml(data) }
asoResults?.addEventListener('click', e => {
  const openL = e.target.closest('[data-openpath]')
  if (openL) { e.preventDefault(); window.studio?.openPath?.(openL.dataset.openpath); return }
  const link = e.target.closest('a[data-url]')
  if (link) { e.preventDefault(); window.studio?.openExternal?.(link.dataset.url); return }
  const chip = e.target.closest('a[data-term]')
  if (chip) { e.preventDefault(); document.getElementById('aso-terms').value = chip.dataset.term; asoRun?.click() }
  const cp = e.target.closest('[data-copy]')
  if (cp) { e.preventDefault(); navigator.clipboard?.writeText(cp.dataset.copy).then(() => showToast('post scaffold copied — open a venue and paste', 2500)) }
})
asoRun?.addEventListener('click', async () => {
  if (!window.studio?.asoResearch) { showToast('research requires the desktop app  (npm start)', 3000); return }
  const termsEl = document.getElementById('aso-terms')
  const terms = termsEl.value
  const cc = document.getElementById('aso-cc').value || 'us'
  if (!terms.trim()) { termsEl.focus(); return }
  asoOut.textContent = ''; if (asoResults) asoResults.innerHTML = ''
  const stop = busyDots(asoRun, 'researching', 'research')
  asoRun.disabled = true
  const res = await window.studio.asoResearch(terms, cc)
  stop(); asoRun.disabled = false
  if (res?.ok && res.data) renderResearch(res.data)
  else if (asoResults) asoResults.innerHTML = `<div class="rs-err">${escHtml((res && res.error) || 'research failed')}</div>`
})
document.getElementById('aso-terms')?.addEventListener('keydown', e => { if (e.key === 'Enter') asoRun?.click() })

// suggest — Google-autocomplete demand proxy. TWO audiences: single WORDS → the App Store
// keyword field (auto-fills the compose box), natural PHRASES → website/press SEO.
const sugRun = document.getElementById('sug-run')
function suggestHtml(data) {
  let h = '<div class="rs-note">Google-intent demand proxy — relative, not App Store volume.</div>'
  h += `<div class="rs-block"><b>App Store keyword words</b> <span class="rs-dim">→ filled into the compose box below</span><div class="rs-chips">`
    + (data.mined || []).map(([w, c]) => `<span class="rs-chip">${escHtml(w)}·${c}</span>`).join(' ') + '</div></div>'
  h += `<div class="rs-block"><b>Website / press-kit phrases</b> <span class="rs-dim">→ meta description, headings, copy</span><div class="rs-phrases">`
    + (data.phrases || []).map(p => `<div class="rs-phrase">${escHtml(p.phrase)} <span class="rs-dim">·${p.demand}</span></div>`).join('') + '</div></div>'
  return h
}
function renderSuggest(data) {
  if (!asoResults) return
  asoResults.innerHTML = suggestHtml(data)
  const cc = document.getElementById('comp-cands')   // Apps-lab only: feed the compose box (app-scope)
  if (cc && (data.candidates || []).length) cc.value = data.candidates.join(', ')
}
async function runSuggest(terms, cc) {
  if (!window.studio?.asoSuggest) { showToast('suggest requires the desktop app  (npm start)', 3000); return }
  if (!terms.trim()) { document.getElementById('sug-terms')?.focus(); return }
  asoOut.textContent = ''; if (asoResults) asoResults.innerHTML = ''
  const stop = busyDots(sugRun, 'suggesting', 'suggest'); if (sugRun) sugRun.disabled = true
  const res = await window.studio.asoSuggest(terms, cc || 'us')
  stop(); if (sugRun) sugRun.disabled = false
  if (res?.ok && res.data) renderSuggest(res.data)
  else if (asoResults) asoResults.innerHTML = `<div class="rs-err">${escHtml((res && res.error) || 'suggest failed')}</div>`
}
sugRun?.addEventListener('click', () => runSuggest(document.getElementById('sug-terms').value, document.getElementById('sug-cc')?.value))
document.getElementById('sug-terms')?.addEventListener('keydown', e => { if (e.key === 'Enter') sugRun?.click() })

// ── Keyword research popup — the SECOND scope-parameterized shared popup (after the trailer) ──
// Runs the scope-neutral aso-research (competition) + aso-suggest (demand) for a subject, seeded
// from its de:meta (cart) or listing (app). Reuses researchHtml/suggestHtml. Opened from the Promote
// tab (cart) and the Apps card (app). docs/design/editor-scopes-and-facets.md (shared-popup pattern).
async function openKeywords(subject) {
  const kind = (subject && typeof subject === 'object' && subject.kind) || 'app'
  const name = (subject && typeof subject === 'object') ? subject.name : subject
  document.getElementById('kw-subject').textContent = `${kind} · ${name}`
  const results = document.getElementById('kw-results'); if (results) results.innerHTML = ''
  let seed = ''   // seed the terms box from the subject's own words — edit freely
  if (kind === 'cart') {
    try {
      const idx = await fetch('/carts/index.json').then(r => r.json())
      const e = (idx || []).find(c => (c.file || '') === `${name}.cart.png`)
      // genre + kind are the market-facing descriptors; teaches are internal technique names (noise here)
      if (e) seed = [e.genre, ...(e.kind || [])].filter(Boolean).join(', ')
    } catch {}
  } else {
    const L = (appListings[name] && appListings[name].listing) || {}
    seed = L.keywords || [L.title, L.subtitle].filter(Boolean).join(', ')
  }
  const termsEl = document.getElementById('kw-terms'); if (termsEl) termsEl.value = seed
  document.getElementById('keywords-modal').hidden = false
  termsEl?.focus()
}
async function kwRun(tool) {
  const results = document.getElementById('kw-results')
  const terms = document.getElementById('kw-terms')?.value || ''
  const cc = document.getElementById('kw-cc')?.value || 'us'
  if (!terms.trim()) { document.getElementById('kw-terms')?.focus(); return }
  const fn = tool === 'research' ? window.studio?.asoResearch : window.studio?.asoSuggest
  if (!fn) { showToast('keyword research requires the desktop app  (npm start)', 3000); return }
  const btn = document.getElementById(tool === 'research' ? 'kw-research' : 'kw-suggest')
  const stop = busyDots(btn, tool === 'research' ? 'researching' : 'suggesting', tool); if (btn) btn.disabled = true
  if (results) results.innerHTML = '<div class="rs-dim">working…</div>'
  const res = await fn(terms, cc)
  stop(); if (btn) btn.disabled = false
  if (res?.ok && res.data) results.innerHTML = tool === 'research' ? researchHtml(res.data) : suggestHtml(res.data)
  else if (results) results.innerHTML = `<div class="rs-err">${escHtml((res && res.error) || (tool + ' failed'))}</div>`
}
document.getElementById('kw-research')?.addEventListener('click', () => kwRun('research'))
document.getElementById('kw-suggest')?.addEventListener('click', () => kwRun('suggest'))
document.getElementById('kw-terms')?.addEventListener('keydown', e => { if (e.key === 'Enter') kwRun('research') })
document.getElementById('kw-close')?.addEventListener('click', () => { document.getElementById('keywords-modal').hidden = true })
// popup result clicks: open a store link, or a mined chip re-runs research within the popup
document.getElementById('kw-results')?.addEventListener('click', e => {
  const link = e.target.closest('a[data-url]')
  if (link) { e.preventDefault(); window.studio?.openExternal?.(link.dataset.url); return }
  const chip = e.target.closest('a[data-term]')
  if (chip) { e.preventDefault(); const t = document.getElementById('kw-terms'); if (t) t.value = chip.dataset.term; kwRun('research') }
})

// score — render aso-score's scorecard WITH its gotchas inline (a bare number in a GUI reads as
// gospel; the caveats have to travel with it). Committed listing only; the A/B tweak loop is CLI.
function renderScore(data) {
  if (!asoResults) return
  const s = data.committed || {}, ev = s.ev || {}, L = (s.listing) || {}
  const band = v => v == null ? 'na' : v >= 67 ? 'good' : v >= 34 ? 'mid' : 'bad'
  const bar = (label, v, note) => `<div class="sc-row"><span class="sc-lab">${label}</span>`
    + (v == null
      ? `<span class="sc-na">n/a</span><span class="sc-note">${note}</span>`
      : `<span class="sc-track"><span class="sc-fill sc-${band(v)}" style="width:${v}%"></span></span><span class="sc-val">${v}</span>`)
    + `</div>`
  const chips = (arr, cls) => (arr || []).map(t => `<span class="rs-chip ${cls || ''}">${escHtml(typeof t === 'string' ? t : `${t.term}·${t.score}`)}</span>`).join(' ')
  const hard = (ev.terms || []).filter(t => t.band === 'HARD')
  const easy = (ev.terms || []).filter(t => t.band === 'EASY')
  let h = `<div class="sc-head"><b>score — ${escHtml(data.app || '')}</b> <span class="rs-dim">committed listing · --deep</span></div>`
  h += `<div class="sc-fields">`
    + `<div>title <span class="rs-dim">${L.title?.length ?? 0}/30</span></div>`
    + `<div>subtitle <span class="rs-dim">${L.subtitle?.length ?? 0}/30</span></div>`
    + `<div>keywords <span class="rs-dim">${L.keywords?.length ?? 0}/100</span></div></div>`
  h += bar('overall', s.overall) + bar('reach', s.reach, 'run 📝 worksheet') + bar('winnability', s.winnability, '')
    + bar('hygiene', s.hygiene) + bar('budget', s.budget)
  h += `<div class="sc-why">`
  if (hard.length) h += `<div>⚠ HARD keywords <span class="rs-dim">(crowded — a new app won't rank; drop unless core)</span><div>${chips(hard, 'rs-hard')}</div></div>`
  if (easy.length) h += `<div>✓ EASY keywords <span class="rs-dim">(winnable — lean here)</span><div>${chips(easy.map(t => t.term))}</div></div>`
  if (ev.repeats?.length) h += `<div>⚠ repeated in title/subtitle + keywords (ranks once): ${escHtml([...new Set(ev.repeats)].join(', '))}</div>`
  if (ev.missed?.length) h += `<div class="rs-dim">missing demand words: ${escHtml(ev.missed.slice(0, 14).join(', '))}${ev.missed.length > 14 ? ' …' : ''}</div>`
  h += `</div>`
  // the gotchas travel WITH the number — this is the honest part
  h += `<div class="sc-gotchas"><b>read this before trusting the number</b><ul>`
    + `<li><b>winnability &amp; hygiene</b> are the trustworthy signals; <b>reach is a soft nudge</b> (gameable — its word-list can carry noise, so read the missing list, don't chase the %).</li>`
    + `<li><b>relevance &amp; "does it read like a person"</b> are NOT scored — that stays your call and mine.</li>`
    + `<li>winnability is scored per <b>single word</b>, but Apple auto-combines your singles into phrases — a HARD single (<code>drum</code>) can still pull a winnable phrase (<code>drum machine</code>). Use bands as a lean, not a cut.</li>`
    + `<li>difficulty is a <b>relative proxy</b> (crowding × incumbent strength), not real search volume — and "HARD" is relative to app authority: bad for a new app, fine once you rank. The real number is your own App Store data post-launch.</li>`
    + `<li>iterate tweaks in the terminal: <code>node tools/aso-score.js ${escHtml(data.app || '')} --deep --keywords "…"</code> (A/B vs committed, with deltas).</li>`
    + `</ul></div>`
  asoResults.innerHTML = h
}
async function runScore(app) {
  if (!window.studio?.asoScore) { showToast('score requires the desktop app  (npm start)', 3000); return }
  asoOut.textContent = ''; if (asoResults) asoResults.innerHTML = ''
  document.getElementById('aso-terms')?.scrollIntoView({ behavior: 'smooth', block: 'center' })
  const res = await window.studio.asoScore(app)
  if (res?.ok && res.data) renderScore(res.data)
  else if (asoResults) asoResults.innerHTML = `<div class="rs-err">${escHtml((res && res.error) || 'score failed')}</div>`
}

// App Store metadata push — the ☁︎ two-click ceremony. runStore = click 1 (dry-run → checklist);
// the panel's push button = click 2 (writes ONLY the ticked fields live). Metadata-only for now.
async function runStore(app) {
  if (!window.studio?.ascMetadata) { showToast('App Store push requires the desktop app  (npm start)', 3000); return }
  asoOut.textContent = ''; if (asoResults) asoResults.innerHTML = '<div class="rs-dim">checking the live App Store listing…</div>'
  asoResults?.scrollIntoView({ behavior: 'smooth', block: 'center' })
  // two channels, one panel: listing copy (metadata) + promoted purchases. Both dry-run (read-only).
  const [meta, promo] = await Promise.all([
    window.studio.ascMetadata(app),
    window.studio.ascPromote ? window.studio.ascPromote(app) : Promise.resolve(null),
  ])
  if (!meta?.ok) { if (asoResults) asoResults.innerHTML = `<div class="rs-err">${escHtml(meta?.error || 'could not reach App Store')}</div>`; return }
  renderStorePanel(meta.data, promo, app)
}
// The ☁︎ App Store panel: two independent sections, each its own tick-then-push ceremony —
// "listing copy" (metadata fields) and "promoted purchases" (IAPs surfaced in App Store search).
function renderStorePanel(plan, promoRes, app) {
  if (!asoResults) return
  const clip = s => { s = String(s || ''); return s.length > 60 ? s.slice(0, 60) + '…' : s }
  const notEditable = !plan.editable
  const allFields = (plan.groups || []).flatMap(g => g.fields)
  const changed = allFields.filter(f => f.changed)
  const same = allFields.filter(f => !f.changed)

  let h = `<div class="sc-head"><b>App Store — ${escHtml(plan.app.name)}</b> `
    + `<span class="rs-dim">v${escHtml(plan.version.versionString)} · ${escHtml(plan.version.state)} · ${escHtml(plan.version.locale)}</span></div>`
  if (notEditable) h += `<div class="rs-err">version is ${escHtml(plan.version.state)} — not editable; can't push now</div>`

  // ── section 1: listing copy (metadata) ──
  h += `<div class="store-sec">listing copy</div>`
  if (!changed.length) {
    h += `<div class="store-sync">✓ listing in sync — nothing to push</div>`
  } else {
    h += `<div class="store-hint">tick fields to push — writes to the <b>live</b> listing</div><div class="store-list">`
    for (const f of changed) {
      const over = f.limit && f.local.length > f.limit
      h += `<label class="store-row"><input type="checkbox" class="store-f" value="${escHtml(f.field)}" checked>`
        + `<span class="store-fn">${escHtml(f.field)}</span>`
        + `<span class="store-cc${over ? ' app-over' : ''}">${f.local.length}${f.limit ? '/' + f.limit : ''}</span>`
        + `<div class="store-diff"><span class="store-live">${f.live ? escHtml(clip(f.live)) : '<em>empty</em>'}</span>`
        + `<span class="store-arrow">→</span><span class="store-local">${escHtml(clip(f.local))}</span></div></label>`
    }
    h += `</div>`
    if (same.length) h += `<div class="store-same">in sync: ${same.map(f => escHtml(f.field)).join(', ')}</div>`
    h += `<div class="store-foot"><button class="store-push"${notEditable ? ' disabled' : ''}>☁︎ Push <span class="store-n">${changed.length}</span> fields → live</button>`
      + `<span class="rs-dim">click 2 of 2</span></div>`
  }

  // ── section 2: promoted purchases (IAPs in App Store search — NOT the editor Promote tab) ──
  const promo = promoRes?.ok ? promoRes.data : null
  if (promoRes && !promoRes.ok) {
    h += `<div class="store-sec">promoted purchases</div><div class="rs-dim">${escHtml(promoRes.error || 'unavailable')}</div>`
  } else if (promo) {
    const prods = promo.products || []
    const toPromote = prods.filter(p => p.iapExists && !p.promoted)
    h += `<div class="store-sec">promoted purchases <span class="rs-dim">— IAPs as tappable App Store search results</span></div>`
    if (!prods.length) {
      h += `<div class="store-same">none promotable (all have "promote": false)</div>`
    } else {
      h += `<div class="store-list">`
      for (const p of prods) {
        const status = !p.iapExists ? '<span class="rs-err">no IAP yet — run --iap</span>'
          : p.promoted ? `<span class="store-ok">✓ promoted${p.state ? ' (' + escHtml(p.state) + ')' : ''}</span>`
          : '<span class="store-todo">not promoted</span>'
        const canTick = p.iapExists && !p.promoted
        h += `<label class="store-row${canTick ? '' : ' store-row-off'}">`
          + `<input type="checkbox" class="store-p" value="${escHtml(p.productId)}"${canTick ? ' checked' : ' disabled'}>`
          + `<span class="store-fn">${escHtml(p.name)}</span> ${status}`
          + `<div class="store-diff"><span class="rs-dim">${escHtml(p.productId)}</span></div></label>`
      }
      h += `</div>`
      if (toPromote.length) {
        h += `<div class="store-foot"><button class="store-promote"${notEditable ? ' disabled' : ''}>★ Promote <span class="store-pn">${toPromote.length}</span> → live</button>`
          + `<span class="rs-dim">needs the app live + Store.swift PurchaseIntent to be tappable</span></div>`
      } else {
        h += `<div class="store-sync">✓ all promotable IAPs already promoted</div>`
      }
    }
  }

  asoResults.innerHTML = h

  // wire section 1 (metadata)
  const mBoxes = () => [...asoResults.querySelectorAll('.store-f')]
  const mPush = asoResults.querySelector('.store-push'), mN = asoResults.querySelector('.store-n')
  const mRefresh = () => { const n = mBoxes().filter(b => b.checked).length; if (mN) mN.textContent = n; if (mPush) mPush.disabled = notEditable || n === 0 }
  mBoxes().forEach(b => b.addEventListener('change', mRefresh)); mRefresh()
  mPush?.addEventListener('click', async () => {
    const sel = mBoxes().filter(b => b.checked).map(b => b.value); if (!sel.length) return
    mPush.disabled = true; mPush.textContent = 'pushing…'
    const r = await window.studio.ascMetadata(app, { push: sel })
    if (!r?.ok) { mPush.disabled = false; mPush.textContent = '☁︎ Push fields → live'; showToast(r?.error || 'push failed', 4000); return }
    showToast(`pushed to App Store: ${(r.data?.pushed || sel).join(', ')}`, 3500)
    runStore(app)
  })

  // wire section 2 (promoted purchases)
  const pBoxes = () => [...asoResults.querySelectorAll('.store-p:not([disabled])')]
  const pPush = asoResults.querySelector('.store-promote'), pN = asoResults.querySelector('.store-pn')
  const pRefresh = () => { const n = pBoxes().filter(b => b.checked).length; if (pN) pN.textContent = n; if (pPush) pPush.disabled = notEditable || n === 0 }
  pBoxes().forEach(b => b.addEventListener('change', pRefresh)); pRefresh()
  pPush?.addEventListener('click', async () => {
    const sel = pBoxes().filter(b => b.checked).map(b => b.value); if (!sel.length) return
    pPush.disabled = true; pPush.textContent = 'promoting…'
    const r = await window.studio.ascPromote(app, { push: sel })
    if (!r?.ok) { pPush.disabled = false; pPush.textContent = '★ Promote → live'; showToast(r?.error || 'promote failed', 4000); return }
    showToast(`promoted: ${(r.data?.pushed || sel).join(', ')}`, 3500)
    runStore(app)
  })
}

// leads — the demand-GENERATION glance (twin of score's capture glance). Per cart of the app:
// its tribes + venues (clickable → open in browser) + a ready-to-copy post scaffold. We prep;
// the maker does the actual posting (copy the draft, open the venue, paste — "leave it to us").
const venueIcon = { reddit: '🔺', facebook: '📘', forum: '💬', youtube: '▶', tiktok: '🎵', web: '🌐', discord: '💬' }
// build the "find tribes" HTML from a leads payload. Shared by the app-scoped Apps glance
// (asoResults) and the per-cart Promote tab (#promote-leads) — same payload shape, both render
// this. data.app present → app scope (header names it); absent → cart scope (payload has one cart).
function leadsHtml(data) {
  const vlink = v => `<div class="ld-venue">${venueIcon[v.kind] || '·'} `
    + (v.url ? `<a href="#" data-url="${escHtml(v.url)}">${escHtml(v.name)}</a>` : escHtml(v.name))
    + (v.etiquette ? ` <span class="rs-dim">— ${escHtml(v.etiquette)}</span>` : '') + `</div>`
  let h = `<div class="sc-head"><b>find tribes${data.app ? ` — ${escHtml(data.app)}` : ''}</b> <span class="rs-dim">where to gift-post ${data.app ? 'each cart' : 'this cart'} · we prep, you post</span></div>`
  for (const c of (data.carts || [])) {
    if (!c || !c.ok) { h += `<div class="ld-cart"><b>${escHtml(c?.cart || '?')}</b> <span class="rs-dim">— ${escHtml(c?.error || 'no match')}</span></div>`; continue }
    h += `<div class="ld-cart"><div class="ld-title"><b>${escHtml(c.title || c.cart)}</b> <span class="rs-dim">${escHtml(c.domain)} · ${c.tribes.length} tribe${c.tribes.length === 1 ? '' : 's'}</span></div>`
    if (!c.tribes.length) h += `<div class="rs-dim">no tribe matched — try <code>node tools/leads.js discover ${escHtml(c.cart)} "your genre"</code></div>`
    for (const t of c.tribes) {
      h += `<div class="ld-tribe"><div class="ld-tlab">${escHtml(t.label)} `
        + t.matched.map(m => `<span class="rs-chip">${escHtml(m)}</span>`).join(' ') + `</div>`
      if (t.hook) h += `<div class="ld-hook">“${escHtml(t.hook)}”</div>`
      h += t.venues.map(vlink).join('') + `</div>`
    }
    if (c.draft) {
      h += `<details class="ld-draft"><summary>📝 post scaffold → ${escHtml(c.draft.tribe)} <span class="rs-dim">(your voice fills the [brackets])</span></summary>`
        + `<button class="ld-copy" data-copy="${escHtml(c.draft.body)}">copy</button>`
        + `<pre class="ld-body">${escHtml(c.draft.body)}</pre></details>`
    }
    h += `</div>`
  }
  if (data.crosscutting && data.crosscutting.length) {
    h += `<div class="ld-cart"><div class="ld-title"><b>Cross-cutting</b> <span class="rs-dim">— every music launch also posts here</span></div>`
      + data.crosscutting.map(vlink).join('') + `</div>`
  }
  h += `<div class="sc-gotchas"><b>before you post</b><ul>`
    + `<li>venue URLs are <b>candidates to browser-check</b> (Reddit blocks automated lookup) — confirm the room's still active + right before posting.</li>`
    + `<li><b>gift-first, always</b>: you're a member there first. The scaffold uses the cart's OWN words + the tribe hook; your voice fills the [brackets]. Never a copy-paste spam.</li>`
    + `<li>log it after: <code>node tools/leads.js track add &lt;cart&gt; "&lt;venue&gt;" --status posted --url &lt;link&gt;</code></li>`
    + `</ul></div>`
  return h
}
function renderLeads(data) {
  if (!asoResults) return
  asoResults.innerHTML = leadsHtml(data)
}
async function runLeads(app) {
  if (!window.studio?.leads) { showToast('find tribes requires the desktop app  (npm start)', 3000); return }
  asoOut.textContent = ''; if (asoResults) asoResults.innerHTML = ''
  document.getElementById('aso-lab')?.scrollIntoView({ behavior: 'smooth', block: 'start' })
  const res = await window.studio.leads(app)
  if (res?.ok) renderLeads(res)
  else if (asoResults) asoResults.innerHTML = `<div class="rs-err">${escHtml((res && res.error) || 'find tribes failed')}</div>`
}

// ── Promote tab (per-cart): clips/takes (A) + find tribes (D) + gallery link (E) ──
// The per-cart Promote verb. Reuses built pieces — leadsHtml() for tribes, recordCart()/
// studio.replay for takes, the deterministic gallery URL for the link. Rendered on tab open
// (mirrors renderAppsList). Scope = the currently-open cart (currentCartFile).
// Design: docs/design/promote-tab.md.
async function renderPromote() {
  const empty = document.getElementById('promote-empty')
  const main  = document.getElementById('promote-main')
  const cart  = currentCartFile
  if (!cart) { if (empty) empty.hidden = false; if (main) main.hidden = true; return }
  if (empty) empty.hidden = true; if (main) main.hidden = false
  const nameEl = document.getElementById('promote-cart')
  if (nameEl) nameEl.textContent = currentCartName || cart
  const clipsEl = document.getElementById('promote-clips')
  if (!window.studio?.cartClips) {
    if (clipsEl) clipsEl.innerHTML = `<div class="apps-empty">the Promote tab needs the desktop app  (npm start)</div>`
    return
  }
  // A · clips & takes — click a take to WATCH it (▶), open its baked clip (🎬)
  const cc = await window.studio.cartClips(cart)
  if (clipsEl) {
    if (!cc?.ok) clipsEl.innerHTML = `<div class="rs-err">${escHtml(cc?.error || 'could not read clips')}</div>`
    else if (!cc.clips.length) clipsEl.innerHTML = `<div class="apps-empty">no takes or clips yet — record one below (or <code>node tools/play.js ${escHtml(cart)} record</code>)</div>`
    else clipsEl.innerHTML = cc.clips.map(cl => {
      // every take format plays: .rec/.script run against the live editor code, .beats via play.js
      // (the on-disk cart). playKind/playPath come from the IPC; a bare baked clip has no take file.
      const label = cl.playPath
        ? `<a href="#" class="pm-label pm-replay" data-play-path="${escHtml(cl.playPath)}" data-play-kind="${escHtml(cl.playKind)}" title="watch — play this ${escHtml(cl.playKind)} take${cl.playKind === 'beats' ? ' (runs the on-disk cart)' : ' against the open cart'}">▶ ${escHtml(cl.label)}</a>`
        : `<span class="pm-label pm-noplay" title="baked clip only — no take file to replay">${escHtml(cl.label)}</span>`
      // native clip → open it; a recipe → 🎬 bake (at the "bake at" ratio above: native or a variant);
      // variant chips show which per-ratio versions are already baked (export-ratios Stage 2).
      const clipLink = cl.baked ? `<a href="#" class="pm-clip" data-url="${escHtml(location.origin + cl.clipUrl)}" title="open the native clip">🎬 clip</a>` : ''
      const bakeBtn  = cl.playPath ? `<button class="pm-bake" data-bake="${escHtml(cl.label)}" title="bake this take at the output ratio selected above">🎬 bake</button>` : ''
      const notBaked = (!cl.baked && !cl.playPath) ? `<span class="rs-dim">not baked</span>` : ''
      // each baked variant is a STANDALONE export (single-clip → a post) — click to open/save it
      const varChips = (cl.variants || []).map(v => `<a href="#" class="pm-var" data-url="${escHtml(location.origin + `/clips/${cart}/${cl.label}--${v}.webm`)}" title="open the ${escHtml(v)} export — save it for a post">${escHtml(ratioLabel(v))} ↗</a>`).join(' ')
      return `<div class="pm-take">${label} <span class="rs-dim">${escHtml(cl.kinds.join('/'))}</span> ${clipLink} ${bakeBtn} ${notBaked} ${varChips}</div>`
    }).join('')
  }
  // B · stills — a per-cart gallery; click a thumb to open it full size
  const shotsEl = document.getElementById('promote-shots')
  if (shotsEl) {
    shotsEl.innerHTML = (cc?.shots && cc.shots.length)
      ? cc.shots.map(s => `<a href="#" class="pm-shot" data-url="${escHtml(location.origin + s)}" title="open full size"><img src="${escHtml(s)}" loading="lazy"></a>`).join('')
      : `<div class="apps-empty">no stills yet — 📸 snapshot the cart below</div>`
  }
  // E · the link — the deterministic gallery URL + copy (published-state dot is v2)
  const linkEl = document.getElementById('promote-link')
  if (linkEl && cc?.url) {
    linkEl.innerHTML = `<div class="pm-link"><a href="#" data-url="${escHtml(cc.url)}">${escHtml(cc.url)}</a>`
      + ` <button class="ld-copy" data-copy="${escHtml(cc.url)}">copy</button></div>`
      + `<div class="rs-dim">the gallery URL once this cart is published — if it isn't live yet, publish from <b>⇪ share</b> (Ship) first.</div>`
  }
  // D · find tribes — same payload/render as the Apps glance, aimed at one cart
  const leadsEl = document.getElementById('promote-leads')
  if (leadsEl && window.studio?.cartLeads) {
    leadsEl.innerHTML = `<div class="rs-dim">matching tribes…</div>`
    const res = await window.studio.cartLeads(cart)
    leadsEl.innerHTML = res?.ok ? leadsHtml(res) : `<div class="rs-err">${escHtml(res?.error || 'find tribes failed')}</div>`
  }
}
// watch a take: .rec/.script replay against the live editor code; .beats plays via play.js.
async function playTake(playPath, playKind) {
  if (playKind === 'beats') {
    if (!window.studio?.playBeats) { showToast('play requires the desktop app  (npm start)', 3000); return }
    const res = await window.studio.playBeats(currentCartFile, playPath)
    if (res && !res.ok) showToast(res.output || 'play failed', 3500)
    return
  }
  if (!window.studio?.replay) { showToast('replay requires the desktop app  (npm start)', 3000); return }
  const code = view.state.doc.toString()
  const res = await window.studio.replay(code, { ...settings, cartName: currentCartName, cartFile: currentCartFile }, playPath)
  if (res && !res.ok) showLog(res)
}
// friendly labels for the per-ratio output sizes (shared by the bake picker + the variant chips)
const RATIO_LABEL = { '1280x720': '16:9', '720x1280': '9:16', '1080x1080': '1:1', '886x1920': 'iPhone', '1920x886': 'iPhone L', '1200x1600': 'iPad' }
const ratioLabel = v => RATIO_LABEL[v] || v
// bake a take → a clip (webm) at the "bake at" ratio (native, or a per-ratio variant), then re-render.
async function bakeTake(label, btn) {
  if (!window.studio?.bakeClip) { showToast('bake requires the desktop app  (npm start)', 3000); return }
  const size = document.getElementById('promote-bake-ratio')?.value || null   // '' = native
  if (btn) { btn.textContent = '⏳ baking…'; btn.disabled = true }
  const res = await window.studio.bakeClip(currentCartFile, label, size)
  if (res?.ok) { showToast(`✓ ${res.mode === 'letterbox' ? 'letterboxed (bars)' : 'baked'}${size ? ' @ ' + ratioLabel(size) : ''}`, 2500); renderPromote() }
  else { showToast(res?.output || 'bake failed — see the log', 4000); if (btn) { btn.textContent = '🎬 bake'; btn.disabled = false } }
}
// delegated clicks in the Promote panel: data-url / data-copy (same as the Apps glance),
// ▶ watch a take, 🎬 bake a take into a clip.
document.getElementById('promote-body')?.addEventListener('click', e => {
  const link = e.target.closest('a[data-url]')
  if (link) { e.preventDefault(); window.studio?.openExternal?.(link.dataset.url); return }
  const cp = e.target.closest('[data-copy]')
  if (cp) { e.preventDefault(); navigator.clipboard?.writeText(cp.dataset.copy).then(() => showToast('copied', 2000)); return }
  const rp = e.target.closest('[data-play-path]')
  if (rp) { e.preventDefault(); playTake(rp.dataset.playPath, rp.dataset.playKind); return }
  const bk = e.target.closest('[data-bake]')
  if (bk) { e.preventDefault(); bakeTake(bk.dataset.bake, bk); return }
})
document.getElementById('promote-rec')?.addEventListener('click', () => recordCart())
document.getElementById('promote-snap')?.addEventListener('click', async (e) => {
  const btn = e.currentTarget
  if (!window.studio?.cartShot) { showToast('snapshot requires the desktop app  (npm start)', 3000); return }
  if (!currentCartFile) { showToast('open a cart first', 2500); return }
  btn.textContent = '⏳ capturing…'; btn.disabled = true
  const res = await window.studio.cartShot(currentCartFile)
  btn.textContent = '📸 snapshot'; btn.disabled = false
  if (res?.ok) { showToast('✓ still captured', 2000); renderPromote() }
  else showToast(res?.output || 'snapshot failed — see the log', 4000)
})
document.getElementById('promote-trailer')?.addEventListener('click', () => {
  if (!currentCartFile) { showToast('open a cart first', 2500); return }
  openTrailer({ kind: 'cart', name: currentCartFile })
})
document.getElementById('promote-keywords')?.addEventListener('click', () => {
  if (!currentCartFile) { showToast('open a cart first', 2500); return }
  openKeywords({ kind: 'cart', name: currentCartFile })
})
document.querySelector('.tab[data-tab="promote"]')?.addEventListener('click', renderPromote)

const asoVal = id => (document.getElementById(id)?.value || '')
const lintRun = document.getElementById('lint-run')
lintRun?.addEventListener('click', async () => {
  if (!window.studio?.asoLint) { showToast('requires the desktop app  (npm start)', 3000); return }
  const f = { title: asoVal('lint-title'), subtitle: asoVal('lint-sub'), keywords: asoVal('lint-kw') }
  if (!f.title && !f.subtitle && !f.keywords) return
  asoOut.textContent = ''; if (asoResults) asoResults.innerHTML = ''
  const stop = busyDots(lintRun, 'linting', 'lint'); lintRun.disabled = true
  await window.studio.asoLint(f); stop(); lintRun.disabled = false
})
const compRun = document.getElementById('comp-run')
compRun?.addEventListener('click', async () => {
  if (!window.studio?.asoCompose) { showToast('requires the desktop app  (npm start)', 3000); return }
  const f = { title: asoVal('comp-title'), subtitle: asoVal('comp-sub'), candidates: asoVal('comp-cands') }
  if (!f.candidates.trim()) { document.getElementById('comp-cands')?.focus(); return }
  asoOut.textContent = ''; if (asoResults) asoResults.innerHTML = ''
  const stop = busyDots(compRun, 'composing', 'compose'); compRun.disabled = true
  await window.studio.asoCompose(f); stop(); compRun.disabled = false
})

// ── trailer builder (docs/design/trailer-builder.md) — the click-to-edit v1 (A) ──────────
// A thin, NON-DESTRUCTIVE editor over tools/reels/<app>.reel: pick clips, order them, choose the
// transition at each join, Build → bake+compose → preview. Sources are never touched.
const TL_XFADES = ['fade', 'dissolve', 'wipeleft', 'wiperight', 'wipeup', 'wipedown',
  'slideleft', 'slideright', 'circleopen', 'circleclose', 'pixelize', 'radial']
const TL_ANIMS  = ['fade', 'slide bottom', 'slide top', 'slide left', 'slide right']   // text-card entrances
const TL_ROLES  = ['title', 'sub', 'body']                                             // sized content lines
const TL_PAL = ['#000000','#1d2b53','#7e2553','#008751','#ab5236','#5f574f','#c2c3c7','#fff1e8',   // pico32, for bg swatches
  '#ff004d','#ffa300','#ffec27','#00e436','#29adff','#83769c','#ff77a8','#ffccaa',
  '#291814','#111d35','#422136','#125359','#742f29','#49333b','#a28879','#f3ef7d',
  '#be1250','#ff6c24','#a8e72e','#00b543','#065ab5','#754665','#ff6e59','#ff9d81']
const paletteHex = i => TL_PAL[((i | 0) % 32 + 32) % 32]
// rows are either a clip {clip, xtype, xdur, trim, speed} or a text card
// {card:true, dur, lines:[{role,text}], anim, bg, xtype, xdur} — the timeline = the .reel
let tlApp = null, tlLib = [], tlRows = []
let tlSubject = null   // the SUBJECT reels group under (cart/app name) — the saved-reels list is scoped
                       // to it: <subject>.reel + <subject>--<variant>.reel. Distinct from tlApp (the
                       // build target). Standalone reels (demo/teaser) belong to no subject → not shown here.
let tlLoop = null   // {type, dur} — seamless loop-close back to the start (null = off)
let tlBaked = new Set()   // clips with a baked .webm (scrubbable / probeable)
let tlDur = {}            // clip → source duration (s), probed client-side from the <video> (no ffprobe)
let tlFocus = -1          // which clip block the preview monitor is showing
let tlSel = -1            // which block is selected → its properties fill the inspector panel
let tlPps = 1             // current timeline scale (pixels per second) — set each render, read by the overlap drag
let tlOvSel = null        // {r, o} — a selected text OVERLAY (clip r, overlay o); its fields fill the inspector
const TL_OVPOS = ['top', 'center', 'bottom']   // where the overlay text sits (all the titlecard cart supports)
const round1 = x => Math.round(x * 10) / 10
const tlSrc = clip => `/clips/${clip}.webm`   // Vite serves editor/public at root
// The trailer builder is scope-parameterized: openTrailer({kind:'app'|'cart', name}) — the Apps card
// passes an app, the Promote tab passes the open cart. Same modal, same tl* machinery; only the clip
// SOURCE differs (app → appClips across its carts; cart → cartClips for one). A string arg = an app
// (back-compat). build-reel writes tools/reels/<name>.reel either way (cart/app share that namespace).
async function openTrailer(subject) {
  tlSeqStop()
  const kind = (subject && typeof subject === 'object' && subject.kind) || 'app'
  const name = (subject && typeof subject === 'object') ? subject.name : subject
  let res
  if (kind === 'cart') {
    const cc = await window.studio?.cartClips?.(name)
    if (!cc?.ok) { showToast(cc?.error || 'trailer needs the desktop app  (npm start)', 3000); return }
    // adapt cart-clips → the library shape ([{cart, clips:[{label, clip, baked}]}]) the builder expects.
    // A per-cart trailer starts empty (no saved cart .reel pre-pop yet); Build writes tools/reels/<cart>.reel.
    const clips = (cc.clips || []).map(c => ({ label: c.label, clip: `${name}/${c.label}`, baked: c.baked }))
    if (!clips.length) { showToast('no clips yet — bake a take first (🎬 bake)', 3500); return }
    res = { ok: true, name, carts: [{ cart: name, clips }], rows: null, loop: null }
  } else {
    res = await window.studio?.appClips?.(name)
    if (!res?.ok) { showToast(res?.error || 'trailer needs the desktop app  (npm start)', 3000); return }
  }
  tlApp = name; tlSubject = name; tlLib = res.carts || []
  tlBaked = new Set(); for (const c of tlLib) for (const cl of c.clips) if (cl.baked) tlBaked.add(cl.clip)
  tlDur = {}; tlFocus = -1; tlSel = -1; tlOvSel = null
  // start from the saved .reel, else a default: each rack's first clip in manifest order
  tlRows = (res.rows && res.rows.length) ? res.rows.map(r => ({ ...r }))
    : tlLib.filter(c => c.clips.length).map(c => ({ clip: c.clips[0].clip, xtype: 'fade', xdur: 0.5, trim: null, speed: 1 }))
  tlLoop = res.loop || null
  document.getElementById('tl-app').textContent = `${kind === 'cart' ? 'cart · ' : ''}${res.name || name}`
  { const rsel = document.getElementById('tl-ratio'); if (rsel) rsel.value = res.size || '' }   // reflect the reel's saved output size
  const prev = document.getElementById('tl-preview'); if (prev) { prev.pause?.(); prev.hidden = true; prev.removeAttribute('src') }
  const mon = document.getElementById('tl-monwrap'); if (mon) mon.hidden = false   // monitor + inspector sit side by side while the panel is open
  const tlLog = document.getElementById('tl-log'); if (tlLog) { tlLog.hidden = true; tlLog.textContent = '' }
  document.getElementById('trailer-modal').hidden = false
  tlRender()
  tlRenderReels()
}
// the "saved reels" strip: every tools/reels/*.reel, click to load its scenario into the builder.
// The current subject's reel is highlighted. docs/design/trailer-builder.md (save/load the scenario).
async function tlRenderReels() {
  const el = document.getElementById('tl-reels'); if (!el) return
  const res = await window.studio?.listReels?.()
  // scope to THIS subject: <subject>.reel + <subject>--<variant>.reel — not every reel in the repo
  // (the full cross-subject overview is a separate surface). Standalone reels belong to no subject.
  const reels = ((res && res.reels) || []).filter(r => tlSubject && (r.name === tlSubject || r.name.startsWith(tlSubject + '--')))
  if (!reels.length) { el.innerHTML = `<span class="rs-dim">no saved reels for ${escHtml(tlSubject || 'this')} yet — Build (or Save) writes one</span>`; return }
  el.innerHTML = reels.map(r =>
    `<a href="#" class="tl-reel${r.name === tlApp ? ' tl-reel-cur' : ''}" data-reel="${escHtml(r.name)}" title="load this scenario">${escHtml(r.name)}${r.hasWebm ? ' <span class="rs-dim">▶</span>' : ''}</a>`
  ).join('')
}
async function tlLoadReel(name) {
  const res = await window.studio?.reelLoad?.(name)
  if (!res?.ok) { showToast(res?.error || 'could not load reel', 3000); return }
  tlSeqStop()
  tlApp = name; tlLib = res.carts || []
  tlBaked = new Set(); for (const c of tlLib) for (const cl of c.clips) if (cl.baked) tlBaked.add(cl.clip)
  tlDur = {}; tlFocus = -1; tlSel = -1; tlOvSel = null
  tlRows = (res.rows || []).map(r => ({ ...r }))
  tlLoop = res.loop || null
  document.getElementById('tl-app').textContent = `reel · ${name}`
  { const rsel = document.getElementById('tl-ratio'); if (rsel) rsel.value = res.size || '' }   // reflect the reel's saved output size
  tlRender(); tlRenderReels()
}
document.getElementById('tl-reels')?.addEventListener('click', e => {
  const a = e.target.closest('[data-reel]'); if (!a) return
  e.preventDefault(); tlLoadReel(a.dataset.reel)
})
// open a saved reel directly in the trailer builder (from the cross-subject overview). Sets the
// subject from the reel name (<subject>--<variant> → <subject>) so the in-builder list stays scoped.
async function openReel(name) {
  tlSeqStop()
  tlSubject = String(name).split('--')[0]
  const prev = document.getElementById('tl-preview'); if (prev) { prev.pause?.(); prev.hidden = true; prev.removeAttribute('src') }
  const mon = document.getElementById('tl-monwrap'); if (mon) mon.hidden = false
  const tlLog = document.getElementById('tl-log'); if (tlLog) { tlLog.hidden = true; tlLog.textContent = '' }
  document.getElementById('trailer-modal').hidden = false
  await tlLoadReel(name)
}
// the cross-subject Reels overview (top of the Apps page) — every saved scenario, click to open it.
async function renderReelsOverview() {
  const el = document.getElementById('reels-all'); if (!el) return
  const res = await window.studio?.listReels?.()
  const reels = (res && res.reels) || []
  el.innerHTML = reels.length
    ? reels.map(r => `<a href="#" class="tl-reel" data-reel="${escHtml(r.name)}" title="open in the trailer builder">${escHtml(r.name)}${r.hasWebm ? ' <span class="rs-dim">▶</span>' : ''}</a>`).join('')
    : '<span class="rs-dim">no reels yet — build one in the trailer builder (Apps card or Promote tab)</span>'
}
document.getElementById('reels-all')?.addEventListener('click', e => {
  const a = e.target.closest('[data-reel]'); if (!a) return
  e.preventDefault(); openReel(a.dataset.reel)
})
document.querySelector('.tab[data-tab="apps"]')?.addEventListener('click', renderReelsOverview)
// probe durations for any baked clip we haven't measured yet — a hidden <video>, no ffprobe/IPC
function tlProbeDurations() {
  for (const clip of new Set(tlRows.map(r => r.clip))) {
    if (!tlBaked.has(clip) || tlDur[clip] !== undefined) continue
    tlDur[clip] = null   // in-flight (falsy → no track drawn yet, but won't re-probe)
    const v = document.createElement('video')
    v.preload = 'metadata'; v.muted = true; v.src = tlSrc(clip)
    v.addEventListener('loadedmetadata', () => { tlDur[clip] = v.duration || 0; tlRender() }, { once: true })
    v.addEventListener('error', () => { tlDur[clip] = 0 }, { once: true })
  }
}
// ── absolute px-per-second timeline — blocks positioned by time; the overlap between them IS the transition ──
// a row's on-screen length in seconds: card total, or the clip's trimmed length (3s placeholder until probed)
function tlRowDur(r) {
  if (r.card) return (r.waitBefore ?? 0) + (r.inDur ?? 0.5) + (r.holdDur ?? 1.5) + (r.outDur ?? 0) + (r.waitAfter ?? 0) || 1
  const dur = tlDur[r.clip]
  if (!dur) return 3
  return (r.trim ? (r.trim[1] - r.trim[0]) : dur) || 1
}
// one absolutely-positioned block. No ⧉/✕ here (they'd hide under an overlap) — those live in the inspector.
function tlBlock(r, i, L, pps) {
  const sel = i === tlSel ? ' tl-focus' : ''
  const style = `left:${(L.left * pps).toFixed(1)}px;width:${(L.dur * pps).toFixed(1)}px`
  if (r.card) {
    const bg = r.bg == null ? 17 : r.bg
    const first = (r.lines && r.lines[0] && r.lines[0].text) || 'card'
    return `<span class="tl-block tl-card${sel}" draggable="true" data-tl-block="${i}" style="${style}">`
      + `<span class="tl-lbl"><span class="tl-sw" style="background:${paletteHex(bg)}"></span> 📝 ${escHtml(first)}</span>`
      + `<span class="tl-dur">${L.dur.toFixed(1)}s</span></span>`
  }
  let bar
  if (!tlBaked.has(r.clip)) bar = `<span class="tl-dur rs-dim">·bake</span>`
  else if (!tlDur[r.clip]) bar = `<span class="tl-dur rs-dim">probing…</span>`
  else { const dur = tlDur[r.clip], inT = r.trim ? r.trim[0] : 0, outT = r.trim ? r.trim[1] : dur
    bar = `<span class="tl-minibar"><span class="tl-minirange" style="left:${inT / dur * 100}%;right:${100 - outT / dur * 100}%"></span></span>`
      + `<span class="tl-dur">${L.dur.toFixed(1)}s${r.speed && r.speed !== 1 ? ` ·${r.speed}×` : ''}</span>` }
  return `<span class="tl-block${sel}" draggable="true" data-tl-block="${i}" style="${style}">`
    + `<span class="tl-lbl">${escHtml(r.clip)}</span>${bar}</span>`
}
function tlRender() {
  const lib = document.getElementById('tl-library')
  lib.innerHTML = `<button class="kw-mini tl-addcard" data-tl-addcard title="add a text card (title / subtitle / body)">＋ text card</button> `
    + tlLib.map(c => c.clips.length
    ? c.clips.map(cl => `<button class="kw-mini" draggable="true" data-tl-add="${escHtml(cl.clip)}" title="click to append · drag onto the timeline to place">＋ ${escHtml(cl.clip)}${cl.baked ? '' : ' <span class="rs-dim">·bake</span>'}</button>`).join('')
    : `<span class="rs-dim">${escHtml(c.cart)}: no clip</span>`).join(' ')
  const tl = document.getElementById('tl-timeline')
  if (!tlRows.length) { tl.innerHTML = '<span class="rs-dim">empty — add clips or a text card from above</span>' }
  else {
    // lay out in TIME: block i starts at right[i-1] − xdur[i], so adjacent blocks overlap by the transition
    let cur = 0
    const lay = tlRows.map((r, i) => {
      const dur = tlRowDur(r)
      const prevRight = cur   // the right edge of block i-1 (0 for the first block)
      const xd = i === 0 ? 0 : Math.max(0, Math.min(r.xdur || 0, dur - 0.05, tlRowDur(tlRows[i - 1]) - 0.05))
      const left = i === 0 ? 0 : Math.max(0, prevRight - xd)
      cur = left + dur
      return { left, dur, xd, prevRight }
    })
    const totalSec = cur || 1
    const availW = (tl.clientWidth || 760) - 6
    const pps = Math.max(26, Math.min(130, availW / totalSec))   // fit to the panel, but stay legible (scrolls if long)
    tlPps = pps
    const px = s => (s * pps).toFixed(1)
    // main lane: clip/card blocks + the transition-overlap handles on the joins
    let main = ''
    lay.forEach((L, i) => { main += tlBlock(tlRows[i], i, L, pps) })
    lay.forEach((L, i) => { if (i > 0 && L.xd > 0.001)
      main += `<span class="tl-overlap" data-tl-ov="${i}" data-tl-pr="${L.prevRight}" title="${escHtml(tlRows[i].xtype)} ${(+tlRows[i].xdur).toFixed(1)}s — drag to change length; click to edit type" style="left:${px(L.left)}px;width:${px(L.xd)}px"><span>◇</span></span>` })
    // overlay lane: each clip's text overlays, drawn ABOVE the clip at its [a,b] window (in reel time)
    let ovl = ''
    lay.forEach((L, i) => { const r = tlRows[i]; if (r.card) return
      ;(r.overlays || []).forEach((ov, o) => {
        const a = Math.max(0, Math.min(ov.a || 0, L.dur)), b = Math.max(a + 0.1, Math.min(ov.b == null ? L.dur : ov.b, L.dur))
        const sc = tlOvSel && tlOvSel.r === i && tlOvSel.o === o ? ' tl-focus' : ''
        ovl += `<span class="tl-ovblk${sc}" data-tl-ovblk="${i},${o}" data-tl-cl="${L.left}" title="text overlay on ${escHtml(r.clip)} — ${a.toFixed(1)}–${b.toFixed(1)}s (drag to move, edges to resize)" style="left:${px(L.left + a)}px;width:${px(b - a)}px">`
          + `<span class="tl-ovh" data-tl-ovh="${i},${o},0" style="left:0"></span>`
          + `<span class="tl-ovlbl">▣ ${escHtml((ov.lines && ov.lines[0] && ov.lines[0].text) || 'text')}</span>`
          + `<span class="tl-ovh" data-tl-ovh="${i},${o},1" style="right:0"></span></span>`
      }) })
    tl.innerHTML = `<div class="tl-tracks"><div class="tl-ovlane" style="width:${px(totalSec)}px">${ovl}</div>`
      + `<div class="tl-track" style="width:${px(totalSec)}px">${main}</div></div>` + tlLoopControl()
  }
  tlRenderInspector()
  tlProbeDurations()
  tlUpdateOvPreview()   // reflect a selection/edit that didn't move the playhead
}
// ── inspector — the SELECTED block's full properties (transition-in · trim/speed · card lines/timing/look) ──
function tlRenderInspector() {
  const insp = document.getElementById('tl-inspector'); if (!insp) return
  if (tlOvSel) {   // a text overlay is selected → show ITS properties
    const R = tlRows[tlOvSel.r], ov = R && !R.card && R.overlays && R.overlays[tlOvSel.o]
    if (ov) { insp.innerHTML = tlOvInspector(R, ov, tlOvSel.r, tlOvSel.o); return }
    tlOvSel = null   // stale selection (the clip/overlay was removed) — fall through
  }
  if (tlSel < 0 || tlSel >= tlRows.length) { insp.innerHTML = '<div class="tl-inhint">click a block in the timeline to edit it</div>'; return }
  const r = tlRows[tlSel], i = tlSel, parts = []
  parts.push(`<div class="tl-inhead">${r.card ? '📝 text card' : escHtml(r.clip)}`
    + `<span class="tl-btns"><button data-tl-dup="${i}" title="duplicate this part">⧉</button>`
    + `<button data-tl-rm="${i}" title="remove this part">✕</button></span></div>`)
  if (i > 0) parts.push(`<div class="tl-inrow"><span class="tl-ink">transition in</span>`   // the cut that brings this block in
    + `<select data-tl-x="${i}">` + TL_XFADES.map(x => `<option${x === r.xtype ? ' selected' : ''}>${x}</option>`).join('') + `</select>`
    + `<input class="tl-secs" type="number" min="0.1" max="2" step="0.1" value="${r.xdur}" data-tl-d="${i}"></div>`)
  parts.push(r.card ? tlCardFields(r, i) : tlClipFields(r, i))
  insp.innerHTML = parts.join('')
}
function tlClipFields(r, i) {
  const dur = tlDur[r.clip]
  const inT = r.trim ? r.trim[0] : 0, outT = r.trim ? r.trim[1] : (dur || 0)
  let track = `<div class="tl-tinfo rs-dim">${tlBaked.has(r.clip) ? 'probing…' : '·bake this clip to trim it'}</div>`
  if (tlBaked.has(r.clip) && dur) {
    const inP = inT / dur * 100, outP = outT / dur * 100
    track = `<div class="tl-trim" data-tl-trim="${i}"><span class="tl-range" style="left:${inP}%;right:${100 - outP}%"></span>`
      + `<span class="tl-h" data-tl-h="${i},0" style="left:${inP}%" title="in"></span>`
      + `<span class="tl-h" data-tl-h="${i},1" style="left:${outP}%" title="out"></span></div>`
      + `<div class="tl-tinfo">${inT.toFixed(1)}→${outT.toFixed(1)}s of ${dur.toFixed(1)}s${r.speed && r.speed !== 1 ? ` · ${r.speed}×` : ''}</div>`
  }
  const speed = `<label class="tl-splbl" title="playback speed (× faster)">speed ×<input class="tl-num" type="number" min="0.1" step="0.25" placeholder="1" value="${r.speed && r.speed !== 1 ? r.speed : ''}" data-tl-sp="${i}"></label>`
  const ovs = (r.overlays || []).map((ov, o) => `<button class="kw-mini" data-tl-ovsel="${i},${o}" title="edit this overlay">▣ ${escHtml((ov.lines && ov.lines[0] && ov.lines[0].text) || 'text')}</button>`).join(' ')
  return `<div class="tl-ingroup"><span class="tl-ink">trim</span>${track}<div class="tl-inhint2">scrub the monitor + ⇤/⇥, or drag the handles</div></div>`
    + `<div class="tl-ingroup"><span class="tl-ink">speed</span>${speed}</div>`
    + `<div class="tl-ingroup"><span class="tl-ink">text overlays</span><div class="tl-cardopts">${ovs}`
    +   `<button class="tl-cadd" data-tl-ovadd="${i}" title="add a text overlay that rides on top of this clip">＋ text overlay</button></div>`
    +   `<div class="tl-inhint2">text keyed over the clip on Build (green is the transparent colour)</div></div>`
}
// the inspector for a selected text overlay — window · position · entrance · lines · boil/breathe/bpm
function tlOvInspector(r, ov, i, o) {
  const clipDur = tlRowDur(r)
  const a = ov.a || 0, b = ov.b == null ? clipDur : ov.b
  const roleSel = (role, k) => `<select class="tl-crole" data-tl-ovrole="${i},${o},${k}">`
    + TL_ROLES.map(x => `<option${x === role ? ' selected' : ''}>${x}</option>`).join('') + `</select>`
  const lines = (ov.lines || []).map((l, k) => `<div class="tl-cline">${roleSel(l.role, k)}`
    + `<input class="tl-ctext" data-tl-ovtext="${i},${o},${k}" value="${escHtml(l.text)}" placeholder="text">`
    + `<button class="tl-cx" data-tl-ovlinedel="${i},${o},${k}" title="remove line">✕</button></div>`).join('')
  const posSel = `<select data-tl-ovpos="${i},${o}">` + TL_OVPOS.map(p => `<option${p === (ov.pos || 'bottom') ? ' selected' : ''}>${p}</option>`).join('') + `</select>`
  const inDur = ov.inDur ?? 0.4, inEff = ov.inEffect || ov.anim || 'fade', outDur = ov.outDur ?? 0, outEff = ov.outEffect || 'slide top'
  const effSel = (val, attr) => `<select class="tl-canim" data-${attr}="${i},${o}">` + TL_ANIMS.map(x => `<option${x === val ? ' selected' : ''}>${x}</option>`).join('') + `</select>`
  const num = (attr, val, mn, mx, st) => `<input class="tl-num" type="number" min="${mn}"${mx != null ? ` max="${mx}"` : ''} step="${st}" data-${attr}="${i},${o}" value="${val}">`
  return `<div class="tl-inhead">▣ overlay on ${escHtml(r.clip)}`
    + `<span class="tl-btns"><button data-tl-ovrm="${i},${o}" title="remove this overlay">✕</button></span></div>`
    + `<div class="tl-ingroup"><span class="tl-ink">lines</span><div class="tl-cardlines">${lines}<button class="tl-cadd" data-tl-ovlineadd="${i},${o}">＋ line</button></div></div>`
    + `<div class="tl-ingroup"><span class="tl-ink">window (seconds into the clip, 0–${clipDur.toFixed(1)})</span><div class="tl-cardopts">`
    +   `<span class="tl-phase">start ${num('tl-ova', +a.toFixed(1), 0, clipDur, 0.1)}</span>`
    +   `<span class="tl-phase">end ${num('tl-ovb', +b.toFixed(1), 0, clipDur, 0.1)}</span></div></div>`
    + `<div class="tl-ingroup"><span class="tl-ink">position</span><div class="tl-cardopts"><span class="tl-phase">pos ${posSel}</span></div></div>`
    + `<div class="tl-ingroup"><span class="tl-ink">in / out transitions</span><div class="tl-cardopts">`
    +   `<span class="tl-phase" title="entrance — effect + seconds">in ${effSel(inEff, 'tl-ovineff')}${num('tl-ovindur', inDur, 0, null, 0.1)}</span>`
    +   `<span class="tl-phase" title="exit — effect + seconds (0 = no exit tween)">out ${effSel(outEff, 'tl-ovouteff')}${num('tl-ovoutdur', outDur, 0, null, 0.1)}</span></div></div>`
    + `<div class="tl-ingroup"><span class="tl-ink">life</span><div class="tl-cardopts">`
    +   `<label class="tl-splbl" title="boil — hand-drawn wobble (0…1)">boil<input class="tl-num" type="number" min="0" max="1" step="0.1" data-tl-ovboil="${i},${o}" value="${ov.boil ?? 0}"></label>`
    +   `<label class="tl-splbl" title="breathe — grow/shrink pulse (0…1)">breathe<input class="tl-num" type="number" min="0" max="1" step="0.1" data-tl-ovbreathe="${i},${o}" value="${ov.breathe ?? 0}"></label>`
    +   `<label class="tl-splbl" title="beat-sync BPM (0 = off)">bpm<input class="tl-num" type="number" min="0" step="1" data-tl-ovbpm="${i},${o}" value="${ov.bpm ?? 0}"></label></div></div>`
}
function tlCardFields(r, i) {
  const roleSel = (role, k) => `<select class="tl-crole" data-tl-role="${i},${k}">`
    + TL_ROLES.map(o => `<option${o === role ? ' selected' : ''}>${o}</option>`).join('') + `</select>`
  const lines = (r.lines || []).map((l, k) => `<div class="tl-cline">${roleSel(l.role, k)}`
    + `<input class="tl-ctext" data-tl-ctext="${i},${k}" value="${escHtml(l.text)}" placeholder="text">`
    + `<button class="tl-cx" data-tl-cdel="${i},${k}" title="remove line">✕</button></div>`).join('')
  const animSel = (val, attr) => `<select class="tl-canim" data-${attr}="${i}">`
    + TL_ANIMS.map(a => `<option${a === (val || 'fade') ? ' selected' : ''}>${a}</option>`).join('') + `</select>`
  const inDur = r.inDur ?? 0.5, holdDur = r.holdDur ?? 1.5, outDur = r.outDur ?? 0
  const wb = r.waitBefore ?? 0, wa = r.waitAfter ?? 0, bg = r.bg == null ? 17 : r.bg
  const num = (attr, val, min) => `<input class="tl-num" type="number" min="${min}" step="0.1" data-${attr}="${i}" value="${val}">`
  return `<div class="tl-ingroup"><span class="tl-ink">lines</span>`
    +   `<div class="tl-cardlines">${lines}<button class="tl-cadd" data-tl-cadd="${i}">＋ line</button></div></div>`
    + `<div class="tl-ingroup"><span class="tl-ink">timing</span><div class="tl-cardopts">`
    +   `<span class="tl-phase" title="wait — blank lead-in before the in (s)">wait ${num('tl-cwaitb', wb, 0)}</span>`
    +   `<span class="tl-phase" title="in transition (s + effect)">in ${animSel(r.inEffect, 'tl-cinanim')}${num('tl-cindur', inDur, 0)}</span>`
    +   `<span class="tl-phase" title="hold — boil/breathe (s)">hold ${num('tl-chold', holdDur, 0)}</span>`
    +   `<span class="tl-phase" title="out transition (s + effect)">out ${animSel(r.outEffect, 'tl-coutanim')}${num('tl-coutdur', outDur, 0)}</span>`
    +   `<span class="tl-phase" title="wait — blank tail after the out (s)">wait ${num('tl-cwaita', wa, 0)}</span></div></div>`
    + `<div class="tl-ingroup"><span class="tl-ink">look</span><div class="tl-cardopts">`
    +   `<label class="tl-splbl" title="background colour (0–31)"><span class="tl-sw" style="background:${paletteHex(bg)}"></span>`
    +   `<input class="tl-num" type="number" min="0" max="31" data-tl-cbg="${i}" value="${bg}"></label>`
    +   `<label class="tl-splbl" title="boil — hand-drawn wobble (0 = off … 1)">boil<input class="tl-num" type="number" min="0" max="1" step="0.1" data-tl-cboil="${i}" value="${r.boil ?? 1}"></label>`
    +   `<label class="tl-splbl" title="breathe — grow/shrink pulse (0 = off … 1)">breathe<input class="tl-num" type="number" min="0" max="1" step="0.1" data-tl-cbreathe="${i}" value="${r.breathe ?? 0}"></label>`
    +   `<label class="tl-splbl" title="beat-sync BPM (0 = off) — breathe punches + boil ticks on the beat">bpm<input class="tl-num" type="number" min="0" step="1" data-tl-cbpm="${i}" value="${r.bpm ?? 0}"></label></div></div>`
}
// the loop-close diamond after the last block: crossfade the end back into the start (seamless loop)
function tlLoopControl() {
  if (tlRows.length < 2) return ''
  const on = !!tlLoop
  const sel = `<select data-tl-loopx>`
    + `<option value=""${on ? '' : ' selected'}>↺ no loop</option>`
    + TL_XFADES.map(x => `<option value="${x}"${on && tlLoop.type === x ? ' selected' : ''}>↺ ${x}</option>`).join('') + `</select>`
  const secs = `<input class="tl-secs" type="number" min="0.1" max="2" step="0.1" data-tl-loopd value="${on ? tlLoop.dur : 0.5}"${on ? '' : ' disabled'}>`
  return `<span class="tl-loop" title="loop the reel back to the start with a seamless crossfade">${sel}${secs}</span>`
}
// live overlay preview — render the selected (and any scrubbed-into) text overlay over the monitor.
// An approximation of the magic-key composite; Build is exact. Skipped during ▶ play all (a follow-up).
function tlPaintOv(layer, shown) {   // render a list of overlays into the layer (positioned top/center/bottom), or hide
  const html = shown.map(ov => `<div class="tl-ovp ${ov.pos || 'bottom'}">`
    + (ov.lines || []).map(l => `<div class="tl-ph-${l.role}">${escHtml(l.text)}</div>`).join('') + `</div>`).join('')
  if (layer._ovhtml === html) { layer.hidden = !html; return }   // skip rewriting unchanged content (called ~60fps during play)
  layer._ovhtml = html
  layer.innerHTML = html
  layer.hidden = !html
}
function tlUpdateOvPreview() {
  const layer = document.getElementById('tl-ov-preview'); if (!layer) return
  if (tlSeq) {   // ── ▶ play all: composite the playing clip's overlays over the active video buffer ──
    const it = tlSeq.items[tlSeq.idx]
    if (!it || it.kind !== 'clip' || !(it.overlays && it.overlays.length)) { layer.hidden = true; return }
    const v = tlVid(tlSeq.buf), t = (v.currentTime - (it.tStart || 0)) / (it.speed || 1)
    tlPaintOv(layer, it.overlays.filter(ov => t >= (ov.a || 0) - 0.05 && t <= (ov.b == null ? 1e9 : ov.b) + 0.05))
    return
  }
  // ── scrub mode: the selected overlay + any whose window the monitor is currently in ──
  const mon = document.getElementById('tl-monitor'), i = tlFocus, r = tlRows[i]
  if (i < 0 || !r || r.card || !(r.overlays && r.overlays.length)) { layer.hidden = true; return }
  const t = (mon.currentTime - (r.trim ? r.trim[0] : 0)) / (r.speed || 1)   // on-screen seconds into the clip
  tlPaintOv(layer, r.overlays.filter((ov, o) => (tlOvSel && tlOvSel.r === i && tlOvSel.o === o)   // always show the one being edited
    || (t >= (ov.a || 0) - 0.05 && t <= (ov.b == null ? 1e9 : ov.b) + 0.05)))                     // plus any whose window we're in
}
document.getElementById('tl-monitor')?.addEventListener('timeupdate', tlUpdateOvPreview)
// select a block → fill the inspector with its properties; a clip also loads into the monitor to scrub
function tlSelect(i) {
  tlSel = i; tlOvSel = null; tlRender()
  const r = tlRows[i]
  if (r && !r.card) tlFocusBlock(i)
}
// load a clip into the preview monitor (the NLE "one monitor" model)
function tlFocusBlock(i) {
  const r = tlRows[i]; if (!r || !tlBaked.has(r.clip)) return
  tlSeqStop()   // scrubbing one clip and playing the whole reel fight over the same <video>s
  tlFocus = i
  const mon = document.getElementById('tl-monitor'); if (!mon) return
  document.getElementById('tl-mon-name').textContent = r.clip
  const seek = () => { mon.playbackRate = r.speed || 1; try { mon.currentTime = r.trim ? r.trim[0] : 0 } catch {} }
  if (!mon.src.endsWith(`${r.clip}.webm`)) { mon.src = tlSrc(r.clip); mon.addEventListener('loadedmetadata', seek, { once: true }) }
  else seek()
}
// write a trim for a row (null when it's the full clip), keep focus, re-render
function tlSetTrim(i, a, b) {
  const dur = tlDur[tlRows[i].clip] || 0
  a = Math.max(0, Math.min(round1(a), dur)); b = Math.max(a + 0.1, Math.min(round1(b), dur))
  tlRows[i].trim = (a <= 0.05 && b >= dur - 0.05) ? null : [a, b]
  tlRender()
}
document.getElementById('tl-close')?.addEventListener('click', () => {
  tlSeqStop()
  document.getElementById('trailer-modal').hidden = true
  for (const id of ['tl-preview', 'tl-monitor', 'tl-monitor2']) {   // stop playback + release files (nothing humming behind a closed panel)
    const v = document.getElementById(id); if (v) { v.pause?.(); v.removeAttribute('src'); v.load?.(); }
  }
  const prev = document.getElementById('tl-preview'); if (prev) prev.hidden = true
  const mw = document.getElementById('tl-monwrap'); if (mw) mw.hidden = true
})
// deep-clone a row so a duplicate (or drag-copy) never shares its arrays
const tlCloneRow = s => ({ ...s, trim: s.trim ? [...s.trim] : null,
  lines: s.lines ? s.lines.map(l => ({ ...l })) : undefined,
  overlays: s.overlays ? s.overlays.map(o => ({ ...o, lines: (o.lines || []).map(l => ({ ...l })) })) : undefined })
document.getElementById('tl-timeline')?.addEventListener('click', e => {
  if (tlOvMoved) { tlOvMoved = false; return }   // an overlap drag just ended — don't also treat it as a click
  const block = e.target.closest('[data-tl-block]')   // click a block → select it (fills the inspector; a clip loads in the monitor)
  if (block) tlSelect(+block.dataset.tlBlock)
})
// ── drag & drop: drag a library clip onto the timeline to place it (repeats allowed) · drag a block to reorder ──
// the insert index under the cursor: before the first block whose horizontal midpoint we're left of
const tlDropIndexAt = clientX => {
  const blocks = [...document.querySelectorAll('#tl-timeline .tl-block')]
  for (let k = 0; k < blocks.length; k++) { const r = blocks[k].getBoundingClientRect(); if (clientX < r.left + r.width / 2) return k }
  return blocks.length
}
const tlShowDrop = idx => {
  const blocks = [...document.querySelectorAll('#tl-timeline .tl-block')]
  blocks.forEach(b => b.classList.remove('tl-drop-before', 'tl-drop-after'))
  if (idx == null) return
  if (idx >= blocks.length) blocks[blocks.length - 1]?.classList.add('tl-drop-after')
  else blocks[idx]?.classList.add('tl-drop-before')
}
document.getElementById('tl-library')?.addEventListener('dragstart', e => {
  const chip = e.target.closest('[data-tl-add]'); if (!chip) return
  e.dataTransfer.setData('text/plain', `lib:${chip.dataset.tlAdd}`); e.dataTransfer.effectAllowed = 'copy'
})
document.getElementById('tl-timeline')?.addEventListener('dragstart', e => {
  const block = e.target.closest('[data-tl-block]')
  // grabbing a handle / button / field must NOT start a block drag (those have their own behaviour)
  if (!block || e.target.closest('.tl-h, .tl-btns, input, select')) { e.preventDefault(); return }
  e.dataTransfer.setData('text/plain', `row:${block.dataset.tlBlock}`); e.dataTransfer.effectAllowed = 'move'
  block.classList.add('tl-dragging')
})
document.getElementById('tl-timeline')?.addEventListener('dragover', e => { e.preventDefault(); tlShowDrop(tlDropIndexAt(e.clientX)) })
document.getElementById('tl-timeline')?.addEventListener('dragleave', e => {
  if (!e.relatedTarget || !e.currentTarget.contains(e.relatedTarget)) tlShowDrop(null)
})
document.getElementById('tl-timeline')?.addEventListener('drop', e => {
  e.preventDefault()
  const data = e.dataTransfer.getData('text/plain') || ''
  let to = tlDropIndexAt(e.clientX)
  tlShowDrop(null)
  if (data.startsWith('lib:')) {
    tlRows.splice(to, 0, { clip: data.slice(4), xtype: 'fade', xdur: 0.5, trim: null, speed: 1 })
    tlSel = -1; tlOvSel = null; tlFocus = -1; tlRender(); tlSeqSync()
  } else if (data.startsWith('row:')) {
    const from = +data.slice(4); if (from === to || from + 1 === to) return   // dropped in place
    if (to > from) to--                                                        // removing shifts everything left
    const [row] = tlRows.splice(from, 1); tlRows.splice(to, 0, row)
    tlSel = -1; tlOvSel = null; tlFocus = -1; tlRender(); tlSeqSync()
  }
})
document.getElementById('tl-timeline')?.addEventListener('dragend', () => {
  document.querySelectorAll('#tl-timeline .tl-block').forEach(b => b.classList.remove('tl-dragging', 'tl-drop-before', 'tl-drop-after'))
})
document.getElementById('tl-clear')?.addEventListener('click', () => { tlRows = []; tlSel = -1; tlOvSel = null; tlFocus = -1; tlRender(); tlSeqSync() })   // start a subset from empty
// drag the in/out handles (in the inspector) → live trim, seeking the monitor to the cut frame as you go
let tlDrag = null
document.getElementById('tl-inspector')?.addEventListener('pointerdown', e => {
  const h = e.target.closest('[data-tl-h]'); if (!h) return
  e.preventDefault()
  const [i, which] = h.dataset.tlH.split(',').map(Number)
  const dur = tlDur[tlRows[i].clip]; if (!dur) return
  tlDrag = { i, which, dur, rect: h.closest('.tl-trim').getBoundingClientRect() }
  h.setPointerCapture?.(e.pointerId)
  if (tlFocus !== i) tlFocusBlock(i)
})
window.addEventListener('pointermove', e => {
  if (!tlDrag) return
  const { i, which, dur, rect } = tlDrag, r = tlRows[i]
  const t = round1(Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width)) * dur)
  let a = r.trim ? r.trim[0] : 0, b = r.trim ? r.trim[1] : dur
  if (which === 0) a = Math.min(t, b - 0.1); else b = Math.max(t, a + 0.1)
  r.trim = (a <= 0.05 && b >= dur - 0.05) ? null : [a, b]
  // update visuals in place (no full re-render mid-drag) + seek the monitor to the moving edge
  const inP = a / dur * 100, outP = b / dur * 100
  const trk = document.querySelector(`[data-tl-trim="${i}"]`)   // the trim track lives in the inspector now
  if (trk) { trk.querySelector('.tl-range').style.left = inP + '%'; trk.querySelector('.tl-range').style.right = (100 - outP) + '%'
    trk.querySelectorAll('.tl-h')[0].style.left = inP + '%'; trk.querySelectorAll('.tl-h')[1].style.left = outP + '%' }
  const info = document.querySelector('#tl-inspector .tl-tinfo:not(.rs-dim)')
  if (info) info.textContent = `${a.toFixed(1)}→${b.toFixed(1)}s of ${dur.toFixed(1)}s${r.speed && r.speed !== 1 ? ` · ${r.speed}×` : ''}`
  const mon = document.getElementById('tl-monitor')
  if (mon && tlFocus === i) { try { mon.currentTime = which === 0 ? a : b } catch {} }
})
window.addEventListener('pointerup', () => { if (tlDrag) { tlDrag = null; tlRender() } })
// ── slice 2: drag a transition overlap → set its xdur (live width; downstream blocks snap on release) ──
let tlOvDrag = null, tlOvMoved = false
document.getElementById('tl-timeline')?.addEventListener('pointerdown', e => {
  const ov = e.target.closest('[data-tl-ov]'); if (!ov) return
  e.preventDefault()
  const i = +ov.dataset.tlOv
  const maxXd = Math.min(tlRowDur(tlRows[i]) - 0.05, tlRowDur(tlRows[i - 1]) - 0.05)   // an xfade can't outlast either clip
  tlOvDrag = { i, ov, blk: ov.closest('.tl-track')?.querySelector(`.tl-block[data-tl-block="${i}"]`),
    prevRight: +ov.dataset.tlPr, maxXd, startX: e.clientX, startXd: tlRows[i].xdur || 0.5 }
  tlOvMoved = false
  ov.setPointerCapture?.(e.pointerId)
})
window.addEventListener('pointermove', e => {
  if (!tlOvDrag) return
  const { i, ov, blk, prevRight, maxXd, startX, startXd } = tlOvDrag
  // the overlap is pinned to block i-1's right edge, so it grows LEFTWARD — drag left = wider (follows the pointer)
  const nd = Math.max(0.1, Math.min(round1(startXd - (e.clientX - startX) / tlPps), Math.max(0.1, maxXd)))
  if (Math.abs(nd - startXd) > 0.001) tlOvMoved = true
  tlRows[i].xdur = nd
  const leftPx = (prevRight - nd) * tlPps   // widening the overlap pulls block i (and the overlap) left
  ov.style.left = leftPx.toFixed(1) + 'px'; ov.style.width = (nd * tlPps).toFixed(1) + 'px'
  if (blk) blk.style.left = leftPx.toFixed(1) + 'px'
})
window.addEventListener('pointerup', () => {
  if (!tlOvDrag) return
  const { i } = tlOvDrag, moved = tlOvMoved
  tlOvDrag = null
  if (moved) { tlRender(); tlSeqSync() }   // re-lay everything to the new overlap; refresh a running preview
  else tlSelect(i)                          // a click (no drag) → select the block so its transition shows in the inspector
})
// ── overlay track: drag an overlay block to move its window, or its ◧▮ edges to resize [a,b] ──
let tlOvbDrag = null, tlOvbMoved = false
document.getElementById('tl-timeline')?.addEventListener('pointerdown', e => {
  const h = e.target.closest('[data-tl-ovh]'); const blk = e.target.closest('[data-tl-ovblk]')
  if (!h && !blk) return
  e.preventDefault()
  const container = h ? h.closest('.tl-ovblk') : blk
  const [i, o] = container.dataset.tlOvblk.split(',').map(Number)
  const ov = tlRows[i]?.overlays?.[o]; if (!ov) return
  const clipDur = tlRowDur(tlRows[i])
  tlOvbDrag = { i, o, el: container, mode: h ? (+h.dataset.tlOvh.split(',')[2] === 0 ? 'a' : 'b') : 'move',
    clipDur, clipLeft: +container.dataset.tlCl || 0, startX: e.clientX, startA: ov.a || 0, startB: ov.b == null ? clipDur : ov.b }
  tlOvbMoved = false
  ;(h || blk).setPointerCapture?.(e.pointerId)
})
window.addEventListener('pointermove', e => {
  if (!tlOvbDrag) return
  const { i, o, el, mode, clipDur, clipLeft, startX, startA, startB } = tlOvbDrag
  const d = (e.clientX - startX) / tlPps
  let a = startA, b = startB
  if (mode === 'a') a = Math.max(0, Math.min(round1(startA + d), b - 0.1))
  else if (mode === 'b') b = Math.min(clipDur, Math.max(round1(startB + d), a + 0.1))
  else { const w = startB - startA; a = Math.max(0, Math.min(round1(startA + d), clipDur - w)); b = round1(a + w) }
  if (Math.abs(a - startA) > 0.001 || Math.abs(b - startB) > 0.001) tlOvbMoved = true
  const ov = tlRows[i].overlays[o]; ov.a = a; ov.b = b
  if (el) { el.style.left = ((clipLeft + a) * tlPps).toFixed(1) + 'px'; el.style.width = ((b - a) * tlPps).toFixed(1) + 'px' }
})
window.addEventListener('pointerup', () => {
  if (!tlOvbDrag) return
  const { i, o } = tlOvbDrag, moved = tlOvbMoved
  tlOvbDrag = null
  if (moved) { tlRender(); tlSeqSync() }
  else { tlOvSel = { r: i, o }; tlSel = -1; tlRender(); tlFocusBlock(i) }   // a click (no drag) → select the overlay
})
// the timeline itself only carries the loop-close control now; block properties are handled in the inspector
document.getElementById('tl-timeline')?.addEventListener('change', e => {
  const lx = e.target.closest('[data-tl-loopx]'); const ld = e.target.closest('[data-tl-loopd]')
  if (lx) { tlLoop = lx.value ? { type: lx.value, dur: (tlLoop && tlLoop.dur) || 0.5 } : null; tlRender() }
  if (ld && tlLoop) tlLoop.dur = parseFloat(ld.value) || 0.5
})
// ── inspector: all the selected block's property edits (dup/remove · transition-in · trim/speed · card fields) ──
document.getElementById('tl-inspector')?.addEventListener('click', e => {
  const dup = e.target.closest('[data-tl-dup]'); const rm = e.target.closest('[data-tl-rm]')
  const cadd = e.target.closest('[data-tl-cadd]'); const cdel = e.target.closest('[data-tl-cdel]')
  const ovadd = e.target.closest('[data-tl-ovadd]'); const ovrm = e.target.closest('[data-tl-ovrm]')
  const ovsel = e.target.closest('[data-tl-ovsel]')
  const ovla = e.target.closest('[data-tl-ovlineadd]'); const ovld = e.target.closest('[data-tl-ovlinedel]')
  if (dup) { const i = +dup.dataset.tlDup; tlRows.splice(i + 1, 0, tlCloneRow(tlRows[i])); tlSel = -1; tlOvSel = null; tlFocus = -1; tlRender(); tlSeqSync() }
  else if (rm) { tlRows.splice(+rm.dataset.tlRm, 1); tlSel = -1; tlOvSel = null; tlFocus = -1; tlRender(); tlSeqSync() }
  else if (cadd) { const i = +cadd.dataset.tlCadd; (tlRows[i].lines || (tlRows[i].lines = [])).push({ role: 'body', text: '' }); tlRender(); tlSeqSync() }
  else if (cdel) { const [i, k] = cdel.dataset.tlCdel.split(',').map(Number); tlRows[i].lines.splice(k, 1); tlRender(); tlSeqSync() }
  // ── text overlays ──
  else if (ovadd) { const i = +ovadd.dataset.tlOvadd, dur = tlRowDur(tlRows[i])   // a new overlay riding this clip
    const ovs = tlRows[i].overlays || (tlRows[i].overlays = [])
    ovs.push({ a: round1(Math.min(0.3, dur * 0.1)), b: round1(Math.min(dur, Math.max(1.5, dur * 0.6))), pos: 'bottom', inDur: 0.4, inEffect: 'fade', outDur: 0.4, outEffect: 'slide top', lines: [{ role: 'body', text: 'new' }] })
    tlOvSel = { r: i, o: ovs.length - 1 }; tlSel = -1; tlRender(); tlSeqSync() }
  else if (ovrm) { const [i, o] = ovrm.dataset.tlOvrm.split(',').map(Number); tlRows[i].overlays.splice(o, 1); tlOvSel = null; tlRender(); tlSeqSync() }
  else if (ovsel) { const [i, o] = ovsel.dataset.tlOvsel.split(',').map(Number); tlOvSel = { r: i, o }; tlSel = -1; tlRender() }
  else if (ovla) { const [i, o] = ovla.dataset.tlOvlineadd.split(',').map(Number); (tlRows[i].overlays[o].lines || (tlRows[i].overlays[o].lines = [])).push({ role: 'body', text: '' }); tlRender() }
  else if (ovld) { const [i, o, k] = ovld.dataset.tlOvlinedel.split(',').map(Number); tlRows[i].overlays[o].lines.splice(k, 1); tlRender() }
})
document.getElementById('tl-inspector')?.addEventListener('change', e => {
  const x = e.target.closest('[data-tl-x]'); const d = e.target.closest('[data-tl-d]'); const sp = e.target.closest('[data-tl-sp]')
  if (x) { tlRows[+x.dataset.tlX].xtype = x.value; tlRender() }     // re-render so the ◇ join marker reflects it
  if (d) { tlRows[+d.dataset.tlD].xdur = parseFloat(d.value) || 0.5; tlRender() }
  if (sp) { const i = +sp.dataset.tlSp; tlRows[i].speed = parseFloat(sp.value) || 1
    const mon = document.getElementById('tl-monitor'); if (mon && tlFocus === i) mon.playbackRate = tlRows[i].speed; tlRender() }
  // text-card fields
  const role = e.target.closest('[data-tl-role]'); const cbg = e.target.closest('[data-tl-cbg]')
  if (role) { const [i, k] = role.dataset.tlRole.split(',').map(Number); tlRows[i].lines[k].role = role.value; tlRender() }
  if (cbg)  { tlRows[+cbg.dataset.tlCbg].bg = Math.max(0, Math.min(31, parseInt(cbg.value) || 0)); tlRender() }
  // in / hold / out timing (effect selects don't resize; duration fields do)
  const ci = e.target.closest('[data-tl-cinanim]'); const co = e.target.closest('[data-tl-coutanim]')
  if (ci) tlRows[+ci.dataset.tlCinanim].inEffect = ci.value
  if (co) tlRows[+co.dataset.tlCoutanim].outEffect = co.value
  const setDur = (el, attr, key) => { if (!el) return; const i = +el.dataset[attr]; const R = tlRows[i]; R[key] = Math.max(0, parseFloat(el.value) || 0)
    R.dur = (R.waitBefore ?? 0) + (R.inDur ?? 0.5) + (R.holdDur ?? 1.5) + (R.outDur ?? 0) + (R.waitAfter ?? 0); tlRender() }
  setDur(e.target.closest('[data-tl-cwaitb]'),  'tlCwaitb',  'waitBefore')
  setDur(e.target.closest('[data-tl-cindur]'),  'tlCindur',  'inDur')
  setDur(e.target.closest('[data-tl-chold]'),   'tlChold',   'holdDur')
  setDur(e.target.closest('[data-tl-coutdur]'), 'tlCoutdur', 'outDur')
  setDur(e.target.closest('[data-tl-cwaita]'),  'tlCwaita',  'waitAfter')
  const cboil = e.target.closest('[data-tl-cboil]'); const cbr = e.target.closest('[data-tl-cbreathe]'); const cbpm = e.target.closest('[data-tl-cbpm]')
  if (cboil) tlRows[+cboil.dataset.tlCboil].boil = Math.max(0, Math.min(1, parseFloat(cboil.value) || 0))
  if (cbr)   tlRows[+cbr.dataset.tlCbreathe].breathe = Math.max(0, Math.min(1, parseFloat(cbr.value) || 0))
  if (cbpm)  tlRows[+cbpm.dataset.tlCbpm].bpm = Math.max(0, parseFloat(cbpm.value) || 0)
  // ── text-overlay fields (keyed by "i,o" clip,overlay — or "i,o,k" for a line role) ──
  const ov = (el) => { const [i, o] = el.dataset[Object.keys(el.dataset).find(k => k.startsWith('tlOv'))].split(',').map(Number); return tlRows[i].overlays[o] }
  const ovrole = e.target.closest('[data-tl-ovrole]'); const ovpos = e.target.closest('[data-tl-ovpos]')
  const ovineff = e.target.closest('[data-tl-ovineff]'); const ovindur = e.target.closest('[data-tl-ovindur]')
  const ovouteff = e.target.closest('[data-tl-ovouteff]'); const ovoutdur = e.target.closest('[data-tl-ovoutdur]')
  const ova = e.target.closest('[data-tl-ova]'); const ovb = e.target.closest('[data-tl-ovb]')
  const ovboil = e.target.closest('[data-tl-ovboil]'); const ovbre = e.target.closest('[data-tl-ovbreathe]'); const ovbpm = e.target.closest('[data-tl-ovbpm]')
  if (ovrole) { const [i, o, k] = ovrole.dataset.tlOvrole.split(',').map(Number); tlRows[i].overlays[o].lines[k].role = ovrole.value; tlRender() }
  if (ovpos)  { ov(ovpos).pos = ovpos.value; tlRender() }
  if (ovineff)  { const o = ov(ovineff); o.inEffect = ovineff.value; if (o.inDur == null) o.inDur = 0.4 }   // set inDur so build uses the in/out path
  if (ovindur)  { const o = ov(ovindur); o.inDur = Math.max(0, parseFloat(ovindur.value) || 0); o.inEffect = o.inEffect || 'fade' }
  if (ovouteff) ov(ovouteff).outEffect = ovouteff.value
  if (ovoutdur) ov(ovoutdur).outDur = Math.max(0, parseFloat(ovoutdur.value) || 0)
  if (ova)    { const o = ov(ova); o.a = Math.max(0, parseFloat(ova.value) || 0); if (o.b <= o.a) o.b = o.a + 0.5; tlRender(); tlSeqSync() }
  if (ovb)    { const o = ov(ovb); o.b = Math.max((o.a || 0) + 0.1, parseFloat(ovb.value) || 0); tlRender(); tlSeqSync() }
  if (ovboil) { ov(ovboil).boil = Math.max(0, Math.min(1, parseFloat(ovboil.value) || 0)) }
  if (ovbre)  { ov(ovbre).breathe = Math.max(0, Math.min(1, parseFloat(ovbre.value) || 0)) }
  if (ovbpm)  { ov(ovbpm).bpm = Math.max(0, parseFloat(ovbpm.value) || 0) }
})
// live text edits — update the model without a re-render (keeps focus in the field)
document.getElementById('tl-inspector')?.addEventListener('input', e => {
  const t = e.target.closest('[data-tl-ctext]')
  if (t) { const [i, k] = t.dataset.tlCtext.split(',').map(Number); if (tlRows[i] && tlRows[i].lines[k]) tlRows[i].lines[k].text = t.value; return }
  const ot = e.target.closest('[data-tl-ovtext]')   // an overlay's line text
  if (ot) { const [i, o, k] = ot.dataset.tlOvtext.split(',').map(Number); const L = tlRows[i]?.overlays?.[o]?.lines?.[k]; if (L) L.text = ot.value }
})
// monitor buttons: capture the scrubbed frame as in/out, or play just the in→out range at speed
document.getElementById('tl-mon-in')?.addEventListener('click', () => {
  if (tlFocus < 0) return; const mon = document.getElementById('tl-monitor'), r = tlRows[tlFocus]
  tlSetTrim(tlFocus, mon.currentTime, r.trim ? r.trim[1] : (tlDur[r.clip] || 0))
})
document.getElementById('tl-mon-out')?.addEventListener('click', () => {
  if (tlFocus < 0) return; const mon = document.getElementById('tl-monitor'), r = tlRows[tlFocus]
  tlSetTrim(tlFocus, r.trim ? r.trim[0] : 0, mon.currentTime)
})
document.getElementById('tl-mon-play')?.addEventListener('click', () => {
  if (tlFocus < 0) return; const mon = document.getElementById('tl-monitor'), r = tlRows[tlFocus]
  const end = r.trim ? r.trim[1] : (tlDur[r.clip] || mon.duration)
  const onTU = () => { if (mon.currentTime >= end - 0.03) { mon.pause(); mon.removeEventListener('timeupdate', onTU) } }
  mon.playbackRate = r.speed || 1
  try { mon.currentTime = r.trim ? r.trim[0] : 0 } catch {}
  mon.addEventListener('timeupdate', onTU); mon.play()
})
// ── slice 3: continuous sequence player — play the WHOLE reel live from <video>s, NO bake ──
// Two stacked <video>s double-buffer the clips; the incoming one is raised above and its opacity
// ramps 0→1 so fade/dissolve cross-fade for real. Wipes/slides/pixelize can't be done with opacity,
// so they hard-cut and flash a "≈ <type> — exact on Build" badge. Card rows show a static text
// placeholder for their duration (boil/breathe only appear on Build). An approximation on purpose —
// per the doc, live preview is the render cache; Build is truth. (trailer-builder.md §Live preview slice 3.)
let tlSeq = null   // active player state (null = stopped)
const tlVid = k => document.getElementById(k === 0 ? 'tl-monitor' : 'tl-monitor2')
const OPACITY_X = new Set(['fade', 'dissolve'])   // the only xfades opacity can honestly fake
function tlSeqStop() {
  if (!tlSeq) return
  cancelAnimationFrame(tlSeq.raf); clearTimeout(tlSeq.timer)
  for (const k of [0, 1]) { const v = tlVid(k); v.pause?.(); }
  const a = tlVid(0), b = tlVid(1)
  a.controls = true; a.style.opacity = ''; a.style.zIndex = ''; a.muted = false
  b.style.opacity = 0; b.style.zIndex = ''; b.muted = true
  document.getElementById('tl-cardph').hidden = true
  document.getElementById('tl-seqbadge').hidden = true
  const btn = document.getElementById('tl-seq-play'); if (btn) btn.textContent = '▶ play all'
  tlSeq = null
  tlUpdateOvPreview()   // play-all hid overlays; restore the scrub preview
}
// a structural timeline edit (add / remove / reorder / clear) happened — keep a RUNNING live preview
// honest to it by rebuilding the item list and restarting from the top (a no-op when not playing).
function tlSeqSync() {
  if (!tlSeq) return
  cancelAnimationFrame(tlSeq.raf); clearTimeout(tlSeq.timer)
  const items = tlBuildSeq()
  if (!items.length) { tlSeqStop(); return }
  tlSeq.items = items
  tlSeqPlay(0, tlSeq.buf)
}
// flatten the timeline into playable items — EVERY row is represented so nothing silently vanishes.
// A baked clip's out-point (tEnd) is resolved from the real <video> at play time (no dependence on the
// async duration probe → no race). An unbaked clip becomes a "bake to preview" placeholder, not a gap.
function tlBuildSeq() {
  return tlRows.map(r => {
    if (r.card) return { kind: 'card', dur: r.dur || 2, lines: r.lines || [], bg: r.bg == null ? 17 : r.bg, xtype: r.xtype, xdur: r.xdur }
    if (!tlBaked.has(r.clip)) return { kind: 'need', clip: r.clip, dur: 1.5, xtype: r.xtype, xdur: r.xdur }
    return { kind: 'clip', clip: r.clip, trim: r.trim, tStart: r.trim ? r.trim[0] : 0, tEnd: null, speed: r.speed || 1, xtype: r.xtype, xdur: r.xdur, overlays: r.overlays || [] }
  })
}
// how many seconds the NEXT part overlaps this one (its transition), or the loop-close on the last part.
// The baked reel overlaps every join by this much, so the preview cuts each part this much early to match.
function tlSeqOverlap(idx) {
  const next = tlSeq.items[idx + 1]
  if (next) return next.xdur || 0
  return tlLoop ? (tlLoop.dur || 0) : 0
}
function tlSeqBadge(text) {
  const el = document.getElementById('tl-seqbadge')
  el.textContent = text; el.hidden = false
  clearTimeout(tlSeq.badgeT); tlSeq.badgeT = setTimeout(() => { if (tlSeq) el.hidden = true }, 900)
}
// load a clip file into a buffer, seek to its in-point + set speed, then call cb once it's ready to show
function tlLoadClip(v, it, cb) {
  const ready = () => { v.playbackRate = it.speed; cb && cb() }
  const afterMeta = () => {
    const onSeek = () => { v.removeEventListener('seeked', onSeek); ready() }
    v.addEventListener('seeked', onSeek)
    try { v.currentTime = it.tStart } catch { ready() }
  }
  if (v.src.endsWith(`${it.clip}.webm`) && v.readyState >= 1) afterMeta()
  else { v.src = tlSrc(it.clip); v.addEventListener('loadedmetadata', afterMeta, { once: true }) }
}
// show/advance to item `idx` in buffer `buf` (the OTHER buffer is left for preloading the next clip)
function tlSeqPlay(idx, buf) {
  if (!tlSeq) return
  if (idx >= tlSeq.items.length) {   // end of reel → loop if the loop-close diamond is on, else stop
    if (tlLoop && tlSeq.items.length) { tlSeqPlay(0, buf); return }
    tlSeqStop(); return
  }
  tlSeq.idx = idx; tlSeq.buf = buf; tlSeq.xfading = false; tlSeq.timer = 0   // consumed any card timer; 0 so the tick guard passes
  const it = tlSeq.items[idx], ph = document.getElementById('tl-cardph')
  const a = tlVid(buf), b = tlVid(1 - buf)
  if (it.kind === 'card' || it.kind === 'need') {   // no <video> to play — a static placeholder for its duration
    a.pause(); b.pause(); a.style.opacity = 0; b.style.opacity = 0
    ph.style.background = it.kind === 'card' ? paletteHex(it.bg) : '#222'
    ph.innerHTML = it.kind === 'card'
      ? (it.lines.map(l => `<div class="tl-ph-${l.role}">${escHtml(l.text)}</div>`).join('') || '<div class="tl-ph-body">·</div>')
      : `<div class="tl-ph-sub">⋯ ${escHtml(it.clip)}</div><div class="tl-ph-body">bake to preview</div>`
    ph.hidden = false
    tlSeqBadge(it.kind === 'card' ? '≈ card — motion on Build' : '≈ not baked — Build to include')
    const showFor = Math.max(0.2, it.dur - tlSeqOverlap(idx))   // cut early by the next join's overlap → matches the bake
    tlSeq.timer = setTimeout(() => tlSeqPlay(idx + 1, buf), showFor * 1000)
    return
  }
  // a clip: raise this buffer on top, make it the audible one, play from its in-point
  ph.hidden = true
  a.style.zIndex = 3; b.style.zIndex = 1
  a.muted = false; b.muted = true
  tlLoadClip(a, it, () => {
    if (!tlSeq || tlSeq.idx !== idx) return
    if (it.tEnd == null) it.tEnd = it.trim ? it.trim[1] : (a.duration || 0)   // resolve the out-point from the real video
    a.style.opacity = 1; a.play?.()
    const next = tlSeq.items[idx + 1]
    if (next && next.kind === 'clip') tlLoadClip(b, next, () => { if (next.tEnd == null) next.tEnd = next.trim ? next.trim[1] : (b.duration || 0) })   // preload the next clip
    tlSeq.raf = requestAnimationFrame(tlSeqTick)
  })
}
// per-frame: watch the active clip approach its out-point, then cross-fade (opacity) or hard-cut into the next
function tlSeqTick() {
  if (!tlSeq || tlSeq.timer) return
  const S = tlSeq, it = S.items[S.idx]
  if (!it || it.kind !== 'clip') return
  tlUpdateOvPreview()   // composite this clip's overlays over the playing buffer (a card's placeholder covers it anyway)
  const a = tlVid(S.buf), b = tlVid(1 - S.buf)
  const next = S.items[S.idx + 1]
  const t = a.currentTime
  const tEnd = it.tEnd != null ? it.tEnd : (a.duration || t)
  const overlapSrc = tlSeqOverlap(S.idx) * it.speed         // source-seconds the outgoing clip spends in the overlap
  if (!S.xfading) {
    const canOpacity = next && next.kind === 'clip' && OPACITY_X.has(next.xtype) && (tEnd - it.tStart) > overlapSrc + 0.05
    if (canOpacity && t >= tEnd - overlapSrc) {             // begin an opacity cross-fade into the next clip
      S.xfading = true; S.xstart = performance.now(); S.xdur = next.xdur
      b.style.zIndex = 3; a.style.zIndex = 1                // incoming rides on top; its opacity ramps up over it
      b.style.opacity = 0; b.muted = false; a.muted = true
      tlLoadClip(b, next, () => { if (next.tEnd == null) next.tEnd = next.trim ? next.trim[1] : (b.duration || 0); if (tlSeq && S.xfading) b.play?.() })
    } else if (t >= tEnd - Math.max(overlapSrc, 0.04)) {    // reached the cut point with no opacity fade → hard cut
      a.pause(); a.style.opacity = 0                         // hide the outgoing frame so it can't peek behind the incoming
      if (next && next.kind === 'clip' && !OPACITY_X.has(next.xtype)) tlSeqBadge(`≈ ${next.xtype} — exact on Build`)
      tlSeqPlay(S.idx + 1, 1 - S.buf); return
    }
  } else {                                                  // animating the cross-fade in wall-clock time
    const p = Math.min(1, (performance.now() - S.xstart) / (S.xdur * 1000))
    b.style.opacity = p
    if (p >= 1) { a.pause(); a.style.opacity = 0; tlSeqPlay(S.idx + 1, 1 - S.buf); return }
  }
  S.raf = requestAnimationFrame(tlSeqTick)
}
document.getElementById('tl-seq-play')?.addEventListener('click', () => {
  if (tlSeq) { tlSeqStop(); return }
  const items = tlBuildSeq()
  if (!items.length) { showToast('nothing playable yet — add baked clips (or Build to see cards)', 3000); return }
  tlFocus = -1; document.querySelectorAll('.tl-block').forEach(b => b.classList.remove('tl-focus'))
  document.getElementById('tl-monwrap').hidden = false
  document.getElementById('tl-mon-name').textContent = 'whole trailer (live preview)'
  const a = tlVid(0); a.controls = false
  tlSeq = { items, idx: 0, buf: 0, raf: 0, timer: 0, badgeT: 0, xfading: false }
  document.getElementById('tl-seq-play').textContent = '⏸ stop'
  tlSeqPlay(0, 0)
})
document.getElementById('tl-library')?.addEventListener('click', e => {
  if (e.target.closest('[data-tl-addcard]')) {   // append a fresh text card
    tlRows.push({ card: true, waitBefore: 0, inDur: 0.5, holdDur: 1.5, outDur: 0, waitAfter: 0, inEffect: 'slide bottom', outEffect: 'slide top', dur: 2, lines: [{ role: 'title', text: 'TITLE' }], bg: 17, boil: 1, breathe: 0, bpm: 0, xtype: 'fade', xdur: 0.5 }); tlRender(); tlSeqSync(); return
  }
  const add = e.target.closest('[data-tl-add]'); if (!add) return
  tlRows.push({ clip: add.dataset.tlAdd, xtype: 'fade', xdur: 0.5, trim: null, speed: 1 }); tlRender(); tlSeqSync()
})
// stream the build log (bake + compose progress) into the trailer panel while it's open
window.studio?.onAsoLog?.(s => {
  const lab = document.getElementById('trailer-modal'); const tlLog = document.getElementById('tl-log')
  if (tlLog && lab && !lab.hidden && !tlLog.hidden) { tlLog.textContent += stripAnsi(s); tlLog.scrollTop = tlLog.scrollHeight }
})
document.getElementById('tl-build')?.addEventListener('click', async () => {
  if (!tlApp || !tlRows.length) { showToast('add at least one clip', 2500); return }
  tlSeqStop()
  const btn = document.getElementById('tl-build'); const stop = busyDots(btn, 'building (bakes + composes)', '▶ Build trailer'); btn.disabled = true
  const tlLog = document.getElementById('tl-log')
  const size = document.getElementById('tl-ratio')?.value || null   // '' = native (first clip's dims)
  if (tlLog) { tlLog.hidden = false; tlLog.textContent = `building ${tlApp}${size ? ` @ ${size}` : ''}…\n` }   // live feedback of what it's doing
  const res = await window.studio.buildReel(tlApp, tlRows, tlLoop, size)
  stop(); btn.disabled = false
  if (!res?.ok) { if (tlLog) tlLog.textContent += `\n✗ ${res?.error || 'build failed'}\n`; showToast(res?.error || 'build failed', 3500); return }
  if (tlLog) tlLog.textContent += `\n✓ done\n`
  const prev = document.getElementById('tl-preview')
  if (res.out && prev) { prev.src = `/${res.out}?t=${Date.now()}`; prev.hidden = false; prev.scrollIntoView({ behavior: 'smooth', block: 'center' }) }
  else showToast(res.note || 'built', 3000)
})

const appListings = {}   // dir → { listing, iapProducts } so the "load into all tools" button can fill the lab
async function renderAppsList() {
  const el = document.getElementById('apps-list')
  if (!el || !window.studio?.listApps) return
  const { apps } = await window.studio.listApps()
  if (!apps || !apps.length) { el.innerHTML = '<div class="apps-empty">no apps yet — an app is <code>apps/&lt;name&gt;/app.json</code></div>'; return }
  el.innerHTML = ''
  for (const a of apps) {
    const card = document.createElement('div')
    card.className = 'app-card'
    card.dataset.app = a.dir
    const meta = [a.carts.join(', '), a.iap ? `${a.iap} IAP` : ''].filter(Boolean).join('  ·  ')
    const L = a.listing || {}
    appListings[a.dir] = { listing: L, iapProducts: a.iapProducts || [] }
    // char-count badge (red if over Apple's limit); a clickable "key" that fills the research box
    const cc = (v, lim) => v ? `<span class="app-cc${v.length > lim ? ' app-over' : ''}">${v.length}/${lim}</span>` : ''
    const chip = t => `<span class="kw-chip" data-term="${escHtml(t)}">${escHtml(t)}</span>`
    const kwChips = kw => String(kw || '').split(',').map(s => s.trim()).filter(Boolean).map(chip).join('')
    const lrow = (k, v, lim) => v ? `<div class="app-lrow"><span class="app-lk">${k}</span><span class="app-lv">${escHtml(v)}</span>${cc(v, lim)}</div>` : ''
    const listingHtml = (L.title || L.subtitle || L.keywords)
      ? `<div class="app-listing">${lrow('title', L.title, 30)}${lrow('subtitle', L.subtitle, 30)}`
        + (L.keywords ? `<div class="app-lrow"><span class="app-lk">keywords</span>${cc(L.keywords, 100)}</div><div class="kw-chips">${kwChips(L.keywords)}</div>` : '')
        + `<div class="app-hint">click any key to drop it in the research box · or:</div>`
        + `<button class="kw-mini" data-allkw>▸ all keys (app + IAP) → research box</button>`
        + `<button class="kw-all" data-fillall>▸ load this listing into all the ASO tools below</button></div>`
      : `<div class="app-listing app-nolisting">no listing yet — add a "listing" block to app.json</div>`
    // IAPs carry their OWN copy — name + description are each a searchable App Store surface.
    const iapHtml = (a.iapProducts && a.iapProducts.length)
      ? `<div class="app-iap"><span class="app-lk">in-app purchases</span> <span class="rs-dim">— each name/desc is its own searchable surface</span>`
        + a.iapProducts.map(p => `<div class="iap-row">${chip(p.name)}<span class="rs-dim">$${escHtml(p.price)}</span>${cc(p.name, 30)}`
          + `<div class="iap-desc">${escHtml(p.desc)}${cc(p.desc, 45)}</div></div>`).join('')
        + `</div>`
      : ''
    card.innerHTML = `<div class="app-name"></div><div class="app-meta"></div>
      <div class="app-actions" hidden>
        ${listingHtml}
        ${iapHtml}
        <span class="app-sec">give</span>
        <button data-act="mac">🍎 Mac app</button>
        <button data-act="ios">📱 iOS app</button>
        <span class="app-sec">site</span>
        <button data-act="shots">📸 screenshots</button>
        <button data-act="press">📄 press kit</button>
        <button data-act="trailer">🎞 trailer</button>
        <span class="app-sec">sell</span>
        <button data-act="brief">📝 seo worksheet</button>
        <button data-act="research">🔎 research keywords</button>
        <button data-act="suggest">💡 suggest keywords</button>
        <button data-act="compose">🧩 compose keywords</button>
        <button data-act="analyze">🔬 analyze listing</button>
        <button data-act="score">📊 score listing</button>
        <button data-act="lint">✅ lint listing</button>
        <button data-act="coverage">🪞 check copy</button>
        <span class="app-sec">reach</span>
        <button data-act="leads">📣 find tribes</button>
        <button data-act="keywords">🔑 keywords</button>
        <span class="app-sec">publish</span>
        <button data-act="store">☁︎ App Store</button>
      </div>`
    card.querySelector('.app-name').textContent = a.name
    card.querySelector('.app-meta').textContent = meta
    el.appendChild(card)
  }
}
document.querySelector('.tab[data-tab="apps"]')?.addEventListener('click', renderAppsList)
// click a card → toggle its actions; click an action → run the app-scoped tool (streams to runtime log)
document.getElementById('apps-list')?.addEventListener('click', async e => {
  // a keyword/IAP "key" chip → drop the term in the research box (don't auto-run; the maker hits
  // research to see the results). Handled before the card-toggle so clicking a chip doesn't collapse.
  const chip = e.target.closest('.kw-chip')
  if (chip) {
    e.stopPropagation()
    const term = chip.dataset.term || ''
    const box = document.getElementById('aso-terms')
    if (box) { box.value = term; box.scrollIntoView({ behavior: 'smooth', block: 'center' }); box.focus() }
    showToast(`"${term}" → research box · hit research to see results`, 2500)
    return
  }
  // "all keys → research box" — gather every keyword (app field + IAP names) into the research
  // box at once (the select-all companion to clicking one key). Fills, doesn't run.
  const allkw = e.target.closest('[data-allkw]')
  if (allkw) {
    e.stopPropagation()
    const data = appListings[allkw.closest('.app-card')?.dataset.app] || {}
    const kwTerms = String(data.listing?.keywords || '').split(',').map(s => s.trim()).filter(Boolean)
    const iapNames = (data.iapProducts || []).map(p => p.name).filter(Boolean)
    const terms = [...new Set([...kwTerms, ...iapNames])]
    const box = document.getElementById('aso-terms')
    if (box) { box.value = terms.join(', '); box.scrollIntoView({ behavior: 'smooth', block: 'center' }); box.focus() }
    showToast(`${terms.length} keys → research box · hit research`, 2800)
    return
  }
  // "load into all tools" — populate EVERY ASO-lab input from the manifest listing, so you can run
  // any tool / tweak manually without retyping. Fills, never runs (you drive from here).
  const fillAll = e.target.closest('[data-fillall]')
  if (fillAll) {
    e.stopPropagation()
    const data = appListings[fillAll.closest('.app-card')?.dataset.app] || {}
    const L = data.listing || {}
    const set = (id, v) => { const el = document.getElementById(id); if (el) el.value = v || '' }
    const kwTerms = String(L.keywords || '').split(',').map(s => s.trim()).filter(Boolean)
    const iapNames = (data.iapProducts || []).map(p => p.name).filter(Boolean)
    set('lint-title', L.title); set('lint-sub', L.subtitle); set('lint-kw', L.keywords)
    set('comp-title', L.title); set('comp-sub', L.subtitle); set('comp-cands', L.keywords)
    set('aso-terms', [...new Set([...kwTerms, ...iapNames])].join(', '))   // research: keys + IAP names
    set('sug-terms', kwTerms.slice(0, 4).join(', '))                       // suggest: a few seeds (a-z soup is heavy)
    document.querySelector('.aso-adv')?.setAttribute('open', '')           // reveal lint & compose
    document.getElementById('aso-lab')?.scrollIntoView({ behavior: 'smooth', block: 'start' })
    showToast('loaded the listing into every ASO tool — run or tweak below', 3000)
    return
  }
  const btn = e.target.closest('button[data-act]')
  if (btn) {
    if (!window.studio?.buildApp) { showToast('requires the desktop app  (npm start)', 3000); return }
    const app = btn.closest('.app-card')?.dataset.app
    if (!app) return
    const act = btn.dataset.act, label = btn.textContent
    // research (de:meta category terms — "explore my landscape") and analyze (the app's OWN
    // listing terms — "grade my chosen keywords vs the competition") both seed the research box
    // and run the SAME research path; results land in the research lab panel, editable + re-runnable.
    if (act === 'research' || act === 'analyze') {
      const stop = busyDots(btn, act === 'analyze' ? 'reading listing' : 'seeding', label); btn.disabled = true
      const res = await window.studio.appSeeds(app, act === 'analyze' ? 'listing' : 'meta')
      stop(); btn.disabled = false
      if (!res?.ok || !res.terms?.length) {
        showToast(res?.error || (act === 'analyze' ? 'no listing to analyze' : 'no seed terms — add teaches/title to the carts'), 3000)
        return
      }
      const termsEl = document.getElementById('aso-terms')
      if (termsEl) { termsEl.value = res.terms.join(', '); termsEl.scrollIntoView({ behavior: 'smooth', block: 'center' }) }
      asoRun?.click()
      return
    }
    // suggest is heavier per seed (a-z autocomplete soup), so seed from fewer terms
    if (act === 'suggest') {
      const stop = busyDots(btn, 'seeding', label); btn.disabled = true
      const res = await window.studio.appSeeds(app)
      stop(); btn.disabled = false
      if (!res?.ok || !res.terms?.length) { showToast(res?.error || 'no seed terms — add teaches/title to the carts', 3000); return }
      const sugEl = document.getElementById('sug-terms')
      if (sugEl) { sugEl.value = res.terms.slice(0, 4).join(', '); sugEl.scrollIntoView({ behavior: 'smooth', block: 'center' }) }
      sugRun?.click()
      return
    }
    // open the trailer builder (its own Apps-tab section, not the runtime log)
    if (act === 'trailer') { await openTrailer({ kind: 'app', name: app }); return }
    // score the committed listing (--deep hits the network) → scorecard + gotchas in the panel.
    if (act === 'score') {
      const stop = busyDots(btn, 'scoring (fetches difficulty)', label); btn.disabled = true
      await runScore(app)
      stop(); btn.disabled = false
      return
    }
    // leads — tribes + venues + post scaffold per cart, in the same results panel (local, fast)
    if (act === 'leads') {
      const stop = busyDots(btn, 'matching tribes', label); btn.disabled = true
      await runLeads(app)
      stop(); btn.disabled = false
      return
    }
    if (act === 'keywords') { openKeywords({ kind: 'app', name: app }); return }
    // App Store — click 1 of 2: dry-run the metadata diff into a checklist panel (read-only, safe).
    // The push (click 2) lives on the panel's button; nothing goes live from this click.
    if (act === 'store') {
      const stop = busyDots(btn, 'checking App Store', label); btn.disabled = true
      await runStore(app)
      stop(); btn.disabled = false
      return
    }
    // brief writes apps/<app>/seo-brief.md; coverage checks the finished copy — both stream to
    // the aso log panel (they're ASO output, not a build), so route there, not the runtime log.
    if (act === 'brief' || act === 'coverage') {
      const stop = busyDots(btn, act === 'brief' ? 'building (fetches Google/App Store)' : 'checking', label); btn.disabled = true
      asoOut.textContent = ''; if (asoResults) asoResults.innerHTML = ''
      document.getElementById('aso-terms')?.scrollIntoView({ behavior: 'smooth', block: 'center' })
      if (act === 'brief') {
        const res = await window.studio.asoBrief(app)
        stop(); btn.disabled = false
        // clickable link to open the written worksheet (open-path re-checks it's inside the repo)
        if (res?.path && asoResults) asoResults.innerHTML =
          `<a href="#" class="rs-chip" data-openpath="${escHtml(res.path)}">📄 open ${escHtml(res.rel || `apps/${app}/seo-brief.md`)}</a>`
        showToast(`wrote apps/${app}/seo-brief.md`, 3000)
      } else {
        await window.studio.asoCoverage(app)
        stop(); btn.disabled = false
      }
      return
    }
    const stop = busyDots(btn, 'working', label); btn.disabled = true
    rlogClear()
    if (act === 'press') await window.studio.pressKit(app)
    else if (act === 'shots') await window.studio.appShots(app)
    else if (act === 'lint' || act === 'compose') await window.studio.asoApp(app, act)
    else await window.studio.buildApp(app, act)
    stop(); btn.disabled = false
    return
  }
  const card = e.target.closest('.app-card')
  if (card) {
    // a drag to select text (e.g. copying an IAP description into the research box) ends in a
    // click — don't let that collapse the card. Only toggle on a real click, not a text selection.
    if (String(window.getSelection?.() || '').length) return
    const acts = card.querySelector('.app-actions'); if (acts) acts.hidden = !acts.hidden
  }
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

// Gather the current editor state (code + sprites + map + settings) as the chunk
// payload shared by Save As / Save in-place / Save to source.
function gatherCartChunks() {
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
    renderEvery: settings.renderEvery,
  }
  return { source: view.state.doc.toString(), spritesDataUrl, mapBase64, settings: cartSettings }
}

// Read the 128×128 sprite sheet as 64 slots of 256 palette INDICES (row-major
// 16×16), snapping each pixel to the nearest pico32 colour (transparent → 0). This
// is what save-to-source diffs against a generator cart's output to build the
// reversible sprite PATCH (Gap 2 / Option D). Returns null if the canvas is absent
// or unreadable. Nearest-colour (not exact) tolerates any 1-LSB PNG round-trip.
let _palRgb = null
function readSheetSlots() {
  const cv = document.querySelector('#tilemap-canvas')
  if (!cv || cv.width < 128 || cv.height < 128) return null
  let img
  try { img = cv.getContext('2d', { willReadFrequently: true }).getImageData(0, 0, 128, 128) }
  catch { return null }
  const d = img.data
  if (!_palRgb) _palRgb = TL_PAL.map(h => { const n = parseInt(h.slice(1), 16); return [(n >> 16) & 255, (n >> 8) & 255, n & 255] })
  const nearest = (r, g, b) => {
    let bi = 0, bd = Infinity
    for (let i = 0; i < 32; i++) {
      const p = _palRgb[i], dr = r - p[0], dg = g - p[1], db = b - p[2], dd = dr * dr + dg * dg + db * db
      if (dd < bd) { bd = dd; bi = i; if (dd === 0) break }
    }
    return bi
  }
  const slots = new Array(64)
  for (let slot = 0; slot < 64; slot++) {
    const ox = (slot % 8) * 16, oy = Math.floor(slot / 8) * 16
    const arr = new Array(256)
    for (let py = 0; py < 16; py++) for (let px = 0; px < 16; px++) {
      const i = ((oy + py) * 128 + (ox + px)) * 4
      arr[py * 16 + px] = d[i + 3] < 8 ? 0 : nearest(d[i], d[i + 1], d[i + 2])
    }
    slots[slot] = arr
  }
  return slots
}

// Save the open cart. `forceDialog` (Save As) always prompts; otherwise we
// save-in-place when we have a real on-disk origin, else fall through to the
// dialog (fresh / gallery carts have no origin to overwrite).
async function saveCart(forceDialog) {
  if (!window.studio) return
  const { source, spritesDataUrl, mapBase64, settings: cartSettings } = gatherCartChunks()
  const targetPath = (!forceDialog && currentCartPath) ? currentCartPath : undefined
  const saved = await window.studio.saveCart({ source, spritesDataUrl, mapBase64, settings: cartSettings, targetPath })
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

// Save to source: write the Code-tab buffer back to tools/carts/<slug>.c and
// rebake the gallery .cart.png (source only — the .cart.js sprite story stands).
// For repo carts; external/fresh carts have no source and get a clear error.
// NOT a git commit — do that deliberately with `node tools/cart-commit.js <slug>`.
const saveSourceBtn = document.getElementById('save-source-btn')
async function saveToSource() {
  if (!window.studio || !window.studio.saveToSource) { showToast('save to source needs Electron', 3000); return }
  const slug = currentCartFile
  if (!slug) { showToast('open a repo cart first', 2500); return }
  const { source, spritesDataUrl, mapBase64, settings: cartSettings } = gatherCartChunks()
  const spriteSlots = readSheetSlots()   // for the generator-cart reversible sprite patch (Gap 2)
  // busy feedback: a long-lived toast (replaced by the result) + a disabled,
  // relabelled button, so the write+rebake round-trip never looks like a no-op.
  const btn = saveSourceBtn, label = btn ? btn.textContent : ''
  showToast(`saving → tools/carts/${slug}.c …`, 60000)
  if (btn) { btn.disabled = true; btn.textContent = 'saving…' }
  try {
    const res = await window.studio.saveToSource({ slug, source, spritesDataUrl, mapBase64, settings: cartSettings, spriteSlots })
    if (!res || !res.ok) { showToast(res && res.error ? `save to source: ${res.error}` : 'save to source failed', 4500); return }
    let msg = `saved → ${res.cRel} + rebaked`
    if (res.patchedSlots > 0) msg += `  ✎ ${res.patchedSlots} hand-edited sprite slot(s) saved as a reversible patch`
    else if (res.hasGenerator) msg += `  (${slug}.cart.js generator — no sprite edits to patch)`
    if (res.spriteError) msg += `  ⚠ sprite patch: ${res.spriteError}`
    showToast(msg, res.patchedSlots ? 6500 : 3500)
    setSpritePatchBar(res.patchSlots || [])   // reflect the (re)written patch — or clear it if it emptied
    if (res.indexError) console.warn('save to source: index.json regen failed:', res.indexError)
  } catch (e) {
    showToast('save to source failed: ' + ((e && e.message) || e), 4500)
  } finally {
    if (btn) { btn.disabled = false; btn.textContent = label }
  }
}
if (saveSourceBtn) saveSourceBtn.addEventListener('click', saveToSource)

// ── the sprite-patch bar (Gap 2 / Option D) ───────────────────
// Shows which sprite slots are hand-owned (patched over the generator) on the
// loaded cart, and offers a one-click DISCARD (delete the patch + restore the
// generator's sprites). Hidden entirely when there's no patch.
function setSpritePatchBar(slots) {
  const bar  = document.getElementById('sprite-patch-bar')
  const info = document.getElementById('sprite-patch-info')
  if (!bar || !info) return
  const list = Array.isArray(slots) ? slots.slice().map(Number).sort((a, b) => a - b) : []
  if (!list.length) { bar.style.display = 'none'; return }
  info.textContent = `✎ ${list.length} slot${list.length > 1 ? 's' : ''} hand-owned over the generator: ${list.map(s => '#' + s).join(', ')}`
  bar.style.display = ''
}

// parse the de:spritepatch chunk (JSON) into the sorted slot-index list the bar shows
function patchSlotsFromChunk(patchJson) {
  if (!patchJson) return []
  try { return Object.keys(JSON.parse(patchJson).slots || {}).map(Number) } catch { return [] }
}

async function discardSpritePatch() {
  if (!window.studio || !window.studio.discardSpritePatch) { showToast('discard needs Electron', 2500); return }
  const slug = currentCartFile
  if (!slug) { showToast('open a repo cart first', 2500); return }
  if (!confirm(`Discard hand-edits on ${slug} and restore the generator's sprites?\nThis deletes tools/carts/${slug}.sprites.patch.json.`)) return
  const btn = document.getElementById('discard-patch-btn'), label = btn ? btn.textContent : ''
  if (btn) { btn.disabled = true; btn.textContent = 'discarding…' }
  try {
    const res = await window.studio.discardSpritePatch({ slug })
    if (!res || !res.ok) { showToast(res && res.error ? `discard: ${res.error}` : 'discard failed', 4000); return }
    if (res.spritesDataUrl) window.dispatchEvent(new CustomEvent('de:load-sprites', { detail: res.spritesDataUrl }))
    setSpritePatchBar([])
    showToast(`hand-edits discarded — ${slug} restored to its generator sprites`, 3500)
  } catch (e) {
    showToast('discard failed: ' + ((e && e.message) || e), 4000)
  } finally {
    if (btn) { btn.disabled = false; btn.textContent = label }
  }
}
const discardPatchBtn = document.getElementById('discard-patch-btn')
if (discardPatchBtn) discardPatchBtn.addEventListener('click', discardSpritePatch)
// hide the whole cart-actions row (save / load / save to source) if opted out in settings
if (!settings.showCartButtons) {
  const cartActions = document.getElementById('cart-actions')
  if (cartActions) cartActions.style.display = 'none'
}

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
function showToast(msg, ms = 2000, onClick = null) {
  toast.textContent = msg
  toast.classList.add('visible')
  toast.classList.toggle('clickable', !!onClick)   // plain toasts stay click-through; this one accepts the click
  toast.onclick = onClick ? () => { toast.classList.remove('visible'); toast.onclick = null; onClick() } : null
  clearTimeout(toastTimer)
  toastTimer = setTimeout(() => { toast.classList.remove('visible'); toast.onclick = null }, ms)
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
  setSpritePatchBar(patchSlotsFromChunk(cart.spritepatch))   // flag hand-owned slots (Gap 2)
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
  // currentCartFile is the FILE slug (drives the record/replay clip dir), NOT the display title —
  // take it from the chosen file's basename (…/squishy.cart.png → squishy), else a record lands in
  // squishy-lines/. Same fix as the .cart.png drop path above.
  if (cart && cart.ok) { if (cart.name) setCartName(cart.name); currentCartFile = (cart.origin || '').split('/').pop().replace(/\.cart\.png$/i, ''); currentCartPath = cart.origin || ''; applyCart(cart) }
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

// ── drag-and-drop file loading (.png cart) ──
// CAPTURE phase + stopPropagation so a FILE dropped on the code editor is handled here and NOT
// inserted as text by CodeMirror (which grabs the drop at its own element first). Only intercept
// file drags — an internal text drag (types has no 'Files') passes straight through to CodeMirror.
// (Replaying a .rec take is no longer a drop target — open the cart, go to the promote tab, and
// click a take to watch it. docs/design/promote-tab.md.)
const isFileDrag = e => [...(e.dataTransfer?.types || [])].includes('Files')
document.addEventListener('dragover', e => { if (isFileDrag(e)) { e.preventDefault(); e.stopPropagation() } }, true)
// Push a sprite-sheet image (as a data-URL) into the sprite editor and reveal it.
function importSpriteSheet(dataUrl) {
  window.dispatchEvent(new CustomEvent('de:load-sprites', { detail: dataUrl }))
  pixMode = 'sprites'
  switchTab('pixels')
}
const fileToDataUrl = file => new Promise((res, rej) => {
  const r = new FileReader(); r.onload = () => res(r.result); r.onerror = rej; r.readAsDataURL(file)
})

document.addEventListener('drop', async e => {
  if (!isFileDrag(e)) return
  e.preventDefault(); e.stopPropagation()
  const file = e.dataTransfer.files[0]
  if (!file) return
  const name = file.name.toLowerCase()

  // .cart.png → load the whole cart (its embedded source/sprites/settings)
  if (name.endsWith('.cart.png')) {
    const filePath = window.studio.getFilePath(file)
    const cart = await window.studio.loadCartFile(filePath)
    // currentCartFile is the FILE slug (drives the record/replay clip dir), NOT the display title —
    // take it from the dropped filename (squishy.cart.png → squishy), else a record lands in squishy-lines/.
    if (cart && cart.ok) { if (cart.name) setCartName(cart.name); currentCartFile = file.name.replace(/\.cart\.png$/i, ''); currentCartPath = cart.origin || ''; applyCart(cart) }
    return
  }

  // a plain image or an Aseprite file → import into the sprite sheet (the inverse
  // of the ⬇ png / ⬇ ase export buttons). Drawn at native size into the 128×128
  // sheet, so a matching-size sheet round-trips exactly.
  try {
    if (name.endsWith('.png')) {
      importSpriteSheet(await fileToDataUrl(file))
    } else if (name.endsWith('.ase') || name.endsWith('.aseprite')) {
      importSpriteSheet(await aseToDataUrl(await file.arrayBuffer()))
    }
  } catch (err) {
    console.error('sprite import failed:', err)
    alert('Could not import that file as sprites:\n' + (err?.message || err))
  }
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
  const who = result.attached && result.cartName ? `“${result.cartName}”`
            : currentCartName ? `“${currentCartName}”` : 'cart'
  head.textContent = `⏱ profiled ${who} · ${seconds}s`
            + (result.attached ? ' · attached to the running cart (its current state)' : '')
  buildLog.appendChild(head)

  // ── C — frame budget ───────────────────────────────────────────
  if (perf) {
    const fps = Math.round(perf.fps)
    const verdict = fps >= 58 ? 'smooth 60fps' : fps >= 50 ? `~${fps}fps` : `dropping to ~${fps}fps`
    addLine(`  CPU ${perf.workMedian.toFixed(1)}ms/frame typical · ${perf.workP95.toFixed(1)}ms p95`
          + ` · ${Math.round(perf.budgetPct)}% of the 16.6ms budget · ${verdict}`,
            fps >= 58 ? 'build-ok' : 'build-warn')
  } else if (result.perfNote) {
    addLine('  ' + result.perfNote, 'build-warn')
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
  // linkify https URLs (→ system browser) and file:// paths (→ default app, e.g. a press kit)
  const re = /(?:https?|file):\/\/[^\s)]+/g
  let last = 0, m
  while ((m = re.exec(text)) !== null) {
    const url = m[0]
    line.appendChild(document.createTextNode(text.slice(last, m.index)))
    const a = document.createElement('a')
    a.href = url
    a.textContent = url
    a.addEventListener('click', e => {
      e.preventDefault()
      if (url.startsWith('file://')) window.studio?.openPath?.(decodeURI(url.slice('file://'.length)))
      else if (window.studio?.openExternal) window.studio.openExternal(url)
      else window.open(url, '_blank')
    })
    line.appendChild(a)
    last = m.index + url.length
  }
  line.appendChild(document.createTextNode(text.slice(last)))
  // a log line that names a repo output file (a baked clip/reel/still/export) gets a 📂 reveal-in-Finder
  // button — reveal-path takes the repo-relative path as-is. Covers bake/export/screenshot/press lines.
  const pm = text.match(/(?:editor\/public|apps|docs\/media|build|site|tools)\/[^\s)'"]+\.(?:webm|mp4|gif|apng|png|zip|html|app|exe|rec|script|beats)/)
  if (pm && window.studio?.revealPath) {
    const btn = document.createElement('button')
    btn.className = 'rlog-reveal'; btn.textContent = '📂'; btn.title = `reveal in Finder — ${pm[0]}`
    btn.addEventListener('click', () => window.studio.revealPath(pm[0]))
    line.appendChild(document.createTextNode(' ')); line.appendChild(btn)
  }
  // the flight-recorder "session recorded → build/.rec/…" close line gets a ✂ keep-take button —
  // promote the just-closed session into tools/clips (no keyboard / menu needed). keepTake logs a
  // "✓ kept → …" line which itself grows a 🎬 bake + 📂 reveal, so keep → clip is all clicks.
  const km = text.match(/session recorded → (build\/\.rec\/\S+\.rec)/)
  if (km && window.studio?.keepTakeFile) {
    const btn = document.createElement('button')
    btn.className = 'rlog-reveal'; btn.textContent = '✂ keep take'; btn.title = 'keep this take → tools/clips (for a clip or an exact repro)'
    btn.addEventListener('click', async () => {
      btn.disabled = true; btn.textContent = '✂ keeping…'
      try { const r = await window.studio.keepTakeFile(km[1]); btn.textContent = (r && r.ok) ? '✓ kept' : '✗ failed' }
      catch { btn.textContent = '✗ failed' }
    })
    line.appendChild(document.createTextNode(' ')); line.appendChild(btn)
  }
  // a log line showing a `make-gif … --recipe <label>` command (the "clip:" hint after keeping a
  // take / recording) gets a 🎬 bake button that RUNS it in place — record → keep → clip without
  // leaving the console. Output streams back into this same log (the baked .webm line then gets 📂).
  const bm = text.match(/make-gif\.js\s+([a-z0-9_-]+)\s+--recipe\s+([a-z0-9][\w.-]*)/i)
  if (bm && window.studio?.bakeClip) {
    const btn = document.createElement('button')
    btn.className = 'rlog-reveal'; btn.textContent = '🎬 bake'; btn.title = `bake a clip from this take — ${bm[1]}/${bm[2]}`
    btn.addEventListener('click', async () => {
      btn.disabled = true; btn.textContent = '🎬 baking…'
      try { const r = await window.studio.bakeClip(bm[1], bm[2]); btn.textContent = (r && r.ok) ? '✓ baked' : '✗ failed' }
      catch { btn.textContent = '✗ failed' }
    })
    line.appendChild(document.createTextNode(' ')); line.appendChild(btn)
  }
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
  // attach-profiling (Debug menu / ⌘⇧P) runs in main and pushes its result here
  window.studio?.onProfileAttached?.(result => showLog(result))
}

// ── welcome cart ──────────────────────────────────────────────
// On startup, load the configured welcome cart (settings tab → startup).
// Works in both Electron and the browser — loadCartFromUrl has a browser-side
// fallback for parsing carts; only ▶ run requires Electron.
// Kept for historic purposes — the original starter cart, which introduced
// persistent state (S->x) and update() right away. Superseded by the leaner
// EMPTY_TEMPLATE below.
const EMPTY_TEMPLATE_CLASSIC =
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

const EMPTY_TEMPLATE =
`// dreamengine - dream it up, write c, make things, get them out.

#include "studio.h"

void draw() {
    cls(CLR_DARK_BLUE);
    print("hello world here we are!", 10, 10, CLR_PEACH);
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

