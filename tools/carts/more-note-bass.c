/* de:meta
{
  "title": "More Note Bass",
  "status": "active",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer",
    "drum-synthesis",
    "subtractive-synth",
    "swing-timing",
    "scale-quantize"
  ],
  "lineage": "A stripped sibling of drummachine and One Note: the same 16-step paintable grid, but the bass lane opens into five scale-locked rows so a monophonic bassline can walk a pentatonic pocket without leaving the on/off sequencer idiom.",
  "description": "A tiny groovebox that splits the difference between the drum-machine grid and One Note's bass feel. Two drum rows — KICK and SNARE — sit over FIVE bass rows locked to a minor-pentatonic scale, so every step can hold one in-scale bass note without needing a piano roll. The bass stays monophonic by column: light a note and that step becomes that pitch, simple and punchy like a baby 303 sequencer but with honest on/off cells. A starter groove is baked in so it thumps immediately. Tap or drag cells to paint; one octave of GarageBand-style musical-typing keys (A W S E D F T G Y H U J) transposes the bass root chromatically, Z/X shifts that root by octaves, `SUB` folds in an extra oscillator one octave down, and the arrow keys (or on-screen buttons) change tempo and swing."
}
de:meta */
#include "studio.h"
#include "pointer.h"     // multi-finger pool: PTR_MAX/PTR_NONE + PTR_CLEAR/PTR_ACQUIRE/PTR_FIND
#include "ui.h"
#include <math.h>

// MORE NOTE BASS — a tiny groovebox: kick, snare, and a five-note bass lane.
//
// The idea is halfway between drummachine.c and onenote.c:
//   • the same 16-step on/off grid and touch-drag painting as the drum machine
//   • but the bass is not ONE note — it is five rows of a locked scale
//   • every column can hold at most one bass note, so the line stays monophonic
//
// It's intentionally simple: no knobs yet, just the honest skeleton for building a groove.
//
//   TOUCH / MOUSE  tap a cell to toggle; drag across cells to paint
//   A W S E D ...  GarageBand musical-typing map — transpose the bass root chromatically
//   Z / X          bass root octave down / up
//   ← / →          tempo down / up
//   ↑ / ↓          swing down / up

#define ROWS  7
#define STEPS 16

#define GX 54
#define GY 20
#define SX 16
#define SY 12
#define CW 14
#define CH 10

enum {
    R_KICK = 0,
    R_SNARE,
    R_BASS4,   // highest
    R_BASS3,
    R_BASS2,
    R_BASS1,
    R_BASS0,   // lowest
};

enum { SL_KICK = 5, SL_SNARE, SL_BASS };

static const char *LABEL[ROWS] = { "KICK", "SNARE", "A#", "G", "F", "D#", "C" };
static const int   LIT  [ROWS] = {
    CLR_RED, CLR_ORANGE,
    CLR_BLUE, CLR_BLUE, CLR_BLUE, CLR_BLUE, CLR_BLUE
};

// C minor pentatonic across five rows: C, D#, F, G, A#
static const int BASS_SEMI[5] = { 0, 3, 5, 7, 10 };
#define BASS_BASE 36   // C2

static const char *PRESET[ROWS] = {
    "x...x...x...x...",   // kick
    "....x.......x...",   // snare
    "..x.............",   // A#
    "...........x....",   // G
    "......x.........",   // F
    "....x.......x...",   // D#
    "x.......x.......",   // C
};

static bool grid[ROWS][STEPS];
static int  curR = 0, curC = 0;
static int  cur_step = 0;
static int  last_16 = -1;
static int  flash[ROWS];
static int  tempo = 112;
static int  swing = 18;
static float k_tempo = (112 - 70) / 110.0f;   // → 70..180 BPM
static float k_swing = 18 / 60.0f;            // → 0..60 %
static int  last_bass_midi = BASS_BASE;
static int  bass_root = BASS_BASE;
static int  bass_root_flash = -100;
static bool sub = false;   // optional extra oscillator one octave down
static float kick_x = 0.48f, kick_y = 0.60f;   // boom / kick
static float snr_x  = 0.45f, snr_y  = 0.45f;   // tone / snap
static float tone_x = 0.42f, tone_y = 0.40f;   // cutoff / resonance
static float flg_x  = 0.22f, flg_y  = 0.00f;   // speed / amount

typedef struct { int id, paint, lastR, lastC; } Ptr;   // id MUST be first (pointer.h)
static Ptr ptr[PTR_MAX];

#define KB_ROOT_OCT_MIN 1
#define KB_ROOT_OCT_MAX 4
static int kb_root_oct = 2;   // C2 default
static const int KB_KEY[12] = {
    'A','W','S','E','D','F','T','G','Y','H','U','J'
};
#define NKB 12

#define BTN_Y 9
#define BTN_H 9
enum { B_SUB, B_CLEAR, NBTN };
static const struct { int x, w; const char *label; } BTN[NBTN] = {
    { 146, 28, "SUB"   },
    { 286, 30, "CLEAR" },
};

#define PAD_W 146
#define PAD_H 28
#define PAD_GAP_Y 8
#define PAD_ROW1_Y 120
#define PAD_ROW2_Y (PAD_ROW1_Y + PAD_H + PAD_GAP_Y)
#define PAD_L_X 8
#define PAD_R_X 166
#define KICK_X PAD_L_X
#define TONE_X PAD_R_X
#define SNR_X  PAD_L_X
#define FLG_X  PAD_R_X

static bool is_bass_row(int r) { return r >= R_BASS4; }
static int bass_index_from_row(int r) { return R_BASS0 - r; }   // row -> 4..0 maps to C..A#
static int bass_midi_from_row(int r) { return bass_root + BASS_SEMI[bass_index_from_row(r)]; }
static bool in_box(int px, int py, int x, int y, int w, int h) { return px >= x && px < x + w && py >= y && py < y + h; }

static int cell_rc(int x, int y, int *r, int *c) {
    if (x < GX || y < GY) return 0;
    int cc = (x - GX) / SX, rr = (y - GY) / SY;
    if (cc < 0 || cc >= STEPS || rr < 0 || rr >= ROWS) return 0;
    *r = rr; *c = cc; return 1;
}

static void clear_grid(void) {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < STEPS; c++)
            grid[r][c] = false;
}

static void clear_bass_column(int keep_r, int c) {
    for (int r = R_BASS4; r <= R_BASS0; r++)
        if (r != keep_r) grid[r][c] = false;
}

static void set_bass_root(int midi) {
    bass_root = midi;
    kb_root_oct = bass_root / 12 - 1;
    bass_root_flash = frame();
}

static void shift_bass_root_octave(int d) {
    int oct = kb_root_oct + d;
    if (oct < KB_ROOT_OCT_MIN || oct > KB_ROOT_OCT_MAX) return;
    set_bass_root((oct + 1) * 12 + (bass_root % 12));
}

static void apply_kick_shape(void) {
    static float ax = -1, ay = -1;
    if (kick_x == ax && kick_y == ay) return;
    int body_ms   = 140 + (int)(kick_x * 240.0f);    // short punch .. long boom
    int rel_ms    = 40  + (int)(kick_x * 45.0f);
    int pitch_ms  = 24  + (int)(kick_y * 38.0f);
    float pitch_a = 18.0f + kick_y * 18.0f;          // more donk
    int cutoff    = 220 + (int)(kick_x * 140.0f);
    float drive   = 0.05f + kick_y * 0.42f;
    instrument(SL_KICK, INSTR_SINE, 0, body_ms, 0, rel_ms);
    instrument_filter(SL_KICK, FILTER_LOW, cutoff, 2);
    instrument_env(SL_KICK, 1, ENV_PITCH, 0, pitch_ms, pitch_a);
    instrument_drive(SL_KICK, drive);
    ax = kick_x; ay = kick_y;
}

static void apply_snare_shape(void) {
    static float ax = -1, ay = -1;
    if (snr_x == ax && snr_y == ay) return;
    int noise_ms = 70 + (int)(snr_y * 100.0f);       // short crack .. longer tail
    int rel_ms   = 32 + (int)(snr_y * 40.0f);
    int cutoff   = 900 + (int)(snr_x * 1700.0f);     // darker bark .. brighter snap
    int res      = 2 + (int)(snr_y * 4.0f + 0.5f);
    instrument(SL_SNARE, INSTR_NOISE, 0, noise_ms, 0, rel_ms);
    instrument_filter(SL_SNARE, FILTER_BAND, cutoff, res);
    ax = snr_x; ay = snr_y;
}

static void apply_bass_tone(void) {
    static float ax = -1, ay = -1;
    if (tone_x == ax && tone_y == ay) return;
    int cutoff = (int)(120.0f * powf(5000.0f / 120.0f, tone_x));   // 120 Hz → ~5 kHz, log
    int res    = (int)(tone_y * 12.0f + 0.5f);                     // 0..12
    instrument_filter(SL_BASS, FILTER_LADDER, cutoff, res);
    ax = tone_x; ay = tone_y;
}

static void apply_bass_flanger(void) {
    static float ax = -1, ay = -1;
    if (flg_x == ax && flg_y == ay) return;
    float rate  = 0.06f + flg_x * 0.90f;                // keep the sweet zone compact
    float depth = flg_y * 0.34f;
    float fb    = flg_y * 0.22f;
    float mix   = flg_y * 0.30f;
    instrument_flanger(SL_BASS, rate, depth, fb, mix);
    ax = flg_x; ay = flg_y;
}

static void play_row(int r) {
    switch (r) {
        case R_KICK:
            hit(72, INSTR_NOISE, 2 + (int)(kick_y * 2.0f + 0.5f), 12);
            hit(34, SL_KICK, 6, 250);
            break;
        case R_SNARE:
            hit(58, SL_SNARE, 5, 110);
            hit(53, INSTR_TRI, 2 + (int)(snr_y * 3.0f + 0.5f), 35 + (int)(snr_y * 25.0f));
            break;
        default:
            last_bass_midi = bass_midi_from_row(r);
            hit(last_bass_midi, SL_BASS, 5, 140);
            if (sub) hit(last_bass_midi - 12, SL_BASS, 3, 110);
            break;
    }
}

static void set_cell(int r, int c, bool on) {
    if (on && is_bass_row(r)) clear_bass_column(r, c);
    grid[r][c] = on;
    curR = r; curC = c;
    if (on) { play_row(r); flash[r] = frame(); }
}

static const char *NOTE_NAMES[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
static const char *note_name(int midi) { return str("%s%d", NOTE_NAMES[midi % 12], midi / 12 - 1); }
static const char *note_short(int midi) { return NOTE_NAMES[midi % 12]; }

void init(void) {
    instrument(SL_BASS, INSTR_SAW, 1, 180, 2, 110);
    instrument_drive(SL_BASS, 0.18f);
    apply_kick_shape();
    apply_snare_shape();
    apply_bass_tone();
    apply_bass_flanger();

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < STEPS; c++)
            grid[r][c] = (PRESET[r][c] == 'x');

    set_bass_root(BASS_BASE);
    PTR_CLEAR(ptr);
}

void update(void) {
    if (btnp(1, BTN_LEFT))  k_tempo = clamp(k_tempo - (2.0f / 110.0f), 0, 1);
    if (btnp(1, BTN_RIGHT)) k_tempo = clamp(k_tempo + (2.0f / 110.0f), 0, 1);
    if (btnp(1, BTN_UP))    k_swing = clamp(k_swing + (5.0f / 60.0f), 0, 1);
    if (btnp(1, BTN_DOWN))  k_swing = clamp(k_swing - (5.0f / 60.0f), 0, 1);
    tempo = 70 + (int)(k_tempo * 110.0f + 0.5f);
    swing = (int)(k_swing * 60.0f + 0.5f);
    bpm(tempo);

    // GarageBand-style musical typing: press a chromatic key to move the WHOLE
    // bass lane's root, preserving the row intervals.
    for (int i = 0; i < NKB; i++)
        if (keyp(KB_KEY[i])) set_bass_root((kb_root_oct + 1) * 12 + i);
    if (keyp('Z')) shift_bass_root_octave(-1);
    if (keyp('X')) shift_bass_root_octave(+1);

    float pos16 = beat() * 4 + beat_pos() * 4.0f;
    int   n     = (int)pos16;
    int sixteenth = (n % 2 == 1 && pos16 - n < swing / 100.0f) ? n - 1 : n;
    if (sixteenth != last_16) {
        last_16  = sixteenth;
        cur_step = sixteenth % STEPS;
        for (int r = 0; r < ROWS; r++)
            if (grid[r][cur_step]) { play_row(r); flash[r] = frame(); }
    }

    if (keyp('C') || tapp(BTN[B_SUB].x, BTN_Y, BTN[B_SUB].w, BTN_H)) sub = !sub;
    if (tapp(BTN[B_CLEAR].x, BTN_Y, BTN[B_CLEAR].w, BTN_H)) clear_grid();

    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i), r, c;
        if (in_box(tx, ty, KICK_X, PAD_ROW1_Y, PAD_W, PAD_H)) {
            kick_x = clamp((tx - KICK_X) / (float)PAD_W, 0, 1);
            kick_y = clamp(1.0f - (ty - PAD_ROW1_Y) / (float)PAD_H, 0, 1);
            continue;
        }
        if (in_box(tx, ty, TONE_X, PAD_ROW1_Y, PAD_W, PAD_H)) {
            tone_x = clamp((tx - TONE_X) / (float)PAD_W, 0, 1);
            tone_y = clamp(1.0f - (ty - PAD_ROW1_Y) / (float)PAD_H, 0, 1);
            continue;
        }
        if (in_box(tx, ty, SNR_X, PAD_ROW2_Y, PAD_W, PAD_H)) {
            snr_x = clamp((tx - SNR_X) / (float)PAD_W, 0, 1);
            snr_y = clamp(1.0f - (ty - PAD_ROW2_Y) / (float)PAD_H, 0, 1);
            continue;
        }
        if (in_box(tx, ty, FLG_X, PAD_ROW2_Y, PAD_W, PAD_H)) {
            flg_x = clamp((tx - FLG_X) / (float)PAD_W, 0, 1);
            flg_y = clamp(1.0f - (ty - PAD_ROW2_Y) / (float)PAD_H, 0, 1);
            continue;
        }
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;
        if (fresh) {
            if (!cell_rc(tx, ty, &r, &c)) { p->id = PTR_NONE; continue; }
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

    apply_kick_shape();
    apply_snare_shape();
    apply_bass_tone();
    apply_bass_flanger();

#ifdef DE_TRACE
    int lit = 0;
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < STEPS; c++) lit += grid[r][c];
    watch("tempo", "%d", tempo);
    watch("swing", "%d", swing);
    watch("bass",  "%d", last_bass_midi);
    watch("kick",  "%d", (int)(kick_y * 100));
    watch("snare", "%d", (int)(snr_y * 100));
    watch("cutoff","%d", (int)(120.0f * powf(5000.0f / 120.0f, tone_x)));
    watch("flg",   "%d", (int)(flg_y * 100));
    watch("lit",   "%d", lit);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);

    font(FONT_SMALL);
    print("MORE NOTE BASS", 2, 3, CLR_LIGHT_YELLOW);
    print_right(str("%d BPM  SW %d%%", tempo, swing), 140, 3, CLR_WHITE);

    ui_begin();
    ui_knob(&k_tempo, 190, 11, 0);
    ui_knob(&k_swing, 224, 11, 0);
    ui_end();
    print("BPM", 177, 22, CLR_LIGHT_GREY);
    print("SW", 216, 22, CLR_LIGHT_GREY);

    for (int b = 0; b < NBTN; b++) {
        int fill = (b == B_SUB && sub) ? CLR_BLUE : CLR_DARK_GREY;
        int edge = (b == B_SUB && sub) ? CLR_TRUE_BLUE : CLR_MEDIUM_GREY;
        int ink  = (b == B_CLEAR) ? CLR_ORANGE
                 : (b == B_SUB && sub) ? CLR_WHITE
                                       : CLR_LIGHT_GREY;
        rectfill(BTN[b].x, BTN_Y, BTN[b].w, BTN_H, fill);
        rect(BTN[b].x, BTN_Y, BTN[b].w, BTN_H, edge);
        int tw = text_width(BTN[b].label);
        print(BTN[b].label, BTN[b].x + (BTN[b].w - tw) / 2, BTN_Y + 2, ink);
    }

    font(FONT_NORMAL);

    rectfill(GX + cur_step * SX, GY - 4, CW, 2, CLR_WHITE);

    for (int r = 0; r < ROWS; r++) {
        bool lit = (frame() - flash[r]) < 5;
        font(FONT_SMALL);
        const char *label = is_bass_row(r) ? note_short(bass_midi_from_row(r)) : LABEL[r];
        print(label, 4, GY + r * SY + 2, lit ? CLR_WHITE : LIT[r]);
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

    rect(GX + curC * SX - 1, GY + curR * SY - 1, CW + 2, CH + 2, CLR_GREEN);

    bool root_glow = (frame() - bass_root_flash) < 8;
    font(FONT_SMALL);
    print(str("root %s  |  now %s  |  sub %s  |  A..J chromatic  |  Z/X oct  |  %s minor pentatonic",
              note_name(bass_root), note_name(last_bass_midi), sub ? "on" : "off", note_short(bass_root)),
          4, 108, root_glow ? CLR_LIGHT_YELLOW : CLR_LIGHT_GREY);

    // bottom pad block — four XY pads
    rectfill(KICK_X, PAD_ROW1_Y, PAD_W, PAD_H, CLR_DARKER_GREY);
    rect(KICK_X, PAD_ROW1_Y, PAD_W, PAD_H, CLR_MEDIUM_GREY);
    rectfill(TONE_X, PAD_ROW1_Y, PAD_W, PAD_H, CLR_DARKER_GREY);
    rect(TONE_X, PAD_ROW1_Y, PAD_W, PAD_H, CLR_MEDIUM_GREY);
    rectfill(SNR_X, PAD_ROW2_Y, PAD_W, PAD_H, CLR_DARKER_GREY);
    rect(SNR_X, PAD_ROW2_Y, PAD_W, PAD_H, CLR_MEDIUM_GREY);
    rectfill(FLG_X, PAD_ROW2_Y, PAD_W, PAD_H, CLR_DARKER_GREY);
    rect(FLG_X, PAD_ROW2_Y, PAD_W, PAD_H, CLR_MEDIUM_GREY);
    print("KICK", KICK_X + 4, PAD_ROW1_Y + 3, CLR_RED);
    print("BASS TONE", TONE_X + 4, PAD_ROW1_Y + 3, CLR_BLUE);
    print("SNARE", SNR_X + 4, PAD_ROW2_Y + 3, CLR_ORANGE);
    print("BASS FLANGE", FLG_X + 4, PAD_ROW2_Y + 3, CLR_BLUE);
    font(FONT_SMALL);
    print("boom", KICK_X + 4, PAD_ROW1_Y + PAD_H - 9, CLR_LIGHT_GREY);
    print("kick", KICK_X + PAD_W - 24, PAD_ROW1_Y + 3, CLR_LIGHT_GREY);
    print("cut", TONE_X + 4, PAD_ROW1_Y + PAD_H - 9, CLR_LIGHT_GREY);
    print("res", TONE_X + PAD_W - 20, PAD_ROW1_Y + 3, CLR_LIGHT_GREY);
    print("tone", SNR_X + 4, PAD_ROW2_Y + PAD_H - 9, CLR_LIGHT_GREY);
    print("snap", SNR_X + PAD_W - 24, PAD_ROW2_Y + 3, CLR_LIGHT_GREY);
    print("spd", FLG_X + 4, PAD_ROW2_Y + PAD_H - 9, CLR_LIGHT_GREY);
    print("amt", FLG_X + PAD_W - 20, PAD_ROW2_Y + 3, CLR_LIGHT_GREY);
    int kx = KICK_X + (int)(kick_x * PAD_W), ky = PAD_ROW1_Y + (int)((1.0f - kick_y) * PAD_H);
    int tx = TONE_X + (int)(tone_x * PAD_W), ty = PAD_ROW1_Y + (int)((1.0f - tone_y) * PAD_H);
    int sx = SNR_X + (int)(snr_x * PAD_W),  sy = PAD_ROW2_Y + (int)((1.0f - snr_y) * PAD_H);
    int fx = FLG_X + (int)(flg_x * PAD_W),  fy = PAD_ROW2_Y + (int)((1.0f - flg_y) * PAD_H);
    rect(kx - 2, ky - 2, 5, 5, CLR_WHITE);
    rect(tx - 2, ty - 2, 5, 5, CLR_WHITE);
    rect(sx - 2, sy - 2, 5, 5, CLR_WHITE);
    rect(fx - 2, fy - 2, 5, 5, CLR_WHITE);
    line(kx, PAD_ROW1_Y + 12, kx, PAD_ROW1_Y + PAD_H - 4, CLR_DARK_BLUE);
    line(KICK_X + 4, ky, KICK_X + PAD_W - 4, ky, CLR_DARK_BLUE);
    line(tx, PAD_ROW1_Y + 12, tx, PAD_ROW1_Y + PAD_H - 4, CLR_DARK_BLUE);
    line(TONE_X + 4, ty, TONE_X + PAD_W - 4, ty, CLR_DARK_BLUE);
    line(sx, PAD_ROW2_Y + 12, sx, PAD_ROW2_Y + PAD_H - 4, CLR_DARK_BLUE);
    line(SNR_X + 4, sy, SNR_X + PAD_W - 4, sy, CLR_DARK_BLUE);
    line(fx, PAD_ROW2_Y + 12, fx, PAD_ROW2_Y + PAD_H - 4, CLR_DARK_BLUE);
    line(FLG_X + 4, fy, FLG_X + PAD_W - 4, fy, CLR_DARK_BLUE);
    font(FONT_NORMAL);
}
