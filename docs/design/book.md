# Learn You a dreamengine for Great Good!

STATUS: building (2026-07-12) — 7 chapters drafted (incl. a playable game + a sound chapter you can actually hear); voice + pipeline settled. Pick up any time.

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
   (`song`, a webm you can HEAR). Currently the finale ("go do a great good").

## TODO / ideas (pick up when we feel like)

- More chapters could follow the same shape: **tilemaps** (`map()` + a level), **saving**
  (`save`/`load`), or a **share it** chapter (bake to web). Each new chapter wants its own
  mood creature; a sound chapter proved the webm-with-audio path works.
- Decide whether to add a `--check` staleness gate + repo-doctor hook once the content
  stabilises (skipped for now so it doesn't nag while WIP).
- Consider a light "paper" theme toggle (currently a committed single dark-screen world).
