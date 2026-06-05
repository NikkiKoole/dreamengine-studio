// fm — INSTR_FM showcase: two operators, three macros, four envelope sliders, one scope.
//
// The third modeled ENGINE: a carrier sine you hear, and an inaudible modulator sine
// that bends the carrier's phase — the DX-synth recipe. Brightness DECAYS within each
// note (bright strike → mellow tail) and SWELLS along a slow amp attack (so brass can
// speak). Every engine answers the same three 0..1 macro knobs — the API never grows:
//   harmonics = carrier:mod RATIO (snapped detents — integers musical, offs = bells/metal)
//   timbre    = brightness        (FM amount; decays per note like a real DX strike)
//   morph     = feedback          (0 clean .. saw-edge growl .. noisy clang)
//
// This cart is the engine's MODELING RIG: the FM engine deliberately doesn't bake its
// amp envelope, so a DX patch is macros + an ADSR — both live here as sliders. Dial a
// sound until it earns a nameplate; the panel prints the exact instrument() call to
// copy into your cart. The 1:1 detent carries the built-in DX tine (E.PIANO attack ping).
//
// controls: A S D F G H J K  play (major scale)   ·   1..5 presets:
//           epiano / bell / bass / brass / clang
//           drag any slider with the mouse (auditions as you drag), or
//           LEFT/RIGHT pick a slider + UP/DOWN turn it (7 sliders: 3 macros + ADSR)
//           SPACE chord   ·   M autoplay on/off

#include "studio.h"
#include <math.h>

#define I_FM  5
#define NKEY  8

static const char STRKEY[NKEY] = { 'A','S','D','F','G','H','J','K' };
static const char *MNAME[3] = { "harmonics (ratio)", "timbre (bright)", "morph (feedback)" };
static const char *MLO[3]   = { "sub",  "mellow", "clean" };
static const char *MHI[3]   = { "tine", "bright", "clang" };
static const char *ENAME[4] = { "att", "dec", "sus", "rel" };

// MUST match the engine's snapped table (sound.h sound_fm_sample) — display only
static const float RATIO[10] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 3.5f, 4.0f, 5.0f, 7.0f, 14.0f };

// the presets: macro positions + an ADSR, both in slider space (0..1). The engine doesn't
// bake its amp envelope, so a DX patch is BOTH — brass without its slow attack can never
// speak. Clang sits on the 3.5 detent, not 14: integer ratios are HARMONIC (buzzy);
// metal needs non-integer sidebands. STARTING GUESSES — this rig exists to better them.
typedef struct { const char *name; float h, t, m, ea, ed, es, er; } Preset;
static const Preset PRESET[5] = {
    { "epiano", 0.15f, 0.45f, 0.10f, 0.06f, 0.66f, 0.43f, 0.47f },
    { "bell",   0.55f, 0.60f, 0.15f, 0.00f, 0.75f, 0.29f, 0.62f },
    { "bass",   0.00f, 0.75f, 0.30f, 0.00f, 0.36f, 0.71f, 0.26f },
    { "brass",  0.15f, 0.90f, 0.45f, 0.48f, 0.32f, 0.86f, 0.33f },   // the attack IS the brass
    { "clang",  0.55f, 0.95f, 0.95f, 0.00f, 0.61f, 0.29f, 0.50f },   // bell's detent, feedback cranked
};

#define NSLIDER 7   // 0..2 = macros, 3..6 = ADSR
static int   midi_of[NKEY];
static float glow[NKEY];
static float knob[NSLIDER];    // macro 0..2 + env 3..6, all 0..1
static int   sel = 0;
static int   drag_k = -1;
static bool  autoplay = true;
static int   cur_preset = 0;
static int   apos = 0;
static int   scope_age = 9999;

// geometry: scope · keys · presets · macro row · env row · footer
#define KEY_W    34
#define KEY_X(b) (10 + (b) * (KEY_W + 4))
#define KEY_Y    88
#define KEY_H    24
#define MROW_Y   138
#define MROW_W   88
#define MROW_X(k) (14 + (k) * 102)
#define EROW_Y   172
#define EROW_W   50
#define EROW_X(k) (14 + (k) * 62)

// envelope slider space → instrument() ms/level (squared = finer control at the low end)
static int env_a(void) { return 1  + (int)(knob[3] * knob[3] * 300.0f); }
static int env_d(void) { return 50 + (int)(knob[4] * knob[4] * 1500.0f); }
static int env_s(void) { return (int)(knob[5] * 7.0f + 0.5f); }
static int env_r(void) { return 20 + (int)(knob[6] * knob[6] * 1500.0f); }

static void apply_patch(void) {
    instrument(I_FM, INSTR_FM, env_a(), env_d(), env_s(), env_r());
    instrument_harmonics(I_FM, knob[0]);
    instrument_timbre(I_FM, knob[1]);
    instrument_morph(I_FM, knob[2]);
}

static void play_key(int b, int vol) {
    note(midi_of[b], I_FM, vol);
    glow[b] = 1.0f;
    scope_age = 0;
}

static void play_chord(void) {
    chord(midi_of[0], CHORD_MAJ7, I_FM, 5);
    glow[0] = glow[2] = glow[4] = 1.0f;
    scope_age = 0;
}

static void set_preset(int p) {
    knob[0] = PRESET[p].h;  knob[1] = PRESET[p].t;  knob[2] = PRESET[p].m;
    knob[3] = PRESET[p].ea; knob[4] = PRESET[p].ed; knob[5] = PRESET[p].es; knob[6] = PRESET[p].er;
    cur_preset = p;
    apply_patch();
    play_key(2, 6);
}

void init(void) {
    set_preset(0);
    for (int b = 0; b < NKEY; b++) midi_of[b] = degree(SCALE_MAJOR, 4, b);
    bpm(96);
}

void update(void) {
    for (int b = 0; b < NKEY; b++)
        if (keyp(STRKEY[b])) play_key(b, 6);
    for (int p = 0; p < 5; p++)
        if (keyp('1' + p)) set_preset(p);

    if (keyp(KEY_LEFT))  sel = (sel + NSLIDER - 1) % NSLIDER;
    if (keyp(KEY_RIGHT)) sel = (sel + 1) % NSLIDER;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        knob[sel] = clamp(knob[sel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        cur_preset = -1;
        apply_patch();
        if (frame() % 12 == 0) play_key(2, 5);
    }

    if (keyp(KEY_SPACE)) play_chord();
    if (keyp('M')) autoplay = !autoplay;

    if (mouse_pressed(MOUSE_LEFT)) {
        for (int k = 0; k < 3; k++)
            if (point_in_box(mouse_x(), mouse_y(), MROW_X(k) - 2, MROW_Y - 6, MROW_W + 4, 18))
                drag_k = sel = k;
        for (int k = 0; k < 4; k++)
            if (point_in_box(mouse_x(), mouse_y(), EROW_X(k) - 2, EROW_Y - 6, EROW_W + 4, 18))
                drag_k = sel = 3 + k;
        if (drag_k < 0)
            for (int b = 0; b < NKEY; b++)
                if (point_in_box(mouse_x(), mouse_y(), KEY_X(b), KEY_Y, KEY_W, KEY_H))
                    play_key(b, 6);
    }
    if (drag_k >= 0) {
        if (mouse_down(MOUSE_LEFT)) {
            int x0 = drag_k < 3 ? MROW_X(drag_k) : EROW_X(drag_k - 3);
            int w  = drag_k < 3 ? MROW_W : EROW_W;
            knob[drag_k] = clamp((float)(mouse_x() - x0) / (float)w, 0.0f, 1.0f);
            cur_preset = -1;
            apply_patch();
            if (frame() % 12 == 0) play_key(2, 5);
        } else drag_k = -1;
    }

    // autoplay: epiano comping — a maj7 chord at the top, a noodle between
    if (autoplay && every(1)) {
        static const int seq[16] = { 2,4,5,7, 4,2,0,4, 5,7,6,4, 2,0,1,2 };
        if (beat() % 8 == 0) play_chord();
        else {
            play_key(seq[apos % 16] % NKEY, 4);
            if (chance(25)) schedule_hit(280, midi_of[(seq[apos % 16] + 2) % NKEY], I_FM, 3, 250);
        }
        apos++;
    }
    scope_age++;

#ifdef DE_TRACE
    watch("harm", "%.2f", knob[0]);
    watch("timb", "%.2f", knob[1]);
    watch("mor",  "%.2f", knob[2]);
    watch("adsr", "%d/%d/%d/%d", env_a(), env_d(), env_s(), env_r());
    watch("preset", "%d", cur_preset);
#endif
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("FM", 8, 6, CLR_BLUE);
    font(FONT_SMALL);
    print("two-operator fm engine", 32, 8, CLR_MEDIUM_GREY);
    print_right(autoplay ? "M autoplay: on" : "M autoplay: off", SCREEN_W - 10, 8, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // the engine's parameters, derived exactly like sound_fm_sample does
    int   ri    = (int)(knob[0] * 9.999f); if (ri > 9) ri = 9;
    float ratio = RATIO[ri];
    float settle = 0.25f + 0.75f * expf(-(float)scope_age / 54.0f);   // ~0.9s at 60fps
    float beta  = knob[1] * knob[1] * 12.0f * settle;
    // the DX tine (1:1 detent only) — same containment as the engine
    float tine  = (ri == 1) ? 0.18f * knob[1] * (1.0f - knob[2]) * expf(-(float)scope_age / 1.5f) : 0.0f;

    // the scope: the actual FM formula, two carrier cycles, feedback iterated
    int sy = 50, sx0 = 10, sx1 = SCREEN_W - 10, prev_y = sy;
    rectfill(sx0 - 2, 22, sx1 - sx0 + 4, 58, CLR_DARK_BLUE);
    line(sx0, sy, sx1, sy, CLR_DARKER_GREY);
    float fb_s = 0.0f;
    for (int x = sx0; x <= sx1; x++) {
        float t = (float)(x - sx0) / (float)(sx1 - sx0) * 2.0f;       // 2 carrier cycles
        float m = sinf(t * ratio * 6.2831853f + knob[2] * 1.3f * fb_s * 3.14159265f);
        fb_s = m;
        float s = sinf(t * 6.2831853f + m * beta) + tine * sinf(t * 14.0f * 6.2831853f);
        int y = sy - (int)(s * 22.0f);
        if (x > sx0) line(x - 1, prev_y, x, y, CLR_BLUE);
        prev_y = y;
    }
    font(FONT_TINY);
    print(str("ratio 1:%.1f%s   brightness %.1f   feedback %.2f", ratio,
              ri == 1 ? " +tine" : "", beta, knob[2] * 1.3f),
          sx0, 24, CLR_MEDIUM_GREY);
    // the copy-into-your-cart line: this exact call reproduces the current patch
    print(str("instrument(I, INSTR_FM, %d, %d, %d, %d)  h %.2f t %.2f m %.2f",
              env_a(), env_d(), env_s(), env_r(), knob[0], knob[1], knob[2]),
          sx0, 72, CLR_BLUE_GREEN);
    font(FONT_NORMAL);

    // the keys
    for (int b = 0; b < NKEY; b++) {
        int x = KEY_X(b);
        glow[b] *= 0.90f;
        int col = glow[b] > 0.5f ? CLR_LIGHT_YELLOW : glow[b] > 0.1f ? CLR_BLUE : CLR_DARK_BLUE;
        rectfill(x, KEY_Y, KEY_W, KEY_H, col);
        rect(x, KEY_Y, KEY_W, KEY_H, glow[b] > 0.1f ? CLR_WHITE : CLR_DARK_GREY);
        print(str("%c", STRKEY[b]), x + KEY_W / 2 - 3, KEY_Y + KEY_H - 10,
              glow[b] > 0.5f ? CLR_DARKER_BLUE : CLR_MEDIUM_GREY);
    }

    // preset row
    font(FONT_SMALL);
    for (int p = 0; p < 5; p++) {
        int x = 14 + p * 58;
        bool on = (p == cur_preset);
        print(str("%d %s", p + 1, PRESET[p].name), x, KEY_Y + KEY_H + 6, on ? CLR_YELLOW : CLR_DARK_GREY);
    }
    font(FONT_NORMAL);

    // macro row
    for (int k = 0; k < 3; k++) {
        int x = MROW_X(k), y = MROW_Y;
        bool on = (k == sel);
        font(FONT_SMALL);
        print(MNAME[k], x, y - 8, on ? CLR_YELLOW : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
        bar(x, y, MROW_W, 7, knob[k], on ? CLR_ORANGE : CLR_TRUE_BLUE, CLR_DARK_BLUE);
        font(FONT_TINY);
        print(MLO[k], x, y + 9, CLR_DARK_GREY);
        print_right(MHI[k], x + MROW_W, y + 9, CLR_DARK_GREY);
        font(FONT_NORMAL);
        if (on) print(">", x - 9, y, CLR_YELLOW);
    }

    // envelope row — the other half of the patch
    for (int k = 0; k < 4; k++) {
        int x = EROW_X(k), y = EROW_Y;
        bool on = (3 + k == sel);
        int v = k == 0 ? env_a() : k == 1 ? env_d() : k == 2 ? env_s() : env_r();
        font(FONT_SMALL);
        print(str(k == 2 ? "%s %d" : "%s %dms", ENAME[k], v), x, y - 8, on ? CLR_YELLOW : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
        bar(x, y, EROW_W, 7, knob[3 + k], on ? CLR_ORANGE : CLR_BLUE_GREEN, CLR_DARK_BLUE);
        if (on) print(">", x - 9, y, CLR_YELLOW);
    }

    // the envelope, VISIBLE: the ADSR as a shape, segment widths true to their ms.
    // The selected slider's segment lights up — drag and watch the contour move.
    {
        int gx = 264, gw = SCREEN_W - 10 - 264, gh = 20, gb = EROW_Y + 7;
        rectfill(gx - 2, gb - gh - 2, gw + 4, gh + 4, CLR_DARK_BLUE);
        float a = (float)env_a(), d = (float)env_d(), r = (float)env_r();
        float sl = (float)env_s() / 7.0f;
        float hold = 250.0f;                          // fixed visual chunk for the sustain
        float tot  = a + d + hold + r;
        int x1 = gx + (int)(gw * a / tot);
        int x2 = x1 + (int)(gw * d / tot);
        int x3 = x2 + (int)(gw * hold / tot);
        int x4 = gx + gw;
        int top = gb - gh, sus = gb - (int)(sl * gh);
        line(gx, gb, x1, top, sel == 3 ? CLR_YELLOW : CLR_BLUE_GREEN);   // attack
        line(x1, top, x2, sus, sel == 4 ? CLR_YELLOW : CLR_BLUE_GREEN);  // decay
        line(x2, sus, x3, sus, sel == 5 ? CLR_YELLOW : CLR_BLUE_GREEN);  // sustain
        line(x3, sus, x4, gb, sel == 6 ? CLR_YELLOW : CLR_BLUE_GREEN);   // release
    }

    font(FONT_TINY);
    print("A..K play   1..5 presets   SPACE chord   7 sliders: arrows pick + turn, or drag", 10, SCREEN_H - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
