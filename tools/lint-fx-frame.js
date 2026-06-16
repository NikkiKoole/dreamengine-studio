#!/usr/bin/env node
// lint-fx-frame.js — flags the SET-AND-HOLD effect footgun: a bus effect reconfigured EVERY
// FRAME. Wiring a knob straight into update()/draw() so crush()/tape()/eq()/chorus()/reverb()/
// flanger()/… fires 60×/s rebuilds the bus DSP (ring buffers, filter coefficients) every frame,
// churning the audio thread → STUTTER (silent, easy to misattribute — it doesn't crash). The fix
// is always the same: configure once in init(), or gate the call on the value actually changing
// (keep a last-applied copy, or use the ui_* "changed" return — copy groovebox's apply_fx()).
// See CLAUDE.md ("Effects are SET-AND-HOLD") + docs/guides/effects-recipes.md (intro callout).
//
//   node tools/lint-fx-frame.js            report every unconditional per-frame effect call
//   node tools/lint-fx-frame.js --quiet    exit 1 if any are found (CI gate); else silent
//   node tools/lint-fx-frame.js --json     machine-readable
//
// HEURISTIC (in the style of ui-audit / mobile-lint — static, waivable, deliberately low-false-
// positive). It only inspects the bodies of update() and draw() directly, and flags an effect
// call that runs UNCONDITIONALLY — not inside any if/for/while/switch/else block, and not guarded
// by an inline `if (...)` / `?:` on its own statement. Calls already inside a conditional are
// presumed gated (the correct pattern) and pass. A call routed through a helper (the groovebox
// apply_fx() pattern) is NOT followed — that's the right structure anyway, so under-reporting
// there is intended. Waive a confirmed-safe line with `// fx-lint-ignore` on it or the line above.
//
// EXCLUDED by design — effects built to be ridden live (internal slew / cheap coefficient retune,
// no buffer rebuild): filter() (the DJ filter, "ride it live"), varispeed() (slewed tape dive),
// and the per-note live setters (note_cutoff/note_reverb/note_vol — slewed, gate-while-moving is
// their job). echo()/tremolo() are also excluded (time-slew / cheap LFO reset).

const fs = require('fs')
const path = require('path')

const ROOT = path.resolve(__dirname, '..')
const CARTS = path.join(ROOT, 'tools', 'carts')

// the buffer/coefficient-rebuilding bus effects — reconfiguring these per-frame is the footgun.
const FX = new Set([
  'reverb', 'reverb_bus', 'reverb_bus_fx', 'reverb_insert',
  'echo_insert', 'chorus', 'flanger', 'tape', 'tape_inst',
  'wah', 'wah_lfo', 'crush', 'crush_inst', 'eq', 'eq_inst',
  'phaser', 'univibe', 'dropout', 'shimmer', 'leslie',
  'drive_insert', 'drive_insert_inst',
])

// strip // line comments, /* block */ comments, and string/char literals → spaces (preserve
// newlines + length so byte offsets still map to the original line numbers).
function stripNoise(src) {
  let out = '', i = 0, n = src.length
  const keep = (c) => (out += c === '\n' ? '\n' : ' ')
  while (i < n) {
    const c = src[i], d = src[i + 1]
    if (c === '/' && d === '/') { while (i < n && src[i] !== '\n') keep(src[i++]) }
    else if (c === '/' && d === '*') { keep(c); keep(d); i += 2; while (i < n && !(src[i] === '*' && src[i + 1] === '/')) keep(src[i++]); if (i < n) { keep(src[i++]); keep(src[i++]) } }
    else if (c === '"' || c === "'") { const q = c; out += src[i++]; while (i < n && src[i] !== q) { if (src[i] === '\\') { keep(src[i++]) } keep(src[i] === '\n' ? '\n' : src[i]); i++ } if (i < n) out += src[i++] }
    else { out += c; i++ }
  }
  return out
}

// return [start,end) byte range of a function body `{...}` given the index of `void name(`
function funcBody(code, name) {
  const re = new RegExp(`\\bvoid\\s+${name}\\s*\\(`, 'g')
  const m = re.exec(code)
  if (!m) return null
  let i = code.indexOf('{', m.index)
  if (i < 0) return null
  const open = i
  let depth = 0
  for (; i < code.length; i++) {
    if (code[i] === '{') depth++
    else if (code[i] === '}') { if (--depth === 0) return { open, close: i } }
  }
  return null
}

const lineOf = (src, idx) => src.slice(0, idx).split('\n').length

// scan one function body: flag FX calls that run unconditionally (brace stack carries no
// conditional frame, and the call's own statement has no inline if/?: guard).
function scanBody(code, open, close) {
  const hits = []
  // stack of booleans: is this brace a conditional (if/for/while/switch/else) body?
  const stack = []
  let stmtStart = open + 1   // start of the current statement (after the last ; { or })
  for (let i = open; i <= close; i++) {
    const c = code[i]
    if (c === '{') {
      // was the text since stmtStart a control header?
      const head = code.slice(stmtStart, i)
      stack.push(/\b(if|for|while|switch|else)\b/.test(head))
      stmtStart = i + 1
    } else if (c === '}') { stack.pop(); stmtStart = i + 1 }
    else if (c === ';') stmtStart = i + 1
    else if (/[A-Za-z_]/.test(c)) {
      // try to match an identifier( here
      const m = /^([A-Za-z_]\w*)\s*\(/.exec(code.slice(i))
      if (m && FX.has(m[1])) {
        const insideConditional = stack.some(Boolean)
        const stmtText = code.slice(stmtStart, i)
        const inlineGuard = /\b(if|else)\b|\?/.test(stmtText)
        if (!insideConditional && !inlineGuard) hits.push({ fn: m[1], idx: i })
        i += m[1].length   // skip past the name
      } else {
        // skip the whole identifier so we don't re-scan its tail
        const id = /^[A-Za-z_]\w*/.exec(code.slice(i))
        if (id) i += id[0].length - 1
      }
    }
  }
  return hits
}

function lintFile(file) {
  const raw = fs.readFileSync(file, 'utf8')
  const code = stripNoise(raw)
  const rawLines = raw.split('\n')
  const hits = []
  for (const fn of ['update', 'draw']) {
    const body = funcBody(code, fn)
    if (!body) continue
    for (const h of scanBody(code, body.open, body.close)) {
      const ln = lineOf(code, h.idx)
      // waiver: `// fx-lint-ignore` on the hit line, OR a STANDALONE comment line directly above
      // (a trailing ignore on a different statement above must not leak onto this line).
      const here = rawLines[ln - 1] || '', above = rawLines[ln - 2] || ''
      if (/fx-lint-ignore/.test(here) || /^\s*\/\/.*fx-lint-ignore/.test(above)) continue
      hits.push({ in: fn, fn: h.fn, line: ln, text: (rawLines[ln - 1] || '').trim() })
    }
  }
  return hits
}

// ── run ──────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2)
const json = argv.includes('--json')
const quiet = argv.includes('--quiet')

const files = fs.readdirSync(CARTS).filter(f => f.endsWith('.c')).sort()
const report = []
for (const f of files) {
  const hits = lintFile(path.join(CARTS, f))
  if (hits.length) report.push({ cart: f.replace(/\.c$/, ''), hits })
}

if (json) { console.log(JSON.stringify(report, null, 2)); process.exit(quiet && report.length ? 1 : 0) }

if (!report.length) {
  if (!quiet) console.log('✓ no per-frame effect reconfiguration found (' + files.length + ' carts scanned)')
  process.exit(0)
}

if (!quiet) {
  const total = report.reduce((a, r) => a + r.hits.length, 0)
  console.log(`⚠ ${total} per-frame effect call(s) in ${report.length} cart(s) — reconfigured every frame (set-and-hold footgun):\n`)
  for (const r of report) {
    console.log(`  ${r.cart}.c`)
    for (const h of r.hits) console.log(`    L${h.line}  ${h.fn}()  in ${h.in}()   ${h.text}`)
  }
  console.log('\nFix: move to init(), or gate on the value changing (copy groovebox apply_fx()).')
  console.log('False positive? add `// fx-lint-ignore` on the line (or the line above).')
}
process.exit(quiet ? 1 : 0)
