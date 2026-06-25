#!/usr/bin/env node
// fml-assets.js — bake Floorplanner top-down furniture renders into palette
// pixel-art sprites for the floorwalker cart.
//
//   node fmltools/fml-assets.js <file.fml> [options]      bake every furniture refid in the fml
//   node fmltools/fml-assets.js --refids <a,b,c> [options] bake specific refids (quick test)
//
// options:
//   --max <px>        longest side of the output sprite (default 24)
//   --palette <path>  palette JSON ({"palette":[...]} or a bare array of hex / [r,g,b]).
//                     default editor/public/palettes/pico32.json
//   --outline[=N]     add a 1px silhouette outline; N = palette index (default = darkest)
//   --posterize <n>   quantize each RGB channel to n levels BEFORE palette match (default off)
//   --out <dir>       output dir (default build/.fml-assets)
//   --limit <n>       only bake the first n refids (handy while iterating)
//
// Outputs under <out>/:  <refid>.png (baked sprite), manifest.json, preview.png (contact sheet).
// Raw CDN downloads are cached under <out>/cache/ so re-runs don't refetch.

const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const https = require('https');

const RENDER_BASE = 'https://fp-media-img-cdn.floorplanner.com/cdb/renders';

// ---------- args ----------
const argv = process.argv.slice(2);
const opt = { in: null, refids: null, max: 24, palette: 'editor/public/palettes/pico32.json',
              outline: false, outlineIdx: null, posterize: 0, saturate: 1, out: 'build/.fml-assets', limit: 0 };
for (let i = 0; i < argv.length; i++) {
  const a = argv[i];
  if (a === '--refids') opt.refids = argv[++i].split(',').map(s => s.trim()).filter(Boolean);
  else if (a === '--max') opt.max = parseInt(argv[++i], 10);
  else if (a === '--palette') opt.palette = argv[++i];
  else if (a === '--posterize') opt.posterize = parseInt(argv[++i], 10);
  else if (a === '--saturate') opt.saturate = parseFloat(argv[++i]);
  else if (a === '--out') opt.out = argv[++i];
  else if (a === '--limit') opt.limit = parseInt(argv[++i], 10);
  else if (a === '--outline' || a === '-o') opt.outline = true;
  else if (a.startsWith('--outline=')) { opt.outline = true; opt.outlineIdx = parseInt(a.slice(10), 10); }
  else if (!a.startsWith('-')) opt.in = a;
  else { console.error('unknown arg', a); process.exit(1); }
}
if (!opt.in && !opt.refids) {
  console.error('usage: node fmltools/fml-assets.js <file.fml> [--max N] [--palette p] [--outline[=N]] [--posterize N] [--limit N]');
  process.exit(1);
}

// ---------- PNG decode (color types 2 RGB, 3 palette, 6 RGBA; 8-bit; no interlace) ----------
function decodePNG(buf) {
  if (buf.readUInt32BE(0) !== 0x89504e47) throw new Error('not a PNG');
  let pos = 8, w = 0, h = 0, bitDepth = 0, colorType = 0, interlace = 0;
  let palette = null, trns = null;
  const idat = [];
  while (pos < buf.length) {
    const len = buf.readUInt32BE(pos); const type = buf.toString('ascii', pos + 4, pos + 8);
    const data = buf.subarray(pos + 8, pos + 8 + len);
    if (type === 'IHDR') {
      w = data.readUInt32BE(0); h = data.readUInt32BE(4);
      bitDepth = data[8]; colorType = data[9]; interlace = data[12];
    } else if (type === 'PLTE') palette = data;
    else if (type === 'tRNS') trns = data;
    else if (type === 'IDAT') idat.push(data);
    else if (type === 'IEND') break;
    pos += 12 + len;
  }
  if (bitDepth !== 8) throw new Error('unsupported bit depth ' + bitDepth);
  if (interlace) throw new Error('interlaced PNG unsupported');
  const channels = colorType === 2 ? 3 : colorType === 6 ? 4 : colorType === 3 ? 1 : colorType === 4 ? 2 : 1;
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const stride = w * channels;
  const out = Buffer.alloc(h * stride);
  let rp = 0;
  const paeth = (a, b, c) => { const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c); return pa <= pb && pa <= pc ? a : pb <= pc ? b : c; };
  for (let y = 0; y < h; y++) {
    const filter = raw[rp++];
    for (let x = 0; x < stride; x++) {
      const v = raw[rp++];
      const a = x >= channels ? out[y * stride + x - channels] : 0;
      const b = y > 0 ? out[(y - 1) * stride + x] : 0;
      const c = (x >= channels && y > 0) ? out[(y - 1) * stride + x - channels] : 0;
      let r;
      switch (filter) {
        case 0: r = v; break;
        case 1: r = v + a; break;
        case 2: r = v + b; break;
        case 3: r = v + ((a + b) >> 1); break;
        case 4: r = v + paeth(a, b, c); break;
        default: throw new Error('bad filter ' + filter);
      }
      out[y * stride + x] = r & 0xff;
    }
  }
  // expand to RGBA
  const rgba = new Uint8Array(w * h * 4);
  for (let i = 0; i < w * h; i++) {
    let R, G, B, A = 255;
    if (colorType === 6) { R = out[i * 4]; G = out[i * 4 + 1]; B = out[i * 4 + 2]; A = out[i * 4 + 3]; }
    else if (colorType === 2) { R = out[i * 3]; G = out[i * 3 + 1]; B = out[i * 3 + 2]; }
    else if (colorType === 3) { const idx = out[i]; R = palette[idx * 3]; G = palette[idx * 3 + 1]; B = palette[idx * 3 + 2]; if (trns && idx < trns.length) A = trns[idx]; }
    else if (colorType === 4) { R = G = B = out[i * 2]; A = out[i * 2 + 1]; }
    else { R = G = B = out[i]; }
    rgba[i * 4] = R; rgba[i * 4 + 1] = G; rgba[i * 4 + 2] = B; rgba[i * 4 + 3] = A;
  }
  return { w, h, rgba };
}

// ---------- PNG encode (RGBA, color type 6) ----------
function encodePNG(w, h, rgba) {
  const stride = w * 4;
  const raw = Buffer.alloc(h * (stride + 1));
  for (let y = 0; y < h; y++) { raw[y * (stride + 1)] = 0; rgba.subarray ? raw.set(rgba.subarray(y * stride, y * stride + stride), y * (stride + 1) + 1) : null; if (!rgba.subarray) for (let x = 0; x < stride; x++) raw[y * (stride + 1) + 1 + x] = rgba[y * stride + x]; }
  const idat = zlib.deflateSync(raw, { level: 9 });
  const chunks = [];
  const chunk = (type, data) => {
    const len = Buffer.alloc(4); len.writeUInt32BE(data.length, 0);
    const t = Buffer.from(type, 'ascii');
    const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(Buffer.concat([t, data])) >>> 0, 0);
    chunks.push(len, t, data, crc);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
  chunk('IHDR', ihdr); chunk('IDAT', idat); chunk('IEND', Buffer.alloc(0));
  return Buffer.concat([Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]), ...chunks]);
}
const CRC_TABLE = (() => { const t = new Int32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c; } return t; })();
function crc32(buf) { let c = -1; for (let i = 0; i < buf.length; i++) c = CRC_TABLE[(c ^ buf[i]) & 0xff] ^ (c >>> 8); return c ^ -1; }

// ---------- image ops ----------
function downscale(img, maxPx) {
  const scale = Math.min(1, maxPx / Math.max(img.w, img.h));
  const dw = Math.max(1, Math.round(img.w * scale)), dh = Math.max(1, Math.round(img.h * scale));
  const out = new Uint8Array(dw * dh * 4);
  for (let y = 0; y < dh; y++) for (let x = 0; x < dw; x++) {
    const sx0 = Math.floor(x / dw * img.w), sx1 = Math.max(sx0 + 1, Math.floor((x + 1) / dw * img.w));
    const sy0 = Math.floor(y / dh * img.h), sy1 = Math.max(sy0 + 1, Math.floor((y + 1) / dh * img.h));
    let r = 0, g = 0, b = 0, a = 0, aw = 0, n = 0;
    for (let sy = sy0; sy < sy1; sy++) for (let sx = sx0; sx < sx1; sx++) {
      const i = (sy * img.w + sx) * 4, al = img.rgba[i + 3];
      r += img.rgba[i] * al; g += img.rgba[i + 1] * al; b += img.rgba[i + 2] * al; // premultiplied avg (no dark fringe)
      a += al; aw += al; n++;
    }
    const o = (y * dw + x) * 4;
    out[o] = aw ? r / aw : 0; out[o + 1] = aw ? g / aw : 0; out[o + 2] = aw ? b / aw : 0; out[o + 3] = a / n;
  }
  return { w: dw, h: dh, rgba: out };
}

// boost chroma so muted renders map to colourful palette entries instead of all
// collapsing to the one neutral grey/lavender. f=1 no-op; f>1 more saturated.
function saturate(img, f) {
  if (!f || f === 1) return img;
  for (let i = 0; i < img.rgba.length; i += 4) {
    const r = img.rgba[i], g = img.rgba[i + 1], b = img.rgba[i + 2];
    const luma = 0.3 * r + 0.59 * g + 0.11 * b;
    img.rgba[i]     = Math.max(0, Math.min(255, luma + (r - luma) * f));
    img.rgba[i + 1] = Math.max(0, Math.min(255, luma + (g - luma) * f));
    img.rgba[i + 2] = Math.max(0, Math.min(255, luma + (b - luma) * f));
  }
  return img;
}

function posterize(img, levels) {
  if (!levels || levels < 2) return img;
  const step = 255 / (levels - 1);
  for (let i = 0; i < img.rgba.length; i += 4)
    for (let c = 0; c < 3; c++) img.rgba[i + c] = Math.round(Math.round(img.rgba[i + c] / step) * step);
  return img;
}

// quantize -> palette indices (-1 = transparent for alpha < 128)
function quantize(img, palette) {
  const idx = new Int16Array(img.w * img.h);
  for (let i = 0; i < img.w * img.h; i++) {
    if (img.rgba[i * 4 + 3] < 128) { idx[i] = -1; continue; }
    const r = img.rgba[i * 4], g = img.rgba[i * 4 + 1], b = img.rgba[i * 4 + 2];
    let best = 0, bd = 1e18;
    for (let p = 0; p < palette.length; p++) {
      const dr = r - palette[p][0], dg = g - palette[p][1], db = b - palette[p][2];
      const d = 0.3 * dr * dr + 0.59 * dg * dg + 0.11 * db * db; // luma-weighted
      if (d < bd) { bd = d; best = p; }
    }
    idx[i] = best;
  }
  return idx;
}

function addOutline(idx, w, h, colorIdx) {
  const out = Int16Array.from(idx);
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
    if (idx[y * w + x] !== -1) continue;
    let near = false;
    for (let dy = -1; dy <= 1 && !near; dy++) for (let dx = -1; dx <= 1; dx++) {
      const nx = x + dx, ny = y + dy;
      if (nx >= 0 && nx < w && ny >= 0 && ny < h && idx[ny * w + nx] >= 0) { near = true; break; }
    }
    if (near) out[y * w + x] = colorIdx;
  }
  return out;
}

function idxToRGBA(idx, w, h, palette) {
  const rgba = new Uint8Array(w * h * 4);
  for (let i = 0; i < w * h; i++) {
    if (idx[i] < 0) continue;
    const c = palette[idx[i]];
    rgba[i * 4] = c[0]; rgba[i * 4 + 1] = c[1]; rgba[i * 4 + 2] = c[2]; rgba[i * 4 + 3] = 255;
  }
  return rgba;
}

// ---------- palette ----------
function loadPalette(p) {
  const j = JSON.parse(fs.readFileSync(p, 'utf8'));
  const arr = Array.isArray(j) ? j : j.palette;
  return arr.map(e => {
    if (Array.isArray(e)) return e;
    const h = e.replace('#', '');
    return [parseInt(h.slice(0, 2), 16), parseInt(h.slice(2, 4), 16), parseInt(h.slice(4, 6), 16)];
  });
}

// ---------- fetch ----------
function get(url) {
  return new Promise((resolve) => {
    https.get(url, { timeout: 15000 }, (res) => {
      if (res.statusCode !== 200) { res.resume(); return resolve(null); }
      const bufs = []; res.on('data', d => bufs.push(d)); res.on('end', () => resolve(Buffer.concat(bufs)));
    }).on('error', () => resolve(null)).on('timeout', function () { this.destroy(); resolve(null); });
  });
}
async function fetchRender(refid, cacheDir) {
  const cached = path.join(cacheDir, refid + '.png');
  if (fs.existsSync(cached)) return fs.readFileSync(cached);
  for (const nn of ['01', '00', '02', '03']) {
    const url = `${RENDER_BASE}/${refid.slice(0, 2)}/${refid}_${nn}.top.x3.png`;
    const buf = await get(url);
    if (buf && buf.length > 8 && buf.readUInt32BE(0) === 0x89504e47) { fs.writeFileSync(cached, buf); return buf; }
  }
  return null;
}

// ---------- collect refids ----------
function refidsFromFml(file) {
  const d = JSON.parse(fs.readFileSync(file, 'utf8'));
  const set = new Set();
  const hex = (r) => /^[0-9a-fx]{40}$/.test(r || '');   // refids may contain 'x' (part of the id, not a placeholder)
  for (const fl of (d.floors || [])) for (const de of (fl.designs || [])) {
    for (const it of (de.items || [])) if (hex(it.refid)) set.add(it.refid);
    for (const wl of (de.walls || [])) for (const o of (wl.openings || [])) if (hex(o.refid)) set.add(o.refid); // doors + windows
  }
  return [...set];
}

// ---------- preview contact sheet ----------
function buildPreview(sprites, palette) {
  const scale = 6, pad = 6, cols = Math.ceil(Math.sqrt(sprites.length)) || 1;
  const cell = opt.max * scale + pad * 2;
  const rows = Math.ceil(sprites.length / cols);
  const W = cols * cell, H = rows * cell;
  const rgba = new Uint8Array(W * H * 4);
  for (let i = 0; i < W * H; i++) { const x = i % W, y = (i / W) | 0; const ch = ((x >> 2) + (y >> 2)) & 1 ? 40 : 28; rgba[i * 4] = ch; rgba[i * 4 + 1] = ch; rgba[i * 4 + 2] = ch + 6; rgba[i * 4 + 3] = 255; }
  sprites.forEach((s, k) => {
    const cx = (k % cols) * cell + pad, cy = ((k / cols) | 0) * cell + pad;
    const ox = cx + ((opt.max - s.w) * scale >> 1), oy = cy + ((opt.max - s.h) * scale >> 1);
    for (let y = 0; y < s.h; y++) for (let x = 0; x < s.w; x++) {
      const id = s.idx[y * s.w + x]; if (id < 0) continue;
      const c = palette[id];
      for (let py = 0; py < scale; py++) for (let px = 0; px < scale; px++) {
        const X = ox + x * scale + px, Y = oy + y * scale + py;
        if (X < 0 || X >= W || Y < 0 || Y >= H) continue;
        const o = (Y * W + X) * 4; rgba[o] = c[0]; rgba[o + 1] = c[1]; rgba[o + 2] = c[2]; rgba[o + 3] = 255;
      }
    }
  });
  return encodePNG(W, H, rgba);
}

// ---------- main ----------
(async () => {
  const palette = loadPalette(opt.palette);
  const outlineIdx = opt.outlineIdx != null ? opt.outlineIdx
    : palette.map((c, i) => [c[0] + c[1] + c[2], i]).sort((a, b) => a[0] - b[0])[0][1]; // darkest
  fs.mkdirSync(opt.out, { recursive: true });
  const cacheDir = path.join(opt.out, 'cache'); fs.mkdirSync(cacheDir, { recursive: true });

  let refids = opt.refids || refidsFromFml(opt.in);
  if (opt.limit) refids = refids.slice(0, opt.limit);
  console.log(`baking ${refids.length} refids  (max ${opt.max}px, palette ${path.basename(opt.palette)}${opt.outline ? `, outline=${outlineIdx}` : ''}${opt.posterize ? `, posterize ${opt.posterize}` : ''})`);

  const manifest = { palette: opt.palette, palette_rgb: palette, max: opt.max, items: {} };
  const sprites = [];
  let ok = 0, fail = 0;
  const CONC = 4;
  let next = 0;
  async function worker() {
    while (next < refids.length) {
      const refid = refids[next++];
      const buf = await fetchRender(refid, cacheDir);
      if (!buf) { fail++; process.stdout.write('x'); continue; }
      try {
        let img = decodePNG(buf);
        img = downscale(img, opt.max);
        img = saturate(img, opt.saturate);
        img = posterize(img, opt.posterize);
        let idx = quantize(img, palette);
        if (opt.outline) idx = addOutline(idx, img.w, img.h, outlineIdx);
        fs.writeFileSync(path.join(opt.out, refid + '.png'), encodePNG(img.w, img.h, idxToRGBA(idx, img.w, img.h, palette)));
        manifest.items[refid] = { w: img.w, h: img.h, idx: Array.from(idx) };
        sprites.push({ refid, w: img.w, h: img.h, idx });
        ok++; process.stdout.write('.');
      } catch (e) { fail++; process.stdout.write('!'); manifest.items[refid] = { error: e.message }; }
    }
  }
  await Promise.all(Array.from({ length: CONC }, worker));
  process.stdout.write('\n');

  fs.writeFileSync(path.join(opt.out, 'manifest.json'), JSON.stringify(manifest));
  if (sprites.length) fs.writeFileSync(path.join(opt.out, 'preview.png'), buildPreview(sprites, palette));
  console.log(`done: ${ok} baked, ${fail} failed -> ${opt.out}/  (manifest.json, preview.png)`);
})();
