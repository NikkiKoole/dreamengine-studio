// tools/fixtures/squishy-features/make-grid.js — the FIXTURE GENERATOR for
// `squishy-features.js --selfcheck`. Not a test itself: it synthesizes matrix-grid PNGs with
// KNOWN per-cell pixel differences, so the checker can be judged without compiling and running
// the squishy cart (which would make the answer depend on the very render being audited).
//
// A generator rather than committed .png blobs on purpose: the interesting thing about these
// fixtures is *which cells differ by how much and under which PNG scanline filter*, and that is
// readable here and editable. A binary blob in git is neither.
//
// Layout constants MUST match squishy-features.js, which in turn must match draw_matrix() in
// tools/carts/squishy.c. They are duplicated rather than imported because squishy-features.js
// runs its whole report at module scope — requiring it would execute the tool.
const zlib = require('zlib')

const SCREEN = 320, MTX_LW = 40, MTX_HH = 12
const NTOOLS = 14, COLS = 8                      // 7 features + the baseline column
const cw = Math.floor((SCREEN - MTX_LW) / COLS)  // 35
const ch = Math.floor((SCREEN - MTX_HH) / NTOOLS) // 22

// cellDiff() scans an inset window: dy 2..ch-3, dx 2..cw-3. That is the only region a painted
// pixel can be COUNTED in, and its size is the per-cell ceiling.
const INSET = 2
const WIN_H = ch - 2 * INSET - 0, WIN_W = cw - 2 * INSET - 0
const CELL_CAP = (ch - 4) * (cw - 4)             // 18 * 31 = 558

const BG   = [40, 40, 48, 255]
const DIFF = [220, 40, 40, 255]

// ── a minimal PNG encoder that emits a CHOSEN scanline filter -----------------------------
// The point of the filter parameter: squishy-features.js hand-rolls a PNG decoder implementing
// all five filter types including Paeth. A wrong reconstruction there corrupts every cell diff
// SILENTLY — the report still prints a tidy table of plausible numbers. Encoding one logical
// image under each filter and demanding identical diffs is what pins that.
function crc32(buf) {
  let c, crc = 0xffffffff
  for (let i = 0; i < buf.length; i++) {
    c = (crc ^ buf[i]) & 0xff
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1
    crc = c ^ (crc >>> 8)
  }
  return (crc ^ 0xffffffff) >>> 0
}
function chunk(type, data) {
  const len = Buffer.alloc(4); len.writeUInt32BE(data.length)
  const td = Buffer.concat([Buffer.from(type, 'latin1'), data])
  const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(td))
  return Buffer.concat([len, td, crc])
}
const paeth = (a, b, c) => {
  const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c)
  return pa <= pb && pa <= pc ? a : pb <= pc ? b : c
}

function encodePNG(px, W, H, filter) {
  const CHN = 4, stride = W * CHN
  const out = []
  for (let y = 0; y < H; y++) {
    const row = px.slice(y * stride, (y + 1) * stride)
    const prev = y > 0 ? px.slice((y - 1) * stride, y * stride) : Buffer.alloc(stride)
    const enc = Buffer.alloc(stride)
    for (let x = 0; x < stride; x++) {
      const a  = x >= CHN ? row[x - CHN] : 0            // left
      const b  = prev[x]                                 // up
      const c  = x >= CHN ? prev[x - CHN] : 0            // upper-left
      let v
      switch (filter) {
        case 1: v = row[x] - a; break
        case 2: v = row[x] - b; break
        case 3: v = row[x] - ((a + b) >> 1); break
        case 4: v = row[x] - paeth(a, b, c); break
        default: v = row[x]
      }
      enc[x] = v & 255
    }
    out.push(Buffer.from([filter]), enc)
  }
  const ihdr = Buffer.alloc(13)
  ihdr.writeUInt32BE(W, 0); ihdr.writeUInt32BE(H, 4)
  ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0   // 8-bit RGBA
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk('IHDR', ihdr),
    chunk('IDAT', zlib.deflateSync(Buffer.concat(out))),
    chunk('IEND', Buffer.alloc(0)),
  ])
}

// ── the grid --------------------------------------------------------------------------------
// diffs: { [brushRow]: { [featureCol 1..7]: nPixels } }  — how many pixels of that cell to paint
//        a different colour from the row's baseline cell (col 0), INSIDE the counted window.
// outside: { [brushRow]: [featureCol, …] } — paint a differing pixel OUTSIDE the inset window,
//        which must NOT be counted (the geometry guard).
// alphaOnly: { [brushRow]: [featureCol, …] } — differ in the ALPHA channel only, which must NOT
//        be counted (cellDiff compares RGB and ignores alpha).
// bottom: { [brushRow]: { [featureCol]: nPixels } } — paint from the LAST inset row upward
//        instead of the first. This is the CELL-ORIGIN guard, and it has to be its own option:
//        a cell is 22px tall but the header offset is only 12px, so a window shifted by the
//        header still overlaps the top of the cell and a diff painted there is found anyway.
//        Painted against the bottom edge, a shifted window misses it entirely.
// `size` forces a non-320 canvas, for the layout-mismatch guard: squishy-features refuses a dump
// whose dimensions do not match the cart's screen, because every cell offset would be wrong and
// the diffs meaningless rather than merely off.
function makeGrid({ diffs = {}, outside = {}, alphaOnly = {}, bottom = {}, filter = 0, size = SCREEN } = {}) {
  const W = size, H = size, CHN = 4
  const px = Buffer.alloc(W * H * CHN)
  for (let i = 0; i < W * H; i++) {
    px[i * 4] = BG[0]; px[i * 4 + 1] = BG[1]; px[i * 4 + 2] = BG[2]; px[i * 4 + 3] = BG[3]
  }
  const set = (x, y, col) => {
    if (x < 0 || y < 0 || x >= W || y >= H) return
    const i = (y * W + x) * CHN
    px[i] = col[0]; px[i + 1] = col[1]; px[i + 2] = col[2]; px[i + 3] = col[3]
  }
  for (const [rowS, cols] of Object.entries(diffs)) {
    const bi = +rowS
    for (const [colS, nS] of Object.entries(cols)) {
      const c = +colS
      let n = +nS
      const y0 = MTX_HH + bi * ch, x0 = MTX_LW + c * cw
      for (let dy = INSET; dy < ch - INSET && n > 0; dy++)
        for (let dx = INSET; dx < cw - INSET && n > 0; dx++, n--)
          set(x0 + dx, y0 + dy, DIFF)
    }
  }
  for (const [rowS, cols] of Object.entries(bottom)) {
    const bi = +rowS
    for (const [colS, nS] of Object.entries(cols)) {
      const c = +colS
      let n = +nS
      const y0 = MTX_HH + bi * ch, x0 = MTX_LW + c * cw
      for (let dy = ch - INSET - 1; dy >= INSET && n > 0; dy--)
        for (let dx = INSET; dx < cw - INSET && n > 0; dx++, n--)
          set(x0 + dx, y0 + dy, DIFF)
    }
  }
  for (const [rowS, cols] of Object.entries(outside)) {
    const bi = +rowS
    for (const c of cols) {
      const y0 = MTX_HH + bi * ch, x0 = MTX_LW + c * cw
      // the four ring positions the inset must exclude
      set(x0 + 0, y0 + 0, DIFF); set(x0 + 1, y0 + 1, DIFF)
      set(x0 + cw - 1, y0 + ch - 1, DIFF); set(x0 + cw - 2, y0 + ch - 2, DIFF)
    }
  }
  for (const [rowS, cols] of Object.entries(alphaOnly)) {
    const bi = +rowS
    for (const c of cols) {
      const y0 = MTX_HH + bi * ch, x0 = MTX_LW + c * cw
      let n = 200
      for (let dy = INSET; dy < ch - INSET && n > 0; dy++)
        for (let dx = INSET; dx < cw - INSET && n > 0; dx++, n--)
          set(x0 + dx, y0 + dy, [BG[0], BG[1], BG[2], 17])   // same RGB, different A
    }
  }
  return encodePNG(px, W, H, filter)
}

module.exports = { makeGrid, SCREEN, MTX_LW, MTX_HH, cw, ch, NTOOLS, COLS, CELL_CAP, INSET }
