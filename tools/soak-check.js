#!/usr/bin/env node
// soak-check.js — the engine's LONG-RUN stability gate. tune/level/fx-check each render a few
// seconds; this renders ~64s of tools/carts/soak.c — cycles of dense note-firing through a big
// reverb+echo tail (STRESS) and silence (IDLE) — and asserts the things only a long run reveals:
//
//   • STRESS level stays STABLE across all cycles — late cycles ≈ early ones. A slow gain drift,
//     or a progressive VOICE LEAK starving the pool, shows as the stress level creeping cycle to cycle.
//   • IDLE DECAYS well below stress — the tails die; a leaked voice or a runaway feedback loop would
//     keep an idle window ringing instead.
//   • No cross-cycle ENERGY ACCUMULATION — the idle-tail floor doesn't climb run-long (a feedback
//     loop slowly piling up energy / DC).
//   • No blowup or NaN-collapse — every stress window still makes sound; peak stays bounded.
//
//   node tools/soak-check.js            render the long run + per-cycle stability report
//   node tools/soak-check.js --quiet    CI gate: exit 1 on drift / leak / accumulation / blowup
//   node tools/soak-check.js --json     machine-readable
//   node tools/soak-check.js --keep     keep the render (build/.soak/)
//   node tools/soak-check.js --selfcheck  known answers for the VERDICTS (renders no cart)
//
// ⚠ THE CONTROLS, and why this gate needed them more than its siblings: every assertion above is a
// statement about a SET of cycles, and all of them are vacuously true of the empty set. This
// shipped printing "✓ stable over the long run" and exiting 0 on a run that measured ZERO cycles —
// and on one that measured 3 of 24. For a gate whose whole subject is duration, that made "no soak
// happened" indistinguishable from "the soak was clean", and no threshold could ever notice,
// because a shorter run drifts less, decays as well and accumulates less: the evidence vanishing
// makes every check EASIER. controlCheck() now requires the run to have actually happened before
// any of it counts. See docs/guides/checks-and-oracles.md → "The OTHER way a green check lies".
//
// Assertions are DECAY-RELATIVE (not an absolute silence floor) so they don't depend on exactly
// how long an aggressive tail takes to die. Pairs with the denormal flush-to-zero in sound.h: this
// proves the tails decay (the audible side); FTZ handles the audio-thread CPU side of that decay
// passing through the denormal range. Render is deterministic (--det).

const fs = require('fs')
const path = require('path')
const { spawnSync } = require('child_process')

const ROOT = path.resolve(__dirname, '..')

// ── the cart's geometry, mirrored from tools/carts/soak.c ────────────────────
// Ground truth for the controls below AND for the frame budget, so the two cannot drift apart —
// the budget used to be a hand-written `24 * 160 + 300` with the 24 repeated nowhere else, which
// is the same shape that cost level-check.js a whole note. If soak.c changes these, control 2
// goes red and names the mismatch instead of the gate quietly measuring fewer cycles.
const CART_STRESS = 60, CART_IDLE = 100, CART_CYCLES = 24
const CART_CYCLE = CART_STRESS + CART_IDLE

// thresholds (dB)
const STRESS_DRIFT_DB = 4.0    // spread of per-cycle stress RMS — more = drift or voice-pool starvation
const DECAY_MARGIN_DB = 12.0   // idle-late must sit at least this far below the same cycle's stress (tail is dying; healthy run decays 15-18, a stuck/leaked voice ~0-5)
const ACCUM_DB        = 4.0    // idle-late floor must not climb more than this across the run (no energy pile-up)
const ALIVE_DBFS      = -45.0  // a stress window quieter than this = the engine went silent (crash / total starvation)
const BLOWUP_DBFS     = -0.2   // sustained peak above this for a whole stress window = something runaway (limiter pinned)

const dbfs = (x) => x <= 0 ? -Infinity : 20 * Math.log10(x)
const fmtDb = (d) => d === -Infinity ? ' -inf' : `${d >= 0 ? '+' : ''}${d.toFixed(1)}`.padStart(6)

// ── WAV reader (same as the other audio gates) ───────────────────────────────
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

const peakOf = (s, a, b) => { let p = 0; for (let i = a; i < b; i++) { const v = Math.abs(s[i]); if (v > p) p = v } return p }
const rmsOf  = (s, a, b) => { let q = 0, n = 0; for (let i = a; i < b; i++) { q += s[i] * s[i]; n++ } return n ? Math.sqrt(q / n) : 0 }

// ── trace → (cycle, phase) windows ───────────────────────────────────────────
function windows(traceFile) {
  const lines = fs.readFileSync(traceFile, 'utf8').trim().split('\n')
  const wins = []
  let cur = null, lastFrame = 0
  for (const ln of lines) {
    let row; try { row = JSON.parse(ln) } catch { continue }
    if (row.vev !== undefined) continue   // skip voice-trace events (-DDE_TRACE): they share the trace JSONL but carry no cyc/phase window info (would split a window)
    const w = row.w || {}
    const cyc = +w.cyc, phase = +w.phase, f = row.f
    lastFrame = f
    if (cur && cur.cyc === cyc && cur.phase === phase) cur.f1 = f
    else { if (cur) wins.push(cur); cur = { cyc, phase, f0: f, f1: f } }
  }
  if (cur) wins.push(cur)
  return { wins, lastFrame }
}

function runPlay(frames, wav, trace) {
  const r = spawnSync('node',
    [path.join('tools', 'play.js'), 'soak', 'run', '--headless', '--det',
     '--frames', String(frames), '--trace', trace, '--wav', wav],
    { cwd: ROOT, encoding: 'utf8' })
  if (r.status !== 0) { process.stderr.write((r.stdout || '') + (r.stderr || '')); throw new Error('render failed (play.js soak)') }
}

function analyze() {
  const dir = path.join(ROOT, 'build', '.soak')
  fs.mkdirSync(dir, { recursive: true })
  const wav = path.join(dir, 'soak.wav'), trace = path.join(dir, 'soak.trace.jsonl')
  runPlay(CART_CYCLES * CART_CYCLE + 300, wav, trace)   // + slack so the last tail is fully rendered
  const { sr, s } = readWavMono(wav)
  const { wins, lastFrame } = windows(trace)
  const spf = s.length / (lastFrame + 1)
  // collect per-cycle stress + idle-late measurements
  const cycles = {}
  for (const w of wins) {
    if (w.cyc < 0) continue
    const a = Math.floor(w.f0 * spf), b = Math.floor(w.f1 * spf), span = b - a
    cycles[w.cyc] = cycles[w.cyc] || { cyc: w.cyc }
    if (w.phase === 1) {                                  // stress: measure the body
      cycles[w.cyc].stressRms = dbfs(rmsOf(s, Math.floor(a + span * 0.2), Math.floor(a + span * 0.9)))
      cycles[w.cyc].stressPeak = dbfs(peakOf(s, a, b))
    } else {                                              // idle: measure the LATE part (tail decayed)
      cycles[w.cyc].idleLateRms = dbfs(rmsOf(s, Math.floor(a + span * 0.55), Math.floor(a + span * 0.95)))
    }
  }
  const rows = Object.values(cycles).filter(c => c.stressRms !== undefined).sort((a, b) => a.cyc - b.cyc)
  return { dir, wav, trace, sr, rows, meta: { sr, spf, wins, lastFrame } }
}

// ── the controls ─────────────────────────────────────────────────────────────
// THE ONE THIS GATE COULD NOT LIVE WITHOUT IS #2, and the reason is worth stating plainly: every
// assertion in assess() is a statement about a SET of cycles, and every one of them is vacuously
// true of the empty set. Measured before this existed — a run whose rows came back empty printed
// "✓ stable over the long run — no drift, leak, accumulation, or blowup" and exited 0. So did a run
// that measured 3 cycles of 24. For a gate whose entire subject is DURATION, "no soak happened" and
// "the soak was clean" were the same output. Nothing else here can catch that: a shorter run has
// less drift, decays just as well, and accumulates less, so every threshold gets EASIER as the
// evidence disappears.
function controlCheck(rows, meta) {
  const bad = []
  if (meta) {
    const want = meta.sr / 60           // the render is frame-locked at 60fps
    if (Math.abs(meta.spf - want) > 0.5)
      bad.push(`${meta.spf.toFixed(1)} samples per frame, but a 60fps render at ${meta.sr}Hz is ${want}`)
    // window lengths, checked only over the cycles that actually have a stress phase: soak.c lets
    // `cyc` keep counting past NCYC, so the render's tail legitimately holds two long idle windows
    const measured = new Set(rows.map(r => r.cyc))
    const lenOf = (phase) => new Set(meta.wins.filter(w => w.phase === phase && measured.has(w.cyc))
      .map(w => w.f1 - w.f0))
    const sl = lenOf(1), il = lenOf(0)
    if (sl.size > 1) bad.push(`stress windows are not all the same length (${[...sl].sort((a, b) => a - b).join(', ')} frames)`)
    else if (sl.size === 1 && [...sl][0] !== CART_STRESS - 1)
      bad.push(`a stress window spans ${[...sl][0] + 1} frames, but soak.c gates ${CART_STRESS} — the cart and this file have drifted`)
    if (il.size > 1) bad.push(`idle windows are not all the same length (${[...il].sort((a, b) => a - b).join(', ')} frames)`)
    else if (il.size === 1 && [...il][0] !== CART_IDLE - 1)
      bad.push(`an idle window spans ${[...il][0] + 1} frames, but soak.c idles ${CART_IDLE} — the cart and this file have drifted`)
  }
  // (2) THE VACUITY GUARD. See the note above.
  if (rows.length !== CART_CYCLES)
    bad.push(rows.length === 0
      ? `NO cycles were measured at all — this is not a passing soak, it is no soak (soak.c runs ${CART_CYCLES})`
      : `${rows.length} cycles measured, but soak.c runs ${CART_CYCLES} — a short run makes every check below EASIER, not cleaner`)
  // (3) both phases per cycle: a cycle with no idle window drops silently out of the decay and
  // accumulation checks, taking its evidence with it and leaving the remaining ones looking fine
  const noIdle = rows.filter(r => r.idleLateRms === undefined)
  if (noIdle.length)
    bad.push(`${noIdle.length} cycle(s) have a stress phase but no idle window (first: cycle ${noIdle[0].cyc}) — their decay was never checked`)
  return bad
}

function assess(rows) {
  const issues = []
  const stressRms = rows.map(r => r.stressRms)
  const idleLate  = rows.filter(r => r.idleLateRms !== undefined).map(r => r.idleLateRms)
  const spread = Math.max(...stressRms) - Math.min(...stressRms)
  // 1) stress level stable across cycles (drift / progressive starvation)
  if (spread > STRESS_DRIFT_DB) issues.push({ sev: 'bad', msg: `stress level drifts ${spread.toFixed(1)} dB across cycles (>${STRESS_DRIFT_DB}) — gain drift or voice-pool starvation` })
  // 2) engine still alive every cycle (no crash / total starvation)
  const dead = rows.filter(r => r.stressRms < ALIVE_DBFS)
  if (dead.length) issues.push({ sev: 'bad', msg: `${dead.length} stress cycle(s) went silent (< ${ALIVE_DBFS} dBFS) — crash or total voice starvation (first: cycle ${dead[0].cyc})` })
  // 3) idle decays below stress each cycle (tails die, no stuck/leaked voice ringing)
  const noDecay = rows.filter(r => r.idleLateRms !== undefined && r.idleLateRms > r.stressRms - DECAY_MARGIN_DB)
  if (noDecay.length) issues.push({ sev: 'bad', msg: `${noDecay.length} cycle(s) idle tail did not decay ≥ ${DECAY_MARGIN_DB} dB below stress — leaked voice or runaway tail (first: cycle ${noDecay[0].cyc})` })
  // 4) idle floor not climbing across the run (energy / DC accumulation).
  // The END of the run is the mean of the last few cycles, not the single last one. Same threshold,
  // same intent — but `last - min` was a claim about the whole run resting on ONE sample of it, and
  // a floor that ramps for twenty cycles and dips on the twenty-fourth defeated it completely. That
  // is not a tolerance choice, it is the statistic failing to measure what its own message says.
  // Pinned in --selfcheck in both directions, including the ramp-then-dip the old form let through.
  if (idleLate.length >= 2) {
    const tail = idleLate.slice(-Math.min(3, idleLate.length))
    const end = tail.reduce((a, b) => a + b, 0) / tail.length
    const climb = end - Math.min(...idleLate)
    if (climb > ACCUM_DB) issues.push({ sev: 'bad', msg: `idle-tail floor climbs ${climb.toFixed(1)} dB over the run (>${ACCUM_DB}) — energy/DC accumulating in a feedback loop` })
  }
  // 5) no runaway pinning the limiter for a whole stress window
  const pinned = rows.filter(r => r.stressPeak > BLOWUP_DBFS)
  if (pinned.length) issues.push({ sev: 'warn', msg: `${pinned.length} stress cycle(s) peak near full-scale (> ${BLOWUP_DBFS} dBFS) — dense, leaning on the limiter` })
  return issues
}

function printReport(rows, sr, issues, control) {
  console.log(`soak — ${rows.length} cycles @ ${sr}Hz   (drift <${STRESS_DRIFT_DB}dB · decay ≥${DECAY_MARGIN_DB}dB · accum <${ACCUM_DB}dB)\n`)
  console.log('  cyc   stress rms   stress peak   idle-late rms   decay')
  for (const r of rows) {
    const decay = r.idleLateRms !== undefined ? (r.stressRms - r.idleLateRms) : null
    console.log(`  ${String(r.cyc).padStart(3)}     ${fmtDb(r.stressRms)}      ${fmtDb(r.stressPeak)}       ${r.idleLateRms !== undefined ? fmtDb(r.idleLateRms) : '   —'}     ${decay !== null ? decay.toFixed(1) + 'dB' : ''}`)
  }
  console.log()
  for (const it of issues) console.log(`  ${it.sev === 'bad' ? '✗' : '⚠'} ${it.msg}`)
  // the ✓ is gated on the CONTROL, not just on the issue list — claiming a clean long run off a
  // measurement that never happened is the exact failure this gate shipped with
  if (control && control.length) {
    console.error(`${issues.length ? '\n' : ''}✗ THE MEASUREMENT IS OFF, NOT THE ENGINE:`)
    for (const c of control) console.error(`    ${c}`)
    console.error('  Nothing above is evidence of stability. Start with `node tools/soak-check.js --selfcheck`.')
  } else if (!issues.length) {
    console.log(`✓ stable over the long run — no drift, leak, accumulation, or blowup (${rows.length} cycles)`)
  }
}

// ── --selfcheck: KNOWN ANSWERS FOR THE VERDICTS ──────────────────────────────
// Renders no cart. Unlike the other audio gates most of the surface here is not a measurement but
// a JUDGEMENT over a per-cycle series, so the fixture is mostly synthetic series with a known
// verdict — and the healthy numbers below are the real ones, taken from an actual run.
function selfcheck() {
  let pass = 0, fail = 0
  const ok = (name, cond, got) => {
    if (cond) { pass++; console.log(`  ✓ ${name}`) } else { fail++; console.log(`  ✗ ${name}   got: ${got}`) }
  }
  // a healthy run, measured: stress ≈ -25 dB (spread 1.5), idle-late ≈ -42, decay 15-18 dB
  const healthy = (n = CART_CYCLES, f = () => ({})) => [...Array(n)].map((_, i) => ({
    cyc: i, stressRms: -25.0 - (i % 3) * 0.5, stressPeak: -12.0, idleLateRms: -42.0 + (i % 4) * 0.5, ...f(i) }))
  const msgs = (rs) => assess(rs).map(x => x.msg).join(' | ')
  const sevs = (rs) => assess(rs).filter(x => x.sev === 'bad').length

  console.log('soak-check --selfcheck — known answers for the verdicts (no cart is rendered)\n')

  console.log('THE VACUITY GUARD — what this gate shipped without')
  ok('an EMPTY run raises no issue at all: every check is vacuously true of no cycles',
     assess([]).length === 0, msgs([]))
  ok('  …so the CONTROL is the only thing that can catch it, and it does',
     controlCheck([], null).some(c => c.includes('it is no soak')), controlCheck([], null))
  ok('  …a 3-of-24 run is caught too (a short run is easier, not cleaner)',
     controlCheck(healthy(3), null).some(c => c.includes('3 cycles measured')), controlCheck(healthy(3), null))
  ok('  …and 23 of 24 is still caught — one missing cycle is a missing cycle',
     controlCheck(healthy(23), null).length > 0, 'silent')
  ok('a full healthy run passes the control', controlCheck(healthy(), null).length === 0,
     controlCheck(healthy(), null))
  // the reason a short run cannot be caught by the thresholds: it scores BETTER on all of them
  ok('  …and proof it needed a control: 3 cycles score no worse than 24 on every assertion',
     sevs(healthy(3)) === 0 && sevs(healthy()) === 0, `${sevs(healthy(3))} / ${sevs(healthy())}`)
  ok('a cycle with a stress phase but no idle window is caught',
     controlCheck(healthy().map((r, i) => i === 7 ? { ...r, idleLateRms: undefined } : r), null)
       .some(c => c.includes('no idle window')), 'silent')

  console.log('\nTHE STRUCTURAL CONTROLS')
  // real geometry: stress windows span 59 frames (soak.c gates 60), idle 99, spf exactly 735
  const meta = (o = {}) => {
    const wins = []
    for (let i = 0; i < CART_CYCLES; i++) {
      wins.push({ cyc: i, phase: 1, f0: i * CART_CYCLE, f1: i * CART_CYCLE + (o.shortStress && i === 3 ? 20 : CART_STRESS - 1) })
      wins.push({ cyc: i, phase: 0, f0: i * CART_CYCLE + CART_STRESS, f1: i * CART_CYCLE + CART_STRESS + (CART_IDLE - 1) })
    }
    // soak.c keeps counting past NCYC, so the render tail legitimately holds two LONG idle windows
    wins.push({ cyc: CART_CYCLES, phase: 0, f0: CART_CYCLES * CART_CYCLE, f1: CART_CYCLES * CART_CYCLE + 159 })
    return { sr: 44100, spf: 735, wins, lastFrame: 4139, ...o.over }
  }
  ok('the real geometry passes', controlCheck(healthy(), meta()).length === 0, controlCheck(healthy(), meta()))
  ok('  …including the two over-run idle windows in the tail, which are NOT a length mismatch',
     controlCheck(healthy(), meta()).length === 0, 'flagged the tail')
  ok('samples-per-frame off by 2% is caught',
     controlCheck(healthy(), meta({ over: { spf: 749.7 } })).some(c => c.includes('samples per frame')), 'silent')
  ok('a stress window shorter than its neighbours is caught',
     controlCheck(healthy(), meta({ shortStress: true })).some(c => c.includes('same length')), 'silent')
  // if soak.c retimes its phases, the tool's mirrored constants are wrong and every window is off
  const retimed = () => { const m = meta(); m.wins = m.wins.map(w => w.phase === 1 ? { ...w, f1: w.f0 + 49 } : w); return m }
  ok('soak.c changing STRESS without this file is caught, and named as a drift',
     controlCheck(healthy(), retimed()).some(c => c.includes('have drifted')), controlCheck(healthy(), retimed()))

  console.log('\nTHE FIVE ASSERTIONS, BOTH DIRECTIONS')
  ok('a healthy run raises nothing', assess(healthy()).length === 0, msgs(healthy()))
  ok('a slow gain drift across cycles is bad',
     msgs(healthy(CART_CYCLES, (i) => ({ stressRms: -30 + i * 0.4 }))).includes('drifts'), 'silent')
  ok('  …and a drift just under the threshold is not',
     !msgs(healthy(CART_CYCLES, (i) => ({ stressRms: -25 - i * 0.15 }))).includes('drifts'), 'flagged')
  ok('a cycle that went silent is bad and names the first one',
     msgs(healthy(CART_CYCLES, (i) => i >= 9 ? { stressRms: -60 } : {})).includes('first: cycle 9'), 'silent')
  ok('an idle tail that never decays is a leaked voice',
     msgs(healthy(CART_CYCLES, () => ({ idleLateRms: -26.0 }))).includes('did not decay'), 'silent')
  ok('  …and a tail 15 dB down is healthy (the real run decays 15-18)',
     !msgs(healthy(CART_CYCLES, () => ({ stressRms: -25, idleLateRms: -40 }))).includes('did not decay'), 'flagged')
  ok('a stress window pinned at full scale warns, but does not fail the gate',
     assess(healthy(CART_CYCLES, () => ({ stressPeak: -0.1 }))).every(i => i.sev === 'warn'), 'bad')

  console.log('\nENERGY ACCUMULATION — the statistic, and the hole it used to have')
  const ramp = (dipLast) => healthy(CART_CYCLES, (i) => ({
    idleLateRms: -50 + i * 0.35 + (dipLast && i === CART_CYCLES - 1 ? -6 : 0) }))
  ok('a floor climbing run-long is caught', msgs(ramp(false)).includes('climbs'), msgs(ramp(false)))
  ok('  …and STILL caught when the very last cycle dips',
     msgs(ramp(true)).includes('climbs'), msgs(ramp(true)))
  // the regression guard: the shipped form was `last - min`, which the dip above defeated outright
  const oldForm = (rs) => { const v = rs.map(r => r.idleLateRms); return v[v.length - 1] - Math.min(...v) }
  ok('  …which the OLD single-point statistic did not (it read 1.5 dB, under the 4 dB limit)',
     oldForm(ramp(true)) < ACCUM_DB && oldForm(ramp(false)) > ACCUM_DB,
     `${oldForm(ramp(true)).toFixed(1)} / ${oldForm(ramp(false)).toFixed(1)}`)
  ok('a flat floor is not accumulation', !msgs(healthy()).includes('climbs'), msgs(healthy()))
  ok('  …nor is a floor that FALLS over the run', !msgs(healthy(CART_CYCLES, (i) => ({ idleLateRms: -40 - i * 0.3 }))).includes('climbs'), 'flagged')

  console.log(`\n${fail ? '✗' : '✓'} ${pass} passed, ${fail} failed`)
  return fail ? 1 : 0
}

// ── run ──────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2)
const json = argv.includes('--json'), quiet = argv.includes('--quiet'), keep = argv.includes('--keep')

if (argv.includes('--selfcheck')) process.exit(selfcheck())

try {
  const { dir, sr, rows, meta } = analyze()
  const issues = assess(rows)
  const control = controlCheck(rows, meta)
  if (!keep) fs.rmSync(dir, { recursive: true, force: true })
  if (json) console.log(JSON.stringify({ control, rows, issues }, null, 2))
  else printReport(rows, sr, issues, control)
  const bad = issues.filter(i => i.sev === 'bad').length + control.length
  if (quiet) process.exit(bad ? 1 : 0)
} catch (e) { console.error('soak-check:', e.message); process.exit(2) }
