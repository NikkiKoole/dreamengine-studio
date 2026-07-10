// config for fmbox.c — the LCD DANCER strip sprites, drawn programmatically with
// sprite-draw.js (the boom/flank UI-glyph pattern). The strip is a segmented-LCD
// window (Pocket Operator / Game & Watch lineage): ONE ink color, a few fixed
// poses that toggle on sequencer events — never tweened, never anti-aliased.
//
// There are THREE selectable SCENES (a tap-chip / 'V' cycles them). Each scene is
// a 6-pose FIGURE set with the SAME semantics; the C side picks a pose off
// flash[]/fires[] and offsets by scene*6. Three shared musical GLYPHS ride on top
// of any scene (metal → star, perc → ring, tone → eighth-note at its pitch).
//
//   figure poses (per scene): 0 idle-A (beat)   1 idle-B (off-beat bob)
//     2 stomp-L (kick, odd)   3 stomp-R (kick, even)   4 clap (snare)   5 ta-da (chord)
//   slot blocks:  0-5 DUDE    6-11 TRAIN    12-17 HIPPO
//   shared glyphs: 18 star (metal)   19 ring (perc)   20 note (tone)
//
// Everything is ink 23 (CLR_LIGHT_YELLOW — matches the cart's title text) on
// transparent; the C side lays it over the black LCD glass.

const { blank, pixel, rectfill, rrectfill, line, circlefill, ovalfill, trifill, flat } = require('../sprite-draw.js')

const INK = 23

// horizontal flip (sprite-draw's mirror() symmetrizes; we want the other leg)
const flip = g => g.map(r => r.slice().reverse())

// 2px-thick limb — a line drawn twice, offset 1px right (chunky G&W arms)
function limb(g, x0, y0, x1, y1) {
  line(g, x0, y0, x1, y1, INK)
  line(g, x0 + 1, y0, x1 + 1, y1, INK)
}

// ── SCENE 1: the DUDE — a segmented-LCD Game & Watch guy ──────────────────────

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

// ── SCENE 2: the TRAIN — a segmented-LCD steam loco, facing left. Same 6 poses:
//    idle bobs = wheel phase + a wisp; KICK = a big chug puff (two rod phases);
//    SNARE = a whistle toot; CHORD = a fat smoke burst. ─────────────────────────
function loco(g, rodY, puff) {
  rectfill(g, 0, 15, 15, 15, INK)               // the rail
  circlefill(g, 4, 12, 2, INK)                  // front driver
  circlefill(g, 11, 12, 2, INK)                 // rear driver
  rectfill(g, 4, rodY, 11, rodY, INK)           // driving rod — its Y is the wheel phase
  rrectfill(g, 1, 6, 12, 4, 1, INK)             // boiler
  rectfill(g, 9, 2, 14, 9, INK)                 // cab
  rectfill(g, 11, 4, 13, 5, 0)                  // cab window (cleared out)
  rectfill(g, 2, 3, 3, 5, INK)                  // smokestack
  rectfill(g, 1, 2, 4, 2, INK)                  // stack cap
  rectfill(g, 6, 4, 7, 5, INK)                  // steam dome
  if (puff >= 1) pixel(g, 2, 1, INK)            // a wisp
  if (puff >= 2) { rectfill(g, 1, 0, 4, 1, INK); pixel(g, 5, 0, INK) }   // a puff
}
function trIdleA()  { const g = blank(); loco(g, 10, 1); pixel(g, 3, 1, INK); return g }
function trIdleB()  { const g = blank(); loco(g, 13, 1); return g }
function trChugA()  { const g = blank(); loco(g, 10, 2); return g }
function trChugB()  { const g = blank(); loco(g, 13, 2); return g }
function trWhistle(){ const g = blank(); loco(g, 12, 0)
  rectfill(g, 13, 0, 15, 1, INK); pixel(g, 12, 2, INK); return g }     // a toot from the cab
function trBurst()  { const g = blank(); loco(g, 12, 1)
  ovalfill(g, 3, 1, 4, 2, INK); pixel(g, 6, 0, INK); return g }        // a fat smoke cloud

// ── SCENE 3: the HIPPO — a chunky segmented-LCD hippo, facing left. idle bobs;
//    KICK = a stomp bounce (dust kicks up); SNARE = a chomp (mouth cracks open);
//    CHORD = the big Hungry-Hippo yawn (mouth gapes wide). ──────────────────────
function hippo(g, dy, mouth) {   // dy: body bob (legs stay planted); mouth: 0 shut / 1 chomp / 2 gape
  rectfill(g, 4, 13, 5, 15, INK)                // four stubby legs
  rectfill(g, 7, 13, 8, 15, INK)
  rectfill(g, 10, 13, 11, 15, INK)
  rectfill(g, 12, 13, 13, 15, INK)
  ovalfill(g, 10, 9 + dy, 5, 4, INK)            // rounded back/rump
  ovalfill(g, 5, 8 + dy, 4, 4, INK)             // head
  rrectfill(g, 0, 8 + dy, 5, 5, 1, INK)         // the broad muzzle jutting forward (the signature)
  pixel(g, 3, 4 + dy, INK); pixel(g, 6, 4 + dy, INK)   // ears
  pixel(g, 1, 8 + dy, 0); pixel(g, 3, 8 + dy, 0)       // two nostrils on the muzzle top
  pixel(g, 6, 6 + dy, 0)                              // eye
  if (mouth === 0) {
    line(g, 0, 11 + dy, 4, 11 + dy, 0)                                  // a shut-mouth line
  } else if (mouth === 1) {
    rectfill(g, 0, 11 + dy, 4, 12 + dy, 0)                              // a small chomp crack
  } else {
    rectfill(g, 0, 10 + dy, 5, 12 + dy, 0)                             // the open maw (carved out)
    rectfill(g, 0, 12 + dy, 5, 13 + dy, INK)                           // the dropped lower jaw
    pixel(g, 5, 11 + dy, INK)                                          // the jaw hinge
  }
}
function hpIdleA() { const g = blank(); hippo(g, 0, 0); return g }
function hpIdleB() { const g = blank(); hippo(g, 1, 0); return g }
function hpStompL(){ const g = blank(); hippo(g, -1, 0)                 // bounce up...
  pixel(g, 2, 15, INK); pixel(g, 5, 15, INK); return g }                // ...front dust
function hpStompR(){ const g = blank(); hippo(g, -1, 0)
  pixel(g, 10, 15, INK); pixel(g, 13, 15, INK); return g }              // ...rear dust
function hpChomp() { const g = blank(); hippo(g, 0, 1); return g }
function hpGape()  { const g = blank(); hippo(g, 0, 2); return g }

// ── shared musical glyphs (ride on top of any scene) ──────────────────────────
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
    // scene 0 — DUDE
    0: flat(idleA()),
    1: flat(idleB()),
    2: flat(stompL()),
    3: flat(flip(stompL())),   // stomp-R = the other leg
    4: flat(clap()),
    5: flat(tada()),
    // scene 1 — TRAIN
    6:  flat(trIdleA()),
    7:  flat(trIdleB()),
    8:  flat(trChugA()),
    9:  flat(trChugB()),
    10: flat(trWhistle()),
    11: flat(trBurst()),
    // scene 2 — HIPPO
    12: flat(hpIdleA()),
    13: flat(hpIdleB()),
    14: flat(hpStompL()),
    15: flat(hpStompR()),
    16: flat(hpChomp()),
    17: flat(hpGape()),
    // shared glyphs
    18: flat(sparkle()),
    19: flat(blip()),
    20: flat(note()),
  },
}
