// room.js — the isoroom probe's object set, as ASCII voxel layers.
// Baked by tools/voxel-bake.js into 8 rotations each. See docs/design/iso-rooms.md.
//
// SCALE. 8 voxels = one floor tile. With --tw 4 --zh 2 that makes a tile 32x16px and a
// voxel a 4x2px cube, so a 16-voxel-tall figure stands 32px — the height the maker fixed
// for characters. Every model below is sized in those voxel units.
//
// These are PLACEHOLDERS for a shape test, deliberately generic: the probe is measuring
// whether the pipeline works, not designing a furniture catalogue. Nothing here is copied
// from any reference game.
//
// FLOORS ARE NOT BAKED. A floor tile is a flat diamond the cart draws with primitives.
// Baking one would waste atlas AND look wrong, because each baked cell is independent, so
// a floor cell's side faces would draw even where a neighbouring tile should hide them.

module.exports = {
  materials: {
    w: 4,          // wood, brown
    s: 12,         // upholstery, blue
    c: 14,         // cushion, pink
    m: 6,          // white goods / metal, light grey
    p: 7,          // porcelain, white
    k: 5,          // dark trim
    // The wall gets EXPLICIT tones, warm plaster, and must not share a colour with the white
    // goods above. It did (both were index 6) and the fridge dissolved into the wall behind it:
    // architecture and objects need separate value families or the room reads as one mass.
    // Tones are [top, screen-right, screen-left]. The dark end is 21 rather than 16 on purpose:
    // at 16 the two visible inner faces came out 142 vs 29 luminance, a 5x jump that read as two
    // walls made of DIFFERENT materials instead of one wall lit from one side. 142/88/58 still
    // separates lit from shaded while keeping them obviously the same plaster.
    W: [22, 5, 21],
    b: 8,          // shirt, red
    j: 1,          // trousers, dark blue
    h: 15,         // skin, peach
  },

  models: {
    // ── seating: 2 tiles wide, 1 deep. y=0 is the BACK row. ──
    sofa: { layers: [
      ['wwwwwwwwwwwwwwww',
       'wwwwwwwwwwwwwwww',
       'wwwwwwwwwwwwwwww',
       'wwwwwwwwwwwwwwww',
       'wwwwwwwwwwwwwwww',
       'wwwwwwwwwwwwwwww',
       'wwwwwwwwwwwwwwww',
       'wwwwwwwwwwwwwwww'],
      1,
      ['wwwwwwwwwwwwwwww',
       'wwssssssssssssww',
       'wwssssssssssssww',
       'wwssssssssssssww',
       'wwssssssssssssww',
       'wwssssssssssssww',
       'wwssssssssssssww',
       'wwwwwwwwwwwwwwww'],
      2,
      ['cccccccccccccccc',
       'cccccccccccccccc',
       'ww............ww',
       'ww............ww',
       'ww............ww',
       'ww............ww',
       'ww............ww',
       '................'],
      2,
    ]},

    // ── bed: 1 tile wide, 2 deep. Headboard at y=0. ──
    bed: { layers: [
      ['wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww'],
      1,
      ['wwwwwwww',
       'pppppppp',
       'pppppppp',
       'pppppppp',
       'pppppppp',
       'pppppppp',
       'pppppppp',
       'pppppppp',
       'pppppppp',
       'pppppppp',
       'pppppppp',
       'pppppppp',
       'pppppppp',
       'pppppppp',
       'pppppppp',
       'wwwwwwww'],
      1,
      ['wwwwwwww',
       'cccccccc',
       'cccccccc',
       'cccccccc',
       '........',
       '........',
       '........',
       '........',
       '........',
       '........',
       '........',
       '........',
       '........',
       '........',
       '........',
       '........'],
    ]},

    // ── toilet: tank at y=0, bowl in front. ──
    // Rebuilt chunkier. The first cut spread thin 2-voxel detail across the whole tile and
    // baked to an unidentifiable white blob: at 8 voxels per tile there is no room for
    // fine detail, so the read has to come from the SILHOUETTE (a tall tank behind a low
    // bowl) rather than from any modelled feature.
    toilet: { layers: [
      ['..pppp..',              // one solid base block: tank footprint + bowl
       '..pppp..',
       '..pppp..',
       '..pppp..',
       '........',
       '........',
       '........',
       '........'],
      3,
      ['..pppp..',              // the bowl stops here; the tank keeps going
       '..pppp..',
       '........',
       '........',
       '........',
       '........',
       '........',
       '........'],
      4,
    ]},

    // ── fridge: a full-height white box, the tallest object in the room. ──
    fridge: { layers: [
      ['mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm'],
      7,
      ['mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'kkkkkkkk',
       'mmmmmmmm'],
      ['mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm'],
      6,
    ]},

    // ── counter: waist-high, metal top. ──
    counter: { layers: [
      ['wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww'],
      6,
      ['mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm',
       'mmmmmmmm'],
      1,
    ]},

    // ── wall segment: one tile long, 2 voxels thick. LOW (Theme Hospital strategy):
    // a stub that never occludes, so there is no cutdown problem to solve. ──
    wall_low: { layers: [
      ['WWWWWWWW',
       'WWWWWWWW'],
      5,
    ]},

    // ── wall segment: FULL height (the Sims strategy, which needs cutdown). ──
    wall_full: { layers: [
      ['WWWWWWWW',
       'WWWWWWWW'],
      15,
    ]},

    // ── the figure: 16 voxels tall = 32px, the fixed character height. ──
    // FIVE voxels wide, not four, and with the arms held out at torso level. The first cut was
    // 4 wide with no shoulders, which at 16px across against 32px tall read as a lamp rather
    // than a person — the maker could not find him in the room. A human silhouette needs a
    // shoulder line wider than its head; that contrast is what makes it read at this size.
    person: { layers: [
      ['.j.j.',                 // legs, two of them, with daylight between
       '.j.j.',
       '.....'],
      5,
      ['bbbbb',                 // shoulders + arms out: the widest part of the silhouette
       '.bbb.',
       '.....'],
      5,
      ['.bbb.',                 // neck / upper chest, narrower again
       '.bbb.',
       '.....'],
      1,
      ['.hhh.',                 // head
       '.hhh.',
       '.....'],
      1,
    ]},
  },
}
