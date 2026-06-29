/* de:meta
{
  "title": "Othello",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "turn-based-loop",
    "finite-state-ai"
  ],
  "lineage": "Classic Reversi; the CPU uses a corner-weighted heuristic board (WEIGHT[N][N]) — greedy positional evaluation, not minimax — making it a clean example of table-driven finite-state AI over a turn-based rule engine.",
  "genre": "tabletop",
  "description": "Reversi on a felt 8x8 board: you are black, the CPU is white. Drop a disc only where it flanks a straight line of the opponent and every captured line turns over, the flip travelling outward from your new disc as a squashed-oval roll that glints green through the seam. Legal squares pulse, the score rolls live in the HUD, corners trigger a chord and a screen kick, and a side passes when it has no move; the game ends when the board fills or both pass. The CPU plays a greedy, corner-weighted heuristic that hoards corners and shuns the squares beside them. Drawn entirely from primitives, no sprite sheet. Mouse: click a glowing square to place. Keyboard: arrows/WASD move a cursor, Z/ENTER places; click or Z on the banner for a new game."
}
de:meta */
#include "studio.h"
#include <stddef.h>
#include <string.h>

// ── OTHELLO — Reversi on an 8×8 board ────────────────────────────────────────
// You are BLACK, the CPU is WHITE. Drop a disc only where it flanks at least one
// straight line of the opponent between your new disc and one of yours — that
// whole line flips to your color, travelling outward with a little turn-over
// animation. Legal squares pulse; the score is live; you pass when stuck; the
// game ends when the board is full or both sides pass. The CPU is greedy and
// corner-hungry (it grabs corners, dodges the squares next to them).
//
// Mouse: click a glowing square to place. Keyboard: arrows/WASD move a cursor,
// Z / ENTER places. Click (or Z) on the game-over banner for a new game.

#define N        8
#define CELL     20
#define BOARD_PX (N * CELL)           // 160
#define BX       ((SCREEN_W - BOARD_PX) / 2)   // board left  (80)
#define BY       28                            // board top
#define EMPTY    0
#define BLACK    1
#define WHITE    2

static int  board[N][N];
static int  legal[N][N];              // flips available if current player plays here
static int  nLegal;
static int  turn;                     // BLACK / WHITE
static int  passes;                   // consecutive passes
static int  winner;                   // -1 none yet, 0 tie, BLACK, WHITE

enum { PLAY, ANIMATING, CPU_WAIT, OVER };
static int   phase;
static float cpuTimer;                // think delay before the CPU moves
static int   passMsg;                 // frames to flash a "PASS" banner
static int   passWho;

// flip animation: per cell, a disc that is turning over
static float flipT[N][N];             // 0..1 progress, <=0 = not animating
static float flipDelay[N][N];         // stagger before this cell starts
static int   flipFrom[N][N];          // owner before flip
static int   flipTo[N][N];            // owner after flip
static float dropT[N][N];             // freshly placed disc: radius ease-in 0..1

// keyboard cursor
static int  curx = 3, cury = 3;
static int  lastmx = -1, lastmy = -1;

// rolling HUD score
static float showBlack, showWhite;

// corner-weighted board values for the CPU
static const int WEIGHT[N][N] = {
    { 120, -20,  20,   5,   5,  20, -20, 120 },
    { -20, -40,  -5,  -5,  -5,  -5, -40, -20 },
    {  20,  -5,  15,   3,   3,  15,  -5,  20 },
    {   5,  -5,   3,   3,   3,   3,  -5,   5 },
    {   5,  -5,   3,   3,   3,   3,  -5,   5 },
    {  20,  -5,  15,   3,   3,  15,  -5,  20 },
    { -20, -40,  -5,  -5,  -5,  -5, -40, -20 },
    { 120, -20,  20,   5,   5,  20, -20, 120 },
};

static const int DIRX[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
static const int DIRY[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };

// ── rules ────────────────────────────────────────────────────────────────────
static int other(int p) { return p == BLACK ? WHITE : BLACK; }

// how many discs flip if `me` plays at (x,y); 0 if illegal
static int count_flips(int g[N][N], int x, int y, int me) {
    if (g[y][x] != EMPTY) return 0;
    int total = 0, foe = other(me);
    for (int d = 0; d < 8; d++) {
        int cx = x + DIRX[d], cy = y + DIRY[d], run = 0;
        while (cx >= 0 && cx < N && cy >= 0 && cy < N && g[cy][cx] == foe) {
            cx += DIRX[d]; cy += DIRY[d]; run++;
        }
        if (run > 0 && cx >= 0 && cx < N && cy >= 0 && cy < N && g[cy][cx] == me)
            total += run;
    }
    return total;
}

static int compute_legal(int me) {
    nLegal = 0;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        legal[y][x] = count_flips(board, x, y, me);
        if (legal[y][x] > 0) nLegal++;
    }
    return nLegal;
}

static void tally(int *b, int *w) {
    *b = *w = 0;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        if (board[y][x] == BLACK) (*b)++;
        else if (board[y][x] == WHITE) (*w)++;
    }
}

// ── sound ────────────────────────────────────────────────────────────────────
static void snd_place(void)   { note(60, 7, 4); }
static void snd_flip(int run)  { note(58 + min(run, 8) * 2, 6, 3); }
static void snd_corner(void)  { strum(60, CHORD_MAJ, 8, 5, 50); shake(2.5f); }
static void snd_pass(void)    { note(48, 5, 3); }
static void snd_end(void) {
    schedule(0,   60, 8, 5); schedule(150, 64, 8, 5);
    schedule(300, 67, 8, 5); schedule(450, 72, 8, 6);
}

// ── placing a disc + kicking off the flip animation ──────────────────────────
static bool is_corner(int x, int y) {
    return (x == 0 || x == N - 1) && (y == 0 || y == N - 1);
}

// place `me` at (x,y): set the disc, walk every flanked line, start flip anims
static void place(int x, int y, int me) {
    int foe = other(me), flipped = 0;
    board[y][x] = me;
    dropT[y][x] = 0.0f;                       // fresh disc eases its radius in
    flipT[y][x] = -1.0f;

    for (int d = 0; d < 8; d++) {
        // first confirm this direction actually flanks
        int cx = x + DIRX[d], cy = y + DIRY[d], run = 0;
        while (cx >= 0 && cx < N && cy >= 0 && cy < N && board[cy][cx] == foe) {
            cx += DIRX[d]; cy += DIRY[d]; run++;
        }
        if (run == 0 || cx < 0 || cx >= N || cy < 0 || cy >= N || board[cy][cx] != me)
            continue;
        // walk it again, flipping, staggering the start so the line turns outward
        cx = x + DIRX[d]; cy = y + DIRY[d];
        for (int k = 0; k < run; k++) {
            board[cy][cx] = me;
            flipFrom[cy][cx] = foe;
            flipTo[cy][cx]   = me;
            flipT[cy][cx]    = 0.0f;
            flipDelay[cy][cx] = 0.06f * k;    // travel speed
            cx += DIRX[d]; cy += DIRY[d];
            flipped++;
        }
    }
    snd_place();
    if (flipped > 0) snd_flip(flipped);
    if (is_corner(x, y)) snd_corner();
    phase = ANIMATING;
}

static bool anims_running(void) {
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        if (flipT[y][x] >= 0.0f && flipT[y][x] < 1.0f) return true;
        if (dropT[y][x] >= 0.0f && dropT[y][x] < 1.0f) return true;
    }
    return false;
}

// ── turn flow ────────────────────────────────────────────────────────────────
static void finish_game(void) {
    int b, w; tally(&b, &w);
    winner = (b > w) ? BLACK : (w > b) ? WHITE : 0;
    phase = OVER;
    snd_end();
    if (winner == BLACK) {
        int margin = b - w;
        if (margin > load_int("bestmargin", 0)) save_int("bestmargin", margin);
    }
}

// hand the turn to `next`; auto-pass / end as the rules require
static void pass_to(int next) {
    if (compute_legal(next) > 0) {
        turn = next; passes = 0;
        phase = (next == WHITE) ? CPU_WAIT : PLAY;
        if (next == WHITE) cpuTimer = 0.55f;
    } else {
        // `next` has no move → it passes
        passes++;
        passMsg = 70; passWho = next;
        snd_pass();
        if (passes >= 2) { finish_game(); return; }
        // try the player after that
        int after = other(next);
        if (compute_legal(after) > 0) {
            turn = after; phase = (after == WHITE) ? CPU_WAIT : PLAY;
            if (after == WHITE) cpuTimer = 0.55f;
        } else {
            finish_game();                    // neither can move
        }
    }
}

// ── CPU: greedy, corner-weighted ─────────────────────────────────────────────
static void cpu_move(void) {
    int bx = -1, by = -1, best = -100000;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        if (legal[y][x] <= 0) continue;
        int score = WEIGHT[y][x] + legal[y][x];   // position dominates, flips break ties
        if (score > best) { best = score; bx = x; by = y; }
    }
    if (bx < 0) { pass_to(BLACK); return; }
    place(bx, by, WHITE);
}

// ── setup ────────────────────────────────────────────────────────────────────
static void reset_game(void) {
    memset(board, 0, sizeof board);
    board[3][3] = WHITE; board[3][4] = BLACK;
    board[4][3] = BLACK; board[4][4] = WHITE;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        flipT[y][x] = -1.0f; dropT[y][x] = 1.0f; flipDelay[y][x] = 0.0f;
        flipFrom[y][x] = flipTo[y][x] = EMPTY;
    }
    turn = BLACK; passes = 0; winner = -1; phase = PLAY;
    passMsg = 0; curx = 3; cury = 3; cpuTimer = 0;
    int b, w; tally(&b, &w); showBlack = b; showWhite = w;
    compute_legal(BLACK);
}

void init(void) {
    instrument(7, INSTR_SINE, 1, 90, 0, 70);     // soft click
    instrument(6, INSTR_TRI,  1, 60, 2, 90);     // flip tick
    instrument(5, INSTR_TRI,  4, 80, 3, 140);    // pass tone
    instrument(8, INSTR_SINE, 3, 120, 4, 200);   // corner chord
    bpm(110);
    reset_game();
}

// ── input ────────────────────────────────────────────────────────────────────
static bool confirm_pressed(void) {
    return mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A) || keyp(KEY_ENTER);
}

void update(void) {
    if (passMsg > 0) passMsg--;

    // advance flip + drop animations (framerate-independent)
    float d = dt();
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        if (flipT[y][x] >= 0.0f && flipT[y][x] < 1.0f) {
            if (flipDelay[y][x] > 0.0f) flipDelay[y][x] -= d;
            else { flipT[y][x] += d * 5.0f; if (flipT[y][x] >= 1.0f) flipT[y][x] = 1.0f; }
        }
        if (dropT[y][x] >= 0.0f && dropT[y][x] < 1.0f) {
            dropT[y][x] += d * 6.0f; if (dropT[y][x] > 1.0f) dropT[y][x] = 1.0f;
        }
    }

    // roll the HUD score toward the real tally
    int b, w; tally(&b, &w);
    showBlack = lerp(showBlack, b, clamp(d * 8.0f, 0, 1));
    showWhite = lerp(showWhite, w, clamp(d * 8.0f, 0, 1));

    if (phase == OVER) {
        if (confirm_pressed()) reset_game();
        return;
    }

    if (phase == ANIMATING) {
        if (!anims_running()) pass_to(other(turn));
        return;
    }

    if (phase == CPU_WAIT) {
        cpuTimer -= d;
        if (cpuTimer <= 0) cpu_move();
        return;
    }

    // PLAY — human (black)
    // keyboard cursor
    if (btnp(0, BTN_LEFT)  || keyp(KEY_LEFT))  curx = max(0, curx - 1);
    if (btnp(0, BTN_RIGHT) || keyp(KEY_RIGHT)) curx = min(N - 1, curx + 1);
    if (btnp(0, BTN_UP)    || keyp(KEY_UP))    cury = max(0, cury - 1);
    if (btnp(0, BTN_DOWN)  || keyp(KEY_DOWN))  cury = min(N - 1, cury + 1);

    // mouse follows the board
    if (mouse_x() != lastmx || mouse_y() != lastmy) {
        lastmx = mouse_x(); lastmy = mouse_y();
        int mx = (mouse_x() - BX) / CELL, my = (mouse_y() - BY) / CELL;
        if (mouse_x() >= BX && mouse_y() >= BY && mx >= 0 && mx < N && my >= 0 && my < N) {
            curx = mx; cury = my;
        }
    }

    if (confirm_pressed() && legal[cury][curx] > 0) place(curx, cury, BLACK);
}

// ── drawing ────────────────────────────────────────────────────────────────
static int cellpx(int x) { return BX + x * CELL; }
static int cellpy(int y) { return BY + y * CELL; }

// draw a disc at cell (x,y) with owner color; flip/drop aware
static void draw_disc(int x, int y) {
    int cxp = cellpx(x) + CELL / 2, cyp = cellpy(y) + CELL / 2;
    int rmax = CELL / 2 - 3;

    if (flipT[y][x] >= 0.0f && flipT[y][x] < 1.0f) {
        // turning over: squash horizontally to 0 at the midpoint, swap color there
        float t = ease_in_out(flipT[y][x]);
        int owner = (t < 0.5f) ? flipFrom[y][x] : flipTo[y][x];
        float sq = (t < 0.5f) ? (1.0f - t * 2.0f) : ((t - 0.5f) * 2.0f);
        int rx = (int)(rmax * sq);
        // green edge glint as it passes through the seam
        if (rx <= 1) { ovalfill(cxp, cyp, 1, rmax, CLR_DARK_GREEN); return; }
        int col = owner == BLACK ? CLR_BLACK : CLR_WHITE;
        ovalfill(cxp, cyp, rx, rmax, col);
        if (owner == BLACK) oval(cxp, cyp, rx, rmax, CLR_DARK_GREY);
        return;
    }

    int owner = board[y][x];
    if (owner == EMPTY) return;
    float r = rmax;
    if (dropT[y][x] >= 0.0f && dropT[y][x] < 1.0f) r = rmax * ease_out(dropT[y][x]);
    int col = owner == BLACK ? CLR_BLACK : CLR_WHITE;
    // soft shadow then disc, with a tiny highlight for shape
    circfill(cxp + 1, cyp + 1, (int)r, CLR_DARKER_GREY);
    circfill(cxp, cyp, (int)r, col);
    if (owner == BLACK) circ(cxp, cyp, (int)r, CLR_DARK_GREY);
    if (r > 4) circfill(cxp - (int)r/3, cyp - (int)r/3, 2, owner == BLACK ? CLR_DARK_GREY : CLR_LIGHT_GREY);
}

void draw(void) {
    cls(CLR_DARKER_GREY);

    // ── felt board ──
    rectfill(BX - 4, BY - 4, BOARD_PX + 8, BOARD_PX + 8, CLR_BROWNISH_BLACK);
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        int felt = ((x + y) & 1) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN;
        rectfill(cellpx(x), cellpy(y), CELL, CELL, felt);
    }
    // grid lines
    for (int i = 0; i <= N; i++) {
        line(BX + i * CELL, BY, BX + i * CELL, BY + BOARD_PX, CLR_BROWNISH_BLACK);
        line(BX, BY + i * CELL, BX + BOARD_PX, BY + i * CELL, CLR_BROWNISH_BLACK);
    }

    // ── legal-move hints + keyboard cursor (human turn only) ──
    if (phase == PLAY) {
        for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) if (legal[y][x] > 0) {
            int cxp = cellpx(x) + CELL / 2, cyp = cellpy(y) + CELL / 2;
            float pulse = 2.0f + sin_deg(now() * 220 + (x + y) * 40) * 1.4f;
            circ(cxp, cyp, (int)(4 + pulse), CLR_LIGHT_YELLOW);
            circfill(cxp, cyp, 2, CLR_YELLOW);
        }
        // cursor box
        int col = legal[cury][curx] > 0 ? CLR_YELLOW : CLR_DARK_RED;
        rect(cellpx(curx), cellpy(cury), CELL, CELL, col);
    }

    // ── discs ──
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) draw_disc(x, y);

    // ── HUD ──
    rectfill(0, 0, SCREEN_W, BY - 6, CLR_BROWNISH_BLACK);
    print("OTHELLO", 6, 4, CLR_LIGHT_YELLOW);
    // black score chip
    circfill(118, 11, 6, CLR_BLACK); circ(118, 11, 6, CLR_DARK_GREY);
    print(str("%d", (int)(showBlack + 0.5f)), 128, 8, CLR_WHITE);
    // white score chip
    circfill(178, 11, 6, CLR_WHITE);
    print(str("%d", (int)(showWhite + 0.5f)), 188, 8, CLR_WHITE);
    // whose turn
    if (phase == PLAY)
        print_right("YOUR MOVE", SCREEN_W - 6, 4, CLR_GREEN);
    else if (phase == CPU_WAIT || (phase == ANIMATING && turn == WHITE))
        print_right("CPU...", SCREEN_W - 6, 4, CLR_BLUE);
    else if (phase == ANIMATING)
        print_right("FLIPPING", SCREEN_W - 6, 4, CLR_LIGHT_GREY);

    // best margin
    int best = load_int("bestmargin", 0);
    if (best > 0) print(str("BEST +%d", best), 6, BY + BOARD_PX + 4, CLR_INDIGO);
    print_right("click a glowing square", SCREEN_W - 6, BY + BOARD_PX + 4, CLR_DARK_GREY);

    // ── pass banner ──
    if (passMsg > 0 && phase != OVER) {
        int a = min(passMsg, 30);
        rectfill(SCREEN_W/2 - 56, 92, 112, 22, CLR_DARKER_BLUE);
        rect(SCREEN_W/2 - 56, 92, 112, 22, CLR_WHITE);
        print_centered(passWho == BLACK ? "BLACK PASSES" : "WHITE PASSES", SCREEN_W/2, 100, a > 4 ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    }

    // ── game-over banner ──
    if (phase == OVER) {
        fade(0.45f);
        int b, w; tally(&b, &w);
        int boxw = 200, bxp = (SCREEN_W - boxw) / 2;
        int top = winner == BLACK ? CLR_DARK_GREEN : winner == WHITE ? CLR_DARK_PURPLE : CLR_DARKER_BLUE;
        rectfill(bxp, 70, boxw, 56, top);
        rect(bxp, 70, boxw, 56, CLR_WHITE);
        const char *msg = winner == BLACK ? "YOU WIN!" : winner == WHITE ? "CPU WINS" : "A TIE";
        print_centered(msg, SCREEN_W/2, 78, winner == BLACK ? CLR_GREEN : winner == WHITE ? CLR_RED : CLR_LIGHT_YELLOW);
        print_centered(str("black %d   white %d", b, w), SCREEN_W/2, 92, CLR_WHITE);
        print_centered("click for a new game", SCREEN_W/2, 108, CLR_YELLOW);
    }
}
