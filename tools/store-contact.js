#!/usr/bin/env node
// store-contact.js — build a labelled CONTACT SHEET from a dump of cart frames, so an agent
// (or you) can eyeball a whole clip at once and pick the hero shots. The deterministic half
// of store-agents.md §1's hero-frame director: the tool samples + tiles + labels; the AGENT
// looks and chooses (no oracle scores "which frame sells it"); then store-shots.js renders
// the chosen frames. ffmpeg-based, no node deps.
//
//   node tools/play.js groovebox run --headless --frames 90 --dump /tmp/gb
//   node tools/store-contact.js --dir /tmp/gb --n 16 --out build/.contact/sheet.png
//   # read the sheet, note the tile numbers you like, then:
//   node tools/store-shots.js --in /tmp/gb/frame_00040.png --device iphone69 --caption "…"
//
// Each tile is labelled with its TILE index and the mapping table (printed) tells you which
// original frame file each tile is — so a pick maps straight back to a --in for store-shots.
'use strict'
const fs = require('fs')
const path = require('path')
const { execFileSync } = require('child_process')

const ROOT = path.join(__dirname, '..')
const argv = process.argv.slice(2)
const opt = { dir: '', n: '16', cols: '4', thumb: '320', out: 'build/.contact/sheet.png',
  font: 'editor/public/ComicMono-Bold.ttf' }
for (let i = 0; i < argv.length; i++) {
  const k = argv[i].replace(/^--/, '')
  if (argv[i].startsWith('--') && k in opt) opt[k] = argv[++i] ?? ''
  else { console.error(`unknown arg: ${argv[i]}\nusage: node tools/store-contact.js --dir <dump> [--n 16] [--cols 4] [--out sheet.png]`); process.exit(1) }
}
if (!opt.dir || !fs.existsSync(opt.dir)) { console.error(`--dir <dump dir of frame PNGs> required`); process.exit(1) }

const frames = fs.readdirSync(opt.dir).filter(f => /\.png$/i.test(f)).sort()
if (!frames.length) { console.error(`no PNGs in ${opt.dir}`); process.exit(1) }
const N = Math.min(parseInt(opt.n, 10) || 16, frames.length)

// pick N evenly-spaced frames across the clip (v0.2: fingerprint dedup / mpdecimate)
const idx = []
for (let i = 0; i < N; i++) {
  const j = N === 1 ? 0 : Math.round(i * (frames.length - 1) / (N - 1))
  if (!idx.includes(j)) idx.push(j)
}
const picked = idx.map(j => frames[j])

// stage the picked frames in sequence order so ffmpeg's %{n} == tile index
const stage = path.resolve(ROOT, 'build/.contact/.seq')
fs.rmSync(stage, { recursive: true, force: true })
fs.mkdirSync(stage, { recursive: true })
picked.forEach((f, i) => fs.copyFileSync(path.join(opt.dir, f), path.join(stage, `f${String(i).padStart(4, '0')}.png`)))

const cols = parseInt(opt.cols, 10) || 4
const rows = Math.ceil(picked.length / cols)
const tw = parseInt(opt.thumb, 10) || 320
const fontAbs = path.resolve(ROOT, opt.font)
const outAbs = path.resolve(ROOT, opt.out)
fs.mkdirSync(path.dirname(outAbs), { recursive: true })

const label = `drawtext=fontfile=${fontAbs}:text='%{n}':fontcolor=yellow:fontsize=${Math.round(tw * 0.13)}`
  + `:box=1:boxcolor=black@0.6:boxborderw=6:x=8:y=8`
const vf = `scale=${tw}:-1:flags=neighbor,${label},tile=${cols}x${rows}:padding=10:margin=10:color=0x1a1a1a`
execFileSync('ffmpeg', ['-y', '-loglevel', 'error', '-i', path.join(stage, 'f%04d.png'), '-vf', vf, outAbs])

console.log(`\ncontact sheet → ${path.relative(ROOT, outAbs)}  (${picked.length} of ${frames.length} frames, ${cols}×${rows})\n`)
console.log('tile → original frame:')
picked.forEach((f, i) => console.log(`  ${String(i).padStart(2)}  ${path.join(opt.dir, f)}`))
console.log(`\nRead the sheet, then: store-shots.js --in <the frame for your chosen tile> …\n`)
