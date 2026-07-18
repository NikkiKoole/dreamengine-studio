/* de:meta
{
  "slug": "afxkeys",
  "title": "afx keys",
  "status": "active",
  "created": "2026-07-18",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "subtractive-synth",
    "adsr-envelope"
  ],
  "homage": "Novation AFX Station (2020) — the Bass Station II built with Richard D. James, whose headline firmware feature 'AFX Mode' made every key its own patch, turning the keyboard into a per-key sound-design / drum module.",
  "todo": [
    "BREAKCHOP MODE (the second half of the idea): chop a sampled break and map the slices across the keys, each slice REPITCHED not sped-up (sample_autotune keeps timing / grains for independent time-stretch) — so a key retunes a slice without chipmunking it. Leans on the PCM sampler that's still BUILDING (docs/design/mic-and-sampling.md); the braindance blind brief flags this same gap ('the chopped break kit').",
    "Per-key MODULATION p-locks: an LFO/mod-env per key (instrument_lfo/instrument_env) so a key can wobble/sweep on its own — the deeper AFX-Mode parameter set beyond the static patch."
  ],
  "description": {
    "summary": "AFX MODE for a keybed: every key is its own frozen patch (a p-lock per note), so the one C is always THIS creature and the B always THAT one — not one voice played chromatically.",
    "detail": "Each of the 128 keys holds an independent patch — engine (SAW/SQR/FM/PD/PLUCK/MALLET/MEMBRANE/EPIANO/PIPE/NOISE), the three engine macros, a per-key tuning offset (so a key can be pitched anywhere, in or out of the scale), filter type + cutoff + resonance, drive amount + waveshaper, and its own ADSR (plucky vs sustained). Press a key and it PLAYS and becomes the SELECTED key; the panel above edits that key's patch, and the edits ride LIVE under your finger (the AFX 'hold a key and turn a knob' feel). A voice-slot pool gives real polyphony with a different sound on every key, so a chord can be ten different creatures at once. ROLL KEY rolls the selected key into a new random personality; ROLL ALL (or press R) reseeds the whole board — the happy-accident engine. INIT resets a key to a plain saw at its true pitch. SAVE/LOAD keep your board. Play on-screen (multi-finger, glissando), on the QWERTY row (A=white, W E T Y U O P=black), Z/X octave, or a plugged-in MIDI keyboard.",
    "controls": "Tap/hold keys to play + select · panel knobs edit the selected key (live) · ENGINE row picks the voice · ROLL KEY / ROLL ALL (R) / INIT · SAVE / LOAD · Z/X octave"
  }
}
de:meta */
// AFX KEYS — every key is its own patch.
//
// The Novation AFX Station's "AFX Mode" (Aphex Twin's collaboration with Novation)
// let every key on a Bass Station II hold a full, independent patch — so the
// keyboard stops being "one synth voice played up and down a scale" and becomes a
// board of distinct little creatures: this C is a bright FM ping, that D is a
// detuned membrane thud, the next key a screaming diode-filtered saw. This cart is
// that idea in the studio: a per-MIDI-note patch table + a voice-slot POOL so a
// chord can sound ten different patches at once.
//
// How it works (the moog.c unmanaged-keybed pattern):
//   keybed.h runs in UNMANAGED mode (keybed_manage_voices(false)) — it only tells
//   us which key went down / up (touch + QWERTY + MIDI, with glissando). WE own
//   every voice: on a fresh press we grab a free slot from the pool, program it to
//   THAT key's patch, and note_on it; on release we note_off + free the slot.
//
//   Editing: pressing a key SELECTS it; the panel edits pat[sel], and each frame
//   the selected key's ringing voice is re-pushed live (the "hold a key and turn a
//   knob" feel). ROLL randomizes a patch — the emergent, RDJ happy-accident heart.
#include "studio.h"
#define KEYBED_WHITE_KEYS "ASDFGHJKL"     // GarageBand musical-typing default
#define KEYBED_BLACK_KEYS "WE TYU OP"
#include "keybed.h"
#include <stdint.h>

#define CLI(v,a,b) ((int)clamp((float)(v),(float)(a),(float)(b)))

// ── the engine roster (a curated palette that reads as distinct per-key voices) ──
enum { E_SAW, E_SQR, E_FM, E_PD, E_PLK, E_MAL, E_MEM, E_EPN, E_PIP, E_NOI, E_COUNT };
static const int   ENG_INSTR[E_COUNT] = { INSTR_SAW, INSTR_SQUARE, INSTR_FM, INSTR_PD,
    INSTR_PLUCK, INSTR_MALLET, INSTR_MEMBRANE, INSTR_EPIANO, INSTR_PIPE, INSTR_NOISE };
static const char *ENG_NAME[E_COUNT]  = { "SAW","SQR","FM","PD","PLK","MAL","MEM","EPN","PIP","NOI" };
static const int   ENG_COL[E_COUNT]   = { CLR_ORANGE, CLR_YELLOW, CLR_PINK, CLR_MAUVE,
    CLR_LIME_GREEN, CLR_GREEN, CLR_DARK_ORANGE, CLR_BLUE, CLR_INDIGO, CLR_LIGHT_GREY };

// ── filter options (index → engine FILTER_* mode) ──
enum { FT_OFF, FT_LP, FT_HP, FT_BP, FT_LAD, FT_DIO, FT_COUNT };
static const int   FT_MODE[FT_COUNT] = { FILTER_OFF, FILTER_LOW, FILTER_HIGH, FILTER_BAND, FILTER_LADDER, FILTER_DIODE };
static const char *FT_NAME[FT_COUNT] = { "NO FILT", "LOWPASS", "HIGHPASS", "BANDPASS", "LADDER", "DIODE" };
// filter modes the engine can switch LIVE on a held voice (note_filter); others apply at next press
static int ft_live(int m) { return m == FILTER_OFF || m == FILTER_LOW || m == FILTER_HIGH || m == FILTER_BAND; }

static const char *DM_NAME[4] = { "SOFT", "HARD", "FOLD", "ASYM" };
static const char *NOTE_NM[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
static const char *note_name(int midi) { return str("%s%d", NOTE_NM[midi % 12], midi / 12 - 1); }

// ── the per-key patch (128 of them = the instrument) ──
typedef struct {
    int8_t engine, ft, dmode, sustain;   // sustain 0..7
    float  harm, timb, morph;            // the 3 engine macros 0..1
    float  tune;                         // per-key pitch offset in semitones (-24..24)
    float  drive;                        // 0..1
    int    cutoff, res;                  // Hz, 0..15
    int    atk, dec, rel;                // ms
} Patch;
static Patch pat[128];

// ── voice-slot pool: one instrument slot per simultaneous note ──
#define POOL_N    12
#define POOL_BASE 5
static int  pool_midi[POOL_N];     // which MIDI note owns pool slot i, -1 = free
static int  vh[128], vs[128];      // live voice handle + slot per MIDI note (-1 = silent)
static int  sel = 60;              // the selected (last-pressed) key = what the panel edits

// ── program one slot from a patch ──
static void program(int slot, const Patch *p) {
    instrument(slot, ENG_INSTR[p->engine], p->atk, p->dec, p->sustain, p->rel);
    if (p->engine == E_SQR) instrument_duty(slot, 0.5f);
    instrument_harmonics(slot, p->harm);
    instrument_timbre(slot, p->timb);
    instrument_morph(slot, p->morph);
    instrument_tune(slot, p->tune);
    instrument_filter(slot, FT_MODE[p->ft], p->cutoff, CLI(p->res, 0, 15));
    instrument_drive(slot, p->drive);
    instrument_drive_mode(slot, p->dmode);
}

// ── keybed callbacks (unmanaged: we own the voices) ──
static void note_start(int midi, int vel) {
    int pi = -1;
    for (int i = 0; i < POOL_N; i++) if (pool_midi[i] < 0) { pi = i; break; }
    if (pi < 0) {                                   // pool full — steal the oldest slot's voice
        pi = 0; int old = pool_midi[0];
        if (old >= 0) { if (vh[old] >= 0) note_off(vh[old]); vh[old] = -1; vs[old] = -1; }
    }
    int slot = POOL_BASE + pi;
    pool_midi[pi] = midi;
    program(slot, &pat[midi]);
    vs[midi] = slot;
    vh[midi] = note_on(midi, slot, vel);            // pitch = the key; per-key tune is on the slot
    sel = midi;                                      // pressing selects for editing
}
static void note_stop(int midi) {
    if (vh[midi] >= 0) note_off(vh[midi]);
    vh[midi] = -1;
    for (int i = 0; i < POOL_N; i++) if (pool_midi[i] == midi) pool_midi[i] = -1;
    vs[midi] = -1;
}
// re-strike a held key after its patch changed (so engine/ADSR edits are audible now)
static void restrike(int midi) {
    if (!keybed_held(midi) || vs[midi] < 0) return;
    if (vh[midi] >= 0) note_off(vh[midi]);
    program(vs[midi], &pat[midi]);
    vh[midi] = note_on(midi, vs[midi], 6);
}
// push the selected key's live params to its ringing voice (the AFX knob-under-finger feel)
static void live_push(int midi) {
    int h = vh[midi], s = vs[midi];
    if (h < 0) return;
    Patch *p = &pat[midi];
    note_harmonics(h, p->harm); note_timbre(h, p->timb); note_morph(h, p->morph);
    if (s >= 0) instrument_tune(s, p->tune);
    int m = FT_MODE[p->ft];
    if (ft_live(m)) note_filter(h, m);
    note_cutoff(h, p->cutoff); note_res(h, CLI(p->res, 0, 15));
    note_drive(h, p->drive);
}

// ── ROLL: randomize a patch into a distinct personality ──
static void roll(Patch *p, int wild) {
    p->engine = rnd_between(0, E_COUNT);
    p->harm = rnd_float(); p->timb = rnd_float(); p->morph = rnd_float();
    p->tune = wild ? (float)rnd_between(-12, 13) : (float)rnd_between(-2, 3);
    p->ft = rnd_between(0, FT_COUNT);
    p->cutoff = rnd_between(300, 4000);
    p->res = rnd_between(0, wild ? 14 : 8);
    float dv = rnd_float(); p->drive = dv * dv * (wild ? 1.0f : 0.4f);   // skew toward clean
    p->dmode = rnd_between(0, 4);
    if (rnd_float() < 0.5f) {                                            // PLUCKY character
        p->atk = rnd_between(0, 8); p->dec = rnd_between(60, 400); p->sustain = 0; p->rel = rnd_between(80, 300);
    } else {                                                            // SUSTAINED character
        p->atk = rnd_between(2, 120); p->dec = rnd_between(100, 400); p->sustain = rnd_between(3, 8); p->rel = rnd_between(150, 600);
    }
}
static void init_patch(Patch *p) {   // a plain saw at true pitch
    p->engine = E_SAW; p->ft = FT_LP; p->dmode = DRIVE_SOFT; p->sustain = 6;
    p->harm = 0.5f; p->timb = 0.5f; p->morph = 0.5f; p->tune = 0; p->drive = 0.1f;
    p->cutoff = 2200; p->res = 3; p->atk = 6; p->dec = 160; p->rel = 260;
}

// ── UI pointer (the finger driving the panel — touch that began above the keys) ──
#define KB_Y 210
#define NOFINGER -999
static int mx, my, ui_press, ui_held;
static int ui_finger = NOFINGER;
static int seen_ids[12], seen_n;
static int touch_began(int id) { for (int i = 0; i < seen_n; i++) if (seen_ids[i] == id) return 0; return 1; }
static int in_box(int x, int y, int w, int h) { return mx >= x && mx < x + w && my >= y && my < y + h; }

static int ui_btn(int x, int y, int w, int h, const char *label, int on, int col) {
    int hot = in_box(x, y, w, h);
    rectfill(x, y, w, h, on ? col : CLR_BLACK);
    rect(x, y, w, h, hot ? CLR_WHITE : CLR_DARK_GREY);
    print(label, x + 3, y + (h - 5) / 2, on ? CLR_BLACK : CLR_MEDIUM_GREY);
    return hot && ui_press;
}
static float ui_slider(int id, int x, int y, int w, float val, float lo, float hi, int col) {
    if (ui_press && in_box(x - 2, y - 4, w + 6, 12)) ui_held = id;
    if (ui_held == id) val = lo + clamp((float)(mx - x) / w, 0, 1) * (hi - lo);
    rectfill(x, y + 1, w, 2, CLR_DARK_GREY);
    int kx = x + (int)((val - lo) / (hi - lo) * w);
    rectfill(kx - 2, y - 2, 4, 8, col);
    return val;
}

void init(void) {
    keybed_config(POOL_BASE, 4, 14);                 // base C4, two octaves of white keys
    keybed_layout(6, KB_Y, SCREEN_W - 12, SCREEN_H - KB_Y);
    keybed_manage_voices(false);                     // we own every voice
    keybed_on_note(note_start);
    keybed_on_off(note_stop);
    keybed_velocity(6);
    for (int i = 0; i < POOL_N; i++) pool_midi[i] = -1;
    for (int m = 0; m < 128; m++) { vh[m] = -1; vs[m] = -1; }

    // restore a saved board, else seed a fresh characterful one (reproducible)
    if (load_bytes(pat, sizeof pat) != (int)sizeof pat) {
        rnd_seed(4);                                 // fixed seed → the same board every fresh boot
        for (int m = 0; m < 128; m++) roll(&pat[m], 0);   // tame roll = playable but distinct
    }
}

void update(void) {
    keybed_update();                                 // Z/X octave shift handled inside keybed
    if (keyp('R')) { for (int m = 0; m < 128; m++) { roll(&pat[m], 1); restrike(m); } }
}

// draw one white/black key tinted by its patch personality
static void draw_key(int x, int y, int w, int h, int midi, int black) {
    Patch *p = &pat[midi];
    int held = keybed_held(midi);
    int base = ENG_COL[p->engine];
    int col  = held ? CLR_WHITE : base;
    rectfill(x, y, w - (black ? 0 : 1), h, col);
    rect(x, y, w - (black ? 0 : 1), h, black ? CLR_BLACK : CLR_DARK_GREY);
    if (midi == sel) { rect(x, y, w - (black ? 0 : 1), h, CLR_WHITE); rect(x + 1, y + 1, w - (black ? 2 : 3), h - 2, CLR_WHITE); }
    font(FONT_TINY);
    int tc = (col == CLR_YELLOW || col == CLR_WHITE || col == CLR_LIGHT_GREY) ? CLR_BLACK : CLR_WHITE;
    print(ENG_NAME[p->engine], x + 2, y + h - (black ? 18 : 24), tc);
    if ((int)p->tune != 0) print(str("%+d", (int)p->tune), x + 2, y + h - (black ? 11 : 15), tc);
    if (!black) print(note_name(midi), x + 2, y + h - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

void draw(void) {
    // resolve the UI pointer: keep ui_finger while down, else adopt a touch that began above the keys
    ui_press = 0; int fi = -1;
    for (int i = 0; i < touch_count(); i++) if (touch_id(i) == ui_finger) { fi = i; break; }
    if (fi < 0) {
        ui_finger = NOFINGER; ui_held = 0;
        for (int i = 0; i < touch_count(); i++)
            if (touch_y(i) < KB_Y && touch_began(touch_id(i))) { ui_finger = touch_id(i); ui_press = 1; fi = i; break; }
    }
    if (fi >= 0) { mx = touch_x(fi); my = touch_y(fi); } else { mx = mouse_x(); my = mouse_y(); }

    cls(CLR_DARKER_GREY);
    print("AFX KEYS", 8, 4, CLR_WHITE);
    font(FONT_SMALL); print("every key its own patch", 78, 5, CLR_ORANGE); font(FONT_NORMAL);

    // header buttons (right) — small font so the labels fit
    font(FONT_SMALL);
    if (ui_btn(250, 2, 58, 11, "ROLL ALL", 1, CLR_RED))    for (int m = 0; m < 128; m++) { roll(&pat[m], 1); restrike(m); }
    if (ui_btn(310, 2, 34, 11, "SAVE", 1, CLR_GREEN))      save_bytes(pat, sizeof pat);
    if (ui_btn(346, 2, 34, 11, "LOAD", 1, CLR_YELLOW))     { if (load_bytes(pat, sizeof pat) == (int)sizeof pat) for (int m = 0; m < 128; m++) restrike(m); }
    if (ui_btn(382, 2, 32, 11, "PANIC", 1, CLR_MAUVE))     { keybed_all_off(); for (int i = 0; i < POOL_N; i++) pool_midi[i] = -1; for (int m = 0; m < 128; m++) { vh[m] = -1; vs[m] = -1; } }
    font(FONT_NORMAL);

    // ── PATCH panel — edits the selected key ──
    Patch *p = &pat[sel];
    rect(6, 16, SCREEN_W - 12, 182, CLR_DARK_GREY);
    print(str("PATCH  %s", note_name(sel)), 12, 20, ENG_COL[p->engine]);
    font(FONT_SMALL);
    if (ui_btn(120, 18, 60, 11, "ROLL KEY", 1, CLR_RED))  { roll(p, 1); restrike(sel); }
    if (ui_btn(184, 18, 40, 11, "INIT", 1, CLR_INDIGO))   { init_patch(p); restrike(sel); }
    font(FONT_NORMAL);

    // engine row (also the legend for the key colours)
    for (int i = 0; i < E_COUNT; i++) {
        int bx = 10 + i * 40;
        if (ui_btn(bx, 34, 38, 14, ENG_NAME[i], p->engine == i, ENG_COL[i])) { p->engine = i; restrike(sel); }
    }

    // left column — macros + tune + drive
    font(FONT_TINY);
    print("HARM",  14, 58, CLR_MEDIUM_GREY);  p->harm  = ui_slider(1, 70, 58, 140, p->harm,  0, 1, CLR_PINK);
    print("TIMB",  14, 76, CLR_MEDIUM_GREY);  p->timb  = ui_slider(2, 70, 76, 140, p->timb,  0, 1, CLR_PINK);
    print("MORPH", 14, 94, CLR_MEDIUM_GREY);  p->morph = ui_slider(3, 70, 94, 140, p->morph, 0, 1, CLR_PINK);
    print("TUNE",  14, 112, CLR_MEDIUM_GREY); { float t = ui_slider(4, 70, 112, 140, p->tune, -24, 24, CLR_YELLOW); p->tune = (float)((int)(t + (t < 0 ? -0.5f : 0.5f))); }
    print(str("%+d st", (int)p->tune), 176, 106, CLR_MEDIUM_GREY);
    print("DRIVE", 14, 130, CLR_MEDIUM_GREY); p->drive = ui_slider(5, 70, 130, 140, p->drive, 0, 1, CLR_ORANGE);

    // right column — filter + drive mode
    if (ui_btn(232, 52, 80, 14, FT_NAME[p->ft], 1, CLR_ORANGE)) { p->ft = (p->ft + 1) % FT_COUNT; restrike(sel); }
    print("CUT", 232, 76, CLR_MEDIUM_GREY); { float c = ui_slider(6, 262, 76, 146, p->cutoff, 80, 6000, CLR_ORANGE); p->cutoff = (int)c; }
    print("RES", 232, 94, CLR_MEDIUM_GREY); { float r = ui_slider(7, 262, 94, 146, p->res, 0, 15, CLR_ORANGE); p->res = (int)(r + 0.5f); }
    if (ui_btn(232, 110, 80, 14, str("DRV %s", DM_NAME[p->dmode]), 1, CLR_INDIGO)) { p->dmode = (p->dmode + 1) % 4; restrike(sel); }
    print(str("cut %dhz  res %d", p->cutoff, p->res), 320, 114, CLR_MEDIUM_GREY);

    // ADSR row (the plucky-vs-sustained character)
    print("ENV", 14, 150, CLR_LIGHT_GREY);
    print("A", 32, 168, CLR_MEDIUM_GREY);  { float a = ui_slider(8,  44, 168, 74, p->atk,     0, 300, CLR_GREEN); p->atk = (int)a; }
    print("D", 126, 168, CLR_MEDIUM_GREY); { float d = ui_slider(9, 138, 168, 74, p->dec,    10, 600, CLR_GREEN); p->dec = (int)d; }
    print("S", 220, 168, CLR_MEDIUM_GREY); { float s = ui_slider(10,232, 168, 74, p->sustain, 0,  7, CLR_GREEN); p->sustain = (int)(s + 0.5f); }
    print("R", 314, 168, CLR_MEDIUM_GREY); { float r = ui_slider(11,326, 168, 80, p->rel,    20, 800, CLR_GREEN); p->rel = (int)r; }
    print(str("a%d d%d s%d r%d", p->atk, p->dec, p->sustain, p->rel), 14, 184, CLR_DARK_GREY);
    font(FONT_NORMAL);

    // info line
    print(str("OCT C%d   [Z -] [X +]   ROLL ALL = R   %d voices", keybed_octave(), POOL_N), 8, 201, CLR_LIGHT_GREY);

    // ── keyboard (each key tinted by its patch) ──
    int nw = keybed_white_count();
    for (int k = 0; k < nw; k++) { int x, y, w, h; keybed_white_rect(k, &x, &y, &w, &h); draw_key(x, y, w, h, keybed_white_midi(k), 0); }
    for (int k = 0; k < nw; k++) { int x, y, w, h, midi; if (!keybed_black_rect(k, &x, &y, &w, &h, &midi)) continue; draw_key(x, y, w, h, midi, 1); }

    // LIVE: the selected key's ringing voice follows the knobs under your finger
    live_push(sel);

    seen_n = 0;
    for (int i = 0; i < touch_count() && seen_n < 12; i++) seen_ids[seen_n++] = touch_id(i);
}
