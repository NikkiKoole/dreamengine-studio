// config for fmbox.c — the LCD DANCER strip sprites, drawn programmatically with
// sprite-draw.js (the boom/flank UI-glyph pattern). The strip is a segmented-LCD
// window (Pocket Operator / Game & Watch lineage): ONE ink color, a few fixed
// poses that toggle on sequencer events — never tweened, never anti-aliased.
//
// Slot layout (the C side picks a pose per frame off flash[]/fires[]):
//   0 idle-A (beat)    1 idle-B (off-beat bob)   2 stomp-L (kick, odd)
//   3 stomp-R (kick, even)   4 clap (snare)      5 arms-up ta-da (chord)
//   6 sparkle (metal)  7 blip ring (perc)        8 eighth-note (tone, at pitch)
//
// Everything is ink 23 (CLR_LIGHT_YELLOW — matches the cart's title text) on
// transparent; the C side lays it over the black LCD glass.

const { blank, pixel, rectfill, line, circlefill, flat } = require('../sprite-draw.js')

const INK = 23

// horizontal flip (sprite-draw's mirror() symmetrizes; we want the other leg)
const flip = g => g.map(r => r.slice().reverse())

// 2px-thick limb — a line drawn twice, offset 1px right (chunky G&W arms)
function limb(g, x0, y0, x1, y1) {
  line(g, x0, y0, x1, y1, INK)
  line(g, x0 + 1, y0, x1 + 1, y1, INK)
}

// the dancer's fixed parts: head + torso at a given whole-figure drop
function trunk(g, dy) {
  rectfill(g, 6, 0 + dy, 9, 3 + dy, INK)      // head
  rectfill(g, 7, 4 + dy, 8, 4 + dy, INK)      // neck
  rectfill(g, 6, 5 + dy, 9, 9 + dy, INK)      // torso
}

function idleA() {                             // standing tall, arms at sides
  const g = blank()
  trunk(g, 0)
  rectfill(g, 4, 5, 4, 9, INK); rectfill(g, 11, 5, 11, 9, INK)     // arms down
  rectfill(g, 6, 10, 7, 14, INK); rectfill(g, 9, 10, 10, 14, INK)  // legs
  rectfill(g, 4, 15, 7, 15, INK); rectfill(g, 9, 15, 12, 15, INK)  // feet out
  return g
}

function idleB() {                             // the off-beat bob: 1px drop, knees out
  const g = blank()
  trunk(g, 1)
  rectfill(g, 4, 6, 4, 10, INK); rectfill(g, 11, 6, 11, 10, INK)   // arms down
  rectfill(g, 5, 11, 6, 14, INK); rectfill(g, 10, 11, 11, 14, INK) // knees out
  rectfill(g, 3, 15, 6, 15, INK); rectfill(g, 10, 15, 13, 15, INK) // feet wide
  return g
}

function stompL() {                            // KICK: left leg raised, arms swing
  const g = blank()
  trunk(g, 0)
  limb(g, 4, 8, 4, 4)                           // left arm swings up-back
  pixel(g, 4, 3, INK); pixel(g, 5, 3, INK)      // its fist
  rectfill(g, 11, 6, 11, 10, INK)               // right arm down
  rectfill(g, 3, 10, 7, 11, INK)                // raised thigh, knee out left
  rectfill(g, 2, 12, 4, 13, INK)                // hanging shin + foot
  rectfill(g, 9, 10, 10, 14, INK)               // planted leg
  rectfill(g, 9, 15, 12, 15, INK)               // planted foot
  return g
}

function clap() {                              // SNARE: hands meet above the head
  const g = blank()
  trunk(g, 3)                                   // drop to make headroom
  limb(g, 4, 8, 6, 4); limb(g, 10, 8, 8, 4)     // arms angle up-in
  rectfill(g, 7, 1, 8, 2, INK)                  // the clapped hands
  pixel(g, 4, 1, INK); pixel(g, 11, 1, INK)     // impact sparks
  rectfill(g, 6, 13, 7, 14, INK); rectfill(g, 9, 13, 10, 14, INK)  // short legs
  rectfill(g, 4, 15, 7, 15, INK); rectfill(g, 9, 15, 12, 15, INK)
  return g
}

function tada() {                              // CHORD: arms up in a V, feet apart
  const g = blank()
  trunk(g, 2)
  limb(g, 4, 7, 2, 2); limb(g, 10, 7, 12, 2)    // arms up-out
  rectfill(g, 1, 0, 2, 1, INK); rectfill(g, 13, 0, 14, 1, INK)     // open hands
  rectfill(g, 5, 12, 6, 14, INK); rectfill(g, 9, 12, 10, 14, INK)  // legs apart
  rectfill(g, 3, 15, 6, 15, INK); rectfill(g, 9, 15, 12, 15, INK)
  return g
}

function sparkle() {                           // METAL: a 4-point LCD star
  const g = blank()
  line(g, 8, 3, 8, 13, INK)
  line(g, 3, 8, 13, 8, INK)
  pixel(g, 6, 6, INK); pixel(g, 10, 6, INK)
  pixel(g, 6, 10, INK); pixel(g, 10, 10, INK)
  rectfill(g, 7, 7, 9, 9, INK)
  return g
}

function blip() {                              // PERC: a small pop ring
  const g = blank()
  pixel(g, 8, 5, INK); pixel(g, 8, 11, INK)
  pixel(g, 5, 8, INK); pixel(g, 11, 8, INK)
  pixel(g, 6, 6, INK); pixel(g, 10, 6, INK)
  pixel(g, 6, 10, INK); pixel(g, 10, 10, INK)
  return g
}

function note() {                              // TONE: an eighth note, hops at its pitch
  const g = blank()
  circlefill(g, 7, 11, 2, INK)                  // head
  line(g, 9, 3, 9, 11, INK)                     // stem
  line(g, 9, 3, 11, 5, INK); pixel(g, 11, 6, INK)  // flag
  return g
}

module.exports = {
  sprites: {
    0: flat(idleA()),
    1: flat(idleB()),
    2: flat(stompL()),
    3: flat(flip(stompL())),   // stomp-R = the other leg
    4: flat(clap()),
    5: flat(tada()),
    6: flat(sparkle()),
    7: flat(blip()),
    8: flat(note()),
  },
}
