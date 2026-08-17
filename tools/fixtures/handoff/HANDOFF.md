# Fixture — handoff.js --selfcheck known answers

Not a real handoff. `__TODAY__`, `__RECENT__` and `__ANCIENT__` are substituted with real dates by
`--selfcheck`, so this file never rots. Link targets point at REAL docs, because the checker resolves
them against `docs/`, exactly as in production.

The two `edited` lanes below are driven by `DE_HANDOFF_EDITED`, the test-only injection point for
"when was this lane's body last touched" (production reads `git blame`, which cannot see a fixture
written to a temp dir).

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

> **▶ ACTIVE THREAD (__RECENT__) — a lane edited after its date.**
> **Resume-at:** [`VISION.md`](VISION.md#the-idea).
> EXPECT: edited-after-date, and NOT stale-by-age. The date is recent enough to clear the 14-day
> bar, which is the point: this is work that HAPPENED and was not recorded, and the age check is
> structurally blind to it — the lane looks fresher than the thing it describes.

> **▶ ACTIVE THREAD (__TODAY__) — CONTROL: a lane edited before its date.**
> **Resume-at:** [`VISION.md`](VISION.md#the-idea).
> EXPECT: no finding. Someone refreshed the date without touching the body (verifying a lane is
> still live is a real action). If this one ever reports, the check is firing on the mere PRESENCE
> of edit history rather than on the comparison, and every lane in the file would flag.
