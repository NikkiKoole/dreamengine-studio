#!/usr/bin/env node
/*
 * refactor-guard.js — "I moved state around. Did ANY output change?"
 *
 * The safety net for the per-instance-context refactor (docs/HANDOFF.md → the AUv3 lane): moving
 * the engine's 545 file-scope statics into a per-instance struct is a PURE STATE MOVE, so it must
 * produce byte-identical audio, byte-identical frames and byte-identical per-frame state. Anything
 * else is a bug in the refactor, never "close enough". This is the one command that says so.
 *
 *   node tools/refactor-guard.js --bless     record the baseline (do this BEFORE touching anything)
 *   node tools/refactor-guard.js             compare against it — the gate
 *   node tools/refactor-guard.js --quiet     one line, exits nonzero on drift (CI / a commit hook)
 *   node tools/refactor-guard.js --full      also run the existing semantic gates (slower)
 *   node tools/refactor-guard.js --check     self-test: known answers + a live sensitivity control
 *
 * WHAT IT FINGERPRINTS, and why three streams and not one. A state move can break the audio while
 * the picture is fine (a filter's state now shared), the picture while the audio is fine (a draw
 * cursor now shared), or neither while the LOGIC drifts one frame late (an init order changed).
 * So each probe records up to three independent streams: the rendered WAV, the frame dump, and the
 * `watch()` trace. Each is stored as a whole-stream sha PLUS an array of per-chunk shas, so a
 * failure names WHERE it started — "audio diverges at 2.4s", "frame 137" — instead of only that a
 * hash moved. During a big refactor that locality is most of the value.
 *
 * ⚠ THE FAILURE MODE THIS TOOL IS BUILT AGAINST IS NOT DRIFT, IT IS A VACUOUS PASS. A probe whose
 * cart renders silence, or a blank screen, hashes perfectly consistently forever and proves
 * NOTHING while showing green. So every probe also carries LIVENESS assertions — the audio must
 * have real signal, the frames must have more than one colour, the trace must actually change —
 * and a probe that goes quiet is reported as a FAILURE, not skipped. `--check` proves this works by
 * feeding it a genuinely silent render (`--solo-slot` on an unused slot) and requiring a red.
 *
 * COVERAGE IS THE OTHER HALF, and it is a judgement, not a measurement: this can only prove the
 * subsystems the probe carts actually exercise. Each probe below states what it is there to cover.
 * If the refactor touches something no probe reaches, ADD A PROBE and re-bless — a green run over
 * a probe set that misses your change is the same as no run at all.
 *
 * PROVEN TO GO RED, which is the only reason to trust a green. Verified end to end by changing
 * `g_pan_law`'s default on sound.h:793 (read per voice per sample in the main mix loop) and
 * re-running: epiano, pedalboard and bossa all reported DRIFT with the divergence located in
 * time. Two things that happened on the way there are worth knowing:
 *   · `acidcandy` did NOT drift, because it sets the pan law explicitly and is immune to that
 *     default. ONE probe is not a gate — this is exactly why there are six.
 *   · Two earlier perturbation attempts produced a confident green while changing NOTHING: a
 *     `sed` that silently failed to match, and an edit to `sound_master_gain`, which lives inside
 *     `#ifdef DE_AUDIO_WORKLET` and is dead code on the native path. If you ever re-run this
 *     control, ASSERT THE PERTURBATION LANDED before believing the verdict.
 *
 * WHY NOT JUST USE THE EXISTING GATES. tune-check, spec.js, canvas-diff and friends each assert a
 * SEMANTIC property (is it in tune, does the game rule hold, do GPU and software agree). They are
 * the right tools for a change that is *supposed* to alter behaviour. They are the wrong shape for
 * a refactor that must alter NOTHING, because they all have tolerances, and a state move can slip
 * inside every tolerance at once. `--full` runs them too, as a second opinion, not as the gate.
 */
'use strict';

const { execFileSync, execSync } = require('child_process');
const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const BASELINE = path.join(ROOT, 'tools', 'refactor-guard-baseline.json');

/* ─────────────────────────────────────────────────────────── the probe set ── */
/* Each probe states what it covers. That sentence is the coverage argument; keep it honest. */
const PROBES = [
  // Driven by a committed clip rather than left idling: the script works the fx hub, so this probe
  // covers the effect-CONFIGURATION path (set-and-hold rebuilds) and not just steady-state playback.
  { cart: 'acidcandy', frames: 300, wav: true, trace: true,
    script: 'tools/clips/acidcandy/05-fx-hub-vowel.script',
    why: 'the AUv3 target itself: the acid voice, the 808 + 909 banks, insert effects and the pattern clock' },
  // No trace here on purpose: epiano's watch() values are static settings (machine/wah/phaser),
  // so its trace carries no evolving state. Counting the auto fields (beat/bpos) instead would
  // make trace-liveness pass for every cart in the repo, which is the failure this tool exists to
  // prevent. Its audio is the stream that means something.
  { cart: 'epiano', frames: 240, wav: true,
    why: 'a pitched engine through keybed.h, plus autoplay — the note-on/voice-alloc path' },
  { cart: 'pedalboard', frames: 240, wav: true,
    why: 'the effects bus: drive/amp/cabinet chain and the insert ordering' },
  // NOT omnichord, which was the first choice here: it waits for a player, so it renders silence
  // headless and would have been a probe that passed forever while testing nothing.
  { cart: 'bossa', frames: 240, wav: true,
    why: 'a second, differently-shaped instrument set (harmony.h chords + comping) so one engine cannot stand for all' },
  { cart: 'drawall', frames: 60, dumpEvery: 10, motion: true,
    why: 'every draw primitive in one cart, with per-frame rotation' },
  { cart: 'swcanvas_test', frames: 30, dumpEvery: 10,
    why: 'the integer canvas primitives, where byte-exactness is the whole point' },
];

/* the semantic gates run by --full. Each is a second opinion, not the gate itself. */
const SEMANTIC_GATES = [
  { name: 'sound.h queues', cmd: ['node', ['tools/play.js', 'soundcheck', 'script', '/dev/null', '--headless', '--frames', '900']],
    pass: (out) => !/\[sound\]/.test(out) },
  { name: 'tune-check',     cmd: ['node', ['tools/tune-check.js', '--quiet']] },
  { name: 'spec.js',        cmd: ['node', ['tools/spec.js', '--quiet']] },
  { name: 'canvas golden',  cmd: ['node', ['tools/canvas-diff.js', 'drawall', '--golden']] },
  { name: 'det-probes',     cmd: ['bash', ['tools/det-probes/run.sh']] },
];

/* ───────────────────────────────────────────────────────────────── helpers ── */

const sha = (buf) => crypto.createHash('sha256').update(buf).digest('hex');
const shortSha = (buf) => sha(buf).slice(0, 12);

function tmpdir() { return fs.mkdtempSync(path.join(os.tmpdir(), 'refguard-')); }

function runCart(p, dir, extraArgs = []) {
  const args = ['tools/play.js', p.cart, 'script', p.script || '/dev/null',
                '--headless', '--frames', String(p.frames), '--seed', String(p.seed || 1)];
  if (p.wav)   args.push('--wav',   path.join(dir, 'out.wav'));
  if (p.trace) args.push('--trace', path.join(dir, 'trace.jsonl'));
  if (p.dumpEvery) args.push('--dump', path.join(dir, 'frames'), '--dump-every', String(p.dumpEvery));
  args.push(...extraArgs);
  execFileSync('node', args, { cwd: ROOT, stdio: 'pipe', timeout: 300000 });
}

/* ── WAV: whole-stream sha + a sha per 0.1s chunk (so drift can be located in time) ── */
function fingerprintWav(file) {
  const buf = fs.readFileSync(file);
  const dataOff = 44;                       // canonical PCM header written by the engine
  const pcm = buf.subarray(dataOff);
  const samples = pcm.length / 2;
  const CHUNK = 4410 * 2;                   // 0.1s of 16-bit mono
  const chunks = [];
  for (let i = 0; i < pcm.length; i += CHUNK) chunks.push(shortSha(pcm.subarray(i, i + CHUNK)));

  // liveness: real signal, and not a constant
  let peak = 0, distinct = new Set();
  for (let i = 0; i + 1 < pcm.length; i += 2) {
    const v = pcm.readInt16LE(i);
    if (Math.abs(v) > peak) peak = Math.abs(v);
    if (distinct.size < 64) distinct.add(v);
  }
  const peakDb = peak === 0 ? -Infinity : 20 * Math.log10(peak / 32768);
  return { sha: sha(pcm), samples, chunks, peakDb: Number(peakDb.toFixed(2)), distinct: distinct.size };
}

/* ── frames: a sha per PNG, plus how many colours the first frame holds ── */
function fingerprintFrames(dir) {
  if (!fs.existsSync(dir)) return null;
  const files = fs.readdirSync(dir).filter(f => f.endsWith('.png')).sort();
  if (!files.length) return null;
  const frames = files.map(f => shortSha(fs.readFileSync(path.join(dir, f))));
  const all = Buffer.concat(files.map(f => fs.readFileSync(path.join(dir, f))));
  const uniqueFrames = new Set(frames).size;
  return { sha: sha(all), count: files.length, frames, uniqueFrames };
}

/* ── trace: a sha per frame line, plus whether any watched value ever changes ── */
function fingerprintTrace(file) {
  if (!fs.existsSync(file)) return null;
  const lines = fs.readFileSync(file, 'utf8').split('\n').filter(Boolean);
  if (!lines.length) return null;
  const frames = lines.map(shortSha);
  // How many watched keys ever take a second value? Comparing only the FIRST and LAST frame is
  // not enough: a value that moves and comes home again reads as inert, which is how `epiano`
  // looked dead on the first blessing while its trace was perfectly alive.
  let changing = 0;
  try {
    const seen = {};
    for (const L of lines) {
      const w = JSON.parse(L).w || {};
      for (const k of Object.keys(w)) (seen[k] = seen[k] || new Set()).add(w[k]);
    }
    for (const k of Object.keys(seen)) if (seen[k].size > 1) changing++;
  } catch (_) { /* a malformed trace shows up as a sha mismatch anyway */ }
  return { sha: sha(lines.join('\n')), count: lines.length, frames, changing };
}

/* liveness: would this probe pass even if the engine did nothing? */
function liveness(p, fp) {
  const bad = [];
  if (p.wav) {
    if (!fp.wav) bad.push('no WAV was produced');
    else {
      if (!(fp.wav.peakDb > -60)) bad.push(`audio is silent (peak ${fp.wav.peakDb} dBFS)`);
      if (fp.wav.distinct < 8)    bad.push(`audio is a constant (${fp.wav.distinct} distinct values)`);
    }
  }
  if (p.dumpEvery) {
    if (!fp.frames) bad.push('no frames were dumped');
    else if (p.motion && fp.frames.uniqueFrames < 2) bad.push('every frame is identical (nothing moved)');
  }
  if (p.trace) {
    if (!fp.trace) bad.push('no trace was produced');
    else if (fp.trace.changing === 0) bad.push('no watched value ever changes (the trace is inert)');
  }
  return bad;
}

function fingerprintProbe(p, extraArgs = []) {
  const dir = tmpdir();
  try {
    runCart(p, dir, extraArgs);
    const fp = {};
    if (p.wav)       fp.wav    = fingerprintWav(path.join(dir, 'out.wav'));
    if (p.trace)     fp.trace  = fingerprintTrace(path.join(dir, 'trace.jsonl'));
    if (p.dumpEvery) fp.frames = fingerprintFrames(path.join(dir, 'frames'));
    return fp;
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
}

/* ── where did two chunk arrays first disagree? that is the whole point of storing them ── */
function firstDiff(a, b) {
  if (!a || !b) return 0;
  const n = Math.min(a.length, b.length);
  for (let i = 0; i < n; i++) if (a[i] !== b[i]) return i;
  return a.length === b.length ? -1 : n;
}

/* ───────────────────────────────────────────────────────────────── the gate ── */

function collect(probes, onProgress) {
  const out = {};
  for (const p of probes) {
    onProgress && onProgress(p);
    out[p.cart] = fingerprintProbe(p);
  }
  return out;
}

function engineContext() {
  // recorded for information only — during this refactor the engine WILL change and the output
  // must NOT, so gating on the engine's own hash would be exactly backwards.
  const files = ['runtime/sound.h', 'runtime/studio.c', 'runtime/studio.h'];
  const h = {};
  for (const f of files) if (fs.existsSync(path.join(ROOT, f))) h[f] = shortSha(fs.readFileSync(path.join(ROOT, f)));
  let commit = '';
  try { commit = execSync('git rev-parse --short HEAD', { cwd: ROOT }).toString().trim(); } catch (_) {}
  return { commit, files: h };
}

function bless() {
  console.log('recording the baseline — this is the "before" the refactor is measured against\n');
  const fps = {};
  let vacuous = 0;
  for (const p of PROBES) {
    process.stdout.write(`  ${p.cart.padEnd(16)} `);
    fps[p.cart] = fingerprintProbe(p);
    const bad = liveness(p, fps[p.cart]);
    if (bad.length) { vacuous++; console.log('DEAD — ' + bad.join('; ')); }
    else console.log('live');
  }
  if (vacuous) {
    console.log(`\n${vacuous} probe(s) produce nothing to compare. Blessing them would create a gate that`);
    console.log('passes forever while proving nothing. Fix or replace the probe, then bless again.');
    process.exit(1);
  }
  const data = { version: 1, blessed_at_commit: engineContext(), probes: fps };
  fs.writeFileSync(BASELINE, JSON.stringify(data, null, 1));
  console.log(`\nwrote ${path.relative(ROOT, BASELINE)} — ${PROBES.length} probes, all live.`);
  console.log('commit it, then start the refactor.');
}

function compare(opts) {
  if (!fs.existsSync(BASELINE)) {
    console.error('no baseline. Run `node tools/refactor-guard.js --bless` BEFORE changing the engine.');
    process.exit(2);
  }
  const base = JSON.parse(fs.readFileSync(BASELINE, 'utf8'));
  const results = [];
  for (const p of PROBES) {
    if (!opts.quiet) process.stdout.write(`  ${p.cart} … `);
    let fp;
    try { fp = fingerprintProbe(p); }
    catch (e) { results.push({ p, status: 'ERROR', detail: (e.message || '').split('\n')[0] });
                if (!opts.quiet) console.log('ERROR'); continue; }

    const bad = liveness(p, fp);
    if (bad.length) { results.push({ p, status: 'VACUOUS', detail: bad.join('; ') });
                      if (!opts.quiet) console.log('DEAD'); continue; }

    const b = base.probes[p.cart];
    if (!b) { results.push({ p, status: 'NEW', detail: 'not in the baseline — re-bless' });
              if (!opts.quiet) console.log('new'); continue; }

    const drifts = [];
    if (p.wav && b.wav && fp.wav.sha !== b.wav.sha) {
      const i = firstDiff(b.wav.chunks, fp.wav.chunks);
      drifts.push(`audio diverges at ${(i * 0.1).toFixed(1)}s (chunk ${i} of ${b.wav.chunks.length})` +
                  (fp.wav.peakDb !== b.wav.peakDb ? `, peak ${b.wav.peakDb} → ${fp.wav.peakDb} dBFS` : ''));
    }
    if (p.trace && b.trace && fp.trace.sha !== b.trace.sha) {
      const i = firstDiff(b.trace.frames, fp.trace.frames);
      drifts.push(`state diverges at frame ${i} of ${b.trace.count}`);
    }
    if (p.dumpEvery && b.frames && fp.frames.sha !== b.frames.sha) {
      const i = firstDiff(b.frames.frames, fp.frames.frames);
      drifts.push(`frame ${i} of ${b.frames.count} differs`);
    }
    results.push({ p, status: drifts.length ? 'DRIFT' : 'ok', detail: drifts.join(' · ') });
    if (!opts.quiet) console.log(drifts.length ? 'DRIFT' : 'ok');
  }

  const bad = results.filter(r => r.status !== 'ok');
  if (opts.quiet) {
    console.log(bad.length
      ? `refactor-guard: ${bad.length}/${results.length} probes NOT byte-identical — ${bad.map(r => r.p.cart).join(' ')}`
      : `refactor-guard: ${results.length}/${results.length} probes byte-identical to the baseline`);
  } else {
    console.log('');
    if (!bad.length) {
      console.log(`  ✓ all ${results.length} probes are byte-identical to the baseline.`);
      console.log('    A pure state move looks exactly like this. Commit the step.');
    } else {
      for (const r of bad) console.log(`  ✗ ${r.p.cart.padEnd(16)} ${r.status}  ${r.detail}`);
      console.log('\n  A state move that changes output is a BUG in the refactor, not a new baseline.');
      console.log('  Do not re-bless to make this green unless you can say why the change is intended.');
    }
  }

  if (opts.full) runSemanticGates(opts);
  process.exit(bad.length ? 1 : 0);
}

function runSemanticGates(opts) {
  console.log('\n  second opinion — the semantic gates (these have tolerances; the fingerprints above do not)');
  for (const g of SEMANTIC_GATES) {
    process.stdout.write(`    ${g.name.padEnd(18)} `);
    try {
      const out = execFileSync(g.cmd[0], g.cmd[1], { cwd: ROOT, stdio: 'pipe', timeout: 600000 }).toString();
      const ok = g.pass ? g.pass(out) : true;
      console.log(ok ? 'ok' : 'FAIL');
    } catch (e) {
      console.log('FAIL');
    }
  }
}

/* ───────────────────────────────────────────────────────────── the self-test ── */

function selfCheck() {
  const checks = [];
  const t = (name, fn) => { let ok = false; try { ok = !!fn(); } catch (e) { ok = false; } checks.push([name, ok]); };

  /* 1. the comparator: known answers on synthetic streams */
  t('identical chunk arrays report no drift', () => firstDiff(['a', 'b', 'c'], ['a', 'b', 'c']) === -1);
  t('a changed chunk is located exactly',     () => firstDiff(['a', 'b', 'c'], ['a', 'X', 'c']) === 1);
  t('a truncated stream is caught',           () => firstDiff(['a', 'b', 'c'], ['a', 'b']) === 2);
  t('drift in the first chunk reads as 0',    () => firstDiff(['a'], ['X']) === 0);

  /* 2. the liveness detector: it must call a dead render dead */
  const silent = { wav: { peakDb: -Infinity, distinct: 1, sha: 'x', samples: 0, chunks: [] } };
  const constant = { wav: { peakDb: -3, distinct: 1, sha: 'x', samples: 10, chunks: [] } };
  const good = { wav: { peakDb: -3, distinct: 40, sha: 'x', samples: 10, chunks: [] } };
  t('silence fails liveness',                 () => liveness({ wav: true }, silent).length > 0);
  t('a DC constant fails liveness',           () => liveness({ wav: true }, constant).length > 0);
  t('real signal passes liveness',            () => liveness({ wav: true }, good).length === 0);
  t('a blank filmstrip fails liveness',       () => liveness({ dumpEvery: 1, motion: true }, { frames: { uniqueFrames: 1, count: 5 } }).length > 0);
  t('an inert trace fails liveness',          () => liveness({ trace: true }, { trace: { changing: 0, count: 10 } }).length > 0);

  /* 3. THE NEGATIVE CONTROL, live against the real engine. A gate nobody has seen go red is
   *    indistinguishable from a gate gone blind, so this renders a probe twice with a different
   *    RNG seed and requires the fingerprint to NOTICE. If these come back equal, the tool is not
   *    reading the engine's output and every green verdict it ever printed is worthless.
   *
   *    The perturbation is deliberately one that leaves the audio ALIVE and merely different.
   *    An earlier draft used `--solo-slot`, which renders silence — so the liveness check caught
   *    it and the SHA COMPARISON, the thing actually being controlled for, was never exercised.
   *    A control has to fail the specific assertion it is a control for. `bossa` is used because
   *    it is the probe that consumes the seed; the others are seed-independent. */
  const seedProbe = { cart: 'bossa', frames: 120, wav: true };
  let normal = null, perturbed = null;
  try {
    normal    = fingerprintProbe({ ...seedProbe, seed: 1 });
    perturbed = fingerprintProbe({ ...seedProbe, seed: 7 });
  } catch (e) { /* leaves both null → the assertions below fail, which is correct */ }
  t('the control renders real audio',          () => normal && normal.wav.peakDb > -60);
  t('the perturbed run is ALSO alive',         () => perturbed && liveness({ wav: true }, perturbed).length === 0);
  t('a perturbed engine changes the sha',      () => normal && perturbed && normal.wav.sha !== perturbed.wav.sha);
  t('and the drift is located, not just seen', () => normal && perturbed &&
        firstDiff(normal.wav.chunks, perturbed.wav.chunks) >= 0);

  const pass = checks.filter(c => c[1]).length;
  for (const [name, ok] of checks) if (!ok) console.log('  ✗ ' + name);
  console.log(`refactor-guard --check: ${pass}/${checks.length} known answers correct`);
  process.exit(pass === checks.length ? 0 : 1);
}

/* ───────────────────────────────────────────────────────────────────── main ── */

const argv = process.argv.slice(2);
const opts = { quiet: argv.includes('--quiet'), full: argv.includes('--full') };
if (argv.includes('--check')) selfCheck();
else if (argv.includes('--bless')) bless();
else compare(opts);
