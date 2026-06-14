#include "studio.h"
#include <math.h>
#define KEYBED_WHITE_KEYS "ASDFGHJKL"   // 9 whites = an octave + a bit
#define KEYBED_BLACK_KEYS "WE TYU OP"   // C# D# (gap) F# G# A# (gap) C#
#include "keybed.h"      // the shared polyphonic keybed: touch + mouse + QWERTY + MIDI

// ── COMBO ──────────────────────────────────────────────────────────────────────────────────────
// A vintage combo amp you plug a guitar straight into. This is the "cab story" (effects-bus
// Increment E): an amp/cab is NOT new DSP — it's a PRESET BUNDLE of effects we already ship.
// Each VOICING is one bundle of:  instrument_drive() + DRIVE_* mode  ·  instrument_eq()  ·  glue().
// The amp is the pinned OUTPUT STAGE (like leslie) — it lives outside fx_order, it just colours
// whatever the strings play.
//
//   CLEAN   — black-panel sparkle (soft clip, scooped-bright, barely compressed)
//   CHIME   — British chime (asym warmth, airy top)
//   CRUNCH  — Plexi-style rock (the effects-recipes "guitar-amp tone": ASYM 0.55 + mid-forward EQ)
//   HI-GAIN — hot-rodded, scooped-mid, hard clip + heavy power-amp sag
//   LO-FI   — broken/boxy wavefolder, narrow honk
//
// GAIN scales the voicing's drive · BASS/MID/TREBLE bend its EQ curve · MASTER = how hard you
// pick into it (keybed velocity) · SAG = power-amp compression (glue). The tube glow brightens
// (and reddens with gain), the VU needle tracks, the grille shudders on loud chords — all driven
// by a cart-side level proxy (the engine exposes no output meter), the way every synth cart fakes
// its VU. Phase 2: a RIG-recall knob (named legendary chains) + the pinned slot inside pedalboard.
//
// controls: white A S D F G H J K L · black W E . T Y U . O P · Z/X octave · 1-5 voicing ·
//           drag the VOICING knob & the tone knobs · M demo on/off. Multitouch + USB MIDI.

#define I_GTR 5

// ── the voicings: each is a complete amp/cab bundle ──
typedef struct { const char *name; int mode; float drive; float lo, mid, hi; float glue; float timbre; int col; } Voicing;
static const Voicing VC[5] = {
    { "CLEAN",   DRIVE_SOFT, 0.08f,  2,-2, 3, 0.15f, 0.75f, CLR_LIGHT_GREY   },
    { "CHIME",   DRIVE_ASYM, 0.30f,  0, 2, 4, 0.30f, 0.65f, CLR_LIGHT_YELLOW },
    { "CRUNCH",  DRIVE_ASYM, 0.55f,  3, 2,-2, 0.40f, 0.55f, CLR_ORANGE       },   // = the effects-recipes anchor
    { "HI-GAIN", DRIVE_HARD, 0.80f,  4,-4, 2, 0.60f, 0.45f, CLR_RED          },
    { "LO-FI",   DRIVE_FOLD, 0.70f, -3, 4,-6, 0.50f, 0.30f, CLR_MAUVE        },
};
static int voicing = 1;     // start on CHIME — pretty, and shows a little glow

// ── the 6 control knobs (0..1) ──
enum { K_GAIN, K_BASS, K_MID, K_TREBLE, K_MASTER, K_SAG, NK };
static float kv[NK]        = { 0.50f, 0.50f, 0.50f, 0.50f, 0.70f, 0.40f };
static const char *KL[NK]  = { "GAIN", "BASS", "MID", "TREBLE", "MASTER", "SAG" };
static const int   KX[NK]  = { 50, 90, 130, 170, 210, 250 };
#define KCY 44
#define KR  9

// ── the voicing selector (a big chickenhead, top-right) ──
#define SX 290
#define SY 38
#define SR 15

// ── visual feedback proxy (the engine has no output meter, so we model one) ──
static float level = 0.0f;     // 0..1 — drives the tube glow + VU needle
static float shud  = 0.0f;     // 0..1 — grille shudder on transients
static bool  held_last[128];   // to detect fresh key attacks

// ── demo strummer (so the amp breathes on its own / for the thumbnail) ──
static bool autoplay = true;
static int  demoH[3] = { -1, -1, -1 };
static int  demoStep = 0;
static bool struck   = false;  // forces a strum on the very first update (live thumbnail)

// ── per-contact pointer pool (drag several knobs at once) ──
#define NPTR 8
#define NOID (-999)
enum { PTR_IDLE, PTR_KNOB };
typedef struct { int id, mode, knob; float grabv; int grabY; } Ptr;   // knob -1 = the voicing selector
static Ptr ptr[NPTR];

// ════ the amp: drive + eq + glue, SET-AND-HOLD (reconfiguring every frame churns the bus → stutter) ══
static void apply_amp(void) {
    static int   aMode = -1, aVoice = -1, aVel = -1;
    static float aDrive = -1, aLo = -99, aMid = -99, aHi = -99, aGlue = -1;
    const Voicing *v = &VC[voicing];

    if (voicing != aVoice) {                                   // mode + timbre only change with the voicing
        instrument_drive_mode(I_GTR, v->mode);
        instrument_timbre(I_GTR, v->timbre);
        aMode = v->mode; aVoice = voicing;
    }
    float drive = v->drive * (0.30f + 1.4f * kv[K_GAIN]);      // GAIN scales the voicing's base drive
    if (drive != aDrive) { instrument_drive(I_GTR, drive); aDrive = drive; }

    float lo = v->lo + (kv[K_BASS]   - 0.5f) * 12.0f;          // tone knobs bend the voicing's EQ (±12dB)
    float mi = v->mid + (kv[K_MID]    - 0.5f) * 12.0f;
    float hi = v->hi + (kv[K_TREBLE] - 0.5f) * 12.0f;
    if (lo != aLo || mi != aMid || hi != aHi) { instrument_eq(I_GTR, lo, mi, hi); aLo = lo; aMid = mi; aHi = hi; }

    float g = v->glue * kv[K_SAG];                             // SAG = power-amp compression
    if (g != aGlue) { glue(0, g, 8, 120); aGlue = g; }

    int vel = (int)(kv[K_MASTER] * 7.0f + 0.5f);               // MASTER = how hard you pick in
    if (vel != aVel) { keybed_velocity(vel); aVel = vel; }
}

static void set_voicing(int v) { if (v < 0) v = 0; if (v > 4) v = 4; voicing = v; apply_amp(); }

static void demo_strum(void) {                                 // the demo: roll a chord, light the amp
    static const int CH[4][3] = { {40,47,52}, {45,52,57}, {43,50,55}, {38,45,50} };
    const int *c = CH[demoStep++ % 4];
    int vel = (int)(kv[K_MASTER] * 7.0f + 0.5f);
    for (int i = 0; i < 3; i++) { if (demoH[i] >= 0) note_off(demoH[i]); demoH[i] = note_on(c[i], I_GTR, vel); }
    level = 1.0f; shud = 1.0f;
}
static void demo_silence(void) { for (int i = 0; i < 3; i++) if (demoH[i] >= 0) { note_off(demoH[i]); demoH[i] = -1; } }

void init(void) {
    instrument(I_GTR, INSTR_GUITAR, 1, 0, 7, 1200);            // a string that rings (long release)
    instrument_harmonics(I_GTR, 0.55f);                        // body
    instrument_morph(I_GTR, 0.20f);                            // a little ring, not muted
    keybed_config(I_GTR, 3, 9);                                // slot, base octave C3, 9 whites
    keybed_layout(8, 190, 304, 48);
    colorkey(0);                                               // baked logo: palette 0 around it = transparent
    for (int i = 0; i < NPTR; i++) ptr[i].id = NOID;
    apply_amp();
    bpm(90);
}

void update(void) {
    keybed_update();                                           // touch + mouse + QWERTY + MIDI + Z/X octave
    if (keyp('1')) set_voicing(0);
    if (keyp('2')) set_voicing(1);
    if (keyp('3')) set_voicing(2);
    if (keyp('4')) set_voicing(3);
    if (keyp('5')) set_voicing(4);
    if (keyp('M')) { autoplay = !autoplay; if (!autoplay) demo_silence(); }

    // level proxy: bump on every fresh attack (any source), float up while keys are held, decay
    int held = 0;
    for (int m = 0; m < 128; m++) {
        bool h = keybed_held(m);
        if (h) held++;
        if (h && !held_last[m]) { level = 1.0f; shud = 1.0f; }   // a fresh pluck
        held_last[m] = h;
    }
    level *= 0.90f; shud *= 0.82f;
    float floorv = held ? 0.30f + 0.08f * (held > 4 ? 4 : held) : 0.0f;
    if (level < floorv) level = floorv;

    // drag knobs (own pointer pool; the keys belong to keybed_update)
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        Ptr *p = 0, *freeP = 0;
        for (int j = 0; j < NPTR; j++) { if (ptr[j].id == id) { p = &ptr[j]; break; } if (ptr[j].id == NOID && !freeP) freeP = &ptr[j]; }
        if (!p) {
            if (!freeP) continue;
            p = freeP; *p = (Ptr){ id, PTR_IDLE, -2, 0, ty };
            if (point_in_box(tx, ty, SX - SR, SY - SR, SR * 2, SR * 2)) {   // the voicing selector
                p->mode = PTR_KNOB; p->knob = -1; p->grabv = voicing / 4.0f; p->grabY = ty; continue;
            }
            for (int k = 0; k < NK; k++)
                if (point_in_box(tx, ty, KX[k] - KR - 2, KCY - KR - 2, (KR + 2) * 2, (KR + 2) * 2)) {
                    p->mode = PTR_KNOB; p->knob = k; p->grabv = kv[k]; p->grabY = ty; break;
                }
        } else if (p->mode == PTR_KNOB) {
            float nv = clamp(p->grabv + (p->grabY - ty) * 0.012f, 0.0f, 1.0f);
            if (p->knob == -1) set_voicing((int)(nv * 4.999f));
            else { kv[p->knob] = nv; apply_amp(); }
        }
    }
    for (int i = 0; i < touch_ended_count(); i++)
        for (int j = 0; j < NPTR; j++) if (ptr[j].id == touch_ended_id(i)) ptr[j].id = NOID;

    if (autoplay) { if (!struck) { demo_strum(); struck = true; } else if (every(2)) demo_strum(); }

#ifdef DE_TRACE
    watch("voicing", "%d", voicing); watch("gain", "%.2f", kv[K_GAIN]); watch("level", "%.2f", level);
#endif
}

// a chickenhead knob: dark body, accent ring, a pointer that juts past the rim
static void draw_knob(int cx, int cy, int r, float v, const char *label, int accent) {
    circfill(cx, cy, r, CLR_BROWNISH_BLACK);
    circ(cx, cy, r, accent);
    float a = (-135.0f + v * 270.0f) * 0.0174533f;
    int px = cx + (int)(sinf(a) * (r + 2)), py = cy - (int)(cosf(a) * (r + 2));
    line(cx, cy, px, py, CLR_WHITE);
    circfill(px, py, 1, accent);
    if (label) { font(FONT_TINY); print_centered(label, cx, cy + r + 2, CLR_LIGHT_GREY); font(FONT_NORMAL); }
}

void draw(void) {
    const Voicing *v = &VC[voicing];
    cls(CLR_BROWNISH_BLACK);

    // ── control panel (tolex face + brass trim) ──
    rectfill(0, 0, SCREEN_W, 74, CLR_DARK_BROWN);
    rectfill(4, 4, SCREEN_W - 8, 66, CLR_BROWNISH_BLACK);
    rect(4, 4, SCREEN_W - 8, 66, CLR_DARK_ORANGE);
    sspr(0, 0, 128, 32, 10, 7, 84, 18);                            // baked "COMBO" logo badge
    print_centered(v->name, 170, 8, v->col);                       // current voicing, centre-top

    for (int k = 0; k < NK; k++) draw_knob(KX[k], KCY, KR, kv[k], KL[k], v->col);

    // the voicing selector — big chickenhead + 5 detent ticks
    for (int d = 0; d < 5; d++) {
        float a = (-135.0f + (d / 4.0f) * 270.0f) * 0.0174533f;
        int tx = SX + (int)(sinf(a) * (SR + 4)), ty = SY - (int)(cosf(a) * (SR + 4));
        pset(tx, ty, d == voicing ? VC[d].col : CLR_DARK_GREY);
    }
    draw_knob(SX, SY, SR, voicing / 4.0f, 0, v->col);
    font(FONT_TINY); print_centered("VOICING", SX, SY + SR + 2, CLR_MEDIUM_GREY); font(FONT_NORMAL);

    // ── the speaker: grille cloth + tube glow + VU (the whole thing shudders on transients) ──
    int sh = (int)(sinf(frame() * 0.8f) * shud * 3.0f);
    int gx0 = 12, gy0 = 78, gw = SCREEN_W - 24, gh = 104;
    rectfill(gx0, gy0, gw, gh, CLR_DARK_BROWN);                     // cabinet around the cloth
    int cx0 = gx0 + 6, cy0 = gy0 + 6 + sh, cw = gw - 12, ch = gh - 12;
    rectfill(cx0, cy0, cw, ch, CLR_BLACK);                          // cloth backing

    // tube glow — a warm wash that shows BETWEEN the grille lines (drawn under them)
    bool hot = (kv[K_GAIN] > 0.6f || voicing >= 3);
    int gdim = hot ? CLR_DARK_RED : CLR_DARK_ORANGE, ghot = hot ? CLR_RED : CLR_ORANGE;
    int gcx = cx0 + cw / 2, gcy = cy0 + ch / 2, R = 16 + (int)(level * 46.0f);
    circfill(gcx, gcy, R, gdim);
    circfill(gcx, gcy, R / 2, ghot);
    if (level > 0.6f) circfill(gcx, gcy, R / 4, CLR_LIGHT_YELLOW);

    for (int x = cx0 + 2; x < cx0 + cw; x += 3)                     // grille lines over the glow
        line(x, cy0 + 1, x, cy0 + ch - 1, (x / 3) % 2 ? CLR_BROWNISH_BLACK : CLR_DARK_BROWN);
    rect(gx0, gy0, gw, gh, CLR_DARK_ORANGE);

    // VU meter — bottom-right corner of the cabinet face
    int vx = SCREEN_W - 42, vy = gy0 + gh - 26;
    circfill(vx, vy, 16, CLR_BROWNISH_BLACK); circ(vx, vy, 16, CLR_DARK_ORANGE);
    float va = (-1.0f + clamp(level, 0, 1) * 2.0f) * 0.9f;          // needle ±~50°
    line(vx, vy, vx + (int)(sinf(va) * 13), vy - (int)(cosf(va) * 13), level > 0.85f ? CLR_RED : CLR_LIGHT_YELLOW);
    font(FONT_TINY); print_centered("VU", vx, vy - 14, CLR_MEDIUM_GREY); font(FONT_NORMAL);

    // ── the manual (keybed owns layout/voices/glow; we colour it) ──
    int nw = keybed_white_count();
    for (int b = 0; b < nw; b++) {
        int x, y, w, h; keybed_white_rect(b, &x, &y, &w, &h);
        int midi = keybed_white_midi(b); bool down = keybed_held(midi);
        int col = down ? v->col : keybed_glow(midi) > 0.1f ? CLR_LIGHT_PEACH : CLR_LIGHT_GREY;
        rectfill(x, y, w - 1, h, col);
        rect(x, y, w - 1, h, down ? CLR_WHITE : CLR_DARK_GREY);
    }
    for (int b = 0; b < nw; b++) {
        int x, y, w, h, midi; if (!keybed_black_rect(b, &x, &y, &w, &h, &midi)) continue;
        bool down = keybed_held(midi);
        rectfill(x, y, w, h, down ? v->col : CLR_BROWNISH_BLACK);
        rect(x, y, w, h, down ? CLR_WHITE : CLR_DARK_GREY);
    }
}
