/* de:meta
{
  "title": "Hangman",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "save-load-persistence"
  ],
  "lineage": "Classic word-guessing game implemented from scratch with per-blank pop animations and an on-screen clickable alphabet; win/streak counts are the first save_int usage in a pure word/puzzle cart.",
  "genre": "word",
  "description": "The classic word-guessing game, drawn entirely from primitives — no sprite sheet. A hidden word waits as a row of underscored blanks; guess a letter and right answers pop into place with a green flash and a rising chime, while every wrong letter burns a guess and adds the next body part to the gallows: head, torso, two arms, two legs, then a red-X-eyed hanging. Win by completing the word before six misses; the figure drops in with a little ease and the screen shakes on a bad guess. Tracks your session win count and current streak across runs. Type a letter (A-Z) to guess, or click a key on the on-screen alphabet; press ENTER or click to deal a fresh word after a win or loss.",
  "todo": [
    "Bigger, less-nerdy word library.",
    "Comic font for the big text, plus text effects.",
    "Make the hanged figure look much better, and dead (X X eyes) when lost.",
    "Make the gallows clunkier (chunkier lines, maybe some dithering)."
  ]
}
de:meta */
#include "studio.h"
#include <string.h>

// HANGMAN — guess the hidden word a letter at a time.
//   right letter fills the blanks; wrong letter draws the next body part.
//   6 wrong = the hanging. type a letter or click the on-screen alphabet.
//   drawn entirely from primitives (line/circ/rect) — no sprite sheet.

#define MAX_WRONG 6

static const char *WORDS[] = {
    "PIXEL", "RAYLIB", "CARTRIDGE", "PALETTE", "SPRITE",
    "DREAM", "ENGINE", "GALLOWS", "PHANTOM", "JOYSTICK",
    "ARCADE", "NEON", "VECTOR", "RASTER", "SYNTH", "MELODY",
};
#define NWORDS ((int)(sizeof(WORDS) / sizeof(WORDS[0])))

// game states
enum { ST_PLAY, ST_WIN, ST_LOSE };

static char     word[24];          // current target (uppercase)
static int      wordLen;
static int      guessed[26];       // 1 = letter already guessed
static int      wrong;             // wrong guesses so far
static int      state;
static int      lastWord = -1;     // avoid back-to-back repeats
static int      wins, streak;

static float    flashGood;         // green flash timer
static float    flashBad;          // red flash timer
static float    revealT[24];       // per-blank pop animation (counts down)
static float    partT;             // new body-part drop-in (counts down)
static float    bannerT;           // banner ease-in

static int      keyX(int i) { return 14 + (i % 9) * 33; }     // alphabet key layout
static int      keyY(int i) { return 150 + (i / 9) * 16; }
#define KEY_W 30
#define KEY_H 13

static void new_word(void) {
    int idx;
    do { idx = rnd(NWORDS); } while (NWORDS > 1 && idx == lastWord);
    lastWord = idx;
    strcpy(word, WORDS[idx]);
    wordLen = (int)strlen(word);
    memset(guessed, 0, sizeof guessed);
    for (int i = 0; i < 24; i++) revealT[i] = 0;
    wrong = 0;
    state = ST_PLAY;
    flashGood = flashBad = partT = bannerT = 0;
}

void init(void) {
    wins   = load_int("hm_wins", 0);
    streak = load_int("hm_streak", 0);
    new_word();
}

static bool all_revealed(void) {
    for (int i = 0; i < wordLen; i++)
        if (!guessed[word[i] - 'A']) return false;
    return true;
}

static void guess(int letter) {        // letter is 0..25
    if (state != ST_PLAY) return;
    if (letter < 0 || letter > 25) return;
    if (guessed[letter]) return;
    guessed[letter] = 1;

    bool matched = false;
    for (int i = 0; i < wordLen; i++) {
        if (word[i] - 'A' == letter) { matched = true; revealT[i] = 0.35f; }
    }

    if (matched) {
        flashGood = 0.2f;
        note(72 + rnd(5), INSTR_SINE, 4);
        if (all_revealed()) {
            state = ST_WIN;
            bannerT = 0;
            wins++; streak++;
            save_int("hm_wins", wins);
            save_int("hm_streak", streak);
            // ascending win arpeggio
            schedule(0,   72, INSTR_TRI, 5);
            schedule(110, 76, INSTR_TRI, 5);
            schedule(220, 79, INSTR_TRI, 5);
            schedule(330, 84, INSTR_TRI, 6);
        }
    } else {
        wrong++;
        flashBad = 0.25f;
        partT = 0.3f;
        shake(3.0f);
        hit(46 + (MAX_WRONG - wrong), INSTR_SQUARE, 4, 140);
        if (wrong >= MAX_WRONG) {
            state = ST_LOSE;
            bannerT = 0;
            streak = 0;
            save_int("hm_streak", streak);
            schedule(0,   60, INSTR_SAW, 5);
            schedule(140, 56, INSTR_SAW, 5);
            schedule(280, 52, INSTR_SAW, 5);
            schedule(420, 47, INSTR_SAW, 6);
        }
    }
}

void update(void) {
    float d = dt();
    if (flashGood > 0) flashGood -= d;
    if (flashBad  > 0) flashBad  -= d;
    if (partT     > 0) partT     -= d;
    if (bannerT   < 1) bannerT   += d * 3.0f;
    for (int i = 0; i < wordLen; i++)
        if (revealT[i] > 0) revealT[i] -= d;

    if (state == ST_PLAY) {
        // keyboard letters
        const char *in = text_input();
        for (int i = 0; in[i]; i++) {
            char c = in[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            if (c >= 'A' && c <= 'Z') guess(c - 'A');
        }
        // click an alphabet key
        if (mouse_pressed(MOUSE_LEFT)) {
            int mx = mouse_x(), my = mouse_y();
            for (int i = 0; i < 26; i++)
                if (point_in_box(mx, my, keyX(i), keyY(i), KEY_W, KEY_H)) guess(i);
        }
    } else {
        // any key / click deals a new word
        if (keyp(KEY_ENTER) || keyp(KEY_SPACE) || mouse_pressed(MOUSE_LEFT) ||
            text_input()[0])
            new_word();
    }
}

// ------------------------------------------------------------
// the gallows + figure, drawn part by part from primitives
// ------------------------------------------------------------

static void draw_gallows(void) {
    int gx = 200, gy = 18;            // top-left of the gallows area
    int baseY = gy + 130;
    // structure (always drawn)
    line(gx - 10, baseY, gx + 70, baseY, CLR_BROWN);          // ground
    line(gx + 10, baseY, gx + 10, gy, CLR_BROWN);             // post
    line(gx + 10, gy, gx + 55, gy, CLR_BROWN);                // beam
    line(gx + 25, gy, gx + 10, gy + 18, CLR_BROWN);           // brace
    line(gx + 55, gy, gx + 55, gy + 14, CLR_DARK_GREY);       // rope

    int hx = gx + 55, hy = gy + 22;   // head center
    // each wrong guess reveals one more part; the newest one eases/drops in
    int show = wrong;
    // a tiny ease for the most-recent part
    float drop = (partT > 0) ? (1.0f - ease_out(1.0f - partT / 0.3f)) * 6.0f : 0;

    if (show >= 1) { int o = (show == 1) ? (int)drop : 0; circ(hx, hy - o, 8, CLR_LIGHT_PEACH); }            // head
    if (show >= 2) line(hx, hy + 8, hx, hy + 40, CLR_WHITE);                                                 // torso
    if (show >= 3) line(hx, hy + 16, hx - 14, hy + 28, CLR_WHITE);                                           // left arm
    if (show >= 4) line(hx, hy + 16, hx + 14, hy + 28, CLR_WHITE);                                           // right arm
    if (show >= 5) line(hx, hy + 40, hx - 12, hy + 58, CLR_WHITE);                                           // left leg
    if (show >= 6) line(hx, hy + 40, hx + 12, hy + 58, CLR_WHITE);                                           // right leg

    // sad/dead face once the head is up
    if (show >= 1) {
        pset(hx - 3, hy - 1, CLR_BLACK);
        pset(hx + 3, hy - 1, CLR_BLACK);
        if (state == ST_LOSE) {
            // X eyes on full hanging
            line(hx - 5, hy - 3, hx - 1, hy + 1, CLR_RED);
            line(hx - 1, hy - 3, hx - 5, hy + 1, CLR_RED);
            line(hx + 1, hy - 3, hx + 5, hy + 1, CLR_RED);
            line(hx + 5, hy - 3, hx + 1, hy + 1, CLR_RED);
        }
    }
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    if (flashGood > 0) { fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_DARK_GREEN); fillp_reset(); }
    if (flashBad  > 0) { fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_DARK_RED);   fillp_reset(); }

    print_scaled("HANGMAN", 14, 8, CLR_YELLOW, 2);
    print(str("wins %d", wins), 14, 30, CLR_LIGHT_GREY);
    print(str("streak %d", streak), 14, 40, streak > 0 ? CLR_GREEN : CLR_DARK_GREY);
    print(str("misses %d/%d", wrong, MAX_WRONG), 200, 6, wrong >= MAX_WRONG - 1 ? CLR_RED : CLR_LIGHT_GREY);

    draw_gallows();

    // the word as blanks / revealed letters
    int slot = 22;
    int totalW = wordLen * slot;
    int wx = (170 - totalW) / 2 + 8;
    if (wx < 8) wx = 8;
    int wy = 96;
    for (int i = 0; i < wordLen; i++) {
        int cx = wx + i * slot;
        bool shown = guessed[word[i] - 'A'] || state == ST_LOSE;
        line(cx + 2, wy + 14, cx + slot - 4, wy + 14, CLR_INDIGO);   // underscore
        if (shown) {
            int pop = (revealT[i] > 0) ? (int)((revealT[i] / 0.35f) * 4) : 0;
            int col = (state == ST_LOSE && !guessed[word[i] - 'A']) ? CLR_RED : CLR_WHITE;
            print_scaled(str("%c", word[i]), cx + 3, wy - pop, col, 1);
            print_scaled(str("%c", word[i]), cx + 3, wy - pop, col, 1); // 2nd pass = a touch bolder
        }
    }

    // on-screen alphabet
    for (int i = 0; i < 26; i++) {
        int kx = keyX(i), ky = keyY(i);
        int bg, fg;
        if (guessed[i]) {
            // greyed; if it was in the word, tint it green-ish
            bool inWord = false;
            for (int j = 0; j < wordLen; j++) if (word[j] - 'A' == i) inWord = true;
            bg = inWord ? CLR_DARK_GREEN : CLR_DARKER_GREY;
            fg = inWord ? CLR_LIGHT_PEACH : CLR_DARK_GREY;
        } else {
            bool hover = point_in_box(mouse_x(), mouse_y(), kx, ky, KEY_W, KEY_H);
            bg = (hover && state == ST_PLAY) ? CLR_TRUE_BLUE : CLR_DARK_BLUE;
            fg = CLR_WHITE;
        }
        rectfill(kx, ky, KEY_W, KEY_H, bg);
        rect(kx, ky, KEY_W, KEY_H, CLR_DARKER_BLUE);
        print(str("%c", 'A' + i), kx + 11, ky + 3, fg);
    }

    // win / lose banner
    if (state != ST_PLAY) {
        float e = ease_out(clamp(bannerT, 0, 1));
        int bw = 220, bh = 56;
        int by = (int)lerp(-bh, 70, e);
        int bx = (SCREEN_W - bw) / 2;
        rectfill(bx, by, bw, bh, state == ST_WIN ? CLR_DARK_GREEN : CLR_DARK_RED);
        rect(bx, by, bw, bh, CLR_WHITE);
        if (state == ST_WIN) {
            print_scaled("YOU GOT IT!", bx + 30, by + 12, CLR_YELLOW, 2);
        } else {
            print_scaled("HANGED!", bx + 56, by + 8, CLR_WHITE, 2);
            print(str("the word was %s", word), bx + 24, by + 30, CLR_LIGHT_PEACH);
        }
        if (state == ST_WIN)
            print_centered("ENTER or click for a new word", SCREEN_W/2, by + 42, CLR_LIGHT_PEACH);
        else
            print_centered("ENTER or click to try again", SCREEN_W/2, by + 44, CLR_LIGHT_GREY);
    }
}
