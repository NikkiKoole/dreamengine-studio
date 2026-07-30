# STATUS — synthetic fixture for status-check --selfcheck

> Not a real ledger. Every entry below exists to pin ONE known answer, including the two
> false-positive shapes that fooled the first version of the check (items 1 and 40 here).
> If you change a heuristic and this file's expectations still pass, the change is safe.

_Last updated: 2026-01-02 — short headline, well under budget._

---

## Shipped ✓

- **A dated changelog entry** (2026-03-02) — newest, so ordering is fine here.
- **An older dated entry** (2026-03-01) — correctly reverse-chronological after the one above.
- **An entry with no date at all** — EXPECT: undated.
- **An out-of-order entry** (2026-04-01) — newer than the one above it. EXPECT: unsorted.

**Capability inventory**

- **An undated inventory bullet** — a thing, not an event. EXPECT: NO undated finding (inventory half).
- **A dated bullet stranded in the inventory** (2026-03-15) — EXPECT: log-in-inventory (once, aggregated).

## Open

1. **A genuinely open item citing existing API** — collision already SHIPPED as `boxes_touch()`, but
   the open work here is teaching and discoverability. EXPECT: NO done-in-open. (This is the item-1
   shape that the first version of the check got wrong.)
2. ~~**A struck, fully-shipped item**~~ — **SHIPPED 2026-03-02**. EXPECT: done-in-open, MOVE.
3. **A shipped item that still has a tail** — ✓ **SHIPPED 2026-03-02.**
   **Still open:** the one thing left. EXPECT: done-in-open, SPLIT.
5. **A numbering inversion** — appears after item 3 but before item 4. EXPECT: numbering.
4. **The out-of-order sibling** — EXPECT: no finding of its own.
6. **A long entry with no owning design doc** — EXPECT: too-long.
   line 2 of filler
   line 3 of filler
   line 4 of filler
   line 5 of filler
   line 6 of filler
   line 7 of filler
   line 8 of filler
   line 9 of filler
   line 10 of filler
   line 11 of filler
   line 12 of filler
   line 13 of filler
   line 14 of filler
   line 15 of filler
   line 16 of filler
   line 17 of filler
   line 18 of filler
   line 19 of filler
   line 20 of filler
   line 21 of filler
   line 22 of filler
   line 23 of filler
   line 24 of filler
   line 25 of filler
   line 26 of filler
7. **A long entry that DOES link its owning doc** — see [`design/api-notes.md`](design/api-notes.md).
   EXPECT: no finding. Length alone is not a defect once the rationale has a home.
   line 3 of filler
   line 4 of filler
   line 5 of filler
   line 6 of filler
   line 7 of filler
   line 8 of filler
   line 9 of filler
   line 10 of filler
   line 11 of filler
   line 12 of filler
   line 13 of filler
   line 14 of filler
   line 15 of filler
   line 16 of filler
   line 17 of filler
   line 18 of filler
   line 19 of filler
   line 20 of filler
   line 21 of filler
   line 22 of filler
   line 23 of filler
   line 24 of filler
   line 25 of filler
   line 26 of filler
8. **A dead pointer** — `hud()` was cut (see Decided-against). EXPECT: dead-pointer.
40. **Spatial audio v3 — acoustic zones** — v1 (per-voice) + v2 (emitter buses) SHIPPED; the remaining
    layer is environment. EXPECT: NO done-in-open. (This is the item-40 shape the first version of the
    check got wrong: an OPEN item named for a later version, noting the earlier ones landed.)

## Decided-against / deferred ✗

- **Something cut** (2026-02-01) — dated, so no finding.
