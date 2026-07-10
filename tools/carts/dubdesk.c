/* de:meta
{
  "slug": "dubdesk",
  "title": "Dubdesk",
  "status": "active",
  "created": "2026-06-22",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "euclidean-rhythm",
    "shift-register",
    "lowpass-gate",
    "self-oscillation",
    "drum-synthesis"
  ],
  "lineage": "Jam station fusing three shipped chassis — euclid rhythm, a turing shift-register melody driving an LPG voice, and a dubsiren — over one clock; novel: the siren's throw rides a MASTER echo so it dubs the whole mix, and one pattern brain drives drums and melody together.",
  "description": "A generative dub groovebox — the jam station that fuses three liveset chassis into one instrument over a single clock. RHYTHM: 3 euclid drum rings (KICK/SNARE/HAT) plus a 4th MELODY ring; dial each ring's density/rotation and it plays itself. The melody ring fires the LPG VOICE at a pitch read from a little shift register (a coherent, slowly-drifting motif; M re-rolls it). VOICE: the LPG (west-coast low-pass gate) — the melody ring auto-plays it AND you can finger it live on A S D F G H J K; DECAY/FOLD/COUPLE sculpt the plonk. SIREN: your live hands — the FIRE PAD (hold / SPACE / LATCH), Y = pitch, X = THROW, which rides a MASTER echo so a throw smears the WHOLE groove into the distance (the dub move); past the red it self-oscillates and howls. The PART tabs (RHYTHM / VOICE / SIREN) re-target one shared 3-knob row; in RHYTHM, 1-4 (or tap a ring) picks which ring the knobs edit. Z/X/C switch part. Per-part MUTE on Q/W/E (or re-tap a PART tab) drops the DRUMS / MELODY / SIREN — a muted melody still lets A-K play through, a muted siren still lets the pad throw the mix. When the MELODY ring is selected its 3rd knob becomes MORPH (lock the riff or let the shift register evolve); ↑/↓ set SWING. Everything stays visible and sounding at once. Built from euclid + lpg + dubsiren; spec in docs/design/dubdesk-spec.md."
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>

// DUBDESK — a generative dub groovebox (the jam station). Three shipped chassis fused into
// one instrument: euclid rhythm + an LPG melodic voice + a dub siren, sharing one clock.
// (teaches[]/lineage live in editor/public/carts/index.json, per the compendium convention.)
//
// • RHYTHM — 3 euclid drum rings (KICK/SNARE/HAT) + a 4th MELODY ring. Dial each ring's
//   density/rotation; it plays itself. The melody ring fires the LPG voice at a pitch read
//   from a little shift register (a coherent, slowly-drifting motif; M = re-roll).
// • VOICE — the LPG (west-coast low-pass gate): the melody ring auto-plays it, AND you can
//   finger it live on A S D F G H J K. DECAY/FOLD/COUPLE sculpt the plonk.
// • SIREN — your live hands: the FIRE PAD (hold/SPACE/LATCH). Y = pitch, X = THROW, which
//   rides a MASTER echo so a throw smears the WHOLE groove into the distance (the dub move).
//
//   PART tabs (RHYTHM/VOICE/SIREN) re-target the shared 3-knob row. In RHYTHM, 1-4 (or tap a
//   ring) picks which ring the knobs edit. Everything stays visible and sounding at once.
//   Spec: docs/design/dubdesk-spec.md.

enum { SL_KICK = 5, SL_SNARE, SL_HAT, SL_LPG, SL_SIR_A, SL_SIR_B };
#define NV 5                       // LPG voice pool

// ── pattern brain ──
static int   gstep = -1, last_16 = -1, tempo = 124;
static float kswing = 0.0f;        // swing: drag the odd 16ths late (0..0.6)
static unsigned mreg = 0xB4;       // melody shift register (8-bit)
static float kmorph = 0.10f;       // melody lock↔evolve (flip prob = kmorph*0.5)
static bool  mute[3] = { false, false, false };   // DRUMS / MELODY / SIREN

// ── rhythm: 4 rings (0..2 drums, 3 melody), params normalized 0..1 ──
static float hits_n[4] = { 0.25f, 0.125f, 0.50f, 0.375f };  // 4 / 2 / 8 / 6  of 16
static float rot_n[4]  = { 0.0f, 0.27f, 0.0f, 0.13f };
static const char *RNAME[4] = { "KICK", "SNARE", "HAT", "MELODY" };
static const int   RCOL [4] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_BLUE };
static const int   RAD  [4] = { 62, 48, 34, 20 };
#define CX 76
#define CY 88
static int HITS(int r) { int k = (int)(hits_n[r] * 16 + 0.5f); return k < 0 ? 0 : k > 16 ? 16 : k; }
static int ROT (int r) { return (int)(rot_n[r] * 15 + 0.5f); }

// ── LPG voice ──
static int   h_lpg[NV]; static float env_l[NV]; static bool atk_l[NV]; static int strk_l[NV];
static float kdecay = 0.42f, kfold = 0.55f, kcouple = 1.0f;
static const int PENTA[5] = { 0, 2, 4, 7, 9 };
static const int LKEY[8] = { 'A','S','D','F','G','H','J','K' };
static int penta_pitch(int d) { return 48 + 12 * (d / 5) + PENTA[d % 5]; }
static int melflash = -100;

// ── siren ──
static int   hA = -1, hB = -1;
static bool  latch = false;
static float px = 0.5f, py = 0.55f;
static float kwob = 0.38f, kecho = 0.45f, kverb = 0.5f;
static bool  sir_fire = false;
static float beacon = 0;

// ── shared knob row ──
static int   part = 0, selring = 0;     // part 0=RHYTHM 1=VOICE 2=SIREN
static float g0, g1, g2;
#define PAD_X 150
#define PAD_Y 16
#define PAD_W 164
#define PAD_H 140

// RHYTHM's 3rd knob is TEMPO for the drum rings, but MORPH for the MELODY ring (ring 3).
static void load_knobs(void) {
    if (part == 0)      { g0 = hits_n[selring]; g1 = rot_n[selring]; g2 = (selring == 3) ? kmorph : (tempo - 80) / 100.0f; }
    else if (part == 1) { g0 = kdecay; g1 = kfold; g2 = kcouple; }
    else                { g0 = kwob; g1 = kecho; g2 = kverb; }
}
static void store_knobs(void) {
    if (part == 0)      { hits_n[selring] = g0; rot_n[selring] = g1; if (selring == 3) kmorph = g2; else tempo = 80 + (int)(g2 * 100 + 0.5f); }
    else if (part == 1) { kdecay = g0; kfold = g1; kcouple = g2; }
    else                { kwob = g0; kecho = g1; kverb = g2; }
}

// sticky widget-ownership: once a finger is captured by a ui knob it stays "owned" until it
// LIFTS, so a knob drag that wanders over the pad never triggers it — robust even if ui.h's
// per-frame capture flickers. (ui_captured alone can drop for a frame mid-drag.)
static int own_ids[8], own_n = 0;
static bool is_owned(int id) { for (int i = 0; i < own_n; i++) if (own_ids[i] == id) return true; return false; }
static void own(int id)      { if (!is_owned(id) && own_n < 8) own_ids[own_n++] = id; }
static void unown(int id)    { for (int i = 0; i < own_n; i++) if (own_ids[i] == id) { own_ids[i] = own_ids[--own_n]; return; } }

static void fire_lpg(int pitch) {
    int v = 0; float lo = env_l[0];
    for (int i = 1; i < NV; i++) if (env_l[i] < lo) { lo = env_l[i]; v = i; }
    note_pitch(h_lpg[v], pitch);
    atk_l[v] = true; if (env_l[v] < 0.05f) env_l[v] = 0.05f; strk_l[v] = frame();
}

static void play_drum(int r) {
    if (r == 0)      { hit(72, INSTR_NOISE, 2, 12); hit(34, SL_KICK, 6, 220); }
    else if (r == 1) { hit(58, SL_SNARE, 5, 110); hit(53, INSTR_TRI, 3, 45); }
    else             { hit(92, SL_HAT, 3, 24); }
}

void init(void) {
    // drums
    instrument(SL_KICK, INSTR_SINE, 0, 250, 0, 60); instrument_env(SL_KICK, 1, ENV_PITCH, 0, 55, 30); instrument_drive(SL_KICK, 0.26f);
    instrument(SL_SNARE, INSTR_NOISE, 0, 130, 0, 50); instrument_filter(SL_SNARE, FILTER_BAND, 1400, 3);
    instrument(SL_HAT, INSTR_NOISE, 0, 28, 0, 16); instrument_filter(SL_HAT, FILTER_HIGH, 7000, 2);
    instrument_echo(SL_KICK, 0.05f); instrument_echo(SL_SNARE, 0.12f); instrument_echo(SL_HAT, 0.12f);
    instrument_reverb(SL_SNARE, 0.15f); instrument_reverb(SL_HAT, 0.12f);

    // LPG voice pool (sends BEFORE note_on — they're sampled at trigger)
    instrument(SL_LPG, INSTR_TRI, 4, 0, 9, 90); instrument_filter(SL_LPG, FILTER_LOW, 8000, 3);
    instrument_drive_mode(SL_LPG, DRIVE_FOLD); instrument_drive(SL_LPG, 0.0f);
    instrument_echo(SL_LPG, 0.3f); instrument_reverb(SL_LPG, 0.35f);
    for (int v = 0; v < NV; v++) { h_lpg[v] = note_on(60, SL_LPG, 0); note_glide(h_lpg[v], 0); env_l[v] = 0; atk_l[v] = false; strk_l[v] = -100; }

    // siren (sends BEFORE note_on)
    instrument(SL_SIR_A, INSTR_SQUARE, 1, 0, 9, 200); instrument_drive(SL_SIR_A, 0.12f);
    instrument(SL_SIR_B, INSTR_SAW, 1, 0, 9, 200);
    instrument_echo(SL_SIR_A, 0.5f); instrument_echo(SL_SIR_B, 0.45f);   // moderate send — a plain tap shouldn't smear
    instrument_reverb(SL_SIR_A, 0.4f); instrument_reverb(SL_SIR_B, 0.35f);
    hA = note_on(60, SL_SIR_A, 0); hB = note_on(60, SL_SIR_B, 0);
    note_glide(hA, 35); note_glide(hB, 35);

    echo(300, 0.3f, 0.5f);
    reverb(0.5f, 0.45f);
    load_knobs();
}

void update(void) {
    // part / ring selection
    if (keyp('Z')) { part = 0; load_knobs(); }
    if (keyp('X')) { part = 1; load_knobs(); }
    if (keyp('C')) { part = 2; load_knobs(); }
    if (part == 0) for (int i = 0; i < 4; i++) if (keyp('1' + i)) { selring = i; load_knobs(); }
    if (keyp('M')) mreg = (unsigned)(rnd(256));
    if (keyp('Q')) mute[0] = !mute[0];   // mute DRUMS
    if (keyp('W')) mute[1] = !mute[1];   // mute MELODY (A-K hand-play still sounds)
    if (keyp('E')) mute[2] = !mute[2];   // mute SIREN voice (the pad still throws the mix)
    if (btnp(0, BTN_UP))   kswing = clamp(kswing + 0.05f, 0, 0.6f);
    if (btnp(0, BTN_DOWN)) kswing = clamp(kswing - 0.05f, 0, 0.6f);

    // LPG hand-play (always live)
    for (int b = 0; b < 8; b++) if (keyp(LKEY[b])) fire_lpg(penta_pitch(b));

    // siren fire pad
    for (int i = 0; i < touch_count(); i++) if (ui_captured(touch_id(i))) own(touch_id(i));
    for (int i = 0; i < touch_ended_count(); i++) unown(touch_ended_id(i));
    bool touching = false;
    for (int i = 0; i < touch_count(); i++) {
        if (is_owned(touch_id(i))) continue;   // finger started on a knob → don't let it trigger the pad
        int tx = touch_x(i), ty = touch_y(i);
        if (tx >= PAD_X && tx < PAD_X + PAD_W && ty >= PAD_Y && ty < PAD_Y + PAD_H) {
            touching = true;
            px = clamp((tx - PAD_X) / (float)PAD_W, 0, 1);
            py = clamp(1.0f - (ty - PAD_Y) / (float)PAD_H, 0, 1);
        }
    }
    if (keyp('L')) latch = !latch;
    sir_fire = touching || latch || key(KEY_SPACE);

    bpm(tempo);

    // ── transport: one 16th clock; SWING drags the odd 16ths late ──
    float pos16 = beat() * 4 + beat_pos() * 4.0f;
    int n16 = (int)pos16;
    int sixteenth = (n16 % 2 == 1 && (pos16 - n16) < kswing) ? n16 - 1 : n16;
    if (sixteenth != last_16) {
        last_16 = sixteenth; gstep++;
        // melody register clocks every step; MORPH = flip probability (lock ↔ evolve)
        int fb = (mreg >> 7) & 1; if (rnd(1000) < (int)(kmorph * 0.5f * 1000)) fb ^= 1;
        mreg = ((mreg << 1) | fb) & 0xFF;
        if (!mute[0]) for (int r = 0; r < 3; r++)
            if (euclid(HITS(r), 16, (gstep + ROT(r)) % 16)) play_drum(r);
        if (!mute[1] && euclid(HITS(3), 16, (gstep + ROT(3)) % 16)) {
            int d = (int)(mreg / 255.0f * 10); if (d > 9) d = 9;
            fire_lpg(penta_pitch(d)); melflash = frame();
        }
    }

    // ── LPG voices: coupled vactrol envelope (vol + cutoff + fold) ──
    float T = 0.15f + kdecay * kdecay * 2.85f;
    float dc = expf(logf(0.0015f) * (1.0f / 60.0f) / T);
    for (int v = 0; v < NV; v++) {
        if (atk_l[v]) { env_l[v] += (1.0f / 60.0f) / 0.012f; if (env_l[v] >= 1) { env_l[v] = 1; atk_l[v] = false; } }
        else env_l[v] *= dc;
        if (env_l[v] < 0.0008f) { if (env_l[v] != 0) { env_l[v] = 0; note_vol(h_lpg[v], 0); } continue; }
        float e = env_l[v];
        note_vol(h_lpg[v], e * 5.5f);
        note_cutoff(h_lpg[v], (int)(300.0f + 7700.0f * ((1.0f - kcouple) + kcouple * powf(e, 1.6f))));
        note_drive(h_lpg[v], kfold * e);
    }

    // ── siren ──
    float pitch = 42.0f + py * 42.0f;
    bool sfire = sir_fire && !mute[2];          // siren VOICE (muteable); the pad still throws the mix
    static bool wasfire = false;
    if (sfire) { note_pitch(hA, pitch); note_pitch(hB, pitch + 0.09f); beacon += (0.5f + kwob * 19.5f) * 0.02f; }
    if (sfire != wasfire) { note_vol(hA, sfire ? 5.0f : 0.0f); note_vol(hB, sfire ? 3.6f : 0.0f); wasfire = sfire; }
    static float awob = -1;
    float rate = 0.5f + kwob * 19.5f;
    if (rate != awob) { note_lfo(hA, 0, LFO_PITCH, rate, 5.0f); note_lfo(hB, 0, LFO_PITCH, rate, 5.0f); awob = rate; }

    // ── master dub bus: the siren's X throws the WHOLE mix ──
    static int aet = -1; static float afb = -1, averb = -1;
    int etime = 60 + (int)(kecho * 540);
    float dubfb = sir_fire ? clamp(px * 1.1f, 0, 1.1f) : 0.12f;   // base: a light slap, not a 1s tail; the X-throw is the smear
    if (etime != aet || dubfb != afb) { echo(etime, dubfb, 0.5f); aet = etime; afb = dubfb; }
    if (kverb != averb) { reverb(0.25f + kverb * 0.7f, 0.45f); averb = kverb; }

#ifdef DE_TRACE
    watch("part", "%d", part);
    watch("step", "%d", gstep % 16);
    watch("fire", "%d", sir_fire);
    watch("dubfb", "%.2f", dubfb);
#endif
}

static const char *PART_LBL[3][3] = {
    { "HITS", "ROT", "TEMPO" }, { "DECAY", "FOLD", "COUPLE" }, { "WOBBLE", "ECHO", "VERB" }
};

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    float dubfb = sir_fire ? clamp(px * 1.1f, 0, 1.1f) : 0.12f;   // base: a light slap, not a 1s tail; the X-throw is the smear

    // top bar
    print("DUBDESK", 2, 3, CLR_LIGHT_YELLOW);
    print_right(str("%d BPM  sw%d", tempo, (int)(kswing * 100 + 0.5f)), 318, 3, CLR_LIGHT_GREY);
    for (int b = 0; b < 4; b++) circfill(64 + b * 7, 6, 2, (gstep >= 0 && (gstep / 4) % 4 == b) ? CLR_WHITE : CLR_DARK_GREY);
    // master-throw meter
    print("DUB", 120, 3, dubfb > 1.0f ? CLR_RED : CLR_DARK_GREY);
    for (int i = 0; i < 8; i++) rectfill(140 + i * 4, 3, 3, 6, (dubfb / 1.1f) * 8 > i ? (dubfb > 1.0f ? CLR_RED : CLR_BLUE) : CLR_DARKER_GREY);

    // ── ring cluster (drums + melody) ──
    int ph = gstep < 0 ? -1 : gstep % 16;
    for (int r = 0; r < 4; r++) {
        int hits = HITS(r), rot = ROT(r), rad = RAD[r];
        bool sel = (part == 0 && r == selring);
        if (sel) circ(CX, CY, rad, CLR_MEDIUM_GREY);
        for (int i = 0; i < 16; i++) {
            float a = -1.5708f + (float)i / 16 * 6.2832f;
            int x = CX + (int)(cosf(a) * rad), y = CY + (int)(sinf(a) * rad);
            bool on = euclid(hits, 16, (i + rot) % 16);
            bool head = (i == ph);
            if (on) { circfill(x, y, head ? 3 : 2, head ? CLR_WHITE : RCOL[r]); }
            else circfill(x, y, 1, head ? CLR_MEDIUM_GREY : CLR_DARKER_GREY);
        }
    }
    bool melon = (frame() - melflash) < 5;
    circfill(CX, CY, melon ? 4 : 2, melon ? CLR_BLUE : CLR_DARK_GREY);

    // ── siren fire pad ──
    bool firing = sir_fire;
    rectfill(PAD_X, PAD_Y, PAD_W, PAD_H, firing ? CLR_DARKER_BLUE : CLR_DARKER_GREY);
    int redx = PAD_X + (int)(0.909f * PAD_W);
    rectfill(redx, PAD_Y, PAD_X + PAD_W - redx, PAD_H, CLR_DARK_RED);
    for (int g = 1; g < 5; g++) {
        line(PAD_X + g * PAD_W / 5, PAD_Y, PAD_X + g * PAD_W / 5, PAD_Y + PAD_H, CLR_DARK_GREY);
        line(PAD_X, PAD_Y + g * PAD_H / 5, PAD_X + PAD_W, PAD_Y + g * PAD_H / 5, CLR_DARK_GREY);
    }
    rect(PAD_X, PAD_Y, PAD_W, PAD_H, firing ? CLR_WHITE : CLR_MEDIUM_GREY);
    int dx = PAD_X + (int)(px * PAD_W), dy = PAD_Y + (int)((1 - py) * PAD_H);
    int cc = (dubfb > 1.0f) ? CLR_RED : CLR_BLUE;
    if (firing) { line(PAD_X, dy, PAD_X + PAD_W, dy, cc); line(dx, PAD_Y, dx, PAD_Y + PAD_H, cc); circfill(dx, dy, 6, cc); circ(dx, dy, 6, CLR_WHITE); }
    else circ(dx, dy, 5, CLR_DARK_GREY);
    font(FONT_SMALL);
    print(firing ? "SIREN" : (latch ? "LATCH" : "hold = fire"), PAD_X + 3, PAD_Y + 2, firing ? CLR_WHITE : CLR_MEDIUM_GREY);
    print("pitch ^", PAD_X + 3, PAD_Y + PAD_H - 8, CLR_DARK_GREY);
    print_right("throw >", PAD_X + PAD_W - 2, PAD_Y + PAD_H - 8, dubfb > 1 ? CLR_RED : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // ── bottom: PART tabs + shared knob row ──
    const char *PT[3] = { "RHYTHM", "VOICE", "SIREN" };
    for (int p = 0; p < 3; p++) {
        int bx = 6 + p * 46, by = 162;
        if (tapp(bx, by, 44, 13)) { if (part == p) mute[p] = !mute[p]; else { part = p; load_knobs(); } }  // tap = select; re-tap = mute
        bool s = (p == part), m = mute[p];
        rectfill(bx, by, 44, 13, s ? CLR_INDIGO : CLR_DARK_GREY);
        rect(bx, by, 44, 13, CLR_MEDIUM_GREY);
        font(FONT_SMALL);
        print(PT[p], bx + 3, by + 4, m ? CLR_DARK_GREY : (s ? CLR_WHITE : CLR_LIGHT_GREY));
        if (m) print("M", bx + 37, by + 4, CLR_RED);
        font(FONT_NORMAL);
    }
    font(FONT_SMALL);
    if (part == 0) print(str("ring: %s  %d/16   (1-4 pick ring)", RNAME[selring], HITS(selring)), 6, 180, CLR_MEDIUM_GREY);
    else if (part == 1) print("LPG voice  -  A..K play live", 6, 180, CLR_MEDIUM_GREY);
    else print("dub siren  -  X = throw whole mix", 6, 180, CLR_MEDIUM_GREY);
    print("ZXC part  QWE mute  M roll  ^v swing", 6, 190, CLR_DARK_GREY);

    ui_begin();
    const char *l2 = (part == 0 && selring == 3) ? "MORPH" : PART_LBL[part][2];
    ui_knob(&g0, 200, 178, PART_LBL[part][0]);
    ui_knob(&g1, 250, 178, PART_LBL[part][1]);
    ui_knob(&g2, 300, 178, l2);
    store_knobs();
    font(FONT_NORMAL);
    ui_end();
}
