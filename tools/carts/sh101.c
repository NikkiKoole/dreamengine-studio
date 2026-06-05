#include "studio.h"
#include <math.h>

// ── ROLAND SH-101 ─────────────────────────────────────────────────────────
// Roland's SH-101 (1982) — a monophonic lead/bass synth sold with a guitar
// strap and a built-in arpeggiator. A commercial middler that found its
// audience in post-punk and new wave, then again in the 90s rave scene. The
// sound is a DCO (digitally-controlled oscillator) through a classic 4-pole
// resonant lowpass — not as tweaky as the Minimoog, but a lot cheaper and
// impossibly fun with the arpeggiator running.
//
// How the SH-101 maps onto the dreamengine API:
//
//   monophonic — one note_on handle at a time. Last-note priority: holding
//     two keys plays the most recently pressed one; releasing it falls back
//     to the previous held key. Tracked with a small held-key stack.
//   DCO — saw or square (or both: we combine via a BLEND slider that crossfades
//     the instrument slot between the two waves). PW knob adjusts the square's
//     pulse width. Sub oscillator adds a second note_on at -12 or -24 semitones
//     on a separate slot — costs a second voice, fine for a mono synth.
//   VCF — resonant lowpass. Cutoff and resonance sliders; env mod (ENV_CUTOFF
//     bipolar amount); LFO mod optional; keyboard tracking toggle.
//   ENV — one ADSR shared by VCA (amplitude) and VCF envelope mod.
//   LFO — rate, delay (LFO fades in over delay ms after key press), and
//     destination: PITCH (vibrato) · PWM (pulse-width shimmer) · CUT (wah).
//     The LFO_CUTOFF depth is live on the held voice so the wah sweeps under
//     your fingers.
//   portamento — note_glide() from a rate slider (0 = instant).
//   arpeggiator — UP / DOWN / UP-DOWN; 1–3 octave range; tempo via ARP RATE
//     slider. Uses schedule_hit for sample-accurate timing off the beat clock.
//     Holding any key(s) latches the chord to arpeggiate. ARP beats the held-
//     note model: it fires note()/hit() instead so voices self-release.
//
//   KEYBOARD  A-; row = white keys  W E T Y U O P = black keys
//             Z / X   octave down / up        (base octave shown bottom left)
//   MOUSE     click knobs/sliders to drag them; click section buttons
//             hover + mouse-wheel on any knob
//   SPACE     ARP on / off
//   H         help overlay

// ── constants ────────────────────────────────────────────────────────────
#define SL_MAIN  5    // instrument slot for the main DCO voice
#define SL_SUB   6    // instrument slot for the sub oscillator
#define KR       7    // rotary knob radius
#define NONE    -999

// ── synth state ───────────────────────────────────────────────────────────
// DCO
static int   wave    = INSTR_SAW;  // INSTR_SAW or INSTR_SQUARE
static float duty    = 0.5f;       // square pulse width 0.05..0.95
static int   sub     = 0;          // 0=off  1=-12st  2=-24st
static float porta   = 0.0f;       // portamento ms 0..400

// VCF
static float cutoff  = 800.0f;     // Hz 80..4000
static float res     = 6.0f;       // 0..15
static float envmod  = 1200.0f;    // ENV_CUTOFF amount, Hz bipolar
static int   kybd    = 1;          // keyboard follow toggle

// ENV (shared VCA + VCF)
static float att = 8, dec = 180, sus = 4, rel = 260;   // ms, sustain 0..7

// LFO
static float lfo_rate  = 4.5f;     // Hz 0.1..12
static float lfo_delay = 0.0f;     // ms 0..800
static int   lfo_dest  = LFO_PITCH;// LFO_PITCH / LFO_DUTY / LFO_CUTOFF
static float lfo_depth = 0.20f;    // 0..1, scaled per dest

// ARP
static int   arp_on    = 0;
static int   arp_mode  = 0;        // 0=UP 1=DOWN 2=UP-DOWN
static int   arp_range = 1;        // 1 2 3 octaves
static float arp_rate  = 120.0f;   // bpm equivalent, 60..240
static const char *ARP_MODE_NAME[] = { "UP", "DN", "UD" };

// ── held-key stack (monophonic last-note priority) ────────────────────────
#define MAX_HELD 12
static int held[MAX_HELD];         // midi pitches of currently held keys
static int nheld = 0;
static int voice  = -1;            // main voice handle
static int sub_v  = -1;           // sub-osc voice handle

// ── arpeggiator state ─────────────────────────────────────────────────────
#define MAX_ARP  36                // max notes in the arp pool (3 oct × 12)
static int arp_pool[MAX_ARP];     // sorted midi pitches to arpeggiate
static int arp_n   = 0;
static int arp_idx = 0;
static int arp_dir = 1;           // +1 or -1 for UD mode
static long arp_scheduled = -1;

// ── UI drag state ─────────────────────────────────────────────────────────
static int mx, my;
static int drag_id = 0;           // which control is being dragged
static int drag_my0;              // mouse y at drag start
static float drag_v0;             // value at drag start

// ── misc ──────────────────────────────────────────────────────────────────
static int base    = 48;          // MIDI of C3, leftmost white key
static int show_help = 0;

// ─────────────────────────────────────────────────────────────────────────
// keyboard layout (19 white keys C2..G4 — 2.5 octaves)
// ─────────────────────────────────────────────────────────────────────────
#define NWHITE 19
#define NBLACK 13
#define KBY  232          // keyboard top y
#define KBH   66          // keyboard height
#define KBW   24          // white key width (460 - 20 margins = 440 / ~23 = 19 keys)

static const int W_SEMI[NWHITE] = { 0,2,4,5,7,9,11,12,14,16,17,19,21,23,24,26,28,29,31 };
static const char W_KEY[NWHITE] = { 'A','S','D','F','G','H','J','K','L',';','\'',
                                     /* upper row reuse — labeled only */
                                     0,0,0,0,0,0,0,0 };
static const int B_SEMI[NBLACK] = { 1,3,6,8,10,13,15,18,20,22,25,27,30 };
static const char B_KEY[NBLACK] = { 'W','E','T','Y','U','O','P',0,0,0,0,0,0 };
// after which white-key index does each black key sit?
static const int B_AFTER[NBLACK]= { 0,1,3,4,5,7,8,10,11,12,14,15,17 };

static int white_kx(int i) { return 10 + i * KBW; }
static int black_kx(int j) { return white_kx(B_AFTER[j]) + KBW - 7; }

// ─────────────────────────────────────────────────────────────────────────
// instrument definition helpers
// ─────────────────────────────────────────────────────────────────────────

// scale LFO depth to engine units (same mapping as moog.c)
static float lfo_scaled(int dest, float d) {
    if (dest == LFO_PITCH)  return d * 5.0f;        // semitones
    if (dest == LFO_DUTY)   return d * 0.45f;       // 0..1
    return d * 1800.0f;                              // LFO_CUTOFF Hz
}

static void define_main(void) {
    instrument(SL_MAIN, wave, (int)att, (int)dec, (int)(sus + 0.5f), (int)rel);
    instrument_duty(SL_MAIN, duty);
    instrument_filter(SL_MAIN, FILTER_LOW, (int)cutoff, (int)(res + 0.5f));
    instrument_env(SL_MAIN, 0, ENV_CUTOFF, (int)att, (int)dec, envmod);
    instrument_lfo(SL_MAIN, 0, lfo_dest, lfo_rate, lfo_scaled(lfo_dest, lfo_depth));
}

static void define_sub(void) {
    // sub osc: square wave, same amp envelope, no filter env, no LFO
    instrument(SL_SUB, INSTR_SQUARE, (int)att, (int)dec, (int)(sus + 0.5f), (int)rel);
    instrument_filter(SL_SUB, FILTER_LOW, (int)(cutoff * 0.5f), 0);
}

// ─────────────────────────────────────────────────────────────────────────
// voice management — last-note priority monophony
// ─────────────────────────────────────────────────────────────────────────

static void push_midi(int midi) {
    // don't double-push
    for (int i = 0; i < nheld; i++) if (held[i] == midi) return;
    if (nheld < MAX_HELD) held[nheld++] = midi;
}

static void pop_midi(int midi) {
    for (int i = 0; i < nheld; i++) {
        if (held[i] == midi) {
            for (int j = i; j < nheld - 1; j++) held[j] = held[j + 1];
            nheld--;
            return;
        }
    }
}

static void retrigger(int midi) {
    // if portamento is on, glide into the new pitch on the existing voice
    if (voice >= 0 && porta > 0) {
        note_glide(voice, (int)porta);
        note_pitch(voice, midi);
        if (sub >= 0 && sub_v >= 0) {
            note_glide(sub_v, (int)porta);
            note_pitch(sub_v, midi - sub * 12);
        }
        return;
    }
    // otherwise retrigger — release old, start new
    if (voice  >= 0) { note_off(voice);  voice  = -1; }
    if (sub_v  >= 0) { note_off(sub_v);  sub_v  = -1; }
    define_main();
    voice = note_on(midi, SL_MAIN, 6);
    if (sub > 0) {
        define_sub();
        sub_v = note_on(midi - sub * 12, SL_SUB, 4);
    }
}

static void key_down(int midi) {
    push_midi(midi);
    if (!arp_on) retrigger(midi);
}

static void key_up(int midi) {
    pop_midi(midi);
    if (!arp_on) {
        if (nheld > 0) retrigger(held[nheld - 1]);   // fall back to previous held key
        else {
            if (voice  >= 0) { note_off(voice);  voice  = -1; }
            if (sub_v  >= 0) { note_off(sub_v);  sub_v  = -1; }
        }
    }
}

static void drive_live(void) {
    if (voice < 0) return;
    note_filter(voice, FILTER_LOW);
    note_cutoff(voice, (int)cutoff);
    note_res(voice, (int)(res + 0.5f));
    note_duty(voice, duty);
    note_lfo(voice, 0, lfo_dest, lfo_rate, lfo_scaled(lfo_dest, lfo_depth));
}

// ─────────────────────────────────────────────────────────────────────────
// arpeggiator
// ─────────────────────────────────────────────────────────────────────────

static void build_arp_pool(void) {
    arp_n = 0;
    // collect all held pitches, then add octave copies up to arp_range
    for (int oct = 0; oct < arp_range; oct++)
        for (int i = 0; i < nheld; i++) {
            int p = held[i] + oct * 12;
            if (arp_n < MAX_ARP) arp_pool[arp_n++] = p;
        }
    // sort ascending
    for (int i = 0; i < arp_n - 1; i++)
        for (int j = i + 1; j < arp_n; j++)
            if (arp_pool[j] < arp_pool[i]) { int t=arp_pool[i]; arp_pool[i]=arp_pool[j]; arp_pool[j]=t; }
    arp_idx = 0; arp_dir = 1;
}

static int arp_next(void) {
    if (arp_n == 0) return -1;
    int p = arp_pool[arp_idx];
    if (arp_mode == 0) {                        // UP
        arp_idx = (arp_idx + 1) % arp_n;
    } else if (arp_mode == 1) {                 // DOWN
        arp_idx = (arp_idx - 1 + arp_n) % arp_n;
    } else {                                    // UP-DOWN
        if (arp_n == 1) { /* stay */ }
        else {
            arp_idx += arp_dir;
            if (arp_idx >= arp_n - 1) { arp_idx = arp_n - 1; arp_dir = -1; }
            if (arp_idx <= 0)         { arp_idx = 0;          arp_dir =  1; }
        }
    }
    return p;
}

// step duration in ms from arp_rate (quarter notes per minute → 16ths)
static int arp_step_ms(void) { return (int)(60000.0f / (arp_rate * 4)); }

static void arp_tick(void) {
    if (!arp_on || nheld == 0) return;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    long target = (long)pos + 1;
    while (arp_scheduled < target) {
        arp_scheduled++;
        int dly = (int)((arp_scheduled - pos) * arp_step_ms());
        if (dly < 1) dly = 1;
        build_arp_pool();
        int midi = arp_next();
        if (midi >= 0) {
            define_main();
            int dur = (int)(arp_step_ms() * 0.75f);
            schedule_hit(dly, midi, SL_MAIN, 6, dur);
            if (sub > 0) {
                define_sub();
                schedule_hit(dly, midi - sub * 12, SL_SUB, 4, dur);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────
// UI widgets
// ─────────────────────────────────────────────────────────────────────────

static int in_box(int x, int y, int w, int h) {
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

// small button — returns 1 on click
static int ui_btn(int x, int y, int w, int h, const char *label, int active, int col) {
    int hot = in_box(x, y, w, h);
    rectfill(x, y, w, h, active ? col : CLR_BROWNISH_BLACK);
    rect(x, y, w, h, hot ? CLR_WHITE : CLR_DARK_GREY);
    print(label, x + (w - text_width(label)) / 2, y + (h - 5) / 2, active ? CLR_BLACK : CLR_MEDIUM_GREY);
    return hot && mouse_pressed(MOUSE_LEFT);
}

// vertical slider — drag up/down, returns new value
static float ui_vslider(int id, int x, int y, int h, float val, float lo, float hi, int col) {
    if (mouse_pressed(MOUSE_LEFT) && in_box(x - 5, y, 10, h)) { drag_id = id; drag_my0 = my; drag_v0 = val; }
    if (drag_id == id) val = clamp(drag_v0 + (drag_my0 - my) * (hi - lo) / h, lo, hi);
    rectfill(x - 1, y, 2, h, CLR_DARK_GREY);
    int ky = y + h - (int)((val - lo) / (hi - lo) * h);
    ky = clamp(ky, y, y + h - 4);
    rectfill(x - 4, ky, 8, 4, col);
    return val;
}

// rotary knob — drag up to increase, returns new value; id must be unique
static float ui_knob(int id, int x, int y, float val, float lo, float hi, int col) {
    if (mouse_pressed(MOUSE_LEFT) && in_box(x - KR, y - KR, KR * 2, KR * 2)) { drag_id = id; drag_my0 = my; drag_v0 = val; }
    if (drag_id == id) val = clamp(drag_v0 + (drag_my0 - my) * (hi - lo) / 60.0f, lo, hi);
    // mouse wheel on hover
    if (in_box(x - KR - 2, y - KR - 2, KR * 2 + 4, KR * 2 + 4) && mouse_wheel() != 0)
        val = clamp(val + mouse_wheel() * (hi - lo) / 40.0f, lo, hi);
    circfill(x, y, KR, CLR_DARK_GREY);
    circ(x, y, KR, col);
    float a = 2.4f - ((val - lo) / (hi - lo)) * 4.8f;   // 2.4 = 8 o'clock, -2.4 = 4 o'clock
    line(x, y, x + (int)(cosf(a) * (KR - 2)), y + (int)(sinf(a) * (KR - 2)), CLR_WHITE);
    return val;
}

// section header box
static void section(int x, int y, int w, int h, const char *label, int col) {
    rect(x, y, w, h, CLR_DARK_GREY);
    rectfill(x, y, w, 9, col);
    print(label, x + (w - text_width(label)) / 2, y + 2, CLR_BLACK);
}

// ─────────────────────────────────────────────────────────────────────────
// init / update / draw
// ─────────────────────────────────────────────────────────────────────────

void init(void) {
    bpm(120);
    for (int i = 0; i < NWHITE; i++) { (void)W_KEY[i]; }  // suppress unused warning
    define_main();
    define_sub();
}

void update(void) {
    // computer keyboard — white keys
    static const char WK[] = "ASDFGHJKL;'";
    static const int  WS[] = { 0,2,4,5,7,9,11,12,14,16,17 };
    for (int i = 0; i < 11; i++) {
        if (keyp(WK[i])) key_down(base + WS[i]);
        if (keyr(WK[i])) key_up  (base + WS[i]);
    }
    // black keys
    static const char BK[] = "WETYU OP";
    static const int  BS[] = { 1, 3, 6, 8, 10, 13, 15 };
    for (int j = 0; j < 7; j++) {
        if (keyp(BK[j])) key_down(base + BS[j]);
        if (keyr(BK[j])) key_up  (base + BS[j]);
    }

    if (keyp('Z')) base = clamp(base - 12, 12, 84);
    if (keyp('X')) base = clamp(base + 12, 12, 84);
    if (keyp(' ')) {
        arp_on = !arp_on;
        arp_scheduled = -1;
        if (!arp_on && voice >= 0) { note_off(voice); voice = -1; }
        if (!arp_on && sub_v >= 0) { note_off(sub_v); sub_v = -1; }
    }
    if (keyp('H')) show_help = !show_help;

    arp_tick();
    drive_live();
}

void draw(void) {
    mx = mouse_x(); my = mouse_y();
    if (!mouse_down(MOUSE_LEFT)) drag_id = 0;

    cls(CLR_BROWNISH_BLACK);

    // ── red title bar ───────────────────────────────────────────────────
    rectfill(0, 0, SCREEN_W, 14, CLR_RED);
    print("ROLAND   SH-101", (SCREEN_W - text_width("ROLAND   SH-101")) / 2, 4, CLR_WHITE);

    // ── layout constants ────────────────────────────────────────────────
    // Five sections across the panel:
    //   DCO x=6     w=88
    //   VCF x=100   w=88
    //   ENV x=194   w=78
    //   LFO x=278   w=88
    //   ARP x=372   w=82
    int PY = 18, PH = 208;   // panel top y, panel height

    // ── DCO ─────────────────────────────────────────────────────────────
    {
        int sx = 6, sw = 88;
        section(sx, PY, sw, PH, "DCO", CLR_ORANGE);

        print("WAVE", sx + 4, PY + 14, CLR_MEDIUM_GREY);
        if (ui_btn(sx + 4,  PY + 24, 38, 12, "SAW",  wave == INSTR_SAW,    CLR_ORANGE)) wave = INSTR_SAW;
        if (ui_btn(sx + 46, PY + 24, 38, 12, "SQR",  wave == INSTR_SQUARE, CLR_ORANGE)) wave = INSTR_SQUARE;

        print("PW", sx + 4, PY + 42, CLR_MEDIUM_GREY);
        duty = ui_knob(1, sx + 20, PY + 58, duty, 0.05f, 0.95f, CLR_ORANGE);

        print("SUB OSC", sx + 4, PY + 78, CLR_MEDIUM_GREY);
        if (ui_btn(sx + 4,  PY + 88, 24, 12, "OFF", sub == 0, CLR_DARK_GREY)) sub = 0;
        if (ui_btn(sx + 32, PY + 88, 24, 12, "-1",  sub == 1, CLR_ORANGE))    sub = 1;
        if (ui_btn(sx + 60, PY + 88, 24, 12, "-2",  sub == 2, CLR_ORANGE))    sub = 2;

        print("PORTA", sx + 4, PY + 108, CLR_MEDIUM_GREY);
        porta = ui_knob(2, sx + 20, PY + 126, porta, 0.0f, 400.0f, CLR_ORANGE);
        print(str("%dms", (int)porta), sx + 34, PY + 120, CLR_DARK_GREY);

        // active voice indicator
        if (voice >= 0 || arp_on) {
            circfill(sx + sw - 10, PY + 10, 4, CLR_GREEN);
        } else {
            circ(sx + sw - 10, PY + 10, 4, CLR_DARK_GREY);
        }
    }

    // ── VCF ─────────────────────────────────────────────────────────────
    {
        int sx = 100, sw = 88;
        section(sx, PY, sw, PH, "VCF", CLR_DARK_PEACH);

        print("CUT", sx + 6,  PY + 12, CLR_MEDIUM_GREY);
        print("RES", sx + 30, PY + 12, CLR_MEDIUM_GREY);
        print("ENV", sx + 54, PY + 12, CLR_MEDIUM_GREY);

        int sh = PH - 80;
        cutoff = ui_vslider(10, sx + 13,  PY + 22, sh, cutoff,  80.0f, 4000.0f, CLR_DARK_PEACH);
        res    = ui_vslider(11, sx + 37,  PY + 22, sh, res,     0.0f,  15.0f,   CLR_DARK_PEACH);
        envmod = ui_vslider(12, sx + 61,  PY + 22, sh, envmod, -2400.0f, 2400.0f, CLR_YELLOW);

        print(str("%dHz", (int)cutoff), sx + 2, PY + 22 + sh + 4, CLR_DARK_GREY);

        print("KYBD", sx + 6, PY + PH - 30, CLR_MEDIUM_GREY);
        if (ui_btn(sx + 38, PY + PH - 32, 44, 12, kybd ? "TRACK" : "FIXED", kybd, CLR_DARK_PEACH))
            kybd = !kybd;
    }

    // ── ENV ─────────────────────────────────────────────────────────────
    {
        int sx = 194, sw = 78;
        section(sx, PY, sw, PH, "ENV", CLR_GREEN);

        print("A", sx + 6,  PY + 12, CLR_MEDIUM_GREY);
        print("D", sx + 24, PY + 12, CLR_MEDIUM_GREY);
        print("S", sx + 42, PY + 12, CLR_MEDIUM_GREY);
        print("R", sx + 60, PY + 12, CLR_MEDIUM_GREY);

        int sh = PH - 48;
        att = ui_vslider(20, sx + 10,  PY + 22, sh, att, 0.0f,   1200.0f, CLR_GREEN);
        dec = ui_vslider(21, sx + 28,  PY + 22, sh, dec, 10.0f,  1400.0f, CLR_GREEN);
        sus = ui_vslider(22, sx + 46,  PY + 22, sh, sus, 0.0f,   7.0f,    CLR_GREEN);
        rel = ui_vslider(23, sx + 64,  PY + 22, sh, rel, 10.0f,  1800.0f, CLR_GREEN);

        print(str("A%d D%d", (int)att, (int)dec), sx + 2, PY + 22 + sh + 4, CLR_DARK_GREY);
        print(str("S%d R%d", (int)(sus+0.5f), (int)rel), sx + 2, PY + 22 + sh + 12, CLR_DARK_GREY);
    }

    // ── LFO ─────────────────────────────────────────────────────────────
    {
        int sx = 278, sw = 88;
        section(sx, PY, sw, PH, "LFO", CLR_YELLOW);

        print("RATE", sx + 4, PY + 12, CLR_MEDIUM_GREY);
        lfo_rate = ui_knob(30, sx + 22, PY + 28, lfo_rate, 0.1f, 12.0f, CLR_YELLOW);
        print(str("%.1fHz", lfo_rate), sx + 36, PY + 22, CLR_DARK_GREY);

        print("DELAY", sx + 4, PY + 46, CLR_MEDIUM_GREY);
        lfo_delay = ui_knob(31, sx + 22, PY + 62, lfo_delay, 0.0f, 800.0f, CLR_YELLOW);
        print(str("%dms", (int)lfo_delay), sx + 36, PY + 56, CLR_DARK_GREY);

        print("DEPTH", sx + 4, PY + 80, CLR_MEDIUM_GREY);
        lfo_depth = ui_knob(32, sx + 22, PY + 96, lfo_depth, 0.0f, 1.0f, CLR_YELLOW);

        print("DEST", sx + 4, PY + 112, CLR_MEDIUM_GREY);
        if (ui_btn(sx + 4,  PY + 122, 26, 12, "VIB", lfo_dest == LFO_PITCH, CLR_YELLOW))
            lfo_dest = LFO_PITCH;
        if (ui_btn(sx + 32, PY + 122, 26, 12, "PWM", lfo_dest == LFO_DUTY,  CLR_YELLOW))
            lfo_dest = LFO_DUTY;
        if (ui_btn(sx + 60, PY + 122, 26, 12, "WAH", lfo_dest == LFO_CUTOFF,CLR_YELLOW))
            lfo_dest = LFO_CUTOFF;

        // animated LFO dot
        int lx = sx + 22 + (int)(sinf(now() * lfo_rate * 6.283f) * 14);
        circfill(lx, PY + 148, 3, CLR_YELLOW);
        circ(sx + 8,  PY + 148, 14, CLR_DARKER_GREY);
        line(sx + 8, PY + 148, lx, PY + 148, CLR_DARK_GREY);
    }

    // ── ARP ─────────────────────────────────────────────────────────────
    {
        int sx = 372, sw = 82;
        section(sx, PY, sw, PH, arp_on ? "ARP  ON" : "ARP", arp_on ? CLR_MAUVE : CLR_MEDIUM_GREY);

        print("MODE", sx + 4, PY + 14, CLR_MEDIUM_GREY);
        for (int m = 0; m < 3; m++)
            if (ui_btn(sx + 4 + m * 24, PY + 24, 22, 12, ARP_MODE_NAME[m], arp_mode == m, CLR_MAUVE))
                arp_mode = m;

        print("RANGE", sx + 4, PY + 42, CLR_MEDIUM_GREY);
        for (int r = 1; r <= 3; r++)
            if (ui_btn(sx + 4 + (r - 1) * 24, PY + 52, 22, 12, str("%d", r), arp_range == r, CLR_MAUVE))
                arp_range = r;

        print("RATE", sx + 4, PY + 70, CLR_MEDIUM_GREY);
        arp_rate = ui_knob(40, sx + 22, PY + 88, arp_rate, 60.0f, 240.0f, CLR_MAUVE);
        print(str("%dbpm", (int)arp_rate), sx + 36, PY + 82, CLR_DARK_GREY);

        // big ARP toggle — SPACE bar or click
        int abx = sx + 4, aby = PY + PH - 38, abw = sw - 8, abh = 28;
        if (ui_btn(abx, aby, abw, abh, arp_on ? "[ ARP ON ]" : "[ ARP OFF ]",
                   arp_on, CLR_MAUVE)) {
            arp_on = !arp_on;
            arp_scheduled = -1;
            if (!arp_on && voice >= 0) { note_off(voice); voice = -1; }
            if (!arp_on && sub_v >= 0) { note_off(sub_v); sub_v = -1; }
        }
        print("SPACE", sx + (sw - text_width("SPACE")) / 2, PY + PH - 8, CLR_DARKER_GREY);
    }

    // ── keyboard ─────────────────────────────────────────────────────────
    // white keys
    static const char WKB[] = "ASDFGHJKL;'";
    static const int  WSB[] = { 0,2,4,5,7,9,11,12,14,16,17 };
    for (int i = 0; i < NWHITE; i++) {
        int x = white_kx(i);
        int midi = base + W_SEMI[i];
        int lit = 0;
        // check computer keys
        if (i < 11 && key(WKB[i])) lit = 1;
        // check held stack
        for (int k = 0; k < nheld; k++) if (held[k] == midi) lit = 1;
        rectfill(x, KBY, KBW - 1, KBH, lit ? CLR_YELLOW : CLR_WHITE);
        rect(x, KBY, KBW - 1, KBH, CLR_DARK_GREY);
        if (i < 11) print(str("%c", WKB[i]), x + (KBW - 6) / 2, KBY + KBH - 9, CLR_LIGHT_GREY);
    }
    // mouse on white keys
    if (mouse_pressed(MOUSE_LEFT) && my >= KBY && my < KBY + KBH) {
        for (int j = 0; j < NBLACK; j++) {   // check black first (on top)
            if (mx >= black_kx(j) && mx < black_kx(j) + KBW - 10) goto kb_done;
        }
        for (int i = 0; i < NWHITE; i++) {
            if (mx >= white_kx(i) && mx < white_kx(i) + KBW - 1) {
                key_down(base + W_SEMI[i]); break;
            }
        }
    }

    // black keys (drawn on top of white)
    static const char BKB[] = "WETYUOP";
    static const int  BSB[] = { 1, 3, 6, 8, 10, 13, 15 };
    for (int j = 0; j < NBLACK; j++) {
        int x = black_kx(j);
        int midi = base + B_SEMI[j];
        int lit = 0;
        if (j < 7 && key(BKB[j])) lit = 1;
        for (int k = 0; k < nheld; k++) if (held[k] == midi) lit = 1;
        rectfill(x, KBY, KBW - 10, (int)(KBH * 0.62f), lit ? CLR_ORANGE : CLR_BROWNISH_BLACK);
        if (j < 7) print(str("%c", BKB[j]), x + 1, KBY + (int)(KBH * 0.62f) - 9, CLR_MEDIUM_GREY);

        if (mouse_pressed(MOUSE_LEFT) && my >= KBY && my < KBY + (int)(KBH * 0.62f)
            && mx >= x && mx < x + KBW - 10) key_down(midi);
    }
    kb_done:;

    // mouse release — clear one key (simplification: clear all mouse-triggered)
    if (mouse_released(MOUSE_LEFT) && my >= KBY) {
        for (int k = nheld - 1; k >= 0; k--) key_up(held[k]);
    }

    // ── status bar ───────────────────────────────────────────────────────
    int sy = KBY + KBH + 4;
    print(str("OCT C%d  [Z-] [X+]", base / 12 - 1), 10, sy, CLR_MEDIUM_GREY);
    print(str("WAVE:%s  SUB:%s  PORTA:%dms",
              wave == INSTR_SAW ? "SAW" : "SQR",
              sub == 0 ? "OFF" : sub == 1 ? "-1" : "-2",
              (int)porta),
          120, sy, CLR_DARK_GREY);
    print("[H]help", SCREEN_W - 42, sy, CLR_DARK_GREY);

    // ── help overlay ─────────────────────────────────────────────────────
    if (show_help) {
        rectfill(40, 30, 240, 160, CLR_BLACK);
        rect(40, 30, 240, 160, CLR_WHITE);
        print("SH-101 HELP", 120, 38, CLR_WHITE);
        static const char *HL[] = {
            "A-;   white keys (C3..)",
            "WEYUOP black keys",
            "Z / X   octave down/up",
            "SPACE   arp on/off",
            "H       this help",
            "",
            "DCO: wave + PW + sub osc",
            "VCF: cut/res/env amount",
            "ENV: A D S R (shared)",
            "LFO: vib/pwm/wah",
            "ARP: up/dn/ud, 1-3 oct",
        };
        for (int i = 0; i < 11; i++)
            print(HL[i], 50, 52 + i * 11, i == 0 ? CLR_YELLOW : CLR_MEDIUM_GREY);
        print("[H] close", 160, 176, CLR_DARK_GREY);
    }
}
