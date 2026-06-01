# The game you're building: DECKBUILDER

## Premise
A Slay-the-Spire-grain single-run deckbuilder boiled down to its irreducible core:
**one tiny dungeon, three nodes, three fights.** You hold a small hand of cards, spend
**energy** to play them, and survive turn-based duels against an enemy whose **INTENT**
is telegraphed every turn (a sword icon + a number = "this is how hard it hits"). Win a
fight, **pick one of three reward cards** to fatten your deck, walk to the next node on
a three-step map, and finish on a **boss**. That's the whole arc — no relics, no
potions, no sprawling card pool. Eight starter cards is plenty.

## The core loop (one combat = several turns)
1. At turn start you refill to **3 energy** and **draw up to 5 cards** from your deck
   (reshuffle the discard when the draw pile empties).
2. The enemy rolls its **intent** for the turn — ATTACK (shows damage) or BUFF/BLOCK —
   shown as an icon above its sprite so you can plan around it.
3. **Play cards by clicking** them: attacks deal damage to the enemy; skills grant
   **block** (a shield that soaks the next incoming hit, then evaporates). Each card
   costs energy; you can keep playing until energy runs out.
4. Click **End Turn**: leftover cards discard, the enemy resolves its intent (your block
   absorbs the hit first, overflow chips your HP), then a new turn begins.
5. Drop the enemy to 0 HP → **victory** → a **3-card reward** screen → advance the map.
6. Lose all your HP → **defeat** → restart the run. Beat the boss → **run cleared**.

## States
- **MAP** — three nodes on a path (fight, fight, boss); click the next reachable node.
- **COMBAT** — the duel: enemy + intent up top, your HP/block/energy + hand at the bottom.
- **REWARD** — three face-up cards; click one to add it to your deck (or Skip).
- **WIN / LOSE** — short banner + click to continue / restart.

## The engine features it leans into (and why)
- **Full `mouse_*` API** — this is a click game. `mouse_pressed` to play a card, click
  End Turn, click a map node, click a reward. `point_in_box` for every hit test. Card
  hover lifts the card (`lerp` toward a raised y) for tactile feedback.
- **Primitives, no sprite sheet** — cards, the HP/energy/block pips, the enemy, and the
  intent icon are all `rectfill`/`circfill`/`tri`/`print`. A card game is cleaner drawn
  from shapes than from a 16×16 sheet, and it keeps the slot budget at zero.
- **Juice at the earned moments** — `shake` + a red `pal` flash when the enemy lands a
  hit; a damage number floats up (`lerp`) when you strike; block shimmers; the energy
  orb pulses. `fade` dims the field on win/lose.
- **Synth SFX that match the action** — a percussive pluck instrument for card play,
  a noisy thud for hits, a rising `strum` on victory, a low buzz on defeat.
- **`save()`** for a best-run counter (deepest node reached / runs won).
- **`rnd`** drives the deal/shuffle and the enemy intent table; **`dt()`** for all the
  card-lift and float-number animation so feel is framerate-independent.

## Data model (suggested)
- A `Card` is just `{ int type; }` indexing a small `const` table of `{name, cost, kind,
  value}` where kind = ATTACK | SKILL. Deck/hand/draw/discard are fixed `int` arrays of
  card-type indices plus counts (no heap).
- Player: `hp, maxhp, block, energy`. Enemy: `hp, maxhp, block, intent_kind,
  intent_value`.
- Map: `node` 0..2, each with an enemy stat-block (last is the boss).

## Controls
Mouse-primary: **click a card** in your hand to play it on the enemy; **click End Turn**
to pass; **click a reward card** to take it (or Skip); **click the next map node** to
advance. Keyboard fallback: number keys 1–5 play hand slots, ENTER ends the turn / 
confirms, SPACE continues banners.
