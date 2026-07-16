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
    "detail": "Nav = faces (tap a cartridge body to FOCUS, tap its LED to MUTE from anywhere). Sound: two TB-303 lines on the shared runtime/acid303.h (303b an octave up = the bass+acid-lead duo), the full TR-808 on the extracted runtime/tr808.h and the TR-909 on runtime/tr909.h (both byte-honest to the tr808/tr909 carts). Each 303 face: gear-drag knobs CUT/RES/ENV/DEC/ACC with a DF soft-key that flips the row to a DEEP page (SUB octave-down sub / ADEC two-decay / SLDT slide time / TRK filter tracking) and an FX soft-key that opens the per-machine FX panel (DRV drive / SEND delay / VERB reverb — the SAME three-knob row on every machine so they align); an LCD roll + NEW; the 16 NOTE-BARS (tap = on/off, drag up/down = pitch, snapped to a minor-pentatonic so it stays musical); and a FLAG soft-key that opens a 6-button palette in the screen (ACC/SLD/TIE/OCT+/OCT-/LEN) you arm then paint on the bars — per-step depth + per-line LENGTH (polymeter). Drum faces (808 blue, 909 amber): contextual TUNE/DEC/COL for the picked voice (or an FX panel — DIST/SEND/VERB, aligned with the 303s), an LCD kit overview, a voice picker, and the 16 hits on the bottom in the iconic 808 quarter-colours. FX routing: a shared tempo-synced delay + per-machine reverb (303s → a warm hall, 808/909 → their own plates with the kick kept dry) + per-machine drive. MST: master GLU/FLT/RES/FB/PUMP, a 4-channel mix overview, delay TIME + per-machine SEND. Knob feel: vertical = value, pull sideways for a fine gear, double-tap resets. (The old step-row + keybed is kept behind use_bars = 0.)",
    "controls": "Tap a cartridge to focus a machine; tap its LED to mute. PLAY runs the shared transport. 303: drag CUT/RES/ENV/DEC/ACC (sideways = fine, double-tap = reset); the DF soft-key flips to the DEEP page (SUB/ADEC/SLDT/TRK); the FX soft-key opens the per-machine FX panel (DRV/SEND/VERB); on the note-bars tap = note on/off, drag up/down = pitch; SEQ/FLAG soft-keys switch the screen between the roll and the flag palette (arm ACC/SLD/TIE/OCT+/OCT-/LEN, then tap bars); NEW = a fresh line. 808/909: tap a voice to pick+audition, tap a bottom cell to place a hit or DRAG across the row to paint a fill (the first cell decides on/off), turn TUNE/DEC/COL for the picked voice, the FX soft-key opens DIST/SEND/VERB, NEW = a fresh beat. MST: GLU tames level, FLT is the live DJ filter, PUMP ducks to the kick, pick a delay TIME + per-machine SEND."
  },
  "todo": [
    "808/909 completeness: only 8 of the 808's 16 (and 8 of the 909's 11) voices are reachable in the picker — add a 'more' page + turn the LCD kit-minimap into a full all-voice density grid so every voice is seen AND reachable.",
    "drum DEPTH: the drum hits are on/off only — add per-step ACCENT + trig PROBABILITY (RD-8/9; graft gprob/snap_prob from tr808.c/tr909.c) and the 909 STROKE family (flam/drag/ratchet — tr909.h already has tr909_fire_stroke) + the 909 METAL-filter XY.",
    "ARRANGEMENT (the last big acidrack gap): pattern BANKS A-D + a SONG chain + the 8-hex GEN song-code + WAV export.",
    "SWING (per-machine shuffle) + SAVE/LOAD (autosave patterns/knobs) — acidrack has both.",
    "waveshape flip: add a way to toggle each 303's oscillator wave (SAW <-> SQUARE, the a.wave field) — and maybe the drvmode waveshaper (SOFT/HARD/FOLD/ASYM). A small chip on the DEEP page or beside the DF toggle; acid_define() must be re-called on change (wave/drvmode bake into the instrument).",
    "303 DEEP knobs: SHIPPED (2026-07-15) a DF soft-key flips the zone-2 row between vanilla (CUT/RES/ENV/DEC/ACC) and a DEEP page (SUB/ADEC/SLDT/TRK) — the headline Devil Fish knobs, per-303, sub-osc wired on slots 36/37 (34/35 belong to the 909's 13-slot bank). DRV moved to the FX panel (aligns with the drums). Still not surfaced: ATK (soft attack) + SQL (env-sweep) + the accent-SWEEP mode; a fuller flow could page those too.",
    "wire the still-decorative soft-keys: SHIPPED (2026-07-15) the per-machine FX panel — the FX soft-key on every 303/808/909 opens an aligned DRV/DIST + SEND (shared delay) + VERB (reverb) row (draw_fxrow); reverb is real (303s → warm hall tank 0, 909 → tight plate tank 1, 808 → room tank 2, kicks kept dry) + per-machine drum drive. STILL decorative: 303 SCP, drum MAP/SCP, MST SNG/SCP; the 909 FX could also host the METAL XY + a per-machine SWING.",
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
static int  flagmode = 0;                    // step row is painting a flag (vs pitch/on-off)
static int  kpage[2];                        // per-303 knob page: 0 = vanilla, 1 = DEEP (Devil Fish + drive)
enum { FL_ACC, FL_SLD, FL_TIE, FL_OCTU, FL_OCTD, FL_LEN, FL_N };
static int  armed = FL_ACC;                  // which flag a bar-tap paints
static const char *FLNAME[FL_N] = { "ACC", "SLD", "TIE", "OCT+", "OCT-", "LEN" };
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
static int   dsel = TR_BD;                                 // selected drum voice
static float dtrig[TR_NV];                                 // per-voice flash: 1 on fire, decays (picker pad lights up)
// the 909 drum face — voices from the shared tr909.h
static int   d9grid[TR9_NV][STEPS];
static float d9tune[TR9_NV], d9decay[TR9_NV], d9color[TR9_NV];
static int   d9sel = TR9_BD;
static float d9trig[TR9_NV];
static float m9cut = 0.40f, m9res = 0.33f;                 // the 909 metal-filter XY

// the MST master/mix face
static float mglu = 0.30f, mflt = 0.5f, mfres = 0.35f, mfb = 0.35f, mpump = 0.0f;
static int   mdiv = 2;                                      // delay div: 0=1/16 1=1/8 2=dotted 3=1/4
static float msend[4] = { 0.10f, 0.10f, 0.0f, 0.0f };      // per-machine delay send: 303a 303b 808 909
static float fxverb[4] = { 0, 0, 0, 0 };                   // per-machine reverb send: 303a/b → warm hall (tank 0), 808 → tank 2, 909 → tank 1
static float dist8 = 0, dist9 = 0;                          // per-machine drum drive — ADDS on top of the kit's baked kick drive
static int   fxview[4];                                     // per-machine FX panel open? (index 0=303a 1=303b 2=808 3=909)
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

static void gen_line(int i) {
    static const int pool[8] = { 0, 0, 0, 3, 5, 7, 10, 12 };
    int prev = 0;
    for (int s = 0; s < STEPS; s++) {
        on[i][s] = rnd_between(0, 99) < 72;
        int p = pool[rnd_between(0, 7)];
        if (rnd_between(0, 99) < 35) p = prev;
        pit[i][s] = prev = p;
        acc[i][s] = rnd_between(0, 99) < 30;
        sld[i][s] = rnd_between(0, 99) < 22;
        oct[i][s] = rnd_between(0, 99) < 10 ? 1 : 0;
        tie[i][s] = (!on[i][s] && rnd_between(0, 99) < 15) ? 1 : 0;
    }
    plen[i] = STEPS;
}

static void gen_drums(void) {
    for (int v = 0; v < TR_NV; v++) for (int s = 0; s < STEPS; s++) dgrid[v][s] = 0;
    for (int s = 0; s < STEPS; s += 4) dgrid[TR_BD][s] = 1;         // kick on the floor
    dgrid[TR_SD][4] = dgrid[TR_SD][12] = 1;                         // snare backbeat
    for (int s = 0; s < STEPS; s += 2) dgrid[TR_CH][s] = 1;         // 8th closed hats
    dgrid[TR_OH][2] = dgrid[TR_OH][10] = 1;                         // off-beat open hat
    for (int s = 0; s < STEPS; s++) {                              // a little spice
        if (rnd_between(0, 99) < 12) dgrid[TR_CP][s] = 1;
        if (rnd_between(0, 99) < 10) dgrid[TR_CB][s] = 1;
    }
}

static void gen_drums9(void) {
    for (int v = 0; v < TR9_NV; v++) for (int s = 0; s < STEPS; s++) d9grid[v][s] = 0;
    for (int s = 0; s < STEPS; s += 4) d9grid[TR9_BD][s] = 1;       // house four-on-floor
    d9grid[TR9_CP][4] = d9grid[TR9_CP][12] = 1;                     // clap backbeat
    for (int s = 0; s < STEPS; s += 2) d9grid[TR9_CH][s] = 1;       // closed hats
    for (int s = 2; s < STEPS; s += 4) d9grid[TR9_OH][s] = 1;       // off-beat open hats
    for (int s = 0; s < STEPS; s++) if (rnd_between(0, 99) < 8) d9grid[TR9_RC][s] = 1;  // ride spice
}

// ── candy widgets ──────────────────────────────────────────────────────────
static void plabel(const char *s, int cx, int y, int col) { print(s, cx - text_width(s) / 2, y, col); }

// TAP-SETTLE: after a drag widget (knob / bar / lane) lifts, a mouse can deliver a
// spurious second button-down a few frames later at the drifted position — a "bounce"
// that lands as a stray tap (muted a cartridge in a playtest). Ignore nav taps for a
// short window after any drag was held. (acidrack's fix, ported.)
#define TAP_SETTLE 12   // ~200ms; the observed bounce lagged the release by 8 frames
static int g_drag_frame = -100;             // last frame a drag widget was captured (ui_frame_ct clock)
static int g_drag_y = 100;                   // ...and WHERE it was (canvas y) — a drag released below the
                                             // tab row (y>=14) can't bounce onto a tab, so it needn't block one
static int tap_settled(void) { return ui_frame_ct - g_drag_frame >= TAP_SETTLE; }

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
static void knob(float *v, int cx, int cy, int r, const char *label, float def) {
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
    if (held) { blend(BLEND_AVG); circfill(cx, cy, r + 1, CLR_WHITE); blend_reset(); }   // grab-glow halo
    circfill(cx, cy, r, CLR_INDIGO);
    ring(cx, cy, r - 2, r - 1, 165, 285, CLR_PINK);
    ring(cx, cy, r - 2, r - 1, -15, 105, CLR_DARKER_PURPLE);
    float ang = 150 + *v * 240;
    if (held) ring(cx, cy, r - 3, r, 150, ang, CLR_LIGHT_YELLOW);      // FAT value band — fills as you turn
    circ(cx, cy, r, held ? CLR_WHITE : hot ? CLR_LIGHT_PEACH : CLR_BROWNISH_BLACK);
    line(cx + (int)dx(1, ang), cy + (int)dy(1, ang), cx + (int)dx(r - 1, ang), cy + (int)dy(r - 1, ang),
         fine ? CLR_ORANGE : CLR_WHITE);                              // pointer goes amber in fine gear
    font(FONT_TINY);
    if (held) { int p = (int)(*v * 99 + 0.5f); char b[3] = { (char)('0' + p / 10), (char)('0' + p % 10), 0 };
                plabel(b, cx, cy + r + 1, CLR_DARK_GREEN); }
    else plabel(label, cx, cy + r + 1, CLR_DARK_BROWN);
}

static int cbtn(unsigned seed, int x, int y, int w, int hh, const char *s, int on2) {
    int pr = 0, hot = 0, foc = 0; void *wid = ui_wid_hash(seed, x, y, w, hh);
    int act = ui_button_core(wid, x, y, w, hh, &foc, &pr, &hot);
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

// ── the cartridge nav strip (zone 1) ─────────────────────────────────────────
// each cartridge is a COMPOUND control: left body taps to FOCUS the face, the
// right LED taps to MUTE the machine (from any face). Two non-overlapping
// sub-buttons, so ui.h's visual-hit-wins routes touch cleanly.
static float lvg[M_N], lvgt[M_N];      // slider grab value + time (tap-vs-drag: a tap = mute)
static int   lvmv[M_N] = { -100, -100, -100, -100, -100 };   // frame of the last drag/toggle — a bounce right after it is ignored
static void cartridge(int m) {
    int x = 19 + m * 25, y = 3, foc = (m == face), live = !mac[m].mute;  // pitch 25; y3 = a small top safe margin off the device edge
    // --- the TAB: tap = focus. Drag-free, so it never fights the iOS top-edge pull. ---
    int prf = 0, hotf = 0, fof = 0;
    void *wf = ui_wid_hash(0x70u + m, x, y, 24, 10);
    int af = ui_button_core(wf, x, y, 24, 10, &fof, &prf, &hotf);
    if (af && (g_drag_y >= 14 || tap_settled())) face = m;           // only a drag ending IN the tab row can bounce a tab
    rrectfill(x, y, 24, 10, 2, foc ? mac[m].col : mac[m].lo);
    if (foc) { blend(BLEND_AVG); line(x + 2, y + 1, x + 21, y + 1, CLR_WHITE); blend_reset(); }   // top sheen
    rrect(x, y, 24, 10, 2, (foc || hotf) ? CLR_WHITE : CLR_BROWNISH_BLACK);
    font(FONT_TINY);
    print(mac[m].name, x + (24 - text_width(mac[m].name)) / 2, y + 2, foc ? CLR_BROWNISH_BLACK : mac[m].col);

    // --- BELOW the tab: a little HORIZONTAL level slider that doubles as the mute
    //     (acidwire's tap-vs-drag, turned sideways). TAP = mute · HORIZONTAL DRAG =
    //     level. Down here the drag never triggers the phone's top-edge gesture, and
    //     mute gets its own target instead of a sliver crammed beside the name. ---
    int sx = x, sy = 14, sw = 24, sh = 6;   // cluster sits 3px down with the tabs, filling the old gap above the knobs
    ui_reg(&level[m], sx, sy, sw, sh, 0);
    UiCap *c = ui_cap_for(&level[m]);
    int held = c != 0;
    if (c) {
        g_drag_frame = ui_frame_ct; g_drag_y = c->cy;
        if (!c->has_v0) { c->has_v0 = 1; c->v0 = level[m]; c->by = c->cx; }   // by = the x-anchor (horizontal)
        int px = c->released ? c->rx : c->cx;
        level[m] = clamp(c->v0 + (px - c->by) / 40.0f, 0, 1);        // relative → unlimited travel
        c->v0 = level[m]; c->by = px;
    }
    if (ui_grabbed(&level[m])) { lvg[m] = level[m]; lvgt[m] = now(); }
    if (ui_released(&level[m])) {
        float dv = level[m] - lvg[m]; if (dv < 0) dv = -dv;
        if (dv >= 0.03f) lvmv[m] = ui_frame_ct;                      // that was a level DRAG (a bounce may follow)
        else if (now() - lvgt[m] < 0.3f && ui_frame_ct - lvmv[m] >= TAP_SETTLE) {   // a real, quick TAP = mute
            level[m] = lvg[m]; mac[m].mute = !mac[m].mute; lvmv[m] = ui_frame_ct;   // (and block ITS bounce too)
        }
    }
    // draw the slider: a left-to-right fill = level; red-tinted when muted
    rrectfill(sx, sy, sw, sh, 1, CLR_BROWNISH_BLACK);
    int fw = (int)(level[m] * (sw - 2) + 0.5f);
    if (fw > 0) rrectfill(sx + 1, sy + 1, fw, sh - 2, 1, live ? mac[m].col : CLR_DARK_GREY);
    if (!live) { blend(BLEND_AVG); rrectfill(sx + 1, sy + 1, sw - 2, sh - 2, 1, CLR_RED); blend_reset(); }   // muted tint
    rrect(sx, sy, sw, sh, 1, !live ? CLR_RED : (held ? CLR_WHITE : CLR_BROWNISH_BLACK));
}

static void navspine(void) {
    // transport (shared)
    int px = 5, py = 3, pw = 14, ph = 10, pr = 0, hot = 0, fo = 0;    // play, in from the left bezel
    void *w = ui_wid_hash(0x01u, px, py, pw, ph);
    if (ui_button_core(w, px, py, pw, ph, &fo, &pr, &hot) && tap_settled()) { playing = !playing; laststep = -1; }
    rrectfill(px, py, pw, ph, 2, playing ? CLR_TRUE_BLUE : CLR_DARK_BROWN);
    rrect(px, py, pw, ph, 2, hot ? CLR_WHITE : CLR_BROWNISH_BLACK);
    if (playing) { rectfill(px + 4, py + 3, 2, 4, CLR_WHITE); rectfill(px + 8, py + 3, 2, 4, CLR_WHITE); }
    else trifill(px + 5, py + 3, px + 5, py + 7, px + 10, py + 5, CLR_WHITE);
    for (int m = 0; m < M_N; m++) cartridge(m);

    // HOME (meta) — reserved space only; the app shell owns the real leave-cart gesture
    int hx = 143, hy = 3, hw = 12, hh = 10, hpr = 0, hhot = 0, hfo = 0;   // home, in from the right bezel
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
    if (m < 2) knob(&ac[m].p[ACID_DRV], 40, 26, 6, "DRV",  0.35f);   // 303 voice drive
    else       knob(m == 2 ? &dist8 : &dist9, 40, 26, 6, "DIST", 0.0f);
    knob(&msend[m],  80, 26, 6, "SEND", m < 2 ? 0.10f : 0.0f);       // → the shared delay
    knob(&fxverb[m], 120, 26, 6, "VERB", 0.0f);                     // → the reverb
}

// ── a 303 face (zones 2–5) ───────────────────────────────────────────────────
static void draw_303(int i) {
    Acid *a = &ac[i];
    // ② gear-drag knob row. FX button → the per-machine FX panel (DRV/SEND/VERB,
    // aligned with the drums). Else the DF button flips the row between the vanilla
    // 303 and the DEEP page (Devil Fish depth; drive now lives on the FX panel).
    if (cbtn(0x06u, 138, 38, 16, 8, "FX", fxview[i])) fxview[i] = !fxview[i];
    if (cbtn(0x07u, 138, 29, 16, 8, "DF", kpage[i]))  kpage[i]  = !kpage[i];
    if (fxview[i]) {
        draw_fxrow(i);
    } else if (!kpage[i]) {                                                // page 0 — vanilla
        knob(&a->p[ACID_CUT], 20, 26, 6, "CUT", 0.55f); knob(&a->p[ACID_RES], 48, 26, 6, "RES", 0.70f);
        knob(&a->p[ACID_ENV], 76, 26, 6, "ENV", 0.55f); knob(&a->p[ACID_DEC], 104, 26, 6, "DEC", 0.45f);
        knob(&a->p[ACID_ACC], 132, 26, 6, "ACC", 0.55f);
    } else {                                                              // page 1 — DEEP: the headline Devil Fish knobs
        knob(&a->p[ACID_SUB],  20, 26, 6, "SUB",  0.0f);  knob(&a->p[ACID_ADEC], 48, 26, 6, "ADEC", 0.40f);
        knob(&a->p[ACID_SLDT], 76, 26, 6, "SLDT", 0.14f); knob(&a->p[ACID_TRK], 104, 26, 6, "TRK",  0.0f);
    }

    // ③ display — soft-keys pick what the screen does: SEQ = the roll (+ NEW),
    // FLAG = the per-step flag palette (arm one, then paint it on the bars below).
    if (cbtn(0x08u, 6, 38, 16, 8, "SEQ",  !flagmode)) flagmode = 0;
    if (cbtn(0x09u, 6, 47, 16, 8, "FLAG",  flagmode)) flagmode = 1;
    chip(138, 47, "SCP", 0);   // (FX + DF buttons are drawn with the knob row above)
    rrectfill(24, 37, 112, 24, 3, CLR_BROWNISH_BLACK);
    rrectfill(27, 39, 106, 20, 2, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = 40; y < 58; y += 2) line(27, y, 132, y, CLR_BROWNISH_BLACK); blend_reset();
    if (flagmode) {
        for (int f = 0; f < FL_N; f++)                                // the 6-flag palette IN the screen
            if (cbtn(0x0Au + f, 30 + (f % 3) * 34, 40 + (f / 3) * 9, 32, 8, FLNAME[f], armed == f)) armed = f;
    } else {
        font(FONT_TINY); print("132", 29, 40, CLR_MEDIUM_GREEN);      // bpm lives in the screen
        if (cbtn(0x02u, 113, 39, 18, 7, "NEW", 0)) gen_line(i);       // ...and so does NEW
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
            if (flagmode) {                              // FLAG mode: tap or DRAG paints the armed flag
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
            rrect(bx, by, bw, bh, 1, (here || (!flagmode && s == sel[i])) ? CLR_WHITE : CLR_BROWNISH_BLACK);
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
    static const int VL[8]   = { TR_BD, TR_SD, TR_LT, TR_CP, TR_CH, TR_OH, TR_CB, TR_CY };  // core 8 (‹more› = later)
    static const int QCLR[4] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_WHITE };

    // ② the PICKED voice's knobs (contextual — repaint on pick), or the FX panel
    if (fxview[2]) {
        draw_fxrow(2);                                     // DIST · SEND · VERB (aligned with the 303s)
    } else {
        font(FONT_TINY); print("VOICE", 6, 18, CLR_DARK_BROWN); print(TR808_NAME[dsel], 6, 24, CLR_TRUE_BLUE);
        knob(&dtune[dsel],  52, 27, 6, "TUNE", 0.5f);
        knob(&ddecay[dsel], 88, 27, 6, "DEC",  0.5f);
        knob(&dcolor[dsel], 124, 27, 6, "COL", 0.5f);
    }

    // ③ screen — whole-kit overview (readout) + playhead + NEW, soft-keys flank
    chip(6, 38, "KIT", !fxview[2]);
    if (cbtn(0x22u, 6, 47, 16, 8, "FX", fxview[2])) fxview[2] = !fxview[2];
    chip(138, 38, "MAP", 0); chip(138, 47, "SCP", 0);
    rrectfill(24, 37, 112, 24, 3, CLR_BROWNISH_BLACK);
    rrectfill(27, 39, 106, 20, 2, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = 40; y < 58; y += 2) line(27, y, 132, y, CLR_BROWNISH_BLACK); blend_reset();
    rrectfill(28, 40, 15, 7, 1, CLR_TRUE_BLUE); font(FONT_TINY); print("808", 30, 41, CLR_WHITE);   // identity badge
    if (cbtn(0x02u, 113, 39, 18, 7, "NEW", 0)) gen_drums();
    for (int r = 0; r < 8; r++) {                          // kit minimap
        int v = VL[r], gy = 42 + r * 2;
        for (int s = 0; s < STEPS; s++) {
            int gx = 48 + s * 5;
            if (s == step && playing) { blend(BLEND_AVG); rectfill(gx - 1, 42, 4, 16, CLR_MEDIUM_GREEN); blend_reset(); }
            if (dgrid[v][s]) rectfill(gx, gy, 3, 1, v == dsel ? CLR_LIGHT_YELLOW : CLR_LIME_GREEN);
        }
    }

    // ④ VOICE PICKER — one row of pads (selectors), just above the hits
    for (int r = 0; r < 8; r++) {
        int v = VL[r], x = 6 + r * 18, selp = (v == dsel);
        int pr = 0, hot = 0, foc = 0; void *wp = ui_wid_hash(0x90u + v, x, 64, 17, 9);
        if (ui_button_core(wp, x, 64, 17, 9, &foc, &pr, &hot)) { dsel = v; tr808_fire(TR808_BASE, v, 1, 0, dtune, ddecay, dcolor); dtrig[v] = 1; }
        rrectfill(x, 64, 17, 9, 1, selp ? CLR_TRUE_BLUE : CLR_DARK_BLUE);
        if (dtrig[v] > 0) { blend(BLEND_AVG); rrectfill(x, 64, 17, 9, 1, CLR_WHITE); blend_reset(); }   // flash when the voice triggers
        rrect(x, 64, 17, 9, 1, (selp || hot || dtrig[v] > 0) ? CLR_WHITE : CLR_BROWNISH_BLACK);
        font(FONT_TINY); print(TR808_NAME[v], x + (17 - text_width(TR808_NAME[v])) / 2, 66, selp ? CLR_WHITE : CLR_BLUE);
    }

    // ⑤ the HITS — the picked voice's 16 steps, on the BOTTOM (thumb surface).
    // tap = toggle one · DRAG across = PAINT the same on/off (decided on the first cell) — draw a fill in one stroke
    for (int s = 0; s < STEPS; s++) {
        int x = 6 + s * 9, on2 = dgrid[dsel][s], here = (s == step && playing);
        int fc = on2 ? QCLR[s / 4] : CLR_DARKER_PURPLE;
        if (here) fc = on2 ? CLR_WHITE : CLR_DARKER_GREY;
        void *ws = ui_wid_hash(0xA0u + s, x, 81, 8, 16);
        ui_reg(ws, x, 81, 8, 16, 0);
        UiCap *c = ui_cap_for(ws);
        if (c) {
            g_drag_frame = ui_frame_ct; g_drag_y = c->cy;
            if (ui_grabbed(ws)) { paint_val = !dgrid[dsel][s]; if (paint_val) { tr808_fire(TR808_BASE, dsel, 1, 0, dtune, ddecay, dcolor); dtrig[dsel] = 1; mbop = 1; } }
            int fx = c->released ? c->rx : c->cx, cell = (fx - 6) / 9;
            if (cell < 0) cell = 0; if (cell >= STEPS) cell = STEPS - 1;
            dgrid[dsel][cell] = paint_val;
        }
        rrectfill(x, 81, 8, 16, 1, fc);
        rrect(x, 81, 8, 16, 1, CLR_BROWNISH_BLACK);
    }
}

// ── the 909 DRUM face — same compact model as the 808, but its OWN identity:
// AMBER/steel (vs the 808's blue) so you know which drum machine you're on.
static void draw_909(void) {
    static const int VL[8] = { TR9_BD, TR9_SD, TR9_CH, TR9_OH, TR9_CP, TR9_LT, TR9_CC, TR9_RC };

    // ② the picked voice's knobs (contextual), or the FX panel
    if (fxview[3]) {
        draw_fxrow(3);                                     // DIST · SEND · VERB (aligned with the 303s)
    } else {
        font(FONT_TINY); print("VOICE", 6, 18, CLR_DARK_BROWN); print(TR909_NAME[d9sel], 6, 24, CLR_ORANGE);
        knob(&d9tune[d9sel],  52, 27, 6, "TUNE", 0.5f);
        knob(&d9decay[d9sel], 88, 27, 6, "DEC",  0.5f);
        knob(&d9color[d9sel], 124, 27, 6, "COL", 0.5f);
    }

    // ③ screen — kit overview + playhead + NEW, amber 909 badge
    chip(6, 38, "KIT", !fxview[3]);
    if (cbtn(0x23u, 6, 47, 16, 8, "FX", fxview[3])) fxview[3] = !fxview[3];
    chip(138, 38, "MAP", 0); chip(138, 47, "SCP", 0);
    rrectfill(24, 37, 112, 24, 3, CLR_BROWNISH_BLACK);
    rrectfill(27, 39, 106, 20, 2, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = 40; y < 58; y += 2) line(27, y, 132, y, CLR_BROWNISH_BLACK); blend_reset();
    rrectfill(28, 40, 15, 7, 1, CLR_ORANGE); font(FONT_TINY); print("909", 30, 41, CLR_BROWNISH_BLACK);
    if (cbtn(0x02u, 113, 39, 18, 7, "NEW", 0)) gen_drums9();
    for (int r = 0; r < 8; r++) {
        int v = VL[r], gy = 42 + r * 2;
        for (int s = 0; s < STEPS; s++) {
            int gx = 48 + s * 5;
            if (s == step && playing) { blend(BLEND_AVG); rectfill(gx - 1, 42, 4, 16, CLR_MEDIUM_GREEN); blend_reset(); }
            if (d9grid[v][s]) rectfill(gx, gy, 3, 1, v == d9sel ? CLR_LIGHT_YELLOW : CLR_LIME_GREEN);
        }
    }

    // ④ voice picker — amber pads
    for (int r = 0; r < 8; r++) {
        int v = VL[r], x = 6 + r * 18, selp = (v == d9sel);
        int pr = 0, hot = 0, foc = 0; void *wp = ui_wid_hash(0x90u + v, x, 64, 17, 9);
        if (ui_button_core(wp, x, 64, 17, 9, &foc, &pr, &hot)) { d9sel = v; tr909_fire(D909_BASE, v, 1, 0, d9tune, d9decay, d9color); d9trig[v] = 1; }
        rrectfill(x, 64, 17, 9, 1, selp ? CLR_ORANGE : CLR_DARK_ORANGE);
        if (d9trig[v] > 0) { blend(BLEND_AVG); rrectfill(x, 64, 17, 9, 1, CLR_WHITE); blend_reset(); }   // flash when the voice triggers
        rrect(x, 64, 17, 9, 1, (selp || hot || d9trig[v] > 0) ? CLR_WHITE : CLR_BROWNISH_BLACK);
        font(FONT_TINY); print(TR909_NAME[v], x + (17 - text_width(TR909_NAME[v])) / 2, 66, selp ? CLR_BROWNISH_BLACK : CLR_LIGHT_YELLOW);
    }

    // ⑤ the HITS — picked voice's 16 steps at the bottom; amber, white downbeat accents.
    // tap = toggle one · DRAG across = PAINT the same on/off (decided on the first cell)
    for (int s = 0; s < STEPS; s++) {
        int x = 6 + s * 9, on2 = d9grid[d9sel][s], here = (s == step && playing);
        int fc = on2 ? (s % 4 == 0 ? CLR_LIGHT_YELLOW : CLR_ORANGE) : CLR_DARKER_PURPLE;
        if (here) fc = on2 ? CLR_WHITE : CLR_DARKER_GREY;
        void *ws = ui_wid_hash(0xA0u + s, x, 81, 8, 16);
        ui_reg(ws, x, 81, 8, 16, 0);
        UiCap *c = ui_cap_for(ws);
        if (c) {
            g_drag_frame = ui_frame_ct; g_drag_y = c->cy;
            if (ui_grabbed(ws)) { paint_val = !d9grid[d9sel][s]; if (paint_val) { tr909_fire(D909_BASE, d9sel, 1, 0, d9tune, d9decay, d9color); d9trig[d9sel] = 1; mbop = 1; } }
            int fx = c->released ? c->rx : c->cx, cell = (fx - 6) / 9;
            if (cell < 0) cell = 0; if (cell >= STEPS) cell = STEPS - 1;
            d9grid[d9sel][cell] = paint_val;
        }
        rrectfill(x, 81, 8, 16, 1, fc);
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
    knob(&mglu,  20, 27, 6, "GLU",  0.30f);
    knob(&mflt,  48, 27, 6, "FLT",  0.50f);
    knob(&mfres, 76, 27, 6, "RES",  0.35f);
    knob(&mfb,  104, 27, 6, "FB",   0.35f);
    knob(&mpump, 132, 27, 6, "PUMP", 0.0f);

    // ③ screen — soft-keys pick MIX (channel meters) or PCF (the drawable filter lane)
    if (cbtn(0x20u, 6, 38, 16, 8, "MIX", !mstflow)) mstflow = 0;
    if (cbtn(0x21u, 6, 47, 16, 8, "PCF",  mstflow)) mstflow = 1;
    chip(138, 38, "SNG", 0); chip(138, 47, "SCP", 0);
    rrectfill(24, 37, 112, 24, 3, CLR_BROWNISH_BLACK);
    rrectfill(27, 39, 106, 20, 2, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = 40; y < 58; y += 2) line(27, y, 132, y, CLR_BROWNISH_BLACK); blend_reset();
    font(FONT_TINY);
    if (mstflow == 0) {
        for (int m = 0; m < 4; m++) {                        // the four channel meters
            int y = 41 + m * 4, muted = mac[m].mute, act = machine_active(m);
            print(MLAB[m], 30, y, muted ? CLR_DARKER_GREY : mac[m].col);
            int bw = muted ? 3 : act ? 78 : 30;
            rectfill(52, y, bw, 2, muted ? CLR_DARKER_GREY : act ? CLR_LIME_GREEN : mac[m].col);
            if (muted) print("M", 124, y, CLR_RED);
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
        gen_line(i);
    }
    tr808_build(TR808_BASE);                               // the shared, honest 808 kit (slots 9..22)
    for (int v = 0; v < TR_NV; v++) { dtune[v] = ddecay[v] = dcolor[v] = 0.5f; }
    gen_drums();
    tr909_build(D909_BASE);                                // the honest 909 kit (slots 23..35)
    tr909_metal(D909_BASE, m9cut, m9res);
    for (int v = 0; v < TR9_NV; v++) { d9tune[v] = d9decay[v] = d9color[v] = 0.5f; }
    gen_drums9();
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
            for (int v = 0; v < TR_NV; v++)                       // the 808 line, same transport
                if (dgrid[v][step] && !mac[M_808].mute) { tr808_fire(TR808_BASE, v, 0, 0, dtune, ddecay, dcolor); dtrig[v] = 1; }
            for (int v = 0; v < TR9_NV; v++)                      // the 909 line
                if (d9grid[v][step] && !mac[M_909].mute) { tr909_fire(D909_BASE, v, 0, 0, d9tune, d9decay, d9color); d9trig[v] = 1; }
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
