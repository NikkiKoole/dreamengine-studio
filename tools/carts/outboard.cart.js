// The rack needs room: four stage panels with real knobs and a scope above them do not fit the
// 320x200 default without either shrinking the widgets below a finger or dropping the metering,
// which is the half that TEACHES (docs/design/analog-outboard-chain.md §2b). 400x260 at scale 3 is
// 1200x780 on screen, the same physical size as a 320x200 cart at scale 4.
module.exports = { screenW: 400, screenH: 260, scale: 3 }
