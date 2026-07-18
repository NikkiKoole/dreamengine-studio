/* de:meta
{
  "slug": "acidcandy",
  "collection": ["device-face"],
  "title": "acid candy (160x100)",
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
    "detail": "Nav = faces (tap a cartridge body to FOCUS, tap its LED to MUTE from anywhere). Sound: two TB-303 lines on the shared runtime/acid303.h (303b an octave up = the bass+acid-lead duo), the full TR-808 on the extracted runtime/tr808.h and the TR-909 on runtime/tr909.h (both byte-honest to the tr808/tr909 carts). Each 303 face: the top gear-drag knob row is always the acid knobs — vanilla CUT/RES/ENV/DEC/ACC, or (via an inline DF switch at the row's right end, tinted the machine colour) a DEEP Devil Fish page (SUB octave-down sub / ADEC two-decay / SLDT slide time / TRK filter tracking + a SAW/SQR WAVE toggle that re-defines the oscillator). The green screen has soft-keys like the drums — SEQ (the LCD roll), FLAG (a palette NOTE/ACC/SLD/TIE/OCT+/OCT- you arm then paint on the bars — NOTE toggles the note itself so you add notes WITHOUT leaving for SEQ; loop LENGTH/polymeter is now a ▼ marker you DRAG in a lane above the note-bars, not a flag), FX (DRV/SEND/VERB as LCD-native green knobs), PERF (5 live-play lenses — HALF/2X speed, ACC accent-everything, STAC/GLIDE slide-flip — non-destructive; each is TAP=latch (persists across faces, white border) or HOLD=momentary), and GEN (a CLEAR + MIN/MID/BUSY generate menu). The 16 NOTE-BARS below (tap = on/off, drag up/down = pitch, snapped to a minor-pentatonic so it stays musical). Drum faces (808 blue, 909 amber): the TOP row is always the machine FX (DIST/SEND/VERB, the 909 also a METAL XY pad for the metal-voice highpass, tr909_metal) — no FX toggle. (SWING is one master SWG on the MST face, ReBirth-style — it shuffles the whole rack, drums AND both 303s, so they lock.) The green SCREEN is the voice inspector: four soft-keys pick its content (the mode is sticky — a voice-pad tap only re-picks the voice, so you can paint flags across the whole kit without re-selecting) — VCE (the default: the picked voice's TUNE + DEC as LCD-native green knobs, plus a voice-specific CHARACTER knob only where the machine has one — SNPY/THUD/TONE/RING on the 808, ATTK/SNPY/CLIK/TONE on the 909, faithful to tr808/tr909; voices without one show just the two) / KIT (the kit minimap) / FLAG (a depth palette) / GEN (a CLEAR + MIN/MID/BUSY generate menu) you arm then paint on the cells: ACC (per-step accent, a red marker + louder hit) / PROB (vertical-slide a cell for its trig-chance, 100/75/50/25 = a shorter bar, the RD-8 move) / STRK (909 only — cycle none→flam→drag→ratchet, blue pips, via tr909_fire_stroke) / and the per-step p-LOCKS TUN / DEC / <character> (each its own button — the palette is two rows: ACC/PROB(/STRK) on top, the p-locks below; the character button appears only on voices that have one, e.g. SNPY). Arming a p-lock turns the cell lane into a BIPOLAR bar-from-centre lens editing that knob's per-step OFFSET (centre = follow the voice knob, up/down = ± its range); all three offsets play at once, each riding its voice knob so the knobs still transpose the whole contour. PROB and the p-locks DRAW ACROSS — drag over the row and each cell takes the finger's height, so a whole probability ramp or melodic/decay/timbre contour sweeps out in one gesture (the PCF-lane / note-bar feel). A single-row voice picker holding the FULL roster (16 tiny 2-char pads on the 808, 11 on the 909, acidrack's acid order — full name in the VCE header), doubling as a per-voice MUTE grid under a MUT soft-key (muted = dimmed + red slash, silenced but kept in the pattern), and the 16 hits on the bottom in the iconic 808 quarter-colours. FX routing: a shared tempo-synced delay + per-machine reverb (303s → a warm hall, 808/909 → their own plates with the kick kept dry) + per-machine drive. MST: master GLU/FLT/RES/FB/PUMP + SWG (the ONE rack-wide swing, ReBirth's model), a 4-channel mix overview, delay TIME + a draggable master TEMPO (BPM readout, drives the step clock + the tempo-synced delay) + per-machine SEND. Knob feel: vertical = value, pull sideways for a fine gear, double-tap resets. Widgets that live INSIDE the LCD (the flag/depth palettes, NEW, the drum VCE voice knobs, the 303 FX knobs) use LCD-native variants — lcdbtn (pure green outline, filled when armed) + lcdknob (green outline + value arc) — so they read as part of the readout, not the candy chassis. (The old step-row + keybed is kept behind use_bars = 0.)",
    "controls": "Tap a cartridge to focus a machine; tap its LED to mute. PLAY runs the shared transport. 303: drag CUT/RES/ENV/DEC/ACC (sideways = fine, double-tap = reset); the inline DF switch (right of the knob row) flips to the DEEP page (SUB/ADEC/SLDT/TRK + a SAW/SQR WAVE toggle); SEQ/FLAG/FX/GEN soft-keys switch the screen between the roll, the flag palette (arm ACC/SLD/TIE/OCT+/OCT-, then tap bars; loop length = drag the ▼ above the bars), the FX knobs (DRV/SEND/VERB), and the generate menu (CLEAR / MIN / MID / BUSY); on the note-bars tap = note on/off, drag up/down = pitch. 808/909: DIST/SEND/VERB (+909 METAL XY) sit on top always; tap a voice pad to pick+audition (the screen mode stays put — VCE by default shows its TUNE + DEC + a voice-specific character knob where the machine has one, SNPY/THUD/TONE/RING/ATTK/CLIK), the picker shows the whole roster in one row (all 16/11 voices, acid order); the MUT soft-key flips a pad tap from SELECT to per-voice MUTE; VCE/KIT/FLAG/GEN soft-keys switch the screen between the voice knobs, the minimap, the depth palette, and the generate menu (CLEAR / MIN / MID / BUSY). Cells: tap = place a hit, DRAG across = paint a fill (VCE/KIT); in FLAG, the palette is two rows — ACC/PROB(/STRK) on top, the p-locks TUN/DEC/<character> below — arm one then work the cells (ACC tap = accent, PROB vertical-slide = trig-chance, STRK tap = cycle flam/drag/ratchet, TUN/DEC/char vertical-slide = a per-step bipolar offset around that voice knob). MST: GLU tames level, FLT is the live DJ filter, PUMP ducks to the kick, SWG is the one rack-wide swing (drums + both 303s), pick a delay TIME, DRAG the BPM readout (right of the delay row) to set the master tempo, + per-machine SEND."
  },
  "todo": [
    "OPEN — LCD GROW: grow the green LCD glass a few px toward the bottom (more room for content). Per face — mind what sits just below: the 303's loop-length ▼ handle lane (y61-66) + note-bars (y67); check the drum/MST faces don't collide either. Small layout tweak, do it carefully with a before/after look.",
    "OPEN — REC / mode HINT OUTLINES: when you engage a mode (the drum REC pad-tool, or arm a FLAG), draw an OUTLINE around the buttons/cells you then need to press — spatial 'what do I do now' guidance. (Sibling idea, a transient LCD TOAST that NAMES the action, was prototyped 2026-07-18 and PARKED — maker didn't like it; don't rebuild without asking.)",
    "PARKED — mute SCENES: deprioritized 2026-07-18. For MACHINE-level muting it's largely redundant with the top-nav mute LEDs (one tap vs a few); the only real gain is one-tap recall of a COMPLEX state incl. per-voice drum mutes. The genuinely-more-than-muting version (mutes + which bank + levels + PERF = a 'verse/chorus/drop') IS the deferred SONG layer — build that, not a mute-only scene. If a cheap live win is wanted instead: a SOLO gesture (hold/latch a cartridge LED to solo, mute the rest) clearly beats the LEDs.",
    "OPEN — PERF lens follow-ups (live-set arc): (1) the 'funny accent order' for 2X — a cross-rhythm accent overlay (e.g. accent every 3rd 16th) so a doubled line isn't a flat repeat; (2) octave-shove + reverse lenses; (3) a PERF layer for the DRUMS too (whole-rack momentary total-accent + fills); (4) minor edge: a momentary HOLD of HALF while 2X is LATCHED still plays 2x (update precedence dbl>half / stac>glide) — momentary doesn't override a conflicting latch. LATCH itself SHIPPED 2026-07-18 (see below).",
    "808/909 completeness: SHIPPED (2026-07-16) — EVERY voice is a pad in ONE picker row (no pages): 808 = 16 tiny 2-char pads, 909 = 11 wider ones, in acidrack's ACID ORDER (kick/hats/clap/snare first, fills last). The full name shows in the VCE screen header on select. The KIT minimap is a full-roster density grid (all 16/11 voices as 1px rows). Pads DOUBLE-DUTY: a MUT soft-key flips a tap between SELECT+audition and per-voice MUTE (dmute/d9mute — dimmed + red slash, silenced in playback + dimmed in the minimap), on top of the machine mute. A muted pad still GHOST-PULSES (light-grey flash) on its would-be triggers — dtrig is set on the would-trigger, the fire() is what's gated by mute — so you can see the silenced pattern playing and time an unmute.",
    "drum DEPTH: SHIPPED (2026-07-16) a KIT/FLAG soft-key pair on both drum faces (the 303's SEQ/FLAG idiom) — FLAG fills the LCD with a depth palette (ACC/PROB, 909 also STRK) you arm then paint on the cells: per-voice-per-step ACCENT (fires boost +2, red marker in the gap above the cell), trig PROBABILITY (vertical-slide → snap_prob 100/75/50/25, shorter bar = lower chance, rnd()-gated in update()), and 909 STROKE (cycle none→flam→drag→ratchet via tr909_fire_stroke, blue pips). The 909 metal XY (m9cut/m9res, tr909_metal) is now a live XY pad in the 909 FX panel. NEW resets a voice's depth too. Still open: per-step accent SWEEP + a clearer stroke glyph than pips.",
    "p-LOCKS: SHIPPED (2026-07-16) — a LOCK flag in the drum FLAG palette. Arming it makes the cell lane a BIPOLAR per-step OFFSET lens (centre = follow the voice knob, up/down slide, drawn across); the offsets live in doff/d9off[LK_TUNE|LK_DEC|LK_CHAR] and are applied by swapping the voice's TUNE/DEC/CHAR knobs around each fire, so the voice knobs still transpose the whole contour. Each is its OWN button — the FLAG palette is now two rows (ACC/PROB(/STRK) on top, TUN/DEC/<char> below; the character button only shows on voices that have one). All three offsets play at once. FOLLOW-UPS: (1) a way to UNLOCK a step back to 0 (currently only NEW/GEN clears — maybe a plain tap zeroes it); (2) the same p-lock lens for the 303 knobs (CUT/RES).",
    "per-voice LANES (VOL + PAN): SHIPPED (2026-07-17) — the p-locks generalised into a data-driven LANE list (drum_lanes(): each lane = name + base-knob + per-step offset + stable id + sink; the FLAG palette AND the fire both read it, so paint/lens/palette are lane-generic and the darmed↔lane map is by stable id not row position). VOL = per-voice LEVEL folded into the per-hit velocity (boost) — truly per-voice, no engine change. PAN = per-voice STEREO via new additive tr808_pan()/tr909_pan() helpers (toms/congas GROUP-pan their shared slot — hardware-honest; independent tom/conga pan = the deferred 14-slot-bank split, device-face-paradigm §2c). MST SCP chip was a LIN/PWR pan-LAW toggle (PAN_LINEAR centre-full/mono-safe vs PAN_POWER equal-loudness); REMOVED 2026-07-18 once stereo was confirmed working — the rack is fixed at PAN_LINEAR. Two bugs fixed en route: dpan/d9pan were UNINITIALISED (defaulted hard-LEFT), and tr808_pan was pushed EVERY fire (queued set-and-hold → control-queue flood = clicks) — now cached + pushed only on change. Adding the next continuous lane = one drum_lanes row (+ an engine param to make it sound). FOLLOW-UPS: the MIX sub-view (next todo) now homes VOL/PAN so VCE is uncrowded again; the same lane system could still feed the 303 CUT/RES.",
    "drum MIX + FINE microtune & per-303 KEY: SHIPPED (2026-07-17). MIX: VCE is back to TONE (TUNE/DEC/[char]); a new MIX soft-key (drum faces, right col under GEN/MUT) holds the line-scope mixer VOL / PAN / FINE — un-crowds the 5-knob VCE. FINE = a per-voice ±50-cent MICROTUNE (dfine) via additive tr808_tune()/tr909_tune() (instrument_tune, push-on-change) so two kicks can be NULLED against each other; the coarse TUNE knob keeps its musical semitone steps (a continuous-TUNE experiment was built then DROPPED on play-test — user wanted best-of-both). KEY (melody): each 303 now owns its ROOT + SCALE + whole-line OCTAVE on a KEY soft-key (303 face, right col under GEN); mroot/mscale/loct are per-line [2]; KEY removed from MST (back to MIX/PCF). Scales min/maj-pent, minor, dorian, major, chromatic (generalised from the hardcoded PENTA); loct = ±2 whole-line octaves, DISTINCT from the per-step OCT flag. ALL default byte-identical (root0/min-pent/oct0/fine0; verified vs the prior render SHA). OPEN: the '2 303s can be in different keys' is intentional freedom with no coherence rail — a shared-key/link option could return if it bites; per-step 303 CUT/RES lanes still open.",
    "ARRANGEMENT: pattern BANKS SHIPPED (2026-07-17) — per-machine A/B/C/D via a PAT soft-key (303 right col; drums shrank the right col h7->h6 so GEN/MUT/MIX/PAT stack 4-high). A slot stores the SEQUENCE only (steps + flags/lanes + length), sound stays global; copy-on-switch (pat_switch). Empty slots seeded plen=STEPS/prob=100 so a fresh slot is playable. LIVE-SET (2026-07-18): bank switches are now BAR-QUANTIZED while playing — a bank tap ARMS the slot (pat_queue → armpat[machine]), the pad BLINKS on the 16th, and the switch LANDS on the next bar downbeat (step==0 in update(), before the 303/drum blocks read their arrays so the new pattern plays from step 0) — so the whole rack changes pattern in time (arm several machines, they flip together on the downbeat). Re-tap the current/armed slot = cancel; STOPPED = instant switch (no bar to wait for); stop drops pending arms. First live-set feature (queued banks = perform the arrangement by hand, the lightweight stand-in for the deferred SONG layer). Mute SCENES are the planned companion (parked for now). STILL OPEN, DEFERRED: the SONG/PROJECT layer (the maker's '6 songs in master' — 6 whole-rack snapshots + save). DEFERRED ON PURPOSE until the synths/feature-set STABILISE — persisting state that's still churning = a save format that rots every time a knob/lane/mode is added. When it's time: consolidate the ~40 scattered globals into ONE state struct FIRST, then save = a trivial versioned dump (save_bytes) instead of a hand-maintained 40-field blob. Also open: a SONG CHAIN (arrange banks over time) + WAV export.",
    "SWING: MASTER shuffle SHIPPED (2026-07-18) — the per-machine swing8/swing9 knobs are GONE; ONE g_swing (rack-scope) is now the SWG knob on the MST face (right of PUMP), ReBirth's model — one shuffle for the whole rack. Resolves BOTH old opens: (1) the 303s NOW swing too, so bass/lead lock into the drum shuffle; (2) it's tempo-scaled. Mechanism (the acidrack/tb303 split): drums CAN be scheduled ahead so they DRAG the odd-16th fire onset in ms (swms = sw*(15000/bpm), tempo-scaled); a 303's held voice CAN'T (slide/tie hold the handle), so it delays its step FLIP instead — laststep303 + ctr303 = ((ctr&1) && frac<sw) ? ctr-1 : ctr, and acid_gate gets the swung onset. BOTH use the SAME fraction sw = g_swing*0.60 (0..0.6 of a 16th) so they lock. g_swing 0 → ctr303==ctr → dead straight (default). SAVE/LOAD (autosave patterns/knobs) still open — acidrack has it.",
    "TEMPO: master BPM SHIPPED (2026-07-18) — was hardcoded 132 in THREE places (init bpm(), the step clock, the tempo-synced delay in apply_fx) + a 113ms stroke. Now ONE g_bpm (rack-scope) drives all of them, a draggable BPM readout on the MST face (right of the delay row, 60-200). Phase-accumulated (g_phase) so a live tempo change moves the RATE, never JUMPS the counter; the delay re-times on change (set-and-hold); the 303 screen shows the live value. OPEN: a tap-to-type / +/- for exact BPM; tempo-scale the swing + PCF stroke feel.",
    "PERF lenses: PROTOTYPE (2026-07-18) — a 2nd live-set feature, per-303. A new PERF soft-key (left col, recompacted to SEQ/FLAG/FX/PERF at h6) fills the LCD with 5 buttons (originally lcdhold momentary; now lcdlatch = TAP-latch/HOLD-momentary, see the PERF LATCH entry): HALF / 2X (speed), ACC (accent every note, the 909 move), STAC / GLIDE (slide-flip). All are NON-DESTRUCTIVE lenses over the READ path (pattern arrays untouched → clear = back to normal). Speed = a per-line phase multiplier (g_phase*spd), clean 2:1 so it stays phase-locked to the drums; laststep303 became [2] (each line its own swung, speed-lensed counter). GLIDE skips the staccato acid_gate so notes sustain; STAC forces slide=0; ACC ORs accent in. OPEN follow-ups are now their own top-of-list entry ('OPEN — PERF lens follow-ups': latch, funny-accent-order, octave/reverse, a drums PERF layer).",
    "SLIDE-GATE BUGFIX (2026-07-18, found via the GLIDE-lens question): the 303 staccato acid_gate was called UNCONDITIONALLY between flips — it cut the held voice at 70% of EVERY step, sparing only a cut-into-a-tie (next_ties). So a slid step got cut before it could glide → the next step retriggered FRESH instead of sliding: acidcandy's manual slides NEVER actually glided (and tie steps got chopped at 70% too). tb303 had it right all along (`else if (on[s] && !sld[s])`); acidcandy dropped that guard. FIX: gate only a fresh, NON-slid note — `if (on[i][ls] && !slide)` (slide = pf_stac ? 0 : sld[ls]; STAC still forces staccato). PROVEN by WAV correlate: manual all-SLD now == a pure glide (1.00000), and a no-slide/no-tie pattern is byte-identical before/after (1.00000) so only slid/tied steps change. acidrack is on its own step_303/gate_303 path and was never affected. NB: this changes how existing SLID/TIED committed patterns sound (they now glide/hold instead of chop) — the correct 303 behaviour.",
    "PERF LATCH: SHIPPED (2026-07-18) — the PERF buttons are now TAP=latch / HOLD=momentary (lcdhold → lcdlatch). A quick tap (release within PERF_TAP=12 frames) TOGGLES a persistent per-lens-per-line latch (pf_latch[PL_N][2]); a longer press is momentary (on while held, no toggle). The latch PERSISTS across faces + stop/start, so 303a can keep doubling while you work 303b — the whole point (momentary alone only touched the FOCUSED line). draw() seeds each line's effective pf_* from its latch, then the focused PERF screen's lcdlatch adds hold on top. HALF↔2X and STAC↔GLIDE are mutually exclusive (tapping one clears its partner via the excl arg). Latched = WHITE border, momentary = green fill. EDGE (noted in the follow-ups todo): a momentary hold can't override a conflicting latch (update precedence dbl>half, stac>glide).",
    "FLAG note-on: SHIPPED (2026-07-18) — added FL_NOTE as the FIRST entry in the 303 flag palette (NOTE/ACC/SLD/TIE/OCT+/OCT-) and made it the DEFAULT armed flag. Arming NOTE makes a bar tap/drag toggle the note itself (flag_get/set → on[i][s]), so you add notes right on the FLAG screen instead of maneuvring to SEQ and back — the maker's repeated ask. Pitch still lives in SEQ (drag) — NOTE keeps the bar's existing pitch. paint-across works (grab an off bar → paints on, grab an on bar → erases).",
    "drum-machine ergonomics: (1) CLEAR — SHIPPED (2026-07-16) as part of a GEN flow. Every instrument has a GEN soft-key (right margin, replacing the decorative SCP) that fills the LCD with a CLEAR + MIN/MID/BUSY menu — CLEAR empties the pattern, the three densities regenerate it (MID = the old NEW recipe). Replaces the single NEW button. (2) LIVE RECORD — still open: while the loop plays, a mode where tapping a voice pad punches its hit onto the CURRENT step (step/overdub record), instead of only auditioning.",
    "waveshape flip: SHIPPED (2026-07-16) a SAW/SQR WAVE toggle in the 5th knob slot of the 303 DEEP page (flips a.wave + re-calls acid_define). Still open: the drvmode waveshaper (SOFT/HARD/FOLD/ASYM) could get the same treatment.",
    "303 DEEP knobs: SHIPPED (2026-07-15) a DF soft-key flips the zone-2 row between vanilla (CUT/RES/ENV/DEC/ACC) and a DEEP page (SUB/ADEC/SLDT/TRK) — the headline Devil Fish knobs, per-303, sub-osc wired on slots 36/37 (34/35 belong to the 909's 13-slot bank). DRV moved to the FX panel (aligns with the drums). Still not surfaced: ATK (soft attack) + SQL (env-sweep) + the accent-SWEEP mode; a fuller flow could page those too.",
    "wire the still-decorative soft-keys: SHIPPED (2026-07-15) the per-machine FX panel — the FX soft-key on every 303/808/909 opens an aligned DRV/DIST + SEND (shared delay) + VERB (reverb) row (draw_fxrow); reverb is real (303s → warm hall tank 0, 909 → tight plate tank 1, 808 → room tank 2, kicks kept dry) + per-machine drum drive. every soft-key is now live: the 303/drum SCP slots → GEN (2026-07-16); MST SCP → the LIN/PWR pan-LAW toggle; MST SNG removed (SONG layer deferred); the 909 METAL XY shipped into its FX panel; MUT/REC are arm-toggles by the voices.",
    "SOUL: the LCD screens have no MASCOT yet (the slime from tinyface/facemock, candy-style §3) — a FACE flow could give a bopping/reacting creature its own screen. Deferred for now.",
    "graduate the gear-drag knob (vertical=value, sideways=fine gear, double-tap=reset) into ui.h's ui_knob so every cart gets it.",
    "note-bars: a CHROMATIC toggle (out-of-key acid) + let a drag PAINT across several bars; MST could grow per-machine level faders.",
    "DRUM-FACE middle-panel cleanup — 808 + 909 (SHIPPED 2026-07-18): split the controls into TOOL vs VIEW. The pad TOOL (what a voice-pad tap does — PICK select+audition→shows TONE / PLAY just fire the voice (finger-drum, no select) / MUTE mute / REC punch) is now a TALL, NARROW 4-way selector down the FAR-RIGHT edge, flush against the pads it retargets; tap to cycle, the fill colour (PICK = the neutral brown button colour, PLAY green, MUTE orange, REC red) + the full word STACKED one letter per row (tiny font) show which — replacing both the cryptic tiny REC/MUT margin toggles (draw_arms, now DELETED) and the interim V/M/R letter-strip. The side soft-keys are PURE VIEWS — KIT/FLAG/MIX (left) + GEN/PAT (right); VCE is no longer a soft-key (the PICK tool snaps the screen to DS_VCE/TONE). Picker + hits are CENTRED (x6, matching the 303s) and pulled DOWN, which freed the corner below PAT. Went through mockups: left/right button columns, a GEN/PAT under-picker spill, a tall right slab (too heavy), a horizontal gap strip, rotated labels (too cramped), a corner rotary → landed on the far-right stacked-word button. 909 keeps its STRK flag + metal-XY. OPEN: (1) the freed corner below PAT wants a knob (a 4th tone param, or SWG moved down from the top row); (2) narrate the active MUTE/REC mode in the screen header too, as a second legibility cue."
  ]
}
de:meta */
#include "studio.h"
#include "ui.h"
#include "acid303.h"
#include "tr808.h"    // the shared, honest TR-808 voice bank (the 808 face's SOUND)
#include "tr909.h"    // ...and the TR-909 (the 909 face's SOUND)

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
    { "303", MK_303,  CLR_PINK,      CLR_DARK_PURPLE, 0 },   // pink 303 (bass)
    { "303", MK_303,  CLR_ORANGE,    CLR_DARK_ORANGE, 0 },   // orange 303 (lead) — told apart by colour
    { "808",  MK_DRUM, CLR_TRUE_BLUE, CLR_DARK_BLUE,   0 },
    { "909",  MK_DRUM, CLR_YELLOW,    CLR_DARK_ORANGE, 0 },
    { "MST",  MK_MST,  CLR_GREEN,     CLR_DARK_GREEN,  0 },
};
static int face = M_303A;

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
enum { FL_NOTE, FL_ACC, FL_SLD, FL_TIE, FL_OCTU, FL_OCTD, FL_N };   // FL_NOTE = toggle the note itself (so you add notes WITHOUT leaving FLAG for SEQ). (LEN moved out — it's a per-LINE loop length, now a draggable handle at the end of the note-bars, not a per-step flag)
static int  armed = FL_NOTE;                 // which flag a bar-tap paints (default NOTE = add notes right from the FLAG screen)
static const char *FLNAME[FL_N] = { "NOTE", "ACC", "SLD", "TIE", "OCT+", "OCT-" };
enum { LK_TUNE, LK_DEC, LK_CHAR, LK_VOL, LK_PAN, LK_N };    // continuous-lane params (doff[] index). VOL = per-step level (velocity); PAN = per-step stereo
// drum-depth flags. STRK = 909-only. The continuous lanes (TUN/DEC/CHAR/VOL) map to LK_* by
// darmed = DD_TUN + lane.lk (a STABLE id, NOT the button's row position — a lane after the
// optional CHAR would otherwise land on the wrong slot). DD_* stays PARALLEL to LK_*.
enum { DD_ACC, DD_PROB, DD_STRK, DD_TUN, DD_DEC, DD_CHAR, DD_VOL, DD_PAN, DD_N };
static const char *DDNAME[DD_N] = { "ACC", "PROB", "STRK", "TUN", "DEC", "CHR", "VOL", "PAN" };
enum { DS_VCE, DS_KIT, DS_FLAG, DS_GEN, DS_MIX, DS_PAT };     // drum LCD: tone / kit / flags / generate / MIX (level·pan·fine) / PAT (A-D banks)
static int  dscreen = DS_VCE;                 // (VCE is entered by tapping a voice; KIT/FLAG are soft-keys)
static int  darmed = DD_ACC;                  // which drum flag a cell paints (FLAG mode)
// What a voice-pad tap DOES (drum faces) — one 4-way TOOL selector (the far-right button).
enum { PT_PICK, PT_PLAY, PT_MUTE, PT_REC };
static int  padtool = PT_PICK;                // PICK = select+audition (shows TONE) · PLAY = just fire the voice (finger-drum, no select) · MUTE = toggle voice mute · REC = punch onto the current step while playing
static int  dmute[TR_NV]  = { 0 };            // per-808-voice mute (in addition to the machine mute); toggled by LONG-PRESSING a voice pad
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
static int   mstflow = 0;                                   // MST screen: 0 = MIX meters, 1 = the PCF lane

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
static float g_phase = 0, g_last_t = 0;                     // accumulated 16th-note phase, so a live bpm change changes the RATE, never JUMPS the counter
static float g_swing = 0;                                   // master SWING (rack-scope, MST face) — one global shuffle for ALL machines (ReBirth's model); 0 = straight, delays the odd 16ths by up to 0.6 of a step

// PERF lenses — NON-DESTRUCTIVE performance transforms over the 303 read path (per line).
// The pattern arrays are untouched; the transport just READS them through whichever lens is
// active, so clearing it = instantly back to normal. Each lens is TAP=latch (persists, even
// across faces — 303a keeps doubling while you work 303b) OR HOLD=momentary (on while held).
// pf_* = the EFFECTIVE per-frame state read by the step clock; recomputed in draw() from the
// persistent latch + the focused line's held buttons.
enum { PL_HALF, PL_DBL, PL_ACC, PL_STAC, PL_GLIDE, PL_N };   // lens index (for pf_latch/pf_hold)
#define PERF_TAP 12                  // release within this many frames (~200ms) = a TAP (toggle latch); longer = a momentary HOLD
static int pf_latch[PL_N][2];        // persistent per-lens-per-line LATCH (tap-toggled; holds across faces + stop/start)
static int pf_hold[PL_N][2];         // frames each button has been held (for the tap-vs-hold split)
static int pf_half[2], pf_dbl[2];    // effective speed lens: half-time / double-time (clean 2:1 → stays phase-locked to the drums)
static int pf_acc[2];                // effective total-accent: every fired note gets the accent (the 909 "accent everything" move)
static int pf_stac[2], pf_glide[2];  // effective slide lens: STAC forces all slides OFF (plucky), GLIDE forces them ALL on (a smooth river)

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
    static float aFb = -1, aS[4] = { -1, -1, -1, -1 }, aComp = -1, aBpm = -1;
    static int   aDiv = -1, aMode = -2;
    if (mfb != aFb || mdiv != aDiv || g_bpm != aBpm) {     // the shared delay unit (re-times with tempo)
        static const float DIV[4] = { 0.25f, 0.5f, 0.75f, 1.0f };   // 1/16 · 1/8 · dotted · 1/4
        echo((int)(60000.0f / g_bpm * DIV[mdiv]), mfb * 0.72f, 0.45f);
        bpm((int)g_bpm);                                   // keep the engine musical clock synced on a tempo change
        aFb = mfb; aDiv = mdiv; aBpm = g_bpm;
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

// generate a 303 line at density d: 0=clear · 1=minimal · 2=classic · 3=busy
static void gen_line(int i, int d) {
    static const int pool[8] = { 0, 0, 0, 3, 5, 7, 10, 12 };
    plen[i] = STEPS;
    for (int s = 0; s < STEPS; s++) { on[i][s] = acc[i][s] = sld[i][s] = oct[i][s] = tie[i][s] = 0; }
    if (d == 0) return;                                                   // CLEAR = empty
    int onp = d == 1 ? 42 : d == 3 ? 90 : 72;                             // note density
    int accp = d == 1 ? 18 : d == 3 ? 45 : 30, sldp = d == 1 ? 12 : d == 3 ? 42 : 22;
    int prev = 0;
    for (int s = 0; s < STEPS; s++) {
        on[i][s] = rnd_between(0, 99) < onp;
        int p = pool[rnd_between(0, 7)];
        if (rnd_between(0, 99) < 35) p = prev;
        pit[i][s] = prev = p;
        acc[i][s] = rnd_between(0, 99) < accp;
        sld[i][s] = rnd_between(0, 99) < sldp;
        oct[i][s] = rnd_between(0, 99) < (d == 3 ? 18 : 10) ? 1 : 0;
        tie[i][s] = (!on[i][s] && rnd_between(0, 99) < 15) ? 1 : 0;
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
    ring(cx, cy, r - 2, r - 1, 165, 285, CLR_PINK);
    ring(cx, cy, r - 2, r - 1, -15, 105, CLR_DARKER_PURPLE);
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

static int cbtn(unsigned seed, int x, int y, int w, int hh, const char *s, int on2) {
    int pr = 0, hot = 0, foc = 0; void *wid = ui_wid_hash(seed, x, y, w, hh);
    int act = ui_button_core(wid, x, y, w, hh, &foc, &pr, &hot) && !drag_bounce(y, hh);
    int down = pr || on2;
    rrectfill(x, y, w, hh, 2, down ? CLR_TRUE_BLUE : CLR_DARK_BROWN);
    rrect(x, y, w, hh, 2, hot ? CLR_WHITE : CLR_BROWNISH_BLACK);
    font(FONT_TINY); print(s, x + (w - text_width(s)) / 2, y + 1, down ? CLR_WHITE : CLR_LIGHT_PEACH);
    return act;
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

// ── the cartridge nav strip (zone 1) ─────────────────────────────────────────
// each cartridge is a COMPOUND control: left body taps to FOCUS the face, the
// right LED taps to MUTE the machine (from any face). Two non-overlapping
// sub-buttons, so ui.h's visual-hit-wins routes touch cleanly.
// (Per-machine LEVEL lives on the MST mixer, not here — keeps the tabs compact.)
static void cartridge(int m) {
    int x = 19 + m * 25, y = 0, foc = (m == face), live = !mac[m].mute;  // y0 = tabs poke out the panel top (iOS edge gestures are deferred at the VC now)
    int prf = 0, hotf = 0, fof = 0, prm = 0, hotm = 0, fom = 0;
    void *wf = ui_wid_hash(0x70u + m, x, y, 16, 10);              // FOCUS = the name body
    void *wm = ui_wid_hash(0x80u + m, x + 16, y, 8, 10);          // MUTE = the LED pad (its hit-pad falls into the free space below the tabs)
    int af = ui_button_core(wf, x, y, 16, 10, &fof, &prf, &hotf);
    int am = ui_button_core(wm, x + 16, y, 8, 10, &fom, &prm, &hotm);
    int cleanf = nav_clean(wf), cleanm = nav_clean(wm);     // latch bounce-poison at GRAB (call every frame, not behind af/am)
    if (af && cleanf) face = m;                              // ignore a top-knob drag-release bounce onto the tab band
    if (am && cleanm) mac[m].mute = !mac[m].mute;

    rrectfill(x, y, 24, 10, 2, foc ? mac[m].col : mac[m].lo);
    if (foc) { blend(BLEND_AVG); line(x + 2, y + 1, x + 19, y + 1, CLR_WHITE); blend_reset(); }   // top sheen
    rrect(x, y, 24, 10, 2, (foc || hotf) ? CLR_WHITE : CLR_BROWNISH_BLACK);
    font(FONT_TINY);
    print(mac[m].name, x + (16 - text_width(mac[m].name)) / 2, y + 2, foc ? CLR_BROWNISH_BLACK : mac[m].col);
    int lx = x + 20, ly = y + 5;                                  // mute LED
    circfill(lx, ly, 2, live ? (foc ? CLR_LIME_GREEN : CLR_DARK_GREEN) : CLR_DARKER_PURPLE);
    circ(lx, ly, 2, (hotm && live) ? CLR_WHITE : CLR_BROWNISH_BLACK);
    if (!live) line(lx - 2, ly - 2, lx + 2, ly + 2, CLR_RED);     // muted = red slash
}

static void navspine(void) {
    int nc = mac[face].lo;                     // the top of the panel takes the FOCUSED voice's DARKER (nav-body) colour — an at-a-glance cue
    rrectfill(3, 2, 154, 10, 5, nc);           // …matching the panel's own rounded top corners (same x/w/radius)…
    rectfill(3, 7, 154, 5, nc);                // …with a flat bottom (same w=154 → right edge matches the rrectfill above)
    // transport (shared)
    int px = 5, py = 0, pw = 14, ph = 10, pr = 0, hot = 0, fo = 0;    // play, in from the left bezel
    void *w = ui_wid_hash(0x01u, px, py, pw, ph);
    int actp = ui_button_core(w, px, py, pw, ph, &fo, &pr, &hot), cleanp = nav_clean(w);
    if (actp && cleanp) { playing = !playing; laststep = -1; laststep303[0] = laststep303[1] = -1;
                          for (int m = 0; m < M_N; m++) armpat[m] = -1; }   // stop → drop any pending queued switches
    rrectfill(px, py, pw, ph, 2, playing ? CLR_TRUE_BLUE : CLR_DARK_BROWN);
    rrect(px, py, pw, ph, 2, hot ? CLR_WHITE : CLR_BROWNISH_BLACK);
    if (playing) { rectfill(px + 4, py + 3, 2, 4, CLR_WHITE); rectfill(px + 8, py + 3, 2, 4, CLR_WHITE); }
    else trifill(px + 5, py + 3, px + 5, py + 7, px + 10, py + 5, CLR_WHITE);
    for (int m = 0; m < M_N; m++) cartridge(m);

    // HOME (meta) — reserved space only; the app shell owns the real leave-cart gesture
    int hx = 143, hy = 0, hw = 12, hh = 10, hpr = 0, hhot = 0, hfo = 0;   // home, in from the right bezel
    void *wh = ui_wid_hash(0x03u, hx, hy, hw, hh);
    ui_button_core(wh, hx, hy, hw, hh, &hfo, &hpr, &hhot);            // registered but unwired
    rrectfill(hx, hy, hw, hh, 2, CLR_DARK_BROWN);
    rrect(hx, hy, hw, hh, 2, hhot ? CLR_WHITE : CLR_BROWNISH_BLACK);
    int cxh = hx + hw / 2;                                            // little house glyph
    trifill(cxh - 3, hy + 5, cxh + 3, hy + 5, cxh, hy + 2, CLR_LIGHT_PEACH);
    rectfill(cxh - 2, hy + 5, 5, 3, CLR_LIGHT_PEACH);
}

// the per-machine FX knob row (shown in zone 2 when a machine's FX panel is open):
// DRV/DIST · SEND · VERB, in the SAME three spots for every machine so they align.
// m: 0=303a 1=303b 2=808 3=909. SEND rides the shared delay (same value as MST); VERB
// = reverb send; the drive is the 303 voice DRV (rides live) or the drum kit DIST.
static void draw_fxrow(int m) {
    if (m < 2) knob(&ac[m].p[ACID_DRV], 40, 22, 6, "DRV",  0.35f);   // 303 voice drive
    else       knob(m == 2 ? &dist8 : &dist9, 40, 22, 6, "DIST", 0.0f);
    knob(&msend[m],  80, 22, 6, "SEND", m < 2 ? 0.10f : 0.0f);       // → the shared delay
    knob(&fxverb[m], 120, 22, 6, "VERB", 0.0f);                     // → the reverb
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

// ── a 303 face (zones 2–5) ───────────────────────────────────────────────────
static void draw_303(int i) {
    Acid *a = &ac[i];
    // ② the gear-drag knob row — ALWAYS the acid knobs. The inline DF switch (right end
    // of the row, tinted the machine colour — NOT a bezel side-button) flips the row
    // between the vanilla 303 and the DEEP page (Devil Fish). FX moved into the screen.
    if (!kpage[i]) {                                                       // page 0 — vanilla
        knob(&a->p[ACID_CUT], 20, 22, 6, "CUT", 0.55f); knob(&a->p[ACID_RES], 48, 22, 6, "RES", 0.70f);
        knob(&a->p[ACID_ENV], 76, 22, 6, "ENV", 0.55f); knob(&a->p[ACID_DEC], 104, 22, 6, "DEC", 0.45f);
        knob(&a->p[ACID_ACC], 132, 22, 6, "ACC", 0.55f);
    } else {                                                              // page 1 — DEEP: Devil Fish knobs + WAVE
        knob(&a->p[ACID_SUB],  20, 22, 6, "SUB",  0.0f);  knob(&a->p[ACID_ADEC], 48, 22, 6, "ADEC", 0.40f);
        knob(&a->p[ACID_SLDT], 76, 22, 6, "SLDT", 0.14f); knob(&a->p[ACID_TRK], 104, 22, 6, "TRK",  0.0f);
        int wx = 125, wy = 16, ww = 15, wh = 12, wp = 0, whot = 0, wf = 0;  // WAVE: SAW <-> SQUARE (bakes in → re-define)
        void *wv = ui_wid_hash(0x1Au, wx, wy, ww, wh);
        if (ui_button_core(wv, wx, wy, ww, wh, &wf, &wp, &whot)) { a->wave = (a->wave == INSTR_SAW) ? INSTR_SQUARE : INSTR_SAW; acid_define(a); }
        rrectfill(wx, wy, ww, wh, 2, CLR_INDIGO);
        rrect(wx, wy, ww, wh, 2, whot ? CLR_WHITE : CLR_BROWNISH_BLACK);
        font(FONT_TINY); plabel(a->wave == INSTR_SQUARE ? "SQR" : "SAW", wx + ww / 2, wy + 3, CLR_LIGHT_YELLOW);
        plabel("WAVE", wx + ww / 2, wy + wh, CLR_DARK_BROWN);
    }
    {   // the DF page switch — inline at the row's right end, machine-tinted, with an LED.
        int bx = 142, by = 16, bw = 14, bh = 12, dp = 0, dhot = 0, df = 0;
        void *wd = ui_wid_hash(0x07u, bx, by, bw, bh);
        if (ui_button_core(wd, bx, by, bw, bh, &df, &dp, &dhot)) kpage[i] = !kpage[i];
        rrectfill(bx, by, bw, bh, 2, kpage[i] ? mac[i].col : CLR_DARK_PURPLE);
        rrect(bx, by, bw, bh, 2, (dhot || kpage[i]) ? CLR_WHITE : CLR_BROWNISH_BLACK);
        font(FONT_TINY); plabel("DF", bx + bw / 2, by + 2, kpage[i] ? CLR_BROWNISH_BLACK : CLR_LIGHT_PEACH);
        circfill(bx + bw / 2, by + 9, 1, kpage[i] ? CLR_LIME_GREEN : CLR_DARKER_PURPLE);
    }

    // ③ display — soft-keys pick the LCD content: SEQ = the roll, FLAG = the flag
    // palette, FX = the DRV/SEND/VERB knobs, GEN (right) = the CLEAR + randomize menu.
    if (cbtn(0x08u, 6, 37, 16, 6, "SEQ",  pscreen[i] == PS_SEQ))  pscreen[i] = PS_SEQ;
    if (cbtn(0x09u, 6, 44, 16, 6, "FLAG", pscreen[i] == PS_FLAG)) pscreen[i] = PS_FLAG;
    if (cbtn(0x06u, 6, 51, 16, 6, "FX",   pscreen[i] == PS_FX))   pscreen[i] = PS_FX;
    if (cbtn(0x37u, 6, 58, 16, 6, "PERF", pscreen[i] == PS_PERF)) pscreen[i] = PS_PERF;   // live play lenses (hold-to-apply)
    if (cbtn(0x31u, 138, 38, 16, 7, "GEN", pscreen[i] == PS_GEN)) pscreen[i] = PS_GEN;
    if (cbtn(0x34u, 138, 47, 16, 7, "KEY", pscreen[i] == PS_KEY)) pscreen[i] = PS_KEY;   // this line's root / scale / octave
    if (cbtn(0x35u, 138, 56, 16, 7, "PAT", pscreen[i] == PS_PAT)) pscreen[i] = PS_PAT;   // A-D pattern banks (this line)
    rrectfill(24, 37, 112, 24, 3, CLR_BROWNISH_BLACK);
    rrectfill(27, 39, 106, 20, 2, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = 40; y < 58; y += 2) line(27, y, 132, y, CLR_BROWNISH_BLACK); blend_reset();
    if (pscreen[i] == PS_FLAG) {
        for (int f = 0; f < FL_N; f++)                                // the 6-flag palette IN the screen
            if (lcdbtn(0x0Au + f, 30 + (f % 3) * 34, 40 + (f / 3) * 9, 32, 8, FLNAME[f], armed == f)) armed = f;
    } else if (pscreen[i] == PS_FX) {                                 // the FX knobs, LCD-native
        font(FONT_TINY); plabel("FX", 80, 40, CLR_LIME_GREEN);
        lcdknob(&a->p[ACID_DRV], 46, 49, 5, "DRV",  0.35f);
        lcdknob(&msend[i],       80, 49, 5, "SEND", 0.10f);
        lcdknob(&fxverb[i],      114, 49, 5, "VERB", 0.0f);
    } else if (pscreen[i] == PS_GEN) {                                // CLEAR + randomize menu
        draw_gen(i);
    } else if (pscreen[i] == PS_KEY) {                                // KEY — THIS line's root (12-note strip) + scale + octave
        for (int k = 0; k < 12; k++) {                                // root strip, piano-coloured
            int sharp = (k == 1 || k == 3 || k == 6 || k == 8 || k == 10), px = 29 + k * 8, pw = 7;
            int pr = 0, hot = 0, foc = 0; void *w = ui_wid_hash(0x70u + k, px, 40, pw, 8);
            if (ui_button_core(w, px, 40, pw, 8, &foc, &pr, &hot)) mroot[i] = k;
            int lit = (mroot[i] == k);
            rrectfill(px, 40, pw, 8, 1, lit ? CLR_LIME_GREEN : sharp ? CLR_BROWNISH_BLACK : CLR_DARK_GREEN);
            rrect(px, 40, pw, 8, 1, (lit || hot) ? CLR_WHITE : CLR_DARK_GREEN);
            if (!sharp) { font(FONT_TINY); plabel(NOTE[k], px + pw / 2, 41, lit ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREEN); }
        }
        if (lcdbtn(0x78u, 29, 50, 40, 8, SCALES[mscale[i]].name, 0)) mscale[i] = (mscale[i] + 1) % NSCALE;   // tap = next scale
        font(FONT_TINY); plabel("OCT", 82, 50, CLR_DARK_BROWN);       // whole-line octave: - [n] +
        if (lcdbtn(0x79u, 92, 50, 9, 8, "-", 0) && loct[i] > -2) loct[i]--;
        { char ob[4]; ob[0] = loct[i] > 0 ? '+' : loct[i] < 0 ? '-' : ' '; ob[1] = '0' + (loct[i] < 0 ? -loct[i] : loct[i]); ob[2] = 0; plabel(ob, 108, 51, CLR_LIME_GREEN); }
        if (lcdbtn(0x7Au, 114, 50, 9, 8, "+", 0) && loct[i] < 2) loct[i]++;
    } else if (pscreen[i] == PS_PAT) {                                // PAT — A-D pattern banks for THIS 303 line
        static const char *AD[NPAT] = { "A", "B", "C", "D" };
        for (int k = 0; k < NPAT; k++)
            pat_pad(0x3Bu + k, 30 + k * 25, 42, 22, 14, AD[k], i, k);
    } else if (pscreen[i] == PS_PERF) {                              // PERF — momentary play LENSES (HOLD a button; release = normal)
        // TAP = latch (persists across faces) · HOLD = momentary. HALF↔2X and STAC↔GLIDE are
        // mutually exclusive (tapping one clears its partner). Effective state overrides the latch seed.
        pf_half[i]  = lcdlatch(0x7Bu, 30, 40, 24, 8, "HALF",  &pf_latch[PL_HALF][i],  &pf_hold[PL_HALF][i],  &pf_latch[PL_DBL][i]);   // ½-time (spreads out, 2 bars/loop)
        pf_dbl[i]   = lcdlatch(0x7Cu, 56, 40, 22, 8, "2X",    &pf_latch[PL_DBL][i],   &pf_hold[PL_DBL][i],   &pf_latch[PL_HALF][i]);  // 2×-time (doubles up, phase-locked)
        pf_acc[i]   = lcdlatch(0x7Du, 80, 40, 26, 8, "ACC",   &pf_latch[PL_ACC][i],   &pf_hold[PL_ACC][i],   0);                      // accent EVERY note (the 909 move)
        pf_stac[i]  = lcdlatch(0x7Eu, 30, 50, 34, 8, "STAC",  &pf_latch[PL_STAC][i],  &pf_hold[PL_STAC][i],  &pf_latch[PL_GLIDE][i]); // drop all slides → plucky
        pf_glide[i] = lcdlatch(0x7Fu, 66, 50, 40, 8, "GLIDE", &pf_latch[PL_GLIDE][i], &pf_hold[PL_GLIDE][i], &pf_latch[PL_STAC][i]);  // force all slides → a smooth river
    } else {
        { int bi = (int)(g_bpm + 0.5f); char nb[4]; int ni = 0;       // live BPM readout (set on the MST face)
          if (bi >= 100) nb[ni++] = '0' + bi / 100;
          nb[ni++] = '0' + (bi / 10) % 10; nb[ni++] = '0' + bi % 10; nb[ni] = 0;
          font(FONT_TINY); print(nb, 29, 40, CLR_MEDIUM_GREEN); }
        int heldy = -1;                                              // y of the sounding note (for ties + slide origin)
        for (int s = 0; s < plen[i]; s++) {
            int cx = 29 + s * 6;
            if (s == lpos[i] && playing) { blend(BLEND_AVG); rectfill(cx - 1, 40, 5, 18, CLR_MEDIUM_GREEN); blend_reset(); }
            int y = 56 - pit[i][s] - oct[i][s] * 6; if (y < 41) y = 41; if (y > 56) y = 56;
            if (on[i][s]) {
                rectfill(cx, y, 4, 2, acc[i][s] ? CLR_LIGHT_YELLOW : CLR_LIME_GREEN);   // the note
                if (oct[i][s] > 0) pset(cx + 2, y - 2, CLR_LIGHT_YELLOW);               // octave-up tick
                if (oct[i][s] < 0) pset(cx + 2, y + 3, CLR_TRUE_BLUE);                  // octave-down tick
                if (sld[i][s]) {                                                        // slide → a glide LINE to the next note
                    int ns = (s + 1) % plen[i];
                    if (on[i][ns] || tie[i][ns]) {
                        int ny = tie[i][ns] ? y : 56 - pit[i][ns] - oct[i][ns] * 6;
                        if (ny < 41) ny = 41; if (ny > 56) ny = 56;
                        line(cx + 3, y + 1, cx + 6, ny + 1, CLR_MEDIUM_GREEN);
                    }
                }
                heldy = y;
            } else if (tie[i][s] && heldy >= 0) {
                rectfill(cx, heldy, 5, 2, CLR_MEDIUM_GREEN);                            // tie → the held note carries on
            } else heldy = -1;                                                          // a rest breaks the hold
        }
    }

    static int use_bars = 1;   // 1 = the drag-to-pitch NOTE BARS; 0 = the old step row + keybed
    if (use_bars) {
        // ④⑤ the 16 NOTE BARS — tap = note on/off · drag up/down = pitch (scale-snapped).
        // One chunky surface; bar HEIGHT is the pitch, so you draw + see the melody.
        int by = 67, bh = 27;   // ~3px shorter than the panel floor → a clean strip below for the loop-length handle
        for (int s = 0; s < STEPS; s++) {
            int bx = 6 + s * 9, bw = 8, dead = (s >= plen[i]);
            void *w = ui_wid_hash(0xB0u + s, bx, by, bw, bh);
            ui_reg(w, bx, by, bw, bh, 0);
            UiCap *c = ui_cap_for(w);
            if (c) { g_drag_frame = ui_frame_ct; g_drag_y = c->cy; }
            if (pscreen[i] == PS_FLAG) {                 // FLAG mode: tap or DRAG paints the armed flag
                if (c) {                                 // the captured bar tracks the finger across the row
                    if (ui_grabbed(w)) paint_val = !flag_get(i, s, armed);
                    int fx = c->released ? c->rx : c->cx, cell = (fx - 6) / 9;
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
                if (drag_axis) {                                     // free draw: height = pitch, bottom = rest
                    int cell = (px - 6) / 9; if (cell < 0) cell = 0; if (cell >= STEPS) cell = STEPS - 1;
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
                int idx = scale_idx(mscale[i], pit[i][s]), fh = bh * (idx + 1) / SCALES[mscale[i]].n;
                rrectfill(bx, by + bh - fh, bw, fh, 1, acc[i][s] ? CLR_ORANGE : CLR_LIME_GREEN);
                if (sld[i][s]) {                                    // slide → bright top cap + a GLIDE LINE to the next note
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
            int ly = by + bh;                                                  // the clean strip below the bars
            void *wl = ui_wid_hash(0x2Au, 6, ly, 144, 100 - ly); ui_reg(wl, 6, ly, 144, 100 - ly, 0);
            UiCap *lc = ui_cap_for(wl);
            if (lc) { g_drag_frame = ui_frame_ct; g_drag_y = lc->cy;
                int fx = lc->released ? lc->rx : lc->cx, n = (fx - 6) / 9 + 1;
                if (n < 1) n = 1; if (n > STEPS) n = STEPS; plen[i] = n;
            }
            int bxr = 6 + plen[i] * 9, th = (lc != 0);
            line(6, ly + 1, bxr - 2, ly + 1, CLR_MEDIUM_GREEN);                // active-length track, into the handle
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
// voice pads, and the hits in the iconic 808 red·orange·yellow·white quarters.
static void draw_808(void) {
    // acid order (from acidrack): always-on kick/hats/clap/snare, then cowbell + rim/clave,
    // then the fill voices (maracas/cymbal/toms/congas) — page 1 is a whole beat on its own.
    static const int VL[TR_NV] = { TR_BD, TR_CH, TR_OH, TR_CP, TR_SD, TR_CB, TR_RS, TR_CLV,
                                   TR_MA, TR_CY, TR_LT, TR_MT, TR_HT, TR_LC, TR_MC, TR_HC };
    static const int QCLR[4] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_WHITE };

    // ② TOP ZONE (always): the machine FX row. Per-voice tone lives in the screen now
    // (tap a voice); SWING is now one master control on the MST face (was per-machine here).
    draw_fxrow(2);                                         // DIST · SEND · VERB (aligned with the 303s)

    // ③ screen — soft-keys pick the LCD content: VCE = the picked voice's knobs (also
    // shown by default), KIT = the kit minimap, FLAG = the depth palette, GEN (right) =
    // the CLEAR + randomize menu.
    if (darmed == DD_STRK) darmed = DD_ACC;                 // STRK is 909-only — reset if it was armed on the 909
    // the side columns are now ALL the SCREEN VIEW (what the LCD shows): the pad TOOL
    // (VCE/MUT/REC) moved to a 3-position selector by the cells (⑤), so the 5 views fit
    // the 6 column slots with room to spare — no more GEN/PAT spill.
    if (cbtn(0x20u,   6, 37, 16, 7, "KIT",  dscreen == DS_KIT))  dscreen = DS_KIT;    // LEFT col
    if (cbtn(0x21u,   6, 45, 16, 7, "FLAG", dscreen == DS_FLAG)) dscreen = DS_FLAG;
    if (cbtn(0x33u,   6, 53, 16, 7, "MIX",  dscreen == DS_MIX))  dscreen = DS_MIX;   // per-voice level·pan·fine
    if (cbtn(0x31u, 138, 38, 16, 7, "GEN",  dscreen == DS_GEN))  dscreen = DS_GEN;   // RIGHT col (tone = DS_VCE, reached via the VCE tool selector)
    if (cbtn(0x36u, 138, 47, 16, 7, "PAT",  dscreen == DS_PAT))  dscreen = DS_PAT;   // A-D pattern banks
    rrectfill(24, 37, 112, 24, 3, CLR_BROWNISH_BLACK);
    rrectfill(27, 39, 106, 20, 2, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = 40; y < 58; y += 2) line(27, y, 132, y, CLR_BROWNISH_BLACK); blend_reset();
    if (dscreen == DS_GEN) {
        draw_gen(2);
    } else if (dscreen == DS_FLAG) {
        if (darmed == DD_CHAR && !CH8[dsel]) darmed = DD_TUN;   // this voice has no character knob
        // row 1 — the depth flags; row 2 — the per-step p-locks (TUN/DEC/character)
        if (lcdbtn(0x24u + DD_ACC,  30, 40, 32, 8, "ACC",  darmed == DD_ACC))  darmed = DD_ACC;
        if (lcdbtn(0x24u + DD_PROB, 64, 40, 32, 8, "PROB", darmed == DD_PROB)) darmed = DD_PROB;
        Lane L[LK_N]; int nl = drum_lanes(M_808, dsel, L);      // row 2 = the continuous lanes (data-driven, id-stable, even-spaced)
        for (int li = 0; li < nl; li++) {
            int dd = DD_TUN + L[li].lk;                          // STABLE id (not row position) — safe past the optional CHAR
            if (lcdbtn(0x24u + dd, 27 + 106 * li / nl, 49, 106 / nl - 2, 8, L[li].name, darmed == dd)) darmed = dd;
        }
    } else if (dscreen == DS_KIT) {
        rrectfill(28, 40, 15, 7, 1, CLR_TRUE_BLUE); font(FONT_TINY); print("808", 30, 41, CLR_WHITE);   // identity badge
        for (int r = 0; r < TR_NV; r++) {                   // full-roster density grid — every voice SEEN
            int v = VL[r], gy = 42 + r;
            for (int s = 0; s < STEPS; s++) {
                int gx = 48 + s * 5;
                if (s == step && playing) { blend(BLEND_AVG); rectfill(gx - 1, 42, 4, TR_NV, CLR_MEDIUM_GREEN); blend_reset(); }
                if (dgrid[v][s]) rectfill(gx, gy, 3, 1, dmute[v] ? CLR_DARKER_GREY : v == dsel ? CLR_LIGHT_YELLOW : CLR_LIME_GREEN);
            }
        }
    } else if (dscreen == DS_MIX) {                         // MIX — per-voice LEVEL / PAN / FINE-tune (line-scope mixer)
        font(FONT_TINY); plabel(TR808_NAME[dsel], 80, 40, CLR_LIME_GREEN);
        lcdknob(&dvol[dsel],   46, 49, 5, "VOL",  0.5f);
        lcdknob(&dpan[dsel],   80, 49, 5, "PAN",  0.5f);
        lcdknob(&dfine[dsel], 114, 49, 5, "FINE", 0.5f);    // ±50 cents — null a kick beat; TUNE keeps its semitone steps
    } else if (dscreen == DS_PAT) {                         // PAT — A-D pattern banks for the 808
        static const char *AD[NPAT] = { "A", "B", "C", "D" };
        for (int k = 0; k < NPAT; k++)
            pat_pad(0x3Fu + k, 30 + k * 25, 42, 22, 14, AD[k], M_808, k);
    } else {                                                // DS_VCE — the voice's TONE knobs, LCD-native
        font(FONT_TINY); plabel(TR808_NAME[dsel], 80, 40, CLR_LIME_GREEN);
        float *kv[3]; const char *kl[3]; int nk = 0;        // TUNE / DEC / [char] — even-spaced (VOL/PAN/FINE now live in MIX)
        kv[nk] = &dtune[dsel];  kl[nk++] = "TUNE";
        kv[nk] = &ddecay[dsel]; kl[nk++] = "DEC";
        if (CH8[dsel]) { kv[nk] = &dcolor[dsel]; kl[nk++] = CH8[dsel]; }
        for (int i = 0; i < nk; i++) lcdknob(kv[i], 27 + 106 * (2 * i + 1) / (2 * nk), 49, 5, kl[i], 0.5f);
    }

    // ④ VOICE PICKER — ALL 16 voices in one row (acid order, 2-char pads), now CENTRED
    // (x8) + pulled DOWN by the cells; the TOOL vacated the right margin so the row runs
    // full-width. A tap SELECTs + auditions; in MUT mode it toggles mute (red slash).
    for (int r = 0; r < TR_NV; r++) {
        int v = VL[r], x = 6 + r * 9, selp = (v == dsel), mtd = dmute[v];
        void *wp = ui_wid_hash(0x90u + v, x, 68, 8, 9); ui_reg(wp, x, 68, 8, 9, 0);
        UiCap *c = ui_cap_for(wp); int hot = (c != 0);
        if (c) {
            if (padtool == PT_REC && playing) {                  // REC — punch onto the CURRENT step (multitouch)
                if (ui_grabbed(wp)) { dgrid[v][step] = 1; tr808_fire(TR808_BASE, v, 1, 0, dtune, ddecay, dcolor); dtrig[v] = 1; }
            } else if (padtool == PT_MUTE) {                     // MUTE — INSTANT mute toggle on tap (live-precise)
                if (ui_grabbed(wp)) dmute[v] = !dmute[v];
            } else if (padtool == PT_PLAY) {                     // PLAY — just fire the voice (finger-drum), no select
                if (ui_grabbed(wp)) { tr808_fire(TR808_BASE, v, 1, 0, dtune, ddecay, dcolor); dtrig[v] = 1; }
            } else if (c->released) {                            // PICK — SELECT (+ audition only when STOPPED)
                dsel = v; if (!playing) { tr808_fire(TR808_BASE, v, 1, 0, dtune, ddecay, dcolor); dtrig[v] = 1; }
            }
        }
        rrectfill(x, 68, 8, 9, 1, mtd ? CLR_DARKER_PURPLE : selp ? CLR_TRUE_BLUE : CLR_DARK_BLUE);
        if (dtrig[v] > 0) { blend(BLEND_AVG); rrectfill(x, 68, 8, 9, 1, mtd ? CLR_LIGHT_GREY : CLR_WHITE); blend_reset(); }   // trigger flash (ghost-grey when muted)
        rrect(x, 68, 8, 9, 1, (selp || hot) ? CLR_WHITE : CLR_BROWNISH_BLACK);
        if (mtd) line(x + 1, 69, x + 6, 75, CLR_RED);        // muted = red slash (like the cartridge LED)
        font(FONT_TINY); print(AB8[v], x + (8 - text_width(AB8[v])) / 2, 70, mtd ? CLR_DARKER_GREY : selp ? CLR_WHITE : CLR_BLUE);
    }
    // ④a the pad TOOL — a tall, narrow selector down the FAR-RIGHT edge, flush against the
    // pads it retargets (a held-mode belongs adjacent to its target). Tap to cycle
    // PICK→PLAY→MUTE→REC; the fill colour + the stacked word show which. The corner (below
    // PAT) is now free for a knob.
    {
        static const char *TL[4] = { "PICK", "PLAY", "MUTE", "REC" };   // the 4-way pad TOOL
        static const int    TC[4] = { CLR_DARK_BROWN, CLR_MEDIUM_GREEN, CLR_ORANGE, CLR_RED };   // PICK neutral · PLAY green · MUTE orange · REC red
        int bx = 150, by = 68, bw = 7, bh = 29;
        int pr = 0, hot = 0, foc = 0;
        void *w = ui_wid_hash(0xE0u, bx, by, bw, bh);
        if (ui_button_core(w, bx, by, bw, bh, &foc, &pr, &hot) && tap_settled()) {
            padtool = (padtool + 1) % 4; if (padtool == PT_PICK) dscreen = DS_VCE;   // PICK also snaps the screen to TONE
        }
        rrectfill(bx, by, bw, bh, 2, TC[padtool]);
        rrect(bx, by, bw, bh, 2, hot ? CLR_WHITE : CLR_BROWNISH_BLACK);
        const char *wd = TL[padtool]; int nl = 0; while (wd[nl]) nl++;   // stack the word one letter per row
        font(FONT_TINY);
        int ch = 6, ty = by + (bh - nl * ch) / 2;                    // 5px glyph + 1px lead, vertically centred
        for (int i = 0; i < nl; i++) { char s[2] = { wd[i], 0 }; print(s, bx + (bw - text_width(s)) / 2 + 1, ty + i * ch, CLR_WHITE); }
    }

    // ⑤ the HITS — the picked voice's 16 steps, on the BOTTOM (thumb surface).
    // KIT: tap = toggle · DRAG across = paint on/off (first cell decides). FLAG: paint
    // the armed flag — ACC toggles accent (bright cap); PROB vertical-slides the trig
    // chance (short bar = <100%, the tr808 gesture, gated to this mode).
    for (int s = 0; s < STEPS; s++) {
        int x = 6 + s * 9, here = (s == step && playing);
        void *ws = ui_wid_hash(0xA0u + s, x, 81, 8, 16);
        ui_reg(ws, x, 81, 8, 16, 0);
        UiCap *c = ui_cap_for(ws);
        if (c) {
            g_drag_frame = ui_frame_ct; g_drag_y = c->cy;
            int fx = c->released ? c->rx : c->cx, cell = (fx - 6) / 9;
            if (cell < 0) cell = 0; if (cell >= STEPS) cell = STEPS - 1;
            if (dscreen != DS_FLAG) {                        // VCE/KIT — paint on/off across the drag
                if (ui_grabbed(ws)) { paint_val = !dgrid[dsel][s]; if (paint_val) { tr808_fire(TR808_BASE, dsel, 1, 0, dtune, ddecay, dcolor); dtrig[dsel] = 1; mbop = 1; } }
                dgrid[dsel][cell] = paint_val;
            } else if (darmed == DD_ACC) {                   // FLAG/ACC — toggle accent, paint across
                if (ui_grabbed(ws)) { paint_val = !dacc[dsel][s]; if (paint_val) dgrid[dsel][s] = 1; }
                dacc[dsel][cell] = paint_val; if (paint_val) dgrid[dsel][cell] = 1;
            } else if (darmed == DD_PROB) {                  // FLAG/PROB — vertical slide, DRAWN across the row (finger height = chance)
                float f = clamp((94 - c->cy) / 13.0f, 0, 1);
                dprob[dsel][cell] = snap_prob(f); dgrid[dsel][cell] = 1;
            } else {                                         // FLAG/TUN|DEC|CHAR — bipolar p-lock offset, drawn across
                doff[darmed - DD_TUN][dsel][cell] = clamp((89 - c->cy) / 8.0f, -1, 1); dgrid[dsel][cell] = 1;
            }
        }
        int on2 = dgrid[dsel][s], locklens = (dscreen == DS_FLAG && darmed >= DD_TUN);
        int lp = locklens ? darmed - DD_TUN : 0;
        rrectfill(x, 81, 8, 16, 1, (here && !on2) ? CLR_DARKER_GREY : CLR_DARKER_PURPLE);
        if (on2 && locklens) {                               // LOCK lens — bipolar bar from centre (up = more, down = less)
            int cy = 89, op = (int)(doff[lp][dsel][s] * 7 + (doff[lp][dsel][s] >= 0 ? 0.5f : -0.5f));
            line(x, cy, x + 7, cy, CLR_DARKER_GREY);         // centre = the voice's knob
            int col = here ? CLR_WHITE : op ? CLR_LIGHT_YELLOW : CLR_MEDIUM_GREEN;
            if (op > 0)      rrectfill(x, cy - op, 8, op, 1, col);
            else if (op < 0) rrectfill(x, cy + 1, 8, -op, 1, col);
            else             rectfill(x + 2, cy, 4, 1, col);
        } else if (on2) {
            int pr = dprob[dsel][s] > 0 ? dprob[dsel][s] : 100, fh = 16 * pr / 100; if (fh < 3) fh = 3;
            rrectfill(x, 81 + 16 - fh, 8, fh, 1, here ? CLR_WHITE : QCLR[s / 4]);
            if (dacc[dsel][s]) rectfill(x + 1, 78, 6, 2, CLR_RED);   // accent marker in the gap above the cell
        }
        rrect(x, 81, 8, 16, 1, CLR_BROWNISH_BLACK);
    }
}

// ── the 909 DRUM face — same compact model as the 808, but its OWN identity:
// AMBER/steel (vs the 808's blue) so you know which drum machine you're on.
static void draw_909(void) {
    // acid order (from acidrack): kick/hats/clap/snare, then rim + ride/crash, then the fill toms.
    static const int VL[TR9_NV] = { TR9_BD, TR9_CH, TR9_OH, TR9_CP, TR9_SD, TR9_RS, TR9_RC, TR9_CC, TR9_LT, TR9_MT, TR9_HT };

    // ② TOP ZONE (always): FX row + the 909 METAL XY (right gutter). SWING is now one
    // master control on the MST face (was per-machine here).
    draw_fxrow(3);                                         // DIST · SEND · VERB (aligned with the 303s)
    {   // 909 METAL-filter XY — the shared highpass on the metal voices (hats/cymbal/cowbell).
        int px0 = 134, py0 = 15, pw = 18, ph = 15;         // right gutter. Drag: X = cut, Y = res.
        void *wp = ui_wid_hash(0xC0u, px0, py0, pw, ph);
        ui_reg(wp, px0, py0, pw, ph, 0);
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

    // ③ screen — soft-keys are PURE VIEWS (mirrors the 808): KIT/FLAG/MIX (left) + GEN/PAT
    // (right). VCE is no longer a soft-key — the PICK tool snaps the screen to DS_VCE/TONE.
    if (cbtn(0x20u,   6, 37, 16, 7, "KIT",  dscreen == DS_KIT))  dscreen = DS_KIT;    // LEFT col
    if (cbtn(0x21u,   6, 45, 16, 7, "FLAG", dscreen == DS_FLAG)) dscreen = DS_FLAG;
    if (cbtn(0x33u,   6, 53, 16, 7, "MIX",  dscreen == DS_MIX))  dscreen = DS_MIX;   // per-voice level·pan·fine
    if (cbtn(0x31u, 138, 38, 16, 7, "GEN",  dscreen == DS_GEN))  dscreen = DS_GEN;   // RIGHT col (tone = DS_VCE, reached via the PICK tool)
    if (cbtn(0x36u, 138, 47, 16, 7, "PAT",  dscreen == DS_PAT))  dscreen = DS_PAT;   // A-D pattern banks
    rrectfill(24, 37, 112, 24, 3, CLR_BROWNISH_BLACK);
    rrectfill(27, 39, 106, 20, 2, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = 40; y < 58; y += 2) line(27, y, 132, y, CLR_BROWNISH_BLACK); blend_reset();
    if (dscreen == DS_GEN) {
        draw_gen(3);
    } else if (dscreen == DS_FLAG) {
        if (darmed == DD_CHAR && !CH9[d9sel]) darmed = DD_TUN;   // this voice has no character knob
        // row 1 — the depth flags (incl. STRK); row 2 — the per-step p-locks
        if (lcdbtn(0x24u + DD_ACC,  30, 40, 32, 8, "ACC",  darmed == DD_ACC))  darmed = DD_ACC;
        if (lcdbtn(0x24u + DD_PROB, 64, 40, 32, 8, "PROB", darmed == DD_PROB)) darmed = DD_PROB;
        if (lcdbtn(0x24u + DD_STRK, 98, 40, 32, 8, "STRK", darmed == DD_STRK)) darmed = DD_STRK;
        Lane L[LK_N]; int nl = drum_lanes(M_909, d9sel, L);     // row 2 = the continuous lanes (data-driven, id-stable, even-spaced)
        for (int li = 0; li < nl; li++) {
            int dd = DD_TUN + L[li].lk;                          // STABLE id (not row position) — safe past the optional CHAR
            if (lcdbtn(0x24u + dd, 27 + 106 * li / nl, 49, 106 / nl - 2, 8, L[li].name, darmed == dd)) darmed = dd;
        }
    } else if (dscreen == DS_KIT) {
        rrectfill(28, 40, 15, 7, 1, CLR_ORANGE); font(FONT_TINY); print("909", 30, 41, CLR_BROWNISH_BLACK);
        for (int r = 0; r < TR9_NV; r++) {                  // full-roster density grid — every voice SEEN
            int v = VL[r], gy = 42 + r;
            for (int s = 0; s < STEPS; s++) {
                int gx = 48 + s * 5;
                if (s == step && playing) { blend(BLEND_AVG); rectfill(gx - 1, 42, 4, TR9_NV, CLR_MEDIUM_GREEN); blend_reset(); }
                if (d9grid[v][s]) rectfill(gx, gy, 3, 1, d9mute[v] ? CLR_DARKER_GREY : v == d9sel ? CLR_LIGHT_YELLOW : CLR_LIME_GREEN);
            }
        }
    } else if (dscreen == DS_MIX) {                         // MIX — per-voice LEVEL / PAN / FINE-tune
        font(FONT_TINY); plabel(TR909_NAME[d9sel], 80, 40, CLR_LIME_GREEN);
        lcdknob(&d9vol[d9sel],   46, 49, 5, "VOL",  0.5f);
        lcdknob(&d9pan[d9sel],   80, 49, 5, "PAN",  0.5f);
        lcdknob(&d9fine[d9sel], 114, 49, 5, "FINE", 0.5f);
    } else if (dscreen == DS_PAT) {                         // PAT — A-D pattern banks for the 909
        static const char *AD[NPAT] = { "A", "B", "C", "D" };
        for (int k = 0; k < NPAT; k++)
            pat_pad(0x43u + k, 30 + k * 25, 42, 22, 14, AD[k], M_909, k);
    } else {                                                // DS_VCE — the voice's TONE knobs, LCD-native
        font(FONT_TINY); plabel(TR909_NAME[d9sel], 80, 40, CLR_LIME_GREEN);
        float *kv[3]; const char *kl[3]; int nk = 0;        // TUNE / DEC / [char] — even-spaced (VOL/PAN/FINE now live in MIX)
        kv[nk] = &d9tune[d9sel];  kl[nk++] = "TUNE";
        kv[nk] = &d9decay[d9sel]; kl[nk++] = "DEC";
        if (CH9[d9sel]) { kv[nk] = &d9color[d9sel]; kl[nk++] = CH9[d9sel]; }
        for (int i = 0; i < nk; i++) lcdknob(kv[i], 27 + 106 * (2 * i + 1) / (2 * nk), 49, 5, kl[i], 0.5f);
    }

    // ④ voice picker — ALL 11 voices in one row (acid order, 2-char amber pads), pulled
    // DOWN by the cells (mirrors the 808). Tap = SELECT+audition; in MUT mode = toggle that
    // voice's mute (dimmed + red slash).
    for (int r = 0; r < TR9_NV; r++) {
        int v = VL[r], x = 6 + r * 13, selp = (v == d9sel), mtd = d9mute[v];
        void *wp = ui_wid_hash(0x90u + v, x, 68, 12, 9); ui_reg(wp, x, 68, 12, 9, 0);
        UiCap *c = ui_cap_for(wp); int hot = (c != 0);
        if (c) {
            if (padtool == PT_REC && playing) {                  // REC — punch onto the CURRENT step (multitouch)
                if (ui_grabbed(wp)) { d9grid[v][step] = 1; tr909_fire(D909_BASE, v, 1, 0, d9tune, d9decay, d9color); d9trig[v] = 1; }
            } else if (padtool == PT_MUTE) {                     // MUTE — INSTANT mute toggle on tap (live-precise)
                if (ui_grabbed(wp)) d9mute[v] = !d9mute[v];
            } else if (padtool == PT_PLAY) {                     // PLAY — just fire the voice (finger-drum), no select
                if (ui_grabbed(wp)) { tr909_fire(D909_BASE, v, 1, 0, d9tune, d9decay, d9color); d9trig[v] = 1; }
            } else if (c->released) {                            // PICK — SELECT (+ audition only when STOPPED)
                d9sel = v; if (!playing) { tr909_fire(D909_BASE, v, 1, 0, d9tune, d9decay, d9color); d9trig[v] = 1; }
            }
        }
        rrectfill(x, 68, 12, 9, 1, mtd ? CLR_DARKER_PURPLE : selp ? CLR_ORANGE : CLR_DARK_ORANGE);
        if (d9trig[v] > 0) { blend(BLEND_AVG); rrectfill(x, 68, 12, 9, 1, mtd ? CLR_LIGHT_GREY : CLR_WHITE); blend_reset(); }   // trigger flash (ghost-grey when muted)
        rrect(x, 68, 12, 9, 1, (selp || hot) ? CLR_WHITE : CLR_BROWNISH_BLACK);
        if (mtd) line(x + 2, 69, x + 9, 75, CLR_RED);        // muted = red slash
        font(FONT_TINY); print(AB9[v], x + (12 - text_width(AB9[v])) / 2, 70, mtd ? CLR_DARKER_GREY : selp ? CLR_BROWNISH_BLACK : CLR_LIGHT_YELLOW);
    }
    // ④a the pad TOOL — the far-right PICK/PLAY/MUTE/REC selector (mirrors the 808), flush
    // against the pads it retargets. Tap to cycle; fill colour + the stacked word show which.
    {
        static const char *TL[4] = { "PICK", "PLAY", "MUTE", "REC" };   // the 4-way pad TOOL
        static const int    TC[4] = { CLR_DARK_BROWN, CLR_MEDIUM_GREEN, CLR_ORANGE, CLR_RED };   // PICK neutral · PLAY green · MUTE orange · REC red
        int bx = 150, by = 68, bw = 7, bh = 29;
        int pr = 0, hot = 0, foc = 0;
        void *w = ui_wid_hash(0xE0u, bx, by, bw, bh);
        if (ui_button_core(w, bx, by, bw, bh, &foc, &pr, &hot) && tap_settled()) {
            padtool = (padtool + 1) % 4; if (padtool == PT_PICK) dscreen = DS_VCE;   // PICK also snaps the screen to TONE
        }
        rrectfill(bx, by, bw, bh, 2, TC[padtool]);
        rrect(bx, by, bw, bh, 2, hot ? CLR_WHITE : CLR_BROWNISH_BLACK);
        const char *wd = TL[padtool]; int nl = 0; while (wd[nl]) nl++;   // stack the word one letter per row
        font(FONT_TINY);
        int ch = 6, ty = by + (bh - nl * ch) / 2;                    // 5px glyph + 1px lead, vertically centred
        for (int i = 0; i < nl; i++) { char s[2] = { wd[i], 0 }; print(s, bx + (bw - text_width(s)) / 2 + 1, ty + i * ch, CLR_WHITE); }
    }

    // ⑤ the HITS — picked voice's 16 steps at the bottom; amber, white downbeat accents.
    // KIT: paint on/off. FLAG: ACC toggles accent · PROB vertical-slides the chance
    // (short bar) · STRK cycles none→flam→drag→ratchet (cyan pips mark the type).
    for (int s = 0; s < STEPS; s++) {
        int x = 6 + s * 9, here = (s == step && playing);
        void *ws = ui_wid_hash(0xA0u + s, x, 81, 8, 16);
        ui_reg(ws, x, 81, 8, 16, 0);
        UiCap *c = ui_cap_for(ws);
        if (c) {
            g_drag_frame = ui_frame_ct; g_drag_y = c->cy;
            int fx = c->released ? c->rx : c->cx, cell = (fx - 6) / 9;
            if (cell < 0) cell = 0; if (cell >= STEPS) cell = STEPS - 1;
            if (dscreen != DS_FLAG) {                        // VCE/KIT — paint on/off
                if (ui_grabbed(ws)) { paint_val = !d9grid[d9sel][s]; if (paint_val) { tr909_fire(D909_BASE, d9sel, 1, 0, d9tune, d9decay, d9color); d9trig[d9sel] = 1; mbop = 1; } }
                d9grid[d9sel][cell] = paint_val;
            } else if (darmed == DD_ACC) {                   // FLAG/ACC — toggle accent
                if (ui_grabbed(ws)) { paint_val = !d9acc[d9sel][s]; if (paint_val) d9grid[d9sel][s] = 1; }
                d9acc[d9sel][cell] = paint_val; if (paint_val) d9grid[d9sel][cell] = 1;
            } else if (darmed == DD_PROB) {                  // FLAG/PROB — vertical slide, drawn across
                float f = clamp((94 - c->cy) / 13.0f, 0, 1);
                d9prob[d9sel][cell] = snap_prob(f); d9grid[d9sel][cell] = 1;
            } else if (darmed == DD_STRK) {                  // FLAG/STRK — cycle the stroke, paint across
                if (ui_grabbed(ws)) { paint_val = (d9strk[d9sel][s] + 1) % TR9_NSTROKE; d9grid[d9sel][s] = 1; }
                d9strk[d9sel][cell] = paint_val; if (paint_val) d9grid[d9sel][cell] = 1;
            } else {                                         // FLAG/TUN|DEC|CHAR — bipolar p-lock offset, drawn across
                d9off[darmed - DD_TUN][d9sel][cell] = clamp((89 - c->cy) / 8.0f, -1, 1); d9grid[d9sel][cell] = 1;
            }
        }
        int on2 = d9grid[d9sel][s], locklens = (dscreen == DS_FLAG && darmed >= DD_TUN);
        int lp = locklens ? darmed - DD_TUN : 0;
        rrectfill(x, 81, 8, 16, 1, (here && !on2) ? CLR_DARKER_GREY : CLR_DARKER_PURPLE);
        if (on2 && locklens) {                               // LOCK lens — bipolar bar from centre
            int cy = 89, op = (int)(d9off[lp][d9sel][s] * 7 + (d9off[lp][d9sel][s] >= 0 ? 0.5f : -0.5f));
            line(x, cy, x + 7, cy, CLR_DARKER_GREY);
            int col = here ? CLR_WHITE : op ? CLR_LIGHT_YELLOW : CLR_MEDIUM_GREEN;
            if (op > 0)      rrectfill(x, cy - op, 8, op, 1, col);
            else if (op < 0) rrectfill(x, cy + 1, 8, -op, 1, col);
            else             rectfill(x + 2, cy, 4, 1, col);
        } else if (on2) {
            int pr = d9prob[d9sel][s] > 0 ? d9prob[d9sel][s] : 100, fh = 16 * pr / 100; if (fh < 3) fh = 3;
            rrectfill(x, 81 + 16 - fh, 8, fh, 1, here ? CLR_WHITE : (s % 4 == 0 ? CLR_LIGHT_YELLOW : CLR_ORANGE));
            if (d9acc[d9sel][s]) rectfill(x + 1, 78, 6, 2, CLR_RED);                                          // accent marker in the gap above
            for (int k = 0; k < d9strk[d9sel][s]; k++) pset(x + 2 + k * 2, 83, CLR_TRUE_BLUE);                // stroke pips
        }
        rrect(x, 81, 8, 16, 1, CLR_BROWNISH_BLACK);
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
static void draw_mst(void) {
    static const char *MLAB[4] = { "303a", "303b", "808", "909" };
    static const char *DL[4]   = { "1/16", "1/8", "DOT", "1/4" };

    // ①b per-machine VOLUME — tiny sliders right under the nav tabs (MST face only), each
    // beneath its OWN cartridge so a machine's level sits under its button. Drag L/R; the
    // fill = level, tinted the machine's colour (grey when the tab is muted).
    for (int m = 0; m < 4; m++) {
        int sx = 19 + m * 25, sw = 23;
        void *w = ui_wid_hash(0xF0u + m, sx, 10, sw, 6); ui_reg(w, sx, 10, sw, 6, 0);
        UiCap *c = ui_cap_for(w);
        if (c) { g_drag_frame = ui_frame_ct; g_drag_y = c->cy;
                 int fxp = c->released ? c->rx : c->cx; level[m] = clamp((fxp - sx) / (float)(sw - 1), 0, 1); }
        rrectfill(sx, 11, sw, 4, 1, CLR_BROWNISH_BLACK);   // top edge butts the nav panel (ends y11) → touches the tabs
        int lw = (int)(level[m] * (sw - 2) + 0.5f);
        if (lw > 0) rectfill(sx + 1, 12, lw, 2, mac[m].mute ? CLR_DARKER_GREY : mac[m].col);
    }

    // ② master live knobs (nudged down 2px to clear the volume-slider row above); SWG =
    // the ONE global shuffle for the whole rack (ReBirth's model — a shuffle knob by the tempo)
    knob(&mglu,    18, 24, 6, "GLU",  0.30f);
    knob(&mflt,    43, 24, 6, "FLT",  0.50f);
    knob(&mfres,   68, 24, 6, "RES",  0.35f);
    knob(&mfb,     93, 24, 6, "FB",   0.35f);
    knob(&mpump,  118, 24, 6, "PUMP", 0.0f);
    knob(&g_swing,143, 24, 6, "SWG",  0.0f);

    // ③ screen — soft-keys pick MIX (channel meters) or PCF (the drawable filter lane)
    if (cbtn(0x20u, 6, 38, 16, 8, "MIX", !mstflow)) mstflow = 0;
    if (cbtn(0x21u, 6, 47, 16, 8, "PCF",  mstflow)) mstflow = 1;   // (KEY moved to the 303 faces — per-line root/scale/octave)
    // (the LIN/PWR pan-LAW chip was an A/B experiment — stereo works, so it's gone;
    //  the rack is fixed at PAN_LINEAR, set once at init.)
    rrectfill(24, 37, 112, 24, 3, CLR_BROWNISH_BLACK);
    rrectfill(27, 39, 106, 20, 2, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = 40; y < 58; y += 2) line(27, y, 132, y, CLR_BROWNISH_BLACK); blend_reset();
    font(FONT_TINY);
    if (mstflow == 0) {
        // MIX — the 4 channel LEVEL faders (drag up/down). Fill = level; bright green when the
        // channel fires, dim grey when muted (mute lives on the tabs). This is the ONE place the
        // per-machine levels live now — the instrument faces stay uncluttered.
        for (int m = 0; m < 4; m++) {
            int cw = 26, cx = 29 + m * cw, fy = 40, fh = 17, fw = cw - 6;
            int muted = mac[m].mute, act = machine_active(m);
            void *w = ui_wid_hash(0xD0u + m, cx, fy, fw, fh);
            ui_reg(w, cx, fy, fw, fh, 0);
            UiCap *c = ui_cap_for(w);
            if (c) {
                g_drag_frame = ui_frame_ct; g_drag_y = c->cy;
                int fyv = c->released ? c->ry : c->cy;
                level[m] = clamp((fy + fh - 1 - fyv) / (float)(fh - 1), 0, 1);
            }
            int lv = (int)(level[m] * (fh - 2) + 0.5f);
            rectfill(cx, fy, fw, fh, CLR_BROWNISH_BLACK);                         // track
            int col = muted ? CLR_DARKER_GREY : act ? CLR_LIME_GREEN : mac[m].col;
            if (lv > 0) rectfill(cx + 1, fy + fh - 1 - lv, fw - 2, lv, col);      // fill up from the bottom
            print(MLAB[m], cx + (fw - text_width(MLAB[m])) / 2, fy + fh - 6, CLR_LIGHT_YELLOW);
        }
    } else if (mstflow == 1) {
        // PCF — a DRAWABLE filter lane: drag to shape the master cutoff across the 16 steps
        int lx0 = 30, lw = 6, ly = 40, lh = 17;
        for (int s = 0; s < STEPS; s++) {
            int cx = lx0 + s * lw;
            void *w = ui_wid_hash(0xC0u + s, cx, ly, lw, lh);
            ui_reg(w, cx, ly, lw, lh, 0);
            UiCap *c = ui_cap_for(w);
            if (c) {                                         // the captured cell tracks the finger → draw the curve
                g_drag_frame = ui_frame_ct; g_drag_y = c->cy;
                int fx = c->released ? c->rx : c->cx, fy = c->released ? c->ry : c->cy;
                int cell = (fx - lx0) / lw; if (cell < 0) cell = 0; if (cell >= STEPS) cell = STEPS - 1;
                mpcf[cell] = (int)(clamp((ly + lh - 1 - fy) / (float)(lh - 1), 0, 1) * 7 + 0.5f);
            }
            if (s == step && playing) { blend(BLEND_AVG); rectfill(cx, ly, lw - 1, lh, CLR_MEDIUM_GREEN); blend_reset(); }
            int fh = lh * mpcf[s] / 7;
            rectfill(cx, ly + lh - fh, lw - 1, fh, mpcf[s] < 7 ? CLR_LIME_GREEN : CLR_DARK_GREEN);
        }
    }

    // ④ delay TIME selector + the master TEMPO (rack-scope, next to the delay it re-times)
    print("DELAY", 6, 66, CLR_DARK_BROWN);
    for (int i = 0; i < 4; i++)
        if (cbtn(0x04u + i, 30 + i * 22, 64, 20, 9, DL[i], mdiv == i)) mdiv = i;
    {   // ④b TEMPO — drag the readout up/down to set BPM; drives the step clock AND the delay
        int bx = 124, by = 64, bw = 34, bh = 9;
        ui_reg(&g_bpm, bx, by, bw, bh, 0);
        UiCap *c = ui_cap_for(&g_bpm); int held = c != 0;
        if (c) {
            g_drag_frame = ui_frame_ct; g_drag_y = c->cy;
            if (!c->has_v0) { c->has_v0 = 1; c->v0 = g_bpm; c->by = c->cy; }
            int py = c->released ? c->ry : c->cy;
            g_bpm = clamp(c->v0 + (c->by - py) * 0.6f, 60, 200);
            c->v0 = g_bpm; c->by = py;
        }
        rrectfill(bx, by, bw, bh, 2, held ? CLR_TRUE_BLUE : CLR_DARK_BROWN);
        rrect(bx, by, bw, bh, 2, held ? CLR_WHITE : CLR_BROWNISH_BLACK);
        int bi = (int)(g_bpm + 0.5f), tc = held ? CLR_WHITE : CLR_LIGHT_PEACH;
        char nb[4]; int ni = 0;
        if (bi >= 100) nb[ni++] = '0' + bi / 100;
        nb[ni++] = '0' + (bi / 10) % 10; nb[ni++] = '0' + bi % 10; nb[ni] = 0;
        font(FONT_TINY); print("BPM", bx + 3, by + 2, tc);
        print(nb, bx + bw - text_width(nb) - 3, by + 2, tc);
    }

    // ⑤ per-machine delay SEND
    print("SEND", 6, 79, CLR_DARK_BROWN);
    knob(&msend[0], 36, 84, 5, MLAB[0], 0.10f);
    knob(&msend[1], 72, 84, 5, MLAB[1], 0.10f);
    knob(&msend[2], 108, 84, 5, MLAB[2], 0.0f);
    knob(&msend[3], 144, 84, 5, MLAB[3], 0.0f);
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
    reverb(0.62f, 0.42f);                                  // the warm HALL — the 303s' space (tank 0), acidrack's tuning
    reverb_bus(1, 0.34f, 0.15f);                           // 909 → its own tight bright PLATE (tank 1)
    reverb_bus(2, 0.45f, 0.30f);                           // 808 → its own room (tank 2)
    pan_law(PAN_LINEAR);                                   // fixed rack pan law (the LIN/PWR toggle was removed; LINEAR = engine default)
    // pattern slots are zero-filled = empty, but two fields must NOT be 0 or a fresh slot is UNPLAYABLE:
    // 303 plen 0 → ctr%0 (playhead UB); drum prob 0 → every hit 0% trig. Seed them empty-but-PLAYABLE.
    for (int i = 0; i < 2; i++) for (int s = 0; s < NPAT; s++) pat303[i][s].plen = STEPS;
    for (int s = 0; s < NPAT; s++) for (int v = 0; v < TR_NV;  v++) for (int k = 0; k < STEPS; k++) pat808[s].prob[v][k] = 100;
    for (int s = 0; s < NPAT; s++) for (int v = 0; v < TR9_NV; v++) for (int k = 0; k < STEPS; k++) pat909[s].prob[v][k] = 100;
    for (int s = 0; s < STEPS; s++) mpcf[s] = 7;           // PCF lane starts fully open (no effect)
    sidechain_key(TR808_BASE + TRS_BD, 0, 1);              // both kicks drive the PUMP
    sidechain_key(D909_BASE + TR9S_BD, 0, 1);
    apply_fx();                                            // engage the default master glue
}

void update(void) {
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
            float spd = pf_dbl[i] ? 2.0f : pf_half[i] ? 0.5f : 1.0f;   // PERF speed lens
            float lp  = g_phase * spd;                                 // this line's own phase
            int   lc  = (int)lp; float lf = lp - lc;
            int   lcs = ((lc & 1) && lf < sw) ? lc - 1 : lc;          // swung per-line counter
            if (lcs != laststep303[i]) {
                laststep303[i] = lcs;
                int ls = lcs % plen[i]; lpos[i] = ls;
                if (mac[i].mute) { acid_off(&ac[i]); continue; }
                int accent = acc[i][ls] || pf_acc[i];                          // total-accent lens
                int slide  = pf_stac[i] ? 0 : pf_glide[i] ? 1 : sld[i][ls];    // slide-flip lens
                if (on[i][ls]) { acid_note(&ac[i], ac[i].base + mroot[i] + loct[i] * 12 + pit[i][ls] + oct[i][ls] * 12, accent, slide); mbop = 1; }
                else if (tie[i][ls]) acid_tie(&ac[i], slide);         // hold the previous note through
                else acid_off(&ac[i]);
            } else if (!pf_glide[i]) {                                // staccato gate between flips (GLIDE lens sustains → skip entirely)
                int ls = lcs % plen[i];
                int slide = pf_stac[i] ? 0 : sld[i][ls];             // STAC forces staccato; else a slid step glides OUT
                if (on[i][ls] && !slide) {                           // gate ONLY a fresh, NON-slid note (tb303's rule): a slid
                    float onset = (lcs & 1) ? sw : 0.0f;             // step must ring on to glide, and a tie/rest holds — cutting
                    int nx = (lcs + 1) % plen[i];                    // them at 70% was the bug (manual slides never actually glided)
                    acid_gate(&ac[i], lf, onset, tie[i][nx]);       // (next_ties still spares the cut into a tie)
                }
            }
        }

        // drums — fire at the flip, DRAG the odd 16ths via the fire() ms delay
        // (tempo-scaled: one 16th = 15000/bpm ms, so the feel holds at any BPM).
        if (ctr != laststep) {
            laststep = ctr;
            int swms = (step & 1) ? (int)(sw * (15000.0f / g_bpm)) : 0;   // the same 0..0.6-of-a-16th shuffle, in ms
            for (int v = 0; v < TR_NV; v++) {                     // the 808 line, same transport
                if (!dgrid[v][step] || (dprob[v][step] < 100 && rnd(100) >= dprob[v][step])) continue;
                dtrig[v] = 1;                                     // would-trigger → the pad pulses even while muted
                if (!mac[M_808].mute && !dmute[v]) {              // ...but only SOUND if not muted
                    int bo = dacc[v][step] ? 2 : 0;                                     // accent → +2 velocity
                    Lane L[LK_N]; int nl = drum_lanes(M_808, v, L); float sv[LK_N];      // per-step lane offsets, around each voice knob
                    for (int p = 0; p < nl; p++) {
                        float eff = clamp(*L[p].knob + L[p].off[step] * 0.5f, 0, 1);
                        if (L[p].sink == SK_ARG) { sv[p] = *L[p].knob; *L[p].knob = eff; }                        // ride the fire array
                        else if (L[p].sink == SK_VEL) bo += (int)((eff - 0.5f) * 8 + (eff >= 0.5f ? 0.5f : -0.5f)); // VOL → ±4 velocity (0.5 = unity)
                        else if (L[p].sink == SK_PAN && eff != dpanlast[v]) { tr808_pan(TR808_BASE, v, (eff - 0.5f) * 2); dpanlast[v] = eff; }  // PAN → -1..+1; push only on CHANGE (queued set-and-hold)
                    }
                    tr808_fire(TR808_BASE, v, bo, swms, dtune, ddecay, dcolor);
                    for (int p = 0; p < nl; p++) if (L[p].sink == SK_ARG) *L[p].knob = sv[p];
                }
            }
            for (int v = 0; v < TR9_NV; v++) {                    // the 909 line
                if (!d9grid[v][step] || (d9prob[v][step] < 100 && rnd(100) >= d9prob[v][step])) continue;
                d9trig[v] = 1;
                if (!mac[M_909].mute && !d9mute[v]) {
                    int bo = d9acc[v][step] ? 2 : 0, st = d9strk[v][step];
                    Lane L[LK_N]; int nl = drum_lanes(M_909, v, L); float sv[LK_N];      // per-step lane offsets, around each voice knob
                    for (int p = 0; p < nl; p++) {
                        float eff = clamp(*L[p].knob + L[p].off[step] * 0.5f, 0, 1);
                        if (L[p].sink == SK_ARG) { sv[p] = *L[p].knob; *L[p].knob = eff; }
                        else if (L[p].sink == SK_VEL) bo += (int)((eff - 0.5f) * 8 + (eff >= 0.5f ? 0.5f : -0.5f)); // VOL → ±4 velocity
                        else if (L[p].sink == SK_PAN && eff != d9panlast[v]) { tr909_pan(D909_BASE, v, (eff - 0.5f) * 2); d9panlast[v] = eff; }  // PAN → -1..+1; push only on CHANGE
                    }
                    if (st) tr909_fire_stroke(D909_BASE, v, st, bo, swms, (int)(15000.0f / g_bpm), d9tune, d9decay, d9color);   // one 16th = 15000/bpm ms (113 @132)
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

void draw(void) {
    // seed each line's EFFECTIVE lens from its persistent LATCH (so a latched lens holds across
    // faces + while another line is focused); the focused PERF screen then adds momentary-hold on top.
    for (int i = 0; i < 2; i++) {
        pf_half[i]  = pf_latch[PL_HALF][i];  pf_dbl[i]   = pf_latch[PL_DBL][i];  pf_acc[i] = pf_latch[PL_ACC][i];
        pf_stac[i]  = pf_latch[PL_STAC][i];  pf_glide[i] = pf_latch[PL_GLIDE][i];
    }
    cls(CLR_DARK_PURPLE);
    rrectfill(0, 0, 160, 100, 7, CLR_INDIGO);
    rrectfill(3, 2, 154, 96, 5, CLR_LIGHT_PEACH);                     // 2px purple bezel top & bottom
    blend(BLEND_AVG); line(7, 2, 152, 2, CLR_WHITE); blend_reset();
    ui_begin();
    font(FONT_SMALL);

    navspine();
    if (mac[face].kind == MK_303) draw_303(face);
    else if (face == M_808)       draw_808();
    else if (face == M_909)       draw_909();
    else                          draw_mst();   // M_MST

    font(FONT_NORMAL);
    ui_end();
}
