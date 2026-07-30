# Fixture — lint-docs known answers

## Links

A link that resolves: [`design/owner-doc.md`](design/owner-doc.md). EXPECT: silent.

A link that does not: [`design/zzz-missing.md`](design/zzz-missing.md). EXPECT: ERROR broken link.

## Section refs

An exact hit: [`design/owner-doc.md`](design/owner-doc.md) §8.1. EXPECT: silent.

A ref that resolves only via its parent: [`design/owner-doc.md`](design/owner-doc.md) §8.9.
EXPECT: SOFT note, never an error. This distinction is the subtle one — a stub/parent hit is a
legitimate pointer at a section that has not been subdivided yet.

A ref with no section and no parent: [`design/owner-doc.md`](design/owner-doc.md) §99.
EXPECT: ERROR — no such section.
