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
//   node tools/arrange-score.js lofi loficity --sound         # + the SOUND half (below)
//   --json   machine-readable      --selfcheck   known answers on synthetic notes + signals (runs no cart)
//
// THE SOUND HALF (--sound). The notes can match and the two still sound nothing alike: after the arranger
// work lofi's notes scored within a few % of loficity and the owner still heard it as "miles away" — the gap
// was a saturated tape squashing the Rhodes under every hit, a mix an octave darker and a bass 4 dB low
// (2026-09-29). None of that is in a note. So --sound renders a WAV of the mix plus one STEM per part
// (play.js --solo-slot) on the first seed, for --sound-minutes (default 1.5), and reports:
//   mix rms / crest dB     the MUSIC mix (every stem but a noise bed like vinyl, which would lift every window):
//                          loudness, and peak-to-rms: a squashed (saturated / over-glued) mix reads LOW ↑
//   mix centroid Hz        the spectral centroid of the whole mix: dark vs bright (a hiss bed lifts it)
//   low-heavy %            100 ms windows whose centroid is under 500 Hz: bass-and-kick mud ↓
//   <part> vs keys dB      each stem's rms against the keys stem, both measured only where they SOUND (windows
//                          over -60 dBFS), so a lead that enters late does not read as a quiet one: the BALANCE
//   <part> centroid Hz     each stem's own brightness (compares timbre part by part, hiss excluded)
// Stem slots = the STEMS table below (every slot a part sounds on, unlike ROLES' primary slot).
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
//                          (≤ 2 semitones) split as semitone % · third % (3-4) · leap % (≥ 5)
//   rubs <a-b> /min        notes of two pitched parts SOUNDING TOGETHER a semitone or a b9 apart, per minute
//                          (by REGISTER, unlike clash %: what the ear hears as dissonance — see countRubs)
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
  lofi:     { keys: [5, 13], bass: [6], lead: [7], kick: [20], snare: [23, 12], hat: [25, 26] },   // since the morphdrum kit
  loficity: { keys: [5, 6, 14, 15, 16], bass: [7, 8], lead: [10, 11], kick: [20], snare: [23, 9, 18], hat: [25, 26, 32, 33] },
}
// the SOUND half's stems: EVERY slot a part sounds on (a layered kick is all of its slots)
const STEMS = {
  lofi:     { keys: [5, 13], bass: [6], lead: [7], kit: [12, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29] },
  loficity: { keys: [5, 6, 14, 15, 16], bass: [7, 8], lead: [10, 11], kit: [9, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33], vinyl: [12, 13] },
}
const BEDS = new Set(['vinyl'])        // noise beds: reported as a stem, left out of the music mix
const PITCHED = ['keys', 'bass', 'lead']
const DRUMS = ['kick', 'snare', 'hat']
const PARTS = [...PITCHED, ...DRUMS]

const args = process.argv.slice(2)
const opt = (k, d) => { const i = args.indexOf(k); return i >= 0 ? args[i + 1] : d }
const has = k => args.includes(k)
const OPTS_WITH_VAL = new Set(['--seeds', '--minutes', '--trace', '--cart', '--roles', '--sound-minutes'])
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

// RUBS BY REGISTER: pairs of notes SOUNDING TOGETHER (overlap > 10 ms) a semitone or a minor ninth apart, per
// pitched part pair, per minute. The clash % rows judge pitch CLASSES against the chord, so they cannot see
// WHERE notes sit: lofi's Rhodes stacked a 9th under the 3rd an octave up (a b9 inside the chord) and its lead
// sat a semitone off the keys in the same octave, and both rows stayed green while the owner heard it as
// "dissonance" (radio-arranger-lessons §6.3). A maj7 two octaves over the bass is NOT counted (that is the
// chord), which is also why these rows replace bass clash % as the dissonance read. Notes without a known
// duration (dur ≤ 0) are skipped.
const RUB_PAIRS = ['keys-keys', 'keys-lead', 'bass-keys', 'bass-lead']
function countRubs(byPart, minutes) {
  const N = []
  for (const p of PITCHED) for (const n of byPart[p]) if (n.dur > 0) N.push({ p, m: n.midi, s: n.s, e: n.s + n.dur })
  N.sort((a, b) => a.s - b.s)
  const c = {}; for (const k of RUB_PAIRS) c[k] = 0
  const ovl = SR * 0.01
  for (let i = 0; i < N.length; i++) for (let j = i - 1; j >= 0 && j > i - 80; j--) {
    const a = N[i], b = N[j]
    if (b.e <= a.s + ovl) continue
    const d = Math.abs(a.m - b.m); if (d !== 1 && d !== 13) continue
    const k = [a.p, b.p].sort().join('-'); if (k in c) c[k]++
  }
  const r = {}; let tot = 0
  for (const k of RUB_PAIRS) { r[`rubs ${k} /min`] = minutes > 0 ? c[k] / minutes : NaN; tot += c[k] }
  r['rubs total /min'] = minutes > 0 ? tot / minutes : NaN
  return r
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
  // the step % split: on a pentatonic ladder a third IS a step, so the shape needs the histogram
  m['lead semitone %'] = ints.length ? 100 * ints.filter(x => x === 1).length / ints.length : NaN
  m['lead third %'] = ints.length ? 100 * ints.filter(x => x === 3 || x === 4).length / ints.length : NaN
  m['lead leap %'] = ints.length ? 100 * ints.filter(x => x >= 5).length / ints.length : NaN
  Object.assign(m, countRubs(byPart, m.min))
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

// ── the SOUND half: a WAV of the mix + a stem per part ──
function readWav(f) {                        // 16-bit PCM, stereo downmixed (as tools/wav-envelope.js reads it)
  const b = fs.readFileSync(f)
  let off = 12, data = null, sr = SR, ch = 1
  while (off + 8 <= b.length) {
    const id = b.toString('ascii', off, off + 4), len = b.readUInt32LE(off + 4)
    if (id === 'fmt ') { sr = b.readUInt32LE(off + 12); ch = b.readUInt16LE(off + 10) }
    if (id === 'data') data = { off: off + 8, len }
    off += 8 + len + (len & 1)
  }
  if (!data) throw new Error(`${f}: no data chunk`)
  const n = (data.len / 2 / ch) | 0, x = new Float32Array(n)
  for (let i = 0; i < n; i++) x[i] = ch === 1 ? b.readInt16LE(data.off + i * 2) / 32768
    : (b.readInt16LE(data.off + i * 4) + b.readInt16LE(data.off + i * 4 + 2)) / 65536
  return { sr, x }
}
function fft(re, im) {                       // in-place radix-2 (the same one wav-envelope.js uses)
  const n = re.length
  for (let i = 1, j = 0; i < n; i++) { let bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit
    if (i < j) { let t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t } }
  for (let len = 2; len <= n; len <<= 1) {
    const ang = -2 * Math.PI / len, wr = Math.cos(ang), wi = Math.sin(ang)
    for (let i = 0; i < n; i += len) { let cr = 1, ci = 0
      for (let k = 0; k < len / 2; k++) { const a = i + k, b = a + len / 2
        const vr = re[b] * cr - im[b] * ci, vi = re[b] * ci + im[b] * cr
        re[b] = re[a] - vr; im[b] = im[a] - vi; re[a] += vr; im[a] += vi
        const nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr } }
  }
}
// rms + crest over the whole signal, the magnitude-weighted centroid over 4096-sample Hann windows (silent
// windows skipped, so a quiet intro does not drag it), and the share of 100 ms windows centred under 500 Hz
function analyse({ sr, x }) {
  const db = v => 20 * Math.log10(Math.max(v, 1e-9))
  let sq = 0, pk = 0; for (const v of x) { sq += v * v; if (Math.abs(v) > pk) pk = Math.abs(v) }
  const rms = Math.sqrt(sq / Math.max(1, x.length))
  const N = 4096, re = new Float64Array(N), im = new Float64Array(N)
  let num = 0, den = 0, low = 0, win = 0, aSq = 0, aN = 0
  for (let i0 = 0; i0 + N <= x.length; i0 += N) {
    let e = 0; for (let k = 0; k < N; k++) e += x[i0 + k] * x[i0 + k]
    if (Math.sqrt(e / N) < 1e-4) continue
    if (Math.sqrt(e / N) >= 1e-3) { aSq += e; aN += N }   // -60 dBFS: the part is SOUNDING here
    for (let k = 0; k < N; k++) { re[k] = x[i0 + k] * (0.5 - 0.5 * Math.cos(2 * Math.PI * k / (N - 1))); im[k] = 0 }
    fft(re, im)
    let wn = 0, wd = 0
    for (let k = 1; k < N / 2; k++) { const m = Math.hypot(re[k], im[k]); wn += k * sr / N * m; wd += m }
    num += wn; den += wd; win++; if (wd > 0 && wn / wd < 500) low++
  }
  return { rms: db(rms), active: aN ? db(Math.sqrt(aSq / aN)) : NaN, crest: db(pk) - db(rms), centroid: den > 0 ? num / den : NaN, low: win ? 100 * low / win : NaN }
}
function renderWav(cart, seed, minutes, slots, tag) {
  const dir = path.join(ROOT, 'build', 'arrange-score'); fs.mkdirSync(dir, { recursive: true })
  const wav = path.join(dir, `${cart}-${seed}-${tag}.wav`)
  fs.rmSync(path.join(ROOT, 'build', 'saves', cart), { recursive: true, force: true })
  const a = [path.join(ROOT, 'tools/play.js'), cart, 'script', '/dev/null', '--headless',
    '--frames', String(Math.round(minutes * 3600)), '--seed', String(seed), '--wav', wav]
  if (slots) a.push('--solo-slot', slots.join(','))
  execFileSync('node', a, { cwd: ROOT, stdio: ['ignore', 'pipe', 'pipe'], maxBuffer: 1 << 28 })
  return wav
}
function scoreSound(cart, seed, minutes) {
  const stems = STEMS[cart]; if (!stems) throw new Error(`no STEMS entry for "${cart}" in tools/arrange-score.js`)
  // the MUSIC mix: every stem but a noise BED (vinyl), which sits in every window and lifts the centroid of
  // any mix it is in (loficity's hiss read 0.1% low-heavy against lofi's 17.6% before this, on hiss alone)
  const music = Object.entries(stems).filter(([p]) => !BEDS.has(p)).flatMap(([, sl]) => sl)
  const m = analyse(readWav(renderWav(cart, seed, minutes, music, 'mix')))
  const out = { 'mix rms dB': m.rms, 'mix crest dB': m.crest, 'mix centroid Hz': m.centroid, 'low-heavy %': m.low }
  const per = {}
  for (const [part, slots] of Object.entries(stems)) {
    process.stderr.write(`  stem ${cart} ${part}…\n`)
    per[part] = analyse(readWav(renderWav(cart, seed, minutes, slots, part)))
  }
  for (const [part, a] of Object.entries(per)) {
    if (part !== 'keys') out[`${part} vs keys dB`] = per.keys ? a.active - per.keys.active : NaN   // while SOUNDING: a part that enters late is not quiet
    out[`${part} centroid Hz`] = a.centroid
  }
  return out
}

// ── output ──
const ORDER = ['songs', 'bpm', 'grid fit', 'bars', 'min',
  'lead clash %', 'lead strong-CT %', 'bass clash %',
  'block contrast', 'layers/block', 'layers min-max', 'chord changes/bar', 'keys voices', 'keys register',
  ...PARTS.flatMap(p => [`${p} notes/bar`, `${p} repeat %`, `${p} distinct %`]),
  'lead phrase len', 'lead range', 'lead step %', 'lead semitone %', 'lead third %', 'lead leap %',
  ...RUB_PAIRS.map(k => `rubs ${k} /min`), 'rubs total /min',
  'swing ms', ...PARTS.flatMap(p => [`${p} pocket ms`, `${p} spread ms`]),
  ...PARTS.map(p => `${p} vol sd`)]
function fmt(v) { return typeof v === 'string' ? v : Number.isNaN(v) || v === undefined ? '–' : Math.abs(v) >= 100 ? v.toFixed(0) : Math.abs(v) >= 10 ? v.toFixed(1) : v.toFixed(2) }
function table(cols, order = ORDER) {
  const names = Object.keys(cols), w = 20
  console.log(''.padEnd(w) + names.map(n => n.padStart(12)).join(''))
  for (const k of order) console.log(k.padEnd(w) + names.map(n => fmt(cols[n][k]).padStart(12)).join(''))
}

// ── selfcheck: synthetic notes with known answers (no cart, no engine) ──
function selfcheck() {
  let pass = 0, fail = 0
  const ok = (c, msg) => { if (c) pass++; else { fail++; console.log('  ✗ ' + msg) } }
  const bpm = 80, step = SR * 15 / bpm
  const roleMap = roleOf({ keys: [5], bass: [6], lead: [7], kick: [8], snare: [9], hat: [10] })   // the fixture's own slots
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
  // R: rubs by register. A b9 INSIDE the keys (C4 under Db5) is a rub; the same pitch classes 2 octaves
  // apart (C3 under Db5, 25) are not; a lead a semitone off a held keys note rubs, one that starts after the
  // keys note ENDED does not; a maj7 two octaves over the bass is the chord, not a rub.
  const R = mk(16, (b, s, t, ns) => {
    kit(b, s, t, ns)
    if (s === 0) { ns.push({ slot: 5, midi: 60, vol: 4, dur: Math.round(8 * step), s: t }); ns.push({ slot: 5, midi: 73, vol: 4, dur: Math.round(8 * step), s: t }) }   // b9 inside the keys
    if (s === 0) ns.push({ slot: 5, midi: 48, vol: 4, dur: Math.round(8 * step), s: t })            // C3 vs Db5 = 25: not a rub
    if (s === 0) ns.push({ slot: 6, midi: 37, vol: 5, dur: Math.round(8 * step), s: t })            // Db2 vs C4 = 23 (a maj7): not a rub
    if (s === 4) ns.push({ slot: 7, midi: 74, vol: 3, dur: Math.round(2 * step), s: t })            // D5 vs Db5 sounding: a rub
    if (s === 10) ns.push({ slot: 7, midi: 61, vol: 3, dur: Math.round(2 * step), s: t })           // after the keys ended: none
  })
  const rr = scoreSong(R.sort((x, y) => x.s - y.s), roleMap), perMin = 16 / rr.min
  ok(Math.abs(rr['rubs keys-keys /min'] - perMin) < 1e-6, `one b9 inside the keys a bar: ${rr['rubs keys-keys /min'].toFixed(2)} vs ${perMin.toFixed(2)}`)
  ok(Math.abs(rr['rubs keys-lead /min'] - perMin) < 1e-6, `one keys-lead semitone a bar, none after the chord ends: ${rr['rubs keys-lead /min'].toFixed(2)}`)
  ok(rr['rubs bass-keys /min'] === 0 && rr['rubs bass-lead /min'] === 0, `a maj7 / 2 octaves apart is not a rub: ${rr['rubs bass-keys /min']}`)
  ok(a['rubs total /min'] > 0 && Math.abs(a['rubs total /min'] - 16 / a.min) < 1e-6, `fixture A's lead C5 over the keys' B3 IS a b9 (one a bar): ${a['rubs total /min'].toFixed(2)}`)
  // C: silence (no notes) must not score as a perfect song
  ok(scoreSong([], roleMap) === null, 'no notes → no score (not a perfect one)')
  // D: the SOUND analyser on signals with known answers (a broken analyser and a dull mix print alike)
  const sine = (f, a, n = SR * 2) => { const x = new Float32Array(n); for (let i = 0; i < n; i++) x[i] = a * Math.sin(2 * Math.PI * f * i / SR); return { sr: SR, x } }
  const s1 = analyse(sine(1000, 0.5))
  ok(Math.abs(s1.rms - (-9.03)) < 0.05 && Math.abs(s1.crest - 3.01) < 0.05, `sine 0.5: rms -9.03 crest 3.01 dB (${s1.rms.toFixed(2)} / ${s1.crest.toFixed(2)})`)
  ok(Math.abs(s1.centroid - 1000) < 60 && s1.low === 0, `a 1 kHz sine centres near 1 kHz, not low-heavy (${s1.centroid.toFixed(0)} Hz, ${s1.low}%)`)
  const s2 = analyse(sine(120, 0.5))
  ok(s2.centroid < 500 && s2.low === 100, `a 120 Hz sine is low-heavy (${s2.centroid.toFixed(0)} Hz, ${s2.low}%)`)
  const sq = sine(1000, 0.5); for (let i = 0; i < sq.x.length; i++) sq.x[i] = Math.tanh(8 * sq.x[i]) * 0.5
  ok(analyse(sq).crest < s1.crest - 1, `a saturated sine reads a LOWER crest (the squash tell): ${analyse(sq).crest.toFixed(2)}`)
  ok(Number.isNaN(analyse({ sr: SR, x: new Float32Array(SR) }).centroid), 'silence has no centroid (not 0 Hz)')
  const half = sine(1000, 0.5, SR * 4); for (let i = 0; i < SR * 2; i++) half.x[i] = 0
  const h = analyse(half)
  ok(Math.abs(h.active - s1.rms) < 0.2 && h.rms < s1.rms - 2.5, `a part silent half the time: same ACTIVE level, lower rms (${h.active.toFixed(2)} / ${h.rms.toFixed(2)})`)
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
const snd = {}
if (has('--sound')) {
  const seed = Number((opt('--seeds', '1,2,3')).split(',')[0]), minutes = parseFloat(opt('--sound-minutes', '1.5'))
  for (const cart of positional) { process.stderr.write(`sound: ${cart} seed ${seed}…\n`); snd[cart] = scoreSound(cart, seed, minutes); json[cart + ' sound'] = snd[cart] }
}
if (has('--json')) console.log(JSON.stringify(json, null, 1))
else {
  table(cols)
  if (has('--sound')) {
    const keys = [...new Set(Object.values(snd).flatMap(o => Object.keys(o)))]
    console.log('\nSOUND (seed ' + opt('--seeds', '1,2,3').split(',')[0] + ', ' + opt('--sound-minutes', '1.5') + ' min, mix + per-part stems)')
    table(snd, keys)
  }
}
