#!/usr/bin/env node
// floorplanner.js — one command: a Floorplanner project id -> a playable top-down cart.
//
//   node fmltools/floorplanner.js -pid=187256440
//   node fmltools/floorplanner.js --pid 187256440 --name myplan --floor 0 --scale 8
//
// Fetches the project's .fml from the v2 API, caches it under fmltools/cache/, then
// runs the existing make-floor pipeline (geometry -> bake CDN furniture sprites ->
// embed -> build -> register -> thumbnail). The result is a baked cart in exactly the
// floorwalker/seinelaan style — only the data differs per project.
//
// AUTH (two ways, token preferred):
//   1. auth_token (JWT, ~2 weeks) — the editor's `?auth_token=...`. Longest-lived, no
//      cookie. Grab it from any authed editor request and:
//        export FP_AUTH_TOKEN='eyJhbGciOi...'
//   2. session cookie (`fp_site_session`, ~12h) — devtools -> Application -> Cookies ->
//      floorplanner.com, while logged in:
//        export FP_SESSION='fp_site_session=...'      # full pair, or just the value
//
// On a 401/403 the token/cookie has expired — refresh it and re-export.
//
// Flags:  -pid=N | --pid N | <N>   (required, the project id)
//         --name <cart>   cart id (default: fp<pid>); reused name regenerates in place
//         --floor i  --scale cm/px  --maxfurn cm     (passed through to make-floor.sh)
//         --fetch-only    just download the .fml (skip the build) -> prints the path
//         --force         re-download even if cached

const fs = require('fs');
const path = require('path');
const https = require('https');
const { execFileSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..');
const CACHE = path.join(__dirname, 'cache');

// ---- args ----
const argv = process.argv.slice(2);
const opt = { pid: null, name: null, floor: null, scale: null, maxfurn: null, fetchOnly: false, force: false, baked: false };
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
  else if (/^\d+$/.test(a) && !opt.pid) opt.pid = a;
  else { console.error('floorplanner: unknown arg', a); process.exit(1); }
}
if (!opt.pid) {
  console.error('usage: node fmltools/floorplanner.js -pid=<projectid> [--name <id>] [--floor i] [--scale cm/px] [--maxfurn cm] [--baked] [--fetch-only] [--force]');
  process.exit(1);
}

// ---- auth ----
// Prefer the long-lived JWT (query param); fall back to the session cookie (header).
function auth() {
  const tok = (process.env.FP_AUTH_TOKEN || '').trim();
  if (tok) return { kind: 'token', token: tok };
  let c = (process.env.FP_SESSION || process.env.FP_COOKIE || '').trim();
  if (c) return { kind: 'cookie', cookie: c.includes('=') ? c : 'fp_site_session=' + c };
  console.error('floorplanner: no credentials. Set one of:');
  console.error("  export FP_AUTH_TOKEN='eyJhbGciOi...'        # JWT from an authed editor request (~2 weeks)");
  console.error("  export FP_SESSION='fp_site_session=...'     # session cookie from devtools (~12h)");
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
          return reject(new Error(`auth ${res.statusCode} — your ${cred.kind === 'token' ? 'FP_AUTH_TOKEN' : 'FP_SESSION cookie'} is missing/expired. Refresh it and re-export.`));
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

(async () => {
  const cred = auth();
  fs.mkdirSync(CACHE, { recursive: true });
  const fml = path.join(CACHE, `${opt.pid}.fml`);

  if (opt.force || !fs.existsSync(fml)) {
    process.stdout.write(`▸ fetch     project ${opt.pid} (${cred.kind}) … `);
    let txt;
    try { txt = await fetchFml(opt.pid, cred); }
    catch (e) { console.error('\nfloorplanner:', e.message); process.exit(1); }
    fs.writeFileSync(fml, txt);
    const j = JSON.parse(txt);
    const d = ((j.floors || [])[0]?.designs || [])[0] || {};
    console.log(`ok  "${j.name}"  (${(d.walls || []).length} walls, ${(d.areas || []).length} areas, ${(d.items || []).length} items)`);
  } else {
    console.log(`▸ cached    ${path.relative(ROOT, fml)}  (use --force to refetch)`);
  }

  if (opt.fetchOnly) { console.log(fml); return; }

  const name = opt.name || String(opt.pid);
  const floor = opt.floor ?? '0', scale = opt.scale ?? '8', maxfurn = opt.maxfurn ?? '280';
  const node = (script, a) => execFileSync('node', [path.join(__dirname, script), ...a], { cwd: ROOT, stdio: 'inherit' });

  if (opt.baked) {
    // OPTION A — bake a self-contained per-project cart (the make-floor.sh pipeline).
    console.log(`▸ pipeline  make-floor.sh -> tools/carts/${name}.c`);
    execFileSync(path.join(__dirname, 'make-floor.sh'), [fml, name, floor, scale, maxfurn], { cwd: ROOT, stdio: 'inherit' });
    return;
  }

  // OPTION B (default) — emit a runtime data file for the shared `floorplan` cart.
  const dataDir = path.join(ROOT, 'data', 'floorplan');
  fs.mkdirSync(dataDir, { recursive: true });
  const dataRel = path.join('data', 'floorplan', `${name}.json`);
  const data = path.join(ROOT, dataRel);
  const assets = `build/.fml-assets-${name}`;

  console.log(`▸ geometry  -> ${dataRel}`);
  node('fml2cart.js', [fml, '--json', data, '--floor', floor, '--scale', scale, '--maxfurn', maxfurn]);
  console.log(`▸ bake art  furniture renders -> ${assets}`);
  node('fml-assets.js', [fml, '--out', assets, '--max', '24', '--saturate', '2.2', '--posterize', '5']);
  console.log(`▸ sprites   -> ${dataRel}`);
  node('fml-sprites.js', ['--json', data, '--manifest', path.join(assets, 'manifest.json')]);

  const sz = (fs.statSync(data).size / 1024).toFixed(0);
  // the cart runs with cwd=build/, so DE_DATA must be absolute (a relative path would miss)
  console.log(`\n✓ done.  ${dataRel} (${sz}KB)\n   play it:  DE_DATA="${data}" node tools/play.js floorplan run\n   or drag ${dataRel} onto the floorplan cart's window in the editor.`);
})();
