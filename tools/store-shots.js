#!/usr/bin/env node
// store-shots.js — turn a native-res cart frame into App Store screenshots at EXACT device
// resolutions. Solves the aspect-ratio gap by COMPOSITING, not stretching: the crisp,
// integer-upscaled cart canvas is centered on a designed background with a caption in the
// breathing room — the lo-fi console as a framed artifact (looks premium, needs ZERO engine
// work / no responsive layout). store-agents.md §1 (hero-frame director), the deterministic
// asset leg. Needs ffmpeg (already a make-gif dependency); no node deps.
//
//   # dump a native frame first (play.js keeps the PNGs):
//   node tools/play.js groovebox run --headless --frames 60 --dump /tmp/gb
//   node tools/store-shots.js --in /tmp/gb/frame_00040.png --device iphone69,ipad13 \
//     --out build/.shots --caption "Make music with tiny synths" --bg 141726
//
// Devices are PORTRAIT and are the sizes Apple actually needs — the 6.9" set auto-scales down
// to every smaller iPhone; iPad only if you ship it. Or pass a literal WxH.
//   iphone69 1290x2796 · iphone65 1242x2688 · ipad13 2048x2732 · ipad11 1668x2388
'use strict'
const fs = require('fs')
const path = require('path')
const { execFileSync } = require('child_process')

const ROOT = path.join(__dirname, '..')
const DEVICES = {
  iphone69: [1290, 2796], iphone65: [1242, 2688],
  ipad13:   [2048, 2732], ipad11:  [1668, 2388],
}

// ── args ─────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2)
const opt = { in: '', device: 'iphone69', out: 'build/.shots', bg: '141726',
  caption: '', font: 'tools/fonts/Bungee-Regular.ttf', textcolor: 'ffffff', scale: '' }
for (let i = 0; i < argv.length; i++) {
  const k = argv[i].replace(/^--/, '')
  if (argv[i].startsWith('--') && k in opt) opt[k] = argv[++i] ?? ''
  else { console.error(`unknown arg: ${argv[i]}\nusage: node tools/store-shots.js --in frame.png --device iphone69,ipad13 --out dir [--caption "…"] [--bg 141726] [--scale N]`); process.exit(1) }
}
if (!opt.in || !fs.existsSync(opt.in)) { console.error(`--in <png> required (got "${opt.in}")`); process.exit(1) }

const hex = s => '0x' + s.replace(/^(#|0x)/, '')
const fontAbs = path.resolve(ROOT, opt.font)
const haveFont = fs.existsSync(fontAbs)
if (opt.caption && !haveFont) console.error(`  (font not found at ${opt.font} — rendering without caption)`)

// native frame size
const probe = execFileSync('ffprobe', ['-v', 'error', '-select_streams', 'v',
  '-show_entries', 'stream=width,height', '-of', 'csv=p=0', opt.in]).toString().trim()
const [w, h] = probe.split(',').map(Number)

fs.mkdirSync(path.resolve(ROOT, opt.out), { recursive: true })
const base = path.basename(opt.in).replace(/\.[^.]+$/, '')
let capFile = ''
if (opt.caption && haveFont) {
  capFile = path.join(path.resolve(ROOT, opt.out), '.caption.txt')
  fs.writeFileSync(capFile, opt.caption)
}

const targets = opt.device.split(',').map(s => s.trim()).filter(Boolean)
for (const dev of targets) {
  const [DW, DH] = DEVICES[dev] || (/^\d+x\d+$/.test(dev) ? dev.split('x').map(Number) : [])
  if (!DW) { console.error(`  unknown device: ${dev} (skipping)`); continue }

  // largest integer upscale that fits the width and leaves the top ~30% for a caption
  const S = parseInt(opt.scale, 10) || Math.max(1, Math.min(Math.floor(DW / w), Math.floor(DH * 0.7 / h)))
  const sw = w * S, sh = h * S
  // with a caption, seat the cart in the upper-third so headline + cart read as one block
  // (rather than a dead-centre cart floating far below the headline); else centre it.
  const yoff = capFile ? Math.round(DH * 0.24) : Math.round((DH - sh) / 2)
  const filters = [`scale=${sw}:${sh}:flags=neighbor`,
    `pad=${DW}:${DH}:(${DW}-iw)/2:${yoff}:color=${hex(opt.bg)}`]
  if (capFile) {
    const fsz = Math.round(DW * 0.052)
    const capY = Math.round(DH * 0.08)
    filters.push(`drawtext=fontfile=${fontAbs}:textfile=${capFile}:fontcolor=${hex(opt.textcolor)}`
      + `:fontsize=${fsz}:x=(w-text_w)/2:y=${capY}`)
  }
  const outPath = path.join(path.resolve(ROOT, opt.out), `${base}-${dev}.png`)
  execFileSync('ffmpeg', ['-y', '-loglevel', 'error', '-i', opt.in, '-vf', filters.join(','), outPath])
  console.log(`  ✓ ${dev.padEnd(9)} ${DW}x${DH}  (cart ${sw}x${sh}, ×${S})  → ${path.relative(ROOT, outPath)}`)
}
if (capFile) fs.rmSync(capFile, { force: true })
console.log(`\ncart ${w}x${h} composited onto ${targets.length} device frame(s). Not stretched — the ratio gap is the caption's canvas.\n`)
