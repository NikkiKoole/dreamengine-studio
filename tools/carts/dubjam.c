/* de:meta
{
  "slug": "dubjam",
  "collection": ["device-face"],
  "title": "Tiny Dub Jam (mockup)",
  "status": "wip",
  "created": "2026-07-19",
  "kind": ["instrument", "generative"],
  "genre": null,
  "teaches": [],
  "description": {
    "summary": "WIP DUB-TECHNO / DRONE rack built around the GRENADIER filterbank — a device-face sibling to Tiny Acid Jam at 160x100 x4, darker palette. THREE-PART layout: candy KNOBS on top, a SCREEN in the middle, a PLAY surface below. Four machines (keys 1-4): GREN is WIRED — the grenadier triple-resonant filterbank drone (3 held INSTR_USER0 voices), swept live by a big ALPHA/BETA XY pad, gate-pulsed, with draggable BASE/SPACE/Q/MORPH knobs + the live filter strip. DRM is WIRED — the TR-808 (KICK/RIM/HAT/CLAP = TR_BD/RS/CH/CP) sequenced with per-step P-LOCKS (per-voice TUNE/DEC offsets swapped around each fire, acidcandy's method; a few are seeded). MST is WIRED — the dub master: the drone + kit routed into the shared echo + reverb buses (kick kept dry), a tempo-synced dub delay (DLY/FBK), reverb (VERB), a ride-safe DJ filter (FILT), and a DUB THROW pad that momentarily overrides the echo time/feedback. SUB is WIRED — a deep round INSTR_SINE sub (slot 8, lowpassed, kept dry) firing one note per s_on[] step on the drone root. All four voices are now live; the p-lock paint-UI is the remaining seam.",
    "detail": "Wired: GREN drone (gren_init/gren_update) — INSTR_USER0 trapezoid VCO (set_morph), 3 voices on one root through FILTER_LOW/BAND, note_cutoff/note_res ridden live from the XY pad (ALPHA=x sweeps all filters +-2oct, BETA=y opens the spacing), CMOS reroll() drift on each gate pulse. Transport: SPACE play/stop; a shared beat-clock gate pulses the drone. SEAMS: sub_update()/drm_update()/mst_apply_fx() are stubs — the sub bass, the spacious kit, and the dub delay/reverb master go there. Slots reserved: GREN 5-7, SUB 8, DRM 9-12.",
    "controls": "Tap a cartridge to focus a machine; tap the transport (top-left) or SPACE to play/stop; keys 1-4 also switch face. GREN: drag the XY SWEEP pad (ALPHA/BETA) + the BASE/SPACE/Q/MORPH knobs. SUB: TAP a note-bar to toggle it; DRAG a bar up/down to set its pitch. DRM: tap a grid cell to toggle a step; tap a pad to finger-drum. MST: drag DLY/FBK/VERB/FILT + HOLD the DUB THROW pad."
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
#include "ui.h"
#include "tr808.h"   // the shared, honest TR-808 voice bank (the DRM kit's SOUND)
#include <math.h>

// TINY DUB JAM — a dub-techno/drone device-face. GREN (grenadier filterbank) is
// WIRED; SUB/DRM/MST are open seams. 3-part skeleton: knobs · screen · play surface.

#define BPM      120
#define NV       3
enum { SL_F1 = 5, SL_F2, SL_F3 };   // GREN filterbank voices (INSTR_USER0)
#define SL_SUB   8                   // SEAM: sub bass
#define BASE_MIDI 33                 // A1 drone root
#define PAD_X 6
#define PAD_Y 69
#define PAD_W 148
#define PAD_H 28

typedef struct { const char *name; int col, lo; } Machine;
static Machine mac[4] = {
    { "GREN", CLR_INDIGO,     CLR_DARK_PURPLE },   // grenadier filterbank (hero) — WIRED
    { "SUB",  CLR_TRUE_BLUE,  CLR_DARK_BLUE   },   // sub bass — seam
    { "DRM",  CLR_LIGHT_GREY, CLR_DARKER_GREY },   // spacious kit — seam
    { "MST",  CLR_GREEN,      CLR_DARK_GREEN  },   // dub master — seam
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

// ── MST (dub master) state ──
static float k_dly = 0.60f, k_fbk = 0.55f, k_verb = 0.55f, k_filt = 0.50f;   // delay div / feedback / reverb / DJ filter
static int   dub_held = 0;                                    // the DUB THROW pad is being touched → override echo
static float dub_x = 0.5f, dub_y = 0.5f;                      // throw pad: X = delay time, Y = feedback

static float frand(void) { rng = rng * 1664525u + 1013904223u; return (rng >> 8) / 16777216.0f; }
static void plabel(const char *s, int cx, int y, int col) { print(s, cx - text_width(s) / 2, y, col); }

// ── GREN AUDIO ───────────────────────────────────────────────────────────────
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
    // XY pad — drag ALPHA / BETA (only on the GREN face; the pad isn't a ui widget)
    if (face == 0) for (int i = 0; i < touch_count(); i++) {
        int tx = touch_x(i), ty = touch_y(i);
        if (tx >= PAD_X && tx < PAD_X + PAD_W && ty >= PAD_Y && ty < PAD_Y + PAD_H) {
            ax = clamp((tx - PAD_X) / (float)PAD_W, 0, 1);
            ay = clamp(1.0f - (ty - PAD_Y) / (float)PAD_H, 0, 1);
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

// ── DRM (TR-808) — WIRED, with per-step p-locks ──
static void drm_init(void) {
    for (int v = 0; v < TR_NV; v++) { ktune[v] = kdecay[v] = kcolor[v] = 0.5f; }
    tr808_build(TR808_BASE);
    // seed a few tasteful p-locks (the MECHANISM is wired; the paint-UI is a seam):
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
        float sT = ktune[role], sD = kdecay[role];                 // p-lock: swap the voice knob around the fire
        ktune[role]  = clamp(0.5f + doff[PL_TUNE][fv][s], 0, 1);
        kdecay[role] = clamp(0.5f + doff[PL_DEC][fv][s],  0, 1);
        tr808_fire(TR808_BASE, role, (fv == 0) ? 1 : 0, 0, ktune, kdecay, kcolor);
        ktune[role] = sT; kdecay[role] = sD;
    }
}

// ── MST (dub master) — WIRED: echo + reverb buses, DJ filter, DUB throw pad ──
static void mst_init(void) {
    // route the drone + kit INTO the shared echo/reverb buses — this is what makes it DUB.
    for (int i = 0; i < NV; i++) { instrument_echo(SL_F1 + i, 0.22f); instrument_reverb(SL_F1 + i, 0.45f); }  // drone: washy
    for (int i = 0; i < TR808_NSLOT; i++) { instrument_echo(TR808_BASE + i, 0.30f); instrument_reverb(TR808_BASE + i, 0.28f); }  // kit: spacious
    instrument_echo(TR808_BASE + 0, 0.06f); instrument_reverb(TR808_BASE + 0, 0.05f);   // ...but keep the KICK mostly dry
}
static void mst_apply_fx(void) {
    // DUB THROW pad (MST face only): momentarily override the echo while held
    dub_held = 0;
    if (face == 3) for (int i = 0; i < touch_count(); i++) {
        int tx = touch_x(i), ty = touch_y(i);
        if (tx >= PAD_X && tx < PAD_X + PAD_W && ty >= PAD_Y && ty < PAD_Y + PAD_H) {
            dub_held = 1;
            dub_x = clamp((tx - PAD_X) / (float)PAD_W, 0, 1);
            dub_y = clamp(1.0f - (ty - PAD_Y) / (float)PAD_H, 0, 1);
        }
    }
    // the shared echo — set-and-hold (echo() is NOT ride-safe → re-apply only on change).
    static int aEt = -1; static float aEf = -1, aVerb = -1;
    int   et = dub_held ? (int)(30 + dub_x * 470) : (int)(60000.0f / BPM * (0.25f + k_dly * 0.75f));
    float ef = dub_held ? (0.55f + dub_y * 0.37f) : (k_fbk * 0.85f);
    if (et != aEt || ef != aEf) { echo(et, ef, 0.30f); aEt = et; aEf = ef; }   // dark dub tail (low tone)
    // the shared reverb — set-and-hold
    float vsize = 0.45f + k_verb * 0.5f;
    if (vsize != aVerb) { reverb(vsize, 0.6f); aVerb = vsize; }
    // the master DJ filter — ride-safe (every frame): bipolar around centre
    float res = 0.3f;
    if (k_filt > 0.54f)      filter(FILTER_HIGH, 20.0f * powf(6000.0f / 20.0f, (k_filt - 0.54f) / 0.46f), res);
    else if (k_filt < 0.46f) filter(FILTER_LOW, 18000.0f * powf(200.0f / 18000.0f, (0.46f - k_filt) / 0.46f), res);
    else                     filter(FILTER_OFF, 1000.0f, 0.0f);
}

// ── SUB — deep round sub bass, WIRED (kept DRY, like the kick) ──
static void sub_init(void) {
    instrument(SL_SUB, INSTR_SINE, 2, 220, 0, 80);            // round, plucky sub
    instrument_filter(SL_SUB, FILTER_LOW, 400, 2);            // roll off any edge → pure low end
}
static void sub_update(void) {
    if (!playing || g_step16 == last_sub_step) return;
    last_sub_step = g_step16;
    int s = g_step16;
    if (s_on[s]) note(BASE_MIDI + s_pit[s], SL_SUB, 6);       // one round sub per step, on the drone root
}

// ── widgets ──
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
static void dknob(int cx, int cy, int r, const char *label, float v) {    // static (mock) knob
    float ang = 150 + v * 240;
    circfill(cx, cy, r, CLR_INDIGO);
    ring(cx, cy, r - 2, r - 1, 165, 285, CLR_LIGHT_GREY);
    ring(cx, cy, r - 2, r - 1, -15, 105, CLR_DARKER_GREY);
    circ(cx, cy, r, CLR_BLACK);
    line(cx + (int)dx(1, ang), cy + (int)dy(1, ang), cx + (int)dx(r - 1, ang), cy + (int)dy(r - 1, ang), CLR_WHITE);
    font(FONT_TINY); plabel(label, cx, cy + r + 1, CLR_LIGHT_GREY);
}
static void cbtn(int x, int y, int w, int h, const char *s, int on) {
    rrectfill(x, y, w, h, 2, on ? CLR_TRUE_BLUE : CLR_DARK_BLUE);
    rrect(x, y, w, h, 2, CLR_BLACK);
    font(FONT_TINY); print(s, x + (w - text_width(s)) / 2, y + 1, CLR_LIGHT_GREY);
}
static void cartridge(int m, int foc) {
    int x = 19 + m * 27, y = 0;
    rrectfill(x, y, 25, 10, 2, foc ? mac[m].col : mac[m].lo);
    if (foc) { blend(BLEND_AVG); line(x + 2, y + 1, x + 20, y + 1, CLR_WHITE); blend_reset(); }
    rrect(x, y, 25, 10, 2, foc ? CLR_WHITE : CLR_BLACK);
    font(FONT_TINY);
    print(mac[m].name, x + (17 - text_width(mac[m].name)) / 2, y + 2, foc ? CLR_BLACK : mac[m].col);
    int lx = x + 21, ly = y + 5;
    circfill(lx, ly, 2, foc ? CLR_LIME_GREEN : CLR_DARK_GREEN);
    circ(lx, ly, 2, CLR_BLACK);
}
static void glass(void) {
    rrectfill(4, 37, 152, 30, 3, CLR_DARKER_GREY);
    rrect(4, 37, 152, 30, 3, CLR_MEDIUM_GREY);
}

// ── GREN face (wired) ──
static void face_gren(void) {
    static const char *kn[4] = { "BASE", "SPACE", "Q", "MORPH" };
    float *kp[4] = { &k_base, &k_space, &k_q, &k_morph };
    for (int i = 0; i < 4; i++) cknob(kp[i], 20 + i * 26, 23, 8, kn[i]);
    cbtn(126, 16, 30, 8, ra9 ? "RA-9" : "RA-99", 0);
    cbtn(126, 26, 30, 8, "GATE", gate_on);

    glass();
    int sx = 8, sy = 41, sw = 144, sh = 22;
    for (int g = 1; g < 4; g++) line(sx + g * sw / 4, sy, sx + g * sw / 4, sy + sh, CLR_DARK_GREY);
    float f[NV]; bank_freqs(f);
    // map each filter freq (20..18000 log) to strip x
    int pc[3] = { CLR_ORANGE, CLR_LIME_GREEN, CLR_TRUE_BLUE };
    for (int i = 0; i < NV; i++) {
        float lg = (logf(f[i]) - logf(20)) / (logf(18000) - logf(20));
        int px = sx + (int)(lg * sw), w = 6, h = 20;
        for (int d = -w; d <= w; d++) {
            int x = px + d; if (x < sx || x >= sx + sw) continue;
            int hh = (int)(h * (1.0f - (d < 0 ? -d : d) / (float)(w + 1)));
            line(x, sy + sh - 1, x, sy + sh - 1 - hh, pc[i]);
        }
    }
    rect(sx, sy, sw, sh, CLR_MEDIUM_GREY);
    font(FONT_TINY); print("FILTERBANK", sx + 2, sy - 1, CLR_LIGHT_GREY);

    // the XY SWEEP pad — the instrument
    rrectfill(PAD_X, PAD_Y, PAD_W, PAD_H, 2, CLR_BLACK);
    for (int g = 1; g < 4; g++) {
        line(PAD_X + g * PAD_W / 4, PAD_Y, PAD_X + g * PAD_W / 4, PAD_Y + PAD_H, CLR_DARKER_GREY);
        line(PAD_X, PAD_Y + g * PAD_H / 4, PAD_X + PAD_W, PAD_Y + g * PAD_H / 4, CLR_DARKER_GREY);
    }
    rrect(PAD_X, PAD_Y, PAD_W, PAD_H, 2, CLR_MEDIUM_GREY);
    int dxp = PAD_X + (int)(ax * PAD_W), dyp = PAD_Y + (int)((1 - ay) * PAD_H);
    line(PAD_X, dyp, PAD_X + PAD_W, dyp, CLR_DARK_BLUE);
    line(dxp, PAD_Y, dxp, PAD_Y + PAD_H, CLR_DARK_BLUE);
    circfill(dxp, dyp, 4, CLR_PINK); circ(dxp, dyp, 4, CLR_WHITE);
    font(FONT_TINY); print("SWEEP  alpha>", PAD_X + 2, PAD_Y + 1, CLR_DARK_GREY); print("^beta", PAD_X + PAD_W - 22, PAD_Y + 1, CLR_DARK_GREY);
}

// ── SUB / DRM / MST faces (mock visuals; audio is a seam) ──
static void face_sub(int step, int col) {
    static const char *kn[4] = { "TUNE", "DEC", "DRIVE", "GLIDE" };
    static const float kv[4] = { 0.45f, 0.60f, 0.40f, 0.30f };
    for (int i = 0; i < 4; i++) dknob(28 + i * 34, 23, 8, kn[i], kv[i]);
    glass();
    font(FONT_TINY); print("SUB", 8, 39, CLR_LIGHT_GREY);
    int base = 63;
    for (int s = 0; s < 16; s++) {
        int cx = 10 + s * 9;
        if (s == step) { blend(BLEND_AVG); rectfill(cx, 39, 8, 26, CLR_MEDIUM_GREY); blend_reset(); }
        if (s_on[s]) rectfill(cx + 1, base - s_pit[s], 5, 3, CLR_TRUE_BLUE);
    }
    int by = 95;
    for (int s = 0; s < 16; s++) {
        int bx = 8 + s * 9, bw = 8;
        if (s == step) rrect(bx - 1, 68, bw + 2, 29, 1, CLR_WHITE);
        if (!s_on[s]) { rectfill(bx, by - 1, bw, 2, CLR_DARKER_GREY); continue; }
        int h = 8 + s_pit[s] * 2, ty = by - h;
        rrectfill(bx, ty, bw, h, 1, col); rrect(bx, ty, bw, h, 1, CLR_BLACK);
    }
}
static void face_drm(int step, int col) {
    static const char *kn[4] = { "TONE", "DEC", "SPACE", "DIST" };
    static const float kv[4] = { 0.55f, 0.50f, 0.65f, 0.25f };
    for (int i = 0; i < 4; i++) dknob(28 + i * 34, 23, 8, kn[i], kv[i]);
    glass();
    int gx0 = 30, gy0 = 42, cw = 8, rh = 5;
    { int cx = gx0 + step * cw; blend(BLEND_AVG); rectfill(cx, gy0 - 1, cw, 4 * rh + 1, CLR_MEDIUM_GREY); blend_reset(); }
    for (int v = 0; v < 4; v++) {
        int ry = gy0 + v * rh;
        font(FONT_TINY); print(dvn[v], 6, ry, CLR_LIGHT_GREY);
        for (int s = 0; s < 16; s++) {
            int cx = gx0 + s * cw;
            if (dgrid[v][s]) rrectfill(cx, ry, cw - 1, rh - 2, 1, CLR_LIGHT_GREY);
            else pset(cx + (cw - 1) / 2, ry + 1, (s % 4 == 0) ? CLR_MEDIUM_GREY : CLR_DARK_GREY);
        }
    }
    for (int v = 0; v < 4; v++) {
        int px = 6 + v * 38, pw = 35, py = 70, ph = 25, hit = dgrid[v][step];
        rrectfill(px, py, pw, ph, 3, hit ? col : CLR_DARK_BLUE);
        rrect(px, py, pw, ph, 3, CLR_BLACK);
        circfill(px + pw - 5, py + 4, 2, hit ? CLR_LIME_GREEN : CLR_DARK_GREEN);
        font(FONT_TINY); plabel(dvn[v], px + pw / 2, py + ph / 2, hit ? CLR_BLACK : CLR_LIGHT_GREY);
    }
}
static void face_mst(int step) {
    static const char *kn[4] = { "DLY", "FBK", "VERB", "FILT" };
    float *kp[4] = { &k_dly, &k_fbk, &k_verb, &k_filt };
    for (int i = 0; i < 4; i++) cknob(kp[i], 20 + i * 26, 23, 8, kn[i]);
    glass();
    font(FONT_TINY); print("DUB ECHO", 8, 39, CLR_LIGHT_GREY);
    for (int e = 0; e < 6; e++) {
        int ex = 12 + e * 24, eh = 18 - e * 3;
        int c = (e == 0) ? CLR_LIME_GREEN : (e < 3) ? CLR_MEDIUM_GREEN : CLR_DARK_GREEN;
        if (eh > 0) rectfill(ex, 62 - eh, 3, eh, c);
    }
    // the DUB THROW pad — X = delay time, Y = feedback; lit while held
    rrectfill(PAD_X, PAD_Y, PAD_W, PAD_H, 2, CLR_BLACK);
    rrect(PAD_X, PAD_Y, PAD_W, PAD_H, 2, dub_held ? CLR_LIME_GREEN : CLR_MEDIUM_GREY);
    int dxp = PAD_X + (int)(dub_x * PAD_W), dyp = PAD_Y + (int)((1 - dub_y) * PAD_H);
    circfill(dxp, dyp, 4, dub_held ? CLR_LIME_GREEN : CLR_GREEN); circ(dxp, dyp, 4, CLR_WHITE);
    font(FONT_TINY); print("DUB THROW  time>", PAD_X + 2, PAD_Y + 1, CLR_DARK_GREY); print("^fbk", PAD_X + PAD_W - 18, PAD_Y + 1, CLR_DARK_GREY);
}

void init(void) {
    bpm(BPM);
    gren_init();
    drm_init();
    sub_init();
    mst_init();
}

// ── pattern editing (tap to edit; gated to the machine's own face) ──
// quick TAP toggles a bar; a vertical DRAG repitches it (and forces it on).
static void sub_edit(void) {
    int ty = -1;
    for (int i = 0; i < touch_count(); i++) if (touch_y(i) >= 68) { ty = touch_y(i); break; }
    int held = (ty >= 0);
    for (int b = 0; b < 16; b++)                                       // begin: grab the bar
        if (tapp(8 + b * 9, 68, 8, 29)) { edit_bar = b; edit_starty = ty; edit_dragging = 0; }
    if (edit_bar >= 0 && held) {                                       // held: past the threshold → it's a repitch drag
        if (!edit_dragging && (ty - edit_starty > 3 || edit_starty - ty > 3)) edit_dragging = 1;
        if (edit_dragging) { s_on[edit_bar] = 1; int p = (95 - ty) / 2; s_pit[edit_bar] = p < 0 ? 0 : p > 13 ? 13 : p; }
    }
    if (edit_bar >= 0 && !held) {                                      // release: a pure tap → toggle
        if (!edit_dragging) s_on[edit_bar] = !s_on[edit_bar];
        edit_bar = -1;
    }
}
static void drm_edit(void) {
    for (int v = 0; v < 4; v++) {
        for (int s = 0; s < 16; s++)
            if (tapp(30 + s * 8, 42 + v * 5, 7, 4)) dgrid[v][s] = !dgrid[v][s];    // toggle a step
        if (tapp(6 + v * 38, 70, 35, 25))                                          // finger-drum a pad
            tr808_fire(TR808_BASE, drole[v], 1, 0, ktune, kdecay, kcolor);
    }
}

void update(void) {
    for (int i = 0; i < 4; i++) if (keyp('1' + i)) face = i;
    for (int m = 0; m < 4; m++) if (tapp(19 + m * 27, 0, 25, 10)) face = m;      // tap a cartridge to focus
    if (keyp(KEY_SPACE) || tapp(5, 0, 14, 10)) { playing = !playing; last_pulse = -1; last_drm_step = -1; last_sub_step = -1; }
    if (face == 1) sub_edit();     // tap the note-bars
    if (face == 2) drm_edit();     // tap the grid / pads

    if (playing) { float sx = beat() * 4 + beat_pos() * 4; g_step16 = ((int)sx) % 16; }  // shared 16th clock

    gren_update();     // WIRED — the filterbank drone
    drm_update();      // WIRED — the TR-808 kit (p-locked)
    sub_update();      // seam
    mst_apply_fx();    // seam
}

void draw(void) {
    ui_begin();
    int step = g_step16;
    int col = mac[face].lo, mcol = mac[face].col;
    (void)col;

    cls(CLR_BLACK);
    rrectfill(1, 1, 158, 98, 6, CLR_DARK_BLUE);
    rrect(1, 1, 158, 98, 6, CLR_BLACK);

    rrectfill(3, 2, 154, 10, 5, mac[face].lo);
    rectfill(3, 7, 154, 5, mac[face].lo);
    rrectfill(5, 0, 14, 10, 2, playing ? CLR_TRUE_BLUE : CLR_DARK_BLUE);
    rrect(5, 0, 14, 10, 2, CLR_BLACK);
    if (playing) { rectfill(9, 3, 2, 4, CLR_WHITE); rectfill(13, 3, 2, 4, CLR_WHITE); }
    else trifill(9, 3, 9, 7, 14, 5, CLR_WHITE);
    for (int m = 0; m < 4; m++) cartridge(m, m == face);

    if (face == 0)      face_gren();
    else if (face == 1) face_sub(step, mcol);
    else if (face == 2) face_drm(step, mcol);
    else                face_mst(step);

    ui_end();
}
