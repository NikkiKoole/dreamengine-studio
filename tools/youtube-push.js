#!/usr/bin/env node
// youtube-push.js — push a baked cart clip (or an app reel) to YouTube, from the repo. The
// distribution last mile of demand-generation lever #2 (a shareable video): recipe → clip →
// uploaded, with a URL back, in one command. YouTube first because it's the only short-video
// venue with a usable official upload API (TikTok/Reels stay manual) — ADR-0033.
//
// The in-house twin of asc-push.js (ADR-0026): node, zero heavy deps (Node 22 fetch + the
// built-in http/crypto), creds OUTSIDE git (~/.youtube/), --dry-run shows the plan without
// touching the network, --check is an offline gate. The one structural difference from
// asc-push is auth: YouTube needs OAuth2 user consent (not a .p8 JWT) — a one-time browser
// sign-in (--auth), then a cached refresh token drives every later run non-interactively.
//
//   node tools/youtube-push.js --check                       # OFFLINE self-test (no creds/network)
//   node tools/youtube-push.js --auth                        # ONE-TIME browser sign-in → cache refresh token
//   node tools/youtube-push.js acidcandy --recipe 01-take --dry-run   # show what WOULD upload
//   node tools/youtube-push.js acidcandy --recipe 01-take    # bake (if needed) + upload as a Short
//   node tools/youtube-push.js acidcandy --recipe 01-take --landscape # full 16:10, not a Short
//   node tools/youtube-push.js acidcandy --recipe 01-take --public    # publish public (default: unlisted)
//   node tools/youtube-push.js --reel tinyacidjam            # push an app trailer (editor/public/reels/<app>.webm)
//
// OUTPUT SHAPE — default is a SHORT (ADR-0033: a Short is just a <=60s, 9:16 upload with
//   #Shorts in title+description). Two ways to fill the vertical frame, best first:
//     1. FULL-BLEED — if a 9:16 variant already exists (editor/public/clips/<cart>/<label>--720x1280.webm,
//        baked by the editor's Promote tab "bake at 9:16" picker — a resizable cart REFLOWED to fill,
//        export-ratios.md), the tool uploads THAT. No bars. This is the authoring surface's job; the
//        tool just consumes its output.
//     2. LETTERBOX FALLBACK — no variant? The tool integer-upscales the native clip and COMPOSITES it
//        centred on a 1080x1920 canvas (never stretches, same rule as store-shots.js), leaving bars.
//        It says so, and points you at the Promote picker for a full-bleed bake.
//   A clip longer than 60s is REFUSED (trim first) rather than uploaded as a non-Short. --landscape
//   opts into the full native 16:10 clip as a normal video (no 60s limit).
//
// INPUT — a committed recipe under tools/clips/<cart>/<label>.{script,beats,rec}; the native mp4 is
//   baked on demand via make-gif.js --format mp4 (libx264/yuv420p/AAC/+faststart — the format
//   YouTube ingests cleanest). All derived mp4s land in build/.yt/ (throwaway), never editor/public/.
//   --reel <app> pushes a pre-composed app trailer from editor/public/reels/<app>.webm instead
//   (build it with tools/build-app-reel.js).
//
// METADATA — derived from cart data, not a fifth hand-typed copy of the truth (like ASC copy in
//   ADR-0026). The agent owns the taste (final title punch-up); the tool owns the packing:
//     title       cart de:meta.title            / reel: app listing.title      (+ " #Shorts" vertical)
//     description  cart de:meta.description.summary / reel: listing.subtitle    (+ a gallery link, + #Shorts)
//     tags         cart kind[]+teaches[]+genre   / reel: listing.keywords       (+ config.extraTags)
//     categoryId   music kinds -> 10 (Music), else 20 (Gaming); config.categoryId overrides
//   Own generated audio -> no Content ID / copyright risk, safe to upload at scale.
//
// AUTH (one-time, ~5 min):  a Google Cloud project with the YouTube Data API v3 enabled + an
//   OAuth consent screen (uploading to your OWN channel works under a test user, no full
//   verification). Create an OAuth client of type "Desktop app", then:
//     mkdir -p ~/.youtube && chmod 700 ~/.youtube
//     echo '{"clientId":"…apps.googleusercontent.com","clientSecret":"…"}' > ~/.youtube/config.json
//     chmod 600 ~/.youtube/config.json
//     node tools/youtube-push.js --auth        # opens a browser; caches ~/.youtube/token.json
//   Optional config.json keys: "link" (gallery/store URL appended to descriptions),
//   "categoryId", "extraTags":[…], "privacyDefault" ("unlisted"|"public"|"private").
//   Quota: default ~10,000 units/day, an upload costs ~1,600 -> ~6 uploads/day. NEVER commit
//   the config/token — they live outside the repo; treat them like the ~/.appstoreconnect/ key.
//
// The maker reviews the plan before pushing (share-panel rule 2): --dry-run first, always.

const fs = require('fs')
const path = require('path')
const os = require('os')
const http = require('http')
const crypto = require('crypto')
const { spawnSync } = require('child_process')

const ROOT = path.resolve(__dirname, '..')
const YT_DIR = path.join(os.homedir(), '.youtube')
const CONFIG_PATH = path.join(YT_DIR, 'config.json')
const TOKEN_PATH = path.join(YT_DIR, 'token.json')

const OAUTH_SCOPE = 'https://www.googleapis.com/auth/youtube.upload'
const TOKEN_URL = 'https://oauth2.googleapis.com/token'
const AUTH_URL = 'https://accounts.google.com/o/oauth2/v2/auth'
const UPLOAD_URL = 'https://www.googleapis.com/upload/youtube/v3/videos'
const LOOPBACK_PORT = 8719 // must match a redirect URI registered on the OAuth client

// YouTube snippet limits (the deterministic half; the agent owns the taste)
const LIMITS = { title: 100, description: 5000, tagsTotal: 500 }
const SHORT_MAX_SECONDS = 60
const SHORT_W = 1080, SHORT_H = 1920 // canonical 9:16 Short canvas (letterbox fallback target)
const SHORT_VARIANT = '720x1280'     // the editor's shipped FULL-BLEED 9:16 clip variant (export-ratios.md /
                                     // the Promote "bake at 9:16" picker) — preferred over letterboxing
const TMP = path.join(ROOT, 'build', '.yt') // derived mp4s (transcodes/composites) — throwaway, not editor/public
// de:meta kinds that read as "music" -> YouTube category 10 (Music), else 20 (Gaming)
const MUSIC_KINDS = new Set(['instrument', 'generative', 'sequencer', 'music', 'synth', 'drum-machine'])

// ── args ─────────────────────────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2)
const opt = { cart: '', recipe: '', reel: '', landscape: false, public: false, dryRun: false,
              check: false, auth: false, json: false, scale: null, crf: null }
for (let i = 0; i < argv.length; i++) {
  const a = argv[i]
  if (a === '--recipe') opt.recipe = argv[++i]
  else if (a === '--reel') opt.reel = argv[++i]
  else if (a === '--landscape') opt.landscape = true
  else if (a === '--public') opt.public = true
  else if (a === '--dry-run') opt.dryRun = true
  else if (a === '--check') opt.check = true
  else if (a === '--auth') opt.auth = true
  else if (a === '--json') opt.json = true
  else if (a === '--scale') opt.scale = argv[++i]
  else if (a === '--crf') opt.crf = argv[++i]
  else if (!a.startsWith('-') && !opt.cart) opt.cart = a
  else { console.error(`unknown arg: ${a}`); process.exit(2) }
}
if (!opt.check && !opt.auth && !opt.cart && !opt.reel) {
  console.error('usage: node tools/youtube-push.js <cart> --recipe <label> [--landscape] [--public] [--dry-run]')
  console.error('       node tools/youtube-push.js --reel <app> [--landscape] [--public] [--dry-run]')
  console.error('       node tools/youtube-push.js --check | --auth')
  process.exit(2)
}

// ── metadata assembly (cart de:meta OR app manifest) ───────────────────────────────────────────
// Parse the de:meta JSON block out of a cart's .c (the same block build-cart-index.js reads).
function readCartMeta(cart) {
  const cPath = path.join(ROOT, 'tools', 'carts', `${cart}.c`)
  if (!fs.existsSync(cPath)) die(`no cart source at ${rel(cPath)}`)
  const src = fs.readFileSync(cPath, 'utf8')
  const m = src.match(/\/\*\s*de:meta\s*\n([\s\S]*?)\nde:meta\s*\*\//)
  if (!m) die(`${cart}.c has no de:meta block`)
  try { return JSON.parse(m[1]) } catch (e) { die(`${cart}.c: de:meta is not valid JSON — ${e.message}`) }
}

function cleanTags(list) {
  const seen = new Set(), out = []
  for (const t of list) {
    const s = String(t || '').trim()
    if (!s || seen.has(s.toLowerCase())) continue
    seen.add(s.toLowerCase()); out.push(s)
  }
  return out
}

// Build the desired video metadata (title/description/tags/categoryId/privacy/short) from source.
function collectMeta(cfg) {
  const link = cfg.link ? `\n\n${cfg.link}` : ''
  const privacy = opt.public ? 'public' : (cfg.privacyDefault || 'unlisted')
  const short = !opt.landscape
  const shortTag = short ? ' #Shorts' : ''
  const extra = Array.isArray(cfg.extraTags) ? cfg.extraTags : []

  if (opt.reel) {
    const manifest = readManifest(opt.reel)
    const L = manifest.listing || {}
    const title = clampTitle((L.title || manifest.name || opt.reel) + shortTag)
    const summary = L.subtitle || L.title || manifest.name || opt.reel
    const description = clip5000(summary + (short ? '\n\n#Shorts' : '') + link)
    const tags = cleanTags([...(L.keywords ? L.keywords.split(',') : []), ...extra])
    return { title, description, tags, categoryId: cfg.categoryId || '20', privacy, short, source: `reel:${opt.reel}` }
  }

  const meta = readCartMeta(opt.cart)
  const summary = typeof meta.description === 'string' ? meta.description : (meta.description?.summary || meta.title || opt.cart)
  const title = clampTitle((meta.title || opt.cart) + shortTag)
  const description = clip5000(summary + (short ? '\n\n#Shorts' : '') + link)
  const kinds = Array.isArray(meta.kind) ? meta.kind : []
  const tags = cleanTags([...kinds, ...(meta.teaches || []), ...(meta.genre ? [meta.genre] : []), ...extra])
  const musicish = kinds.some(k => MUSIC_KINDS.has(k))
  return { title, description, tags, categoryId: cfg.categoryId || (musicish ? '10' : '20'), privacy, short, source: `cart:${opt.cart}` }
}
const clampTitle = t => t.length > LIMITS.title ? t.slice(0, LIMITS.title - 1).trimEnd() + '…' : t
const clip5000 = d => d.length > LIMITS.description ? d.slice(0, LIMITS.description - 1) + '…' : d

function readManifest(app) {
  const p = path.join(ROOT, 'apps', app, 'app.json')
  if (!fs.existsSync(p)) die(`no app manifest at ${rel(p)}`)
  return JSON.parse(fs.readFileSync(p, 'utf8'))
}

// ── clip resolution + baking (make-gif) + Short compositing (ffmpeg) ─────────────────────────────
// A cart clip already baked at the FULL-BLEED 9:16 shape by the editor — the Promote tab's
// "bake at 9:16" picker (export-ratios.md Stage 2): editor/public/clips/<cart>/<recipe>--720x1280.webm.
// Prefer this over letterboxing a landscape clip: a resizable cart reflows to FILL the frame.
function findShortVariant(cart, recipe) {
  const v = path.join(ROOT, 'editor', 'public', 'clips', cart, `${recipe}--${SHORT_VARIANT}.webm`)
  return fs.existsSync(v) ? v : null
}

// Resolve the LANDSCAPE / native source mp4: reel = the pre-composed reel; cart = the recipe baked
// at native res. Derived mp4s go to build/.yt/ (throwaway), leaving editor/public/ clean.
function resolveLandscape(meta) {
  fs.mkdirSync(TMP, { recursive: true })
  if (opt.reel) {
    const reel = path.join(ROOT, 'editor', 'public', 'reels', `${opt.reel}.webm`)
    if (!fs.existsSync(reel)) die(`no reel at ${rel(reel)} — build it with tools/build-app-reel.js`)
    if (opt.dryRun) return reel
    const mp4 = path.join(TMP, `reel-${opt.reel}.mp4`)
    transcodeToMp4(reel, mp4)
    return mp4
  }
  if (!opt.recipe) die('a cart upload needs --recipe <label>')
  const RECIPE_EXTS = ['script', 'beats', 'rec']
  const track = RECIPE_EXTS.map(e => path.join(ROOT, 'tools', 'clips', opt.cart, `${opt.recipe}.${e}`)).find(fs.existsSync)
  if (!track) die(`no recipe at tools/clips/${opt.cart}/${opt.recipe}.{${RECIPE_EXTS.join(',')}}`)
  if (opt.dryRun) { console.log(`  (dry-run: would bake native mp4 from ${rel(track)})`); return track }
  const out = path.join(TMP, `${opt.cart}-${opt.recipe}.mp4`)
  const args = [path.join(ROOT, 'tools', 'make-gif.js'), opt.cart, '--recipe', opt.recipe, '--format', 'mp4', '--out', out]
  if (opt.scale) args.push('--scale', opt.scale)
  if (opt.crf) args.push('--crf', opt.crf)
  console.log(`  baking native mp4: make-gif.js ${opt.cart} --recipe ${opt.recipe}`)
  const r = spawnSync('node', args, { stdio: 'inherit' })
  if (r.status !== 0) die(`make-gif failed for ${opt.cart}/${opt.recipe}`)
  return out
}

function guardShortDuration(file) {
  const dur = probeDuration(file)
  if (dur != null && dur > SHORT_MAX_SECONDS)
    die(`clip is ${dur.toFixed(1)}s — a Short must be ≤${SHORT_MAX_SECONDS}s. Trim the recipe, or pass --landscape.`)
}

function transcodeToMp4(src, dst) {
  console.log(`  transcoding ${rel(src)} → mp4`)
  ffmpeg(['-y', '-i', src, '-c:v', 'libx264', '-crf', '20', '-pix_fmt', 'yuv420p',
          '-c:a', 'aac', '-b:a', '128k', '-movflags', '+faststart', dst])
}

// Composite a landscape clip centred on the 9:16 Short canvas — integer upscale (crisp), never
// stretch. Returns the path to the vertical mp4.
function makeShort(landscapeMp4, bg) {
  const [w, h] = probeDims(landscapeMp4)
  const s = Math.max(1, Math.min(Math.floor(SHORT_W / w), Math.floor(SHORT_H / h)))
  fs.mkdirSync(TMP, { recursive: true })
  const out = path.join(TMP, path.basename(landscapeMp4).replace(/\.\w+$/, '') + '-short.mp4')
  const color = bg || '0x0d0d14'
  const vf = `scale=${w * s}:${h * s}:flags=neighbor,pad=${SHORT_W}:${SHORT_H}:(ow-iw)/2:(oh-ih)/2:color=${color}`
  console.log(`  compositing 9:16 Short (${w}x${h} ×${s} → ${SHORT_W}x${SHORT_H})`)
  ffmpeg(['-y', '-i', landscapeMp4, '-vf', vf, '-c:v', 'libx264', '-crf', '18', '-pix_fmt', 'yuv420p',
          '-c:a', 'copy', '-movflags', '+faststart', out])
  return out
}

function probeDims(file) {
  const r = spawnSync('ffprobe', ['-v', 'error', '-select_streams', 'v:0', '-show_entries',
    'stream=width,height', '-of', 'csv=p=0', file], { encoding: 'utf8' })
  if (r.status !== 0) die(`ffprobe failed on ${rel(file)}`)
  const [w, h] = r.stdout.trim().split(',').map(Number)
  return [w, h]
}
function probeDuration(file) {
  const r = spawnSync('ffprobe', ['-v', 'error', '-show_entries', 'format=duration',
    '-of', 'default=nw=1:nk=1', file], { encoding: 'utf8' })
  if (r.status !== 0) return null
  return parseFloat(r.stdout.trim())
}
function ffmpeg(args) {
  const r = spawnSync('ffmpeg', args, { stdio: ['ignore', 'ignore', 'inherit'] })
  if (r.status !== 0) die('ffmpeg failed')
}

// ── auth (OAuth2: refresh-token grant; --auth for the one-time loopback consent) ────────────────
function loadConfig() {
  if (!fs.existsSync(CONFIG_PATH)) return { problems: [`no ${rel2home(CONFIG_PATH)} (clientId/clientSecret)`] }
  let cfg
  try { cfg = JSON.parse(fs.readFileSync(CONFIG_PATH, 'utf8')) } catch (e) { return { problems: [`${rel2home(CONFIG_PATH)} is not valid JSON — ${e.message}`] } }
  const problems = []
  if (!cfg.clientId) problems.push('config.json missing "clientId"')
  if (!cfg.clientSecret) problems.push('config.json missing "clientSecret"')
  return { ...cfg, problems }
}
function loadToken() {
  if (!fs.existsSync(TOKEN_PATH)) return null
  try { return JSON.parse(fs.readFileSync(TOKEN_PATH, 'utf8')) } catch { return null }
}

async function accessToken(cfg) {
  const tok = loadToken()
  if (!tok?.refresh_token) die(`not signed in — run: node tools/youtube-push.js --auth`)
  const body = new URLSearchParams({ client_id: cfg.clientId, client_secret: cfg.clientSecret,
    refresh_token: tok.refresh_token, grant_type: 'refresh_token' })
  const res = await fetch(TOKEN_URL, { method: 'POST', body })
  const json = await res.json().catch(() => null)
  if (!res.ok) die(`token refresh failed (${res.status}): ${json?.error_description || json?.error || 'unknown'}`)
  return json.access_token
}

// One-time interactive consent via a localhost loopback redirect (Google deprecated OOB). Opens
// the browser, catches ?code=…, exchanges it, and caches the refresh token in ~/.youtube/token.json.
async function interactiveAuth() {
  const cfg = loadConfig()
  if (cfg.problems.length) die('auth config not ready — ' + cfg.problems.join('; ') + `\n  See the file header (${rel2home(CONFIG_PATH)}).`)
  const redirectUri = `http://127.0.0.1:${LOOPBACK_PORT}`
  const state = crypto.randomBytes(16).toString('hex')
  const url = `${AUTH_URL}?` + new URLSearchParams({ client_id: cfg.clientId, redirect_uri: redirectUri,
    response_type: 'code', scope: OAUTH_SCOPE, access_type: 'offline', prompt: 'consent', state }).toString()

  const code = await new Promise((resolve, reject) => {
    const server = http.createServer((req, res) => {
      const q = new URL(req.url, redirectUri).searchParams
      res.writeHead(200, { 'Content-Type': 'text/html' })
      if (q.get('state') !== state) { res.end('<h2>State mismatch — try again.</h2>'); server.close(); return reject(new Error('OAuth state mismatch')) }
      if (q.get('error')) { res.end(`<h2>Denied: ${q.get('error')}</h2>`); server.close(); return reject(new Error('consent denied: ' + q.get('error'))) }
      res.end('<h2>✓ dreamengine is signed in to YouTube. You can close this tab.</h2>')
      server.close(); resolve(q.get('code'))
    })
    server.listen(LOOPBACK_PORT, '127.0.0.1', () => {
      console.log(`\nOpen this URL to authorise (opening your browser)…\n\n  ${url}\n`)
      spawnSync('open', [url]) // macOS; on other platforms paste the URL above
    })
  })

  const body = new URLSearchParams({ client_id: cfg.clientId, client_secret: cfg.clientSecret,
    code, grant_type: 'authorization_code', redirect_uri: redirectUri })
  const res = await fetch(TOKEN_URL, { method: 'POST', body })
  const json = await res.json().catch(() => null)
  if (!res.ok || !json?.refresh_token) die(`code exchange failed (${res.status}): ${json?.error_description || 'no refresh_token returned (revoke prior access + retry with prompt=consent)'}`)
  fs.mkdirSync(YT_DIR, { recursive: true })
  fs.writeFileSync(TOKEN_PATH, JSON.stringify({ refresh_token: json.refresh_token }, null, 2))
  fs.chmodSync(TOKEN_PATH, 0o600)
  console.log(`✓ refresh token cached in ${rel2home(TOKEN_PATH)} — future runs are non-interactive.`)
}

// ── resumable upload ───────────────────────────────────────────────────────────────────────────
async function uploadVideo(token, file, meta) {
  const buf = fs.readFileSync(file)
  const snippet = { title: meta.title, description: meta.description, tags: meta.tags, categoryId: meta.categoryId }
  const initBody = JSON.stringify({ snippet, status: { privacyStatus: meta.privacy, selfDeclaredMadeForKids: false } })
  const init = await fetch(`${UPLOAD_URL}?uploadType=resumable&part=snippet,status`, {
    method: 'POST',
    headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json',
      'X-Upload-Content-Type': 'video/mp4', 'X-Upload-Content-Length': String(buf.length) },
    body: initBody,
  })
  if (!init.ok) die(`upload init failed (${init.status}): ${(await init.text()).slice(0, 400)}`)
  const location = init.headers.get('location')
  if (!location) die('upload init returned no resumable Location header')

  const put = await fetch(location, { method: 'PUT',
    headers: { 'Content-Type': 'video/mp4', 'Content-Length': String(buf.length) }, body: buf })
  const json = await put.json().catch(() => null)
  if (!put.ok) die(`upload PUT failed (${put.status}): ${JSON.stringify(json)?.slice(0, 400)}`)
  return json.id
}

// ── offline self-test ────────────────────────────────────────────────────────────────────────
function selfCheck() {
  let ok = true
  const cfg = loadConfig()
  console.log(`youtube-push --check\n`)

  // ffmpeg / ffprobe present?
  console.log('tooling:')
  for (const bin of ['ffmpeg', 'ffprobe']) {
    const have = spawnSync(bin, ['-version'], { stdio: 'ignore' }).status === 0
    if (!have) ok = false
    console.log(`  ${have ? '✓' : '✗'} ${bin}`)
  }

  // metadata derivation, if a target was named
  if (opt.cart || opt.reel) {
    try {
      const meta = collectMeta(cfg.problems.length ? {} : cfg)
      console.log(`\nmetadata (${meta.source}, ${meta.short ? 'SHORT 9:16' : 'landscape'}, ${meta.privacy}):`)
      const overTitle = meta.title.length > LIMITS.title
      const tagsChars = meta.tags.join('').length
      if (overTitle || tagsChars > LIMITS.tagsTotal) ok = false
      console.log(`  ${overTitle ? '✗' : '✓'} title       ${meta.title.length}/${LIMITS.title}  ${JSON.stringify(meta.title)}`)
      console.log(`  ✓ description ${meta.description.length}/${LIMITS.description}`)
      console.log(`  ${tagsChars > LIMITS.tagsTotal ? '✗' : '✓'} tags        ${meta.tags.length} (${tagsChars}/${LIMITS.tagsTotal} chars)  ${meta.tags.join(', ')}`)
      console.log(`  ✓ categoryId  ${meta.categoryId} (${meta.categoryId === '10' ? 'Music' : 'Gaming'})`)
    } catch (e) { ok = false; console.log(`  ✗ metadata: ${e.message}`) }
  } else {
    console.log('\n(no cart/reel named — pass one to check its metadata)')
  }

  console.log('\nauth:')
  if (cfg.problems.length) {
    console.log('  ⚠ not configured yet — ' + cfg.problems.join('; '))
    console.log('    (offline checks still pass; see the file header for the ~5-min OAuth setup)')
  } else {
    const tok = loadToken()
    console.log(`  ✓ config.json (client ${cfg.clientId.slice(0, 12)}…)`)
    console.log(`  ${tok?.refresh_token ? '✓ signed in (refresh token cached)' : '⚠ no token yet — run --auth'}`)
  }
  console.log(`\n${ok ? '✓ PASS' : '✗ FAIL'} (offline checks)`)
  process.exit(ok ? 0 : 1)
}

// ── helpers ────────────────────────────────────────────────────────────────────────────────────
const rel = p => path.relative(ROOT, p)
const rel2home = p => p.replace(os.homedir(), '~')
function die(msg) { console.error('✗ ' + msg); process.exit(1) }

// ── main ───────────────────────────────────────────────────────────────────────────────────────
;(async () => {
  if (opt.check) return selfCheck()
  if (opt.auth) return interactiveAuth()

  const cfg = loadConfig()
  const meta = collectMeta(cfg.problems.length ? {} : cfg)

  // Pick the source + framing. Short: prefer the editor's FULL-BLEED 9:16 variant (a resizable cart
  // reflowed to fill the frame); fall back to letterboxing the native clip. Enforce ≤60s on Shorts.
  let shaped, framing
  const variant = meta.short && !opt.reel ? findShortVariant(opt.cart, opt.recipe) : null
  if (meta.short && variant) {
    framing = 'full-bleed 9:16 (editor variant)'
    if (opt.dryRun) shaped = variant
    else { guardShortDuration(variant); shaped = path.join(TMP, `${opt.cart}-${opt.recipe}-9x16.mp4`); fs.mkdirSync(TMP, { recursive: true }); transcodeToMp4(variant, shaped) }
  } else if (meta.short) {
    framing = opt.reel ? 'letterbox 9:16 (reel)'
      : 'letterbox 9:16 — no full-bleed variant; bake one via the Promote tab "bake at 9:16" for a fill'
    const land = resolveLandscape(meta)
    if (opt.dryRun) shaped = land
    else { guardShortDuration(land); shaped = makeShort(land, cfg.shortBg) }
  } else {
    framing = 'landscape'
    shaped = resolveLandscape(meta)
  }

  // the plan (always printed)
  console.log(`\n${opt.dryRun ? 'DRY-RUN — would upload' : 'uploading'}:`)
  console.log(`  file         ${rel(shaped)}`)
  console.log(`  shape        ${meta.short ? 'Short (9:16, #Shorts)' : 'landscape'}`)
  console.log(`  framing      ${framing}`)
  console.log(`  privacy      ${meta.privacy}`)
  console.log(`  title        ${meta.title}`)
  console.log(`  tags         ${meta.tags.join(', ')}`)
  console.log(`  category     ${meta.categoryId} (${meta.categoryId === '10' ? 'Music' : 'Gaming'})`)
  console.log(`  description  ${meta.description.replace(/\n/g, '\\n').slice(0, 120)}${meta.description.length > 120 ? '…' : ''}`)

  if (opt.json) console.log('\n' + JSON.stringify({ file: rel(shaped), ...meta }, null, 2))
  if (opt.dryRun) { console.log('\n(dry-run: nothing uploaded)'); return }

  if (cfg.problems.length) die('auth not configured — ' + cfg.problems.join('; ') + '\n  Run --check offline, or see the header for the OAuth setup.')
  const token = await accessToken(cfg)
  const id = await uploadVideo(token, shaped, meta)
  const url = meta.short ? `https://youtube.com/shorts/${id}` : `https://youtu.be/${id}`
  console.log(`\n✓ uploaded (${meta.privacy}): ${url}`)
})().catch(e => { console.error('\n✗ ' + e.message); process.exit(1) })
