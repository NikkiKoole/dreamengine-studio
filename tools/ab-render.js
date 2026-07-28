#!/usr/bin/env node
// ab-render.js — A/B a cart against ITSELF by flipping one compile-time value, render each variant,
// and print a comparison table. The LISTEN-item workhorse for docs/design/synth-secrets-plan.md.
//
//   node tools/ab-render.js <cart> --set <ident>=<v1,v2,...> [options]
//
//   --set <ident>=<vals>   REQUIRED. `ident` is a cart-source identifier assigned at file scope —
//                          either `static <type> ident = X;` or `#define ident X`. Each comma-separated
//                          value is substituted in turn. Values may be C expressions (0.5f, WOW_RANDOM).
//   --file <path>          patch this file instead of tools/carts/<cart>.c — for a value that lives in
//                          runtime/*.h (an engine or a cart-land header). The cart is still what renders.
//   --frames <n>           frames to render per variant (default 420 ≈ 7s @60fps)
//   --script <file>        input script to drive the cart (default /dev/null = whatever it does on boot)
//   --f0 <hz>              also print harmonic extent + >4kHz energy at this fundamental
//   --keep                 keep the rendered WAVs and print their paths
//   --seed <n>             pass through to play.js for determinism
//
// WHY THIS EXISTS: the manual version is "sed the flag, render, sed it back", which is three commands
// and a footgun — a regex that matches the *initial* form of a line stops matching after the first
// substitution, so every later variant silently renders the FIRST state and the WAVs come out
// byte-identical. That happened, and byte-identical output nearly got written up as a finding. So this
// tool does two things hand-rolling doesn't: it **restores the source in a finally block** (even on
// crash or Ctrl-C), and it **shouts if two variants render byte-identical audio**, because that means
// the flag never reached the DSP and any conclusion drawn from the numbers would be wrong.
//
// Numbers come from tools/wav-envelope.js and tools/harmonic-spec.js so there is ONE source of truth
// for peak/brightness/centroid; this tool only adds the sha (the did-it-actually-change check) and the
// table. It measures, it does not judge — a LISTEN item still needs the owner's ear (plan §1).
//
// Example:
//   node tools/ab-render.js solina --set wow=WOW_CLASSIC,WOW_RANDOM,WOW_BREATHE --frames 1800
//   node tools/ab-render.js brass  --set A_REL=1200,200 --f0 220

const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');
const crypto = require('crypto');

const ROOT = path.resolve(__dirname, '..');
const argv = process.argv.slice(2);

// `process.exit()` does NOT run finally blocks, so a die() from inside the render loop would leave the
// patched source on disk — which then becomes the next run's "original" and gets restored *to the
// corrupted value*. It happened (`#define TR808_CYM3 04010` survived a failed run and was faithfully
// put back). Every exit path now goes through the same restore.
let restoreHook = null;
function die(msg) { if (restoreHook) restoreHook(); console.error('ab-render: ' + msg); process.exit(1); }
function flag(name, dflt) {
  const i = argv.indexOf('--' + name);
  return i >= 0 && i + 1 < argv.length ? argv[i + 1] : dflt;
}
const has = (name) => argv.includes('--' + name);

const cart = argv.find((a) => !a.startsWith('--') && !argv[argv.indexOf(a) - 1]?.startsWith('--'));
if (!cart) die('need a cart name.  usage: node tools/ab-render.js <cart> --set ident=v1,v2');

const setArg = flag('set');
if (!setArg || !setArg.includes('=')) die('--set <ident>=<v1,v2,...> is required');
const ident = setArg.slice(0, setArg.indexOf('=')).trim();
const values = setArg.slice(setArg.indexOf('=') + 1).split(',').map((s) => s.trim()).filter(Boolean);
if (values.length < 2) die('--set needs at least two comma-separated values to compare');

const frames = flag('frames', '420');
const script = flag('script', '/dev/null');
const f0 = flag('f0', null);
const seed = flag('seed', null);

const fileArg = flag('file', null);
const src = fileArg ? path.resolve(ROOT, fileArg) : path.join(ROOT, 'tools/carts', cart + '.c');
if (!fs.existsSync(src)) die('no such source file: ' + src);

// Locate the assignment. Two accepted forms, both at file scope:
//   static <type> ident = <value>;      →  replace <value>
//   #define ident <value>               →  replace <value>
const original = fs.readFileSync(src, 'utf8');
const declRe = new RegExp(`(^[ \\t]*static[^;\\n]*\\b${ident}\\s*=\\s*)([^;]+)(;)`, 'm');
const defRe = new RegExp(`(^[ \\t]*#define[ \\t]+${ident}[ \\t]+)([^\\n/]+)`, 'm');
const kind = declRe.test(original) ? 'static' : defRe.test(original) ? 'define' : null;
if (!kind) {
  die(`could not find \`${ident}\` as a file-scope \`static ... ${ident} = X;\` or \`#define ${ident} X\` in ${path.relative(ROOT, src)}.\n` +
      `            (a local variable or a struct field can't be substituted — hoist it to file scope first)`);
}
const re = kind === 'static' ? declRe : defRe;
const before = original.match(re)[2].trim();

// Substitute group 2. Note the arity difference: declRe has THREE groups (the `;` is group 3) and
// defRe has TWO, so a shared `(m, a, b, c) => a + v + c` callback silently appends the match OFFSET
// (an integer — String.replace passes offset right after the last group) for every #define. That
// produced `#define X 042` and a build that measured a stale value. Pick the tail by `kind`, never
// by argument position.
const subst = (text, v) => text.replace(re, (...args) => args[1] + v + (kind === 'static' ? args[3] : ''));

const outDir = fs.mkdtempSync(path.join(require('os').tmpdir(), 'abrender-'));
const rows = [];
let restored = false;
function restore() {
  if (restored) return;
  fs.writeFileSync(src, original);
  restored = true;
}
restoreHook = restore;
process.on('SIGINT', () => { restore(); process.exit(130); });
process.on('uncaughtException', (e) => { restore(); console.error(e); process.exit(1); });

try {
  for (let i = 0; i < values.length; i++) {
    const v = values[i];
    fs.writeFileSync(src, subst(original, v));
    // sanity: the substitution must have actually landed
    const now = fs.readFileSync(src, 'utf8').match(re);
    if (!now || now[2].trim() !== v) die(`substitution of \`${ident} = ${v}\` did not take — aborting rather than measure a stale build`);

    const wav = path.join(outDir, `${cart}_${i}.wav`);
    const args = [path.join(ROOT, 'tools/play.js'), cart, 'script', script, '--headless', '--frames', frames, '--wav', wav];
    if (seed) args.push('--seed', seed);
    let ok = true, err = '';
    try {
      execFileSync('node', args, { cwd: ROOT, stdio: ['ignore', 'pipe', 'pipe'] });
    } catch (e) {
      ok = false; err = (e.stderr || e.stdout || '').toString().split('\n').filter(Boolean).slice(-2).join(' ');
    }
    if (!ok || !fs.existsSync(wav)) { rows.push({ v, fail: err || 'no wav produced' }); continue; }

    const sha = crypto.createHash('sha1').update(fs.readFileSync(wav)).digest('hex').slice(0, 12);
    const env = execFileSync('node', [path.join(ROOT, 'tools/wav-envelope.js'), wav], { cwd: ROOT }).toString();
    const m = env.match(/peak\s+(-?[\d.]+)\s+dBFS.*?brightness\(HF\/total\)\s+mean\s+([\d.]+).*?centroid\s+(\d+)\s*Hz/s);
    const row = { v, sha, wav, peak: m ? m[1] : '?', bright: m ? m[2] : '?', cent: m ? m[3] : '?' };
    if (f0) {
      const hs = execFileSync('node', [path.join(ROOT, 'tools/harmonic-spec.js'), wav, f0], { cwd: ROOT }).toString();
      const hm = hs.match(/highest harmonic within 20dB of f1:\s*(h\d+)/);
      const em = hs.match(/energy >4kHz \/ total:\s*([\d.]+)%/);
      row.ext = hm ? hm[1] : '?';
      row.hf = em ? em[1] + '%' : '?';
    }
    rows.push(row);
  }
} finally {
  restore();
}

// ── report ───────────────────────────────────────────────────────────────────
const W = (s, n) => String(s).padEnd(n);
console.log(`\nab-render: ${cart}  ·  ${path.relative(ROOT, src)}   ${ident} = [${values.join(', ')}]   (was \`${before}\`, restored)`);
console.log(`  ${frames} frames/variant · script ${script}${seed ? ' · seed ' + seed : ''}\n`);
const head = `  ${W(ident, 18)}${W('sha', 14)}${W('peak dBFS', 11)}${W('bright', 9)}${W('centroid', 10)}`;
console.log(head + (f0 ? `${W('extent', 8)}>4kHz` : ''));
console.log('  ' + '-'.repeat(head.length + (f0 ? 14 : 0) - 2));
for (const r of rows) {
  if (r.fail) { console.log(`  ${W(r.v, 18)}BUILD/RUN FAILED — ${r.fail}`); continue; }
  console.log(`  ${W(r.v, 18)}${W(r.sha, 14)}${W(r.peak, 11)}${W(r.bright, 9)}${W(r.cent + ' Hz', 10)}` +
              (f0 ? `${W(r.ext, 8)}${r.hf}` : ''));
}

// The check hand-rolling doesn't do: did the flag actually reach the audio?
const good = rows.filter((r) => !r.fail);
const dupes = new Map();
for (const r of good) dupes.set(r.sha, (dupes.get(r.sha) || 0) + 1);
const collided = [...dupes.entries()].filter(([, n]) => n > 1);
if (collided.length) {
  console.log(`\n  ⚠  ${collided.length === 1 && collided[0][1] === good.length ? 'ALL' : 'SOME'} variants rendered BYTE-IDENTICAL audio.`);
  console.log(`     \`${ident}\` did not reach the DSP for those values — do NOT draw conclusions from the`);
  console.log(`     numbers above. Likely causes: the value is read once at init and this render never`);
  console.log(`     re-reads it; the code path is gated behind a mode the render never enters; or the`);
  console.log(`     identifier shadows another of the same name.`);
  process.exitCode = 2;
} else if (good.length > 1) {
  console.log(`\n  ✓ all ${good.length} variants differ (distinct sha) — the flag reaches the audio.`);
  console.log(`    Numbers are a sanity check, not a verdict: a LISTEN item still needs the ear (plan §1).`);
}
if (has('keep')) { console.log('\n  wavs:'); for (const r of good) console.log('    ' + r.wav); }
else { for (const r of good) { try { fs.unlinkSync(r.wav); } catch {} } try { fs.rmdirSync(outDir); } catch {} }
