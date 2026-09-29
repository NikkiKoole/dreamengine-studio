#!/usr/bin/env node
'use strict'
// arrange-score.js — the ARRANGEMENT SCORECARD: how good is what a music cart actually PLAYED?
// Every other audio gate checks the SOUND (in tune, level, clicks). None checks the MUSIC: a lead in the
// wrong scale, every bar identical, a B section that is the A section again. Those only showed up by ear —
// which is how lofi.c shipped a minor-pentatonic lead over major-key chords (2026-09-29). This reads the
// notes a cart played and turns "the arrangement is crap" into numbers you can close, side by side against
// a REFERENCE (loficity for lofi; loficity's other styles for house/bossa/…). Strategy + reading guide:
// docs/design/radio-arranger-lessons.md §5.
//
// Source of truth = the engine's DE_TRACE voice events ({"vev":"on",…,"smp","vol","dur"}: every note-on on
// its exact sample, runtime/sound.h), so it needs NO cart changes and works on any station. Only a ROLE MAP
// (which instrument slot is keys / bass / lead / kick / snare / hat) is per cart: the ROLES table below, or
// --roles. Tempo + bar lines are ESTIMATED from the onsets per song (song boundaries = the cart's `song`
// watch), so a pocket that drags does not move the grid.
//
// Usage:
//   node tools/arrange-score.js lofi loficity                 # render each (3 seeds × 6 min) + compare
//   node tools/arrange-score.js lofi --seeds 1,2,3 --minutes 8
//   node tools/arrange-score.js --trace a.jsonl --cart lofi   # score an existing trace (roles by cart)
//   node tools/arrange-score.js lofi --roles "keys=5 bass=6 lead=7 kick=8 snare=9 hat=10"
//   --json   machine-readable      --selfcheck   known answers on synthetic notes (runs no cart)
//
// METRICS (per song, then averaged; ↑/↓ = which way is usually better, but the REFERENCE decides):
//   bpm / bars / min       the estimated tempo, song length
//   lead clash %           lead onsets a SEMITONE ABOVE a sounding chord tone (keys ∪ bass) and not a chord
//                          tone themselves — the b9 rub. ↓. The wrong-scale detector.
//   lead strong-CT %       lead onsets on beats 1/3 that ARE chord tones ↑
//   bass clash %           same test for the bass (chromatic approaches land here legitimately — compare)
//   <part> repeat %        bars identical to the bar before (pitched: step+pitch-class; drums: step) ↓
//   <part> distinct %      unique bar patterns / bars ↑
//   <part> notes/bar       density
//   block contrast         coefficient of variation of notes per 4-bar block — does the song have SHAPE ↑
//   layers/block           mean parts active per 4-bar block (and its min→max range)
//   chord changes/bar      how often the sounding keys pitch-class set changes
//   keys voices / reg      notes per keys hit, their mean MIDI register
//   lead phrase len        notes per phrase (a gap ≥ 1 beat ends one) · range (semitones) · step %
//   <part> pocket ms       mean offset from the 16th grid (+ = late) · <part> spread = its std
//   swing ms               odd-16th onsets' extra delay over even ones (all parts)
//   <part> vol sd          dynamics: std of note volume (the engine's 0–7 scale)

const fs = require('fs')
const path = require('path')
const { execFileSync } = require('child_process')
const ROOT = path.resolve(__dirname, '..')
const SR = 44100

// the per-cart ROLE MAP (instrument slots). A layered voice lists ONLY its primary slot (loficity's kick is
// three slots; counting all three would triple its density).
const ROLES = {
  lofi:     { keys: [5], bass: [6], lead: [7], kick: [8], snare: [9], hat: [10] },
  loficity: { keys: [5, 6, 14, 15, 16], bass: [7, 8], lead: [10, 11], kick: [20], snare: [23, 9, 18], hat: [25, 26, 32, 33] },
}
const PITCHED = ['keys', 'bass', 'lead']
const DRUMS = ['kick', 'snare', 'hat']
const PARTS = [...PITCHED, ...DRUMS]

const args = process.argv.slice(2)
const opt = (k, d) => { const i = args.indexOf(k); return i >= 0 ? args[i + 1] : d }
const has = k => args.includes(k)
const OPTS_WITH_VAL = new Set(['--seeds', '--minutes', '--trace', '--cart', '--roles'])
const positional = args.filter((a, i) => !a.startsWith('--') && !OPTS_WITH_VAL.has(args[i - 1]))

// ── reading a trace ──
function readTrace(file) {
  const notes = [], songs = []   // songs: [{ f, id }]
  let lastSong = null, unstamped = 0
  for (const line of fs.readFileSync(file, 'utf8').split('\n')) {
    if (!line) continue
    let r; try { r = JSON.parse(line) } catch { continue }
    if (r.vev !== undefined) {
      if (r.vev === 'on') { if (r.smp === undefined) unstamped++; else notes.push({ slot: r.slot, midi: r.midi, vol: r.vol, dur: r.dur, s: r.smp, f: r.f }) }
      continue
    }
    const id = r.w && r.w.song
    if (id !== undefined && id !== lastSong) { songs.push({ f: r.f, id }); lastSong = id }
  }
  if (unstamped && !notes.length) throw new Error(`${file}: voice events carry no sample stamp — an old engine build (runtime/sound.h sve_push_v)`)
  notes.sort((a, b) => a.s - b.s)
  return { notes, songs }
}
function parseRoles(str) {
  const r = {}
  for (const kv of str.trim().split(/\s+/)) { const [k, v] = kv.split('='); r[k] = v.split(',').map(Number) }
  return r
}
function roleOf(roles) {
  const m = new Map()
  for (const p of PARTS) for (const s of roles[p] || []) m.set(s, p)
  return m
}

// ── tempo + grid: the 8th-note grid whose onsets line up best (circular resultant), then the bar phase
// that puts kicks on 1 and snares on 2/4. Range 60–119 bpm so the half/double ambiguity cannot win.
function estimateGrid(onsets, kicks, snares) {
  if (onsets.length < 8) return null
  let best = null
  const score = (bpm) => {
    const p8 = SR * 30 / bpm; let c = 0, s = 0
    for (const t of onsets) { const a = 2 * Math.PI * (t / p8); c += Math.cos(a); s += Math.sin(a) }
    return { R: Math.hypot(c, s) / onsets.length, ph: Math.atan2(s, c) }
  }
  for (let bpm = 60; bpm < 120; bpm += 0.25) { const r = score(bpm); if (!best || r.R > best.R) best = { bpm, ...r } }
  for (let bpm = best.bpm - 0.25; bpm <= best.bpm + 0.25; bpm += 0.01) { const r = score(bpm); if (r.R > best.R) best = { bpm, ...r } }
  const step = SR * 15 / best.bpm
  // the grid's PHASE comes from the KICK (the clock the pocket is measured against): a dragged snare or a
  // swung hat would otherwise pull the whole grid toward itself and hide its own lateness
  let ph = best.ph
  if (kicks.length >= 4) {
    let c = 0, s = 0
    for (const t of kicks) { const a = 2 * Math.PI * (t / (2 * step)); c += Math.cos(a); s += Math.sin(a) }
    ph = Math.atan2(s, c)
  }
  let origin = (ph / (2 * Math.PI)) * (2 * step)                      // an 8th-grid line
  if (origin < 0) origin += 2 * step
  // the bar phase: which of the 16 step offsets makes kicks land on 1 and snares on 2 and 4
  let bo = 0, bs = -1
  for (let o = 0; o < 16; o++) {
    let sc = 0
    const at = t => ((Math.round((t - origin) / step) - o) % 16 + 16) % 16
    for (const t of kicks) { const k = at(t); if (k === 0) sc += 2; else if (k === 8) sc += 1 }
    for (const t of snares) { const k = at(t); if (k === 4 || k === 12) sc += 2 }
    if (sc > bs) { bs = sc; bo = o }
  }
  return { bpm: best.bpm, R: best.R, step, origin: origin + bo * step }
}

// ── scoring one song ──
const pcOf = m => ((m % 12) + 12) % 12
const mean = a => a.length ? a.reduce((x, y) => x + y, 0) / a.length : NaN
const sd = a => { if (a.length < 2) return NaN; const m = mean(a); return Math.sqrt(mean(a.map(x => (x - m) ** 2))) }

function soundingPcs(byPart, t, win) {
  // chord tones at time t: keys + bass notes still inside their gate; else the latest keys cluster within a bar
  const pcs = new Set()
  for (const p of ['keys', 'bass']) {
    const arr = byPart[p]
    let latest = null
    for (let i = arr.length - 1; i >= 0; i--) {
      const n = arr[i]
      if (n.s > t) continue
      if (t - n.s > win) break
      if (n.dur > 0 && n.s + n.dur > t) pcs.add(pcOf(n.midi))
      if (latest === null) latest = n.s
      if (p === 'keys' && latest - n.s < SR * 0.08) pcs.add(pcOf(n.midi))   // the latest keys hit, whole cluster
      if (p === 'bass' && n.s === latest) pcs.add(pcOf(n.midi))
    }
  }
  return pcs
}

function scoreSong(notes, roleMap) {
  const byPart = {}; for (const p of PARTS) byPart[p] = []
  for (const n of notes) { const p = roleMap.get(n.slot); if (p) byPart[p].push(n) }
  const all = PARTS.flatMap(p => byPart[p].map(n => n.s))
  const grid = estimateGrid(all, byPart.kick.map(n => n.s), byPart.snare.map(n => n.s))
  if (!grid) return null
  const { step, origin } = grid
  const stepIdx = t => Math.round((t - origin) / step)
  const first = Math.min(...all), last = Math.max(...all)
  const bar0 = Math.floor(stepIdx(first) / 16), barN = Math.floor(stepIdx(last) / 16)
  const nBars = barN - bar0 + 1
  const m = { bpm: grid.bpm, 'grid fit': grid.R, bars: nBars, min: (last - first) / SR / 60 }

  // per-part bar patterns: repetition / distinctness / density / pocket / dynamics
  const offOdd = [], offEven = []
  for (const p of PARTS) {
    const bars = new Map()
    const offs = []
    for (const n of byPart[p]) {
      const k = stepIdx(n.s), b = Math.floor(k / 16), st = ((k % 16) + 16) % 16
      const key = PITCHED.includes(p) ? `${st}:${pcOf(n.midi)}` : `${st}`
      if (!bars.has(b)) bars.set(b, new Set())
      bars.get(b).add(key)
      const off = (n.s - (origin + k * step)) / SR * 1000
      offs.push(off); (st % 2 ? offOdd : offEven).push(off)
    }
    const played = [...bars.keys()].sort((a, b) => a - b)
    const sig = b => [...(bars.get(b) || [])].sort().join(' ')
    let rep = 0, cmp = 0
    for (const b of played) if (bars.has(b - 1)) { cmp++; if (sig(b) === sig(b - 1)) rep++ }
    m[`${p} notes/bar`] = byPart[p].length / nBars
    m[`${p} repeat %`] = cmp ? 100 * rep / cmp : NaN
    m[`${p} distinct %`] = played.length ? 100 * new Set(played.map(sig)).size / played.length : NaN
    m[`${p} pocket ms`] = mean(offs)
    m[`${p} spread ms`] = sd(offs)
    m[`${p} vol sd`] = sd(byPart[p].map(n => n.vol).filter(v => v >= 0))
  }
  m['swing ms'] = mean(offOdd) - mean(offEven)

  // shape: notes + active parts per 4-bar block
  const blocks = new Map()
  for (const p of PARTS) for (const n of byPart[p]) {
    const bl = Math.floor(stepIdx(n.s) / 64)
    if (!blocks.has(bl)) blocks.set(bl, { n: 0, parts: new Set() })
    const B = blocks.get(bl); B.n++; B.parts.add(p)
  }
  const bl = [...blocks.values()]
  m['block contrast'] = sd(bl.map(b => b.n)) / mean(bl.map(b => b.n))
  m['layers/block'] = mean(bl.map(b => b.parts.size))
  m['layers min-max'] = `${Math.min(...bl.map(b => b.parts.size))}-${Math.max(...bl.map(b => b.parts.size))}`

  // harmony: keys hits clustered (80 ms) → voicings, chord changes
  const hits = []
  for (const n of byPart.keys) {
    const h = hits[hits.length - 1]
    if (h && n.s - h.s < SR * 0.08) h.notes.push(n.midi); else hits.push({ s: n.s, notes: [n.midi] })
  }
  let changes = 0, prev = ''
  for (const h of hits) { const k = [...new Set(h.notes.map(pcOf))].sort((a, b) => a - b).join(','); if (prev && k !== prev) changes++; prev = k }
  m['chord changes/bar'] = changes / nBars
  m['keys voices'] = mean(hits.map(h => h.notes.length))
  m['keys register'] = mean(byPart.keys.map(n => n.midi))

  // clashes: lead + bass onsets vs the sounding chord
  const win = 16 * step
  for (const p of ['lead', 'bass']) {
    let clash = 0, tot = 0, strong = 0, strongCT = 0
    for (const n of byPart[p]) {
      const pcs = soundingPcs(p === 'bass' ? { keys: byPart.keys, bass: [] } : byPart, n.s - 1, win)
      if (!pcs.size) continue
      const pc = pcOf(n.midi), ct = pcs.has(pc)
      tot++
      if (!ct && pcs.has(pcOf(pc - 1))) clash++
      const st = ((stepIdx(n.s) % 16) + 16) % 16
      if (st === 0 || st === 8) { strong++; if (ct) strongCT++ }
    }
    m[`${p} clash %`] = tot ? 100 * clash / tot : NaN
    if (p === 'lead') m['lead strong-CT %'] = strong ? 100 * strongCT / strong : NaN
  }

  // lead phrasing: a gap of a beat or more ends a phrase
  const L = byPart.lead, phrases = []
  for (let i = 0; i < L.length; i++) {
    if (!i || L[i].s - L[i - 1].s >= 4 * step) phrases.push([])
    phrases[phrases.length - 1].push(L[i].midi)
  }
  const ints = []; for (const ph of phrases) for (let i = 1; i < ph.length; i++) ints.push(Math.abs(ph[i] - ph[i - 1]))
  m['lead phrase len'] = mean(phrases.map(p => p.length))
  m['lead range'] = mean(phrases.map(p => Math.max(...p) - Math.min(...p)))
  m['lead step %'] = ints.length ? 100 * ints.filter(x => x <= 2).length / ints.length : NaN
  return m
}

// ── a whole trace: split by song, score each, average ──
function scoreTrace(file, roles) {
  const { notes, songs } = readTrace(file)
  const roleMap = roleOf(roles)
  // song boundaries (frames) → samples: the first note drained on/after that frame
  const bounds = songs.map(s => { const n = notes.find(x => x.f >= s.f); return n ? n.s : Infinity })
  const per = []
  for (let i = 0; i < songs.length; i++) {
    const a = bounds[i], b = i + 1 < bounds.length ? bounds[i + 1] : Infinity
    const seg = notes.filter(n => n.s >= a && n.s < b)
    const lastSong = i === songs.length - 1
    const r = scoreSong(seg, roleMap)
    if (r && !(lastSong && songs.length > 1 && r.bars < 24)) per.push(r)     // drop a truncated final song
  }
  return per
}
function average(list) {
  const out = {}
  const keys = [...new Set(list.flatMap(o => Object.keys(o)))]
  for (const k of keys) {
    const v = list.map(o => o[k]).filter(x => typeof x === 'number' && !Number.isNaN(x))
    out[k] = v.length ? mean(v) : list.find(o => typeof o[k] === 'string')?.[k] ?? NaN
  }
  out.songs = list.length
  return out
}

function render(cart, seed, minutes) {
  const dir = path.join(ROOT, 'build', 'arrange-score'); fs.mkdirSync(dir, { recursive: true })
  const trace = path.join(dir, `${cart}-${seed}.jsonl`)
  fs.rmSync(path.join(ROOT, 'build', 'saves', cart), { recursive: true, force: true })   // autosaving carts boot from their last run
  execFileSync('node', [path.join(ROOT, 'tools/play.js'), cart, 'script', '/dev/null', '--headless',
    '--frames', String(Math.round(minutes * 3600)), '--seed', String(seed), '--trace', trace],
  { cwd: ROOT, stdio: ['ignore', 'pipe', 'pipe'], maxBuffer: 1 << 28 })
  return trace
}

// ── output ──
const ORDER = ['songs', 'bpm', 'grid fit', 'bars', 'min',
  'lead clash %', 'lead strong-CT %', 'bass clash %',
  'block contrast', 'layers/block', 'layers min-max', 'chord changes/bar', 'keys voices', 'keys register',
  ...PARTS.flatMap(p => [`${p} notes/bar`, `${p} repeat %`, `${p} distinct %`]),
  'lead phrase len', 'lead range', 'lead step %',
  'swing ms', ...PARTS.flatMap(p => [`${p} pocket ms`, `${p} spread ms`]),
  ...PARTS.map(p => `${p} vol sd`)]
function fmt(v) { return typeof v === 'string' ? v : Number.isNaN(v) || v === undefined ? '–' : Math.abs(v) >= 100 ? v.toFixed(0) : Math.abs(v) >= 10 ? v.toFixed(1) : v.toFixed(2) }
function table(cols) {
  const names = Object.keys(cols), w = 20
  console.log(''.padEnd(w) + names.map(n => n.padStart(12)).join(''))
  for (const k of ORDER) console.log(k.padEnd(w) + names.map(n => fmt(cols[n][k]).padStart(12)).join(''))
}

// ── selfcheck: synthetic notes with known answers (no cart, no engine) ──
function selfcheck() {
  let pass = 0, fail = 0
  const ok = (c, msg) => { if (c) pass++; else { fail++; console.log('  ✗ ' + msg) } }
  const bpm = 80, step = SR * 15 / bpm
  const roleMap = roleOf(ROLES.lofi)
  const mk = (bars, fn) => { const ns = []; for (let b = 0; b < bars; b++) for (let s = 0; s < 16; s++) fn(b, s, (b * 16 + s) * step + 1000, ns); return ns }
  const kit = (b, s, t, ns) => {
    if (s === 0 || s === 10) ns.push({ slot: 8, midi: 34, vol: 5, dur: 5000, s: t })
    if (s === 4 || s === 12) ns.push({ slot: 9, midi: 60, vol: 5, dur: 5000, s: t })
    if (s % 2 === 0) ns.push({ slot: 10, midi: 90, vol: 1, dur: 1000, s: t })
  }
  // A: Cmaj7 held every bar; the lead plays C major pentatonic (no clash); every bar identical
  const A = mk(16, (b, s, t, ns) => {
    kit(b, s, t, ns)
    if (s === 0) for (const m of [52, 55, 59, 62]) ns.push({ slot: 5, midi: m, vol: 4, dur: Math.round(16 * step), s: t })
    if (s === 0) ns.push({ slot: 6, midi: 36, vol: 5, dur: Math.round(8 * step), s: t })
    if (s === 0 || s === 4 || s === 8) ns.push({ slot: 7, midi: [72, 74, 76][s / 4], vol: 3, dur: 3000, s: t })
  })
  const a = scoreSong(A.sort((x, y) => x.s - y.s), roleMap)
  ok(Math.abs(a.bpm - bpm) < 0.2, `tempo recovered: ${a.bpm} vs ${bpm}`)
  ok(a.bars === 16, `bars: ${a.bars}`)
  ok(a['lead clash %'] === 0, `consonant lead clash 0: ${a['lead clash %']}`)
  ok(a['lead strong-CT %'] === 100, `C and E on 1/3 are chord tones: ${a['lead strong-CT %']}`)
  ok(a['keys repeat %'] === 100 && a['kick repeat %'] === 100, `a looped bar repeats 100%: ${a['keys repeat %']}`)
  ok(Math.abs(a['keys voices'] - 4) < 1e-9, `4-note voicing: ${a['keys voices']}`)
  ok(Math.abs(a['kick pocket ms']) < 0.5 && !(Math.abs(a['swing ms']) >= 0.5), `on-grid pocket ~0: ${a['kick pocket ms']} swing ${a['swing ms']}`)
  // B: the SAME chords, the lead in C MINOR pentatonic (Eb over E, Bb over B) = the lofi bug; snare 20 ms late
  const B = mk(16, (b, s, t, ns) => {
    kit(b, s, t + (s === 4 || s === 12 ? SR * 0.02 : 0), ns)
    if (s === 0) for (const m of [52, 55, 59, 62]) ns.push({ slot: 5, midi: m, vol: 4, dur: Math.round(16 * step), s: t })
    if (s === 0) ns.push({ slot: 6, midi: 36, vol: 5, dur: Math.round(8 * step), s: t })
    if (s === 2 || s === 6) ns.push({ slot: 7, midi: s === 2 ? 75 : 72, vol: 3, dur: 3000, s: t })   // Eb (clash on D? no: over E → b9 of D) …
    if (s === 10) ns.push({ slot: 7, midi: 72 + (b % 4), vol: 3, dur: 3000, s: t })                   // … and a varying note
  })
  const bb = scoreSong(B.sort((x, y) => x.s - y.s), roleMap)
  ok(bb['lead clash %'] > 20, `minor-pent lead over maj7 clashes: ${bb['lead clash %']}`)
  ok(Math.abs(bb['snare pocket ms'] - 20) < 1.5, `a 20 ms late snare reads ~20: ${bb['snare pocket ms']}`)
  ok(bb['lead distinct %'] > 20 && bb['lead repeat %'] < 100, `a varying lead is not 100% repeat: ${bb['lead repeat %']}`)
  // C: silence (no notes) must not score as a perfect song
  ok(scoreSong([], roleMap) === null, 'no notes → no score (not a perfect one)')
  console.log(`arrange-score selfcheck: ${pass} passed, ${fail} failed`)
  process.exit(fail ? 1 : 0)
}

// ── main ──
if (has('--selfcheck')) selfcheck()
const cols = {}, json = {}
if (opt('--trace')) {
  const cart = opt('--cart')
  const roles = opt('--roles') ? parseRoles(opt('--roles')) : ROLES[cart]
  if (!roles) { console.error(`no role map for "${cart}" — pass --roles "keys=5 bass=6 …"`); process.exit(2) }
  const per = scoreTrace(opt('--trace'), roles)
  cols[cart || 'trace'] = average(per); json[cart || 'trace'] = per
} else {
  if (!positional.length) { console.error('usage: node tools/arrange-score.js <cart> [ref-cart…] [--seeds 1,2,3] [--minutes 6] | --trace f --cart c | --selfcheck'); process.exit(2) }
  const seeds = (opt('--seeds', '1,2,3')).split(',').map(Number)
  const minutes = parseFloat(opt('--minutes', '6'))
  for (const cart of positional) {
    const roles = opt('--roles') && positional.length === 1 ? parseRoles(opt('--roles')) : ROLES[cart]
    if (!roles) { console.error(`no role map for "${cart}" — add it to ROLES in tools/arrange-score.js or pass --roles`); process.exit(2) }
    const per = []
    for (const s of seeds) { process.stderr.write(`rendering ${cart} seed ${s}…\n`); per.push(...scoreTrace(render(cart, s, minutes), roles)) }
    if (!per.length) { console.error(`${cart}: no scorable song (silent, or the role map is wrong)`); process.exit(1) }
    cols[cart] = average(per); json[cart] = per
  }
}
if (has('--json')) console.log(JSON.stringify(json, null, 1))
else table(cols)
