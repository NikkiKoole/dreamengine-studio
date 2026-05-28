import './shell.css'
import { view } from './main.js'
import './sprite-editor.js'
import { studioDocs } from './studioDocs.js'
import { settings, buildSettingsPanel } from './settings.js'

// ── tab switching ─────────────────────────────────────────────
document.querySelectorAll('.tab').forEach(btn => {
  btn.addEventListener('click', () => {
    if (btn.disabled) return
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'))
    document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'))
    btn.classList.add('active')
    document.getElementById('panel-' + btn.dataset.tab).classList.add('active')
  })
})

// ── help panel ───────────────────────────────────────────────
const sections = [
  { title: 'callbacks',  keys: ['update', 'draw'] },
  { title: 'graphics',   keys: ['cls', 'spr', 'print', 'rect', 'rectfill', 'circle', 'circlefill', 'line', 'pixel'] },
  { title: 'input',      keys: ['btn', 'BTN_UP', 'BTN_DOWN', 'BTN_LEFT', 'BTN_RIGHT', 'BTN_A', 'BTN_B'] },
  { title: 'screen',     keys: ['SCREEN_W', 'SCREEN_H'] },
  { title: 'palette',    keys: ['CLR_BLACK', 'CLR_WHITE', 'CLR_RED', 'CLR_GREEN', 'CLR_BLUE'] },
]

const helpPanel = document.getElementById('panel-help')
sections.forEach(({ title, keys }) => {
  const section = document.createElement('div')
  section.className = 'help-section'
  section.innerHTML = `<div class="help-section-title">${title}</div>`
  keys.forEach(key => {
    const entry = studioDocs[key]
    if (!entry) return
    const row = document.createElement('div')
    row.className = 'help-entry'
    row.innerHTML = `
      <div class="help-sig">${entry.sig}</div>
      <div class="help-doc">${entry.doc.replace(/\n/g, '<br>')}</div>
    `
    section.appendChild(row)
  })
  helpPanel.appendChild(section)
})

// ── settings panel ───────────────────────────────────────────
buildSettingsPanel(document.getElementById('panel-settings'))

// ── run button ────────────────────────────────────────────────
const runBtn  = document.getElementById('run-btn')
const buildLog = document.getElementById('build-log')

runBtn.addEventListener('click', async () => {
  if (!window.studio) {
    showLog({ ok: false, cmd: null, output: 'run requires the desktop app  (npm start)' })
    return
  }

  const code = view.state.doc.toString()
  runBtn.textContent = '⏳ compiling…'
  runBtn.disabled = true
  buildLog.style.display = 'none'

  // export sprite sheet from editor canvas before compiling
  const tilemapCanvas = document.querySelector('#tilemap-canvas')
  if (tilemapCanvas) {
    await window.studio.saveSprites(tilemapCanvas.toDataURL('image/png'))
  }

  const result = await window.studio.run(code, settings)

  runBtn.textContent = '▶ run'
  runBtn.disabled = false

  showLog(result)
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
  }
}
