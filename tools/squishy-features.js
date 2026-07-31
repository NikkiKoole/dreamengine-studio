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
//   node tools/squishy-features.js --json      # machine-readable verdicts
//   node tools/squishy-features.js --selfcheck # assert the CHECKER (known-answer fixture)
//
// Layout constants MUST match draw_matrix() in tools/carts/squishy.c.
const fs = require('fs'), zlib = require('zlib'), path = require('path');
const { execFileSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..');
const MTX_LW = 40, MTX_HH = 12, SCREEN = 320;
// brush rows, in TOOL_DISP order (must match the cart's BRUSHES table)
const TOOLS = ['ink','pencil','liner','marker','chalk','sketch','spray','bristle','paint','nib','brushpen','reed','twist','oil'];
const NTOOLS = TOOLS.length;
const FEATURES = ['bevel', 'outline', 'shadow', 'dither', 'grad', 'boil', 'close'];   // cols 1..7 (col 0 = baseline)
const COLS = FEATURES.length + 1;   // + the baseline column (must match the cart's MC_COLS)
const cw = Math.floor((SCREEN - MTX_LW) / COLS), ch = Math.floor((SCREEN - MTX_HH) / NTOOLS);

// EXPECTED support per brush: which of [bevel,outline,shadow,dither,grad,boil,close] SHOULD change the render.
// 1 = must apply, 0 = intentionally inert (N/A for this brush). This is the source of truth for INTENT.
// gradient FILL covers every solid-body brush: the stamp family, the wet-paint + oil bodies
// (round silhouette) and the nib family (true ribbon shape) — only the airy brushes skip it.
// CLOSE (the closed-shape vector fill) applies to EVERY brush: the fill is a property of the
// PATH; the brush is just the boundary treatment.
const EXPECT = {
  ink:      [1,1,1,1,1,1,1], pencil: [1,1,1,1,1,1,1], liner: [1,1,1,1,1,1,1], marker: [1,1,1,1,1,1,1],
  chalk:    [1,1,1,1,1,1,1],
  sketch:   [0,0,0,0,0,1,1], spray:  [0,0,0,0,0,1,1], bristle:[0,0,0,0,0,1,1],
  paint:    [1,1,1,1,1,1,1],   // drip: rim/dither/gradient all apply to the wet body (drips run in the "from" colour)
  nib:      [1,1,1,1,1,1,1], brushpen:[1,1,1,1,1,1,1], reed:[1,1,1,1,1,1,1], twist:[1,1,1,1,1,1,1],
  oil:      [0,0,1,0,1,1,1],   // impasto: bevel intrinsic (not a toggle), no outline/dither; grad body + drop-shadow + boil apply
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

// ── --selfcheck: assert the CHECKER against known answers ──────────────────────────────────
// See docs/guides/checks-and-oracles.md "Self-test the checker". Feeds the checker SYNTHETIC grid
// PNGs with known per-cell pixel differences (tools/fixtures/squishy-features/make-grid.js), so
// the judgement is tested without compiling and running the squishy cart — which would make the
// answer depend on the very render being audited.
//
// WHY. Two layers here fail silently and produce a plausible table either way:
//   1. A HAND-ROLLED PNG DECODER implementing all five scanline filters. Get Paeth or Average
//      wrong and every cell diff is garbage, but the report still prints tidy numbers. The filter
//      round-trip below encodes ONE logical image under each of filters 0-4 and demands identical
//      diffs; nothing else in the repo covers that decoder.
//   2. CELL GEOMETRY + the APPLIED_MIN threshold. An off-by-one in the cell origin, or a shifted
//      inset, silently compares the wrong rectangles — and a threshold that drifts turns
//      "the feature is a no-op for this brush" into "applied" with no visible symptom.
if (args.includes('--selfcheck')) {
  const os = require('os');
  const { makeGrid, CELL_CAP } = require(path.join(__dirname, 'fixtures', 'squishy-features', 'make-grid.js'));
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'squishy-selfcheck-'));
  const { execFileSync } = require('child_process');
  let seq = 0;
  const run = (opts) => {
    const f = path.join(tmp, `g${seq++}.png`);
    fs.writeFileSync(f, makeGrid(opts));
    let raw, code = 0;
    // stdio 'pipe' for stderr too: execFileSync INHERITS the child's stderr by default, so the
    // layout-guard case ("grid dump is 256x256…") printed straight through to our own stderr and
    // repo-doctor picked THAT up as this row's summary line instead of the 21/21 verdict.
    try { raw = execFileSync(process.execPath, [__filename, '--png', f, '--json'],
                             { encoding: 'utf8', maxBuffer: 1 << 26, stdio: ['ignore', 'pipe', 'pipe'] }); }
    catch (e) { raw = e.stdout || ''; code = e.status; }
    let j = null; try { j = JSON.parse(raw); } catch {}
    return { j, code, raw };
  };
  // paint a big diff in every cell EXPECT says should apply, and nothing where it says inert —
  // i.e. a grid that agrees with the intent matrix everywhere
  const agreeing = {};
  TOOLS.forEach((name, bi) => {
    const exp = EXPECT[name]; const row = {};
    exp.forEach((want, k) => { if (want === 1) row[k + 1] = 60; });
    agreeing[bi] = row;
  });

  const T = [];
  const t = (n, ok) => T.push({ n, ok });
  const cell = (j, brush, feat) => {
    const r = (j.rows || []).find(r => r.brush === brush);
    return (r ? r.cells.find(c => c.feature === feat) : null) || { n: -1, mark: 'ABSENT' };
  };

  // ── the PASS case, and that it is not passing blind
  const ok = run({ diffs: agreeing });
  t('a grid agreeing with EXPECT everywhere PASSes  [cry-wolf guard]',
    ok.j && ok.j.fails === 0 && ok.code === 0);
  t('...and it really measured 14 brushes x 7 features  [blind-pass guard]',
    ok.j && ok.j.rows.length === 14 && ok.j.rows.every(r => r.cells.length === 7));
  t('...and CELL_CAP agrees between generator and checker  [layout drift guard]',
    ok.j && ok.j.cellCap === CELL_CAP);

  // ── all four verdicts, from one grid. ink expects every feature; sketch expects only boil+close.
  const four = run({ diffs: { 0: { 1: 60, /* bevel applies -> ok */ }, 5: { 1: 60 /* sketch bevel: inert-expected but changed -> UNEXP */ } } });
  t('verdict ok: a feature that should apply and does', cell(four.j, 'ink', 'bevel').mark === 'ok');
  t('verdict MISS: a feature that should apply but is a no-op  [the bug class]',
    cell(four.j, 'ink', 'outline').mark === 'MISS');
  t('verdict inert: a feature declared N/A that does nothing  [exempt class]',
    cell(four.j, 'sketch', 'dither').mark === 'inert');
  t('verdict UNEXP: a feature declared N/A that changed the render',
    cell(four.j, 'sketch', 'bevel').mark === 'UNEXP');
  t('any disagreement exits nonzero  [it is a gate, not a report]', four.code === 1);

  // `four` holds BOTH a MISS and an UNEXP, so either one alone keeps the exit code at 1 — which
  // means it cannot prove that EACH is a failure. These two grids isolate them.
  const onlyMiss = (() => { const d = JSON.parse(JSON.stringify(agreeing)); delete d[0][1]; return run({ diffs: d }); })();
  t('MISS alone is a failure  [isolated: agreeing everywhere except one no-op]',
    onlyMiss.j && onlyMiss.j.fails === 1 && onlyMiss.code === 1 &&
    cell(onlyMiss.j, 'ink', 'bevel').mark === 'MISS');
  const onlyUnexp = (() => { const d = JSON.parse(JSON.stringify(agreeing)); d[5][1] = 60; return run({ diffs: d }); })();
  t('UNEXP alone is a failure too  [an N/A feature that started working is news]',
    onlyUnexp.j && onlyUnexp.j.fails === 1 && onlyUnexp.code === 1 &&
    cell(onlyUnexp.j, 'sketch', 'bevel').mark === 'UNEXP');

  // ── the APPLIED_MIN threshold, exactly at the boundary
  const thr = run({ diffs: { 0: { 1: 11, 2: 12 } } });
  t('threshold: 11 changed pixels is NOT applied  [one under APPLIED_MIN]',
    cell(thr.j, 'ink', 'bevel').n === 11 && cell(thr.j, 'ink', 'bevel').applied === false);
  t('threshold: 12 changed pixels IS applied  [exactly APPLIED_MIN]',
    cell(thr.j, 'ink', 'outline').n === 12 && cell(thr.j, 'ink', 'outline').applied === true);

  // ── the PNG decoder: one logical image under every scanline filter must decode identically
  const sig = (j) => j.rows.flatMap(r => r.cells.map(c => c.n)).join(',');
  const base = run({ diffs: { 0: { 1: 50, 5: 33 }, 3: { 2: 77 } }, filter: 0 });
  const filts = [1, 2, 3, 4].map(filter => run({ diffs: { 0: { 1: 50, 5: 33 }, 3: { 2: 77 } }, filter }));
  t('decoder: filter 1 (Sub) reconstructs identically', filts[0].j && sig(filts[0].j) === sig(base.j));
  t('decoder: filter 2 (Up) reconstructs identically',  filts[1].j && sig(filts[1].j) === sig(base.j));
  t('decoder: filter 3 (Average) reconstructs identically', filts[2].j && sig(filts[2].j) === sig(base.j));
  t('decoder: filter 4 (Paeth) reconstructs identically  [the fiddliest one]',
    filts[3].j && sig(filts[3].j) === sig(base.j));
  t('decoder: ...and the signature is not trivially empty  [inert-fixture guard]',
    cell(base.j, 'ink', 'bevel').n === 50 && cell(base.j, 'marker', 'outline').n === 77);

  // ── cell ORIGIN. Painted against the cell's BOTTOM edge on purpose: a cell is 22px tall but
  //    the header offset is only 12px, so a window shifted by the header still overlaps the TOP
  //    of the cell and a diff painted there is counted anyway (that mutation scored 18/18).
  const originRow = 13, originCol = 7;
  const org = run({ diffs: agreeing, bottom: { [originRow]: { [originCol]: 40 } } });
  t('origin: an exact count survives only if the cell origin includes the header offset',
    org.j && cell(org.j, TOOLS[originRow], FEATURES[originCol - 1]).n === 60 + 40);

  // ── cell geometry: the 2px inset, and alpha being ignored
  const geo = run({ diffs: agreeing, outside: { 5: [1, 2] } });
  t('geometry: a difference OUTSIDE the 2px inset is not counted  [border/label bleed]',
    geo.j && geo.j.fails === 0 && cell(geo.j, 'sketch', 'bevel').n === 0);
  const al = run({ diffs: agreeing, alphaOnly: { 5: [1] } });
  t('geometry: an ALPHA-only difference is not counted  [cellDiff compares RGB]',
    al.j && al.j.fails === 0 && cell(al.j, 'sketch', 'bevel').n === 0);

  // ── the layout guard: a wrong-sized dump must be refused, not silently mis-offset
  const wrong = run({ diffs: agreeing, size: 256 });
  t('layout: a dump of the wrong size is REFUSED (exit 2), not diffed at wrong offsets',
    wrong.code === 2);

  try { fs.rmSync(tmp, { recursive: true }); } catch {}
  const failed = T.filter(x => !x.ok);
  for (const x of T) console.log(`  ${x.ok ? '\x1b[32m✓\x1b[0m' : '\x1b[31m✗\x1b[0m'} ${x.n}`);
  console.log(failed.length
    ? `\x1b[31msquishy-features --selfcheck FAILED\x1b[0m — ${failed.length} of ${T.length} expectations broken`
    : `squishy-features --selfcheck: ${T.length}/${T.length} known answers correct`);
  process.exit(failed.length ? 1 : 0);
}


const pngArg = args.indexOf('--png');
const pngFile = pngArg >= 0 ? args[pngArg + 1] : renderGrid();
const img = decodePNG(pngFile);
if (img.W !== SCREEN || img.H !== SCREEN) {
  console.error(`grid dump is ${img.W}×${img.H}, expected ${SCREEN}×${SCREEN} — layout mismatch`);
  process.exit(2);
}

// ── the verdicts, computed ONCE ─────────────────────────────────────────────────────────────
// Shared by --json and the table below. They used to compute `mark` and the failure count
// SEPARATELY, which is the same "written in two places that must agree" hazard lint-aux-params
// exists for: mutating the table's verdict logic left --json (and therefore --selfcheck) reading
// the old answer, so two mutations that made the gate never fail still scored 21/21.
const grid = TOOLS.map((name, bi) => {
  const exp = EXPECT[name] || [1,1,1,1,1,1,1];
  return { brush: name, cells: FEATURES.map((f, k) => {
    const n = cellDiff(img, bi, k + 1), applied = n >= APPLIED_MIN, want = exp[k] === 1;
    return { feature: f, n, applied, want,
             mark: want && applied ? 'ok' : want && !applied ? 'MISS' : !want && !applied ? 'inert' : 'UNEXP' };
  }) };
});
const badCells = grid.flatMap(r => r.cells).filter(c => c.mark === 'MISS' || c.mark === 'UNEXP');
const fails = badCells.length;
const cleanup = () => { if (!args.includes('--keep') && pngArg < 0) { try { fs.rmSync(path.dirname(pngFile), { recursive: true }); } catch {} } };

if (args.includes('--json')) {
  console.log(JSON.stringify({ appliedMin: APPLIED_MIN, cellCap: (ch - 4) * (cw - 4),
                               screen: { w: img.W, h: img.H }, fails, rows: grid }, null, 2));
  cleanup();
  process.exit(fails ? 1 : 0);
}

const pad = s => s.padEnd(9);
console.log(`\nsquishy feature × drawtool coverage   (cell diff vs baseline, \u2a7e${APPLIED_MIN}px = applied)\n`);
console.log('  ' + pad('brush') + FEATURES.map(f => f.padStart(9)).join(''));
const MARK = { ok: c => `\u2713${c.n}`, MISS: c => `\u2717MISS(${c.n})`, inert: () => '\u00b7', UNEXP: c => `!UNEXP(${c.n})` };
for (const r of grid)
  console.log('  ' + pad(r.brush) + r.cells.map(c => MARK[c.mark](c).padStart(9)).join(''));
console.log('\n  ✓ = feature applies · = intentionally inert (N/A)   ✗MISS = should apply but no-op   !UNEXP = inert-expected but changed');

cleanup();
if (fails) { console.error(`\nFAIL: ${fails} cell(s) disagree with the expected matrix.`); process.exit(1); }
console.log('\nPASS: every brush applies exactly the features it should.');
