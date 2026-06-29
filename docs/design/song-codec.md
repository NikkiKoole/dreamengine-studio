# Song codec — sharing a Tinyjam song as a compact, growable file

STATUS: IDEA / exploration (2026-06-29). Nothing built yet. Captures a design conversation about
how a cart serialises a song for share/save. The operative decision so far: **ship the seed
envelope; defer everything else until a rack lets users author.** Companion reading:
[`tinyjam-racks.md`](tinyjam-racks.md) (§"The seed is a song code", §"Export — three tiers"),
[`product-notes-followup.md`](product-notes-followup.md) §1 (the URL-codec / static-host reality),
[`action-plan.md`](action-plan.md) (Tier 0 "Save/share codec"), and `runtime/radio.h`
(the seed-compatibility rule this all rests on).

## The question

The web gallery has no save/share. Tinyjam racks will need a "song code" handoff. Both want the
same machinery — so: **how do we define the file?** We don't want to over-engineer a universal
format before anything can produce one, but we also don't want a v1 that paints us into a corner.

## The key realisation: there are *layers*, not one format

A song can be represented at three fidelities, each a valid export at a different
portability/size trade-off. They nest: **seed → (run generator) → lanes → (perform) → events.**

| Layer | What it is | Size | Who can play it | Status today |
|-------|-----------|------|-----------------|--------------|
| **Seed** | a `u32` — "re-run *this* generator" | ~16 hex chars | only the cart that owns that station's `new_song()` | **every radio has one, free** |
| **Lanes** | editable pattern state (triggers, pitched lanes, chord row, per-lane voice) | <1 KB | any cart with the matching genre schema + player | the rack's new artifact — doesn't exist yet |
| **Events** | flattened note/time/voice stream (post-generator "performance") | bigger | *any* cart with a generic player (`song.h`), MIDI export | not built |

The seed is the most fantasy-console (tiny, typeable, pasteable in Discord) but the **least
portable** — it's a *pointer*, not a *document*. The event stream is the most portable but the
**biggest** and has no consumer yet.

## Why the seed is enough *now* — and exactly when it stops being enough

`radio.h` makes every station a seeded `xorshift32`: **same seed = same tune, forever** (the
seed-compatibility rule — a composition *is* its sequence of `rad_srnd()` calls). So a seed-code
file is **already universal across all 34 radio carts** with zero new engine work.

A seed only describes **pure generator output.** The instant a user toggles a cell, nudges a voice,
or draws a pattern, the state is no longer reproducible from any seed — the seed becomes a pointer
to something that no longer exists. **A seed can never represent *authored* content.**

The happy part: **that boundary lands exactly where the rack does.** The radios have no edit
affordance today, so a seed is genuinely *complete* for them — nothing is lost. The moment a rack
ships editing, we simultaneously (a) need the full-state blob and (b) finally have a producer/
consumer for it. We never need the blob one day before the rack. The timing is a seam, not a
compromise.

## What already ships today (the installed base)

Every one of the 34 stations already renders its seed on the dial as a **bare 8-hex `u32`**
(`snprintf(l2, …, "%d bpm #%08X", tempo, sng.seed)`), with `R` to replay and `[ ]` to walk session
history. So **"show the code" is done** — and the format already in the wild is a *bare `u32`*, no
envelope. Two consequences:

1. The only *missing* half of seed-sharing is **type-a-code-in** — a way to enter a friend's
   `#A3F90C12` and have that exact song land. (Today you can only replay codes from your own
   session.)
2. Any envelope we add later must **not** invalidate the bare 8-hex codes already shown/noted
   (e.g. pinned `HOUSE_SEED`). The migration is therefore *length-distinguished*, not a re-encode
   (see below) — which means we should **not** retrofit an envelope onto the seed tier now.

## The decision: bare seed now (already shipped), envelope only when the format grows

Given the installed base, the pragmatic call is the opposite of retrofitting bytes: **keep the bare
`u32` for the seed tier** (it's already out there and it's the most typeable thing possible — 8
hex), and **add the envelope only when there's a second payload type to disambiguate** (the rack's
edited blob). The decoder distinguishes by length / sigil, so old codes stay valid forever:

```
8 hex            →  legacy bare seed, for the station you typed it into   (v0, today)
prefix + longer  →  enveloped: magic + version + station_id + flags + payload   (when the rack lands)
```

The envelope, when it arrives:

- `magic` — cheap "is this even ours" check.
- `version` — bump when the payload layout changes; old decoder refuses a newer version gracefully.
- `station_id` — which generator/schema (stops a house code loading into the bossa rack).
- `flags` — `seed-vs-blob` bit + room for `has-banks`, etc.
- `payload` — a `u32` seed, or the lane blob.

**Why length-distinguished beats retrofitting:** the bare seed is *already* the wild format, so the
cheapest forward-compatible move is to leave it alone and make the new, longer code self-evidently
different. Adding `magic`/`version` bytes to the seed *now* would itself be the flag-day it was
meant to avoid — it'd orphan every 8-hex code already noted. (This reverses the earlier instinct to
"spend the bytes now"; learning that show-code already ships as a bare `u32` is what flipped it —
grow as we go.)

### How it grows (do NOT build yet)

When the rack ships editing, the *same* envelope grows a `flags` byte with a `seed-vs-blob` bit, one
decode entry point:

- **seed bit set** → 4-byte seed (the compact path stays, for unedited songs).
- **seed bit clear** → lane blob in the payload: serialise → compress (lz-string) → URL-safe Base64
  → `…github.io/<cart>/?song=…`.

Later still, an event/`song.h` tier for cross-cart soundtracks and a MIDI `tools/` converter — both
are *projections* of the lane blob, never parallel formats. (And a possible middle option: seed + a
tiny diff of edits-on-top, to keep codes short for lightly-touched songs — only if blobs feel too
big in practice. Lane state is <1 KB, so probably never needed.)

## Principles to hold onto

- **One canonical artifact, two skins.** The cart owns the *payload* (it knows its lanes); the
  engine owns *transport* (compress/Base64/URL) — exactly like `save_bytes()` knows nothing about
  its bytes. `song.h` / JSON-diff / MIDI are *views* of the canonical blob, never new formats.
- **Byte-stable packing.** A seed must reproduce identically on arm64 / x86 / wasm (see
  `det-probes/`). Explicit little-endian, fixed-width fields, **no `struct` padding** — pack field
  by field, never `memcpy` a struct. Same discipline `radio.h` already demands.
- **Namespace web storage per cart.** `localStorage` / IndexedDB is per-*origin*; keys must be
  `tinyjam:<cart>:…` or carts clobber each other on the one gallery domain.
- **Grow as we go.** Seed now (free, all radios). Blob exactly when authoring arrives. Event/`song.h`
  and MIDI only when a real consumer exists. Let multiple users elaborate before universalising.

## Open questions (for when the blob tier earns its way in)

- **Voice recipes: embed vs reference.** A generic player needs to know what "voice 7" is — either
  embed the recipe (self-contained, bigger) or reference a versioned engine recipe-table id
  (compact, but the table must stay stable). The id route is more in-keeping (carts already "copy
  the closest relative" recipe); embedding helps true external portability. Tier it.
- **Is there ONE universal lane schema** (variable lane count + per-lane type/voice + chord row +
  tempo/swing) that all genres instantiate, so every rack shares one format and one player? Likely
  yes (it's basically a tracker model) — but decide *after* a couple of racks exist, not before.

## First move (when greenlit)

Show-code already ships (bare 8-hex `u32`, all 34 stations). The one missing half is **type-a-code-in**:
a small shared affordance in `radio.h` (since the seed/history plumbing already lives there) that
lets the player enter 8 hex chars and calls `new_song(pos, typed_seed)` — the same path `R`/`[`/`]`
already use. Build it once in the shared chassis, prototype on one station (house), then it's live
everywhere. No envelope, no new format — just close the loop on the codes already on screen. Revisit
the envelope + blob when the rack lands.
