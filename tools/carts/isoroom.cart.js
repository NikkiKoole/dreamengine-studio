// isoroom.cart.js — bake the voxel object set into this cart's sprite sheet.
//
// Unlike a normal generator cart this exports `atlas` rather than `sprites`: the cells are
// arbitrary rects (a sofa is 64x32 at one angle and 48x40 at another), so they cannot live
// in a 16x16 slot grid. make-cart writes an RGBA sheet straight from the index buffer and
// the cart addresses cells with sspr(). See docs/design/iso-rooms.md.
//
// The rect TABLE the cart compiles against lives in runtime/isoroom/atlas.h, emitted by
// `node tools/voxel-bake.js tools/voxel-models/room.js --tw 4 --zh 2 --emit-c runtime/isoroom/atlas.h`.
// Header and sheet are two products of one deterministic bake, so they can only disagree if
// the header is stale — which the signature check below turns into a loud failure instead of
// furniture drawn with last week's rectangles.

const fs   = require('fs')
const path = require('path')
const vb   = require('../voxel-bake.js')

const spec = require('../voxel-models/room.js')

// Must match the --emit-c invocation above EXACTLY, or the signature check trips.
const OPT = { tw: 4, cw: 4, zh: 2, rots: [0, 1, 2, 3, 4, 5, 6, 7], atlasW: 256 }

const res = vb.bake(spec, OPT)
const sig = vb.signature(spec, OPT)

const headerPath = path.join(__dirname, '..', '..', 'runtime', 'isoroom', 'atlas.h')
if (!fs.existsSync(headerPath)) {
  throw new Error(`isoroom: missing ${headerPath} — run:\n` +
    `  node tools/voxel-bake.js tools/voxel-models/room.js --tw 4 --zh 2 --emit-c runtime/isoroom/atlas.h`)
}
const header = fs.readFileSync(headerPath, 'utf8')
if (!header.includes(`"${sig}"`)) {
  const had = (header.match(/ISO_SIG\s+"([0-9a-f]+)"/) || [])[1] || '?'
  throw new Error(`isoroom: runtime/isoroom/atlas.h is STALE (header ${had}, models ${sig}).\n` +
    `  The voxel models or bake params changed. Regenerate:\n` +
    `  node tools/voxel-bake.js tools/voxel-models/room.js --tw 4 --zh 2 --emit-c runtime/isoroom/atlas.h`)
}

module.exports = { atlas: vb.atlasPixels(res) }
