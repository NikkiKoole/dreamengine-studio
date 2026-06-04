// Sprites for doom.c — all generated programmatically so we can reach the
// extended palette (indices 16–31) for dark outlines / dark-red gore, and build
// the 32×16 weapon view-models out of two adjacent 16×16 slots.
//
// Slot layout (8-col sheet):
//   0,1 imp walk · 2 imp attack · 3 imp gib
//   4,5 gunner walk · 6 gunner attack · 7 gunner gib
//   8 keycard · 9 medkit · 10 armor · 11 ammo
//   12 face ok · 13 face hurt
//   16,17 pistol idle · 18,19 pistol fire   (each gun = two slots, sspr'd as 32×16)
//   24,25 shotgun idle · 26,27 shotgun fire

const { blank, pixel, rectfill, outlined, flat, split, OUT } = require('../sprite-draw.js')

// ── imp (red horned demon) ──────────────────────────────────────────────────
function imp(leg) {
  const g = blank(), lf = leg ? 13 : 15, rf = leg ? 15 : 13
  rectfill(g, 5, 12, 6, lf, 24); rectfill(g, 9, 12, 10, rf, 24)   // legs
  rectfill(g, 4, 6, 11, 12, 8); rectfill(g, 6, 8, 9, 11, 24)      // torso + belly
  rectfill(g, 3, 7, 4, 11, 8); rectfill(g, 11, 7, 12, 11, 8)      // arms
  pixel(g, 3, 11, 15); pixel(g, 12, 11, 15)                 // claws
  rectfill(g, 5, 2, 10, 6, 8)                                // head
  pixel(g, 4, 1, 15); pixel(g, 11, 1, 15); pixel(g, 4, 0, 15); pixel(g, 11, 0, 15)  // horns
  pixel(g, 6, 4, 10); pixel(g, 9, 4, 10)                    // eyes
  rectfill(g, 6, 5, 9, 5, 16); pixel(g, 7, 6, 7); pixel(g, 8, 6, 7)  // mouth + fangs
  return flat(outlined(g))
}
function impAtk() {
  const g = blank()
  rectfill(g, 5, 12, 6, 15, 24); rectfill(g, 9, 12, 10, 15, 24)
  rectfill(g, 4, 6, 11, 12, 8); rectfill(g, 6, 8, 9, 11, 24)
  rectfill(g, 3, 2, 4, 7, 8); rectfill(g, 11, 2, 12, 7, 8)        // arms RAISED
  pixel(g, 3, 1, 15); pixel(g, 12, 1, 15)
  rectfill(g, 5, 2, 10, 6, 8)
  pixel(g, 4, 1, 15); pixel(g, 11, 1, 15)
  pixel(g, 6, 4, 10); pixel(g, 9, 4, 10)
  rectfill(g, 6, 5, 9, 7, 16); pixel(g, 6, 5, 7); pixel(g, 9, 5, 7)  // big open maw
  return flat(outlined(g))
}
function impDead() {
  const g = blank()
  rectfill(g, 3, 12, 12, 15, 24); rectfill(g, 4, 13, 11, 15, 8)
  pixel(g, 5, 12, 8); pixel(g, 8, 12, 8); pixel(g, 10, 13, 15); pixel(g, 3, 14, 8); pixel(g, 12, 14, 8)
  return flat(g)
}

// ── gunner (green armored soldier) ────────────────────────────────────────────
function gunner(leg) {
  const g = blank(), lf = leg ? 13 : 15, rf = leg ? 15 : 13
  rectfill(g, 5, 12, 6, lf, 3); rectfill(g, 9, 12, 10, rf, 3)
  rectfill(g, 4, 6, 11, 12, 11); rectfill(g, 5, 8, 10, 11, 3)
  rectfill(g, 3, 7, 4, 10, 11)
  rectfill(g, 6, 2, 9, 6, 15); rectfill(g, 5, 1, 10, 2, 3)        // head + helmet
  pixel(g, 6, 4, 8); pixel(g, 9, 4, 8)                      // eyes
  rectfill(g, 10, 8, 13, 9, 5); pixel(g, 13, 8, 16)            // gun, barrel toward you
  return flat(outlined(g))
}
function gunnerAtk() {
  const g = blank()
  rectfill(g, 5, 12, 6, 15, 3); rectfill(g, 9, 12, 10, 15, 3)
  rectfill(g, 4, 6, 11, 12, 11); rectfill(g, 5, 8, 10, 11, 3)
  rectfill(g, 3, 7, 4, 10, 11)
  rectfill(g, 6, 2, 9, 6, 15); rectfill(g, 5, 1, 10, 2, 3)
  pixel(g, 6, 4, 8); pixel(g, 9, 4, 8)
  rectfill(g, 10, 8, 13, 9, 5)
  pixel(g, 14, 8, 10); pixel(g, 15, 8, 7); pixel(g, 14, 7, 9); pixel(g, 14, 9, 9); pixel(g, 15, 9, 10)  // muzzle flash
  return flat(outlined(g))
}
function gunnerDead() {
  const g = blank()
  rectfill(g, 3, 12, 12, 15, 3); rectfill(g, 4, 13, 11, 15, 11)
  pixel(g, 5, 12, 11); pixel(g, 9, 12, 11); pixel(g, 4, 14, 8); pixel(g, 11, 13, 5)
  return flat(g)
}

// ── pickups ───────────────────────────────────────────────────────────────────
function keycard() {
  const g = blank()
  rectfill(g, 4, 5, 11, 11, 8); rectfill(g, 5, 6, 10, 7, 7); rectfill(g, 5, 9, 10, 9, 24); pixel(g, 11, 8, 16)
  return flat(outlined(g))
}
function medkit() {
  const g = blank()
  rectfill(g, 3, 4, 12, 12, 7); rectfill(g, 3, 4, 12, 5, 6)
  rectfill(g, 7, 6, 8, 11, 8); rectfill(g, 5, 8, 10, 9, 8)        // red cross
  return flat(outlined(g))
}
function armorPack() {
  const g = blank()
  rectfill(g, 5, 4, 10, 11, 12); rectfill(g, 6, 4, 9, 5, 7); rectfill(g, 6, 7, 9, 9, 28)
  rectfill(g, 4, 5, 4, 9, 12); rectfill(g, 11, 5, 11, 9, 12); pixel(g, 7, 12, 12); pixel(g, 8, 12, 12)
  return flat(outlined(g))
}
function ammoBox() {
  const g = blank()
  rectfill(g, 3, 7, 12, 12, 4); rectfill(g, 3, 7, 12, 7, 9)
  rectfill(g, 4, 4, 5, 7, 9); pixel(g, 4, 4, 10); pixel(g, 5, 4, 10)     // shells
  rectfill(g, 7, 3, 8, 7, 9); pixel(g, 7, 3, 10); pixel(g, 8, 3, 10)
  rectfill(g, 10, 5, 11, 7, 9); pixel(g, 10, 5, 10); pixel(g, 11, 5, 10)
  return flat(outlined(g))
}

// ── HUD face ────────────────────────────────────────────────────────────────
function faceBase() {
  const g = blank()
  rectfill(g, 4, 3, 11, 13, 15)                              // face
  rectfill(g, 4, 2, 11, 3, 4); rectfill(g, 4, 2, 4, 6, 4); rectfill(g, 11, 2, 11, 6, 4)  // hair
  rectfill(g, 5, 6, 6, 7, 7); rectfill(g, 9, 6, 10, 7, 7); pixel(g, 6, 6, 1); pixel(g, 9, 6, 1)  // eyes
  rectfill(g, 5, 5, 6, 5, 4); rectfill(g, 9, 5, 10, 5, 4)         // brow
  pixel(g, 7, 8, 4); pixel(g, 8, 8, 4)                      // nose
  return g
}
function faceOk() {
  const g = faceBase()
  rectfill(g, 6, 11, 9, 11, 16); pixel(g, 6, 10, 7); pixel(g, 9, 10, 7)
  return flat(outlined(g))
}
function faceHurt() {
  const g = faceBase()
  pixel(g, 5, 8, 8); pixel(g, 5, 9, 8); pixel(g, 10, 9, 8); pixel(g, 6, 12, 8)   // blood
  rectfill(g, 6, 11, 9, 12, 16); pixel(g, 7, 11, 7); pixel(g, 8, 11, 7)          // grimace
  pixel(g, 6, 5, 16); pixel(g, 9, 5, 16)
  return flat(outlined(g))
}

// ── weapon view-models (32×16) ─────────────────────────────────────────────────
function pistolGrid(fire) {
  const g = blank(32, 16)
  rectfill(g, 13, 3, 19, 9, 5); rectfill(g, 13, 3, 19, 4, 6)      // body + highlight
  rectfill(g, 15, 0, 17, 3, 5); pixel(g, 16, 0, 16)            // barrel up
  rectfill(g, 13, 9, 18, 15, 4); rectfill(g, 14, 9, 15, 15, 20)   // grip
  rectfill(g, 12, 9, 13, 13, 15); rectfill(g, 18, 9, 19, 13, 15)  // hands
  if (fire) { rectfill(g, 14, 0, 18, 2, 10); rectfill(g, 15, 0, 17, 1, 7); pixel(g, 13, 2, 9); pixel(g, 19, 2, 9) }
  return outlined(g)
}
function shotgunGrid(fire) {
  const g = blank(32, 16)
  rectfill(g, 9, 4, 23, 8, 5); rectfill(g, 9, 4, 23, 4, 6); rectfill(g, 9, 6, 23, 6, 16)  // twin barrels
  rectfill(g, 11, 8, 20, 10, 16)                             // pump
  rectfill(g, 12, 10, 20, 15, 4); rectfill(g, 13, 11, 15, 15, 20) // stock
  rectfill(g, 10, 9, 11, 14, 15); rectfill(g, 21, 9, 22, 14, 15)  // hands
  if (fire) {
    rectfill(g, 7, 2, 13, 5, 10); rectfill(g, 8, 3, 12, 4, 7)
    rectfill(g, 19, 2, 25, 5, 10); rectfill(g, 20, 3, 24, 4, 7); pixel(g, 16, 1, 9)
  }
  return outlined(g)
}

const [pIdleL, pIdleR] = split(pistolGrid(false))
const [pFireL, pFireR] = split(pistolGrid(true))
const [sIdleL, sIdleR] = split(shotgunGrid(false))
const [sFireL, sFireR] = split(shotgunGrid(true))

module.exports = {
  screenW: 320, screenH: 200, scale: 3, cellW: 16, cellH: 16,
  sprites: {
    0: imp(false), 1: imp(true), 2: impAtk(), 3: impDead(),
    4: gunner(false), 5: gunner(true), 6: gunnerAtk(), 7: gunnerDead(),
    8: keycard(), 9: medkit(), 10: armorPack(), 11: ammoBox(),
    12: faceOk(), 13: faceHurt(),
    16: pIdleL, 17: pIdleR, 18: pFireL, 19: pFireR,
    24: sIdleL, 25: sIdleR, 26: sFireL, 27: sFireR,
  },
}
