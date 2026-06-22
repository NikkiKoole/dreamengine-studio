#include "studio.h"
#include "ui.h"
#include <math.h>

// DUBSIREN — a dub siren, after King Tubby / Scientist and a sound-system mixing desk.
//
// Not a melodic instrument — a GESTURE box you play OVER a riddim. Two detuned oscillators
// warble (an LFO bends their pitch — the "wheeeoo" siren), and they fire into a huge
// FEEDBACK DELAY: stab the siren, lift your hand, and the echo THROWS the stab tumbling
// into the distance. Crank the throw past the red and the delay self-oscillates and howls.
// A big reverb splash sits under it all.
//
//   • the FIRE PAD — hold to sound the siren (or SPACE / LATCH).  Y = PITCH, X = THROW
//     (feedback — drag right past the red and it self-oscillates / howls).
//   • WOBBLE rate · DEPTH (semitones) · ECHO time (sweep it while it rings → the repeats
//     pitch-bend) · VERB splash.  1-4 pick the siren shape (whoop / two-tone / ramp / zigzag).
//   • ↑/↓ pitch · ←/→ throw · L latch · SPACE = fire.
//
//   Built on echo() feedback + note_lfo(LFO_PITCH) + reverb() — no new engine DSP. The
//   hands-on twin of the `dub` auto-radio; a liveset plaything (cart-library-direction §2c).

enum { SL_A = 5, SL_B };
static int hA = -1, hB = -1;

static bool  latch = false;
static float px = 0.55f, py = 0.5f;   // pad: x = throw(feedback), y = pitch
static int   mode = 0;                // siren LFO shape

static float k_rate  = 0.38f;         // → wobble 0.5..20 Hz
static float k_depth = 0.42f;         // → 0..12 semitones
static float k_echo  = 0.45f;         // → 60..600 ms delay
static float k_verb  = 0.5f;          // → reverb size

static float beacon = 0.0f;           // spinning-light angle
static int   fireflash = -100;

static const int   SHAPE[4]  = { LFO_SHAPE_SINE, LFO_SHAPE_SQUARE, LFO_SHAPE_SAW, LFO_SHAPE_TRI };
static const char *SNAME[4]  = { "WHOOP", "TWO-TONE", "RAMP", "ZIGZAG" };

#define PAD_X 8
#define PAD_Y 40
#define PAD_W 184
#define PAD_H 152

static float pitch_of(void)  { return 42.0f + py * 42.0f; }   // MIDI 42..84 (3.5 oct)
static float fb_of(void)     { return px * 1.1f; }            // 0..1.1 — past 1.0 = self-osc
static float rate_of(void)   { return 0.5f + k_rate * 19.5f; }
static float depth_of(void)  { return k_depth * 12.0f; }
static int   echo_of(void)   { return 60 + (int)(k_echo * 540); }

// the wobble LFO — set on change (rate/depth/shape)
static void apply_wobble(void) {
    static float ar = -1, ad = -1; static int am = -1;
    float r = rate_of(), d = depth_of();
    if (r == ar && d == ad && mode == am) return;
    note_lfo(hA, 0, LFO_PITCH, r, d); note_lfo_shape(hA, 0, SHAPE[mode]);
    note_lfo(hB, 0, LFO_PITCH, r, d); note_lfo_shape(hB, 0, SHAPE[mode]);
    ar = r; ad = d; am = mode;
}
// the delay bus — ride only while time/feedback actually move (set-and-hold)
static void apply_echo(void) {
    static int at = -1; static float af = -1;
    int t = echo_of(); float f = fb_of();
    if (t == at && f == af) return;
    echo(t, f, 0.55f); at = t; af = f;
}
static void apply_verb(void) {
    static float av = -1;
    if (k_verb == av) return;
    reverb(0.2f + k_verb * 0.8f, 0.45f); av = k_verb;
}

void init(void) {
    instrument(SL_A, INSTR_SQUARE, 1, 0, 9, 220);   // sustaining siren tone
    instrument(SL_B, INSTR_SAW,    1, 0, 9, 220);   // detuned body
    instrument_drive(SL_A, 0.12f);
    instrument_echo(SL_A, 0.7f);  instrument_echo(SL_B, 0.6f);    // dub throw send — set BEFORE note_on
    instrument_reverb(SL_A, 0.4f); instrument_reverb(SL_B, 0.35f); //   (sends are sampled at note_on time)
    hA = note_on(60, SL_A, 0); hB = note_on(60, SL_B, 0);   // alive but silent
    note_glide(hA, 35); note_glide(hB, 35);
    apply_wobble(); apply_echo(); apply_verb();
}

void update(void) {
    for (int i = 0; i < 4; i++) if (keyp('1' + i)) mode = i;
    if (keyp('L')) latch = !latch;
    if (btnp(0, BTN_UP))    py = clamp(py + 0.05f, 0, 1);
    if (btnp(0, BTN_DOWN))  py = clamp(py - 0.05f, 0, 1);
    if (btnp(0, BTN_LEFT))  px = clamp(px - 0.05f, 0, 1);
    if (btnp(0, BTN_RIGHT)) px = clamp(px + 0.05f, 0, 1);

    // FIRE PAD: a finger sets pitch(Y) + throw(X); touching or SPACE or LATCH = firing
    bool touching = false;
    for (int i = 0; i < touch_count(); i++) {
        int tx = touch_x(i), ty = touch_y(i);
        if (tx >= PAD_X && tx < PAD_X + PAD_W && ty >= PAD_Y && ty < PAD_Y + PAD_H) {
            touching = true;
            px = clamp((tx - PAD_X) / (float)PAD_W, 0, 1);
            py = clamp(1.0f - (ty - PAD_Y) / (float)PAD_H, 0, 1);
        }
    }
    bool fire = touching || latch || key(KEY_SPACE);

    static bool wasfire = false;
    if (fire) { note_pitch(hA, pitch_of()); note_pitch(hB, pitch_of() + 0.09f); fireflash = frame(); }
    if (fire != wasfire) { note_vol(hA, fire ? 5.0f : 0.0f); note_vol(hB, fire ? 3.6f : 0.0f); wasfire = fire; }

    apply_wobble(); apply_echo(); apply_verb();
    beacon += rate_of() * 0.12f;

#ifdef DE_TRACE
    watch("fire",  "%d", fire);
    watch("pitch", "%.1f", pitch_of());
    watch("fb",    "%.2f", fb_of());
    watch("mode",  "%s", SNAME[mode]);
#endif
}

static const char *NN[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    bool firing = (frame() - fireflash) < 4;
    float fb = fb_of();

    print("DUBSIREN", 2, 4, CLR_LIGHT_YELLOW);
    print(SNAME[mode], 86, 4, CLR_ORANGE);
    int n = (int)(pitch_of() + 0.5f);
    print_right(str("%s%d", NN[n % 12], n / 12 - 1), 318, 4, CLR_LIGHT_GREY);

    // spinning beacon light (spins at the wobble rate, glows when firing)
    int bcx = 168, bcy = 16;
    int bcol = firing ? CLR_RED : CLR_DARK_RED;
    circfill(bcx, bcy, 6, firing ? CLR_RED : CLR_DARKER_GREY);
    float ba = beacon;
    line(bcx, bcy, bcx + (int)(cosf(ba) * 9), bcy + (int)(sinf(ba) * 9), bcol);
    line(bcx, bcy, bcx - (int)(cosf(ba) * 9), bcy - (int)(sinf(ba) * 9), bcol);

    // ── the FIRE PAD (Y = pitch, X = throw) ──
    rectfill(PAD_X, PAD_Y, PAD_W, PAD_H, firing ? CLR_DARKER_BLUE : CLR_DARKER_GREY);
    for (int g = 1; g < 6; g++) {
        line(PAD_X + g * PAD_W / 6, PAD_Y, PAD_X + g * PAD_W / 6, PAD_Y + PAD_H, CLR_DARK_GREY);
        line(PAD_X, PAD_Y + g * PAD_H / 6, PAD_X + PAD_W, PAD_Y + g * PAD_H / 6, CLR_DARK_GREY);
    }
    // the self-oscillation danger zone (X past the red)
    int redx = PAD_X + (int)(0.909f * PAD_W);   // px where fb crosses 1.0
    rectfill(redx, PAD_Y, PAD_X + PAD_W - redx, PAD_H, CLR_DARK_RED);
    rect(PAD_X, PAD_Y, PAD_W, PAD_H, firing ? CLR_WHITE : CLR_MEDIUM_GREY);
    int dx = PAD_X + (int)(px * PAD_W), dy = PAD_Y + (int)((1 - py) * PAD_H);
    int cc = (fb > 1.0f) ? CLR_RED : CLR_BLUE;
    if (firing) {
        line(PAD_X, dy, PAD_X + PAD_W, dy, cc);
        line(dx, PAD_Y, dx, PAD_Y + PAD_H, cc);
        circfill(dx, dy, 6, cc); circ(dx, dy, 6, CLR_WHITE);
    } else circ(dx, dy, 5, CLR_DARK_GREY);
    font(FONT_SMALL);
    print(firing ? "FIRE" : (latch ? "LATCH" : "hold to fire"), PAD_X + 3, PAD_Y + 2, firing ? CLR_WHITE : CLR_MEDIUM_GREY);
    print("pitch ^", PAD_X + 3, PAD_Y + PAD_H - 8, CLR_DARK_GREY);
    print_right("throw >", PAD_X + PAD_W - 2, PAD_Y + PAD_H - 8, fb > 1 ? CLR_RED : CLR_DARK_GREY);
    if (fb > 1.0f) print("HOWL!", redx - 2, PAD_Y - 9, (frame() & 8) ? CLR_RED : CLR_ORANGE);
    font(FONT_NORMAL);

    // ── knobs ──
    ui_begin();
    font(FONT_SMALL);
    ui_knob(&k_rate,  222, 58,  "WOBBLE");
    ui_knob(&k_depth, 280, 58,  "DEPTH");
    ui_knob(&k_echo,  222, 112, "ECHO");
    ui_knob(&k_verb,  280, 112, "VERB");
    // siren-shape selector + latch
    for (int i = 0; i < 4; i++)
        if (ui_button(204 + (i % 2) * 58, 150 + (i / 2) * 14, 56, 13, SNAME[i])) mode = i;
    if (ui_button(204, 178, 114, 14, latch ? "LATCH ON" : "LATCH")) latch = !latch;
    print("hold pad / SPACE = fire   L latch", PAD_X, 31, CLR_DARK_GREY);
    font(FONT_NORMAL);
    ui_end();
}
