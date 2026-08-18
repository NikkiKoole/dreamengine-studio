# 029 — Every check we own compares the repo with itself

> This note captures a discovery made during the evolution of DreamEngine.
>
> It records our understanding at the time it was written.
> Later notes may refine, extend or replace it.

**Status**
Observed

**Date**
2026-08-18

**Confidence**
High

---

## Observation

The first full run of [`reality-audit-prompt.md`](../guides/reality-audit-prompt.md) found **68
verified false statements** about the state of the world, across `CLAUDE.md`, `docs/STATUS.md`,
`docs/HANDOFF.md`, 40-odd design docs, three tool headers and one cart's punch-list. Every
mechanical check in the repo was green before, during and after.

That is not a gap in the checks. It is their shape. `lint-docs` asks whether a link resolves.
`lint-carts` asks whether `index.json` matches the `de:meta` it was generated from. `status-check`
asks whether the ledger is internally consistent. Each of those is answerable **without leaving the
repo**, which is exactly why they are cheap, fast and trustworthy.

"Is this sentence still true?" is a different question. It needs the code, git history, or an
outside authority (App Store Connect, a live URL). No amount of link-checking sees a perfectly
formed sentence that is false.

---

## Why this matters

Two specific consequences, both observed rather than reasoned:

**1. A false sentence in a rules file is more expensive than a broken one.** `CLAUDE.md:635`
advertised "449 carts pending" for a migration that had finished, on a `--write` tool. A broken link
gets ignored. A confident open chore gets *acted on*. The same file said the `tenement` cart was "not
written yet" (it is 45 KB with a green 249-assertion spec) and that `voxel-bake` could not ship a
wider atlas (the cap became a default six days earlier, in the very work that tool exists for).

**2. The half-closed class is the subtlest and the most valuable.** A write-up that records fixing an
*instance* reads as a closed chapter, and a closed chapter is what stops the next person looking.
`CLAUDE.md` scored the per-instance header work "Done: all 8 headers that hold state" over a shelf
count that had since grown from 30 to 32, and two of the new ones hold mutable file-scope state with
no macro and no recorded exception. The scoreboard was the thing hiding them.

---

## Evidence

**The ledger deleted its own record of a shipped thing, and the class was never closed.**
`runtime/harmony.h` shipped 2026-07-20. Its *only* trace in `docs/STATUS.md` was the
`_Last updated:_` headline, and the 2026-07-30 headline compression (`6a060e09`, whose own message
is "the headline was being used as an entry, and it hid three shipped things") replaced that headline
and took the harmony record with it. `STATUS.md:12` documents this exact failure for three other
victims. Those three were fixed. The mechanism was not, so it happened again to a fourth and nobody
noticed for four weeks.

**The ledger denied a capability that 24 carts use.** `blend()` shipped 2026-07-10 under ADR-0031;
open item 18 still read "This is a real *capability* dreamengine lacks. Next step: ADR, after the
palette decision." `lint-capability-claims` exists precisely to catch that shape and was green,
for two compounding reasons: `STATUS.md` is carved out of both linters, **and** `blend` was absent
from `tools/capability-roster.js`. A capability missing from the roster is invisible to the check
built to police it.

**A correspondence check is the first check here that a stale clone can invert.** Mid-audit, a
parallel session in this shared clone ran `git pull --rebase` (nine commits behind) and committed
`acd69e02`, whose subject is *"I audited docs against a stale clone, so three of my own notes were
wrong."* Every other check in this repo is safe on a stale clone, because internal consistency
travels with the tree. A date-versus-content check is not: on a stale clone, a doc already fixed
upstream still reads as drifted, and a doc updated upstream reads fresher than it is. Four running
agents had to re-verify every finding against HEAD, and one retracted a finding as a result.

**Giving absence its own agent is what made it visible.** Five agents were partitioned by subject and
one was given the silent-ledger shape as its only job, told that an empty result was a reason to look
harder and that it had to *prove* absence by naming the search terms it ran. It returned 11 ledger
gaps. Subject agents optimising for a list find false sentences all day; nobody stumbles onto a
missing entry.

---

## Implications

- **Fill `capability-roster.js` before writing another linter.** It covers 29 capabilities against 396
  API functions. Every gap in it is a hole in a check that already exists.
- **A prose count with no `de:driftable` marker is structurally invisible.** Four snapshots are
  registered repo-wide; this audit found roughly a dozen unmarked stale counts in one pass. The
  mechanism exists and is simply unused.
- **If a date check is ever built, it must assert the clone is current and refuse to report otherwise.**
  Exiting nonzero with "clone is N commits behind origin" is more useful than findings it cannot
  trust. That asymmetry belongs in its header, because it is the first check here to have it.
- **When a table row gets a "✅ since <date>" stamp, the unstamped rows are the audit surface.** That
  is a convention, not a tool. Two findings were found exactly that way, and no check could have.
- **Absence detection has a hard recall ceiling, and saying so beats a tool that goes quietly blind.**
  The four mechanizable shape-B comparisons would have caught roughly 5 of the 11 ledger gaps. They
  would have caught none of the per-instance refactor, MIDI OUT, host MIDI notes, or the tool and ADR
  gaps, because a new tool, a new gate, a vendored dependency, a passed spike and a maker's verdict
  leave no symbol, no doc phase and no file.

---

## Open questions

- Should `status-check.js` grow an advisory correspondence tier (an `## Open` item naming a symbol
  that now exists in `studio.h` would have caught the `blend` and `device_class` cases), accepting
  that it stops being a purely internal-consistency checker?
- Should the `## Open` carve-out be removed from `lint-capability-claims` instead? That is cheaper
  than any new check and would have caught the highest-ranked finding in the audit.
- The audit rule is "report, do not edit", because a confident wrong correction destroys the trail.
  It held: the maker read the list and decided one thing (the palette gate is void) that no agent
  could have decided. How much of the second pass can be delegated without reintroducing that risk?

---

## Related notes

- 007-the-evolution-of-documentation
- 012-self-describing-artifacts
- 021-status-labels-cannot-be-linted
- 100-first-synthesis
