# Multitouch, polyphony & shared-code audit (the instrument shelf)

> AUDIT (snapshot, 2026-06-13): a sweep of all **85 `kind:"instrument"` carts** asking three
> questions — is the voice **mono or poly**, does the cart **read more than one touch** (and
> should it?), and what plumbing is **hand-rolled across carts** that could become a shared
> `runtime/*.h` header the way `keybed.h` did for pianos. Forward-looking; pairs with
> [`cart-library-direction.md`](cart-library-direction.md) (what to *build*) and
> [`touch-notes.md`](touch-notes.md) (the touch API itself).

The trigger was a real observation: **the upright bass listens to only one touch**, though a
real double bass can be fingered on multiple strings. That turns out to be the tip of a short,
concrete list — not a systemic problem. Most mono carts are mono *on purpose* (faithful
homages), and every cart that uses `keybed.h` or copies the `Ptr[NPTR=10]` finger-pool already
gets full 10-finger multitouch for free.

The two findings, in one line each: the **multitouch gaps are a handful of mouse-only carts**
whose real-world counterpart is polyphonic or two-handed; the **biggest shared-code win is
`runtime/pointer.h`** — the same 10-finger pool copied near-verbatim into 15+ carts.

---

## Part 1 — Multitouch & polyphony opportunities

### Framing

Of the 50 *playable* (non-radio) carts: 27 are mono, and 19 read only one touch. But the
overlap with "should be more" is small. Radio stations are correctly `n/a` (they play
themselves; single-point navigation is by design).

The genuine missed opportunities, ranked, leading with the flagship the owner named:

| Rank | Cart | Current input | What multitouch would add | Severity | Note |
|---|---|---|---|---|---|
| 1 | **upright** | mouse-only, 1 touch | A second finger on the fingerboard → **double-stops** / drone-plus-melody; one finger stops while another picks | notable\* | The flagship. It's a string instrument; double-stops are the most natural expansion. |
| 2 | **tr909** | mouse-only, 1 touch | The metal-filter **XY pad** is a textbook two-finger surface (X=cutoff, Y=res), plus ride a knob *while* painting the grid (currently mutually exclusive) | **big** | The single most expressive win in the drum-machine group — the XY surface is already drawn. |
| 3 | **glassharmonica** | mouse-only, 1 touch | Multiple fingers each wetting a different ring → **true polyphonic glass chords** (a real one does this) | notable | Internals are *already* poly (per-ring voice pool + level smoothing); it just can't address >1 ring. Highest effort-to-payoff ratio here. |
| 4 | **voxlab** | mouse-only, 1 touch | Two-finger formant pad — **F1 (open) / F2 (front) axes in parallel** | notable | The XY formant pad is the whole interaction; two-axis simultaneous control is how a formant space wants to be played. |
| 5 | **cr78 / tr808** | mouse-only, 1 touch | **Drag-paint across grid rows** + knob-while-painting — the standard drum-grid multitouch their siblings already have | notable | Pure parity fix: tr808/909/drummachine/groovebox already paint multi-finger; these are the odd ones out. |
| 6 | **loopstation** | hand-rolled, "few" | **Theremin in one hand while tapping pads with the other** — a looper's core live-layering move; the theremin claim (`ther_id`) currently serializes it | notable | Player-controlled, genuinely poly; the serial limit directly contradicts what a looper is for. |
| 7 | **piano** | keybed.h, but mono | Sustain/overlap of struck notes (it disables keybed voice management to model struck ring-out, and goes mono in the process) | notable | Worth revisiting whether the struck model can hold overlapping voices. |
| 8 | **pdbass** | mouse-only, 1 touch | Double-stops / multi-finger fretless fingerings, like upright | minor | Same engine constraint as upright (see \*); secondary of the two. |
| 9 | **mariopaint / djfilter / pan / vox\*** | mouse-only / ui.h, 1 touch | Live-play-while-sequencing; ride a knob while tapping; second-axis modulation | minor | Pleasant refinements; each cart is a focused single-parameter study, so low urgency. |

**\* The engine cost for upright & pdbass:** both hold a *single* oscillator voice (`b_handle`).
Double-stops need a **second engine voice**, not just reading a second pointer — a real change
to the voice model, not a wiring tweak. Scope the two together.

### Leave these mono — it's faithful

Checked and recommended to **stay mono**; the single voice is the homage, not a limitation:

- **sh101**, **tb303** — the real hardware is monophonic. Both already exploit full multitouch
  at the *input* level (fingers multiplex across faders/knobs/bender/keys); the mono audio is
  correct.
- **otamatone**, **stylophone** — single-reed / single-stylus; one touch *is* the instrument.
- **pipe / reed** (MONO mode) — last-note-priority legato is the flute/reed's defining
  behavior; POLY mode exists but is the unauthentic option, not the default.
- **mouthharp**, **musicalsaw**, **heldnotes** — genuinely monophonic real instruments.
- **martenot** — monophonic by design (all regions track one `curMidi`); the multitouch it
  *does* have (ribbon + touche + keybed at once) is already correct.
- **drift**, **voxab** — diagnostic / A-B tools, not expressive controllers.

### Shared root causes (sequence the work)

Several gaps share a fix, so a little shared plumbing knocks out multiple ranks:

- **cr78 + tr808** lack the multi-finger grid pool their siblings already have → one shared
  finger-pool / grid-paint helper (`pointer.h` below) fixes both.
- **tr909 + voxlab + djfilter** all want a per-touch **XY pad** → a shared `ui_xypad()` widget
  serves all three.
- **upright + pdbass** double-stops both need a **second engine voice** → scope together; the
  real cost is the voice model, not the input.

---

## Part 2 — Shared-code generalization opportunities

The library already factored out the heavy layers: `keybed.h` (poly key-table + finger pool),
`gestures.h`, `solo.h`, `radio.h`, `improv.h`, `ui.h`. What's *still* hand-rolled across
multiple carts, ranked by how strong the de-duplication case is:

### Strong cases (many carts, near-identical code)

**1. A multi-touch pointer pool — `runtime/pointer.h` — the biggest win. ✅ SHIPPED 2026-06-13,
fully adopted 2026-06-14.** Header built (`PTR_CLEAR`/`PTR_ACQUIRE`/`PTR_FIND` over a cart-defined
struct whose first field is `int id`) and adopted across **all 15 instrument carts** that carried
the hand-rolled pool: pluck, mallet, tabla, handpan, fm, pd, bowed, guitar, brass, spacecho,
pedalboard, drummachine, tb303, groovebox, sh101. (`pedalboard` was a special case: its pool was
declared *after* `init()` and had no explicit clear — it self-cleared via the per-frame release
"present-scan" freeing the zero-init slots on frame 1, so input worked. Adoption moved the decl
above `init()` and added an explicit `PTR_CLEAR` — a tidy-up, **verified behavior-equivalent**:
a scripted pedal-drag drops `chain_n` 5→4 identically before and after.)
The same construct appears verbatim across **15+ carts**: a `Ptr` struct (`id`/`mode`/…),
`NPTR=10`, and a per-frame loop over `touch_count()`. Records call it out as identical or
near-identical in: **pluck, mallet** ("identical pattern to pluck.c"), **tabla** ("nearly
identical to mallet.c"), **handpan, fm, pd** ("identical to fm.c"), **bowed, guitar, brass,
spacecho, pedalboard** (the richest — 6 modes: KNOB/PICK/DRAGSLOT/DRAGPAL/SCROLL),
**drummachine** ("identical pattern to mallet.c sweep"), **tb303, groovebox, sh101**. Three
records independently nominate it *by name*. The header owns: the `Ptr` array, claim/release on
press/lift, the `touch_count()` dispatch loop, and a small `mode` enum the cart fills in.
`pedalboard`'s 6-mode version is a ready-made reference. **If you do one thing, this is it.**
(A follow-up sweep of the *non*-instrument carts — Part 3 — found 3 more adopters: `touchlab`,
`loupe`, `uikit`, all for the same widget-capture reason; ship it instrument-shaped.)

**2. Radio-station voice-leading toolbox** — `radio.h`/`improv.h` are deliberately thin, but
several musical sub-patterns repeat across stations:
- **Bass voice-leading (`bass_peek`/`bass_near`)** — duplicated in **lowend, citypop, house,
  dub, italo** (5+ carts, exact helper names). Safe extraction.
- **Borrowed-chord / melodic-accommodation pitch filtering** — **jingle, lowend, italo, house**;
  one record nominates `borrowed-chord.h`. Safe extraction.
- **Song-archetype scaffolding** (`PROG[]`/`FORM[]` + per-archetype voice swap) — structurally
  identical across **air, polopan, napoleon** (+ citypop). The header owns the *dispatch
  skeleton*; the songs stay per-genre. More invasive.
- **Held-voice pad driver** (`note_on` + live `note_pitch`/`note_glide`/`note_lfo` on a
  `padH[]` array) — **ambient, dub, house, italo, motorik, carlos**. Medium-strong.

> Honest caveat: station authors clearly *value* hand-rolling the brain per genre. The
> bass-lead and borrowed-chord helpers are the safe moves; archetype scaffolding is invasive.

**3. Drum-machine step-grid + `fire()` dispatch — `runtime/stepgrid.h`.**
**cr78, tr808, tr909** share the same skeleton: preset system, grid hit-test, and a "massive
switch per voice" `fire()` dispatch (tr808 "duplicates cr78 voice recipe + dispatch"; tr909
"fire() and knob patterns duplicate tr808"). **drummachine, groovebox** add a 10-finger
grid-paint pool on top. Header owns: a `bool grid[ROWS][STEPS]` model, the step clock,
multi-finger paint (shared with `pointer.h`), and a per-row voice-fire callback.

### Medium cases

**4. Fingerboard fret-to-pitch geometry — `runtime/fretboard.h`.** **upright** and **pdbass**
both hand-roll string-lane geometry (`SBASE`, `LANE_H`) and a `pos_at()` fret→MIDI function, the
same shape. **guitar/bowed** add per-string bend tracking on top (only partial adopters). Worth
doing if a third fingerboard cart appears.

**5. FX set-and-hold gating — `runtime/fx-gating.h`.** The CLAUDE.md rule "(re)configure effects
only when a value CHANGES" is re-implemented per cart. **djfilter** hand-rolls it explicitly;
**groovebox**'s `apply_fx()` is the canonical copy cited in CLAUDE.md. A tiny "last-applied
value, compare-before-push" guard. High value relative to size — getting it wrong causes audio
stutter.

**6. "Drive live params on every held voice" loop — `runtime/drive-live.h`.** Carts loop over
sounding voices each frame pushing `note_cutoff`/`note_vol`/`note_lfo` to follow live knobs:
**moog** (`drive_live()`, flagged "a model pattern"), **organ** (`apply_live()`), **sh101**,
**reed**/**pipe** (`push_live()`). The header owns the iteration + a per-voice callback; each
cart maps different knobs, so not a pure clone.

**7. Vowel / voice-family plumbing — `runtime/voice.h` (or `syllable.h`).** The speech sub-family
hand-rolls overlapping logic: effort curve (`push_effort` in **vox, voxpad**), syllable timing /
coda firing (**say, voxpad**), param indexing (**voxlab, voxab, vox4**). **vox4** is the cleanest
"locked public-API" template. Niche, but the duplication is explicit and named.

### Weak cases (≤2 carts, or too cart-specific — listed honestly)

- **`ui_xypad()`** — voxlab + tr909 (+ djfilter). ~2 real users; belongs *inside* `ui.h`, not a
  new header. (Note: this is also the fix for multitouch ranks 2 & 4 above.)
- **Vertical/custom slider (`fader`)** — stylophone + piano. Extend `ui.h`, don't mint a header.
- **A/B compare automation (`compare.h`)** — only voxab. Conditional.
- **Echo/feedback scheduler (`echo.h`)** — only dub's `echo_hit()`.
- **Walking bass (`walking-bass.h`)** — only cocktail.
- **Ribbon controller (`ribbon.h`)** — otamatone + martenot + heldnotes, but each is bespoke;
  only if they're deliberately aligned with a future ribbon cart.
- **Module/patcher framework** — only modrack. No case until a second patcher exists.

### Free wins — adopt an existing header (no new code)

- **tr808 / tr909 / cr78** hand-roll `draw_knob` rotaries — `ui.h` already owns knob widgets.
  Adopting it *also* gets them the multi-touch knob handling they lack. **Highest-value free
  win — fixes duplication *and* a mobile gap at once.**
- **piano** uses `keybed.h` but bypasses its voice management (legit, for struck ring-out) then
  hand-rolls a slider pool — the slider drag should still come from `ui.h`.
- **mt70 / sh101** *deliberately* defer `keybed.h` (mt70's struck ring-out semantics; sh101's
  two-row stacked manual). **Legitimate non-adoptions, not oversights.** sh101's two-manual
  layout is the one concrete feature that, if added to `keybed.h`, would let a real cart drop a
  hand-rolled key table.

---

## Recommended order

1. **`runtime/pointer.h`** — copied near-verbatim into 15+ carts; `pedalboard` is the reference.
   The clear top win. (Also unblocks multitouch ranks 5 — cr78/tr808 grid paint.)
2. **`ui.h` knob adoption** by tr808/tr909/cr78 — free win, fixes duplication + a mobile gap.
3. **`ui_xypad()` inside `ui.h`** — unblocks multitouch ranks 2 & 4 (tr909, voxlab) + djfilter.
4. **Bass-lead / borrowed-chord radio helpers** and the **`fx-gating` guard** — best
   value-per-line after the pointer pool.
5. **upright/pdbass double-stops** — needs a second engine voice; scope as a voice-model change,
   not an input tweak.
6. Everything in the weak list waits for a third adopter.

### Spin-off: fretting-by-finger (the `monochord` probe → a string fretboard)

The multitouch-strings thread (ranks 1/8, upright/pdbass) opened a broader idea: model the
string *for real* instead of abstracting chords behind buttons (the way `pedalboard` does). The
atomic test shipped as **`monochord`** (2026-06-14, a `pointer.h` probe): one string, fingers
are movable bridges, segment length = pitch — the đàn bầu / diddley bow / monochord gesture, on a
PD oscillator so a held note glides as a bridge slides. The gesture proved fun (early
desktop verdict — see [`probe-carts.md`](probe-carts.md)), so the payoff shipped as **`fretboard`**
(2026-06-14): six fretless strings of the monochord side by side, finger position = pitch, a chord
is *emergent finger geometry* (not a lookup), a drag is a strum. Touch-ceiling caveat: a full 6-finger chord + strum exceeds the iPhone 5-touch cap, so
the fretboard leans desktop/iPad; the probe itself stays ≤3 fingers (testable everywhere).

---

---

## Part 3 — Non-instrument carts that want pointer.h (follow-up sweep)

After Part 2 nominated `runtime/pointer.h`, a second sweep checked the **~263 non-instrument
carts** — does the multi-finger pool justify itself beyond the sound shelf? The drag/touch-heavy
candidates (physics sandboxes, paint toys, board games, sims, and the 9 carts already using the
touch API) were assessed. Verdict: **a handful of real adopters, all for the same
widget-capture reason — not a reason to broaden the design.**

Of 47 distinct non-instrument carts assessed: **3 real customers** (touchlab, loupe, uikit),
**7 marginal** (a parallel-grab would be *fun* but the cart never needs it), the rest **no**.
Three of the worth-it carts already **hand-roll their own pool** (touchlab, loupe, multitouch) —
adopting pointer.h there is a pure free win (delete bookkeeping).

### Customers (YES + MARGINAL only), ranked

| cart | current input | what multi-finger adds | severity | free-win? |
|------|---------------|------------------------|----------|-----------|
| touchlab | touch_count loop (hand-rolled `prev_id/_x/_y`, MAXF=16) | a device probe tracking all simultaneous fingers w/ per-finger id/lift; pointer.h replaces the whole hand-rolled array | big | **yes** |
| loupe | hand-rolled `Cap[MAXCAP]` capture structs | one finger positions the lens while another grabs a knob/step inside it; pointer.h replaces the manual `cap_for()`/`cap_free()` lifecycle | notable | **yes** |
| uikit | ui.h capture table | three knobs each grabbed by a separate finger at once; pointer.h would make the hand-off explicit/reusable rather than buried in ui.h | notable | no |
| multitouch | hand-rolled pool (`held_id[10]`) | reference multi-finger painting demo; adopting pointer.h is consolidation (one action per finger) | minor | **yes** |
| physics | single mouse drag | many throwable points; multi-finger fling would be fun but isn't essential (`grab=-1`, one at a time) | minor | no |
| coaster | single mouse drag | grab two track points to reshape a loop in parallel; gameplay never requires it | minor | no |
| incmachine | single mouse drag | reposition two parts at once; not a core mechanic | minor | no |
| sfxed / gnomeseek / linerider | single drag / tap-loop | one contour / one tap / one stroke at a time | minor | no |

### Clear NOs (checked, grouped by reason)

- **Single-pointer drag, one object at a time:** ragdoll, physics (sandbox), collision-lab-1/2/4/5,
  dreamcad, bones, dpaint, inkrunner, loveparade, mandala, solitaire, dungeonkeeper, dwarffort,
  soccermgr, respond, eq, bitcrush.
- **Tap-as-mouse / point-and-click, no drag:** neonrain, zak, wordle, mariopaint, geometrydash,
  calgames, monkey, pizzatycoon, merchant, turfwar, infiniminer.
- **Many objects but inherently serial (select-then-act):** bejeweled, poker, strippoker, boggle,
  pool, chess, dune.
- **No touch input at all:** sloop (button steering), soundcheck (automated playback).

### Does this change the recommendation?

**No — keep pointer.h instrument-shaped.** The only non-instrument carts that genuinely want it
(touchlab, loupe, uikit) want it for the *same* multi-finger-widget-capture reason the
instruments do, not for a new physics grab/fling idiom. The marginal sandboxes only ever grab
one object, so there's no demand to generalize toward parallel object dragging. Ship the
instrument-style 10-finger claim/release pool; touchlab/loupe/multitouch are bonus free-win
adopters that fit the existing shape.

---

## Appendix — per-cart record (playable carts only)

`poly?` = does playing produce simultaneous voices · `real-poly?` = is the real instrument
polyphonic · `touches` = how many fingers it reads for playing · `gap` = multitouch opportunity
severity. Radio stations omitted (`n/a` — they play themselves).

| cart | kind | poly? | real-poly? | input | touches | gap |
|---|---|---|---|---|---|---|
| cr78 | drum-machine | mono | no | mouse-only | 1 | notable |
| drummachine | drum-machine | mono | no | step-grid | many | none |
| groovebox | drum-machine | poly | yes | step-grid | many | none |
| tabla | drum-machine | mono | yes | held-notes | many | minor |
| tr808 | drum-machine | mono | no | mouse-only | 1 | notable |
| tr909 | drum-machine | mono | no | mouse-only | 1 | **big** |
| pedalboard | effect-showcase | poly | yes | held-notes | many | none |
| spacecho | effect-showcase | mono | yes | held-notes | many | none |
| bowed | fingerboard-string | poly | yes | held-notes | 10 | none |
| guitar | fingerboard-string | poly | yes | held-notes | 10 | none |
| omnichord | fingerboard-string | poly | yes | hand-rolled-touch | many | none |
| pdbass | fingerboard-string | mono | yes | mouse-only | 1 | minor |
| upright | fingerboard-string | mono | yes | mouse-only | 1 | minor\* |
| glassharmonica | held-notes-expr | poly | yes | mouse-only | 1 | notable |
| heldnotes | held-notes-expr | mono | no | mouse-only | 1 | none |
| hurdygurdy | held-notes-expr | mono | yes | held-notes | many | minor |
| martenot | held-notes-expr | mono | no | held-notes | many | none |
| mouthharp | held-notes-expr | mono | no | mouse-only | 1 | none |
| musicalsaw | held-notes-expr | mono | no | mouse-only | 1 | none |
| otamatone | held-notes-expr | mono | no | hand-rolled-touch | 1 | none |
| pipe | held-notes-expr | poly | no | held-notes | few | none |
| pluck | held-notes-expr | mono | yes | held-notes | many | minor |
| reed | held-notes-expr | poly | yes | held-notes | few | none |
| stylophone | held-notes-expr | mono | no | hand-rolled-touch | 1 | none |
| vox | held-notes-expr | mono | yes | held-notes | 1 | minor |
| vox4 | held-notes-expr | mono | yes | held-notes | 1 | minor |
| brass | keybed-piano | poly | no | held-notes | 10 | minor |
| epiano | keybed-piano | poly | yes | keybed.h | many | none |
| mellotron | keybed-piano | poly | yes | keybed.h | many | none |
| mt70 | keybed-piano | poly | yes | hand-rolled-touch | many | none |
| organ | keybed-piano | poly | yes | keybed.h | many | none |
| piano | keybed-piano | mono | yes | keybed.h | many | notable |
| loopstation | looper-toy | poly | yes | hand-rolled-touch | few | notable |
| drift | showcase-keyboard | poly | yes | keybed.h | few | none |
| handpan | showcase-keyboard | mono | yes | held-notes | many | none |
| mallet | showcase-keyboard | mono | yes | held-notes | many | minor |
| mariopaint | showcase-keyboard | poly | yes | mouse-only | 1 | minor |
| pan | showcase-keyboard | poly | yes | mouse-only | 1 | minor |
| touchpiano | showcase-keyboard | poly | yes | keybed.h | many | none |
| voxab | showcase-keyboard | mono | yes | mouse-only | 1 | none |
| voxlab | showcase-keyboard | mono | yes | mouse-only | 1 | notable |
| voxpad | step-grid | mono | yes | ui.h | 1 | minor |
| fartsynth | synth-panel | mono | no | QWERTY-only | 1 | none |
| fm | synth-panel | poly | yes | held-notes | many | none |
| juno | synth-panel | poly | yes | ui.h | many | none |
| modrack | synth-panel | poly | yes | hand-rolled-touch | many | none |
| moog | synth-panel | poly | no | keybed.h | many | none |
| pd | synth-panel | poly | yes | held-notes | many | none |
| sh101 | synth-panel | mono | no | hand-rolled-touch | few | none |
| tb303 | synth-panel | mono | yes | held-notes | many | minor |

\* upright's `gap` is recorded `minor` per-cart but is the **flagship** opportunity at the
report level — the per-cart severity reflects that the win requires a second engine voice (a
voice-model change), not because the opportunity is small.
