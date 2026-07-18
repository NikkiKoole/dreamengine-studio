/* de:meta
{
  "slug": "eartrainer",
  "title": "Ear Trainer",
  "status": "wip",
  "created": "2026-07-18",
  "kind": ["game"],
  "genre": "puzzle",
  "teaches": ["chord-voicing", "scale-quantize", "title-play-gameover-loop"],
  "lineage": "Born from demand discovery (field-note 026-demand-discovery-cross-tribe): 'ear training app/game' is the loudest ZERO-coverage ask across the music tribes (Google: 'ear training app' 22, 'ear training game' 11). The first cart built straight from the reddit-gaps → aso-suggest pipeline.",
  "description": {
    "summary": "Train your ear — a note or chord plays, tap to replay, then pick what you heard. Right = green + streak; wrong = it shows AND replays the answer so your ear learns it.",
    "detail": "A dead-simple ear-training loop: menu (pick what to train) -> a target plays -> tap LISTEN AGAIN to hear it as many times as you like -> answer from four choices -> feedback -> next round. NOTES mode quizzes a single white key in one fixed octave (piano); CHORDS mode quizzes the quality of a triad (Major / Minor / Diminished / Augmented) on a random root. Getting it wrong replays the correct target and highlights it green, so the association is trained by ear, not just marked wrong. Content is deliberately swappable — if single-note absolute-pitch ID feels too hard, notes mode is the first thing to pivot to relative intervals (play a reference C, then the target); the loop stays identical.",
    "controls": "Menu: tap NOTES or CHORDS. In a round: tap the LISTEN AGAIN bar (or click anywhere on it) to replay the target, tap one of the four choice tiles to answer, MENU (top-left) to go back. Feedback auto-advances to the next round after ~1s."
  },
  "todo": [
    "NOTES mode is absolute-pitch ID (hard for humans) — evaluate pivoting to relative intervals (reference C then target) or scale-degree recognition.",
    "add an INTERVALS mode (m3, P5, octave…) — the classic ear-training staple, higher demand than single notes.",
    "difficulty ramp: widen the octave range / add chromatic notes / add 7th chords as streak climbs.",
    "persist BEST streak via save_int so it survives sessions.",
    "game-feel pass: particle pop + a short chime on a correct streak milestone (per docs/guides/game-feel.md)."
  ]
}
de:meta */

// Ear Trainer — the prototype loop:
//   menu (pick mode) -> round (target plays) -> tap-to-replay -> multiple choice -> feedback -> next
// Content is deliberately swappable: NOTES = which white key, CHORDS = which triad quality.
// If single-note absolute ID feels too hard, the notes mode is the first thing we'd pivot to
// intervals (play a reference C, then the target) — the loop below stays identical.

#include "studio.h"
#include "ui.h"   // ui_button — per-finger capture + fat hit pads (menu + answers + replay)

// ---- screens & modes ------------------------------------------------------
enum { ST_MENU, ST_PLAY };
enum { TR_NOTES, TR_CHORDS, TR_INTERVALS };

// ---- content --------------------------------------------------------------
// one octave of white keys (C4..B4) — consistent octave so the quiz is fair
static const int   WHITE_MIDI[7] = { 60, 62, 64, 65, 67, 69, 71 };
static const char *WHITE_NAME[7] = { "C", "D", "E", "F", "G", "A", "B" };

// four triad qualities, each as semitone offsets from the root
enum { Q_MAJOR, Q_MINOR, Q_DIM, Q_AUG, N_QUAL };
static const char *QUAL_NAME[N_QUAL]     = { "Major", "Minor", "Diminished", "Augmented" };
static const int   QUAL_IVL[N_QUAL][3]   = { {0,4,7}, {0,3,7}, {0,3,6}, {0,4,8} };

// intervals — root then target note (played in sequence); name the distance
enum { N_IVL = 9 };
static const char *IVL_NAME[N_IVL] = { "Minor 2nd", "Major 2nd", "Minor 3rd", "Major 3rd",
                                       "Perfect 4th", "Tritone", "Perfect 5th", "Major 6th", "Octave" };
static const int   IVL_SEMI[N_IVL] = { 1, 2, 3, 4, 5, 6, 7, 9, 12 };

// ---- state ----------------------------------------------------------------
static int st       = ST_MENU;
static int mode     = TR_NOTES;
static int answer   = 0;        // index of the correct choice (0..6 notes / 0..3 chords)
static int root     = 60;       // chord root midi
static int choices[4];          // choice-index shown in each of the 4 buttons
static int n_choice = 4;
static int picked   = -1;       // choice-index the player tapped (-1 = unanswered)
static int result_t = 0;        // feedback countdown (frames)
static int score = 0, streak = 0, best = 0;
static float flash = 0.0f;      // 0..1 feedback glow, decays each frame
static int pend_midi = -1;      // interval's 2nd note, played after a gap
static int pend_t = 0;          // frames until the pending note fires

// ---- helpers --------------------------------------------------------------
// adaptive font: usefont() sets the active font AND the glyph width so text_w /
// printc measure correctly at any canvas size (small screens use FONT_SMALL).
static int g_cw = 8;
static void usefont(int f) { font(f); g_cw = (f == FONT_SMALL) ? 4 : (f == FONT_TINY ? 3 : 8); }
static int text_w(const char *s) { int n = 0; while (s[n]) n++; return n * g_cw; }
static void printc(const char *s, int cx, int y, int c) { print(s, cx - text_w(s) / 2, y, c); }

static void play_target(void) {
    if (mode == TR_CHORDS) {
        for (int i = 0; i < 3; i++) note(root + QUAL_IVL[answer][i], INSTR_PIANO, 5);
    } else if (mode == TR_INTERVALS) {
        note(root, INSTR_PIANO, 6);                 // reference note now...
        pend_midi = root + IVL_SEMI[answer];        // ...target note a beat later
        pend_t = 16;
    } else {
        note(WHITE_MIDI[answer], INSTR_PIANO, 6);
    }
}

// pick 4 distinct choices from a pool of N, guaranteed to include the answer
static void build_choices(int N) {
    int pool[16];
    for (int i = 0; i < N; i++) pool[i] = i;
    for (int i = N - 1; i > 0; i--) { int j = rnd(i + 1); int t = pool[i]; pool[i] = pool[j]; pool[j] = t; }
    n_choice = 4;
    for (int i = 0; i < 4; i++) choices[i] = pool[i];
    int has = 0; for (int i = 0; i < 4; i++) if (choices[i] == answer) has = 1;
    if (!has) choices[rnd(4)] = answer;
    for (int i = 3; i > 0; i--) { int j = rnd(i + 1); int t = choices[i]; choices[i] = choices[j]; choices[j] = t; }
}

static void new_round(void) {
    picked = -1; result_t = 0; pend_midi = -1; pend_t = 0;
    if (mode == TR_CHORDS) {
        root = 57 + rnd(8);            // random root A3..E4
        answer = rnd(N_QUAL);
        build_choices(N_QUAL);         // all four qualities, shuffled
    } else if (mode == TR_INTERVALS) {
        root = 55 + rnd(10);           // random reference note G3..
        answer = rnd(N_IVL);
        build_choices(N_IVL);
    } else {
        answer = rnd(7);               // which white key
        build_choices(7);
    }
    play_target();
}

static const char *choice_label(int ci) {
    if (mode == TR_CHORDS)    return QUAL_NAME[ci];
    if (mode == TR_INTERVALS) return IVL_NAME[ci];
    return WHITE_NAME[ci];
}

// ---- update ---------------------------------------------------------------
void update(void) {
    if (flash > 0.0f) flash -= 0.06f;
    if (flash < 0.0f) flash = 0.0f;
    if (pend_t > 0) { pend_t--; if (pend_t == 0 && pend_midi >= 0) { note(pend_midi, INSTR_PIANO, 6); pend_midi = -1; } }
    if (st == ST_PLAY && picked >= 0) {
        if (result_t > 0) result_t--;
        else new_round();
    }
}

// ---- draw -----------------------------------------------------------------
// P (portrait) = narrow canvas (e.g. 100x160 phone): shrink margins/fonts and
// stack the answer choices in ONE column. Landscape keeps the roomy 2x2 look.
static void draw_menu(void) {
    int P = SCREEN_W < SCREEN_H;
    cls(CLR_DARK_BLUE);
    usefont(P ? FONT_SMALL : FONT_NORMAL);
    printc("EAR TRAINER", SCREEN_W / 2, SCREEN_H / 8, CLR_WHITE);
    printc("train your ear", SCREEN_W / 2, SCREEN_H / 8 + (P ? 8 : 12), CLR_LIGHT_GREY);

    int marg = P ? 6 : 40;
    int bw = SCREEN_W - 2 * marg, bx = marg;
    int bh = P ? 24 : 34, gap = P ? 8 : 14;
    int by = SCREEN_H / 2 - (3 * bh + 2 * gap) / 2 + (P ? 6 : 4);
    const char *names[3] = { "NOTES", "INTERVALS", "CHORDS" };
    const int   modes[3] = { TR_NOTES, TR_INTERVALS, TR_CHORDS };
    usefont(FONT_NORMAL);
    for (int i = 0; i < 3; i++)
        if (ui_button(bx, by + i * (bh + gap), bw, bh, names[i])) { mode = modes[i]; st = ST_PLAY; new_round(); }

    usefont(P ? FONT_SMALL : FONT_NORMAL);
    printc(P ? "pick one" : "what do you want to train?", SCREEN_W / 2, SCREEN_H - (P ? 10 : 16), CLR_DARK_GREY);
    usefont(FONT_NORMAL);
}

static const char *listen_label(int P) {
    if (P) return "REPLAY";   // \x0e note-glyph is absent from FONT_SMALL → plain text
    if (mode == TR_CHORDS)    return "\x0e LISTEN AGAIN (chord)";
    if (mode == TR_INTERVALS) return "\x0e LISTEN AGAIN (interval)";
    return "\x0e LISTEN AGAIN (note)";
}

static void draw_play(void) {
    int P = SCREEN_W < SCREEN_H;

    // feedback-tinted background so a right/wrong answer is felt, not just read
    int bg = CLR_DARK_BLUE;
    if (picked >= 0) bg = (picked == answer) ? CLR_GREEN : CLR_RED;
    cls(picked >= 0 && flash > 0.25f ? bg : CLR_DARK_BLUE);

    // header: back, score, streak (compact in portrait)
    usefont(FONT_NORMAL);
    if (ui_button(3, 3, P ? 24 : 44, P ? 13 : 16, P ? "<" : "MENU")) st = ST_MENU;
    usefont(P ? FONT_SMALL : FONT_NORMAL);
    if (P) {
        print(str("STRK %d", streak), 30, 4, CLR_LIGHT_GREY);
        print(str("SC %d", score), SCREEN_W - 26, 4, CLR_YELLOW);
    } else {
        print(str("STREAK %d  BEST %d", streak, best), 54, 3, CLR_LIGHT_GREY);
        print(str("SCORE %d", score), SCREEN_W - 74, 3, CLR_YELLOW);
    }

    // LISTEN AGAIN — the "tap to replay" area (works for mouse + touch via ui_button)
    int marg = P ? 6 : 20;
    int ly = P ? 19 : 24, lh = P ? 26 : 40;
    usefont(P ? FONT_SMALL : FONT_NORMAL);
    if (ui_button(marg, ly, SCREEN_W - 2 * marg, lh, listen_label(P))) play_target();

    // answer choices: 1 column (portrait) or 2x2 grid (landscape)
    int cols = P ? 1 : 2, rows = 4 / cols;
    int gm = P ? 6 : 10, cgap = P ? 4 : 8;
    int gy0 = ly + lh + (P ? 12 : 10);
    int cw = (SCREEN_W - 2 * gm - (cols - 1) * cgap) / cols;
    int ch = (SCREEN_H - gy0 - (P ? 6 : 8) - (rows - 1) * cgap) / rows;
    usefont(P ? FONT_SMALL : FONT_NORMAL);
    for (int i = 0; i < n_choice; i++) {
        int cx = gm + (i % cols) * (cw + cgap);
        int cy = gy0 + (i / cols) * (ch + cgap);
        int ci = choices[i];
        // during feedback, recolor the correct + the mistaken pick
        if (picked >= 0) {
            int fill = CLR_DARK_GREY;
            if (ci == answer) fill = CLR_GREEN;
            else if (ci == picked) fill = CLR_RED;
            rectfill(cx, cy, cw, ch, fill);
            rect(cx, cy, cw, ch, CLR_WHITE);
            printc(choice_label(ci), cx + cw / 2, cy + ch / 2 - (P ? 3 : 4), CLR_WHITE);
        } else {
            if (ui_button(cx, cy, cw, ch, choice_label(ci))) {
                picked = ci;
                result_t = 70;
                flash = 1.0f;
                if (picked == answer) { score++; streak++; if (streak > best) best = streak; }
                else                   streak = 0;
                play_target();  // replay on reveal — the ear learns by hearing the answer
            }
        }
    }

    // verdict line (sits just above the choices)
    if (picked >= 0) {
        const char *msg = (picked == answer) ? "Correct!" : str("It was %s", choice_label(answer));
        printc(msg, SCREEN_W / 2, gy0 - (P ? 9 : 10), CLR_WHITE);
    }
    usefont(FONT_NORMAL);

#ifdef DE_TRACE
    watch("mode",   "%d", mode);
    watch("answer", "%d", answer);
    watch("picked", "%d", picked);
    watch("streak", "%d", streak);
#endif
}

void draw(void) {
    ui_begin();                    // FIRST: sample contacts + focus keys (ui.h two-phase)
    if (st == ST_MENU) draw_menu();
    else               draw_play();
    ui_end();                      // LAST: resolve presses against widgets drawn this frame
}
