#!/usr/bin/env node
/*
 * icon-mask.js — the APP-ICON MASK template + "what gets cut off" oracle.
 *
 * WHY: iOS masks a square 1024x1024 app icon to a rounded-rect "squircle" and throws the corners
 * away. Designing blind means finding out at review time. This tool makes the cut VISIBLE up front
 * (a template you draw against) and CHECKABLE after (a per-corner report on a finished icon).
 *
 * The mask is not guessed. It is MEASURED from Apple's own renderer: Xcode 26 ships
 *   Icon Composer.app/Contents/Executables/ictool
 * which renders a .icon document through the real iOS mask. `rebuild` drives it with a flat
 * full-bleed layer and keeps the resulting ALPHA channel — that alpha IS the mask. The result is
 * committed as tools/icon-masks/ios26-2048.png (8-bit grey, 8-fold symmetrised) so the tool works
 * on machines without Xcode; `--check` re-derives and diffs it.
 *
 * MEASURED FACTS (docs/design/app-icon-mask.md holds the write-up):
 *   - The iOS 26 mask is NOT a superellipse. Best-fit |x|^n+|y|^n=1 is n=4.39 and still misses by
 *     42px at 1024. Don't approximate it with a formula — use the measured mask.
 *   - It is a STRICT ENVELOPE of the classic iOS 7-18 mask (continuous rounded rect, r=0.2237w):
 *     the iOS 26 boundary is at or inside the old one at every angle. So ONE template is safe for
 *     both; if it survives iOS 26 it survives iOS 18.
 *   - The flat part of each side is only 38% of the side. Corners eat ~31% from each end.
 *   - THE DESIGNER'S RULE: the inscribed circle (diameter = full icon width) is entirely inside the
 *     mask. Anything within that circle survives. Only the four corner slivers outside it are at risk.
 *   - VERIFIED ON A REAL iOS 26.5 DEVICE (simulator, iPhone 17): the committed mask matches the
 *     home-screen silhouette to mean 0.13px / max 0.70px, and the interior comes back FLAT (a probe
 *     rendered rgb 254-255,0-1,254-255) — so iOS applies NO gloss/specular to the flat PNG we ship
 *     from the asset catalog. Masking alone is a faithful prediction. `device` re-runs that proof.
 *
 * USAGE
 *   node tools/icon-mask.js template [--size 1024] [--out f.png] [--overlay] [--plain]
 *       The thing you draw against. Cut region tinted red, mask edge stroked, guides in blue
 *       (inscribed circle = the safe rule, inner circles, thirds, diagonals, centre cross).
 *       --overlay  transparent inside — a LAYER to float on top of artwork in Procreate/Photoshop
 *       --plain    no guides, just mask + cut tint
 *   node tools/icon-mask.js mask [--size 1024] [--out f.png]
 *       The bare mask: white inside / black outside, 8-bit grey. For compositing yourself.
 *   node tools/icon-mask.js check <icon.png> [--out proof.png] [--no-proof] [--tol 24] [--quiet]
 *       --no-proof skips writing the 3-up PNG — what build-app.js's advisory staging check passes
 *       Report what the mask cuts off THIS icon, per corner: is the cut region flat background
 *       (safe) or does it carry detail (loss)? Writes a 3-up proof PNG (as drawn / as shown /
 *       what got cut). --quiet exits nonzero if any corner loses detail — release gate.
 *   node tools/icon-mask.js preview <icon.png> [--out sheet.png] [--nearest]
 *       WHAT IT WILL LOOK LIKE: the icon masked and shrunk to the real pixel sizes iOS and the
 *       store display it at, on both a light and a dark backdrop. This is the everyday "will my art
 *       survive" view — the mask eats the corners, the downscale eats the fine pixel detail, and
 *       small sizes are where a lo-fi icon dies. --nearest keeps pixels crisp (NOT what iOS does;
 *       it smooth-scales, which is the default here).
 *   node tools/icon-mask.js device <icon.png> [--sim <udid|name>] [--out f.png] [--keep]
 *       GROUND TRUTH, not a prediction: stage the icon into a throwaway app, install it into a
 *       booted iOS 26 simulator, screenshot the home screen, auto-locate the icon (before/after
 *       screenshot diff), crop it, and diff the silhouette against the committed mask. Needs Xcode
 *       + a simulator .app to borrow (ios/build/.../Debug-iphonesimulator/*.app). Uninstalls after.
 *   node tools/icon-mask.js rebuild        re-derive the committed mask from ictool (needs Xcode 26)
 *   node tools/icon-mask.js --check        gate: committed mask still matches ictool (skips w/o Xcode)
 *
 * Deps: sharp (already a repo-root dependency). No ImageMagick, no ffmpeg.
 */

const fs = require('fs')
const path = require('path')
const os = require('os')
const { execFileSync } = require('child_process')
const sharp = require('sharp')

const ROOT = path.join(__dirname, '..')
const MASK_DIR = path.join(ROOT, 'tools/icon-masks')
const MASK_FILE = path.join(MASK_DIR, 'ios26-2048.png')
const MASTER = 2048

const ICTOOL = '/Applications/Xcode26_6.app/Contents/Applications/Icon Composer.app/Contents/Executables/ictool'
function findIctool () {
  if (fs.existsSync(ICTOOL)) return ICTOOL
  for (const app of ['/Applications/Xcode.app', ...glob('/Applications', /^Xcode.*\.app$/)]) {
    const p = path.join(app, 'Contents/Applications/Icon Composer.app/Contents/Executables/ictool')
    if (fs.existsSync(p)) return p
  }
  return null
}
function glob (dir, re) {
  try { return fs.readdirSync(dir).filter(f => re.test(f)).map(f => path.join(dir, f)) } catch { return [] }
}

// ── the mask ────────────────────────────────────────────────────────────────────────────────────
// Drive ictool with a flat full-bleed layer; keep the alpha. Returns {data (N*N grey), n}.
async function deriveMask (ict) {
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'iconmask-'))
  const doc = path.join(tmp, 'Probe.icon')
  fs.mkdirSync(path.join(doc, 'Assets'), { recursive: true })
  await sharp({ create: { width: 1024, height: 1024, channels: 4, background: { r: 255, g: 0, b: 255, alpha: 1 } } })
    .png().toFile(path.join(doc, 'Assets/flat.png'))
  fs.writeFileSync(path.join(doc, 'icon.json'), JSON.stringify({
    fill: { 'automatic-gradient': 'display-p3:1,0,1,1' },
    groups: [{ layers: [{ 'image-name': 'flat.png' }] }],
    'supported-platforms': { circles: ['watchOS'], squares: ['macOS'] }
  }, null, 2) + '\n')
  const out = path.join(tmp, 'render.png')
  execFileSync(ict, [doc, '--export-image', '--output-file', out, '--platform', 'iOS',
    '--rendition', 'Default', '--width', '1024', '--height', '1024', '--scale', String(MASTER / 1024)],
    { stdio: ['ignore', 'ignore', 'pipe'] })
  const { data, info } = await sharp(out).ensureAlpha().raw().toBuffer({ resolveWithObject: true })
  if (info.width !== MASTER) throw new Error(`ictool gave ${info.width}px, expected ${MASTER}`)
  const a = Buffer.alloc(MASTER * MASTER)
  for (let i = 0; i < MASTER * MASTER; i++) a[i] = data[i * info.channels + 3]
  fs.rmSync(tmp, { recursive: true, force: true })
  return a
}

// The mask is 8-fold symmetric by construction (a square icon shape). Average the 8 images of each
// pixel to kill renderer noise, and report how far off it was — a big number would mean our
// assumption is wrong, not that the renderer is sloppy.
function symmetrise (a, n) {
  const out = Buffer.alloc(n * n)
  let maxDev = 0
  for (let y = 0; y < n; y++) {
    for (let x = 0; x < n; x++) {
      const xs = [x, n - 1 - x], ys = [y, n - 1 - y]
      let sum = 0, lo = 255, hi = 0
      for (const X of xs) for (const Y of ys) for (const [p, q] of [[X, Y], [Y, X]]) {
        const v = a[q * n + p]; sum += v; if (v < lo) lo = v; if (v > hi) hi = v
      }
      out[y * n + x] = Math.round(sum / 8)
      if (hi - lo > maxDev) maxDev = hi - lo
    }
  }
  return { out, maxDev }
}

// Erode the mask by `inset` px — the curve to draw your OWN chassis/border on so iOS never shaves
// it. A hand-drawn rounded rect does NOT work here: its corners are a different curve to Apple's,
// so it pokes outside the mask near the diagonals even when it looks safely inside. Two-pass
// chamfer distance transform (3-4 weights, /3), exact enough at these radii.
function erode (m, n, inset) {
  if (inset <= 0) return m
  const INF = 1e9
  const d = new Float32Array(n * n)
  for (let i = 0; i < n * n; i++) d[i] = m[i] >= 128 ? INF : 0
  const at = (x, y) => (x < 0 || y < 0 || x >= n || y >= n) ? 0 : d[y * n + x]
  for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) {
    if (!d[y * n + x]) continue
    d[y * n + x] = Math.min(d[y * n + x], at(x - 1, y) + 1, at(x, y - 1) + 1, at(x - 1, y - 1) + 4 / 3, at(x + 1, y - 1) + 4 / 3)
  }
  for (let y = n - 1; y >= 0; y--) for (let x = n - 1; x >= 0; x--) {
    if (!d[y * n + x]) continue
    d[y * n + x] = Math.min(d[y * n + x], at(x + 1, y) + 1, at(x, y + 1) + 1, at(x + 1, y + 1) + 4 / 3, at(x - 1, y + 1) + 4 / 3)
  }
  const out = Buffer.alloc(n * n)
  for (let i = 0; i < n * n; i++) {
    const v = d[i] - inset                       // signed distance past the inset line
    out[i] = v <= 0 ? 0 : v >= 1 ? 255 : Math.round(v * 255)   // 1px of feather so the edge reads
  }
  return out
}

async function loadMask (size) {
  if (!fs.existsSync(MASK_FILE)) {
    console.error(`✗ missing ${path.relative(ROOT, MASK_FILE)} — run: node tools/icon-mask.js rebuild`)
    process.exit(1)
  }
  let img = sharp(MASK_FILE)
  if (size !== MASTER) img = img.resize(size, size, { kernel: 'lanczos3' })
  const { data } = await img.greyscale().raw().toBuffer({ resolveWithObject: true })
  return data // one byte per pixel: 255 = kept, 0 = cut
}

// ── geometry read off the mask (for the report + the docs) ───────────────────────────────────────
function geometry (m, n) {
  const inside = (x, y) => m[y * n + x] >= 128
  const leftEdge = (y) => { for (let x = 0; x < n; x++) if (inside(x, y)) return x; return n }
  let flatFrom = -1, flatTo = -1
  for (let y = 0; y < n; y++) { if (leftEdge(y) === 0) { if (flatFrom < 0) flatFrom = y; flatTo = y } }
  let diag = 0
  for (let d = 0; d < n; d++) if (inside(d, d)) { diag = d; break }
  let cutPx = 0
  for (let i = 0; i < n * n; i++) if (m[i] < 128) cutPx++
  return {
    flatSidePct: ((flatTo - flatFrom + 1) / n) * 100,
    cornerRun: flatFrom,                        // px of each side eaten by the corner curve
    diagFirstInside: diag,                      // corner square of this size is fully cut
    cutAreaPct: (cutPx / (n * n)) * 100
  }
}

// boundary band (for stroking the mask edge)
function edgeBand (m, n, w) {
  const e = Buffer.alloc(n * n)
  for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) {
    const v = m[y * n + x] >= 128
    let b = false
    for (let dy = -1; dy <= 1 && !b; dy++) for (let dx = -1; dx <= 1; dx++) {
      const X = x + dx, Y = y + dy
      const u = (X < 0 || Y < 0 || X >= n || Y >= n) ? false : m[Y * n + X] >= 128
      if (u !== v) { b = true; break }
    }
    if (b) e[y * n + x] = 255
  }
  if (w <= 1) return e
  const o = Buffer.from(e), r = Math.floor(w / 2)
  for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) {
    if (!e[y * n + x]) continue
    for (let dy = -r; dy <= r; dy++) for (let dx = -r; dx <= r; dx++) {
      const X = x + dx, Y = y + dy
      if (X >= 0 && Y >= 0 && X < n && Y < n) o[Y * n + X] = 255
    }
  }
  return o
}

// ── template ────────────────────────────────────────────────────────────────────────────────────
function guidesSVG (n) {
  const s = (v) => (v * n).toFixed(2)
  const B = '#3b9eff', thin = Math.max(1, Math.round(n / 512)), fine = Math.max(1, Math.round(n / 1024))
  const L = (x1, y1, x2, y2, w) => `<line x1="${s(x1)}" y1="${s(y1)}" x2="${s(x2)}" y2="${s(y2)}" stroke="${B}" stroke-width="${w}" opacity="0.55"/>`
  const C = (r, w, op) => `<circle cx="${s(0.5)}" cy="${s(0.5)}" r="${s(r)}" fill="none" stroke="${B}" stroke-width="${w}" opacity="${op}"/>`
  const parts = []
  // thirds + quarters grid
  for (const f of [1 / 8, 1 / 4, 3 / 8, 5 / 8, 3 / 4, 7 / 8]) { parts.push(L(0, f, 1, f, fine), L(f, 0, f, 1, fine)) }
  // diagonals
  parts.push(L(0, 0, 1, 1, fine), L(1, 0, 0, 1, fine))
  // centre cross
  parts.push(L(0, 0.5, 1, 0.5, thin), L(0.5, 0, 0.5, 1, thin))
  // the safe rule: inscribed circle (everything inside survives the mask) + inner rings
  parts.push(C(0.5, thin * 1.5, 0.85), C(0.4, thin, 0.5), C(0.25, thin, 0.5), C(0.06, thin, 0.5))
  return Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${n}" height="${n}">${parts.join('')}</svg>`)
}

async function cmdTemplate (opts) {
  const n = opts.size
  const m = await loadMask(n)
  const stroke = Math.max(2, Math.round(n / 340))
  const band = edgeBand(m, n, stroke)
  const iband = opts.inset > 0 ? edgeBand(erode(m, n, opts.inset * (n / 1024)), n, stroke) : null
  const px = Buffer.alloc(n * n * 4)
  for (let i = 0; i < n * n; i++) {
    const keep = m[i] / 255
    const o = i * 4
    if (band[i]) { px[o] = 0xff; px[o + 1] = 0x2d; px[o + 2] = 0x55; px[o + 3] = 0xff; continue }
    if (iband && iband[i]) { px[o] = 0x21; px[o + 1] = 0xc0; px[o + 2] = 0x6b; px[o + 3] = 0xff; continue }
    if (keep >= 0.5) {
      // inside the mask
      if (opts.overlay) { px[o + 3] = 0 } else { px[o] = px[o + 1] = px[o + 2] = 0xff; px[o + 3] = 0xff }
    } else {
      // cut away — tint it so it reads as "this is gone"
      const a = opts.overlay ? 0.55 : 1
      px[o] = 0xff; px[o + 1] = 0x3b; px[o + 2] = 0x4e
      px[o + 3] = Math.round(255 * a * (1 - keep * 0.6))
    }
  }
  let img = sharp(px, { raw: { width: n, height: n, channels: 4 } })
  if (!opts.plain) {
    img = sharp(await img.png().toBuffer()).composite([{ input: guidesSVG(n), blend: 'over' }])
  }
  await img.png().toFile(opts.out)
  const g = geometry(m, n)
  console.log(`✓ ${path.relative(process.cwd(), opts.out)}  ${n}x${n}${opts.overlay ? ' (transparent inside — use as a layer)' : ''}${opts.plain ? ' (no guides)' : ''}`)
  console.log(`  mask cuts ${g.cutAreaPct.toFixed(1)}% of the square; flat side = ${g.flatSidePct.toFixed(0)}% of the edge`)
  console.log(`  the corner curve eats the first ${g.cornerRun} px of every edge; a ${g.diagFirstInside}x${g.diagFirstInside} px corner square is gone entirely`)
  console.log(`  SAFE RULE: everything inside the big circle (diameter = the full ${n} px) survives — only the four corner slivers are at risk`)
  if (opts.inset > 0) console.log(`  green line = the mask inset by ${opts.inset}px (at 1024) — draw your own chassis/border ON it, never a hand-rolled rounded rect`)
}

async function cmdMask (opts) {
  const n = opts.size
  const m = erode(await loadMask(n), n, opts.inset * (n / 1024))
  await sharp(m, { raw: { width: n, height: n, channels: 1 } }).png().toFile(opts.out)
  console.log(`✓ ${path.relative(process.cwd(), opts.out)}  ${n}x${n} grey (255 = kept, 0 = cut)${opts.inset ? `, inset ${opts.inset}px` : ''}`)
}

// ── check a real icon ───────────────────────────────────────────────────────────────────────────
function median (arr) { const a = arr.slice().sort((x, y) => x - y); return a[a.length >> 1] }

async function cmdCheck (file, opts) {
  if (!fs.existsSync(file)) { console.error(`✗ no such file: ${file}`); process.exit(1) }
  const meta = await sharp(file).metadata()
  const n = Math.min(meta.width, meta.height)
  if (meta.width !== meta.height) console.log(`⚠ not square: ${meta.width}x${meta.height} — the store wants 1024x1024`)
  else if (meta.width !== 1024) console.log(`⚠ ${meta.width}x${meta.width} — the store wants 1024x1024`)
  // Read once, with alpha, and judge transparency from the RAW bytes. (sharp's extractChannel(3)
  // does NOT reliably hand back the alpha band here — it reported min=9 on a fully opaque icon.)
  const { data, info } = await sharp(file).resize(n, n, { fit: 'cover' }).ensureAlpha().raw().toBuffer({ resolveWithObject: true })
  const C = info.channels
  if (meta.hasAlpha) {
    let nonOpaque = 0
    for (let i = 3; i < data.length; i += C) if (data[i] < 255) nonOpaque++
    console.log(nonOpaque
      ? `⚠ ${nonOpaque} TRANSPARENT pixels — an iOS icon must be OPAQUE or the build fails validation; flatten it onto a background`
      : `· carries an (unused) alpha channel, every pixel opaque — harmless, actool flattens it`)
  }
  const m = await loadMask(n)
  const g = geometry(m, n)

  // Per corner: is the cut region flat background (safe) or does it carry detail (loss)?
  const half = Math.floor(n / 2)
  const corners = [['top-left', 0, 0], ['top-right', half, 0], ['bottom-left', 0, half], ['bottom-right', half, half]]
  const tol = opts.tol
  const lost = Buffer.alloc(n * n)   // 255 where detail is being cut
  const rows = []
  let worst = null
  for (const [name, ox, oy] of corners) {
    const R = [], G = [], B = []
    const cut = []
    for (let y = oy; y < oy + half; y++) for (let x = ox; x < ox + half; x++) {
      if (m[y * n + x] >= 128) continue
      const o = (y * n + x) * C
      R.push(data[o]); G.push(data[o + 1]); B.push(data[o + 2]); cut.push([x, y])
    }
    if (!cut.length) continue
    const md = [median(R), median(G), median(B)]
    let detail = 0, deepest = 0
    for (let i = 0; i < cut.length; i++) {
      const d = Math.max(Math.abs(R[i] - md[0]), Math.abs(G[i] - md[1]), Math.abs(B[i] - md[2]))
      if (d > tol) {
        detail++
        lost[cut[i][1] * n + cut[i][0]] = 255
        // how far in from its own corner does the lost ink reach (diagonal depth)?
        const cx = ox === 0 ? 0 : n - 1, cy = oy === 0 ? 0 : n - 1
        const dep = Math.round(Math.hypot(cut[i][0] - cx, cut[i][1] - cy))
        if (dep > deepest) deepest = dep
      }
    }
    const pct = (detail / cut.length) * 100
    rows.push({ name, cut: cut.length, detail, pct, deepest, bg: md })
    if (!worst || pct > worst.pct) worst = rows[rows.length - 1]
  }

  console.log(`\n${path.relative(process.cwd(), file)} — iOS 26 mask cuts ${g.cutAreaPct.toFixed(1)}% of the square (${(g.cutAreaPct / 100 * n * n / 1000).toFixed(0)}k px)`)
  console.log('corner         cut px   detail cut   reaches   corner colour')
  for (const r of rows) {
    const flag = r.pct < 0.5 ? '✓' : r.pct < 5 ? '·' : '⚠'
    console.log(`${flag} ${r.name.padEnd(14)}${String(r.cut).padStart(6)}  ${(r.pct.toFixed(1) + '%').padStart(9)}  ${(r.deepest ? r.deepest + 'px' : '—').padStart(8)}   rgb(${r.bg.join(',')})`)
  }
  const anyLoss = rows.some(r => r.pct >= 5)
  console.log(anyLoss
    ? `\n⚠ real detail is being cut (worst: ${worst.name}, ${worst.pct.toFixed(1)}% of its cut region is not flat background).\n  Pull that content inside the circle guide, or extend the background out to the corners.`
    : `\n✓ the corners are flat background — the mask takes nothing but backdrop.`)

  if (opts.out) {
    const pane = Math.round(n / 2)
    const gap = Math.round(n / 32)
    // 1: as drawn, with the mask edge stroked
    const band = edgeBand(m, n, Math.max(2, Math.round(n / 340)))
    const p1 = Buffer.alloc(n * n * 4)
    // 2: as shown, masked onto a checkerboard so the cut reads as "gone"
    const p2 = Buffer.alloc(n * n * 4)
    // 3: what got cut — kept region dimmed, lost ink in magenta
    const p3 = Buffer.alloc(n * n * 4)
    const chk = Math.max(8, Math.round(n / 32))
    for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) {
      const i = y * n + x, o = i * 4, s = i * C
      const r = data[s], gg = data[s + 1], b = data[s + 2]
      const keep = m[i] / 255
      p1[o] = r; p1[o + 1] = gg; p1[o + 2] = b; p1[o + 3] = 255
      if (band[i]) { p1[o] = 0xff; p1[o + 1] = 0x2d; p1[o + 2] = 0x55 }
      const c = (((x / chk | 0) + (y / chk | 0)) % 2) ? 0x33 : 0x22
      p2[o] = Math.round(r * keep + c * (1 - keep)); p2[o + 1] = Math.round(gg * keep + c * (1 - keep))
      p2[o + 2] = Math.round(b * keep + c * (1 - keep)); p2[o + 3] = 255
      if (lost[i]) { p3[o] = 0xff; p3[o + 1] = 0x00; p3[o + 2] = 0xd0; p3[o + 3] = 255 }
      else if (keep < 0.5) { p3[o] = Math.round(r * 0.35 + 60); p3[o + 1] = Math.round(gg * 0.35); p3[o + 2] = Math.round(b * 0.35); p3[o + 3] = 255 }
      else { const l = Math.round((r * 0.3 + gg * 0.6 + b * 0.1) * 0.45); p3[o] = p3[o + 1] = p3[o + 2] = l; p3[o + 3] = 255 }
    }
    const W = pane * 3 + gap * 4, H = pane + gap * 2 + Math.round(gap * 1.6)
    const panels = []
    for (const px of [p1, p2, p3]) {
      panels.push(await sharp(px, { raw: { width: n, height: n, channels: 4 } }).resize(pane, pane, { kernel: 'lanczos3' }).png().toBuffer())
    }
    const labels = ['as drawn (red = mask edge)', 'as iOS shows it', 'cut: magenta = lost detail']
    const ty = pane + gap + Math.round(gap * 1.15)
    const svg = Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}">` +
      labels.map((t, i) => `<text x="${gap + i * (pane + gap) + pane / 2}" y="${ty}" font-family="Helvetica,Arial" font-size="${Math.round(gap * 0.8)}" fill="#cfd6e0" text-anchor="middle">${t}</text>`).join('') +
      '</svg>')
    let proof = sharp({ create: { width: W, height: H, channels: 4, background: { r: 0x14, g: 0x16, b: 0x1a, alpha: 1 } } })
      .composite(panels.map((input, i) => ({ input, left: gap + i * (pane + gap), top: gap })))
    try { proof = sharp(await proof.png().toBuffer()).composite([{ input: svg }]) } catch { /* no librsvg — skip labels */ }
    await proof.png().toFile(opts.out)
    console.log(`\n✓ proof → ${path.relative(process.cwd(), opts.out)}`)
  }
  if (opts.quiet && anyLoss) process.exit(1)
}

// ── preview: what it will actually look like ─────────────────────────────────────────────────────
// The pixel sizes iOS/the store really display an icon at. Only the iPhone home-screen number is
// MEASURED (192px on iOS 26.5 / iPhone 17 — iOS 26 grew the home-screen icon to 64pt @3x, so the
// familiar "180" is stale); the rest are Apple's nominal pt×scale and are labelled as such.
const DISPLAY_SIZES = [
  { px: 1024, label: 'App Store', note: 'product page' },
  { px: 192, label: 'home screen', note: '64pt @3x', measured: true },
  { px: 128, label: 'iPad home', note: '64pt @2x' },
  { px: 120, label: 'Spotlight', note: '40pt @3x' },
  { px: 87, label: 'Settings', note: '29pt @3x' },
  { px: 60, label: 'notification', note: '20pt @3x' }
]

// mask an icon at a given size and return a premultiplied-over-background RGBA buffer
async function maskedAt (file, n, bg, kernel) {
  const m = await loadMask(n)
  const { data, info } = await sharp(file).resize(n, n, { fit: 'cover', kernel }).ensureAlpha().raw()
    .toBuffer({ resolveWithObject: true })
  const C = info.channels
  const px = Buffer.alloc(n * n * 4)
  for (let i = 0; i < n * n; i++) {
    const a = (m[i] / 255) * (data[i * C + 3] / 255)
    for (let c = 0; c < 3; c++) px[i * 4 + c] = Math.round(data[i * C + c] * a + bg[c] * (1 - a))
    px[i * 4 + 3] = 255
  }
  return px
}

async function cmdPreview (file, opts) {
  if (!fs.existsSync(file)) { console.error(`✗ no such file: ${file}`); process.exit(1) }
  // 'mitchell', not lanczos: swept against the REAL device render (see `device`), mitchell was the
  // closest match to iOS's own downscale at 192px — mean |delta| 5.44/255 vs cubic 5.90, lanczos2
  // 5.94, lanczos3 6.93, nearest 11.82, all at zero pixel offset. The ~2% residual is filter-level
  // difference on high-contrast pixel art, not a treatment iOS applies.
  const kernel = opts.nearest ? 'nearest' : 'mitchell'
  const BIG = 384                                    // the 1024 tile, shown scaled down to fit
  const ROWS = [
    { name: 'light', bg: [0xf2, 0xf2, 0xf7], fg: '#1c1c1e' },
    { name: 'dark', bg: [0x1c, 0x1c, 0x1e], fg: '#e8e8ed' }
  ]
  const pad = 28, lab = 34, gap = 24
  const tiles = DISPLAY_SIZES.map(s => ({ ...s, draw: s.px > BIG ? BIG : s.px }))
  const rowW = tiles.reduce((a, t) => a + t.draw + gap, 0) - gap
  const rowH = Math.max(...tiles.map(t => t.draw)) + lab
  const W = rowW + pad * 2, H = ROWS.length * (rowH + pad)   // bands tile H exactly, no sliver

  const layers = []
  const texts = []
  let top = pad / 2
  for (const row of ROWS) {
    let left = pad
    for (const t of tiles) {
      let buf = await maskedAt(file, t.px, row.bg, kernel)
      let img = sharp(buf, { raw: { width: t.px, height: t.px, channels: 4 } })
      if (t.draw !== t.px) img = img.resize(t.draw, t.draw, { kernel: 'lanczos3' })
      layers.push({ input: await img.png().toBuffer(), left, top: top + (rowH - lab - t.draw) })
      const cx = left + t.draw / 2, ty = top + rowH - 10
      texts.push(`<text x="${cx}" y="${ty - 14}" fill="${row.fg}" font-family="Helvetica,Arial" font-size="15" font-weight="600" text-anchor="middle">${t.label}${t.measured ? ' ✓' : ''}</text>` +
        `<text x="${cx}" y="${ty}" fill="${row.fg}" opacity="0.6" font-family="Helvetica,Arial" font-size="12" text-anchor="middle">${t.px}px · ${t.note}${t.draw !== t.px ? ` (shown ${t.draw})` : ''}</text>`)
      left += t.draw + gap
    }
    top += rowH + pad
  }
  // backdrop bands, one per row, so each row reads as its own environment
  const bands = ROWS.map((r, i) => `<rect x="0" y="${i * (rowH + pad)}" width="${W}" height="${rowH + pad}" fill="rgb(${r.bg.join(',')})"/>`).join('')
  let out = sharp({ create: { width: W, height: H, channels: 4, background: { r: 0xf2, g: 0xf2, b: 0xf7, alpha: 1 } } })
    .composite([{ input: Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}">${bands}</svg>`) }])
  out = sharp(await out.png().toBuffer()).composite(layers)
  try {
    out = sharp(await out.png().toBuffer())
      .composite([{ input: Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}">${texts.join('')}</svg>`) }])
  } catch { /* no librsvg — skip labels */ }
  await out.png().toFile(opts.out)
  console.log(`✓ ${path.relative(process.cwd(), opts.out)}  ${path.basename(file)} as iOS will show it, light + dark`)
  console.log(`  sizes: ${DISPLAY_SIZES.map(s => s.px + (s.measured ? '✓' : '')).join(' · ')}   (✓ = measured on a real iOS 26.5 home screen; rest are Apple's pt×scale)`)
  console.log(`  scaled with ${opts.nearest ? 'NEAREST (crisp, but NOT what iOS does)' : 'mitchell — the closest match to iOS\'s own downscale, measured against a device render (~2% residual)'}`)
  console.log(`  the mask is applied, and nothing else: a flat asset-catalog PNG gets NO gloss on iOS 26 (verified — see \`device\`)`)
}

// ── device: the real thing, not a prediction ─────────────────────────────────────────────────────
function sh (cmd, args, opts = {}) {
  return execFileSync(cmd, args, { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'], ...opts })
}
function pickSim (want) {
  const list = JSON.parse(sh('xcrun', ['simctl', 'list', 'devices', 'available', '--json'])).devices
  const all = []
  for (const [runtime, devs] of Object.entries(list)) for (const d of devs) all.push({ ...d, runtime })
  if (want) {
    const hit = all.find(d => d.udid === want || d.name === want)
    if (!hit) throw new Error(`no such simulator: ${want}`)
    return hit
  }
  const booted = all.filter(d => d.state === 'Booted')
  const ios26 = (d) => /iOS-2[6-9]/.test(d.runtime)
  return booted.find(ios26) || booted[0] ||
    all.filter(d => ios26(d) && /iPhone/.test(d.name)).sort((a, b) => a.name.localeCompare(b.name))[0] ||
    all.find(d => /iPhone/.test(d.name))
}
// largest roughly-square connected component of a boolean mask over a W×H field
function biggestBlob (hit, W, H, minSide) {
  const seen = new Uint8Array(W * H)
  let best = null
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
    if (seen[y * W + x] || !hit(x, y)) continue
    const st = [[x, y]]; seen[y * W + x] = 1
    let n = 0, x0 = W, y0 = H, x1 = -1, y1 = -1
    while (st.length) {
      const [px, py] = st.pop(); n++
      if (px < x0) x0 = px; if (py < y0) y0 = py; if (px > x1) x1 = px; if (py > y1) y1 = py
      for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
        const nx = px + dx, ny = py + dy
        if (nx < 0 || ny < 0 || nx >= W || ny >= H || seen[ny * W + nx] || !hit(nx, ny)) continue
        seen[ny * W + nx] = 1; st.push([nx, ny])
      }
    }
    const w = x1 - x0 + 1, h = y1 - y0 + 1
    const square = w >= minSide && h >= minSide && w / h > 0.75 && h / w > 0.75
    if (square && (!best || n > best.n)) best = { n, x0, y0, x1, y1, w, h }
  }
  return best
}

async function cmdDevice (file, opts) {
  if (!fs.existsSync(file)) { console.error(`✗ no such file: ${file}`); process.exit(1) }
  // a simulator .app to borrow — we only swap its icon, we never run it
  const donors = [
    'ios/build/Build/Products/Debug-iphonesimulator',
    'ios/build/Build/Products/Release-iphonesimulator'
  ].flatMap(d => glob(path.join(ROOT, d), /\.app$/))
  if (!donors.length) {
    console.error('✗ no simulator .app to borrow. Build one first: (cd ios && ./build.sh) — or run `preview`, which needs nothing.')
    process.exit(1)
  }
  const dev = pickSim(opts.sim)
  console.log(`· simulator: ${dev.name} (${dev.runtime.replace(/.*SimRuntime\./, '')}) ${dev.state}`)
  if (dev.state !== 'Booted') { sh('xcrun', ['simctl', 'boot', dev.udid]); sh('xcrun', ['simctl', 'bootstatus', dev.udid, '-b']) }

  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'icondev-'))
  const app = path.join(tmp, 'IconProbe.app')
  sh('cp', ['-R', donors[0], app])
  for (const junk of ['PlugIns', '__preview.dylib']) fs.rmSync(path.join(app, junk), { recursive: true, force: true })
  for (const f of fs.readdirSync(app)) if (f.endsWith('.storekit')) fs.rmSync(path.join(app, f), { force: true })
  const BUNDLE = 'com.dreamengine.iconprobe'
  for (const [k, v] of [['CFBundleIdentifier', BUNDLE], ['CFBundleName', 'IconProbe'], ['CFBundleDisplayName', 'IconProbe']]) {
    try { sh('plutil', ['-replace', k, '-string', v, path.join(app, 'Info.plist')]) } catch { sh('plutil', ['-insert', k, '-string', v, path.join(app, 'Info.plist')]) }
  }
  const set = path.join(tmp, 'Assets.xcassets/AppIcon.appiconset')
  fs.mkdirSync(set, { recursive: true })
  await sharp(file).resize(1024, 1024, { fit: 'cover' }).removeAlpha().png().toFile(path.join(set, 'icon-1024.png'))
  fs.writeFileSync(path.join(set, 'Contents.json'),
    '{ "images": [{ "filename": "icon-1024.png", "idiom": "universal", "platform": "ios", "size": "1024x1024" }], "info": { "author": "xcode", "version": 1 } }\n')
  sh('xcrun', ['actool', '--compile', app, '--app-icon', 'AppIcon', '--platform', 'iphonesimulator',
    '--minimum-deployment-target', '18.0', '--target-device', 'iphone',
    '--output-partial-info-plist', path.join(tmp, 'partial.plist'), path.join(tmp, 'Assets.xcassets')])

  // make sure it isn't already there, so the AFTER shot really is "one new icon"
  try { sh('xcrun', ['simctl', 'uninstall', dev.udid, BUNDLE]) } catch {}
  const shot = (name) => { const p = path.join(tmp, name); sh('xcrun', ['simctl', 'io', dev.udid, 'screenshot', p]); return p }
  const before = shot('before.png')
  sh('xcrun', ['simctl', 'install', dev.udid, app])

  const A = await sharp(before).removeAlpha().raw().toBuffer({ resolveWithObject: true })
  const W = A.info.width, H = A.info.height, C = A.info.channels
  // The new icon = the biggest roughly-square region that CHANGED between the two shots. But right
  // after install the home screen shows the icon DIMMED under an install progress ring, so poll
  // until two consecutive shots agree on that region — otherwise we'd photograph the ring.
  let after = null, B = null, blob = null, prev = null
  for (let attempt = 0; attempt < 14; attempt++) {
    sh('sleep', ['1'])
    after = shot(`after${attempt}.png`)
    B = await sharp(after).removeAlpha().raw().toBuffer({ resolveWithObject: true })
    const changed = (x, y) => {
      const o = (y * W + x) * C
      return Math.max(Math.abs(A.data[o] - B.data[o]), Math.abs(A.data[o + 1] - B.data[o + 1]), Math.abs(A.data[o + 2] - B.data[o + 2])) > 24
    }
    blob = biggestBlob(changed, W, H, 48)
    if (!blob) { prev = null; continue }
    const snap = Buffer.alloc(blob.w * blob.h * 3)
    for (let y = 0; y < blob.h; y++) for (let x = 0; x < blob.w; x++) {
      const o = ((blob.y0 + y) * W + blob.x0 + x) * C, t = (y * blob.h + x) * 3
      snap[t] = B.data[o]; snap[t + 1] = B.data[o + 1]; snap[t + 2] = B.data[o + 2]
    }
    if (prev && prev.length === snap.length) {
      let d = 0
      for (let i = 0; i < snap.length; i++) d += Math.abs(snap[i] - prev[i])
      if (d / snap.length < 2) break                       // settled
    }
    prev = snap
  }
  if (!blob) {
    console.error('✗ could not find the new icon in the home-screen screenshot. Is the home screen showing (not an open app)?')
    console.error(`  screenshots kept for inspection: ${tmp}`)
    process.exit(1)
  }
  const n = Math.max(blob.w, blob.h)
  await sharp(after).extract({ left: blob.x0, top: blob.y0, width: blob.w, height: blob.h }).png().toFile(opts.out)

  // silhouette check: the device's own edge vs our committed mask, subpixel, per row
  const mk = await loadMask(n)
  const dat = (await sharp(after).extract({ left: blob.x0, top: blob.y0, width: blob.w, height: blob.h })
    .resize(n, n, { fit: 'fill' }).removeAlpha().raw().toBuffer())
  const bgo = (blob.y0 - 6) * W * C + Math.max(0, blob.x0 - 6) * C  // wallpaper just outside the icon
  const bg = [B.data[bgo], B.data[bgo + 1], B.data[bgo + 2]]
  const isIcon = (x, y) => {
    const o = (y * n + x) * 3
    return Math.max(Math.abs(dat[o] - bg[0]), Math.abs(dat[o + 1] - bg[1]), Math.abs(dat[o + 2] - bg[2])) > 24 ? 255 : 0
  }
  const edge = (f, y) => { for (let x = 0; x < n - 1; x++) { const a = f(x, y), b = f(x + 1, y); if (a < 128 && b >= 128) return x + (128 - a) / (b - a); if (a >= 128) return x } return NaN }
  // Skip the top/bottom few rows: there the boundary runs almost horizontally, so a row-scan for a
  // vertical edge is ill-conditioned and a sub-pixel vertical offset reads as a large horizontal one.
  let s = 0, k = 0, mx = 0
  for (let y = 4; y < n - 4; y++) {
    const a = edge(isIcon, y), b = edge((x, yy) => mk[yy * n + x], y)
    if (!isFinite(a) || !isFinite(b)) continue
    const d = Math.abs(a - b); s += d; k++; if (d > mx) mx = d
  }
  console.log(`✓ ${path.relative(process.cwd(), opts.out)}  the REAL device render, ${blob.w}×${blob.h} px`)
  if (k > 8) console.log(`  silhouette vs our committed mask: mean ${(s / k).toFixed(2)}px, max ${mx.toFixed(2)}px over ${k} rows — the prediction holds`)
  else console.log('  (silhouette compare skipped: the icon art is too close to the wallpaper to key an edge)')
  if (!opts.keep) { try { sh('xcrun', ['simctl', 'uninstall', dev.udid, BUNDLE]) } catch {} ; fs.rmSync(tmp, { recursive: true, force: true }) }
  else console.log(`  kept: probe app installed, workdir ${tmp}`)
}

// ── rebuild / self-check ────────────────────────────────────────────────────────────────────────
async function cmdRebuild (verifyOnly) {
  const ict = findIctool()
  if (!ict) {
    if (verifyOnly) { console.log('· icon-mask: ictool not found (needs Xcode 26) — skipped'); return }
    console.error('✗ ictool not found. Needs Xcode 26+ ("Icon Composer.app/Contents/Executables/ictool").')
    process.exit(1)
  }
  const raw = await deriveMask(ict)
  const { out, maxDev } = symmetrise(raw, MASTER)
  if (verifyOnly) {
    if (!fs.existsSync(MASK_FILE)) { console.error('✗ icon-mask: committed mask missing'); process.exit(1) }
    const have = await sharp(MASK_FILE).greyscale().raw().toBuffer()
    let md = 0
    for (let i = 0; i < out.length; i++) md = Math.max(md, Math.abs(have[i] - out[i]))
    if (md > 2) { console.error(`✗ icon-mask: committed mask differs from ictool by ${md}/255 — run: node tools/icon-mask.js rebuild`); process.exit(1) }
    console.log(`✓ icon-mask: committed mask matches ictool (max delta ${md}/255)`)
    return
  }
  fs.mkdirSync(MASK_DIR, { recursive: true })
  await sharp(out, { raw: { width: MASTER, height: MASTER, channels: 1 } }).png({ compressionLevel: 9 }).toFile(MASK_FILE)
  const g = geometry(out, MASTER)
  console.log(`✓ ${path.relative(ROOT, MASK_FILE)}  ${MASTER}x${MASTER}  (${(fs.statSync(MASK_FILE).size / 1024).toFixed(0)} kB)`)
  console.log(`  source: ${ict}`)
  console.log(`  8-fold asymmetry before symmetrising: ${maxDev}/255 (small = the shape really is 4-fold symmetric)`)
  console.log(`  cuts ${g.cutAreaPct.toFixed(2)}% of the square; flat side ${g.flatSidePct.toFixed(1)}%; corner run ${(g.cornerRun / MASTER * 1024).toFixed(0)}px at 1024`)
}

// ── cli ─────────────────────────────────────────────────────────────────────────────────────────
function usage () {
  console.log(`icon-mask — app-icon mask template + "what gets cut" oracle

  node tools/icon-mask.js template [--size 1024] [--out f.png] [--overlay] [--plain] [--inset px]
  node tools/icon-mask.js mask     [--size 1024] [--out f.png] [--inset px]
  node tools/icon-mask.js check    <icon.png> [--out proof.png] [--tol 24] [--quiet]
  node tools/icon-mask.js preview  <icon.png> [--out sheet.png] [--nearest]
  node tools/icon-mask.js device   <icon.png> [--sim <udid|name>] [--out f.png] [--keep]
  node tools/icon-mask.js rebuild        re-derive the mask from Apple's ictool
  node tools/icon-mask.js --check        gate: committed mask still matches ictool`)
}

;(async () => {
  const argv = process.argv.slice(2)
  const flag = (name, def) => { const i = argv.indexOf('--' + name); return i < 0 ? def : argv[i + 1] }
  const has = (name) => argv.includes('--' + name)
  const cmd = argv.find(a => !a.startsWith('--')) || (has('check') ? '--check' : 'template')

  if (has('help') || has('h')) return usage()
  if (cmd === '--check' || (has('check') && !argv.some(a => !a.startsWith('--')))) return cmdRebuild(true)
  const size = Math.max(64, Math.min(4096, parseInt(flag('size', '1024'), 10) || 1024))

  const inset = Math.max(0, parseInt(flag('inset', '0'), 10) || 0)

  if (cmd === 'rebuild') return cmdRebuild(false)
  if (cmd === 'template') return cmdTemplate({ size, inset, overlay: has('overlay'), plain: has('plain'),
    out: flag('out', path.join(ROOT, `build/icon-template-${size}${has('overlay') ? '-overlay' : ''}.png`)) })
  if (cmd === 'mask') return cmdMask({ size, inset, out: flag('out', path.join(ROOT, `build/icon-mask-${size}${inset ? '-inset' + inset : ''}.png`)) })
  // <cmd> <icon.png> commands share one default-output convention: build/<parent>-<file>-<suffix>.png
  const arg1 = () => argv.filter(a => !a.startsWith('--'))[1]
  const defOut = (file, suffix) => {
    const stem = (path.basename(path.dirname(path.resolve(file))) + '-' + path.basename(file)).replace(/\.png$/i, '')
    return path.join(ROOT, 'build', `${stem}-${suffix}.png`)
  }
  if (cmd === 'preview') {
    const file = arg1()
    if (!file) { console.error('✗ preview needs an icon path'); process.exit(1) }
    return cmdPreview(file, { nearest: has('nearest'), out: flag('out', defOut(file, 'preview')) })
  }
  if (cmd === 'device') {
    const file = arg1()
    if (!file) { console.error('✗ device needs an icon path'); process.exit(1) }
    return cmdDevice(file, { sim: flag('sim', null), keep: has('keep'), out: flag('out', defOut(file, 'device')) })
  }
  if (cmd === 'check') {
    const file = arg1()
    if (!file) { console.error('✗ check needs an icon path'); process.exit(1) }
    // name the proof after <parent>-<file> — every app's icon is called "icon.png", so a plain
    // basename would have each check silently overwrite the last one's proof.
    return cmdCheck(file, { out: has('no-proof') ? null : flag('out', defOut(file, 'maskproof')), tol: parseInt(flag('tol', '24'), 10), quiet: has('quiet') })
  }
  usage(); process.exit(1)
})().catch(e => { console.error('✗ ' + (e.stack || e.message)); process.exit(1) })
