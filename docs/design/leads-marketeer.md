# Leads — the local marketeer (find venues to post + track outreach)

STATUS: BUILDING (2026-07-07) — the tool + ledger are built and working; the MUSIC tribe
taxonomy is substantially filled (32 tribes, 90% music coverage — remaining gaps are engine
tech-demos that get no tribe by design). Open: the games-buckets decision + the editor Apps-page surface.
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

### Model refinement (2026-07-07): a tribe is the SCENE, not the FORMAT

The genre-radio carts forced a sharpening of what a "tribe" is. The mistake was to reach for a
single **`genre-radio`** bucket — but "radio" is the *format* (the cart shape), not a room anybody
gathers in. The homages show the real rooms: `jingle` **is** Mac DeMarco, `eno` **is** Brian Eno,
`afrobeat` **is** Fela Kuti / Tony Allen, `house` **is** Daft Punk / French house, `citypop` **is**
Tatsuro Yamashita. So:

- **The tribe is the scene / fandom** — the actual place those people hang out (r/macdemarco,
  r/ambientmusic, r/CityPop, r/DaftPunk). A tribe **earns its slot only if it has a real, reachable
  room.** A one-cart artist-tribe is fine (only `jingle` is Mac DeMarco) — the whole value is
  gift-posting into *that specific room*; it still obeys "add on demand, no speculative tribes."
- **The format is a cross-cutting-style amplifier, not a scene.** Every radio cart is *generative*,
  so `generative` (r/generative, lines/llllllll, Disquiet Junto) is its own **format tribe** that
  amplifies the whole radio family, while the ambient *scene* tribe (Brian Eno / drone) stays
  reserved for the carts that are actually ambient. A cart like `citypop` correctly lands in
  **city-pop scene + generative format** — not lumped into "ambient" just because it's generative.
- **Guard the generic-adjective tag.** A bare word that describes *technique* rather than *identity*
  over-matches exactly like `subtractive` did on `moog`: `drone` pulled in 19 pad-synth carts, and
  `generative` bleeds across domains (generative *art* vs *music*). Tag on the identity word
  (`brian eno`, `mac demarco`, `gamelan`), not the adjective.

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

- Tool with all 7 commands; ledger with **32 tribes** (the 2026-07-07 scene/format pass added
  ambient/citypop/afrobeat/frenchhouse/indie-jangle/microtonal + the cross-domain `generative`
  tribe, `piano`/`vintage-poly` for the misfiles, physical-modeling/guitar/world-folk for the
  acoustic cluster, then novelty-toy/vocal-synth) and **9 cross-cutting** (4 music / 5 game).
  Music coverage 62% → **90%**.
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

1. **The `subtractive` over-match** — ✅ **DONE (2026-07-07).** Dropped BOTH `subtractive` and
   `subtractive-synth` from the `moog` tribe (keeping only the moog/modular *identity* words:
   `moog/minimoog/ladder filter/vco/modular/eurorack/steiner-parker`) — dropping the bare tag
   alone wouldn't have killed the tr808/909/cr78/more-note-bass false positives, since they also
   matched via `subtractive-synth`. `moog` went 31 → 14 carts; the 17 that left are all drum/acid/
   misc-synth carts that belong to `drum-machine`/`acid` (verified: tr808→drum+acid, more-note-bass→
   drum+acid). Genuine modular carts kept via other tags (moog, filterenv via moog/ladder-filter;
   sh101, grenadier via vco/modular). `arcade` (40) was checked and is fine — 32 bare-`arcade`
   matches are genuine arcade-cabinet titles; only `poker` is a stray (not worth a fix). `--check`
   green.
2. **Fill the music bucket-gaps** (music coverage 62% → **73%**). The radio family is done, but
   done as **scene tribes, not one "genre-radio" bucket** (see the model refinement above). Added
   2026-07-07: `ambient` (Brian Eno / drone), `citypop` (city-pop / vaporwave), `afrobeat` (Fela),
   `frenchhouse` (Daft Punk / French touch), `indie-jangle` (Mac DeMarco), `microtonal` (gamelan /
   xenharmonic), and the `generative` **format** tribe (r/generative · lines · Disquiet Junto) that
   amplifies every radio cart. Venue URLs are in the ledger but flagged **VERIFY** where I couldn't
   confirm the exact sub (Reddit blocks fetch — browser-check before posting, that's the workflow).
   **Still open:**
   - **Three judgment-call scenes with weaker/broader rooms** — `satie` (Erik Satie → r/classicalmusic
     / r/piano?), `bossa` (bossa nova → r/Jazz / r/bossanova?), `mariachi`, `tango` (dance-heavy
     r/tango?). One-cart each, rooms I can't confidently name. Mint or leave on the generative
     amplifier? (maker call.)
   - **The `generative` domain** — ✅ **RESOLVED (2026-07-07): domain `any`** (maker: "generative is
     both music and art"). r/generative + lines welcome generative *art*, so it's a cross-domain maker
     community (Disquiet Junto flagged music-leaning in its etiquette). This also gives the worldgen
     carts (`roadnet`/`wfc`/`citygrow`) a real venue, while real games (highnoon/dinorun) don't match
     — they never claim "generative".
   - **The misfiles** — ✅ **DONE (2026-07-07).** On a closer look neither fit an existing tribe, so
     each got its own small real room: **`piano`** → new `piano` tribe (physical-modeled acoustic
     keys — grand/harpsichord/celesta; venues Pianoteq/Modartt forum · r/piano · r/synthesizers), NOT
     the electric-piano `keys` scene. **`juno-6`** → new `vintage-poly` tribe (Juno/Jupiter/Prophet
     analog poly + chorus; Vintage Synth Explorer · r/synthesizers), NOT moog/modular. Tags kept
     identity-specific (`grand piano`, `juno-6`) to avoid the preset-name/incidental-mention noise.
   - **Acoustic/world** — ✅ **DONE (2026-07-07).** Not one bucket: split into `physical-modeling`
     (the waveguide/Karplus-Strong/STK showcases — guitar/bowed/brass/reed/pipe/pluck/monochord +
     piano; the real synth-nerd/Mutable-Rings scene; r/synthesizers · KVR · Modartt · r/modular),
     `guitar` (fretboard + pedalboard → r/guitar · r/acousticguitar), and `world-folk` (hurdy-gurdy/
     mouth-harp/glass-harmonica/dan-bau/handpan → r/WorldMusic · r/folk). Coverage 74% → **84%**.
     Radio carts (afrobeat/mariachi) bleed lightly into physical-modeling because they *name* their
     waveguide voices — accepted (correct primary homes elsewhere). `pan` (stereo-panning demo) and
     `cathedral` (reverb demo) correctly stay tech-demos, no tribe.
   - **Novelty/toy + vocal-synth** — ✅ **DONE (2026-07-07).** `novelty-toy` (otamatone/otafamily/
     stylophone/le petomane/mario-paint → r/synthesizers + short-form video, which is the *real* reach
     for novelty instruments) and `vocal-synth` (vox/voxab/voxlab/voxpad/say/vowel — the INSTR_VOICE
     formant engine → r/Vocaloid + r/synthesizers). Tagged on identity (`formant voice`/`phoneme`/
     `simlish`, not bare `vowel`/`vocoder` which catch filter/effect mentions). Music coverage **90%**.
   - **What's left is correctly un-tribed:** engine tech-demos (`fxmod`/`lfoshapes`/`reverb spaces`/
     `cathedral`/`pan`/`varispeed`/`ampnoise`/`enginelab`/`tracker`/`univibe`) get no tribe by design;
     `touchpiano` (near-empty `de:meta`) and `mallet` (modal/percussion) are the only real one-offs
     still homeless — low priority, both fixable with a de:meta touch-up or a tiny tribe later.
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
