# cart-metadata — each cart owns its metadata; index.json becomes a generated view

STATUS: DESIGN / agreed in conversation 2026-06-29, not yet built. Implements field note
`013-generated-cart-index` + the action-plan Tier-1 keystone
([`action-plan.md`](action-plan.md)). Companion: `lint-carts.js` (owns the tag vocabulary today),
field note `012-self-describing-artifacts` (the principle), `003-curation` (the featuring system this
unblocks).

## The problem

`editor/public/carts/index.json` is one hand-edited file (414 entries). Every new cart or tag tweak
edits it, so parallel agents collide on it constantly — CLAUDE.md spends a whole hazard (#2) + a
stash/splice dance on exactly this. Registration is also a manual step (`make-cart` bakes the
`.cart.png` but doesn't touch index.json).

## The move

Each cart carries a **`de:meta` block** in its `.c`; a generator scans the carts and *produces*
`index.json`. The block is the cart's **full self-description — a superset** of what any one
consumer needs. index.json is just the gallery's *view* of it; a future man-page view and a
data-I/O registry are other views over the same block (the field-note-012 "self-describing artifact
→ many derived views" principle).

```c
/* de:meta
{
  "title": "pixel zoo",
  "status": "active",
  "kind": ["toy", "tech-demo"],
  "genre": null,
  "homage": null,
  "teaches": ["dithering-gradient", "adsr-envelope"],
  "lineage": "animals from primitives; fillp() dither + synth ADSR + filter-swept roar",
  "description": {
    "summary":  "Stroll a little zoo — each enclosure answers as you walk up.",
    "detail":   "Showcases fillp() dither gradients, spr_rot()/sspr_ex(), and the synth (ADSR pad, vibrato birds, lowpass-swept roar). Animals drawn from primitives.",
    "controls": "WASD walk · Z wave"
  }
}
de:meta */
#include "studio.h"
// ... the human docblock stays below, free for prose, todos, research links ...
```

It's a C block comment, so it never affects compilation, travels inside `de:source` harmlessly, and
is greppable in source (`grep -l adsr-envelope tools/carts/*.c`).

## Schema

| field | in index.json? | notes |
|---|---|---|
| `title` | yes | |
| `status` | not yet | `active`(default) / `showcase` / `retired` / `archive` / `hidden` — seeds the real featuring system (replaces the dead array-order "featured" sort). All carts migrate to `active`. |
| `kind[]` | yes | vocabulary stays in `lint-carts.js` (`KINDS`) |
| `genre` | yes | required when `kind` includes `game` (`GENRES`) |
| `homage` | yes | optional |
| `teaches[]` | yes | controlled vocab (`tools/teaches-vocab.js`) |
| `lineage` | yes | one-liner |
| `description` | yes (flattened) | **string OR parts** — see below |
| `file` | yes | **derived**, not stored: `<stem>.cart.png` from the `.c` filename |
| `inputs` / `outputs` | not yet | RESERVED — the data-contract a cart consumes/produces (the "carts that take in AND emit data" direction). Generator ignores until a consumer exists; additive, no future migration. |
| `see_also` / `notes` | not yet | RESERVED — man-page growth. |

### `description`: string or parts

- **String** (legacy / just-migrated): a single blob, exactly today's behavior.
- **Parts** (object): `summary` (the gallery one-liner / man-page NAME), `detail` (the longer
  showcase prose / DESCRIPTION), `controls` (how you interact / USAGE).

The generator **flattens parts → the single `description` string** index.json wants today, so the
gallery is unchanged. The day the gallery learns to render sections, it reads the parts directly —
the man-page grows *in place*, no migration. Splitting a blob into parts is *editorial*, so it's
**opt-in, done when a cart is touched** (same promotion spirit as the rest of the repo). We never
hand-edit 414 descriptions.

## Ordering — array order is deprecated

The gallery's old default sort `'featured'` == index.json array order. Per the maker (2026-06-29)
that order is **stale and unused** — browsing is by `updated` (git date), and real featuring needs
its own system anyway. So:

- The generator emits **deterministic alphabetical order** (stable diffs; nobody curates the array).
- `shell.js` default `cartSort` flips `'featured'` → `'updated'`; the `'featured'` option is later
  **repointed to `status: "showcase"`** (a generated curated view), or dropped.

## Tooling

- **`build-cart-index.js`** (new) — scan `tools/carts/*.c`, parse each `de:meta`, flatten
  `description`, derive `file`, emit `editor/public/carts/index.json` alphabetically. **Generated and
  committed** (like `build-compendium.js` output); `cart-status.js --check` gates staleness.
- **`lint-carts.js`** (flip) — was "index.json is source of truth"; becomes "validate each cart's
  `de:meta` (required fields, vocab, `status`) **and** assert index.json is in sync with a fresh
  generate." Keeps owning `KINDS`/`GENRES`.
- **`make-cart.js`** (extend) — on bake, regenerate index.json so **baking auto-registers a cart**.
  This deletes cart-authoring manual step 5.

## Migration (one-time, run while no other agent is active)

1. For each index.json entry, find its cart by `file` stem → inject a `de:meta` block at the top of
   `<stem>.c`, lifting `title/kind/genre/homage/teaches/lineage` and `description` **as a verbatim
   string**, plus `status: "active"`.
2. Guard: assert no `description`/`lineage` contains `*/` (would close the C comment) — escape or
   flag if any do.
3. **Proof (content-lossless):** run `build-cart-index.js`, then assert the regenerated entries —
   keyed by `file` — are deep-equal to the current index.json on every field. The *only* allowed
   difference is array order (now deterministic). Commit nothing until this passes.

## Future views from the same block (not now)

- **Man-page** rendering of `description` parts + `controls` + `see_also`.
- **Data-I/O registry** from `inputs`/`outputs` — once carts declare what data they read/write
  (`de_data_path()` / save blobs / `.rvb` etc.), this becomes a generated map of which cart consumes
  or produces which data shape.
- **Curated collections** (`003-curation`) from `status` + `kind` + relationships.

All are *additional* generated views; none require touching the carts again.
