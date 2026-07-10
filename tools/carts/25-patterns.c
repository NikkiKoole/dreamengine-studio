/* de:meta
{
  "slug": "25-patterns",
  "title": "25. entity pool and friendly state",
  "status": "active",
  "created": "2026-07-01",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "lineage": "Track A of tutorial-curriculum.md (#25-32, 'how code actually works'), compressed from 2 planned carts into paged sections of one; sibling of 23-basics/24-building-blocks",
  "description": {
    "summary": "The engine's two signature idioms: a fixed entity pool with an on/off flag, and STATE{}/S-> sugar over de_state().",
    "controls": "left/right change page; A spawns/scores, B kills/loses a life"
  }
}
de:meta */
#include "studio.h"

// 25. ENTITY POOL AND FRIENDLY STATE — Track A of tutorial-curriculum.md, compressed.
// Last sibling of 23-basics/24-building-blocks: same paging scheme (<- -> flips it).
// These two idioms are VISION's core pattern -- almost every non-trivial cart in the
// shelf uses both, usually without ever explaining them to a newcomer.

#define NPAGES 2
static int page = 0;

// ---- page 0: the entity pool ----
// "Enemy e[64]; bool on;" -- a FIXED bank of slots, each either in use or not.
// Nothing is ever malloc'd or removed; a dead slot is just marked off and reused.
#define NMAX 64
typedef struct { bool on; } Enemy;
static Enemy e[NMAX];

void update_pool() {
    if (btnp(0, BTN_A)) {                 // spawn: claim the FIRST inactive slot
        for (int i = 0; i < NMAX; i++) { if (!e[i].on) { e[i].on = true; break; } }
    }
    if (btnp(0, BTN_B)) {                 // despawn: just flip the flag back off
        for (int i = 0; i < NMAX; i++) { if (e[i].on) { e[i].on = false; break; } }
    }
}

void draw_pool() {
    print("typedef struct { bool on; } Enemy;", 4, 44, CLR_LIGHT_GREY);
    print("Enemy e[64]; fixed, reusable bank.", 4, 54, CLR_LIGHT_GREY);

    print("for (i=0; i<64; i++)", 4, 66, CLR_WHITE);
    print("  if (!e[i].on) continue;", 4, 76, CLR_WHITE);
    print("A: spawn (first free slot)", 4, 88, CLR_LIGHT_GREY);
    print("B: kill (first active slot)", 4, 98, CLR_LIGHT_GREY);

    int n = 0;
    for (int i = 0; i < NMAX; i++) {
        int col = (i % 8), row = (i / 8);
        int x = 20 + col * 11, y = 108 + row * 11;
        bool on = e[i].on;
        if (on) n++;
        rectfill(x, y, 10, 10, on ? CLR_YELLOW : CLR_DARKER_GREY);
    }
    print(str("%d / %d", n, NMAX), 124, 140, CLR_GREEN);
    print("active", 124, 150, CLR_GREEN);
}

// ---- page 1: friendly state ----
// STATE{}/S-> is sugar over de_state(): one zero-filled block that survives a
// live-reload, so you never juggle a pile of separate globals by hand.
#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

STATE {
    int score;
    int lives;
};

void update_state() {
    if (btnp(0, BTN_A)) S->score += 10;
    if (btnp(0, BTN_B)) S->lives = max(S->lives - 1, 0);
}

void draw_state() {
    print("#define STATE struct GameState", 4, 44, CLR_LIGHT_GREY);
    print("#define S  ((STATE*)de_state(...))", 4, 54, CLR_LIGHT_GREY);
    print("STATE { int score; int lives; };", 20, 72, CLR_WHITE);
    print("S->score works from ANY function --", 4, 92, CLR_LIGHT_GREY);
    print("no separate global to pass around.", 4, 102, CLR_LIGHT_GREY);

    print(str("S->score = %d   (A adds 10)", S->score), 20, 124, CLR_YELLOW);
    print(str("S->lives = %d   (B loses one)", S->lives), 20, 136, CLR_YELLOW);

    print("de_state() zero-fills on first use.", 4, 160, CLR_DARK_GREY);
    print("it also survives a live-reload edit.", 4, 170, CLR_DARK_GREY);
}

void update() {
    if (btnp(0, BTN_RIGHT)) page = min(page + 1, NPAGES - 1);
    if (btnp(0, BTN_LEFT))  page = max(page - 1, 0);

    switch (page) {
        case 0: update_pool();  break;
        case 1: update_state(); break;
    }
}

void draw() {
    cls(CLR_BROWNISH_BLACK);
    print("entity pool and friendly state.", 4, 4, CLR_WHITE);
    print(str("page %d/%d  (left/right to flip)", page + 1, NPAGES), 4, 14, CLR_LIGHT_PEACH);
    line(4, 24, SCREEN_W - 4, 24, CLR_DARKER_GREY);

    switch (page) {
        case 0: draw_pool();  break;
        case 1: draw_state(); break;
    }
}
