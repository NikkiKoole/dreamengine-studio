// room.js — the isoroom probe's object set, as ASCII voxel layers.
// Baked by tools/voxel-bake.js into the four 45-degree rotations. docs/design/iso-rooms.md
//
// SCALE: 6 voxels = one floor tile. With --tw 4 --zh 2 a tile is 24x12px and a voxel is a 4x2px
// cube, so a 12-voxel figure stands 24px.
//
// THE SCALE LIVES IN THESE MODELS, WHICH IS WHY IT IS NOT A KNOB. The on-screen size cannot be
// changed by changing the voxel SIZE: a crisp 2:1 diamond needs the voxel width to be a multiple
// of 4 (sx steps tw/2, sy steps tw/4), so tw=4 — a 4x2px voxel — is the floor and 8 is the next
// step up. Size is therefore set by the voxel COUNT, and changing it means re-authoring every
// model here. This set has been through 8 per tile (32px figure), 4 (16px) and now 6 (24px).
// If the scale moves again, that is a re-authoring job, not a flag.
//
// ONLY THE FOUR DIAGONAL VIEWS ARE BAKED. The four cardinal ones were cut: with no X/Y mixing
// there is no depth cue left, objects flattened into slabs, and edge-on walls survived as 1-2px
// bars. See iso-rooms.md §7.
//
// EVERY OBJECT MUST READ FROM ITS SILHOUETTE, not from modelled detail — there is no room for
// detail at this size. The specific trap, learned at 4 voxels per tile: the sofa and the bed both
// became a pale slab with a coloured band and were indistinguishable. The fix is not more detail,
// it is making them differ in HEIGHT and in outline — the sofa now has a tall cushioned back and
// arms, the bed stays low and flat.
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
    // ── sofa: 2 tiles wide, 1 deep. y=0 is the BACK row. ──
    // Tall cushioned back + arms, so its outline cannot be confused with the bed's.
    sofa: { layers: [
      ['wwwwwwwwwwww',
       'wwwwwwwwwwww',
       'wwwwwwwwwwww',
       'wwwwwwwwwwww',
       'wwwwwwwwwwww',
       'wwwwwwwwwwww'],
      1,
      ['wwwwwwwwwwww',
       'wwssssssssww',
       'wwssssssssww',
       'wwssssssssww',
       'wwssssssssww',
       'wwwwwwwwwwww'],
      1,
      ['cccccccccccc',
       'cccccccccccc',
       'ww........ww',
       'ww........ww',
       'ww........ww',
       '............'],
      1,
    ]},

    // ── bed: 1 tile wide, 2 deep. Headboard at y=0. Stays LOW and flat. ──
    bed: { layers: [
      ['wwwwww',
       'wwwwww',
       'wwwwww',
       'wwwwww',
       'wwwwww',
       'wwwwww',
       'wwwwww',
       'wwwwww',
       'wwwwww',
       'wwwwww',
       'wwwwww',
       'wwwwww'],
      1,
      ['wwwwww',
       'pppppp',
       'pppppp',
       'pppppp',
       'pppppp',
       'pppppp',
       'pppppp',
       'pppppp',
       'pppppp',
       'pppppp',
       'pppppp',
       'wwwwww'],
      ['wwwwww',
       'cccccc',
       'cccccc',
       '......',
       '......',
       '......',
       '......',
       '......',
       '......',
       '......',
       '......',
       '......'],
    ]},

    // ── toilet: tall tank at y=0, low bowl in front. Reads by silhouette. ──
    toilet: { layers: [
      ['.pppp.',
       '.pppp.',
       '.pppp.',
       '.pppp.',
       '......',
       '......'],
      2,
      ['.pppp.',
       '.pppp.',
       '......',
       '......',
       '......',
       '......'],
      2,
    ]},

    // ── fridge: full-height white box, as tall as the figure. ──
    fridge: { layers: [
      ['mmmmmm',
       'mmmmmm',
       'mmmmmm',
       'mmmmmm',
       'mmmmmm',
       'mmmmmm'],
      5,
      ['mmmmmm',
       'mmmmmm',
       'mmmmmm',
       'mmmmmm',
       'kkkkkk',
       'mmmmmm'],
      ['mmmmmm',
       'mmmmmm',
       'mmmmmm',
       'mmmmmm',
       'mmmmmm',
       'mmmmmm'],
      4,
    ]},

    // ── counter: waist-high, metal top. ──
    counter: { layers: [
      ['wwwwww',
       'wwwwww',
       'wwwwww',
       'wwwwww',
       'wwwwww',
       'wwwwww'],
      4,
      ['mmmmmm',
       'mmmmmm',
       'mmmmmm',
       'mmmmmm',
       'mmmmmm',
       'mmmmmm'],
      1,
    ]},

    // ── wall segments, one tile long, 2 voxels thick. ──
    // There are separate NS and EW models rather than one model turned 90 degrees, and that is
    // deliberate: a turned non-square object's WORLD footprint swaps x/y, but the cart derives an
    // item's depth (and its shadow) from the model's UNROTATED footprint. Turning the art alone
    // therefore sorted the east/west walls at the wrong depth and they drew OVER furniture
    // standing in front of them, leaving a triangular sliver of the furniture visible. Baking the
    // second orientation costs 8 cells and removes the whole class of error for walls.
    // LOW = a stub that never occludes.
    wall_low_ns: { layers: [
      ['WWWWWW',
       'WWWWWW'],
      3,
    ]},
    wall_low_ew: { layers: [
      ['WW',
       'WW',
       'WW',
       'WW',
       'WW',
       'WW'],
      3,
    ]},

    // FULL height: needs the near side cut away.
    wall_full_ns: { layers: [
      ['WWWWWW',
       'WWWWWW'],
      11,
    ]},
    wall_full_ew: { layers: [
      ['WW',
       'WW',
       'WW',
       'WW',
       'WW',
       'WW'],
      11,
    ]},

    // ── the figure: 12 voxels tall = 24px. ──
    // Arms out at torso level so the shoulder line is wider than the head. That contrast is what
    // makes a small figure read as a person; the 32px first cut had none and read as a lamp.
    person: { layers: [
      ['.j.j.',
       '.....',
       '.....'],
      4,
      ['bbbbb',
       '.bbb.',
       '.....'],
      4,
      ['.bbb.',
       '.bbb.',
       '.....'],
      ['.hhh.',
       '.hhh.',
       '.....'],
    ]},
  },
}
