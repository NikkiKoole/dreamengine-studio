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
    "detail": "Nav = faces (tap a cartridge body to FOCUS, tap its LED to MUTE from anywhere). Sound: two TB-303 lines on the shared runtime/acid303.h (303b an octave up = the bass+acid-lead duo), the full TR-808 on the extracted runtime/tr808.h and the TR-909 on runtime/tr909.h (both byte-honest to the tr808/tr909 carts). Each 303 face: the top gear-drag knob row is always the acid knobs — vanilla CUT/RES/ENV/DEC/ACC, or (via an inline DF switch at the row's right end, tinted the machine colour) a DEEP Devil Fish page (SUB octave-down sub / ADEC two-decay / SLDT slide time / TRK filter tracking + a SAW/SQR WAVE toggle that re-defines the oscillator). The green screen has four soft-keys like the drums — SEQ (the LCD roll), FLAG (a 6-button palette ACC/SLD/TIE/OCT+/OCT-/LEN you arm then paint on the bars — per-step depth + per-line LENGTH/polymeter), FX (DRV/SEND/VERB as LCD-native green knobs), and GEN (a CLEAR + MIN/MID/BUSY generate menu). The 16 NOTE-BARS below (tap = on/off, drag up/down = pitch, snapped to a minor-pentatonic so it stays musical). Drum faces (808 blue, 909 amber): the TOP row is always the machine FX + a machine-global SWING (SWG/DIST/SEND/VERB, the 909 also a METAL XY pad for the metal-voice highpass, tr909_metal) — no FX toggle. The green SCREEN is the voice inspector: four soft-keys pick its content (the mode is sticky — a voice-pad tap only re-picks the voice, so you can paint flags across the whole kit without re-selecting) — VCE (the default: the picked voice's TUNE + DEC as LCD-native green knobs, plus a voice-specific CHARACTER knob only where the machine has one — SNPY/THUD/TONE/RING on the 808, ATTK/SNPY/CLIK/TONE on the 909, faithful to tr808/tr909; voices without one show just the two) / KIT (the kit minimap) / FLAG (a depth palette) / GEN (a CLEAR + MIN/MID/BUSY generate menu) you arm then paint on the cells: ACC (per-step accent, a red marker + louder hit) / PROB (vertical-slide a cell for its trig-chance, 100/75/50/25 = a shorter bar, the RD-8 move) / STRK (909 only — cycle none→flam→drag→ratchet, blue pips, via tr909_fire_stroke) / and the per-step p-LOCKS TUN / DEC / <character> (each its own button — the palette is two rows: ACC/PROB(/STRK) on top, the p-locks below; the character button appears only on voices that have one, e.g. SNPY). Arming a p-lock turns the cell lane into a BIPOLAR bar-from-centre lens editing that knob's per-step OFFSET (centre = follow the voice knob, up/down = ± its range); all three offsets play at once, each riding its voice knob so the knobs still transpose the whole contour. PROB and the p-locks DRAW ACROSS — drag over the row and each cell takes the finger's height, so a whole probability ramp or melodic/decay/timbre contour sweeps out in one gesture (the PCF-lane / note-bar feel). A single-row voice picker holding the FULL roster (16 tiny 2-char pads on the 808, 11 on the 909, acidrack's acid order — full name in the VCE header), doubling as a per-voice MUTE grid under a MUT soft-key (muted = dimmed + red slash, silenced but kept in the pattern), and the 16 hits on the bottom in the iconic 808 quarter-colours. FX routing: a shared tempo-synced delay + per-machine reverb (303s → a warm hall, 808/909 → their own plates with the kick kept dry) + per-machine drive. MST: master GLU/FLT/RES/FB/PUMP, a 4-channel mix overview, delay TIME + per-machine SEND. Knob feel: vertical = value, pull sideways for a fine gear, double-tap resets. Widgets that live INSIDE the LCD (the flag/depth palettes, NEW, the drum VCE voice knobs, the 303 FX knobs) use LCD-native variants — lcdbtn (pure green outline, filled when armed) + lcdknob (green outline + value arc) — so they read as part of the readout, not the candy chassis. (The old step-row + keybed is kept behind use_bars = 0.)",
    "controls": "Tap a cartridge to focus a machine; tap its LED to mute. PLAY runs the shared transport. 303: drag CUT/RES/ENV/DEC/ACC (sideways = fine, double-tap = reset); the inline DF switch (right of the knob row) flips to the DEEP page (SUB/ADEC/SLDT/TRK + a SAW/SQR WAVE toggle); SEQ/FLAG/FX/GEN soft-keys switch the screen between the roll, the flag palette (arm ACC/SLD/TIE/OCT+/OCT-/LEN, then tap bars), the FX knobs (DRV/SEND/VERB), and the generate menu (CLEAR / MIN / MID / BUSY); on the note-bars tap = note on/off, drag up/down = pitch. 808/909: SWG/DIST/SEND/VERB (+909 METAL XY) sit on top always; tap a voice pad to pick+audition (the screen mode stays put — VCE by default shows its TUNE + DEC + a voice-specific character knob where the machine has one, SNPY/THUD/TONE/RING/ATTK/CLIK), the picker shows the whole roster in one row (all 16/11 voices, acid order); the MUT soft-key flips a pad tap from SELECT to per-voice MUTE; VCE/KIT/FLAG/GEN soft-keys switch the screen between the voice knobs, the minimap, the depth palette, and the generate menu (CLEAR / MIN / MID / BUSY). Cells: tap = place a hit, DRAG across = paint a fill (VCE/KIT); in FLAG, the palette is two rows — ACC/PROB(/STRK) on top, the p-locks TUN/DEC/<character> below — arm one then work the cells (ACC tap = accent, PROB vertical-slide = trig-chance, STRK tap = cycle flam/drag/ratchet, TUN/DEC/char vertical-slide = a per-step bipolar offset around that voice knob). MST: GLU tames level, FLT is the live DJ filter, PUMP ducks to the kick, pick a delay TIME + per-machine SEND."
  },
  "todo": [
    "808/909 completeness: SHIPPED (2026-07-16) — EVERY voice is a pad in ONE picker row (no pages): 808 = 16 tiny 2-char pads, 909 = 11 wider ones, in acidrack's ACID ORDER (kick/hats/clap/snare first, fills last). The full name shows in the VCE screen header on select. The KIT minimap is a full-roster density grid (all 16/11 voices as 1px rows). Pads DOUBLE-DUTY: a MUT soft-key flips a tap between SELECT+audition and per-voice MUTE (dmute/d9mute — dimmed + red slash, silenced in playback + dimmed in the minimap), on top of the machine mute. A muted pad still GHOST-PULSES (light-grey flash) on its would-be triggers — dtrig is set on the would-trigger, the fire() is what's gated by mute — so you can see the silenced pattern playing and time an unmute.",
    "drum DEPTH: SHIPPED (2026-07-16) a KIT/FLAG soft-key pair on both drum faces (the 303's SEQ/FLAG idiom) — FLAG fills the LCD with a depth palette (ACC/PROB, 909 also STRK) you arm then paint on the cells: per-voice-per-step ACCENT (fires boost +2, red marker in the gap above the cell), trig PROBABILITY (vertical-slide → snap_prob 100/75/50/25, shorter bar = lower chance, rnd()-gated in update()), and 909 STROKE (cycle none→flam→drag→ratchet via tr909_fire_stroke, blue pips). The 909 metal XY (m9cut/m9res, tr909_metal) is now a live XY pad in the 909 FX panel. NEW resets a voice's depth too. Still open: per-step accent SWEEP + a clearer stroke glyph than pips.",
    "p-LOCKS: SHIPPED (2026-07-16) — a LOCK flag in the drum FLAG palette. Arming it makes the cell lane a BIPOLAR per-step OFFSET lens (centre = follow the voice knob, up/down slide, drawn across); the offsets live in doff/d9off[LK_TUNE|LK_DEC|LK_CHAR] and are applied by swapping the voice's TUNE/DEC/CHAR knobs around each fire, so the voice knobs still transpose the whole contour. Each is its OWN button — the FLAG palette is now two rows (ACC/PROB(/STRK) on top, TUN/DEC/<char> below; the character button only shows on voices that have one). All three offsets play at once. FOLLOW-UPS: (1) a way to UNLOCK a step back to 0 (currently only NEW/GEN clears — maybe a plain tap zeroes it); (2) the same p-lock lens for the 303 knobs (CUT/RES).",
    "ARRANGEMENT (the last big acidrack gap): pattern BANKS A-D + a SONG chain + the 8-hex GEN song-code + WAV export.",
    "SWING: per-machine shuffle SHIPPED (2026-07-16) — a SWG lcdknob in each drum KIT screen delays odd 16ths via the fire delay param (0 = straight). SAVE/LOAD (autosave patterns/knobs) still open — acidrack has it.",
    "drum-machine ergonomics: (1) CLEAR — SHIPPED (2026-07-16) as part of a GEN flow. Every instrument has a GEN soft-key (right margin, replacing the decorative SCP) that fills the LCD with a CLEAR + MIN/MID/BUSY menu — CLEAR empties the pattern, the three densities regenerate it (MID = the old NEW recipe). Replaces the single NEW button. (2) LIVE RECORD — still open: while the loop plays, a mode where tapping a voice pad punches its hit onto the CURRENT step (step/overdub record), instead of only auditioning.",
    "waveshape flip: SHIPPED (2026-07-16) a SAW/SQR WAVE toggle in the 5th knob slot of the 303 DEEP page (flips a.wave + re-calls acid_define). Still open: the drvmode waveshaper (SOFT/HARD/FOLD/ASYM) could get the same treatment.",
    "303 DEEP knobs: SHIPPED (2026-07-15) a DF soft-key flips the zone-2 row between vanilla (CUT/RES/ENV/DEC/ACC) and a DEEP page (SUB/ADEC/SLDT/TRK) — the headline Devil Fish knobs, per-303, sub-osc wired on slots 36/37 (34/35 belong to the 909's 13-slot bank). DRV moved to the FX panel (aligns with the drums). Still not surfaced: ATK (soft attack) + SQL (env-sweep) + the accent-SWEEP mode; a fuller flow could page those too.",
    "wire the still-decorative soft-keys: SHIPPED (2026-07-15) the per-machine FX panel — the FX soft-key on every 303/808/909 opens an aligned DRV/DIST + SEND (shared delay) + VERB (reverb) row (draw_fxrow); reverb is real (303s → warm hall tank 0, 909 → tight plate tank 1, 808 → room tank 2, kicks kept dry) + per-machine drum drive. STILL decorative: MST SNG/SCP only. (Drum MAP → the FLAG toggle; the 303/drum SCP slots → the GEN soft-key 2026-07-16; the 909 METAL XY shipped into its FX panel.) So every 303/808/909 soft-key is now live.",
    "SOUL: the LCD screens have no MASCOT yet (the slime from tinyface/facemock, candy-style §3) — a FACE flow could give a bopping/reacting creature its own screen. Deferred for now.",
    "graduate the gear-drag knob (vertical=value, sideways=fine gear, double-tap=reset) into ui.h's ui_knob so every cart gets it.",
    "note-bars: a CHROMATIC toggle (out-of-key acid) + let a drag PAINT across several bars; MST could grow per-machine level faders."
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
// the note-bar editor: pitch snaps to a minor-pentatonic scale (always musical).
// (named PENTA, not SCALE — SCALE is the engine's -D scale-factor flag.)
static const int PENTA[6] = { 0, 3, 5, 7, 10, 12 };        // semitones above base
#define NPENTA 6
static int penta_idx(int semi) { for (int k = 0; k < NPENTA; k++) if (PENTA[k] == semi) return k; return 0; }
// SEQ note-bar paint drag: grab pos + axis-lock (0=undecided 1=pitch 2=on/off) + values
static int drag_gx, drag_gy, drag_axis, drag_paint, drag_on0;
// per-step DEPTH (the 303 flags) + polymeter
static int  tie[2][STEPS], oct[2][STEPS];   // TIE = hold prev note; OCT = octave -1/0/+1
static int  plen[2] = { STEPS, STEPS };     // per-line LENGTH (short = polymeter drift)
static int  lpos[2] = { 0, 0 };             // per-line playhead
enum { PS_SEQ, PS_FLAG, PS_FX, PS_GEN };     // 303 LCD content: the roll / flag palette / FX knobs / generate menu
static int  pscreen[2] = { PS_SEQ, PS_SEQ };  // per-303 screen mode (SEQ/FLAG/FX soft-keys)
static int  kpage[2];                        // per-303 knob page: 0 = vanilla, 1 = DEEP (Devil Fish + drive)
enum { FL_ACC, FL_SLD, FL_TIE, FL_OCTU, FL_OCTD, FL_LEN, FL_N };
static int  armed = FL_ACC;                  // which flag a bar-tap paints
static const char *FLNAME[FL_N] = { "ACC", "SLD", "TIE", "OCT+", "OCT-", "LEN" };
enum { LK_TUNE, LK_DEC, LK_CHAR, LK_VOL, LK_PAN, LK_N };    // continuous-lane params (doff[] index). VOL = per-step level (velocity); PAN = per-step stereo
// drum-depth flags. STRK = 909-only. The continuous lanes (TUN/DEC/CHAR/VOL) map to LK_* by
// darmed = DD_TUN + lane.lk (a STABLE id, NOT the button's row position — a lane after the
// optional CHAR would otherwise land on the wrong slot). DD_* stays PARALLEL to LK_*.
enum { DD_ACC, DD_PROB, DD_STRK, DD_TUN, DD_DEC, DD_CHAR, DD_VOL, DD_PAN, DD_N };
static const char *DDNAME[DD_N] = { "ACC", "PROB", "STRK", "TUN", "DEC", "CHR", "VOL", "PAN" };
enum { DS_VCE, DS_KIT, DS_FLAG, DS_GEN };     // drum LCD content: voice knobs / kit minimap / depth palette / generate menu
static int  dscreen = DS_VCE;                 // (VCE is entered by tapping a voice; KIT/FLAG are soft-keys)
static int  darmed = DD_ACC;                  // which drum flag a cell paints (FLAG mode)
static int  mutemode = 0;                     // voice-picker tap: 0 = SELECT+audition, 1 = toggle per-voice MUTE
static int  dmute[TR_NV]  = { 0 };            // per-808-voice mute (in addition to the machine mute)
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
// SEAM: toms/congas share a slot → they pan as a GROUP (hardware-honest). Independent
// tom/conga pan = splitting the 14-slot bank into per-voice slots (device-face §2c). Deferred.
static int   dsel = TR_BD;                                 // selected drum voice
static float dtrig[TR_NV];                                 // per-voice flash: 1 on fire, decays (picker pad lights up)
// the 909 drum face — voices from the shared tr909.h
static int   d9grid[TR9_NV][STEPS];
static float d9tune[TR9_NV], d9decay[TR9_NV], d9color[TR9_NV];
static float d9vol[TR9_NV];                                // per-voice LEVEL (0.5 = unity) — see dvol
static float d9pan[TR9_NV];                                // per-voice PAN (0.5 = centre) — see dpan
static int   d9sel = TR9_BD;
static float d9trig[TR9_NV];
static float m9cut = 0.40f, m9res = 0.33f;                 // the 909 metal-filter XY
// drum DEPTH — per-voice-per-step, painted in FLAG mode (mirrors the 303 flags)
static int   dacc[TR_NV][STEPS],  dprob[TR_NV][STEPS];                             // 808: accent flag + trig-prob %
static int   d9acc[TR9_NV][STEPS], d9prob[TR9_NV][STEPS], d9strk[TR9_NV][STEPS];   // 909: + stroke type (TR9_ST_*)
static float doff[LK_N][TR_NV][STEPS], d9off[LK_N][TR9_NV][STEPS];                 // per-step p-lock OFFSETs (TUNE/DEC/CHAR) from the voice knobs: -1..+1 (0 = follow the voice)
static float swing8 = 0, swing9 = 0;                       // per-machine shuffle: odd 16ths fire late (0 = straight)

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

// transport (shared across the rack)
static int   playing = 1, step = 0, laststep = -1;
static float mbop = 0;

// re-apply the master effects, set-and-hold — reconfigure ONLY on change (the
// fx-frame rule); the live FLT is ride-safe so it runs every frame. Voicings
// copied from acidrack's apply_fx so the master matches the mature rack.
static void apply_fx(void) {
    static float aFb = -1, aS[4] = { -1, -1, -1, -1 }, aComp = -1;
    static int   aDiv = -1, aMode = -2;
    if (mfb != aFb || mdiv != aDiv) {                       // the shared delay unit
        static const float DIV[4] = { 0.25f, 0.5f, 0.75f, 1.0f };   // 1/16 · 1/8 · dotted · 1/4
        echo((int)(60000.0f / 132 * DIV[mdiv]), mfb * 0.72f, 0.45f);
        aFb = mfb; aDiv = mdiv;
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
static int g_drag_y = 100;                   // ...and WHERE it was (canvas y) — a drag released below the
                                             // tab row (y>=11) can't bounce onto a tab, so it needn't block one
static int tap_settled(void) { return ui_frame_ct - g_drag_frame >= TAP_SETTLE; }

// A bounce lands near the last drag-release Y. So a momentary button should
// swallow its activation only while the settle window is open AND the drag
// ended near THIS button's rect — otherwise a knob-drag (y~22) would wrongly
// block a soft-key (y~40) two zones away. drag_bounce() is that per-rect test;
// the DF/WAVE row-1 buttons (same row as the knobs) are the ones it actually
// saves. (The nav tabs keep their own y<11 guard above — don't touch it.)
#define BOUNCE_MARGIN 6
static int drag_bounce(int y, int h) {
    if (tap_settled()) return 0;
    return g_drag_y >= y - BOUNCE_MARGIN && g_drag_y <= y + h + BOUNCE_MARGIN;
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
    if (af && (g_drag_y >= 11 || tap_settled())) face = m;                 // ignore a drag-release bounce
    if (am && (g_drag_y >= 11 || tap_settled())) mac[m].mute = !mac[m].mute;

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
    if (ui_button_core(w, px, py, pw, ph, &fo, &pr, &hot) && tap_settled()) { playing = !playing; laststep = -1; }
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
    if (cbtn(0x08u, 6, 37, 16, 7, "SEQ",  pscreen[i] == PS_SEQ))  pscreen[i] = PS_SEQ;
    if (cbtn(0x09u, 6, 45, 16, 7, "FLAG", pscreen[i] == PS_FLAG)) pscreen[i] = PS_FLAG;
    if (cbtn(0x06u, 6, 53, 16, 7, "FX",   pscreen[i] == PS_FX))   pscreen[i] = PS_FX;
    if (cbtn(0x31u, 138, 38, 16, 7, "GEN", pscreen[i] == PS_GEN)) pscreen[i] = PS_GEN;
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
    } else {
        font(FONT_TINY); print("132", 29, 40, CLR_MEDIUM_GREEN);      // bpm lives in the screen
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
        int by = 67, bh = 30;
        for (int s = 0; s < STEPS; s++) {
            int bx = 6 + s * 9, bw = 8, dead = (s >= plen[i]);
            void *w = ui_wid_hash(0xB0u + s, bx, by, bw, bh);
            ui_reg(w, bx, by, bw, bh, 0);
            UiCap *c = ui_cap_for(w);
            if (c) { g_drag_frame = ui_frame_ct; g_drag_y = c->cy; }
            if (pscreen[i] == PS_FLAG) {                 // FLAG mode: tap or DRAG paints the armed flag
                if (c) {                                 // the captured bar tracks the finger across the row
                    if (ui_grabbed(w)) paint_val = (armed == FL_LEN) ? 0 : !flag_get(i, s, armed);
                    int fx = c->released ? c->rx : c->cx, cell = (fx - 6) / 9;
                    if (cell < 0) cell = 0; if (cell >= STEPS) cell = STEPS - 1;
                    sel[i] = cell;
                    if (armed == FL_LEN) plen[i] = cell + 1;         // drag to set the loop length
                    else flag_set(i, cell, armed, paint_val);        // paint the same value across the drag
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
                        pit[i][cell] = PENTA[(int)(frac * (NPENTA - 1) + 0.5f)];
                        on[i][cell] = 1;
                    }
                }
                if (ui_released(w) && !drag_axis) { on[i][s] = !drag_on0; sel[i] = s; if (on[i][s]) mbop = 1; }
            }
            int here = (s == lpos[i] && playing);
            rrectfill(bx, by, bw, bh, 1, dead ? CLR_DARKER_PURPLE : CLR_DARK_BROWN);
            if (here) { blend(BLEND_AVG); rrectfill(bx, by, bw, bh, 1, CLR_MEDIUM_GREEN); blend_reset(); }
            if (on[i][s] && !dead) {
                int idx = penta_idx(pit[i][s]), fh = bh * (idx + 1) / NPENTA;
                rrectfill(bx, by + bh - fh, bw, fh, 1, acc[i][s] ? CLR_ORANGE : CLR_LIME_GREEN);
                if (sld[i][s]) {                                    // slide → bright top cap + a GLIDE LINE to the next note
                    rectfill(bx, by + bh - fh, bw, 1, CLR_WHITE);
                    int ns = (s + 1) % plen[i];
                    int nidx = tie[i][ns] ? idx : penta_idx(pit[i][ns]);
                    int ntop = (on[i][ns] || tie[i][ns]) ? by + bh - bh * (nidx + 1) / NPENTA : by + bh - fh;
                    line(bx + bw - 1, by + bh - fh, bx + bw + 1, ntop, CLR_WHITE);
                }
                if (oct[i][s] > 0) rectfill(bx + 1, by + 1, bw - 2, 2, CLR_LIGHT_YELLOW);           // OCT+
                if (oct[i][s] < 0) rectfill(bx + 1, by + bh - 3, bw - 2, 2, CLR_TRUE_BLUE);         // OCT-
            }
            if (tie[i][s] && !on[i][s] && !dead) rectfill(bx, by + bh / 2, bw, 2, CLR_TRUE_BLUE);  // TIE = hold prev
            if (!dead && s == plen[i] - 1 && plen[i] < STEPS) rectfill(bx + bw, by, 1, bh, CLR_RED);  // loop end
            rrect(bx, by, bw, bh, 1, (here || (pscreen[i] != PS_FLAG && s == sel[i])) ? CLR_WHITE : CLR_BROWNISH_BLACK);
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
            if (ui_button_core(wid, x, ky, 20, kh, &foc, &pr, &hot)) { pit[i][sel[i]] = n; on[i][sel[i]] = 1; mbop = 1; acid_note(a, a->base + n, 0, 0); }
            int lit = pit[i][sel[i]] == n && on[i][sel[i]];
            rrectfill(x, ky, 20, kh, 2, lit ? CLR_LIGHT_YELLOW : CLR_LIGHT_PEACH);
            rrect(x, ky, 20, kh, 2, CLR_BROWNISH_BLACK);
        }
        wi = 0;
        for (int n = 0; n < 12; n++) {
            if (isblack[n]) {
                int x = kb + wi * 21 - 6;
                int pr = 0, hot = 0, foc = 0; void *wid = ui_wid_hash(0x60u + n, x, ky, 12, 9);
                if (ui_button_core(wid, x, ky, 12, 9, &foc, &pr, &hot)) { pit[i][sel[i]] = n; on[i][sel[i]] = 1; mbop = 1; acid_note(a, a->base + n, 0, 0); }
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

    // ② TOP ZONE (always): the machine FX row + the machine-global SWING. No toggle —
    // per-voice tone lives in the screen now (tap a voice), FX/swing stay at hand up top.
    draw_fxrow(2);                                         // DIST · SEND · VERB (aligned with the 303s)
    knob(&swing8, 14, 22, 6, "SWG", 0.0f);                 // machine-global shuffle, left gutter

    // ③ screen — soft-keys pick the LCD content: VCE = the picked voice's knobs (also
    // shown by default), KIT = the kit minimap, FLAG = the depth palette, GEN (right) =
    // the CLEAR + randomize menu.
    if (darmed == DD_STRK) darmed = DD_ACC;                 // STRK is 909-only — reset if it was armed on the 909
    if (cbtn(0x1Fu, 6, 37, 16, 7, "VCE",  dscreen == DS_VCE))  dscreen = DS_VCE;
    if (cbtn(0x20u, 6, 45, 16, 7, "KIT",  dscreen == DS_KIT))  dscreen = DS_KIT;
    if (cbtn(0x21u, 6, 53, 16, 7, "FLAG", dscreen == DS_FLAG)) dscreen = DS_FLAG;
    if (cbtn(0x31u, 138, 38, 16, 7, "GEN", dscreen == DS_GEN)) dscreen = DS_GEN;
    if (cbtn(0x32u, 138, 47, 16, 7, "MUT", mutemode)) mutemode = !mutemode;   // pad tap → mute voices
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
    } else {                                                // DS_VCE — the picked voice's tone knobs, LCD-native
        font(FONT_TINY); plabel(TR808_NAME[dsel], 80, 40, CLR_LIME_GREEN);
        float *kv[5]; const char *kl[5]; int nk = 0;        // TUNE / DEC / [char] / VOL / PAN — even-spaced
        kv[nk] = &dtune[dsel];  kl[nk++] = "TUNE";
        kv[nk] = &ddecay[dsel]; kl[nk++] = "DEC";
        if (CH8[dsel]) { kv[nk] = &dcolor[dsel]; kl[nk++] = CH8[dsel]; }
        kv[nk] = &dvol[dsel];   kl[nk++] = "VOL";
        kv[nk] = &dpan[dsel];   kl[nk++] = "PAN";
        for (int i = 0; i < nk; i++) lcdknob(kv[i], 27 + 106 * (2 * i + 1) / (2 * nk), 49, 5, kl[i], 0.5f);
    }

    // ④ VOICE PICKER — ALL 16 voices in one row (acid order, 2-char pads). A tap SELECTs
    // + auditions; in MUT mode it toggles that voice's mute (dimmed + red slash = muted).
    for (int r = 0; r < TR_NV; r++) {
        int v = VL[r], x = 6 + r * 9, selp = (v == dsel), mtd = dmute[v];
        int pr = 0, hot = 0, foc = 0; void *wp = ui_wid_hash(0x90u + v, x, 64, 8, 9);
        if (ui_button_core(wp, x, 64, 8, 9, &foc, &pr, &hot)) {
            if (mutemode) dmute[v] = !dmute[v];
            else { dsel = v; tr808_fire(TR808_BASE, v, 1, 0, dtune, ddecay, dcolor); dtrig[v] = 1; }
        }
        rrectfill(x, 64, 8, 9, 1, mtd ? CLR_DARKER_PURPLE : selp ? CLR_TRUE_BLUE : CLR_DARK_BLUE);
        if (dtrig[v] > 0) { blend(BLEND_AVG); rrectfill(x, 64, 8, 9, 1, mtd ? CLR_LIGHT_GREY : CLR_WHITE); blend_reset(); }   // trigger flash (ghost-grey when muted)
        rrect(x, 64, 8, 9, 1, (selp || hot) ? CLR_WHITE : CLR_BROWNISH_BLACK);
        if (mtd) line(x + 1, 65, x + 6, 71, CLR_RED);        // muted = red slash (like the cartridge LED)
        font(FONT_TINY); print(AB8[v], x + (8 - text_width(AB8[v])) / 2, 66, mtd ? CLR_DARKER_GREY : selp ? CLR_WHITE : CLR_BLUE);
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

    // ② TOP ZONE (always): FX row + machine-global SWING + the 909 METAL XY (right gutter).
    draw_fxrow(3);                                         // DIST · SEND · VERB (aligned with the 303s)
    knob(&swing9, 14, 22, 6, "SWG", 0.0f);                 // machine-global shuffle, left gutter
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

    // ③ screen — VCE / KIT / FLAG (left) + GEN (right, the CLEAR + randomize menu).
    if (cbtn(0x1Fu, 6, 37, 16, 7, "VCE",  dscreen == DS_VCE))  dscreen = DS_VCE;
    if (cbtn(0x20u, 6, 45, 16, 7, "KIT",  dscreen == DS_KIT))  dscreen = DS_KIT;
    if (cbtn(0x21u, 6, 53, 16, 7, "FLAG", dscreen == DS_FLAG)) dscreen = DS_FLAG;
    if (cbtn(0x31u, 138, 38, 16, 7, "GEN", dscreen == DS_GEN)) dscreen = DS_GEN;
    if (cbtn(0x32u, 138, 47, 16, 7, "MUT", mutemode)) mutemode = !mutemode;   // pad tap → mute voices
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
    } else {                                                // DS_VCE — the picked voice's tone knobs, LCD-native
        font(FONT_TINY); plabel(TR909_NAME[d9sel], 80, 40, CLR_LIME_GREEN);
        float *kv[5]; const char *kl[5]; int nk = 0;        // TUNE / DEC / [char] / VOL / PAN — even-spaced
        kv[nk] = &d9tune[d9sel];  kl[nk++] = "TUNE";
        kv[nk] = &d9decay[d9sel]; kl[nk++] = "DEC";
        if (CH9[d9sel]) { kv[nk] = &d9color[d9sel]; kl[nk++] = CH9[d9sel]; }
        kv[nk] = &d9vol[d9sel];   kl[nk++] = "VOL";
        kv[nk] = &d9pan[d9sel];   kl[nk++] = "PAN";
        for (int i = 0; i < nk; i++) lcdknob(kv[i], 27 + 106 * (2 * i + 1) / (2 * nk), 49, 5, kl[i], 0.5f);
    }

    // ④ voice picker — ALL 11 voices in one row (acid order, 2-char amber pads). Tap =
    // SELECT+audition; in MUT mode = toggle that voice's mute (dimmed + red slash).
    for (int r = 0; r < TR9_NV; r++) {
        int v = VL[r], x = 6 + r * 13, selp = (v == d9sel), mtd = d9mute[v];
        int pr = 0, hot = 0, foc = 0; void *wp = ui_wid_hash(0x90u + v, x, 64, 12, 9);
        if (ui_button_core(wp, x, 64, 12, 9, &foc, &pr, &hot)) {
            if (mutemode) d9mute[v] = !d9mute[v];
            else { d9sel = v; tr909_fire(D909_BASE, v, 1, 0, d9tune, d9decay, d9color); d9trig[v] = 1; }
        }
        rrectfill(x, 64, 12, 9, 1, mtd ? CLR_DARKER_PURPLE : selp ? CLR_ORANGE : CLR_DARK_ORANGE);
        if (d9trig[v] > 0) { blend(BLEND_AVG); rrectfill(x, 64, 12, 9, 1, mtd ? CLR_LIGHT_GREY : CLR_WHITE); blend_reset(); }   // trigger flash (ghost-grey when muted)
        rrect(x, 64, 12, 9, 1, (selp || hot) ? CLR_WHITE : CLR_BROWNISH_BLACK);
        if (mtd) line(x + 2, 65, x + 9, 71, CLR_RED);        // muted = red slash
        font(FONT_TINY); print(AB9[v], x + (12 - text_width(AB9[v])) / 2, 66, mtd ? CLR_DARKER_GREY : selp ? CLR_BROWNISH_BLACK : CLR_LIGHT_YELLOW);
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

    // ② master live knobs
    knob(&mglu,  20, 22, 6, "GLU",  0.30f);
    knob(&mflt,  48, 22, 6, "FLT",  0.50f);
    knob(&mfres, 76, 22, 6, "RES",  0.35f);
    knob(&mfb,  104, 22, 6, "FB",   0.35f);
    knob(&mpump, 132, 22, 6, "PUMP", 0.0f);

    // ③ screen — soft-keys pick MIX (channel meters) or PCF (the drawable filter lane)
    if (cbtn(0x20u, 6, 38, 16, 8, "MIX", !mstflow)) mstflow = 0;
    if (cbtn(0x21u, 6, 47, 16, 8, "PCF",  mstflow)) mstflow = 1;
    chip(138, 38, "SNG", 0); chip(138, 47, "SCP", 0);
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
    } else {
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

    // ④ delay TIME selector
    print("DELAY", 6, 66, CLR_DARK_BROWN);
    for (int i = 0; i < 4; i++)
        if (cbtn(0x04u + i, 34 + i * 24, 64, 22, 9, DL[i], mdiv == i)) mdiv = i;

    // ⑤ per-machine delay SEND
    print("SEND", 6, 79, CLR_DARK_BROWN);
    knob(&msend[0], 36, 84, 5, MLAB[0], 0.10f);
    knob(&msend[1], 72, 84, 5, MLAB[1], 0.10f);
    knob(&msend[2], 108, 84, 5, MLAB[2], 0.0f);
    knob(&msend[3], 144, 84, 5, MLAB[3], 0.0f);
}

void init(void) {
    bpm(132);
    acid_init(&ac[0], 6, 36);                                          // 303a — the bass line (+ octave-down sub on slot 36)
    acid_init(&ac[1], 7, 37);                                          // 303b — an octave up = the acid lead (+ sub on 37)
    ac[1].base = 48;
    for (int i = 0; i < 2; i++) {
        ac[i].p[ACID_CUT] = 0.55f; ac[i].p[ACID_RES] = 0.70f; ac[i].p[ACID_ENV] = 0.55f;
        ac[i].p[ACID_DEC] = 0.45f; ac[i].p[ACID_ACC] = 0.55f;
        acid_define(&ac[i]);
        gen_line(i, 2);
    }
    tr808_build(TR808_BASE);                               // the shared, honest 808 kit (slots 9..22)
    for (int v = 0; v < TR_NV; v++) { dtune[v] = ddecay[v] = dcolor[v] = dvol[v] = 0.5f; }
    for (int v = 0; v < TR_NV; v++)  for (int s = 0; s < STEPS; s++) dprob[v][s]  = 100;   // every hit certain until loosened
    gen_drums(2);
    tr909_build(D909_BASE);                                // the honest 909 kit (slots 23..35)
    tr909_metal(D909_BASE, m9cut, m9res);
    for (int v = 0; v < TR9_NV; v++) { d9tune[v] = d9decay[v] = d9color[v] = d9vol[v] = 0.5f; }
    for (int v = 0; v < TR9_NV; v++) for (int s = 0; s < STEPS; s++) d9prob[v][s] = 100;
    gen_drums9(2);
    reverb(0.62f, 0.42f);                                  // the warm HALL — the 303s' space (tank 0), acidrack's tuning
    reverb_bus(1, 0.34f, 0.15f);                           // 909 → its own tight bright PLATE (tank 1)
    reverb_bus(2, 0.45f, 0.30f);                           // 808 → its own room (tank 2)
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
    if (playing) {
        float stepf = now() * (132 / 60.0f * 4);                       // 16th notes at 132 bpm
        int   ctr = (int)stepf;                                        // free-running counter (polymeter)
        step = ctr % STEPS;                                            // drums + shared display base
        float frac = stepf - ctr;
        if (ctr != laststep) {
            laststep = ctr;
            for (int i = 0; i < 2; i++) {                              // each 303 line at its OWN length
                int ls = ctr % plen[i]; lpos[i] = ls;
                if (mac[i].mute) { acid_off(&ac[i]); continue; }
                if (on[i][ls]) { acid_note(&ac[i], ac[i].base + pit[i][ls] + oct[i][ls] * 12, acc[i][ls], sld[i][ls]); mbop = 1; }
                else if (tie[i][ls]) acid_tie(&ac[i], sld[i][ls]);     // hold the previous note through
                else acid_off(&ac[i]);
            }
            int sw8 = (step & 1) ? (int)(swing8 * 55) : 0;        // swing: odd 16ths fire late (fire's delay param)
            int sw9 = (step & 1) ? (int)(swing9 * 55) : 0;
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
                        else if (L[p].sink == SK_PAN) tr808_pan(TR808_BASE, v, (eff - 0.5f) * 2);                  // PAN → -1..+1 (centre = 0.5)
                    }
                    tr808_fire(TR808_BASE, v, bo, sw8, dtune, ddecay, dcolor);
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
                        else if (L[p].sink == SK_PAN) tr909_pan(D909_BASE, v, (eff - 0.5f) * 2);                   // PAN → -1..+1 (centre = 0.5)
                    }
                    if (st) tr909_fire_stroke(D909_BASE, v, st, bo, sw9, 113, d9tune, d9decay, d9color);   // 113ms ≈ one 16th @132
                    else    tr909_fire(D909_BASE, v, bo, sw9, d9tune, d9decay, d9color);
                    for (int p = 0; p < nl; p++) if (L[p].sink == SK_ARG) *L[p].knob = sv[p];
                }
            }
        } else for (int i = 0; i < 2; i++) {
            int nx = (ctr + 1) % plen[i];
            acid_gate(&ac[i], frac, 0.0f, tie[i][nx]);                 // don't cut into a tie
        }
    } else for (int i = 0; i < 2; i++) acid_off(&ac[i]);
#ifdef DE_TRACE
    watch("face", "%d", face); watch("step", "%d", step); watch("cut", "%d", acid_cut_hz(&ac[0]));
    watch("mute0", "%d", mac[0].mute); watch("mute1", "%d", mac[1].mute);
#endif
}

void draw(void) {
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
