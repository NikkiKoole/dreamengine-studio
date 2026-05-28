import './style.css'
import { EditorState } from '@codemirror/state'
import { EditorView, keymap, lineNumbers, highlightActiveLine, hoverTooltip } from '@codemirror/view'
import { defaultKeymap, history, historyKeymap, indentWithTab } from '@codemirror/commands'
import { cpp } from '@codemirror/lang-cpp'
import { foldGutter, foldKeymap } from '@codemirror/language'
import { autocompletion, completionKeymap, closeBrackets, closeBracketsKeymap, completeAnyWord, completeFromList } from '@codemirror/autocomplete'
import { oneDark } from '@codemirror/theme-one-dark'
import { studioDocs } from './studioDocs.js'

// build autocomplete entries from studioDocs
const studioCompletions = Object.entries(studioDocs).map(([label, { sig, doc }]) => ({
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

const startDoc = `#include "studio.h"

int x = 152;
int y = 92;

void update() {
    if (btn(0, BTN_RIGHT)) x += 2;
    if (btn(0, BTN_LEFT))  x -= 2;
    if (btn(0, BTN_UP))    y -= 2;
    if (btn(0, BTN_DOWN))  y += 2;

    if (x < 0)              x = 0;
    if (x > SCREEN_W - 16)  x = SCREEN_W - 16;
    if (y < 0)              y = 0;
    if (y > SCREEN_H - 16)  y = SCREEN_H - 16;
}

void draw() {
    cls(CLR_BLACK);
    spr(0, x, y);
    print("use arrow keys", 86, 180, CLR_WHITE);
}
`

const state = EditorState.create({
  doc: startDoc,
  extensions: [
    history(),
    keymap.of([indentWithTab, ...defaultKeymap, ...historyKeymap, ...foldKeymap, ...completionKeymap, ...closeBracketsKeymap]),
    lineNumbers(),
    foldGutter(),
    highlightActiveLine(),
    closeBrackets(),
    autocompletion({ override: [cKeywords, completeAnyWord] }),
    studioHover,
    cpp(),
    oneDark,
  ],
})

export const view = new EditorView({
  state,
  parent: document.getElementById('editor'),
})
