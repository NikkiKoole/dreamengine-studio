// sprite-patch.js — the slot-level sprite PATCH/OVERLAY core (editor-cart-workflow
// Gap 2, "Option D"). A generator cart's sprites come from a .cart.js program; a
// patch lets a human hand-edit individual 16×16 slots reversibly, and those edits
// SURVIVE the next CLI bake (the COMIC_PATCHES pattern from gen-rom-font.js,
// generalized). The .cart.js stays a pure program; the patch is pure DATA.
//
// This module is palette/PNG-agnostic on purpose: it works entirely in
// palette-INDEX space (each slot = 256 ints 0–31, row-major 16×16). The caller
// (make-cart.js on the bake side, main.cjs on the editor side) supplies the
// generator's parsed slots + the patch object; this owns the fingerprints and the
// bake algorithm, so the two sides can never drift. ONE implementation, two callers.
//
// Design + schema: docs/design/editor-cart-workflow.md → "Gap 2 — the sprite story".

const fs     = require('fs')
const crypto = require('crypto')

const PATCH_FORMAT = 'de-sprite-patch/1'
const SLOT_LEN     = 256   // 16×16

// ── fingerprints ──────────────────────────────────────────────
// A slot's fingerprint = sha256 of its 256 palette indices as a 256-byte buffer
// (indices 0–31, one byte each), hex, truncated to 16 chars. This isn't
// adversarial — 16 chars is plenty to tell "the generator's output moved". We
// fingerprint the generator's OUTPUT, never the .cart.js source: a comment-only
// or refactor edit that produces identical pixels must NOT nuke patches.
function fingerprintSlot(px) {
  const buf = Buffer.alloc(SLOT_LEN)
  for (let i = 0; i < SLOT_LEN; i++) buf[i] = (px[i] | 0) & 0xff
  return crypto.createHash('sha256').update(buf).digest('hex').slice(0, 16)
}

function slotIsEmpty(px) {
  if (!px) return true
  for (let i = 0; i < SLOT_LEN; i++) if ((px[i] | 0) !== 0) return false
  return true
}

// sheet fingerprint = hash over every NON-EMPTY generator slot, concatenated in
// index order (slot 0..63). The fast "did anything at all change?" gate.
function fingerprintSheet(genSlots) {
  const h = crypto.createHash('sha256')
  for (let s = 0; s < 64; s++) {
    const px = genSlots[s]
    if (!px || slotIsEmpty(px)) continue
    const buf = Buffer.alloc(SLOT_LEN)
    for (let i = 0; i < SLOT_LEN; i++) buf[i] = (px[i] | 0) & 0xff
    h.update(buf)
  }
  return h.digest('hex').slice(0, 16)
}

// ── the bake algorithm ────────────────────────────────────────
// gen:   { slotIndex(0–63): [256 indices] }  the generator's pristine output
// patch: { format, cart, sheet, slots: { s: { base, pixels, note?, edited? } } }
//
// Returns { slots, patch, warnings, changed }:
//   slots    = the COMPOSITED final sheet, gen with surviving patches overlaid
//   patch    = the SURVIVING patch, fingerprints rewritten to the current
//              generator so the next bake is a clean fast-path match (stale slots
//              pruned out — so a wholesale regenerate empties the file for free)
//   warnings = one line per discarded (stale) slot, for a LOUD console log
//   changed  = did the surviving patch differ from the input (pruned / rewritten)?
//              — the caller uses this to avoid rewriting the file on a no-op bake
function applyPatch(gen, patch) {
  const warnings = []
  const finalSlots = {}
  for (let s = 0; s < 64; s++) if (gen[s]) finalSlots[s] = gen[s].slice()

  const surviving = {
    format: PATCH_FORMAT,
    cart:   patch.cart || '',
    sheet:  '',
    slots:  {},
  }
  const genHash  = fingerprintSheet(gen)
  const fastPath = patch.sheet && patch.sheet === genHash

  for (const key of Object.keys(patch.slots || {})) {
    const s = parseInt(key, 10)
    if (!(s >= 0 && s < 64)) continue
    const p = patch.slots[key]
    if (!p || !Array.isArray(p.pixels) || p.pixels.length !== SLOT_LEN) {
      warnings.push(`patch slot ${s} is malformed — discarded`)
      continue
    }
    const g       = gen[s]
    const genGone = slotIsEmpty(g)

    // apply IFF: (fast path — whole sheet byte-identical) OR (this slot's
    // generator output still matches the fingerprint the patch was diffed
    // against) OR (a pure human ADDITION over a slot the generator leaves empty).
    const applies =
      fastPath ||
      (!genGone && fingerprintSlot(g) === p.base) ||
      (genGone  && p.base === 'empty')

    if (!applies) {
      // STALE: the generator's output for this slot moved out from under the
      // patch — a hand-edit against the old art is meaningless on the new art.
      warnings.push(`patched slot ${s} no longer matches the generator — discarded`)
      continue
    }
    finalSlots[s] = p.pixels.slice()
    // carry the entry forward, re-anchoring `base` to the CURRENT generator
    // output so the next bake matches per-slot (step 4: rewrite bases).
    surviving.slots[key] = {
      ...p,
      base:   genGone ? 'empty' : fingerprintSlot(g),
      pixels: p.pixels.slice(),
    }
  }
  surviving.sheet = genHash

  // changed = did we prune anything or move any fingerprint? (drives "rewrite the
  // file only when it actually changed", so a clean re-bake is git-silent)
  const changed =
    Object.keys(surviving.slots).length !== Object.keys(patch.slots || {}).length ||
    surviving.sheet !== patch.sheet ||
    Object.keys(surviving.slots).some(k => surviving.slots[k].base !== (patch.slots[k] && patch.slots[k].base))

  return { slots: finalSlots, patch: surviving, warnings, changed }
}

// ── build a fresh patch from a human-edited sheet ─────────────
// gen:  the generator's pristine slots (the diff base)
// edited: { slotIndex: [256 indices] } the human's current sheet slots
// Returns a patch object holding ONE entry per slot that differs from gen (the
// machine-readable "a human changed this"). Empty .slots ⇒ no hand-edits.
function buildPatch(cart, gen, edited, now) {
  const patch = { format: PATCH_FORMAT, cart: cart || '', sheet: fingerprintSheet(gen), slots: {} }
  for (let s = 0; s < 64; s++) {
    const e = edited[s]
    if (!e) continue
    const g       = gen[s]
    const genGone = slotIsEmpty(g)
    // a slot that matches the generator (incl. both empty) is NOT a patch
    if (genGone && slotIsEmpty(e)) continue
    if (!genGone && slotsEqual(g, e)) continue
    patch.slots[s] = {
      base:   genGone ? 'empty' : fingerprintSlot(g),
      pixels: e.slice(0, SLOT_LEN),
      edited: now || undefined,
    }
  }
  return patch
}

function slotsEqual(a, b) {
  if (!a || !b) return false
  for (let i = 0; i < SLOT_LEN; i++) if ((a[i] | 0) !== (b[i] | 0)) return false
  return true
}

// ── file I/O ──────────────────────────────────────────────────
// The committed source of truth is tools/carts/<name>.sprites.patch.json (deleting
// it = "kill ALL patches", clean + obvious). It's mirrored into the .cart.png as a
// de:spritepatch zTXt chunk so the editor loads/saves it alongside de:source.
function patchPathFor(srcFile) {
  return srcFile.replace(/\.c$/, '.sprites.patch.json')
}

function readPatch(srcFileOrPath) {
  const p = srcFileOrPath.endsWith('.c') ? patchPathFor(srcFileOrPath) : srcFileOrPath
  if (!fs.existsSync(p)) return null
  try {
    const obj = JSON.parse(fs.readFileSync(p, 'utf8'))
    if (obj && obj.format === PATCH_FORMAT && obj.slots) return obj
    return null
  } catch { return null }
}

// Write (or prune-to-nothing → delete) the patch file. Pretty-printed + stable key
// order so git diffs stay legible.
function writePatch(srcFileOrPath, patch) {
  const p = srcFileOrPath.endsWith('.c') ? patchPathFor(srcFileOrPath) : srcFileOrPath
  if (!patch || !patch.slots || Object.keys(patch.slots).length === 0) {
    if (fs.existsSync(p)) fs.unlinkSync(p)
    return false
  }
  fs.writeFileSync(p, serializePatch(patch) + '\n')
  return true
}

// deterministic serialization (numeric slot order) so re-bakes don't churn git
function serializePatch(patch) {
  const slots = {}
  for (const k of Object.keys(patch.slots).map(Number).sort((a, b) => a - b)) {
    slots[k] = patch.slots[k]
  }
  return JSON.stringify({ format: PATCH_FORMAT, cart: patch.cart || '', sheet: patch.sheet, slots }, null, 2)
}

module.exports = {
  PATCH_FORMAT, SLOT_LEN,
  fingerprintSlot, fingerprintSheet, slotIsEmpty, slotsEqual,
  applyPatch, buildPatch,
  patchPathFor, readPatch, writePatch, serializePatch,
}
