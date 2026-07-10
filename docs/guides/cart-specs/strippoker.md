# The game you're building: STRIPPOKER (PG-13)

## Premise
A cheeky, after-hours card lounge. You sit across a green-felt table from one CPU
sweetheart — **Sable** — for a round of **5-card draw**. Win a hand and the
**REVEAL meter** nudges up a notch; lose and it slips back. Fill it and Sable strikes
a playful new silhouette pose and throws you a wink — but it stays firmly **PG-13**:
soft silhouettes, a feather boa, a coy wink, double-entendre banter. No nudity. The
tone is the [smooch-lounge](smooch-lounge.md)'s velvet cabaret cheek, ported to a poker table.

## The core loop (one hand)
1. **Ante**: both sides put a chip in the pot. Five cards dealt face-down to you,
   face-down to Sable.
2. **Hold**: click your cards to toggle HELD (a gold frame). Up to all five.
3. **Bet round**: choose **BET** (push a chip), **CALL** (match), or **FOLD** (muck —
   you lose the ante, no reveal change). Sable answers with simple AI (calls a decent
   draw, folds a weak one, occasionally bluff-raises).
4. **Draw**: unheld cards replaced from the deck. Sable draws too (keeps pairs+/draws).
5. **Showdown**: best 5-card hand wins the pot. Win → **REVEAL meter +1 notch** + a
   flirty line + chips; lose → meter slips, Sable gloats.
6. Fill the meter (5 notches) → **the BIG REVEAL**: Sable changes silhouette pose,
   confetti hearts, the marquee blazes, a sax sting. Then the meter resets a touch and
   the flirtation escalates one tier. Run out of chips → game over, restart.

## States
- **TITLE** — neon "STRIP POKER" marquee, Sable in silhouette, press to deal.
- **DEAL/HOLD** — cards face up, click to hold.
- **BET** — three buttons; Sable "thinks" with a beat.
- **DRAW/SHOWDOWN** — cards flip, hands compared, banter callout.
- **GAME OVER** — out of chips; best meter tier saved; press to restart.

## The reveal meter (the cheeky core, kept tasteful)
- A 5-notch heart meter at the top. Each won showdown lights a notch; each loss
  dims one. Lighting the 5th triggers a pose change + escalating-but-coy banter tier
  (TEASE → FLIRT → SWOON → SCANDALOUS — all *text*, never explicit art).
- Sable is **all silhouette**: a filled body shape (ovals/tris/primitives) over the
  velvet backdrop, recolored with `pal()`-style dark fills, a feather-boa flourish,
  blinking eyes, and a wink animation on a win. Poses cycle as the meter tiers up:
  a hand-on-hip lean, a boa toss, a coy over-the-shoulder, a blown kiss.

## Hand evaluation
Reuse `poker.c`'s `evaluate()` shape (rank/suit counts → category) but compare TWO
hands head-to-head with a tiebreak on category, then high card. Categories:
high card, pair, two pair, trips, straight, flush, full house, quads, straight flush,
royal. No payout table — it's pot-vs-pot, winner takes the pot.

## Make it juicy
- **Cards** drawn as primitives (rounded rect, pip suits like poker.c's `draw_suit`),
  a flip animation on deal/draw (x-scale squash via `sspr`-style width lerp), gold
  HELD frame, green selection ring under the mouse.
- **Chip stacks** that grow/shrink; a coin `note()` on every bet/win.
- **Sable**: idle breathing sway (`sin_deg`), blink (`blink`), a wink + boa-flutter on
  a won hand, a pose morph + heart confetti on a meter fill (`shake` + sax `strum`).
- **Banter**: random flirty/teasing one-liners per outcome, smooch-lounge voice.
- **Sound**: a slow sultry lounge bass walk on the beat (custom filtered instrument),
  card-shuffle riffle (noise bursts), chip clinks, a wink "mwah", a sax stab on the
  big reveal. Quiet, classy, not spammy.

## Controls
Mouse-primary: **click your cards** to toggle HOLD; click **BET / CALL / FOLD**
buttons; click **DEAL** to start a hand and to advance through title / game over.
Keyboard fallback: ◀ ▶ to select a card, **Z** hold, **B**et, **C**all, **F**old,
**SPACE/ENTER** deal/advance.

## Lean into / read
`poker.c` (deal + hand-eval to extend), `smooch.c` (velvet cabaret tone, silhouette
performer, pal-recolor crowd, sax stings, beat-locked bass), `slots.c` (gamble feel +
chip economy), `juice.c` (shake/flash/ease). Skip: a tile map (single-screen table),
free movement, real betting depth — it's a flirty one-on-one, not a poker sim.

## Notes / open choices
- Pure-primitive art (no `.cart.js`) keeps Sable's silhouette flexible and the cart
  self-contained. PG-13 is a hard rule: silhouettes + innuendo text only.
- `save()` the best REVEAL tier reached as a tiny meta-score.
