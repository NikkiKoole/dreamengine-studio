# The device-face paradigm — a reusable shape for a deep instrument on a small screen

STATUS: EXPLORING (2026-07-13) — reverse-engineered from Pure Acid (JimAudio) with the maker, prototyped
live in [`acidface`](../../tools/carts/acidface.c). A general UI template, NOT an acidrack task; this doc
crystallizes the pattern so it can be reused across instrument carts (chordblossom, epiano, the radios…).
chordblossom (§1c), more-note-bass (§1d) + pocketbox (§1e) worked through on paper (2026-07-13) — all three
transferred, and the corrections along the way (a hero surface filed as a strip; a flat grid blessed as "show
everything") hardened the §2 rules and produced the reading guardrail (§2b, the pointers that keep the next
reader off the wrong route). pocketbox is also the worked case of the paradigm *fixing* a hardware clone's UX
pain (14-page paramscroll, no knobs, invisible holds) without touching its sequencer brain.

> **Why this doc exists.** We kept re-solving "how do I fit a big instrument on a phone?" per-cart
> (the [acidrack accordion](acidrack-layout-brief.md), the [epiano keybed brief](epiano-layout-brief.md)).
> Studying Pure Acid with the maker surfaced a *general* answer that isn't rack-specific. It generalises
> the way [`device-adaptive-layout.md`](device-adaptive-layout.md) is about the engine and
> [`responsive-instrument-ui.md`](../guides/responsive-instrument-ui.md) is about the process — this is
> about the **shape of the face itself**.

## 0 · The problem it answers

Every deep instrument has three tensions on a small screen:

- **(a) a few params you always want live** (the knobs you ride while playing), against
- **(b) far more params + data than fit at once** (all the params, patterns, an editor, a mixer), against
- **(c) the need to both PERFORM and EDIT** without leaving the surface.

The [acidrack](acidrack-layout-brief.md) answer was **disclosure by accordion** — fold/compact/expand the
strips so the whole *band* stays visible. The device-face answer is different: **stop trying to show
everything; give ONE instrument the whole screen as purpose-built hardware, and disclose the depth
through a touchable display + a mode-switched button grid.** Not amputation — the depth is all reachable,
just one mode-tap away (the [ADR-0022](../decisions/0022-collaboration-is-the-north-star.md) "disclosure,
not amputation" rule, applied by *mode* instead of by *panel size*).

## 1 · The five zones (the core model)

A device face is a fixed vertical stack of five zones. Every instrument fills the same slots; only the
labels change.

```
┌────────────────────────────────────────────────┐
│ 1  NAV SPINE   ▶ · [BASS][SEQ][MIX][DRUM] · name │  thin; play + view-tabs + preset
├────────────────────────────────────────────────┤
│ 2  CONTINUOUS  (o)(o)(o)(o)(o)(o)                │  the knobs/sliders you ride LIVE
├───┬────────────────────────────────────┬────────┤
│ s │ 3  THE CONTEXT DISPLAY              │ s      │  a TOUCHABLE screen: renders the
│ o │    (piano-roll / kit / patterns /   │ o      │  current FLOW + hosts its ops.
│ f │     mixer / scope) + its own ops    │ f      │  soft-keys pick the flow.
├───┴────────────────────────────────────┴────────┤
│ 4  MODE-SWITCHED GRID  [][][][][][][][] …        │  ONE button row, many jobs by mode
├────────────────────────────────────────────────┤
│ 5  PERFORMANCE SURFACE  ▌▌█▌▌ ▌█▌▌                │  keybed / pads — play & enter
└────────────────────────────────────────────────┘
```

1. **Nav spine** (thin top) — transport (play/stop), the *view* tabs (the coarse navigation — Pure Acid's
   `BASS / SEQ / MIXER / DRUMS`), preset/pattern name. This is the real navigation backbone; the soft-keys
   in zone 3 are *within* one view.
2. **Continuous controls** — the handful of always-live params (tension **a**). Fixed, never disclosed
   away — comfort-sized ([`ui.h`](../../runtime/ui.h) `ui_knob`, one finger each; the acidfit "comfort
   threshold, not fit threshold" lesson). **They're contextual too**: with nothing held they edit the
   patch globally; **hold a step in zone 4 and the same knobs edit THAT step** (Elektron parameter locks
   — §2).
3. **The context display** — the heart. A **touchable UI element** (Pure Acid's pink screen, not hardware
   glass) that renders the current **flow** and **hosts that flow's operations inside itself** (the maker,
   2026-07-13: "the screen is a UI element too; clear/random live in the little screen"). Soft-keys
   flanking it pick the flow (edit / patterns / mixer / scope…). This absorbs tension **b**: depth lives
   here, swapped by flow, not crammed.
4. **The mode-switched grid** — one physical button row that does **double/triple duty by mode**: step
   toggles, voice-select, pattern-select, chromatic notes. Pure Acid's drum row is *voices* in `TRIGGER`
   mode and *steps* in `STEP EDIT` mode; Elektron's `[TRK]`/`[FUNC]` + `[TRIG]` overloads the same 16 keys.
   This is disclosure-by-mode: one surface, a held/toggled modifier repaints its job.
5. **Performance surface** — the keybed or pads (tension **c** stays satisfied — you can always play).

## 1b · acidface, annotated (the model made concrete)

The reference demo ([`acidface`](../../tools/carts/acidface.c)) in portrait — the five zones with the
acid-box labels:

```
┌───────────────────────────────────────────────────┐
│ ▶   ◀ ▶    NOTE  A                          LOOP    │  ① TRANSPORT / nav spine
├───────────────────────────────────────────────────┤
│  (TUN) (CUT) (RES) (ENV) (DEC) (ACC)                │  ② KNOBS — always-live sound params
├──────┬─────────────────────────────────┬──────────┤
│ SEQ  │                                 │  MIX     │  ③ DISPLAY, flanked by
│ PAT  │        T H E   D I S P L A Y     │  FX      │     6 SOFT-KEYS that pick the
│ SONG │     (renders the current flow)   │  SCOPE   │     display FLOW
├──────┴─────────────────────────────────┴──────────┤
│ [NOTE] [ BD ] [ SD ] [ CH ] [ OH ]                  │  ④ LANE STRIP — what the row edits
├───────────────────────────────────────────────────┤
│ ▪▪▪▪  ▪▪▪▪  ▪▪▪▪  ▪▪▪▪                               │  ⑤ 16-STEP ROW — mode-switched
├───────────────────────────────────────────────────┤
│ █ █▐█ █ █ █▐█▐█ █   (one octave of piano keys)      │  ⑥ KEYBED — play / enter notes
└───────────────────────────────────────────────────┘
```

**Four independent mode drivers** — named for the control that triggers each — repaint different
surfaces; this is the "shallow surface, deep by context" idea in the concrete:

**FLOW** (a soft-key) **· repaints the DISPLAY only** (row + keybed untouched):

| flow | the display shows |
|---|---|
| **SEQ** | the editor for the active lane (see LANE) |
| **PAT** | pattern picker — 6 slots |
| **SONG** | an 8-slot pattern chain |
| **MIX** | 4 faders (MST/OSC/DRM/ACD) |
| **FX** | 3 knobs (DLY/FB/RVB) *inside* the screen |
| **SCOPE** | animated waveform (reacts to tempo + last note) |

**LANE** (a lane chip) **· re-purposes the STEP ROW and the SEQ display together:**

| lane | ⑤ step row | ③ SEQ display | ⑥ keybed |
|---|---|---|---|
| **NOTE** | **step-select** (tap = pick step; lit = has note) | touchable piano-roll + `ACC SLD TIE` (sel step) · `CLR RND` (pattern) | sets the selected step's pitch, then advances |
| **BD/SD/CH/OH** | **drum cells** (tap = toggle hit) | whole-kit overview (4×16) + `CLR RND` | auditions only |

**SHIFT · hijacks the STEP ROW** into pattern-select (buttons 1–6 pick the pattern; transport shows
`PAT-SEL`). Plus **play** lights the running step across the row and in the display.

**LOCK · retargets the KNOBS** (parameter locks, NOTE lane). The `LOK` button in the SEQ edit strip
arms it: the 6 knobs frame amber and edit the **selected step** instead of the patch — turning one stamps
a per-step value (that knob gets an amber ring; the step gets an amber pip; transport shows `LOCK Snn`).
So even zone 2 does double duty. *(Prototyped as a latch — press `LOK`/`k`; true two-finger hold-a-step-
while-turning is the device refinement.)*

One line: **the soft-keys reskin the screen; the lane chips re-purpose the row + SEQ screen together;
SHIFT hijacks the row; LOCK retargets the knobs.** Four surfaces, each doing several jobs by mode.
*(Still §5-open: the row is a small lane-strip kit not the two-mode 16-voice row, and there's no
face-level tab in ① — the flow soft-keys are the within-face level.)*

**Persistence map — the skeleton vs. the surfaces.** Sort every element by what a mode does to it:

- **● Always consistent** (never moves, never changes job) — the mode *selectors*: `▶` play · `◀ ▶`
  pattern · `LOOP` (①), the 6 soft-keys (③), the lane chips (④).
- **◆ Fixed in place, JOB changes by mode** — the *editing surfaces*: the **knobs** (LOCK → patch ↔
  selected step), the **16-step row** (lane + SHIFT → steps / drum cells / pattern-select), the
  **keybed** (lane → pitch-entry ↔ audition).
- **○ Fully repaints** — content, not a control: the **display** (soft-key + lane) and the spine's
  **status readout** (mirrors lane / LOCK / SHIFT).

The rule this states as *layout*: **the mode selectors are the persistent skeleton; the editing surfaces
shift meaning.** You never lose your navigation — only the thing under your fingers changes job. That is
the "shallow surface, deep by context" principle turned into a placement law, and the checklist for
laying out the next face: keep the selectors fixed, let the surfaces be contextual.

## 1c · chordblossom, annotated (the paradigm's second face — and its stress test)

Worked through on paper with the maker (2026-07-13). chordblossom is the harder case: it's an **Omnichord**,
so it has *two* bottom surfaces where acidface has one keybed — a keybed that **picks the root** and a strum
plate that **performs the chord**. Deciding where each goes is what taught §2's two newest rules. The face,
portrait:

```
┌──────────────────────────────────────────────────────┐
│ ▶   CHORD  PROG  DRUMS  MIX      C MAJ7        96 BPM  │ ① NAV SPINE — transport · face tabs · readout
├──────────────────────────────────────────────────────┤
│  (SOUND)(FX)(BASS)(KEY)(LEAD)(VOL)                     │ ② KNOBS — always-live (dissolves the old MIX tab)
├──────┬──────────────────────────────────┬─────────────┤
│ STRUM│  ▌ ▌ ▌ ▌ ▌ ▌ ▌ ▌ ▌ ▌  ~ strum ~   │ CLR RND     │ ③ DISPLAY = the SONIC-STRINGS plate at full
│ PROG │  (the voiced chord, as strings)   │             │    height — a surface you PERFORM on; soft-keys
│ DRUMS│  (PROG → chain · DRUMS → grid)     │             │    swap what the screen plays/edits
├──────┴──────────────────────────────────┴─────────────┤
│ [dim][min][MAJ][sus] │ [6][m7][maj7][9]                │ ④ CHORD-BUTTON ROW — QUALITY ↔ PROG (mode-switched)
├──────────────────────────────────────────────────────┤
│ A  S  D  F  G  H  J   (root picker, + black keys)      │ ⑤ KEYBED — picks the ROOT (a selector)
└──────────────────────────────────────────────────────┘
```

**The pivotal call — the strum plate is a DISPLAY, not a zone-5 strip.** The first sketch filed it under the
keybed as a thin performance strip; that was backwards, because the strum *is* the whole point of the
instrument. It already **renders the voiced chord** (the colored strings are `strNote[]`) and is
**touchable** — the exact definition of the context display — so it earns zone 3's full height. The keybed,
which only **picks the root**, is a *selector* and stays in zone 5. (See §2: "the hero surface owns the
display" + "selector vs performer".)

**The mode drivers** (acidface's FLOW/LANE/LOCK, in chord terms):

| driver | what it does |
|---|---|
| **FLOW** (soft-key) | reskins the display: **STRUM** (play the chord on the strings) · **PROG** (edit the progression chain, `REC PLAY CLEAR` in-screen) · **DRUMS** (6×16 grid + presets) · **MIX/FX** (overflow knobs) · **SCOPE** |
| **GRID MODE** | re-purposes the chord row: **QUALITY** (set the current chord's dim/min/MAJ/sus + 6/m7/maj7/9) ↔ **PROG** (8 progression slots; tap = stamp the current chord) |
| **LOCK** | retargets the knobs: hold a PROG slot → zone 2 edits *that chord's* voicing, so a progression carries per-chord inversions (Elektron p-locks, but *musical* here) |

The old three tabs (CHORD / MIX / RHYTHM) dissolve: MIX → zone 2 + a flow, RHYTHM → the DRUMS/PROG flows.
Everything is one mode-tap away, never a tab away — which is the whole point of the face over the tab-stack.

## 1d · more-note-bass, annotated (depth goes in the display — a wrong route, corrected)

Worked through with the maker (2026-07-13). A tiny groovebox — KICK + SNARE over a five-row pentatonic bass
lane — and the face's first *cautionary* example, because the first reading took the **wrong route** and the
maker corrected it. The trap: the existing cart shows all seven lanes flat at once (it predates the face), so
the first instinct was to bless that as a rule ("small pattern → merge the display and the grid, show
everything"). That is precisely the show-everything anti-pattern §0 exists to kill. The right route keeps the
surface shallow and moves the *depth* into the flow-switched display:

```
┌──────────────────────────────────────────────────────┐
│ ▶  MORE NOTE BASS        C min-pent       112 BPM SW18 │ ① NAV SPINE — transport · key · tempo
├──────────────────────────────────────────────────────┤
│  (TEMPO)(SWING)(VOL)                                   │ ② KNOBS — only what you RIDE live
├──────┬──────────────────────────────────┬─────────────┤
│ BASS │  A# ·  ·  G ·  ·  · F ·  ·  ·  ·  │ SUB         │ ③ DISPLAY, flow-switched:
│ SOUND│  ·  · D# ·  ·  ·  ·  ·  · C ·  ·  │ CLR         │   BASS  = draw the actual bass notes (a
│ DRUM │  (the bassline, as a piano-roll)  │             │           piano-roll — the pitched depth)
├──────┴──────────────────────────────────┴─────────────┤   SOUND = the four XY pads (set-and-leave)
│ KICK  ▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪   SNARE ▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪         │ ④ STEP ROW — the on/off drum lanes;
│  (in BASS flow this row = 16 bass step-selects)        │    mode-switches to bass step-select
├──────────────────────────────────────────────────────┤
│ A W S E D F T G Y H U J    transpose root · Z/X octave │ ⑤ KEYBED — sets the step's pitch / transposes
└──────────────────────────────────────────────────────┘
```

The **SOUND** flow just repaints the display with the pads (big + rectangular — the cart's own todo):

    [KICK  boom×kick]   [BASS  cut×res]
    [SNARE tone×snap]   [FLNG  spd×amt]

This is **acidface's NOTE lane, 1:1**: the bass is monophonic-per-column, so the step row picks the step, the
display shows the pitched pattern, and the keybed sets the selected step's pitch (and transposes the root — a
*selector*, §2). The maker rederived the paradigm's existing pattern from scratch — which is the proof it's
the right route. Two rules fell out of the correction (both in §2): **depth decides surface-vs-display** (the
on/off drums stay on the surface; the pitched bass + the sound pads go in the display) and **usage, not
widget type, picks the zone** (the XY pads are set-and-leave sound design → a display flow, NOT zone 2).

## 1e · pocketbox, treated (the paradigm applied to a hardware clone's pain)

pocketbox (see [`pocketbox.md`](pocketbox.md)) is a full 8-track tracker — p-locks, trigger conditions,
polymeter, song parts — modelled faithfully on the Wee Noise Makers PGB-1: a 128×64 "OLED", NO knobs, one
touch strip, everything driven by 16 step keys + hold-combos. It's the completest device-face in the repo, and
the clearest case of the paradigm *solving* a cart's problems — because every pain it has is inherited from
copying a tiny-screened, knobless hardware box onto a phone that has neither limit. The brain is great; the
*face* is cramped. The fixes, one per pain:

| pocketbox's pain (inherited from the hardware) | the paradigm's fix |
|---|---|
| Set one track's sound = arrow through **14 pages** (ENGINE, P1–P4, VOL, PAN, BUS, OCT, SHUF, 3×LFO, DRAW), one param visible at a time | grow the OLED into a full display that shows the **whole param set at once**, touchable; the pages become soft-key **flows** you tap, not arrows you scroll (disclosure by *flow*, not by *paging*) |
| **No knobs** — ride everything through one shared strip, filter-only, FX-hold-only | zone 2 = a few **comfort knobs** for the params you ride; the strip stays for sweeps |
| Modes are **invisible hold-combos** (hold EDIT = keyboard, PLAY+key = mute, FX+key = live FX) you must memorize | make them **visible**: mute/solo → a MIX flow, the keyboard → a real keybed, FX → a flow — nothing behind a hold |
| The **p-lock hold works** (hold step + strip) but you can barely see the step on the tiny OLED | hold a step → the big display shows **that step's p-locks**, knobs retarget to it (the LOCK rule); pocketbox already has the gesture, the paradigm gives it a screen worth looking at |
| **Solo missing**, mute is a hold-combo, no meters (its own todos) | a **MIX flow** with visible per-track mute/solo + level meters |
| **ROLL code can't be typed in** — you can share a groove code but not enter one (todo) | a **SEED flow** — a touch field to type/paste a code, in the display (the display is the toolbox) |

The touch-native face — same brain, rebuilt surface:

```
┌────────────────────────────────────────────────────────┐
│ ▶  TRACK STEP PATT SONG      BASS   124 BPM       ROLL  │ ① transport · mode tabs · track · bpm
├────────────────────────────────────────────────────────┤
│  (CUTOFF)(RES)(VOL)(PAN)      ~~~~ strip ~~~~            │ ② comfort knobs you RIDE + the strip
├──────┬───────────────────────────────────────┬─────────┤
│ SOUND│  ENGINE BASS    P1[====] P2[==]         │ MIX     │ ③ the whole track's params AT ONCE
│ STEP │  P3[===] P4[=====]   OCT -1  LFO ~~~~    │ FX      │    (was 14 arrowed pages), touchable.
│ SONG │  ── hold a step → this shows THAT       │ WAVE    │    soft-keys pick the flow: MIX = meters
│      │     step's p-locks; knobs retarget ──   │ SEED    │    + mute/solo · SEED = paste a code
├──────┴───────────────────────────────────────┴─────────┤
│ [1][2][3][4][5][6][7][8]                                │ ④ the 16 STEP KEYS, mode-switched
│ [9][10][11][12][13][14][15][16]                         │    exactly as today (track/step/patt/part)
├────────────────────────────────────────────────────────┤
│ A W S E D F T G Y H U J    (keybed — was hold-EDIT)     │ ⑤ a real keybed, foldable on a phone
└────────────────────────────────────────────────────────┘
```

**Nothing in the sequencer brain changes** — the 8 tracks, engines, p-locks, conditions, polymeter and song
parts all stay. The paradigm only rebuilds the face: grow the screen, add a few knobs, and turn the invisible
holds into things you can see and touch. It also settles two open threads: pocketbox already ships the **true
hold-a-step p-lock gesture** §5 listed as open (that item is now done), and its 8-track scale is the strongest
case yet for extracting zone 4 into a shared library. The buttons-only charm is worth *keeping* if you love it
— but that's a *look* you can keep while still fixing the paging and the hidden holds.

## 2 · The principles that make it work

- **Disclosure by MODE, not by panel size.** The accordion promotes panels folded→compact→expanded to fit
  the band. The face keeps one instrument at full size and swaps *what a surface means* (flow, grid mode).
  Cheaper on vertical budget; reads as hardware.
- **The display is the editor AND the toolbox.** Because it's touchable, contextual ops (clear, random,
  copy, flag-a-step) live *in* the display next to what they act on — not as chrome around it. On hardware
  those ops must be physical buttons because the glass can't be touched; we don't have that constraint, so
  we don't inherit those buttons.
- **Hardware charm is a look you can keep; its limits are not one you have to inherit.** A phone screen is
  genuinely small — so it's daft to spend it *also* emulating a device's constraints: one-param-at-a-time
  paging, no knobs, modes hidden behind hold-combos. Those were forced on the hardware by physical knobs and a
  glass you can't touch — you have neither problem, so don't paint yourself into a corner you were never in.
  But the *vibe* is the whole point: the buttons-only feel, the little-OLED look, the pocket-device intimacy
  are why people love these boxes — keep them. The move is to separate the two — **the aesthetic is a gift, the
  limitations are baggage** — and take only the gift. pocketbox (§1e) is the worked case: keep the PGB-1 charm,
  drop the 14-page paramscroll.
- **The hero surface owns the display — and zone 3 can be PLAYED, not just read.** Every instrument has one
  gesture that *is* the point of it (acidface: writing the pattern; chordblossom: the strum). That surface
  earns the display's whole real estate. The trap is filing the play-gesture reflexively into zone 5 (the
  keybed slot) and cramming the most important thing into a sliver — chordblossom's first sketch put its
  strum plate in a 44px strip under the keybed, exactly backwards. The display is defined by two things — it
  *renders the current flow's live state* **and** it's *touchable* — and nothing says you only tap *ops* on
  it: the sonic-strings plate draws the voiced chord as strings you strum, so it renders live state × is
  touchable → it's a display you **perform on**, not a keybed. Perform and edit then become two flows of one
  screen (STRUM plays the chord; PROG edits the chain).
- **Selector vs performer — not every "second performance surface" is one.** When a face seems to need two
  play surfaces in zone 5, check whether one is actually a *selector*. chordblossom's keybed **picks the
  root** (a selector → keep it in zone 5, or promote it to a grid mode in zone 4); the strum plate
  **articulates the chord** (the performer → the display). Sorting the two dissolves the "zone 5 is
  overloaded" problem: only true performers compete for play space, and the hero one belongs in zone 3. The
  general test — *does it CHOOSE what will sound, or does it MAKE the sound?* Choosers are selectors; makers
  are performers.
- **The mode-switched grid scales the instrument's size.** A 4-voice kit fits a lane strip; a 16-voice kit
  does not — but one row that is *voices in one mode, steps in another* fits any kit. So "support a bigger
  kit" and "the row does double duty" are the **same decision**.
- **Continuous vs contextual is the split that never moves.** Zone 2 (ride-live params) is always present;
  everything else is disclosed. Deciding which params are "always live" is the one real taste call per cart.
- **Depth decides surface-vs-display — not size.** A **binary** lane (a drum hit, on/off) stays on the
  shallow surface as a step row (zone 4); you want the pulse visible. A lane with **per-step depth** — pitch,
  or a continuous value — moves *into* the flow-switched display as a proper editor (a piano-roll; an XY-pad
  panel). more-note-bass (§1d) is the worked case: KICK/SNARE stay on the surface, the pitched bass becomes a
  display piano-roll. The tempting shortcut — "the pattern is small, so just show every lane at once" — is the
  show-everything instinct the face is built to resist (§0); keep the surface shallow and put depth in the
  display, *even when it would fit*.
- **Usage, not widget type, picks the zone.** A knob or an XY-pad is not zone-2 material *because* it's a knob
  or a pad. The signal is whether you **ride it live** (→ zone 2) or **set it and leave** (→ a display flow,
  the toolbox-in-the-screen). more-note-bass's four XY pads are set-and-leave sound design, so they live in a
  SOUND *flow*, not on the always-live top row — only tempo/swing/volume are actually ridden. This sharpens
  "continuous vs contextual" above: the split is read off the *gesture*, not off the control's shape. (The
  near-miss that named it: the XY-pads were first filed into zone 2 for being continuous — the maker moved
  them into the screen.)
- **Parameter locks — the knobs edit the SELECTED step, not just the patch** (Elektron's key idea). Hold a
  step in zone 4, and zone 2's knobs stamp that step's own value (pitch, accent, filter…); release, and
  they're global again. This is what lets a face stay *shallow* (few knobs, few modes) yet reach
  *per-step* depth — no dedicated per-step editor screen needed. It's the same double-duty trick as the
  grid, applied to the knobs.
- **Fewer choices, bigger targets** — the theory under the ergonomics. **Hick's Law**: fewer visible
  options = faster decisions, so "decide what to show, never everything" ([acidrack §0](acidrack-layout-brief.md))
  is a *speed* feature, not just a fit one. **Fitts's Law**: big, well-spaced targets = quick confident
  hits — which is exactly why `finger_px()` sizing is non-negotiable. Teenage Engineering leans on both
  explicitly; naming them tells us *why* the discipline pays off, and that **subtraction is the hard,
  valuable move** (their "removing features is the hard part" — our ADR-0022 legibility bar agrees).
- **Focus a face — don't "swap" it. And scale phone→tablet by showing MORE, not by rearranging.** The nav
  spine *focuses* an instrument; what focusing DOES is room-dependent — the same logic as the accordion's
  fold/expand, just at the whole-face level. On a **tablet** Pure Acid stacks *all* racks at once (its iPad
  shots show the BASSLINE knobs, the SEQUENCER screen editing the bass line, AND the DRUMS pad row all on
  screen together — the `BASS/SEQ/MIXER/DRUMS` tabs mostly swap the *middle* slot + focus, they don't hide
  the drums). So focusing on a tablet just *highlights* while the rest stay visible. On a **phone** there's
  no room, so focusing collapses the others to tabs and you see one face. This reconciles the [acidface
  full-face] vs [acidrack accordion] tension: **they're the same app at two sizes.** A corollary the drums
  make plain: some surfaces want to be **persistent** (the pad row stays under your thumb even while the
  screen does bassline work), not a face you tab away from — persistence is a per-zone call, granted when
  there's room. *(Caveat: every Pure Acid shot we have is iPad; its actual iPhone collapse is inferred, not
  confirmed — a claim to verify if a phone screenshot ever turns up. Landscape phone stays tight; the open
  refinement is flanking the display with the knobs to use the width — see §5.)*

## 2b · Reading a cart onto the face — the guardrail (don't take the wrong route)

Twice now the first sketch took a wrong turn and the maker corrected it — chordblossom's strum plate filed as
a zone-5 strip (§1c), more-note-bass's flat grid blessed as "show everything" (§1d). Both share one root
mistake: **reading an existing cart's PRE-FACE layout as if it were the design.** An old cart shows things
flat because it never had the face; copying that flatness reinvents the anti-pattern the face exists to kill.
So don't start from the cart's current screen — start from these questions, *in order*. Each is a §2 rule
turned into a check, and each catches one of the wrong routes we actually took:

1. **What is the ONE hero gesture?** — the thing the instrument is *for* (the strum; the pattern). It owns
   the **display**, not a strip. If you've put it in a sliver, stop. (§2 *hero-surface*)
2. **For each surface: does it CHOOSE a sound, or MAKE one?** — choosers are **selectors** (a keybed picking
   a root → zone 5, or a grid mode); makers are **performers** (→ the display). Don't stack two "performers"
   in zone 5 before checking that one isn't really a selector. (§2 *selector-vs-performer*)
3. **For each lane: binary, or per-step DEPTH?** — on/off → the shallow surface (zone 4 step row);
   pitch/continuous → *into* the display as a real editor. (§2 *depth-decides*)
4. **For each control: do I RIDE it, or SET-and-leave it?** — ride → zone 2; set-and-leave → a display flow.
   Never "it's a knob, so zone 2." (§2 *usage-not-type*)
5. **Smell test.** If your face shows *more at once* than acidface or chordblossom do, you've probably kept
   the pre-face flatness. Suspect it and push depth back into the display — fewer visible things is the
   feature (Hick's Law), not a compromise.

The meta-rule under all five: **the face's job is to MOVE depth off the surface and into the display — so when
in doubt, disclose.** The wrong route is always the one that leaves everything showing at once.

## 3 · Hardware lineage (where the shape comes from)

- **Elektron** (Digitakt / Digitone / Analog Rytm) — the screen-flanked-by-soft-keys and the
  modifier-overloaded 16-trig row (zones 3 + 4).
- **Roland x0x** (TB-303 / TR-808 / TR-909) — the step-button row and the *select-instrument-then-the-row-
  is-its-steps* workflow (the TR-8S model = exactly zone 4's double duty).
- **Pure Acid** (JimAudio, iOS) — the mobile proof: three x0x racks with an Elektron-style touchable
  display, tabbed by `BASS/SEQ/MIXER/DRUMS`. The reference we studied.
- Korg **electribe** / Novation **Circuit** / Synthstrom **Deluge** — the knobs-on-top + pads/keybed-on-
  bottom family (zones 2 + 5).
- **Teenage Engineering** (OP-1 / OP-Z / Pocket Operators) — not a layout to copy but the *discipline*:
  minimalism as a feature (subtract, don't add), and the explicit use of **Hick's Law** (fewer options →
  faster) + **Fitts's Law** (big spaced targets) that underwrites §2's ergonomics. The lesson is a small
  surface that *invites pressing*, not a dense one that must be studied.

Two paradigms often coexist in one face (Pure Acid does): a **pitched** lane edits step-write
(note/rest/pitch + step advance — the 303 way, no grid), while **drums** edit as a grid. The face holds
both; the flow/mode decides which editor the display + grid present.

## 4 · Worked examples

- **[`acidface`](../../tools/carts/acidface.c)** — the reference demo (acid box). Zone 2 = the 6 303 knobs;
  zone 3 = SEQ (touchable piano-roll + ACC/SLD/TIE/CLR/RND) / PAT / SONG / MIX / FX / SCOPE flows; zone 4 =
  the 16-step row doing double duty (NOTE lane = step-select + keybed pitch entry; drum lane = drum cells),
  with SHIFT → pattern-select; zone 5 = the keybed. Built on [`acidwire`](../../tools/carts/acidwire.c)'s
  device-honest scaffolding (real finger units, shape flipper, safe-area skin) per
  [`interactive-wireframes.md`](../guides/interactive-wireframes.md).
- **chordblossom** — the paradigm's proven second face; **fully annotated in §1c.** The short version:
  zone 2 = the promoted voicing/tone knobs; zone 3 = the **strum plate itself** (the hero surface,
  played-upon); zone 4 = the chord-quality row ↔ progression slots; zone 5 = the keybed root-*selector*.
  It transferred, and its Omnichord two-surface shape is what taught §2's *hero-surface-owns-the-display*
  and *selector-vs-performer* rules.
- **more-note-bass** — the paradigm's third face and its *cautionary* one; **annotated in §1d.** A groovebox
  where the on/off drums stay on the surface (zone 4) but the pitched bass + the sound pads move into the
  flow-switched display (a BASS piano-roll + a SOUND pad panel), keybed = transpose. It re-derives acidface's
  NOTE lane, and its two wrong-then-right readings produced §2's *depth-decides* + *usage-not-type* rules and
  the §2b reading guardrail.
- **pocketbox** — the paradigm *treating a hardware clone's pain*; **worked out in §1e.** A full 8-track
  tracker faithful to the PGB-1 (128×64 OLED, no knobs, hold-combos), so its problems (14-page paramscroll,
  one shared strip, invisible holds) are the exact ones the face exists to fix — grow the OLED to a real
  display, add comfort knobs, turn the holds into visible flows, all over the *same* sequencer brain. It also
  proves the true hold-step p-lock gesture already ships (see §5).
- **Relation to acidrack/epiano** — the [acidrack brief](acidrack-layout-brief.md) accordion and this face
  are two answers to the same problem; §2's phone→tablet principle says the face is the phone answer and the
  accordion/grid is the tablet answer, unified by the nav spine. The [epiano brief](epiano-layout-brief.md)
  is already a degenerate device-face (keybed-dominant, one sound panel).

## 5 · Open questions / next steps

- **Zone 4, the two-mode row** — build TRIGGER↔STEP-EDIT so the kit can be any size (the maker wants a
  bigger drum kit). This is the concrete next build in acidface.
- **Build chordblossom as a face** (§1c is the paper design) — the concrete next build after acidface's
  two-mode row. It's the first face to exercise the *display-as-performance-surface* rule (the strum plate),
  which acidface never does, so it's the real proof that zone 3 can be *played*.
- **Landscape** — flank the display with the knobs (use the width) instead of just shrinking every band;
  decide against Pure Acid's landscape once we have its shots.
- **The nav spine as the real backbone** — a top `view-tab` strip (BASS/DRUM/MIX) that **focuses** a face
  (not "swaps" — §2): on a phone the others collapse to tabs, on a tablet they stay stacked and the tab
  just highlights. Decide per zone which surfaces are **persistent** (e.g. the drum pad row stays under the
  thumb) vs. folded. acidface already has the *flow-level* tabs (the soft-keys); this is the missing
  *face-level* strip, and it's what lets several faces live in one app.
- ~~**Parameter locks (hold-step → knobs edit that step)**~~ — **PROTOTYPED in acidface (2026-07-13)** as a
  LOCK latch (`LOK`/`k`): the 6 knobs retarget to the selected step, turning one stamps a per-step value.
  Proves the zone-2↔4 contextual link. **The true hold-a-step gesture already SHIPS in pocketbox** (hold a
  step key + strip edits that step — §1e), so that half is done; *remaining in acidface:* port the real hold
  (multitouch) and actually *use* the locked values (acidface is silent — they're stored + shown, not sounded).
- **Conditional trigs — depth without clutter, and it's emergent.** Per-step conditions (probability,
  every-Nth-loop, fill) turn a static pattern into one that varies from simple rules — the maker's
  prefer-emergent-behaviour grain. A candidate flow the display hosts, not new chrome.
- **Does zone 4 belong in a library?** If two+ carts adopt the mode-switched grid, extract it (the
  [`scale-grid.md`](scale-grid.md) "build in a cart first, extract when a second wants it" precedent).
- **Fold back into acidrack** — once the paradigm is proven on acidface + chordblossom, revisit whether
  acidrack's Phase 3 re-land should be a device-face-per-machine rather than the accordion.

## Related

[`device-adaptive-layout.md`](device-adaptive-layout.md) (the engine plan — this rides on `finger_px()` +
resizable canvas) · [`responsive-instrument-ui.md`](../guides/responsive-instrument-ui.md) (the *process*;
this is the *shape* it produces) · [`acidrack-layout-brief.md`](acidrack-layout-brief.md) +
[`acidrack-ui-research.md`](acidrack-ui-research.md) (the accordion answer + the x0x survey) ·
[`epiano-layout-brief.md`](epiano-layout-brief.md) · [`interactive-wireframes.md`](../guides/interactive-wireframes.md)
(how acidface is built) · [`design-system.md`](design-system.md) §6/§8 (roles, widget swap) ·
[`runtime/lay.h`](../../runtime/lay.h) + [`runtime/ui.h`](../../runtime/ui.h) + `runtime/disclose.h` (the primitives).
