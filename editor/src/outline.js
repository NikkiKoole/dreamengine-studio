// ── outline panel ─────────────────────────────────────────────
// A live list of the functions defined in the current cart (VS Code calls this
// the "Outline" view). Click an entry to jump the editor to that function. The
// parsing is regex-based, in the same house style as the go-to-definition logic
// in main.js — carts are single-file C, usually short, so a heuristic scan is
// plenty and never blocks on a real parser.

import { EditorView } from '@codemirror/view'
import { view, flashRange, onDocChange } from './main.js'

// Top-level function definitions: an unindented `[static] [inline] type [*] name(args) {`.
// Anchoring at column 0 (^ with no leading whitespace) is what separates a real
// definition from a call or an `if (...) {` inside a body — those are indented.
const FUNC_RE = /^(?:static\s+)?(?:inline\s+)?(?:unsigned\s+|signed\s+)?(int|float|bool|void|char|long|short|double|[A-Z]\w*)(\s+\*?\s*|\s*\*\s*)([A-Za-z_]\w*)\s*\([^;{)]*\)\s*\{/gm

// names that look like a type+call but are really control flow — never list them
const NOT_FUNCS = new Set(['if', 'for', 'while', 'switch', 'return', 'sizeof', 'else'])

function parseFunctions(doc) {
  const out = []
  FUNC_RE.lastIndex = 0
  let m
  while ((m = FUNC_RE.exec(doc))) {
    const ret = m[1]
    const name = m[3]
    if (NOT_FUNCS.has(name)) continue
    // offset of the name within the whole document, for an accurate jump + flash
    const namePos = m.index + m[0].indexOf(name, ret.length)
    out.push({ name, ret, pos: namePos })
  }
  return out
}

function jumpTo(pos, len) {
  // cursor-only selection (anchor === head) so highlightSelectionMatches doesn't
  // light up every occurrence of the name across the file.
  // y:'start' parks the definition near the TOP of the viewport (with a little
  // breathing room above) so you see the signature + body below — rather than
  // scrollIntoView:true, which does the minimum scroll and can leave the line
  // pinned to the bottom edge.
  view.dispatch({
    selection: { anchor: pos, head: pos },
    effects: EditorView.scrollIntoView(pos, { y: 'start', yMargin: 48 }),
  })
  view.focus()
  flashRange(view, pos, pos + len)
}

let listEl = null

function render() {
  if (!listEl) return
  const fns = parseFunctions(view.state.doc.toString())
  listEl.innerHTML = ''
  if (!fns.length) {
    const empty = document.createElement('div')
    empty.className = 'outline-empty'
    empty.textContent = 'no functions yet'
    listEl.appendChild(empty)
    return
  }
  for (const fn of fns) {
    const btn = document.createElement('button')
    btn.className = 'outline-item'
    btn.innerHTML = `<span class="outline-ret">${fn.ret}</span> <span class="outline-name">${fn.name}</span>`
    btn.title = `jump to ${fn.name}()`
    btn.addEventListener('click', () => jumpTo(fn.pos, fn.name.length))
    listEl.appendChild(btn)
  }
}

let debounce = null

// show/hide the whole sidebar — toggles a class on #panel-code, persisted so the
// choice survives a reload. Collapsed leaves a thin reopen tab (see shell.css).
function setCollapsed(collapsed) {
  const panel = document.getElementById('panel-code')
  if (!panel) return
  panel.classList.toggle('outline-collapsed', collapsed)
  localStorage.setItem('outline-collapsed', collapsed ? '1' : '0')
}

export function initOutline() {
  listEl = document.getElementById('outline-list')
  if (!listEl) return
  render()
  // refresh as you type, debounced so big edits don't re-parse on every keystroke
  onDocChange(() => {
    clearTimeout(debounce)
    debounce = setTimeout(render, 200)
  })
  // collapse / reopen wiring
  document.getElementById('outline-toggle')?.addEventListener('click', () => setCollapsed(true))
  document.getElementById('outline-reopen')?.addEventListener('click', () => setCollapsed(false))
  setCollapsed(localStorage.getItem('outline-collapsed') === '1')
}

// called by shell.js when a cart is loaded (doc replaced wholesale) or the code
// tab is shown, so the list is fresh even if the change didn't come through typing
export function refreshOutline() { render() }
