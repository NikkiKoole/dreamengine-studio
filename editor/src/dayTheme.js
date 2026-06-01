import { EditorView } from '@codemirror/view'
import { HighlightStyle, syntaxHighlighting } from '@codemirror/language'
import { tags as t } from '@lezer/highlight'

// warm cream-paper light theme, matched to the shell's day palette
const bg        = '#f4f1e8'
const bgActive  = '#ebe6d4'
const bgSelect  = '#d9d1b8'
const fg        = '#3a3a3a'
const gutter    = '#9a9486'
const cursor    = '#c0497f'

const dayUI = EditorView.theme({
  '&':              { color: fg, backgroundColor: bg },
  '.cm-content':    { caretColor: cursor },
  '&.cm-focused .cm-cursor': { borderLeftColor: cursor },
  '.cm-activeLine': { backgroundColor: bgActive },
  '.cm-gutters':    { backgroundColor: bg, color: gutter, border: 'none' },
  '.cm-activeLineGutter': { backgroundColor: bgActive, color: fg },
  '&.cm-focused .cm-selectionBackground, ::selection, .cm-selectionBackground': { backgroundColor: bgSelect },
  '.cm-tooltip':    { backgroundColor: '#fbf8ef', border: '1px solid #c9bfa8', color: fg },
  '.cm-tooltip-autocomplete > ul > li[aria-selected]': { backgroundColor: bgActive, color: fg },
  '.cm-foldPlaceholder': { backgroundColor: bgActive, color: gutter, border: 'none' },
  '.cm-matchingBracket': { backgroundColor: 'transparent', outline: '1px solid #c0497f' },
}, { dark: false })

const dayHighlight = HighlightStyle.define([
  { tag: t.comment,                          color: '#9a9486', fontStyle: 'italic' },
  { tag: t.string,                           color: '#5c7d3a' },
  { tag: t.number,                           color: '#b85a3a' },
  { tag: [t.keyword, t.controlKeyword],      color: '#8a6b3a', fontWeight: 'bold' },
  { tag: t.typeName,                         color: '#4a6d8a' },
  { tag: t.function(t.variableName),         color: '#6a4a8a' },
  { tag: t.variableName,                     color: fg },
  { tag: t.operator,                         color: '#6a655a' },
  { tag: [t.bracket, t.paren, t.brace, t.squareBracket, t.punctuation], color: '#6a655a' },
  { tag: t.macroName,                        color: '#8a6b3a' },
  { tag: t.bool,                             color: '#b85a3a' },
  { tag: t.null,                             color: '#b85a3a' },
  { tag: t.escape,                           color: '#5c7d3a', fontWeight: 'bold' },
  { tag: t.atom,                             color: '#b85a3a' },
])

export const dayTheme = [dayUI, syntaxHighlighting(dayHighlight)]
