// navigate.js — read-only engine-source viewer (lives in the DOCS tab).
//
// The runtime's C files — studio.h, the cart-land library headers (ui.h,
// gestures.h, pointer.h, cursor.h, improv.h, radio.h, keybed.h, solo.h,
// shadermath.h, cards.h, ampcab.h), spec.h (the gameplay-test harness API),
// sound.h, studio.c — are readable in-editor
// under the docs sidebar's "engine source" group. Cmd/ctrl-clicking the
// filename of an `#include "ui.h"` in your cart does the same thing: it
// switches to the docs tab with that file open (no overlay over your code —
// the code tab is always just your cart).
//
// Served by the vite /runtime-src/ route, so it works in both Electron and
// the browser tab. Clicks keep working inside the viewer: includes chain to
// other headers, documented symbols jump to the API reference.
//
// Wiring: openFileViewer(file) (called from main.js's cmd-click dispatch and
// from the viewer's own chained clicks) dispatches an 'engine-source' event;
// shell.js owns the docs tab, listens, switches tabs, and mounts the viewer
// via showEngineFileIn(container, file).

import { EditorState, Compartment } from '@codemirror/state'
import { EditorView, lineNumbers, highlightActiveLine } from '@codemirror/view'
import { cpp } from '@codemirror/lang-cpp'
import { oneDark } from '@codemirror/theme-one-dark'
import { dayTheme } from './dayTheme.js'
import { studioDocs } from './studioDocs.js'
import { flashField, flashRange } from './main.js'

// the files offered in the sidebar group, curated order: the public API,
// then the cart-land library headers, then engine internals
export const ENGINE_SOURCES = [
  // studio.h first, then the cart-land library headers (the capabilities the engine
  // deliberately doesn't own — ADR-0006), then spec.h (the gameplay-test harness a
  // cart writes spec() against), then sound.h + studio.c. Generated/data headers
  // (font *_data.h, studio_tcc_symbols.h) and internal ones (platform.h, midi_input.h,
  // raylib_compat, stb_image) are left out.
  'studio.h',
  'ui.h', 'gestures.h', 'pointer.h', 'cursor.h',
  'improv.h', 'radio.h', 'keybed.h', 'solo.h',
  'shadermath.h', 'cards.h', 'ampcab.h',
  'spec.h',
  'sound.h', 'studio.c',
]

let vview = null
const vTheme = new Compartment()

function themeExt() {
  return document.documentElement.classList.contains('day') ? dayTheme : oneDark
}

// follow the day/night toggle without touching shell.js: applyTheme() flips a
// class on <html>, which is all we need to observe
new MutationObserver(() => {
  if (vview) vview.dispatch({ effects: vTheme.reconfigure(themeExt()) })
}).observe(document.documentElement, { attributes: true, attributeFilter: ['class'] })

// cmd/ctrl-click inside the viewer: includes chain, documented symbols → API reference
const viewerClicks = EditorView.domEventHandlers({
  click(event, v) {
    if (!event.metaKey && !event.ctrlKey) return false
    const pos = v.posAtCoords({ x: event.clientX, y: event.clientY })
    if (pos === null) return false

    const line = v.state.doc.lineAt(pos)
    const inc = /#include\s*"([^"]+)"/.exec(line.text)
    if (inc) {
      const from = line.from + inc.index + inc[0].indexOf('"')
      const to   = line.from + inc.index + inc[0].length
      if (pos >= from && pos <= to) {
        openFileViewer(inc[1])
        event.preventDefault()
        return true
      }
    }

    const word = v.state.wordAt(pos)
    if (!word) return false
    const name = v.state.sliceDoc(word.from, word.to)
    if (studioDocs[name]) {
      window.dispatchEvent(new CustomEvent('help-jump', { detail: { key: name } }))
      event.preventDefault()
      return true
    }
    return false
  },
})

function makeViewerState(docText) {
  return EditorState.create({
    doc: docText,
    extensions: [
      lineNumbers(),
      highlightActiveLine(),
      cpp(),
      EditorState.readOnly.of(true),
      EditorView.editable.of(false),
      viewerClicks,
      flashField,        // kept so future jump+flash affordances work here too
      vTheme.of(themeExt()),
    ],
  })
}

// Ask the docs tab to show `file` (e.g. 'gestures.h'). Quote-includes only —
// <math.h> and friends are system headers, not ours. shell.js listens.
export function openFileViewer(file) {
  window.dispatchEvent(new CustomEvent('engine-source', { detail: { file } }))
}

// ── outline for the viewer ──────────────────────────────────────
// Same idea as outline.js (the cart's function list), but for engine source.
// Engine files are mostly DECLARATIONS (studio.h: `void spr(...);`) as well as
// DEFINITIONS (sound.h/studio.c: `static void foo(...) {`), so the regex ends
// on `;` OR `{` — where the cart outline only matches definitions. Same
// heuristic, column-0-anchored, house style (no real parser).
const SYM_RE = /^(?:static\s+)?(?:inline\s+)?(?:unsigned\s+|signed\s+)?(int|float|bool|void|char|long|short|double|[A-Z]\w*)(\s+\*?\s*|\s*\*\s*)([A-Za-z_]\w*)\s*\([^;{)]*\)\s*[;{]/gm
const NOT_FUNCS = new Set(['if', 'for', 'while', 'switch', 'return', 'sizeof', 'else', 'typedef'])

function parseSymbols(doc) {
  const out = []
  const seen = new Set()
  SYM_RE.lastIndex = 0
  let m
  while ((m = SYM_RE.exec(doc))) {
    const ret = m[1]
    const name = m[3]
    if (NOT_FUNCS.has(name) || seen.has(name)) continue
    seen.add(name)
    const namePos = m.index + m[0].indexOf(name, ret.length)
    out.push({ name, ret, pos: namePos })
  }
  return out
}

// jump the viewer to a symbol (readOnly view — selection + scroll still work)
function jumpInViewer(pos, len) {
  if (!vview) return
  vview.dispatch({
    selection: { anchor: pos, head: pos },
    effects: EditorView.scrollIntoView(pos, { y: 'start', yMargin: 48 }),
  })
  flashRange(vview, pos, pos + len)
}

// Populate `listEl` with the current viewer file's functions (click to jump).
// Called by shell.js right after showEngineFileIn mounts a file.
export function renderEngineOutline(listEl) {
  if (!listEl) return
  const syms = vview ? parseSymbols(vview.state.doc.toString()) : []
  listEl.innerHTML = ''
  if (!syms.length) {
    const empty = document.createElement('div')
    empty.className = 'outline-empty'
    empty.textContent = 'no functions'
    listEl.appendChild(empty)
    return
  }
  for (const s of syms) {
    const btn = document.createElement('button')
    btn.className = 'outline-item'
    btn.innerHTML = `<span class="outline-ret">${s.ret}</span> <span class="outline-name">${s.name}</span>`
    btn.title = `jump to ${s.name}`
    btn.addEventListener('click', () => jumpInViewer(s.pos, s.name.length))
    listEl.appendChild(btn)
  }
}

// Mount/refresh the viewer inside `container` (the docs content pane) showing
// `file`. The single EditorView is reused and re-parented across calls.
export async function showEngineFileIn(container, file) {
  let text
  try {
    const r = await fetch('/runtime-src/' + encodeURIComponent(file))
    if (!r.ok) throw new Error()
    text = await r.text()
  } catch {
    text = `// ${file} — not found in runtime/\n// (only the engine's own source files can be viewed here)`
  }
  if (!vview) vview = new EditorView({ state: makeViewerState(text) })
  else vview.setState(makeViewerState(text))
  container.appendChild(vview.dom)
}
