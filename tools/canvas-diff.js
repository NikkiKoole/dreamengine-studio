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

if (!cart) { console.error('usage: node tools/canvas-diff.js <cart> [--frames N] [--bytecheck] [--raw] [--max N] [--heatmap] [--keep]\n       node tools/canvas-diff.js <cart> --golden [--bless] [--frames N]'); process.exit(2); }

const src = path.join('tools/carts', `${cart}.c`);
if (!fs.existsSync(src)) { console.error(`canvas-diff: no such cart ${src}`); process.exit(2); }

// ── gotcha #1: detect primitives that trip sw_force_gpu, so a passing A/B isn't a lie ──────────
const source = fs.readFileSync(src, 'utf8');

// Per-cart declared budget: a cart may carry `// canvas-diff: max N` when it INTENTIONALLY exercises
// a primitive that can't be byte-portable across GPUs (e.g. a fractional-scale sspr, whose
// nearest-neighbour texel-boundary ties the GPU and CPU floor resolve opposite ways — see
// docs/design/software-canvas.md). An explicit --max always wins; else this default; else 0. This is
// what stops `canvas-diff <cart>` from being a recurring false alarm on a known, accepted divergence.
const declared = source.match(/\/\/\s*canvas-diff:\s*max\s+(\d+)/);
const maxPx    = maxArg != null ? parseInt(maxArg, 10)
               : declared      ? parseInt(declared[1], 10)
               :                 0;
if (maxArg == null && declared) console.error(`canvas-diff: using ${cart}'s declared budget --max ${maxPx} (\`// canvas-diff: max\` in source)\n`);
// strip // line comments + /* */ blocks so a comment mentioning spr_rot doesn't false-positive
const code = source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');
// All rotated PRIMITIVES (rectfill_rot/spr_rot/sspr_ex/print_rot) now render in software. The only
// thing still tripping the sticky sw_force_gpu fallback is a rotating camera (Tier-2, by design):
const ROT = ['camera_ex'];
const hits = ROT.filter(p => new RegExp(`\\b${p}\\s*\\(`).test(code));
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
console.log(`\nframe   ${bytecheck ? 'bytes' : 'diff px'}`);
for (let i = 0; i < frames; i++) {
  const a = frameFile(refDir, i), b = frameFile(testDir, i);
  if (!fs.existsSync(a) || !fs.existsSync(b)) continue;
  if (bytecheck) {
    const same = sha(a) === sha(b);
    if (!same) fail = true;
    console.log(`${String(i).padStart(5)}   ${same ? 'identical' : '\x1b[31mDIFFER\x1b[0m'}`);
    if (!same && worstFrame < 0) worstFrame = i;
  } else {
    const d = ae(a, b); n++; total += d;
    if (d > worst) { worst = d; worstFrame = i; }
    if (d > maxPx) fail = true;
    console.log(`${String(i).padStart(5)}   ${d}${d > maxPx ? '  \x1b[31m> '+maxPx+'\x1b[0m' : ''}`);
  }
}

if (!bytecheck && n)
  console.log(`\nworst ${worst}px (frame ${worstFrame}), mean ${Math.round(total/n)}px, budget ${maxPx}px`);
if (heatmap && worstFrame >= 0 && haveMagick) {
  const hm = path.join(outRoot, `diff_frame_${String(worstFrame).padStart(5,'0')}.png`);
  try { execSync(`magick "${frameFile(refDir,worstFrame)}" "${frameFile(testDir,worstFrame)}" -compose difference -composite -auto-level "${hm}"`);
        console.log(`heatmap: ${hm}`); } catch {}
}

if (!keep && !heatmap) fs.rmSync(outRoot, { recursive: true, force: true });
else if (!keep && heatmap) { fs.rmSync(refDir, {recursive:true,force:true}); fs.rmSync(testDir, {recursive:true,force:true}); }

console.log(fail ? '\n\x1b[31mFAIL\x1b[0m — canvas render diverges from GPU (see above; --heatmap to localise)'
                 : '\n\x1b[32mPASS\x1b[0m — canvas matches GPU within budget');
process.exit(fail ? 1 : 0);
