#!/usr/bin/env node
// mobile-lint.js — static report card: how is this cart not perfect for mobile?
// Design + check rationale: docs/design/mobile-web-notes.md §3.
//
//   node tools/mobile-lint.js <name> [<name>...]   lint specific carts
//   node tools/mobile-lint.js --site               lint every cart published in site/
//   node tools/mobile-lint.js --all                lint every cart in tools/carts/
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
//                  ≈ 15–17 canvas px on a 320-wide cart fullscreen on an iPhone)
//
// v1 is grep-grade and informational (always exits 0): it produces a worklist,
// not a gate. Known gap (manual check): key-gated title screens — "press Z to
// start" — on otherwise touch-capable carts.

const fs   = require('fs')
const path = require('path')

const ROOT      = path.join(__dirname, '..')
const CARTS_DIR = path.join(ROOT, 'tools', 'carts')
const SITE_DIR  = path.join(ROOT, 'site')

function stripComments(src) {
  return src.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '')
}

function lint(name) {
  const srcFile = path.join(CARTS_DIR, `${name}.c`)
  if (!fs.existsSync(srcFile)) return { name, verdict: 'MISSING', warnings: [] }
  const src = stripComments(fs.readFileSync(srcFile, 'utf8'))
  const has = (re) => re.test(src)

  // .cart.js settings (touchControls)
  let cfgTouch = false
  const cfgFile = path.join(CARTS_DIR, `${name}.cart.js`)
  if (fs.existsSync(cfgFile)) {
    try { cfgTouch = !!require(cfgFile).touchControls } catch {}
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

  return { name, verdict, warnings }
}

module.exports = { lint }   // build-site.js badges the gallery with these verdicts
if (require.main !== module) return

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
  names = fs.readdirSync(CARTS_DIR).filter(f => f.endsWith('.c')).map(f => f.slice(0, -2))
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
