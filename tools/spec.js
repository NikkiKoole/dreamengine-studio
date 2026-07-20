#!/usr/bin/env node
// tools/spec.js — run each cart's spec() (the gameplay twin of tune-check.js). For every cart whose
// source defines a non-empty `void spec(void)`, compile it + studio.c with -DDE_SPEC -DDE_TRACE, run
// the binary with --spec (headless: it calls spec() then prints JSONL pass/fail and exits), parse the
// results, and report. Carts WITHOUT a spec() are skipped (the weak stub), never failed.
//
//   node tools/spec.js                 # every cart that has a spec()
//   node tools/spec.js streetlab       # just one cart
//   node tools/spec.js --quiet         # CI: only failures + a summary; exit 1 if any assertion failed
//
// Sibling of build-all.js (compile-check) and the audio gates. Design: docs/design/spec-harness.md.
const fs = require('fs'), path = require('path')
const { execSync, spawnSync } = require('child_process')
const mk = require('./make-cart.js')

const args  = process.argv.slice(2)
const quiet = args.includes('--quiet')
const only  = args.find(a => !a.startsWith('--'))

const CARTS = path.join(mk.ROOT_DIR, 'tools', 'carts')
const hasSpec = (src) => /\bvoid\s+spec\s*\(\s*void\s*\)\s*\{/.test(fs.readFileSync(src, 'utf8'))

function build(name, work) {
  const SRC = path.join(CARTS, `${name}.c`)
  const cfg = mk.loadConfig(SRC)
  const SW = cfg.screenW ?? 320, SH = cfg.screenH ?? 200
  const CW = cfg.cellW ?? 16, CH = cfg.cellH ?? 16, MW = cfg.mapW ?? 128, MH = cfg.mapH ?? 64
  fs.mkdirSync(work, { recursive: true })
  fs.copyFileSync(SRC, path.join(work, 'cart.c'))
  const spritesBuf = cfg.sprites ? mk.buildSpriteSheet(cfg.sprites, cfg.charMap) : mk.makeBlankSpritePng()
  fs.writeFileSync(path.join(work, 'sprites.png'), spritesBuf)
  const mapBytes = cfg.map ? mk.buildMap(cfg.map.layout || cfg.map, cfg.map.tiles, MW, MH) : new Uint8Array(8192)
  fs.writeFileSync(path.join(work, 'map.dat'), Buffer.from(mapBytes))
  const xxd = (file) => execSync(`xxd -i ${file}`, { cwd: work }).toString()
  fs.writeFileSync(path.join(work, 'sprites_data.h'),
    xxd('sprites.png').replace(/unsigned char sprites_png\[\]/, 'static const unsigned char SPRITES_DATA[]')
      .replace(/unsigned int sprites_png_len/, 'static const unsigned int  SPRITES_DATA_LEN'))
  fs.writeFileSync(path.join(work, 'map_data.h'),
    xxd('map.dat').replace(/unsigned char map_dat\[\]/, 'static const unsigned char MAP_DATA[]')
      .replace(/unsigned int map_dat_len/, 'static const unsigned int  MAP_DATA_LEN'))
  const BIN = path.join(work, `${name}-spec`)
  const clangArgs = [
    `"${path.join(work, 'cart.c')}"`, `"${path.join(mk.RUNTIME_DIR, 'studio.c')}"`,
    `-I"${mk.RUNTIME_DIR}"`, `-I"${work}"`, `-I"${mk.RAYLIB}/include"`,
    `-DSCREEN_W=${SW}`, `-DSCREEN_H=${SH}`, '-DSCALE=2',
    `-DMAP_W=${MW}`, `-DMAP_H=${MH}`, `-DCELL_W=${CW}`, `-DCELL_H=${CH}`,
    '-DTOUCH_CONTROLS_DEFAULT=0', '-DDE_SPEC', '-DDE_TRACE', '-Os', '-fno-delete-null-pointer-checks',
    `"${mk.RAYLIB}/lib/libraylib.a"`,
    '-framework OpenGL', '-framework Cocoa', '-framework IOKit',
    '-framework CoreVideo', '-framework CoreFoundation', '-framework CoreMIDI',
    '-framework AudioToolbox',   // mic_desktop.h AudioQueue capture (Tier-1 mic input)
    '-Wl,-dead_strip', `-o "${BIN}"`,
  ].join(' ')
  execSync(`clang ${clangArgs}`, { stdio: 'pipe' })
  return BIN
}

function runSpec(name) {
  const SRC = path.join(CARTS, `${name}.c`)
  if (!fs.existsSync(SRC)) return { name, err: 'no source' }
  if (!hasSpec(SRC)) return { name, skipped: true }
  const work = path.join(mk.BUILD_DIR, '.spec', name)
  let BIN
  try { BIN = build(name, work) }
  catch (e) { return { name, err: 'compile failed\n' + (e.stderr?.toString() || e.message) } }
  // darwin: run under `caffeinate -dims` — a SLEEPING DISPLAY segfaults raylib's window init
  // (even --headless), which silently killed every unattended night run (bit 2026-07-02)
  const r = process.platform === 'darwin'
    ? spawnSync('caffeinate', ['-dims', BIN, '--spec'], { cwd: work, encoding: 'utf8' })
    : spawnSync(BIN, ['--spec'], { cwd: work, encoding: 'utf8' })
  const asserts = []; let done = null
  for (const ln of (r.stdout || '').trim().split('\n').filter(Boolean)) {
    let o; try { o = JSON.parse(ln) } catch { continue }
    if (o.done) done = o; else asserts.push(o)
  }
  if (!done) return { name, err: 'crashed before finishing (exit ' + r.status + ')\n' + (r.stderr || '') }
  return { name, asserts, done }
}

// ── run ──
const names = only ? [only]
  : fs.readdirSync(CARTS).filter(f => f.endsWith('.c')).map(f => f.slice(0, -2))
      .filter(n => hasSpec(path.join(CARTS, `${n}.c`))).sort()

if (names.length === 0) { console.log('no carts with a spec() found' + (only ? ` (looked for "${only}")` : '')); process.exit(0) }

let totalPass = 0, totalFail = 0, hadError = false
const C = { g: '\x1b[32m', r: '\x1b[31m', d: '\x1b[90m', y: '\x1b[33m', x: '\x1b[0m' }
for (const name of names) {
  const res = runSpec(name)
  if (res.skipped) continue
  if (res.err) { hadError = true; console.log(`${C.r}✘ ${name}${C.x}  ${res.err.split('\n')[0]}`);
    if (!quiet) console.log(res.err.split('\n').slice(1).map(l => '    ' + l).join('\n')); continue }
  const { pass, fail } = res.done; totalPass += pass; totalFail += fail
  const mark = fail > 0 ? `${C.r}✘${C.x}` : `${C.g}✓${C.x}`
  console.log(`${mark} ${name}  ${C.g}${pass} passed${C.x}` + (fail ? `, ${C.r}${fail} failed${C.x}` : ''))
  for (const a of res.asserts) {
    if (a.pass && quiet) continue
    const m = a.pass ? `  ${C.g}✓${C.x}` : `  ${C.r}✘${C.x}`
    let extra = (!a.pass && a.got !== undefined) ? `  ${C.d}(got ${a.got}, want ${a.want})${C.x}` : ''
    console.log(`${m} ${a.pass ? C.d : ''}${a.msg}${C.x}${extra}`)
  }
}
console.log(`\n${totalFail || hadError ? C.r : C.g}${totalPass} passed, ${totalFail} failed` +
            `${hadError ? ', build/run errors' : ''}${C.x} across ${names.length} cart(s)`)
process.exit((totalFail > 0 || hadError) ? 1 : 0)
