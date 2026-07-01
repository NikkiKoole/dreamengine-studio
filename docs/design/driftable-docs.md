# Driftable docs — computed numbers frozen into prose, and how to keep them honest

STATUS: SHIPPED (2026-07-02) — the `de:driftable` convention + `stale-doc-check --driftable` + a
cart-status advisory. This note is the "why" and the how-to.

## The problem

Some docs are hand-authored **analysis** wrapped around a **snapshot of a tool's output** —
`teaching-gaps.md` freezes `api-usage.js` counts, `api-usage-audit.md` freezes a
functions×carts table, the profiling docs quote `profile-fleet.js` hotspots. The prose is real
hand work; the *numbers* are derived, and they rot the instant a cart lands. A dated caveat
("snapshot 2026-06-22, re-run before acting") does **not** save them — a specific,
authoritative-looking list still misleads a reader (we caught 6 of 18 "zero-use" functions in
`teaching-gaps.md` that already had callers; acting on it would waste a showcase cart).

## The hierarchy — prefer to *not need* a checker

Ranked best-to-worst; climb it by moving content up a tier where you can:

1. **Generate it.** If the content is derivable from source, don't write-and-check — *generate*
   it and gate with `--check` (`build-cart-index.js --check`, `build-compendium.js --check`,
   `studioDocs.js`). The doc is a *view* of the truth, so it can't drift. This is the repo's
   strong suit; reach for it first.
2. **Execute it.** For runnable claims — command examples, code snippets — *run* them (the
   doctest idea) so a removed flag fails a build, not a human's re-read. (The dead `--det` flag on
   `play.js` was one a run would have caught; see [`../guides/debug-harness.md`](../guides/debug-harness.md).)
3. **Lint the references.** Deterministic checks that literals resolve —
   [`../../tools/stale-doc-check.js`](../../tools/stale-doc-check.js)'s BROKEN REFERENCES tier
   (dead paths/flags), [`lint-docs.js`](../../tools/lint-docs.js), [`lint-xrefs.js`](../../tools/lint-xrefs.js).
4. **Register and review** (this note). When the number *can't* be generated in place but IS
   derived, the doc **declares** the dependency and an occasional check flags when the source
   moved. Curated, not inferred — high trust, cheap because the set is tiny.
5. **Accept as dated.** Prose-woven numbers ("the phaser hit 0.99999" mid-argument) can't be
   block-injected or diffed — they're part of the sentence. A `de:driftable` marker can still
   nudge "the source moved, re-read this," but no tool will ever fix the number itself.

## The `de:driftable` convention

A doc that freezes a tool's output declares it in an invisible HTML comment, **co-located with
the numbers**:

```
<!-- de:driftable cmd="node tools/api-usage.js" as-of="2026-06-22" -->
<!-- de:driftable watch="checklist,carts" -->   (a prime-only, cmd-less doc-level declaration)
```

- `watch` — WHAT can drift, from a fixed vocabulary (below). Defaults to `numbers` when a `cmd`
  is given. Only `numbers` is tool-verifiable; the rest are honest "watch this" metadata.
- `cmd` — the command that (re)produces the numbers. Re-run it to refresh; a reader can too.
- `as-of` — the snapshot date (`YYYY-MM-DD`). Omit and the doc lists but can't be drift-checked.
- `inputs` (optional) — comma-separated paths whose change means the numbers moved. **Default:**
  the tool's own script + `tools/carts` (the cart shelf, which almost all these tools read).
  Declare it when the doc depends on something else too, e.g.
  `inputs="runtime/studio.h,tools/carts"` (an API-surface count depends on `studio.h`).

### The `watch` vocabulary (owned by `stale-doc-check.js`)

Four kinds, on two axes — what drifts, and whether a tool can *verify* it:

| kind | what drifts | verifiable? |
|---|---|---|
| `numbers` | a frozen count / table / metric from a tool | ✅ via `cmd`+`as-of`+`inputs` (gets the ⚠ verdict) |
| `checklist` | done/undone task state, phase items (`[x]`/`[ ]`, "shipped") | ⚠ prime-only |
| `carts` | a claim that counts/enumerates carts ("16 carts have touch") | ⚠ prime-only |
| `decisions` | an open/proposed choice that may get settled | ⚠ prime-only |

`numbers` is the only kind a tool checks; the other three are surfaced (at orient time / in the
`--driftable` overview) as "watch this," but **no tool pretends to verify them** — they're a
*prime*, not a detector. Aim `decisions` at a fork settled *inside the prose* — the doc-level
phase (ready/building/shipped) is already tracked by `STATUS:` + the generated `design-board.html`,
so don't duplicate that. `stale-doc-check --driftable` warns on any `watch` value outside this set.

## How it's checked (occasional, curated, human-decides)

```bash
node tools/stale-doc-check.js --driftable          # the overview + which likely drifted
node tools/stale-doc-check.js --driftable --json    # machine-readable
```

For each marker it compares `as-of` against the newest last-commit date among the resolved
`inputs`. Inputs moved after the snapshot → **⚠ LIKELY DRIFTED** ("re-run cmd + eyeball"). It
never edits the doc and never guesses — a human declared the dependency and a human decides. It
reuses the git-date engine already in `stale-doc-check.js`; it is the *declared* (trustworthy)
twin of that tool's heuristic mtime tiers.

**Where it surfaces — two doors, bracketing a work session:**
- **Front door (prime, going in):** [`build-context.js`](../../tools/build-context.js) (hence
  `orient`) annotates each related doc it lists with its drift declaration —
  `[driftable: numbers ⚠ 28d stale]` / `[driftable: checklist, carts]`. So the agent orienting on
  a cart is primed: *if I change this cart, these home docs snapshot something I might move.* The
  agent who causes the drift is the one nudged, in-session, with context loaded.
- **Back door (catch, coming out):** [`cart-status.js`](../../tools/cart-status.js) runs the check
  as a non-gating advisory, so after a round of cart edits you're told which snapshot docs likely
  drifted, alongside NEEDS REBAKE / STALE PUBLISHED.

Both are soft nudges (advisory, never auto-fix, human decides). The front door raises the odds the
drift is fixed by whoever caused it; the back door catches what slipped through.

## When you write a doc — the rule

Adding a table or counts derived from a tool, that will change as the repo grows? Either move it
up the hierarchy (generate/execute), or drop a `de:driftable` marker next to it so the drift is
tracked instead of silently rotting. Don't add a bespoke generator per doc — the set is small and
clustered (two producer tools, a handful of consumer docs); the marker + one shared check is the
proportionate answer.

## See also
- [`../../tools/stale-doc-check.js`](../../tools/stale-doc-check.js) — the checker (header = full contract)
- [`teaching-gaps.md`](teaching-gaps.md), [`api-usage-audit.md`](api-usage-audit.md) — the current driftable docs
- [`../guides/checks-and-oracles.md`](../guides/checks-and-oracles.md) — the task→gate reverse index
