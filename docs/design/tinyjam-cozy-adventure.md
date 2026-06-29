# Tinyjam module seed: Cozy Adventure

> **Status:** PARKED concept / design seed (2026-06-29). A module idea that grew out of the
> [marketing plan](tinyjam-marketing.md) §3.10 "cozy cluster" + the Adventure-Time-crowd outreach.
> Gated on the sketch-first rule: build it *after* Strumharpy lands with the cozy crowd, not before.
> This is the design sketch, not a commitment.

## The feeling

The sound of wandering a soft pixel world at dusk: warm, twinkly, a little wistful, playful, never
threatening. It's the *quiet* end of cartoon-adventure music, the cozy-exploration-game score, the
lullaby side of chiptune. Think the world that Adventure Time's gentle moments, Steven Universe, cozy
games (A Short Hike, Stardew, Animal Crossing), and Mort Garson's Plantasia all share, **evoked, never
copied** (see the guardrail below).

Where Strumharpy is *one instrument*, Cozy Adventure is the **rack that ties the cozy cluster together**:
it bundles the omnichord shimmer, chiptune, and cozy folk into one little toy. That makes it the natural
centrepiece of the cozy meta-tribe, and a strong cross-sell with Strumharpy / lofi / plantasia.

## The band (mostly buildable from carts we already have)

| Role | Voice | Engine / cart it rides |
|---|---|---|
| **Shimmer / pad** | the omnichord auto-strum (the connective cozy colour) | the `omnichord` / Strumharpy voice, reused |
| **Lead / melody** | soft chiptune (pulse + triangle), songful not bleepy | `pd` (Casio CZ phase-distortion) / a chiptune voice |
| **Comping** | cozy nylon / ukulele warmth | `pluck` / `guitar` |
| **Bass** | a round, slightly punky little bounce (the axe-bass *archetype*, softened) | `pdbass` / `fretboard` |
| **Sparkle** | glassy bells / celesta / music box | `mallet` |
| **Percussion** | soft, toy-like, brushed or tiny chiptune kit, gentle never banging | existing kits, low-energy |

Engine cost is **low**: it's a recombination of shipped engines + voices, like `italo` was. The real
work is the *brain* and the *face*, not new DSP.

## The soul (the generative brain)

- **THE SONGWRITER** (plantasia's melody brain #3, the seeded theme-and-variation hook, A-A'-B-A) doing
  gentle, *wandering* tunes, the melody is the protagonist.
- **Bittersweet major**: bright tonality with wistful passing chords, the cozy-but-a-little-sad core.
  Always resolves softly; never harsh.
- **A wandering / exploration form** that meanders and breathes rather than building to a drop.
- **One feel knob**: dawn → dusk, or playful → wistful, biasing tempo, register, and chord colour.

## The face

An **original** little pixel character that reacts to the music (the Game-&-Watch dancer from
[tinyjam-racks-followup](tinyjam-racks-followup.md)): a small wanderer with a backpack, or a round companion
creature, exploring a soft scene as the song plays. Cute, ours, no resemblance to any existing show
character.

## IP guardrail (non-negotiable, see [marketing §2](tinyjam-marketing.md))

Evoke the *feeling*, never the franchise. **No** BMO / Marceline / Finn / Jake / the axe-bass / any show
art, name, or character. Original name, original character, original faceplate. Vibe comparisons in copy
are fine as plain description ("cozy cartoon-adventure music"); trade dress and characters are not. The
ReBirth-deleted-by-Roland tale is the reminder of why this matters even for a beloved, free thing.

## Name (decision, later)

Original + cute (à la Strumharpy). Starters to choose from: **Wanderchime**, **Driftsong**, **Campglow**,
**Lullabox**, **Pocket Meadow**. Pick when it graduates from parked.

## Tribe & channels (when it ships)

Reach the cozy meta-tribe by aesthetic, not by any franchise:

- **Chiptune** — r/chiptunes, chipmusic.org / Battle of the Bits
- **Cozy games** — cozy-games communities (Animal Crossing / Stardew-adjacent)
- **VGM** — video-game-music fans
- **Ukulele** — ukulele communities
- **Adventure-Time-adjacent** — via the vibe only, gift-tone, in promo-tolerant corners (marketing §3.10)

## Why park it (not build it now)

Sketch-first ([marketing §5](tinyjam-marketing.md)): prove the cozy crowd shows up for **Strumharpy**
first (cheap, one instrument, the OM-108 wave). If they do, Cozy Adventure is the obvious second swing,
and a richer one, because it's a whole rack. Building it before that evidence is putting the rack before
the proof.
