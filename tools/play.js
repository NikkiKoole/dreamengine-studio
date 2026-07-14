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
//   node tools/play.js <name> netdemo [script]    LOCKSTEP NETPLAY demo/check: spawn a host +
//                      joiner pair over UDP loopback (rung 1 — docs/design/multiplayer-research.md),
//                      side-by-side windows, arrows/WASD move YOUR player in the focused window.
//                      Optional [script] drives BOTH sides; --host-script/--join-script <f> drive
//                      each side differently (the strong lockstep test: different inputs, same sim).
//                      With --frames + traces it ends with a LOCKSTEP OK / DESYNC verdict by
//                      diffing the two sides' per-frame traces. --port <n> picks the UDP port.
//
// Common options (all optional):
//   --trace <file>     write per-frame JSONL state  (default: build/<name>.trace.jsonl)
//   --frames <n>       stop after n frames (headless runs should set this)
//   --dump <dir>       export a screenshot filmstrip
//   --dump-every <n>   ... every n frames (default 1 when --dump is given)
//   --show-size        live "WxH  win WxH" overlay top-left (resizable carts; sets DE_SHOW_SIZE)
//   --resize WxH,WxH,… canvas-size SWEEP for device-adaptive layouts: hold each size a few
//                      frames (reflow settles), dump ONE PNG cropped to the active region per
//                      size (resize_NN_WxH.png). Implies -DDE_RESIZABLE; auto-dumps to
//                      build/.resize/<cart> if no --dump. The "resize → look → resize → look" test.
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
//   press   <frame> <x> <y> [btn]  button DOWN and HELD (pair with a later `release`)
//   release <frame> <x> <y> [btn]  button up — ends a held press
//   drag <f0> <x0> <y0> <f1> <x1> <y1> [btn]   press → glide (one move/frame) → release
//   wheel   <frame> <delta>        scroll the wheel <delta> ticks on that frame (+up / -down)
//   <key> = a single char (a, z, 1) or a name (SPACE ENTER LEFT RIGHT UP DOWN TAB BACKSPACE)
//   self-describing meta as `# frames/fps/scale/crf N` lines (make-gif reads these).
//   MOUSE IS SCRIPTABLE — click at KNOWN spots; press/release/drag give you HELD
//   presses so knob / slider / paint DRAGS are replayable (a held target, not a
//   moving one). Live-aiming a moving target blind still misses; use `record`
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

// device-adaptive-layout.md: a cart with "resizable": true in its de:meta compiles with
// -DDE_RESIZABLE (parity with the editor ▶-run). A --resize sweep implies it too — resizing
// only reflows a resizable build. Parsed with build-cart-index.js's canonical de:meta regex.
const resizeSpec = opt('--resize', null)
const cartResizable = (() => {
  try { const m = fs.readFileSync(SRC, 'utf8').match(/\/\*\s*de:meta\s*\n([\s\S]*?)\nde:meta\s*\*\//); return m ? JSON.parse(m[1]).resizable === true : false }
  catch { return false }
})()
const alreadyDef = (process.env.DE_DEFINES || '').split(',').includes('DE_RESIZABLE')
const wantResizable = cartResizable || !!resizeSpec || alreadyDef

const BIN = path.join(mk.BUILD_DIR, `${name}-dbg`)
const clangArgs = [
  `"${path.join(mk.BUILD_DIR, 'cart.c')}"`, `"${path.join(mk.RUNTIME_DIR, 'studio.c')}"`,
  `-I"${mk.RUNTIME_DIR}"`, `-I"${mk.BUILD_DIR}"`, `-I"${mk.RAYLIB}/include"`,
  `-DSCREEN_W=${SW}`, `-DSCREEN_H=${SH}`, '-DSCALE=2',
  `-DMAP_W=${MW}`, `-DMAP_H=${MH}`, `-DCELL_W=${CW}`, `-DCELL_H=${CH}`,
  '-DTOUCH_CONTROLS_DEFAULT=0', '-DDE_TRACE', '-Os', '-fno-delete-null-pointer-checks',
  // dev pass-through: DE_DEFINES=FOO,BAR=1 → -DFOO -DBAR=1 (e.g. DE_DEFINES=DE_FIELD_ROADS)
  ...(process.env.DE_DEFINES ? process.env.DE_DEFINES.split(',').filter(Boolean).map(d => '-D' + d) : []),
  ...(wantResizable && !alreadyDef ? ['-DDE_RESIZABLE'] : []),   // resizable cart / --resize sweep → live-reflow build
  ...mk.box2dArgs(fs.readFileSync(path.join(mk.BUILD_DIR, 'cart.c'), 'utf8')),   // opt-in: cart #includes box2d/box2d.h
  `"${mk.RAYLIB}/lib/libraylib.a"`,
  '-framework OpenGL', '-framework Cocoa', '-framework IOKit',
  '-framework CoreVideo', '-framework CoreFoundation', '-framework CoreMIDI',
  '-Wl,-dead_strip', `-o "${BIN}"`,
].join(' ')

process.stdout.write('compiling (instrumented)... ')
try { execSync(`clang ${clangArgs}`, { stdio: 'pipe' }); console.log('ok') }
catch (e) { console.error('failed\n' + (e.stderr?.toString() || e.message)); process.exit(1) }

// ── netdemo: a host + joiner pair over UDP loopback ──────────
// Two instances of the same binary in lockstep (runtime/net.h). Exits nonzero
// on DESYNC — with --frames + a script this is the deterministic netplay gate.
if (mode === 'netdemo') {
  const { spawn } = require('child_process')
  const port     = opt('--port', '33445')
  const frames   = opt('--frames', null)
  const seed     = opt('--seed', null)
  const headless = hasFlag('--headless')
  const script     = modeFile ? path.resolve(modeFile) : null
  const hostScript = opt('--host-script', null) ? path.resolve(opt('--host-script')) : null
  const joinScript = opt('--join-script', null) ? path.resolve(opt('--join-script')) : null

  const traceOf = (role) => path.resolve(path.join(mk.BUILD_DIR, `${name}.${role}.trace.jsonl`))
  const argsFor = (role) => {
    const a = role === 'host' ? ['--net-host'] : ['--net-join', '127.0.0.1']
    a.push('--net-port', port, '--trace', traceOf(role), '--save-dir', `saves/${name}`)
    const s = role === 'host' ? (hostScript || script) : (joinScript || script)
    if (s)        a.push('--script', s)
    if (frames)   a.push('--frames', frames)
    if (seed && role === 'host') a.push('--seed', seed)   // joiner adopts it via the handshake
    if (headless) a.push('--headless')
    return a
  }
  const launch = (role, tag) => {
    const a = argsFor(role)
    const [cmd, cargs] = process.platform === 'darwin' ? ['caffeinate', ['-dims', BIN, ...a]] : [BIN, a]
    const c = spawn(cmd, cargs, { cwd: mk.BUILD_DIR })
    const pipe = (s, out) => s.on('data', d =>
      out.write(String(d).split('\n').filter(Boolean).map(l => `[${tag}] ${l}`).join('\n') + '\n'))
    pipe(c.stdout, process.stdout); pipe(c.stderr, process.stderr)
    return new Promise(res => c.on('exit', code => res(code)))
  }
  console.log(`netdemo: host + joiner over 127.0.0.1:${port}`)
  Promise.all([launch('host', 'P1'), launch('join', 'P2')]).then(([h, j]) => {
    console.log(`netdemo: host exit ${h ?? '?'}, joiner exit ${j ?? '?'}`)
    // lockstep verdict: both sims are (supposed to be) identical, so the two
    // traces must match line for line — first differing line = desync frame
    try {
      // drop the per-peer "net":{...} diagnostics block — it legitimately differs
      // between host and joiner (each side's own buffer/tx/rx). Desync = the SIM
      // state (f/beat/bpos + the watch "w" object) diverging, nothing else.
      const noNet = s => s.replace(/"net":\{[^}]*\},/, '')
      const A = fs.readFileSync(traceOf('host'), 'utf8').trim().split('\n').map(noNet)
      const B = fs.readFileSync(traceOf('join'), 'utf8').trim().split('\n').map(noNet)
      const n = Math.min(A.length, B.length)
      let bad = -1
      for (let i = 0; i < n; i++) if (A[i] !== B[i]) { bad = i; break }
      if (bad >= 0) {
        console.log(`netdemo: DESYNC at trace line ${bad}:\n  [P1] ${A[bad]}\n  [P2] ${B[bad]}`)
        process.exitCode = 1
      } else {
        console.log(`netdemo: LOCKSTEP OK — ${n} traced frames identical`
          + (A.length !== B.length ? ` (lengths ${A.length}/${B.length}: one side stepped a bit further before exiting; normal)` : ''))
      }
    } catch { console.log('netdemo: no traces to compare (unbounded run?)') }
  })
  return   // async section owns the rest of this run
}

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
let saveDirRel = `saves/${name}`   // per-cart live saves; a replay with a start-state sidecar overrides this
if (mode === 'record') {
  if (!modeFile) { console.error('record needs an output file'); process.exit(1) }
  const recAbs = path.resolve(modeFile)
  runArgs.push('--record', recAbs)
  // Record deterministically so replay can reconstruct the SAME world. A .rec
  // stores only inputs, not the RNG stream — replay implies --det (seeded RNG +
  // fixed timestep), so a live (unseeded, wall-clock) recording replays into a
  // DIFFERENT world and diverges (bit flank: won live, died on replay). --seed
  // passes through below and must match the replay's seed (both default 1).
  runArgs.push('--det')
  // But the seed doesn't cover a SAVE-BOOTING cart's state (acidrack loads its blob at boot).
  // Snapshot the start-state beside the .rec (<stem>.save/) so the take is self-contained; skipped
  // when the save-dir is empty/absent (most carts never save). Replay restores it into an isolated dir.
  try {
    const src = path.join(mk.BUILD_DIR, 'saves', name)
    const files = fs.existsSync(src) ? fs.readdirSync(src).filter(f => !f.startsWith('.')) : []
    if (files.length) {
      const dest = recAbs.replace(/\.rec$/, '.save')
      fs.rmSync(dest, { recursive: true, force: true }); fs.mkdirSync(dest, { recursive: true })
      for (const f of files) fs.copyFileSync(path.join(src, f), path.join(dest, f))
      console.log(`start-state snapshot → ${path.relative(process.cwd(), dest)} (self-contained take)`)
    }
  } catch {}
} else if (mode === 'replay') {
  if (!modeFile) { console.error('replay needs a .rec file'); process.exit(1) }
  const repAbs = path.resolve(modeFile)
  runArgs.push('--replay', repAbs)
  // If the take carries a start-state sidecar, boot from it in a THROWAWAY save-dir so the replay
  // reproduces the recorded run AND never clobbers the live build/saves/<name>/ (it used to).
  try {
    const sidecar = repAbs.replace(/\.(rec|script)$/, '.save')
    if (fs.existsSync(sidecar)) {
      const rel = path.join('.replay', name)
      const iso = path.join(mk.BUILD_DIR, rel)
      fs.rmSync(iso, { recursive: true, force: true }); fs.mkdirSync(iso, { recursive: true })
      for (const f of fs.readdirSync(sidecar)) fs.copyFileSync(path.join(sidecar, f), path.join(iso, f))
      saveDirRel = rel
      console.log(`replay start-state ← ${path.relative(process.cwd(), sidecar)} (isolated save-dir; live saves untouched)`)
    }
  } catch {}
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
runArgs.push('--save-dir', saveDirRel)   // per-cart saves, like the editor (relative to cwd=build/); replay w/ sidecar → isolated
if (hasFlag('--headless'))     runArgs.push('--headless')
if (opt('--frames', null))     runArgs.push('--frames', opt('--frames'))
if (opt('--seed', null))       runArgs.push('--seed', opt('--seed'))
if (opt('--dump', null))     { runArgs.push('--dump', path.resolve(opt('--dump'))); fs.mkdirSync(opt('--dump'), { recursive: true }) }
if (opt('--dump-every', null)) runArgs.push('--dump-every', opt('--dump-every'))
if (resizeSpec) {                                 // --resize "WxH,WxH,…": sweep sizes, one labeled PNG each
  runArgs.push('--resize', resizeSpec)
  if (!opt('--dump', null)) {                     // default a dump dir so the captures land somewhere
    const rd = path.join(mk.BUILD_DIR, '.resize', name)
    fs.mkdirSync(rd, { recursive: true }); runArgs.push('--dump', rd)
    console.log('resize captures →', rd)
  }
}
if (hasFlag('--net-echo'))     runArgs.push('--net-echo')   // lockstep vs the loopback fake peer (P2 mirrors P1; implies --det)
if (opt('--wav', null))        runArgs.push('--wav', path.resolve(opt('--wav')))   // deterministic audio render → WAV
if (opt('--solo-slot', null))  runArgs.push('--solo-slot', opt('--solo-slot'))   // stem render: mute all but these instrument slot(s), e.g. 6 or 5,6 (docs/design/audio-voice-debugging.md)
if (opt('--uiaudit', null))    runArgs.push('--uiaudit', path.resolve(opt('--uiaudit')))   // per-frame draw bounding boxes → JSONL (tools/ui-audit.js)

if (hasFlag('--show-size')) process.env.DE_SHOW_SIZE = '1'   // live WxH overlay, top-left (resizable carts only)

console.log('run:', name, mode, runArgs.join(' '))
// darwin: run under `caffeinate -dims` — a SLEEPING DISPLAY segfaults raylib's window init
// (even --headless), which silently killed every unattended night run (bit 2026-07-02)
if (process.platform === 'darwin') spawnSync('caffeinate', ['-dims', BIN, ...runArgs], { cwd: mk.BUILD_DIR, stdio: 'inherit' })
else                               spawnSync(BIN, runArgs, { cwd: mk.BUILD_DIR, stdio: 'inherit' })
console.log('trace:', tracePath)
