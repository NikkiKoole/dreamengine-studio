/* de:meta
{
  "title": "Wordle",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "save-load-persistence"
  ],
  "lineage": "Faithful port of the NYT Wordle mechanic; duplicate-letter scoring rule and per-letter keyboard state are the notable implementation challenges.",
  "genre": "puzzle",
  "homage": "Wordle (2021)",
  "description": "The daily word puzzle, distilled to its purest loop. A hidden five-letter word waits behind six empty rows; type a guess and watch the five tiles flip one-by-one — green for a letter in the right spot, orange for one that's in the word but misplaced, gray for absent — with the proper duplicate-letter rule so repeated letters score honestly. A live on-screen QWERTY keyboard repaints each key to the best news it has earned, an invalid or short guess buzzes and shakes the board, and a solve sets the winning row bouncing over a rising arpeggio while a loss reveals the answer. Your win streak persists between runs. Type A-Z to fill the row, BACKSPACE to delete, ENTER to submit (or click the on-screen keyboard, including its ENT and DEL pads); ENTER on the result screen plays a fresh word."
}
de:meta */
#include "studio.h"
#include <string.h>
#include <stddef.h>

// WORDLE — guess the hidden 5-letter word in 6 tries.
//   type A-Z to fill the row, BACKSPACE to delete, ENTER to submit.
//   green = right letter+spot, yellow = in word/wrong spot, gray = absent.
//   the on-screen QWERTY tints each key by the best news it has earned.
//   also clickable with the mouse. ENTER on the result screen plays again.

#define ROWS 6
#define COLS 5

// ---- word lists -------------------------------------------------------
// answers: the pool the hidden word is picked from.
static const char *ANSWERS[] = {
    "CRANE","SLATE","TRACE","ROAST","BLINK","GHOST","PLUMB","FROWN","DWELL","QUERY",
    "MIRTH","JUMBO","VIXEN","ZESTY","PROXY","FJORD","GLYPH","NYMPH","WALTZ","CIVIC",
    "BRINE","CHALK","DROVE","EMBER","FABLE","GIANT","HASTY","IVORY","JOLLY","KNEEL",
    "LUNAR","MANGO","NOBLE","OZONE","PIANO","RUMBA","SHARK","TIGER","UNZIP","VAULT",
    "WHISK","YACHT","BAYOU","CANDY","DAISY","EAGLE","FLUTE","GRAPE","HONEY","IGLOO",
};
#define N_ANSWERS (int)(sizeof(ANSWERS)/sizeof(ANSWERS[0]))

// extra accepted guesses (real words that are never the answer)
static const char *EXTRA[] = {
    "ABOUT","AUDIO","ADIEU","ALONE","AROSE","ABIDE","ANGEL","ARGUE","AWARE","BREAD",
    "BLAME","BOAST","BROIL","CRATE","CHEAP","CLOUD","DEALT","DODGE","DREAM","EARTH",
    "ELDER","FAITH","FRESH","GLOOM","GREAT","HEART","HOUSE","INPUT","IRATE","JOIST",
    "KAYAK","LIGHT","LOYAL","MOIST","MOUTH","NORTH","OCEAN","PAINT","PLANT","QUIET",
    "RAISE","REACT","SOLID","STARE","STORM","TABLE","THUMB","TRAIN","ULCER","UNITE",
    "VOICE","WORLD","WRACK","XENON","YEARN","ZONAL","STEAK","SUGAR","SWORD","SOUND",
};
#define N_EXTRA (int)(sizeof(EXTRA)/sizeof(EXTRA[0]))

// ---- tile color states ------------------------------------------------
#define ST_EMPTY  0   // not yet judged
#define ST_GRAY   1   // absent
#define ST_YELLOW 2   // present, wrong spot
#define ST_GREEN  3   // correct spot

// game phases
#define PH_PLAY   0
#define PH_WIN    1
#define PH_LOSE   2

static char answer[6];
static char guess[ROWS][COLS+1];   // filled letters, '\0' for empty cells
static int  judge[ROWS][COLS];     // ST_* per submitted cell
static int  cur_row;
static int  cur_col;
static int  phase;
static int  key_state[26];         // best ST_* learned per letter, 0 = unused
static float reveal_t;             // start time of the current row's flip
static int  revealing;             // which row is mid-reveal (-1 = none)
static float buzz_t;               // invalid-guess flash timer
static int  streak;

// layout
#define TILE 30
#define TGAP 4
#define GRID_X ((SCREEN_W - (COLS*TILE + (COLS-1)*TGAP)) / 2)
#define GRID_Y 8

// QWERTY rows
static const char *KB[3] = { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" };
#define KEY_W 22
#define KEY_H 18
#define KEY_GAP 2
#define KB_Y 232

// ---------------------------------------------------------------------
static int letters_only(const char *w) {
    for (int i = 0; i < COLS; i++) if (w[i] < 'A' || w[i] > 'Z') return 0;
    return w[COLS] == 0;
}

static int word_allowed(const char *w) {
    for (int i = 0; i < N_ANSWERS; i++) if (strncmp(w, ANSWERS[i], COLS) == 0) return 1;
    for (int i = 0; i < N_EXTRA;   i++) if (strncmp(w, EXTRA[i],   COLS) == 0) return 1;
    return 0;
}

static void new_game(void) {
    strcpy(answer, ANSWERS[rnd(N_ANSWERS)]);
    memset(guess, 0, sizeof guess);
    memset(judge, 0, sizeof judge);
    memset(key_state, 0, sizeof key_state);
    cur_row = 0; cur_col = 0; phase = PH_PLAY;
    revealing = -1; reveal_t = 0; buzz_t = 0;
}

void init(void) {
    streak = load(0);
    new_game();
}

// judge the current row against the answer using proper duplicate handling
static void judge_row(int r) {
    int used[COLS]; for (int i = 0; i < COLS; i++) used[i] = 0;
    // first pass: greens
    for (int i = 0; i < COLS; i++) {
        if (guess[r][i] == answer[i]) { judge[r][i] = ST_GREEN; used[i] = 1; }
        else judge[r][i] = ST_GRAY;
    }
    // second pass: yellows (only against unmatched answer letters)
    for (int i = 0; i < COLS; i++) {
        if (judge[r][i] == ST_GREEN) continue;
        for (int j = 0; j < COLS; j++) {
            if (!used[j] && answer[j] == guess[r][i]) { judge[r][i] = ST_YELLOW; used[j] = 1; break; }
        }
    }
    // fold results into keyboard state (keep the best)
    for (int i = 0; i < COLS; i++) {
        int k = guess[r][i] - 'A';
        if (k >= 0 && k < 26 && judge[r][i] > key_state[k]) key_state[k] = judge[r][i];
    }
}

static void submit(void) {
    if (cur_col < COLS) { buzz_t = 0.4f; shake(3); hit(40, INSTR_NOISE, 3, 90); return; }
    guess[cur_row][COLS] = 0;
    if (!word_allowed(guess[cur_row])) { buzz_t = 0.4f; shake(3); hit(40, INSTR_NOISE, 3, 90); return; }

    judge_row(cur_row);
    revealing = cur_row;
    reveal_t = now();
    // reveal blips, staggered
    for (int i = 0; i < COLS; i++) schedule(i * 130, 64 + i*2, INSTR_SQUARE, 2);

    int win = (strncmp(guess[cur_row], answer, COLS) == 0);
    if (win) {
        phase = PH_WIN;
        streak++; save(0, streak);
        for (int i = 0; i < 5; i++) schedule(700 + i*90, 67 + i*4, INSTR_TRI, 5);
    } else if (cur_row == ROWS - 1) {
        phase = PH_LOSE;
        streak = 0; save(0, streak);
        schedule(700, 36, INSTR_SAW, 4);
    } else {
        cur_row++; cur_col = 0;
    }
}

static void type_letter(char c) {
    if (phase != PH_PLAY) return;
    if (cur_col >= COLS) return;
    guess[cur_row][cur_col++] = c;
    note(72, INSTR_SQUARE, 2);
}

static void backspace(void) {
    if (phase != PH_PLAY) return;
    if (cur_col > 0) { guess[cur_row][--cur_col] = 0; note(60, INSTR_SQUARE, 2); }
}

// on-screen keyboard hit-test: returns the key box for KB[r][c]
static int key_box(int r, int c, int *bx, int *by) {
    int n = (int)strlen(KB[r]);
    int roww = n * KEY_W + (n-1) * KEY_GAP;
    // rows 1 and 2 get extra width for ENTER/DEL pads; keep letters centered on the grid
    int x0 = (SCREEN_W - roww) / 2;
    *bx = x0 + c * (KEY_W + KEY_GAP);
    *by = KB_Y + r * (KEY_H + KEY_GAP);
    return 1;
}

void update(void) {
    if (buzz_t > 0) buzz_t -= dt();

    if (phase != PH_PLAY) {
        if (keyp(KEY_ENTER) || mouse_pressed(MOUSE_LEFT)) { new_game(); note(72, INSTR_TRI, 4); }
        return;
    }

    // typed letters
    const char *in = text_input();
    for (int i = 0; in[i]; i++) {
        char c = in[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c >= 'A' && c <= 'Z') type_letter(c);
    }
    if (keyp(KEY_BACKSPACE)) backspace();
    if (keyp(KEY_ENTER)) submit();

    // on-screen keyboard clicks
    if (mouse_pressed(MOUSE_LEFT)) {
        int mx = mouse_x(), my = mouse_y(), handled = 0;
        for (int r = 0; r < 3 && !handled; r++) {
            int n = (int)strlen(KB[r]);
            for (int c = 0; c < n; c++) {
                int bx, by; key_box(r, c, &bx, &by);
                if (point_in_box(mx, my, bx, by, KEY_W, KEY_H)) { type_letter(KB[r][c]); handled = 1; break; }
            }
        }
        // ENTER / DEL pads sit at the ends of the bottom row
        int br, bb; key_box(2, 0, &br, &bb);
        int enter_x = br - (KEY_W + KEY_GAP) - 16;
        int del_x;  { int rb; key_box(2, (int)strlen(KB[2]) - 1, &del_x, &rb); del_x += KEY_W + KEY_GAP; }
        if (!handled && point_in_box(mx, my, enter_x, bb, KEY_W + 14, KEY_H)) submit();
        if (!handled && point_in_box(mx, my, del_x,   bb, KEY_W + 14, KEY_H)) backspace();
    }
}

// ---------------------------------------------------------------------
static int st_color(int st) {
    switch (st) {
        case ST_GREEN:  return CLR_MEDIUM_GREEN;
        case ST_YELLOW: return CLR_ORANGE;
        case ST_GRAY:   return CLR_DARK_GREY;
        default:        return CLR_DARKER_BLUE;
    }
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);

    // ---- guess grid ----
    for (int r = 0; r < ROWS; r++) {
        // per-row flip progress for the row currently revealing
        for (int c = 0; c < COLS; c++) {
            int x = GRID_X + c * (TILE + TGAP);
            int y = GRID_Y + r * (TILE + TGAP);

            int st = judge[r][c];
            int shown = (r < cur_row) || (r == cur_row && phase != PH_PLAY) || (r == revealing);
            int show_color = ST_EMPTY;

            if (shown && st != ST_EMPTY) {
                if (r == revealing) {
                    // staggered flip: each tile reveals 0.13s apart
                    float local = now() - reveal_t - c * 0.13f;
                    if (local > 0) {
                        show_color = st;
                        float t = clamp(local / 0.18f, 0, 1);
                        float sc = 0.5f + 0.5f * ease_out(t);   // grows 0.5 -> 1.0
                        int h = (int)(TILE * sc);
                        rectfill(x, y + (TILE - h)/2, TILE, h, st_color(show_color));
                    } else {
                        rectfill(x, y, TILE, TILE, st_color(ST_EMPTY));
                        rect(x, y, TILE, TILE, CLR_DARK_GREY);
                    }
                } else {
                    show_color = st;
                    rectfill(x, y, TILE, TILE, st_color(show_color));
                }
            } else {
                rectfill(x, y, TILE, TILE, st_color(ST_EMPTY));
                // outline the active typing cell
                int active = (r == cur_row && phase == PH_PLAY);
                rect(x, y, TILE, TILE, active ? CLR_MEDIUM_GREY : CLR_DARKER_PURPLE);
            }

            // winning row bounce
            int oy = 0;
            if (phase == PH_WIN && r == cur_row) {
                float t = now() - reveal_t;
                oy = (int)(-6.0f * sin_deg((t * 360.0f) + c * 40.0f) * clamp(1.5f - t, 0, 1));
            }

            // letter
            char ch = guess[r][c];
            if (ch) {
                char s[2] = { ch, 0 };
                int lc = (show_color != ST_EMPTY) ? CLR_WHITE : CLR_LIGHT_PEACH;
                int tx = x + (TILE - text_width(s) * 2) / 2;
                int ty = y + (TILE - 16) / 2 + oy;
                print_scaled(s, tx, ty, lc, 2);
            }
        }
    }

    // ---- on-screen keyboard ----
    for (int r = 0; r < 3; r++) {
        int n = (int)strlen(KB[r]);
        for (int c = 0; c < n; c++) {
            int bx, by; key_box(r, c, &bx, &by);
            int st = key_state[KB[r][c] - 'A'];
            int bg = (st == ST_EMPTY) ? CLR_DARKER_GREY : st_color(st);
            rectfill(bx, by, KEY_W, KEY_H, bg);
            char s[2] = { KB[r][c], 0 };
            print(s, bx + (KEY_W - text_width(s)) / 2, by + (KEY_H - 8) / 2, CLR_WHITE);
        }
    }
    // ENTER / DEL pads
    {
        int br, bb; key_box(2, 0, &br, &bb);
        int enter_x = br - (KEY_W + KEY_GAP) - 16;
        int del_x;  { int rb; key_box(2, (int)strlen(KB[2]) - 1, &del_x, &rb); del_x += KEY_W + KEY_GAP; }
        rectfill(enter_x, bb, KEY_W + 14, KEY_H, CLR_TRUE_BLUE);
        print("ENT", enter_x + 4, bb + (KEY_H - 8)/2, CLR_WHITE);
        rectfill(del_x, bb, KEY_W + 14, KEY_H, CLR_DARK_RED);
        print("DEL", del_x + 5, bb + (KEY_H - 8)/2, CLR_WHITE);
    }

    // ---- HUD / result banner ----
    if (buzz_t > 0 && phase == PH_PLAY) {
        print_centered("not in word list", SCREEN_W/2, KB_Y - 14, CLR_RED);
    } else if (phase == PH_PLAY) {
        print_centered(str("streak %d", streak), SCREEN_W/2, KB_Y - 14, CLR_MEDIUM_GREY);
    }

    if (phase == PH_WIN || phase == PH_LOSE) {
        int by = KB_Y - 16;
        rectfill(0, by, SCREEN_W, 14, CLR_BLACK);
        if (phase == PH_WIN) {
            print_centered(str("SOLVED!  streak %d  -  ENTER to play again", streak), SCREEN_W/2, by + 3, CLR_GREEN);
        } else {
            print_centered(str("answer: %s  -  ENTER to play again", answer), SCREEN_W/2, by + 3, CLR_ORANGE);
        }
    }
}
