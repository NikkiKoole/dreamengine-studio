#!/usr/bin/env node
// make-gif.js — capture an animated clip (WebM / WebP / GIF / MP4 / APNG) of a cart.
//
// Built on the debug harness: it runs the cart headless under play.js with
// --dump (which exports the NATIVE-resolution canvas as one PNG per frame — the
// real pixels, BEFORE the on-screen window scale) then assembles them with
// ffmpeg. Because the harness is deterministic, a clip is a reproducible BUILD
// ARTIFACT of a committed input track — re-run it after an engine change and get
// the identical animation. Pixel art stays crisp: any --scale upscale is
// NEAREST-NEIGHBOUR, GIF uses a global palette with no dither, WebM is VP9 at
// yuv444p (full chroma, no colour bleed on hard edges).
//
//   node tools/make-gif.js <name>                       # self-animating cart, no input
//   node tools/make-gif.js <name> --from demo.rec       # replay a recording (you driving)
//   node tools/make-gif.js <name> --from solo.beats     # author inputs in beats
//
//   # the reproducible path — a committed demo track under tools/clips/<name>/:
//   node tools/make-gif.js sloop --recipe 01-autodrive
//     reads  tools/clips/sloop/01-autodrive.script   (.script | .beats | .rec)
//     bakes  editor/public/clips/sloop/01-autodrive.webm   (the history-page convention)
//
//   node tools/make-gif.js --all                        # rebuild EVERY clip from its recipe
//
// Options:
//   --frames <n>    cart frames to run   (recipe `# frames N`, else .script max+tail, else 180)
//   --fps <n>       output framerate     (default 30; cart runs at 60, dumps every 2nd)
//   --scale <n>     integer upscale, nearest  (default 1 = native; the web page upscales
//                                              crisply via image-rendering:pixelated. Bump to
//                                              2–3 for a standalone GIF/README where nothing
//                                              else scales it.)
//   --crf <n>       webm/mp4 quality, lower=bigger+cleaner  (default 30; ~16 near-lossless
//                                              for a hero clip, ~40 for max-tiny grid loops)
//   --start <n>     drop the first n cart-frames (skip boot/title)
//   --format <fmt>  webm | webp | gif | mp4 | apng   (default webm)
//   --clip <label>  file into editor/public/clips/<name>/NN-label.<ext>, auto-numbering NN
//                   (a bare "NN-label" is verbatim). --recipe implies this, verbatim.
//   --out <path>    explicit output (overrides --clip/--recipe; default docs/media/<name>.<ext>)
//   --seed <n> --bpm <n> --screen WxH    passthrough to play.js
//   --keep          keep the temp frame dir (build/.gif/<name>/) for inspection
//
// A recipe / --from file may carry self-describing metadata as `# key value` comment
// lines (harmless to the runtime script parser): `# frames 490`, `# fps 30`, `# scale 1`,
// `# crf 30`. A CLI flag always overrides the recipe's value. This is what lets `--all`
// rebuild every clip at its own length with no per-cart bookkeeping.

const fs   = require('fs')
const path = require('path')
const { spawnSync } = require('child_process')
const mk = require('./make-cart.js')

const args = process.argv.slice(2)
const opt = (flag, def) => { const i = args.indexOf(flag); return i >= 0 && i + 1 < args.length ? args[i + 1] : def }
const hasFlag = (f) => args.includes(f)

const CLIPS_RECIPES = path.join(mk.ROOT_DIR, 'tools', 'clips')   // committed demo tracks
const CLIPS_OUT     = path.join(mk.ROOT_DIR, 'editor', 'public', 'clips')   // served webm/gif
const RECIPE_EXTS   = ['script', 'beats', 'rec']
const MODE_BY_EXT   = { '.rec': 'replay', '.beats': 'beats', '.script': 'script' }
const EXT = { webm: 'webm', webp: 'webp', gif: 'gif', mp4: 'mp4', apng: 'png' }

// ── --all: rebuild every committed recipe (spawns self per recipe so each cart
//    compiles fresh and picks up its own `# frames`) ───────────────────────────
if (args[0] === '--all') {
  if (!fs.existsSync(CLIPS_RECIPES)) { console.error('no tools/clips/ recipes yet'); process.exit(1) }
  const pass = []   // forward only the global knobs the user set; length comes from each recipe
  for (const f of ['--format', '--scale', '--crf', '--fps']) if (opt(f, null)) pass.push(f, opt(f, null))
  let n = 0, fail = 0
  for (const cart of fs.readdirSync(CLIPS_RECIPES).sort()) {
    const cdir = path.join(CLIPS_RECIPES, cart)
    if (!fs.statSync(cdir).isDirectory()) continue
    for (const rec of fs.readdirSync(cdir).sort()) {
      const m = rec.match(/^(.+)\.(script|beats|rec)$/); if (!m) continue
      console.log(`\n=== ${cart} / ${m[1]} ===`)
      const rr = spawnSync('node', [__filename, cart, '--recipe', m[1], ...pass], { stdio: 'inherit' })
      if (rr.status !== 0) { console.error(`✗ failed: ${cart}/${m[1]}`); fail++ } else n++
    }
  }
  console.log(`\n${fail ? '⚠' : '✓'} rebuilt ${n} clip(s)${fail ? `, ${fail} failed` : ''} from tools/clips/`)
  process.exit(fail ? 1 : 0)
}

const name = args[0]
if (!name || name.startsWith('--')) {
  console.error('usage: node tools/make-gif.js <name> [--recipe <label> | --from <file>] [options]')
  console.error('       node tools/make-gif.js --all')
  process.exit(1)
}
const SRC = path.join(mk.ROOT_DIR, 'tools', 'carts', `${name}.c`)
if (!fs.existsSync(SRC)) { console.error('no cart source at', SRC); process.exit(1) }

// ── resolve the input track (--recipe → --from) ───────────────
let from = opt('--from', null)
let clipLabel = opt('--clip', null)
let clipVerbatim = false
const recipe = opt('--recipe', null)
if (recipe) {
  const found = RECIPE_EXTS.map(e => path.join(CLIPS_RECIPES, name, `${recipe}.${e}`)).find(p => fs.existsSync(p))
  if (!found) { console.error(`no recipe at tools/clips/${name}/${recipe}.{${RECIPE_EXTS.join(',')}}`); process.exit(1) }
  from = found
  clipLabel = recipe        // the recipe filename IS the canonical output name
  clipVerbatim = true
}

let playMode = 'run', playFile = null
if (from) {
  playMode = MODE_BY_EXT[path.extname(from)]
  if (!playMode) { console.error('input must be .rec / .beats / .script, got:', path.extname(from)); process.exit(1) }
  playFile = path.resolve(from)
  if (!fs.existsSync(playFile)) { console.error('no such file:', playFile); process.exit(1) }
}

// ── self-describing recipe metadata (`# key value`) ───────────
const meta = {}
if (playFile && path.extname(playFile) !== '.rec') {
  let maxFrame = 0
  for (const raw of fs.readFileSync(playFile, 'utf8').split('\n')) {
    const mm = raw.match(/^#\s*(frames|fps|scale|crf)\s+(\d+)/)
    if (mm) { meta[mm[1]] = +mm[2]; continue }
    const fe = raw.match(/^\s*(?:down|up|tap)\s+(\d+)/)   // a .script event frame
    if (fe) maxFrame = Math.max(maxFrame, +fe[1])
  }
  if (meta.frames === undefined && maxFrame > 0) meta.frames = maxFrame + 30   // auto-length + tail
}

// CLI flag > recipe meta > default
const num = (flag, key, def) => opt(flag, null) !== null ? +opt(flag, null) : (meta[key] ?? def)
const frames = num('--frames', 'frames', 180)
const fps    = num('--fps', 'fps', 30)
const scale  = num('--scale', 'scale', 1)
const crf    = num('--crf', 'crf', 30)
const start  = +opt('--start', 0)
const format = opt('--format', 'webm')
const every  = Math.max(1, Math.round(60 / fps))   // cart is 60fps → dump every Nth for target fps
if (!EXT[format]) { console.error('unknown --format:', format, `(${Object.keys(EXT).join('|')})`); process.exit(1) }

// ── output path: --out wins; else --clip/--recipe → the convention; else docs/media/ ──
function resolveOut() {
  if (opt('--out', null)) return path.resolve(opt('--out'))
  if (clipLabel) {
    const dir = path.join(CLIPS_OUT, name)
    fs.mkdirSync(dir, { recursive: true })
    let label = clipLabel
    if (!clipVerbatim && !/^\d+-/.test(label)) {        // auto-assign next NN-
      const used = fs.readdirSync(dir).map(f => +(f.match(/^(\d+)-/)?.[1] ?? -1))
      label = `${String((used.length ? Math.max(...used) : 0) + 1).padStart(2, '0')}-${label}`
    }
    return path.join(dir, `${label}.${EXT[format]}`)
  }
  return path.join(mk.ROOT_DIR, 'docs', 'media', `${name}.${EXT[format]}`)
}
const out = resolveOut()

// ── 1. dump frames via play.js (headless) ─────────────────────
const dumpDir = path.join(mk.BUILD_DIR, '.gif', name)
fs.rmSync(dumpDir, { recursive: true, force: true })
fs.mkdirSync(dumpDir, { recursive: true })

const playArgs = [path.join(__dirname, 'play.js'), name, playMode]
if (playFile) playArgs.push(playFile)
playArgs.push('--headless', '--frames', String(frames), '--dump', dumpDir, '--dump-every', String(every))
for (const f of ['--seed', '--bpm', '--screen']) if (opt(f, null)) playArgs.push(f, opt(f, null))

console.log(`\n[1/3] dumping ${frames} frames (every ${every} → ${fps}fps) of '${name}' ${from ? `from ${path.basename(from)}` : '(headless run)'}…`)
if (spawnSync('node', playArgs, { stdio: 'inherit' }).status !== 0) { console.error('frame dump failed'); process.exit(1) }

// ── 2. select + renumber frames into a contiguous %05d sequence ──
const seqDir = path.join(dumpDir, 'seq')
fs.mkdirSync(seqDir, { recursive: true })
const all = fs.readdirSync(dumpDir).filter(f => /^frame_\d+\.png$/.test(f))
  .map(f => ({ f, n: +f.match(/\d+/)[0] })).filter(o => o.n >= start).sort((a, b) => a.n - b.n)
if (all.length === 0) { console.error('no frames were dumped — does the cart run headless?'); process.exit(1) }
all.forEach((o, i) => fs.copyFileSync(path.join(dumpDir, o.f), path.join(seqDir, `f_${String(i).padStart(5, '0')}.png`)))
console.log(`[2/3] ${all.length} frames → encoding ${format} ${scale > 1 ? `at ${scale}× ` : '(native) '}${/webm|mp4/.test(format) ? `crf ${crf}` : ''}…`)

// ── 3. encode with ffmpeg ─────────────────────────────────────
fs.mkdirSync(path.dirname(out), { recursive: true })
const inPat = path.join(seqDir, 'f_%05d.png')
const up = scale > 1 ? `scale=iw*${scale}:ih*${scale}:flags=neighbor` : 'null'   // native = no scale filter
const ff = (a) => { const r = spawnSync('ffmpeg', a, { stdio: ['ignore', 'pipe', 'pipe'] })
  if (r.status !== 0) { console.error(r.stderr?.toString() || 'ffmpeg failed'); process.exit(1) } }

if (format === 'gif') {
  const pal = path.join(dumpDir, 'palette.png')
  ff(['-y', '-framerate', String(fps), '-i', inPat, '-vf', `${up},palettegen=max_colors=256:stats_mode=full`, pal])
  ff(['-y', '-framerate', String(fps), '-i', inPat, '-i', pal, '-lavfi', `${up}[x];[x][1:v]paletteuse=dither=none`, '-loop', '0', out])
} else if (format === 'webm') {
  ff(['-y', '-framerate', String(fps), '-i', inPat, '-vf', up, '-c:v', 'libvpx-vp9', '-pix_fmt', 'yuv444p', '-crf', String(crf), '-b:v', '0', '-an', out])
} else if (format === 'webp') {
  ff(['-y', '-framerate', String(fps), '-i', inPat, '-vf', up, '-c:v', 'libwebp', '-lossless', '1', '-q:v', '100', '-loop', '0', out])
} else if (format === 'apng') {
  ff(['-y', '-framerate', String(fps), '-i', inPat, '-vf', up, '-c:v', 'apng', '-plays', '0', out])
} else if (format === 'mp4') {
  ff(['-y', '-framerate', String(fps), '-i', inPat, '-vf', `${up},pad=ceil(iw/2)*2:ceil(ih/2)*2,format=yuv420p`, '-c:v', 'libx264', '-crf', String(crf), '-pix_fmt', 'yuv420p', '-movflags', '+faststart', out])
}

if (!hasFlag('--keep')) fs.rmSync(dumpDir, { recursive: true, force: true })
console.log(`[3/3] ✓ ${path.relative(mk.ROOT_DIR, out)}  (${(fs.statSync(out).size / 1024).toFixed(0)} KB, ${all.length} frames @ ${fps}fps${scale > 1 ? `, ${scale}×` : ', native'})`)
