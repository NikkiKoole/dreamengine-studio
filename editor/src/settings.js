const DEFAULTS = { screenW: 320, screenH: 200, scale: 4, mapW: 128, mapH: 64, cellW: 16, cellH: 16, touchControls: false, worklet: false, showProfiler: false, showPublish: false, showNetplay: false, showShare: false, recordPlays: true, welcomeCart: 'zoo', backend: 'native', buildMode: 'normal', scaleFilter: 0, renderMode: 'gpu', iosConfig: 'debug' }

// ── key bindings ──────────────────────────────────────────────
// Values are raylib (GLFW) keycodes — letters/digits are ASCII, specials match
// raylib's enum. main.cjs passes these to the build as -DP0_BTN_A=<code> etc.;
// studio.c's btn() reads them (falling back to these same defaults if absent).
// Machine-local (not baked into a .cart.png) — your keyboard, your layout.
//   Player 0: Arrows + Z/X/C/V      Player 1: WASD + J/K/L/;
export const KEYMAP_DEFAULTS = {
  p0: { up: 265, down: 264, left: 263, right: 262, a: 90, b: 88, x: 67, y: 86 },
  p1: { up: 87,  down: 83,  left: 65,  right: 68,  a: 74, b: 75, x: 76, y: 59 },
  pause: 80,   // P — single key for both players; configurable in settings → controls
}

// The bindable keys: [ KeyboardEvent.code, raylib keycode, display label ].
// Keyed on .code (the PHYSICAL key — layout-independent) rather than .key, so the
// left/right modifiers and the numpad are distinct and match how raylib/GLFW
// identify keys. The raylib codes below are raylib's KeyboardKey enum values.
const KEYS = [
  // letters (KeyA..KeyZ → raylib KEY_A=65..KEY_Z=90)
  ...'ABCDEFGHIJKLMNOPQRSTUVWXYZ'.split('').map(c => ['Key' + c, c.charCodeAt(0), c]),
  // top-row digits (Digit0..9 → KEY_ZERO=48..KEY_NINE=57)
  ...'0123456789'.split('').map(d => ['Digit' + d, d.charCodeAt(0), d]),
  // punctuation
  ['Minus', 45, '-'], ['Equal', 61, '='], ['BracketLeft', 91, '['], ['BracketRight', 93, ']'],
  ['Backslash', 92, '\\'], ['Semicolon', 59, ';'], ['Quote', 39, "'"], ['Backquote', 96, '`'],
  ['Comma', 44, ','], ['Period', 46, '.'], ['Slash', 47, '/'],
  // whitespace / editing
  ['Space', 32, 'SPC'], ['Enter', 257, '⏎'], ['Tab', 258, 'TAB'], ['Backspace', 259, '⌫'],
  ['Insert', 260, 'INS'], ['Delete', 261, 'DEL'],
  // arrows
  ['ArrowRight', 262, '→'], ['ArrowLeft', 263, '←'], ['ArrowDown', 264, '↓'], ['ArrowUp', 265, '↑'],
  // navigation
  ['PageUp', 266, 'PgUp'], ['PageDown', 267, 'PgDn'], ['Home', 268, 'Home'], ['End', 269, 'End'],
  // locks / system
  ['CapsLock', 280, 'Caps'], ['ScrollLock', 281, 'ScrLk'], ['NumLock', 282, 'NumLk'],
  ['PrintScreen', 283, 'PrtSc'], ['Pause', 284, 'Pause'],
  // function keys (F1..F12 → 290..301)
  ...Array.from({ length: 12 }, (_, i) => ['F' + (i + 1), 290 + i, 'F' + (i + 1)]),
  // numpad (Numpad0..9 → KEY_KP_0=320..KEY_KP_9=329)
  ...Array.from({ length: 10 }, (_, i) => ['Numpad' + i, 320 + i, 'KP' + i]),
  ['NumpadDecimal', 330, 'KP.'], ['NumpadDivide', 331, 'KP/'], ['NumpadMultiply', 332, 'KP*'],
  ['NumpadSubtract', 333, 'KP-'], ['NumpadAdd', 334, 'KP+'], ['NumpadEnter', 335, 'KP⏎'], ['NumpadEqual', 336, 'KP='],
  // modifiers (left/right distinct → KEY_LEFT_SHIFT=340 .. KEY_KB_MENU=348)
  ['ShiftLeft', 340, 'LShift'], ['ControlLeft', 341, 'LCtrl'], ['AltLeft', 342, 'LAlt'], ['MetaLeft', 343, 'LSuper'],
  ['ShiftRight', 344, 'RShift'], ['ControlRight', 345, 'RCtrl'], ['AltRight', 346, 'RAlt'], ['MetaRight', 347, 'RSuper'],
  ['ContextMenu', 348, 'Menu'],
]
const CODE_TO_KEY = new Map(KEYS.map(([code, ray, label]) => [code, { code: ray, label }]))
const RAYLIB_LABEL = new Map(KEYS.map(([, ray, label]) => [ray, label]))

// browser KeyboardEvent → { code, label } in raylib terms, or null if we don't bind it
function eventToKey(e) {
  return CODE_TO_KEY.get(e.code) || null
}

// stored raylib keycode → display label
export function keyLabel(code) {
  return RAYLIB_LABEL.get(code) || ('#' + code)
}

function loadKeymap() {
  try {
    const raw = JSON.parse(localStorage.getItem('keymap'))
    if (raw && raw.p0 && raw.p1) {
      // merge over defaults so a partial/old blob still yields all keys
      return { p0: { ...KEYMAP_DEFAULTS.p0, ...raw.p0 }, p1: { ...KEYMAP_DEFAULTS.p1, ...raw.p1 },
               pause: Number.isInteger(raw.pause) ? raw.pause : KEYMAP_DEFAULTS.pause }
    }
  } catch {}
  return { p0: { ...KEYMAP_DEFAULTS.p0 }, p1: { ...KEYMAP_DEFAULTS.p1 }, pause: KEYMAP_DEFAULTS.pause }
}

function load() {
  return {
    screenW:       parseInt(localStorage.getItem('screenW') || DEFAULTS.screenW),
    screenH:       parseInt(localStorage.getItem('screenH') || DEFAULTS.screenH),
    scale:         parseInt(localStorage.getItem('scale')   || DEFAULTS.scale),
    mapW:          parseInt(localStorage.getItem('mapW')    || DEFAULTS.mapW),
    mapH:          parseInt(localStorage.getItem('mapH')    || DEFAULTS.mapH),
    cellW:         parseInt(localStorage.getItem('cellW')   || DEFAULTS.cellW),
    cellH:         parseInt(localStorage.getItem('cellH')   || DEFAULTS.cellH),
    touchControls: localStorage.getItem('touchControls') === '1',
    worklet:       localStorage.getItem('worklet') === '1',
    showProfiler:  localStorage.getItem('showProfiler') === '1',
    showPublish:   localStorage.getItem('showPublish') === '1',
    showNetplay:   localStorage.getItem('showNetplay') === '1',
    showShare:     localStorage.getItem('showShare') === '1',
    hideCartButtons: localStorage.getItem('hideCartButtons') === '1',   // hide save/load/save-to-source (default: shown)
    recordPlays:   localStorage.getItem('recordPlays') !== '0',   // flight recorder: default ON
    welcomeCart:   localStorage.getItem('welcomeCart') || 'zoo',
    backend:       localStorage.getItem('backend')   || DEFAULTS.backend,
    buildMode:     localStorage.getItem('buildMode') || DEFAULTS.buildMode,
    scaleFilter:   parseInt(localStorage.getItem('scaleFilter') || DEFAULTS.scaleFilter),
    renderMode:    localStorage.getItem('renderMode') || DEFAULTS.renderMode,
    keymap:        loadKeymap(),
  }
}

function save(key, val) {
  localStorage.setItem(key, val)
}

export const settings = load()

// Apply a cart's embedded settings — or reset to safe defaults when the cart
// has none (older carts). Loading a cart should run it at the config it was
// authored for, not whatever globals the user last tinkered with.
export function applyCartSettings(obj) {
  for (const k of ['screenW', 'screenH', 'scale', 'cellW', 'cellH', 'mapW', 'mapH']) {
    const v = obj && obj[k] != null ? obj[k] : DEFAULTS[k]
    settings[k] = v
    save(k, v)
  }
}

export function buildSettingsPanel(el) {
  el.innerHTML = ''

  // ── screen size ──────────────────────────────────────────────
  const screenSection = section('screen')
  screenSection.appendChild(row(
    field('width',  'number', settings.screenW, v => { settings.screenW = v; save('screenW', v) }),
    field('height', 'number', settings.screenH, v => { settings.screenH = v; save('screenH', v) }),
    field('scale',  'number', settings.scale,   v => { settings.scale   = v; save('scale', v)   }),
  ))
  el.appendChild(screenSection)

  // ── scaling (present filter) ─────────────────────────────────
  const scaleSection = section('scaling')
  scaleSection.appendChild(select(
    'how the canvas scales to the window',
    [
      { value: '0', label: 'crisp — nearest (default)' },
      { value: '1', label: 'smooth — bilinear' },
      { value: '2', label: 'sharp — anti-aliased pixels' },
      { value: '3', label: 'sharp + gamma-correct' },
    ],
    String(settings.scaleFilter),
    v => { settings.scaleFilter = parseInt(v); save('scaleFilter', v) },
  ))
  scaleSection.appendChild(note('only changes anything at a NON-INTEGER scale — the web build fit to the browser, fullscreen, a resizable window. At the editor’s integer scale all four look identically crisp. "sharp" keeps pixels crisp but anti-aliases the 1px seam at pixel edges, so fractional scaling neither shimmers (like nearest) nor blurs (like bilinear); "+gamma" blends that seam in linear light so bright edges keep their brightness. Free (one GPU pass). Takes effect on the next ▶ run; native + live, desktop. See the "Pixel-Perfect Scaling" demo cart.'))
  el.appendChild(scaleSection)

  // ── controls (key bindings) ──────────────────────────────────
  // Click a key, then press the key you want. Stored as raylib keycodes in
  // settings.keymap and shipped to the build by main.cjs; btn() honors them.
  const keysSection = section('controls')

  const BTNS = ['up', 'down', 'left', 'right', 'a', 'b', 'x', 'y']
  const PLAYERS = [['p0', 'player 0'], ['p1', 'player 1']]
  const cells = { p0: {}, p1: {} }   // pid → btn → button element, for the reset button

  const table = document.createElement('div')
  table.className = 'key-table'

  const header = document.createElement('div')
  header.className = 'key-row key-header'
  header.innerHTML = '<span></span><span>↑</span><span>↓</span><span>←</span><span>→</span><span>A</span><span>B</span><span>X</span><span>Y</span>'
  table.appendChild(header)

  let armed = null   // { cell, pid, btn, prevLabel } while capturing a keypress

  function disarm() {
    if (!armed) return
    armed.cell.classList.remove('key-armed')
    armed.cell.textContent = armed.prevLabel
    armed = null
    window.removeEventListener('keydown', onKey, true)
  }

  function onKey(e) {
    if (!armed) return
    e.preventDefault(); e.stopPropagation()
    if (e.key === 'Escape') { disarm(); return }
    const k = eventToKey(e)
    if (!k) return   // key we don't bind — stay armed, let them try again
    settings.keymap[armed.pid][armed.btn] = k.code
    save('keymap', JSON.stringify(settings.keymap))
    armed.cell.classList.remove('key-armed')
    armed.cell.textContent = k.label
    armed = null
    window.removeEventListener('keydown', onKey, true)
  }

  PLAYERS.forEach(([pid, label]) => {
    const r = document.createElement('div')
    r.className = 'key-row'
    const name = document.createElement('span')
    name.className = 'key-player'
    name.textContent = label
    r.appendChild(name)
    BTNS.forEach(btn => {
      const cell = document.createElement('button')
      cell.className = 'key'
      cell.textContent = keyLabel(settings.keymap[pid][btn])
      cell.title = `${label} ${btn.toUpperCase()} — click, then press a key`
      cell.addEventListener('click', () => {
        const wasArmed = armed && armed.cell === cell
        disarm()
        if (wasArmed) return   // clicking the armed cell again just cancels
        armed = { cell, pid, btn, prevLabel: cell.textContent }
        cell.classList.add('key-armed')
        cell.textContent = '…'
        window.addEventListener('keydown', onKey, true)
      })
      cells[pid][btn] = cell
      r.appendChild(cell)
    })
    table.appendChild(r)
  })

  keysSection.appendChild(table)

  // pause key row — single key for both players
  const pauseRow = document.createElement('div')
  pauseRow.className = 'key-row'
  const pauseLabel = document.createElement('span')
  pauseLabel.className = 'key-player'
  pauseLabel.textContent = 'pause'
  const pauseCell = document.createElement('button')
  pauseCell.className = 'key'
  pauseCell.textContent = keyLabel(settings.keymap.pause)
  pauseCell.title = 'pause menu key — click, then press a key'
  pauseCell.addEventListener('click', () => {
    const wasArmed = armed && armed.cell === pauseCell
    disarm()
    if (wasArmed) return
    armed = { cell: pauseCell, pid: null, btn: 'pause', prevLabel: pauseCell.textContent }
    pauseCell.classList.add('key-armed')
    pauseCell.textContent = '…'
    window.addEventListener('keydown', e => {
      if (!armed) return
      e.preventDefault(); e.stopPropagation()
      if (e.key === 'Escape') { disarm(); return }
      const k = eventToKey(e)
      if (!k) return
      settings.keymap.pause = k.code
      save('keymap', JSON.stringify(settings.keymap))
      pauseCell.classList.remove('key-armed')
      pauseCell.textContent = k.label
      armed = null
    }, { once: false, capture: true })
  })
  pauseRow.appendChild(pauseLabel)
  pauseRow.appendChild(pauseCell)
  keysSection.appendChild(pauseRow)

  const reset = document.createElement('button')
  reset.className = 'settings-reset'
  reset.textContent = 'reset to defaults'
  reset.addEventListener('click', () => {
    disarm()
    settings.keymap = { p0: { ...KEYMAP_DEFAULTS.p0 }, p1: { ...KEYMAP_DEFAULTS.p1 }, pause: KEYMAP_DEFAULTS.pause }
    save('keymap', JSON.stringify(settings.keymap))
    for (const [pid] of PLAYERS)
      for (const btn of BTNS) cells[pid][btn].textContent = keyLabel(settings.keymap[pid][btn])
    pauseCell.textContent = keyLabel(settings.keymap.pause)
  })
  keysSection.appendChild(reset)
  keysSection.appendChild(note('click a slot, then press any key to bind it — letters, digits, arrows, numpad (KP8…), function keys, and modifiers (LShift, RAlt…) all work. Esc cancels. machine-local — applies to every cart you run; carts read these via btn(), or use key() for raw keypresses. takes effect on the next ▶ run.'))
  el.appendChild(keysSection)

  // ── touch controls ───────────────────────────────────────────
  const touchSection = section('touch')
  touchSection.appendChild(checkbox(
    'show on-screen stick + A/B buttons',
    settings.touchControls,
    v => { settings.touchControls = v; save('touchControls', v ? '1' : '0') },
  ))
  touchSection.appendChild(note('floating stick on the left, A/B on the right. carts can override with touch_controls(true/false).'))
  el.appendChild(touchSection)

  // ── audio (web) ──────────────────────────────────────────────
  const audioSection = section('audio (web)')
  audioSection.appendChild(checkbox(
    'AudioWorklet backend (build for web / publish)',
    settings.worklet,
    v => { settings.worklet = v; save('worklet', v ? '1' : '0') },
  ))
  audioSection.appendChild(note('dedicated-thread, sample-tight web audio (vs the main-thread ScriptProcessor). Ships a worklet build + a fallback + auto-pick loader, like instrument carts get automatically via build-site. Needs a cross-origin-isolated host to activate; falls back otherwise.'))
  el.appendChild(audioSection)

  // ── startup ──────────────────────────────────────────────────
  const startupSection = section('startup')
  startupSection.appendChild(select(
    'welcome cart',
    [
      { value: 'zoo',   label: 'pixel zoo (default)' },
      { value: 'empty', label: 'empty template' },
    ],
    settings.welcomeCart,
    v => { settings.welcomeCart = v; save('welcomeCart', v) },
  ))
  el.appendChild(startupSection)

  // ── run mode (▶ backend) ──────────────────────────────────────
  const backendSection = section('run mode')
  backendSection.appendChild(select(
    'how ▶ run compiles the cart',
    [
      { value: 'native', label: 'native (clang) — optimised, default' },
      { value: 'live',   label: 'live (libtcc) — instant hot-reload' },
    ],
    settings.backend,
    v => { settings.backend = v; save('backend', v) },
  ))
  backendSection.appendChild(select(
    'build mode (native only)',
    [
      { value: 'normal',  label: 'normal — profiler + inspection hooks on' },
      { value: 'release', label: 'release — stripped, -O2, no overhead' },
    ],
    settings.buildMode,
    v => { settings.buildMode = v; save('buildMode', v) },
  ))
  backendSection.appendChild(note('native: a fresh optimised build each run. live: a persistent libtcc host JIT-compiles the cart and hot-reloads it on every run/save — game state in de_state() survives the swap. desktop app only; sprite/screen changes relaunch the live window. "Build for web" is unaffected.'))
  el.appendChild(backendSection)

  // ── deploy to iPhone (📱 button) ──────────────────────────────
  const iosSection = section('deploy to iPhone')
  iosSection.appendChild(select(
    'build config for the device',
    [
      { value: 'debug',   label: 'Debug — perf overlay + inspection hooks on' },
      { value: 'release', label: 'Release — optimised, clean screen (to play/hand off)' },
    ],
    settings.iosConfig,
    v => { settings.iosConfig = v; save('iosConfig', v) },
  ))
  iosSection.appendChild(note('the 📱 to iPhone button signs the LIVE editor cart and launches it on a connected device (~90s). Debug is the dev build — unoptimised engine with the on-screen perf overlay. Release is stripped + -O2 for real performance and a clean screen, for when you’re handing the cart to someone to play. Needs Electron + an unlocked, signing-set-up iPhone (see ios/device.sh).'))
  el.appendChild(iosSection)

  // ── rendering (GPU vs software canvas) ────────────────────────
  const renderSection = section('rendering')
  renderSection.appendChild(select(
    'render backend',
    [
      { value: 'gpu',      label: 'hardware (GPU) — immediate-mode, default' },
      { value: 'software', label: 'software (CPU canvas) — rasterise on the CPU' },
    ],
    settings.renderMode,
    v => { settings.renderMode = v; save('renderMode', v) },
  ))
  renderSection.appendChild(note('software draws the whole screen into a CPU framebuffer and uploads it once per frame (DE_SOFTWARE_CANVAS). It wins 2–3.5× on pset/fill-bound carts (pixel art, top-down painters) and is a wash on GPU/draw-call-bound ones; carts that rotate the whole view (camera_ex angle) auto-fall back to the GPU regardless. A runtime switch — no recompile — takes effect on the next ▶ run (live: relaunches the window).'))
  el.appendChild(renderSection)

  // ── profiler (advanced) ───────────────────────────────────────
  const profSection = section('profiler')
  profSection.appendChild(checkbox(
    'show ⏱ profile button in the toolbar',
    settings.showProfiler,
    v => {
      settings.showProfiler = v
      save('showProfiler', v ? '1' : '0')
      const btn = document.getElementById('profile-btn')
      if (btn) btn.style.display = v ? '' : 'none'
    },
  ))
  profSection.appendChild(note('runs the cart for a few seconds, samples it with macOS `sample`, and lists which functions burned the most CPU — straight into the build log. desktop app only. (to profile a cart you’re already playing, use the “Debug” menu in the macOS menu bar / ⌘⇧P — always available.)'))
  profSection.appendChild(checkbox(
    'record every play (flight recorder)',
    settings.recordPlays,
    v => { settings.recordPlays = v; save('recordPlays', v ? '1' : '0') },
  ))
  profSection.appendChild(note('every ▶ Run is recorded deterministically to a rolling scratch ring (build/.rec/, thrown away automatically). “Keep take” in the Debug menu / ⌘⇧K promotes the current session to tools/clips for a clip or an exact bug repro. Makes runs deterministic (fixed timestep + seed); disable for wall-clock-sensitive carts. desktop app only.'))
  el.appendChild(profSection)

  // (recording is triggered from the Promote tab's ● record a take — no toolbar toggle.)

  // ── share (advanced) ──────────────────────────────────────────
  const shareSection = section('share')
  shareSection.appendChild(checkbox(
    'show ⇪ share button in the toolbar',
    settings.showShare,
    v => {
      settings.showShare = v
      save('showShare', v ? '1' : '0')
      const btn = document.getElementById('share-btn')
      if (btn) btn.style.display = v ? '' : 'none'
    },
  ))
  shareSection.appendChild(note('one popover with every way to get the current cart out of the editor — export, send to a device, publish.'))
  el.appendChild(shareSection)

  // ── publish to site (advanced) ────────────────────────────────
  const pubSection = section('publish to site')
  pubSection.appendChild(checkbox(
    'show \u{1F680} publish button in the cart panel',
    settings.showPublish,
    v => {
      settings.showPublish = v
      save('showPublish', v ? '1' : '0')
      const btn = document.getElementById('publish-btn')
      if (btn) btn.style.display = v ? '' : 'none'
    },
  ))
  pubSection.appendChild(note('\u26A0 publishes the CURRENT cart to the PUBLIC internet. One click: compiles to wasm, writes the C source back to tools/carts/<name>.c, COMMITS to the git repo (github.com/NikkiKoole/dreamengine) and PUSHES to master \u2014 live ~1 min later at nikkikoole.github.io/dreamengine/<name>/. The cart needs a name (load one from the gallery, or save yours first). Sprites drawn in the sprite editor ship with the build but are NOT written back to a .cart.js generator \u2014 see the "sprite story" in docs/STATUS.md. desktop app only.'))
  el.appendChild(pubSection)

  // \u2500\u2500 multiplayer (netplay, advanced) \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
  const netSection = section('multiplayer')
  netSection.appendChild(checkbox(
    'show \u{1F310} multiplayer button in the toolbar',
    settings.showNetplay,
    v => {
      settings.showNetplay = v
      save('showNetplay', v ? '1' : '0')
      const btn = document.getElementById('net-btn')
      if (btn) btn.style.display = v ? '' : 'none'
    },
  ))
  netSection.appendChild(note('lockstep netplay over the LAN \u2014 host on one machine, join by IP from another, and both drive the same cart (P0 = host, P1 = joiner). Experimental; carts stay network-unaware. desktop app only.'))
  el.appendChild(netSection)

  // \u2500\u2500 cart panel \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
  const cartPanelSection = section('cart panel')
  cartPanelSection.appendChild(checkbox(
    'hide the save / load / save to source buttons',
    settings.hideCartButtons,
    v => {
      settings.hideCartButtons = v
      save('hideCartButtons', v ? '1' : '0')
      const row = document.getElementById('cart-actions')
      if (row) row.style.display = v ? 'none' : ''
    },
  ))
  cartPanelSection.appendChild(note('the save cart / load cart / save to source buttons at the top of the tutorials panel. Rarely needed day-to-day \u2014 hide them for a cleaner panel; carts still load from the gallery and everything else keeps working.'))
  el.appendChild(cartPanelSection)
}

function section(title) {
  const el = document.createElement('div')
  el.className = 'settings-section'
  el.innerHTML = `<div class="help-section-title">${title}</div>`
  return el
}

function row(...children) {
  const el = document.createElement('div')
  el.className = 'settings-row'
  children.forEach(c => el.appendChild(c))
  return el
}

function field(label, type, value, onChange) {
  const wrap = document.createElement('label')
  wrap.className = 'settings-field'
  const input = document.createElement('input')
  input.type = type
  input.value = value
  if (type === 'number') { input.min = 1; input.style.width = '64px' }
  input.addEventListener('change', e => onChange(parseInt(e.target.value)))
  wrap.innerHTML = `<span>${label}</span>`
  wrap.appendChild(input)
  return wrap
}

function checkbox(label, value, onChange) {
  const wrap = document.createElement('label')
  wrap.className = 'settings-field'
  const input = document.createElement('input')
  input.type = 'checkbox'
  input.checked = value
  input.addEventListener('change', e => onChange(e.target.checked))
  wrap.appendChild(input)
  const span = document.createElement('span')
  span.textContent = label
  wrap.appendChild(span)
  return wrap
}

function select(label, options, value, onChange) {
  const wrap = document.createElement('label')
  wrap.className = 'settings-field'
  const sel = document.createElement('select')
  options.forEach(({ value: v, label: l }) => {
    const opt = document.createElement('option')
    opt.value = v
    opt.textContent = l
    if (v === value) opt.selected = true
    sel.appendChild(opt)
  })
  sel.addEventListener('change', e => onChange(e.target.value))
  const span = document.createElement('span')
  span.textContent = label
  wrap.appendChild(span)
  wrap.appendChild(sel)
  return wrap
}

function note(text) {
  const el = document.createElement('div')
  el.className = 'settings-note'
  el.textContent = text
  return el
}
