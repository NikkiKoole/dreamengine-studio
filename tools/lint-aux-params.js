#!/usr/bin/env node
// lint-aux-params.js — the per-engine AUX PARAM channel (`instrument_mode` / `eng_p[]`) has its width
// written down in FIVE places that must agree. This asserts they do.
//
//   node tools/lint-aux-params.js             report
//   node tools/lint-aux-params.js --quiet     exit 1 on any mismatch (CI gate)
//   node tools/lint-aux-params.js --json      machine-readable
//   node tools/lint-aux-params.js --selfcheck assert the CHECKER (known-answer fixture)
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
//
// EVERY CHECK IS A REGEX OVER C SOURCE, so this tool can rot in the direction that leaves no trace: a
// pattern that stops matching finds nothing wrong and prints the same green "✓" as a healthy engine.
// Three of the five checks would pass VACUOUSLY on zero matches (no bounds → "all bounds equal the width";
// no modes → "every mode is registered"), which is exactly the silence this lint was built to break. So
// the "did I see anything at all?" guards are load-bearing, and `--selfcheck` pins them.

const fs = require('fs')
const path = require('path')
const ROOT = path.resolve(__dirname, '..')
const args = process.argv.slice(2)
const has = (f) => args.includes(f)

// ── the check, over an injectable set of four sources ────────────────────────────────────────
// Paths come from env so --selfcheck can point them at tools/fixtures/lint-aux-params/*.
function analyze() {
  const sound  = fs.readFileSync(process.env.DE_AUX_SOUND_H  || path.join(ROOT, 'runtime', 'sound.h'), 'utf8')
  // The DECLARATIONS moved. The per-instance refactor lifted every file-scope struct out of sound.h
  // into the generated sound_ctx.h, and both `float eng_p[7];` went with them — so this lint scanned
  // sound.h, found ZERO declarations, and had been RED since, unseen (repo-doctor ran its
  // --selfcheck, which passes on a fixture, and never the lint itself). Read both, and treat the ctx
  // header as optional so a fixture that keeps everything in one file still works.
  const soundCtxPath = process.env.DE_AUX_SOUND_CTX_H || path.join(ROOT, 'runtime', 'sound_ctx.h')
  const soundCtx = fs.existsSync(soundCtxPath) ? fs.readFileSync(soundCtxPath, 'utf8') : ''
  const soundDecls = sound + '\n' + soundCtx
  const studio = fs.readFileSync(process.env.DE_AUX_STUDIO_H || path.join(ROOT, 'runtime', 'studio.h'), 'utf8')
  const docs   = fs.readFileSync(process.env.DE_AUX_DOCS_JS  || path.join(ROOT, 'editor', 'src', 'studioDocs.js'), 'utf8')
  const shell  = fs.readFileSync(process.env.DE_AUX_SHELL_JS || path.join(ROOT, 'editor', 'src', 'shell.js'), 'utf8')

  const problems = []   // {kind, msg} — kind is what --selfcheck asserts on, msg is what a human reads
  const notes = []
  const bad = (kind, msg) => problems.push({ kind, msg })

  // 1. every `eng_p[N]` DECLARATION must agree (the Instrument bank + the Voice)
  const decls = [...soundDecls.matchAll(/^\s*float\s+eng_p\[(\d+)\]\s*;/gm)].map(m => +m[1])
  if (decls.length < 2)
    bad('decl-count', `expected ≥2 eng_p[] declarations in sound.h + sound_ctx.h, found ${decls.length} — if they moved again, update this lint`)
  const width = decls[0]
  if (!decls.every(w => w === width))
    bad('decl-disagree', `eng_p[] declarations disagree: ${decls.join(' vs ')} — the Instrument bank and the Voice must be the same width`)
  else if (decls.length) notes.push(`eng_p[] width = ${width} (${decls.length} declarations agree)`)

  // 2. every aux-param BOUND must equal the width. There are two: instrument_mode() and SR_ENG_TUNE.
  //    Matched on the shared shape `idx < 0 || idx >= N` so an unrelated `idx >=` elsewhere is not swept in.
  const bounds = [...sound.matchAll(/idx\s*<\s*0\s*\|\|\s*idx\s*>=\s*(\d+)/g)].map(m => +m[1])
  if (bounds.length < 2)
    bad('bound-count', `expected 2 aux-param bounds (the instrument_mode setter AND the SR_ENG_TUNE handler), found ${bounds.length} — if one was refactored away, update this lint`)
  for (const b of bounds)
    if (b !== width) bad('bound-mismatch', `an aux-param bound is \`idx >= ${b}\` but eng_p[] is ${width} wide — a MODE_* index in between is silently DROPPED`)
  if (bounds.length >= 2 && bounds.every(b => b === width)) notes.push(`${bounds.length} aux-param bounds all = ${width}`)

  // 3. the note-on COPY must cover every index (a missed one never reaches the voice)
  const copies = [...sound.matchAll(/v->eng_p\[(\d+)\]\s*=\s*ins->eng_p\[\1\]/g)].map(m => +m[1])
  const uncopied = Number.isInteger(width) ? [...Array(width).keys()].filter(i => !copies.includes(i)) : []
  if (uncopied.length)
    bad('copy-missing', `note-on does not copy eng_p[${uncopied.join(',')}] from the instrument — those params never reach the voice`)
  else if (Number.isInteger(width)) notes.push(`note-on copies all ${width} indices`)

  // 4. every MODE_* constant must be inside the width. Indices are PER-ENGINE namespaces, so reuse across
  //    engines (MODE_BOW_PIZZ 0 vs MODE_STRING_WEIGHT 0) is legal and not flagged.
  const modes = [...studio.matchAll(/^#define\s+(MODE_[A-Z0-9_]+)\s+(\d+)/gm)].map(m => ({ name: m[1], idx: +m[2] }))
  if (!modes.length) bad('no-modes', 'no MODE_* constants found in studio.h — did they move?')
  for (const m of modes)
    if (m.idx >= width) bad('mode-out-of-range', `${m.name} = ${m.idx} is outside eng_p[${width}] — it can never be stored`)
  const maxMode = modes.reduce((a, m) => Math.max(a, m.idx), -1)
  if (maxMode >= 0) notes.push(`${modes.length} MODE_* constants, highest index ${maxMode} < ${width}`)

  // 5. every MODE_* constant must be registered for docs + help (the four-places rule)
  for (const m of modes) {
    if (!docs.includes(`${m.name}:`))
      bad('mode-no-docs', `${m.name} has no studioDocs.js entry (autocomplete/hover/help will miss it)`)
    if (!shell.includes(`'${m.name}'`))
      bad('mode-no-shell', `${m.name} is not listed in shell.js sections (absent from the help tab)`)
  }

  return { width, decls, bounds, copies, modes, notes, problems }
}

// ── --selfcheck: assert the CHECKER against known answers ────────────────────────────────────
// See docs/guides/checks-and-oracles.md "Self-test the checker". Three fixture cases because the
// findings are mutually exclusive by construction — a channel with DISAGREEING declarations cannot
// also be the one demonstrating two that agree. tools/fixtures/lint-aux-params/README.md is the map.
if (has('--selfcheck')) {
  const cp = require('child_process')
  const FX = path.join(__dirname, 'fixtures', 'lint-aux-params')
  const run = (kase) => {
    const d = path.join(FX, kase)
    let raw
    try {
      raw = cp.execFileSync(process.execPath, [__filename, '--json'], {
        env: { ...process.env,
               // `.h.txt` / `.js.txt`, never `.h` / `.js`: a fixture header is never compiled, and a
               // real .h here makes clangd index it and report phantom errors at you.
               DE_AUX_SOUND_H:  path.join(d, 'sound.h.txt'),
               // Only split/ has one. The others keep everything in sound.h.txt, and the reader
               // treats a missing ctx header as empty — so those three cases are unchanged, which
               // is what makes this addition a pure extension of the fixture set.
               DE_AUX_SOUND_CTX_H: path.join(d, 'sound_ctx.h.txt'),
               DE_AUX_STUDIO_H: path.join(d, 'studio.h.txt'),
               DE_AUX_DOCS_JS:  path.join(d, 'studioDocs.js.txt'),
               DE_AUX_SHELL_JS: path.join(d, 'shell.js.txt') },
        encoding: 'utf8', maxBuffer: 1 << 24,
      })
    } catch (e) { raw = e.stdout }
    return JSON.parse(raw)
  }

  const broken = run('broken'), clean = run('clean'), stale = run('stale'), split = run('split')
  const kinds = (g) => g.problems.map(p => p.kind)
  const saw = (g, kind) => kinds(g).includes(kind)
  const about = (g, kind, name) => g.problems.some(p => p.kind === kind && p.msg.includes(name))

  const T = []
  const t = (n, ok) => T.push({ n, ok })

  // ── broken/ — the real bug shape. Each MODE_* carries exactly one defect.
  t('broken: parses the width at all  [broken-regex guard]', broken.width === 4)
  t('broken: a bound narrower than the width is reported', saw(broken, 'bound-mismatch'))
  t('broken: ...and names the narrow bound, not the correct one',
    about(broken, 'bound-mismatch', 'idx >= 3'))
  t('broken: an index the note-on copy skips is reported', about(broken, 'copy-missing', 'eng_p[2]'))
  t('broken: a MODE_* past the width is reported', about(broken, 'mode-out-of-range', 'MODE_FIX_TOOBIG'))
  t('broken: a MODE_* missing from studioDocs.js is reported', about(broken, 'mode-no-docs', 'MODE_FIX_NODOCS'))
  t('broken: a MODE_* missing from shell.js is reported', about(broken, 'mode-no-shell', 'MODE_FIX_NOSHELL'))
  t('broken: a fully-registered in-range MODE_* is silent  [noise guard]',
    !broken.problems.some(p => p.msg.includes('MODE_FIX_OK')))

  // ── clean/ — the cry-wolf guard, and the one it hides behind: a BLIND pass prints the same "✓".
  t('clean: a correctly-widened channel reports nothing  [cry-wolf guard]', clean.problems.length === 0)
  t('clean: ...and got there having actually SEEN the source  [blind-pass guard]',
    clean.width === 4 && clean.bounds.length === 2 && clean.copies.length === 4 && clean.modes.length === 3)
  t('clean: two engines reusing aux index 0 is legal  [exempt-class guard]',
    clean.modes.filter(m => m.idx === 0).length === 2 && clean.problems.length === 0)

  // ── stale/ — parser rot. Every one of these passes VACUOUSLY without its explicit guard.
  t('stale: disagreeing eng_p[] declarations are reported', saw(stale, 'decl-disagree'))
  t('stale: a vanished second bound is reported, not passed vacuously  [rot guard]',
    saw(stale, 'bound-count'))
  t('stale: a vanished MODE_* roster is reported, not passed vacuously  [rot guard]',
    saw(stale, 'no-modes') && stale.modes.length === 0)

  // ── split/ — THE REGRESSION THIS LINT ACTUALLY SUFFERED (2026-08-14). The per-instance refactor
  // moved both eng_p[] declarations out of sound.h into the generated sound_ctx.h. The lint read
  // only sound.h, found zero, and reported three findings on a healthy engine for weeks — while
  // repo-doctor showed green, because it ran --selfcheck (a fixture) and never the lint itself.
  // A blind lint and a healthy engine are indistinguishable from the outside, so pin BOTH halves:
  // it must find the declarations across the split, AND still say nothing about a correct channel.
  t('split: declarations living in sound_ctx.h are found  [the 2026-08-14 blindness]',
    split.decls.length === 2 && split.width === 4)
  t('split: ...and the channel is then judged clean, not merely quiet  [blind-pass guard]',
    split.problems.length === 0 && split.bounds.length === 2 && split.copies.length === 4)
  t('split: a case with NO ctx header is unaffected  [the other three cases still hold]',
    clean.width === 4 && clean.decls.length === 2)

  const failed = T.filter(x => !x.ok)
  for (const x of T) console.log(`  ${x.ok ? '\x1b[32m✓\x1b[0m' : '\x1b[31m✗\x1b[0m'} ${x.n}`)
  console.log(failed.length
    ? `\x1b[31mlint-aux-params --selfcheck FAILED\x1b[0m — ${failed.length} of ${T.length} expectations broken`
    : `lint-aux-params --selfcheck: ${T.length}/${T.length} known answers correct`)
  process.exit(failed.length ? 1 : 0)
}

// ── report ───────────────────────────────────────────────────────────────────────────────────
const { notes, problems, ...seen } = analyze()

if (has('--json')) {
  console.log(JSON.stringify({ ...seen, notes, problems }, null, 2))
  process.exit(problems.length ? 1 : 0)
}

if (problems.length) {
  console.log(`AUX PARAM REGISTRATION — ${problems.length} problem(s):`)
  for (const p of problems) console.log(`  ✗ ${p.msg}`)
  process.exit(1)
}
if (!has('--quiet')) {
  console.log('AUX PARAM REGISTRATION ✓')
  for (const n of notes) console.log(`  · ${n}`)
}
process.exit(0)
