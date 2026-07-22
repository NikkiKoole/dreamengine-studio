# acidcandy — the new iPad ROOMY layout (feature-parity checklist)

**STATUS: exploring** (draw-only mockup done; wiring not started).

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
