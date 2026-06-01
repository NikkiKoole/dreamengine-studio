# The game you're building: STREET FIGHTER

## Premise
A **1-on-1 versus fighter** — two stick-fighters facing off on a single screen, health
bars up top, a round clock between them, **best-of-3**. You're P1 against a reactive AI;
spacing, timing and one well-timed fireball decide the match. This is the *duel*, NOT a
side-scrolling brawler — there's no walking to the right, no waves of goons. One screen,
two fighters, the distance between them is the whole game.

## The core loop (one round)
1. **ROUND n / FIGHT!** intro, then the clock starts ticking down.
2. Both fighters maneuver: walk in/out, jump, crouch, **block** (hold away), and throw a
   **light** attack, a **heavy** attack, or a **special** (fireball).
3. An attack hits → **hit-stun** + knockback + a spark + screen `shake`; on **block** it's
   **chip damage** + short blockstun. Hit-stop (a couple frozen frames) sells the impact.
4. Round ends on a **K.O.** (someone hits 0 HP) or **TIME UP** (higher HP wins). First to
   **2 round wins** takes the match.
5. **MATCH END** → press Z to rematch. Best win-streak `save()`d.

## The fighter as a state machine (the heart of it)
Each fighter is a tiny FSM: idle / walk / jump / crouch / **block** / **attack** /
**hitstun** / **blockstun**. Every attack runs **startup → active → recovery** — only the
*active* window has a live hitbox — so whiffing leaves you punishable and spacing matters.
Three attacks only (the locked cut): **light** (fast, short, low damage), **heavy** (slow,
long reach, big damage + knockback), and the **special fireball** — thrown with a
**quarter-circle-forward** motion (down → down-toward → toward, then light). An **input
buffer** of recent directions recognises the QCF, the trick behind every fireball in every
fighter.

## The opponent — reactive AI (the new piece vs. the exemplar)
P2 is a simple **state-driven AI**, not a second human. It reads the gap to P1 and picks an
intent each "think" tick: close the distance when far, **block** when P1 is attacking in
range, **whiff-punish** with a poke when P1 recovers, jump-in occasionally, and throw a
fireball at range. Reaction is deliberately *human-flawed* (a think delay + randomness) so
it's beatable but pushy. Difficulty rises a touch each round you win.

## Engine features it leans into (and why)
- **Primitives, no sprite sheet.** Fighters are drawn from `line`/`circfill`/`rectfill`
  stick-figures whose limbs *pose per attack frame* — cheaper and more readable than
  16×16 sprites for a 200px-tall fighter, and the pose IS the animation/telegraph.
- **`shake` + hit-stop + `pal` flash** at the contact frame — the juice that earns its
  keep is exactly on impacts (juice.c idiom).
- **`fade`** for the round-end dim, **`blink`** for the FIGHT! flash and low-HP bar.
- **Synth SFX** shaped per event: noise hits for punches, a saw whoosh for the fireball,
  a tri sting on K.O. — `note`/`hit`, not spam.
- **`bar`** isn't ideal here (it fills left-to-right only), so P2's mirrored bar is drawn
  by hand; clock via `print_centered`.
- **`save_int`** for a best-win streak across runs.

## Controls
**P1 only** (P2 is AI):
- `A` / `D` — walk toward / away (auto-faces the opponent)
- `W` — jump, `S` — crouch
- Hold **back** (away from opponent) — **block**
- `Z` — light attack, `X` — heavy attack
- **down, down-forward, forward + `Z`** — **fireball** (quarter-circle-forward)
- At match end: `Z` to rematch.
