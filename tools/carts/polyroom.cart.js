// polyroom.cart.js — bake the SAME voxel object set tenement ships, so the A/B is honest.
//
// This cart exists to compare two renderings of one room: low-poly triangles against the baked
// voxel sprites. That comparison is worthless unless the voxel half is the REAL one, so this
// bakes `tools/voxel-models/tenement.js` with byte-identical options to `tenement.cart.js` and
// compiles against the same `runtime/tenement/atlas.h`. Same models, same rects, same sheet.
//
//   node tools/voxel-bake.js tools/voxel-models/tenement.js --tw 4 --zh 2 --rots 1,3,5,7 \
//        --atlas-w 128 --emit-c runtime/tenement/atlas.h
//
// The signature check is inherited for the same reason it exists there: header and sheet are two
// products of one deterministic bake, and a stale header means furniture drawn with last week's
// rectangles. If this throws, regenerate — do not hand-edit either side.

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
if (!fs.existsSync(headerPath)) throw new Error(`polyroom: missing ${headerPath} — run:\n  ${REGEN}`)
const header = fs.readFileSync(headerPath, 'utf8')
if (!header.includes(`"${sig}"`)) {
  const had = (header.match(/ISO_SIG\s+"([0-9a-f]+)"/) || [])[1] || '?'
  throw new Error(`polyroom: runtime/tenement/atlas.h is STALE (header ${had}, models ${sig}).\n  ${REGEN}`)
}

module.exports = { atlas: vb.atlasPixels(res) }
