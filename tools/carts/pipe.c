// pipe — INSTR_PIPE showcase: a blown flute (the ninth modeled ENGINE). An STK jet-drive bore:
// an air jet strikes the labium edge and self-oscillates, recirculating through a bore delay —
// so like REED and ORGAN it HOLDS for as long as you blow. One id covers concert flute /
// recorder / pan pipe — breathy, airy, woody. Every engine answers the same three 0..1 macros:
//   harmonics = OVERBLOW   (0 = pure fundamental → 1 = the octave flageolet jump + bright)
//   timbre    = BREATH AIR (0 = pure tone → 1 = airy/breathy — a flute is mostly air)
//   morph     = EMBOUCHURE (0 = hollow/dark recorder → 1 = focused/bright concert flute)
//
// Because it HOLDS, the macros are live expression: sweep OVERBLOW on a sustained note and it
// jumps to the octave the way a flautist overblows; sweep BREATH (SPACE auto-swell) for the air.
//
// MONO + SLIDE (key V): a flute is monophonic and you slide between notes, not retrigger. In mono
// mode one voice sounds; a new key glides the breath to the new pitch (note_pitch + note_glide,
// last-note priority — release and it slides back to the key still held). The 4th slider is
// cart-side SLIDE (the portamento time). Poly mode (default) blows an independent voice per key.
//
// The flute on the left shows the air jet at the embouchure + the overblow register; the breath
// meter rides the air macro; the readout prints the instrument() call to copy into your cart.
//
// controls: A S D F G H J K  blow & HOLD (release to stop)   ·   Z / X octave down / up
//           1..5 presets: flute / recorder / pan pipe / piccolo / breathy
//           V mono/poly   ·   SPACE auto-breath swell   ·   M autoplay phrase on/off
//           drag a slider (live on held notes), or LEFT/RIGHT pick + UP/DOWN turn it
//
// MULTITOUCH: poly — every finger blows its own pad (hold a chord); mono — the last finger down
// wins and slides. Hold a slider with one finger while a note drones. Desktop mouse = one pointer.

#include "studio.h"
#include <math.h>

#define I_PIPE 5
#define NPAD   8

static const char KEYS[NPAD] = { 'A','S','D','F','G','H','J','K' };

#define NSLIDER 4   // 0..2 = engine macros, 3 = cart-side SLIDE (portamento ms, mono only)
static const char *MNAME[NSLIDER] = { "harmonics", "timbre", "morph",   "slide" };
static const char *MLO[NSLIDER]   = { "pure",      "pure",   "hollow",  "snap"  };
static const char *MHI[NSLIDER]   = { "octave",    "airy",   "focused", "smear" };

// presets: macro positions for the flute family (overblow / breath air / embouchure).
// STARTING GUESSES, tuned by ear against the navkit pipe reference.
typedef struct { const char *name; float h, t, m; } Preset;
static const Preset PRESET[5] = {
    { "flute",    0.00f, 0.38f, 0.70f },  // concert flute: pure fundamental, moderate air, focused
    { "recorder", 0.00f, 0.55f, 0.30f },  // hollow + breathier, low embouchure (medieval)
    { "pan pipe", 0.08f, 0.78f, 0.50f },  // very airy, a shimmer of overblow
    { "piccolo",  0.55f, 0.28f, 0.82f },  // overblown to the octave, focused, less air
    { "breathy",  0.00f, 0.90f, 0.42f },  // maximum air — the jazzy breathy flute
};

static int   midi_of[NPAD];
static int   handle_of[NPAD];      // POLY: held-note handle per pad, -1 = silent
static float glow[NPAD];
static float knob[NSLIDER];
static int   sel = 0;
static int   cur_preset = 0;
static int   oct = 0;
static bool  autoplay = true;
static bool  breath = false;       // SPACE: auto-breath swell on held notes
static bool  mono = false;         // V: monophonic + slide
static float breath_lfo = 0.0f;
static int   apos = 0;
static float airflow = 0.0f;

// MONO state: one voice, last-note priority over a held-key stack — release the top key and
// the breath slides back down to whatever's still held (a flute never goes silent mid-phrase).
static int   mono_handle = -1;
static int   held_stack[NPAD];
static int   held_n = 0;
static int   active_pad = -1;

// per-finger pointer table — a finger blows a pad or drags a slider
#define NPTR 10
#define NOID (-999)
enum { PTR_IDLE, PTR_DRAG, PTR_BLOW };
typedef struct { int id, mode, k; } Ptr;
static Ptr ptr[NPTR];

// layout
#define PAD_W    34
#define PAD_X(b) (10 + (b) * (PAD_W + 4))
#define PAD_Y    90
#define PAD_H    24
#define PRE_Y    122
#define MROW_Y   152
#define MROW_W   66
#define MROW_X(k) (8 + (k) * 78)
// flute viz
#define BORE_X   10
#define BORE_Y   46
#define BORE_W   86

static int km(int b) { return midi_of[b] + oct * 12; }
static int slide_ms(void) { return (int)(8.0f + knob[3] * knob[3] * 250.0f); }

static void apply_patch(void) {
    instrument(I_PIPE, INSTR_PIPE, 1, 0, 4, 1200);   // held wind voice (attack 1, full sustain, release 4)
    instrument_harmonics(I_PIPE, knob[0]);
    instrument_timbre(I_PIPE, knob[1]);
    instrument_morph(I_PIPE, knob[2]);
}

// the SPACE auto-breath swell rides TIMBRE (the air macro) for the flute — a swelling airiness.
static float swelled_air(void) {
    float t = knob[1];
    if (breath) t = clamp(t * 0.5f + 0.5f * (0.5f + 0.5f * sinf(breath_lfo)), 0.0f, 1.0f);
    return t;
}

// push the live macros to every sounding voice — pipe reads all three LIVE, so an overblow or a
// breath swell reaches a sustained note in either mode.
static void push_live(void) {
    float t = swelled_air();
    int h[NPAD + 1], n = 0;
    if (mono) { if (mono_handle >= 0) h[n++] = mono_handle; }
    else for (int b = 0; b < NPAD; b++) if (handle_of[b] >= 0) h[n++] = handle_of[b];
    for (int i = 0; i < n; i++) {
        note_harmonics(h[i], knob[0]);
        note_timbre(h[i], t);
        note_morph(h[i], knob[2]);
    }
}

static void stack_push(int b)   { for (int i = 0; i < held_n; i++) if (held_stack[i] == b) return; held_stack[held_n++] = b; }
static void stack_remove(int b) { for (int i = 0; i < held_n; i++) if (held_stack[i] == b) { for (int j = i; j < held_n - 1; j++) held_stack[j] = held_stack[j + 1]; held_n--; return; } }

// blow pad b — in mono, slide the one voice to it (legato); in poly, start its own voice.
static void note_start(int b) {
    glow[b] = 1.0f;
    if (mono) {
        stack_push(b);
        if (mono_handle < 0) mono_handle = note_on(km(b), I_PIPE, 6);   // first note: real attack
        else { note_glide(mono_handle, slide_ms()); note_pitch(mono_handle, (float)km(b)); }  // legato slide
        active_pad = b;
    } else {
        if (handle_of[b] < 0) handle_of[b] = note_on(km(b), I_PIPE, 6);
    }
}
// release pad b — in mono, slide back to the next held key (or stop if none).
static void note_stop(int b) {
    if (mono) {
        stack_remove(b);
        if (held_n == 0) { if (mono_handle >= 0) note_off(mono_handle); mono_handle = -1; active_pad = -1; }
        else { active_pad = held_stack[held_n - 1]; note_glide(mono_handle, slide_ms()); note_pitch(mono_handle, (float)km(active_pad)); }
    } else {
        if (handle_of[b] >= 0) { note_off(handle_of[b]); handle_of[b] = -1; }
    }
}

static void all_notes_off(void) {
    for (int b = 0; b < NPAD; b++) if (handle_of[b] >= 0) { note_off(handle_of[b]); handle_of[b] = -1; }
    if (mono_handle >= 0) note_off(mono_handle);
    mono_handle = -1; held_n = 0; active_pad = -1;
}

static void set_preset(int p) {
    knob[0] = PRESET[p].h; knob[1] = PRESET[p].t; knob[2] = PRESET[p].m;
    cur_preset = p;
    apply_patch();
    push_live();
}

void init(void) {
    for (int i = 0; i < NPTR; i++) ptr[i].id = NOID;
    for (int b = 0; b < NPAD; b++) { midi_of[b] = degree(SCALE_MAJOR, 5, b); handle_of[b] = -1; }  // flute register
    knob[3] = 0.45f;       // a singing default slide for mono mode
    set_preset(0);         // concert flute
    bpm(96);
}

void update(void) {
    for (int b = 0; b < NPAD; b++) {
        if (keyp(KEYS[b])) note_start(b);
        if (keyr(KEYS[b])) note_stop(b);
    }
    for (int p = 0; p < 5; p++) if (keyp('1' + p)) set_preset(p);

    if (keyp(KEY_LEFT))  sel = (sel + NSLIDER - 1) % NSLIDER;
    if (keyp(KEY_RIGHT)) sel = (sel + 1) % NSLIDER;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        knob[sel] = clamp(knob[sel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        if (sel < 3) cur_preset = -1;     // slide is cart-side, doesn't change the voice
        apply_patch();
    }
    if (keyp('V'))       { mono = !mono; all_notes_off(); }   // swap mode — clear any stuck voices
    if (keyp(KEY_SPACE)) breath = !breath;
    if (keyp('M'))       { autoplay = !autoplay; if (!autoplay && mono && held_n == 0) all_notes_off(); }
    if (keyp('Z') && oct > -2) oct--;
    if (keyp('X') && oct <  2) oct++;

    // touch: blow a pad (held until the finger lifts) or drag a slider
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        Ptr *p = 0, *freeP = 0;
        for (int j = 0; j < NPTR; j++) {
            if (ptr[j].id == id) { p = &ptr[j]; break; }
            if (ptr[j].id == NOID && !freeP) freeP = &ptr[j];
        }
        if (!p) {
            if (!freeP) continue;
            p = freeP; *p = (Ptr){ id, PTR_IDLE, -1 };
            if (point_in_box(tx, ty, SCREEN_W - 92, 2, 88, 12)) { autoplay = !autoplay; continue; }
            if (point_in_box(tx, ty, SCREEN_W - 152, 2, 50, 12)) { mono = !mono; all_notes_off(); continue; }
            if (ty >= 122 - 2 && ty < 122 + 12)
                for (int q = 0; q < 5; q++) if (tx >= 12 + q * 60 && tx < 12 + q * 60 + 58) { set_preset(q); break; }
            for (int k = 0; k < NSLIDER; k++)
                if (point_in_box(tx, ty, MROW_X(k) - 2, MROW_Y - 8, MROW_W + 4, 20)) { p->mode = PTR_DRAG; p->k = sel = k; }
            if (p->mode == PTR_IDLE)
                for (int b = 0; b < NPAD; b++)
                    if (point_in_box(tx, ty, PAD_X(b), PAD_Y, PAD_W, PAD_H)) { p->mode = PTR_BLOW; p->k = b; note_start(b); break; }
        } else if (p->mode == PTR_DRAG) {
            knob[p->k] = clamp((float)(tx - MROW_X(p->k)) / (float)MROW_W, 0.0f, 1.0f);
            if (p->k < 3) cur_preset = -1;
            apply_patch();
        }
    }
    for (int i = 0; i < touch_ended_count(); i++)
        for (int j = 0; j < NPTR; j++)
            if (ptr[j].id == touch_ended_id(i)) {
                if (ptr[j].mode == PTR_BLOW && ptr[j].k >= 0) note_stop(ptr[j].k);
                ptr[j].id = NOID;
            }

    // autoplay: a slow legato phrase. POLY blows a fresh note each step; MONO drives the one
    // voice with SLIDES (the feature on show) whenever the player isn't holding a key.
    if (autoplay && every(2)) {
        static const int seq[8] = { 0, 2, 4, 2, 5, 4, 2, 0 };
        int b = seq[apos % 8] % NPAD;
        if (mono) {
            if (held_n == 0) {
                if (mono_handle < 0) mono_handle = note_on(km(b), I_PIPE, 6);
                else { note_glide(mono_handle, slide_ms()); note_pitch(mono_handle, (float)km(b)); }
                active_pad = b; glow[b] = 1.0f;
            }
        } else {
            hit(km(b), I_PIPE, 6, 360); glow[b] = 1.0f;   // self-oscillates through the gate
        }
        apos++;
    }

    breath_lfo += 0.10f;
    airflow += 0.05f + knob[0] * 0.08f;   // overblow speeds the jet
    push_live();

#ifdef DE_TRACE
    watch("harm", "%.2f", knob[0]);
    watch("timb", "%.2f", knob[1]);
    watch("mor",  "%.2f", knob[2]);
    watch("mono", "%d", mono ? 1 : 0);
    watch("slide_ms", "%d", slide_ms());
    watch("breath", "%d", breath ? 1 : 0);
    watch("preset", "%d", cur_preset);
    watch("held_n", "%d", held_n);
#endif
}

// returns whether the voice is currently sounding on pad b
static bool pad_on(int b) {
    if (mono) return (mono_handle >= 0 && active_pad == b);
    return handle_of[b] >= 0;
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("PIPE", 8, 6, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print("blown flute engine", 50, 8, CLR_MEDIUM_GREY);
    print_right(mono ? "V mono+slide" : "V poly", SCREEN_W - 102, 8, mono ? CLR_BLUE : CLR_DARK_GREY);
    print_right(autoplay ? "M phrase: on" : "M phrase: off", SCREEN_W - 10, 8, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // ── the flute: a horizontal pipe with an air jet at the embouchure; overblow speeds + splits it
    rectfill(BORE_X - 2, BORE_Y - 16, BORE_W + 8, 34, CLR_DARK_BROWN);
    int px0 = BORE_X + 4, px1 = BORE_X + BORE_W - 2, py = BORE_Y;
    rectfill(px0, py - 4, px1 - px0, 9, CLR_LIGHT_PEACH);    // pipe body
    rect(px0, py - 4, px1 - px0, 9, CLR_PEACH);
    circfill(px0 + 4, py, 2, CLR_BROWNISH_BLACK);            // embouchure hole
    for (int i = 0; i < 6; i++) circfill(px0 + 20 + i * 10, py, 1, CLR_DARK_BROWN);   // finger holes
    int held = mono ? (mono_handle >= 0 ? 1 : 0) : 0;
    if (!mono) for (int b = 0; b < NPAD; b++) if (handle_of[b] >= 0) held++;
    bool sounding = held > 0 || autoplay;
    if (sounding)                                            // the air jet across the mouth
        for (int s = 0; s < 6; s++) {
            float ph = airflow + s * 0.5f;
            float u = ph - floorf(ph);
            int x = px0 + (int)(u * (px1 - px0));
            int yy = py - 8 + (int)(sinf(ph * 6.28f * (1.0f + knob[0] * 2.0f)) * 2.5f);  // overblow → faster ripple
            pset(x, yy, u < 0.5f ? CLR_TRUE_BLUE : CLR_BLUE_GREEN);
        }
    font(FONT_TINY);
    print(knob[0] < 0.33f ? "fundamental" : knob[0] < 0.66f ? "overblowing" : "octave (8va)", BORE_X, BORE_Y + 14, CLR_LIGHT_PEACH);
    font(FONT_NORMAL);

    // ── breath/air meter (timbre) — the flute's air, swelling with SPACE
    {
        int bx = BORE_X, by = BORE_Y + 26;
        float t = swelled_air();
        font(FONT_TINY); print("air", bx, by - 7, breath ? CLR_LIME_GREEN : CLR_DARK_GREY); font(FONT_NORMAL);
        bar(bx, by, BORE_W, 6, t, breath ? CLR_LIME_GREEN : CLR_ORANGE, CLR_DARK_BROWN);
    }

    // ── readout
    font(FONT_SMALL);
    const char *pname = (cur_preset >= 0) ? PRESET[cur_preset].name : "(custom)";
    print(str("voice: %s", pname), 108, 24, CLR_LIGHT_YELLOW);
    print(knob[0] < 0.33f ? "fundamental register" : "overblown - octave + bright", 108, 36, CLR_MEDIUM_GREY);
    if (mono)
        print(held > 0 ? str("mono - sliding (%dms portamento)", slide_ms()) : "mono - press a key, then SLIDE to others",
              108, 48, held > 0 ? CLR_BLUE : CLR_DARK_GREY);
    else
        print(held > 0 ? str("poly - blowing %d note%s", held, held == 1 ? "" : "s") : "poly - press & HOLD keys to blow",
              108, 48, held > 0 ? CLR_PEACH : CLR_DARK_GREY);
    print(breath ? "SPACE auto-breath: swelling" : "SPACE auto-breath: off", 108, 60, breath ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_TINY);
    print(str("instrument(I, INSTR_PIPE, 1,0,4,1200)  h %.2f t %.2f m %.2f", knob[0], knob[1], knob[2]),
          108, 74, CLR_BLUE_GREEN);
    font(FONT_NORMAL);

    // ── pads
    for (int b = 0; b < NPAD; b++) {
        int x = PAD_X(b);
        bool on = pad_on(b);
        if (!on) glow[b] *= 0.88f;
        int col = on ? CLR_LIGHT_YELLOW : glow[b] > 0.2f ? CLR_DARK_PEACH : CLR_DARK_BROWN;
        rectfill(x, PAD_Y, PAD_W, PAD_H, col);
        rect(x, PAD_Y, PAD_W, PAD_H, on ? CLR_WHITE : CLR_DARK_RED);
        print(str("%c", KEYS[b]), x + PAD_W / 2 - 3, PAD_Y + PAD_H - 10, on ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREY);
    }

    // ── preset row
    font(FONT_SMALL);
    for (int p = 0; p < 5; p++)
        print(str("%d %s", p + 1, PRESET[p].name), 14 + p * 60, PRE_Y, p == cur_preset ? CLR_YELLOW : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // ── macro row (0..2 engine macros move live; 3 = cart-side slide, dimmed in poly)
    for (int k = 0; k < NSLIDER; k++) {
        int x = MROW_X(k), y = MROW_Y;
        bool on = (k == sel);
        bool dim = (k == 3 && !mono);    // slide only bites in mono mode
        font(FONT_SMALL);
        print(MNAME[k], x, y - 8, dim ? CLR_DARK_GREY : on ? CLR_YELLOW : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
        bar(x, y, MROW_W, 7, knob[k], dim ? CLR_DARK_BROWN : on ? CLR_ORANGE : CLR_DARK_PEACH, CLR_DARK_BROWN);
        font(FONT_TINY);
        print(MLO[k], x, y + 9, CLR_DARK_GREY);
        print_right(MHI[k], x + MROW_W, y + 9, CLR_DARK_GREY);
        font(FONT_NORMAL);
        if (on) print(">", x - 8, y, CLR_YELLOW);
    }

    font(FONT_TINY);
    int rx = print("A..K blow   Z/X oct   1..5 voices   V mono   ", 10, SCREEN_H - 8, CLR_DARK_GREY);
    int sx = print("SPACE breath", rx, SCREEN_H - 8, CLR_MEDIUM_GREY);
    print("   sliders: drag or arrows", sx, SCREEN_H - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
