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
//   node tools/lint-fx-frame.js --quiet    exit 1 if any are found (CI gate); else SILENT
//   node tools/lint-fx-frame.js --strict   same gate, but still prints the report (repo-doctor row)
//   node tools/lint-fx-frame.js --json     machine-readable
//   node tools/lint-fx-frame.js --selfcheck  assert the CHECKER (known-answer fixture, 30 assertions)
//
// HEURISTIC (in the style of ui-audit / mobile-lint — static, waivable, deliberately low-false-
// positive). It only inspects the bodies of update() and draw() directly, and flags an effect
// call that runs UNCONDITIONALLY — not inside an if / else / while / switch block, and not guarded
// by an inline `if (...)` / `?:` on its own statement. Calls already inside a conditional are
// presumed gated (the correct pattern) and pass. A call routed through a helper (the groovebox
// apply_fx() pattern) is NOT followed — that's the right structure anyway, so under-reporting
// there is intended. Waive a confirmed-safe line with `// fx-lint-ignore` on it or the line above.
//
// A `for` BODY IS NOT A GATE — deliberately, and unlike while/switch. The brace classifier reads
// the text between the last statement boundary and the `{`, and `;` is a statement boundary, so a
// for header's semicolons erase the keyword before it is classified; a `while (…) {` header has no
// semicolon and survives. That asymmetry started as an accident of the parser but it is the
// behaviour we want, so it is now the contract: `for (i…) crush(i, …);` rebuilds the DSP N times
// per frame, which is the footgun *worse*, not an exemption from it. A per-channel gate belongs
// INSIDE the loop (`for (…) if (dirty[i]) crush(i, …);` — the inline guard then passes it). This
// paragraph replaced a line claiming for-blocks were exempt, which was never true; the mismatch
// surfaced the moment the fixture below probed each construct instead of trusting the prose.
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
  // stack of booleans: is this brace a conditional body? `for` is in the regex but is
  // unreachable for a real for-header — the `;` handler below resets stmtStart past the keyword.
  // That is the intended contract, not a bug; see the `for` BODY note in the header.
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

// ── --selfcheck: assert the CHECKER against known answers ────────────────────
// See docs/guides/checks-and-oracles.md "Self-test the checker". lintFile() already takes a path,
// so this drives it directly on tools/fixtures/lint-fx-frame/*.c.txt — no fake carts dir needed.
//
// WHY THIS ONE NEEDED IT MOST. It reported "✓ 0 findings across 573 carts" and was wired into no
// gate, which is indistinguishable from a scanner that had gone blind — and the heuristic is a
// hand-rolled C parser (brace classifier, statement tracker, comment/string stripper) with SIX
// exempt classes. Writing the fixture immediately turned up a real doc/behaviour mismatch: see
// the for-vs-while note in the header above.
//
// Convention: a fixture line carrying the marker `@@FLAG@@` MUST be reported and every other line
// must NOT be. That makes each file one BIDIRECTIONAL assertion which cannot go vacuous — a
// scanner that goes blind empties `got` and fails the files with markers, one that floods grows
// `got` and fails the files without.
if (process.argv.includes('--selfcheck')) {
  const FX_DIR = path.join(__dirname, 'fixtures', 'lint-fx-frame')
  // `.c.txt`, not `.c`: a fixture cart is never compiled, and a real .c would be indexed by
  // clangd and read as a cart by anything globbing tools/carts for sources.
  const load = (file) => {
    const p = path.join(FX_DIR, file)
    const src = fs.readFileSync(p, 'utf8').split('\n')
    const hits = lintFile(p)
    const want = src.map((l, i) => ({ l, n: i + 1 })).filter(x => x.l.includes('@@FLAG@@')).map(x => x.n)
    const got = hits.map(h => h.line).sort((a, b) => a - b)
    return { src, hits, want, got, exact: JSON.stringify(want) === JSON.stringify(got) }
  }
  // Resolve a finding back to the LINE it points at, then assert on that text — never on the
  // message, which quotes its own advice and not the offending call (rule 4 in the guide: a
  // "must not be flagged" assertion has to be able to fail).
  const hitAt = (v, marker) => v.hits.some(h => (v.src[h.line - 1] || '').includes(marker))

  const T = []
  const t = (n, ok) => T.push({ n, ok })

  const flagged = load('flagged.c.txt'), guarded = load('guarded.c.txt')
  const excluded = load('excluded.c.txt'), scope = load('scope.c.txt')
  const waiver = load('waiver.c.txt'), noise = load('noise.c.txt')

  // ── the footgun shapes
  t('flagged: a bare unconditional call in update() is caught  [the footgun]',
    hitAt(flagged, 'bare call, unconditional'))
  t('flagged: draw() is scanned too, not just update()', hitAt(flagged, 'draw() is scanned too'))
  t('flagged: a multi-word FX name resolves  [reverb_bus]', hitAt(flagged, 'multi-word FX name'))
  t('flagged: every planted call is found, not just the first  [loop guard]', flagged.got.length === 5)
  t('flagged: a for-loop body is NOT treated as a gate  [see the header note]',
    hitAt(flagged, 'for-loop body is NOT a gate'))

  // ── the gated patterns. Every one is a real FX-set member, so a broken conditional detector
  //    cannot hide behind an inert fixture (the "passes for the wrong reason" rule).
  t('guarded: a call inside an if-block is silent', !hitAt(guarded, 'OK_ifblock'))
  t('guarded: an inline `if (x) fx()` is silent', !hitAt(guarded, 'OK_inline'))
  t('guarded: a ternary-guarded call is silent', !hitAt(guarded, 'OK_ternary'))
  t('guarded: a while-loop body is silent', !hitAt(guarded, 'OK_while'))
  t('guarded: a switch case body is silent', !hitAt(guarded, 'OK_switch'))
  t('guarded: an else-block body is silent', !hitAt(guarded, 'OK_else'))
  t('guarded: ...and the file really does call real FX-set effects  [inert-fixture guard]',
    (() => {
      const txt = guarded.src.join('\n')
      return ['crush(', 'tape(', 'chorus(', 'flanger(', 'phaser(', 'wah(', 'eq(']
        .every(f => txt.includes(f))
    })())

  // ── the ride-live exclusions: unconditional AND per-frame, and silent BECAUSE of the FX set
  t('excluded: filter/varispeed/echo/tremolo per-frame are silent  [ride-it-live]',
    !['OK_filter', 'OK_varispeed', 'OK_echo', 'OK_tremolo'].some(m => hitAt(excluded, m)))
  t('excluded: the per-note live setters are silent  [note_cutoff/reverb/vol]',
    !['OK_note_cutoff', 'OK_note_reverb', 'OK_note_vol'].some(m => hitAt(excluded, m)))
  t('excluded: ...and they sit in the exact shape flagged.c.txt IS reported for  [proves the list]',
    excluded.got.length === 0 && /^\s*filter\(/m.test(excluded.src.join('\n')))

  // ── scope
  t('scope: init() is not inspected  [configuring once is the FIX]', !hitAt(scope, 'OK_init'))
  t('scope: a call in a helper is not followed  [the apply_fx() shape]', !hitAt(scope, 'OK_helper'))

  // ── the waiver, and the one leak it must not have
  t('waiver: a trailing fx-lint-ignore waives', !hitAt(waiver, 'OK_trailing'))
  t('waiver: a STANDALONE ignore comment on the line above waives', !hitAt(waiver, 'OK_above'))
  t('waiver: a TRAILING ignore on another statement does NOT leak down  [regression]',
    hitAt(waiver, '@@FLAG@@'))

  // ── comment/string stripping, and line numbers surviving it
  t('noise: a commented-out call is not reported  [line + block comment]',
    !noise.src.some((l, i) => noise.got.includes(i + 1) && /^\s*(\/\/|\/\*|\s)*\s*(crush|eq|reverb)\s*\(/.test(l)))
  t('noise: an FX name inside a string literal is not reported', !hitAt(noise, 'inside a string literal'))
  t('noise: a prefixed name (my_crush) is not read as crush()', !hitAt(noise, 'PREFIXED name'))
  t('noise: the real call is reported at the RIGHT line, after all the stripping  [offset guard]',
    noise.got.length === 1 && hitAt(noise, 'the one real call'))

  // ── the bidirectional set check per file: neither blind nor flooding, on any of the six
  for (const [name, v] of [['flagged', flagged], ['guarded', guarded], ['excluded', excluded],
                           ['scope', scope], ['waiver', waiver], ['noise', noise]]) {
    t(`${name}.c.txt: reported lines are EXACTLY the marked ones  [want ${JSON.stringify(v.want)}, got ${JSON.stringify(v.got)}]`,
      v.exact)
  }

  const failed = T.filter(x => !x.ok)
  for (const x of T) console.log(`  ${x.ok ? '\x1b[32m✓\x1b[0m' : '\x1b[31m✗\x1b[0m'} ${x.n}`)
  console.log(failed.length
    ? `\x1b[31mlint-fx-frame --selfcheck FAILED\x1b[0m — ${failed.length} of ${T.length} expectations broken`
    : `lint-fx-frame --selfcheck: ${T.length}/${T.length} known answers correct`)
  process.exit(failed.length ? 1 : 0)
}

// ── run ──────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2)
const json = argv.includes('--json')
const quiet = argv.includes('--quiet')
// --strict = report AND exit nonzero (the repo-doctor row). --quiet gates silently, which makes a
// clean run print nothing at all — fine for a hook, useless as a health-strip row ("(no output)").
const strict = argv.includes('--strict')

const files = fs.readdirSync(CARTS).filter(f => f.endsWith('.c')).sort()
const report = []
for (const f of files) {
  const hits = lintFile(path.join(CARTS, f))
  if (hits.length) report.push({ cart: f.replace(/\.c$/, ''), hits })
}

if (json) { console.log(JSON.stringify(report, null, 2)); process.exit((quiet || strict) && report.length ? 1 : 0) }

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
process.exit(quiet || strict ? 1 : 0)
