# The game you're building: BLACKJACK

## Premise
The casino table classic, one player against a stone-faced dealer. You sit down with a
**bankroll**, push some chips out as a **bet**, and the felt does the rest: two cards
each, the dealer's hole card face-down, and the eternal question — **hit or stand?**
Aces are soft (1 or 11), the dealer plays by the book (**hits 16, stands on 17**), and a
natural blackjack pays you back with a flourish. The bankroll **persists between runs**
via `save()`, so the bragging rights (and the slow bleed) carry over.

## The core loop (one hand)
1. **BETTING** — adjust the wager up/down from your bankroll, then **Deal**.
2. **DEAL** — player gets two cards face-up, dealer one up + one hole card. A two-card
   21 is an instant **blackjack** (pays 3:2) unless the dealer also has one (push).
3. **PLAYER TURN** — **Hit** (draw a card, bust over 21), **Stand** (lock your total),
   or **Double** (one card only, double the bet — bankroll permitting).
4. **DEALER TURN** — flip the hole card, then draw until **17+** (stands on all 17s,
   including soft 17 for simplicity).
5. **SETTLE** — compare totals: bust / dealer-bust / win / lose / push / blackjack. The
   bet is settled against the bankroll, a result banner shows, then back to BETTING.
   Broke? A one-tap "rebuy" tops you back up so the table never closes.

## What it leans into (and why)
- **Cards from primitives, no sprite sheet.** Following `poker.c`: rounded white cards
  drawn with `rectfill`/`rect`, pip suits built from `circfill`/`trifill`, rank strings
  via `print`. A card game is all UI — primitives are the right tool, and it keeps the
  cart self-contained with zero `.cart.js`.
- **Soft-ace hand evaluation.** The one bit of real logic: sum cards counting aces as 11,
  then demote aces to 1 while busting. Shows "soft N" when an ace is still flexible.
- **A dealt-card animation + reveal.** Cards **ease in** from the deck to their slot
  (`lerp`/`ease_out`), the hole card **flips** on the dealer's turn — the moments that
  earn a little motion. A short per-card deal cadence makes the table feel alive.
- **Juice at the payoff.** `shake` + red flash on a bust, a rising `strum` major chord on
  a win/blackjack, a soft thud `note` on each dealt card, a chip `note` on bet changes.
- **Bankroll that persists** — `save()`/`load()` the bankroll (and a hands-played count)
  so a session is continuous. The house edge does its quiet work over time, slots-style.

## Controls
Keyboard-first, mouse-clickable buttons too.
- **BETTING:** Left/Right (or A/D) = bet -/+, **Z / Enter** = Deal.
- **PLAYER TURN:** **Z** = Hit, **X** = Stand, **C / Down** = Double.
- **SETTLE:** **Z / Enter** = next hand. When broke, **Z** = rebuy.
- Mouse: click the on-screen buttons (Bet -, Bet +, Deal / Hit / Stand / Double).
