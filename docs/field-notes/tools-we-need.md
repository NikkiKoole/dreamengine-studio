# Tool Ideas from the Field Notes Session

## 1. `build-context`

Assemble task-specific context for agents.

Instead of manually telling an agent which docs, carts, ADRs and notes to read, this tool generates a focused briefing.

Example:

```sh
build-context roads
build-context cart:coaster
build-context api:audio
build-context task:retire-carts
```

It should gather:

* relevant design docs
* related carts
* ADRs
* field notes
* known pitfalls
* verification steps
* current status

Why:

Agents should not have to discover project knowledge manually. The right knowledge should reach the right agent at the right time.

---

## 2. `build-field-notes`

Generate an index and graph for `field-notes/`.

It should read all field notes and produce:

* chronological index
* related-note graph
* notes by status
* notes by confidence
* syntheses vs observations
* glossary terms used

Why:

The field notes will become more valuable as they grow, but only if they remain navigable.

---

## 3. `cart-meta-check`

Validate per-cart metadata once metadata moves out of one central index.

It should check:

* required fields
* valid status values
* missing descriptions
* broken relationships
* retired carts without successors
* showcase carts without enough metadata
* duplicate or inconsistent tags

Why:

Per-cart metadata enables parallel agent work, but only if the generated global index stays clean.

---

## 4. `build-cart-index`

Generate the global cart index from local cart metadata.

Instead of editing one giant `index.json`, each cart owns its own metadata file.

The tool produces:

* `index.json`
* website data
* search data
* compendium input
* category pages

Why:

Local truth should generate global truth.

---

## 5. `stale-doc-check`

Detect documentation that may need review after code, cart or ADR changes.

It should use frontmatter like:

```yaml
status: active
domain: roads
updated: 2026-06-26
depends_on:
  - docs/design/road-program-state.md
  - carts/streetlab/meta.hjson
```

Why:

The problem is not only finding knowledge. It is knowing when knowledge has gone stale.

---

## 6. `promote-candidate`

Find repeated patterns that may deserve promotion.

It should scan carts, metadata and docs for recurring concepts.

Example outputs:

* “graph routing appears in 5 carts”
* “audio modulation appears in 4 carts”
* “state-machine appears in 19 carts”
* “road geometry concepts appear across 7 carts”

Why:

DreamEngine promotes proven patterns. This tool helps notice when something has earned promotion.
