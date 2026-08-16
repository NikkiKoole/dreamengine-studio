#!/usr/bin/env node
// canvas-diff.js — the GPU-vs-software-canvas render oracle. The twin of mirror-diff.js /
// road-check.js, for the DE_SOFTWARE_CANVAS path. Compiles a cart once, runs it headless in BOTH
// modes, and pixel-diffs the frames — so a canvas change (a ported primitive, pal()/colorkey, a
// fill) is proven against the GPU truth in one command instead of a hand-rolled magick loop.
//
//   node tools/canvas-diff.js <cart> [--frames N] [--bytecheck] [--raw] [--max N] [--heatmap] [--keep]
//   node tools/canvas-diff.js <cart> --golden [--bless] [--frames N]     # SW-vs-committed-golden byte lock
//
//   --frames N    frames to render + compare (default 10)
//   --golden      DON'T compare to the GPU — compare the SOFTWARE render to a committed golden PNG per
//                 frame (tools/canvas-golden/<cart>/). The GPU↔SW diff is a PARITY oracle and can't see
//                 a subtle sampler change (it may even IMPROVE parity — the 63→48 truncation case); the
//                 SW canvas is deterministic across machines, so a golden locks its ABSOLUTE output and
//                 fails on any sampler/rasterizer drift. Pixel-exact (magick AE == 0).
//   --bless       with --golden: (re)write the goldens from the current SW render. Run it, eyeball the
//                 result, `git add tools/canvas-golden/<cart>/ && commit`. Re-bless ONLY on an intended change.
//   --bytecheck   require BYTE-identical (sha256) instead of a pixel count — for the pset/fill
//                 primitives that ARE exact (cls/pset/pset_rgb/rectfill/spans; e.g. swcanvas_test).
//   --raw         compare against the TRUE shipping GPU primitives. Default sets DE_CPU_RASTER=on on
//                 the reference so the GL-vs-CPU rasterization diffs (line, rotated fill) don't pollute
//                 the diff (use --raw to MEASURE that divergence on purpose).
//   --max N       per-frame pixel-diff budget; exit nonzero if any frame exceeds it. Default: the
//                 cart's own `// canvas-diff: max N` directive if it declares one, else 0. (Declare
//                 a nonzero budget in a cart ONLY for a primitive that genuinely can't be byte-exact
//                 across GPUs — e.g. a fractional-scale sspr's texel-boundary ties; drawall is the case.)
//   --seed N      RNG seed for BOTH runs (default 1) — rnd()-driven carts diverge without a fixed seed.
//   --heatmap     write a difference PNG for the worst frame (needs ImageMagick).
//   --keep        keep the dumped frames (build/.canvas-diff/<cart>/) for inspection.
//
// THE THREE GOTCHAS IT GUARDS (learned the hard way; see docs/design/software-canvas.md):
//   1. sw_force_gpu — a cart calling spr_rot/sspr_ex(deg)/rectfill_rot/print_rot/camera_ex(angle)
//      trips the sticky GPU fallback, so the =on build silently runs GPU after frame 0 and the A/B
//      is GPU-vs-GPU (proves nothing). We grep the source and WARN loudly.
//   2. GL-vs-CPU rasterization (line, rotated fill) — neutralized by DE_CPU_RASTER=on on the
//      reference (unless --raw), so a line/rotated-fill cart A/Bs byte-exact instead of with noise.
//   3. wrong oracle — pixel-diff (magick AE) by default, not shasum; --bytecheck only where exact.
//   4. NOTHING COMPARED — the one the other three cannot see, because they are all about comparing
//      the WRONG thing and this is about comparing nothing at all. The verdict is "no frame exceeded
//      the budget", which is trivially true of no frames: with both runs producing nothing this
//      printed a green "PASS — canvas matches GPU within budget" under an empty table and exited 0.
//      Partial coverage was silent too (3 compared of 10 requested read like 10 of 10), and an
//      UNREADABLE frame passed, because ae() returns NaN and `NaN > maxPx` is false. controlCheck()
//      + withinBudget() close all three; --golden mode already got the NaN case right.
//
// Exit: 0 = within budget (or byte-identical), 1 = exceeded / mismatch, 2 = setup error.
// Routing among all the gates: docs/guides/checks-and-oracles.md.

const { execSync, execFileSync } = require('child_process');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const args = process.argv.slice(2);
function flag(name)      { const i = args.indexOf(name); if (i >= 0) { args.splice(i, 1); return true; } return false; }
function opt(name, def)  { const i = args.indexOf(name); if (i >= 0) { const v = args[i+1]; args.splice(i, 2); return v; } return def; }

const bytecheck = flag('--bytecheck');
const raw       = flag('--raw');
const heatmap   = flag('--heatmap');
const keep      = flag('--keep');
const golden    = flag('--golden');       // lock the SOFTWARE render's ABSOLUTE output vs a committed golden
const bless      = flag('--bless');        // (with --golden) (re)write the goldens from the current SW render
const frames    = parseInt(opt('--frames', '10'), 10);
const maxArg    = opt('--max', null);     // null = not passed → fall back to the cart's declared budget (below)
const seed      = opt('--seed', '1');     // fixed RNG seed for BOTH runs, else rnd()-driven carts diverge
const cart      = args[0];

// ── --selfcheck: KNOWN ANSWERS FOR THE COMPARISON ────────────
// Compiles nothing and renders nothing. What it pins is this file's own reasoning: which runs are
// evidence, how a frame's diff becomes a verdict, and the two source-reading rules (the declared
// budget and the sw_force_gpu grep) that decide how the A/B is set up in the first place.
function selfcheck() {
  let pass = 0, fail = 0;
  const ok = (name, cond, got) => {
    if (cond) { pass++; console.log(`  ✓ ${name}`); } else { fail++; console.log(`  ✗ ${name}   got: ${got}`); }
  };
  // the per-frame verdict, exactly as the loop applies it
  const within = withinBudget, fails = (d, max) => !withinBudget(d, max);

  console.log('canvas-diff --selfcheck — known answers for the comparison (nothing is rendered)\n');

  console.log('THE CONTROL — comparing nothing is not a match');
  ok('0 frames compared is refused', controlCheck(0, 10, [0,1,2,3,4,5,6,7,8,9]).some(c => c.includes('NO frames')),
     controlCheck(0, 10, []));
  ok('  …and it says how many were asked for', controlCheck(0, 10, []).some(c => c.includes('10 requested')),
     controlCheck(0, 10, []));
  ok('3 of 10 compared is refused — partial coverage used to read like full coverage',
     controlCheck(3, 10, [3,4,5,6,7,8,9]).some(c => c.includes('only 3 of 10')), controlCheck(3, 10, [3,4,5,6,7,8,9]));
  ok('  …and it names the frames that never rendered',
     controlCheck(3, 10, [3,4,5]).some(c => c.includes('3, 4, 5')), controlCheck(3, 10, [3,4,5]));
  ok('a full run raises nothing', controlCheck(10, 10, []).length === 0, controlCheck(10, 10, []));
  ok('  …and a single-frame run is fine when that is what was asked for',
     controlCheck(1, 1, []).length === 0, controlCheck(1, 1, []));

  console.log('\nTHE PER-FRAME VERDICT, AND THE UNREADABLE-FRAME HOLE');
  ok('a diff inside the budget passes', within(30, 64), '30 vs 64');
  ok('a diff over the budget fails', fails(65, 64), '65 vs 64');
  ok('  …and exactly at the budget passes (the budget is inclusive)', within(64, 64) && !fails(64, 64), '64 vs 64');
  ok('a byte-identical frame passes a zero budget', within(0, 0), '0 vs 0');
  // measured: ae() returns NaN when magick cannot read a frame ("insufficient image data"), and
  // every comparison with NaN is false — so `d > maxPx` said "within budget" for an unreadable frame
  ok('an UNREADABLE frame (NaN) FAILS', fails(NaN, 64), 'passed');
  ok('  …which the old `d > max` test did NOT — this is the regression guard',
     (NaN > 64) === false && fails(NaN, 64) === true, 'inconsistent');
  ok('  …and it fails against a ZERO budget too, where "unreadable" is likeliest to hide',
     fails(NaN, 0), 'passed');

  console.log('\nTHE DECLARED BUDGET (read out of the cart source)');
  const withDecl = 'void draw(void){}\n// canvas-diff: max 64\n';
  ok('a cart\'s declared budget is used when --max is absent', resolveBudget(null, withDecl).maxPx === 64,
     resolveBudget(null, withDecl).maxPx);
  ok('  …and reported as declared', resolveBudget(null, withDecl).declared === 64,
     resolveBudget(null, withDecl).declared);
  ok('an explicit --max always wins over the declaration', resolveBudget('8', withDecl).maxPx === 8,
     resolveBudget('8', withDecl).maxPx);
  ok('  …including --max 0, which must not fall back to 64', resolveBudget('0', withDecl).maxPx === 0,
     resolveBudget('0', withDecl).maxPx);
  ok('no declaration and no --max means a ZERO budget (byte-exact by default)',
     resolveBudget(null, 'void draw(void){}').maxPx === 0, resolveBudget(null, 'void draw(void){}').maxPx);
  ok('  …and reports that nothing was declared', resolveBudget(null, 'void draw(void){}').declared === null,
     resolveBudget(null, 'void draw(void){}').declared);

  console.log('\nGOTCHA #1: THE sw_force_gpu GREP, AND ITS EXEMPT CLASSES');
  ok('a real camera_ex call is detected', detectForceGpu('void draw(void){ camera_ex(1,2,0.5f); }').length === 1,
     detectForceGpu('void draw(void){ camera_ex(1,2,0.5f); }'));
  ok('a // comment mentioning it does NOT trip the warning',
     detectForceGpu('// we deliberately avoid camera_ex(...) here\nvoid draw(void){}').length === 0, 'tripped');
  ok('  …nor does a /* block */ comment',
     detectForceGpu('/* camera_ex(0,0,1) would break the A/B */\nvoid draw(void){}').length === 0, 'tripped');
  ok('a name that merely CONTAINS it is not a call (word boundary)',
     detectForceGpu('void draw(void){ my_camera_extra(1); }').length === 0, 'tripped');
  ok('a clean cart is silent', detectForceGpu('void draw(void){ cls(0); spr(1,2,3); }').length === 0, 'tripped');

  console.log(`\n${fail ? '✗' : '✓'} ${pass} passed, ${fail} failed`);
  return fail ? 1 : 0;
}

if (flag('--selfcheck')) process.exit(selfcheck());

if (!cart) { console.error('usage: node tools/canvas-diff.js <cart> [--frames N] [--bytecheck] [--raw] [--max N] [--heatmap] [--keep]\n       node tools/canvas-diff.js <cart> --golden [--bless] [--frames N]\n       node tools/canvas-diff.js --selfcheck'); process.exit(2); }

const src = path.join('tools/carts', `${cart}.c`);
if (!fs.existsSync(src)) { console.error(`canvas-diff: no such cart ${src}`); process.exit(2); }

// ── gotcha #1: detect primitives that trip sw_force_gpu, so a passing A/B isn't a lie ──────────
const source = fs.readFileSync(src, 'utf8');

// Per-cart declared budget: a cart may carry `// canvas-diff: max N` when it INTENTIONALLY exercises
// a primitive that can't be byte-portable across GPUs (e.g. a fractional-scale sspr, whose
// nearest-neighbour texel-boundary ties the GPU and CPU floor resolve opposite ways — see
// docs/design/software-canvas.md). An explicit --max always wins; else this default; else 0. This is
// what stops `canvas-diff <cart>` from being a recurring false alarm on a known, accepted divergence.
function resolveBudget(maxArg, src) {
  const declared = src.match(/\/\/\s*canvas-diff:\s*max\s+(\d+)/);
  return { maxPx: maxArg != null ? parseInt(maxArg, 10) : declared ? parseInt(declared[1], 10) : 0,
           declared: declared ? parseInt(declared[1], 10) : null };
}
const budget   = resolveBudget(maxArg, source);
const maxPx    = budget.maxPx;
const declared = budget.declared !== null;
// stdout, NOT stderr: this is an informational banner, not an error. On stderr it broke any caller
// that captures `stdout + stderr` and reads the LAST line as the verdict — repo-doctor did exactly
// that and showed this banner instead of the PASS/FAIL line.
if (maxArg == null && declared) console.log(`canvas-diff: using ${cart}'s declared budget --max ${maxPx} (\`// canvas-diff: max\` in source)\n`);
// All rotated PRIMITIVES (rectfill_rot/spr_rot/sspr_ex/print_rot) now render in software. The only
// thing still tripping the sticky sw_force_gpu fallback is a rotating camera (Tier-2, by design):
// the list lives INSIDE the function: --selfcheck calls this before the module's consts are
// initialised, and a hoisted function closing over a later `const` is a temporal-dead-zone crash
function detectForceGpu(src) {
  const ROT = ['camera_ex'];
  // strip // line comments + /* */ blocks so a comment mentioning camera_ex doesn't false-positive
  const code = src.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');
  return ROT.filter(p => new RegExp(`\\b${p}\\s*\\(`).test(code));
}
const hits = detectForceGpu(source);
if (hits.length) {
  console.error('\x1b[33m⚠ WARNING: this cart calls camera_ex\x1b[0m');
  console.error('  camera_ex(angle≠0) trips sw_force_gpu → the =on build falls back to GPU mid-frame, so');
  console.error('  frames after the first are GPU-vs-GPU and prove NOTHING about the canvas. (zoom-only');
  console.error('  camera_ex stays on the software path.) Use a non-rotating-camera cart for a real A/B.\n');
}

// ── magick presence (only needed for AE / heatmap; bytecheck is pure node) ─────────────────────
let haveMagick = false;
try { execSync('magick -version', { stdio: 'ignore' }); haveMagick = true; } catch {}
if (!bytecheck && !haveMagick) { console.error('canvas-diff: ImageMagick (magick) not found — needed for pixel diff. Use --bytecheck, or `brew install imagemagick`.'); process.exit(2); }

const outRoot = path.join('build', '.canvas-diff', cart);
const refDir  = path.join(outRoot, raw ? 'ref-gpu-raw' : 'ref-gpu');
const testDir = path.join(outRoot, 'test-canvas');
fs.rmSync(outRoot, { recursive: true, force: true });

function run(dir, env, label, nframes = frames) {
  fs.mkdirSync(dir, { recursive: true });
  try {
    execFileSync('node', ['tools/play.js', cart, 'script', '/dev/null', '--headless', '--frames', String(nframes), '--seed', seed, '--dump', dir],
                 { stdio: 'ignore', timeout: 120000, env: { ...process.env, ...env } });
  } catch (e) { console.error(`canvas-diff: ${label} run failed — ${(e.message||'').slice(0,80)}`); process.exit(2); }
}
function framePng(i) { return `frame_${String(i).padStart(5,'0')}.png`; }
function aePix(a, b) {   // pixel diff (magick AE) — immune to PNG-encoding drift, unlike a byte sha
  const out = execSync(`magick compare -metric AE "${a}" "${b}" null: 2>&1 || true`).toString().trim();
  const m = out.match(/^[0-9.eE+]+/); return m ? Math.round(parseFloat(m[0])) : NaN;
}

// ── golden mode: byte-lock the SOFTWARE canvas's ABSOLUTE output ───────────────────────────────
// The GPU↔SW diff below is a PARITY oracle: a subtle sampler change can move SW output yet stay in
// budget — or even improve parity on a given GPU (see docs/design/software-canvas.md, the 63→48 case).
// The SW canvas is deterministic across machines (det-probes), so a committed golden PNG per frame
// locks it: any sampler/rasterizer change flips a pixel and fails, regardless of GPU tie-breaking.
if (golden) {
  const goldDir = path.join('tools', 'canvas-golden', cart);
  if (bless) {
    run(testDir, { DE_SOFTWARE_CANVAS: 'on' }, 'software canvas', frames);
    fs.rmSync(goldDir, { recursive: true, force: true });
    fs.mkdirSync(goldDir, { recursive: true });
    for (let i = 0; i < frames; i++) { const f = path.join(testDir, framePng(i)); if (fs.existsSync(f)) fs.copyFileSync(f, path.join(goldDir, framePng(i))); }
    if (!keep) fs.rmSync(outRoot, { recursive: true, force: true });
    console.log(`blessed ${frames} golden frame(s) → ${goldDir}  (git add + commit them)`);
    process.exit(0);
  }
  if (!fs.existsSync(goldDir)) { console.error(`canvas-diff: no goldens for ${cart} — bless first:\n  node tools/canvas-diff.js ${cart} --golden --bless --frames N`); process.exit(2); }
  const golds = fs.readdirSync(goldDir).filter(f => /^frame_\d+\.png$/.test(f)).sort();
  if (!golds.length) { console.error(`canvas-diff: ${goldDir} has no golden frames`); process.exit(2); }
  run(testDir, { DE_SOFTWARE_CANVAS: 'on' }, 'software canvas', golds.length);
  let gfail = false;
  console.log(`\nsoftware-canvas render vs golden (${golds.length} frame(s), pixel-exact):\n`);
  for (const g of golds) {
    const cur = path.join(testDir, g);
    if (!fs.existsSync(cur)) { console.log(`  ${g}   \x1b[31mNOT RENDERED\x1b[0m`); gfail = true; continue; }
    const d = aePix(path.join(goldDir, g), cur), ok = d === 0;
    if (!ok) gfail = true;
    console.log(`  ${g}   ${ok ? 'identical' : '\x1b[31m'+d+'px DIFFER\x1b[0m'}`);
  }
  if (!keep) fs.rmSync(outRoot, { recursive: true, force: true });
  console.log(gfail ? `\n\x1b[31mFAIL\x1b[0m — software canvas changed vs golden (a sampler/rasterizer regression; re-bless with --golden --bless ONLY if the change is intended)`
                    : `\n\x1b[32mPASS\x1b[0m — software canvas byte-matches golden`);
  process.exit(gfail ? 1 : 0);
}

// ⚠ THE CONTROL — gotcha #4, and the one the other three could not see. The verdict is "no frame
// exceeded the budget", which is trivially true of no frames. Measured: with both runs producing
// nothing, this printed a green "PASS — canvas matches GPU within budget" and exited 0, under an
// empty frame table. The three documented gotchas are all about comparing the WRONG thing; this is
// about comparing NOTHING, and it is the more likely accident — a dump that renamed its frames, a
// cart that exits early, a --frames the run never reached. Partial coverage was silent too, so 3
// compared of 10 requested read exactly like 10 of 10.
// The per-frame verdict, as ONE named function used by both the loop and --selfcheck. It started
// out inline as `d > maxPx`, and the fixture re-implemented it — so mutating the real test left the
// suite green, which is a fixture that pins a copy of the logic rather than the logic. Written this
// way so there is only one of it.
// NOT `d > maxPx`: ae() returns NaN when magick cannot read a frame (measured on a truncated PNG),
// and every comparison with NaN is false, so an unreadable frame scored as within budget.
function withinBudget(d, maxPx) { return d <= maxPx; }

function controlCheck(compared, requested, missing) {
  const bad = [];
  if (compared === 0)
    bad.push(`NO frames were compared (${requested} requested) — an empty comparison is not a match; check that both runs actually dumped frames`);
  else if (missing.length)
    bad.push(`only ${compared} of ${requested} frames were compared — ${missing.length} never rendered in one or both modes (${missing.slice(0, 8).join(', ')}${missing.length > 8 ? ', …' : ''})`);
  return bad;
}

const refEnv  = raw ? {} : { DE_CPU_RASTER: 'on' };        // gotcha #2
console.error(`canvas-diff ${cart}: reference = GPU${raw ? ' (raw GL rasterizers)' : ' + DE_CPU_RASTER=on'}, test = DE_SOFTWARE_CANVAS=on, ${frames} frames`);
run(refDir,  refEnv, 'reference (GPU)');
run(testDir, { DE_SOFTWARE_CANVAS: 'on' }, 'test (software canvas)');

function frameFile(dir, i) { return path.join(dir, `frame_${String(i).padStart(5,'0')}.png`); }
function sha(f) { return crypto.createHash('sha256').update(fs.readFileSync(f)).digest('hex'); }
function ae(a, b) {            // gotcha #3: pixel diff, not shasum
  try { const out = execSync(`magick compare -metric AE "${a}" "${b}" null: 2>&1 || true`).toString().trim();
        const m = out.match(/^[0-9.eE+]+/); return m ? Math.round(parseFloat(m[0])) : NaN;
  } catch { return NaN; }
}

let worst = -1, worstFrame = -1, total = 0, n = 0, fail = false;
const missing = [];
console.log(`\nframe   ${bytecheck ? 'bytes' : 'diff px'}`);
for (let i = 0; i < frames; i++) {
  const a = frameFile(refDir, i), b = frameFile(testDir, i);
  // a frame that never rendered used to be skipped in silence, so 3 compared of 10 requested read
  // exactly like 10 of 10. Recorded now, and the control below refuses the run.
  if (!fs.existsSync(a) || !fs.existsSync(b)) { missing.push(i); continue; }
  if (bytecheck) {
    const same = sha(a) === sha(b);
    if (!same) fail = true;
    n++;
    console.log(`${String(i).padStart(5)}   ${same ? 'identical' : '\x1b[31mDIFFER\x1b[0m'}`);
    if (!same && worstFrame < 0) worstFrame = i;
  } else {
    const d = ae(a, b); n++;
    // ⚠ NOT `d > maxPx`. ae() returns NaN when magick cannot read a frame (measured with a
    // truncated PNG: "insufficient image data", no number to parse), and every comparison with NaN
    // is false — so an UNREADABLE frame scored as within budget and passed. Inverting the test
    // makes NaN fail, which is what golden mode already did with `d === 0`; the two modes
    // disagreed on the same condition.
    if (!withinBudget(d, maxPx)) fail = true;
    if (Number.isNaN(d)) {
      console.log(`${String(i).padStart(5)}   \x1b[31mUNREADABLE\x1b[0m  (magick could not compare these frames)`);
    } else {
      total += d;
      if (d > worst) { worst = d; worstFrame = i; }
      console.log(`${String(i).padStart(5)}   ${d}${d > maxPx ? '  \x1b[31m> '+maxPx+'\x1b[0m' : ''}`);
    }
  }
}
const control = controlCheck(n, frames, missing);
if (control.length) fail = true;

if (!bytecheck && n)
  console.log(`\nworst ${worst}px (frame ${worstFrame}), mean ${Math.round(total/n)}px, budget ${maxPx}px`);
if (heatmap && worstFrame >= 0 && haveMagick) {
  const hm = path.join(outRoot, `diff_frame_${String(worstFrame).padStart(5,'0')}.png`);
  try { execSync(`magick "${frameFile(refDir,worstFrame)}" "${frameFile(testDir,worstFrame)}" -compose difference -composite -auto-level "${hm}"`);
        console.log(`heatmap: ${hm}`); } catch {}
}

if (!keep && !heatmap) fs.rmSync(outRoot, { recursive: true, force: true });
else if (!keep && heatmap) { fs.rmSync(refDir, {recursive:true,force:true}); fs.rmSync(testDir, {recursive:true,force:true}); }

if (control.length) {
  console.error('\n\x1b[31m✗ THIS COMPARISON IS NOT EVIDENCE\x1b[0m');
  for (const c of control) console.error(`    ${c}`);
  console.error('  `node tools/canvas-diff.js --selfcheck` checks the comparison itself.');
} else {
  console.log(fail ? '\n\x1b[31mFAIL\x1b[0m — canvas render diverges from GPU (see above; --heatmap to localise)'
                   : `\n\x1b[32mPASS\x1b[0m — canvas matches GPU within budget (${n} frames)`);
}
process.exit(fail ? 1 : 0);
