#!/usr/bin/env node
// disp-model.js — what a dispersion allpass cascade does to a waveguide's PARTIALS, computed
// analytically. The design counterpart to inharm-spec.js: that one MEASURES a render, this one
// PREDICTS from the filter, so a dispersion design can be settled before any engine is touched.
//
//   node tools/disp-model.js                  what it COSTS to reach a target B, per note (default)
//   node tools/disp-model.js --curve          forward transfer curve: coefficient → B, both signs
//   node tools/disp-model.js --check          self-test (exits nonzero on failure)
//   node tools/disp-model.js --b 2e-4 --stages 1,2,4 --notes C2,C4    retarget / narrow
//
// WHY IT EXISTS. Answering "can a 4-stage allpass cascade reach a real piano's inharmonicity?" by
// patching runtime/sound.h and rendering a grid was the first attempt, and it was both slow (minutes)
// and DANGEROUS: sound.h is shared across parallel agents, and a long sweep holds the engine in a
// broken state while others compile against it. Worse, a timeout kills the sweep with SIGTERM and the
// restore in the `finally` never runs — which left the engine patched twice. The loop is exactly
// computable, so none of that is necessary. Patch the engine only to CONFIRM a chosen point, never to
// search (one render, seconds).
//
// THE MODEL. A Karplus-Strong loop resonates where the round-trip phase lag is a whole number of
// cycles:  Theta(w) = w*L + N*theta_ap(w) = 2*pi*n  (L = delay-line samples, N = allpass stages).
// A first-order allpass H(z) = (c + z^-1)/(1 + c z^-1) has phase lag
//     theta_ap(w) = -[ atan2(-sin w, c + cos w) - atan2(-c*sin w, 1 + c*cos w) ]
// Solve per partial, fit B from f_n = n*f0*sqrt(1+B*n^2). With N = 0 it returns the exact harmonic
// series, which is the model's own control (--check asserts it).
//
// VALIDATED AGAINST THE ENGINE (2026-07-30, audit §I4b). At C3 with 2 stages at c = -0.7770 the model
// predicts B = 1.00e-4, h16 = +19.8c, residual 1.1c; INSTR_PIANO measured B = 1.02e-4, h16 = +19.9c,
// residual 1.2c via inharm-spec. f0 agreed to 1.8 cents (the gap is PIANO's own Railsback stretch plus
// §I4d's loop offset, neither of which this model includes).
//
// TWO RESULTS WORTH KNOWING BEFORE TOUCHING sound.h:
//   1. THE SIGN. A POSITIVE c gives phase delay that RISES with frequency (pt at DC to exactly 1
//      sample at Nyquist), which flattens upper partials — the opposite of string stiffness. Stretching
//      needs c < 0. The engine computes c = (1-pt)/(1+pt) with pt clamped to <= 0.9, so c is always in
//      (0.05, 1] and the useful half of the parameter space is unreachable by construction.
//   2. THE PITCH DEPENDENCE IS BACKWARDS. The |c| needed for a fixed B FALLS as pitch rises (-0.72 at
//      C3 to -0.09 at C6, 4 stages), because a high note's partials span more of the Nyquist band where
//      an allpass's delay variation lives. The engine's pt grows with freq, moving c the other way.
//
// The cost side is the part that has to be designed, not just fixed: the cascade also delays the
// FUNDAMENTAL, so that phase delay must come out of the delay line or the note plays flat. This tool
// reports it as "delay@f0" and what is left of the line ("L left"), which is the constraint that
// decides whether a register is reachable at all.

const SR = 44100
const NPART = 16
const NOTE_HZ = { C1: 32.703, C2: 65.406, C3: 130.813, C4: 261.626, C5: 523.251, C6: 1046.502, C7: 2093.005 }

// ── the filter ───────────────────────────────────────────────────────────────
function thetaAp(w, c) {                     // phase LAG (radians, >= 0) of one first-order allpass
  let phi = Math.atan2(-Math.sin(w), c + Math.cos(w)) - Math.atan2(-c * Math.sin(w), 1 + c * Math.cos(w))
  while (phi > 0) phi -= 2 * Math.PI
  return -phi
}
const Theta = (w, L, N, c) => w * L + N * thetaAp(w, c)

function solvePartial(n, L, N, c) {          // bisect Theta(w) = 2*pi*n on (0, pi)
  const target = 2 * Math.PI * n
  let lo = 1e-9, hi = Math.PI - 1e-9
  if (Theta(hi, L, N, c) < target) return null        // would land above Nyquist
  for (let i = 0; i < 200; i++) {
    const mid = 0.5 * (lo + hi)
    if (Theta(mid, L, N, c) < target) lo = mid; else hi = mid
  }
  return 0.5 * (lo + hi) * SR / (2 * Math.PI)
}
const cents = (f, ref) => 1200 * Math.log2(f / ref)

function analyse(L, N, c) {
  const f = []
  for (let n = 1; n <= NPART; n++) f.push(solvePartial(n, L, N, c))
  const f0 = f[0]
  if (!f0) return null
  let num = 0, den = 0
  for (let n = 2; n <= NPART; n++) {
    if (!f[n - 1]) continue
    num += (Math.pow(f[n - 1] / (n * f0), 2) - 1) * n * n
    den += Math.pow(n, 4)
  }
  const B = den ? num / den : 0
  let ss = 0, k = 0
  for (let n = 2; n <= NPART; n++) {
    if (!f[n - 1]) continue
    ss += Math.pow(cents(f[n - 1], n * f0 * Math.sqrt(1 + B * n * n)), 2); k++
  }
  const dev = (n) => f[n - 1] ? cents(f[n - 1], n * f0) : null
  const w0 = 2 * Math.PI * f0 / SR
  return { B, resid: k ? Math.sqrt(ss / k) : 0, h8: dev(8), h16: dev(16), f0,
           dcDelay: N * thetaAp(w0, c) / w0 }      // what must come OUT of the delay line
}

// solve c in (-0.9995, 0) for a target B — monotone, B grows as c approaches -1
function solveC(L, N, bTarget) {
  const bOf = (c) => { const r = analyse(L, N, c); return r ? r.B : 0 }
  let lo = -0.9995, hi = -1e-6
  if (bOf(lo) < bTarget) return null                 // unreachable even at the limit
  for (let i = 0; i < 120; i++) {
    const mid = 0.5 * (lo + hi)
    if (bOf(mid) >= bTarget) lo = mid; else hi = mid
  }
  return 0.5 * (lo + hi)
}

// ── views ────────────────────────────────────────────────────────────────────
const pad = (s, w) => String(s).padStart(w)
const sc = (v, w = 6) => v === null ? pad('?', w) : pad((v >= 0 ? '+' : '') + v.toFixed(1), w)

function costTable(bTarget, stages, notes) {
  console.log(`\nWHAT IT COSTS TO REACH B = ${bTarget}   (ideal h16 = ` +
    `+${(1200 * Math.log2(Math.sqrt(1 + bTarget * 256))).toFixed(1)}c)`)
  console.log('c is solved per (note, stages). "delay@f0" is the phase delay the cascade adds at the')
  console.log('fundamental, which must be SUBTRACTED from the delay line or the note plays flat;')
  console.log('"L left" is what remains. Under ~2 samples the loop cannot exist at that pitch.\n')
  console.log('note      L0    N        c   delay@f0    L left   resid     h16   verdict')
  for (const name of notes) {
    const f0 = NOTE_HZ[name]
    if (!f0) { console.log(`${name}: unknown note`); continue }
    const L0 = Math.round(SR / f0)
    for (const N of stages) {
      const c = solveC(L0, N, bTarget)
      if (c === null) { console.log(`${name.padEnd(4)} ${pad(L0, 5)}  ${pad(N, 3)}        —         —         —       —       —   UNREACHABLE`); continue }
      const r = analyse(L0, N, c)
      const left = L0 - r.dcDelay
      const verdict = left < 2 ? 'IMPOSSIBLE' : left < 0.25 * L0 ? 'severe' : left < 0.6 * L0 ? 'costly' : 'ok'
      console.log(`${name.padEnd(4)} ${pad(L0, 5)}  ${pad(N, 3)}  ${pad(c.toFixed(4), 7)}  ` +
        `${pad(r.dcDelay.toFixed(1), 8)}  ${pad(left.toFixed(1), 8)}  ${pad(r.resid.toFixed(1) + 'c', 6)}  ${sc(r.h16)}   ${verdict}`)
    }
    console.log('')
  }
}

function curveTable(stages, notes) {
  for (const name of notes) {
    const L0 = Math.round(SR / NOTE_HZ[name])
    console.log(`\nFORWARD TRANSFER CURVE — ${name} (L0 = ${L0})`)
    console.log('POSITIVE c is what the engine\'s c = (1-pt)/(1+pt) produces; note the sign of B.\n')
    console.log('   N        c   delay@f0     fitted B   resid     h16')
    for (const N of stages) {
      for (const c of [0.9802, 0.8182, 0.5385, 0.2500, -0.1, -0.3, -0.5, -0.7, -0.85]) {
        const r = analyse(L0, N, c)
        if (!r) continue
        console.log(`  ${pad(N, 2)}  ${pad(c.toFixed(4), 7)}  ${pad(r.dcDelay.toFixed(1), 8)}  ` +
          `${pad(r.B.toExponential(2), 11)}  ${pad(r.resid.toFixed(1) + 'c', 6)}  ${sc(r.h16)}`)
      }
      console.log('')
    }
  }
}

function selfCheck() {
  let fails = 0
  const ok = (cond, label, got) => { if (!cond) { fails++; console.log(`  FAIL ${label}  (got ${got})`) } else console.log(`  ok   ${label}`) }
  console.log('\ndisp-model --check\n')
  // 1. the control: no allpass must give an exact harmonic series
  const ctl = analyse(337, 0, 0)
  ok(Math.abs(ctl.B) < 1e-12, 'N=0 returns an exact harmonic series (B ~ 0)', ctl.B.toExponential(1))
  // 2. sign: positive c must FLATTEN (B < 0), negative c must SHARPEN (B > 0)
  ok(analyse(337, 2, 0.5385).B < 0, 'positive c flattens the partials (B < 0)', analyse(337, 2, 0.5385).B.toExponential(1))
  ok(analyse(337, 2, -0.5).B > 0, 'negative c sharpens the partials (B > 0)', analyse(337, 2, -0.5).B.toExponential(1))
  // 3. the ENGINE-VALIDATED point: C3, 2 stages, c = -0.7770 (see the header)
  const v = analyse(337, 2, -0.7770)
  ok(Math.abs(v.B - 1.0e-4) / 1.0e-4 < 0.05, 'engine-validated point: B within 5% of 1.00e-4', v.B.toExponential(3))
  ok(Math.abs(v.h16 - 19.8) < 0.5, 'engine-validated point: h16 within 0.5c of +19.8c', v.h16.toFixed(2))
  // 4. monotonicity the solver relies on
  ok(analyse(337, 2, -0.9).B > analyse(337, 2, -0.6).B, 'B grows monotonically as c approaches -1', '')
  // 5. the solver inverts itself
  const c = solveC(337, 4, 1e-4)
  ok(c !== null && Math.abs(analyse(337, 4, c).B - 1e-4) / 1e-4 < 0.01, 'solveC round-trips to the target B', String(c))
  console.log(fails === 0 ? '\nPASS\n' : `\nFAIL — ${fails} check(s)\n`)
  return fails === 0
}

// ── cli ──────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2)
const flag = (k, d) => { const i = argv.indexOf(k); return i >= 0 ? argv[i + 1] : d }
if (argv.includes('--check')) process.exit(selfCheck() ? 0 : 1)
const stages = flag('--stages', '1,2,4,8').split(',').map(Number)
const notes = flag('--notes', 'C2,C3,C4,C5,C6').split(',')
if (argv.includes('--curve')) curveTable(stages, notes.slice(0, 1))
else costTable(parseFloat(flag('--b', '1e-4')), stages, notes)
