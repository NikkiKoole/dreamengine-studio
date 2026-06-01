// Textures for infiniminer.c — four 16x16 voxel materials, built as raw
// palette-index arrays so we can reach the extended colors (16-31).
// slot order matches material id - 1:  0 grass, 1 dirt, 2 stone, 3 wood.
//
// palette indices used:
//   3  dark-green   11 green       26 lime-green   (grass)
//   4  brown        20 dark-brown  16 brownish-blk (dirt)
//   5  dark-grey    6  light-grey  22 medium-grey  (stone)
//   4  brown        20 dark-brown  25 dark-orange  (wood)

function noiseTile(base, spec, dark) {
  // base fill with a sprinkle of speckle/dark for texture
  const px = []
  let seed = 1234567
  const rng = () => (seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      const r = rng()
      px.push(r < 0.14 ? dark : r < 0.30 ? spec : base)
    }
  }
  return px
}

function grass() {
  const G = 3, L = 11, S = 26       // dark-green base, green, lime speckle
  const px = []
  let seed = 99
  const rng = () => (seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      const r = rng()
      let c = r < 0.12 ? G : r < 0.4 ? S : L
      // a few darker blades poking up
      if (x % 5 === 2 && y > 6 && r < 0.5) c = G
      px.push(c)
    }
  }
  return px
}

function dirt() {
  return noiseTile(4, 16, 20)        // brown base, brownish-black + dark-brown bits
}

function stone() {
  const px = []
  let seed = 4242
  const rng = () => (seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      let c = 5                       // dark-grey base
      const r = rng()
      if (r < 0.18) c = 22            // medium-grey
      else if (r < 0.26) c = 6        // light-grey highlight
      // cracks: a couple of dark diagonal seams
      if ((x + y) % 11 === 0 || (x - y + 16) % 13 === 0) c = 16
      px.push(c)
    }
  }
  return px
}

function wood() {
  const px = []
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      // vertical grain: alternating brown / dark-brown columns with a ring
      let c = (x % 4 < 2) ? 4 : 20
      if (x === 0 || x === 15) c = 16          // bark edge
      if (x === 7 || x === 8) c = 25           // bright heart line
      if (y === 0 || y === 15) c = 20          // end-grain rings top/bottom
      px.push(c)
    }
  }
  return px
}

module.exports = {
  sprites: {
    0: grass(),
    1: dirt(),
    2: stone(),
    3: wood(),
  },
}
