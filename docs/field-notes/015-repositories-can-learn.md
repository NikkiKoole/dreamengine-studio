# 015 — Repositories Can Learn

> This note captures a discovery made during the evolution of DreamEngine.
>
> It records our understanding at the time it was written.
> Later notes may refine, extend or replace it.

**Status**
Observed

**Date**
2026-07-01

**Confidence**
Medium–High

---

## Observation

As DreamEngine evolved, an unexpected pattern began to emerge.

The repository was no longer just storing knowledge. It was beginning to use it.

Early field notes often ended as ideas or documentation. More recent notes increasingly resulted in
new tools, conventions, generators, linters, or workflows. Each successful observation left behind
something permanent that made the repository easier to work with.

Knowledge was no longer simply recorded. It became operational.

---

## Discussion

Many software projects accumulate documentation over time. DreamEngine increasingly accumulates
capabilities.

A field note may begin with an observation about friction or repetition. That observation may
become an experiment. If successful, the experiment becomes a small tool, a convention, or an
automated process.

Examples include generated indexes, context assembly, tool discovery, self-describing artifacts,
documentation linting, and orientation tools.

The important change is not the individual tools. The important change is that the repository itself
slowly becomes a better collaborator.

Future contributors no longer need to remember every convention. The repository increasingly
explains itself:

- Metadata lives with the artifact it describes.
- Indexes are generated rather than maintained.
- Context is assembled rather than manually collected.
- Documentation becomes discoverable instead of hidden.

Every improvement reduces the amount of tribal knowledge required to work effectively.

---

## Implications

The repository should not only preserve knowledge. It should continually convert useful knowledge
into infrastructure.

Every recurring observation should prompt a simple question:

> Can the repository help with this next time?

If the answer is yes, the observation may deserve to become a tool, a generator, a convention, or an
automated check.

Over time, this creates a repository that gradually learns from its own development — not through
artificial intelligence, but through accumulated engineering decisions.

---

## Open questions

- Which observations deserve to become infrastructure?
- When should an experiment remain a field note rather than become a tool?
- How can the repository make its own capabilities increasingly discoverable?
- Can every recurring manual task eventually become part of the repository itself?

---

## Related notes

- 002-context-assembly
- 007-the-evolution-of-documentation
- 011-tool-discovery
- 012-self-describing-artifacts
- 013-generated-cart-index
- 100-first-synthesis

---

## Outcome

At the time of writing, several earlier field notes have already completed this cycle. Ideas around
context assembly, generated indexes, tool discovery, documentation quality, and self-describing
artifacts have resulted in concrete tooling and established workflows.

This suggests that field notes are becoming more than documentation. They are increasingly acting as
the research layer from which the repository evolves.
