#!/usr/bin/env node
// Usage: node tools/make-cart.js <source.c> <output.cart.png>

const fs   = require('fs')
const path = require('path')
const zlib = require('zlib')

const ROOT_DIR    = path.join(__dirname, '..')
const BUILD_DIR   = path.join(ROOT_DIR, 'build')
const RUNTIME_DIR = path.join(ROOT_DIR, 'runtime')
const RAYLIB      = fs.existsSync('/opt/homebrew/opt/raylib') ? '/opt/homebrew/opt/raylib' : '/usr/local/opt/raylib'
const CART_BIN    = path.join(BUILD_DIR, 'cart')

// ── PNG chunk helpers ─────────────────────────────────────────
function crc32(buf) {
  let crc = 0xFFFFFFFF
  for (let i = 0; i < buf.length; i++) {
    crc ^= buf[i]
    for (let j = 0; j < 8; j++) crc = (crc & 1) ? (0xEDB88320 ^ (crc >>> 1)) : (crc >>> 1)
  }
  return (crc ^ 0xFFFFFFFF) >>> 0
}

function makeChunk(type, data) {
  const typeBuf = Buffer.from(type, 'ascii')
  const lenBuf  = Buffer.allocUnsafe(4); lenBuf.writeUInt32BE(data.length)
  const crcBuf  = Buffer.allocUnsafe(4); crcBuf.writeUInt32BE(crc32(Buffer.concat([typeBuf, data])))
  return Buffer.concat([lenBuf, typeBuf, data, crcBuf])
}

function makeZtxtChunk(keyword, text) {
  const compressed = zlib.deflateSync(Buffer.from(text, 'utf8'))
  return makeChunk('zTXt', Buffer.concat([
    Buffer.from(keyword, 'latin1'), Buffer.from([0, 0]), compressed,
  ]))
}

const PNG_SIG = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10])

function makePng(w, h, drawPixel) {
  const scanlines = Buffer.alloc(h * (1 + w * 3))
  for (let y = 0; y < h; y++) {
    scanlines[y * (1 + w * 3)] = 0  // filter: None
    for (let x = 0; x < w; x++) {
      const [r, g, b] = drawPixel(x, y)
      const i = y * (1 + w * 3) + 1 + x * 3
      scanlines[i] = r; scanlines[i + 1] = g; scanlines[i + 2] = b
    }
  }
  const ihdr = Buffer.alloc(13)
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4)
  ihdr[8] = 8; ihdr[9] = 2  // 8-bit RGB
  return Buffer.concat([
    PNG_SIG,
    makeChunk('IHDR', ihdr),
    makeChunk('IDAT', zlib.deflateSync(scanlines)),
    makeChunk('IEND', Buffer.alloc(0)),
  ])
}

// pico-8 palette
const P = [
  [0x00,0x00,0x00], [0x1d,0x2b,0x53], [0x7e,0x25,0x53], [0x00,0x87,0x51],
  [0xab,0x52,0x36], [0x5f,0x57,0x4f], [0xc2,0xc3,0xc7], [0xff,0xf1,0xe8],
  [0xff,0x00,0x4d], [0xff,0xa3,0x00], [0xff,0xec,0x27], [0x00,0xe4,0x36],
  [0x29,0xad,0xff], [0x83,0x76,0x9c], [0xff,0x77,0xa8], [0xff,0xcc,0xaa],
]

function makePlaceholderPng() {
  return makePng(320, 200, (x, y) => {
    if (y < 8) return P[12]   // blue header bar
    return P[1]               // dark blue background
  })
}

function makeBlankSpritePng() {
  return makePng(128, 128, () => P[0])  // all black
}

// ── sprite sheet builder ──────────────────────────────────────
// sprites: { slotIndex: pixelArray }
// pixelArray: flat 256-element array of palette indices (16×16, row-major)
// OR a multi-line string where each char maps via charMap to a palette index
//
// default charMap covers the most useful pico-8 colors:
const DEFAULT_CHAR_MAP = {
  '.':0, '0':0,
  '1':1,  '2':2,  '3':3,  '4':4,  '5':5,  '6':6,  '7':7,
  '8':8,  '9':9,  'a':10, 'b':11, 'c':12, 'd':13, 'e':14, 'f':15,
  // friendly aliases
  'K':0,  // blacK
  'B':1,  // dark Blue
  'P':2,  // dark Purple
  'G':3,  // dark Green
  'N':4,  // browN
  'S':5,  // dark grey (Slate)
  'L':6,  // Light grey
  'W':7,  // White
  'R':8,  // Red
  'O':9,  // Orange
  'Y':10, // Yellow
  'g':11, // bright Green
  'b':12, // bright Blue
  'I':13, // Indigo
  'k':14, // pinK
  'p':15, // Peach
}

function parseSprite(src, charMap = DEFAULT_CHAR_MAP) {
  if (Array.isArray(src)) return src.slice(0, 256)
  // string: strip leading/trailing blank lines, split into chars
  const rows = src.split('\n').map(l => l.trimEnd()).filter(l => l.length > 0)
  const pixels = []
  for (let y = 0; y < 16; y++) {
    const row = rows[y] || ''
    for (let x = 0; x < 16; x++) {
      pixels.push(charMap[row[x]] ?? 0)
    }
  }
  return pixels
}

function buildSpriteSheet(sprites, charMap) {
  // sprites: { slotIndex: pixelArrayOrString, ... }
  const COLS = 8, SIZE = 16, SHEET = 128
  const pixels = new Array(SHEET * SHEET).fill(0)
  for (const [slot, src] of Object.entries(sprites)) {
    const idx    = parseInt(slot)
    const parsed = parseSprite(src, charMap || DEFAULT_CHAR_MAP)
    const ox     = (idx % COLS) * SIZE
    const oy     = Math.floor(idx / COLS) * SIZE
    for (let py = 0; py < SIZE; py++) {
      for (let px = 0; px < SIZE; px++) {
        pixels[(oy + py) * SHEET + (ox + px)] = parsed[py * SIZE + px] || 0
      }
    }
  }
  const palette = [...P,
    [0x29,0x18,0x14],[0x11,0x1d,0x35],[0x42,0x21,0x36],[0x12,0x53,0x59],
    [0x74,0x2f,0x29],[0x49,0x33,0x3b],[0xa2,0x88,0x79],[0xf3,0xef,0x7d],
    [0xbe,0x12,0x50],[0xff,0x6c,0x24],[0xa8,0xe7,0x2e],[0x00,0xb5,0x43],
    [0x06,0x5a,0xb5],[0x75,0x46,0x65],[0xff,0x6e,0x59],[0xff,0x9d,0x81],
  ]
  return makePng(SHEET, SHEET, (x, y) => palette[pixels[y * SHEET + x]] || [0,0,0])
}

// ── map builder ───────────────────────────────────────────────
// layout: array of strings (ASCII art rows)
// tiles:  { 'char': tileIndex, ... }  default: '#'→1, '.'→0, ' '→0
function buildMap(layout, tiles = {}, mapW = 128, mapH = 64) {
  const tileMap = { '.': 0, ' ': 0, '#': 1, ...tiles }
  const data    = new Uint8Array(mapW * mapH)
  for (let y = 0; y < Math.min(layout.length, mapH); y++) {
    const row = layout[y]
    for (let x = 0; x < Math.min(row.length, mapW); x++) {
      data[y * mapW + x] = tileMap[row[x]] ?? 0
    }
  }
  return data
}

function embedCartChunks(pngBuf, data) {
  const parts = [PNG_SIG]
  let offset = 8, iend = null
  while (offset + 12 <= pngBuf.length) {
    const len  = pngBuf.readUInt32BE(offset)
    const type = pngBuf.slice(offset + 4, offset + 8).toString('ascii')
    const chunk = pngBuf.slice(offset, offset + 12 + len)
    if (type === 'IEND') { iend = chunk; break }
    parts.push(chunk)
    offset += 12 + len
  }
  for (const [key, val] of Object.entries(data)) parts.push(makeZtxtChunk(`de:${key}`, val))
  if (iend) parts.push(iend)
  return Buffer.concat(parts)
}

// ── config loader ─────────────────────────────────────────────
// looks for a .cart.js file alongside the .c source
// exports: { sprites, map, charMap, mapW, mapH }
function loadConfig(srcFile) {
  const cfgFile = srcFile.replace(/\.c$/, '.cart.js')
  if (!fs.existsSync(cfgFile)) return {}
  try {
    return require(path.resolve(cfgFile))
  } catch (e) {
    console.warn('warning: could not load', cfgFile, e.message)
    return {}
  }
}

// ── extract chunks from an existing cart ─────────────────────
function extractCartChunks(pngBuf) {
  const result = {}
  let offset = 8
  while (offset + 12 <= pngBuf.length) {
    const len  = pngBuf.readUInt32BE(offset)
    const type = pngBuf.slice(offset + 4, offset + 8).toString('ascii')
    const data = pngBuf.slice(offset + 8, offset + 8 + len)
    if (type === 'zTXt') {
      const nullIdx = data.indexOf(0)
      if (nullIdx !== -1) {
        const key = data.slice(0, nullIdx).toString('latin1')
        if (key.startsWith('de:')) {
          try { result[key.slice(3)] = zlib.inflateSync(data.slice(nullIdx + 2)).toString('utf8') } catch {}
        }
      }
    }
    offset += 12 + len
    if (type === 'IEND') break
  }
  return result
}

// ── main ──────────────────────────────────────────────────────
const args = process.argv.slice(2)

if (args[0] === '--update') {
  // update the screenshot of an existing cart
  // usage: node make-cart.js --update <cart.png> <screenshot.png>
  const [, cartFile, screenshotFile] = args
  if (!cartFile || !screenshotFile) {
    console.error('usage: node tools/make-cart.js --update <cart.png> <screenshot.png>')
    process.exit(1)
  }
  const chunks = extractCartChunks(fs.readFileSync(cartFile))
  if (!chunks.source) { console.error('not a dreamengine cart'); process.exit(1) }
  const newPng = embedCartChunks(fs.readFileSync(screenshotFile), chunks)
  fs.writeFileSync(cartFile, newPng)
  console.log('updated screenshot in', cartFile)

} else if (args[0] === '--run') {
  // compile + run a cart in screenshot mode, then bake the result back in
  // usage: node tools/make-cart.js --run <cart.png>
  const { execSync, spawnSync } = require('child_process')
  const cartFile = args[1]
  if (!cartFile) { console.error('usage: node tools/make-cart.js --run <cart.png>'); process.exit(1) }

  const chunks = extractCartChunks(fs.readFileSync(cartFile))
  if (!chunks.source) { console.error('not a dreamengine cart'); process.exit(1) }

  fs.mkdirSync(BUILD_DIR, { recursive: true })

  // write source, sprites, map to build/
  fs.writeFileSync(path.join(BUILD_DIR, 'cart.c'), chunks.source)
  const spritesBuf = chunks.sprites
    ? Buffer.from(chunks.sprites.replace(/^data:image\/png;base64,/, ''), 'base64')
    : makeBlankSpritePng()
  fs.writeFileSync(path.join(BUILD_DIR, 'sprites.png'), spritesBuf)
  fs.writeFileSync(path.join(BUILD_DIR, 'map.dat'),
    chunks.map ? Buffer.from(chunks.map, 'base64') : Buffer.alloc(8192))

  // generate headers via xxd
  const xxd = (file) => execSync(`xxd -i ${file}`, { cwd: BUILD_DIR }).toString()
  fs.writeFileSync(path.join(BUILD_DIR, 'sprites_data.h'),
    xxd('sprites.png')
      .replace(/unsigned char sprites_png\[\]/, 'static const unsigned char SPRITES_DATA[]')
      .replace(/unsigned int sprites_png_len/,  'static const unsigned int  SPRITES_DATA_LEN'))
  fs.writeFileSync(path.join(BUILD_DIR, 'map_data.h'),
    xxd('map.dat')
      .replace(/unsigned char map_dat\[\]/, 'static const unsigned char MAP_DATA[]')
      .replace(/unsigned int map_dat_len/,  'static const unsigned int  MAP_DATA_LEN'))

  // bake the thumbnail at the cart's intended config so the screenshot matches
  // how it'll actually run. (SCALE only affects window size, not the captured
  // native-res canvas, so it stays 1 — the screenshot is identical either way.)
  let st = {}
  try { st = JSON.parse(chunks.settings || '{}') } catch {}
  const SW = st.screenW ?? 320, SH = st.screenH ?? 200
  const CW = st.cellW   ?? 16,  CH = st.cellH   ?? 16
  const MW = st.mapW    ?? 128, MH = st.mapH    ?? 64

  // compile
  const studioC = path.join(RUNTIME_DIR, 'studio.c')
  const cartSrc = path.join(BUILD_DIR, 'cart.c')
  const clangArgs = [
    `"${cartSrc}"`, `"${studioC}"`,
    `-I"${RUNTIME_DIR}"`, `-I"${BUILD_DIR}"`, `-I"${RAYLIB}/include"`,
    `-DSCREEN_W=${SW}`, `-DSCREEN_H=${SH}`, '-DSCALE=1',
    `-DMAP_W=${MW}`, `-DMAP_H=${MH}`, `-DCELL_W=${CW}`, `-DCELL_H=${CH}`,
    '-DTOUCH_CONTROLS_DEFAULT=0', '-Os', '-fno-delete-null-pointer-checks',
    `"${RAYLIB}/lib/libraylib.a"`,
    '-framework OpenGL', '-framework Cocoa', '-framework IOKit',
    '-framework CoreVideo', '-framework CoreFoundation',
    `-Wl,-dead_strip`, `-o "${CART_BIN}"`,
  ].join(' ')

  process.stdout.write('compiling... ')
  try {
    execSync(`clang ${clangArgs}`, { stdio: 'pipe' })
    console.log('ok')
  } catch (e) {
    console.error('failed\n' + (e.stderr?.toString() || e.message))
    process.exit(1)
  }

  // run with --screenshot: window opens briefly, 3 frames, exits, saves screenshot.png
  console.log('running (window will flash briefly)...')
  spawnSync(CART_BIN, ['--screenshot'], { cwd: BUILD_DIR, stdio: 'inherit' })

  // bake screenshot into cart
  const screenshotPath = path.join(BUILD_DIR, 'screenshot.png')
  if (!fs.existsSync(screenshotPath)) {
    console.error('screenshot.png not written — did the cart crash?')
    process.exit(1)
  }
  const newPng = embedCartChunks(fs.readFileSync(screenshotPath), chunks)
  fs.writeFileSync(cartFile, newPng)
  console.log('updated', cartFile)

} else {
  // create a new cart from source
  // usage: node make-cart.js <source.c> <output.cart.png>
  const [srcFile, outFile] = args
  if (!srcFile || !outFile) {
    console.error('usage: node tools/make-cart.js <source.c> <output.cart.png>')
    process.exit(1)
  }
  const source     = fs.readFileSync(srcFile, 'utf8')
  const cfg        = loadConfig(srcFile)
  const spritesBuf = cfg.sprites ? buildSpriteSheet(cfg.sprites, cfg.charMap) : makeBlankSpritePng()
  const mapBytes   = cfg.map ? buildMap(cfg.map.layout || cfg.map, cfg.map.tiles, cfg.mapW, cfg.mapH) : new Uint8Array(8192)
  const spritesUrl = 'data:image/png;base64,' + spritesBuf.toString('base64')
  const mapB64     = Buffer.from(mapBytes).toString('base64')
  // the config a cart is meant to run at — travels with the cart so loading it
  // restores the right screen/scale/cell/map dims regardless of editor globals.
  // mapW/mapH default to 128/64 to match buildMap()'s own defaults.
  const cartSettings = {
    screenW: cfg.screenW ?? 320, screenH: cfg.screenH ?? 200, scale: cfg.scale ?? 4,
    cellW:   cfg.cellW   ?? 16,  cellH:   cfg.cellH   ?? 16,
    mapW:    cfg.mapW    ?? 128,  mapH:    cfg.mapH    ?? 64,
  }
  const cartPng    = embedCartChunks(makePlaceholderPng(), { source, sprites: spritesUrl, map: mapB64, settings: JSON.stringify(cartSettings) })
  fs.writeFileSync(outFile, cartPng)
  console.log('wrote', outFile)
}
