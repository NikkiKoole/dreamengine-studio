#include "studio.h"

// drum machine — a 16-step sequencer that shows off the sound API.
//
// The whole thing is one idea: a grid of on/off cells. Each ROW is an
// instrument, each of the 16 COLUMNS is a sixteenth-note in the bar. A
// playhead sweeps left-to-right forever; when it lands on a lit cell, that
// row's drum fires.
//
// The clock comes straight from the synth's own beat counter. beat() counts
// quarter-notes at the current bpm, beat_pos() is the fraction inside the
// beat — so beat()*4 + floor(beat_pos()*4) is a sixteenth-note counter. We
// trigger sounds the instant that number changes. No timers of our own.
//
//   WASD       move the cursor
//   Z          toggle the cell under the cursor (and audition it)
//   X          clear the whole grid
//   ← / →      tempo down / up (the arrow keys are "player 1")

#define ROWS  6
#define STEPS 16

#define GX 46   // grid left edge
#define GY 28   // grid top edge
#define SX 17   // column stride (px)
#define SY 21   // row stride (px)
#define CW 15   // cell width
#define CH 18   // cell height

static const char *LABEL[ROWS] = { "KICK", "SNARE", "HIHAT", "OHAT", "CLAP", "BASS" };
static const int   LIT  [ROWS] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_LIGHT_YELLOW, CLR_PINK, CLR_BLUE };

// a starter groove so it sounds good the moment it loads
static const char *PRESET[ROWS] = {
    "x...x...x...x...",   // kick — four on the floor
    "....x.......x...",   // snare — backbeat
    "x.x.x.x.x.x.x.x.",   // closed hat — eighths
    "..............x.",   // open hat — lift before the loop
    "................",   // clap — empty, for you to fill in
    "x.....x.x.....x.",   // bass — a little bounce
};

static bool grid[ROWS][STEPS];
static int  curR = 0, curC = 0;       // cursor cell
static int  cur_step = 0;             // column the playhead is on
static int  last_16 = -1;             // last sixteenth-note we triggered on
static int  flash[ROWS];              // frame() when each row last fired
static int  tempo = 120;

// fire one row's drum — different midi/instrument/decay per voice
static void play_row(int r) {
    switch (r) {
        case 0: hit(36, INSTR_TRI,    6, 100); break;  // kick   — low triangle thump
        case 1: hit(55, INSTR_NOISE,  5, 120); break;  // snare  — mid noise
        case 2: hit(84, INSTR_NOISE,  3,  28); break;  // hihat  — short noise tick
        case 3: hit(84, INSTR_NOISE,  2, 170); break;  // o.hat  — long noise wash
        case 4: hit(64, INSTR_NOISE,  4,  60); break;  // clap   — snappy noise
        case 5: hit(40, INSTR_SQUARE, 4, 110); break;  // bass   — tonal square
    }
}

void init() {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < STEPS; c++)
            grid[r][c] = (PRESET[r][c] == 'x');
}

void update() {
    bpm(tempo);

    // ── transport: advance the playhead off the synth clock ──
    int sixteenth = beat() * 4 + (int)(beat_pos() * 4.0f);
    if (sixteenth != last_16) {
        last_16  = sixteenth;
        cur_step = sixteenth % STEPS;
        for (int r = 0; r < ROWS; r++)
            if (grid[r][cur_step]) { play_row(r); flash[r] = frame(); }
    }

    // ── edit: move cursor (WASD), toggle (Z), clear (X) ──
    if (btnp(0, BTN_UP))    curR = (curR + ROWS - 1) % ROWS;
    if (btnp(0, BTN_DOWN))  curR = (curR + 1) % ROWS;
    if (btnp(0, BTN_LEFT))  curC = (curC + STEPS - 1) % STEPS;
    if (btnp(0, BTN_RIGHT)) curC = (curC + 1) % STEPS;

    if (btnp(0, BTN_A)) {
        grid[curR][curC] = !grid[curR][curC];
        if (grid[curR][curC]) { play_row(curR); flash[curR] = frame(); }  // audition
    }
    if (btnp(0, BTN_B))
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < STEPS; c++) grid[r][c] = false;

    // ── tempo: arrow keys are "player 1" ──
    if (btnp(1, BTN_LEFT))  tempo = max(60,  tempo - 5);
    if (btnp(1, BTN_RIGHT)) tempo = min(240, tempo + 5);
}

void draw() {
    cls(CLR_BROWNISH_BLACK);

    print("DRUM MACHINE", 2, 4, CLR_LIGHT_YELLOW);
    print_right(str("%d BPM", tempo), 318, 4, CLR_WHITE);

    // playhead marker above the active column
    rectfill(GX + cur_step * SX, GY - 4, CW, 2, CLR_WHITE);

    for (int r = 0; r < ROWS; r++) {
        // row label, brightened to white for a few frames when it fires
        bool lit = (frame() - flash[r]) < 5;
        print(LABEL[r], 2, GY + r * SY + 6, lit ? CLR_WHITE : LIT[r]);

        for (int c = 0; c < STEPS; c++) {
            int x = GX + c * SX, y = GY + r * SY;
            if (grid[r][c]) {
                // active cell — full color, white flash on the beat it plays
                rectfill(x, y, CW, CH, LIT[r]);
                if (c == cur_step) rect(x, y, CW, CH, CLR_WHITE);
            } else {
                // empty cell — playhead column lights up grey, downbeats tinted
                int bg = (c == cur_step) ? CLR_DARK_GREY
                       : (c % 4 == 0)    ? CLR_DARKER_BLUE
                                         : CLR_DARKER_GREY;
                rectfill(x, y, CW, CH, bg);
            }
        }
    }

    // cursor — a bright outline around the selected cell
    rect(GX + curC * SX - 1, GY + curR * SY - 1, CW + 2, CH + 2, CLR_GREEN);

    print("WASD move   Z toggle   X clear", 2, GY + ROWS * SY + 6, CLR_LIGHT_GREY);
    print("arrow keys: tempo", 2, GY + ROWS * SY + 18, CLR_DARK_GREY);
}
