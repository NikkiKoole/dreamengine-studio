// patch-match.cjs — parse what `pm` already writes (patches.txt + cand-N.wav).
//
// The editor drop UI (option A of docs/design/patch-matching-cart.md) is a TOOL
// seam, not an engine one: Electron spawns `build/pm`, this file turns the
// existing report into something the renderer can audition and paste. ADR-0006.
//
//   node editor/electron/patch-match.cjs --selfcheck
//
'use strict'

const fs = require('fs')
const path = require('path')
const os = require('os')

const ANSI = /\x1b\[[0-9;]*m/g

function stripAnsi(s) {
  return String(s || '').replace(ANSI, '')
}

// pm prints "stage 1/2/3" then "candidates" then "wrote …", and WITHIN a stage a progress line
// ("racing  53%  of 18 engines  22s"). The stage headers are ANCHORS on the bar and the in-stage
// percentage INTERPOLATES between the current anchor and the next, so the bar moves continuously
// instead of jumping and then sitting still for minutes.
//
// ⚠ The anchors have to stay in the order pm prints them, and each stage's span is the distance to
// the following one. Getting that wrong does not error: the bar just goes backwards.
const PM_ANCHORS = [0.12, 0.42, 0.72, 0.92, 1]

function progressFromLine(raw) {
  const line = stripAnsi(raw).replace(/^\s+/, '')
  if (/^stage 1\b/i.test(line)) return { stage: 1, pct: PM_ANCHORS[0], label: 'stage 1 — engine race' }
  if (/^stage 2\b/i.test(line)) return { stage: 2, pct: PM_ANCHORS[1], label: 'stage 2 — voice refine' }
  if (/^stage 3\b/i.test(line)) return { stage: 3, pct: PM_ANCHORS[2], label: 'stage 3 — fx fit' }
  if (/^candidates\b/i.test(line)) return { stage: 4, pct: PM_ANCHORS[3], label: 'writing candidates' }
  if (/^wrote\b/i.test(line)) return { stage: 5, pct: PM_ANCHORS[4], label: 'done' }
  return null
}

// The in-stage line. `stage` is whatever the last header said, so this is only meaningful after one
// (a progress line with no stage yet is ignored rather than guessed at).
const PM_INSTAGE = /^(racing|refining|fitting fx)\s+(\d+)%\s+of\s+(\d+)\s+(\S+)\s+(\d+)s$/i

function inStageProgress(raw, stage) {
  if (!stage || stage < 1 || stage > 3) return null
  const m = PM_INSTAGE.exec(stripAnsi(raw).trim())
  if (!m) return null
  const within = Math.max(0, Math.min(1, parseInt(m[2], 10) / 100))
  const from = PM_ANCHORS[stage - 1]
  const to = PM_ANCHORS[stage]
  return {
    stage,
    pct: from + (to - from) * within,
    label: `${m[1].toLowerCase()} ${m[2]}% of ${m[3]} ${m[4]}  ${m[5]}s`,
  }
}

// Split patches.txt on the candidate banners pm writes. Each block is the
// pasteable instrument() snippet (plus the hit() the CLI includes so you can
// hear it immediately). Header lines above candidate 1 are kept as `header`.
function parsePatchesTxt(text) {
  const src = String(text || '')
  const re = /^\/\/ ---- candidate (\d+):\s+(\S+)\s+voice\s+([0-9.]+)\s+->\s+fx\s+([0-9.]+)\s*$/gm
  const hits = []
  let m
  while ((m = re.exec(src))) {
    hits.push({
      n: parseInt(m[1], 10),
      engine: m[2],
      voice: parseFloat(m[3]),
      fx: parseFloat(m[4]),
      at: m.index,
      end: m.index + m[0].length,
    })
  }
  if (!hits.length) return { header: src.trim(), candidates: [] }
  const header = src.slice(0, hits[0].at).trim()
  const candidates = hits.map((h, i) => {
    const from = h.end
    const to = i + 1 < hits.length ? hits[i + 1].at : src.length
    const snippet = src.slice(from, to).replace(/^\n+/, '').replace(/\s+$/, '')
    return { n: h.n, engine: h.engine, voice: h.voice, fx: h.fx, snippet }
  })
  return { header, candidates }
}

function wavDataUrl(abs) {
  const buf = fs.readFileSync(abs)
  return 'data:audio/wav;base64,' + buf.toString('base64')
}

// Read a finished pm --out dir. Missing cand-N.wav is not fatal (paste still
// works); a missing or empty patches.txt is.
function collectResults(outdir) {
  const dir = path.resolve(outdir)
  const report = path.join(dir, 'patches.txt')
  if (!fs.existsSync(report)) return { ok: false, error: 'pm finished but wrote no patches.txt' }
  const parsed = parsePatchesTxt(fs.readFileSync(report, 'utf8'))
  if (!parsed.candidates.length) return { ok: false, error: 'pm wrote patches.txt with no candidates' }
  const targetPath = path.join(dir, 'target.wav')
  const candidates = parsed.candidates.map(c => {
    const wavPath = path.join(dir, `cand-${c.n}.wav`)
    const hasWav = fs.existsSync(wavPath)
    return {
      ...c,
      wavPath: hasWav ? wavPath : null,
      wavDataUrl: hasWav ? wavDataUrl(wavPath) : null,
    }
  })
  return {
    ok: true,
    dir,
    header: parsed.header,
    targetWav: fs.existsSync(targetPath) ? wavDataUrl(targetPath) : null,
    candidates,
  }
}

// ── THE BENCH REGION ────────────────────────────────────────────────────────────────────────────
// Turn a parsed run into the block that lives between `// de:patch-slots begin` and `... end` in
// tools/carts/patchbench.c. This is the whole answer to §7 of docs/design/patch-matching-cart.md:
// a search result has ONE legal home, written by a generator, instead of being pasted at whatever
// cursor happened to be in the editor.
//
// The snippet pm writes ends with a `hit(...)` so the CLI's output is audible by itself. Here that
// line is DROPPED: the bench decides when to play and at what pitch, and a stray hit() inside the
// apply function would fire a note every time you touched a knob.
const SLOT_BEGIN = '// de:patch-slots begin'
const SLOT_END = '// de:patch-slots end'

// Same roster as pmpatch.h PM_ENGINE / PM_ENGINE_NAME. Kept here so a snippet can become a
// PmPatch without compiling C — the bench needs the vector to breed, and older patches.txt
// files only have the printed instrument() calls.
const PM_ENGINE_ID = {
  SQUARE: 0, SAW: 1, TRI: 2, SINE: 4,
  PLUCK: 16, MALLET: 17, FM: 18, ORGAN: 19, EPIANO: 20, PD: 21,
  MEMBRANE: 22, REED: 23, VOICE: 24, PIPE: 25, GUITAR: 26, PIANO: 27, BOWED: 28, BRASS: 29,
}
const FILTER_BIN = { FILTER_OFF: 0, FILTER_LOW: 1, FILTER_LADDER: 2, FILTER_BAND: 3 }
const DRIVE_BIN = { DRIVE_SOFT: 0, DRIVE_HARD: 1, DRIVE_FOLD: 2, DRIVE_ASYM: 3 }

function engineId(name) {
  const s = String(name || '').replace(/^INSTR_/, '')
  return Object.prototype.hasOwnProperty.call(PM_ENGINE_ID, s) ? PM_ENGINE_ID[s] : -1
}

function clamp01(x) { return x < 0 ? 0 : x > 1 ? 1 : x }
function unatk(ms) { return ms <= 0 ? 0 : Math.sqrt(ms / 1500) }
function undec(ms) { return ms <= 0 ? 0 : Math.sqrt(ms / 3000) }
function unsus(s) { return s / 7.999 }
function unrel(ms) { return ms <= 0 ? 0 : Math.sqrt(ms / 4000) }
function uncut(hz) { return hz <= 40 ? 0 : Math.log(hz / 40) / Math.log(400) }
function unres(r) { return r / 15 }
function unenvOct(o) { return o / 4 }
function unenvMs(ms) { return ms <= 0 ? 0 : Math.sqrt(ms / 1200) }
function unvibSemi(s) { return s <= 0 ? 0 : Math.sqrt(s / 1.5) }
function unvibHz(hz) { return (hz - 0.5) / 8 }
function uncrushBits(b) { return (16 - b) / 15 }
function uncrushRate(r) { return (r - 1) / 63 }
function unchRate(hz) { return (hz - 0.1) / 4.9 }
function untremRate(hz) { return (hz - 0.1) / 19.9 }
function unechoMs(ms) { return ms <= 1 ? 0 : Math.sqrt((ms - 1) / 800) }
function unechoFb(fb) { return fb / 0.9 }
function uneqDb(db) { return db / 24 + 0.5 }

function defaultPatch(engine) {
  const v = new Array(19).fill(0.5)
  v[3] = 0; v[4] = 0.5; v[5] = 0.9; v[6] = 0.2          // ATK DEC SUS REL
  v[7] = 0                                               // FMODE off
  v[10] = 0; v[11] = 0.3                                 // ENVAMT ENVDEC
  v[12] = 0; v[13] = 0.5; v[14] = 0                      // VIBDEP VIBRATE TREMDEP
  const f = new Array(23).fill(0)
  f[20] = f[21] = f[22] = 0.5                            // EQ flat
  return { engine, nmode: 0, v, f }
}

function parseVecLine(snippet) {
  const m = /\/\/\s*pm:vec\s+(\d+)\s+(\d+)\s+([\d.,eE+-]+)\s*\|\s*([\d.,eE+-]+)/.exec(String(snippet || ''))
  if (!m) return null
  const v = m[3].split(',').map(Number)
  const f = m[4].split(',').map(Number)
  if (v.length < 7 || f.length < 3) return null
  const p = defaultPatch(parseInt(m[1], 10))
  p.nmode = parseInt(m[2], 10)
  for (let i = 0; i < v.length && i < p.v.length; i++) p.v[i] = clamp01(v[i])
  for (let i = 0; i < f.length && i < p.f.length; i++) p.f[i] = clamp01(f[i])
  return p
}

// Reconstruct a PmPatch from the printed snippet. Prefer an exact // pm:vec line
// (new pm writes); otherwise invert the mapping curves. Lossy on the integer
// rounded ms/Hz, close enough to breed from.
function snippetToPatch(snippet, engineName) {
  const exact = parseVecLine(snippet)
  if (exact) return exact
  const p = defaultPatch(engineId(engineName))
  const src = String(snippet || '')
  const inst = /instrument\s*\(\s*\d+\s*,\s*(INSTR_[A-Z0-9]+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\)/.exec(src)
  if (inst) {
    const id = engineId(inst[1])
    if (id >= 0) p.engine = id
    p.v[3] = clamp01(unatk(+inst[2]))
    p.v[4] = clamp01(undec(+inst[3]))
    p.v[5] = clamp01(unsus(+inst[4]))
    p.v[6] = clamp01(unrel(+inst[5]))
  }
  const mac = /instrument_harmonics\s*\(\s*\d+\s*,\s*([0-9.]+)f?\s*\).*instrument_timbre\s*\(\s*\d+\s*,\s*([0-9.]+)f?\s*\).*instrument_morph\s*\(\s*\d+\s*,\s*([0-9.]+)f?/.exec(src)
  if (mac) { p.v[0] = clamp01(+mac[1]); p.v[1] = clamp01(+mac[2]); p.v[2] = clamp01(+mac[3]) }
  const fil = /instrument_filter\s*\(\s*\d+\s*,\s*(FILTER_[A-Z]+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\)/.exec(src)
  if (fil) {
    const b = FILTER_BIN[fil[1]]
    if (b !== undefined) p.v[7] = (b + 0.5) / 4
    p.v[8] = clamp01(uncut(+fil[2]))
    p.v[9] = clamp01(unres(+fil[3]))
  }
  const env = /instrument_env\s*\(\s*\d+\s*,\s*0\s*,\s*ENV_CUTOFF_OCT\s*,\s*0\s*,\s*(-?\d+)\s*,\s*([0-9.]+)f?/.exec(src)
  if (env) { p.v[11] = clamp01(unenvMs(+env[1])); p.v[10] = clamp01(unenvOct(+env[2])) }
  const vib = /instrument_lfo\s*\(\s*\d+\s*,\s*0\s*,\s*LFO_PITCH\s*,\s*([0-9.]+)f?\s*,\s*([0-9.]+)f?/.exec(src)
  if (vib) { p.v[13] = clamp01(unvibHz(+vib[1])); p.v[12] = clamp01(unvibSemi(+vib[2])) }
  const trm = /instrument_lfo\s*\(\s*\d+\s*,\s*1\s*,\s*LFO_VOLUME\s*,\s*([0-9.]+)f?\s*,\s*([0-9.]+)f?/.exec(src)
  if (trm) { p.v[13] = clamp01(unvibHz(+trm[1])); p.v[14] = clamp01(+trm[2]) }
  const modes = [...src.matchAll(/instrument_mode\s*\(\s*\d+\s*,\s*\d+\s*,\s*([0-9.]+)f?/g)]
  p.nmode = modes.length
  modes.forEach((m, i) => { if (i < 4) p.v[15 + i] = clamp01(+m[1]) })
  const drv = /instrument_drive\s*\(\s*\d+\s*,\s*([0-9.]+)f?/.exec(src)
  if (drv) p.f[0] = clamp01(+drv[1])
  const drm = /instrument_drive_mode\s*\(\s*\d+\s*,\s*(DRIVE_[A-Z]+)/.exec(src)
  if (drm && DRIVE_BIN[drm[1]] !== undefined) p.f[1] = (DRIVE_BIN[drm[1]] + 0.5) / 4
  const tape = /instrument_tape\s*\(\s*\d+\s*,\s*([0-9.]+)f?\s*,\s*([0-9.]+)f?\s*,\s*([0-9.]+)f?/.exec(src)
  if (tape) { p.f[2] = clamp01(+tape[1]); p.f[3] = clamp01(+tape[2]); p.f[4] = clamp01(+tape[3]) }
  const cr = /instrument_crush\s*\(\s*\d+\s*,\s*([0-9.]+)f?\s*,\s*([0-9.]+)f?\s*,\s*([0-9.]+)f?/.exec(src)
  if (cr) { p.f[5] = clamp01(uncrushBits(+cr[1])); p.f[6] = clamp01(uncrushRate(+cr[2])); p.f[7] = clamp01(+cr[3]) }
  const ch = /instrument_chorus\s*\(\s*\d+\s*,\s*([0-9.]+)f?\s*,\s*([0-9.]+)f?\s*,\s*([0-9.]+)f?/.exec(src)
  if (ch) { p.f[8] = clamp01(unchRate(+ch[1])); p.f[9] = clamp01(+ch[2]); p.f[10] = clamp01(+ch[3]) }
  const echo = /echo\s*\(\s*(-?\d+)\s*,\s*([0-9.]+)f?\s*,\s*([0-9.]+)f?/.exec(src)
  const esnd = /instrument_echo\s*\(\s*\d+\s*,\s*([0-9.]+)f?/.exec(src)
  if (echo) { p.f[13] = clamp01(unechoMs(+echo[1])); p.f[14] = clamp01(unechoFb(+echo[2])); p.f[15] = clamp01(+echo[3]) }
  if (esnd) p.f[16] = clamp01(+esnd[1])
  const rvb = /reverb\s*\(\s*([0-9.]+)f?\s*,\s*([0-9.]+)f?/.exec(src)
  const rsnd = /instrument_reverb\s*\(\s*\d+\s*,\s*([0-9.]+)f?/.exec(src)
  if (rvb) { p.f[17] = clamp01(+rvb[1]); p.f[18] = clamp01(+rvb[2]) }
  if (rsnd) p.f[19] = clamp01(+rsnd[1])
  const eq = /instrument_eq\s*\(\s*\d+\s*,\s*([0-9.-]+)f?\s*,\s*([0-9.-]+)f?\s*,\s*([0-9.-]+)f?/.exec(src)
  if (eq) { p.f[20] = clamp01(uneqDb(+eq[1])); p.f[21] = clamp01(uneqDb(+eq[2])); p.f[22] = clamp01(uneqDb(+eq[3])) }
  return p
}

function fnum(x) { return `${Number(x).toFixed(5)}f` }

function emitFillSeeds(patches) {
  const L = ['static PmPatch PB_SEED[PB_N];', 'static void pb_fill_seeds(void) {']
  patches.forEach((p, i) => {
    L.push(`    pm_patch_default(&PB_SEED[${i}], ${p.engine});`)
    L.push(`    PB_SEED[${i}].nmode = ${p.nmode};`)
    L.push(`    { static const float v[PM_NV] = { ${p.v.map(fnum).join(', ')} }; memcpy(PB_SEED[${i}].v, v, sizeof v); }`)
    L.push(`    { static const float f[PM_NF] = { ${p.f.map(fnum).join(', ')} }; memcpy(PB_SEED[${i}].f, f, sizeof f); }`)
  })
  L.push('}')
  return L
}

function renderSlots(parsed, runName) {
  const cands = (parsed.candidates || []).slice(0, 8)
  if (!cands.length) throw new Error('no candidates to write')
  const short = (e) => String(e || '').replace(/^INSTR_/, '')
  const seeds = cands.map(c => snippetToPatch(c.snippet, c.engine))
  const L = []
  L.push(SLOT_BEGIN)
  L.push(`// GENERATED from build/patch-match/${runName}/patches.txt — edit the cart, not this block.`)
  L.push(`#define PB_RUN  ${JSON.stringify(runName)}`)
  L.push(`#define PB_N    ${cands.length}`)
  L.push(`static const char *PB_NAME[PB_N] = { ${cands.map(c => JSON.stringify(short(c.engine))).join(', ')} };`)
  L.push(`static const float PB_LOSS[PB_N] = { ${cands.map(c => `${Number(c.fx).toFixed(5)}f`).join(', ')} };`)
  L.push(...emitFillSeeds(seeds))
  L.push('static void pb_apply(int i, int slot) {')
  L.push('    switch (i) {')
  cands.forEach((c, i) => {
    const last = i === cands.length - 1
    L.push(last ? '    default:' : `    case ${i}:`)
    for (const raw of String(c.snippet || '').split('\n')) {
      const line = raw.trim()
      if (!line || /^hit\s*\(/.test(line)) continue     // the bench owns when a note sounds
      if (/^\/\/\s*pm:vec\b/.test(line)) continue       // the vector lives in PB_SEED, not in apply
      // Every emitted call targets slot 5; the bench passes its own slot in, so rewrite the first
      // argument. GLOBAL, because pm puts three calls on one line ("harmonics(5,..) timbre(5,..)
      // morph(5,..)") and a first-match-only rewrite leaves two of them hardcoded: it still works
      // while the bench happens to use slot 5, and silently plays the wrong slot the day it does not.
      // The `instrument` prefix is load-bearing: echo(251,...) and reverb(0.96,...) are MASTER calls
      // whose first argument is a time and a size, not a slot, and must be left exactly alone.
      L.push('        ' + line.replace(/\b(instrument[a-z_]*)\s*\(\s*5\s*,/g, '$1(slot,'))
    }
    L.push('        break;')
  })
  L.push('    }')
  L.push('}')
  L.push(SLOT_END)
  return L.join('\n')
}

// Replace the region in the cart source. Refuses rather than guesses: no markers, markers in the
// wrong order, or more than one of either means somebody edited the cart by hand and a blind
// rewrite would eat their work.
function spliceSlots(source, region) {
  const nb = source.split(SLOT_BEGIN).length - 1
  const ne = source.split(SLOT_END).length - 1
  if (nb !== 1 || ne !== 1) throw new Error(`patchbench.c must hold exactly one de:patch-slots marker pair (found ${nb} begin, ${ne} end)`)
  const a = source.indexOf(SLOT_BEGIN)
  const b = source.indexOf(SLOT_END)
  if (b < a) throw new Error('de:patch-slots end comes before begin')
  return source.slice(0, a) + region + source.slice(b + SLOT_END.length)
}

function failMsg(code, stderr, stdout) {
  const tail = stripAnsi((stderr || '') + '\n' + (stdout || ''))
    .split('\n')
    .map(s => s.trimEnd())
    .filter(Boolean)
    .slice(-12)
    .join('\n')
  if (code === null) return tail ? `pm timed out.\n${tail}` : 'pm timed out.'
  if (code === 1 && /only .* after onset trim/i.test(tail))
    return `that WAV is too short for the analyser.\n${tail}`
  if (code === 2 && /usage:/i.test(tail)) return `pm refused the arguments.\n${tail}`
  return tail ? `pm exited ${code}.\n${tail}` : `pm exited ${code}.`
}

function selfcheck() {
  let n = 0, failed = 0
  const t = (name, cond) => {
    n++
    if (cond) console.log(`  ✓ ${name}`)
    else { failed++; console.log(`  ✗ ${name}`) }
  }

  t('stripAnsi drops colour', stripAnsi('\x1b[1mstage 1\x1b[0m') === 'stage 1')
  t('stage 1 progress', progressFromLine('  \x1b[1mstage 1\x1b[0m  engine race (18 engines)')?.pct === 0.12)
  t('stage 2 progress', progressFromLine('  stage 2  voice refine (top 3 engines x 3 seeds)')?.stage === 2)
  t('stage 3 progress', progressFromLine('  stage 3  fx fit (voice frozen)')?.stage === 3)
  t('candidates progress', progressFromLine('  candidates  (42s)')?.pct === 0.92)
  t('wrote progress', progressFromLine('  wrote build/patch-match/  (target.wav, cand-1..8.wav, patches.txt)')?.pct === 1)
  t('noise is not progress', progressFromLine('    1. INSTR_SAW      0.12345') === null)

  // ── the in-stage bar. These lines are REAL pm output, pasted from a run, not invented: the
  // failure this whole selfcheck cannot otherwise see is a fixture written to match the parser.
  t('in-stage line is not a stage header', progressFromLine('    racing  53%  of 18 engines  22s') === null)
  t('racing interpolates inside stage 1',
    Math.abs(inStageProgress('    racing  50%  of 18 engines  22s', 1).pct - (0.12 + 0.30 * 0.5)) < 1e-9)
  t('refining interpolates inside stage 2',
    Math.abs(inStageProgress('    refining  50%  of 9 voices  40s', 2).pct - (0.42 + 0.30 * 0.5)) < 1e-9)
  t('fitting fx interpolates inside stage 3',
    Math.abs(inStageProgress('    fitting fx  50%  of 8 candidates  60s', 3).pct - (0.72 + 0.20 * 0.5)) < 1e-9)
  t('0% sits exactly on the stage anchor', inStageProgress('    racing   0%  of 18 engines  0s', 1).pct === 0.12)
  t('100% reaches the NEXT anchor, never past it',
    Math.abs(inStageProgress('    racing 100%  of 18 engines  37s', 1).pct - 0.42) < 1e-9)
  t('the label names the stage size, not a completion count',
    /53% of 18 engines/.test(inStageProgress('    racing  53%  of 18 engines  22s', 1).label))
  // the two ways it must REFUSE rather than guess
  t('no stage yet → no in-stage progress', inStageProgress('    racing  53%  of 18 engines  22s', 0) === null)
  t('a stage with no bar (candidates) → null', inStageProgress('    racing  53%  of 18 engines  22s', 4) === null)
  t('an engine-table row is not a bar', inStageProgress('    1. INSTR_SAW      0.12345', 1) === null)
  t('a truncated bar line is not a bar', inStageProgress('    racing  53%', 1) === null)

  const fixture = fs.readFileSync(path.join(__dirname, '../../tools/fixtures/patch-match/patches.txt'), 'utf8')
  const parsed = parsePatchesTxt(fixture)
  t('fixture has 3 candidates', parsed.candidates.length === 3)
  t('candidate 1 is PIANO', parsed.candidates[0].engine === 'INSTR_PIANO' && parsed.candidates[0].n === 1)
  t('voice/fx numbers', parsed.candidates[0].voice === 0.12345 && parsed.candidates[0].fx === 0.09876)
  t('snippet starts with instrument(', parsed.candidates[0].snippet.startsWith('    instrument(5, INSTR_PIANO'))
  t('snippet keeps hit()', /hit\(60, 5, 5, 1000\);/.test(parsed.candidates[0].snippet))
  t('candidate 2 has tape', /instrument_tape/.test(parsed.candidates[1].snippet))
  t('header keeps the target line', /piano\.wav/.test(parsed.header))
  t('empty text → no candidates', parsePatchesTxt('').candidates.length === 0)
  t('banner-less text → no candidates', parsePatchesTxt('// just a comment\n').candidates.length === 0)

  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'de-pm-'))
  try {
    t('missing dir fails', collectResults(tmp).ok === false)
    fs.writeFileSync(path.join(tmp, 'patches.txt'), fixture)
    const emptyWav = Buffer.alloc(44)   // header-sized placeholder; we only check the file exists
    fs.writeFileSync(path.join(tmp, 'cand-1.wav'), emptyWav)
    const got = collectResults(tmp)
    t('collectResults ok', got.ok === true)
    t('cand 1 has a data URL', typeof got.candidates[0].wavDataUrl === 'string' && got.candidates[0].wavDataUrl.startsWith('data:audio/wav'))
    t('cand 2 wav is optional', got.candidates[1].wavDataUrl === null)
  } finally {
    fs.rmSync(tmp, { recursive: true, force: true })
  }

  // ── the region writer. It REWRITES A SOURCE FILE, so it is the most dangerous thing in here
  // and gets the most assertions, in both directions.
  {
    const fake = { candidates: [
      { n: 1, engine: 'INSTR_PIPE', voice: 0.3, fx: 0.17486, snippet:
        '    instrument(5, INSTR_PIPE, 137, 0, 6, 372);\n' +
        '    instrument_harmonics(5, 0.022f);  instrument_timbre(5, 0.083f);  instrument_morph(5, 0.025f);\n' +
        '    echo(251, 0.420f, 0.034f);  instrument_echo(5, 0.249f);\n' +
        '    reverb(0.960f, 0.409f);  instrument_reverb(5, 0.092f);\n' +
        '    hit(60, 5, 5, 1000);' },
      { n: 2, engine: 'INSTR_FM', voice: 0.2, fx: 0.177, snippet:
        '    instrument(5, INSTR_FM, 10, 80, 6, 180);\n    hit(60, 5, 5, 1000);' },
    ] }
    const r = renderSlots(fake, 'ambitone')
    t('region names the run', /#define PB_RUN\s+"ambitone"/.test(r))
    t('region counts the candidates', /#define PB_N\s+2\b/.test(r))
    t('engine names lose the INSTR_ prefix', /"PIPE", "FM"/.test(r))
    t('losses are the FX losses', /0\.17486f, 0\.17700f/.test(r))
    // THE ONE THAT MATTERS: pm puts three calls on ONE line, and a first-match-only rewrite leaves
    // two of them pointing at a hardcoded slot 5. It still works while the bench uses slot 5 and
    // breaks silently the day it does not. This caught exactly that bug on the first real run.
    t('EVERY instrument_ call on a line is re-slotted', !/instrument[a-z_]*\(5,/.test(r))
    t('and all three of a triple line moved', /instrument_harmonics\(slot,.*instrument_timbre\(slot,.*instrument_morph\(slot,/.test(r))
    // …and the opposite: a MASTER call's first argument is a time or a size, never a slot.
    t('echo() is left alone (master, not a slot)', /echo\(251, 0\.420f, 0\.034f\);/.test(r))
    t('reverb() is left alone (master, not a slot)', /reverb\(0\.960f, 0\.409f\);/.test(r))
    t('the per-slot echo send IS re-slotted', /instrument_echo\(slot,/.test(r))
    t('hit() is dropped (the bench decides when a note sounds)', !/\bhit\(/.test(r))
    t('the last case is default (so the switch is total)', /    default:/.test(r) && (r.split('case ').length - 1) === 1)
    t('no candidates → throws rather than writing an empty switch',
      (() => { try { renderSlots({ candidates: [] }, 'x'); return false } catch { return true } })())
    t('region writes a seed vector for breeding', /static PmPatch PB_SEED\[PB_N\]/.test(r) && /pb_fill_seeds\(/.test(r))
    t('seed 0 is PIPE (engine 25)', /pm_patch_default\(&PB_SEED\[0\], 25\)/.test(r))
    t('seed 1 is FM (engine 18)', /pm_patch_default\(&PB_SEED\[1\], 18\)/.test(r))
    t('pm:vec comments are not copied into pb_apply', !/pm:vec/.test(r.split('pb_apply')[1] || ''))

    const piano = snippetToPatch(fake.candidates[0].snippet, 'INSTR_PIPE')
    t('inverse seed keeps PIPE', piano.engine === 25)
    t('inverse seed recovered the printed macros',
      Math.abs(piano.v[0] - 0.022) < 1e-6 && Math.abs(piano.v[1] - 0.083) < 1e-6)
    t('inverse seed recovered ADSR attack', Math.abs(piano.v[3] - Math.sqrt(137 / 1500)) < 1e-6)
    t('inverse seed recovered echo send', Math.abs(piano.f[16] - 0.249) < 1e-6)
    t('inverse seed recovered reverb send', Math.abs(piano.f[19] - 0.092) < 1e-6)

    const vec = snippetToPatch(
      '    // pm:vec 18 0 0.15500,0.40000,0.20000,0.10000,0.50000,0.90000,0.20000,0.00000,0.50000,0.50000,0.00000,0.30000,0.00000,0.50000,0.00000,0.50000,0.50000,0.50000,0.50000 | 0.00000,0.00000,0.00000,0.00000,0.00000,0.00000,0.00000,0.00000,0.00000,0.00000,0.00000,0.00000,0.00000,0.00000,0.00000,0.00000,0.00000,0.00000,0.00000,0.00000,0.50000,0.50000,0.50000\n' +
      '    instrument(5, INSTR_FM, 10, 80, 6, 180);\n',
      'INSTR_SAW')
    t('pm:vec wins over the instrument() engine', vec.engine === 18 && Math.abs(vec.v[0] - 0.155) < 1e-6)
    t('pm:vec keeps the printed ADSR from being inverse-mapped', Math.abs(vec.v[3] - 0.1) < 1e-6)

    const cart = `head\n${SLOT_BEGIN}\nold junk\n${SLOT_END}\ntail`
    const out = spliceSlots(cart, r)
    t('splice keeps what is outside the markers', out.startsWith('head\n') && out.endsWith('\ntail'))
    t('splice removes the old body', !/old junk/.test(out))
    t('splice is idempotent', spliceSlots(out, r) === out)
    const refuses = (src) => { try { spliceSlots(src, r); return false } catch { return true } }
    t('splice REFUSES a file with no markers', refuses('nothing here'))
    t('splice REFUSES a doubled marker pair', refuses(`${SLOT_BEGIN}\nx\n${SLOT_END}\n${SLOT_BEGIN}\ny\n${SLOT_END}`))
    t('splice REFUSES a lone begin', refuses(`${SLOT_BEGIN}\nx`))
    t('splice REFUSES end-before-begin', refuses(`${SLOT_END}\nx\n${SLOT_BEGIN}`))
  }

  t('timeout message names timeout', /timed out/.test(failMsg(null, '', 'stage 2')))
  t('short-target message is specific', /too short/.test(failMsg(1, 'pm: only 0.100s after onset trim; the 2048-point scale needs 0.058s\n', '')))

  console.log(failed ? `\n${failed}/${n} failed` : `\n${n}/${n} ok`)
  return failed === 0
}

if (require.main === module && process.argv.includes('--selfcheck')) {
  process.exit(selfcheck() ? 0 : 1)
}

// `--load <run>` — the same thing the editor's "open in bench" button does, from a terminal.
// One code path for both, so the button and the CLI cannot drift.
if (require.main === module && process.argv.includes('--load')) {
  const run = process.argv[process.argv.indexOf('--load') + 1]
  const ROOT = path.join(__dirname, '../..')
  if (!run) { console.error('usage: patch-match.cjs --load <run>   (a directory under build/patch-match/)'); process.exit(2) }
  const dir = path.join(ROOT, 'build', 'patch-match', run)
  const got = collectResults(dir)
  if (!got.ok) { console.error(`pm: ${got.error}`); process.exit(1) }
  const cart = path.join(ROOT, 'tools', 'carts', 'patchbench.c')
  const src = fs.readFileSync(cart, 'utf8')
  fs.writeFileSync(cart, spliceSlots(src, renderSlots(got, run)))
  console.log(`patchbench.c loaded with ${got.candidates.length} candidate(s) from ${run}`)
  process.exit(0)
}

module.exports = {
  stripAnsi,
  renderSlots,
  spliceSlots,
  SLOT_BEGIN,
  SLOT_END,
  progressFromLine,
  inStageProgress,
  parsePatchesTxt,
  collectResults,
  failMsg,
  wavDataUrl,
  snippetToPatch,
  engineId,
}
