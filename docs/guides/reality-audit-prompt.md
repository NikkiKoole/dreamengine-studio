# Reality audit: the prompt

A reusable brief for an agent whose job is to find **docs that are confidently, checkably wrong
about the state of the world**. Paste the block below into a fresh session.

Born 2026-08-17, when one conversation tripped over five instances of this class in an hour: a
handoff lane saying "APP IN REVIEW" about an app that had been on sale for weeks, a ledger with no
entry for that app at all, a lane dated two days before the work it described, a design doc carrying
a "the fix shape, if we take it" proposal three paragraphs below the record of that fix shipping, and
a second doc calling the live app "already an app in review". Every repo linter was green throughout.
That is not a coincidence: see "why nothing catches this" below.

---

## Why nothing catches this

Every check in this repo asserts **internal consistency**. Links resolve. Section refs resolve.
`index.json` matches the `de:meta` it was generated from. Cart metadata is valid. The compendium is
not stale. Those are good checks and they are all answerable without leaving the repo.

The class here is different: it is **correspondence to reality**. Is this sentence still TRUE, given
what the code now does, what git says happened, and what the outside world (App Store Connect, a
shipped binary) reports? No amount of link-checking sees a perfectly-formed sentence that is false.

Two tools already reach partway across this line and are the models to imitate rather than duplicate:
`lint-capability-claims.js` (docs denying a capability we now ship) and `wants-check.js` (a
`*-wants.md` row that its own dependency column says is unblocked). Both earn their precision from
hard discriminators, and both are documented in `CLAUDE.md`. Read them before proposing a third.

---

## The prompt

> **Your job: find every place in this repo where a document states something about the state of the
> world that is no longer true, and prove it.** Not broken links. Not typos. Claims.
>
> Run `node tools/orient.js` first for the lay of the land, and read `CLAUDE.md` for the rules
> (especially: never branch, commit by explicit pathspec, and a grep finds candidates that you then
> have to READ).
>
> ### What already covers adjacent ground. Do not redo these.
>
> | tool | what it already finds |
> |---|---|
> | `lint-docs.js` | broken md links, dead §-refs, tools/headers missing from the CLAUDE.md index |
> | `lint-xrefs.js` | docs that should cross-link and do not |
> | `stale-doc-check.js` | docs citing a code path that once existed and is gone; the `--driftable` snapshots |
> | `lint-capability-claims.js` | docs saying we CANNOT do something we now ship |
> | `wants-check.js` | a `*-wants.md` row whose own "unblocked by" column says it is ready |
> | `status-check.js --check` | STATUS.md's internal drift (a DONE marker inside Open, ordering, length) |
> | `handoff.js --check` | a lane >2wk old, or whose body was edited after its header date |
> | `build-*.js --check` | generated pages that are stale vs their source |
>
> Run each of them once so you know what is ALREADY reported. A finding that one of these prints is
> not yours.
>
> ### The four shapes to hunt
>
> **A. A status claim overtaken by events.** Prose written in a tense that has since expired: "in
> review", "next", "if we take it", "the fix shape would be", "not wired yet", "still open", "WIP",
> "blocked on X", "coming", "once Y lands". Each was true when written. The question is only whether
> it still is. This is the largest and easiest shape; it is also where most of the false positives
> live, so read the exemptions below before reporting one.
>
> **B. A silent ledger.** The inverse, and harder, because there is no sentence to find: something
> SHIPPED that `docs/STATUS.md` has no entry for. Look for shipped things in git history, in `apps/`,
> in the tools list, and ask whether the ledger knows. The instance that started this: the first app
> this project ever sold went live and the ledger whose entire job is "shipped vs open vs cut" said
> nothing at all, for weeks, while every check stayed green.
>
> **C. A date that contradicts its own content.** A doc's `_Last updated:_`, a lane header, a
> `STATUS:` line, or an "as of" that is older than the newest thing the same document describes.
> `git log`/`git blame` on the file answers this directly, and `handoff.js --check` now does it for
> HANDOFF lanes only. Every other dated doc is unguarded.
>
> **D. A half-closed class.** A doc records "we fixed this by DERIVING the per-app value", and a
> sibling value in the same file is still hardcoded, unmentioned. The doc is not wrong about what it
> says; it is wrong by omission, and it reads as a closed chapter. This is the subtlest shape and the
> most valuable, because the closed-sounding write-up is exactly what stops the next person looking.
> Cue to hunt for: any doc that describes fixing an INSTANCE of something, where you can check
> whether the CLASS was closed.
>
> ### How to verify. This is most of the work.
>
> A candidate is a grep hit. A **finding** cites both sides:
>
> 1. The claim, as `path/to/doc.md:LINE`, quoted.
> 2. The contradicting evidence, as `file:line` in code, a git commit, or a named command with its
>    output. Read the code; do not infer it from a symbol name. Where an external system is the
>    authority, ask it: `node tools/asc-push.js <app> --metadata --dry-run` reports every App Store
>    version and its state, and its refusal ("no editable App Store version") IS the answer.
> 3. The one-line fix.
>
> **Report nothing you have not verified both sides of.** The precision bar is set by this repo's own
> history: `stale-doc-check`'s broken-reference tier once sat at 47 findings with a 0% true-positive
> rate, every one a proposal that had never been built, until a discriminator landed. A wrong finding
> here is expensive in a specific way: it sends somebody to "correct" a document that was right.
>
> ### Exemptions. Without these the report is noise.
>
> - **Deliberately historical prose.** This repo keeps superseded reasoning on purpose, and says so:
>   "▼ superseded, kept for the trail", "this block is factually WRONG and kept only so the trail of
>   how it was believed is readable", struck-through text. Marked history is not drift. It is the
>   record working as intended.
> - **A nearby acknowledgement.** If the same paragraph or the next one says the thing shipped, the
>   doc is not lying to anyone. This is the escape hatch `lint-capability-claims` uses and it should
>   be yours.
> - **ADRs.** `docs/decisions/*` record a decision AT A POINT IN TIME. An ADR describing the world as
>   it was when the call was made is correct by construction. Only a *superseded* ADR that does not
>   say so is a finding.
> - **Rejected options.** A design doc weighing option C and rejecting it is not claiming C exists.
> - **Fixtures and test data.** Everything under `tools/fixtures/` is deliberately wrong; that is what
>   it is for. Same for a negative control.
> - **A doc that scopes its own claim** ("as of 2026-07, this was...").
>
> ### Output
>
> A ranked work list, most consequential first, each entry: the claim (`doc:line`, quoted), the
> evidence, the fix. Rank by **what a reader would DO wrong** if they believed it. A stale sentence
> nobody acts on ranks below one that would send an agent to rebuild something that exists, or stop
> them shipping something that is ready.
>
> Then two short sections:
>
> - **Mechanizable?** Which of shapes A to D could become a linter with a `--selfcheck` known-answer
>   fixture, and what the discriminator would have to be to keep its true-positive rate up. Be
>   honest about the ones that cannot: a check that judges prose has a recall ceiling, and saying so
>   is more useful than a tool that goes quietly blind. If a shape is mechanizable, note whether an
>   existing tool should grow the check rather than a new tool being born.
> - **What you could not check.** Anything whose truth needs a credential, a device, a build you
>   could not run, or the maker's knowledge. Name it as a question for them rather than guessing.
>
> **Report, do not edit.** A confident wrong correction to a doc is worse than the stale sentence,
> because it destroys the trail. The fixes get made in a second pass, once the maker has read the
> list.
>
> Scope: `docs/` is ~257 files plus `CLAUDE.md`, every tool header comment, and the `de:meta` blocks
> in `tools/carts/`. If you are running with parallel agents, partition by area (engine/audio,
> store/apps/iOS, carts, tooling, process docs) rather than by file count, so each agent holds one
> coherent subject and can judge what "still true" means in it.

---

## Notes for whoever runs this

- **It pairs with `orient.js`.** Run the audit against a repo whose mechanical checks are already
  green, or you will spend the budget rediscovering what `repo-doctor` prints in three seconds.
- **The findings belong in different homes.** A wrong sentence gets fixed in place. A missing ledger
  entry goes to `STATUS.md`. A recurring shape becomes a check with a `--selfcheck`, per
  [`checks-and-oracles.md`](checks-and-oracles.md). Resist putting any of it in `CLAUDE.md`.
- **Expect shape B to be under-reported.** Absence is much harder to see than a false sentence, and
  an agent optimizing for a long list will find shape A all day. If the report has no shape-B
  findings, that is a reason to ask again, not evidence there are none.
