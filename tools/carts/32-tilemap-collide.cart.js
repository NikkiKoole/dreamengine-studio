// config for 32-tilemap-collide.c
// The MAP is a grid of tile numbers. 0 = empty floor (drawn as nothing / walkable).
// Any non-zero number is a sprite slot — here slot 1 is our one wall tile.
// The `tiles` mapping below turns each drawing character into a tile number:
//   '#' -> 1 (a wall),  ' ' -> 0 (floor).  So you can "paint" the maze as text.

module.exports = {
  sprites: {
    // slot 1 — a chunky brick wall tile, 16x16. Bright so walls read instantly.
    // Colour letters: R=red brick, D=dark red mortar, W=white highlight.
    1: `
DDDDDDDDDDDDDDDD
DRRRRRDRRRRRRRWD
DRRRRRDRRRRRRRWD
DRRRRRDRRRRRRWWD
DDDDDDDDDDDDDDDD
DRRRDRRRRRRDRRWD
DRRRDRRRRRRDRRWD
DRRRDRRRRRRDRWWD
DDDDDDDDDDDDDDDD
DRRRRRRRDRRRRRWD
DRRRRRRRDRRRRRWD
DRRRRRRRDRRRRWWD
DDDDDDDDDDDDDDDD
DRRRDRRRRRRDRRWD
DRRRDRRRRRRDRRWD
DDDDDDDDDDDDDDDD
`,
  },

  // The maze. Each row is 20 characters wide, 12 rows tall.
  // 20 cells x 16px = 320 wide, 12 cells x 16px = 192 tall (fits the 320x200 screen).
  // An outer wall boxes the player in; a few interior walls make it a little maze.
  map: {
    layout: [
      '####################',
      '#                  #',
      '#  ####    ####    #',
      '#                  #',
      '#      ####        #',
      '#  ####            #',
      '#             ###  #',
      '#     ####    ###  #',
      '#                  #',
      '#   ###      ####  #',
      '#                  #',
      '####################',
    ],
    tiles: { '#': 1, ' ': 0 },
  },
}
