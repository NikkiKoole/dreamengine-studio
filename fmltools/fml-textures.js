#!/usr/bin/env node
// fml-textures.js — resolve & bake Floorplanner FLOOR-COVERING textures into a
// `floorplan` data file's textures[] (and set surfaces[].tex / .tile).
//
//   node fmltools/fml-textures.js --json data/floorplan/<id>.json
//                                 [--out build/.fml-textures] [--tile N] [--saturate f] [--posterize n]
//
// Surfaces carry a Roomstyler material id (`rs-####`). Resolve it (no auth):
//   1. POST the bare id to https://search.floorplanner.com/materials/ids
//   2. read _source.texture (+ _source.color, _source.width)
//   3. fetch https://fp-media-img-cdn.floorplanner.com/cdb/textures/floor_and_wall/original/<texture>
// then JPEG -> (sips) PNG -> downscale -> quantise to the palette, and tile it across the
// surface polygon at runtime. tile px is derived from the material's real cm size, the
// surface's sx scale %, and the plan's cm/px so the pattern is roughly true-to-scale.
// Run after fml2cart.js --json. PNG decode/quantise mirror fml-assets.js.

const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const https = require('https');
const { execFileSync } = require('child_process');

const argv = process.argv.slice(2);
const opt = { json: null, out: 'build/.fml-textures', tile: 16, saturate: 1.4, posterize: 0,
              palette: 'editor/public/palettes/pico32.json' };
for (let i = 0; i < argv.length; i++) {
  const a = argv[i];
  if (a === '--json') opt.json = argv[++i];
  else if (a === '--out') opt.out = argv[++i];
  else if (a === '--tile') opt.tile = parseInt(argv[++i], 10);
  else if (a === '--saturate') opt.saturate = parseFloat(argv[++i]);
  else if (a === '--posterize') opt.posterize = parseInt(argv[++i], 10);
  else if (a === '--palette') opt.palette = argv[++i];
  else { console.error('fml-textures: unknown arg', a); process.exit(1); }
}
if (!opt.json) { console.error('usage: node fmltools/fml-textures.js --json <data.json> [--out dir] [--tile N]'); process.exit(1); }

const SEARCH = 'https://search.floorplanner.com/materials/ids';
const TEX_BASE = 'https://fp-media-img-cdn.floorplanner.com/cdb/textures/floor_and_wall/original';

// ---------- palette ----------
function loadPalette(p) {
  const j = JSON.parse(fs.readFileSync(p, 'utf8'));
  const arr = j.palette || j;
  return arr.map(e => typeof e === 'string'
    ? [parseInt(e.slice(1, 3), 16), parseInt(e.slice(3, 5), 16), parseInt(e.slice(5, 7), 16)] : e);
}

// ---------- PNG decode + image ops (mirror of fml-assets.js) ----------
function decodePNG(buf) {
  if (buf.readUInt32BE(0) !== 0x89504e47) throw new Error('not a PNG');
  let pos = 8, w = 0, h = 0, bitDepth = 0, colorType = 0, interlace = 0, palette = null, trns = null;
  const idat = [];
  while (pos < buf.length) {
    const len = buf.readUInt32BE(pos), type = buf.toString('ascii', pos + 4, pos + 8);
    const data = buf.subarray(pos + 8, pos + 8 + len);
    if (type === 'IHDR') { w = data.readUInt32BE(0); h = data.readUInt32BE(4); bitDepth = data[8]; colorType = data[9]; interlace = data[12]; }
    else if (type === 'PLTE') palette = data;
    else if (type === 'tRNS') trns = data;
    else if (type === 'IDAT') idat.push(data);
    else if (type === 'IEND') break;
    pos += 12 + len;
  }
  if (bitDepth !== 8) throw new Error('unsupported bit depth ' + bitDepth);
  if (interlace) throw new Error('interlaced PNG unsupported');
  const channels = colorType === 2 ? 3 : colorType === 6 ? 4 : colorType === 3 ? 1 : colorType === 4 ? 2 : 1;
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const stride = w * channels, out = Buffer.alloc(h * stride);
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
      switch (filter) { case 0: r = v; break; case 1: r = v + a; break; case 2: r = v + b; break; case 3: r = v + ((a + b) >> 1); break; case 4: r = v + paeth(a, b, c); break; default: throw new Error('bad filter ' + filter); }
      out[y * stride + x] = r & 0xff;
    }
  }
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
function downscale(img, maxPx) {
  const scale = Math.min(1, maxPx / Math.max(img.w, img.h));
  const dw = Math.max(1, Math.round(img.w * scale)), dh = Math.max(1, Math.round(img.h * scale));
  const out = new Uint8Array(dw * dh * 4);
  for (let y = 0; y < dh; y++) for (let x = 0; x < dw; x++) {
    const sx0 = Math.floor(x / dw * img.w), sx1 = Math.max(sx0 + 1, Math.floor((x + 1) / dw * img.w));
    const sy0 = Math.floor(y / dh * img.h), sy1 = Math.max(sy0 + 1, Math.floor((y + 1) / dh * img.h));
    let r = 0, g = 0, b = 0, n = 0;
    for (let sy = sy0; sy < sy1; sy++) for (let sx = sx0; sx < sx1; sx++) { const i = (sy * img.w + sx) * 4; r += img.rgba[i]; g += img.rgba[i + 1]; b += img.rgba[i + 2]; n++; }
    const o = (y * dw + x) * 4; out[o] = r / n; out[o + 1] = g / n; out[o + 2] = b / n; out[o + 3] = 255;
  }
  return { w: dw, h: dh, rgba: out };
}
function saturate(img, f) {
  if (!f || f === 1) return img;
  for (let i = 0; i < img.rgba.length; i += 4) {
    const r = img.rgba[i], g = img.rgba[i + 1], b = img.rgba[i + 2], luma = 0.3 * r + 0.59 * g + 0.11 * b;
    img.rgba[i] = Math.max(0, Math.min(255, luma + (r - luma) * f));
    img.rgba[i + 1] = Math.max(0, Math.min(255, luma + (g - luma) * f));
    img.rgba[i + 2] = Math.max(0, Math.min(255, luma + (b - luma) * f));
  }
  return img;
}
function posterize(img, levels) {
  if (!levels || levels < 2) return img;
  const step = 255 / (levels - 1);
  for (let i = 0; i < img.rgba.length; i += 4) for (let c = 0; c < 3; c++) img.rgba[i + c] = Math.round(Math.round(img.rgba[i + c] / step) * step);
  return img;
}
function quantize(img, palette) {
  const idx = new Int16Array(img.w * img.h);
  for (let i = 0; i < img.w * img.h; i++) {
    const r = img.rgba[i * 4], g = img.rgba[i * 4 + 1], b = img.rgba[i * 4 + 2];
    let best = 0, bd = 1e18;
    for (let p = 0; p < palette.length; p++) { const dr = r - palette[p][0], dg = g - palette[p][1], db = b - palette[p][2]; const d = 0.3 * dr * dr + 0.59 * dg * dg + 0.11 * db * db; if (d < bd) { bd = d; best = p; } }
    idx[i] = best;
  }
  return idx;
}

// ---------- net ----------
function postJSON(url, body) {
  const u = new URL(url), payload = JSON.stringify(body);
  return new Promise((resolve, reject) => {
    const req = https.request({ hostname: u.hostname, path: u.pathname, method: 'POST',
      headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(payload) }, timeout: 20000 }, (r) => {
      const b = []; r.on('data', d => b.push(d)); r.on('end', () => { try { resolve(JSON.parse(Buffer.concat(b).toString('utf8'))); } catch (e) { reject(e); } });
    });
    req.on('error', reject); req.on('timeout', () => { req.destroy(); reject(new Error('timeout')); });
    req.write(payload); req.end();
  });
}
function get(url) {
  return new Promise((resolve) => {
    https.get(url, { timeout: 20000 }, (r) => {
      if (r.statusCode !== 200) { r.resume(); return resolve(null); }
      const b = []; r.on('data', d => b.push(d)); r.on('end', () => resolve(Buffer.concat(b)));
    }).on('error', () => resolve(null)).on('timeout', function () { this.destroy(); resolve(null); });
  });
}

(async () => {
  const palette = loadPalette(opt.palette);
  const dpath = path.resolve(opt.json);
  const data = JSON.parse(fs.readFileSync(dpath, 'utf8'));
  const surfs = data.surfaces || [];
  const refs = [...new Set(surfs.map(s => s.ref).filter(r => /^rs-/.test(r || '')))];
  if (!refs.length) { console.log('fml-textures: no rs-#### floor materials in', opt.json, '— nothing to bake'); data.textures = []; fs.writeFileSync(dpath, JSON.stringify(data)); return; }

  fs.mkdirSync(opt.out, { recursive: true });
  const ids = refs.map(r => r.replace(/^rs-/, ''));
  let resp;
  try { resp = await postJSON(SEARCH, { ids }); }
  catch (e) { console.error('fml-textures: material lookup failed:', e.message); process.exit(1); }
  const byId = {};
  for (const h of (resp.hits?.hits || [])) byId[h._id] = h._source;

  const textures = [], texMeta = [], refToTex = {};
  let ok = 0;
  for (const ref of refs) {
    const id = ref.replace(/^rs-/, ''), m = byId[id];
    if (!m || !m.texture) { console.warn(`  ${ref}: no material/texture`); continue; }
    const jpg = await get(`${TEX_BASE}/${m.texture}`);
    if (!jpg) { console.warn(`  ${ref}: fetch failed (${m.texture})`); continue; }
    const stem = path.join(opt.out, id);
    fs.writeFileSync(stem + '.jpg', jpg);
    try { execFileSync('sips', ['-s', 'format', 'png', stem + '.jpg', '--out', stem + '.png'], { stdio: 'ignore' }); }
    catch (e) { console.warn(`  ${ref}: sips decode failed`); continue; }
    let img = decodePNG(fs.readFileSync(stem + '.png'));
    img = downscale(img, opt.tile);
    saturate(img, opt.saturate); posterize(img, opt.posterize);
    const idx = quantize(img, palette);
    refToTex[ref] = textures.length;
    textures.push({ w: img.w, h: img.h, px: Array.from(idx, v => (v < 0 ? 0 : v)) });
    texMeta.push({ wcm: m.width || 100, name: m.name || m.texture });
    ok++;
  }

  // assign each surface its texture index + on-screen tile size (cm -> px, honouring sx)
  const scale = data.scale || 8;
  for (const s of surfs) {
    const ti = refToTex[s.ref];
    if (ti == null) { s.tex = -1; s.tile = 0; continue; }
    s.tex = ti;
    s.tile = Math.max(4, Math.round((texMeta[ti].wcm * ((s.sx || 100) / 100)) / scale));
  }
  data.textures = textures;
  fs.writeFileSync(dpath, JSON.stringify(data));
  const px = textures.reduce((a, t) => a + t.px.length, 0);
  console.log(`fml-textures: ${ok}/${refs.length} materials baked (${textures.map((t, i) => texMeta[i].name).join(', ')}), ${px}px\n  -> wrote ${opt.json}`);
})();
