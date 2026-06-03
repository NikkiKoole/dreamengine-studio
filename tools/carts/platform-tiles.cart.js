// Level for platform-tiles.c, built programmatically so every row is exactly
// LEVEL_W wide and features are placed by (col,row). Tile ids (see tiles map):
//   # solid   = one-way   o coin   ^ spike   G goal   S spawn   e enemy
const W = 56, H = 13;
const g = Array.from({ length: H }, () => Array(W).fill(" "));
const hline = (r, c0, c1, ch) => { for (let c = c0; c <= c1; c++) g[r][c] = ch; };
const put   = (c, r, ch) => { g[r][c] = ch; };

// ground (two solid rows along the bottom)
hline(11, 0, W - 1, "#");
hline(12, 0, W - 1, "#");

// two pits in the ground — fall in and you respawn
hline(11, 25, 27, " "); hline(12, 25, 27, " ");
hline(11, 44, 45, " "); hline(12, 44, 45, " ");

// a stretch of spikes on the ground you must jump over
hline(10, 17, 19, "^");

// solid floating platforms
hline(8, 8, 12, "#");
hline(6, 32, 37, "#");

// one-way platforms (jump up through, land on top)
hline(8, 20, 24, "=");
hline(9, 39, 43, "=");

// coins
hline(7, 9, 11, "o");          // over the first platform
hline(5, 33, 36, "o");         // over the high platform
put(21, 7, "o"); put(23, 7, "o");
put(48, 10, "o"); put(50, 10, "o"); put(52, 10, "o");

// enemies patrolling the ground
put(14, 10, "e");
put(34, 5, "e");               // on the high platform
put(48, 10, "e");

// spawn (left) and goal (right)
put(2, 10, "S");
put(W - 3, 10, "G");

module.exports = {
  map: {
    layout: g.map(row => row.join("")),
    tiles: { "#": 1, "=": 2, "o": 3, "^": 4, "G": 5, "S": 6, "e": 7 },
  },
  mapW: 64,
  mapH: 16,
};
