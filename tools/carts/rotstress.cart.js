// config for rotstress.c — one fully-opaque 16×16 textured sprite. Opaque everywhere (no index-0
// transparent pixels) so the rotated blit SAMPLES AND PLOTS every dest pixel — the worst case for the
// software-canvas sampling path we're profiling (GetImageColor + the inverse-map inner loop).
const { blank, pixel, flat } = require('../sprite-draw.js')

function tex() {
  const g = blank()
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++)
      pixel(g, x, y, 1 + ((x * 3 + y * 5) % 30))   // varied 1..30 (never 0 → fully opaque)
  return flat(g)
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4, cellW: 16, cellH: 16,
  sprites: { 0: tex() },
}
