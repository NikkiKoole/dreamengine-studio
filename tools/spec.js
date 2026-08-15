#!/usr/bin/env node
// tools/spec.js — run each cart's spec() (the gameplay twin of tune-check.js). For every cart whose
// source defines a non-empty `void spec(void)`, compile it + studio.c with -DDE_SPEC -DDE_TRACE, run
// the binary with --spec (headless: it calls spec() then prints JSONL pass/fail and exits), parse the
// results, and report. Carts WITHOUT a spec() are skipped (the weak stub), never failed.
//
//   node tools/spec.js                 # every cart that has a spec()
//   node tools/spec.js streetlab       # just one cart
//   node tools/spec.js --quiet         # CI: only failures + a summary; exit 1 if any assertion failed
//   node tools/spec.js --selfcheck     # known answers for the RUNNER's own judgement (compiles nothing)
//
// ⚠ THE CONTROLS, and why a gate like this needs them. Every verdict here is "no assertion failed",
// which is trivially true of a run that made no assertions. Measured: a cart whose spec() body is
// empty compiled, ran, and printed a green `✓ 0 passed`, and the run exited 0. The same holds at
// every scale — `{"done":1,"pass":0,"fail":0}` is what a WEAK-STUBBED spec prints, so if the stub
// ever won over a cart's own definition (a rename, a linker change, -DDE_SPEC not reaching the
// build) all 32 carts would report ✓ 0 passed and CI would stay green with the entire gameplay gate
// doing nothing. specControl()/sweepControl() below require the run to have actually asserted.
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
  // -u -t 1 first: -dims only PREVENTS display sleep, it cannot WAKE an already-dark display, which
  // still segfaults the run (bit 2026-07-28 — the gap the original -dims fix left open).
  if (process.platform === 'darwin') spawnSync('caffeinate', ['-u', '-t', '1'], { stdio: 'ignore' })
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

// ── the controls ─────────────────────────────────────────────────────────────
// Pure, so --selfcheck can put known results through them. These do not judge GAMEPLAY — they judge
// whether this runner is in a position to judge gameplay at all.
function specControl(res) {
  if (!res || res.skipped || res.err) return []          // those are reported on their own terms
  const bad = []
  const { pass, fail } = res.done
  // (1) a spec that asserts NOTHING is not a passing spec. This is exactly what the weak stub
  // prints, so it is also the only local signal that a cart's own spec() never made it into the
  // build. Proven to have printed a green tick before this existed.
  if (pass + fail === 0)
    bad.push(`${res.name}: spec() ran and asserted NOTHING (0 assertions) — an empty spec is not a passing spec, and it is also what the weak stub prints`)
  // (2) the parsed assertion lines must account for the counts the binary reported. expect() prints
  // one line per assertion and fflushes, so a shortfall means lines were lost or unparseable —
  // note expect() interpolates `msg` into JSON raw, so a quote or backslash in a message yields a
  // line JSON.parse rejects and the runner silently drops
  else if (res.asserts && res.asserts.length !== pass + fail)
    bad.push(`${res.name}: parsed ${res.asserts.length} assertion line(s) but the cart reported ${pass + fail} — output was lost (an unescaped quote or backslash in an expect() message will do this)`)
  return bad
}

// Run-level: the sweep itself has to have found something to do. `no carts found` and `that cart has
// no spec` both used to print a friendly line and exit 0, which reads identically to success.
// `hadError` is taken so this stays quiet when the per-cart path has ALREADY said what went wrong —
// a cart that does not exist is reported as `no source` there, and adding "it has no spec()" on top
// of that would be a second, less accurate explanation of the same thing.
function sweepControl(names, only, ran, hadError) {
  const bad = []
  if (names.length === 0)
    bad.push('the sweep found NO carts with a spec() — the discovery broke, which is not the same as nothing to do')
  else if (only && ran === 0 && !hadError)
    bad.push(`"${only}" has no spec() — nothing ran, so this is not a pass (drop the argument to sweep every cart that has one)`)
  return bad
}

const exitCode = (totalFail, hadError, control) => (totalFail > 0 || hadError || control.length) ? 1 : 0

// ── --selfcheck: KNOWN ANSWERS FOR THE RUNNER'S JUDGEMENT ────────────────────
// Compiles nothing and runs no cart. What it pins is this file's own reasoning: which results are a
// pass, which are a refusal to judge, and what the process exits with. `hasSpec` is exercised
// against real source text, since that regex decides which carts are in the sweep at all.
function selfcheck() {
  let pass = 0, fail = 0
  const ok = (name, cond, got) => {
    if (cond) { pass++; console.log(`  ✓ ${name}`) } else { fail++; console.log(`  ✗ ${name}   got: ${got}`) }
  }
  const result = (o = {}) => ({ name: 'probe', asserts: o.asserts !== undefined ? o.asserts
      : [...Array((o.pass ?? 3) + (o.fail ?? 0))].map(() => ({ pass: 1, msg: 'x' })),
    done: { done: 1, pass: o.pass ?? 3, fail: o.fail ?? 0 } })

  console.log('spec.js --selfcheck — known answers for the runner (nothing is compiled)\n')

  console.log('THE VACUITY GUARD — what this gate shipped without')
  ok('a spec that asserted NOTHING is refused, not ticked',
     specControl(result({ pass: 0, fail: 0 })).some(c => c.includes('asserted NOTHING')),
     specControl(result({ pass: 0, fail: 0 })))
  // {"done":1,"pass":0,"fail":0} is exactly what a weak-stubbed spec() prints, so the check above
  // is also the only local signal that a cart's own spec never reached the build
  ok('  …which is the same output a WEAK-STUBBED spec produces, so the stub cannot pass either',
     specControl(result({ pass: 0, fail: 0 })).length === 1, 'silent')
  ok('  …and it makes the process exit nonzero even with zero failures',
     exitCode(0, false, specControl(result({ pass: 0, fail: 0 }))) === 1, 'exit 0')
  ok('one real assertion is enough to be judged normally', specControl(result({ pass: 1 })).length === 0,
     specControl(result({ pass: 1 })))
  ok('a cart with failures is left to the normal path, not double-reported',
     specControl(result({ pass: 2, fail: 1 })).length === 0, specControl(result({ pass: 2, fail: 1 })))

  console.log('\nLOST OUTPUT — expect() interpolates its message into JSON raw')
  ok('fewer parsed lines than the cart counted is caught',
     specControl(result({ pass: 5, asserts: [1, 2, 3].map(() => ({ pass: 1 })) })).some(c => c.includes('output was lost')),
     'silent')
  ok('  …and a matching count is not', specControl(result({ pass: 5 })).length === 0,
     specControl(result({ pass: 5 })))
  // a real message with a quote in it produces a line JSON.parse rejects — the shape of the bug
  ok('  …the JSON a quoted message would emit really is unparseable (why the count can drop)',
     (() => { try { JSON.parse('{"pass":0,"msg":"he said "no""}'); return false } catch { return true } })(),
     'parsed fine')

  console.log('\nTHE SWEEP ITSELF HAS TO HAVE FOUND WORK')
  ok('an empty sweep is an error, not "nothing to do"',
     sweepControl([], null, 0, false).some(c => c.includes('discovery broke')), sweepControl([], null, 0, false))
  ok('a cart that does not exist is left to the per-cart "no source" error, not explained twice',
     sweepControl(['nosuch'], 'nosuch', 0, true).length === 0, sweepControl(['nosuch'], 'nosuch', 0, true))
  ok('naming a cart with no spec() is an error, not a silent success',
     sweepControl(['tetris'], 'tetris', 0, false).some(c => c.includes('has no spec')), sweepControl(['tetris'], 'tetris', 0, false))
  ok('  …and it says what to do instead',
     sweepControl(['tetris'], 'tetris', 0, false).some(c => c.includes('drop the argument')), 'no hint')
  ok('a normal sweep that ran raises nothing', sweepControl(['a', 'b'], null, 2, false).length === 0,
     sweepControl(['a', 'b'], null, 2, false))

  console.log('\nTHE EXIT CODE')
  ok('clean run exits 0', exitCode(0, false, []) === 0, exitCode(0, false, []))
  ok('a failed assertion exits 1', exitCode(1, false, []) === 1, exitCode(1, false, []))
  ok('a build error exits 1', exitCode(0, true, []) === 1, exitCode(0, true, []))
  ok('a control problem alone exits 1 — this is the case that used to exit 0',
     exitCode(0, false, ['x']) === 1, exitCode(0, false, ['x']))

  console.log('\nWHICH CARTS ARE IN THE SWEEP AT ALL (hasSpec, on real source text)')
  const os = require('os')
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'speccheck-'))
  let n = 0
  const src = (text) => { const f = path.join(dir, `c${n++}.c`); fs.writeFileSync(f, text); return f }
  ok('a plain spec is found', hasSpec(src('void spec(void) {\n expect(1,"a");\n}')), false)
  ok('  …with spaces inside the parens', hasSpec(src('void spec( void )\n{\n}')), false)
  ok('  …and with the brace on the next line', hasSpec(src('void spec(void)\n{\n}')), false)
  ok('a cart with no spec is not enrolled', hasSpec(src('void draw(void) { cls(0); }')) === false, true)
  ok('  …nor is a mere mention in prose', hasSpec(src('// this cart has no spec yet\n')) === false, true)
  // KNOWN AND DELIBERATELY LEFT: the regex reads raw source, so a commented-out definition enrols
  // the cart. Not worth a comment-stripping pass — the cart then asserts nothing, and control (1)
  // above turns that into a loud, accurate failure instead of the silent green it used to be.
  ok('a COMMENTED-OUT spec still enrols the cart (pinned: control 1 is what catches the consequence)',
     hasSpec(src('// void spec(void) {\n//   expect(1,"x");\n// }\n')) === true, false)
  fs.rmSync(dir, { recursive: true, force: true })

  console.log(`\n${fail ? '✗' : '✓'} ${pass} passed, ${fail} failed`)
  return fail ? 1 : 0
}

if (args.includes('--selfcheck')) process.exit(selfcheck())

// ── run ──
const names = only ? [only]
  : fs.readdirSync(CARTS).filter(f => f.endsWith('.c')).map(f => f.slice(0, -2))
      .filter(n => hasSpec(path.join(CARTS, `${n}.c`))).sort()

if (names.length === 0) {
  for (const c of sweepControl(names, only, 0, false)) console.error(`✗ ${c}`)
  process.exit(1)
}

let totalPass = 0, totalFail = 0, hadError = false, ran = 0
const control = []
const C = { g: '\x1b[32m', r: '\x1b[31m', d: '\x1b[90m', y: '\x1b[33m', x: '\x1b[0m' }
for (const name of names) {
  const res = runSpec(name)
  if (res.skipped) continue
  if (res.err) { hadError = true; console.log(`${C.r}✘ ${name}${C.x}  ${res.err.split('\n')[0]}`);
    if (!quiet) console.log(res.err.split('\n').slice(1).map(l => '    ' + l).join('\n')); continue }
  ran++
  const { pass, fail } = res.done; totalPass += pass; totalFail += fail
  const ctl = specControl(res); control.push(...ctl)
  // a cart that asserted nothing must NOT wear a tick — that green tick is the whole defect
  const mark = (fail > 0 || ctl.length) ? `${C.r}✘${C.x}` : `${C.g}✓${C.x}`
  console.log(`${mark} ${name}  ${C.g}${pass} passed${C.x}` + (fail ? `, ${C.r}${fail} failed${C.x}` : ''))
  for (const c of ctl) console.log(`  ${C.r}✘${C.x} ${c.replace(name + ': ', '')}`)
  for (const a of res.asserts) {
    if (a.pass && quiet) continue
    const m = a.pass ? `  ${C.g}✓${C.x}` : `  ${C.r}✘${C.x}`
    let extra = (!a.pass && a.got !== undefined) ? `  ${C.d}(got ${a.got}, want ${a.want})${C.x}` : ''
    console.log(`${m} ${a.pass ? C.d : ''}${a.msg}${C.x}${extra}`)
  }
}
control.push(...sweepControl(names, only, ran, hadError))
console.log(`\n${totalFail || hadError || control.length ? C.r : C.g}${totalPass} passed, ${totalFail} failed` +
            `${hadError ? ', build/run errors' : ''}${C.x} across ${ran} cart(s)`)
if (control.length) {
  console.error(`\n${C.r}✗ THE RUNNER IS NOT IN A POSITION TO JUDGE:${C.x}`)
  for (const c of control) console.error(`    ${c}`)
  console.error('  A spec that asserts nothing passes every check there is. `node tools/spec.js --selfcheck`.')
}
process.exit(exitCode(totalFail, hadError, control))
