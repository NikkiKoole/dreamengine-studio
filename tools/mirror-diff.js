#!/usr/bin/env node
// mirror-diff.js — the GOLDEN-PIXEL-DIFF / visual-symmetry harness (STATUS.md §43).
//
// The framebuffer twin of tune-check / level-check: it can't be a spec() because the
// pixels are the output of the engine's polyfill/line scan-conversion, not a pure
// function of the cart's inputs (see docs/design/spec-harness.md). Instead it renders a
// cart HEADLESS, then asserts a SYMMETRY INVARIANT on the rendered pixels: a cart whose
// scene is mirror-symmetric (e.g. streetlab's default 4-way junction about x=160) should
// be pixel-symmetric too. It isn't — that's the ≤1px corner floor the road docs accept
// (docs/design/road-program-state.md "Accepted floor", streetlab.c §SEAM). This tool
// MEASURES that floor and, once the symmetric-corner mirror-blit fix lands
// (docs/design/streetlab-corner-symmetry-plan.md), GATES it back to zero.
//
// Promoted from the throwaway pngdiff.js prototype written during the arcsym
// investigation (the cart `arcsym` is the petri-dish demo of the same mechanism).
//
// Usage:
//   node tools/mirror-diff.js <cart>            render the cart headless, then diff
//   node tools/mirror-diff.js --png <file.png>  diff an existing PNG (e.g. a --dump frame)
// Options:
//   --axis h|v|both   mirror axis: h = left/right about cx (default), v = up/down, both
//   --cx <x> --cy <y> reflection centre (default: image centre W/2, H/2)
//   --band y0,y1      only compare rows in [y0,y1) (skip title/toolbar/dashed lanes)
//   --xband x0,x1     only compare columns in [x0,x1)
//   --overlay <file>  write a magnified PNG with mismatched pairs painted red
//   --frames <n>      frames to run before dumping (default 2)
//   --json            machine-readable output
//   --quiet           print nothing on success; exit 1 if any mismatch (CI gate)
//   --expect <n>      gate an ACCEPTED nonzero floor: exit 1 unless the count is exactly n
//   --selfcheck       known answers for the comparison itself (renders nothing)
//
// ⚠ A COMPARISON OF NOTHING IS NOT SYMMETRY. `--band 500,600` on a 200-tall frame compared 0 of 0
// pixel-pairs and exited 0 — the gate silently disabled by a typo and reporting a pass. The control
// below refuses that in every mode. And `--expect` exists because this tool could not hold a
// number: the roadkit extraction was gated on "mirror-diff 68=68", which was a person reading two
// terminal outputs, since the accepted floor is nonzero and `--quiet` can only assert zero.
//
// Zero-dependency: its own minimal PNG decode (zlib + the 5 PNG filters) and encode.

const fs = require('fs'), zlib = require('zlib'), path = require('path');
const { execFileSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..');

// ── arg parsing ──────────────────────────────────────────────
const args = process.argv.slice(2);
function opt(name, def) { const i = args.indexOf(name); return i >= 0 ? args[i + 1] : def; }
function flag(name) { return args.includes(name); }
const QUIET = flag('--quiet'), JSON_OUT = flag('--json');
const axis = opt('--axis', 'h');
const overlayPath = opt('--overlay', null);
const frames = parseInt(opt('--frames', '2'), 10);
const pngArg = opt('--png', null);
const bandArg = opt('--band', null), xbandArg = opt('--xband', null);
const expectRaw = opt('--expect', null);
const expectArg = expectRaw === null ? null : parseInt(expectRaw, 10);
if (expectRaw !== null && !Number.isInteger(expectArg)) {
  console.error(`--expect wants a whole number of mismatched pairs, got "${expectRaw}"`);
  process.exit(2);
}
const positional = args.filter((a, i) => !a.startsWith('--') && !(i > 0 && args[i - 1].startsWith('--') && args[i - 1] !== '--json' && args[i - 1] !== '--quiet'));
const cart = pngArg ? null : positional[0];

// before the usage guard: --selfcheck needs neither a cart nor a png. The helpers it calls are all
// function declarations, so they are hoisted and reachable from here.
if (flag('--selfcheck')) process.exit(selfcheck());

if (!pngArg && !cart) {
  console.error('usage: node tools/mirror-diff.js <cart> | --png <file>  [--axis h|v|both] [--band y0,y1] [--expect n] [--overlay out.png] [--json] [--quiet]');
  process.exit(2);
}

// ── PNG decode (RGBA/RGB/gray, all 5 filters) ────────────────
function decodePNG(file) {
  const b = fs.readFileSync(file);
  let off = 8, W, H, ct, bitDepth, interlace, idat = [];
  while (off < b.length) {
    const len = b.readUInt32BE(off), type = b.toString('latin1', off + 4, off + 8);
    const data = b.slice(off + 8, off + 8 + len);
    if (type === 'IHDR') { W = data.readUInt32BE(0); H = data.readUInt32BE(4);
                           bitDepth = data[8]; ct = data[9]; interlace = data[12]; }
    else if (type === 'IDAT') idat.push(data);
    else if (type === 'IEND') break;
    off += 12 + len;
  }
  // ⚠ REFUSE WHAT THIS DECODER CANNOT ACTUALLY READ. It handles 8-bit non-interlaced
  // grey / RGB / RGBA and nothing else, but it used to fall through to `ch = 4` for every other
  // colour type — so a PALETTE png (1 byte per pixel + a PLTE table this decoder ignores) was read
  // with a 4x stride against a buffer a quarter the expected size, producing zeros. Measured with
  // an 8x4 palette image, left half red and right half black, i.e. maximally ASYMMETRIC: it
  // reported 2 mismatches of 16 instead of 16 of 16. An image whose garbage happened to come out
  // symmetric would have reported a clean PASS. A decoder that cannot read its input must say so,
  // not answer anyway. (ct 4 = grey+alpha is refused for the same reason: it needs ch 2.)
  if (bitDepth !== 8) throw new Error(`${file}: ${bitDepth}-bit PNG — this decoder only reads 8-bit`);
  if (interlace) throw new Error(`${file}: interlaced PNG — this decoder only reads non-interlaced`);
  if (ct !== 0 && ct !== 2 && ct !== 6)
    throw new Error(`${file}: PNG colour type ${ct}${ct === 3 ? ' (palette)' : ct === 4 ? ' (grey+alpha)' : ''} — this decoder reads only 0 (grey), 2 (RGB) and 6 (RGBA)`);
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const ch = ct === 6 ? 4 : ct === 2 ? 3 : 1;
  if (raw.length < H * (W * ch + 1))
    throw new Error(`${file}: truncated image data (${raw.length} bytes, expected ${H * (W * ch + 1)})`);
  const stride = W * ch, px = Buffer.alloc(H * stride);
  const paeth = (a, bb, c) => { const p = a + bb - c, pa = Math.abs(p - a), pb = Math.abs(p - bb), pc = Math.abs(p - c); return pa <= pb && pa <= pc ? a : pb <= pc ? bb : c; };
  let p = 0, r = 0;
  for (let y = 0; y < H; y++) {
    const f = raw[p++];
    for (let x = 0; x < stride; x++) {
      const cur = raw[p++];
      const a = x >= ch ? px[r + x - ch] : 0;
      const up = y > 0 ? px[r - stride + x] : 0;
      const ul = (x >= ch && y > 0) ? px[r - stride + x - ch] : 0;
      let v;
      switch (f) { case 0: v = cur; break; case 1: v = cur + a; break; case 2: v = cur + up; break;
        case 3: v = cur + ((a + up) >> 1); break; case 4: v = cur + paeth(a, up, ul); break; default: v = cur; }
      px[r + x] = v & 255;
    }
    r += stride;
  }
  return { W, H, ch, px };
}

// ── PNG encode (RGB, filter 0) for the overlay ───────────────
function crc32(buf) { let c = ~0; for (let i = 0; i < buf.length; i++) { c ^= buf[i]; for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xEDB88320 & -(c & 1)); } return ~c >>> 0; }
function chunk(type, data) { const t = Buffer.from(type, 'latin1'), b = Buffer.concat([t, data]); const o = Buffer.alloc(12 + data.length); o.writeUInt32BE(data.length, 0); t.copy(o, 4); data.copy(o, 8); o.writeUInt32BE(crc32(b), 8 + data.length); return o; }
function encodePNG(W, H, rgb) {
  const stride = W * 3, raw = Buffer.alloc(H * (stride + 1));
  for (let y = 0; y < H; y++) { raw[y * (stride + 1)] = 0; rgb.copy(raw, y * (stride + 1) + 1, y * stride, y * stride + stride); }
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(W, 0); ihdr.writeUInt32BE(H, 4); ihdr[8] = 8; ihdr[9] = 2;
  const sig = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
  return Buffer.concat([sig, chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}

// ── the comparison, as a pure function of an image ───────────
// Lifted out of the top-level script so --selfcheck can put known images through it. Behaviour is
// unchanged: verified against streetlab's three documented invocations before anything else moved.
// GREY (ch 1) is handled here rather than read as three consecutive pixels, which is what the old
// inline version did — it only ever saw the engine's RGBA dumps, so the bug had no way to show.
function pixelAt(img, x, y) {
  const i = (y * img.W + x) * img.ch;
  if (img.ch === 1) { const v = img.px[i]; return (v << 16) | (v << 8) | v; }
  return (img.px[i] << 16) | (img.px[i + 1] << 8) | img.px[i + 2];
}

function mirrorDiff(img, o) {
  const { W, H } = img;
  const useH = o.axis === 'h' || o.axis === 'both';
  const useV = o.axis === 'v' || o.axis === 'both';
  const mx_ = x => Math.round(2 * o.cx - 1 - x);
  const my_ = y => Math.round(2 * o.cy - 1 - y);
  let pairs = 0, mism = 0; const rowmis = {}, marks = [];
  for (let y = o.by0; y < o.by1; y++) {
    for (let x = o.bx0; x < o.bx1; x++) {
      if (y < 0 || y >= H || x < 0 || x >= W) continue;
      const mx = useH ? mx_(x) : x, my = useV ? my_(y) : y;
      if (mx < 0 || mx >= W || my < 0 || my >= H) continue;
      if (useH && mx <= x && !useV) continue;        // count each L/R pair once
      pairs++;
      if (pixelAt(img, x, y) !== pixelAt(img, mx, my)) {
        mism++; rowmis[y] = (rowmis[y] || 0) + 1; marks.push([x, y]);
      }
    }
  }
  return { pairs, mism, rowmis, marks };
}

// ⚠ THE CONTROL. Comparing NOTHING is not symmetry. A band outside the image, an out-of-range
// centre, or a transposed x/y band leaves the loop with no iterations, and this printed
// "mirror mismatch: 0 of 0 compared pixel-pairs" and exited 0 — measured with `--band 500,600` on
// a 200-tall frame. That is the whole gate turning into a no-op on a typo, reported as a pass.
// There is no threshold that could catch it: zero mismatches out of zero pairs is as clean as the
// number gets. Also flags the far side of the same mistake — a band that only partly overlaps the
// image silently measures less than it claims to.
function controlCheck(img, o, res) {
  const bad = [];
  if (res.pairs === 0)
    bad.push(`0 pixel-pairs were compared — nothing was checked, which is not the same as symmetric`
      + ` (band y ${o.by0},${o.by1} and x ${o.bx0},${o.bx1} against a ${img.W}x${img.H} image)`);
  else {
    const clipY = Math.max(0, -o.by0) + Math.max(0, o.by1 - img.H);
    const clipX = Math.max(0, -o.bx0) + Math.max(0, o.bx1 - img.W);
    if (clipY || clipX)
      bad.push(`the band reaches outside the ${img.W}x${img.H} image (${clipY} row(s), ${clipX} column(s) ignored) — it is measuring less than it names`);
  }
  return bad;
}

// ── the verdict ──────────────────────────────────────────────
// The exploratory/gate split is deliberate and kept exactly as it was: a bare run MEASURES and
// exits 0 whatever it finds (that is how the docs use it), `--quiet` gates at zero. What is new is
// `--expect <n>`, and it exists because this tool could not hold the number it was being used for.
// The roadkit extraction was gated on "mirror-diff 68=68" — a HUMAN reading two terminal outputs,
// because streetlab's accepted floor is nonzero and `--quiet` can only assert zero. Nothing ran it
// again afterwards. A control failure overrides both modes: `0 of 0` is not a measurement in
// either, so it can never be the exploratory kind of exit 0.
function judge(res, expect, control) {
  if (control && control.length) return { ok: false, msg: null };
  if (expect !== null && expect !== undefined)
    return res.mism === expect ? { ok: true, msg: null }
      : { ok: false, msg: `expected ${expect} mismatched pair(s), got ${res.mism}` };
  if (QUIET) return res.mism === 0 ? { ok: true, msg: null }
    : { ok: false, msg: null };                 // the count is already printed above
  return { ok: true, msg: null };               // exploratory: measure, do not judge
}

// ── --selfcheck: KNOWN ANSWERS FOR THE COMPARISON ────────────
// Renders no cart. Every image here is built by hand with a symmetry that can be counted on paper,
// so the expected numbers are arithmetic rather than blessed output.
function selfcheck() {
  let pass = 0, fail = 0;
  const ok = (name, cond, got) => {
    if (cond) { pass++; console.log(`  ✓ ${name}`); } else { fail++; console.log(`  ✗ ${name}   got: ${got}`); }
  };
  // build an RGBA image from a row-major array of 0xRRGGBB
  const img = (W, H, f) => {
    const px = Buffer.alloc(W * H * 4);
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      const c = f(x, y), i = (y * W + x) * 4;
      px[i] = (c >> 16) & 255; px[i + 1] = (c >> 8) & 255; px[i + 2] = c & 255; px[i + 3] = 255;
    }
    return { W, H, ch: 4, px };
  };
  const full = (im, o = {}) => ({ axis: 'h', cx: im.W / 2, cy: im.H / 2,
    by0: 0, by1: im.H, bx0: 0, bx1: im.W, ...o });
  const run = (im, o) => mirrorDiff(im, full(im, o));

  console.log('mirror-diff --selfcheck — known answers for the comparison (nothing is rendered)\n');

  console.log('THE ARITHMETIC OF A MIRROR');
  const sym = img(8, 4, (x) => (x < 4 ? 3 - x : x - 4) * 0x110000);   // left/right mirrored
  ok('a left/right symmetric image has 0 mismatches', run(sym).mism === 0, run(sym).mism);
  ok('  …and compares W*H/2 = 16 pairs, each counted once', run(sym).pairs === 16, run(sym).pairs);
  const asym = img(8, 4, (x) => x < 4 ? 0xff0000 : 0x000000);         // maximally asymmetric
  ok('a maximally asymmetric image mismatches EVERY pair', run(asym).mism === 16, run(asym).mism);
  const one = img(8, 4, (x, y) => (x === 0 && y === 0) ? 0xffffff : 0);
  ok('a single odd pixel is exactly 1 mismatched pair', run(one).mism === 1, run(one).mism);
  ok('  …and it is reported on its own row', JSON.stringify(run(one).rowmis) === '{"0":1}',
     JSON.stringify(run(one).rowmis));
  // v-symmetric in y (rows 0↔3, 1↔2 agree) and deliberately h-ASYMMETRIC in x, so the two axes
  // cannot both be satisfied by the same image. The first draft of this varied only with y, which
  // made it symmetric on BOTH axes and the h assertion went red — the image was wrong, not the code
  const vsym = img(8, 4, (x, y) => (x < 4 ? 0x110000 : 0x220000) + (y < 2 ? 1 - y : y - 2) * 0x000100);
  ok('vertical symmetry is found on the v axis', run(vsym, { axis: 'v' }).mism === 0,
     run(vsym, { axis: 'v' }).mism);
  ok('  …and the SAME image is asymmetric on the h axis (the axes are independent)',
     run(vsym, { axis: 'h' }).mism === 16, run(vsym, { axis: 'h' }).mism);

  console.log('\nA CHARACTERISTIC WORTH KNOWING BEFORE COMPARING TWO RUNS');
  // the "count each pair once" guard only covers the h-only case, so `pairs` means different
  // things per axis: half the pixels on h, all of them on v and both. Pinned, not "fixed" —
  // changing it would silently move every number anyone has ever recorded from this tool.
  ok('h compares each pair ONCE (16 of 32 pixels)', run(sym).pairs === 16, run(sym).pairs);
  ok('v compares every pixel (32) — pairs are counted twice', run(sym, { axis: 'v' }).pairs === 32,
     run(sym, { axis: 'v' }).pairs);
  ok('both compares every pixel too, so an h count and a v count are not comparable',
     run(sym, { axis: 'both' }).pairs === 32, run(sym, { axis: 'both' }).pairs);

  console.log('\nTHE CONTROL — comparing nothing is not symmetry');
  const outside = full(sym, { by0: 500, by1: 600 });
  const rOut = mirrorDiff(sym, outside);
  ok('a band outside the image compares 0 pairs', rOut.pairs === 0, rOut.pairs);
  ok('  …which the control refuses', controlCheck(sym, outside, rOut).some(c => c.includes('0 pixel-pairs')),
     controlCheck(sym, outside, rOut));
  ok('  …and it can never be an exploratory pass, even without --quiet',
     judge(rOut, null, controlCheck(sym, outside, rOut)).ok === false, 'passed');
  const trans = full(sym, { by0: 3, by1: 1 });
  ok('a transposed band (y1 < y0) compares nothing and is refused',
     controlCheck(sym, trans, mirrorDiff(sym, trans)).length > 0, 'silent');
  const partial = full(sym, { by0: 0, by1: 40 });
  ok('a band that only PARTLY overlaps is flagged as measuring less than it names',
     controlCheck(sym, partial, mirrorDiff(sym, partial)).some(c => c.includes('reaches outside')), 'silent');
  ok('  …while a band inside the image is silent',
     controlCheck(sym, full(sym, { by0: 1, by1: 3 }), run(sym, { by0: 1, by1: 3 })).length === 0, 'flagged');

  console.log('\nTHE DECODER REFUSES WHAT IT CANNOT READ');
  const os = require('os');
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'mirrordiff-'));
  const mkpng = (ct, bitDepth, body, extra) => {
    const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(8, 0); ihdr.writeUInt32BE(4, 4);
    ihdr[8] = bitDepth; ihdr[9] = ct;
    const parts = [Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr)];
    if (extra) parts.push(extra);
    parts.push(chunk('IDAT', zlib.deflateSync(body)), chunk('IEND', Buffer.alloc(0)));
    const f = path.join(dir, `t${ct}-${bitDepth}.png`); fs.writeFileSync(f, Buffer.concat(parts)); return f;
  };
  const threw = (f) => { try { decodePNG(f); return null; } catch (e) { return e.message; } };
  // the measured case: an 8x4 PALETTE image, left half red, right half black — maximally
  // asymmetric, and the old decoder read it as 2 mismatches of 16 instead of 16 of 16
  const palBody = Buffer.alloc(4 * 9);
  for (let y = 0; y < 4; y++) for (let x = 0; x < 8; x++) palBody[y * 9 + 1 + x] = x < 4 ? 1 : 0;
  const palette = mkpng(3, 8, palBody, chunk('PLTE', Buffer.from([0, 0, 0, 255, 0, 0])));
  ok('a PALETTE png is refused, not silently read as garbage',
     (threw(palette) || '').includes('palette'), threw(palette));
  ok('a 16-bit png is refused', (threw(mkpng(2, 16, Buffer.alloc(4 * (8 * 6 + 1)))) || '').includes('16-bit'),
     threw(mkpng(2, 16, Buffer.alloc(4 * (8 * 6 + 1)))));
  ok('a GREY+ALPHA png is refused (it needs 2 channels, not 4)',
     (threw(mkpng(4, 8, Buffer.alloc(4 * (8 * 2 + 1)))) || '').includes('grey+alpha'),
     threw(mkpng(4, 8, Buffer.alloc(4 * (8 * 2 + 1)))));
  const rgbBody = Buffer.alloc(4 * (8 * 3 + 1));
  ok('  …while a plain RGB png is accepted', threw(mkpng(2, 8, rgbBody)) === null, threw(mkpng(2, 8, rgbBody)));
  ok('truncated image data is refused rather than read as zeros',
     (threw(mkpng(2, 8, Buffer.alloc(10))) || '').includes('truncated'), threw(mkpng(2, 8, Buffer.alloc(10))));
  // grey is legal, and must not be read as three consecutive pixels
  const greyBody = Buffer.alloc(4 * 9);
  for (let y = 0; y < 4; y++) for (let x = 0; x < 8; x++) greyBody[y * 9 + 1 + x] = x < 4 ? 200 : 200;
  const grey = decodePNG(mkpng(0, 8, greyBody));
  ok('a GREY png decodes, and a flat grey image is symmetric', mirrorDiff(grey, full(grey)).mism === 0,
     mirrorDiff(grey, full(grey)).mism);
  fs.rmSync(dir, { recursive: true, force: true });

  console.log('\nTHE VERDICT, INCLUDING THE MODE SPLIT THAT WAS ALREADY THERE');
  const r0 = { mism: 0, pairs: 16 }, r5 = { mism: 5, pairs: 16 };
  ok('--expect matching the count passes', judge(r5, 5, []).ok === true, judge(r5, 5, []));
  ok('--expect NOT matching fails, and says both numbers',
     judge(r5, 4, []).ok === false && judge(r5, 4, []).msg.includes('got 5'), judge(r5, 4, []).msg);
  ok('  …which is what "68=68" needed and could not express before',
     judge({ mism: 68, pairs: 14400 }, 68, []).ok === true, 'failed');
  // FEWER mismatches than expected must fail too, and this is the assertion that matters most.
  // Mutation-testing caught its absence: changing `===` to `<=` left the whole suite green, so a
  // symmetry fix that improved the count would have slipped past the gate unrecorded. An accepted
  // floor is a number you re-bless on purpose, not a ceiling to come in under.
  ok('a count BELOW the expectation fails too — the floor is a value, not a ceiling',
     judge({ mism: 12, pairs: 14400 }, 68, []).ok === false, 'passed');
  ok('  …and it says so plainly', (judge({ mism: 12 }, 68, []).msg || '').includes('got 12'),
     judge({ mism: 12 }, 68, []).msg);
  ok('--expect 0 is still a real assertion', judge(r0, 0, []).ok === true && judge(r5, 0, []).ok === false,
     'inconsistent');
  ok('a control failure beats a matching --expect (nothing was compared)',
     judge(r0, 0, ['nothing compared']).ok === false, 'passed');

  console.log(`\n${fail ? '✗' : '✓'} ${pass} passed, ${fail} failed`);
  return fail ? 1 : 0;
}

// ── render the cart headless if no --png given ───────────────
function renderCart(name) {
  const dump = path.join(ROOT, 'build', '.mirror-diff', name);
  fs.rmSync(dump, { recursive: true, force: true });
  fs.mkdirSync(dump, { recursive: true });
  execFileSync('node', ['tools/play.js', name, 'script', '/dev/null', '--headless',
    '--frames', String(frames), '--dump', dump, '--dump-every', '1'],
    { cwd: ROOT, stdio: 'ignore' });
  const fr = fs.readdirSync(dump).filter(f => f.endsWith('.png')).sort();
  if (!fr.length) throw new Error('no frame dumped for ' + name);
  return path.join(dump, fr[fr.length - 1]);
}

// ── run ──────────────────────────────────────────────────────
const file = pngArg || renderCart(cart);
const img = decodePNG(file);
const { W, H, ch, px } = img;
const cx = parseFloat(opt('--cx', String(W / 2)));
const cy = parseFloat(opt('--cy', String(H / 2)));
const [by0, by1] = bandArg ? bandArg.split(',').map(Number) : [0, H];
const [bx0, bx1] = xbandArg ? xbandArg.split(',').map(Number) : [0, W];
const at = (x, y) => pixelAt(img, x, y);

// reflect a coordinate about a real centre: c -> round(2*centre - 1 - c) keeps it a
// bijection on the integer grid (matches an even-width framebuffer's pixel mirror).
const mirX = x => Math.round(2 * cx - 1 - x);
const mirY = y => Math.round(2 * cy - 1 - y);

const res = mirrorDiff(img, { axis, cx, cy, by0, by1, bx0, bx1 });

if (overlayPath) {
  const S = 4, OW = W * S, OH = (by1 - by0) * S, out = Buffer.alloc(OW * OH * 3);
  const mset = new Set(res.marks.map(([x, y]) => y * W + x));
  for (let y = by0; y < by1; y++) for (let x = 0; x < W; x++) {
    const c = at(x, y); let r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
    if (mset.has(y * W + x)) { r = 255; g = 0; b = 0; }
    for (let sy = 0; sy < S; sy++) for (let sx = 0; sx < S; sx++) {
      const i = (((y - by0) * S + sy) * OW + (x * S + sx)) * 3; out[i] = r; out[i + 1] = g; out[i + 2] = b;
    }
  }
  fs.writeFileSync(overlayPath, encodePNG(OW, OH, out));
}

const control = controlCheck(img, { by0, by1, bx0, bx1 }, res);
const verdict = judge(res, expectArg, control);

if (JSON_OUT) {
  console.log(JSON.stringify({ file: path.relative(ROOT, file), W, H, axis, cx, cy,
    band: [by0, by1], xband: [bx0, bx1], pairs: res.pairs, mismatch: res.mism,
    expect: expectArg, control, ok: verdict.ok }));
} else if (!QUIET || !verdict.ok) {
  const tag = pngArg ? file : `${cart} (${path.relative(ROOT, file)})`;
  console.log(`${tag}: ${W}x${H}, axis=${axis}, centre=(${cx},${cy})`);
  console.log(`mirror mismatch: ${res.mism} of ${res.pairs} compared pixel-pairs`
    + (expectArg === null ? '' : `   (expected ${expectArg})`));
  const rows = Object.entries(res.rowmis).sort((a, b) => b[1] - a[1]).slice(0, 10);
  if (rows.length) console.log('worst rows (y:count):', rows.map(([y, n]) => `${y}:${n}`).join('  '));
  if (overlayPath) console.log('overlay:', overlayPath);
  if (control.length) {
    console.error('\n✗ THIS COMPARISON IS NOT EVIDENCE:');
    for (const c of control) console.error(`    ${c}`);
    console.error('  `node tools/mirror-diff.js --selfcheck` checks the comparison itself.');
  } else if (verdict.msg) console.error(`\n✗ ${verdict.msg}`);
}

process.exit(verdict.ok ? 0 : 1);
