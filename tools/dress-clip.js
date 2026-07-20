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
//   --title / --sub  (top card)   --hook / --cta  (bottom)   --footer  (bottom edge)
// Look: --bg <hex> (canvas) · --accent <hex> (title + frame + rule) · --ink <hex> (body text) ·
//   --w/--h (default 1080x1920) · --scale <n> (force integer upscale; default = largest that fits) ·
//   --font-display / --font-body (default Bungee / Comic Mono Bold) · --out <path> · --mp4.
//
// The clip is integer-upscaled (crisp, nearest) to FIT the frame, centred, leaving bars — the bars
// are the canvas the text lives on. (A resizable cart baked FULL-BLEED 9:16 has no bars; this tool
// is the LETTERBOX-framed look — see export-ratios.md.) Arbitrary text is passed via textfile= so
// colons/quotes/etc. never break the ffmpeg filtergraph.
//
// FONT — v1 uses smooth faces (Bungee display + Comic Mono body); the on-brand engine BITMAP font
// (FONT_BOOT/dos_8x8) needs a pixel TTF that isn't in the repo yet — swap it in via --font-display
// once it exists (the documented follow-up).

const fs = require('fs')
const path = require('path')
const { spawnSync } = require('child_process')

const ROOT = path.resolve(__dirname, '..')
const TMP = path.join(ROOT, 'build', '.yt') // text region files + nothing else — throwaway

const argv = process.argv.slice(2)
const opt = {
  input: '', title: '', sub: '', hook: '', cta: '', footer: '',
  bg: '0d0d16', accent: 'ff6ab5', ink: 'd2d2e2',
  w: 1080, h: 1920, scale: 0, out: '', mp4: false,
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
  else if (a === '--out') opt.out = argv[++i]
  else if (a === '--mp4') opt.mp4 = true
  else if (a === '--font-display') opt.fontDisplay = path.resolve(argv[++i])
  else if (a === '--font-body') opt.fontBody = path.resolve(argv[++i])
  else if (!a.startsWith('-') && !opt.input) opt.input = argv[i]
  else { console.error(`unknown arg: ${a}`); process.exit(2) }
}
if (!opt.input) {
  console.error('usage: node tools/dress-clip.js <clip.webm|mp4> [--title …] [--sub …] [--hook …] [--cta …] [--footer …] [--bg hex] [--accent hex] [--out path] [--mp4]')
  process.exit(2)
}
if (!fs.existsSync(opt.input)) die(`no clip at ${opt.input}`)
for (const f of [opt.fontDisplay, opt.fontBody]) if (!fs.existsSync(f)) die(`font not found: ${f}`)

// ── measure the source, place the cart, size the text (fractions of the target height) ──────────
const [iw, ih] = probeDims(opt.input)
const W = opt.w, H = opt.h
const s = opt.scale > 0 ? opt.scale : Math.max(1, Math.min(Math.floor((W * 0.92) / iw), Math.floor((H * 0.44) / ih)))
const cw = iw * s, ch = ih * s
const cartX = Math.round((W - cw) / 2), cartTop = Math.round((H - ch) / 2), cartBot = cartTop + ch

const R = v => Math.round(v)
const titleSize = R(H * 0.045), subSize = R(H * 0.020), hookSize = R(H * 0.037), ctaSize = R(H * 0.023), footSize = R(H * 0.016)
const titleY = R(cartTop * 0.30), subY = titleY + titleSize + R(H * 0.010), accentY = cartTop - R(H * 0.085)
const hookY = cartBot + R((H - cartBot) * 0.22), ctaY = hookY + hookSize + R(H * 0.010), footY = H - R(H * 0.055)

// ── build the filtergraph (text via textfile= so arbitrary strings can't break it) ──────────────
fs.mkdirSync(TMP, { recursive: true })
const textFile = (name, s) => { const p = path.join(TMP, `dress-${name}.txt`); fs.writeFileSync(p, s); return p }
const dt = (font, file, color, size, y) =>
  `drawtext=fontfile=${font}:textfile=${file}:fontcolor=0x${color}:fontsize=${size}:x=(w-text_w)/2:y=${y}`

const chain = []
chain.push(`color=c=0x${opt.bg}:s=${W}x${H}[bg]`)
chain.push(`[0:v]scale=${cw}:${ch}:flags=neighbor[cart]`)
chain.push(`[bg][cart]overlay=${cartX}:${cartTop}:shortest=1[base]`)
let last = 'base'
const step = (filter) => { const tag = `s${chain.length}`; chain.push(`[${last}]${filter}[${tag}]`); last = tag }

// a frame around the cart so it reads as a device, not a floating rectangle
step(`drawbox=x=${cartX - 6}:y=${cartTop - 6}:w=${cw + 12}:h=${ch + 12}:color=0x${opt.accent}:t=4`)
if (opt.title)  step(dt(opt.fontDisplay, textFile('title', opt.title), opt.accent, titleSize, titleY))
if (opt.sub)    step(dt(opt.fontBody, textFile('sub', opt.sub), opt.ink, subSize, subY))
if (opt.title || opt.sub) step(`drawbox=x=${R((W - 260) / 2)}:y=${accentY}:w=260:h=6:color=0x${opt.accent}:t=fill`)
if (opt.hook)   step(dt(opt.fontDisplay, textFile('hook', opt.hook), 'ffffff', hookSize, hookY))
if (opt.cta)    step(dt(opt.fontBody, textFile('cta', opt.cta), opt.accent, ctaSize, ctaY))
if (opt.footer) step(dt(opt.fontBody, textFile('footer', opt.footer), '707088', footSize, footY))

const graph = chain.join(';\n')
const scriptPath = path.join(TMP, 'dress.filter')
fs.writeFileSync(scriptPath, graph)

// ── render ───────────────────────────────────────────────────────────────────────────────────
const out = opt.out || opt.input.replace(/\.\w+$/, '') + (opt.mp4 ? '-dressed.mp4' : '-dressed.webm')
// VP9 at its default speed is glacial on a full-length 1080x1920 clip; -row-mt + -cpu-used make it
// practical with negligible quality cost for a Short (YouTube re-encodes anyway).
const vcodec = opt.mp4 ? ['-c:v', 'libx264', '-crf', '18', '-pix_fmt', 'yuv420p', '-movflags', '+faststart']
                       : ['-c:v', 'libvpx-vp9', '-crf', '32', '-b:v', '0', '-pix_fmt', 'yuv420p', '-row-mt', '1', '-cpu-used', '4', '-deadline', 'good']
console.log(`dressing ${path.relative(ROOT, opt.input)} → ${path.relative(ROOT, out)}`)
console.log(`  frame ${W}x${H} · cart ${iw}x${ih} ×${s} = ${cw}x${ch} centred (bars top/bottom)`)
const regions = ['title', 'sub', 'hook', 'cta', 'footer'].filter(k => opt[k])
console.log(`  text: ${regions.length ? regions.map(k => `${k}="${opt[k]}"`).join('  ') : '(none — bare bars)'}`)

const r = spawnSync('ffmpeg', ['-y', '-i', opt.input, '-filter_complex_script', scriptPath,
  '-map', '[' + last + ']', '-map', '0:a?', ...vcodec, '-c:a', 'copy', out],
  { stdio: ['ignore', 'ignore', 'inherit'] })
if (r.status !== 0) die('ffmpeg failed')
console.log(`✓ ${path.relative(ROOT, out)}`)

function probeDims(file) {
  const r = spawnSync('ffprobe', ['-v', 'error', '-select_streams', 'v:0', '-show_entries',
    'stream=width,height', '-of', 'csv=p=0', file], { encoding: 'utf8' })
  if (r.status !== 0) die(`ffprobe failed on ${file}`)
  return r.stdout.trim().split(',').map(Number)
}
function die(m) { console.error('✗ ' + m); process.exit(1) }
