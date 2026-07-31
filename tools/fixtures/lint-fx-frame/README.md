# lint-fx-frame fixture — known answers

`node tools/lint-fx-frame.js --selfcheck` asserts the checker against these. 30 assertions
over six synthetic carts.

**Why this tool needed a fixture most.** It reported `✓ 0 findings across 573 carts` and was
wired into no gate — which is indistinguishable from a scanner that had gone blind. And the
thing behind that green tick is a hand-rolled C parser: a brace classifier, a statement
tracker, a comment/string stripper that must preserve byte offsets, and **six exempt classes**.

## The marker convention

A line carrying `@@FLAG@@` MUST be reported; every other line must NOT be. Each file is
therefore one **bidirectional** assertion (`reported lines == marked lines`) that cannot go
vacuous: a scanner going blind empties the result and fails the files *with* markers, one that
floods grows it and fails the files *without*.

| file | must report | pins |
|---|---|---|
| `flagged.c.txt` | 5 lines | the footgun itself: bare calls in `update()` **and** `draw()`, a multi-word FX name (`reverb_bus`), several in one file, a `for` body |
| `guarded.c.txt` | nothing | the six gated shapes — `if` block · inline `if` · ternary · `while` · `switch` case · `else` |
| `excluded.c.txt` | nothing | the ride-live list: `filter` `varispeed` `echo` `tremolo` `note_cutoff` `note_reverb` `note_vol` |
| `scope.c.txt` | nothing | only `update()`/`draw()` bodies are inspected — `init()` and helpers are not |
| `waiver.c.txt` | 1 line | `// fx-lint-ignore` trailing and standalone-above both waive, **and a trailing one does not leak down** onto the next statement |
| `noise.c.txt` | 1 line | comment + string stripping, a prefixed name (`my_crush`) not read as `crush`, and the reported line number surviving all of it |

## Two fixtures shaped by the "passes for the wrong reason" rule

`guarded.c.txt` and `excluded.c.txt` both assert that *nothing* is reported, which is the easiest
kind of assertion to satisfy by accident — an inert file passes no matter how broken the tool is.
So `--selfcheck` additionally asserts:

- **`guarded.c.txt` really does call real FX-set members** (`crush` `tape` `chorus` `flanger`
  `phaser` `wah` `eq`). Without that, a broken conditional detector would still score green.
- **`excluded.c.txt`'s calls sit in the exact shape `flagged.c.txt` gets reported for** —
  unconditional, top level of `update()`. The *only* thing keeping them silent is their absence
  from the FX set, which is what makes the case evidence about the exclusion list.

Confirmed by mutation: emptying the FX set, dropping the conditional-frame check, dropping the
inline guard, skipping comment stripping, dropping the waiver, letting a trailing waiver leak,
scanning `init()`, dropping `draw()`, shifting line numbers by one, and moving `filter` into the
FX set each turn 2–8 assertions red.

## What the fixture found

The header comment claimed a call inside a `for` block was exempt. It never was — `for` headers
contain semicolons, and `;` resets the statement tracker, so the keyword is gone by the time the
brace is classified (a `while (…) {` header has none and survives). The behaviour is the *right*
one — `for (i…) crush(i, …)` rebuilds the DSP N times a frame, which is the footgun worse rather
than an exemption — so the fix was to the prose, and it is now a documented contract instead of
an accident one cleanup away from being "fixed" into a blind spot.

See `docs/guides/checks-and-oracles.md` → "Self-test the checker".
