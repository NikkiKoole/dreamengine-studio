#!/usr/bin/env node
// mobile-lint.js — static report card: how is this cart not perfect for mobile?
// Design + check rationale: docs/design/mobile-web-notes.md §3.
//
//   node tools/mobile-lint.js <name> [<name>...]   lint specific carts
//   node tools/mobile-lint.js --site               lint every cart published in site/
//   node tools/mobile-lint.js --all                lint every cart in tools/carts/
//   node tools/mobile-lint.js --selfcheck          assert the CHECKER (known-answer fixture, 27)
//
// Verdicts (input path on a phone):
//   touch-ready    reads touch_*/tap*/touch_controls, or .cart.js sets touchControls
//   tap-as-mouse   mouse-button driven — browsers map single taps to clicks; usually fine
//   fixable        btn()-driven only — one line (touchControls: true) enables the
//                  on-screen stick + A/B overlay that feeds btn()
//   keyboard-only  reads key()/keyp()/keyr() — needs a hardware keyboard, the overlay
//                  can't synthesize arbitrary keys
//   no-input       reads nothing (radios etc.) — fine; first tap unlocks audio
//
// Warnings (work everywhere except a touch screen):
//   hover          mouse position read but no mouse button — hover has no touch equivalent
//   wheel          mouse_wheel() — no scroll wheel on a phone
//   right/middle   MOUSE_RIGHT / MOUSE_MIDDLE reads — no second button on a touch screen
//   touch>5        literal touch index ≥ 5 — iOS Safari caps at ~5 simultaneous touches
//   tiny-target    tap()/tapp() with a literal w or h < 16 canvas px (44pt Apple HIG floor
//                  ≈ 15–17 canvas px on a 320-wide cart fullscreen on an iPhone).
//                  FIXES: a ui.h widget panel can add ui_loupe(1) — a drag-out
//                  magnifier for fat fingers (docs/design/loupe-notes.md); swept
//                  grids/keybeds want pinch-zoom (gestures.h pinch_scale + camera_ex)
//   keys-untapped  literal-arg key reads on a line with no tap/touch alternative —
//                  the keycap-retrofit checklist for touch-capable carts. Grep-grade:
//                  a key whose touch path lives in ANOTHER statement (a pointer-table
//                  branch, a tappable tab) still shows up — verify by hand before
//                  retrofitting. Dynamic args (key arrays = play surfaces, usually
//                  mirrored by a touch surface) are skipped.
//
// v1 is grep-grade and informational (always exits 0): it produces a worklist,
// not a gate. Known gap (manual check): key-gated title screens — "press Z to
// start" — on otherwise touch-capable carts.
//
// Library headers: a quote-include of a runtime/ header (ui.h, gestures.h …)
// is inlined before scanning, so a cart whose whole input story lives in
// ui.h widgets still ranks touch-ready (the §5.4 contract: an all-ui.h cart
// lints green by construction). studio.h is skipped — it's declarations
// only, and its prototypes would match every input regex.

const fs   = require('fs')
const path = require('path')

const ROOT      = path.join(__dirname, '..')
// Overridable so --selfcheck can lint a tiny FIXTURE shelf (tools/fixtures/mobile-lint/) with its
// own fake runtime header. CART_EXT exists so fixture carts can be `.c.txt`: never compiled, and a
// real `.c` gets indexed by clangd and read as a cart by anything globbing for sources.
const CARTS_DIR   = process.env.DE_MOBILE_CARTS_DIR   || path.join(ROOT, 'tools', 'carts')
const RUNTIME_DIR = process.env.DE_MOBILE_RUNTIME_DIR || path.join(ROOT, 'runtime')
const CART_EXT    = process.env.DE_MOBILE_CART_EXT    || '.c'
const SITE_DIR    = path.join(ROOT, 'site')

function stripComments(src) {
  return src.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '')
}

// inline quote-included runtime/ library headers (ui.h, gestures.h, …) so
// their input reads count as the cart's; studio.h skipped (declarations only)
function inlineRuntimeIncludes(src, seen) {
  seen = seen || new Set()
  return src.replace(/#include\s+"([^"]+\.h)"/g, (m, h) => {
    if (h === 'studio.h' || seen.has(h)) return ''
    const f = path.join(RUNTIME_DIR, h)
    if (!fs.existsSync(f)) return ''
    seen.add(h)
    return inlineRuntimeIncludes(stripComments(fs.readFileSync(f, 'utf8')), seen)
  })
}

function lint(name) {
  const srcFile = path.join(CARTS_DIR, `${name}${CART_EXT}`)
  if (!fs.existsSync(srcFile)) return { name, verdict: 'MISSING', warnings: [] }
  const src = inlineRuntimeIncludes(stripComments(fs.readFileSync(srcFile, 'utf8')))
  const has = (re) => re.test(src)

  // .cart.js settings (touchControls)
  let cfgTouch = false
  const cfgFile = path.join(CARTS_DIR, `${name}.cart.js`)
  if (fs.existsSync(cfgFile)) {
    // path.resolve, not the raw join: require() treats a bare RELATIVE path as a module NAME and
    // throws MODULE_NOT_FOUND, which the catch below swallows — so a relative CARTS_DIR (now
    // possible via DE_MOBILE_CARTS_DIR) would silently read touchControls as false and downgrade
    // a touch-ready cart to `fixable`. Production always passed an absolute path, so this only
    // became reachable with the override; it cost one confused probe to find.
    try { cfgTouch = !!require(path.resolve(cfgFile)).touchControls } catch {}
  }

  const reads = {
    btn:      has(/\bbtnp?r?\s*\(/),
    key:      has(/\bkeyp?r?\s*\(/),
    mousePos: has(/\bmouse_(x|y|world_x|world_y)\s*\(/),
    mouseBtn: has(/\bmouse_(down|pressed|released)\s*\(/),
    wheel:    has(/\bmouse_wheel\s*\(/),
    touch:    has(/\btouch_(count|x|y|id)\s*\(/) || has(/\btapp?\s*\(/),
    tc:       has(/\btouch_controls\s*\(/) || cfgTouch,
  }

  const warnings = []
  if (reads.mousePos && !reads.mouseBtn && !reads.touch)
    warnings.push('hover')
  if (reads.wheel)
    warnings.push('wheel')
  if (has(/\bMOUSE_(RIGHT|MIDDLE)\b/) || has(/\bmouse_(down|pressed|released)\s*\(\s*[12]\s*\)/))
    warnings.push('right/middle')
  for (const m of src.matchAll(/\btouch_(?:x|y|id)\s*\(\s*(\d+)\s*\)/g))
    if (+m[1] >= 5) { warnings.push('touch>5'); break }
  for (const m of src.matchAll(/\btapp?\s*\([^;]*?,\s*(\d+)\s*,\s*(\d+)\s*\)/g))
    if (+m[1] < 16 || +m[2] < 16) { warnings.push(`tiny-target(${m[1]}x${m[2]})`); break }

  // verdict = the BEST input path a phone can use; key()-only is the worst case.
  // (a cart that grabs with the mouse but spawns with 'C' is still mostly playable —
  // rank by playability, surface the rest as warnings)
  let verdict
  if (reads.touch || reads.tc)            verdict = 'touch-ready'
  else if (reads.mouseBtn)                verdict = 'tap-as-mouse'
  else if (reads.btn)                     verdict = 'fixable'
  else if (reads.key)                     verdict = 'keyboard-only'
  else                                    verdict = 'no-input'

  // key()/btn() features alongside a better path are dead on phone — note them
  if (verdict !== 'keyboard-only' && reads.key) warnings.push('also-reads-keys')
  if ((verdict === 'touch-ready' || verdict === 'tap-as-mouse') && reads.btn && !reads.tc)
    warnings.push('btn-without-overlay')

  // which keys, exactly — the manual-testing worklist / keycap-retrofit shopping
  // list (mobile-web-notes §6b). Literal args only; dynamic args show as '?'.
  if (reads.key) {
    const keys = new Set()
    for (const m of src.matchAll(/\bkey[pr]?\s*\(\s*([^)]*)\)/g)) {
      const arg = m[1].trim()
      let k = /^KEY_(\w+)$/.exec(arg)?.[1] ?? /^'(.)'$/.exec(arg)?.[1]
      keys.add(k ?? '?')
    }
    warnings.push(`keys(${[...keys].sort().join(',')})`)

    // which of those literal keys lack an INLINE tap/touch alternative — the
    // "could this label be tappable?" checklist (see header for caveats)
    if (verdict === 'touch-ready' || verdict === 'tap-as-mouse') {
      const untapped = new Set()
      for (const line of src.split('\n')) {
        if (/\btapp?\s*\(|\btouch_/.test(line)) continue
        for (const m of line.matchAll(/\bkey[pr]?\s*\(\s*(KEY_\w+|'(?:[^'\\]|\\.)')\s*\)/g)) {
          const arg = m[1]
          untapped.add(arg.startsWith('KEY_') ? arg.slice(4) : arg.slice(1, -1))
        }
      }
      if (untapped.size)
        warnings.push(`keys-untapped(${[...untapped].sort().join(',')})`)
    }
  }

  // text_input() never sees a phone — no OS keyboard on canvas (§6c)
  if (has(/\btext_input\s*\(/)) warnings.push('text-input')

  return { name, verdict, warnings }
}

module.exports = { lint }   // build-site.js badges the gallery with these verdicts
if (require.main !== module) return

// ── --selfcheck: assert the CHECKER against known answers ─────────────────────
// See docs/guides/checks-and-oracles.md "Self-test the checker". Re-runs the tool as a child with
// DE_MOBILE_* pointed at tools/fixtures/mobile-lint/, one fixture cart per judgement.
//
// WHY. The verdict is a PRECEDENCE CHAIN — a cart reading touch AND mouse AND btn AND key must
// rank by the BEST path a phone can use, not the worst — and the whole thing runs on regexes over
// preprocessed source with three transforms in front of it (comment stripping, library-header
// inlining, and a studio.h SKIP without which every cart in the repo reads touch-ready). Get the
// skip wrong and the tool cheerfully reports a green shelf. Nothing measured it.
if (process.argv.includes('--selfcheck')) {
  const cp = require('child_process')
  const FX = path.join(__dirname, 'fixtures', 'mobile-lint')
  let raw
  try {
    raw = cp.execFileSync(process.execPath, [__filename, '--all'], {
      // ABSOLUTE paths: require() reads a bare relative path as a module name (see the
      // path.resolve note in lint()), and the fixture's .cart.js must actually load.
      env: { ...process.env,
             DE_MOBILE_CARTS_DIR:   path.join(FX, 'carts'),
             DE_MOBILE_RUNTIME_DIR: path.join(FX, 'runtime'),
             DE_MOBILE_CART_EXT:    '.c.txt' },
      encoding: 'utf8', maxBuffer: 1 << 24,
    })
  } catch (e) { raw = e.stdout || '' }

  // parse the grouped report back into {cart: {verdict, warnings}}
  const got = {}
  let cur = null
  for (const line of raw.split('\n')) {
    const h = /^── (\S+) \(\d+\) ──$/.exec(line.trim())
    if (h) { cur = h[1]; continue }
    const m = /^  (\S+)\s*(.*)$/.exec(line)
    if (cur && m && !line.startsWith('  tip:'))
      // split on ', ' (comma-SPACE), not ',': the report joins warnings with ', ' while a
      // warning's own PAYLOAD uses bare commas — `keys(A,SPACE)`. Splitting on ',' shreds it
      // into "keys(A" + "SPACE)" and the keys() assertion can never match.
      got[m[1]] = { verdict: cur, warnings: m[2].trim() ? m[2].split(', ').map(w => w.trim()) : [] }
  }

  const T = []
  const t = (n, ok) => T.push({ n, ok })
  const v  = (cart) => (got[cart] || {}).verdict
  const w  = (cart) => (got[cart] || {}).warnings || []
  const has = (cart, warn) => w(cart).some(x => x === warn || x.startsWith(warn + '('))

  // ── all five verdicts, each on a cart that reads exactly one input path
  t('parsed the fixture shelf  [blind-pass guard]', Object.keys(got).length === 12)
  t('verdict: touch_*() reads → touch-ready', v('touchy') === 'touch-ready')
  t('verdict: mouse button only → tap-as-mouse  [a tap IS a click]', v('mousey') === 'tap-as-mouse')
  t('verdict: btn() only → fixable  [one line of config away]', v('btnonly') === 'fixable')
  t('verdict: key() only → keyboard-only  [the overlay cannot synthesize keys]',
    v('keyonly') === 'keyboard-only')
  t('verdict: reads nothing → no-input', v('silent') === 'no-input')

  // ── the precedence chain: rank by the BEST path, not the worst
  t('precedence: a cart reading touch+mouse+btn+key ranks touch-ready  [best path wins]',
    v('precedence') === 'touch-ready')
  t('precedence: ...and the dead key path is surfaced as a warning, not buried',
    has('precedence', 'also-reads-keys'))
  t('precedence: ...and btn() without the overlay is flagged',
    has('precedence', 'btn-without-overlay'))
  t('precedence: btn-without-overlay does NOT fire when touchControls is set  [exempt class]',
    !has('cfgtouch', 'btn-without-overlay'))

  // ── the three source transforms in front of every regex
  t('transform: a COMMENTED-OUT input read does not count  [stripComments]',
    v('commented') === 'no-input' && w('commented').length === 0)
  t('transform: a quote-included library header IS inlined  [an all-widget cart lints green]',
    v('viaui') === 'touch-ready')
  t('transform: studio.h is SKIPPED  [else its prototypes make EVERY cart touch-ready]',
    v('viastudio') === 'no-input')
  t('transform: ...and the fixture studio.h really does name every input fn  [inert-fixture guard]',
    (() => {
      // `studio.h.txt`, not `studio.h`: the tool never READS this file (studio.h is skipped
      // before the path lookup), it is only evidence that the skip has something to skip. Named
      // with the real extension it also tripped the pre-commit hook's unanchored
      // `runtime/(sound|studio)\.h` match on every commit touching the fixture.
      const sh = fs.readFileSync(path.join(FX, 'runtime', 'studio.h.txt'), 'utf8')
      return ['touch_count', 'touch_x', 'tap', 'btn', 'key', 'mouse_down', 'mouse_wheel', 'text_input']
        .every(fn => sh.includes(fn + '('))
    })())
  t('config: .cart.js touchControls promotes a btn()-only cart to touch-ready',
    v('cfgtouch') === 'touch-ready')

  // ── the warning classes
  t('warn: hover — mouse position with no button and no touch', has('hovery', 'hover'))
  t('warn: ...and hover does NOT fire when a button IS read  [exempt class]',
    !has('mousey', 'hover'))
  t('warn: wheel — no scroll wheel on a phone', has('warnings', 'wheel'))
  t('warn: right/middle — no second button on a touch screen', has('warnings', 'right/middle'))
  t('warn: touch>5 — iOS Safari caps at ~5 simultaneous touches', has('warnings', 'touch>5'))
  t('warn: ...and a touch index UNDER 5 is not flagged  [threshold guard]',
    !has('touchy', 'touch>5'))
  t('warn: tiny-target — a tap target under 16 canvas px, with its size', has('warnings', 'tiny-target'))
  t('warn: text-input — no OS keyboard over the canvas', has('warnings', 'text-input'))
  t('warn: keys(...) lists the literal keys read', w('keyonly').some(x => x === 'keys(A,SPACE)'))
  t('warn: keys-untapped names keys with no inline tap alternative',
    w('precedence').some(x => x === 'keys-untapped(Z)'))
  t('warn: ...and EXCLUDES a key sharing its line with a tap()  [Q is tappable already]',
    w('precedence').some(x => x === 'keys(Q,Z)') &&
    !w('precedence').some(x => x.includes('keys-untapped(Q')))
  t('warn: a clean cart carries NO warnings  [cry-wolf guard]',
    w('touchy').length === 0 && w('silent').length === 0 && w('btnonly').length === 0)

  const failed = T.filter(x => !x.ok)
  for (const x of T) console.log(`  ${x.ok ? '\x1b[32m✓\x1b[0m' : '\x1b[31m✗\x1b[0m'} ${x.n}`)
  console.log(failed.length
    ? `\x1b[31mmobile-lint --selfcheck FAILED\x1b[0m — ${failed.length} of ${T.length} expectations broken`
    : `mobile-lint --selfcheck: ${T.length}/${T.length} known answers correct`)
  process.exit(failed.length ? 1 : 0)
}

// ── target selection ──────────────────────────────────────────
const argv = process.argv.slice(2)
let names
if (argv.includes('--site')) {
  names = fs.existsSync(SITE_DIR)
    ? fs.readdirSync(SITE_DIR, { withFileTypes: true })
        .filter(d => d.isDirectory() && fs.existsSync(path.join(SITE_DIR, d.name, 'index.html')))
        .map(d => d.name)
    : []
} else if (argv.includes('--all')) {
  names = fs.readdirSync(CARTS_DIR).filter(f => f.endsWith(CART_EXT)).map(f => f.slice(0, -CART_EXT.length))
} else {
  names = argv.filter(a => !a.startsWith('--'))
}
if (names.length === 0) {
  console.log('usage: node tools/mobile-lint.js <name>... | --site | --all')
  process.exit(1)
}

// ── report ────────────────────────────────────────────────────
const ORDER = ['keyboard-only', 'fixable', 'tap-as-mouse', 'touch-ready', 'no-input', 'MISSING']
const results = names.sort().map(lint)
const wName = Math.max(...results.map(r => r.name.length), 4)

for (const v of ORDER) {
  const group = results.filter(r => r.verdict === v)
  if (!group.length) continue
  console.log(`\n── ${v} (${group.length}) ──`)
  for (const r of group)
    console.log(`  ${r.name.padEnd(wName)}  ${r.warnings.join(', ')}`)
}
const counts = ORDER.map(v => [v, results.filter(r => r.verdict === v).length]).filter(([, n]) => n)
console.log(`\n${results.length} carts: ` + counts.map(([v, n]) => `${n} ${v}`).join(', '))

if (results.some(r => r.warnings.some(w => w.startsWith('tiny-target')))) {
  console.log('\ntip: tiny-target carts — a ui.h widget panel can add ui_loupe(1), a drag-out')
  console.log('     magnifier for fat fingers; swept grids/keybeds want pinch-zoom')
  console.log('     (gestures.h pinch_scale + camera_ex). See docs/design/loupe-notes.md.')
}
