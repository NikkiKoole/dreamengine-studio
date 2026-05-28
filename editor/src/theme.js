import { EditorView } from '@codemirror/view'
import { HighlightStyle, syntaxHighlighting } from '@codemirror/language'
import { tags as t } from '@lezer/highlight'

const bg       = '#1b1c1f'
const fg       = '#c8ccd4'       // most code
const fgBright = '#e8eaf0'       // keywords
const fgDim    = '#4e525c'       // comments
const accent   = '#c9a96e'       // strings, numbers — one warm tone
const cursor   = '#e8eaf0'
const selection = '#2e3240'
const gutterFg = '#3e4250'
const lineHighlight = '#22232a'

export const eventuallyTheme = EditorView.theme({
  '&': {
    color: fg,
    backgroundColor: bg,
  },
  '.cm-content': {
    caretColor: cursor,
    padding: '16px 0',
  },
  '.cm-cursor': {
    borderLeftColor: cursor,
    borderLeftWidth: '2px',
  },
  '&.cm-focused .cm-selectionBackground, .cm-selectionBackground': {
    backgroundColor: selection,
  },
  '.cm-gutters': {
    backgroundColor: bg,
    color: gutterFg,
    border: 'none',
    paddingRight: '8px',
  },
  '.cm-activeLineGutter': {
    backgroundColor: lineHighlight,
    color: fg,
  },
  '.cm-activeLine': {
    backgroundColor: lineHighlight,
  },
  '.cm-line': {
    paddingLeft: '12px',
  },
}, { dark: true })

export const eventuallyHighlight = syntaxHighlighting(HighlightStyle.define([
  { tag: t.keyword,                    color: fgBright, fontWeight: 'bold' },
  { tag: t.controlKeyword,             color: fgBright, fontWeight: 'bold' },
  { tag: t.definitionKeyword,          color: fgBright, fontWeight: 'bold' },
  { tag: t.comment,                    color: fgDim, fontStyle: 'italic' },
  { tag: t.string,                     color: accent },
  { tag: t.number,                     color: accent },
  { tag: t.bool,                       color: accent },
  { tag: t.null,                       color: accent },
  { tag: t.typeName,                   color: fg },
  { tag: t.variableName,               color: fg },
  { tag: t.function(t.variableName),   color: fg },
  { tag: t.definition(t.variableName), color: fg },
  { tag: t.operator,                   color: fg },
  { tag: t.punctuation,                color: fgDim },
  { tag: t.bracket,                    color: fgDim },
]))
