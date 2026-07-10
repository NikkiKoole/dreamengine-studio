/* de:meta
{
  "slug": "23-basics",
  "title": "23. variables, conditionals, loops",
  "status": "active",
  "created": "2026-07-01",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "lineage": "Track A of tutorial-curriculum.md (#25-32, 'how code actually works'), compressed from 3 planned carts into paged sections of one",
  "description": {
    "summary": "The C basics every tutorial has used silently since cart 3, taught explicitly: int/float/bool, if/else, for/while.",
    "controls": "left/right change page; up/down adjust each page's live value; Z toggles rain on page 2"
  }
}
de:meta */
#include "studio.h"

// 23. VARIABLES, CONDITIONALS, LOOPS — Track A of tutorial-curriculum.md, compressed.
// Three pages, one cart: <- -> flips the lesson, ^v tweaks the live number on it.
// Every tutorial since 03-move has used int/float/bool/if/for without ever teaching
// them — this is that lesson, made of things you can nudge and watch change.

#define NPAGES 3
static int page = 0;

// ---- page 0: variables & types ----
static int div_a = 7;   // ^v adjusts this

void update_vars() {
    if (btnp(0, BTN_UP))   div_a = min(div_a + 1, 20);
    if (btnp(0, BTN_DOWN)) div_a = max(div_a - 1, 0);
}

void draw_vars() {
    int   b = 2;
    int   int_result   = div_a / b;             // int / int TRUNCATES -- drops the remainder
    float float_result = (float)div_a / b;       // casting one side to float keeps the fraction
    bool  even          = (div_a % 2 == 0);

    print("variables have a TYPE.", 4, 44, CLR_LIGHT_GREY);
    print("the compiler needs to know the size.", 4, 54, CLR_LIGHT_GREY);

    print(str("int   a = %d;", div_a), 20, 76, CLR_WHITE);
    print("int   b = 2;", 20, 86, CLR_WHITE);
    print(str("a / b        = %d   (int)", int_result), 20, 104, CLR_YELLOW);
    print("int / int TRUNCATES the remainder", 20, 114, CLR_DARK_GREY);
    print(str("(float)a / b = %.2f  (float)", float_result), 20, 130, CLR_GREEN);
    print("casting keeps the fraction", 20, 140, CLR_DARK_GREY);

    print(str("even = (a %% 2 == 0) -> %s", even ? "true" : "false"),
          20, 160, even ? CLR_GREEN : CLR_RED);
}

// ---- page 1: conditionals ----
static int  temp    = 20;   // ^v adjusts this
static bool raining = false; // BTN_A (Z) toggles this

void update_cond() {
    if (btnp(0, BTN_UP))   temp = min(temp + 1, 40);
    if (btnp(0, BTN_DOWN)) temp = max(temp - 1, 0);
    if (btnp(0, BTN_A))    raining = !raining;
}

void draw_cond() {
    const char *label; int col;
    if      (temp < 10) { label = "COLD"; col = CLR_BLUE; }
    else if (temp < 20) { label = "COOL"; col = CLR_BLUE_GREEN; }
    else if (temp < 30) { label = "WARM"; col = CLR_ORANGE; }
    else                { label = "HOT";  col = CLR_RED; }
    bool wear_coat = (temp < 15) || raining;

    print("if / else if / else -- ONE branch.", 4, 44, CLR_LIGHT_GREY);
    print("&& and || combine yes/no checks.", 4, 54, CLR_LIGHT_GREY);

    print(str("temp = %d  (up/down to change)", temp), 20, 70, CLR_WHITE);
    print("if (temp < 10)      COLD", 20, 84, CLR_WHITE);
    print("else if (temp < 20) COOL", 20, 94, CLR_WHITE);
    print("else if (temp < 30) WARM", 20, 104, CLR_WHITE);
    print("else                HOT", 20, 114, CLR_WHITE);

    rectfill(20, 128, 140, 24, col);
    print(label, 72, 135, CLR_BLACK);

    print(str("raining: %s  (Z toggles)", raining ? "yes" : "no"), 20, 160, CLR_LIGHT_GREY);
    print("if (temp<15 || raining) coat=true", 20, 172, CLR_WHITE);
    print(str("-> wear a coat? %s", wear_coat ? "YES" : "no"), 20, 184,
          wear_coat ? CLR_YELLOW : CLR_DARK_GREY);
}

// ---- page 2: loops ----
static int n = 5;   // ^v adjusts this

void update_loops() {
    if (btnp(0, BTN_UP))   n = min(n + 1, 16);
    if (btnp(0, BTN_DOWN)) n = max(n - 1, 0);
}

void draw_loops() {
    print("a FOR loop repeats a known N times.", 4, 44, CLR_LIGHT_GREY);
    print("one line draws the whole row below.", 4, 54, CLR_LIGHT_GREY);

    print("for (i = 0; i < n; i++)", 20, 70, CLR_WHITE);
    print(str("  rectfill(...);   n = %d", n), 20, 80, CLR_WHITE);
    for (int i = 0; i < n; i++) {
        rectfill(20 + i * 18, 96, 14, 14, CLR_YELLOW);
    }
    if (n == 0) print("(n = 0 -- the loop body never runs)", 20, 96, CLR_DARK_GREY);

    int i = 1, sum = 0;
    while (i <= n) { sum += i; i++; }   // a WHILE loop repeats until a condition goes false
    print("a WHILE loop repeats UNTIL false:", 4, 136, CLR_LIGHT_GREY);
    print("int i = 1, sum = 0;", 20, 152, CLR_WHITE);
    print("while (i <= n) { sum += i; i++; }", 20, 162, CLR_WHITE);
    print(str("-> sum 1..%d = %d", n, sum), 20, 176, CLR_GREEN);
}

void update() {
    if (btnp(0, BTN_RIGHT)) page = min(page + 1, NPAGES - 1);
    if (btnp(0, BTN_LEFT))  page = max(page - 1, 0);

    switch (page) {
        case 0: update_vars();  break;
        case 1: update_cond();  break;
        case 2: update_loops(); break;
    }
}

void draw() {
    cls(CLR_DARKER_BLUE);
    print("variables, conditionals, loops.", 4, 4, CLR_WHITE);
    print(str("page %d/%d  (arrows: page / adjust)", page + 1, NPAGES), 4, 14, CLR_MEDIUM_GREY);
    line(4, 24, SCREEN_W - 4, 24, CLR_DARKER_GREY);

    switch (page) {
        case 0: draw_vars();  break;
        case 1: draw_cond();  break;
        case 2: draw_loops(); break;
    }
}
