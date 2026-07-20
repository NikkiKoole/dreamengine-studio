/* de:meta
{
  "slug": "dubjam",
  "collection": ["device-face", "responsive"],
  "title": "Tiny Dub Jam (mockup)",
  "status": "wip",
  "created": "2026-07-19",
  "kind": ["instrument", "generative"],
  "genre": null,
  "teaches": [],
  "resizable": true,
  "description": {
    "summary": "WIP DUB-TECHNO / DRONE rack built around the GRENADIER filterbank — a device-face sibling to Tiny Acid Jam, darker palette. NOW RESPONSIVE via face.h (Layer 3 of responsive-first-device-face.md): the layout is a 4-row FaceZone[] table (nav · knob band · SCREEN hero · PLAY surface) and — the reason dubjam was the hard case — its coordinate-coupled TOUCH input (XY pads, keybed, note-bar drag, drum grid) now hit-tests against the SAME zone rects the draw uses, computed once in a shared relayout() at the top of update(). Four machines (keys 1-4): GREN is WIRED (grenadier triple-resonant filterbank drone, big ALPHA/BETA XY sweep, gate-pulsed); DRM is WIRED (TR-808 with per-step p-locks); MST is WIRED (dub echo+reverb buses, DJ filter, DUB THROW pad); SUB is WIRED (deep INSTR_SINE sub, one note per s_on[] step).",
    "detail": "The face.h conversion (Layer-3 proving pass 2 — the perform-zone/input stress test). Where chipjam's input all lived in draw() via ui.h capture and converted trivially, dubjam hit-tests raw touch in update() at fixed 160x100 coords — which would desync from the visuals on any reflow. Fix: relayout() (face_resize -> face_area -> face_layout, stored in file statics + derived interaction rects) runs FIRST in update(), and both input and draw read those statics. That is the pattern for any input-coupled device face. Audio DSP is unchanged. SUB's screen pitch-view and its note-bars now share the full-width register; the DRM grid sits after a name gutter so it uses a local lay_lane (the escape hatch).",
    "controls": "Tap a cartridge to focus a machine; tap the transport (top-left) or SPACE to play/stop; keys 1-4 also switch face. GREN: drag the XY SWEEP pad + BASE/SPACE/Q/MORPH knobs; tap the ROOT keybed. SUB: TAP a note-bar to toggle, DRAG up/down to pitch. DRM: tap a grid cell / a pad. MST: drag DLY/FBK/VERB/FILT + HOLD the DUB THROW pad. Resize / rotate — face.h reflows it, never scales."
  },
  "todo": [
    "SEAM sub_update(): wire the deep SUB bass — a short-decay INSTR_SINE/SQUARE on slot 8, one note per s_on[] step following the drone root; the note-bars become the pattern.",
    "P-LOCK paint UI: right now the per-step TUNE/DEC offsets are seeded in drm_init(); add the acidcandy FLAG palette (arm a lane, paint offsets across the grid) so they're editable live.",
    "MST follow-ups: tempo-sync the delay to a live BPM (currently the fixed BPM define); expose the delay DIVISION as buttons; consider a per-voice reverb send on the kit (rim/hat wetter, kick dry) once the feel is judged.",
    "Make GREN knobs the acidcandy gear-drag (sideways=fine, double-tap=reset) once the feel is proven; expose RND + LAYOUT/GATE as real controls."
  ]
}
de:meta */
#include "studio.h"
#include "lay.h"
#include "ui.h"
#include "face.h"
#include "tr808.h"   // the shared, honest TR-808 voice bank (the DRM kit's SOUND)
#include <math.h>

// TINY DUB JAM — a dub-techno/drone device-face. GREN/DRM/MST/SUB are WIRED.
// Layout DECLARED through face.h (Layer 3); coordinate-coupled input hit-tests the
// shared zone rects (relayout() in update()), so touch stays glued to the visuals
// under any reflow — the thing that made dubjam the hard conversion.

#define BPM      120
#define NV       3
enum { SL_F1 = 5, SL_F2, SL_F3 };   // GREN filterbank voices (INSTR_USER0)
#define SL_SUB   8                   // sub bass
#define BASE_MIDI 33                 // A1 drone root
#define NKEYS  8
#define STEPS  16

// ── THE LAYOUT — one declarative table (visually top→bottom) ──────────────────
static FaceZone ZONES[] = {
    { FACE_BAND, EDGE_TOP,    0.12f, "nav"    },   // ① transport + machine cartridges
    { FACE_BAND, EDGE_TOP,    0.24f, "knobs"  },   // ② candy knob row
    { FACE_HERO, 0,           0.00f, "screen" },   // ③ the dark glass screen
    { FACE_BAND, EDGE_BOTTOM, 0.32f, "play"   },   // ④ the play surface (pads / keybed / bars)
};
#define NZ 4

// shared layout — computed ONCE per frame in relayout() (top of update), read by
// BOTH the input handlers and draw() so touch and pixels never disagree.
static Face g_face;
static Box  z_nav, z_knobs, z_screen, z_play;
static Box  r_transport, r_cart[4];        // nav interaction rects
static Box  r_gpad, r_gkb;                 // GREN XY sweep pad + root keybed
static Box  r_drmnames, r_drmgrid;         // DRM name gutter + grid area

typedef struct { const char *name; int col, lo; } Machine;
static Machine mac[4] = {
    { "GREN", CLR_INDIGO,     CLR_DARK_PURPLE },   // grenadier filterbank (hero) — WIRED
    { "SUB",  CLR_TRUE_BLUE,  CLR_DARK_BLUE   },   // sub bass — WIRED
    { "DRM",  CLR_LIGHT_GREY, CLR_DARKER_GREY },   // spacious kit — WIRED
    { "MST",  CLR_GREEN,      CLR_DARK_GREEN  },   // dub master — WIRED
};
static int face = 0;
static int playing = 1;

// ── GREN (grenadier) state ──
static int   hv[NV] = { -1, -1, -1 };            // held drone voices
static float ax = 0.5f, ay = 0.5f;               // ALPHA (x) / BETA (y) — the XY sweep
static float k_base = 0.50f, k_space = 0.55f, k_q = 0.70f, k_morph = 0.35f;
static float k_rnd = 0.25f;                      // CMOS drift amount (seam: expose as a knob)
static int   ra9 = 0, gate_on = 1, gate_div = 2; // RA-99 vs RA-9; gate pulses per beat
static float jbase = 1, jf[NV] = { 1, 1, 1 }, jq[NV] = { 1, 1, 1 };
static unsigned rng = 0x2468acef;
static int   last_pulse = -1;
static int   gren_root = 0;                                  // drone root offset from BASE_MIDI (set by the keybed)
static const int GSCALE[NKEYS] = { 0, 3, 5, 7, 10, 12, 15, 17 };   // minor-pentatonic root ladder

// EDITABLE patterns (SUB line + DRM kit) — start-states double as the default groove
static int s_on[16]  = { 1,0,0,0, 1,0,0,1, 1,0,0,0, 1,0,1,0 };
static int s_pit[16] = { 0,0,0,0, 0,0,0,5, 0,0,0,0, 3,0,7,0 };
static const char *dvn[4] = { "KICK", "RIM", "HAT", "CLAP" };
static int dgrid[4][16] = {
    { 1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0 },
    { 0,0,0,1, 0,0,1,0, 0,0,0,1, 0,1,0,0 },
    { 0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0 },
    { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0 },
};
// ── DRM (TR-808) state ──  face voices 0..3 → 808 roles
static const int drole[4] = { TR_BD, TR_RS, TR_CH, TR_CP };   // KICK / RIM / HAT / CLAP
static float ktune[TR_NV], kdecay[TR_NV], kcolor[TR_NV];      // per-voice knobs (0.5 = neutral)
enum { PL_TUNE, PL_DEC, PL_N };                               // p-lock params
static float doff[PL_N][4][16];                               // per-step OFFSET from the voice knob (0 = follow)
static int   last_drm_step = -1;
static int   last_sub_step = -1;
static int   g_step16 = 0;                                    // shared 16th clock (set in update)
static int   edit_bar = -1, edit_starty = 0, edit_dragging = 0;   // SUB note-bar tap-vs-drag tracking
static float k_stune = 0.5f, k_sdec = 0.5f, k_sdrive = 0.3f, k_sglide = 0.2f;   // SUB knobs (TUNE/DEC/DRIVE/GLIDE)
static int   subh = -1;                                           // held sub voice handle
static float k_dtone = 0.5f, k_ddec = 0.5f, k_dspace = 0.5f, k_ddist = 0.3f;   // DRM knobs (TONE/DEC/SPACE/DIST)

// ── MST (dub master) state ──
static float k_dly = 0.60f, k_fbk = 0.55f, k_verb = 0.55f, k_filt = 0.50f;   // delay div / feedback / reverb / DJ filter
static int   dub_held = 0;                                    // the DUB THROW pad is being touched → override echo
static float dub_x = 0.5f, dub_y = 0.5f;                      // throw pad: X = delay time, Y = feedback

static float frand(void) { rng = rng * 1664525u + 1013904223u; return (rng >> 8) / 16777216.0f; }
static void plabel(const char *s, int cx, int y, int col) { print(s, cx - text_width(s) / 2, y, col); }
static int tappb(Box b) { return tapp((int)b.x, (int)b.y, (int)b.w, (int)b.h); }
static int inbox(int x, int y, Box b) { return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h; }

// ── LAYOUT — compute the face + every interaction rect (shared by input + draw) ──
static void relayout(void) {
    face_resize();                                   // ① chunky canvas (route 2)
    Box area = face_area(1);                          // ② content = screen inset 1, ∩ safe-rect
    g_face = face_layout(area, ZONES, NZ, STEPS);     // ③ carve + enforce + build the register
    z_nav = g_face.box[0]; z_knobs = g_face.box[1]; z_screen = g_face.box[2]; z_play = g_face.box[3];
    Box nav = z_nav;
    r_transport = lay_split(nav, EDGE_LEFT, lay_clamp(nav.h + 2, 12, 20), &nav);
    for (int m = 0; m < 4; m++) r_cart[m] = lay_grid(nav, 4, 4, m, 1);
    Box play = z_play;                               // GREN splits it: keybed (bottom) + XY pad (top)
    r_gkb  = lay_split(play, EDGE_BOTTOM, play.h * 0.40f, &play);
    r_gpad = lay_inset(play, 1);
    Box si = lay_inset(z_screen, 3);                 // DRM: name gutter + grid
    r_drmnames = lay_split(si, EDGE_LEFT, lay_clamp(si.w * 0.13f, 12, 24), &si);
    r_drmgrid  = si;
}

// ── GREN AUDIO (unchanged) ───────────────────────────────────────────────────
static void reroll(void) {                       // CMOS drift: ±base, ±Q, per RND
    float a = k_rnd;
    jbase = powf(2.0f, (frand() * 2 - 1) * a);
    for (int i = 0; i < NV; i++) { jf[i] = powf(2.0f, (frand() * 2 - 1) * 0.15f * a); jq[i] = 1 + (frand() * 2 - 1) * 0.30f * a; }
}
static void bank_freqs(float f[NV]) {             // three filter freqs from BASE/SPACE + ALPHA/BETA
    float baseHz    = 80.0f * powf(3000.0f / 80.0f, k_base);
    float base_eff  = baseHz * powf(2.0f, (ax - 0.5f) * 4.0f) * jbase;   // ALPHA = ±2 oct
    float space     = 1.0f + k_space * 2.5f;
    float space_eff = 1.0f + (space - 1.0f) * (0.3f + ay * 0.7f);        // BETA opens the spread
    f[0] = base_eff * jf[0];
    f[1] = base_eff * space_eff * jf[1];
    f[2] = base_eff * space_eff * space_eff * jf[2];
    for (int i = 0; i < NV; i++) f[i] = f[i] < 20 ? 20 : f[i] > 18000 ? 18000 : f[i];
}
static void set_morph(void) {                     // trapezoid VCO (triangle → square) into INSTR_USER0
    float buf[64], gain = 1.0f / (1.0f - k_morph * 0.99f + 0.01f);
    for (int i = 0; i < 64; i++) {
        float ph = i / 64.0f, tri = (ph < 0.5f) ? (-1.0f + 4.0f * ph) : (3.0f - 4.0f * ph);
        float v = tri * gain; buf[i] = v < -1 ? -1 : v > 1 ? 1 : v;
    }
    wave_set(0, buf, 64);
}
static void gren_init(void) {
    set_morph();
    for (int i = 0; i < NV; i++) {
        instrument(SL_F1 + i, INSTR_USER0, 8, 0, 7, 240);
        instrument_filter(SL_F1 + i, (i == 0 && !ra9) ? FILTER_LOW : FILTER_BAND, 600, 8);
        instrument_drive(SL_F1 + i, 0.35f);
    }
    reroll();
    for (int i = 0; i < NV; i++) { hv[i] = note_on(BASE_MIDI, SL_F1 + i, 3); note_glide(hv[i], 60); }
}
static void gren_update(void) {
    // XY pad — drag ALPHA / BETA + the ROOT keybed (only on the GREN face). Hit-tests
    // the SHARED zone rects (r_gpad / r_gkb from relayout) → glued to the visuals.
    if (face == 0) {
        for (int i = 0; i < touch_count(); i++) {
            int tx = touch_x(i), ty = touch_y(i);
            if (inbox(tx, ty, r_gpad)) {
                ax = clamp((tx - r_gpad.x) / r_gpad.w, 0, 1);
                ay = clamp(1.0f - (ty - r_gpad.y) / r_gpad.h, 0, 1);
            }
        }
        for (int k = 0; k < NKEYS; k++)
            if (tappb(lay_grid(r_gkb, NKEYS, NKEYS, k, 1))) {          // tap a root → the held drone glides to it
                gren_root = GSCALE[k];
                for (int i = 0; i < NV; i++) if (hv[i] >= 0) note_pitch(hv[i], BASE_MIDI + gren_root);
            }
    }
    static float aMorph = -1;
    if (k_morph != aMorph) { set_morph(); aMorph = k_morph; }

    float env = playing ? 1.0f : 0.0f;
    if (playing && gate_on) {                     // gate: pulse the drone in time
        float steps = beat() * gate_div + beat_pos() * gate_div;
        int pulse = (int)steps;
        if (pulse != last_pulse) { last_pulse = pulse; reroll(); }   // fresh CMOS drift each pulse
        env = expf(-(steps - pulse) * 3.2f);
    }
    float f[NV]; bank_freqs(f);
    float Q = 1.0f + k_q * 13.0f;
    for (int i = 0; i < NV; i++) {                // ride the bank live (cutoff/res/vol are frame-safe)
        if (hv[i] < 0) continue;
        note_cutoff(hv[i], (int)f[i]);
        note_res(hv[i], Q * jq[i]);
        note_vol(hv[i], 7.0f * env);
    }
}

// ── DRM (TR-808) — WIRED, with per-step p-locks (audio unchanged) ──
static void drm_init(void) {
    for (int v = 0; v < TR_NV; v++) { ktune[v] = kdecay[v] = kcolor[v] = 0.5f; }
    tr808_build(TR808_BASE);
    doff[PL_TUNE][0][4] = -0.04f; doff[PL_TUNE][0][8] = -0.08f; doff[PL_TUNE][0][12] = -0.12f;  // kick descent
    doff[PL_DEC][2][14] = 0.34f;                                                                 // an open hat tail
    doff[PL_TUNE][1][6] = 0.10f;  doff[PL_TUNE][1][12] = -0.06f;                                 // rim wobble
}
static void drm_update(void) {
    if (!playing || g_step16 == last_drm_step) return;
    last_drm_step = g_step16;
    int s = g_step16;
    for (int fv = 0; fv < 4; fv++) {
        if (!dgrid[fv][s]) continue;
        int role = drole[fv];
        ktune[role]  = clamp(0.5f    + doff[PL_TUNE][fv][s], 0, 1);
        kdecay[role] = clamp(k_ddec  + doff[PL_DEC][fv][s],  0, 1);
        kcolor[role] = clamp(k_dtone, 0, 1);
        tr808_fire(TR808_BASE, role, (fv == 0) ? 1 : 0, 0, ktune, kdecay, kcolor);
    }
}
static void drm_fx(void) {
    static float aSp = -2, aDs = -2;
    if (k_dspace != aSp) {
        for (int i = 0; i < TR808_NSLOT; i++) instrument_reverb(TR808_BASE + i, k_dspace * 0.6f);
        instrument_reverb(TR808_BASE + 0, k_dspace * 0.1f);      // keep the KICK tight
        aSp = k_dspace;
    }
    if (k_ddist != aDs) {
        for (int i = 0; i < TR808_NSLOT; i++) instrument_drive(TR808_BASE + i, k_ddist * 0.7f);
        aDs = k_ddist;
    }
}

// ── MST (dub master) — WIRED (audio unchanged; input hit-tests z_play) ──
static void mst_init(void) {
    for (int i = 0; i < NV; i++) { instrument_echo(SL_F1 + i, 0.22f); instrument_reverb(SL_F1 + i, 0.45f); }  // drone: washy
    for (int i = 0; i < TR808_NSLOT; i++) instrument_echo(TR808_BASE + i, 0.30f);   // kit → dub delay
    instrument_echo(TR808_BASE + 0, 0.06f);                                          // ...but keep the KICK's delay dry
}
static void mst_apply_fx(void) {
    dub_held = 0;
    if (face == 3) for (int i = 0; i < touch_count(); i++) {
        int tx = touch_x(i), ty = touch_y(i);
        if (inbox(tx, ty, z_play)) {
            dub_held = 1;
            dub_x = clamp((tx - z_play.x) / z_play.w, 0, 1);
            dub_y = clamp(1.0f - (ty - z_play.y) / z_play.h, 0, 1);
        }
    }
    static int aEt = -1; static float aEf = -1, aVerb = -1;
    int   et = dub_held ? (int)(30 + dub_x * 470) : (int)(60000.0f / BPM * (0.25f + k_dly * 0.75f));
    float ef = dub_held ? (0.55f + dub_y * 0.37f) : (k_fbk * 0.85f);
    if (et != aEt || ef != aEf) { echo(et, ef, 0.30f); aEt = et; aEf = ef; }
    float vsize = 0.45f + k_verb * 0.5f;
    if (vsize != aVerb) { reverb(vsize, 0.6f); aVerb = vsize; }
    float res = 0.3f;
    if (k_filt > 0.54f)      filter(FILTER_HIGH, 20.0f * powf(6000.0f / 20.0f, (k_filt - 0.54f) / 0.46f), res);
    else if (k_filt < 0.46f) filter(FILTER_LOW, 18000.0f * powf(200.0f / 18000.0f, (0.46f - k_filt) / 0.46f), res);
    else                     filter(FILTER_OFF, 1000.0f, 0.0f);
}

// ── SUB — deep round sub bass, WIRED (audio unchanged) ──
static void sub_setup(int dec) {
    instrument(SL_SUB, INSTR_SINE, 2, dec, 0, 80);
    instrument_filter(SL_SUB, FILTER_LOW, 400, 2);
    instrument_drive(SL_SUB, k_sdrive * 0.8f);
}
static void sub_init(void) { sub_setup(220); }
static void sub_update(void) {
    static int aDec = -1; static float aDrv = -1;
    int dec = 60 + (int)(k_sdec * 440);
    if (dec != aDec) { sub_setup(dec); aDec = dec; aDrv = k_sdrive; }
    if (k_sdrive != aDrv) { instrument_drive(SL_SUB, k_sdrive * 0.8f); aDrv = k_sdrive; }

    if (!playing) { if (subh >= 0) { note_off(subh); subh = -1; } return; }
    if (g_step16 == last_sub_step) return;
    last_sub_step = g_step16;
    int s = g_step16;
    if (!s_on[s]) { if (subh >= 0) { note_off(subh); subh = -1; } return; }
    int midi = BASE_MIDI + s_pit[s] + (int)((k_stune - 0.5f) * 24.0f);   // TUNE = ±12 semis
    int gl = (int)(k_sglide * 120);                                      // GLIDE = 0..120 ms
    if (subh < 0 || gl < 6) {
        if (subh >= 0) note_off(subh);
        subh = note_on(midi, SL_SUB, 6); note_glide(subh, 0);
    } else {
        note_glide(subh, gl); note_pitch(subh, midi); note_vol(subh, 6);
    }
}

// ── widgets (positioned from zone cells) ─────────────────────────────────────
static void cknob(float *v, int cx, int cy, int r, const char *label) {   // draggable candy knob
    ui_reg(v, cx - r, cy - r, 2 * r + 1, 2 * r + 1, 0);
    UiCap *c = ui_cap_for(v);
    if (c) {
        if (!c->has_v0) { c->has_v0 = 1; c->v0 = *v; c->by = c->cy; }
        int py = c->released ? c->ry : c->cy;
        *v = clamp(c->v0 + (c->by - py) / 24.0f, 0, 1);
    }
    int hot = c != 0 || ui_hover(cx - r, cy - r, 2 * r + 1, 2 * r + 1);
    if (!c && hot && mouse_wheel() != 0) *v = clamp(*v + mouse_wheel() * 0.04f, 0, 1);
    float ang = 150 + *v * 240;
    circfill(cx, cy, r, CLR_INDIGO);
    ring(cx, cy, r - 2, r - 1, 165, 285, CLR_LIGHT_GREY);
    ring(cx, cy, r - 2, r - 1, -15, 105, CLR_DARKER_GREY);
    if (c) ring(cx, cy, r - 3, r, 150, ang, CLR_LIGHT_YELLOW);
    circ(cx, cy, r, c ? CLR_WHITE : hot ? CLR_LIGHT_PEACH : CLR_BLACK);
    line(cx + (int)dx(1, ang), cy + (int)dy(1, ang), cx + (int)dx(r - 1, ang), cy + (int)dy(r - 1, ang), c ? CLR_ORANGE : CLR_WHITE);
    font(FONT_TINY); plabel(label, cx, cy + r + 1, CLR_LIGHT_GREY);
}
static void knob_in(Box c, float *v, const char *lab) {                  // knob sized to its cell
    float rh = c.h * 0.34f, rw = c.w * 0.40f;
    int r = (int)lay_clamp(rh < rw ? rh : rw, 5, 10);
    int cx = (int)(c.x + c.w / 2), cy = (int)(c.y + r + 1);
    if (cy + r + 7 > (int)(c.y + c.h)) cy = (int)(c.y + c.h) - r - 7;
    cknob(v, cx, cy, r, lab);
}
static void cbtn(Box b, const char *s, int on) {
    rrectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, 2, on ? CLR_TRUE_BLUE : CLR_DARK_BLUE);
    rrect((int)b.x, (int)b.y, (int)b.w, (int)b.h, 2, CLR_BLACK);
    font(FONT_TINY); print(s, (int)(b.x + (b.w - text_width(s)) / 2), (int)(b.y + b.h / 2 - 2), CLR_LIGHT_GREY);
}
static void cartridge(Box b, int m, int foc) {
    int x = (int)b.x, y = (int)b.y, w = (int)b.w - 5, h = (int)b.h;
    rrectfill(x, y, w, h, 2, foc ? mac[m].col : mac[m].lo);
    if (foc) { blend(BLEND_AVG); line(x + 2, y + 1, x + w - 3, y + 1, CLR_WHITE); blend_reset(); }
    rrect(x, y, w, h, 2, foc ? CLR_WHITE : CLR_BLACK);
    font(FONT_TINY);
    print(mac[m].name, x + (w - text_width(mac[m].name)) / 2, y + (h - 5) / 2, foc ? CLR_BLACK : mac[m].col);
    int lx = x + w + 2, ly = y + h / 2;
    circfill(lx, ly, 2, foc ? CLR_LIME_GREEN : CLR_DARK_GREEN);
    circ(lx, ly, 2, CLR_BLACK);
}
static void glass(Box g) {
    rrectfill((int)g.x, (int)g.y, (int)g.w, (int)g.h, 3, CLR_DARKER_GREY);
    rrect((int)g.x, (int)g.y, (int)g.w, (int)g.h, 3, CLR_MEDIUM_GREY);
}
static int roll_y(Box rb, int pit) { return (int)(rb.y + rb.h - 3 - (pit / 14.0f) * (rb.h - 6)); }

static void draw_nav(void) {
    rrectfill((int)z_nav.x, (int)z_nav.y, (int)z_nav.w, (int)z_nav.h, 4, mac[face].lo);
    Box tp = lay_inset(r_transport, 1);
    rrectfill((int)tp.x, (int)tp.y, (int)tp.w, (int)tp.h, 2, playing ? CLR_TRUE_BLUE : CLR_DARK_BLUE);
    rrect((int)tp.x, (int)tp.y, (int)tp.w, (int)tp.h, 2, CLR_BLACK);
    int tcx = (int)(tp.x + tp.w / 2), tcy = (int)(tp.y + tp.h / 2);
    if (playing) { rectfill(tcx - 2, tcy - 2, 2, 4, CLR_WHITE); rectfill(tcx + 1, tcy - 2, 2, 4, CLR_WHITE); }
    else trifill(tcx - 2, tcy - 2, tcx - 2, tcy + 2, tcx + 2, tcy, CLR_WHITE);
    for (int m = 0; m < 4; m++) cartridge(r_cart[m], m, m == face);
}

// ── GREN face (wired) ──
static void face_gren(void) {
    static const char *kn[4] = { "BASE", "SPACE", "Q", "MORPH" };
    float *kp[4] = { &k_base, &k_space, &k_q, &k_morph };
    for (int i = 0; i < 4; i++) knob_in(lay_grid(z_knobs, 6, 6, i, 1), kp[i], kn[i]);
    cbtn(lay_inset(lay_grid(z_knobs, 6, 6, 4, 1), 2), ra9 ? "RA-9" : "RA-99", 0);
    cbtn(lay_inset(lay_grid(z_knobs, 6, 6, 5, 1), 2), "GATE", gate_on);

    glass(z_screen);
    Box si = lay_inset(z_screen, 3);
    for (int g = 1; g < 4; g++) line((int)(si.x + g * si.w / 4), (int)si.y, (int)(si.x + g * si.w / 4), (int)(si.y + si.h), CLR_DARK_GREY);
    float f[NV]; bank_freqs(f);
    int pc[3] = { CLR_ORANGE, CLR_LIME_GREEN, CLR_TRUE_BLUE };
    for (int i = 0; i < NV; i++) {
        float lg = (logf(f[i]) - logf(20)) / (logf(18000) - logf(20));
        int px = (int)(si.x + lg * si.w), w = 6, h = (int)si.h - 4;
        for (int d = -w; d <= w; d++) {
            int x = px + d; if (x < si.x || x >= si.x + si.w) continue;
            int hh = (int)(h * (1.0f - (d < 0 ? -d : d) / (float)(w + 1)));
            line(x, (int)(si.y + si.h - 1), x, (int)(si.y + si.h - 1 - hh), pc[i]);
        }
    }
    font(FONT_TINY); print("FILTERBANK", (int)si.x + 2, (int)si.y - 1, CLR_LIGHT_GREY);

    // the XY SWEEP pad (r_gpad)
    rrectfill((int)r_gpad.x, (int)r_gpad.y, (int)r_gpad.w, (int)r_gpad.h, 2, CLR_BLACK);
    for (int g = 1; g < 4; g++) {
        line((int)(r_gpad.x + g * r_gpad.w / 4), (int)r_gpad.y, (int)(r_gpad.x + g * r_gpad.w / 4), (int)(r_gpad.y + r_gpad.h), CLR_DARKER_GREY);
        line((int)r_gpad.x, (int)(r_gpad.y + g * r_gpad.h / 4), (int)(r_gpad.x + r_gpad.w), (int)(r_gpad.y + g * r_gpad.h / 4), CLR_DARKER_GREY);
    }
    rrect((int)r_gpad.x, (int)r_gpad.y, (int)r_gpad.w, (int)r_gpad.h, 2, CLR_MEDIUM_GREY);
    int dxp = (int)(r_gpad.x + ax * r_gpad.w), dyp = (int)(r_gpad.y + (1 - ay) * r_gpad.h);
    line((int)r_gpad.x, dyp, (int)(r_gpad.x + r_gpad.w), dyp, CLR_DARK_BLUE);
    line(dxp, (int)r_gpad.y, dxp, (int)(r_gpad.y + r_gpad.h), CLR_DARK_BLUE);
    circfill(dxp, dyp, 3, CLR_PINK); circ(dxp, dyp, 3, CLR_WHITE);
    font(FONT_TINY); print("SWEEP", (int)r_gpad.x + 2, (int)r_gpad.y + 1, CLR_DARK_GREY);
    // the ROOT keybed (r_gkb)
    for (int k = 0; k < NKEYS; k++) {
        Box kb = lay_inset(lay_grid(r_gkb, NKEYS, NKEYS, k, 1), 0);
        int cur = (GSCALE[k] == gren_root);
        rrectfill((int)kb.x, (int)kb.y, (int)kb.w - 1, (int)kb.h, 1, cur ? mac[0].col : CLR_DARK_BLUE);
        rrect((int)kb.x, (int)kb.y, (int)kb.w - 1, (int)kb.h, 1, CLR_BLACK);
        if (cur) rectfill((int)kb.x + 2, (int)kb.y + 2, (int)kb.w - 5, 2, CLR_WHITE);
    }
}

// ── SUB face (wired) ──
static void face_sub(int step, int col) {
    static const char *kn[4] = { "TUNE", "DEC", "DRIVE", "GLIDE" };
    float *kp[4] = { &k_stune, &k_sdec, &k_sdrive, &k_sglide };
    for (int i = 0; i < 4; i++) knob_in(lay_grid(z_knobs, 4, 4, i, 1), kp[i], kn[i]);

    glass(z_screen);
    Box si = lay_inset(z_screen, 3);
    font(FONT_TINY); print("SUB", (int)si.x, (int)si.y, CLR_LIGHT_GREY);
    for (int s = 0; s < STEPS; s++) {          // screen pitch-view — on the register (aligns with the bars)
        Box c = face_col(&g_face, si, s, 0);
        if (s == step) { blend(BLEND_AVG); rectfill((int)c.x, (int)si.y, (int)c.w, (int)si.h, CLR_MEDIUM_GREY); blend_reset(); }
        if (s_on[s]) rectfill((int)c.x + 1, roll_y(si, s_pit[s]), (int)c.w - 2, 3, CLR_TRUE_BLUE);
    }
    for (int s = 0; s < STEPS; s++) {          // note-bars — SAME register
        Box c = face_col(&g_face, z_play, s, 1);
        if (s == step) rrect((int)c.x - 1, (int)z_play.y, (int)c.w + 2, (int)z_play.h, 1, CLR_WHITE);
        if (!s_on[s]) { rectfill((int)c.x, (int)(z_play.y + z_play.h - 2), (int)c.w, 2, CLR_DARKER_GREY); continue; }
        float hf = 0.25f + 0.75f * s_pit[s] / 13.0f;
        int h = (int)(z_play.h * hf), ty = (int)(z_play.y + z_play.h - h);
        rrectfill((int)c.x, ty, (int)c.w, h, 1, col); rrect((int)c.x, ty, (int)c.w, h, 1, CLR_BLACK);
    }
}

// ── DRM face (wired) ──
static void face_drm(int step, int col) {
    static const char *kn[4] = { "TONE", "DEC", "SPACE", "DIST" };
    float *kp[4] = { &k_dtone, &k_ddec, &k_dspace, &k_ddist };
    for (int i = 0; i < 4; i++) knob_in(lay_grid(z_knobs, 4, 4, i, 1), kp[i], kn[i]);

    glass(z_screen);
    for (int v = 0; v < 4; v++) {
        Box rb = lay_grid(r_drmnames, 1, 4, v, 1);
        font(FONT_TINY); print(dvn[v], (int)rb.x, (int)(rb.y + rb.h / 2 - 2), CLR_LIGHT_GREY);
    }
    LayLane dgl = lay_lane(r_drmgrid, STEPS);   // grid sits AFTER the gutter → local lane (escape hatch)
    for (int s = 0; s < STEPS; s++) {
        Box cs = lay_lane_cell(dgl, r_drmgrid, s, 0);
        if (s == step) { blend(BLEND_AVG); rectfill((int)cs.x, (int)r_drmgrid.y, (int)cs.w, (int)r_drmgrid.h, CLR_MEDIUM_GREY); blend_reset(); }
        for (int v = 0; v < 4; v++) {
            Box cell = lay_grid(r_drmgrid, 1, 4, v, 1);
            if (dgrid[v][s]) rrectfill((int)cs.x, (int)cell.y, (int)cs.w - 1, (int)cell.h - 1, 1, CLR_LIGHT_GREY);
            else pset((int)cs.x + (int)cs.w / 2, (int)(cell.y + cell.h / 2), (s % 4 == 0) ? CLR_MEDIUM_GREY : CLR_DARK_GREY);
        }
    }
    for (int v = 0; v < 4; v++) {                // 4 pads (NOT per-step)
        Box p = lay_inset(lay_grid(z_play, 4, 4, v, 2), 1);
        int hit = dgrid[v][step];
        rrectfill((int)p.x, (int)p.y, (int)p.w, (int)p.h, 3, hit ? col : CLR_DARK_BLUE);
        rrect((int)p.x, (int)p.y, (int)p.w, (int)p.h, 3, CLR_BLACK);
        circfill((int)(p.x + p.w - 5), (int)(p.y + 4), 2, hit ? CLR_LIME_GREEN : CLR_DARK_GREEN);
        font(FONT_TINY); plabel(dvn[v], (int)(p.x + p.w / 2), (int)(p.y + p.h / 2 - 2), hit ? CLR_BLACK : CLR_LIGHT_GREY);
    }
}

// ── MST face (wired) ──
static void face_mst(int step) {
    static const char *kn[4] = { "DLY", "FBK", "VERB", "FILT" };
    float *kp[4] = { &k_dly, &k_fbk, &k_verb, &k_filt };
    for (int i = 0; i < 4; i++) knob_in(lay_grid(z_knobs, 4, 4, i, 1), kp[i], kn[i]);

    glass(z_screen);
    Box si = lay_inset(z_screen, 3);
    font(FONT_TINY); print("DUB ECHO", (int)si.x, (int)si.y, CLR_LIGHT_GREY);
    for (int e = 0; e < 6; e++) {
        int ex = (int)(si.x + 4 + e * si.w / 7), eh = (int)(si.h * (0.7f - e * 0.11f));
        int c = (e == 0) ? CLR_LIME_GREEN : (e < 3) ? CLR_MEDIUM_GREEN : CLR_DARK_GREEN;
        if (eh > 0) rectfill(ex, (int)(si.y + si.h - eh), 3, eh, c);
    }
    (void)step;
    // the DUB THROW pad (z_play) — X = delay time, Y = feedback; lit while held
    rrectfill((int)z_play.x, (int)z_play.y, (int)z_play.w, (int)z_play.h, 2, CLR_BLACK);
    rrect((int)z_play.x, (int)z_play.y, (int)z_play.w, (int)z_play.h, 2, dub_held ? CLR_LIME_GREEN : CLR_MEDIUM_GREY);
    int dxp = (int)(z_play.x + dub_x * z_play.w), dyp = (int)(z_play.y + (1 - dub_y) * z_play.h);
    circfill(dxp, dyp, 4, dub_held ? CLR_LIME_GREEN : CLR_GREEN); circ(dxp, dyp, 4, CLR_WHITE);
    font(FONT_TINY); print("DUB THROW  time>", (int)z_play.x + 2, (int)z_play.y + 1, CLR_DARK_GREY);
}

void init(void) {
    bpm(BPM);
    gren_init();
    drm_init();
    sub_init();
    mst_init();
}

// ── pattern editing (tap to edit; gated to the machine's own face) ──
static void sub_edit(void) {
    int ty = -1;
    for (int i = 0; i < touch_count(); i++) if (touch_y(i) >= z_play.y) { ty = touch_y(i); break; }
    int held = (ty >= 0);
    for (int b = 0; b < 16; b++)                                       // begin: grab the bar
        if (tappb(face_col(&g_face, z_play, b, 0))) { edit_bar = b; edit_starty = ty; edit_dragging = 0; }
    if (edit_bar >= 0 && held) {                                       // held: past the threshold → repitch drag
        if (!edit_dragging && (ty - edit_starty > 3 || edit_starty - ty > 3)) edit_dragging = 1;
        if (edit_dragging) { s_on[edit_bar] = 1; int p = (int)((z_play.y + z_play.h - ty) / z_play.h * 15); s_pit[edit_bar] = p < 0 ? 0 : p > 13 ? 13 : p; }
    }
    if (edit_bar >= 0 && !held) {                                      // release: a pure tap → toggle
        if (!edit_dragging) s_on[edit_bar] = !s_on[edit_bar];
        edit_bar = -1;
    }
}
static void drm_edit(void) {
    LayLane dgl = lay_lane(r_drmgrid, STEPS);
    for (int v = 0; v < 4; v++) {
        Box row = lay_grid(r_drmgrid, 1, 4, v, 1);
        for (int s = 0; s < 16; s++)
            if (tappb(lay_lane_cell(dgl, row, s, 0))) dgrid[v][s] = !dgrid[v][s];
        if (tappb(lay_grid(z_play, 4, 4, v, 1)))                                      // finger-drum a pad
            tr808_fire(TR808_BASE, drole[v], 1, 0, ktune, kdecay, kcolor);
    }
}

void update(void) {
    relayout();                                                                   // FIRST — input needs the zone rects
    for (int i = 0; i < 4; i++) if (keyp('1' + i)) face = i;
    for (int m = 0; m < 4; m++) if (tappb(r_cart[m])) face = m;                    // tap a cartridge to focus
    if (keyp(KEY_SPACE) || tappb(r_transport)) { playing = !playing; last_pulse = -1; last_drm_step = -1; last_sub_step = -1; }
    if (face == 1) sub_edit();
    if (face == 2) drm_edit();

    if (playing) { float sx = beat() * 4 + beat_pos() * 4; g_step16 = ((int)sx) % 16; }  // shared 16th clock

    gren_update();
    drm_update();
    drm_fx();
    sub_update();
    mst_apply_fx();
}

void draw(void) {
    ui_begin();
    int step = g_step16, mcol = mac[face].col;

    cls(CLR_BLACK);                                    // chassis bleeds to every edge
    Box full = box(0, 0, screen_w(), screen_h());
    rrectfill((int)full.x, (int)full.y, (int)full.w, (int)full.h, 6, CLR_DARK_BLUE);
    rrect((int)full.x, (int)full.y, (int)full.w, (int)full.h, 6, CLR_BLACK);

    draw_nav();
    if (face == 0)      face_gren();
    else if (face == 1) face_sub(step, mcol);
    else if (face == 2) face_drm(step, mcol);
    else                face_mst(step);

    ui_end();
}
