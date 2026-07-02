#!/usr/bin/env node
// play.js — the "play together" debug harness driver for dreamengine carts.
//
// Compiles a cart from its source-of-truth (tools/carts/<name>.c + optional
// .cart.js) WITH -DDE_TRACE so its watch() instrumentation feeds the trace, then
// runs the native runtime in one of the harness modes (see runtime/studio.c):
//
// BINARY PATH (matters for profiling/sampling): play.js builds + runs
//   build/<name>-dbg  — NOT build/cart. build/cart is the EDITOR's ▶ binary and is
//   usually stale relative to play.js. To `sample`/inspect a play.js cart, target
//   build/<name>-dbg. (Profiling how-to: docs/guides/profiler.md → "Manual sample".)
//
//   node tools/play.js <name> run                 windowed live play
//   node tools/play.js <name> record <out.rec>    play live, log inputs for replay
//   node tools/play.js <name> replay <in.rec>     replay a recording deterministically
//   node tools/play.js <name> beats  <in.beats>   compile a beat-script + run it (Mode 2)
//   node tools/play.js <name> script <in.script>  run a raw frame-script as-is
//
// Common options (all optional):
//   --trace <file>     write per-frame JSONL state  (default: build/<name>.trace.jsonl)
//   --frames <n>       stop after n frames (headless runs should set this)
//   --dump <dir>       export a screenshot filmstrip
//   --dump-every <n>   ... every n frames (default 1 when --dump is given)
//   --headless         hidden window (for batch replay/script runs)
//   --seed <n>         RNG seed for deterministic runs (default 1)
//   --wav <file>       render the audio to a WAV (deterministic: same script +
//                      seed + frames = identical bytes; analyze with
//                      tools/wav-analyze.js — see docs/guides/debug-harness.md)
//   --bpm <n>          tempo used to convert `beat` directives to frames (default 120)
//   --screen WxH       screen dims (default from cart settings / 320x200)
//
// `beats` script format (compiled here to the runtime's frame events):
//   bpm 96                tempo for beat->frame conversion (overrides --bpm)
//   start 2               press SPACE at this frame; beat 0 origin lives here (default 2)
//   beat 20 tap S 3       at beat 20 press S, release 3 beats later (a hold note)
//   beat 4  tap A         at beat 4 tap A briefly (default 0.12 beat)
//   frame 900 down L      raw frame events still work and pass straight through
//   # comments and blank lines are ignored
//
// `script` directives (raw frame events; the runtime's load_script — see studio.c):
//   down/up <frame> <key>          key press / release
//   tap     <frame> <key> [dur]    press, release dur frames later (default 6)
//   move    <frame> <x> <y>        move the pointer to a canvas pixel
//   click   <frame> <x> <y> [btn]  move there + click  (btn 1=right, 2=mid; default left)
//   wheel   <frame> <delta>        scroll the wheel <delta> ticks on that frame (+up / -down)
//   <key> = a single char (a, z, 1) or a name (SPACE ENTER LEFT RIGHT UP DOWN TAB BACKSPACE)
//   self-describing meta as `# frames/fps/scale/crf N` lines (make-gif reads these).
//   MOUSE IS SCRIPTABLE (move/click) — clicks at KNOWN spots (menu buttons, a fixed
//   target) are reliable; live-aiming a MOVING target blind misses, so use `record`
//   (a .rec) for that. GOTCHA: dismiss a cart's TITLE screen (ENTER/Z) before its
//   gameplay keys register. Full reel-making runbook: docs/guides/debug-harness.md.

const fs   = require('fs')
const path = require('path')
const { execSync, spawnSync } = require('child_process')
const mk = require('./make-cart.js')

const args = process.argv.slice(2)
const name = args[0]
const mode = args[1]
const modeFile = (args[2] && !args[2].startsWith('--')) ? args[2] : null

if (!name || !mode) {
  console.error('usage: node tools/play.js <name> <run|record|replay|beats|script> [file] [options]')
  process.exit(1)
}

function opt(flag, def) {
  const i = args.indexOf(flag)
  return i >= 0 && i + 1 < args.length ? args[i + 1] : def
}
const hasFlag = (flag) => args.includes(flag)

const SRC = path.join(mk.ROOT_DIR, 'tools', 'carts', `${name}.c`)
if (!fs.existsSync(SRC)) { console.error('no cart source at', SRC); process.exit(1) }

// ── build the instrumented binary ────────────────────────────
const cfg = mk.loadConfig(SRC)
let SW = cfg.screenW ?? 320, SH = cfg.screenH ?? 200
const screenOpt = opt('--screen', null)
if (screenOpt && /^\d+x\d+$/.test(screenOpt)) { const [w, h] = screenOpt.split('x'); SW = +w; SH = +h }
const CW = cfg.cellW ?? 16, CH = cfg.cellH ?? 16, MW = cfg.mapW ?? 128, MH = cfg.mapH ?? 64

fs.mkdirSync(mk.BUILD_DIR, { recursive: true })
fs.copyFileSync(SRC, path.join(mk.BUILD_DIR, 'cart.c'))
const spritesBuf = cfg.sprites ? mk.buildSpriteSheet(cfg.sprites, cfg.charMap) : mk.makeBlankSpritePng()
fs.writeFileSync(path.join(mk.BUILD_DIR, 'sprites.png'), spritesBuf)
const mapBytes = cfg.map ? mk.buildMap(cfg.map.layout || cfg.map, cfg.map.tiles, MW, MH) : new Uint8Array(8192)
fs.writeFileSync(path.join(mk.BUILD_DIR, 'map.dat'), Buffer.from(mapBytes))

const xxd = (file) => execSync(`xxd -i ${file}`, { cwd: mk.BUILD_DIR }).toString()
fs.writeFileSync(path.join(mk.BUILD_DIR, 'sprites_data.h'),
  xxd('sprites.png')
    .replace(/unsigned char sprites_png\[\]/, 'static const unsigned char SPRITES_DATA[]')
    .replace(/unsigned int sprites_png_len/,  'static const unsigned int  SPRITES_DATA_LEN'))
fs.writeFileSync(path.join(mk.BUILD_DIR, 'map_data.h'),
  xxd('map.dat')
    .replace(/unsigned char map_dat\[\]/, 'static const unsigned char MAP_DATA[]')
    .replace(/unsigned int map_dat_len/,  'static const unsigned int  MAP_DATA_LEN'))

const BIN = path.join(mk.BUILD_DIR, `${name}-dbg`)
const clangArgs = [
  `"${path.join(mk.BUILD_DIR, 'cart.c')}"`, `"${path.join(mk.RUNTIME_DIR, 'studio.c')}"`,
  `-I"${mk.RUNTIME_DIR}"`, `-I"${mk.BUILD_DIR}"`, `-I"${mk.RAYLIB}/include"`,
  `-DSCREEN_W=${SW}`, `-DSCREEN_H=${SH}`, '-DSCALE=2',
  `-DMAP_W=${MW}`, `-DMAP_H=${MH}`, `-DCELL_W=${CW}`, `-DCELL_H=${CH}`,
  '-DTOUCH_CONTROLS_DEFAULT=0', '-DDE_TRACE', '-Os', '-fno-delete-null-pointer-checks',
  // dev pass-through: DE_DEFINES=FOO,BAR=1 → -DFOO -DBAR=1 (e.g. DE_DEFINES=DE_FIELD_ROADS)
  ...(process.env.DE_DEFINES ? process.env.DE_DEFINES.split(',').filter(Boolean).map(d => '-D' + d) : []),
  `"${mk.RAYLIB}/lib/libraylib.a"`,
  '-framework OpenGL', '-framework Cocoa', '-framework IOKit',
  '-framework CoreVideo', '-framework CoreFoundation', '-framework CoreMIDI',
  '-Wl,-dead_strip', `-o "${BIN}"`,
].join(' ')

process.stdout.write('compiling (instrumented)... ')
try { execSync(`clang ${clangArgs}`, { stdio: 'pipe' }); console.log('ok') }
catch (e) { console.error('failed\n' + (e.stderr?.toString() || e.message)); process.exit(1) }

// ── compile a beat-script to the runtime's frame-event format ─
function compileBeats(text, cliBpm) {
  let bpm = cliBpm, origin = 2, events = []
  const keyTok = (k) => (k === 'SPACE' || k.length === 1) ? k : k.toUpperCase()
  for (const raw of text.split('\n')) {
    const line = raw.trim()
    if (!line || line[0] === '#') continue
    const t = line.split(/\s+/)
    if (t[0] === 'bpm')        { bpm = +t[1] }
    else if (t[0] === 'start') { origin = +t[1]; events.push(`down ${origin} SPACE`, `up ${origin + 2} SPACE`) }
    else if (t[0] === 'frame') { // frame <N> <down|up|tap> <key> [dur]  ->  <cmd> <N> <key> [dur]
      const [, f, cmd, key, dur] = t
      events.push(`${cmd} ${f} ${keyTok(key)}${dur !== undefined ? ' ' + dur : ''}`)
    }
    else if (t[0] === 'beat')  {
      const fpb = 3600 / bpm
      const b = parseFloat(t[1]), cmd = t[2], key = keyTok(t[3])
      const f = origin + Math.round(b * fpb)
      if (cmd === 'tap') {
        const holdBeats = t[4] !== undefined ? parseFloat(t[4]) : 0.12
        events.push(`down ${f} ${key}`, `up ${f + Math.max(1, Math.round(holdBeats * fpb))} ${key}`)
      } else if (cmd === 'down' || cmd === 'up') {
        events.push(`${cmd} ${f} ${key}`)
      }
    }
  }
  return events.join('\n') + '\n'
}

// ── run ───────────────────────────────────────────────────────
// resolve against the repo cwd — the binary runs with cwd=build/, so a relative
// path would otherwise land under build/build/.
const tracePath = path.resolve(opt('--trace', path.join(mk.BUILD_DIR, `${name}.trace.jsonl`)))
const runArgs = []
if (mode === 'record') {
  if (!modeFile) { console.error('record needs an output file'); process.exit(1) }
  runArgs.push('--record', path.resolve(modeFile))
} else if (mode === 'replay') {
  if (!modeFile) { console.error('replay needs a .rec file'); process.exit(1) }
  runArgs.push('--replay', path.resolve(modeFile))
} else if (mode === 'beats') {
  if (!modeFile) { console.error('beats needs a .beats file'); process.exit(1) }
  const compiled = compileBeats(fs.readFileSync(modeFile, 'utf8'), +opt('--bpm', 120))
  const out = path.join(mk.BUILD_DIR, `${name}.compiled.script`)
  fs.writeFileSync(out, compiled)
  console.log(`compiled ${modeFile} -> ${out}:\n` + compiled.replace(/^/gm, '  '))
  runArgs.push('--script', out)
} else if (mode === 'script') {
  if (!modeFile) { console.error('script needs a .script file'); process.exit(1) }
  runArgs.push('--script', path.resolve(modeFile))
} else if (mode !== 'run') {
  console.error('unknown mode:', mode); process.exit(1)
}

runArgs.push('--trace', tracePath)
runArgs.push('--save-dir', `saves/${name}`)   // per-cart saves, like the editor (relative to cwd=build/)
if (hasFlag('--headless'))     runArgs.push('--headless')
if (opt('--frames', null))     runArgs.push('--frames', opt('--frames'))
if (opt('--seed', null))       runArgs.push('--seed', opt('--seed'))
if (opt('--dump', null))     { runArgs.push('--dump', path.resolve(opt('--dump'))); fs.mkdirSync(opt('--dump'), { recursive: true }) }
if (opt('--dump-every', null)) runArgs.push('--dump-every', opt('--dump-every'))
if (opt('--wav', null))        runArgs.push('--wav', path.resolve(opt('--wav')))   // deterministic audio render → WAV
if (opt('--uiaudit', null))    runArgs.push('--uiaudit', path.resolve(opt('--uiaudit')))   // per-frame draw bounding boxes → JSONL (tools/ui-audit.js)

console.log('run:', name, mode, runArgs.join(' '))
// darwin: run under `caffeinate -dims` — a SLEEPING DISPLAY segfaults raylib's window init
// (even --headless), which silently killed every unattended night run (bit 2026-07-02)
if (process.platform === 'darwin') spawnSync('caffeinate', ['-dims', BIN, ...runArgs], { cwd: mk.BUILD_DIR, stdio: 'inherit' })
else                               spawnSync(BIN, runArgs, { cwd: mk.BUILD_DIR, stdio: 'inherit' })
console.log('trace:', tracePath)
