# cart-dupes fixture — known answers

`node tools/cart-dupes.js --selfcheck` runs the whole tool against this 10-cart fixture repo
(via the `DE_DUPES_RUNTIME_DIR` / `DE_DUPES_CART_DIR` / `DE_DUPES_CART_EXT` overrides) and
asserts 20 known answers. `.c.txt`, never `.c`: a fixture cart is never compiled, and a real
`.c` would be indexed by clangd and read as a cart by anything globbing for sources.

**Why it needs one.** The tool's own header calls normalization *"the trick that makes it find
REAL clones instead of every `for` loop"*: cart-local identifiers collapse to `V` while the
engine/library vocabulary stays literal. Both halves are load-bearing **in opposite directions**.
Lose the collapse and a renamed copy stops matching, so a real clone goes unreported. Lose the
literal vocabulary and two blocks calling *different* engine functions match anyway, so the
report fills with false clones. Nothing measured either half.

## The alpha / beta / gamma trio

Three carts with **identical structure**, differing only in which *class* of identifier changes.
That isolates normalization from every other variable:

| cart | vs alpha | must |
|---|---|---|
| `beta` | every **local** renamed (`pads`→`cells`, `i`→`k`, `px`→`bx`) | **cluster** with alpha — proves locals collapse to `V` |
| `gamma` | same locals, but **different engine calls** (`note_on`→`note_off`, `circfill`↔`rectfill`) | **not cluster** — proves engine vocabulary stays literal |

## Everything else

| carts | shape | pins |
|---|---|---|
| `eta` + `zeta` | a byte-identical `clamp_all()` | an exact copy is an **extraction** candidate, and explicitly **not** drift |
| `delta` + `epsilon` | `apply_env()` copied then diverged | the drift band `[0.6, 0.999)`, median similarity 0.7, and the `where` list |
| `theta` + `iota` + `kappa` | `mix_bus()` ×3: two identical, one drifted | the only shape that produces `identicalPairs` **and** `driftedPairs` together — a two-copy name can't. Also a `spread: 3` extraction cluster |
| all 10 | a long `draw()` in six of them | `HOOK_CUTOFF` (>40% of carts ⇒ an engine hook, not a copy). Without it, `draw` is reported as drift |

`runtime/fake.h` is the fixture's own tiny anchor vocabulary (27 identifiers vs the real 6,747).
It declares `N` and `V` as **parameter names on purpose** — that is exactly how the normalization
sentinels reach the real anchor set, and without them `anchor.delete('V'/'N')` is a no-op here and
the sentinel-collision assertion can never fail.

## What the fixture found

Writing it exposed **two real bugs in the shipped tool**, both fixed in the same commit:

1. **The anchor vocabulary was harvested from raw header text, comments included.** 8,709 of
   15,456 entries (56%) were English prose — `the`, `and`, `shared`, `extracted`, `carts`,
   `drifting`. Since an anchored identifier stays literal instead of collapsing, any cart-local
   whose name happened to appear in a header comment silently defeated the core trick. It became
   visible here because *this fixture's* anchor set contained the words of its own header comment
   (`That`, `Never`, `trick`, `loop`). Fixed by `decomment()`-ing headers first.
2. **The sentinels `V` and `N` were themselves vocabulary.** `N` really is a header parameter
   name, so a cart-local named `N` and every numeric literal both normalized to `"N"` and
   compared **equal** — a false match — and the report listed `V`/`N` among a cluster's api
   calls, which is visible nonsense. Fixed by excluding both.

## Coverage this fixture deliberately does NOT have

Two mutations survive it, and both are honest rather than fixable-by-contortion:

- **The lockstep BACKWARD extension is unexercised.** Seeds are tried in scan order, so the
  earliest-inserted window of a shared block always wins and `back` is 0 for that block's first
  cluster. Backward extension only matters for a cluster whose *head* was already consumed by an
  earlier one, which cannot be constructed deliberately without reaching into the hash iteration
  order. Deleting the backward loop still scores 20/20.
- **The `≥2 distinct carts` guard is redundant in the tool**, not untested: the same condition is
  checked three times (`cartsSeen.size`, then `members` carts, then `perCart.size`). Removing any
  one changes no behaviour, so no fixture can catch it.

Everything else is confirmed by mutation: collapsing the engine vocab too, making all identifiers
literal, re-polluting the anchor with prose, restoring the sentinels, dropping `--min-tokens`,
scoring without cart-spread, widening the drift band past identical, dropping `HOOK_CUTOFF`,
reporting names with no drifted pairs, and re-ranking drift — each turns 1–3 assertions red.

One more lesson, recorded in the guide: the selfcheck's own accessors must be **crash-safe**. The
first draft read `clusterOf('alpha','beta').tokens` directly, so the mutation that removes that
cluster entirely died with a `TypeError` instead of reporting which expectations broke.

See `docs/guides/checks-and-oracles.md` → "Self-test the checker".
