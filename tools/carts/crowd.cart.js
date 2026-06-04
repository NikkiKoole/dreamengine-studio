// config for crowd.c — a plaza full of little people walking, colliding and
// bouncing. Sprites are generated programmatically here (16×32 characters split
// into two 16×16 slots) so we can use palette indices > 15 for the "magic"
// clothing colors that pal() recolors per-agent at runtime.
//
// Magic colors: every character's SHIRT pixels are drawn in index 28 and PANTS
// pixels in index 29. They're just placeholders — the cart calls pal(28, ...) and
// pal(29, ...) before drawing each agent, so one set of sprites yields a whole
// crowd of differently-dressed people.
//
// Slot layout (8-col sheet):
//   0,1,2   — three character TOPS (head + upper torso), one per body type
//   8,9,10,11 — four shared LEG frames (lower torso + legs + shoes): a 4-step
//               front-view march cycle [neutral, left-up, neutral, right-up]
//   32,33,34 — map tiles: grass, path, flower

const { blank, pixel, rectfill, outlined, flat, noise, OUT } = require('../sprite-draw.js')

const W = 16, H = 32

// palette indices
const TRANS = 0       // transparent (cart calls colorkey(0))
const WHITE = 7
const SHOE  = 5       // dark grey
const SHIRT = 28      // MAGIC: recolored by pal(28, shirtColor)
const PANTS = 29      // MAGIC: recolored by pal(29, pantsColor)

// ── tiny pixel canvas helpers ─────────────────────────────────
const blank16x32 = () => blank(16, 32)

// ── character TOP (rows 0-15): head, hair, face, upper torso + sleeves ────────
function makeTop({ skin, hair, style }) {
  const g = blank()
  // upper torso + shoulders in shirt (continues into the leg slot below)
  rectfill(g, 4, 10, 11, 15, SHIRT)      // chest, flush to bottom edge (row 15) → clean seam
  rectfill(g, 3, 11, 3, 15, SHIRT)       // left sleeve
  rectfill(g, 12, 11, 12, 15, SHIRT)     // right sleeve
  // head
  rectfill(g, 5, 4, 10, 9, skin)         // face
  // hair cap + bangs
  rectfill(g, 4, 2, 11, 4, hair)
  rectfill(g, 5, 5, 10, 5, hair)         // fringe just over the brow
  if (style === 'long')  { rectfill(g, 4, 5, 4, 9, hair); rectfill(g, 11, 5, 11, 9, hair) }
  if (style === 'spiky') { pixel(g, 5, 1, hair); pixel(g, 7, 1, hair); pixel(g, 9, 1, hair); pixel(g, 11, 1, hair) }
  if (style === 'short') { rectfill(g, 4, 5, 4, 6, hair); rectfill(g, 11, 5, 11, 6, hair) }
  // eyes
  pixel(g, 6, 7, OUT); pixel(g, 9, 7, OUT)
  return flat(outlined(g))
}

// ── shared LEG frame (figure rows 16-31): lower torso, arms/hands, pants, shoes ──
// local y = figureY - 16. `leftUp`/`rightUp` raise a leg for the march cycle.
function makeLegs(leftUp, rightUp) {
  const g = blank()
  rectfill(g, 4, 0, 11, 3, SHIRT)        // lower torso (shirt), flush to top edge → clean seam
  rectfill(g, 3, 0, 3, 2, SHIRT)         // forearms
  rectfill(g, 12, 0, 12, 2, SHIRT)
  pixel(g, 3, 3, 15); pixel(g, 12, 3, 15)   // hands (light skin — shared across body types)
  rectfill(g, 4, 4, 11, 6, PANTS)        // pelvis / hips
  // legs — normal foot at local y12, raised foot at y10
  const legs = [
    { x0: 5, x1: 6, up: leftUp },
    { x0: 9, x1: 10, up: rightUp },
  ]
  for (const L of legs) {
    const footY = L.up ? 10 : 12
    rectfill(g, L.x0, 7, L.x1, footY, PANTS)         // leg
    rectfill(g, L.x0, footY + 1, L.x1, footY + 2, SHOE)  // shoe
  }
  return flat(outlined(g))
}

// ── map tiles (16×16, fully opaque — avoid index 0 so nothing turns transparent) ──
function grassTile() {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, 3)           // dark green base
  // scattered lighter blades, deterministic
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++) {
      const n = noise(x, y, 17)
      if (n === 0) g[y][x] = 11      // bright green
      else if (n === 5) g[y][x] = 27 // medium green
    }
  return flat(g)
}
function pathTile() {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, 6)           // light grey
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++) {
      const n = noise(x, y, 13)
      if (n === 0) g[y][x] = 5       // darker grey cobble fleck
      else if (n === 7) g[y][x] = 22 // warm grey fleck
    }
  return flat(g)
}
function flowerTile() {
  const g = grassTile().reduce((acc, v, i) => { acc[Math.floor(i / 16)][i % 16] = v; return acc },
                               blank())
  // a little flower: petals + center
  const petal = [8, 14, 9, 12][(0)] // base; we vary by position in the layout instead
  rectfill(g, 7, 7, 8, 8, 10)            // yellow center
  pixel(g, 6, 7, petal); pixel(g, 9, 7, petal); pixel(g, 7, 6, petal); pixel(g, 8, 9, petal)
  return flat(g)
}

// ── build the map layout (20×13 for a 320×200 screen) ─────────────────────────
const MW = 20, MH = 13
const layout = []
for (let y = 0; y < MH; y++) {
  let row = ''
  for (let x = 0; x < MW; x++) {
    let c = 'g'
    if (y >= 6 && y <= 7) c = 'p'          // horizontal path
    if (x >= 9 && x <= 10) c = 'p'         // vertical path
    if (c === 'g' && noise(x, y, 11) === 0) c = 'f'  // sparse flowers
    row += c
  }
  layout.push(row)
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16, mapW: MW, mapH: MH,
  sprites: {
    0: makeTop({ skin: 15, hair: 4,  style: 'short' }),  // light skin, brown hair
    1: makeTop({ skin: 31, hair: 16, style: 'long'  }),  // peach skin, black hair
    2: makeTop({ skin: 4,  hair: 10, style: 'spiky' }),  // brown skin, blonde hair
    8:  makeLegs(false, false),   // neutral
    9:  makeLegs(true,  false),   // left foot up
    10: makeLegs(false, false),   // neutral
    11: makeLegs(false, true),    // right foot up
    32: grassTile(),
    33: pathTile(),
    34: flowerTile(),
  },
  map: {
    layout,
    tiles: { g: 32, p: 33, f: 34 },
  },
}
