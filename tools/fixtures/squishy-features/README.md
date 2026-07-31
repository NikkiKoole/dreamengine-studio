# squishy-features fixture — known answers

`node tools/squishy-features.js --selfcheck` asserts 21 known answers by feeding the checker
**synthetic** grid PNGs with known per-cell pixel differences. It compiles and runs nothing —
which is the point: rendering the squishy cart would make the answer depend on the very render
being audited.

`make-grid.js` is a **generator**, not committed `.png` blobs, because the interesting property of
these fixtures is *which cells differ, by how much, and under which PNG scanline filter*. That is
readable and editable as code; a binary blob in git is neither.

**Why it needs one.** Two layers here fail silently and print a plausible table either way:

1. **A hand-rolled PNG decoder** implementing all five scanline filters. Get Paeth or Average
   wrong and every cell diff is garbage, but the report still shows tidy numbers. Nothing else in
   the repo covers that decoder. The fixture encodes **one logical image under each of filters
   0–4** and demands identical diffs.
2. **Cell geometry and the `APPLIED_MIN` threshold.** An off-by-one in the cell origin silently
   compares the wrong rectangles, and a drifting threshold turns "this feature is a no-op for this
   brush" into "applied" with no visible symptom.

## What the 21 assertions cover

- **the PASS case** and that it is not passing blind (14 brushes × 7 features actually measured,
  and `CELL_CAP` agreeing between generator and checker)
- **all four verdicts** — `ok` · `MISS` · `inert` · `UNEXP` — plus each of `MISS` and `UNEXP`
  **in isolation**, so each is proven to be a failure on its own
- **the threshold at the boundary**: 11 changed pixels is not applied, 12 is
- **all five PNG filters** decoding identically, with the signature checked non-empty
- **cell origin**, **the 2px inset**, and **alpha being ignored** (`cellDiff` compares RGB only)
- **the layout guard**: a wrong-sized dump exits 2 rather than diffing at wrong offsets

## Three fixture shapes forced by mutation-testing

- **`bottom:` exists as its own generator option.** A cell is 22px tall but the header offset is
  only 12px, so a window shifted by the header still overlaps the *top* of a cell and a diff
  painted there is found anyway — dropping `MTX_HH` scored a clean 18/18. Painted against the
  cell's bottom edge, a shifted window misses it entirely.
- **`MISS` and `UNEXP` get their own single-defect grids.** The combined grid holds both, so
  either one alone still exits 1 and neither could be proven to matter individually.
- **the verdict logic had to be unified first.** `--json` originally recomputed `mark` and the
  failure count separately from the table, so two mutations that made the gate *never fail* still
  scored 21/21 — the selfcheck was reading a different copy of the answer than the report printed.
  That is exactly the "written in two places that must agree" hazard `lint-aux-params` exists for,
  reproduced inside a checker while fixturing it. Both now render from one `grid` computation, and
  the table output is byte-identical to before the refactor.

Confirmed by mutation: breaking each of the four non-trivial PNG filters, moving `APPLIED_MIN`
either way, dropping the inset, comparing alpha, dropping the row or column cell origin, accepting
a wrong-sized dump, and each way of making a verdict stop failing — every one turns 1–12
assertions red.

See `docs/guides/checks-and-oracles.md` → "Self-test the checker".
