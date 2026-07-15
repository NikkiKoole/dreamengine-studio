#!/usr/bin/env node
// reddit-gaps.js — the demand-DISCOVERY miner: mine a tribe's public RSS for what people
// keep ASKING FOR that nobody's built, and cross-reference it against what THIS repo could ship.
//
// The third demand tool, distinct from the other two:
//   • aso-*        = demand CAPTURE  (get found by people already searching the store)
//   • leads.js     = demand GENERATION (promote a cart you already made to its tribe)
//   • reddit-gaps  = demand DISCOVERY (decide what to build next) ← this one
// It feeds lever #1 in docs/design/demand-generation.md ("a delightful product") — the only
// lever that saves everything below it. Design: docs/design/demand-discovery.md.
//
// WHY RSS, NOT THE API. Reddit's API is approval-gated + its create-app form is broken for
// everyone (2026 crackdown), and the .json endpoints 403. But the PUBLIC ATOM FEEDS still serve
// 200 — listing feeds (/r/<sub>/top/.rss) AND search feeds (/r/<sub>/search.rss?q=...). RSS is a
// documented reader feature, so this is the front door, not circumvention. The search feed IS the
// wish-miner: point it at "is there an app" and it returns matching threads. Caveats baked in:
//   • polite: sequential fetches, --delay spacing, one 429 backoff-retry, on-disk cache.
//   • RSS carries NO score/comment count → we rank by CLUSTER SIZE (many threads, one ask =
//     strong demand), which is a sturdier signal than upvotes anyway.
//   • Reddit may block RSS someday → the fetch layer is the only Reddit-specific part; the
//     mine→cluster→cross-reference engine is source-agnostic (a paste/open-web pipe drops in).
//
// THE PIPELINE:  scan (RSS, cached) → wish-mine (regex patterns) → cluster (topic taxonomy)
//                → cross-reference the cart shelf (editor/public/carts/index.json) → ranked gaps.
// A gap = high demand × low coverage × on the engine's grain. Deterministic, no deps, no auth,
// no model tokens: it PREPARES the evidence; the human/agent reads it and decides (store-agents
// rule). The topic taxonomy + wish patterns are editable constants below — retune per tribe.
//
// COMMANDS
//   node tools/reddit-gaps.js <subreddit>            full run → ranked gap report (uses cache)
//     --refresh            ignore cache, re-fetch every feed
//     --limit <n>          entries per feed (default 25)
//     --delay <ms>         spacing between fetches (default 1500)
//     --queries "a,b,c"    extra search-phrase feeds on top of the built-ins
//     --raw                also dump every mined wish thread (title + link + matched types)
//     --json               machine-readable payload (no ANSI); implies no table
//   node tools/reddit-gaps.js <subreddit> --ingest <file|dir>   PASTE-BRIDGE: when Reddit throttles
//     our script, fetch the RSS in a browser (a human clicking one URL isn't rate-limited like a
//     hammering script), save the feeds, and merge them into the cache — same engine, no fetch.
//     Accepts one .xml/.rss/.atom file, a directory of them, or a saved cache JSON. Then report as usual.
//   node tools/reddit-gaps.js --drip [subs.txt]      fetch ONE sub per run (the stalest in the
//     rotation list, default tools/reddit-gaps-subs.txt) — the polite, never-bursting form meant
//     for a cron/launchd drip. Successive runs round-robin the list and accumulate caches over time.
//   node tools/reddit-gaps.js --check                offline self-test on a fixture (CI gate)
//
// Cache: tools/reddit-gaps-cache/<sub>.json (gitignored — we don't retain scraped data in the repo).

const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "..");
const INDEX = path.join(ROOT, "editor/public/carts/index.json");
const CACHE_DIR = path.join(__dirname, "reddit-gaps-cache");
const UA = "dreamengine-gaps/1.0 (demand-discovery feed reader; +github.com/NikkiKoole/dreamengine-studio)";

// ── tiny ANSI (skipped when not a TTY or --json) ────────────────────────────
let COLOR = process.stdout.isTTY;
const c = (n, s) => (COLOR ? `\x1b[${n}m${s}\x1b[0m` : s);
const bold = (s) => c(1, s), dim = (s) => c(2, s);
const green = (s) => c(32, s), yellow = (s) => c(33, s), cyan = (s) => c(36, s), red = (s) => c(31, s);

function die(msg) { console.error(msg); process.exit(1); }

// ── CONFIG: the feeds we pull ───────────────────────────────────────────────
// Listing feeds give the tribe's recent centre of gravity; search feeds are the wish probes.
const LISTINGS = [
  { path: "top/.rss?t=year",  label: "top-year"  },
  { path: "top/.rss?t=month", label: "top-month" },
  { path: "hot/.rss",         label: "hot"       },
  { path: "new/.rss",         label: "new"       },
];
const SEARCH_QUERIES = [
  "is there an app", "looking for an app", "any app that", "wish there was",
  "alternative to", "why isn't there", "need an app", "how do i", "app that can",
];

// ── CONFIG: wish patterns (classify + score an entry as unmet demand) ───────
// Matched against title+selftext, lowercased. Weight = how strongly it signals a real ask.
const WISH_PATTERNS = [
  { type: "request",     w: 3, re: /\bis there (an?|any)\b[^.?!]*\bapp\b/ },
  { type: "request",     w: 3, re: /\bany app (that|which|to)\b/ },
  { type: "request",     w: 2, re: /\bapp that (can|will|does|lets|would)\b/ },
  { type: "request",     w: 3, re: /\blooking for (an?|a|the)\b[^.?!]*\b(app|tool|way|synth|sampler|sequencer)\b/ },
  { type: "request",     w: 2, re: /\bneed (an?|a)\b[^.?!]*\bapp\b/ },
  { type: "request",     w: 1, re: /\bdoes (anyone|any app|anything|any)\b/ },
  { type: "request",     w: 1, re: /\brecommend|suggestions?\b/ },
  { type: "request",     w: 1, re: /\bbest app (for|to)\b/ },
  { type: "request",     w: 1, re: /\bhow (do|can) i\b/ },
  { type: "wish",        w: 3, re: /\bwish (there|it|i|they|apple)\b/ },
  { type: "wish",        w: 2, re: /\bwould love (an?|a|to|it|some)\b/ },
  { type: "wish",        w: 2, re: /\bif only\b/ },
  { type: "wish",        w: 1, re: /\bi (just )?want (an?|a)\b/ },
  { type: "frustration", w: 3, re: /\bwhy (is there no|isn'?t there|can'?t|does no|do no|aren'?t)\b/ },
  { type: "frustration", w: 3, re: /\bnothing (does|that|out there|seems|i've)\b/ },
  { type: "frustration", w: 2, re: /\bcan'?t find (an?|any|a)\b/ },
  { type: "frustration", w: 2, re: /\bthere'?s no\b/ },
  { type: "alternative", w: 2, re: /\balternative to\b/ },
  { type: "alternative", w: 2, re: /\breplacement for\b/ },
  { type: "alternative", w: 1, re: /\blike .{2,30} but\b/ },
];

// ── CONFIG: topic taxonomy (cluster wishes + judge our grain) ───────────────
// keys = substrings to match (lowercased). grain = how well this fits a humble lo-fi cart:
//   core = dead-centre (Tiny Jam territory) · edge = plausible but off-grain · off = not us.
const TOPICS = [
  { id: "synth",      label: "Synths",                 grain: "core", keys: ["synth", "oscillator", "analog", "subtractive", "wavetable", "moog", "animoog", " pad ", "fm synth", " vco", "patch"] },
  { id: "sampler",    label: "Samplers",               grain: "core", keys: ["sampl", "koala", "chop", "slice", "one-shot", "one shot", "field record"] },
  { id: "sequencer",  label: "Sequencers",             grain: "core", keys: ["sequenc", "step seq", "piano roll", " pattern", " arp", "euclid"] },
  { id: "drums",      label: "Drum machines / beats",  grain: "core", keys: [" drum", "beat maker", "beatmak", "808", "909", "drum kit", "drum machine", "percussion", "rhythm"] },
  { id: "looper",     label: "Loopers",                grain: "core", keys: ["looper", "loop station", "loopstation", "live loop", "loop pedal"] },
  { id: "generative", label: "Generative / ambient",   grain: "core", keys: ["generative", "algorithmic", "random", "ambient generat", "endless", "drone", "soundscape"] },
  { id: "chords",     label: "Chords / theory",        grain: "core", keys: ["chord", "scale", "music theory", "progression", "harmon", "voicing", "arpeggi"] },
  { id: "tape",       label: "Tape / lo-fi",           grain: "core", keys: ["tape", "lofi", "lo-fi", "cassette", "vinyl", "wobble", "saturat", "warble"] },
  { id: "effects",    label: "Effects / FX",           grain: "core", keys: ["reverb", "delay", " fx", " effect", "distortion", "granular", "vocoder", "filter "] },
  { id: "sfx",        label: "SFX / sound design",     grain: "core", keys: ["sfxr", "bfxr", "jsfxr", "sound effect", "sound design", "game sound", "8-bit sound", "8bit sound", "chiptune sfx", "retro sound", "laser sound", "coin sound", "power-up", "powerup", "blip"] },
  { id: "midi",       label: "MIDI routing / control", grain: "edge", keys: [" midi", "transpose", "cc ", "controller", "routing", "midi fx", " aum", "au host"] },
  { id: "vocal",      label: "Vocals",                 grain: "edge", keys: ["vocal", "autotune", "harmoniz", "singing", " voice "] },
  { id: "hardware",   label: "Hardware companion",     grain: "edge", keys: ["beatstep", "arturia", "launchpad", "hardware", "geoshred", "op-1", "op-z", "external", "eurorack"] },
  { id: "daw",        label: "DAW / arrangement",      grain: "edge", keys: [" daw", "multitrack", "arrange", "mixdown", "full song", "song structure", "stems"] },
  { id: "notation",   label: "Notation / sheet music", grain: "off",  keys: ["notation", "sheet music", "staff", "score", "transcri", "tablature", " tab "] },
  { id: "dj",         label: "DJ / mixing",            grain: "off",  keys: [" dj ", "djing", "crossfad", "turntable", " deck", "beatmatch"] },
  { id: "utility",    label: "Utilities (bpm/tuner)",  grain: "off",  keys: ["bpm", "metronome", "tuner", "convert", "file format", "export ", "import "] },
];
const GRAIN_W = { core: 1, edge: 0.5, off: 0.15 };
const GAP_MIN = 2; // a topic needs >= this many distinct threads to count as real demand

// ── RSS/Atom parsing (regex — Reddit's Atom is regular; no xml dep) ─────────
function decodeHtml(s) {
  return String(s)
    .replace(/&lt;/g, "<").replace(/&gt;/g, ">")
    .replace(/&quot;/g, '"').replace(/&#0?39;/g, "'").replace(/&apos;/g, "'")
    .replace(/&#(\d+);/g, (_, n) => String.fromCharCode(+n))
    .replace(/&amp;/g, "&");
}
const stripTags = (s) => String(s).replace(/<[^>]+>/g, " ");
const clean = (s) => decodeHtml(stripTags(decodeHtml(s))).replace(/\s+/g, " ").trim();

function parseEntries(xml) {
  const out = [];
  const blocks = xml.match(/<entry>[\s\S]*?<\/entry>/g) || [];
  for (const b of blocks) {
    const title = clean((b.match(/<title[^>]*>([\s\S]*?)<\/title>/) || [])[1] || "");
    const link = ((b.match(/<link[^>]*href="([^"]+)"/) || [])[1] || "").replace(/&amp;/g, "&");
    const content = clean((b.match(/<content[^>]*>([\s\S]*?)<\/content>/) || [])[1] || "");
    const published = ((b.match(/<published>([\s\S]*?)<\/published>/) || [])[1]
      || (b.match(/<updated>([\s\S]*?)<\/updated>/) || [])[1] || "").trim();
    const author = clean((b.match(/<author>[\s\S]*?<name>([\s\S]*?)<\/name>/) || [])[1] || "");
    const idm = link.match(/comments\/(\w+)\//);
    const id = idm ? idm[1] : ((b.match(/<id>([\s\S]*?)<\/id>/) || [])[1] || link).trim();
    if (title || content) out.push({ id, title, link, content, published, author });
  }
  return out;
}

// ── fetch layer (the only Reddit-specific part) ─────────────────────────────
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// How long to wait after a 429: honour Reddit's Retry-After header (it tells you exactly,
// in seconds or as an HTTP date), capped at 2 min; else fall back to exponential 4/8/16/32s.
function retryAfterMs(res, attempt) {
  const ra = res.headers.get("retry-after");
  if (ra) {
    const secs = Number(ra);
    if (Number.isFinite(secs)) return Math.min(secs * 1000, 120000);
    const when = Date.parse(ra);
    if (!Number.isNaN(when)) return Math.max(0, Math.min(when - Date.now(), 120000));
  }
  return 4000 * 2 ** attempt;
}

// Reddit throttles RSS aggressively (bursty 429s). Retry up to MAX times, waiting per Retry-After.
async function fetchText(url) {
  const MAX = 4;
  for (let attempt = 0; attempt <= MAX; attempt++) {
    let res;
    try {
      res = await fetch(url, { headers: { "User-Agent": UA, "Accept": "application/atom+xml" } });
    } catch (e) {
      if (attempt === MAX) return { ok: false, status: 0, err: e.message };
      await sleep(4000 * 2 ** attempt); continue;
    }
    if (res.status === 429) {
      if (attempt === MAX) return { ok: false, status: 429 };
      const wait = retryAfterMs(res, attempt);
      process.stderr.write(dim(`429(${Math.round(wait / 1000)}s)…`));
      await sleep(wait); continue;
    }
    if (!res.ok) return { ok: false, status: res.status };
    return { ok: true, status: res.status, text: await res.text() };
  }
  return { ok: false, status: 429 };
}

// Search (wish-probe) feeds FIRST — they carry the demand signal, so when Reddit throttles
// we want them to land before the budget's spent on listing feeds (mere popularity).
function feedUrls(sub, limit, extraQueries) {
  const urls = [];
  for (const q of [...SEARCH_QUERIES, ...extraQueries]) {
    const qs = encodeURIComponent(`"${q}"`);
    urls.push({ label: `q:${q}`, url: `https://www.reddit.com/r/${sub}/search.rss?q=${qs}&restrict_sr=1&sort=relevance&limit=${limit}` });
  }
  for (const l of LISTINGS) {
    const sep = l.path.includes("?") ? "&" : "?";
    urls.push({ label: l.label, url: `https://www.reddit.com/r/${sub}/${l.path}${sep}limit=${limit}` });
  }
  return urls;
}

async function scan(sub, opts) {
  const feeds = feedUrls(sub, opts.limit, opts.extraQueries);
  const byId = new Map();
  // Seed from any existing cache so a throttled run ACCUMULATES rather than starting over —
  // repeated runs top up the pool, and a killed/timed-out run keeps what it already got.
  const prior = loadCache(sub);
  if (prior && prior.entries) for (const e of prior.entries) byId.set(e.id, { ...e, feeds: e.feeds || [] });
  const priorCount = byId.size;
  const snapshot = () => ({ sub, fetchedAt: new Date().toISOString(), okFeeds, failed, entries: [...byId.values()] });
  let okFeeds = 0, failed = [];
  for (let i = 0; i < feeds.length; i++) {
    const f = feeds[i];
    process.stderr.write(dim(`  [${i + 1}/${feeds.length}] ${f.label} … `));
    const r = await fetchText(f.url);
    if (!r.ok) { process.stderr.write(red(`fail (${r.status || r.err})\n`)); failed.push(f.label); }
    else {
      const entries = parseEntries(r.text);
      okFeeds++;
      process.stderr.write(green(`${entries.length}\n`));
      for (const e of entries) {
        const prev = byId.get(e.id);
        if (prev) { if (!prev.feeds.includes(f.label)) prev.feeds.push(f.label); }
        else byId.set(e.id, { ...e, feeds: [f.label] });
      }
      saveCache(sub, snapshot()); // incremental: persist after every successful feed
    }
    if (i < feeds.length - 1) await sleep(opts.delay);
  }
  if (priorCount) process.stderr.write(dim(`  (${priorCount} thread(s) carried over from cache)\n`));
  return snapshot();
}

function cachePath(sub) { return path.join(CACHE_DIR, `${sub}.json`); }
function loadCache(sub) { try { return JSON.parse(fs.readFileSync(cachePath(sub), "utf8")); } catch { return null; } }
function saveCache(sub, data) {
  fs.mkdirSync(CACHE_DIR, { recursive: true });
  fs.writeFileSync(cachePath(sub), JSON.stringify(data, null, 2) + "\n");
}

// The paste-bridge (design/demand-discovery.md): when Reddit throttles our script, a human's
// browser can still fetch RSS. Save those feeds to files and merge them into the cache here —
// same engine, no fetch. Accepts a single file, or a directory of .xml/.rss/.atom/.json feeds
// (a saved cache JSON with an `entries` array works too). Accumulates into any existing cache.
function ingestFiles(sub, p) {
  let stat; try { stat = fs.statSync(p); } catch { die(`--ingest: no such path: ${p}`); }
  const files = stat.isDirectory()
    ? fs.readdirSync(p).filter((f) => /\.(xml|rss|atom|txt|json)$/i.test(f)).map((f) => path.join(p, f))
    : [p];
  if (!files.length) die(`--ingest: no .xml/.rss/.atom/.json files in ${p}`);
  const byId = new Map();
  const prior = loadCache(sub);
  if (prior && prior.entries) for (const e of prior.entries) byId.set(e.id, { ...e, feeds: e.feeds || [] });
  let added = 0;
  for (const file of files) {
    const raw = fs.readFileSync(file, "utf8");
    const label = `imported:${path.basename(file)}`;
    let entries = [];
    if (raw.trim().startsWith("{")) { try { const j = JSON.parse(raw); if (Array.isArray(j.entries)) entries = j.entries; } catch { /* not our json — fall through to XML */ } }
    if (!entries.length) entries = parseEntries(raw);
    for (const e of entries) {
      const id = e.id || (e.link && (e.link.match(/comments\/(\w+)\//) || [])[1]) || e.link;
      if (!id) continue;
      const feeds = e.feeds && e.feeds.length ? e.feeds : [label];
      const prev = byId.get(id);
      if (prev) { for (const fl of feeds) if (!prev.feeds.includes(fl)) prev.feeds.push(fl); }
      else { byId.set(id, { id, title: e.title || "", link: e.link || "", content: e.content || "", published: e.published || "", author: e.author || "", feeds }); added++; }
    }
    process.stderr.write(dim(`  ingested ${path.basename(file)} → ${entries.length} entries\n`));
  }
  const data = { sub, fetchedAt: new Date().toISOString(), okFeeds: 0, failed: [], entries: [...byId.values()] };
  saveCache(sub, data);
  process.stderr.write(green(`  merged into cache: ${data.entries.length} threads (${added} new)\n`));
  return data;
}

// ── wish-mine ───────────────────────────────────────────────────────────────
function classify(entry) {
  const text = `${entry.title} ${entry.content}`.toLowerCase();
  const types = new Set();
  let score = 0;
  for (const p of WISH_PATTERNS) if (p.re.test(text)) { types.add(p.type); score += p.w; }
  return { score, types: [...types] };
}
function mine(entries) {
  return entries
    .map((e) => ({ ...e, ...classify(e) }))
    .filter((e) => e.score > 0)
    .sort((a, b) => b.feeds.length - a.feeds.length || b.score - a.score);
}

// ── cluster by topic ─────────────────────────────────────────────────────────
// Pad with spaces so a key's leading/trailing-space word-boundary guards (" drum", " dj ")
// actually bind — a bare "dj" must not match "django".
function topicsFor(text) {
  const t = ` ${text.toLowerCase()} `;
  const ids = TOPICS.filter((topic) => topic.keys.some((k) => t.includes(k))).map((x) => x.id);
  return ids.length ? ids : ["other"];
}

// ── cross-reference the cart shelf ───────────────────────────────────────────
function audioTags() {
  try {
    const v = require("./teaches-vocab.js");
    const cats = v.CATEGORIES || {};
    return new Set((cats.audio && cats.audio.tags) || []);
  } catch { return new Set(); }
}
function loadCorpus() {
  let carts;
  try { carts = JSON.parse(fs.readFileSync(INDEX, "utf8")); }
  catch (e) { die(`cannot read cart index (${INDEX}): ${e.message}`); }
  const AUDIO = audioTags();
  const musicish = (c) => {
    const kinds = (c.kind || []).map((k) => String(k).toLowerCase());
    if (kinds.some((k) => ["instrument", "radio", "synth", "music"].includes(k))) return true;
    if (c.genre && /music|synth|rhythm|audio/i.test(String(c.genre))) return true;
    if ((c.teaches || []).some((t) => AUDIO.has(t))) return true;
    return /synth|sound|audio|music|sequenc|drum|piano|sampler|reverb|tape|chord/i.test(`${c.title} ${c.description}`);
  };
  return carts.filter(musicish).map((c) => ({
    title: c.title,
    text: `${c.title} ${c.description} ${(c.teaches || []).join(" ").replace(/-/g, " ")} ${c.genre || ""} ${(c.kind || []).join(" ")}`.toLowerCase(),
  }));
}
function coverage(topic, corpus) {
  const hits = corpus.filter((c) => { const t = ` ${c.text} `; return topic.keys.some((k) => t.includes(k)); });
  return { count: hits.length, samples: hits.slice(0, 3).map((h) => h.title) };
}

// ── build the report ─────────────────────────────────────────────────────────
function buildReport(scanData, corpus) {
  const wishes = mine(scanData.entries);
  const clustered = new Map(TOPICS.map((t) => [t.id, []]));
  clustered.set("other", []);
  for (const w of wishes) for (const id of topicsFor(`${w.title} ${w.content}`)) clustered.get(id).push(w);

  const rows = TOPICS.map((topic) => {
    const threads = clustered.get(topic.id) || [];
    const demand = threads.length;
    const cov = coverage(topic, corpus);
    const gap = cov.count === 0;
    const gapScore = demand * GRAIN_W[topic.grain] * (gap ? 2 : 0.4);
    let verdict;
    if (topic.grain === "off") verdict = "off-grain";
    else if (demand === 0) verdict = "no signal";
    else if (gap && demand >= GAP_MIN) verdict = "GAP";
    else if (gap) verdict = "weak gap";
    else verdict = demand >= GAP_MIN ? "covered (hot)" : "covered";
    return { ...topic, demand, coverage: cov.count, coverSamples: cov.samples, verdict, gapScore,
      threads: threads.map((t) => ({ title: t.title, link: t.link, types: t.types, feeds: t.feeds.length })) };
  }).sort((a, b) => b.gapScore - a.gapScore);

  const other = (clustered.get("other") || []);
  return { sub: scanData.sub, fetchedAt: scanData.fetchedAt, okFeeds: scanData.okFeeds,
    failed: scanData.failed, entries: scanData.entries.length, wishes: wishes.length,
    topics: rows, uncategorized: other.length, wishList: wishes };
}

// ── printing ──────────────────────────────────────────────────────────────────
const ICON = { "GAP": "★", "weak gap": "◦", "covered (hot)": "●", "covered": "·", "off-grain": " ", "no signal": " " };
function paint(v, s) {
  if (v === "GAP") return green(s);
  if (v === "weak gap") return yellow(s);
  if (v === "covered (hot)" || v === "covered") return dim(s);
  return dim(s);
}
function printReport(r, opts) {
  console.log("");
  console.log(bold(`  r/${r.sub}`) + dim(`  · ${r.okFeeds} feeds · ${r.entries} threads · ${r.wishes} wishes mined · ${r.fetchedAt.slice(0, 10)}`));
  if (r.failed && r.failed.length) console.log(dim(`  (feeds that failed: ${r.failed.join(", ")})`));
  console.log(dim(`  ranked by gap-score = demand × grain × (uncovered?2:0.4). ★=GAP  ●=covered+hot  ·=covered  ◦=weak\n`));
  console.log(dim("  verdict         topic                     demand  ours  examples"));
  for (const t of r.topics) {
    if (t.demand === 0 && t.coverage === 0) continue;
    const ic = ICON[t.verdict] || " ";
    const v = (ic + " " + t.verdict).padEnd(15);
    const label = t.label.padEnd(25);
    const dem = String(t.demand).padStart(6);
    const ours = String(t.coverage).padStart(5);
    const ex = t.threads.slice(0, 1).map((x) => x.title).join("")
      || (t.coverSamples.length ? dim("we: " + t.coverSamples.slice(0, 2).join(", ")) : "");
    console.log("  " + paint(t.verdict, v) + label + dem + ours + "  " + dim(ex.slice(0, 60)));
  }

  const gaps = r.topics.filter((t) => t.verdict === "GAP");
  if (gaps.length) {
    console.log("\n" + bold("  ★ TOP UNMET DEMANDS (on-grain, high ask, we don't cover):"));
    for (const g of gaps.slice(0, 5)) {
      console.log("\n  " + green(bold(g.label)) + dim(`  — ${g.demand} threads, ${g.coverage} of our carts`));
      for (const th of g.threads.slice(0, 4)) console.log("    " + dim("•") + " " + th.title.slice(0, 80));
    }
  } else {
    console.log("\n  " + dim("no clean GAP this run — widen with --queries or --refresh, or the tribe is well-served on-grain."));
  }

  if (opts.raw) {
    console.log("\n" + bold(`  RAW WISHES (${r.wishList.length}):`));
    for (const w of r.wishList) console.log("  " + dim("[" + w.types.join("/") + "]") + " " + w.title.slice(0, 78) + "\n    " + dim(w.link));
  }
  console.log("");
}

// ── offline self-test ─────────────────────────────────────────────────────────
function selfCheck() {
  COLOR = false;
  const FIXTURE = [
    { id: "a", title: "Is there an app that does generative ambient drones?", content: "looking for something endless", link: "x", feeds: ["q:is there an app", "top-year"] },
    { id: "b", title: "wish there was a good generative melody tool", content: "", link: "w", feeds: ["q:wish there was"] },
    { id: "c", title: "Why isn't there a simple tape lo-fi cassette wobble app?", content: "nothing does this", link: "y", feeds: ["q:why isn't there"] },
    { id: "d", title: "Looking for an app for lo-fi cassette tape saturation", content: "", link: "v", feeds: ["q:looking for an app"] },
    { id: "e", title: "Looking for an app like a step sequencer but simpler", content: "", link: "z", feeds: ["q:looking for an app"] },
    { id: "f", title: "Is there a sheet music notation app with staff view?", content: "", link: "n", feeds: ["q:is there an app"] },
    { id: "g", title: "just chatting about my favourite synth today", content: "no ask here", link: "u", feeds: ["hot"] }, // not a wish
  ];
  const corpus = [
    { title: "acidrack", text: "acid synth 303 subtractive-synth step-sequencer" },
    { title: "epiano", text: "electric piano fm-synth" },
  ];
  const r = buildReport({ sub: "fixture", fetchedAt: "2026-07-13T00:00:00Z", okFeeds: 4, failed: [], entries: FIXTURE }, corpus);
  const problems = [];
  const wish = mine(FIXTURE);
  if (wish.length !== 6) problems.push(`expected 6 wishes (all but the chit-chat), got ${wish.length}`);
  const gen = r.topics.find((t) => t.id === "generative");
  if (!gen || gen.demand < 2) problems.push(`generative should cluster >=2 threads, got ${gen && gen.demand}`);
  if (!gen || gen.verdict !== "GAP") problems.push(`generative should be a GAP (uncovered, >=2), got ${gen && gen.verdict}`);
  const tape = r.topics.find((t) => t.id === "tape");
  if (!tape || tape.demand < 2) problems.push(`tape should cluster >=2 threads, got ${tape && tape.demand}`);
  if (!tape || tape.verdict !== "GAP") problems.push(`tape should be a GAP, got ${tape && tape.verdict}`);
  const seq = r.topics.find((t) => t.id === "sequencer");
  if (!seq || seq.coverage < 1) problems.push(`sequencer should be covered (acidrack), got coverage ${seq && seq.coverage}`);
  const notation = r.topics.find((t) => t.id === "notation");
  if (!notation || notation.verdict !== "off-grain") problems.push(`notation should be off-grain, got ${notation && notation.verdict}`);

  if (problems.length) { console.error("FAIL reddit-gaps self-test:\n  - " + problems.join("\n  - ")); process.exit(1); }
  console.log("reddit-gaps self-test PASS (mine + cluster + cross-reference on fixture)");
}

// ── drip: fetch ONE sub per run (the stalest) so a cron drip round-robins a list ──
// without ever bursting. Sub list = tools/reddit-gaps-subs.txt (one per line, # comments),
// or a path after --drip. "Stalest" = oldest cache mtime (missing cache sorts first).
const SUBS_FILE = path.join(__dirname, "reddit-gaps-subs.txt");
async function drip(argv) {
  const i = argv.indexOf("--drip");
  const maybe = argv[i + 1];
  const file = maybe && !maybe.startsWith("--") ? maybe : SUBS_FILE;
  let subs;
  try { subs = fs.readFileSync(file, "utf8").split("\n").map((s) => s.replace(/#.*/, "").trim()).filter(Boolean); }
  catch (e) { die(`--drip: cannot read subs list ${file}: ${e.message}`); }
  if (!subs.length) die(`--drip: no subs listed in ${file}`);
  const di = argv.indexOf("--delay");
  const delay = di >= 0 && argv[di + 1] ? parseInt(argv[di + 1], 10) : 4000;

  const staleness = subs.map((s) => {
    let m = 0; try { m = fs.statSync(cachePath(s)).mtimeMs; } catch { m = 0; }
    return { s, m };
  }).sort((a, b) => a.m - b.m);
  const pick = staleness[0].s;
  process.stderr.write(dim(`  drip: ${subs.length} sub(s) in rotation; stalest = r/${pick}\n`));

  const data = await scan(pick, { limit: 25, delay, extraQueries: [], raw: false });
  data.sub = pick;
  // Always persist (even an empty snapshot) so this sub's cache mtime advances — otherwise a sub
  // that never returns data (dead name, or a persistent throttle) would stay "stalest" forever and
  // starve the rotation. A real-but-throttled sub keeps its prior threads (scan seeds from cache);
  // it just gets retried on its next turn, not immediately.
  saveCache(pick, data);
  const wishes = mine(data.entries).length;
  const stamp = new Date().toISOString();
  console.log(`[reddit-gaps drip ${stamp}] r/${pick}: ${data.okFeeds} feeds ok, ${data.entries.length} threads, ${wishes} wishes cached`);
}

// ── main ───────────────────────────────────────────────────────────────────
async function main() {
  const argv = process.argv.slice(2);
  if (argv.includes("--check")) return selfCheck();
  if (argv.includes("--drip")) return drip(argv);
  const flag = (name, def) => { const i = argv.indexOf(name); return i >= 0 && argv[i + 1] ? argv[i + 1] : def; };
  const has = (name) => argv.includes(name);
  const VALUE_FLAGS = new Set(["--limit", "--delay", "--queries", "--ingest"]);
  const sub = argv.find((a) => !a.startsWith("--") && !VALUE_FLAGS.has(argv[argv.indexOf(a) - 1]));

  if (!sub || has("--help") || has("-h")) {
    console.log("usage: node tools/reddit-gaps.js <subreddit> [--refresh] [--limit n] [--delay ms] [--queries \"a,b\"] [--raw] [--json]");
    console.log("       node tools/reddit-gaps.js <subreddit> --ingest <file|dir>   (parse browser-saved RSS instead of fetching)");
    console.log("       node tools/reddit-gaps.js --drip [subs.txt]                 (fetch the stalest sub — for a cron drip)");
    console.log("       node tools/reddit-gaps.js --check");
    console.log("\nMine a tribe's public RSS for unmet demand, cross-referenced against our cart shelf.");
    console.log("e.g.  node tools/reddit-gaps.js ipadmusic");
    process.exit(sub ? 0 : 1);
  }

  const json = has("--json");
  if (json) COLOR = false;
  const opts = {
    limit: parseInt(flag("--limit", "25"), 10),
    delay: parseInt(flag("--delay", "1500"), 10),
    extraQueries: (flag("--queries", "") || "").split(",").map((s) => s.trim()).filter(Boolean),
    raw: has("--raw"),
  };

  let scanData;
  if (has("--ingest")) {
    const p = flag("--ingest");
    if (!p) die("--ingest needs a file or directory path");
    scanData = ingestFiles(sub, p);
  } else {
  const cached = loadCache(sub);
  if (cached && !has("--refresh")) {
    process.stderr.write(dim(`  using cache ${path.relative(ROOT, cachePath(sub))} (${cached.fetchedAt}, ${cached.entries.length} threads); --refresh to top up\n`));
    scanData = cached;
  } else {
    process.stderr.write(dim(`  fetching r/${sub} RSS feeds (polite, ${opts.delay}ms spacing, backoff on 429)…\n`));
    scanData = await scan(sub, opts);
    if (scanData.okFeeds === 0 && !scanData.entries.length)
      die(`\n  every feed failed for r/${sub} and no cache to fall back on — sub misspelled/private, or RSS is throttled (wait, then retry with a bigger --delay).`);
    if (scanData.okFeeds === 0) process.stderr.write(dim(`  (all feeds throttled this run — reporting from ${scanData.entries.length} cached threads)\n`));
  }
  }
  scanData.sub = sub;

  const corpus = loadCorpus();
  const report = buildReport(scanData, corpus);
  if (json) { console.log(JSON.stringify(report, null, 2)); return; }
  printReport(report, opts);
}

main().catch((e) => die(`reddit-gaps: ${e.stack || e.message}`));
