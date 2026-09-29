# What loficity's arranger teaches the radio stations

STATUS: BUILDING (2026-09-29): phase 0 landed on `lofi.c` (the `radio.h` sample-clock grid + its gate); the other 38 stations migrate next, then the phase 1 arranger pilot.

`loficity` (2026-09-28) is a line-for-line port of Lofi Cities' arranger
([`runtime/loficity/arranger.h`](../../runtime/loficity/arranger.h), played by
[`tools/carts/loficity.c`](../../tools/carts/loficity.c)). Played next to our own radio stations it
simply sounds *better*, and the reason is mostly not the sound. This doc is the audit of **why**
(section 1), a scan of where our 39 `radio.h` stations stand on each point (section 2), and a phased
plan for carrying the lessons over without breaking what makes each station itself (section 3).

Related: [`radio-genre-fidelity.md`](radio-genre-fidelity.md) (the per-station gap ledger, which is
about *instruments and brains*; this one is about *arrangement*),
[`audio-timing.md`](audio-timing.md) (the sample-clock fix, section 2b),
[`harmony-brain.md`](harmony-brain.md) (progressions), [`yacht-rack.md`](yacht-rack.md) (the one
place we already built comp anticipation), and [`lofi-blind-brief.md`](lofi-blind-brief.md).

## 1. The audit: seven differences, biggest first

The comparison baseline is `tools/carts/lofi.c`, our lofi radio station (same genre, so the gap is
about method, not idiom). Findings 1 to 3 do not depend on the sound engine at all.

### 1.1 It plans with look-ahead; our stations roll per step

Our stations are **step players**: `rad_clock_step()` hands `play_step(abs, pos)` one 16th at a time
and the step asks "what happens *now*?", answering with `chance(35)`-style coin flips. It has no
future to look at.

Lofi Cities plans a **whole bar** (`plan_bar`) while holding the **next** bar's chords and layers:

- **The push.** When the next bar's first chord differs, the keys play it *early*, just before the
  barline (`P->ep.push` odds from the style's energy row), and the next bar skips its downbeat hit.
  This is what a real comper does and the single most "human" gesture in the port.
- **Fills** land in the bar *before* a section change.
- **`dipLast`**: the last bar of a section sometimes drops the master tone to 500 Hz, a breath before
  the next section.
- **`hatsFirst`**: a B section can bring the hats in ahead of the rest.

Anticipation reads as intention. A step player structurally cannot do it.

### 1.2 A song form, with per-layer roles per section

`plan_track` picks one of three **forms** (intro / A / B / break / outro, `FORMS[3]`), grows it with
extra B+A pairs until it lasts 2.5 to 4 minutes, and then the track **ends**. The next track moves to
a **related key** (`related_key`: often up a fourth or fifth, or to the relative major/minor).

Each section sets every layer separately (`Layers`: `ep`, `bass`, `drums`, `lead`, `level`):
the bass holds whole notes in the break (`BS_WHOLE`), the drums go hats-only in the intro
(`DR_HATS2`), the lead only enters from the Nth A section (`leadFrom`), the outro has its own bass
and drum modes.

`lofi.c` has no form: one loop, endlessly, at one density. Where our stations do have form
(`rad_level`), it is a single **density number** per section, not a decision per part.

### 1.3 Two progressions and a turnaround

The A section and the B section play **different progressions** from the style's bank, or (40% of
the time) B reuses A with a **ii-V turnaround** spliced into its last 4 beats (`lc_expand` with
`turn`). Either way B contrasts with A and there is a pull back to the top. `chordBars` (1 or 2 bars
per chord) is a per-track roll too. `lofi.c` has one 2 to 4 chord loop.

### 1.4 The melody is a developed motif, not a cell with dropouts

`make_motif` + `lc_phrase` build a phrase the way a player does:

- a **motif** (rhythm cell + tail + contour) stated over the first half of a 4-bar window;
- a **variant** per statement: inverted contour, shifted start, and every third statement bends its
  last note and adds a pickup;
- an **answer** phrase in the second half, from its own rhythm bank, ending on a chord tone;
- **strong beats snapped to chord tones** (`s_chord_tone` at steps 0 and 8 of each half);
- **register continuity**: each phrase starts near the last note of the previous one
  (`prevLast`), pentatonic, clamped to MIDI 67 to 84.

`lofi.c`'s "dab" is a fixed 2 to 5 note cell where each note plays with 62% chance: random gaps, not
phrasing. `improv.h` does real motif development, but only 8 stations use it and only for solos.

### 1.5 The mix is automated

Master tone events per bar: the intro opens the filter from 900 Hz to the track's tone over the
section, the break dips to 1800 Hz, the tone drifts ±12% every 4 bars, the outro closes to 700 Hz,
the vinyl swells at the intro and outro. The track has a shape you hear even with your eyes closed.
Ours is set-and-hold per song (correctly, for effects that rebuild their DSP, but `filter()` and the
`note_*` family are built to be ridden and we rarely ride them).

### 1.6 Timing on the sample clock

The port books every note with `audio_time()` + `schedule_at()` (commit `9ec14d85`, made for this
cart). The stations compute a delay from `beat()` (which advances by the clamped frame dt) and call
`schedule_hit(dly, …)`, whose delay counts from whichever audio callback drains the request: up to a
23 ms buffer of jitter on native. `tools/schedule-check` measured the old way at 785 samples of
click-gap error against 0 for the new one.

So `lofi.c`'s "drunk pocket" is intentional drag stacked on **unintentional** jitter. In Lofi
Cities swing and snare lag are exact, and the humanise is a controlled Gaussian per voice (`at_()`:
sum of three uniforms times a per-part sigma), so the pocket reads as a choice.

### 1.7 Randomness is spent on the plan, not while it plays

Their seed decides key, progressions, form, comp weights, groove, motif; the bar then plays that
plan out. Each concern draws from **its own seeded stream** (`lc_stream(seed, k)`: harmony 1, rhythm
2, melody 3, arrangement 4, title 5, per-bar streams 11 to 17), so adding a decision to one concern
never reshuffles another. Ours spread `chance()`/`rnd()` across every step, so parts flicker instead
of committing, and all composition draws share one `rad_srnd` sequence (see the seed rule below).

Last, and not portable: someone tuned Lofi Cities' numbers by listening for a long time (the energy
tables, push odds, comp weights). The port inherits that; we should not copy the numbers into other
stations (they are theirs, and they are lofi numbers), only the **method**.

## 2. Where the 39 `radio.h` stations stand

Scanned 2026-09-29 by grepping each cart (a candidate list, not a verdict: read a cart before
changing it).

| lesson | stations that have it | notes |
|---|---|---|
| 1.6 sample-clock timing | **0 of 39** | every beat station uses `schedule_hit`; `schedule_at` is one day old |
| 1.2 song form | ~18 (`rad_level`: addis, air, afrobeat, cocktail, exotica, house, italo, mariachi, modaljazz, napoleon, plantasia, polopan, roadhouse, tango, thexx, wba, yacht, yachtrack) | density-only; none sets per-layer roles, none ends a track into a related key |
| 1.4 motif development | 8 via `improv.h` (addis, afrobeat, cocktail, mariachi, modaljazz, motorik, roadhouse, squarepusher) | solos only; the "main" melody is usually a cell |
| 1.3 functional progressions | 4 via `harmony.h` (bossa, bossabloom, cocktail, modaljazz) | none has an A/B pair or turnaround splice |
| 1.1 look-ahead (push, fills) | ~0 in stations (yachtrack's rack has comp anticipation) | needs a bar planner |
| 1.5 mix automation | ad hoc in a few | no shared idiom |

Beatless or odd-meter stations (`ambient`, `eno`, `satie` in 3/4, `gamelan`) are out of scope for the
bar planner as-is; they may take 1.2/1.5 only.

## 3. The plan

Constraints that shape every phase:

- **`radio.h` is a toolkit, not a framework** (its own header): new blocks the cart *calls*, never a
  base class that owns `update()`. Stations keep their idiom; a bossa and a house station should not
  converge on one arrangement.
- **The seed-compatibility rule** (`radio.h` header): a pinned seed IS the song and a composition is
  exactly the sequence of `rad_srnd()` calls. So every new composition decision draws from a
  **separate derived stream** (lesson 1.7), e.g. `rad_stream(seed, k)` hashing the seed. The existing
  `rad_srnd` sequence stays untouched: a pinned seed keeps its key, mood, loop and title. Its
  *arrangement* will change once a station gains form, and that is an intended change, logged in the
  station's commit, not a silent break.
- **Performance vs composition** stays as `radio.h` defines it: humanise may use `rnd()`.

### Phase 0: sample-clock timing for every station (mechanical, highest ratio)

Goal: lesson 1.6 across all 39 without touching composition.

1. Add to `radio.h` a clock anchor on `audio_time()`: `RadioClock` gains `t0` (audio seconds at the
   song's step 0) and the step period in seconds; `rad_step_time(&clk, abs)` returns the absolute
   audio time of a step, and `rad_hit(&clk, abs, off_ms, midi, instr, vol, dur)` wraps
   `schedule_at(rad_step_time(...) + off_ms/1000, ...)`.
2. Tempo changes (UP/DOWN, sync) **rebase** `t0` at the current step so the grid stays continuous.
   Open question to settle on the pilot: how `rad_clock_step`'s look-ahead window is measured once
   the clock is audio time rather than `beat()` (loficity uses 100 ms ahead; copy that).
3. Migrate `lofi.c` first, then the rest: `schedule_hit(dly + X, …)` becomes `rad_hit(&clk, abs, X, …)`.
   Mostly a mechanical rewrite per cart; the feel offsets (`snDrag`, `swing`) carry over as `off_ms`.
4. Gates: `tools/schedule-check` style probe on one station (timing error ~0); `play.js … --trace`
   `watch()` values unchanged per cart (composition untouched, the seed rule holds);
   `build-all.js`. Ear check: `lofi.c`'s pocket should now read as deliberate.

### Phase 1: pilot the arranger on `lofi.c`

Goal: lessons 1.1 to 1.3 and 1.5 in one station, A/B-able directly against `loficity`.

1. **Form + layers**: a small `Section { name, bars, prog, Layers }` array planned at `new_song`
   from a new stream; intro / A / B / break / outro with per-part modes (keys hold vs comp, bass
   off / whole / walking, drums none / hats / full, dab off / on). The track ends and the next song
   moves to a related key (the station already has `[ ]` history, so "next" stays seeded).
2. **A/B progressions + turnaround**: pick B from the station's own `LOOPS[]`, or splice a ii-V
   turnaround into A's last bar.
3. **A bar planner** (`plan_bar(bar, next)`), run once per bar a little ahead of it, emitting timed
   events that the step clock then books. It sees the next bar, so: the **push** (comp the next chord
   on the and-of-4), a **fill** before a section change, **dipLast**.
4. **Tone automation** through `filter()` (ridden, not set-and-hold): intro opens, break dips,
   outro closes.
5. Deliverable: `lofi.c` baked, with a note in its `de:meta` lineage; a `tools/clips/lofi/` recipe;
   the owner A/Bs it against `loficity`.

### Phase 2: extract the shared blocks (on the second customer)

Same extract-on-second-user rule that produced `radio.h` and `improv.h`. Candidate second customers:
`house` (loficity has a house style to A/B against) and `bossa` (already has `harmony.h`, so it tests
the A/B-progression block on a Markov chart rather than a fixed loop).

Likely shape, all in `radio.h` or a sibling `radarr.h` (decide at extraction):

- `rad_stream(seed, k)`: derived composition streams (lesson 1.7).
- `RadForm` / `rad_form_plan()`: form templates + growth to a target length; per-section `Layers`
  is **cart-defined** (each station names its own parts), the header owns only the section clock.
- `rad_bar_next()`: the bar-planning hook with current + next bar info, so push/fill/dip are cart
  logic written against a shared "what's next" query.
- `rad_prog_pair()`: A/B + turnaround splice over a cart's chord list.
- `rad_tone_ride()`: the section-shaped filter automation.

Then roll out, most payoff first: the ~21 stations with **no form at all** (lofi, citypop, dub,
jangle, jingle, lowend, vapor, ymo, plaid, braindance, …), then upgrade the 18 density-only ones to
per-layer roles.

### Phase 3: phrasing for the main melody

Lesson 1.4 for the tune, not only the solo. Options, decide at the pilot:

- extend `improv.h` with a **seeded** mode (today it is performance-only on `rnd()`, by design, so
  a seeded motif belongs in a new function, not a change to the old one), or
- a small `motif.h`: statement / variant / answer with chord-tone snapping on strong beats and
  register continuity, fed by the cart's scale and a `chord_at(step)` callback (the `lc_phrase`
  shape, reimplemented, not copied).

First customer: `lofi.c`'s dab.

### Phase 4: humanise as a controlled layer

Replace scattered `rnd(2)` ms jitter with per-part sigma (Gaussian-ish, sum of uniforms) plus exact
fixed lags, the `at_()` model. loficity's own `humanize_bar` (grace notes, passing tones, bass
walks, suspensions) is ours, not the port's, and is the natural candidate to share once a second
station wants it.

### What we will NOT do

- Copy Lofi Cities' tables or style numbers into other stations. The port stays in `loficity`,
  bit-exact; the stations take the method.
- Force a form on beatless or through-composed stations.
- Change `rad_srnd` or the xorshift (the seed rule).

## 4. Tracking

- [x] Phase 0: `rad_hit` / audio-time clock in `radio.h`; `lofi.c` migrated; gate probe (2026-09-29: `rad_audio_pos` /
  `rad_audio_step` / `rad_hit`; `tools/schedule-check` now runs `radclockcheck` through the chassis: 0 samples vs 785
  for today's path; `lofi.c`'s sequence of watch() states identical at seed 7, only the displayed chord flips ~6 frames
  later because the grid books 100 ms ahead instead of one step)
- [ ] Phase 0: remaining 38 stations migrated (`build-all` + trace-unchanged per cart)
- [ ] Phase 1: `lofi.c` form + layers + A/B progs + push/fill/dip + tone ride; baked + clip
- [ ] Phase 2: second customer (`house` or `bossa`), shared blocks extracted
- [ ] Phase 2: rollout to the no-form stations
- [ ] Phase 3: seeded motif phrasing, first on `lofi.c`
- [ ] Phase 4: shared humanise layer
