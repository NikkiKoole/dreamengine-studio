// make-mac-icon.js — generate the shared mac-app icon (every exported .app gets it).
//
// Draws the icon as 32×32 pixel art with sprite-draw (pink rounded tile, "de" in
// the engine's own dos_8x8 font, plum drop-shadow), then emits mac-app.iconset/
// with every size macOS wants — all crisp nearest-neighbour integer upscales of
// the 32px master (only 16px is a smooth downscale, via sips in the bake step).
//
// Regenerate + rebake the committed .icns after editing the art:
//   node tools/assets/make-mac-icon.js
//   sips -z 16 16 tools/assets/mac-app.iconset/icon_32x32.png \
//        --out tools/assets/mac-app.iconset/icon_16x16.png
//   iconutil -c icns tools/assets/mac-app.iconset -o tools/assets/mac-app.icns
//
// tools/mac-app.sh picks up tools/assets/mac-app.icns automatically (or --icon).
'use strict'
const path = require('path')
const fs   = require('fs')
const zlib = require('zlib')
const SD   = require('../sprite-draw.js')
const { PAL32 } = require('../make-cart.js')
const { blank, rrectfill, print, scale, FONT_DEFAULT } = SD

// ── minimal RGBA PNG encoder (makePng in make-cart.js is RGB-only, no alpha) ──
const CRC_T = (() => { const t = []; for (let n = 0; n < 256; n++) { let c = n
  for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0 } return t })()
function crc32(buf) { let c = 0xFFFFFFFF
  for (const b of buf) c = CRC_T[(c ^ b) & 0xFF] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0 }
function chunk(type, data) {
  const t = Buffer.from(type), len = Buffer.alloc(4), crc = Buffer.alloc(4)
  len.writeUInt32BE(data.length); crc.writeUInt32BE(crc32(Buffer.concat([t, data])))
  return Buffer.concat([len, t, data, crc])
}
function writeRGBA(file, w, h, g) {   // g[y][x] = palette index, 0 = transparent
  const raw = Buffer.alloc(h * (1 + w * 4))
  for (let y = 0; y < h; y++) { raw[y * (1 + w * 4)] = 0
    for (let x = 0; x < w; x++) { const c = g[y]?.[x] ?? 0
      const i = y * (1 + w * 4) + 1 + x * 4
      if (c !== 0) { const [r, gg, b] = PAL32[c]
        raw[i] = r; raw[i + 1] = gg; raw[i + 2] = b; raw[i + 3] = 255 } } }
  const ihdr = Buffer.alloc(13)
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 6
  fs.writeFileSync(file, Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]),
    chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw, { level: 9 })), chunk('IEND', Buffer.alloc(0)),
  ]))
}

// ── the icon: macOS grid on a 32×32 canvas (tile 2..29, pixel corner radius) ──
const PINK = 14, WHITE = 7, PLUM = 2
const g = blank(32, 32)
rrectfill(g, 2, 2, 28, 28, 7, PINK)
print(g, 'de', 9, 13, PLUM, FONT_DEFAULT)   // drop-shadow, then the face
print(g, 'de', 8, 12, WHITE, FONT_DEFAULT)

// ── emit the iconset: every size is an integer upscale of the 32px master ─────
const setDir = path.join(__dirname, 'mac-app.iconset')
fs.mkdirSync(setDir, { recursive: true })
const sizes = [   // [iconset filename, nearest-neighbour factor from 32px]
  ['icon_16x16@2x.png',   1], ['icon_32x32.png',     1],
  ['icon_32x32@2x.png',   2], ['icon_128x128.png',   4],
  ['icon_128x128@2x.png', 8], ['icon_256x256.png',   8],
  ['icon_256x256@2x.png', 16], ['icon_512x512.png',  16],
  ['icon_512x512@2x.png', 32],
]
for (const [name, f] of sizes) writeRGBA(path.join(setDir, name), 32 * f, 32 * f, scale(g, f))
writeRGBA(path.join(__dirname, 'mac-app-icon.png'), 1024, 1024, scale(g, 32))
console.log('wrote mac-app.iconset/ + mac-app-icon.png — now sips the 16px + iconutil (see header)')
