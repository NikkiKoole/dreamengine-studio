// 15-anim.cart.js — v2: the walk cycle is GENERATED, not hand-drawn.
//
// One parametric function walker(t) draws the little figure at any point
// t = 0..1 of its stride; sampling it 6 times yields the whole animation.
// That's the deeper lesson hiding behind anim(): frames are just the same
// drawing evaluated at different times. (sprite-draw.js workflow — see
// monstermix.cart.js for the full showcase.)
//
// The body uses the magic pal() indices (28 main / 29 dark) so the cart
// recolors every walker at draw time — seven outfits from one sprite set.
//
// Slots 1-6: the 6-frame cycle.

const { blank, pixel, line, rectfill, circlefill, ovalfill, outlined, flat } =
  require('../sprite-draw.js')

const M = 28, A = 29, EYE = 7   // magic body, magic accent, white eye

function walker(t) {
  const g = blank()
  // legs swing like a pendulum; +0.5 centers the samples between the
  // extremes so no two of the 6 frames come out identical
  const swing = Math.sin((t + 0.5 / 6) * 2 * Math.PI)   // -1..1
  const bob = Math.abs(swing) > 0.8 ? 1 : 0             // body sinks at full stride

  // back arm + back leg first (body covers their tops)
  line(g, 8, 8 + bob, 8 + swing * 3, 11.5 + bob, A)     // back arm (counter-swings)
  line(g, 7, 11 + bob, 8 - swing * 3.5, 14, A)          // back leg
  rectfill(g, Math.round(8 - swing * 3.5) - 1, 15, Math.round(8 - swing * 3.5), 15, A)

  // body + head (1px gap at the neck — outlined() fills it, separating them)
  ovalfill(g, 8, 9 + bob, 3.4, 3.0, M)                  // torso
  rectfill(g, 7, 10 + bob, 9, 10 + bob, A)              // belt stripe
  circlefill(g, 8, 3 + bob, 2.5, M)                     // head
  pixel(g, 9, 3 + bob, EYE)                             // eyes, facing right
  pixel(g, 10, 3 + bob, EYE)

  // front leg + front arm on top
  line(g, 9, 11 + bob, 8 + swing * 3.5, 14, M)          // front leg
  rectfill(g, Math.round(8 + swing * 3.5), 15, Math.round(8 + swing * 3.5) + 1, 15, A)
  line(g, 8, 8 + bob, 8 - swing * 3, 11.5 + bob, A)     // front arm

  return flat(outlined(g))
}

// sample the stride at 6 points -> slots 1-6
const sprites = {}
for (let f = 0; f < 6; f++) sprites[1 + f] = walker(f / 6)

module.exports = { sprites }
