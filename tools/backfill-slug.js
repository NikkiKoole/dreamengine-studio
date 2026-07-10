#!/usr/bin/env node
// backfill-slug.js — add the canonical `slug` field to every cart's de:meta.
//
// WHY  A .cart.png embeds its source (de:source) but NOT the name of the .c it
//      came from — the .png→.c link is a bare basename convention nothing
//      records, and the de:meta `title` is a display string that can't be
//      reversed to a filename ("acid rack" ↛ acidrack). `slug` is the authored
//      anchor: the canonical <name>, equal to the .c filename stem, carried
//      inside the PNG via de:source. Design: docs/design/editor-cart-workflow.md
//      Gap 1b · docs/design/cart-metadata.md (the schema row).
//
// WHAT  For each tools/carts/<name>.c that has a de:meta block and no slug:
//        1. inserts  "slug": "<name>"  as the de:meta's first field (strict-JSON
//           text insert — no reformat, re-parsed to validate before writing),
//        2. re-embeds de:source into editor/public/carts/<name>.cart.png so the
//           slug travels in the PNG and de:source stays == the .c (no drift).
//      Only de:source changes — sprites/map/settings are round-tripped verbatim,
//      the thumbnail is untouched, and index.json is NOT regenerated (slug isn't
//      emitted there; `file` already covers the forward direction). Idempotent:
//      carts that already carry the correct slug are skipped; a slug that DISagrees
//      with the filename is reported, never auto-"fixed" (that's a rename, your call).
//
// USAGE
//   node tools/backfill-slug.js            # dry run — report what WOULD change
//   node tools/backfill-slug.js --write    # apply .c edits + re-embed PNGs
//   node tools/backfill-slug.js --write --no-bake   # edit .c only (WARNING: leaves
//                                          # de:source drifted until you re-bake)
//
// This touches many files — RUN WHILE NO OTHER AGENT IS ACTIVE (the shared-tree
// hazard). After --write: `node tools/lint-carts.js` should pass, and
// `node tools/cart-status.js` should show no new source-drift.

const fs   = require('fs')
const path = require('path')
const { extractCartChunks, embedCartChunks } = require('./make-cart.js')

const ROOT   = path.join(__dirname, '..')
const CARTS  = path.join(ROOT, 'tools/carts')
const GALLERY = path.join(ROOT, 'editor/public/carts')

const WRITE   = process.argv.includes('--write')
const NO_BAKE = process.argv.includes('--no-bake')

const META_RE = /(\/\*\s*de:meta\s*\n)([\s\S]*?)(\nde:meta\s*\*\/)/

// insert "slug": "<name>" as the first field of the de:meta JSON body, preserving
// formatting. Returns the edited body, or throws if the result isn't valid JSON.
function insertSlug(body, name) {
  const indent = (body.match(/\n([ \t]+)"/) || [])[1] || '  '
  const line   = `${indent}"slug": ${JSON.stringify(name)},`
  let edited
  if (/^\{[ \t]*\r?\n/.test(body)) {
    // multi-line: `{`↵ … — drop the slug on its own indented line after the brace
    edited = body.replace(/^(\{[ \t]*\r?\n)/, `$1${line}\n`)
  } else {
    // inline `{ "title": … }` — splice after the opening brace
    edited = body.replace('{', `{ "slug": ${JSON.stringify(name)},`)
  }
  JSON.parse(edited) // validate; throws on a malformed splice
  return edited
}

const carts = fs.readdirSync(CARTS).filter(f => f.endsWith('.c')).sort()
const added = [], already = [], mismatch = [], nometa = [], noPng = [], failed = []

for (const f of carts) {
  const name = f.slice(0, -2)
  const cPath = path.join(CARTS, f)
  const src = fs.readFileSync(cPath, 'utf8')
  const mm = src.match(META_RE)
  if (!mm) { nometa.push(name); continue }   // test/probe cart — no de:meta, skip

  let meta
  try { meta = JSON.parse(mm[2]) }
  catch (e) { failed.push(`${name}: de:meta not valid JSON — ${e.message}`); continue }

  if ('slug' in meta) {
    if (meta.slug === name) already.push(name)
    else mismatch.push(`${name}: slug '${meta.slug}' != filename '${name}'`)
    continue
  }

  // needs a slug
  let newBody
  try { newBody = insertSlug(mm[2], name) }
  catch (e) { failed.push(`${name}: could not insert slug — ${e.message}`); continue }

  const pngPath = path.join(GALLERY, `${name}.cart.png`)
  const hasPng = fs.existsSync(pngPath)
  if (!hasPng) noPng.push(name)

  if (WRITE) {
    // Replacement FUNCTION, not a string: newBody is arbitrary cart text and may
    // contain `$1`/`$&`/`$$` etc., which a replacement STRING would expand (bit
    // mule.c: "$100" → group-1 + "00"). A function returns its value verbatim.
    const newSrc = src.replace(META_RE, (_m, p1, _p2, p3) => p1 + newBody + p3)
    fs.writeFileSync(cPath, newSrc)
    if (hasPng && !NO_BAKE) {
      const png = fs.readFileSync(pngPath)
      const chunks = extractCartChunks(png)   // { source, sprites, map, settings }
      chunks.source = newSrc                    // swap ONLY de:source; keep the rest
      fs.writeFileSync(pngPath, embedCartChunks(png, chunks))
    }
  }
  added.push(name)
}

// ── report ────────────────────────────────────────────────────
const n = carts.length
console.log(`\n${WRITE ? 'APPLIED' : 'DRY RUN'} — ${n} .c files scanned\n`)
console.log(`  ${added.length}  ${WRITE ? 'slug added' : 'need a slug (would add)'}`)
console.log(`  ${already.length}  already have the correct slug`)
console.log(`  ${nometa.length}  no de:meta (test/probe carts — skipped)`)
if (WRITE && !NO_BAKE) console.log(`  ${added.filter(x => !noPng.includes(x)).length}  PNGs re-embedded (de:source refreshed)`)
if (noPng.length)    console.log(`\n  ⚠ ${noPng.length} cart(s) have a de:meta but no .cart.png — .c edited, needs a bake:\n     ${noPng.join(', ')}`)
if (mismatch.length) { console.log(`\n  ✗ ${mismatch.length} slug MISMATCH (not touched — rename is your call):`); mismatch.forEach(m => console.log(`     ${m}`)) }
if (failed.length)   { console.log(`\n  ✗ ${failed.length} FAILED:`); failed.forEach(m => console.log(`     ${m}`)) }

if (!WRITE && added.length) console.log(`\n  → run  node tools/backfill-slug.js --write  to apply`)
if (WRITE && added.length)  console.log(`\n  → now: node tools/lint-carts.js   (and cart-status.js should show no new drift)`)
console.log('')

process.exit(mismatch.length || failed.length ? 1 : 0)
