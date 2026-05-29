#include "studio.h"

// ── ESCAPE THE CAVE — a tiny Sierra-style adventure ───────────
// Walk around with the ARROW KEYS, or CLICK where you want to go.
// Stand next to something and press A to look / take / use it.
//
//   • read the sign            (+1)
//   • take the rusty key       (+1)
//   • open the iron door       (+1)  → you escape!
//
// ...and watch your step. This is a Sierra game, after all.

// ── scene geometry ────────────────────────────────────────────
#define FLOOR_TOP 110     // floor starts here
#define BAR_Y     168     // status bar
#define WALK_MINX 12
#define WALK_MAXX 304
#define WALK_MINY 120
#define WALK_MAXY 160

// hotspot stand points (x on the floor)
#define SIGN_X 104
#define KEY_X   44
#define DOOR_X 286
#define ACT_DIST 30

// pit (ellipse) — step in and it's the classic Sierra death
#define PIT_CX 168
#define PIT_CY 150
#define PIT_RX 26
#define PIT_RY 9

// narration messages
enum { MSG_NONE, MSG_INTRO, MSG_SIGN, MSG_KEY, MSG_LOCKED, MSG_PIT, MSG_WIN };

static float px, py;          // player feet position
static int   face = 1;        // 1 right, -1 left
static int   tx = -1, ty;     // click-to-walk target (-1 = none)
static bool  has_key, sign_read, won;
static int   score;
static int   msg = MSG_INTRO;
static bool  ready = false;

static void respawn(void) { px = 28; py = 150; tx = -1; face = 1; }

static void reset_game(void) {
    has_key = sign_read = won = false;
    score = 0; msg = MSG_INTRO;
    respawn();
    ready = true;
}

static bool in_pit(float x, float y) {
    float dx = (x - PIT_CX) / (float)PIT_RX, dy = (y - PIT_CY) / (float)PIT_RY;
    return dx * dx + dy * dy < 1.0f;
}

// which hotspot is the player standing next to? 0 none,1 sign,2 key,3 door
static int near_hotspot(void) {
    if (!has_key && distance((int)px, (int)py, KEY_X, 150) < ACT_DIST) return 2;
    if (px > DOOR_X - 38)                                              return 3;
    if (distance((int)px, (int)py, SIGN_X, 150) < ACT_DIST)           return 1;
    return 0;
}

void update(void) {
    if (!ready) reset_game();

    // a narration box is up: A or click dismisses it (win → restart)
    if (msg != MSG_NONE) {
        if (btnp(0, BTN_A) || mouse_pressed(MOUSE_LEFT)) {
            if (msg == MSG_WIN) reset_game();
            else msg = MSG_NONE;
        }
        return;
    }

    // movement — arrows take priority and cancel any walk-to target
    float vx = 0, vy = 0;
    if (btn(0, BTN_LEFT))  { vx = -1; tx = -1; }
    if (btn(0, BTN_RIGHT)) { vx =  1; tx = -1; }
    if (btn(0, BTN_UP))    { vy = -1; tx = -1; }
    if (btn(0, BTN_DOWN))  { vy =  1; tx = -1; }

    // click anywhere in the scene to walk there
    if (mouse_pressed(MOUSE_LEFT) && mouse_y() < BAR_Y) {
        tx = mouse_x(); ty = (int)clamp(mouse_y(), WALK_MINY, WALK_MAXY);
    }
    if (tx >= 0) {
        if (abs((int)px - tx) > 2) vx = sgn(tx - (int)px);
        if (abs((int)py - ty) > 2) vy = sgn(ty - (int)py);
        if (vx == 0 && vy == 0) tx = -1;
    }
    if (vx != 0) face = vx < 0 ? -1 : 1;

    px = clamp(px + vx * 1.3f, WALK_MINX, WALK_MAXX);
    py = clamp(py + vy * 1.3f, WALK_MINY, WALK_MAXY);

    if (in_pit(px, py)) { msg = MSG_PIT; respawn(); note(40, INSTR_SAW, 4); return; }

    // act on the nearby hotspot
    if (btnp(0, BTN_A)) {
        int h = near_hotspot();
        if (h == 1) { if (!sign_read) { sign_read = true; score++; } msg = MSG_SIGN; }
        else if (h == 2) { has_key = true; score++; msg = MSG_KEY; note(76, INSTR_SINE, 3); }
        else if (h == 3) {
            if (has_key) { won = true; score++; msg = MSG_WIN; strum(60, CHORD_MAJ7, INSTR_TRI, 4, 50); }
            else { msg = MSG_LOCKED; note(48, INSTR_SQUARE, 3); }
        }
    }
}

// ── drawing ───────────────────────────────────────────────────
static void draw_player(void) {
    int x = (int)px, y = (int)py;
    bool walking = (tx >= 0) || btn(0, BTN_LEFT) || btn(0, BTN_RIGHT) || btn(0, BTN_UP) || btn(0, BTN_DOWN);
    int step = walking ? anim(2, 6, 0) : 0;

    circfill(x, y, 4, CLR_DARKER_PURPLE);                       // shadow
    rectfill(x - 3, y - 8, 2, 8, CLR_DARK_BROWN);               // left leg
    rectfill(x + 1, y - 8 + step, 2, 8 - step, CLR_DARK_BROWN); // right leg (shuffles)
    rectfill(x - 4, y - 17, 9, 10, CLR_DARK_GREEN);             // tunic
    rectfill(x - 6, y - 16, 3, 6, CLR_PEACH);                   // back arm
    rectfill(x + 4, y - 16, 3, 6, CLR_PEACH);                   // front arm
    circfill(x, y - 20, 4, CLR_LIGHT_PEACH);                    // head
    pset(x + face * 2, y - 20, CLR_BLACK);                      // eye (faces walk dir)
    rectfill(x - 5, y - 23, 11, 2, CLR_BROWN);                  // hat brim
    rectfill(x - 3, y - 26, 7, 3, CLR_BROWN);                   // hat top
}

static void draw_torch(int x, int y) {
    rectfill(x - 1, y, 2, 8, CLR_DARK_BROWN);                   // bracket
    float fl = noise3(x, 0, now() * 6.0f);                      // flicker
    int h = 6 + (int)(fl * 4);
    trifill(x - 3, y, x + 3, y, x, y - h, CLR_ORANGE);
    trifill(x - 2, y, x + 2, y, x, y - h + 3, CLR_YELLOW);
}

static void draw_box(const char *l1, const char *l2, int col) {
    int w = 240, h = l2 ? 52 : 40, bx = (SCREEN_W - w) / 2, by = 56;
    rectfill(bx, by, w, h, CLR_DARK_BLUE);
    rect(bx, by, w, h, CLR_WHITE);
    rect(bx + 2, by + 2, w - 4, h - 4, CLR_INDIGO);
    print_centered(l1, by + 10, col);
    if (l2) print_centered(l2, by + 22, col);
    print_centered("- press A -", by + h - 12, CLR_LIGHT_GREY);
}

void draw(void) {
    if (!ready) reset_game();

    // ── scene ──
    cls(CLR_BROWNISH_BLACK);                                    // cave dark
    rectfill(0, 0, SCREEN_W, FLOOR_TOP, CLR_DARKER_PURPLE);     // back wall
    rectfill(0, FLOOR_TOP, SCREEN_W, BAR_Y - FLOOR_TOP, CLR_DARK_BROWN);  // floor
    // stalactites
    for (int i = 0; i < 10; i++)
        trifill(i * 34 + 6, 0, i * 34 + 22, 0, i * 34 + 14, 12 + (i % 3) * 6, CLR_BROWNISH_BLACK);

    draw_torch(214, 40);
    draw_torch(70, 40);

    // sign post
    rectfill(SIGN_X - 1, 128, 3, 22, CLR_DARK_BROWN);
    rectfill(SIGN_X - 11, 116, 24, 14, CLR_BROWN);
    rect(SIGN_X - 11, 116, 24, 14, CLR_DARK_BROWN);
    print("!", SIGN_X - 2, 119, CLR_LIGHT_YELLOW);

    // key on a pedestal (until taken)
    rectfill(KEY_X - 6, 144, 12, 12, CLR_DARK_GREY);
    if (!has_key) {
        circfill(KEY_X, 138, 3, CLR_YELLOW);
        circ(KEY_X, 138, 3, CLR_ORANGE);
        rectfill(KEY_X, 138, 2, 7, CLR_YELLOW);
        rectfill(KEY_X + 2, 142, 2, 2, CLR_YELLOW);
    }

    // pit
    for (int yy = -PIT_RY; yy <= PIT_RY; yy++) {
        int half = (int)(PIT_RX * cos_deg(90.0f * yy / (float)PIT_RY));
        line(PIT_CX - half, PIT_CY + yy, PIT_CX + half, PIT_CY + yy, CLR_BLACK);
    }
    rect(PIT_CX - PIT_RX, PIT_CY - PIT_RY, PIT_RX * 2, PIT_RY * 2 + 1, CLR_DARKER_GREY);

    // iron door (right wall)
    int dc = won ? CLR_BLACK : CLR_DARK_GREY;
    rectfill(DOOR_X - 14, 84, 30, 66, dc);
    rect(DOOR_X - 14, 84, 30, 66, CLR_LIGHT_GREY);
    if (!won) {
        line(DOOR_X, 84, DOOR_X, 150, CLR_DARKER_GREY);
        circfill(DOOR_X - 8, 118, 2, has_key ? CLR_GREEN : CLR_YELLOW);   // lock
    } else {
        rectfill(DOOR_X - 10, 92, 22, 54, CLR_BLUE);           // open doorway — the way out
    }

    draw_player();

    // ── status bar ──
    rectfill(0, BAR_Y, SCREEN_W, SCREEN_H - BAR_Y, CLR_BLACK);
    line(0, BAR_Y, SCREEN_W, BAR_Y, CLR_DARK_GREY);
    print(str("SCORE %d of 3", score), 4, BAR_Y + 4, CLR_LIGHT_PEACH);

    // inventory
    print("ITEMS:", 4, BAR_Y + 16, CLR_DARK_GREY);
    if (has_key) {
        int kx = 52;
        circfill(kx, BAR_Y + 19, 3, CLR_YELLOW);
        rectfill(kx, BAR_Y + 19, 8, 2, CLR_YELLOW);
    }

    // contextual hint (when no box is up)
    if (msg == MSG_NONE) {
        const char *hint = "arrows or click to walk";
        int h = near_hotspot();
        if (h == 1) hint = "a weathered sign  - [A] read";
        else if (h == 2) hint = "a rusty key  - [A] take";
        else if (h == 3) hint = has_key ? "an iron door  - [A] open" : "an iron door  - [A] try";
        print_right(hint, SCREEN_W - 4, BAR_Y + 10, CLR_LIGHT_GREY);
    }

    // ── narration boxes ──
    if      (msg == MSG_INTRO)  draw_box("ESCAPE THE CAVE", "find the key, open the door", CLR_LIGHT_PEACH);
    else if (msg == MSG_SIGN)   draw_box("The sign reads:", "\"Beware the pit. Key opens door.\"", CLR_LIGHT_YELLOW);
    else if (msg == MSG_KEY)    draw_box("You take the rusty key.", 0, CLR_YELLOW);
    else if (msg == MSG_LOCKED) draw_box("The door is locked.", "You need a key.", CLR_LIGHT_PEACH);
    else if (msg == MSG_PIT)    draw_box("You fell into a bottomless pit!", "(this is why we save often)", CLR_RED);
    else if (msg == MSG_WIN)    draw_box(str("YOU ESCAPED!  score %d of 3", score), "*** press A to play again ***", CLR_GREEN);
}
