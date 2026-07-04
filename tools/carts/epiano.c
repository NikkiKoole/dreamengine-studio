/* de:meta
{
  "title": "epiano",
  "status": "active",
  "created": "2026-06-07",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "analog-voice-modeling",
    "adsr-envelope"
  ],
  "lineage": "Showcases INSTR_EPIANO with three electromechanical machines (Rhodes/Wurli/Clav), each with its authentic stompbox chain; novel in the per-machine pedal + preset UI modelled after the real hardware signal path.",
  "todo": [
    "device-adaptive redesign (Phase 3 — design/device-adaptive-layout.md): keybed width scales to the device (more octaves on iPad/landscape, per otafit's finger-per-semitone lesson), pedalboard reflows (phone: compact row; iPad: full board beside the keys). LIKELY GRADUATES INTO keybed.h so every keybed cart (moog/touchpiano/mellotron) benefits — a shared win. Consider a lock-landscape hint on phone-portrait"
  ],
  "description": "INSTR_EPIANO showcase - the electromechanical keyboards, rooted in the originals: pick the MACHINE (1 RHODES / 2 WURLI / 3 CLAV - each its own colour), then flip ITS authentic stompboxes. Every key sums 12 decaying inharmonic sine modes through a PICKUP NONLINEARITY - the bark/buzz/honk that makes it electric, not a dull bell. Each machine carries the modulation that actually defines it, as self-contained pedals (footswitch + its own knobs): RHODES - VIBE (Suitcase stereo vibrato), DYNO (bright sheen), PHASE (the 70s swirl, RATE+STR); WURLI - TREM (the built-in tremolo), BUZZ (reed dig-in growl); CLAV - WAH (the Superstition funk), MUTE (the D6 damper). A BARK knob on every machine drives the pickup hard (read live on ringing notes). A small per-machine PRESET column (stage/suitcase/dyno, wurli/buzz, clav/mute) flips the pedals for you. White A S D F G H J K, black W E T Y U; SLIDE across the keys for glissando; Z/X octave; M autoplay. The DX/digital EP is the fm cart; this is the electromechanical one. Multitouch."
}
de:meta */
#include "studio.h"
#include "fxicons.h"     // shared effect icons + colours (same "pedals" as the pedalboard)
#include <math.h>
#define KEYBED_WHITE_KEYS "ASDFGHJK"   // epiano's one-octave-plus-C manual (8 whites)
#define KEYBED_BLACK_KEYS "WE TYU"     // C# D# (gap) F# G# A#
#include "keybed.h"      // the shared polyphonic keybed: touch + mouse + QWERTY + MIDI

// ── EPIANO ───────────────────────────────────────────────────────────────────────────────────
// The electromechanical keyboards, rooted in the originals — pick the MACHINE, then flip its own
// authentic PEDALS. One INSTR_EPIANO slot voiced three ways, each with the modulation that
// actually defines it. Every effect is a self-contained stompbox: a footswitch + its own knob(s).
//
//   RHODES (tine)  — VIBE (Suitcase stereo vibrato = auto-pan) · DYNO (bright stereo chorus) ·
//                    PHASE (the 70s swirl). All off = a dry Stage Rhodes.
//   WURLI (reed)   — TREM (its built-in mono tremolo) · BUZZ (reed dig-in growl).
//   CLAV (string)  — WAH (the Superstition funk) · MUTE (the D6 damper tone).
//   + a BARK knob on every machine (the pickup driven hard — read live on ringing notes).
//
// The pedal icons + colours come from runtime/fxicons.h (the pedalboard's exact visual language).
// VIBE drives the real bus auto-pan (instrument_autopan = antiphase tremolo); its icon is a custom
// L↔R sway drawn for this cart (the effect has no standard stompbox icon — ep_icon() draws it).
//
// controls: white A S D F G H J K · black W E . T Y U · Z/X octave · 1/2/3 machine · tap pedals/
//           footswitches · drag knobs · SLIDE across the keys for glissando · M autoplay. Multitouch.

#define I_EP 5

// ── the keybed ── (layout/voices/glissando/input live in keybed.h now)
#define NWHITE 8                            // 8 white keys = one octave + the top C
static const char WKEY[NWHITE] = { 'A','S','D','F','G','H','J','K' };   // QWERTY labels (drawing only)
static const char BLBL[NWHITE] = { 'W','E', 0 ,'T','Y','U', 0 , 0 };   // black-key label after white k

// ── the engine param layer (drives the tuned apply_* — unchanged voicing) ──
enum { SL_INST, SL_BRITE, SL_BARK, SL_WAH, SL_TREM, NSL };
static float val[NSL] = { 0.08f, 0.30f, 0.25f, 0.5f, 0.35f };
static int   wahmode = 0;        // clav wah — NOT `wah` (clashes with the API)
static int   phasermode = 0;     // phased-Rhodes swirl — NOT `phaser`
static float phaser_rate = 0.9f, phaser_str = 0.6f, trem_spd = 0.45f;   // knob-driven

// ── the machines ──
enum { M_RHODES, M_WURLI, M_CLAV, NMACHINE };
static const char *MNAME[NMACHINE] = { "RHODES", "WURLI", "CLAV" };
static const int M_BODY[NMACHINE]   = { CLR_DARK_BROWN, CLR_BLUE_GREEN,   CLR_DARK_PURPLE };
static const int M_ACCENT[NMACHINE] = { CLR_DARK_ORANGE, CLR_LIGHT_YELLOW, CLR_MAUVE };
static const int M_LIT[NMACHINE]    = { CLR_PEACH,       CLR_LIGHT_PEACH,  CLR_PINK };
static const float M_INST[NMACHINE] = { 0.15f, 0.50f, 0.85f };   // the instrument-macro detents (original values)
static int machine = M_RHODES;

// ── the PEDALS: a footswitch + 1-2 knobs each, per machine ──
// pedal icons: real fxicons (PHASE/TREM/WAH genuinely ARE those effects) or CUSTOM ones drawn for
// this cart (VIBE/DYNO/BUZZ/MUTE — machine-specific, not standard pedals; never borrow an icon from
// an effect a pedal isn't based on). The custom kinds are negative; ep_icon() draws them.
#define ICON_NONE (-1)
#define ICON_PAN  (-2)   // VIBE — stereo vibrato; custom L↔R sway icon (the real bus auto-pan has no stock stompbox icon)
#define ICON_DYNO (-3)   // DYNO — the bright "sheen" mod
#define ICON_BUZZ (-4)   // BUZZ — reed overdrive growl
#define ICON_MUTE (-5)   // MUTE — the D6 damper
typedef struct { int icon; const char *name; int nk; const char *kl[2]; float kdef[2]; } PedalDef;
static const PedalDef PED[NMACHINE][3] = {
  { { ICON_PAN,  "VIBE",  2, {"SPD","DEP"}, {0.45f,0.50f} },    // RHODES — Suitcase stereo vibrato
    { ICON_DYNO, "DYNO",  1, {"AMT", 0},    {0.55f,0} },        //          Dyno bright-chorus
    { FX_PHASER, "PHASE", 2, {"RATE","STR"}, {0.50f,0.60f} } }, //          the swirl (rate + strength)
  { { FX_TREM,   "TREM",  2, {"SPD","DEP"}, {0.45f,0.55f} },    // WURLI  — built-in tremolo
    { ICON_BUZZ, "BUZZ",  1, {"AMT", 0},    {0.50f,0} },        //          reed dig-in growl
    { ICON_NONE,0,0,{0,0},{0,0} } },
  { { FX_WAH,    "WAH",   1, {"AMT", 0},    {0.60f,0} },        // CLAV   — the funk wah
    { ICON_MUTE, "MUTE",  1, {"TONE",0},    {0.40f,0} },        //          the D6 damper
    { ICON_NONE,0,0,{0,0},{0,0} } },
};
static const int  NPED[NMACHINE] = { 3, 2, 2 };
static bool  ped_on[NMACHINE][3] = { { true, false, false }, { true, false, false }, { true, false, false } };
static float ped_k [NMACHINE][3][2];
static float barkv [NMACHINE] = { 0.25f, 0.40f, 0.55f };

// PRESETS — per machine, a small vertical stack of buttons at the LEFT of the option row. Each is a
// full named patch: the VOICE macros (bright, bark) + which pedals are on. bright/bark are the
// ORIGINAL values that gave each its character (rho-brite is barky 0.78/0.55, etc.). Pick one, or
// flip footswitches / dial BARK by hand (clears the highlight).
typedef struct { const char *name; float bright, bark; bool on[3]; } MPreset;
static const MPreset MPRESET[NMACHINE][3] = {
  { { "stage",    0.30f, 0.25f, { 0, 0, 0 } },   // RHODES: warm dry tine (the original "rhodes")
    { "suitcase", 0.20f, 0.12f, { 1, 0, 1 } },   //         mellow + stereo vibrato + the phased swirl
    { "dyno",     0.78f, 0.55f, { 0, 1, 1 } } }, //         bright + barky, chorus + phaser ("rho brite")
  { { "wurli",    0.45f, 0.60f, { 1, 0, 0 } },   // WURLI:  soul ballad — pickupPos/dist (the 200A buzz)
    { "buzz",     0.66f, 0.82f, { 1, 1, 0 } },   //         cranked reed growl
    { 0,0,0, { 0 } } },
  { { "clav",     0.75f, 0.55f, { 1, 0, 0 } },   // CLAV:   funky bright bridge pickup through the wah
    { "mute",     0.50f, 0.55f, { 1, 1, 0 } },   //         the D6 damper
    { 0,0,0, { 0 } } },
};
static const int NMP[NMACHINE] = { 3, 2, 2 };
static int   cur_mp [NMACHINE] = { 0, 0, 0 };   // active preset per machine (-1 = hand-tweaked)
static float bright [NMACHINE];                 // live brightness (timbre macro) per machine — set by presets / the MACRO panel
static float mharm  [NMACHINE];                 // live harmonics macro per machine (init from M_INST; the MACRO panel tweaks it)
static bool  show_macros = false;               // the raw-macro tweak panel (HARM/TIMB/MORPH), behind the ⠿ button after BARK

static bool  autoplay = true;
static int   ap_h[3] = { -1, -1, -1 };
static int   ap_step = 0;
static int   aud_h = -1;

// ── per-contact pointer pool (glissando + drag knobs at once) ──
#define NPTR 10
#define NOID (-999)
enum { PTR_IDLE, PTR_KEY, PTR_KNOB };
typedef struct { int id, mode, key, pedal, knob; float grabv; int grabY; } Ptr;
static Ptr ptr[NPTR];

// ── geometry ──
#define MSEL_Y   17
#define MSEL_H   19
#define MSEL_W   100
#define MSEL_X(m) (8 + (m) * 102)
#define OPT_Y    40
#define OPT_H    58            // tall enough for icon + knobs + footswitch
#define PED_TOP  41
#define BARK_CX  252                 // BARK knob centre (moved left to make room for the MACRO button)
#define MACRO_BX 276                 // the MACRO panel toggle button (right of BARK)
#define MACRO_BW 38
#define MAC_CX(i) (74 + (i) * 78)    // the 3 raw-macro knob centres (HARM/TIMB/MORPH) when the panel is open
#define KEY_Y    102           // big keybed
#define KEY_H    92
#define KEY_W    36
#define KEY_GAP  1
#define KEY_X(b) (10 + (b) * (KEY_W + KEY_GAP))
#define BLACK_W  20
#define BLACK_H  54
#define BLACK_X(k) (KEY_X(BWHICH[k]) + KEY_W - BLACK_W / 2)
#define OCT_DN_X 232
#define OCT_UP_X 286
#define OCT_Y    4
#define OCT_W    24
#define OCT_H    14
// the per-machine PRESET column (left of the pedals): a small vertical stack of buttons
#define MP_X     6
#define MP_W     48
#define MP_GAP   (MP_X + MP_W + 6)          // pedals begin after the preset column
#define MP_BY(n, i) (PED_TOP + (i) * ((OPT_H - 2) / (n)))
#define MP_BH(n)    ((OPT_H - 2) / (n) - 1)

// pedal x/width: 2-knob pedals are wider. laid out left→right per machine.
static int ped_w(int m, int i) { return PED[m][i].nk >= 2 ? 66 : 48; }
static int ped_x(int m, int i) { int x = MP_GAP; for (int j = 0; j < i; j++) x += ped_w(m, j) + 4; return x; }
static int pknob_cx(int m, int i, int k) {            // knob centre within pedal i
    int x = ped_x(m, i), w = ped_w(m, i);
    return PED[m][i].nk >= 2 ? (x + (k ? w * 7 / 10 : w * 3 / 10)) : (x + w / 2);
}
#define PKNOB_CY (PED_TOP + 30)
#define FOOT_Y   (PED_TOP + 47)
#define KNOB_R   7

static float bark_drive(float bark) { return bark > 0.5f ? (bark - 0.5f) * 1.4f : 0.0f; }

// ════ the TUNED voicing (verbatim navkit-matched; driven by val[]/wahmode/phasermode) ═══════════
static void apply_slot(void) {
    int ty = (int)(val[SL_INST] * 2.999f); if (ty < 0) ty = 0; else if (ty > 2) ty = 2;
    instrument(I_EP, INSTR_EPIANO, 1, 0, 7, ty == 2 ? 140 : ty == 1 ? 280 : 450);
    instrument_harmonics(I_EP, val[SL_INST]);
    instrument_timbre(I_EP, val[SL_BRITE]);
    instrument_morph(I_EP, val[SL_BARK]);
    instrument_drive(I_EP, ty == 2 ? 0.20f : ty == 1 ? 0.17f : bark_drive(val[SL_BARK]));
}
static void apply_wah(void) {
    float amt = val[SL_WAH];
    int  ty   = (int)(val[SL_INST] * 2.999f); if (ty < 0) ty = 0; else if (ty > 2) ty = 2;
    bool clav = (ty == 2), on = (wahmode != 0);
    instrument_lfo(I_EP, 0, LFO_CUTOFF, 0.0f, 0.0f);
    instrument_env(I_EP, 0, ENV_CUTOFF, 0, 0, 0.0f);
    instrument_wah(I_EP, 0.0f, 0.0f, 0.0f);
    if (clav)     { instrument_filter(I_EP, FILTER_LOW, 1500, 6); instrument_env(I_EP, 0, ENV_CUTOFF, 2, 100, 1600.0f); }
    else if (on)  { instrument_filter(I_EP, FILTER_LOW, 700, 9);  instrument_env(I_EP, 0, ENV_CUTOFF, 2, 110, 1700.0f + amt * 700.0f); }
    else            instrument_filter(I_EP, FILTER_OFF, 4000, 0);
    if (on) {
        if (clav) instrument_wah_lfo(I_EP, 2.0f, 0.70f, 1.0f);
        else { instrument_lfo(I_EP, 0, LFO_CUTOFF, 2.5f + amt * 3.5f, 500.0f + amt * 900.0f);
               instrument_wah(I_EP, 0.4f + amt * 0.6f, 0.45f + amt * 0.4f, 0.75f + amt * 0.25f); }
    }
}
static void ep_keytrack(int h, int midi) { if (h < 0 || wahmode == 0) return; note_cutoff(h, 300 + (midi - 36) * 14); }
static void apply_trem(void)   { instrument_tremolo(I_EP, 4.5f + trem_spd * 3.0f, val[SL_TREM] * 0.9f, LFO_SHAPE_SINE); }
static void apply_phaser(void) { instrument_phaser(I_EP, phaser_rate, 0.5f + phaser_str * 0.5f, 0.35f + phaser_str * 0.5f, (machine == M_RHODES && phasermode) ? 0.5f : 0.0f, 6); }

// ════ the FRONT PANEL → the param layer + the per-machine effects ════════════════════════════════
static void apply_machine(void) {
    // VOICE MACROS — exactly as the original fed them (so the engine's bell/bark is unchanged):
    // harmonics = the instrument detent, timbre = the machine's brightness, morph = bark.
    val[SL_INST]  = mharm[machine];      // the live harmonics macro (= M_INST by default; the MACRO panel tweaks it)
    val[SL_BRITE] = bright[machine];
    val[SL_BARK]  = barkv[machine];
    val[SL_TREM]  = 0.0f; wahmode = 0; phasermode = 0;

    bool suitcase = false, dyno = false;
    if (machine == M_RHODES) {
        suitcase   = ped_on[0][0];
        dyno       = ped_on[0][1];
        phasermode = ped_on[0][2];
        phaser_rate = 0.4f + ped_k[0][2][0] * 1.6f;
        phaser_str  = ped_k[0][2][1];
    } else if (machine == M_WURLI) {
        if (ped_on[1][0]) { trem_spd = ped_k[1][0][0]; val[SL_TREM] = ped_k[1][0][1]; }
    } else { // CLAV
        wahmode     = ped_on[2][0];
        val[SL_WAH] = ped_k[2][0][0];
    }
    apply_slot(); apply_wah(); apply_trem(); apply_phaser();

    // RHODES VIBE — the Suitcase stereo vibrato: the real bus auto-pan (antiphase tremolo), so the
    // whole Rhodes incl. the ringing tine tails sways L↔R coherently (the per-voice LFO_PAN stand-in
    // this replaced couldn't — there each note had its own phase). SPD = rate, DEP = sweep width.
    instrument_autopan(I_EP, 3.0f + ped_k[0][0][0] * 4.0f, suitcase ? (0.35f + ped_k[0][0][1] * 0.55f) : 0.0f, LFO_SHAPE_SINE);
    // RHODES DYNO — lush stereo chorus (bypassed at mix 0). EQ presence ONLY when on, so a clean
    // machine is NEVER routed through an EQ (that was the character-killer — it dulled the bell).
    instrument_chorus(I_EP, 0.6f, 0.35f + ped_k[0][1][0] * 0.4f, dyno ? 0.55f : 0.0f);
    if (dyno) instrument_eq(I_EP, 0.0f, 0.0f, 4.0f);
    // WURLI BUZZ — extra reed drive
    if (machine == M_WURLI && ped_on[1][1]) instrument_drive(I_EP, 0.17f + ped_k[1][1][0] * 0.45f);
    // CLAV MUTE — the D6 damper: a lowpass the TONE knob opens/closes
    if (machine == M_CLAV && ped_on[2][1]) instrument_filter(I_EP, FILTER_LOW, 600 + (int)(ped_k[2][1][0] * 1400.0f), 5);
}

static void octave_step(int d) { keybed_octave_shift(d); }   // keybed releases held notes + clamps
static void audition(void) { int m = keybed_white_midi(3); if (aud_h >= 0) note_off(aud_h); aud_h = note_on(m, I_EP, 5); ep_keytrack(aud_h, m); }
static void set_machine(int m) { machine = m; apply_machine(); audition(); }
static void load_mpreset(int m, int i) {                // set a machine's voice macros + pedals from a preset
    bright[m] = MPRESET[m][i].bright;
    barkv[m]  = MPRESET[m][i].bark;
    for (int k = 0; k < 3; k++) ped_on[m][k] = MPRESET[m][i].on[k];
    cur_mp[m] = i;
}
static void apply_mpreset(int i) {                      // the current machine's named patch
    load_mpreset(machine, i);
    apply_machine(); audition();
}

void init(void) {
    instrument(I_EP, INSTR_EPIANO, 1, 0, 7, 450);
    keybed_config(I_EP, 4, NWHITE);                         // slot, base octave C4, 8 white keys
    keybed_layout(KEY_X(0), KEY_Y, NWHITE * (KEY_W + KEY_GAP), KEY_H);
    for (int i = 0; i < NPTR; i++) ptr[i].id = NOID;
    for (int m = 0; m < NMACHINE; m++) for (int i = 0; i < 3; i++) { ped_k[m][i][0] = PED[m][i].kdef[0]; ped_k[m][i][1] = PED[m][i].kdef[1]; }
    for (int m = 0; m < NMACHINE; m++) mharm[m] = M_INST[m];         // harmonics macro starts at the instrument detent
    for (int m = 0; m < NMACHINE; m++) load_mpreset(m, cur_mp[m]);   // seed each machine's voice + pedals from its default preset
    apply_machine();
    bpm(76);
}

void update(void) {
    keybed_update();                         // keys: touch + mouse + QWERTY (ASDF../WETYU) + MIDI + Z/X octave
    if (keyp('1')) set_machine(M_RHODES);
    if (keyp('2')) set_machine(M_WURLI);
    if (keyp('3')) set_machine(M_CLAV);
    if (keyp('M')) autoplay = !autoplay;

    // live bark/keytrack on every ringing keybed voice (any source)
    for (int m = 0; m < 128; m++) if (keybed_held(m)) {
        int h = keybed_handle(m);
        note_morph(h, val[SL_BARK]); note_drive(h, bark_drive(val[SL_BARK])); ep_keytrack(h, m);
    }

    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        Ptr *p = 0, *freeP = 0;
        for (int j = 0; j < NPTR; j++) { if (ptr[j].id == id) { p = &ptr[j]; break; } if (ptr[j].id == NOID && !freeP) freeP = &ptr[j]; }
        if (!p) {
            if (!freeP) continue;
            p = freeP; *p = (Ptr){ id, PTR_IDLE, -1, -1, -1, 0, ty };
            if (point_in_box(tx, ty, 150, 2, 72, 14)) { autoplay = !autoplay; continue; }
            if (point_in_box(tx, ty, OCT_DN_X, OCT_Y, OCT_W, OCT_H)) { octave_step(-1); continue; }
            if (point_in_box(tx, ty, OCT_UP_X, OCT_Y, OCT_W, OCT_H)) { octave_step(+1); continue; }
            for (int m = 0; m < NMACHINE; m++) if (point_in_box(tx, ty, MSEL_X(m), MSEL_Y, MSEL_W, MSEL_H)) { set_machine(m); break; }
            if (ty >= MSEL_Y && ty < MSEL_Y + MSEL_H) continue;
            if (ty >= OPT_Y && ty < OPT_Y + OPT_H) {
                if (point_in_box(tx, ty, MACRO_BX, PED_TOP, MACRO_BW, OPT_H - 2)) { show_macros = !show_macros; continue; }
                if (show_macros) {                          // raw-macro panel: HARM / TIMB / MORPH
                    for (int g = 0; g < 3; g++)
                        if (point_in_box(tx, ty, MAC_CX(g) - 13, PKNOB_CY - 13, 26, 26)) {
                            p->mode = PTR_KNOB; p->pedal = -2; p->knob = g;
                            p->grabv = (g == 0 ? mharm[machine] : g == 1 ? bright[machine] : barkv[machine]); p->grabY = ty; break;
                        }
                    continue;
                }
                // preset column (left): pick the machine's named patch
                int n = NMP[machine];
                for (int q = 0; q < n; q++) if (point_in_box(tx, ty, MP_X, MP_BY(n, q), MP_W, MP_BH(n))) { apply_mpreset(q); break; }
                if (tx < MP_GAP - 3) continue;
                // bark knob + pedals (footswitch + knobs)
                if (point_in_box(tx, ty, BARK_CX - 12, PKNOB_CY - 12, 24, 24)) { p->mode = PTR_KNOB; p->pedal = -1; p->grabv = barkv[machine]; p->grabY = ty; continue; }
                for (int i2 = 0; i2 < NPED[machine]; i2++) {
                    int x = ped_x(machine, i2), w = ped_w(machine, i2);
                    if (point_in_box(tx, ty, x + 3, FOOT_Y, w - 6, 10)) { ped_on[machine][i2] = !ped_on[machine][i2]; cur_mp[machine] = -1; apply_machine(); audition(); break; }
                    for (int k = 0; k < PED[machine][i2].nk; k++)
                        if (point_in_box(tx, ty, pknob_cx(machine, i2, k) - 11, PKNOB_CY - 11, 22, 22)) { p->mode = PTR_KNOB; p->pedal = i2; p->knob = k; p->grabv = ped_k[machine][i2][k]; p->grabY = ty; break; }
                    if (p->mode == PTR_KNOB) break;
                }
                continue;
            }
            // touches in the key area are handled by keybed_update(); this pool is knobs only
        } else if (p->mode == PTR_KNOB) {
            float nv = clamp(p->grabv + (p->grabY - ty) * 0.012f, 0.0f, 1.0f);
            if (p->pedal == -2) { if (p->knob == 0) mharm[machine] = nv; else if (p->knob == 1) bright[machine] = nv; else barkv[machine] = nv; cur_mp[machine] = -1; }
            else if (p->pedal < 0) barkv[machine] = nv;
            else ped_k[machine][p->pedal][p->knob] = nv;
            apply_machine();                                    // live — but DON'T strike a note while dialing
        }
    }
    for (int i = 0; i < touch_ended_count(); i++)
        for (int j = 0; j < NPTR; j++)
            if (ptr[j].id == touch_ended_id(i)) ptr[j].id = NOID;

    if (autoplay) {
        if (every(2)) {
            static const int ROOT[4] = { 50, 55, 48, 57 }, THIRD[4] = { 3, 4, 4, 3 };
            for (int i = 0; i < 3; i++) if (ap_h[i] >= 0) { note_off(ap_h[i]); ap_h[i] = -1; }
            int r = ROOT[ap_step % 4]; int notes[3] = { r, r + THIRD[ap_step % 4], r + 7 };
            for (int i = 0; i < 3; i++) { ap_h[i] = note_on(notes[i], I_EP, 4); note_morph(ap_h[i], val[SL_BARK]); ep_keytrack(ap_h[i], notes[i]); }
            ap_step++;
        }
    } else for (int i = 0; i < 3; i++) if (ap_h[i] >= 0) { note_off(ap_h[i]); ap_h[i] = -1; }

#ifdef DE_TRACE
    watch("machine", "%d", machine); watch("wah", "%d", wahmode); watch("phaser", "%d", phasermode); watch("bark", "%.2f", val[SL_BARK]);
#endif
}

// a panel knob (hand-rolled, generous hit box)
static void draw_knob(int cx, int cy, float v, const char *label, int accent, int lblcol) {
    circfill(cx, cy, KNOB_R, CLR_BROWNISH_BLACK);
    circ(cx, cy, KNOB_R, accent);
    float a = (-135.0f + v * 270.0f) * 0.0174533f;
    line(cx, cy, cx + (int)(sinf(a) * (KNOB_R - 2)), cy - (int)(cosf(a) * (KNOB_R - 2)), CLR_WHITE);
    font(FONT_TINY); print_centered(label, cx, cy + KNOB_R + 1, lblcol); font(FONT_NORMAL);
}

// CUSTOM icons for the machine-specific pedals (not borrowed from any real effect's icon)
static void ep_icon(int kind, int cx, int cy, int col, int bg) {
    if (kind == ICON_PAN) {                                  // stereo vibrato — TEMP rectangle + L↔R sway
        rect(cx - 11, cy - 6, 22, 12, col);
        line(cx - 8, cy, cx + 8, cy, col);
        pset(cx - 8, cy, CLR_WHITE); pset(cx + 8, cy, CLR_WHITE);
    } else if (kind == ICON_DYNO) {                          // bright sheen — a sparkle/star
        line(cx - 7, cy, cx + 7, cy, col); line(cx, cy - 7, cx, cy + 7, col);
        line(cx - 4, cy - 4, cx + 4, cy + 4, col); line(cx - 4, cy + 4, cx + 4, cy - 4, col);
        circfill(cx, cy, 1, CLR_WHITE);
    } else if (kind == ICON_BUZZ) {                          // reed growl — a clipped/jagged wave
        int px = cx - 12, py = cy;
        for (int s = 1; s <= 6; s++) { int xx = cx - 12 + s * 4, yy = cy + ((s & 1) ? -5 : 5); line(px, py, xx, yy, col); px = xx; py = yy; }
    } else {                                                 // ICON_MUTE — a felt damper on a tine
        line(cx - 11, cy + 3, cx + 11, cy + 3, col);
        rectfill(cx - 5, cy - 5, 10, 6, col);
        for (int xx = cx + 4; xx <= cx + 11; xx += 3) pset(xx, cy + 3, bg);   // dampened after the pad
    }
}

// one self-contained pedal: icon (top) + its knob(s) + a footswitch (bottom, lit = on)
static void draw_pedal(int m, int i) {
    const PedalDef *d = &PED[m][i];
    int x = ped_x(m, i), w = ped_w(m, i), cx = x + w / 2;
    bool on = ped_on[m][i], custom = (d->icon < 0);
    int body = on ? (custom ? M_BODY[m]   : fx_body(d->icon))   : CLR_DARKER_GREY;
    int acc  = on ? (custom ? M_ACCENT[m] : fx_accent(d->icon)) : CLR_DARK_GREY;
    rrectfill(x, PED_TOP, w, OPT_H - 2, 3, body);
    rrect(x, PED_TOP, w, OPT_H - 2, 3, acc);
    if (custom) ep_icon(d->icon, cx, PED_TOP + 10, acc, body);
    else        fx_icon(d->icon, cx, PED_TOP + 10, acc, body);
    for (int k = 0; k < d->nk; k++) draw_knob(pknob_cx(m, i, k), PKNOB_CY, ped_k[m][i][k], d->kl[k], acc, on ? CLR_LIGHT_PEACH : CLR_DARK_GREY);
    rrectfill(x + 3, FOOT_Y, w - 6, 10, 2, on ? acc : CLR_BROWNISH_BLACK);   // footswitch
    rrect(x + 3, FOOT_Y, w - 6, 10, 2, on ? CLR_WHITE : CLR_DARK_GREY);
    font(FONT_TINY); print_centered(d->name, cx, FOOT_Y + 3, on ? CLR_BLACK : CLR_MEDIUM_GREY); font(FONT_NORMAL);
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("EPIANO", 8, 5, M_LIT[machine]);
    print_right(autoplay ? "M auto: on" : "M auto: off", 220, 6, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    rectfill(OCT_DN_X, OCT_Y, OCT_W, OCT_H, CLR_DARK_BROWN); rect(OCT_DN_X, OCT_Y, OCT_W, OCT_H, CLR_BROWN);
    print("Z-", OCT_DN_X + 6, OCT_Y + 4, CLR_LIGHT_PEACH);
    print_scaled(str("%d", keybed_octave()), OCT_DN_X + 30, OCT_Y - 1, CLR_LIGHT_YELLOW, 2);
    rectfill(OCT_UP_X, OCT_Y, OCT_W, OCT_H, CLR_DARK_BROWN); rect(OCT_UP_X, OCT_Y, OCT_W, OCT_H, CLR_BROWN);
    print("X+", OCT_UP_X + 6, OCT_Y + 4, CLR_LIGHT_PEACH);

    // machine selector
    for (int m = 0; m < NMACHINE; m++) {
        bool on = (m == machine);
        rrectfill(MSEL_X(m), MSEL_Y, MSEL_W, MSEL_H, 4, on ? M_BODY[m] : CLR_DARKER_GREY);
        rrect(MSEL_X(m), MSEL_Y, MSEL_W, MSEL_H, 4, on ? M_LIT[m] : CLR_DARK_GREY);
        print_centered(MNAME[m], MSEL_X(m) + MSEL_W / 2, MSEL_Y + 6, on ? M_LIT[m] : CLR_MEDIUM_GREY);
        font(FONT_TINY); print(str("%d", m + 1), MSEL_X(m) + 4, MSEL_Y + 2, on ? M_LIT[m] : CLR_DARK_GREY); font(FONT_NORMAL);
    }

    // option panel — either the MACRO tweak panel or the normal presets+pedals+bark
    if (show_macros) {
        // raw engine macros for dialing/finalizing the voice — with numeric readouts to bake
        static const char *ML[3] = { "HARM", "TIMB", "MORPH" };
        float mv[3] = { mharm[machine], bright[machine], barkv[machine] };
        font(FONT_SMALL); print("raw engine macros - read the value to bake it", 6, OPT_Y + 1, CLR_DARK_GREY); font(FONT_NORMAL);
        for (int g = 0; g < 3; g++) {
            draw_knob(MAC_CX(g), PKNOB_CY - 1, mv[g], ML[g], M_ACCENT[machine], CLR_LIGHT_PEACH);
            font(FONT_SMALL); print_centered(str("%.2f", mv[g]), MAC_CX(g), PKNOB_CY + 16, CLR_WHITE); font(FONT_NORMAL);
        }
    } else {
        int nmp = NMP[machine];
        for (int q = 0; q < nmp; q++) {
            bool on = (cur_mp[machine] == q);
            int by = MP_BY(nmp, q), bh = MP_BH(nmp);
            rrectfill(MP_X, by, MP_W, bh, 2, on ? M_BODY[machine] : CLR_DARKER_GREY);
            rrect(MP_X, by, MP_W, bh, 2, on ? M_LIT[machine] : CLR_DARK_GREY);
            font(FONT_SMALL); print_centered(MPRESET[machine][q].name, MP_X + MP_W / 2, by + bh / 2 - 2, on ? M_LIT[machine] : CLR_MEDIUM_GREY); font(FONT_NORMAL);
        }
        for (int i = 0; i < NPED[machine]; i++) draw_pedal(machine, i);
        draw_knob(BARK_CX, PKNOB_CY, barkv[machine], "BARK", M_ACCENT[machine], CLR_MEDIUM_GREY);
    }
    // the MACRO panel toggle (always visible, right of BARK) — a little 3-knob icon
    rrectfill(MACRO_BX, PED_TOP, MACRO_BW, OPT_H - 2, 3, show_macros ? M_BODY[machine] : CLR_DARKER_GREY);
    rrect(MACRO_BX, PED_TOP, MACRO_BW, OPT_H - 2, 3, show_macros ? M_LIT[machine] : CLR_DARK_GREY);
    for (int d = 0; d < 3; d++) circ(MACRO_BX + 10 + d * 9, PED_TOP + 12, 3, show_macros ? M_LIT[machine] : CLR_MEDIUM_GREY);
    font(FONT_TINY); print_centered(show_macros ? "x" : "MACRO", MACRO_BX + MACRO_BW / 2, FOOT_Y + 3, show_macros ? CLR_WHITE : CLR_MEDIUM_GREY); font(FONT_NORMAL);

    // the manual — keybed.h owns layout/voices/glow; we draw it in epiano's machine colours
    int nw = keybed_white_count();
    for (int b = 0; b < nw; b++) {
        int x, y, w, h; keybed_white_rect(b, &x, &y, &w, &h);
        int midi = keybed_white_midi(b); bool down = keybed_held(midi);
        int col = down ? M_LIT[machine] : keybed_glow(midi) > 0.1f ? CLR_PEACH : CLR_LIGHT_PEACH;
        rectfill(x, y, w - 1, h, col);
        rect(x, y, w - 1, h, down ? CLR_WHITE : CLR_DARK_BROWN);
        print(str("%c", WKEY[b]), x + w / 2 - 2, y + h - 14, down ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREY);
    }
    for (int b = 0; b < nw; b++) {
        int x, y, w, h, midi; if (!keybed_black_rect(b, &x, &y, &w, &h, &midi)) continue;
        bool down = keybed_held(midi);
        int col = down ? CLR_ORANGE : keybed_glow(midi) > 0.1f ? CLR_DARK_ORANGE : CLR_BROWNISH_BLACK;
        rectfill(x, y, w, h, col);
        rect(x, y, w, h, down ? CLR_LIGHT_YELLOW : CLR_DARK_BROWN);
        if (BLBL[b]) { font(FONT_TINY); print(str("%c", BLBL[b]), x + w / 2 - 1, y + h - 9, down ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREY); font(FONT_NORMAL); }
    }
}
