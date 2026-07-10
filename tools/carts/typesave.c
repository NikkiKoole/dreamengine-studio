/* de:meta
{
  "slug": "typesave",
  "title": "21. type & save",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "Keyboard + persistence: text_input() reads typed characters, keyp(KEY_BACKSPACE/KEY_ENTER) handles special keys, and save_bytes()/load_bytes() remember a whole struct between runs — type your name, press ENTER, reopen the cart and it greets you. Also shows text_width() for centering and blink() for the cursor."
}
de:meta */
#include "studio.h"

// TYPE & SAVE — the keyboard + blob-persistence API:
//   text_input()  the characters typed this frame
//   keyp(KEY_*)   special keys (BACKSPACE to delete, ENTER to save)
//   save_bytes()/load_bytes()  remember a whole struct between runs
//   text_width()  to center text   ·   blink() for the cursor
//
// Type your name, press ENTER — close the cart and reopen it: it remembers.

typedef struct { int len; char text[16]; } Name;

static Name cur;          // what you're typing now
static Name saved;        // what was loaded from a previous run
static int  hasSaved;
static float msgT;

static void load_saved(void) {
    Name r;
    int n = load_bytes(&r, sizeof r);
    if (n == (int)sizeof r && r.len >= 0 && r.len < 16) { saved = r; saved.text[saved.len] = 0; hasSaved = 1; }
    else hasSaved = 0;
}

void init(void) {
    cur.len = 0; cur.text[0] = 0; msgT = 0;
    load_saved();
}

void update(void) {
    // append every printable character typed this frame
    const char *in = text_input();
    for (int i = 0; in[i]; i++) {
        char c = in[i];
        if (c >= 32 && c < 127 && cur.len < 15) { cur.text[cur.len++] = c; cur.text[cur.len] = 0; }
    }
    if (keyp(KEY_BACKSPACE) && cur.len > 0) cur.text[--cur.len] = 0;
    if (keyp(KEY_ENTER) && cur.len > 0) {
        save_bytes(&cur, sizeof cur);          // persist the whole struct
        saved = cur; hasSaved = 1; msgT = 2.0f;
        note(76, INSTR_SINE, 4);
    }
    if (msgT > 0) msgT -= dt();
}

void draw(void) {
    cls(CLR_DARKER_PURPLE);

    const char *title = "TYPE & SAVE";
    print_scaled(title, (SCREEN_W - text_width(title) * 2) / 2, 12, CLR_YELLOW, 2);

    // a line loaded from a previous run
    if (hasSaved) {
        const char *w = str("welcome back, %s!", saved.text);
        print(w, (SCREEN_W - text_width(w)) / 2, 46, CLR_LIGHT_PEACH);
    } else {
        print_centered("(nothing saved yet)", SCREEN_W/2, 46, CLR_DARK_GREY);
    }

    // input box
    print_centered("your name:", SCREEN_W/2, 80, CLR_LIGHT_GREY);
    int boxw = 160, bx = (SCREEN_W - boxw) / 2;
    rect(bx, 92, boxw, 22, CLR_LIGHT_GREY);

    // typed text with a blinking cursor, centered in the box
    const char *cursor = blink(20) ? "|" : " ";
    const char *shown = str("%s%s", cur.text, cursor);
    print(shown, (SCREEN_W - text_width(shown)) / 2, 99, CLR_WHITE);

    if (msgT > 0) print_centered("saved!", SCREEN_W/2, 122, CLR_GREEN);

    hint("letters to type    BACKSPACE    ENTER = save");
}
