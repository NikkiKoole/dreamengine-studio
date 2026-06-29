/* de:meta
{
  "title": "pd",
  "status": "active",
  "created": "2026-06-05",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "subtractive-synth",
    "adsr-envelope"
  ],
  "lineage": "Sixth engine showcase, porting the Casio CZ phase-distortion oscillator from navkit's soundsystem — the only engine that omits the DCW sweep, which this cart rebuilds from scratch as the morph macro.",
  "description": "INSTR_PD showcase - the sixth modeled ENGINE: phase distortion, the Casio CZ sound. A cosine read through a WARPED phase ramp makes bright, buzzy, resonant tones with no filter at all (the warp IS the brightness) - rubbery basses, glassy resonant leads, and the famous CZ 'wowww' sweep. The three engine macros: instrument_harmonics = wavetype (snapped across 8 - saw / square / pulse / double-pulse / sawpulse, then 3 resonant types), instrument_timbre = distortion amount (brightness, or how high the resonant peak sits), instrument_morph = the DCW sweep (0 = static; turn it up and the distortion snaps bright on the strike then settles - the CZ envelope, which the navkit reference omits, so this is built from scratch). Like FM, the engine doesn't bake its amp envelope, so a CZ patch is macros plus an ADSR - both are sliders here. The scope draws the actual warped waveform; with the DCW sweep up it shows TWO traces - the bright strike (faint) settling to the steady tone. Five presets (1 cz bass, 2 reso lead, 3 synth brass, 4 sweep pad, 5 cz pluck), and the panel prints the exact instrument() call to copy into your cart. A S D F G H J K play a major scale, Z/X octave, 1..5 presets, drag any slider (or LEFT/RIGHT + UP/DOWN), SPACE chord, M autoplay on/off."
}
de:meta */
// pd — INSTR_PD showcase: phase distortion (the Casio CZ engine), three macros, ADSR, scope.
//
// The sixth modeled ENGINE: a cosine read through a WARPED phase ramp — bright, buzzy,
// resonant tones with no filter at all (the warp IS the brightness). The famous CZ sound:
// rubbery basses, glassy resonant leads, and the "wowww" sweep. Every engine answers the
// same three 0..1 macro knobs — the API never grows:
//   harmonics = WAVETYPE  (snapped across 8: saw/square/pulse/dbl/sawpulse + 3 resonant)
//   timbre    = DISTORTION (the warp/DCW amount — brightness, or how high the reso peak sits)
//   morph     = DCW SWEEP  (0 = static; up = distortion snaps bright on the strike then
//                           settles to the timbre value — the CZ envelope, which navkit omits)
//
// Like FM, this engine deliberately doesn't bake its amp envelope, so a CZ patch is macros
// + an ADSR — both live here as sliders. The scope draws the ACTUAL warped waveform; with
// the DCW sweep up it shows TWO traces — the bright strike (faint) settling to the steady
// tone (bright). Dial a sound, then copy the printed instrument() call into your cart.
//
// controls: A S D F G H J K  play (major scale)   ·   Z / X octave down / up
//           1..5 presets: cz bass / reso lead / synth brass / sweep pad / cz pluck
//           drag any slider (auditions as you drag), or
//           LEFT/RIGHT pick a slider + UP/DOWN turn it (7 sliders: 3 macros + ADSR)
//           SPACE chord   ·   M autoplay on/off
//
// MULTITOUCH: every finger is its own pointer — tap keys for chords, sweep across them for
// a run, hold a slider with another finger while the chord rings. Desktop mouse = one finger.

#include "studio.h"
#include "pointer.h"     // multi-finger pool: PTR_MAX/PTR_NONE + PTR_CLEAR/PTR_ACQUIRE/PTR_FIND
#include <math.h>

#define I_PD  5
#define NKEY  8

static const char STRKEY[NKEY] = { 'A','S','D','F','G','H','J','K' };
static const char *MNAME[3] = { "harmonics (wave)", "timbre (distort)", "morph (DCW sweep)" };
static const char *MLO[3]   = { "saw",  "clean",  "static" };
static const char *MHI[3]   = { "reso", "buzzy",  "wowww" };
static const char *ENAME[4] = { "att", "dec", "sus", "rel" };
// the 8 wavetypes, in the engine's order (sound.h sound_pd_sample) — display only
static const char *WNAME[8] = { "saw","square","pulse","dbl pulse","sawpulse","reso tri","reso trap","reso saw" };

// the presets: macro positions + an ADSR, both in slider space (0..1). harmonics is snapped
// to a wavetype: knob ~ (wt+0.5)/8 lands on detent wt. STARTING GUESSES — the acceptance
// tests for the engine; this rig exists to better them by ear.
typedef struct { const char *name; float h, t, m, ea, ed, es, er; } Preset;
static const Preset PRESET[5] = {
    { "cz bass",     0.06f, 0.62f, 0.40f, 0.00f, 0.40f, 0.45f, 0.20f },  // saw, punchy, light DCW snap
    { "reso lead",   0.94f, 0.55f, 0.45f, 0.02f, 0.32f, 0.62f, 0.30f },  // reso saw, sustained, bright peak
    { "synth brass", 0.56f, 0.70f, 0.50f, 0.42f, 0.30f, 0.80f, 0.32f },  // sawpulse, slow attack swell
    { "sweep pad",   0.69f, 0.38f, 0.95f, 0.50f, 0.62f, 0.70f, 0.70f },  // reso tri, full DCW "wowww"
    { "cz pluck",    0.31f, 0.72f, 0.80f, 0.00f, 0.30f, 0.00f, 0.22f },  // pulse, hard DCW snap, no sustain
};

#define NSLIDER 7   // 0..2 = macros, 3..6 = ADSR
static int   midi_of[NKEY];
static float glow[NKEY];
static float knob[NSLIDER];    // macro 0..2 + env 3..6, all 0..1
static int   sel = 0;
static bool  autoplay = true;

// per-finger pointer table — each finger independently drags a slider or plays/sweeps keys
enum { PTR_IDLE, PTR_DRAG, PTR_SWEEP };
typedef struct { int id, mode, k, prevX; } Ptr;   // id MUST be first (pointer.h)
static Ptr ptr[PTR_MAX];       // .id == PTR_NONE → slot free
static int chord_rx = -1;
static int cur_preset = 0;
static int apos = 0;
static int scope_age = 9999;
static int oct = 0;            // Z/X octave shift

static int km(int b) { return midi_of[b] + oct * 12; }

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

static int env_a(void) { return 1  + (int)(knob[3] * knob[3] * 300.0f); }
static int env_d(void) { return 50 + (int)(knob[4] * knob[4] * 1500.0f); }
static int env_s(void) { return (int)(knob[5] * 7.0f + 0.5f); }
static int env_r(void) { return 20 + (int)(knob[6] * knob[6] * 1500.0f); }

// the PD oscillator, mirrored from sound.h sound_pd_sample — for the scope. phase 0..1,
// wt = wavetype 0..7, d = distortion 0..1, fn = register 0..1 (reso cap, the STEP 0 finding).
static float pd_wave(float phase, int wt, float d, float fn) {
    if (d > 0.999f) d = 0.999f; else if (d < 0.0f) d = 0.0f;
    switch (wt) {
        case 0: {  // saw
            float dp;
            if (phase < 0.5f) dp = phase * (1.0f + d);
            else { float t = (phase - 0.5f) * 2.0f; dp = 0.5f * (1.0f + d) + t * (1.0f - 0.5f * (1.0f + d)); }
            if (dp < 0.0f) dp = 0.0f; else if (dp > 1.0f) dp = 1.0f;
            return cosf(dp * 3.14159265f);
        }
        case 1: {  // square
            float sh = 0.5f - d * 0.45f, dp;
            if      (phase < 0.25f) dp = phase / 0.25f * sh;
            else if (phase < 0.5f)  dp = sh + (phase - 0.25f) / 0.25f * (0.5f - sh);
            else if (phase < 0.75f) dp = 0.5f + (phase - 0.5f) / 0.25f * sh;
            else                    dp = 0.5f + sh + (phase - 0.75f) / 0.25f * (0.5f - sh);
            return cosf(dp * 6.2831853f);
        }
        case 2: {  // pulse
            float w = 0.5f - d * 0.45f, dp;
            if (phase < w) dp = phase / w * 0.5f; else dp = 0.5f + (phase - w) / (1.0f - w) * 0.5f;
            return cosf(dp * 6.2831853f);
        }
        case 3: {  // double pulse
            float dp = phase * 2.0f; if (dp >= 1.0f) dp -= 1.0f;
            float w = 0.5f - d * 0.4f;
            if (dp < w) dp = dp / w * 0.5f; else dp = 0.5f + (dp - w) / (1.0f - w) * 0.5f;
            return cosf(dp * 6.2831853f);
        }
        case 4: {  // sawpulse
            float dp1;
            if (phase < 0.5f) dp1 = phase * (1.0f + d * 0.5f);
            else              dp1 = 0.5f * (1.0f + d * 0.5f) + (phase - 0.5f) * (1.0f - d * 0.25f);
            if (dp1 < 0.0f) dp1 = 0.0f; else if (dp1 > 1.0f) dp1 = 1.0f;
            float saw = cosf(dp1 * 3.14159265f);
            float w = 0.5f - d * 0.3f, dp2;
            if (phase < w) dp2 = phase / w * 0.5f; else dp2 = 0.5f + (phase - w) / (1.0f - w) * 0.5f;
            return (saw + cosf(dp2 * 6.2831853f)) * 0.5f;
        }
        default: { // 5/6/7 resonant
            float resoFreq = 1.0f + d * 7.0f * (1.0f - fn * 0.7f);
            float window;
            if (wt == 5)      window = 1.0f - fabsf(2.0f * phase - 1.0f);
            else if (wt == 6) window = (phase < 0.25f) ? phase * 4.0f : (phase < 0.75f) ? 1.0f : (1.0f - phase) * 4.0f;
            else              window = 1.0f - phase;
            return window * cosf(phase * resoFreq * 6.2831853f);
        }
    }
}

static void apply_patch(void) {
    instrument(I_PD, INSTR_PD, env_a(), env_d(), env_s(), env_r());
    instrument_harmonics(I_PD, knob[0]);
    instrument_timbre(I_PD, knob[1]);
    instrument_morph(I_PD, knob[2]);
}

static void play_key(int b, int vol) {
    note(km(b), I_PD, vol);
    glow[b] = 1.0f;
    scope_age = 0;
}

static void play_chord(void) {
    chord(km(0), CHORD_MIN7, I_PD, 5);
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
    PTR_CLEAR(ptr);
    for (int b = 0; b < NKEY; b++) midi_of[b] = degree(SCALE_MAJOR, 4, b);
    set_preset(0);
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

    if (keyp(KEY_SPACE) || (chord_rx >= 0 && tapp(chord_rx - 2, SCREEN_H - 12, 52, 12))) play_chord();
    if (keyp('M')) autoplay = !autoplay;
    if ((keyp('Z') || tapp(154, 2, 14, 13)) && oct > -2) { oct--; play_key(2, 5); }
    if ((keyp('X') || tapp(212, 2, 14, 13)) && oct <  3) { oct++; play_key(2, 5); }

    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;                          // pool full (>PTR_MAX fingers)
        if (fresh) {
            *p = (Ptr){ id, PTR_IDLE, -1, tx };
            if (point_in_box(tx, ty, SCREEN_W - 92, 2, 88, 12)) { autoplay = !autoplay; continue; }
            if (ty >= KEY_Y + KEY_H + 2 && ty < KEY_Y + KEY_H + 16) {
                for (int q = 0; q < 5; q++)
                    if (tx >= 12 + q * 58 && tx < 12 + q * 58 + 56) set_preset(q);
                continue;
            }
            for (int k = 0; k < 3; k++)
                if (point_in_box(tx, ty, MROW_X(k) - 2, MROW_Y - 6, MROW_W + 4, 18))
                    { p->mode = PTR_DRAG; p->k = sel = k; }
            for (int k = 0; k < 4; k++)
                if (point_in_box(tx, ty, EROW_X(k) - 2, EROW_Y - 6, EROW_W + 4, 18))
                    { p->mode = PTR_DRAG; p->k = sel = 3 + k; }
            if (p->mode == PTR_IDLE && ty >= KEY_Y - 4 && ty < KEY_Y + KEY_H + 2) {
                for (int b = 0; b < NKEY; b++)
                    if (point_in_box(tx, ty, KEY_X(b), KEY_Y, KEY_W, KEY_H)) play_key(b, 6);
                p->mode = PTR_SWEEP; p->prevX = tx;
            }
        } else if (p->mode == PTR_DRAG) {
            int x0 = p->k < 3 ? MROW_X(p->k) : EROW_X(p->k - 3);
            int w  = p->k < 3 ? MROW_W : EROW_W;
            knob[p->k] = clamp((float)(tx - x0) / (float)w, 0.0f, 1.0f);
            cur_preset = -1;
            apply_patch();
            if (frame() % 12 == 0) play_key(2, 5);
        } else if (p->mode == PTR_SWEEP) {
            for (int b = 0; b < NKEY; b++) {
                int cx = KEY_X(b) + KEY_W / 2;
                if ((p->prevX < cx && tx >= cx) || (p->prevX > cx && tx <= cx)) play_key(b, 5);
            }
            p->prevX = tx;
        }
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (p) p->id = PTR_NONE;
    }

    // autoplay: a CZ noodle — a min7 stab on the downbeat, a line between
    if (autoplay && every(1)) {
        static const int seq[16] = { 0,2,4,2, 5,4,2,0, 4,5,7,5, 4,2,1,0 };
        if (beat() % 8 == 0) play_chord();
        else {
            play_key(seq[apos % 16] % NKEY, 4);
            if (chance(25)) schedule_hit(280, km((seq[apos % 16] + 2) % NKEY), I_PD, 3, 250);
        }
        apos++;
    }
    scope_age++;

#ifdef DE_TRACE
    watch("harm", "%.2f", knob[0]);
    watch("wt",   "%d",   (int)(knob[0] * 7.999f));
    watch("timb", "%.2f", knob[1]);
    watch("mor",  "%.2f", knob[2]);
    watch("preset", "%d", cur_preset);
#endif
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("PD", 8, 6, CLR_PINK);
    font(FONT_SMALL);
    print("phase distortion (casio cz)", 32, 8, CLR_MEDIUM_GREY);
    print_right(autoplay ? "M autoplay: on" : "M autoplay: off", SCREEN_W - 10, 8, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    rect(154, 2, 14, 13, CLR_DARK_GREY);  print("-", 159, 6, CLR_LIGHT_GREY);
    print(str("oct %+d", oct), 172, 8, oct ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    rect(212, 2, 14, 13, CLR_DARK_GREY);  print("+", 217, 6, CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    // the engine parameters, derived exactly like sound_pd_sample
    int   wt    = (int)(knob[0] * 7.999f); if (wt > 7) wt = 7;
    float d_sus = knob[1];
    // the DCW envelope at the current note age (1 → 0 over ~0.25s ≈ 15 frames)
    float dcw   = expf(-(float)scope_age / 15.0f);
    float d_now = d_sus + (1.0f - d_sus) * knob[2] * dcw;     // bright strike → settles
    float fn    = 0.0f;                                        // scope shows the low-register (uncapped) shape

    // the scope: the ACTUAL warped waveform, two cycles. With the DCW sweep up, draw the
    // bright STRIKE trace faint behind the steady (settled) trace bright.
    int sy = 50, sx0 = 10, sx1 = SCREEN_W - 10, prev_y = sy, prev_yp = sy;
    rectfill(sx0 - 2, 22, sx1 - sx0 + 4, 58, CLR_DARK_BLUE);
    line(sx0, sy, sx1, sy, CLR_DARKER_GREY);
    bool show_strike = (knob[2] > 0.02f);
    float d_strike = d_sus + (1.0f - d_sus) * knob[2];        // the strike endpoint (settled is d_sus)
    for (int x = sx0; x <= sx1; x++) {
        float ph = (float)(x - sx0) / (float)(sx1 - sx0) * 2.0f;
        float frac = ph - (float)(int)ph;                     // 0..1 within each of the 2 cycles
        if (show_strike) {
            float sp = pd_wave(frac, wt, d_strike, fn);
            int yp = sy - (int)(sp * 22.0f);
            if (x > sx0) line(x - 1, prev_yp, x, yp, CLR_DARK_PURPLE);
            prev_yp = yp;
        }
        float s = pd_wave(frac, wt, d_sus, fn);
        int y = sy - (int)(s * 22.0f);
        if (x > sx0) line(x - 1, prev_y, x, y, CLR_PINK);
        prev_y = y;
    }
    font(FONT_TINY);
    print(str("wave: %s    distortion %.2f%s", WNAME[wt], d_sus,
              show_strike ? "   (+DCW sweep)" : ""),
          sx0, 24, CLR_MEDIUM_GREY);
    (void)d_now;
    print(str("instrument(I, INSTR_PD, %d, %d, %d, %d)  h %.2f t %.2f m %.2f",
              env_a(), env_d(), env_s(), env_r(), knob[0], knob[1], knob[2]),
          sx0, 72, CLR_BLUE_GREEN);
    font(FONT_NORMAL);

    // the keys
    for (int b = 0; b < NKEY; b++) {
        int x = KEY_X(b);
        glow[b] *= 0.90f;
        int col = glow[b] > 0.5f ? CLR_LIGHT_YELLOW : glow[b] > 0.1f ? CLR_PINK : CLR_DARK_BLUE;
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
        // harmonics: show the wavetype name instead of lo/hi (it's a snapped detent)
        if (k == 0) print(WNAME[wt], x, y + 9, CLR_DARK_GREY);
        else        print(MLO[k], x, y + 9, CLR_DARK_GREY);
        if (k != 0) print_right(MHI[k], x + MROW_W, y + 9, CLR_DARK_GREY);
        else        print_right("reso", x + MROW_W, y + 9, CLR_DARK_GREY);
        font(FONT_NORMAL);
        if (on) print(">", x - 9, y, CLR_YELLOW);
    }

    // envelope row
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

    // the ADSR shape (segment widths true to ms; selected segment lights up)
    {
        int gx = 264, gw = SCREEN_W - 10 - 264, gh = 20, gb = EROW_Y + 7;
        rectfill(gx - 2, gb - gh - 2, gw + 4, gh + 4, CLR_DARK_BLUE);
        float a = (float)env_a(), d = (float)env_d(), r = (float)env_r();
        float sl = (float)env_s() / 7.0f;
        float hold = 250.0f;
        float tot  = a + d + hold + r;
        int x1 = gx + (int)(gw * a / tot);
        int x2 = x1 + (int)(gw * d / tot);
        int x3 = x2 + (int)(gw * hold / tot);
        int x4 = gx + gw;
        int top = gb - gh, sus = gb - (int)(sl * gh);
        line(gx, gb, x1, top, sel == 3 ? CLR_YELLOW : CLR_BLUE_GREEN);
        line(x1, top, x2, sus, sel == 4 ? CLR_YELLOW : CLR_BLUE_GREEN);
        line(x2, sus, x3, sus, sel == 5 ? CLR_YELLOW : CLR_BLUE_GREEN);
        line(x3, sus, x4, gb, sel == 6 ? CLR_YELLOW : CLR_BLUE_GREEN);
    }

    font(FONT_TINY);
    chord_rx = print("A..K play   Z/X octave   1..5 presets   ", 10, SCREEN_H - 8, CLR_DARK_GREY);
    int after_chord = print("SPACE chord", chord_rx, SCREEN_H - 8, CLR_MEDIUM_GREY);
    print("   sliders: drag or arrows", after_chord, SCREEN_H - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
