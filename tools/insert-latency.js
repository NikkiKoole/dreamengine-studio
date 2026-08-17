#!/usr/bin/env node
/*
 * insert-latency.js — how long does a sample take to get from de_audio_input() to the output?
 *
 * THE QUESTION THIS ANSWERS. docs/design/auv3-plugin-types.md §4.1 wants to make a cart an `aumf`
 * (audio-effect) plug-in, so the host's track runs through the cart's pedal chain. The seam for that
 * already exists (`de_audio_input`, and `input_monitor` routes it into the master insert chain), but
 * it was built for ANALYSIS — mic_level()/mic_pitch() — and §4.1's first caveat is that its insert
 * latency is UNMEASURED. An envelope follower does not care. A guitar pedal cares enormously, and a
 * plug-in must also be able to TELL the host its latency so the host can compensate, which requires
 * the figure to be CONSTANT. So there are three questions, not one:
 *
 *   1. how many samples of delay?      (is it usable at all)
 *   2. is it the SAME every time?      (can it be declared to a host, or does it drift)
 *   3. does anything get LOST?         (ring underrun/overflow → dropouts a pedal would expose)
 *
 * HOW. Feed impulses through the real path with a known input offset, render, and find where they
 * came out. The vehicle is the harness's own DE_MIC_WAV hook (runtime/studio.c): it pushes one
 * frame's worth of input samples and then renders one frame's worth of audio — which is EXACTLY the
 * shape of an AUv3 render block (N samples in, N samples out, same callback). That is what makes a
 * headless number meaningful here instead of merely a fact about our microphone.
 *
 * WHY IMPULSES AND NOT A SINE. A sine tells you phase, and phase is ambiguous by whole periods —
 * exactly the ambiguity that makes a latency figure wrong by a round number. An impulse has one
 * unambiguous arrival. Amplitude 0.5 keeps it under the master soft-clip's ±0.8 knee (sound.h), so
 * the path stays LINEAR and the output amplitude doubles as a unity-gain check.
 *
 * The probe cart is `tools/carts/inslat.c`, deliberately the emptiest cart that can answer this:
 * nothing else in it makes a sound, so every non-zero output sample came through the input ring.
 *
 *   node tools/insert-latency.js              measure (default 6 impulses)
 *   node tools/insert-latency.js --impulses 12
 *   node tools/insert-latency.js --check      self-test: known answers, builds nothing
 *   node tools/insert-latency.js --keep       leave the generated/rendered wavs for inspection
 *
 * ⚠ THIS MEASURES THE ENGINE HALF ONLY. A real host adds its own buffer, and on a non-44.1k host
 * ios/AU/RateConvert.swift is in the path too (gated separately by ios/rate-convert-check). A green
 * number here is a precondition for an insert effect, not a promise about GarageBand.
 */
'use strict';

const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const SR = 44100;                 // SOUND_SAMPLE_RATE — the engine is compile-time 44.1k
const RING = 2048;                // SOUND_EXTIN_LEN (sound.h) — the ceiling any answer must sit under
const AMP = 0.5;                  // under the 0.8 soft-clip knee → the path stays linear

const argv = process.argv.slice(2);
const has = (f) => argv.includes(f);
const val = (f, d) => { const i = argv.indexOf(f); return i >= 0 && argv[i + 1] ? +argv[i + 1] : d; };

/* ─────────────────────────────────────────────────────────────── wav io ── */

// 16-bit PCM mono, which is what DE_MIC_WAV accepts (it rejects anything else outright).
function writeWav16(file, samples, sr = SR) {
  const n = samples.length;
  const buf = Buffer.alloc(44 + n * 2);
  buf.write('RIFF', 0); buf.writeUInt32LE(36 + n * 2, 4); buf.write('WAVE', 8);
  buf.write('fmt ', 12); buf.writeUInt32LE(16, 16); buf.writeUInt16LE(1, 20);
  buf.writeUInt16LE(1, 22); buf.writeUInt32LE(sr, 24); buf.writeUInt32LE(sr * 2, 28);
  buf.writeUInt16LE(2, 32); buf.writeUInt16LE(16, 34);
  buf.write('data', 36); buf.writeUInt32LE(n * 2, 40);
  for (let i = 0; i < n; i++) {
    let v = Math.round(Math.max(-1, Math.min(1, samples[i])) * 32767);
    buf.writeInt16LE(v, 44 + i * 2);
  }
  fs.writeFileSync(file, buf);
}

// Reads 16-bit PCM, mono or stereo. ⚠ Returns the MAX of |L| and |R| per frame rather than the mean:
// the monitor sums the input into both channels equally, so a mean would be right here — but a mean
// is what silently hid a whole class of stereo bug elsewhere in this repo, and a peak cannot.
function readWavPeak(file) {
  const b = fs.readFileSync(file);
  let p = 12, ch = 1, bits = 16, dataOff = -1, dataLen = 0;
  while (p + 8 <= b.length) {
    const id = b.toString('ascii', p, p + 4), sz = b.readUInt32LE(p + 4);
    if (id === 'fmt ') { ch = b.readUInt16LE(p + 10); bits = b.readUInt16LE(p + 22); }
    else if (id === 'data') { dataOff = p + 8; dataLen = sz; break; }
    p += 8 + sz + (sz & 1);
  }
  if (dataOff < 0 || bits !== 16) throw new Error(`${file}: not 16-bit PCM with a data chunk`);
  const frames = Math.floor(dataLen / (2 * ch));
  const out = new Float32Array(frames);
  for (let i = 0; i < frames; i++) {
    let m = 0;
    for (let c = 0; c < ch; c++) {
      const v = Math.abs(b.readInt16LE(dataOff + (i * ch + c) * 2) / 32768);
      if (v > m) m = v;
    }
    out[i] = m;
  }
  return out;
}

/* ────────────────────────────────────────────────────────────── analysis ── */

// Find one local peak per expected impulse, searching only FORWARD of the input offset (a negative
// latency is not a physical answer, and allowing one lets noise before the impulse win the search).
// `window` bounds it to the ring's own depth: past that, whatever we found is a different impulse.
function findArrivals(sig, inputOffsets, window, floor) {
  return inputOffsets.map((off) => {
    let best = -1, bestV = 0;
    const end = Math.min(sig.length, off + window);
    for (let i = off; i < end; i++) if (sig[i] > bestV) { bestV = sig[i]; best = i; }
    return bestV >= floor ? { at: best, amp: bestV, delay: best - off } : { at: -1, amp: bestV, delay: null };
  });
}

/* ────────────────────────────────────────────────────────────── selfcheck ── */

// Known answers, over SYNTHETIC signals — so the analyser is judged without compiling a cart or
// touching audio. A broken detector and a broken engine print the same disappointing table, and this
// is the half that says which. (The pattern is repo-standard: see tools/*-check.js --selfcheck.)
function selfcheck() {
  let pass = 0, fail = 0;
  const t = (name, ok, detail) => { ok ? pass++ : fail++; console.log(`  ${ok ? '\x1b[32m✓\x1b[0m' : '\x1b[31m✗\x1b[0m'} ${name}${detail ? '  — ' + detail : ''}`); };

  // wav round-trip: what we write is what we read (within 16-bit quantisation)
  const tmp = path.join(ROOT, 'build', '.inslat-selfcheck.wav');
  fs.mkdirSync(path.dirname(tmp), { recursive: true });
  const s = new Float32Array(1000); s[100] = 0.5; s[500] = -0.25;
  writeWav16(tmp, s);
  const rt = readWavPeak(tmp);
  t('wav round-trip length', rt.length === 1000, `${rt.length}`);
  t('wav round-trip peak +', Math.abs(rt[100] - 0.5) < 1e-3, rt[100].toFixed(5));
  t('wav round-trip peak - (read as magnitude)', Math.abs(rt[500] - 0.25) < 1e-3, rt[500].toFixed(5));
  t('wav round-trip silence', rt[300] === 0, `${rt[300]}`);

  // a KNOWN delay must be recovered exactly
  const sig = new Float32Array(5000);
  const ins = [100, 1100, 2100];
  const D = 37;
  for (const o of ins) sig[o + D] = 0.5;
  const got = findArrivals(sig, ins, RING, 0.01);
  t('recovers a known constant delay', got.every(g => g.delay === D), got.map(g => g.delay).join(','));
  t('reports the amplitude it found', got.every(g => Math.abs(g.amp - 0.5) < 1e-6), got[0].amp.toFixed(3));

  // a VARIABLE delay must show up as variable, not be averaged away
  const sig2 = new Float32Array(5000);
  [[100, 10], [1100, 20], [2100, 30]].forEach(([o, d]) => { sig2[o + d] = 0.5; });
  const got2 = findArrivals(sig2, ins, RING, 0.01);
  t('a DRIFTING delay reads as drifting', new Set(got2.map(g => g.delay)).size === 3, got2.map(g => g.delay).join(','));

  // a MISSING impulse must be reported missing, not silently scored as delay 0
  const sig3 = new Float32Array(5000);
  sig3[100 + 12] = 0.5; sig3[2100 + 12] = 0.5;            // the middle one never arrives
  const got3 = findArrivals(sig3, ins, 900, 0.01);
  t('a DROPPED impulse reads as missing', got3[1].delay === null, JSON.stringify(got3.map(g => g.delay)));

  // ⚠ the guard that matters most: silence must NOT produce a confident answer
  const got4 = findArrivals(new Float32Array(5000), ins, RING, 0.01);
  t('silence yields no arrivals (a dead render cannot look fast)', got4.every(g => g.delay === null), JSON.stringify(got4.map(g => g.delay)));

  // and the search must never look backwards
  const sig5 = new Float32Array(5000);
  sig5[50] = 0.9;      // a big blip BEFORE the input offset
  sig5[100 + 25] = 0.5;
  const got5 = findArrivals(sig5, [100], RING, 0.01);
  t('never reports a negative latency', got5[0].delay === 25, `${got5[0].delay}`);

  fs.unlinkSync(tmp);
  console.log(`\n${fail === 0 ? '\x1b[32m' : '\x1b[31m'}${pass}/${pass + fail} known answers correct\x1b[0m`);
  return fail === 0 ? 0 : 1;
}

/* ─────────────────────────────────────────────────────────────── measure ── */

function measure() {
  const nImp = val('--impulses', 6);
  const gap = 4410;                       // 100ms apart: far longer than the ring, so no overlap
  const lead = 4410;                      // let the cart boot + the monitor arm before the first one
  const frames = Math.ceil((lead + gap * (nImp + 1)) / (SR / 60) ) + 30;

  const inWav = path.join(ROOT, 'build', '.inslat-in.wav');
  const outWav = path.join(ROOT, 'build', '.inslat-out.wav');
  fs.mkdirSync(path.dirname(inWav), { recursive: true });

  const offsets = [];
  const sig = new Float32Array(lead + gap * (nImp + 1));
  for (let i = 0; i < nImp; i++) { const o = lead + gap * i; sig[o] = AMP; offsets.push(o); }
  writeWav16(inWav, sig);

  console.log(`▸ ${nImp} impulses of ${AMP} at ${SR}Hz, ${gap} samples (${(gap / SR * 1000).toFixed(0)}ms) apart`);
  console.log(`▸ rendering ${frames} frames of \`inslat\` with DE_MIC_WAV…`);

  try {
    execFileSync('node', ['tools/play.js', 'inslat', 'script', '/dev/null', '--headless',
                          '--frames', String(frames), '--wav', outWav],
                 { cwd: ROOT, env: { ...process.env, DE_MIC_WAV: inWav }, stdio: ['ignore', 'ignore', 'pipe'], maxBuffer: 1 << 24 });
  } catch (e) {
    console.error('✗ render failed:\n' + (e.stderr ? e.stderr.toString().split('\n').slice(-15).join('\n') : e.message));
    return 2;
  }

  const out = readWavPeak(outWav);
  // ⚠ LIVENESS FIRST. Everything below is a delay between two events, and if the render is silent
  // every one of them is "missing" — which reads like a broken ring when it may be a broken probe.
  let peak = 0; for (const v of out) if (v > peak) peak = v;
  console.log(`▸ rendered ${out.length} samples, peak ${peak.toFixed(4)}`);
  if (peak < 0.01) {
    console.log('\n\x1b[31m✗ THE RENDER IS SILENT\x1b[0m — nothing reached the output, so there is no latency to measure.');
    console.log('  Check in this order: DE_MIC_WAV loaded (stderr says so) · mic_active() · input_monitor gain.');
    return 1;
  }

  const arrivals = findArrivals(out, offsets, RING, peak * 0.25);
  console.log('\n  #   input      output     delay              amp');
  console.log('  ──────────────────────────────────────────────────────');
  for (let i = 0; i < arrivals.length; i++) {
    const a = arrivals[i];
    if (a.delay === null) { console.log(`  ${String(i + 1).padEnd(3)} ${String(offsets[i]).padEnd(10)} ${'—'.padEnd(10)} \x1b[31mMISSING\x1b[0m`); continue; }
    console.log(`  ${String(i + 1).padEnd(3)} ${String(offsets[i]).padEnd(10)} ${String(a.at).padEnd(10)} ` +
                `${String(a.delay).padStart(5)} smp (${(a.delay / SR * 1000).toFixed(2)}ms)   ${a.amp.toFixed(4)}`);
  }

  const found = arrivals.filter(a => a.delay !== null);
  const missing = arrivals.length - found.length;
  const delays = found.map(a => a.delay);
  const lo = Math.min(...delays), hi = Math.max(...delays);
  const gains = found.map(a => a.amp / AMP);

  console.log('\nVERDICT');
  let bad = 0;
  const say = (ok, msg) => { if (!ok) bad++; console.log(`  ${ok ? '\x1b[32m✓\x1b[0m' : '\x1b[31m✗\x1b[0m'} ${msg}`); };

  say(missing === 0, missing === 0 ? 'nothing was dropped' : `${missing} impulse(s) never arrived — the ring dropped or underran`);
  if (found.length) {
    say(hi - lo === 0, hi - lo === 0
      ? `latency is CONSTANT at ${lo} samples (${(lo / SR * 1000).toFixed(2)}ms) — declarable to a host`
      : `latency VARIES: ${lo}..${hi} samples (${((hi - lo) / SR * 1000).toFixed(2)}ms spread) — a host cannot compensate a moving target`);
    say(hi < RING, `within the ${RING}-sample ring (${(RING / SR * 1000).toFixed(0)}ms ceiling)`);
    const gLo = Math.min(...gains), gHi = Math.max(...gains);
    say(gLo > 0.9 && gHi < 1.1, `unity gain through the monitor (${gLo.toFixed(3)}..${gHi.toFixed(3)}x at input_monitor(1.0))`);
    // The pedal test. 10ms is where a player starts hearing their own attack late; a plug-in that
    // reports its latency lets the host align tracks, but a PERFORMER hears the raw number.
    const ms = hi / SR * 1000;
    say(ms < 10, ms < 10 ? `${ms.toFixed(2)}ms is inside the ~10ms a player will not notice`
                         : `${ms.toFixed(2)}ms is above ~10ms — playable-through is doubtful, an insert on a recorded track is fine`);
  }

  if (!has('--keep')) { fs.unlinkSync(inWav); fs.unlinkSync(outWav); }
  else console.log(`\n  kept ${path.relative(ROOT, inWav)} and ${path.relative(ROOT, outWav)}`);

  console.log(bad === 0 ? '\n\x1b[32mPASS\x1b[0m — the input path is insert-grade.'
                        : `\n\x1b[31m${bad} concern(s)\x1b[0m — see docs/design/auv3-plugin-types.md §4.1.`);
  return bad === 0 ? 0 : 1;
}

process.exit(has('--check') || has('--selfcheck') ? selfcheck() : measure());
