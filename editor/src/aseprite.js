// Minimal Aseprite (.ase / .aseprite) reader — decodes the FIRST frame to a
// flattened RGBA data-URL for import into the sprite editor. The write side is
// in sprite-editor.js (the ⬇ ase button); this is its inverse.
//
// Handles: color depth 32 (RGBA), 16 (grayscale) and 8 (indexed, via the
// embedded palette + transparent-index); cel types 0 (raw) and 2 (zlib). It
// composites every cel of frame 0 in layer order (plain source-over, honoring
// cel opacity) — enough for pixel-art sheets. It ignores layer blend modes,
// groups and animation frames beyond the first. Spec:
// https://github.com/aseprite/aseprite/blob/main/docs/ase-file-specs.md

// Aseprite compresses cel pixels with ZLIB (RFC 1950), which is exactly what
// DecompressionStream('deflate') expects (vs. 'deflate-raw' for headerless).
async function inflate(bytes) {
  const stream = new Blob([bytes]).stream().pipeThrough(new DecompressionStream('deflate'))
  return new Uint8Array(await new Response(stream).arrayBuffer())
}

function compositeCel(out, W, H, cx, cy, cw, ch, raw, depth, palette, transIndex, celOpacity) {
  for (let y = 0; y < ch; y++) {
    for (let x = 0; x < cw; x++) {
      const dx = cx + x, dy = cy + y
      if (dx < 0 || dy < 0 || dx >= W || dy >= H) continue
      let r, g, b, a
      if (depth === 32) {
        const i = (y * cw + x) * 4
        r = raw[i]; g = raw[i + 1]; b = raw[i + 2]; a = raw[i + 3]
      } else if (depth === 16) {              // grayscale: value + alpha
        const i = (y * cw + x) * 2
        r = g = b = raw[i]; a = raw[i + 1]
      } else {                                // 8-bit indexed
        const idx = raw[y * cw + x]
        if (idx === transIndex) { r = g = b = 0; a = 0 }
        else { const pe = palette[idx] || [0, 0, 0, 255]; r = pe[0]; g = pe[1]; b = pe[2]; a = pe[3] }
      }
      a = Math.round((a * celOpacity) / 255)
      if (a === 0) continue
      // source-over onto whatever's already there
      const o = (dy * W + dx) * 4
      const sa = a / 255, da = out[o + 3] / 255
      const oa = sa + da * (1 - sa)
      if (oa <= 0) continue
      out[o]     = Math.round((r * sa + out[o]     * da * (1 - sa)) / oa)
      out[o + 1] = Math.round((g * sa + out[o + 1] * da * (1 - sa)) / oa)
      out[o + 2] = Math.round((b * sa + out[o + 2] * da * (1 - sa)) / oa)
      out[o + 3] = Math.round(oa * 255)
    }
  }
}

// arrayBuffer → { width, height, data } (RGBA) of the composited first frame.
// DOM-free, so it's unit-testable outside the browser.
export async function aseToRGBA(arrayBuffer) {
  const buf = new Uint8Array(arrayBuffer)
  const dv = new DataView(arrayBuffer)
  let p = 0
  const u8  = () => buf[p++]
  const u16 = () => { const v = dv.getUint16(p, true); p += 2; return v }
  const i16 = () => { const v = dv.getInt16(p, true);  p += 2; return v }
  const u32 = () => { const v = dv.getUint32(p, true); p += 4; return v }
  const skip = n => { p += n }

  u32()                                   // file size
  if (u16() !== 0xA5E0) throw new Error('not an Aseprite file')
  const nFrames = u16()
  const W = u16(), H = u16()
  const depth = u16()
  u32()                                   // flags
  u16()                                   // speed (deprecated)
  u32(); u32()
  const transIndex = u8()
  skip(3)                                 // ignore
  u16()                                   // number of colors
  skip(2)                                 // pixel w/h ratio
  skip(4)                                 // grid x/y
  skip(4)                                 // grid w/h
  skip(84)                                // reserved → header is 128 bytes

  const palette = []                      // [[r,g,b,a], …]
  const out = new Uint8ClampedArray(W * H * 4)

  for (let f = 0; f < nFrames; f++) {
    const frameStart = p
    const frameBytes = u32()
    if (u16() !== 0xF1FA) throw new Error('bad frame magic')
    const oldCount = u16(); u16(); skip(2); const newCount = u32()
    const nChunks = newCount || oldCount
    for (let c = 0; c < nChunks; c++) {
      const chunkStart = p
      const size = u32()
      const type = u16()
      if (type === 0x2019) {                              // new palette
        const n = u32(); const first = u32(); u32(); skip(8)
        for (let i = 0; i < n; i++) {
          const flags = u16()
          const r = u8(), g = u8(), b = u8(), a = u8()
          palette[first + i] = [r, g, b, a]
          if (flags & 1) skip(u16())                       // skip a named-entry's string
        }
      } else if ((type === 0x0004 || type === 0x0011) && palette.length === 0) {  // old palette
        const packets = u16()
        let idx = 0
        for (let pk = 0; pk < packets; pk++) {
          idx += u8()
          let cnt = u8(); if (cnt === 0) cnt = 256
          for (let i = 0; i < cnt; i++) { const r = u8(), g = u8(), b = u8(); palette[idx++] = [r, g, b, 255] }
        }
      } else if (type === 0x2005 && f === 0) {            // cel (first frame only)
        u16()                                             // layer index
        const cx = i16(), cy = i16()
        const opacity = u8()
        const celType = u16()
        skip(2 + 5)                                       // z-index + reserved
        if (celType === 0 || celType === 2) {
          const cw = u16(), ch = u16()
          let raw = buf.subarray(p, chunkStart + size)
          if (celType === 2) raw = await inflate(raw)
          compositeCel(out, W, H, cx, cy, cw, ch, raw, depth, palette, transIndex, opacity)
        }
        // celType 1 (linked cel) is skipped — rare in a flat sheet
      }
      p = chunkStart + size
    }
    p = frameStart + frameBytes
    break                         // sprite sheet = just the first frame's composite
  }

  return { width: W, height: H, data: out }
}

// arrayBuffer → PNG data-URL of the composited first frame (browser only).
export async function aseToDataUrl(arrayBuffer) {
  const { width, height, data } = await aseToRGBA(arrayBuffer)
  const canvas = document.createElement('canvas')
  canvas.width = width; canvas.height = height
  canvas.getContext('2d').putImageData(new ImageData(data, width, height), 0, 0)
  return canvas.toDataURL()
}
