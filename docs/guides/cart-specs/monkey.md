# The game you're building: MONKEY ISLAND

## Premise
A bite-size **SCUMM point-and-click** in the spirit of *The Secret of Monkey Island*.
You're a would-be pirate on the docks of Mêlée Island. Pick a **verb** (LOOK / TALK /
USE / TAKE), click a **hotspot**, walk over, act. Talk your way through a **branching
dialogue tree** with the Sword Master to earn a duel — then win the signature
**insult sword-fighting** minigame: every parry is a war of words, not steel. Trade
insults, pick the **right comeback**, and the one who runs out of retorts loses.

This is a single-screen scene + a conversation + a duel — NOT a free-roam world. Depth
in three tight systems: scene/verb/inventory, dialogue branches, insult combat.

## The locked slice (build exactly this — no more)
1. **One SCUMM scene** — the dock at Mêlée Island. A walkable floor, a verb bar, an
   inventory strip, hover labels ("Look at sign"), a message line. Reuse the `zak.c`
   scene engine wholesale: hotspots-as-structs, verb resolution per scene, walk-to-then-
   act, held-item-used-on-hotspot.
2. **A branching dialogue tree** — talk to the **Sword Master**. A small node graph:
   each node shows 2–4 numbered choices; a choice prints her reply and jumps to another
   node (or ends). One branch (challenging her / proving you've trained) **unlocks the
   duel**. Reuse the `papers.c` panel idiom for the conversation box.
3. **The insult sword-fight** (the signature, reusing `insult.c`'s word-trade idea) —
   a turn-based duel: the opponent **hurls an insult**, you're shown a hand of possible
   **retorts**, exactly one of which is the correct comeback to *that* insult. Pick it →
   you parry and press the attack; pick wrong → you eat the hit. Then **you** insult and
   the opponent must answer (it gets it wrong sometimes, so you can win). First to land
   N successful exchanges wins the duel. A clean **win / lose / restart** loop.

## Engine features it leans into (and why)
- **Mouse-first verb UI** (`mouse_pressed`, `point_in_box`) — the SCUMM grammar IS
  click-verb-then-click-thing. Keyboard digit keys (`keyp('1')`) pick dialogue/retort
  lines as a fallback.
- **Procedural pixel actors** (no sprite sheet) — like `zak.c`, draw the pirate, the
  Sword Master, and the dock from primitives (`rectfill`/`circfill`/`oval`/`tri`). Keeps
  the cart self-contained and lets the duel poses (lunge / parry / hit) animate with
  simple offset math + `anim`.
- **The classic insult-pair table** — the heart of Monkey Island: a `const` array of
  `{insult, correct_retort}` pairs. The "hand" of retorts shown is the correct one plus
  decoys drawn from other pairs — pure array indexing, the `insult.c` combinatorics
  move applied to a matching game.
- **Sound where it earns it** — a custom pluck for menu clicks, a bright `strum`
  (CHORD_MAJ7) sting on a landed parry, a buzzy `INSTR_NOISE`/low square "clang" on a
  miss, a `shake` + red `fade` flash when you take a hit. The duel breathes through `dt()`-
  paced beat timing between turns.
- **Juice at the moments that earn it** — `shake` on hits, blink on the active prompt,
  a fade-in win/lose card. No juice spam elsewhere; the dock is calm.

## States
- **DOCK** (default): the scene + verb/inventory bar + message line.
- **TALK**: dialogue overlay over the dock (numbered choices).
- **DUEL**: the insult fight — full screen, two fighters, the insult line, the retort
  hand, a score-of-exchanges pip row for each side.
- **OVER**: win ("You fight like a dairy farmer... but you WON") / lose card → click to
  restart the duel.

## Controls
Mouse-primary: click a verb, then a hotspot, then walk + act; click the floor to walk;
click an inventory item to hold it then click a hotspot to use it. In a conversation,
click (or press 1–4 for) a dialogue line. In the duel, **click the right retort/insult
line** (or press its number). SPACE/ESC dismisses a message; click to restart on the
end card.

## Lean into / read
`zak.c` (scene/verb/inventory/walk-to-act engine — reuse it), `insult.c` (the word-trade
core — reuse the matching idea), `papers.c` (panel/dialogue UI). Skip: tile maps (one
hand-drawn screen is right here), free-roam, combat physics.
