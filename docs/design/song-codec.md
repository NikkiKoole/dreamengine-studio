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

## The decision: a seed envelope now, the same envelope grows later

Build **one shared helper** that all radios use immediately:

```
magic(1)  + version(1) + station_id(2) + seed(u32)   →  ~16 hex chars
```

- `magic` — cheap "is this even ours" check.
- `version` — bump when the payload layout changes; an old decoder refuses a newer version
  gracefully, a new decoder still reads old codes.
- `station_id` — which generator the seed belongs to (stops a house seed loading into the bossa
  rack and producing garbage).
- `seed` — the `u32`.

**Why spend the `version` + `station_id` bytes when seed-only doesn't strictly need them:** they're
the hinge that lets the fuller format slot in **without a flag-day** — a future v3 decoder still
resolves a v1 seed link. Omitting them is the *only* choice here that would actually paint us into a
corner. This is the opposite of over-engineering: it's the minimum that lets us grow.

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

The ~16-hex seed envelope as a small shared helper (encode/decode) + the radio-dial "show code /
type code" affordance, so all 34 radios get sharing at once. Revisit the blob when the rack lands.
