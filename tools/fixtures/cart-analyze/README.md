# cart-analyze fixture — known answers

`node tools/cart-analyze.js --selfcheck` re-runs the tool as a child with `DE_ANALYZE_CARTS_DIR` /
`DE_ANALYZE_CART_EXT` pointed here and asserts 23 known answers over 11 fixture carts.
`.c.txt`, never `.c` (never compiled; a real `.c` gets indexed by clangd and read as a cart by
anything globbing for sources).

**Why it needs one.** The verdict is a **fall-through chain**, and its *order* is the judgement:
`simple` is tested first, so a tiny widget cart is `simple` rather than `reactive`. Reorder the
chain and the answer changes for a whole class of carts. Underneath, every metric is a line regex
over decommented source, and two of those rules exist purely to stop the score inflating —
commented-out scratch code must not count, and `static const` tables are data rather than the
cart's mutable state. All of it fails **silently**: nothing breaks, carts just rank in the wrong
order and the spec backlog points somewhere useless.

## One cart per branch

| cart | verdict | pins |
|---|---|---|
| `tiny` | simple | small + few globals + light update |
| `tinyreactive` | **simple** | the chain ORDER: it is *also* reactive, and `simple` wins |
| `statey` | stateful | the `de_state()` branch, **and only that branch** (see below) |
| `bigstate` | stateful | the second branch: globals ≥ 6, arrays ≥ 1, update-heavy, no `de_state` |
| `proc` | procedural | draw-dominant, `drw` 64 |
| `drawish` | **mixed** | draw-leaning (`drw > upd*1.8`) but under the `drw >= 60` floor |
| `react` | reactive | `ui_slider` / `note_on`, too big to be `simple` |
| `mixed` | mixed | the fall-through |
| `commented` | simple | `decomment()` — statics, a function, `ui_button`, `note_on` and `S->` all inside comments |
| `conster` | mixed | 5 `static const` tables + a `static inline` helper, still only 2 globals |
| `probey` | mixed | a `_probe()` fn: `+2`, visible as a score delta against its twin `mixed` |

The score assertion **recomputes the whole formula** from each row's reported metrics, so a changed
weight fails even when every verdict still agrees.

## Two fixture shapes forced by mutation-testing

- **`statey` carries exactly 5 globals and no arrays.** The first draft had 7 globals and an array,
  which satisfies the *second* stateful branch too — so deleting the `de_state()` branch outright
  still scored 22/22. Five is the tuned window: >4 so it cannot fall through to `simple`, <6 so the
  globals/arrays branch cannot also claim it.
- **`drawish` exists only to sit in the `procedural` floor's blind spot.** No other fixture cart has
  `drw > upd*1.8` while `drw < 60`, so lowering the floor to 0 changed nothing and the threshold was
  untested.

Confirmed by mutation: flipping the chain order, dropping comment stripping, counting `static const`
as state, not counting arrays, dropping either stateful branch, lowering the procedural floor,
dropping either score dampener, changing the probe bonus, reweighting globals, sorting ascending, and
measuring `draw()` where `update()` is meant — each turns 1–4 assertions red.

See `docs/guides/checks-and-oracles.md` → "Self-test the checker".
