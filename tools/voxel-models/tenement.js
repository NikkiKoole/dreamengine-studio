// tenement.js — the tenement sim's object set, as ASCII voxel layers.
// Baked by tools/voxel-bake.js into the four 45-degree rotations.
// Design: docs/design/tenement.md. Geometry (and why it is not a knob): docs/design/iso-rooms.md.
//
// Seeded from the isoroom probe's placeholder set and extended with the two objects the game
// needs that a renderer probe did not: a LOOM (the dumb machine of design §4, where labour is
// visible) and a WARDROBE (tagged storage, design §6). These remain placeholders: the point of
// the object set right now is to exercise the offer index, not to be a furniture catalogue.
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
    // OFF THE NEUTRAL RAMP, because a critic measured the earlier "fix" as cosmetic: it changed the
    // wall's TOP tone only, and both of its side faces stayed byte-identical to the fridge's
    // (#49333b and #5f574f, delta luminance 0.0 — not "similar"). Third time this has bitten. A warm
    // violet gives the architecture its own identity and leaves the neutral greys to the white goods.
    W: [22, 29, 21],
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

    // ── loom: the dumb machine. A resident stands at it for a shift (design §4). ──
    // Deliberately tall and skeletal so it reads as machinery rather than furniture: at this
    // size silhouette is the only thing carrying the read.
    loom: { layers: [
      ['wwwwww',
       'w....w',
       'w....w',
       'wwwwww'],
      1,
      ['w....w',
       'w....w',
       'w....w',
       'w....w'],
      4,
      ['wwwwww',
       'cccccc',
       'cccccc',
       'wwwwww'],
      1,
      ['wwwwww',
       'w....w',
       'w....w',
       'wwwwww'],
      2,
    ]},

    // ── wardrobe: storage, chest height, obviously a box you open. ──
    wardrobe: { layers: [
      ['wwwwww',
       'wwwwww',
       'wwwwww',
       'wwwwww'],
      6,
      ['wwwwww',
       'wwwwww',
       'kkkkkk',
       'wwwwww'],
      ['wwwwww',
       'wwwwww',
       'wwwwww',
       'wwwwww'],
      1,
    ]},

    // ── the figure, LYING DOWN. ──
    // Posture is a property of the OFFER, not a special case in the renderer: a bed says "using me
    // means lying down" and the art follows. Standing on a bed was the giveaway that the sim had no
    // notion of posture at all.
    //
    // Head at y=0 so the model lies along +y, the same axis a bed's headboard sits on. Two voxels
    // tall, because a person on a mattress is a low silhouette and at 24px per tile the read comes
    // from the OUTLINE (long, with a head at one end), never from detail.
    person_lie: { layers: [
      ['.h.',
       'bbb',
       'bbb',
       '.b.',
       '.j.',
       '.j.',
       'j.j',
       'j.j'],
      ['.h.',
       'bbb',
       '.b.',
       '...',
       '...',
       '...',
       '...',
       '...'],
    ]},

    // ── the figure, WORKING: arms out in front of it. ──
    // design §4 says the loom is where labour is VISIBLE, and until now a resident on a 480-minute
    // shift stood there exactly like one waiting for a bus. Arms are the whole read: a standing
    // figure plus two arms reaching forward is the only silhouette at this size that says BUSY.
    //
    // THE ARMS POINT +y (SOUTH) IN MODEL SPACE and the renderer turns the model so they point at the
    // thing being used. Not at `facing`, which is only the last step of the walk — a resident that
    // arrived from the north is facing south whether or not the loom is south of it.
    //
    // Straight arms from the shoulder tips rather than modelled elbows: at 4x2px a voxel there is no
    // room for a joint, and the earlier lesson from this file is that detail at this scale reads as
    // noise while OUTLINE reads as meaning.
    person_work: { layers: [
      ['.j.j.',
       '.....',
       '.....',
       '.....'],
      4,
      ['.bbb.',
       '.....',
       '.....',
       '.....'],
      1,
      ['bbbbb',
       'b...b',
       'b...b',
       'b...b'],
      1,
      ['.bbb.',
       '.....',
       '.....',
       '.....'],
      ['.hhh.',
       '.....',
       '.....',
       '.....'],
      1,
    ]},

    // ── the figure, SITTING. ──
    // The contract has had TN_POSE_SIT since day one and offer.h has always said a sofa and a toilet
    // are things you SIT on — but art had no cell for it, so it fell through to the standing figure
    // and a resident using the sofa was drawn upright on top of the backrest. A declared pose with no
    // art is the same class of bug as a declared seam with no consumer.
    //
    // EIGHT voxels, not twelve, and the thighs project FORWARD in +y. That projection is the whole
    // read: at this size a seated figure cannot be recognised from its proportions, only from its
    // outline breaking the vertical — the same reason the standing figure needs shoulders wider than
    // its head. No shins: the body is placed at SEAT height, so anything below the seat would need
    // negative z, and a foot two voxels wide would read as noise rather than as a leg.
    // SIX deep, not four, and the first two rows are EMPTY on purpose. art centres a posed body in
    // its object's footprint, so a 4-deep sitter on a 6-deep sofa lands in the middle — which is
    // where the BACKREST is, and the torso ended up inside it. Matching the object's depth and
    // parking the body at the FRONT of it makes the same model correct on both things you sit on:
    // on a sofa the torso clears the backrest, on a toilet it clears the cistern.
    person_sit: { layers: [
      ['.....',
       '.....',
       '.....',
       '.....',
       '.jjj.',
       '.jjj.'],
      1,
      ['.....',
       '.....',
       'bbbbb',
       'bbbbb',
       '.....',
       '.....'],
      2,
      ['.....',
       '.....',
       '.bbb.',
       '.bbb.',
       '.....',
       '.....'],
      ['.....',
       '.....',
       '.hhh.',
       '.hhh.',
       '.....',
       '.....'],
      1,
    ]},

    // ── the figure, CARRYING SOMETHING. ──
    // The punch list called this out: "a resident carrying an item looks identical to one walking
    // empty-handed, and hauling is how goods move in this sim". It was invisible for a deeper reason
    // than art, though — work.h sold every good where it was made and deleted it in the same breath,
    // so `carrying` was never once set in a running game and there was nothing to draw.
    //
    // THE LOAD IS THE SILHOUETTE, not the arms. Same rule this file keeps relearning: at 4x2px per
    // voxel an outline reads and a detail does not. So the box sits PROUD in front of the chest in
    // +y, breaking the figure's vertical edge exactly the way the sitting pose's thighs do, and the
    // arms are only wide enough to explain it. It is WOOD, not shirt, so the thing being carried
    // does not read as part of the person — a household-coloured box would just look like a fat torso.
    //
    // +y is FORWARD, the same convention person_work uses, so the renderer's existing "point at the
    // thing" turn works on this model with no new rule.
    person_haul: { layers: [
      ['.j.j.',
       '.....',
       '.....',
       '.....'],
      4,
      ['bbbbb',
       '.....',
       '.....',
       '.....'],
      1,
      ['bbbbb',
       'b...b',
       '.www.',
       '.www.'],
      1,
      ['.bbb.',
       '.....',
       '.....',
       '.....'],
      ['.hhh.',
       '.....',
       '.....',
       '.....'],
      1,
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
