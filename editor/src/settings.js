const DEFAULTS = { screenW: 320, screenH: 200, scale: 4, mapW: 128, mapH: 64, cellW: 16, cellH: 16, touchControls: false }

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

  // ── key layouts ──────────────────────────────────────────────
  const keysSection = section('keyboard')

  const layouts = [
    { player: 'player 1', up: 'W', down: 'S', left: 'A', right: 'D', a: 'Z', b: 'X' },
    { player: 'player 2', up: '↑', down: '↓', left: '←', right: '→', a: ',', b: '.' },
  ]

  const table = document.createElement('div')
  table.className = 'key-table'

  const header = document.createElement('div')
  header.className = 'key-row key-header'
  header.innerHTML = '<span></span><span>↑</span><span>↓</span><span>←</span><span>→</span><span>A</span><span>B</span>'
  table.appendChild(header)

  layouts.forEach(l => {
    const row = document.createElement('div')
    row.className = 'key-row'
    row.innerHTML = `
      <span class="key-player">${l.player}</span>
      <span class="key">${l.up}</span>
      <span class="key">${l.down}</span>
      <span class="key">${l.left}</span>
      <span class="key">${l.right}</span>
      <span class="key">${l.a}</span>
      <span class="key">${l.b}</span>
    `
    table.appendChild(row)
  })

  keysSection.appendChild(table)
  keysSection.appendChild(note('key remapping coming soon'))
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

function note(text) {
  const el = document.createElement('div')
  el.className = 'settings-note'
  el.textContent = text
  return el
}
