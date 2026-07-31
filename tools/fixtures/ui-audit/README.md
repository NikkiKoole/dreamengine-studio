# ui-audit fixture — known answers

`node tools/ui-audit.js --selfcheck` asserts the checker against these. 31 assertions, and it
compiles and runs **nothing**: `analyze()` is pure (per-frame draw records in, findings out), so
the fixture is a synthetic record set rather than a cart.

**Why it needs one.** Every check is a geometric judgement wrapped in an exempt class: text is
off-screen *unless* it sits inside a `clip()`; two labels overlap *unless* the strings are equal;
a widget pair is piled *unless* the overlap is ≤3px; a finding counts *unless* it lasted fewer
than `minFrames`; and text behind a **later** fill is discounted entirely. Wrong in one direction
and the tool floods, so someone stops running it. Wrong in the other and it reports a clean UI
while a control sits off the edge of a phone screen.

**And the waiver subsystem had no coverage at all.** `grep -l ui-audit-ignore tools/carts/*.c`
returns nothing, so identity matching, side handling, stale detection and malformed detection had
never once fired in production. Eight of the 31 assertions are its only test.

## Files

| file | role |
|---|---|
| `findings.jsonl` | 6 frames carrying one instance of every finding kind **and** every exempt class, plus one 1-frame transient in frame 2 |
| `clean.jsonl` | 4 frames of a tidy layout that must yield zero findings |
| `waivers.c.txt` | a stand-in cart source with every `// ui-audit-ignore` form, including two that must **not** work |

Screen is 320×200 throughout. Entry kinds match the `--uiaudit` log: `t` = text (`t` is the
string, `c:1` = inside a `clip()` scissor), `f` = fill, `w` = `ui.h` widget. **Array order is
draw order**, which is what the occlusion rule reads. The leading `_readme` line in each `.jsonl`
is skipped by the loader exactly as `run()` skips unparseable lines.

## The expectation table

| entry | verdict | pins |
|---|---|---|
| `OFFRIGHT` / `OFFLEFT` | reported | the edge test, and that the **side** is named right |
| `CLIPPED` | exempt | inside `clip()` = bounded on purpose |
| `BEHIND` | exempt | fully covered by a **later** fill (text-behind-a-panel) |
| `WIDGETLABEL` | **reported** | drawn *after* its covering fill, so it survives occlusion. The counterpart to `BEHIND`: together they pin that occlusion is **draw-order sensitive**, not just containment |
| `AAA` ∩ `BBB` | reported | an overlapping text pair |
| `SAME` ×2 | exempt | identical strings are the same label drawn twice, not a collision |
| `ZOOM` ∩ `ALPHA` | reported | see below |
| widget pair at (30,160) | reported | overlap 20×10px, both axes past the threshold |
| widget pair at (120,150) | exempt | adjacency, 0px overlap |
| widget pair at (168,150) | exempt | 2px overlap, under the `>3` threshold |
| widget at (300,175) | reported | a control past the right edge is unreachable |
| widget at (-1,-1), 2×2 | exempt | sub-3px sliver |
| `TRANSIENT` (frame 2 only) | hidden, **counted** | the persistence filter, and `--min-frames 1` revealing it |

## Two fixture shapes that mutation-testing forced

**`ZOOM` is drawn before `ALPHA`, i.e. reverse-alphabetical.** The overlap waiver sorts both the
waiver's pair and the finding's `(a,b)` before comparing. The `AAA`/`BBB` pair happens to be drawn
in sorted order already, so an **order-sensitive** comparison still matched it and the mutation
that removed the sort scored a clean 30/30. A pair whose draw order disagrees with alphabetical
order is what actually proves it. Both the finding side and the parse side are now covered.

**`clean.jsonl` must contain the shapes the checks look at.** Asserting that *nothing* is
reported is the easiest thing to satisfy by accident, so `--selfcheck` additionally asserts the
frames hold ≥4 text entries, ≥4 widgets, a fill, and a string flush to `x+w == 320` (at the edge
but not past it). Otherwise trimming this fixture would quietly turn the cry-wolf guard into a
test that an empty file has no bugs.

Confirmed by mutation: dropping the `clip()` exemption, dropping the occlusion rule, making
occlusion order-insensitive, dropping the identical-string exemption, moving the widget threshold
from `>3` to `>0`, dropping the sliver filter, dropping the persistence filter, hardcoding
`minFrames`, ignoring the waiver side, comparing overlap pairs order-sensitively (either side),
dropping the fired-waiver tracking, and silently discarding malformed waivers each turn 1–2
assertions red.

The refactor that made this possible (extracting `analyze()` out of the module body) was verified
byte-identical against the pre-refactor tool on two real carts, one clean and one with a live
finding.

See `docs/guides/checks-and-oracles.md` → "Self-test the checker".
