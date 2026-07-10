#!/usr/bin/env node
// tools/squishy-features.js — the feature × drawtool COVERAGE oracle for the squishy cart.
//
// Every squishy brush is a render path, and each rim/fill feature (bevel / outline / drop-
// shadow / dither) is applied in that path — or silently isn't. The bug class this guards:
// a feature that quietly does NOTHING for a given brush (drip×dither and nib×outline both
// hid this way). This renders the cart's built-in SQUISHY_MATRIX grid — every brush (rows)
// against every feature (cols), each cell the SAME reference stroke with just that one
// feature on (col 0 = baseline) — then pixel-diffs each feature cell against its baseline.
// A cell identical to baseline = the feature is a no-op for that brush. Results are checked
// against the EXPECTED matrix below (intent), so declared-N/A cells (spray has no bevel) pass.
//
// Usage:
//   node tools/squishy-features.js            # run the check, print the matrix, exit nonzero on mismatch
//   node tools/squishy-features.js --keep      # also leave the grid PNG for eyeballing
//   node tools/squishy-features.js --png <f>   # diff an existing grid dump instead of rendering
//
// Layout constants MUST match draw_matrix() in tools/carts/squishy.c.
const fs = require('fs'), zlib = require('zlib'), path = require('path');
const { execFileSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..');
const MTX_LW = 40, MTX_HH = 12, SCREEN = 320;
// brush rows, in TOOL_DISP order (must match the cart's BRUSHES table)
const TOOLS = ['ink','pencil','liner','marker','chalk','sketch','spray','bristle','paint','nib','brushpen','reed','twist','oil'];
const NTOOLS = TOOLS.length;
const FEATURES = ['bevel', 'outline', 'shadow', 'dither', 'grad', 'boil'];   // cols 1..6 (col 0 = baseline)
const COLS = FEATURES.length + 1;   // + the baseline column (must match the cart's MC_COLS)
const cw = Math.floor((SCREEN - MTX_LW) / COLS), ch = Math.floor((SCREEN - MTX_HH) / NTOOLS);

// EXPECTED support per brush: which of [bevel,outline,shadow,dither,grad,boil] SHOULD change the render.
// 1 = must apply, 0 = intentionally inert (N/A for this brush). This is the source of truth for INTENT.
// gradient FILL covers every solid-body brush: the stamp family, the wet-paint + oil bodies
// (round silhouette) and the nib family (true ribbon shape) — only the airy brushes skip it.
const EXPECT = {
  ink:      [1,1,1,1,1,1], pencil: [1,1,1,1,1,1], liner: [1,1,1,1,1,1], marker: [1,1,1,1,1,1],
  chalk:    [1,1,1,1,1,1],
  sketch:   [0,0,0,0,0,1], spray:  [0,0,0,0,0,1], bristle:[0,0,0,0,0,1],
  paint:    [1,1,1,1,1,1],   // drip: rim/dither/gradient all apply to the wet body (drips run in the "from" colour)
  nib:      [1,1,1,1,1,1], brushpen:[1,1,1,1,1,1], reed:[1,1,1,1,1,1], twist:[1,1,1,1,1,1],
  oil:      [0,0,1,0,1,1],   // impasto: bevel intrinsic (not a toggle), no outline/dither; grad body + drop-shadow + boil apply
};
const APPLIED_MIN = 12;   // ≥ this many changed pixels in a cell ⇒ the feature did something

function decodePNG(file) {
  const b = fs.readFileSync(file);
  let off = 8, W, H, ct, idat = [];
  while (off < b.length) {
    const len = b.readUInt32BE(off), type = b.toString('latin1', off + 4, off + 8);
    const data = b.slice(off + 8, off + 8 + len);
    if (type === 'IHDR') { W = data.readUInt32BE(0); H = data.readUInt32BE(4); ct = data[9]; }
    else if (type === 'IDAT') idat.push(data);
    else if (type === 'IEND') break;
    off += 12 + len;
  }
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const chn = ct === 6 ? 4 : ct === 2 ? 3 : ct === 0 ? 1 : 4;
  const stride = W * chn, px = Buffer.alloc(H * stride);
  const paeth = (a, bb, c) => { const p = a + bb - c, pa = Math.abs(p - a), pb = Math.abs(p - bb), pc = Math.abs(p - c); return pa <= pb && pa <= pc ? a : pb <= pc ? bb : c; };
  let p = 0, r = 0;
  for (let y = 0; y < H; y++) {
    const f = raw[p++];
    for (let x = 0; x < stride; x++) {
      const cur = raw[p++];
      const a = x >= chn ? px[r + x - chn] : 0;
      const up = y > 0 ? px[r - stride + x] : 0;
      const ul = (x >= chn && y > 0) ? px[r - stride + x - chn] : 0;
      let v;
      switch (f) { case 0: v = cur; break; case 1: v = cur + a; break; case 2: v = cur + up; break;
        case 3: v = cur + ((a + up) >> 1); break; case 4: v = cur + paeth(a, up, ul); break; default: v = cur; }
      px[r + x] = v & 255;
    }
    r += stride;
  }
  return { W, H, ch: chn, px };
}

// count pixels differing between feature cell (col c) and the baseline cell (col 0) of row bi
function cellDiff(img, bi, c) {
  const { px, ch: chn, W } = img;
  const y0 = MTX_HH + bi * ch, xb = MTX_LW + 0 * cw, xf = MTX_LW + c * cw;
  let n = 0;
  for (let dy = 2; dy < ch - 2; dy++) {
    for (let dx = 2; dx < cw - 2; dx++) {
      const ib = ((y0 + dy) * W + (xb + dx)) * chn, iff = ((y0 + dy) * W + (xf + dx)) * chn;
      if (px[ib] !== px[iff] || px[ib + 1] !== px[iff + 1] || px[ib + 2] !== px[iff + 2]) n++;
    }
  }
  return n;
}

function renderGrid() {
  const outDir = path.join(ROOT, 'build', '.bake', 'squishy-matrix');
  fs.mkdirSync(outDir, { recursive: true });
  execFileSync('node', ['tools/play.js', 'squishy', 'run', '--headless', '--frames', '2',
    '--dump', outDir, '--dump-every', '1'],
    { cwd: ROOT, env: { ...process.env, SQUISHY_MATRIX: '1' }, stdio: 'pipe' });
  return path.join(outDir, 'frame_00001.png');
}

// ── main ──
const args = process.argv.slice(2);
const pngArg = args.indexOf('--png');
const pngFile = pngArg >= 0 ? args[pngArg + 1] : renderGrid();
const img = decodePNG(pngFile);
if (img.W !== SCREEN || img.H !== SCREEN) {
  console.error(`grid dump is ${img.W}×${img.H}, expected ${SCREEN}×${SCREEN} — layout mismatch`);
  process.exit(2);
}

const pad = s => s.padEnd(9);
console.log(`\nsquishy feature × drawtool coverage   (cell diff vs baseline, ⩾${APPLIED_MIN}px = applied)\n`);
console.log('  ' + pad('brush') + FEATURES.map(f => f.padStart(9)).join(''));
let fails = 0;
for (let bi = 0; bi < NTOOLS; bi++) {
  const name = TOOLS[bi], exp = EXPECT[name] || [1,1,1,1,1];
  let row = '  ' + pad(name);
  for (let c = 1; c <= FEATURES.length; c++) {
    const n = cellDiff(img, bi, c), applied = n >= APPLIED_MIN, want = exp[c - 1] === 1;
    let mark;
    if (want && applied)       mark = `✓${n}`;
    else if (want && !applied) { mark = `✗MISS(${n})`; fails++; }
    else if (!want && !applied) mark = `·`;
    else                       { mark = `!UNEXP(${n})`; fails++; }
    row += mark.padStart(9);
  }
  console.log(row);
}
console.log('\n  ✓ = feature applies · = intentionally inert (N/A)   ✗MISS = should apply but no-op   !UNEXP = inert-expected but changed');

if (!args.includes('--keep') && pngArg < 0) { try { fs.rmSync(path.dirname(pngFile), { recursive: true }); } catch {} }
if (fails) { console.error(`\nFAIL: ${fails} cell(s) disagree with the expected matrix.`); process.exit(1); }
console.log('\nPASS: every brush applies exactly the features it should.');
