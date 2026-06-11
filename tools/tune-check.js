#!/usr/bin/env node
// tune-check.js — does each synth engine play IN TUNE? wav-analyze.js measures levels;
// this measures PITCH. It renders tools/carts/tunecheck.c (a sweep of every non-standard
// engine across four octaves of A), reads the trace to learn what each note SHOULD be,
// detects the actual fundamental with YIN, and reports the error in cents.
//
//   node tools/tune-check.js                 render the sweep + full report (DEFAULT macros)
//   node tools/tune-check.js --json          machine-readable
//   node tools/tune-check.js --keep          keep the rendered WAV/trace (build/.tune/)
//   node tools/tune-check.js --quiet         exit 1 if any note is out of tune (CI gate)
//   node tools/tune-check.js <file.wav> --note <midi>   measure ONE wav against a note
//
//   # RECIPE mode — check ONE engine at the macros a CART actually uses, across a range.
//   # This is the one to run before shipping a PIPE/REED/etc. voice: the default sweep tests
//   # as-shipped macros (all 0), but the modeled engines' tuning DEPENDS on the macros (PIPE's
//   # in-tune range moves with the morph/embouchure macro). e.g. air's flute:
//   node tools/tune-check.js --engine PIPE --macros 0,0.38,0.70 --range 48-90 [--step 2]
//
// "cents" = 1200·log2(measured/expected); 100 cents = one semitone. SINE is rendered
// first as a control — it's mathematically exact, so a non-zero SINE reading means the
// MEASUREMENT is off, not the engine. Tuning reference is A440 (studio.h sound_midi_to_freq).
// Pitch math + the harness contract: see the header of tools/carts/tunecheck.c.

const fs = require('fs')
const path = require('path')
const { spawnSync } = require('child_process')

const ROOT = path.resolve(__dirname, '..')

// thresholds (cents). well-tuned acoustic instruments sit inside a few cents; physical
// models drift more. flag at a comma's worth, scream past a quarter-tone.
const WARN_CENTS = 12
const BAD_CENTS  = 35

// engine id → label, mirrors the INSTR_* block in runtime/studio.h
const ENGINE_NAMES = {
  0: 'SQUARE', 1: 'SAW', 2: 'TRI', 3: 'NOISE', 4: 'SINE (control)',
  16: 'PLUCK  karplus-strong', 17: 'MALLET struck bar', 18: 'FM 2-op',
  19: 'ORGAN tonewheel', 20: 'EPIANO rhodes/wurli', 21: 'PD casio-cz',
  22: 'MEMBRANE drum', 23: 'REED clarinet/sax', 24: 'VOICE formant',
  25: 'PIPE flute', 26: 'GUITAR plucked+body', 27: 'PIANO stiff-string',
  28: 'BOWED violin/cello', 29: 'BRASS lip-reed',
}

const midiToFreq = (m) => 440 * Math.pow(2, (m - 69) / 12)
const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
const midiName = (m) => `${NOTE_NAMES[m % 12]}${Math.floor(m / 12) - 1}`

// ── WAV (16-bit PCM, mono or stereo→mono) ──────────────────────────────────
function readWavMono(file) {
  const b = fs.readFileSync(file)
  if (b.toString('ascii', 0, 4) !== 'RIFF' || b.toString('ascii', 8, 12) !== 'WAVE')
    throw new Error(`${file}: not a WAV`)
  let off = 12, fmt = null, data = null
  while (off + 8 <= b.length) {
    const id = b.toString('ascii', off, off + 4)
    const len = b.readUInt32LE(off + 4)
    if (id === 'fmt ') fmt = { tag: b.readUInt16LE(off + 8), ch: b.readUInt16LE(off + 10),
                               sr: b.readUInt32LE(off + 12), bits: b.readUInt16LE(off + 22) }
    if (id === 'data') data = { off: off + 8, len }
    off += 8 + len + (len & 1)
  }
  if (!fmt || !data) throw new Error(`${file}: missing fmt/data chunk`)
  if (fmt.tag !== 1 || fmt.bits !== 16) throw new Error(`${file}: expected 16-bit PCM`)
  const ch = fmt.ch, n = Math.floor(data.len / 2 / ch)
  const s = new Float64Array(n)
  for (let i = 0; i < n; i++) {
    if (ch === 1) s[i] = b.readInt16LE(data.off + i * 2) / 32768
    else s[i] = (b.readInt16LE(data.off + i * 2 * ch) + b.readInt16LE(data.off + i * 2 * ch + 2)) / 65536
  }
  return { sr: fmt.sr, s }
}

// ── pitch detection ─────────────────────────────────────────────────────────
// We KNOW the note we asked for, so pitch is measured by normalized autocorrelation
// CONSTRAINED to a tight band around known candidate pitches, never a free/wide search.
// That sidesteps both classic failure modes at once: a wide search picks subharmonics
// (a tone's autocorrelation correlates at multiples of its period, often better than at
// the period itself when the true period isn't a whole number of samples), while a
// harmonically-rich tone has a deep dip at period/2 that fools a free detector into
// reporting an octave sharp. Measuring around {expected×2, ×1, ÷2} and preferring the
// played octave (measurePitch) gives the true ±cents AND the octave offset, cleanly.

// normalized autocorrelation period search over an arbitrary lag band [loFreq, hiFreq],
// with parabolic interpolation on the peak. Sub-cent accurate on clean tones. Returns
// the strongest single periodicity (no octave games) — callers constrain the band.
function autocorr(s, start, W, sr, loFreq, hiFreq) {
  const lo = Math.max(2, Math.floor(sr / hiFreq))
  const hi = Math.min(Math.ceil(sr / loFreq), s.length - start - 1)
  if (hi <= lo + 2 || start + W + hi > s.length) return null
  const r = new Float64Array(hi + 2)
  let best = lo, bestR = -2
  for (let tau = lo; tau <= hi; tau++) {
    let ac = 0, e0 = 0, e1 = 0
    for (let j = 0; j < W; j++) { const a = s[start + j], b = s[start + j + tau]; ac += a * b; e0 += a * a; e1 += b * b }
    const rr = (e0 > 0 && e1 > 0) ? ac / Math.sqrt(e0 * e1) : 0
    r[tau] = rr
    if (rr > bestR) { bestR = rr; best = tau }
  }
  let tau = best
  if (tau > lo && tau < hi) {                      // parabolic interpolation on the peak
    const a = r[tau - 1], b = r[tau], c = r[tau + 1], denom = 2 * (2 * b - a - c)
    if (denom !== 0) tau += (c - a) / denom
  }
  return { hz: sr / tau, confidence: Math.max(0, Math.min(1, bestR)) }
}

// PRECISE measurement around ONE target pitch: autocorrelation constrained to ±18% of the
// target (≈ ±300 cents — wider than any real detune, tighter than the nearest confusing
// ratios, so it can't lock onto a harmonic or subharmonic). Per-window reads are kept only
// if confident (>0.3) AND they AGREE (tight cents spread) — that's what distinguishes a
// real tone at the target from autocorrelation noise when there's no pitch there at all.
// Returns { hz, confidence, n } or null. A WIDE global-max search is deliberately NOT used:
// with a non-integer true period a higher-multiple lag can out-correlate the fundamental,
// so a wide search picks subharmonics (octave/twelfth down) — measuring around known
// candidates instead sidesteps that entirely.
function measureAt(s, s0, s1, sr, target) {
  const windows = 8, span = s1 - s0, W = 1500
  const reads = []
  for (let k = 0; k < windows; k++) {
    const start = Math.floor(s0 + (span * k) / windows)
    let rms = 0; const N = Math.min(2048, s1 - start)
    for (let i = 0; i < N; i++) rms += s[start + i] * s[start + i]
    if (Math.sqrt(rms / Math.max(1, N)) < 0.0015) continue
    const r = autocorr(s, start, W, sr, target * 0.84, target * 1.19)
    if (r && r.confidence > 0.3 && r.hz > 20) reads.push(r)
  }
  if (reads.length < 2) return null
  const cents = reads.map(r => 1200 * Math.log2(r.hz / target)).sort((a, b) => a - b)
  const med = cents[Math.floor(cents.length / 2)]
  const mad = cents.map(c => Math.abs(c - med)).sort((a, b) => a - b)[Math.floor(cents.length / 2)]
  if (mad > 20) return null                                   // scattered → no real pitch here
  return { hz: target * Math.pow(2, med / 1200),
           confidence: reads.reduce((a, r) => a + r.confidence, 0) / reads.length, n: reads.length }
}

// fold a measured/expected ratio into { cents ∈ [-600,600], octaves } — the cents error
// within the octave plus how many whole octaves the measured pitch sits from expected.
function octaveFold(measuredHz, expectedHz) {
  const semis = 1200 * Math.log2(measuredHz / expectedHz)
  const octaves = Math.round(semis / 1200)
  return { cents: semis - octaves * 1200, octaves }
}

// median across several windows spanning [s0, s1); robust to a bad attack window or a
// decaying tail. We accept fairly low per-window confidence (≥0.3): a breathy flute or a
// chorused organ is perfectly pitched but its normalized-autocorrelation peak is well
// under 1, so a strict floor would wrongly report "no pitch". The aggregate confidence is
// returned so noisy reads are visibly less certain. (No octave detection is needed: the
// ±35% window can't land on a wrong octave, and a real octave bug would show up as
// consistent no-pitch across all four octaves of that engine.)
// Measure pitch around the MUSICAL candidates {expected×2, expected, expected/2}, then pick
// the played octave UNLESS another octave is clearly stronger. Most engines speak at the
// played pitch (octave 0 wins). Some are dominated by an octave that's expected behaviour,
// not a tuning fault — a Hammond's 16′ drawbar (octave down), a flute overblowing (octave
// up) — and those win only when their periodicity is clearly the stronger one. Returns
// { hz, confidence, n } at the chosen octave, or null if nothing is periodic anywhere.
function measurePitch(s, s0, s1, sr, expectedHz) {
  const cands = [
    { hzTarget: expectedHz * 2, oct: 1 },
    { hzTarget: expectedHz,     oct: 0 },
    { hzTarget: expectedHz / 2, oct: -1 },
  ].map(c => ({ ...c, m: measureAt(s, s0, s1, sr, c.hzTarget) })).filter(c => c.m)
  if (!cands.length) return null
  const maxConf = Math.max(...cands.map(c => c.m.confidence))
  // among candidates within 0.15 confidence of the best, prefer the smallest octave shift
  // (the played octave) — this stops a rich tone's equally-strong sub-octave from winning.
  const pick = cands.filter(c => c.m.confidence >= maxConf - 0.15)
    .sort((a, b) => Math.abs(a.oct) - Math.abs(b.oct))[0]
  return pick.m
}

// ── trace → note windows ───────────────────────────────────────────────────
// Each gated run with a constant (eng, emidi) is one note; return its [f0,f1] FRAME
// span plus the last frame seen. We index by FRAME, not by the trace's `t` seconds:
// the deterministic-clock `t` advances at a slightly different rate than the audio
// render (which is frame-locked at SR/60 samples per frame), so using `t` drifts the
// window ~18ms/sec and lands on the wrong note within a few seconds. Frame × samples-
// per-frame (derived from the WAV length) is exact.
function noteWindows(traceFile) {
  const lines = fs.readFileSync(traceFile, 'utf8').trim().split('\n')
  const notes = []
  let cur = null, lastFrame = 0
  for (const ln of lines) {
    let row; try { row = JSON.parse(ln) } catch { continue }
    const w = row.w || {}
    const gate = +w.gate, eng = +w.eng, emidi = +w.emidi, f = row.f
    lastFrame = f
    if (gate === 1 && emidi > 0) {
      if (cur && cur.eng === eng && cur.midi === emidi) cur.f1 = f
      else { if (cur) notes.push(cur); cur = { eng, midi: emidi, f0: f, f1: f } }
    } else if (cur) { notes.push(cur); cur = null }
  }
  if (cur) notes.push(cur)
  return { notes, lastFrame }
}

// ── render the sweep ───────────────────────────────────────────────────────
function renderSweep(keep) {
  const dir = path.join(ROOT, 'build', '.tune')
  fs.mkdirSync(dir, { recursive: true })
  const wav = path.join(dir, 'sweep.wav'), trace = path.join(dir, 'sweep.trace.jsonl')
  // 13 engines × 4 pitches × 62 frames ≈ 3224; over-run is harmless (trace-driven).
  runPlay('tunecheck', 3400, wav, trace)
  return { wav, trace, dir }
}

// engine name → INSTR_* id (matches runtime/studio.h)
const ENGINE_ID = {
  SQUARE: 0, SAW: 1, TRI: 2, NOISE: 3, SINE: 4, PLUCK: 16, MALLET: 17, FM: 18, ORGAN: 19,
  EPIANO: 20, PD: 21, MEMBRANE: 22, REED: 23, VOICE: 24, PIPE: 25, GUITAR: 26, PIANO: 27,
  BOWED: 28, BRASS: 29,
}

function runPlay(cart, frames, wav, trace) {
  const r = spawnSync('node',
    [path.join('tools', 'play.js'), cart, 'run', '--headless',
     '--frames', String(frames), '--trace', trace, '--wav', wav],
    { cwd: ROOT, encoding: 'utf8' })
  if (r.status !== 0) {
    process.stderr.write((r.stdout || '') + (r.stderr || ''))
    throw new Error(`render failed (play.js ${cart})`)
  }
}

// RECIPE mode: render ONE engine at specified macros across a pitch range, by generating a
// tiny cart and rendering it. This is how you verify a PIPE (or any engine) voice the way a
// CART actually uses it — the default sweep only tests as-shipped defaults (macros all 0),
// which for the modeled engines (PIPE especially) is the WORST case and what no cart uses.
function renderRecipe(opts, keep) {
  const id = ENGINE_ID[opts.engine]
  if (id === undefined) throw new Error(`unknown engine "${opts.engine}" — one of: ${Object.keys(ENGINE_ID).join(', ')}`)
  const [h, t, m] = opts.macros.map(x => x.toFixed(4))   // valid C float literals (avoid "0f")
  const pitches = []
  for (let p = opts.lo; p <= opts.hi; p += opts.step) pitches.push(p)
  const dir = path.join(ROOT, 'build', '.tune')
  fs.mkdirSync(dir, { recursive: true })
  const wav = path.join(dir, 'recipe.wav'), trace = path.join(dir, 'recipe.trace.jsonl')
  const cartName = '_tunegen'
  const cartPath = path.join(ROOT, 'tools', 'carts', `${cartName}.c`)
  const src = `#include "studio.h"
#include <stdio.h>
// GENERATED by tools/tune-check.js --engine — safe to delete.
#define NF 48
#define GAP 14
#define PER (NF+GAP)
static const int P[] = { ${pitches.join(', ')} };
#define NP ((int)(sizeof(P)/sizeof(P[0])))
static int fnum=-1, held=-1;
void init(void){
  instrument(5, ${id}, 4, 60, 7, 140);
  instrument_harmonics(5, ${h}f); instrument_timbre(5, ${t}f); instrument_morph(5, ${m}f);
}
void update(void){
  fnum++;
  int idx=fnum/PER, local=fnum%PER, gate=(idx<NP)&&(local<NF);
  int midi = idx<NP ? P[idx] : -1;
  if(idx<NP){ if(local==0) held=note_on(midi,5,7);
              else if(local==NF-1 && held>=0){ note_off(held); held=-1; } }
#ifdef DE_TRACE
  watch("note","%d",idx); watch("eng","%d",midi<0?-1:${id});
  watch("emidi","%d",midi); watch("gate","%d",gate);
#endif
}
void draw(void){ cls(0); }
`
  fs.writeFileSync(cartPath, src)
  try { runPlay(cartName, pitches.length * 62 + 120, wav, trace) }
  finally { if (!keep) fs.rmSync(cartPath, { force: true }) }
  return { wav, trace, dir }
}

// ── analysis (shared by every render path) ──────────────────────────────────
function verdict(cents) {
  const a = Math.abs(cents)
  return a > BAD_CENTS ? 'OUT OF TUNE' : a > WARN_CENTS ? 'off' : 'ok'
}
const mark = (v) => v === 'OUT OF TUNE' ? '✗' : v === 'off' ? '⚠' : v === 'no-pitch' ? '?' : '·'
const octLabel = (o) => o === 0 ? '' : `  (${Math.abs(o)} oct ${o > 0 ? 'high' : 'low'})`

function analyzeRender(wav, trace) {
  const { sr, s } = readWavMono(wav)
  const { notes, lastFrame } = noteWindows(trace)
  if (!notes.length) throw new Error('no gated notes found in trace — did the cart build?')
  const spf = s.length / (lastFrame + 1)   // samples per frame (audio is frame-locked)
  const results = []
  for (const nt of notes) {
    // measure past the onset transient (12%) up to near the end (88%) — wide enough to
    // still catch a fast-decaying high pluck before it rings out into silence.
    const a = nt.f0 * spf, b = nt.f1 * spf, span = b - a
    const expected = midiToFreq(nt.midi)
    const mres = measurePitch(s, Math.floor(a + span * 0.12), Math.floor(a + span * 0.88), sr, expected)
    const row = {
      engine: nt.eng, engineName: ENGINE_NAMES[nt.eng] || `INSTR ${nt.eng}`,
      midi: nt.midi, note: midiName(nt.midi), expectedHz: +expected.toFixed(2),
      measuredHz: null, cents: null, octaves: 0,
      confidence: mres ? +mres.confidence.toFixed(2) : 0, verdict: 'no-pitch',
    }
    if (mres) {
      const f = octaveFold(mres.hz, expected)
      row.measuredHz = +mres.hz.toFixed(2); row.cents = +f.cents.toFixed(1)
      row.octaves = f.octaves; row.verdict = verdict(f.cents)
    }
    results.push(row)
  }
  return { results, sr }
}

function printResults(results, sr, title) {
  let lastEng = null
  console.log(`${title} — ${results.length} notes @ ${sr}Hz   (warn >${WARN_CENTS}¢, bad >${BAD_CENTS}¢)\n`)
  for (const r of results) {
    if (r.engine !== lastEng) { console.log(`${r.engineName}  (id ${r.engine})`); lastEng = r.engine }
    const m = mark(r.verdict)
    const cents = r.cents === null ? '' : `${r.cents >= 0 ? '+' : ''}${r.cents.toFixed(1).padStart(5)}¢`
    const meas = r.measuredHz === null ? 'no pitch detected'
      : `meas ${String(r.measuredHz).padStart(8)} Hz   ${cents.padStart(7)}${octLabel(r.octaves)}`
    console.log(`  ${m} ${r.note.padEnd(3)} ${String(r.expectedHz).padStart(8)} Hz   ${meas}   conf ${r.confidence}`)
  }
  const bad = results.filter(r => r.verdict === 'off' || r.verdict === 'OUT OF TUNE')
    .sort((a, b) => Math.abs(b.cents) - Math.abs(a.cents))
  const transposed = results.filter(r => r.octaves !== 0)
  console.log()
  if (!bad.length) console.log('✓ every detected pitch is within tuning tolerance')
  else {
    console.log(`${bad.length} note(s) out of tune (worst first):`)
    for (const r of bad)
      console.log(`  ${mark(r.verdict)} ${r.engineName} ${r.note}  ${r.cents >= 0 ? '+' : ''}${r.cents}¢  (${r.measuredHz} vs ${r.expectedHz} Hz)${octLabel(r.octaves)}`)
  }
  if (transposed.length) {
    console.log(`\nnote: ${transposed.length} note(s) sound in a different octave than played (e.g. an organ's 16′ sub-octave drawbar) — in tune, just transposed:`)
    for (const r of transposed)
      console.log(`  · ${r.engineName} ${r.note}  ${r.measuredHz} Hz${octLabel(r.octaves)}`)
  }
}

function run(opts) {
  const { wav, trace, dir } = opts.recipe ? renderRecipe(opts.recipe, opts.keep) : renderSweep(opts.keep)
  const { results, sr } = analyzeRender(wav, trace)
  if (!opts.keep) fs.rmSync(dir, { recursive: true, force: true })
  if (opts.json) { console.log(JSON.stringify(results, null, 2)); return results }
  const title = opts.recipe
    ? `${opts.recipe.engine} @ h${opts.recipe.macros[0]} t${opts.recipe.macros[1]} m${opts.recipe.macros[2]}`
    : 'tuning sweep'
  printResults(results, sr, title)
  return results
}

// ── single-WAV mode ──────────────────────────────────────────────────────────
function single(file, midi, json) {
  const { sr, s } = readWavMono(file)
  const expected = midiToFreq(midi)
  const m = measurePitch(s, Math.floor(s.length * 0.2), Math.floor(s.length * 0.9), sr, expected)
  const out = { file, note: midiName(midi), expectedHz: +expected.toFixed(2),
    measuredHz: null, cents: null, octaves: 0,
    confidence: m ? +m.confidence.toFixed(2) : 0, verdict: 'no-pitch' }
  if (m) {
    const f = octaveFold(m.hz, expected)
    out.measuredHz = +m.hz.toFixed(2); out.cents = +f.cents.toFixed(1)
    out.octaves = f.octaves; out.verdict = verdict(f.cents)
  }
  if (json) console.log(JSON.stringify(out, null, 2))
  else {
    console.log(`${file}  vs ${out.note} (${out.expectedHz} Hz)`)
    if (out.measuredHz === null) console.log('  no pitch detected')
    else console.log(`  measured ${out.measuredHz} Hz   ${out.cents >= 0 ? '+' : ''}${out.cents}¢   conf ${out.confidence}   ${mark(out.verdict)} ${out.verdict}${octLabel(out.octaves)}`)
  }
  return out
}

// ── cli ──────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2)
const json = argv.includes('--json')
const keep = argv.includes('--keep')
const quiet = argv.includes('--quiet')
const positional = argv.filter(a => !a.startsWith('--'))
const noteIdx = argv.indexOf('--note')
const flag = (name, def) => { const i = argv.indexOf(name); return i === -1 ? def : argv[i + 1] }

// recipe mode: --engine NAME [--macros h,t,m] [--range lo-hi] [--step n]
let recipe = null
const engArg = flag('--engine', null)
if (engArg) {
  const macros = (flag('--macros', '0,0,0')).split(',').map(Number)
  if (macros.length !== 3 || macros.some(isNaN)) { console.error('--macros wants h,t,m (e.g. 0,0.38,0.70)'); process.exit(1) }
  const [lo, hi] = (flag('--range', '45-88')).split('-').map(Number)
  recipe = { engine: engArg.toUpperCase(), macros, lo, hi, step: +flag('--step', 2) }
}

try {
  if (positional.length && positional[0].endsWith('.wav')) {
    if (noteIdx === -1) { console.error('single-wav mode needs --note <midi>'); process.exit(1) }
    single(positional[0], +argv[noteIdx + 1], json)
  } else {
    const results = run({ json, keep, recipe })
    if (quiet) {
      const bad = results.filter(r => r.verdict === 'off' || r.verdict === 'OUT OF TUNE')
      process.exit(bad.length ? 1 : 0)
    }
  }
} catch (e) { console.error('tune-check:', e.message); process.exit(2) }
