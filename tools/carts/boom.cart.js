// config for boom.c — a sheet of tiny pixel-art TOOLBAR ICONS, drawn
// programmatically with sprite-draw.js. boom renders its world procedurally (a
// 4px material grid + particles); the ONLY sprites are these cute clickable
// buttons for the BOOM blasts and the BUILD brushes.
//
// Slot layout (so the C side can index them with zero bookkeeping):
//   0..5   BOOM blasts:  0 blast · 1 grenade · 2 molotov · 3 car bomb ·
//                        4 gas main · 5 ram car   (= the blast enum / RAMCAR)
//   16+m   BUILD brush for material/sentinel value m (icon_slot = 16 + brush):
//          16 ground 17 road 18 grass 19 wood 20 oil 21 water 23 barrel
//          24 car 25 glass 26 concrete 28 gasoline 29 explosive 30 torch 31 crate
//
// pico32 indices used a lot here: 4 brown · 5 dk-grey · 6 lt-grey · 7 white ·
// 8 red · 9 orange · 10 yellow · 11 green · 12 blue · 16 brownish-black (outline)
// · 19 blue-green · 20 dk-brown · 24 dk-red · 30 dk-peach · 31 peach.

const { blank, pixel, rectfill, line, circlefill, ovalfill, rrectfill, outlined, flat } =
  require('../sprite-draw.js')

// ── BOOM blasts (slots 0..5) ──────────────────────────────────────────────────
function ic_blast() {
  const g = blank()
  line(g, 8, 1, 8, 14, 9); line(g, 1, 8, 14, 8, 9)      // orange spikes
  line(g, 3, 3, 12, 12, 9); line(g, 12, 3, 3, 12, 9)
  circlefill(g, 8, 8, 4, 9)                              // orange body
  circlefill(g, 8, 8, 2.4, 10)                           // yellow core
  pixel(g, 8, 8, 7)                                      // white-hot centre
  return flat(g)
}
function ic_grenade() {
  const g = blank()
  ovalfill(g, 8, 10, 4, 4, 3)                            // dark-green body
  pixel(g, 6, 9, 11); pixel(g, 6, 8, 11)                 // highlight
  rectfill(g, 6, 4, 9, 6, 5)                             // grey fuse cap
  rectfill(g, 7, 2, 8, 4, 6)
  pixel(g, 11, 2, 6); pixel(g, 12, 2, 6); pixel(g, 12, 3, 6); pixel(g, 11, 3, 6)  // pin ring
  return flat(outlined(g))
}
function ic_molotov() {
  const g = blank()
  rectfill(g, 6, 7, 10, 14, 19)                          // blue-green bottle
  rectfill(g, 7, 4, 9, 7, 19)                            // neck
  rectfill(g, 7, 10, 9, 13, 30)                          // amber fuel
  pixel(g, 6, 9, 7)                                      // glass glint
  pixel(g, 8, 3, 9); pixel(g, 7, 2, 8); pixel(g, 8, 1, 10); pixel(g, 9, 2, 9)  // rag flame
  return flat(outlined(g))
}
function ic_carbomb() {
  const g = blank()
  rrectfill(g, 3, 6, 9, 6, 2, 8)                         // red car (sideways)
  rectfill(g, 5, 7, 9, 8, 19)                            // windows
  pixel(g, 4, 5, 5); pixel(g, 10, 5, 5); pixel(g, 4, 12, 5); pixel(g, 10, 12, 5)  // wheels
  pixel(g, 13, 3, 10); pixel(g, 12, 2, 9); pixel(g, 14, 4, 9); pixel(g, 13, 2, 7)  // spark
  return flat(outlined(g))
}
function ic_gasmain() {
  const g = blank()
  rectfill(g, 1, 12, 14, 14, 5)                          // horizontal main, across the bottom
  rectfill(g, 1, 12, 14, 12, 6)                          // pipe top highlight
  circlefill(g, 3, 13, 2, 6); pixel(g, 3, 13, 5)         // valve wheel (left)
  rectfill(g, 6, 10, 9, 11, 6)                           // ruptured flange (centre)
  rectfill(g, 6, 8, 9, 11, 8)                            // erupting jet: red base (wide)
  rectfill(g, 6, 5, 9, 8, 9)                             // orange
  rectfill(g, 7, 2, 8, 5, 10)                            // yellow column (tapering up)
  pixel(g, 7, 1, 7)                                      // white-hot tip
  pixel(g, 5, 7, 9); pixel(g, 10, 7, 9)                  // side licks → reads as a flare
  return flat(outlined(g))
}
function ic_ramcar() {
  const g = blank()
  rrectfill(g, 5, 6, 9, 6, 2, 12)                        // blue car
  rectfill(g, 7, 7, 11, 8, 19)                           // window
  pixel(g, 6, 5, 5); pixel(g, 12, 5, 5); pixel(g, 6, 12, 5); pixel(g, 12, 12, 5)  // wheels
  line(g, 0, 6, 3, 6, 6); line(g, 0, 8, 2, 8, 6); line(g, 0, 10, 3, 10, 6)  // motion lines
  return flat(g)
}

// ── BUILD materials (slot = 16 + material value) ──────────────────────────────
function ic_ground() {
  const g = blank(); rectfill(g, 1, 1, 14, 14, 4)
  ;[[3, 3], [10, 5], [6, 9], [12, 11], [4, 12]].forEach(([x, y]) => pixel(g, x, y, 20))
  ;[[7, 4], [11, 8], [5, 6]].forEach(([x, y]) => pixel(g, x, y, 30))
  return flat(g)
}
function ic_road() {
  const g = blank(); rectfill(g, 1, 1, 14, 14, 5)
  line(g, 1, 1, 14, 1, 21); line(g, 1, 14, 14, 14, 21)   // kerb shadows
  rectfill(g, 7, 2, 8, 4, 10); rectfill(g, 7, 7, 8, 9, 10); rectfill(g, 7, 12, 8, 13, 10)  // dashes
  return flat(g)
}
function ic_grass() {
  const g = blank(); rectfill(g, 1, 1, 14, 14, 3)
  ;[3, 6, 9, 12].forEach((x) => { line(g, x, 12, x, 8, 11); pixel(g, x, 7, 11) })
  ;[5, 11].forEach((x) => line(g, x, 13, x, 10, 27))
  return flat(g)
}
function ic_wood() {
  const g = blank(); rectfill(g, 1, 1, 14, 14, 4)
  line(g, 1, 5, 14, 5, 20); line(g, 1, 10, 14, 10, 20)   // plank seams
  ;[[4, 3], [9, 7], [5, 12]].forEach(([x, y]) => pixel(g, x, y, 20))  // grain knots
  return flat(g)
}
function ic_oil() {
  const g = blank(); rectfill(g, 1, 1, 14, 14, 21)
  ovalfill(g, 8, 8, 6, 5, 16)                            // dark slick
  pixel(g, 5, 6, 6); pixel(g, 6, 6, 6); pixel(g, 10, 10, 12)  // oily sheen
  return flat(g)
}
function ic_water() {
  const g = blank(); rectfill(g, 1, 1, 14, 14, 12)
  line(g, 2, 4, 13, 4, 28); line(g, 2, 9, 13, 9, 28)
  pixel(g, 4, 6, 7); pixel(g, 9, 11, 7); pixel(g, 11, 3, 7)  // sparkle
  return flat(g)
}
function ic_barrel() {
  const g = blank(); circlefill(g, 8, 8, 6, 8)           // red drum (top-down)
  circlefill(g, 8, 8, 6, 8)
  ovalfill(g, 8, 8, 5, 5, 24)                            // dark-red band
  circlefill(g, 8, 8, 2.4, 9); pixel(g, 8, 8, 24)        // orange hazard cap
  return flat(outlined(g))
}
function ic_car() {
  const g = blank(); rrectfill(g, 4, 2, 8, 12, 2, 12)    // blue car (top-down)
  rectfill(g, 5, 4, 10, 6, 19); rectfill(g, 5, 9, 10, 11, 19)  // wind/rear glass
  pixel(g, 4, 3, 5); pixel(g, 11, 3, 5); pixel(g, 4, 12, 5); pixel(g, 11, 12, 5)
  return flat(outlined(g))
}
function ic_glass() {
  const g = blank(); rectfill(g, 2, 2, 13, 13, 19)
  line(g, 2, 2, 13, 2, 6); line(g, 2, 2, 2, 13, 6)       // light frame
  line(g, 3, 12, 12, 3, 7); line(g, 4, 12, 12, 4, 6)     // diagonal sheen
  return flat(g)
}
function ic_concrete() {
  const g = blank(); rectfill(g, 1, 1, 14, 14, 6)
  line(g, 1, 5, 14, 5, 5); line(g, 1, 10, 14, 10, 5)     // brick courses
  line(g, 8, 1, 8, 5, 5); line(g, 4, 5, 4, 10, 5); line(g, 11, 10, 11, 14, 5)
  return flat(g)
}
function ic_gas() {
  const g = blank()
  rrectfill(g, 2, 5, 9, 9, 2, 30)                        // jerry-can body
  line(g, 4, 4, 9, 4, 16); rectfill(g, 4, 4, 9, 4, 20)   // handle
  rectfill(g, 11, 3, 13, 6, 30); pixel(g, 13, 2, 30)     // pour spout
  rectfill(g, 4, 9, 8, 10, 31)                           // label stripe
  pixel(g, 4, 7, 7)                                      // highlight
  return flat(outlined(g))
}
function ic_tnt() {
  const g = blank()
  rectfill(g, 4, 6, 5, 13, 8); rectfill(g, 7, 6, 8, 13, 8); rectfill(g, 10, 6, 11, 13, 8)  // 3 sticks
  rectfill(g, 4, 6, 11, 6, 24); rectfill(g, 4, 13, 11, 13, 24)   // caps
  rectfill(g, 3, 9, 12, 10, 20)                          // binding band
  line(g, 8, 6, 11, 2, 16); pixel(g, 11, 2, 10); pixel(g, 12, 1, 9); pixel(g, 12, 2, 7)  // fuse + spark
  return flat(outlined(g))
}
function ic_torch() {
  const g = blank()
  rectfill(g, 7, 8, 8, 14, 4); rectfill(g, 6, 7, 9, 8, 20)  // handle + wrap
  ovalfill(g, 7, 5, 2, 3, 8); ovalfill(g, 7, 5, 2, 2, 9)    // flame
  pixel(g, 7, 3, 10); pixel(g, 8, 4, 10)
  return flat(outlined(g))
}
function ic_crate() {
  const g = blank(); rectfill(g, 2, 2, 13, 13, 4)
  line(g, 2, 2, 13, 2, 20); line(g, 2, 13, 13, 13, 20)
  line(g, 2, 2, 2, 13, 20); line(g, 13, 2, 13, 13, 20)
  line(g, 2, 2, 13, 13, 20); line(g, 13, 2, 2, 13, 20)   // X slats
  pixel(g, 4, 4, 30)
  return flat(g)
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16,
  sprites: {
    0: ic_blast(), 1: ic_grenade(), 2: ic_molotov(),
    3: ic_carbomb(), 4: ic_gasmain(), 5: ic_ramcar(),
    16: ic_ground(), 17: ic_road(), 18: ic_grass(), 19: ic_wood(),
    20: ic_oil(), 21: ic_water(), 23: ic_barrel(), 24: ic_car(),
    25: ic_glass(), 26: ic_concrete(), 28: ic_gas(), 29: ic_tnt(),
    30: ic_torch(), 31: ic_crate(),
  },
}
