// Minesweeper draws entirely from primitives — no sprite sheet, no tile map.
// It just needs a screen tall/wide enough for the 16x14 board (16px cells) + HUD.
//   board: 16*16 = 256 wide, 14*16 = 224 tall, + 24px HUD = 248
module.exports = {
  screenW: 272,
  screenH: 256,
  scale: 3,
};
