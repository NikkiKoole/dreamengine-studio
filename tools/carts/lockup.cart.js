// config for lockup.c — EVERY drawn sprite in the prison, plus the screen settings.
// Slot map is FROZEN in runtime/lockup/model.h; this file must obey it exactly.
//
//   row 0   0..7   person  S0 S1 S2  N0 N1 N2  E0 E1        (dir*3+stride)
//   row 1   8..15  person  E2  W0 W1 W2 · sleep down fight sit
//   row 2  16..23  floors  dirt grass gravel concrete concrete2 tile tile2 wood
//   row 3  24..31  bed(top) toilet sink showerhead bench serving cooker fridge
//   row 4  32..39  bed(bot) desk chair cabinet tv phone weights bookshelf
//   row 5  40..47  table(L) table(R) pool(TL) pool(TR) medbed locker light detector
//   row 6  48..55  asphalt gravel2 pool(BL) pool(BR) cctv bin bunk grass2
//   row 7  56..63  icons: alarm money tray contraband wrench key star skull
//
// ── PALETTE CONTRACT (pico32 indices) ────────────────────────────────────────
// The whole prison is built from ONE warm neutral ramp so a room reads as
// architecture rather than as a bag of coloured props. Accents are rationed:
// only food, blood, felt, book spines, hazard bands and the HUD icons get hue.
//
//   INK  16 brownish-black — every outline, the deepest shadow, drains, sockets
//   SHD  21 darker grey    — shadow step, asphalt base, cavities, grout dark
//   STL   5 dark grey      — THE body colour: steel, concrete, frames
//   TAN  22 medium grey    — worn/warm light step, blankets, dust, stone
//   LGT   6 light grey     — lit steel, porcelain body, tile face
//   WHT   7 white          — top-left highlight, porcelain, sheets, paper
//   SHN  13 indigo         — cool sheen on polished steel/glass, AND the one muted
//                            blue this palette owns, so it is also every BLANKET:
//                            bed, bunk and sleeping actor share it, which is what
//                            stops a top-down bed reading as a chest of drawers
//   WD_D 20 dark brown · WD_M 4 brown · WD_L 22 tan  — all wood, all furniture
//   GR_D  3 · GR_M 27 · GR_L 11 · GR_H 26            — grass / pool felt
//   GR_X 19 blue-green     — the DESATURATOR: this palette's greens are ferocious,
//                            and a yard of them is confetti. Grass is mottled with
//                            19 in 2×2 patches to pull the hue back to dull turf.
//   accents, sparingly: RED 8 · DRED 24 · ORG 9 · DORG 25 · YEL 10 · LYEL 23
//                       BLU 12 · DBLU 1 · PNK 14
//
// ── MAGIC INDICES (people only — pal()'d per role by the renderer) ───────────
//   28 uniform body · 29 trousers/dark accent · 30 skin · 26 hair
// The person sprites paint NO literal orange or blue: the role palette
// (LK_ROLE_UNIFORM / LK_ROLE_TROUSER) supplies it at draw time. 29 doubles as
// the torso's SHADOW side, which is why every role's trouser colour is a darker
// sibling of its uniform. Boots stay a fixed dark (21) for every role, so a
// white-uniformed cook still has feet.
// NOTE: 26 is also the grass highlight. Floors draw with the palette reset, so
// the two never collide — but never put 26 into an OBJECT sprite, because
// objects draw in the same pass as the people and it would flicker with hair.
//
// ── LIGHT + OUTLINE POLICY (the renderer's lighting model assumes it) ───────
// Light comes from the NORTH-WEST. Every object: highlight on its top/left edge,
// the dark ramp step on its bottom/right edge, then one 1px INK outline all
// round so it reads on dirt as well as on white tile. Floors are flat (no
// directional light) except tile grout and plank seams, which catch a lit top
// edge. Objects keep a 1px transparent margin where they can, so two adjacent
// objects never fuse into one blob — a body drawn out to the tile EDGE gets its
// outline ring pushed off the sprite and ends up fully opaque, which hides the floor
// and welds neighbours together (it did: fridge, bookshelf, bunk).
// Floor grain goes through grain(), NOT noise() — see the note on that function.
// And a two-variant floor has two rules of its own, both learned the hard way:
// the variants must share a BASE VALUE (gravel v2 sat a step darker and chequered
// the path) and neither may carry a feature longer than a few px (concrete v2's
// full-tile crack became regular diagonal hatching across the whole block, because
// with two variants every feature repeats every other tile).
//
// Iterate: node tools/sprite-preview.js lockup --scale 10 --out /tmp/lk.png
//     and: node tools/sprite-preview.js lockup --scale 3   (the real-size test)

const { blank, pixel, rectfill, rrectfill, line, circlefill, ovalfill, trifill,
        polyfill, noise, outlined, flat, split } = require('../sprite-draw.js')

// ── the ramp ─────────────────────────────────────────────────────────────────
const INK = 16, SHD = 21, STL = 5, TAN = 22, LGT = 6, WHT = 7, SHN = 13
const WD_D = 20, WD_M = 4, WD_L = 22
const GR_D = 3, GR_M = 27, GR_L = 11, GR_H = 26, GR_X = 19   // GR_X = blue-green
const RED = 8, DRED = 24, ORG = 9, DORG = 25, YEL = 10, LYEL = 23
const BLU = 12, DBLU = 1, PNK = 14
// magic recolour targets — PEOPLE ONLY
const UNI = 28, TRS = 29, SKN = 30, HAIR = 26
const BOOT = 21          // never recoloured: boots are boots

// mirror a whole canvas left↔right (sprite-draw's mirror() only folds one half)
function flipH(g) { return g.map(r => r.slice().reverse()) }

// ── grain(): the floor-texture hash ──────────────────────────────────────────
// sprite-draw's noise() is two FNV rounds over (x,y) with NO avalanche, so within
// a 16×16 patch its low bits stay correlated along diagonals: every small modulus
// laid visible corduroy across a big concrete slab, and because a floor tiles, the
// corduroy repeated at grid pitch. One xorshift-multiply finish kills it. Any
// per-pixel floor speckle goes through here; noise() is still fine for the
// once-per-tile scalar picks (which board splits, which spine is tall).
function grain(x, y, mod, salt = 0) {
  let h = (x * 374761393 + y * 668265263 + salt * 1274126177) >>> 0
  h = (h ^ (h >>> 13)) >>> 0
  h = Math.imul(h, 1274126177) >>> 0
  h = (h ^ (h >>> 16)) >>> 0
  return h % mod
}

// ═════════════════════════════════════════════════════════════════════════════
// PEOPLE — slots 0..15
// A flat top-down figure (the sensi/cannonfodder idiom): hair crown, a face brim
// on the FACING side so a glance tells you which way someone walks, chunky
// shoulders, sensi's parametric leg-length stride. Facing away (N) shows no skin
// at all — that read is what makes a queue of prisoners legible. The torso's
// east flank is painted in the trouser/accent index, which is every role's own
// darker sibling colour, so the figure has a lit side without a 5th magic index.
// ═════════════════════════════════════════════════════════════════════════════

// ── HOW BIG THE HEAD IS, is the whole role read ──────────────────────────────
// The first pass gave the head 10×5 px of HAIR and the torso 8×4 px of UNIFORM —
// i.e. MORE HAIR THAN UNIFORM — so from the map every prisoner read as a small dark
// brown speck and the role colour the palette works so hard to supply never got
// enough pixels to be seen. Top-down, the role lives in the SHOULDERS. So the head
// is now 6 wide over 4 rows (y1..y4) and the shoulders are 12–14 wide over 5 rows
// (y5..y9): 66 px of pure role colour against 22 of hair. Head and shoulders touch
// (no transparent row between) so outlined() rings them as ONE silhouette.
// d 0=S 1=N 2=E (W is E, flipped)
function lk_head(g, d) {
  rectfill(g, 6, 1, 9, 1, HAIR)                   // crown
  rectfill(g, 5, 2, 10, 2, HAIR)
  if (d === 1) {                                  // ── N: the back of the head
    rectfill(g, 5, 3, 10, 4, HAIR)
    rectfill(g, 6, 4, 9, 4, SHD)                  // nape shadow under the crown
  } else if (d === 0) {                           // ── S: facing the viewer
    rectfill(g, 5, 3, 10, 4, SKN)
    rectfill(g, 5, 3, 5, 4, HAIR)                 // hair frames the face
    rectfill(g, 10, 3, 10, 4, HAIR)
    pixel(g, 6, 4, INK); pixel(g, 9, 4, INK)      // eyes
  } else {                                        // ── E: profile, face to +x
    rectfill(g, 5, 3, 7, 4, HAIR)                 // back of the skull
    rectfill(g, 8, 3, 10, 4, SKN)                 // cheek + jaw
    pixel(g, 8, 3, HAIR)                          // fringe
    pixel(g, 11, 4, SKN)                          // nose
    pixel(g, 10, 4, INK)                          // eye
  }
}

// dir 0=S 1=N 2=E 3=W ; stride 0 = passing, 1 = left lead, 2 = right lead
function lk_person(dir, stride) {
  const g = blank()
  const d = dir === 3 ? 2 : dir                   // W is drawn as E then flipped
  const profile = d === 2

  if (!profile) {
    // ── legs: sensi's trick, a 2px length differential ──
    const ll = stride === 1 ? 14 : stride === 2 ? 12 : 13
    const rl = stride === 2 ? 14 : stride === 1 ? 12 : 13
    rectfill(g, 4, 12, 6, ll, TRS); rectfill(g, 4, ll, 6, ll, BOOT)
    rectfill(g, 9, 12, 11, rl, TRS); rectfill(g, 9, rl, 11, rl, BOOT)
    rectfill(g, 3, 10, 12, 11, TRS)               // hips
    // ── THE SHOULDERS: the single biggest block in the figure, all role colour.
    // Tapered 10 → 14 → 10 rather than filled square: a 12×5 rectangle of uniform
    // reads as a table seen from above, and the taper is what makes it a person.
    rectfill(g, 3, 5, 12, 5, UNI)                 // collar line
    rectfill(g, 1, 6, 14, 8, UNI)                 // shoulders, out to the sleeve caps
    rectfill(g, 3, 9, 12, 9, UNI)                 // waist
    rectfill(g, 12, 6, 13, 8, TRS)                // NW light → the east flank darkens
    pixel(g, 14, 8, TRS); pixel(g, 12, 9, TRS); pixel(g, 12, 5, TRS)
    const hl = stride === 1 ? 8 : 7
    const hr = stride === 2 ? 8 : 7
    pixel(g, 1, hl, SKN); pixel(g, 14, hr, SKN)   // hands swing counter to the legs
  } else {
    // ── profile: a narrower body, legs stagger fore/aft AND in length ──
    if (stride === 1) {
      rectfill(g, 8, 11, 10, 12, TRS); rectfill(g, 8, 13, 10, 13, BOOT)  // lead, lifted
      rectfill(g, 4, 11, 6, 13, TRS); rectfill(g, 4, 14, 6, 14, BOOT)    // trail, planted
    } else if (stride === 2) {
      rectfill(g, 7, 11, 9, 13, TRS); rectfill(g, 7, 14, 9, 14, BOOT)    // planted
      rectfill(g, 3, 11, 5, 11, TRS); rectfill(g, 3, 12, 5, 12, BOOT)    // trail, lifting
    } else {
      rectfill(g, 5, 11, 7, 13, TRS); rectfill(g, 5, 14, 7, 14, BOOT)
      rectfill(g, 8, 11, 10, 13, TRS); rectfill(g, 8, 14, 10, 14, BOOT)
    }
    rectfill(g, 4, 10, 11, 11, TRS)               // hips
    rectfill(g, 3, 5, 12, 9, UNI)                 // torso
    rectfill(g, 12, 6, 12, 9, TRS)                // shadow flank
    if (stride === 1) {                           // the arm swings forward
      rectfill(g, 13, 6, 13, 8, UNI); pixel(g, 13, 8, SKN)
    } else if (stride === 2) {                    // …and back
      rectfill(g, 2, 7, 2, 9, UNI); pixel(g, 2, 9, SKN)
    } else {
      rectfill(g, 13, 7, 13, 8, UNI); pixel(g, 13, 8, SKN)
    }
  }
  lk_head(g, d)
  const out = outlined(g, INK)
  return flat(dir === 3 ? flipH(out) : out)
}

// slot 12 — asleep: lying along the bed (beds run N–S), head on the pillow. Drawn
// OVER ob_bed, so it can't just be another blanket or the sleeper disappears into
// the bedding — the read is head + SHOULDERS in the role's own uniform colour,
// with the blanket pulled to the chest. No limbs at all = definitely not standing.
function lk_sleep() {
  const g = blank()
  rectfill(g, 6, 1, 9, 1, HAIR)                   // head on the pillow
  rectfill(g, 5, 2, 10, 2, HAIR)
  rectfill(g, 5, 3, 10, 4, SKN)                   // face up
  rectfill(g, 5, 3, 5, 4, HAIR); rectfill(g, 10, 3, 10, 4, HAIR)
  pixel(g, 6, 4, INK); pixel(g, 9, 4, INK)        // shut eyes
  rectfill(g, 3, 5, 12, 8, UNI)                   // shoulders + chest: the ROLE read
  rectfill(g, 12, 6, 12, 8, TRS)
  rectfill(g, 3, 5, 12, 5, TRS)                   // collar
  rrectfill(g, 4, 9, 8, 6, 2, BLANKET)            // the blanket, x4..11 y9..14
  rectfill(g, 4, 9, 11, 9, WHT)                   // turned-down sheet
  rectfill(g, 4, 10, 4, 14, LGT)                  // lit west edge
  rectfill(g, 11, 10, 11, 14, SHD)                // shadowed east edge
  rectfill(g, 5, 12, 10, 12, SHD)                 // a fold at the knees
  pixel(g, 5, 14, LGT); pixel(g, 6, 14, LGT)      // feet, under the blanket
  pixel(g, 9, 14, LGT); pixel(g, 10, 14, LGT)
  return flat(outlined(g, INK))
}

// slot 13 — down/injured. Every OTHER pose in this set stands upright on a N–S
// axis, so a body on the floor is drawn on the EAST–WEST axis: the 90° rotation is
// the read, not the anatomy, because at 16px a prone figure is a blob whichever way
// you draw it. Head west, limbs at four clearly separated angles so the silhouette
// is a sprawl rather than a lump, and a blood pool under the head.
function lk_down() {
  const g = blank()
  for (const [bx, by] of [[0, 10], [1, 11], [2, 12], [1, 9], [3, 12], [0, 12]])
    pixel(g, bx, by, DRED)                        // blood, under the head
  pixel(g, 1, 10, RED); pixel(g, 2, 11, RED)
  rectfill(g, 10, 4, 12, 5, TRS)                  // legs, splayed apart
  rectfill(g, 12, 3, 13, 5, BOOT)
  rectfill(g, 10, 10, 12, 11, TRS)
  rectfill(g, 12, 10, 13, 12, BOOT)
  rectfill(g, 9, 5, 11, 10, TRS)                  // hips
  rectfill(g, 5, 4, 10, 11, UNI)                  // torso, face down — 6×8 of role
  rectfill(g, 5, 11, 10, 11, TRS)                 // shadow along the low edge
  rectfill(g, 6, 2, 7, 3, UNI); pixel(g, 6, 1, SKN)      // one arm flung north
  rectfill(g, 7, 12, 8, 13, UNI); pixel(g, 7, 14, SKN)   // the other folded south
  rectfill(g, 3, 5, 5, 9, HAIR)                   // head, cheek to the floor
  rectfill(g, 4, 4, 5, 4, HAIR)
  pixel(g, 5, 8, SKN); pixel(g, 5, 9, SKN); pixel(g, 4, 10, SKN)
  return flat(outlined(g, INK))
}

// slot 14 — fighting: a lunge east. Weight forward, one fist out, stance braced.
function lk_fight() {
  const g = blank()
  rectfill(g, 3, 11, 4, 13, TRS); rectfill(g, 2, 13, 4, 14, BOOT)   // braced back leg
  rectfill(g, 9, 11, 11, 12, TRS); rectfill(g, 10, 12, 12, 13, BOOT) // lead leg
  rectfill(g, 4, 10, 10, 11, TRS)                 // hips, twisted
  rectfill(g, 3, 5, 11, 9, UNI)                   // torso, thrown east
  rectfill(g, 11, 6, 11, 9, TRS)
  rectfill(g, 11, 4, 12, 5, UNI)                  // the driving arm
  rectfill(g, 12, 3, 13, 5, SKN)                  // the fist
  pixel(g, 13, 3, WHT)                            // knuckle glint
  rectfill(g, 2, 6, 2, 8, UNI); pixel(g, 2, 8, SKN)   // rear arm, cocked
  rectfill(g, 5, 1, 8, 1, HAIR)                   // head, ducked in
  rectfill(g, 4, 2, 9, 3, HAIR)
  rectfill(g, 4, 4, 6, 4, HAIR)
  rectfill(g, 7, 4, 9, 4, SKN); pixel(g, 10, 3, SKN)
  pixel(g, 9, 4, INK)
  return flat(outlined(g, INK))
}

// slot 15 — sitting. The first pass drew this as a SIDE view, an L-shape, which was
// clever and wrong: it was the only figure in the set not on the same axis as the
// others, so on a bench it read as a fallen prisoner. This is the front figure with
// the lower body collapsed into ONE wide short block of knees and two feet — the
// standing frames have two narrow legs with a gap between them, so wide-and-short vs
// narrow-and-long is the whole cue, and the bench underneath supplies the rest.
function lk_sit() {
  const g = blank()
  rectfill(g, 4, 11, 11, 13, TRS)                 // thighs + knees, foreshortened
  rectfill(g, 4, 11, 11, 11, SHD)                 // the lap, in its own shadow
  rectfill(g, 7, 12, 8, 13, SHD)                  // the gap between the knees
  rectfill(g, 4, 14, 6, 14, BOOT)                 // feet on the floor
  rectfill(g, 9, 14, 11, 14, BOOT)
  rectfill(g, 2, 5, 13, 10, UNI)                  // torso, sat up
  rectfill(g, 13, 6, 13, 10, TRS)
  rectfill(g, 1, 6, 1, 9, UNI); pixel(g, 1, 9, SKN)       // arms, resting
  rectfill(g, 14, 6, 14, 9, UNI); pixel(g, 14, 9, SKN)
  lk_head(g, 0)
  return flat(outlined(g, INK))
}

// ═════════════════════════════════════════════════════════════════════════════
// FLOORS — slots 16..23, 48, 49, 55.  Fully opaque, never index 0.
// Every material gets a DIFFERENT grain density AND a different structure
// (mottle / blades / stones / speckle / grout / planks) so a change of surface
// reads at a glance even in monochrome. Variants break grid repetition.
// ═════════════════════════════════════════════════════════════════════════════

// ── THE ONE RULE FOR GROUND SPRITES, learned by shipping the opposite ────────
// A floor sprite is 16×16 and it repeats IDENTICALLY on every tile, so ANY feature
// you can pick out of it at 1× — a stone, a clump, a bright fleck — comes back as a
// regular lattice of that feature across the whole map. That is what made the first
// exterior read as visual damage rather than terrain. So a ground sprite carries
// ONLY fine, statistically flat, LOW-CONTRAST mottle (one value step, never two),
// and everything a player is supposed to actually SEE lives in the renderer, where
// it can vary per tile and per AREA:
//   · per-tile features (pebbles, tufts, clods)  → art.h lkr_floor_detail()
//   · area-scale dry/damp variation, 5-tile scale → art.h lkr_ground_pass()
//   · the interlock where two materials meet      → art.h lkr_floor_edges()
// Keep the sprites boring. The renderer makes the ground interesting.

// dirt — unimproved institutional land: packed buff earth, and about two thirds of
// the map, so it has to be the calmest thing in the frame. The first pass based it
// on 20 (#742f29) churned with 4 (#ab5236) — i.e. BRICK RED — which fought the
// actual brick walls, over-saturated the whole exterior and put a fixed lattice of
// black stones on every tile. Now it is 5 mottled with 22 in 2×2 patches: two warm
// neutrals ONE value step apart that average out to a khaki, nothing else.
function fl_dirt() {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, STL)
  for (let y = 0; y < 16; y += 2)
    for (let x = 0; x < 16; x += 2) {
      const n = grain(x, y, 8)
      if (n === 0) rectfill(g, x, y, x + 1, y + 1, TAN)        // a dry buff patch
      else if (n === 1) { pixel(g, x + 1, y, TAN); pixel(g, x, y + 1, TAN) }
      else if (n === 2) pixel(g, x, y, TAN)
      else if (n === 3) pixel(g, x + 1, y + 1, SHD)            // one damp speck
    }
  return flat(g)
}

// grass — the exercise yard and the waste ground. This palette has NO olive: 3, 27,
// 11 and 26 are all ferocious. So turf is MIXED rather than picked — 3 (#008751)
// dithered against 5 (#5f574f, the warm neutral) and 19 (#125359, the cool one) in
// 2×2 patches averages out to about #217255, a dull institutional green, and the
// only saturated green left is a ~3% scatter of 27 as blade tips. 11 and 26 are gone
// entirely: they were the confetti, and a lawn of them is an arcade, not a prison.
// The standing tufts moved to the renderer, because on a repeating sprite a tuft at a
// fixed pixel becomes a regular grid of tufts.
function fl_grass(v) {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, GR_D)
  const s = v ? 11 : 0
  for (let y = 0; y < 16; y += 2)
    for (let x = 0; x < 16; x += 2) {
      const n = grain(x, y, 5, s)
      if (n === 0) rectfill(g, x, y, x + 1, y + 1, STL)         // the desaturator
      else if (n === 1) { pixel(g, x, y, STL); pixel(g, x + 1, y + 1, STL) }
      else if (n === 2) { pixel(g, x + 1, y, GR_X); pixel(g, x, y + 1, GR_X) }
      else if (n === 3) { pixel(g, x, y, TAN); pixel(g, x + 1, y + 1, STL) }  // dry stems
    }
  for (let y = 0; y < 16; y++)                                  // blade tips, rationed
    for (let x = 0; x < 16; x++)
      if (grain(x, y, 47, s + 1) === 0) g[y][x] = GR_M
  return flat(g)
}

// gravel — the yard surface: compacted crushed stone. Calmed hard from the first
// pass, which gave 3 of every 7 patches a pale stone carrying BOTH a #c2c3c7
// highlight and a black pit — 40% of the tile at full palette contrast, which
// shimmered and read as broken paving. Now ONE value step (5 → 22) does all of it and
// there is no #c2c3c7 in the sprite at all: the single lit stone per tile that gives
// the surface its sparkle comes from art.h's lkr_floor_detail, where it can move.
// (v2 is model.h slot 49; LK_FLOOR_SPR uses it as gravel's SECOND variant, so it
//  has to read as coarser aggregate, not as smooth concrete. BOTH variants share
//  the base value: v2 used to sit on SHD, and alternating a dark tile with a light
//  one made a gravel path look chequered.)
function fl_gravel(v) {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, STL)
  for (let y = 0; y < 16; y += 2)
    for (let x = 0; x < 16; x += 2) {
      const n = grain(x, y, v ? 7 : 8, v ? 5 : 0)
      if (n === 0) {                                           // a stone
        rectfill(g, x, y, x + 1, y + 1, TAN)
      } else if (n === 1) {                                    // a smaller one
        pixel(g, x, y, TAN); pixel(g, x + 1, y + 1, SHD)
      } else if (n === 2) {                                    // a pit
        pixel(g, x + 1, y, SHD)
      } else if (n === 3 && v) {                               // a coarse lump
        rectfill(g, x, y, x + 1, y + 1, TAN); pixel(g, x + 1, y + 1, SHD)
      }
    }
  return flat(g)
}

// concrete — poured slab. The finest grain in the set; v2 is warmer and more worn
// so a big slab breaks up. The first pass gave v2 a full-tile diagonal CRACK, and
// because a two-variant floor repeats every other tile that crack became regular
// diagonal hatching across the whole block. So v2's damage is now LOCAL: a short
// chip and a scuff patch, no feature long enough to line up with its neighbours.
function fl_concrete(v) {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, STL)
  // 1-in-31, not the old 1-in-19: #c2c3c7 on #5f574f is nearly the widest value jump
  // in this palette, so at the old density an indoor slab fizzed exactly the way the
  // exterior did. The speckle is meant to be a whisper you notice only up close.
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++) {
      if (grain(x, y, v ? 23 : 31, v ? 7 : 0) === 0) g[y][x] = v ? TAN : LGT
      else if (grain(x, y, v ? 19 : 25, v ? 8 : 1) === 0) g[y][x] = SHD
    }
  if (v) {
    line(g, 9, 4, 12, 7, SHD)                                  // a short chip
    pixel(g, 10, 5, INK)
    for (const [px, py] of [[3, 10], [4, 10], [5, 11], [3, 11], [4, 12], [2, 11]])
      pixel(g, px, py, TAN)                                    // a scuffed patch
    pixel(g, 3, 10, LGT); pixel(g, 5, 11, SHD)
  }
  return flat(g)
}

// tile — real grout. 8×8 tiles on a lattice that stays continuous ACROSS cells
// (tile floors should look tiled), each face lit along its top edge.
function fl_tile(v) {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, LGT)
  for (const [ox, oy] of [[0, 0], [8, 0], [0, 8], [8, 8]]) {
    rectfill(g, ox + 1, oy + 1, ox + 7, oy + 1, WHT)           // lit top edge
    pixel(g, ox + 1, oy + 2, WHT)
    rectfill(g, ox + 7, oy + 2, ox + 7, oy + 7, TAN)           // SE shade
    rectfill(g, ox + 2, oy + 7, ox + 7, oy + 7, TAN)
  }
  for (let i = 0; i < 16; i++) {
    pixel(g, 0, i, STL); pixel(g, 8, i, STL)                   // grout
    pixel(g, i, 0, STL); pixel(g, i, 8, STL)
  }
  for (let y = 1; y < 16; y++)
    for (let x = 1; x < 16; x++)
      if (x !== 8 && y !== 8 && grain(x, y, v ? 29 : 53, v ? 4 : 0) === 0)
        g[y][x] = TAN                                          // scuffs
  if (v) {
    rectfill(g, 9, 9, 15, 15, TAN)                             // one dulled tile
    rectfill(g, 9, 9, 15, 9, LGT)
    for (const [px, py] of [[11, 12], [12, 12], [12, 13], [13, 13]])
      pixel(g, px, py, STL)                                    // …chipped, not cracked
    pixel(g, 11, 11, WHT)
  }
  return flat(g)
}

// wood — boards running east–west, each with its own tone and lengthwise grain.
// One end-joint per tile only: scatter more and the whole floor reads as brick.
function fl_wood() {
  const g = blank()
  const tone = [WD_M, WD_M, WD_D, WD_M]
  const joint = noise(3, 7, 4)                                 // which board splits
  for (let p = 0; p < 4; p++) {
    const y = p * 4
    rectfill(g, 0, y, 15, y + 3, tone[p])
    rectfill(g, 0, y, 15, y, WD_D)                             // seam between boards
    rectfill(g, 0, y + 1, 15, y + 1, p === 2 ? WD_M : WD_L)    // lit top of the board
    for (let x = 0; x < 16; x++) {                             // lengthwise grain
      if (grain(x, p, 3) === 0) pixel(g, x, y + 2, WD_D)
      if (grain(x, p, 5, 1) === 0) pixel(g, x, y + 3, p === 2 ? WD_D : WD_L)
      if (grain(x, p, 7, 2) === 0) pixel(g, x, y + 2, WD_L)
    }
    if (p === joint) {                                         // one end joint
      const j = 4 + noise(p, 2, 8)
      rectfill(g, j, y + 1, j, y + 3, WD_D)
      pixel(g, j + 1, y + 1, WD_L)
    }
  }
  return flat(g)
}

// asphalt — the darkest surface: the public road, the delivery apron. 21 (#49333b) on
// its own is PLUM, which read as a purple road, so half the tile is dithered up to 5
// to land on a neutral dark warm grey (~#544545). Bright chips are gone: a #c2c3c7
// fleck on a near-black base is the highest-contrast pixel pair in the palette and it
// twinkled at 1×.
function fl_asphalt() {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, SHD)
  for (let y = 0; y < 16; y += 2)
    for (let x = 0; x < 16; x += 2) {
      const n = grain(x, y, 3)
      if (n === 0) rectfill(g, x, y, x + 1, y + 1, STL)
      else if (n === 1) { pixel(g, x + 1, y, STL); pixel(g, x, y + 1, STL) }
    }
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++) {
      if (grain(x, y, 41, 2) === 0) g[y][x] = TAN              // one pale aggregate
      else if (grain(x, y, 19, 3) === 0) g[y][x] = INK         // tar
    }
  return flat(g)
}

// ═════════════════════════════════════════════════════════════════════════════
// OBJECTS — slots 24..54.  Top-down, lit from the NORTH-WEST, 1px INK outline.
// Each one has to be nameable at 16px with no label. That is the whole test.
// ═════════════════════════════════════════════════════════════════════════════

// a plate with a lit top/left edge and a dark bottom/right edge
function plate(g, x0, y0, x1, y1, body, lit, dark) {
  rectfill(g, x0, y0, x1, y1, body)
  rectfill(g, x0, y0, x1, y0, lit)
  rectfill(g, x0, y0, x0, y1, lit)
  rectfill(g, x0, y1, x1, y1, dark)
  rectfill(g, x1, y0 + 1, x1, y1, dark)
}

// ── bed, 1×2 (slots 24 top + 32 bottom) ─────────────────────────────────────
// The hardest object in the set: a bed seen from directly above is a rectangle,
// and the first two passes both read as a CHEST OF DRAWERS in the cell — grey
// frame, grey sheet, grey blanket, three horizontal bands. What fixes it is
// VALUE + HUE separation between the three zones a bed has and nothing else does:
// a white pillow at the head, a white turned-down sheet cuff, and then two thirds
// of the object in ONE flat institutional-blue blanket (index 13, the only muted
// blue this palette owns). Plus visible steel side rails and a foot rail, so the
// silhouette has a frame around the bedding instead of being one solid slab.
const BLANKET = SHN                                  // 13 indigo — institutional
function ob_bed() {
  const g = blank(16, 32)
  rectfill(g, 2, 1, 13, 30, STL)                     // frame
  rectfill(g, 3, 3, 12, 28, LGT)                     // sheeted mattress
  rrectfill(g, 4, 3, 8, 6, 1, WHT)                   // pillow, x4..11 y3..8
  rectfill(g, 5, 5, 10, 5, LGT)                      // pillow crease
  rectfill(g, 4, 8, 11, 8, TAN)                      // pillow's shadow
  rectfill(g, 3, 10, 12, 12, WHT)                    // turned-down sheet
  rectfill(g, 3, 12, 12, 12, LGT)
  rectfill(g, 3, 13, 12, 28, BLANKET)                // the blanket
  rectfill(g, 3, 13, 12, 13, LGT)                    // its lit top fold
  rectfill(g, 3, 14, 3, 28, LGT)                     // lit west edge
  rectfill(g, 12, 14, 12, 28, SHD)                   // shadowed east edge
  rectfill(g, 4, 19, 11, 19, SHD)                    // folds
  rectfill(g, 4, 24, 11, 24, SHD)
  rectfill(g, 5, 26, 10, 27, LGT)                    // feet tenting the blanket
  pixel(g, 7, 26, BLANKET); pixel(g, 8, 26, BLANKET)
  rectfill(g, 2, 1, 13, 2, STL)                      // head rail
  rectfill(g, 2, 1, 13, 1, LGT)
  rectfill(g, 2, 29, 13, 30, STL)                    // foot rail
  rectfill(g, 2, 30, 13, 30, INK)
  rectfill(g, 2, 3, 2, 28, TAN)                      // side rails: lit / shadowed
  rectfill(g, 13, 3, 13, 28, SHD)
  return split(outlined(g, INK))                     // → [top, bottom]
}

// ── bunk bed (slot 54) — two levels, the lower one in shadow, plus a ladder
function ob_bunk() {
  const g = blank()
  rectfill(g, 1, 1, 14, 13, STL)                     // y13: keeps a transparent margin
  rectfill(g, 1, 1, 14, 1, LGT)
  rectfill(g, 5, 2, 13, 8, LGT)                      // UPPER bunk, sheeted
  rrectfill(g, 5, 2, 9, 3, 1, WHT)                   // pillow
  rectfill(g, 5, 5, 13, 5, WHT)                      // turned-down sheet
  rectfill(g, 5, 6, 13, 8, BLANKET)                  // blanket — same as ob_bed's
  rectfill(g, 5, 6, 5, 8, LGT); rectfill(g, 13, 6, 13, 8, SHD)
  rectfill(g, 5, 9, 13, 9, INK)                      // the deck between bunks
  rectfill(g, 5, 10, 13, 13, SHD)                    // LOWER bunk, in shadow
  rrectfill(g, 5, 10, 9, 2, 1, TAN)                  // its pillow, dimmed
  rectfill(g, 5, 12, 13, 13, BLANKET)                // its blanket, unlit
  rectfill(g, 5, 12, 13, 12, SHD)
  rectfill(g, 13, 10, 13, 13, INK)
  rectfill(g, 1, 2, 3, 13, SHD)                      // ladder
  rectfill(g, 1, 2, 1, 13, STL); rectfill(g, 3, 2, 3, 13, STL)
  for (let y = 3; y < 13; y += 3) { rectfill(g, 1, y, 3, y, LGT); pixel(g, 3, y, TAN) }
  return flat(outlined(g, INK))
}

// ── toilet (slot 25) — the first pass was a white oval with a dark middle inside a
//    pale box, which at 16px is the digit 0, and five of them down a cell block read
//    as five crude circles. A toilet seen from directly above is FOUR concentric
//    rings — porcelain rim, open seat, bowl wall, water — hung off a squarer cistern
//    at the back, and it is the ring COUNT that names it. So: a real rim (bright, and
//    brightest on its north-west), the seat opening stepped down through two shades
//    into near-black water, seat bumpers where the lid hinges, and the whole pan
//    PEAR-shaped — narrower at the front lip than at the hinge — which is the outline
//    difference between a toilet and a sink's round basin in a square surround.
function ob_toilet() {
  const g = blank()
  plate(g, 5, 1, 10, 3, LGT, WHT, TAN)               // cistern: a small BOX, and
  rectfill(g, 6, 2, 9, 2, WHT)                       // narrower than the pan, so the
  pixel(g, 10, 2, STL)                               // silhouette steps twice
  rectfill(g, 7, 4, 8, 4, TAN)                       // the spud, cistern into pan —
                                                     // row 4 is otherwise EMPTY so
                                                     // outlined() inks a seam there
  ovalfill(g, 8, 9.8, 4.3, 4.6, LGT)                 // 1. the porcelain rim
  ovalfill(g, 7.3, 9.0, 3.6, 3.8, WHT)               //    …lit on its north-west
  ovalfill(g, 8, 10.2, 2.9, 3.4, TAN)                // 2. the open seat, shaded
  ovalfill(g, 8, 10.4, 2.2, 2.7, SHD)                // 3. the bowl wall
  ovalfill(g, 8, 10.6, 1.5, 2.0, INK)                // 4. water
  pixel(g, 7, 10, STL)                               // one reflected glint
  pixel(g, 5, 12, LGT); pixel(g, 11, 12, SHD)
  pixel(g, 5, 13, TAN); pixel(g, 10, 13, SHD)        // the front lip, narrowing
  return flat(outlined(g, INK))
}

// ── sink (slot 26) — ROUND basin in a SQUARE surround, against the toilet's egg on
//    a stalk: the two are the objects most often side by side in a cell, so they are
//    told apart by silhouette, not by detail. The first pass built the whole thing
//    out of mid-greys and read as a box with a hole; the surround is now bright
//    porcelain so the darker bowl inside it is unmistakably a basin.
function ob_sink() {
  const g = blank()
  rectfill(g, 6, 1, 9, 3, STL)                       // tap body
  rectfill(g, 6, 1, 9, 1, LGT)
  rectfill(g, 7, 3, 8, 5, LGT); pixel(g, 7, 3, WHT)  // spout, over the bowl
  pixel(g, 5, 2, BLU); pixel(g, 10, 2, RED)          // cold / hot
  pixel(g, 5, 3, SHD); pixel(g, 10, 3, SHD)
  rrectfill(g, 2, 4, 12, 10, 3, WHT)                 // surround, x2..13 y4..13
  rectfill(g, 3, 13, 13, 13, TAN)                    // shaded south/east lip
  rectfill(g, 13, 5, 13, 13, TAN)
  ovalfill(g, 8, 9, 4.2, 3.4, LGT)                   // the bowl
  ovalfill(g, 8, 9.3, 3.4, 2.6, TAN)
  ovalfill(g, 7.4, 8.6, 2.6, 2, LGT)                 // lit NW inner wall
  pixel(g, 8, 10, INK); pixel(g, 9, 10, INK)         // drain
  pixel(g, 3, 5, WHT); pixel(g, 12, 12, TAN)
  return flat(outlined(g, INK))
}

// ── shower head (slot 27) — not solid, so prisoners stand IN it. Rose + pipe to the
//    north, then the SPRAY widening south, which is what actually names the object;
//    the first pass drew a floor drain instead and read as a dark smudge.
//    ⚠ The droplets are stamped AFTER outlined(), on purpose: outlined() inks every
//    transparent pixel touching content, so an isolated 1px droplet passed through it
//    comes out as a 3×3 black square with a blue middle. It did. Order matters here.
function ob_shower() {
  const g = blank()
  rectfill(g, 7, 0, 8, 3, STL)                       // pipe
  pixel(g, 7, 0, LGT); pixel(g, 7, 1, LGT)
  ovalfill(g, 8, 5, 4.2, 2.4, LGT)                   // the rose
  rectfill(g, 5, 4, 10, 4, WHT)
  pixel(g, 4, 4, LGT); pixel(g, 12, 6, SHD)
  for (const x of [5, 7, 9, 11]) pixel(g, x, 6, INK) // nozzles
  const o = outlined(g, INK)
  for (const [dx, dy] of [[8, 8], [6, 9], [10, 9], [7, 11], [9, 11],   // the spray
                          [5, 12], [11, 12], [8, 13], [4, 14], [12, 14],
                          [6, 15], [10, 15]])
    pixel(o, dx, dy, BLU)
  pixel(o, 8, 8, WHT); pixel(o, 7, 11, WHT); pixel(o, 11, 12, SHN)
  return flat(o)
}

// ── bench (slot 28) — two wooden slats with an air gap, on steel legs
function ob_bench() {
  const g = blank()
  rectfill(g, 1, 4, 14, 7, WD_M)                     // near slat
  rectfill(g, 1, 4, 14, 4, WD_L)
  rectfill(g, 1, 7, 14, 7, WD_D)
  rectfill(g, 1, 9, 14, 12, WD_M)                    // far slat
  rectfill(g, 1, 9, 14, 9, WD_L)
  rectfill(g, 1, 12, 14, 12, WD_D)
  for (let x = 2; x < 15; x += 4) { pixel(g, x, 5, WD_D); pixel(g, x + 1, 11, WD_D) }
  rectfill(g, 2, 8, 3, 8, STL); rectfill(g, 12, 8, 13, 8, STL)   // frame between slats
  rectfill(g, 2, 13, 3, 14, STL); rectfill(g, 12, 13, 13, 14, STL)  // legs
  pixel(g, 3, 14, SHD); pixel(g, 13, 14, SHD)
  return flat(outlined(g, INK))
}

// ── serving table (slot 29) — steel counter, hot food wells, a tray stack
function ob_serving() {
  const g = blank()
  plate(g, 1, 3, 14, 12, STL, LGT, INK)
  rectfill(g, 2, 4, 13, 4, WHT)                      // polished top edge
  for (let i = 0; i < 3; i++) {
    const x = 2 + i * 3
    rectfill(g, x, 6, x + 2, 10, INK)                // the well
    rectfill(g, x, 7, x + 2, 9, i === 1 ? LYEL : i === 0 ? DORG : GR_M)
    rectfill(g, x, 7, x + 2, 7, i === 1 ? WHT : i === 0 ? ORG : GR_L)
    pixel(g, x + 2, 9, SHD)
  }
  rectfill(g, 11, 6, 13, 10, SHN)                    // clean trays, stacked
  rectfill(g, 11, 6, 13, 6, WHT)
  rectfill(g, 11, 8, 13, 8, LGT)
  rectfill(g, 1, 12, 14, 12, SHD)
  return flat(outlined(g, INK))
}

// ── cooker (slot 30) — a light stainless top so the four burners read as holes
function ob_cooker() {
  const g = blank()
  plate(g, 1, 1, 14, 13, LGT, WHT, TAN)
  for (const [cx, cy, r, hot] of [[4.5, 4.5, 2.8, 0], [11, 4.5, 2.8, 1],
                                  [4.5, 9, 2.4, 0], [11, 9, 2.4, 0]]) {
    circlefill(g, cx, cy, r, TAN)
    circlefill(g, cx, cy, r - 0.8, INK)
    if (hot) {
      circlefill(g, cx, cy, r - 1.6, DRED)
      circlefill(g, cx, cy, r - 2.4, RED)
      pixel(g, cx - 1, cy - 1, ORG)
    } else {
      circlefill(g, cx, cy, r - 2, SHD)
    }
    pixel(g, cx - r, cy - 1, WHT)                    // rim glint
  }
  rectfill(g, 1, 11, 14, 11, TAN)                    // control fascia
  rectfill(g, 1, 12, 14, 13, STL)
  for (let i = 0; i < 4; i++) {
    pixel(g, 3 + i * 3, 12, LGT); pixel(g, 3 + i * 3, 13, INK)
  }
  return flat(outlined(g, INK))
}

// ── fridge (slot 31) — white cabinet, a long vertical handle, no vents
function ob_fridge() {
  const g = blank()
  plate(g, 1, 1, 14, 13, LGT, WHT, TAN)              // y13, not y14: outlined() adds a
  rectfill(g, 2, 2, 13, 3, WHT)                      // ring, and a body reaching the
  rectfill(g, 2, 5, 13, 5, TAN)                      // tile edge leaves NO transparent
  rectfill(g, 2, 6, 13, 6, WHT)                      // margin, so the floor stops
  rectfill(g, 10, 8, 12, 12, TAN)                    // showing and two fridges fuse
  rectfill(g, 11, 8, 12, 12, STL)                    // into one grey slab
  rectfill(g, 11, 8, 11, 12, SHN)                    // ← handle recess
  pixel(g, 12, 12, INK)
  rectfill(g, 3, 3, 4, 4, WHT)                       // a badge
  rectfill(g, 3, 11, 7, 12, TAN)                     // shadow at the base
  return flat(outlined(g, INK))
}

// ── desk (slot 33) — wood top, paperwork, a screen at the far edge
function ob_desk() {
  const g = blank()
  rectfill(g, 1, 2, 14, 13, WD_M)
  rectfill(g, 1, 2, 14, 2, WD_L)
  rectfill(g, 1, 2, 1, 13, WD_L)
  rectfill(g, 1, 13, 14, 13, WD_D)
  rectfill(g, 14, 3, 14, 13, WD_D)
  for (let x = 2; x < 14; x += 3) pixel(g, x, 7 + (x % 3), WD_D)     // grain
  rectfill(g, 8, 3, 13, 6, INK)                      // monitor
  rectfill(g, 9, 4, 12, 5, DBLU); pixel(g, 9, 4, SHN)
  rectfill(g, 2, 8, 6, 12, WHT)                      // paperwork
  rectfill(g, 3, 9, 5, 9, LGT); rectfill(g, 3, 11, 5, 11, LGT)
  pixel(g, 6, 12, TAN)
  rectfill(g, 11, 9, 13, 12, WD_D)                   // drawer
  rectfill(g, 11, 9, 13, 9, WD_L)
  pixel(g, 12, 11, LGT)
  return flat(outlined(g, INK))
}

// ── chair (slot 34) — a top-down chair is the easiest object in the set to get
//    wrong: a filled rectangle with a line across it IS a small chest of drawers,
//    which is what the first pass drew. Three fixes, all silhouette rather than
//    detail: the backrest is a THIN bar of full width, the seat is a rounded square
//    NARROWER than the bar (so the outline steps in twice), and four legs poke out
//    past the seat corners. Steel frame + wooden seat also keeps it a different
//    MATERIAL from the bench, which is the object it competes with.
function ob_chair() {
  const g = blank()
  for (const [lx, ly] of [[4, 12], [10, 12]]) {      // front legs, past the seat
    rectfill(g, lx, ly, lx + 1, ly + 2, STL)
    pixel(g, lx, ly, LGT); pixel(g, lx + 1, ly + 2, INK)
  }
  rectfill(g, 3, 2, 12, 4, WD_M)                     // backrest, full width
  rectfill(g, 3, 2, 12, 2, WD_L)
  rectfill(g, 3, 4, 12, 4, WD_D)
  pixel(g, 3, 3, WD_L); pixel(g, 12, 3, WD_D)
  rectfill(g, 3, 5, 4, 6, STL); rectfill(g, 11, 5, 12, 6, STL)   // back uprights
  rrectfill(g, 5, 6, 7, 6, 2, WD_M)                  // seat, x5..11 y6..11
  rectfill(g, 5, 6, 11, 6, WD_L)                     // lit north lip
  rectfill(g, 5, 6, 5, 11, WD_L)
  rectfill(g, 6, 11, 11, 11, WD_D)                   // shaded south lip
  rectfill(g, 11, 7, 11, 11, WD_D)
  pixel(g, 7, 8, WD_D); pixel(g, 9, 9, WD_L)         // grain
  return flat(outlined(g, INK))
}

// ── cabinet (slot 35) — wooden drawers with pull handles
function ob_cabinet() {
  const g = blank()
  rectfill(g, 2, 1, 13, 14, WD_M)
  rectfill(g, 2, 1, 13, 1, WD_L)
  rectfill(g, 2, 1, 2, 14, WD_L)
  rectfill(g, 2, 14, 13, 14, WD_D)
  rectfill(g, 13, 2, 13, 14, WD_D)
  for (let i = 0; i < 3; i++) {
    const y = 3 + i * 4
    rectfill(g, 3, y, 12, y, WD_D)                   // drawer seam
    rectfill(g, 3, y + 1, 12, y + 1, WD_L)
    rectfill(g, 6, y + 2, 9, y + 2, STL)             // pull handle
    pixel(g, 6, y + 2, LGT)
  }
  return flat(outlined(g, INK))
}

// ── television (slot 36) — bezel to the north, a lit face toward the room
function ob_tv() {
  const g = blank()
  rectfill(g, 3, 2, 12, 5, SHD)                      // cabinet back
  rectfill(g, 3, 2, 12, 2, STL)
  rectfill(g, 1, 5, 14, 12, INK)                     // bezel
  rectfill(g, 2, 6, 13, 6, STL)
  rectfill(g, 2, 7, 13, 11, BLU)                     // screen
  rectfill(g, 2, 7, 13, 7, SHN)
  rectfill(g, 3, 8, 7, 8, WHT)                       // a band of picture
  rectfill(g, 2, 10, 13, 10, DBLU)
  pixel(g, 12, 11, DBLU); pixel(g, 3, 9, LYEL)
  rectfill(g, 6, 13, 9, 14, SHD)                     // stand
  pixel(g, 6, 14, STL)
  pixel(g, 13, 12, RED)                              // standby lamp
  return flat(outlined(g, INK))
}

// ── phone (slot 37) — a wall payphone. The first pass hid the handset inside the
//    body's own grey and read as a grey box with lines on it; here the handset hangs
//    OUTSIDE the body's silhouette on the west, as a bold dark bar with two thick
//    ends, and the keypad sits on a black faceplate so the nine dots actually pop.
function ob_phone() {
  const g = blank()
  rectfill(g, 4, 1, 12, 13, STL)                     // body
  rectfill(g, 4, 1, 12, 1, LGT)
  rectfill(g, 4, 1, 4, 13, LGT)
  rectfill(g, 4, 13, 12, 13, INK)
  rectfill(g, 12, 2, 12, 13, SHD)
  rectfill(g, 6, 3, 11, 9, INK)                      // faceplate
  for (let r = 0; r < 3; r++)                        // keypad
    for (let c = 0; c < 3; c++) pixel(g, 7 + c * 2, 4 + r * 2, LGT)
  rectfill(g, 7, 11, 10, 12, TAN)                    // coin return
  rectfill(g, 8, 11, 9, 11, INK)
  rectfill(g, 1, 3, 3, 4, INK)                       // handset: earpiece…
  rectfill(g, 1, 8, 3, 9, INK)                       // …mouthpiece…
  rectfill(g, 2, 5, 3, 7, INK)                       // …and the grip between them
  pixel(g, 1, 3, LGT); pixel(g, 2, 5, STL); pixel(g, 1, 9, SHD)
  line(g, 3, 10, 4, 12, STL)                         // cord, back to the body
  pixel(g, 3, 11, SHD)
  return flat(outlined(g, INK))
}

// ── weights (slot 38) — three of these stand in the bare yard, and in the first pass
//    the plates were dark RECTANGLES, so each one read as a brown crate dumped on the
//    gravel: the objects a critic called "unexplained brown boxes outside". A weight
//    stack is named by ROUND plates on a straight bar, so the plates are now discs,
//    the bar runs clear across the tile east–west, and the bench pad runs north–south
//    under it. Cross + two circles: unmistakable at 16px, and it needs to be, because
//    it is out in the open with no room around it to explain it.
function ob_weights() {
  const g = blank()
  rectfill(g, 6, 2, 9, 14, SHD)                      // bench pad, running N–S
  rectfill(g, 6, 2, 9, 2, STL)
  rectfill(g, 6, 2, 6, 14, STL)                      // lit west edge
  rectfill(g, 9, 3, 9, 14, INK)                      // shaded east edge
  rectfill(g, 7, 10, 8, 10, INK)                     // pad seam
  rectfill(g, 1, 6, 14, 7, STL)                      // the bar, across the pad
  rectfill(g, 1, 6, 14, 6, LGT)
  rectfill(g, 5, 5, 5, 8, STL); rectfill(g, 10, 5, 10, 8, STL)   // collars
  pixel(g, 5, 5, LGT); pixel(g, 10, 8, INK)
  for (const cx of [3, 12]) {                        // the plates: DISCS, not crates
    circlefill(g, cx, 6.5, 2.9, INK)
    circlefill(g, cx, 6.5, 2.1, STL)
    circlefill(g, cx - 0.7, 5.8, 1.2, TAN)           // lit north-west of the disc
    pixel(g, cx, 6, LGT)
    pixel(g, cx + 1, 8, INK)
  }
  return flat(outlined(g, INK))
}

// ── bookshelf (slot 39) — spines from above: the one licence for colour
function ob_bookshelf() {
  const g = blank()
  rectfill(g, 1, 1, 14, 13, WD_D)                    // case (y13: keeps a margin)
  rectfill(g, 1, 1, 14, 1, WD_M)
  rectfill(g, 2, 2, 13, 12, INK)                     // interior
  const spines = [DRED, ORG, GR_D, DBLU, SHN, WD_M, DRED, GR_M, BLU, PNK, LYEL, STL]
  for (let i = 0; i < 12; i++) {
    const x = 2 + i, top = 3 + noise(i, 5, 3)
    rectfill(g, x, top, x, 11, spines[i])
    pixel(g, x, top, LYEL)                           // lit top of each spine
  }
  rectfill(g, 2, 12, 13, 12, SHD)                    // shelf shadow
  return flat(outlined(g, INK))
}

// ── table, 2×1 (slots 40 L + 41 R) — ONE surface. No centre seam: with one it
//    reads as two benches pushed together, which is what the first pass did.
function ob_table() {
  const g = blank(32, 16)
  rectfill(g, 1, 2, 30, 12, WD_M)
  rectfill(g, 1, 2, 30, 3, WD_L)                     // lit north rim
  rectfill(g, 1, 2, 2, 12, WD_L)
  rectfill(g, 1, 12, 30, 12, WD_D)                   // shadowed south rim
  rectfill(g, 29, 3, 30, 12, WD_D)
  for (let x = 3; x < 29; x++) {                     // lengthwise grain
    if (noise(x, 1, 3) === 0) pixel(g, x, 5, WD_D)
    if (noise(x, 2, 4) === 0) pixel(g, x, 8, WD_L)
    if (noise(x, 3, 3) === 0) pixel(g, x, 10, WD_D)
    if (noise(x, 4, 7) === 0) pixel(g, x, 6, WD_L)
  }
  rectfill(g, 2, 13, 4, 15, WD_D); rectfill(g, 27, 13, 29, 15, WD_D)   // legs
  rectfill(g, 2, 13, 4, 13, WD_M); rectfill(g, 27, 13, 29, 13, WD_M)
  pixel(g, 4, 15, INK); pixel(g, 29, 15, INK)
  return split(outlined(g, INK))                     // → [left, right]
}

// ── pool table, 2×2 (slots 42 TL, 43 TR, 50 BL, 51 BR) ─────────────────────
function ob_pool() {
  const g = blank(32, 32)
  rrectfill(g, 1, 1, 30, 30, 3, WD_M)                // rails
  rectfill(g, 2, 1, 29, 2, WD_L)
  rectfill(g, 1, 2, 2, 29, WD_L)
  rectfill(g, 2, 29, 29, 30, WD_D)
  rectfill(g, 29, 2, 30, 29, WD_D)
  rectfill(g, 4, 4, 27, 27, INK)                     // cushion
  rectfill(g, 5, 5, 26, 26, GR_D)                    // felt
  rectfill(g, 5, 5, 26, 5, GR_M)                     // lit north cushion
  rectfill(g, 5, 5, 5, 26, GR_M)
  rectfill(g, 5, 26, 26, 26, INK)
  rectfill(g, 26, 6, 26, 26, INK)
  for (let y = 6; y < 26; y++)                       // felt nap
    for (let x = 6; x < 26; x++)
      if (noise(x, y, 23) === 0) g[y][x] = GR_M
  for (const [px, py] of [[5, 5], [26, 5], [5, 26], [26, 26], [15.5, 4.5], [15.5, 26.5]]) {
    circlefill(g, px + 0.5, py + 0.5, 2.2, INK)      // pockets
    pixel(g, px, py, SHD)
  }
  const balls = [[11, 10, WHT], [17, 14, YEL], [20, 11, DRED], [14, 19, BLU],
                 [19, 21, ORG], [10, 17, GR_L], [22, 17, PNK], [13, 13, INK],
                 [16, 9, DBLU], [21, 15, LYEL]]
  for (const [bx, by, c] of balls) {
    circlefill(g, bx + 0.5, by + 0.5, 1.6, c)
    pixel(g, bx, by - 1, c === INK ? STL : WHT)      // NW glint
    pixel(g, bx + 1, by + 1, INK)                    // contact shadow
  }
  line(g, 7, 24, 24, 8, WD_L)                        // a cue left on the felt
  line(g, 7, 25, 23, 8, WD_D)
  pixel(g, 24, 8, SHN)
  return split(outlined(g, INK))                     // → [TL, TR, BL, BR]
}

// ── medical bed (slot 44) — white, red cross, one raised side rail
function ob_medbed() {
  const g = blank()
  rectfill(g, 2, 1, 13, 14, LGT)                     // frame
  rectfill(g, 2, 1, 13, 1, WHT)
  rectfill(g, 2, 14, 13, 14, SHD)
  rectfill(g, 3, 2, 12, 13, WHT)                     // mattress
  rectfill(g, 3, 2, 12, 4, LGT)                      // raised head end
  rectfill(g, 4, 2, 11, 2, WHT)
  rectfill(g, 3, 5, 12, 5, TAN)
  rectfill(g, 3, 6, 12, 12, LGT)                     // blanket
  rectfill(g, 3, 6, 12, 6, WHT)
  rectfill(g, 12, 7, 12, 12, TAN)
  rectfill(g, 6, 8, 9, 9, RED)                       // red cross
  rectfill(g, 7, 7, 8, 11, RED)
  pixel(g, 7, 7, PNK)
  rectfill(g, 2, 5, 2, 11, STL)                      // side rail
  pixel(g, 2, 5, SHN); pixel(g, 2, 11, INK)
  rectfill(g, 4, 13, 11, 13, TAN)
  return flat(outlined(g, INK))
}

// ── locker (slot 45) — steel, louvred vents, a latch (fridge has neither)
function ob_locker() {
  const g = blank()
  plate(g, 2, 1, 13, 14, STL, LGT, INK)
  rectfill(g, 3, 2, 12, 2, LGT)
  for (let y = 4; y <= 8; y += 2) {                  // vents
    rectfill(g, 5, y, 10, y, INK)
    rectfill(g, 5, y + 1, 10, y + 1, TAN)
  }
  rectfill(g, 10, 10, 12, 12, SHD)                   // latch plate
  rectfill(g, 10, 11, 11, 11, LGT)
  pixel(g, 12, 12, INK)
  rectfill(g, 3, 13, 12, 13, SHD)
  return flat(outlined(g, INK))
}

// ── light (slot 46) — a fluorescent batten, not solid, up under the ceiling
function ob_light() {
  const g = blank()
  rectfill(g, 1, 5, 14, 10, STL)                     // housing
  rectfill(g, 1, 5, 14, 5, LGT)
  rectfill(g, 1, 10, 14, 10, INK)
  rectfill(g, 2, 6, 13, 9, LYEL)                     // the tube
  rectfill(g, 3, 7, 12, 8, WHT)
  rectfill(g, 1, 6, 2, 9, SHD); rectfill(g, 13, 6, 14, 9, SHD)   // end caps
  pixel(g, 0, 7, LYEL); pixel(g, 15, 8, LYEL)        // spill off the ends
  pixel(g, 1, 4, TAN); pixel(g, 14, 11, SHD)
  return flat(outlined(g, INK))
}

// ── metal detector (slot 47) — a portal you walk through: two posts, a gap
function ob_detector() {
  const g = blank()
  for (const x of [1, 12]) {
    rectfill(g, x, 2, x + 2, 13, STL)
    rectfill(g, x, 2, x + 2, 2, LGT)
    rectfill(g, x, 2, x, 13, LGT)
    rectfill(g, x + 2, 3, x + 2, 13, INK)
    rectfill(g, x, 13, x + 2, 13, INK)
    rectfill(g, x, 6, x + 2, 7, YEL)                 // hazard band
    pixel(g, x + 2, 7, DORG)
    rectfill(g, x, 10, x + 2, 10, SHD)
  }
  rectfill(g, 4, 2, 11, 3, STL)                      // header beam
  rectfill(g, 4, 2, 11, 2, LGT)
  rectfill(g, 4, 3, 11, 3, SHD)
  pixel(g, 6, 4, GR_L); pixel(g, 9, 4, RED)          // pass / fail lamps
  for (let x = 5; x < 11; x += 2) pixel(g, x, 12, SHD)   // tread marks
  return flat(outlined(g, INK))
}

// ── cctv camera (slot 52) — body on a bracket, hood + lens angled at the room
function ob_cctv() {
  const g = blank()
  rectfill(g, 7, 1, 9, 3, SHD)                       // wall bracket
  pixel(g, 7, 1, STL); pixel(g, 9, 3, INK)
  rectfill(g, 3, 4, 11, 8, STL)                      // body
  rectfill(g, 3, 4, 11, 4, LGT)
  rectfill(g, 3, 4, 3, 8, LGT)
  rectfill(g, 3, 8, 11, 8, INK)
  rectfill(g, 11, 5, 11, 8, SHD)
  rectfill(g, 5, 9, 11, 11, SHD)                     // hood
  rectfill(g, 5, 9, 11, 9, STL)
  ovalfill(g, 9, 11, 2.6, 2.2, INK)                  // lens
  ovalfill(g, 9, 11, 1.6, 1.4, DBLU)
  pixel(g, 8, 10, SHN)                               // glass glint
  pixel(g, 4, 5, WHT)
  pixel(g, 10, 5, RED)                               // recording
  return flat(outlined(g, INK))
}

// ── bin (slot 53) — an open drum with rubbish spilling over the rim
function ob_bin() {
  const g = blank()
  ovalfill(g, 8, 9.5, 6, 5.5, STL)                   // drum
  for (let x = 3; x <= 13; x += 3) rectfill(g, x, 7, x, 14, SHD)   // ribs
  ovalfill(g, 8, 7.5, 5.4, 3.4, TAN)                 // rim
  ovalfill(g, 8, 7.6, 4.4, 2.6, INK)                 // the dark inside
  pixel(g, 3, 7, LGT); pixel(g, 4, 6, LGT)           // lit NW rim
  pixel(g, 12, 9, INK); pixel(g, 13, 10, INK)
  rectfill(g, 5, 5, 8, 7, TAN)                       // rubbish over the lip
  rectfill(g, 5, 5, 7, 5, LGT)
  pixel(g, 9, 6, WHT); pixel(g, 10, 5, WHT)
  pixel(g, 6, 4, LGT); pixel(g, 9, 7, WD_M)
  return flat(outlined(g, INK))
}

// ═════════════════════════════════════════════════════════════════════════════
// ICONS — slots 56..63.  ~12px, hard contrast, must read at 1× in the HUD.
// ═════════════════════════════════════════════════════════════════════════════

function ic_alarm() {                                // a wall klaxon, flashing
  const g = blank()
  rectfill(g, 4, 12, 11, 14, STL)                    // base
  rectfill(g, 4, 12, 11, 12, LGT)
  rectfill(g, 4, 14, 11, 14, INK)
  rectfill(g, 3, 10, 12, 11, LGT)                    // rim
  rectfill(g, 3, 11, 12, 11, TAN)
  ovalfill(g, 8, 8, 4.4, 4.4, DRED)                  // the lens dome
  ovalfill(g, 7, 7, 2.8, 3, RED)
  rectfill(g, 8, 4, 8, 10, DRED)                     // lens segments
  pixel(g, 6, 5, PNK); pixel(g, 7, 5, PNK)           // hot glint
  line(g, 1, 2, 3, 4, YEL); line(g, 14, 2, 12, 4, YEL)   // flash rays
  pixel(g, 8, 1, YEL); pixel(g, 8, 2, LYEL)
  pixel(g, 1, 8, YEL); pixel(g, 14, 8, YEL)
  return flat(outlined(g, INK))
}

function ic_money() {                                // a banknote
  const g = blank()
  rectfill(g, 1, 3, 14, 12, GR_D)
  rectfill(g, 1, 3, 14, 3, GR_M)                     // lit top edge
  rectfill(g, 1, 12, 14, 12, INK)
  rectfill(g, 3, 5, 12, 10, GR_M)                    // inner panel
  rectfill(g, 3, 5, 12, 5, GR_L)
  rectfill(g, 7, 4, 8, 11, LYEL)                     // the $ — stem
  rectfill(g, 5, 5, 10, 5, LYEL)                     // top bar
  pixel(g, 5, 6, LYEL); pixel(g, 4, 6, LYEL)
  rectfill(g, 5, 7, 10, 7, LYEL)                     // waist
  pixel(g, 10, 8, LYEL); pixel(g, 11, 8, LYEL)
  rectfill(g, 5, 9, 10, 9, LYEL)                     // bottom bar
  pixel(g, 2, 4, GR_L); pixel(g, 13, 11, GR_D)
  return flat(outlined(g, INK))
}

function ic_tray() {                                 // a meal tray
  const g = blank()
  rrectfill(g, 1, 3, 14, 9, 2, LGT)
  rectfill(g, 2, 4, 13, 4, WHT)                      // lit rim
  rectfill(g, 2, 10, 13, 10, TAN)
  rectfill(g, 2, 5, 7, 9, SHD)                       // the big well
  rectfill(g, 2, 6, 7, 9, DORG); rectfill(g, 2, 6, 7, 6, ORG)
  rectfill(g, 9, 5, 12, 6, SHD); rectfill(g, 9, 5, 12, 5, GR_M)
  rectfill(g, 9, 8, 12, 9, SHD); rectfill(g, 9, 8, 12, 8, LYEL)
  pixel(g, 8, 7, TAN)
  return flat(outlined(g, INK))
}

function ic_band() {                                 // contraband: a taped shiv
  const g = blank()
  polyfill(g, [12, 2, 14, 4, 7, 12, 5, 10], LGT)     // blade
  polyfill(g, [12, 2, 13, 3, 6, 11, 5, 10], WHT)     // its bright edge
  pixel(g, 13, 2, WHT); pixel(g, 14, 3, LGT)
  rectfill(g, 2, 10, 6, 14, WD_D)                    // handle
  pixel(g, 3, 11, WD_M); pixel(g, 4, 12, WD_M)
  line(g, 3, 9, 7, 13, TAN)                          // tape wrap
  line(g, 4, 8, 8, 12, TAN)
  pixel(g, 5, 9, LGT); pixel(g, 6, 10, LGT)
  return flat(outlined(g, INK))
}

// A combination spanner, stood upright: OPEN JAW at the top, RING at the bottom.
// The first pass drew it diagonally with a fat pale head and read as a shovel, so
// this one is axis-aligned and symmetric, which is what makes a tool glyph legible
// at 1× — the two ends are the word, the shaft is just the space between them.
function ic_wrench() {
  const g = blank()
  rectfill(g, 4, 1, 5, 4, LGT)                       // jaw, west prong
  rectfill(g, 10, 1, 11, 4, LGT)                     // jaw, east prong
  rectfill(g, 4, 4, 11, 6, LGT)                      // the throat
  rectfill(g, 4, 1, 4, 6, WHT)                       // lit west flank
  rectfill(g, 4, 1, 5, 1, WHT)
  rectfill(g, 11, 2, 11, 6, TAN)                     // shaded east flank
  rectfill(g, 5, 6, 11, 6, TAN)
  rectfill(g, 6, 7, 9, 9, LGT)                       // shaft
  rectfill(g, 6, 7, 6, 9, WHT)
  rectfill(g, 9, 7, 9, 9, TAN)
  circlefill(g, 8, 12, 3.6, LGT)                     // ring end
  circlefill(g, 6.8, 10.8, 2.8, WHT)
  circlefill(g, 8, 12, 1.7, 0)                       // …and its hole
  pixel(g, 10, 14, TAN); pixel(g, 9, 15, TAN)
  return flat(outlined(g, INK))
}

function ic_key() {                                  // brass jail key
  const g = blank()
  circlefill(g, 4.5, 5, 3.6, DORG)                   // bow
  circlefill(g, 4.5, 5, 3, YEL)
  circlefill(g, 4.5, 5, 1.6, 0)
  pixel(g, 3, 3, LYEL); pixel(g, 4, 2, LYEL)
  line(g, 6, 7, 12, 13, YEL)                         // shank
  line(g, 7, 7, 13, 13, DORG)
  rectfill(g, 8, 11, 10, 12, YEL)                    // teeth
  rectfill(g, 10, 9, 12, 10, YEL)
  pixel(g, 13, 13, LYEL); pixel(g, 8, 12, DORG)
  return flat(outlined(g, INK))
}

function ic_star() {                                 // prison grade
  const g = blank()
  const pts = []
  for (let i = 0; i < 10; i++) {
    const a = -Math.PI / 2 + i * Math.PI / 5
    const r = i % 2 ? 2.8 : 6.8
    pts.push(8 + r * Math.cos(a), 8 + r * Math.sin(a))
  }
  polyfill(g, pts, YEL)
  for (let y = 0; y < 16; y++)                        // NW lit, SE in shadow
    for (let x = 0; x < 16; x++)
      if (g[y][x] === YEL && (x - 7) + (y - 8) > 2) g[y][x] = DORG
  pixel(g, 7, 4, LYEL); pixel(g, 8, 4, LYEL); pixel(g, 7, 5, LYEL)
  return flat(outlined(g, INK))
}

function ic_skull() {                                // a death
  const g = blank()
  ovalfill(g, 8, 6.5, 4.6, 4.2, LGT)                 // cranium
  ovalfill(g, 7.2, 5.6, 3.6, 3.2, WHT)
  rectfill(g, 5, 9, 10, 10, LGT)                     // cheekbones
  rectfill(g, 6, 11, 9, 13, LGT)                     // jaw
  rectfill(g, 6, 11, 9, 11, TAN)
  rectfill(g, 4, 5, 6, 7, INK); rectfill(g, 9, 5, 11, 7, INK)   // sockets
  pixel(g, 4, 5, SHD); pixel(g, 9, 5, SHD)
  rectfill(g, 7, 8, 8, 9, INK)                       // nose
  pixel(g, 7, 12, INK); pixel(g, 9, 12, INK)         // tooth gaps
  pixel(g, 6, 13, INK); pixel(g, 8, 13, INK)
  return flat(outlined(g, INK))
}

// ═════════════════════════════════════════════════════════════════════════════
const [bedTop, bedBot] = ob_bed()
const [tableL, tableR] = ob_table()
const [poolTL, poolTR, poolBL, poolBR] = ob_pool()

module.exports = {
  screenW: 720, screenH: 450, scale: 2,
  cellW: 16, cellH: 16,
  sprites: {
    // ── people: dir*3 + stride, dir 0=S 1=N 2=E 3=W ──
    0: lk_person(0, 0), 1: lk_person(0, 1), 2: lk_person(0, 2),
    3: lk_person(1, 0), 4: lk_person(1, 1), 5: lk_person(1, 2),
    6: lk_person(2, 0), 7: lk_person(2, 1), 8: lk_person(2, 2),
    9: lk_person(3, 0), 10: lk_person(3, 1), 11: lk_person(3, 2),
    12: lk_sleep(), 13: lk_down(), 14: lk_fight(), 15: lk_sit(),

    // ── floors ──
    16: fl_dirt(),      17: fl_grass(0),  18: fl_gravel(0), 19: fl_concrete(0),
    20: fl_concrete(1), 21: fl_tile(0),   22: fl_tile(1),   23: fl_wood(),
    48: fl_asphalt(),   49: fl_gravel(1), 55: fl_grass(1),

    // ── objects ──
    24: bedTop,       25: ob_toilet(),  26: ob_sink(),      27: ob_shower(),
    28: ob_bench(),   29: ob_serving(), 30: ob_cooker(),    31: ob_fridge(),
    32: bedBot,       33: ob_desk(),    34: ob_chair(),     35: ob_cabinet(),
    36: ob_tv(),      37: ob_phone(),   38: ob_weights(),   39: ob_bookshelf(),
    40: tableL,       41: tableR,       42: poolTL,         43: poolTR,
    44: ob_medbed(),  45: ob_locker(),  46: ob_light(),     47: ob_detector(),
    50: poolBL,       51: poolBR,       52: ob_cctv(),      53: ob_bin(),
    54: ob_bunk(),

    // ── icons ──
    56: ic_alarm(),  57: ic_money(), 58: ic_tray(), 59: ic_band(),
    60: ic_wrench(), 61: ic_key(),   62: ic_star(), 63: ic_skull(),
  },
}
