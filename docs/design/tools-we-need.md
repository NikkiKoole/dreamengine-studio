# Tool Ideas from the Field Notes Session

STATUS: BUILDING — living tool-idea backlog from the field-notes sessions; 5 of 6 shipped (build-context, build-field-notes, lint-carts, build-cart-index, stale-doc-check), 1 partial (promote-candidate).

**Progress (as of 2026-07-01):** 5 of 6 shipped, 1 partial.

| # | Idea | Status | Shipped as |
|---|------|--------|-----------|
| 1 | `build-context` | ✅ shipped | `tools/build-context.js` |
| 2 | `build-field-notes` | ✅ shipped | `tools/build-field-notes.js` |
| 3 | `cart-meta-check` | ✅ shipped | `tools/lint-carts.js` |
| 4 | `build-cart-index` | ✅ shipped | `tools/build-cart-index.js` |
| 5 | `stale-doc-check` | ✅ shipped | `tools/stale-doc-check.js` (cheap grep+git-date version) |
| 6 | `promote-candidate` | 🟡 partial | `tools/cart-index.js` + `tools/cart-dupes.js` |

## 1. `build-context` ✅ SHIPPED — `tools/build-context.js`

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

## 2. `build-field-notes` ✅ SHIPPED — `tools/build-field-notes.js`

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

## 3. `cart-meta-check` ✅ SHIPPED — as `tools/lint-carts.js`

Validate per-cart metadata once metadata moves out of one central index.
(Built as `lint-carts.js`: validates each cart's `de:meta` — tags/status/created/description
against the tag vocabulary — and asserts `index.json` is in sync with a fresh generate.)

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

## 4. `build-cart-index` ✅ SHIPPED — `tools/build-cart-index.js`

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

## 5. `stale-doc-check` ✅ SHIPPED — `tools/stale-doc-check.js`

Detect documentation that may need review after code, cart or ADR changes.

Shipped as the **cheap, zero-upkeep** version, deliberately *not* the `depends_on:` frontmatter
scheme sketched below (that needs a hand-maintained dependency graph that rots). Instead it uses
one mechanical signal that needs no maintenance: **a doc mentions an entity (a tool or another doc,
by hyphenated basename in prose) whose last git-commit date is NEWER than the doc's own** → the doc
may describe a stale version, so flag it for a re-read. Reconciling and committing the doc resets
its clock, so a flag clears the moment you look. Two confidence tiers — **TOOL DRIFT** (a doc
describing a tool whose code moved on; shown in full) and **DOC CHURN** (doc→doc mentions edited
later; collapsed to a count, `--docs` expands — mostly ordinary churn). Companion to `lint-docs.js`
(do links resolve?) and `lint-xrefs.js` (should a link exist?); this one asks *has the prose gone
stale?* The `depends_on:` frontmatter idea is left below as a possible future upgrade for
higher-precision, opt-in dependency tracking.

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

## 6. `promote-candidate` 🟡 PARTIAL — `tools/cart-index.js` + `tools/cart-dupes.js`

Find repeated patterns that may deserve promotion.
(Partially covered: `cart-index.js` gives the computed technique coverage / "appears in N
carts" clusters, and `cart-dupes.js` surfaces cross-cart duplication as refactor/promotion
candidates. No single tool frames the output as promotion recommendations yet.)

It should scan carts, metadata and docs for recurring concepts.

Example outputs:

* “graph routing appears in 5 carts”
* “audio modulation appears in 4 carts”
* “state-machine appears in 19 carts”
* “road geometry concepts appear across 7 carts”

Why:

DreamEngine promotes proven patterns. This tool helps notice when something has earned promotion.
