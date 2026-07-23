/* de:meta
{
  "slug": "acidcandy",
  "collection": ["device-face"],
  "title": "Tiny Acid Jam",
  "resizable": true,
  "status": "active",
  "created": "2026-07-14",
  "kind": [
    "instrument",
    "generative"
  ],
  "genre": null,
  "teaches": [],
  "description": {
    "summary": "A candy-toy acid RACK on the device-face skeleton at 160x100 x4: a colour-cartridge nav strip of five FOCUSABLE machines — 303a / 303b / 808 / 909 / MST — all on one transport, packaging acidrack's guts as the device-face paradigm instead of an accordion. Every voice is honest, shared from runtime/ headers so it can't drift.",
    "detail": "Nav = faces (tap a cartridge body to FOCUS, tap its LED to MUTE from anywhere). Sound: two TB-303 lines on the shared runtime/acid303.h (303b an octave up = the bass+acid-lead duo), the full TR-808 on the extracted runtime/tr808.h and the TR-909 on runtime/tr909.h (both byte-honest to the tr808/tr909 carts). Each 303 face: the top gear-drag knob row is always the acid knobs — vanilla CUT/RES/ENV/DEC/ACC, or (via an inline DF switch at the row's right end, tinted the machine colour) a DEEP Devil Fish page (SUB octave-down sub / ADEC two-decay / SLDT slide time / TRK filter tracking + a SAW/SQR WAVE toggle that re-defines the oscillator). The green screen has soft-keys like the drums — SEQ (the LCD roll), FLAG (a palette NOTE/ACC/SLD/TIE/OCT+/OCT- you arm then paint on the bars — NOTE toggles the note itself so you add notes WITHOUT leaving for SEQ; loop LENGTH/polymeter is now a ▼ marker you DRAG in a lane above the note-bars, not a flag), FX (DRV/SEND/VERB as LCD-native green knobs), PERF (5 live-play lenses — HALF/2X speed, ACC accent-everything, STAC/GLIDE slide-flip — non-destructive; each is TAP=latch (persists across faces, white border) or HOLD=momentary), and GEN (a CLEAR + MIN/MID/BUSY generate menu). The 16 NOTE-BARS below (tap = on/off, drag up/down = pitch, snapped to a minor-pentatonic so it stays musical). Drum faces (808 blue, 909 amber): the TOP row is always the machine FX (DIST/SEND/VERB, the 909 also a METAL XY pad for the metal-voice highpass, tr909_metal) — no FX toggle. (SWING is one master SWG on the MST face, ReBirth-style — it shuffles the whole rack, drums AND both 303s, so they lock.) The green SCREEN is the voice inspector: soft-keys pick its content, split by SCOPE — LEFT = the selected voice (VCE/FLAG + the MUT pad-latch), RIGHT = the machine (GEN/PAT/PERF/KIT). Stickiness follows the scope: a voice-pad tap snaps a machine-scoped screen (GEN/PAT/PERF/KIT) to VCE, but a voice-scoped one (VCE/FLAG) stays put — so you can paint flags across the whole kit without re-selecting, yet never land on a voice while staring at the bank picker. The screens — VCE (the default: the picked voice's TUNE + DEC as LCD-native green knobs, plus a voice-specific CHARACTER knob only where the machine has one — SNPY/THUD/TONE/RING on the 808, ATTK/SNPY/CLIK/TONE on the 909, faithful to tr808/tr909; voices without one show just the two) / KIT (the kit minimap) / FLAG (a depth palette) / GEN (a CLEAR + MIN/MID/BUSY generate menu) you arm then paint on the cells: ACC (per-step accent, a red marker + louder hit) / PROB (vertical-slide a cell for its trig-chance, 100/75/50/25 = a shorter bar, the RD-8 move) / STRK (909 only — cycle none→flam→drag→ratchet, a blue glyph cut by black lines — more cuts = busier, via tr909_fire_stroke) / and the per-step p-LOCKS TUN / DEC / <character> (each its own button — the palette is two rows: ACC/PROB(/STRK) on top, the p-locks below; the character button appears only on voices that have one, e.g. SNPY). Arming a p-lock turns the cell lane into a BIPOLAR bar-from-centre lens editing that knob's per-step OFFSET (centre = follow the voice knob, up/down = ± its range); all three offsets play at once, each riding its voice knob so the knobs still transpose the whole contour. PROB and the p-locks DRAW ACROSS — drag over the row and each cell takes the finger's height, so a whole probability ramp or melodic/decay/timbre contour sweeps out in one gesture (the PCF-lane / note-bar feel). A single-row voice picker holding the FULL roster (16 tiny 2-char pads on the 808, 11 on the 909, acidrack's acid order — full name in the VCE header): a pad tap always SELECTS, and AUDITIONS only while STOPPED (the kept PICK rule — an off-grid hit would smear a running groove). Two modeless latches re-flavour the pads, both TAP=latch / HOLD=momentary (the PERF-lens grammar — the old 4-way PICK/PLAY/MUTE/REC cycle tool is gone): MUT (left column) makes each pad tap a DIRECT per-voice mute (muted = dimmed + red slash, silenced but kept in the pattern; the pad rims tint orange while MUT is live), and REC (a tall red latch on the far-right edge) is the audible opt-in while playing — a pad tap FIRES and punches onto the current step (live/overdub record). And the 16 hits on the bottom (808 blue, downbeats brighter to find the 4th; the playhead column lights up). FX routing: a shared tempo-synced delay + per-machine reverb (303s → a warm hall, 808/909 → their own plates with the kick kept dry) + per-machine drive. MST: master GLU/FLT/RES/FB/PUMP; SWG + master TEMPO as a matching pair of knobs in the LCD's right gutter (SWG = the ONE rack-wide swing, ReBirth's model; tempo drives the step clock + the tempo-synced delay); a 4-channel mix overview; DELAY division as small buttons under the LCD; + per-machine SEND. The MST LCD is now the same size as the other faces. Knob feel: vertical = value, pull sideways for a fine gear, double-tap resets. Widgets that live INSIDE the LCD (the flag/depth palettes, NEW, the drum VCE voice knobs, the 303 FX knobs) use LCD-native variants — lcdbtn (pure green outline, filled when armed) + lcdknob (green outline + value arc) — so they read as part of the readout, not the candy chassis. (The old step-row + keybed is kept behind use_bars = 0.)",
    "controls": "Tap a cartridge to focus a machine; tap its LED to mute. PLAY runs the shared transport. 303: drag CUT/RES/ENV/DEC/ACC (sideways = fine, double-tap = reset); the inline DF switch (right of the knob row) flips to the DEEP page (SUB/ADEC/SLDT/TRK + a SAW/SQR WAVE toggle); SEQ/FLAG/FX/GEN soft-keys switch the screen between the roll, the flag palette (arm ACC/SLD/TIE/OCT+/OCT-, then tap bars; loop length = drag the ▼ above the bars), the FX knobs (DRV/SEND/VERB), and the generate menu (CLEAR / MIN / MID / BUSY); on the note-bars tap = note on/off, drag up/down = pitch. 808/909: DIST/SEND/VERB (+909 METAL XY) sit on top always; tap a voice pad to pick it (auditions only while stopped; while playing, light REC to hear taps) — a machine-scoped screen (GEN/PAT/PERF/KIT) snaps to VCE on a voice tap, a voice-scoped one (VCE/FLAG) stays put; VCE shows TUNE/DEC/[character]/VOL/PAN/FINE (character where the machine has one, SNPY/THUD/TONE/RING/ATTK/CLIK); the picker shows the whole roster in one row (all 16/11 voices, acid order); MUT (left column, tap=latch / hold=momentary) flips pad taps to DIRECT per-voice mute — orange pad rims while live; the far-right REC latch (same grammar) punches pad taps onto the current step while playing; left soft-keys VCE/FLAG + MUT, right GEN/PAT/PERF/KIT (the generate menu = CLEAR / MIN / MID / BUSY, KIT = the minimap). Cells: tap = place a hit, DRAG across = paint a fill (VCE/KIT); in FLAG, the palette is two rows — ACC/PROB(/STRK) on top, the p-locks TUN/DEC/<character> below — arm one then work the cells (ACC tap = accent, PROB vertical-slide = trig-chance, STRK tap = cycle flam/drag/ratchet, TUN/DEC/char vertical-slide = a per-step bipolar offset around that voice knob). MST: GLU tames level, FLT is the live DJ filter, PUMP ducks to the kick; SWG + TEMPO are a matching knob pair in the LCD's right gutter (SWG = the one rack-wide swing for drums + both 303s; drag TEMPO 60-200 BPM); the DELAY division buttons (1/16·1/8·DOT·1/4) sit under the LCD, + per-machine SEND; a little DUB pad in the bottom-right corner momentarily throws the delay (HOLD + drag: X = time, Y = feedback)."
  },
  "todo": [
    "FOR ANOTHER TIME — SOLO in the MUT grammar (parked 2026-07-22; the maker LIKES the new MUT latch, build on it): now that MUT is a clatch pad-latch (tap=latch / hold=momentary, pads mute directly), SOLO is the natural sibling. Candidate shapes, pick on play-feel: (a) a SOLO latch key — but the left col is full (VCE/FLAG/MUT), so either a half-height MUT/SOL stack or a second state ON the MUT key (tap again while latched = solo mode, a different colour); (b) a gesture while MUT is live — e.g. tap = mute, hold a pad = solo it (note: solo drops are beat-critical too, so if hold feels late here the same way hold-to-mute did, prefer shape a); (c) machine-level: hold/latch a cartridge LED to solo that machine (mute the rest) — the cheap live win already suggested in the mute-SCENES entry. Whatever the shape, the semantics: solo = mute everything else BUT remember the hand-set mute pattern, un-solo = restore it (don't wipe the maker's mutes).",
    "SESSION 2026-07-22 — drum-face PANEL RESHUFFLE / pad-tool dissolution (SHIPPED): the far-right 4-way PICK/PLAY/MUTE/REC cycle tool is GONE (a hidden meta-mode — the maker forgot it existed, remembered it as 3-way; mute was buried 2-3 taps into the cycle). (1) PICK+PLAY merged into the DEFAULT pad tap = SELECT + audition-when-STOPPED. (CORRECTED same day: the first cut auditioned while playing too — the maker flagged that the old PICK-silent-during-playback rule was DELIBERATE, an off-grid hit smears the running groove and was exactly why PLAY existed as the audible opt-in. Now: playing + no REC = silent select; REC lit = the tap fires audibly AND punches the step — REC absorbed PLAY's job.) (2) MUTE = a MUT key in the LEFT soft-key column with the PERF-lens grammar via clatch() (the new candy-chassis twin of lcdlatch): TAP=latch (white border) / HOLD=momentary — each pad tap then mutes DIRECTLY, no hold delay (hold-to-mute was tried earlier and REJECTED: live mutes must land on the beat); the picker pad rims tint ORANGE while MUT is live (a mini mode-hint-outline). (3) REC = a dedicated red latch strip where the tool was (same tap=latch/hold=momentary; on touch, hold REC with the thumb + tap pads = momentary punch-in); lit + playing → a pad tap also writes the current step. (4) SCOPE-AWARE stickiness: a voice tap snaps machine-scoped screens (GEN/PAT/PERF/KIT) to VCE, voice-scoped ones (VCE/FLAG) stay sticky — fixes 'select a voice, then also tap VCE' WITHOUT breaking flag-painting across voices (the earlier always-snap-to-VCE attempt broke exactly that). (5) columns now honest to scope: LEFT = the voice (VCE/FLAG/MUT), RIGHT = the machine (GEN/PAT/PERF/KIT — KIT moved right; it was the one machine key masquerading as a voice key). PLAY-TEST WATCHLIST: latched-MUT footgun (next 'select' tap mutes instead — mitigated by white border + orange rims, but maybe auto-unlatch after N idle seconds if it bites); the 909's orange MUT rims on amber pads (contrast); 4 right keys = shorter buttons.",
    "SESSION 2026-07-21 — PERF / GEN / voice-panel overhaul (SHIPPED): (1) 303 PERF reworked to HALF + OCT (octave-down every OTHER step, the acid bounce) + REV (reverse the read) + ROLL (HOLD-to-stutter the last played note, HALF-aware rate); the 2X/8-12 speeds AND the AC3/AC4 accent cross-rhythms were tried and DROPPED (not musical — the maker's call). (2) DRUM PERF layer added, per machine (a PERF soft-key on 808/909): beat-repeat RP1/RP2/RP4 (grid-locked N-step slice loop via drum_effstep), DENSITY THIN (kit → downbeats+accents) / BUSY (fill the SELECTED voice's off-16ths), ACC (accent all). Grid-locked by construction (flows through the step grid, not a retrig clock). (3) 303 reverb tank retuned warm-hall → bright PLATE (reverb(0.45,0.22)) for the modern stab space. (4) MST CLR is now a small TOP-RIGHT CORNER overlay on the PCF/CRUSH/GATE lanes (columns beneath it clip their capture) — no longer steals a header row. (5) 808/909 VCE now folds in MIX = ONE panel (TUNE/DEC/[CHAR]/VOL/PAN/FINE) + a per-voice MUTE toggle in the header + a proper VCE soft-key (left col VCE/KIT/FLAG); the MIX soft-key is RETIRED (DS_MIX enum kept, unused). (6) GEN rewritten into an ACID-RIFF generator (gen_line): a scale-degree MOTIF repeated across the bar, octave climb on the last repeat, root-anchored accented downbeat, slides gliding between neighbours, key/scale-aware (mscale). (7) responsive polish on the 303 face: one shared right-column width (DF switch aligns with GEN/KEY/PAT), even soft-key spacing (colcell), PERF/PAT bottom-aligned to the glass; nav cartridge labels no longer spill left on tall/square window ratios. (8) the 303 BARS→GRID toggle is HIDDEN (static grid_view_on=0 — note-bars are the shipping view; the editable GRID is kept intact, flip to expose). REMAINING OPEN (punch-list): DRIFT knob (below); DUB-pad + GATE-lane EAR-CHECKS (GATE wants a master VOLUME to ride = the level[] master-vol TODO); per-step 303 CUT/RES p-lock lanes; exact-BPM entry (tap/±); LIVE/step record; a SOLO gesture; the drvmode waveshaper; mode HINT-OUTLINES; the SONG layer + SAVE/LOAD (deferred until the feature-set stabilises → one state struct first); the mascot/SOUL; the iPad ROOMY tablet layout.",
    "VOICING switch: SHIPPED (2026-07-20) — the 303 knob-row's right end now stacks TWO switches (audio-notes.md §26): TOP = VOICING (classic 'CL' ⟷ Devil Fish 'DF', per-303, LED lit = DF; flips ac[i].classic + re-acid_define, NON-destructive — the DF knob values survive), BOTTOM = DUAL-PURPOSE: in DF it's the VIEW page-tab (core-5 '1' ⟷ DF-extras '2'); in classic it becomes the SAW/SQR WAVE switch. The old single 'DF' button (a mere kpage flip) is gone. WAVE-IN-VANILLA (fixed 2026-07-20): the SAW/SQR switch is a stock-303 CORE control, but it lives on the DF-extras page (unreachable in classic since kpage locks to 0). Since classic has no page to switch, the bottom slot hosts WAVE there instead → reachable in both voicings. (Engine was always fine — classic never gated a->wave.)",
    "303 DRIFT knob (OPEN, 2026-07-19): the 303 voices now carry analog drift (acid303.h Acid.drift, default 0.5 — slow LFO_SHAPE_RANDOM pitch+cutoff wander that killed the 'sounds kinda digital' tell; see audio-notes.md §26). It's a fixed bake right now; maker wants a little tweak somewhere. Cleanest: an MST-global DRIFT knob writing ac[0].drift=ac[1].drift=v then re-acid_define (or a per-303 knob on the DEEP page). Acid.drift is already a per-voice float so no ACID_* enum churn. Small follow-up, not yet wired.",
    "DUB PAD: REMOVED (2026-07-21) — the MST corner XY dub-throw pad (echo time/feedback while held) was cut as gimmicky. The MST right column is now just SWG + TEMPO (each half-height). The shared delay stays tempo-synced (DIVISION buttons + FB knob) in apply_fx; dub_held/dub_t/dub_fb globals gone.",
    "GATE lane: SHIPPED (2026-07-18) — a 3rd drawable master lane on the MST screen (MIX/PCF/CRU/GAT, soft-keys shrunk to h7 so 4 fit), the RHYTHM twin: PCF=tone, CRUSH=texture, GATE=chop. mgate[16] 0..7/step (7=open, drawn DOWN = the step cuts); apply_fx rides the master gate((1-o)*0.85 threshold, 2ms atk, 45ms rel) on STEP CHANGE (set-and-hold, guarded by aGate). Pink bars (0xA0u hashes). Verified audible: open-vs-full-gate WAV correlate 0.78, no NaN. CAVEAT (worth a play-test): gate() is a THRESHOLD/dynamics gate, not a pure per-step volume chop — the cut depends on the signal level, so it reads more as a rhythmic dynamics-gate than a clean trance-gate. If a cleaner chop is wanted, the real fix is a master VOLUME the lane can ride (the level[] master-vol TODO) — revisit if the feel is off.",
    "CRUSH lane: SHIPPED (2026-07-18) — a 2nd drawable master-automation lane on the MST screen, the PCF's TEXTURE twin (PCF = tone/filter; CRUSH = bit-depth grit). A 3rd MST soft-key (MIX/PCF/CRU) draws mcrush[16] (0..7/step, 0 = clean); apply_fx rides the whole-mix crush(16-c*13 bits, 1+c*24 rate, c mix) on STEP CHANGE only (crush() is NOT ride-safe → set-and-hold, guarded by aCrush; fx-frame lint passes). Drawn in ORANGE bars (vs PCF's green) so the two lanes read distinct; empty step = a floor tick. Same draw code + widget pattern as PCF (0xE0u hashes). Verified audible: clean-vs-full-crush WAV correlate 0.79 (same notes, degraded texture), no NaN. The engine already had crush/gate/tape/eq/chorus/reverb/grains as global master fx, so more lanes (e.g. a GATE rhythm lane) are cheap follow-ons if wanted.",
    "LCD GROW: SHIPPED (2026-07-18) — the green LCD glass on ALL FOUR faces (303/808/909/MST) is now h30 (y37-66), one consistent size; it CONTAINS the soft-key columns (PERF/PAT/GEN/KEY used to poke below) and leaves a 1px gap above the content below (drum voice pads at y68; 303 note-bars nudged 67→68, bh 27→26 so the loop handle stays at y94). MST caught up (2026-07-18): the master TEMPO moved to a compact KNOB in the LCD's right gutter (gknob — shows the real VALUE label, not knob()'s 0-99; bpm01 maps 0..1 → 60..200, rounded to int so no per-frame fx re-apply), which freed the full-width y64 row so the LCD could grow. DELAY stayed as its 4 small division buttons (maker preferred buttons to a knob) — now tucked UNDER the grown LCD in the y67-78 band (set mdiv directly, label = the division). The 303 SEQ roll fills the taller glass (baseline 56→61, taller playhead) and the MST MIX faders/PCF lane grew to fh22. Also: accented notes in the 303 roll get an ORANGE CAP in the space ABOVE the note (the 'accents use space above' ask) — matches how the drum faces mark accents above the cell.",
    "OPEN — REC / mode HINT OUTLINES: when you engage a mode (the drum REC pad-tool, or arm a FLAG), draw an OUTLINE around the buttons/cells you then need to press — spatial 'what do I do now' guidance. (Sibling idea, a transient LCD TOAST that NAMES the action, was prototyped 2026-07-18 and PARKED — maker didn't like it; don't rebuild without asking.)",
    "PARKED — mute SCENES: deprioritized 2026-07-18. For MACHINE-level muting it's largely redundant with the top-nav mute LEDs (one tap vs a few); the only real gain is one-tap recall of a COMPLEX state incl. per-voice drum mutes. The genuinely-more-than-muting version (mutes + which bank + levels + PERF = a 'verse/chorus/drop') IS the deferred SONG layer — build that, not a mute-only scene. If a cheap live win is wanted instead: a SOLO gesture (hold/latch a cartridge LED to solo, mute the rest) clearly beats the LEDs.",
    "PERF lens follow-ups — MOSTLY RESOLVED 2026-07-21 (see the session entry at top): (2) octave + reverse lenses → OCT + REV SHIPPED; (3) a DRUMS PERF layer → SHIPPED (beat-repeat + THIN/BUSY + ACC); (1) the 'funny accent order' / AC3/AC4 cross-rhythm accents → tried and DROPPED (not musical). STILL OPEN: only the minor edge — a momentary HOLD can't override a conflicting LATCH (update precedence dbl>half / stac>glide), now just HALF vs the others.",
    "808/909 completeness: SHIPPED (2026-07-16) — EVERY voice is a pad in ONE picker row (no pages): 808 = 16 tiny 2-char pads, 909 = 11 wider ones, in acidrack's ACID ORDER (kick/hats/clap/snare first, fills last). The full name shows in the VCE screen header on select. The KIT minimap is a full-roster density grid (all 16/11 voices as 1px rows). Pads DOUBLE-DUTY: a MUT soft-key flips a tap between SELECT+audition and per-voice MUTE (dmute/d9mute — dimmed + red slash, silenced in playback + dimmed in the minimap), on top of the machine mute. A muted pad still GHOST-PULSES (light-grey flash) on its would-be triggers — dtrig is set on the would-trigger, the fire() is what's gated by mute — so you can see the silenced pattern playing and time an unmute.",
    "drum DEPTH: SHIPPED (2026-07-16) a KIT/FLAG soft-key pair on both drum faces (the 303's SEQ/FLAG idiom) — FLAG fills the LCD with a depth palette (ACC/PROB, 909 also STRK) you arm then paint on the cells: per-voice-per-step ACCENT (fires boost +2, red marker in the gap above the cell), trig PROBABILITY (vertical-slide → snap_prob 100/75/50/25, shorter bar = lower chance, rnd()-gated in update()), and 909 STROKE (cycle none→flam→drag→ratchet via tr909_fire_stroke, blue pips). The 909 metal XY (m9cut/m9res, tr909_metal) is now a live XY pad in the 909 FX panel. NEW resets a voice's depth too. Still open: per-step accent SWEEP + a clearer stroke glyph than pips.",
    "p-LOCKS: SHIPPED (2026-07-16) — a LOCK flag in the drum FLAG palette. Arming it makes the cell lane a BIPOLAR per-step OFFSET lens (centre = follow the voice knob, up/down slide, drawn across); the offsets live in doff/d9off[LK_TUNE|LK_DEC|LK_CHAR] and are applied by swapping the voice's TUNE/DEC/CHAR knobs around each fire, so the voice knobs still transpose the whole contour. Each is its OWN button — the FLAG palette is now two rows (ACC/PROB(/STRK) on top, TUN/DEC/<char> below; the character button only shows on voices that have one). All three offsets play at once. FOLLOW-UPS: (1) a way to UNLOCK a step back to 0 (currently only NEW/GEN clears — maybe a plain tap zeroes it); (2) the same p-lock lens for the 303 knobs (CUT/RES).",
    "per-voice LANES (VOL + PAN): SHIPPED (2026-07-17) — the p-locks generalised into a data-driven LANE list (drum_lanes(): each lane = name + base-knob + per-step offset + stable id + sink; the FLAG palette AND the fire both read it, so paint/lens/palette are lane-generic and the darmed↔lane map is by stable id not row position). VOL = per-voice LEVEL folded into the per-hit velocity (boost) — truly per-voice, no engine change. PAN = per-voice STEREO via new additive tr808_pan()/tr909_pan() helpers (toms/congas GROUP-pan their shared slot — hardware-honest; independent tom/conga pan = the deferred 14-slot-bank split, device-face-paradigm §2c). MST SCP chip was a LIN/PWR pan-LAW toggle (PAN_LINEAR centre-full/mono-safe vs PAN_POWER equal-loudness); REMOVED 2026-07-18 once stereo was confirmed working — the rack is fixed at PAN_LINEAR. Two bugs fixed en route: dpan/d9pan were UNINITIALISED (defaulted hard-LEFT), and tr808_pan was pushed EVERY fire (queued set-and-hold → control-queue flood = clicks) — now cached + pushed only on change. Adding the next continuous lane = one drum_lanes row (+ an engine param to make it sound). FOLLOW-UPS: the MIX sub-view (next todo) now homes VOL/PAN so VCE is uncrowded again; the same lane system could still feed the 303 CUT/RES.",
    "drum MIX + FINE microtune & per-303 KEY: SHIPPED (2026-07-17; MIX RE-MERGED into VCE 2026-07-21). MIX was briefly its own soft-key (VOL/PAN/FINE) to un-crowd VCE — but as of 2026-07-21 it's folded BACK into a single VCE panel (TUNE/DEC/[char]/VOL/PAN/FINE in one row) and the MIX soft-key is retired (see the session entry). FINE = a per-voice ±50-cent MICROTUNE (dfine) via additive tr808_tune()/tr909_tune() (instrument_tune, push-on-change) so two kicks can be NULLED against each other; the coarse TUNE knob keeps its musical semitone steps (a continuous-TUNE experiment was built then DROPPED on play-test — user wanted best-of-both). KEY (melody): each 303 now owns its ROOT + SCALE + whole-line OCTAVE on a KEY soft-key (303 face, right col under GEN); mroot/mscale/loct are per-line [2]; KEY removed from MST (back to MIX/PCF). Scales min/maj-pent, minor, dorian, major, chromatic (generalised from the hardcoded PENTA); loct = ±2 whole-line octaves, DISTINCT from the per-step OCT flag. ALL default byte-identical (root0/min-pent/oct0/fine0; verified vs the prior render SHA). OPEN: the '2 303s can be in different keys' is intentional freedom with no coherence rail — a shared-key/link option could return if it bites; per-step 303 CUT/RES lanes still open.",
    "ARRANGEMENT: pattern BANKS SHIPPED (2026-07-17) — per-machine A/B/C/D via a PAT soft-key (303 right col; drums shrank the right col h7->h6 so GEN/MUT/MIX/PAT stack 4-high). A slot stores the SEQUENCE only (steps + flags/lanes + length), sound stays global; copy-on-switch (pat_switch). Empty slots seeded plen=STEPS/prob=100 so a fresh slot is playable. LIVE-SET (2026-07-18): bank switches are now BAR-QUANTIZED while playing — a bank tap ARMS the slot (pat_queue → armpat[machine]), the pad BLINKS on the 16th, and the switch LANDS on the next bar downbeat (step==0 in update(), before the 303/drum blocks read their arrays so the new pattern plays from step 0) — so the whole rack changes pattern in time (arm several machines, they flip together on the downbeat). Re-tap the current/armed slot = cancel; STOPPED = instant switch (no bar to wait for); stop drops pending arms. First live-set feature (queued banks = perform the arrangement by hand, the lightweight stand-in for the deferred SONG layer). Mute SCENES are the planned companion (parked for now). STILL OPEN, DEFERRED: the SONG/PROJECT layer (the maker's '6 songs in master' — 6 whole-rack snapshots + save). DEFERRED ON PURPOSE until the synths/feature-set STABILISE — persisting state that's still churning = a save format that rots every time a knob/lane/mode is added. When it's time: consolidate the ~40 scattered globals into ONE state struct FIRST, then save = a trivial versioned dump (save_bytes) instead of a hand-maintained 40-field blob. Also open: a SONG CHAIN (arrange banks over time) + WAV export.",
    "SWING: MASTER shuffle SHIPPED (2026-07-18) — the per-machine swing8/swing9 knobs are GONE; ONE g_swing (rack-scope) is now the SWG knob in the MST right-gutter pair (with TEMPO, same size), ReBirth's model — one shuffle for the whole rack. Resolves BOTH old opens: (1) the 303s NOW swing too, so bass/lead lock into the drum shuffle; (2) it's tempo-scaled. Mechanism (the acidrack/tb303 split): drums CAN be scheduled ahead so they DRAG the odd-16th fire onset in ms (swms = sw*(15000/bpm), tempo-scaled); a 303's held voice CAN'T (slide/tie hold the handle), so it delays its step FLIP instead — laststep303 + ctr303 = ((ctr&1) && frac<sw) ? ctr-1 : ctr, and acid_gate gets the swung onset. BOTH use the SAME fraction sw = g_swing*0.60 (0..0.6 of a 16th) so they lock. g_swing 0 → ctr303==ctr → dead straight (default). SAVE/LOAD (autosave patterns/knobs) still open — acidrack has it.",
    "TEMPO: master BPM SHIPPED (2026-07-18) — was hardcoded 132 in THREE places (init bpm(), the step clock, the tempo-synced delay in apply_fx) + a 113ms stroke. Now ONE g_bpm (rack-scope) drives all of them, a draggable BPM readout on the MST face (right of the delay row, 60-200). Phase-accumulated (g_phase) so a live tempo change moves the RATE, never JUMPS the counter; the delay re-times on change (set-and-hold); the 303 screen shows the live value. OPEN: a tap-to-type / +/- for exact BPM; tempo-scale the swing + PCF stroke feel.",
    "PERF lenses: SHIPPED (2026-07-18, reworked 2026-07-21) — a per-303 PERF soft-key (left col SEQ/FLAG/FX/PERF) fills the LCD with NON-DESTRUCTIVE read-path lenses (TAP=latch / HOLD=momentary, lcdlatch). CURRENT set (post-rework): HALF (half-time speed) · ACC (accent every note) · OCT (octave-down every other step) · REV (reverse read) · STAC / GLIDE (slide-flip) · ROLL (hold-to-stutter the last played note). Mechanism kept from the prototype: laststep303[2] = each line its own swung, speed-lensed counter; GLIDE skips the staccato acid_gate; STAC forces slide=0; ACC ORs accent in. (The old 2X + 8-12 speeds were removed — see the session entry.)",
    "SLIDE-GATE BUGFIX (2026-07-18, found via the GLIDE-lens question): the 303 staccato acid_gate was called UNCONDITIONALLY between flips — it cut the held voice at 70% of EVERY step, sparing only a cut-into-a-tie (next_ties). So a slid step got cut before it could glide → the next step retriggered FRESH instead of sliding: acidcandy's manual slides NEVER actually glided (and tie steps got chopped at 70% too). tb303 had it right all along (`else if (on[s] && !sld[s])`); acidcandy dropped that guard. FIX: gate only a fresh, NON-slid note — `if (on[i][ls] && !slide)` (slide = pf_stac ? 0 : sld[ls]; STAC still forces staccato). PROVEN by WAV correlate: manual all-SLD now == a pure glide (1.00000), and a no-slide/no-tie pattern is byte-identical before/after (1.00000) so only slid/tied steps change. acidrack is on its own step_303/gate_303 path and was never affected. NB: this changes how existing SLID/TIED committed patterns sound (they now glide/hold instead of chop) — the correct 303 behaviour.",
    "PERF LATCH: SHIPPED (2026-07-18) — the PERF buttons are now TAP=latch / HOLD=momentary (lcdhold → lcdlatch). A quick tap (release within PERF_TAP=12 frames) TOGGLES a persistent per-lens-per-line latch (pf_latch[PL_N][2]); a longer press is momentary (on while held, no toggle). The latch PERSISTS across faces + stop/start, so 303a can keep doubling while you work 303b — the whole point (momentary alone only touched the FOCUSED line). draw() seeds each line's effective pf_* from its latch, then the focused PERF screen's lcdlatch adds hold on top. HALF↔2X and STAC↔GLIDE are mutually exclusive (tapping one clears its partner via the excl arg). Latched = WHITE border, momentary = green fill. EDGE (noted in the follow-ups todo): a momentary hold can't override a conflicting latch (update precedence dbl>half, stac>glide).",
    "FLAG note-on: SHIPPED (2026-07-18) — added FL_NOTE as the FIRST entry in the 303 flag palette (NOTE/ACC/SLD/TIE/OCT+/OCT-) and made it the DEFAULT armed flag. Arming NOTE makes a bar tap/drag toggle the note itself (flag_get/set → on[i][s]), so you add notes right on the FLAG screen instead of maneuvring to SEQ and back — the maker's repeated ask. Pitch still lives in SEQ (drag) — NOTE keeps the bar's existing pitch. paint-across works (grab an off bar → paints on, grab an on bar → erases).",
    "drum-machine ergonomics: (1) CLEAR — SHIPPED (2026-07-16) as part of a GEN flow. Every instrument has a GEN soft-key (right margin, replacing the decorative SCP) that fills the LCD with a CLEAR + MIN/MID/BUSY menu — CLEAR empties the pattern, the three densities regenerate it (MID = the old NEW recipe). Replaces the single NEW button. (2) LIVE RECORD — still open: while the loop plays, a mode where tapping a voice pad punches its hit onto the CURRENT step (step/overdub record), instead of only auditioning.",
    "waveshape flip: SHIPPED (2026-07-16) a SAW/SQR WAVE toggle in the 5th knob slot of the 303 DEEP page (flips a.wave + re-calls acid_define). Still open: the drvmode waveshaper (SOFT/HARD/FOLD/ASYM) could get the same treatment.",
    "303 DEEP knobs: SHIPPED (2026-07-15) a DF soft-key flips the zone-2 row between vanilla (CUT/RES/ENV/DEC/ACC) and a DEEP page (SUB/ADEC/SLDT/TRK) — the headline Devil Fish knobs, per-303, sub-osc wired on slots 36/37 (34/35 belong to the 909's 13-slot bank). DRV moved to the FX panel (aligns with the drums). Still not surfaced: ATK (soft attack) + SQL (env-sweep) + the accent-SWEEP mode; a fuller flow could page those too.",
    "wire the still-decorative soft-keys: SHIPPED (2026-07-15) the per-machine FX panel — the FX soft-key on every 303/808/909 opens an aligned DRV/DIST + SEND (shared delay) + VERB (reverb) row (draw_fxrow); reverb is real (303s → warm hall tank 0, 909 → tight plate tank 1, 808 → room tank 2, kicks kept dry) + per-machine drum drive. every soft-key is now live: the 303/drum SCP slots → GEN (2026-07-16); MST SCP → the LIN/PWR pan-LAW toggle; MST SNG removed (SONG layer deferred); the 909 METAL XY shipped into its FX panel; MUT/REC are arm-toggles by the voices.",
    "SOUL: the LCD screens have no MASCOT yet (the slime from tinyface/facemock, candy-style §3) — a FACE flow could give a bopping/reacting creature its own screen. Deferred for now.",
    "graduate the gear-drag knob (vertical=value, sideways=fine gear, double-tap=reset) into ui.h's ui_knob so every cart gets it. SCOPED 2026-07-20 (deferred — the shipped cart's ~40 knob sites work, so the retrofit is risk for no user-visible gain; the widget can land against a fresh demo cart first). Faithful graduation needs TWO new bits of ui.h surface, not one: (1) a persistent per-widget ring for the double-tap timing (UiCap is transient — gone one frame after release; acidcandy's kmeta[] ring is the model); (2) a generic 'a widget is being dragged this frame' signal, because _knobx pokes this cart's private g_drag_frame/g_drag_y drag-bounce guard and a generic widget can't know about it. Also: the sizing (radius keyed to cell HEIGHT for a consistent row margin, width only as an overflow cap) + the scale-clean bevel (rim = r/4) are the proven cell-fit maths to carry over. See docs/design/responsive-layout.md 'known gaps' (ui_knob entry).",
    "note-bars: a CHROMATIC toggle (out-of-key acid) + let a drag PAINT across several bars; MST could grow per-machine level faders.",
    "DRUM-FACE middle-panel cleanup — 808 + 909 (SHIPPED 2026-07-18): split the controls into TOOL vs VIEW. The pad TOOL (what a voice-pad tap does — PICK select+audition→shows TONE / PLAY just fire the voice (finger-drum, no select) / MUTE mute / REC punch) is now a TALL, NARROW 4-way selector down the FAR-RIGHT edge, flush against the pads it retargets; tap to cycle, the fill colour (PICK = the neutral brown button colour, PLAY green, MUTE orange, REC red) + the full word STACKED one letter per row (tiny font) show which — replacing both the cryptic tiny REC/MUT margin toggles (draw_arms, now DELETED) and the interim V/M/R letter-strip. The side soft-keys are PURE VIEWS. (UPDATED 2026-07-21: left col is now VCE/KIT/FLAG — VCE got its soft-key BACK, and MIX folded into it; right col GEN/PAT/PERF. Originally VCE had no key + MIX was left, reachable only via the PICK tool — that stranded the voice knobs, hence the fix.) Picker + hits are CENTRED (x6, matching the 303s) and pulled DOWN, which freed the corner below PAT. Went through mockups: left/right button columns, a GEN/PAT under-picker spill, a tall right slab (too heavy), a horizontal gap strip, rotated labels (too cramped), a corner rotary → landed on the far-right stacked-word button. 909 keeps its STRK flag + metal-XY. OPEN: (1) the freed corner below PAT wants a knob (a 4th tone param, or SWG moved down from the top row); (2) narrate the active MUTE/REC mode in the screen header too, as a second legibility cue."
  ]
}
de:meta */
#include "studio.h"
#define UI_MAX_WID 256   // iPad rack mode draws all 5 machines at once → far more than the default 64 widgets/frame; without this, every tap past widget #64 silently drops (dead panels)
#include "ui.h"
#include "cursor.h"   // pixel hand cursor (CUR_HAND/CUR_GRAB) — desktop only, auto-hides the OS arrow, no-op on touch
#include "acid303.h"
#include "tr808.h"    // the shared, honest TR-808 voice bank (the 808 face's SOUND)
#include "tr909.h"    // ...and the TR-909 (the 909 face's SOUND)
#include <string.h>  // memcpy/memset/memcmp — the SaveBlob snapshot/restore + autosave dirty-check
#include "lay.h"      // responsive containers (device-adaptive-layout.md): the faces reflow to fill
                      // the live canvas in LANDSCAPE via lay_split/lay_grid + finger_px() (FU) — never
                      // a camera scale (that desyncs ui.h; see CLAUDE.md). One focused face, spread to fill.
extern void de_resize(int w, int h);   // engine seam (platform.h): set the active canvas size. We drive
                                       // it to a CHUNKY ratio-matched canvas (see draw) so the 160×100
                                       // scales up crisp + the leftover ratio-offset spreads, no bars.

// ACID CANDY — a candy-toy acid RACK on the device-face skeleton, 160x100. The
// nav spine is a strip of colour CARTRIDGES you focus machines through (nav=faces);
// the acid VOICE is the shared runtime/acid303.h. This cart is the FACE + the rack.

#define STEPS 16
// knob-feel tunables (dial these while play-testing)
#define KNOB_SWEEP   24.0f   // canvas px for a full 0..1 on a straight vertical drag (smaller = snappier)
#define KNOB_GEAR    22.0f   // sideways px per +1x of fine gear
#define KNOB_GEARMAX 2.0f    // cap so FINE still covers real ground (max sweep = SWEEP*this)

// ── the rack: one cartridge per machine, all on one transport ────────────────
enum { M_303A, M_303B, M_808, M_909, M_MST, M_N };
enum { MK_303, MK_DRUM, MK_MST };
typedef struct { const char *name; int kind; int col, lo; int mute; } Machine;
static Machine mac[M_N] = {
    { "303", MK_303,  CLR_PINK,      CLR_DARK_PURPLE, 1 },   // pink 303 (bass) — MUTED by default (bring it in on record)
    { "303", MK_303,  CLR_ORANGE,    CLR_DARK_ORANGE, 1 },   // orange 303 (lead) — MUTED by default
    { "808",  MK_DRUM, CLR_TRUE_BLUE, CLR_DARK_BLUE,   0 },
    { "909",  MK_DRUM, CLR_YELLOW,    CLR_DARK_ORANGE, 0 },
    { "MST",  MK_MST,  CLR_GREEN,     CLR_DARK_GREEN,  0 },
};
static int face = M_303A;
static int rack_view = -1;   // LAYOUT: -1 = auto (from device_class on frame 0) · 0 = phone single-face+tabs · nonzero (2) = iPad ROOMY rack (draw_rack2 — sticky-focus, one big shared screen). HOME/nav toggles phone⇄ROOMY. (The old 2×2 draw_rack was removed 2026-07-23 — ROOMY is THE tablet view.)
static int r2_focus   = 0;   // ROOMY: which machine owns the big shared screen (0=303a 1=303b 2=808 3=909 4=MST) — sticky focus, set by tapping a nameplate. Play stays live for ALL regardless.
static int r2_selmach = M_808;   // ROOMY: the last-picked DRUM machine (M_808/M_909) — the shared context panel + its ring colour follow it (voice = dsel/d9sel)
static int r2_dpaint  = 0;       // ROOMY drum grid paint tool: 0=HIT (toggle) · 1=ACC · 2=PROB · 3=STRK (909) — the drum twin of the 303's `armed` flag palette
static int r2_octpen[2] = { 0, 0 };   // ROOMY 303 "active draw octave" pen, per line: +1 up / 0 center / -1 down. A note drawn on the roll lands at this octave; tapping the matching note erases it.
static int r2_303panel[2] = { 0, 0 }; // ROOMY 303: which control panel is REVEALED, per line (0 = none / roll gets the room · 1 = PERF · 2 = GEN), toggled by the bottom-left chips.
static int r2_drumpanel[2] = { 1, 1 };// ROOMY drum: which panel is REVEALED, per machine (0 = none · 1 = FLAG/paint · 2 = GEN · 3 = PERF), toggled by the bottom-left chips. Default FLAG so the paint tools are up.
static int r2_mstpanel = 0;           // ROOMY master: which panel is REVEALED (0 = none / mixer+lanes all-at-once · 1 = GEN song-generator · 2 = SONG save/load slots · 3 = DLY delay division), toggled by the bottom-left chips.
static int r2_mstlane  = -1;          // ROOMY master: which automation lane is EXPANDED to full height for precise editing (-1 = the 3-up overview · 0 = PCF · 1 = CRUSH · 2 = GATE). Tap a lane's label to toggle.
static int r2_dark     = 0;           // ROOMY chassis theme: 0 = salmon candy skin · 1 = dark. Toggled by the tiny header button (flips R2_PNL/INK/DIM).

// the two TB-303 lines (index 0/1 == machine M_303A/M_303B). Pattern lives here.
static Acid ac[2];
static int  on[2][STEPS], pit[2][STEPS], acc[2][STEPS], sld[2][STEPS];
static int  sel[2] = { 0, 0 };
// the note-bar editor snaps pitch to a chosen KEY (root + scale, set on the MST face) so it
// stays musical. DEFAULT = minor pentatonic at root 0 → byte-identical to the original snap.
// (avoid the name SCALE — that's the engine's -D pixel-scale flag.)
typedef struct { const char *name; int n; int deg[13]; } KeyScale;   // deg[] = semitones above root, ending on the octave
static const KeyScale SCALES[] = {
    { "m.PENT", 6,  { 0, 3, 5, 7, 10, 12 } },                  // minor pentatonic — DEFAULT (index 0 = the original PENTA)
    { "M.PENT", 6,  { 0, 2, 4, 7, 9, 12 } },                   // major pentatonic
    { "MINOR",  8,  { 0, 2, 3, 5, 7, 8, 10, 12 } },            // natural minor
    { "DORIAN", 8,  { 0, 2, 3, 5, 7, 9, 10, 12 } },            // minor with a raised 6th
    { "MAJOR",  8,  { 0, 2, 4, 5, 7, 9, 11, 12 } },            // ionian
    { "CHROM",  13, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 } },  // out-of-key acid
};
#define NSCALE (int)(sizeof(SCALES) / sizeof(SCALES[0]))
static int mroot[2]  = { 0, 0 };                               // PER-303 KEY root, 0..11 semitones (0 = original); added to that line's notes
static int mscale[2] = { 0, 0 };                               // PER-303 scale index into SCALES (0 = minor pentatonic = original)
static int loct[2]   = { 0, 0 };                               // PER-303 whole-line OCTAVE shift (played as +loct*12; 0 = original). NB distinct from the per-STEP OCT flag (oct[])
static const char *NOTE[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
static int scale_idx(int sc, int semi) { const KeyScale *S = &SCALES[sc]; for (int k = 0; k < S->n; k++) if (S->deg[k] == semi) return k; return 0; }
// SEQ note-bar paint drag: grab pos + axis-lock (0=undecided 1=pitch 2=on/off) + values
static int drag_gx, drag_gy, drag_axis, drag_paint, drag_on0;
// per-step DEPTH (the 303 flags) + polymeter
static int  tie[2][STEPS], oct[2][STEPS];   // TIE = hold prev note; OCT = octave -1/0/+1
static int  plen[2] = { STEPS, STEPS };     // per-line LENGTH (short = polymeter drift)
static int  lpos[2] = { 0, 0 };             // per-line playhead
enum { PS_SEQ, PS_FLAG, PS_FX, PS_GEN, PS_KEY, PS_PAT, PS_PERF };  // 303 LCD content: roll / flags / FX / generate / KEY (root·scale·octave) / PAT (A-D banks) / PERF (live play lenses)
static int  pscreen[2] = { PS_SEQ, PS_SEQ };  // per-303 screen mode (SEQ/FLAG/FX soft-keys)
static int  kpage[2];                        // per-303 knob page: 0 = vanilla, 1 = DEEP (Devil Fish + drive)
static int  seq_grid = 0;                     // 0 = the tall NOTE BARS (default, old); 1 = the editable wide GRID (drag notes on the SEQ screen). STAGE 1 of the acidwide-style layout.
static const int grid_view_on = 0;            // the BARS→GRID toggle is HIDDEN — the note-bars are the shipping view (the maker's call). The GRID view is kept fully intact; flip this to 1 to expose the chip again.
static int  keys_mode = 0;                    // GRID bottom row: 0 = STEP strip · 1 = KEYS keypad (13 chromatic notes + OCT-/OCT+/SLIDE, 303 step-entry into sel[i]). Stage 2. Toggled by the STEP/KEYS chip.
static int  key_oct   = 0;                    // KEYS entry octave offset (-1..+2), written into oct[i][sel]
enum { FL_NOTE, FL_ACC, FL_SLD, FL_TIE, FL_OCTU, FL_OCTD, FL_N };   // FL_NOTE = toggle the note itself (so you add notes WITHOUT leaving FLAG for SEQ). (LEN moved out — it's a per-LINE loop length, now a draggable handle at the end of the note-bars, not a per-step flag)
static int  armed = FL_NOTE;                 // which flag a bar-tap paints (default NOTE = add notes right from the FLAG screen)
static const char *FLNAME[FL_N] = { "NOTE", "ACC", "SLD", "TIE", "OCT+", "OCT-" };
enum { LK_TUNE, LK_DEC, LK_CHAR, LK_VOL, LK_PAN, LK_N };    // continuous-lane params (doff[] index). VOL = per-step level (velocity); PAN = per-step stereo
// drum-depth flags. STRK = 909-only. The continuous lanes (TUN/DEC/CHAR/VOL) map to LK_* by
// darmed = DD_TUN + lane.lk (a STABLE id, NOT the button's row position — a lane after the
// optional CHAR would otherwise land on the wrong slot). DD_* stays PARALLEL to LK_*.
enum { DD_ACC, DD_PROB, DD_STRK, DD_TUN, DD_DEC, DD_CHAR, DD_VOL, DD_PAN, DD_N };
static const char *DDNAME[DD_N] = { "ACC", "PROB", "STRK", "TUN", "DEC", "CHR", "VOL", "PAN" };
enum { DS_VCE, DS_KIT, DS_FLAG, DS_GEN, DS_MIX, DS_PAT, DS_PERF };     // drum LCD: VCE (tone+mix knobs, one panel) / kit / flags / generate / (DS_MIX retired — folded into VCE) / PAT (A-D banks) / PERF (ROLL·ACC)
static int  dscreen = DS_VCE;                 // (a voice tap snaps machine-scoped screens GEN/PAT/PERF/KIT to VCE; VCE/FLAG stay sticky)
static int  darmed = DD_ACC;                  // which drum flag a cell paints (FLAG mode)
// What a voice-pad tap DOES (drum faces): the DEFAULT is select — audition fires only while
// STOPPED (the kept PICK rule: an off-grid hit would smear a running groove; while playing, REC
// is the audible opt-in). Two modeless latches re-flavour it (both tap=latch / hold=momentary,
// the PERF-lens grammar — the old 4-way PICK/PLAY/MUTE/REC cycle tool is GONE):
static int  dmut_latch, dmut_hold, dmut_now;  // MUT (left soft-key col): pads MUTE directly — each tap toggles, no hold delay (live mutes must land on the beat)
static int  drec_latch, drec_hold, drec_now;  // REC (far-right strip): lit + playing → a pad tap ALSO punches onto the current step (live/overdub record)
static int  dmute[TR_NV]  = { 0 };            // per-808-voice mute (in addition to the machine mute); toggled by a pad tap while MUT is live
static int  d9mute[TR9_NV] = { 0 };           // per-909-voice mute
// 2-char pad abbreviations (indexed by voice enum) — so all 16/11 fit one picker row
static const char *AB8[TR_NV]  = { "BD","SD","LT","MT","HT","LC","MC","HC","RS","CL","CP","MA","CB","CY","OH","CH" };
static const char *AB9[TR9_NV] = { "BD","SD","LT","MT","HT","RS","CP","CH","OH","CC","RC" };
// per-voice CHARACTER knob (the 3rd VCE knob) — voice-specific, and ONLY on voices whose
// circuit reads the color param (mirrors tr808.c/tr909.c K2LABEL); NULL = just TUNE + DEC.
static const char *CH8[TR_NV]  = { 0,"SNPY","THUD","THUD","THUD",0,0,0,0,0,0,0,"TONE","TONE","RING",0 };
static const char *CH9[TR9_NV] = { "ATTK","SNPY","CLIK","CLIK","CLIK",0,0,0,0,"TONE",0 };
static int  paint_val = 0;                    // what a FLAG paint-drag writes (decided on the first cell)
static int flag_get(int i, int s, int f) {
    switch (f) {
    case FL_NOTE: return on[i][s];
    case FL_ACC:  return acc[i][s];
    case FL_SLD:  return sld[i][s];
    case FL_TIE:  return tie[i][s];
    case FL_OCTU: return oct[i][s] == 1;
    case FL_OCTD: return oct[i][s] == -1;
    }
    return 0;
}
static void flag_set(int i, int s, int f, int v) {
    switch (f) {
    case FL_NOTE: on[i][s] = v; break;
    case FL_ACC:  acc[i][s] = v; break;
    case FL_SLD:  sld[i][s] = v; break;
    case FL_TIE:  tie[i][s] = v; break;
    case FL_OCTU: oct[i][s] = v ? 1 : (oct[i][s] == 1 ? 0 : oct[i][s]); break;   // paint clears only its own dir
    case FL_OCTD: oct[i][s] = v ? -1 : (oct[i][s] == -1 ? 0 : oct[i][s]); break;
    }
}

// the 808 drum face — voices from the shared tr808.h (byte-honest to the tr808 cart).
// The 808 kit lives at slots TR808_BASE(9)..22; the 909 kit sits above it at 23+.
#define D909_BASE 23
static int   dgrid[TR_NV][STEPS];                          // the drum pattern
static float dtune[TR_NV], ddecay[TR_NV], dcolor[TR_NV];   // per-voice knobs (0.5 = neutral)
static float dvol[TR_NV];                                  // per-voice LEVEL (0.5 = unity); rides the per-hit velocity — line-scope base the VOL lane offsets around
static float dpan[TR_NV];                                  // per-voice PAN (0.5 = centre); applied via tr808_pan() around each fire (voice snapshots slot pan at note-on)
static float dpanlast[TR_NV];                              // last PAN pushed per voice — instrument_pan is a QUEUED set-and-hold ctrl, so only re-push on CHANGE (else the fire loop floods the control queue → clicks)
static float dfine[TR_NV];                                 // per-voice FINE tune (0.5 = 0; ±0.5 semitone = ±50 cents) — the MICROTUNE trim (null a kick beat); the coarse TUNE knob keeps its semitone steps
static float dtunefine[TR_NV];                             // last FINE detune pushed via instrument_tune — push on CHANGE only (it's a queued set-and-hold ctrl)
// SEAM: toms/congas share a slot → they pan as a GROUP (hardware-honest). Independent
// tom/conga pan = splitting the 14-slot bank into per-voice slots (device-face §2c). Deferred.
static int   dsel = TR_BD;                                 // selected drum voice
static float dtrig[TR_NV];                                 // per-voice flash: 1 on fire, decays (picker pad lights up)
// the 909 drum face — voices from the shared tr909.h
static int   d9grid[TR9_NV][STEPS];
static float d9tune[TR9_NV], d9decay[TR9_NV], d9color[TR9_NV];
static float d9vol[TR9_NV];                                // per-voice LEVEL (0.5 = unity) — see dvol
static float d9pan[TR9_NV];                                // per-voice PAN (0.5 = centre) — see dpan
static float d9panlast[TR9_NV];                            // last PAN pushed per voice — see dpanlast
static float d9fine[TR9_NV];                               // per-voice FINE tune — see dfine
static float d9tunefine[TR9_NV];                           // last FINE detune pushed per voice — see dtunefine
static int   d9sel = TR9_BD;
static float d9trig[TR9_NV];
static float m9cut = 0.40f, m9res = 0.33f;                 // the 909 metal-filter XY
// drum DEPTH — per-voice-per-step, painted in FLAG mode (mirrors the 303 flags)
static int   dacc[TR_NV][STEPS],  dprob[TR_NV][STEPS];                             // 808: accent flag + trig-prob %
static int   d9acc[TR9_NV][STEPS], d9prob[TR9_NV][STEPS], d9strk[TR9_NV][STEPS];   // 909: + stroke type (TR9_ST_*)
static float doff[LK_N][TR_NV][STEPS], d9off[LK_N][TR9_NV][STEPS];                 // per-step p-lock OFFSETs (TUNE/DEC/CHAR) from the voice knobs: -1..+1 (0 = follow the voice)
// (swing is now a single master g_swing, MST face — was per-machine swing8/swing9)

// ── per-step automation LANES (the growth point) ─────────────────────────────
// A continuous lane = a per-voice base KNOB + a per-step OFFSET array (doff/d9off),
// painted as a bipolar contour and applied AROUND the knob at fire-time (so the voice
// knob still transposes the whole contour). accent/prob/flam are the few QUANTIZED
// flags (kept special, above); tune/decay/character — and later pan/vol — are lanes.
// drum_lanes() is the ONE list the FLAG palette + the fire both read: adding a lane =
// a row here (+ its knob array; + an engine param to make it SOUND). The paint + lens
// are already lane-index-generic, so they need no change. Order matches LK_* / DD_TUN+i.
// A lane's `sink` = WHERE its computed value goes at fire-time:
//   SK_ARG = mutate the voice's TUNE/DEC/COLOR array (passed to *_fire) — the p-locks.
//   SK_VEL = fold into the per-hit velocity (`boost` int) — truly per-voice, no engine change.
//   SK_PAN = call tr808_pan/tr909_pan around the fire (voice snapshots slot pan at note-on,
//            so per-fire = per-hit); per-voice, but toms/congas group (shared slot).
enum { SK_ARG, SK_VEL, SK_PAN };
typedef struct { const char *name; float *knob; float *off; int lk; int sink; } Lane;
static int drum_lanes(int m, int v, Lane L[LK_N]) {
    int n = 0;
    if (m == M_808) {
        L[n++] = (Lane){ "TUN", &dtune[v],  doff[LK_TUNE][v], LK_TUNE, SK_ARG };
        L[n++] = (Lane){ "DEC", &ddecay[v], doff[LK_DEC][v],  LK_DEC,  SK_ARG };
        if (CH8[v]) L[n++] = (Lane){ CH8[v], &dcolor[v], doff[LK_CHAR][v], LK_CHAR, SK_ARG };
        L[n++] = (Lane){ "VOL", &dvol[v],   doff[LK_VOL][v],  LK_VOL,  SK_VEL };
        L[n++] = (Lane){ "PAN", &dpan[v],   doff[LK_PAN][v],  LK_PAN,  SK_PAN };
    } else {
        L[n++] = (Lane){ "TUN", &d9tune[v],  d9off[LK_TUNE][v], LK_TUNE, SK_ARG };
        L[n++] = (Lane){ "DEC", &d9decay[v], d9off[LK_DEC][v],  LK_DEC,  SK_ARG };
        if (CH9[v]) L[n++] = (Lane){ CH9[v], &d9color[v], d9off[LK_CHAR][v], LK_CHAR, SK_ARG };
        L[n++] = (Lane){ "VOL", &d9vol[v],   d9off[LK_VOL][v],  LK_VOL,  SK_VEL };
        L[n++] = (Lane){ "PAN", &d9pan[v],   d9off[LK_PAN][v],  LK_PAN,  SK_PAN };
    }
    return n;
}

// the MST master/mix face
static float mglu = 0.30f, mflt = 0.5f, mfres = 0.35f, mfb = 0.35f, mpump = 0.0f;
static int   mdiv = 2;                                      // delay div: 0=1/16 1=1/8 2=dotted 3=1/4
static float msend[4] = { 0.10f, 0.10f, 0.0f, 0.0f };      // per-machine delay send: 303a 303b 808 909
static float fxverb[4] = { 0, 0, 0, 0 };                   // per-machine reverb send: 303a/b → warm hall (tank 0), 808 → tank 2, 909 → tank 1
static float dist8 = 0, dist9 = 0;                          // per-machine drum drive — ADDS on top of the kit's baked kick drive
static float level[M_N] = { 1, 1, 1, 1, 1 };                // per-machine TAB fader (1 = unity/stock); 4 = MST (master wire TODO)
static int   mpcf[STEPS];                                   // pattern-controlled filter: cutoff level 0..7 per step (7 = open)
static int   mcrush[STEPS];                                 // pattern-controlled CRUSH: bitcrush level 0..7 per step (0 = clean; the PCF's texture twin)
static int   mgate[STEPS];                                  // pattern-controlled GATE: openness 0..7 per step (7 = open, down = chop; the rhythm twin)
static int   mstflow = 0;                                   // MST screen: 0 = MIX meters, 1 = PCF lane, 2 = CRUSH lane, 3 = GATE lane

// ── PATTERN BANKS (ARRANGEMENT) ──────────────────────────────────────────────
// PER-MACHINE A/B/C/D slots. A pattern stores only the SEQUENCE (steps + per-step
// flags/lanes + length), NOT the sound (knobs/key/tuning/FX stay global — you sculpt the
// timbre once and switch which pattern plays it). Live edits happen on the live arrays;
// switching SAVES the live arrays into the old slot and LOADS the new one (copy-on-switch,
// so the draw/fire code stays unchanged). curpat[machine] = 0 holds today's default beat.
#define NPAT 4
typedef struct { int on[STEPS], pit[STEPS], acc[STEPS], sld[STEPS], tie[STEPS], oct[STEPS], plen; } P303;
typedef struct { int grid[TR_NV][STEPS], acc[TR_NV][STEPS], prob[TR_NV][STEPS]; float off[LK_N][TR_NV][STEPS]; } P808;
typedef struct { int grid[TR9_NV][STEPS], acc[TR9_NV][STEPS], prob[TR9_NV][STEPS], strk[TR9_NV][STEPS]; float off[LK_N][TR9_NV][STEPS]; } P909;
static P303 pat303[2][NPAT];        // [line][slot]
static P808 pat808[NPAT];
static P909 pat909[NPAT];
static int  curpat[M_N] = { 0, 0, 0, 0, 0 };   // current slot per machine (303a/303b/808/909/-); MST unused
static int  armpat[M_N] = { -1, -1, -1, -1, -1 };  // QUEUED next slot per machine (-1 = none): tapped while PLAYING, lands on the next bar downbeat so the rack switches in time

static void pat_io_303(int i, int s, int save) { P303 *p = &pat303[i][s];   // save!=0 : live→slot ; else slot→live
    for (int k = 0; k < STEPS; k++) {
        int *L[6] = { on[i], pit[i], acc[i], sld[i], tie[i], oct[i] };
        int *P[6] = { p->on, p->pit, p->acc, p->sld, p->tie, p->oct };
        for (int a = 0; a < 6; a++) { if (save) P[a][k] = L[a][k]; else L[a][k] = P[a][k]; }
    }
    if (save) p->plen = plen[i]; else plen[i] = p->plen;
}
static void pat_io_808(int s, int save) { P808 *p = &pat808[s];
    for (int v = 0; v < TR_NV; v++) for (int k = 0; k < STEPS; k++)
        if (save) { p->grid[v][k] = dgrid[v][k]; p->acc[v][k] = dacc[v][k]; p->prob[v][k] = dprob[v][k]; }
        else      { dgrid[v][k] = p->grid[v][k]; dacc[v][k] = p->acc[v][k]; dprob[v][k] = p->prob[v][k]; }
    for (int a = 0; a < LK_N; a++) for (int v = 0; v < TR_NV; v++) for (int k = 0; k < STEPS; k++)
        { if (save) p->off[a][v][k] = doff[a][v][k]; else doff[a][v][k] = p->off[a][v][k]; }
}
static void pat_io_909(int s, int save) { P909 *p = &pat909[s];
    for (int v = 0; v < TR9_NV; v++) for (int k = 0; k < STEPS; k++)
        if (save) { p->grid[v][k] = d9grid[v][k]; p->acc[v][k] = d9acc[v][k]; p->prob[v][k] = d9prob[v][k]; p->strk[v][k] = d9strk[v][k]; }
        else      { d9grid[v][k] = p->grid[v][k]; d9acc[v][k] = p->acc[v][k]; d9prob[v][k] = p->prob[v][k]; d9strk[v][k] = p->strk[v][k]; }
    for (int a = 0; a < LK_N; a++) for (int v = 0; v < TR9_NV; v++) for (int k = 0; k < STEPS; k++)
        { if (save) p->off[a][v][k] = d9off[a][v][k]; else d9off[a][v][k] = p->off[a][v][k]; }
}
// switch machine m to slot `s`: save the live sequence into the old slot, load the new.
static void pat_switch(int m, int s) {
    if (s == curpat[m]) return;
    if (m == M_303A || m == M_303B) { int i = m; pat_io_303(i, curpat[m], 1); curpat[m] = s; pat_io_303(i, s, 0); }
    else if (m == M_808) { pat_io_808(curpat[m], 1); curpat[m] = s; pat_io_808(s, 0); }
    else if (m == M_909) { pat_io_909(curpat[m], 1); curpat[m] = s; pat_io_909(s, 0); }
}

// transport (shared across the rack)
static int   playing = 1, step = 0, laststep = -1, laststep303[2] = { -1, -1 };   // laststep303[i] = each 303 line's OWN (swung, PERF-speed-lensed) step trigger
static float mbop = 0;
static float g_bpm = 132;                                   // master TEMPO (rack-scope, MST face); drives the step clock + the tempo-synced delay
static float bpm01 = 0.5143f;                               // MST TEMPO-knob proxy (0..1 → g_bpm 60..200; 0.5143 → 132, matches g_bpm's init)
static float g_phase = 0, g_last_t = 0;                     // accumulated 16th-note phase, so a live bpm change changes the RATE, never JUMPS the counter
static float g_swing = 0;                                   // master SWING (rack-scope, MST face) — one global shuffle for ALL machines (ReBirth's model); 0 = straight, delays the odd 16ths by up to 0.6 of a step

// PERF lenses — NON-DESTRUCTIVE performance transforms over the 303 read path (per line).
// The pattern arrays are untouched; the transport just READS them through whichever lens is
// active, so clearing it = instantly back to normal. Each lens is TAP=latch (persists, even
// across faces — 303a keeps doubling while you work 303b) OR HOLD=momentary (on while held).
// pf_* = the EFFECTIVE per-frame state read by the step clock; recomputed in draw() from the
// persistent latch + the focused line's held buttons.
enum { PL_HALF, PL_ACC, PL_OCT, PL_STAC, PL_GLIDE, PL_REV, PL_ROLL, PL_N };   // lens index (for pf_latch/pf_hold). Speed = just HALF (2X/8-12 dropped); accent cross-rhythms (AC3/AC4) dropped — didn't add much
#define PERF_TAP 12                  // release within this many frames (~200ms) = a TAP (toggle latch); longer = a momentary HOLD
#define ROLL_RATE 2.0f               // 303 ROLL retrigger subdivisions per 16th (2 = 32nd-note roll; 4 = 64th buzz, 1.5 = 16th-triplet). Tune the fill's speed by ear here.
static int pf_latch[PL_N][2];        // persistent per-lens-per-line LATCH (tap-toggled; holds across faces + stop/start)
static int pf_hold[PL_N][2];         // frames each button has been held (for the tap-vs-hold split)
static int pf_half[2];               // effective speed lens: half-time (the one that survived — clean 2:1 → phase-locked to the drums)
static int pf_acc[2];                // effective total-accent: every fired note gets the accent (the 909 "accent everything" move)
static int pf_oct[2];                // effective OCTAVE-JUMP lens: every OTHER step (odd counter) drops an octave — the classic acid octave bounce, live-only (pattern untouched)
static int pf_stac[2], pf_glide[2];  // effective slide lens: STAC forces all slides OFF (plucky), GLIDE forces them ALL on (a smooth river)
static int pf_rev[2];                // effective REVERSE lens: read the pattern backwards (step index mirrored) — an instant melodic flip / turnaround
static int pf_roll[2];               // effective ROLL lens: while held, retrigger the last played note at ROLL_RATE (a stutter/fill — momentary by nature)
static int rollctr[2] = { -1, -1 };  // ROLL sub-step counter (reset to -1 when not rolling → clean re-engage)
static int roll_pit[2] = { 24, 24 }, roll_acc[2];   // the last played note (midi + accent) ROLL repeats

// Drum PERF (per machine: 0 = 808, 1 = 909) — live variation that all flows through the STEP GRID, so
// it's grid-locked by construction. BEAT-REPEAT (RP1/RP2/RP4) loops the last 1/2/4-step slice; DENSITY
// (THIN drops the kit to downbeats+accents, BUSY fills the SELECTED voice's off-16ths); ACC accents every hit. Same
// TAP=latch / HOLD=momentary model as the 303 PERF; seeded from the latch in draw() so it holds.
enum { DPL_RP1, DPL_RP2, DPL_RP4, DPL_THIN, DPL_BUSY, DPL_ACC, DPL_N };
static int dpf_latch[DPL_N][2], dpf_hold[DPL_N][2];   // per-lens-per-machine latch + hold frames
static int pf_rp1[2], pf_rp2[2], pf_rp4[2];           // effective beat-repeat holds (1/2/4-step slice), per machine
static int pf_thin[2], pf_busy[2], pf_dacc[2];        // effective DENSITY (thin/busy) + accent-all, per machine
static int rpt_base[2], rpt_n[2], rpt_was[2];         // beat-repeat slice capture (frozen N-aligned start)

// beat-repeat: map the real transport `step` to a FROZEN, N-aligned slice that loops (grid-locked).
// m = 0 (808) / 1 (909). Returns `step` unchanged when no RP button is held for that machine.
static int drum_effstep(int m, int step) {
    int n = pf_rp1[m] ? 1 : pf_rp2[m] ? 2 : pf_rp4[m] ? 4 : 0;   // priority if several held
    if (!n) { rpt_was[m] = 0; return step; }
    if (!rpt_was[m] || rpt_n[m] != n) { rpt_base[m] = step - (step % n); rpt_n[m] = n; rpt_was[m] = 1; }  // (re)capture the slice
    return rpt_base[m] + (((step - rpt_base[m]) % n) + n) % n;   // loop [base, base+n); n divides 16 → always in range
}

// LIVE bank switching: a bank tap is QUANTIZED to the bar when the transport runs —
// it ARMS the slot (blinks) and the switch LANDS on the next bar downbeat (update()),
// so the whole rack changes pattern in time. Stopped → switch instantly (no bar to wait
// for). Re-tapping the current or the armed slot cancels a pending arm. (pat_pad, the
// blinking pad widget, lives just below lcdbtn — it needs that declared first.)
static void pat_queue(int m, int s) {
    if (!playing)          { pat_switch(m, s); armpat[m] = -1; }   // stopped → instant
    else if (s == curpat[m] || s == armpat[m]) armpat[m] = -1;     // re-tap → cancel the queue
    else                   armpat[m] = s;                          // playing → arm for the next bar
}

// re-apply the master effects, set-and-hold — reconfigure ONLY on change (the
// fx-frame rule); the live FLT is ride-safe so it runs every frame. Voicings
// copied from acidrack's apply_fx so the master matches the mature rack.
static void apply_fx(void) {
    static float aS[4] = { -1, -1, -1, -1 }, aComp = -1, aBpm = -1, aEf = -1;
    static int   aMode = -2, aEt = -1;
    {   // the shared delay unit: tempo-synced DIVISION (buttons) + the FB knob. Set-and-hold —
        // echo() re-applies only when the computed time/fb CHANGE (echo() is not ride-safe).
        static const float DIV[4] = { 0.25f, 0.5f, 0.75f, 1.0f };   // 1/16 · 1/8 · dotted · 1/4
        int   et = (int)(60000.0f / g_bpm * DIV[mdiv]);
        float ef = mfb * 0.72f;
        if (et != aEt || ef != aEf) { echo(et, ef, 0.45f); aEt = et; aEf = ef; }
        if (g_bpm != aBpm) { bpm((int)g_bpm); aBpm = g_bpm; }   // keep the engine musical clock synced on a tempo change
    }
    if (msend[0] != aS[0]) { instrument_echo(6, msend[0] * 0.6f); aS[0] = msend[0]; }   // 303a = slot 6
    if (msend[1] != aS[1]) { instrument_echo(7, msend[1] * 0.6f); aS[1] = msend[1]; }   // 303b = slot 7
    if (msend[2] != aS[2]) { for (int i = 0; i < TR808_NSLOT; i++) instrument_echo(TR808_BASE + i, msend[2] * 0.6f); aS[2] = msend[2]; }
    if (msend[3] != aS[3]) { for (int i = 0; i < TR909_NSLOT; i++) instrument_echo(D909_BASE + i, msend[3] * 0.6f); aS[3] = msend[3]; }
    // master FILTER — the live DJ filter (FLT, bipolar) composed with the drawn PCF
    // lane (a lowpass automated per step). A live HP takes over; otherwise the MORE
    // CLOSED lowpass (hand vs drawn) wins. filter() is ride-safe (every frame).
    float res = 0.15f + 0.75f * mfres;
    if (mflt > 0.52f) {
        filter(FILTER_HIGH, 20.0f * powf(6000.0f / 20.0f, (mflt - 0.52f) / 0.48f), res);
    } else {
        float cut = 1e9f;                                   // 1e9 = "open" (no lowpass)
        if (mflt < 0.48f) cut = 18000.0f * powf(150.0f / 18000.0f, (0.48f - mflt) / 0.48f);   // FLT lowpass
        int pcf_active = 0;
        for (int s = 0; s < STEPS; s++) if (mpcf[s] < 7) { pcf_active = 1; break; }
        if (pcf_active) { float pc = 250.0f * powf(2.0f, mpcf[step] / 7.0f * 5.2f); if (pc < cut) cut = pc; }
        if (cut < 1e9f) filter(FILTER_LOW, cut, res);
        else            filter(FILTER_OFF, 1000.0f, 0.0f);
    }
    // master CRUSH lane — the drawn per-step bitcrush (PCF's TEXTURE twin: filter darkens tone,
    // crush degrades bit-depth). crush() is NOT ride-safe → reconfigure ONLY when the step's
    // drawn level changes (set-and-hold). 0 = clean bypass; up = fewer bits + rate reduction + wetter.
    static int aCrush = -1;
    if (mcrush[step] != aCrush) {
        float c = mcrush[step] / 7.0f;
        crush(16.0f - c * 13.0f, 1.0f + c * 24.0f, c);
        aCrush = mcrush[step];
    }
    // master GATE lane — the drawn per-step chop (PCF's RHYTHM twin). 7 = open (threshold 0 =
    // bypass); drawing DOWN raises the gate threshold → the step cuts (snappy release = a stutter).
    // set-and-hold on step change (gate() is not ride-safe).
    static int aGate = -1;
    if (mgate[step] != aGate) {
        float thr = (1.0f - mgate[step] / 7.0f) * 0.85f;
        gate(thr, 2, 45);
        aGate = mgate[step];
    }
    // GLU / PUMP share the master gain stage → mutually exclusive (PUMP wins)
    int   pumping = mpump > 0.02f, mode = pumping ? 1 : 0;
    float arg = pumping ? mpump : mglu;
    if (mode != aMode || arg != aComp) {
        if (pumping) sidechain(0, 0, mpump, 1, 140);
        else         glue(0, mglu < 0.02f ? 0.0f : mglu * 0.55f, 8, 150);
        aMode = mode; aComp = arg;
    }
    // per-machine REVERB sends — 303s into the warm hall (tank 0, sub stays dry),
    // drums into their own plates with the KICK hard-excluded (else it muds the floor).
    static float aRv[4] = { -1, -1, -1, -1 };
    if (fxverb[0] != aRv[0]) { instrument_reverb(6, fxverb[0]); aRv[0] = fxverb[0]; }
    if (fxverb[1] != aRv[1]) { instrument_reverb(7, fxverb[1]); aRv[1] = fxverb[1]; }
    if (fxverb[2] != aRv[2]) { for (int i = 0; i < TR808_NSLOT; i++) instrument_reverb_bus(TR808_BASE + i, 2, i == TRS_BD ? 0.0f : fxverb[2]); aRv[2] = fxverb[2]; }
    if (fxverb[3] != aRv[3]) { for (int i = 0; i < TR909_NSLOT; i++) instrument_reverb_bus(D909_BASE + i, 1, (i == TR9S_BD || i == TR9S_BDC) ? 0.0f : fxverb[3]); aRv[3] = fxverb[3]; }
    // per-machine drum DISTORTION — ADD on top of the baked kick drive (0 = the stock kit)
    static float aD8 = -1, aD9 = -1;
    if (dist8 != aD8) { for (int i = 0; i < TR808_NSLOT; i++) { float b = (i == TRS_BD) ? 0.28f : 0.0f; instrument_drive(TR808_BASE + i, b + dist8 * (0.85f - b)); } aD8 = dist8; }
    if (dist9 != aD9) { for (int i = 0; i < TR909_NSLOT; i++) { float b = (i == TR9S_BD) ? 0.35f : 0.0f; instrument_drive(D909_BASE + i, b + dist9 * (0.85f - b)); } aD9 = dist9; }
    // per-machine LEVEL — the tab fader (instrument_level, 1 = unity/stock). 303 = its
    // voice slot + octave-down sub; drums = every kit slot. MST (level[4]) waits on a master vol.
    static float aLv[4] = { -1, -1, -1, -1 };
    if (level[0] != aLv[0]) { instrument_level(6, level[0]); instrument_level(36, level[0]); aLv[0] = level[0]; }
    if (level[1] != aLv[1]) { instrument_level(7, level[1]); instrument_level(37, level[1]); aLv[1] = level[1]; }
    if (level[2] != aLv[2]) { for (int i = 0; i < TR808_NSLOT; i++) instrument_level(TR808_BASE + i, level[2]); aLv[2] = level[2]; }
    if (level[3] != aLv[3]) { for (int i = 0; i < TR909_NSLOT; i++) instrument_level(D909_BASE + i, level[3]); aLv[3] = level[3]; }
}

// generate a 303 line at density d: 0=clear · 1=minimal · 2=classic · 3=busy.
// NOT random noise — an ACID riff: a short scale-degree MOTIF repeated across the bar (the last
// repeat climbs an octave = the classic acid climb), anchored on the ROOT downbeat, with SLIDES
// gliding between neighbouring notes and ACCENTS driving the strong steps. Notes come from the
// line's own KEY/SCALE (mscale), so GEN respects the key you set.
static void gen_line(int i, int d) {
    plen[i] = STEPS;
    for (int s = 0; s < STEPS; s++) { on[i][s] = acc[i][s] = sld[i][s] = oct[i][s] = tie[i][s] = pit[i][s] = 0; }
    if (d == 0) return;                                                   // CLEAR = empty

    const KeyScale *sc = &SCALES[mscale[i]];
    int nd   = sc->n;                                                     // scale degrees (index nd-1 = the octave)
    int gate = d == 1 ? 55 : d == 3 ? 95 : 78;                            // chance a step SOUNDS (else a rest — the line breathes)
    int accp = d == 1 ? 16 : d == 3 ? 34 : 24;
    int sldp = d == 1 ? 28 : d == 3 ? 55 : 42;                            // acid = plenty of glide
    int octp = d == 1 ? 6  : d == 3 ? 20 : 12;
    int ML   = d == 1 ? 8  : 4;                                           // motif length; repeats = STEPS/ML

    // MOTIF — a small random walk over scale degrees (small steps + the odd snap home), rooted.
    int cellpit[8], deg = 0;
    cellpit[0] = sc->deg[0];                                              // the cell opens on the root
    for (int c = 1; c < ML; c++) {
        int r = rnd_between(0, 99);
        if      (r < 34) deg += 1;
        else if (r < 60) deg -= 1;
        else if (r < 72) deg += 2;
        else if (r < 82) deg -= 2;
        else             deg  = 0;                                        // pull back to the root
        if (deg < 0) deg = 0; if (deg > nd - 1) deg = nd - 1;             // stay within one octave of degrees
        cellpit[c] = sc->deg[deg];
    }

    // lay the motif across the bar; the LAST repeat climbs an octave (call → response)
    int last = STEPS / ML - 1;
    for (int s = 0; s < STEPS; s++) {
        int c = s % ML, rep = s / ML, strong = (s % 4 == 0);
        on[i][s]  = rnd_between(0, 99) < gate;
        pit[i][s] = cellpit[c];
        acc[i][s] = rnd_between(0, 99) < (strong ? accp + 22 : accp);     // strong steps accented more
        oct[i][s] = rnd_between(0, 99) < (rep == last ? octp + 18 : octp);// the acid octave climb on the final repeat
        if (!on[i][s]) tie[i][s] = rnd_between(0, 99) < 18;               // a held tail through some rests
    }
    on[i][0] = 1; acc[i][0] = 1; pit[i][0] = sc->deg[0]; oct[i][0] = 0;   // strong ROOT downbeat, anchored

    // SLIDES — glide between consecutive sounding notes (the liquid 303 legato)
    for (int s = 0; s < STEPS; s++) {
        int ns = (s + 1) % STEPS;
        if (on[i][s] && (on[i][ns] || tie[i][ns]) && rnd_between(0, 99) < sldp) sld[i][s] = 1;
    }
}

// generate the 808 beat at density d: 0=clear · 1=minimal · 2=classic · 3=busy
static void gen_drums(int d) {
    for (int v = 0; v < TR_NV; v++) for (int s = 0; s < STEPS; s++) { dgrid[v][s] = 0; dacc[v][s] = 0; dprob[v][s] = 100; for (int p = 0; p < LK_N; p++) doff[p][v][s] = 0; }
    if (d == 0) return;                                            // CLEAR = empty
    for (int s = 0; s < STEPS; s += 4) dgrid[TR_BD][s] = 1;        // kick four-on-floor (the pulse, always)
    dgrid[TR_OH][2] = dgrid[TR_OH][10] = 1;                        // off-beat open hat
    if (d == 1) { dgrid[TR_CH][6] = dgrid[TR_CH][14] = 1; return; }// MINIMAL: kick + off-hats + a touch of closed
    dgrid[TR_SD][4] = dgrid[TR_SD][12] = 1;                        // snare backbeat (classic + busy)
    for (int s = 0; s < STEPS; s += (d == 3 ? 1 : 2)) dgrid[TR_CH][s] = 1;   // 8th (classic) or 16th (busy) closed hats
    int cp = d == 3 ? 25 : 12, cb = d == 3 ? 18 : 10;
    for (int s = 0; s < STEPS; s++) {                             // spice — denser when BUSY
        if (rnd_between(0, 99) < cp) dgrid[TR_CP][s] = 1;
        if (rnd_between(0, 99) < cb) dgrid[TR_CB][s] = 1;
        if (d == 3 && rnd_between(0, 99) < 12) dgrid[TR_LT][s] = 1;                    // busy: tom fills
        if (d == 3 && dgrid[TR_BD][s] && rnd_between(0, 99) < 30) dacc[TR_BD][s] = 1;  // busy: accented kicks
    }
}

// generate the 909 beat at density d: 0=clear · 1=minimal · 2=classic · 3=busy
static void gen_drums9(int d) {
    for (int v = 0; v < TR9_NV; v++) for (int s = 0; s < STEPS; s++) { d9grid[v][s] = 0; d9acc[v][s] = 0; d9prob[v][s] = 100; d9strk[v][s] = 0; for (int p = 0; p < LK_N; p++) d9off[p][v][s] = 0; }
    if (d == 0) return;                                            // CLEAR = empty
    for (int s = 0; s < STEPS; s += 4) d9grid[TR9_BD][s] = 1;      // house four-on-floor
    d9grid[TR9_OH][2] = d9grid[TR9_OH][10] = 1;                    // off-beat open hats
    if (d == 1) { d9grid[TR9_CH][6] = d9grid[TR9_CH][14] = 1; return; }  // MINIMAL
    d9grid[TR9_CP][4] = d9grid[TR9_CP][12] = 1;                    // clap backbeat
    for (int s = 0; s < STEPS; s += (d == 3 ? 1 : 2)) d9grid[TR9_CH][s] = 1;  // closed hats
    int rc = d == 3 ? 18 : 8;
    for (int s = 0; s < STEPS; s++) {
        if (rnd_between(0, 99) < rc) d9grid[TR9_RC][s] = 1;                            // ride spice
        if (d == 3 && rnd_between(0, 99) < 12) d9grid[TR9_LT][s] = 1;                  // busy: tom fills
        if (d == 3 && d9grid[TR9_BD][s] && rnd_between(0, 99) < 30) d9acc[TR9_BD][s] = 1;
    }
}

// snap a 0..1 vertical drag to the four RD-8-style trig-prob buckets (grafted from tr808.c)
static int snap_prob(float f) { return f >= 0.875f ? 100 : f >= 0.625f ? 75 : f >= 0.375f ? 50 : 25; }

// ── WHOLE-RACK genre presets (MST GEN) ───────────────────────────────────────
// One tap fills the entire rack — both 303s (via the acid-riff gen_line at a genre density),
// a drum kit programmed for the style, plus tempo / swing / key. A starting song you then tweak.
// The 303s always play; the drums use ONE machine per genre (the other is cleared for a clean start).
enum { SNG_HOUSE, SNG_ACID, SNG_ELECTRO, SNG_GLITCH, SNG_TECHNO, SNG_TEKNO, SNG_N };
static const char *SNG_NAME[SNG_N] = { "PARTY", "ALOT", "YO", "SING", "PUMP", "TOMS" };   // cute labels: PARTY=house · ALOT=acid · YO=electro · SING=glitch · PUMP=techno · TOMS=tekno
static void gen_song(int g) {
    float bpm; float sw; int scale;                        // transport + key per genre
    switch (g) {
        case SNG_HOUSE:   bpm = 124; sw = 0.18f; scale = 2; break;   // MINOR
        case SNG_ACID:    bpm = 130; sw = 0.00f; scale = 0; break;   // m.PENT
        case SNG_ELECTRO: bpm = 128; sw = 0.00f; scale = 2; break;
        case SNG_GLITCH:  bpm = 142; sw = 0.00f; scale = 2; break;
        case SNG_TEKNO:   bpm = 150; sw = 0.00f; scale = 2; break;   // tribe tekno — fast + dark
        default:          bpm = 132; sw = 0.10f; scale = 2; break;   // TECHNO
    }
    bpm01 = clamp((bpm - 60) / 140.0f, 0, 1); g_bpm = bpm; g_swing = sw;
    mroot[0] = mroot[1] = 0; mscale[0] = mscale[1] = scale; loct[0] = 0; loct[1] = 0;

    gen_drums(0); gen_drums9(0);                           // clear both kits, then program the genre's
    if (g == SNG_HOUSE || g == SNG_ACID || g == SNG_TECHNO) {          // 909 four-on-floor
        for (int s = 0; s < STEPS; s += 4) d9grid[TR9_BD][s] = 1;
        d9grid[TR9_CP][4] = d9grid[TR9_CP][12] = 1;                    // clap backbeat
        for (int s = 2; s < STEPS; s += 4) d9grid[TR9_OH][s] = 1;      // off-beat open hats
        int hs = (g == SNG_TECHNO) ? 1 : 2;                            // techno = 16th hats, else 8ths
        for (int s = 0; s < STEPS; s += hs) d9grid[TR9_CH][s] = 1;
        if (g == SNG_ACID) for (int s = 0; s < STEPS; s++) if (rnd_between(0, 99) < 18) d9grid[TR9_RC][s] = 1;   // ride sparkle
    } else if (g == SNG_ELECTRO) {                                     // 808 broken/syncopated
        dgrid[TR_BD][0] = dgrid[TR_BD][3] = dgrid[TR_BD][6] = dgrid[TR_BD][10] = 1;
        dgrid[TR_SD][4] = dgrid[TR_SD][12] = 1; dgrid[TR_CP][4] = dgrid[TR_CP][12] = 1;
        for (int s = 0; s < STEPS; s++) dgrid[TR_CH][s] = 1;           // 16th hats
        dgrid[TR_OH][2] = dgrid[TR_OH][10] = 1;
    } else if (g == SNG_TEKNO) {                                      // TRIBE TEKNO — hard four-on-floor + rolling tribal toms/congas, dark
        for (int s = 0; s < STEPS; s += 4) dgrid[TR_BD][s] = 1;
        dacc[TR_BD][0] = dacc[TR_BD][8] = 1;                          // accent the 1 & the 3
        static const int tribe[6] = { TR_LC, TR_MC, TR_HC, TR_LT, TR_MT, TR_HT };   // congas + toms
        for (int s = 0; s < STEPS; s++) if (s % 4 != 0 && rnd_between(0, 99) < 55) dgrid[tribe[rnd_between(0, 5)]][s] = 1;   // rolling percussion OFF the kick
        for (int s = 2; s < STEPS; s += 4) dgrid[TR_MA][s] = 1;       // maraca shaker on the off-beats
    } else {                                                          // GLITCH — sparse, broken, probabilistic
        dgrid[TR_BD][0] = dgrid[TR_BD][7] = dgrid[TR_BD][11] = 1; dacc[TR_BD][0] = 1;
        dgrid[TR_SD][4] = dgrid[TR_SD][12] = 1;
        for (int s = 0; s < STEPS; s++) if (rnd_between(0, 99) < 55) { dgrid[TR_CH][s] = 1; dprob[TR_CH][s] = snap_prob(rnd_between(0, 99) / 100.0f); }
        for (int s = 0; s < STEPS; s++) if (rnd_between(0, 99) < 15) dgrid[TR_CP][s] = 1;
    }

    switch (g) {                                           // the 303 lines (mscale is set → gen_line stays in key)
        case SNG_HOUSE:   gen_line(0, 2); gen_line(1, 1); loct[1] = 1; break;   // rolling bass + a sparse lead up an octave
        case SNG_ACID:    gen_line(0, 3); gen_line(1, 2); loct[1] = 1; break;   // busy acid bass + lead
        case SNG_ELECTRO: gen_line(0, 2); gen_line(1, 0); break;                // bass only
        case SNG_GLITCH:  gen_line(0, 3); gen_line(1, 3); break;                // both busy/chaotic
        case SNG_TEKNO:   gen_line(0, 1); gen_line(1, 0); loct[0] = -1; break;  // minimal DARK bass an octave down, no lead
        default:          gen_line(0, 1); gen_line(1, 0); break;                // TECHNO — a minimal hypnotic stab
    }
}

// ── candy widgets ──────────────────────────────────────────────────────────
static void plabel(const char *s, int cx, int y, int col) { print(s, cx - text_width(s) / 2, y, col); }

// TAP-SETTLE: after a drag widget (knob / bar / lane) lifts, a mouse can deliver a
// spurious second button-down a few frames later at the drifted position — a "bounce"
// that lands as a stray tap (muted a cartridge in a playtest). Ignore nav taps for a
// short window after any drag was held. (acidrack's fix, ported.)
#define TAP_SETTLE 12   // ~200ms; the observed bounce lagged the release by 8 frames
static int g_drag_frame = -100;             // last frame a drag widget was captured (ui_frame_ct clock)
static int g_drag_y = 100;                   // ...and WHERE it was (canvas y) — a bounce lands near here,
                                             // so a nav tap only blocks when THIS was near the tab band (drag_bounce)
static int tap_settled(void) { return ui_frame_ct - g_drag_frame >= TAP_SETTLE; }

// A bounce lands near the last drag-release Y. So a momentary button should
// swallow its activation only while the settle window is open AND the drag
// ended near THIS button's rect — otherwise a knob-drag (y~22) would wrongly
// block a soft-key (y~40) two zones away. drag_bounce() is that per-rect test;
// the DF/WAVE row-1 buttons (same row as the knobs) are the ones it actually
// saves. The NAV TABS use it too (against the y=0..10 band): the top knob row
// sits at y~16..28, close enough that a knob-release bounce can slip into a
// tab's hit-padded region (the old crude `g_drag_y >= 11` guard let it through).
#define BOUNCE_MARGIN 6
static int drag_bounce(int y, int h) {
    if (tap_settled()) return 0;
    return g_drag_y >= y - BOUNCE_MARGIN && g_drag_y <= y + h + BOUNCE_MARGIN;
}

// nav_clean() — the guard for the tab-band buttons (PLAY / focus / mute). A knob
// release bounce isn't always a quick tap: a trackpad can re-report the finger as a
// FRESH contact that GRABS a tab and then DWELLS many frames before lifting — so by
// the time it activates, tap_settled() has already closed again and drag_bounce()
// wrongly says "fine" (this is why the y-threshold AND the plain drag_bounce guard
// both leaked). Fix: judge the bounce at GRAB, not at release. When a nav widget
// grabs a contact inside the settle window near the tab band, POISON that contact;
// honour the poison whenever it activates, however long it held. Call once per nav
// widget EVERY frame (not short-circuited behind `activated`) so the grab is seen.
//   CASCADE: the phantoms come in a CHAIN — bounce → bounce → bounce, each ~8-15
//   frames apart. A window anchored only to the KNOB release lets the 2nd/3rd one
//   through (it's now >TAP_SETTLE past the knob). So while a poisoned bounce is held,
//   RE-ARM the settle clock: the quiet window restarts on every bounce and only
//   reopens once the finger truly settles (TAP_SETTLE frames with no new bounce).
static void *nav_poison[6]; static int nav_poison_n;
static int nav_clean(void *wid) {
    int poisoned = 0;
    for (int i = 0; i < nav_poison_n; i++) if (nav_poison[i] == wid) { poisoned = 1; break; }
    if (ui_grabbed(wid) && !poisoned && drag_bounce(0, 10))    // grabbed mid-settle, near the tabs → a bounce
        { if (nav_poison_n < 6) nav_poison[nav_poison_n++] = wid; poisoned = 1; }
    if (poisoned && ui_cap_for(wid)) g_drag_frame = ui_frame_ct;   // keep the clock armed while held → covers the CHAIN
    if (ui_released(wid))                                      // contact lifted → was it poisoned?
        for (int i = 0; i < nav_poison_n; i++) if (nav_poison[i] == wid)
            { nav_poison[i] = nav_poison[--nav_poison_n]; return 0; }
    return 1;
}

// per-knob interaction memory (for double-tap-to-reset), keyed by the value pointer
static struct { void *v; float gval, gt, ltt; } kmeta[8];
static int kmeta_i(void *v) {
    for (int i = 0; i < 8; i++) if (kmeta[i].v == v) return i;
    for (int i = 0; i < 8; i++) if (!kmeta[i].v) { kmeta[i].v = v; kmeta[i].ltt = -9; return i; }
    return 0;
}

// candy rotary. VERTICAL drag = value; PULL SIDEWAYS to shift into a finer gear
// (further out = finer) — one gesture gives quick AND precise. DOUBLE-TAP = reset
// to `def`. While turning, the label shows the live value + a bright value band
// rides the rim; the pointer goes amber in fine gear. Wheel = fine (desktop).
static void _knobx(float *v, int cx, int cy, int r, const char *label, float def, int lcd) {
    ui_reg(v, cx - r, cy - r, 2 * r + 1, 2 * r + 1, 0);
    UiCap *c = ui_cap_for(v);
    if (c) { g_drag_frame = ui_frame_ct; g_drag_y = c->cy; }   // a knob is being dragged → arm the tap-settle
    int mi = kmeta_i(v), fine = 0, held = c != 0;
    if (ui_grabbed(v)) { kmeta[mi].gval = *v; kmeta[mi].gt = now(); }
    if (c) {
        if (!c->has_v0) { c->has_v0 = 1; c->v0 = *v; c->by = c->cy; }   // (re)snap on grab / lens-cross
        int py = c->released ? c->ry : c->cy;
        int px = c->released ? c->rx : c->cx;
        int ox = px - cx; if (ox < 0) ox = -ox;                         // horizontal offset from the knob
        float gear = 1.0f + ox / KNOB_GEAR;                            // 1x over the knob → finer as you pull out
        if (gear > KNOB_GEARMAX) gear = KNOB_GEARMAX;                  // capped so FINE still covers ground
        fine = gear > 1.5f;
        float sweep = KNOB_SWEEP * gear;
        *v = clamp(c->v0 + (c->by - py) / sweep, 0, 1);
        c->v0 = *v; c->by = py;                                        // re-anchor each frame → a gear change never JUMPS the value
    }
    if (ui_released(v)) {                                              // a tap (barely moved, quick) can reset
        float rt = now(), dv = *v - kmeta[mi].gval; if (dv < 0) dv = -dv;
        if (dv < 0.02f && rt - kmeta[mi].gt < 0.25f) {
            if (rt - kmeta[mi].ltt < 0.35f) { *v = def; kmeta[mi].ltt = -9; }   // double-tap → default
            else kmeta[mi].ltt = rt;
        }
    }
    int hot = held || ui_hover(cx - r, cy - r, 2 * r + 1, 2 * r + 1);
    if (!c && hot && mouse_wheel() != 0) *v = clamp(*v + mouse_wheel() * 0.04f, 0, 1);
    float ang = 150 + *v * 240;
    font(FONT_TINY);
    if (lcd) {                                                        // LCD-native: pure green outline, drawn ON the glass
        circ(cx, cy, r, (held || hot) ? CLR_LIME_GREEN : CLR_MEDIUM_GREEN);
        ring(cx, cy, r - 3, r - 1, 150, ang, CLR_LIME_GREEN);         // value arc (always shown, not just while held)
        line(cx + (int)dx(1, ang), cy + (int)dy(1, ang), cx + (int)dx(r - 1, ang), cy + (int)dy(r - 1, ang), CLR_LIME_GREEN);
        if (held) { int p = (int)(*v * 99 + 0.5f); char b[3] = { (char)('0' + p / 10), (char)('0' + p % 10), 0 };
                    plabel(b, cx, cy + r + 1, CLR_LIME_GREEN); }
        else plabel(label, cx, cy + r + 1, CLR_MEDIUM_GREEN);
        return;
    }
    if (held) { blend(BLEND_AVG); circfill(cx, cy, r + 1, CLR_WHITE); blend_reset(); }   // grab-glow halo
    circfill(cx, cy, r, CLR_INDIGO);
    int rim = r / 4; if (rim < 1) rim = 1;   // bevel thickness scales with r → no thin/gappy arc when the knob is big
    ring(cx, cy, r - 1 - rim, r - 1, 150, 300, CLR_PINK);          // highlight (lower-left), gaps narrowed 60°→30°
    ring(cx, cy, r - 1 - rim, r - 1, -30, 120, CLR_DARKER_PURPLE); // shadow (lower-right)
    if (held) ring(cx, cy, r - 3, r, 150, ang, CLR_LIGHT_YELLOW);      // FAT value band — fills as you turn
    circ(cx, cy, r, held ? CLR_WHITE : hot ? CLR_LIGHT_PEACH : CLR_BROWNISH_BLACK);
    line(cx + (int)dx(1, ang), cy + (int)dy(1, ang), cx + (int)dx(r - 1, ang), cy + (int)dy(r - 1, ang),
         fine ? CLR_ORANGE : CLR_WHITE);                              // pointer goes amber in fine gear
    if (held) { int p = (int)(*v * 99 + 0.5f); char b[3] = { (char)('0' + p / 10), (char)('0' + p % 10), 0 };
                plabel(b, cx, cy + r + 1, CLR_DARK_GREEN); }
    else plabel(label, cx, cy + r + 1, CLR_DARK_BROWN);
}
static void knob(float *v, int cx, int cy, int r, const char *label, float def)    { _knobx(v, cx, cy, r, label, def, 0); }
static void lcdknob(float *v, int cx, int cy, int r, const char *label, float def) { _knobx(v, cx, cy, r, label, def, 1); }

// ── responsive placement (device-adaptive) ───────────────────────────────────
// FU = a comfortable finger, from the engine (44pt in logical px; 22 at iOS 2×). Size
// finger controls to this so they stay tappable as the canvas grows — the CONTAINERS
// (lay.h) absorb the extra space; we never scale the render (that desyncs ui.h).
#define FU ((float)finger_px())
// centre a knob in a cell with its label below it; radius from the cell, not a raw px guess.
// Knob radius keyed to the cell HEIGHT (a consistent fraction of the row), not width — so it keeps the
// SAME breathing room above/below at every size (the band height is ~constant on the chunky canvas, so
// a width-scaled knob would swell to fill the row and eat the margin). ≈ r6 at 160×100; it grows
// PHYSICALLY via the blit on bigger screens. Width only caps it so it can't overflow a narrow cell.
static void knob_cell(Box c, float *v, const char *lab, float def) {
    float fh = rack_view ? 0.40f : 0.30f, fw = rack_view ? 0.50f : 0.42f;   // iPad: knobs grow a little (roomier panels)
    int  rmax = rack_view ? 15 : 12;
    float rh = c.h * fh, rw = c.w * fw;
    int r = (int)lay_clamp(rh < rw ? rh : rw, 5, rmax);
    int cy = (int)(c.y + r + 1); if (cy + r + 7 > (int)(c.y + c.h)) cy = (int)(c.y + c.h) - r - 7;
    knob(v, (int)(c.x + c.w / 2), cy, r, lab, def);
}
static void lcdknob_cell(Box c, float *v, const char *lab, float def) {
    float rh = c.h * 0.30f, rw = c.w * 0.42f;
    int r = (int)lay_clamp(rh < rw ? rh : rw, 4, 12);
    int cy = (int)(c.y + r + 1); if (cy + r + 7 > (int)(c.y + c.h)) cy = (int)(c.y + c.h) - r - 7;
    lcdknob(v, (int)(c.x + c.w / 2), cy, r, lab, def);
}

// gknob — a compact knob that ALWAYS shows a caller-formatted VALUE label (a delay division,
// a BPM number), unlike knob() which shows a 0-99 while held. Vertical drag, gentle sweep (the
// values span a wide range). Used in the MST LCD's right gutter for DELAY + TEMPO.
static void gknob(float *v, int cx, int cy, int r, const char *vlabel) {
    ui_reg(v, cx - r, cy - r, 2 * r + 1, 2 * r + 1, 0);
    UiCap *c = ui_cap_for(v); int held = c != 0;
    if (c) { g_drag_frame = ui_frame_ct; g_drag_y = c->cy;
        if (!c->has_v0) { c->has_v0 = 1; c->v0 = *v; c->by = c->cy; }
        int py = c->released ? c->ry : c->cy;
        *v = clamp(c->v0 + (c->by - py) / 100.0f, 0, 1);   // gentle sweep (100px = full range)
        c->v0 = *v; c->by = py;
    }
    float ang = 150 + *v * 240;
    circfill(cx, cy, r, CLR_INDIGO);
    int rim = r / 4; if (rim < 1) rim = 1;   // bevel thickness scales with r → no thin/gappy arc when the knob is big
    ring(cx, cy, r - 1 - rim, r - 1, 150, 300, CLR_PINK);          // highlight (lower-left), gaps narrowed 60°→30°
    ring(cx, cy, r - 1 - rim, r - 1, -30, 120, CLR_DARKER_PURPLE); // shadow (lower-right)
    if (held) ring(cx, cy, r - 3, r, 150, ang, CLR_LIGHT_YELLOW);
    circ(cx, cy, r, held ? CLR_WHITE : CLR_BROWNISH_BLACK);
    line(cx + (int)dx(1, ang), cy + (int)dy(1, ang), cx + (int)dx(r - 1, ang), cy + (int)dy(r - 1, ang), CLR_WHITE);
    font(FONT_TINY); plabel(vlabel, cx, cy + r + 1, held ? CLR_LIGHT_YELLOW : CLR_DARK_BROWN);
}

static int cbtn(unsigned seed, int x, int y, int w, int hh, const char *s, int on2) {
    int pr = 0, hot = 0, foc = 0; void *wid = ui_wid_hash(seed, x, y, w, hh);
    int act = ui_button_core(wid, x, y, w, hh, &foc, &pr, &hot) && !drag_bounce(y, hh);
    int down = pr || on2;
    rrectfill(x, y, w, hh, 2, down ? CLR_TRUE_BLUE : CLR_DARK_BROWN);
    rrect(x, y, w, hh, 2, hot ? CLR_WHITE : CLR_BROWNISH_BLACK);
    font(FONT_TINY); print(s, x + (w - text_width(s)) / 2, y + 1, down ? CLR_WHITE : CLR_LIGHT_PEACH);
    return act;
}
// clatch — cbtn's TAP=latch / HOLD=momentary twin (the lcdlatch grammar on the candy chassis):
// a quick tap (released within PERF_TAP frames) TOGGLES *latch (persists, WHITE border); a
// longer press is momentary (on while held, no toggle). `col` = the engaged fill colour.
// Returns the EFFECTIVE state (*latch || held-now) — the MUT/REC pad-latches.
static int clatch(unsigned seed, int x, int y, int w, int hh, const char *s, int *latch, int *hf, int col) {
    int pr = 0, hot = 0, foc = 0; void *wid = ui_wid_hash(seed, x, y, w, hh);
    int act = ui_button_core(wid, x, y, w, hh, &foc, &pr, &hot) && !drag_bounce(y, hh);
    if (pr) (*hf)++;
    else { if (act && *hf <= PERF_TAP) *latch = !*latch; *hf = 0; }
    int on = *latch || pr;
    rrectfill(x, y, w, hh, 2, on ? col : CLR_DARK_BROWN);
    rrect(x, y, w, hh, 2, *latch ? CLR_WHITE : hot ? CLR_WHITE : CLR_BROWNISH_BLACK);
    font(FONT_TINY); print(s, x + (w - text_width(s)) / 2, y + 1, on ? CLR_WHITE : CLR_LIGHT_PEACH);
    return on;
}
static void chip(int x, int y, const char *s, int sel2) {
    rrectfill(x, y, 16, 8, 2, sel2 ? CLR_TRUE_BLUE : CLR_DARK_BROWN);
    font(FONT_TINY); print(s, x + (16 - text_width(s)) / 2, y + 1, sel2 ? CLR_WHITE : CLR_LIGHT_PEACH);
}
// LCD-native button — a pure green OUTLINE on the glass; active = filled green (inverted).
// For soft-keys/palettes that live INSIDE the screen, so they read as part of the readout.
static int lcdbtn(unsigned seed, int x, int y, int w, int hh, const char *s, int on2) {
    int pr = 0, hot = 0, foc = 0; void *wid = ui_wid_hash(seed, x, y, w, hh);
    int act = ui_button_core(wid, x, y, w, hh, &foc, &pr, &hot);
    int down = pr || on2;
    if (down) rrectfill(x, y, w, hh, 2, CLR_MEDIUM_GREEN);
    rrect(x, y, w, hh, 2, (hot || down) ? CLR_LIME_GREEN : CLR_MEDIUM_GREEN);
    font(FONT_TINY); print(s, x + (w - text_width(s)) / 2, y + 1, down ? CLR_DARK_GREEN : CLR_LIME_GREEN);
    return act;
}
// a live bank pad (lcdbtn variant): lit when it's the current slot, BLINKING (16th flash)
// while ARMED for a queued switch. Tap → pat_queue (bar-quantized while playing).
static int pat_pad(unsigned id, int x, int y, int w, int h, const char *lab, int m, int k) {
    int hi = (curpat[m] == k) || (armpat[m] == k && ((int)g_phase & 1));
    if (lcdbtn(id, x, y, w, h, lab, hi)) { pat_queue(m, k); return 1; }
    return 0;
}
// lcdlatch — a TAP=latch / HOLD=momentary LCD button, for the PERF play lenses. A quick tap
// (released within PERF_TAP frames) TOGGLES *latch (persists); a longer press is momentary
// (on while held, no toggle). Returns the EFFECTIVE state (*latch || held-now). `hf` tracks
// hold frames across calls; `excl` (may be 0) = a mutually-exclusive partner latch cleared
// when this one taps ON (HALF↔2X, STAC↔GLIDE). Latched = WHITE border; momentary = green fill.
static int lcdlatch(unsigned seed, int x, int y, int w, int hh, const char *s, int *latch, int *hf, int *excl) {
    int pr = 0, hot = 0, foc = 0; void *wid = ui_wid_hash(seed, x, y, w, hh);
    int act = ui_button_core(wid, x, y, w, hh, &foc, &pr, &hot);
    if (pr) (*hf)++;
    else { if (act && *hf <= PERF_TAP) { *latch = !*latch; if (*latch && excl) *excl = 0; }  // quick tap → toggle latch (+ clear the exclusive partner)
           *hf = 0; }                                                                         // release (or idle) → reset the hold counter
    int on = *latch || pr;
    if (on) rrectfill(x, y, w, hh, 2, CLR_MEDIUM_GREEN);
    rrect(x, y, w, hh, 2, *latch ? CLR_WHITE : (hot || on) ? CLR_LIME_GREEN : CLR_MEDIUM_GREEN);
    font(FONT_TINY); print(s, x + (w - text_width(s)) / 2, y + 1, on ? CLR_DARK_GREEN : CLR_LIME_GREEN);
    return on;
}

// text-fit LCD widgets that FLOW left-to-right (width = label + a little padding, not stretched to
// fill) — so screen buttons read as buttons, not full-width bars. Each advances *px past itself.
static int lcdbtnf(unsigned seed, int *px, int y, int h, const char *s, int on2) {
    font(FONT_TINY); int bw = text_width(s) + 7;
    int r = lcdbtn(seed, *px, y, bw, h, s, on2); *px += bw + 3; return r;
}
static int lcdlatchf(unsigned seed, int *px, int y, int h, const char *s, int *lat, int *hld, int *sib) {
    font(FONT_TINY); int bw = text_width(s) + 7;
    int r = lcdlatch(seed, *px, y, bw, h, s, lat, hld, sib); *px += bw + 3; return r;
}

// ── the cartridge nav strip (zone 1) ─────────────────────────────────────────
// each cartridge is a COMPOUND control: left body taps to FOCUS the face, the
// right LED taps to MUTE the machine (from any face). Two non-overlapping
// sub-buttons, so ui.h's visual-hit-wins routes touch cleanly.
// (Per-machine LEVEL lives on the MST mixer, not here — keeps the tabs compact.)
static void cartridge(Box c, int m) {
    int x = (int)c.x, y = (int)c.y, w = (int)c.w, h = (int)c.h;
    int foc = (m == face), live = !mac[m].mute;
    int ledW = h; if (ledW > 9) ledW = 9;                         // square-ish LED slot, but CAPPED — on tall/square window ratios a full-h slot starved the name body (width stays pinned at 160) and the label spilled out the left
    int bodyW = w - ledW; if (bodyW < 8) bodyW = 8;
    int prf = 0, hotf = 0, fof = 0, prm = 0, hotm = 0, fom = 0;
    void *wf = ui_wid_hash(0x70u + m, x, y, bodyW, h);            // FOCUS = the name body
    void *wm = ui_wid_hash(0x80u + m, x + bodyW, y, ledW, h);     // MUTE = the LED pad
    int af = ui_button_core(wf, x, y, bodyW, h, &fof, &prf, &hotf);
    int am = ui_button_core(wm, x + bodyW, y, ledW, h, &fom, &prm, &hotm);
    int cleanf = nav_clean(wf), cleanm = nav_clean(wm);          // latch bounce-poison at GRAB (call every frame)
    if (af && cleanf) face = m;
    if (am && cleanm) mac[m].mute = !mac[m].mute;

    rrectfill(x, y, w, h, 2, foc ? mac[m].col : mac[m].lo);
    if (foc) { blend(BLEND_AVG); line(x + 2, y + 1, x + w - 3, y + 1, CLR_WHITE); blend_reset(); }   // top sheen
    rrect(x, y, w, h, 2, (foc || hotf) ? CLR_WHITE : CLR_BROWNISH_BLACK);
    font(FONT_TINY);
    int tx = x + (bodyW - text_width(mac[m].name)) / 2; if (tx < x) tx = x;   // centre in the body, but never spill left of the cartridge
    print(mac[m].name, tx, y + (h - 5) / 2, foc ? CLR_BROWNISH_BLACK : mac[m].col);
    int lx = x + bodyW + ledW / 2, ly = y + h / 2;               // mute LED, centred in its slot
    circfill(lx, ly, 2, live ? (foc ? CLR_LIME_GREEN : CLR_DARK_GREEN) : CLR_DARKER_PURPLE);
    circ(lx, ly, 2, (hotm && live) ? CLR_WHITE : CLR_BROWNISH_BLACK);
    if (!live) line(lx - 2, ly - 2, lx + 2, ly + 2, CLR_RED);    // muted = red slash
}

static void navspine(Box nav) {
    int nc = mac[face].lo;                                        // strip takes the FOCUSED voice's darker colour
    rrectfill((int)nav.x, (int)nav.y, (int)nav.w, (int)nav.h, 3, nc);
    Box row = lay_inset(nav, 1);
    // transport (left) + home (right) bracket the cartridge run
    Box pcell = lay_split(row, EDGE_LEFT,  lay_clamp(FU * 0.8f, 12, 24), &row);
    Box hcell = lay_split(row, EDGE_RIGHT, lay_clamp(FU * 0.7f, 11, 22), &row);
    {   // PLAY / STOP
        int px = (int)pcell.x, py = (int)pcell.y, pw = (int)pcell.w - 1, ph = (int)pcell.h, pr = 0, hot = 0, fo = 0;
        void *w = ui_wid_hash(0x01u, px, py, pw, ph);
        int actp = ui_button_core(w, px, py, pw, ph, &fo, &pr, &hot), cleanp = nav_clean(w);
        if (actp && cleanp) { playing = !playing; laststep = -1; laststep303[0] = laststep303[1] = -1;
                              for (int m = 0; m < M_N; m++) armpat[m] = -1; }
        rrectfill(px, py, pw, ph, 2, playing ? CLR_TRUE_BLUE : CLR_DARK_BROWN);
        rrect(px, py, pw, ph, 2, hot ? CLR_WHITE : CLR_BROWNISH_BLACK);
        int cx = px + pw / 2, cy = py + ph / 2;
        if (playing) { rectfill(cx - 3, cy - 2, 2, 5, CLR_WHITE); rectfill(cx + 1, cy - 2, 2, 5, CLR_WHITE); }
        else trifill(cx - 2, cy - 3, cx - 2, cy + 3, cx + 3, cy, CLR_WHITE);
    }
    for (int m = 0; m < M_N; m++) cartridge(lay_grid(row, M_N, M_N, m, 2), m);   // cartridges spread across the width
    {   // HOME — toggles the LAYOUT: phone single-face+tabs ⇄ the full iPad rack
        int hx = (int)hcell.x + 1, hy = (int)hcell.y, hw = (int)hcell.w - 1, hh = (int)hcell.h, hpr = 0, hhot = 0, hfo = 0;
        void *wh = ui_wid_hash(0x03u, hx, hy, hw, hh);
        int acth = ui_button_core(wh, hx, hy, hw, hh, &hfo, &hpr, &hhot);
        if (acth && nav_clean(wh)) rack_view = rack_view ? 0 : 2;   // flip phone ⇄ ROOMY rack (nav_clean = ignore a drag-bounce tap)
        rrectfill(hx, hy, hw, hh, 2, CLR_DARK_BROWN);
        rrect(hx, hy, hw, hh, 2, hhot ? CLR_WHITE : CLR_BROWNISH_BLACK);
        // little HOUSE glyph — hand-placed so it's pixel-symmetric about cxh (the old trifill roof rasterised lopsided)
        int cxh = hx + hw / 2, cyh = hy + hh / 2 - 2;
        pset(cxh, cyh, CLR_LIGHT_PEACH);                          // roof apex
        rectfill(cxh - 1, cyh + 1, 3, 1, CLR_LIGHT_PEACH);        // roof mid (3 wide)
        rectfill(cxh - 2, cyh + 2, 5, 1, CLR_LIGHT_PEACH);        // roof base (5 wide)
        rectfill(cxh - 2, cyh + 3, 5, 3, CLR_LIGHT_PEACH);        // body (5×3)
        rectfill(cxh,     cyh + 4, 1, 2, CLR_DARK_BROWN);         // a little door, centred
    }
}

// the per-machine FX knob row (shown in zone 2 when a machine's FX panel is open):
// DRV/DIST · SEND · VERB, in the SAME three spots for every machine so they align.
// m: 0=303a 1=303b 2=808 3=909. SEND rides the shared delay (same value as MST); VERB
// = reverb send; the drive is the 303 voice DRV (rides live) or the drum kit DIST.
static void draw_fxrow(Box c, int m) {
    float *k0     = (m < 2) ? &ac[m].p[ACID_DRV] : (m == 2 ? &dist8 : &dist9);   // 303 voice drive / drum kit DIST
    const char *l0 = (m < 2) ? "DRV" : "DIST";
    knob_cell(lay_grid(c, 3, 3, 0, 2), k0,         l0,     (m < 2) ? 0.35f : 0.0f);
    knob_cell(lay_grid(c, 3, 3, 1, 2), &msend[m],  "SEND", (m < 2) ? 0.10f : 0.0f);   // → the shared delay
    knob_cell(lay_grid(c, 3, 3, 2, 2), &fxverb[m], "VERB", 0.0f);                     // → the reverb
}

// the GEN flow — CLEAR + density randomizers, drawn IN the screen (a 2×2 menu).
// m: 0/1 = a 303 line · 2 = the 808 beat · 3 = the 909 beat. Each button regenerates
// that machine's pattern; CLEAR empties it. MID = the classic (old NEW) recipe.
static void draw_gen(int m) {
    static const char *GN[4] = { "CLEAR", "MIN", "MID", "BUSY" };
    for (int g = 0; g < 4; g++) {
        int gx = 30 + (g % 2) * 53, gy = 41 + (g / 2) * 9;
        if (lcdbtn(0x40u + g, gx, gy, 50, 8, GN[g], 0)) {
            if (m < 2)       gen_line(m, g);
            else if (m == 2) gen_drums(g);
            else             gen_drums9(g);
        }
    }
}

// stack button k of n in column `col` with EVEN integer vertical spacing (every gap == `gap`,
// no lay_grid float→int drift). BOTTOM-anchored: the last cell's bottom == col bottom, and any
// integer-division remainder sits as slack ABOVE the first cell — so a flank column lines its
// last soft-key up with the glass edge instead of drifting a pixel per resolution.
static Box colcell(Box col, int n, int k, int gap) {
    if (n < 1) n = 1;
    int x = (int)col.x, w = (int)col.w;
    int y0 = (int)col.y, y1 = (int)(col.y + col.h);
    int ch = ((y1 - y0) - gap * (n - 1)) / n; if (ch < 1) ch = 1;
    int top = y1 - (n * ch + gap * (n - 1));                       // bottom-anchor the stack
    return box(x, top + k * (ch + gap), w, ch);
}

// ── a 303 face (zones 2–5) ───────────────────────────────────────────────────
static void draw_303(Box stage, int i) {
    Acid *a = &ac[i];
    // ── LANDSCAPE reflow: carve the stage into the device-face zones, each filling the width ──
    // Size bands by the DESIGN's proportions of the (chunky) canvas — NOT finger_px — so it reads
    // like the 160×100 scaled up; the leftover ratio-offset spreads into the hero + the widths.
    float H = stage.h, W = stage.w;
    Box body  = lay_inset(stage, 2);
    Box krow  = lay_split(body, EDGE_TOP,    H * 0.24f, &body);   // ② acid knob row (design ≈24/100)
    Box loopS = lay_split(body, EDGE_BOTTOM, H * 0.05f, &body);   // loop-length handle strip
    Box notes = lay_split(body, EDGE_BOTTOM, H * (seq_grid ? 0.12f : 0.28f), &body);   // GRID: thin compact strip (the LCD, = the remainder, grows into the freed space); BARS: tall note bars
    Box skcL = {0}, skcR = {0};                                  // ③ soft-key columns — BARS only; GRID moves the mode selector IN-screen (tabs) for a full-width glass
    float colw = W * 0.11f;                                      // ONE width for BOTH flank columns AND the DF switch above, so the right side reads as one column
    if (!seq_grid) { skcL = lay_split(body, EDGE_LEFT, colw, &body); skcR = lay_split(body, EDGE_RIGHT, colw, &body); }
    Box lcd   = lay_inset(body, 1);                               // the hero glass = the remainder (full-width in GRID mode, edge-to-edge with the 16-step row)

    // ② the gear-drag knob row. The DF switch at the right end flips vanilla↔DEEP (Devil Fish).
    Box krDF = lay_split(krow, EDGE_RIGHT, colw, &krow);          // same width + right edge as the GEN/KEY/PAT column below → the right side is one column
    Box krPAGE = lay_split(krDF, EDGE_BOTTOM, krDF.h * 0.44f, &krDF);      // krDF = top (VOICING switch), krPAGE = bottom (VIEW tab)
    if (rack_view && !seq_grid) { Box kg = lay_split(krow, EDGE_LEFT, colw, &krow); (void)kg; }   // iPad: align the knob row's LEFT edge with the LCD (its right already matches krDF/skcR)
    if (!kpage[i]) {                                                       // page 0 — vanilla
        knob_cell(lay_grid(krow, 5, 5, 0, 2), &a->p[ACID_CUT], "CUT", 0.55f);
        knob_cell(lay_grid(krow, 5, 5, 1, 2), &a->p[ACID_RES], "RES", 0.70f);
        knob_cell(lay_grid(krow, 5, 5, 2, 2), &a->p[ACID_ENV], "ENV", 0.55f);
        knob_cell(lay_grid(krow, 5, 5, 3, 2), &a->p[ACID_DEC], "DEC", 0.45f);
        knob_cell(lay_grid(krow, 5, 5, 4, 2), &a->p[ACID_ACC], "ACC", 0.55f);
    } else {                                                              // page 1 — DEEP: Devil Fish knobs + WAVE
        Box krW = lay_split(krow, EDGE_RIGHT, lay_clamp(FU * 0.8f, 15, 28), &krow);
        knob_cell(lay_grid(krow, 4, 4, 0, 2), &a->p[ACID_SUB],  "SUB",  0.0f);
        knob_cell(lay_grid(krow, 4, 4, 1, 2), &a->p[ACID_ADEC], "ADEC", 0.40f);
        knob_cell(lay_grid(krow, 4, 4, 2, 2), &a->p[ACID_SLDT], "SLDT", 0.14f);
        knob_cell(lay_grid(krow, 4, 4, 3, 2), &a->p[ACID_TRK],  "TRK",  0.0f);
        int wx = (int)krW.x, wy = (int)krW.y + 1, ww = (int)krW.w, wh = (int)krW.h - 6, wp = 0, whot = 0, wf = 0;  // WAVE: SAW<->SQR
        void *wv = ui_wid_hash(0x1Au, wx, wy, ww, wh);
        if (ui_button_core(wv, wx, wy, ww, wh, &wf, &wp, &whot)) { a->wave = (a->wave == INSTR_SAW) ? INSTR_SQUARE : INSTR_SAW; acid_define(a); }
        rrectfill(wx, wy, ww, wh, 2, CLR_INDIGO);
        rrect(wx, wy, ww, wh, 2, whot ? CLR_WHITE : CLR_BROWNISH_BLACK);
        font(FONT_TINY); plabel(a->wave == INSTR_SQUARE ? "SQR" : "SAW", wx + ww / 2, wy + wh / 2 - 2, CLR_LIGHT_YELLOW);
        plabel("WAVE", wx + ww / 2, wy + wh, CLR_DARK_BROWN);
    }
    {   // TOP = VOICING switch: flip classic ⟷ Devil Fish (non-destructive — a->p[] preserved).
        int bx = (int)krDF.x, by = (int)krDF.y + 1, bw = (int)krDF.w - 1, bh = (int)krDF.h - 2, dp = 0, dhot = 0, df = 0;
        void *wd = ui_wid_hash(0x07u, bx, by, bw, bh);                     // keep the old DF hash id
        if (ui_button_core(wd, bx, by, bw, bh, &df, &dp, &dhot)) {
            a->classic = !a->classic;                                     // FLIP the sound
            if (a->classic) kpage[i] = 0;                                 // DF-extras page is meaningless in classic → snap to core-5
            acid_define(a);                                              // re-apply: attack + cutoff-range change at define time
        }
        int dfon = !a->classic;                                          // LED lit = Devil Fish voicing ON
        rrectfill(bx, by, bw, bh, 2, dfon ? mac[i].col : CLR_DARK_PURPLE);
        rrect(bx, by, bw, bh, 2, (dhot || dfon) ? CLR_WHITE : CLR_BROWNISH_BLACK);
        font(FONT_TINY); plabel(dfon ? "DF" : "CL", bx + bw / 2, by + 2, dfon ? CLR_BROWNISH_BLACK : CLR_LIGHT_PEACH);
        circfill(bx + bw / 2, by + bh - 3, 1, dfon ? CLR_LIME_GREEN : CLR_DARKER_PURPLE);
    }
    {   // BOTTOM slot — DUAL PURPOSE. DF: the VIEW page-tab (core-5 ⟷ DF-extras). CLASSIC: the SAW/SQR
        // WAVE switch. WAVE is a stock-303 CORE control, so it must be reachable in vanilla — but its
        // usual home is the DF-extras page, which is unreachable in classic (kpage locked to 0). In
        // classic there's no page to switch, so this slot is free to host WAVE instead.
        int bx = (int)krPAGE.x, by = (int)krPAGE.y, bw = (int)krPAGE.w - 1, bh = (int)krPAGE.h - 1, pp = 0, phot = 0, pf = 0;
        if (a->classic) {                                                 // WAVE toggle (SAW ⟷ SQR) — same as the DEEP-page one
            void *ww2 = ui_wid_hash(0x1Au, bx, by, bw, bh);               // WAVE hash (distinct rect; never coexists with the DEEP-page WAVE)
            if (ui_button_core(ww2, bx, by, bw, bh, &pf, &pp, &phot)) { a->wave = (a->wave == INSTR_SAW) ? INSTR_SQUARE : INSTR_SAW; acid_define(a); }
            rrectfill(bx, by, bw, bh, 2, CLR_INDIGO);
            rrect(bx, by, bw, bh, 2, phot ? CLR_WHITE : CLR_BROWNISH_BLACK);
            font(FONT_TINY); plabel(a->wave == INSTR_SQUARE ? "SQR" : "SAW", bx + bw / 2, by + 1, CLR_LIGHT_YELLOW);
        } else {                                                          // VIEW page-tab (core-5 ⟷ DF-extras)
            void *wp2 = ui_wid_hash(0x05u, bx, by, bw, bh);
            if (ui_button_core(wp2, bx, by, bw, bh, &pf, &pp, &phot)) kpage[i] = !kpage[i];
            int lit = kpage[i];
            rrectfill(bx, by, bw, bh, 2, lit ? mac[i].col : CLR_DARK_PURPLE);
            rrect(bx, by, bw, bh, 2, (phot || kpage[i]) ? CLR_WHITE : CLR_BROWNISH_BLACK);
            font(FONT_TINY); plabel(kpage[i] ? "2" : "1", bx + bw / 2, by + 1, lit ? CLR_BROWNISH_BLACK : CLR_LIGHT_PEACH);
        }
    }

    // ③ soft-keys flank the LCD (left: SEQ/FLAG/FX/PERF · right: GEN/KEY/PAT) — BARS mode only.
    // GRID mode draws these as an IN-SCREEN tab row instead (below), so the glass is full-width.
    if (!seq_grid) {
    // Lay the flank keys over the GLASS's vertical extent (not the full body): the last key's
    // bottom lands on the glass edge, leaving a 1px gap above the notes strip (which starts at
    // body bottom == lcd bottom + 1) — and colcell keeps every gap equal at any resolution.
    Box colL = box(skcL.x, lcd.y, skcL.w, lcd.h);
    Box colR = box(skcR.x, lcd.y, skcR.w, lcd.h);
    { static const char *L[4] = { "SEQ", "FLAG", "FX", "PERF" }; static const int Lm[4] = { PS_SEQ, PS_FLAG, PS_FX, PS_PERF };
      static const unsigned Ls[4] = { 0x08u, 0x09u, 0x06u, 0x37u };
      for (int k = 0; k < 4; k++) { Box c = colcell(colL, 4, k, 2);
          if (cbtn(Ls[k], (int)c.x, (int)c.y, (int)c.w, (int)c.h, L[k], pscreen[i] == Lm[k])) pscreen[i] = Lm[k]; } }
    { static const char *R[3] = { "GEN", "KEY", "PAT" }; static const int Rm[3] = { PS_GEN, PS_KEY, PS_PAT };
      static const unsigned Rs[3] = { 0x31u, 0x34u, 0x35u };
      for (int k = 0; k < 3; k++) { Box c = colcell(colR, 3, k, 2);
          if (cbtn(Rs[k], (int)c.x, (int)c.y, (int)c.w, (int)c.h, R[k], pscreen[i] == Rm[k])) pscreen[i] = Rm[k]; } }
    }
    rrectfill((int)lcd.x, (int)lcd.y, (int)lcd.w, (int)lcd.h, 3, CLR_BROWNISH_BLACK);
    Box glass = lay_inset(lcd, 2);
    rrectfill((int)glass.x, (int)glass.y, (int)glass.w, (int)glass.h, 2, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = (int)glass.y + 1; y < glass.y + glass.h - 1; y += 2) line((int)glass.x, y, (int)(glass.x + glass.w - 1), y, CLR_BROWNISH_BLACK); blend_reset();
    Box gc = lay_inset(glass, 2);                                   // content area inside the glass
    if (seq_grid) {   // IN-SCREEN tab menu (acidwide H) → full-width glass, no side columns. 7 mode tabs + BARS/GRID + STEP/KEYS.
        Box tabs = lay_split(gc, EDGE_TOP, gc.h * 0.20f, &gc);
        static const char *TL[7] = { "SEQ", "FLAG", "FX", "PERF", "GEN", "KEY", "PAT" };
        static const int   TM[7] = { PS_SEQ, PS_FLAG, PS_FX, PS_PERF, PS_GEN, PS_KEY, PS_PAT };
        static const unsigned TH[7] = { 0x08u, 0x09u, 0x06u, 0x37u, 0x31u, 0x34u, 0x35u };   // reuse the soft-key hashes (never coexist)
        for (int t = 0; t < 7; t++) { Box c = lay_grid(tabs, 9, 9, t, 1);
            if (lcdbtn(TH[t], (int)c.x, (int)c.y, (int)c.w, (int)c.h, TL[t], pscreen[i] == TM[t])) pscreen[i] = TM[t]; }
        Box cg = lay_grid(tabs, 9, 9, 7, 1);
        if (lcdbtn(0x2Du, (int)cg.x, (int)cg.y, (int)cg.w, (int)cg.h, "GRID", 1)) seq_grid = 0;   // lit → tap = back to BARS
        Box ck = lay_grid(tabs, 9, 9, 8, 1);
        if (lcdbtn(0x2Eu, (int)ck.x, (int)ck.y, (int)ck.w, (int)ck.h, keys_mode ? "KEYS" : "STEP", keys_mode)) keys_mode = !keys_mode;
    }
    if (pscreen[i] == PS_FLAG) {
        for (int f = 0; f < FL_N; f++) { Box c = lay_grid(gc, 3, FL_N, f, 2);   // the 6-flag palette
            if (lcdbtn(0x0Au + f, (int)c.x, (int)c.y, (int)c.w, (int)c.h, FLNAME[f], armed == f)) armed = f; }
    } else if (pscreen[i] == PS_FX) {                                 // FX + voice character, LCD-native
        Box knobs; Box clean = lay_split(gc, EDGE_BOTTOM, gc.h * 0.28f, &knobs);   // bottom strip = the CLEAN/RAW saw toggle
        lcdknob_cell(lay_grid(knobs, 4, 4, 0, 2), &a->p[ACID_DRV], "DRV",   0.35f);
        lcdknob_cell(lay_grid(knobs, 4, 4, 1, 2), &msend[i],       "SEND",  0.10f);
        lcdknob_cell(lay_grid(knobs, 4, 4, 2, 2), &fxverb[i],      "VERB",  0.0f);
        lcdknob_cell(lay_grid(knobs, 4, 4, 3, 2), &a->drift,       "DRIFT", 0.5f);   // analog wander amount (rides live via acid_ride)
        if (lcdbtn(0x46u, (int)clean.x, (int)clean.y, (int)clean.w, (int)clean.h, a->clean ? "CLEAN SAW" : "RAW SAW", a->clean)) { a->clean = !a->clean; acid_define(a); }   // PolyBLEP band-limit toggle (SAW only)
    } else if (pscreen[i] == PS_GEN) {                                // CLEAR + randomize menu (2×2)
        static const char *GN[4] = { "CLEAR", "MIN", "MID", "BUSY" };
        for (int g = 0; g < 4; g++) { Box c = lay_grid(gc, 2, 4, g, 2);
            if (lcdbtn(0x40u + g, (int)c.x, (int)c.y, (int)c.w, (int)c.h, GN[g], 0)) gen_line(i, g); }
    } else if (pscreen[i] == PS_KEY) {                                // KEY — root (12-note strip) + scale + octave
        Box krow2; Box ktop = lay_split(gc, EDGE_TOP, gc.h * 0.52f, &krow2);
        for (int k = 0; k < 12; k++) {
            int sharp = (k == 1 || k == 3 || k == 6 || k == 8 || k == 10);
            Box c = lay_grid(ktop, 12, 12, k, 1);
            int px = (int)c.x, py = (int)c.y, pw = (int)c.w, ph = (int)c.h;
            int pr = 0, hot = 0, foc = 0; void *w = ui_wid_hash(0x70u + k, px, py, pw, ph);
            if (ui_button_core(w, px, py, pw, ph, &foc, &pr, &hot)) mroot[i] = k;
            int lit = (mroot[i] == k);
            rrectfill(px, py, pw, ph, 1, lit ? CLR_LIME_GREEN : sharp ? CLR_BROWNISH_BLACK : CLR_DARK_GREEN);
            rrect(px, py, pw, ph, 1, (lit || hot) ? CLR_WHITE : CLR_DARK_GREEN);
            if (!sharp) { font(FONT_TINY); plabel(NOTE[k], px + pw / 2, py + 1, lit ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREEN); }
        }
        Box sc = lay_split(krow2, EDGE_LEFT, krow2.w * 0.45f, &krow2);   // scale name (left)
        if (lcdbtn(0x78u, (int)sc.x, (int)sc.y, (int)sc.w, (int)sc.h, SCALES[mscale[i]].name, 0)) {
            int oldsc = mscale[i], nw = (oldsc + 1) % NSCALE;                 // REMAP the live line to the same DEGREE in the new
            for (int s = 0; s < STEPS; s++) {                                 // scale, so maj↔minor actually re-pitches the melody
                int di = scale_idx(oldsc, pit[i][s]); if (di >= SCALES[nw].n) di = SCALES[nw].n - 1;
                pit[i][s] = SCALES[nw].deg[di];
            }
            mscale[i] = nw;
        }
        Box ol = lay_split(krow2, EDGE_LEFT, krow2.w * 0.28f, &krow2);   // "OCT" label
        font(FONT_TINY); plabel("OCT", (int)(ol.x + ol.w / 2), (int)(ol.y + ol.h / 2 - 2), CLR_DARK_BROWN);
        Box om = lay_grid(krow2, 3, 3, 0, 1), on2 = lay_grid(krow2, 3, 3, 1, 1), op = lay_grid(krow2, 3, 3, 2, 1);
        if (lcdbtn(0x79u, (int)om.x, (int)om.y, (int)om.w, (int)om.h, "-", 0) && loct[i] > -2) loct[i]--;
        { char ob[4]; ob[0] = loct[i] > 0 ? '+' : loct[i] < 0 ? '-' : ' '; ob[1] = '0' + (loct[i] < 0 ? -loct[i] : loct[i]); ob[2] = 0;
          plabel(ob, (int)(on2.x + on2.w / 2), (int)(on2.y + on2.h / 2 - 2), CLR_LIME_GREEN); }
        if (lcdbtn(0x7Au, (int)op.x, (int)op.y, (int)op.w, (int)op.h, "+", 0) && loct[i] < 2) loct[i]++;
    } else if (pscreen[i] == PS_PAT) {                                // PAT — A-D pattern banks
        static const char *AD[NPAT] = { "A", "B", "C", "D" };
        for (int k = 0; k < NPAT; k++) { Box c = lay_grid(lay_inset(gc, 1), NPAT, NPAT, k, 3);
            pat_pad(0x3Bu + k, (int)c.x, (int)c.y, (int)c.w, (int)c.h, AD[k], i, k); }
    } else if (pscreen[i] == PS_PERF) {                              // PERF — play LENSES (TAP=latch / HOLD=momentary)
        // A 4×2 grid. Row 1: HALF · ACC · OCT · REV  ·  Row 2: STAC · GLIDE · ROLL (last cell spare).
        // Read-path lenses (TAP=latch / HOLD=momentary); ROLL is momentary by nature (hold to stutter).
        Box c0 = lay_grid(gc, 4, 8, 0, 2), c1 = lay_grid(gc, 4, 8, 1, 2), c2 = lay_grid(gc, 4, 8, 2, 2), c3 = lay_grid(gc, 4, 8, 3, 2);
        Box c4 = lay_grid(gc, 4, 8, 4, 2), c5 = lay_grid(gc, 4, 8, 5, 2), c6 = lay_grid(gc, 4, 8, 6, 2);
        pf_half[i]  = lcdlatch(0x7Bu, (int)c0.x, (int)c0.y, (int)c0.w, (int)c0.h, "HALF",  &pf_latch[PL_HALF][i],  &pf_hold[PL_HALF][i],  0);
        pf_acc[i]   = lcdlatch(0x7Du, (int)c1.x, (int)c1.y, (int)c1.w, (int)c1.h, "ACC",   &pf_latch[PL_ACC][i],   &pf_hold[PL_ACC][i],   0);
        pf_oct[i]   = lcdlatch(0x80u, (int)c2.x, (int)c2.y, (int)c2.w, (int)c2.h, "OCT",   &pf_latch[PL_OCT][i],   &pf_hold[PL_OCT][i],   0);
        pf_rev[i]   = lcdlatch(0x82u, (int)c3.x, (int)c3.y, (int)c3.w, (int)c3.h, "REV",   &pf_latch[PL_REV][i],   &pf_hold[PL_REV][i],   0);
        pf_stac[i]  = lcdlatch(0x7Eu, (int)c4.x, (int)c4.y, (int)c4.w, (int)c4.h, "STAC",  &pf_latch[PL_STAC][i],  &pf_hold[PL_STAC][i],  &pf_latch[PL_GLIDE][i]);
        pf_glide[i] = lcdlatch(0x7Fu, (int)c5.x, (int)c5.y, (int)c5.w, (int)c5.h, "GLIDE", &pf_latch[PL_GLIDE][i], &pf_hold[PL_GLIDE][i], &pf_latch[PL_STAC][i]);
        pf_roll[i]  = lcdlatch(0x83u, (int)c6.x, (int)c6.y, (int)c6.w, (int)c6.h, "ROLL",  &pf_latch[PL_ROLL][i],  &pf_hold[PL_ROLL][i],  0);
    } else {                                                          // SEQ — piano-roll readout OR editable GRID (seq_grid)
        char nb[4]; { int bi = (int)(g_bpm + 0.5f), ni = 0;
          if (bi >= 100) nb[ni++] = '0' + bi / 100;
          nb[ni++] = '0' + (bi / 10) % 10; nb[ni++] = '0' + bi % 10; nb[ni] = 0; }
        if (!seq_grid) {                                             // BARS: in-content header (BPM readout; the GRID toggle is hidden unless grid_view_on)
            Box ghdr = lay_split(gc, EDGE_TOP, gc.h * 0.20f, &gc);
            font(FONT_TINY); print(nb, (int)ghdr.x, (int)ghdr.y, CLR_MEDIUM_GREEN);
            if (grid_view_on) {                                      // the BARS→GRID chip — hidden by default (note-bars are the shipping view)
                Box tg = lay_split(ghdr, EDGE_RIGHT, ghdr.w * 0.30f, &ghdr);
                if (lcdbtn(0x2Du, (int)tg.x, (int)tg.y, (int)tg.w, (int)tg.h, "BARS", 0)) seq_grid = 1;
            }
        } else {                                                     // GRID: the tab row above carries the chrome; just a small BPM in the corner
            font(FONT_TINY); print(nb, (int)gc.x, (int)gc.y, CLR_MEDIUM_GREEN);
        }
        float sw = gc.w / (float)plen[i];                            // px per step across the glass
        int top = (int)gc.y, bot = (int)(gc.y + gc.h), span = bot - top; if (span < 8) span = 8;
        if (seq_grid) {   // EDITABLE grid: drag on the screen — x = step, y = pitch (scale-snapped); the bottom band = rest → erase
            void *wg = ui_wid_hash(0x2Cu, (int)gc.x, top, (int)gc.w, span); ui_reg(wg, (int)gc.x, top, (int)gc.w, span, 0);
            UiCap *gcc = ui_cap_for(wg);
            if (gcc) { g_drag_frame = ui_frame_ct; g_drag_y = gcc->cy;
                int px = gcc->released ? gcc->rx : gcc->cx, py = gcc->released ? gcc->ry : gcc->cy;
                int cell = (int)((px - gc.x) / sw); if (cell < 0) cell = 0; if (cell >= plen[i]) cell = plen[i] - 1;
                sel[i] = cell;
                if (py >= bot - 4) on[i][cell] = 0;                  // bottom rest band → note OFF (pitch kept)
                else { float frac = clamp((bot - 4 - py) / (float)(span - 5), 0, 1);
                       pit[i][cell] = SCALES[mscale[i]].deg[(int)(frac * (SCALES[mscale[i]].n - 1) + 0.5f)]; on[i][cell] = 1; } }
        }
        int heldy = -1;
        for (int s = 0; s < plen[i]; s++) {
            int cx = (int)(gc.x + s * sw), pw = (int)sw; if (pw < 2) pw = 2;
            if (s == lpos[i] && playing) { blend(BLEND_AVG); rectfill(cx, top, pw, span, CLR_MEDIUM_GREEN); blend_reset(); }
            int pitch = pit[i][s] + oct[i][s] * 6; if (pitch < 0) pitch = 0; if (pitch > 24) pitch = 24;   // ~2 octaves of range
            int y = bot - 2 - pitch * (span - 3) / 24;
            if (on[i][s]) {
                if (acc[i][s]) rectfill(cx, y - 3, pw - 1, 1, CLR_ORANGE);                   // accent cap above
                rectfill(cx, y, pw - 1, 2, acc[i][s] ? CLR_LIGHT_YELLOW : CLR_LIME_GREEN);   // the note
                if (sld[i][s]) {
                    int ns = (s + 1) % plen[i];
                    if (on[i][ns] || tie[i][ns]) {
                        int np = tie[i][ns] ? pitch : pit[i][ns] + oct[i][ns] * 6; if (np < 0) np = 0; if (np > 24) np = 24;
                        int ny = bot - 2 - np * (span - 3) / 24;
                        line(cx + pw - 1, y + 1, (int)(gc.x + (s + 1) * sw), ny + 1, CLR_MEDIUM_GREEN);
                    }
                }
                heldy = y;
            } else if (tie[i][s] && heldy >= 0) {
                rectfill(cx, heldy, pw, 2, CLR_MEDIUM_GREEN);                            // tie → the held note carries on
            } else heldy = -1;
        }
    }

    static int use_bars = 1;   // 1 = the drag-to-pitch NOTE BARS; 0 = the old step row + keybed
    if (seq_grid && keys_mode) {
        // KEYS — a 13-note chromatic keypad (C..C) + OCT- / OCT+ / SLIDE. 303 STEP-ENTRY: a tap writes
        // the note into the SELECTED step (sel[i]) and advances the cursor; chromatic, so it escapes the
        // scale-lock like a real 303. Auditions only WHILE PLAYING (the sequencer gates it on the next
        // 16th → no hung note); stopped = silent write. SLIDE toggles slide on the step you just entered.
        static const char *KLAB[16] = { "C","","D","","E","F","","G","","A","","B","C","O-","O+","SL" };
        static const int   kblack[13] = { 0,1,0,1,0,0,1,0,1,0,1,0,0 };
        float kw = notes.w / 16.0f;
        for (int k = 0; k < 16; k++) {
            int kx = (int)(notes.x + k * kw), kcw = (int)kw - 1; if (kcw < 3) kcw = 3;
            int ky = (int)notes.y, kh = (int)notes.h;
            void *w = ui_wid_hash(0xC0u + k, kx, ky, kcw, kh); ui_reg(w, kx, ky, kcw, kh, 0);
            UiCap *c = ui_cap_for(w); int hot = (c != 0);
            if (c && ui_grabbed(w)) {                                        // fire once on grab (tap)
                if (k <= 12) {                                               // note → write sel + advance
                    pit[i][sel[i]] = k; oct[i][sel[i]] = key_oct; on[i][sel[i]] = 1; mbop = 1;
                    if (playing) acid_note(&ac[i], ac[i].base + mroot[i] + loct[i] * 12 + k + key_oct * 12, acc[i][sel[i]], 0);
                    sel[i] = (sel[i] + 1) % plen[i];
                } else if (k == 13) { if (key_oct > -1) key_oct--; }
                else if (k == 14) { if (key_oct <  2) key_oct++; }
                else { int ls = (sel[i] + plen[i] - 1) % plen[i]; sld[i][ls] = !sld[i][ls]; }   // SLIDE on the last-entered step
            }
            int fill = (k <= 12) ? (kblack[k] ? CLR_BROWNISH_BLACK : CLR_DARK_GREEN)
                     : (k == 15) ? CLR_INDIGO : CLR_DARK_PURPLE;
            rrectfill(kx, ky, kcw, kh, 1, fill);
            rrect(kx, ky, kcw, kh, 1, hot ? CLR_WHITE : CLR_BROWNISH_BLACK);
            font(FONT_TINY);
            if (k <= 12) { if (KLAB[k][0]) plabel(KLAB[k], kx + kcw / 2, ky + 1, kblack[k] ? CLR_MEDIUM_GREEN : CLR_LIME_GREEN); }
            else plabel(KLAB[k], kx + kcw / 2, ky + 1, CLR_LIGHT_PEACH);
        }
    } else if (use_bars) {
        // ④⑤ the 16 NOTE BARS — tap = note on/off · drag up/down = pitch (scale-snapped).
        // One chunky surface; bar HEIGHT is the pitch, so you draw + see the melody.
        int by = (int)notes.y, bh = (int)notes.h;   // the note-bar band, filling the width
        float stepw = notes.w / (float)STEPS;        // px per step across the band
        for (int s = 0; s < STEPS; s++) {
            int bx = (int)(notes.x + s * stepw), bw = (int)stepw - 1, dead = (s >= plen[i]); if (bw < 3) bw = 3;
            void *w = ui_wid_hash(0xB0u + s, bx, by, bw, bh);
            ui_reg(w, bx, by, bw, bh, 0);
            UiCap *c = ui_cap_for(w);
            if (c) { g_drag_frame = ui_frame_ct; g_drag_y = c->cy; }
            if (pscreen[i] == PS_FLAG) {                 // FLAG mode: tap or DRAG paints the armed flag
                if (c) {                                 // the captured bar tracks the finger across the row
                    if (ui_grabbed(w)) paint_val = !flag_get(i, s, armed);
                    int fx = c->released ? c->rx : c->cx, cell = (int)((fx - notes.x) / stepw);
                    if (cell < 0) cell = 0; if (cell >= STEPS) cell = STEPS - 1;
                    sel[i] = cell;
                    flag_set(i, cell, armed, paint_val);             // paint the same value across the drag
                }
            } else if (c) {                              // NORMAL (SEQ): tap toggles; DRAG draws the melody
                if (ui_grabbed(w)) { drag_gx = c->cx; drag_gy = c->cy; drag_axis = 0; drag_on0 = on[i][s]; sel[i] = s; }
                int px = c->released ? c->rx : c->cx, py = c->released ? c->ry : c->cy;
                int adx = px - drag_gx; if (adx < 0) adx = -adx;
                int ady = py - drag_gy; if (ady < 0) ady = -ady;
                if (!drag_axis && (adx > 4 || ady > 4)) drag_axis = 1;   // moved past the deadzone → drawing
                if (drag_axis && !seq_grid) {                        // free draw: height = pitch, bottom = rest (GRID mode owns pitch → bars are tap-on/off only)
                    int cell = (int)((px - notes.x) / stepw); if (cell < 0) cell = 0; if (cell >= STEPS) cell = STEPS - 1;
                    sel[i] = cell;
                    if (py >= by + bh - 4) on[i][cell] = 0;          // bottom band → note OFF (pitch kept)
                    else {
                        float frac = clamp((by + bh - 4 - py) / (float)(bh - 5), 0, 1);
                        pit[i][cell] = SCALES[mscale[i]].deg[(int)(frac * (SCALES[mscale[i]].n - 1) + 0.5f)];
                        on[i][cell] = 1;
                    }
                }
                if (ui_released(w) && !drag_axis) { on[i][s] = !drag_on0; sel[i] = s; if (on[i][s]) mbop = 1; }
            }
            int here = (s == lpos[i] && playing);
            rrectfill(bx, by, bw, bh, 1, dead ? CLR_DARKER_PURPLE : CLR_DARK_BROWN);
            if (here) { blend(BLEND_AVG); rrectfill(bx, by, bw, bh, 1, CLR_MEDIUM_GREEN); blend_reset(); }
            if (on[i][s] && !dead) {
                int idx = scale_idx(mscale[i], pit[i][s]);
                int fh = seq_grid ? bh : bh * (idx + 1) / SCALES[mscale[i]].n;   // GRID: full-cell (pitch lives on the grid) · BARS: pitch-proportional
                rrectfill(bx, by + bh - fh, bw, fh, 1, acc[i][s] ? CLR_ORANGE : CLR_LIME_GREEN);
                if (sld[i][s] && !seq_grid) {                       // slide → bright top cap + a GLIDE LINE to the next note (BARS only; the grid draws its own slide line)
                    rectfill(bx, by + bh - fh, bw, 1, CLR_WHITE);
                    int ns = (s + 1) % plen[i];
                    int nidx = tie[i][ns] ? idx : scale_idx(mscale[i], pit[i][ns]);
                    int ntop = (on[i][ns] || tie[i][ns]) ? by + bh - bh * (nidx + 1) / SCALES[mscale[i]].n : by + bh - fh;
                    line(bx + bw - 1, by + bh - fh, bx + bw + 1, ntop, CLR_WHITE);
                }
                if (oct[i][s] > 0) rectfill(bx + 1, by + 1, bw - 2, 2, CLR_LIGHT_YELLOW);           // OCT+
                if (oct[i][s] < 0) rectfill(bx + 1, by + bh - 3, bw - 2, 2, CLR_TRUE_BLUE);         // OCT-
            }
            if (tie[i][s] && !on[i][s] && !dead) rectfill(bx, by + bh / 2, bw, 2, CLR_TRUE_BLUE);  // TIE = hold prev
            if (!dead && s == plen[i] - 1 && plen[i] < STEPS) rectfill(bx + bw, by, 1, bh, CLR_RED);  // loop end
            rrect(bx, by, bw, bh, 1, (here || (pscreen[i] != PS_FLAG && s == sel[i])) ? CLR_WHITE : CLR_BROWNISH_BLACK);
        }
        {   // LOOP-END handle — a little grab RECT in the strip just BELOW the cells, sitting at the
            // loop boundary (the end of the length). Drag it to set THIS line's loop LENGTH /
            // polymeter. Fixed-rect hit-lane (stable capture) so the rect can ride under the finger.
            // Was the FL_LEN flag.
            int ly = by + bh, lw = (int)notes.w, lx = (int)notes.x;            // the clean strip below the bars
            void *wl = ui_wid_hash(0x2Au, lx, ly, lw, (int)loopS.h + 2); ui_reg(wl, lx, ly, lw, (int)loopS.h + 2, 0);
            UiCap *lc = ui_cap_for(wl);
            if (lc) { g_drag_frame = ui_frame_ct; g_drag_y = lc->cy;
                int fx = lc->released ? lc->rx : lc->cx, n = (int)((fx - notes.x) / stepw) + 1;
                if (n < 1) n = 1; if (n > STEPS) n = STEPS; plen[i] = n;
            }
            int bxr = (int)(notes.x + plen[i] * stepw), th = (lc != 0);
            line(lx, ly + 1, bxr - 2, ly + 1, CLR_MEDIUM_GREEN);                // active-length track, into the handle
            rrectfill(bxr - 2, ly, 4, 4, 1, th ? CLR_WHITE : CLR_LIGHT_YELLOW);  // tiny grab rect — flush at the last active cell's bottom-right, at the loop end
            rrect(bxr - 2, ly, 4, 4, 1, CLR_BROWNISH_BLACK);
        }
    } else {
        // ④ step row — tap toggles
        for (int s = 0; s < STEPS; s++) {
            int x = 6 + s * 9, pr = 0, hot = 0, foc = 0;
            void *wid = ui_wid_hash(0x30u + s, x, 64, 8, 9);
            if (ui_button_core(wid, x, 64, 8, 9, &foc, &pr, &hot)) { on[i][s] = !on[i][s]; sel[i] = s; if (on[i][s]) mbop = 1; }
            int fc = on[i][s] ? (acc[i][s] ? CLR_ORANGE : CLR_LIME_GREEN) : CLR_DARK_BROWN;
            if (s == step && playing) fc = CLR_WHITE;
            rrectfill(x, 64, 8, 9, 1, fc);
            if (s == sel[i]) rrect(x - 1, 63, 10, 11, 1, CLR_LIGHT_YELLOW);
            rrect(x, 64, 8, 9, 1, CLR_BROWNISH_BLACK);
        }
        // ⑤ keybed — tap a key to set the selected step's pitch + audition it
        int kb = 6, ky = 77, kh = 16;
        static const int isblack[12] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };
        int wi = 0;
        for (int n = 0; n < 12; n++) if (!isblack[n]) {
            int x = kb + wi * 21; wi++;
            int pr = 0, hot = 0, foc = 0; void *wid = ui_wid_hash(0x50u + n, x, ky, 20, kh);
            if (ui_button_core(wid, x, ky, 20, kh, &foc, &pr, &hot)) { pit[i][sel[i]] = n; on[i][sel[i]] = 1; mbop = 1; acid_note(a, a->base + mroot[i] + loct[i] * 12 + n, 0, 0); }
            int lit = pit[i][sel[i]] == n && on[i][sel[i]];
            rrectfill(x, ky, 20, kh, 2, lit ? CLR_LIGHT_YELLOW : CLR_LIGHT_PEACH);
            rrect(x, ky, 20, kh, 2, CLR_BROWNISH_BLACK);
        }
        wi = 0;
        for (int n = 0; n < 12; n++) {
            if (isblack[n]) {
                int x = kb + wi * 21 - 6;
                int pr = 0, hot = 0, foc = 0; void *wid = ui_wid_hash(0x60u + n, x, ky, 12, 9);
                if (ui_button_core(wid, x, ky, 12, 9, &foc, &pr, &hot)) { pit[i][sel[i]] = n; on[i][sel[i]] = 1; mbop = 1; acid_note(a, a->base + mroot[i] + loct[i] * 12 + n, 0, 0); }
                int lit = pit[i][sel[i]] == n && on[i][sel[i]];
                rrectfill(x, ky, 12, 9, 1, lit ? CLR_LIGHT_YELLOW : CLR_BROWNISH_BLACK);
                rrect(x, ky, 12, 9, 1, CLR_BLACK);
            } else wi++;
        }
    }
}

// ── the 808 DRUM face — the device-face paradigm (NOT a full grid): the bottom
// is the work surface (voice picker + the 16 HITS of the picked voice); the screen
// stays free for the kit overview + tweaks. Its identity: a blue 808 badge, blue
// voice pads, and the hits in the 808's blue (brighter on the downbeat = find the 4th).
// the drum PERF screen (shared by both drum faces) — ROLL + ACC for the face being drawn.
// m is derived from `face` (draw_808 → 0, draw_909 → 1), so the branch is identical in both.
static void draw_drum_perf(Box gc) {
    int m = (face == M_909) ? 1 : 0;
    // Row 1: BEAT-REPEAT RP1/RP2/RP4 (hold → loop the last 1/2/4 steps) · Row 2: THIN · BUSY · ACC.
    Box a0 = lay_grid(gc, 3, 6, 0, 2), a1 = lay_grid(gc, 3, 6, 1, 2), a2 = lay_grid(gc, 3, 6, 2, 2);
    Box b0 = lay_grid(gc, 3, 6, 3, 2), b1 = lay_grid(gc, 3, 6, 4, 2), b2 = lay_grid(gc, 3, 6, 5, 2);
    pf_rp1[m]  = lcdlatch(0x84u, (int)a0.x, (int)a0.y, (int)a0.w, (int)a0.h, "RP1",  &dpf_latch[DPL_RP1][m],  &dpf_hold[DPL_RP1][m],  0);
    pf_rp2[m]  = lcdlatch(0x85u, (int)a1.x, (int)a1.y, (int)a1.w, (int)a1.h, "RP2",  &dpf_latch[DPL_RP2][m],  &dpf_hold[DPL_RP2][m],  0);
    pf_rp4[m]  = lcdlatch(0x86u, (int)a2.x, (int)a2.y, (int)a2.w, (int)a2.h, "RP4",  &dpf_latch[DPL_RP4][m],  &dpf_hold[DPL_RP4][m],  0);
    pf_thin[m] = lcdlatch(0x87u, (int)b0.x, (int)b0.y, (int)b0.w, (int)b0.h, "THIN", &dpf_latch[DPL_THIN][m], &dpf_hold[DPL_THIN][m], &dpf_latch[DPL_BUSY][m]);
    pf_busy[m] = lcdlatch(0x88u, (int)b1.x, (int)b1.y, (int)b1.w, (int)b1.h, "BUSY", &dpf_latch[DPL_BUSY][m], &dpf_hold[DPL_BUSY][m], &dpf_latch[DPL_THIN][m]);
    pf_dacc[m] = lcdlatch(0x89u, (int)b2.x, (int)b2.y, (int)b2.w, (int)b2.h, "ACC",  &dpf_latch[DPL_ACC][m],  &dpf_hold[DPL_ACC][m],  0);
}

static void draw_808(Box stage) {
    // acid order (from acidrack): always-on kick/hats/clap/snare, then cowbell + rim/clave,
    // then the fill voices (maracas/cymbal/toms/congas) — page 1 is a whole beat on its own.
    static const int VL[TR_NV] = { TR_BD, TR_CH, TR_OH, TR_CP, TR_SD, TR_CB, TR_RS, TR_CLV,
                                   TR_MA, TR_CY, TR_LT, TR_MT, TR_HT, TR_LC, TR_MC, TR_HC };
    // LANDSCAPE reflow (canvas-density-spectrum.md): design-proportion bands, spread to fill.
    float H = stage.h, W = stage.w;
    Box body   = lay_inset(stage, 2);
    Box krow   = lay_split(body, EDGE_TOP,    H * 0.22f, &body);            // ② FX row
    Box bottom = lay_split(body, EDGE_BOTTOM, H * 0.36f, &body);            // ④⑤ picker + hits + tool
    Box skcL   = lay_split(body, EDGE_LEFT,   W * 0.11f, &body);            // ③ soft-key columns
    Box skcR   = lay_split(body, EDGE_RIGHT,  W * 0.11f, &body);
    Box lcd    = lay_inset(body, 1);                                                      // the hero glass
    if (rack_view) { float cw = W * 0.11f; krow.x += cw; krow.w -= 2 * cw; }  // iPad: align the FX row (DIST/SEND/VERB) to the LCD x-span
    Box tool   = lay_split(bottom, EDGE_RIGHT,  W * 0.05f, &bottom);        // ④a pad TOOL (right edge)
    Box hits   = lay_split(bottom, EDGE_BOTTOM, bottom.h * 0.60f, &bottom); // ⑤ the 16 hits
    Box pick   = bottom;                                                    // ④ voice picker row

    draw_fxrow(krow, 2);                                    // ② DIST · SEND · VERB

    if (darmed == DD_STRK) darmed = DD_ACC;                 // STRK is 909-only — reset if armed here
    // ③ soft-keys flank the LCD, split by SCOPE — LEFT = the selected VOICE (VCE/FLAG views +
    // the MUT pad-latch) · RIGHT = the MACHINE (GEN/PAT/PERF/KIT). KIT moved right: it's the
    // whole-kit minimap, the one machine key that used to masquerade as a voice key.
    { static const char *L[2] = { "VCE", "FLAG" }; static const int Lm[2] = { DS_VCE, DS_FLAG };   // VCE holds TONE *and* MIX knobs in one panel
      static const unsigned Ls[2] = { 0x22u, 0x21u };
      for (int k = 0; k < 2; k++) { Box c = lay_grid(skcL, 1, 3, k, 2);
          if (cbtn(Ls[k], (int)c.x, (int)c.y, (int)c.w, (int)c.h, L[k], dscreen == Lm[k])) dscreen = Lm[k]; }
      Box mc = lay_grid(skcL, 1, 3, 2, 2);                  // MUT — tap=latch / hold=momentary; pads mute DIRECTLY while live
      dmut_now = clatch(0x23u, (int)mc.x, (int)mc.y, (int)mc.w, (int)mc.h, "MUT", &dmut_latch, &dmut_hold, CLR_ORANGE); }
    { static const char *R[4] = { "GEN", "PAT", "PERF", "KIT" }; static const int Rm[4] = { DS_GEN, DS_PAT, DS_PERF, DS_KIT };
      static const unsigned Rs[4] = { 0x31u, 0x36u, 0x39u, 0x20u };
      for (int k = 0; k < 4; k++) { Box c = lay_grid(skcR, 1, 4, k, 2);
          if (cbtn(Rs[k], (int)c.x, (int)c.y, (int)c.w, (int)c.h, R[k], dscreen == Rm[k])) dscreen = Rm[k]; } }
    rrectfill((int)lcd.x, (int)lcd.y, (int)lcd.w, (int)lcd.h, 3, CLR_BROWNISH_BLACK);
    Box glass = lay_inset(lcd, 2);
    rrectfill((int)glass.x, (int)glass.y, (int)glass.w, (int)glass.h, 2, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = (int)glass.y + 1; y < glass.y + glass.h - 1; y += 2) line((int)glass.x, y, (int)(glass.x + glass.w - 1), y, CLR_BROWNISH_BLACK); blend_reset();
    Box gc = lay_inset(glass, 2);
    if (dscreen == DS_GEN) {                                // CLEAR + randomize (2×2)
        static const char *GN[4] = { "CLEAR", "MIN", "MID", "BUSY" };
        for (int g = 0; g < 4; g++) { Box c = lay_grid(gc, 2, 4, g, 2);
            if (lcdbtn(0x40u + g, (int)c.x, (int)c.y, (int)c.w, (int)c.h, GN[g], 0)) gen_drums(g); }
    } else if (dscreen == DS_FLAG) {
        if (darmed == DD_CHAR && !CH8[dsel]) darmed = DD_TUN;   // this voice has no character knob
        Box r2; Box r1 = lay_split(gc, EDGE_TOP, gc.h * 0.5f, &r2);   // row1 = flags · row2 = p-locks
        Box a0 = lay_grid(r1, 2, 2, 0, 2), a1 = lay_grid(r1, 2, 2, 1, 2);
        if (lcdbtn(0x24u + DD_ACC,  (int)a0.x, (int)a0.y, (int)a0.w, (int)a0.h, "ACC",  darmed == DD_ACC))  darmed = DD_ACC;
        if (lcdbtn(0x24u + DD_PROB, (int)a1.x, (int)a1.y, (int)a1.w, (int)a1.h, "PROB", darmed == DD_PROB)) darmed = DD_PROB;
        Lane L[LK_N]; int nl = drum_lanes(M_808, dsel, L);      // the continuous lanes (id-stable)
        for (int li = 0; li < nl; li++) { int dd = DD_TUN + L[li].lk; Box c = lay_grid(r2, nl, nl, li, 2);
            if (lcdbtn(0x24u + dd, (int)c.x, (int)c.y, (int)c.w, (int)c.h, L[li].name, darmed == dd)) darmed = dd; }
    } else if (dscreen == DS_KIT) {
        font(FONT_TINY);
        rrectfill((int)gc.x, (int)gc.y, 15, 7, 1, CLR_TRUE_BLUE); print("808", (int)gc.x + 2, (int)gc.y + 1, CLR_WHITE);   // badge
        Box grid = lay_pad(gc, 0, 0, 0, 18);               // full-roster density grid, right of the badge
        float gsw = grid.w / (float)STEPS;
        for (int r = 0; r < TR_NV; r++) { int v = VL[r], gy = (int)grid.y + r;
            for (int s = 0; s < STEPS; s++) { int gx = (int)(grid.x + s * gsw);
                if (s == step && playing) { blend(BLEND_AVG); rectfill(gx - 1, (int)grid.y, (int)gsw, TR_NV, CLR_MEDIUM_GREEN); blend_reset(); }
                if (dgrid[v][s]) rectfill(gx, gy, (int)gsw - 1 < 2 ? 2 : (int)gsw - 1, 1, dmute[v] ? CLR_DARKER_GREY : v == dsel ? CLR_LIGHT_YELLOW : CLR_LIME_GREEN);
            } }
    } else if (dscreen == DS_PAT) {                         // PAT — A-D banks
        static const char *AD[NPAT] = { "A", "B", "C", "D" };
        for (int k = 0; k < NPAT; k++) { Box c = lay_grid(lay_inset(gc, 1), NPAT, NPAT, k, 3);
            pat_pad(0x3Fu + k, (int)c.x, (int)c.y, (int)c.w, (int)c.h, AD[k], M_808, k); }
    } else if (dscreen == DS_PERF) {                        // PERF — ROLL (fill) + ACC (accent all), per machine
        draw_drum_perf(gc);
    } else {                                                // DS_VCE — the voice's TONE knobs
        font(FONT_TINY); plabel(TR808_NAME[dsel], (int)(gc.x + gc.w / 2), (int)gc.y, CLR_LIME_GREEN);
        {   // little MUTE toggle for the SELECTED voice, top-right of the header (ease-of-use — no need to reach for the MUT pad-tool)
            int bw = (int)lay_clamp(gc.w * 0.24f, 16, 26), bx = (int)(gc.x + gc.w) - bw, by = (int)gc.y;
            if (lcdbtn(0x8Au, bx, by, bw, 6, dmute[dsel] ? "MUTED" : "MUTE", dmute[dsel])) dmute[dsel] = !dmute[dsel];
        }
        Box vr = lay_pad(gc, 6, 0, 0, 0);
        float *kv[6]; const char *kl[6]; int nk = 0;        // ONE panel: TONE (TUNE/DEC/[char]) + MIX (VOL/PAN/FINE) in a single row
        kv[nk] = &dtune[dsel];  kl[nk++] = "TUNE";
        kv[nk] = &ddecay[dsel]; kl[nk++] = "DEC";
        if (CH8[dsel]) { kv[nk] = &dcolor[dsel]; kl[nk++] = CH8[dsel]; }
        kv[nk] = &dvol[dsel];   kl[nk++] = "VOL";
        kv[nk] = &dpan[dsel];   kl[nk++] = "PAN";
        kv[nk] = &dfine[dsel];  kl[nk++] = "FINE";   // ±50c microtune
        for (int i = 0; i < nk; i++) lcdknob_cell(lay_grid(vr, nk, nk, i, 2), kv[i], kl[i], 0.5f);
    }

    // ④ VOICE PICKER — all 16 voices in one row (acid order), spread across the width.
    float psw = pick.w / (float)TR_NV; int pby = (int)pick.y + 1, pbh = (int)pick.h - 2;   // 1px gap top & bottom → the voice button never touches its cell edge
    for (int r = 0; r < TR_NV; r++) {
        int v = VL[r], x = (int)(pick.x + r * psw), pw = (int)psw - 1, selp = (v == dsel), mtd = dmute[v]; if (pw < 4) pw = 4;
        void *wp = ui_wid_hash(0x90u + v, x, pby, pw, pbh); ui_reg(wp, x, pby, pw, pbh, 0);
        UiCap *c = ui_cap_for(wp); int hot = (c != 0);
        if (c) {
            if (dmut_now) { if (ui_grabbed(wp)) dmute[v] = !dmute[v]; }   // MUT live — a tap mutes, directly (no hold delay)
            else if (ui_grabbed(wp)) {                                    // default: SELECT — audition only when STOPPED (an off-grid hit would smear the running groove; REC is the audible opt-in while playing)
                dsel = v;
                if (drec_now && playing) { dgrid[v][step] = 1; tr808_fire(TR808_BASE, v, 1, 0, dtune, ddecay, dcolor); dtrig[v] = 1; }   // REC lit → punch + hear it
                else if (!playing)       { tr808_fire(TR808_BASE, v, 1, 0, dtune, ddecay, dcolor); dtrig[v] = 1; }
                if (dscreen == DS_GEN || dscreen == DS_PAT || dscreen == DS_PERF || dscreen == DS_KIT)
                    dscreen = DS_VCE;                                     // machine-scoped screens yield to a voice tap; VCE/FLAG stay sticky
            }
        }
        rrectfill(x, pby, pw, pbh, 1, mtd ? CLR_DARKER_PURPLE : selp ? CLR_TRUE_BLUE : CLR_DARK_BLUE);
        if (dtrig[v] > 0) { blend(BLEND_AVG); rrectfill(x, pby, pw, pbh, 1, mtd ? CLR_LIGHT_GREY : CLR_WHITE); blend_reset(); }
        rrect(x, pby, pw, pbh, 1, dmut_now ? CLR_ORANGE : (selp || hot) ? CLR_WHITE : CLR_BROWNISH_BLACK);   // orange rim = MUT live (pads re-targeted)
        if (mtd) line(x + 1, pby + 1, x + pw - 1, pby + pbh - 2, CLR_RED);
        font(FONT_TINY); print(AB8[v], x + (pw - text_width(AB8[v])) / 2, pby + (pbh - 5) / 2, mtd ? CLR_DARKER_GREY : selp ? CLR_WHITE : CLR_BLUE);
    }
    // ④a REC — a dedicated punch-in latch down the far-right edge (spans picker+hits), where
    // the old 4-way tool cycle was. Same tap=latch / hold=momentary grammar as MUT; lit +
    // playing → a voice-pad tap ALSO writes onto the current step (live/overdub record).
    {
        int bx = (int)tool.x, by = (int)tool.y, bw = (int)tool.w - 1, bh = (int)tool.h; if (bw < 5) bw = 5;
        int pr = 0, hot = 0, foc = 0;
        void *w = ui_wid_hash(0xE0u, bx, by, bw, bh);
        int act = ui_button_core(w, bx, by, bw, bh, &foc, &pr, &hot);
        if (pr) drec_hold++;
        else { if (act && drec_hold <= PERF_TAP && tap_settled()) drec_latch = !drec_latch; drec_hold = 0; }
        drec_now = drec_latch || pr;
        rrectfill(bx, by, bw, bh, 2, drec_now ? CLR_RED : CLR_DARK_BROWN);
        rrect(bx, by, bw, bh, 2, drec_latch ? CLR_WHITE : hot ? CLR_WHITE : CLR_BROWNISH_BLACK);
        font(FONT_TINY);
        int ch = 6, ty = by + (bh - 3 * ch) / 2;
        for (int i = 0; i < 3; i++) { char s[2] = { "REC"[i], 0 }; print(s, bx + (bw - text_width(s)) / 2, ty + i * ch, drec_now ? CLR_WHITE : CLR_LIGHT_PEACH); }
    }

    // ⑤ the HITS — the picked voice's 16 steps, on the bottom, spread across the width.
    int hby = (int)hits.y, hbh = (int)hits.h; float hsw = hits.w / (float)STEPS;
    for (int s = 0; s < STEPS; s++) {
        int x = (int)(hits.x + s * hsw), bw = (int)hsw - 1, here = (s == step && playing); if (bw < 4) bw = 4;
        void *ws = ui_wid_hash(0xA0u + s, x, hby, bw, hbh); ui_reg(ws, x, hby, bw, hbh, 0);
        UiCap *c = ui_cap_for(ws);
        if (c) {
            g_drag_frame = ui_frame_ct; g_drag_y = c->cy;
            int fx = c->released ? c->rx : c->cx, cell = (int)((fx - hits.x) / hsw);
            if (cell < 0) cell = 0; if (cell >= STEPS) cell = STEPS - 1;
            if (dscreen != DS_FLAG) {                        // VCE/KIT — paint on/off across the drag
                if (ui_grabbed(ws)) { paint_val = !dgrid[dsel][s]; if (paint_val) { tr808_fire(TR808_BASE, dsel, 1, 0, dtune, ddecay, dcolor); dtrig[dsel] = 1; mbop = 1; } }
                dgrid[dsel][cell] = paint_val;
            } else if (darmed == DD_ACC) {                   // FLAG/ACC — toggle accent, paint across
                if (ui_grabbed(ws)) { paint_val = !dacc[dsel][s]; if (paint_val) dgrid[dsel][s] = 1; }
                dacc[dsel][cell] = paint_val; if (paint_val) dgrid[dsel][cell] = 1;
            } else if (darmed == DD_PROB) {                  // FLAG/PROB — vertical slide = chance, drawn across
                float f = clamp((hby + hbh - 3 - c->cy) / (float)(hbh - 3), 0, 1);
                dprob[dsel][cell] = snap_prob(f); dgrid[dsel][cell] = 1;
            } else {                                         // FLAG/TUN|DEC|CHAR — bipolar p-lock offset, drawn across
                doff[darmed - DD_TUN][dsel][cell] = clamp((hby + hbh / 2 - c->cy) / (float)(hbh / 2), -1, 1); dgrid[dsel][cell] = 1;
            }
        }
        int on2 = dgrid[dsel][s], locklens = (dscreen == DS_FLAG && darmed >= DD_TUN);
        int lp = locklens ? darmed - DD_TUN : 0;
        rrectfill(x, hby, bw, hbh, 1, (here && !on2) ? CLR_LIGHT_GREY : (s % 4 == 0 ? CLR_DARK_PURPLE : CLR_DARKER_PURPLE));   // OFF cell: dark purple, the 4th a bit lighter (playhead pops)
        if (on2 && locklens) {                               // LOCK lens — bipolar bar from centre
            int cy = hby + hbh / 2, mx = hbh / 2 - 1; if (mx < 1) mx = 1;
            int op = (int)(doff[lp][dsel][s] * mx + (doff[lp][dsel][s] >= 0 ? 0.5f : -0.5f));
            line(x, cy, x + bw - 1, cy, CLR_DARKER_GREY);    // centre = the voice's knob
            int col = here ? CLR_WHITE : op ? CLR_LIGHT_YELLOW : CLR_MEDIUM_GREEN;
            if (op > 0)      rrectfill(x, cy - op, bw, op, 1, col);
            else if (op < 0) rrectfill(x, cy + 1, bw, -op, 1, col);
            else             rectfill(x + bw / 3, cy, bw / 3 < 1 ? 1 : bw / 3, 1, col);
        } else if (on2) {
            int pr = dprob[dsel][s] > 0 ? dprob[dsel][s] : 100, fh = hbh * pr / 100; if (fh < 3) fh = 3;
            rrectfill(x, hby + hbh - fh, bw, fh, 1, here ? CLR_WHITE : (s % 4 == 0 ? CLR_BLUE : CLR_TRUE_BLUE));   // ON = blue (other hue vs the purple OFF), the 4th an even lighter sky-blue
            if (dacc[dsel][s]) rectfill(x + 1, hby - 3, bw - 2 < 1 ? 1 : bw - 2, 2, CLR_LIGHT_YELLOW);   // accent marker above
        }
        rrect(x, hby, bw, hbh, 1, here ? CLR_WHITE : CLR_BROWNISH_BLACK);   // white outline on the played column
    }
}

// ── the 909 DRUM face — same compact model as the 808, but its OWN identity:
// AMBER/steel (vs the 808's blue) so you know which drum machine you're on.
static void draw_909(Box stage) {
    // acid order (from acidrack): kick/hats/clap/snare, then rim + ride/crash, then the fill toms.
    static const int VL[TR9_NV] = { TR9_BD, TR9_CH, TR9_OH, TR9_CP, TR9_SD, TR9_RS, TR9_RC, TR9_CC, TR9_LT, TR9_MT, TR9_HT };
    // LANDSCAPE reflow (canvas-density-spectrum.md): design-proportion bands, spread to fill (mirrors 808).
    float H = stage.h, W = stage.w;
    Box body   = lay_inset(stage, 2);
    Box krow   = lay_split(body, EDGE_TOP,    H * 0.22f, &body);            // ② FX row + METAL XY
    Box bottom = lay_split(body, EDGE_BOTTOM, H * 0.36f, &body);            // ④⑤ picker + hits + tool
    Box skcL   = lay_split(body, EDGE_LEFT,   W * 0.11f, &body);            // ③ soft-key columns
    Box skcR   = lay_split(body, EDGE_RIGHT,  W * 0.11f, &body);
    Box lcd    = lay_inset(body, 1);                                                      // the hero glass
    Box tool   = lay_split(bottom, EDGE_RIGHT,  W * 0.05f, &bottom);        // ④a pad TOOL
    Box hits   = lay_split(bottom, EDGE_BOTTOM, bottom.h * 0.60f, &bottom); // ⑤ the 16 hits
    Box pick   = bottom;                                                    // ④ voice picker

    if (rack_view) { float cw = W * 0.11f; krow.x += cw; krow.w -= 2 * cw; }  // iPad: align the FX row + METAL XY to the LCD x-span
    Box mtl = lay_split(krow, EDGE_RIGHT, W * 0.13f, &krow);   // METAL XY in the top-zone right gutter
    draw_fxrow(krow, 3);                                        // ② DIST · SEND · VERB
    {   // 909 METAL-filter XY — the shared highpass on the metal voices (hats/cymbal/cowbell). Drag X=cut Y=res.
        int px0 = (int)mtl.x, py0 = (int)mtl.y, pw = (int)mtl.w, ph = (int)mtl.h - 5;
        void *wp = ui_wid_hash(0xC0u, px0, py0, pw, ph); ui_reg(wp, px0, py0, pw, ph, 0);
        UiCap *c = ui_cap_for(wp);
        if (c) {
            g_drag_frame = ui_frame_ct; g_drag_y = c->cy;
            float nx = clamp((c->cx - px0) / (float)pw, 0, 1), ny = clamp((py0 + ph - c->cy) / (float)ph, 0, 1);
            if (nx != m9cut || ny != m9res) { m9cut = nx; m9res = ny; tr909_metal(D909_BASE, m9cut, m9res); }
        }
        rrectfill(px0, py0, pw, ph, 1, CLR_BROWNISH_BLACK);
        pset(px0 + (int)(m9cut * (pw - 1)), py0 + ph - 1 - (int)(m9res * (ph - 1)), CLR_LIGHT_YELLOW);
        font(FONT_TINY); plabel("MTL", px0 + pw / 2, py0 + ph, CLR_ORANGE);
    }

    // ③ soft-keys flank the LCD, split by SCOPE (mirrors the 808) — LEFT = the selected VOICE
    // (VCE/FLAG + the MUT pad-latch) · RIGHT = the MACHINE (GEN/PAT/PERF/KIT).
    { static const char *L[2] = { "VCE", "FLAG" }; static const int Lm[2] = { DS_VCE, DS_FLAG };   // VCE holds TONE *and* MIX knobs in one panel
      static const unsigned Ls[2] = { 0x22u, 0x21u };
      for (int k = 0; k < 2; k++) { Box c = lay_grid(skcL, 1, 3, k, 2);
          if (cbtn(Ls[k], (int)c.x, (int)c.y, (int)c.w, (int)c.h, L[k], dscreen == Lm[k])) dscreen = Lm[k]; }
      Box mc = lay_grid(skcL, 1, 3, 2, 2);                  // MUT — tap=latch / hold=momentary; pads mute DIRECTLY while live
      dmut_now = clatch(0x23u, (int)mc.x, (int)mc.y, (int)mc.w, (int)mc.h, "MUT", &dmut_latch, &dmut_hold, CLR_ORANGE); }
    { static const char *R[4] = { "GEN", "PAT", "PERF", "KIT" }; static const int Rm[4] = { DS_GEN, DS_PAT, DS_PERF, DS_KIT };
      static const unsigned Rs[4] = { 0x31u, 0x36u, 0x39u, 0x20u };
      for (int k = 0; k < 4; k++) { Box c = lay_grid(skcR, 1, 4, k, 2);
          if (cbtn(Rs[k], (int)c.x, (int)c.y, (int)c.w, (int)c.h, R[k], dscreen == Rm[k])) dscreen = Rm[k]; } }
    rrectfill((int)lcd.x, (int)lcd.y, (int)lcd.w, (int)lcd.h, 3, CLR_BROWNISH_BLACK);
    Box glass = lay_inset(lcd, 2);
    rrectfill((int)glass.x, (int)glass.y, (int)glass.w, (int)glass.h, 2, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = (int)glass.y + 1; y < glass.y + glass.h - 1; y += 2) line((int)glass.x, y, (int)(glass.x + glass.w - 1), y, CLR_BROWNISH_BLACK); blend_reset();
    Box gc = lay_inset(glass, 2);
    if (dscreen == DS_GEN) {                                // CLEAR + randomize (2×2)
        static const char *GN[4] = { "CLEAR", "MIN", "MID", "BUSY" };
        for (int g = 0; g < 4; g++) { Box c = lay_grid(gc, 2, 4, g, 2);
            if (lcdbtn(0x40u + g, (int)c.x, (int)c.y, (int)c.w, (int)c.h, GN[g], 0)) gen_drums9(g); }
    } else if (dscreen == DS_FLAG) {
        if (darmed == DD_CHAR && !CH9[d9sel]) darmed = DD_TUN;   // this voice has no character knob
        Box r2; Box r1 = lay_split(gc, EDGE_TOP, gc.h * 0.5f, &r2);   // row1 = flags (incl. STRK) · row2 = p-locks
        Box a0 = lay_grid(r1, 3, 3, 0, 2), a1 = lay_grid(r1, 3, 3, 1, 2), a2 = lay_grid(r1, 3, 3, 2, 2);
        if (lcdbtn(0x24u + DD_ACC,  (int)a0.x, (int)a0.y, (int)a0.w, (int)a0.h, "ACC",  darmed == DD_ACC))  darmed = DD_ACC;
        if (lcdbtn(0x24u + DD_PROB, (int)a1.x, (int)a1.y, (int)a1.w, (int)a1.h, "PROB", darmed == DD_PROB)) darmed = DD_PROB;
        if (lcdbtn(0x24u + DD_STRK, (int)a2.x, (int)a2.y, (int)a2.w, (int)a2.h, "STRK", darmed == DD_STRK)) darmed = DD_STRK;
        Lane L[LK_N]; int nl = drum_lanes(M_909, d9sel, L);     // the continuous lanes (id-stable)
        for (int li = 0; li < nl; li++) { int dd = DD_TUN + L[li].lk; Box c = lay_grid(r2, nl, nl, li, 2);
            if (lcdbtn(0x24u + dd, (int)c.x, (int)c.y, (int)c.w, (int)c.h, L[li].name, darmed == dd)) darmed = dd; }
    } else if (dscreen == DS_KIT) {
        font(FONT_TINY);
        rrectfill((int)gc.x, (int)gc.y, 15, 7, 1, CLR_ORANGE); print("909", (int)gc.x + 2, (int)gc.y + 1, CLR_BROWNISH_BLACK);   // badge
        Box grid = lay_pad(gc, 0, 0, 0, 18);               // full-roster density grid, right of the badge
        float gsw = grid.w / (float)STEPS;
        for (int r = 0; r < TR9_NV; r++) { int v = VL[r], gy = (int)grid.y + r;
            for (int s = 0; s < STEPS; s++) { int gx = (int)(grid.x + s * gsw);
                if (s == step && playing) { blend(BLEND_AVG); rectfill(gx - 1, (int)grid.y, (int)gsw, TR9_NV, CLR_MEDIUM_GREEN); blend_reset(); }
                if (d9grid[v][s]) rectfill(gx, gy, (int)gsw - 1 < 2 ? 2 : (int)gsw - 1, 1, d9mute[v] ? CLR_DARKER_GREY : v == d9sel ? CLR_LIGHT_YELLOW : CLR_LIME_GREEN);
            } }
    } else if (dscreen == DS_PAT) {                         // PAT — A-D banks
        static const char *AD[NPAT] = { "A", "B", "C", "D" };
        for (int k = 0; k < NPAT; k++) { Box c = lay_grid(lay_inset(gc, 1), NPAT, NPAT, k, 3);
            pat_pad(0x43u + k, (int)c.x, (int)c.y, (int)c.w, (int)c.h, AD[k], M_909, k); }
    } else if (dscreen == DS_PERF) {                        // PERF — ROLL (fill) + ACC (accent all), per machine
        draw_drum_perf(gc);
    } else {                                                // DS_VCE — the voice's TONE knobs
        font(FONT_TINY); plabel(TR909_NAME[d9sel], (int)(gc.x + gc.w / 2), (int)gc.y, CLR_LIME_GREEN);
        {   // little MUTE toggle for the SELECTED voice, top-right of the header (ease-of-use)
            int bw = (int)lay_clamp(gc.w * 0.24f, 16, 26), bx = (int)(gc.x + gc.w) - bw, by = (int)gc.y;
            if (lcdbtn(0x8Bu, bx, by, bw, 6, d9mute[d9sel] ? "MUTED" : "MUTE", d9mute[d9sel])) d9mute[d9sel] = !d9mute[d9sel];
        }
        Box vr = lay_pad(gc, 6, 0, 0, 0);
        float *kv[6]; const char *kl[6]; int nk = 0;        // ONE panel: TONE (TUNE/DEC/[char]) + MIX (VOL/PAN/FINE) in a single row
        kv[nk] = &d9tune[d9sel];  kl[nk++] = "TUNE";
        kv[nk] = &d9decay[d9sel]; kl[nk++] = "DEC";
        if (CH9[d9sel]) { kv[nk] = &d9color[d9sel]; kl[nk++] = CH9[d9sel]; }
        kv[nk] = &d9vol[d9sel];   kl[nk++] = "VOL";
        kv[nk] = &d9pan[d9sel];   kl[nk++] = "PAN";
        kv[nk] = &d9fine[d9sel];  kl[nk++] = "FINE";   // ±50c microtune
        for (int i = 0; i < nk; i++) lcdknob_cell(lay_grid(vr, nk, nk, i, 2), kv[i], kl[i], 0.5f);
    }

    // ④ voice picker — all 11 voices in one row (acid order, amber pads), spread across the width.
    float psw = pick.w / (float)TR9_NV; int pby = (int)pick.y + 1, pbh = (int)pick.h - 2;   // 1px gap top & bottom → the voice button never touches its cell edge
    for (int r = 0; r < TR9_NV; r++) {
        int v = VL[r], x = (int)(pick.x + r * psw), pw = (int)psw - 1, selp = (v == d9sel), mtd = d9mute[v]; if (pw < 4) pw = 4;
        void *wp = ui_wid_hash(0x90u + v, x, pby, pw, pbh); ui_reg(wp, x, pby, pw, pbh, 0);
        UiCap *c = ui_cap_for(wp); int hot = (c != 0);
        if (c) {
            if (dmut_now) { if (ui_grabbed(wp)) d9mute[v] = !d9mute[v]; }   // MUT live — a tap mutes, directly (no hold delay)
            else if (ui_grabbed(wp)) {                                      // default: SELECT — audition only when STOPPED (see the 808 note; REC is the audible opt-in while playing)
                d9sel = v;
                if (drec_now && playing) { d9grid[v][step] = 1; tr909_fire(D909_BASE, v, 1, 0, d9tune, d9decay, d9color); d9trig[v] = 1; }   // REC lit → punch + hear it
                else if (!playing)       { tr909_fire(D909_BASE, v, 1, 0, d9tune, d9decay, d9color); d9trig[v] = 1; }
                if (dscreen == DS_GEN || dscreen == DS_PAT || dscreen == DS_PERF || dscreen == DS_KIT)
                    dscreen = DS_VCE;                                        // machine-scoped screens yield to a voice tap; VCE/FLAG stay sticky
            }
        }
        rrectfill(x, pby, pw, pbh, 1, mtd ? CLR_DARKER_PURPLE : selp ? CLR_ORANGE : CLR_DARK_ORANGE);
        if (d9trig[v] > 0) { blend(BLEND_AVG); rrectfill(x, pby, pw, pbh, 1, mtd ? CLR_LIGHT_GREY : CLR_WHITE); blend_reset(); }
        rrect(x, pby, pw, pbh, 1, dmut_now ? CLR_ORANGE : (selp || hot) ? CLR_WHITE : CLR_BROWNISH_BLACK);   // orange rim = MUT live (pads re-targeted)
        if (mtd) line(x + 1, pby + 1, x + pw - 1, pby + pbh - 2, CLR_RED);
        font(FONT_TINY); print(AB9[v], x + (pw - text_width(AB9[v])) / 2, pby + (pbh - 5) / 2, mtd ? CLR_DARKER_GREY : selp ? CLR_BROWNISH_BLACK : CLR_LIGHT_YELLOW);
    }
    // ④a REC — the punch-in latch on the far-right edge (mirrors the 808).
    {
        int bx = (int)tool.x, by = (int)tool.y, bw = (int)tool.w - 1, bh = (int)tool.h; if (bw < 5) bw = 5;
        int pr = 0, hot = 0, foc = 0;
        void *w = ui_wid_hash(0xE0u, bx, by, bw, bh);
        int act = ui_button_core(w, bx, by, bw, bh, &foc, &pr, &hot);
        if (pr) drec_hold++;
        else { if (act && drec_hold <= PERF_TAP && tap_settled()) drec_latch = !drec_latch; drec_hold = 0; }
        drec_now = drec_latch || pr;
        rrectfill(bx, by, bw, bh, 2, drec_now ? CLR_RED : CLR_DARK_BROWN);
        rrect(bx, by, bw, bh, 2, drec_latch ? CLR_WHITE : hot ? CLR_WHITE : CLR_BROWNISH_BLACK);
        font(FONT_TINY);
        int ch = 6, ty = by + (bh - 3 * ch) / 2;
        for (int i = 0; i < 3; i++) { char s[2] = { "REC"[i], 0 }; print(s, bx + (bw - text_width(s)) / 2, ty + i * ch, drec_now ? CLR_WHITE : CLR_LIGHT_PEACH); }
    }

    // ⑤ the HITS — picked voice's 16 steps, spread across the width; amber, white downbeat accents.
    int hby = (int)hits.y, hbh = (int)hits.h; float hsw = hits.w / (float)STEPS;
    for (int s = 0; s < STEPS; s++) {
        int x = (int)(hits.x + s * hsw), bw = (int)hsw - 1, here = (s == step && playing); if (bw < 4) bw = 4;
        void *ws = ui_wid_hash(0xA0u + s, x, hby, bw, hbh); ui_reg(ws, x, hby, bw, hbh, 0);
        UiCap *c = ui_cap_for(ws);
        if (c) {
            g_drag_frame = ui_frame_ct; g_drag_y = c->cy;
            int fx = c->released ? c->rx : c->cx, cell = (int)((fx - hits.x) / hsw);
            if (cell < 0) cell = 0; if (cell >= STEPS) cell = STEPS - 1;
            if (dscreen != DS_FLAG) {                        // VCE/KIT — paint on/off
                if (ui_grabbed(ws)) { paint_val = !d9grid[d9sel][s]; if (paint_val) { tr909_fire(D909_BASE, d9sel, 1, 0, d9tune, d9decay, d9color); d9trig[d9sel] = 1; mbop = 1; } }
                d9grid[d9sel][cell] = paint_val;
            } else if (darmed == DD_ACC) {                   // FLAG/ACC — toggle accent
                if (ui_grabbed(ws)) { paint_val = !d9acc[d9sel][s]; if (paint_val) d9grid[d9sel][s] = 1; }
                d9acc[d9sel][cell] = paint_val; if (paint_val) d9grid[d9sel][cell] = 1;
            } else if (darmed == DD_PROB) {                  // FLAG/PROB — vertical slide = chance, drawn across
                float f = clamp((hby + hbh - 3 - c->cy) / (float)(hbh - 3), 0, 1);
                d9prob[d9sel][cell] = snap_prob(f); d9grid[d9sel][cell] = 1;
            } else if (darmed == DD_STRK) {                  // FLAG/STRK — cycle the stroke, paint across
                if (ui_grabbed(ws)) { paint_val = (d9strk[d9sel][s] + 1) % TR9_NSTROKE; d9grid[d9sel][s] = 1; }
                d9strk[d9sel][cell] = paint_val; if (paint_val) d9grid[d9sel][cell] = 1;
            } else {                                         // FLAG/TUN|DEC|CHAR — bipolar p-lock offset, drawn across
                d9off[darmed - DD_TUN][d9sel][cell] = clamp((hby + hbh / 2 - c->cy) / (float)(hbh / 2), -1, 1); d9grid[d9sel][cell] = 1;
            }
        }
        int on2 = d9grid[d9sel][s], locklens = (dscreen == DS_FLAG && darmed >= DD_TUN);
        int lp = locklens ? darmed - DD_TUN : 0;
        rrectfill(x, hby, bw, hbh, 1, (here && !on2) ? CLR_LIGHT_GREY : (s % 4 == 0 ? CLR_DARK_PURPLE : CLR_DARKER_PURPLE));   // OFF cell: dark purple, the 4th a bit lighter (playhead pops)
        if (on2 && locklens) {                               // LOCK lens — bipolar bar from centre
            int cy = hby + hbh / 2, mx = hbh / 2 - 1; if (mx < 1) mx = 1;
            int op = (int)(d9off[lp][d9sel][s] * mx + (d9off[lp][d9sel][s] >= 0 ? 0.5f : -0.5f));
            line(x, cy, x + bw - 1, cy, CLR_DARKER_GREY);
            int col = here ? CLR_WHITE : op ? CLR_LIGHT_YELLOW : CLR_MEDIUM_GREEN;
            if (op > 0)      rrectfill(x, cy - op, bw, op, 1, col);
            else if (op < 0) rrectfill(x, cy + 1, bw, -op, 1, col);
            else             rectfill(x + bw / 3, cy, bw / 3 < 1 ? 1 : bw / 3, 1, col);
        } else if (on2) {
            int pr = d9prob[d9sel][s] > 0 ? d9prob[d9sel][s] : 100, fh = hbh * pr / 100; if (fh < 3) fh = 3;
            rrectfill(x, hby + hbh - fh, bw, fh, 1, here ? CLR_WHITE : (s % 4 == 0 ? CLR_LIGHT_YELLOW : CLR_ORANGE));
            if (d9acc[d9sel][s]) rectfill(x + 1, hby - 3, bw - 2 < 1 ? 1 : bw - 2, 2, CLR_RED);               // accent marker above
            if (d9strk[d9sel][s]) {                                                                          // STROKE glyph — a blue bar cut by BLACK lines (more cuts = busier: flam / drag / ratchet)
                int gx = x + 1, gw = (bw - 2 < 2 ? 2 : bw - 2), gy = hby + 1, nseg = d9strk[d9sel][s] + 1;
                rectfill(gx, gy, gw, 2, CLR_TRUE_BLUE);
                for (int k = 1; k < nseg; k++) { int dx = gx + k * gw / nseg; rectfill(dx, gy, 1, 2, CLR_BROWNISH_BLACK); }
            }
        }
        rrect(x, hby, bw, hbh, 1, here ? CLR_WHITE : CLR_BROWNISH_BLACK);   // white outline on the played column
    }
}

// ── the MST master / mixer face — its own shape (not a voice): master FX +
// a per-machine mix overview + the shared delay. Green identity.
static int machine_active(int m) {
    if (!playing) return 0;
    if (m == M_303A) return on[0][step];
    if (m == M_303B) return on[1][step];
    if (m == M_808) { for (int v = 0; v < TR_NV; v++)  if (dgrid[v][step])  return 1; return 0; }
    if (m == M_909) { for (int v = 0; v < TR9_NV; v++) if (d9grid[v][step]) return 1; return 0; }
    return 0;
}
// The SONG save/load core lives just above init() (it needs apply_fx/pat_io/etc.);
// the MST SONGS page below drives it through these four accessors — so the page
// never touches the SaveBank type/global directly.
#define NSLOT 6
#define SONG_HOLD (PERF_TAP + 26)    // frames a slot must be HELD to commit a save (~0.6s) — the charge-up bar fills across PERF_TAP..SONG_HOLD
static int  song_slot_used(int i);   // does slot i hold a saved song?
static int  song_slot_cur(void);     // last loaded/saved slot (-1 = none) — for the highlight
static void song_save_slot(int i);   // snapshot the live rack INTO slot i (overwrites)
static void song_load_slot(int i);   // restore slot i into the live rack

static void draw_mst(Box stage) {
    static const char *MLAB[4] = { "303a", "303b", "808", "909" };
    static const char *DL[4]   = { "1/16", "1/8", "DOT", "1/4" };
    // LANDSCAPE reflow (canvas-density-spectrum.md): design-proportion bands, spread to fill.
    float H = stage.h, W = stage.w;
    Box body   = lay_inset(stage, 2);
    Box volrow = lay_split(body, EDGE_TOP,    H * 0.06f, &body);   // ①b per-machine vol sliders
    Box rcol   = lay_split(body, EDGE_RIGHT,  W * 0.11f, &body);   // ③b RIGHT soft-key margin (GEN + room); SWG → knob row, TEMPO → bottom
    Box krow   = lay_split(body, EDGE_TOP,    H * 0.24f, &body);   // ② master knobs (now LEFT of rcol → no PUMP↔SWG clash)
    Box bottom = lay_split(body, EDGE_BOTTOM, H * 0.20f, &body);   // ⑤ TEMPO + SEND (one row now — DELAY moved to the right side-column)
    Box skcL   = lay_split(body, EDGE_LEFT,   W * 0.11f, &body);   // ③ soft-keys (band reclaimed above → tall enough for the labels)
    Box lcd    = lay_inset(body, 1);                               // the hero glass

    // ①b per-machine VOLUME sliders — aligned UNDER each machine's nav cartridge tab, so the pink
    // slider sits below the pink 303a tab, etc. Same geometry as navspine (skip the PLAY/HOME
    // bookends, then M_N cartridge cells — machines are cells 0..3). Drag L/R; fill = level, tinted.
    Box crun = lay_inset(box(stage.x, volrow.y, stage.w, volrow.h), 1);
    { Box pc = lay_split(crun, EDGE_LEFT,  lay_clamp(FU * 0.8f, 12, 24), &crun);   // skip PLAY
      Box hc = lay_split(crun, EDGE_RIGHT, lay_clamp(FU * 0.7f, 11, 22), &crun);   // skip HOME
      (void)pc; (void)hc; }
    for (int m = 0; m < 4; m++) {
        Box c = lay_grid(crun, M_N, M_N, m, 2);   // cartridge cell m = machine m's tab column
        int sx = (int)c.x, sy = (int)c.y, sw = (int)c.w, sh = (int)c.h; if (sh < 3) sh = 3;
        void *w = ui_wid_hash(0xF0u + m, sx, sy, sw, sh); ui_reg(w, sx, sy, sw, sh, 0);
        UiCap *cc = ui_cap_for(w);
        if (cc) { g_drag_frame = ui_frame_ct; g_drag_y = cc->cy;
                  int fxp = cc->released ? cc->rx : cc->cx; level[m] = clamp((fxp - sx) / (float)(sw - 1), 0, 1); }
        rrectfill(sx, sy, sw, sh, 1, CLR_BROWNISH_BLACK);
        int lw = (int)(level[m] * (sw - 2) + 0.5f);
        if (lw > 0) rectfill(sx + 1, sy + 1, lw, sh - 2, mac[m].mute ? CLR_DARKER_GREY : mac[m].col);
    }

    // ② master live knobs — GLU/FLT/RES/FB/PUMP + SWG (the rack-wide swing joins the master row)
    knob_cell(lay_grid(krow, 6, 6, 0, 2), &mglu,    "GLU",  0.30f);
    knob_cell(lay_grid(krow, 6, 6, 1, 2), &mflt,    "FLT",  0.50f);
    knob_cell(lay_grid(krow, 6, 6, 2, 2), &mfres,   "RES",  0.35f);
    knob_cell(lay_grid(krow, 6, 6, 3, 2), &mfb,     "FB",   0.35f);
    knob_cell(lay_grid(krow, 6, 6, 4, 2), &mpump,   "PUMP", 0.0f);
    knob_cell(lay_grid(krow, 6, 6, 5, 2), &g_swing, "SWG",  0.0f);

    // ③ soft-keys — LEFT margin: the 4 LCD views (MIX + the PCF/CRUSH/GATE lanes).
    { static const char *L[4] = { "MIX", "PCF", "CRU", "GAT" };
      for (int k = 0; k < 4; k++) { Box c = lay_grid(skcL, 1, 4, k, 2);
          if (cbtn(0x20u + k, (int)c.x, (int)c.y, (int)c.w, (int)c.h, L[k], mstflow == k)) mstflow = k; } }
    // ③b RIGHT margin — the SIDE-BUTTON column: a BIG GEN, a little SAVE, then the DELAY divisions (compact = less prominent)
    { Box gcell = lay_split_gap(rcol, EDGE_TOP, rcol.h * 0.30f, 1, &rcol);   // 1px gaps (lay_split_gap) so GEN/SAVE/ratios all sit apart evenly
      if (cbtn(0x24u, (int)gcell.x, (int)gcell.y, (int)gcell.w, (int)gcell.h, "GEN", mstflow == 4)) mstflow = 4; }
    { Box scell = lay_split_gap(rcol, EDGE_TOP, rcol.h * 0.22f, 1, &rcol);   // SONG — opens the SONGS page in the LCD (tap a slot = load · hold = save). The deferred SONG layer, now wired.
      if (cbtn(0x26u, (int)scell.x, (int)scell.y, (int)scell.w, (int)scell.h, "SONG", mstflow == 5)) mstflow = (mstflow == 5) ? 0 : 5; }
    for (int i = 0; i < 4; i++) { Box c = lay_grid(rcol, 1, 4, i, 1);   // DELAY divisions — small, in the bottom of the column
        if (cbtn(0x04u + i, (int)c.x, (int)c.y, (int)c.w, (int)c.h, DL[i], mdiv == i)) mdiv = i; }
    rrectfill((int)lcd.x, (int)lcd.y, (int)lcd.w, (int)lcd.h, 3, CLR_BROWNISH_BLACK);
    Box glass = lay_inset(lcd, 2);
    rrectfill((int)glass.x, (int)glass.y, (int)glass.w, (int)glass.h, 2, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = (int)glass.y + 1; y < glass.y + glass.h - 1; y += 2) line((int)glass.x, y, (int)(glass.x + glass.w - 1), y, CLR_BROWNISH_BLACK); blend_reset();
    Box gc = lay_inset(glass, 2);
    font(FONT_TINY);
    if (mstflow == 0) {
        // MIX — the 4 channel LEVEL faders (drag up/down). Fill = level; bright green when the
        // channel fires, dim grey when muted. The ONE place per-machine levels live.
        float fsw = gc.w / 4.0f; int fy = (int)gc.y, fh = (int)gc.h;
        for (int m = 0; m < 4; m++) {
            int cx = (int)(gc.x + m * fsw), fw = (int)fsw - 3; if (fw < 4) fw = 4;
            int muted = mac[m].mute, act = machine_active(m);
            void *w = ui_wid_hash(0xD0u + m, cx, fy, fw, fh); ui_reg(w, cx, fy, fw, fh, 0);
            UiCap *c = ui_cap_for(w);
            if (c) { g_drag_frame = ui_frame_ct; g_drag_y = c->cy; int fyv = c->released ? c->ry : c->cy; level[m] = clamp((fy + fh - 1 - fyv) / (float)(fh - 1), 0, 1); }
            int lv = (int)(level[m] * (fh - 2) + 0.5f);
            rectfill(cx, fy, fw, fh, CLR_BROWNISH_BLACK);
            int col = muted ? CLR_DARKER_GREY : act ? CLR_LIME_GREEN : mac[m].col;
            if (lv > 0) rectfill(cx + 1, fy + fh - 1 - lv, fw - 2, lv, col);
            print(MLAB[m], cx + (fw - text_width(MLAB[m])) / 2, fy + fh - 6, CLR_LIGHT_YELLOW);
        }
    } else if (mstflow == 4) {
        // GEN — whole-rack genre presets. One tap fills both 303s + a kit + tempo/swing/key.
        for (int i = 0; i < SNG_N; i++) { Box c = lay_grid(gc, 2, 6, i, 2);
            if (lcdbtn(0x60u + i, (int)c.x, (int)c.y, (int)c.w, (int)c.h, SNG_NAME[i], 0)) gen_song(i); }
    } else if (mstflow == 5) {
        // SONGS — six whole-rack song slots. TAP a slot = load · HOLD = save; an
        // OCCUPIED slot asks X/OK before it overwrites. Autosave keeps the live rack,
        // so loading never loses your working take (the "6 songs in master" layer).
        static int shf[NSLOT] = { 0 };     // per-tile hold-frame counter (tap vs hold, like the PERF lenses)
        static int confirm = -1;           // slot showing the X/OK overwrite guard (-1 = none)
        int cur = song_slot_cur();
        Box hint = lay_split(gc, EDGE_BOTTOM, 6, &gc);        // reserve the bottom for the hint line
        for (int i = 0; i < NSLOT; i++) {
            Box c = lay_grid(gc, 3, NSLOT, i, 2);            // 3 cols × 2 rows
            int x = (int)c.x, y = (int)c.y, w = (int)c.w, h = (int)c.h;
            int used = song_slot_used(i);
            char n[2] = { (char)('1' + i), 0 };
            if (confirm == i) {                               // this slot is asking before overwrite
                if (lcdbtn(0xB0u + i, x, y, w / 2 - 1, h, "X", 0))  confirm = -1;                       // cancel
                if (lcdbtn(0xB8u + i, x + w / 2, y, w - w / 2, h, "OK", 0)) { song_save_slot(i); confirm = -1; }  // overwrite
                continue;
            }
            if (confirm >= 0) {                               // another slot is confirming → this one is inert
                rrect(x, y, w, h, 2, CLR_MEDIUM_GREEN);
                print(n, x + 3, y + 2, CLR_MEDIUM_GREEN);
                if (used) circfill(x + w - 4, y + 4, 1, CLR_MEDIUM_GREEN);
                continue;
            }
            void *wid = ui_wid_hash(0x68u + i, x, y, w, h);
            int pr = 0, hot = 0, foc = 0;
            int act = ui_button_core(wid, x, y, w, h, &foc, &pr, &hot);
            if (pr) shf[i]++;
            else {
                if (act) {                                       // fire on RELEASE (so a full charge, then lift)
                    if (shf[i] >= SONG_HOLD) { if (used) confirm = i; else song_save_slot(i); }  // charged to full → save (occupied asks first)
                    else if (shf[i] <= PERF_TAP && used) song_load_slot(i);                      // quick tap → load (mid-hold release = abort)
                }
                shf[i] = 0;
            }
            int holding = pr && shf[i] > PERF_TAP;               // past the tap window → charging a save
            int ready   = pr && shf[i] >= SONG_HOLD;             // bar full → release to save
            // charge-up: fill the tile left→right as you hold, so you SEE the save coming
            if (hot && !pr) rrectfill(x, y, w, h, 2, CLR_MEDIUM_GREEN);
            if (holding) {
                float p = clamp((shf[i] - PERF_TAP) / (float)(SONG_HOLD - PERF_TAP), 0, 1);
                int bw = (int)((w - 2) * p + 0.5f);
                if (bw > 0) rectfill(x + 1, y + 1, bw, h - 2, ready ? CLR_LIGHT_YELLOW : CLR_MEDIUM_GREEN);
            }
            rrect(x, y, w, h, 2, (i == cur) ? CLR_WHITE : ready ? CLR_LIGHT_YELLOW : CLR_LIME_GREEN);
            print(n, x + 3, y + 2, ready ? CLR_DARK_GREEN : CLR_LIME_GREEN);
            if (used) circfill(x + w - 4, y + 4, 1, (i == cur) ? CLR_WHITE : CLR_LIME_GREEN);
        }
        print("tap load  hold save", (int)hint.x, (int)hint.y, CLR_MEDIUM_GREEN);
    } else {
        // PCF / CRUSH / GATE — a drawable 16-step master lane (mstflow 1/2/3), spread across the glass.
        // PCF = tone (green), CRUSH = texture (orange), GATE = chop (pink); full bar = no effect.
        int *lane = (mstflow == 1) ? mpcf : (mstflow == 2) ? mcrush : mgate;
        unsigned seed = (mstflow == 1) ? 0xC0u : (mstflow == 2) ? 0xE0u : 0xA0u;
        int dflt = (mstflow == 2) ? 0 : 7;                   // the lane's no-effect value (CRUSH clean = 0; PCF/GATE open = 7)
        int drawn = 0; for (int s = 0; s < STEPS; s++) if (lane[s] != dflt) { drawn = 1; break; }
        // CLR = a small TOP-RIGHT CORNER button (only when the lane is drawn). It OVERLAYS the lane
        // instead of stealing a header row; the columns beneath it clip their capture BELOW it so a
        // CLR tap can never also draw a step. Drawn after the loop; wipes the lane back to default.
        int clrw = drawn ? (int)lay_clamp(gc.w * 0.16f, 14, 22) : 0, clrh = 7;
        int clrx = (int)(gc.x + gc.w) - clrw, clry = (int)gc.y;
        float lsw = gc.w / (float)STEPS; int ly = (int)gc.y, lh = (int)gc.h;
        for (int s = 0; s < STEPS; s++) {
            int cx = (int)(gc.x + s * lsw), lw = (int)lsw - 1; if (lw < 2) lw = 2;
            int cy = ly, chh = lh;
            if (drawn && cx + lw > clrx) { cy = clry + clrh; chh = ly + lh - cy; }   // column under CLR → capture only below it (value still maps to full ly/lh)
            void *w = ui_wid_hash(seed + s, cx, cy, lw, chh); ui_reg(w, cx, cy, lw, chh, 0);
            UiCap *c = ui_cap_for(w);
            if (c) {                                         // the captured cell tracks the finger → draw the curve
                g_drag_frame = ui_frame_ct; g_drag_y = c->cy;
                int fx = c->released ? c->rx : c->cx, fy = c->released ? c->ry : c->cy;
                int cell = (int)((fx - gc.x) / lsw); if (cell < 0) cell = 0; if (cell >= STEPS) cell = STEPS - 1;
                lane[cell] = (int)(clamp((ly + lh - 1 - fy) / (float)(lh - 1), 0, 1) * 7 + 0.5f);
            }
            if (s == step && playing) { blend(BLEND_AVG); rectfill(cx, ly, lw, lh, CLR_MEDIUM_GREEN); blend_reset(); }
            int fh = lh * lane[s] / 7;
            if (mstflow == 2) {                              // CRUSH — orange; floor tick when clean
                if (fh > 0) rectfill(cx, ly + lh - fh, lw, fh, CLR_ORANGE);
                else        pset(cx, ly + lh - 1, CLR_DARK_BROWN);
            } else {                                         // PCF green · GATE pink
                int col = (mstflow == 1) ? (lane[s] < 7 ? CLR_LIME_GREEN : CLR_DARK_GREEN)
                                         : (lane[s] < 7 ? CLR_PINK : CLR_DARK_BROWN);
                rectfill(cx, ly + lh - fh, lw, fh, col);
            }
        }
        if (drawn && lcdbtn(0x90u, clrx, clry, clrw, clrh, "CLR", 0))
            for (int s = 0; s < STEPS; s++) lane[s] = dflt;   // one-tap wipe back to no-effect
    }

    // ④ the bottom band: TEMPO (left gutter) + the per-machine SEND knobs — ONE row, all at the same y
    Box tc = lay_split(bottom, rack_view ? EDGE_RIGHT : EDGE_LEFT, W * 0.13f, &bottom);  // TEMPO gutter — iPad: on the RIGHT (under SWG, next to the swing knob); phone: left. Sends fill the rest
    // (tempo g_bpm sync moved to update() so it runs in ALL modes/faces, not just when this face is drawn)
    for (int m = 0; m < 4; m++) { Box c = lay_grid(bottom, 4, 4, m, 2);    // ⑤ per-machine delay SEND (knob label = the machine)
        float rh = c.h * 0.34f, rw = c.w * 0.42f; int r = (int)lay_clamp(rh < rw ? rh : rw, 4, 9);
        knob(&msend[m], (int)(c.x + c.w / 2), (int)(c.y + r + 1), r, MLAB[m], m < 2 ? 0.10f : 0.0f); }
    {   // TEMPO gknob — SAME y as the SEND knobs (aligned bottom row); the value IS the BPM readout
        char b[4]; int bi = (int)g_bpm, ni = 0;
        if (bi >= 100) b[ni++] = '0' + bi / 100;
        b[ni++] = '0' + (bi / 10) % 10; b[ni++] = '0' + bi % 10; b[ni] = 0;
        float rh = bottom.h * 0.34f, rw = tc.w * 0.42f; int r = (int)lay_clamp(rh < rw ? rh : rw, 4, 9);
        gknob(&bpm01, (int)(tc.x + tc.w / 2), (int)(bottom.y + r + 1), r, b);
    }
}

// ── save / load: whole-rack SONG snapshots (the "6 songs in master" layer) ───
// A song = a WHOLE-RACK snapshot: every pattern bank of every machine + all the
// sound/FX/transport values. View state, transport runtime and the momentary
// PERF lenses stay OUT (they aren't the song). save_bytes is ONE unnamed blob
// per cart, so all six named slots + a rolling autosave live in ONE SaveBank
// file. Versioned MAGIC: a format bump IGNORES old files (fall back to the
// generated default) rather than loading a rotten struct. STEP 1 = the core +
// autosave; the six-slot picker UI lands on top of this (the dead SAVE button).
// (NSLOT is #defined just above draw_mst so the SONGS page can size its grid.)
#define SAVE_MAGIC 0xACCA0002u   // v2 — added per-machine mutes (mmute[]); v1 saves ignored. Bump on any SaveBlob layout change

typedef struct {                 // the sound-defining half of an Acid — NOT its live handles/change-guards
    float p[ACID_NPARAM];
    int   wave, drvmode, sweep, base, classic, clean;
    float cut_top, drift, echo_send, rev_send;
} AcidSnap;

typedef struct {
    // sequences — every pattern bank of every machine + which bank each has selected
    P303 pat303[2][NPAT];
    P808 pat808[NPAT];
    P909 pat909[NPAT];
    int  curpat[M_N];
    // 303 sound (per line)
    AcidSnap ac[2];
    int  mroot[2], mscale[2], loct[2], kpage[2], plen[2];
    // drum sound (global, per voice)
    float dtune[TR_NV], ddecay[TR_NV], dcolor[TR_NV], dvol[TR_NV], dpan[TR_NV], dfine[TR_NV];
    float d9tune[TR9_NV], d9decay[TR9_NV], d9color[TR9_NV], d9vol[TR9_NV], d9pan[TR9_NV], d9fine[TR9_NV];
    float m9cut, m9res;
    int   mmute[M_N];                // per-MACHINE mute (the cartridge-LED mute); MST's entry is unused
    int   dmute[TR_NV], d9mute[TR9_NV];
    // master fx + transport
    float mglu, mflt, mfres, mfb, mpump, msend[4], fxverb[4], dist8, dist9, level[M_N];
    int   mdiv, mpcf[STEPS], mcrush[STEPS], mgate[STEPS];
    float g_bpm_s, bpm01_s, g_swing_s;
} SaveBlob;

typedef struct {
    unsigned      magic;
    int           cur;             // last active/loaded named slot (UI highlight); -1 = none
    int           autos_used;      // 1 = the rolling autosave holds real state (else boot keeps the generated default)
    unsigned char used[NSLOT];     // 1 = that named slot holds a saved song
    SaveBlob      autos;           // rolling autosave — resume-where-you-left-off, SEPARATE from the six deliberate slots
    SaveBlob      slot[NSLOT];     // the six song slots
} SaveBank;

static SaveBank g_bank;            // in-memory mirror of the save file (BSS — big, do not put on the stack)
static SaveBlob g_scratch;         // dirty-check scratch for the autosave (BSS, not stack)

static void acid_snap_save(AcidSnap *s, Acid *a) {
    for (int i = 0; i < ACID_NPARAM; i++) s->p[i] = a->p[i];
    s->wave = a->wave; s->drvmode = a->drvmode; s->sweep = a->sweep; s->base = a->base;
    s->classic = a->classic; s->clean = a->clean;
    s->cut_top = a->cut_top; s->drift = a->drift; s->echo_send = a->echo_send; s->rev_send = a->rev_send;
}
static void acid_snap_load(Acid *a, const AcidSnap *s) {
    for (int i = 0; i < ACID_NPARAM; i++) a->p[i] = s->p[i];
    a->wave = s->wave; a->drvmode = s->drvmode; a->sweep = s->sweep; a->base = s->base;
    a->classic = s->classic; a->clean = s->clean;
    a->cut_top = s->cut_top; a->drift = s->drift; a->echo_send = s->echo_send; a->rev_send = s->rev_send;
    acid_define(a);   // rebuild the voice from restored params (also resets the change-guards)
}

// live editing arrays → the current bank slot, so an in-progress edit isn't left
// stale in the snapshot (the same flush pat_switch does before it reads a slot).
static void song_flush_live(void) {
    pat_io_303(0, curpat[M_303A], 1);
    pat_io_303(1, curpat[M_303B], 1);
    pat_io_808(curpat[M_808], 1);
    pat_io_909(curpat[M_909], 1);
}

static void song_capture(SaveBlob *b) {
    memset(b, 0, sizeof *b);       // zero padding too, so memcmp dirty-checks are stable
    song_flush_live();
    memcpy(b->pat303, pat303, sizeof pat303);
    memcpy(b->pat808, pat808, sizeof pat808);
    memcpy(b->pat909, pat909, sizeof pat909);
    memcpy(b->curpat, curpat, sizeof curpat);
    for (int i = 0; i < 2; i++) acid_snap_save(&b->ac[i], &ac[i]);
    memcpy(b->mroot, mroot, sizeof mroot);   memcpy(b->mscale, mscale, sizeof mscale);
    memcpy(b->loct, loct, sizeof loct);      memcpy(b->kpage, kpage, sizeof kpage);
    memcpy(b->plen, plen, sizeof plen);
    memcpy(b->dtune, dtune, sizeof dtune);   memcpy(b->ddecay, ddecay, sizeof ddecay);
    memcpy(b->dcolor, dcolor, sizeof dcolor);memcpy(b->dvol, dvol, sizeof dvol);
    memcpy(b->dpan, dpan, sizeof dpan);      memcpy(b->dfine, dfine, sizeof dfine);
    memcpy(b->d9tune, d9tune, sizeof d9tune);memcpy(b->d9decay, d9decay, sizeof d9decay);
    memcpy(b->d9color, d9color, sizeof d9color);memcpy(b->d9vol, d9vol, sizeof d9vol);
    memcpy(b->d9pan, d9pan, sizeof d9pan);   memcpy(b->d9fine, d9fine, sizeof d9fine);
    b->m9cut = m9cut; b->m9res = m9res;
    for (int m = 0; m < M_N; m++) b->mmute[m] = mac[m].mute;
    memcpy(b->dmute, dmute, sizeof dmute);   memcpy(b->d9mute, d9mute, sizeof d9mute);
    b->mglu = mglu; b->mflt = mflt; b->mfres = mfres; b->mfb = mfb; b->mpump = mpump;
    memcpy(b->msend, msend, sizeof msend);   memcpy(b->fxverb, fxverb, sizeof fxverb);
    b->dist8 = dist8; b->dist9 = dist9; memcpy(b->level, level, sizeof level);
    b->mdiv = mdiv;
    memcpy(b->mpcf, mpcf, sizeof mpcf); memcpy(b->mcrush, mcrush, sizeof mcrush); memcpy(b->mgate, mgate, sizeof mgate);
    b->g_bpm_s = g_bpm; b->bpm01_s = bpm01; b->g_swing_s = g_swing;
}

static void song_restore(const SaveBlob *b) {
    memcpy(pat303, b->pat303, sizeof pat303);
    memcpy(pat808, b->pat808, sizeof pat808);
    memcpy(pat909, b->pat909, sizeof pat909);
    memcpy(curpat, b->curpat, sizeof curpat);
    for (int m = 0; m < M_N; m++) armpat[m] = -1;      // drop any queued bank switches
    for (int i = 0; i < 2; i++) acid_snap_load(&ac[i], &b->ac[i]);
    memcpy(mroot, b->mroot, sizeof mroot);   memcpy(mscale, b->mscale, sizeof mscale);
    memcpy(loct, b->loct, sizeof loct);      memcpy(kpage, b->kpage, sizeof kpage);
    memcpy(plen, b->plen, sizeof plen);
    memcpy(dtune, b->dtune, sizeof dtune);   memcpy(ddecay, b->ddecay, sizeof ddecay);
    memcpy(dcolor, b->dcolor, sizeof dcolor);memcpy(dvol, b->dvol, sizeof dvol);
    memcpy(dpan, b->dpan, sizeof dpan);      memcpy(dfine, b->dfine, sizeof dfine);
    memcpy(d9tune, b->d9tune, sizeof d9tune);memcpy(d9decay, b->d9decay, sizeof d9decay);
    memcpy(d9color, b->d9color, sizeof d9color);memcpy(d9vol, b->d9vol, sizeof d9vol);
    memcpy(d9pan, b->d9pan, sizeof d9pan);   memcpy(d9fine, b->d9fine, sizeof d9fine);
    m9cut = b->m9cut; m9res = b->m9res;
    for (int m = 0; m < M_N; m++) mac[m].mute = b->mmute[m];
    memcpy(dmute, b->dmute, sizeof dmute);   memcpy(d9mute, b->d9mute, sizeof d9mute);
    mglu = b->mglu; mflt = b->mflt; mfres = b->mfres; mfb = b->mfb; mpump = b->mpump;
    memcpy(msend, b->msend, sizeof msend);   memcpy(fxverb, b->fxverb, sizeof fxverb);
    dist8 = b->dist8; dist9 = b->dist9; memcpy(level, b->level, sizeof level);
    mdiv = b->mdiv;
    memcpy(mpcf, b->mpcf, sizeof mpcf); memcpy(mcrush, b->mcrush, sizeof mcrush); memcpy(mgate, b->mgate, sizeof mgate);
    g_bpm = b->g_bpm_s; bpm01 = b->bpm01_s; g_swing = b->g_swing_s;
    // re-sync the LIVE editing arrays from the restored current banks
    pat_io_303(0, curpat[M_303A], 0);
    pat_io_303(1, curpat[M_303B], 0);
    pat_io_808(curpat[M_808], 0);
    pat_io_909(curpat[M_909], 0);
    // force the queued set-and-hold controls to re-push (the *last[] dedup caches are derived)
    for (int v = 0; v < TR_NV;  v++) { dpanlast[v]  = -9; dtunefine[v]  = -9; }
    for (int v = 0; v < TR9_NV; v++) { d9panlast[v] = -9; d9tunefine[v] = -9; }
    tr909_metal(D909_BASE, m9cut, m9res);   // metal filter is set-and-hold
    bpm((int)g_bpm);
    apply_fx();                             // effects are set-and-hold — re-engage the restored values
}

static void bank_write(void) { g_bank.magic = SAVE_MAGIC; save_bytes(&g_bank, sizeof g_bank); }

static int bank_load(void) {   // 1 = a valid bank was read into g_bank; 0 = none (g_bank left empty)
    int n = load_bytes(&g_bank, sizeof g_bank);
    if (n != (int)sizeof g_bank || g_bank.magic != SAVE_MAGIC) {
        memset(&g_bank, 0, sizeof g_bank); g_bank.cur = -1;
        return 0;
    }
    return 1;
}

// autosave: capture the live rack ~3×/s, and when it actually CHANGED, debounce a
// file write. No per-edit-site wiring (the change-detect replaces mark_dirty()).
static int save_cooldown = 0;
static void autosave_tick(void) {
    static unsigned tick = 0;
    if ((tick++ % 20) == 0) {                       // cheap change-detect a few times a second
        song_capture(&g_scratch);
        if (memcmp(&g_scratch, &g_bank.autos, sizeof g_scratch) != 0) {
            memcpy(&g_bank.autos, &g_scratch, sizeof g_scratch);
            g_bank.autos_used = 1;
            save_cooldown = 45;                     // debounce: write ~0.75s after the last change
        }
    }
    if (save_cooldown > 0 && --save_cooldown == 0) bank_write();
}

// ── the four accessors the MST SONGS page drives (forward-declared above draw_mst) ──
static int  song_slot_used(int i) { return i >= 0 && i < NSLOT && g_bank.used[i]; }
static int  song_slot_cur(void)   { return g_bank.cur; }
static void song_save_slot(int i) {
    if (i < 0 || i >= NSLOT) return;
    song_capture(&g_bank.slot[i]); g_bank.used[i] = 1; g_bank.cur = i; bank_write();
}
static void song_load_slot(int i) {
    if (i < 0 || i >= NSLOT || !g_bank.used[i]) return;
    song_restore(&g_bank.slot[i]); g_bank.cur = i; bank_write();
}

void init(void) {
    bpm((int)g_bpm);
    acid_init(&ac[0], 6, 36);                                          // 303a — the bass line (+ octave-down sub on slot 36)
    acid_init(&ac[1], 7, 37);                                          // 303b — an octave up = the acid lead (+ sub on 37)
    ac[1].base = 48;
    rnd_seed(1);   // PIN the boot pattern: seed the gen stream to a fixed value so the default
                   // beat/lines are REPRODUCIBLE (same every run/build), not "whatever the ambient
                   // RNG happened to be". The GEN soft-key re-gens WITHOUT reseeding → stays varied.
    for (int i = 0; i < 2; i++) {
        ac[i].p[ACID_CUT] = 0.55f; ac[i].p[ACID_RES] = 0.70f; ac[i].p[ACID_ENV] = 0.55f;
        ac[i].p[ACID_DEC] = 0.45f; ac[i].p[ACID_ACC] = 0.55f;
        acid_define(&ac[i]);
        gen_line(i, 2);
    }
    tr808_build(TR808_BASE);                               // the shared, honest 808 kit (slots 9..22)
    for (int v = 0; v < TR_NV; v++) { dtune[v] = ddecay[v] = dcolor[v] = dvol[v] = dpan[v] = dfine[v] = 0.5f; dpanlast[v] = dtunefine[v] = -9; }  // 0.5 = neutral; -9 = force first push
    for (int v = 0; v < TR_NV; v++)  for (int s = 0; s < STEPS; s++) dprob[v][s]  = 100;   // every hit certain until loosened
    gen_drums(2);
    tr909_build(D909_BASE);                                // the honest 909 kit (slots 23..35)
    tr909_metal(D909_BASE, m9cut, m9res);
    for (int v = 0; v < TR9_NV; v++) { d9tune[v] = d9decay[v] = d9color[v] = d9vol[v] = d9pan[v] = d9fine[v] = 0.5f; d9panlast[v] = d9tunefine[v] = -9; }  // 0.5 = neutral; -9 = force first push
    for (int v = 0; v < TR9_NV; v++) for (int s = 0; s < STEPS; s++) d9prob[v][s] = 100;
    reverb(0.45f, 0.22f);                                  // the 303s' space (tank 0) — a bright, tighter PLATE (was a warm hall 0.62/0.42): glassier + shorter = the modern acid/house stab sound
    reverb_bus(1, 0.34f, 0.15f);                           // 909 → its own tight bright PLATE (tank 1)
    reverb_bus(2, 0.45f, 0.30f);                           // 808 → its own room (tank 2)
    pan_law(PAN_LINEAR);                                   // fixed rack pan law (the LIN/PWR toggle was removed; LINEAR = engine default)
    // pattern slots are zero-filled = empty, but two fields must NOT be 0 or a fresh slot is UNPLAYABLE:
    // 303 plen 0 → ctr%0 (playhead UB); drum prob 0 → every hit 0% trig. Seed them empty-but-PLAYABLE.
    for (int i = 0; i < 2; i++) for (int s = 0; s < NPAT; s++) pat303[i][s].plen = STEPS;
    for (int s = 0; s < NPAT; s++) for (int v = 0; v < TR_NV;  v++) for (int k = 0; k < STEPS; k++) pat808[s].prob[v][k] = 100;
    for (int s = 0; s < NPAT; s++) for (int v = 0; v < TR9_NV; v++) for (int k = 0; k < STEPS; k++) pat909[s].prob[v][k] = 100;
    for (int s = 0; s < STEPS; s++) mpcf[s] = 7;           // PCF lane starts fully open (no effect)
    for (int s = 0; s < STEPS; s++) mcrush[s] = 0;         // CRUSH lane starts clean (no effect)
    for (int s = 0; s < STEPS; s++) mgate[s]  = 7;         // GATE lane starts fully open (no effect)
    sidechain_key(TR808_BASE + TRS_BD, 0, 1);              // both kicks drive the PUMP
    sidechain_key(D909_BASE + TR9S_BD, 0, 1);
    apply_fx();                                            // engage the default master glue
    // resume the last session from the rolling autosave (else keep the generated
    // default above). Named-slot recall lands with the picker UI (step 2).
    if (bank_load() && g_bank.autos_used) song_restore(&g_bank.autos);
    else { song_capture(&g_bank.autos); g_bank.autos_used = 1; bank_write(); }  // seed the file with the default
}

void update(void) {
    g_bpm = (float)(int)(60 + bpm01 * 140 + 0.5f);                     // TEMPO sync — here (every frame, every mode), NOT in draw_mst (which only runs on the MST tab in phone mode → tempo would desync by layout)
    autosave_tick();                                                   // rolling autosave (resume-where-you-left-off)
    if (mbop > 0) mbop -= 0.08f;
    for (int v = 0; v < TR_NV;  v++) if (dtrig[v]  > 0) dtrig[v]  -= 0.14f;   // drum-pad flash decays
    for (int v = 0; v < TR9_NV; v++) if (d9trig[v] > 0) d9trig[v] -= 0.14f;
    apply_fx();                                                        // master FX (glue/filter/delay/pump)
    for (int i = 0; i < 2; i++) acid_ride(&ac[i]);                     // ride cutoff/reso live on both lines
    // FINE tune: a separate per-voice cents trim (MIX screen) applied via instrument_tune on CHANGE
    // only — the coarse TUNE knob keeps its musical semitone steps; FINE (±0.5 semitone) nulls a beat.
    for (int v = 0; v < TR_NV;  v++) { float fn = dfine[v]  - 0.5f; if (fn != dtunefine[v])  { tr808_tune(TR808_BASE, v, fn); dtunefine[v]  = fn; } }
    for (int v = 0; v < TR9_NV; v++) { float fn = d9fine[v] - 0.5f; if (fn != d9tunefine[v]) { tr909_tune(D909_BASE, v, fn); d9tunefine[v] = fn; } }
    float t = now();                                                   // live-tempo clock: accumulate 16th-note phase so a
    if (g_last_t == 0) g_last_t = t;                                   // bpm change moves the RATE, never JUMPS the counter
    // NB: NO per-frame dt clamp — phase must accumulate REAL elapsed time so the tempo is
    // correct at ANY frame rate (a clamp made the heavy rack run persistently slow < 20 FPS).
    g_phase += (t - g_last_t) * (g_bpm / 60.0f * 4);
    g_last_t = t;
    if (playing) {
        float stepf = g_phase;                                         // 16th-note counter (polymeter), live-tempo
        int   ctr = (int)stepf;                                        // free-running counter (polymeter)
        step = ctr % STEPS;                                            // drums + shared display base
        float frac = stepf - ctr;

        // LIVE bank switch — armed (queued) bank changes LAND on the bar downbeat, so the
        // whole rack flips in time. Runs before the 303/drum blocks read their arrays, so
        // the new pattern plays from step 0. Fires once per bar (ctr flip into step 0).
        if (ctr != laststep && step == 0)
            for (int m = 0; m < M_N; m++) if (armpat[m] >= 0) { pat_switch(m, armpat[m]); armpat[m] = -1; }

        // ── ONE master SWING (g_swing, MST face — ReBirth's model). The 303s and
        //    the drums shuffle by the SAME fraction (up to 0.6 of a 16th) so they
        //    lock, but they get there differently: a 303's held voice can't be
        //    scheduled ahead, so we DELAY its step FLIP on odd 16ths (tb303's
        //    trick); the drums CAN schedule ahead, so they DRAG the onset in ms.
        float sw = g_swing * 0.60f;                                    // 0..0.6 of a 16th

        // 303 lines — each on its OWN swung, PERF-speed-lensed clock. A held speed lens
        // scales this line's phase (×2 / ×0.5); clean 2:1 ratios stay phase-locked to the
        // drums. Swung FLIP: on odd 16ths hold the prior even step until the swung onset
        // (lf >= sw). The other lenses (total-accent, slide-flip) override the READ, so the
        // stored pattern is untouched — release the button and it's back to normal.
        for (int i = 0; i < 2; i++) {
            float spd = pf_half[i] ? 0.5f : 1.0f;                      // PERF speed lens (half-time only) — also halves the ROLL rate below
            // ROLL lens: while held, TAKE OVER this line — retrigger the last played note at a fast
            // subdivision (a stutter/fill). Respects HALF (rolls half as fast when HALF is on).
            // Momentary by nature; the normal clock is skipped so the pattern picks straight back up on release.
            if (pf_roll[i] && !mac[i].mute) {
                int rc = (int)(g_phase * ROLL_RATE * spd);
                if (rc != rollctr[i]) { rollctr[i] = rc; acid_note(&ac[i], roll_pit[i], roll_acc[i], 0); mbop = 1; }
                continue;
            }
            rollctr[i] = -1;                                          // not rolling → reset so the next ROLL starts clean

            float lp  = g_phase * spd;                                 // this line's own phase
            int   lc  = (int)lp; float lf = lp - lc;
            int   lcs = ((lc & 1) && lf < sw) ? lc - 1 : lc;          // swung per-line counter
            if (lcs != laststep303[i]) {
                laststep303[i] = lcs;
                int raw = lcs % plen[i];
                int ls  = pf_rev[i] ? (plen[i] - 1 - raw) : raw;      // REV lens: mirror the step index (play backwards)
                lpos[i] = ls;
                if (mac[i].mute) { acid_off(&ac[i]); continue; }
                int accent = acc[i][ls] || pf_acc[i];                          // total-accent lens
                int slide  = pf_stac[i] ? 0 : pf_glide[i] ? 1 : sld[i][ls];    // slide-flip lens
                int oshift = (pf_oct[i] && (lcs & 1)) ? -12 : 0;               // OCT lens: every OTHER step (odd counter) an octave down — the acid bounce
                if (on[i][ls]) {
                    int midi = ac[i].base + mroot[i] + loct[i] * 12 + pit[i][ls] + oct[i][ls] * 12 + oshift;
                    acid_note(&ac[i], midi, accent, slide); mbop = 1;
                    roll_pit[i] = midi; roll_acc[i] = accent;         // remember the last played note for ROLL to repeat
                }
                else if (tie[i][ls]) acid_tie(&ac[i], slide);         // hold the previous note through
                else acid_off(&ac[i]);
            } else if (!pf_glide[i]) {                                // staccato gate between flips (GLIDE lens sustains → skip entirely)
                int raw = lcs % plen[i];
                int ls  = pf_rev[i] ? (plen[i] - 1 - raw) : raw;
                int slide = pf_stac[i] ? 0 : sld[i][ls];             // STAC forces staccato; else a slid step glides OUT
                if (on[i][ls] && !slide) {                           // gate ONLY a fresh, NON-slid note (tb303's rule): a slid
                    float onset = (lcs & 1) ? sw : 0.0f;             // step must ring on to glide, and a tie/rest holds — cutting
                    int rawn = (lcs + 1) % plen[i];                  // them at 70% was the bug (manual slides never actually glided)
                    int nx = pf_rev[i] ? (plen[i] - 1 - rawn) : rawn;
                    acid_gate(&ac[i], lf, onset, tie[i][nx]);       // (next_ties still spares the cut into a tie)
                }
            }
        }

        // drums — fire at the flip, DRAG the odd 16ths via the fire() ms delay
        // (tempo-scaled: one 16th = 15000/bpm ms, so the feel holds at any BPM).
        if (ctr != laststep) {
            laststep = ctr;
            int swms = (step & 1) ? (int)(sw * (15000.0f / g_bpm)) : 0;   // the same 0..0.6-of-a-16th shuffle, in ms
            int es8 = drum_effstep(0, step);                      // beat-repeat: WHICH pattern column plays (timing stays on `step`)
            for (int v = 0; v < TR_NV; v++) {                     // the 808 line, same transport
                int hit = dgrid[v][es8] && !(dprob[v][es8] < 100 && rnd(100) >= dprob[v][es8]);
                int ghost = 0;
                if (pf_thin[0] && hit && !(es8 % 4 == 0 || dacc[v][es8])) hit = 0;                                            // THIN: keep only downbeats + accents
                if (pf_busy[0] && !hit && (es8 & 1) && v == dsel && v != TR_BD) { hit = ghost = 1; }                          // BUSY: fill the SELECTED voice's off-16ths (add the &s — pick a hat)
                if (!hit) continue;
                dtrig[v] = 1;                                     // would-trigger → the pad pulses even while muted
                if (!mac[M_808].mute && !dmute[v]) {              // ...but only SOUND if not muted
                    int bo = dacc[v][es8] ? 2 : 0; if (pf_dacc[0]) bo += 2; if (ghost) bo -= 1;   // accent lens + quieter ghosts
                    Lane L[LK_N]; int nl = drum_lanes(M_808, v, L); float sv[LK_N];      // per-step lane offsets, around each voice knob
                    for (int p = 0; p < nl; p++) {
                        float eff = clamp(*L[p].knob + L[p].off[es8] * 0.5f, 0, 1);
                        if (L[p].sink == SK_ARG) { sv[p] = *L[p].knob; *L[p].knob = eff; }                        // ride the fire array
                        else if (L[p].sink == SK_VEL) bo += (int)((eff - 0.5f) * 8 + (eff >= 0.5f ? 0.5f : -0.5f)); // VOL → ±4 velocity (0.5 = unity)
                        else if (L[p].sink == SK_PAN && eff != dpanlast[v]) { tr808_pan(TR808_BASE, v, (eff - 0.5f) * 2); dpanlast[v] = eff; }  // PAN → -1..+1; push only on CHANGE (queued set-and-hold)
                    }
                    tr808_fire(TR808_BASE, v, bo, swms, dtune, ddecay, dcolor);
                    for (int p = 0; p < nl; p++) if (L[p].sink == SK_ARG) *L[p].knob = sv[p];
                }
            }
            int es9 = drum_effstep(1, step);
            for (int v = 0; v < TR9_NV; v++) {                    // the 909 line
                int hit = d9grid[v][es9] && !(d9prob[v][es9] < 100 && rnd(100) >= d9prob[v][es9]);
                int ghost = 0;
                if (pf_thin[1] && hit && !(es9 % 4 == 0 || d9acc[v][es9])) hit = 0;
                if (pf_busy[1] && !hit && (es9 & 1) && v == d9sel && v != TR9_BD) { hit = ghost = 1; }                        // BUSY: fill the SELECTED voice's off-16ths
                if (!hit) continue;
                d9trig[v] = 1;
                if (!mac[M_909].mute && !d9mute[v]) {
                    int bo = d9acc[v][es9] ? 2 : 0, st = d9strk[v][es9]; if (pf_dacc[1]) bo += 2; if (ghost) bo -= 1;   // accent lens + quieter ghosts
                    Lane L[LK_N]; int nl = drum_lanes(M_909, v, L); float sv[LK_N];      // per-step lane offsets, around each voice knob
                    for (int p = 0; p < nl; p++) {
                        float eff = clamp(*L[p].knob + L[p].off[es9] * 0.5f, 0, 1);
                        if (L[p].sink == SK_ARG) { sv[p] = *L[p].knob; *L[p].knob = eff; }
                        else if (L[p].sink == SK_VEL) bo += (int)((eff - 0.5f) * 8 + (eff >= 0.5f ? 0.5f : -0.5f)); // VOL → ±4 velocity
                        else if (L[p].sink == SK_PAN && eff != d9panlast[v]) { tr909_pan(D909_BASE, v, (eff - 0.5f) * 2); d9panlast[v] = eff; }  // PAN → -1..+1; push only on CHANGE
                    }
                    if (st && !ghost) tr909_fire_stroke(D909_BASE, v, st, bo, swms, (int)(15000.0f / g_bpm), d9tune, d9decay, d9color);   // one 16th = 15000/bpm ms (113 @132)
                    else    tr909_fire(D909_BASE, v, bo, swms, d9tune, d9decay, d9color);
                    for (int p = 0; p < nl; p++) if (L[p].sink == SK_ARG) *L[p].knob = sv[p];
                }
            }
        }
    } else for (int i = 0; i < 2; i++) acid_off(&ac[i]);
#ifdef DE_TRACE
    watch("face", "%d", face); watch("step", "%d", step); watch("cut", "%d", acid_cut_hz(&ac[0]));
    watch("mute0", "%d", mac[0].mute); watch("mute1", "%d", mac[1].mute);
#endif
}

// ── iPad ROOMY: the full RB-338 rack — all four machines + master at once ────────
// 808 & 909 were built one-at-a-time, so they SHARE dscreen/darmed/dmut_*/drec_*.
// In the rack both draw each frame, so each drum gets its own persisted copy that we
// SWAP into those globals around its draw call (exchange in, draw, exchange back) —
// zero edits inside the 800-line drum functions, and phone mode (one drum at a time)
// is untouched. The 303s (sel[2]/pscreen[2]/kpage[2]) are already independent.
typedef struct { int scr, arm, ml, mh, mn, rl, rh, rn; } DrumUI;
static DrumUI drum_ui[2] = { { DS_VCE, DD_ACC, 0,0,0, 0,0,0 }, { DS_VCE, DD_ACC, 0,0,0, 0,0,0 } };
static void drum_ui_swap(DrumUI *u) {   // exchange the shared drum globals ⇄ this store
    int t;
    t = dscreen;    dscreen    = u->scr; u->scr = t;
    t = darmed;     darmed     = u->arm; u->arm = t;
    t = dmut_latch; dmut_latch = u->ml;  u->ml  = t;
    t = dmut_hold;  dmut_hold  = u->mh;  u->mh  = t;
    t = dmut_now;   dmut_now   = u->mn;  u->mn  = t;
    t = drec_latch; drec_latch = u->rl;  u->rl  = t;
    t = drec_hold;  drec_hold  = u->rh;  u->rh  = t;
    t = drec_now;   drec_now   = u->rn;  u->rn  = t;
}

// ═══ ROOMY: the sticky-focus iPad layout — THE tablet view (the old 2×2 draw_rack is gone) ═══
// Narrow 303a/303b + MST knob-columns bracket ONE big shared SCREEN; the 808/909 sit as pad-bank
// strips at the bottom. Tapping a nameplate FOCUSES that machine's DEEP editor onto the screen
// (r2_focus); play stays live for ALL machines regardless of focus. rack_view: 0 = phone · nonzero
// = this ROOMY view. Design: docs/design/acidcandy-ipad-layout.md.
// roomy chassis THEME — the header toggle (r2_dark) flips between the salmon candy skin and a dark
// one; the ink tones flip with it so labels stay legible on either background.
#define R2_INK  (r2_dark ? CLR_LIGHT_GREY  : CLR_BROWNISH_BLACK)   // primary text/label ink
#define R2_DIM  (r2_dark ? CLR_MEDIUM_GREY : CLR_DARK_BROWN)       // dim label ink
#define R2_PNL  (r2_dark ? CLR_DARKER_GREY : CLR_LIGHT_PEACH)      // panel/chassis fill (salmon ⇄ dark)

// the ROOMY knob look the maker prefers (from the mockup): just a black disc + a coloured outline +
// a white tick — NOT the candy bevel rotary. Same drag FEEL as knob() though (gear/fine pull-out,
// double-tap = default, wheel), so it's the exact same interaction under a cleaner face.
static void r2_knob(float *v, int cx, int cy, int r, const char *label, float def, int accent) {
    ui_reg(v, cx - r, cy - r, 2 * r + 1, 2 * r + 1, 0);
    UiCap *c = ui_cap_for(v);
    if (c) { g_drag_frame = ui_frame_ct; g_drag_y = c->cy; }
    int mi = kmeta_i(v), held = c != 0;
    if (ui_grabbed(v)) { kmeta[mi].gval = *v; kmeta[mi].gt = now(); }
    if (c) {
        if (!c->has_v0) { c->has_v0 = 1; c->v0 = *v; c->by = c->cy; }
        int py = c->released ? c->ry : c->cy, px = c->released ? c->rx : c->cx;
        int ox = px - cx; if (ox < 0) ox = -ox;
        float gear = 1.0f + ox / KNOB_GEAR; if (gear > KNOB_GEARMAX) gear = KNOB_GEARMAX;
        *v = clamp(c->v0 + (c->by - py) / (KNOB_SWEEP * gear), 0, 1);
        c->v0 = *v; c->by = py;
    }
    if (ui_released(v)) {
        float rt = now(), dv = *v - kmeta[mi].gval; if (dv < 0) dv = -dv;
        if (dv < 0.02f && rt - kmeta[mi].gt < 0.25f) {
            if (rt - kmeta[mi].ltt < 0.35f) { *v = def; kmeta[mi].ltt = -9; } else kmeta[mi].ltt = rt;
        }
    }
    int hot = held || ui_hover(cx - r, cy - r, 2 * r + 1, 2 * r + 1);
    if (!c && hot && mouse_wheel() != 0) *v = clamp(*v + mouse_wheel() * 0.04f, 0, 1);
    float ang = 150 + *v * 240;
    int oc = (held || hot) ? CLR_WHITE : accent;
    circfill(cx, cy, r, oc);              // 2px-thick ring, done as fill-and-punch so it's SOLID
    circfill(cx, cy, r - 2, CLR_BLACK);   // (two circ() outlines leave holes) — punch the black face
    line(cx, cy, cx + (int)dx(r - 1, ang), cy + (int)dy(r - 1, ang), CLR_WHITE);
    font(FONT_TINY);
    if (held) { int p = (int)(*v * 99 + 0.5f); char b[3] = { (char)('0' + p / 10), (char)('0' + p % 10), 0 }; plabel(b, cx, cy + r + 1, accent); }
    else plabel(label, cx, cy + r + 1, R2_DIM);
}
// centre an r2_knob in a cell, label below — the r2 twin of knob_cell. Small discs (r≤10) so the
// knob strips stay compact and the pattern screen gets the room.
static void r2_kcell(Box c, float *v, const char *lab, float def, int accent) {
    float rh = c.h * 0.34f, rw = c.w * 0.44f;
    int r = (int)lay_clamp(rh < rw ? rh : rw, 4, 10);
    int cy = (int)(c.y + r + 1); if (cy + r + 7 > (int)(c.y + c.h)) cy = (int)(c.y + c.h) - r - 7;
    r2_knob(v, (int)(c.x + c.w / 2), cy, r, lab, def, accent);
}

// header nameplate: tap the body = FOCUS this machine onto the shared screen; the corner LED = MUTE.
static void r2_header(Box hd, int m) {
    int x = (int)hd.x, y = (int)hd.y, w = (int)hd.w, h = (int)hd.h;
    int foc = (r2_focus == m), live = !mac[m].mute, ledW = (m == M_MST) ? 0 : 10;
    int prf = 0, hotf = 0, fof = 0, prm = 0, hotm = 0, fom = 0;
    void *wf = ui_wid_hash(0x90u + m, x, y, w - ledW, h);
    if (ui_button_core(wf, x, y, w - ledW, h, &fof, &prf, &hotf)) r2_focus = m;
    if (ledW) { void *wm = ui_wid_hash(0x98u + m, x + w - ledW, y, ledW, h);
        if (ui_button_core(wm, x + w - ledW, y, ledW, h, &fom, &prm, &hotm)) mac[m].mute = !mac[m].mute; }
    rrectfill(x, y, w, h, 2, foc ? mac[m].col : mac[m].lo);
    rrect(x, y, w, h, 2, (foc || hotf) ? CLR_WHITE : CLR_BROWNISH_BLACK);
    font(FONT_NORMAL);   // the dos_8x8 in-game font reads best on the instrument nameplates (the maker's call)
    print(mac[m].name, x + 3, y + (h - 8) / 2, foc ? CLR_BROWNISH_BLACK : mac[m].col);
    if (ledW) { int lx = x + w - ledW / 2 - 1, ly = y + h / 2;
        circfill(lx, ly, 2, live ? (foc ? CLR_LIME_GREEN : CLR_DARK_GREEN) : CLR_DARKER_PURPLE);
        if (!live) line(lx - 2, ly - 2, lx + 2, ly + 2, CLR_RED); }
}

// ROOMY playhead — a LIT column that walks the on-screen cells, drawn ON TOP with an ADD glow so it
// reads over both empty grid and filled cells (the dark-green behind-column was invisible on the LCD).
// `s` = the live step (303 = its own lpos[i]; drums/MST = the shared transport `step`).
static void r2_playcol(int gx, int stepw, int s, int gy, int gh) {
    if (!playing || s < 0) return;
    blend(BLEND_ADD); rectfill(gx + s * stepw, gy, stepw - 1, gh, CLR_MEDIUM_GREEN); blend_reset();
}

// SHARED SCREEN — the 303 NOTE GRID (scale-degree rows × 16 steps). colour = accent, shape carries
// octave (^/v) + tie (a stretched cell); a line = slide. Draw + edit: tap = on/off, drag = draw the
// melody (pitch = row). ONE capture widget (not 16 — keeps well under UI_MAX_WID).
static void r2_screen303(Box g, int i) {
    int hi = mac[i ? M_303B : M_303A].col;
    int NR = SCALES[mscale[i]].n;
    // the note roll on top; three per-step effect lanes (ACC/SLD/TIE) stacked below it — the old
    // tb303 model (no arm-a-flag palette): draw notes right on the roll, toggle a lane cell directly.
    int laneH = 7, laneGap = 1, nlanes = 3, lanesH = nlanes * (laneH + laneGap);
    int octH = 9;                                     // the "active draw octave" pen strip (^ / — / v)
    int gut = 14, gx = (int)g.x + gut, gy = (int)g.y, gw = (int)g.w - gut - 1;
    int gridH = (int)g.h - 1 - lanesH - 2 - octH - 1; // reserve the octave-pen strip + the lane strip at the bottom
    int step = gw / STEPS, rh = gridH / NR, gh = rh * NR;
    int pen = r2_octpen[i];
    // ── the NOTE ROLL — one capture; tap/drag draws the melody (row = pitch) at the PEN octave. Tapping
    // the SAME note (row + pen octave) erases it; tapping a note at a DIFFERENT octave moves it to the pen.
    void *w = ui_wid_hash(0x110u + i, gx, gy, step * STEPS, gh); ui_reg(w, gx, gy, step * STEPS, gh, 0);
    UiCap *c = ui_cap_for(w);
    if (c) {
        static int note_erase;   // is this drag an ERASE gesture? latched on the grab frame so a held tap doesn't re-add
        int px = c->released ? c->rx : c->cx, py = c->released ? c->ry : c->cy;
        int s = (px - gx) / step, frow = (py - gy) / rh;
        if (s >= 0 && s < plen[i] && frow >= 0 && frow < NR) {
            int deg = (NR - 1) - frow;
            int match = on[i][s] && scale_idx(mscale[i], pit[i][s]) == deg && oct[i][s] == pen;  // the exact same note under the pen
            if (ui_grabbed(w)) note_erase = match;
            if (note_erase) { if (match) on[i][s] = 0; }
            else { on[i][s] = 1; pit[i][s] = SCALES[mscale[i]].deg[deg]; oct[i][s] = pen; }   // draw at the pen octave
        }
    }
    // faint grid + beat lines (the walking playhead is drawn ON TOP at the end)
    for (int s = 0; s < STEPS; s++) { int cx = gx + s * step;
        if (s % 4 == 0) line(cx, gy, cx, gy + gh, CLR_DARK_GREEN);
        if (s >= plen[i]) rectfill(cx, gy, step - 1, gh, CLR_BROWNISH_BLACK); }
    for (int r = 0; r <= NR; r++) line(gx, gy + r * rh, gx + gw, gy + r * rh, CLR_BROWNISH_BLACK);
    // the notes
    for (int s = 0; s < plen[i]; s++) {
        if (!on[i][s]) { if (tie[i][s]) rectfill(gx + s * step, gy + gh / 2, step - 1, 2, CLR_TRUE_BLUE); continue; }
        int idx = scale_idx(mscale[i], pit[i][s]), row = (NR - 1) - idx;
        int ry = gy + row * rh, cx = gx + s * step;
        int cw = step - 1 + (tie[i][s] ? step : 0), col = acc[i][s] ? CLR_WHITE : hi;
        rectfill(cx + 1, ry + 1, cw - 1, rh - 1, col);
        if (oct[i][s] > 0) { line(cx + 2, ry + 4, cx + 4, ry + 1, CLR_BLACK); line(cx + 4, ry + 1, cx + 6, ry + 4, CLR_BLACK); }
        else if (oct[i][s] < 0) { line(cx + 2, ry + rh - 5, cx + 4, ry + rh - 2, CLR_BLACK); line(cx + 4, ry + rh - 2, cx + 6, ry + rh - 5, CLR_BLACK); }
        int ns = (s + 1) % plen[i];
        if (sld[i][s] && on[i][ns]) { int r2 = (NR - 1) - scale_idx(mscale[i], pit[i][ns]);
            line(cx + step, ry + rh / 2, gx + ns * step, gy + r2 * rh + rh / 2, CLR_LIGHT_PEACH); }
    }
    // ── the OCTAVE PEN — an "active draw octave" toggle (^ up / — center / v down). Notes drawn on the
    // roll land at whichever is lit; default center = the old behaviour. Sits right under the roll.
    int oy = gy + gh + 1;
    font(FONT_TINY); print("OCT", (int)g.x + 1, oy + 2, R2_DIM);
    { static const int OV[3] = { 1, 0, -1 }; int bw = 15;
      for (int b = 0; b < 3; b++) { int bx = gx + b * (bw + 2), sel = (pen == OV[b]);
          if (lcdbtn(0x168u + i * 3 + b, bx, oy, bw, octH, "", sel)) r2_octpen[i] = OV[b];
          int cxg = bx + bw / 2, cyg = oy + octH / 2, gc = sel ? CLR_BLACK : hi;
          if (OV[b] > 0)      { line(cxg - 3, cyg + 2, cxg, cyg - 1, gc); line(cxg, cyg - 1, cxg + 3, cyg + 2, gc); }   // ^ up
          else if (OV[b] < 0) { line(cxg - 3, cyg - 1, cxg, cyg + 2, gc); line(cxg, cyg + 2, cxg + 3, cyg - 1, gc); }   // v down
          else                  line(cxg - 3, cyg, cxg + 3, cyg, gc); } }                                             // — center
    // ── the three effect LANES — one row each, tap/drag a cell to toggle it directly (no flag to arm)
    int ly0 = oy + octH + 2;
    static const char *LN[3]  = { "ACC", "SLD", "TIE" };
    for (int f = 0; f < 3; f++) {
        int *arr = (f == 0) ? acc[i] : (f == 1) ? sld[i] : tie[i];
        int lcol = (f == 0) ? CLR_ORANGE : (f == 1) ? CLR_LIME_GREEN : CLR_TRUE_BLUE;
        int ly = ly0 + f * (laneH + laneGap);
        void *lw = ui_wid_hash(0x160u + i * 3 + f, gx, ly, step * STEPS, laneH); ui_reg(lw, gx, ly, step * STEPS, laneH, 0);
        UiCap *lc = ui_cap_for(lw);
        if (lc) { int px = lc->released ? lc->rx : lc->cx, s = (px - gx) / step;
            if (s >= 0 && s < plen[i]) { if (ui_grabbed(lw)) paint_val = !arr[s]; arr[s] = paint_val; } }   // grab decides on/off, drag paints it across
        font(FONT_TINY); print(LN[f], (int)g.x + 1, ly + 1, lcol);
        for (int s = 0; s < plen[i]; s++) { int cx = gx + s * step;
            if (arr[s]) rectfill(cx + 1, ly, step - 2, laneH, lcol);
            else        rect(cx + 1, ly, step - 2, laneH, CLR_BROWNISH_BLACK); }
    }
    r2_playcol(gx, step, lpos[i], gy, gh);            // playhead over the roll…
    r2_playcol(gx, step, lpos[i], ly0, lanesH);       // …and over the lanes (skipping the octave-pen strip between)
}

// SHARED SCREEN — the drum 2D VOICE GRID (voices × 16 steps). left gutter names each voice; the
// selected row is lit. tap/drag paints hits. ONE capture widget.
static void r2_screendrum(Box g, int focus) {
    int hi = mac[focus].col, nv = (focus == M_808) ? TR_NV : TR9_NV;
    int (*grid)[STEPS] = (focus == M_808) ? dgrid : d9grid;
    int (*accg)[STEPS] = (focus == M_808) ? dacc : d9acc;
    int (*prbg)[STEPS] = (focus == M_808) ? dprob : d9prob;
    const char **vn = (focus == M_808) ? AB8 : AB9;
    int sel = (focus == M_808) ? dsel : d9sel;
    int tstep = step;   // capture the global transport step before the local `step` (cell width) shadows it below
    int gut = 13, gx = (int)g.x + gut, gy = (int)g.y, gw = (int)g.w - gut - 1, gh = (int)g.h;
    if (r2_dpaint >= 4) {   // p-lock LANE for the SELECTED voice — a bipolar per-step OFFSET (TUN/DEC/⟨char⟩) you draw
        int lk = r2_dpaint - 4, step0 = gw / STEPS, cy0 = gy + gh / 2, half = gh / 2 - 2;
        float *lane = (focus == M_808) ? doff[lk][sel] : d9off[lk][sel];
        const char *chn = (focus == M_808) ? CH8[sel] : CH9[sel];
        const char *nm = lk == LK_TUNE ? "TUN" : lk == LK_DEC ? "DEC" : (chn ? chn : "CHAR");
        void *w = ui_wid_hash(0x128u + focus, gx, gy, step0 * STEPS, gh); ui_reg(w, gx, gy, step0 * STEPS, gh, 0);
        UiCap *c = ui_cap_for(w);
        if (c) { int px = c->released ? c->rx : c->cx, py = c->released ? c->ry : c->cy, s = (px - gx) / step0;
            if (s >= 0 && s < STEPS) lane[s] = clamp((cy0 - py) / (float)half, -1, 1); }
        font(FONT_SMALL); print(vn[sel], (int)g.x + 2, gy + 1, hi);          // the voice being tuned (bigger)
        font(FONT_TINY);  print(nm, (int)g.x + 2, gy + 10, CLR_MEDIUM_GREEN);
        line(gx, cy0, gx + step0 * STEPS, cy0, CLR_MEDIUM_GREEN);             // zero / no-detune line
        for (int s = 0; s < STEPS; s++) { int cx = gx + s * step0;
            if (s % 4 == 0) line(cx, gy, cx, gy + gh, CLR_DARK_GREEN);
            int bh = (int)(lane[s] * half), col = lane[s] >= 0 ? hi : CLR_ORANGE;
            if (bh > 0)      rectfill(cx + 1, cy0 - bh, step0 - 2, bh, col);   // detune up = machine colour
            else if (bh < 0) rectfill(cx + 1, cy0, step0 - 2, -bh, col);       // detune down = orange
            rectfill(cx + 1, cy0 - bh - 1, step0 - 2, 2, CLR_WHITE);          // the slider HANDLE — always drawn (sits on the line at 0) so all 16 read as draggable sliders
            if (grid[sel][s]) rectfill(cx + 1, gy + gh - 2, step0 - 2, 2, hi); // this step FIRES — a clear block along the bottom (was a 1px dot)
        }
        r2_playcol(gx, step0, tstep, gy, gh);   // walking playhead on top (drum transport step)
        return;
    }
    int step = gw / STEPS, rh = gh / nv;
    // one capture over the grid — the FLAG tools are armed from the submenu ROW below (never over the grid)
    {
        void *w = ui_wid_hash(0x120u + focus, gx, gy, step * STEPS, rh * nv); ui_reg(w, gx, gy, step * STEPS, rh * nv, 0);
        UiCap *c = ui_cap_for(w);
        if (c) {
            int px = c->released ? c->rx : c->cx, py = c->released ? c->ry : c->cy;
            int s = (px - gx) / step, v = (py - gy) / rh;
            if (s >= 0 && s < STEPS && v >= 0 && v < nv) {
                if (r2_dpaint == 1) {                                 // ACC — fill-drag: a hit + its accent
                    if (ui_grabbed(w)) paint_val = !accg[v][s];
                    accg[v][s] = paint_val; if (paint_val) grid[v][s] = 1;
                } else if (r2_dpaint == 2 && ui_grabbed(w)) {         // PROB — cycle on the tap
                    grid[v][s] = 1; int p = prbg[v][s]; prbg[v][s] = p >= 100 ? 75 : p >= 75 ? 50 : p >= 50 ? 25 : 100;
                } else if (r2_dpaint == 3 && ui_grabbed(w) && focus == M_909) {   // STRK — cycle none→flam→drag→ratchet
                    d9strk[v][s] = (d9strk[v][s] + 1) & 3; grid[v][s] = 1;
                } else if (r2_dpaint == 0) {                          // HIT — paint hits (the default)
                    if (ui_grabbed(w)) paint_val = !grid[v][s];
                    grid[v][s] = paint_val;
                }
            }
        }
    }
    for (int v = 0; v < nv; v++) {
        int ry = gy + v * rh;
        if (v == sel) rectfill((int)g.x + 1, ry, gut - 1, rh - 1, CLR_DARK_GREEN);
        font(FONT_TINY); print(vn[v], (int)g.x + 2, ry + (rh - 5) / 2, (v == sel) ? CLR_WHITE : CLR_MEDIUM_GREEN);
        for (int s = 0; s < STEPS; s++) { int cx = gx + s * step;
            if (grid[v][s]) {
                int pr = prbg[v][s] > 0 ? prbg[v][s] : 100, ch = (rh - 2) * pr / 100; if (ch < 2) ch = 2;   // PROB = a shorter cell
                rectfill(cx + 1, ry + 1 + (rh - 2 - ch), step - 2, ch, accg[v][s] ? CLR_WHITE : hi);
                if (focus == M_909 && d9strk[v][s]) for (int p = 0; p <= d9strk[v][s]; p++) pset(cx + 1 + p * 2, ry + 1, CLR_TRUE_BLUE);   // STRK pips
            } else pset(cx + step / 2, ry + rh / 2, (s % 4 == 0) ? CLR_DARK_GREEN : CLR_BROWNISH_BLACK);
        }
    }
    r2_playcol(gx, step, tstep, gy, rh * nv);   // walking playhead on top (drum transport step)
}

// one master automation lane in box `lb`: a left LABEL (tap it to expand this lane full-height / back
// to the 3-up overview) + the 16-step 0..7 bars (drag to paint). Bigger box = taller bars = finer edit.
static void r2_mstlane_draw(Box lb, int L, int tstep) {
    int *lane = (L == 0) ? mpcf : (L == 1) ? mcrush : mgate;
    int col   = (L == 0) ? CLR_GREEN : (L == 1) ? CLR_ORANGE : CLR_PINK;
    const char *nm = (L == 0) ? "PCF" : (L == 1) ? "CRU" : "GAT";
    int gut = 20, gy = (int)lb.y, gx = (int)lb.x + gut, gw = (int)lb.w - gut, gh = (int)lb.h, stw = gw / STEPS;
    { int lx = (int)lb.x, lw = gut - 1, pr = 0, hot = 0, fo = 0;   // the label = a toggle: expand this lane / collapse to the overview
      void *w = ui_wid_hash(0x190u + L, lx, gy, lw, gh);
      if (ui_button_core(w, lx, gy, lw, gh, &fo, &pr, &hot)) r2_mstlane = (r2_mstlane == L) ? -1 : L;
      font(FONT_TINY); print(nm, lx + 1, gy + gh / 2 - 2, (hot || r2_mstlane == L) ? CLR_WHITE : col); }
    void *w2 = ui_wid_hash(0x180u + L, gx, gy, stw * STEPS, gh); ui_reg(w2, gx, gy, stw * STEPS, gh, 0);
    UiCap *c = ui_cap_for(w2);
    if (c) { int px = c->released ? c->rx : c->cx, py = c->released ? c->ry : c->cy, s = (px - gx) / stw;
        if (s >= 0 && s < STEPS) { int lv = 7 - (py - gy) * 8 / gh; if (lv < 0) lv = 0; if (lv > 7) lv = 7; lane[s] = lv; } }
    for (int s = 0; s < STEPS; s++) { int cx = gx + s * stw;
        if (s % 4 == 0) line(cx, gy, cx, gy + gh, CLR_DARK_GREEN);
        int v = lane[s] * (gh - 1) / 7;
        rectfill(cx, gy + gh - v, stw - 1, v, col); }
    r2_playcol(gx, stw, tstep, gy, gh);
}

// SHARED SCREEN — master, ALL AT ONCE (no tabs): the 4-channel mixer on the left, the three
// per-step automation lanes PCF / CRUSH / GATE on the right — the 3-up overview, or one EXPANDED.
static void r2_screenmst(Box g) {
    int tstep = step;                                        // transport step, before local `step` shadows it
    Box mx = lay_split(g, EDGE_LEFT, lay_clamp(g.w * 0.26f, 40, 90), &g);
    // ── the 4-channel mixer ──
    { static const int MC[4] = { M_303A, M_303B, M_808, M_909 };
      static const char *ML[4] = { "303a", "303b", "808", "909" };
      Box m = lay_inset(mx, 3); int fw = (int)m.w / 4, bh = (int)m.h - 8;
      for (int k = 0; k < 4; k++) { int cx = (int)m.x + k * fw, by = (int)m.y;
          int lv = (int)(level[MC[k]] / 2.0f * bh); if (lv > bh) lv = bh; if (lv < 0) lv = 0;
          rectfill(cx, by, fw - 2, bh, CLR_BLACK);
          rectfill(cx, by + bh - lv, fw - 2, lv, mac[MC[k]].col);
          font(FONT_TINY); print(ML[k], cx, by + bh + 2, mac[MC[k]].col); } }
    // ── the automation lanes ── the 3-up overview, OR one lane EXPANDED to full height (tap a label)
    Box la = lay_inset(g, 2);
    if (r2_mstlane >= 0) r2_mstlane_draw(la, r2_mstlane, tstep);
    else { int lh = (int)la.h / 3; for (int L = 0; L < 3; L++) r2_mstlane_draw(box(la.x, la.y + L * lh, la.w, lh - 2), L, tstep); }
}

// MST GEN panel — the whole-rack song GENERATOR: one tap fills both 303s + a kit + tempo/swing/key
// by genre (gen_song). Text-fit row of the cute style names.
static void r2_mstgen(Box b) {
    Box m = lay_inset(b, 2);
    int gx = (int)m.x;
    for (int i = 0; i < SNG_N; i++) if (lcdbtnf(0x2A0u + i, &gx, (int)m.y, (int)m.h - 1, SNG_NAME[i], 0)) gen_song(i);
}

// MST DLY panel — the tempo-synced delay DIVISION picker (1/16 · 1/8 · DOT · 1/4 → mdiv).
static void r2_mstdelay(Box b) {
    Box m = lay_inset(b, 2);
    static const char *DL[4] = { "1/16", "1/8", "DOT", "1/4" };
    int dx = (int)m.x;
    for (int k = 0; k < 4; k++) if (lcdbtnf(0x2D0u + k, &dx, (int)m.y, (int)m.h - 1, DL[k], mdiv == k)) mdiv = k;
}

// MST SONG panel — six whole-rack song slots (the "6 songs in master" layer). TAP a slot = load ·
// HOLD to charge = save (an occupied slot asks X/OK first). Ported from the phone SONGS page.
static void r2_mstsong(Box b) {
    Box m = lay_inset(b, 2);
    static int shf[NSLOT] = { 0 };
    static int confirm = -1;
    int cur = song_slot_cur(), sw = (int)m.w / NSLOT;
    for (int i = 0; i < NSLOT; i++) {
        int x = (int)m.x + i * sw, y = (int)m.y, w = sw - 2, h = (int)m.h - 1, used = song_slot_used(i);
        char n[2] = { (char)('1' + i), 0 };
        if (confirm == i) {                                   // this slot is asking before overwrite
            if (lcdbtn(0x2B8u + i, x, y, w / 2 - 1, h, "X", 0)) confirm = -1;
            if (lcdbtn(0x2C8u + i, x + w / 2, y, w - w / 2, h, "OK", 0)) { song_save_slot(i); confirm = -1; }
            continue;
        }
        if (confirm >= 0) { rrect(x, y, w, h, 2, CLR_MEDIUM_GREEN); font(FONT_TINY); print(n, x + 3, y + 2, CLR_MEDIUM_GREEN); if (used) circfill(x + w - 4, y + 4, 1, CLR_MEDIUM_GREEN); continue; }
        void *wid = ui_wid_hash(0x2B0u + i, x, y, w, h);
        int pr = 0, hot = 0, foc = 0, act = ui_button_core(wid, x, y, w, h, &foc, &pr, &hot);
        if (pr) shf[i]++;
        else { if (act) { if (shf[i] >= SONG_HOLD) { if (used) confirm = i; else song_save_slot(i); } else if (shf[i] <= PERF_TAP && used) song_load_slot(i); } shf[i] = 0; }
        int holding = pr && shf[i] > PERF_TAP, ready = pr && shf[i] >= SONG_HOLD;
        if (hot && !pr) rrectfill(x, y, w, h, 2, CLR_MEDIUM_GREEN);
        if (holding) { float p = clamp((shf[i] - PERF_TAP) / (float)(SONG_HOLD - PERF_TAP), 0, 1); int bw = (int)((w - 2) * p + 0.5f); if (bw > 0) rectfill(x + 1, y + 1, bw, h - 2, ready ? CLR_LIGHT_YELLOW : CLR_MEDIUM_GREEN); }
        rrect(x, y, w, h, 2, (i == cur) ? CLR_WHITE : ready ? CLR_LIGHT_YELLOW : CLR_LIME_GREEN);
        font(FONT_TINY); print(n, x + 3, y + 2, ready ? CLR_DARK_GREEN : CLR_LIME_GREEN);
        if (used) circfill(x + w - 4, y + 4, 1, (i == cur) ? CLR_WHITE : CLR_LIME_GREEN);
    }
}

// the drum GEN panel — CLR/MIN/MID/BSY (text-fit row), per machine.
static void r2_gendrum(Box main, int focus) {
    Box m = lay_inset(main, 2);
    static const char *GN[4] = { "CLR", "MIN", "MID", "BSY" };
    int gx = (int)m.x;
    for (int g = 0; g < 4; g++) if (lcdbtnf(0x158u + g, &gx, (int)m.y, (int)m.h - 1, GN[g], 0)) { if (focus == M_808) gen_drums(g); else gen_drums9(g); }
}

// the drum FLAG panel — the paint-tool palette (what a grid tap does): HIT/ACC/PROB[/STRK] on top,
// the per-step p-locks TUN/DEC/⟨char⟩ below. Sets r2_dpaint (>=4 = a p-lock → the grid becomes the
// bipolar offset lane for the selected voice). Text-fit + left-packed, 2 rows.
static void r2_drumflags(Box b, int focus) {
    Box m = lay_inset(b, 2);
    int rowH = ((int)m.h - 1) / 2, y0 = (int)m.y, y1 = y0 + rowH + 1, px = (int)m.x;
    static const char *FL[4] = { "HIT", "ACC", "PROB", "STRK" };
    int fn = (focus == M_909) ? 4 : 3;
    for (int k = 0; k < fn; k++) if (lcdbtnf(0x148u + k, &px, y0, rowH, FL[k], r2_dpaint == k)) r2_dpaint = k;
    px = (int)m.x;
    int selv = (focus == M_808) ? dsel : d9sel;
    const char *chn = (focus == M_808) ? CH8[selv] : CH9[selv];
    const char *PL[3] = { "TUN", "DEC", chn ? chn : "" };
    int pn = chn ? 3 : 2;
    for (int k = 0; k < pn; k++) if (lcdbtnf(0x14Cu + k, &px, y1, rowH, PL[k], r2_dpaint == 4 + k)) r2_dpaint = 4 + k;
}

// the 303 screen's bottom band (2 rows): SEQ/SETUP tabs LEFT-aligned, the flag palette (2 rows × 3)
// RIGHT-aligned — the grid above keeps the two groups apart. SEQ = grid; SETUP = GEN + KEY.
// 303 PERF — the live-play lenses (TAP=latch / HOLD=momentary; read-path, non-destructive). Same
// pf_latch[] state as the phone; draw() seeds the effective pf_* from the latches each frame.
// PERF as a revealed 2-row panel (the 7 live-play lenses, text-fit + left-packed) — PERF chip toggles it.
static void r2_perf303(Box main, int i) {
    Box m = lay_inset(main, 2);
    int rowH = ((int)m.h - 1) / 2, y0 = (int)m.y, y1 = y0 + rowH + 1, px = (int)m.x;
    pf_half[i]  = lcdlatchf(0x7Bu, &px, y0, rowH, "HALF",  &pf_latch[PL_HALF][i],  &pf_hold[PL_HALF][i],  0);
    pf_acc[i]   = lcdlatchf(0x7Du, &px, y0, rowH, "ACC",   &pf_latch[PL_ACC][i],   &pf_hold[PL_ACC][i],   0);
    pf_oct[i]   = lcdlatchf(0x80u, &px, y0, rowH, "OCT",   &pf_latch[PL_OCT][i],   &pf_hold[PL_OCT][i],   0);
    pf_rev[i]   = lcdlatchf(0x82u, &px, y0, rowH, "REV",   &pf_latch[PL_REV][i],   &pf_hold[PL_REV][i],   0);
    px = (int)m.x;
    pf_stac[i]  = lcdlatchf(0x7Eu, &px, y1, rowH, "STAC",  &pf_latch[PL_STAC][i],  &pf_hold[PL_STAC][i],  &pf_latch[PL_GLIDE][i]);
    pf_glide[i] = lcdlatchf(0x7Fu, &px, y1, rowH, "GLIDE", &pf_latch[PL_GLIDE][i], &pf_hold[PL_GLIDE][i], &pf_latch[PL_STAC][i]);
    pf_roll[i]  = lcdlatchf(0x83u, &px, y1, rowH, "ROLL",  &pf_latch[PL_ROLL][i],  &pf_hold[PL_ROLL][i],  0);
}

// a one-octave PIANO KEYBOARD root picker (white C D E F G A B + narrow black keys between), like the
// phone KEY screen — tap a key to set this line's root; the selected key lights up.
static void r2_keyboard(Box b, int i) {
    static const int   WMIDI[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static const char *WN[7]    = { "C", "D", "E", "F", "G", "A", "B" };
    static const int   BK[5][2] = { { 1, 1 }, { 3, 2 }, { 6, 4 }, { 8, 5 }, { 10, 6 } };  // {root midi, white index it sits left of}
    int bx = (int)b.x, by = (int)b.y, bh = (int)b.h, ww = (int)b.w / 7; if (ww > 20) ww = 20;   // cap the key width so the keyboard is compact (not stretched full-width)
    for (int k = 0; k < 7; k++) {                     // white keys (full height)
        int px = bx + k * ww, pw = ww - 1, lit = (mroot[i] == WMIDI[k]);
        int pr = 0, hot = 0, fo = 0; void *w = ui_wid_hash(0x250u + k, px, by, pw, bh);
        if (ui_button_core(w, px, by, pw, bh, &fo, &pr, &hot)) mroot[i] = WMIDI[k];
        rrectfill(px, by, pw, bh, 1, lit ? CLR_LIME_GREEN : CLR_DARK_GREEN);
        rrect(px, by, pw, bh, 1, (lit || hot) ? CLR_WHITE : CLR_MEDIUM_GREEN);
        font(FONT_TINY); plabel(WN[k], px + pw / 2, by + bh - 6, lit ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREEN);
    }
    int bkw = (ww * 6) / 10, bkh = (bh * 6) / 10;     // black keys (narrow, top portion, ON TOP → hit-tested after)
    for (int j = 0; j < 5; j++) {
        int px = bx + BK[j][1] * ww - bkw / 2, lit = (mroot[i] == BK[j][0]);
        int pr = 0, hot = 0, fo = 0; void *w = ui_wid_hash(0x258u + j, px, by, bkw, bkh);
        if (ui_button_core(w, px, by, bkw, bkh, &fo, &pr, &hot)) mroot[i] = BK[j][0];
        rrectfill(px, by, bkw, bkh, 1, lit ? CLR_LIME_GREEN : CLR_BROWNISH_BLACK);
        rrect(px, by, bkw, bkh, 1, (lit || hot) ? CLR_WHITE : CLR_BLACK);
    }
}

// GEN panel (the GEN chip toggles it): the generate buttons AND the KEY editor together — because
// "when generating you want to do key too". Rows: CLR/MIN/MID/BSY · the piano keyboard (root) · scale + octave.
static void r2_gen303(Box b, int i) {
    Box m = lay_inset(b, 2);
    Box genr = lay_split(m, EDGE_TOP, lay_clamp(m.h * 0.22f, 8, 12), &m);
    static const char *GN[4] = { "CLR", "MIN", "MID", "BSY" };
    { int gx = (int)genr.x; for (int g = 0; g < 4; g++) if (lcdbtnf(0x240u + g, &gx, (int)genr.y, (int)genr.h - 1, GN[g], 0)) gen_line(i, g); }
    Box kb = lay_split(m, EDGE_TOP, m.h * 0.55f, &m);
    r2_keyboard(kb, i);
    int sx = (int)m.x, sy = (int)m.y, sh = (int)m.h - 1;         // scale + octave row, flowing
    if (lcdbtnf(0x246u + i, &sx, sy, sh, SCALES[mscale[i]].name, 0)) {   // scale name (cycles + remaps the line)
        int old = mscale[i], nw = (old + 1) % NSCALE;
        for (int s = 0; s < STEPS; s++) { int di = scale_idx(old, pit[i][s]); if (di >= SCALES[nw].n) di = SCALES[nw].n - 1; pit[i][s] = SCALES[nw].deg[di]; }
        mscale[i] = nw;
    }
    sx += 4; font(FONT_TINY); print("OCT", sx, sy + sh / 2 - 2, CLR_MEDIUM_GREEN); sx += text_width("OCT") + 4;
    if (lcdbtnf(0x248u + i, &sx, sy, sh, "-", 0) && loct[i] > -2) loct[i]--;
    { char ob[3] = { (char)(loct[i] > 0 ? '+' : loct[i] < 0 ? '-' : ' '), (char)('0' + (loct[i] < 0 ? -loct[i] : loct[i])), 0 };
      print(ob, sx, sy + sh / 2 - 2, CLR_LIME_GREEN); sx += text_width(ob) + 4; }
    if (lcdbtnf(0x24Au + i, &sx, sy, sh, "+", 0) && loct[i] <  2) loct[i]++;
}

// drum PERF — beat-repeat RP1/RP2/RP4 + THIN/BUSY density + ACC (the roomy twin of draw_drum_perf,
// which keys off `face`; here the machine comes in as `focus`).
static void r2_perfdrum(Box main, int focus) {
    int m = focus - M_808;   // 0 = 808, 1 = 909
    Box g = lay_inset(main, 2);
    int rowH = ((int)g.h - 1) / 2, y0 = (int)g.y, y1 = y0 + rowH + 1, px = (int)g.x;
    pf_rp1[m]  = lcdlatchf(0x84u, &px, y0, rowH, "RP1",  &dpf_latch[DPL_RP1][m],  &dpf_hold[DPL_RP1][m],  0);
    pf_rp2[m]  = lcdlatchf(0x85u, &px, y0, rowH, "RP2",  &dpf_latch[DPL_RP2][m],  &dpf_hold[DPL_RP2][m],  0);
    pf_rp4[m]  = lcdlatchf(0x86u, &px, y0, rowH, "RP4",  &dpf_latch[DPL_RP4][m],  &dpf_hold[DPL_RP4][m],  0);
    px = (int)g.x;
    pf_thin[m] = lcdlatchf(0x87u, &px, y1, rowH, "THIN", &dpf_latch[DPL_THIN][m], &dpf_hold[DPL_THIN][m], &dpf_latch[DPL_BUSY][m]);
    pf_busy[m] = lcdlatchf(0x88u, &px, y1, rowH, "BUSY", &dpf_latch[DPL_BUSY][m], &dpf_hold[DPL_BUSY][m], &dpf_latch[DPL_THIN][m]);
    pf_dacc[m] = lcdlatchf(0x89u, &px, y1, rowH, "ACC",  &dpf_latch[DPL_ACC][m],  &dpf_hold[DPL_ACC][m],  0);
}

static void r2_bigscreen(Box c, int focus) {
    int x = (int)c.x, y = (int)c.y, w = (int)c.w, h = (int)c.h;
    rectfill(x, y, w, h, CLR_DARK_GREEN);
    rect(x, y, w, h, mac[focus].lo); rect(x + 1, y + 1, w - 2, h - 2, CLR_BLACK);
    static const char *ROLE[M_N] = { "NOTE GRID", "NOTE GRID", "16 VOICES", "11 VOICES", "MIX+FX" };
    font(FONT_TINY);
    print(mac[focus].name, x + 4, y + 3, mac[focus].col);
    print(ROLE[focus], x + 34, y + 3, CLR_MEDIUM_GREEN);
    { char b[8]; int bp = (int)g_bpm, k = 0; if (bp >= 100) b[k++] = '0' + bp / 100; b[k++] = '0' + (bp / 10) % 10; b[k++] = '0' + bp % 10; b[k++] = 0;
      print(b, x + w - 24, y + 3, CLR_LIME_GREEN); }
    if (focus <= M_303B) {   // 303: full-width roll; PERF / GEN are panels REVEALED by the bottom-left chips (roll keeps the room when closed)
        int i = focus, chipH = 11, open = r2_303panel[i];
        int panelH = (open == 2) ? 46 : 26;            // GEN (piano + gen + scale/oct) needs more height than PERF
        int botH = chipH + (open ? panelH + 1 : 0);
        Box main = box(x + 3, y + 13, w - 6, h - 13 - botH - 2);
        r2_screen303(main, i);                                       // roll + octave pen + ACC/SLD/TIE lanes
        if (open) {                                                  // the revealed control panel sits just above the chips
            Box panel = box(x + 3, y + h - chipH - panelH - 1, w - 6, panelH);
            if (open == 1) r2_perf303(panel, i); else r2_gen303(panel, i);
        }
        int chx = x + 3;                                             // toggle chips, bottom-left (text-fit)
        if (lcdbtnf(0x132u + i, &chx, y + h - chipH - 1, chipH - 1, "PERF", open == 1)) r2_303panel[i] = (open == 1) ? 0 : 1;
        if (lcdbtnf(0x135u + i, &chx, y + h - chipH - 1, chipH - 1, "GEN",  open == 2)) r2_303panel[i] = (open == 2) ? 0 : 2;
        return;
    }
    if (focus <= M_909) {   // drum: full-width voice grid; FLAG (paint) / GEN / PERF are panels REVEALED by the bottom-left chips
        int mi = focus - M_808, chipH = 11, open = r2_drumpanel[mi];
        int panelH = (open == 2) ? 14 : 22;                         // GEN = 1 row · FLAG/PERF = 2 rows
        int botH = chipH + (open ? panelH + 1 : 0);
        Box main = box(x + 3, y + 13, w - 6, h - 13 - botH - 2);
        r2_screendrum(main, focus);                                 // the voice grid (or the p-lock offset lane when a p-lock is armed)
        if (open) {
            Box panel = box(x + 3, y + h - chipH - panelH - 1, w - 6, panelH);
            if      (open == 1) r2_drumflags(panel, focus);
            else if (open == 2) r2_gendrum(panel, focus);
            else                r2_perfdrum(panel, focus);
        }
        int chx = x + 3, cy = y + h - chipH - 1;                    // toggle chips, bottom-left (text-fit)
        if (lcdbtnf(0x230u + focus * 3 + 0, &chx, cy, chipH - 1, "FLAG", open == 1)) r2_drumpanel[mi] = (open == 1) ? 0 : 1;
        if (lcdbtnf(0x230u + focus * 3 + 1, &chx, cy, chipH - 1, "GEN",  open == 2)) r2_drumpanel[mi] = (open == 2) ? 0 : 2;
        if (lcdbtnf(0x230u + focus * 3 + 2, &chx, cy, chipH - 1, "PERF", open == 3)) r2_drumpanel[mi] = (open == 3) ? 0 : 3;
        return;
    }
    // MST — mixer + PCF/CRU/GAT lanes all at once; GEN / SONG / DLY are chip-revealed panels (like 303/drums)
    {
        int chipH = 11, open = r2_mstpanel;
        int panelH = (open == 2) ? 22 : 13;                         // SONG (6 slots) taller; GEN/DLY are one row
        int botH = chipH + (open ? panelH + 1 : 0);
        r2_screenmst(box(x + 3, y + 13, w - 6, h - 13 - botH - 2));  // mixer + lanes (default all-at-once)
        if (open) {
            Box panel = box(x + 3, y + h - chipH - panelH - 1, w - 6, panelH);
            if      (open == 1) r2_mstgen(panel);
            else if (open == 2) r2_mstsong(panel);
            else                r2_mstdelay(panel);
        }
        int chx = x + 3, cy = y + h - chipH - 1;                    // toggle chips, bottom-left (text-fit)
        if (lcdbtnf(0x13Cu, &chx, cy, chipH - 1, "GEN",  open == 1)) r2_mstpanel = (open == 1) ? 0 : 1;
        if (lcdbtnf(0x13Du, &chx, cy, chipH - 1, "SONG", open == 2)) r2_mstpanel = (open == 2) ? 0 : 2;
        if (lcdbtnf(0x13Eu, &chx, cy, chipH - 1, "DLY",  open == 3)) r2_mstpanel = (open == 3) ? 0 : 3;
    }
}

// a narrow 303 knob-column: header + 5 acid knobs + FX trio + CL/DF + KEY. The note surface lives
// on the big screen; this column is only the SOUND (always live, focus or not).
static void r2_col303(Box c, int i) {
    int m = i ? M_303B : M_303A; Acid *a = &ac[i];
    rrectfill((int)c.x, (int)c.y, (int)c.w, (int)c.h, 2, mac[m].lo);                  // 2px frame (fill-and-punch)
    rrectfill((int)c.x + 2, (int)c.y + 2, (int)c.w - 4, (int)c.h - 4, 2, R2_PNL);
    Box cc = lay_inset(c, 2);
    r2_header(lay_split(cc, EDGE_TOP, 12, &cc), m);      // nameplate at the TOP as well…
    r2_header(lay_split(cc, EDGE_BOTTOM, 12, &cc), m);   // …AND the BOTTOM — focus/mute reachable from either end (the maker's ask)
    Box meta = lay_split(cc, EDGE_BOTTOM, 30, &cc);   // voicing+WAVE row · A/B/C/D banks · KEY
    Box fx   = lay_split(cc, EDGE_BOTTOM, 26, &cc);
    // acid knobs, 2-wide (filter pair CUT|RES together): CORE = CUT/RES/ENV/DEC/ACC, ALWAYS shown.
    // In Devil Fish voicing the DEEP knobs (SUB/ADEC/SLDT/TRK) show BELOW the core — no page toggle,
    // both live at once (the full-height column has the room). WAVE stays in the meta row (both voicings).
    Box knob = cc;
    if (!a->classic) {                                   // DF voicing → a DEEP knob section under the core
        Box dfb = lay_split(knob, EDGE_BOTTOM, knob.h * 0.42f, &knob);
        line((int)dfb.x + 2, (int)dfb.y, (int)dfb.x + (int)dfb.w - 2, (int)dfb.y, mac[m].lo);
        Box dfl = lay_split(dfb, EDGE_TOP, 6, &dfb);
        font(FONT_TINY); print("DF", (int)dfl.x + 2, (int)dfl.y, R2_DIM);
        static const int DK[4] = { ACID_SUB, ACID_ADEC, ACID_SLDT, ACID_TRK };
        static const char *DN[4] = { "SUB", "ADEC", "SLDT", "TRK" };
        static const float DFD[4] = { 0.0f, 0.40f, 0.14f, 0.0f };
        for (int k = 0; k < 4; k++) r2_kcell(lay_grid(dfb, 2, 4, k, 1), &a->p[DK[k]], DN[k], DFD[k], mac[m].col);
    }
    { Box accr = lay_split(knob, EDGE_BOTTOM, knob.h / 3, &knob);   // ACC on its own centred row, bottom of the CORE cluster
      static const int AK[4] = { ACID_CUT, ACID_RES, ACID_ENV, ACID_DEC };
      static const char *AN[4] = { "CUT", "RES", "ENV", "DEC" };
      static const float AD[4] = { 0.55f, 0.70f, 0.55f, 0.45f };
      for (int k = 0; k < 4; k++) r2_kcell(lay_grid(knob, 2, 4, k, 1), &a->p[AK[k]], AN[k], AD[k], mac[m].col);
      r2_kcell(accr, &a->p[ACID_ACC], "ACC", 0.55f, mac[m].col); }
    // FX trio — a horizontal row so it reads as "the FX", not more core
    line((int)fx.x + 2, (int)fx.y, (int)fx.x + (int)fx.w - 2, (int)fx.y, mac[m].lo);
    font(FONT_TINY); print("FX", (int)fx.x + 2, (int)fx.y + 1, R2_DIM);
    Box fxr = lay_split(fx, EDGE_BOTTOM, fx.h - 6, &fx);
    r2_kcell(lay_grid(fxr, 3, 3, 0, 1), &a->p[ACID_DRV], "DRV", 0.35f, mac[m].col);
    r2_kcell(lay_grid(fxr, 3, 3, 1, 1), &msend[i],       "SND", 0.10f, mac[m].col);
    r2_kcell(lay_grid(fxr, 3, 3, 2, 1), &fxverb[i],      "VRB", 0.0f,  mac[m].col);
    // meta = 3 rows: [CL/DF voicing + WAVE] · [A/B/C/D pattern banks] · [KEY label].
    Box keyR  = lay_split(meta, EDGE_BOTTOM, 7, &meta);        // KEY label (bottom)
    Box bankR = lay_split(meta, EDGE_BOTTOM, 9, &meta);        // A/B/C/D banks
    int mx = (int)meta.x, my = (int)meta.y, mw = (int)meta.w, bh = 9, pr = 0, hot = 0, fo = 0;
    // voicing toggle (CL|DF) on the left, WAVE (SAW/SQR — a CORE control) on the right
    int vw = mw * 3 / 5, cw = (vw - 1) / 2;
    void *wd = ui_wid_hash(0xA0u + i, mx, my, vw, bh);
    if (ui_button_core(wd, mx, my, vw, bh, &fo, &pr, &hot)) { a->classic = !a->classic; if (a->classic) kpage[i] = 0; acid_define(a); }
    int df = !a->classic;
    rrectfill(mx, my, cw, bh, 2, df ? R2_PNL : mac[m].col);
    rrectfill(mx + cw + 1, my, cw, bh, 2, df ? mac[m].col : R2_PNL);
    font(FONT_TINY);
    print("CL", mx + cw / 2 - 4, my + 2, df ? R2_DIM : CLR_BLACK);
    print("DF", mx + cw + 1 + cw / 2 - 4, my + 2, df ? CLR_BLACK : R2_DIM);
    int wx = mx + vw + 2, ww = mw - vw - 2;
    if (cbtn(0x1A8u + i, wx, my, ww, bh, a->wave == INSTR_SQUARE ? "SQR" : "SAW", 0)) { a->wave = (a->wave == INSTR_SAW) ? INSTR_SQUARE : INSTR_SAW; acid_define(a); }
    // A/B/C/D pattern banks — little always-visible rects (lit = current; blinks = armed while playing)
    static const char *BK[NPAT] = { "A", "B", "C", "D" };
    int kw = mw / NPAT;
    for (int k = 0; k < NPAT; k++) {
        int lit = (curpat[m] == k) || (armpat[m] == k && ((int)g_phase & 1));
        if (cbtn(0x1B0u + i * 8 + k, mx + k * kw, (int)bankR.y, kw - 1, (int)bankR.h - 1, BK[k], lit)) pat_queue(m, k);
    }
    // KEY label
    font(FONT_TINY);
    print("KEY", mx, (int)keyR.y + 1, R2_DIM);
    print(NOTE[mroot[i]], mx + 18, (int)keyR.y + 1, mac[m].col);
    print(SCALES[mscale[i]].name, mx + 30, (int)keyR.y + 1, mac[m].col);
}

// the MASTER knob-column — same shape as a 303: header + master knobs + a 4-channel mini mixer.
static void r2_colmst(Box c) {
    rrectfill((int)c.x, (int)c.y, (int)c.w, (int)c.h, 2, mac[M_MST].lo);             // 2px frame (fill-and-punch)
    rrectfill((int)c.x + 2, (int)c.y + 2, (int)c.w - 4, (int)c.h - 4, 2, R2_PNL);
    Box cc = lay_inset(c, 2);
    r2_header(lay_split(cc, EDGE_TOP, 12, &cc), M_MST);      // nameplate top…
    r2_header(lay_split(cc, EDGE_BOTTOM, 12, &cc), M_MST);   // …and bottom (aligns with the 303 columns)
    Box mix = lay_split(cc, EDGE_BOTTOM, 26, &cc);
    // master knobs — 2-wide grid, filter pair FLT|RES together (mirrors the 303 CUT|RES); FB centred
    // on its own row below (the delay feedback). RES + FB were the two missing from the column.
    float *KV[6] = { &bpm01, &g_swing, &mglu, &mpump, &mflt, &mfres };
    static const char *KN[6] = { "TMP", "SWG", "GLU", "PMP", "FLT", "RES" };
    static const float KD[6] = { 0.5143f, 0.0f, 0.30f, 0.0f, 0.5f, 0.35f };
    Box fbr = lay_split(cc, EDGE_BOTTOM, cc.h / 4, &cc);      // FB row (centred, full width)
    for (int k = 0; k < 6; k++) r2_kcell(lay_grid(cc, 2, 6, k, 1), KV[k], KN[k], KD[k], mac[M_MST].col);
    r2_kcell(fbr, &mfb, "FB", 0.35f, mac[M_MST].col);
    // 4-channel mini mixer (303a/303b/808/909 faders) + delay division label
    int mmx = (int)mix.x + 2, mmy = (int)mix.y, fw = ((int)mix.w - 4) / 4;
    static const int MC[4] = { M_303A, M_303B, M_808, M_909 };
    for (int k = 0; k < 4; k++) { int cx = mmx + k * fw, bh = 14, lv = (int)(level[MC[k]] / 2.0f * bh); if (lv > bh) lv = bh;
        rectfill(cx, mmy, fw - 1, bh, CLR_BLACK);
        rectfill(cx, mmy + bh - lv, fw - 1, lv, mac[MC[k]].col); }
    static const char *DIV[4] = { "1/16", "1/8", "DOT", "1/4" };
    font(FONT_TINY); print("DLY", mmx, mmy + 16, mac[M_MST].col); print(DIV[mdiv & 3], mmx + 18, mmy + 16, R2_INK);
}

// a horizontal drum strip: header + a PAD per voice + FX trio + (909's spare room) the SHARED
// context panel that edits whichever voice was last picked on either machine, ringed in its colour.
// the shared VOICE context row (its own full-width band above the drums): the selected voice's
// knobs (TUN/DEC/⟨char⟩/VOL/PAN/FINE), following whichever pad was last picked on either machine,
// ringed + labelled in that machine's colour (blue = 808 voice, amber = 909).
static void r2_ctxrow(Box c) {
    int sm = r2_selmach, sv = (sm == M_808) ? dsel : d9sel, hi = mac[sm].col;
    const char *svn = (sm == M_808) ? AB8[sv] : AB9[sv];
    const char *chn = (sm == M_808) ? CH8[sv] : CH9[sv];
    rrectfill((int)c.x, (int)c.y, (int)c.w, (int)c.h, 2, hi);                        // 2px frame (fill-and-punch)
    rrectfill((int)c.x + 2, (int)c.y + 2, (int)c.w - 4, (int)c.h - 4, 2, R2_PNL);
    Box cc = lay_inset(c, 2);
    Box lab = lay_split(cc, EDGE_LEFT, 34, &cc);
    font(FONT_NORMAL); print((sm == M_808) ? "808" : "909", (int)lab.x, (int)lab.y + 1, hi);
    font(FONT_TINY);   print(svn, (int)lab.x, (int)lab.y + 11, R2_INK);
    // MUT / REC pad-latches — BEFORE the knobs now (the maker's ask), a stacked pair right after the label
    Box mr = lay_split(cc, EDGE_LEFT, 42, &cc);
    { Box rc = lay_inset(mr, 1); int mh = ((int)rc.h - 1) / 2;
      dmut_now = clatch(0x260u, (int)rc.x, (int)rc.y,      (int)rc.w, mh - 1, "MUT", &dmut_latch, &dmut_hold, CLR_ORANGE);
      drec_now = clatch(0x261u, (int)rc.x, (int)rc.y + mh, (int)rc.w, mh - 1, "REC", &drec_latch, &drec_hold, CLR_RED); }
    Box knobs = lay_split(cc, EDGE_LEFT, lay_clamp(6 * 28, 0, cc.w), &cc);   // the voice knobs; cc = the freed space to the right
    float *ktun = (sm == M_808) ? &dtune[sv]  : &d9tune[sv];
    float *kdec = (sm == M_808) ? &ddecay[sv] : &d9decay[sv];
    float *kcol = (sm == M_808) ? &dcolor[sv] : &d9color[sv];
    float *kvol = (sm == M_808) ? &dvol[sv]   : &d9vol[sv];
    float *kpan = (sm == M_808) ? &dpan[sv]   : &d9pan[sv];
    float *kfin = (sm == M_808) ? &dfine[sv]  : &d9fine[sv];
    float *K[6] = { ktun, kdec, kcol, kvol, kpan, kfin };
    const char *N[6] = { "TUN", "DEC", chn ? chn : "COL", "VOL", "PAN", "FINE" };
    for (int k = 0; k < 6; k++) r2_kcell(lay_grid(knobs, 6, 6, k, 1), K[k], N[k], 0.5f, hi);
    // the freed space to the right: the 909 METAL XY pad (tr909_metal highpass: X = cut, Y = res)
    if (sm == M_909) {
        Box rc = lay_inset(cc, 2);
        int pw = (int)rc.h - 6, px0 = (int)rc.x, py0 = (int)rc.y;
        void *w = ui_wid_hash(0x262u, px0, py0, pw, pw); ui_reg(w, px0, py0, pw, pw, 0);
        UiCap *cp = ui_cap_for(w);
        if (cp) { int px = cp->released ? cp->rx : cp->cx, py = cp->released ? cp->ry : cp->cy;
            float nx = clamp((px - px0) / (float)(pw - 1), 0, 1), ny = clamp(1.0f - (py - py0) / (float)(pw - 1), 0, 1);
            if (nx != m9cut || ny != m9res) { m9cut = nx; m9res = ny; tr909_metal(D909_BASE, m9cut, m9res); } }
        rrectfill(px0, py0, pw, pw, 2, CLR_BLACK);
        rrect(px0, py0, pw, pw, 2, mac[M_909].col);
        pset(px0 + (int)(m9cut * (pw - 1)), py0 + pw - 1 - (int)(m9res * (pw - 1)), CLR_LIGHT_YELLOW);
        font(FONT_TINY); plabel("METAL", px0 + pw / 2, py0 + pw + 1, mac[M_909].col);
    }
}

// a drum PAD row. 808 = the WHITE keys (16, full-width, light); 909 = the BLACK keys (11, narrower
// + spread across, centred over the 808's gaps, dark) — two SEPARATE rows that read as a keyboard.
// header (left) + FX trio (right) bracket the keys; both rows share the key x-span so they line up.
static void r2_drumstrip(Box c, int focus) {
    int nv = (focus == M_808) ? TR_NV : TR9_NV, black = (focus == M_909);
    int (*grid)[STEPS] = (focus == M_808) ? dgrid : d9grid;
    const char **vn = (focus == M_808) ? AB8 : AB9;
    float *trig = (focus == M_808) ? dtrig : d9trig;
    int *mut = (focus == M_808) ? dmute : d9mute;
    int sel = (focus == M_808) ? dsel : d9sel;
    rrectfill((int)c.x, (int)c.y, (int)c.w, (int)c.h, 2, mac[focus].lo);             // 2px frame (fill-and-punch)
    rrectfill((int)c.x + 2, (int)c.y + 2, (int)c.w - 4, (int)c.h - 4, 2, R2_PNL);
    Box cc = lay_inset(c, 2);
    // A/B/C/D pattern banks BEFORE the header (a 2×2 block at the far-left edge) — the drum twin of
    // the 303 column banks (lit = current, blink = armed while playing).
    Box bnk = lay_split(cc, EDGE_LEFT, 26, &cc);   // pattern banks — wider now (taken from the header) so A/B/C/D aren't cramped
    { static const char *BK[NPAT] = { "A", "B", "C", "D" };
      for (int k = 0; k < NPAT; k++) { Box bc = lay_grid(bnk, 2, NPAT, k, 1);
          int lit = (curpat[focus] == k) || (armpat[focus] == k && ((int)g_phase & 1));
          if (cbtn(0x1C0u + focus * 8 + k, (int)bc.x, (int)bc.y, (int)bc.w - 1, (int)bc.h - 1, BK[k], lit)) pat_queue(focus, k); } }
    r2_header(lay_split(cc, EDGE_LEFT, 22, &cc), focus);
    Box fx = lay_split(cc, EDGE_RIGHT, 66, &cc);
    float *dst = (focus == M_808) ? &dist8 : &dist9;
    r2_kcell(lay_grid(fx, 3, 3, 0, 1), dst,            "DST", 0.0f,  mac[focus].col);
    r2_kcell(lay_grid(fx, 3, 3, 1, 1), &msend[focus],  "SND", 0.10f, mac[focus].col);
    r2_kcell(lay_grid(fx, 3, 3, 2, 1), &fxverb[focus], "VRB", 0.0f,  mac[focus].col);
    // the keys — tap = select + audition-when-stopped. Same 16-slot pitch on both rows → aligned.
    int x0 = (int)cc.x, py = (int)cc.y + 1, ph = (int)cc.h - 2, ww = (int)cc.w / 16;
    for (int v = 0; v < nv; v++) {
        int cx, pw;
        if (black) { int b = (int)((v + 0.5f) * 16.0f / nv + 0.5f);   // 11 black keys spread over the 16-slot span
                     pw = (ww * 7) / 10; if (pw < 8) pw = 8; cx = x0 + b * ww - pw / 2; }
        else       { pw = ww - 3; cx = x0 + v * ww + 1; }   // 808 white keys: a touch narrower (more gap, less padded)
        int active = 0; for (int s = 0; s < STEPS; s++) if (grid[v][s]) { active = 1; break; }
        int firing = trig[v] > 0.05f, pr = 0, hot = 0, fo = 0;
        void *w = ui_wid_hash(0xB0u + focus * 32 + v, cx, py, pw, ph);
        if (ui_button_core(w, cx, py, pw, ph, &fo, &pr, &hot)) {
            if (dmut_now) mut[v] = !mut[v];                       // MUT latch live → a tap mutes the voice directly
            else if (drec_now && playing) {                      // REC latch live → punch onto the current step + hear it
                int st = lpos[0]; grid[v][st] = 1; r2_selmach = focus;
                if (focus == M_808) { tr808_fire(TR808_BASE, v, 1, 0, dtune, ddecay, dcolor); dtrig[v] = 1; }
                else                { tr909_fire(D909_BASE, v, 1, 0, d9tune, d9decay, d9color); d9trig[v] = 1; }
            } else {                                             // default: SELECT + audition-when-stopped
                r2_selmach = focus;
                if (focus == M_808) { dsel = v; if (!playing) { tr808_fire(TR808_BASE, v, 1, 0, dtune, ddecay, dcolor); dtrig[v] = 1; } }
                else                { d9sel = v; if (!playing) { tr909_fire(D909_BASE, v, 1, 0, d9tune, d9decay, d9color); d9trig[v] = 1; } }
            }
        }
        int selhere = (v == sel && focus == r2_selmach), fill, ink;
        if (black) { fill = firing ? CLR_WHITE : (mut[v] ? CLR_DARKER_PURPLE : (active ? mac[focus].col : CLR_BROWNISH_BLACK)); ink = active ? CLR_BLACK : mac[focus].col; }
        else       { fill = firing ? CLR_WHITE : (mut[v] ? CLR_DARKER_PURPLE : (active ? mac[focus].col : CLR_LIGHT_GREY));     ink = CLR_BLACK; }
        rectfill(cx, py, pw, ph, fill);
        rect(cx, py, pw, ph, dmut_now ? CLR_ORANGE : (hot || selhere) ? CLR_WHITE : mac[focus].lo);   // orange rim = MUT live
        if (mut[v]) line(cx, py, cx + pw, py + ph, CLR_RED);
        // the 2-letter abbreviation in the chunky dos 8x8 font where it fits (wide 808 white keys),
        // falling back to FONT_TINY on the narrow 909 black keys / narrow tablets
        font(FONT_NORMAL); int lw = text_width(vn[v]), gh = 8;
        if (lw > pw - 1) { font(FONT_TINY); lw = text_width(vn[v]); gh = 5; }
        print(vn[v], cx + (pw - lw) / 2, py + (ph - gh) / 2, ink);
    }
}

static void draw_rack2(Box area) {
    Box stage = area;
    Box bar = lay_split(stage, EDGE_TOP, 16, &stage);          // transport
    Box leg = lay_split(stage, EDGE_BOTTOM, 8, &stage);        // legend
    float dh = stage.h * 0.125f;                               // each drum key-row (808 white / 909 black)
    Box d808 = lay_split(stage, EDGE_BOTTOM, dh, &stage);      // white keys at the very bottom
    Box d909 = lay_split(stage, EDGE_BOTTOM, dh, &stage);      // black keys just above
    // middle band: [303a | 303b | SCREEN | MST]. 303a/303b are knob-columns on the left, the MASTER
    // column is on the RIGHT (classic mixer layout), and the shared SCREEN sits between them. The
    // columns take the FULL band height; the shared VOICE context row is carved from the screen region
    // only, so it lines up UNDER the screen instead of stretching across the whole width.
    float colw = lay_clamp(area.w * 0.11f, 40, 58);   // narrower (small-disc knobs) → the shared pattern screen gets the width
    Box c3a = lay_split(stage, EDGE_LEFT, colw, &stage);       // 303a / 303b on the left…
    Box c3b = lay_split(stage, EDGE_LEFT, colw, &stage);
    Box cms = lay_split(stage, EDGE_RIGHT, colw, &stage);      // …MASTER on the RIGHT (mixer convention)
    Box ctx = lay_split(stage, EDGE_BOTTOM, lay_clamp(stage.h * 0.19f, 32, 44), &stage);   // context row under the screen
    Box scr = stage;                                           // the shared screen sits between 303b and MST
    // transport bar — PLAY on the LEFT, HOME (house) on the RIGHT, same spots as the phone nav strip
    rrectfill((int)bar.x, (int)bar.y, (int)bar.w, (int)bar.h, 2, CLR_DARK_BROWN);
    int ph = (int)bar.h - 4, py = (int)bar.y + 2;
    {   // PLAY / STOP (left)
      int pw = 24, px = (int)bar.x + 2, pr = 0, hot = 0, fo = 0;
      void *wid = ui_wid_hash(0x2Fu, px, py, pw, ph);
      if (ui_button_core(wid, px, py, pw, ph, &fo, &pr, &hot)) { playing = !playing; laststep = -1; laststep303[0] = laststep303[1] = -1; for (int m = 0; m < M_N; m++) armpat[m] = -1; }
      rrectfill(px, py, pw, ph, 2, playing ? CLR_TRUE_BLUE : CLR_DARK_BROWN);
      rrect(px, py, pw, ph, 2, hot ? CLR_WHITE : CLR_BROWNISH_BLACK);
      int cx = px + pw / 2, cy = py + ph / 2;
      if (playing) { rectfill(cx - 3, cy - 2, 2, 5, CLR_WHITE); rectfill(cx + 1, cy - 2, 2, 5, CLR_WHITE); } else trifill(cx - 2, cy - 3, cx - 2, cy + 3, cx + 3, cy, CLR_WHITE);
      font(FONT_NORMAL); print("TINY ACID JAM", px + pw + 5, (int)bar.y + (int)bar.h / 2 - 4, CLR_LIGHT_PEACH); }
    {   // THEME toggle — a tiny square just left of HOME; a swatch of the OTHER skin, flips salmon ⇄ dark
      int tw = ph, tx = (int)(bar.x + bar.w) - 22 - 3 - tw - 3, pr = 0, hot = 0, fo = 0;
      void *w = ui_wid_hash(0x2Du, tx, py, tw, ph);
      if (ui_button_core(w, tx, py, tw, ph, &fo, &pr, &hot)) r2_dark = !r2_dark;
      rrectfill(tx, py, tw, ph, 2, CLR_DARK_BROWN); rrect(tx, py, tw, ph, 2, hot ? CLR_WHITE : CLR_BROWNISH_BLACK);
      rectfill(tx + tw / 2 - 2, py + ph / 2 - 2, 4, 4, r2_dark ? CLR_LIGHT_PEACH : CLR_DARKER_GREY); }
    {   // HOME (right) — the house glyph, exactly as the phone nav; taps back to the phone single-face view
      int hw = 22, hx = (int)(bar.x + bar.w) - hw - 3, prh = 0, hoh = 0, foh = 0;
      void *wh = ui_wid_hash(0x2Eu, hx, py, hw, ph);
      if (ui_button_core(wh, hx, py, hw, ph, &foh, &prh, &hoh)) rack_view = 0;
      rrectfill(hx, py, hw, ph, 2, CLR_DARK_BROWN); rrect(hx, py, hw, ph, 2, hoh ? CLR_WHITE : CLR_BROWNISH_BLACK);
      int cxh = hx + hw / 2, cyh = py + ph / 2 - 2;
      pset(cxh, cyh, CLR_LIGHT_PEACH);
      rectfill(cxh - 1, cyh + 1, 3, 1, CLR_LIGHT_PEACH);
      rectfill(cxh - 2, cyh + 2, 5, 1, CLR_LIGHT_PEACH);
      rectfill(cxh - 2, cyh + 3, 5, 3, CLR_LIGHT_PEACH);
      rectfill(cxh,     cyh + 4, 1, 2, CLR_DARK_BROWN); }
    // the machines
    r2_col303(c3a, 0);
    r2_col303(c3b, 1);
    r2_bigscreen(scr, r2_focus);
    r2_colmst(cms);
    r2_ctxrow(ctx);
    { Box d = d909; d.x += 1; d.w -= 2; r2_drumstrip(d, M_909); }   // black keys (top)
    { Box d = d808; d.x += 1; d.w -= 2; r2_drumstrip(d, M_808); }   // white keys (bottom)
    font(FONT_TINY); print("STICKY FOCUS \x7f tap a nameplate to put that machine on the big screen; play stays live", (int)leg.x + 3, (int)leg.y + 1, R2_DIM);
    font(FONT_SMALL);
}
#undef R2_INK
#undef R2_DIM
#undef R2_PNL

void draw(void) {
    // seed each line's EFFECTIVE lens from its persistent LATCH (so a latched lens holds across
    // faces + while another line is focused); the focused PERF screen then adds momentary-hold on top.
    for (int i = 0; i < 2; i++) {
        pf_half[i]  = pf_latch[PL_HALF][i];  pf_acc[i]  = pf_latch[PL_ACC][i];  pf_oct[i] = pf_latch[PL_OCT][i];
        pf_stac[i]  = pf_latch[PL_STAC][i];  pf_glide[i] = pf_latch[PL_GLIDE][i];
        pf_rev[i]   = pf_latch[PL_REV][i];   pf_roll[i]  = pf_latch[PL_ROLL][i];
    }
    for (int m = 0; m < 2; m++) {   // drum PERF holds when its screen isn't showing
        pf_rp1[m]  = dpf_latch[DPL_RP1][m];  pf_rp2[m]  = dpf_latch[DPL_RP2][m];  pf_rp4[m]  = dpf_latch[DPL_RP4][m];
        pf_thin[m] = dpf_latch[DPL_THIN][m]; pf_busy[m] = dpf_latch[DPL_BUSY][m]; pf_dacc[m] = dpf_latch[DPL_ACC][m];
    }
    // DEVICE CLASS — classify ONCE on the first frame, BEFORE we shrink the canvas below
    // (our own de_resize makes de_sw/de_sh tiny, so device_class() would then read WIDE
    // forever; frame 0 still reports the physical screen). ROOMY (tablet) → the full rack.
    if (rack_view < 0) rack_view = (device_class() == 2) ? 2 : 0;   // tablet → ROOMY, phone → single-face; HOME toggles it (frame 0, before we shrink the canvas)
    // CANVAS: phone keeps the chunky "scale up 160×100, spread the leftover" density (one face);
    // ROOMY holds a fixed HEIGHT tall enough for 2 instrument rows + a full master strip, and
    // matches WIDTH to the window ratio so the rack FILLS the window without clipping.
    { int cw = screen_w(), ch = screen_h();
      if (cw > 0 && ch > 0) {
          int tw, th;
          if (rack_view) {                                             // ROOMY — the rack
              th = 320; tw = (int)(320.0f * (float)cw / (float)ch + 0.5f);
              if (tw < 380) tw = 380;                                  // keep 2 landscape instruments legible
          } else {                                                     // phone — the chunky single face
              float r = (float)cw / (float)ch;
              if (r >= 1.6f) { th = 100; tw = (int)(100.0f * r + 0.5f); }
              else           { tw = 160; th = (int)(160.0f / r + 0.5f); }
          }
          if (tw != cw || th != ch) de_resize(tw, th);
      } }

    cls(CLR_DARK_PURPLE);                       // the candy shell — BLEEDS to every screen edge (fills margins, no black bars)

    // device-adaptive frame: the chassis fills the safe area (notch/home-bar dodged on device,
    // whole canvas on desktop), then LANDSCAPE reflow — one focused face spread to fill. See
    // device-adaptive-layout.md; acidwire is the layout reference. (808/909/MST interiors still
    // author at 160×100 — being reflowed next; the 303 is the first re-landed face.)
    // The candy CHASSIS bleeds to the FULL screen (under the notch / home-bar) so there are NO
    // purple or black margins — only the CONTROLS stay inside the safe area (dodge the notch).
    // On desktop safe_rect == the whole canvas, so this is identical to filling the canvas.
    Box full = box(0, 0, screen_w(), screen_h());
    rrectfill((int)full.x, (int)full.y, (int)full.w, (int)full.h, 7, CLR_INDIGO);
    Box panel = lay_inset(full, 3);
    rrectfill((int)panel.x, (int)panel.y, (int)panel.w, (int)panel.h, 5, CLR_LIGHT_PEACH);
    blend(BLEND_AVG); line((int)panel.x + 4, (int)panel.y, (int)(panel.x + panel.w - 4), (int)panel.y, CLR_WHITE); blend_reset();

    ui_begin();
    font(FONT_SMALL);

    // Controls area = panel ∩ safe-area, so the controls clear the notch / home-bar while the chassis
    // bleeds full underneath. safe_rect() now RESCALES to our chunky canvas (engine fix), so this is a
    // SLIM correct inset — only where the notch actually is — not the old fat all-round border.
    int sx, sy, sw, sh; safe_rect(&sx, &sy, &sw, &sh);
    float ax0 = panel.x > sx ? panel.x : sx, ay0 = panel.y > sy ? panel.y : sy;
    float ax1 = (panel.x + panel.w) < (sx + sw) ? (panel.x + panel.w) : (float)(sx + sw);
    float ay1 = (panel.y + panel.h) < (sy + sh) ? (panel.y + panel.h) : (float)(sy + sh);
    Box area = box(ax0, ay0, ax1 - ax0, ay1 - ay0);
    // ARRANGEMENT (canvas-density-spectrum.md axis 2): ROOMY (iPad) shows the WHOLE rack at once;
    // phone (TALL/WIDE) shows one focused face reached through the nav-tab strip. Same faces, two modes.
    if (rack_view) {
        draw_rack2(area);                                            // ROOMY — the tablet view (sticky focus + one big shared screen)
    } else {
        Box stage;
        Box nav = lay_split(area, EDGE_TOP, area.h * 0.12f, &stage); // nav strip ≈ design 12/100
        navspine(nav);
        if (mac[face].kind == MK_303) draw_303(stage, face);
        else if (face == M_808)       draw_808(stage);
        else if (face == M_909)       draw_909(stage);
        else                          draw_mst(stage);              // M_MST
    }

    font(FONT_NORMAL);
    ui_end();
    cursor_draw(mouse_down(MOUSE_LEFT) ? CUR_GRAB : CUR_HAND);   // cute pixel hand — curls to a fist while grabbing; desktop-only (no-op on touch)
}
