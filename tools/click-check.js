#!/usr/bin/env node
// click-check.js — the CLICK / SPLICE oracle: find waveform DISCONTINUITIES in a rendered WAV and say
// WHERE they are, so "does this edit crackle?" is a measurement instead of an argument.
//
//   node tools/click-check.js <file.wav> [more.wav ...] [options]
//
//   --top <n>        how many events to list per file (default 8)
//   --thresh <x>     an event counts as splice-like at this multiple of the LOCAL step-rms (default 4)
//   --window <ms>    local-context window, also the event de-dup distance (default 10)
//   --quiet          print one PASS/FAIL line per file and exit nonzero if any file has an event
//                    at/above --thresh. The CI form.
//
// WHY A MULTIPLE OF THE LOCAL STEP-RMS, not a raw threshold: a bright loud waveform legitimately has big
// sample-to-sample steps (a saw's flyback is a huge step and is not a click). What marks a splice is a
// step that is anomalous *against the signal around it*. Measured on real takes, a cart's own waveform
// slope sits at ~2x its local step-rms, and an audible click lands at 6-20x. Hence the default of 4.
//
// WHAT IT CANNOT DO: it finds discontinuities, not every kind of unwanted noise. Aliasing, denormal
// fizz, quantisation hiss and a too-fast envelope are all inaudible to this metric — see the harmonic /
// level / fx gates for those (docs/guides/checks-and-oracles.md).
//
// BORN FROM: martenot's MODE_MORPH (plan item 1.7). It rebuilt a wavetable in 8 quantised steps across a
// swell, and `wave_set` replaces the table under a running oscillator, so every step crossing jumped the
// output from old[phase] to new[phase] — 16 clicks per swell. It shipped with a source comment asserting
// the stepping was "inaudible", which had never been measured; the owner's ear caught it, and this
// detector then localised it in one run (15.4x the local step-rms, at the step crossings) and sized the
// fix (64 steps → nothing above 4x). No oracle we had could see it: an envelope plot cannot tell a clean
// ramp from a splice, which is the same blind spot that made the brass release call in item 1.4 hard.
//
// Docs: docs/guides/checks-and-oracles.md · docs/design/synth-secrets-plan.md item 1.7

const fs = require('fs');

const argv = process.argv.slice(2);
const flag = (n, d) => { const i = argv.indexOf('--' + n); return i >= 0 && i + 1 < argv.length ? argv[i + 1] : d };
const has = (n) => argv.includes('--' + n);
const files = argv.filter((a, i) => !a.startsWith('--') && !(i > 0 && argv[i - 1].startsWith('--') && !['--quiet'].includes(argv[i - 1])));

if (!files.length) {
  console.error('usage: node tools/click-check.js <file.wav> [more.wav ...] [--top n] [--thresh x] [--window ms] [--quiet]');
  process.exit(1);
}

const TOP = parseInt(flag('top', '8'), 10);
const THRESH = parseFloat(flag('thresh', '4'));
const WIN_MS = parseFloat(flag('window', '10'));
const QUIET = has('quiet');

// ── minimal WAV reader (16-bit PCM, any channel count) ────────
function readWav(file) {
  const b = fs.readFileSync(file);
  if (b.toString('ascii', 0, 4) !== 'RIFF' || b.toString('ascii', 8, 12) !== 'WAVE')
    throw new Error(`${file}: not a RIFF/WAVE file`);
  let pos = 12, fmt = null, dataOff = 0, dataLen = 0;
  while (pos + 8 <= b.length) {
    const id = b.toString('ascii', pos, pos + 4), sz = b.readUInt32LE(pos + 4);
    if (id === 'fmt ') fmt = { ch: b.readUInt16LE(pos + 10), rate: b.readUInt32LE(pos + 12), bits: b.readUInt16LE(pos + 22) };
    if (id === 'data') { dataOff = pos + 8; dataLen = Math.min(sz, b.length - pos - 8); break }
    pos += 8 + sz + (sz & 1);
  }
  if (!fmt || !dataOff) throw new Error(`${file}: no fmt/data chunk`);
  if (fmt.bits !== 16) throw new Error(`${file}: only 16-bit PCM supported (got ${fmt.bits})`);
  const n = Math.floor(dataLen / 2 / fmt.ch);
  const x = new Float32Array(n);
  for (let i = 0; i < n; i++) {                    // sum to mono: a click is a click in either channel
    let s = 0;
    for (let c = 0; c < fmt.ch; c++) s += b.readInt16LE(dataOff + (i * fmt.ch + c) * 2) / 32768;
    x[i] = s / fmt.ch;
  }
  return { x, rate: fmt.rate, ch: fmt.ch };
}

function analyze(file) {
  const { x, rate } = readWav(file);
  const n = x.length;
  let peak = 0;
  for (let i = 0; i < n; i++) { const a = Math.abs(x[i]); if (a > peak) peak = a }
  const W = Math.max(4, Math.floor(rate * WIN_MS / 1000));

  // running step-rms over the PRECEDING window: what a normal step looks like right there
  const cand = [];
  let sumSq = 0;
  for (let i = 1; i < n; i++) {
    const d = x[i] - x[i - 1];
    if (i > W) { const dOld = x[i - W] - x[i - W - 1]; sumSq -= dOld * dOld }
    const rms = Math.sqrt(sumSq / Math.min(i, W));            // BEFORE adding this step, so a spike
    sumSq += d * d;                                            // never inflates its own baseline
    const ad = Math.abs(d);
    if (peak > 0 && ad > 0.005 * peak && rms > 0) cand.push({ i, d: ad, mult: ad / rms });
  }
  cand.sort((a, b) => b.mult - a.mult);
  const kept = [];
  for (const c of cand) {                                      // one report per event
    if (kept.some((k) => Math.abs(k.i - c.i) < W)) continue;
    kept.push(c);
    if (kept.length >= Math.max(TOP, 64)) break;
  }
  return { file, rate, n, peak, events: kept };
}

let failed = false;
for (const f of files) {
  let r;
  try { r = analyze(f) } catch (e) { console.error('click-check: ' + e.message); process.exitCode = 1; continue }
  const bad = r.events.filter((e) => e.mult >= THRESH);
  if (bad.length) failed = true;
  const name = f.split('/').pop();

  if (QUIET) {
    console.log(bad.length
      ? `✘ ${name}  ${bad.length} splice-like event(s) ≥ ${THRESH}x local step-rms · worst ${bad[0].mult.toFixed(1)}x at t=${(bad[0].i / r.rate).toFixed(3)}s`
      : `✓ ${name}  no discontinuity ≥ ${THRESH}x local step-rms (worst ${r.events.length ? r.events[0].mult.toFixed(1) : '0.0'}x)`);
    continue;
  }

  console.log(`\n${name}  ${r.n} samples @ ${r.rate}Hz · peak ${r.peak.toFixed(4)}`);
  console.log(`  largest first-difference events, one per ${WIN_MS}ms, worst first:`);
  if (!r.events.length) console.log('    (none above the noise floor)');
  for (const e of r.events.slice(0, TOP)) {
    const mark = e.mult >= THRESH ? ' ← SPLICE-LIKE' : '';
    console.log(`    t=${(e.i / r.rate).toFixed(3)}s  step ${(100 * e.d / r.peak).toFixed(1)}% of peak  ${e.mult.toFixed(1)}x local step-rms${mark}`);
  }
  console.log(bad.length
    ? `  ✘ ${bad.length} event(s) at/above ${THRESH}x — listen at those timestamps.`
    : `  ✓ nothing at/above ${THRESH}x — no splice this metric can see.`);
}

if (QUIET && failed) process.exit(1);
