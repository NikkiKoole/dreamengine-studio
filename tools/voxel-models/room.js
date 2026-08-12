// room.js — the isoroom probe's object set, as ASCII voxel layers.
// Baked by tools/voxel-bake.js into the four 45-degree rotations. docs/design/iso-rooms.md
//
// SCALE: 4 voxels = one floor tile. With --tw 4 --zh 2 a tile is 16x8px and a voxel is a 4x2px
// cube, so an 8-voxel figure stands 16px — the height the maker settled on after seeing 32.
//
// WHY THE MODELS ARE THIS COARSE, since it looks like a mistake next to the first cut: the
// on-screen scale cannot be halved by halving the voxel SIZE. A crisp 2:1 diamond needs the voxel
// width to be a multiple of 4 (sx steps tw/2, sy steps tw/4), so tw=4 — a 4x2px voxel — is the
// floor; below it the rows land on half-pixels and voxels start disappearing. Halving the picture
// therefore means halving the voxel COUNT, which is what happened here: every model was re-authored
// at 4 voxels per tile instead of 8. At 16px per tile there is no room for modelled detail anyway,
// so every object has to read from its SILHOUETTE.
//
// ONLY THE FOUR DIAGONAL VIEWS ARE BAKED. The four cardinal ones were cut: with no X/Y mixing
// there is no depth cue left, objects flattened into slabs, and edge-on walls survived as 1-2px
// bars. See iso-rooms.md §7.
//
// These are PLACEHOLDERS for a shape test, deliberately generic. Nothing is copied from any
// reference game.
//
// FLOORS ARE NOT BAKED. A floor tile is a flat diamond the cart draws with primitives. Baking one
// would waste atlas AND look wrong: each baked cell is independent, so a floor cell's side faces
// would draw even where a neighbouring tile should hide them.

module.exports = {
  materials: {
    w: 4,          // wood, brown
    s: 12,         // upholstery, blue
    c: 14,         // cushion, pink
    m: 6,          // white goods / metal, light grey
    p: 7,          // porcelain, white
    k: 5,          // dark trim
    // Tones are [top, screen-right, screen-left]. The wall gets EXPLICIT ones for two reasons:
    // it must not share a colour with the white goods (it did — both were index 6 — and the
    // fridge dissolved into the wall behind it), and the dark end is 21 rather than 16 because
    // at 16 the two visible inner faces came out 142 vs 29 luminance, a 5x jump that read as two
    // DIFFERENT materials instead of one wall lit from one side.
    W: [22, 5, 21],
    b: 8,          // shirt, red
    j: 1,          // trousers, dark blue
    h: 15,         // skin, peach
  },

  models: {
    // ── seating: 2 tiles wide, 1 deep. y=0 is the BACK row. ──
    sofa: { layers: [
      ['wwwwwwww',
       'wwwwwwww',
       'wwwwwwww',
       'wwwwwwww'],
      ['wwwwwwww',
       'wssssssw',
       'wssssssw',
       'wwwwwwww'],
      ['cccccccc',
       'w......w',
       'w......w',
       '........'],
      ['cccccccc',
       '........',
       '........',
       '........'],
    ]},

    // ── bed: 1 tile wide, 2 deep. Headboard at y=0. ──
    bed: { layers: [
      ['wwww',
       'wwww',
       'wwww',
       'wwww',
       'wwww',
       'wwww',
       'wwww',
       'wwww'],
      ['wwww',
       'pppp',
       'pppp',
       'pppp',
       'pppp',
       'pppp',
       'pppp',
       'wwww'],
      ['wwww',
       'cccc',
       'cccc',
       '....',
       '....',
       '....',
       '....',
       '....'],
    ]},

    // ── toilet: tank at y=0, low bowl in front of it. Reads by silhouette only. ──
    toilet: { layers: [
      ['.pp.',
       '.pp.',
       '....',
       '....'],
      1,
      ['.pp.',
       '....',
       '....',
       '....'],
      1,
    ]},

    // ── fridge: full-height white box, as tall as the figure. ──
    fridge: { layers: [
      ['mmmm',
       'mmmm',
       'mmmm',
       'mmmm'],
      3,
      ['mmmm',
       'mmmm',
       'kkkk',
       'mmmm'],
      ['mmmm',
       'mmmm',
       'mmmm',
       'mmmm'],
      2,
    ]},

    // ── counter: waist-high, metal top. ──
    counter: { layers: [
      ['wwww',
       'wwww',
       'wwww',
       'wwww'],
      2,
      ['mmmm',
       'mmmm',
       'mmmm',
       'mmmm'],
    ]},

    // ── wall segment, one tile long. LOW = a stub that never occludes. ──
    wall_low: { layers: [
      ['WWWW'],
      2,
    ]},

    // ── wall segment, FULL height: needs the near side cut away. ──
    wall_full: { layers: [
      ['WWWW'],
      7,
    ]},

    // ── the figure: 8 voxels tall = 16px. ──
    // Arms out at torso level so the shoulder line is wider than the head. At 32px the first cut
    // was 4 voxels wide with no shoulders and read as a lamp; at 16px there is even less room, so
    // the shoulder-vs-head contrast is doing all the work.
    person: { layers: [
      ['j.j',
       '...'],
      2,
      ['bbb',
       '.b.'],
      2,
      ['.b.',
       '...'],
      ['.h.',
       '...'],
    ]},
  },
}
