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
    W: 6,          // wall
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

    // ── toilet: fits inside a tile with clearance. Tank at y=0. ──
    toilet: { layers: [
      ['..pppp..',
       '..pppp..',
       '..pppp..',
       '..pppp..',
       '..pppp..',
       '........',
       '........',
       '........'],
      2,
      ['pppppppp',
       'pppppppp',
       'pppppppp',
       'pp....pp',
       'pp....pp',
       'pppppppp',
       '........',
       '........'],
      1,
      ['pppppppp',
       'pppppppp',
       '........',
       '........',
       '........',
       '........',
       '........',
       '........'],
      3,
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
    person: { layers: [
      ['.jj.',
       '.jj.',
       '....',
       '....'],
      5,
      ['.jj.',
       '.jj.',
       '.jj.',
       '....'],
      1,
      ['bbbb',
       'bbbb',
       'bbbb',
       '....'],
      4,
      ['.hh.',
       '.hh.',
       '.hh.',
       '....'],
      2,
    ]},
  },
}
