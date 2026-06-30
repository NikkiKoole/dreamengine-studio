#!/usr/bin/env node
// publish-all.js — publish every cart that NEEDS it (or all of them) in one deploy.
//
// The batch companion to publish-cart.sh. Run it bare and it CHECKS what's out
// of date (via cart-status.js), shows you the numbers, and PROMPTS for what to
// do — publish the stale set, include engine-stale, publish everything, run the
// build-all compile gate first, or just list/quit. Flags skip the prompt for
// scripting.
//
//   node tools/publish-all.js                # interactive: check, then a menu
//   node tools/publish-all.js --dry-run      # print the plan (smart set), do nothing
//   node tools/publish-all.js -y             # non-interactive: publish the smart set
//   node tools/publish-all.js -y --engine-stale   # …also republish carts older than sound.h
//   node tools/publish-all.js -y --all       # …every cart (build-site mtime-skips fresh ones)
//   node tools/publish-all.js -y --all --force    # …force-rebuild even the fresh ones
//
// SMART SELECTION comes straight from `cart-status.js --json`: notPublished +
// stalePublished (+ engineStale on request). "Stale" is git-time based — a
// commit touched the cart's source/thumbnail after its last site/ build. The
// smart set is always force-rebuilt (we already know it's stale); under --all,
// build-site's own mtime check skips fresh carts unless you pass --force.
//
// RESILIENT: carts build via build-site.js, which keeps going past a cart that
// fails to compile. Only carts that actually produced a site/<name>/ build get
// published; failures are reported and skipped — they don't block the rest. The
// git leg reuses publish-cart.sh (--no-build), so the same stray-file guard
// protects against committing another agent's WIP.
//
// LONG RUN: web builds are emcc per cart — a full --all is minutes to tens of
// minutes; the smart set is usually small. The menu's build-all gate is the
// cheap clang check that catches API rot before the slow emcc loop.
//
// Publishing PUSHES to the live site, exactly like publish-cart.sh.

'use strict'

const fs = require('fs')
const path = require('path')
const readline = require('readline')
const { execFileSync, spawnSync } = require('child_process')

const ROOT = path.join(__dirname, '..')
const CARTS_DIR = path.join(ROOT, 'tools', 'carts')
const SITE_DIR = path.join(ROOT, 'site')

const args = process.argv.slice(2)
if (args.includes('-h') || args.includes('--help')) {
  const out = []
  for (const l of fs.readFileSync(__filename, 'utf8').split('\n')) {
    if (l.startsWith('//')) out.push(l.replace(/^\/\/ ?/, '')); else if (out.length) break
  }
  console.log(out.join('\n'))
  process.exit(0)
}
const DRY = args.includes('--dry-run')
const YES = args.includes('-y') || args.includes('--yes')
const ALL = args.includes('--all')
const FORCE = args.includes('--force')
const ENGINE = args.includes('--engine-stale')

const uniq = a => [...new Set(a)].sort()

function allCartNames() {
  return fs.readdirSync(CARTS_DIR)
    .filter(f => f.endsWith('.c') && !f.startsWith('_'))   // skip throwaway _scratch carts
    .map(f => f.slice(0, -2))
    .sort()
}

function getStatus() {
  let s
  try {
    s = JSON.parse(execFileSync('node', [path.join(__dirname, 'cart-status.js'), '--json'], { encoding: 'utf8' }))
  } catch (e) {
    console.error('publish-all: could not read cart-status.js --json\n' + (e.stderr || e.message || e))
    process.exit(1)
  }
  const all = allCartNames()
  const smart = uniq([...s.notPublished, ...s.stalePublished])
  return {
    notPublished: s.notPublished, stalePublished: s.stalePublished, engineStale: s.engineStale,
    all, smart, smartEngine: uniq([...smart, ...s.engineStale]),
  }
}

function printStatus(st) {
  console.log(`\npublish status:`)
  console.log(`  not-published    ${String(st.notPublished.length).padStart(4)}`)
  console.log(`  stale-published  ${String(st.stalePublished.length).padStart(4)}`)
  console.log(`  engine-stale     ${String(st.engineStale.length).padStart(4)}   (instrument carts older than latest runtime/sound.h)`)
  console.log(`  total catalog    ${String(st.all.length).padStart(4)}`)
}

// Build (resilient) then publish only the carts that produced a build. Terminal.
function buildAndPublish(names, { force, reason }) {
  if (names.length === 0) { console.log('nothing to publish.'); process.exit(0) }
  const buildArgs = [path.join(__dirname, 'build-site.js'), ...names]
  if (force) buildArgs.push('--force')
  console.log(`\nbuilding ${names.length} cart(s) for web…`)
  spawnSync('node', buildArgs, { stdio: 'inherit' })   // exit code ignored — verify by outputs

  const built = names.filter(n => fs.existsSync(path.join(SITE_DIR, n, 'index.html')))
  const failed = names.filter(n => !built.includes(n))
  if (failed.length)
    console.error(`\n⚠ ${failed.length} cart(s) did not build (skipped, not published):\n${failed.map(n => '  ' + n).join('\n')}`)
  if (built.length === 0) { console.error('\npublish-all: nothing built — aborting before commit'); process.exit(1) }

  const msg = `site: publish-all — ${built.length} cart(s) (${reason}${failed.length ? `, ${failed.length} skipped` : ''})`
  console.log(`\npublishing ${built.length} cart(s)…`)
  const pub = spawnSync(path.join(__dirname, 'publish-cart.sh'), ['--no-build', '-m', msg, ...built], { stdio: 'inherit' })
  process.exit(pub.status || 0)
}

function runGate() {
  console.log('\nrunning build-all compile gate (clang syntax-check, catches API rot)…\n')
  const r = spawnSync('node', [path.join(__dirname, 'build-all.js'), '--quiet'], { stdio: 'inherit' })
  console.log(r.status === 0 ? '\n✓ gate passed — all carts compile against studio.h'
                             : '\n⚠ gate FAILED — some carts no longer compile (they will be skipped at publish)')
}

function ask(rl, q) { return new Promise(res => rl.question(q, a => res(a.trim()))) }

async function interactive(st) {
  const rl = readline.createInterface({ input: process.stdin, output: process.stdout })
  try {
    for (;;) {
      printStatus(st)
      console.log(`\nwhat do you want to do?`)
      console.log(`  [1]  publish smart set — ${st.smart.length} cart(s)  (not-published + stale)   ← recommended`)
      console.log(`  [2]  publish smart + engine-stale — ${st.smartEngine.length} cart(s)`)
      console.log(`  [3]  publish ALL — ${st.all.length} cart(s)  (rebuild only changed)`)
      console.log(`  [3f] publish ALL, force-rebuild every cart`)
      console.log(`  [l]  list the smart set (no build)`)
      console.log(`  [g]  run the build-all compile gate first`)
      console.log(`  [q]  quit`)
      const a = (await ask(rl, '> ')).toLowerCase()
      if (a === 'q' || a === '') { console.log('bye.'); return }
      if (a === 'l') { console.log(st.smart.length ? st.smart.map(n => '  ' + n).join('\n') : '  (smart set is empty)'); continue }
      if (a === 'g') { runGate(); continue }
      let names, reason, force = false
      if (a === '1') { names = st.smart; reason = 'smart' }
      else if (a === '2') { names = st.smartEngine; reason = 'smart+engine' }
      else if (a === '3') { names = st.all; reason = 'all' }
      else if (a === '3f') { names = st.all; reason = 'all-force'; force = true }
      else { console.log('  ? pick 1 / 2 / 3 / 3f / l / g / q'); continue }
      if (names.length === 0) { console.log('  nothing to do there.'); continue }
      const ok = (await ask(rl, `\nthis builds ${names.length} cart(s) (can take many minutes) and PUSHES to the live site.\ncontinue? [y/N] `)).toLowerCase()
      if (ok === 'y' || ok === 'yes') { rl.close(); return buildAndPublish(names, { force, reason }) }
      console.log('  cancelled.')
    }
  } finally { rl.close() }
}

async function main() {
  // Non-interactive paths first.
  if (DRY) {
    const st = getStatus()
    const names = ALL ? st.all : (ENGINE ? st.smartEngine : st.smart)
    printStatus(st)
    console.log(`\nwould publish ${names.length} cart(s)${ALL ? ' (--all)' : ENGINE ? ' (smart + engine-stale)' : ' (smart)'}:`)
    console.log(names.length ? names.map(n => '  ' + n).join('\n') : '  (nothing — all up to date)')
    console.log(`\n(dry run — nothing built or pushed)`)
    return
  }
  if (YES) {
    const st = getStatus()
    const names = ALL ? st.all : (ENGINE ? st.smartEngine : st.smart)
    const reason = ALL ? (FORCE ? 'all-force' : 'all') : (ENGINE ? 'smart+engine' : 'smart')
    return buildAndPublish(names, { force: ALL ? FORCE : true, reason })
  }
  // Interactive (default). Needs a TTY to prompt.
  if (!process.stdin.isTTY) {
    const st = getStatus()
    printStatus(st)
    console.log(`\nnot a TTY — can't prompt. Re-run with -y to publish (smart set), or --dry-run to preview.`)
    return
  }
  await interactive(getStatus())
}

main()
