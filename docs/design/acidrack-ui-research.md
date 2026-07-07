# acidrack UI research — the 303 / 909 / 808, and how they scale

STATUS: RESEARCH (2026-07-07) — the external survey behind the taste calls in
[`acidrack-layout-brief.md`](acidrack-layout-brief.md) §2/§5. Hardware anatomy + what the best
software clones changed + the touch/density numbers, so the brief's open questions get **decided
against evidence**, not guessed. No decisions here — this doc supplies ammunition; the brief owns
the calls.

> **Why this exists.** The brief asks "does WAVE earn a compact slot?", "voice selector as icon row
> or prev/next?", "what finger-footprints?" — questions with real answers in how the 303/808/909 and
> their clones (ReBirth, Phoscyon 2, ABL3, Roland Cloud, the iOS acid apps) actually lay out. This
> is that answer, synthesized from hardware manuals, clone reviews, and platform HIGs. Sources at the
> bottom.

## 1 · Three laws that fix "too big / too small"

Both failure modes — a few blown-up meaningless knobs, or an unreadable cram — are **one bug:
density drifting out of range.** Hold density roughly constant across sizes and both vanish.

1. **Reflow between bands, scale within a band.** Same aspect / bigger window → uniform-scale a
   fixed layout (cheap, consistent — the Vital/Serum lineage). Genuinely different aspect
   (phone-portrait vs desktop) → **reflow at a breakpoint**. Scaling *across* an aspect change is
   precisely what forces the cram-or-inflate choice. (This is the `lay.h` reflow + fixed-canvas
   letterbox split the engine already has.)
2. **When space grows, reveal more — never enlarge fewer.** A bigger canvas gets *more per screen*
   (unfold strips, all four machines, step lanes, meters), not the same 3 knobs inflated. Three
   knobs filling a 4K window is the smell. This *is* the brief's "iPad = all five strips at compact"
   payoff — the answer to the dead void §7 flags.
3. **48 px hit area — the visual may be smaller.** Every tappable control captures ≥48 px (satisfies
   Apple 44 pt + Material 48 dp at once). The *drawn* knob can be 32 px with a transparent fat-finger
   pad — `ui.h` already does this. Size earns rank by hierarchy, not by leftover room. Feeds §5.

## 2 · TB-303 — the six-knob soul (evidence for the brief's 303 row)

The identity is **one horizontal row of six equal, small silver knobs**, in canonical order:

> Tuning · Cutoff Freq · Resonance · Env Mod · Decay · Accent

No visual hierarchy *inside* the row — that democratic sameness is the brand. Waveform (saw/sqr) is
a switch beside them; Tempo is the one physically-larger knob (a global, not a tone control); Volume
+ Tuning read as utility. **Every clone leaves the six-knob row untouched and innovates only in the
sequencer** — the one thing hardware lacked: a *visible, editable* per-step grid with pitch, accent,
slide, and gate as separate layers.

| clone | what it does with the face | transferable move |
|---|---|---|
| **ReBirth RB-338** | exact edit knobs; piano-button step-write; per-device 32-pattern selector the original lacks | patterns are *switched*, not reprogrammed live |
| **D16 Phoscyon 2** | keeps the 6; adds ~7 trims but *subordinates* them (smaller/greyed) so it reads cleaner than v1; graphic 64-step seq that **collapses** for small screens | extras stay quiet; the sequencer is the co-hero and can fold away |
| **AudioRealism ABL3** | 6 on the face; calibration trims moved to a separate **Setup** view; resizable window; 4 pattern-editor layers (note/accent/slide/gate) | hide the non-canonical controls on another page |
| **Roland Cloud / TB-03** | "layout unchanged"; adds Overdrive + Delay knobs + an LCD + a modern step-write mode | knob row sacred; add fx as *quieter* controls |

**iOS acid apps** (Troublemaker, Pure Acid, DB-303): the small control count is an *asset* on touch.
Knobs become **tap-and-drag-vertically** (needs a drag zone taller than the visible circle, fine
adjust on slow drag). iPad shows the whole thing on one screen; iPhone **pages/tabs** the sequencer
away from the knobs, or shrinks visible steps.

**What this says to brief §2 (303-A/B row) — evidence, not a decision:**
- The **6-knob row is the expanded state's spine**; don't cluster or resize within it.
- For the *compact* slot: clones treat **Cutoff + Resonance** as the two everyone reaches for
  (the acid gesture); Env Mod / Decay / Accent are the "shape it" tier; Tuning is utility.
  → CUT · RES are the safe compact pair; the third slot (DRV vs ENV vs WAVE) is the live taste call.
  WAVE is a *two-state switch*, not a knob — cheap to keep as a tiny toggle even in compact.
- **Orange = hot** everywhere (accented step, playhead, record-arm) lets you drop text labels when
  tight — matches the brief's "good icons are smaller than text".

Height budget on roomy bands: **knob row ~15–20 %, sequencer ~40–50 %** (co-hero, give it room),
transport ~10–15 %, the rest breathing space. The 303 look is deliberately *not* dense.

## 3 · TR-808 & TR-909 — the beat is a colored grid (evidence for the 909/808 rows)

Shared skeleton: **per-voice knob band on top, the single row of 16 step buttons on the bottom.**
The signature is the step row **split into four groups of four, colored red / orange / amber /
white** — each block is one quarter-note beat, so downbeats read at a glance. Keep that coloring;
it's the single biggest legibility win, and it costs nothing.

| | TR-808 | TR-909 |
|---|---|---|
| voice select | **one 12-way rotary** | **push-buttons** (hold Select + tap voice) |
| per-voice knobs | **sparse** — mostly one Level each | **dense** — a full cluster per voice (Kick: Level/Tune/Attack/Decay…) |
| extras | Accent is a selectable "instrument" | Shuffle & Flam |
| feel | calm, roomy top panel | busy top panel |

**The core question — one lane at a time vs. the full grid** (the brief's 909/808 rows hinge on
this). Hardware shows *one* voice's 16 steps at a time (focus, polymeter, fits a phone). Modern
software converged on the **full grid** (rows = voices) for overview + speed. But the grid
**mathematically cannot hold the touch floor on a phone**: 16 cells × ~48 px ≈ 900 px, and a phone
is ~360–410 px. So the answer is **band-dependent — and it's what the brief already sketches**:

- **Phone (compact/tall):** the brief's "selected voice's 16 steps + a voice-selector" is exactly
  right and is what the faithful clones + iOS apps do. On the *selector*: hardware 808 is a rotary,
  909 is prev/next-style buttons; on touch, an **icon/label row of the ~11 voices** reads faster than
  prev/next (one tap to any voice vs. cycling) — but prev/next is narrower. Evidence leans icon-row
  where width allows, prev/next as the fallback below it.
- **Tablet / desktop (medium/expanded):** the **full grid** — every voice a row, whole pattern
  visible, ~48–64 px cells. This is the ReBirth "see & touch" payoff the brief's §4 iPad arrangement
  wants. Roland's own software defaults single-row and *expands to per-voice lanes* — precedent for
  the exact folded↔expanded swap.

**When 16 cells won't fit: fold, don't crush** (feeds §5 footprints):

| shape | step layout | ~cell |
|---|---|---|
| desktop / tablet / phone-landscape (≳700 px) | 1 × 16 | 48–64 px |
| phone portrait (≲480 px) | **2 × 8** (still chunked in 4s) | ~40 px |
| tiny / lo-fi 320 | **4 × 4** reshape | ~93 px |

Never add *columns* to fit more — page or zoom (Logic's "page overviews"). Keep the within-beat gap
small and the between-beat gap ~2–3× larger (Gestalt proximity groups the beats for free).
Comfortable row count is **4–8 voices**; divide every 4 rows past that. Don't rely on hue alone
(colour-blindness) — pair each beat colour with a brightness step or heavier border, meet 3:1
non-text contrast.

**What this says to brief §2 (909/808 rows):** share the 909/808 compact design 1:1 (the brief's own
guess) — the *lane + selector* is identical; only the expanded knob density differs (909 dense, 808
sparse). Compact knobs: the **selected voice's** TUNE/DEC are the ones worth riding (they're what
change per-voice); global AC (accent) is a candidate for the shared strip, not per-voice.

## 4 · Touch floors & density (feeds brief §5 — the numbers, in fingers)

One finger-unit = **44 pt (Apple) / 48 dp (Material)**; use **48 px** as the single safe target for a
control's *hit area*. Derived comfort minimums:

- **Knob (rotary drag):** visual ≥44–48 px if it's a primary drag target; may render ~32 px *only*
  with a ≥48 px pad and ≥8 px separation. A cramped knob is unturnable, not just hard to hit — err
  large.
- **Step button:** 44–48 px with ~8 px gaps. A naive 1×16 on a 375 pt phone = ~23 pt/step (half the
  floor) → this is *why* portrait folds to 2×8/4×4.
- **Spacing carries hierarchy:** tight *within* a group (4/8/12 px grid), larger *between* groups.
  ~30–40 % structural whitespace overall — structural (separating groups), never accidental (the gaps
  left when 3 knobs got scaled onto a big screen).

These are the comfort thresholds §5's folded/compact/expanded footprints get sized against — recall
acidfit's lesson: **the footprint is a control-comfort threshold, not a box-fit threshold.**

## 5 · The band table (2–4 breakpoints is the professional norm)

Switch on the **measured shape** (min-side + orientation), not a device name — a short landscape
phone defers more than a tall portrait one.

| band | trigger | 303 | drum grid | the rack (all 4) |
|---|---|---|---|---|
| **compact** (phone) | min-side ≲480 · portrait | knob row + single-lane step programmer; full knobs one tap away | **one voice** at a time, full-width 16 (or 2×8) + selector | 1 strip expanded, others folded (brief §4 "tall") |
| **medium** (tablet) | ~600–840 | knob row + full 16-step grid + accent/slide lanes | **full grid**, per-voice knobs reveal on select | all 5 strips at compact, or folded panels / tabs |
| **expanded** (desktop) | ≥1024 · landscape | full panel — every knob + sequencer + fx | full grid + all per-voice knobs + shuffle/flam | **all strips at compact simultaneously** — the ReBirth payoff |

This is the same topology set as brief §4 (roomy / tall / short-wide); the numbers here are what
`disclose.h` (R2) and `finger_px()` (R3) enforce per band.

## 6 · How this maps onto the brief's open questions

| brief §2 open question | what the evidence says (the maker still calls it) |
|---|---|
| 303: does WAVE earn a slot? ENV instead of DRV? | CUT · RES are the two everyone rides; WAVE is a cheap 2-state toggle (not a knob) so it can stay tiny even in compact; the 3rd knob slot is the real DRV-vs-ENV call |
| 909/808: selector as icon row or prev/next? | icon/label row where width allows (one tap to any voice); prev/next as the narrow fallback |
| 909/808: does AC (total accent) show? | accent is *global* on hardware → belongs on the shared/master strip, not per-voice compact |
| 808 share the 909's compact design 1:1? | yes — lane + selector identical; only expanded knob density differs |
| MASTER worth a compact state? | it has no sequenced lane like the machines; folded↔expanded is defensible — the odd one out |
| compact knob count fixed ≤3 or per-machine? | ≤3 holds for the 303 (CUT/RES/+1); drums want ~2 (selected-voice TUNE/DEC) — so *per-machine* fits the evidence better than a fixed count |

## Sources

Hardware: [Roland Cloud TB-303 manual](https://www.rolandcloud.com/getmedia/b3e2da28-1951-467e-bd79-7aae34481570/TB-303-Manual-E.pdf) ·
[TB-03 manual](https://static.roland.com/assets/media/pdf/TB-03_eng03_W.pdf) ·
[Vintage Synth — TR-808](https://www.vintagesynth.com/roland/tr-808) ·
[MusicRadar — anatomy of the TR-909](https://www.musicradar.com/how-to/anatomy-of-a-roland-tr-909-the-classic-drum-machines-features-explained) ·
[io808 tutorial (step-colour groups)](https://io808.com/tutorial).
Clones: [gearnews — Phoscyon 2](https://www.gearnews.com/d16-phoscyon-2-review/) ·
[MusicRadar — ABL3](https://www.musicradar.com/reviews/tech/audiorealism-bass-line-3-636138) ·
[MusicRadar — D16 Drumazon](https://www.musicradar.com/tuition/tech/how-to-program-patterns-in-d16-drumazon-633370) ·
[Sweetwater — Roland TR-808 software (grid option)](https://www.sweetwater.com/store/detail/TR808plug--roland-tr-808-drum-machine-software) ·
[Sound on Sound — Troublemaker (iOS)](https://www.soundonsound.com/reviews/bram-bos-troublemaker).
Responsive/HIG: [Apple HIG — layout/44 pt](https://developer.apple.com/design/human-interface-guidelines/layout) ·
[Material — 48 dp targets](https://support.google.com/accessibility/android/answer/7101858) ·
[NN/G — breakpoints](https://www.nngroup.com/articles/breakpoints-in-responsive-design/) ·
[Vital forum — GUI scaling](https://forum.vital.audio/t/gui-scaling-resizing-options/1217) ·
[JUCE — constrained aspect-ratio resize](https://forum.juce.com/t/correct-way-to-resize-with-constrained-aspect-ratio/24974).

Related: [`acidrack-layout-brief.md`](acidrack-layout-brief.md) (the calls this feeds) ·
[`device-adaptive-layout.md`](device-adaptive-layout.md) (the plan + ReBirth study) ·
[`rebirth-classic.md`](rebirth-classic.md) (the cart's design doc) ·
[`design-system.md`](design-system.md) §6/§8 · field note
[018](../field-notes/018-passing-the-gates-felt-like-done.md).
