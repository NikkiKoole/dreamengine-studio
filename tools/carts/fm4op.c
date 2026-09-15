/* de:meta
{
  "slug": "fm4op",
  "title": "fm4op",
  "status": "active",
  "created": "2026-09-15",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "fm-synth",
    "adsr-envelope",
    "analog-voice-modeling"
  ],
  "lineage": "INSTR_FM4 showcase — the engine-reach §7.2 four-operator FM engine (TX81Z / OPN / OPM / OPZ eight-algorithm family, not a free matrix). The cart is the tuning rig for the two three-macro mappings in engine-reach-macro-mapping.md §3; both stay live as a tappable A/B, not a #define.",
  "description": {
    "summary": "Four-operator FM — curated Yamaha algorithms, not a DX7.",
    "detail": "INSTR_FM4 showcase: four operators, eight OPN/OPM/OPZ algorithms, three macros. Two mappings stay live — tap A/B (or B) to reinterpret the SAME three knobs (recommended: voicing / brightness / feedback vs alt: algorithm / brightness / feedback, ratios via MODE_*). That fork is meant to be opposite personalities, not a subtle EQ. 0 loads the hearable-fork gesture (√2 tube-bell vs a 1:1 stack). Named presets (tine, bell, metal, brass, bass, wood, glass, organ) still voice themselves when you re-pick them. Autoplay walks a scale while sweeping the macros. The engine does not bake amplitude — each preset carries an ADSR.",
    "controls": "Tap or sweep the keys (multitouch) · A S D F G H J K play · 1..8 presets · 0 hearable A/B fork · tap A/B (or B) to swap mapping (same knobs) · drag the sliders · M autoplay (scale + macros) · SPACE chord"
  },
  "todo": [
    "ear-settle which mapping won (recommended harmonics=voicing vs alt harmonics=algorithm) — both stay live via A/B until then",
    "bake a real thumbnail once someone can make-cart --run on a machine with a window",
    "playbook step 7: retrofit one radio station's 2-op FM fake once the macros settle"
  ]
}
de:meta */
// fm4op — INSTR_FM4 showcase: eight keys + the three engine macros + a live A/B mapping toggle.
//
// The engine-reach §7.2 engine: four operators, the Yamaha eight-algorithm family
// (TX81Z / DX21 / DX100 / OPN / OPM / OPZ), phase-mod of the accumulator so
// feedback stays stable. Both three-macro routes from engine-reach-macro-mapping.md
// §3 stay hearable from this one build — tap A/B (or press B). Same three knobs,
// opposite mapping (no rematch). Key 0 is the hearable fork. A #define would need
// two builds and could not be judged on a phone.
//
//   recommended: harmonics = voicing (whole patch), timbre = brightness, morph = feedback
//   alt:         harmonics = algorithm (8 snapped), timbre = brightness, morph = feedback
//                (the four ratios come from MODE_FM4_R0..R3, baked per preset)
//
// controls: tap/sweep the keys (multitouch) · A S D F G H J K
//           1..8 presets · 0 hearable A/B fork · tap A/B (or B) mapping (keeps knobs)
//           drag sliders · M autoplay (slow scale + macro sweep) · SPACE chord

#include "studio.h"
#include "pointer.h"
#include <math.h>

#define I_FM4 5
#define NKEY  8
#define NPRESET 8

static const char STRKEY[NKEY] = { 'A','S','D','F','G','H','J','K' };

typedef struct {
    const char *name;
    float hA;                // recommended: voicing detent
    float tA, mA;
    float hB;                // alt: algorithm detent
    float tB, mB;
    float r0, r1, r2, r3;    // MODE_FM4_R* (alt route)
    float ea, ed, es, er;    // ADSR in slider space (0..1) — a DX patch is macros + envelope
} Preset;

// (alg + 0.5) / 8  and  (ratio-detent + 0.5) / 13  so the engine lands on the named slot.
// ratio detents: 0=0.5  1=1  2=√2  3=1.5  4=φ  5=2  6=2.414  7=3  8=3.5  9=4  10=5  11=7  12=14
#define RDET(i)  (((float)(i) + 0.5f) / 13.0f)
#define ADET(a)  (((float)(a) + 0.5f) /  8.0f)
#define VDET(v)  (((float)(v) + 0.5f) /  8.0f)

static const Preset PRESET[NPRESET] = {
    { "tine",  VDET(0), 0.45f, 0.10f,  ADET(4), 0.45f, 0.10f,  RDET(1), RDET(12), RDET(1), RDET(1),  0.06f, 0.66f, 0.43f, 0.47f },
    { "bell",  VDET(1), 0.60f, 0.15f,  ADET(4), 0.60f, 0.15f,  RDET(1), RDET(2),  RDET(1), RDET(2),  0.00f, 0.75f, 0.29f, 0.62f },
    { "metal", VDET(2), 0.85f, 0.55f,  ADET(0), 0.85f, 0.55f,  RDET(1), RDET(8),  RDET(2), RDET(6),  0.00f, 0.61f, 0.29f, 0.50f },
    { "brass", VDET(3), 0.90f, 0.40f,  ADET(1), 0.90f, 0.40f,  RDET(1), RDET(1),  RDET(1), RDET(1),  0.48f, 0.32f, 0.86f, 0.33f },
    { "bass",  VDET(4), 0.70f, 0.25f,  ADET(6), 0.70f, 0.25f,  RDET(0), RDET(0),  RDET(1), RDET(5),  0.00f, 0.36f, 0.71f, 0.26f },
    { "wood",  VDET(5), 0.40f, 0.12f,  ADET(6), 0.40f, 0.12f,  RDET(1), RDET(5),  RDET(7), RDET(9),  0.04f, 0.50f, 0.50f, 0.40f },
    { "glass", VDET(6), 0.55f, 0.20f,  ADET(5), 0.55f, 0.20f,  RDET(1), RDET(6),  RDET(8), RDET(4),  0.00f, 0.70f, 0.25f, 0.70f },
    { "organ", VDET(7), 0.35f, 0.05f,  ADET(7), 0.35f, 0.05f,  RDET(0), RDET(1),  RDET(5), RDET(7),  0.08f, 0.20f, 0.86f, 0.30f },
};

static int   midi_of[NKEY];
static float glow[NKEY];
static int   pend[NKEY];
static float knob[3] = { VDET(0), 0.45f, 0.10f };
static float envk[4] = { 0.06f, 0.66f, 0.43f, 0.47f };
static int   sel = 0;
static int   cur_preset = 0;
static int   altmap = 0;            // 0 = recommended, 1 = alt (MODE_FM4_MAP)
static float mode_r[4] = { RDET(1), RDET(12), RDET(1), RDET(1) };
static bool  autoplay = true;
static int   apos = 0;
static int   chord_rx = -1;
static int   ab_rx = 0, ab_ry = 0, ab_rw = 88, ab_rh = 16;
static int   auto_rx = 0, auto_ry = 0, auto_rw = 108, auto_rh = 14;
static int   fork_rx = 8, fork_ry = 16, fork_rw = 50, fork_rh = 16;

enum { PTR_IDLE, PTR_DRAG, PTR_SWEEP };
typedef struct { int id, mode, k, prevX; } Ptr;
static Ptr ptr[PTR_MAX];

#define KEY_W    26
#define KEY_GAP  6
#define KEY_X(b) (14 + (b) * (KEY_W + KEY_GAP))
#define KEY_Y    36
#define KEY_H    56
#define KNOB_W   88
#define KNOB_Y   (SCREEN_H - 40)
#define KNOB_X(k) (14 + (k) * 102)

static const char *knob_name(int k) {
    static const char *A[3] = { "voicing", "bright", "feedback" };
    static const char *B[3] = { "algorithm", "bright", "feedback" };
    return altmap ? B[k] : A[k];
}
static const char *knob_lo(int k) {
    static const char *A[3] = { "tine", "clean", "clean" };
    static const char *B[3] = { "stack", "clean", "clean" };
    return altmap ? B[k] : A[k];
}
static const char *knob_hi(int k) {
    static const char *A[3] = { "organ", "scream", "clang" };
    static const char *B[3] = { "add",   "scream", "clang" };
    return altmap ? B[k] : A[k];
}

static int env_a(void) { return 1  + (int)(envk[0] * envk[0] * 300.0f); }
static int env_d(void) { return 50 + (int)(envk[1] * envk[1] * 1500.0f); }
static int env_s(void) { return (int)(envk[2] * 7.0f + 0.5f); }
static int env_r(void) { return 20 + (int)(envk[3] * envk[3] * 1500.0f); }

static void apply_modes(void) {
    instrument_mode(I_FM4, MODE_FM4_MAP, altmap ? 1.0f : 0.0f);
    instrument_mode(I_FM4, MODE_FM4_R0, mode_r[0]);
    instrument_mode(I_FM4, MODE_FM4_R1, mode_r[1]);
    instrument_mode(I_FM4, MODE_FM4_R2, mode_r[2]);
    instrument_mode(I_FM4, MODE_FM4_R3, mode_r[3]);
}

static void apply_knobs(void) {
    instrument(I_FM4, INSTR_FM4, env_a(), env_d(), env_s(), env_r());
    instrument_harmonics(I_FM4, knob[0]);
    instrument_timbre(I_FM4, knob[1]);
    instrument_morph(I_FM4, knob[2]);
    apply_modes();
}

static int gate_ms(void) {
    /* hit() is a one-shot. High sustain (brass/organ) must stay gated past
       attack+decay or the swell dies mid-note. A ping keeps the short gate. */
    int hold = env_s() >= 5 ? 1600 : 400;
    return env_a() + env_d() + hold + env_r();
}

static void strike(int b, int vol) {
    hit(midi_of[b], I_FM4, vol, gate_ms());
    glow[b] = 1.0f;
}

static void triad(void) {
    strike(0, 5); strike(2, 5); strike(4, 6);
}

static void set_preset(int p) {
    cur_preset = p;
    if (altmap) { knob[0] = PRESET[p].hB; knob[1] = PRESET[p].tB; knob[2] = PRESET[p].mB; }
    else        { knob[0] = PRESET[p].hA; knob[1] = PRESET[p].tA; knob[2] = PRESET[p].mA; }
    mode_r[0] = PRESET[p].r0; mode_r[1] = PRESET[p].r1;
    mode_r[2] = PRESET[p].r2; mode_r[3] = PRESET[p].r3;
    envk[0] = PRESET[p].ea; envk[1] = PRESET[p].ed;
    envk[2] = PRESET[p].es; envk[3] = PRESET[p].er;
    apply_knobs();
    strike(4, 6);
}

// Hearable A/B fork: SAME three knobs, opposite personalities.
// Recommended (harmonics=voicing): tube-bell at √2 — inharmonic, no common fundamental.
// Alt (harmonics=algorithm): algorithm 1 (1+2→3→4) with 1:1:1:1 — a harmonic stack.
// Paper maps on a shared tine were too close — this gesture is the ear test.
static void load_fork(void) {
    cur_preset = -2;
    knob[0] = VDET(1);          // bell voicing (A) / algorithm 1 (B) — 1.5/8 = 0.1875
    knob[1] = 0.70f;
    knob[2] = 0.18f;
    mode_r[0] = RDET(1); mode_r[1] = RDET(1);
    mode_r[2] = RDET(1); mode_r[3] = RDET(1);
    envk[0] = 0.00f; envk[1] = 0.75f; envk[2] = 0.29f; envk[3] = 0.62f;
    apply_knobs();
    // No auto-strike: scripts load the fork then play the same notes under
    // each mapping. A ding here would leave a recommended bell ringing under map-b.
}

static void toggle_map(void) {
    // Same gesture, opposite mapping — do NOT rematch preset knobs.
    // Re-pick 1..8 if you want that route's named voicing.
    altmap = !altmap;
    apply_knobs();
    strike(4, 5);
}

void init(void) {
    instrument(I_FM4, INSTR_FM4, 2, 400, 3, 600);
    apply_knobs();
    PTR_CLEAR(ptr);
    for (int b = 0; b < NKEY; b++) midi_of[b] = degree(SCALE_PENTA, 4, b + 1);
    bpm(96);
    glow[2] = 0.6f; glow[5] = 0.9f;
    // autoplay stays at its file-scope default (on). spec() turns it off
    // before the first step so a walk cannot rewrite the knobs under test.
}

void update(void) {
    for (int b = 0; b < NKEY; b++) {
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
    if (keyp(KEY_SPACE) || (chord_rx >= 0 && tapp(chord_rx - 4, SCREEN_H - 16, 72, 16))) triad();

    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;
        if (fresh) {
            *p = (Ptr){ id, PTR_IDLE, -1, tx };
            if (ty >= KNOB_Y - 30 && ty < KNOB_Y - 14) {
                for (int q = 0; q < NPRESET; q++)
                    if (tx >= 6 + q * 39 && tx < 6 + q * 39 + 38) set_preset(q);
                continue;
            }
            for (int k = 0; k < 3; k++)
                if (point_in_box(tx, ty, KNOB_X(k) - 2, KNOB_Y - 8, KNOB_W + 4, 20)) {
                    p->mode = PTR_DRAG; p->k = sel = k;
                }
            if (p->mode == PTR_IDLE && ty >= KEY_Y - 4 && ty < KNOB_Y - 32) {
                for (int b = 0; b < NKEY; b++)
                    if (point_in_box(tx, ty, KEY_X(b), KEY_Y, KEY_W, KEY_H))
                        strike(b, 6);
                p->mode = PTR_SWEEP; p->prevX = tx;
            }
        } else if (p->mode == PTR_DRAG) {
            knob[p->k] = clamp((float)(tx - KNOB_X(p->k)) / (float)KNOB_W, 0.0f, 1.0f);
            cur_preset = -1;
            apply_knobs();
            if (frame() % 12 == 0) strike(4, 5);
        } else if (p->mode == PTR_SWEEP) {
            for (int b = 0; b < NKEY; b++) {
                int cx = KEY_X(b) + KEY_W / 2;
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
        // move together (not a static note).
        static const int walk[8] = { 0, 2, 4, 7, 5, 4, 2, 0 };
        int i = apos & 7;
        float u = (float)i / 7.0f;
        knob[0] = 0.06f + 0.88f * u;            // tine → organ  /  stack → additive
        knob[1] = 0.35f + 0.45f * (1.0f - u);   // stay present
        knob[2] = 0.08f + 0.50f * u;            // clean → growl
        cur_preset = -1;
        apply_knobs();
        strike(walk[i], 6);
        apos++;
    }

#ifdef DE_TRACE
    watch("harm", "%.2f", knob[0]);
    watch("timb", "%.2f", knob[1]);
    watch("mor",  "%.2f", knob[2]);
    watch("map",  "%d", altmap);
    watch("preset", "%d", cur_preset);
#endif
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("FM4OP", 8, 6, CLR_BLUE);
    font(FONT_SMALL);
    print("four-operator FM", 56, 8, CLR_MEDIUM_GREY);

    auto_rx = SCREEN_W - 118; auto_ry = 2; auto_rw = 110; auto_rh = 14;
    print_right(autoplay ? "M walk+macros" : "M autoplay: off", SCREEN_W - 10, 8,
                autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);

    ab_rx = SCREEN_W - 118; ab_ry = 16; ab_rw = 110; ab_rh = 16;
    rectfill(ab_rx, ab_ry, ab_rw, ab_rh, altmap ? CLR_INDIGO : CLR_DARK_BLUE);
    rect(ab_rx, ab_ry, ab_rw, ab_rh, CLR_WHITE);
    print(altmap ? "A/B algorithm" : "A/B voicing", ab_rx + 4, ab_ry + 4,
          altmap ? CLR_LIGHT_YELLOW : CLR_BLUE);

    fork_rx = 8; fork_ry = 16; fork_rw = 50; fork_rh = 16;
    rectfill(fork_rx, fork_ry, fork_rw, fork_rh, cur_preset == -2 ? CLR_DARK_BLUE : CLR_DARKER_BLUE);
    rect(fork_rx, fork_ry, fork_rw, fork_rh, cur_preset == -2 ? CLR_BLUE : CLR_DARK_GREY);
    print("0 fork", fork_rx + 4, fork_ry + 4, cur_preset == -2 ? CLR_BLUE : CLR_MEDIUM_GREY);

    font(FONT_NORMAL);
    for (int b = 0; b < NKEY; b++) {
        int x = KEY_X(b);
        glow[b] *= 0.92f;
        int col = glow[b] > 0.5f ? CLR_BLUE
                : glow[b] > 0.1f ? CLR_TRUE_BLUE
                                 : CLR_DARK_BLUE;
        int bump = (int)(glow[b] * 3.0f);
        rectfill(x, KEY_Y - bump, KEY_W, KEY_H, col);
        rect(x, KEY_Y - bump, KEY_W, KEY_H, glow[b] > 0.1f ? CLR_WHITE : CLR_DARK_GREY);
        print(str("%c", STRKEY[b]), x + KEY_W / 2 - 2, KEY_Y + KEY_H - 11,
              glow[b] > 0.1f ? CLR_DARKER_BLUE : CLR_MEDIUM_GREY);
    }

    font(FONT_SMALL);
    for (int p = 0; p < NPRESET; p++) {
        int x = 6 + p * 39;
        bool on = (p == cur_preset);
        print(str("%d %s", p + 1, PRESET[p].name), x, KNOB_Y - 26, on ? CLR_YELLOW : CLR_DARK_GREY);
    }

    for (int k = 0; k < 3; k++) {
        int x = KNOB_X(k), y = KNOB_Y;
        bool on = (k == sel);
        font(FONT_SMALL);
        print(knob_name(k), x, y - 8, on ? CLR_YELLOW : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
        bar(x, y, KNOB_W, 7, knob[k], on ? CLR_ORANGE : CLR_BLUE, CLR_DARKER_GREY);
        font(FONT_TINY);
        print(knob_lo(k), x, y + 9, CLR_DARK_GREY);
        print_right(knob_hi(k), x + KNOB_W, y + 9, CLR_DARK_GREY);
        font(FONT_NORMAL);
        if (on) print(">", x - 9, y, CLR_YELLOW);
    }

    font(FONT_TINY);
    chord_rx = print("A..K  1..8  0 fork  ", 10, SCREEN_H - 9, CLR_DARK_GREY);
    print("SPACE chord", chord_rx, SCREEN_H - 9, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}

#ifdef DE_SPEC
#include "spec.h"
void spec(void) {
    autoplay = false;
    step(1);
    expect(altmap == 0, "boots on the recommended mapping (harmonics = voicing)");
    expect(spec_close(knob[0], VDET(0), 0.02f), "boots at the tine voicing");
    spec_tap('2');
    expect_eq(cur_preset, 1, "key 2 loads the tube-bell preset");
    expect(spec_close(knob[0], VDET(1), 0.02f), "bell voicing is detent 1");
    spec_tap('B');
    expect_eq(altmap, 1, "B toggles to the algorithm mapping");
    expect(spec_close(knob[1], 0.60f, 0.02f), "A/B keeps the same gesture (no rematch)");
    expect(spec_close(knob[2], 0.15f, 0.02f), "morph stays put; mapping reinterprets harmonics");
    spec_tap('B');
    expect_eq(altmap, 0, "B toggles back to recommended");
    expect(spec_close(knob[0], VDET(1), 0.02f), "knobs still the bell recommended triple");
    spec_tap('0');
    expect_eq(cur_preset, -2, "key 0 loads the hearable A/B fork");
    expect(spec_close(knob[0], VDET(1), 0.02f), "fork harmonics is bell voicing / alg 1");
    expect(spec_close(mode_r[1], RDET(1), 0.02f), "fork keeps alt ratios at 1:1:1:1");
    spec_tap('1');
    expect_eq(cur_preset, 0, "key 1 is tine");
    spec_tap('8');
    expect_eq(cur_preset, 7, "key 8 is organ");
}
#endif
