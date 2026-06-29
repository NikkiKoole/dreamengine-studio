/* de:meta
{
  "title": "brass",
  "status": "active",
  "created": "2026-06-10",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "waveguide-synth",
    "analog-voice-modeling"
  ],
  "lineage": "Port of the STK BrassInstrument physical model (bore delay line + lip-valve biquad resonator); novel additions are the live-draggable trombone slide (continuous glissando via note_pitch) and the per-voice output-lane mute/wah (note_cutoff + note_res, per decision 0017).",
  "description": "INSTR_BRASS showcase - the lip-reed family (the tenth modeled ENGINE), and the LAST instrument the wavetables couldn't reach. An STK BrassInstrument waveguide: a bore delay line closed by a flaring bell, driven by a LIP VALVE modeled as a resonant biquad - the buzzing lips, whose resonance sits just above the played pitch (the mechanism behind lip-slurs and harmonic lock). It self-oscillates, so like REED and PIPE it HOLDS for as long as you blow. One id covers trumpet / cornet / flugelhorn / trombone / french horn / tuba. The three engine macros: instrument_harmonics = instrument/bore (0 = tight bright trumpet, walking up through cornet/flugel/trombone/horn to 1 = dark wide tuba - it drives the lip-resonance ratio, the lip Q, and the bell lowpass, never the pitch), instrument_timbre = BRASSINESS (0 = round and mellow; 1 = loud and blatty - the pressure-driven wave-steepening shockwave that makes a real horn open up at fortissimo, so the drive scales with blow pressure AND the macro: blow harder, get blattier), instrument_morph = breath/lip lean-in (0 = soft steady; 1 = a growling breath with deeper vibrato). THE SLIDE (the marquee): brass tracks pitch LIVE, so it slurs and glisses - grab the trombone SLIDE at the top and DRAG it for a continuous glissando (note_pitch + a tight note_glide, the finger IS the slide). Because it HOLDS, all three macros move live on a sounding note - sweep brassiness (SPACE auto-swell) and hear the tone bloom blatty. THE MUTE: closing the bell with a hand/plunger - a swept resonant lowpass on the voice (note_cutoff + note_res, the OUTPUT lane per decision 0017, not an engine macro); drag the MUTE slider open->closed for the pinched/nasal 'tight' sound, and sweep it live for the wah-wah (a plunger plate creeps over the bell in the viz). Plus a VIBRATO slider (note_lfo pitch wobble). Six HARDWARE PRESETS on the number row are the acceptance tests - if 1 trumpet / 2 cornet / 3 flugelhorn / 4 trombone / 5 french horn / 6 tuba don't each sound like themselves, the macro mapping is wrong. MONO + slide (key V, default - a horn is one voice): a new key legato-slides the voice, last-note priority; poly mode blows an independent voice per key. A S D F G H J K blow and hold, Z/X octave, 1..6 voices, drag the SLIDE for a glissando, V mono/poly, drag a slider (live, or LEFT/RIGHT + UP/DOWN), SPACE brassiness swell, M autoplay fanfare on/off. Multitouch: blow a pad per finger, drag the slide while a note drones. Design + STEP-0: instrument-engines.md §8.8.10."
}
de:meta */
// brass — INSTR_BRASS showcase: the lip-reed family (the tenth modeled ENGINE). An STK
// BrassInstrument waveguide: a bore delay line closed by a flaring bell, driven by a LIP VALVE
// modeled as a resonant biquad — the buzzing lips. It self-oscillates, so like REED and PIPE it
// HOLDS for as long as you blow. One id covers trumpet / cornet / flugelhorn / trombone /
// french horn / tuba. Every engine answers the same three 0..1 macros:
//   harmonics = INSTRUMENT/BORE (0 = tight bright trumpet → 1 = dark wide tuba)
//   timbre    = BRASSINESS      (0 = round & mellow → 1 = loud & blatty — the shockwave)
//   morph     = BREATH/LIP      (0 = soft steady → 1 = a growling breath with deeper vibrato)
//
// THE SLIDE (the marquee): brass tracks pitch LIVE, so it slurs and glisses. Grab the trombone
// SLIDE at the top and DRAG it — the held note glides continuously, a real glissando (note_pitch +
// a tight note_glide, the finger IS the slide). The keybed A..K plays the scale on top; in mono
// mode (default — a horn is one voice) a new key legato-slides the voice, in poly each key blows
// its own. The brassiness explodes as you push TIMBRE — the loud-and-blatty fortissimo.
//
// THE MUTE: closing the bell with a hand/plunger (the "tight"/nasal sound, and the wah when you
// sweep it). It's NOT an engine macro — a mute shapes the OUTPUT, so it's a swept resonant lowpass
// on the voice (note_cutoff + note_res), the output lane per decision 0017. Drag the MUTE slider:
// open = wide bell, closed = pinched & nasal; sweeping it live IS the wah-wah.
//
// controls: A S D F G H J K  blow & HOLD (release to stop)   ·   Z / X octave down / up
//           1..6 presets: trumpet / cornet / flugelhorn / trombone / french horn / tuba
//           drag the SLIDE (trombone, top) for a glissando   ·   V mono/poly   ·   M autoplay
//           SPACE brassiness swell   ·   drag a slider — incl. MUTE (close the bell / wah) ·
//           LEFT/RIGHT pick a slider + UP/DOWN turn it
//
// MULTITOUCH: poly — every finger blows its own pad; mono — the last finger down wins. Drag the
// slide with one finger while a note drones. Desktop mouse = one pointer.

#include "studio.h"
#include "pointer.h"     // multi-finger pool: PTR_MAX/PTR_NONE + PTR_CLEAR/PTR_ACQUIRE/PTR_FIND
#include <math.h>

#define I_BRASS 5
#define NPAD    8

static const char KEYS[NPAD] = { 'A','S','D','F','G','H','J','K' };

#define NSLIDER 6   // 0..2 = engine macros, 3 = cart-side SLIDE (mono only), 4 = VIBRATO (note_lfo), 5 = MUTE
static const char *MNAME[NSLIDER] = { "harmonics", "timbre",  "morph",   "slide",  "vibrato", "mute"   };
static const char *MLO[NSLIDER]   = { "trumpet",   "mellow",  "steady",  "snap",   "none",    "open"   };
static const char *MHI[NSLIDER]   = { "tuba",      "blatty",  "growl",   "smear",  "wide",    "closed" };

// presets: macro positions across the brass family (instrument/bore · brassiness · breath).
// STARTING GUESSES, tuned by ear against the engine — the harmonics arc walks bright→dark.
typedef struct { const char *name; float h, t, m; int oct; } Preset;
static const Preset PRESET[6] = {
    { "trumpet",     0.15f, 0.60f, 0.42f,  1 },  // tight bright bore, plenty of brassiness, up an 8ve
    { "cornet",      0.30f, 0.44f, 0.40f,  1 },  // a touch darker & rounder than the trumpet
    { "flugelhorn",  0.46f, 0.28f, 0.45f,  0 },  // mellow, conical, low brassiness — the dark one
    { "trombone",    0.56f, 0.52f, 0.46f,  0 },  // mid bore, the slide horn
    { "french horn", 0.70f, 0.34f, 0.50f,  0 },  // dark, round, a little breath
    { "tuba",        0.92f, 0.46f, 0.48f, -1 },  // wide dark bore, down an 8ve
};

static int   midi_of[NPAD];
static int   handle_of[NPAD];      // POLY: held-note handle per pad, -1 = silent
static float glow[NPAD];
static float knob[NSLIDER];
static int   sel = 0;
static int   cur_preset = 0;
static int   oct = 0;
static bool  autoplay = true;
static bool  blat = false;         // SPACE: auto-brassiness swell on held notes
static bool  mono = true;          // a horn is monophonic — default on, so the slide makes sense
static float blat_lfo = 0.0f;
static int   apos = 0;
static float buzz = 0.0f;          // lip-buzz animation phase

// MONO state: one voice, last-note priority over a held-key stack.
static int   mono_handle = -1;
static int   held_stack[NPAD];
static int   held_n = 0;
static int   active_pad = -1;

// THE SLIDE: a continuous pitch ribbon. While the player drags it, the voice glides to a float
// pitch read straight off the slide position (a real glissando). -1 = not gripping the slide.
static float slide_pitch = -1.0f;  // current dragged float midi, or -1 when released
static bool  sliding = false;

// per-finger pointer table — a finger blows a pad, drags a slider, or grips the slide
enum { PTR_IDLE, PTR_DRAG, PTR_BLOW, PTR_SLIDE };
typedef struct { int id, mode, k; } Ptr;   // id MUST be first (pointer.h)
static Ptr ptr[PTR_MAX];

// layout
#define PAD_W    34
#define PAD_X(b) (10 + (b) * (PAD_W + 4))
#define PAD_Y    96
#define PAD_H    24
#define PRE_Y    128
#define MROW_W    66
#define MROW_X(k) (8 + ((k) % 4) * 78)       // 4 columns wide
#define MROW_Y(k) (150 + ((k) / 4) * 26)     // wraps to a 2nd row at index 4 (vibrato, +mute…)
// the trombone slide ribbon (top) — the marquee gesture
#define SLIDE_X  10
#define SLIDE_Y  44
#define SLIDE_W  (SCREEN_W - 20)
#define SLIDE_H  20
#define SLIDE_SPAN 14.0f          // semitones across the ribbon (a bit over an octave)

static int   km(int b) { return midi_of[b] + oct * 12; }
static int   slide_ms(void) { return (int)(8.0f + knob[3] * knob[3] * 250.0f); }
// MUTE = closing the bell with a hand/plunger/harmon mute — a swept resonant lowpass (the output
// lane, ADR 0017: a mute shapes what comes OUT, it's not an engine concern). open (0) = wide bell,
// 8kHz, no resonance → full brass; closed (1) = pinched 500Hz with a strong nasal resonant peak.
// Because push_live() re-applies it every frame, dragging the slider sweeps it live = the wah-wah.
static float mute_cut(void) { return 8000.0f * powf(500.0f / 8000.0f, knob[5]); }   // exp open→closed
static float mute_res(void) { return 0.7f + knob[5] * 10.0f; }                       // nasal peak grows as it shuts
static float slide_base(void) { return (float)(midi_of[0] + oct * 12); }   // left edge = lowest pad note
// map a slide-ribbon x to a continuous float midi pitch
static float slide_x_to_pitch(int x) {
    float u = clamp((float)(x - SLIDE_X) / (float)SLIDE_W, 0.0f, 1.0f);
    return slide_base() + u * SLIDE_SPAN;
}
static int slide_pitch_to_x(float p) {
    float u = clamp((p - slide_base()) / SLIDE_SPAN, 0.0f, 1.0f);
    return SLIDE_X + (int)(u * SLIDE_W);
}

static void apply_patch(void) {
    instrument(I_BRASS, INSTR_BRASS, 1, 0, 4, 1200);   // held wind voice (attack 1, full sustain, release 4)
    instrument_harmonics(I_BRASS, knob[0]);
    instrument_timbre(I_BRASS, knob[1]);
    instrument_morph(I_BRASS, knob[2]);
    if (knob[5] < 0.03f)                                              // mute OPEN = NO filter at all (identical to the bare brass)
        instrument_filter(I_BRASS, FILTER_OFF, 0, 0);
    else                                                             // closing the bell — a swept resonant lowpass (note_cutoff/res sweep it live below)
        instrument_filter(I_BRASS, FILTER_LOW, (int)mute_cut(), (int)mute_res());
}

// the SPACE swell rides TIMBRE (brassiness) — a horn blooming into the blatty fortissimo.
static float swelled_brass(void) {
    float t = knob[1];
    if (blat) t = clamp(t * 0.45f + 0.55f * (0.5f + 0.5f * sinf(blat_lfo)), 0.0f, 1.0f);
    return t;
}

// push live macros to every sounding voice — brass reads all three LIVE, so brassiness/breath
// reach a sustained note in either mode.
static void push_live(void) {
    float t = swelled_brass();
    int h[NPAD + 1], n = 0;
    if (mono) { if (mono_handle >= 0) h[n++] = mono_handle; }
    else for (int b = 0; b < NPAD; b++) if (handle_of[b] >= 0) h[n++] = handle_of[b];
    for (int i = 0; i < n; i++) {
        note_harmonics(h[i], knob[0]);
        note_timbre(h[i], t);
        note_morph(h[i], knob[2]);
        note_lfo(h[i], 0, LFO_PITCH, 5.5f, knob[4] * 0.4f);   // VIBRATO — independent of morph's growl
        if (knob[5] < 0.03f) {                                // MUTE — open = bell wide open, NO filter
            note_filter(h[i], FILTER_OFF);
        } else {                                              // closing the bell, swept live (drag it = wah)
            note_filter(h[i], FILTER_LOW);
            note_cutoff(h[i], (int)mute_cut());
            note_res(h[i], mute_res());
        }
    }
}

static void stack_push(int b)   { for (int i = 0; i < held_n; i++) if (held_stack[i] == b) return; held_stack[held_n++] = b; }
static void stack_remove(int b) { for (int i = 0; i < held_n; i++) if (held_stack[i] == b) { for (int j = i; j < held_n - 1; j++) held_stack[j] = held_stack[j + 1]; held_n--; return; } }

// blow pad b — in mono, legato-slide the one voice to it; in poly, start its own voice.
static void note_start(int b) {
    glow[b] = 1.0f;
    sliding = false; slide_pitch = -1.0f;   // a key press takes over from a free slide
    if (mono) {
        stack_push(b);
        if (mono_handle < 0) mono_handle = note_on(km(b), I_BRASS, 6);    // first note: real lip attack
        else { note_glide(mono_handle, slide_ms()); note_pitch(mono_handle, (float)km(b)); }  // legato slide
        active_pad = b;
    } else {
        if (handle_of[b] < 0) handle_of[b] = note_on(km(b), I_BRASS, 6);
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
    mono_handle = -1; held_n = 0; active_pad = -1; sliding = false; slide_pitch = -1.0f;
}

// grip the slide at x — start (or grab) the mono voice and glide it to the dragged pitch.
static void slide_grip(int x) {
    sliding = true;
    slide_pitch = slide_x_to_pitch(x);
    if (mono_handle < 0) mono_handle = note_on((int)(slide_pitch + 0.5f), I_BRASS, 6);
    note_glide(mono_handle, 30);                 // a tight glide so the finger drives the slide directly
    note_pitch(mono_handle, slide_pitch);
    active_pad = -1;
}
static void slide_move(int x) {
    if (mono_handle < 0) return;
    slide_pitch = slide_x_to_pitch(x);
    note_pitch(mono_handle, slide_pitch);
}
static void slide_release(void) {
    sliding = false;
    // if no keys are held, let the slide note ring out and release; if keys held, fall back to them
    if (held_n == 0) { if (mono_handle >= 0) note_off(mono_handle); mono_handle = -1; }
    else { active_pad = held_stack[held_n - 1]; note_glide(mono_handle, slide_ms()); note_pitch(mono_handle, (float)km(active_pad)); }
    slide_pitch = -1.0f;
}

static void set_preset(int p) {
    knob[0] = PRESET[p].h; knob[1] = PRESET[p].t; knob[2] = PRESET[p].m;
    oct = PRESET[p].oct;
    cur_preset = p;
    apply_patch();
    push_live();
}

void init(void) {
    PTR_CLEAR(ptr);
    for (int b = 0; b < NPAD; b++) { midi_of[b] = degree(SCALE_MAJOR, 4, b); handle_of[b] = -1; }  // brass register
    knob[3] = 0.45f;       // a singing default slide for mono legato
    knob[5] = 0.0f;        // mute open (full bell) by default
    set_preset(3);         // trombone — the slide horn, fitting for the marquee gesture
    bpm(96);
}

void update(void) {
    for (int b = 0; b < NPAD; b++) {
        if (keyp(KEYS[b])) note_start(b);
        if (keyr(KEYS[b])) note_stop(b);
    }
    for (int p = 0; p < 6; p++) if (keyp('1' + p)) set_preset(p);

    if (keyp(KEY_LEFT))  sel = (sel + NSLIDER - 1) % NSLIDER;
    if (keyp(KEY_RIGHT)) sel = (sel + 1) % NSLIDER;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        knob[sel] = clamp(knob[sel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        if (sel < 3) cur_preset = -1;     // slide is cart-side, doesn't change the voice
        apply_patch();
    }
    if (keyp('V'))       { mono = !mono; all_notes_off(); }   // swap mode — clear any stuck voices
    if (keyp(KEY_SPACE)) blat = !blat;
    if (keyp('M'))       { autoplay = !autoplay; if (!autoplay && mono && held_n == 0 && !sliding) all_notes_off(); }
    if (keyp('Z') && oct > -2) oct--;
    if (keyp('X') && oct <  2) oct++;

    // touch: grip the slide, blow a pad (held until lift), or drag a slider
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;                          // pool full (>PTR_MAX fingers)
        if (fresh) {
            *p = (Ptr){ id, PTR_IDLE, -1 };
            if (point_in_box(tx, ty, SCREEN_W - 92, 2, 88, 12)) { autoplay = !autoplay; continue; }
            if (point_in_box(tx, ty, SCREEN_W - 152, 2, 50, 12)) { mono = !mono; all_notes_off(); continue; }
            if (ty >= PRE_Y - 2 && ty < PRE_Y + 12)
                for (int q = 0; q < 6; q++) if (tx >= 12 + q * 50 && tx < 12 + q * 50 + 48) { set_preset(q); break; }
            for (int k = 0; k < NSLIDER; k++)
                if (point_in_box(tx, ty, MROW_X(k) - 2, MROW_Y(k) - 8, MROW_W + 4, 20)) { p->mode = PTR_DRAG; p->k = sel = k; }
            if (p->mode == PTR_IDLE && mono && point_in_box(tx, ty, SLIDE_X, SLIDE_Y, SLIDE_W, SLIDE_H)) {
                p->mode = PTR_SLIDE; slide_grip(tx);
            }
            if (p->mode == PTR_IDLE)
                for (int b = 0; b < NPAD; b++)
                    if (point_in_box(tx, ty, PAD_X(b), PAD_Y, PAD_W, PAD_H)) { p->mode = PTR_BLOW; p->k = b; note_start(b); break; }
        } else if (p->mode == PTR_DRAG) {
            knob[p->k] = clamp((float)(tx - MROW_X(p->k)) / (float)MROW_W, 0.0f, 1.0f);
            if (p->k < 3) cur_preset = -1;
            apply_patch();
        } else if (p->mode == PTR_SLIDE) {
            slide_move(tx);
        }
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (!p) continue;
        if (p->mode == PTR_BLOW && p->k >= 0) note_stop(p->k);
        if (p->mode == PTR_SLIDE) slide_release();
        p->id = PTR_NONE;
    }

    // autoplay: a slow legato fanfare. POLY blows a fresh note each step; MONO drives the one
    // voice with SLIDES (the feature on show) whenever the player isn't holding a key or sliding.
    if (autoplay && every(2)) {
        static const int seq[8] = { 0, 2, 4, 7, 4, 2, 4, 0 };
        int b = seq[apos % 8] % NPAD;
        if (mono) {
            if (held_n == 0 && !sliding) {
                if (mono_handle < 0) mono_handle = note_on(km(b), I_BRASS, 6);
                else { note_glide(mono_handle, slide_ms()); note_pitch(mono_handle, (float)km(b)); }
                active_pad = b; glow[b] = 1.0f;
            }
        } else {
            hit(km(b), I_BRASS, 6, 360); glow[b] = 1.0f;   // self-oscillates through the gate
        }
        apos++;
    }

    blat_lfo += 0.10f;
    buzz += 0.30f + knob[1] * 0.40f;   // brassiness drives a faster lip buzz
    push_live();

#ifdef DE_TRACE
    watch("harm", "%.2f", knob[0]);
    watch("timb", "%.2f", knob[1]);
    watch("mor",  "%.2f", knob[2]);
    watch("mono", "%d", mono ? 1 : 0);
    watch("slide_ms", "%d", slide_ms());
    watch("sliding", "%d", sliding ? 1 : 0);
    watch("slide_pitch", "%.2f", slide_pitch);
    watch("blat", "%d", blat ? 1 : 0);
    watch("preset", "%d", cur_preset);
    watch("held_n", "%d", held_n);
#endif
}

// returns whether the voice is currently sounding on pad b
static bool pad_on(int b) {
    if (mono) return (mono_handle >= 0 && active_pad == b && !sliding);
    return handle_of[b] >= 0;
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("BRASS", 8, 6, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print("lip-reed brass engine", 56, 8, CLR_MEDIUM_GREY);
    print_right(mono ? "V mono+slide" : "V poly", SCREEN_W - 102, 8, mono ? CLR_ORANGE : CLR_DARK_GREY);
    print_right(autoplay ? "M fanfare: on" : "M fanfare: off", SCREEN_W - 10, 8, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // ── readout
    font(FONT_SMALL);
    const char *pname = (cur_preset >= 0) ? PRESET[cur_preset].name : "(custom)";
    print(str("voice: %s", pname), 8, 22, CLR_LIGHT_YELLOW);
    print(knob[0] < 0.33f ? "bright tight bore" : knob[0] < 0.66f ? "mid bore" : "dark wide bore", 120, 22, CLR_MEDIUM_GREY);
    print(knob[1] < 0.4f ? "mellow / round" : knob[1] < 0.7f ? "brassy" : "blatty fortissimo", 210, 22, knob[1] > 0.7f ? CLR_ORANGE : CLR_MEDIUM_GREY);
    font(FONT_TINY);
    print(str("instrument(I, INSTR_BRASS, 1,0,4,1200)  h %.2f t %.2f m %.2f", knob[0], knob[1], knob[2]),
          8, 34, CLR_BLUE_GREEN);
    font(FONT_NORMAL);

    // ── THE SLIDE: a trombone slide ribbon (the marquee gesture). The brass bell at the right, a
    // telescoping slide whose handle sits at the current pitch; drag it for a glissando.
    {
        int sy = SLIDE_Y, sh = SLIDE_H;
        rectfill(SLIDE_X, sy, SLIDE_W, sh, mono ? CLR_DARK_BROWN : CLR_BROWNISH_BLACK);
        rect(SLIDE_X, sy, SLIDE_W, sh, mono ? CLR_DARK_RED : CLR_DARK_GREY);
        // the two slide rails
        int railY = sy + sh / 2;
        line(SLIDE_X + 2, railY - 3, SLIDE_X + SLIDE_W - 14, railY - 3, CLR_DARK_GREY);
        line(SLIDE_X + 2, railY + 3, SLIDE_X + SLIDE_W - 14, railY + 3, CLR_DARK_GREY);
        // the flared bell on the right
        rectfill(SLIDE_X + SLIDE_W - 12, sy + 1, 11, sh - 2, CLR_DARK_PEACH);
        trifill(SLIDE_X + SLIDE_W - 12, sy, SLIDE_X + SLIDE_W - 12, sy + sh, SLIDE_X + SLIDE_W, railY, CLR_PEACH);
        // the MUTE plunger: a plate creeping over the bell mouth from the right as the bell closes
        int mu = (int)(knob[5] * 13.0f);
        if (mu > 0) {
            rectfill(SLIDE_X + SLIDE_W - mu, sy + 1, mu, sh - 2, CLR_DARKER_GREY);
            rect(SLIDE_X + SLIDE_W - mu, sy + 1, mu, sh - 2, CLR_MEDIUM_GREY);
        }
        // the slide handle: at the dragged pitch when sliding, else the active note's pitch
        float showp = -1.0f;
        if (sliding && slide_pitch >= 0) showp = slide_pitch;
        else if (mono && mono_handle >= 0 && active_pad >= 0) showp = (float)km(active_pad);
        if (showp >= 0) {
            int hx = slide_pitch_to_x(showp);
            hx = (hx < SLIDE_X + 2) ? SLIDE_X + 2 : (hx > SLIDE_X + SLIDE_W - 16 ? SLIDE_X + SLIDE_W - 16 : hx);
            // lip buzz at the mouthpiece (left) when sounding
            for (int s = 0; s < 5; s++) {
                float ph = buzz + s * 0.6f, u = ph - floorf(ph);
                int bx = SLIDE_X + 3 + (int)(u * (hx - SLIDE_X - 3));
                int by = railY + (int)(sinf(ph * 6.28f) * (1.5f + knob[1] * 2.0f));
                pset(bx, by, u < 0.5f ? CLR_LIGHT_YELLOW : CLR_ORANGE);
            }
            rectfill(hx, sy + 2, 6, sh - 4, sliding ? CLR_LIGHT_YELLOW : CLR_ORANGE);   // the handle
            rect(hx, sy + 2, 6, sh - 4, CLR_WHITE);
        }
        font(FONT_TINY);
        print(mono ? (sliding ? "SLIDE - glissando!" : "drag the SLIDE -> glissando") : "(slide: mono only)",
              SLIDE_X + 2, sy - 7, mono ? (sliding ? CLR_LIGHT_YELLOW : CLR_DARK_GREY) : CLR_DARK_GREY);
        font(FONT_NORMAL);
    }

    // ── brassiness meter (timbre) — the loud/blatty shockwave, swelling with SPACE
    {
        int bx = SLIDE_X, by = SLIDE_Y + SLIDE_H + 8;
        float t = swelled_brass();
        font(FONT_TINY); print("brassiness", bx, by - 7, blat ? CLR_LIME_GREEN : CLR_DARK_GREY); font(FONT_NORMAL);
        bar(bx, by, SLIDE_W, 5, t, t > 0.7f ? CLR_RED : (blat ? CLR_LIME_GREEN : CLR_ORANGE), CLR_DARK_BROWN);
    }

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
    for (int p = 0; p < 6; p++)
        print(str("%d %s", p + 1, PRESET[p].name), 14 + p * 50, PRE_Y, p == cur_preset ? CLR_YELLOW : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // ── macro row (0..2 engine macros move live; 3 = cart-side slide, dimmed in poly)
    for (int k = 0; k < NSLIDER; k++) {
        int x = MROW_X(k), y = MROW_Y(k);
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
    int rx = print("A..K blow   Z/X oct   1..6 voices   V mono   ", 10, SCREEN_H - 8, CLR_DARK_GREY);
    int sx = print("SPACE brass", rx, SCREEN_H - 8, CLR_MEDIUM_GREY);
    print("   drag the slide / sliders (mute = wah)", sx, SCREEN_H - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
