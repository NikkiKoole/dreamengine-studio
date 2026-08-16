#!/usr/bin/env node
// cart-land-headers.js — (lib, not CLI) the ONE definition of "which runtime/*.h is a cart-land
// library header (ADR-0006), and which is engine internals". Owned here so lint-docs.js (the
// discoverability gate) and api-usage.js (the call-site scan) cannot drift apart, the same lib
// shape as doc-status.js and capability-roster.js.
//
//   const { shelf, engineInternals, isShelf, privateModuleDirs } = require('./cart-land-headers')
//   shelf()              → ['acid303.h', 'ampcab.h', …]  cart-land library headers, sorted
//   engineInternals()    → the allowlisted engine/platform/generated headers, with their reasons
//   isShelf('ui.h')      → true
//   privateModuleDirs()  → ['isoroom', 'lockup', 'tenement']  one cart's own modules, NOT shelf
//
// WHY it got extracted: api-usage.js counted call sites in tools/carts/*.c only, so a studio.h
// function called from a SHARED HEADER read as zero — and by 2026-08 four of the ten zeros were
// that (de_state_for, de_state_for_saved, instrument_bandlimit via acid303.h, plus de_switch_cart
// from the generated app shim). The audit's own advice is "ship a cart for it or cut it", so a
// false zero is a live instruction to cut the per-instance-state seam the AUv3 work sits on.
// Teaching the scan the second corpus needs exactly the classification lint-docs already kept,
// and two hand-copies of that list would rot in opposite directions the way the two doc lists it
// gates already did once.
'use strict'

const fs = require('fs')
const path = require('path')

const RUNTIME = path.join(__dirname, '..', 'runtime')

// Not cart-land: the public API itself, the engine + harness, the platform/host seams, the
// per-instance context headers (engine state + the macro block it reads through, compiled only
// inside their owning engine file — docs/design/engine-context.md), and vendored/generated files.
// Each value is the REASON, printed by --explain so a future reader can judge the call rather
// than inherit it. Adding a platform seam? Put it here and lint-docs stops asking for a
// cart-authoring row.
const ENGINE_INTERNALS = new Map([
  ['studio.h', 'the public API, not a cart-land library'],
  ['sound.h', 'engine, documented elsewhere in CLAUDE.md'],
  ['spec.h', 'harness, documented elsewhere in CLAUDE.md'],
  ['color.h', 'platform seam'],
  ['game_rect.h', 'platform seam'],
  ['platform.h', 'platform seam'],
  ['raylib_compat.h', 'platform seam'],
  ['mic.h', 'host input plumbing'],
  ['mic_desktop.h', 'host input plumbing'],
  ['midi_input.h', 'host input plumbing'],
  ['sync.h', 'the external-clock seam; cart-facing API is in studio.h'],
  ['midi_output.h', "the OUT direction's twin of midi_input.h — CoreMIDI virtual source, compiled inside studio.c; cart-facing API is midi_send_* in studio.h"],
  ['param.h', 'HOST PARAMETERS (docs/design/host-parameters.md) — the DAW-facing table, compiled inside studio.c; cart-facing API is param_bind/param_count in studio.h'],
  ['param_ctx.h', 'the host-parameter table’s per-instance context'],
  ['stb_image.h', 'vendored'],
  ['studio_tcc_symbols.h', 'generated (tools/gen-tcc-symbols.js)'],
  ['sound_ctx.h', 'per-instance context, generated (tools/ctx-gen.js)'],
  ['studio_ctx.h', 'per-instance context, generated (tools/ctx-gen.js)'],
  ['sync_ctx.h', 'per-instance context, generated (tools/ctx-gen.js)'],
  ['midi_ctx.h', 'per-instance context, the same shape written by HAND (ctx-gen refuses to re-run on a processed target)'],
])

// Generated, not shelf. `_state.h` is one CART's per-instance context (tools/ctx-gen.js --target
// cart writes runtime/<slug>_state.h): nobody's to reach for, regenerated rather than read, and a
// new one appears for every cart that becomes a plug-in rack — so it belongs in the PATTERN
// rather than in an allowlist somebody has to remember to extend.
const GENERATED_H = /(_data|_font|_baked|_state)\.h$/

// Subdirectories of runtime/ holding ONE cart's private modules (CLAUDE.md: "NOT shelf"). They are
// real call sites, but they belong to a cart that the cart scan already counts, so a caller that
// reports per-corpus totals should keep them apart from the shared shelf.
const PRIVATE_MODULE_DIRS = ['isoroom', 'lockup', 'tenement']

// Vendored or platform trees under runtime/ that are nobody's call site.
const SKIP_DIRS = new Set(['box2d', 'libtcc', 'raylib-web'])

function isGenerated(name) { return GENERATED_H.test(name) }
function isEngineInternal(name) { return ENGINE_INTERNALS.has(name) }
function isShelf(name) {
  return name.endsWith('.h') && !isEngineInternal(name) && !isGenerated(name)
}

/** Cart-land library headers, bare filenames, sorted. Reads runtime/ so a new header is shelf by
 *  DEFAULT — the direction that fails loudly (lint-docs asks for a doc row) rather than silently. */
function shelf(dir = RUNTIME) {
  return fs.readdirSync(dir).sort().filter(isShelf)
}

function engineInternals() { return new Map(ENGINE_INTERNALS) }

/** Private-module dirs that actually exist, sorted. */
function privateModuleDirs(dir = RUNTIME) {
  return PRIVATE_MODULE_DIRS
    .filter(d => fs.existsSync(path.join(dir, d)))
    .sort()
}

/** Generated per-cart context headers (runtime/<slug>_state.h), bare filenames, sorted.
 *  NOT shelf — nobody reaches for one, so lint-docs must not ask for a doc row. But they ARE
 *  first-party call sites, which is a different question: acidcandy_state.h holds the only
 *  de_state_for_saved call in the repo, and a scan that skips it reports the saved-state seam as
 *  unused API. Callers asking "does anything CALL this" want these; callers asking "does this need
 *  documenting" do not. */
function generatedCartContexts(dir = RUNTIME) {
  return fs.readdirSync(dir).sort().filter(n => /_state\.h$/.test(n))
}

module.exports = {
  RUNTIME, GENERATED_H, SKIP_DIRS,
  shelf, engineInternals, privateModuleDirs, generatedCartContexts,
  isShelf, isEngineInternal, isGenerated,
}

// ── --explain: the classification, for a human deciding where a new header goes ──────────────
if (require.main === module) {
  const s = shelf()
  console.log(`cart-land shelf (${s.length}) — reach for these from a cart; lint-docs requires a`)
  console.log(`CLAUDE.md pointer + a cart-authoring.md table row for each:\n`)
  console.log('  ' + s.join(' '))
  console.log(`\nengine internals (${ENGINE_INTERNALS.size}) — allowlisted, no cart-authoring row expected:\n`)
  for (const [name, why] of ENGINE_INTERNALS) console.log(`  ${name.padEnd(24)} ${why}`)
  console.log(`\ngenerated (pattern ${GENERATED_H}) — never shelf, never hand-edited`)
  console.log(`private cart modules: ${privateModuleDirs().map(d => 'runtime/' + d + '/').join(' ')}`)
}
