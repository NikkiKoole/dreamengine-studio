# Leads — the local marketeer (find venues to post + track outreach)

STATUS: BUILDING (2026-07-07) — the tool + ledger are built and working; the tribe
taxonomy is being filled cart-by-cart, and the editor Apps-page surface is not started.
The demand-GENERATION twin of the `aso-*` demand-CAPTURE tools. Sits under
[`demand-generation.md`](demand-generation.md) lever #3 ("showing up in the tribe").

> **One-line pitch.** Given a cart, tell me the *tribe(s)* it belongs to and *where they
> already gather*, help me *discover* new venues, *draft* a gift-first post from the cart's
> own words, and *track* where I've posted. All local; no accounts.

Related: [`demand-generation.md`](demand-generation.md) (the lever hierarchy),
[`store-agents.md`](store-agents.md) (the capture/ASO twin — this is the generation side),
[`sharing-channels.md`](sharing-channels.md) (the channel map),
[`../marketing/tinyjam/tinyjam-marketing.md`](../marketing/tinyjam/tinyjam-marketing.md) §3.9
(the hand-verified channel directory the ledger was seeded from).

## The model: buckets

The whole thing is **buckets** (we call them *tribes*). A tribe is:

- **tags** — the words that identify who they are (`solina`, `303`, `tr-606`, `omnichord`…),
- **venues** — where that bucket actually hangs out (subreddits, forums, FB groups, and
  genre-specific *creators* like a beatmaking YouTube/TikTok channel),
- **a hook** — the one sentence that makes them care.

**A cart drops into a bucket automatically:** the tool reads the cart's `de:meta` (title,
lineage, homage, teaches, description → one lowercased blob) and any tribe whose tags appear
matches, ranked by how many tags hit. A cart can land in several buckets (the acid rack →
acid + drum-machine + modular).

Two layers sit **above** the buckets:

- **Domain** (`music` / `game` / `any`) — a coarse pre-filter from `de:meta` (instrument/radio
  kind → music; a `genre` or kind `game` → game) so a game never gets shown music-press venues
  and vice-versa.
- **Cross-cutting** — amplifiers that apply to a *whole domain* (every music launch also hits
  Loopy Pro / Sound Test Room; every game hits r/WebGames / itch.io).

## The tool — `tools/leads.js`

Full contract in the file header. Commands:

| command | what | network |
|---|---|---|
| `match <cart>` | tribes + venues for a cart, ranked | offline |
| `discover <cart\|term…>` | venue-hunt links (Reddit/Discord/FB/Bandcamp/forums search urls) + Google-autocomplete community signals | Google autocomplete (free, no auth) |
| `draft <cart> [tribe]` | a gift-first post **scaffold** — the cart's own description + the tribe hook + venue etiquette, with `[bracketed]` voice slots | offline |
| `track [add …]` | the outreach board (`○ todo / ● posted / ★ responded`); `track add` appends to the ledger | offline |
| `audit [--json]` | run `match` over EVERY cart → coverage % + the list of carts in no tribe (the gaps) + bucket cart-counts | offline |
| `list [tribeId]` | the whole ledger | offline |
| `--check` | offline self-test / CI gate | offline |

**Design rules honoured:**
- **Scripts prepare → human decides → prose is yours** (the `store-agents.md` rule): `draft`
  never invents marketing copy — it arranges the cart's *own* `de:meta` text + the tribe hook
  and leaves `[your voice here]` slots. No AI-generated posts.
- **Free public data only** (same stance as `aso-suggest.js`): Google Autocomplete. **Reddit's
  free JSON API is dead** (403 "Blocked" since the 2023 API lockdown) — that's why `discover`
  is a venue-*hunt launcher* (ready search urls that always work) rather than an auto-crawler.
- **`audit` is pure local JS** — running it over the whole catalogue costs zero model tokens;
  it's the cheap way to "run through all carts" and see every remaining bucket-gap at once.

## The ledger — `tools/leads-ledger.json`

Committed, hand-editable, the **source of truth** for tribes/venues/hooks/domains + the
outreach log. Seeded from tinyjam-marketing §3.9. Schema (per tribe): `id`, `domain`,
`label`, `tags[]`, `hook`, `venues[]` (each `{name, url, kind, etiquette, domain}`;
`kind` ∈ reddit/facebook/forum/youtube/web/discord/tiktok). Plus top-level `crosscutting[]`
(same venue shape) and `outreach[]`. `--check` validates it.

**The curation loop:** run a cart → it either lands cleanly (validates the buckets) or exposes
a hole → add the missing tribe/venue and re-run. Buckets are added **on demand** (only when a
real cart needs one) — no speculative tribes rotting in the ledger.

## What's done (2026-07-07)

- Tool with all 7 commands; ledger with **18 tribes** (15 music + `arcade` + `drum-machine` +
  `string-machine`) and **9 cross-cutting** (4 music / 5 game).
- Matching hardened by dry runs (see log below).
- Discoverable: CLAUDE.md tools list, `demand-generation.md` lever #3, tinyjam §3.9 backlink.

### Dry-run findings log (each earned a fix)

- **`tr606`** → no drum tribe existed → **added `drum-machine`** (matches 606/808/909/cr78 via
  machine names + `drum synthesis`). Also dropped `subtractive-synth` from tr606's teaches.
- **`epiano`** → matched the FM/DX7 tribe off one incidental "the fm cart" mention → **2-char
  tags (`fm`) now need ≥2 hits**; and `discover` drops <3-char search seeds.
- **`highnoon`** (a game) → crashed on `genre` being a **string** not an array (fixed via
  `asList`), and got shown music venues → **added the `domain` model + an `arcade` tribe + a
  game cross-cutting set** (r/WebGames, itch.io, TIGSource, r/playmygame, r/IndieGaming).
- **`more-note-bass`** → the maker wanted a beatmaking creator surfaced → **added BeatCraft
  Studio (YouTube) + Aaron Batzdorff (TikTok)** as *genre-specific amplifier* venues inside the
  drum-machine tribe (creators live in the tribe, not global cross-cutting).
- **`solina`** → no tribe → **added `string-machine`** (Solina / vintage string-ensemble cult).
- **`grenadier`** → proved the modular tribe is correct when it should be, AND that dropping the
  bare `subtractive` tag would touch zero genuine modular carts (they also match `vco`/`modular`).

## Open questions / resume-at

1. **The `subtractive` over-match** (RECOMMENDED next, agreed-safe by the grenadier run):
   drop the bare `subtractive` tag from the `moog` tribe (keep `moog/minimoog/ladder
   filter/vco/modular/eurorack/steiner-parker`). Kills the false positive on
   tr808/909/cr78/more-note-bass; genuine modular carts still match via other tags. Also check
   `moog` (31 carts) and `arcade` (40) aren't otherwise over-broad. Then re-`audit`.
2. **Fill the music bucket-gaps** the `audit` surfaced (music coverage 62%): a **genre-radio**
   family (ambient/bossa/eno/gamelan/satie/tango/city-pop radio), an **acoustic/world**
   bucket (guitar/brass/reed/pipe/bowed/hurdy-gurdy/glass-harmonica/monochord), a **novelty**
   bucket (otamatone/touch-piano). Some (`fxmod`/`lfoshapes`/`reverb spaces`/`say`) are engine
   tech-demos — probably no tribe at all.
3. **Games** (coverage 24%, only `arcade` so far) — clusters into board/card/sim/puzzle/
   adventure. **Decision pending:** per the GTM games are web-gallery-only, so game buckets may
   be low value — do we invest? (asked, not yet answered.)
4. **The editor Apps-page surface** — the maker's framing: "another type of search thing that
   can work freely or bound to carts/apps." A card that shows a cart/app's buckets + venues +
   a draftable post, plus a free-form `discover` box. Mirrors how `aso-score` got a 📊 glance.
   Not started.

## Where it lives / next session

- Tool: `tools/leads.js` · ledger: `tools/leads-ledger.json` (hand-edit to add venues).
- Orient fast: `node tools/leads.js audit` (free, whole-catalogue coverage + gaps).
- Do (1) above first (one tag edit + `--check`), then (2), then decide (3), then build (4).
