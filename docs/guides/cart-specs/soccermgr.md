# The game you're building: SOCCERMGR

## Premise
An FC-Mobile-style **football manager** mode. You run a club: a starting XI laid
out on a formation pitch, a bench of spares, and a cash budget. Drag bodies
between the pitch and the bench to pick your team, switch the **shape**
(4-4-2 / 4-3-3), wheel and deal in a **transfer market**, then hit **PLAY MATCH**
— the engine auto-sims a scoreline from your XI's strength versus the opponent's
and feeds the result into a small **league table**. Survive a 10-round season;
finish top. It's a menu + table game, mouse-first — NOT a real-time match.

## The core loop (one round = one match)
1. On the **SQUAD** screen, drag players between the 11 formation slots and the
   bench until you've fielded a full XI. Each slot wants a position (GK/DEF/MID/
   FWD); a player out of position still plays but at **-20% rating**, so the
   shape you pick matters. Switch between **4-4-2** and **4-3-3**.
2. Pop into the **MARKET** to spend your budget: a refreshing shortlist of
   buyable players (priced steeply by rating), and a SELL column for your bench
   spares — buy a face, bench a dud, sell the deadwood for cash.
3. Hit **PLAY MATCH**. The scoreline is sampled from a strength ratio
   (your summed XI rating vs the opponent club's), the other AI clubs play their
   own fixtures, and the **league table** re-sorts on points then goal
   difference. Win money on results.
4. After 10 rounds the season ends — your final league position is the score.
   `save()` the best finish (1 = champions).

## Screens / states
- **SQUAD** (default): a green tile-striped **pitch** with 11 draggable player
  chips + an empty-slot ghost per unfilled position; a **bench panel** on the
  right listing spares with pos/name/rating; **4-4-2 / 4-3-3** buttons; a big
  **PLAY MATCH** button (greyed until the XI is full).
- **MARKET**: two columns — a BUY transfer list (click the price to sign) and a
  SELL list of bench players (click the value to cash them in).
- **LEAGUE**: the 6-club table, your row highlighted, sorted live.
- **RESULT**: a scoreboard that **ticks the goals up** with a synth blip per
  goal, then a WIN/DRAW/DEFEAT verdict + your league position.
- **TITLE** + **SEASON OVER** (champions / runners-up / #n), both tiny.
- A bottom **tab bar** (SQUAD / MARKET / LEAGUE) is the spine; TAB cycles it.

## Make it juicy
- **Drag-and-drop** with a floating chip following the cursor; drop on a slot to
  swap, drop on the bench to drop a player. Coin blip on every successful move.
- **Out-of-position warning**: a blinking red ring round any chip in the wrong
  slot, so a bad formation reads at a glance.
- **Result reveal**: the scoreboard counts the goals up with `lerp`, a distinct
  note per home/away goal, then the verdict eases in.
- **PLAY MATCH** button lights up only when the XI is legal; the whistle (a
  two-note `INSTR_TRI` sting) kicks off the sim.
- League row for YOUR FC highlighted green; top spot in gold; GD coloured by sign.

## Data model
- `Player { name, pos, rating, kit }`, one pool `squad[18]`; `XI[11]` holds the
  player index per pitch slot (or -1). "Bench" = squad players not in `XI`.
- Formations as `FPOS[shape][11]` (position per slot) + normalized `FX/FY`
  pitch coords; strength = summed XI rating with the out-of-position penalty.
- `Club { name, str, W,D,L,GF,GA,pts }` × 6; `club[0]` = you, its `str` set from
  the live XI each match. `sim_goals()` samples a scoreline from a strength ratio.
- Market: `market[6]` + `mprice[6]` + `msold[6]`, re-rolled each round.

## Controls
Mouse-primary: **drag a player** to move him between a formation slot and the
bench; click **4-4-2 / 4-3-3** to switch shape; in the MARKET click a player's
**price to BUY** or a bench player's **value to SELL**; click **PLAY MATCH** (on
the squad or league screen) to sim the round. Keys: **TAB** cycles
SQUAD/MARKET/LEAGUE, **ENTER** plays the match, click/ENTER advances the result.

## Lean into / read
`druglord.c` (economy + table/menu UI + modals + the result-card juice idiom),
`poker.c` / `sims.c` / `papers.c` (table/sheet layout). Primitives only — chips
are `circfill` kits, the pitch is `rectfill` stripes; no sprite sheet needed.
Skip: real-time match, training, multiple seasons, free-roam.
