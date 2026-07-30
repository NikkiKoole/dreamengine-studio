# Fixture — handoff.js --selfcheck known answers

Not a real handoff. `__TODAY__` and `__ANCIENT__` are substituted with real dates by `--selfcheck`,
so this file never rots. Link targets point at REAL docs, because the checker resolves them against
`docs/`, exactly as in production.

> **▶ ACTIVE THREAD (__TODAY__) — a clean lane.**
> Everything resolves. **Resume-at:** [`VISION.md`](VISION.md#the-idea).
> EXPECT: no finding.

> **▶ ACTIVE THREAD (__ANCIENT__) — a stale lane.**
> **Resume-at:** [`VISION.md`](VISION.md#the-idea).
> EXPECT: stale.

> **▶ ACTIVE THREAD (__TODAY__) — a lane with a broken doc link.**
> Points at [`zzz-no-such-doc.md`](zzz-no-such-doc.md#anywhere).
> **Resume-at:** [`VISION.md`](VISION.md#the-idea).
> EXPECT: broken link.

> **▶ ACTIVE THREAD (__TODAY__) — a lane with a broken section anchor.**
> **Resume-at:** [`VISION.md`](VISION.md#zzz-no-such-heading).
> EXPECT: broken #section.

> **▶ ACTIVE THREAD (__TODAY__) — a lane with no pick-up point at all.**
> Just prose, no pick-up label of any kind.
> EXPECT: the missing-pick-up-point finding. (This annotation deliberately avoids spelling the
> label, because naming it here would satisfy the very check being tested.)

> **▶ ACTIVE THREAD (__TODAY__) — a lane whose Resume-at has no anchor.**
> **Resume-at:** [`VISION.md`](VISION.md) — a bare doc link.
> EXPECT: unanchored (this is what made the anchor check INERT for a third of the real lanes).

> **▶ ACTIVE THREAD (__TODAY__) — REGRESSION GUARD: anchor a few lines BELOW the label.**
> **Resume-at:** work the queue in order —
> 1. the first thing,
> 2. the second thing, described in [`VISION.md`](VISION.md#target).
> EXPECT: no finding. Requiring the anchor on the label line reported these as unanchored.

> **▶ ACTIVE THREAD (__TODAY__) — REGRESSION GUARD: label mid-bold.**
> **Status + what's-left — Resume at** [`VISION.md`](VISION.md#references).
> EXPECT: no finding. Requiring the label right after `**` reported these as having none.

> **▶ ACTIVE THREAD (__TODAY__, later the same day) — REGRESSION GUARD: qualified date.**
> **Resume-at:** [`VISION.md`](VISION.md#the-idea).
> EXPECT: parsed as a lane AT ALL. A strict `(YYYY-MM-DD)` made this invisible — uncounted, and
> permanently exempt from the very staleness check meant to surface a forgotten lane.

> **▶ ACTIVE THREAD (__TODAY__) — REGRESSION GUARD: drifted lowercase spelling.**
> See resume at [`VISION.md`](VISION.md#the-idea) for the pick-up point.
> EXPECT: no finding. A checker that sees only one spelling is worse than useless.
