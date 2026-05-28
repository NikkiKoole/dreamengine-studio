const DEFAULTS = { screenW: 320, screenH: 200, scale: 4 }

function load() {
  return {
    screenW: parseInt(localStorage.getItem('screenW') || DEFAULTS.screenW),
    screenH: parseInt(localStorage.getItem('screenH') || DEFAULTS.screenH),
    scale:   parseInt(localStorage.getItem('scale')   || DEFAULTS.scale),
  }
}

function save(key, val) {
  localStorage.setItem(key, val)
}

export const settings = load()

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

function note(text) {
  const el = document.createElement('div')
  el.className = 'settings-note'
  el.textContent = text
  return el
}
