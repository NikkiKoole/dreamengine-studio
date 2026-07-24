#!/usr/bin/env node
// build-all.js — compile-check EVERY cart against the current studio.h.
//
// Why: the catalog is hundreds of carts, and a new API (or a cart variable that
// collides with a newly-added API name — the CLAUDE.md namespace trap) can
// silently rot an old cart. Nothing else compiles the whole catalog: cart-status.js
// only checks source-drift/publish state, play.js compiles one at a time. This is
// the regression sweep that catches "mouthharp stopped compiling the day formant()
// shipped" — which sat broken for days unnoticed.
//
// It is a MANUAL / occasional tool (NOT a per-commit hook). Run it after touching
// studio.h / adding API, or before a publish.
//
//   node tools/build-all.js            # full report
//   node tools/build-all.js --quiet    # only failures + summary
//   node tools/build-all.js --jobs 8   # parallelism (default = cpu count)
//
// Exit code 0 = all compile, 1 = at least one failed (so it CAN gate a publish if wanted).
//
// How: clang -fsyntax-only on each tools/carts/<name>.c with -I runtime. That checks
// the cart against studio.h's declarations (catches redefinitions, wrong-arg calls,
// undeclared symbols, type errors) WITHOUT building studio.c or linking raylib — so
// it's seconds, not minutes. It does NOT catch link errors or bugs inside the engine
// itself (soundcheck + the real build cover those); its job is cart-vs-API rot.

const fs = require('fs')
const path = require('path')
const { execFile } = require('child_process')
const { promisify } = require('util')
const os = require('os')
const mk = require('./make-cart.js')
const execFileAsync = promisify(execFile)

const args = process.argv.slice(2)
const QUIET = args.includes('--quiet')
const JOBS = (() => { const i = args.indexOf('--jobs'); return i >= 0 ? Math.max(1, +args[i + 1] || 1) : Math.max(1, os.cpus().length) })()

const CARTS_DIR = path.join(mk.ROOT_DIR, 'tools', 'carts')
const carts = fs.readdirSync(CARTS_DIR)
  .filter(f => f.endsWith('.c') && !f.startsWith('_'))   // skip throwaway _scratch carts
  .map(f => f.slice(0, -2))
  .sort()

// standard compile-time defines (values don't matter for a syntax check — studio.h
// has #ifndef defaults anyway; passed to match the real build's macro environment)
const DEFS = ['-DSCREEN_W=320', '-DSCREEN_H=200', '-DSCALE=2',
              '-DMAP_W=128', '-DMAP_H=64', '-DCELL_W=16', '-DCELL_H=16', '-DDE_TRACE']

// Opt-in library includes some carts need beyond studio.h (ADR: box2d is NOT in the
// default cart build). A syntax check just needs the HEADER on the include path — no
// lib, no link — so we add -Iruntime/box2d/include for any cart that pulls in box2d.
const BOX2D_INC = path.join(mk.RUNTIME_DIR, 'box2d', 'include')
function extraIncludes(name) {
  const src = fs.readFileSync(path.join(CARTS_DIR, `${name}.c`), 'utf8')
  const inc = []
  if (/#include\s+[<"]box2d\//.test(src)) inc.push(`-I${BOX2D_INC}`)
  return inc
}

async function checkOne(name) {
  const src = path.join(CARTS_DIR, `${name}.c`)
  try {
    await execFileAsync('clang', ['-fsyntax-only', src, `-I${mk.RUNTIME_DIR}`, ...extraIncludes(name), ...DEFS])
    return { name, ok: true }
  } catch (e) {
    const stderr = (e.stderr ? e.stderr.toString() : '') || (e.message || '')
    const firstErr = (stderr.split('\n').find(l => /error:/.test(l)) || stderr.split('\n')[0] || 'compile failed').trim()
    return { name, ok: false, err: firstErr }
  }
}

// simple bounded-parallel map (a worker pool over the cart list)
async function run() {
  const results = []
  let idx = 0
  const t0 = Date.now()
  async function worker() {
    while (idx < carts.length) {
      const name = carts[idx++]
      const r = await checkOne(name)   // async clang spawn → the pool runs JOBS of them concurrently
      results.push(r)
      if (!r.ok) console.log(`  ✗ ${r.name.padEnd(22)} ${r.err}`)
      else if (!QUIET) process.stdout.write('.')
    }
  }
  await Promise.all(Array.from({ length: JOBS }, worker))
  if (!QUIET) process.stdout.write('\n')

  const failed = results.filter(r => !r.ok)
  const secs = ((Date.now() - t0) / 1000).toFixed(1)
  console.log(`\n${carts.length} carts · ${results.length - failed.length} ok · ${failed.length} FAILED  (${secs}s, ${JOBS} jobs)`)
  if (failed.length) {
    console.log('\nFAILED:')
    for (const f of failed) console.log(`  ${f.name}: ${f.err}`)
    process.exit(1)
  }
}
run()
