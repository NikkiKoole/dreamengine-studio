# Cart specs — Batch 3

Third wave — fills the empty *genres* the catalog was missing (board games, match-3 /
falling-block, card games, word games, classic fixed shooters, dig-'em, single-screen
arcade) plus a few high-value one-offs. Built the same orchestrated way as batch 2 (see
"Building many in parallel" in [`README.md`](README.md)); pairs under the brief in
[`cart-authoring-prompt.md`](../cart-authoring-prompt.md). Batches 1 & 2 stay intact as
their own historic records.

## Specs

Scope flags: 🟢 well-scoped · 🟡 sliced from a bigger game · 🔴 huge, cut hard.

| Spec | Game | Cart | Scope | Locked slice |
|---|---|---|---|---|
| `chess.md` | Chess | `chess` | 🔴 | full legal moves + check/checkmate; 2P hotseat + a shallow minimax AI; click-to-move |
| `othello.md` | Othello / Reversi | `othello` | 🟢 | 8×8 disc flips, legal-move highlights, greedy AI, live score |
| `connect4.md` | Connect Four | `connect4` | 🟢 | 7×6 drop, 4-in-a-row detect, minimax AI |
| `battleship.md` | Battleship | `battleship` | 🟡 | place fleet, fire at hidden grid, hit/miss/sunk, hunt-AI opponent |
| `bejeweled.md` | Bejeweled (match-3) | `bejeweled` | 🟢 | swap adjacent gems, match-3+ clears, cascade refill, reshuffle on no-move |
| `drmario.md` | Dr. Mario | `drmario` | 🟡 | falling 2-cell pills, clear 4-in-line by color, eliminate all viruses |
| `puyo.md` | Puyo Puyo | `puyo` | 🟡 | falling pairs, group-4 clears with chain combos, rotate |
| `columns.md` | Columns | `columns` | 🟢 | falling gem-triple, cycle colors, match-3 any direction + gravity |
| `solitaire.md` | Klondike Solitaire | `solitaire` | 🟡 | 7 tableau + stock/waste + 4 foundations, drag sequences, auto-flip, win |
| `blackjack.md` | Blackjack | `blackjack` | 🟢 | hit/stand/double, dealer hits<17, bankroll betting, bust/win |
| `wordle.md` | Wordle | `wordle` | 🟢 | 5-letter / 6-guess, green-yellow-gray feedback, on-screen keyboard |
| `hangman.md` | Hangman | `hangman` | 🟢 | guess letters, draw the gallows on misses, reveal word, win/lose |
| `boggle.md` | Boggle | `boggle` | 🟡 | 4×4 letter grid, drag-adjacent to spell, validate vs embedded word list, timer score |
| `minesweeper.md` | Minesweeper | `minesweeper` | 🟢 | flood-fill reveal on zeros, flag mines, number hints, win/lose |
| `missilecmd.md` | Missile Command | `missilecmd` | 🟢 | mouse-aimed intercepts, defend cities, per-battery ammo, escalating waves |
| `centipede.md` | Centipede | `centipede` | 🟡 | shoot a descending centipede through a mushroom field; segments split; spider |
| `defender.md` | Defender | `defender` | 🟡 | side-scroll shooter, rescue humanoids from abductors, scanner minimap, thrust/fire |
| `digdug.md` | Dig Dug | `digdug` | 🟡 | tunnel through dirt, pump enemies to pop them, drop rocks |
| `boulderdash.md` | Boulder Dash | `boulderdash` | 🟡 | dig dirt, collect diamonds, falling boulders crush, exit on quota |
| `loderunner.md` | Lode Runner | `loderunner` | 🟡 | collect gold, ladders/ropes, dig holes to trap guards, reach exit |
| `donkeykong.md` | Donkey Kong | `donkeykong` | 🟡 | climb girders + ladders, jump barrels, reach the top; one stage |
| `qbert.md` | Q*bert | `qbert` | 🟡 | isometric cube pyramid, hop to flip colors, dodge enemies, clear the board |
| `marble.md` | Marble Madness | `marble` | 🟡 | roll a marble through an isometric course to the goal under a timer; slopes/hazards |
| `pool.md` | Pool / Billiards | `pool` | 🟢 | aim + power cue, elastic ball-ball collisions, pockets, turn-based 8-ball |
| `streetfighter.md` | Street Fighter | `streetfighter` | 🔴 | 1v1 health bars, walk/jump/crouch/block, light+heavy + 1–2 specials, best-of-3 vs AI |
| `deckbuilder.md` | Slay the Spire (deckbuilder) | `deckbuilder` | 🔴 | turn card combat: energy, attack/block cards, enemy intent; tiny 3-node map + rewards |
| `farm.md` | Harvest Moon / Stardew (farming) | `farm` | 🔴 | till/plant/water, crops grow over days, harvest→sell for cash, day/energy clock |

**Reuse clusters** (point each at the *existing* exemplar it's nearest — intra-batch
siblings don't exist yet at build time): grid/board (`chess`, `othello`, `connect4`,
`battleship`, `minesweeper` ← `maze`/`astar`/`advancewars`); falling/match (`bejeweled`,
`drmario`, `puyo`, `columns` ← `tetris`); cards (`solitaire`, `blackjack`, `deckbuilder`
← `poker`); text (`wordle`, `hangman`, `boggle` ← `typesave`); fixed shooters
(`missilecmd`, `centipede`, `defender` ← `invaders`/`robotron`/`opwolf`/`sopwith`);
dig-'em (`digdug`, `boulderdash`, `loderunner` ← `pacman`/`sand`/`sokoban`/`platform`);
iso/physics (`qbert`, `marble`, `pool` ← `cube3d`/`pinball`/`lander`); farming (`farm` ←
`sims`/`sokoban`/`garden`).
