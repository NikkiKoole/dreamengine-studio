# Rhythm boxes: 101 preset patterns, read off the manufacturers' own documents

> **STATUS: LIVING (2026-08-20)** ‑ sourced pattern data for three generations of preset rhythm
> hardware (1969 discrete logic, 1972 diode matrix, 1970s mask ROM). Every mark was read off a
> service manual, service note or databook. Evidence trail, including the measurements and the
> readings that were rejected: [`rhythm-box-transcription-log.md`](rhythm-box-transcription-log.md).

The organ-era rhythm box is the machine this studio keeps approximating and had never actually
looked at. `sideman`'s twelve rhythms are **plausible reconstructions**, not sourced data, and so is
every latin pattern anywhere else in the repo. This file is the real thing: 101 rhythms, each
traceable to a page and a figure.

It exists because a research question ("what hardware is Domenique Dumont using?") landed on a
1970s Elgam organ's AUTO RHYTHM section, and the honest answer to "what does that actually play?"
turned out to be documented for the machines around it and, for one specific and interesting
reason, not documented at all for the Elgam itself (§5).

---

## 1. The machines

| Machine | Year | How the patterns are stored | Rhythms | Document |
|---|---|---|---|---|
| Ace Tone Rhythm Ace **FR-2L** | c.1969 | discrete logic, 35 shared pulse trains | 16 of 16 | *Rhythm Ace model FR-2L Service Note*, p14 Fig 10 (patterns), p13 Fig 9 (the pulse trains) |
| Roland Rhythm **TR-77** | 1972 | diode matrix | 18 of 18 | *Roland Rhythm Instrument* service manual, 7th ed. Nov 1976, p15 Fig 9 (Jazz) + Fig 10 (Latin) |
| **SGS M252** rhythm LSI | databook 1979 | mask ROM, two published factory masks | 15 + 15 | *1979 SGS MOS and Special COS/MOS*, 1st ed., pp123-125, Tables 1 and 2 |
| **SGS M253** rhythm LSI | databook 1979 | mask ROM, sibling part | 12 new (+12 duplicates) | same databook, pp134-136, Tables 1 and 2 |
| **Hammond** organ rhythm system | filed 1969, granted 1971 | motor commutator or coincidence type | 3, and they carry CHORD + BASS | US patent **3,567,838** (Tennes & Kern, Hammond Corp), FIG. 2 |
| **SGS M255** rhythm LSI | databook 1979 | mask ROM | 6, with NAMED accompaniment lanes | same databook, pp146-147 + the truth table on p158 |
| **SGS M254** rhythm LSI | databook 1979 | mask ROM, two masks | 16, mostly ACCOMPANIMENT | same databook, pp142-144 (+ pinout p145) |
| **Baldwin** organ accompaniment | filed 1979 | TMS1000 microprocessor | 0 patterns (FIG. 14 rejected, §4g), but the CHORD RULE | US patent **4,292,874**, FIG. 11 |

- FR-2L: <https://archive.org/details/RhythmAceFR2LServiceManual>
- TR-77: <https://archive.org/download/roland_Roland_TR-77_Service_Manual/Roland_TR-77_Service_Manual.pdf>
- SGS databook: <http://www.bitsavers.org/components/sgs/_dataBooks/1979_SGS_MOS_And_Special_COS_MOS_1stEd.pdf>
- Elgam Carousel schematic (rhythm list + the chip designators, §5):
  <https://www.smemmusic.ch/sites/default/files/2026-05/elgam_carousel_sch.pdf>

Why these are the right sources: Ace Tone **sold its rhythm units to Hammond to build into
organs**, the TR-77 is its direct successor (and the CR-78's predecessor, whose patterns this repo
already has from a pattern book), and the SGS M252 is **the chip that put auto-rhythm inside cheap
Italian organs**, which is the class of machine the whole enquiry started from.

**A companion, deliberately kept separate.** [`runtime/drumpat.h`](../../runtime/drumpat.h) holds 565
MODERN 16-step patterns (the maker's own library, converted). It is a step-sequencer grid and cannot
express any of §3's findings, which is exactly why the two are different files rather than one
merged library: squashing a 24-count bar with held gates and a two-bar clave into 16 sixteenths
would quietly destroy the thing this document exists to record.

## 2. How to read the tables

**FR-2L.** A 48-count clock spanning **two bars**: 24 counts per bar, 6 counts per beat. Rows are
24 characters for bar 1, two spaces, 24 for bar 2. `x` = a circle on the chart.
The chart **rules only counts congruent to 0, 2, 3, 4 (mod 6)**, which is the union of the swung
eighths (beat, +2, +4) and the straight eighths (beat, +3). Counts congruent to 1 or 5 have no rule
and can never carry a mark, which doubles as a free noise floor when reading.

**TR-77.** **32 counter states per bar**, of which the chart rules only the even half as 16 columns.
Rows are 16 characters, `x` = mark, `/` = a **hatched** column, meaning a state this rhythm does not
use. Two rhythms (Bolero, Jazz Waltz) put marks on **odd** states, between ruled columns, so 16
columns is a lossy model of this machine and 32 states is the honest one.

**SGS M252.** One character per counter state from 1 to that rhythm's **reset count** (23, 24 or 32,
stated per rhythm). Lanes are `OUT 1..8`, which are **chip pins, not instruments**: the instrument
assignment is per mask and the databook prints it separately (§4).

## 3. What the hardware teaches

Six findings, each visible in more than one place, and all of them design-relevant rather than
trivia:

1. **A fixed clock, re-divided per rhythm.** The FR-2L's 4/4 rhythms take a 24-count bar with
   6-count beats, its **Waltz divides the same bar by three** (8-count beats), and its **Slow Rock
   reads the whole 48 counts as one 12/8 bar** with 4-count triplets. The TR-77 does the same thing
   by a different mechanism: it strikes out columns with hatching. So a player of this data needs
   beats-per-bar and pattern-length **per rhythm**, never one global setting.
2. **Accent by layering, not velocity.** Neither machine has dynamics. The FR-2L's Rock'n Roll runs
   one bass lane on every beat and a second on beat 1, so beat 1 fires twice; its Shuffle stacks
   three voices on every beat. That is why these boxes have a downbeat that leans forward.
3. **Two mechanisms for buying a two-bar feel, both cheap.** A shared **fill pulse late in bar 2**
   (FR-2L Dixieland, Fox Trot and Swing all fire on count 40, which is what identifies it as a fill
   lane rather than a quirk), and a **single asymmetric cell**: the FR-2L Samba's maracas play every
   third count *except* count 0 while hitting count 24, and that one blank is the whole two-bar
   identity of the rhythm. The FR-2L Mambo does it with one extra hit at count 27.
4. **Swing placement is explicit and not uniform.** The FR-2L's Shuffle puts its offbeat on the
   **third** triplet (+4 of 6), its Western on the **second** (+2 of 6). The same chart distinguishes
   them, so both are intended, and neither is expressible on a sixteenth grid.
5. **Some lanes are gates, not triggers.** The FR-2L's Dixieland `B` lane is a thick sustained bar
   held for a whole beat (a brush swish), and the TR-77's guiro lane is drawn the same way. A player
   needs a gate concept, not only hits.
6. **A rhythm is an assignment of shared pulse trains.** The FR-2L's Fig 9 defines **35 numbered
   pulse trains** and Fig 10 wires them to instruments per rhythm, reusing them across rhythms
   (code 1 = beat 1 only, code 3 = every beat, code 5 = every eighth, each confirmed in several
   rhythms), and a lane can be driven by a **sum** of codes. That is the diode matrix, and it is why
   every box of the era sounds related. It is also a far better cart architecture than 16 independent
   grids.

## 4. The SGS chip maps outputs to instruments, and the two masks disagree

Page 122 of the databook prints a generic pinout naming pins by OUTPUT number, beside per-mask
"standard content configuration" pinouts that put instrument names on those same pins. Combining
them gives a real per-mask assignment, and **OUTPUT 5 is conga drum on mask AA but high hat on mask
AD**, with 6, 7 and 8 differing too. Two outputs are double-duty by footnote: on AD, pin 12 drives
snare on rhythms 1, 3, 4, 6, 7, 9, 12, 14, 15 and conga on 5, 8, 10, 11, 13.

Those footnotes doubled as an **independent oracle**, because naming which rhythms use an output is
a claim about pattern content. Checked against lanes already read blind: the output non-empty in
exactly {1,3,4,5,8,9,10,11,12,13,14} is OUT 7, which the pinout calls "long cymbals or claves"; the
output used in every rhythm except Tango is OUT 2, "snare drum or conga drum". Both matched.

### 4a. What the datasheet says about wiring the chip to sound, and one prediction it makes

The M252 pages of the same databook print the electrical contract, which is what a cart would need
if it ever modelled this chip rather than just playing its patterns. Quoted from printed page 120
and the truth-table notes:

- **"DRIVES 8 SOUND GENERATORS (INSTRUMENTS)"** with **"OPEN DRAIN OUTPUTS"**: the chip only pulls
  trigger lines down. The sound was always somebody else's circuit.
- **"15 PROGRAMMABLE RHYTHMS (NOT AVAILABLE IN COMBINATION)"**. Worth contrasting with the Ace Tone
  FR-1, whose selling point was that you *could* hold two rhythm buttons at once for "more than a
  hundred" combinations. The chip generation took that away.
- **"MASK PROGRAMMABLE RESET COUNTS: 24 or 32"**, and a **"DOWN BEAT OUTPUT"** which "appears at the
  beginning of each measure", is "only 2-3 µs long" and "must be stretched and buffered to enable it
  to drive a lamp". So the blinking tempo light on these organs is a real hardware output, not
  decoration derived from the pattern.
- The ROM is **"32 rows which represent elementary times and 120 columns (15 groups of 8)"**.

**And then a testable claim.** To shorten a rhythm you "put a cross in the N+1 position of the
column which now represents the reset output, **rather than the 8th instrument**". That predicts
something specific about the transcribed data: every rhythm with a reset below 32 should have an
empty 8th lane, because that lane is a counter rather than a drum.

Checked against all 30 transcribed rhythms: **7 have a reset below 32, and all 7 have an empty
OUT 8 (7/7, no exceptions)**, while 14 of the 23 full-length rhythms do use OUT 8. So a short SGS
rhythm has **seven** instruments, not eight, and `runtime/rhythmbox.h` flags those lanes
`RB_RESETCOL` so a cart cannot wire a drum to a counter. That is the datasheet prose and the dot
data confirming each other, from two different documents by two different readers.

### 4b. Elektor, April 1976: the article that stands in for Technical Note 131

Source: *"ic rhythm generator"*, **Elektor no. 22, April 1976, pp420-425**, following a shorter
description in the July/August 1975 Summer Circuits issue. It states its own purpose as dealing with
"the applications of these IC's in greater detail, including their connection to simple instrument
generator circuits suggested in the SGS application notes", which is precisely what TN131 was cited
for. Obtained from the Internet Archive's `ElektorMagazine` item (1975-07_08 and 1976-04).

Four facts from it change how this data must be played:

1. **What 24 and 32 MEAN.** "in 4/4 time, with 8 [elements] per beat there would be 32 per bar; in
   3/4 time there would be only 24". So the chip's mask-programmable reset is a TIME SIGNATURE, not
   an arbitrary length. Caveat, and it is ours rather than the article's: 24 also serves 4/4 at six
   elements per beat, which is a triplet feel, and that is plainly how mask AD's SLOW ROCK and
   SHUFFLE use it, since neither is a waltz. So read 24 as "divided by three somewhere", and check
   the rhythm's own marks to see where.
2. **Adjacent marks do NOT retrigger.** The trigger is the positive-going edge of a ROM bit, so "if
   two successive [addresses] were '1' … the output would initially go to '1' and [stay] for two
   time elements, so the second triggering edge would not occur". This is not a corner case: the SGS
   tables hold **36 runs of adjacent marks, the longest six states long**, and measured through the
   generated header, **66 of 955 marks (7%) do not sound**. A naive player fires 7% too many hits.
   `runtime/rhythmbox.h` therefore flags every SGS rhythm `RB_EDGE_ONLY` and provides `rb_trigger()`,
   which is what a sequencer should call; `rb_hit()` reports the bit, not the sound. The FR-2L and
   TR-77 are different machines (discrete pulse trains, a diode matrix), the rule is not transferable
   to them, and their data contains almost no adjacent marks anyway (3 runs in 62 lanes), so they
   carry no flag.
3. **Two ICs can split one bar**, and this is probably what Elgam did. Figures 20 and 21 show a
   circuit where "the first IC plays the first half of the bar, and the second IC the second half".
   Elgam's Carousel carries TWO of these chips (`M252 D1 AE` and `AF`) while its dial offers only
   **15** rhythms, not 30, so the second chip is not buying more rhythms. The half-bar trick explains
   that exactly, and would mean Elgam's rhythms are twice as long as a single chip's. **This is an
   inference, not a sourced fact**: the other candidate is 8 + 8 = up to 16 instrument outputs. Both
   are consistent with the schematic; the Elektor circuit is precedent for one of them.
4. **The panel, the lamp and the start.** Rhythm selection is a 4-bit code driven by a diode matrix
   or a TTL/CMOS encoder from 15 switches (figures 8 to 11, with Table 2 giving the code per rhythm,
   which is the same shape as the Carousel schematic's own "RHYTHMS CODE" table). Figure 12 is the
   clock generator plus the downbeat indicator that the datasheet said needed stretching. Figure 22
   starts and stops the rhythm automatically **from the organ keyboard or pedal**, which is worth
   knowing for a cart: on these instruments the drums began when you played, not when you pressed a
   button.

The voice side is documented too, which closes the last gap TN131 left: figure 24 gives "the circuit
of the noise generators, preamplifier and power supply of a complete percussion unit", and the
article also covers interfacing the chip to Elektor's own *Minidrum* (Elektor nos. 2 and 3).

### 4c. The M253, and what a mechanical cross-check of 720 pairs found

The M253 is the M252's sibling part, in the same databook (pp134-136). Both of its published masks
were transcribed, then every M253 block was compared against every M252 block, lane by lane, over
all 720 pairs. Two results, and they point in opposite directions:

**Mask AC is mask AD's first twelve, byte for byte.** All 12 AC rhythms match their same-numbered,
same-named M252 AD rhythm on all eight lanes, character for character. AD's extra three (BAJON, FOX
TROT, SHUFFLE) have no AC counterpart. SGS shipped the same twelve rhythm ROMs under two part
numbers. So **AC contributes no new pattern data** and is deliberately NOT in the generated header:
including it would inflate the library with duplicates and make the corpus look richer than it is.
Worth noting that the earlier warning here was "names matching is not content matching", which was
exactly the right thing to check, and this time the answer came back that they DO match. Only
reading the cells could establish that either way.

**Mask AA is genuinely new, and is in the header.** Twelve rhythms, unnamed in the databook (blocks
are captioned only "RHYTHM 1" to "RHYTHM 12"). An earlier reading suggested M253 AA blocks were
M252 AA blocks with the lanes shifted down one output; tested mechanically as "M253 OUT k+1 ==
M252 OUT k for all k", **no pair satisfies it**. The best pairs agree on 5 of 7 lanes, and most of
those agreements are two empty lanes matching trivially; counting only lanes that carry marks, the
best pairs manage 2 to 4 out of 3 to 6. What does hold is **lane-level** reuse: individual lanes are
character-for-character identical to an M252 AA lane one output up, and every M253 AA block adds
marks on OUT 1 that its nearest M252 relative lacks. So the reuse is real but finer-grained than a
shifted block.

**A correction to §4a's reset rule, from the M253 reading.** The implication runs ONE WAY ONLY.
Every short rhythm has an empty OUT 8 (3 of 3 on the M253, 7 of 7 on the M252, no exceptions), but
the converse fails: M253 AA rhythms 1, 4 and 6 run the full 32 counts and still leave OUT 8 empty.
So an empty OUT 8 is evidence of nothing by itself, and anyone inferring a reset count from a blank
lane would misread those three as short. Also, the databook conveys a short reset by **shading**
counts 25-32 across all eight columns rather than by printing a cross in the OUT 8 lane, so the
datasheet's "crossed column" language describes the mask programming, not the printed table.

### 4d. A patent that carries CHORD and BASS lanes, which no service manual here does

Source: **US patent 3,567,838**, "Musical instrument rhythm system having provision for introducing
automatically selected chord components", Charles J. Tennes and Donald R. Kern, assigned to
**Hammond Corporation**, filed 1969-11-12, granted 1971-03-02. FIG. 2 is a pattern chart.
<https://patents.google.com/patent/US3567838A/en>

Patents turn out to be the best source class for this material, and this one answers a question the
service manuals could not: what did the **accompaniment** play? Every machine in §1 is percussion
only. This one charts nine lanes including **CHORD**, **HIGH BASS** and **LOW BASS**.

**The mechanism, quoted rather than inferred.** The rhythm programmer "cycles on a two measure
basis with a capability of 24 equally spaced pulses per measure", so "any output lead can receive
any desired number of pulses and at any time according to a preset program covering 48 equally time
spaced intervals before repeating". That is the FR-2L's structure exactly (24 per bar, 48 over two
bars), reached independently by a different company in a different year, and the patent calls such
systems "current and common", which is the closest thing to a statement that this was the industry
norm rather than one machine's quirk.

**What makes the chord and bass lanes usable.** They are not note sequences. The programmer has
"three extra output pulse leads" which, instead of driving percussion circuits, drive the **chord
gate**, the **high bass gate** and the **low bass gate**. So the pattern decides WHEN the chord the
player is already holding is let through, and when each bass note sounds; the notes themselves come
from the keyboard, with the bass derived by frequency division ("the root and fifth bass notes are
obtained by frequency division"). A cart can implement that directly: a gate lane over whatever the
player holds, which needs no chord data at all.

**And the pattern says what the gating is for.** In both LATIN and ROCK, LOW BASS fires on beat 1
and HIGH BASS on beat 3, and nowhere else. That is the alternating root-fifth bass of every organ
accompaniment, in the data. The CHORD lane is drawn combined with the snare (`CHORD & SNARE DRUM`),
i.e. one programmer lead drives both, so the chord stabs land on the snare's rhythm; in WALTZ the
combined lane is `LOW BASS & BASS DRUM`, so the bass note and the kick share a lead.

**The waltz confirms the per-rhythm re-division again.** Its 24-pulse measure divides by THREE
(8 pulses per beat): bass and kick on beat 1, chord and snare on beats 2 and 3. The FR-2L waltz
divides its own 24-count bar the same way, and neither reading was made with the other in view.

Read at 300 dpi from the patent's own drawing sheet, rotated, with a 49-position grid (one column
per pulse; the printed labels sit on every second one, which cost me a first pass that sampled only
the labelled columns and silently halved every lane). Cell ink separated cleanly: 678 cells at or
below 0.229 and 90 at or above 0.46, nothing between, and an overlay of the accepted cells was
checked against the figure by eye.

**CAVEAT, and it is the one the source itself invites.** Only three rhythms are charted, and the
patent explicitly disclaims novelty in the programmer ("no novelty is claimed in any particular
rhythm programmer"), so these three are best read as **representative content of a real Hammond
programmer** rather than as the full dial of a specific model. The percussion roster it assumes is
stated in the text: temple block, wood block, brush, snare drum, bass drum, cymbal.

The data itself is in §8.5, with the other machines.

Bar 2 differs from bar 1 on exactly two lanes, both in LATIN: TEMPLE BLOCK drops its downbeat and
WOOD BLOCK plays a different figure entirely. Every other lane repeats at +24. So this machine, like
the FR-2L, spends its two-measure span on a small deliberate asymmetry rather than on a wholly
different second bar.

### 4e. The chord and bass content is IN the rhythm ROM, one column-group over

This reframes §4d rather than adding to it, and it is the most useful thing found in this pass.

The SGS accompaniment chip **M251** ("arpeggio chord and bass accompaniment generator", databook
printed pp113-120) prints **four truth tables**: an arpeggio table addressed by a 4-bit code with
three simultaneous outputs, and bass and chord tables in both automatic and semiautomatic modes.
But those tables hold no music. They are a DECODER: given a code, which degree sounds. The
vocabulary is degrees plus an octave multiplier (2nd, octave, 9th, 6th-or-7th, 5th, 3rd, TONIC,
each with a divider), and the chip is not told the key: in automatic mode it takes the **lowest key
held** as tonic and derives the rest internally, with major/minor and 6th-versus-7th as front-panel
switches. It is also a clock slave, "normally used in conjunction with an external self-scanning ROM
(such as the M 252 - 3 or 4)".

**So where is the accompaniment PATTERN? In the rhythm chip's own table.** The **M254 B1AD** pinout
(printed p145) labels eight of that chip's twelve outputs as `I2..I8` plus `TRIGGER CHORDS` feeding
the M251, and only four as drums. Which means the M254 table (printed pp150-151, 32 elementary times
by 8 named rhythms: Waltz, Tango, Swing, Beat, Bossa Nova, Samba, Rumba, Slow Rock) is a **published
chord, bass and arpeggio content table**, not a drum table. A method check transcribed its WALTZ end
to end and it decoded into an oom-pah-pah: root bass, two chord stabs, octave bass, two more stabs,
under an independent 12-step arpeggio. That fell out of the crosses rather than out of anyone's
expectations, which is the check that matters.

Two more of these tables carry accompaniment: **M255 B1-AB** (OUT 4 = FIFTH, OUT 5 = CHORD TRIGGER)
and the **M108** single-chip organ's own 3-bit bass table, whose degree vocabulary includes a
**fourth** that the M251 lacks.

The practical consequence for this project: there is no separate chord-pattern book to find. The
accompaniment data sits in the same tables as the drums, distinguished only by which pin a column
group drives, and reading it needs the PINOUT page as well as the truth table.

**Patents, for comparison, mostly do not carry this.** Eight were opened; their figures are block
diagrams, schematics and timing waveforms. Two exceptions worth pulling: **US4292874** names sixteen
bass rhythm patterns (Bossa Nova, Tango, Swing, Teen Beat, Shuffle, Waltz, Pop Rock, March, Soul
Rock, Rhumba Beguine, Fox Trot, Polka March, Bolero, Samba) and prints per-sixteenth trigger flags
in its TABLE 1, and **US3708604A** (Jasper Electronics, 1973) charts "the periods during which pedal
tones and chords sound" as lanes over time, the same figure kind as §4d. A third, US4520707A, prints
its microprocessor program as raw hex with no key: the content is there and unreadable.

### 4f. Two smaller answers: the bass DEGREE vocabulary, and why adjacent marks cannot retrigger

**The M108 single-chip organ prints a complete bass decoder** (databook printed p61, "BASS TRUTH
TABLES", given twice, once in negative and once in positive logic: same content, inverted codes, not
two different tables). A 3-bit code selects a degree:

| code | Bass Arpeggio Output (automatic) | Alternate Bass Output (manual) |
|---|---|---|
| 000 | No change | No change |
| 001 | **Root** | 1st on the left |
| 010 | **3rd** | --- |
| 011 | **4th** | --- |
| 100 | **5th** | 1st on the right |
| 101 | **6th** | --- |
| 110 | **7th** | --- |
| 111 | **8th** (octave) | --- |

Two things worth having. Its vocabulary includes a **fourth**, which the M251's does not, so the
degree set was not fixed across the family. And the manual mode defines the alternating bass
concretely: the chip "gives at the bass output an alternating bass between the first on the left and
the first on the right of the keys pressed in the ACC. section", i.e. leftmost held key against
rightmost held key, with only two codes mattering (root to the left key, fifth to the right).

It also confirms §4e's architecture independently: "the pitch switching timing is dependent on an
external ROM (3 bits)". The chip decodes; the rhythm ROM holds the timing. Two different SGS parts,
same division of labour.

**The M258/M259 pair publishes no content, and says why the edge rule exists.** It is marked
PRELIMINARY DATA: 16 rhythms, 16 outputs in two sections of 8, reset 24 or 32, two chip selects. No
standard-content truth table is printed, so there is nothing to transcribe, and its one
table-looking figure ("INSTRUMENT BEATS VERSUS RHYTHM PROGRAM", printed p165) is a **timing diagram**
rather than a pattern: it traces two example columns through to instrument beats, sync and down beat.

But that diagram is the mechanism behind §4b's edge rule. Its two example columns are labelled
**"OUT 1 WITHOUT RETURN TO 1"** and **"OUT 5 WITH RETURN TO 1"**, and the datasheet lists as a
feature "CHOICE BETWEEN RETURN TO '1' OR NOT ON 8 OUTPUTS SEPARATELY". So an output that does not
return high between two set states cannot present a second rising edge, which is exactly why
adjacent marks fire once on the M252; and by this later part the behaviour had become a **per-output
mask option**. The rule this project inferred from a 1976 magazine article is a documented, and by
1979 configurable, property of the silicon.

### 4g. A bass line with PITCHES, and a chord table that is really one line of arithmetic

Source: **US patent 4,292,874**, "Automatic control apparatus for chords and sequences", Edward M.
Jones and Carlton J. Simmons Jr., **Baldwin Piano and Organ Company**, filed 1979-05-18, granted
1981-10-06. This is the only source in the collection whose bass carries **pitch** rather than gates.

Sixteen rhythms are named, each with a *plain* and a *fancy* variant: Swing, Teen Beat, Shuffle,
3/4 Waltz, Pop Rock, 6/8 March, Soul Rock, Rhumba Beguine, Tango, Fox Trot, Bossa Nova, Polka March,
Bolero, Samba, Merengue, Cha-Cha.

**The step unit is stated, not guessed.** The Activity Next bits "show which of the rhythms have
activity (a trigger or a special damp) during the next sixteenth beat", and FIG. 14's own ruler
counts CN 0..15 per measure. So: sixteenths, 16 per measure.

**FIG. 14 was attempted and REJECTED, and the rejection is the useful part.** It was reported as a
transcribable chart of the fancy SOUL ROCK pedal pattern in root G, three lanes over 33 sixteenths:
`PT` trigger, `DO` damp and `PD` in semitones above the played root. It is not a chart. Rendered at
400 dpi it is a set of **hand-drawn timing waveforms** plus a musical staff: `PT` and `DO` are square
waves whose edges must be read, and `PD` is a **staircase** whose level must be read against a 0-to-12
axis. Reading pulse edges and step heights off freehand traces is a different and much weaker
operation than reading dots off a ruled grid.

The transcription that came back did not survive its own stated cross-check. The patent says "a damp
signal is generated automatically one beat in advance of trigger", so the damp set must be exactly
the triggers shifted back one step; checked against the reported rows it is not (three damps have no
trigger after them, five triggers have no damp before them). That check had been offered as the
evidence the reading was right, so its failure retires the reading. The numbers are therefore NOT in
this document and NOT in `rhythmbox.h`, and the generator's assertion that would have shipped them
was deleted rather than loosened.

Two things about the figure are worth recording for whoever tries again. The staff at the top
notates the bass line directly, which is a far better route than the staircase: read the notation,
not the trace. And the axis labels genuinely are what the earlier reading claimed, so the vocabulary
below stands even though the pattern does not.

**What is NOT there.** Tables 1-3 print only ONE sixteenth beat (MN=1, CN=3) as a worked example of
the ROM encoding: 28 Activity Next bits, 14 fancy plus 14 plain, one per rhythm. The other 26
branches and the other fifteen rhythms' data are printed nowhere. So this patent yields **one**
pattern, not sixteen.

### 4g.1 FIG. 11, "EASY PLAY CHORDS", and the rule hiding inside it

The same patent prints a 13-row chord table giving, per key, the ROOT, MAJOR third, MINOR third,
FIFTH and SEVENTH as keyboard note numbers, every value inside notes 30 to 42. Checked
programmatically against all twelve keys, the whole table reduces to **one line of arithmetic with
zero mismatches**:

    note = 30 + ((pitch_class - 5) mod 12)

applied to root, root+4 (major third), root+3 (minor third), root+7 (fifth) and root+10 (dominant
seventh); the table writes 42 rather than 30 when a tone lands on the window's top F.

The musical point is worth more than the table: **every chord tone is folded into one fixed 12-note
window** (F upward). That is why an organ's auto-chord sits in the same register in every key instead
of marching up the keyboard, and it is two lines in a cart. Note also that the minor third is
notated as the enharmonic a semitone below the major third (in C: major E41, minor D#40).

### 4g.2 The negative, recorded so nobody re-downloads it

**US 3,708,604** (Hebeisen and Tevault, Jasper Electronics, filed 1971, granted 1973) was chased
because its FIG. 5 was reported to chart "the periods during which pedal tones and chords sound".
It is a **hand-drawn timing waveform, not a pattern grid**: ten labelled lanes, no time ruler, no bar
or beat count, freehand ticks at irregular spacing, and the only x-axis annotation is the word
"TIME". The description forecloses it outright: "any desired rhythm pattern could be employed, and
the source of pulses could be derived from any sort of pulse generating devices". Nothing to
transcribe.

It does carry the mechanism, and it agrees with §4d: the bass alternates between two gated keyers
flipped by a monostable multivibrator rather than by any stored step list, where "MJ" is the root and
"MN" the fifth, each optionally 8' plus 16'. Root-fifth alternation, gates only, no degrees, no
pattern memory.

## 5. Elgam: the patterns cannot be sourced, and the reason is the interesting part

Elgam (Castelfidardo, 1968-1982) is the machine that started this. Its **Carousel schematic** gives
the 15-rhythm dial list with 4-bit select codes: WALTZ, JAZZ WALTZ, TANGO, POLKA, FOX TROT, SWING,
SLOW ROCK, ROCK, RHYTHM & BLUES, AFRO, SAMBA, BOSSA NOVA, RUMBA, BEGUINE, CHA-CHA.

The same schematic shows the patterns are **not a board-level diode matrix at all**: they live in two
custom-masked SGS LSIs marked `M252 D1 AE` and `M252 D1 AF`. SGS sold that chip either with standard
content or "tailored to customer requirements", and **never published a customer mask**. So there is
no document to find, and that conclusion rests on chip designators printed on Elgam's own drawing
rather than on a failed search.

**The fabrication boundary, stated once:** the SGS tables in §7.3 are the **factory** masks, not
Elgam's. Rhythms 1-3 and 7 share names and slots with Elgam's dial; 8-15 provably do not. Recovering
Elgam's own content needs a ROM dump off a surviving chip, or transcription from recordings.
Farfisa is not a substitute: same town, same years, same silicon vendor, so its rhythm sections are
very likely the same locked mask ROMs, and no Farfisa pattern chart surfaced.

## 6. Caveats, all of them measured

1. **FR-2L lane labels are PROVISIONAL.** Its embedded scan is about **75 dpi**, so a two-letter lane
   label is roughly 3x4 pixels per letter and the crisp letterforms in an upscaled crop are
   interpolation. Circle **positions** and lane **order** are solid; the letters are not. Where a
   table says `Hc` or `Cb+Hb`, read "the lane at this position, whose label is probably that". A cart
   can play these patterns confidently but should not claim which voice plays which lane. The
   instrument **roster** is sourced (manual pp2 and 4): bass drum, low and high conga, high bongo,
   cowbell, claves, snare, cymbal, maracas, wire brush, with trim pots VR4 to VR11 named in order.
2. **The TR-77 page is sheared** about 0.7 degrees, so a flat column model is wrong by up to half a
   column, which is exactly the error that silently moves every hit one slot. The fitted model
   carries a y term (`x = 1446.767 + 0.012096*y + 39.0577*k` for the upper Latin block). The FR-2L
   page was checked for the same defect: it drifts at most 0.24 of a count, so its flat model stands.
3. **Do not use the SGS M252 preliminary datasheet** (`njohnson.co.uk/pdf/m252.pdf`) as data. Its
   pages 10-11 look exactly like a filled pattern chart, but only rhythms 1-3 carry marks and its own
   text calls them "three imaginary rhythms chosen randomly". It is a blank customer ordering form.
4. **M253 mask AC is a trap in waiting.** It appears to hold only 12 rhythms while reusing mask AD's
   names in the same slots, so it needs cell-by-cell checking rather than name matching.

## 7. Validation nobody designed in

Four named claves fell out of raw dot positions, across two machines, two documents and four
independent readers: the FR-2L Rhumba's **3-2 son clave**, the FR-2L Bossanova's **bossa clave**, the
TR-77 Rhumba's **rumba clave**, the TR-77 Bossanova's **bossa clave**. A mis-fitted grid or a
mis-assigned lane does not produce a named clave.

Separately, both machines' **Waltz** reads the same musically (bass and cymbal on beat 1, snare on 2
and 3) despite completely different clock structures, and neither reading was made with the other in
view. And the TR-77's metronome lane clicks eighths in exactly those rhythms whose trigger number
reads `42 + 5` and quarters in those reading `5`, which was used to **predict** Tango's metronome
before looking at it.

## 8. The data

Per-rhythm confidence markers, the doubtful cells, and the measurement notes live in the
[transcription log](rhythm-box-transcription-log.md). Everything below was extracted from those
readings programmatically, not retyped.

### 8.1 Ace Tone Rhythm Ace FR-2L, 16 rhythms
24 counts per bar, 6 per beat, two bars per row pair. Labels provisional (§6.1).

**BEGUINE**
```
M     x..xx.x..x..x..x..x..x..  x..xx.x..x..x..x..x..x..
C     ........................  ........................
Cb    ...x.....x.....x.....x..  ...x.....x.....x.....x..
Cy    ...x....................  ...x....................
Lc    ...x....................  ...x....................
LcBd  x...........x.....x.....  x...........x.....x.....
```

**Bossanova**
```
Cy      code 25   ......x...........x.....  ......x...........x.....
Cy'     code  5   x..x..x..x..x..x..x..x..  x..x..x..x..x..x..x..x..
Cb+Hb   code 17   x........x........x.....  ......x........x........
Lc+Bd   code  2   x...........x...........  x...........x...........
```

**Waltz**
```
Cy      code  1   x.......................  x.......................
M       code 34'  ........x.......x.......  ........x.......x.......
Sd      code 39   ........x.......x.......  ........x.......x.......
Bd      code  1   x.......................  x.......................
```

**Dixieland**
```
B       code 34   [GATE, see note]          [GATE, see note]
(row 2) code ?    ........................  ................x.......
Sd      code  5?  x.....x.....x.....x.....  x.....x.....x.....x.....
Bd      code  3?  x.....x.....x.....x.....  x.....x.....x.....x.....
```

**Western**
```
C+Hb    code 31'  ............x...........  ............x...........
Hc      code 32   ........x...........x...  ........x...........x...
Lc'Bd   code  1   x.......................  x.......................
```

**Rock'n Roll**
```
Cy'     code  5   x..x..x..x..x..x..x..x..  x..x..x..x..x..x..x..x..
Sd      code 25+30 ......x..x........x.....  ......x..x........x.....
Bd'     code  3   x.....x.....x.....x.....  x.....x.....x.....x.....
Bd      code  1   x.......................  x.......................
```

**Slow Rock**
```
Cy'     code 2+9+33 x...x...x...x...x...x...  x...x...x...x...x...x...
Sd      code 31   ............x...........  ............x...........
Bd      code  1   x.......................  x.......................
```

**Fox Trot**
```
Cy      code 25   ......x...........x.....  ......x...........x.....
(lane2) code 33   ........................  ................x.......
Sd      code 25   ......x...........x.....  ......x...........x.....
Bd'     code  2   x...........x...........  x...........x...........
Bd      code  3   x.....x.....x.....x.....  x.....x.....x.....x.....
```

**Swing**
```
M               ......x...x.......x...x.  ......x...x.......x...x.
Cy              x...........x...........  x...........x...........
(lane3)         ........................  ................x.......
Sd              ......x...x.......x...x.  ......x...x.......x...x.
Bd              x.....x.....x.....x.....  x.....x.....x.....x.....
```

**Cha-Cha**
```
M               x..x..x..x..x..x..x..x..  x..x..x..x..x..x..x..x..
Cb              x.....x.....x.....x.....  x.....x.....x.....x.....
Hb              x.....x.....x...........  x.....x.....x...........
Lc              ..................x..x..  ..................x..x..
Bd              x........x........x.....  x........x........x.....
```

**March**
```
Sd              ......x...........x.....  ......x..x..x.....x.....
Cy              x...........x...........  x...........x...........
Bd              x...........x...........  x...........x...........
```

**Shuffle**
```
Sd              ....x.....x.....x.....x.  ....x.....x.....x.....x.
Sd'             x.....x.....x.....x.....  x.....x.....x.....x.....
Cy              x.....x.....x.....x.....  x.....x.....x.....x.....
Bd              x.....x.....x.....x.....  x.....x.....x.....x.....
```

**Tango**
```
M               x...........x...........  x...........x...........
Cy              .....................x..  .....................x..
Sd              .....................x..  .....................x..
Sd'             x.....x.....x.....x.....  x.....x.....x.....x.....
Bd              x.....x.....x.....x.....  x.....x.....x.....x.....
```

**SAMBA**
```
M     ...x..x..x..x..x..x..x..  x..x..x..x..x..x..x..x..
Hc    x.................x..x..  x.................x..x..
Lc    .........x..x...........  .........x..x...........
Bd    x.......................  x.......................
```

**RHUMBA**
```
L1  M     x..xx.x..x..x..x..x..x..  x..x..x..x..x..x..x..x..
L2  C     x........x........x.....  ......x.....x...........
L3  Hb    ...x..x.....x..x..x..x..  ...x..x.....x..x.....x..   [UNCERTAIN, 2 cells]
L4  Hc    x........x........x.....  x........x........x.....
L5  Lc    ..................x..x..  ..................x..x..
L6  Bd    x...........x...........  x...........x...........
```

**MAMBO** (labels unread, lane order by page position)
```
M?    x..x..x..x..x..x..x..x..  x..x..x..x..x..x..x..x..   (y2416)
Cb?   x.....x.....x.....x.....  x.....x.....x.....x.....   (y2458)
Hb?   x.....x..x..x...........  x..x..x..x..x...........   (y2553)
Lc?   ..................x..x..  ..................x..x..   (y2599)
Bd?   x........x..............  x........x..............   (y2645)
```

### 8.2 Roland TR-77, 18 rhythms
16 columns per bar (32 counter states, even half ruled). `/` = hatched, i.e. a state this rhythm
skips. Trigger numbers are the diode-matrix lines, quoted as printed.

**ROCK'N ROLL 1**
```
             cols   0 . 2 . 4 . 6 . 8 . 10. 12. 14.      (ruler 0,2,4,...,30)
CY      trig 14     ..x...x...x...x.
HH      trig 20     xxxxxxxxxxxxxxxx
SD      trig 11     ..xx..x...xx..x.
BD      trig  9     x..xx...x..xx...
```

**ROCK'N ROLL 2**
```
             cols   0 . 2 . 4 . 6 . 8 . 10. 12. 14.
CY      trig 14     ..x...x...x...x.
HH      trig 20     xxxxxxxxxxxxxxxx
SD      trig 25 + 40 ....x..x.x..x..x
BD      trig 18 + 33 x.x.....xxx.....
```

**SLOW ROCK**
```
             cols   0 . 2 . 4 . 6 . 8 . 10. 12. 14.
CY      trig 20     xxx/xxx/xxx/xxx/
SD      trig 40     .../x../.../x../      -> SD on cols 4 and 12
BD      trig 18     x../.../x../.../      -> BD on cols 0 and 8
     ("F.B." printed under the BD number)
```

**BALLAD**
```
             cols   0 . 2 . 4 . 6 . 8 . 10. 12. 14.
CY      trig 21     xxx/xxx/xxx/xxx/   + TWO extra off-grid marks, see below
SD      trig 40     .../x../.../x../      -> SD on cols 4 and 12
BD      trig 15' + 18'  x../..x/x../..x/  -> BD on cols 0, 6, 8, 14
     ("F.B." printed under the BD number)
```

**WESTERN**
```
             cols   0 . 2 . 4 . 6 . 8 . 10. 12. 14.
C+HB    trig 40     ....x.......x...
LB      trig 35     ..:...:...:...:.      <- see below, ALL FOUR ARE OFF-GRID
LC+BD   trig 18     x.......x.......
```

**6/8 MARCH**
```
             cols   0 . 2 . 4 . 6 . 8 . 10. 12. 14.
CY      trig 3      x../x../x../x../
SD      trig 14+40  ..x/x.x/..x/x.x/
BD      trig 3      x../x../x../x../
Me      trig 5      x../x../x../x../
```

**JAZZ WALTZ**
```
col      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
CY       x  ?  x  /  .  .  .  /  x  ?  x  /  .  .  .  /
SD       .  .  x  /  .  x  .  /  .  .  x  /  .  x  .  /
BD       x  .  .  /  .  .  .  /  x  .  .  /  .  .  .  /
```

**WALTZ**
```
col      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
CY       x  .  .  /  .  .  .  /  x  .  .  /  .  .  .  /
SD       .  .  x  /  .  x  .  /  .  .  x  /  .  x  .  /
BD       x  .  .  /  .  .  .  /  x  .  .  /  .  .  .  /
```

**RHUMBA**
```
M      trig 21      xxxxxxxxxxxxxxxx     (all 16 - maracas on every sixteenth)
C      trig 26      x..x..x...x.x...     (= 0,3,6,10,12 : the RUMBA CLAVE)
HB     trig 39      ..x..x....x..x..     (2,5,10,13)
LB     trig 8       xx.xx...xx.xx...     (0,1,3,4,8,9,11,12)
LC     trig 15      ......xx......xx     (6,7,14,15)
```

**BEGUINE**
```
M       trig 20     xxxxxxxxxxxxxxxx     (all 16)
C       trig 26     x..x..x...x.x...     (0,3,6,10,12 : rumba clave again)
HB      trig 1      .x.x.x.x.x.x.x.x     (ALL EIGHT ODD columns 1,3,5,7,9,11,13,15)
CY+LB   trig 4      .x.......x......     (1, 9)
[unlabelled rule at y 868.5]             (................  - completely EMPTY)
LC+BD   trig 2 (see note) x...x.x.x...x.x.     (0,4,6,8,12,14)
```

**CHA-CHA**
```
GU     trig 6(+12)  SUSTAINED BARS, not dots - see prose below
M      trig 20      x?x?xxxxxxxxxxxx     (0,2,4..15 certain; cols 1 and 3 UNCERTAIN)
CB+HB  trig 41      x.x.xxx.x.x.xxx.     (0,2,4,5,6,8,10,12,13,14)
LB     trig 15      ......xx......xx     (6,7,14 certain; col 15 faint but present)
LC     trig 17      x..x..x.x..x..x.     (0,3,6,8,11,14)
```

**MAMBO**
```
GU     trig 6(+12)  SUSTAINED BARS - same figure as CHA-CHA (see below)
M      trig 20      xxxxxxxxxxxxxxxx     (all 16 - every column verified by eye at high zoom)
CB+LB  trig 42'     x.?.x.x.x.x.x.x.     (0,4,6,8,10,12,14; col 2 UNCERTAIN)
HB     trig 1       .x.x.x.x.x.x.x.x     (ALL EIGHT ODD columns 1,3,5,7,9,11,13,15)
LC     trig 15      ......xx......xx     (6,7,14,15)
BD     trig 17      x..x..x.x..x..x.     (0,3,6,8,11,14 - the tumbao figure)
```

**SAMBA 1**
```
CY     trig 14      ..x...x...x...x.     (2,6,10,14 - the eighth-note UPBEATS)
M      trig 20      xxxxxxxxxxxxxxxx     (all 16; every column carries ink, scores 0.20-0.61)
HB     trig 37      x.x....x.x.x....     (0,2,7,9,11)
LB     trig 40      ....x.......x...     (4,12)
BD     trig 3       x...x...x...x...     (0,4,8,12 - four on the floor)
```

**SAMBA 2**
```
TB     trig 13         ..xx..xx..xx..xx     (2,3,6,7,10,11,14,15 - pairs on each beat's 2nd+3rd 16th)
TB'    trig 20(330K)   xxxxxxxxxxxxxxxx     (all 16)
CB+LB  trig 38         x.x.x..x.x.xx...     (0,2,4,7,9,11,12)
HB     trig 15         ......xx......xx     (6,7,14,15)
LC     trig 3          x...x...x...x...     (0,4,8,12)
```

**BOSSANOVA**
```
CY     trig 14      ..x...x...x...x.     (2,6,10,14 - eighth-note upbeats)
HH     trig 20      xxxxxxxxxxxxxxxx     (all 16)
RS     trig 27      x..x..x...x..x..     (0,3,6,10,13 : the BOSSA NOVA CLAVE)
BD     trig 9       x..xx...x..xx...     (0,3,4,8,11,12)
Me     trig 5       x...x...x...x...     (quarters)
```

**BAION**
```
TB'    trig 20(330K)  xxxxxxxxxxxxxxxx     (all 16)
CB+LB  trig 9         x..xx...x..xx...     (0,3,4,8,11,12 - identical to BOSSANOVA's BD, and the
                                            two share the trigger number 9)
HB     trig 15        ......xx......xx     (6,7,14,15)
LC     trig 23        ...........xx...     (11,12)
Me     trig 5         x...x...x...x...     (quarters)
```

**BOLERO**
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

**TANGO**
```
CY     trig 16      .......x.......x     (7, 15)
SD     trig 34      x.x.x.xxx.x.x.xx     (0,2,4,6,7,8,10,12,14,15)
BD     trig 42'     x.x.x.x.x.x.x.x.     (0,2,4,6,8,10,12,14 - straight eighths)
Me     trig 42 + 5  x.x.x.x.x.x.x.x.     (eighths)
```

### 8.3 SGS M252 rhythm LSI, both factory masks, 30 rhythms
FACTORY STANDARD MASKS, NOT ELGAM (§5). One character per counter state from 1 to the reset count.
`OUT n` are chip pins; the per-mask instrument assignment is in §4.

**TABLE 1 (M252 AA) — RHYTHM 1**
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

**TABLE 1 (M252 AA) — RHYTHM 2**
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

**TABLE 1 (M252 AA) — RHYTHM 3**
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

**TABLE 1 (M252 AA) — RHYTHM 4**
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

**TABLE 1 (M252 AA) — RHYTHM 5**
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

**TABLE 1 (M252 AA) — RHYTHM 6**
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

**TABLE 1 (M252 AA) — RHYTHM 7**
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

**TABLE 1 (M252 AA) — RHYTHM 8**
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

**TABLE 1 (M252 AA) — RHYTHM 9**
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

**TABLE 1 (M252 AA) — RHYTHM 10**
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

**TABLE 1 (M252 AA) — RHYTHM 11**
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

**TABLE 1 (M252 AA) — RHYTHM 12**
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

**TABLE 1 (M252 AA) — RHYTHM 13**
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

**TABLE 1 (M252 AA) — RHYTHM 14**
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

**TABLE 1 (M252 AA) — RHYTHM 15**
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

**TABLE 2 (M252 AD) — RHYTHM 1 (WALTZ)**
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

**TABLE 2 (M252 AD) — RHYTHM 2 (TANGO)**
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

**TABLE 2 (M252 AD) — RHYTHM 3 (MARCH)**
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

**TABLE 2 (M252 AD) — RHYTHM 4 (SWING)**
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

**TABLE 2 (M252 AD) — RHYTHM 5 (MAMBO)**
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

**TABLE 2 (M252 AD) — RHYTHM 6 (SLOW ROCK)**
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

**TABLE 2 (M252 AD) — RHYTHM 7 (BEAT)**
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

**TABLE 2 (M252 AD) — RHYTHM 8 (SAMBA)**
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

**TABLE 2 (M252 AD) — RHYTHM 9 (BOSSA NOVA)**
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

**TABLE 2 (M252 AD) — RHYTHM 10 (CHA-CHA)**
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

**TABLE 2 (M252 AD) — RHYTHM 11 (RUMBA)**
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

**TABLE 2 (M252 AD) — RHYTHM 12 (BEGUINE)**
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

**TABLE 2 (M252 AD) — RHYTHM 13 (BAJON)**
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

**TABLE 2 (M252 AD) — RHYTHM 14 (FOX TROT)**
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

**TABLE 2 (M252 AD) — RHYTHM 15 (SHUFFLE)**
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

### 8.4 SGS M253 rhythm LSI, factory mask AA, 12 rhythms
FACTORY STANDARD MASK, NOT ELGAM (§5). Unnamed in the databook: blocks are captioned only
"RHYTHM 1" to "RHYTHM 12". One character per counter state from 1 to the stated reset count.
`OUT n` are chip pins. Mask AC is NOT here: it duplicates M252 AD (§4c).

**TABLE 1 (M253 AA) — RHYTHM 1** (reset 32)
```
OUT 1    ..............x...............x.
OUT 2    x...x...x...x.x.x...x...x...x.x.
OUT 3    x...x...x...x.x.x...x...x...x.x.
OUT 4    ................................
OUT 5    ................................
OUT 6    ..............x...............x.
OUT 7    x.....x.x...x...x.....x.x...x...
OUT 8    ................................
```

**TABLE 1 (M253 AA) — RHYTHM 2** (reset 24)
```
OUT 1    ............x...........
OUT 2    x...........x...........
OUT 3    ....x...x.......x...x...
OUT 4    ........................
OUT 5    ........................
OUT 6    x...........x...........
OUT 7    ........................
OUT 8    ........................
```

**TABLE 1 (M253 AA) — RHYTHM 3** (reset 24)
```
OUT 1    ...x.....x.....x.....x..
OUT 2    x..x..x..x..x..x..x..x..
OUT 3    x.xx.xx.xx.xx.xx.xx.xx.x
OUT 4    ........................
OUT 5    ........................
OUT 6    ........................
OUT 7    x..x..x..x..x..x..x..x..
OUT 8    ........................
```

**TABLE 1 (M253 AA) — RHYTHM 4** (reset 32)
```
OUT 1    ........x...............x.......
OUT 2    x.......x.......x.......x.......
OUT 3    ....x.......x.......x.......xxxx
OUT 4    ................................
OUT 5    ................................
OUT 6    x.......x.......x.......x.......
OUT 7    ................................
OUT 8    ................................
```

**TABLE 1 (M253 AA) — RHYTHM 5** (reset 24)
```
OUT 1    ............x.........x.
OUT 2    x.........x.x.........x.
OUT 3    ......x...........x.....
OUT 4    ........................
OUT 5    ........................
OUT 6    ........................
OUT 7    x.x.x.x.x.x.x.xxx.x.x.x.
OUT 8    ........................
```

**TABLE 1 (M253 AA) — RHYTHM 6** (reset 32)
```
OUT 1    ........x...............x.......
OUT 2    x.......x.......x.......x.......
OUT 3    ....x.......x.......x.......x...
OUT 4    ................................
OUT 5    ................................
OUT 6    x.......x.......x...............
OUT 7    ....x..x....x..x.......xx..xx..x
OUT 8    ................................
```

**TABLE 1 (M253 AA) — RHYTHM 7** (reset 32)
```
OUT 1    ........x.x...x.........x.x.....
OUT 2    x.x.....x.x...x.x.x.....x.x...x.
OUT 3    ....x..x.x..x.......x..x.x..x..x
OUT 4    ................................
OUT 5    ................................
OUT 6    ................................
OUT 7    ....x.......x.......x.......x...
OUT 8    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
```

**TABLE 1 (M253 AA) — RHYTHM 8** (reset 32)
```
OUT 1    ........x...x...........x...x...
OUT 2    x.....x.x...x...x.....x.x...x...
OUT 3    x.....x.....x.......x...x.......
OUT 4    ....x.....x...x.....x.....x...x.
OUT 5    x.x...x.x...x...x.x...x.x...x...
OUT 6    ................................
OUT 7    ................................
OUT 8    x.xxx.x.x.x.x.x.x.xxx.x.x.x.x.x.
```

**TABLE 1 (M253 AA) — RHYTHM 9** (reset 32)
```
OUT 1    ........x...x...........x...x...
OUT 2    x.......x...x...x.......x...x...
OUT 3    ..x...x...x...x...x...x...x...x.
OUT 4    ..x...x...x...x...x...x...x...x.
OUT 5    ..x...............x.............
OUT 6    ......x...............x.........
OUT 7    ..xxx.............xxx...........
OUT 8    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
```

**TABLE 1 (M253 AA) — RHYTHM 10** (reset 32)
```
OUT 1    ......x.....x.........x.....x...
OUT 2    x.....x.....x...x.....x.....x...
OUT 3    x...x...x.x.x...x...x...x.x.x...
OUT 4    x...x...x...x...x...x...x...x...
OUT 5    ............x.x.............x.x.
OUT 6    ................................
OUT 7    ................................
OUT 8    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
```

**TABLE 1 (M253 AA) — RHYTHM 11** (reset 32)
```
OUT 1    ........x...............x.......
OUT 2    x.......x.......x.......x.......
OUT 3    ..x...x...xx..x...x...x...xx..x.
OUT 4    x...x...........x.x...x.........
OUT 5    ........x.....x.........x...x...
OUT 6    ................................
OUT 7    ....x.......x.......x.......x...
OUT 8    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
```

**TABLE 1 (M253 AA) — RHYTHM 12** (reset 32)
```
OUT 1    ........x.....x.........x.....x.
OUT 2    x.....x.x.....x.x.....x.x.....x.
OUT 3    x.....x.....x.......x.....x.....
OUT 4    ................................
OUT 5    ................................
OUT 6    ................................
OUT 7    ....x.x.....x.x.....x.x.....x.x.
OUT 8    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
```

### 8.5 Hammond organ rhythm system (US patent 3,567,838, FIG. 2), 3 rhythms
48 pulses = TWO measures of 24. Rows are 24 characters for measure 1, two spaces, 24 for measure 2.
`CHORD`, `HIGH BASS` and `LOW BASS` are **gate lanes over the notes the player is holding**, not
drums and not note sequences (§4d). A lane whose label joins two names is ONE programmer lead
driving both. Only three rhythms are charted and the patent disclaims novelty in the programmer,
so read these as representative content rather than a model's full dial.

**LATIN**
```
TEMPLE BLOCK           x.....x..x........x..x..  ......x..x........x..x..
WOOD BLOCK             x........x........x.....  ......x.....x...........
BRUSH                  x..x........x..x........  x..x........x..x........
CHORD & SNARE DRUM     ....x.x..x.....x..x..x..  ....x.x..x.....x..x..x..
BASS DRUM              x...........x.....x.....  x...........x.....x.....
HIGH BASS              ............x...........  ............x...........
LOW BASS               x.......................  x.......................
```
**ROCK**
```
CYMBAL                 x..x..x..x..x..x..x..x..  x..x..x..x..x..x..x..x..
BRUSH                  ......x...........x.....  ......x...........x.....
CHORD & SNARE DRUM     ......x...........x..x..  ......x...........x..x..
BASS DRUM              x........x..x...........  x........x..x...........
HIGH BASS              ............x...........  ............x...........
LOW BASS               x.......................  x.......................
```
**WALTZ**
```
CYMBAL                 ......x.x...............  ......x.x...............
CHORD & SNARE DRUM     ........x.......x.......  ........x.......x.......
LOW BASS & BASS DRUM   x.......................  x.......................
```


### 8.6 SGS M255 mask B1-AB, 6 rhythms, with the pins NAMED
The only table in this collection whose lanes and rhythms are both named on the chip's own pinout
(printed p146, generic pinout beside the "Standard content configuration" for this exact mask):
RHYTHM 1..6 = WALTZ, BEAT, SWING, COUNTRY WESTERN, LATIN, TANGO; OUTPUT 1..5 = BASS DRUM /
FUNDAMENTAL, SNARE DRUM, SHORT CYMBALS, FIFTH, CHORD TRIGGER. Lane names below are those, not
OUTPUT numbers. So FIFTH and CHORD TRIGGER are accompaniment lanes and OUTPUT 1 drives the kick
AND the root note from one lead.
Lengths are 12 or 16 elementary times, read from the greying, not assumed. The waltz decodes as a
literal oom-pah-pah: root on time 1, FIFTH on 7, snare and chord together on 3, 5, 9 and 11.
One inference is flagged rather than hidden: that pinout "RHYTHM n" is the same n as the truth
table's group. It checks out three ways (the 12-time rhythms are exactly the three plausible 3/4
ones, p147 says 3/4 rhythms reset at state 12, and the waltz reads correctly through the names).

**M255 RHYTHM 1 (WALTZ)** length 12
```
BASS DRUM/FUNDAMENTAL  x.....x.....
SNARE DRUM             ..x.x...x.x.
SHORT CYMBALS          x.....x.....
FIFTH                  ......x.....
CHORD TRIGGER          ..x.x...x.x.
```
**M255 RHYTHM 2 (BEAT)** length 16
```
BASS DRUM/FUNDAMENTAL  x..xx...xx.xx.xx
SNARE DRUM             ..x...x...x..x.x
SHORT CYMBALS          xxxxxxxxxxxxxxxx
FIFTH                  ...............x
CHORD TRIGGER          x.x..x..x..x....
```
**M255 RHYTHM 3 (SWING)** length 12
```
BASS DRUM/FUNDAMENTAL  x..x..x..x..
SNARE DRUM             ...x.....x..
SHORT CYMBALS          x..x.xx..x.x
FIFTH                  ......x..x..
CHORD TRIGGER          ...x.....x..
```
**M255 RHYTHM 4 (COUNTRY WESTERN)** length 12
```
BASS DRUM/FUNDAMENTAL  x....xx....x
SNARE DRUM             ...x.....x..
SHORT CYMBALS          xxxxxxxxxxxx
FIFTH                  ......x....x
CHORD TRIGGER          ...x.....x..
```
**M255 RHYTHM 5 (LATIN)** length 16
```
BASS DRUM/FUNDAMENTAL  x.xx..x.x.....xx
SNARE DRUM             x..x..x...x..x..
SHORT CYMBALS          xxxxxxxxxxxxxxxx
FIFTH                  ......x.......xx
CHORD TRIGGER          .x.x.xx.x.x.xx.x
```
**M255 RHYTHM 6 (TANGO)** length 16
```
BASS DRUM/FUNDAMENTAL  x...x...x...x.x.
SNARE DRUM             x...x...x...x.x.
SHORT CYMBALS          ..............x.
FIFTH                  ..............x.
CHORD TRIGGER          x...x...x...x.x.
```

### 8.7 SGS M254 masks AD and AM, 16 rhythms, mostly ACCOMPANIMENT
32 elementary times, 12 outputs, two masks. Names printed on the pages:
AD = WALTZ, TANGO, SWING, BEAT, BOSSA NOVA, SAMBA, RUMBA, SLOW ROCK;
AM = WALTZ, POLKA, TANGO, BOSSA NOVA, SAMBA, SLOW ROCK, BOOGIE, DISCO.
**These are mostly not drum lanes.** The M254 B1AD pinout (printed p145) sends EIGHT of the twelve
outputs to the M251 accompaniment chip as `I2..I8` plus `TRIGGER CHORDS`, and only four to
percussion, so most of what follows is chord, bass and arpeggio content (§4e). Which column group
is which needs that pinout page, so lanes are left as printed OUTPUT numbers here rather than
guessed at.
Lengths come from the counting-control group by the databook's own rule (a cross at count N means
N elementary times, no cross means 32), corroborated by the greying and by the empty tails.
A printing inconsistency, recorded as printed: the counting-control group is headed "RHYTHM 1..12"
on one page and "OUTPUT 1..12" on the next.

**M254 AD 1 (WALTZ)** length 24
```
OUTPUT 1    x...........x...........
OUTPUT 2    ....x...x......xx...x...
OUTPUT 3    x...........x...........
OUTPUT 4    ........................
OUTPUT 5    ............x...........
OUTPUT 6    ....x...x...x...x...x...
OUTPUT 7    x.......x.......x.......
OUTPUT 8    ....x...x.......x...x...
OUTPUT 9    x.x.x.x.x.......x.x.x.x.
OUTPUT 10   x.x.......x.x.x.......x.
OUTPUT 11   x...x.x...x.x.x...x.x...
OUTPUT 12   ..x.x...x.x...x.x...x.x.
```
**M254 AD 2 (TANGO)** length 32
```
OUTPUT 1    x.......x.......x.......x.......
OUTPUT 2    x.......x.......x.......x...xxxx
OUTPUT 3    ............................x...
OUTPUT 4    ................................
OUTPUT 5    ................................
OUTPUT 6    ........x.......x.......x...x...
OUTPUT 7    x...............x...........x...
OUTPUT 8    x.......x.......x.......x...x...
OUTPUT 9    x.x.x.x.x...x.x.x...x.x...x.x.x.
OUTPUT 10   x...x.x.......x.........x.......
OUTPUT 11   x.x...x.x...x...x...x.x.x...x.x.
OUTPUT 12   ..x.x...x...x.x.......x.x.x...x.
```
**M254 AD 3 (SWING)** length 32
```
OUTPUT 1    x.......x.......x.......x.......
OUTPUT 2    ....x.......x.......x......xx..x
OUTPUT 3    x...x..xx...x..xx...x..xx..xx..x
OUTPUT 4    ................................
OUTPUT 5    ............x...x...x...........
OUTPUT 6    ....x...x.......x.......x...x...
OUTPUT 7    x.......x...............x.......
OUTPUT 8    ....x.......x.......x.......x...
OUTPUT 9    x.x.x.x.x.x.x.x.x...x.x...x.x...
OUTPUT 10   x...x.x...x.x.....x.....x.....x.
OUTPUT 11   x.x...x.x.x...x.x.x...x.x.x...x.
OUTPUT 12   ..x.x...x...x.x...x.x...x...x.x.
```
**M254 AD 4 (BEAT)** length 32
```
OUTPUT 1    x.....x.x.......x.x...x.x.....x.
OUTPUT 2    ....x.......x.......x.....x...x.
OUTPUT 3    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUTPUT 4    ................................
OUTPUT 5    x...x..x........x...x..x........
OUTPUT 6    x...x..x....x.x.x...x..x....x.x.
OUTPUT 7    ........x.x...x.........x.x...x.
OUTPUT 8    x...x.....x.....x.....x.........
OUTPUT 9    x.x.x.x.x.x...x.x.x.x.xxx.x.x.x.
OUTPUT 10   x.x.x.......x..........x..x.x.x.
OUTPUT 11   x.x...x.x...x...x...x.x.x...x...
OUTPUT 12   x...x.x...x.x.x...x...xxx.x...x.
```
**M254 AD 5 (BOSSA NOVA)** length 32
```
OUTPUT 1    x.....x.x.....x.x.......x...x.x.
OUTPUT 2    x.....x.....x.......x.....x.....
OUTPUT 3    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUTPUT 4    ....x.......x.......x.......x...
OUTPUT 5    x...........x...x...............
OUTPUT 6    x.....x...x.x.x.x.x...x...x.x...
OUTPUT 7    ..x.......x...x...x.......x...x.
OUTPUT 8    x...x...x.x...x...x...x...x.x...
OUTPUT 9    x...x.x.x.x.x.....x...x...x.x.x.
OUTPUT 10   x.....x.......x...............x.
OUTPUT 11   x...x...x.x...x...........x.x...
OUTPUT 12   ....x.x...x.x.x...x...x.....x.x.
```
**M254 AD 6 (SAMBA)** length 32
```
OUTPUT 1    x...x...x.....x.x.x...x.x.......
OUTPUT 2    ..x...x...xx..x...x...x...xx..x.
OUTPUT 3    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUTPUT 4    x...x...x.....x.x.x...x.x...x...
OUTPUT 5    ................x.....x.........
OUTPUT 6    ......x.x...x...x...x.x...x...x.
OUTPUT 7    x.......x...........x.........x.
OUTPUT 8    x...x...x...x...x.x...x.x...x.x.
OUTPUT 9    x.x.x.x.x.x.x.xx..x.x.x...x.x.x.
OUTPUT 10   x.x...x.........x.............x.
OUTPUT 11   x...x...x.x.x.x.x...x.....x.x...
OUTPUT 12   ..x.x.x.x...x..xx.x...x.....x.x.
```
**M254 AD 7 (RUMBA)** length 32
```
OUTPUT 1    x.....x.....x...x.....x.....x...
OUTPUT 2    x.....x.....x.......x...x.......
OUTPUT 3    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUTPUT 4    ....x...x.x...x.x.....x...x.x.x.
OUTPUT 5    ................x...........x...
OUTPUT 6    ......x.....x...x.....x.....x...
OUTPUT 7    x...........x.........x.........
OUTPUT 8    ....x...x.x.....x.....x...x.....
OUTPUT 9    x..xx.x...x.x.x...x.x.x.x.x.x.x.
OUTPUT 10   x...x...........x...........x.x.
OUTPUT 11   x..x..x...x...x.x...x...x.x...x.
OUTPUT 12   x..xx.....x.x...x.x...x...x.x...
```
**M254 AD 8 (SLOW ROCK)** length 24
```
OUTPUT 1    x.........x.x.........x.
OUTPUT 2    ......x...........x.....
OUTPUT 3    x.x.x...x.x.x.x.x...x.x.
OUTPUT 4    ........................
OUTPUT 5    ............x...........
OUTPUT 6    ......x...x.x.....x...x.
OUTPUT 7    x.........x...........x.
OUTPUT 8    ......x...........x.....
OUTPUT 9    x.x.x.x.x.x...x.x.x.x.x.
OUTPUT 10   x.x.x.......x.......x.x.
OUTPUT 11   x.x...x.x...x...x.x...x.
OUTPUT 12   x...x.x...x.x.x...x.x...
```
**M254 AM 1 (WALTZ)** length 24
```
OUTPUT 1    x.....x.....x.....x.....
OUTPUT 2    ........................
OUTPUT 3    ........................
OUTPUT 4    ........................
OUTPUT 5    ..x.x...x.x...x.x...x.x.
OUTPUT 6    x.....x.....x.....x.....
OUTPUT 7    ..x.x...x.x...x.x...x.x.
OUTPUT 8    x.....x.....x.....x.....
OUTPUT 9    ......x...........x.....
OUTPUT 10   ........................
OUTPUT 11   x...........x...........
OUTPUT 12   ..x.x...x.x...x.x...x.x.
```
**M254 AM 2 (POLKA)** length 32
```
OUTPUT 1    x...x...x...x...x...x...x...x...
OUTPUT 2    ................................
OUTPUT 3    ................................
OUTPUT 4    ................................
OUTPUT 5    ..x...x...x...xx..x...x...xxx.x.
OUTPUT 6    x...x...x...x...x...x...x.......
OUTPUT 7    ..x...x...x...x...x...x...x...x.
OUTPUT 8    x...x...x...x...x...x...x...x...
OUTPUT 9    ....x.......x.......x.......x...
OUTPUT 10   ................................
OUTPUT 11   x.......x.......x.......x.......
OUTPUT 12   ..x...x...x...x...x...x...x...x.
```
**M254 AM 3 (TANGO)** length 32
```
OUTPUT 1    x...x...x...x...x...x...x...x...
OUTPUT 2    ................................
OUTPUT 3    ................................
OUTPUT 4    ................................
OUTPUT 5    x...x...x...x.xxx...x...x...x.x.
OUTPUT 6    ..............x...............x.
OUTPUT 7    x...x...x...x.x.x...x...x...x.x.
OUTPUT 8    x...............x...............
OUTPUT 9    ........x...x...........x...x...
OUTPUT 10   ....x.......x.......x.......x...
OUTPUT 11   x...........x...x...........x...
OUTPUT 12   x...x...x...x.x.x...x...x...x.x.
```
**M254 AM 4 (BOSSA NOVA)** length 32
```
OUTPUT 1    x.......x.....x.x.......x.....x.
OUTPUT 2    x.....x.....x.......x.....x.....
OUTPUT 3    ......x.x.............x...x.....
OUTPUT 4    x.............x.x.............x.
OUTPUT 5    ................................
OUTPUT 6    ................................
OUTPUT 7    x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.
OUTPUT 8    x...............x...............
OUTPUT 9    ........x...............x.......
OUTPUT 10   ................................
OUTPUT 11   x...............x...............
OUTPUT 12   x.....x.....x.......x.....x.....
```
**M254 AM 5 (SAMBA)** length 32
```
OUTPUT 1    x...x...x...x...x...x...x...x...
OUTPUT 2    x...x..x........x...x..x....x...
OUTPUT 3    ...xx.......x......xx.......x...
OUTPUT 4    x......xxx.....xx......xxx.....x
OUTPUT 5    x.x.x..x.x.xx...x.x.x..x.x.xx...
OUTPUT 6    ..x...x...x...x...x...x...x...x.
OUTPUT 7    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
OUTPUT 8    x...............x...............
OUTPUT 9    ....x.......x.......x.......x...
OUTPUT 10   ................................
OUTPUT 11   x.......x.......x.......x.......
OUTPUT 12   ..x...x...x...x...x...x...x...x.
```
**M254 AM 6 (SLOW ROCK)** length 24
```
OUTPUT 1    x.........x.x.........x.
OUTPUT 2    ........................
OUTPUT 3    ........................
OUTPUT 4    ........................
OUTPUT 5    ......x...........x.....
OUTPUT 6    x.....x.....x.....x.....
OUTPUT 7    x.x.x.x.x.x.x.x.x.x.x.x.
OUTPUT 8    x...........x...........
OUTPUT 9    ............x.........x.
OUTPUT 10   ..........x.......x.....
OUTPUT 11   x.......................
OUTPUT 12   x.x.x.x.x.x.x.x.x.x.x.x.
```
**M254 AM 7 (BOOGIE)** length 24
```
OUTPUT 1    x.....x.....x.....x.....
OUTPUT 2    ........................
OUTPUT 3    ........................
OUTPUT 4    ........................
OUTPUT 5    ...x.....x.....x....xx..
OUTPUT 6    x..x..x..x..x..x..x..x..
OUTPUT 7    ..x..x..x..x..x..x..x..x
OUTPUT 8    x...........x...........
OUTPUT 9    ......x..x..x..x..x.....
OUTPUT 10   ...x........x........x..
OUTPUT 11   x........x.....x........
OUTPUT 12   ..x..x..x..x..x..x..x..x
```
**M254 AM 8 (DISCO)** length 32
```
OUTPUT 1    x...x...x...x...x...x...x...x...
OUTPUT 2    ................................
OUTPUT 3    ...x.......xx......x.......xx...
OUTPUT 4    x.....x.......x.x.....x.......x.
OUTPUT 5    ....x.......x.......x.......x...
OUTPUT 6    ..x...x...x...x...x...x...x...x.
OUTPUT 7    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
OUTPUT 8    x...............x...............
OUTPUT 9    ....x.x.....x.x.....x.x.....x.x.
OUTPUT 10   ......x.......x.......x.......x.
OUTPUT 11   x.....x.x.....x.x.....x.x.....x.
OUTPUT 12   ..x...x...x...x...x...x...x...x.
```

## 9. Open items

- ~~A cart.~~ **DONE (2026-08-20):** `autorhythm` (tools/carts/autorhythm.c). A dial of machines, a
  dial of rhythms and one clock knob, with the mechanism on screen instead of a step grid: skipped
  states are struck through and really are stepped over, marks that cannot retrigger are drawn
  hollow, gate lanes are held, and the tempo control is the machine's own variable CLOCK in ticks
  per second with the BPM derived rather than set. Its `spec()` asserts the four sequencing
  properties over all 76 rhythms plus the FR-2L unruled-count invariant (25 assertions). The voices
  are `sideman.h` and are NOT sourced; the cart says so on screen, and the label-hint mapping it
  uses is written up in [`instrument-recipes.md`](../guides/instrument-recipes.md).
- ~~A generated header.~~ **DONE (2026-08-20):** [`runtime/rhythmbox.h`](../../runtime/rhythmbox.h),
  generated from THIS DOC by `node tools/gen-rhythmbox.js`, so the doc stays the single source of
  truth and the header carries only the bits. A lane is a 48-bit mask over counts; per-rhythm
  subdivision, skipped counts and gate lanes are all modelled (§3.1, §3.5). The generator refuses to
  write a partial header, carries 14 known answers taken from the findings stated in this doc's
  prose, and is gated by `repo-doctor`. It caught two errors on its first run: two 25-character rows
  in a 24-count bar (fixed here), and a first version of itself that stamped every FR-2L rhythm with
  a 6-count beat, which would have played the waltz wrong.
- **`sideman`'s twelve rhythms are reconstructions** and should be labelled as such in
  [`sideman.md`](sideman.md), since ten of their twelve names now have sourced data here.
- ~~M253 tables.~~ **DONE (2026-08-20):** both masks transcribed and cross-checked against the M252 (§4c). Mask AA's twelve rhythms are in §8.4 and in the header; mask AC's twelve are duplicates of M252 AD and are recorded in the transcription log only.
- ~~SGS Technical Note no. 131.~~ **CHASED (2026-08-20), not digitised.** The databook cites it four
  times ("TECHNICAL NOTE NO 131 AVAILABLE FOR FULL INFORMATION", "available on request"), so it is a
  separate document, and it is not on bitsavers (whose SGS tree holds exactly one application note,
  a 1995 EEPROM part) nor findable on archive.org. Most of what it was cited FOR is now recovered
  anyway, from the databook's own pages: see §4a. The remaining gap is the suggested instrument
  generator circuits, i.e. the voice side. The practical substitute is **Elektor**, which covered
  these chips twice (1975-07 and 1976-04) "including their connection to simple instrument generator
  circuits suggested in the SGS application notes". **Both issues were then obtained** from the
  Internet Archive rather than Elektor's paywalled archive, and §4b records what they say. Between
  §4a and §4b the note's cited content is effectively recovered; the note itself remains
  un-digitised.
- **The M254's accompaniment content** (§4e) is the best remaining target in the whole project: a
  published chord/bass/arpeggio table for eight named rhythms, in a databook already on disk.
  Reading it needs the pinout page (printed p145) beside the truth table (pp150-151), because which
  column group is music and which is drums is decided by the pinout.
- **FIG. 14 of US4292874** is unfinished business (§4g): its bass line is notated on a STAFF at the
  top of the same figure, which is a far better route than the staircase trace that defeated the
  first attempt. Anyone retrying should read the notation and check it against the patent's
  damp-one-step-before-trigger rule, which is what caught the bad reading.
- ~~M255, M258/M259, M108.~~ **DONE (2026-08-20):** M255's 6 rhythms are in §8.6 with their pins
  named; M258/M259 turned out to publish NO content but to explain the edge rule (§4f); the M108's
  bass decoder is in §4f.
- **Elgam's own masks** would need a ROM dump off a surviving `M252 D1 AE`/`AF`, or transcription
  from recordings.
