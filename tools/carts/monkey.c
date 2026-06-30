/* de:meta
{
  "title": "Monkey Island",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "dialogue-tree",
    "inventory-system",
    "state-machine"
  ],
  "lineage": "Monkey Island 1 (LucasArts 1990) — SCUMM-style verb/hotspot adventure with the insult sword-fight as a playable duel mechanic built on a randomised retort table.",
  "genre": "adventure",
  "homage": "The Secret of Monkey Island (1990)",
  "description": "A bite-size SCUMM point-and-click on the docks of Mêlée Island, in the spirit of The Secret of Monkey Island. Pick a verb (Look / Talk / Use / Take), click a hotspot, walk over and act: grab the rusty cutlass off a crate, read the notice board, eye the moored ship. Talk to the Sword Master through a branching dialogue tree until she challenges you to the signature insult sword-fight — a turn-based war of WORDS where she hurls an insult and you must click the one comeback that actually answers it (then press your own attack while she sometimes flubs hers). Land three exchanges to win and earn the title of pirate; clang a wrong line and the screen shakes red. Controls: mouse picks a verb then a hotspot; click the floor to walk, click an inventory item to hold then a hotspot to use it; in conversation and in the duel click a line or press its number (1-4); SPACE/ESC clears a message, click to duel again on the end card.",
  "todo": [
    "Text overlaps and runs off-screen — use a smaller font.",
    "Sort actors by y so the character can walk in front of/behind props.",
    "Add a simple iMUSE-like musical layer that responds to what you're doing (we have the tools).",
    "Win state should fade the screen to half.",
    "ui-audit: the room-description line (\"Mêlée Island docks…\") runs off the right edge."
  ]
}
de:meta */
#include "studio.h"
#include <stddef.h>

// ── MONKEY ISLAND — a SCUMM dock + a branching chat + INSULT SWORD-FIGHTING ──
//
// You're a wannabe pirate on the dock of Mêlée Island. Pick a verb
// (LOOK / TALK / USE / TAKE), click a thing, walk over, act. Take the
// rusty CUTLASS, then TALK to the Sword Master. Talk your way to a
// challenge and the duel begins: she hurls an INSULT, you pick the one
// right COMEBACK. Parry enough times and you win.
//
//   TAKE the cutlass  →  TALK to the Sword Master  →  pick "I want to duel"
//   →  in the duel, click the retort that actually answers her insult.
//
// The duel is the signature: every clash is a war of WORDS, not steel.

// ── layout ───────────────────────────────────────────────────────────
#define BAR_Y     150
#define FLOOR_TOP 104
#define WALK_MINX 14
#define WALK_MAXX 306
#define ACT_DIST  52

// game states
enum { ST_DOCK, ST_TALK, ST_DUEL, ST_OVER };
static int state = ST_DOCK;

// verbs
enum { V_LOOK, V_TALK, V_USE, V_TAKE, NVERB };
static const char *VERB_NAME[NVERB] = { "Look", "Talk", "Use", "Take" };

// inventory (bitset)
enum { IT_CUTLASS = 1, IT_GROG = 2 };

// ── hotspots ──────────────────────────────────────────────────────────
typedef struct { int x, y, w, h; int standx; const char *name; } Hotspot;
enum { H_MASTER, H_CUTLASS, H_GROG, H_SIGN, H_SHIP, H_SEA, H_N };
static Hotspot dock_hs[H_N] = {
    [H_MASTER]  = { 224, 70, 36, 56, 206, "sword master" },
    [H_CUTLASS] = { 60, 120, 22, 8, 64, "rusty cutlass" },
    [H_GROG]    = { 112, 118, 12, 12, 110, "mug of grog" },
    [H_SIGN]    = { 12, 70, 30, 28, 44, "notice board" },
    [H_SHIP]    = { 150, 28, 70, 44, 150, "moored ship" },
    [H_SEA]     = { 0, 96, SCREEN_W, 8, 160, "the sea" },
};

// ── insult / retort table (the Monkey Island core) ─────────────────────
typedef struct { const char *insult; const char *retort; } Pair;
static Pair PAIRS[] = {
    { "You fight like a dairy farmer!",            "How appropriate. You fight like a cow." },
    { "This is the END for you, you gutter rat!",  "And I've got a little TIP for you. Get the point?" },
    { "Soon you'll be wearing my sword like a shish kebab!", "First you'd better stop waving it like a feather-duster." },
    { "My handkerchief will wipe up your blood!",  "So you got that one as a wedding present?" },
    { "People fall at my feet when they see me!",  "Even BEFORE they smell your breath?" },
    { "I once owned a dog smarter than you.",      "He must have taught you everything you know." },
    { "Nobody's ever drawn blood from me, ever!",  "You run THAT fast?" },
    { "I'm not going to take your insolence!",     "Then you shouldn't have given it to me." },
};
#define NPAIR (int)(sizeof(PAIRS)/sizeof(PAIRS[0]))
#define HAND  3              // retorts shown per turn (1 correct + decoys)
#define WIN_HITS 3           // exchanges needed to win the duel

// ── state ─────────────────────────────────────────────────────────────
static bool  ready = false;
static int   verb = V_LOOK;
static float px, py;
static int   face = 1;
static int   tx = -1, ty;
static int   pending = -1;
static int   inv = 0;
static int   held = 0;
static bool  have_cutlass_seen = false;
static char  msg[80];
static float msg_t = 0;

// dialogue
static int   dnode = 0;
static bool  duel_unlocked = false;

// duel
static int   you_score = 0, foe_score = 0;
static int   duel_turn = 0;          // 0 = foe insults (you retort), 1 = you insult (foe answers)
static int   cur_insult = 0;         // active PAIRS index
static int   hand[HAND];             // shown line indices (PAIRS indices)
static int   correct_slot = 0;       // which hand slot is the right answer
static int   chosen = -1;            // slot you clicked this turn
static float turn_t = 0;             // animates the clash, then advances
static int   last_result = 0;        // +1 you parried, -1 you took it
static bool  player_won = false;
static int   foe_lunge = 0, you_lunge = 0;  // pose offsets

// ── audio ───────────────────────────────────────────────────────────────
static void setup_audio(void) {
    instrument(5, INSTR_SQUARE, 3, 30, 3, 90);     // ui click
    instrument_duty(5, 0.3f);
    instrument(6, INSTR_TRI, 2, 60, 4, 220);       // parry sting
    instrument(7, INSTR_NOISE, 1, 40, 1, 60);      // clang / miss
}

static void say(const char *s) {
    int i = 0; while (s[i] && i < 79) { msg[i] = s[i]; i++; } msg[i] = 0;
    msg_t = 3.6f;
}

static void click_snd(void) { note(72, 5, 2); }

// ── duel helpers ─────────────────────────────────────────────────────────
// deal a hand: correct line for cur_insult plus distinct decoys.
static void deal_hand(void) {
    correct_slot = rnd(HAND);
    for (int i = 0; i < HAND; i++) {
        if (i == correct_slot) { hand[i] = cur_insult; continue; }
        int pick;
        bool dup;
        do {
            pick = rnd(NPAIR);
            dup = (pick == cur_insult);
            for (int j = 0; j < i && !dup; j++) if (hand[j] == pick) dup = true;
        } while (dup);
        hand[i] = pick;
    }
}

static void start_turn(void) {
    chosen = -1; turn_t = 0; last_result = 0; foe_lunge = 0; you_lunge = 0;
    cur_insult = rnd(NPAIR);
    deal_hand();
}

static void begin_duel(void) {
    state = ST_DUEL;
    you_score = foe_score = 0;
    duel_turn = 0;
    start_turn();
    say("");
    strum(60, CHORD_MIN7, INSTR_TRI, 3, 50);
}

// resolve a clicked slot
static void choose(int slot) {
    if (chosen >= 0) return;
    chosen = slot;
    turn_t = 0;
    if (duel_turn == 0) {
        // foe insulted; you pick a retort
        you_lunge = 1;
        if (slot == correct_slot) {
            last_result = +1; you_score++;
            note(67, 6, 4); strum(72, CHORD_MAJ7, INSTR_TRI, 4, 45);
        } else {
            last_result = -1; foe_score++;
            note(40, 7, 5); note(36, INSTR_SQUARE, 3); shake(4);
        }
    } else {
        // you insulted; you pick whether the foe answers right (clicking IS your jab)
        // the foe answers on its own — your click just commits the jab.
        foe_lunge = 1;
        bool foe_right = chance(45);   // foe sometimes flubs → you can win
        if (!foe_right) {
            last_result = +1; you_score++;
            note(67, 6, 4); strum(72, CHORD_MAJ7, INSTR_TRI, 4, 45);
        } else {
            last_result = -1; foe_score++;
            note(40, 7, 5); note(36, INSTR_SQUARE, 3); shake(4);
        }
    }
}

// ── reset ─────────────────────────────────────────────────────────────
static void reset_game(void) {
    setup_audio();
    inv = 0; held = 0; verb = V_LOOK;
    have_cutlass_seen = false; duel_unlocked = false; dnode = 0;
    msg[0] = 0; msg_t = 0;
    state = ST_DOCK;
    tx = -1; pending = -1;
    px = 120; py = 132; face = 1;
    ready = true;
    say("Mêlée Island docks. A Sword Master waits up the pier.");
}

// ── scene helpers ─────────────────────────────────────────────────────
static bool hs_active(int id) {
    if (id == H_CUTLASS) return !(inv & IT_CUTLASS);
    if (id == H_GROG)    return !(inv & IT_GROG);
    return true;
}

static int hover_hotspot(void) {
    int mx = mouse_x(), my = mouse_y();
    if (my >= BAR_Y) return -1;
    int best = -1;
    for (int i = 0; i < H_N; i++) {
        if (!hs_active(i)) continue;
        if (point_in_box(mx, my, dock_hs[i].x, dock_hs[i].y, dock_hs[i].w, dock_hs[i].h)) best = i;
    }
    return best;
}

static bool near_hs(int id) { return abs((int)px - dock_hs[id].standx) < ACT_DIST; }

// ── dialogue tree with the Sword Master ────────────────────────────────
// nodes show up to 4 choices; a choice prints a reply then jumps.
typedef struct { const char *text; int next; const char *reply; } Choice;
typedef struct { const char *prompt; Choice ch[4]; int nch; } Node;

static Node DLG[] = {
    // 0 — opening
    { "\"You're no pirate. What do you want?\"", {
        { "Teach me to sword-fight.",  1, "\"Steel is for amateurs. The TONGUE is the true blade.\"" },
        { "Nice... weather we're having.", 2, "\"Don't waste my time with small talk, swab.\"" },
        { "(Leave.)",                   -1, "\"Run along, then.\"" },
    }, 3 },
    // 1 — she explains insult duelling
    { "\"Win a duel of WITS and you may call yourself a pirate.\"", {
        { "How does a duel of wits work?", 3, "\"I insult you. You answer with the perfect comeback. Falter and you lose.\"" },
        { "I want to DUEL you. Now.",       4, "\"Bold. Or stupid. Draw your blade, swab.\"" },
        { "(Step back.)",                  -1, "\"Coward.\"" },
    }, 3 },
    // 2 — small talk dead end
    { "\"...Are you still here?\"", {
        { "About that sword training?", 1, "\"Ah. Now you're talking.\"" },
        { "(Leave.)",                  -1, "\"Finally.\"" },
    }, 2 },
    // 3 — the explanation, loops to challenge
    { "\"It's all in the wordplay. Are you READY, or not?\"", {
        { "I'm ready. Let's duel.", 4, "\"Then defend yourself!\"" },
        { "I need a moment.",       -1, "\"Don't take too long.\"" },
    }, 2 },
    // 4 — challenge accepted (needs a cutlass)
    { "(challenge)", { { "", -1, "" } }, 0 },
};

// ── update ─────────────────────────────────────────────────────────────
static void update_dock(void) {
    int mx = mouse_x(), my = mouse_y();

    if (mouse_pressed(MOUSE_LEFT)) {
        // verb bar (2x2, left block)
        if (my >= BAR_Y && my < BAR_Y + 24 && mx < 86) {
            int v = my < BAR_Y + 12 ? (mx < 43 ? V_LOOK : V_TALK)
                                    : (mx < 43 ? V_USE  : V_TAKE);
            verb = v; held = 0; click_snd();
        }
        // inventory strip
        else if (my >= BAR_Y) {
            int slot = (mx - 92) / 22;
            int present[2], pn = 0;
            if (inv & IT_CUTLASS) present[pn++] = IT_CUTLASS;
            if (inv & IT_GROG)    present[pn++] = IT_GROG;
            if (mx >= 92 && slot >= 0 && slot < pn) {
                int it = present[slot];
                if (verb == V_LOOK) {
                    if (it == IT_CUTLASS) say("A rusty cutlass. More for show than blood.");
                    else say("A mug of warm grog. Tempting.");
                } else { held = (held == it) ? 0 : it; click_snd(); }
            }
        }
        // scene click
        else {
            int h = hover_hotspot();
            if (h >= 0) {
                pending = h;
                tx = (int)clamp(dock_hs[h].standx, WALK_MINX, WALK_MAXX);
                ty = (int)clamp(py, 116, 144);
            } else if (my >= FLOOR_TOP) {
                tx = (int)clamp(mx, WALK_MINX, WALK_MAXX);
                ty = (int)clamp(my, 116, 144);
                pending = -1;
            }
        }
    }

    // walk
    if (tx >= 0) {
        float sp = 84.0f * dt();
        if (abs((int)px - tx) > 2) { float d = sgn(tx - (int)px); px += d * sp; face = d < 0 ? -1 : 1; }
        if (abs((int)py - ty) > 2) py += sgn(ty - (int)py) * sp;
        px = clamp(px, WALK_MINX, WALK_MAXX);
        if (abs((int)px - tx) <= 2 && abs((int)py - ty) <= 2) {
            tx = -1;
            if (pending >= 0) {
                int id = pending; pending = -1;
                if (near_hs(id)) {
                    // held-item use
                    if (held) {
                        if (held == IT_GROG && id == H_MASTER) { say("\"Keep your grog, swab. I drink WORDS.\""); }
                        else say("That accomplishes nothing.");
                        held = 0;
                    } else switch (verb) {
                    case V_LOOK:
                        if (id == H_MASTER) say("The Sword Master. Famous for her razor tongue.");
                        else if (id == H_CUTLASS) say("A cutlass, rusting on a crate.");
                        else if (id == H_GROG) say("Grog. It could dissolve the mug.");
                        else if (id == H_SIGN) say("'DUEL THE SWORD MASTER. WIT REQUIRED.'");
                        else if (id == H_SHIP) say("A fine ship. One day she'll be mine.");
                        else if (id == H_SEA) say("Grey water slaps the pilings.");
                        break;
                    case V_TAKE:
                        if (id == H_CUTLASS) { inv |= IT_CUTLASS; note(76, 5, 3); say("Got the cutlass. Now I look the part."); }
                        else if (id == H_GROG) { inv |= IT_GROG; note(74, 5, 3); say("Took the grog. Don't mind if I do."); }
                        else say("I can't pick that up.");
                        break;
                    case V_USE:
                        if (id == H_MASTER) { dnode = 0; state = ST_TALK; click_snd(); }
                        else if (id == H_SEA) say("A swim? In THIS weather?");
                        else say("Nothing happens.");
                        break;
                    case V_TALK:
                        if (id == H_MASTER) { dnode = 0; state = ST_TALK; click_snd(); }
                        else if (id == H_SIGN) say("The sign says nothing back.");
                        else say("No reply.");
                        break;
                    }
                } else {
                    if (verb == V_TALK && id == H_MASTER) say("I'm too far to talk. Get closer.");
                    else say("I can't reach that from here.");
                }
            }
        }
    }

    if (keyp(KEY_SPACE) || keyp(KEY_ESCAPE)) msg_t = 0;
}

static void pick_choice(int i) {
    Node *nd = &DLG[dnode];
    if (i < 0 || i >= nd->nch) return;
    Choice *c = &nd->ch[i];
    click_snd();
    say(c->reply);
    if (c->next == 4) {
        // challenge — need a cutlass first
        if (!(inv & IT_CUTLASS)) {
            state = ST_DOCK;
            say("\"Come back with a BLADE, swab. Even for words.\"");
            return;
        }
        duel_unlocked = true;
        begin_duel();
        return;
    }
    if (c->next < 0) { state = ST_DOCK; return; }
    dnode = c->next;
}

static void update_talk(void) {
    Node *nd = &DLG[dnode];
    if (keyp(KEY_ESCAPE)) { state = ST_DOCK; return; }
    for (int i = 0; i < nd->nch; i++) if (keyp('1' + i)) { pick_choice(i); return; }
    if (mouse_pressed(MOUSE_LEFT)) {
        int my = mouse_y();
        int top = SCREEN_H - 8 - nd->nch * 13;
        for (int i = 0; i < nd->nch; i++) {
            int ly = top + i * 13;
            if (my >= ly && my < ly + 13) { pick_choice(i); return; }
        }
    }
}

static void update_duel(void) {
    if (chosen < 0) {
        // awaiting a pick
        for (int i = 0; i < HAND; i++) if (keyp('1' + i)) { choose(i); return; }
        if (mouse_pressed(MOUSE_LEFT)) {
            int my = mouse_y();
            int top = 118;
            for (int i = 0; i < HAND; i++) {
                int ly = top + i * 14;
                if (my >= ly && my < ly + 14) { choose(i); return; }
            }
        }
        return;
    }
    // clash is resolving
    turn_t += dt();
    foe_lunge = (int)(sin_deg(clamp(turn_t,0,0.5f)/0.5f*180) * 10);
    you_lunge = (int)(sin_deg(clamp(turn_t,0,0.5f)/0.5f*180) * 10);
    if (turn_t > 1.4f) {
        if (you_score >= WIN_HITS || foe_score >= WIN_HITS) {
            player_won = you_score >= WIN_HITS;
            state = ST_OVER;
            if (player_won) strum(72, CHORD_MAJ7, INSTR_TRI, 5, 70);
            else { note(36, INSTR_SQUARE, 4); shake(6); }
            return;
        }
        duel_turn ^= 1;     // alternate who insults
        start_turn();
    }
}

void update(void) {
    if (!ready) reset_game();
    if (msg_t > 0) msg_t -= dt();

    switch (state) {
    case ST_DOCK: update_dock(); break;
    case ST_TALK: update_talk(); break;
    case ST_DUEL: update_duel(); break;
    case ST_OVER:
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_SPACE)) {
            // restart the duel (you keep your blade)
            begin_duel();
        }
        break;
    }
}

// ── drawing: actors ─────────────────────────────────────────────────────
static void draw_pirate(int x, int y, int f, int lunge, bool sword) {
    x += f * lunge;
    ovalfill(x, y, 8, 3, CLR_BROWNISH_BLACK);                 // shadow
    rectfill(x - 4, y - 9, 3, 9, CLR_DARK_BLUE);              // legs
    rectfill(x + 1, y - 9, 3, 9, CLR_DARK_BLUE);
    rectfill(x - 5, y - 21, 11, 13, CLR_DARK_RED);            // coat
    rectfill(x - 5, y - 21, 11, 4, CLR_WHITE);               // collar/shirt
    rectfill(x - 5, y - 12, 11, 2, CLR_YELLOW);              // belt
    circfill(x, y - 26, 4, CLR_PEACH);                       // head
    pset(x + f * 2, y - 26, CLR_BLACK);                      // eye
    rectfill(x - 5, y - 31, 11, 3, CLR_DARK_BROWN);          // tricorne
    trifill(x - 6, y - 29, x + 6, y - 29, x, y - 33, CLR_DARK_BROWN);
    rectfill(x + f * 5, y - 19, 3, 7, CLR_LIGHT_PEACH);      // sword arm
    if (sword) line(x + f * 7, y - 16, x + f * 18, y - 24, CLR_LIGHT_GREY); // cutlass
}

static void draw_master(int x, int y, int f, int lunge, bool sword) {
    x += f * lunge;
    ovalfill(x, y, 8, 3, CLR_BROWNISH_BLACK);
    rectfill(x - 6, y - 22, 12, 22, CLR_DARK_PURPLE);        // long coat/dress
    rectfill(x - 6, y - 22, 12, 4, CLR_MAUVE);
    rectfill(x - 6, y - 13, 12, 2, CLR_YELLOW);
    circfill(x, y - 27, 4, CLR_LIGHT_PEACH);                 // head
    pset(x + f * 2, y - 27, CLR_BLACK);
    rectfill(x - 5, y - 33, 11, 6, CLR_DARK_BROWN);          // big hat
    trifill(x - 8, y - 30, x + 8, y - 30, x, y - 36, CLR_DARK_BROWN);
    rectfill(x + f * 5, y - 20, 3, 7, CLR_LIGHT_PEACH);
    if (sword) line(x + f * 7, y - 17, x + f * 18, y - 26, CLR_WHITE);
}

// ── drawing: dock scene ──────────────────────────────────────────────────
static void scene_dock(void) {
    // sky + sea
    cls(CLR_DARKER_BLUE);
    rectfill(0, 60, SCREEN_W, 44, CLR_BLUE_GREEN);            // water
    for (int i = 0; i < 18; i++) {
        int wx = (i * 23 + frame()) % SCREEN_W;
        pset(wx, 70 + (i * 9 % 30), CLR_BLUE);
    }
    // moored ship
    rectfill(150, 40, 70, 26, CLR_DARK_BROWN);               // hull
    trifill(150, 40, 150, 66, 134, 56, CLR_DARK_BROWN);      // bow
    line(176, 40, 176, 8, CLR_MEDIUM_GREY);                  // mast
    trifill(176, 12, 176, 36, 204, 26, CLR_LIGHT_PEACH);     // sail
    if (blink(30)) pset(176, 8, CLR_LIGHT_YELLOW);
    // dock floor (planks)
    rectfill(0, FLOOR_TOP, SCREEN_W, BAR_Y - FLOOR_TOP, CLR_BROWN);
    for (int x = 0; x < SCREEN_W; x += 18) line(x, FLOOR_TOP, x, BAR_Y, CLR_DARK_BROWN);
    line(0, FLOOR_TOP, SCREEN_W, FLOOR_TOP, CLR_DARK_BROWN);
    // notice board (left)
    line(20, 98, 20, 78, CLR_DARK_BROWN);
    line(34, 98, 34, 78, CLR_DARK_BROWN);
    rectfill(14, 64, 28, 22, CLR_LIGHT_PEACH);
    rect(14, 64, 28, 22, CLR_DARK_BROWN);
    line(17, 70, 39, 70, CLR_DARK_GREY); line(17, 74, 36, 74, CLR_DARK_GREY);
    line(17, 78, 38, 78, CLR_DARK_GREY);
    // crate with cutlass
    rectfill(50, 112, 28, 22, CLR_DARK_BROWN); rect(50, 112, 28, 22, CLR_BROWN);
    line(50, 123, 78, 123, CLR_BROWN); line(64, 112, 64, 134, CLR_BROWN);
    if (!(inv & IT_CUTLASS)) {
        line(58, 118, 80, 114, CLR_LIGHT_GREY);              // blade
        line(56, 119, 60, 117, CLR_YELLOW);                  // hilt
    }
    // grog mug on a barrel
    rectfill(104, 120, 16, 14, CLR_DARK_ORANGE);
    if (!(inv & IT_GROG)) {
        rectfill(112, 116, 8, 8, CLR_LIGHT_GREY);
        rectfill(112, 116, 8, 3, CLR_LIME_GREEN);            // foam
    }
    // the sword master, up the pier
    draw_master(224, 122, -1, 0, false);
    // you
    bool walking = tx >= 0;
    int bob = walking ? anim(2, 7, 0) : 0;
    draw_pirate((int)px, (int)py - bob, face, 0, false);
}

// ── drawing: verb bar + inventory + message ──────────────────────────────
static void draw_label(const char *s, int col) {
    int w = text_width(s) + 8, x = mid(2, mouse_x() - w / 2, SCREEN_W - w - 2);
    int y = mouse_y() - 12; if (y < 2) y = mouse_y() + 8;
    fillp(FILL_CHECKER, -1); rectfill(x, y, w, 11, CLR_BLACK); fillp_reset();
    print(s, x + 4, y + 2, col);
}

static void draw_bar(void) {
    fillp(FILL_CHECKER, CLR_BLACK); rectfill(0, BAR_Y, SCREEN_W, SCREEN_H - BAR_Y, CLR_DARKER_GREY); fillp_reset();
    line(0, BAR_Y, SCREEN_W, BAR_Y, CLR_LIGHT_GREY);
    for (int v = 0; v < NVERB; v++) {
        int bx = (v % 2) * 43, by = BAR_Y + (v / 2) * 12;
        bool sel = (v == verb && !held);
        rectfill(bx + 1, by + 1, 41, 11, sel ? CLR_DARK_GREEN : CLR_BLACK);
        print(VERB_NAME[v], bx + 4, by + 3, sel ? CLR_WHITE : CLR_LIGHT_GREY);
    }
    line(88, BAR_Y, 88, SCREEN_H, CLR_DARK_GREY);
    int present[2], pn = 0;
    if (inv & IT_CUTLASS) present[pn++] = IT_CUTLASS;
    if (inv & IT_GROG)    present[pn++] = IT_GROG;
    for (int s = 0; s < 3; s++) {
        int sx = 92 + s * 22, sy = BAR_Y + 4;
        rect(sx, sy, 20, 20, CLR_DARK_GREY);
        if (s < pn) {
            int it = present[s];
            if (it == held) rectfill(sx + 1, sy + 1, 18, 18, CLR_DARK_GREEN);
            if (it == IT_CUTLASS) { line(sx + 4, sy + 15, sx + 16, sy + 5, CLR_LIGHT_GREY); pset(sx + 5, sy + 14, CLR_YELLOW); }
            else { rectfill(sx + 6, sy + 6, 8, 9, CLR_DARK_ORANGE); rectfill(sx + 6, sy + 6, 8, 2, CLR_LIME_GREEN); }
        }
    }
    if (msg_t > 0) print(msg, 162, BAR_Y + 4, CLR_LIGHT_YELLOW);
    else print("MONKEY ISLAND - the docks", 162, BAR_Y + 4, CLR_DARK_GREY);
    print(str("verb: %s%s", VERB_NAME[verb], held ? "  (holding item)" : ""), 162, BAR_Y + 14, CLR_INDIGO);
}

// ── drawing: dialogue overlay ────────────────────────────────────────────
static void draw_talk(void) {
    scene_dock();
    Node *nd = &DLG[dnode];
    int boxh = 28 + nd->nch * 13;
    int by = SCREEN_H - boxh - 2;
    rectfill(2, by, SCREEN_W - 4, boxh, CLR_DARKER_PURPLE);
    rect(2, by, SCREEN_W - 4, boxh, CLR_LIGHT_YELLOW);
    print(nd->prompt, 8, by + 5, CLR_LIGHT_PEACH);
    int mx = mouse_x(), my = mouse_y();
    int top = SCREEN_H - 8 - nd->nch * 13;
    for (int i = 0; i < nd->nch; i++) {
        int ly = top + i * 13;
        bool hot = my >= ly && my < ly + 13 && mx > 2 && mx < SCREEN_W - 2;
        print(str("%d.", i + 1), 8, ly, hot ? CLR_WHITE : CLR_YELLOW);
        print(nd->ch[i].text, 24, ly, hot ? CLR_WHITE : CLR_LIGHT_GREY);
    }
}

// ── drawing: pip row ─────────────────────────────────────────────────────
static void draw_pips(int x, int y, int n, int col) {
    for (int i = 0; i < WIN_HITS; i++)
        circfill(x + i * 9, y, 3, i < n ? col : CLR_DARKER_GREY);
}

// ── drawing: the duel ────────────────────────────────────────────────────
static void draw_duel(void) {
    cls(CLR_DARKER_BLUE);
    // backdrop: a dim dock at night
    rectfill(0, 70, SCREEN_W, 60, CLR_BLUE_GREEN);
    rectfill(0, 130, SCREEN_W, 70, CLR_BROWNISH_BLACK);
    line(0, 130, SCREEN_W, 130, CLR_BROWN);
    if (blink(24)) circfill(SCREEN_W - 34, 26, 7, CLR_LIGHT_YELLOW);   // moon
    else circfill(SCREEN_W - 34, 26, 7, CLR_LIGHT_PEACH);

    // fighters
    bool resolving = chosen >= 0;
    draw_pirate(74, 128, 1, you_lunge, true);
    draw_master(246, 128, -1, foe_lunge, true);

    // scoreboard
    print("YOU", 60, 8, CLR_LIGHT_YELLOW); draw_pips(60, 18, you_score, CLR_GREEN);
    print_right("MASTER", SCREEN_W - 56, 8, CLR_LIGHT_YELLOW); draw_pips(SCREEN_W - 56, 18, foe_score, CLR_RED);
    print_centered("INSULT SWORD-FIGHTING", SCREEN_W/2, 30, CLR_INDIGO);

    // who's speaking
    bool foe_speaks = (duel_turn == 0);
    const char *line_txt = PAIRS[cur_insult].insult;
    // the spoken insult bubble
    int bw = text_width(line_txt) + 10;
    if (bw > SCREEN_W - 8) bw = SCREEN_W - 8;
    int bx = mid(2, (foe_speaks ? 246 : 74) - bw / 2, SCREEN_W - bw - 2);
    rectfill(bx, 44, bw, 13, foe_speaks ? CLR_DARK_RED : CLR_DARK_GREEN);
    rect(bx, 44, bw, 13, CLR_LIGHT_YELLOW);
    print(line_txt, bx + 5, 47, CLR_WHITE);

    if (!resolving) {
        if (foe_speaks) {
            print_centered("Pick the comeback that ANSWERS her:", SCREEN_W/2, 64, CLR_LIGHT_PEACH);
            int top = 118;
            int mx = mouse_x(), my = mouse_y();
            for (int i = 0; i < HAND; i++) {
                int ly = top + i * 14;
                bool hot = my >= ly && my < ly + 14;
                rectfill(8, ly, SCREEN_W - 16, 13, hot ? CLR_DARK_BLUE : CLR_DARKER_PURPLE);
                if (hot) rect(8, ly, SCREEN_W - 16, 13, CLR_LIGHT_YELLOW);
                print(str("%d.", i + 1), 12, ly + 3, CLR_YELLOW);
                print(PAIRS[hand[i]].retort, 28, ly + 3, hot ? CLR_WHITE : CLR_LIGHT_GREY);
            }
        } else {
            print_centered("Press the attack! Hurl your insult:", SCREEN_W/2, 64, CLR_LIGHT_PEACH);
            int top = 118;
            int mx = mouse_x(), my = mouse_y();
            for (int i = 0; i < HAND; i++) {
                int ly = top + i * 14;
                bool hot = my >= ly && my < ly + 14;
                rectfill(8, ly, SCREEN_W - 16, 13, hot ? CLR_DARK_BLUE : CLR_DARKER_PURPLE);
                if (hot) rect(8, ly, SCREEN_W - 16, 13, CLR_LIGHT_YELLOW);
                print(str("%d.", i + 1), 12, ly + 3, CLR_YELLOW);
                print(PAIRS[hand[i]].insult, 28, ly + 3, hot ? CLR_WHITE : CLR_LIGHT_GREY);
            }
        }
    } else {
        // show the chosen line + result
        const char *picked = foe_speaks ? PAIRS[hand[chosen]].retort : PAIRS[hand[chosen]].insult;
        rectfill(8, 118, SCREEN_W - 16, 13, CLR_DARK_BLUE);
        print(picked, 12, 121, CLR_WHITE);
        if (last_result > 0) {
            print_centered("PARRIED! You press the attack.", SCREEN_W/2, 140, CLR_GREEN);
            if (blink(4)) circ((int)74 + 14, 100, 6, CLR_LIGHT_YELLOW);
        } else {
            fade(clamp(0.4f - turn_t * 0.5f, 0, 0.4f));
            print_centered("You falter... a hit lands!", SCREEN_W/2, 140, CLR_RED);
        }
    }

    print_centered("click a line  -  or press its number", SCREEN_W/2, SCREEN_H - 9, CLR_DARK_GREY);
}

// ── drawing: end card ────────────────────────────────────────────────────
static void draw_over(void) {
    draw_duel();
    fade(0.5f);
    int w = 250, h = 60, bx = (SCREEN_W - w) / 2, by = 64;
    rectfill(bx, by, w, h, CLR_DARK_BLUE);
    rect(bx, by, w, h, CLR_LIGHT_YELLOW); rect(bx + 2, by + 2, w - 4, h - 4, CLR_INDIGO);
    if (player_won) {
        print_centered("YOU WIN THE DUEL!", SCREEN_W/2, by + 10, CLR_LIME_GREEN);
        print_centered("\"You fight like a dairy farmer...\"", SCREEN_W/2, by + 26, CLR_LIGHT_PEACH);
        print_centered("\"...but you won. You ARE a pirate.\"", SCREEN_W/2, by + 38, CLR_LIGHT_PEACH);
    } else {
        print_centered("YOU LOSE THE DUEL.", SCREEN_W/2, by + 10, CLR_RED);
        print_centered("\"Go home, swab. Practice your wit.\"", SCREEN_W/2, by + 28, CLR_LIGHT_PEACH);
    }
    if (blink(18)) print_centered("- click to duel again -", SCREEN_W/2, by + h - 12, CLR_LIGHT_GREY);
}

// ── top-level draw ───────────────────────────────────────────────────────
void draw(void) {
    if (!ready) reset_game();
    switch (state) {
    case ST_DOCK:
        scene_dock();
        {
            int hov = hover_hotspot();
            if (hov >= 0 && msg_t <= 0) {
                const char *act = held ? (held == IT_CUTLASS ? "Use cutlass on" : "Use grog on")
                                       : VERB_NAME[verb];
                draw_label(str("%s %s", act, dock_hs[hov].name), CLR_LIGHT_YELLOW);
            }
        }
        draw_bar();
        break;
    case ST_TALK: draw_talk(); break;
    case ST_DUEL: draw_duel(); break;
    case ST_OVER: draw_over(); break;
    }
}
