/* de:meta
{
  "title": "composer (mario paint sound)",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "tool",
    "instrument"
  ],
  "teaches": [
    "step-sequencer",
    "save-load-persistence"
  ],
  "lineage": "Homage to Mario Paint Composer (1992); novel in that each stamp is a distinct synth voice and rows snap to a musical scale via degree(), so everything sounds harmonic by construction.",
  "homage": "Mario Paint (1992)",
  "description": "A melodic music toy in the spirit of Mario Paint's Sound mode. Stamp six cute critters — cat, star, mushroom, note, heart, flower, each its own synth voice (custom ADSR/filter/duty/LFO on slots 5-10) - onto an 11-row x 16-step pitch grid, then hit PLAY and a glowing playhead sweeps the bar singing your tune. Every row snaps to a scale (toggle PENTA/MAJOR) so it always sounds nice; notes squash-pop and flash white as the playhead strikes them. The transport rides the synth's own beat() clock, and your whole song persists between runs via save_bytes(). Mouse: click the grid to stamp the selected critter, click again or right-click to erase, click a critter to pick it; buttons for PLAY/STOP, -/+ tempo, SCALE, CLEAR, SAVE, LOAD. Keys: SPACE play/stop, 1-6 pick critter, C clear, M scale, S save, L load, arrows tempo.",
  "todo": [
    "Some overlapping labels."
  ]
}
de:meta */
#include "studio.h"

// ============================================================
//  COMPOSER  — the Mario Paint "Sound" toy. Stamp cute critters onto a
//  pitch x time grid, hit PLAY, and a glowing playhead sweeps left->right
//  forever, singing your tune. A creative TOOL, not a game: no win/lose,
//  just instantly fun to noodle on.
//
//  The grid is PITCHES (rows, low at the bottom) x STEPS (16 columns, one
//  bar). Every row snaps to a SCALE, so whatever you stamp sounds nice —
//  toggle PENTA (always pretty) / MAJOR. Six instrument STAMPS line the
//  toolbar, each a little drawn critter wired to its own synth voice
//  (custom ADSR/filter/duty/LFO slots). Click the grid to place the
//  selected critter; click it again (or right-click) to erase. Notes
//  bounce and flash white as the playhead strikes them.
//
//  The transport runs off the synth's own beat() clock — no hand timers —
//  exactly like the drum machine, but melodic and multi-octave. The whole
//  song persists between runs via save_bytes().
//
//    Mouse:  click grid = stamp / erase · click a critter = select it
//            PLAY/STOP · -/+ tempo · SCALE · CLEAR · SAVE · LOAD
//    Keys:   SPACE play/stop · 1-6 pick critter · C clear · M scale
//            S save · L load · arrows tempo
// ============================================================

#define ROWS    11        // pitch rows (penta: exactly two octaves, root->root)
#define COLS    16        // sixteenth-note steps in the bar
#define NVOICE  6         // instrument stamps

// ── grid geometry ──
#define GX 22             // grid left edge
#define GY 14             // grid top edge
#define CS 18             // column stride
#define RS 11             // row stride
#define CW 17             // clickable cell width

#define BASE_OCT 3        // lowest row starts here; rows climb the scale

// ── save blob ──
#define MAGIC 0x53504331  // "SPC1"
typedef struct { int magic, scale, tempo, grid[ROWS][COLS]; } Song;

// instrument slots 5..10 — defined in init()
static const int VOICE[NVOICE] = { 5, 6, 7, 8, 9, 10 };
static const int VCOL [NVOICE] = { CLR_ORANGE, CLR_YELLOW, CLR_RED, CLR_GREEN, CLR_PINK, CLR_BLUE };
static const int VDUR [NVOICE] = { 180, 320, 200, 150, 260, 170 };   // hit() duration per voice (ms)
static const char *VNAME[NVOICE] = { "CAT", "STAR", "MUSH", "NOTE", "HEART", "FLWR" };

static int   grid[ROWS][COLS];   // 0 = empty, else (voice index + 1)
static float popT[ROWS][COLS];   // now() a cell was last struck — drives the bounce
static int   sel = 5;            // selected stamp (FLWR — a pretty default)
static int   scale_mode = SCALE_PENTA;
static int   tempo = 110;
static bool  playing = false;
static int   cur_col = 0;        // playhead column
static int   last_16 = -1, base16 = 0;
static float saveMsg = 0;        // "SAVED!"/"LOADED!" flash timer
static const char *saveTxt = "";

// row r (0 = bottom/low) -> MIDI note in the current scale
static int row_midi(int r) { return degree(scale_mode, BASE_OCT, r); }

// ------------------------------------------------------------
// the critters — each stamp is drawn from primitives, big in the
// palette, small in the grid. `col` is the body color (white when struck).
// ------------------------------------------------------------
static void drawStamp(int kind, int cx, int cy, int R, int col) {
    switch (kind) {
        case 0:  // CAT — round head, two ears, two eyes
            trifill(cx-R, cy-R/3, cx-R/3, cy-R/3, cx-R*2/3, cy-R-R/4, col);
            trifill(cx+R, cy-R/3, cx+R/3, cy-R/3, cx+R*2/3, cy-R-R/4, col);
            circfill(cx, cy, R, col);
            if (R >= 4) { pset(cx-R/2, cy-1, CLR_BLACK); pset(cx+R/2, cy-1, CLR_BLACK); }
            break;
        case 1:  // STAR — a four-point sparkle (two crossed ovals)
            ovalfill(cx, cy, R/3 + 1, R, col);
            ovalfill(cx, cy, R, R/3 + 1, col);
            if (R >= 6) pset(cx, cy, CLR_WHITE);
            break;
        case 2:  // MUSHROOM — white stem under a spotted red cap
            rectfill(cx - R/3, cy, R*2/3 + 1, R, CLR_WHITE);
            ovalfill(cx, cy, R, R*2/3, col);
            if (R >= 4) { pset(cx-R/2, cy-2, CLR_WHITE); pset(cx+R/2, cy-1, CLR_WHITE); }
            break;
        case 3:  // NOTE — an eighth note: round head + stem + flag
            circfill(cx - R/3, cy + R/2, R/2 + 1, col);
            line(cx + R/6, cy + R/2, cx + R/6, cy - R, col);
            line(cx + R/6, cy - R, cx + R/2, cy - R/2, col);
            break;
        case 4:  // HEART — two lobes + a point
            circfill(cx - R/2, cy - R/4, R/2 + 1, col);
            circfill(cx + R/2, cy - R/4, R/2 + 1, col);
            trifill(cx - R, cy - R/4, cx + R, cy - R/4, cx, cy + R, col);
            break;
        default: // FLWR — petals around a bright center
            for (int p = 0; p < 5; p++)
                circfill(cx + (int)dx(R*0.7f, p*72), cy + (int)dy(R*0.7f, p*72), R/2, col);
            circfill(cx, cy, R/2, CLR_YELLOW);
            break;
    }
}

// ------------------------------------------------------------
// helpers
// ------------------------------------------------------------
static int col_x(int c) { return GX + c * CS; }
static int row_y(int r) { return GY + (ROWS - 1 - r) * RS; }   // r=0 -> bottom

static void play_cell(int r, int c) {
    int k = grid[r][c] - 1;
    if (k < 0) return;
    hit(row_midi(r), VOICE[k], 5, VDUR[k]);
    popT[r][c] = now();
}

static void clear_grid(void) {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) grid[r][c] = 0;
}

static void seed_default(void) {
    // a gentle penta tune so the toy sings the moment it opens:
    // a flower run up & back, star sparkles on the downbeats, a tail.
    int run[7][2] = { {0,0},{2,2},{4,4},{6,5},{8,4},{10,2},{12,0} };  // {col,row}
    for (int i = 0; i < 7; i++) grid[run[i][1]][run[i][0]] = 5 + 1;   // FLWR
    grid[7][0] = grid[8][4] = grid[7][8] = grid[9][12] = 1 + 1;       // STAR
    grid[3][14] = grid[5][15] = 3 + 1;                                // NOTE tail
    grid[2][3] = grid[2][11] = 4 + 1;                                 // HEART
}

static void save_song(void) {
    Song s = { MAGIC, scale_mode, tempo };
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) s.grid[r][c] = grid[r][c];
    save_bytes(&s, sizeof s);
    saveMsg = 1.6f; saveTxt = "SAVED!";
    note(degree(SCALE_MAJOR, 5, 4), VOICE[1], 5);
}

static bool load_song(void) {
    Song s;
    if (load_bytes(&s, sizeof s) != (int)sizeof s || s.magic != MAGIC) return false;
    scale_mode = (s.scale == SCALE_MAJOR) ? SCALE_MAJOR : SCALE_PENTA;
    tempo = mid(40, s.tempo, 240);
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            int v = s.grid[r][c];
            grid[r][c] = (v >= 0 && v <= NVOICE) ? v : 0;
        }
    return true;
}

// ------------------------------------------------------------
// a generic clickable text button
// ------------------------------------------------------------
static bool ui_btn(int x, int y, int w, int h, const char *label, bool hot, int col) {
    rectfill(x, y, w, h, hot ? col : CLR_DARKER_GREY);
    rect(x, y, w, h, hot ? CLR_WHITE : CLR_DARK_GREY);
    print(label, x + (w - text_width(label)) / 2, y + (h - 6) / 2, hot ? CLR_BLACK : CLR_LIGHT_GREY);
    return mouse_pressed(MOUSE_LEFT) && point_in_box(mouse_x(), mouse_y(), x, y, w, h);
}

// ------------------------------------------------------------
void init(void) {
    // six distinct voices, slots 5..10
    instrument(5,  INSTR_TRI,    2, 120, 2, 200);  instrument_lfo(5, 0, LFO_PITCH, 6, 0.4f);    // CAT  — vibrato meow
    instrument(6,  INSTR_SINE,   1, 240, 0, 340);                                               // STAR — bell / glock
    instrument(7,  INSTR_SQUARE, 2,  90, 0,  90);  instrument_duty(7, 0.5f);
                                                   instrument_filter(7, FILTER_LOW, 700, 3);    // MUSH — warm tuba
    instrument(8,  INSTR_SQUARE, 3,  60, 5, 120);  instrument_duty(8, 0.15f);                   // NOTE — thin lead
    instrument(9,  INSTR_SINE,  70,   0, 7, 380);                                               // HEART— soft swell
    instrument(10, INSTR_SAW,    2, 110, 1, 150);  instrument_filter(10, FILTER_LOW, 1700, 5);  // FLWR — harp pluck

    if (!load_song()) seed_default();
}

// ------------------------------------------------------------
void update(void) {
    bpm(tempo);

    // ── transport off the synth clock; restart the bar at column 0 on PLAY ──
    int now16 = beat() * 4 + (int)(beat_pos() * 4.0f);
    if (playing) {
        if (now16 != last_16) {
            last_16 = now16;
            cur_col = ((now16 - base16) % COLS + COLS) % COLS;
            for (int r = 0; r < ROWS; r++)
                if (grid[r][cur_col]) play_cell(r, cur_col);
        }
    }

    // ── keyboard shortcuts ──
    if (keyp(KEY_SPACE)) { playing = !playing; if (playing) { base16 = now16; last_16 = -1; cur_col = 0; } }
    for (int i = 0; i < NVOICE; i++) if (keyp('1' + i)) sel = i;
    if (keyp('C')) clear_grid();
    if (keyp('M')) scale_mode = (scale_mode == SCALE_PENTA) ? SCALE_MAJOR : SCALE_PENTA;
    if (keyp('S')) save_song();
    if (keyp('L')) { if (load_song()) { saveMsg = 1.6f; saveTxt = "LOADED!"; } }
    if (keyp(KEY_LEFT))  tempo = max(40,  tempo - 4);
    if (keyp(KEY_RIGHT)) tempo = min(240, tempo + 4);

    // ── stamp the grid (place / erase) ──
    int mx = mouse_x(), my = mouse_y();
    bool inGrid = (mx >= GX && mx < GX + COLS * CS && my >= GY && my < GY + ROWS * RS);
    if (inGrid && (mouse_pressed(MOUSE_LEFT) || mouse_pressed(MOUSE_RIGHT))) {
        int c = (mx - GX) / CS;
        int r = ROWS - 1 - (my - GY) / RS;
        if (c >= 0 && c < COLS && r >= 0 && r < ROWS) {
            if (mouse_pressed(MOUSE_RIGHT) || grid[r][c] == sel + 1) {
                grid[r][c] = 0;                              // erase
            } else {
                grid[r][c] = sel + 1;                        // place + audition
                play_cell(r, c);
            }
        }
    }

    if (saveMsg > 0) saveMsg -= dt();
}

// ------------------------------------------------------------
void draw(void) {
    cls(CLR_DARKER_BLUE);

    // ── top bar ──
    rectfill(0, 0, SCREEN_W, 12, CLR_DARK_BLUE);
    print("\x0e COMPOSER", 4, 3, CLR_LIGHT_PEACH);
    print_right("click=stamp  SPACE=play  1-6=pick", SCREEN_W - 4, 3, CLR_INDIGO);

    // ── grid background: measure striping + octave guide lines ──
    int step = (scale_mode == SCALE_PENTA) ? 5 : 7;   // rows per octave
    for (int c = 0; c < COLS; c++) {
        int bg = ((c / 4) % 2 == 0) ? CLR_DARKER_PURPLE : CLR_BROWNISH_BLACK;
        rectfill(col_x(c), GY, CW, ROWS * RS, bg);
    }
    for (int r = 0; r < ROWS; r++)
        if (r % step == 0)                             // octave roots — a brighter floor line
            rectfill(GX, row_y(r) + RS - 1, COLS * CS, 1, CLR_DARK_GREY);

    // ── playhead column glow (only while playing) ──
    if (playing) {
        rectfill(col_x(cur_col), GY, CW, ROWS * RS, CLR_INDIGO);
        rectfill(col_x(cur_col), GY, 2, ROWS * RS, CLR_WHITE);
        trifill(col_x(cur_col) + CW/2 - 3, GY - 4, col_x(cur_col) + CW/2 + 3, GY - 4,
                col_x(cur_col) + CW/2, GY - 1, CLR_WHITE);
    }

    // ── the notes — bounce & flash when freshly struck ──
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            if (!grid[r][c]) continue;
            int k = grid[r][c] - 1;
            int cx = col_x(c) + CW/2, cy = row_y(r) + RS/2;
            float k01 = clamp(1.0f - (now() - popT[r][c]) / 0.30f, 0.0f, 1.0f);  // 1 just struck -> 0
            int R = 4 + (int)(3 * k01);                   // pop bigger on the strike
            drawStamp(k, cx, cy, R, k01 > 0.55f ? CLR_WHITE : VCOL[k]);
        }

    rect(GX - 2, GY - 2, COLS * CS + 3, ROWS * RS + 3, CLR_DARK_GREY);

    // ── toolbar separator ──
    line(0, 136, SCREEN_W, 136, CLR_DARK_BLUE);

    // ── stamp palette (left) — click or 1-6 ──
    for (int i = 0; i < NVOICE; i++) {
        int bx = 4 + i * 28, by = 140, bw = 26, bh = 30;
        bool on = (i == sel);
        rectfill(bx, by, bw, bh, on ? CLR_DARK_BLUE : CLR_DARKER_GREY);
        rect(bx, by, bw, bh, on ? CLR_WHITE : CLR_DARK_GREY);
        // the selected critter bobs a little
        int bob = on ? (int)(2 * sin_deg(now() * 360)) : 0;
        drawStamp(i, bx + bw/2, by + 13 + bob, 9, VCOL[i]);
        print(str("%d", i + 1), bx + 2, by + bh - 8, on ? CLR_YELLOW : CLR_DARK_GREY);
        if (mouse_pressed(MOUSE_LEFT) && point_in_box(mouse_x(), mouse_y(), bx, by, bw, bh)) sel = i;
    }
    print(VNAME[sel], 4, 174, VCOL[sel]);

    // ── transport (right) ──
    if (ui_btn(176, 138, 138, 18, playing ? "STOP" : "\x10 PLAY", playing, CLR_GREEN)) {
        playing = !playing;
        if (playing) { base16 = beat() * 4 + (int)(beat_pos() * 4.0f); last_16 = -1; cur_col = 0; }
    }

    if (ui_btn(176, 160, 22, 16, "-", false, CLR_BLUE)) tempo = max(40,  tempo - 4);
    if (ui_btn(292, 160, 22, 16, "+", false, CLR_BLUE)) tempo = min(240, tempo + 4);
    const char *bt = str("%d BPM", tempo);                 // tempo readout between the -/+ buttons
    rectfill(200, 160, 90, 16, CLR_DARKER_BLUE);
    print(bt, 200 + (90 - text_width(bt)) / 2, 164, CLR_WHITE);

    if (ui_btn(176, 180, 34, 16, scale_mode == SCALE_PENTA ? "PENTA" : "MAJOR", scale_mode == SCALE_MAJOR, CLR_PINK))
        scale_mode = (scale_mode == SCALE_PENTA) ? SCALE_MAJOR : SCALE_PENTA;
    if (ui_btn(212, 180, 28, 16, "CLR",  false, CLR_RED))    clear_grid();
    if (ui_btn(242, 180, 34, 16, "SAVE", false, CLR_ORANGE)) save_song();
    if (ui_btn(278, 180, 36, 16, "LOAD", false, CLR_YELLOW)) { if (load_song()) { saveMsg = 1.6f; saveTxt = "LOADED!"; } }

    if (saveMsg > 0) print_centered(saveTxt, SCREEN_W/2, 124, CLR_GREEN);
}
