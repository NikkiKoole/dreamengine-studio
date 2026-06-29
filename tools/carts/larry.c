/* de:meta
{
  "title": "Leisure Suit Larry",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "dialogue-tree",
    "inventory-system"
  ],
  "lineage": "Built directly on the zak.c SCUMM verb/hotspot engine; novel in layering a multi-condition win state (props + dialogue line together) and a walking-to-act proximity system over that chassis.",
  "genre": "adventure",
  "homage": "Leisure Suit Larry (1987)",
  "description": "A sleazy-lounge point-and-click in the Sierra tradition, played for groans. You're Larry Laffer — polyester suit, gold medallion, hopeful mustache — at the neon-lit Lucky Lips Lounge, out to charm Roxy the cocktail waitress into a date. She's heard every line, so you'll need to swipe the right props and pick the right words: TAKE the rose off the bar, USE the MUSK MAGNET cologne dispenser, feed the jukebox a borrowed quarter, then sashay to the back booth and TALK your way through a menu of pickup lines. Most earn a withering eye-roll (or a full-on SLAP that shakes the screen); arrive holding both props and pick the genuinely smooth line and the candlelit night is yours. Built on the SCUMM verb/inventory scene engine across two velvet rooms, with cheeky PG-13 innuendo only. Mouse only: click a verb (Look/Use/Take/Talk), then a hotspot to act or the floor to walk; click an inventory slot to hold an item then a hotspot to use it; click a dialogue line in the booth; click to play again."
}
de:meta */
#include "studio.h"
#include <stddef.h>

// ── LEISURE SUIT LARRY (PG-13) — a sleazy-lounge point-and-click ──────
// You're Larry Laffer, polyester suit and all, at the Lucky Lips Lounge.
// Goal: charm Roxy the waitress into a date. She's heard every line —
// so you need the right props AND the right words.
//
//   TAKE the rose off the bar.  USE the cologne dispenser (a dab of musk).
//   Walk to the BOOTH, TALK to Roxy, and pick the *smooth* line while
//   holding both — anything less gets you an eye-roll (or a slap).
//
// Built on the zak.c SCUMM scene engine: pick a verb, click a hotspot,
// click the floor to walk, click an inventory item to hold then use it.
// Two scenes: the bar, and Roxy's back booth (with a dialogue exchange).
// Cheeky innuendo only — nothing explicit. The whole thing's a wink.

// ── layout ───────────────────────────────────────────────────────────
#define BAR_Y     150         // bottom UI bar starts here
#define FLOOR_TOP 104         // walkable floor begins
#define WALK_MINX 14
#define WALK_MAXX 306
#define ACT_DIST  52          // how close Larry must be to act

// scenes
enum { SC_BAR, SC_BOOTH, NSCENE };

// verbs
enum { V_LOOK, V_USE, V_TAKE, V_TALK, NVERB };
static const char *VERB_NAME[NVERB] = { "Look", "Use", "Take", "Talk" };

// inventory items (bitset)
enum { IT_ROSE = 1, IT_COLOGNE = 2, IT_COIN = 4 };

// ── hotspots ──────────────────────────────────────────────────────────
typedef struct { int x, y, w, h; int standx; const char *name; } Hotspot;

enum { B_BARMAN, B_ROSE, B_JUKE, B_COLOGNE, B_TIPJAR, B_DOOR, B_BOOTHX, B_N };
static Hotspot bar_hs[B_N] = {
    [B_BARMAN]  = { 132, 70, 28, 34, 150, "bartender" },
    [B_ROSE]    = { 196, 96, 12, 14, 200, "red rose" },
    [B_JUKE]    = { 36, 56, 40, 60, 86, "jukebox" },
    [B_COLOGNE] = { 252, 78, 16, 24, 246, "cologne dispenser" },
    [B_TIPJAR]  = { 168, 94, 16, 12, 168, "tip jar" },
    [B_DOOR]    = { 8, 50, 26, 60, 44, "front door" },
    [B_BOOTHX]  = { 288, 56, 26, 86, 286, "to the booth >" },
};

enum { K_ROXY, K_TABLE, K_CANDLE, K_BACK, K_N };
static Hotspot booth_hs[K_N] = {
    [K_ROXY]   = { 178, 56, 42, 56, 168, "Roxy" },
    [K_TABLE]  = { 96, 104, 70, 20, 110, "booth table" },
    [K_CANDLE] = { 120, 88, 12, 18, 110, "candle" },
    [K_BACK]   = { 4, 56, 22, 86, 30, "< back to the bar" },
};

// ── dialogue tree (the smooth-talk exchange) ──────────────────────────
enum { L_OPEN, L_SUIT, L_DRINK, L_SMOOTH, L_N };
static const char *LINE_TXT[L_N] = {
    "\"Come here often, gorgeous?\"",
    "\"Like the suit? It's machine washable.\"",
    "\"Can I buy you something tall and cold?\"",
    "\"You look like you could use a quiet evening.\"",
};

// ── state ─────────────────────────────────────────────────────────────
static bool  ready = false;
static int   scene = SC_BAR;
static int   verb = V_LOOK;
static float px, py;                 // Larry feet position
static int   face = 1;
static int   tx = -1, ty;            // walk target (-1 none)
static int   pending = -1;           // hotspot id queued until Larry arrives
static int   inv = 0;                // held items bitset
static int   held = 0;               // currently selected inv item (0 none)
static char  msg[80];
static float msg_t = 0;              // seconds remaining on the message
static bool  won = false;
static float win_t = 0;
static bool  talking = false;        // dialogue menu open in the booth
static int   lines_tried = 0;        // bitset of which lines tried
static float roll_t = 0;             // Roxy eye-roll timer
static float slap_t = 0;             // slap flash timer
static float heart_t = 0;            // win heart pop timer
static bool  juke_on = false;

// instruments
static void setup_audio(void) {
    instrument(5, INSTR_SINE, 30, 80, 4, 400);      // smooth sax
    instrument_lfo(5, 0, LFO_PITCH, 5.0f, 0.3f);
    instrument(6, INSTR_TRI, 4, 40, 3, 200);        // jukebox warm
    instrument(7, INSTR_SQUARE, 2, 30, 1, 60);      // UI blip
}

static void say(const char *s) {
    int i = 0; while (s[i] && i < 79) { msg[i] = s[i]; i++; } msg[i] = 0;
    msg_t = 3.6f;
}

static void reject(const char *s) {  // a put-down: eye-roll + clunk
    say(s); roll_t = 1.4f; note(50, INSTR_SQUARE, 3);
}

static void respawn(int s) {
    scene = s; tx = -1; pending = -1; talking = false;
    py = 138;
    px = (s == SC_BAR) ? 150 : 130;
}

static void reset_game(void) {
    setup_audio();
    inv = 0; held = 0; verb = V_LOOK;
    won = false; talking = false; juke_on = false;
    lines_tried = 0;
    msg[0] = 0; msg_t = 0; win_t = 0; roll_t = 0; slap_t = 0; heart_t = 0;
    respawn(SC_BAR);
    ready = true;
    say("Lucky Lips Lounge. There's a doll in the back booth...");
}

static Hotspot *scene_hs(int *n) {
    if (scene == SC_BAR) { *n = B_N; return bar_hs; }
    *n = K_N; return booth_hs;
}

static bool hs_active(int id) {
    if (scene == SC_BAR) {
        if (id == B_ROSE) return !(inv & IT_ROSE);    // gone once taken
    }
    return true;
}

static int hover_hotspot(void) {
    int n; Hotspot *hs = scene_hs(&n);
    int mx = mouse_x(), my = mouse_y();
    if (my >= BAR_Y) return -1;
    int best = -1;
    for (int i = 0; i < n; i++) {
        if (!hs_active(i)) continue;
        if (point_in_box(mx, my, hs[i].x, hs[i].y, hs[i].w, hs[i].h)) best = i;
    }
    return best;
}

static bool larry_near(int id) {
    int n; Hotspot *hs = scene_hs(&n);
    return abs((int)px - hs[id].standx) < ACT_DIST;
}

// ── verb resolution ────────────────────────────────────────────────────
static void try_exit(int s) { respawn(s); msg_t = 0; }

static void act_bar(int id) {
    if (held) {  // using a held inventory item on a hotspot
        if (held == IT_COIN && id == B_JUKE && !juke_on) {
            juke_on = true; held = 0;
            strum(48, CHORD_MAJ7, 6, 4, 70);
            say("The jukebox croons. Smooth. Real smooth.");
            return;
        }
        if (held == IT_ROSE && id == B_TIPJAR) {
            say("A rose in the tip jar? She'll think you're cheap.");
            held = 0; return;
        }
        reject("That gets you nowhere, lover boy.");
        return;
    }
    switch (verb) {
    case V_LOOK:
        if (id == B_BARMAN)  say("Lou the barkeep. Seen-it-all eyes.");
        else if (id == B_ROSE)    say("A single red rose. Bold. Classic.");
        else if (id == B_JUKE)    say(juke_on ? "Spinning a slow number." : "A jukebox. Needs a coin.");
        else if (id == B_COLOGNE) say("'MUSK MAGNET' — one dab, big promises.");
        else if (id == B_TIPJAR)  say("A tip jar with a lonely quarter.");
        else if (id == B_DOOR)    say("The street. But the night is young.");
        else if (id == B_BOOTHX)  say("The back booth. Roxy's section.");
        break;
    case V_TAKE:
        if (id == B_ROSE)   { inv |= IT_ROSE; note(72, 6, 3); say("Got the rose. A gentleman's weapon."); }
        else if (id == B_TIPJAR) {
            if (inv & IT_COIN) say("One quarter's enough. Don't be greedy.");
            else { inv |= IT_COIN; note(76, 7, 3); say("Borrowed a quarter. For the jukebox, honest."); }
        }
        else if (id == B_BARMAN) say("Lou doesn't tip easy.");
        else say("Can't pocket that.");
        break;
    case V_USE:
        if (id == B_COLOGNE) {
            if (inv & IT_COLOGNE) say("You're plenty fragrant already, tiger.");
            else { inv |= IT_COLOGNE; note(64, INSTR_NOISE, 2); note(79, 5, 3);
                   say("*pssht* A dab of musk. The ladies won't stand a chance."); }
        }
        else if (id == B_JUKE) {
            if (juke_on) say("Already playing your song.");
            else { note(48, INSTR_SQUARE, 3); say("It wants a coin. Where's a quarter..."); }
        }
        else if (id == B_BARMAN) { note(60, 6, 2); say("\"What'll it be?\" \"Confidence. Double.\""); }
        else if (id == B_BOOTHX) try_exit(SC_BOOTH);
        else if (id == B_DOOR)   say("And miss my big chance? Not tonight.");
        else say("Nothing happens.");
        break;
    case V_TALK:
        if (id == B_BARMAN) say("\"Any tips, Lou?\" \"Yeah. Shower.\"");
        else if (id == B_BOOTHX) try_exit(SC_BOOTH);
        else say("It doesn't talk back.");
        break;
    }
}

// dialogue: clicking line `l` while talking to Roxy
static void say_line(int l) {
    lines_tried |= (1 << l);
    note(74, 7, 2);
    switch (l) {
    case L_OPEN:
        reject("\"Often enough to hear THAT one nightly.\"");
        break;
    case L_SUIT:
        reject("\"It shows. Maybe wash the line too.\"");
        break;
    case L_DRINK:
        say("\"...Cute. But drinks won't do it, slick.\"");
        roll_t = 0.8f;
        break;
    case L_SMOOTH:
        // the win — only if he's holding BOTH props
        if ((inv & IT_ROSE) && (inv & IT_COLOGNE)) {
            won = true; win_t = 0; heart_t = 0; talking = false;
            held = 0;
            note(64, 5, 4); strum(60, CHORD_MAJ7, 5, 4, 90);
            say("She smiles. \"...Pick me up at eight, smooth guy.\"");
        } else if (!(inv & IT_ROSE)) {
            reject("\"Sweet words. Where's the flowers, Romeo?\"");
        } else {
            // has rose, no cologne -> the slap gag
            slap_t = 0.9f; shake(4); note(40, INSTR_NOISE, 4);
            reject("*SLAP* \"You smell like a gym sock, pal.\"");
        }
        break;
    }
}

static void act_booth(int id) {
    if (held) {
        if (held == IT_ROSE && id == K_ROXY) {
            say("\"A rose? ...Keep holding it, we'll see.\"");
            held = 0; return;
        }
        reject("Roxy raises one perfect eyebrow.");
        return;
    }
    switch (verb) {
    case V_LOOK:
        if (id == K_ROXY)   say("Roxy. Out of your league, but who's counting?");
        else if (id == K_TABLE)  say("A booth for two. Optimistic of you.");
        else if (id == K_CANDLE) say("A candle. Mood lighting. Very nice.");
        else if (id == K_BACK)   say("Back to the bar.");
        break;
    case V_TALK:
        if (id == K_ROXY) { talking = true; note(72, 7, 2); }
        else say("Talking to furniture now, Larry?");
        break;
    case V_USE:
        if (id == K_BACK) try_exit(SC_BAR);
        else if (id == K_ROXY) say("Use your WORDS. TALK to her.");
        else say("Nothing happens.");
        break;
    case V_TAKE:
        if (id == K_CANDLE) say("Arson's not a great first impression.");
        else if (id == K_ROXY) reject("\"Hands to yourself, hot shot.\"");
        else say("Can't take that.");
        break;
    }
}

static void run_verb(int id) {
    if (scene == SC_BAR) act_bar(id); else act_booth(id);
}

// ── update ─────────────────────────────────────────────────────────────
void update(void) {
    if (!ready) reset_game();
    if (msg_t  > 0) msg_t  -= dt();
    if (roll_t > 0) roll_t -= dt();
    if (slap_t > 0) slap_t -= dt();
    if (won) { win_t += dt(); heart_t += dt(); }

    if (won) {
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_SPACE)) reset_game();
        return;
    }

    int mx = mouse_x(), my = mouse_y();

    // ── dialogue menu takes clicks first (booth only) ──
    if (talking) {
        if (mouse_pressed(MOUSE_LEFT)) {
            int top = 30, lh = 16;
            int sel = (my - top) / lh;
            if (my >= top && sel >= 0 && sel < L_N && mx < 250) {
                say_line(sel);
            } else if (my >= top + L_N * lh && my < top + L_N * lh + 14) {
                talking = false;                 // "(leave)" row
                note(50, 7, 2);
            }
        }
        return;
    }

    if (mouse_pressed(MOUSE_LEFT)) {
        // 1) verb bar — four buttons in a 2x2
        if (my >= BAR_Y && my < BAR_Y + 24 && mx < 86) {
            int v = my < BAR_Y + 12 ? (mx < 43 ? V_LOOK : V_USE)
                                    : (mx < 43 ? V_TAKE : V_TALK);
            verb = v; held = 0;
            note(70, 7, 2);
        }
        // 2) inventory strip — slots from x=92
        else if (my >= BAR_Y) {
            int slot = (mx - 92) / 22;
            int present[3], pn = 0;
            if (inv & IT_ROSE)    present[pn++] = IT_ROSE;
            if (inv & IT_COLOGNE) present[pn++] = IT_COLOGNE;
            if (inv & IT_COIN)    present[pn++] = IT_COIN;
            if (mx >= 92 && slot >= 0 && slot < pn) {
                int it = present[slot];
                if (verb == V_LOOK) {
                    if (it == IT_ROSE)    say("A red rose. Smells better than I do.");
                    else if (it == IT_COLOGNE) say("Eau de Larry. Industrial strength.");
                    else if (it == IT_COIN)    say("A borrowed quarter. The jukebox calls.");
                } else {
                    held = (held == it) ? 0 : it;
                    note(72, 7, 2);
                }
            }
        }
        // 3) scene click — hotspot or floor
        else {
            int h = hover_hotspot();
            if (h >= 0) {
                pending = h;
                int n; Hotspot *hs = scene_hs(&n);
                tx = (int)clamp(hs[h].standx, WALK_MINX, WALK_MAXX);
                ty = (int)clamp(py, 116, 144);
            } else if (my >= FLOOR_TOP) {
                tx = (int)clamp(mx, WALK_MINX, WALK_MAXX);
                ty = (int)clamp(my, 116, 144);
                pending = -1;
            }
        }
    }

    // walk toward target
    if (tx >= 0) {
        float sp = 80.0f * dt();
        if (abs((int)px - tx) > 2) { float d = (float)sgn(tx - (int)px); px += d * sp; face = d < 0 ? -1 : 1; }
        if (abs((int)py - ty) > 2) py += sgn(ty - (int)py) * sp;
        px = clamp(px, WALK_MINX, WALK_MAXX);
        if (abs((int)px - tx) <= 2 && abs((int)py - ty) <= 2) {
            tx = -1;
            if (pending >= 0) {
                if (larry_near(pending)) run_verb(pending);
                else say("I can't reach that from here.");
                pending = -1;
            }
        }
    }

    if (keyp(KEY_SPACE) || keyp(KEY_ESCAPE)) { msg_t = 0; talking = false; }
}

// ── drawing ─────────────────────────────────────────────────────────────
static void draw_larry(void) {
    int x = (int)px, y = (int)py;
    bool walking = tx >= 0;
    int step = walking ? anim(2, 7, 0) : 0;
    ovalfill(x, y, 8, 3, CLR_DARKER_PURPLE);                   // shadow
    rectfill(x - 3, y - 9, 3, 9 - step, CLR_WHITE);            // far leg (white slacks)
    rectfill(x + 1, y - 9 + step, 3, 9 - step, CLR_WHITE);     // near leg
    rectfill(x - 5, y - 21, 11, 13, CLR_LIGHT_GREY);           // leisure jacket
    trifill(x - 5, y - 21, x, y - 14, x + 5, y - 21, CLR_DARK_RED); // open shirt V
    rectfill(x - 7, y - 20, 3, 9, CLR_LIGHT_PEACH);            // back arm
    rectfill(x + 4, y - 20, 3, 9, CLR_LIGHT_PEACH);            // front arm
    if (inv & IT_ROSE) { line(x + 6, y - 18, x + 6, y - 25, CLR_DARK_GREEN);
                          circfill(x + 6, y - 26, 2, CLR_RED); }     // rose in hand
    circfill(x, y - 25, 4, CLR_LIGHT_PEACH);                   // head
    rectfill(x - 4, y - 30, 9, 4, CLR_DARK_BROWN);             // toupee
    pset(x + face * 2, y - 25, CLR_BLACK);                     // eye
    pset(x - 1, y - 22, CLR_DARK_BROWN);                       // mustache
    rectfill(x - 1, y - 18, 2, 3, CLR_YELLOW);                 // gold medallion
}

static void draw_roxy(int x, int y) {
    bool roll = roll_t > 0;
    ovalfill(x, y + 30, 9, 3, CLR_DARKER_PURPLE);
    rectfill(x - 8, y + 2, 16, 28, CLR_DARK_PURPLE);          // cocktail dress
    trifill(x - 8, y + 30, x + 8, y + 30, x, y + 14, CLR_DARK_PURPLE);
    rectfill(x - 8, y + 4, 4, 12, CLR_LIGHT_PEACH);           // arms
    rectfill(x + 4, y + 4, 4, 12, CLR_LIGHT_PEACH);
    circfill(x, y - 4, 6, CLR_LIGHT_PEACH);                   // head
    // hair
    arcfill(x, y - 6, 8, 180, 360, CLR_DARK_RED);
    rectfill(x - 8, y - 6, 3, 10, CLR_DARK_RED);
    rectfill(x + 5, y - 6, 3, 10, CLR_DARK_RED);
    // eyes — roll up on a put-down
    int ey = roll ? y - 7 : y - 5;
    pset(x - 2, ey, CLR_BLACK); pset(x + 2, ey, CLR_BLACK);
    // lips
    rectfill(x - 1, y - 1, 3, 1, CLR_RED);
    // a tray
    circfill(x - 12, y + 12, 4, CLR_MEDIUM_GREY);
}

static void draw_label(const char *s, int col) {
    int w = text_width(s) + 8, x = mid(2, mouse_x() - w / 2, SCREEN_W - w - 2);
    int yy = mouse_y() - 12; if (yy < 2) yy = mouse_y() + 8;
    fillp(FILL_CHECKER, -1);
    rectfill(x, yy, w, 11, CLR_BLACK);
    fillp_reset();
    print(s, x + 4, yy + 2, col);
}

static void scene_bar(void) {
    cls(CLR_DARKER_PURPLE);
    rectfill(0, 0, SCREEN_W, FLOOR_TOP, CLR_DARK_PURPLE);            // wall
    // velvet wall sheen
    fillp(FILL_VLINES, CLR_DARKER_PURPLE); rectfill(0, 0, SCREEN_W, FLOOR_TOP, CLR_MAUVE); fillp_reset();
    rectfill(0, FLOOR_TOP, SCREEN_W, BAR_Y - FLOOR_TOP, CLR_DARK_BROWN); // carpet
    fillp(FILL_DOTS, -1); rectfill(0, FLOOR_TOP, SCREEN_W, BAR_Y - FLOOR_TOP, CLR_DARK_RED); fillp_reset();
    line(0, FLOOR_TOP, SCREEN_W, FLOOR_TOP, CLR_BLACK);
    // neon "LUCKY LIPS" sign
    int neon = blink(16) ? CLR_PINK : CLR_DARK_PURPLE;
    print("LUCKY LIPS", 116, 12, neon);
    arcfill(146, 28, 7, 0, 180, blink(16) ? CLR_RED : CLR_DARK_RED);  // lips
    line(139, 28, 153, 28, CLR_BROWNISH_BLACK);
    // jukebox (left)
    rectfill(36, 56, 40, 60, CLR_DARK_RED); rect(36, 56, 40, 60, CLR_RED);
    arcfill(56, 60, 18, 180, 360, CLR_DARK_ORANGE);                 // dome top
    rectfill(44, 78, 24, 22, CLR_BLACK);
    for (int i = 0; i < 5; i++) {
        int on = juke_on && ((i + frame() / 8) % 5 == 0);
        rectfill(47 + i * 5, 80 + (on ? -2 : 0), 3, 18, on ? CLR_LIGHT_YELLOW : CLR_TRUE_BLUE);
    }
    if (juke_on) { int nx = 30 + (frame() % 40); print("o", nx, 50 - (frame() % 8), CLR_PINK); }
    // long bar counter
    rectfill(96, 96, 180, 8, CLR_BROWN);
    rectfill(96, 104, 180, 6, CLR_DARK_BROWN);
    // bartender behind it
    circfill(146, 80, 8, CLR_BROWN);                                // torso/apron back
    circfill(146, 70, 6, CLR_DARK_PEACH);                           // head
    rectfill(140, 64, 13, 3, CLR_BLACK);                            // slicked hair
    pset(143, 70, CLR_BLACK); pset(149, 70, CLR_BLACK);
    rectfill(138, 88, 16, 8, CLR_WHITE);                            // shirt
    // tip jar
    rectfill(168, 88, 14, 8, CLR_BLUE); rect(168, 88, 14, 8, CLR_LIGHT_GREY);
    circfill(175, 94, 1, CLR_LIGHT_GREY);
    print("TIPS", 166, 82, CLR_LIGHT_YELLOW);
    // rose on the bar
    if (!(inv & IT_ROSE)) { line(200, 96, 200, 106, CLR_DARK_GREEN); circfill(200, 95, 3, CLR_RED);
                            pset(202, 100, CLR_DARK_GREEN); }
    // cologne dispenser (right, by the restroom sign)
    rectfill(250, 78, 16, 24, CLR_LIGHT_GREY); rect(250, 78, 16, 24, CLR_MEDIUM_GREY);
    rectfill(254, 72, 8, 8, CLR_TRUE_BLUE);
    print("MUSK", 248, 64, CLR_LIGHT_YELLOW);
    // front door (left)
    rectfill(8, 50, 26, 56, CLR_BROWNISH_BLACK); rect(8, 50, 26, 56, CLR_DARK_BROWN);
    circfill(30, 80, 1, CLR_YELLOW);
    // exit to booth (right)
    rectfill(288, 56, 26, 50, CLR_DARKER_PURPLE);
    if (blink(20)) print(">", 297, 76, CLR_PINK);
    print("booth", 285, 92, CLR_MAUVE);
}

static void scene_booth(void) {
    cls(CLR_BROWNISH_BLACK);
    rectfill(0, 0, SCREEN_W, FLOOR_TOP, CLR_DARKER_PURPLE);
    fillp(FILL_VLINES, CLR_BROWNISH_BLACK); rectfill(0, 0, SCREEN_W, FLOOR_TOP, CLR_DARKER_PURPLE); fillp_reset();
    rectfill(0, FLOOR_TOP, SCREEN_W, BAR_Y - FLOOR_TOP, CLR_DARK_BROWN);
    fillp(FILL_DOTS, -1); rectfill(0, FLOOR_TOP, SCREEN_W, BAR_Y - FLOOR_TOP, CLR_DARK_RED); fillp_reset();
    line(0, FLOOR_TOP, SCREEN_W, FLOOR_TOP, CLR_BLACK);
    // back-arrow exit
    rectfill(4, 56, 22, 50, CLR_BROWNISH_BLACK); rect(4, 56, 22, 50, CLR_DARK_BROWN);
    if (blink(20)) print("<", 9, 76, CLR_PINK);
    // a private booth — curved velvet bench behind the table
    arcfill(130, 96, 56, 180, 360, CLR_DARK_RED);
    arc(130, 96, 56, 180, 360, CLR_RED);
    // table
    rectfill(96, 100, 70, 8, CLR_DARK_BROWN);
    rectfill(96, 108, 70, 16, CLR_BROWNISH_BLACK);
    ovalfill(131, 100, 35, 5, CLR_BROWN);
    // candle on the table (flickering)
    rectfill(127, 92, 4, 10, CLR_LIGHT_PEACH);
    int fl = (frame() / 4) % 3 - 1;
    ovalfill(129 + fl, 88, 2, 4, CLR_ORANGE);
    pset(129 + fl, 86, CLR_LIGHT_YELLOW);
    circfill(129, 90, 8, CLR_DARK_ORANGE);                          // glow (drawn behind via order? simple)
    // two cocktail glasses
    trifill(110, 100, 118, 100, 114, 106, CLR_LIME_GREEN);
    line(114, 106, 114, 100, CLR_LIGHT_GREY);
    // Roxy in the booth
    draw_roxy(199, 68);
    // slap flash
    if (slap_t > 0 && blink(3)) print("SLAP!", 150, 50, CLR_RED);
}

static void draw_dialogue(void) {
    int top = 30, lh = 16;
    fillp(FILL_CHECKER, CLR_BLACK);
    rectfill(6, top - 8, 244, L_N * lh + 22, CLR_DARKER_BLUE);
    fillp_reset();
    rect(6, top - 8, 244, L_N * lh + 22, CLR_PINK);
    int mx = mouse_x(), my = mouse_y();
    for (int l = 0; l < L_N; l++) {
        int y = top + l * lh;
        bool hov = (mx < 250 && my >= y && my < y + lh);
        bool used = lines_tried & (1 << l);
        int col = hov ? CLR_LIGHT_YELLOW : (used ? CLR_DARK_GREY : CLR_LIGHT_PEACH);
        if (hov) { print(">", 10, y, CLR_PINK); }
        print(LINE_TXT[l], 18, y, col);
    }
    int ly = top + L_N * lh;
    bool lh_hov = (mx < 250 && my >= ly && my < ly + 14);
    print("(say nothing and leave)", 18, ly + 2, lh_hov ? CLR_WHITE : CLR_INDIGO);
}

void draw(void) {
    if (!ready) reset_game();

    if (scene == SC_BAR) scene_bar(); else scene_booth();

    draw_larry();

    // hover label (verb + hotspot) when not on the bar / not talking
    int hov = hover_hotspot();
    if (!won && !talking && hov >= 0 && msg_t <= 0) {
        int n; Hotspot *hs = scene_hs(&n);
        const char *act = held ? (held == IT_ROSE ? "Give rose to" :
                                  held == IT_COLOGNE ? "Use cologne on" : "Use coin on")
                                : VERB_NAME[verb];
        draw_label(str("%s %s", act, hs[hov].name), CLR_LIGHT_YELLOW);
    }

    // dialogue menu
    if (talking && !won) draw_dialogue();

    // ── verb bar + inventory ──
    fillp(FILL_CHECKER, CLR_BLACK); rectfill(0, BAR_Y, SCREEN_W, SCREEN_H - BAR_Y, CLR_DARKER_PURPLE); fillp_reset();
    line(0, BAR_Y, SCREEN_W, BAR_Y, CLR_PINK);
    for (int v = 0; v < NVERB; v++) {
        int bx = (v % 2) * 43, by = BAR_Y + (v / 2) * 12;
        bool sel = (v == verb && !held);
        rectfill(bx + 1, by + 1, 41, 11, sel ? CLR_DARK_RED : CLR_BLACK);
        print(VERB_NAME[v], bx + 4, by + 3, sel ? CLR_WHITE : CLR_LIGHT_GREY);
    }
    line(88, BAR_Y, 88, SCREEN_H, CLR_DARK_PURPLE);
    // inventory slots
    int present[3], pn = 0;
    if (inv & IT_ROSE)    present[pn++] = IT_ROSE;
    if (inv & IT_COLOGNE) present[pn++] = IT_COLOGNE;
    if (inv & IT_COIN)    present[pn++] = IT_COIN;
    for (int s = 0; s < 4; s++) {
        int sx = 92 + s * 22, sy = BAR_Y + 4;
        rect(sx, sy, 20, 20, CLR_DARK_PURPLE);
        if (s < pn) {
            int it = present[s];
            if (it == held) rectfill(sx + 1, sy + 1, 18, 18, CLR_DARK_RED);
            if (it == IT_ROSE) { line(sx + 10, sy + 16, sx + 10, sy + 7, CLR_DARK_GREEN); circfill(sx + 10, sy + 6, 3, CLR_RED); }
            else if (it == IT_COLOGNE) { rectfill(sx + 6, sy + 6, 8, 11, CLR_LIGHT_GREY); rectfill(sx + 8, sy + 3, 4, 4, CLR_TRUE_BLUE); }
            else if (it == IT_COIN) { circfill(sx + 10, sy + 10, 5, CLR_YELLOW); circ(sx + 10, sy + 10, 5, CLR_DARK_ORANGE); }
        }
    }
    // message line
    if (msg_t > 0) print(msg, 92, BAR_Y + 27, CLR_LIGHT_YELLOW);
    else print(str("LEISURE SUIT LARRY  -  %s", scene == SC_BAR ? "the bar" : "the booth"),
               92, BAR_Y + 27, CLR_MAUVE);

    // ── win card ──
    if (won) {
        fade(clamp(win_t, 0, 0.5f));
        int w = 252, h = 60, bx = (SCREEN_W - w) / 2, by = 44;
        rectfill(bx, by, w, h, CLR_DARK_PURPLE);
        rect(bx, by, w, h, CLR_PINK); rect(bx + 2, by + 2, w - 4, h - 4, CLR_RED);
        // a popping heart
        float p = ease_out(clamp(heart_t * 1.6f, 0, 1));
        int hr = 4 + (int)(p * 6);
        int hcx = SCREEN_W / 2, hcy = by + 18;
        circfill(hcx - hr/2, hcy - hr/3, hr/2, CLR_RED);
        circfill(hcx + hr/2, hcy - hr/3, hr/2, CLR_RED);
        trifill(hcx - hr, hcy, hcx + hr, hcy, hcx, hcy + hr, CLR_RED);
        print_centered("- IT'S A DATE -", SCREEN_W/2, by + 30, CLR_LIGHT_YELLOW);
        print_centered("Larry's still got it. (Had it?)", SCREEN_W/2, by + 42, CLR_LIGHT_PEACH);
        if (blink(18)) print_centered("click to play again", SCREEN_W/2, by + 52, CLR_LIGHT_GREY);
    }
}
