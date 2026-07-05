#!/usr/bin/env node
// ui-audit.js — find UI bugs a screenshot wouldn't tell you about: text that runs
// off the screen edge (so it's silently clipped to nothing), text labels that
// overlap, widgets that draw but never respond to clicks (a missing ui_end() —
// surfaced from ui.h's DE_TRACE tripwire), and — with --explore — panels that
// only open on a keypress or a tap.
//
// Runs the cart headless under runtime/studio.c's --uiaudit mode (via play.js),
// which logs every print()/spr()/rect()/circ() bounding box per frame, plus each
// ui.h widget rect (so the auditor knows which boxes are interactive targets).
//
//   node tools/ui-audit.js <name> [--frames N] [--script f | --beats f] [--json]
//   node tools/ui-audit.js <name> --resize WxH,WxH,…        AUDIT A RESIZE SWEEP — reflow the
//                                                           canvas through each size (resizable
//                                                           carts) and flag off-screen / overlap
//                                                           AT EVERY SIZE; findings are tagged with
//                                                           the size they occur at. The responsive
//                                                           layout gate (device-adaptive-layout.md).
//   node tools/ui-audit.js <name> --explore                 press keys + tap widgets to reveal panels
//   node tools/ui-audit.js <name> --overlay [out.svg] [--frame N]   visualise the boxes
//
// Honest caveats:
//   • Coverage = the frames it sees. A clip bug that only shows on a long string
//     or a deep menu needs that state reached — --explore or --beats gets you there.
//   • Draw coords are raw (pre-camera); a HUD scrolled with camera() reports
//     world-space boxes. UI is usually drawn camera-free.
//   • Text inside an active clip() scissor is treated as intentionally bounded.
//   • --explore taps TOP-LEVEL widgets and presses each read key once; it won't
//     chase arbitrarily deep nested menus, and state can carry between taps.

const fs   = require('fs')
const path = require('path')
const os   = require('os')
const { spawnSync } = require('child_process')

const args = process.argv.slice(2)
const name = args[0]
if (!name || name.startsWith('--')) {
  console.error('usage: node tools/ui-audit.js <name> [--frames N] [--explore] [--overlay [out.svg]] [--script f|--beats f] [--json]')
  process.exit(1)
}
const opt = (flag, def) => { const i = args.indexOf(flag); return i >= 0 && i + 1 < args.length ? args[i + 1] : def }
const asJson      = args.includes('--json')
const wantOverlay = args.includes('--overlay')
const wantExplore = args.includes('--explore')
const resizeSpec  = opt('--resize', null)   // "WxH,WxH,…" → audit the reflow at every size
const overlayArg  = (() => { const i = args.indexOf('--overlay'); const v = args[i + 1]; return (v && !v.startsWith('--')) ? v : null })()

const ROOT = path.resolve(__dirname, '..')

// ── run the cart once under --uiaudit, return the parsed per-frame records ──
// ui.h's DE_TRACE tripwire prints "[ui] WARNING: …" to stdout when a frame draws
// widgets but never calls ui_end() (clicks silently dead). We pipe stdout and
// collect those across every run() so the report can surface them.
const uiWarnings = new Set()
let runSeq = 0
function run(inMode, frames, dumpDir, resize) {
  const auditPath = path.join(os.tmpdir(), `uiaudit-${name}-${process.pid}-${runSeq++}.jsonl`)
  const play = [path.join('tools', 'play.js'), name, ...inMode,
                '--headless', '--frames', String(frames), '--uiaudit', auditPath]
  if (resize) play.push('--resize', resize)   // sweep sizes; each held RESIZE_HOLD frames, all captured
  if (dumpDir) play.push('--dump', dumpDir, '--dump-every', '1')
  const r = spawnSync('node', play, { cwd: ROOT, stdio: ['ignore', 'pipe', 'inherit'] })
  if (r.stdout) for (const line of r.stdout.toString().split('\n'))
    if (line.startsWith('[ui] WARNING')) uiWarnings.add(line.trim())
  if (r.status !== 0 || !fs.existsSync(auditPath)) {
    console.error(`ui-audit: cart "${name}" failed to run (see compile output above)`); process.exit(1)
  }
  const recs = fs.readFileSync(auditPath, 'utf8').split('\n').filter(Boolean)
    .map(l => { try { return JSON.parse(l) } catch { return null } }).filter(Boolean)
  fs.unlinkSync(auditPath)
  if (!recs.length) { console.error('ui-audit: no frames captured'); process.exit(1) }
  return recs
}

// ── discovery: keys the cart reads, widget centres it draws ─────────────────
const KEY_TOKENS = { SPACE:'SPACE', ENTER:'ENTER', TAB:'TAB', ESCAPE:'ESCAPE', BACKSPACE:'BACKSPACE',
                     LEFT:'LEFT', RIGHT:'RIGHT', UP:'UP', DOWN:'DOWN', COMMA:'COMMA', PERIOD:'PERIOD' }
function discoverKeys() {
  const src = fs.readFileSync(path.join(ROOT, 'tools', 'carts', `${name}.c`), 'utf8')
    .replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '')
  const seen = new Map()
  for (const m of src.matchAll(/\bkey[pr]?\s*\(\s*'(.)'/g)) { if (m[1] !== '\\') seen.set(m[1], `'${m[1]}'`) }
  for (const m of src.matchAll(/\bkey[pr]?\s*\(\s*KEY_([A-Z]+)/g)) { const t = KEY_TOKENS[m[1]]; if (t) seen.set(t, `KEY_${m[1]}`) }
  return [...seen].map(([token, label]) => ({ token, label }))
}
// linter-style in-source suppressions, one per line in the cart .c:
//   // ui-audit-ignore off "SCOPE" bottom    — reason
//   // ui-audit-ignore overlap "env" "vb"    — reason
// Matched by finding IDENTITY (kind + text + side), never by pixel position, so a
// waiver survives layout jitter but a genuinely new off-screen string still trips.
// `off` with no side waives any edge. Each waiver tracks whether it fired (stale = clean up).
const SIDES = new Set(['left', 'top', 'right', 'bottom'])
function parseWaivers() {
  const src = fs.readFileSync(path.join(ROOT, 'tools', 'carts', `${name}.c`), 'utf8')
  const out = []
  for (const m of src.matchAll(/\/\/\s*ui-audit-ignore\s+(.+)$/gm)) {
    const body = m[1].trim()
    const kind = body.split(/\s+/)[0]
    const strs = [...body.matchAll(/"([^"]*)"/g)].map(q => q[1])
    const side = body.split(/\s+/).slice(1).find(t => SIDES.has(t))
    const reason = (body.split(/—|--|#|reason:/)[1] || '').trim()
    if (kind === 'off' && strs.length >= 1)        out.push({ kind, text: strs[0], side, reason, used: false, raw: body })
    else if (kind === 'overlap' && strs.length >= 2) out.push({ kind, pair: [strs[0], strs[1]].sort(), reason, used: false, raw: body })
    else out.push({ kind: 'bad', raw: body })   // malformed → surfaced as a warning
  }
  return out
}
const waiveOff  = (f, w) => w.kind === 'off' && w.text === f.text && (!w.side || f.sides.includes(w.side))
const waiveOver = (f, w) => w.kind === 'overlap' && w.pair[0] === [f.a, f.b].sort()[0] && w.pair[1] === [f.a, f.b].sort()[1]

// unique widget centres across a run (top-level targets to tap)
function widgetTargets(recs) {
  const seen = new Map()
  for (const rec of recs)
    for (const e of rec.d || []) {
      if (e.k !== 'w') continue
      if (e.w < 3 || e.h < 3 || e.w > 280 || e.h > 220) continue   // skip full-screen panels / slivers
      const cx = e.x + (e.w >> 1), cy = e.y + (e.h >> 1)
      seen.set(`${cx},${cy}`, { cx, cy })
    }
  return [...seen.values()]
}

// ── plan the session, run, collect per-frame records ────────────────────────
let exploreKeys = [], exploreTaps = [], timeline = [], exploreScript = null, frames
const dumpDir = wantOverlay ? path.join(os.tmpdir(), `uiaudit-shots-${name}-${process.pid}`) : null
const stateRanges = []   // matrix mode: frame-range → which key opened that state
let recs

if (wantExplore && resizeSpec) {
  // RESPONSIVE MATRIX: audit every revealed state at every size. The --resize
  // sweep runs ONCE (it clamps at the last size), so do one sweep PER state —
  // the default view plus each discovered key pressed first — and stitch the
  // per-run records together, reindexing frames so they don't collide.
  exploreKeys = discoverKeys()
  const nSizes = resizeSpec.split(',').length
  const sweepFrames = nSizes * 8 + 10                        // RESIZE_HOLD(8)/size + settle
  const states = [{ label: 'default', token: null }, ...exploreKeys]
  recs = []; let base = 0
  for (const st of states) {
    const scr = path.join(os.tmpdir(), `uiaudit-state-${name}-${process.pid}-${runSeq}.script`)
    fs.writeFileSync(scr, st.token ? `tap 2 ${st.token} 3\n` : '# default state\n')
    const r = run(['script', scr], sweepFrames, null, resizeSpec)
    try { fs.unlinkSync(scr) } catch {}
    stateRanges.push({ label: st.label, lo: base, hi: base + sweepFrames + 1 })
    for (const rec of r) { rec.f += base; recs.push(rec) }
    base += sweepFrames + 1
  }
} else if (wantExplore) {
  exploreKeys = discoverKeys()
  exploreTaps = widgetTargets(run(['script', '/dev/null'], 16)).slice(0, 24)   // baseline pass harvests targets
  if (!exploreKeys.length && !exploreTaps.length) {
    console.error(`ui-audit: --explore found no keys or ui.h widgets in ${name} to drive`); process.exit(1)
  }
  const lines = ['# auto-explore']
  let f = 8
  for (const k of exploreKeys) {
    timeline.push({ kind: 'key', label: k.label, frame: f })
    lines.push(`tap ${f} ${k.token} 3`, `tap ${f + 9} ${k.token} 3`)   // open, then toggle back
    f += 18
  }
  for (const t of exploreTaps) {
    timeline.push({ kind: 'tap', label: `tap(${t.cx},${t.cy})`, frame: f })
    lines.push(`click ${f} ${t.cx} ${t.cy}`, `tap ${f + 10} ESCAPE 3`)  // tap, then try to dismiss
    f += 20
  }
  exploreScript = path.join(os.tmpdir(), `uiaudit-explore-${name}-${process.pid}.script`)
  fs.writeFileSync(exploreScript, lines.join('\n') + '\n')
  frames = Math.max(+opt('--frames', 0), f + 16)
  recs = run(['script', exploreScript], frames, dumpDir, resizeSpec)
  try { fs.unlinkSync(exploreScript) } catch {}
} else {
  frames = +opt('--frames', 120)
  const inMode = opt('--beats', null) ? ['beats', opt('--beats')]
               : opt('--script', null) ? ['script', opt('--script')]
               : ['script', '/dev/null']
  recs = run(inMode, frames, dumpDir, resizeSpec)
}

// ── analyse ─────────────────────────────────────────────────────────────────
const overlaps = (a, b) => a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h
const contains = (f, t) => f.x <= t.x && f.y <= t.y && f.x + f.w >= t.x + t.w && f.y + f.h >= t.y + t.h
// the VISIBLE text in a frame: drop any string that a LATER fill fully covers
// (a modal/backdrop painted on top of a previous screen). Draw order = array
// order, so a fill at a higher index sits on top. Kills "text-behind-a-panel"
// false positives. A widget's own label is drawn AFTER its fill, so it survives.
function visibleText(d) {
  const fills = [], out = []
  d.forEach((e, i) => { if (e.k === 'f') fills.push({ ...e, i }) })
  d.forEach((e, i) => {
    if (e.k !== 't' || !e.t) return
    if (fills.some(fl => fl.i > i && contains(fl, e))) return   // occluded
    out.push(e)
  })
  return out
}
const offscreen = new Map(), collide = new Map(), byFrame = new Map(), sigByFrame = new Map()
let SW = 0, SH = 0, framesSeen = 0

for (const rec of recs) {
  framesSeen++; SW = rec.sw; SH = rec.sh
  const d = rec.d || [], f = rec.f
  byFrame.set(f, rec)
  const texts = visibleText(d)
  sigByFrame.set(f, new Set(texts.map(e => e.t)))

  for (const e of texts) {                         // (1) text off the screen edge
    if (e.c) continue                              // inside clip() → bounded on purpose
    const sides = []
    if (e.x < 0)        sides.push('left')
    if (e.y < 0)        sides.push('top')
    if (e.x + e.w > SW) sides.push('right')
    if (e.y + e.h > SH) sides.push('bottom')
    if (!sides.length) continue
    const key = `${e.t}@${e.x},${e.y}`, hit = offscreen.get(key)
    if (hit) { hit.last = f; hit.n++ }
    else offscreen.set(key, { text: e.t, x: e.x, y: e.y, w: e.w, h: e.h, sides, first: f, last: f, n: 1 })
  }
  for (let i = 0; i < texts.length; i++)           // (2) overlapping text labels
    for (let j = i + 1; j < texts.length; j++) {
      const a = texts[i], b = texts[j]
      if (a.t === b.t || !overlaps(a, b)) continue
      const key = [a.t, b.t].sort().join(' ∩ '), hit = collide.get(key)
      if (hit) { hit.last = f; hit.n++ } else collide.set(key, { a: a.t, b: b.t, first: f, last: f, n: 1 })
    }
}

// ── explore: which inputs made new UI appear? ───────────────────────────────
const discovered = []
for (const t of timeline) {
  const before = sigByFrame.get(t.frame - 1) || new Set()
  let best = null
  for (let df = 1; df <= 8; df++) {
    const s = sigByFrame.get(t.frame + df); if (!s) continue
    const added = [...s].filter(x => !before.has(x))
    if (!best || added.length > best.added.length) best = { frame: t.frame + df, added }
  }
  if (best && best.added.length >= 3)
    discovered.push({ via: t.label, frame: best.frame, count: best.added.length, sample: best.added.slice(0, 6) })
}

// ── overlay (--overlay): screenshot + boxes as a self-contained SVG ─────────
function buildOverlay() {
  const target = opt('--frame', null) != null ? +opt('--frame')
               : discovered.length ? discovered[0].frame   // default to a discovered panel if exploring
               : Math.max(...byFrame.keys())
  const rec = byFrame.get(target)
  if (!rec) { console.error(`ui-audit: no frame ${target} for overlay`); return null }
  const shot = path.join(dumpDir, `frame_${String(target).padStart(5, '0')}.png`)
  const bg = fs.existsSync(shot)
    ? `<image x="0" y="0" width="${SW}" height="${SH}" image-rendering="pixelated" xlink:href="data:image/png;base64,${fs.readFileSync(shot).toString('base64')}"/>`
    : `<rect x="0" y="0" width="${SW}" height="${SH}" fill="#111"/>`
  const d = rec.d || [], texts = visibleText(d)
  const vis = new Set(texts)                       // occluded text → discounted (drawn dim)
  const isOff  = (e) => !e.c && (e.x < 0 || e.y < 0 || e.x + e.w > SW || e.y + e.h > SH)
  const isOver = (e) => texts.some(o => o !== e && o.t !== e.t && overlaps(e, o))
  const fillCol = { f: '#3a78ff', R: '#22d3ee', s: '#888', c: '#a855f7' }
  const esc = (s) => s.replace(/[<&]/g, c => c === '<' ? '&lt;' : '&amp;')
  const boxes = d.map(e => {
    if (e.k === 'w')
      return `<rect x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}" fill="#ffd60a" fill-opacity="0.12" stroke="#ffd60a" stroke-width="0.8"><title>widget${e.t === '1' ? ' (focusable)' : ''}</title></rect>`
    if (e.k === 't') {
      const col = !vis.has(e) ? '#555' : isOff(e) ? '#ff3b3b' : isOver(e) ? '#ff9f1c' : '#39d353'
      return `<rect x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}" fill="none" stroke="${col}" stroke-opacity="${vis.has(e) ? 1 : 0.5}" stroke-width="0.6"><title>"${esc(e.t)}"${vis.has(e) ? '' : ' (occluded)'}</title></rect>`
    }
    const col = fillCol[e.k] || '#bbb'
    return `<rect x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}" fill="${col}" fill-opacity="0.10" stroke="${col}" stroke-opacity="0.7" stroke-width="0.5"/>`
  })
  const M = 24, SCALE = 3
  const svg =
`<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink"
     width="${(SW + 2 * M) * SCALE}" height="${(SH + 2 * M) * SCALE}"
     viewBox="${-M} ${-M} ${SW + 2 * M} ${SH + 2 * M}" shape-rendering="crispEdges">
  <rect x="${-M}" y="${-M}" width="${SW + 2 * M}" height="${SH + 2 * M}" fill="#1d1d22"/>
  ${bg}
  <rect x="0" y="0" width="${SW}" height="${SH}" fill="none" stroke="#fff" stroke-opacity="0.5" stroke-width="0.7"/>
  ${boxes.join('\n  ')}
  <text x="${-M + 1}" y="${-M + 7}" font-family="monospace" font-size="6" fill="#fff">${name} f${target} — widget=yellow panel=blue text=green off=red overlap=orange</text>
</svg>`
  const out = path.resolve(overlayArg || path.join(ROOT, 'build', '.bake', `${name}-ui.svg`))
  fs.mkdirSync(path.dirname(out), { recursive: true })
  fs.writeFileSync(out, svg)
  return { out, target, widgets: d.filter(e => e.k === 'w').length }
}
let overlayInfo = null
if (wantOverlay) {
  overlayInfo = buildOverlay()
  try { for (const fn of fs.readdirSync(dumpDir)) fs.unlinkSync(path.join(dumpDir, fn)); fs.rmdirSync(dumpDir) } catch {}
}

// ── transient filter: a real layout bug sits still; something on screen for
// only a frame or two is mid-animation (a card dealing in, a number sliding).
// Require a finding to persist >= minFrames. Off-screen findings are keyed by
// position, so a moving element makes many 1-frame entries that all fall here;
// a static clip keeps one entry with a high count. --min-frames 1 shows everything.
const minFrames = Math.max(1, Math.min(+opt('--min-frames', 3), framesSeen))
const persists = (f) => f.n >= minFrames

// ── waivers: partition findings into live vs suppressed ─────────────────────
const waivers = parseWaivers()
const badWaivers = waivers.filter(w => w.kind === 'bad')
const isWaived = (f, pred) => { const w = waivers.find(w => pred(f, w)); if (w) { w.used = true; return true } return false }
const allOff = [...offscreen.values()], allCol = [...collide.values()]
const offSteady = allOff.filter(persists), colSteady = allCol.filter(persists)
const transientN = (allOff.length - offSteady.length) + (allCol.length - colSteady.length)
const offList = offSteady.filter(f => !isWaived(f, waiveOff))
const colList = colSteady.filter(f => !isWaived(f, waiveOver))
const waivedN = (offSteady.length - offList.length) + (colSteady.length - colList.length)
const stale   = waivers.filter(w => w.kind !== 'bad' && !w.used)

// ── report ──────────────────────────────────────────────────────────────────
if (asJson) {
  console.log(JSON.stringify({ cart: name, framesSeen, minFrames, screen: { w: SW, h: SH }, sizesSwept: [...new Set(recs.map(r => `${r.sw}×${r.sh}`))],
    offscreenText: offList, textOverlap: colList, uiLifecycle: [...uiWarnings],
    waived: waivedN, transient: transientN, staleWaivers: stale.map(w => w.raw), badWaivers: badWaivers.map(w => w.raw),
    explored: wantExplore ? { keys: exploreKeys.map(k => k.label), taps: exploreTaps.length, discovered } : undefined },
    null, 2))
  process.exit(offList.length || colList.length || uiWarnings.size ? 1 : 0)
}

// size attribution — with a --resize sweep, tag each finding with the canvas size
// it occurred at (the reflow bug is only meaningful next to the size that caused it)
const sizeAt = (f) => { const r = byFrame.get(f); return r ? `${r.sw}×${r.sh}` : '?' }
const stateAt = (f) => { for (const r of stateRanges) if (f >= r.lo && f < r.hi) return r.label; return null }
const sizesSeen = [...new Set(recs.map(r => `${r.sw}×${r.sh}`))]
const multi = sizesSeen.length > 1
const span = (o) => {
  const fr = o.first === o.last ? `frame ${o.first}` : `frames ${o.first}–${o.last}`
  const st = stateAt(o.first)
  const tag = [st && st !== 'default' ? `key ${st}` : null, multi ? sizeAt(o.first) : null].filter(Boolean).join(' · ')
  return tag ? `${tag} · ${fr}` : fr
}
console.log(`\nui-audit: ${name}  (${multi ? sizesSeen.join(' · ') : SW + '×' + SH}, ${framesSeen} frames seen)\n`)

if (offList.length) {
  console.log(`  ✘ ${offList.length} text string(s) run off the screen edge:`)
  for (const o of offList)
    console.log(`      "${o.text}"  at (${o.x},${o.y}) ${o.w}px wide → past ${o.sides.join('+')}  [${span(o)}]`)
  console.log('')
}
if (colList.length) {
  console.log(`  ⚠ ${colList.length} pair(s) of overlapping text:`)
  for (const c of colList) console.log(`      "${c.a}"  overlaps  "${c.b}"  [${span(c)}]`)
  console.log('')
}
if (!offList.length && !colList.length) console.log('  ✓ no off-screen or overlapping text found.')

if (uiWarnings.size) {
  console.log(`\n  ✘ ${uiWarnings.size} ui.h lifecycle bug(s) — widgets render but won't respond to clicks (only hover):`)
  for (const w of uiWarnings) console.log(`      ${w}`)
}

if (transientN)     console.log(`  · ${transientN} transient finding(s) hidden (< ${minFrames} frames — likely mid-animation; --min-frames 1 to show)`)
if (waivedN)        console.log(`  · ${waivedN} finding(s) waived by // ui-audit-ignore`)
if (stale.length) {
  console.log(`  ⚑ ${stale.length} stale waiver(s) — matched nothing this run, delete or fix:`)
  for (const w of stale) console.log(`      // ui-audit-ignore ${w.raw}`)
}
if (badWaivers.length) {
  console.log(`  ⚑ ${badWaivers.length} malformed waiver(s) (expected: off "TEXT" [side] | overlap "A" "B"):`)
  for (const w of badWaivers) console.log(`      // ui-audit-ignore ${w.raw}`)
}
if (transientN || waivedN || stale.length || badWaivers.length) console.log('')

if (wantExplore) {
  console.log(`  ⌨ explored ${exploreKeys.length} key(s)${exploreTaps.length ? ` + ${exploreTaps.length} widget tap(s)` : ''}` +
              (exploreKeys.length ? `: ${exploreKeys.map(k => k.label).join(' ')}` : ''))
  if (discovered.length) {
    console.log(`    inputs that opened new UI:`)
    for (const d of discovered)
      console.log(`      ${d.via}  → +${d.count} labels at frame ${d.frame}  (${d.sample.map(s => `"${s}"`).join(', ')}${d.count > d.sample.length ? ', …' : ''})`)
  } else console.log(`    no input opened a new panel`)
  console.log('')
}
if (overlayInfo)
  console.log(`  ▣ overlay (frame ${overlayInfo.target}, ${overlayInfo.widgets} widgets): ${overlayInfo.out}`)

console.log(`  (coverage: ${framesSeen} frames${wantExplore ? ' incl. auto-explore' : ' of default play — drive more with --explore/--beats'})\n`)
process.exit(offList.length || colList.length || uiWarnings.size ? 1 : 0)
