// flappy.cart.js — the bird + props, drawn in code (sprite-draw.js), not by hand.
//
// One bird() function draws the body at any wing height; sampling it at three
// heights gives the flap cycle (slots 1-4 = up/mid/down/mid, so anim(4,…) reads
// as a natural up-down-up beat). Same trick as 15-anim.cart.js: frames are one
// drawing evaluated at different parameters. A dead frame (X eyes) + a soft
// cloud + a grass bush round out the sheet.

const { blank, pixel, line, rectfill, circlefill, ovalfill, trifill, outlined, flat } =
  require('../sprite-draw.js')

const BLACK = 0, WHITE = 7, ORANGE = 9, YELLOW = 10, GREEN = 11,
      DGREEN = 3, DORANGE = 25, LGREEN = 26

// wy = wing vertical offset (-3 up … +3 down); dead = X eyes + open beak
function bird(wy, dead) {
  const g = blank()
  ovalfill(g, 8, 9, 5, 4, YELLOW)        // body
  ovalfill(g, 7, 11, 3, 2, WHITE)        // pale belly
  ovalfill(g, 5, 9 + wy, 3, 2, ORANGE)   // wing — the animated part
  pixel(g, 3, 9 + wy, DORANGE)           // wing-tip shade
  circlefill(g, 11, 6, 2, WHITE)         // eye white
  if (dead) {
    pixel(g, 10, 5, BLACK); pixel(g, 12, 7, BLACK)   // X
    pixel(g, 12, 5, BLACK); pixel(g, 10, 7, BLACK)
    trifill(g, 13, 8, 16, 9, 13, 11, ORANGE)         // beak hangs open
  } else {
    pixel(g, 12, 6, BLACK); pixel(g, 11, 6, BLACK)   // pupil, facing right
    trifill(g, 13, 7, 16, 8, 13, 9, ORANGE)          // upper beak
    trifill(g, 13, 9, 16, 10, 13, 11, DORANGE)       // lower beak
  }
  return flat(outlined(g))                            // crisp dark outline
}

function cloud() {
  const g = blank()
  ovalfill(g, 8, 10, 7, 3, WHITE)
  circlefill(g, 5, 9, 3, WHITE)
  circlefill(g, 10, 7, 4, WHITE)
  return flat(g)                                       // soft — no outline
}

function bush() {
  const g = blank()
  circlefill(g, 4, 11, 3, GREEN)
  circlefill(g, 9, 8, 4, GREEN)
  circlefill(g, 13, 11, 3, GREEN)
  rectfill(g, 1, 11, 15, 15, GREEN)
  // a couple of lighter dabs for form
  pixel(g, 9, 6, LGREEN); pixel(g, 5, 9, LGREEN); pixel(g, 12, 9, LGREEN)
  return flat(outlined(g, DGREEN))
}

module.exports = {
  sprites: {
    1: bird(-3, false),   // wing up
    2: bird(0,  false),   // wing mid
    3: bird(3,  false),   // wing down
    4: bird(0,  false),   // wing mid (ping-pong back to up)
    5: bird(2,  true),    // dead — drawn tumbling
    6: cloud(),
    7: bush(),
  },
}
