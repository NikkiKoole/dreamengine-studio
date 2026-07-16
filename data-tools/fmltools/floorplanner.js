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
//
// --search (no pid): browse the TOKEN-FREE public plan feed (newest first) — the discovery half. No
//   auth needed. Prints id · floors · name · thumbnail; pick an id and fetch it with --pid (that
//   download still needs a token). --count N pages past the first 50; --json for machine output.
//     node data-tools/fmltools/floorplanner.js --search --count 100
//     node data-tools/fmltools/floorplanner.js --search --json

const fs = require('fs');
const path = require('path');
const https = require('https');
const { execFileSync, spawn } = require('child_process');

const ROOT = path.resolve(__dirname, '../..');  // data-tools/fmltools/ → repo root
const CACHE = path.join(__dirname, 'cache');

// ---- args ----
const argv = process.argv.slice(2);
const opt = { pid: null, name: null, floor: null, scale: null, maxfurn: null, fetchOnly: false, force: false, baked: false, play: false, serve: false, search: false, count: null, json: false };
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
  else if (a === '--search' || a === '--browse') opt.search = true;   // token-free: list PUBLIC plans from the search feed (no --pid)
  else if (a === '--count') opt.count = argv[++i];   // how many public plans to list (paged; default 50 = one page)
  else if (a === '--json') opt.json = true;          // --search: emit the results as JSON instead of a table
  else if (/^\d+$/.test(a) && !opt.pid) opt.pid = a;
  else { console.error('floorplanner: unknown arg', a); process.exit(1); }
}
if (!opt.pid && !opt.serve && !opt.search) {
  console.error('usage: node data-tools/fmltools/floorplanner.js -pid=<projectid> [--name <id>] [--floor i] [--scale cm/px] [--maxfurn cm] [--baked] [--fetch-only] [--force] [--play]');
  console.error('   or: node data-tools/fmltools/floorplanner.js --serve [--play]   # fetch-bridge for the cart\'s id picker (run alongside the editor)');
  console.error('   or: node data-tools/fmltools/floorplanner.js --search [--count N] [--json]   # list PUBLIC plans (token-free) — pick an id to fetch');
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

// ---- search (token-free) ----
// GET a URL and JSON.parse the body. No auth — the search feed is public.
function httpGetJson(url) {
  return new Promise((resolve, reject) => {
    https.get(url, { headers: { 'User-Agent': 'dreamengine-fmltools' }, timeout: 30000 }, (res) => {
      const bufs = [];
      res.on('data', (d) => bufs.push(d));
      res.on('end', () => {
        if (res.statusCode !== 200) return reject(new Error(`HTTP ${res.statusCode} on ${url}`));
        try { resolve(JSON.parse(Buffer.concat(bufs).toString('utf8'))); }
        catch { reject(new Error('response was not JSON')); }
      });
    }).on('error', reject).on('timeout', function () { this.destroy(); reject(new Error('timeout')); });
  });
}

// Fetch `count` PUBLIC plans from the token-free search feed (newest first). The feed pages via a
// `search_after: [ts_ms, tiebreak]` cursor; loop pages until we've collected enough. No auth needed.
// Shared by the CLI (--search) and the --serve bridge (the cart's B/browse request).
async function fetchPublicList(count) {
  const want = Math.max(1, parseInt(count || '50', 10) || 50);
  const base = 'https://floorplanner.com/api/v2/projects/search.json?all=true';
  const out = [];
  let after = null;
  while (out.length < want) {
    const url = after ? `${base}&search_after=${encodeURIComponent(JSON.stringify(after))}` : base;
    const page = await httpGetJson(url);
    const rows = page.results || [];
    if (!rows.length) break;
    out.push(...rows);
    after = page.search_after;
    if (!after) break;
  }
  return out.slice(0, want).map((r) => ({
    id: r.id, name: r.name || '(untitled)', floors: r.floor_count, url: r.project_url, thumbnail: r.thumbnail,
  }));
}

// --search — the DISCOVERY half: browse the public feed at the CLI. Fetching a listed id into a
// playable plan still needs a token (ensureFml), so the flow is: browse → `--pid <id>` (or type/pick
// it in the cart's picker). Inside the cart, B (browse) requests this same list via the --serve bridge.
async function search(count, asJson) {
  let list;
  try { list = await fetchPublicList(count); }
  catch (e) { console.error('floorplanner: search failed —', e.message); process.exit(1); }
  if (asJson) { console.log(JSON.stringify(list, null, 2)); return; }
  console.log(`▸ public plans  (token-free feed — newest first, showing ${list.length})\n`);
  for (const p of list) {
    const nm = p.name.length > 46 ? p.name.slice(0, 45) + '…' : p.name;
    console.log(`  ${String(p.id).padEnd(11)} ${(p.floors + 'fl').padStart(4)}  ${nm.padEnd(47)} ${p.thumbnail || ''}`);
  }
  console.log(`\n  fetch one:  node data-tools/fmltools/floorplanner.js --pid <id> --play`);
  console.log(`  or type its id in the cart's TAB picker (with --serve running).`);
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
  console.log(`▸ bake art  furniture renders (24-bit, posterized pop) -> ${assets}`);
  // --rgb keeps true colour (blitted via pset_rgb) instead of the muddy 32-palette quantise;
  // saturate + posterize give it a punchy, graphic "pop". fml-sprites auto-detects the rgb manifest.
  node('fml-assets.js', [fml, '--out', assets, '--max', '24', '--rgb', '--saturate', '1.6', '--posterize', '5']);
  console.log(`▸ sprites   -> ${dataRel}`);
  node('fml-sprites.js', ['--json', data, '--manifest', path.join(assets, 'manifest.json')]);
  console.log(`▸ textures  resolve rs-#### floor materials -> ${dataRel}`);
  node('fml-textures.js', ['--json', data, '--out', `build/.fml-textures-${name}`]);
  return data;
}

// --serve — the fetch-bridge for the floorplan cart's id picker. Watches build/.fp-request for an
// id the cart writes (Enter an unknown id in the picker), fetches+builds it, then hands the data
// path back via build/.fp-ready (or a one-line message via build/.fp-error). It ALSO answers the
// cart's B/browse request (build/.fp-search-request) with a token-free page of public plans written
// to build/.fp-search-ready (one `id\tfloors\tname` line each). Run it alongside the editor; the cart
// polls these files. One relay serves any run. Design: docs/design/external-data-carts.md → "Loader".
async function serve(cred) {
  const BUILD = path.join(ROOT, 'build');
  fs.mkdirSync(BUILD, { recursive: true });
  const REQ = path.join(BUILD, '.fp-request'), READY = path.join(BUILD, '.fp-ready'), ERR = path.join(BUILD, '.fp-error');
  const SREQ = path.join(BUILD, '.fp-search-request'), SREADY = path.join(BUILD, '.fp-search-ready'), SERR = path.join(BUILD, '.fp-search-error');
  for (const f of [REQ, READY, ERR, SREQ, SREADY, SERR]) { try { fs.unlinkSync(f); } catch {} }   // clear stale signals
  if (opt.play) { console.log('▸ launch   floorplan cart…'); spawn('node', ['tools/play.js', 'floorplan', 'run'], { cwd: ROOT, stdio: 'inherit' }); }
  console.log(`▸ serve     watching build/.fp-request + .fp-search-request (auth ${cred.kind}) — type/browse an id in the cart's picker. Ctrl-C to stop.`);
  let busy = false, searchBusy = false;
  setInterval(async () => {
    // browse (token-free) — the cart's B writes .fp-search-request; answer with a page of public plans.
    if (!searchBusy && fs.existsSync(SREQ)) {
      searchBusy = true;
      let n = '60';
      try { const s = fs.readFileSync(SREQ, 'utf8').trim(); if (/^\d+$/.test(s)) n = s; fs.unlinkSync(SREQ); } catch {}
      try {
        const list = await fetchPublicList(n);
        const lines = list.map((p) => `${p.id}\t${p.floors}\t${(p.name || '').replace(/[\t\r\n]/g, ' ')}`).join('\n');
        fs.writeFileSync(SREADY + '.tmp', lines); fs.renameSync(SREADY + '.tmp', SREADY);   // atomic
        console.log(`\n▸ browse    served ${list.length} public plans`);
      } catch (e) {
        fs.writeFileSync(SERR, ((e && e.message) ? e.message : String(e)).split('\n')[0].slice(0, 120));
      }
      searchBusy = false;
    }
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
  if (opt.search) return search(opt.count, opt.json);   // token-free — no auth() needed

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
