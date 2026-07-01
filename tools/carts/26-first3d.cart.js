// config for 26-first3d.c — one 16x16 texture: a sky-blue tile with a yellow
// arrow pointing right and a black dot in the top-left corner. Both the
// direction (left/right) and the up/down orientation are visually obvious,
// so spinning the textured quad in 3D actually SHOWS you which way it turned.
function arrowTile() {
  const BG = 12    // CLR_BLUE
  const ARROW = 10 // CLR_YELLOW
  const OUTLINE = 0 // CLR_BLACK
  const DOT = 0     // CLR_BLACK
  const px = []
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      let c = BG
      // shaft: a horizontal bar
      if (y >= 6 && y <= 9 && x >= 1 && x <= 8) c = ARROW
      // head: a triangle narrowing to a point at x=14
      if (x >= 8 && x <= 14) {
        const half = 14 - x
        if (Math.abs(y - 7.5) <= half) c = ARROW
      }
      px.push(c)
    }
  }
  // outline the arrow so it reads clearly against the affine warp
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      if (px[y * 16 + x] !== BG) continue
      const n = [[-1,0],[1,0],[0,-1],[0,1]]
      for (const [dx, dy] of n) {
        const nx = x + dx, ny = y + dy
        if (nx < 0 || ny < 0 || nx > 15 || ny > 15) continue
        if (px[ny * 16 + nx] === ARROW) { px[y * 16 + x] = OUTLINE; break }
      }
    }
  }
  // a corner dot marks "up" so pitch is as visible as yaw
  px[1 * 16 + 1] = DOT
  px[1 * 16 + 2] = DOT
  px[2 * 16 + 1] = DOT
  return px
}

module.exports = {
  sprites: {
    0: arrowTile(),
  },
}
