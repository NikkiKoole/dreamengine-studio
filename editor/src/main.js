import { EditorState, Compartment, StateField, StateEffect, RangeSetBuilder } from '@codemirror/state'
import { EditorView, keymap, lineNumbers, highlightActiveLine, hoverTooltip, Decoration, GutterMarker, gutter } from '@codemirror/view'
import { defaultKeymap, history, historyKeymap, indentWithTab } from '@codemirror/commands'
import { cpp } from '@codemirror/lang-cpp'
import { foldGutter, foldKeymap } from '@codemirror/language'
import { autocompletion, completionKeymap, closeBrackets, closeBracketsKeymap, completeAnyWord, completeFromList } from '@codemirror/autocomplete'
import { search, searchKeymap, highlightSelectionMatches } from '@codemirror/search'
import { oneDark } from '@codemirror/theme-one-dark'
import { dayTheme } from './dayTheme.js'
import { studioDocs } from './studioDocs.js'

// build autocomplete entries from studioDocs
// (skip kind: 'keyword' — those are C-basics tooltip-only entries, already in cKeywords below)
const studioCompletions = Object.entries(studioDocs)
  .filter(([_, e]) => e.kind !== 'keyword')
  .map(([label, { sig, doc }]) => ({
    label,
    type: label === label.toUpperCase() ? 'constant' : 'function',
    info: `${sig}\n\n${doc}`,
  }))

const cKeywords = completeFromList([
  'auto', 'break', 'case', 'char', 'const', 'continue', 'default',
  'do', 'double', 'else', 'enum', 'extern', 'float', 'for', 'goto',
  'if', 'int', 'long', 'return', 'short', 'signed', 'sizeof',
  'static', 'struct', 'switch', 'typedef', 'union', 'unsigned',
  'void', 'volatile', 'while', 'bool', 'true', 'false', 'NULL',
  ...studioCompletions,
])

// brief highlight on a range — used after go-to-definition jumps
const addFlash   = StateEffect.define()
const clearFlash = StateEffect.define()
const flashField = StateField.define({
  create() { return Decoration.none },
  update(value, tr) {
    value = value.map(tr.changes)
    for (const e of tr.effects) {
      if (e.is(addFlash))   value = Decoration.set([Decoration.mark({ class: 'cm-flash' }).range(e.value.from, e.value.to)])
      if (e.is(clearFlash)) value = Decoration.none
    }
    return value
  },
  provide: f => EditorView.decorations.from(f),
})

// ── error line markers ────────────────────────────────────────
const errorLinesEffect = StateEffect.define()

const errorLinesField = StateField.define({
  create: () => [],
  update(lines, tr) {
    for (const e of tr.effects) if (e.is(errorLinesEffect)) return e.value
    return lines
  },
})

const errorLineDecoration = EditorView.decorations.compute([errorLinesField], state => {
  const lines = state.field(errorLinesField)
  if (!lines.length) return Decoration.none
  const decs = []
  for (const n of lines) {
    try {
      const line = state.doc.line(n)
      decs.push(Decoration.line({ class: 'cm-error-line' }).range(line.from))
    } catch {}
  }
  return Decoration.set(decs)
})

class ErrorGutterMarker extends GutterMarker {
  toDOM() {
    const el = document.createElement('span')
    el.textContent = '●'
    el.className = 'cm-error-gutter-dot'
    return el
  }
}
const errorGutterMarker = new ErrorGutterMarker()

const errorGutter = gutter({
  class: 'cm-error-gutter',
  markers(view) {
    const lines = view.state.field(errorLinesField)
    const builder = new RangeSetBuilder()
    for (const n of [...lines].sort((a, b) => a - b)) {
      try {
        const line = view.state.doc.line(n)
        builder.add(line.from, line.from, errorGutterMarker)
      } catch {}
    }
    return builder.finish()
  },
  initialSpacer: () => errorGutterMarker,
})

export function flashRange(view, from, to) {
  view.dispatch({ effects: addFlash.of({ from, to }) })
  setTimeout(() => view.dispatch({ effects: clearFlash.of(null) }), 1200)
}

// cmd/ctrl-click on a word:
//   - if it's documented in studioDocs → open help tab + scroll to entry
//   - else search the cart for a declaration (int x, void f(), #define X, etc.) and jump cursor there
const studioClickToHelp = EditorView.domEventHandlers({
  click(event, view) {
    if (!event.metaKey && !event.ctrlKey) return false
    const pos = view.posAtCoords({ x: event.clientX, y: event.clientY })
    if (pos === null) return false
    const word = view.state.wordAt(pos)
    if (!word) return false
    const name = view.state.sliceDoc(word.from, word.to)

    // 1) help lookup
    if (studioDocs[name]) {
      window.dispatchEvent(new CustomEvent('help-jump', { detail: { key: name } }))
      event.preventDefault()
      return true
    }

    // 2) go-to-definition in the cart
    const doc      = view.state.doc.toString()
    const escName  = name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
    const typeWords = 'int|float|bool|void|char|long|short|double|unsigned|signed'
    const patterns = [
      // #define NAME ...
      new RegExp(`#define\\s+(${escName})\\b`),
      // [static] type [*] name
      new RegExp(`\\b(?:static\\s+)?(?:${typeWords})\\s+\\*?\\s*(${escName})\\b`),
      // typedef struct/union/enum { ... } TypeName;
      new RegExp(`}\\s*(${escName})\\s*;`),
    ]
    let best = null
    for (const re of patterns) {
      const m = doc.match(re)
      if (m && (best === null || m.index < best.index)) best = m
    }
    if (!best) return false

    // jump cursor to the name part of the match (always the last group)
    const namePos = best.index + best[0].length - name.length
    if (namePos === word.from) return false  // already at the declaration

    // for typedef closings (} TypeName;), scroll to the opening typedef keyword
    let jumpPos = namePos
    let jumpLen = name.length
    if (best[0].trimStart().startsWith('}')) {
      const tdIdx = doc.lastIndexOf('typedef', best.index)
      if (tdIdx !== -1) { jumpPos = tdIdx; jumpLen = 'typedef'.length }
    }

    // cursor-only (anchor === head) so highlightSelectionMatches doesn't
    // light up every instance of the matched keyword across the file
    view.dispatch({
      selection: { anchor: jumpPos, head: jumpPos },
      scrollIntoView: true,
    })
    view.focus()
    flashRange(view, jumpPos, jumpPos + jumpLen)
    event.preventDefault()
    return true
  },
})

// hover tooltip for studio symbols
const studioHover = hoverTooltip((view, pos) => {
  const word = view.state.wordAt(pos)
  if (!word) return null
  const name = view.state.sliceDoc(word.from, word.to)
  const entry = studioDocs[name]
  if (!entry) return null

  return {
    pos: word.from,
    end: word.to,
    above: true,
    create() {
      const dom = document.createElement('div')
      dom.className = 'studio-tooltip'
      dom.innerHTML = `<div class="studio-tooltip-sig">${entry.sig}</div><div class="studio-tooltip-doc">${entry.doc.replace(/\n/g, '<br>')}</div>`
      return { dom }
    },
  }
})

// startDoc is intentionally empty — on boot the editor loads the welcome
// cart (pixel zoo) the same way the Load Cart button does, so the cart PNG is
// the single source of truth for its code + sprites + settings. See shell.js.
const startDoc = ``

// theme compartment lets us swap day/night at runtime without rebuilding state
const themeCompartment = new Compartment()
const initialThemeIsDay = localStorage.getItem('theme') === 'day'

// live auto-reload hook: subscribers (the live libtcc backend, the outline panel)
// register a callback that fires on every doc edit. Multiple subscribers are
// supported — each gets called on every change.
const docChangeHooks = []
export function onDocChange(cb) { docChangeHooks.push(cb) }

const state = EditorState.create({
  doc: startDoc,
  extensions: [
    history(),
    EditorView.updateListener.of(u => { if (u.docChanged) docChangeHooks.forEach(h => h()) }),
    keymap.of([indentWithTab, ...defaultKeymap, ...historyKeymap, ...foldKeymap, ...completionKeymap, ...closeBracketsKeymap, ...searchKeymap]),
    lineNumbers(),
    foldGutter(),
    highlightActiveLine(),
    highlightSelectionMatches(),
    closeBrackets(),
    search({ top: true }),
    autocompletion({ override: [cKeywords, completeAnyWord] }),
    studioHover,
    studioClickToHelp,
    flashField,
    errorLinesField,
    errorLineDecoration,
    errorGutter,
    cpp(),
    themeCompartment.of(initialThemeIsDay ? dayTheme : oneDark),
  ],
})

export const view = new EditorView({
  state,
  parent: document.getElementById('editor'),
})

export function setErrorLines(lineNums) {
  view.dispatch({ effects: errorLinesEffect.of(lineNums) })
}

export function setEditorTheme(mode) {
  view.dispatch({
    effects: themeCompartment.reconfigure(mode === 'day' ? dayTheme : oneDark),
  })
}
