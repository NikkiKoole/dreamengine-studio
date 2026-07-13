# The device-face paradigm — a reusable shape for a deep instrument on a small screen

STATUS: EXPLORING (2026-07-13) — reverse-engineered from Pure Acid (JimAudio) with the maker, prototyped
live in [`acidface`](../../tools/carts/acidface.c). A general UI template, NOT an acidrack task; this doc
crystallizes the pattern so it can be reused across instrument carts (chordblossom, epiano, the radios…).

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

**Three independent mode drivers** repaint different surfaces — this is the "shallow surface, deep by
context" idea in the concrete:

**A · a soft-key repaints the DISPLAY only** (row + keybed untouched):

| flow | the display shows |
|---|---|
| **SEQ** | the editor for the active lane (see B) |
| **PAT** | pattern picker — 6 slots |
| **SONG** | an 8-slot pattern chain |
| **MIX** | 4 faders (MST/OSC/DRM/ACD) |
| **FX** | 3 knobs (DLY/FB/RVB) *inside* the screen |
| **SCOPE** | animated waveform (reacts to tempo + last note) |

**B · a lane chip re-purposes the STEP ROW and the SEQ display together:**

| lane | ⑤ step row | ③ SEQ display | ⑥ keybed |
|---|---|---|---|
| **NOTE** | **step-select** (tap = pick step; lit = has note) | touchable piano-roll + `ACC SLD TIE` (sel step) · `CLR RND` (pattern) | sets the selected step's pitch, then advances |
| **BD/SD/CH/OH** | **drum cells** (tap = toggle hit) | whole-kit overview (4×16) + `CLR RND` | auditions only |

**C · SHIFT hijacks the STEP ROW** into pattern-select (buttons 1–6 pick the pattern; transport shows
`PAT-SEL`). Plus **play** lights the running step across the row and in the display.

One line: **the soft-keys reskin the screen; the lane chips re-purpose the row + SEQ screen together;
SHIFT temporarily hijacks the row.** Three surfaces, each doing several jobs by mode. *(What's NOT here
yet is exactly §5: the knobs aren't contextual, the row is a small lane-strip kit not the two-mode
16-voice row, and there's no face-level tab in ① — the flow soft-keys are the within-face level.)*

## 2 · The principles that make it work

- **Disclosure by MODE, not by panel size.** The accordion promotes panels folded→compact→expanded to fit
  the band. The face keeps one instrument at full size and swaps *what a surface means* (flow, grid mode).
  Cheaper on vertical budget; reads as hardware.
- **The display is the editor AND the toolbox.** Because it's touchable, contextual ops (clear, random,
  copy, flag-a-step) live *in* the display next to what they act on — not as chrome around it. On hardware
  those ops must be physical buttons because the glass can't be touched; we don't have that constraint, so
  we don't inherit those buttons.
- **The mode-switched grid scales the instrument's size.** A 4-voice kit fits a lane strip; a 16-voice kit
  does not — but one row that is *voices in one mode, steps in another* fits any kit. So "support a bigger
  kit" and "the row does double duty" are the **same decision**.
- **Continuous vs contextual is the split that never moves.** Zone 2 (ride-live params) is always present;
  everything else is disclosed. Deciding which params are "always live" is the one real taste call per cart.
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
- **chordblossom** (candidate second example) — the same five zones: zone 2 = per-part voicing/tone params;
  zone 3 flows = chord-grid / progression / voicing view; zone 4 = chord slots ↔ progression steps
  (mode-switched); zone 5 = its MIDI keybed. Proving it transfers here is the paradigm's real test.
- **Relation to acidrack/epiano** — the [acidrack brief](acidrack-layout-brief.md) accordion and this face
  are two answers to the same problem; §2's phone→tablet principle says the face is the phone answer and the
  accordion/grid is the tablet answer, unified by the nav spine. The [epiano brief](epiano-layout-brief.md)
  is already a degenerate device-face (keybed-dominant, one sound panel).

## 5 · Open questions / next steps

- **Zone 4, the two-mode row** — build TRIGGER↔STEP-EDIT so the kit can be any size (the maker wants a
  bigger drum kit). This is the concrete next build in acidface.
- **Landscape** — flank the display with the knobs (use the width) instead of just shrinking every band;
  decide against Pure Acid's landscape once we have its shots.
- **The nav spine as the real backbone** — a top `view-tab` strip (BASS/DRUM/MIX) that **focuses** a face
  (not "swaps" — §2): on a phone the others collapse to tabs, on a tablet they stay stacked and the tab
  just highlights. Decide per zone which surfaces are **persistent** (e.g. the drum pad row stays under the
  thumb) vs. folded. acidface already has the *flow-level* tabs (the soft-keys); this is the missing
  *face-level* strip, and it's what lets several faces live in one app.
- **Parameter locks (hold-step → knobs edit that step)** — the highest-leverage borrow; prototype it in
  acidface so per-step accent/slide/pitch come from the knobs, not dedicated buttons. Proves the zone-2↔4
  contextual link.
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
