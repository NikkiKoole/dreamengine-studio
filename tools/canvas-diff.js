#!/usr/bin/env node
// canvas-diff.js — the GPU-vs-software-canvas render oracle. The twin of mirror-diff.js /
// road-check.js, for the DE_SOFTWARE_CANVAS path. Compiles a cart once, runs it headless in BOTH
// modes, and pixel-diffs the frames — so a canvas change (a ported primitive, pal()/colorkey, a
// fill) is proven against the GPU truth in one command instead of a hand-rolled magick loop.
//
//   node tools/canvas-diff.js <cart> [--frames N] [--bytecheck] [--raw] [--max N] [--heatmap] [--keep]
//
//   --frames N    frames to render + compare (default 10)
//   --bytecheck   require BYTE-identical (sha256) instead of a pixel count — for the pset/fill
//                 primitives that ARE exact (cls/pset/pset_rgb/rectfill/spans; e.g. swcanvas_test).
//   --raw         compare against the TRUE shipping GPU line (DrawLine). Default sets DE_CPU_LINE=on
//                 on the reference so line()'s GPU-vs-sw_sline divergence doesn't pollute the diff
//                 (use --raw to MEASURE that divergence on purpose).
//   --max N       per-frame pixel-diff budget; exit nonzero if any frame exceeds it (default 0).
//   --heatmap     write a difference PNG for the worst frame (needs ImageMagick).
//   --keep        keep the dumped frames (build/.canvas-diff/<cart>/) for inspection.
//
// THE THREE GOTCHAS IT GUARDS (learned the hard way; see docs/design/software-canvas.md):
//   1. sw_force_gpu — a cart calling spr_rot/sspr_ex(deg)/rectfill_rot/print_rot/camera_ex(angle)
//      trips the sticky GPU fallback, so the =on build silently runs GPU after frame 0 and the A/B
//      is GPU-vs-GPU (proves nothing). We grep the source and WARN loudly.
//   2. line() divergence — neutralized by DE_CPU_LINE=on on the reference (unless --raw).
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
const frames    = parseInt(opt('--frames', '10'), 10);
const maxPx     = parseInt(opt('--max', '0'), 10);
const cart      = args[0];

if (!cart) { console.error('usage: node tools/canvas-diff.js <cart> [--frames N] [--bytecheck] [--raw] [--max N] [--heatmap] [--keep]'); process.exit(2); }

const src = path.join('tools/carts', `${cart}.c`);
if (!fs.existsSync(src)) { console.error(`canvas-diff: no such cart ${src}`); process.exit(2); }

// ── gotcha #1: detect primitives that trip sw_force_gpu, so a passing A/B isn't a lie ──────────
const source = fs.readFileSync(src, 'utf8');
// strip // line comments + /* */ blocks so a comment mentioning spr_rot doesn't false-positive
const code = source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');
const ROT = ['spr_rot', 'sspr_ex', 'rectfill_rot', 'print_rot', 'print_rot_scaled', 'camera_ex'];
const hits = ROT.filter(p => new RegExp(`\\b${p}\\s*\\(`).test(code));
if (hits.length) {
  console.error('\x1b[33m⚠ WARNING: this cart calls ' + hits.join(', ') + '\x1b[0m');
  console.error('  These can trip sw_force_gpu (rotation) → the =on build falls back to GPU mid-frame,');
  console.error('  so frames after the first are GPU-vs-GPU and prove NOTHING about the canvas.');
  console.error('  spr_rot/rectfill_rot ALWAYS trip it; sspr_ex/print_rot/camera_ex only when deg/angle≠0.');
  console.error('  Pick a rotation-free cart for a meaningful A/B, or trust only the result if these are unreached.\n');
}

// ── magick presence (only needed for AE / heatmap; bytecheck is pure node) ─────────────────────
let haveMagick = false;
try { execSync('magick -version', { stdio: 'ignore' }); haveMagick = true; } catch {}
if (!bytecheck && !haveMagick) { console.error('canvas-diff: ImageMagick (magick) not found — needed for pixel diff. Use --bytecheck, or `brew install imagemagick`.'); process.exit(2); }

const outRoot = path.join('build', '.canvas-diff', cart);
const refDir  = path.join(outRoot, raw ? 'ref-gpu-raw' : 'ref-gpu');
const testDir = path.join(outRoot, 'test-canvas');
fs.rmSync(outRoot, { recursive: true, force: true });

function run(dir, env, label) {
  fs.mkdirSync(dir, { recursive: true });
  try {
    execFileSync('node', ['tools/play.js', cart, 'script', '/dev/null', '--headless', '--frames', String(frames), '--dump', dir],
                 { stdio: 'ignore', timeout: 120000, env: { ...process.env, ...env } });
  } catch (e) { console.error(`canvas-diff: ${label} run failed — ${(e.message||'').slice(0,80)}`); process.exit(2); }
}

const refEnv  = raw ? {} : { DE_CPU_LINE: 'on' };          // gotcha #2
console.error(`canvas-diff ${cart}: reference = GPU${raw ? ' (raw DrawLine)' : ' + DE_CPU_LINE=on'}, test = DE_SOFTWARE_CANVAS=on, ${frames} frames`);
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
