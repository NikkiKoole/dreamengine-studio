# acidcandy — the new iPad ROOMY layout (feature-parity checklist)

**STATUS: building** (Milestone 1 wired into `acidcandy.c` behind `rack_view==2`, coexisting with the old 2×2; the mockup's DECIDED features are live + interactive, the OPEN ones are the M2 backlog below).

## Wiring status (2026-07-23) — Milestone 1 SHIPPED into `acidcandy.c`

The layout is now a real `draw_rack2()` in `tools/carts/acidcandy.c` (helpers `r2_*`), dispatched
when `rack_view==2`. **Coexistence, default untouched:** the phone view (0) and old 2×2 (1) are
unchanged; reach the new layout via the **NEW** button (in the old 2×2 transport) and flip back
with **2×2 / HM**. On desktop: load acidcandy → **HOME** (→ 2×2) → **NEW** (→ roomy). On iPad it
still boots to the old 2×2 (safe while under review).

**Live + interactive now (M1 + maker's look pass):** sticky FOCUS (tap a nameplate) routing the big
shared screen; per-machine MUTE (LED); PLAY; TEMPO/SWING + GLU/FLT/PUMP + the 4-ch mixer (MST
column); the two 303 columns (CUT/RES/ENV/DEC/ACC + DRV/SEND/VERB + CL/DF voicing + KEY label); the
shared screen per focus — 303 NOTE GRID (tap=on/off, drag=draw melody, real pattern), drum 2D VOICE
GRID (tap/drag paints hits), MST automation lanes (PCF/CRU/GAT display). The DRUMS read as a
KEYBOARD: 808 = white keys (16, full-width, bottom row), 909 = black keys (11, narrower, spread +
centred over the 808 gaps, just above) — two separate rows, tap=select + audition-when-stopped. The
shared colour-coded VOICE context row (its OWN compact band above the drums, TUN/DEC/⟨char⟩/VOL/PAN/
FINE) follows the last-picked voice on either machine. Look pass (maker's call): nameplates in the
dos_8x8 in-game font; the knobs are the mockup's clean black-disc + coloured-outline + white-tick
style (`r2_knob`), NOT the candy bevel rotary, but with the identical gear/fine + double-tap-reset
drag feel.

**M2 SHIPPED (2026-07-23) — the soft-keys now switch the screen content:**
- **303**: SEQ (note grid) · FLAG (arm NOTE/ACC/SLD/TIE/OCT± then paint the grid) · GEN (CLEAR/MIN/
  MID/BUSY → the real `gen_line`).
- **808/909**: VCE (voice grid) · FLAG (ACC paint-drag; PROB cycle 100/75/50/25; 909 STRK cycle
  flam/drag/ratchet — drawn as shorter cells / pips) · GEN (CLEAR/MIN/MID/BUSY → `gen_drums`/`gen_drums9`).
- **MST**: MIX (4-channel meters) · PCF / CRU / GAT (each a big editable automation lane — drag to
  paint the per-step 0..7 level).

**303 screen reorg (2026-07-23):** SEQ+FLAG merged (one grid, flags = paint-tools) and GEN+KEY
merged into a SETUP panel → the 303 soft-keys are now just **SEQ | SETUP**, drawn as a 2-row bottom
band (tabs left-aligned, the flag palette a right-aligned 2×3 block, grid between). SETUP holds GEN
+ KEY editing (root/scale/octave — now editable). Drums still use the VCE/FLAG/GEN 1-row submenu
(same merge pending there). MUT + REC deferred (planned as on-screen buttons).

**M3 backlog (still ◻):** 303 PERF lenses; drum tab-merge (VCE+FLAG) + KIT minimap + PERF + MUT/REC
latches; per-step p-locks (TUN/DEC/⟨char⟩) as grid lenses;
909 METAL XY; the MST DELAY-division buttons + per-machine SEND (the FX SND knob covers the send
level, the division picker is still open). (DONE since M2: MST RES + FB — the column is a 2-wide
grid TMP|SWG / GLU|PMP / FLT|RES + FB; the 303 Devil Fish DEEP page — a CORE/DF-KNOBS page tab in
DF voicing swaps in SUB/ADEC/SLDT/TRK + a SAW/SQR WAVE toggle; and the on-screen tools now live in
a 2-row UI, a submenu row above the soft-keys; PAT pattern banks A/B/C/D per machine via a PAT
soft-key → bank pads in the submenu row, reusing pat_pad's instant/bar-quantized switching; and the
knobs are small discs with narrower columns so the pattern screen gets the width.) Do NOT flip the default off `draw_rack` until
the checklist is fully green AND it feels good in play-testing.

The [`acidcandy_ipad`](../../tools/carts/acidcandy_ipad.c) mockup is a ground-up redesign of
acidcandy's iPad view. The full model + open questions live in that cart's `de:meta`
(`node tools/orient.js acidcandy_ipad`); the layer-3 grammar context is in
[responsive-first-device-face.md](responsive-first-device-face.md). This doc is the one thing that
belongs *here*: the **feature-parity gate**.

## Why this doc exists

The new layout **replaces `draw_rack`** (acidcandy.c ~L2273, the `rack_view==1` path — a 2×2 of the
full phone device-faces the maker isn't happy with). But the app is **under App Store review**, so
the plan is **coexistence**: keep the old 2×2 as the shipping default, wire the new layout behind a
toggle (`rack_view==2` / dev flag), and flip the default **only when every phone-mode feature below
is reachable in the new layout AND it feels good**. This checklist is that gate. Do not delete
`draw_rack` until it's fully green.

- **✓** = has a home in the mockup already.
- **◻** = OPEN — no home decided yet (the real work of wiring).
- The "→" names where the feature should live in the new layout.

Source of truth for the phone feature set = acidcandy's own `de:meta` + `draw_303` / `draw_808` /
`draw_909` / `draw_mst`. Re-check against the cart when wiring (features ship fast).

## Global / transport
- [ ] ✓ PLAY (shared transport) → transport bar
- [ ] ✓ master TEMPO 60–200, draggable → MST column `TMP`
- [ ] ✓ master SWING (SWG, one rack-wide shuffle) → MST column `SWG`
- [ ] ✓ per-machine MUTE → nameplate LED (confirm the gesture: tap LED = mute, tap plate = focus)
- [ ] ✓ focus a machine (sticky) → tap the nameplate (replaces the phone's nav spine)

## 303a / 303b (each line)
- [ ] ✓ acid knobs CUT / RES / ENV / DEC / ACC → column knob stack
- [ ] ✓ FX DRV / SEND / VERB → column FX trio (same size as the drum FX)
- [ ] ✓ CL / DF voicing switch → column meta toggle
- [ ] ◻ DF **deep page** (SUB / ADEC / SLDT / TRK) → OPEN (a screen page? long-press the CL/DF toggle?)
- [ ] ◻ WAVE SAW / SQR toggle → OPEN (lives on the DF page today)
- [ ] ✓ SEQ / note entry (tap on/off, drag pitch, scale-snapped) → the screen NOTE GRID
- [ ] ◻ FLAG palette (NOTE / ACC / SLD / TIE / OCT± ) → screen `FLAG` soft-key (exists; wire the palette)
- [ ] ◻ loop LENGTH / polymeter (drag the ▼ marker) → OPEN (a lane above the grid?)
- [ ] ◻ PERF lenses (HALF / OCT / REV / ROLL; tap=latch / hold=momentary) → screen `PERF` soft-key
- [ ] ◻ GEN (CLEAR / MIN / MID / BUSY acid-riff generator) → screen `GEN` soft-key
- [ ] ◻ KEY — per-303 root / scale / whole-line octave → column shows the KEY *label*; **editing** is OPEN
- [ ] ◻ knob feel — drag sideways = fine, double-tap = reset → wire on the column knobs

## 808 (16 voices) / 909 (11 voices)
- [ ] ✓ every voice as a pad, one row, acid order → bottom pad bank (KI/SN/…)
- [ ] ◻ pad tap = SELECT + audition-when-stopped (the kept PICK rule) → bottom pads (wire it)
- [ ] ◻ MUT latch — per-voice mute (tap=latch / hold=momentary, dimmed + red slash) → OPEN on the strip
- [ ] ◻ REC latch — punch a pad onto the current step while playing → OPEN on the strip
- [ ] ✓/◻ VCE TUNE / DEC / [character] → the SHARED context panel (has TUN/DEC/ATK/SNP; confirm the 808's SNPY/THUD/TONE/RING vs 909's ATTK/SNPY/CLIK/TONE)
- [ ] ◻ VOL / PAN per voice → OPEN (context panel only has 4 knobs)
- [ ] ◻ FINE ±50-cent microtune → OPEN
- [ ] ◻ KIT minimap (full-roster density) → screen `KIT` soft-key
- [ ] ◻ FLAG depth palette — ACC / PROB / STRK(909) / p-locks TUN / DEC / <char> → screen `FLAG`
- [ ] ◻ per-step ACCENT / PROBABILITY / 909 STROKE (flam/drag/ratchet) → the screen grid + FLAG
- [ ] ◻ drag-across paint (fill / ramp a whole row in one gesture) → the screen grid
- [ ] ✓ machine FX DIST / SEND / VERB → bottom strip FX trio
- [ ] ◻ 909 METAL XY pad (tr909_metal highpass) → OPEN (bottom strip? screen FX page?)
- [ ] ◻ drum PERF (RP1/2/4 beat-repeat, THIN / BUSY density, ACC) → screen `PERF` soft-key
- [ ] ◻ drum GEN (CLEAR / MIN / MID / BUSY) → screen `GEN` soft-key
- [ ] ✓ the pattern itself (was: a 16-step hits row per voice) → subsumed by the screen 2D voice-grid
- [ ] ◻ ghost-pulse on a muted voice's would-be triggers → bottom pads (wire it)

## MST (master)
- [ ] ◻ GLU / FLT / RES / FB / PUMP → column shows TMP/SWG/GLU/FLT/PMP; **RES + FB are missing** — decide
- [ ] ✓ 4-channel mix overview → MST column mini-mixer
- [ ] ◻ DELAY division (1/16 · 1/8 · DOT · 1/4) → column shows "DLY 1/16"; the **selectable buttons** are OPEN
- [ ] ◻ per-machine SEND (to the shared delay) → OPEN
- [ ] ✓ master automation lanes PCF / CRUSH / GATE (drawable) → the screen (MST focus)
- [ ] n/a DUB pad — already REMOVED on the phone (don't re-add)

## FX routing (should carry over unchanged, engine-side)
- [ ] ✓ shared tempo-synced delay
- [ ] ✓ per-machine reverb (303s → plate, 808/909 → their plates, kick dry)
- [ ] ✓ per-machine drive

## The honest tradeoff (already accepted with the maker)
The 2×2 let you edit **all four patterns at once** (four tiny screens). The new layout shows **one
deep editor at a time** (whichever machine is focused) on one big, legible screen; play — knobs and
pads — stays live for every machine regardless of focus. If simultaneous *pattern* editing turns out
to matter in play-testing, the fallback is a hybrid (the shared screen with a peek of the other
patterns). Keep the 2×2 around until this is settled.
