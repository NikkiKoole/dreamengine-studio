#!/usr/bin/env node
// dress-clip.js — "dress" a cart clip into a 9:16 Short with ON-SCREEN TEXT in the letterbox bars.
// The missing step 3 of the Promote → post pipeline: record a take → bake a clip → *dress it with
// your own words* → export/upload. The console becomes a framed artifact (a title card above, a
// hook/CTA below), same "composite don't stretch" spirit as store-shots.js — but for video, with
// text you type. Design + decisions: docs/design/export-ratios.md "Dressed composite".
//
//   node tools/dress-clip.js editor/public/clips/acidcandy/01-take.webm \
//        --title "TINY ACID JAM" --sub "303 / 808 / 909 in your pocket" \
//        --hook "MAKE A BEAT" --cta "play free in your browser" --footer "made in dreamengine"
//
//   # every text region is OPTIONAL — only what you pass is drawn:
//   node tools/dress-clip.js clip.webm --title "hello" --out /tmp/out.webm
//
// Text is ARBITRARY / hand-typed (you write the words — no auto-derived prose). Regions:
//   --title / --sub  (top card)   --hook / --cta / --footer  (bottom card)
// Look: --bg <hex> (canvas) · --accent <hex> (device frame) · --w/--h (default 1080x1920) ·
//   --scale <n> (force console integer upscale; default = largest that fits) · --out <path> · --mp4.
// Motion: --boil <0..1> (per-letter hand-drawn wobble, dflt 0.6) · --breathe <0..1> (scale pulse) ·
//   --anim <fade|slide bottom|slide top|slide left|slide right> (entrance, dflt "slide bottom") ·
//   --in <sec> (entrance duration) · --secs <n> (cap output length — used for a quick motion preview).
//
// FONT + MOTION — the on-screen text is the REAL engine bitmap font, drawn by the `titlecard` cart
// (the trailer builder's text renderer): dos_8x8 with a drop shadow, per-letter BOIL, and a tween-in
// entrance. dress-clip bakes two titlecards (title/sub up top, hook/cta/footer at the bottom) onto a
// magic-green key, then colour-keys them into the bars over the console — the same pixel-perfect
// overlay path compose-clips.js uses for reels. No smooth TTFs, no drawtext for the output.
//
//   --preview <out.png> [--at <sec>]: a FAST single-frame LAYOUT preview (ffmpeg drawtext with a
//     smooth face — approximate placement/wrapping/colours as you type in the editor). The real
//     pixel font + boil + tween only appear in the full render (the editor's ▶ preview motion bakes
//     a short real clip). This split is why the preview is instant and the render is engine-true.
//
// The clip is integer-upscaled (crisp, nearest) to FIT the frame, centred, leaving bars — the bars
// are the canvas the text lives on. (A resizable cart baked FULL-BLEED 9:16 has no bars; this tool
// is the LETTERBOX-framed look — see export-ratios.md.)

const fs = require('fs')
const path = require('path')
const { spawnSync } = require('child_process')

const ROOT = path.resolve(__dirname, '..')
const TMP = path.join(ROOT, 'build', '.yt') // titlecard params + baked cards + text files — throwaway
const MAGIC_KEY = '0x00e436'                 // titlecard's overlay bg (CLR_GREEN); keyed with tolerance
const CARD_DIV = 3                            // frame px per card px — the card renders at W/3 × H/3, ×3 up

const argv = process.argv.slice(2)
const opt = {
  input: '', title: '', sub: '', hook: '', cta: '', footer: '',
  bg: '0d0d16', accent: 'ff6ab5', ink: 'd2d2e2',
  w: 1080, h: 1920, scale: 0, out: '', mp4: false,
  boil: 0.6, breathe: 0.0, anim: 'slide bottom', in: 0.5, secs: 0,
  preview: '', at: 0.5, // --preview <png>: fast drawtext LAYOUT preview (smooth face), one frame
  fontDisplay: path.join(ROOT, 'tools/fonts/Bungee-Regular.ttf'),
  fontBody: path.join(ROOT, 'editor/public/ComicMono-Bold.ttf'),
}
for (let i = 0; i < argv.length; i++) {
  const a = argv[i]
  if (a === '--title') opt.title = argv[++i]
  else if (a === '--sub') opt.sub = argv[++i]
  else if (a === '--hook') opt.hook = argv[++i]
  else if (a === '--cta') opt.cta = argv[++i]
  else if (a === '--footer') opt.footer = argv[++i]
  else if (a === '--bg') opt.bg = argv[++i].replace(/^#|^0x/, '')
  else if (a === '--accent') opt.accent = argv[++i].replace(/^#|^0x/, '')
  else if (a === '--ink') opt.ink = argv[++i].replace(/^#|^0x/, '')
  else if (a === '--w') opt.w = +argv[++i]
  else if (a === '--h') opt.h = +argv[++i]
  else if (a === '--scale') opt.scale = +argv[++i]
  else if (a === '--boil') opt.boil = +argv[++i]
  else if (a === '--breathe') opt.breathe = +argv[++i]
  else if (a === '--anim') opt.anim = argv[++i]
  else if (a === '--in') opt.in = +argv[++i]
  else if (a === '--secs') opt.secs = +argv[++i]
  else if (a === '--out') opt.out = argv[++i]
  else if (a === '--preview') opt.preview = argv[++i]
  else if (a === '--at') opt.at = +argv[++i]
  else if (a === '--mp4') opt.mp4 = true
  else if (a === '--font-display') opt.fontDisplay = path.resolve(argv[++i])
  else if (a === '--font-body') opt.fontBody = path.resolve(argv[++i])
  else if (!a.startsWith('-') && !opt.input) opt.input = argv[i]
  else { console.error(`unknown arg: ${a}`); process.exit(2) }
}
if (!opt.input) {
  console.error('usage: node tools/dress-clip.js <clip.webm|mp4> [--title …] [--sub …] [--hook …] [--cta …] [--footer …]\n' +
    '       [--bg hex] [--accent hex] [--boil 0..1] [--breathe 0..1] [--anim "slide bottom"] [--out path] [--mp4]\n' +
    '       [--preview out.png [--at sec]]  [--secs n]')
  process.exit(2)
}
if (!fs.existsSync(opt.input)) die(`no clip at ${opt.input}`)

// ── measure the source, place the console (integer upscale that fits, centred) ───────────────────
const [iw, ih] = probeDims(opt.input)
const W = opt.w, H = opt.h
const s = opt.scale > 0 ? opt.scale : Math.max(1, Math.min(Math.floor((W * 0.92) / iw), Math.floor((H * 0.44) / ih)))
const cw = iw * s, ch = ih * s
const cartX = Math.round((W - cw) / 2), cartTop = Math.round((H - ch) / 2)
fs.mkdirSync(TMP, { recursive: true })

// ── the FAST layout preview: ffmpeg drawtext (smooth face) → one frame, then stop. Approximate
//    placement only; the real pixel font + boil + tween live in the full render below. ────────────
if (opt.preview) {
  for (const f of [opt.fontDisplay, opt.fontBody]) if (!fs.existsSync(f)) die(`font not found: ${f}`)
  const cartBot = cartTop + ch
  const R = v => Math.round(v)
  const titleSize = R(H * 0.045), subSize = R(H * 0.020), hookSize = R(H * 0.037), ctaSize = R(H * 0.023), footSize = R(H * 0.016)
  const titleY = R(cartTop * 0.30), subY = titleY + titleSize + R(H * 0.010), accentY = cartTop - R(H * 0.085)
  const hookY = cartBot + R((H - cartBot) * 0.22), ctaY = hookY + hookSize + R(H * 0.010), footY = H - R(H * 0.055)
  const textFile = (name, str) => { const p = path.join(TMP, `dress-${name}.txt`); fs.writeFileSync(p, str); return p }
  const dt = (font, file, color, size, y) =>
    `drawtext=fontfile=${font}:textfile=${file}:fontcolor=0x${color}:fontsize=${size}:x=(w-text_w)/2:y=${y}`
  const chain = []
  chain.push(`color=c=0x${opt.bg}:s=${W}x${H}[bg]`)
  chain.push(`[0:v]scale=${cw}:${ch}:flags=neighbor[cart]`)
  chain.push(`[bg][cart]overlay=${cartX}:${cartTop}:shortest=1[base]`)
  let last = 'base'
  const step = (filter) => { const tag = `s${chain.length}`; chain.push(`[${last}]${filter}[${tag}]`); last = tag }
  step(`drawbox=x=${cartX - 6}:y=${cartTop - 6}:w=${cw + 12}:h=${ch + 12}:color=0x${opt.accent}:t=4`)
  if (opt.title)  step(dt(opt.fontDisplay, textFile('title', opt.title), 'ffffff', titleSize, titleY))
  if (opt.sub)    step(dt(opt.fontBody, textFile('sub', opt.sub), 'ffffff', subSize, subY))
  if (opt.hook)   step(dt(opt.fontDisplay, textFile('hook', opt.hook), 'ffffff', hookSize, hookY))
  if (opt.cta)    step(dt(opt.fontBody, textFile('cta', opt.cta), 'ffffff', ctaSize, ctaY))
  if (opt.footer) step(dt(opt.fontBody, textFile('footer', opt.footer), 'ffffff', footSize, footY))
  const scriptPath = path.join(TMP, 'dress-preview.filter')
  fs.writeFileSync(scriptPath, chain.join(';\n'))
  const r = spawnSync('ffmpeg', ['-y', '-ss', String(opt.at), '-i', opt.input,
    '-filter_complex_script', scriptPath, '-map', '[' + last + ']',
    '-frames:v', '1', '-update', '1', opt.preview],
    { stdio: ['ignore', 'ignore', 'inherit'] })
  if (r.status !== 0) die('ffmpeg preview failed')
  console.log(`✓ preview ${path.relative(ROOT, opt.preview)} (${W}x${H}, frame @ ${opt.at}s)`)
  process.exit(0)
}

// ── full render: engine text via the titlecard cart, colour-keyed into the bars ──────────────────
const clipDur = probeDur(opt.input)
const SHORT_CAP = 60   // a Short maxes at 60s; a long take gets capped (baking 2min of engine text is glacial)
const dur = opt.secs > 0 ? Math.min(opt.secs, clipDur) : Math.min(clipDur, SHORT_CAP)
if (opt.secs <= 0 && clipDur > SHORT_CAP) console.log(`  clip is ${clipDur.toFixed(0)}s → capping the Short to ${SHORT_CAP}s`)
const fps = 30
const cardW = Math.round(W / CARD_DIV), cardH = Math.round(H / CARD_DIV)

// bake ONE titlecard (a stack of role'd lines, pinned top/bottom) onto the magic key, for `dur`s.
function bakeCard(name, pos, lines) {
  const pfile = path.join(TMP, `dress-${name}.txt`)
  const out = path.join(TMP, `dress-${name}.webm`)
  const p = ['bg magic', `pos ${pos}`, `boil ${opt.boil}`, `breathe ${opt.breathe}`,
    `dur ${dur.toFixed(3)}`, `in ${opt.in} ${opt.anim}`]
  for (const l of lines) p.push(`${l.role} ${l.text}`)
  fs.writeFileSync(pfile, p.join('\n') + '\n')
  const cartFrames = Math.max(1, Math.ceil(dur * 60)) // titlecard runs at 60fps; --fps downsamples
  console.log(`  baking ${name} card [${lines.map(l => l.text).join(' / ')}]…`)
  const r = spawnSync('node', [path.join(ROOT, 'tools/make-gif.js'), 'titlecard',
    '--frames', String(cartFrames), '--fps', String(fps), '--screen', `${cardW}x${cardH}`,
    '--out', out, '--mute'],
    { env: { ...process.env, TITLECARD_PARAMS: pfile }, cwd: ROOT, encoding: 'utf8' })
  if (r.status !== 0) die(`titlecard bake failed (${name}):\n` + (r.stderr || r.stdout || ''))
  return out
}

const topLines = []
if (opt.title) topLines.push({ role: 'title', text: opt.title })
if (opt.sub)   topLines.push({ role: 'sub', text: opt.sub })
const botLines = []
if (opt.hook)   botLines.push({ role: 'title', text: opt.hook })
if (opt.cta)    botLines.push({ role: 'sub', text: opt.cta })
if (opt.footer) botLines.push({ role: 'body', text: opt.footer })

const cards = []
if (topLines.length) cards.push({ file: bakeCard('top', 'top', topLines) })
if (botLines.length) cards.push({ file: bakeCard('bot', 'bottom', botLines) })

// build the composite: bg → console (nearest, centred) → device frame → each card colour-keyed over
const inputs = ['-i', opt.input]
cards.forEach((c, i) => { c.vin = i + 1; inputs.push('-i', c.file) })
const chain = []
chain.push(`color=c=0x${opt.bg}:s=${W}x${H}:d=${dur.toFixed(3)}[bg]`)
chain.push(`[0:v]scale=${cw}:${ch}:flags=neighbor[cart]`)
chain.push(`[bg][cart]overlay=${cartX}:${cartTop}:shortest=1[base]`)
chain.push(`[base]drawbox=x=${cartX - 6}:y=${cartTop - 6}:w=${cw + 12}:h=${ch + 12}:color=0x${opt.accent}:t=4[framed]`)
let last = 'framed'
cards.forEach((c, i) => {
  chain.push(`[${c.vin}:v]scale=${W}:${H}:flags=neighbor,setsar=1,format=rgba,colorkey=${MAGIC_KEY}:0.10:0.0[k${i}]`)
  const out = i === cards.length - 1 ? 'txt' : `o${i}`
  chain.push(`[${last}][k${i}]overlay=0:0:eof_action=pass[${out}]`)
  last = out
})
chain.push(`[${last}]format=yuv420p[v]`)
const scriptPath = path.join(TMP, 'dress.filter')
fs.writeFileSync(scriptPath, chain.join(';\n'))

const out = opt.out || opt.input.replace(/\.\w+$/, '') + (opt.mp4 ? '-dressed.mp4' : '-dressed.webm')
// VP9 at its default speed is glacial on a full-length 1080x1920 clip; -row-mt + -cpu-used make it
// practical with negligible quality cost for a Short (YouTube re-encodes anyway).
const vcodec = opt.mp4 ? ['-c:v', 'libx264', '-crf', '18', '-pix_fmt', 'yuv420p', '-movflags', '+faststart']
                       : ['-c:v', 'libvpx-vp9', '-crf', '32', '-b:v', '0', '-pix_fmt', 'yuv420p', '-row-mt', '1', '-cpu-used', '4', '-deadline', 'good']
console.log(`dressing ${path.relative(ROOT, opt.input)} → ${path.relative(ROOT, out)}`)
console.log(`  frame ${W}x${H} · console ${iw}x${ih} ×${s} centred · engine text (boil ${opt.boil}, ${opt.anim})${opt.secs > 0 ? ` · ${dur.toFixed(1)}s preview` : ''}`)
const durArg = dur < clipDur - 0.01 ? ['-t', dur.toFixed(3)] : []   // trim OUTPUT (motion preview / Short cap)
const r = spawnSync('ffmpeg', ['-y', ...inputs, '-filter_complex_script', scriptPath,
  '-map', '[v]', '-map', '0:a?', ...durArg, ...vcodec, '-c:a', 'copy', out],
  { stdio: ['ignore', 'ignore', 'inherit'] })
if (r.status !== 0) die('ffmpeg failed')
console.log(`✓ ${path.relative(ROOT, out)}`)

function probeDims(file) {
  const r = spawnSync('ffprobe', ['-v', 'error', '-select_streams', 'v:0', '-show_entries',
    'stream=width,height', '-of', 'csv=p=0', file], { encoding: 'utf8' })
  if (r.status !== 0) die(`ffprobe failed on ${file}`)
  return r.stdout.trim().split(',').map(Number)
}
function probeDur(file) {
  const r = spawnSync('ffprobe', ['-v', 'error', '-show_entries', 'format=duration',
    '-of', 'default=nk=1:nw=1', file], { encoding: 'utf8' })
  const d = parseFloat((r.stdout || '').trim())
  return d > 0 ? d : 5
}
function die(m) { console.error('✗ ' + m); process.exit(1) }
