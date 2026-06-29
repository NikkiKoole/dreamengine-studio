/* de:meta
{
  "title": "One Note",
  "status": "active",
  "created": "2026-06-21",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer",
    "swing-timing",
    "generative-melody"
  ],
  "lineage": "After BeatCraft Studio's 'the magic of one note' — a stripped groovebox that proves rhythm + swing beat melody; novel: scale-locked root transposition queued to the next bass step, and an XY pad Moog filter shaping the one voice.",
  "description": "A one-note funk groovebox — make a whole track move on a SINGLE note (after BeatCraft Studio's 'the magic of one note'). No melody, no piano roll: the funk is all RHYTHM, SWING, and sliding that one note around. A 16-step grid with three rows — KICK, SNARE, BASS — where the bass plays a single pitch on whatever steps you light. You transpose the whole loop by playing a new root on the little in-scale KEYBED (A S D F G H J K L, scale-locked to minor pentatonic so every root grooves), exactly like setting the root on a Keystep — and it's QUEUED: the new root latches in on the NEXT bass step, in time, with the armed key highlighted and a green tab marking the root that's actually sounding. SUB folds in the same note an octave down (the 'lower wave'). An XY PAD shapes the bass tone only — drag right for a brighter cutoff, up for more resonance (a Moog ladder filter, the funk quack). SWING drags the off-beat 16ths; TEMPO sets the head-nod. Tap or drag the grid to build a beat; type ASDFGHJKL to walk the root; ← / → tempo, ↑ / ↓ swing."
}
de:meta */
#include "studio.h"
#include "pointer.h"     // multi-finger pool: PTR_MAX/PTR_NONE + PTR_CLEAR/PTR_ACQUIRE/PTR_FIND
#include "ui.h"
#include <math.h>

// ONE NOTE — a one-note funk groovebox (after BeatCraft Studio's "the magic of one note").
//
// The whole conceit: a bassline is ONE note. The funk doesn't come from a melody — it
// comes from RHYTHM, SWING, and sliding that one note around. So there's no piano roll.
// The BASS row plays a single pitch on whatever steps you lit; you make the song by
// TRANSPOSING the whole loop — playing a new root on the little in-scale keybed, exactly
// like setting the root on a Keystep. The keybed is scale-locked (minor pentatonic) so
// every root grooves and there's no sour note to hit.
//
//   • 16-step grid, 3 rows: KICK · SNARE · BASS
//   • the KEYBED (A S D F G H J K L) — play a key to set the bass ROOT. Tap, click, or type.
//   • SUB — fold in the same note an octave DOWN (the "turn on a lower wave").
//   • the XY PAD — shapes the BASS tone only: drag right = brighter (cutoff up),
//     drag up = more resonant (the funk quack). A Moog ladder filter on the bass.
//   • SWING — drag the off-beat 16ths late; the groove that makes it funk, not march.
//   • TEMPO — ride it, or ← / → (↑ / ↓ for swing).
//
//   TAP / DRAG the grid cells to build a rhythm (each finger paints).

#define ROWS  3
#define STEPS 16

#define GX 50    // grid left edge
#define GY 20    // grid top edge
#define SX 16    // column stride
#define SY 18    // row stride
#define CW 14    // cell width
#define CH 15    // cell height

// instrument slots — the kit + the ONE bass voice
enum { SL_KICK = 5, SL_SNARE, SL_BASS };
enum { R_KICK = 0, R_SNARE, R_BASS };

static const char *LABEL[ROWS] = { "KICK", "SNARE", "BASS" };
static const int   LIT  [ROWS] = { CLR_RED, CLR_ORANGE, CLR_BLUE };

// a funky starter pattern — kick/snare hold it down, the bass rolls a syncopated one-note line
static const char *PRESET[ROWS] = {
    "x..x..x...x.x...",   // kick — funky, not four-on-the-floor
    "....x.......x...",   // snare — backbeat
    "x..x.x..x.xx.x..",   // bass — the rolling one-note riff
};

// the funk note pool: minor pentatonic. The keybed plays SCALE DEGREES, so every root
// lands in key. degree d → semitones above the base (5 notes per octave).
static const int PENTA[5] = { 0, 3, 5, 7, 10 };   // A minor pentatonic intervals
#define NDEG 5
#define NKEYS 9                                    // A S D F G H J K L → degrees 0..8
#define BASE 33                                    // A1 — a low, fat root
static int deg_semi(int d) { return 12 * (d / NDEG) + PENTA[d % NDEG]; }

// the keybed: the GarageBand home row, each key = the next scale degree up
static const int KB_KEY[NKEYS] = { 'A','S','D','F','G','H','J','K','L' };
#define KB_X 6
#define KB_Y 84
#define KB_W 22
#define KB_H 34

// the XY pad — bass tone (X = cutoff, Y = resonance)
#define XY_X 212
#define XY_Y 84
#define XY_W 100
#define XY_H 100

static bool grid[ROWS][STEPS];
static int  cur_step = 0;              // playhead column
static int  last_16  = -1;             // last sixteenth we triggered
static int  flash[ROWS];               // frame() each row last fired
static int  keyflash = -100;           // frame() the root last changed (keybed glow)

static int  cur_deg = 0;               // the bass root currently SOUNDING (scale degree)
static int  pending_deg = 0;           // the QUEUED root — latches in on the next bass step
static bool sub = false;               // the -1-octave "lower wave"
static int  tempo = 100;               // a head-nod funk tempo
static int  swing = 30;                // % the off-beat 16ths drag late

static float k_tempo = (100 - 80) / 60.0f;   // → 80..140 BPM
static float k_swing = 30 / 60.0f;            // → 0..60 %
static float xy_x = 0.55f, xy_y = 0.30f;      // bass-tone pad (cutoff, resonance)

// per-finger grid paint (the drummachine.c sweep pattern)
typedef struct { int id, paint, lastR, lastC; } Ptr;   // id MUST be first (pointer.h)
static Ptr ptr[PTR_MAX];

static int bass_note(void) { return BASE + deg_semi(cur_deg); }

static int cell_rc(int x, int y, int *r, int *c) {
    if (x < GX || y < GY) return 0;
    int cc = (x - GX) / SX, rr = (y - GY) / SY;
    if (cc < 0 || cc >= STEPS || rr < 0 || rr >= ROWS) return 0;
    *r = rr; *c = cc; return 1;
}

// fire one row. The BASS plays ONE note, at the keybed's current root.
static void play_row(int r) {
    switch (r) {
        case R_KICK:  hit(72, INSTR_NOISE, 2, 12);                 // click...
                      hit(34, SL_KICK, 6, 250); break;             // ...over the sine boom
        case R_SNARE: hit(58, SL_SNARE, 5, 110);                   // noise bark...
                      hit(53, INSTR_TRI, 3, 45); break;            // ...over a tonal body
        case R_BASS:  { cur_deg = pending_deg;                     // the queued root latches in HERE,
                        int n = bass_note();                       // in time, on the note it plays
                        hit(n, SL_BASS, 5, 150);
                        if (sub) hit(n - 12, SL_BASS, 4, 150);     // the "lower wave"
                      } break;
    }
}

static void set_cell(int r, int c, bool on) {
    grid[r][c] = on;
    if (on) { play_row(r); flash[r] = frame(); }   // audition only on turn-on
}

static void set_root(int d) {                      // keybed → QUEUE a new bass root (no off-beat blip)
    if (d < 0) d = 0; if (d > NKEYS - 1) d = NKEYS - 1;
    pending_deg = d;                               // latches in on the next bass step (see play_row)
    keyflash = frame();
}

// re-shape the bass tone from the XY pad — only when it actually moved (set-and-hold)
static void apply_tone(void) {
    static float aX = -1, aY = -1;
    if (xy_x == aX && xy_y == aY) return;
    int cutoff = (int)(120.0f * powf(5000.0f / 120.0f, xy_x));   // 120 Hz → ~5 kHz, log
    int res    = (int)(xy_y * 12.0f + 0.5f);                     // 0..12 (ladder quack)
    instrument_filter(SL_BASS, FILTER_LADDER, cutoff, res);      // the Moog 4-pole on the bass
    aX = xy_x; aY = xy_y;
}

void init() {
    // kick + snare — the drummachine recipes (punchy sine boom, noise+tone snare)
    instrument(SL_KICK, INSTR_SINE, 0, 280, 0, 60);
    instrument_env(SL_KICK, 1, ENV_PITCH, 0, 55, 30);          // +30st donk, settles 55ms
    instrument(SL_SNARE, INSTR_NOISE, 0, 130, 0, 50);
    instrument_filter(SL_SNARE, FILTER_BAND, 1400, 3);

    // THE bass — one plucky, slightly driven synth-bass voice. Tone comes from the XY pad.
    instrument(SL_BASS, INSTR_SAW, 0, 210, 2, 140);            // quick attack, short tail = funk
    instrument_drive(SL_BASS, 0.22f);                          // a touch of grit
    apply_tone();                                              // seed the ladder filter

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < STEPS; c++)
            grid[r][c] = (PRESET[r][c] == 'x');

    PTR_CLEAR(ptr);
}

void update() {
    // tempo + swing: knobs, or the arrow keys nudge them (and snap the knobs to match)
    if (btnp(0, BTN_LEFT))  { tempo = max(80,  tempo - 2); k_tempo = (tempo - 80) / 60.0f; }
    if (btnp(0, BTN_RIGHT)) { tempo = min(140, tempo + 2); k_tempo = (tempo - 80) / 60.0f; }
    if (btnp(0, BTN_DOWN))  { swing = max(0,  swing - 5);  k_swing = swing / 60.0f; }
    if (btnp(0, BTN_UP))    { swing = min(60, swing + 5);  k_swing = swing / 60.0f; }
    tempo = 80 + (int)(k_tempo * 60 + 0.5f);
    swing = (int)(k_swing * 60 + 0.5f);
    bpm(tempo);

    // ── transport: advance the playhead off the synth clock, with SWING ──
    // swing drags every ODD 16th late by swing% of a step (the drummachine recipe):
    // the counter only ticks over once beat_pos clears the swung onset.
    float pos16 = beat() * 4 + beat_pos() * 4.0f;
    int   n     = (int)pos16;
    int sixteenth = (n % 2 == 1 && pos16 - n < swing / 100.0f) ? n - 1 : n;
    if (sixteenth != last_16) {
        last_16  = sixteenth;
        cur_step = sixteenth % STEPS;
        for (int r = 0; r < ROWS; r++)
            if (grid[r][cur_step]) { play_row(r); flash[r] = frame(); }
    }

    // ── the KEYBED: type A S D F G H J K L to set the bass root ──
    for (int i = 0; i < NKEYS; i++)
        if (keyp(KB_KEY[i])) set_root(i);
    // …or tap/click a key
    for (int i = 0; i < NKEYS; i++)
        if (tapp(KB_X + i * KB_W, KB_Y, KB_W - 1, KB_H)) set_root(i);

    // ── the XY PAD: any finger/mouse inside it drags the bass tone ──
    for (int i = 0; i < touch_count(); i++) {
        int tx = touch_x(i), ty = touch_y(i);
        if (tx >= XY_X && tx < XY_X + XY_W && ty >= XY_Y && ty < XY_Y + XY_H) {
            xy_x = clamp((tx - XY_X) / (float)XY_W, 0, 1);
            xy_y = clamp(1.0f - (ty - XY_Y) / (float)XY_H, 0, 1);   // up = more
        }
    }
    apply_tone();

    // ── touch/mouse on the GRID: tap a cell to toggle, drag across to paint ──
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i), r, c;
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;                                 // pool full (>PTR_MAX fingers)
        if (fresh) {
            if (!cell_rc(tx, ty, &r, &c)) { p->id = PTR_NONE; continue; } // outside grid → keybed/pad own it
            *p = (Ptr){ id, !grid[r][c], r, c };
            set_cell(r, c, p->paint);
        } else if (cell_rc(tx, ty, &r, &c) && (r != p->lastR || c != p->lastC)) {
            p->lastR = r; p->lastC = c;
            set_cell(r, c, p->paint);
        }
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (p) p->id = PTR_NONE;
    }

#ifdef DE_TRACE
    watch("tempo",  "%d", tempo);
    watch("swing",  "%d", swing);
    watch("root",   "%d", deg_semi(cur_deg));
    watch("cutoff", "%d", (int)(120.0f * powf(5000.0f / 120.0f, xy_x)));
#endif
}

static const char *NOTE_NAMES[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
static const char *note_name(int midi) { return str("%s%d", NOTE_NAMES[midi % 12], midi / 12 - 1); }

void draw() {
    cls(CLR_BROWNISH_BLACK);

    print("ONE NOTE", 2, 4, CLR_LIGHT_YELLOW);
    print_right(str("%d BPM  swing %d%%", tempo, swing), 318, 4, CLR_LIGHT_GREY);

    // ── the step grid ──
    rectfill(GX + cur_step * SX, GY - 4, CW, 2, CLR_WHITE);   // playhead marker
    for (int r = 0; r < ROWS; r++) {
        bool hot = (frame() - flash[r]) < 5;
        font(FONT_SMALL);
        print(LABEL[r], 2, GY + r * SY + 4, hot ? CLR_WHITE : LIT[r]);
        font(FONT_NORMAL);
        for (int c = 0; c < STEPS; c++) {
            int x = GX + c * SX, y = GY + r * SY;
            if (grid[r][c]) {
                rectfill(x, y, CW, CH, LIT[r]);
                if (c == cur_step) rect(x, y, CW, CH, CLR_WHITE);
            } else {
                int bg = (c == cur_step) ? CLR_DARK_GREY
                       : (c % 4 == 0)    ? CLR_DARKER_BLUE
                                         : CLR_DARKER_GREY;
                rectfill(x, y, CW, CH, bg);
            }
        }
    }

    // ── the in-scale KEYBED — the ARMED key is highlighted; a green tab marks the root
    //    that's actually SOUNDING (they differ while a change is queued for the next step) ──
    font(FONT_SMALL);
    print("ROOT", KB_X, KB_Y - 9, CLR_LIGHT_GREY);
    print(note_name(bass_note()), KB_X + 30, KB_Y - 9, CLR_BLUE);     // currently sounding
    if (pending_deg != cur_deg)                                       // a change is queued
        print(str("> %s", note_name(BASE + deg_semi(pending_deg))), KB_X + 64, KB_Y - 9, CLR_YELLOW);
    bool kglow = (frame() - keyflash) < 8;
    for (int i = 0; i < NKEYS; i++) {
        int x = KB_X + i * KB_W;
        bool armed = (i == pending_deg), playing = (i == cur_deg);
        int col = armed ? (kglow ? CLR_WHITE : CLR_BLUE) : CLR_DARK_GREY;
        rectfill(x, KB_Y, KB_W - 1, KB_H, col);
        rect(x, KB_Y, KB_W - 1, KB_H, CLR_MEDIUM_GREY);
        if (playing)                                                  // the sounding-now marker
            rectfill(x + 2, KB_Y + 2, KB_W - 5, 4, armed ? CLR_BROWNISH_BLACK : CLR_GREEN);
        char lbl[2] = { (char)KB_KEY[i], 0 };
        print(lbl, x + KB_W / 2 - 2, KB_Y + KB_H - 9, armed ? CLR_BROWNISH_BLACK : CLR_LIGHT_GREY);
    }
    font(FONT_NORMAL);

    // ── the XY pad: bass tone ──
    rectfill(XY_X, XY_Y, XY_W, XY_H, CLR_DARKER_GREY);
    for (int g = 1; g < 4; g++) {                              // faint grid
        line(XY_X + g * XY_W / 4, XY_Y, XY_X + g * XY_W / 4, XY_Y + XY_H, CLR_DARK_GREY);
        line(XY_X, XY_Y + g * XY_H / 4, XY_X + XY_W, XY_Y + g * XY_H / 4, CLR_DARK_GREY);
    }
    rect(XY_X, XY_Y, XY_W, XY_H, CLR_MEDIUM_GREY);
    int dx = XY_X + (int)(xy_x * XY_W), dy = XY_Y + (int)((1.0f - xy_y) * XY_H);
    line(XY_X, dy, XY_X + XY_W, dy, CLR_DARKER_BLUE);
    line(dx, XY_Y, dx, XY_Y + XY_H, CLR_DARKER_BLUE);
    circfill(dx, dy, 4, CLR_BLUE);
    circ(dx, dy, 4, CLR_WHITE);
    font(FONT_SMALL);
    print("BASS TONE", XY_X, XY_Y - 9, CLR_LIGHT_GREY);
    print("cutoff >", XY_X + 2, XY_Y + XY_H + 2, CLR_DARK_GREY);      // brighter to the right
    print("res ^", XY_X + XY_W - 22, XY_Y - 9, CLR_DARK_GREY);        // more resonant upward
    font(FONT_NORMAL);

    // ── the rack: TEMPO · SWING · SUB ──
    ui_begin();
    int ky = 160;
    font(FONT_SMALL);
    ui_knob(&k_tempo, 28, ky, "TEMPO");
    ui_knob(&k_swing, 74, ky, "SWING");
    if (ui_button(108, ky - 10, 46, 18, sub ? "SUB *" : "SUB")) sub = !sub;
    print("ASDFGHJKL = root  -  tap grid  -  drag XY = tone", 6, 192, CLR_DARK_GREY);
    font(FONT_NORMAL);
    ui_end();
}
