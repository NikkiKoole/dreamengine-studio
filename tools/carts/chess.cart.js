// config for chess.c — pixel-art chess pieces drawn with sprite-draw.js, replacing
// the hard-to-read primitive shapes draw_piece() used to build. One shape function
// per piece (pawn..king), called twice with a colour scheme:
//   WHITE pieces — white body + DARK outline  (reads on both square shades)
//   BLACK pieces — dark body  + LIGHT outline
// so each piece pops on the pale-yellow light squares AND the dark-teal dark ones.
//
// Slot == piece type for white (1..6), type+8 for black (9..14), so C indexes them
// with spr(white ? type : type+8, …). Pieces are ~14px tall on a shared pedestal,
// centred in the 16×16 slot → drawn at spr(slot, cx-8, cy-8). Transparent bg = 0.
//
// pico32 indices: 5 dk-grey · 6 lt-grey · 7 white · 16 brownish-black (outline).
// Iterate: node tools/sprite-preview.js chess --slots 1-6,9-14

const { blank, pixel, rectfill, line, circlefill, outlined, flat } =
  require('../sprite-draw.js')

// flared pedestal every piece stands on
function ped(g, b) { rectfill(g, 4, 14, 11, 14, b); rectfill(g, 5, 13, 10, 13, b); }

function pawn(b, o) {
  const g = blank(); ped(g, b)
  rectfill(g, 6, 9, 9, 12, b)        // stem
  rectfill(g, 5, 8, 10, 8, b)        // collar
  circlefill(g, 7, 5, 2, b)          // round head
  return flat(outlined(g, o))
}
function knight(b, o) {                // horse head facing left, on a neck
  const g = blank(); ped(g, b)
  rectfill(g, 7, 2, 9, 2, b)          // poll / ears
  rectfill(g, 5, 3, 10, 3, b)         // top of head
  rectfill(g, 3, 4, 11, 4, b)         // forehead → mane
  rectfill(g, 2, 5, 11, 5, b)         // brow + muzzle reaches left
  rectfill(g, 2, 6, 11, 6, b)         // muzzle + jaw
  rectfill(g, 4, 7, 10, 7, b)         // jaw narrows to neck
  rectfill(g, 5, 8, 10, 8, b)         // neck
  rectfill(g, 5, 9, 10, 9, b)
  rectfill(g, 5, 10, 11, 10, b)
  rectfill(g, 4, 11, 11, 11, b)       // neck base
  rectfill(g, 5, 12, 10, 12, b)
  pixel(g, 5, 5, o)                   // eye
  pixel(g, 2, 7, o)                   // under-muzzle notch (the chin)
  return flat(outlined(g, o))
}
function bishop(b, o) {                // smooth mitre with the signature slit
  const g = blank(); ped(g, b)
  rectfill(g, 6, 10, 9, 12, b)        // stem
  rectfill(g, 5, 9, 10, 9, b)         // collar
  rectfill(g, 6, 8, 9, 8, b)          // mitre, widening down
  rectfill(g, 5, 6, 10, 7, b)
  rectfill(g, 6, 4, 9, 5, b)
  rectfill(g, 7, 3, 8, 3, b)
  pixel(g, 7, 2, b)                   // top finial
  line(g, 7, 5, 8, 8, o)              // the diagonal slit
  return flat(outlined(g, o))
}
function rook(b, o) {
  const g = blank(); ped(g, b)
  rectfill(g, 5, 7, 10, 12, b)       // tower body
  rectfill(g, 4, 5, 11, 6, b)        // top band
  rectfill(g, 4, 3, 5, 4, b); rectfill(g, 7, 3, 8, 4, b); rectfill(g, 10, 3, 11, 4, b)  // 3 crenellations
  return flat(outlined(g, o))
}
function queen(b, o) {
  const g = blank(); ped(g, b)
  rectfill(g, 5, 8, 10, 12, b)       // body
  rectfill(g, 4, 7, 11, 7, b)        // crown band
  for (const x of [3, 5, 7, 9, 11]) { pixel(g, x, 6, b); pixel(g, x, 5, b) }  // 5 spikes
  pixel(g, 7, 4, b)                  // centre spike taller
  pixel(g, 3, 4, b); pixel(g, 11, 4, b)  // outer orbs
  return flat(outlined(g, o))
}
function king(b, o) {
  const g = blank(); ped(g, b)
  rectfill(g, 5, 9, 10, 12, b)       // body
  rectfill(g, 4, 8, 11, 8, b)        // crown band
  pixel(g, 5, 7, b); pixel(g, 8, 7, b)   // shoulder points
  rectfill(g, 7, 3, 8, 7, b)         // cross — vertical shaft (down into the crown)
  rectfill(g, 6, 4, 9, 4, b)         // cross — horizontal arm
  return flat(outlined(g, o))
}

const W_BODY = 7, W_OUT = 16, B_BODY = 5, B_OUT = 6
module.exports = {
  screenW: 320, screenH: 200, scale: 4, cellW: 16, cellH: 16,
  sprites: {
    1: pawn(W_BODY, W_OUT), 2: knight(W_BODY, W_OUT), 3: bishop(W_BODY, W_OUT),
    4: rook(W_BODY, W_OUT), 5: queen(W_BODY, W_OUT), 6: king(W_BODY, W_OUT),
    9: pawn(B_BODY, B_OUT), 10: knight(B_BODY, B_OUT), 11: bishop(B_BODY, B_OUT),
    12: rook(B_BODY, B_OUT), 13: queen(B_BODY, B_OUT), 14: king(B_BODY, B_OUT),
  },
}
