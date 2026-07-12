# Learn You a dreamengine for Great Good!

STATUS: shipped (2026-07-12) — all 15 planned chapters drafted (draw → ship-it). Content-complete; bench chapters optional. Pick up any time.

An illustrated, chatty field guide to the `studio.h` API, in the voice (and spirit) of
*Learn You a Haskell for Great Good!* — the beginner kept as a delighted critic (per
[ADR-0022](../decisions/0022-collaboration-is-the-north-star.md)). Warm, silly, a little
gremlin narrator who explains the scary bits.

**The one rule that makes it special: every illustration is REAL console output.** No
hand-drawn diagrams, no mockups. Each picture is a tiny cart the engine actually renders,
so the book can never quietly drift from the thing it teaches. Stills are baked with
`play.js --dump`, moving ones with `make-gif --format gif`.

Kinds of illustration, deliberately distinct: **honest "screens"** (the CRT-bezel
figures — actual program output, captioned, the honesty pitch; stills or GIFs, and for
the sound chapter a **webm `<video>` that carries the cart's real audio** — press play,
hear the engine) and **mood "creatures"** (a per-chapter blob mascot, no bezel,
uncaptioned — atmosphere, the LYAH whimsy). All console-drawn; only the screens carry a
teaching caption.

## Voice notes (guardrails)

- Warm, silly, self-implicating ("who stole my roof — it was me too"). The gremlin admits
  its own mistakes; errors are demystified, not dreaded (Ch.1 quotes a real compiler error).
- **Don't over-do the "type this / hit ▶ / watch" call-to-action** (owner's note) — the
  honest-output screenshots already imply "this is real, go try it." Use it sparingly, not
  every example.
- Captions on the honest screens do real work (e.g. *"the output of the four lines above —
  nothing else"*) — keep them; that's the pitch. (LYAH leaves its art uncaptioned; we don't,
  for the screens.)
- Puns escalate/relax with difficulty (LYAH's trick): playful chapter titles, literal when
  the content gets heavy.

## What we took from the source (LYAH)

Studied the *Learn You a Haskell* community edition
([learnyouahaskell.github.io/chapters.html](https://learnyouahaskell.github.io/chapters.html))
to calibrate voice and structure. The devices that make it feel like LYAH, and how we handled each:

| LYAH device | What it is | Us |
|---|---|---|
| **Punny headings on a difficulty dial** | "Baby's first functions", "Only folds and horses"; headings go *literal* as content gets heavier | Playful chapter titles; `h3`s in-voice — **borrowed** |
| **The REPL as co-teacher** | "open GHCi, type `2 + 15`, get `17`. Pretty cool, huh? Yeah, I know it's not, bear with me" — a cascade of trivial runnable examples | Our ▶-run is the REPL, but owner's note: **don't over-lean on "hit run and watch"** — see Voice notes |
| **Errors shown then translated** | pastes `5 + "llama"`, the real error, "llama is not a number" | **borrowed** — Ch.1 quotes the real `CLR_GRAY` error and translates it |
| **A vulnerable narrator** | "I failed to learn Haskell approximately 2 times" | **borrowed** — the gremlin admits its own mistakes |
| **Whimsical creature art** | bird/egg/lazy-boat/"list monster" — decorative, uncaptioned, mood not diagram | **borrowed as "creatures"** — but console-drawn, one per chapter |
| **Uncaptioned images** | art carries no caption | **declined for the "screens"** — our captions ("the output of the four lines above — nothing else") *are* the honesty pitch; creatures stay uncaptioned |
| **Mid-book real-program payoff** | ch.10 "Functionally Solving Problems" (RPN calc, Heathrow→London) solves a whole thing before the hard abstractions | **borrowed** — Ch.5's catch game; a milestone, not the finale |

Our own twist LYAH couldn't have: **every illustration is honest engine output** (see the rule
up top) — for a *visual* console that beats decorative art, because the pictures can't lie.

Structural arc, for reference: LYAH runs foundations → practical techniques → the type system →
the scary abstractions (functors/monads) → specialised. It *earns* the hard stuff late and rewards
you with a real program in the middle. Our spine so far: draw → bounds → shapes → time → **game
payoff** → sprites → sound; the payoff sits mid-book on purpose.

## Where things live

- `tools/carts/book/<name>.c` — the illustration carts (a dedicated folder, deliberately
  a subfolder so it's invisible to the flat cart gates — `build-all`, `lint-carts`,
  `build-cart-index` all `readdirSync` non-recursively, so these need no `de:meta` and never
  hit the gallery). Driven as `play.js book/<name>` / `make-gif book/<name>`.
- `tools/build-book.js` — the generator. Renders every illustration through the harness,
  then emits the page. **The chapter prose lives inside this file** (like the other
  `build-*.js` tools embed their templates). Re-run it after an engine change and the
  pictures re-bake themselves.
- `docs/learn-you-a-dreamengine.html` — the generated page, opened via the Docs tab's
  **★ book** item (`showBook()` in `editor/src/shell.js`).
- `docs/book-assets/` — the generated PNG/GIF illustrations (committed; referenced
  relatively, the same convention as `history.html`'s clips).

## Adding a chapter / illustration

1. Write `tools/carts/book/<name>.c` — a normal cart (`#include "studio.h"`, `draw()`,
   optional `update()`/`init()`). For an animated illustration, make it self-animate off
   `frame()` and aim for a clean loop (e.g. `wave` loops every 120 frames).
2. Add an entry to `ILLUS` in `tools/build-book.js` — `{ kind: 'still' }`,
   `{ kind: 'gif', frames, fps }`, or `{ kind: 'video', frames, fps }` (a webm that
   carries the cart's audio; `screen()` auto-emits `<video controls>` for it). Then write
   the prose + `screen('<name>', …)` call.
3. `node tools/build-book.js` — renders assets + rewrites the page. Look at it.

Sprites: a cart that uses `spr()` needs a `tools/carts/book/<name>.cart.js` sidecar
(sprite-draw.js generators, keyed by slot) — see `sprites.cart.js`. Preview sprites alone
with `node tools/sprite-preview.js book/<name>`.

Each chapter also gets a mood creature (see `greeter`/`leaper`/`juggler`/`dreamer`/`champion`)
placed with the `creature('<id>', alt)` builder right after its `chapHead(...)`. New chapter →
draw a new blob (copy an existing creature cart's body recipe, swap the pose/prop).

Gotcha baked into the generator: a `book/<name>` cart makes the harness write build
artifacts to `build/book/…`, so `build-book.js` pre-creates `build/book/` and
`build/.gif/book/` (otherwise the linker fails with `open() failed … build/book/<name>-dbg`).

## Chapters so far

1. **So, You Want To Make A Little World** — `draw`/`cls`/`circfill`, the 32-colour palette, poke-it-with-`btn`
2. **Keeping The Sun On The Screen** — clamping to `SCREEN_W`, the off-by-one dragon, then a `fade()` comet-trail bounce (GIF)
3. **A Small Cast Of Shapes** — `rectfill`/`trifill`/`ovalfill`/`line`/`print`, painter's order
4. **A World That Moves On Its Own** — `frame()` through `sin_deg` so the world breathes (GIF)
5. **Your First Actual Game** — the payoff: a whole playable catch game (`catch`, GIF, plays
   itself in attract mode) that is *only* chapters 1–4 stacked; nothing new but the collision question
6. **Drawing With Pictures, Not Just Shapes** — `spr` + the numbered sheet, two-frame walk
   animation, `colorkey` (`sprites` GIF; sprites in `sprites.cart.js`)
7. **Making A Racket** — `note(midi, instr, vol)`, pentatonic sequencer, `instrument()` setup
   (`song`, a webm you can HEAR). Ends Part I — hinges into "Part II: saying the six words well".
8. **Do Two Things Touch?** — AABB collision, the `overlap()` test, the hitbox "honest lie" (`collide` GIF)
9. **A Cast of Thousands** — arrays + one `for` loop for the whole cast; ties back to `overlap()` (`swarm` GIF)
10. **Juice: Making It Feel Good** — feedback tied to events: `shake()` + squash/flash timers + a
    particle burst; the "a little goes a long way" restraint (`juice` GIF)
11. **Worlds Bigger Than the Screen** — `camera()` scrolling + a minimap; `map()` mentioned (`world` GIF)
12. **Remembering Things** — `save_int`/`load_int` + a title→play→over state machine (`states` GIF)
13. **A Proper Little Game** — the second payoff: a self-playing shooter that is chapters 7-12
    stacked (arrays + `overlap` + `shake` + `note`); a webm you can HEAR (`shooter`)
14. **Make It Work on a Phone** — `touch_layout()` (a mocked on-screen stick + A/B), `safe_rect`,
    `finger_px`; "design for the thumb" (`phone` GIF)
15. **Ship It** — the finale: bake to web, a link to a friend; the greeter returns to wave goodbye
    (bookend). Ends on "go do a great good" + "— fin —" (`ship` GIF, `farewell` creature)

## Roadmap — chapters 8–15 (planned, locked 2026-07-12)

The arc keeps earning harder ideas, drops a **second bigger payoff** at 13, lets titles get
more literal as they get meatier, and **bookends** the book (15's farewell creature calls
back to Ch.1's greeter). Each still needs its own mood creature.

 8. ✓ SHIPPED — **Do Two Things Touch?** (collision)
 9. ✓ SHIPPED — **A Cast of Thousands** (arrays + loop)
10. ✓ SHIPPED — **Juice: Making It Feel Good** (game feel; used the built-in `shake()`)
11. ✓ SHIPPED — **Worlds Bigger Than the Screen** (`camera()` scrolling + minimap)
12. ✓ SHIPPED — **Remembering Things** (`save_int`/`load_int` + title→play→over states)
13. ✓ SHIPPED — **A Proper Little Game** (self-playing shooter, webm with audio)
14. ✓ SHIPPED — **Make It Work on a Phone** (`touch_layout` mock + `safe_rect`/`finger_px`)
15. ✓ SHIPPED — **Ship It** (bake-to-web finale; greeter-callback farewell + "— fin —")

Note: the engine hides on-screen touch controls on desktop/headless, so Ch.14's `phone` cart
MOCKS them (draws its own stick + A/B) to illustrate what `touch_layout()` paints on a device.

Bench / possible swaps: audio **effects** (`crush`/`reverb`, a sequel to Ch.7), **mouse & pointer**,
a **menus & UI** chapter (`lay.h`/`ui.h`), or a cheeky **"Here Be Dragons"** 3D (`tritex`) /
multiplayer teaser as a "there's a whole other book after this" wink.

## TODO / ideas (pick up when we feel like)

- Decide whether to add a `--check` staleness gate + repo-doctor hook once the content
  stabilises (skipped for now so it doesn't nag while WIP).
- Consider a light "paper" theme toggle (currently a committed single dark-screen world).
