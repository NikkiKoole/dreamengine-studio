#!/usr/bin/env node
// lint-aux-params.js — the per-engine AUX PARAM channel (`instrument_mode` / `eng_p[]`) has its width
// written down in FIVE places that must agree. This asserts they do.
//
//   node tools/lint-aux-params.js            report
//   node tools/lint-aux-params.js --quiet     exit 1 on any mismatch (CI gate)
//
// WHY. Adding an aux param means widening `eng_p[]` AND both `idx >= N` bounds AND the note-on copy AND
// keeping every `MODE_*` constant inside the width. Miss one and the failure is SILENT: the public setter
// accepts the value, queues it, and the request handler drops it — the parameter simply does nothing, with
// no error anywhere. That has now happened TWICE:
//
//   · 2026-07-28 — `instrument_mode` guarded `idx >= 2` while PIANO's idx 2/3 were implemented end to end,
//     so the piano cart's DECAY and KNOCK sliders did nothing for as long as they had existed (audit §I9).
//   · 2026-07-30 — adding MODE_PIANO_STRETCH (idx 4) widened the SETTER's bound but not the SR_ENG_TUNE
//     HANDLER's, so tune-check's new differential pass rendered byte-identical to the normal one and read
//     a 0¢ stretch at every note, including where the stretch demonstrably works.
//
// Both were minutes of confusion that a grep would have caught instantly. That is what this is.
// Related: docs/design/synth-secrets-plan.md §2.3(a) postscript ("would spec() have caught this?" — no;
// this class wants a static lint, not a runtime assertion).

const fs = require('fs')
const path = require('path')
const ROOT = path.resolve(__dirname, '..')

const sound = fs.readFileSync(path.join(ROOT, 'runtime', 'sound.h'), 'utf8')
const studio = fs.readFileSync(path.join(ROOT, 'runtime', 'studio.h'), 'utf8')
const problems = []
const notes = []

// 1. every `eng_p[N]` DECLARATION must agree (the Instrument bank + the Voice)
const decls = [...sound.matchAll(/^\s*float\s+eng_p\[(\d+)\]\s*;/gm)].map(m => +m[1])
if (decls.length < 2) problems.push(`expected ≥2 eng_p[] declarations in sound.h, found ${decls.length}`)
const width = decls[0]
if (!decls.every(w => w === width))
  problems.push(`eng_p[] declarations disagree: ${decls.join(' vs ')} — the Instrument bank and the Voice must be the same width`)
else notes.push(`eng_p[] width = ${width} (${decls.length} declarations agree)`)

// 2. every aux-param BOUND must equal the width. There are two: instrument_mode() and SR_ENG_TUNE.
//    Matched on the shared shape `idx < 0 || idx >= N` so an unrelated `idx >=` elsewhere is not swept in.
const bounds = [...sound.matchAll(/idx\s*<\s*0\s*\|\|\s*idx\s*>=\s*(\d+)/g)].map(m => +m[1])
if (bounds.length < 2)
  problems.push(`expected 2 aux-param bounds (the instrument_mode setter AND the SR_ENG_TUNE handler), found ${bounds.length} — if one was refactored away, update this lint`)
for (const b of bounds)
  if (b !== width) problems.push(`an aux-param bound is \`idx >= ${b}\` but eng_p[] is ${width} wide — a MODE_* index in between is silently DROPPED`)
if (bounds.length >= 2 && bounds.every(b => b === width)) notes.push(`${bounds.length} aux-param bounds all = ${width}`)

// 3. the note-on COPY must cover every index (a missed one never reaches the voice)
const copies = [...sound.matchAll(/v->eng_p\[(\d+)\]\s*=\s*ins->eng_p\[\1\]/g)].map(m => +m[1])
const missing = [...Array(width).keys()].filter(i => !copies.includes(i))
if (missing.length)
  problems.push(`note-on does not copy eng_p[${missing.join(',')}] from the instrument — those params never reach the voice`)
else notes.push(`note-on copies all ${width} indices`)

// 4. every MODE_* constant must be inside the width. Indices are PER-ENGINE namespaces, so reuse across
//    engines (MODE_BOW_PIZZ 0 vs MODE_STRING_WEIGHT 0) is legal and not flagged.
const modes = [...studio.matchAll(/^#define\s+(MODE_[A-Z0-9_]+)\s+(\d+)/gm)].map(m => ({ name: m[1], idx: +m[2] }))
if (!modes.length) problems.push('no MODE_* constants found in studio.h — did they move?')
for (const m of modes)
  if (m.idx >= width) problems.push(`${m.name} = ${m.idx} is outside eng_p[${width}] — it can never be stored`)
const maxMode = modes.reduce((a, m) => Math.max(a, m.idx), -1)
if (maxMode >= 0) notes.push(`${modes.length} MODE_* constants, highest index ${maxMode} < ${width}`)

// 5. every MODE_* constant must be registered for docs + help (the four-places rule)
const docs = fs.readFileSync(path.join(ROOT, 'editor', 'src', 'studioDocs.js'), 'utf8')
const shell = fs.readFileSync(path.join(ROOT, 'editor', 'src', 'shell.js'), 'utf8')
for (const m of modes) {
  if (!docs.includes(`${m.name}:`)) problems.push(`${m.name} has no studioDocs.js entry (autocomplete/hover/help will miss it)`)
  if (!shell.includes(`'${m.name}'`)) problems.push(`${m.name} is not listed in shell.js sections (absent from the help tab)`)
}

const quiet = process.argv.includes('--quiet')
if (problems.length) {
  console.log(`AUX PARAM REGISTRATION — ${problems.length} problem(s):`)
  for (const p of problems) console.log(`  ✗ ${p}`)
  process.exit(1)
}
if (!quiet) {
  console.log('AUX PARAM REGISTRATION ✓')
  for (const n of notes) console.log(`  · ${n}`)
}
process.exit(0)
