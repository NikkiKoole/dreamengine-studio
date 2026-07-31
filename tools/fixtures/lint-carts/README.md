# lint-carts fixture — known answers

`node tools/lint-carts.js --selfcheck` asserts the checker against these. 48 assertions
over the two halves it validates:

- **`hazards/*.c.txt`** — synthetic cart sources for the three SOURCE HAZARDS (the CLAUDE.md
  gotchas promoted into checks). This is the half worth pinning: each hazard is a regex with
  an **exempt class** (a comment, a struct field, a waiver, a grandfathered cart, a
  paren-nested argument, a cart not on `touch_id()`), and every exempt class is one `continue`
  away from either flooding — so someone waives the lint wholesale — or going blind, so the
  SIGSEGV ships.
- **`meta/cases.json`** — a declarative table of `de:meta` objects and the verdict each must
  get. Mechanical rules, but there are ~20 of them and the *valid* cases are the ones that
  keep the vocabulary honest.

`.c.txt`, never `.c`: a fixture cart is never compiled, and a real `.c` here would be indexed
by clangd (phantom diagnostics) and read as a cart by anything globbing for sources.

## The hazard cases

| file | must find | must NOT find |
|---|---|---|
| `watch.c.txt` | both bare-value 2nd args (`FLAG_*`) | a real format string · a comma nested in the 1st arg's own parens · a comma inside the format string · a commented-out bad call · a `// lint-watch-ignore` line |
| `shadow.c.txt` | a file-scope local named `map` | struct fields named `timer`/`line`/`circ` (incl. one nested a level deeper) · the renamed forms `grid`/`tmr` · a `// lint-shadow-ignore` line |
| `shadow-grandfathered.c.txt` | `line`, when read as any cart **except** `sensi` | `line`, when read as `sensi` (the pair is in `GRANDFATHERED_SHADOWS`) |
| `pool.c.txt` | both the `.id < 0` and `.id == -1` free-slot forms | — |
| `pool-ok.c.txt` | nothing (the cart is on `pointer.h`) | the `.id < 0` it deliberately contains |
| `pool-notouch.c.txt` | nothing (never calls `touch_id()`) | the `.id < 0` it deliberately contains |
| `clean.c.txt` | nothing at all | anything |

Two of these are shaped by mutation-testing, and the shapes are load-bearing:

**`shadow-grandfathered.c.txt` is run twice, under two cart names.** One run proves the
exemption works; the other proves it is keyed on `cart:builtin` rather than globally
swallowing every local named `line`. A single run can't tell those apart.

**`pool-ok.c.txt` must contain a regex-matchable `.id < 0`.** Its first draft used a bare local
(`int id = touch_id(i); if (id < 0)`), which the hazard regex never matches — so the case passed
for the wrong reason, and deleting the `pointer.h` exemption outright still scored a clean 48/48.
Same trap in a different costume in `clean.c.txt`: it deliberately calls `watch()`, calls
`touch_id()` and declares a struct, and `--selfcheck` **asserts those three triggers are still
present in its bytes** — otherwise the day someone trims it down, the cry-wolf guard quietly
becomes a test that an empty file has no bugs.

## What `--selfcheck` deliberately does not cover

The `index.json`-in-sync assertion and the "every `de:meta` has a baked `.cart.png`" check are
inherently about the real tree, so they aren't fixtured. `build-cart-index.js --check` already
gates the first, and both are mechanical (a hash comparison and an `existsSync`) rather than
judgement calls.

See `docs/guides/checks-and-oracles.md` → "Self-test the checker".
