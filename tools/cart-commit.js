#!/usr/bin/env node
// cart-commit.js — the per-cart commit helper: do the mechanical ship steps AND
// the parallel-agent SAFETY checks as code, then commit by a derived pathspec.
//
// It exists because the routine that ends every cart edit (re-embed → lint →
// regen views → check nothing foreign got swept → commit by exact pathspec) is
// both tedious AND where the two documented commit hazards live (a missed file
// in the pathspec; a foreign change swept into a generated registry). This bakes
// the guards in so they can't be skipped.
//
//   node tools/cart-commit.js <name>                      # DRY-RUN: build, lint, regen,
//                                                         #   scope-check, PRINT the git command
//   node tools/cart-commit.js <name> -m "msg" --commit    # actually commit
//
// What it does, each a gate:
//   1. BUILD     re-embed source via make-cart (regenerates index.json). Skip with --no-build.
//                --thumb <png> sets the thumbnail from a file; --run bakes it by running the cart.
//   2. LINT      lint-carts; aborts if THIS cart is invalid (foreign-cart issues only warn).
//   3. REGEN     rebuild the aggregate views (design board + compendium) so their staleness
//                pre-commit hooks pass on disk.
//   4. SCOPE     diff index.json vs HEAD and assert ONLY this cart's entry changed. If another
//                cart moved (a parallel agent's dirty work), ABORT — don't sweep it. (--force to override.)
//   5. PATHSPEC  assemble the commit set: the cart's own files (<name>.c + .cart.js + .cart.png +
//                changed clips/<name>/*) + index.json (only if it changed). No hand-listing.
//   6. COMMIT    with --commit + -m. Otherwise prints the exact `git add && git commit -- …`.
//
// Deliberately OUT of scope (left dirty + reported, not auto-committed): design-board.html /
// cart-compendium.html. They aggregate EVERY doc/cart, so they can't be cleanly scoped to one
// cart; regenerating them (step 3) is enough for the hooks — commit them deliberately yourself.
//
// Flags: -m "<msg>" · --commit · --thumb <png> · --run · --no-build · --force · --no-coauthor
//
// Companion to cart-status.js (what's out of date) and lint-carts.js (registry valid?).

const { execFileSync } = require('child_process')
const fs = require('fs')
const path = require('path')

const ROOT = path.resolve(__dirname, '..')
const COAUTHOR = 'Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>'

// ── tiny arg parser ─────────────────────────────────────────────────────────
const a = process.argv.slice(2)
const opt = { commit: false, build: true, force: false, coauthor: true, run: false, msg: null, thumb: null, name: null }
for (let i = 0; i < a.length; i++) {
  const t = a[i]
  if      (t === '--commit')      opt.commit = true
  else if (t === '--no-build')    opt.build = false
  else if (t === '--force')       opt.force = true
  else if (t === '--no-coauthor') opt.coauthor = false
  else if (t === '--run')         opt.run = true
  else if (t === '-m')            opt.msg = a[++i]
  else if (t === '--thumb')       opt.thumb = a[++i]
  else if (t.startsWith('-'))     { console.error(`unknown flag: ${t}`); process.exit(2) }
  else if (!opt.name)             opt.name = t
}
if (!opt.name) {
  console.error('usage: node tools/cart-commit.js <name> [-m "msg"] [--commit] [--thumb <png>] [--run] [--no-build] [--force]')
  process.exit(2)
}

const NAME = opt.name
const SRC  = `tools/carts/${NAME}.c`
const CFG  = `tools/carts/${NAME}.cart.js`
const PNG  = `editor/public/carts/${NAME}.cart.png`
const CLIPS = `tools/clips/${NAME}`
const INDEX = 'editor/public/carts/index.json'

const die = (m) => { console.error(`\n✗ ${m}`); process.exit(1) }
const run = (cmd, args, capture) =>
  execFileSync(cmd, args, { cwd: ROOT, encoding: 'utf8', stdio: capture ? ['ignore', 'pipe', 'pipe'] : 'inherit' })
const git = (...args) => run('git', args, true).trim()
const tryGit = (...args) => { try { return git(...args) } catch { return '' } }
// porcelain-safe line read: NO global trim (that would strip the leading status
// space off the first line and shift slice(3) by a char — a partial-commit bug).
const gitLines = (...args) => {
  try { return run('git', args, true).split('\n').filter(Boolean).map(l => l.slice(3)) }
  catch { return [] }
}

if (!fs.existsSync(path.join(ROOT, SRC))) die(`no source at ${SRC}`)
const hasCfg = fs.existsSync(path.join(ROOT, CFG))

// ── 1. build (re-embed source → regenerates index.json) ─────────────────────
if (opt.build) {
  console.log(`• build: re-embedding ${SRC}`)
  run('node', ['tools/make-cart.js', SRC, PNG])
}
if (opt.thumb) {
  console.log(`• thumbnail: ${opt.thumb}`)
  run('node', ['tools/make-cart.js', '--update', PNG, opt.thumb])
}
if (opt.run) {
  console.log('• thumbnail: baking via --run')
  run('node', ['tools/make-cart.js', '--run', PNG])
}

// ── 2. lint (abort only if THIS cart is the problem) ────────────────────────
let lintOut = ''
try { lintOut = run('node', ['tools/lint-carts.js'], true) }
catch (e) { lintOut = (e.stdout || '') + (e.stderr || '') }
const lintHitsCart = new RegExp(`\\b${NAME}\\b`).test(lintOut)
if (lintHitsCart && !opt.force) {
  console.error(lintOut)
  die(`lint-carts flagged "${NAME}". Fix it, or pass --force to commit anyway.`)
} else if (/\b(error|invalid|✗|out of sync)\b/i.test(lintOut)) {
  console.log('• lint: unrelated issues present (foreign carts?) — continuing')
} else {
  console.log('• lint: clean')
}

// ── 3. regen the aggregate views so staleness hooks pass on disk ─────────────
console.log('• regen: design board + compendium')
try { run('node', ['tools/build-design-board.js'], true) } catch {}
try { run('node', ['tools/build-compendium.js'], true) } catch {}

// ── 4. SCOPE check: index.json must only change THIS cart ───────────────────
const idxNames = (txt) => {
  try { const j = JSON.parse(txt); return new Map((j.carts || j).map(c => [c.name, JSON.stringify(c)])) }
  catch { return new Map() }
}
const headIdx = idxNames(tryGit('show', `HEAD:${INDEX}`))
const diskIdx = idxNames(fs.readFileSync(path.join(ROOT, INDEX), 'utf8'))
const indexChanged = []
for (const [n, v] of diskIdx) if (headIdx.get(n) !== v) indexChanged.push(n)
for (const [n] of headIdx) if (!diskIdx.has(n)) indexChanged.push(`${n} (removed)`)
const foreign = indexChanged.filter(n => n !== NAME)
if (foreign.length && !opt.force) {
  die(`index.json also changed for: ${foreign.join(', ')} — a parallel agent's dirty cart(s).\n` +
      `  Refusing to sweep them. Commit those carts first (or with their own cart-commit), then retry.\n` +
      `  (Override with --force if you really mean to include them.)`)
}
const indexDirty = indexChanged.length > 0

// ── 5. derive the pathspec (only files that actually changed) ───────────────
const dirty = new Set(gitLines('status', '--porcelain', '--', SRC, CFG, PNG, CLIPS, INDEX).map(f => f.trim()))
const pathspec = [...dirty].filter(f => f.startsWith(`tools/carts/${NAME}`) ||
                                        f === PNG ||
                                        f.startsWith(`tools/clips/${NAME}/`) ||
                                        (f === INDEX && indexDirty))
if (!pathspec.length) die('nothing changed for this cart — already committed?')

// ── 6. commit (or dry-run) ──────────────────────────────────────────────────
console.log('\nfiles for this commit:')
pathspec.forEach(f => console.log('  ' + f))

// what's left dirty that we deliberately don't touch (aggregates, foreign work)
const otherDirty = gitLines('status', '--porcelain').map(f => f.trim()).filter(f => !pathspec.includes(f))
if (otherDirty.length) {
  console.log('\nleft dirty (NOT committed — handle deliberately):')
  otherDirty.forEach(f => console.log('  ' + f))
}

const msgArgs = opt.msg ? ['-m', opt.msg] : []
if (opt.coauthor && opt.msg) msgArgs.push('-m', COAUTHOR)

if (opt.commit) {
  if (!opt.msg) die('--commit needs a message: -m "…"')
  run('git', ['add', '--', ...pathspec])
  run('git', ['commit', ...msgArgs, '--', ...pathspec])
  console.log('\n✓ committed:', tryGit('log', '--oneline', '-1'))
} else {
  const q = s => `'${s.replace(/'/g, "'\\''")}'`
  console.log('\nDRY-RUN — to commit, re-run with `-m "…" --commit`, or run:')
  console.log(`  git add -- ${pathspec.join(' ')}`)
  console.log(`  git commit -m ${opt.msg ? q(opt.msg) : q('<message>')} -m ${q(COAUTHOR)} -- ${pathspec.join(' ')}`)
}
