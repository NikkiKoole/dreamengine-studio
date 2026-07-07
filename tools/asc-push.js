#!/usr/bin/env node
// asc-push.js — push the NON-cart product surface (listing copy + screenshots) to App Store
// Connect, straight from the app manifest. The in-house upload tool ADR-0026 chose over Fastlane:
// zero deps (Node 22's built-in fetch + ES256 crypto), talks to the ASC REST API directly, and —
// unlike Fastlane — it GETs the live listing and DIFFS it against the repo (--dry-run), because
// here the copy and screenshots are derivable artifacts of the cart, not opaque files.
//
//   node tools/asc-push.js <app> --check                 # OFFLINE self-test (no key/network needed)
//   node tools/asc-push.js <app> --metadata --dry-run    # GET live listing, diff vs manifest, show plan
//   node tools/asc-push.js <app> --metadata              # PATCH title/subtitle/keywords/… live
//   node tools/asc-push.js <app> --screenshots           # upload apps/<app>/screenshots/*.png
//   node tools/asc-push.js <app> --metadata --screenshots
//
// SOURCE OF TRUTH: apps/<app>/app.json  -> listing.{title,subtitle,keywords}  (the copy you edit).
//   Optional Fastlane-style overrides:  apps/<app>/metadata/<locale>/{description,promotional_text,
//   keywords,subtitle,name,whats_new,support_url,marketing_url}.txt  (a file wins over the manifest).
//   Screenshots: apps/<app>/screenshots/<cart>-<device>.png  (device = iphone69/iphone65/ipad13/…,
//   same keys tools/store-shots.js emits). Grouped by ASC display type, ordered by filename.
//
// AUTH (ASC API key — one-time, ~2 min): App Store Connect -> Users and Access -> Integrations ->
//   App Store Connect API -> generate a key with "App Manager" access. Download the .p8 ONCE, note
//   the Key ID + the Issuer ID (top of that page). Then EITHER drop a config file (persists across
//   sessions, lives OUTSIDE the repo so it never leaks to git):
//     mkdir -p ~/.appstoreconnect/private_keys
//     mv ~/Downloads/AuthKey_<KEYID>.p8 ~/.appstoreconnect/private_keys/
//     echo '{"keyId":"<KEYID>","issuerId":"<ISSUER-UUID>"}' > ~/.appstoreconnect/config.json
//     chmod 600 ~/.appstoreconnect/config.json ~/.appstoreconnect/private_keys/*.p8
//   …OR set env vars per run: ASC_KEY_ID / ASC_ISSUER_ID / ASC_KEY_PATH (env wins over the config).
//   Resolution order: env -> ~/.appstoreconnect/config.json -> the single AuthKey_*.p8 in the dir
//   (keyId derived from its filename). Same key shape testflight.sh's AUTH_KEY/AUTH_ISSUER uses.
//   NEVER commit the .p8 or the IDs — the repo .gitignore blocks *.p8 as a backstop.
//
// The maker reviews the diff before pushing (share-panel rule 2): --dry-run first, always.

const fs = require('fs')
const path = require('path')
const os = require('os')
const crypto = require('crypto')

const ROOT = path.resolve(__dirname, '..')
const BASE = 'https://api.appstoreconnect.apple.com'

// ── App Store Connect char limits (the deterministic half; the agent owns the taste) ──────────
const LIMITS = { name: 30, subtitle: 30, keywords: 100, promotionalText: 170, description: 4000, whatsNew: 4000, copyright: 200 }
// per-product IAP localization limits (shown to buyers)
const IAP_LIMITS = { name: 30, description: 45 }
const IAP_TYPES = new Set(['CONSUMABLE', 'NON_CONSUMABLE', 'NON_RENEWING_SUBSCRIPTION'])
const PRICE_TERRITORY = 'USA' // base territory; Apple equalizes the rest from this price point

// store-shots.js device key -> ASC screenshotDisplayType. 6.9"/6.7" share the 1290x2796 slot.
const DISPLAY_TYPE = {
  iphone69: 'APP_IPHONE_67', iphone67: 'APP_IPHONE_67',
  iphone65: 'APP_IPHONE_65', iphone61: 'APP_IPHONE_61',
  ipad13: 'APP_IPAD_PRO_3GEN_129', ipad129: 'APP_IPAD_PRO_3GEN_129',
  ipad11: 'APP_IPAD_PRO_3GEN_11',
}

// Three ASC homes: appInfoLocalization (name/subtitle), appStoreVersionLocalization (the localized
// copy), and the appStoreVersion resource itself (copyright — NOT localized).
const APP_INFO_FIELDS = new Set(['name', 'subtitle'])
const VERSION_FIELDS = ['description', 'keywords', 'promotionalText', 'whatsNew', 'supportUrl', 'marketingUrl']
const VERSION_ATTR_FIELDS = new Set(['copyright'])
// manifest listing.<key> → ASC field. listing.title is the App Store *name*; the rest match 1:1.
const LISTING_TO_FIELD = {
  title: 'name', subtitle: 'subtitle', keywords: 'keywords', description: 'description',
  promotionalText: 'promotionalText', whatsNew: 'whatsNew', supportUrl: 'supportUrl',
  marketingUrl: 'marketingUrl', copyright: 'copyright',
}
// Fastlane-style filename -> our field name
const FILE_TO_FIELD = {
  name: 'name', subtitle: 'subtitle', keywords: 'keywords', description: 'description',
  promotional_text: 'promotionalText', whats_new: 'whatsNew', support_url: 'supportUrl', marketing_url: 'marketingUrl',
}

// ── args ──────────────────────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2)
const opt = { app: '', metadata: false, screenshots: false, iap: false, dryRun: false, check: false, locale: 'en-US', version: '' }
for (let i = 0; i < argv.length; i++) {
  const a = argv[i]
  if (a === '--metadata') opt.metadata = true
  else if (a === '--screenshots') opt.screenshots = true
  else if (a === '--iap') opt.iap = true
  else if (a === '--dry-run') opt.dryRun = true
  else if (a === '--check') opt.check = true
  else if (a === '--locale') opt.locale = argv[++i]
  else if (a === '--version') opt.version = argv[++i]
  else if (!a.startsWith('-') && !opt.app) opt.app = a
  else { console.error(`unknown arg: ${a}`); process.exit(2) }
}
if (!opt.app) {
  console.error('usage: node tools/asc-push.js <app> [--metadata] [--screenshots] [--iap] [--dry-run] [--check] [--locale en-US]')
  process.exit(2)
}
if (!opt.check && !opt.metadata && !opt.screenshots && !opt.iap) opt.metadata = true // default action

// ── manifest + copy assembly ────────────────────────────────────────────────────────────────
const appDir = path.join(ROOT, 'apps', opt.app)
const manifestPath = path.join(appDir, 'app.json')
if (!fs.existsSync(manifestPath)) die(`no manifest at ${rel(manifestPath)}`)
const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'))
const listing = manifest.listing || {}

// Build the desired field set: manifest listing first, then metadata/<locale>/*.txt overrides.
function collectFields() {
  const f = {}
  for (const [k, field] of Object.entries(LISTING_TO_FIELD)) {
    if (listing[k] != null) f[field] = String(listing[k])
  }
  const mdDir = path.join(appDir, 'metadata', opt.locale)
  if (fs.existsSync(mdDir)) {
    for (const file of fs.readdirSync(mdDir)) {
      const m = file.match(/^(.+)\.txt$/)
      if (!m) continue
      const field = FILE_TO_FIELD[m[1]]
      if (field) f[field] = fs.readFileSync(path.join(mdDir, file), 'utf8').replace(/\s+$/, '')
    }
  }
  return f
}

// Group screenshot files by ASC display type. Filename: <cart>-<device>.png
function collectScreenshots() {
  const dir = path.join(appDir, 'screenshots')
  const groups = {} // displayType -> [{file, device, cart}]
  if (!fs.existsSync(dir)) return groups
  for (const file of fs.readdirSync(dir).sort()) {
    const m = file.match(/^(.+)-([a-z0-9]+)\.png$/i)
    if (!m) continue
    const [, cart, device] = m
    const dt = DISPLAY_TYPE[device.toLowerCase()]
    if (!dt) { console.warn(`  ⚠ ${file}: unknown device "${device}" (no ASC display type) — skipping`); continue }
    ;(groups[dt] ||= []).push({ file: path.join(dir, file), device, cart })
  }
  return groups
}

// ── auth ────────────────────────────────────────────────────────────────────────────────────
const ASC_DIR = path.join(os.homedir(), '.appstoreconnect')
const KEY_DIR = path.join(ASC_DIR, 'private_keys')

// Resolve creds: env → ~/.appstoreconnect/config.json → the single AuthKey_*.p8 (keyId from filename).
// Everything lives OUTSIDE the repo, so git never sees it.
function loadAuth() {
  let cfg = {}
  const cfgPath = path.join(ASC_DIR, 'config.json')
  if (fs.existsSync(cfgPath)) { try { cfg = JSON.parse(fs.readFileSync(cfgPath, 'utf8')) } catch { /* ignore */ } }

  let keyId = process.env.ASC_KEY_ID || cfg.keyId
  const issuerId = process.env.ASC_ISSUER_ID || cfg.issuerId
  if (!keyId && fs.existsSync(KEY_DIR)) {
    const p8s = fs.readdirSync(KEY_DIR).filter(f => /^AuthKey_.+\.p8$/.test(f))
    if (p8s.length === 1) keyId = p8s[0].replace(/^AuthKey_(.+)\.p8$/, '$1')
  }
  const keyPath = process.env.ASC_KEY_PATH || (keyId && path.join(KEY_DIR, `AuthKey_${keyId}.p8`))

  const problems = []
  if (!keyId) problems.push('no key id (ASC_KEY_ID env, config.json, or a single AuthKey_*.p8)')
  if (!issuerId) problems.push('no issuer id (ASC_ISSUER_ID env or config.json)')
  if (!keyPath || !fs.existsSync(keyPath)) problems.push(`key file not found (${keyPath || 'ASC_KEY_PATH unset'})`)
  return { keyId, issuerId, keyPath, problems }
}

function mintJWT({ keyId, issuerId, keyPath }) {
  const header = { alg: 'ES256', kid: keyId, typ: 'JWT' }
  const now = Math.floor(Date.now() / 1000)
  const payload = { iss: issuerId, iat: now, exp: now + 1200, aud: 'appstoreconnect-v1' }
  const input = b64url(JSON.stringify(header)) + '.' + b64url(JSON.stringify(payload))
  const key = crypto.createPrivateKey(fs.readFileSync(keyPath, 'utf8'))
  // ES256 JWTs need the raw r||s signature (JOSE), NOT DER — dsaEncoding does that for us.
  const sig = crypto.sign('sha256', Buffer.from(input), { key, dsaEncoding: 'ieee-p1363' })
  return input + '.' + b64url(sig)
}

let TOKEN = null
async function api(method, urlPath, body, extraHeaders) {
  const headers = { Authorization: `Bearer ${TOKEN}`, ...extraHeaders }
  if (body !== undefined) headers['Content-Type'] = 'application/json'
  const res = await fetch(BASE + urlPath, { method, headers, body: body === undefined ? undefined : JSON.stringify(body) })
  const text = await res.text()
  const json = text ? safeJson(text) : null
  if (!res.ok) {
    const errs = json?.errors?.map(e => `${e.status} ${e.title}: ${e.detail}`).join('\n    ') || text.slice(0, 400)
    throw new Error(`${method} ${urlPath} → ${res.status}\n    ${errs}`)
  }
  return json
}

// GET that returns null on 404 instead of throwing — for "does this optional resource exist yet?"
async function apiOrNull(method, urlPath) {
  try { return await api(method, urlPath) }
  catch (e) { if (/→ 404/.test(e.message)) return null; throw e }
}

// ── metadata push ───────────────────────────────────────────────────────────────────────────
const EDITABLE = new Set([
  'PREPARE_FOR_SUBMISSION', 'METADATA_REJECTED', 'DEVELOPER_REJECTED', 'REJECTED', 'INVALID_BINARY',
])

async function resolveApp() {
  const r = await api('GET', `/v1/apps?filter[bundleId]=${encodeURIComponent(manifest.bundleId)}&limit=1`)
  const app = r.data?.[0]
  if (!app) die(`no app on ASC with bundleId ${manifest.bundleId} — create the app record first`)
  return app
}

async function editableVersionLoc(appId) {
  const r = await api('GET', `/v1/apps/${appId}/appStoreVersions?limit=50`)
  const versions = r.data || []
  let v = opt.version ? versions.find(x => x.attributes.versionString === opt.version) : null
  if (!v) v = versions.find(x => EDITABLE.has(x.attributes.appStoreState))
  if (!v) {
    const states = versions.map(x => `${x.attributes.versionString} (${x.attributes.appStoreState})`).join(', ')
    die(`no editable App Store version. Existing: ${states || 'none'}.\n` +
        `Create/prepare a version in ASC (or a READY_FOR_SALE app needs a new version) before pushing metadata.`)
  }
  const locs = await api('GET', `/v1/appStoreVersions/${v.id}/appStoreVersionLocalizations?limit=50`)
  const loc = locs.data.find(l => l.attributes.locale === opt.locale)
  if (!loc) die(`version ${v.attributes.versionString} has no ${opt.locale} localization`)
  return { version: v, loc }
}

async function editableInfoLoc(appId) {
  const infos = await api('GET', `/v1/apps/${appId}/appInfos?limit=10`)
  const info = infos.data.find(i => EDITABLE.has(i.attributes.appStoreState)) || infos.data[0]
  if (!info) die('no appInfo record on this app')
  const locs = await api('GET', `/v1/appInfos/${info.id}/appInfoLocalizations?limit=50`)
  const loc = locs.data.find(l => l.attributes.locale === opt.locale)
  if (!loc) die(`appInfo has no ${opt.locale} localization`)
  return loc
}

async function pushMetadata() {
  const want = collectFields()
  lintFields(want, /* fatal */ !opt.dryRun)
  const app = await resolveApp()
  console.log(`\n▸ ${manifest.bundleId} → app ${app.id} "${app.attributes.name}"`)

  const infoLoc = await editableInfoLoc(app.id)
  const { version, loc: verLoc } = await editableVersionLoc(app.id)
  console.log(`  version ${version.attributes.versionString} (${version.attributes.appStoreState}) · locale ${opt.locale}\n`)

  // split desired fields across the three resources
  const infoPatch = {}, verPatch = {}, verAttrPatch = {}
  for (const [field, val] of Object.entries(want)) {
    if (APP_INFO_FIELDS.has(field)) infoPatch[field] = val
    else if (VERSION_ATTR_FIELDS.has(field)) verAttrPatch[field] = val
    else if (VERSION_FIELDS.includes(field)) verPatch[field] = val
  }

  const changed = diffAndPrint('app name/subtitle', infoLoc.attributes, infoPatch)
    | diffAndPrint('version copy', verLoc.attributes, verPatch)
    | diffAndPrint('version attributes', version.attributes, verAttrPatch)
  if (!changed) { console.log('  ✓ already in sync — nothing to push'); return }
  if (opt.dryRun) { console.log('\n  (--dry-run: no changes sent)'); return }

  if (Object.keys(infoPatch).length) {
    await api('PATCH', `/v1/appInfoLocalizations/${infoLoc.id}`,
      { data: { type: 'appInfoLocalizations', id: infoLoc.id, attributes: infoPatch } })
    console.log('  ✓ pushed app name/subtitle')
  }
  if (Object.keys(verPatch).length) {
    await api('PATCH', `/v1/appStoreVersionLocalizations/${verLoc.id}`,
      { data: { type: 'appStoreVersionLocalizations', id: verLoc.id, attributes: verPatch } })
    console.log('  ✓ pushed version copy (keywords/description/promo/URLs)')
  }
  if (Object.keys(verAttrPatch).length) {
    await api('PATCH', `/v1/appStoreVersions/${version.id}`,
      { data: { type: 'appStoreVersions', id: version.id, attributes: verAttrPatch } })
    console.log('  ✓ pushed version attributes (copyright)')
  }
}

// ── screenshots push ──────────────────────────────────────────────────────────────────────────
async function pushScreenshots() {
  const groups = collectScreenshots()
  const total = Object.values(groups).reduce((n, g) => n + g.length, 0)
  if (!total) { console.log('  no screenshots found under apps/' + opt.app + '/screenshots/'); return }
  const app = await resolveApp()
  const { version, loc: verLoc } = await editableVersionLoc(app.id)
  console.log(`\n▸ screenshots → version ${version.attributes.versionString} · ${opt.locale} (${total} files)`)
  if (opt.dryRun) {
    for (const [dt, files] of Object.entries(groups))
      console.log(`  ${dt}: ${files.map(f => path.basename(f.file)).join(', ')}`)
    console.log('\n  (--dry-run: no upload)')
    return
  }
  for (const [dt, files] of Object.entries(groups)) {
    const set = await ensureScreenshotSet(verLoc.id, dt)
    console.log(`  ${dt} (set ${set.id})`)
    for (const f of files) await uploadScreenshot(set.id, f.file)
  }
}

async function ensureScreenshotSet(verLocId, displayType) {
  const sets = await api('GET', `/v1/appStoreVersionLocalizations/${verLocId}/appScreenshotSets?limit=50`)
  const found = sets.data.find(s => s.attributes.screenshotDisplayType === displayType)
  if (found) return found
  const r = await api('POST', '/v1/appScreenshotSets', {
    data: {
      type: 'appScreenshotSets',
      attributes: { screenshotDisplayType: displayType },
      relationships: { appStoreVersionLocalization: { data: { type: 'appStoreVersionLocalizations', id: verLocId } } },
    },
  })
  return r.data
}

async function uploadScreenshot(setId, filePath) {
  const buf = fs.readFileSync(filePath)
  const fileName = path.basename(filePath)
  const reserve = await api('POST', '/v1/appScreenshots', {
    data: {
      type: 'appScreenshots',
      attributes: { fileName, fileSize: buf.length },
      relationships: { appScreenshotSet: { data: { type: 'appScreenshotSets', id: setId } } },
    },
  })
  const id = await putAndCommit('appScreenshots', reserve.data, buf, fileName)
  const state = await pollAsset('appScreenshots', id)
  console.log(`    ${state === 'COMPLETE' ? '✓' : '✗'} ${fileName} (${state})`)
  if (state !== 'COMPLETE') throw new Error(`${fileName}: asset delivery ${state}`)
}

// The asset dance shared by every ASC upload: PUT each reserved chunk to its presigned URL, then
// commit with the md5. Returns the resource id — the caller polls for readiness (which field signals
// "done" differs per resource: screenshots use assetDeliveryState, images use imageAsset).
async function putAndCommit(type, reserved, buf, label) {
  const id = reserved.id
  for (const op of reserved.attributes.uploadOperations || []) {
    const headers = {}
    for (const h of op.requestHeaders || []) headers[h.name] = h.value
    const slice = buf.subarray(op.offset, op.offset + op.length)
    const res = await fetch(op.url, { method: op.method, headers, body: slice })
    if (!res.ok) throw new Error(`chunk PUT ${label} → ${res.status} ${await res.text()}`)
  }
  const md5 = crypto.createHash('md5').update(buf).digest('hex')
  await api('PATCH', `/v1/${type}/${id}`,
    { data: { type, id, attributes: { uploaded: true, sourceFileChecksum: md5 } } })
  return id
}

// screenshots (app + IAP review): readiness = assetDeliveryState reaches COMPLETE
async function pollAsset(type, id) {
  for (let i = 0; i < 30; i++) {
    const r = await api('GET', `/v1/${type}/${id}`)
    const st = r.data.attributes.assetDeliveryState?.state
    if (st === 'COMPLETE' || st === 'FAILED') return st
    await sleep(2000)
  }
  return 'TIMEOUT'
}

// IAP promo images: readiness = the processed imageAsset (CDN template url) appears
async function pollImage(id) {
  for (let i = 0; i < 30; i++) {
    const r = await api('GET', `/v1/inAppPurchaseImages/${id}`)
    if (r.data.attributes.imageAsset) return 'COMPLETE'
    if (r.data.attributes.errorType) return 'FAILED'
    await sleep(2000)
  }
  return 'TIMEOUT'
}

// ── in-app purchases (create → localize → price) ────────────────────────────────────────────────
// The manifest's iap.products[] is the source. Idempotent: skips products that already exist, only
// fills what's missing. Product IDs are PERMANENT once created — dry-run first, always.
function collectIAP() {
  return (manifest.iap?.products || []).map(p => ({
    productId: p.id,
    type: (p.type || 'NON_CONSUMABLE').toUpperCase().replace(/-/g, '_'),
    price: String(p.price),
    name: p.name || p.id,
    desc: p.desc || '',
    reviewNote: p.reviewNote || '',
    unlocks: p.unlocks || [],
  }))
}

// The review screenshot Apple requires per IAP: prefer a dedicated apps/<app>/iap-screenshots/<slug>.png,
// else reuse the app screenshot of the cart this product unlocks (or the first shot for an all-access pass).
function reviewShotFor(p) {
  const slug = p.productId.split('.').pop()
  const dedicated = path.join(appDir, 'iap-screenshots', slug + '.png')
  if (fs.existsSync(dedicated)) return dedicated
  const dir = path.join(appDir, 'screenshots')
  if (!fs.existsSync(dir)) return null
  const shots = fs.readdirSync(dir).filter(f => f.endsWith('.png')).sort()
  const cart = p.unlocks.find(u => u && u !== '*')
  if (cart) { const hit = shots.find(f => f.startsWith(cart + '-')); if (hit) return path.join(dir, hit) }
  return shots.length ? path.join(dir, shots[0]) : null // all-access pass → first available shot
}

async function ensureIAPAvailability(iapId, territories) {
  const existing = await apiOrNull('GET', `/v2/inAppPurchases/${iapId}/inAppPurchaseAvailability`)
  if (existing && existing.data) { console.log('    = availability set'); return }
  await api('POST', '/v1/inAppPurchaseAvailabilities', {
    data: {
      type: 'inAppPurchaseAvailabilities',
      attributes: { availableInNewTerritories: true },
      relationships: {
        inAppPurchase: { data: { type: 'inAppPurchases', id: iapId } },
        availableTerritories: { data: territories.map(id => ({ type: 'territories', id })) },
      },
    },
  })
  console.log(`    ✓ availability set (${territories.length} territories + future)`)
}

async function ensureIAPReviewShot(iapId, p) {
  const existing = await apiOrNull('GET', `/v2/inAppPurchases/${iapId}/appStoreReviewScreenshot`)
  if (existing && existing.data && existing.data.attributes.assetDeliveryState?.state === 'COMPLETE') {
    console.log('    = review screenshot present'); return
  }
  const file = reviewShotFor(p)
  if (!file) { console.log('    ⚠ no review screenshot found (add apps/' + opt.app + '/iap-screenshots/' + p.productId.split('.').pop() + '.png)'); return }
  const buf = fs.readFileSync(file)
  const reserve = await api('POST', '/v1/inAppPurchaseAppStoreReviewScreenshots', {
    data: {
      type: 'inAppPurchaseAppStoreReviewScreenshots',
      attributes: { fileName: path.basename(file), fileSize: buf.length },
      relationships: { inAppPurchaseV2: { data: { type: 'inAppPurchases', id: iapId } } },
    },
  })
  const id = await putAndCommit('inAppPurchaseAppStoreReviewScreenshots', reserve.data, buf, path.basename(file))
  const state = await pollAsset('inAppPurchaseAppStoreReviewScreenshots', id)
  console.log(`    ${state === 'COMPLETE' ? '✓' : '✗'} review screenshot ${path.basename(file)} (${state})`)
}

// The 1024x1024 promoted-IAP image (the `images` relationship). Optional: apps/<app>/iap-images/<slug>.png.
async function ensureIAPImage(iapId, p) {
  const slug = p.productId.split('.').pop()
  const file = path.join(appDir, 'iap-images', slug + '.png')
  if (!fs.existsSync(file)) return // optional — silent if absent
  const cur = await api('GET', `/v2/inAppPurchases/${iapId}/images?limit=10`)
  if ((cur.data || []).some(i => i.attributes.imageAsset)) {
    console.log('    = promo image present'); return
  }
  const buf = fs.readFileSync(file)
  const reserve = await api('POST', '/v1/inAppPurchaseImages', {
    data: {
      type: 'inAppPurchaseImages',
      attributes: { fileName: path.basename(file), fileSize: buf.length },
      // NB: images use 'inAppPurchase' — localization/review-shot use 'inAppPurchaseV2' (Apple inconsistency)
      relationships: { inAppPurchase: { data: { type: 'inAppPurchases', id: iapId } } },
    },
  })
  const id = await putAndCommit('inAppPurchaseImages', reserve.data, buf, path.basename(file))
  const state = await pollImage(id) // PREPARE_FOR_SUBMISSION is fine — it's reviewed with the app
  console.log(`    ${state === 'COMPLETE' ? '✓' : '✗'} promo image ${path.basename(file)} (${state})`)
}

async function pushIAP() {
  const products = collectIAP()
  if (!products.length) { console.log('  no iap.products in manifest'); return }
  for (const p of products) {
    if (!IAP_TYPES.has(p.type)) die(`${p.productId}: bad type "${p.type}" (want ${[...IAP_TYPES].join('/')})`)
    if (p.name.length > IAP_LIMITS.name) die(`${p.productId}: name ${p.name.length}/${IAP_LIMITS.name}`)
    if (p.desc.length > IAP_LIMITS.description) die(`${p.productId}: description ${p.desc.length}/${IAP_LIMITS.description}`)
  }
  const app = await resolveApp()
  const existing = await listIAPs(app.id)
  console.log(`\n▸ IAPs → app ${app.id} (${products.length} in manifest, ${Object.keys(existing).length} live)`)

  // territories to make each IAP available in (worldwide) — fetched once, reused per product
  let territories = []
  if (!opt.dryRun) {
    const t = await api('GET', '/v1/territories?limit=200')
    territories = (t.data || []).map(d => d.id)
  }

  for (const p of products) {
    const rec = existing[p.productId]
    console.log(`\n  ${p.productId} — "${p.name}" $${p.price} (${p.type})`)
    if (opt.dryRun) {
      const shot = reviewShotFor(p)
      console.log(rec ? `    = exists (${rec.attributes.state}) — would sync localization + price + review shot`
                      : `    + would CREATE → localize (name/desc) → price $${p.price} (base ${PRICE_TERRITORY})`)
      console.log(`      review screenshot: ${shot ? path.relative(ROOT, shot) : '⚠ none found'}`)
      console.log('      ⚠ product id is PERMANENT once created')
      continue
    }
    let iap = rec
    if (!iap) { iap = await createIAP(app.id, p); console.log(`    ✓ created (${iap.id})`) }
    else console.log(`    = exists (${iap.attributes.state})`)
    await ensureIAPLocalization(iap.id, p)
    await ensureIAPPrice(iap.id, p)
    await ensureIAPAvailability(iap.id, territories)
    await ensureIAPReviewShot(iap.id, p)
    await ensureIAPImage(iap.id, p)
  }
  if (opt.dryRun) console.log('\n  (--dry-run: nothing created)')
}

async function listIAPs(appId) {
  const r = await api('GET', `/v1/apps/${appId}/inAppPurchasesV2?limit=200`)
  const map = {}
  for (const d of r.data || []) map[d.attributes.productId] = d
  return map
}

async function createIAP(appId, p) {
  const attributes = { name: p.name.slice(0, 64), productId: p.productId, inAppPurchaseType: p.type }
  if (p.reviewNote) attributes.reviewNote = p.reviewNote
  const r = await api('POST', '/v2/inAppPurchases', {
    data: {
      type: 'inAppPurchases', attributes,
      relationships: { app: { data: { type: 'apps', id: appId } } },
    },
  })
  return r.data
}

async function ensureIAPLocalization(iapId, p) {
  const locs = await api('GET', `/v2/inAppPurchases/${iapId}/inAppPurchaseLocalizations?limit=50`)
  const found = (locs.data || []).find(l => l.attributes.locale === opt.locale)
  const attrs = { name: p.name, description: p.desc }
  if (found) {
    if (found.attributes.name === attrs.name && found.attributes.description === attrs.description) {
      console.log('    = localization in sync'); return
    }
    await api('PATCH', `/v1/inAppPurchaseLocalizations/${found.id}`,
      { data: { type: 'inAppPurchaseLocalizations', id: found.id, attributes: attrs } })
    console.log('    ✓ localization updated')
  } else {
    await api('POST', '/v1/inAppPurchaseLocalizations', {
      data: {
        type: 'inAppPurchaseLocalizations', attributes: { locale: opt.locale, ...attrs },
        relationships: { inAppPurchaseV2: { data: { type: 'inAppPurchases', id: iapId } } },
      },
    })
    console.log(`    ✓ localization created (${opt.locale})`)
  }
}

async function ensureIAPPrice(iapId, p) {
  // idempotent: a price schedule is a singleton per IAP (its id == the IAP id). If one exists, leave it.
  const existing = await apiOrNull('GET', `/v2/inAppPurchases/${iapId}/iapPriceSchedule`)
  if (existing && existing.data) { console.log('    = price already scheduled'); return }

  // find the USA price point whose customer price matches the manifest price
  const pts = await api('GET', `/v2/inAppPurchases/${iapId}/pricePoints?filter[territory]=${PRICE_TERRITORY}&limit=8000`)
  const want = Number(p.price).toFixed(2)
  const point = (pts.data || []).find(pp => Number(pp.attributes.customerPrice).toFixed(2) === want)
  if (!point) {
    const near = (pts.data || []).map(pp => pp.attributes.customerPrice)
      .map(Number).filter(n => Math.abs(n - Number(p.price)) < 1).sort((a, b) => a - b).map(n => n.toFixed(2))
    console.log(`    ✗ no $${p.price} price point in ${PRICE_TERRITORY}. Nearby: ${[...new Set(near)].join(', ') || '(none within $1)'} — set price manually or pick one of these`)
    return
  }
  // Apple inline-creation: the included entity's id MUST be a local id in '${local-id}' format,
  // and manualPrices must reference that same local id.
  const localId = '${new-price}'
  try {
    await api('POST', '/v1/inAppPurchasePriceSchedules', {
      data: {
        type: 'inAppPurchasePriceSchedules',
        relationships: {
          inAppPurchase: { data: { type: 'inAppPurchases', id: iapId } },
          baseTerritory: { data: { type: 'territories', id: PRICE_TERRITORY } },
          manualPrices: { data: [{ type: 'inAppPurchasePrices', id: localId }] },
        },
      },
      included: [{
        type: 'inAppPurchasePrices', id: localId,
        attributes: { startDate: null },
        relationships: { inAppPurchasePricePoint: { data: { type: 'inAppPurchasePricePoints', id: point.id } } },
      }],
    })
    console.log(`    ✓ price set $${p.price} (base ${PRICE_TERRITORY}, Apple equalizes other territories)`)
  } catch (e) {
    console.log(`    ⚠ price not set: ${e.message.split('\n')[0]} (create/localization still done — set price in ASC)`)
  }
}

// ── offline self-test ──────────────────────────────────────────────────────────────────────────
function selfCheck() {
  let ok = true
  console.log(`asc-push --check: ${opt.app}\n`)

  const want = collectFields()
  console.log('listing fields (from app.json + metadata/):')
  for (const [f, v] of Object.entries(want)) {
    const lim = LIMITS[f]
    const bad = lim && v.length > lim
    if (bad) ok = false
    console.log(`  ${bad ? '✗' : '✓'} ${f.padEnd(16)} ${v.length}${lim ? '/' + lim : ''}  ${JSON.stringify(v.slice(0, 48))}${v.length > 48 ? '…' : ''}`)
  }
  if (!Object.keys(want).length) { console.log('  ⚠ no listing fields — add a "listing" block to app.json'); }

  const groups = collectScreenshots()
  const total = Object.values(groups).reduce((n, g) => n + g.length, 0)
  console.log(`\nscreenshots: ${total} file(s)`)
  for (const [dt, files] of Object.entries(groups))
    console.log(`  ✓ ${dt}: ${files.map(f => path.basename(f.file)).join(', ')}`)
  if (!total) console.log('  ⚠ none under apps/' + opt.app + '/screenshots/')

  const iaps = collectIAP()
  if (iaps.length) {
    console.log(`\nin-app purchases: ${iaps.length} in manifest`)
    for (const p of iaps) {
      const badName = p.name.length > IAP_LIMITS.name, badDesc = p.desc.length > IAP_LIMITS.description
      const badType = !IAP_TYPES.has(p.type)
      if (badName || badDesc || badType) ok = false
      console.log(`  ${badName || badDesc || badType ? '✗' : '✓'} ${p.productId}  $${p.price} ${p.type}` +
        `  name ${p.name.length}/${IAP_LIMITS.name}  desc ${p.desc.length}/${IAP_LIMITS.description}`)
    }
  }

  const auth = loadAuth()
  console.log('\nauth:')
  if (auth.problems.length) {
    console.log('  ⚠ not configured yet — ' + auth.problems.join('; '))
    console.log('    (offline checks still pass; set the key to push live — see the file header)')
  } else {
    try { mintJWT(auth); console.log(`  ✓ JWT signs (key ${auth.keyId}, issuer ${auth.issuerId.slice(0, 8)}…)`) }
    catch (e) { ok = false; console.log('  ✗ JWT signing failed: ' + e.message) }
  }
  console.log(`\n${ok ? '✓ PASS' : '✗ FAIL'} (offline checks)`)
  process.exit(ok ? 0 : 1)
}

// ── helpers ─────────────────────────────────────────────────────────────────────────────────
function lintFields(fields, fatal) {
  const bad = []
  for (const [f, v] of Object.entries(fields)) if (LIMITS[f] && v.length > LIMITS[f]) bad.push(`${f} ${v.length}/${LIMITS[f]}`)
  if (bad.length) {
    const msg = 'char-limit overflow: ' + bad.join(', ')
    if (fatal) die(msg)
    else console.warn('  ⚠ ' + msg)
  }
}
function diffAndPrint(label, live, want) {
  const keys = Object.keys(want)
  if (!keys.length) return 0
  let changed = 0
  console.log(`  ${label}:`)
  for (const k of keys) {
    const cur = live[k] ?? ''
    if (cur === want[k]) { console.log(`    = ${k}: unchanged`); continue }
    changed++
    console.log(`    ~ ${k}:`)
    console.log(`        live:  ${JSON.stringify(clip(cur))}`)
    console.log(`        local: ${JSON.stringify(clip(want[k]))}`)
  }
  return changed
}
const clip = s => (s || '').length > 70 ? s.slice(0, 70) + '…' : (s || '')
const b64url = x => Buffer.from(x).toString('base64').replace(/=/g, '').replace(/\+/g, '-').replace(/\//g, '_')
const safeJson = t => { try { return JSON.parse(t) } catch { return null } }
const rel = p => path.relative(ROOT, p)
const sleep = ms => new Promise(r => setTimeout(r, ms))
function die(msg) { console.error('✗ ' + msg); process.exit(1) }

// ── main ────────────────────────────────────────────────────────────────────────────────────
;(async () => {
  if (opt.check) return selfCheck()
  const auth = loadAuth()
  if (auth.problems.length) die('auth not configured — ' + auth.problems.join('; ') + '\n  See the file header for the ~2-min ASC API-key setup, or run --check offline first.')
  TOKEN = mintJWT(auth)
  if (opt.metadata) await pushMetadata()
  if (opt.screenshots) await pushScreenshots()
  if (opt.iap) await pushIAP()
})().catch(e => { console.error('\n✗ ' + e.message); process.exit(1) })
