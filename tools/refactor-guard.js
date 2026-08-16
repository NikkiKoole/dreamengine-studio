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
  // ⚠ ISOLATE THE SAVE DIR, AND WIPE IT. Without this a probe inherits build/saves/<cart>/, which is
  // untracked, mutable, and REWRITTEN BY EVERY RUN — so the baseline silently encodes whatever rack
  // state the cart happened to have persisted, and any later run from a different history "drifts".
  //
  // That is not hypothetical: `acidcandy` persists its whole rack to a 437 KB cart.blob, and this
  // gate reported `audio diverges at 0.0s … state diverges at frame 0` — with the SAME numbers on an
  // unmodified tree — purely because the blob had been deleted between bless and compare. A gate
  // whose headline is "a state move that changes output is a BUG in the refactor" must not be able
  // to say that about a file nobody edited. Same class as the trap it caught: the run has to start
  // from a KNOWN state, and "whatever was on disk" is not one.
  const saveRel = `saves/.refguard/${p.cart}`;
  fs.rmSync(path.join(ROOT, 'build', saveRel), { recursive: true, force: true });
  const args = ['tools/play.js', p.cart, 'script', p.script || '/dev/null',
                '--headless', '--frames', String(p.frames), '--seed', String(p.seed || 1),
                '--save-dir', saveRel];
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

/* Everything whose content can change a probe's bytes. Recorded so a baseline cannot lie about its
 * own provenance — see the note on blessProvenance below for the failure that made this necessary. */
function provenanceFiles() {
  const files = ['runtime/sound.h', 'runtime/studio.c', 'runtime/studio.h'];
  for (const p of PROBES) {
    files.push(`tools/carts/${p.cart}.c`);
    if (p.script) files.push(p.script);
  }
  return files.filter(f => fs.existsSync(path.join(ROOT, f)));
}

/* `git status --porcelain` → the paths, nothing else.
 *
 * ⚠ DO NOT TRIM THE WHOLE OUTPUT BEFORE SPLITTING. Porcelain lines begin with a two-char status
 * field which is very often " M", so trimming the output eats the FIRST line's leading space, and a
 * fixed slice(3) then shaves a character off the first path only — "ools/carts/drawall.c". That is
 * a real bug this tool shipped for about a minute, caught by its own dirty-refusal control. Strip
 * the status field per line, and keep it a pure function so the fixture can pin it. */
function parsePorcelain(out) {
  return out.split('\n').filter(l => l.trim()).map(l => l.replace(/^.{2}\s*/, '').trim());
}

function engineContext() {
  // recorded for information only — during this refactor the engine WILL change and the output
  // must NOT, so gating on the engine's own hash would be exactly backwards.
  //
  // ⚠ IT USED TO RECORD ONLY THE THREE ENGINE FILES, AND THAT MADE A BROKEN BASELINE LOOK SOUND.
  // Every probe is a CART, so a dirty or since-changed cart moves the bytes while all three engine
  // hashes still match — which is exactly what happened: the 2026-08-13 baseline claimed acidcandy's
  // peak was -0.79 dBFS where every commit from the blessing onward renders -3.82, with the engine
  // hashes matching PERFECTLY. Reading them was what made the baseline look trustworthy. So the
  // probe carts and their driving clips are recorded too, plus whether the tree was dirty.
  const h = {};
  for (const f of provenanceFiles()) h[f] = shortSha(fs.readFileSync(path.join(ROOT, f)));
  let commit = '', dirty = [];
  try { commit = execSync('git rev-parse --short HEAD', { cwd: ROOT }).toString().trim(); } catch (_) {}
  try {
    // scoped to the paths that matter: a foreign edit elsewhere in a shared working tree is none of
    // this gate's business, and refusing on it would make the tool unusable with parallel agents.
    const out = execSync(`git status --porcelain -- ${provenanceFiles().map(f => `'${f}'`).join(' ')}`,
                         { cwd: ROOT }).toString();
    dirty = parsePorcelain(out);
  } catch (_) {}
  return { commit, dirty, files: h };
}

/* ── WHY is it red? (pure, so --check can pin it) ────────────────────────────────────────────────
 * A drift is only a bug if the baseline is still a fair comparison. Three ways it stops being one,
 * and the tool already collected the evidence for all three — it just never read the second.
 *
 * ⚠ THE ONE THAT COST A DAY: the tree can move to a DIFFERENT COMMIT between a blessing and its
 * compare. On a branch several agents commit to, that is not exotic, it is Tuesday — somebody else
 * lands a change to `runtime/studio.c` while you are mid-loop, your probe legitimately renders
 * different bytes, and the gate answers with "a state move that changes output is a BUG in the
 * refactor". It is nobody's bug and there is nothing on screen to suggest otherwise. It burned four
 * bless/compare cycles before `git log` explained it, and it was caught in the act on the fifth:
 * HEAD went 5873a465 → dc35b110 *while the bless was running*.
 *
 * ⚠ AND WHY THIS IS A NOTE, NOT AN EXCUSE. The refactor workflow is: bless clean → edit the engine →
 * compare dirty. There, engine files differing IS the measurement and calling it "expected" would
 * invert the gate into uselessness. What is NOT part of that workflow is HEAD itself moving, so the
 * COMMIT is the honest discriminator: same commit + changed engine files = you, measured; different
 * commit = somebody else may be in the frame, so re-bless on a settled tree before believing a red.
 */
function provenanceNotes(base, nowCommit, moved) {
  const bc = (base && base.blessed_at_commit) || {};
  const notes = [];
  const wasDirty = bc.dirty || [];
  // worst first: a baseline blessed dirty records bytes NO commit reproduces, so it can never go green
  if (wasDirty.length) notes.push({ kind: 'dirty', files: wasDirty });
  if (bc.commit && nowCommit && bc.commit !== nowCommit)
    notes.push({ kind: 'moved-commit', from: bc.commit, to: nowCommit,
                 files: moved.filter(f => f.startsWith('runtime/')) });
  const carts = moved.filter(f => f.startsWith('tools/'));
  if (carts.length) notes.push({ kind: 'moved-cart', files: carts });
  return notes;
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
  // ⚠ A BASELINE BLESSED FROM A DIRTY TREE RECORDS A STATE NO COMMIT REPRODUCES, and it fails
  // SILENTLY: every later run is red against bytes nobody can get back, so the safety net for the
  // whole refactor is gone and looks merely inconvenient. That is what happened on 2026-08-13.
  // Refuse instead, scoped to the files that can move a probe's bytes.
  const ctx = engineContext();
  if (ctx.dirty.length && !process.argv.includes('--force')) {
    console.log('\nREFUSING to bless: these files can change a probe\'s output and are uncommitted —');
    for (const f of ctx.dirty) console.log(`  ${f}`);
    console.log('\nCommit them first. A baseline recorded from a dirty tree is a reference no commit');
    console.log('reproduces, so every later run goes red against bytes that cannot be recovered.');
    console.log('`--force` if you genuinely mean to (the dirty list is recorded either way).');
    process.exit(1);
  }
  const data = { version: 1, blessed_at_commit: ctx, probes: fps };
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

  // Which recorded files have MOVED since the blessing. This is not a verdict — during the refactor
  // the engine is meant to change while the output does not, which is the whole point. But when a
  // probe's own CART or driving CLIP has changed, "not byte-identical" is expected rather than
  // alarming, and saying so is the difference between a gate and a puzzle.
  const moved = [];
  if (base.blessed_at_commit && base.blessed_at_commit.files)
    for (const [f, want] of Object.entries(base.blessed_at_commit.files)) {
      const abs = path.join(ROOT, f);
      if (!fs.existsSync(abs)) { moved.push(`${f} (gone)`); continue; }
      if (shortSha(fs.readFileSync(abs)) !== want) moved.push(f);
    }
  const notes = provenanceNotes(base, engineContext().commit, moved);
  const movedCommit = notes.find(n => n.kind === 'moved-commit');

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
      // ⚠ firstDiff returns -1 for "every chunk matches", which formatted as a LOCATION reads
      // `audio diverges at -0.1s (chunk -1 of 100)` — a nonsense coordinate presented with the same
      // confidence as a real one. It means the whole-stream sha moved while no 0.1s chunk did, so
      // say that instead of inventing a timestamp for it.
      const where = i < 0 ? 'audio differs but every 0.1s chunk matches (length or header changed?)'
                          : `audio diverges at ${(i * 0.1).toFixed(1)}s (chunk ${i} of ${b.wav.chunks.length})`;
      drifts.push(where + (fp.wav.peakDb !== b.wav.peakDb ? `, peak ${b.wav.peakDb} → ${fp.wav.peakDb} dBFS` : ''));
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
      ? `refactor-guard: ${bad.length}/${results.length} probes NOT byte-identical — ${bad.map(r => r.p.cart).join(' ')}` +
        // the ONE-LINE form repo-doctor shows: without this hint a moved tree reads as a regression
        (movedCommit ? ` (⚠ tree moved since the blessing: ${movedCommit.from} → ${movedCommit.to} — re-bless on a settled tree)` : '')
      : `refactor-guard: ${results.length}/${results.length} probes byte-identical to the baseline`);
  } else {
    console.log('');
    if (!bad.length) {
      console.log(`  ✓ all ${results.length} probes are byte-identical to the baseline.`);
      console.log('    A pure state move looks exactly like this. Commit the step.');
    } else {
      for (const r of bad) console.log(`  ✗ ${r.p.cart.padEnd(16)} ${r.status}  ${r.detail}`);
      // Say WHY before saying "this is a bug". A drift whose probe cart has changed since the
      // blessing is expected; a baseline recorded from a dirty tree is not comparable at all.
      for (const n of notes) {
        if (n.kind === 'dirty') {
          console.log('\n  ⚠ THIS BASELINE WAS BLESSED FROM A DIRTY TREE — these were uncommitted at the time:');
          for (const f of n.files) console.log(`      ${f}`);
          console.log('    It therefore records bytes no commit reproduces, and cannot go green. Re-bless.');
        } else if (n.kind === 'moved-commit') {
          console.log(`\n  ⚠ THE TREE MOVED SINCE THE BLESSING — baseline at ${n.from}, now at ${n.to}.`);
          if (n.files.length) {
            console.log('    Recorded ENGINE inputs that differ:');
            for (const f of n.files) console.log(`      ${f}`);
          }
          console.log('    A drift may belong to SOMEBODY ELSE\'S COMMIT rather than to your change —');
          console.log('    several agents commit to this branch. Re-bless on a settled tree and compare');
          console.log('    again before believing this red. (Same commit + changed engine files is the');
          console.log('    normal refactor loop and IS the measurement; a moved commit is not.)');
        } else if (n.kind === 'moved-cart') {
          console.log('\n  ⚠ a probe\'s own cart/clip has CHANGED since the blessing, so drift is expected here:');
          for (const f of n.files) console.log(`      ${f}`);
          console.log('    Re-bless if the cart change is the intended one.');
        }
      }
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

  /* 4. PROVENANCE. Added after this gate spent a day RED for a reason that was not a regression:
   *    it recorded content hashes of three ENGINE files while every probe is a CART, so a baseline
   *    blessed from a tree whose cart differed looked perfectly sound — the engine hashes matched to
   *    the character. These assertions exist because the hardening itself was unverified, which is
   *    the same shape as the bug: `gate-controls` counts a `--selfcheck` as a control without being
   *    able to tell whether it covers the gate's CURRENT code. */
  const prov = provenanceFiles();
  t('provenance records the engine files',      () => prov.includes('runtime/sound.h') &&
                                                     prov.includes('runtime/studio.c'));
  t('provenance records every probe CART',      () => PROBES.every(p => prov.includes(`tools/carts/${p.cart}.c`)));
  t('and the clips that DRIVE those probes',    () => PROBES.filter(p => p.script)
                                                     .every(p => prov.includes(p.script)));
  t('it is more than the three engine files',   () => prov.length > 3);

  /* 4b. WHY-IS-IT-RED. The reasoning that turns a confusing red into an actionable one, pinned in
   *     BOTH directions — the failure mode here is not a wrong message, it is going quietly silent
   *     and leaving the operator with "a state move that changes output is a BUG in the refactor"
   *     on a day when somebody else's commit moved the tree under them. That cost four bless/compare
   *     cycles before git log explained it. Pure function, so these are known answers, not a run. */
  const bcFixture = (over) => ({ blessed_at_commit: { commit: 'aaaaaaa', dirty: [], files: {} }, ...over });
  t('a settled tree at the same commit says NOTHING',
        () => provenanceNotes(bcFixture(), 'aaaaaaa', []).length === 0);
  t('a MOVED COMMIT is reported',
        () => { const n = provenanceNotes(bcFixture(), 'bbbbbbb', []);
                return n.length === 1 && n[0].kind === 'moved-commit' && n[0].from === 'aaaaaaa' && n[0].to === 'bbbbbbb'; });
  t('…and it names the ENGINE files that moved with it',
        () => { const n = provenanceNotes(bcFixture(), 'bbbbbbb', ['runtime/studio.c']);
                return n[0].files.length === 1 && n[0].files[0] === 'runtime/studio.c'; });
  t('an engine file moving at the SAME commit is the refactor loop, not a note',
        () => provenanceNotes(bcFixture(), 'aaaaaaa', ['runtime/studio.c']).length === 0);
  t('a moved CART is still reported on its own',
        () => { const n = provenanceNotes(bcFixture(), 'aaaaaaa', ['tools/carts/acidcandy.c']);
                return n.length === 1 && n[0].kind === 'moved-cart'; });
  t('a DIRTY blessing outranks everything (it can never go green)',
        () => provenanceNotes(bcFixture({ blessed_at_commit: { commit: 'aaaaaaa', dirty: ['runtime/sound.h'], files: {} } }),
                              'bbbbbbb', ['runtime/studio.c'])[0].kind === 'dirty');
  t('a baseline with no provenance at all does not crash',
        () => provenanceNotes({}, 'aaaaaaa', ['runtime/studio.c']).length === 0);
  t('an UNKNOWN current commit is not reported as a move',
        () => provenanceNotes(bcFixture(), '', ['runtime/studio.c']).length === 0);

  /* the porcelain parse, pinned in the exact shape that broke: a worktree-only modification is
   * " M path", and the bug ate the leading space and then a character of the path. */
  t('porcelain " M path" yields the full path', () => parsePorcelain(' M tools/carts/drawall.c')[0] === 'tools/carts/drawall.c');
  t('staged "M  path" too',                     () => parsePorcelain('M  runtime/sound.h')[0] === 'runtime/sound.h');
  t('and "??" untracked',                       () => parsePorcelain('?? tools/carts/new.c')[0] === 'tools/carts/new.c');
  t('several lines, all paths intact',          () => {
        const r = parsePorcelain(' M a/one.c\nMM b/two.c\n?? c/three.c\n');
        return r.length === 3 && r[0] === 'a/one.c' && r[1] === 'b/two.c' && r[2] === 'c/three.c';
      });
  t('firstDiff says -1 when the chunk arrays are identical',
        () => firstDiff(['a','b'], ['a','b']) === -1);
  t('…and a real index when they are not',
        () => firstDiff(['a','b','c'], ['a','x','c']) === 1);
  t('a clean tree parses to nothing',           () => parsePorcelain('').length === 0 &&
                                                      parsePorcelain('\n').length === 0);

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
