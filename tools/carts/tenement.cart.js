// tenement.cart.js — bake this game's OWN voxel object set into its sheet.
//
// Deliberately not isoroom's: that is a probe cart's placeholder art, and tying the game to it
// would be backwards (see the note at the top of runtime/tenement/model.h). The models live in
// tools/voxel-models/tenement.js, and the rect table the cart compiles against is
// runtime/tenement/atlas.h, emitted by:
//   node tools/voxel-bake.js tools/voxel-models/tenement.js --tw 4 --zh 2 --rots 1,3,5,7 \
//        --atlas-w 128 --emit-c runtime/tenement/atlas.h
// Header and sheet are two products of one deterministic bake, so the signature check below turns
// a stale header into a loud failure instead of furniture drawn with last week's rectangles.

const fs   = require('fs')
const path = require('path')
const vb   = require('../voxel-bake.js')
const spec = require('../voxel-models/tenement.js')

const OPT = { tw: 4, cw: 4, zh: 2, rots: [1, 3, 5, 7], atlasW: 128 }

const res = vb.bake(spec, OPT)
const sig = vb.signature(spec, OPT)
const headerPath = path.join(__dirname, '..', '..', 'runtime', 'tenement', 'atlas.h')
const REGEN = 'node tools/voxel-bake.js tools/voxel-models/tenement.js --tw 4 --zh 2 ' +
              '--rots 1,3,5,7 --atlas-w 128 --emit-c runtime/tenement/atlas.h'
if (!fs.existsSync(headerPath)) throw new Error(`tenement: missing ${headerPath} — run:\n  ${REGEN}`)
const header = fs.readFileSync(headerPath, 'utf8')
if (!header.includes(`"${sig}"`)) {
  const had = (header.match(/ISO_SIG\s+"([0-9a-f]+)"/) || [])[1] || '?'
  throw new Error(`tenement: runtime/tenement/atlas.h is STALE (header ${had}, models ${sig}).\n  ${REGEN}`)
}

module.exports = { atlas: vb.atlasPixels(res) }
