// epiano runs at the Tinyjam bundle size (320x240) so it can join the app alongside
// acidrack/yachtrack (build-app.js requires matching dims). Its layout is top-anchored and
// doesn't reference SCREEN_H, so the extra 40px is headroom below the keybed.
module.exports = { screenW: 320, screenH: 240, scale: 3 }
