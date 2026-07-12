# Learn You a dreamengine for Great Good!

STATUS: building (2026-07-12) — 4 chapters drafted; voice + pipeline settled. Pick up any time.

An illustrated, chatty field guide to the `studio.h` API, in the voice (and spirit) of
*Learn You a Haskell for Great Good!* — the beginner kept as a delighted critic (per
[ADR-0022](../decisions/0022-collaboration-is-the-north-star.md)). Warm, silly, a little
gremlin narrator who explains the scary bits.

**The one rule that makes it special: every illustration is REAL console output.** No
hand-drawn diagrams, no mockups. Each picture is a tiny cart the engine actually renders,
so the book can never quietly drift from the thing it teaches. Stills are baked with
`play.js --dump`, moving ones with `make-gif --format gif`.

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
2. Add an entry to `ILLUS` in `tools/build-book.js` (`{ kind: 'still' }` or
   `{ kind: 'gif', frames, fps }`), and write the prose + `screen('<name>', …)` call.
3. `node tools/build-book.js` — renders assets + rewrites the page. Look at it.

Gotcha baked into the generator: a `book/<name>` cart makes the harness write build
artifacts to `build/book/…`, so `build-book.js` pre-creates `build/book/` and
`build/.gif/book/` (otherwise the linker fails with `open() failed … build/book/<name>-dbg`).

## Chapters so far

1. **So, You Want To Make A Little World** — `draw`/`cls`/`circfill`, the 32-colour palette, poke-it-with-`btn`
2. **Keeping The Sun On The Screen** — clamping to `SCREEN_W`, the off-by-one dragon, then a `fade()` comet-trail bounce (GIF)
3. **A Small Cast Of Shapes** — `rectfill`/`trifill`/`ovalfill`/`line`/`print`, painter's order
4. **A World That Moves On Its Own** — `frame()` through `sin_deg` so the world breathes (GIF)

## TODO / ideas (pick up when we feel like)

- More chapters: **sprites** (`spr` + the sprite editor), **sound** (a first `hit()`/note),
  a **first tiny complete game** that ties it together (the "great good" payoff).
- Decide whether to add a `--check` staleness gate + repo-doctor hook once the content
  stabilises (skipped for now so it doesn't nag while WIP).
- Consider a light "paper" theme toggle (currently a committed single dark-screen world).
