/* de:meta
{
  "title": "omnichord",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "chord-voicing",
    "step-sequencer"
  ],
  "lineage": "Tribute to the Suzuki Omnichord (1981); novel in the chord-extension combo logic (same-root multi-hold → maj7/m7/dim/aug) and the rainbow strumplate glissando mapped across six octaves.",
  "description": "A tribute to the Suzuki Omnichord (1981) — an electronic autoharp — with a built-in drum machine. Two views toggle from the top bar (or TAB). OMNI: pick a chord from the grid (rows = major/minor/7th, columns = the 12 roots) with the mouse OR the computer keyboard — three rows, one per quality: the top row QWERTYUIOP[] plays major, the home row ASDFGHJKL;'\\ plays minor, the bottom row `ZXCVBNM,./ plays 7ths, each key a root from C up — and just like the real Omnichord, holding two keys of the same root gives an extended chord (major+7th = maj7, minor+7th = m7, major+minor = diminished, all three = augmented). Then sweep the rainbow SONIC STRINGS strumplate (mouse or finger) to play it as a harp glissando spanning six octaves. RHYTHM: a 16-step drum sequencer (kick/snare/hat/clap, musical noise voices) whose BASS row follows your selected chord root, with preset grooves from the home-organ days — rock, bossa nova, cha-cha, swing, disco, march (keys 1-6 or click). The beat keeps playing while you strum over it; PLAY/STOP (or SPACE) starts it, arrows set tempo. The strings are a soft plucked bell with a warm lowpass."
}
de:meta */
#include "studio.h"

// ============================================================
//  OMNICHORD  — a tribute to the Suzuki Omnichord (1981), with a built-in
//  drum machine you can flip to. Two views share one top bar:
//
//    OMNI   — pick a CHORD (grid: rows = maj/min/7th, cols = 12 roots) with
//             a tap OR the computer keyboard, then sweep the SONIC STRINGS
//             strumplate to play it as a harp glissando across the octaves.
//             MULTITOUCH: every finger strums its own glissando, and you can
//             tap chords mid-strum. HOLD a second button of the same root to
//             extend the chord (maj+7 = maj7, min+7 = m7, maj+min = dim, all
//             three = aug) — same combos as the keyboard. Desktop mouse works
//             everywhere too (the runtime exposes it as one synthetic finger).
//    RHYTHM — a 16-step drum sequencer (kick/snare/hat/clap + a BASS row that
//             follows your selected chord root). Borrowed from the drummachine
//             cart: musical noise voices, transport off the synth clock.
//
//  The sequencer keeps playing in BOTH views, so you program a beat and strum
//  over it. TAB switches view, SPACE plays/stops, the tabs up top are clickable.
//
//  Keyboard chords (OMNI):  A W S E D F T G Y H U J  = the 12 roots (a piano
//  octave laid on the keys),  1 / 2 / 3 = major / minor / 7th.
// ============================================================

#define OMNI    0
#define RHYTHM  1

// ── omnichord ──
#define PLATE_X  6
#define PLATE_Y  18
#define PLATE_W  (SCREEN_W - 12)
#define PLATE_H  102
#define GRID_X   8
#define COLW     25
#define GRID_Y   126
#define ROWH     19
#define HARP     5

// ── drum sequencer ──
#define ROWS   6
#define STEPS  16
#define GX     46
#define GY     26
#define SX     17
#define SY     21
#define CW     15
#define CH     18

const char *NOTE[12]   = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
const int   STRCOL[7]  = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_INDIGO, CLR_PINK };
// three keyboard rows, each a chord quality across the 12 roots (C..B).
// top row = major, home row = minor, bottom row = 7th. the bottom row's 12th
// root (B7) lands on RIGHT SHIFT (raylib KEY_RIGHT_SHIFT = 344).
#define KEY_RSHIFT 344
const int   MAJKEY [12] = { 'Q','W','E','R','T','Y','U','I','O','P','[',']' };
const int   MINKEY [12] = { 'A','S','D','F','G','H','J','K','L',';','\'','\\' };
const int   DOM7KEY[12] = { '`','Z','X','C','V','B','N','M',',','.','/', KEY_RSHIFT };

const char *LABEL[ROWS] = { "KICK", "SNARE", "HIHAT", "OHAT", "CLAP", "BASS" };
const int   LIT  [ROWS] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_LIGHT_YELLOW, CLR_PINK, CLR_BLUE };

// preset grooves — the kind a home organ's auto-rhythm had. Rows are
// KICK / SNARE / HIHAT / OHAT / CLAP / BASS, 16 sixteenth-notes per bar.
#define NPRESET 6
const char *PNAME[NPRESET] = { "ROCK", "BOSSA", "CHA", "SWING", "DISCO", "MARCH" };
const char *PATTERN[NPRESET][ROWS] = {
    {   // ROCK — backbeat, straight eighth hats
        "x...x...x..x....",
        "....x.......x...",
        "x.x.x.x.x.x.x.x.",
        "................",
        "................",
        "x.......x.......",
    },
    {   // BOSSA NOVA — son-clave rim, gentle surdo kick
        "x..x..x...x..x..",
        "................",
        "x.x.x.x.x.x.x.x.",
        "................",
        "..x.x...x..x.x..",
        "x..x..x...x..x..",
    },
    {   // CHA-CHA — the cha-cha-cha lands on the 4-and-1
        "x...x...x...x...",
        "....x.......x...",
        "x.x.x.x.x.x.x.x.",
        "................",
        "............x.xx",
        "x...x...x...x...",
    },
    {   // SWING — shuffled triplet feel, walking-ish bass
        "x.......x.......",
        "....x.......x...",
        "x..x..x..x..x..x",
        "................",
        "................",
        "x..x..x..x..x..x",
    },
    {   // DISCO — four-on-the-floor, offbeat open hats, clap backbeat
        "x...x...x...x...",
        "................",
        "x.x.x.x.x.x.x.x.",
        "..x...x...x...x.",
        "....x.......x...",
        "x.x.x.x.x.x.x.x.",
    },
    {   // MARCH — kick on the beat, snare upbeats
        "x...x...x...x...",
        "..x...x...x...x.",
        "................",
        "................",
        "................",
        "x...x...x...x...",
    },
};
int curPreset = 0;

int   view = OMNI;

// omnichord state
int   selRoot  = 0;
int   selChord = CHORD_MAJ;     // the live chord type — extended ones via 2-key holds
int   strNote[20], nStr = 0;
float litT[20];

// per-finger strum state — each finger on the plate sweeps its own glissando
#define NFINGER  10
#define NOFINGER (-999)
int   strId  [NFINGER];         // touch id owning this slot, or NOFINGER
int   strLast[NFINGER];         // last string that finger struck (-1 = just landed)

// sequencer state
bool  grid[ROWS][STEPS];
int   curR = 0, curC = 0, cur_step = 0, last_16 = -1;
int   flash[ROWS];
int   tempo = 96;
bool  playing = false;

// chord type → semitone intervals; returns how many notes
int chordIvals(int ch, int *out) {
    switch (ch) {
        case CHORD_MIN:  out[0]=0; out[1]=3; out[2]=7;            return 3;
        case CHORD_DOM7: out[0]=0; out[1]=4; out[2]=7; out[3]=10; return 4;
        case CHORD_MAJ7: out[0]=0; out[1]=4; out[2]=7; out[3]=11; return 4;
        case CHORD_MIN7: out[0]=0; out[1]=3; out[2]=7; out[3]=10; return 4;
        case CHORD_DIM:  out[0]=0; out[1]=3; out[2]=6;            return 3;
        case CHORD_AUG:  out[0]=0; out[1]=4; out[2]=8;            return 3;
        default:         out[0]=0; out[1]=4; out[2]=7;            return 3;  // MAJ
    }
}
const char *chordName(int ch) {
    switch (ch) {
        case CHORD_MIN:  return "min";
        case CHORD_DOM7: return "7th";
        case CHORD_MAJ7: return "maj7";
        case CHORD_MIN7: return "m7";
        case CHORD_DIM:  return "dim";
        case CHORD_AUG:  return "aug";
        default:         return "MAJ";
    }
}
// does grid row r (0=maj, 1=min, 2=7th) light up for chord ch?
bool rowActive(int ch, int r) {
    switch (ch) {
        case CHORD_MIN:  return r == 1;
        case CHORD_DOM7: return r == 2;
        case CHORD_MAJ7: return r == 0 || r == 2;
        case CHORD_MIN7: return r == 1 || r == 2;
        case CHORD_DIM:  return r == 0 || r == 1;
        case CHORD_AUG:  return true;
        default:         return r == 0;  // MAJ
    }
}

void buildStrings() {
    int base = 36 + selRoot;        // start low (C2) — the plate spans the full range
    int iv[4]; int ivn = chordIvals(selChord, iv);
    nStr = 0;
    for (int oct = 0; oct < 6 && nStr < 20; oct++)   // ~6 octaves of strings
        for (int k = 0; k < ivn && nStr < 20; k++)
            strNote[nStr++] = base + 12 * oct + iv[k];
}

int stringAt(int x) {
    if (x < PLATE_X || x > PLATE_X + PLATE_W || nStr == 0) return -1;
    return mid(0, (x - PLATE_X) * nStr / PLATE_W, nStr - 1);
}
void strike(int k) {
    if (k < 0 || k >= nStr) return;
    note(strNote[k], HARP, 5);
    litT[k] = now();
}
void playChord() { strum(48 + selRoot, selChord, HARP, 5, 22); }

void selectChord(int root, int chord) {
    selRoot = root; selChord = chord;
    buildStrings();
    playChord();
}

// fire one drum voice — different midi/instrument/decay per row (the BASS row
// follows whatever chord the omnichord has selected)
void play_row(int r) {
    switch (r) {
        case 0: hit(36, INSTR_TRI,    6, 100); break;   // kick
        case 1: hit(55, INSTR_NOISE,  5, 120); break;   // snare
        case 2: hit(84, INSTR_NOISE,  3,  28); break;   // hihat
        case 3: hit(84, INSTR_NOISE,  2, 170); break;   // open hat
        case 4: hit(64, INSTR_NOISE,  4,  60); break;   // clap
        case 5: hit(36 + selRoot, INSTR_SQUARE, 4, 110); break;  // bass = chord root
    }
}

void loadPreset(int p) {
    curPreset = p;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < STEPS; c++)
            grid[r][c] = (PATTERN[p][r][c] == 'x');
}

void init() {
    instrument(HARP, INSTR_TRI, 1, 180, 1, 280);     // sonic strings: soft plucked bell
    instrument_filter(HARP, FILTER_LOW, 2200, 4);
    for (int k = 0; k < NFINGER; k++) strId[k] = NOFINGER;
    buildStrings();
    loadPreset(0);
}

void update() {
    bpm(tempo);

    // ── global: transport (runs in both views), view + play toggles ──
    if (playing) {
        int sixteenth = beat() * 4 + (int)(beat_pos() * 4.0f);
        if (sixteenth != last_16) {
            last_16  = sixteenth;
            cur_step = sixteenth % STEPS;
            for (int r = 0; r < ROWS; r++)
                if (grid[r][cur_step]) { play_row(r); flash[r] = frame(); }
        }
    } else {
        last_16 = -1;
    }

    if (keyp(KEY_TAB))   view = !view;
    if (keyp(KEY_SPACE)) playing = !playing;

    // tappable top bar: view tabs + play/stop (tapp = any finger; the desktop
    // mouse arrives as a synthetic touch, so it taps too)
    if (tapp(92,  1, 44, 12)) view = OMNI;
    if (tapp(140, 1, 58, 12)) view = RHYTHM;
    if (tapp(204, 1, 44, 12)) playing = !playing;

    if (view == OMNI) {
        // keyboard chords: three rows (major / minor / 7th). Holding two keys of
        // the SAME root gives an extended chord, just like the real Omnichord:
        //   maj+7 = maj7,  min+7 = m7,  maj+min = dim,  all three = aug.
        for (int r = 0; r < 12; r++) {
            if (keyp(MAJKEY[r]) || keyp(MINKEY[r]) || keyp(DOM7KEY[r])) {
                bool hm = key(MAJKEY[r]), hn = key(MINKEY[r]), h7 = key(DOM7KEY[r]);
                int ch = hm && hn && h7 ? CHORD_AUG
                       : hm && h7       ? CHORD_MAJ7
                       : hn && h7       ? CHORD_MIN7
                       : hm && hn       ? CHORD_DIM
                       : h7             ? CHORD_DOM7
                       : hn             ? CHORD_MIN
                       :                  CHORD_MAJ;
                selectChord(r, ch);
            }
        }

        // tap a chord button — fingers still HELD on other rows of the same root
        // extend the chord, mirroring the 2-key keyboard combos above
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 12; c++)
                if (tapp(GRID_X + c * COLW, GRID_Y + r * ROWH, COLW - 1, ROWH - 2)) {
                    bool hm = tap(GRID_X + c * COLW, GRID_Y + 0 * ROWH, COLW - 1, ROWH - 2);
                    bool hn = tap(GRID_X + c * COLW, GRID_Y + 1 * ROWH, COLW - 1, ROWH - 2);
                    bool h7 = tap(GRID_X + c * COLW, GRID_Y + 2 * ROWH, COLW - 1, ROWH - 2);
                    int ch = hm && hn && h7 ? CHORD_AUG
                           : hm && h7       ? CHORD_MAJ7
                           : hn && h7       ? CHORD_MIN7
                           : hm && hn       ? CHORD_DIM
                           : h7             ? CHORD_DOM7
                           : hn             ? CHORD_MIN
                           :                  CHORD_MAJ;
                    selectChord(c, ch);
                }

        // strumplate: EVERY finger sweeps its own glissando (desktop mouse = one finger)
        for (int i = 0; i < touch_count(); i++) {
            int id = touch_id(i);
            int f = -1, freeSlot = -1;
            for (int k = 0; k < NFINGER; k++) {
                if (strId[k] == id) { f = k; break; }
                if (strId[k] == NOFINGER && freeSlot < 0) freeSlot = k;
            }
            if (touch_y(i) < PLATE_Y || touch_y(i) > PLATE_Y + PLATE_H) {
                if (f >= 0) strId[f] = NOFINGER;       // slid off the plate
                continue;
            }
            int s = stringAt(touch_x(i));
            if (s < 0) continue;
            if (f < 0) {                               // finger landed on the plate
                if (freeSlot < 0) continue;
                f = freeSlot; strId[f] = id; strLast[f] = -1;
            }
            if (strLast[f] < 0) strike(s);
            else if (s != strLast[f]) {                // sweep every string crossed
                int d = sgn(s - strLast[f]);
                for (int k = strLast[f] + d; ; k += d) { strike(k); if (k == s) break; }
            }
            strLast[f] = s;
        }
        for (int i = 0; i < touch_ended_count(); i++)  // lifted fingers free their slot
            for (int k = 0; k < NFINGER; k++)
                if (strId[k] == touch_ended_id(i)) strId[k] = NOFINGER;
    } else {
        // ── RHYTHM view: edit the grid (WASD/Z/X + mouse), arrows = tempo ──
        if (btnp(0, BTN_UP))    curR = (curR + ROWS - 1) % ROWS;
        if (btnp(0, BTN_DOWN))  curR = (curR + 1) % ROWS;
        if (btnp(0, BTN_LEFT))  curC = (curC + STEPS - 1) % STEPS;
        if (btnp(0, BTN_RIGHT)) curC = (curC + 1) % STEPS;
        if (btnp(0, BTN_A)) {
            grid[curR][curC] = !grid[curR][curC];
            if (grid[curR][curC]) { play_row(curR); flash[curR] = frame(); }
        }
        if (btnp(0, BTN_B))
            for (int r = 0; r < ROWS; r++)
                for (int c = 0; c < STEPS; c++) grid[r][c] = false;

        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < STEPS; c++)
                if (tapp(GX + c * SX, GY + r * SY, CW, CH)) {
                    curR = r; curC = c;
                    grid[r][c] = !grid[r][c];
                    if (grid[r][c]) { play_row(r); flash[r] = frame(); }
                }

        if (btnp(1, BTN_LEFT))  tempo = max(60,  tempo - 4);
        if (btnp(1, BTN_RIGHT)) tempo = min(220, tempo + 4);

        // load a preset groove — number keys 1..6 or the buttons along the bottom
        for (int p = 0; p < NPRESET; p++)
            if (keyp('1' + p) || tapp(4 + p * 52, 180, 50, 15)) loadPreset(p);
    }
}

// draw one clickable top-bar tab
void drawTab(int bx, int bw, const char *label, bool active) {
    rectfill(bx, 1, bw, 12, active ? CLR_ORANGE : CLR_DARKER_PURPLE);
    rect(bx, 1, bw, 12, active ? CLR_WHITE : CLR_MAUVE);
    print(label, bx + (bw - text_width(label)) / 2, 4, active ? CLR_BLACK : CLR_LIGHT_GREY);
}

void drawTopBar() {
    print("OMNICHORD", 6, 4, CLR_LIGHT_PEACH);
    drawTab(92,  44, "OMNI",   view == OMNI);
    drawTab(140, 58, "RHYTHM", view == RHYTHM);
    drawTab(204, 44, playing ? "STOP" : "PLAY", playing);   // clickable play/stop
    print_right(str("%d BPM", tempo), SCREEN_W - 4, 4, CLR_LIGHT_GREY);
}

void drawOmni() {
    // strumplate panel (dithered = the backlit-plate look)
    fillp(FILL_CHECKER, CLR_DARKER_PURPLE);
    rectfill(PLATE_X, PLATE_Y, PLATE_W, PLATE_H, CLR_DARK_BLUE);
    fillp_reset();
    rect(PLATE_X, PLATE_Y, PLATE_W, PLATE_H, CLR_INDIGO);

    for (int k = 0; k < nStr; k++) {
        int x   = PLATE_X + (k * 2 + 1) * PLATE_W / (nStr * 2);
        int col = STRCOL[k % 7];
        bool lit = now() - litT[k] < 0.22f;
        line(x, PLATE_Y + 3, x, PLATE_Y + PLATE_H - 3, lit ? CLR_WHITE : col);
        if (lit) {
            line(x - 1, PLATE_Y + 3, x - 1, PLATE_Y + PLATE_H - 3, col);
            line(x + 1, PLATE_Y + 3, x + 1, PLATE_Y + PLATE_H - 3, col);
        }
    }
    for (int i = 0; i < touch_count(); i++)        // a cursor under every strumming finger
        if (touch_y(i) >= PLATE_Y && touch_y(i) <= PLATE_Y + PLATE_H)
            circfill(touch_x(i), touch_y(i), 3, CLR_WHITE);

    print(str("%s %s", NOTE[selRoot], chordName(selChord)), PLATE_X + 5, PLATE_Y + 4, CLR_WHITE);
    print("SONIC STRINGS  ~  strum me", PLATE_X + 5, PLATE_Y + PLATE_H - 11, CLR_INDIGO);

    // chord button grid
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 12; c++) {
            int bx = GRID_X + c * COLW, by = GRID_Y + r * ROWH;
            bool sel = (c == selRoot && rowActive(selChord, r));
            rectfill(bx, by, COLW - 1, ROWH - 2, sel ? CLR_ORANGE : CLR_DARKER_PURPLE);
            rect(bx, by, COLW - 1, ROWH - 2, sel ? CLR_WHITE : CLR_MAUVE);
            const char *sfx = r == 1 ? "m" : (r == 2 ? "7" : "");
            print(str("%s%s", NOTE[c], sfx), bx + 3, by + 5, sel ? CLR_BLACK : CLR_LIGHT_GREY);
        }

    print("rows: major / minor / 7th", 4, SCREEN_H - 16, CLR_DARK_GREY);
    print("hold 2 keys = maj7 m7 dim aug", 4, SCREEN_H - 8, CLR_INDIGO);
}

void drawRhythm() {
    rectfill(GX + cur_step * SX, GY - 4, CW, 2, playing ? CLR_WHITE : CLR_DARK_GREY);
    for (int r = 0; r < ROWS; r++) {
        bool lit = (frame() - flash[r]) < 5;
        print(LABEL[r], 2, GY + r * SY + 6, lit ? CLR_WHITE : LIT[r]);
        for (int c = 0; c < STEPS; c++) {
            int x = GX + c * SX, y = GY + r * SY;
            if (grid[r][c]) {
                rectfill(x, y, CW, CH, LIT[r]);
                if (c == cur_step && playing) rect(x, y, CW, CH, CLR_WHITE);
            } else {
                int bg = (c == cur_step && playing) ? CLR_DARK_GREY
                       : (c % 4 == 0)               ? CLR_DARKER_BLUE
                                                    : CLR_DARKER_GREY;
                rectfill(x, y, CW, CH, bg);
            }
        }
    }
    rect(GX + curC * SX - 1, GY + curR * SY - 1, CW + 2, CH + 2, CLR_GREEN);
    print("WASD move   Z toggle   X clear", 2, GY + ROWS * SY + 6, CLR_LIGHT_GREY);
    print("arrows tempo   SPACE play   TAB omni", 2, GY + ROWS * SY + 18, CLR_DARK_GREY);

    // preset grooves — 1..6 / click
    for (int p = 0; p < NPRESET; p++) {
        int bx = 4 + p * 52;
        bool on = (p == curPreset);
        rectfill(bx, 180, 50, 15, on ? CLR_BLUE : CLR_DARKER_GREY);
        rect(bx, 180, 50, 15, on ? CLR_WHITE : CLR_DARK_GREY);
        print(str("%d%s", p + 1, PNAME[p]), bx + 2, 184, on ? CLR_WHITE : CLR_LIGHT_GREY);
    }
}

void draw() {
    cls(view == OMNI ? CLR_DARKER_BLUE : CLR_BROWNISH_BLACK);
    drawTopBar();
    if (view == OMNI) drawOmni();
    else              drawRhythm();
}
