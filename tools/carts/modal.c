/* de:meta
{
  "slug": "modal",
  "title": "modal",
  "status": "active",
  "created": "2026-09-14",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "additive-synth",
    "analog-voice-modeling",
    "adsr-envelope"
  ],
  "lineage": "INSTR_MODAL showcase — the engine-reach §7.1 exciter→resonator bank (excited FILTERS, not decaying sines). The cart is the tuning rig for the two three-macro mappings in engine-reach-macro-mapping.md §2; both stay live as a tappable A/B, not a #define.",
  "description": {
    "summary": "Exciter into resonator — strike, blow, or bow a bank of tuned filters.",
    "detail": "INSTR_MODAL showcase: a filter bank you put energy into, so one engine covers struck bars, breath/chiff, and additive-ish pads. Eight pentatonic bars plus the three engine macros. Two mappings stay live — tap A/B (or B) to reinterpret the SAME three knobs (recommended: geometry / material / exciter vs Elements: geometry / brightness / damping). That fork is meant to be opposite personalities, not a subtle EQ. 0 loads the hearable-fork gesture (bowed ring vs short strike). Named presets (marimba, glass bowl, steel drum, tube, bowed glass, breath) still voice themselves when you re-pick them. Autoplay walks a scale while sweeping the macros.",
    "controls": "Tap or sweep the bars (multitouch) · A S D F G H J K strike · 1..6 presets · 0 hearable A/B fork · tap A/B (or B) to swap mapping (same knobs) · drag the sliders · M autoplay (scale + macros) · SPACE gliss"
  },
  "todo": [
    "ear-settle which mapping won (recommended morph=exciter vs Elements morph=damping) — both stay live via A/B until then",
    "bake a real thumbnail once someone can make-cart --run on a machine with a window",
    "playbook step 7: retrofit one radio station against its old fake mallet/noise once the macros settle"
  ]
}
de:meta */
// modal — INSTR_MODAL showcase: eight bars + the three engine macros + a live A/B mapping toggle.
//
// The engine-reach §7.1 bank: tuned FILTERS with an input, so strike / blow / bow is one
// mechanism (a decaying-sine design would split this back into three engines). Both
// three-macro routes from engine-reach-macro-mapping.md §2 stay hearable from this one
// build — tap A/B (or press B). Same three knobs, opposite mapping (no rematch).
// Key 0 is the hearable fork. A #define would need two builds and could not be judged
// on a phone.
//
//   recommended: harmonics = geometry,  timbre = material,   morph = exciter
//   Elements:    harmonics = geometry,  timbre = brightness, morph = damping
//                (exciter comes from MODE_MODAL_EXCITE, baked per preset)
//
// controls: tap/sweep the bars (multitouch) · A S D F G H J K
//           1..6 presets · 0 hearable A/B fork · tap A/B (or B) mapping (keeps knobs)
//           drag sliders · M autoplay (slow scale + macro sweep) · SPACE gliss

#include "studio.h"
#include "pointer.h"
#include <math.h>

#define I_BAR 5
#define NBAR  8
#define NPRESET 6

static const char STRKEY[NBAR] = { 'A','S','D','F','G','H','J','K' };

typedef struct {
    const char *name;
    float h;                 // geometry (always)
    float tA, mA;            // recommended: material / exciter
    float tB, mB;            // Elements:    brightness / damping
    float excite;            // MODE_MODAL_EXCITE (Elements route)
    float pos, direct, modes;
} Preset;

static const Preset PRESET[NPRESET] = {
    { "marimba",  0.62f, 0.22f, 0.06f,  0.35f, 0.28f,  0.06f,  0.32f, 0.12f, 0.25f },
    { "bowl",     0.92f, 0.12f, 0.10f,  0.20f, 0.18f,  0.10f,  0.55f, 0.08f, 0.75f },
    { "steel",    0.18f, 0.32f, 0.12f,  0.45f, 0.35f,  0.12f,  0.48f, 0.18f, 0.50f },
    { "tube",     0.72f, 0.38f, 0.48f,  0.40f, 0.42f,  0.48f,  0.22f, 0.05f, 0.40f },
    { "bowed",    0.88f, 0.18f, 0.92f,  0.25f, 0.22f,  0.92f,  0.60f, 0.10f, 0.65f },
    { "breath",   0.38f, 0.55f, 0.50f,  0.62f, 0.50f,  0.50f,  0.20f, 0.22f, 0.15f },
};

static int   midi_of[NBAR];
static float glow[NBAR];
static int   pend[NBAR];
static float knob[3] = { 0.62f, 0.22f, 0.06f };
static int   sel = 0;
static int   cur_preset = 0;
static int   elements = 0;          // 0 = recommended, 1 = Elements (MODE_MODAL_MAP)
static float excite_hold = 0.06f;   // MODE_MODAL_EXCITE — independent of morph (the A/B fork)
static float mode_pos = 0.32f, mode_direct = 0.12f, mode_modes = 0.25f;
static bool  autoplay = true;
static int   apos = 0;
static int   gliss_rx = -1;
static int   ab_rx = 0, ab_ry = 0, ab_rw = 88, ab_rh = 16;
static int   auto_rx = 0, auto_ry = 0, auto_rw = 108, auto_rh = 14;
static int   fork_rx = 8, fork_ry = 16, fork_rw = 50, fork_rh = 16;

enum { PTR_IDLE, PTR_DRAG, PTR_SWEEP };
typedef struct { int id, mode, k, prevX; } Ptr;
static Ptr ptr[PTR_MAX];

#define BAR_W    26
#define BAR_GAP  6
#define BAR_X(b) (14 + (b) * (BAR_W + BAR_GAP))
#define BAR_Y    36
#define BAR_H(b) (72 - (b) * 5)
#define KNOB_W   88
#define KNOB_Y   (SCREEN_H - 40)
#define KNOB_X(k) (14 + (k) * 102)

static const char *knob_name(int k) {
    static const char *A[3] = { "geometry", "material", "exciter" };
    static const char *B[3] = { "geometry", "brightness", "damping" };
    return elements ? B[k] : A[k];
}
static const char *knob_lo(int k) {
    static const char *A[3] = { "plate", "ring",  "strike" };
    static const char *B[3] = { "plate", "bright","long" };
    return elements ? B[k] : A[k];
}
static const char *knob_hi(int k) {
    static const char *A[3] = { "bell", "short", "bow" };
    static const char *B[3] = { "bell", "mute",  "short" };
    return elements ? B[k] : A[k];
}

static void apply_modes(void) {
    instrument_mode(I_BAR, MODE_MODAL_MAP,    elements ? 1.0f : 0.0f);
    instrument_mode(I_BAR, MODE_MODAL_EXCITE, excite_hold);
    instrument_mode(I_BAR, MODE_MODAL_POS,    mode_pos);
    instrument_mode(I_BAR, MODE_MODAL_DIRECT, mode_direct);
    instrument_mode(I_BAR, MODE_MODAL_MODES,  mode_modes);
}

static void apply_knobs(void) {
    instrument_harmonics(I_BAR, knob[0]);
    instrument_timbre(I_BAR, knob[1]);
    instrument_morph(I_BAR, knob[2]);
    apply_modes();
}

static float damp_of(void)   { return elements ? knob[2] : knob[1]; }
static float excite_of(void) { return elements ? excite_hold : knob[2]; }

static int gate_ms(void) {
    float d = damp_of(), ex = excite_of();
    int ring = 500 + (int)((1.0f - d) * (1.0f - d) * 4500.0f);
    if (ex > 0.35f) ring += 900;                 // blow/bow need the gate held open
    return ring;
}

static void strike(int b, int vol) {
    hit(midi_of[b], I_BAR, vol, gate_ms());
    glow[b] = 1.0f;
}

static void gliss(void) {
    for (int b = 0; b < NBAR; b++) {
        schedule_hit(b * 55, midi_of[b], I_BAR, 5, gate_ms());
        pend[b] = 1 + (b * 55 * 60) / 1000;
    }
}

static void set_preset(int p) {
    cur_preset = p;
    knob[0] = PRESET[p].h;
    if (elements) { knob[1] = PRESET[p].tB; knob[2] = PRESET[p].mB; }
    else          { knob[1] = PRESET[p].tA; knob[2] = PRESET[p].mA; }
    excite_hold = PRESET[p].excite;
    mode_pos = PRESET[p].pos;
    mode_direct = PRESET[p].direct;
    mode_modes = PRESET[p].modes;
    apply_knobs();
    strike(4, 6);
}

// Hearable A/B fork: SAME three knobs, opposite personalities.
// Recommended (morph=exciter): bright ringing bowed bell.
// Elements (morph=damping, excite_hold=strike): short bright ping.
// Paper maps on a bowl strike were too close — this gesture is the ear test.
static void load_fork(void) {
    cur_preset = -2;
    knob[0] = 0.88f;
    knob[1] = 0.12f;
    knob[2] = 0.95f;
    excite_hold = 0.04f;
    mode_pos = 0.58f;
    mode_direct = 0.08f;
    mode_modes = 0.70f;
    apply_knobs();
    // No auto-strike: scripts load the fork then play the same notes under
    // each mapping. A ding here would leave a recommended bow ringing under map-b.
}

static void toggle_map(void) {
    // Same gesture, opposite mapping — do NOT rematch preset knobs.
    // Re-pick 1..6 if you want that route's named voicing.
    elements = !elements;
    apply_knobs();
    strike(4, 5);
}

void init(void) {
    instrument(I_BAR, INSTR_MODAL, 1, 0, 7, 1600);
    apply_knobs();
    PTR_CLEAR(ptr);
    for (int b = 0; b < NBAR; b++) midi_of[b] = degree(SCALE_PENTA, 4, b + 1);
    bpm(88);
    glow[2] = 0.6f; glow[5] = 0.9f;
    // autoplay stays at its file-scope default (on). spec() turns it off
    // before the first step so a walk cannot rewrite the knobs under test.
}

void update(void) {
    for (int b = 0; b < NBAR; b++) {
        if (keyp(STRKEY[b])) strike(b, 6);
        if (pend[b] > 0 && --pend[b] == 0) glow[b] = 1.0f;
    }
    for (int p = 0; p < NPRESET; p++)
        if (keyp('1' + p)) set_preset(p);

    if (keyp(KEY_LEFT))  sel = (sel + 2) % 3;
    if (keyp(KEY_RIGHT)) sel = (sel + 1) % 3;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        knob[sel] = clamp(knob[sel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        cur_preset = -1;
        apply_knobs();
        if (frame() % 12 == 0) strike(4, 5);
    }

    if (keyp('B') || (ab_rw > 0 && tapp(ab_rx, ab_ry, ab_rw, ab_rh))) toggle_map();
    if (keyp('0') || (fork_rw > 0 && tapp(fork_rx, fork_ry, fork_rw, fork_rh))) load_fork();
    if (keyp('M') || (auto_rw > 0 && tapp(auto_rx, auto_ry, auto_rw, auto_rh))) autoplay = !autoplay;
    if (keyp(KEY_SPACE) || (gliss_rx >= 0 && tapp(gliss_rx - 4, SCREEN_H - 16, 72, 16))) gliss();

    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;
        if (fresh) {
            *p = (Ptr){ id, PTR_IDLE, -1, tx };
            if (ty >= KNOB_Y - 30 && ty < KNOB_Y - 14) {
                for (int q = 0; q < NPRESET; q++)
                    if (tx >= 10 + q * 52 && tx < 10 + q * 52 + 50) set_preset(q);
                continue;
            }
            for (int k = 0; k < 3; k++)
                if (point_in_box(tx, ty, KNOB_X(k) - 2, KNOB_Y - 8, KNOB_W + 4, 20)) {
                    p->mode = PTR_DRAG; p->k = sel = k;
                }
            if (p->mode == PTR_IDLE && ty >= BAR_Y - 4 && ty < KNOB_Y - 32) {
                for (int b = 0; b < NBAR; b++)
                    if (point_in_box(tx, ty, BAR_X(b), BAR_Y, BAR_W, BAR_H(b)))
                        strike(b, 6);
                p->mode = PTR_SWEEP; p->prevX = tx;
            }
        } else if (p->mode == PTR_DRAG) {
            knob[p->k] = clamp((float)(tx - KNOB_X(p->k)) / (float)KNOB_W, 0.0f, 1.0f);
            cur_preset = -1;
            apply_knobs();
            if (frame() % 12 == 0) strike(4, 5);
        } else if (p->mode == PTR_SWEEP) {
            for (int b = 0; b < NBAR; b++) {
                int cx = BAR_X(b) + BAR_W / 2;
                if ((p->prevX < cx && tx >= cx) || (p->prevX > cx && tx <= cx)) strike(b, 5);
            }
            p->prevX = tx;
        }
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (p) p->id = PTR_NONE;
    }

    if (autoplay && (every(1) || (apos == 0 && frame() == 1))) {
        // Slow pentatonic walk + a stepped macro sweep so mapping and pitch
        // move together (not a static note). excite_hold stays strike so the
        // same walk on Elements is short-damped hits, not a rematch.
        static const int walk[8] = { 0, 2, 4, 7, 5, 4, 2, 0 };
        int i = apos & 7;
        float u = (float)i / 7.0f;
        knob[0] = 0.16f + 0.74f * u;            // plate → bell
        knob[1] = 0.14f + 0.08f * (1.0f - u);   // stay ringing
        knob[2] = 0.05f + 0.90f * u;            // strike → bow (A) / long → dead (B)
        excite_hold = 0.05f;
        cur_preset = -1;
        apply_knobs();
        strike(walk[i], 6);
        apos++;
    }

#ifdef DE_TRACE
    watch("harm", "%.2f", knob[0]);
    watch("timb", "%.2f", knob[1]);
    watch("mor",  "%.2f", knob[2]);
    watch("map",  "%d", elements);
    watch("preset", "%d", cur_preset);
    watch("excite", "%.2f", excite_of());
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("MODAL", 8, 6, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print("exciter into resonator", 56, 8, CLR_MEDIUM_GREY);

    auto_rx = SCREEN_W - 118; auto_ry = 2; auto_rw = 110; auto_rh = 14;
    print_right(autoplay ? "M walk+macros" : "M autoplay: off", SCREEN_W - 10, 8,
                autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);

    ab_rx = SCREEN_W - 118; ab_ry = 16; ab_rw = 110; ab_rh = 16;
    rectfill(ab_rx, ab_ry, ab_rw, ab_rh, elements ? CLR_INDIGO : CLR_DARK_BROWN);
    rect(ab_rx, ab_ry, ab_rw, ab_rh, CLR_WHITE);
    print(elements ? "A/B Elements" : "A/B recommended", ab_rx + 4, ab_ry + 4,
          elements ? CLR_LIGHT_YELLOW : CLR_PEACH);

    fork_rx = 8; fork_ry = 16; fork_rw = 50; fork_rh = 16;
    rectfill(fork_rx, fork_ry, fork_rw, fork_rh, cur_preset == -2 ? CLR_DARK_BROWN : CLR_BROWNISH_BLACK);
    rect(fork_rx, fork_ry, fork_rw, fork_rh, cur_preset == -2 ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    print("0 fork", fork_rx + 4, fork_ry + 4, cur_preset == -2 ? CLR_LIGHT_YELLOW : CLR_MEDIUM_GREY);

    font(FONT_NORMAL);
    for (int b = 0; b < NBAR; b++) {
        int x = BAR_X(b), h = BAR_H(b);
        glow[b] *= 0.92f;
        int col = glow[b] > 0.5f ? CLR_LIGHT_YELLOW
                : glow[b] > 0.1f ? CLR_PEACH
                                 : CLR_DARK_BROWN;
        int bump = (int)(glow[b] * 3.0f);
        rectfill(x, BAR_Y - bump, BAR_W, h, col);
        rect(x, BAR_Y - bump, BAR_W, h, glow[b] > 0.1f ? CLR_WHITE : CLR_DARK_GREY);
        int ty = BAR_Y + h + 4, th = 16 + (NBAR - b) * 2;
        rectfill(x + 8, ty, BAR_W - 16, th, CLR_DARKER_GREY);
        print(str("%c", STRKEY[b]), x + BAR_W / 2 - 2, BAR_Y + h - 11,
              glow[b] > 0.1f ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREY);
    }

    font(FONT_SMALL);
    for (int p = 0; p < NPRESET; p++) {
        int x = 10 + p * 52;
        bool on = (p == cur_preset);
        print(str("%d %s", p + 1, PRESET[p].name), x, KNOB_Y - 26, on ? CLR_YELLOW : CLR_DARK_GREY);
    }

    for (int k = 0; k < 3; k++) {
        int x = KNOB_X(k), y = KNOB_Y;
        bool on = (k == sel);
        font(FONT_SMALL);
        print(knob_name(k), x, y - 8, on ? CLR_YELLOW : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
        bar(x, y, KNOB_W, 7, knob[k], on ? CLR_ORANGE : CLR_BROWN, CLR_DARKER_GREY);
        font(FONT_TINY);
        print(knob_lo(k), x, y + 9, CLR_DARK_GREY);
        print_right(knob_hi(k), x + KNOB_W, y + 9, CLR_DARK_GREY);
        font(FONT_NORMAL);
        if (on) print(">", x - 9, y, CLR_YELLOW);
    }

    font(FONT_TINY);
    gliss_rx = print("A..K  1..6  0 fork  ", 10, SCREEN_H - 9, CLR_DARK_GREY);
    print("SPACE gliss", gliss_rx, SCREEN_H - 9, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}

#ifdef DE_SPEC
#include "spec.h"
void spec(void) {
    autoplay = false;
    step(1);
    expect(elements == 0, "boots on the recommended mapping (morph = exciter)");
    expect(spec_close(knob[0], 0.62f, 0.02f), "boots at the marimba geometry");
    spec_tap('2');
    expect_eq(cur_preset, 1, "key 2 loads the glass-bowl preset");
    expect(spec_close(knob[0], 0.92f, 0.02f), "bowl geometry is near-bell");
    spec_tap('B');
    expect_eq(elements, 1, "B toggles to the Elements mapping");
    expect(spec_close(knob[1], 0.12f, 0.02f), "A/B keeps the same gesture (no rematch)");
    expect(spec_close(knob[2], 0.10f, 0.02f), "morph stays put; mapping reinterprets it");
    spec_tap('B');
    expect_eq(elements, 0, "B toggles back to recommended");
    expect(spec_close(knob[2], 0.10f, 0.02f), "knobs still the bowl recommended triple");
    spec_tap('0');
    expect_eq(cur_preset, -2, "key 0 loads the hearable A/B fork");
    expect(spec_close(knob[2], 0.95f, 0.02f), "fork morph is bow (A) / short (B)");
    expect(spec_close(excite_hold, 0.04f, 0.02f), "fork keeps Elements excite as strike");
    spec_tap('1');
    expect_eq(cur_preset, 0, "key 1 is marimba");
    spec_tap('6');
    expect_eq(cur_preset, 5, "key 6 is breath");
    expect(excite_of() > 0.4f, "breath is a blow, not a strike");
}
#endif
