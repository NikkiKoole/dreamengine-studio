#!/usr/bin/env node
// motor-check.js: the INSTR_MOTOR gate: does the cylinder + exhaust engine voice behave like an engine?
//
//   node tools/motor-check.js            run every check, print a table
//   node tools/motor-check.js --quiet    exit 1 on any failure, print only failures (CI)
//
// Renders tools/carts/motorprobe.c (one held engine, configured by DE_DEFINES) headless and reads the WAV
// with Goertzel filters at known frequencies (the probe sets the rpm, so every harmonic's frequency is
// known: no pitch detection to go wrong). What it asserts, and the reference each number came from:
//   RUMBLE   the in-between CYCLE harmonics (not multiples of the firing rate) carry energy. Measured on the
//            CC0 references (tools/refs/engines-cc0.txt, build/refs/engines-cc0/): an inline-6 at ~1400 rpm
//            has them 14-25 dB under its firing harmonics, a lumpy V8 idle within a few dB. A perfectly even
//            pulse train has NONE and reads as a buzzer: that is the old LFO-on-a-saw carengine voice.
//   CONTROL  MODE_MOTOR_ROUGH = 0 is a perfectly even machine, so its in-between harmonics must VANISH.
//            This is the check that proves the rumble check can fail (gate-controls.js).
//   FIXED    the exhaust's resonance hump stays put while the revs double (a filter that tracked the pitch,
//            a synth's habit, would move it an octave).
//   LOAD     a harder pull is brighter (the blowdown spike sharpens).
//   KNOCK    MODE_MOTOR_KNOCK lifts the 1-3 kHz band of an idle by 8 dB or more (the diesel clatter).
//   LEVEL    every layout, idle (stock) to redline (straight pipe), stays audible and under 0 dBFS.
'use strict'
const fs = require('fs'), path = require('path'), os = require('os'), { execFileSync } = require('child_process')
const ROOT = path.resolve(__dirname, '..')
const QUIET = process.argv.includes('--quiet')
const TMP = fs.mkdtempSync(path.join(os.tmpdir(), 'motor-check-'))

function render(defs, name) {
  const wav = path.join(TMP, name + '.wav')
  execFileSync('node', [path.join(ROOT, 'tools/play.js'), 'motorprobe', 'script', '/dev/null', '--headless', '--frames', '170', '--wav', wav],
    { cwd: ROOT, env: { ...process.env, DE_DEFINES: defs }, stdio: 'pipe' })
  return readWav(wav)
}
function readWav(p) {
  const b = fs.readFileSync(p); let q = 12, ch = 1, sr = 44100, off = 44, len = 0
  while (q + 8 <= b.length) {
    const id = b.toString('ascii', q, q + 4), sz = b.readUInt32LE(q + 4)
    if (id === 'fmt ') { ch = b.readUInt16LE(q + 10); sr = b.readUInt32LE(q + 12) }
    if (id === 'data') { off = q + 8; len = sz; break }
    q += 8 + sz + (sz & 1)
  }
  const n = Math.floor(len / (2 * ch)), x = new Float64Array(n)
  for (let i = 0; i < n; i++) x[i] = b.readInt16LE(off + i * 2 * ch) / 32768
  return { sr, x }
}
// the steady middle (skip the attack): 1.0 .. 2.6 s, Hann-windowed
function seg({ sr, x }) {
  const a = Math.floor(1.0 * sr), z = Math.min(x.length, Math.floor(2.6 * sr)), n = z - a, s = new Float64Array(n)
  for (let i = 0; i < n; i++) s[i] = x[a + i] * (0.5 - 0.5 * Math.cos(2 * Math.PI * i / (n - 1)))
  return { sr, s }
}
function goertzel({ sr, s }, f) {        // power at f
  const w = 2 * Math.PI * f / sr, c = 2 * Math.cos(w); let p1 = 0, p2 = 0
  for (let i = 0; i < s.length; i++) { const p = s[i] + c * p1 - p2; p2 = p1; p1 = p }
  return p1 * p1 + p2 * p2 - c * p1 * p2
}
const db = p => 10 * Math.log10(p + 1e-20)
const median = a => { const b = [...a].sort((x, y) => x - y); return b[Math.floor(b.length / 2)] }
// the in-between cycle harmonics vs the firing ones, in dB (median of each set, first `kmax` cycle harmonics)
function rumble(w, rpm, ncyl, kmax = 24) {
  const S = seg(w), fc = rpm / 120, fire = [], mid = []
  for (let k = 1; k <= kmax; k++) (k % (ncyl / 2 || 1) === 0 ? fire : mid).push(db(goertzel(S, k * fc)))
  return median(mid) - Math.max(...fire)
}
// the exhaust envelope: power of each FIRING harmonic, by frequency → the frequency of the loudest under 1 kHz
function hump(w, rpm, ncyl) {
  const S = seg(w), ff = rpm / 60 * ncyl / 2; let best = -1e9, at = 0
  for (let f = ff; f < 1000; f += ff) { const p = db(goertzel(S, f)); if (p > best) { best = p; at = f } }
  return at
}
function centroid(w, rpm, ncyl) {
  const S = seg(w), ff = rpm / 60 * ncyl / 2; let num = 0, den = 0
  for (let f = ff; f < 8000; f += ff) { const p = goertzel(S, f); num += f * p; den += p }
  return num / den
}
function level({ sr, x }) {
  let pk = 0, ss = 0, n = 0, nan = 0
  for (let i = sr; i < x.length; i++) { const v = x[i]; if (v !== v) nan++; const a = Math.abs(v); if (a > pk) pk = a; ss += v * v; n++ }
  return { pk: 20 * Math.log10(pk + 1e-9), rms: 10 * Math.log10(ss / n + 1e-20), nan }
}

const rows = []
const check = (name, ok, got, want) => rows.push({ name, ok, got, want })
const NCYL = [1, 2, 3, 4, 4, 5, 6, 6, 6, 8, 8, 12]
const LNAME = ['single', 'V-twin', 'inline-3', 'inline-4', 'flat-4', 'inline-5', 'inline-6', 'V6', 'flat-6', 'V8 cross', 'V8 flat', 'V12']

// RUMBLE + CONTROL
{
  const i6 = rumble(render('LAYOUT=6,RPM=1418,LOAD=0.25f,EXH=0.2f', 'i6'), 1418, 6)
  check('rumble  inline-6 @1418 (ref 14-25 dB under)', i6 > -32 && i6 < -10, i6.toFixed(1) + ' dB', '-32 .. -10')
  const v8 = rumble(render('LAYOUT=9,RPM=800,LOAD=0.2f,EXH=0.4f', 'v8'), 800, 8)
  check('rumble  crossplane V8 idle (ref: a few dB)', v8 > -22, v8.toFixed(1) + ' dB', '> -22')
  const even = rumble(render('LAYOUT=6,RPM=1418,LOAD=0.25f,EXH=0.2f,ROUGH=0.0f', 'i6even'), 1418, 6)
  check('CONTROL inline-6 ROUGH=0 is an even machine', even < -45, even.toFixed(1) + ' dB', '< -45')
  check('CONTROL the rumble is the ROUGH (≥ 20 dB apart)', i6 - even >= 20, (i6 - even).toFixed(1) + ' dB', '>= 20')
}
// FIXED resonance
{
  const a = hump(render('LAYOUT=9,RPM=1500,LOAD=0.5f,EXH=0.4f,ROUGH=0.0f', 'h1'), 1500, 8)
  const b = hump(render('LAYOUT=9,RPM=3000,LOAD=0.5f,EXH=0.4f,ROUGH=0.0f', 'h2'), 3000, 8)
  const oct = Math.abs(Math.log2(b / a))
  check('fixed   V8 hump stays put as revs double', oct < 0.35, `${a.toFixed(0)} → ${b.toFixed(0)} Hz`, '< 0.35 oct apart')
}
// LOAD brightens
{
  const lo = centroid(render('LAYOUT=6,RPM=3000,LOAD=0.1f,EXH=0.5f', 'l0'), 3000, 6)
  const hi = centroid(render('LAYOUT=6,RPM=3000,LOAD=1.0f,EXH=0.5f', 'l1'), 3000, 6)
  check('load    a hard pull is brighter', hi > lo * 1.1, `${lo.toFixed(0)} → ${hi.toFixed(0)} Hz`, '+10%')
}
// KNOCK: a diesel's block clatter lifts the 1-3 kHz band (the CC0 diesels sit 5-17 dB under their peak there;
// a petrol idle ~20); KNOCK 0 must leave it alone
{
  const band = w => { const S = seg(w); let p = 0; for (let f = 1000; f <= 3000; f += 25) p += goertzel(S, f); return db(p) }
  const tot = w => { const S = seg(w); let p = 0; for (let f = 50; f <= 6000; f += 25) p += goertzel(S, f); return db(p) }
  const d0 = render('LAYOUT=3,RPM=750,LOAD=0.15f,EXH=0.3f,KNOCK=0.0f', 'k0'), d1 = render('LAYOUT=3,RPM=750,LOAD=0.15f,EXH=0.3f,KNOCK=0.75f', 'k1')
  const r0 = band(d0) - tot(d0), r1 = band(d1) - tot(d1)
  check('knock   a diesel clatters at 1-3 kHz', r1 - r0 >= 8, `${r0.toFixed(1)} → ${r1.toFixed(1)} dB of total`, '+8 dB')
}
// LEVEL, every layout
for (let L = 0; L < NCYL.length; L++) {
  const idle = level(render(`LAYOUT=${L},RPM=800,LOAD=0.2f,EXH=0.3f`, 'i' + L))
  const red = level(render(`LAYOUT=${L},RPM=6500,LOAD=1.0f,EXH=1.0f`, 'r' + L))
  check(`level   ${LNAME[L].padEnd(8)} idle..redline`, !idle.nan && !red.nan && idle.rms > -42 && red.pk < -1,
    `idle rms ${idle.rms.toFixed(1)} · red pk ${red.pk.toFixed(1)}`, 'idle > -42 rms, red < -1 pk')
}
fs.rmSync(TMP, { recursive: true, force: true })

const bad = rows.filter(r => !r.ok)
for (const r of rows) if (!QUIET || !r.ok) console.log(`${r.ok ? '✓' : '✗'} ${r.name.padEnd(48)} ${String(r.got).padEnd(30)} want ${r.want}`)
console.log(`motor-check: ${rows.length - bad.length}/${rows.length}`)
process.exit(bad.length ? 1 : 0)
