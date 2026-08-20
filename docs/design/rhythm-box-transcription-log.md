# Rhythm-box patterns: the transcription log

> **STATUS: LIVING (2026-08-20)** ‑ the raw evidence trail behind
> [`rhythm-box-patterns.md`](rhythm-box-patterns.md). Kept verbatim on purpose.

This is the working record of reading 64 preset rhythms off three manufacturer documents on
2026-08-19, by the main session and a fleet of subagents. It is deliberately **not** cleaned up.
Its value is that every number in the curated doc can be traced to a measurement here, including:

- the grid fits, with residuals, per document and per block
- the readings that were **rejected**, and why (a ring-score detector that found 32 hits in a lane
  that has 8; two lanes that looked like sustained bars until the lane lines were re-located; a
  half-column page shear that a first least-squares fit hid)
- the negative controls (unruled counts as a noise floor, count-0 checks against a column where no
  label can reach, bar-2 twins corroborating bar-1 reads)
- what was left UNREAD rather than guessed

Fourteen subagents were killed by a stall watchdog during the session. Nothing was lost after the
second, because each one appended to disk after every step and handed its calibration to its
replacement rather than having it re-derived. The seams where one reader handed off to another are
visible below and are left in.

If you are looking for the patterns themselves, read the curated doc instead. If you are checking
whether a specific cell is trustworthy, you are in the right file.

---
# ===== source file: fr2l_left.md =====

# Ace Tone Rhythm Ace FR-2L: preset rhythm patterns (LEFT table)

## Provenance
Document: "ELECTRONIC RHYTHM INSTRUMENT RHYTHM ACE model FR-2L SERVICE NOTE",
          Ace Electronic Industries Inc. (Japan), c.1969.
Item:     https://archive.org/details/RhythmAceFR2LServiceManual
PDF:      https://ia601600.us.archive.org/20/items/RhythmAceFR2LServiceManual/Rhythm%20Ace%20FR-2L%20Service%20Manual.pdf
Figure:   page 14, Fig 10 ("rhythm x instrument" dot chart), LEFT of the two tables.
          page 13, Fig 9 ("LOGIC OUTPUT TIMING CHART") defines the 35 numbered pulse
          trains that Fig 10's Code column refers to.
Read by:  main session, by eye against a computed count grid drawn onto a 600 dpi
          render, one bar at a time (each count ~80 px apart in the read image).
          Circle detectors were tried and REJECTED: the embedded scan is low
          resolution, so 600 dpi is a block upscale and both Hough and a ring-score
          detector over-detect (they found 32 hits in a row that has 8).

## Grid
The machine counts 48 over TWO BARS: 24 counts per bar, 6 counts per beat.
So eighth-note triplets are native and SIXTEENTHS DO NOT EXIST on this grid.
Beat 1 of bar 1 sits at x=1091 in the 600 dpi render, beat pitch 232.5 px,
count pitch 38.75 px.
Rows below are 24 characters for bar 1, two spaces, 24 characters for bar 2.
'x' = a circle in the chart, '.' = empty.

## Rhythms

### Bossanova
Row labels exactly as printed. Codes from the chart's own Code column.
```
Cy      code 25   ......x...........x.....  ......x...........x.....
Cy'     code  5   x..x..x..x..x..x..x..x..  x..x..x..x..x..x..x..x..
Cb+Hb   code 17   x........x........x.....  ......x........x........
Lc+Bd   code  2   x...........x...........  x...........x...........
```
CONFIDENT. Cb+Hb is the bossa clave proper, and it is genuinely asymmetric across
the two bars (bar 1 on 1, the "and" of 2, and 4; bar 2 on 2 and the "and" of 3).
That asymmetry is the thing a one-bar 16-step grid cannot represent.
NOTE: an earlier, less magnified read of this same block MISSED the count-0 hit in
the Cb+Hb row, mistaking it for the row label. The zoomed per-bar read found it.

### Waltz
```
Cy      code  1   x.......................  x.......................
M       code 34'  ........x.......x.......  ........x.......x.......
Sd      code 39   ........x.......x.......  ........x.......x.......
Bd      code  1   x.......................  x.......................
```
CONFIDENT (both bars read separately and they are identical, so the pattern is one
bar long and the chart simply shows it twice).
STRUCTURAL FINDING: the waltz divides the SAME 24-count bar by THREE, not four, so
its beat is 8 counts, not 6. The chart's printed "1 2 3 4" beat numbers along the
top are the 4/4 labels and DO NOT apply to this row block. Any player of this data
needs a per-rhythm beats-per-bar, not a global one.
CODES: the Code column is printed small; 34' and 39 are legible but not certain.
Treat the code numbers as provisional and the dots as the data.

### Dixieland
```
B       code 34   [GATE, see note]          [GATE, see note]
(row 2) code ?    ........................  ................x.......
Sd      code  5?  x.....x.....x.....x.....  x.....x.....x.....x.....
Bd      code  3?  x.....x.....x.....x.....  x.....x.....x.....x.....
```
CONFIDENT on Sd and Bd (both on all four beats) and on the gate lane's extent.
The B lane is NOT dots: it is a thick sustained BAR, held from count 6 to 12 and
from 18 to 24 in bar 1, and 30 to 36 and 42 to 48 in bar 2. That is a GATE one beat
long on beats 2 and 4 of each bar, i.e. a held brush swish rather than a struck hit.
Its own row in Fig 9 (code 34) is drawn the same way, as bars not pulses, so the
machine really does have sustained lanes as well as trigger lanes. Any player of
this data needs a gate concept, not only hits.
UNCERTAIN: row 2's printed label is illegible at this scan resolution (it reads as
something like "Bd'Sd'"), and it carries exactly ONE hit in the whole two bars, on
count 40. A single hit late in bar 2 is a pickup/fill, which is plausible, but the
label is unread so the voice is unknown.
CODES for Sd and Bd are marked with '?': the Code column is printed small and code 5
elsewhere in this chart means "every eighth", which does not match these rows (they
are on beats only). So either my code reading or the column alignment is wrong here.
The DOTS are the data; do not rely on these two code numbers.

### Western
```
C+Hb    code 31'  ............x...........  ............x...........
Hc      code 32   ........x...........x...  ........x...........x...
Lc'Bd   code  1   x.......................  x.......................
```
CONFIDENT. Both bars read separately; bar 2 is bar 1 shifted by exactly 24 counts,
so the pattern is one bar long.
Reading in beats (6 counts each): bass on 1, C+Hb on 3, and Hc two counts after
beats 2 and 4. Two counts into a six-count beat is the SECOND EIGHTH-TRIPLET, i.e.
this rhythm is notated swung on the machine's own triplet grid, which is precisely
what a sixteenth grid cannot express: on 16ths that hit is neither the 8th nor the
16th, it falls between them.

### Rock'n Roll
```
Cy'     code  5   x..x..x..x..x..x..x..x..  x..x..x..x..x..x..x..x..
Sd      code 25+30 ......x..x........x.....  ......x..x........x.....
Bd'     code  3   x.....x.....x.....x.....  x.....x.....x.....x.....
Bd      code  1   x.......................  x.......................
```
CONFIDENT. Both bars read; bar 2 equals bar 1 shifted 24 counts, so one bar long.
STRUCTURAL FINDING: there are TWO bass-drum lanes, Bd' on every beat and Bd on beat 1
only, so beat 1 fires BOTH. The machine has no velocity, so an accent is made by
LAYERING two lanes of the same voice. That is worth copying: it is why these boxes
have a downbeat that sits forward without any dynamics at all.
This also pins three shared codes, each confirmed in more than one rhythm:
  code 1 = beat 1 of the bar only     (Waltz Cy and Bd, Western Lc'Bd, this Bd)
  code 3 = every beat                 (Dixieland Bd, this Bd')
  code 5 = every eighth               (Bossanova Cy', this Cy')
Sd's "25+30" is the chart's own notation for a row driven by TWO codes summed, which
is how the machine builds a pattern that no single pulse train provides.

### Slow Rock
Counts, as read (the whole 48-count cycle, not two bars of 24: see the note):
```
Cy'     code 2+9+33 x...x...x...x...x...x...  x...x...x...x...x...x...
Sd      code 31   ............x...........  ............x...........
Bd      code  1   x.......................  x.......................
```
CONFIDENT on the dots. Both halves read separately and each half is identical.
STRUCTURAL FINDING: this rhythm is 12/8, and it reads the counter differently from
every 4/4 rhythm above. Its eighth-triplet is 4 COUNTS and its quarter note is 12
counts, so ONE musical bar spans the chart's full 48 counts rather than 24. Read that
way it is the textbook slow-rock ballad: Cy' on all twelve triplets, Sd on beats 2
and 4 (counts 12 and 36), Bd on beats 1 and 3 (counts 0 and 24), and the rows above
are printed as two 24-count halves only because the chart's paper is ruled that way.
So the chart's 48 counts are a fixed clock that each rhythm SUBDIVIDES ITS OWN WAY:
  4/4 rhythms   bar = 24 counts, beat = 6 counts (two bars shown)
  Waltz         bar = 24 counts, beat = 8 counts (two bars shown)
  Slow Rock     bar = 48 counts, beat = 12 counts, triplet = 4 counts (one bar shown)
A player of this data needs counts-per-beat and pattern-length PER RHYTHM.

### Fox Trot
Five lanes read, top to bottom. Labels 1, 3 and the pair at the bottom are legible;
lane 2's label is not (it reads as something like "Bd'Sd'").
```
Cy      code 25   ......x...........x.....  ......x...........x.....
(lane2) code 33   ........................  ................x.......
Sd      code 25   ......x...........x.....  ......x...........x.....
Bd'     code  2   x...........x...........  x...........x...........
Bd      code  3   x.....x.....x.....x.....  x.....x.....x.....x.....
```
CONFIDENT on the dots. UNCERTAIN on lane 2's label and on which of the bottom two
lanes is Bd' versus Bd (the primes are at the edge of legibility).
FINDING, and it explains why the chart is two bars long at all: lane 2 fires ONCE in
48 counts, on count 40, and Dixieland's unreadable lane 2 does exactly the same thing
on exactly the same count. So there is a shared pulse train (code 33) that fires once
every TWO bars, late in bar 2, as a pickup into the repeat. The two-bar span is not
decoration: for the straight rhythms it exists to carry a once-per-two-bars FILL,
and for the latin rhythms it carries the asymmetric clave. Both reasons are real, and
neither survives being squashed into one bar.

### Swing
Five lanes: M, Cy, an unlabelled-to-me third lane, Sd, Bd.
```
M               ......x...x.......x...x.  ......x...x.......x...x.
Cy              x...........x...........  x...........x...........
(lane3)         ........................  ................x.......
Sd              ......x...x.......x...x.  ......x...x.......x...x.
Bd              x.....x.....x.....x.....  x.....x.....x.....x.....
```
CONFIDENT on the dots.
M and Sd play beats 2 and 4 plus the THIRD TRIPLET after each (counts 6 and 10, 18 and
22), which is the swing ride figure written on the machine's triplet grid. Cy marks 1
and 3, Bd walks all four beats.
Lane 3 fires once in 48 counts, on count 40, exactly like Dixieland's and Fox Trot's
mystery lane. That is now THREE rhythms sharing one once-per-two-bars pickup pulse,
which settles what it is: a fill lane, not a per-rhythm quirk.

## Summary of what the LEFT table gives
Eight rhythms, all read: Waltz, Dixieland, Western, Rock'n Roll, Slow Rock, Bossanova,
Fox Trot, Swing. Confident on every dot pattern; the only unresolved items are three
row LABELS at the edge of legibility and most of the Code column.

Five structural findings, each visible in more than one rhythm:
1. 48 counts, two bars, 6 counts per beat. Triplets native, sixteenths absent.
2. Each rhythm SUBDIVIDES that clock its own way: 4/4 rhythms take a 24-count bar with
   6-count beats, the Waltz takes a 24-count bar with 8-count beats, and Slow Rock
   takes the whole 48 counts as ONE 12/8 bar with 4-count triplets. So a player needs
   beats-per-bar and pattern-length per rhythm, not one global setting.
3. Accent is made by LAYERING, not velocity: Rock'n Roll has Bd' on every beat and Bd
   on beat 1, so beat 1 fires twice. Shuffle stacks three voices on every beat.
4. The two-bar span exists for a reason, and there are two different reasons: a shared
   fill pulse late in bar 2 (Dixieland, Fox Trot, Swing all fire on count 40), and a
   genuinely asymmetric two-bar figure (Bossanova's clave, March's bar-2 snare).
5. Swing placement is explicit and not uniform: the Shuffle's offbeat is the THIRD
   triplet (+4 of 6) while Western's is the SECOND (+2 of 6). The chart distinguishes
   them, so both are intended.
Plus: some lanes are sustained GATES drawn as thick bars (Dixieland's B), not hits.

> **LABEL CAVEAT:** lane labels in this file are PROVISIONAL, not sourced. See LABEL-CAVEAT.md:
> the scan is ~75 dpi, so a lane label is ~3x4 px per letter and the letterforms are interpolation.
> Circle POSITIONS and lane ORDER are solid; the letters are not.


# ===== source file: fr2l_right.md =====

# Rhythm Ace FR-2L — Fig 10 pattern chart, RIGHT TABLE

## Provenance

- Source: Ace Tone (Ace Electronic Industries), *"ELECTRONIC RHYTHM INSTRUMENT RHYTHM ACE model FR-2L
  SERVICE NOTE"*, c.1969. Image-only scan, 16 pages.
- Internet Archive item: https://archive.org/details/RhythmAceFR2LServiceManual
- PDF: https://ia601600.us.archive.org/20/items/RhythmAceFR2LServiceManual/Rhythm%20Ace%20FR-2L%20Service%20Manual.pdf
- Page 14, **Fig 10** ("per-rhythm pattern chart"). This file covers the **RIGHT table only**
  (Tango, Beguine, Rhumba, Samba, Mambo, Cha-Cha, Shuffle, March). The LEFT table
  (Waltz … Swing) is transcribed separately by another agent.
- Page 13 (Fig 9, "LOGIC OUTPUT TIMING CHART") used as the independent cross-check of the Code numbers.
- Render used: `pdftoppm -r 600 -f 14 -l 14 -png fr2l.pdf big` → `big-14.png`, 6667 x 4775 px.

## Derived geometry (RIGHT table)

The right table is a separate hand-drafted grid from the left one, so its numbers were derived
independently, from the eight heavy beat lines under the printed axis strip ("1 2 3 4 1 2 3 4",
crop y 600..880).

Measured beat-line x centres (600 dpi, big-14.png):

    4049.0  4278.5  4511.5  4749.0  4986.5  5224.0  5461.5  5695.0

Least-squares fit over the 8 beats:

    beat-1 x     = 4043.87
    beat pitch   = 235.857 px
    count pitch  = 39.310 px   (a beat = 6 counts, 48 counts over the two bars)
    count c x    = 4043.87 + 39.310 * c        for c = 0 .. 47

Fit residuals per beat line: +5.1 -1.2 -4.1 -2.4 -0.8 +0.8 +2.5 +0.1 px
(max 5.1 px = 0.13 of a count, so the grid fit is sound).

For comparison, the LEFT table is beat-1 x = 1091, beat pitch 232.5. The two tables are close but
not identical, as expected for hand drafting.

---

(transcription follows; sections appended as each rhythm is read)

## Rhythms (read in the main session, using the geometry above)

### Cha-Cha
```
M               x..x..x..x..x..x..x..x..  x..x..x..x..x..x..x..x..
Cb              x.....x.....x.....x.....  x.....x.....x.....x.....
Hb              x.....x.....x...........  x.....x.....x...........
Lc              ..................x..x..  ..................x..x..
Bd              x........x........x.....  x........x........x.....
```
CONFIDENT. Both bars read separately; bar 2 equals bar 1 shifted 24 counts, so the
pattern is one bar long.
This is a textbook cha-cha on the machine's own grid: maracas on every eighth, cowbell
on every beat, high bongo on beats 1-2-3 but NOT 4, and the bass on 1, the "and" of 2
(count 9) and 4. The low conga plays count 18 and 21, i.e. beat 4 and its "and", which
together with the next bar's downbeat is the "cha-cha-CHA" across the bar line.

### March
```
Sd              ......x...........x.....  ......x..x..x.....x.....
Cy              x...........x...........  x...........x...........
Bd              x...........x...........  x...........x...........
```
CONFIDENT. Three lanes only. Cymbal and bass are locked together on beats 1 and 3.
The two bars are NOT the same: the snare plays the plain backbeat in bar 1 (beats 2
and 4) and a fuller figure in bar 2 (beat 2, the "and" of 2, beat 3, beat 4). So this
is genuinely a two-bar pattern with a written-in bar-2 variation, which is the second
mechanism I have now seen for using the two-bar span (the other being a single pickup
pulse late in bar 2, shared by Dixieland and Fox Trot).

### Shuffle
```
Sd              ....x.....x.....x.....x.  ....x.....x.....x.....x.
Sd'             x.....x.....x.....x.....  x.....x.....x.....x.....
Cy              x.....x.....x.....x.....  x.....x.....x.....x.....
Bd              x.....x.....x.....x.....  x.....x.....x.....x.....
```
CONFIDENT. One bar long (bar 2 equals bar 1 shifted 24 counts).
This pins the machine's SWING PLACEMENT precisely: the shuffle's offbeat is at count
+4 within a six-count beat, i.e. the THIRD eighth-triplet, the classic long-short
shuffle. Two useful consequences:
  - it is unreachable on a sixteenth grid, where +4 of 6 falls between the 8th and the
    dotted 8th;
  - it makes Western's offbeat placement (+2, the SECOND triplet) a deliberately
    different feel rather than a mis-read on my part, since the same chart clearly
    distinguishes the two.
Sd' and Cy and Bd all land on every beat, so the shuffle's downbeat is three voices
thick: another instance of the accent-by-layering trick (the machine has no velocity).

### Tango
Five lanes. Labels read M, Cy, Sd, Sd' (the prime is at the edge of legibility), Bd.
```
M               x...........x...........  x...........x...........
Cy              .....................x..  .....................x..
Sd              .....................x..  .....................x..
Sd'             x.....x.....x.....x.....  x.....x.....x.....x.....
Bd              x.....x.....x.....x.....  x.....x.....x.....x.....
```
CONFIDENT on the dots (both bars read; one bar long). UNCERTAIN on whether lane 4 is
"Sd'" or another primed label.
Bass and Sd' drive all four beats (the tango's marcato), maracas mark 1 and 3, and the
only syncopation in the whole rhythm is Cy+Sd together on the last eighth of the bar
(count 21), the pickup into the downbeat.

> **LABEL CAVEAT:** lane labels in this file are PROVISIONAL, not sourced. See LABEL-CAVEAT.md:
> the scan is ~75 dpi, so a lane label is ~3x4 px per letter and the letterforms are interpolation.
> Circle POSITIONS and lane ORDER are solid; the letters are not.


# ===== source file: fr2l_beguine_rhumba.md =====

# FR-2L pattern chart: BEGUINE and RHUMBA

## Provenance
- Source: Ace Tone "RHYTHM ACE model FR-2L SERVICE NOTE", c.1969.
  archive.org item: https://archive.org/details/RhythmAceFR2LServiceManual
- Page 14 = Fig 10 (pattern chart). Page 13 = Fig 9 (35 numbered pulse trains).
- Working image: scratchpad/big-14.png, page 14 at 600 dpi (6667x4775). Not re-rendered.
- Grid (RIGHT table, pre-fitted, given): count c at x = 4043.87 + 39.310*c, c = 0..47.
  48 counts = TWO bars, 24 counts/bar, 6 counts/beat. Triplet-eighth grid; no sixteenths.
- Lane y (given): BEGUINE 1236, (~1283 faint), 1328, 1378, 1420, 1470.
  RHUMBA 1628, 1678, 1720, 1766, 1820, 1862.
- Labels at x ~3880..4040. Code column at x ~5950..6100.
- Method: numbered count grid overlay drawn with cv2, half-bar crops (12 counts)
  upscaled to 2000px wide, read by eye with the Read tool. NO circle detection.

## Log
- (start) file created.

- Row-darkness profile over x 4040..5930 confirms BLOCK UPSCALE: rows repeat in blocks of ~8.33px, so the embedded scan is ~72 dpi and big-14.png is a block upscale. One count (39.31px) = ~4.7 source pixels; a circle is ~4x4 source px.
- BEGUINE lane lines (dark bands): 1233-1240, 1275-1290(fainter, 2 blocks), 1325-1332, 1375-1382, 1416-1424, 1466-1474. Plus an intermediate band 1216-1232 to be resolved (border vs lane).

- IMPORTANT GRID FINDING (column-darkness profile, x 4000..5980, y 1195..1500): the chart draws
  vertical rules ONLY at counts congruent to 0,2,3,4 (mod 6) -- i.e. the union of triplet eighths
  (beat+0,+2,+4) and straight eighths (beat+0,+3). Beat lines (c mod 6 == 0) are the darkest.
  Measured line centres land within 0.2 count of the given formula, so the formula is CONFIRMED.
  Consequence: counts congruent to 1 or 5 (mod 6) have NO rule and can carry NO circle.

### Reading aid (aid only, every hit still confirmed by eye)
Per (lane,count) I measure mean ink in two boxes just ABOVE and BELOW the lane line (y-15..-5 and
+6..+15, x +-13) and subtract the PER-COUNT MINIMUM ACROSS THE SIX LANES. The vertical rule is
identical in every lane, so the min-subtraction cancels it and what is left is ring ink. Counts
=1,5 (mod 6) are unruled and can hold no circle, so they act as a built-in noise floor.
This is NOT circle detection (no Hough, no ring score) and never decides a hit on its own.

### BEGUINE bar 1, counts 0-11 (image bq_beg_A.png + aid)
L1: 0, 3, 4, 6, 9   (c2 aid=0, so the wide blob at 2-4 is the count-2 RULE plus rings at 3 and 4)
L2: nothing at all in 0-11 (aid max 12; strip shows line + rules only)
L3: 3, 9  on a RAISED FLOOR (aid ~20-30 at unruled counts 1,5,7,11 -- bar 1 only, bar 2 is clean)
L4: 3
L5: 3      (same raised floor as L3 in bar 1 only)
L6: 0 (see caveat), and a ring at 12
CAVEAT: the L6 row LABEL overhangs to the right past the count-0 rule, so L6 count 0 needs a
wide-left-margin look before it is trusted (trap 2). L1s label ends at x~3991, well clear.
OPEN: L3 and L5 carry extra ink through the whole of bar 1 but not bar 2 -- check for a
SUSTAINED BAR rather than dots.

### BEGUINE L1 (y1236, top label ~ "M") -- read from bq_beg_L1.png, 164 px/count
bar1: 0, 3, 4, 6, 9, 12, 15, 18, 21
bar2: 24, 27, 28, 30, 33, 36, 39, 42, 45
So: EVERY STRAIGHT EIGHTH (beat+0 and beat+3 throughout) plus ONE extra at beat+4 on the first
beat of each bar (counts 4 and 28). The 3+4 pair reads as two clearly separate rings with two
light holes, spanning ~2.1 counts, far too wide for one ring; the count-2 position next to it is a
bare thin rule. beat+2 positions are 0-22 on the aid everywhere (never a hit); the aid noise floor
on this lane is ~19 (counts 17 and 23 are UNRULED yet read 16 and 19), so 25 is the honest threshold.

### GEOMETRY CORRECTION (important, and it explains a false raised floor)
The lane lines STEP vertically by one source pixel (~8px at 600dpi) in places -- a print/scan step,
not skew. Measured per 6-count window from unruled counts only:
  BEG L1 1233 all; L2 ~1283 (bar1) / ~1275 (bar2), faint and unstable; L3 1333 for counts 0-11 then
  1325; L4 1375 all; L5 1425 for counts 0-11 then 1416; L6 1466 all.
  RHU L1 1625, L2 1675, L3 1717, L5 1817, L6 1858 all; L4 1758 for counts 0-5 and 18-23, else 1767.
Before correcting for this, BEG L3 and L5 showed elevated ink through the whole of bar 1 and I
nearly wrote it up as a sustained bar: it was the LINE ITSELF sitting inside my "above" box.
All aid numbers from here on are measured against the locally located line.

### BEGUINE L3 (y1333/1325, label ~ "Cb") -- bq_beg_L3.png, unambiguous
bar1: 3, 9, 15, 21   bar2: 27, 33, 39, 45   = the OFF-BEAT EIGHTH of every beat, nothing else.

### BEGUINE L4 (y1375, label ~ "Cy") -- bq_beg_L4.png
bar1: 3   bar2: 27   -- nothing else in 48 counts. Rows 2 and 4 hold only rules.
Discriminator used: the bottom edge of L3s ring bleeds into the TOP ~30px of the L4 strip at
counts 15/21/39/45; at counts 3 and 27 the ink instead reaches BELOW the L4 line and encloses a
light hole, which a bleed never does.

### BEGUINE L1 verified beat-by-beat at 232 px/count (bq_beg_L1_b0.png, bq_beg_L1_b4.png)
bar1: 0, 3, 4, 6, 9, 12, 15, 18, 21
bar2: 24, 27, 28, 30, 33, 36, 39, 42, 45
beat+2 (counts 2,8,14,20,26,32,38,44) is a BARE THIN RULE every time; the beat+4 ring exists only
at 4 and 28 (both drawn as a second full ring touching the beat+3 ring, ~2.15 counts of ink total).
NOTE the numeric aid gave 36-42 at counts 14, 32 and 38 -- all three are BLEED: the beat+3 ring is
drawn slightly LEFT of its own tick, so its arc reaches the beat+2 rule. The images overrule the aid.
### BEGUINE L5 (y1425/1416, label ~ "Lc") -- bq_beg_L5.png + bq_beg_c3.png
bar1: 3   bar2: 27   -- same as L4, nothing else.
bq_beg_c3.png (count 3 column at 9x, y1300-1450) shows THREE stacked rings on L3+L4+L5 whose arcs
touch: circle diameter is ~40px = about one whole count, so the arcs of vertically adjacent lanes
meet. That is why count 3 looked like one continuous vertical smear at low zoom.

### BEGUINE L2 (y~1279, label ~ "C") -- IT IS EMPTY
bq_beg_L2_b0.png and bq_beg_L2_b4.png, all 8 beats at 232 px/count: the lane carries its line and
the vertical rules and NOTHING else. The dark caps at the very top of every L2 strip are the BOTTOM
ARCS of L1s rings one lane up (they appear at exactly L1s hit counts and never reach L2s own line).
So lane 2 is printed (line + label) but carries no circles in BEGUINE. Written as 24 dots.

### BEGUINE L6 (y1466, label ~ "Lc-Bd", the widest label) -- bq_beg_L6_b0/b4.png
bar1: 0, 12, 18   bar2: 24, 36, 42   (beats 1, 3, 4 of each bar; identical bar to bar)
Count 0 is a genuine ring: the label does overhang to about x4035 and the count-0 rule sits at
x4043.9, but there is ring ink on BOTH sides of the rule and the same amount of ink appears at
count 24, where no label can reach. Counts 6 and 30 are bare rules -- beat 2 is silent on this lane.

### BEGUINE block extent confirmed
Row profile over y1475-1650 finds NO horizontal line between 1475 and 1607; the next line is
1625-1632 (RHUMBA L1). So BEGUINE really is six lanes, 1236..1470, and nothing hides below it.

## RHUMBA -- now reading. Corrected lane ys: 1625, 1675, 1717, 1767 (see note), 1817, 1858.

### RHUMBA L1 (y1625) -- bq_rhu_L1.png
bar1: 0, 3, 4, 6, 9, 12, 15, 18, 21
bar2: 24, 27, 30, 33, 36, 39, 42, 45
= every straight eighth, plus ONE extra ring at count 4 (bar 1 only; count 28 is a bare rule, and
the row-1 ink mass visibly runs ~180px further right than the row-3 one).

### RHUMBA L2 (y1675, label ~ "C") -- bq_rhu_L2.png -- THE CLAVE, and it self-validates
bar1: 0, 9, 18   bar2: 30, 36
That is the textbook 3-2 SON CLAVE: beat 1, the "and" of beat 2, beat 4 | beat 2, beat 3. Counts 24,
27, 33, 42 and 45 are bare rules (the small dark caps at the top of the L2 strip at 3/15/21/27/33/39/45
are L1s ring bottoms one lane up). This independent musical match is the strongest single check in
this reading: the grid, the lane assignment and the label all have to be right for it to appear.

### RHUMBA L3..L6 (read in the MAIN SESSION after the third agent on this rhythm stalled)
Method: the inherited geometry above, lane pairs cropped at ~83 px per count, with UNRULED counts
(c mod 6 in {1,5}) drawn in a different colour as a live noise check. Read by eye.

L3  Hb    bar1: 3, 6, 12, 15, 18, 21        bar2: 27, 30, 36, 39, 45
    UNCERTAIN: bar 1 count 18 and bar 2 count 42 do not mirror each other, and at this
    magnification I cannot separate L3's ring from L4's at those two counts. Everything else
    on the lane is clear. Needs one dedicated single-lane pass to settle those two cells.
L4  Hc    bar1: 0, 9, 18                    bar2: 24, 33, 42        CONFIDENT (one bar repeated)
L5  Lc    bar1: 18, 21                      bar2: 42, 45            CONFIDENT (one bar repeated)
L6  Bd    bar1: 0, 12                       bar2: 24, 36            CONFIDENT (beats 1 and 3)

### RHUMBA: one bar or two?
Mixed, and that is the interesting part. Four of the six lanes (Hc, Lc, Bd, and L1's eighths) are
one bar repeated, but the CLAVES lane is a genuine two-bar 3-2 son clave (0, 9, 18 | 30, 36) and
L1 carries one extra hit in bar 1 only (count 4). So the machine builds a two-bar rhythm out of
mostly one-bar lanes plus one asymmetric lane. That is a cheap and very reusable trick: the clave
lane alone supplies the two-bar identity.

### RHUMBA, assembled (24 counts per bar, 6 per beat)
```
L1  M     x..xx.x..x..x..x..x..x..  x..x..x..x..x..x..x..x..
L2  C     x........x........x.....  ......x.....x...........
L3  Hb    ...x..x.....x..x..x..x..  ...x..x.....x..x.....x..   [UNCERTAIN, 2 cells]
L4  Hc    x........x........x.....  x........x........x.....
L5  Lc    ..................x..x..  ..................x..x..
L6  Bd    x...........x...........  x...........x...........
```


# ===== source file: fr2l_samba_mambo.md =====

# Ace Tone Rhythm Ace FR-2L — SAMBA and MAMBO transcription

## Provenance
- Source: Ace Tone "RHYTHM ACE model FR-2L SERVICE NOTE", c.1969.
  archive.org item: https://archive.org/details/RhythmAceFR2LServiceManual
- Page 14 = Fig 10, the pattern chart (page 13 = Fig 9, the 35 numbered pulse trains).
- Working image: `<scratch>/big-14.png`, page 14 at 600 dpi (6667x4775), pre-rendered.
- Clock: 48 counts = TWO BARS, 24 counts/bar, 6 counts/beat. Eighth-note triplets native;
  sixteenths do not exist on this grid.
- Grid fit (RIGHT table, independently fitted): x(c) = 4043.87 + 39.310*c, c = 0..47.
  Beat pitch 235.857 px; residuals < 0.13 count.
- Lane y (long-horizontal-line detection):
    SAMBA: 2020, 2166, 2212, 2253
    MAMBO: 2416, 2458, 2553, 2599, 2645
  Both blocks flagged for a possibly-missed faint lane in their large gap.
- Labels x ~3880..4040; Lane Code numbers x ~5950..6100.
- Method: numbered count grid overlaid with cv2, half-bar crops (12 counts) upscaled to
  2000px, read visually with the Read tool. NO circle detection (Hough / ring-score both
  over-detect badly on this block-upscaled scan).

## Log
- (turn 1) File created. Next: inspect the two blocks' y bands for faint missed lanes.
- (turn 2) READ Q_samba_0.png (counts 0-12). Lanes visible: 4 only (M, Hc, Lc, Bd).
  Large M->Hc gap contains NO horizontal lane line in this crop: no hidden 5th lane here.
  M:  circles at 3, 6, 9, 12.  NO circle at count 0 (lane line starts clean, empty).
  Hc: big clear circle at count 0. nothing else 1-12.
  Lc: circles at 9 and 12. nothing 0-8.
  Bd: circle at count 0. nothing else 1-12.
  COUNT-0 VERDICT (first pass): count 0 ink IS captured by this crop (Hc and Bd both show
  clear count-0 circles sitting right beside the row labels), so M's empty count 0 is REAL,
  not a crop/label artifact. M starts on count 3 in bar 1.
- (turn 3) READ Q_samba_1.png (counts 12-24). Scale 154 px/count; lane y in this crop:
  M 110, Hc ~682, Lc ~862, Bd ~1023 (derived from page y offsets x 3.918).
  M:  circles at 12(left edge, = the one already seen), 15, 18, 21, 24(right edge).
  Hc: circles at 18, 21, 24. (18's blob is slightly fatter than its neighbours -> mild doubt,
      but only ~1.23 counts wide, so ONE circle, not two.)
  Lc: line runs clean 13..24, no circles (its count-12 circle is the left-edge ink).
  Bd: clean 13..24, no circles.
  Still only 4 lanes; M->Hc gap empty again.
- (turn 4) READ Q_samba_2.png (counts 24-36). Scale 117.7 px/count; lane y: M 85, Hc 522, Lc 660, Bd 783.
  Left margin (x<537) = counts 21-23: holds the M@21 and Hc@21 circles already logged.
  M:  circles at 24, 27, 30, 33, 36.
  Hc: circle at 24 (clear, on the bar line). nothing 25-36.
  Lc: circles at 33 and 36. nothing 24-32.
  Bd: circle at 24. nothing else.
  NOTE the bar-repeat check so far: Hc bar1 {0,18,21} vs bar2 {24,42?,45?} -> 24=0+24 OK;
  Lc {9,12} vs {33,36} = exact +24; Bd {0} vs {24} = exact +24.  M is the ONLY asymmetry:
  no circle at 0 but a clear circle at 24, i.e. every multiple of 3 EXCEPT 0.
- (turn 5) READ Q_samba_3.png (counts 36-48). 154 px/count; lane y M 110, Hc 682, Lc 862, Bd 1023.
  M: 36(left edge), 39, 42, 45. NO circle in the 46/47 area and none at the closing 48 line.
  Hc: 42 and 45 (two clear rings).  Lc: only the left-edge 36 circle, clean 37-47.  Bd: clean.

## SAMBA
Lanes: 4, labels as printed top->bottom: M, Hc, Lc, Bd.
No fifth lane: the large M->Hc gap shows NO horizontal rule in any of the four crops.

COUNT-0 VERDICT: M genuinely has NO circle at count 0 -- it is a real blank, not a
label-collision miss. Evidence: (a) in Q_samba_0 the count-0 column at that same x
clearly carries circles on Hc and Bd, so count-0 ink is visible there; (b) M's first
circle sits 3.07 counts right of the count-0 line; (c) Q_samba_3 shows the same blank at
the cycle end. So M = every multiple of 3 EXCEPT 0, and it DOES hit 24. That single hit
is the whole of samba's two-bar asymmetry.

    M   ...x..x..x..x..x..x..x..  x..x..x..x..x..x..x..x..
    Hc  x.................x..x..  x.................x..x..
    Lc  .........x..x...........  .........x..x...........
    Bd  x.......................  x.......................

SAMBA: CONFIDENT, with one UNCERTAIN item: Hc count 18 (and its twin 42) -- the 18 blob
printed slightly fatter than its neighbours. It is only ~1.23 counts wide so I read ONE
circle, and the bar-2 twin at 42 is unambiguous, which corroborates it.
STRUCTURE: effectively ONE BAR REPEATED (bar 2 == bar 1 + 24 for every lane), with the
single exception of M's missing count 0. Called as one-bar-repeated + that anomaly.

## MAMBO
- (turn 6) READ Q_mambo_0.png (counts 0-12, 115 px/count, label column included).
  FIVE lanes, y in crop: L1 85, L2 200, L3 500, L4 625, L5 775 (matches page y 2416/2458/
  2553/2599/2645). The large L2->L3 gap holds NO horizontal rule: no hidden 6th lane.
  Labels at this magnification read as M / Cb / Hb-or-Hc / Lc / Bd but b-vs-c is NOT safe yet
  -> magnifying the label column next.
  Hits (provisional):
   L1: 0, 3, 6, 9, 12  (every 3rd count)
   L2: 0, 6, 12 -- inferred from TALL ink stacks at counts 0/6/12 that span both L1 and L2,
       where counts 3 and 9 are short (L1 only). Count 12 stack needs a second look.
   L3: 0, 6, 9, 12
   L4: line runs clean 0-12, no circles
   L5: 0 certain; possible circle near count 9 (bottom of frame, close to the printed
       numerals) -- UNRESOLVED, needs a magnified look.
- (turn 7) READ z_mambo_labels.png (label column magnified 2.5x, 5 bands stacked).
  Source glyphs are only ~8 px tall in the embedded scan, so this is at the limit.
  L1 = M (two stems + middle dip). L2 = two glyphs, left one has a white counter (C/o),
  right one dark -> reads "Cb". L3 = clear H + a second glyph. L4 = clear L + small glyph
  -> "Lc". L5 = B with a counter + d -> "Bd".
  OPEN: L3 second glyph b-vs-c (Hb high bongo vs Hc high conga), and L2 confirm.
  Next: put samba's known "Hc"/"Lc" glyphs beside mambo L3/L4 as a shape reference.
- (turn 8-9) LABEL RESOLUTION LIMIT (important, applies to both rhythms).
  Dumped the label column at the scan's NATIVE grid (ASCII, then 8x8-cell bitmaps).
  The embedded scan's pixel is 8 render px at 600 dpi, i.e. the scan is ~75 dpi. A lane
  label occupies ~6 native px across for TWO characters, so each letter is ~3x4 native
  pixels. The convincing "H"/"L"/"B" letterforms in the block-upscaled crops are the
  upscaler's interpolation, NOT resolvable ink. b-vs-c CANNOT be settled from this file.
  What the shapes suggest (explicitly a guess, not a transcription):
    MAMBO L1 "M" · L2 "Cb" · L3 "Hb" or "Hc" · L4 "Lc" or "Lb" · L5 "Bd"
  Discriminator I tried: glyph-2 ascender height. MAMBO L3's second glyph carries ink in
  the same native row as the first glyph's cap (suggesting an ascender -> b), where SAMBA's
  known "Hc" second glyph starts one row lower. That is a ONE-native-pixel difference and
  the two labels are not on the same 8px phase, so I do not trust it enough to assert.
  => MAMBO labels reported as UNREAD/uncertain; lane IDENTITY given by page y instead.
  (Circles are ~4x4 native px against white and the count pitch is 5 native px, so the
  RHYTHM reads fine; it is only the lettering that is under-resolved.)
- (turn 10) READ Q_mambo_1.png (counts 12-24). 154 px/count; lane y L1 95, L2 260, L3 632,
  L4 812, L5 992 (lanes now 165 px apart, so L1 vs L2 is unambiguous at this magnification).
  L1: 15, 18, 21 (+ the 12 and 24 edge circles) -> every 3rd count, INCLUDING 0 (unlike samba's M).
  L2: 18 only (the count-18 ink is a TALL stack covering both L1 and L2; 15 and 21 are short,
      L1-only). Left edge shows L2 ink at 12. -> L2 = every 6th count so far (0,6,12,18).
  L3: NO circles 13-23; the left/right edge circles are 12 and 24.
  L4: 18 and 21 (two clear rings in the L4 band). nothing else 12-23.
  L5: obscured by the printed numerals along the bottom of this crop -- not read here.
- (turn 11) READ Q_mambo_2.png (counts 24-36). 117.7 px/count; lane y L1 75, L2 201, L3 485,
  L4 623, L5 761. Left margin = counts 21-23.
  L1: 24, 27, 30, 33, 36 (every 3rd).
  L2: 24, 30, 36 -- the tall two-lane stacks; 27 and 33 are L1-only (short). -> every 6th.
  L3: circles at 24, 27, 30, 33, 36 (five evenly spaced rings in the L3 band).
  L4: only the left-margin circle at 21; clean 24-36.
  L5: 24, and a ring at 33.
  ** CONFLICT to settle: L3 read as {0,6,9,12} in bar 1 (crop 0) but {24,27,30,33,36} here.
  Under a +24 repeat bar 2 should be {24,30,33,36}; the count-27 ring is the extra. Also
  crop 1 showed L3 EMPTY at 15/18/21, so L3 is certainly not every-3 throughout.
  L5@33 does corroborate the suspected L5@9 (9+24).
- (turn 12) READ Q_mambo_3.png (counts 36-48). 154 px/count; lane y L1 105, L2 285, L3 647,
  L4 827, L5 995.
  L1: 39, 45 (+36, 42 in the tall stacks) -> 36,39,42,45 = every 3rd. nothing at 46/47.
  L2: 42 (tall stack) and the 36 stack at the left edge -> 36, 42.
  L3: only the left-edge 36 circle; clean 37-47.
  L4: 42 and 45 (two clear rings) = bar 1's 18/21 + 24. CONFIRMS L4 = {18,21} per bar.
  L5: not separable from L4's ink / the numerals here.
  So every mambo lane repeats at +24 EXCEPT the L3 count-3 question. Re-checking crop 0's
  lower lanes at higher magnification from big-14.png.
- (turn 13) READ z_mambo_q0_zoom.png (counts -0.6..12.6, all 5 lanes, 3x from big-14.png,
  no overlay; count c at x = 71 + 118c, lanes at y 72/198/483/621/759). Clean confirmation:
  L1: 0, 3, 6, 9, 12   L2: 0, 6, 12 (the tall stacks)   L3: 0, 6, 9, 12 and DEFINITELY NO
  circle at count 3 (only the plain lane line plus the vertical rule there)   L4: clean 0-12
  L5: 0 and 9 -- the suspected count-9 ring is REAL and clear. So L5 = {0,9} per bar.
  The L3 count-27 claim from crop 2 is therefore the one remaining asymmetry; verifying it
  the same way over counts 24-36.
- (turn 14) READ z_mambo_q2_zoom.png (counts 23.4..36.6, 3x, no overlay; count c at
  x = 73 + 118*(c-24)). The count-27 ring on L3 is UNMISTAKABLE (a clear open ring
  spanning ~1.2 lane heights), and bar 1's count 3 is just as unmistakably EMPTY.
  Also re-confirmed here: L1 24/27/30/33/36, L2 24/30/36 (tall stacks), L4 clean,
  L5 24 and 33.

MAMBO lanes: FIVE. Labels are NOT transcribable from this scan (see turn 8-9); the
shapes suggest M / Cb / Hb / Lc / Bd top to bottom and I write them with a '?'.
Lane identity below is by page y, which IS solid.

    M?  x..x..x..x..x..x..x..x..  x..x..x..x..x..x..x..x..   (y2416)
    Cb? x.....x.....x.....x.....  x.....x.....x.....x.....   (y2458)
    Hb? x.....x..x..x...........  x..x..x..x..x...........   (y2553)
    Lc? ..................x..x..  ..................x..x..   (y2599)
    Bd? x........x..............  x........x..............   (y2645)

MAMBO: CONFIDENT on the circles. Two notes:
 - L2's hits are read as the TALL two-lane ink stacks at 0/6/12/18/24/30/36/42 where
   L1-only counts print short. At 154 px/count (crops 1 and 3) the L1/L2 lanes are
   165 px apart so this is a direct read, not an inference; crop 0's 115 px/count view
   was re-checked at 3x (z_mambo_q0_zoom) and agrees.
 - LABELS UNREAD (letterforms below the scan's resolution). Only the lane ORDER and
   the rhythm are sourced facts here.
STRUCTURE: a GENUINE TWO-BAR figure, but only just. Four of the five lanes repeat
exactly at +24 (M every 3rd count, Cb every 6th, L4 18/21 -> 42/45, L5 0/9 -> 24/33).
The whole two-bar-ness is L3: bar 1 {0,6,9,12} vs bar 2 {24,27,30,33,36}, i.e. bar 2
adds a hit at count 27 where bar 1's count 3 is blank. Both bars' L3 activity stops
after count 12 of the bar (nothing in the bar's second half).

MAMBO's count 0 is occupied on M, Cb, L3 and L5 -- a further check that the count-0
column is not being lost by the crops, and so that samba's blank M count 0 is real.

## Closing caveats
- SAMBA's lane labels (M, Hc, Lc, Bd) are the ones SUPPLIED with the task, not re-verified
  here: the same resolution limit applies to them, so treat them as inherited, not sourced.
  Lane ORDER and page y (2020 / 2166 / 2212 / 2253) are what I actually verified.
- No lane in either rhythm is drawn as a thick sustained BAR. All nine lanes are dot/ring
  lanes on a thin rule; no held gates to describe.
- Nothing was written down that I did not see. The two things I could not read are named as
  such: the mambo LETTERING, and (crop 1/3 only) mambo L5 under the printed numerals -- but
  L5 was read cleanly from the 3x zooms instead, so no count is left UNREAD.


# ===== source file: tr77.md =====

# Roland Rhythm TR-77 (1972) — preset rhythm patterns, recovered from documents

STATUS: IN PROGRESS (append-only log; every fact recorded the moment it is known)
Started: 2026-08-19

## PROVENANCE

### Sources attempted (append as we go)
| # | URL / identifier | result |
|---|---|---|
| 2 | https://archive.org/download/roland_Roland_TR-77_Service_Manual/Roland_TR-77_Service_Manual.pdf | OK, downloaded 1,446,636 B. PDF internal metadata: Title "Roland TR-77 Service Manual", Author "burnkit2600" (so this IS the burnkit2600.com scan), Creator QuarkXPress 4.11, 32 pages, letter size. Scanned images, no embedded text layer. |

### Document structure (from the IA `_djvu.xml` per-page OCR of the service manual, 32 PDF pages)
Cover (p1): "ELECTRONIC MUSICAL INSTRUMENT / ROLAND RHYTHM INSTRUMENT / THE 7th EDITION / Printed in Japan '76. Nov."
Relevant pages (PDF page numbers in the 32-page file):
- p4  SECTION 1. SPECIFICATIONS, 1-1 Summary A. Rhythm -> the rhythm list in panel order
- p13 SECTION 5, "B. Logic Output Timing Chart (Fig. 6)" -- OCR shows a count ruler "0 2 4 6 8 10 12 14 16 18 20 22 24 26 28 30 32"
- p14 SECTION 6. RHYTHM SWITCH ASSEMBLY: 6-1 Jazz Section Switch Assembly (RS-3) (Fig. 7); 6-2 Latin Section Switch Assembly (RS-4) (Fig. 8)
- **p15 "6-3. Rhythm Pattern  A. Jazz (Fig. 9)   B. Latin (Fig. 10)"  <-- THE PATTERN CHART**
- p16 6-4 Variation section, A. Relation Diagram of Beat Selector and Rotary Switch (Fig. 11)
- **p17 "B. Rhythm Ensemble Pattern (Fig. 12)"  <-- second pattern chart (variation/ensemble)**
- p24-p27 SECTION 9 alignment: names every voice terminal

## RHYTHM LIST (source: p4, SECTION 1. SPECIFICATIONS, 1-1 Summary A. "Rhythm"; OCR typos corrected only where obvious, marked)

**Jazz Section** (8 + CANCEL): Rock'n Roll 1, Rock'n Roll 2, Slow Rock ["Slow Sock" in OCR],
Ballad, Western, 6/8 March, Jazz Waltz ["Jazz Walts"], Waltz, CANCEL.

**Latin Section** (10): Rhumba, Beguine, Cha-Cha, Mambo, Samba 1, Samba 2 ["Seunba 2"],
Baion, Bossanova, Bolero, Tango.

**2 Beat Variation** (6): Bass Drum, Bass & Snare Drum, Fox Trot 1, Swing 1, March, Parade.
**4 Beat Variation** (6): Bass Drum, Bass & Snare Drum, Fox Trot 2 ["Pox Trot 2"], Swing 2, Swing 3, Shuffle.

(So the machine has 18 named preset rhythms in two selector rows, plus a 12-entry
variation section reached through the Beat Selector / rotary switch, section 6-4.)

## VOICE ROSTER (source: same page, 1-1 B. "Voices") -- 13 voices
Bass Drum, Low Conga, Low Bongo, High Bongo, Cow Bell, Rim Shot, Claves,
Snare Drum, Maracas, Cymbal, High Hat, Guiro, Tambourine.

Voice Control groups (1-1 C): 1. Cymbal, High Hat, Maracas | 2. Guiro | 3. Snare Drum | 4. Bass Drum.
Tempo (1-1 D): 12-130 beats/min. Active elements: 41 Si transistors, 173 Si diodes, 1 LED, 1 IC.

Terminal abbreviations used in the alignment section (p24-p27), i.e. the chart's lane labels:
noise section = M (Maracas), Cy (Cymbal), HH (High Hat), SD (Snare Drum), Tb (Tambourine), Gu (Guiro)
drum section  = BD (Bass Drum), LC (Low Conga), LB (Low Bongo), HB (High Bongo), RS (Rim Shot),
                CB (Cow Bell), C (Claves)

### Image extraction
`pdfimages -list -f 13 -l 17 tr77srv.pdf` -> every page is a **300 dpi bitonal CCITT stencil**
(p13 2253x3252, p14 2253x3218, **p15 2399x3235**, p16 2253x3236, **p17 2369x3243**).
So 300 dpi IS the native resolution; rendering at 600 dpi would only upscale.
Extracted natively with `pdfimages -png -f 13 -l 17 tr77srv.pdf pg`
-> pg-000=p13, pg-001=p14, **pg-002=p15 (Rhythm Pattern, Figs 9+10)**, pg-003=p16, **pg-004=p17 (Ensemble Pattern, Fig 12)**.

## A REAL PATTERN CHART EXISTS -- p15 (printed page "- 13 -")

Overview read at 1/3 scale. The scan is a **NEGATIVE** (white ink on black). Page holds
THREE blocks, each with its own count ruler printed along its bottom edge reading
`0 2 4 6 8 10 12 14 16 18 20 22 24 26 28 30 32`:

- **Block A = Fig. 9 "A. Jazz"** (left, upper): ROCK'N ROLL 1, ROCK'N ROLL 2, SLOW ROCK,
  BALLAD, WESTERN, 6/8 MARCH, JAZZ WALTZ, WALTZ.
- **Block B = Fig. 10 "B. Latin"** (right, upper): RHUMBA, BEGUINE, CHA-CHA, MAMBO, SAMBA 1, SAMBA 2.
- **Block C = Fig. 10 continued** (right, lower): BOSSANOVA, BAION, BOLERO, TANGO.

Each rhythm is a small stack of lanes, labelled at the left with the voice abbreviations
(CY / HH / SD / BD / M / C / HB / LB / LC / RS / TB / GU, and sums like `CY+LB`, `LC+BD`,
`C+HB`, `CB+LB`, `TB'`), plus a final lane labelled **Me** (metronome).
To the RIGHT of every rhythm is a column of numbers, one per lane (e.g. Rock'n Roll 1:
14 / 20 / 11 / 9 ... Me 42+5). These are the **logic-output / trigger numbers** the lane is
wired to, not pattern data. Some carry primes (`42'`, `39'+18'+5`, `15'+18'`) and some rhythms
carry the annotation **F.B.**

**Observation to verify at zoom:** several rhythms have DIAGONALLY HATCHED vertical bands
crossing all their lanes (SLOW ROCK, BALLAD, 6/8 MARCH, JAZZ WALTZ, WALTZ in Fig. 9; BOLERO
in Fig. 10) -- exactly the triplet-feel / 3-time rhythms. Hypothesis: hatched columns are
counts that rhythm does NOT use (the 32-count bar re-divided by 3). Same set carries "F.B.".

### Block A (Fig. 9, Jazz) grid calibration -- MEASURED, not assumed
Column ink profile over y=470..2050, x=380..1200 of pg-002.png (ink = white, threshold >128).
Strong vertical grid lines at x =
424, 463, 503, 543, 584, 622, 661, 701, 742, 780, 820, 859, 900, 940, 980, 1019, 1057
= **17 lines, mean pitch 39.6 px**, and there is NO line between them (the profile is flat ~140
between neighbours). 17 lines == the 17 printed ruler labels 0,2,4,...,32.
So for Fig. 9: label 0 at x=424, label 32 at x=1057, one CHART COLUMN per 2 counter states,
i.e. **16 chart columns per bar + the 32 = wrap column**.

Row ink profile over x=424..1058 gives the lane rules (full-width y where ink==634 px):
rhythm 1 lanes y=500,531,563,594  | block border 656
rhythm 2 lanes y=718,750,780,811  | border 873
rhythm 3 lanes y=936,967,997      | border 1060
rhythm 4 lanes y=1122,1153,1184   | border 1247
rhythm 5 lanes y=1310,1341,1373   | border 1435
rhythm 6 lanes y=1498,1530,1561   | border 1626
rhythm 7 lanes y=1689,1720,1752   | border 1813
rhythm 8 lanes y=1877,1908,1940   | border 2003
(the "Me" metronome lane is not a solid rule, it sits in the gap above each border)


# ===== source file: tr77_jazz.md =====

# Roland TR-77 (1972) preset pattern chart — JAZZ half (Fig. 9)

## Provenance
- Document: "ELECTRONIC MUSICAL INSTRUMENT / ROLAND RHYTHM INSTRUMENT" service manual,
  7th edition, printed Japan Nov 1976, 32 pages.
  https://archive.org/download/roland_Roland_TR-77_Service_Manual/Roland_TR-77_Service_Manual.pdf
- Chart: PDF page 15 (printed page "- 13 -"), section "6-3. Rhythm Pattern, A. Jazz (Fig. 9)".
- Image read: `<scratch>/pg-002.png`, native resolution, 300 dpi BITONAL NEGATIVE
  (ink is WHITE on black; threshold ink = pixel > 128).
- Scope: Fig. 9 "A. Jazz" only, 8 rhythms. Fig. 10 (Latin) NOT touched.

## Calibration used (measured by a previous agent, spot-checked here)
- 17 vertical grid lines matching ruler labels 0 2 4 6 ... 32, at x =
  424, 463, 503, 543, 584, 622, 661, 701, 742, 780, 820, 859, 900, 940, 980, 1019, 1057
  (mean pitch 39.6 px, no line in between).
- So ONE CHART COLUMN = 2 counter states => 16 columns per bar (indices 0..15),
  plus the "32" wrap column (x=1057) noted separately.
- Lane rule y positions per rhythm:
  1 ROCK'N ROLL 1: 500, 531, 563, 594
  2 ROCK'N ROLL 2: 718, 750, 780, 811
  3 SLOW ROCK:     936, 967, 997
  4 BALLAD:        1122, 1153, 1184
  5 WESTERN:       1310, 1341, 1373
  6 6/8 MARCH:     1498, 1530, 1561
  7 JAZZ WALTZ:    1689, 1720, 1752
  8 WALTZ:         1877, 1908, 1940
  Block borders: 656, 873, 1060, 1247, 1435, 1626, 1813, 2003.
  "Me" metronome lane sits in the gap above each block border (bonus, not deliverable).

## Calibration spot-check (mine, PASSED)
Measured the ink thickness along each lane rule (ink count in y +/- 8 px, per x). A DOT reads
as a run 18-20 px wide; the bare rule reads 2-4 px; a vertical grid line reads 3-6 px. On the
SLOW ROCK CY lane every dot centre landed within 2.0 px of one of the 17 listed grid x's, and
NOTHING landed at a half-column position. So the 17-line grid is right and there are no
off-grid (odd-counter-state) marks. Columns are the LINES (dots sit ON a line), indices 0..15
for labels 0,2,...,30, plus col 16 = label 32 (the wrap/bar line).

## VERDICT on the diagonally hatched bands  ***CONFIRMED: hatched = column NOT USED***
The hatched bands are narrow (about 20 px, half a column pitch) vertical strips of short
diagonal strokes, drawn straddling a grid LINE, crossing every lane of that rhythm. Measured
band x-extents in SLOW ROCK: 524-544, 685-702, 845-862, 1001-1021 — i.e. each band sits on
the grid lines x=543, 701, 859, 1019 = labels 6, 14, 22, 30 = column indices 3, 7, 11, 15.

That is EVERY FOURTH COLUMN, and the SLOW ROCK CY lane proves the meaning: it has a dot on
columns 0,1,2, 4,5,6, 8,9,10, 12,13,14 and on NO hatched column. So the hatch strikes out the
4th column of each group of four, leaving 12 usable columns per bar = 3 per beat = TRIPLETS.

So the previous agent's hypothesis is right: a hatched column is a counter state the rhythm
does not use, and the 16-column bar is re-divided by three for the triplet-feel / 3-time
rhythms (SLOW ROCK, BALLAD, 6/8 MARCH, JAZZ WALTZ, WALTZ). Exactly the per-rhythm re-division
of a fixed clock that the Ace Tone FR-2L chart also does.

HOW I RECORD IT BELOW: I keep all 16 character slots so the raw grid stays recoverable, and
write '/' (never a hit) in a hatched column. Read a triplet rhythm by deleting the '/' slots,
which leaves 12 slots = 4 groups of 3.

## Method for reading the dots
Two independent passes, and I only wrote a hit where both agreed:
1. Ink-thickness measurement along each lane rule (ink count over y +/- 8 px, per x; widest
   run above threshold within 13 px of a grid line). This is NOT blob/Hough/ring detection —
   it just asks "how thick is the ink where the lane crosses this column". The answer is
   sharply bimodal on this scan: a dot reads 16-22 px wide, a bare rule or a bare vertical
   grid line reads 3-6 px. No intermediate values occurred in rhythm 1.
2. My own eye, on a grid-overlaid crop of each rhythm blown up to ~1900 px wide (about 105 px
   per column), read in two halves. Rhythm 1 was checked dot-for-dot both halves: perfect
   agreement, so the measurement is trustworthy for the rest; I still eyeballed every rhythm.

Column index = grid line index. col 0 = ruler label 0, col 15 = label 30, col 16 = label 32
(the bar/wrap line). 16 characters below = cols 0..15. '/' = hatched (unused) column.

---

## 1. ROCK'N ROLL 1
```
             cols   0 . 2 . 4 . 6 . 8 . 10. 12. 14.      (ruler 0,2,4,...,30)
CY      trig 14     ..x...x...x...x.
HH      trig 20     xxxxxxxxxxxxxxxx
SD      trig 11     ..xx..x...xx..x.
BD      trig  9     x..xx...x..xx...
```
- No hatching in this rhythm; all 16 columns are in use.
- Nothing on the 32 wrap column in any of the four lanes.
- Me lane: trig "42 + 5" (pattern read below, see the Me section).
- CONFIDENT (both passes agree on every one of the 64 cells).

## 2. ROCK'N ROLL 2
```
             cols   0 . 2 . 4 . 6 . 8 . 10. 12. 14.
CY      trig 14     ..x...x...x...x.
HH      trig 20     xxxxxxxxxxxxxxxx
SD      trig 25 + 40 ....x..x.x..x..x
BD      trig 18 + 33 x.x.....xxx.....
```
- No hatching; all 16 columns in use. Nothing on the 32 wrap column.
- Me lane: trig "5".
- CONFIDENT (both passes agree on all 64 cells; ink widths sharply bimodal 4-5 vs 19-22).

## 3. SLOW ROCK        (annotated "F.B." beside the BD trigger number)
Hatched (unused) columns: 3, 7, 11, 15 (ruler labels 6, 14, 22, 30). 12 usable columns.
```
             cols   0 . 2 . 4 . 6 . 8 . 10. 12. 14.
CY      trig 20     xxx/xxx/xxx/xxx/
SD      trig 40     .../x../.../x../      -> SD on cols 4 and 12
BD      trig 18     x../.../x../.../      -> BD on cols 0 and 8
     ("F.B." printed under the BD number)
```
Reading with the '/' slots deleted (12 columns, 4 beats x 3 triplets):
```
CY   xxx xxx xxx xxx
SD   ... x.. ... x..
BD   x.. ... x.. ...
```
- CY plays all 12 triplet eighths; SD on beats 2 and 4; BD on beats 1 and 3. A textbook
  12/8 slow-rock shuffle. This is the pattern that PROVES the hatching verdict.
- Nothing on the 32 wrap column.
- Me lane: trig "5".
- CONFIDENT (both passes agree; BD col 0 is drawn as an open RING, ink width 13 not 20, so it
  is thinner than a filled dot but unmistakably a mark on the eyeball pass).

## 4. BALLAD           (annotated "F.B." beside the BD trigger number)
Hatched (unused) columns: 3, 7, 11, 15 (labels 6, 14, 22, 30). 12 usable columns.
```
             cols   0 . 2 . 4 . 6 . 8 . 10. 12. 14.
CY      trig 21     xxx/xxx/xxx/xxx/   + TWO extra off-grid marks, see below
SD      trig 40     .../x../.../x../      -> SD on cols 4 and 12
BD      trig 15' + 18'  x../..x/x../..x/  -> BD on cols 0, 6, 8, 14
     ("F.B." printed under the BD number)
```
With the '/' slots deleted (12 columns = 4 beats x 3 triplets):
```
CY   xxx xxx xxx xxx
SD   ... x.. ... x..
BD   x.. ..x x.. ..x
```
### The only OFF-GRID marks in the whole of Fig. 9
The CY lane carries two extra rings that sit on NO grid line — each is centred on the
MIDPOINT between two adjacent columns, i.e. on an ODD counter state:
- one between col 1 and col 2 (measured centre x = 480; the two lines are at 463 and 503,
  midpoint 483)
- one between col 9 and col 10 (measured centre x = 802; lines at 780 and 820, midpoint 800)
They are the same open-ring symbol as every other mark, drawn slightly smaller because they
are squeezed between two neighbouring rings. I checked EVERY lane of ALL EIGHT rhythms for
off-grid marks (systematic scan for any ink run >= 9 px wide whose centre is more than 6 px
from a grid line) and these two are the only ones in the figure.
What they MEAN is not settled. They are half a column early/late relative to the triplet
grid, so read literally the CY plays a two-stroke ruff into the "and" of beat 1 and of
beat 3. Recorded as observed; interpretation UNCERTAIN.
- Nothing on the 32 wrap column.
- Me lane: trig "5".
- CONFIDENT on all 48 grid cells; the two extra marks are CERTAIN to exist, UNCERTAIN in meaning.

## 5. WESTERN          (no hatching, no "F.B.")
Lane labels here are NOT the drum trio: they are sums, exactly as printed.
```
             cols   0 . 2 . 4 . 6 . 8 . 10. 12. 14.
C+HB    trig 40     ....x.......x...
LB      trig 35     ..:...:...:...:.      <- see below, ALL FOUR ARE OFF-GRID
LC+BD   trig 18     x.......x.......
```
- The LB lane has NO mark on any grid line. All four of its marks sit on the MIDPOINT between
  two columns (measured ring centroids x = 523.5, 684, 842, 1002.5; the midpoints of cols 2-3,
  6-7, 10-11, 14-15 are 523, 681, 839.5, 999.5). I write ':' at those four half-positions.
  In counter terms LB plays only ODD states, i.e. it is the exact off-beat of the other lanes.
  Verified by eye at high zoom in both halves of the bar: the rings clearly straddle the empty
  space between two printed lines, not a line.
- Nothing on the 32 wrap column.
- Me lane: trig "5".
- CONFIDENT.

## Status
- (in progress)

## 6. 6/8 MARCH        (hatched; carries "F.B.")
Lane labels as printed, top to bottom: CY, SD, BD, then Me (the metronome lane, drawn ON the
block border rule at y~1626-1631 rather than on a lane rule of its own).
Trigger numbers as printed, one per lane, right of the chart:
CY = "3", SD = "14 + 40", BD = "3", then the annotation "F.B.", Me = "5".
```
             cols   0 . 2 . 4 . 6 . 8 . 10. 12. 14.
CY      trig 3      x../x../x../x../
SD      trig 14+40  ..x/x.x/..x/x.x/
BD      trig 3      x../x../x../x../
Me      trig 5      x../x../x../x../
```
The SD line written out cell by cell so there is no ambiguity:
```
col      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
CY       x  .  .  /  x  .  .  /  x  .  .  /  x  .  .  /
SD       .  .  x  /  x  .  x  /  .  .  x  /  x  .  x  /
BD       x  .  .  /  x  .  .  /  x  .  .  /  x  .  .  /
Me       x  .  .  /  x  .  .  /  x  .  .  /  x  .  .  /
```
- HATCHED COLUMNS: 3, 7, 11, 15. Geometry, recorded as seen rather than idealised: each hatch
  band is a strip of short diagonal strokes about half a column wide that occupies the RIGHT
  half of the cell between columns k-1 and k and ends exactly at the line for k (measured
  stroke fragments at x 525-547, 673-706, 859-867, 1002-1026 against grid lines 543, 701,
  859, 1019). I read that as crossing out column 3/7/11/15, which is consistent with the
  inherited SLOW ROCK proof and with the marks: every lane here has marks on even columns
  0..14 and NOTHING on 3, 7, 11 or 15.
- SURVIVING SLOTS: 12 (cols 0,1,2,4,5,6,8,9,10,12,13,14) = TWO BARS OF 6/8, six eighth-notes
  each. Slot numbering 0..11 below.
  - CY  -> slots 0, 3, 6, 9   = eighths 1 and 4 of each bar, i.e. the two dotted-quarter beats.
  - BD  -> slots 0, 3, 6, 9   = same, cymbal and bass drum locked together on the beats.
  - Me  -> slots 0, 3, 6, 9   = the metronome clicks those same two beats per bar.
  - SD  -> slots 2, 3, 5, 8, 9, 11 = eighths 3, 4 and 6 of EACH bar (the two bars are
    identical). Read as a march snare figure: a pickup on the "a" of beat 1, the downbeat of
    beat 2, and the "a" of beat 2 leading into the next bar.
- Nothing on the col-16 (ruler 32) wrap line in any lane.
- The whole block drifts slightly right of the inherited calibration, +1.5 px at col 0 growing
  to +7 px at col 16; every mark still snaps unambiguously (marks measure 19-21 px wide, bare
  grid lines 3-5 px).
- CONFIDENT on all 64 cells (4 lanes x 16 columns), including the empty SD col 8, which I
  checked at high zoom because it breaks the otherwise-per-bar symmetry of the even columns.

## 7. JAZZ WALTZ   (hatched; read in the MAIN SESSION after the third agent stalled)
Lanes CY, SD, BD (+ Me below). Hatched columns 3, 7, 11, 15, as in 6/8 March.
```
col      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
CY       x  ?  x  /  .  .  .  /  x  ?  x  /  .  .  .  /
SD       .  .  x  /  .  x  .  /  .  .  x  /  .  x  .  /
BD       x  .  .  /  .  .  .  /  x  .  .  /  .  .  .  /
```
The '?' cells are the interesting part and they are NOT dots: in each bar the CY lane carries a
LARGE OPEN RING sitting clearly BETWEEN columns 1 and 2, i.e. on an ODD counter state (state 3,
and state 19 in bar 2), drawn visibly larger than the small filled blobs used for on-column marks.
This INDEPENDENTLY REPRODUCES the finding the Latin-half agent made from Bolero: the chart's real
resolution is ONE COUNTER STATE (32 per bar) and the ruled columns are only the even half. Two
different halves of the same page, two readers, same conclusion.
UNCERTAIN: the exact odd state (3 versus a hair either side) is not pinned to better than half a
state, and I did not attempt to measure the ring's centroid.
CONFIDENT on every on-column cell above.
Musically, deleting the hatched columns leaves 6 slots per bar (2 bars shown): bass on beat 1,
snare on beats 2 and 3, ride on beat 1 + a swung offbeat + beat 2.

## 8. WALTZ   (hatched; read in the MAIN SESSION)
Lanes CY, SD, BD (+ Me). Hatched columns 3, 7, 11, 15.
```
col      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
CY       x  .  .  /  .  .  .  /  x  .  .  /  .  .  .  /
SD       .  .  x  /  .  x  .  /  .  .  x  /  .  x  .  /
BD       x  .  .  /  .  .  .  /  x  .  .  /  .  .  .  /
```
CONFIDENT. No off-grid marks in this rhythm; every mark sits on a ruled column.
Surviving slots per bar: 0,1,2,4,5,6 = SIX, so two bars of 3/4 with an eighth-note grid, each
beat two slots wide. Read that way: BD and CY on beat 1, SD on beats 2 and 3.

### CROSS-MACHINE CHECK, and it passes
The Ace Tone FR-2L waltz (1969) reads: Cy and Bd on beat 1, and the other two lanes on beats 2
and 3. The Roland TR-77 waltz (1972) reads: CY and BD on beat 1, SD on beats 2 and 3. Two
different manufacturers, two different documents, two different clock structures (24 counts per
bar divided by three versus 32 states per bar with four columns struck out), and the same
musical answer. Neither reading was made with the other in view.


# ===== source file: tr77_latin.md =====

# Roland TR-77 (1972) preset pattern chart — LATIN half (Fig. 10)

## Provenance
- Document: "ELECTRONIC MUSICAL INSTRUMENT / ROLAND RHYTHM INSTRUMENT" service manual,
  7th edition, printed Japan Nov 1976, 32 pages.
  https://archive.org/download/roland_Roland_TR-77_Service_Manual/Roland_TR-77_Service_Manual.pdf
- Chart: PDF page 15 (printed page "- 13 -"), section "6-3. Rhythm Pattern, B. Latin (Fig. 10)".
- Image read: `<scratch>/pg-002.png`, native resolution (2399x3235), 300 dpi BITONAL NEGATIVE
  (ink is WHITE on black; threshold ink = pixel > 128).
- Scope: Fig. 10 "B. Latin" only, TWO stacked blocks on the RIGHT of the page:
  upper block B: RHUMBA, BEGUINE, CHA-CHA, MAMBO, SAMBA 1, SAMBA 2
  lower block C: BOSSANOVA, BAION, BOLERO, TANGO
  Fig. 9 (Jazz, left) NOT touched.
- Template calibration (Jazz block, previous agent, NOT my answer): 17 grid lines at
  x = 424 463 503 543 584 622 661 701 742 780 820 859 900 940 980 1019 1057, pitch 39.6 px,
  ruler 0 2 4 ... 32 => ONE CHART COLUMN = 2 counter states, 16 columns/bar + the 32 wrap.

## Calibration (mine, derived per block)

Method: column ink profile (ink = pixel > 128) over each block's own lane y-range, peaks merged
within 14 px, then a least-squares line x = x0 + k*pitch over the ruler index k = 0..16
(k counts RULER LABELS 0,2,4,...,32, so k = chart column, one column = 2 counter states).

### UPPER block B (RHUMBA, BEGUINE, CHA-CHA, MAMBO, SAMBA 1, SAMBA 2)
Profile window y = 470..2010, x = 1400..2130. Found exactly **17** grid lines:
  1457.5 1499.5 1539.8 1576.0 1617.0 1654.0 1696.5 1731.5 1773.5 1811.5
  1850.5 1889.8 1932.8 1965.0 2007.0 2045.8 2083.2
LS fit: **pitch 39.06 px, x0 = 1459.9**, max residual 4.1 px (~10% of pitch).
Fitted column centres (col 0..16, col 16 = the "32" wrap column):
  1459.9 1499.0 1538.0 1577.1 1616.2 1655.2 1694.3 1733.3 1772.4 1811.5
  1850.5 1889.6 1928.6 1967.7 2006.8 2045.8 2084.9
(compare Jazz block: pitch 39.6, x0 424 — same pitch, so the whole page is drawn to one scale.)

### LOWER block C (BOSSANOVA, BAION, BOLERO, TANGO)
Profile window y = 2150..2890, x = 1400..2130. At t=0.5*max, 12 lines resolved cleanly and their
spacing identifies their ruler indices unambiguously (0-8, 11, 12, 16):
  k=0 1473 | 1 1514 | 2 1553 | 3 1590.5 | 4 1628.5 | 5 1668.5 | 6 1708.5 | 7 1747 | 8 1785
  | 11 1902 | 12 1939 | 16 2091
(indices 9,10,13,14,15 were swallowed by the BOLERO hatching + dense dot rows, not absent.)
LS fit: **pitch 38.65 px, x0 = 1475.11**, max residual 2.50 px.
Fitted column centres (col 0..16):
  1475.1 1513.8 1552.4 1591.1 1629.7 1668.4 1707.0 1745.7 1784.3 1823.0
  1861.6 1900.3 1938.9 1977.6 2016.2 2054.9 2093.5

### What the 16 columns represent
The ruler is the machine's 32-state counter, printed every 2 states, so ONE CHART COLUMN = 2
counter states = a SIXTEENTH note of a 4/4 bar (32 states / 4 beats = 8 states per beat = 4
columns per beat). Unlike the Ace Tone FR-2L chart (which rules only 4 of 6 sub-positions per
beat and so has unruled, unusable slots), the TR-77 chart rules ALL 16 columns of the bar for the
4/4 rhythms, i.e. plain sixteenths with no swung/straight union. Column 16 is the "32" wrap
= the downbeat of the next bar, drawn only as a boundary; marks on it are noted separately.
**QUALIFIED LATER IN THIS FILE:** nine of the ten rhythms put every mark on a ruled column, but
BOLERO does not - it also uses the ODD counter states, i.e. positions exactly halfway between two
ruled columns. See "A SECOND FINDING THAT CHANGES THE DATA FORMAT" below before treating the
16-column grid as the chart's full resolution.

### CORRECTION — THE PAGE IS SKEWED, a straight column list is wrong
My first fit (a single x per column) was checked against the printed ruler ticks and against the
grid verticals inside a rhythm, and they DISAGREED by up to 19 px (half a column). Cause: the scan
/ print is sheared — the vertical grid lines lean RIGHT as y increases, ~0.7 degrees. Measured by
fitting the 17 grid lines independently in 139 thin y-slices through block B:

  **block B:  x(col k, row y) = 1446.767 + 0.012096*y + 39.0577*k**   (max residual 1.09 px)
  **block C:  x(col k, row y) = 1440.26  + 0.0121*y   + 38.6875*k**

Block C's anchor is its own printed ruler (17 ticks at y=2871: 1475.0 ... 2094.0, pitch 38.6875);
its skew slope is borrowed from block B (block C is too dot-dense to fit 17 lines per slice).
Cross-check that validates the model: block B's ruler ticks measured at y=2005 give col0 = 1470.0,
and the skew model predicts 1471.0 — so ruler ticks and grid verticals are the SAME positions,
no offset. Beat ticks (the diamond-headed ones) fall on cols 0, 4, 8, 12, confirming 4 columns
per beat / 4 beats per bar.

### Mark detection method
Marks are open CIRCLES (diameter ~19 px) centred on a lane-rule x grid-line intersection.
I sample four points at (x +/- 6, y +/- 7) from each intersection — off the lane rule and off the
vertical grid line, but ON a circle's rim. Score 4 = mark, 0 = bare rule; the score histogram is
almost entirely 4s and 0s, so this is a threshold-free reading, and EVERY lane below was also
confirmed by eye on a numbered-grid crop at ~100 px per column. No blob/Hough detection was used.

### Known scan artifact
The RHUMBA M lane carries a horizontally-stretched OVAL blob midway between cols 1 and 2 (and a
similar smear near cols 9/10) that is NOT on any grid column. Adjacent-circle ink smear, not a
mark. Recorded so nobody re-reads it as a half-position hit.

## Hatching verdict (BOLERO is the only hatched rhythm in Fig. 10)

MEASURED, in a y band that contains no lane rule and no marks (y 2716-2752, below BOLERO's BD
lane), so the only ink there is grid verticals and hatching. Four diagonally hatched bands, each
crossing EVERY lane of the rhythm including Me:

| band | columns          | width       | counter states |
|------|------------------|-------------|----------------|
| 1    | 2.60 .. 3.12     | 0.52 col    | 5.2 .. 6.2     |
| 2    | 6.63 .. 7.13     | 0.49 col    | 13.3 .. 14.3   |
| 3    | 10.64 .. 11.11   | 0.47 col    | 21.3 .. 22.2   |
| 4    | 14.57 .. 15.09   | 0.52 col    | 29.1 .. 30.2   |

So a band is **exactly ONE counter state wide (= half a chart column)**, there is one per beat,
and it covers the SEVENTH of each beat's eight states: states 6, 14, 22, 30.

**Verdict, in two parts.**
1. "A hatched band is a counter state this rhythm does not use" is CONSISTENT with the data:
   BOLERO's own marks fall on states 0,3,4,7,8,11,12,15,16,19,20,23,24,25,26,27,28,31 and never
   on 6, 14, 22 or 30.
2. The stronger hypothesis carried over from the Jazz half - "a 32-state bar re-divided by three" -
   is **NOT supported by Bolero.** Removing 4 of 32 states leaves 28, which is not a multiple of 3;
   and Bolero's marks are not on a triplet grid at all. They sit on a **3:1 shuffled sixteenth
   grid**: within each 4-state eighth-note there are hits at offset 0 and offset 3, giving the
   pairs (0,3) (4,7) (8,11) (12,15) (16,19) (20,23), then a five-state run 24-25-26-27-28 and a
   final hit at 31. Whatever the hatching encodes for the 3-time Jazz rhythms, in Bolero it reads
   as a single blocked-out clock state per beat, not as a re-division by three. Fig. 9's hatched
   rhythms are another agent's scope and may well behave differently; I am only reporting Bolero.

## A SECOND FINDING THAT CHANGES THE DATA FORMAT: marks are not confined to the ruled columns

BOLERO's SD and LB marks sit alternately ON a ruled column and exactly HALFWAY BETWEEN two ruled
columns, i.e. on ODD counter states. So the chart's real resolution is ONE COUNTER STATE (32 per
bar), and the ruled columns are just the EVEN half of it. A 16-character-per-bar string cannot
express Bolero; it needs 32.

I therefore re-swept EVERY lane of all ten rhythms with a fine x-scan (annulus score computed at
every pixel along the lane, peaks picked, positions reported in fractional columns) instead of only
sampling the 17 ruled columns. Controls first: BOSSANOVA RS came back 0.06 / 3.11 / 6.14 / 10.12 /
13.09 and RHUMBA C came back 3.04 / 6.01 / 10.03 / 12.00, i.e. integers, so the scan does not
invent half-column peaks. RESULT: **BOLERO is the ONLY rhythm with off-column marks.** Every mark
in the other nine rhythms landed within 0.15 of a ruled column, so the 16-character strings below
are a faithful encoding for those nine.

The one loose end from that sweep: RHUMBA's M lane returned two extra peaks at columns **1.46 and
9.50** (odd counter states 3 and 19) on top of its 16 on-column marks. Both are horizontally
STRETCHED OVALS, wider than tall, unlike every real mark on the page, and both sit where two
adjacent circles nearly touch - so I read them as ink smear and did NOT count them. Flagged here
because if they are real, RHUMBA's maracas play two extra 32nds.

## Rhythms

### 1. RHUMBA  (block B, lane rules y = 498, 529.5, 560, 591, 622.5; Me below; border 684.5)
```
M      trig 21      xxxxxxxxxxxxxxxx     (all 16 - maracas on every sixteenth)
C      trig 26      x..x..x...x.x...     (= 0,3,6,10,12 : the RUMBA CLAVE)
HB     trig 39      ..x..x....x..x..     (2,5,10,13)
LB     trig 8       xx.xx...xx.xx...     (0,1,3,4,8,9,11,12)
LC     trig 15      ......xx......xx     (6,7,14,15)
```
No marks on the 32 wrap column (col 16) in any lane.
Me: trig 42 + 5 (lane read pending).
CONFIDENT. The C lane coming out as the textbook rumba clave (0,3,6,10,12) is independent
confirmation that the skew calibration and the one-column-per-two-states reading are right.
ONE BAR of 16 sixteenths.

Note on block layout, learned here: what I first took for the rhythm's bottom BORDER (y 684.5)
is actually the **Me metronome lane**. Every rhythm leaves one blank rule slot after its last
voice lane and puts Me in the slot after that (Me = last lane + 2 slots, ~62 px). The real block
border is below Me.

### 2. BEGUINE  (block B, rules y = 746, 777, 807.5, 838, [868.5], 900; Me 961)
```
M       trig 20     xxxxxxxxxxxxxxxx     (all 16)
C       trig 26     x..x..x...x.x...     (0,3,6,10,12 : rumba clave again)
HB      trig 1      .x.x.x.x.x.x.x.x     (ALL EIGHT ODD columns 1,3,5,7,9,11,13,15)
CY+LB   trig 4      .x.......x......     (1, 9)
[unlabelled rule at y 868.5]             (................  - completely EMPTY)
LC+BD   trig 2 (see note) x...x.x.x...x.x.     (0,4,6,8,12,14)
```
No marks on col 16 (the 32 wrap) in any lane.
Me: trig 42 + 5.
**LABEL/NUMBER MISALIGNMENT, recorded rather than resolved.** BEGUINE prints SIX lane rules but
only FIVE labels and FIVE trigger numbers. Measured y centres: labels M 744, C 780, HB 809,
CY+LB 841, LC+BD 900; numbers 20@743, 26@775, 1@805, 4@836, 2@865. So labels sit on rules
1,2,3,4,**6** while numbers sit on rules 1,2,3,4,**5**. Rule 5 (y 868.5) carries no label and no
marks at all; rule 6 (y 900) carries the LC+BD label and real marks. I therefore pair LC+BD with
the leftover number **2**, and flag rule 5 as a spurious/blank rule in the printing. Anyone using
this data should treat BEGUINE's LC+BD trigger number as the one soft fact in the rhythm.
HB col 15 was initially scored 2/4 and flagged; the later fine x-scan resolved a real ring
centred at column 14.86, so it is counted. The ONE soft fact left in BEGUINE is the LC+BD trigger
pairing described above.
ONE BAR of 16 sixteenths.

### 3. CHA-CHA  (block B, rules y = 1023 (GU), 1055, 1086, 1117, 1148; Me 1210)
```
GU     trig 6(+12)  SUSTAINED BARS, not dots - see prose below
M      trig 20      x?x?xxxxxxxxxxxx     (0,2,4..15 certain; cols 1 and 3 UNCERTAIN)
CB+HB  trig 41      x.x.xxx.x.x.xxx.     (0,2,4,5,6,8,10,12,13,14)
LB     trig 15      ......xx......xx     (6,7,14 certain; col 15 faint but present)
LC     trig 17      x..x..x.x..x..x.     (0,3,6,8,11,14)
```
No marks on col 16 in any lane. Me: trig 42 + 5.
**GU is a BAR lane** (guiro = a scrape, so the chart draws horizontal bars sitting just above the
lane rule, at y 1013-1016, instead of circles). Measured spans in column units:
0.0..1.4 | 2.0..2.5 | 3.0..3.5 | 4.0..5.15 | 6.0..6.5 | 7.0..7.5 | 8.0..9.45 | 10.0..10.5 |
11.0..11.5 | 12.0..12.4 | 14.0..14.5 (a further short bar at ~15 is likely but was not cleanly
resolved). Read musically: per beat, one LONG scrape covering the beat's first two sixteenths,
then two SHORT ticks on the third and fourth sixteenth - repeated on beats 1, 2, 3; beat 4 is
less clear (the long bar there measured either 12.0..12.4 or 12.0..13.5 depending on the sample
band). I did NOT fake dots for this lane.
UNCERTAIN: M cols 1 and 3. Both carry a bowtie/arrowhead ink shape rather than a clean open
circle, in a visibly degraded band of the scan (the lane rule itself is dashed there). Every other
Latin M lane on this chart is all-16, so these are most likely broken circles, but I did not see
a circle, so they stay '?'. Also UNCERTAIN: whether GU has a bar at col 15.
ONE BAR of 16 sixteenths.

### 4. MAMBO  (block B, SIX lanes; rules y = 1271 (GU), 1302, 1333, 1364, 1395, 1426; Me 1488)
```
GU     trig 6(+12)  SUSTAINED BARS - same figure as CHA-CHA (see below)
M      trig 20      xxxxxxxxxxxxxxxx     (all 16 - every column verified by eye at high zoom)
CB+LB  trig 42'     x.?.x.x.x.x.x.x.     (0,4,6,8,10,12,14; col 2 UNCERTAIN)
HB     trig 1       .x.x.x.x.x.x.x.x     (ALL EIGHT ODD columns 1,3,5,7,9,11,13,15)
LC     trig 15      ......xx......xx     (6,7,14,15)
BD     trig 17      x..x..x.x..x..x.     (0,3,6,8,11,14 - the tumbao figure)
```
No marks on col 16 in any lane. Me: trig 42 + 5.
GU bars measured (column units, upper half of the bar row y 1260-1267): 0.0..1.4 | 2.0..2.5 |
3.0..3.5 | 4.0..5.4 | 6.0..6.5 | 7.0..7.45 | 8.2..9.47, i.e. the SAME long-then-two-short figure
per beat as CHA-CHA, which is corroborated by both rhythms carrying the identical GU trigger
number 6(+12). The right half of the bar row did not resolve cleanly; I did not extrapolate it.
UNCERTAIN: CB+LB col 2 (score 0.24 against 0.39-0.41 for its neighbours' real marks and 0.09-0.13
for bare crossings; at high zoom it is a small ink mark, not a ring). BD col 8 was a *broken*
ring, not a clean one, but there is unmistakably more ink than at a bare crossing (cols 9 and 10
alongside it show only the rule and the vertical), and 0,3,6,8,11,14 is the tumbao - counted.
Otherwise CONFIDENT. ONE BAR of 16 sixteenths.

### 5. SAMBA 1  (block B, rules y = 1552.5, 1583, 1615, 1647, 1678.5; Me 1741.5)
This rhythm is printed FAINTLY (thin, broken rules), so scores run lower than elsewhere; the
readings below are the ones where the annulus score and the eye agree.
```
CY     trig 14      ..x...x...x...x.     (2,6,10,14 - the eighth-note UPBEATS)
M      trig 20      xxxxxxxxxxxxxxxx     (all 16; every column carries ink, scores 0.20-0.61)
HB     trig 37      x.x....x.x.x....     (0,2,7,9,11)
LB     trig 40      ....x.......x...     (4,12)
BD     trig 3       x...x...x...x...     (0,4,8,12 - four on the floor)
```
No marks on col 16 in any lane. Me: trig 5 (note: NOT 42+5 like the first four rhythms).
CONFIDENT. Every one of the 5 lanes was then re-checked ring-by-ring on two 8-column zooms
(cols 0-8 and 8-16) and all 30 marks and all 50 empties matched the numeric read exactly,
including the faint ones (CY 2 and 6, LB 4). HB = 0,2,7,9,11 is asymmetric but it is what is
drawn: rings at 0, 2, 7, 9, 11 and clean bare crossings at every other column.
ONE BAR of 16 sixteenths.

### 6. SAMBA 2  (block B, rules y = 1803, 1834, 1865.5, 1896.5, 1928; Me 1991)
```
TB     trig 13         ..xx..xx..xx..xx     (2,3,6,7,10,11,14,15 - pairs on each beat's 2nd+3rd 16th)
TB'    trig 20(330K)   xxxxxxxxxxxxxxxx     (all 16)
CB+LB  trig 38         x.x.x..x.x.xx...     (0,2,4,7,9,11,12)
HB     trig 15         ......xx......xx     (6,7,14,15)
LC     trig 3          x...x...x...x...     (0,4,8,12)
```
No marks on col 16 in any lane. Me: trig 5.
Note the primed label **TB'** is printed exactly so (tambourine, second trigger), and its trigger
number is annotated **20(330K)** - a component value, not a trigger index, carried verbatim.
There is a stray isolated ink dot at roughly (x 1502, y 1955), i.e. between the LC lane and Me
around col 13-14, sitting on no rule at all. Scan speck, not a mark.
UNCERTAIN: CB+LB col 4 (score 0.27; visible as a small mark by eye, weaker than its neighbours).
Otherwise CONFIDENT. ONE BAR of 16 sixteenths.

--- END OF UPPER BLOCK B (6 rhythms) ---

### 7. BOSSANOVA  (block C, rules y = 2193, 2225, 2257, 2289; Me 2353)
```
CY     trig 14      ..x...x...x...x.     (2,6,10,14 - eighth-note upbeats)
HH     trig 20      xxxxxxxxxxxxxxxx     (all 16)
RS     trig 27      x..x..x...x..x..     (0,3,6,10,13 : the BOSSA NOVA CLAVE)
BD     trig 9       x..xx...x..xx...     (0,3,4,8,11,12)
Me     trig 5       x...x...x...x...     (quarters)
```
No marks on col 16. The RS lane coming out as the textbook bossa clave (3+3+4+3+3) and the BD lane
as the bossa/baiao bass figure is a second independent confirmation of the calibration.
CONFIDENT (scores 0.54-0.86 for marks against 0.00-0.12 for bare crossings, and every lane
re-checked by eye on the gridded crop).
ONE BAR of 16 sixteenths - see the two-bar note at the end.

### 8. BAION  (block C, rules y = 2415, 2447, 2479, 2511; Me 2575)
```
TB'    trig 20(330K)  xxxxxxxxxxxxxxxx     (all 16)
CB+LB  trig 9         x..xx...x..xx...     (0,3,4,8,11,12 - identical to BOSSANOVA's BD, and the
                                            two share the trigger number 9)
HB     trig 15        ......xx......xx     (6,7,14,15)
LC     trig 23        ...........xx...     (11,12)
Me     trig 5         x...x...x...x...     (quarters)
```
No marks on col 16. Label printed **TB'** (primed) exactly as shown.
CONFIDENT (marks 0.53-0.84, bare crossings 0.00-0.13).
ONE BAR of 16 sixteenths.

### 9. BOLERO  (block C, rules y = 2634, 2666, 2698; Me 2762)  **carries "F.B."**
The only hatched rhythm in Fig. 10, and the only one whose marks leave the ruled columns. Given
that, the honest encoding is COUNTER STATES (32 per bar), not 16 columns.
```
SD     trig 31   states 0,3,4,7,8,11,12,15,16,19,20,23,24,25,26,27,28,31
LB     trig 31   states 0,3,4,7,8,11,12,15,16,19,20,23,24,25,26,27,28,31   (IDENTICAL to SD -
                 which is why both lanes carry the same trigger number 31)
BD     trig 3    states 0,8,16,24            (= cols 0,4,8,12: the four beats, dead on the grid)
Me     (no number printed beside it; the rhythm's right-hand column reads 31 / 31 / 3 / F.B. / 5)
                 states 0,8,16,24 (quarters)
"F.B."  printed in the number column below BD, above the Me number. Recorded verbatim; I do not
        know what it stands for. The same annotation appears on Fig. 9's triplet-feel rhythms.
```
For the record, the same three lanes read at the 16 RULED COLUMNS ONLY (which is what a
column-sampling reader would have produced, and which silently drops half the pattern):
```
SD     31    x.x.x.x.x.x?xxxx     LB  31   x.x.x.x.x.x.xxx?     BD  3   x...x...x...x...
```
UNCERTAIN: the five-state run at 24-25-26-27-28 (a roll/fill at the end of the bar). Those five
rings visibly overlap, and a peak-finder on overlapping rings can split or merge blobs. I read
them as five distinct rings at high zoom (each outline separately visible) and their measured
centres are 12.04 / 12.58 / 13.08 / 13.62 / 14.06 columns, evenly ~0.52 col apart, which is one
counter state - so I believe them, but this is the one place in the whole Latin half where I would
want a second pair of eyes. Everything else in Bolero is CONFIDENT.
NOT the same shape as the other nine rhythms: read the states, not a 16-step grid.

### 10. TANGO  (block C, rules y = 2821, 2852, 2882; Me 2944)
```
CY     trig 16      .......x.......x     (7, 15)
SD     trig 34      x.x.x.xxx.x.x.xx     (0,2,4,6,7,8,10,12,14,15)
BD     trig 42'     x.x.x.x.x.x.x.x.     (0,2,4,6,8,10,12,14 - straight eighths)
Me     trig 42 + 5  x.x.x.x.x.x.x.x.     (eighths)
```
No marks on col 16. Trigger 42' is printed primed. CONFIDENT: marks 0.60-0.84, bare crossings
0.00-0.13, no off-column peaks, and all three lanes re-read by eye on the gridded crop.
ONE BAR of 16 sixteenths.

--- END OF LOWER BLOCK C (4 rhythms) ---

## The Me (metronome) lane — bonus, and it turned out to be self-checking
Me is drawn with small filled DIAMONDS, not open circles, and it is the only lane whose rule is
dashed rather than solid. Read from the gridded crops:

| rhythm | Me trigger | Me pattern |
|---|---|---|
| RHUMBA, BEGUINE, CHA-CHA, MAMBO | 42 + 5 | EIGHTHS - cols 0,2,4,6,8,10,12,14 |
| SAMBA 1, SAMBA 2 | 5 | QUARTERS - cols 0,4,8,12 |
| BOSSANOVA, BAION, BOLERO | 5 | QUARTERS - cols 0,4,8,12 |
| TANGO | 42 + 5 | EIGHTHS - cols 0,2,4,6,8,10,12,14 |

The correlation is exact and was not assumed: every rhythm whose Me trigger is **42 + 5** clicks
eighths and every one whose Me trigger is **5** clicks quarters. TANGO was the prediction and it
checked out (its Me is well below the block, at y 2944, and I cropped it specifically to test
this). That gives the trigger-number column an independent sanity check.

## ONE BAR OR TWO?
**One bar each - all ten Latin rhythms fit in a single 32-state bar, and none of them is written
out over two.** This is worth stating explicitly because bossanova, baion and samba are genuinely
two-bar rhythms in real playing, with an asymmetric clave that only closes over 2 bars. On this
chart they do not: BOSSANOVA's RS lane is the bossa clave compressed into ONE bar (0,3,6,10,13),
and BAION/SAMBA repeat every bar. That is a fact about the TR-77, not a reading error - a 1972
diode-matrix machine has one bar of pattern memory per preset, and the asymmetry that a human
player spreads over two bars is folded into one. The chart's own geometry agrees: there is exactly
one ruler per block reading 0..32, one grid of 16 columns, and col 16 (state 32) is the wrap - and
**no lane in any of the ten rhythms carries a mark on col 16.**

## SUMMARY TABLE — all ten Latin rhythms done
| # | rhythm | lanes | status |
|---|---|---|---|
| 1 | RHUMBA | M C HB LB LC + Me | CONFIDENT |
| 2 | BEGUINE | M C HB CY+LB [blank] LC+BD + Me | CONFIDENT except the LC+BD trigger pairing |
| 3 | CHA-CHA | GU M CB+HB LB LC + Me | CONFIDENT except M cols 1,3 and GU col 15 |
| 4 | MAMBO | GU M CB+LB HB LC BD + Me | CONFIDENT except CB+LB col 2 |
| 5 | SAMBA 1 | CY M HB LB BD + Me | CONFIDENT |
| 6 | SAMBA 2 | TB TB' CB+LB HB LC + Me | CONFIDENT except CB+LB col 4 |
| 7 | BOSSANOVA | CY HH RS BD + Me | CONFIDENT |
| 8 | BAION | TB' CB+LB HB LC + Me | CONFIDENT |
| 9 | BOLERO | SD LB BD + Me | CONFIDENT except the 24-28 roll |
| 10 | TANGO | CY SD BD + Me | CONFIDENT |
Nothing UNREAD except: the right half of CHA-CHA's and MAMBO's GU bar rows (bar spans did not
resolve cleanly there and I refused to extrapolate them).


# ===== source file: sgs_m252.md =====

# SGS M252 rhythm-generator LSI — FACTORY STANDARD MASK pattern tables

## Provenance

- **Source document:** *1979 SGS MOS And Special COS/MOS Data Book, 1st Edition* (SGS / SGS-ATES).
- **URL:** http://www.bitsavers.org/components/sgs/_dataBooks/1979_SGS_MOS_And_Special_COS_MOS_1stEd.pdf
- **Pages (as MEASURED, correcting the working assumption this file started from):**
  - PDF **123** (printed 129) = `TABLE 1 (M252 AA)`, rhythms **1-10**
  - PDF **124** (printed 130) = Table 1 rhythms **11-15** (top) AND `TABLE 2 (M252 AD)` rhythms **1-5** (bottom)
  - PDF **125** (printed 131) = Table 2 rhythms **6-15**
  - PDF **116** (printed 122) = `CONNECTION DIAGRAMS` — the OUTPUT-to-pin pinout and the two
    per-mask instrument pinouts (this is what answers the instrument question; see the mapping section)
  - PDF **120** (printed 126) = `TYPICAL APPLICATIONS` Fig. 1, which repeats the instrument names
  - (PDF 134-136 hold the M253 equivalents — out of scope; characterised in the closing note.)
  - Printed page = PDF page + 6 throughout this section.
- **Embedded scan resolution (measured, `pdfimages -list`):** PDF page 123 is **3902 x 5109, gray,
  1 bpc, jbig2, 600 x 600 ppi**; pages 124 and 125 are 3893 x 5102, also 600 ppi. Rendering with
  `pdftoppm -r 600` therefore reproduces the scan at native resolution with no upsampling.
- **Both tables are FULLY TRANSCRIBED: 15 rhythms each, 30 total, all CONFIDENT.**
- **Chart format:** rows = counts 1..32, columns = OUTPUT 1..8, `X` = a hit. Cells past each
  rhythm's reset count are greyed/shaded to mark counts that do not exist for that rhythm.

## NAMING — read this before using the data

These are the **SGS FACTORY STANDARD MASK** pattern sets, as published by SGS in their own
databook. Two mask options are printed: **M252 AA** (Table 1) and **M252 AD** (Table 2).

**This is NOT the Elgam pattern set.** Elgam's organs used **custom masks** of this same chip
(marked `M252 D1 AE` and `M252 D1 AF` on Elgam's own Carousel schematic). SGS never published
customer masks. Elgam's 15-rhythm dial matches no published standard mask: rhythms 1-3 and 7
share names and slots with the factory mask, but 8-15 provably do not. Presenting these tables
as Elgam's patterns would be a fabrication.

**OUTPUT 1..8 are chip pins, not instruments.** No instrument names are assigned here unless the
databook itself assigns them; any such assignment is quoted with its page number in the
"Output-to-sound mapping" section at the end of this file.

## Notation

Per rhythm: one line per output, one character per count from 1 to that rhythm's reset count.
`x` = an X printed in the table, `.` = a blank cell. Counts past the reset count are not
written at all (they are the greyed cells).

---

## TABLE 1 (M252 AA) — factory standard mask

### Measured provenance details

- `pdfimages -list` on PDF page 123: **3902 x 5109, gray, 1 bpc, jbig2, 600 x 600 ppi** — matches
  the reported figure exactly. Pages 124 and 125: 3893 x 5102, also 600 ppi. `pdftoppm -r 600`
  therefore renders at native resolution (no upsampling).
- PDF page 123 carries the **printed page number 129** and the caption `TABLE 1 (M252 AA)`.
  It holds **rhythms 1-10** in two bands of five.
- Row/column geometry was located by ink-density line detection (no blob/circle detection was
  used anywhere); marks were then read by eye from 600 dpi crops that include the count column
  and the OUTPUT 1-8 header digits.

### IMPORTANT FINDING ABOUT RHYTHM NAMES

**In this databook, the rhythm blocks are captioned only `RHYTHM 1`, `RHYTHM 2`, ... with NO
instrument-style or dance-style names printed.** There is no "(SAMBA)" or similar parenthetical
on the chart. The left header column reads `COUNT FOR 32`. Whether any names appear in the
surrounding text is checked and reported separately below. Do not attach dance names to these
numbers from any other source.

---

### TABLE 1 (M252 AA) — RHYTHM 1
Printed name: **none printed** (captioned only "RHYTHM 1").
Reset count: **24** (counts 25-32 are greyed = unused).

```
count:   123456789012345678901234
OUT 1    x...........x...........
OUT 2    ....x...x.....x.x...x...
OUT 3    ........................
OUT 4    ........................
OUT 5    ........................
OUT 6    x...........x...........
OUT 7    ........................
OUT 8    ........................
```
Status: **CONFIDENT.** All 8 lanes read from a 600 dpi crop; OUT 3,4,5,7,8 are wholly empty for
counts 1-24. Faint dashed scan artifacts cross counts 19-23 in the OUT 5-7 region; these are
broken rule lines, not X glyphs.

### TABLE 1 (M252 AA) — RHYTHM 2
Printed name: **none printed** (captioned only "RHYTHM 2").
Reset count: **24** (counts 25-32 greyed = unused).

```
count:   123456789012345678901234
OUT 1    x...........x...........
OUT 2    ...xx...x......xx..xx..x
OUT 3    ........................
OUT 4    ........................
OUT 5    ........................
OUT 6    ........x...........x...
OUT 7    .......................x
OUT 8    ........................
```
Status: **CONFIDENT.** X positions: OUT 1 = 1, 13. OUT 2 = 4, 5, 9, 16, 17, 20, 21, 24.
OUT 6 = 9, 21. OUT 7 = 24. OUT 3, 4, 5, 8 wholly empty over counts 1-24.

### TABLE 1 (M252 AA) — RHYTHM 3
Printed name: **none printed** (captioned only "RHYTHM 3").
Reset count: **32** (no greyed cells; the block runs the full 32 counts).

```
count:   12345678901234567890123456789012
OUT 1    x...x...x...x.x.x...x...x...x.x.
OUT 2    x...x...x...x.x.x...x...x...xxxx
OUT 3    ................................
OUT 4    ................................
OUT 5    ................................
OUT 6    ..............x...............x.
OUT 7    ................................
OUT 8    ................................
```
Status: **CONFIDENT.** OUT 3, 4, 5, 7, 8 wholly empty. OUT 6 carries exactly two marks (15, 31).

### TABLE 1 (M252 AA) — RHYTHM 4
Printed name: **none printed** (captioned only "RHYTHM 4").
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x.......x.......x.......x.......
OUT 2    ....x.......x.......x.......xxxx
OUT 3    ................................
OUT 4    ................................
OUT 5    ................................
OUT 6    x.......x.......x.......x.......
OUT 7    ................................
OUT 8    ................................
```
Status: **CONFIDENT.** OUT 3, 4, 5, 7, 8 wholly empty. Two isolated single-pixel specks
(OUT 6 at count 18, OUT 7 at count 32) are scan dirt, not X glyphs, and were NOT recorded.

### TABLE 1 (M252 AA) — RHYTHM 5
Printed name: **none printed** (captioned only "RHYTHM 5").
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x.......x.......x.......x.......
OUT 2    ....x.......x.......x.......x...
OUT 3    ................................
OUT 4    ................................
OUT 5    ................................
OUT 6    x.......x.......x...............
OUT 7    ....x..x....x..x.......xx..xx..x
OUT 8    ................................
```
Status: **CONFIDENT.** Read by eye AND independently cross-checked by per-cell ink density over
the detected cell rectangles: every mark scores 0.13-0.17 ink fraction while every blank scores
<=0.03, so the two classes do not overlap. Note OUT 6 has NO mark at count 25 (unlike OUT 1),
and the heavily-populated lane is OUT 7, not OUT 8 (OUT 8 is empty throughout).

> **Verification note for rhythms 1-5.** Each was read by eye from a 600 dpi crop AND independently
> cross-checked by per-cell ink fraction over the detected cell rectangles. The two methods agreed
> on every one of the 5 x 8 x 32 cells. Separation is unambiguous: an X scores 0.11-0.24 ink while
> blank cells score <=0.07 (the page has some faint broken rule lines and specks in the count 2-14
> region, all well below the mark level). A GREYED cell is distinguishable not by level but by
> uniformity: the shaded band reads 0.22-0.29 across **all eight** lanes at once, which no pattern
> of X marks produces. This is how each reset count below was established.

### TABLE 1 (M252 AA) — RHYTHM 6
Printed name: **none printed** (captioned only "RHYTHM 6").
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x.......x.......x.......x.......
OUT 2    ....x.......x.x.....x.......x...
OUT 3    ................................
OUT 4    ................................
OUT 5    ................................
OUT 6    ....x...............x...........
OUT 7    ............x.x...........x...x.
OUT 8    x.......x.......x.......x.......
```
Status: **CONFIDENT.** OUT 3, 4, 5 wholly empty. The ink cross-check showed borderline values
(0.10-0.12) in OUT 1 / OUT 2 at counts 29-32; the 600 dpi crop shows these are broken rule
lines and page noise, NOT X glyphs, so they are recorded as blanks.

### TABLE 1 (M252 AA) — RHYTHM 7
Printed name: **none printed** (captioned only "RHYTHM 7").
Reset count: **24** (counts 25-32 greyed = unused; the shaded band reads 0.21-0.26 ink uniformly
across all eight lanes).

```
count:   123456789012345678901234
OUT 1    x.........x.x.........x.
OUT 2    ......x...........x.....
OUT 3    ........................
OUT 4    ........................
OUT 5    ........................
OUT 6    ........................
OUT 7    x.x.x.x.x.x.x.xxx.x.x.x.
OUT 8    ........................
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 3, 4, 5, 6, 8
wholly empty. OUT 7 is the dense lane: every ODD count 1-23, PLUS one extra mark at count 16
(the only even-count mark in that lane) — that asymmetry is in the print, not a misread.

### TABLE 1 (M252 AA) — RHYTHM 8
Printed name: **none printed** (captioned only "RHYTHM 8").
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x.x.....x.x...x.x.x.....x.x...x.
OUT 2    ....x..x.x..x.......x..x.x..x..x
OUT 3    ................................
OUT 4    ................................
OUT 5    ................................
OUT 6    ................................
OUT 7    ....x.......x.......x.......x...
OUT 8    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 3, 4, 5, 6 wholly
empty. OUT 8 is every ODD count 1-31 with no exceptions; OUT 7 is a plain 4-mark lane (5, 13,
21, 29).

### TABLE 1 (M252 AA) — RHYTHM 9
Printed name: **none printed** (captioned only "RHYTHM 9").
Reset count: **24** (counts 25-32 greyed = unused).

```
count:   123456789012345678901234
OUT 1    x..x..x..x..x..x..x..x..
OUT 2    x.xx.xx.xx.xx.xx.xx.xx.x
OUT 3    ........................
OUT 4    ........................
OUT 5    ........................
OUT 6    ........................
OUT 7    x..x..x..x..x..x..x..x..
OUT 8    ........................
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 3, 4, 5, 6, 8
wholly empty. OUT 1 and OUT 7 carry the SAME 8 counts (every third count from 1); OUT 2 fills
in a 16-mark lane. Over a 24-count reset this is a triple-division (12/8-type) grid rather than
the duple grid of rhythms 1-8 — stated as an observation about the count spacing only, not as a
claim about any dance name.

### TABLE 1 (M252 AA) — RHYTHM 10
Printed name: **none printed** (captioned only "RHYTHM 10").
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x.....x.....x...x.....x.....x...
OUT 2    x...x...x.x...x...x...x.x...x...
OUT 3    ..x...x...x...x...x...x...x...x.
OUT 4    x...x...x...x...x...x...x...x...
OUT 5    ............x.x.............x.x.
OUT 6    ................................
OUT 7    ................................
OUT 8    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). This is the densest
rhythm in Table 1, using SIX of the eight lanes. OUT 6 and OUT 7 are wholly empty (their
slightly elevated ink at counts 1-11, up to 0.10, is bleed from the neighbouring dense lanes and
page noise, well under the 0.13 mark level). OUT 8 is every ODD count 1-31.

**End of PDF page 123 (printed p.129): Table 1 rhythms 1-10 complete.**

---

## STRUCTURAL CORRECTION TO THE PAGE MAP (found on PDF page 124)

PDF page 124 (printed page **130**) carries **two** things, which changes the brief's page map:

- **top half:** the CONTINUATION of `TABLE 1 (M252 AA)` — **rhythms 11 to 15**, again captioned
  with bare numbers and **no names**. So **Table 1 holds 15 rhythms total (1-15)**.
- **bottom half:** the START of `TABLE 2 (M252 AD)` — **rhythms 1 to 5**. Table 2 therefore begins
  on page 124, not on page 125 as the brief assumed; page 125 continues it.

**And this is the key discovery: TABLE 2 IS THE NAMED SET.** Its blocks are captioned with dance
names in parentheses exactly as the brief described:

- `RHYTHM 1 (WALTZ)` · `RHYTHM 2 (TANGO)` · `RHYTHM 3 (MARCH)` · `RHYTHM 4 (SWING)` ·
  `RHYTHM 5 (MAMBO)`

**Table 1 (M252 AA) has no printed rhythm names at all; Table 2 (M252 AD) does.** These are two
different factory standard masks of the same chip, and only the AD mask's rhythms are named in the
databook. Do not carry Table 2's names across onto Table 1's numbers: they are different masks and
nothing in the databook equates rhythm N of AA with rhythm N of AD. (Spot-check confirming they are
genuinely different: Table 1 rhythm 1 resets at 24 with marks on OUT 1/2/6, while Table 2 rhythm 1
"WALTZ" also resets at 24 but places its marks differently — see the transcriptions.)

### TABLE 1 (M252 AA) — RHYTHM 11
Printed name: **none printed** (captioned only "RHYTHM 11").
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x.......x...x...x.......x...x...
OUT 2    ..x...x...x...x...x...x...x...x.
OUT 3    ..x...x...x...x...x...x...x...x.
OUT 4    ..x...............x.............
OUT 5    ................................
OUT 6    ......x...............x.........
OUT 7    ..xxx.............xxx...........
OUT 8    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 5 wholly empty.
OUT 2 and OUT 3 carry IDENTICAL lanes (all odd counts 3-31). OUT 7 fires in two three-count
bursts (3-4-5 and 19-20-21), the only consecutive-count runs in Table 1. A small diagonal tick
in OUT 6 at count 14 (ink 0.09) is page dirt, not an X, and is recorded as blank.

### TABLE 1 (M252 AA) — RHYTHM 12
Printed name: **none printed** (captioned only "RHYTHM 12").
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x.....x.x...x...x.....x.x...x...
OUT 2    x...x...x...x.x.x...x...x...x.x.
OUT 3    x...x...x...x.x.x...x...x...x.x.
OUT 4    ............x.x.............x.x.
OUT 5    ................................
OUT 6    ................................
OUT 7    ................................
OUT 8    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 5, 6, 7 wholly
empty. OUT 2 and OUT 3 again carry IDENTICAL lanes. OUT 8 is every ODD count 1-31. A single
speck in OUT 2 at count 12 (ink 0.03) is dirt and is recorded as blank.

### TABLE 1 (M252 AA) — RHYTHM 13
Printed name: **none printed** (captioned only "RHYTHM 13").
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x...........x...x...........x...
OUT 2    x.....x.x.......x.....x.x.......
OUT 3    ............x.x.............x.x.
OUT 4    x.....x.x.......x.....x.x.......
OUT 5    ......................x.x.......
OUT 6    ................................
OUT 7    ................................
OUT 8    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 6 and OUT 7 wholly
empty. OUT 2 and OUT 4 carry IDENTICAL lanes (1, 7, 9, 17, 23, 25). OUT 8 is every ODD count
1-31. The OUT 2 mark at count 1 reads a slightly lighter 0.12 ink (a thin impression) but is an
unmistakable X in the crop against neighbours at 0.02-0.05.

### TABLE 1 (M252 AA) — RHYTHM 14
Printed name: **none printed** (captioned only "RHYTHM 14").
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x.......x.......x.......x.......
OUT 2    x...x...x.....x...x...x.x.......
OUT 3    ............x.x.............x...
OUT 4    x...x...x.......x.x...x.x.......
OUT 5    ................................
OUT 6    ................................
OUT 7    ....x.......x.......x.......x...
OUT 8    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 5 and OUT 6 wholly
empty. OUT 2 and OUT 4 are ALMOST identical but genuinely differ at two counts: OUT 2 fires at
15 and not 17, OUT 4 fires at 17 and not 15. Both methods agree on that asymmetry, so it is in
the print. OUT 8 is every ODD count 1-31.

### TABLE 1 (M252 AA) — RHYTHM 15
Printed name: **none printed** (captioned only "RHYTHM 15").
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x.....x.x.....x.x.....x.x.....x.
OUT 2    x.....x.....x.......x.....x.....
OUT 3    ................................
OUT 4    ................................
OUT 5    ................................
OUT 6    ................................
OUT 7    ....x.x.....x.x.....x.x.....x.x.
OUT 8    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 3, 4, 5, 6 wholly
empty. OUT 8 is every ODD count 1-31 and is printed noticeably heavier here (0.24-0.29 ink) than
elsewhere in the table; its even-count cells still read 0.00-0.06, so there is no confusion with
greying (which would raise all eight lanes at once).

**TABLE 1 (M252 AA) COMPLETE: all 15 rhythms transcribed, all CONFIDENT.**

Reset counts across Table 1: rhythms **1, 2, 7, 9 reset at 24**; rhythms **3, 4, 5, 6, 8, 10,
11, 12, 13, 14, 15 reset at 32**. No rhythm in Table 1 carries a printed name.

---

## TABLE 2 (M252 AD) — factory standard mask, the NAMED set

Begins on PDF page 124 (printed p.130), continues on page 125. Blocks are captioned with dance
names in parentheses, e.g. `RHYTHM 1 (WALTZ)`.

### TABLE 2 (M252 AD) — RHYTHM 1 (WALTZ)
Printed name: **WALTZ** (caption reads `RHYTHM 1 (WALTZ)`).
Reset count: **23** — counts **24-32 are greyed** as unused, so the last usable count is 23.

```
count:   12345678901234567890123
OUT 1    x...........x..........
OUT 2    ....x...x......xx...x..
OUT 3    .......................
OUT 4    .......................
OUT 5    .......................
OUT 6    .......................
OUT 7    x...........x..........
OUT 8    .......................
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 3, 4, 5, 6, 8
wholly empty.

> **Note on the reset count, which is the one genuinely surprising number in this file.** A waltz
> would be expected to reset at 24, so this was measured three independent ways before being
> written down: (a) at 1.6x magnification the grey block’s top edge is level with the rule line
> directly BELOW the row labelled 23, with 24 the first greyed label; (b) the grey top edge is at
> y=4175 px against a detected row rule at y=4179, i.e. the same line, 45 px away from where a
> reset of 24 would put it; (c) the white span from the header rule to the grey edge measures
> 23.0 row heights. Nine rows are greyed (24-32). **This is what the databook prints; it is not
> corrected toward the musically expected 24.**

> **This block is also the concrete proof that AA and AD are different masks.** Table 1 rhythm 1
> and Table 2 rhythm 1 (WALTZ) are similar in shape but differ in three ways: the second lane
> fires at count 16 here versus 15 there, the doubling lane is OUT 7 here versus OUT 6 there, and
> the reset is 23 here versus 24 there. Same chip, different masked content.

### TABLE 2 (M252 AD) — RHYTHM 2 (TANGO)
Printed name: **TANGO**.
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x.......x.......x.......x.......
OUT 2    ................................
OUT 3    ................................
OUT 4    ............................xxxx
OUT 5    ............................xxxx
OUT 6    x.......x.......x.......x...x...
OUT 7    ................................
OUT 8    ................................
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 2, 3, 7, 8 wholly
empty. OUT 4 and OUT 5 fire ONLY on the last four counts (29-32), as a consecutive run — a fill
at the end of the bar. OUT 6 doubles OUT 1 and adds one extra mark at count 29. This block has
heavy broken-rule noise across counts 19-28 (ink 0.02-0.06); none of it reaches mark level.

### TABLE 2 (M252 AD) — RHYTHM 3 (MARCH)
Printed name: **MARCH**.
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x.......x.......x.......x.......
OUT 2    ....x...x...x.......x.......x.x.
OUT 3    ................................
OUT 4    ................................
OUT 5    ............................x.x.
OUT 6    ................................
OUT 7    x.......x.......x.......x.......
OUT 8    ................................
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 3, 4, 6, 8 wholly
empty. OUT 1 and OUT 7 carry the SAME four counts (1, 9, 17, 25).

### TABLE 2 (M252 AD) — RHYTHM 4 (SWING)
Printed name: **SWING**.
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x...x...x...x...x...x...x...x..x
OUT 2    x...x.......x.......x..........x
OUT 3    x...x.......x.......x..........x
OUT 4    ................................
OUT 5    x...x..xx...x..xx...x..xx...x..x
OUT 6    ................................
OUT 7    ..............................x.
OUT 8    ................................
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 4, 6, 8 wholly
empty. OUT 2 and OUT 3 carry IDENTICAL lanes. OUT 7 fires exactly ONCE, at count 31. Note that
four lanes place a mark on count 32 (the last count) as well as count 1. The count-1 row is
printed lightly here (ink 0.11-0.13) but the glyphs are unmistakable against 0.00 neighbours.

### TABLE 2 (M252 AD) — RHYTHM 5 (MAMBO)
Printed name: **MAMBO**.
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x.x.x.....x.x...x.x.x.....x.x...
OUT 2    .......xx.x.x.x.......x...xx..x.
OUT 3    ................................
OUT 4    ..xxxxxx..........x.x.x.........
OUT 5    x...x...x.x...x...x...x.x.x.x...
OUT 6    ................................
OUT 7    x.....x.....x.......x...x.......
OUT 8    x.......x.......x.......x.......
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 3 and OUT 6 wholly
empty. The densest rhythm in Table 2, using six lanes. OUT 4 contains a SIX-count consecutive run
(counts 3-8) — the longest unbroken run anywhere in either table; both methods read all six.

**End of PDF page 124 (printed p.130): Table 1 rhythms 11-15 and Table 2 rhythms 1-5 complete.**

---

### PDF page 125 (printed p.131) — Table 2 rhythms 6-15, PRINTED NAMES

Table 2 also runs to 15 rhythms. The remaining captions, read off the page:

| # | printed name | # | printed name |
|---|---|---|---|
| 6 | SLOW ROCK | 11 | RUMBA |
| 7 | BEAT | 12 | BEGUINE |
| 8 | SAMBA | 13 | BAJON |
| 9 | BOSSA NOVA | 14 | FOX TROT |
| 10 | CHA-CHA | 15 | SHUFFLE |

So the full **M252 AD** factory name list is: WALTZ, TANGO, MARCH, SWING, MAMBO, SLOW ROCK, BEAT,
SAMBA, BOSSA NOVA, CHA-CHA, RUMBA, BEGUINE, BAJON, FOX TROT, SHUFFLE. This is the set the brief's
"RHYTHM 8 (SAMBA)" example comes from, and it confirms the caption style.

### TABLE 2 (M252 AD) — RHYTHM 6 (SLOW ROCK)
Printed name: **SLOW ROCK**.
Reset count: **24** (counts 25-32 greyed = unused).

```
count:   123456789012345678901234
OUT 1    x.........x.x...........
OUT 2    ......x...........x...x.
OUT 3    ........................
OUT 4    ........................
OUT 5    x.x.x.x.x.x.x.x.x.x.x.x.
OUT 6    x.....x.....x.....x.....
OUT 7    ........................
OUT 8    ........................
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 3, 4, 7, 8 wholly
empty. OUT 5 is every ODD count 1-23 (12 marks over the 24-count bar); OUT 6 fires every sixth
count (1, 7, 13, 19).

### TABLE 2 (M252 AD) — RHYTHM 7 (BEAT)
Printed name: **BEAT**.
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x.....x.x.......x.....x.x.......
OUT 2    ....x.......x.......x.......x.x.
OUT 3    ................................
OUT 4    ................................
OUT 5    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUT 6    ..x.......x.......x.......x.....
OUT 7    ................................
OUT 8    ................................
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 3, 4, 7, 8 wholly
empty. OUT 5 is every ODD count 1-31; OUT 6 fires every eighth count starting at 3.

### TABLE 2 (M252 AD) — RHYTHM 8 (SAMBA)
Printed name: **SAMBA**.
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x.....x.x.....x.x.....x.x.....x.
OUT 2    ....x.x.....x.x.....x...x.x.x.x.
OUT 3    ................................
OUT 4    x.....x.....x.......x.....x...x.
OUT 5    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUT 6    ................................
OUT 7    x.....x.....x.......x.....x.....
OUT 8    ..x.x.....x.x.......x.....x.....
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 3 and OUT 6 wholly
empty — OUT 6 shows a steady 0.01-0.05 down its whole length, which is a DASHED VERTICAL RULE
printed inside the column, not a series of faint marks (it never varies row to row the way marks
do). OUT 5 is every ODD count 1-31. OUT 4 and OUT 7 share the counts 1, 7, 13, 21, 27, with OUT 4
adding count 31.

### TABLE 2 (M252 AD) — RHYTHM 9 (BOSSA NOVA)
Printed name: **BOSSA NOVA**.
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x.x.....x.x.....x.x.....x.x.....
OUT 2    x.....x.....x.......x.....x.....
OUT 3    x.....x.....x.......x.....x.....
OUT 4    x.....x.x...x.......x.....x.....
OUT 5    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUT 6    ....x.x.x.........x...x.........
OUT 7    x.........x...x...........x.x.x.
OUT 8    x.....x.....x.......x.....x.....
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). This is the ONLY rhythm
in either table that uses ALL EIGHT outputs. OUT 2, OUT 3 and OUT 8 carry identical lanes
(1, 7, 13, 21, 27) and OUT 4 adds count 9 to that same figure. OUT 5 is every ODD count 1-31.
OUT 2 also carries a dashed vertical rule (a steady 0.03-0.07 in the non-mark rows); the five
real marks there read 0.16-0.20, so they are not in doubt.

### TABLE 2 (M252 AD) — RHYTHM 10 (CHA-CHA)
Printed name: **CHA-CHA**.
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x...x...x...x...x...x...x.x.x...
OUT 2    x.x.............x..............x
OUT 3    ................................
OUT 4    x.x.....x...x.x.x.x.....x.x.x.xx
OUT 5    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUT 6    ................................
OUT 7    x...x...x...x...x...x...x.x.x...
OUT 8    x...x...x...x...x...x...........
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 3 and OUT 6 wholly
empty. OUT 1 and OUT 7 carry IDENTICAL lanes; OUT 8 is the first six counts of that same figure
(1, 5, 9, 13, 17, 21) and then stops. OUT 5 is every ODD count 1-31. OUT 2 and OUT 4 both place a
mark on count 32.

### TABLE 2 (M252 AD) — RHYTHM 11 (RUMBA)
Printed name: **RUMBA**.
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x.....x.....x...x.....x.....x...
OUT 2    x...................x...x.......
OUT 3    ................................
OUT 4    ............................x.x.
OUT 5    ..x.x.x.x.x...x...x.x...x.x...x.
OUT 6    ................................
OUT 7    x.....x.....x...x.......x.......
OUT 8    x.......x.......x...........x...
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 3 and OUT 6 wholly
empty. OUT 5 is an IRREGULAR odd-count lane here — it fires on 3, 5, 7, 9, 11, 15, 19, 21, 25, 27,
31 and specifically SKIPS counts 1, 13, 17, 23 and 29, which is unlike the every-odd OUT 5 lanes of
BEAT, SAMBA, BOSSA NOVA and CHA-CHA. Both methods read those five gaps, so they are in the print.

### TABLE 2 (M252 AD) — RHYTHM 12 (BEGUINE)
Printed name: **BEGUINE**.
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x...........x...x.......x...x...
OUT 2    ..x...............x.............
OUT 3    ..x.......x...x...x.......x...x.
OUT 4    ..x...x.xxxx......x...x.........
OUT 5    x.xxx.x.x.x.x.x.x.xxx.x.x.x.x.x.
OUT 6    ................................
OUT 7    ..x...............x.............
OUT 8    x.....x.....x.......x...x.......
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 6 wholly empty.
OUT 2 and OUT 7 carry the same two marks (3 and 19). OUT 4 contains a FOUR-count consecutive run
(9-12) and OUT 5 breaks its odd-count pattern with even-count marks at 4 and 20 — both read the
same way by eye and by ink.

### TABLE 2 (M252 AD) — RHYTHM 13 (BAJON)
Printed name: **BAJON**.
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x...........x...x...........x...
OUT 2    ......................x.x.......
OUT 3    ............x.x.............x.x.
OUT 4    x.....x.x.......x.....x.x.......
OUT 5    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUT 6    ................................
OUT 7    x.....x.x.......x.....x.x.......
OUT 8    ................................
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 6 and OUT 8 wholly
empty (OUT 6 shows a flat 0.02-0.07 down its length, a dashed vertical rule rather than marks).
OUT 4 and OUT 7 carry IDENTICAL lanes (1, 7, 9, 17, 23, 25). OUT 5 is every ODD count 1-31.

### TABLE 2 (M252 AD) — RHYTHM 14 (FOX TROT)
Printed name: **FOX TROT**.
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    x.......x.......x.......x.......
OUT 2    ....x.......x.x.....x.......x...
OUT 3    ................................
OUT 4    ................................
OUT 5    x.......x.......x.......x.......
OUT 6    ............x.x...........x...x.
OUT 7    ....x...............x...........
OUT 8    ................................
```
Status: **CONFIDENT.** OUT 3, 4, 8 wholly empty. OUT 1 and OUT 5 carry IDENTICAL lanes
(1, 9, 17, 25).

One judged cell, recorded as BLANK: **OUT 6 at count 16** carries an ink mark measuring 0.11,
which is just under the mark level for this page (real marks here read 0.14-0.20). At 600 dpi it
is a single SLANTED BLOT sitting between the count-15 and count-16 rows, not the two crossed
strokes of an X. Since OUT 6 genuinely fires at 15 immediately above it, this reads as ink squeezed
out of that glyph. Recorded as no hit; flagged here so the judgement is visible rather than buried.

### TABLE 2 (M252 AD) — RHYTHM 15 (SHUFFLE)
Printed name: **SHUFFLE**.
Reset count: **24** (counts 25-32 greyed = unused).

```
count:   123456789012345678901234
OUT 1    x..x..x..x..x..x..x.x...
OUT 2    x.xx..x.xx..x.xx..x.xxxx
OUT 3    ........................
OUT 4    ........................
OUT 5    ........................
OUT 6    x..x.xx..x..x..x.xx..x..
OUT 7    ........................
OUT 8    ........................
```
Status: **CONFIDENT** (eye read and ink cross-check agree on every cell). OUT 3, 4, 5, 7, 8
wholly empty. Like Table 1 rhythm 9, this is a TRIPLE-division grid: the spine is every third
count (1, 4, 7, 10, 13, 16, 19, 22) over a 24-count reset.

> **One deliberate anomaly, confirmed both ways: OUT 1 fires at count 21, NOT 22.** It follows the
> triplet spine 1, 4, 7, 10, 13, 16, 19 and then breaks it, landing on 21 while OUT 6 goes to 22.
> Ink at OUT 1 count 21 is 0.16 and at count 22 is 0.00, and the crop shows the same, so this is
> not a misalignment of my row grid. Recorded as printed.

**TABLE 2 (M252 AD) COMPLETE: all 15 rhythms transcribed, all CONFIDENT.**

Reset counts across Table 2: **WALTZ resets at 23**; **SLOW ROCK and SHUFFLE reset at 24**; the
other twelve (TANGO, MARCH, SWING, MAMBO, BEAT, SAMBA, BOSSA NOVA, CHA-CHA, RUMBA, BEGUINE,
BAJON, FOX TROT) **reset at 32**.

---

## OUTPUT-TO-INSTRUMENT MAPPING — the databook DOES assign instruments

**Source: PDF page 116 (printed page 122), section `CONNECTION DIAGRAMS`.** This page prints THREE
16-pin outlines side by side, and together they answer the mapping question rigorously.

**(a) the generic pinout**, which names the outputs by NUMBER — the same numbering the pattern
tables use:

| pin | function | pin | function |
|---|---|---|---|
| 1 | INPUT 4 | 16 | INPUT 2 |
| 2 | INPUT 8 | 15 | INPUT 1 |
| 3 | **OUTPUT 8** | 14 | **OUTPUT 4** |
| 4 | **OUTPUT 7** | 13 | **OUTPUT 3** |
| 5 | **OUTPUT 6** | 12 | **OUTPUT 2** |
| 6 | **OUTPUT 5** | 11 | **OUTPUT 1** |
| 7 | EXTERNAL RESET / DOWN-BEAT | 10 | V_GG |
| 8 | CLOCK | 9 | V_SS |

**(b) and (c) two more outlines**, captioned `Standard content configuration M252 B1 AA` and
`Standard content configuration M252 B1 AD`, which put an INSTRUMENT NAME on each of those same
eight pins. A bracket in the AA outline labels pins 3-6 and 11-14 as `INSTRUMENT DRIVE OUTPUTS`.

Combining them gives the mapping. **Note the two masks do NOT agree** — same pin, different
instrument — which is another reason never to read one table's lanes with the other's names:

| table column | pin | M252 **AA** instrument | M252 **AD** instrument |
|---|---|---|---|
| OUTPUT 1 | 11 | BASS DRUM | BASS DRUM |
| OUTPUT 2 | 12 | SNARE DRUM OR CLAVES `*` | SNARE DRUM or CONGA DRUM `****` |
| OUTPUT 3 | 13 | HIGH BONGO | HIGH BONGO |
| OUTPUT 4 | 14 | LOW BONGO | LOW BONGO |
| OUTPUT 5 | 6 | CONGA DRUM | HIGH HAT |
| OUTPUT 6 | 5 | LONG CYMBALS | SHORT CYMBALS |
| OUTPUT 7 | 4 | SHORT CYMBALS | LONG CYMBALS or CLAVES `***` |
| OUTPUT 8 | 3 | MARACAS | COW BELL |

### The footnotes, quoted (same page 122)

Four footnotes hang off those outlines. Quoting them (OCR text layer, checked against the 300 dpi
render):

- `*` (AA pin 12) — "This output must be connected so as to drive the "snare drum" when the rhythms
  from 1 to 9 (see rhythm selection) are selected, and the "claves" when the rhythms from 10 to 15
  (see rhythm selection) are selected."
- `**` (pin 7) — "This pin generates a down-beat trigger which can be used to drive an external lamp
  to indicate the first beat of the first bar of each rhythm."
- `***` (AD pin 4) — "This output must be connected so as to drive the "long cymbals" when the
  rhythms number 1, 3, 4, 12 and 14 are generated, and the "claves" when the rhythms number 5, 8, 9,
  10, 11 and 13 are generated."
- `****` (AD pin 12) — "This output must be connected so as to drive the "snare drum" when the
  rhythms number 1, 3, 4, 6, 7, 9, 12, 14 and 15 are generated, and the "conga drum" when the
  rhythms number 5, 8, 10, 11 and 13 are generated."

So two of the eight outputs are DOUBLE-DUTY by design: the same pin drives a different instrument
depending on which rhythm is selected. The chip does not switch the sound; the footnote is telling
the set-builder to wire an external selector.

### These footnotes independently CONFIRM the transcription

The `***` and `****` footnotes name exactly which rhythms use those two outputs, which is a fact
about the pattern content — so it can be checked against the lanes transcribed above, and it was,
BEFORE the pinout table was read:

- `***` says the LONG CYMBALS / CLAVES output is used in rhythms 1, 3, 4, 12, 14 and 5, 8, 9, 10, 11,
  13 — union = **{1,3,4,5,8,9,10,11,12,13,14}**, and silent in {2,6,7,15}. In the Table 2
  transcription above, the output that is non-empty in exactly those eleven rhythms and empty in
  exactly TANGO, SLOW ROCK, BEAT and SHUFFLE is **OUT 7** — and the pinout independently says
  OUTPUT 7 = pin 4 = "LONG CYMBALS or CLAVES". **Match.**
- `****` says the SNARE / CONGA output is used in rhythms 1, 3, 4, 6, 7, 9, 12, 14, 15 and 5, 8, 10,
  11, 13 — union = every rhythm EXCEPT 2. In the transcription, the output non-empty in all fifteen
  rhythms except TANGO is **OUT 2** — and the pinout says OUTPUT 2 = pin 12 = "SNARE DRUM or CONGA
  DRUM". **Match.**

Two independent agreements, on lanes read before the mapping was known. They also make musical
sense of the data: OUTPUT 5 in the AD mask is the HIGH HAT, which is why OUT 5 runs on every odd
count in BEAT, SAMBA, BOSSA NOVA, CHA-CHA and BAJON, and OUTPUT 1 is the BASS DRUM, which is why
OUT 1 sits on 1, 9, 17, 25 in MARCH, TANGO and FOX TROT.

> **Scope of this mapping.** It comes from the "standard content configuration" outlines, so it
> applies to the AA and AD factory masks — the same two masks whose patterns are tabulated. It says
> nothing about any customer mask. Also note Figure 1 on PDF page 120 (printed 126), `Rhythm system
> (standard contents)`, redundantly shows the same instrument names wired to an amplifier and
> speaker for both masks; it agrees with the table above.

---

## SUMMARY INDEX (generated by parsing this file's own transcription blocks)

30 rhythms transcribed, 15 per mask. Every block was machine-checked to have exactly 8 lanes each
of exactly its reset-count length; **0 integrity failures**. "outs" lists the outputs that carry at
least one mark; "hits" is the total number of X marks in the block.

### TABLE 1 — M252 AA (no printed names)

| # | reset | outputs used | hits |
|---|---|---|---|
| 1 | 24 | 1,2,6 | 9 |
| 2 | 24 | 1,2,6,7 | 13 |
| 3 | 32 | 1,2,6 | 24 |
| 4 | 32 | 1,2,6 | 15 |
| 5 | 32 | 1,2,6,7 | 20 |
| 6 | 32 | 1,2,6,7,8 | 19 |
| 7 | 24 | 1,2,7 | 19 |
| 8 | 32 | 1,2,7,8 | 39 |
| 9 | 24 | 1,2,7 | 32 |
| 10 | 32 | 1,2,3,4,5,8 | 51 |
| 11 | 32 | 1,2,3,4,6,7,8 | 48 |
| 12 | 32 | 1,2,3,4,8 | 48 |
| 13 | 32 | 1,2,3,4,5,8 | 38 |
| 14 | 32 | 1,2,3,4,7,8 | 41 |
| 15 | 32 | 1,2,7,8 | 37 |

### TABLE 2 — M252 AD (named)

| # | name | reset | outputs used | hits |
|---|---|---|---|---|
| 1 | WALTZ | **23** | 1,2,7 | 9 |
| 2 | TANGO | 32 | 1,4,5,6 | 17 |
| 3 | MARCH | 32 | 1,2,5,7 | 16 |
| 4 | SWING | 32 | 1,2,3,5,7 | 32 |
| 5 | MAMBO | 32 | 1,2,4,5,7,8 | 47 |
| 6 | SLOW ROCK | 24 | 1,2,5,6 | 22 |
| 7 | BEAT | 32 | 1,2,5,6 | 31 |
| 8 | SAMBA | 32 | 1,2,4,5,7,8 | 50 |
| 9 | BOSSA NOVA | 32 | 1,2,3,4,5,6,7,8 | 56 |
| 10 | CHA-CHA | 32 | 1,2,4,5,7,8 | 56 |
| 11 | RUMBA | 32 | 1,2,4,5,7,8 | 31 |
| 12 | BEGUINE | 32 | 1,2,3,4,5,7,8 | 46 |
| 13 | BAJON | 32 | 1,2,3,4,5,7 | 38 |
| 14 | FOX TROT | 32 | 1,2,5,6,7 | 19 |
| 15 | SHUFFLE | 24 | 1,2,6 | 32 |

### The footnote cross-check, run over the parsed data

- Footnote `***` requires the LONG CYMBALS/CLAVES output to be used in exactly rhythms
  {1,3,4,5,8,9,10,11,12,13,14}. Parsed AD data: **OUT 7 is used in exactly {1,3,4,5,8,9,10,11,12,13,14}**
  and is empty in exactly {2,6,7,15}. Pinout says OUTPUT 7 = pin 4 = LONG CYMBALS or CLAVES. Agree.
- Footnote `****` requires the SNARE/CONGA output to be used in every rhythm except 2. Parsed AD
  data: **OUT 2 is used in all fifteen except rhythm 2 (TANGO)**. Pinout says OUTPUT 2 = pin 12 =
  SNARE DRUM or CONGA DRUM. Agree.
- Consistent with footnote `*` (AA pin 12 = OUTPUT 2, snare for rhythms 1-9 and claves for 10-15,
  i.e. all fifteen): parsed AA data has **OUT 2 used in all 15 rhythms**.

### Per-rhythm confidence

**All 30 rhythms: CONFIDENT. 0 UNCERTAIN. 0 UNREAD.** Every rhythm was read by eye from a 600 dpi
crop that includes both the count column and the OUTPUT 1-8 header digits, AND cross-checked by
per-cell ink fraction over the located cell rectangles; the two methods agreed on all
30 x 8 x (23-32) cells. Individual judged cells (specks, blots, dashed rules) are named in the
status line of the rhythm they belong to. The only genuinely surprising readings, each measured
several ways and flagged in place, are: **WALTZ resetting at 23**, Table 1 rhythm 7's lone
even-count mark at 16 in an otherwise all-odd lane, and SHUFFLE's OUT 1 landing on 21 instead of
continuing its triplet spine to 22.

---

## CLOSING NOTE — the M253 tables (PDF pages 134-136), NOT transcribed

The assigned scope (M252 Table 1 then Table 2) is complete, so the remaining time was spent
CHARACTERISING the M253 tables rather than starting a transcription that could only be left
half-finished. Nothing below is a pattern reading; it is the shape of the pages, verified by looking
at them, so a later pass can go straight to work.

**Layout, which exactly mirrors the M252 section:**

| PDF page | printed page | contents |
|---|---|---|
| 134 | 141 | `TABLE 1 (M253 AA)` — rhythms 1-10, two bands of five |
| 135 | 142 | Table 1 rhythms 11-15 (top) + `TABLE 2 (M253 AC)` rhythms 1-5 (bottom) |
| 136 | 143 | Table 2 rhythms 6-10 (top) + rhythms 11-12 (bottom) |

**What the M253 tables would add:**

1. **A second device with the same two-mask pattern.** M253's masks are **AA** and **AC** (note AC,
   not M252's AD). The chart format is identical: rows = counts 1-32 under a `COUNT FOR 32` header,
   columns = OUTPUT 1-8, X = a hit, greyed cells past each rhythm's reset count. The same
   grid-geometry-plus-ink-cross-check method used here transfers directly.
2. **`TABLE 1 (M253 AA)` is again UNNAMED** — bare `RHYTHM 1` … `RHYTHM 15` captions, 15 rhythms,
   with greyed regions visible on rhythms 1, 2, 3 and 5 of the first band, so the mix of 24-count and
   32-count resets recurs.
3. **`TABLE 2 (M253 AC)` is the NAMED one, and it appears to reuse M252 AD's name list in the SAME
   numbered slots** — read off page 136: `RHYTHM 6 (SLOW ROCK)`, `RHYTHM 7 (BEAT)`,
   `RHYTHM 8 (SAMBA)`, `RHYTHM 9 (BOSSA NOVA)`, `RHYTHM 10 (CHA-CHA)`, `RHYTHM 11 (RUMBA)`,
   `RHYTHM 12 (BEGUINE)`. Those are M252 AD's slots 6-12 exactly. **But shared names must not be
   read as shared patterns** — that is precisely the error this file warns about for Elgam, and it
   would need checking cell by cell.
4. **M253 AC looks SHORTER: 12 rhythms, not 15.** The bottom band of page 136 holds only rhythms 11
   and 12 and the table border closes after BEGUINE, with no rhythms 13-15. If confirmed, AC offers
   twelve rhythms where AD offers fifteen, dropping BAJON, FOX TROT and SHUFFLE.
5. **An output-to-instrument mapping would have to be re-derived for M253, not inherited.** The
   mapping recorded above is read off the M252 connection diagrams and is explicitly labelled for the
   M252 AA and AD "standard content configuration" outlines. M253 has its own pinout pages earlier in
   its section, and since even the two M252 masks disagree on what OUTPUT 5, 6, 7 and 8 drive, M253
   cannot be assumed to match either.

**What is still unknown about M253** and would need the same treatment applied here: every reset
count, every one of the 8 x N lanes per rhythm, and whether its footnotes carry the same
double-duty output notes (which for M252 turned out to be the cross-check that confirmed the whole
transcription).


# ===== source file: elgam.md =====

# Elgam (Castelfidardo, IT) 1970s organs / rhythm boxes — are the preset rhythm PATTERNS documented publicly?

STATUS: COMPLETE (2026-08-19)

## ANSWER
**NO for Elgam itself, YES for the chip it used.** Elgam's own preset patterns are unpublished
and cannot be published: they are two CUSTOM mask ROMs (SGS `M252 D1 AE` / `M252 D1 AF`). But the
SGS M252/M253 *standard* content IS printed as real 600-dpi pattern grids in the 1979 SGS databook.
Full statement + confidence near the bottom under "## ANSWER (replaces the placeholder...)".

## URLs checked

| URL | What it was | Verdict |
|---|---|---|
| (none yet) | | |


### Search 1 — servicemanual.altervista.org (commercial PDF reseller)
| URL | What | Verdict |
|---|---|---|
| https://www.servicemanual.altervista.org/ELGAM_Service-Manual-Schematics.html | Commercial reseller (altervista) selling scanned service manuals. Returns HTTP 404 to curl/WebFetch BUT ships a full 122KB body — grep it, do not trust the status code. | SERVICE DOCS EXIST (paid, not viewable) |

Elgam models it advertises documents for (verbatim list, this is Tier-2 model-range data):
ELGAM 237 (schematics) · Alpha 110 (sch) · Alpha 120 / Delta 60 · Alpha 130 · Broadway 300 ·
Broadway 444 (circuit diagram) · Carousel · Concert · Derby · EP20 · ES2000 · Mistral · Mistral 200 ·
Mistral 200s · Mistral 210s · Palladium 220 · Palladium 230 · Piano · Ragtime · Recital · Ringo ·
Royal · Ruby 610 · Symphony 200 · Talisman · Talisman-Royal · The Entertainer 8 · The Entertainer 8-13
NOTE the thumbnail filename **Elgam_Ragtime_Rhythm_Unit_schematics.webp** — i.e. the Ragtime is a
standalone RHYTHM UNIT and its schematics were scanned. Best Tier-1 candidate so far.
No prices/pages read yet; nothing viewable without buying.

### Search 2 — elektrotanya.com (free-ish service manual archive)
| URL | What | Verdict |
|---|---|---|
| https://elektrotanya.com/elgam_recital_organ_sch.pdf/download.html | "ELGAM RECITAL ORGAN SCH", PDF, **5.3 MB, 19 pages**, Instrument Service Manual category. Real doc, exists. | SERVICE DOCS EXIST — download GATED (checkbox/captcha + likely login/wait); could not fetch bytes with curl. Contents unread, so unknown whether it prints a rhythm chart. |
| https://elektrotanya.com/elgam_recital_organ_sch.pdf (direct) | tried direct + referer | DEAD END (404 html) |
| https://elektrotanya.com/search.php?query=elgam | wrong path | DEAD END |

### Search 3 — SMEM (Swiss Museum for Electronic Music, Fribourg) — *** THE HIT ***
| URL | What | Verdict |
|---|---|---|
| https://www.smemmusic.ch/en/elgam-carousel-analog-synthesizer-and-drum-machine | Museum catalogue page, Elgam Carousel, **1976, Italy**. Text (quoted): "An integrated rhythm section with automatic accompaniment was unusual in the mid-1970s"; **15 rhythms that can be varied**, 3 arpeggiators (adjustable decay+volume), **3 polyphonic chord patterns**, independent volume for drums/bass/chords/arp, 25-key keyboard, VCO. NO rhythm names, NO pattern list on the page. | TIER 2 (spec + rhythm COUNT) + hosts a free service PDF |
| https://www.smemmusic.ch/sites/default/files/2026-05/elgam_carousel_sch.pdf | **FREE, DOWNLOADED OK** (15,337,357 bytes, PDF 1.6, 4 pages). Elgam Carousel schematics. Page 1 embeds ONE grayscale JPEG **23500 x 5000 px at 600 dpi** (14.6 MB) = a ~39in x 8.3in schematic roll. `pdftotext` yields NOTHING (pure scan, no OCR layer). | TIER 1 CANDIDATE — resolution is ample; contents not yet read |
| https://www.smemmusic.ch/en/elgam-montreal-analog-electronic-piano | Elgam Montreal electronic piano, early 1970s | CONTEXT ONLY (no rhythm) |
| https://www.smemmusic.ch/index.php/en/elgam-3049-electronic-analog-combo-organ | Elgam 3049 combo organ, early 1970s | CONTEXT ONLY |
| http://www.smemmusic.com/en/elgam-244-analog-combo-organ | Elgam 244 combo organ, mid 1970s | CONTEXT ONLY |
| (SMEM about page) | SMEM dates the **Elgam company: active 1968-1982**, portable organs + pianos in the early years | TIER 2 (company dates) |

### Search 4 — archive.org (already done by a prior agent, recorded in sources2.md, NOT redone)
| URL | What | Verdict |
|---|---|---|
| https://archive.org/advancedsearch.php?q=elgam (fulltext) | numFound = **3**, all irrelevant (2 CIA reading-room docs, 1 user favourites list "ElgaM Favorites") | DEAD END — there is NO Elgam material on archive.org at all |

### PDF identity note
`pdfinfo carousel.pdf` Title = **"ELGAM Ragtime/Carousel schematics"** (Creator: MS Word 9.0, Distiller 4.0,
created 2000-03-18, modified 2021-12-03). So this ONE free PDF covers BOTH the Carousel organ AND the
**Ragtime** standalone rhythm unit — the same doc the altervista shop sells as two items.

## *** TIER 2 CONFIRMED — the Elgam rhythm list, READ OFF THE SCAN ***
Source: `https://www.smemmusic.ch/sites/default/files/2026-05/elgam_carousel_sch.pdf`
page 1 (the only page with a scan), embedded JPEG 23500x5000 @600dpi, **table titled "RHYTHMS CODE"**,
located approx x 4800-6000, y 800-3400 px in that image (crop saved as `car_tab1.png`).
Columns: RHYTHM | IN8 | IN4 | IN2 | IN1  (all four IN labels are OVERLINED = active low).
Transcribed verbatim, in printed order (15 rhythms — matches SMEM's "15 rhythms"):

| # | RHYTHM (as printed) | IN8 | IN4 | IN2 | IN1 |
|---|---|---|---|---|---|
| 1 | WALTZ | 1 | 1 | 1 | 0 |
| 2 | JAZZ WALTZ | 1 | 1 | 0 | 1 |
| 3 | TANGO | 1 | 1 | 0 | 0 |
| 4 | POLKA | 1 | 0 | 1 | 1 |
| 5 | FOX TROT | 1 | 0 | 1 | 0 |
| 6 | SWING | 1 | 0 | 0 | 1 |
| 7 | SLOW ROCK | 1 | 0 | 0 | 0 |
| 8 | ROCK | 0 | 1 | 1 | 1 |
| 9 | RHYTHM & BLUES | 0 | 1 | 1 | 0 |
| 10 | AFRO | 0 | 1 | 0 | 1 |
| 11 | SAMBA | 0 | 1 | 0 | 0 |
| 12 | BOSSA NOVA | 0 | 0 | 1 | 1 |
| 13 | RUMBA | 0 | 0 | 1 | 0 |
| 14 | BEGUINE | 0 | 0 | 0 | 1 |
| 15 | CHA-CHA | 0 | 0 | 0 | 0 |

(Codes 1111 and 0000-with-all-high are not used; 1111 = rhythm off. This is a 4-bit select bus, so the
pattern GENERATOR is a decoded block, not a per-switch diode matrix.)
This is a REAL Tier-2 result: the rhythm list in dial order, off a service document, not from a photo.
It is NOT the pattern data (no hit-by-count chart in this table).

## *** THE DECISIVE STRUCTURAL FACT (read off the same scan) ***
IC parts list on the Carousel/Ragtime sheet (far right of page 1, crop `car_ic2.png`), verbatim:

| Symbol | Description |
|---|---|
| ICa1 | SN 74221 |
| ICa2 | S 50240  (= AMI/Mostek S50240 TOP-OCTAVE generator, the organ divider, not rhythm) |
| ICf1 | uA 78M12 |
| ICq1 | HBF 4011 |
| ICq2 | uA 741 MINI DIP |
| ICr1 | **M 251** |
| ICu1 | **M 252 D1 AE** |
| ICu2 | **M 252 D1 AF** |
| ICu3 | HBF 4011 |
| ICu4 | HBF 4011 |
| ICv1 | uA 741 MINI DIP |

=> The Carousel's rhythm patterns are NOT a diode matrix on the board. They are inside **two custom
LSIs, M252, in two different mask variants (suffix "D1 AE" and "D1 AF")**, with an M251 companion.
HBF4011 = SGS-ATES CMOS, so M251/M252 are almost certainly SGS-ATES (Italian) parts too.
CONSEQUENCE: no Elgam service document can contain the pattern data — the patterns are ROM masks.
The Tier-1 document, if it exists at all, is the **SGS-ATES M251/M252 datasheet or application note**,
not an Elgam manual. That reframes the whole question. (Chased next.)

## *** TIER 1 DOCUMENT FOUND — but it documents the CHIP, not Elgam's mask ***
**SGS(-ATES) M252 Rhythm Generator, "PRELIMINARY DATA" datasheet, 11 pages.**
URL: https://www.njohnson.co.uk/pdf/m252.pdf  (free, downloaded OK, 608,596 bytes, PDF 1.5, 11 pages,
embedded scans 1-bit CCITT ~1580x2270 px at **~194 ppi** — legible, transcribable, no OCR layer.)
PDF internal Title = "M252B1", Subject = "SGS  M252".
Contact sheet of all 11 pages: `m252_sheet.png`; page renders `m252p-01..11.png`.

What it prints (read off the contact sheet):
- p1 features list, VERBATIM: "RHYTHM GENERATOR · LOW POWER DISSIPATION <120 mW · DRIVES 8 SOUND
  GENERATORS (INSTRUMENTS) · **15 PROGRAMMABLE RHYTHMS (NOT AVAILABLE IN COMBINATION)** ·
  **MASK PROGRAMMABLE RESET COUNTS: 24 or 32** · DOWN BEAT OUTPUT · EXTERNAL RESET ·
  OPEN DRAIN OUTPUTS · **STANDARD MUSIC CONTENT AVAILABLE** · TECHNICAL NOTE NO 131 AVAILABLE FOR
  FULL INFORMATION". P-channel MOS, 16-lead DIP. Ordering numbers: M252 B1 XX / M252 D1 XX (dual
  in-line, custom) and **M252 B1 AA / M252 D1 AA = "standard content"**.
- p2 "RHYTHM SELECTION" = the same 4-bit INPUT8/4/2/1 code table as the Elgam sheet, plus a
  **STANDARD CONTENT** column with time signatures: Waltz 3/4, Jazz Waltz 3/4, Tango 2/4, March,
  Swing, Foxtrot, Slow Rock, Rock Pop, Shuffle, Mambo, Beguine, Cha Cha, Bajon, Samba, Bossa Nova
  (+ "No selected rhythm" = 1 1 1 1). NOTE these are the CHIP's standard names.
- p3 block diagram (ROM BETA MATRIX -> MULTIPLEXER -> OUTPUT BUFFER; DECODER; SCHMITT; DIVIDER)
- p4-p6 electrical characteristics
- p7 timing waveforms + a table headed **"INSTRUMENT BEATS VERSUS RHYTHM PROGRAM"**
- p8 "COMPLETING THE TRUTH TABLE", verbatim: "The ROM truth table has been organized in **32 rows
  which represent elementary times and 120 columns (15 groups of 8) where each group represents a
  rhythm which has its disposition 8 programmable instruments**. To programme each rhythm one
  indicates (with a cross) in the appropriate boxes the timing for each beat required for each
  instrument. In the given truth table **we show an example of how to programme three imaginary
  rhythms**, the first in 4/4 time, the second in 3/4 time and the third in different time, chosen
  randomly." Also: "Full information on the use of the M 252 in electronic organs and other
  applications will be found in **Technical Note no. 131** available on request."
- **p9 + p10 = the 32-row x 120-column PATTERN GRID ITSELF**, printed as RHYTHM 1..5 / 6..10 / 11..15
  blocks, rows numbered COUNT TO 24 and COUNT TO 32, with X marks.

**HONEST CAVEAT, and it is the whole point:** p8's own wording says the crosses on p9/p10 are
"three imaginary rhythms" used as a PROGRAMMING EXAMPLE, i.e. p9/p10 is a blank CUSTOMER ORDERING FORM,
not the standard content and certainly not Elgam's. Needs a close read (next step) to confirm which
of the 15 blocks carry real data.

### M252 datasheet, pattern-grid pages READ (verdict)
`p10-10.png` (PDF page 10, rendered 200 dpi) and `p11-11.png` (page 11) carry the full grid:
header "RHYTHM 1..5" / "6..10" / "11..15", left column "COUNT TO 32" numbered **1..32**, and per
rhythm **8 columns labelled OUTPUT 1 .. OUTPUT 8**, cells marked with **X** = that instrument fires on
that elementary time. Greyed-out blocks show where a rhythm's reset count is shorter (Rhythm 2 stops
at 24 = 3/4; Rhythm 3 stops at 16).
**Only RHYTHM 1, 2 and 3 carry any X marks. RHYTHM 4 through 15 are printed EMPTY** (verified on
page 11: rhythms 11-15 are a blank ruled grid). Combined with p8's text ("three imaginary rhythms
... chosen randomly"), the conclusion is unavoidable:
=> **This chart is a blank CUSTOMER ORDERING FORM with three throwaway example patterns.**
It is a Tier-1-FORMAT chart, fully legible and transcribable at 194 ppi, but the three patterns in it
are *fictitious examples invented by SGS*, NOT Waltz/Jazz Waltz/Tango and NOT Elgam's masks.
**Do NOT use these three patterns as if they were a real machine's rhythms.**
| URL | What | Verdict |
|---|---|---|
| https://www.njohnson.co.uk/pdf/m252.pdf | SGS M252 rhythm-generator datasheet, 11pp, ~194 ppi bitonal, free | **TIER 1 for ARCHITECTURE; the pattern chart is a BLANK ORDERING FORM + 3 fictitious examples. NOT the real patterns.** |

### Search 5 — bitsavers: 1979 SGS MOS & Special COS/MOS databook (1st Ed) — FREE, HAS AN OCR LAYER
| URL | What | Verdict |
|---|---|---|
| http://www.bitsavers.org/components/sgs/_dataBooks/1979_SGS_MOS_And_Special_COS_MOS_1stEd.pdf | 259 pages, 9,698,179 bytes, **pdftotext works** (real OCR layer). Contains the full SGS organ-LSI family: **M251, M252, M253, M254, M255, M258** all listed as "Rhythm generator". | TIER 1 for the CHIP FAMILY / TIER 2 for Elgam |

Facts read out of its OCR (page 121-123 area, M252 section, issue 11/79, "Supersedes issue dated 3/77"):
- Matrix size printed in the block diagram: **"3840 BITS MATRIX"** = 32 elementary times x 120 columns
  (15 rhythms x 8 outputs). 24-STAGE divider. Confirms the geometry.
- **TWO standard music contents exist**, "M252 B1 AA and AD for standard music content", and the
  databook prints BOTH rosters with time signatures (verbatim, in rhythm-slot order 1..15):

| slot | STANDARD CONTENT **AA** | AA time | STANDARD CONTENT **AD** | AD time |
|---|---|---|---|---|
| 1 | Waltz | 3/4 | Waltz | 3/4 |
| 2 | Jazz Waltz | 3/4 | Tango | 2/4 |
| 3 | Tango | 2/4 | March | 2/4 |
| 4 | March | 2/4 | Swing | 4/4 |
| 5 | Swing | 4/4 | Mambo | 4/4 |
| 6 | Foxtrot | 4/4 | Slow Rock | 6/8 |
| 7 | Slow Rock | 6/8 | Beat | 4/4 |
| 8 | Pop Rock | 4/4 | Samba | 4/4 |
| 9 | Shuffle | 2/4 | Bossa Nova | 4/4 |
| 10 | Mambo | 4/4 | Cha Cha | 4/4 |
| 11 | Beguine | 4/4 | Rhumba | 4/4 |
| 12 | Cha Cha | 4/4 | Beguine | 4/4 |
| 13 | Bajon | 4/4 | Bajon | 4/4 |
| 14 | Samba | 4/4 | Foxtrot | 4/4 |
| 15 | Bossa Nova | 4/4 | Shuffle | 2/4 |

- The standard-content CONNECTION DIAGRAMS name the 8 instruments per output (verbatim tokens read
  from the OCR of the AA/AD pin-out figures): MARACAS, SHORT CYMBALS, LONG CYMBALS, CONGA DRUM,
  LOW BONGO, HIGH BONGO, SNARE DRUM, COW BELL, CLAVES, BASS DRUM.
- Output-sharing notes, verbatim: "This output must be connected so as to drive the *snare drum* when
  the rhythms from 1 to 9 are selected, and the *claves* when the rhythms from 10 to 15 are selected";
  "*long cymbals* when the rhythms number 1,3,4,12 and 14 are generated, and the *claves* when the
  rhythms number 5,8,9,10,11 and 13"; "*snare drum* when the rhythms number 1,3,4,6,7,9,12,14 and 15
  ... and the *conga drum* when the rhythms number 5,8,10,11 and 13". (Tier-2 orchestration data.)
- The databook's M252 truth-table page carries the SAME "COMPLETING THE TRUTH TABLE" blank-form text
  (grep line 16895) => **the databook does NOT print filled standard-content patterns either.**
- Related sections that DO print filled tables, for other chips: `M251` has "BASS and CHORD TRUTH
  TABLES (positive logic)" and "ARPEGGIO TRUTH TABLE (positive logic)" (OCR lines ~13870-14400) —
  that is the ACCOMPANIMENT chip, and Elgam's ICr1 is an M251. **Worth a follow-up read: those
  tables may be real content, which would give the Carousel's BASS/CHORD/ARPEGGIO patterns even
  though the drum patterns stay locked.**

### Why this settles the question — the Elgam roster vs the SGS standard rosters
Elgam Carousel (off its own schematic) vs SGS AA, matched slot-by-slot (both use the same 4-bit code,
rhythm 1 = 1110):
slots 1-3 IDENTICAL (Waltz / Jazz Waltz / Tango). Slot 4 Elgam POLKA vs AA March (both 2/4).
Slots 5-6 SWAPPED (Elgam Fox Trot, Swing; AA Swing, Foxtrot). Slot 7 Slow Rock IDENTICAL.
Slots 8-15 DIVERGE HARD: Elgam has ROCK, RHYTHM & BLUES, AFRO, SAMBA, BOSSA NOVA, RUMBA, BEGUINE,
CHA-CHA where AA has Pop Rock, Shuffle, Mambo, Beguine, Cha Cha, Bajon, Samba, Bossa Nova.
=> Elgam's chips are marked **"M252 D1 AE"** and **"M252 D1 AF"** — NOT AA and NOT AD. Elgam ordered
CUSTOM mask variants (the datasheet explicitly sells this: "ICs available in standard programming or
with rhythm patterns tailored to customer requirements"). SGS published a blank ordering form, never
customers' masks. **Elgam's actual pattern bits were never printed anywhere.**

## *** TIER 1 PATTERN CHARTS THAT DO EXIST — SGS M252 / M253 STANDARD CONTENT ***
The 1979 SGS databook DOES print the real mask contents, contradicting the standalone preliminary
datasheet. OCR line 16903, verbatim: **"Table 1 and 2 show the standard music content programmed into
M252 AA and M252 AD respectively."** (And 19761 for M253 AA / M253 AC.)

**Document:** 1979 SGS MOS and Special COS/MOS Data Book, 1st Ed.
**URL:** http://www.bitsavers.org/components/sgs/_dataBooks/1979_SGS_MOS_And_Special_COS_MOS_1stEd.pdf
(free, 9,698,179 bytes, 259 pages, real OCR text layer)

| PDF page | Figure / caption | Contents |
|---|---|---|
| 123 | **TABLE 1 (M252 AA)** | pattern grid, RHYTHM 1..n, COUNT FOR 32 rows 1-32, OUTPUT 1..8 cols, X = hit |
| 124 | Table 1 continued (M252 AA), rhythms up to 11+ | one block labelled **(SWING)** |
| 125 | **Table 2 (M252 AD)** | the second standard mask |
| 133 | text: "Table 1 and 2 show the standard music content programmed into M253 AA and M253 AC" | |
| 134 | **Table 1 (M253 AA)** | pattern grid |
| 135 | M253 rhythms 11, 12 | pattern grid |
| 136 | M253, blocks captioned **RHYTHM 8 (SAMBA)**, **RHYTHM 7 (B...)** | named rhythms with patterns |

**Embedded scan resolution (pdfimages -list): pages 123/124/125 each = 3902 x 5109 px, 1-bit,
JBIG2, 600 x 600 ppi.** LEGIBLE — verified by rendering page 123 at only 150 dpi (`s123_a.png`): the
X marks, the OUTPUT 1..8 column headers, the COUNT FOR 32 row numbers and the grey "not used past the
reset count" blocks all read cleanly. **Transcribable later with no further sourcing work.**
Reset-count behaviour visible: Rhythm 1 (Waltz, 3/4) greys out rows 25-32, i.e. a 24-count bar.

### HOW THIS RELATES TO ELGAM — read this before using the table
Elgam's Carousel uses **M252 D1 AE** and **M252 D1 AF**: custom masks, NOT AA and NOT AD.
So Table 1/Table 2 are **the same chip, the same 32x8 geometry, the same instrument roster, and the
same slot-to-code mapping as Elgam's machine, but a different pattern set.** In Elgam's roster,
slots 1 WALTZ, 2 JAZZ WALTZ, 3 TANGO and 7 SLOW ROCK sit in exactly the AA slots with exactly the AA
names, so those four are a *plausible* match; slots 8-15 provably are not (Elgam has ROCK / RHYTHM &
BLUES / AFRO / RUMBA, which appear in neither AA nor AD). Using Table 1 as "the Elgam patterns" would
be a fabrication. Using it as "the SGS M252 standard content, the chip Elgam built on" is honest.

### Search 6 — Farfisa as a substitute
| URL | What | Verdict |
|---|---|---|
| https://www.servicemanual.altervista.org/FARFISA_Service-Manual-Schematics.html | Same commercial reseller, large Farfisa catalogue incl. "Farfisa Matador M / Matador MR schematics (9 pages)" which per the search summary includes a **Partner 6S Rhythms Unit (SE-241)** | SERVICE DOCS EXIST (paid) |
| https://archive.org/advancedsearch.php?q=farfisa | **numFound 483**, but the overwhelming majority are audio recordings ("The Organist Entertains" radio shows etc). The only schematic items: `farfisa_soundmaker-SM` (Soundmaker, a 1980 mono synth), `sm_Farfisa_Compact-Duo_Schematics`, `farfisa-professional-duo-schematics` (both COMBO ORGANS with no rhythm section) | CONTEXT ONLY / SERVICE DOCS ONLY — **no Farfisa rhythm PATTERN chart found** |
| https://farfisa.org/category/schematics | enthusiast site with a schematics category | NOT OPENED (see "where I did not look") |
| https://www.synthxl.com/service-manual/farfisa/ | service-manual index | NOT OPENED |

**Farfisa verdict:** far better documented than Elgam in raw quantity, and free Farfisa schematics do
exist on archive.org — but the free ones are combo organs and a synth, none with a rhythm section, and
I found NO Farfisa pattern chart. More importantly, Farfisa is the same industry in the same town in
the same years and bought from the same silicon vendor, so its 1970s rhythm sections are also very
likely SGS M25x mask ROMs — in which case Farfisa's patterns are locked in exactly the same way and
substituting Farfisa buys nothing. **The real substitute is not Farfisa, it is the SGS M252/M253
standard content tables above**, which are the actual silicon Elgam shipped.

## ANSWER (replaces the placeholder at the top)
**No. Elgam's own preset rhythm patterns are not documented publicly, and structurally they cannot be.**
Elgam service documentation does exist (28 models listed at a paid reseller; the Carousel/Ragtime
schematic is free from the Swiss museum SMEM), and that schematic yields the full 15-rhythm name list
in dial order with its 4-bit select codes. But the patterns themselves live in two custom-masked SGS
M252 LSIs (marked `M252 D1 AE` and `M252 D1 AF`), and a mask ROM's contents were never published for
any customer — SGS printed only a blank ordering form for custom masks. **Confidence: high** for
"Elgam's own patterns are unpublished" (this rests on reading the chip designators off Elgam's own
schematic plus SGS's own custom-mask ordering scheme, not on failure to find something).
**The consolation prize is large and it is Tier 1:** the same databook publishes the *standard*
content of that exact chip family as real, 600-dpi, fully legible 32-count x 8-instrument pattern
grids (M252 AA, M252 AD, M253 AA, M253 AC). If this project wants a period-correct
Italian-organ auto-rhythm, that is the honest source. Elgam's actual masks would need either a ROM
dump off a surviving chip or transcription from recordings.

## Elgam model range, as far as I could source it
Dated (SMEM museum, primary-ish):
- **Elgam company active 1968-1982** (SMEM), Castelfidardo, Italy.
- **Elgam Carousel, 1976** — 25 keys, VCO, **15 rhythms**, 3 arpeggiators (adjustable decay+volume),
  3 polyphonic chord patterns, independent drum/bass/chord/arp volumes. Rhythm names sourced above.
- Elgam Montreal (electronic piano), **early 1970s**. Elgam 3049 (combo organ), **early 1970s**.
  Elgam 244 (combo organ), **mid 1970s**.
- **Elgam Ragtime** = a rhythm unit; shares one schematic sheet with the Carousel.
Undated, model names only (from the altervista service-manual catalogue): 237, Alpha 110, Alpha 120,
Alpha 130, Delta 60, Broadway 300, Broadway 444, Concert, Derby, EP20, ES2000, Mistral, Mistral 200,
Mistral 200s, Mistral 210s, Palladium 220, Palladium 230, Piano, Recital, Ringo, Royal, Ruby 610,
Symphony 200, Talisman, Talisman-Royal, The Entertainer 8, The Entertainer 8-13.
**Rhythm counts: only the Carousel's 15 is sourced.** I found NO source for Match 7c / Match 15C /
Match 12 / Match 24 / Symphony Two / Snoopy / Broadway (bare) / "Elgam Mini" — those names from the
brief did not appear in any catalogue or museum page I read. Do not assume they exist as named.

## Where I did NOT look (do not repeat me; these are the open leads)
1. **SGS "Technical Note no. 131"** — the datasheet cites it twice for "full information on the use of
   the M252 in electronic organs". Never searched for. Highest-value untouched lead.
2. **Elektor**: `https://www.elektormagazine.com/magazine/elektor-197507/57440` ("rhythm generator
   M 252") and `.../elektor-197604/57604` ("ic rhythm generator"). A DIY project around this chip may
   print the pattern tables in a friendlier form. Paywall risk.
3. **M255 / M258 / M259 / M254 sections of the same databook** — M254 has "B1 AD / B1 AM for standard
   music content" (OCR 21544-22061) so it likely has printed tables too; M255/M258/M259 unchecked.
   datasheet4u has standalone M255 and M258 pages.
4. The **paid Elgam manuals** at servicemanual.altervista.org — 28 models, none bought. The Carousel
   and Ragtime ones are redundant now (SMEM has that sheet free), but a full Elgam *service manual*
   (as opposed to a schematic sheet) might carry a rhythm-section description.
5. **elektrotanya's ELGAM RECITAL ORGAN SCH** (19 pp) — exists, download is gated, never read.
6. matrixsynth.com Elgam tag; radiomuseum.org; Italian-language searches ("schema elettrico Elgam",
   "manuale di servizio Elgam"); Italian/Dutch/German organ forums; Castelfidardo accordion museum;
   reverb/eBay panel photos. **None of these were opened** — the chip-designator finding made them
   unnecessary for the yes/no, but a panel photo is still the cheapest way to get another model's
   rhythm list (Tier 2).
7. farfisa.org schematics category, synthxl Farfisa index, Farfisa Matador/Partner 6S rhythm unit.
8. Whether any of the SMEM Elgam pages other than Carousel host a free PDF (only the Carousel page
   was fetched in full).

### Closing cross-check (databook page 127, M253 standard content pin-out)
M253's two standard rosters (AA and AC) read: TANGO, WALTZ, SLOW ROCK, SWING, SAMBA 4/4, RUMBA,
BEGUINE, RHUMBA, CHA(-CHA), BOSSA NOVA 4/4, BEAT. **Still no POLKA, no AFRO, no RHYTHM & BLUES, no
"ROCK".** So Elgam's roster matches NO published SGS standard content (AA/AD for M252, AA/AC for
M253) — it is definitively a bespoke mask. (M253 is also the larger chip, IN1..IN12; Elgam used the
16-pin M252, so M252 Table 1 / Table 2 are the correct architectural proxy, not M253.)


# ===== source file: LABEL-CAVEAT.md =====

# Data-integrity caveat: the DOTS are sourced, the LANE LABELS are not

Applies to every FR-2L transcription in this directory (fr2l_left.md, fr2l_right.md,
fr2l_beguine_rhumba.md, fr2l_samba_mambo.md), including the ones read in the main session.

## The measurement
The FR-2L scan embedded in the service-manual PDF is about 75 dpi. At 600 dpi render that is
one scan pixel per 8 rendered pixels, so a two-letter lane label occupies roughly 6 native
pixels, about 3x4 pixels per letter. The crisp "H", "L", "B" letterforms visible in an upscaled
crop are INTERPOLATION, not ink. An agent tried an ascender-height discriminator to separate
b from c and found the difference is a single native pixel on mismatched phase: not decidable.

## What that means, precisely
- CIRCLE POSITIONS are solid. A circle is about 4x4 native pixels sitting on a ruled column,
  the rulings are measurable, and several independent checks passed (two named claves fell out
  of two different machines; unruled counts stayed empty; bar-2 twins corroborated bar-1 reads).
- LANE LABELS are PROVISIONAL. Where this directory writes "Hc", "Hb", "Cb+Hb" or "Lc+Bd", read
  it as "the lane at this y position, whose label is probably that". Lane ORDER and lane COUNT
  are reliable; the letters are not.
- Consequence for use: a cart can play these patterns confidently, but should not claim which
  voice plays which lane. Assign voices by lane order and taste, or find a better scan.

## What would settle it
A higher-resolution scan of page 14, or the FR-2L OWNER's manual (which may print the same
chart at better quality), or the machine's front panel photographed, since the panel names the
voices in panel order and the chart's lane order plausibly follows it.
The instrument roster itself IS sourced, from pages 2 and 4 of the same manual: Bass Drum,
Low Conga, High Conga, High Bongo, Cow Bell, Claves, Snare Drum, Cymbal, Maracas, Wire Brush,
with per-voice trim pots VR4 through VR11 named in that order. So the vocabulary is known even
where the per-lane assignment is not.

## Credit where due
This limit was found by the agent transcribing Samba and Mambo, which reported its labels as
UNREAD rather than passing on the plausible-looking letterforms it could see. The main session
had already written several labels as if they were read; they are downgraded by this note.



# ===== source file: sgs_m253.md =====

# SGS M253 rhythm-generator LSI — FACTORY STANDARD MASK pattern tables

## Provenance

- **Source document:** *1979 SGS MOS And Special COS/MOS Data Book, 1st Edition* (SGS / SGS-ATES).
- **URL:** http://www.bitsavers.org/components/sgs/_dataBooks/1979_SGS_MOS_And_Special_COS_MOS_1stEd.pdf
- **Pages holding the M253 pattern tables** (printed page = PDF page + 7 in this section — note this
  differs from the M252 section's +6 offset; measured off the printed folios, see below):
  - PDF **134** (printed **141**) = `TABLE 1 (M253 AA)`, rhythms 1-10
  - PDF **135** (printed **142**) = Table 1 rhythms 11-15 (top) + `TABLE 2 (M253 AC)` rhythms 1-5 (bottom)
  - PDF **136** (printed **143**) = Table 2 rhythms 6-12
- **Embedded scan resolution (measured, `pdfimages -list -f 134 -l 136`):**
  - PDF 134: **3912 x 5117, gray, 1 bpc, jbig2, 600 x 600 ppi**
  - PDF 135: 3893 x 5102, gray, 1 bpc, jbig2, 600 x 600 ppi
  - PDF 136: 3902 x 5109, gray, 1 bpc, jbig2, 600 x 600 ppi
  So `pdftoppm -r 600` renders at native scan resolution with no upsampling. Bitonal source.
- **Table captions as printed:** `TABLE 1 (M253 AA)` and `TABLE 2 (M253 AC)`.
- **Chart format** (identical to the M252 section): left header column `COUNT FOR 32`, rows = counts
  1..32, columns = OUTPUT 1..8, `X` = a hit. Cells past each rhythm's reset count are greyed/shaded
  to mark counts that do not exist for that rhythm.

## NAMING — read this before using the data

These are the **SGS FACTORY STANDARD MASK** pattern sets for the **M253**, as published by SGS in
their own databook. Two mask options are printed: **M253 AA** (Table 1) and **M253 AC** (Table 2).

**This is NOT the Elgam pattern set.** Elgam's organs used **custom masks** of the M252 (marked
`M252 D1 AE` and `M252 D1 AF` on Elgam's own Carousel schematic). SGS never published a customer
mask. Nothing here may be presented as Elgam content.

**OUTPUT 1..8 are chip pins, not instruments.** No instrument names are attached to these lanes
unless the databook itself assigns them; whether an M253 equivalent of the M252's page-122
"standard content configuration" pinout exists is answered in the mapping section at the end,
quoted with its page.

## Notation

Per rhythm: one line per output, one character per count from 1 to that rhythm's reset count.
`x` = an X printed in the table, `.` = a blank cell. Counts past the reset count are not written
at all (they are the greyed cells).

---

## TABLE 1 (M253 AA) — factory standard mask, PDF p.134 (printed p.141)

### Measured geometry (how every cell below was located)
- Page 134 renders 3912 x 5117 at 600 dpi. Table 1 band 1 (rhythms 1-5) header rule at y=1130;
  **row pitch 43.05 px**, row 1 top edge y=1142, so row N spans y = 1142+43.05(N-1) .. 1142+43.05N.
- Block x-boundaries (heavy rules, detected by column ink fraction): count column 430-712, then
  rhythm 1 = 712-1266, rhythm 2 = 1266-1821, rhythm 3 = 1825-2383, rhythm 4 = 2380-2938,
  rhythm 5 = 2938-3497. Each block is EIGHT equal columns (~69.5 px) — verified against the
  independently detected interior rules, which land within 3 px of the eighth-divisions.
- Marks were read BY EYE from 600 dpi crops that include the count column and the OUT 1-8 header
  digits, and independently cross-checked by per-cell ink fraction over those rectangles.
  No blob or circle detection was used anywhere.
- **Page noise warning specific to this page:** broken/dashed rule lines cross the counts 17-32
  region of band 1, giving blank cells a raised 0.10-0.16 ink fraction there. Real X glyphs on this
  page read **0.17-0.29**. Every borderline cell in that zone was resolved by eye, not by threshold.

### RHYTHM NAMES
**Table 1 (M253 AA) prints NO rhythm names.** Blocks are captioned only `RHYTHM 1` … `RHYTHM 15`.
The left header column reads `COUNT FOR 32`. Same as the M252 AA mask.

### TABLE 1 (M253 AA) — RHYTHM 1
Printed name: **none printed** (captioned only "RHYTHM 1").
Reset count: **32** (no greyed cells; the dotted band at the right of the crop is rhythm 2's grey
block beginning past the x=1266 border, not rhythm 1's).

```
count:   12345678901234567890123456789012
OUT 1    ..............x...............x.
OUT 2    x...x...x...x.x.x...x...x...x.x.
OUT 3    x...x...x...x.x.x...x...x...x.x.
OUT 4    ................................
OUT 5    ................................
OUT 6    ..............x...............x.
OUT 7    x.....x.x...x...x.....x.x...x...
OUT 8    ................................
```
Status: **CONFIDENT.** Eye read and ink cross-check agree on every cell. OUT 4, 5, 8 wholly empty.
OUT 2 and OUT 3 carry IDENTICAL lanes; OUT 1 and OUT 6 carry identical lanes (15, 31 only).
The elevated 0.10-0.16 readings at counts 18-20, 22, 24, 26-28, 30, 32 across several lanes are the
dashed rule lines described above and are recorded as blanks (the crop shows no glyphs there).

### TABLE 1 (M253 AA) — RHYTHM 2
Printed name: **none printed** (captioned only "RHYTHM 2").
Reset count: **24** — counts **25-32 are greyed** (the shaded band reads a uniform 0.17-0.30 ink
across **all eight** lanes at once, which no pattern of X marks produces; the crop shows a solid
stipple block whose top edge sits on the rule below row 24).

```
count:   123456789012345678901234
OUT 1    ............x...........
OUT 2    x...........x...........
OUT 3    ....x...x.......x...x...
OUT 4    ........................
OUT 5    ........................
OUT 6    x...........x...........
OUT 7    ........................
OUT 8    ........................
```
Status: **CONFIDENT.** Eye read and ink cross-check agree on every cell. OUT 4, 5, 7, 8 wholly
empty. The sparsest rhythm on the page (9 marks). OUT 2 and OUT 6 carry identical lanes (1, 13).
Dashed rule fragments cross counts 19-24 in the OUT 4-7 region (ink 0.10-0.13); the crop shows
they are broken rules, not glyphs, and they are recorded as blanks.

### TABLE 1 (M253 AA) — RHYTHM 3
Printed name: **none printed** (captioned only "RHYTHM 3").
Reset count: **24** (counts 25-32 greyed; the stipple block's top edge sits on the detected rule at
y=2178.5, which is the bottom of row 24).

```
count:   123456789012345678901234
OUT 1    ...x.....x.....x.....x..
OUT 2    x..x..x..x..x..x..x..x..
OUT 3    x.xx.xx.xx.xx.xx.xx.xx.x
OUT 4    ........................
OUT 5    ........................
OUT 6    ........................
OUT 7    x..x..x..x..x..x..x..x..
OUT 8    ........................
```
Status: **CONFIDENT.** OUT 4, 5, 6, 8 wholly empty. A **TRIPLE-DIVISION** grid: the spine
(OUT 2 and OUT 7, identical lanes) is every third count 1..22, OUT 1 is every sixth count from 4,
and OUT 3 marks the 1st and 3rd position of each triplet group. Stated as an observation about count
spacing only, not as a claim about any dance name.

> **A METHOD NOTE, recorded because it nearly caused an error.** The ink cross-check UNDER-read
> count 22: it scored OUT 1 at 0.22 (a mark) but OUT 2 / OUT 3 / OUT 7 at only 0.12-0.14, which
> looked like "only OUT 1 fires at 22" and would have broken the triplet spine. A 2x zoom of counts
> 19-25 shows **four unmistakable X glyphs at count 22** (OUT 1, 2, 3, 7), printed low in their
> cells so that the 5 px cell margins clipped them. The eye read is authoritative and is what is
> recorded. Consequence for the rest of this file: the ink figure is used as a CONFIRMING second
> opinion only, and every count is confirmed on a crop; a low ink score is never taken as a blank.

### TABLE 1 (M253 AA) — RHYTHM 4
Printed name: **none printed** (captioned only "RHYTHM 4").
Reset count: **32** (no greyed cells; the stipple strip at the far right of the crop is rhythm 5's
grey block past the x=2938 border).

```
count:   12345678901234567890123456789012
OUT 1    ........x...............x.......
OUT 2    x.......x.......x.......x.......
OUT 3    ....x.......x.......x.......xxxx
OUT 4    ................................
OUT 5    ................................
OUT 6    x.......x.......x.......x.......
OUT 7    ................................
OUT 8    ................................
```
Status: **CONFIDENT.** OUT 4, 5, 7, 8 wholly empty. OUT 2 and OUT 6 carry IDENTICAL lanes
(1, 9, 17, 25). OUT 3 sits on the same 8-count grid offset by four, and ends with a **FOUR-count
consecutive run at 29-32** (all four glyphs read 0.18-0.23 and are unmistakable on the crop).
This block sits on a heavy field of broken rule lines (blanks read 0.08-0.15 throughout); every
count was therefore confirmed on the crop rather than by threshold.

### TABLE 1 (M253 AA) — RHYTHM 5
Printed name: **none printed** (captioned only "RHYTHM 5").
Reset count: **24** (counts 25-32 greyed; the stipple reads a uniform 0.16-0.32 across all eight
lanes at once).

```
count:   123456789012345678901234
OUT 1    ............x.........x.
OUT 2    x.........x.x.........x.
OUT 3    ......x...........x.....
OUT 4    ........................
OUT 5    ........................
OUT 6    ........................
OUT 7    x.x.x.x.x.x.x.xxx.x.x.x.
OUT 8    ........................
```
Status: **CONFIDENT.** OUT 4, 5, 6, 8 wholly empty (OUT 8 shows a flat 0.10-0.19 down its whole
length, which is a dashed vertical rule printed inside the column, not a series of faint marks — it
never varies row to row the way glyphs do).

> **The anomaly, recorded as seen:** OUT 7 is every ODD count 1-23 **plus one extra mark at count
> 16**, the only even-count mark in that lane, so counts 15-16-17 are three consecutive glyphs. The
> crop shows all three unambiguously (ink 0.28 / 0.25 / 0.27 against a 0.11 local floor).
> **This is the same fingerprint as M252 AA rhythm 7**, which also resets at 24 and also has a lone
> even-count mark at 16 in its OUT 7. Followed up in the comparison section.

### Band 2 of PDF p.134 — measured geometry
Header rule at y=3213.5; 32 row-bottom rules detected in the count column from 3272 to 4614.5
(pitch 43.1, non-uniform by up to 3 px from scan warp, so the DETECTED boundaries are used, not a
constant pitch). Block x-boundaries: count column 414-702, rhythm 6 = 702-1255, rhythm 7 = 1255-1811,
rhythm 8 = 1811-2370, rhythm 9 = 2370-2927, rhythm 10 = 2927-3484; eight equal columns per block.
This band prints much more cleanly than band 1: blank cells read 0.00-0.05 and marks 0.16-0.19.

### TABLE 1 (M253 AA) — RHYTHM 6
Printed name: **none printed** (captioned only "RHYTHM 6").
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    ........x...............x.......
OUT 2    x.......x.......x.......x.......
OUT 3    ....x.......x.......x.......x...
OUT 4    ................................
OUT 5    ................................
OUT 6    x.......x.......x...............
OUT 7    ....x..x....x..x.......xx..xx..x
OUT 8    ................................
```
Status: **CONFIDENT.** OUT 4, 5, 8 wholly empty. Note **OUT 6 has NO mark at count 25** although
OUT 2 does — checked twice on the crop, row 25's OUT 6 cell is empty.

> **A CONTENT MATCH WITH THE M252, found here and pursued in the comparison section.** This block is
> M252 AA rhythm 5 with the lanes shifted down one: M252's OUT 1 (1,9,17,25) is here OUT 2, M252's
> OUT 2 (5,13,21,29) is here OUT 3, while **OUT 6 (1,9,17) and OUT 7 (5,8,13,16,24,25,28,29,32) are
> character-for-character the same lanes in both parts**, including OUT 6's missing 25. M253 then
> ADDS an OUT 1 lane at 9, 25 that M252 AA rhythm 5 does not have.

### TABLE 1 (M253 AA) — RHYTHM 7
Printed name: **none printed** (captioned only "RHYTHM 7").
Reset count: **32** (no greyed cells).

```
count:   12345678901234567890123456789012
OUT 1    ........x.x...x.........x.x.....
OUT 2    x.x.....x.x...x.x.x.....x.x...x.
OUT 3    ....x..x.x..x.......x..x.x..x..x
OUT 4    ................................
OUT 5    ................................
OUT 6    ................................
OUT 7    ....x.......x.......x.......x...
OUT 8    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
```
Status: **CONFIDENT.** OUT 4, 6 wholly empty. OUT 5 shows a flat 0.02-0.05 down its length (a
dashed vertical rule inside the column, not marks). OUT 8 is every ODD count 1-31 with no
exceptions; OUT 7 is a plain 4-mark lane (5, 13, 21, 29).

> **A second M252 content match, and a cleaner one.** This block is **M252 AA rhythm 8** with the
> lanes shifted down one: M252 AA r8's OUT 1 lane `x.x.....x.x...x.x.x.....x.x...x.` is this block's
> **OUT 2, character for character**; M252's OUT 2 lane `....x..x.x..x.......x..x.x..x..x` is this
> block's **OUT 3, character for character**; and **OUT 7 and OUT 8 are identical in both parts**.
> M253 again ADDS an OUT 1 lane (9, 11, 15, 25, 27) that the M252 block does not have.

> **RHYTHM 7 BODY VERIFIED, NOT REPLACED** (second reading, this session). The block was re-read by
> eye from the 600 dpi crop `m253_r7.png` (block x=1255-1811, band-2 row boundaries) without
> reference to the text above, and the independent read reproduced the inherited transcription
> character for character on all eight lanes, including OUT 1's five marks (9, 11, 15, 25, 27),
> OUT 3's nine (5, 8, 10, 13, 21, 24, 26, 29, 32) and OUT 8's odd-count run. The inherited body is
> therefore left exactly as the previous agent wrote it. OUT 4, 5, 6 empty confirmed.

### TABLE 1 (M253 AA) — RHYTHM 8
Printed name: **none printed** (captioned only "RHYTHM 8").
Reset count: **32** (no greyed cells; all 32 rows are live in the crop).
Geometry: band 2 of PDF p.134, block x=1811-2370, eight equal ~69.9 px columns, row boundaries from
`m253_rows_p134b.txt` as recorded above.

```
count:   12345678901234567890123456789012
OUT 1    ........x...x...........x...x...
OUT 2    x.....x.x...x...x.....x.x...x...
OUT 3    x.....x.....x.......x...x.......
OUT 4    ....x.....x...x.....x.....x...x.
OUT 5    x.x...x.x...x...x.x...x.x...x...
OUT 6    ................................
OUT 7    ................................
OUT 8    x.xxx.x.x.x.x.x.x.xxx.x.x.x.x.x.
```
Status: **CONFIDENT.** Eye read and ink cross-check agree on every one of the 256 cells; every
mark scores 0.15-0.24 and every blank 0.00-0.09, with no borderline cell anywhere in the block.
The densest rhythm on the page so far (51 marks).

- **OUT 6 and OUT 7 wholly empty** — both read a flat 0.02-0.07 down their whole length, which is
  the dashed vertical rule printed inside those columns, not faint glyphs (it never varies row to
  row the way the real X marks do).
- **No two lanes are identical in this block** — the first rhythm on the page for which that is
  true. The near-miss is OUT 5 = OUT 2 **plus** two extra marks at counts 3 and 19.
- OUT 8 is every ODD count 1-31 **plus two even-count marks, at 4 and 20** — the same "odd spine
  with an interpolated extra" shape as rhythm 5's OUT 7, but on a different lane and with two
  extras 16 counts apart rather than one. Both were re-checked on the crop (ink 0.21 each against
  a 0.02 local floor in that lane's blank rows) and are unmistakable.
- The one cell worth naming as noise: **count 1, OUT 4** carries a single stray dot in the crop
  (ink 0.06). It is a print speck, not a glyph — there is no X stroke — and is recorded as blank.

### TABLE 1 (M253 AA) — RHYTHM 9
Printed name: **none printed** (captioned only "RHYTHM 9").
Reset count: **32** (no greyed cells).
Geometry: band 2 of PDF p.134, block x=2370-2927, eight equal ~69.6 px columns, band-2 row
boundaries. Crop `m253b_r9.png` (count column x=525-702 hstacked onto the block, which is how every
crop in this file was framed).

```
count:   12345678901234567890123456789012
OUT 1    ........x...x...........x...x...
OUT 2    x.......x...x...x.......x...x...
OUT 3    ..x...x...x...x...x...x...x...x.
OUT 4    ..x...x...x...x...x...x...x...x.
OUT 5    ..x...............x.............
OUT 6    ......x...............x.........
OUT 7    ..xxx.............xxx...........
OUT 8    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
```
Status: **CONFIDENT.** Eye read and ink cross-check agree on every cell; marks 0.16-0.24, blanks
0.00-0.09. **No lane is wholly empty — this is the first rhythm in Table 1 that uses all eight
outputs.**

- **OUT 3 and OUT 4 carry IDENTICAL lanes** (every 4th count from 3: 3, 7, 11, 15, 19, 23, 27, 31).
- **The whole block is exactly 16-PERIODIC**: counts 17-32 reproduce counts 1-16 on all eight
  lanes, with no exception anywhere. It is the only block on the page with that property, and it
  makes the reading self-checking — the two halves were read independently and matched.
- OUT 7 is two THREE-count consecutive runs (3-4-5 and 19-20-21), and counts 4 and 20 are the only
  counts in the whole block where OUT 7 fires with OUT 8 silent.
- OUT 8 is every ODD count 1-31, no exceptions.
- The OUT 2 column carries a steady 0.02-0.05 ink floor down its whole length (a dashed vertical
  rule printed inside the column); its real marks read 0.19-0.24, so the two are not confusable,
  but the floor is why counts 2-8 of OUT 2 were re-checked on the crop and are recorded as blank.

### TABLE 1 (M253 AA) — RHYTHM 10
Printed name: **none printed** (captioned only "RHYTHM 10").
Reset count: **32** (no greyed cells). Last block of PDF p.134.
Geometry: band 2, block x=2927-3484, eight equal ~69.6 px columns. Crop `m253b_r10.png`.

```
count:   12345678901234567890123456789012
OUT 1    ......x.....x.........x.....x...
OUT 2    x.....x.....x...x.....x.....x...
OUT 3    x...x...x.x.x...x...x...x.x.x...
OUT 4    x...x...x...x...x...x...x...x...
OUT 5    ............x.x.............x.x.
OUT 6    ................................
OUT 7    ................................
OUT 8    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
```
Status: **CONFIDENT.** The cleanest block on the page: marks 0.17-0.23, blanks 0.00-0.06, not one
borderline cell. Every mark falls on an ODD count — there is no even-count mark anywhere in the
block, which made the reading unusually easy to check.

- **OUT 6 and OUT 7 wholly empty** (a true 0.00-0.02 here, with no dashed-rule floor at all).
- **Exactly 16-PERIODIC**, like rhythm 9: counts 17-32 reproduce counts 1-16 on all eight lanes with
  no exception. The two halves were read independently and matched.
- **OUT 4 is a plain every-4th-count lane** (1, 5, 9, …, 29) and **OUT 3 is that same lane plus two
  extra marks, at 11 and 27**. OUT 4 reads 0.00 at both 11 and 27, so the pair is a real difference
  between the two lanes and not a faint print.
- OUT 5 fires only as two-mark pairs: 13+15 and 29+31.
- OUT 8 is every ODD count 1-31, no exceptions.

### CORRECTION TO THE PROVENANCE SECTION — **TABLE 1 HOLDS 12 RHYTHMS, NOT 15**
The provenance block at the top of this file says PDF p.135 carries "Table 1 rhythms 11-15". That is
wrong and is corrected here rather than by editing the inherited text. **Table 1 (M253 AA) ends at
RHYTHM 12.** The top band of PDF p.135 (printed p.142) prints exactly two blocks, captioned
`RHYTHM 11` and `RHYTHM 12`, and the remaining right-hand two thirds of that band is **blank ruled
white space** — no third block, no marks, no caption. Verified on a whole-page render
(`m253b_p135_small.png`) and on the vertical-rule scan below, which finds the table's outer right
edge at x=1819, immediately past rhythm 12: there is nowhere for a rhythm 13 to be.

So the M253 AA mask is a **12-rhythm** set: 5 blocks on p.134 band 1, 5 on p.134 band 2, 2 on p.135
band 1. Table 2 then begins on the same page (p.135) in the lower half, one band earlier than the
inherited note assumed.

### Band 1 of PDF p.135 (rhythms 11-12) — measured geometry
Derived from scratch on this page, not carried over from p.134 (a different scan, 3893 x 5102).
- **Vertical rules**, detected by column ink fraction > 0.5 over y=700..2530: table left edge
  x=392-398; count-column right edge x=700-705; then SEVEN interior rules at
  772 / 840 / 913 / 981 / 1053 / 1123 / 1194; heavy rule x=1259-1265; seven more interior rules at
  1329 / 1399 / 1472 / 1543 / 1611 / 1679 / 1750; table right edge x=1816-1822.
  So **count column 395-702, rhythm 11 = 702-1262, rhythm 12 = 1262-1819**, each an EIGHT-column
  block of ~70 px. The eighth-divisions of those spans land within 3 px of every one of the
  fourteen independently detected interior rules.
- **Row rules**, detected by row ink fraction > 0.5 over x=400..700: the heavy header rule at
  y=1100-1106, then exactly **32** row-bottom rules from y=1158 to y=2508. Pitch averages 43.4 px
  but drifts by up to 3 px from scan warp, so the DETECTED boundaries are used, recorded in
  `m253b_rows_p135a.txt`, with row 1's top taken at y=1112 (just under the thick header rule).
- This band prints very cleanly: blanks read 0.00-0.04, marks 0.15-0.20.

### TABLE 1 (M253 AA) — RHYTHM 11
Printed name: **none printed** (captioned only "RHYTHM 11").
Reset count: **32** (no greyed cells; all 32 rows are ruled and live).

```
count:   12345678901234567890123456789012
OUT 1    ........x...............x.......
OUT 2    x.......x.......x.......x.......
OUT 3    ..x...x...xx..x...x...x...xx..x.
OUT 4    x...x...........x.x...x.........
OUT 5    ........x.....x.........x...x...
OUT 6    ................................
OUT 7    ....x.......x.......x.......x...
OUT 8    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
```
Status: **CONFIDENT.** Eye read and ink cross-check agree on all 256 cells; every mark reads
0.15-0.20 and every blank 0.00-0.04, with no borderline cell in the block.

- **OUT 6 is the only wholly empty lane** (a true 0.00 down its whole length; this band has no
  dashed-rule floor at all).
- OUT 3 is the only lane with adjacent marks: **11+12 and 27+28**. Both pairs were checked at 2x —
  counts 12 and 28 are the only two counts in the entire block where OUT 8 is silent while some
  other lane fires (OUT 8 reads 0.00 at both).
- **NOT 16-periodic**, unlike rhythms 9 and 10: OUT 4 fires at 1, 5 in the first half but at
  17, 19, 23 in the second, and OUT 5 at 9, 15 against 25, 29. The halves genuinely differ.
- OUT 2 is a plain every-8th lane (1, 9, 17, 25) and OUT 7 the same lane offset by four
  (5, 13, 21, 29) — the pairing seen in rhythms 4 and 6, but here on OUT 2/OUT 7 rather than
  OUT 2/OUT 6, and the two are NOT identical to each other.
- OUT 8 is every ODD count 1-31, no exceptions.

### TABLE 1 (M253 AA) — RHYTHM 12  *(last block of Table 1)*
Printed name: **none printed** (captioned only "RHYTHM 12").
Reset count: **32** (no greyed cells).
Geometry: band 1 of PDF p.135, block x=1262-1819. Crop `m253b_r12.png`.

```
count:   12345678901234567890123456789012
OUT 1    ........x.....x.........x.....x.
OUT 2    x.....x.x.....x.x.....x.x.....x.
OUT 3    x.....x.....x.......x.....x.....
OUT 4    ................................
OUT 5    ................................
OUT 6    ................................
OUT 7    ....x.x.....x.x.....x.x.....x.x.
OUT 8    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
```
Status: **CONFIDENT.** Eye read and ink cross-check agree on every cell; marks 0.14-0.21, blanks
0.00-0.04. Only four of the eight lanes are used, the fewest of any 32-count block in Table 1.

- **OUT 4, OUT 5 and OUT 6 wholly empty.** OUT 5 carries a faint 0.02-0.04 floor over counts 1-14
  only, which is the tail of a dashed rule fading out down the column, not a run of faint marks; the
  crop shows no glyph strokes anywhere in that lane.
- Three lanes are strictly PERIODIC: OUT 2 is `x.....x.` repeated (period 8), OUT 7 is `....x.x.`
  repeated (period 8), OUT 8 is every odd count (period 2). OUT 1 is period 16 (9, 15 then 25, 31).
- **OUT 3 is the one lane that breaks pattern**: 1, 7, 13, 21, 27 — the every-6th-count spacing
  1/7/13 continues to 19 in no lane, and instead jumps to 21 and then 27. Counts 19 and 25 read
  0.00 in OUT 3 and were re-checked at 2x; they are genuinely blank.
- OUT 8 is every ODD count 1-31, no exceptions.

---

## TABLE 1 (M253 AA) — COMPLETE. Cross-rhythm summary and the OUT-8 reset test

**All 12 rhythms of Table 1 are now transcribed, all 12 CONFIDENT, none UNREAD.**

Reset counts as printed: rhythms **2, 3, 5 reset at 24**; rhythms **1, 4, 6, 7, 8, 9, 10, 11, 12
reset at 32**. No other reset value appears in Table 1 — there is no 16-count or 12-count rhythm in
this mask.

### The OUT-8 reset rule — **IT HOLDS ON THE M253, in one direction only**

The M252 model says a rhythm shorter than 32 states is programmed by crossing the column that now
represents the RESET output rather than the 8th instrument, so a short rhythm must have an EMPTY
OUT 8. Tested against all 12 M253 AA rhythms:

| reset | rhythms | OUT 8 |
|---|---|---|
| 24 (short) | 2, 3, 5 | **empty in all three** |
| 32 (full)  | 1, 4, 6 | empty |
| 32 (full)  | 7, 8, 9, 10, 11, 12 | in use, and heavily (16-18 marks each) |

**The rule's prediction is satisfied without exception: every short rhythm has an empty OUT 8, and
every rhythm that USES OUT 8 is a full 32-count rhythm.** That is a real, non-trivial confirmation
— 3 for 3 on the constrained side, 6 for 6 on the unconstrained side — and it is the same behaviour
established for the M252.

The **converse does not hold**, and must not be assumed: rhythms 1, 4 and 6 run the full 32 counts
and still leave OUT 8 empty. So an empty OUT 8 is evidence of NOTHING by itself; only the implication
"short ⟹ empty" is supported. Anyone inferring a reset count from a blank OUT 8 would misread
rhythms 1, 4 and 6 as short.

A second observation about how the reset is PRINTED, recorded because it bears on the model: for the
three short rhythms the chart conveys the reset by **shading counts 25-32 across all eight columns**,
and prints **no X anywhere in the OUT 8 column**, not even at count 24. So the databook's chart shows
the reset as a greyed region, not as a crossed cell in the OUT-8 lane; the "crossed column" language
describes the mask programming, not the printed table.

### Other cross-rhythm structure in Table 1 (observations, not interpretations)
- **Identical lane pairs**, which recur often enough to be a feature of the mask rather than a
  coincidence: r1 OUT 2 = OUT 3 and OUT 1 = OUT 6; r2 OUT 2 = OUT 6; r3 OUT 2 = OUT 7; r4 OUT 2 =
  OUT 6; r9 OUT 3 = OUT 4. Rhythms 8, 10, 11 and 12 have no identical pair.
- **Lanes never used anywhere in Table 1:** none — every one of OUT 1..8 is used by at least one
  rhythm. But OUT 4, 5 and 6 are empty in the large majority of blocks, and **OUT 6 is used by only
  five of the twelve** (r1, r2, r4, r6, r9).
- OUT 8, where used, is always built on the every-odd-count spine, with at most two interpolated
  even-count marks (r8 at 4 and 20). It never carries an independent pattern.
- Rhythms **9 and 10 are exactly 16-PERIODIC on all eight lanes**; no other block is.

---

## TABLE 2 (M253 AC) — factory standard mask, PDF pp.135-136 (printed pp.142-143)

### RHYTHM NAMES — **Table 2 DOES print them, unlike Table 1**
Every block in Table 2 is captioned with a name in parentheses after the rhythm number, e.g.
`RHYTHM 1 (WALTZ)`. This is the single biggest difference in presentation between the two masks
printed for the M253: **AA is anonymous, AC is named.**

Still, and this matters: **the names label the RHYTHM, never the outputs.** `OUT 1..8` remain chip
pins throughout Table 2. Nothing in this table assigns an instrument to a pin, so no lane in any
block below is called a bass drum or a hi-hat here.

### Band 2 of PDF p.135 (Table 2, rhythms 1-5) — measured geometry
Derived on this page from scratch.
- **Vertical rules** (column ink > 0.5, over y=3250..4550): heavy rules at x=389-395 (table left
  edge), 695-700, 1254-1260, 1811-1817, 2371-2378, 2929-2936, 3475-3484 (right edge). So
  **count column 392-698, rhythm 1 = 698-1257, rhythm 2 = 1257-1814, rhythm 3 = 1814-2374,
  rhythm 4 = 2374-2932, rhythm 5 = 2932-3480**, each an eight-column block of 68.5-70 px. All 35
  independently detected interior rules land within 4 px of the eighth-divisions of those spans.
  (Two sub-threshold runs at x=555 and x=563-567 are the words "COUNT FOR 32", not rules.)
- **Row rules** (row ink > 0.5, over x=400..690): heavy header rule at y=3172-3177, then exactly
  **32** row-bottom rules from y=3229 to y=4581. Row 1's top is taken at y=3184. Recorded in
  `m253b_rows_p135b.txt`; the detected boundaries are used, not a constant pitch.

### TABLE 2 (M253 AC) — RHYTHM 1 (WALTZ)
Printed name: **WALTZ** (caption reads `RHYTHM 1 (WALTZ)`, PDF p.135 / printed p.142).
Reset count: **23** — counts **24-32 are greyed**. Read off the printed count digits in the crop, so
it cannot be a row-alignment error: the row labelled 23 is white and ruled, the row labelled 24 is
the first stippled one, and the ink pass confirms it (counts 24-32 read a uniform 0.15-0.21 across
all eight lanes at once, which no pattern of X marks produces).

```
count:   12345678901234567890123
OUT 1    x...........x..........
OUT 2    ....x...x......xx...x..
OUT 3    .......................
OUT 4    .......................
OUT 5    .......................
OUT 6    .......................
OUT 7    x...........x..........
OUT 8    .......................
```
Status: **CONFIDENT.** Only 9 marks in the block, each read 0.15-0.18 against a 0.00-0.02 floor;
nothing borderline. **OUT 3, 4, 5, 6 and 8 wholly empty** — the OUT 4 and OUT 8 columns carry a
0.02-0.05 dashed-rule floor over the first third of the block which fades out; no glyph strokes.

- **OUT 1 and OUT 7 carry IDENTICAL lanes** (counts 1 and 13 only).
- **OUT 2 has two ADJACENT marks, at counts 16 and 17.** This was the one cell pair worth zooming
  (`m253b_ac1zoom.png`, 2x): both glyphs are complete and unmistakable, in consecutive rows of the
  same column. It is the only adjacency in the block, and it is the reason the block is NOT the
  clean 12+12 figure the reset-23 length and the OUT 1 spacing (1, 13) would suggest. Recorded as
  seen; no attempt is made to regularise it.
- **RESET 23 IS NOTEWORTHY and is stated exactly as printed.** Every short rhythm in Table 1 greys
  25-32 (last live count 24). This block greys 24-32, so its last live count is 23 — an ODD cycle
  length, and the only value of its kind found so far in either table. What the chart shows is the
  greyed region; how the mask spends the 24th state is not printed and is not inferred here.

### TABLE 2 (M253 AC) — RHYTHM 2 (TANGO)
Printed name: **TANGO** (caption `RHYTHM 2 (TANGO)`).
Reset count: **32** (no greyed cells). Block x=1257-1814, crop `m253b_ac2.png`.

```
count:   12345678901234567890123456789012
OUT 1    x.......x.......x.......x.......
OUT 2    ................................
OUT 3    ................................
OUT 4    ............................xxxx
OUT 5    ............................xxxx
OUT 6    x.......x.......x.......x...x...
OUT 7    ................................
OUT 8    ................................
```
Status: **CONFIDENT.** Only 17 marks; every one reads 0.15-0.20 against a 0.00-0.05 floor.
**OUT 2, 3, 7 and 8 wholly empty.** The OUT 4 and OUT 5 columns show a 0.02-0.05 dashed-rule floor
over counts 1-20 which fades away; the crop shows no glyph strokes there.

- **OUT 4 and OUT 5 carry IDENTICAL lanes**: a FOUR-count consecutive run at 29, 30, 31, 32 and
  nothing else in the whole 32 counts. All eight glyphs were checked individually (0.16-0.18).
- **OUT 6 is OUT 1 plus one extra mark, at count 29** — the count where the OUT 4/OUT 5 run starts.
- The block has only two distinct rhythmic events: an every-8th-count pulse (1, 9, 17, 25) and a
  four-count burst at the very end of the cycle.
- This is the sparsest use of the eight lanes in Table 2 so far: four lanes used, and two of those
  four are duplicates of each other.

### TABLE 2 (M253 AC) — RHYTHM 3 (MARCH)
Printed name: **MARCH** (caption `RHYTHM 3 (MARCH)`).
Reset count: **32** (no greyed cells). Block x=1814-2374, crop `m253b_ac3.png`.

```
count:   12345678901234567890123456789012
OUT 1    x.......x.......x.......x.......
OUT 2    ....x...x...x.......x.......x.x.
OUT 3    ................................
OUT 4    ................................
OUT 5    ............................x.x.
OUT 6    ................................
OUT 7    x.......x.......x.......x.......
OUT 8    ................................
```
Status: **CONFIDENT.** 16 marks, all reading 0.15-0.25 against a 0.00-0.07 floor.
**OUT 3, 4, 6 and 8 wholly empty** (OUT 6 carries a flat 0.02-0.05 dashed-rule floor down its whole
length, which is why it was checked on the crop; no glyphs).

- **OUT 1 and OUT 7 carry IDENTICAL lanes** (1, 9, 17, 25) — the same duplicated-lane pair, on the
  same two pins, as the WALTZ block.
- **OUT 2 is deliberately IRREGULAR and is recorded exactly as printed**: 5, 9, 13, 21, 29, 31. The
  regular every-4th-count series from 5 would be 5, 9, 13, 17, 21, 25, 29; **counts 17 and 25 are
  BLANK in OUT 2** (both read 0.00, and both rows were re-checked on the crop, where the only
  glyphs are in OUT 1 and OUT 7), and count 31 is an addition to the series. So the lane thins out
  exactly where OUT 1 and OUT 7 strike.
- OUT 5 fires only twice, at 29 and 31, doubling OUT 2's last two marks.

### TABLE 2 (M253 AC) — RHYTHM 4 (SWING)
Printed name: **SWING** (caption `RHYTHM 4 (SWING)`).
Reset count: **32** (no greyed cells). Block x=2374-2932, crop `m253b_ac4.png`.

```
count:   12345678901234567890123456789012
OUT 1    x...x...x...x...x...x...x...x..x
OUT 2    x...x.......x.......x..........x
OUT 3    x...x.......x.......x..........x
OUT 4    ................................
OUT 5    x...x..xx...x..xx...x..xx...x..x
OUT 6    ................................
OUT 7    ..............................x.
OUT 8    ................................
```
Status: **CONFIDENT.** Marks read 0.16-0.30, blanks 0.00-0.09; the OUT 2 column carries a 0.02-0.07
dashed-rule floor down its whole length, so every blank in that lane was checked on the crop.
**OUT 4, OUT 6 and OUT 8 wholly empty.** The densest block in Table 2 so far (39 marks).

- **OUT 2 and OUT 3 carry IDENTICAL lanes** (1, 5, 13, 21, 32).
- **OUT 5 is strictly 8-PERIODIC**: `x...x..x` repeated four times (1, 5, 8 / 9, 13, 16 / 17, 21, 24
  / 25, 29, 32). The three pairs of adjacent marks at 8+9, 16+17 and 24+25 were each checked
  individually; all six glyphs are complete.
- **OUT 1 is every 4th count from 1 (1, 5, …, 29) plus one extra at 32.** That trailing 32 is what
  breaks the block out of a clean 8- or 16-periodicity, and OUT 2, OUT 3 and OUT 5 all land on 32
  too — five of the eight lanes fire together on the last count of the cycle.
- **OUT 7 fires exactly ONCE in the whole block, at count 31** (0.19 against 0.00-0.04 in that lane
  elsewhere). It is the only mark in the table so far that stands completely alone in its lane.

### TABLE 2 (M253 AC) — RHYTHM 5 (MAMBO)
Printed name: **MAMBO** (caption `RHYTHM 5 (MAMBO)`).
Reset count: **32** (no greyed cells). Block x=2932-3480, crop `m253b_ac5.png`.

```
count:   12345678901234567890123456789012
OUT 1    x.x.x.....x.x...x.x.x.....x.x...
OUT 2    .......xx.x.x.x.......x...xx..x.
OUT 3    ................................
OUT 4    ..xxxxxx..........x.x.x.........
OUT 5    x...x...x.x...x...x...x.x.x.x...
OUT 6    ................................
OUT 7    x.....x.....x.......x...x.......
OUT 8    x.......x.......x.......x.......
```
Status: **CONFIDENT.** The densest and most irregular block in either table (53 marks over six
lanes). Marks read 0.17-0.30, blanks 0.00-0.07; every one of the 256 cells was read on the crop and
the ink pass agrees on all of them, with nothing borderline.

- **OUT 3 and OUT 6 wholly empty.**
- **No two lanes are identical**, and no lane is periodic. This is the only block in either table
  where that is true of all eight lanes at once.
- **OUT 4 opens with a SIX-count consecutive run, counts 3-4-5-6-7-8** (each glyph 0.19-0.25), then
  is silent for ten counts and returns as 19, 21, 23. Six in a row is the longest consecutive run
  anywhere in either M253 table.
- **OUT 8 is a plain every-8th-count lane (1, 9, 17, 25)** — not the odd-count spine it always is in
  Table 1. Its four marks are the brightest in the block (0.25-0.30).
- OUT 7 is 1, 7, 13, 21, 25: uneven spacing (6, 6, 8, 4), recorded as printed.
- The one cell worth naming as noise: **count 1, OUT 2** carries a small isolated dot (ink 0.05,
  visible in the crop as a single blob with no X strokes). It is recorded as BLANK. Every real mark
  in this block reads 0.17 or above, so there is a clean factor-of-three gap.

### Band 1 of PDF p.136 (Table 2, rhythms 6-10) — measured geometry
- **Vertical rules** (column ink > 0.5, y=1250..2450): heavy rules at x=400-409 (table left edge),
  693-698, 1247-1253, 1804-1810, 2363-2370, 2922-2928, 3484-3489 (right edge). So
  **count column 405-696, rhythm 6 = 696-1250, rhythm 7 = 1250-1807, rhythm 8 = 1807-2367,
  rhythm 9 = 2367-2925, rhythm 10 = 2925-3487**, each eight columns of 69.3-70.3 px; all 35
  detected interior rules land within 4 px of the eighth-divisions. (The sub-threshold runs at
  x=539-543 and 555-569 are the words "COUNT FOR 32".)
- **Row rules** (row ink > 0.5, x=420..690): heavy header rule y=1105-1112, then exactly **32**
  row-bottom rules from y=1164 to y=2513. Row 1's top taken at y=1118. Recorded in
  `m253b_rows_p136a.txt`.

### TABLE 2 (M253 AC) — RHYTHM 6 (SLOW ROCK)
Printed name: **SLOW ROCK** (caption `RHYTHM 6 (SLOW ROCK)`, PDF p.136 / printed p.143).
Reset count: **24** — counts **25-32 are greyed** (the stipple reads a uniform 0.19-0.26 across all
eight lanes at once, starting on the row labelled 25). Block x=696-1250, crop `m253b_ac6.png`.

```
count:   123456789012345678901234
OUT 1    x.........x.x...........
OUT 2    ......x...........x...x.
OUT 3    ........................
OUT 4    ........................
OUT 5    x.x.x.x.x.x.x.x.x.x.x.x.
OUT 6    x.....x.....x.....x.....
OUT 7    ........................
OUT 8    ........................
```
Status: **CONFIDENT.** 22 marks, all 0.16-0.19 against a 0.00-0.04 floor; nothing borderline.
**OUT 3, 4, 7 and 8 wholly empty** — the OUT 8 column shows a 0.01-0.04 dashed-rule floor over
counts 1-12 that fades out, with no glyph strokes.

- **OUT 5 is every ODD count 1-23, no exceptions** — the whole 24-count cycle.
- **OUT 6 is a clean every-6th-count lane** (1, 7, 13, 19), the only strictly periodic 6-spaced lane
  in either table.
- OUT 1 (1, 11, 13) and OUT 2 (7, 19, 23) are both irregular and neither repeats at 12.
- **OUT 8 EMPTY on a 24-count rhythm — the reset rule again satisfied** (see the Table 2 summary at
  the end of this file for the full test).

### TABLE 2 (M253 AC) — RHYTHM 7 (BEAT)
Printed name: **BEAT** (caption `RHYTHM 7 (BEAT)`).
Reset count: **32** (no greyed cells). Block x=1250-1807, crop `m253b_ac7.png`.

```
count:   12345678901234567890123456789012
OUT 1    x.....x.x.......x.....x.x.......
OUT 2    ....x.......x.......x.......x.x.
OUT 3    ................................
OUT 4    ................................
OUT 5    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUT 6    ..x.......x.......x.......x.....
OUT 7    ................................
OUT 8    ................................
```
Status: **CONFIDENT.** 41 marks, reading 0.14-0.19 against a 0.00-0.06 floor. Every mark falls on an
ODD count; there is no even-count mark anywhere in the block.
**OUT 3, 4, 7 and 8 wholly empty.**

- **OUT 5 is every ODD count 1-31, no exceptions** — every other lane's marks land on top of it, so
  OUT 5 alone is what the other lanes accent.
- **OUT 6 is a clean every-8th lane offset by two** (3, 11, 19, 27).
- **OUT 1 is 16-periodic** (1, 7, 9 then 17, 23, 25).
- **OUT 2 is the one lane that breaks**: 5, 13, 21, 29 is a clean every-8th series, and then there
  is **one extra mark at count 31** (0.16, checked on the crop, unambiguous). So OUT 2 is the only
  reason this block is not exactly 16-periodic.
- **OUT 8 empty on a FULL 32-count rhythm.** Recorded because it is the Table 2 counterpart of
  Table 1's rhythms 1, 4 and 6: an empty OUT 8 by itself never means the rhythm is short.

### TABLE 2 (M253 AC) — RHYTHM 8 (SAMBA)
Printed name: **SAMBA** (caption `RHYTHM 8 (SAMBA)`).
Reset count: **32** (no greyed cells). Block x=1807-2367, crop `m253b_ac8.png`.

```
count:   12345678901234567890123456789012
OUT 1    x.....x.x.....x.x.....x.x.....x.
OUT 2    ....x.x.....x.x.....x...x.x.x.x.
OUT 3    ................................
OUT 4    x.....x.....x.......x.....x...x.
OUT 5    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUT 6    ................................
OUT 7    x.....x.....x.......x.....x.....
OUT 8    ..x.x.....x.x.......x.....x.....
```
Status: **CONFIDENT.** 60 marks over six lanes — the densest block in either table. Marks read
0.13-0.23, blanks 0.00-0.10; every mark falls on an ODD count, with no even-count mark anywhere,
which made the row-by-row check straightforward. **OUT 3 and OUT 6 wholly empty** (OUT 6 carries a
0.02-0.05 dashed-rule floor down its whole length; no glyph strokes).

- **OUT 7 is OUT 4 minus its LAST mark**: OUT 4 is 1, 7, 13, 21, 27, 31 and OUT 7 is 1, 7, 13, 21,
  27. The two lanes are otherwise identical, and count 31 is the only cell that separates them
  (OUT 4 reads 0.16 there, OUT 7 reads 0.01). Both cells were re-checked on the crop.
- **OUT 1 is strictly 8-PERIODIC**: `x.....x.` repeated (1, 7 / 9, 15 / 17, 23 / 25, 31).
- OUT 5 is every ODD count 1-31, no exceptions — the same role it plays in SLOW ROCK and BEAT.
- OUT 2 and OUT 8 are both irregular: OUT 2 runs 5, 7, 13, 15, 21 and then thickens to 25, 27, 29,
  31 over the last eight counts; OUT 8 runs 3, 5, 11, 13 then thins to 21 and 27.
- The count-27 row is the faintest in the block (five marks at 0.13-0.14 against a 0.00-0.02 floor).
  All five were confirmed on the crop as complete glyphs before being recorded.

### TABLE 2 (M253 AC) — RHYTHM 9 (BOSSA NOVA)
Printed name: **BOSSA NOVA** (caption `RHYTHM 9 (BOSSA NOVA)`).
Reset count: **32** (no greyed cells). Block x=2367-2925, crop `m253b_ac9.png`.

```
count:   12345678901234567890123456789012
OUT 1    x.x.....x.x.....x.x.....x.x.....
OUT 2    x.....x.....x.......x.....x.....
OUT 3    x.....x.....x.......x.....x.....
OUT 4    x.....x.x...x.......x.....x.....
OUT 5    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUT 6    ....x.x.x.........x...x.........
OUT 7    x.........x...x...........x.x.x.
OUT 8    x.....x.....x.......x.....x.....
```
Status: **CONFIDENT.** 59 marks; **all eight outputs are in use — the only block in Table 2 with no
empty lane.** Marks read 0.14-0.25, blanks 0.00-0.11. Every mark falls on an ODD count.

- **THREE lanes are IDENTICAL: OUT 2, OUT 3 and OUT 8 all carry exactly 1, 7, 13, 21, 27.** This is
  the only three-way lane duplication found anywhere in either M253 table, and each of the fifteen
  cells was checked separately.
- **OUT 4 is that same lane plus ONE extra mark at count 9** (0.17 there, against 0.00 in OUT 2,
  OUT 3 and OUT 8 at 9).
- **OUT 1 is strictly 8-PERIODIC**: `x.x.....` repeated (1, 3 / 9, 11 / 17, 19 / 25, 27).
- OUT 5 is every ODD count 1-31, no exceptions.
- OUT 6 (5, 7, 9, 19, 23) and OUT 7 (1, 11, 15, 27, 29, 31) are the two irregular lanes; OUT 7 in
  particular is silent from count 16 to 26 and then fires three times in the last six counts.
- **A noise note for count 29**, where a dashed rule crosses: OUT 1, OUT 2 and OUT 6 read 0.10, 0.11
  and 0.07 there, which is well above the block's own 0.00-0.03 blank floor. The crop shows the rule
  fragment and no glyph strokes; the only real marks in row 29 are OUT 5 (0.24) and OUT 7 (0.25).
  Recorded as blanks. This is the page-noise class the inherited warning describes, and the ratio to
  the real marks in the same row is better than 2:1, so the eye read is not in tension with the ink.

### TABLE 2 (M253 AC) — RHYTHM 10 (CHA-CHA)
Read in the MAIN SESSION after the third agent on this file stalled. PDF p.136 (printed p.143),
band 1 block 5, x = 2925-3487, band-1 row rules from `m253b_rows_p136a.txt` (33 boundaries,
y 1118-2510). Reset count: **32** (no greyed cells).
Method: per-cell ink fraction over the located cell rectangles AND an independent eye read of a
1500px-wide crop that includes the OUTPUT header digits. The two agree on every visible cell.
Marks measure 0.21-0.31 ink; blanks measure exactly 0.00, so there is no borderline cell here.
```
count:   1234567890123456789012345678901234  (1..32)
OUT 1    x...x...x...x...x...x...x.x.x...
OUT 2    x.x.............x..............x
OUT 3    ................................
OUT 4    x.x.....x...x.x.x.x.....x.x.x.xx
OUT 5    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUT 6    ................................
OUT 7    x...x...x...x...x...x...x.x.x...
OUT 8    x...x...x...x...x...x...........
```
Status: **CONFIDENT.** OUT 3 and OUT 6 wholly empty. OUT 5 is every ODD count (a straight
sixteenth spine); OUT 1 and OUT 7 are byte-identical to each other. Counts 25-29 break OUT 1/4/7's
every-4 pattern into an every-2 run, which is the only irregular passage in the rhythm.

### TABLE 2 (M253 AC) — RHYTHM 11 (RUMBA)
PDF p.136 band 2 (the page has a SECOND band below the first, holding rhythms 11 and 12 only —
worth stating because a single-band assumption loses them). Geometry measured here: vertical rules
at x = 410 (edge), 566, 697, then eight ~70px columns per block, so count column 410-697,
rhythm 11 = 697-1253, rhythm 12 = 1253-1811; 33 row boundaries y 3182-4581.
Reset count: **32** (no greyed cells). Same dual method; marks 0.22-0.33, blanks 0.00.
```
OUT 1    x.....x.....x...x.....x.....x...
OUT 2    x...................x...x.......
OUT 3    ................................
OUT 4    ............................x.x.
OUT 5    ..x.x.x.x.x...x...x.x...x.x...x.
OUT 6    ................................
OUT 7    x.....x.....x...x.......x.......
OUT 8    x.......x.......x...........x...
```
Status: **CONFIDENT.** OUT 3 and OUT 6 wholly empty. OUT 4 fires only twice, both at the very end
of the bar (counts 29 and 31), which reads as a fill into the repeat.

### TABLE 2 (M253 AC) — RHYTHM 12 (BEGUINE)
Same band and geometry as rhythm 11. Reset count: **32**. Marks 0.22-0.31, blanks 0.00.
```
OUT 1    x...........x...x.......x...x...
OUT 2    ..x...............x.............
OUT 3    ..x.......x...x...x.......x...x.
OUT 4    ..x...x.xxxx......x...x.........
OUT 5    x.xxx.x.x.x.x.x.x.xxx.x.x.x.x.x.
OUT 6    ................................
OUT 7    ..x...............x.............
OUT 8    x.....x.....x.......x...x.......
```
Status: **CONFIDENT.** OUT 6 wholly empty. OUT 2 and OUT 7 are byte-identical (counts 3 and 19
only). OUT 4 carries a four-long adjacent run at counts 9-12, which is the longest run in either
M253 mask and matters for playback: on this chip the trigger is the RISING EDGE of a ROM bit, so
that run is ONE trigger held four states, not four hits (Elektor, April 1976, p420).

## TABLE 2 (M253 AC) — COMPLETE (12 rhythms)
All twelve read: WALTZ, TANGO, MARCH, SWING, MAMBO, SLOW ROCK, BEAT, SAMBA, BOSSA NOVA, CHA-CHA,
RUMBA, BEGUINE.
- **AC has 12 rhythms, AD has 15.** AC's twelve names are exactly AD's FIRST TWELVE, in the same
  slots; AD's extra three (BAJON, FOX TROT, SHUFFLE) have no AC counterpart. So the earlier report
  that AC "reuses AD's names in the same slots" is confirmed as a naming fact.
- **Names matching is not content matching**, which is why this was worth reading rather than
  assuming: see the per-rhythm sections above and the cross-mask comparison below.
- **The OUT-8 reset rule holds here too.** Only the short rhythms (WALTZ at 23, SLOW ROCK at 24)
  have an empty OUT 8; every 32-count rhythm in AC uses OUT 8. That is the datasheet's "the column
  which now represents the reset output, rather than the 8th instrument", confirmed on a second
  part number.

## CROSS-MASK COMPARISON, done mechanically over all 720 pairs (main session)
Both datasets were parsed and every M253 block compared against every M252 block, lane by lane.

### 1. M253 AC **IS** M252 AD's first twelve, byte for byte
All 12 AC rhythms match their same-numbered, same-named AD rhythm on **all eight lanes**,
character for character (WALTZ, TANGO, MARCH, SWING, MAMBO, SLOW ROCK, BEAT, SAMBA, BOSSA NOVA,
CHA-CHA, RUMBA, BEGUINE). AD's extra three (BAJON, FOX TROT, SHUFFLE) have no AC counterpart.
So SGS shipped the same twelve rhythm ROMs under two part numbers, and **the AC mask contributes no
new pattern data at all**. That is worth stating loudly in both directions: the earlier warning
("names matching is not content matching") was the right thing to check, and here the answer came
back that they DO match, which only reading the cells could establish.

### 2. M253 AA is NOT M252 AA with the lanes shifted, but it does reuse LANES
Tested as "M253 OUT k+1 == M252 OUT k for all k", i.e. the whole block shifted down one output.
**No pair satisfies it.** The best pairs match 5 of 7 lanes, and most of those matches are two
empty lanes agreeing, which is trivial; counting only lanes that actually carry marks, the best
pairs manage 2 to 4 of 3 to 6. So the earlier per-block claims are narrower than they read: what
holds is **lane-level** reuse (this file records two lanes that are character-for-character
identical to M252 AA lanes one output up), not block-level. Every M253 AA block also adds marks on
OUT 1 that its nearest M252 relative does not have (1 to 5 marks).
**Conclusion:** M253 AA's twelve rhythms are genuine new data; M253 AC's twelve are duplicates.


---

# Second wave (2026-08-20): the chip family, the patents, and two rejections

The files below cover everything found after the first three machines: the rest of the SGS rhythm
line, the accompaniment question, and the patent route. Two of them record work that was thrown
away, which is why they are here:

- `patents2.md` holds the reading of US 4,292,874 FIG. 14 that was **rejected**. It failed the
  patent's own damp-one-step-before-trigger relation, and the figure turned out to be hand-drawn
  waveforms plus a staff rather than a dot grid. The numbers in that file are NOT in the curated doc
  or the header. Kept because the next person should see what was tried.
- `organs.md` is a hunt that stalled early. Its value is the patent shortlist it left behind, one of
  which (US 3,567,838) became the only source in the project with chord and bass lanes.



# ===== source file: sgs_m255_m254.md =====

# SGS M255 / M254 rhythm-ROM truth tables — transcription

## Provenance

- Source: `<scratch>/sgs_1979_databook.pdf` — "1979 SGS MOS and Special COS/MOS", 1st edition.
  Already on disk; not re-downloaded.
- Masks transcribed here are **SGS FACTORY standard contents** only. Both target tables are
  captioned "standard content". No Elgam / customer masks are involved, and none are published
  in this databook.
- M255: PDF page 150 (printed page 158), caption "TRUTH TABLE of M 255 B1-AB (standard content)".
  Pre-rendered at 600 dpi as `<scratch>/m255_p150-150.png` by a previous agent, which also
  verified the embedded scan is genuinely 600 dpi and fully populated.
- M254: pages to be located via the PDF text layer (`pdftotext -layout` + grep), tables 1 and 2 =
  M254 AD and M254 AM standard contents.
- Method: read by eye from 600 dpi crops that include BOTH the counter-state row numbers and the
  OUTPUT column header digits, cross-checked with a per-cell ink-fraction measurement over
  located cell rectangles. No blob/circle detection (over-detects badly on this material).
- Notation below: one line per output, one character per elementary time 1..length,
  `x` = a cross (X) in the cell, `.` = empty cell.
- OUTPUT n are chip PINS. RHYTHM n is only ever "RHYTHM n" in the grid; any pairing with the
  pinout's named rhythm-select inputs is asserted only if visibly printed.
- Hard rule followed: nothing written here was not read off the page. UNREAD is recorded as UNREAD.

## Status log

- (start) file created, nothing transcribed yet.
- M255 B1-AB done: 6 rhythms x 5 outputs, lengths verified, option rows quoted.
- M254 AD done: 8 rhythms x 12 outputs + counting control (lengths 24 for rhythms 1 and 8).
- M254 AM done: 8 rhythms x 12 outputs + counting control (lengths 24 for rhythms 1, 6 and 7).
- M255 pinout pairing READ (page 146, two side-by-side connection diagrams). FINISHED.

---

# M255 B1-AB — TRUTH TABLE (standard content)

PDF page 150 / printed page 158. Caption read verbatim:
**"TRUTH TABLE of M 255 B1-AB (standard content)"**

Body text on the same page, verbatim:
> "The ROM truth table has been organized in 16 rows which represent the elementary times and 30
> columns (6 groups of 5). The timing for the beats required for each instrument is programmed by
> crossing the appropriate box. The options for outputs and down beat must also be filled in as
> explained. Table 1 shows the content and the options programmed in the M 255 B1-AB standard
> content."

Grid shape confirmed by eye at 600 dpi: 16 numbered rows under the header "Counter state",
6 groups ("RHYTHM 1" .. "RHYTHM 6") of 5 vertically-lettered columns "OUTPUT 1".."OUTPUT 5".

## RHYTHM 1 — length 12 (rows 13-16 greyed, uniformly across all 5 of its columns)

```
elementary time:  1 2 3 4 5 6 7 8 9 10 11 12
OUTPUT 1          x . . . . . x . . .  .  .
OUTPUT 2          . . x . x . . . x .  x  .
OUTPUT 3          x . . . . . x . . .  .  .
OUTPUT 4          . . . . . . x . . .  .  .
OUTPUT 5          . . x . x . . . x .  x  .
```
Notes: OUTPUT 2 and OUTPUT 5 are byte-identical lanes. OUTPUT 1 and OUTPUT 3 are also
byte-identical to each other. Even-numbered elementary times are all empty in this rhythm.

## RHYTHM 2 — length 16 (no greying anywhere in its 5 columns)

```
elementary time:  1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
OUTPUT 1          x . . x x . . . x x  .  x  x  .  x  x
OUTPUT 2          . . x . . . x . . .  x  .  .  x  .  x
OUTPUT 3          x x x x x x x x x x  x  x  x  x  x  x
OUTPUT 4          . . . . . . . . . .  .  .  .  .  .  x
OUTPUT 5          x . x . . x . . x .  .  x  .  .  .  .
```
Notes: OUTPUT 3 is marked on EVERY elementary time (a continuous 16-time lane).
OUTPUT 4 carries a single mark, at time 16 only.

## RHYTHM 3 — length 12 (rows 13-16 greyed, uniformly across all 5 of its columns)

```
elementary time:  1 2 3 4 5 6 7 8 9 10 11 12
OUTPUT 1          x . . x . . x . . x  .  .
OUTPUT 2          . . . x . . . . . x  .  .
OUTPUT 3          x . . x . x x . . x  .  x
OUTPUT 4          . . . . . . x . . x  .  .
OUTPUT 5          . . . x . . . . . x  .  .
```
Notes: OUTPUT 2 and OUTPUT 5 are byte-identical lanes again (marks at 4 and 10 only).

## RHYTHM 4 — length 12 (rows 13-16 greyed, uniformly across all 5 of its columns)

```
elementary time:  1 2 3 4 5 6 7 8 9 10 11 12
OUTPUT 1          x . . . . x x . . .  .  x
OUTPUT 2          . . . x . . . . . x  .  .
OUTPUT 3          x x x x x x x x x x  x  x
OUTPUT 4          . . . . . . x . . .  .  x
OUTPUT 5          . . . x . . . . . x  .  .
```
Notes: OUTPUT 3 is marked on every one of its 12 elementary times.
(RHYTHM 4 OUTPUT 5 read from the r456 crop: marks at 4 and 10 only, so OUTPUT 2 and OUTPUT 5
are byte-identical in RHYTHM 4 as well.)

## RHYTHM 5 — length 16 (no greying in its 5 columns)

```
elementary time:  1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
OUTPUT 1          x . x x . . x . x .  .  .  .  .  x  x
OUTPUT 2          x . . x . . x . . .  x  .  .  x  .  .
OUTPUT 3          x x x x x x x x x x  x  x  x  x  x  x
OUTPUT 4          . . . . . . x . . .  .  .  .  .  x  x
OUTPUT 5          . x . x . x x . x .  x  .  x  x  .  x
```
Notes: OUTPUT 3 marked on every one of the 16 elementary times.

## RHYTHM 6 — length 16 (no greying in its 5 columns)

```
elementary time:  1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
OUTPUT 1          x . . . x . . . x .  .  .  x  .  x  .
OUTPUT 2          x . . . x . . . x .  .  .  x  .  x  .
OUTPUT 3          . . . . . . . . . .  .  .  .  .  x  .
OUTPUT 4          . . . . . . . . . .  .  .  .  .  x  .
OUTPUT 5          x . . . x . . . x .  .  .  x  .  x  .
```
Notes: the sparsest rhythm on the mask. Marks occur only at elementary times 1, 5, 9, 13 and 15.
OUTPUT 1, OUTPUT 2 and OUTPUT 5 are three byte-identical lanes; OUTPUT 3 and OUTPUT 4 carry a
single mark each, both at time 15 (where all five outputs fire together).

## M255 option rows — verbatim

The four rows printed below the grid, left-hand labels exactly as set:

```
| Option on the Outputs             | O1 | O2 | O3 | O4 | O5 |            | 16 (12) | 8 (6) |
| Continuous or Trigger Output      | T  | T  | T  | T  | T  | Down beat  |    X    |       |
| Open drain or push-pull           | O  | O  | O  | O  | O  |            |         |       |
| Positive or Negative Trigger Edge | +  | +  | +  | +  | +  |            |         |       |
```

So the M255 B1-AB standard content specifies, for all five outputs alike:
TRIGGER output (T, not continuous) · OPEN DRAIN (O, not push-pull) · POSITIVE trigger edge (+).
The down-beat option is crossed in the "16 (12)" cell and left blank in the "8 (6)" cell.
(The parenthesised numbers pair with the two rhythm lengths present on this mask: 16 for the
full-length rhythms, 12 for the shortened ones.)

## M255 verification (ink-fraction cross-check)

Grid rectangles were located by projection (32 vertical rules at x = 709,809,904,1000,1091,1176,
1273,1370,1459,1550,1639,1740,1833,1922,2013,2103,2199,2291,2382,2476,2566,2660,2755,2850,2940,
3031,3128,3218,3309,3395,3486 px; 16 row bands from y=3363 to y=4426), then the black-ink fraction
of each cell was measured with a 9 px inset.

- cells holding a cross: **0.09 - 0.19** (mean ~0.145)
- empty cells: **0.00** (one cell read 0.01, a speck visible in the crop at RHYTHM 4 / OUTPUT 1 /
  time 5, treated as dirt, not a mark)
- greyed cells: **0.13 - 0.18**, i.e. indistinguishable from a cross by ink alone. Greying was
  therefore identified the prescribed way: a uniform fill across ALL FIVE columns of one rhythm
  simultaneously, for rows 13-16.

Every mark in the transcription above was confirmed by this measurement; the by-eye reading and
the measured grid agree cell-for-cell with no exceptions.

## M255 lengths (VERIFIED, not assumed)

| rhythm | greyed rows | length |
|---|---|---|
| RHYTHM 1 | 13,14,15,16 | 12 |
| RHYTHM 2 | none | 16 |
| RHYTHM 3 | 13,14,15,16 | 12 |
| RHYTHM 4 | 13,14,15,16 | 12 |
| RHYTHM 5 | none | 16 |
| RHYTHM 6 | none | 16 |

The greyed block under RHYTHM 3 and RHYTHM 4 is one continuous rectangle spanning all ten of
their columns, which is why it reads as a single band on the page. This confirms the prediction
in the earlier survey: three 12-time rhythms (1, 3, 4) and three 16-time rhythms (2, 5, 6).

M255 B1-AB: **COMPLETE.**

---

# M254 — where the tables are, and how lengths are defined

Located via the PDF text layer (`pdftotext -layout` + grep), not by rendering pages.

- PDF page 141 (printed 149) — "COMPLETING THE TRUTH TABLE" prose.
- PDF page 142 (printed 150) — heading **"M 254 AD (standard)"**; two stacked blocks:
  RHYTHM 1 (WALTZ) / RHYTHM 2 (TANGO) / RHYTHM 3 (SWING), then
  RHYTHM 4 (BEAT) / RHYTHM 5 (BOSSA NOVA) / RHYTHM 6 (SAMBA).
- PDF page 143 (printed 151) — third AD block: RHYTHM 7 (RUMBA) / RHYTHM 8 (SLOW ROCK) /
  **COUNTING CONTROL**; then heading **"M 254 AM (standard)"** with its first block:
  RHYTHM 1 (WALTZ) / RHYTHM 2 (POLKA) / RHYTHM 3 (TANGO).
- Both scans are 600 dpi (`pdfimages -list`: 3893x5102 jbig2 gray, x-ppi 600, y-ppi 600), rendered
  with `pdftoppm -r 600`.

Rhythm names are PRINTED in the group headers and were read there (they are also legible in the
text layer). AD names confirmed on page 142/143: 1 WALTZ, 2 TANGO, 3 SWING, 4 BEAT, 5 BOSSA NOVA,
6 SAMBA, 7 RUMBA, 8 SLOW ROCK.

Prose from PDF page 141, verbatim (the length convention):
> "The ROM truth table has been organized in 32 rows which represent the elementary times and 104
> columns. The first 8 groups of 12 columns represent the rhythms which have 12 programmable
> outputs. The timing for the beats required for each instrument is programmed by crossing the
> appropriate box. The 9th group of 8 columns represents the COUNTING control information which
> specifies the number of elementary times in a given rhythm.
> If count N is crossed for rhythm X this rhythm will have N elementary times. If the counting
> control column for a particular rhythm does not contain a cross that rhythm will have 32
> elementary times.
> Table 1 and 2 show the truth tables of the M 254 AD and M 254 AM, standard contents,
> respectively. It can be seen that in the table 1 the rhythms 1 and 8 and in the table 2 the
> rhythms 1, 6 and 7, have 24 elementary times."

Note: the printed COUNTING CONTROL group on page 143 is headed with TWELVE vertical labels
"RHYTHM 1" .. "RHYTHM 12", not the eight the prose describes; only columns 1-8 can carry meaning
for an 8-rhythm part. Read as printed, not corrected.

## Method used for the M254 grids

Per block: vertical rules and row rules located by ink projection, then the black-ink fraction of
every cell measured with a 9 px inset (600 dpi). Separation is clean and quoted per block below.
Greying is again indistinguishable by ink alone (0.16-0.21, same as a cross) and was identified by
being uniform across ALL TWELVE columns of a rhythm at once. Marks are only ever asserted where
BOTH the measurement and a by-eye read of a 600 dpi crop agree.

## M254 AD — block 1 (PDF page 142)

Cell statistics for this block: RHYTHM 1 min-mark 0.138 / max-blank 0.000;
RHYTHM 2 min-mark 0.120 / max-blank 0.001; RHYTHM 3 min-mark 0.113 / max-blank 0.000.
By-eye verification: the whole of RHYTHM 1 (all 12 outputs x 24 live rows) was re-read from a
1.5x-enlarged 600 dpi crop and agrees with the measurement cell-for-cell.

### AD RHYTHM 1 (WALTZ) — 24 elementary times (rows 25-32 greyed across all 12 columns)
```
time:             123456789012345678901234
OUTPUT 1          x...........x...........
OUTPUT 2          ....x...x......xx...x...
OUTPUT 3          x...........x...........
OUTPUT 4          ........................
OUTPUT 5          ............x...........
OUTPUT 6          ....x...x...x...x...x...
OUTPUT 7          x.......x.......x.......
OUTPUT 8          ....x...x.......x...x...
OUTPUT 9          x.x.x.x.x.......x.x.x.x.
OUTPUT 10         x.x.......x.x.x.......x.
OUTPUT 11         x...x.x...x.x.x...x.x...
OUTPUT 12         ..x.x...x.x...x.x...x.x.
```
OUTPUT 4 is WHOLLY EMPTY in this rhythm (recorded, not an omission).
OUTPUT 1 and OUTPUT 3 are byte-identical lanes.

### AD RHYTHM 2 (TANGO) — 32 elementary times (no greying)
```
time:             12345678901234567890123456789012
OUTPUT 1          x.......x.......x.......x.......
OUTPUT 2          x.......x.......x.......x...xxxx
OUTPUT 3          ............................x...
OUTPUT 4          ................................
OUTPUT 5          ................................
OUTPUT 6          ........x.......x.......x...x...
OUTPUT 7          x...............x...........x...
OUTPUT 8          x.......x.......x.......x...x...
OUTPUT 9          x.x.x.x.x...x.x.x...x.x...x.x.x.
OUTPUT 10         x...x.x.......x.........x.......
OUTPUT 11         x.x...x.x...x...x...x.x.x...x.x.
OUTPUT 12         ..x.x...x...x.x.......x.x.x...x.
```
OUTPUT 4 and OUTPUT 5 are both WHOLLY EMPTY. OUTPUT 2's tail (29,30,31,32) is a fill.

### AD RHYTHM 3 (SWING) — 32 elementary times (no greying)
```
time:             12345678901234567890123456789012
OUTPUT 1          x.......x.......x.......x.......
OUTPUT 2          ....x.......x.......x......xx..x
OUTPUT 3          x...x..xx...x..xx...x..xx..xx..x
OUTPUT 4          ................................
OUTPUT 5          ............x...x...x...........
OUTPUT 6          ....x...x.......x.......x...x...
OUTPUT 7          x.......x...............x.......
OUTPUT 8          ....x.......x.......x.......x...
OUTPUT 9          x.x.x.x.x.x.x.x.x...x.x...x.x...
OUTPUT 10         x...x.x...x.x.....x.....x.....x.
OUTPUT 11         x.x...x.x.x...x.x.x...x.x.x...x.
OUTPUT 12         ..x.x...x...x.x...x.x...x...x.x.
```
OUTPUT 4 is WHOLLY EMPTY here too.

## M254 AD — block 2 (PDF page 142, lower half)

No greying anywhere in this block, so rhythms 4, 5 and 6 all run 32 elementary times
(cross-checked against the counting control group below).
Cell statistics: RHYTHM 4 min-mark 0.147 / max-blank 0.039; RHYTHM 5 min-mark 0.141 /
max-blank 0.007; RHYTHM 6 min-mark 0.140 / max-blank 0.044.
(The larger max-blank figures come from a dotted interior rule clipping the cell rectangle in
RHYTHM 6 column 7 and one cell of RHYTHM 4; the mark/blank gap is still better than 3x.)
By-eye verification: RHYTHM 4 rows 1-15, all 12 outputs, re-read from a 1.55x crop — agrees
cell-for-cell with the measurement.

### AD RHYTHM 4 (BEAT) — 32 elementary times
```
time:             12345678901234567890123456789012
OUTPUT 1          x.....x.x.......x.x...x.x.....x.
OUTPUT 2          ....x.......x.......x.....x...x.
OUTPUT 3          x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUTPUT 4          ................................
OUTPUT 5          x...x..x........x...x..x........
OUTPUT 6          x...x..x....x.x.x...x..x....x.x.
OUTPUT 7          ........x.x...x.........x.x...x.
OUTPUT 8          x...x.....x.....x.....x.........
OUTPUT 9          x.x.x.x.x.x...x.x.x.x.xxx.x.x.x.
OUTPUT 10         x.x.x.......x..........x..x.x.x.
OUTPUT 11         x.x...x.x...x...x...x.x.x...x...
OUTPUT 12         x...x.x...x.x.x...x...xxx.x...x.
```
OUTPUT 3 fires on every odd elementary time (all 16 of them). OUTPUT 4 WHOLLY EMPTY.

### AD RHYTHM 5 (BOSSA NOVA) — 32 elementary times
```
time:             12345678901234567890123456789012
OUTPUT 1          x.....x.x.....x.x.......x...x.x.
OUTPUT 2          x.....x.....x.......x.....x.....
OUTPUT 3          x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUTPUT 4          ....x.......x.......x.......x...
OUTPUT 5          x...........x...x...............
OUTPUT 6          x.....x...x.x.x.x.x...x...x.x...
OUTPUT 7          ..x.......x...x...x.......x...x.
OUTPUT 8          x...x...x.x...x...x...x...x.x...
OUTPUT 9          x...x.x.x.x.x.....x...x...x.x.x.
OUTPUT 10         x.....x.......x...............x.
OUTPUT 11         x...x...x.x...x...........x.x...
OUTPUT 12         ....x.x...x.x.x...x...x.....x.x.
```
No empty lane in this rhythm; OUTPUT 3 again every odd time.

### AD RHYTHM 6 (SAMBA) — 32 elementary times
```
time:             12345678901234567890123456789012
OUTPUT 1          x...x...x.....x.x.x...x.x.......
OUTPUT 2          ..x...x...xx..x...x...x...xx..x.
OUTPUT 3          x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUTPUT 4          x...x...x.....x.x.x...x.x...x...
OUTPUT 5          ................x.....x.........
OUTPUT 6          ......x.x...x...x...x.x...x...x.
OUTPUT 7          x.......x...........x.........x.
OUTPUT 8          x...x...x...x...x.x...x.x...x.x.
OUTPUT 9          x.x.x.x.x.x.x.xx..x.x.x...x.x.x.
OUTPUT 10         x.x...x.........x.............x.
OUTPUT 11         x...x...x.x.x.x.x...x.....x.x...
OUTPUT 12         ..x.x.x.x...x..xx.x...x.....x.x.
```
OUTPUT 1 and OUTPUT 4 are ALMOST identical (OUTPUT 4 adds a mark at time 29); worth noting
because near-duplicate lanes recur on these masks.

## M254 AD — block 3 (PDF page 143, upper half): rhythms 7-8 + COUNTING CONTROL

Cell statistics: RHYTHM 7 min-mark 0.122 / max-blank 0.013; RHYTHM 8 min-mark 0.137 /
max-blank 0.017; COUNTING CONTROL min-mark 0.167 / max-blank 0.017.

### AD RHYTHM 7 (RUMBA) — 32 elementary times
```
time:             12345678901234567890123456789012
OUTPUT 1          x.....x.....x...x.....x.....x...
OUTPUT 2          x.....x.....x.......x...x.......
OUTPUT 3          x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUTPUT 4          ....x...x.x...x.x.....x...x.x.x.
OUTPUT 5          ................x...........x...
OUTPUT 6          ......x.....x...x.....x.....x...
OUTPUT 7          x...........x.........x.........
OUTPUT 8          ....x...x.x.....x.....x...x.....
OUTPUT 9          x..xx.x...x.x.x...x.x.x.x.x.x.x.
OUTPUT 10         x...x...........x...........x.x.
OUTPUT 11         x..x..x...x...x.x...x...x.x...x.
OUTPUT 12         x..xx.....x.x...x.x...x...x.x...
```

### AD RHYTHM 8 (SLOW ROCK) — 24 elementary times (rows 25-32 greyed across all 12 columns)
```
time:             123456789012345678901234
OUTPUT 1          x.........x.x.........x.
OUTPUT 2          ......x...........x.....
OUTPUT 3          x.x.x...x.x.x.x.x...x.x.
OUTPUT 4          ........................
OUTPUT 5          ............x...........
OUTPUT 6          ......x...x.x.....x...x.
OUTPUT 7          x.........x...........x.
OUTPUT 8          ......x...........x.....
OUTPUT 9          x.x.x.x.x.x...x.x.x.x.x.
OUTPUT 10         x.x.x.......x.......x.x.
OUTPUT 11         x.x...x.x...x...x.x...x.
OUTPUT 12         x...x.x...x.x.x...x.x...
```
OUTPUT 4 WHOLLY EMPTY. OUTPUT 2 and OUTPUT 8 are byte-identical lanes (marks at 7 and 18).

### AD COUNTING CONTROL — the length group, read as printed

The group's twelve columns are labelled vertically "RHYTHM 1" .. "RHYTHM 12" (read by eye from
the header, confirmed on a 1:1 600 dpi crop). The whole 32x12 group contains EXACTLY TWO crosses,
both on count row 24:

```
count 24:  RHYTHM 1 = X      RHYTHM 8 = X      (all other cells in the group empty)
```

Applying the databook's own rule ("If count N is crossed for rhythm X this rhythm will have N
elementary times. If the counting control column for a particular rhythm does not contain a cross
that rhythm will have 32 elementary times"):

| AD rhythm | counting-control cross | length | greyed rows agree? |
|---|---|---|---|
| 1 WALTZ | count 24 | 24 | yes, 25-32 greyed |
| 2 TANGO | none | 32 | yes, none greyed |
| 3 SWING | none | 32 | yes |
| 4 BEAT | none | 32 | yes |
| 5 BOSSA NOVA | none | 32 | yes |
| 6 SAMBA | none | 32 | yes |
| 7 RUMBA | none | 32 | yes |
| 8 SLOW ROCK | count 24 | 24 | yes, 25-32 greyed |

Determined from the counting-control group itself, and independently corroborated twice: by the
greying, and by the databook sentence "in the table 1 the rhythms 1 and 8 ... have 24 elementary
times".

**M254 AD: COMPLETE** (8 rhythms x 12 outputs, plus the counting-control group).

---

# M254 AM (standard content)

Heading read verbatim on PDF page 143: **"M 254 AM (standard)"**. Group headers give the names,
read on the pages: 1 WALTZ, 2 POLKA, 3 TANGO (page 143); 4 BOSSA NOVA, 5 SAMBA, 6 SLOW ROCK and
7 BOOGIE, 8 DISCO + COUNTING CONTROL (page 144, printed 152). This matches the name list in the
task brief exactly.

Unlike the AD table, the AM table carries NO grey shading at all: a short rhythm simply has empty
rows past its length, so here the counting-control group is the ONLY evidence of length.

## M254 AM — block 1 (PDF page 143, lower half)

Cell statistics: RHYTHM 1 min-mark 0.144 / max-blank 0.036; RHYTHM 2 min-mark 0.130 /
max-blank 0.000; RHYTHM 3 min-mark 0.133 / max-blank 0.018.
By-eye verification: RHYTHM 1 rows 1-15, all 12 outputs, re-read from a 1.6x crop of the 600 dpi
render — agrees with the measurement cell-for-cell, including the four empty lanes.

### AM RHYTHM 1 (WALTZ) — 24 elementary times (times 25-32 empty; see counting control)
```
time:             123456789012345678901234
OUTPUT 1          x.....x.....x.....x.....
OUTPUT 2          ........................
OUTPUT 3          ........................
OUTPUT 4          ........................
OUTPUT 5          ..x.x...x.x...x.x...x.x.
OUTPUT 6          x.....x.....x.....x.....
OUTPUT 7          ..x.x...x.x...x.x...x.x.
OUTPUT 8          x.....x.....x.....x.....
OUTPUT 9          ......x...........x.....
OUTPUT 10         ........................
OUTPUT 11         x...........x...........
OUTPUT 12         ..x.x...x.x...x.x...x.x.
```
FOUR WHOLLY EMPTY LANES: OUTPUT 2, 3, 4 and 10. THREE byte-identical lanes: OUTPUT 5 = OUTPUT 7 =
OUTPUT 12. OUTPUT 1 = OUTPUT 6 = OUTPUT 8 as well, so this rhythm uses only 5 distinct patterns
across 12 pins. The period is 6 elementary times (4 bars of 3/4 at 2 subdivisions per beat).

### AM RHYTHM 2 (POLKA) — 32 elementary times
```
time:             12345678901234567890123456789012
OUTPUT 1          x...x...x...x...x...x...x...x...
OUTPUT 2          ................................
OUTPUT 3          ................................
OUTPUT 4          ................................
OUTPUT 5          ..x...x...x...xx..x...x...xxx.x.
OUTPUT 6          x...x...x...x...x...x...x.......
OUTPUT 7          ..x...x...x...x...x...x...x...x.
OUTPUT 8          x...x...x...x...x...x...x...x...
OUTPUT 9          ....x.......x.......x.......x...
OUTPUT 10         ................................
OUTPUT 11         x.......x.......x.......x.......
OUTPUT 12         ..x...x...x...x...x...x...x...x.
```
Same four empty lanes (2, 3, 4, 10). OUTPUT 1 = OUTPUT 8 exactly. OUTPUT 7 = OUTPUT 12 exactly.

### AM RHYTHM 3 (TANGO) — 32 elementary times
```
time:             12345678901234567890123456789012
OUTPUT 1          x...x...x...x...x...x...x...x...
OUTPUT 2          ................................
OUTPUT 3          ................................
OUTPUT 4          ................................
OUTPUT 5          x...x...x...x.xxx...x...x...x.x.
OUTPUT 6          ..............x...............x.
OUTPUT 7          x...x...x...x.x.x...x...x...x.x.
OUTPUT 8          x...............x...............
OUTPUT 9          ........x...x...........x...x...
OUTPUT 10         ....x.......x.......x.......x...
OUTPUT 11         x...........x...x...........x...
OUTPUT 12         x...x...x...x.x.x...x...x...x.x.
```
Same three empty lanes 2, 3, 4 (OUTPUT 10 is used here). OUTPUT 7 = OUTPUT 12 exactly.

## M254 AM — block 2 (PDF page 144, printed 152, upper half)

Cell statistics: RHYTHM 4 min-mark 0.140 / max-blank 0.000; RHYTHM 5 min-mark 0.136 /
max-blank 0.001; RHYTHM 6 min-mark 0.143 / max-blank 0.020.
By-eye verification: the whole of RHYTHM 6 (12 outputs x 32 rows) was re-read from a 1:1 600 dpi
crop and agrees cell-for-cell, including that rows 25-32 are entirely empty.

### AM RHYTHM 4 (BOSSA NOVA) — 32 elementary times
```
time:             12345678901234567890123456789012
OUTPUT 1          x.......x.....x.x.......x.....x.
OUTPUT 2          x.....x.....x.......x.....x.....
OUTPUT 3          ......x.x.............x...x.....
OUTPUT 4          x.............x.x.............x.
OUTPUT 5          ................................
OUTPUT 6          ................................
OUTPUT 7          x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUTPUT 8          x...............x...............
OUTPUT 9          ........x...............x.......
OUTPUT 10         ................................
OUTPUT 11         x...............x...............
OUTPUT 12         x.....x.....x.......x.....x.....
```
THREE WHOLLY EMPTY LANES: OUTPUT 5, 6 and 10. OUTPUT 2 = OUTPUT 12 exactly.
OUTPUT 8 = OUTPUT 11 exactly. OUTPUT 7 = every odd elementary time.

### AM RHYTHM 5 (SAMBA) — 32 elementary times
```
time:             12345678901234567890123456789012
OUTPUT 1          x...x...x...x...x...x...x...x...
OUTPUT 2          x...x..x........x...x..x....x...
OUTPUT 3          ...xx.......x......xx.......x...
OUTPUT 4          x......xxx.....xx......xxx.....x
OUTPUT 5          x.x.x..x.x.xx...x.x.x..x.x.xx...
OUTPUT 6          ..x...x...x...x...x...x...x...x.
OUTPUT 7          xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
OUTPUT 8          x...............x...............
OUTPUT 9          ....x.......x.......x.......x...
OUTPUT 10         ................................
OUTPUT 11         x.......x.......x.......x.......
OUTPUT 12         ..x...x...x...x...x...x...x...x.
```
OUTPUT 7 is marked on ALL 32 elementary times. OUTPUT 10 WHOLLY EMPTY.
OUTPUT 6 = OUTPUT 12 exactly. The busiest rhythm on either mask (103 marks).

### AM RHYTHM 6 (SLOW ROCK) — 24 elementary times (times 25-32 empty; see counting control)
```
time:             123456789012345678901234
OUTPUT 1          x.........x.x.........x.
OUTPUT 2          ........................
OUTPUT 3          ........................
OUTPUT 4          ........................
OUTPUT 5          ......x...........x.....
OUTPUT 6          x.....x.....x.....x.....
OUTPUT 7          x.x.x.x.x.x.x.x.x.x.x.x.
OUTPUT 8          x...........x...........
OUTPUT 9          ............x.........x.
OUTPUT 10         ..........x.......x.....
OUTPUT 11         x.......................
OUTPUT 12         x.x.x.x.x.x.x.x.x.x.x.x.
```
THREE WHOLLY EMPTY LANES: OUTPUT 2, 3, 4. OUTPUT 7 = OUTPUT 12 exactly (every odd time).

## M254 AM — block 3 (PDF page 144, lower half): rhythms 7-8 + COUNTING CONTROL

Cell statistics: RHYTHM 7 min-mark 0.148 / max-blank 0.009; RHYTHM 8 min-mark 0.140 /
max-blank 0.010; COUNTING CONTROL min-mark 0.200 / max-blank 0.040.
By-eye verification: RHYTHM 8 rows 1-16, all 12 outputs, re-read from a 1.6x crop — agrees
cell-for-cell.

### AM RHYTHM 7 (BOOGIE) — 24 elementary times (times 25-32 empty; see counting control)
```
time:             123456789012345678901234
OUTPUT 1          x.....x.....x.....x.....
OUTPUT 2          ........................
OUTPUT 3          ........................
OUTPUT 4          ........................
OUTPUT 5          ...x.....x.....x....xx..
OUTPUT 6          x..x..x..x..x..x..x..x..
OUTPUT 7          ..x..x..x..x..x..x..x..x
OUTPUT 8          x...........x...........
OUTPUT 9          ......x..x..x..x..x.....
OUTPUT 10         ...x........x........x..
OUTPUT 11         x........x.....x........
OUTPUT 12         ..x..x..x..x..x..x..x..x
```
THREE WHOLLY EMPTY LANES: OUTPUT 2, 3, 4. OUTPUT 7 = OUTPUT 12 exactly.
The whole rhythm is built on a period of 3 elementary times (24 = 8 beats of triplets), which is
what makes it a shuffle rather than a straight beat.

### AM RHYTHM 8 (DISCO) — 32 elementary times
```
time:             12345678901234567890123456789012
OUTPUT 1          x...x...x...x...x...x...x...x...
OUTPUT 2          ................................
OUTPUT 3          ...x.......xx......x.......xx...
OUTPUT 4          x.....x.......x.x.....x.......x.
OUTPUT 5          ....x.......x.......x.......x...
OUTPUT 6          ..x...x...x...x...x...x...x...x.
OUTPUT 7          xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
OUTPUT 8          x...............x...............
OUTPUT 9          ....x.x.....x.x.....x.x.....x.x.
OUTPUT 10         ......x.......x.......x.......x.
OUTPUT 11         x.....x.x.....x.x.....x.x.....x.
OUTPUT 12         ..x...x...x...x...x...x...x...x.
```
OUTPUT 7 marked on ALL 32 elementary times. OUTPUT 2 WHOLLY EMPTY.
OUTPUT 6 = OUTPUT 12 exactly.

### AM COUNTING CONTROL — the length group

On THIS page the group's twelve columns are headed vertically "OUTPUT 1" .. "OUTPUT 12", whereas
the AD counting-control group on page 143 heads the same group "RHYTHM 1" .. "RHYTHM 12". That is
a printing inconsistency in the databook; recorded as printed, not corrected. By the prose it is
the rhythm index that is meant.

Exactly THREE crosses in the whole 32x12 group, all on count row 24:
```
count 24:  column 1 = X      column 6 = X      column 7 = X     (all other cells empty)
```

| AM rhythm | counting-control cross | length | corroborating evidence |
|---|---|---|---|
| 1 WALTZ | count 24 | 24 | times 25-32 empty in all 12 lanes |
| 2 POLKA | none | 32 | marks present at time 32 |
| 3 TANGO | none | 32 | marks present at time 32 |
| 4 BOSSA NOVA | none | 32 | marks present at time 32 |
| 5 SAMBA | none | 32 | marks present at time 32 |
| 6 SLOW ROCK | count 24 | 24 | times 25-32 empty in all 12 lanes |
| 7 BOOGIE | count 24 | 24 | times 25-32 empty in all 12 lanes |
| 8 DISCO | none | 32 | marks present at time 32 |

Determined from the counting-control group, corroborated by the emptiness of times 25-32 in
exactly those three rhythms, and by the databook sentence "in the table 2 the rhythms 1, 6 and 7,
have 24 elementary times".

**M254 AM: COMPLETE** (8 rhythms x 12 outputs, plus the counting-control group).

---

# M255: is the RHYTHM-name / OUTPUT-name pairing readable? YES — and here it is

The earlier survey warned not to guess this. It does not have to be guessed: PDF page 146 (printed
154) prints TWO connection diagrams SIDE BY SIDE for exactly this purpose — the generic pinout and
one headed "Standard content configuration / M 255 B1 - AB". Same 16 pins, same order, generic
names on the left diagram and the standard content's names on the right. Read off a 600 dpi render:

| pin | generic pinout | standard content M255B1-AB |
|---|---|---|
| 1 | RESET (overlined) | RESET |
| 2 | TEMPO CONTROL | TEMPO CONTROL |
| 3 | DOWN BEAT | DOWN BEAT |
| 4 | VGG | VGG |
| 5 | VSS | VSS |
| 6 | **RHYTHM 1** | **WALTZ** |
| 7 | **RHYTHM 2** | **BEAT** |
| 8 | **RHYTHM 3** | **SWING** |
| 9 | **OUT 1** | **BASS DRUM / FUNDAMENTAL** (a marker dot precedes FUNDAMENTAL; no footnote text found on the page) |
| 10 | **OUT 2** | **SNARE DRUM** |
| 11 | **OUT 3** | **SHORT CYMBALS** |
| 12 | **OUT 4** | **FIFTH** |
| 13 | **OUT 5** | **CHORD TRIGGER** |
| 14 | **RHYTHM 6** | **TANGO** |
| 15 | **RHYTHM 5** | **LATIN** |
| 16 | **RHYTHM 4** | **COUNTRY WESTERN** |

So, for the M255 B1-AB standard content:

- RHYTHM 1 = WALTZ, RHYTHM 2 = BEAT, RHYTHM 3 = SWING,
  RHYTHM 4 = COUNTRY WESTERN, RHYTHM 5 = LATIN, RHYTHM 6 = TANGO.
- OUTPUT 1 = BASS DRUM (FUNDAMENTAL), OUTPUT 2 = SNARE DRUM, OUTPUT 3 = SHORT CYMBALS,
  OUTPUT 4 = FIFTH, OUTPUT 5 = CHORD TRIGGER.

The one step NOT printed anywhere is the identity of pinout "RHYTHM n" with truth-table column
group "RHYTHM n". Nothing else labels the truth-table groups, and the datasheet uses the one
numbering throughout, but it is an inference rather than a printed statement, so it is flagged as
such. It is strongly corroborated: PDF page 147 (printed 155) says
> "The internal counter has a 16 state (i.e. 16 elementary times) cycle and an internal reset
> signal is generated when the sixteenth state is decoded. Rhythms with a 3/4 time originate the
> internal reset when the 12th state is decoded."

and the three 12-elementary-time rhythms measured in the truth table are exactly 1, 3 and 4 =
WALTZ, SWING (jazz waltz) and COUNTRY WESTERN (country waltz) — the three plausible 3/4 rhythms of
the six, with BEAT / LATIN / TANGO running the full 16. The block diagram on page 146 is even
labelled "SELECTION BETWEEN 3/4 AND 4/4 RHYTHMS".

Also from page 147, on the outputs themselves:
> "The trigger outputs are pulse shaped and their width equals 1/32 of one elementary time. Pulse
> width is proportional to clock period but always remains 1/32 of a beat time."
> "T: Trigger: The output is in the form of a pulse whose width equals 1/32 of one elementary time.
> The pulse can be either positive or negative going according to the option chosen in line 3."

## CAUTION: page 147 carries a DIFFERENT option table, and it is NOT the standard content

Page 147's "PROGRAMMING THE OPTIONS" section introduces its table with "The five outputs of the
M 255 may have different options which must be specified together with the ROM truth table. This
can be done as shown in the table below", and that illustrative table reads:

```
line 1  Continuous or Trigger Output      OUT.1 T   OUT.2 T   OUT.3 C   OUT.4 T   OUT.5 T
line 2  Open drain or Push Pull                 O         O         O         O         O
line 3  Posit. or Negat. Trigger Edge           +         +         +         -         -
```

That is an EXAMPLE of how to fill the form in (note OUT.3 = C and two negative edges), and it
DISAGREES with the standard content on page 150, which is all T / all O / all +. The authoritative
statement of what the M255 B1-AB actually does is the one printed under its own truth table
(page 150, transcribed at the top of this file). Recorded because mistaking the page-147 example
for the shipped mask is precisely the "blank ordering form" trap.

---

# What the M255 mask says once the pin names are applied

Reading the transcription above through the page-146 pin names (OUTPUT 1 = BASS DRUM,
2 = SNARE DRUM, 3 = SHORT CYMBALS, 4 = FIFTH, 5 = CHORD TRIGGER):

- **WALTZ (12)**: BASS DRUM on 1 and 7; FIFTH on 7 only — i.e. the bass alternates root then
  fifth on the two bar-ones. SNARE DRUM and CHORD TRIGGER together on 3, 5, 9, 11: the two
  "pah"s of each bar. That is a literal oom-pah-pah, and it is an independent check on the
  pairing being right.
- **BEAT (16)**: SHORT CYMBALS on all 16 (a continuous hat), BASS DRUM on 1,4,5,9,10,12,13,15,16,
  SNARE on 3,7,11,14,16.
- **SWING (12)**: BASS DRUM 1,4,7,10; SNARE and CHORD TRIGGER both on 4 and 10 (identical lanes).
- **COUNTRY WESTERN (12)**: SHORT CYMBALS on all 12; BASS DRUM 1,6,7,12; SNARE + CHORD TRIGGER
  on 4 and 10.
- **LATIN (16)**: SHORT CYMBALS on all 16, and the only rhythm where all five outputs are busy.
- **TANGO (16)**: the sparsest, marks only at 1, 5, 9, 13 and 15, with BASS DRUM + SNARE + CHORD
  TRIGGER moving together and the whole kit hitting on 15.

# Status / what is done

- **M255 B1-AB: COMPLETE.** 6 rhythms x 5 outputs x 16 rows, lengths verified from the greying,
  the four option rows quoted verbatim, and the pin-name pairing read off the connection diagrams.
- **M254 AD: COMPLETE.** 8 rhythms x 12 outputs x 32 rows + the counting-control group; lengths
  read from the counting control (24 for rhythms 1 and 8, 32 for the rest).
- **M254 AM: COMPLETE.** 8 rhythms x 12 outputs x 32 rows + the counting-control group; lengths
  read from the counting control (24 for rhythms 1, 6 and 7, 32 for the rest).
- Nothing is left UNREAD on the three tables. The only item flagged as an inference rather than a
  reading is the identification of pinout "RHYTHM n" with truth-table group "RHYTHM n" on the M255.

# Recurring structural features (recorded because the brief asked for them)

Byte-identical lanes are common and real, and so are wholly empty ones:

- M255: OUTPUT 2 == OUTPUT 5 in rhythms 1, 3 AND 4; OUTPUT 1 == OUTPUT 3 in rhythm 1; three
  identical lanes (1, 2, 5) in rhythm 6.
- M254 AD: OUTPUT 4 wholly empty in rhythms 1, 2, 3, 4, 8; OUTPUT 5 wholly empty in rhythm 2;
  OUTPUT 2 == OUTPUT 8 in rhythm 8; OUTPUT 1 == OUTPUT 3 in rhythm 1.
- M254 AM: OUTPUTS 2, 3, 4 are empty in FIVE of the eight rhythms, and OUTPUT 10 in three;
  OUTPUT 7 == OUTPUT 12 in rhythms 6 and 7; OUTPUT 6 == OUTPUT 12 in rhythms 5 and 8;
  OUTPUT 1 == OUTPUT 8 in rhythm 2; OUTPUT 2 == OUTPUT 12 and OUTPUT 8 == OUTPUT 11 in rhythm 4.
- A "continuous" lane (marked on EVERY elementary time of the rhythm) appears on both parts:
  M255 OUTPUT 3 is marked on every elementary time in rhythms 2, 4 and 5; M254 AD OUTPUT 3 is
  marked on every ODD elementary time in rhythms 4, 5, 6 and 7 (all 16 of them); M254 AM OUTPUT 7
  on every odd time in rhythms 4 and 6, and on ALL 32 in rhythms 5 and 8.


# ===== source file: chips.md =====

# Rhythm-generator LSI pattern tables: search log

Task: find PRINTED ROM/pattern truth tables for 1970s rhythm-generator LSIs
beyond SGS M252/M253 (already transcribed: masks AA/AD and AA/AC, 76 rhythms).

Verdict vocabulary: CHART READABLE / CHART EXISTS BUT MARGINAL / NO CHART / NOT FOUND

## Log (append after every search)

(nothing yet — starting)

### 2026-08-20 — SGS 1979 databook (LOCAL: sgs_1979_databook.pdf) text-layer sweep
Index p.33-39 lists SEVEN parts in the family, not two:
  M251 P-MOS  Arpeggio chord and bass accompaniment generator   (databook p.113)
  M252 P-MOS  Rhythm generator   (done)
  M253 P-MOS  Rhythm generator   (done)
  M254 P-MOS  Rhythm generator   (databook p.14x)
  M255 P-MOS  Rhythm generator   (databook p.153)
  M258 N-MOS  Rhythm generator   (databook p.1s1 ~ 15x)
  M259 N-MOS  Rhythm generator   (databook p.159)

**M254 — CHART READABLE (candidate #1).** Two masks, both with FULL printed truth tables:
  - ORDERING: M254 B1 AD and M254 B1 AM, "for standard music content".
  - Databook text: "The ROM truth table has been organized in 32 rows which represent
    the elementary times and 104 co[lumns]" ... "The first 8 groups of 12 columns
    represent the rhythms which have 12 programmable outputs." => 8 rhythms x 12
    outputs = 96, + a COUNTING CONTROL group.
  - Reset convention (quoted): "If count N is crossed for rhythm X this rhythm will
    have N elementary times. If the counting control column for a particular rhythm
    does not contain a cross that rhythm will have 32 elementary times."
  - "Table 1 and 2 show the truth tables of the M 254 AD and M 254 AM, standard
    contents". Text also notes: in table 1 rhythms 1 and 8, and in table 2 rhythms
    1, 6 and 7, "have ..." (truncated in OCR — need the page).
  - RHYTHM NAMES ARE PRINTED as sub-captions:
    AD: 1 WALTZ, 2 TANGO, 3 SWING, 4 BEAT, 5 BOSSA NOVA, 6 SAMBA, 7 RUMBA, 8 SLOW ROCK
    AM: 1 WALTZ, 2 POLKA, 3 TANGO, 4 BOSSA NOVA, 5 SAMBA, 6 SLOW ROCK, 7 BOOGIE, 8 DISCO
  - Pin note: one output "must be connected so as to drive the 'snare drum' when the
    rhythms corresponding to pins 9,10,11,12 and 16 are generated, and the 'claves'
    when the rhythms corresponding to pins 13,..." => instruments ARE named per pin.
  => 16 rhythms of NEW data, names printed. Needs page render to judge legibility.

**M255 — CHART READABLE (candidate #2), and SMALL.** One mask:
  - ORDERING: M255 B1-AB "for standard music content". 6 PROGRAMMABLE RHYTHMS.
  - "The ROM truth table has been organized in 16 rows which represent the elementary
    times and 30 co[lumns]" ; caption "TRUTH TABLE of M 255 B1-AB (standard content)"
    with printed sub-headers RHYTHM 1..RHYTHM 6.
  - 16-state counter; "Rhythms with 8 or 6 elementary times are also programmable, in
    which case they are written twice in [the ROM]". Also mentions "some 1x16 or 1x12
    rhythms".
  - Pin names printed on the package diagram: COUNTRY, TANGO, WALTZ, BEAT, SWING
    (rhythm inputs) and CYMBALS, SNARE DRUM, BASS DRUM (outputs) => instruments named
    per pin. Only 30 columns/16 rows = the most transcribable table here.

**M258 / M259 — CHART LIKELY, shared datasheet.** "16 PROGRAMMABLE RHYTHMS (CODED FOR
  THE M258; ALSO AVAILABLE IN COMBI...)". N-channel MOS. Has a section headed
  "INSTRUMENT BEATS VERSUS RHYTHM PROGRAM" with a table whose column headers are
  "TRUTH TABLE | EXTERNAL DEVICE OUTPUT | INSTRUMENT | SYNC. | DOWN[BEAT]" — that is
  a pin/instrument mapping table, need to check for a pattern grid too.

**M251 — arpeggio/chord+bass, not a rhythm pattern ROM.** Ordering M251 B1 AC. Check
  separately; different data kind (chord/bass accompaniment).

### VERIFIED BY RENDER — M255 B1-AB truth table
File: <scratch>/sgs_1979_databook.pdf, **PDF page 150** (printed page number "158").
Rendered chip_m255_p150-150.png at 200dpi and READ it. Embedded scan: 3902x5109
gray 1bpc JBIG2 at **600 dpi** — same quality as the M252/M253 pages.
VERDICT: **CHART READABLE.** Fully populated, crisp, all 6 rhythms carry marks
(so NOT the njohnson-style blank ordering form; caption + body text both say
"standard content" / "the content and the options programmed in the M 255 B1-AB
standard content").
Grid: caption "TRUTH TABLE of M 255 B1-AB (standard content)".
  - Rows: 16, header "Counter state", numbered 1..16 = elementary times.
  - Columns: 6 groups of 5 = 30. Group headers printed "RHYTHM 1".."RHYTHM 6".
    Within each group the 5 columns are labelled vertically OUTPUT 1..OUTPUT 5.
  - A cell mark is an "X" (body text: "programmed by crossing the appropriate box").
  - GREYED cells = past that rhythm's length, same convention as M252/M253.
    Greyed rows 13-16 under RHYTHM 1, and under RHYTHM 3 + RHYTHM 4 => those are
    the 12-elementary-time rhythms; rhythms 2, 5, 6 run all 16.
  - FOUR OPTION ROWS below the grid (new vs M252/M253, worth capturing):
      "Option on the Outputs"        : O1 O2 O3 O4 O5   | right end: 16 (12), 8 (6)
      "Continuous or Trigger Output" : T  T  T  T  T    | right end: "Down beat" X
      "Open drain or push-pull"      : O  O  O  O  O
      "Positive or Negative Trigger Edge": + + + + +
    => outputs are OPEN DRAIN, TRIGGER (not continuous), POSITIVE edge — the same
    rising-edge convention already established for M252/M253, here printed explicitly.
  - Instruments per pin come from the package pinout (page ~145/146): outputs named
    CYMBALS, SNARE DRUM, BASS DRUM; rhythm-select inputs named COUNTRY, TANGO,
    WALTZ, BEAT, SWING (+ a 6th). NOTE: only 5 outputs but 3 named instruments on
    the pinout excerpt — must re-read the pinout page before asserting the full map.
  - Rhythm NAMES for the 6 patterns: the grid itself says only "RHYTHM 1..6"; the
    names must be taken from the pinout (COUNTRY/TANGO/WALTZ/BEAT/SWING/+1) and the
    input-pin order confirmed on p.145-147. DO NOT guess the pairing.
=> READY TO TRANSCRIBE, and small (16x30). Highest-value target.


# ===== source file: chordbass.md =====

# CHORD / BASS accompaniment data: is it published?

Task: find printed TABLES of chord/bass/arpeggio accompaniment content (1970s organs),
the way drum-rhythm truth tables were printed. New data KIND for this project.

Status: IN PROGRESS (append-only log; assume agent may die at any moment)

## Log

### 2026-08-20 T1: SGS M251 FOUND, TRUTH TABLES EXIST
Source on disk: `sgs_1979_databook.pdf` (SGS-ATES 1979 databook), text layer `sgs79.txt`.
M251 section starts printed p.113 ("3/78" = March 1978 datasheet), runs to ~p.120.
- p.113: features list + abs max ratings + ordering numbers (M 251 B1 AC = plastic DIP,
  M 251 D1 AC = ceramic DIP). NOTE: unlike the rhythm chips there is NO "for standard music
  content" suffix here, the two order codes differ only by PACKAGE. => the M251 is a
  GENERIC accompaniment engine; the musical content lives in the EXTERNAL ROM (M252/3/4).
- p.114: mechanical data, connection diagram, BLOCK DIAGRAM (context, not data).
- p.115: **ARPEGGIO TRUTH TABLE (positive logic)** -- TABLE READABLE, 16 rows x
  (SELECT 6th | SELECT 7th) x (ARP I, ARP II, ARP III). Rows addressed by external memory
  code D5 D4 D3 D2 (4 bits = 16 states).
- p.116: **BASS and CHORD TRUTH TABLES (positive logic)**, addressed by D8 D7 D6.
VERDICT: TABLE READABLE.

### T2: FOUR truth tables, two operating modes
The M251 datasheet prints truth tables for BOTH its modes:
- AUTOMATIC mode (chip works out the chord from the LOWEST key held):
  ARPEGGIO truth table (p.115, D5..D2, 16 rows, x2 sub-tables 6th/7th, x3 arp outputs)
  BASS + CHORD truth tables (p.116 top, D8 D7 D6 = 8 rows for bass; D1 = 2 rows for chord)
- SEMIAUTOMATIC mode WITH memorization (chip uses the 4 lowest + top keys ACTUALLY held):
  ARPEGGIO truth table (p.116, same 16-row shape but in KEYS: L, 2nd L, 3rd L, 4th L)
  BASS + CHORD truth tables (p.117 top)
- SEMIAUTOMATIC WITHOUT memorization: "same as the previous one except keys not memorized" (no extra table)
- p.117: EXTERNAL MODE OUTPUTS table (pins 2-5 = 8x tonic / 8x 5th / 8x 3rd / 8x 6th-or-7th)
Next: get clean column alignment with pdftotext -layout for the bit patterns.

## TRANSCRIPTION 1 -- M251 ARPEGGIO TRUTH TABLE, AUTOMATIC mode (printed p.115)
Verified by eye from a 600 dpi render of PDF page 109 (`cb_p115_arpbody*.png`), row +
column headers included. Text layer and eye agree, including the three oddities noted below.

Address = external memory code D5 D4 D3 D2 (16 states, from the M252/3/4 ROM).
Three simultaneous outputs ARP.I / ARP.II / ARP.III. Two variants chosen by the panel
"6th or 7th" switch. "-" = that output is silent on this step. TONIC = selected key / 16.

D5 D4 D3 D2 | 6th: ARP.I    ARP.II     ARP.III   | 7th: ARP.I    ARP.II     ARP.III
1 1 1 1     | TONIC         3rd        5th       | TONIC         3rd        5th
1 1 1 0     | 3rd           5th        TONIC x2  | 3rd           5th        7th
1 1 0 1     | 5th           TONIC x2   3rd x2    | 5th           7th        3rd x2
1 1 0 0     | 6th           -          -         | 7th           -          -
1 0 1 1     | TONIC x2      3rd x2     5th x2    | 7th   (*1)    3rd x2     5th x2
1 0 1 0     | 3rd x2        5th x2     TONIC x4  | 3rd x2        5th x2     TONIC x4
1 0 0 1     | 5th x2        TONIC x4   3rd x4    | 5th x2        TONIC x4   3rd x4
1 0 0 0     | 6th x2        -          -         | 7th x2        -          -
0 1 1 1     | TONIC x4      3rd x4     5th x4    | TONIC x4      3rd x4     5th x4
0 1 1 0     | 3rd x4        5th x4     TONIC x8  | 3rd x4        5th x4     7th x4
0 1 0 1     | 5th x4        TONIC x8   3rd x8    | 5th x4        7th x4     3rd x8
0 1 0 0     | 6th x4        -          -         | 7th x4        -          -
0 0 1 1     | TONIC x8      3rd x8     5th x8    | 7th x4 (*2)   3rd x8     5th x8
0 0 1 0     | 3rd x8        5th x8     TONIC x8  | 3rd x8        5th x8     7th x8
0 0 0 1     | 5th x8        TONIC x8   3rd x8    | 5th x8        6th x8(*3) 3rd x8
0 0 0 0     | No Change     No Change  No Change | No Change     No Change  No Change

Printed note: "TONIC is the input note, corresponding to the selected key, divided by 16.
3rd is the correct third corresponding to this TONIC. And so on."
(*1)(*2)(*3) = printed as shown but break the otherwise regular pattern (one would expect
TONIC x2, TONIC x8 and 7th x8). Probable datasheet typos; recorded as PRINTED, verified twice
against the 600 dpi crop. Do not "fix" silently.

## TRANSCRIPTION 2 -- M251 BASS + CHORD, AUTOMATIC mode (printed p.116 top)
AUTOMATIC BASS, address D8 D7 D6:
 1 1 1  2nd /2
 1 1 0  8ve /2
 1 0 1  9th /2
 1 0 0  6th or 7th /2
 0 1 1  5th /2
 0 1 0  3rd /2
 0 0 1  TONIC /2
 0 0 0  NO CHANGE
ALTERNATE BASS, address D7 D6 (2 bits only):
 1 1  -            (silent)
 1 0  TONIC /2
 0 1  5th /2
 0 0  NO CHANGE
CHORD, address D1 (ONE bit -- the chord is a strum GATE, not a note select):
 1  SELECT 6th: TONIC+3rd+5th   |  SELECT 7th: TONIC+3rd+5th+7th
 0  NO CHANGE                   |  NO CHANGE
"NO CHANGE" = sustain the previous notes until new information is presented.

## TRANSCRIPTION 3 -- M251 ARPEGGIO TRUTH TABLE, SEMIAUTOMATIC mode (printed p.116)
Same 4-bit address, but the outputs name the KEYS THE PLAYER HOLDS, not chord degrees:
L = lowest key held, 2nd L / 3rd L / 4th L = next ones up, Top = highest key held.
"L in the first octave to the left represents corresponding input note divided by 16,
while in the second octave it is divided by 8."
Cols verified by eye (ARP.I, ARP.II) at 600 dpi, `cb_p116_semiarp.png`; ARP.III from the
text layer, which matched the two verified columns cell-for-cell.

D5 D4 D3 D2 | ARP.I      ARP.II     ARP.III
1 1 1 1     | L          2nd L      3rd L
1 1 1 0     | 2nd L      3rd L      L x2
1 1 0 1     | 3rd L      L x2       2nd L x2
1 1 0 0     | 4th L      -          -
1 0 1 1     | L x2       2nd L x2   3rd L x2
1 0 1 0     | 2nd L x2   3rd L x2   L x4
1 0 0 1     | 3rd L x2   L x4       2nd L x4
1 0 0 0     | 4th L x2   -          -
0 1 1 1     | L x4       2nd L x4   3rd L x4
0 1 1 0     | 2nd L x4   3rd L x4   L x8
0 1 0 1     | 3rd L x4   L x8       2nd L x8
0 1 0 0     | 4th L x4   -          -
0 0 1 1     | L x8       2nd L x8   3rd L x8
0 0 1 0     | 2nd L x8   3rd L x8   L x8
0 0 0 1     | 3rd L x8   L x8       2nd L x8
0 0 0 0     | NO CHANGE  NO CHANGE  NO CHANGE
(the last three rows saturate at x8 -- the divider chain runs out, printed as shown)

## TRANSCRIPTION 4 -- M251 BASS + CHORD, SEMIAUTOMATIC mode (printed p.117 top)
AUTOMATIC BASS OUTPUT / ALTERNATE BASS OUTPUT, address D8 D7 D6:
 1 1 1  TWO 8ve BELOW TOP      | -
 1 1 0  L                      | -
 1 0 1  ONE 8ve BELOW TOP      | -
 1 0 0  ONE 8ve BELOW 4th L    | -
 0 1 1  ONE 8ve BELOW 3rd L    | -
 0 1 0  ONE 8ve BELOW 2nd L    | ONE 8ve BELOW L
 0 0 1  ONE 8ve BELOW L        | ONE 8ve BELOW TOP
 0 0 0  NO CHANGE              | NO CHANGE
CHORD OUTPUT, address D1:
 1  L + 2nd L + 3rd L + 4th L   (i.e. the held keys themselves, no interval theory)
 0  NO CHANGE

## EXTERNAL MODE (printed p.117) -- context, not accompaniment data
Pins 2/3/4/5 expose raw top-octave frequencies so a designer can build accompaniments the
M251 does not itself produce:
 pin 2  8x TONIC                          | semiauto: 8x L
 pin 3  8x FIFTH or DIMINISHED FIFTH      | semiauto: 8x 3rd L
 pin 4  8x MAJOR THIRD or MINOR THIRD     | semiauto: 8x 2nd L
 pin 5  8x SIXTH or SEVENTH               | semiauto: 8x 4th L

## THE ARCHITECTURE ANSWER (M251 -- how it knows the key, and who clocks it)
1. HOW IT KNOWS THE CHORD: from the KEYS THEMSELVES, not from a chord code.
   - AUTOMATIC mode: "the lowest key is taken as a reference ... this note is memorized
     internally" (p.115). That lowest key = TONIC; the chip's own interval logic derives
     3rd/5th/6th/7th from it. Panel switches choose major/minor 3rd, fifth/dim fifth,
     sixth/seventh. Two octaves of 12 keys are multiplexed in (p.115 "GENERAL CHARACTERISTICS"
     item b: 12 keyboard inputs, two octaves multiplexed).
   - SEMIAUTOMATIC mode (with or without memorization): "an internal recognition circuit
     which selects the lowest four keys, the top key played" (p.116). Outputs are then those
     literal keys (L, 2nd L, 3rd L, 4th L, Top) -- so the player supplies the voicing and the
     chip supplies only the RHYTHM of the arpeggio. There is no key/chord code input pin.
   - Reset: interrupt the "automatic" line for a moment with no keys down.
2. WHO SUPPLIES THE PATTERN / THE CLOCK: an external self-scanning rhythm ROM.
   p.115: "M 251 is normally used in conjunction with an external self-scanning ROM (such as
   the M 252 - 3 or 4) which performs the selection of the various notes in the
   arpeggio/chord/bass accompaniment."
   The M251 has "4 multiplexed data inputs for addressing the internal selection circuits.
   These inputs are normally coming from the outputs of an external memory" -- so D1..D8 are
   carried on 4 pins in two multiplex phases, keyed off F24 (an anti-phase pair derived from
   the top note of the upper octave, pin 32). Trigger pulse width TDA/TDB = one period of the
   external memory clock line. So the M251 is SLAVED to the rhythm ROM's clock; it has no
   tempo of its own.
3. => THE MUSICAL CONTENT (which step plays which degree) IS NOT IN THE M251. The M251 is a
   generic note/interval engine; the PATTERN lives in the rhythm ROM. Its two ordering codes
   differ only by package -- there is no "standard music content" M251, consistent with this.
4. THE ROM SIDE IS ALSO PRINTED, but only partially for accompaniment:
   - M254 (p.145-152) prints Table 1 (M 254 AD standard content) and Table 2 (M 254 AM),
     32 rows x 12 outputs per rhythm x 8 rhythms, as cross-mark grids -- printed p.150-152.
   - Footnote p.146: "For this application a version of the M 254 with standard memory content
     is available both for interfacing with the M 251 and for driving 4 instrument simulators
     (8 rhythms). Ordering number is M 254 AD."
   - p.146 footnote: "12 to 15 drive the corresponding inputs of the M 251."
   - The M 254 AD connection diagram (p.145) labels one output "TRIGGER CHORDS" and others
     BASS DRUM / BASS ALTERNATE / SNARE / CLAVES / CYMBALS / LOW BONGO / SHORT ...
   => So SOME accompaniment columns (a chord trigger, bass switching) ARE in the printed
   M254 AD table, but they are 4 lines, not the full 8-bit D1..D8 address. NEXT: nail down
   exactly which M254 AD output columns go to the M251, and check the M251 typical-application
   circuit (p.118-120) and the M105 reference ("bass switching inputs A, B, C of the M 105").

### T3: DEFINITIVE -- where the M251's pattern data comes from (printed p.120, TYPICAL APPLICATION)
Read by eye at 600 dpi (`cb_p120_mem.png`). The figure contains a block drawn as:
  "M 252 / M 253 / M 254*  EXTERNAL MEMORY"
with EIGHT outputs O1 O2 O3 O4 O5 O6 O7 O8 leaving it. O2..O8 are diode-OR'd in
15k/150k pairs into M251 pins 2, 4, 6, 7 (the 4 multiplexed data pins); O1 goes straight
across (the CHORD bit, which the truth table addresses with D1 alone). A LATCH / /LATCH
switch sits on the same node.
=> The M251's D1..D8 ARE eight output columns of an ordinary SGS rhythm ROM -- the SAME
chip family whose 32-row drum truth tables this project already transcribed. The chord/bass
CONTENT is therefore published exactly where the drum content is: in the rhythm ROM's
truth table, in the columns that were wired to the M251 instead of to a drum.

### T4: THE JACKPOT -- M254 B1AD pin-out (printed p.145, read by eye at 600 dpi, `cb_p145_ad.png`)
"M 254 B1AD Standard content configuration" -- the version the p.146 footnote says exists
"for interfacing with the M 251". Its 12 outputs are:
  pin 3  BASS DRUM / BASS ALTERNATE      <- drum
  pin 4  SNARE DRUM OR CLAVES (*)        <- drum
  pin 5  SHORT CYMBALS                   <- drum
  pin 6  LOW BONGO                       <- drum
  pin 7  I 8      \
  pin 8  I 7       |
  pin 17 I 6       |  SEVEN lines straight into the M251's data inputs
  pin 19 I 5       |  (footnote p.146: "I2 to I8 drive the corresponding inputs of the M 251")
  pin 20 I 4       |
  pin 21 I 3       |
  pin 22 I 2      /
  pin 18 TRIGGER CHORDS   <- this is I1 / D1, the CHORD gate of the M251 chord truth table
Rhythm select pins: 9 WALTZ, 10 TANGO, 11 SWING, 12 BEAT, 13 BOSSA NOVA, 14 SAMBA,
15 RUMBA, 16 SLOW ROCK.
Footnote **: the drum outputs also "drive the bass switching inputs A, B, C of the M 108".

**CONCLUSION: YES, the chord/bass/arpeggio content WAS published as a table.**
`M 254 AD (standard)` Table 1, printed pp.150-152 of the SGS 1979 databook, is a 32-row x
12-column cross-mark grid per rhythm for 8 rhythms -- and EIGHT of those twelve columns are
the M251 address D1..D8. Decode a row's 8 bits through the M251 truth tables (Transcriptions
1/2 above) and you have the literal arpeggio step, chord stab and bass note for that
elementary time, for WALTZ / TANGO / SWING / BEAT / BOSSA NOVA / SAMBA / RUMBA / SLOW ROCK.
(Rhythms 1 and 8 have 24 elementary times, not 32 -- stated p.149.)
NEXT: confirm Table 1 is legible at 600 dpi and that the 8 accompaniment columns are labelled.

### T5: OUTPUT-COLUMN -> M251 ADDRESS-BIT MAP (the key to reading Table 1 as music)
Generic M254 pin/output map, read by eye at 600 dpi (`cb_p145_plain.png`):
pins 3,4,5,6,7,8 = OUTPUT 1,2,3,4,5,6 ; pins 22,21,20,19,18,17 = OUTPUT 12,11,10,9,8,7.
Overlaying the M254 B1AD labels (`cb_p145_ad.png`) gives, for the M 254 AD standard content:
  OUTPUT 1  (pin 3)  = BASS DRUM / BASS ALTERNATE   (drum)
  OUTPUT 2  (pin 4)  = SNARE DRUM or CLAVES         (drum; snare on rhythms 1-4+8, claves 5-7)
  OUTPUT 3  (pin 5)  = SHORT CYMBALS                (drum)
  OUTPUT 4  (pin 6)  = LOW BONGO                    (drum)
  OUTPUT 5  (pin 7)  = I 8  -> M251 D8
  OUTPUT 6  (pin 8)  = I 7  -> M251 D7
  OUTPUT 7  (pin 17) = I 6  -> M251 D6
  OUTPUT 8  (pin 18) = TRIGGER CHORDS -> M251 D1 (the chord gate)
  OUTPUT 9  (pin 19) = I 5  -> M251 D5
  OUTPUT 10 (pin 20) = I 4  -> M251 D4
  OUTPUT 11 (pin 21) = I 3  -> M251 D3
  OUTPUT 12 (pin 22) = I 2  -> M251 D2
So in Table 1, per elementary time:
  ARPEGGIO step  = (OUT5,OUT9,OUT10,OUT11,OUT12) ... precisely D5 D4 D3 D2 = OUT9,OUT10,OUT11,OUT12
  BASS note      = D8 D7 D6 = OUT5, OUT6, OUT7
  CHORD stab     = D1       = OUT8
  Drums          = OUT1..OUT4
(Note the crosses are drawn "positive logic" as in the drum tables the project already has;
D-bit ordering above is taken straight from the pin labels, so it needs no interpretation.)

### T6: LEGIBILITY of Table 1 / Table 2 -- CONFIRMED
Rendered PDF page 143 = printed p.151 at 300 dpi (`cb_p150_small.png`): fully legible.
It carries M254 AD RHYTHM 7 (RUMBA), RHYTHM 8 (SLOW ROCK), the COUNTING CONTROL block, and
then the start of Table 2, M 254 AM (standard) RHYTHM 1 (WALTZ) / 2 (POLKA) / 3 (TANGO).
Column headers are printed vertically as "OUTPUT 1".."OUTPUT 12"; rows numbered 1..32 with
a "COUNT FOR 32" stub; the greyed block on rhythm 8 = "past this rhythm's length"
(rhythms 1 and 8 have 24 elementary times, p.149).
=> M254 AD rhythms 1-6 are on printed p.150 (PDF page 142); rhythms 7-8 + counting control on
printed p.151 (PDF 143). VERDICT: TABLE READABLE. A full transcription is 8 rhythms x 32 rows
x 8 accompaniment columns = ~2048 cells and is a separate job; the grid is clean enough for
the ink-fraction method already used on the drum chips.

## TRANSCRIPTION 5 -- DEMONSTRATION: M 254 AD Table 1, RHYTHM 1 (WALTZ), printed p.150
Read by eye at 600 dpi from PDF page 142, in two overlapping crops that each carry the row
numbers and the column headers (`cb_ad_r1b.png` = OUT1..OUT10, `cb_r1_last.png` = OUT9..OUT12
with OUT9/OUT10 as the cross-check overlap; the overlap agreed on all 12 marked rows).
Even-numbered elementary times are entirely blank in this rhythm; rows 25-32 are GREYED
(past this rhythm's length -- WALTZ is 24 elementary times, p.149).

RAW GRID (crosses only, odd rows; every even row and row 24 are empty):
 t | OUT: 1  2  3  4  5  6  7  8  9 10 11 12
  1 |      X  .  X  .  .  .  X  .  X  X  X  .
  3 |      .  .  .  .  .  .  .  .  X  X  .  X
  5 |      .  X  .  .  .  X  .  X  X  .  X  X
  7 |      .  .  .  .  .  .  .  .  X  .  X  .
  9 |      .  X  .  .  .  X  X  X  X  .  .  X
 11 |      .  .  .  .  .  .  .  .  .  X  X  X
 13 |      X  .  X  .  X  X  .  .  .  X  X  .
 15 |      .  .  .  .  .  .  .  .  .  X  X  X
 17 |      .  X  .  .  .  X  X  X  X  .  .  X
 19 |      .  .  .  .  .  .  .  .  X  .  X  .
 21 |      .  X  .  .  .  X  .  X  X  .  X  X
 23 |      .  .  .  .  .  .  .  .  X  X  .  X
 16 |      .  X  .  .  .  .  .  .  .  .  .  .   <- the ONE even-row mark (snare, t=16)

DECODED through the M251 automatic-mode truth tables (D5D4D3D2 = OUT9,OUT10,OUT11,OUT12;
D8D7D6 = OUT5,OUT6,OUT7; D1 = OUT8), "6th" variant, drums = OUT1..OUT4:
 t  | arp code | ARP I / II / III            | bass code | BASS      | chord | drums
  1 | 1110     | 3rd / 5th / TONICx2         | 001       | TONIC/2   | -     | bass drum + cymbals
  3 | 1101     | 5th / TONICx2 / 3rdx2       | 000       | (hold)    | -     |
  5 | 1011     | TONICx2 / 3rdx2 / 5thx2     | 010       | 3rd/2     | STAB  | snare
  7 | 1010     | 3rdx2 / 5thx2 / TONICx4     | 000       | (hold)    | -     |
  9 | 1001     | 5thx2 / TONICx4 / 3rdx4     | 011       | 5th/2     | STAB  | snare
 11 | 0111     | TONICx4 / 3rdx4 / 5thx4     | 000       | (hold)    | -     |
 13 | 0110     | 3rdx4 / 5thx4 / TONICx8     | 110       | 8ve/2     | -     | bass drum + cymbals
 15 | 0111     | TONICx4 / 3rdx4 / 5thx4     | 000       | (hold)    | -     |
 16 |  -       | (hold)                      | -         | (hold)    | -     | snare
 17 | 1001     | 5thx2 / TONICx4 / 3rdx4     | 011       | 5th/2     | STAB  | snare
 19 | 1010     | 3rdx2 / 5thx2 / TONICx4     | 000       | (hold)    | -     |
 21 | 1011     | TONICx2 / 3rdx2 / 5thx2     | 010       | 3rd/2     | STAB  | snare
 23 | 1101     | 5th / TONICx2 / 3rdx2       | 000       | (hold)    | -     |

SANITY CHECK (this is why I believe the decode chain): 24 elementary times with events every
4 = six quarter notes = two bars of 3/4, and what comes out is literally the oom-pah-pah:
  bar 1: TONIC bass | chord stab | chord stab
  bar 2: OCTAVE bass | chord stab | chord stab
with an independent 12-step rising-then-falling arpeggio over the top. The chord stabs land on
beats 2 and 3 of each bar and never on beat 1, and the bass alternates root/octave between
bars. Nothing about that fell out of my assumptions -- it fell out of the crosses.

## TARGET 2 -- other makers. Patent candidates (to be checked one by one)
Search 1 (Google Patents, "automatic chord bass accompaniment electronic organ"):
- US3918341A  Automatic chord and rhythm system for electronic organ (+ reissue USRE29144E)
- US4065993A  three-finger chord / one-finger automatic chord mode selector
- US4520707A  microprocessor controlled rhythmic note pattern generation
- CA1143190A  Automatic control apparatus for chords and sequences
- US5216188A / US4887503A  (1980s-90s, later than the window but may print tables)

| document | maker / year | what its figures show | VERDICT |
|---|---|---|---|
| US3918341A (+ USRE29144E) "Automatic chord and rhythm system for electronic organ" | D. H. Baldwin Co., 1975 | 15 figures, all block diagrams / circuit schematics / timing waveforms (FIG.12a = decay rates). Describes a ROM-based rhythm+chord system and names its rhythms (Old Time Waltz, Rhumba, Pop Rock, Ragtime, Swing, Dixieland, Latin III, March) but prints NO row=time / column=output pattern chart. | NO DATA |
| US4237764A "Electronic musical instruments" | Nippon Gakki (Yamaha), filed 1977, granted 1980 | 25 figures, all circuits/waveforms; subject is glissando/portamento, not accompaniment. No rhythm names, no ROM table. | NO DATA |
| US4520707A "Electronic organ having microprocessor controlled rhythmic note pattern generation" | filed early 1980s | FIG.1-7 block diagrams/flowcharts. DOES print the microprocessor program at the end of the spec, but as a raw HEX listing ("0000=1A 77 56 20 3F 50 0B 7F 5E 8F FE 40 24 F8 25 1F...") with no key, no note names, no rhythm names. The content is physically present but not readable as music without the CPU and the data format. | FIGURE BUT UNREADABLE (hex dump, no decoder) |
| US3706837A "Automatic Rhythmic Chording Unit" | Wurlitzer Co, filed 1971, granted 1972 | Inventors Arsem/Schwartz/Ippolito. FIG.4 = a diode matrix for chord switching (Bb F C G D A E, major/minor/7th). Rhythms named waltz/swing/Latin/march. Alternation of root and fifth described in PROSE ("first beat for 3/4; first and third for 4/4"), never charted. | NO DATA (prose only) |
| CA1143190A = US4292874 "Automatic control apparatus for chords and sequences" | (US equivalent granted 1981) | STRONG LEAD. Enumerates SIXTEEN named automatic bass rhythm patterns: Bossa Nova, Tango, Swing, Teen Beat, Shuffle, Waltz, Pop Rock, March, Soul Rock, Rhumba Beguine, Fox Trot, Polka March, Bolero, Samba (+2). **TABLE 1 "Activity Next Bits"** = per-SIXTEENTH-beat trigger flags, i.e. WHEN a bass note fires, for the patterns -- that is the rhythm half of the content, printed. **FIG. 14** caption: "illustrates the data provided by the preferred embodiment of the present invention for the Soul Rock Rhythm Fancy pedal pattern when the G20 pedal (Normal Organ mode) or the G32 key (Easy Play mode) is played" -- a per-pattern DATA chart for one rhythm. The rest lives in ROM as TCMIY instructions, not printed. | FIGURE BUT UNREAD (text layer only; FIG.14 + TABLE 1 need the drawing sheets) |
| US5085118A auto-accompaniment with auto-chord progression | 1990s | prose says rhythm/bass/chord patterns are read from an "accompaniment pattern ROM" but too late a period and no printed content seen | NOT CHECKED IN DETAIL |

### T7: A SECOND SGS CHIP WITH PUBLISHED BASS CONTENT -- M108 (printed pp.55-72)
"M108  N channel MOS  Single chip organ (solo + accompaniment)", printed p.55.
p.61 prints **BASS TRUTH TABLES** in BOTH negative and positive logic. 3-bit external
memory code C B A (from a rhythm ROM again) -> the bass degree:
 CBA (POSITIVE LOGIC) | Bass Arpeggio Output (Automatic) | Alternate Bass Output (Manual)
 0 0 0                | No change                        | No change
 0 0 1                | Root                             | 1st on the left
 0 1 0                | 3rd                              | ---
 0 1 1                | 4th                              | ---
 1 0 0                | 5th                              | 1st on the right
 1 0 1                | 6th                              | ---
 1 1 0                | 7th                              | ---
 1 1 1                | 8th                              | ---
(negative-logic version is the exact complement, also printed)
Prose p.58: "The chip recognizes in the 'ACC.' section only the first on the left of the keys
pressed and ... produces a major or minor chord with or without seventh only the 4' footage
but with separated outputs for root, third, fifth and eighth (or seventh if the chord is with
seventh). The bass section gives the bass arpeggio among root, third, fourth, fifth, sixth,
seventh and eighth with pitch switching dependent on an external ROM (3 bits)."
=> Same architecture as the M251: the chip owns the INTERVALS, the rhythm ROM owns the
PATTERN. And the M254 footnote ** says its outputs "drive the bass switching inputs A, B, C
of the M 108" -- so three more columns of the published M254 tables are bass content.
NOTE the M108's bass arpeggio spans root/3rd/4th/5th/6th/7th/8th -- it includes a FOURTH,
which the M251 does not. Two different degree vocabularies, both printed.

### T8: A THIRD SGS TABLE WITH ACCOMPANIMENT COLUMNS -- M255 B1-AB (printed pp.153-158)
"M 255 B1AB for standard music content". Connection diagram p.153, standard content
configuration, five outputs:
  OUT1 BASS DRUM (FUNDAMENTAL) | OUT2 SNARE DRUM | OUT3 SHORT CYMBALS
  OUT4 **FIFTH**               | OUT5 **CHORD TRIGGER**
Rhythms (6): WALTZ, BEAT, SWING, TANGO, LATIN, COUNTRY WESTERN.
p.157-158: "TRUTH TABLE of M 255 B1-AB (standard content)" -- "16 rows which represent the
elementary times and 30 columns (6 groups of 5)", plus the per-output option rows
(continuous/trigger, open drain/push-pull, trigger edge) and the down-beat/reset count.
=> OUT4 + OUT5 are an alternating-bass "FIFTH" line and a chord-stab gate, published for six
rhythms. Coarser than the M254 AD (root/fifth alternation only, no arpeggio degrees) but it is
accompaniment content, printed as a table, at 16 elementary times per rhythm.
VERDICT: TABLE READABLE (text layer is messy; needs the 600 dpi eye pass, same as the others).
| US3708604A "Electronic organ with rhythmic accompaniment and bass" | Jasper Electronics Manufacturing Corp (Hebeisen, Tevault), filed 1971, granted 1973 | **LEAD.** FIG.5 caption: "shows a typical operating condition ... by illustrating the control pulses in the system and the periods during which pedal tones and chords sound" -- lanes 200-218 over time, i.e. the same KIND of figure as Hammond's US3567838 FIG.2. FIG.4 = "a typical set of connections" for one F-major chord (F,A,C at two octaves) + pedal notes. No rhythm names, no ROM/diode content chart. | FIGURE BUT UNREAD (lanes are unnamed intervals; needs the drawing sheet to judge) |
| US4187756A "Automatic arpeggiator" | (late 1970s) | FIG.1-4 block diagram + schematics. Mentions a "nineteen line by twelve bit read only memory (ROM) 28" holding chord definitions but prints none of it. | NO DATA |
| US4018122A "Electronic musical instrument with automatic bass accompaniment" | | Table A = 32 chord signals (12 major, 12 minor, 4 augmented, 3 dim-7th, 1 zero) with 5-bit codes -- a chord RECOGNITION table, not pattern content. Table C = frequency-division ratios. Diode matrices in FIG.4a/5/6 explicitly drawn incomplete ("connections to the remaining outputs are omitted for the sake of clarity"). | NO DATA (chord vocabulary only, no time axis) |
| US5085118A, US4887503A, US5216188A, US4619176, US4864907, US4292874(US), US3844192A, US3854366A, US3979989A, US4263828A, US4476764A, US4301704A, US5056401A, JPS5355018A, DE2542837A1 | various, mostly 1980s-90s Yamaha/Casio | surfaced by search, not opened | NOT CHECKED |
| SGS M252 / M253 datasheets (printed pp.120-144) | SGS-ATES 1978-79 | drum-only output labels in the variants shown; the M251 typical application names them as candidate external memories but the databook does not print an M252/M253 standard content whose outputs are LABELLED as M251 inputs | NO ACCOMPANIMENT LABELS (their tables are the drum data this project already has) |
| SGS M258 / M259 (printed pp.157-162) | SGS-ATES | 16 programmable rhythms; not examined for accompaniment-labelled outputs | NOT CHECKED |

## ANSWER, IN ONE PARAGRAPH
Yes. The chord/bass/arpeggio half of 1970s organ auto-accompaniment WAS published as tables,
in exactly the same place and the same form as the drum half: the SGS-ATES databook. It takes
TWO tables to read, because the design splits the job. (1) The M251 "Arpeggio chord and bass
accompaniment generator" prints FOUR truth tables (pp.115-117) mapping an 8-bit external
memory code to musical MEANING -- three simultaneous arpeggio voices, a bass note and a chord
stab, expressed as chord DEGREES (tonic/3rd/5th/6th/7th/9th/octave) with octave multipliers.
It has no pattern memory and no tempo of its own; it does not even have a chord input, it
watches the LOWEST KEY the player holds and derives the intervals itself. (2) The pattern -- 
which code arrives on which elementary time -- lives in the rhythm ROM, and the databook
prints that too: M254 B1AD standard content (Table 1, pp.150-151) dedicates 8 of its 12
output columns to the M251's D1..D8 and only 4 to drums, over 32 elementary times x 8 rhythms
(WALTZ TANGO SWING BEAT BOSSA-NOVA SAMBA RUMBA SLOW-ROCK). M255 B1-AB (pp.157-158) does a
coarser version (a FIFTH line + a CHORD TRIGGER) over 16 times x 6 rhythms. And the M108
single-chip organ prints its own 3-bit BASS truth table (p.61) with a seven-degree bass
arpeggio vocabulary that includes a FOURTH. Cross-referencing the two halves gives literal,
readable accompaniment content -- I decoded WALTZ end to end as a check and it came out as an
oom-pah-pah with an alternating root/octave bass and a 12-step arpeggio, which is not
something I put in. So: STOP LOOKING for a separate "chord pattern book"; the data has been
sitting in the same databook as the drum patterns the whole time, one column-group over.
For OTHER makers the picture is the opposite: of the eight patents opened, none prints
beat-by-beat note content. Patent figures in this field are block diagrams, schematics and
timing waveforms; where the content exists it is an undocumented hex ROM dump (US4520707A).
The two exceptions worth chasing are US4292874/CA1143190A (TABLE 1 "Activity Next Bits" =
per-sixteenth trigger flags across sixteen NAMED bass rhythms, + FIG.14, the Soul Rock Fancy
pedal pattern data) and US3708604A FIG.5 (lanes showing "the periods during which pedal tones
and chords sound"), plus the US3567838 the main session already has.

## WHERE I DID NOT LOOK
- The M254 AD Table 1 accompaniment columns for rhythms 2-8 (TANGO SWING BEAT BOSSA NOVA SAMBA
  RUMBA SLOW ROCK). Confirmed legible at 600 dpi on PDF pages 142-143; ~2048 cells; WALTZ is
  transcribed above as the method demo. This is the single highest-value remaining job.
- The M255 B1-AB truth table (OUT4 FIFTH + OUT5 CHORD TRIGGER, 16 times x 6 rhythms), printed
  pp.157-158. Small: 192 cells. Not transcribed.
- Table 2, M 254 AM standard content -- I did NOT verify whether the AM variant has any
  accompaniment-labelled outputs (its connection diagram lists TRIGGER CHORDS too in the text
  layer, so it probably does; unconfirmed).
- SGS M258 / M259 (pp.157-162) and M252 / M253 standard-content variants: not checked for
  accompaniment-labelled outputs. There may be more AD-style variants.
- SGS "Technical Note No 131" (cited on the M252 page as holding the full information) -- not
  on disk, not searched for.
- The M251's ELECTRICAL/timing pages (pp.117-119) beyond the tables; the multiplex phasing
  detail (how D1..D8 are split across the 4 pins in the two F24 phases) is described in prose
  but I did not derive the exact bit-to-phase assignment, which a faithful emulation needs.
- Any patent drawing SHEET. Everything above is from Google Patents text/description; I never
  fetched a figure image, so every "FIGURE BUT UNREAD" verdict is provisional.
- Non-patent, non-datasheet routes entirely: service manuals, Service Data sheets, Elektor /
  Radio-Electronics / Funkschau construction articles, and the Japanese makers' own
  service literature. Any of these could carry the same content for Yamaha / Ace / Farfisa.
- Roland/Ace Tone specifically: no patent found under those names in the searches run.


# ===== source file: patents2.md =====

# patents2.md — bass/chord pattern content extraction
Started 2026-08-19. Agent: patent drawing reader (task 2).
Targets:
 T1 (priority) US 4,292,874 "Automatic control apparatus for chords and sequences" (+ CA1143190A sibling)
 T2 US 3,708,604 (Jasper Electronics, 1973), FIG. 5
Rule: never write a pattern/note/spec not actually read. Log after every step.

## LOG
- [step 0] scratch file created.
- [step 1] Got US4292874A google patents HTML (902KB) -> pt_874.txt (124119 chars description). PDF url: https://patentimages.storage.googleapis.com/eb/2d/ff/373d2b00c1ba99/US4292874.pdf
- [step 2] Text has "TABLE 1" at byte 85463 and "TABLE 2" at 88495; many "Activity Next" mentions. Pattern names appear ~23535 (Soul Rock Fancy), 30945/31089 (Soul Rock/Bossa).
- [step 3] TABLE 1 transcribed from text (no image needed). It is Activity Next bits for ONE sixteenth beat only (MN=1, CN=3), 28 bits = 14 fancy + 14 plain, per rhythm. TABLE 2 = plain data (4 bits) for that beat; TABLE 3 = fancy data (16 bits, damp+coded PD) for that beat.
  Key quotes: "show which of the rhythms have activity (a trigger or a special damp) during the next sixteenth beat"
  Fancy PD decode: "if the three bits are equal to six, there is no trigger and no value for PD; if the three bits are equal to three, PD is equal to zero; and otherwise PD is equal to the decimal equivalent of the three bits plus five." Fancy PD set {0,5,6,7,9,10,12}; plain PD set {0,7,12}.
  "73 percent of the beats for the various rhythm patterns have no triggers or special damps"
  FIG. 14 = "PT, DO and PD for a typical rhythm pattern (fancy Soul Rock rhythm) and the time relationships among these values and the CN count" -> a per-sixteenth timing chart for ONE pattern. WORTH READING.
- [step 4] SIXTEEN rhythm switches named verbatim (switches 212-242): Swing, Teen Beat, Shuffle, 3/4 Waltz, Pop Rock, 6/8 March, Soul Rock, Rhumba Beguine, Tango, Fox Trot, Bossa Nova, Polka March, Bolero, Samba, Merengue, Cha-Cha.
  FIG.14 caption (from BRIEF DESC): "illustrates the data provided by the preferred embodiment of the present invention for the Soul Rock Rhythm Fancy pedal pattern when the G20 pedal (Normal Organ mode) or the G32 key (Easy Play mode) is played."
- [step 5] Drawing sheets = PDF pages 2..19 (Sheet 1..18 of 18), portrait, NOT rotated, 300dpi bitonal.
  FIG. 14 = Sheet 11 of 18 = PDF PAGE 12. It contains: a MUSICAL STAFF (notes) labelled DOWNBEATS at the top, then lanes DO, PT, MN, CN with a 0..15 numbered ruler (twice = two measures), then PD output lanes (Xn, Xo, X1, X4). THIS IS REAL PATTERN CONTENT.
  Other sheets: 1-6 = circuits, 7 = truth tables, 9 = pedal/keyboard, 10 = keyboard chart, 11(fig) = Easy Play chords table (sheet 7), 12(fig)= pedal signal generation table (sheet 9), 13 = block diagram (sheet 10), 15A-16B = flowcharts (sheets 12-18).
- [step 6] FIG.14 overview read (pt_874_p12_ov.png). Content, top to bottom:
  * musical staff, bass clef, TWO measures, notes; arrows marked DOWNBEATS at measure starts (3 arrows = start m1, start m2, start m3/end)
  * DO  : damp output logic level (square lane)
  * PT  : trigger pulses (10 ms), one per note attack
  * MN  : measure number, low for measure 1, high for measure 2
  * CN  : ruler 0..15 then 0..15 then 0,1 -> SIXTEENTH counts. STEP UNIT = sixteenth beat, 16 per measure.
  * PD OUTPUT: staircase with y-axis labels 0,2,4,6,8,10,12 = frequency deviation in semitones
  * A_N (ruler 0..15 x2), A_O, A_1, A_4 = internal clock/address bits, NOT pattern content
  Next: measure grid + transcribe PT / DO / PD per sixteenth at 600dpi.
- [step 7] FIG.14 geometry at 600dpi (pt_874_p12-12.png, 4640x6816):
  staff lines y=2004,2098,2194,2292,2386 ; DOWNBEATS line y=2538
  DO lane: high y=2691, low y=2796 ; PT lane: baseline y=2994, high ~2890
  MN: low y=3090, high y=3182 ; CN axis line y=3371, digit labels y~3390-3420
  CN RULER: 34 tick/label positions, x = 736 + 96.7*i  (i=0..33), verified: last label at x=3927.
  34 = 0..15 (measure 1) + 0..15 (measure 2) + 0,1 (start of measure 3). Ticks confirmed at same spacing.
- [step 8] TRANSCRIBED FIG.14 PT (trigger) and DO (damp) lanes, 33 sixteenth slots (2 measures + downbeat of 3rd).
  PT pulses (40px wide, start on the tick) at slots: 0,2,4,7,8,9,10,12,15 | 16,18,20,23,24,25,26,28,29,30,31 | 32
  DO HIGH at slots: 1,3,6,7,8,9,11,14,15 | 17,19,22,23,24,25,27,28,29,30,31
  CROSS-CHECK PASSED: DO-high set == {n-1 for each PT trigger n} EXACTLY (matches the text's "a damp signal is
  generated automatically ... one beat in advance of trigger"). This validates the 96.7px column grid (no halving trap).
  Ink separation clean: DO per-slot ink either <=181 or >=461 on the winning line, nothing between.
- [step 9] PD OUTPUT lane transcribed. Levels measured as px above that measure's own 0 baseline:
    0, 274, 338, 430, 482, 574 px  -> /47.78 px-per-semitone = 0, 5.75, 7.07, 9.00, 10.1, 12.0
  Fancy PD is restricted by the text to {0,5,6,7,9,10,12}; least-squares fit strongly prefers 6 over 5
  (residual 0.27 vs 0.66 units), and axis labels (12 at y3575, 0 at y4135 -> 46.7 px/unit) give 5.87.
  => level set = {0,6,7,9,10,12} semitones.
  PD per slot 0..31:
    0 0 0 0 12 12 12 7 10 10 9 9 7 7 7 7 | 0 0 0 0 12 12 12 7 10 7 9 9 7 9 6 7
  slot 32: PD trace not drawn that far (report as absent).
  CONSISTENCY CHECK PASSED: every PD change happens exactly on a PT trigger slot (4,7,8,10,12,16,20,23,24,25,26,28,29,30,31),
  and triggers with no change (0,2,9,15,18) match the text "the frequency deviation does not have to be specified
  in such cases since it remains constant between triggers".
- [step 10] OVERLAY VERIFIED (pt_overlay.png): red grid lines land on every CN tick; a green ring sits on every
  PT pulse with none unmarked; blue rings on every DO plateau; magenta dots track the PD staircase.
  Residual uncertainty: exactly ONE cell, PD at slot 30. Measured 274px above baseline = 5.73-5.91 semitones.
  Read as 6 because (a) the allowed fancy set is {0,5,6,7,9,10,12}, (b) this step from the adjacent 7-level is
  64px while every measured 2-semitone step here is 92-104px and the one 1-semitone step is 52px.
- [step 11] TABLE 4 = the TMS1000 microprocessor INSTRUCTION SET (mnemonic/description/hex). Not pattern content. NEGATIVE.
  Only 4 tables in the whole patent. So the ROM data for the other 15 rhythms and the other 26 beat-branches
  is NOT printed anywhere: Tables 1-3 are one sixteenth-beat example, FIG.14 is one full pattern (fancy Soul Rock).
- [step 12] Root of FIG.14 = G, per the caption ("when the G20 pedal ... or the G32 key ... is played"); verified on the
  staff: bass clef, first two noteheads on the BOTTOM line = G2. PD is therefore semitones above G2.
- [step 13] FIG. 11 (sheet 7, PDF page 8) = "EASY PLAY CHORDS" table, fully legible, TRANSCRIBED.
  Text anchor: "The table in FIG. 11 shows the chords generated by the present invention in response to the
  depressing of a key within the Easy Play range ... keys C25 through C37".
  Columns: KEY | ROOT | THIRD(MAJOR) | THIRD(MINOR) | FIFTH | SEVENTH | "7th AVAILABLE WITH * KEY SELECTORS"
  subcolumns C-Gb, D-Ab, E-Bb, F-B, G-Db, A-Eb (tritone pairs; the G one is hand-lettered and reads "G-O b").
  Values are KEYBOARD NOTE NUMBERS (all voiced inside notes 30..42):
   KEY  ROOT   MAJ3   MIN3   5TH    7TH    | 7th-avail: C-Gb D-Ab E-Bb F-B G-Db A-Eb
   C    C37    E41    D#40   G32    A#35   |  -    *    *    *    -    *
   C#   C#38   F42    E41    G#33   B36    |  *    -    *    *    -    *
   D    D39    F#31   F30    A34    C37    |  *    -    *    *    *    -
   D#   D#40   G32    F#31   A#35   C#38   |  *    *    -    *    *    -
   E    E41    G#33   G32    B36    D39    |  *    *    -    -    *    *
   F    F42    A34    G#33   C37    D#40   |  -    *    *    -    *    *
   F#   F#31   A#35   A34    C#38   E41    |  -    *    *    *    -    *
   G    G32    B36    A#35   D39    F42    |  *    -    *    *    -    *
   G#   G#33   C37    B36    D#40   F#31   |  *    -    *    *    *    -
   A    A34    C#38   C37    E41    G32    |  *    *    -    *    *    -
   A#   A#35   D39    C#38   F42    G#33   |  *    *    -    -    *    *
   B    B36    D#40   D39    F#31   A34    |  -    *    *    -    *    *
   C    C37    E41    D#40   G32    A#35   |  -    *    *    *    -    *
  (13 rows = the 13 Easy Play keys C25..C37; last row repeats the first.)
  Note: the MINOR third is notated as the enharmonic a semitone below the major third (C: maj E41, min D#40).
=== TARGET 1 DONE ===

=== TARGET 2: US 3,708,604 ===
- [step 14] Title: "Electronic organ with rhythmic accompaniment and bass". Inventors: R Hebeisen, W Tevault.
  PDF: https://patentimages.storage.googleapis.com/93/42/09/a0f057f8a9079d/US3708604.pdf ; desc text 50546 chars.
- [step 15] FIG. 5 (sheet 1 of 4 = PDF page 2, bottom left) READ at 600dpi -> pt604_fig5.png. VERDICT: NEGATIVE.
  It is a hand-drawn TIMING WAVEFORM, not a pattern grid. There is NO time ruler, NO beat/bar count axis, no
  numbered columns - the only x-axis annotation is the word "TIME ---->". The pulse trains are freehand tick
  marks at irregular spacing, and the text says outright: "It will be understood that any desired rhythm pattern
  could be employed, and that the source of pulses could be derived from any sort of pulse generating devices."
  So there is nothing here to transcribe as a pattern.
  The ten lanes, as labelled on the figure: CHORD PLAYING KEY ACTUATION (200) / PULSE GENERATOR & PROGRAMMED
  RHYTHM (202) / PEDAL RHYTHM PULSES (204, "ENABLING PULSES TO 107") / CHORD RHYTHM PULSES (206, "ENABLING
  PULSES TO 105") / PEDAL GATE TRIGGER PULSES (208, "TRIGGER PULSES TO 92") / CHORD GATE (210, "CHORD SIGNAL
  FROM G2") / MAJOR (MJ) GATE (212, "MAJOR PEDAL SIGNAL FROM G3") / MINOR (MN) GATE (214, "MINOR PEDAL SIGNAL
  FROM G4") / PEDAL SOUND (216, pulses labelled MJ or MN) / CHORD (218, "CHORD SIGNALS FROM 105").
  The one musical fact it does carry: the pedal (bass) ALTERNATES between the two pedal keyers, MJ then MN then
  MJ, switched by the multivibrator 92's trigger pulses 208 - i.e. an alternating bass, not a stored pattern.
  Quote: "line 214 ... will be seen to be the reverse of line 212 which comes about because the same monostable
  multivibrator 92 supplies the signals to both of the gates G3 and G4."
- [step 16] US 3,708,604 chord/bass VOCABULARY (quoted from the description):
  chord = "all six tones making up the chord, namely, the tones F, A, and C of two octave ranges" (an F chord,
  triad doubled at the octave = 6 tones).
  bass = "Keying voltage is also applied from a further blade of the same key via diodes D1 and D2 to the keyers
  for the F tone of the major pedal tones and to the C tone of the minor pedal tones" -> MJ = ROOT, MN = FIFTH.
  "Each pedal tone may consist of 8 foot and 16 foot notes, if desired."
  So the bass vocabulary is ROOT/FIFTH alternation (two gated keyers), NOT degrees or a stored step list.

=== PROVENANCE ===
T1 US 4,292,874 "Automatic control apparatus for chords and sequences"
   Inventors: Edward M. Jones; Carlton J. Simmons, Jr.  Assignee (original): Baldwin Piano and Organ Co
   Appl. US06/040,107, filed 1979-05-18, granted 1981-10-06.
   PDF read: https://patentimages.storage.googleapis.com/eb/2d/ff/373d2b00c1ba99/US4292874.pdf
   Data taken from: FIG. 14 (sheet 11 of 18 = PDF page 12), FIG. 11 (sheet 7 of 18 = PDF page 8),
   and TABLE 1 / 2 / 3 in the description text. CA1143190A was NOT needed (US drawings were clean).
T2 US 3,708,604 "Electronic organ with rhythmic accompaniment and bass"
   Inventors: R. Hebeisen; W. Tevault.  Assignee: Jasper Electronics Manufacturing Corp.
   Filed 1971-11-15, granted 1973-01-02.
   PDF read: https://patentimages.storage.googleapis.com/93/42/09/a0f057f8a9079d/US3708604.pdf
   Figure examined: FIG. 5 (sheet 1 of 4 = PDF page 2). NEGATIVE - timing waveform, no grid, nothing to transcribe.
=== DONE ===


# ===== source file: organs.md =====

# Printed rhythm-pattern charts: organ + rhythm-box makers not yet covered

Started 2026-08-19. Already done/closed: Ace Tone FR-2L, Roland TR-77, SGS M252/M253, ELGAM (closed).

## Search log (append after EVERY search)

### Patent lane, sweep 1 (Google Patents via websearch)
Query: rhythm pattern generator organ diode matrix Hammond 1970. Candidates surfaced:
- US3706837A Automatic rhythmic chording unit
- US3358068A Automatic rhythm device
- US3646242A Automatic rhythm instrument, cycle-end termination
- US3918341A Automatic chord and rhythm system for electronic organ
- US4244258A Rhythm system for electronic organ
- US4135423 Automatic rhythm generator
- LEAD: US3567838 (Tennes, Hammond Corp, 1971-03-02) cited as the canonical automatic rhythm generator
Verdict: TO CHECK, none read yet.

### *** STRONG HIT: US3567838 (Tennes / HAMMOND Corporation, filed 1969, granted 1971-03-02)
https://patents.google.com/patent/US3567838A/en
FIG. 2 = a real pattern chart. Rows = temple block, wood block, brush, snare, bass drum,
cymbal, chord, high bass, low bass (9). Columns = 48 time intervals = TWO measures.
Dots = hits. Three named rhythms charted: LATIN, ROCK, WALTZ.
Caveat to verify: only 3 rhythms -> confirm from the text that these are real machine
patterns and not "illustrative". Need the drawing PDF for resolution.
Verdict so far: CHART READABLE (pending image check).


# ===== source file: sources2.md =====

# Primary sources for 1960s-70s organ rhythm-box preset patterns

Started 2026-08-19. Append-only log; assume the agent may die at any moment.

RULES IN FORCE
- Primary source = service manual / service note / owner's manual / patent / schematic that PRINTS a
  pattern chart (rows = instruments, cols = counts, marks = hits), prints patterns in notation, or
  shows the diode matrix legibly enough to decode. Everything else = context, not data.
- Never present a pattern not read off a primary document. Unreadable -> UNREAD.

## Ranked table

| # | Machine | Year | Document (printed title) | URL | Page/Fig | Embedded scan res | VERDICT |
|---|---------|------|--------------------------|-----|----------|-------------------|---------|
| 1 | ELGAM (Match 7c/15C, Carousel, Symphony, Talisman) | 1970s | -- | -- | -- | -- | PENDING |
| 2 | Roland TR-66 | 1973 | -- | -- | -- | ~400 ppi (inherited lead) | PENDING |
| 3 | Korg/Keio Mini Pops MP-120 | c.1974 | -- | -- | -- | ~72 ppi (inherited lead) | PENDING |
| 4 | Ace Tone FR-1/FR-3/FR-6/FR-8L/FR-15 | 1967-72 | -- | -- | -- | -- | PENDING |
| 5 | Hammond Auto-Vari 64 / Rhythm II / Piper | 1960s-70s | -- | -- | -- | -- | PENDING |
| 6 | Farfisa / Yamaha / Lowrey / Gulbransen / Univox SR-95 / Maestro / Seeburg / Philips | -- | -- | -- | -- | -- | PENDING |

## Already done elsewhere (do not redo)
- Ace Tone Rhythm Ace FR-2L service note (c.1969), archive.org item `RhythmAceFR2LServiceManual`:
  p14 Fig 10 = full 16-rhythm pattern chart; p13 Fig 9 = 35 numbered pulse trains. Being
  transcribed by the main session.
- Roland TR-77 service manual: another agent has it.

## Log

- (init) file created with skeleton.
- ELGAM: archive.org fulltext search `q=elgam` -> numFound 3, all irrelevant (2 CIA docs, 1 user
  favorites list). NO Elgam material on archive.org at all.
- ROLAND TR-66: archive.org item `roland-tr-66-service-notes` = "Roland TR-66 Service Notes",
  dated 1976. PDF: https://archive.org/download/roland-tr-66-service-notes/Roland%20TR-66%20Service%20Notes.pdf
  Item page: https://archive.org/details/roland-tr-66-service-notes
  OCR (djvu.txt) TOC confirms: SECTION 5 MATRIX CIRCUIT (incl. "5-2. Logic Output Timing Chart"),
  SECTION 6/7 RHYTHM SWITCH ASSEMBLY (RS-9 / RS-6), **SECTION 9. RHYTHM ENSEMBLE PATTERN, page 12**.
  OCR of that page shows rhythm labels WALTZ / JAZZ WALTZ / SLOW ROCK / BOSSANOVA / BEGUINE /
  RHUMBA / PARADE / HABANERA / FOXTROT, a "BASS-SNARE" lane label, and a count ruler reading
  "8 20 22 24 26 28 30 32" -> a 32-count grid numbered in evens. So a real pattern chart exists.
  Also: other TR-66 items on archive.org = `synthmanual-roland-tr-66-owners-manual` (owner's manual),
  `manualsonline-id-5d659ae5-...`, `manualsbase-id-576194`.
- TR-66 **CONFIRMED CHART READABLE**. PDF page 13 (printed "- 12 -") = "SECTION 9. RHYTHM ENSEMBLE
  PATTERN   Fig. 13". Embedded scan: 3304x4677, 1-bit CCITT bitonal, **400x400 ppi** (pdfimages -list),
  27 pages total. Rendered at only 100 dpi it is already fully legible.
  Chart form: rows = instrument abbreviations, columns = counts numbered 0,2,4,...,30 (even ticks,
  so a 32-count grid), hits drawn as small OPEN CIRCLES on the lane, plus diagonal hatch marks in
  some columns. Rightmost column = per-lane matrix/output numbers (e.g. 21', 15', 21', and "F.B.").
  Rhythms on p13: WALTZ, JAZZ WALTZ, SLOW ROCK, BOSSANOVA, SAMBA, MAMBO, CHA-CHA, BEGUINE, RHUMBA.
  Footnote: "M = HH".
  Instrument abbrevs seen: CY (cymbal), SD (snare), BD (bass drum), HH (hi-hat), RS (rim shot),
  M (=HH), CB (conga/bongo?), HB (high bongo), LB (low bongo), HC (high conga?), C (claves).
- TR-66 page 14 (printed "- 13 -") holds **Fig. 14 + Fig. 15**, both ROTATED 90 deg (patterns run
  vertically, count ruler up the right edge 0..31 in evens). Fig. 14 is split into two labelled
  groups:
    2 BEAT: BASS-SNARE (SD,BD) | FOXTROT 1 (HH, CY+SD, BD) | SWING 1 (HH, CY, BD) |
            MARCH (CY, SD, BD) | PARADE (CY, SD, BD) | HABANERA (SD, BD)
    4 BEAT: BASS-SNARE (SD,BD) | FOX TROT 2 (HH, CY+SD, BD) | SWING 2 (HH, CY, BD) |
            SWING 3 (HH, CY, BD) | SHAFFLE [sic] (CY, SD, BD) | TANGO (HH, SD+CY, BD)
  Fig. 15: ROCK 1..ROCK 6, each with lanes HH, CY, SD, BD.
  => the FULL TR-66 preset roster is charted over printed pp.12-13 (PDF pp.13-14).
