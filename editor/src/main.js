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

function flashRange(view, from, to) {
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
    view.dispatch({
      selection: { anchor: namePos, head: namePos + name.length },
      scrollIntoView: true,
    })
    view.focus()
    flashRange(view, namePos, namePos + name.length)
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

const startDoc = `#include "studio.h"

#define WORLD_W (SCREEN_W * 2)

int x    = SCREEN_W;
int y    = 92;
int face = 1;           // 1 = right, -1 = left  (set by sgn())

void update() {
    // build a tile floor on the first frame (draw something in sprite slot 1 to see it)
    static bool map_built = false;
    if (!map_built) {
        for (int cx = 0; cx < MAP_W; cx++) mset(cx, 11, 1);
        map_built = true;
    }

    bpm(85);

    // chord progression: Am7 → Fmaj7 → Cmaj7 → G7  (vi - IV - I - V in C)
    int roots[] = { 57, 53, 60, 55 };
    int types[] = { CHORD_MIN7, CHORD_MAJ7, CHORD_MAJ7, CHORD_DOM7 };
    int ci      = (beat() / 4) % 4;

    if (every(4)) strum(roots[ci], types[ci], INSTR_TRI, 3, 45);
    if (every(2)) note(roots[ci] - 12, INSTR_TRI, 4);

    // hihat: closed (short) every beat — except beat 3 of each bar, where it opens up
    if (every(1)) {
        if ((beat() % 4) == 2) hit(60, INSTR_NOISE, 3, 200);    // open hihat
        else                    hit(60, INSTR_NOISE, 2,  25);    // closed hihat
    }

    int melody[] = { 69, 72, 74, 76, 79 };
    if (every(1) && chance(25)) note(melody[rnd(5)], INSTR_SINE, 2);

    int dx = 0, dy = 0;
    if (btn(0, BTN_RIGHT)) dx =  2;
    if (btn(0, BTN_LEFT))  dx = -2;
    if (btn(0, BTN_UP))    dy = -2;
    if (btn(0, BTN_DOWN))  dy =  2;
    x += dx;
    y += dy;
    if (dx != 0) face = sgn(dx);              // remember facing direction

    // mid() clamps without 4 lines of if/else
    x = mid(0, x, WORLD_W - 16);
    y = mid(0, y, SCREEN_H - 16);

    if (btnp(0, BTN_A)) note(melody[rnd(5)], INSTR_TRI, 5);
    if (btnp(0, BTN_B)) strum(roots[ci], types[ci], INSTR_TRI, 5, 30);
}

void draw() {
    // camera follows player; world is twice as wide as the screen
    camera(mid(0, x - SCREEN_W/2, WORLD_W - SCREEN_W), 0);
    cls(CLR_BLUE);

    // starfield in world coords — camera makes it scroll past
    for (int i = 0; i < 40; i++) {
        pset((i * 73) % WORLD_W, (i * 41) % SCREEN_H, CLR_WHITE);
    }

    // draw the map — skips empty cells, respects camera()
    map(0, 0, 0, 0, MAP_W, MAP_H);

    // bob with now(), flip with sprf()
    int bob = ((int)(now() * 4)) % 2 ? 0 : -1;
    sprf(0, x, y + bob, face < 0, false);

    // pget(): if a star was just under the player last frame, draw a glow.
    // pget reads the previous frame's canvas — great for after-the-fact checks.
    if (pget(x + 8, y + 17) == CLR_WHITE) {
        circ(x + 8, y + 8, 12, CLR_WHITE);
    }

    // HUD in screen space — reset camera before drawing UI
    camera(0, 0);

    // minimap (clip): restrict drawing to a 60×24 box in the top-right
    int mm_x = SCREEN_W - 64, mm_y = 4, mm_w = 60, mm_h = 24;
    clip(mm_x, mm_y, mm_w, mm_h);
    rectfill(mm_x, mm_y, mm_w, mm_h, CLR_BLACK);
    rect    (mm_x, mm_y, mm_w, mm_h, CLR_WHITE);
    pset(mm_x + (x * mm_w) / WORLD_W, mm_y + (y * mm_h) / SCREEN_H, CLR_RED);
    clip(0, 0, 0, 0);   // disable

    // sspr(): draw a 2x-sized portrait of sprite 0 next to the help text
    sspr(0, 0, 16, 16, 8, 176, 24, 24);

    int r = 6 - (int)(beat_pos() * 4.0f);
    circfill(SCREEN_W - 12, SCREEN_H - 12, mid(2, r, 6), CLR_WHITE);
    print("wasd move   z melody   x strum", 40, 184, CLR_WHITE);
}
`

// theme compartment lets us swap day/night at runtime without rebuilding state
const themeCompartment = new Compartment()
const initialThemeIsDay = localStorage.getItem('theme') === 'day'

const state = EditorState.create({
  doc: startDoc,
  extensions: [
    history(),
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
