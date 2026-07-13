#!/usr/bin/env node
// floorplanner.js — one command: a Floorplanner project id -> a playable top-down cart.
//
//   node data-tools/fmltools/floorplanner.js -pid=187256440
//   node data-tools/fmltools/floorplanner.js --pid 187256440 --name myplan --floor 0 --scale 8
//
// Fetches the project's .fml from the v2 API, caches it under data-tools/fmltools/cache/, then
// runs the existing make-floor pipeline (geometry -> bake CDN furniture sprites ->
// embed -> build -> register -> thumbnail). The result is a baked cart in exactly the
// floorwalker/seinelaan style — only the data differs per project.
//
// AUTH — three sources, first one found wins. Easiest: paste your credential ONCE into a
// gitignored file and every `<id> --play` after that just works (no re-exporting per shell):
//   0. data-tools/fmltools/.token  — paste a JWT (starts `eyJ...`) OR a cookie
//      (`fp_site_session=...`) into this one file. Refresh it when a fetch 401/403s.
//        echo 'eyJhbGciOi...' > data-tools/fmltools/.token
//   1. FP_AUTH_TOKEN env — the editor's `?auth_token=...` JWT (~2 weeks). Overrides the file.
//        export FP_AUTH_TOKEN='eyJhbGciOi...'
//   2. FP_SESSION env — session cookie (`fp_site_session`, ~12h) from devtools -> Application
//      -> Cookies -> floorplanner.com, while logged in (full pair, or just the value).
//        export FP_SESSION='fp_site_session=...'
//
// On a 401/403 the token/cookie has expired — refresh it (re-paste .token or re-export).
//
// Flags:  -pid=N | --pid N | <N>   (required, the project id)
//         --name <cart>   cart id (default: fp<pid>); reused name regenerates in place
//         --floor i  --scale cm/px  --maxfurn cm     (passed through to make-floor.sh)
//         --fetch-only    just download the .fml (skip the build) -> prints the path
//         --force         re-download even if cached
//         --play          after building, launch the floorplan cart on this project
//
// --serve (no pid): the fetch-bridge for the cart's id picker. Watches build/.fp-request for an id
//   the cart writes (Enter an unknown id in its picker), fetches+builds it, and hands the path back
//   via build/.fp-ready. Run it alongside the editor so typing an id in the cart just loads it:
//     node data-tools/fmltools/floorplanner.js --serve          # watch (pairs with the editor-run cart)
//     node data-tools/fmltools/floorplanner.js --serve --play   # also launch the cart itself

const fs = require('fs');
const path = require('path');
const https = require('https');
const { execFileSync, spawn } = require('child_process');

const ROOT = path.resolve(__dirname, '../..');  // data-tools/fmltools/ → repo root
const CACHE = path.join(__dirname, 'cache');

// ---- args ----
const argv = process.argv.slice(2);
const opt = { pid: null, name: null, floor: null, scale: null, maxfurn: null, fetchOnly: false, force: false, baked: false, play: false, serve: false };
for (let i = 0; i < argv.length; i++) {
  const a = argv[i];
  let m;
  if ((m = a.match(/^-{1,2}pid=(\d+)$/))) opt.pid = m[1];
  else if (a === '--pid' || a === '-pid') opt.pid = argv[++i];
  else if (a === '--name') opt.name = argv[++i];
  else if (a === '--floor') opt.floor = argv[++i];
  else if (a === '--scale') opt.scale = argv[++i];
  else if (a === '--maxfurn') opt.maxfurn = argv[++i];
  else if (a === '--fetch-only') opt.fetchOnly = true;
  else if (a === '--force') opt.force = true;
  else if (a === '--baked') opt.baked = true;   // old path: bake a per-project cart (make-floor.sh) instead of a runtime data file
  else if (a === '--play' || a === '--run') opt.play = true;   // after building, launch the cart on this project
  else if (a === '--serve') opt.serve = true;   // watch build/.fp-request for ids the cart writes → fetch → build/.fp-ready (the picker fetch-bridge)
  else if (/^\d+$/.test(a) && !opt.pid) opt.pid = a;
  else { console.error('floorplanner: unknown arg', a); process.exit(1); }
}
if (!opt.pid && !opt.serve) {
  console.error('usage: node data-tools/fmltools/floorplanner.js -pid=<projectid> [--name <id>] [--floor i] [--scale cm/px] [--maxfurn cm] [--baked] [--fetch-only] [--force] [--play]');
  console.error('   or: node data-tools/fmltools/floorplanner.js --serve [--play]   # fetch-bridge for the cart\'s id picker (run alongside the editor)');
  process.exit(1);
}

// ---- auth ----
// Prefer the long-lived JWT (query param); fall back to the session cookie (header).
// Sources, first hit wins: FP_AUTH_TOKEN env, FP_SESSION env, then the gitignored
// data-tools/fmltools/.token file (paste a JWT or a cookie into it once — see the header).
const TOKEN_FILE = path.join(__dirname, '.token');
// Turn a raw credential string into a cred object: a cookie if it names one, else a JWT.
function credFrom(s) {
  s = s.trim();
  if (!s) return null;
  if (/(^|[;\s])fp_[a-z_]+=/i.test(s)) return { kind: 'cookie', cookie: s };  // full cookie pair(s)
  if (s.startsWith('ey')) return { kind: 'token', token: s };                 // JWT (base64 "{...")
  return { kind: 'cookie', cookie: 'fp_site_session=' + s };                  // bare cookie value
}
function auth() {
  const tok = (process.env.FP_AUTH_TOKEN || '').trim();
  if (tok) return { kind: 'token', token: tok };
  const c = (process.env.FP_SESSION || process.env.FP_COOKIE || '').trim();
  if (c) return { ...credFrom(c), from: 'env' };
  if (fs.existsSync(TOKEN_FILE)) {
    const cred = credFrom(fs.readFileSync(TOKEN_FILE, 'utf8'));
    if (cred) { console.log(`▸ auth      ${path.relative(ROOT, TOKEN_FILE)} (${cred.kind})`); return { ...cred, from: 'file' }; }
  }
  console.error('floorplanner: no credentials. Do ONE of:');
  console.error("  echo 'eyJhbGciOi...' > data-tools/fmltools/.token   # paste a JWT (or a fp_site_session=... cookie) once — persists");
  console.error("  export FP_AUTH_TOKEN='eyJhbGciOi...'                 # JWT from an authed editor request (~2 weeks)");
  console.error("  export FP_SESSION='fp_site_session=...'              # session cookie from devtools (~12h)");
  process.exit(1);
}

// ---- fetch ----
function fetchFml(pid, cred) {
  let url = `https://floorplanner.com/api/v2/projects/${pid}/download_json.fml`;
  const headers = { 'User-Agent': 'dreamengine-fmltools' };
  if (cred.kind === 'token') url += '?auth_token=' + encodeURIComponent(cred.token);
  else headers.Cookie = cred.cookie;
  return new Promise((resolve, reject) => {
    https.get(url, { headers, timeout: 30000 }, (res) => {
      const bufs = [];
      res.on('data', (d) => bufs.push(d));
      res.on('end', () => {
        const body = Buffer.concat(bufs);
        if (res.statusCode === 403 || res.statusCode === 401) {
          const fix = cred.from === 'file' ? 're-paste data-tools/fmltools/.token' : `refresh ${cred.kind === 'token' ? '$FP_AUTH_TOKEN' : '$FP_SESSION'} and re-export`;
          return reject(new Error(`auth ${res.statusCode} — your ${cred.kind} is missing/expired. ${fix}.`));
        }
        if (res.statusCode !== 200) return reject(new Error(`HTTP ${res.statusCode} fetching project ${pid}`));
        // download_json.fml returns JSON despite the text/xml content-type
        const txt = body.toString('utf8');
        try { JSON.parse(txt); } catch { return reject(new Error(`project ${pid}: response was not FML JSON (got ${body.length}B, starts "${txt.slice(0, 40)}")`)); }
        resolve(txt);
      });
    }).on('error', reject).on('timeout', function () { this.destroy(); reject(new Error('timeout')); });
  });
}

// ---- reusable pipeline (shared by the one-shot flow and --serve) ----
// fetch the .fml into cache if missing (or force); returns its path. throws on auth/HTTP error.
async function ensureFml(pid, cred, force) {
  fs.mkdirSync(CACHE, { recursive: true });
  const fml = path.join(CACHE, `${pid}.fml`);
  if (force || !fs.existsSync(fml)) {
    process.stdout.write(`▸ fetch     project ${pid} (${cred.kind}) … `);
    const txt = await fetchFml(pid, cred);
    fs.writeFileSync(fml, txt);
    const j = JSON.parse(txt);
    const d = ((j.floors || [])[0]?.designs || [])[0] || {};
    console.log(`ok  "${j.name}"  (${(d.walls || []).length} walls, ${(d.areas || []).length} areas, ${(d.items || []).length} items)`);
  } else {
    console.log(`▸ cached    ${path.relative(ROOT, fml)}  (use --force to refetch)`);
  }
  return fml;
}
// run the dynamic pipeline (geometry + furniture sprites + floor textures) → returns the data path.
// maxfurn 0 = never drop by size (real floor coverings are surfaces[]/rs-####, not items[]); a 280
// cap dropped normal sofas/beds. --saturate 1.4 (not 2.2/posterize) keeps natural furniture tones.
function buildDynamic(pid, name, fml, floor, scale, maxfurn) {
  const dataDir = path.join(ROOT, 'data', 'floorplan');
  fs.mkdirSync(dataDir, { recursive: true });
  const dataRel = path.join('data', 'floorplan', `${name}.json`);
  const data = path.join(ROOT, dataRel);
  const assets = `build/.fml-assets-${name}`;
  const node = (script, a) => execFileSync('node', [path.join(__dirname, script), ...a], { cwd: ROOT, stdio: 'inherit' });
  console.log(`▸ geometry  -> ${dataRel}`);
  node('fml2cart.js', [fml, '--json', data, '--floor', floor, '--scale', scale, '--maxfurn', maxfurn]);
  console.log(`▸ bake art  furniture renders -> ${assets}`);
  node('fml-assets.js', [fml, '--out', assets, '--max', '24', '--saturate', '1.4']);
  console.log(`▸ sprites   -> ${dataRel}`);
  node('fml-sprites.js', ['--json', data, '--manifest', path.join(assets, 'manifest.json')]);
  console.log(`▸ textures  resolve rs-#### floor materials -> ${dataRel}`);
  node('fml-textures.js', ['--json', data, '--out', `build/.fml-textures-${name}`]);
  return data;
}

// --serve — the fetch-bridge for the floorplan cart's id picker. Watches build/.fp-request for an
// id the cart writes (Enter an unknown id in the picker), fetches+builds it, then hands the data
// path back via build/.fp-ready (or a one-line message via build/.fp-error). Run it alongside the
// editor; the cart polls these files and loads the plan when it appears. One relay serves any run.
// Design: docs/design/external-data-carts.md → "Loader".
async function serve(cred) {
  const BUILD = path.join(ROOT, 'build');
  fs.mkdirSync(BUILD, { recursive: true });
  const REQ = path.join(BUILD, '.fp-request'), READY = path.join(BUILD, '.fp-ready'), ERR = path.join(BUILD, '.fp-error');
  for (const f of [REQ, READY, ERR]) { try { fs.unlinkSync(f); } catch {} }   // clear stale signals
  if (opt.play) { console.log('▸ launch   floorplan cart…'); spawn('node', ['tools/play.js', 'floorplan', 'run'], { cwd: ROOT, stdio: 'inherit' }); }
  console.log(`▸ serve     watching build/.fp-request (auth ${cred.kind}) — type an id in the cart's picker. Ctrl-C to stop.`);
  let busy = false;
  setInterval(async () => {
    if (busy || !fs.existsSync(REQ)) return;
    let id;
    try { id = fs.readFileSync(REQ, 'utf8').trim(); fs.unlinkSync(REQ); } catch { return; }
    if (!/^\d+$/.test(id)) { fs.writeFileSync(ERR, `bad id "${id}"`); return; }
    busy = true;
    console.log(`\n▸ request   ${id}`);
    try {
      const fml = await ensureFml(id, cred, false);
      const data = buildDynamic(id, String(id), fml, '0', '8', '0');
      fs.writeFileSync(READY + '.tmp', data); fs.renameSync(READY + '.tmp', READY);   // atomic — no half-read path
      console.log(`✓ ready     ${path.relative(ROOT, data)}  — loading in the cart`);
    } catch (e) {
      const msg = ((e && e.message) ? e.message : String(e)).split('\n')[0].slice(0, 120);
      fs.writeFileSync(ERR, msg);
      console.error(`✗ ${id}: ${msg}`);
    }
    busy = false;
  }, 400);
}

(async () => {
  const cred = auth();
  if (opt.serve) return serve(cred);

  let fml;
  try { fml = await ensureFml(opt.pid, cred, opt.force); }
  catch (e) { console.error('\nfloorplanner:', e.message); process.exit(1); }
  if (opt.fetchOnly) { console.log(fml); return; }

  const name = opt.name || String(opt.pid);
  const floor = opt.floor ?? '0', scale = opt.scale ?? '8', maxfurn = opt.maxfurn ?? '0';

  if (opt.baked) {
    // OPTION A — bake a self-contained per-project cart (the make-floor.sh pipeline).
    console.log(`▸ pipeline  make-floor.sh -> tools/carts/${name}.c`);
    execFileSync(path.join(__dirname, 'make-floor.sh'), [fml, name, floor, scale, maxfurn], { cwd: ROOT, stdio: 'inherit' });
    if (opt.play) execFileSync('node', ['tools/play.js', name, 'run'], { cwd: ROOT, stdio: 'inherit' });
    return;
  }

  // OPTION B (default) — emit a runtime data file for the shared `floorplan` cart.
  const data = buildDynamic(opt.pid, name, fml, floor, scale, maxfurn);
  const dataRel = path.relative(ROOT, data);
  const sz = (fs.statSync(data).size / 1024).toFixed(0);
  // the cart runs with cwd=build/, so DE_DATA must be absolute (a relative path would miss)
  console.log(`\n✓ done.  ${dataRel} (${sz}KB)`);
  if (opt.play) {
    console.log('▸ launch   floorplan cart on this project…');
    execFileSync('node', ['tools/play.js', 'floorplan', 'run'], { cwd: ROOT, stdio: 'inherit', env: { ...process.env, DE_DATA: data } });
  } else {
    console.log(`   play it:  DE_DATA="${data}" node tools/play.js floorplan run\n   or add --play to launch it automatically, or drag ${dataRel} onto the cart's window in the editor.`);
  }
})();
