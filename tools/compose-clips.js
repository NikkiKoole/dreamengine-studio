#!/usr/bin/env node
// compose-clips.js — stitch baked cart clips into ONE reel with transitions.
//
// This is "Layer B" from docs/design/transitions.md: take already-baked .webm clips
// (made by make-gif.js, which now carry audio) and glue them with ffmpeg crossfades
// into a single showreel video — video via `xfade`, audio via `acrossfade`, so picture
// AND sound dissolve together at each cut.
//
//   node tools/compose-clips.js <reel>          # tools/reels/<reel>.reel → editor/public/reels/<reel>.webm
//   node tools/compose-clips.js --from x.reel --out y.webm
//
// A .reel manifest is a committed, reproducible recipe (the Layer-B sibling of a clip
// recipe). One clip per line; `# key value` comment lines carry defaults:
//
//   # the v0 teaser
//   # fps 30
//   # crf 28
//   # size 320x200            (optional; default = first clip's dimensions)
//   # xfade fade 0.5          (default transition type + seconds between cuts)
//   sloop/01-autodrive                      ← editor/public/clips/sloop/01-autodrive.webm
//   moog/01-fat        | wipeleft 0.6       ← transition INTO this clip overrides the default
//   easel/01-selfplay  | circleopen 0.5 | trim 0 7 | speed 1.25
//   path/to/any.webm                        ← a bare path also works
//
// A clip ref is `<cart>/<label>` (→ editor/public/clips/<cart>/<label>.webm) or a path.
// After the ref, any number of `| <verb> …` segments (order-independent) edit that clip
// NON-DESTRUCTIVELY (applied at compose time on a copy; the source .webm is never touched):
//   | <type> <secs>   the transition that brings THIS clip in (the cut before it); the
//                     first clip's is ignored. Types are ffmpeg xfade names:
//                     fade · dissolve · wipeleft/right/up/down · slideleft/… · circleopen ·
//                     circleclose · pixelize · radial · smoothleft/…  (our iris≈circleopen,
//                     wipe≈wipeleft, dissolve≈dissolve). The overlap region IS the transition.
//   | trim A B        keep source seconds A→B (begin/end in-out points); B clamps to the clip's
//                     length. `trim 0 7` → a 7s clip. Trims BEFORE speed.
//   | speed F         retime: F× faster (finalDur = trimmedDur / F). `speed 2` halves the length.
//                     Video via setpts, audio via atempo (chained for F outside 0.5–2×).
//
// A `@card` line is a GENERATED text-card part (not a clip file) — baked on the fly from the
// parameterised `titlecard` cart, then stitched in like any clip (design: trailer-builder.md):
//   @card <secs> | <cut> | title "…" | sub "…" | body "…" | anim <a> | bg <palette-idx>
//   · <secs>  the card's on-screen duration       · <cut>  the transition INTO it (as above)
//   · title/sub/body  content lines, big→small, rendered in written order (quotes optional)
//   · anim  fade | slide bottom|top|left|right     · bg  a 0–31 palette index for the backdrop
//   e.g.  @card 2.5 | fade 0.5 | title "TINY JAM" | sub "3 synths, one app" | anim slide bottom
//
// Clips of different sizes are letterboxed (nearest-neighbour, pixels stay crisp) onto the
// target canvas. A reel is a standalone *watchable* file, so it's NEAREST-upscaled at encode
// time (default 3×) and written yuv444p — crisp in any video player (the native clips rely on
// the gallery's CSS image-rendering:pixelated, which a raw player doesn't apply). Clips bake
// with audio by default; a silent clip gets silence so the crossfade still lines up.
// Flags / `# meta`: --fps --crf --size WxH --scale N  (N = integer nearest upscale; `# scale N`).

const fs   = require('fs')
const path = require('path')
const { spawnSync, spawn } = require('child_process')
const mk = require('./make-cart.js')

const args = process.argv.slice(2)
const opt = (flag, def) => { const i = args.indexOf(flag); return i >= 0 && i + 1 < args.length ? args[i + 1] : def }

const REELS_IN  = path.join(mk.ROOT_DIR, 'tools', 'reels')
const REELS_OUT = path.join(mk.ROOT_DIR, 'editor', 'public', 'reels')
const CLIPS_OUT = path.join(mk.ROOT_DIR, 'editor', 'public', 'clips')

// ── resolve the manifest + output ─────────────────────────────
let manifest = opt('--from', null), out = opt('--out', null), reelName = null
if (!manifest) {
  reelName = args.find(a => !a.startsWith('--') && !args[args.indexOf(a) - 1]?.startsWith('--'))
  if (!reelName) { console.error('usage: node tools/compose-clips.js <reel> | --from <file> [--out <path>]'); process.exit(1) }
  manifest = path.join(REELS_IN, `${reelName}.reel`)
}
if (!fs.existsSync(manifest)) { console.error('no manifest at', manifest); process.exit(1) }
if (!out) out = reelName ? path.join(REELS_OUT, `${reelName}.webm`) : path.join(mk.ROOT_DIR, 'docs', 'media', 'reel.webm')
out = path.resolve(out)

// ── parse the manifest ────────────────────────────────────────
const meta = { fps: 30, crf: 28, size: null, scale: 3, xtype: 'fade', xdur: 0.5 }
const shots = []
for (const raw of fs.readFileSync(manifest, 'utf8').split('\n')) {
  const line = raw.trim()
  if (!line) continue
  let m
  if ((m = line.match(/^#\s*fps\s+(\d+)/)))        { meta.fps  = +m[1]; continue }
  if ((m = line.match(/^#\s*crf\s+(\d+)/)))        { meta.crf  = +m[1]; continue }
  if ((m = line.match(/^#\s*size\s+(\d+)x(\d+)/))) { meta.size = [+m[1], +m[2]]; continue }
  if ((m = line.match(/^#\s*scale\s+(\d+)/)))      { meta.scale = +m[1]; continue }
  if ((m = line.match(/^#\s*xfade\s+(\w+)\s+([\d.]+)/))) { meta.xtype = m[1]; meta.xdur = +m[2]; continue }
  if (line.startsWith('#')) continue
  if (line.startsWith('@card')) {   // a generated text-card part (baked from the titlecard cart)
    const parts = line.split('|').map(s => s.trim())
    const shot = { card: { dur: parseFloat(parts[0].split(/\s+/)[1]) || 2.0, lines: [], anim: 'fade', bg: null },
                   xtype: meta.xtype, xdur: meta.xdur, trim: null, speed: 1 }
    for (const seg of parts.slice(1)) {
      let cm
      if      ((cm = seg.match(/^(title|sub|body)\s+(.*)$/))) shot.card.lines.push({ role: cm[1], text: cm[2].replace(/^"(.*)"$/, '$1') })
      else if ((cm = seg.match(/^anim\s+(.+)$/)))             shot.card.anim = cm[1]
      else if ((cm = seg.match(/^in\s+([\d.]+)\s+(.+)$/)))    { shot.card.inDur = +cm[1]; shot.card.inEffect = cm[2] }
      else if ((cm = seg.match(/^out\s+([\d.]+)\s+(.+)$/)))   { shot.card.outDur = +cm[1]; shot.card.outEffect = cm[2] }
      else if ((cm = seg.match(/^bg\s+(\d+)/)))               shot.card.bg = +cm[1]
      else if ((cm = seg.match(/^boil\s+([\d.]+)/)))          shot.card.boil = +cm[1]
      else if ((cm = seg.match(/^breathe\s+([\d.]+)/)))       shot.card.breathe = +cm[1]
      else if ((cm = seg.match(/^style\s+/)))                 { /* named styles: TBD */ }
      else if ((cm = seg.match(/^(\w+)\s+([\d.]+)/)))         { shot.xtype = cm[1]; shot.xdur = +cm[2] }   // the cut INTO this card
    }
    shot.ref = `@card "${(shot.card.lines[0] || {}).text || ''}"`
    shots.push(shot)
    continue
  }
  if (line.startsWith('over')) {   // a text OVERLAY on the preceding clip — anchored to it in relative time
    const prev = shots[shots.length - 1]
    if (!prev) { console.error(`"over …" with no clip above it: ${line}`); process.exit(1) }
    const parts = line.split('|').map(s => s.trim())
    const win = parts[0].match(/@\s*([\d.]+)\s*-\s*([\d.]+)/)   // over @a-b (relative seconds into the clip)
    const a = win ? +win[1] : 0, b = win ? +win[2] : 0
    const ov = { a, b, dur: Math.max(0.1, b - a), pos: 'bottom', card: { dur: Math.max(0.1, b - a), lines: [], anim: 'fade', bg: null, magic: true, pos: 'bottom' } }
    for (const seg of parts.slice(1)) {
      let cm
      if      ((cm = seg.match(/^(title|sub|body)\s+(.*)$/))) ov.card.lines.push({ role: cm[1], text: cm[2].replace(/^"(.*)"$/, '$1') })
      else if ((cm = seg.match(/^anim\s+(.+)$/)))             ov.card.anim = cm[1]
      else if ((cm = seg.match(/^in\s+([\d.]+)\s+(.+)$/)))    { ov.card.inDur = +cm[1]; ov.card.inEffect = cm[2] }
      else if ((cm = seg.match(/^out\s+([\d.]+)\s+(.+)$/)))   { ov.card.outDur = +cm[1]; ov.card.outEffect = cm[2] }
      else if ((cm = seg.match(/^pos\s+(\w+)/)))              { ov.pos = cm[1]; ov.card.pos = cm[1] }
      else if ((cm = seg.match(/^boil\s+([\d.]+)/)))          ov.card.boil = +cm[1]
      else if ((cm = seg.match(/^breathe\s+([\d.]+)/)))       ov.card.breathe = +cm[1]
    }
    ;(prev.overlays || (prev.overlays = [])).push(ov)
    continue
  }
  const [refPart, ...segs] = line.split('|').map(s => s.trim())
  const shot = { ref: refPart, xtype: meta.xtype, xdur: meta.xdur, trim: null, speed: 1 }
  for (const seg of segs) {
    if (!seg) continue
    let sm
    if ((sm = seg.match(/^trim\s+([\d.]+)\s+([\d.]+)/)))  { shot.trim = [+sm[1], +sm[2]]; continue }
    if ((sm = seg.match(/^speed\s+([\d.]+)/)))            { shot.speed = +sm[1]; continue }
    if ((sm = seg.match(/^(\w+)\s+([\d.]+)/)))            { shot.xtype = sm[1]; shot.xdur = +sm[2]; continue }
  }
  if (!(shot.speed > 0)) { console.error(`bad speed on "${refPart}" (must be > 0)`); process.exit(1) }
  // resolve ref → a webm path
  shot.file = /[\/.]/.test(refPart) && refPart.endsWith('.webm') ? path.resolve(refPart)
            : path.join(CLIPS_OUT, ...refPart.split('/')) + '.webm'
  if (!fs.existsSync(shot.file)) { console.error(`clip not found for "${refPart}": ${shot.file}\n  (bake it first: node tools/make-gif.js ${refPart.split('/')[0]} --recipe ${refPart.split('/')[1] || ''})`); process.exit(1) }
  shots.push(shot)
}
if (shots.length < 2) { console.error('a reel needs at least 2 clips'); process.exit(1) }
if (opt('--fps', null)) meta.fps = +opt('--fps')
if (opt('--crf', null)) meta.crf = +opt('--crf')
if (opt('--size', null)) { const s = opt('--size').match(/(\d+)x(\d+)/); if (s) meta.size = [+s[1], +s[2]] }
if (opt('--scale', null)) meta.scale = +opt('--scale')

// bake any @card parts to a webm: write a params file, run the parameterised titlecard
// cart through make-gif for the card's duration. Content-hashed so an unchanged card is
// only baked once. (One filtergraph pass downstream then treats it like any clip.)
function bakeCard(card, fps) {
  const dir = path.join(mk.BUILD_DIR, '.cards'); fs.mkdirSync(dir, { recursive: true })
  const plines = []
  if (card.magic)          plines.push('bg magic')       // overlay: keyed out at compose
  else if (card.bg != null) plines.push(`bg ${card.bg}`)
  if (card.pos)            plines.push(`pos ${card.pos}`)
  if (card.boil != null)    plines.push(`boil ${card.boil}`)
  if (card.breathe != null) plines.push(`breathe ${card.breathe}`)
  plines.push(`dur ${card.dur}`)   // the cart needs the total to time the OUT window
  plines.push(`in ${card.inDur != null ? card.inDur : 0.5} ${card.inEffect || card.anim || 'fade'}`)
  if (card.outDur > 0) plines.push(`out ${card.outDur} ${card.outEffect || 'slide top'}`)
  for (const l of card.lines) plines.push(`${l.role} ${l.text}`)
  const key   = require('crypto').createHash('sha1').update(JSON.stringify(card) + fps).digest('hex').slice(0, 16)
  const pfile = path.join(dir, `params-${key}.txt`)
  const out   = path.join(dir, `card-${key}.webm`)
  fs.writeFileSync(pfile, plines.join('\n') + '\n')
  const frames = Math.max(1, Math.round(card.dur * 60))   // the cart runs at 60fps
  console.log(`  baking card [${card.lines.map(l => l.text).join(' / ') || 'empty'}] (${card.dur}s)…`)
  const r = spawnSync('node',
    [path.join(mk.ROOT_DIR, 'tools/make-gif.js'), 'titlecard', '--frames', String(frames), '--fps', String(fps), '--out', out, '--mute'],
    { env: { ...process.env, TITLECARD_PARAMS: pfile }, encoding: 'utf8' })
  if (r.status !== 0) { console.error('card bake failed:\n' + (r.stderr || r.stdout || '')); process.exit(1) }
  return out
}
for (const s of shots) {
  if (s.card) s.file = bakeCard(s.card, meta.fps)
  for (const ov of (s.overlays || [])) ov.file = bakeCard(ov.card, meta.fps)   // magic-bg card, keyed at compose
}

// ── probe each clip: duration + size + has-audio ──────────────
const ffprobe = (a) => spawnSync('ffprobe', ['-v', 'error', ...a], { encoding: 'utf8' }).stdout.trim()
for (const s of shots) {
  s.srcDur = parseFloat(ffprobe(['-show_entries', 'format=duration', '-of', 'default=nk=1:nw=1', s.file])) || 0
  const wh = ffprobe(['-select_streams', 'v:0', '-show_entries', 'stream=width,height', '-of', 'csv=p=0', s.file]).split(',')
  s.w = +wh[0]; s.h = +wh[1]
  s.hasAudio = ffprobe(['-select_streams', 'a', '-show_entries', 'stream=index', '-of', 'csv=p=0', s.file]).length > 0
  if (s.srcDur <= 0) { console.error('could not read duration of', s.file); process.exit(1) }
  // trim (begin/end, clamped to the source) → then speed retimes. s.dur is the FINAL
  // duration everything downstream (xfade offsets, silent-fill, totals) reasons about.
  const t0 = s.trim ? Math.max(0, Math.min(s.trim[0], s.srcDur)) : 0
  const t1 = s.trim ? Math.max(t0, Math.min(s.trim[1], s.srcDur)) : s.srcDur
  s.trimStart = t0; s.trimEnd = t1
  const trimmed = t1 - t0
  s.dur = trimmed / s.speed
  if (s.dur <= 0) { console.error(`"${s.ref}": trim ${t0}→${t1} @ speed ${s.speed} leaves no footage`); process.exit(1) }
}
// canvas = base size × an integer NEAREST upscale, so the reel is crisp in any video
// player (unlike the native clips, which rely on the gallery's CSS image-rendering:pixelated).
const base = meta.size || [shots[0].w, shots[0].h]
const S = Math.max(1, meta.scale)
const W = base[0] * S, H = base[1] * S

// clamp each transition shorter than both neighbouring clips (xfade can't outlast a clip)
for (let i = 1; i < shots.length; i++) {
  const cap = Math.min(shots[i - 1].dur, shots[i].dur) - 0.05
  shots[i].xdur = Math.max(0.1, Math.min(shots[i].xdur, cap))
}

// atempo only accepts 0.5–2.0, so decompose an arbitrary speed into a chain of factors.
const atempoChain = (f) => {
  if (Math.abs(f - 1) < 1e-6) return ''
  const parts = []; let r = f
  while (r > 2.0 + 1e-9) { parts.push('atempo=2.0'); r /= 2 }
  while (r < 0.5 - 1e-9) { parts.push('atempo=0.5'); r *= 2 }
  parts.push(`atempo=${r.toFixed(6)}`)
  return parts.join(',') + ','
}

// ── build the ffmpeg filter graph ─────────────────────────────
// per clip: [trim → retime] → scale (nearest, keep aspect) → letterbox-pad to WxH → fps → [overlays] → yuv420p.
// Inputs are indexed explicitly (s.vin / ov.vin) because text overlays add extra -i inputs between clips.
const MAGIC_KEY = '0x00e436'   // titlecard's overlay bg (CLR_GREEN); keyed with a small tolerance for YUV rounding
const inputs = []
let inIdx = 0
shots.forEach(s => {
  s.vin = inIdx++; inputs.push('-i', s.file)
  for (const ov of (s.overlays || [])) { ov.vin = inIdx++; inputs.push('-i', ov.file) }
})
const vparts = [], aparts = []
shots.forEach((s, i) => {
  const trimming = s.trimStart > 1e-6 || s.trimEnd < s.srcDur - 1e-6
  // video: trim (reset PTS to 0) then retime by speed; skip both when untouched (byte-identical)
  let vpre = ''
  if (trimming)                 vpre += `trim=start=${s.trimStart}:end=${s.trimEnd},`
  if (trimming || s.speed !== 1) vpre += `setpts=(PTS-STARTPTS)/${s.speed},`
  const scaled = `[${s.vin}:v]${vpre}scale=${W}:${H}:flags=neighbor:force_original_aspect_ratio=decrease,` +
                 `pad=${W}:${H}:(ow-iw)/2:(oh-ih)/2:color=black,fps=${meta.fps},setsar=1`
  const overlays = s.overlays || []
  if (!overlays.length) {
    vparts.push(`${scaled},format=yuv420p[v${i}]`)
  } else {
    vparts.push(`${scaled}[vb${i}0]`)
    let cur = `vb${i}0`
    overlays.forEach((ov, k) => {   // key the magic colour → composite in the clip-local window [a,b]
      vparts.push(`[${ov.vin}:v]scale=${W}:${H}:flags=neighbor,setsar=1,format=rgba,` +
                  `colorkey=${MAGIC_KEY}:0.10:0.0,setpts=PTS-STARTPTS+${ov.a}/TB[ovk${i}_${k}]`)
      const out = k === overlays.length - 1 ? `vov${i}` : `vb${i}${k + 1}`
      vparts.push(`[${cur}][ovk${i}_${k}]overlay=0:0:enable='between(t,${ov.a},${ov.b})':eof_action=pass[${out}]`)
      cur = out
    })
    vparts.push(`[vov${i}]format=yuv420p[v${i}]`)
  }
  const apre = trimming ? `atrim=start=${s.trimStart}:end=${s.trimEnd},asetpts=PTS-STARTPTS,` : ''
  aparts.push(s.hasAudio
    ? `[${s.vin}:a]${apre}aformat=sample_rates=44100:channel_layouts=stereo,${atempoChain(s.speed)}asetpts=N/SR/TB[a${i}]`
    : `aevalsrc=0:d=${s.dur.toFixed(3)}:s=44100:c=stereo[a${i}]`)
})

// chain xfade (video) + acrossfade (audio); offset_i = (combined so far) - xdur_i
let vcur = 'v0', acur = 'a0', combined = shots[0].dur
const chain = []
for (let i = 1; i < shots.length; i++) {
  const d = shots[i].xdur, off = (combined - d).toFixed(3)
  const vo = `vx${i}`, ao = `ax${i}`
  chain.push(`[${vcur}][v${i}]xfade=transition=${shots[i].xtype}:duration=${d}:offset=${off}[${vo}]`)
  chain.push(`[${acur}][a${i}]acrossfade=d=${d}[${ao}]`)
  vcur = vo; acur = ao
  combined = combined + shots[i].dur - d
}
const filter = [...vparts, ...aparts, ...chain].join(';')

// ── encode ────────────────────────────────────────────────────
fs.mkdirSync(path.dirname(out), { recursive: true })
const ffArgs = ['-y', ...inputs, '-filter_complex', filter,
  '-map', `[${vcur}]`, '-map', `[${acur}]`,
  '-c:v', 'libvpx-vp9', '-pix_fmt', 'yuv444p', '-crf', String(meta.crf), '-b:v', '0',
  '-c:a', 'libopus', '-b:a', '128k',
  '-progress', 'pipe:1', '-nostats', out]   // stream encode progress → a rough % counter (the long, otherwise-silent step)

console.log(`composing ${shots.length} clips → ${path.relative(mk.ROOT_DIR, out)}  (${W}×${H} = ${S}× nearest @ ${meta.fps}fps, crf ${meta.crf})`)
shots.forEach((s, i) => {
  const edits = [
    s.trim ? `trim ${s.trimStart}→${s.trimEnd}` : null,
    s.speed !== 1 ? `${s.speed}×` : null,
  ].filter(Boolean).join(' ')
  console.log(`  ${i === 0 ? '▸' : `↳ ${shots[i].xtype} ${shots[i].xdur}s`}  ${s.ref}  (${s.dur.toFixed(1)}s${s.hasAudio ? '' : ', silent'}${edits ? `, ${edits}` : ''})`)
})
// stream ffmpeg's -progress (out_time=HH:MM:SS.us, reliable across builds) → a throttled % of the
// known output duration, so the editor's build log keeps ticking through the long encode.
const totalSec = combined
console.log(`  encoding ~${totalSec.toFixed(1)}s of video…`)
const enc = spawn('ffmpeg', ffArgs, { stdio: ['ignore', 'pipe', 'pipe'] })
let errBuf = '', lastDecade = -1
enc.stdout.on('data', d => {
  for (const line of d.toString().split('\n')) {
    const m = line.match(/^out_time=(\d+):(\d+):([\d.]+)/)
    if (!m) continue
    const sec = (+m[1]) * 3600 + (+m[2]) * 60 + parseFloat(m[3])
    const pct = Math.min(99, Math.floor(sec / totalSec * 100))
    const decade = Math.floor(pct / 10) * 10
    if (decade > lastDecade) { lastDecade = decade; process.stdout.write(`  encoding… ${decade}%\n`) }
  }
})
enc.stderr.on('data', d => { errBuf += d.toString() })
enc.on('error', e => { console.error(String(e.message || e)); process.exit(1) })
enc.on('exit', code => {
  if (code !== 0) { console.error(errBuf || 'ffmpeg failed'); process.exit(1) }
  console.log(`✓ ${path.relative(mk.ROOT_DIR, out)}  (${(fs.statSync(out).size / 1024).toFixed(0)} KB, ~${totalSec.toFixed(1)}s)`)
})
