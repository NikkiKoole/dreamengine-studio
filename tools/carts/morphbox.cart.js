// MORPHBOX — no sprites/map; the whole box is primitives. Built resizable (de:meta) and it
// REFLOWS: the layout() in the cart reads screen_w()/screen_h() and picks a ROOMY layout
// (≥240px wide, FONT_NORMAL, breathing room) or the COMPACT 160x100 pocket face. Default is
// the roomy 320x200; set the editor screen to 160x100 (or resize) for the pocket version —
// both are first-class. scale 3 → a 960x600 window.
module.exports = {
  screenW: 320,
  screenH: 200,
  scale: 3,
};
