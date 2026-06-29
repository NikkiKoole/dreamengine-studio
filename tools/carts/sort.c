/* de:meta
{
  "title": "sorting visualizer",
  "status": "active",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "algorithm-visualization",
    "sonification"
  ],
  "lineage": "Sorting visualizer/sonifier: algorithms emit a recorded compare/swap event stream that's replayed slowly with pitch-mapped notes per bar.",
  "description": "Watch and hear sorting algorithms run. A sort only ever compares and swaps; this records that stream of operations and replays it slowly, lighting the bars it touches and playing a note pitched to each bar height. Bubble, selection, insertion and quicksort each make a very different sound. Z cycles algorithm, X shuffles, up/down change speed."
}
de:meta */
#include "studio.h"

// SORTING VISUALIZER — watch (and hear) an algorithm put bars in order.
//
// Each bar is a number; taller = bigger. A sorting algorithm only ever does two
// things: COMPARE two bars, and SWAP them. We record that stream of compares and
// swaps, then replay it slowly — lighting up the bars being touched and playing
// a note pitched to each bar's height. Different algorithms, wildly different
// "songs": bubble sort plods, quicksort darts around.
//
//   Z   next algorithm     up/down  faster / slower
//   X   shuffle & restart

#define N      48
#define MAXEV  20000
enum { EV_CMP, EV_SWAP };

typedef struct { unsigned char type; int a, b; } Ev;
static Ev  ev[MAXEV];
static int nev, head;
static int bars[N], scratch[N];
static int algo, speed = 6, hi_a = -1, hi_b = -1, sweep = -1;
static bool running;
static const char *ANAME[4] = { "BUBBLE SORT", "SELECTION SORT", "INSERTION SORT", "QUICKSORT" };

static void rec(int t, int a, int b) { if (nev < MAXEV) { ev[nev].type = t; ev[nev].a = a; ev[nev].b = b; nev++; } }
static void s_cmp(int i, int j) { rec(EV_CMP, i, j); }
static void s_swap(int i, int j) { int t = scratch[i]; scratch[i] = scratch[j]; scratch[j] = t; rec(EV_SWAP, i, j); }

static void gen_bubble(void)    { for (int i = 0; i < N - 1; i++) for (int j = 0; j < N - 1 - i; j++) { s_cmp(j, j + 1); if (scratch[j] > scratch[j + 1]) s_swap(j, j + 1); } }
static void gen_selection(void) { for (int i = 0; i < N - 1; i++) { int m = i; for (int j = i + 1; j < N; j++) { s_cmp(m, j); if (scratch[j] < scratch[m]) m = j; } if (m != i) s_swap(i, m); } }
static void gen_insertion(void) { for (int i = 1; i < N; i++) { int j = i; while (j > 0) { s_cmp(j - 1, j); if (scratch[j - 1] > scratch[j]) { s_swap(j - 1, j); j--; } else break; } } }
static void gen_quick(int lo, int hi) { if (lo >= hi) return; int p = scratch[hi], i = lo; for (int j = lo; j < hi; j++) { s_cmp(j, hi); if (scratch[j] < p) { s_swap(i, j); i++; } } s_swap(i, hi); gen_quick(lo, i - 1); gen_quick(i + 1, hi); }

static void note_for(int val) { hit(40 + val * 40 / 170, INSTR_TRI, 2, 26); }

static void restart(void) {
    for (int i = 0; i < N; i++) bars[i] = (i + 1) * (150 / N) + 8;
    for (int i = N - 1; i > 0; i--) { int j = rnd(i + 1); int t = bars[i]; bars[i] = bars[j]; bars[j] = t; }
    for (int i = 0; i < N; i++) scratch[i] = bars[i];
    nev = 0; head = 0; sweep = -1; hi_a = hi_b = -1; running = true;
    switch (algo) { case 0: gen_bubble(); break; case 1: gen_selection(); break;
                    case 2: gen_insertion(); break; default: gen_quick(0, N - 1); }
}

void init(void) {
    restart();
    for (int upto = nev * 2 / 5; head < upto; head++)   // pre-roll so the thumbnail is mid-sort
        if (ev[head].type == EV_SWAP) { int t = bars[ev[head].a]; bars[ev[head].a] = bars[ev[head].b]; bars[ev[head].b] = t; }
}

void update(void) {
    if (btnp(0, BTN_A)) { algo = (algo + 1) % 4; restart(); return; }
    if (btnp(0, BTN_B)) { restart(); return; }
    if (btnp(0, BTN_UP))   speed = min(40, speed + 2);
    if (btnp(0, BTN_DOWN)) speed = max(1, speed - 1);

    if (running) {
        for (int k = 0; k < speed && head < nev; k++) {
            Ev e = ev[head++]; hi_a = e.a; hi_b = e.b;
            if (e.type == EV_SWAP) { int t = bars[e.a]; bars[e.a] = bars[e.b]; bars[e.b] = t; note_for(bars[e.a]); }
        }
        if (head >= nev) { running = false; sweep = 0; }
    } else if (sweep >= 0 && sweep < N) {
        sweep += 2; note_for(bars[min(sweep, N - 1)]); hi_a = hi_b = -1;
    }
}

void draw(void) {
    cls(CLR_BLACK);
    int w = (SCREEN_W - 16) / N;
    for (int i = 0; i < N; i++) {
        int col = (i == hi_a || i == hi_b) ? CLR_RED : (sweep >= 0 && i < sweep) ? CLR_GREEN : CLR_INDIGO;
        rectfill(8 + i * w, SCREEN_H - 12 - bars[i], w - 1, bars[i], col);
    }
    print(str("%s   %d / %d ops   speed %d", ANAME[algo], head, nev, speed), 4, 4, CLR_WHITE);
    print("Z: next algorithm   X: shuffle   up/down: speed", 4, SCREEN_H - 9, CLR_LIGHT_GREY);
}
