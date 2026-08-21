// build-app.js — build a MULTI-CART APP binary from an app manifest (apps/<name>/app.json).
// The umbrella-app build-ladder rung 2 (docs/design/share-panel.md §ladder): generates
// everything tools/bundle-spike/build.sh hardcoded, so ADDING A RACK = ONE MANIFEST LINE.
//
//   node tools/build-app.js tinyjam            # apps/tinyjam/app.json → build/tinyjam
//   node tools/build-app.js apps/tinyjam       # same
//   node tools/build-app.js tinyjam --run      # build then launch it
//   node tools/build-app.js tinyjam --mac      # build then wrap → build/<Name>.app
//                                              #   (rung 4: mac-app.sh signs+notarizes+staples;
//                                              #    name+bundleId come from the manifest. Add
//                                              #    --no-notarize for a quick local-only .app.)
//   node tools/build-app.js --check            # CAN EVERY apps/*/app.json STILL BE STAGED? the gate
//   node tools/build-app.js tinyjam --dry      #   one app: validate + generate, stop before clang
//   node tools/build-app.js --selfcheck        #   known-answer fixture for --check (20 assertions)
//
// ⚠ WHY --check EXISTS. Nothing in this repo ran build-app.js: only a human about to ship did. So
// when SOUND_CART_CTX moved into the generated sound_ctx.h during the per-instance work and the
// parser here still grepped sound.h, EVERY app build was dead for weeks behind a fully green health
// board — and the error named the symbol while blaming the wrong file, so it read like engine
// corruption rather than a stale path. --check stages every manifest through the REAL front half
// (--dry, one child process each, so one broken app reports and the sweep continues).
// It stops before the first clang, so a cart that no longer COMPILES still breaks an archive and
// this stays green: that half is build-all.js. This covers what build-all cannot see — the manifest,
// the roster, the ctx cap, the icon path, the engine constants the builder reads.
//
// Manifest (apps/<name>/app.json — the committed home per share-panel.md open-q #1 +
// ADR-0026's metadata-next-to-manifest layout):
//   { "name": "Tinyjam", "bundleId": "com.dreamengine.tinyjam", "version": "0.1",
//     "carts": ["acidrack", "yachtrack"],          // tools/carts/<c>.c, in context order
//     "launcher": "tinyjam-menu",                  // optional menu cart (rung 3): boots first,
//                                                  // gets ctx 0 + a generated app_roster.h from
//                                                  // each rack's de:meta; TAB = back to it
//     "icon": "apps/<app>/icon.png",               // 1024x1024, OPAQUE (no alpha). iOS masks it to
//                                                  // a squircle and eats 6.1% — check before you
//                                                  // ship: tools/icon-mask.js check/preview
//                                                  // (staging below prints the verdict)
//     "orientation": "landscape",                  // optional lock. ⚠ WITHHOLDS the resizable
//                                                  // build (a resizable cart asks UIKit for free
//                                                  // rotation) — ios/app-flags.sh explains why the
//                                                  // two cannot both be had yet
//     "micUsage": "why this app listens…",         // REQUIRED IFF a cart calls mic_start(), and
//                                                  // refused otherwise: derived below, never
//                                                  // declared. Missing it on a mic app is an iOS
//                                                  // kill at the permission prompt
//     "auCart": "epiano",                          // ship this cart as an AUv3 extension. Absent →
//                                                  // testflight.sh strips the AU target entirely
//     "auName": "Studio: My App",                  // ⚠ the AU identity, and the two CODES are
//     "auSubtype": "abcd",                         //   FOREVER: a DAW stores (type,subtype,manufacturer)
//     "auManufacturer": "Abcd",                    //   to re-instantiate a saved plug-in. 4 chars,
//     "auDisplayName": "My App",                   //   one uppercase in the manufacturer. Required
//                                                  //   when auCart is set — ios/au-identity.sh
//     "iap": {}                                    // parked: mac-app / iOS rungs
//   }
//
// What it does:
//   1. Validates: carts exist, count ≤ SOUND_CART_CTX (parsed live from runtime/sound.h),
//      every cart's screen/cell/map dims MATCH (share-panel.md next-spike #3: the manifest
//      picks ONE size — no letterboxing yet).
//   2. Bakes a sprite sheet PER cart (in ctx order) into an indexed table
//      (SPRITES_SHEETS[]/SPRITES_MULTI) so de_sheet_select(ctx) swaps sheets on switch —
//      no cross-cart art bleed (share-panel.md next-spike #1, DONE). No sprites anywhere →
//      a single blank sheet. Maps are still the first cart's (per-cart maps = a later rung).
//   3. Compiles each cart TU with -Ddraw=<slug>_draw -Dupdate=<slug>_update
//      -Dinit=<slug>_init -Dspec=<slug>_spec (everything else in a cart is `static`,
//      so the TUs link clean side by side), then asks `nm` which optional entry points
//      (init/update/spec) the cart actually defines.
//   4. Generates the dispatcher shim: each switch = de_switch_cart(ctx) (the per-cart
//      sound context, ADR-0027 / rung 1) + a one-time init. Without a launcher TAB
//      blind-cycles the carts; with one, the launcher boots first at ctx 0, its
//      app_launch(i) opens rack i, and TAB toggles rack <-> overview. The launcher TU
//      compiles with -DAPP_BUNDLE and #includes the generated app_roster.h (one entry
//      per rack: title + summary straight from the rack's de:meta — adding a rack to
//      the manifest auto-adds its menu entry).
//      DE_BUNDLE_AUTOSWITCH=<N> auto-cycles every N frames — the headless proof hook.
//   5. Links with studio.c + raylib → build/<app name in the manifest, lowercased>.
//
// Verify a build: DE_BUNDLE_AUTOSWITCH=400 build/<name> --headless + live screenshots,
// and tools/bundle-spike/proof-sound.sh covers the underlying context mechanism.
// The old hand-written spike (bundle-spike/build.sh) stays as the minimal reference.
'use strict'
const fs = require('fs')
const path = require('path')
const { execFileSync, spawnSync } = require('child_process')
const mk = require(path.join(__dirname, 'make-cart.js'))

const ROOT = path.join(__dirname, '..')

// ── args ─────────────────────────────────────────────────────────────────────
const args = process.argv.slice(2)
const run = args.includes('--run')
const mac = args.includes('--mac')            // rung 4: wrap the binary into a signed+notarized .app
const noNotarize = args.includes('--no-notarize')  // local .app test build (skips notarize+staple)
const ios = args.includes('--ios')            // Spike A: stage the multi-cart sources for the iOS Xcode build
const dry = args.includes('--dry')            // validate + generate, stop before the first compile (see --check)
const check = args.includes('--check')        // every manifest through --dry; the repo-doctor row
const target = args.find((a, i) => !a.startsWith('--') && args[i - 1] !== '--apps-dir')

// ── --selfcheck: can the gate above go RED? ──────────────────────────────────
// A gate never seen to fail is indistinguishable from one gone blind (tools/gate-controls.js). This
// one is especially exposed to that: its steady state is PASS, and it would go quietly green if the
// fan-out stopped finding manifests, if a child's nonzero status stopped being read, or if `--dry`
// started exiting 0 on a manifest it never actually parsed. So: a fixture tree of KNOWN-BAD apps,
// each broken a different way, plus a known-good control that must still pass beside them.
if (args.includes('--selfcheck')) {
  const dir = path.join(ROOT, 'build', '.app-selfcheck')
  fs.rmSync(dir, { recursive: true, force: true })
  const write = (n, j) => { fs.mkdirSync(path.join(dir, n), { recursive: true })
    fs.writeFileSync(path.join(dir, n, 'app.json'), typeof j === 'string' ? j : JSON.stringify(j)) }
  const good = JSON.parse(fs.readFileSync(path.join(ROOT, 'apps/tinyacidjam/app.json'), 'utf8'))
  const cases = [
    ['aa_control',    { ...good, name: 'Control' },                                     true,  null],
    ['bad_cart',      { ...good, carts: ['no_such_cart_exists'] },                      false, /cart not found/],
    ['bad_overflow',  { ...good, carts: ['acidcandy','epiano','moog','tb303','sh101','omnichord','otamatone','pedalboard','combo'] },
                                                                                        false, /SOUND_CART_CTX/],
    ['bad_nocarts',   { name: 'No Carts', bundleId: 'x.y' },                            false, /needs at least/],
    ['bad_json',      '{ this is not json',                                             false, /./],
    ['bad_icon',      { ...good, icon: 'apps/tinyacidjam/nope.png' },                   false, /icon/i],
  ]
  for (const [n, j] of cases) write(n, j)
  const r = spawnSync(process.execPath, [__filename, '--check', '--apps-dir', dir], { cwd: ROOT, encoding: 'utf8' })
  const out = (r.stdout || '') + (r.stderr || '')
  let pass = 0, fail = 0
  const assert = (ok, what) => { if (ok) pass++; else { fail++; console.log(`  ✗ ${what}`) } }
  for (const [n, , shouldPass, why] of cases) {
    assert(new RegExp(`[✓✗] ${n}$`, 'm').test(out), `${n}: reported at all`)
    assert(new RegExp(`${shouldPass ? '✓' : '✗'} ${n}$`, 'm').test(out), `${n}: expected ${shouldPass ? 'PASS' : 'FAIL'}`)
    if (why) {
      const seg = out.split(new RegExp(`✗ ${n}$`, 'm'))[1] || ''
      assert(why.test(seg.split(/^[ ]{2}[✓✗]/m)[0] || ''), `${n}: reason matches ${why}`)
    }
  }
  // the sweep must not ABORT on the first bad one — the control sorts first, a broken app follows,
  // and the last case must still have been reported. That is the property a naive loop loses.
  assert(/[✓✗] bad_nocarts$/m.test(out), 'sweep continues past a failure')
  assert(r.status === 1, 'exits 1 when any manifest is broken')
  const clean = spawnSync(process.execPath, [__filename, '--check'], { cwd: ROOT, encoding: 'utf8' })
  assert(clean.status === 0, 'exits 0 on the REAL apps/ tree (else this gate is just broken)')
  fs.rmSync(dir, { recursive: true, force: true })
  console.log(fail ? `build-app --selfcheck: ${pass} ok, ${fail} FAILED` : `build-app --selfcheck: ${pass}/${pass} known answers correct`)
  process.exit(fail ? 1 : 0)
}

// ── --check: can every app still be staged? ──────────────────────────────────
// WHY THIS EXISTS: nothing in the repo ran build-app.js except a human about to ship, so the STORE
// PATH could be fatally broken while every gate stayed green — and was. `SOUND_CART_CTX` moved into
// the generated sound_ctx.h during the per-instance work and the parser above still grepped sound.h,
// so EVERY app build died on a message that named the symbol and blamed the wrong file. Nobody
// noticed until an archive was attempted, weeks later.
//
// It runs each manifest through the REAL front half (`--dry`), in a child process so one broken app
// reports and the sweep continues rather than aborting on the first. Re-implementing the checks here
// would prove nothing about the code that actually ships; that is the whole point.
//
// ⚠ WHAT IT DOES NOT COVER, and say so rather than let a green row imply it: it stops before the
// first `clang`, so a cart that no longer COMPILES still breaks the archive and this stays green.
// That gap is `tools/build-all.js`'s (every cart vs the current studio.h). This gate covers the
// part build-all cannot see — the manifest, the roster, the ctx cap, the engine constants it reads —
// which is exactly where the break was.
if (check) {
  const ai = args.indexOf('--apps-dir')          // --selfcheck points this at a fixture tree
  const appsDir = ai >= 0 ? path.resolve(args[ai + 1]) : path.join(ROOT, 'apps')
  const manifests = fs.readdirSync(appsDir)
    .filter(d => fs.existsSync(path.join(appsDir, d, 'app.json'))).sort()
  if (!manifests.length) { console.error('✗ no apps/*/app.json found — that is itself suspicious'); process.exit(1) }
  let bad = 0
  for (const name of manifests) {
    // an ABSOLUTE manifest path, so a fixture tree outside apps/ resolves the same way
    const r = spawnSync(process.execPath, [__filename, path.join(appsDir, name, 'app.json'), '--dry'],
      { cwd: ROOT, encoding: 'utf8' })
    if (r.status === 0) {
      console.log(`  ✓ ${name}`)
    } else {
      bad++
      const why = ((r.stderr || '') + (r.stdout || '')).trim().split('\n').filter(Boolean)
      console.log(`  ✗ ${name}`)
      for (const l of why.slice(-3)) console.log(`      ${l}`)
    }
  }
  console.log(bad
    ? `✗ ${bad}/${manifests.length} app manifest(s) cannot be staged (does NOT cover per-cart compiles — that is build-all.js)`
    : `ok — ${manifests.length} app manifest(s) stage (front half only; per-cart compiles are build-all.js)`)
  process.exit(bad ? 1 : 0)
}

if (!target) {
  console.error('usage: node tools/build-app.js <app-name | apps/<name>[/app.json]> [--run] [--mac [--no-notarize]] [--ios]')
  console.error('       node tools/build-app.js --check      # can every apps/*/app.json still be staged?')
  process.exit(1)
}

// ── load manifest ────────────────────────────────────────────────────────────
let manifestPath = target
if (!manifestPath.endsWith('app.json')) manifestPath = path.join(manifestPath, 'app.json')
if (!fs.existsSync(path.join(ROOT, manifestPath)) && !path.isAbsolute(manifestPath))
  manifestPath = path.join('apps', target, 'app.json')
const manifestFile = path.isAbsolute(manifestPath) ? manifestPath : path.join(ROOT, manifestPath)
if (!fs.existsSync(manifestFile)) {
  console.error(`no manifest at ${manifestPath} — expected apps/<name>/app.json (try: ls apps/)`)
  process.exit(1)
}
const app = JSON.parse(fs.readFileSync(manifestFile, 'utf8'))
if (!app.name || !Array.isArray(app.carts) || app.carts.length === 0) {
  console.error('manifest needs at least: { "name": "...", "carts": ["cart", ...] }')
  process.exit(1)
}
// (manifest "iap" used to be parked-and-ignored here; it is READ now — see the two axes below.)
// Icon existence is validated HERE rather than only at the --ios staging step far below, so that a
// manifest pointing at a moved icon is caught by `--check` instead of at archive time. Found by the
// selfcheck: the bad-icon fixture PASSED, because the only check was past the compile boundary.
if (app.icon && !fs.existsSync(path.join(ROOT, app.icon))) {
  console.error(`✗ manifest "icon" not found: ${app.icon}`); process.exit(1)
}

// ── cap: contexts the engine actually has (never drifts from the engine) ──────
// Read from wherever the define LIVES, not from where it once lived. It moved out of sound.h into
// the GENERATED sound_ctx.h during the per-instance context work (sound.h includes that header), and
// because this only grepped sound.h, EVERY app build died at "could not parse SOUND_CART_CTX" — an
// error that names the symbol and blames the wrong file, so it reads like engine corruption rather
// than a stale path. Search both, in the order the value is most likely to be found today.
const CTX_HOMES = ['runtime/sound_ctx.h', 'runtime/sound.h']
let ctxCap = 0, ctxFrom = ''
for (const rel of CTX_HOMES) {
  const p = path.join(ROOT, rel)
  if (!fs.existsSync(p)) continue
  const m = fs.readFileSync(p, 'utf8').match(/#define\s+SOUND_CART_CTX\s+(\d+)/)
  if (m) { ctxCap = parseInt(m[1], 10); ctxFrom = rel; break }
}
if (!ctxCap) {
  console.error(`could not find "#define SOUND_CART_CTX <n>" in any of: ${CTX_HOMES.join(', ')}`)
  console.error('  it moved again — add its new home to CTX_HOMES above.')
  process.exit(1)
}
const nCtx = app.carts.length + (app.launcher ? 1 : 0) // the launcher gets a ctx slot of its own
if (nCtx > ctxCap) {
  console.error(`${nCtx} contexts (carts${app.launcher ? ' + launcher' : ''}) > SOUND_CART_CTX (${ctxCap}) — raise it in ${ctxFrom} first`)
  process.exit(1)
}

// ── per-cart config + the one-screen-size rule ───────────────────────────────
const slugOf = c => (/^\d/.test(c) ? 'c_' + c : c).replace(/[^A-Za-z0-9]/g, '_')
const carts = app.carts.map(c => {
  const src = path.join(ROOT, 'tools/carts', c + '.c')
  if (!fs.existsSync(src)) { console.error(`cart not found: tools/carts/${c}.c`); process.exit(1) }
  const cfg = mk.loadConfig(src)
  // resizable lives in the de:meta block (not the .cart.js loadConfig reads) — parse it here so the
  // app builds with -DDE_RESIZABLE and reflows to fill the device (canvas-density-spectrum.md).
  let resizable = false
  try { const m = fs.readFileSync(src, 'utf8').match(/\/\*\s*de:meta\s*\n([\s\S]*?)\nde:meta\s*\*\//); resizable = m ? JSON.parse(m[1]).resizable === true : false } catch {}
  return { name: c, slug: slugOf(c), src, cfg, resizable }
})
const dims = c => ({
  screenW: c.cfg.screenW ?? 320, screenH: c.cfg.screenH ?? 200, scale: c.cfg.scale ?? 4,
  cellW: c.cfg.cellW ?? 16, cellH: c.cfg.cellH ?? 16, mapW: c.cfg.mapW ?? 128, mapH: c.cfg.mapH ?? 64,
})
const d0 = dims(carts[0])
for (const c of carts.slice(1)) {
  const d = dims(c)
  for (const k of ['screenW', 'screenH', 'cellW', 'cellH', 'mapW', 'mapH'])
    if (d[k] !== d0[k]) {
      console.error(`screen/grid mismatch: ${carts[0].name} ${k}=${d0[k]} but ${c.name} ${k}=${d[k]}`)
      console.error('a manifest picks ONE size and its carts must match (share-panel.md next-spike #3)')
      process.exit(1)
    }
}

// device-adaptive: build with -DDE_RESIZABLE when EVERY cart opts in (de:meta.resizable), so the
// cart reflows to fill the device instead of letterboxing (the black bars). de_reflow is binary-wide
// and a multi-cart launcher isn't reflow-aware yet, so a mixed/multi app stays fixed for safety.
const appResizable = carts.length > 0 && carts.every(c => c.resizable)
if (appResizable) console.log(`resizable: all ${carts.length} cart(s) opt in → -DDE_RESIZABLE (reflows to fill the device)`)
else if (carts.some(c => c.resizable)) console.log('resizable: NOT all carts opt in → fixed/letterboxed build (make every cart resizable to enable fill)')

// the launcher cart (rung 3): dims-exempt — it draws relative to SCREEN_W/H, so it's
// compiled at the app's size, whatever its own .cart.js (usually none) would say
let launcher = null
if (app.launcher) {
  const src = path.join(ROOT, 'tools/carts', app.launcher + '.c')
  if (!fs.existsSync(src)) { console.error(`launcher cart not found: tools/carts/${app.launcher}.c`); process.exit(1) }
  launcher = { name: app.launcher, slug: slugOf(app.launcher), src, cfg: mk.loadConfig(src) }
  if (launcher.cfg.sprites)
    console.log(`note: launcher ${app.launcher} has a sprite config — ignored (the launcher is code-drawn; it gets a blank sheet in the per-cart sheet table)`)
}

// ── staging ──────────────────────────────────────────────────────────────────
const appId = app.name.toLowerCase().replace(/[^a-z0-9]/g, '')
const stage = path.join(ROOT, 'build', '.app-' + appId)
fs.mkdirSync(stage, { recursive: true })

const cHeader = (sym, buf) => {
  let s = `static const unsigned char ${sym}[] = {\n`
  for (let i = 0; i < buf.length; i += 16)
    s += '  ' + Array.from(buf.slice(i, i + 16)).join(', ') + ',\n'
  return s + `};\nstatic const unsigned int ${sym}_LEN = ${buf.length};\n`
}

// PER-CART sprite sheets — one baked sheet per unit, in ctx order (launcher first when
// present, then racks) so de_switch_cart(ctx) → de_sheet_select(ctx) swaps the active
// sheet and one cart's art can't bleed into the next. The launcher is code-drawn, so it
// takes a blank sheet. When NO cart has sprites, fall back to a single blank sheet (the
// engine's SPRITES_MULTI path stays off — nothing to swap).
const sheetUnits = launcher ? [launcher, ...carts] : carts
const sheetFor = u => (u !== launcher && u.cfg && u.cfg.sprites)
  ? mk.buildSpriteSheet(u.cfg.sprites, u.cfg.charMap)
  : mk.makeBlankSpritePng()
const anySprites = sheetUnits.some(u => u !== launcher && u.cfg && u.cfg.sprites)
if (anySprites) {
  const sheets = sheetUnits.map(sheetFor)
  let h = '// GENERATED by tools/build-app.js — per-cart sprite sheets, indexed by de_switch_cart ctx.\n'
  sheets.forEach((buf, i) => { h += cHeader('SPRITES_DATA_' + i, buf) })
  h += `#define SPRITES_MULTI ${sheets.length}\n`
  h += `static const unsigned char *const SPRITES_SHEETS[SPRITES_MULTI] = { ${sheets.map((_, i) => 'SPRITES_DATA_' + i).join(', ')} };\n`
  h += `static const unsigned int SPRITES_SHEET_LENS[SPRITES_MULTI] = { ${sheets.map((_, i) => 'SPRITES_DATA_' + i + '_LEN').join(', ')} };\n`
  h += `#define SPRITES_DATA SPRITES_DATA_0\n#define SPRITES_DATA_LEN SPRITES_DATA_0_LEN\n`  // compat: sheet 0 = boot cart
  fs.writeFileSync(path.join(stage, 'sprites_data.h'), h)
  console.log(`note: ${sheets.length} per-cart sprite sheets baked (de_sheet_select swaps them on switch)`)
} else {
  fs.writeFileSync(path.join(stage, 'sprites_data.h'), cHeader('SPRITES_DATA', mk.makeBlankSpritePng()))
}

// map: still the first cart's, staged for all (per-cart maps = same pattern, a later rung)
const withMap = carts.find(c => c.cfg.map)
const mapBytes = withMap ? mk.buildMap(withMap.cfg.map.layout || withMap.cfg.map, withMap.cfg.map.tiles, withMap.cfg.mapW, withMap.cfg.mapH) : new Uint8Array(8192)
fs.writeFileSync(path.join(stage, 'map_data.h'),
  cHeader('MAP_DATA', Buffer.from(mapBytes)))

// studio.c includes them with these exact names — keep symbol spellings identical to make-cart:
// SPRITES_DATA/SPRITES_DATA_LEN and MAP_DATA/MAP_DATA_LEN (the cHeader emits <sym>_LEN).

// ── roster header for the launcher (rung 3) ──────────────────────────────────
// One entry per rack, straight from the rack's own de:meta — adding a cart to the
// manifest auto-adds its menu entry. The launcher #includes this under -DAPP_BUNDLE.
// the in-game bitmap fonts are ASCII-only — typographic chars in de:meta prose
// (em-dashes, curly quotes) would render as '?' on the menu, so fold them here
const ascii = s => String(s).normalize('NFKD')
  .replace(/[—–]/g, '-').replace(/[‘’]/g, "'")
  .replace(/[“”]/g, '"').replace(/…/g, '...')
  .replace(/[̀-ͯ]/g, '').replace(/[^\x20-\x7e]/g, '?')
// IAP (manifest iap.products). TWO AXES, and they are read here for EVERY app, not just an
// umbrella with a launcher: a single-cart app has no racks to sell but still sells Pro.
//   CONTENT — a rack is PAID if some product (other than the "*" masterpass catch-all) lists it
//   in `unlocks`. That product's id + price go in the launcher roster.
//   FEATURE — the ONE product declaring a non-empty `features` list is Pro (ADR-0035: the paths
//   that carry audio OUT of the app). It goes in app_pro.h, which runtime/pro.h reads.
// ⚠ Until 2026-08-19 this whole block sat inside `if (launcher)`, so a single-cart app generated
// NO .storekit either — Store.swift's configuredIDs() would have found no products and no purchase
// could ever have been made in Tiny Pedalboard or Tiny Acid Jam.
const products = (app.iap && Array.isArray(app.iap.products)) ? app.iap.products : []
const productFor = slug => products.find(p => Array.isArray(p.unlocks) && !p.unlocks.includes('*') && p.unlocks.includes(slug))
const masterpass = products.find(p => Array.isArray(p.unlocks) && p.unlocks.includes('*'))   // the "unlock all" offer

if (launcher) {
  const { readMeta } = require(path.join(__dirname, 'build-cart-index.js'))
  const summaryOf = m => {
    const d = m && m.description
    return typeof d === 'object' && d ? (d.summary || '') : (d || '')
  }
  const roster = carts.map(c => {
    const meta = readMeta(fs.readFileSync(c.src, 'utf8'), c.name) || {}
    const prod = productFor(c.name)
    return { slug: c.name, title: ascii(meta.title || c.name), desc: ascii(summaryOf(meta)),
             product: prod ? prod.id : '', price: prod ? ascii(prod.price) : '' }
  })
  fs.writeFileSync(path.join(stage, 'app_roster.h'),
`// GENERATED by tools/build-app.js from ${path.relative(ROOT, manifestFile)} — do not edit.
// The launcher cart's view of the "${app.name}" app: one entry per rack, fed by each
// rack's de:meta + the manifest's iap.products. app_launch/app_current are implemented by
// the dispatcher shim; the launcher gates paid racks via the (weak) Store_* bridge.
#pragma once
#define APP_NAME ${JSON.stringify(ascii(app.name))}
#define APP_ROSTER_COUNT ${roster.length}
// the "unlock all" master pass (empty id = none): the launcher shows it as an offer row.
#define APP_MASTERPASS_ID ${JSON.stringify(masterpass ? masterpass.id : '')}
#define APP_MASTERPASS_PRICE ${JSON.stringify(masterpass ? ascii(masterpass.price) : '')}
// product/price are "" for a free rack; non-empty = paid (gate on Store_IsModuleUnlocked).
typedef struct { const char *slug; const char *title; const char *desc; const char *product; const char *price; } AppRosterEntry;
static const AppRosterEntry APP_ROSTER[APP_ROSTER_COUNT] = {
${roster.map(r => `    { ${JSON.stringify(r.slug)}, ${JSON.stringify(r.title)}, ${JSON.stringify(r.desc)}, ${JSON.stringify(r.product)}, ${JSON.stringify(r.price)} },`).join('\n')}
};
void app_launch(int i);   // switch dispatch + sound context to rack i (roster index)
int  app_current(void);   // roster index of the rack most recently active (-1 = none yet)
`)

}

// ── the PRO unlock + the StoreKit config: EVERY app, launcher or not ─────────
// ── the PRO product → a generated header every cart can see (runtime/pro.h reads it) ──────
// The FEATURE axis, next to the roster's content axis. A product is Pro when it declares a
// non-empty `features` list (ADR-0035: WAV export / MIDI / AUv3 — the paths that carry audio OUT
// of the app). `unlocks` stays for RACKS, so the two axes never collide.
const proProducts = products.filter(p => Array.isArray(p.features) && p.features.length)
if (proProducts.length > 1) {
  console.error(`${path.relative(ROOT, manifestFile)}: ${proProducts.length} products declare "features" — there is exactly ONE Pro unlock per app (ADR-0035)`)
  process.exit(1)
}
const pro = proProducts[0]
if (pro && Array.isArray(pro.unlocks) && pro.unlocks.length) {
  console.error(`${path.relative(ROOT, manifestFile)}: the Pro product ${pro.id} also lists "unlocks" — Pro gates FEATURES, racks are the other axis`)
  process.exit(1)
}
// The catch-all is matched by its LAST COMPONENT in both Store.swift and AppGroup.swift (an
// extension has no .storekit to read), so a pass named anything else silently unlocks nothing.
if (masterpass && !masterpass.id.endsWith('.masterpass')) {
  console.error(`${path.relative(ROOT, manifestFile)}: the "*" product ${masterpass.id} must end in ".masterpass" — that suffix IS the catch-all rule the entitlement readers use`)
  process.exit(1)
}
const proHeader =
`// GENERATED by tools/build-app.js from ${path.relative(ROOT, manifestFile)} — do not edit.
// The one Pro unlock for "${app.name}", read by runtime/pro.h. Absent outside a bundle (the
// editor, a bare play.js run, the web build), which pro.h treats as "no store here".
#pragma once
#define APP_PRO_ID    ${JSON.stringify(pro ? pro.id : '')}
#define APP_PRO_PRICE ${JSON.stringify(pro ? ascii(String(pro.price)) : '')}
`
fs.writeFileSync(path.join(stage, 'app_pro.h'), proHeader)

// GENERATE the StoreKit test config from the manifest (single source of truth — no
// drift between the launcher's prices and StoreKit's). Bundled + activated in DEBUG via
// an in-app SKTestSession (a scheme storeKitConfiguration only helps Xcode's Run button,
// not simctl/ios-deploy). Real App Store prices come from ASC; this is the local mirror.
if (products.length && ios) {
  const uuid = i => `00000000-0000-0000-0000-${String(i + 1).padStart(12, '0')}`
  const skProducts = products.map((p, i) => ({
    displayPrice: String(p.price), familyShareable: false, internalID: uuid(i),
    localizations: [{ description: ascii(p.desc || ''), displayName: ascii(p.name || p.id), locale: 'en_US' }],
    productID: p.id, referenceName: ascii(p.name || p.id), type: 'NonConsumable',
  }))
  const storekit = {
    identifier: 'F1A2B3C4', appPolicies: { eula: '', policies: [] },
    nonRenewingSubscriptions: [], products: skProducts, settings: { _failTransactionsEnabled: false },
    subscriptionGroups: [], version: { major: 3, minor: 0 },
  }
  fs.mkdirSync(path.join(ROOT, 'ios/gen/app'), { recursive: true })
  fs.writeFileSync(path.join(ROOT, 'ios/gen/app/Tinyjam.storekit'), JSON.stringify(storekit, null, 2) + '\n')
}

// ── --dry: everything above ran, nothing has been compiled ───────────────────
// The boundary is deliberate: everything ABOVE is manifest parsing, cart resolution, the screen/grid
// agreement, the ctx cap read out of the engine, the sprite/map bake and the launcher roster — all
// the parts that rot silently when something else in the repo moves. Everything BELOW is clang.
// What ran above wrote only into build/.app-* (and ios/gen for --ios), both gitignored scratch.
if (dry) {
  console.log(`✓ dry: "${app.name}" stages — ${carts.length} cart(s)${launcher ? ' + launcher' : ''}, `
    + `${nCtx}/${ctxCap} ctx, ${d0.screenW}x${d0.screenH}  (no compile attempted)`)
  process.exit(0)
}

// ── compile each cart TU with renamed entry points ───────────────────────────
const COMMON = [
  '-I' + path.join(ROOT, 'runtime'), '-I' + stage, '-I' + path.join(mk.RAYLIB, 'include'),
  `-DSCREEN_W=${d0.screenW}`, `-DSCREEN_H=${d0.screenH}`, `-DSCALE=${d0.scale}`,
  `-DMAP_W=${d0.mapW}`, `-DMAP_H=${d0.mapH}`, `-DCELL_W=${d0.cellW}`, `-DCELL_H=${d0.cellH}`,
  '-DTOUCH_CONTROLS_DEFAULT=0', '-DSCALE_FILTER=0', '-O2', '-fno-delete-null-pointer-checks',
  ...(appResizable ? ['-DDE_RESIZABLE'] : []),   // reflow to fill the device (all carts opted in)
]
const clang = (a) => execFileSync('clang', a, { stdio: ['ignore', 'pipe', 'pipe'] })
// ctx order: the launcher (when present) boots first at ctx 0; racks follow at 1..N
const units = launcher ? [launcher, ...carts] : carts
const objs = []
for (const c of units) {
  const staged = path.join(stage, c.slug + '.c')
  fs.copyFileSync(c.src, staged)                     // provenance: the staged copy is what compiled
  const obj = path.join(stage, c.slug + '.o')
  clang(['-c', staged, '-o', obj, ...COMMON,
    ...(c === launcher ? ['-DAPP_BUNDLE'] : []),     // launcher swaps its demo roster for app_roster.h
    `-Ddraw=${c.slug}_draw`, `-Dupdate=${c.slug}_update`,
    `-Dinit=${c.slug}_init`, `-Dspec=${c.slug}_spec`])
  const nm = execFileSync('nm', ['-gU', obj], { encoding: 'utf8' })
  c.has = {
    draw:   nm.includes(` _${c.slug}_draw`),
    update: nm.includes(` _${c.slug}_update`),
    init:   nm.includes(` _${c.slug}_init`),
  }
  if (!c.has.draw) { console.error(`${c.name} defines no draw() — every cart needs one`); process.exit(1) }
  objs.push(obj)
}

// ── generate the dispatcher shim ─────────────────────────────────────────────
const N = units.length
const decls = units.map(c => {
  let s = `void ${c.slug}_draw(void);`
  s += c.has.update ? ` void ${c.slug}_update(void);` : ` static void ${c.slug}_update(void) {}`
  s += c.has.init   ? ` void ${c.slug}_init(void);`   : ` static void ${c.slug}_init(void) {}`
  return s
}).join('\n')
const tabLine = launcher
  ? `if (keyp(KEY_TAB)) activate(active == 0 ? last_rack : 0);   // rack <-> overview (share-panel.md rung 3a)`
  : `if (keyp(KEY_TAB)) activate((active + 1) % ${N});`
const launcherSeam = launcher ? `
// launcher seam — the menu cart (ctx 0) calls these via the generated app_roster.h
void app_launch(int i) {          // i = roster index (rack order in the manifest)
    if (i < 0 || i >= ${carts.length}) return;
    activate(1 + i);
}
int app_current(void) {
    int r = active ? active : last_rack;
    return r - 1;                 // roster index; -1 = no rack opened yet
}
` : ''
// home chip (bundle-with-launcher only): HOLD the top-left corner ~0.3s to return to the
// overview — TAB doesn't exist on a phone, and a small tap target is hard to hit + fights
// the rack's own corner UI. So the HIT REGION is big (a fat-finger pad) but a *hold* is
// required, so quick taps on the rack's controls never trigger it. The little chip fills
// up as you hold (feedback). Drawn by the shim AFTER the cart, so a standalone cart never
// shows it. Cross-input: a held finger OR a held mouse button. GESTURE OWNERSHIP: the hold only
// counts a contact that BEGAN inside the pad (started-in, not present-now), so a finger dragged in
// from the rack — or a knob-drag wandering through the corner — never trips it (see touch-notes.md).
const homeChip = launcher ? `
#define HOME_HIT_W 48        // hold-pad: generous, easy to land a finger in
#define HOME_HIT_H 20        // kept in the top toolbar band (above the rack's knobs)
#define HOME_HOLD  18        // frames (~0.3s @ 60fps) to confirm — quick taps won't
#define HOME_W 20
#define HOME_H 14
static int home_hold  = 0;   // frames the OWNING contact has been held in the pad (0 = not holding)
static int home_owner = -1;  // touch id whose gesture BEGAN inside the pad (-1 = none)
static int home_prev[16], home_prevn = 0;   // touch ids seen last frame (to detect a fresh landing)
// The pad sits inside the device safe area (top-left, below the notch) via safe_rect() — its origin
// is 0,0 on a fixed cart / desktop, so this is unchanged there, but on a resizable rack that fills
// the screen the pad clears the status bar / notch and stays tappable (was stuck under it).
static int home_in(int x, int y) {               // is (x,y) inside the hold-pad?
    int sx = 0, sy = 0; safe_rect(&sx, &sy, 0, 0);
    return x >= sx && x < sx + HOME_HIT_W && y >= sy && y < sy + HOME_HIT_H;
}
static int home_seen(int id) { for (int i = 0; i < home_prevn; i++) if (home_prev[i] == id) return 1; return 0; }
// Advance the hold; return 1 the frame it completes. GESTURE OWNERSHIP (see touch-notes.md): only a
// contact that BEGAN inside the pad drives it — a finger dragged in from the rack, or a knob-drag
// wandering through the corner, never triggers home. Started-in, not present-now.
static int home_step(void) {
    if (home_owner < 0)                          // claim a finger that JUST LANDED inside the pad
        for (int i = 0; i < touch_count(); i++) {
            int id = touch_id(i);
            if (!home_seen(id) && home_in(touch_x(i), touch_y(i))) { home_owner = id; break; }
        }
    static int mouse_owner = 0;                  // desktop/editor: a press that BEGAN inside the pad
    if (mouse_pressed(MOUSE_LEFT) && home_in(mouse_x(), mouse_y())) mouse_owner = 1;
    if (!mouse_down(MOUSE_LEFT)) mouse_owner = 0;

    int holding = 0;
    if (home_owner >= 0) {                        // owner still down AND still inside?
        int fx = -1, fy = -1;
        for (int i = 0; i < touch_count(); i++) if (touch_id(i) == home_owner) { fx = touch_x(i); fy = touch_y(i); break; }
        if (fx >= 0 && home_in(fx, fy)) holding = 1;
        else home_owner = -1;                    // owner lifted or slid off → cancel the hold
    }
    if (mouse_owner && home_in(mouse_x(), mouse_y())) holding = 1;

    home_prevn = touch_count() > 16 ? 16 : touch_count();   // remember this frame's ids
    for (int i = 0; i < home_prevn; i++) home_prev[i] = touch_id(i);

    if (holding) { if (++home_hold >= HOME_HOLD) { home_hold = 0; return 1; } }
    else home_hold = 0;
    return 0;
}
static void draw_home_chip(void) {
    int sx = 0, sy = 0; safe_rect(&sx, &sy, 0, 0);
    int hx = sx + 2, hy = sy + 2;                     // top-left of the pad, inside the safe area
    camera(0, 0);                                     // screen space, even if the cart left a
    clip(0, 0, screen_w(), screen_h());               // camera/clip set at the end of its draw
    rectfill(hx, hy, HOME_W, HOME_H, CLR_BLACK);
    if (home_hold > 0) {                              // fill left→right as you hold (progress)
        int fw = HOME_W * home_hold / HOME_HOLD; if (fw > HOME_W) fw = HOME_W;
        rectfill(hx, hy, fw, HOME_H, CLR_INDIGO);
    }
    rect(hx, hy, HOME_W, HOME_H, home_hold > 0 ? CLR_WHITE : CLR_LIGHT_GREY);
    for (int i = 0; i < 3; i++)                        // hamburger glyph = "back to the menu"
        line(hx + 5, hy + 4 + i * 3, hx + HOME_W - 5, hy + 4 + i * 3, CLR_LIGHT_GREY);
}
` : ''
const shim = `// GENERATED by tools/build-app.js from ${path.relative(ROOT, manifestFile)} — do not edit.
// Dispatcher shim for the "${app.name}" app: owns the real init/update/draw and forwards
// to the active cart. Each switch = de_switch_cart(ctx) — the per-cart sound context
// (ADR-0027): instruments, bus FX, wave tables, bpm all restore per cart.
${launcher
  ? `// The launcher cart "${launcher.name}" boots first (ctx 0); its app_launch() opens a rack,\n// TAB toggles rack <-> overview.`
  : `// TAB cycles carts (add a "launcher" cart to the manifest for a real menu — rung 3).`}
// DE_BUNDLE_AUTOSWITCH=<N> cycles every N frames — the deterministic headless proof hook.
#include "studio.h"
#include <stdlib.h>

${decls}

typedef struct { void (*init)(void); void (*update)(void); void (*draw)(void); } AppCart;
static const AppCart carts[${N}] = {
${units.map((c, i) => `    { ${c.slug}_init, ${c.slug}_update, ${c.slug}_draw },   // ctx ${i}: ${c.name}${c === launcher ? ' (launcher)' : ''}`).join('\n')}
};
static int active = 0;
static int booted[${N}] = { 0 };
static int autoswitch = 0;
${launcher ? '\nstatic int last_rack = 0;         // ctx of the rack most recently active (0 = none yet)' : ''}

static void activate(int i) {
    if (i == active) return;
    de_switch_cart(i);            // engine swaps the whole sound world (panic + restore included)
    active = i;${launcher ? '\n    if (i > 0) last_rack = i;     // remember where TAB-from-overview goes back to' : ''}
    if (!booted[i]) { booted[i] = 1; carts[i].init(); }
}
${launcherSeam}${homeChip}
void init(void) {
    const char *a = getenv("DE_BUNDLE_AUTOSWITCH");
    if (a) autoswitch = atoi(a);
    booted[0] = 1;
    carts[0].init();
}

void update(void) {
    ${tabLine}${launcher ? '\n    if (active != 0 && home_step()) { activate(0); return; }   // HOLD the top-left pad (gesture must START inside it) → overview' : ''}
    if (autoswitch && frame() > 0 && frame() % autoswitch == 0)
        activate((active + 1) % ${N});
    carts[active].update();
}

void draw(void) {
    carts[active].draw();${launcher ? '\n    if (active != 0) draw_home_chip();   // overlay AFTER the cart, so it sits on top' : ''}
}
`
const shimFile = path.join(stage, 'app_main.c')
fs.writeFileSync(shimFile, shim)

// ── iOS staging (Spike A) ──────────────────────────────────────────────────────
// Emit the multi-cart sources for the iOS Xcode target instead of a Raylib link.
// Xcode compiles a whole directory with ONE preprocessor-defines set, so the per-TU
// rename trick (-Ddraw=<slug>_draw) can't be a project flag — each cart gets a WRAPPER
// .c that #defines the renames then inlines the source (same effect, no per-file build
// settings). studio.c (compiled by the same target under DE_NO_RAYLIB) calls the shim's
// init/update/draw → active cart. project.yml points the app target at gen/app (dir), so
// every .c here compiles; device.sh/build.sh APP=<name> drive it. Entry-point detection +
// the shim already ran above (native nm), so this just writes files.
if (ios) {
  const dir = path.join(ROOT, 'ios/gen/app')
  fs.mkdirSync(dir, { recursive: true })
  for (const f of fs.readdirSync(dir)) if (f.endsWith('.c')) fs.unlinkSync(path.join(dir, f))  // drop stale single-cart cart.c / old wrappers
  for (const c of units) {
    const defs = [`#define draw ${c.slug}_draw`, `#define update ${c.slug}_update`,
                  `#define init ${c.slug}_init`, `#define spec ${c.slug}_spec`]
    if (c === launcher) defs.unshift('#define APP_BUNDLE 1')   // launcher swaps its demo roster for app_roster.h
    fs.writeFileSync(path.join(dir, c.slug + '.c'),
`// GENERATED by tools/build-app.js --ios — wrapper for cart "${c.name}": rename its entry
// points (so N carts link side by side) then inline the source. Do not edit.
${defs.join('\n')}
#line 1 "${path.relative(ROOT, c.src)}"
${fs.readFileSync(c.src, 'utf8')}`)
  }
  fs.copyFileSync(shimFile, path.join(dir, 'app_main.c'))
  if (launcher) fs.copyFileSync(path.join(stage, 'app_roster.h'), path.join(dir, 'app_roster.h'))
  fs.copyFileSync(path.join(stage, 'app_pro.h'), path.join(dir, 'app_pro.h'))
  // ⚠ The AUv3 extension is staged by ios/testflight.sh + device.sh (play.js → gen/au), NOT by this
  // tool, and its HEADER_SEARCH_PATHS is gen/au — so without this copy pro.h finds no app_pro.h in
  // the PLUG-IN, reads "no store here", and hands Pro to every host for free. Both stagers run
  // AFTER this and clear only *.c, so the header survives. See runtime/pro.h's fail-open warning.
  if (app.auCart) {
    const auDir = path.join(ROOT, 'ios/gen/au')
    fs.mkdirSync(auDir, { recursive: true })
    fs.copyFileSync(path.join(stage, 'app_pro.h'), path.join(auDir, 'app_pro.h'))
  }
  fs.copyFileSync(path.join(stage, 'sprites_data.h'), path.join(dir, 'sprites_data.h'))
  fs.copyFileSync(path.join(stage, 'map_data.h'), path.join(dir, 'map_data.h'))
  // ── does this app need the MICROPHONE? DERIVED, never declared ──────────────────────────────
  // A manifest key alone would be wrong in BOTH directions, and one of them is a crash:
  //   · declare it and never use it → the app ships a purpose string for a capability it does not
  //     have (Tiny Acid Jam shipped pedalboard's guitar wording; harmless but false, and purpose
  //     strings are on Apple's own review-tips list), and
  //   · USE it without declaring → iOS TERMINATES the app the moment it asks for permission. Not a
  //     warning, not a denied prompt: an instant kill, in front of a reviewer.
  // So the FACT is read from the carts (mic.h: nothing captures until a cart calls mic_start) and
  // only the WORDING comes from the manifest. That makes the crash unreachable — an app containing
  // a mic cart cannot be built without a string — and the false claim impossible, because an app
  // with no mic cart emits no key at all.
  const micCarts = units.filter(c => /\bmic_start\s*\(/.test(fs.readFileSync(c.src, 'utf8'))).map(c => c.name)
  if (micCarts.length && !app.micUsage) {
    console.error(`✗ ${micCarts.join(', ')} call mic_start(), so this app needs a microphone purpose string.`)
    console.error(`  Add to apps/${target}/app.json:`)
    console.error(`    "micUsage": "<why this app listens, and what it does with the audio>"`)
    console.error(`  ⚠ Not optional: iOS kills an app that requests the mic without one.`)
    process.exit(1)
  }
  if (micCarts.length) console.log(`✓ microphone: declared (${micCarts.join(', ')})`)
  // single-quoted for the shell that sources this; ' inside the string is escaped the POSIX way
  const shq = (s) => `'${String(s).replace(/'/g, `'\\''`)}'`
  fs.writeFileSync(path.join(ROOT, 'ios/gen/app.dims'),   // outside gen/app so it's not a compiled source
    `DE_SCREEN_W=${d0.screenW}\nDE_SCREEN_H=${d0.screenH}\nDE_MAP_W=${d0.mapW}\nDE_MAP_H=${d0.mapH}\nDE_CELL_W=${d0.cellW}\nDE_CELL_H=${d0.cellH}\n`
    + (appResizable ? 'DE_RESIZABLE=1\n' : '')                    // ios/app-flags.sh → -DDE_RESIZABLE (reflow to fill)
    + (app.orientation ? `DE_ORIENT=${app.orientation}\n` : '')   // ios/app-flags.sh → INFOPLIST orientation lock
    + (micCarts.length ? `DE_MIC_USAGE=${shq(app.micUsage)}\n` : ''))  // ios/app-flags.sh → NSMicrophoneUsageDescription
  // App icon: manifest "icon" (repo-relative PNG — the STORE format: 1024x1024, no alpha) →
  // the single-size asset catalog at ios/gen/Assets.xcassets. project.yml lists that path as an
  // AppIcon source. The catalog (with a DEFAULT icon = ios/default-icon.png) is COMMITTED, so a
  // manual Xcode build — which runs no staging script — still gets an icon. Here we OVERWRITE
  // icon-1024.png: the manifest "icon" if present, else the default (resetting any stale custom
  // icon a previous app build left). We never rm the catalog — that would delete the tracked
  // default. Contents.json is written in the same compact form build.sh/device.sh use (no churn).
  const setDir = path.join(ROOT, 'ios/gen/Assets.xcassets/AppIcon.appiconset')
  fs.mkdirSync(setDir, { recursive: true })
  fs.writeFileSync(path.join(ROOT, 'ios/gen/Assets.xcassets/Contents.json'),
    '{ "info": { "author": "xcode", "version": 1 } }\n')
  fs.writeFileSync(path.join(setDir, 'Contents.json'),
    '{ "images": [{ "filename": "icon-1024.png", "idiom": "universal", "platform": "ios", "size": "1024x1024" }], "info": { "author": "xcode", "version": 1 } }\n')
  let iconSrc = path.join(ROOT, 'ios/default-icon.png')
  if (app.icon) {
    iconSrc = path.join(ROOT, app.icon)
    if (!fs.existsSync(iconSrc)) { console.error(`✗ manifest "icon" not found: ${app.icon}`); process.exit(1) }
  }
  fs.copyFileSync(iconSrc, path.join(setDir, 'icon-1024.png'))
  console.log(`✓ staged app icon ${app.icon || 'ios/default-icon.png (default)'} → ios/gen/Assets.xcassets`)
  // iOS masks that square PNG to a squircle and throws ~6% away. Warn HERE (the last moment the
  // icon is still cheap to change) if the mask is eating real detail rather than flat background.
  // Advisory only — the build is not the place to refuse over taste. docs/design/app-icon-mask.md
  try {
    const r = spawnSync('node', [path.join(ROOT, 'tools/icon-mask.js'), 'check', iconSrc, '--no-proof'],
      { cwd: ROOT, encoding: 'utf8' })
    // just the verdict, not the per-corner table — that's what `check` itself is for
    const lines = (r.stdout || '').split('\n')
      .filter(l => /^[⚠·✓]/.test(l.trim()) && /detail is being cut|flat background|alpha|TRANSPARENT|1024/.test(l))
    for (const l of lines) console.log(`    ${l.trim()}`)
    if (r.status !== 0 || /real detail is being cut/.test(r.stdout || '')) {
      console.log(`    → see it: node tools/icon-mask.js preview ${path.relative(ROOT, iconSrc)}`)
    }
  } catch { /* icon check is advisory; never break a build over it */ }
  console.log(`✓ staged ${units.length} cart TUs → ios/gen/app  (${d0.screenW}x${d0.screenH})`)
  units.forEach((c, i) => console.log(`    ctx ${i}: ${c.name}${c === launcher ? '  (launcher)' : ''}`))
  console.log(`  next: cd ios && APP=${target} ./build.sh   (sim)  ·  APP=${target} ./device.sh   (phone)`)
  process.exit(0)
}

const shimObj = path.join(stage, 'app_main.o')
clang(['-c', shimFile, '-o', shimObj, ...COMMON])

// ── link ─────────────────────────────────────────────────────────────────────
// Bake the app name into the window title — a double-clicked .app gets no argv, so
// without this studio.c falls back to "dreamengine". (execFileSync = no shell, so the
// C string literal's quotes go through literally; strip any that'd break the token.)
const titleSafe = String(app.name).replace(/["'\\$`]/g, '').trim()
const titleDef = titleSafe ? [`-DDE_WINDOW_TITLE="${titleSafe}"`] : []
const out = path.join(ROOT, 'build', appId)
try {
  clang([shimObj, ...objs, path.join(ROOT, 'runtime/studio.c'), ...COMMON, ...titleDef,
    path.join(mk.RAYLIB, 'lib/libraylib.a'),
    '-framework', 'OpenGL', '-framework', 'Cocoa', '-framework', 'IOKit',
    '-framework', 'CoreVideo', '-framework', 'CoreFoundation', '-framework', 'CoreMIDI',
    '-framework', 'AudioToolbox',   // mic_desktop.h AudioQueue capture (Tier-1 mic input) — matches make-cart.js / main.cjs
    '-Wl,-dead_strip', '-o', out])
} catch (e) {
  const err = (e.stderr || '').toString().split('\n')
    .filter(l => !l.includes('was built for newer macOS version')).join('\n')
  console.error(err || e.message)
  process.exit(1)
}

console.log(`✓ built build/${appId}  (${d0.screenW}x${d0.screenH} @${d0.scale}x)`)
units.forEach((c, i) => console.log(`    ctx ${i}: ${c.name}${c === launcher ? '  (launcher)' : ''}${c.has.init ? '' : '  (no init)'}${c.has.update ? '' : '  (no update)'}`))
console.log(`  ${launcher ? 'boots into the launcher; TAB toggles rack <-> overview' : 'TAB cycles carts'} · DE_BUNDLE_AUTOSWITCH=<frames> auto-cycles (headless proof)`)

if (run) {
  const { spawn } = require('child_process')
  spawn(out, [], { cwd: path.join(ROOT, 'build'), stdio: 'inherit', detached: true }).unref()
  console.log(`▶ launched build/${appId}`)
}

// ── mac .app (rung 4) ─────────────────────────────────────────────────────────
// Hand the freshly-linked binary to mac-app.sh with name+bundleId straight from the
// manifest — so "which app?" is answered once, by the manifest name, all the way through.
// (Per-app icon stays parked: mac-app.sh falls back to the shared dreamengine icon.)
if (mac) {
  const macArgs = [out, '--name', app.name, '--out', path.join(ROOT, 'build')]
  if (app.bundleId) macArgs.push('--id', app.bundleId)
  if (noNotarize)   macArgs.push('--no-notarize')
  console.log(`\n── wrapping build/${appId} → ${app.name}.app${noNotarize ? ' (no notarize)' : ''} ──`)
  try {
    execFileSync(path.join(__dirname, 'mac-app.sh'), macArgs, { stdio: 'inherit' })
  } catch (e) {
    console.error(`✗ mac-app.sh failed (binary is fine at build/${appId})`)
    process.exit(1)
  }
}
