#!/usr/bin/env node
// leads.js — the local MARKETEER: find PLACES to post about a cart, and track outreach.
//
// The demand-GENERATION twin of the store-agents (which are demand CAPTURE / ASO). Per
// docs/design/demand-generation.md the biggest download lever after the product itself is
// "showing up in the tribe" — this tool operationalises that: it maps a cart to its tribe(s),
// hands you the venues where that tribe already gathers, discovers NEW venues from Reddit's
// free API, scaffolds a gift-first post, and keeps a ledger of what you've posted where.
//
// Data lives in tools/leads-ledger.json (committed, hand-editable — the source of truth for
// tribes/venues/hooks). This tool READS it for match/draft, and APPENDS to its outreach[] on
// `track add`. Cart metadata comes from each cart's de:meta (reused via build-cart-index.js).
//
// Honours the repo's "scripts prepare → human decides → scripts execute" rule (store-agents.md):
// `draft` emits a SCAFFOLD seeded with the cart's OWN description + the tribe's hook — it does
// not invent marketing prose. You write the voice; it lays out the beats and hands you the raw
// materials + the venue's etiquette so you don't post the wrong tone into a gift-first space.
//
// COMMANDS
//   node tools/leads.js match <cart>              tribes + venues for a cart (offline)
//   node tools/leads.js discover <cart|term...>   venue-hunt links + autocomplete signals
//   node tools/leads.js draft <cart> [tribe|venue]  a gift-first post scaffold (offline)
//   node tools/leads.js track                      the outreach board (grouped by status)
//   node tools/leads.js track add <cart> "<venue>" [--status todo|posted|responded]
//                                                  [--url <link>] [--note "..."]   (writes ledger)
//   node tools/leads.js audit [--json]            coverage over EVERY cart + bucket-gaps (local)
//   node tools/leads.js list [tribeId]             the whole ledger (or one tribe)
//   node tools/leads.js --check                    offline self-test (CI gate)
//
// Network: discover hits Google Autocomplete (no auth, no key — same free-public-data stance
// as aso-suggest.js). Reddit's free JSON API is gone (403 "Blocked" since the 2023 lockdown),
// so discovery is a venue-HUNT launcher (ready search urls that always work) + autocomplete
// community signals as a bonus. Fails soft: no network → the hunt links still print. match /
// draft / track / list are fully offline.

const fs = require("fs");
const path = require("path");
const { readMeta, flattenDesc } = require("./build-cart-index.js");

const ROOT = path.resolve(__dirname, "..");
const LEDGER_PATH = path.join(__dirname, "leads-ledger.json");
const CARTS = path.join(__dirname, "carts");
const UA = "dreamengine-leads/1 (local marketeer tool)";

// ── tiny ANSI (skipped when not a TTY) ──────────────────────────────────────
const tty = process.stdout.isTTY;
const c = (n, s) => (tty ? `\x1b[${n}m${s}\x1b[0m` : s);
const bold = (s) => c(1, s), dim = (s) => c(2, s);
const green = (s) => c(32, s), yellow = (s) => c(33, s), cyan = (s) => c(36, s);

function die(msg) { console.error(msg); process.exit(1); }

// ── ledger ───────────────────────────────────────────────────────────────────
function loadLedger() {
  try { return JSON.parse(fs.readFileSync(LEDGER_PATH, "utf8")); }
  catch (e) { die(`leads-ledger.json: ${e.message}`); }
}
function saveLedger(l) {
  fs.writeFileSync(LEDGER_PATH, JSON.stringify(l, null, 2) + "\n");
}

// ── cart metadata → searchable blob + tag set ─────────────────────────────────
function loadCart(name) {
  const file = path.join(CARTS, `${name}.c`);
  if (!fs.existsSync(file)) die(`no such cart: tools/carts/${name}.c`);
  const meta = readMeta(fs.readFileSync(file, "utf8"), name);
  if (!meta) die(`${name}: no de:meta block (test/probe cart?) — pass search terms directly instead`);
  return meta;
}
// everything about the cart a tribe tag might match against, lowercased.
// de:meta is loose: kind[] is an array, genre is a STRING, teaches[] an array — coerce all.
const asList = (v) => (Array.isArray(v) ? v : v ? [v] : []);
function cartBlob(meta) {
  const parts = [
    meta.title,
    asList(meta.kind).join(" "),
    asList(meta.genre).join(" "),
    asList(meta.teaches).join(" ").replace(/-/g, " "),
    meta.lineage || "",
    meta.homage || "",
    flattenDesc(meta.description),
  ];
  return parts.join("  \n  ").toLowerCase();
}
// a cart's audience DOMAIN, from de:meta — so a game doesn't get shown music-press venues.
// instrument/radio carts → music; anything with a genre (games need one) or kind "game" → game.
function cartDomain(meta) {
  const kinds = asList(meta.kind).map((k) => String(k).toLowerCase());
  if (kinds.some((k) => k === "instrument" || k === "radio" || k === "synth")) return "music";
  if (asList(meta.genre).length || kinds.includes("game")) return "game";
  return "any"; // unknown → don't hide anything
}
const domainOk = (cartDom, entryDom) => cartDom === "any" || entryDom === "any" || cartDom === entryDom;

// does a tag hit the blob? multi-word/hyphenated → substring; single word → whole-word.
// Short (≤2-char) tags like "fm" need ≥2 hits: one incidental mention ("the fm cart" in an
// epiano that ISN'T FM) shouldn't route a cart to the wrong tribe; a real FM cart says it often.
function tagHits(tag, blob) {
  const t = tag.toLowerCase();
  if (/[\s-]/.test(t)) return blob.includes(t);
  const re = new RegExp(`\\b${t.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}\\b`, "g");
  const n = (blob.match(re) || []).length;
  return t.length <= 2 ? n >= 2 : n >= 1;
}
// score each tribe against the cart; return sorted [{tribe, score, matched[]}].
// only tribes in the cart's domain (or domain-agnostic ones) are considered.
function matchTribes(meta, ledger) {
  const blob = cartBlob(meta);
  const dom = cartDomain(meta);
  return ledger.tribes
    .filter((tribe) => domainOk(dom, tribe.domain || "any"))
    .map((tribe) => {
      const matched = tribe.tags.filter((t) => tagHits(t, blob));
      return { tribe, score: matched.length, matched };
    })
    .filter((r) => r.score > 0)
    .sort((a, b) => b.score - a.score);
}

// ── audit: run match across EVERY cart → a compact coverage report ────────────
// Pure local JS (readMeta over all carts) — no network, no model tokens. Surfaces the
// bucket-gaps all at once: which real carts land in no tribe (= a tribe worth adding).
function cmdAudit(rest) {
  const ledger = loadLedger();
  const wantJson = rest.includes("--json");
  const files = fs.readdirSync(CARTS).filter((f) => f.endsWith(".c"));
  const rows = [];
  for (const f of files) {
    const name = f.slice(0, -2);
    const meta = readMeta(fs.readFileSync(path.join(CARTS, f), "utf8"), name);
    if (!meta) continue; // no de:meta = test/probe cart, not a product
    const dom = cartDomain(meta);
    const hits = matchTribes(meta, ledger);
    rows.push({ name, title: meta.title || name, domain: dom, tribes: hits.map((h) => h.tribe.id) });
  }
  if (wantJson) { console.log(JSON.stringify(rows, null, 2)); return; }

  const by = (d) => rows.filter((r) => r.domain === d);
  const unmatched = (d) => by(d).filter((r) => !r.tribes.length);
  const tribeCount = {};
  rows.forEach((r) => r.tribes.forEach((t) => (tribeCount[t] = (tribeCount[t] || 0) + 1)));

  const pct = (n, d) => (d ? Math.round((n / d) * 100) : 0);
  console.log(`\n${bold("Leads coverage")}  ${dim(`(${rows.length} carts with de:meta, of ${files.length} files)`)}`);
  console.log(`  domains — ${cyan("music")} ${by("music").length} · ${cyan("game")} ${by("game").length} · ${dim("other")} ${by("any").length}\n`);

  for (const d of ["music", "game"]) {
    const all = by(d), miss = unmatched(d);
    const matched = all.length - miss.length;
    console.log(`  ${bold(d)}: ${green(matched + " matched")} / ${all.length}  ${dim(`(${pct(matched, all.length)}%)`)}`);
    if (miss.length) {
      console.log(`    ${yellow("no tribe — gaps to fill:")}`);
      // group by title for readability, cap the printed list
      const names = miss.map((r) => r.title).sort();
      const shown = names.slice(0, 40);
      console.log("      " + shown.join(dim(" · ")));
      if (names.length > shown.length) console.log(dim(`      …and ${names.length - shown.length} more (use --json for all)`));
    }
    console.log("");
  }
  console.log(`  ${bold("buckets by cart count")}`);
  const ranked = Object.entries(tribeCount).sort((a, b) => b[1] - a[1]);
  for (const [id, n] of ranked) console.log(`    ${id.padEnd(18)} ${dim(String(n).padStart(3))}`);
  const empty = ledger.tribes.filter((t) => !tribeCount[t.id]).map((t) => t.id);
  if (empty.length) console.log(dim(`    (no carts yet: ${empty.join(", ")})`));
  console.log("");
}

const KIND_ICON = { reddit: "🔺", facebook: "📘", forum: "💬", youtube: "▶", web: "🌐", discord: "🎧", tiktok: "🎵" };
function venueLine(v) {
  const icon = KIND_ICON[v.kind] || "•";
  const et = v.etiquette ? dim(`  — ${v.etiquette}`) : "";
  return `    ${icon} ${bold(v.name)}  ${cyan(v.url)}${et}`;
}

// ── match ──────────────────────────────────────────────────────────────────
function cmdMatch(name) {
  const ledger = loadLedger();
  const meta = loadCart(name);
  const hits = matchTribes(meta, ledger);
  console.log(`\n${bold(`Leads for ${cyan(meta.title || name)}`)}  ${dim(`(cart: ${name})`)}\n`);
  if (!hits.length) {
    console.log(dim("  No tribe matched this cart's de:meta.\n"));
    console.log(dim("  Try `discover` with explicit terms, e.g.:"));
    console.log(dim(`    node tools/leads.js discover ${name} "your genre" "your instrument"\n`));
  }
  for (const { tribe, matched } of hits) {
    console.log(`  ${bold(tribe.label)}  ${dim(`[matched: ${matched.join(", ")}]`)}`);
    if (tribe.hook) console.log(`    ${green("hook →")} ${tribe.hook}`);
    tribe.venues.forEach((v) => console.log(venueLine(v)));
    console.log("");
  }
  const dom = cartDomain(meta);
  const cc = ledger.crosscutting.filter((v) => domainOk(dom, v.domain || "any"));
  console.log(`  ${bold("Cross-cutting")}  ${dim(`(every ${dom === "any" ? "" : dom + " "}launch also posts here)`)}`);
  cc.forEach((v) => console.log(venueLine(v)));
  console.log(`\n  ${dim("next:")} node tools/leads.js draft ${name}   ·   node tools/leads.js discover ${name}\n`);
}

// ── discover (venue-hunt launcher + Google-autocomplete signals) ─────────────
// Reddit's free JSON API is dead (403 "Blocked" since the 2023 API lockdown), so discovery
// is: (1) ready-to-click SEARCH urls per term across where tribes gather — reliable, no-auth,
// works every time; plus (2) Google-autocomplete suggestions filtered to community signals
// (same free source as aso-suggest.js) — a bonus that surfaces named scenes/venues when the
// term has a strong identity (e.g. "dungeon synth" → reddit / festival / labels / bandcamp).

const VENUE_WORDS = ["reddit", "subreddit", "forum", "subforum", "discord", "community",
  "fans", "fan", "group", "facebook", "bandcamp", "discogs", "wiki", "festival", "scene",
  "club", "association", "society", "owners", "players", "users", "meetup", "collective"];
const slug = (s) => s.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "");
const enc = encodeURIComponent;

// where a tribe gathers → the searches that find it. Never fails (no fetch).
function huntLinks(term) {
  return [
    ["Reddit",   `https://www.reddit.com/search/?q=${enc(term)}&type=sr`],
    ["Discord",  `https://disboard.org/search?keyword=${enc(term)}`],
    ["FB groups",`https://www.facebook.com/groups/search/groups/?q=${enc(term)}`],
    ["Bandcamp", `https://bandcamp.com/tag/${slug(term)}`],
    ["forums",   `https://www.google.com/search?q=${enc(term + " (forum OR community OR discord)")}`],
  ];
}

async function autocomplete(q) {
  const url = "https://suggestqueries.google.com/complete/search"
    + `?client=firefox&hl=us&gl=us&q=${enc(q)}`;
  const r = await fetch(url, { headers: { "User-Agent": UA } });
  if (!r.ok) throw new Error(`autocomplete ${r.status} for "${q}"`);
  const j = await r.json();
  return Array.isArray(j[1]) ? j[1] : [];
}

async function cmdDiscover(rest) {
  const li = rest.indexOf("--limit");
  if (li >= 0) rest.splice(li, 2); // accepted for back-compat; ignored
  if (!rest.length) die("usage: node tools/leads.js discover <cart|term...>");

  const ledger = loadLedger();
  // seed terms: a cart → its matched tribes' tags (+ title words); else the raw terms
  let terms, source;
  const asCartFile = path.join(CARTS, `${rest[0]}.c`);
  if (rest.length === 1 && fs.existsSync(asCartFile)) {
    const meta = loadCart(rest[0]);
    const hits = matchTribes(meta, ledger);
    const tagSeeds = hits.flatMap((h) => h.matched);
    const genreWords = asList(meta.genre).join(" ").split(/\s+/).filter(Boolean);
    const titleWords = (meta.title || "").split(/\s+/).filter((w) => w.length > 2);
    // tribe tags first (best signal), then genre, then title. drop <3-char tokens (e.g. "fm").
    terms = [...new Set([...tagSeeds, ...genreWords, ...titleWords])].filter((t) => t.length >= 3);
    if (!terms.length) terms = titleWords.length ? titleWords : [rest[0]];
    source = `cart ${cyan(meta.title || rest[0])} → ${hits.length ? "tribe tags" : "title words"}`;
  } else {
    terms = [...new Set(rest.map((t) => t.toLowerCase()))];
    source = "your terms";
  }
  terms = terms.slice(0, 6); // be polite to the free endpoint

  console.log(`\n${bold("Venue hunt")}  ${dim(`(${source})`)}`);
  console.log(dim(`  terms: ${terms.join(", ")}`));

  // (2) autocomplete community signals — bonus, best-effort
  const signals = new Map(); // suggestion → seed term
  let netFailed = false;
  for (const term of terms) {
    for (const q of [term, term + " "]) {
      try {
        for (const s of await autocomplete(q)) {
          const sl = s.toLowerCase();
          if (sl === term.toLowerCase()) continue;
          if (VENUE_WORDS.some((w) => new RegExp(`\\b${w}\\b`).test(sl))) signals.set(s, term);
        }
      } catch { netFailed = true; }
    }
  }
  if (signals.size) {
    console.log(`\n  ${bold("Community signals")}  ${dim("(from Google autocomplete — a lead, click to verify)")}`);
    for (const s of signals.keys()) {
      console.log(`    ${yellow("⚑")} ${s}   ${dim(`https://www.google.com/search?q=${enc(s)}`)}`);
    }
  } else if (netFailed) {
    console.log(dim("\n  (autocomplete unreachable — the hunt links below still work)"));
  }

  // (1) the venue-hunt launcher — always printed, always works
  console.log(`\n  ${bold("Where to hunt")}  ${dim("(open these — each lists real venues to evaluate)")}`);
  for (const term of terms) {
    console.log(`\n    ${bold(term)}`);
    for (const [label, url] of huntLinks(term)) {
      console.log(`      ${cyan(label.padEnd(9))} ${url}`);
    }
  }
  console.log(`\n  ${dim("Found a keeper? Add it to the right tribe's venues[] in tools/leads-ledger.json,")}`);
  console.log(`  ${dim("then: node tools/leads.js track add <cart> \"<venue>\" --status todo\n")}`);
}

// ── draft (gift-first scaffold, offline) ──────────────────────────────────────
function firstSentences(text, n) {
  const parts = (text || "").split(/(?<=[.!?])\s+/);
  return parts.slice(0, n).join(" ").trim();
}
function cmdDraft(name, target) {
  const ledger = loadLedger();
  const meta = loadCart(name);
  const hits = matchTribes(meta, ledger);
  // pick the tribe: explicit arg (id or venue-name substring) else the top match
  let tribe = null, venue = null;
  if (target) {
    const tl = target.toLowerCase();
    tribe = ledger.tribes.find((t) => t.id === tl || t.label.toLowerCase().includes(tl));
    for (const t of ledger.tribes) {
      const v = t.venues.find((v) => v.name.toLowerCase().includes(tl));
      if (v) { tribe = t; venue = v; break; }
    }
  }
  if (!tribe) tribe = hits[0]?.tribe;
  if (!tribe) die(`no tribe matched ${name} — pass one explicitly: node tools/leads.js draft ${name} <tribeId>`);

  const desc = flattenDesc(meta.description);
  const summary = typeof meta.description === "object" && meta.description.summary
    ? meta.description.summary
    : firstSentences(desc, 2);
  const controls = typeof meta.description === "object" ? meta.description.controls : null;
  const et = (venue && venue.etiquette) || tribe.venues[0]?.etiquette || "gift-first";

  console.log(`\n${bold("Post scaffold")} — ${cyan(meta.title || name)} → ${bold(tribe.label)}`);
  if (venue) console.log(dim(`  target venue: ${venue.name}  (${venue.url})`));
  console.log(dim(`  etiquette: ${et}`));
  console.log(dim("  ── this is a SCAFFOLD: your voice fills the [brackets]; the rest is the cart's own words ──\n"));

  const L = [];
  L.push(`Title: ${tribe.hook || `[one line — what it is, for a ${tribe.label} reader]`}`);
  L.push("");
  L.push(`[Gift-first opener — you are a member here first. Show, don't sell. e.g. "Made a little`);
  L.push(` free web toy that scratches the ${tribe.label.split(" ")[0].toLowerCase()} itch — thought this crowd might enjoy it."]`);
  L.push("");
  L.push(`What it is: ${summary}`);
  if (controls) L.push(`How you play it: ${controls}`);
  L.push("");
  L.push(`▶ Play it free in the browser: [web-gallery link — build-site.js / publish-cart.sh]`);
  L.push(`📎 8-sec clip: [make-gif.js 9:16 export from tools/clips/${name}/]`);
  L.push("");
  L.push(`[Soft close — invite, don't ask. "No signup, nothing to buy — just curious what you'd change."`);
  L.push(` Match the room: ${et}]`);
  console.log(L.join("\n"));
  console.log(`\n  ${dim("materials pulled from de:meta; edit freely. Log it after posting:")}`);
  console.log(`  ${dim(`node tools/leads.js track add ${name} "${venue ? venue.name : tribe.venues[0]?.name || "venue"}" --status posted --url <link>`)}\n`);
}

// ── track (outreach ledger) ───────────────────────────────────────────────────
const STATUSES = ["todo", "posted", "responded"];
const STATUS_ICON = { todo: "○", posted: "●", responded: "★" };
function cmdTrack(rest) {
  const ledger = loadLedger();
  if (rest[0] === "add") {
    const args = rest.slice(1);
    const opt = (flag) => { const i = args.indexOf(flag); if (i < 0) return null; const v = args[i + 1]; args.splice(i, 2); return v; };
    const status = opt("--status") || "todo";
    const url = opt("--url");
    const note = opt("--note");
    const [cart, venue] = args;
    if (!cart || !venue) die('usage: node tools/leads.js track add <cart> "<venue>" [--status todo|posted|responded] [--url <link>] [--note "..."]');
    if (!STATUSES.includes(status)) die(`--status must be one of: ${STATUSES.join(", ")}`);
    // date: no Date.now() ban here (this is a plain CLI, not a workflow) — real timestamp is fine
    const date = new Date().toISOString().slice(0, 10);
    ledger.outreach.push({ cart, venue, status, date, ...(url ? { url } : {}), ...(note ? { note } : {}) });
    saveLedger(ledger);
    console.log(`${green("logged")} ${STATUS_ICON[status]} ${status}  ${bold(cart)} → ${venue}  ${dim(date)}`);
    return;
  }
  // board
  const rows = ledger.outreach || [];
  console.log(`\n${bold("Outreach board")}  ${dim(`(${rows.length} entries · tools/leads-ledger.json)`)}\n`);
  if (!rows.length) {
    console.log(dim("  Nothing logged yet. After you post somewhere:"));
    console.log(dim('    node tools/leads.js track add <cart> "<venue>" --status posted --url <link>\n'));
    return;
  }
  for (const st of STATUSES) {
    const g = rows.filter((r) => r.status === st);
    if (!g.length) continue;
    console.log(`  ${bold(STATUS_ICON[st] + " " + st)}  ${dim(`(${g.length})`)}`);
    for (const r of g) {
      const link = r.url ? "  " + cyan(r.url) : "";
      const note = r.note ? dim(`  — ${r.note}`) : "";
      console.log(`    ${dim(r.date)}  ${bold(r.cart)} → ${r.venue}${link}${note}`);
    }
    console.log("");
  }
}

// ── list ──────────────────────────────────────────────────────────────────────
function cmdList(id) {
  const ledger = loadLedger();
  const tribes = id ? ledger.tribes.filter((t) => t.id === id) : ledger.tribes;
  if (id && !tribes.length) die(`no tribe "${id}". known: ${ledger.tribes.map((t) => t.id).join(", ")}`);
  console.log("");
  for (const t of tribes) {
    console.log(`${bold(t.id)}  ${t.label}  ${dim(`[tags: ${t.tags.join(", ")}]`)}`);
    if (t.hook) console.log(`  ${green("hook →")} ${t.hook}`);
    t.venues.forEach((v) => console.log(venueLine(v)));
    console.log("");
  }
  if (!id) {
    console.log(`${bold("cross-cutting")}`);
    ledger.crosscutting.forEach((v) => console.log(venueLine(v)));
    console.log("");
  }
}

// ── --check (offline self-test) ───────────────────────────────────────────────
function cmdCheck() {
  const ledger = loadLedger();
  const errs = [];
  const ids = new Set();
  for (const t of ledger.tribes) {
    if (!t.id) errs.push("a tribe has no id");
    if (ids.has(t.id)) errs.push(`duplicate tribe id: ${t.id}`);
    ids.add(t.id);
    if (!t.label) errs.push(`${t.id}: no label`);
    if (!Array.isArray(t.tags) || !t.tags.length) errs.push(`${t.id}: no tags`);
    if (t.tags && new Set(t.tags).size !== t.tags.length) errs.push(`${t.id}: duplicate tag(s)`);
    if (!["music", "game", "any"].includes(t.domain)) errs.push(`${t.id}: domain must be music|game|any`);
    if (!t.hook) errs.push(`${t.id}: no hook`);
    if (!Array.isArray(t.venues) || !t.venues.length) errs.push(`${t.id}: no venues`);
    for (const v of t.venues || []) {
      if (!v.name) errs.push(`${t.id}: a venue has no name`);
      if (!/^https?:\/\//.test(v.url || "")) errs.push(`${t.id}: venue "${v.name}" has a bad url`);
    }
  }
  for (const v of ledger.crosscutting || []) {
    if (!/^https?:\/\//.test(v.url || "")) errs.push(`crosscutting "${v.name}" has a bad url`);
    if (!["music", "game", "any"].includes(v.domain)) errs.push(`crosscutting "${v.name}": domain must be music|game|any`);
  }
  // matching sanity: omnichord cart must reach the omnichord tribe (if both exist)
  const omniFile = path.join(CARTS, "omnichord.c");
  if (fs.existsSync(omniFile) && ids.has("omnichord")) {
    const meta = readMeta(fs.readFileSync(omniFile, "utf8"), "omnichord");
    const hits = matchTribes(meta, ledger).map((h) => h.tribe.id);
    if (!hits.includes("omnichord")) errs.push("match regression: omnichord cart no longer matches the omnichord tribe");
  }
  if (errs.length) { errs.forEach((e) => console.error(`  ✗ ${e}`)); die(`\n${errs.length} problem(s) in leads-ledger.json`); }
  console.log(green(`✓ leads ledger ok`) + dim(`  (${ledger.tribes.length} tribes, ${ledger.crosscutting.length} cross-cutting, ${ledger.outreach.length} outreach)`));
}

// ── dispatch ───────────────────────────────────────────────────────────────
async function main() {
  const [cmd, ...rest] = process.argv.slice(2);
  switch (cmd) {
    case "match": if (!rest[0]) die("usage: node tools/leads.js match <cart>"); return cmdMatch(rest[0]);
    case "discover": return cmdDiscover(rest);
    case "draft": if (!rest[0]) die("usage: node tools/leads.js draft <cart> [tribe|venue]"); return cmdDraft(rest[0], rest[1]);
    case "track": return cmdTrack(rest);
    case "audit": return cmdAudit(rest);
    case "list": return cmdList(rest[0]);
    case "--check": case "check": return cmdCheck();
    default:
      console.log(`leads.js — the local marketeer (find places to post + track outreach)

  match <cart>              tribes + venues for a cart
  discover <cart|term...>   venue-hunt links + autocomplete community signals
  draft <cart> [tribe]      a gift-first post scaffold
  track [add ...]           the outreach board / log a post
  audit [--json]            run match over EVERY cart → coverage + bucket-gaps (local, free)
  list [tribeId]            the whole ledger
  --check                   offline self-test

  ledger: tools/leads-ledger.json (hand-editable — add venues, fix links, tune tags)`);
      if (cmd) die(`\nunknown command: ${cmd}`);
  }
}
main().catch((e) => die(String(e && e.stack || e)));
