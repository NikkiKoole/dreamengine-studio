// touchpad.cart.js — pixel-art on-screen touch controls (design exploration for
// docs/design/touch-controls.md). Each control is one 16×16 slot (≈ the native
// size at a typical SCALE). Shapes are tested against the TRUE grid centre 8.0
// (between pixels 7 and 8) so fills are mirror-symmetric. A/B labels are drawn in
// the engine's OWN dos_8x8 font via sprite-draw's print() — identical to runtime.
// Iterate:  node tools/sprite-preview.js touchpad --scale 14 --cols 7
'use strict'
const { blank, pixel, rectfill, circlefill, outlined, flat,
        print, textSize, FONT_DEFAULT } = require('../sprite-draw.js')

// palette indices (pico32 / CLR_*)
const DGREEN=3, DGREY=5, LGREY=6, WHITE=7, RED=8, ORANGE=9, GREEN=11, BLUE=12,
      DKGREY=21, DRED=24, DKORANGE=25, TBLUE=28
const FACE=LGREY, HI=WHITE, SH=DGREY
const C = 8.0   // continuous grid centre on a 16-wide canvas

// symmetric fill: run `test(dx,dy)` at every pixel, dx/dy measured from centre 8.0
function sym(g, test, c) {
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const dx = x + 0.5 - C, dy = y + 0.5 - C
    if (test(dx, dy)) pixel(g, x, y, c)
  }
}
const disc = (g, R, c)      => sym(g, (dx, dy) => dx*dx + dy*dy <= R*R, c)
const ring = (g, r0, r1, c) => sym(g, (dx, dy) => { const d = dx*dx + dy*dy; return d <= r1*r1 && d >= r0*r0 }, c)
const octa = (g, R, c)      => sym(g, (dx, dy) => { const ax = Math.abs(dx), ay = Math.abs(dy); return ax <= R && ay <= R && ax + ay <= R * Math.SQRT2 }, c)

// centre a label on the button face (centre x=8, y=cy) in the engine's dos_8x8 font
function labelOn(g, char, cy) {
  const { w, h } = textSize(char, FONT_DEFAULT)
  print(g, char, Math.round(C - w / 2), Math.round(cy - h / 2), WHITE, FONT_DEFAULT)
}

// ── 4-way d-pad ──────────────────────────────────────────────────────────────
function dpad(pressedDir /* null | 'up' */) {
  const g = blank()
  rectfill(g, 5, 1, 10, 14, FACE)   // vertical arm  (x5..10 ⇒ centred on 8.0)
  rectfill(g, 1, 5, 14, 10, FACE)   // horizontal arm
  rectfill(g, 5, 1, 10, 1, HI);  rectfill(g, 1, 5, 1, 10, HI);  rectfill(g, 5, 1, 5, 14, HI)
  rectfill(g, 5,14, 10,14, SH);  rectfill(g,14, 5,14,10, SH);  rectfill(g,10, 1,10,14, SH)
  disc(g, 2.5, DKGREY)
  if (pressedDir === 'up') { rectfill(g, 5, 1, 10, 4, WHITE); rectfill(g, 5, 1, 10, 1, HI) }
  return flat(outlined(g))
}

// ── 8-way d-pad — concentric octagon, radially symmetric ───────────────────────
function dpad8(pressedDir /* null | 'ne' */) {
  const g = blank()
  octa(g, 7, DGREY)
  octa(g, 5.3, FACE)
  disc(g, 2, DKGREY)
  const t = DKGREY
  rectfill(g, 7, 1, 8, 2, t); rectfill(g, 7, 13, 8, 14, t)   // N / S
  rectfill(g, 1, 7, 2, 8, t); rectfill(g, 13, 7, 14, 8, t)   // W / E
  pixel(g, 4, 4, t); pixel(g, 11, 4, t); pixel(g, 4, 11, t); pixel(g, 11, 11, t) // diagonals
  if (pressedDir === 'ne') { pixel(g, 11, 4, WHITE); rectfill(g, 9, 3, 12, 6, WHITE) }
  return flat(outlined(g))
}

// ── analog stick: base ring + knob ─────────────────────────────────────────────
function stickBase() {
  const g = blank()
  ring(g, 5.2, 7, DGREY)
  disc(g, 5.2, DKGREY)
  rectfill(g, 7, 1, 8, 1, LGREY); rectfill(g, 7, 14, 8, 14, LGREY)
  rectfill(g, 1, 7, 1, 8, LGREY); rectfill(g, 14, 7, 14, 8, LGREY)
  return flat(outlined(g))
}
function stickKnob(off /* [dx,dy] */) {
  const g = blank()
  const [ox, oy] = off || [0, 0]
  sym(g, (dx, dy) => (dx-ox)*(dx-ox) + (dy-oy)*(dy-oy) <= 5*5, FACE)
  sym(g, (dx, dy) => (dx-ox+2)*(dx-ox+2) + (dy-oy+2)*(dy-oy+2) <= 2*2, HI)
  sym(g, (dx, dy) => (dx-ox-2)*(dx-ox-2) + (dy-oy-2)*(dy-oy-2) <= 1.2*1.2, SH)
  return flat(outlined(g))
}

// ── round action button: symmetric disc + corner gloss + baked label ───────────
function button(face, shadow, lab, pressed) {
  const g = blank()
  const cy = pressed ? C : C - 1
  disc(g, 7, shadow)
  sym(g, (dx, dy) => dx*dx + (dy + (pressed ? 0 : 1))*(dy + (pressed ? 0 : 1)) <= 7*7, face)
  if (!pressed) { pixel(g, 4, 4, WHITE); pixel(g, 5, 4, WHITE); pixel(g, 4, 5, WHITE) }
  if (lab) labelOn(g, lab, cy)
  return flat(outlined(g))
}
function fire(face, shadow) {
  const g = blank()
  disc(g, 7, shadow)
  sym(g, (dx, dy) => dx*dx + (dy+1)*(dy+1) <= 7*7, face)
  pixel(g, 4, 4, WHITE); pixel(g, 5, 4, WHITE); pixel(g, 4, 5, WHITE)
  return flat(outlined(g))
}

module.exports = {
  sprites: {
    0:  dpad(null), 1:  dpad('up'),
    2:  dpad8(null), 3: dpad8('ne'),
    4:  stickBase(), 5: stickKnob([0, 0]), 6: stickKnob([3, -2]),
    8:  button(RED,  DRED,  'A', false), 9:  button(RED,  DRED,  'A', true),
    10: button(BLUE, TBLUE, 'B', false), 11: button(BLUE, TBLUE, 'B', true),
    12: fire(GREEN,  DGREEN), 13: fire(ORANGE, DKORANGE),
  }
}
