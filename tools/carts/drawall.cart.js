// config for drawall.c — the everything-cart's sprite sheet + tilemap.
// Three sprites cover the sprite/pal/colorkey paths; the map uses slots 1 & 2.
//   slot 0 — a bordered block: spr/sspr/spr_rot/sspr_ex source + the tritex texture. Avoids
//            index 8 (the colorkey colour) so colorkey(8) doesn't punch holes in it.
//   slot 1 — a little person painted in the pal-magic indices (shirt 28 / pants 29) so pal()
//            visibly recolours it.
//   slot 2 — a colorkey test tile: the whole 16×16 is index 8 (red); colorkey(8) drops it,
//            leaving the green disc.
const { blank, pixel, rectfill, circlefill, outlined, flat } = require('../sprite-draw.js')

function tile() {
  const g = blank()
  rectfill(g, 1, 1, 14, 14, 12)        // outer
  rectfill(g, 4, 4, 11, 11, 3)         // inner
  circlefill(g, 8, 8, 3, 10)           // dot
  pixel(g, 8, 8, 7)
  return flat(outlined(g))
}
function guy() {
  const g = blank()
  circlefill(g, 8, 4, 3, 31)           // head (peach)
  rectfill(g, 5, 7, 10, 11, 28)        // shirt = pal-magic 28
  rectfill(g, 5, 12, 6, 15, 29)        // pants = pal-magic 29
  rectfill(g, 9, 12, 10, 15, 29)
  return flat(outlined(g))
}
function keyed() {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, 8)         // red field (the key colour)
  circlefill(g, 8, 8, 6, 11)           // green disc survives colorkey(8)
  pixel(g, 8, 8, 7)
  return flat(g)
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4, cellW: 16, cellH: 16,
  sprites: { 0: tile(), 1: guy(), 2: keyed() },
  map: { tiles: { '1': 1, '2': 2 }, layout: ['1212', '2121', '1212'] },   // 4×3 tiles
}
