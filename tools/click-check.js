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
//   --selfcheck      known answers for the DETECTOR itself, on audio it synthesises (no cart, no
//                    engine). 18 assertions, mutation-tested. Run it FIRST when this gate reports
//                    something surprising: a clean render and a blind detector print the same line.
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
// ⚠ WHAT MAKES IT SHOUT WITHOUT ANYTHING BEING WRONG, measured and pinned in --selfcheck:
//   · AN ONSET AFTER A QUIET PASSAGE. The baseline is the LOCAL step-rms, so a hit landing on a
//     near-silent tail divides by almost nothing. A real acidcandy render scores 44 events, worst
//     1834x, all of them kick drums. On sparse percussive material this gate is close to useless
//     as a pass/fail; use it to COMPARE a before and after of the same take, which is what it is
//     for. (An onset out of EXACT digital silence is skipped instead — the rms>0 guard.)
//   · A NAKED GEOMETRIC WAVE. An un-bandlimited saw or square really is a train of discontinuities
//     (~15x). The claim above that a flyback is not a click holds for REAL engine output, which is
//     band-limited and enveloped, not for a synthesised ideal.
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

if (!files.length && !has('selfcheck')) {
  console.error('usage: node tools/click-check.js <file.wav> [more.wav ...] [--top n] [--thresh x] [--window ms] [--quiet]');
  console.error('       node tools/click-check.js --selfcheck    known answers for the DETECTOR (synthesises its own audio)');
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

// ── --selfcheck: KNOWN ANSWERS FOR THE DETECTOR ──────────────────────────────
// This oracle's failure mode is not a false alarm, it is going BLIND: if the detector stops
// finding discontinuities, every render passes and the next martenot ships crackling with a green
// check beside it. A clean take and a broken detector print the same line. So: synthesise audio
// whose answer is known by construction, and assert BOTH directions — clean things stay silent AND
// spliced things are caught. Every number below was measured before it was written down.
const os = require('os'), pathmod = require('path'), { spawnSync } = require('child_process');
const SC_SR = 44100, SC_F = 441;    // 441 Hz at 44.1k = a period of exactly 100 samples, so a phase
                                    // jump can be placed on an exact zero crossing and its size is known

function scWriteWav(file, chans, sr) {
  const ch = chans.length, n = chans[0].length, b = Buffer.alloc(44 + n * ch * 2);
  b.write('RIFF', 0); b.writeUInt32LE(36 + n * ch * 2, 4); b.write('WAVE', 8);
  b.write('fmt ', 12); b.writeUInt32LE(16, 16); b.writeUInt16LE(1, 20); b.writeUInt16LE(ch, 22);
  b.writeUInt32LE(sr, 24); b.writeUInt32LE(sr * ch * 2, 28); b.writeUInt16LE(ch * 2, 32); b.writeUInt16LE(16, 34);
  b.write('data', 36); b.writeUInt32LE(n * ch * 2, 40);
  for (let i = 0; i < n; i++) for (let c = 0; c < ch; c++)
    b.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(chans[c][i] * 32767))), 44 + (i * ch + c) * 2);
  fs.writeFileSync(file, b);
}

function selfcheck() {
  const N = SC_SR * 0.5, K = Math.floor(SC_SR * 0.25);      // splices land at exactly t = 0.250s
  const dir = fs.mkdtempSync(pathmod.join(os.tmpdir(), 'clickcheck-selfcheck-'));
  const gen = (f) => { const x = new Float32Array(N); for (let i = 0; i < N; i++) x[i] = f(i); return x };
  const put = (name, chans) => { const p = pathmod.join(dir, name + '.wav'); scWriteWav(p, chans, SC_SR); return p };
  const sine = (i, dph = 0) => 0.6 * Math.sin(2 * Math.PI * (i * SC_F / SC_SR + dph));

  let pass = 0, fail = 0;
  const ok = (name, cond, got) => {
    if (cond) { pass++; console.log(`  ✓ ${name}`) }
    else { fail++; console.log(`  ✗ ${name}   got: ${got}`) }
  };
  const worst = (p) => { const r = analyze(p); return r.events.length ? r.events[0] : { mult: 0, i: -1 } };
  const nEvents = (p, t) => analyze(p).events.filter(e => e.mult >= (t || THRESH)).length;
  const ms = (i) => (1000 * i / SC_SR);

  try {
    console.log('click-check --selfcheck — known answers for the detector (audio is synthesised here)\n');

    console.log('CLEAN SIGNALS STAY SILENT');
    const clean = put('clean', [gen(i => sine(i))]);
    ok('a pure sine trips nothing', worst(clean).mult < THRESH, worst(clean).mult.toFixed(1));
    const env = put('enveloped', [gen(i => sine(i) * Math.min(1, i / 2000) * Math.min(1, (N - i) / 2000))]);
    ok('an ENVELOPED sine stays silent (a fade is not a splice)', worst(env).mult < THRESH, worst(env).mult.toFixed(1));
    const tri = put('tri', [gen(i => { const ph = (i * SC_F / SC_SR) % 1; return 0.6 * (4 * Math.abs(ph - 0.5) - 1) })]);
    ok('a TRIANGLE stays silent — a slope corner is not a value jump', worst(tri).mult < THRESH, worst(tri).mult.toFixed(1));
    const sil = put('silence', [gen(() => 0)]);
    ok('digital silence yields no events and does not divide by zero',
       analyze(sil).events.length === 0, analyze(sil).events.length);

    console.log('\nSPLICES ARE CAUGHT, AND LOCALISED');
    const jump = put('phasejump', [gen(i => sine(i, i >= K ? 0.25 : 0))]);
    ok('a quarter-cycle PHASE JUMP is caught', worst(jump).mult >= 6, worst(jump).mult.toFixed(1));
    ok('  …within 1 ms of where it was injected', Math.abs(worst(jump).i - K) < SC_SR / 1000, `${ms(worst(jump).i - K).toFixed(2)} ms off`);
    const dc = put('dcstep', [gen(i => sine(i) + (i >= K ? 0.3 : 0))]);
    ok('a DC STEP is caught', worst(dc).mult >= 6, worst(dc).mult.toFixed(1));
    // THE CASE THIS TOOL WAS BORN FOR: wave_set swaps the table under a running oscillator, so the
    // output jumps from old[phase] to new[phase] with the phase itself perfectly continuous.
    const swap = put('tableswap', [gen(i => { const p = 2 * Math.PI * i * SC_F / SC_SR;
      return i < K ? 0.6 * Math.sin(p) : 0.6 * (0.7 * Math.sin(p) + 0.3 * Math.sin(3 * p)) })]);
    ok('a WAVETABLE SWAP at continuous phase is caught (the martenot case)', worst(swap).mult >= 6, worst(swap).mult.toFixed(1));
    ok('  …and it is localised too', Math.abs(worst(swap).i - K) < SC_SR / 1000, `${ms(worst(swap).i - K).toFixed(2)} ms off`);

    console.log('\nTHE DE-DUP WINDOW COLLAPSES ONE EVENT, NOT TWO');
    const close = put('twoclose', [gen(i => sine(i, (i >= K ? 0.25 : 0) + (i >= K + 40 ? 0.25 : 0)))]);
    ok('two splices 0.9 ms apart report as ONE event', nEvents(close) === 1, nEvents(close));
    const far = put('twofar', [gen(i => sine(i, (i >= K ? 0.25 : 0) + (i >= K + 3000 ? 0.25 : 0)))]);
    ok('two splices 68 ms apart stay TWO events', nEvents(far) === 2, nEvents(far));

    console.log('\nBOTH CHANNELS ARE IN THE PATH');
    const oneSide = put('rightonly', [gen(i => sine(i)), gen(i => sine(i, i >= K ? 0.25 : 0))]);
    ok('a splice in the RIGHT channel only is still caught', worst(oneSide).mult >= 4, worst(oneSide).mult.toFixed(1));

    console.log('\nTHE THRESHOLD KNOB IS REAL, AND THE GATE CAN GO RED');
    ok('the phase jump is BELOW a --thresh of 40', nEvents(jump, 40) === 0, nEvents(jump, 40));
    ok('  …and above the default of 4', nEvents(jump, 4) >= 1, nEvents(jump, 4));
    const runq = (f, extra) => spawnSync(process.execPath,
      [__filename, f, '--quiet'].concat(extra || []), { encoding: 'utf8' }).status;
    ok('--quiet exits 0 on the clean file', runq(clean) === 0, runq(clean));
    ok('--quiet exits 1 on the spliced file', runq(jump) === 1, runq(jump));
    ok('--quiet exits 0 on the spliced file at --thresh 40', runq(jump, ['--thresh', '40']) === 0, runq(jump, ['--thresh', '40']));

    console.log('\nCHARACTERISTICS WORTH PINNING, NOT FIXING');
    // An ONSET AFTER SILENCE scores enormously, because the local baseline it is measured against
    // is ~0. That is not a defect, it is what a ratio metric does — but it means a SPARSE
    // PERCUSSIVE render lights this oracle up with dozens of "splices" that are just kick drums.
    // Measured on a real acidcandy render: 44 events, worst 1834x, all at note onsets. Pinned so
    // the number is a known property rather than a fresh scare every time someone tries it.
    // ⚠ TWO DIFFERENT BEHAVIOURS, and the difference is one guard. `rms > 0` skips a step whose
    // preceding window is EXACTLY zero, so an onset out of true digital silence is not reported at
    // all. Out of a QUIET TAIL — which is what real audio has — the baseline is tiny but non-zero
    // and the ratio explodes instead. Both are pinned because both surprise people.
    const onsetPure = put('onset-pure-silence', [gen(i => i < K ? 0 : sine(i))]);
    ok('an onset out of EXACT digital silence is skipped (the rms>0 guard, not a detection)',
       worst(onsetPure).mult < THRESH, worst(onsetPure).mult.toFixed(1));
    const onsetTail = put('onset-quiet-tail', [gen(i => i < K ? 0.0002 * Math.sin(2 * Math.PI * i * 60 / SC_SR) : sine(i))]);
    ok('an onset out of a QUIET TAIL scores huge — a sparse drum take lights this up',
       worst(onsetTail).mult >= 50, worst(onsetTail).mult.toFixed(1));

    // ⚠ A NAKED geometric saw trips this metric hard (measured ~15x), and that is CORRECT: an
    // un-bandlimited saw really is a train of discontinuities. The header's "a saw's flyback is not
    // a click" is about REAL engine output, which is band-limited and enveloped. Pinned so nobody
    // "fixes" the false positive by widening the metric until it can no longer see a splice either.
    const saw = put('nakedsaw', [gen(i => { const ph = (i * SC_F / SC_SR) % 1; return 0.6 * (2 * ph - 1) })]);
    ok('a NAKED geometric saw DOES trip it — point this oracle at rendered audio, not ideals',
       worst(saw).mult >= THRESH, worst(saw).mult.toFixed(1));

    console.log(`\n${fail === 0 ? '✓' : '✗'} ${pass}/${pass + fail} known answers correct`);
    return fail === 0 ? 0 : 1;
  } finally { fs.rmSync(dir, { recursive: true, force: true }) }
}

if (has('selfcheck')) process.exit(selfcheck());

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
