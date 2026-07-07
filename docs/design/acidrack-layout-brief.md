# acidrack — layout brief (the R1 worksheet)

STATUS: DRAFT (2026-07-05) — the first per-rack layout brief (device-adaptive-layout.md §"Phase 3
— revised plan" R1). **Waiting on the maker: the compact-strip taste calls (§2's open questions).**
Nothing here is implemented; this is the palette the R5 re-land gets built — and judged — against.

> **What this doc is.** The committed design artifact the acidrack reflow was missing (field note
> [018](../field-notes/018-passing-the-gates-felt-like-done.md)): the taste calls written down
> *before* layout code, so "done" means *matches this on device*, not "no audit findings". The
> template for every future rack's brief; keep it half-a-page-ish per section.

## 0 · House rules (apply to every rack — maker, 2026-07-05)

- **Decide what to show; never try to show everything.** But everything stays *reachable* —
  disclosure, not amputation.
- **Good icons are smaller than text.** Fantasy console, not an accounting app — LEDs, glyphs,
  7-seg displays where a word would eat the width (`flank` is the worked example; ReBirth is the
  proof at scale).
- **Use the fonts and colours we have** — `FONT_TINY`→`FONT_LARGE`, pico32, design-system §7 roles.
- **No shame in stealing from the homage.** ReBirth already solved this screen (1024×768, ~200
  controls) — steal its *information architecture*, never its mouse-precise control sizes.

## 1 · The three-state strip (the core model)

Every machine strip has three states; the disclosure pass (`disclose.h`, R2) promotes strips
**folded → compact → expanded** by finger-budget. From the ReBirth study: the original never shows
note data as an XY grid — every machine edits one lane at a time; the compact state *is* the
ReBirth machine module.

```
FOLDED (~1 slim row — exists today)
┌──────────────────────────────────────────────┐
│ 303-A  ▪▪·▪··▪▪·▪··▪·▪▪  [M]                 │  name · mini-pattern · mute
└──────────────────────────────────────────────┘

COMPACT (new — ~3-4 finger-rows: playable without expanding)
┌──────────────────────────────────────────────┐
│ 303-A   (CUT) (RES) (DRV)          [M] [fx]  │  the 2-3 knobs you ride live
│ ▪ ▪ · ▪ · · ▪ ▪ · ▪ · · ▪ · ▪ ▪   ◀playhead  │  one finger-sized step lane
└──────────────────────────────────────────────┘

EXPANDED (fills the panel area — exists today)
┌──────────────────────────────────────────────┐
│ 303-A   all 7 knobs                          │
│ full editor (piano roll / drum grid / …)     │
└──────────────────────────────────────────────┘
```

Why compact matters: **iPad = all five strips at compact simultaneously** (the whole ReBirth rack
on one screen — the flat/see-and-touch arrangement that doesn't exist today); **phone** = the
working strip expanded, 1–2 others compact, rest folded — the band stays touchable while you edit.

## 2 · The compact strip, per machine — OPEN TASTE CALLS (maker)

The whole rack's shape hangs on this table. Placeholders below are Claude's guesses to react to,
not decisions:

| strip | knobs that earn a compact slot | the lane | also | open questions |
|---|---|---|---|---|
| 303-A / 303-B | CUT · RES · DRV? | step LEDs (ReBirth-style) or our mini-pattern? | [M] [fx] | does WAVE earn a slot? ENV instead of DRV? |
| 909 | ? (per-voice knobs don't fit — selected voice's TUNE/DEC?) | the **selected voice's** 16 steps + a voice-selector | [M] [fx] | selector as icon row or prev/next? does AC (total accent) show? |
| 808 | same question as 909 | same voice-selected lane | [M] [fx] | share the 909's compact design 1:1? |
| MASTER | delay TIME · FB? GLU? | the PCF lane? or no lane | GEN/WAV? | is MASTER even worth a compact state, or folded↔expanded only? |

Decide too: **where mute/fx sit** in compact (right edge as today?), and whether the compact knob
count is fixed (always ≤3) or per-machine.

**Per-instrument controls the main screen must carry (maker, 2026-07-07) — now in `acidwire`:**
- **Mute** — a `[M]` toggle per strip in *every* state (header right). Muted reads unmistakably: red
  strip border + lit-red M + the pattern/step activity greyed (not green) = "there but silenced."
- **Patterns — 6 per instrument**, in their **own bordered "PAT" box** clearly grouped with the
  instrument (ReBirth's per-machine PATTERN panel — maker, 2026-07-07), not a loose full-width row.
  Placement by state: **expanded** + **compact** = a titled box (3×2 of the 6) on the strip's
  LEFT, machine controls fill the rest to its right; **folded** = a framed row of 6 LEDs in the
  header. Live pattern lit; muted → the box border/LEDs grey/red too. Distinct from the transport's
  A/B/C/D banks (top) and the song-chain row (bottom) — this is *which of the instrument's 6
  patterns plays*. Try it: `node tools/play.js acidwire run` → `m` mutes, `p` cycles the pattern.
  (Open: does each instrument also get its own bank A/B/C/D like ReBirth → 6×4, or is 6 flat?)

## 3 · Editor swap by budget (expanded state, per shape)

Same pattern data, two editors — the panel-level twin of design-system §8.3's widget swap:

- **Roomy (iPad/desktop):** keep our editors — the 303 piano roll (a better overview than ReBirth
  ever had), the full drum trigger grid.
- **Phone:** swap to ReBirth's one-lane editors — the 303 **step programmer** (one octave of
  finger-wide keys + OCT/ACC/SLD flags + step advance — fits a phone width at full finger size);
  drums = voice selector + that voice's single lane.
- OPEN: does the phone step programmer replace the roll outright, or is the roll still reachable
  (pinch/loupe) for overview?

## 4 · Arrangements per shape (the 2–3 topologies)

| shape | arrangement |
|---|---|
| roomy (iPad, either orientation) | **all 5 strips at compact**, stacked; tap one → expands in place (others stay compact until budget forces demotion) |
| tall (phone portrait) | working strip **expanded** + 1–2 **compact** + rest **folded**; song row pinned |
| short-wide (phone landscape) | **tabs** (the acidfit finding: accordions degenerate short) — tab bar + one expanded strip |

**Pinned always:** transport + bank buttons (top), song-chain row (bottom). Disclosure hides
secondary panels, never the performing surface.
**Orientation policy:** `free` (revisit after the compact strip exists — if phone-portrait plays
well, landscape may matter less).

## 5 · Footprints (fingers, for disclose.h — fill in once §2 is decided)

Each strip declares three footprints in finger units (44pt): folded / compact / expanded, sized so
the *controls inside* stay finger-comfortable (acidfit's lesson: comfort threshold, not fit
threshold). To be measured against the §2 sketches — don't guess these in code first.

## 6 · Done means

- Matches §4 on device — **including the iPad all-compact arrangement** (the one the first pass
  never built).
- Every control ≥ 1 finger-unit at every shape, or has a declared remedy (loupe / editor swap).
- `ui-audit --explore --resize` matrix clean + the R4 judgment pass clean.
- **The maker played each arrangement on glass** (baked, not screenshots) and calls it delightful.

## 7 · Device matrix — the baseline to design against (regenerable)

**The shape list + logical sizes + the K=2 pixel-chunk rule now live in
[`device-matrix.md`](device-matrix.md) §2** (the carried, cross-rack reference — grown to 11 shapes
incl. iPhone SE). Design this rack against that matrix; don't re-table it here. This section keeps
only the acidrack-*specific* before-picture and agenda.

Committed snapshot of acidrack's *current* reflow across the matrix (the before-picture the redesign
improves on): [`device-matrix.png`](device-matrix.png) (whole-matrix render) +
[`acidrack-device-matrix.png`](acidrack-device-matrix.png) (the earlier 7-shape snapshot).

**What it shows (the agenda):** the expanded panel leaves a big **dead void** at every size (worst on
iPad) — kill it; phone (logical W<300) takes the 2-row transport, iPad the 1-row; landscape correctly
degenerates to **tabs**; the iPad is almost entirely unused → the all-compact rack (§4). Safe-area
insets (notch / home-bar) are wired engine-side (`de_set_safe_area` → `safe_rect()`), used by the
transport + chain row + the app home-chip, but don't appear in this desktop sheet.

Related: [`device-adaptive-layout.md`](device-adaptive-layout.md) (the plan + ReBirth study) ·
[`design-system.md`](design-system.md) §6/§8/§9 · [`rebirth-classic.md`](rebirth-classic.md) (the
cart's own design doc) · `tools/carts/acidfit.c` (the disclosure prototype).
