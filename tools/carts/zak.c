/* de:meta
{
  "slug": "zak",
  "title": "Zak McKracken",
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
  "lineage": "Homage to Zak McKracken (1988) and the SCUMM engine — a from-scratch verb-bar / hotspot / scene-transition adventure engine, the only SCUMM-style point-and-click in the cabinet.",
  "genre": "adventure",
  "homage": "Zak McKracken (1988)",
  "description": "A SCUMM-style point-and-click caper: you're tabloid reporter Zak, woken by a phone tip that your plane ticket is locked in your desk. Three hand-drawn rooms — a moody SF apartment with a ringing phone and a fishbowl, a hallway with a stuck vending machine and a fire-axe, and a night cab to the airport — wired into a real scene engine with a four-verb bar, a hover-labeled hotspot grammar, an inventory strip, and walk-to-floor navigation. Solve the one combination puzzle (take the paperclip, USE it to bend a lockpick, pick the drawer, grab the ticket) to unlock the cab and win. Mouse: click a verb (Look/Use/Take/Talk) then a hotspot; click the floor to walk; click an inventory item to hold it, then a hotspot to use it; SPACE/ESC clears a message.",
  "todo": [
    "Same y-layering need as the other adventure games (walk in front/behind).",
    "Some color choices make things hard to see."
  ]
}
de:meta */
#include "studio.h"
#include "endcard.h"
#include <stddef.h>

// ── ZAK McKRACKEN — a SCUMM-style point-and-click adventure ──────────
// You're Zak, tabloid reporter. A phone tip says your plane ticket is
// locked in the desk drawer. Pick a verb (LOOK / USE / TAKE / TALK),
// click a thing. Click the floor to walk. Click an inventory item to
// hold it, then click a hotspot to USE it on something.
//
//   TAKE the paperclip → USE it (bends into a lockpick)
//   → USE lockpick on the drawer → grab the TICKET → take the cab.
//
// Three rooms: the apartment, the hallway, the taxi.

// ── layout ───────────────────────────────────────────────────────────
#define BAR_Y     150         // bottom UI bar starts here
#define FLOOR_TOP 100         // walkable floor begins
#define WALK_MINX 14
#define WALK_MAXX 306
#define ACT_DIST  46          // how close Zak must be to act

// scenes
enum { SC_FLAT, SC_HALL, SC_TAXI, NSCENE };

// verbs
enum { V_LOOK, V_USE, V_TAKE, V_TALK, NVERB };
static const char *VERB_NAME[NVERB] = { "Look", "Use", "Take", "Talk" };

// inventory items (bitset)
enum { IT_CLIP = 1, IT_PICK = 2, IT_TICKET = 4 };

// ── hotspots ──────────────────────────────────────────────────────────
typedef struct { int x, y, w, h; int standx; const char *name; } Hotspot;

// per-scene hotspot ids — kept as enums so verb logic reads clearly
enum { F_PHONE, F_DESK, F_DRAWER, F_CLIP, F_TICKET, F_BOWL, F_DOOR, F_EXIT, F_N };
static Hotspot flat_hs[F_N] = {
    [F_PHONE]  = { 188, 78, 22, 16, 178, "phone" },
    [F_DESK]   = { 150, 92, 96, 22, 150, "desk" },
    [F_DRAWER] = { 156, 100, 30, 12, 150, "drawer" },
    [F_CLIP]   = { 210, 90,  8,  6, 200, "paperclip" },
    [F_TICKET] = { 162, 102, 16, 8, 150, "ticket" },
    [F_BOWL]   = { 52, 84, 24, 20, 64, "fishbowl" },
    [F_DOOR]   = { 12, 52, 30, 60, 50, "front door" },
    [F_EXIT]   = { 286, 60, 26, 80, 286, "to the hall >" },
};

enum { H_VEND, H_AXE, H_NOOK, H_BACK, H_ELEV, H_N };
static Hotspot hall_hs[H_N] = {
    [H_VEND] = { 40, 56, 40, 78, 90, "vending machine" },
    [H_AXE]  = { 132, 64, 26, 30, 130, "fire axe" },
    [H_NOOK] = { 200, 88, 40, 24, 210, "janitor's mop" },
    [H_BACK] = { 4, 60, 22, 80, 24, "< apartment" },
    [H_ELEV] = { 270, 48, 44, 92, 268, "elevator >" },
};

enum { T_DRIVER, T_METER, T_BACK, T_N };
static Hotspot taxi_hs[T_N] = {
    [T_DRIVER] = { 40, 56, 60, 50, 120, "cabbie" },
    [T_METER]  = { 150, 70, 30, 22, 160, "meter" },
    [T_BACK]   = { 4, 60, 22, 80, 24, "< hallway" },
};

// ── state ─────────────────────────────────────────────────────────────
static bool  ready = false;
static int   scene = SC_FLAT;
static int   verb = V_LOOK;
static float px, py;                 // Zak feet position
static int   face = 1;
static int   tx = -1, ty;            // walk target (-1 none)
static int   pending = -1;           // hotspot id queued until Zak arrives
static int   inv = 0;                // held items bitset
static int   held = 0;               // currently selected inv item (0 none)
static bool  drawer_open = false;
static char  msg[64];
static float msg_t = 0;              // seconds remaining on the message
static bool  won = false;
static float win_t = 0;
static bool  phone_done = false;

// instruments
static void setup_audio(void) {
    instrument(5, INSTR_SQUARE, 4, 40, 4, 120);     // phone bell
    instrument_lfo(5, 0, LFO_PITCH, 7.0f, 0.4f);
    instrument(6, INSTR_SINE, 2, 60, 3, 200);       // soft pickup
}

static void say(const char *s) {
    int i = 0; while (s[i] && i < 63) { msg[i] = s[i]; i++; } msg[i] = 0;
    msg_t = 3.2f;
}

static void respawn(int s) {
    scene = s; tx = -1; pending = -1;
    py = 134;
    px = (s == SC_FLAT) ? 120 : (s == SC_HALL ? 60 : 200);
}

static void reset_game(void) {
    setup_audio();
    inv = 0; held = 0; verb = V_LOOK;
    drawer_open = won = phone_done = false;
    msg[0] = 0; msg_t = 0; win_t = 0;
    respawn(SC_FLAT);
    ready = true;
    say("The phone is RINGING. Better answer it.");
}

static Hotspot *scene_hs(int *n) {
    if (scene == SC_FLAT) { *n = F_N; return flat_hs; }
    if (scene == SC_HALL) { *n = H_N; return hall_hs; }
    *n = T_N; return taxi_hs;
}

// is this hotspot currently present / interactable?
static bool hs_active(int id) {
    if (scene == SC_FLAT) {
        if (id == F_CLIP)   return !(inv & (IT_CLIP | IT_PICK));   // gone once taken
        if (id == F_DRAWER) return true;
        if (id == F_TICKET) return drawer_open && !(inv & IT_TICKET);
    }
    return true;
}

// which hotspot is the pointer over? -1 none
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

static bool zak_near(int id) {
    int n; Hotspot *hs = scene_hs(&n);
    return abs((int)px - hs[id].standx) < ACT_DIST;
}

// ── verb resolution ───────────────────────────────────────────────────
static void try_exit(int s) { say("..."); respawn(s); msg_t = 0; }

static void act_flat(int id) {
    // using a held inventory item on a hotspot
    if (held) {
        if (held == IT_PICK && id == F_DRAWER && !drawer_open) {
            drawer_open = true; held = 0;
            note(70, 6, 4); note(58, INSTR_NOISE, 2);
            say("CLICK. The drawer slides open. A ticket!");
            return;
        }
        say("That does nothing here.");
        held = 0; return;
    }
    switch (verb) {
    case V_LOOK:
        if (id == F_PHONE)  { say(phone_done ? "Just a phone now." : "It won't stop ringing."); }
        else if (id == F_DESK)   say("Coffee rings and a stuck drawer.");
        else if (id == F_DRAWER) say(drawer_open ? "Open. Plane ticket inside." : "Locked. A flimsy lock.");
        else if (id == F_CLIP)   say("A bent paperclip on the desk.");
        else if (id == F_TICKET) say("My flight to the airport!");
        else if (id == F_BOWL)   say("My goldfish, Sushi, eyes me.");
        else if (id == F_DOOR)   say("Bolted shut. The hall's the other way.");
        else if (id == F_EXIT)   say("The door to the hallway.");
        break;
    case V_TAKE:
        if (id == F_CLIP)   { inv |= IT_CLIP; note(72, 6, 3); say("Got the paperclip."); }
        else if (id == F_TICKET) {
            inv |= IT_TICKET; note(76, 6, 4);
            say("PLANE TICKET! Now to the airport.");
        }
        else if (id == F_BOWL)   say("Sushi stays. He bites.");
        else say("Can't take that.");
        break;
    case V_USE:
        if (id == F_PHONE && !phone_done) {
            phone_done = true; note(64, INSTR_SINE, 3);
            say("TIP: 'Ticket's locked in your desk!' *click*");
        }
        else if (id == F_DRAWER) {
            if (drawer_open) say("Already open.");
            else { note(48, INSTR_SQUARE, 3); say("Locked. Need something thin..."); }
        }
        else if (id == F_DOOR)   { note(48, INSTR_SQUARE, 3); say("Won't budge."); }
        else if (id == F_EXIT) {
            if (inv & IT_TICKET) try_exit(SC_HALL);
            else { note(48, INSTR_SQUARE, 3); say("Not without my ticket."); }
        }
        else say("Nothing happens.");
        break;
    case V_TALK:
        if (id == F_BOWL)  say("\"Morning, Sushi.\" *blub*");
        else if (id == F_PHONE && !phone_done) act_flat(F_PHONE); // talk == answer
        else say("It doesn't answer.");
        break;
    }
}

static void act_hall(int id) {
    if (held) { say("Won't work on that."); held = 0; return; }
    switch (verb) {
    case V_LOOK:
        if (id == H_VEND) say("SNAX machine. A bag's stuck.");
        else if (id == H_AXE)  say("'IN CASE OF FIRE.' Tempting.");
        else if (id == H_NOOK) say("A mop in a bucket of grey water.");
        else if (id == H_BACK) say("Back to my apartment.");
        else if (id == H_ELEV) say("The elevator down to the cab.");
        break;
    case V_USE:
        if (id == H_VEND) { note(45, INSTR_NOISE, 3); say("I shake it. Bag stays stuck."); }
        else if (id == H_AXE)  { note(48, INSTR_SQUARE, 3); say("Glass is locked. Leave it."); }
        else if (id == H_BACK) try_exit(SC_FLAT);
        else if (id == H_ELEV) try_exit(SC_TAXI);
        else say("Nothing happens.");
        break;
    case V_TAKE:
        if (id == H_NOOK) say("I'm a reporter, not a janitor.");
        else say("Can't take that.");
        break;
    case V_TALK:
        say("Nobody's around.");
        break;
    }
}

static void act_taxi(int id) {
    if (held) { say("The cabbie squints at me."); held = 0; return; }
    switch (verb) {
    case V_LOOK:
        if (id == T_DRIVER) say("Cabbie, half asleep.");
        else if (id == T_METER) say("Meter's already at $4.50.");
        else if (id == T_BACK)  say("Back inside.");
        break;
    case V_TALK:
        if (id == T_DRIVER) {
            if (won) say("\"Airport, like you said, pal.\"");
            else { won = true; win_t = 0; note(72, 6, 4);
                   strum(60, CHORD_MAJ7, INSTR_TRI, 4, 60);
                   say("\"AIRPORT — and step on it!\""); }
        }
        else say("It doesn't talk.");
        break;
    case V_USE:
        if (id == T_BACK) try_exit(SC_HALL);
        else if (id == T_DRIVER) say("Just tell him where to go.");
        else say("Nothing happens.");
        break;
    case V_TAKE:
        say("Can't take that.");
        break;
    }
}

static void run_verb(int id) {
    if (scene == SC_FLAT) act_flat(id);
    else if (scene == SC_HALL) act_hall(id);
    else act_taxi(id);
}

// ── update ─────────────────────────────────────────────────────────────
void update(void) {
    if (!ready) reset_game();
    if (msg_t > 0) msg_t -= dt();
    if (won) win_t += dt();

    // ringing phone nags until used
    if (scene == SC_FLAT && !phone_done && every(2)) note(72, 5, 3);

    if (won) {
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_SPACE)) reset_game();
        return;
    }

    int mx = mouse_x(), my = mouse_y();

    if (mouse_pressed(MOUSE_LEFT)) {
        // 1) verb bar — four buttons
        if (my >= BAR_Y && my < BAR_Y + 24 && mx < 86) {
            int v = my < BAR_Y + 12 ? (mx < 43 ? V_LOOK : V_USE)
                                    : (mx < 43 ? V_TAKE : V_TALK);
            verb = v; held = 0;
            note(70, INSTR_SINE, 2);
        }
        // 2) inventory strip — slots from x=92
        else if (my >= BAR_Y) {
            int slot = (mx - 92) / 22;
            // build present-items list left to right
            int present[3], pn = 0;
            if (inv & IT_CLIP)   present[pn++] = IT_CLIP;
            if (inv & IT_PICK)   present[pn++] = IT_PICK;
            if (inv & IT_TICKET) present[pn++] = IT_TICKET;
            if (mx >= 92 && slot >= 0 && slot < pn) {
                int it = present[slot];
                // USE verb on the clip-in-inventory bends it into a pick
                if (verb == V_USE && it == IT_CLIP) {
                    inv &= ~IT_CLIP; inv |= IT_PICK; held = 0;
                    note(64, INSTR_NOISE, 2); note(76, 6, 3);
                    say("I bend the clip into a tiny lockpick.");
                } else if (verb == V_LOOK) {
                    if (it == IT_CLIP)   say("A paperclip. Bendable.");
                    else if (it == IT_PICK)   say("A lockpick. For a flimsy lock.");
                    else if (it == IT_TICKET) say("My ticket out of here.");
                } else {
                    held = (held == it) ? 0 : it;   // toggle hold
                    note(72, INSTR_SINE, 2);
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
                ty = (int)clamp(py, 110, 144);
            } else if (my >= FLOOR_TOP) {
                tx = (int)clamp(mx, WALK_MINX, WALK_MAXX);
                ty = (int)clamp(my, 110, 144);
                pending = -1;
            }
        }
    }

    // walk toward target
    if (tx >= 0) {
        float sp = 78.0f * dt();
        if (abs((int)px - tx) > 2) { float d = sgn(tx - (int)px); px += d * sp; face = d < 0 ? -1 : 1; }
        if (abs((int)py - ty) > 2) py += sgn(ty - (int)py) * sp;
        px = clamp(px, WALK_MINX, WALK_MAXX);
        if (abs((int)px - tx) <= 2 && abs((int)py - ty) <= 2) {
            tx = -1;
            if (pending >= 0) {
                if (zak_near(pending)) run_verb(pending);
                pending = -1;
            }
        }
    }

    if (keyp(KEY_SPACE) || keyp(KEY_ESCAPE)) msg_t = 0;
}

// ── drawing ─────────────────────────────────────────────────────────────
static void draw_zak(void) {
    int x = (int)px, y = (int)py;
    bool walking = tx >= 0;
    int step = walking ? anim(2, 7, 0) : 0;
    ovalfill(x, y, 7, 3, CLR_DARKER_PURPLE);                  // shadow
    rectfill(x - 3, y - 9, 3, 9 - step, CLR_DARK_BLUE);       // far leg
    rectfill(x + 1, y - 9 + step, 3, 9 - step, CLR_DARK_BLUE);// near leg
    rectfill(x - 4, y - 20, 9, 12, CLR_WHITE);               // trench/shirt
    rectfill(x - 6, y - 19, 3, 8, CLR_LIGHT_PEACH);          // back arm
    rectfill(x + 4, y - 19, 3, 8, CLR_LIGHT_PEACH);          // front arm
    circfill(x, y - 24, 4, CLR_PEACH);                       // head
    pset(x + face * 2, y - 24, CLR_BLACK);                   // eye
    rectfill(x - 5, y - 28, 11, 2, CLR_DARK_BROWN);          // hat brim
    rectfill(x - 3, y - 31, 7, 3, CLR_DARK_BROWN);           // hat
    rectfill(x - 4, y - 9, 9, 1, CLR_BROWN);                 // belt
}

static void draw_label(const char *s, int col) {
    font(FONT_SMALL);
    int w = text_width(s) + 6, x = mid(2, mouse_x() - w / 2, SCREEN_W - w - 2);
    int y = mouse_y() - 11; if (y < 2) y = mouse_y() + 8;
    fillp(FILL_CHECKER, -1);
    rectfill(x, y, w, 9, CLR_BLACK);
    fillp_reset();
    print(s, x + 3, y + 2, col);
    font(FONT_NORMAL);
}

static void scene_flat(void) {
    cls(CLR_DARKER_BLUE);
    rectfill(0, 0, SCREEN_W, FLOOR_TOP, CLR_DARK_PURPLE);          // wall
    rectfill(0, FLOOR_TOP, SCREEN_W, BAR_Y - FLOOR_TOP, CLR_BROWN); // floor
    line(0, FLOOR_TOP, SCREEN_W, FLOOR_TOP, CLR_DARK_BROWN);
    // window with night skyline
    rectfill(94, 18, 56, 40, CLR_BLACK);
    rect(94, 18, 56, 40, CLR_LIGHT_GREY);
    for (int i = 0; i < 6; i++) { int bx = 98 + i * 9, bh = 10 + (i * 7 % 18);
        rectfill(bx, 58 - bh, 7, bh, CLR_DARKER_PURPLE);
        if ((i + frame() / 40) % 3 == 0) pset(bx + 2, 58 - bh + 3, CLR_LIGHT_YELLOW); }
    circfill(138, 26, 4, CLR_LIGHT_GREY);                          // moon
    // front door (left)
    rectfill(12, 52, 30, 60, CLR_DARK_BROWN);
    rect(12, 52, 30, 60, CLR_BROWN);
    circfill(36, 84, 1, CLR_YELLOW);
    // fishbowl on a stand
    rectfill(58, 104, 12, 30, CLR_DARK_GREY);
    circfill(64, 92, 12, CLR_TRUE_BLUE);
    arc(64, 92, 12, 200, 340, CLR_BLUE);
    ovalfill(64 + (int)(sin_deg(now() * 90) * 4), 94, 3, 2, CLR_ORANGE);
    // desk
    rectfill(146, 92, 96, 8, CLR_DARK_BROWN);
    rectfill(150, 100, 92, 34, CLR_BROWN);
    rect(156, 100, 30, 12, CLR_DARK_BROWN);                        // drawer face
    if (drawer_open) {
        rectfill(150, 100, 42, 12, CLR_BROWNISH_BLACK);
        if (!(inv & IT_TICKET)) { rectfill(162, 102, 16, 8, CLR_LIGHT_YELLOW);
                                   line(164, 104, 174, 104, CLR_DARK_RED); }
    } else { pset(170, 106, CLR_YELLOW); }                         // keyhole
    // phone
    rectfill(186, 84, 24, 10, CLR_BLACK);
    rectfill(190, 78, 16, 8, CLR_DARKER_GREY);
    if (!phone_done && blink(8)) { print("!", 200, 70, CLR_RED); arc(208, 82, 6, 250, 360, CLR_LIGHT_YELLOW); }
    // paperclip on the desk
    if (!(inv & (IT_CLIP | IT_PICK))) { arc(212, 92, 3, 0, 300, CLR_LIGHT_GREY); }
    // exit arrow (right)
    rectfill(288, 60, 24, 80, CLR_DARKER_PURPLE);
    if (blink(20)) print(">", 296, 96, CLR_LIGHT_YELLOW);
}

static void scene_hall(void) {
    cls(CLR_BROWNISH_BLACK);
    rectfill(0, 0, SCREEN_W, FLOOR_TOP, CLR_DARKER_GREY);
    rectfill(0, FLOOR_TOP, SCREEN_W, BAR_Y - FLOOR_TOP, CLR_DARK_GREY);
    line(0, FLOOR_TOP, SCREEN_W, FLOOR_TOP, CLR_BLACK);
    // carpet runner
    fillp(FILL_HLINES, CLR_DARKER_GREY); rectfill(0, 116, SCREEN_W, 18, CLR_DARK_RED); fillp_reset();
    // back door (left)
    rectfill(4, 60, 22, 78, CLR_DARK_BROWN); rect(4, 60, 22, 78, CLR_BROWN);
    if (blink(20)) print("<", 9, 96, CLR_LIGHT_YELLOW);
    // vending machine
    rectfill(40, 56, 40, 78, CLR_TRUE_BLUE); rect(40, 56, 40, 78, CLR_DARK_BLUE);
    rectfill(46, 62, 28, 40, CLR_BLACK);
    for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++)
        rectfill(48 + c * 9, 64 + r * 12, 6, 8, c == 1 && r == 2 ? CLR_DARK_RED : CLR_DARK_ORANGE);
    rectfill(46, 110, 28, 6, CLR_RED);                            // stuck bag tray
    print("SNAX", 48, 104, CLR_LIGHT_YELLOW);
    // fire-axe case
    rectfill(130, 64, 28, 30, CLR_DARK_RED); rect(130, 64, 28, 30, CLR_RED);
    line(136, 72, 136, 88, CLR_MEDIUM_GREY); trifill(134, 70, 142, 72, 134, 76, CLR_LIGHT_GREY);
    fillp(FILL_CHECKER, -1); rectfill(132, 66, 24, 26, CLR_BLUE); fillp_reset(); // glass
    // janitor nook
    rectfill(204, 100, 12, 12, CLR_DARK_GREY);                    // bucket
    line(214, 112, 220, 70, CLR_DARK_BROWN);                      // mop handle
    circfill(220, 70, 4, CLR_LIGHT_GREY);
    // elevator
    rectfill(270, 48, 44, 90, CLR_MEDIUM_GREY); rect(270, 48, 44, 90, CLR_LIGHT_GREY);
    line(292, 48, 292, 138, CLR_DARK_GREY);
    rectfill(296, 52, 14, 8, CLR_BLACK); print("v", 300, 53, CLR_GREEN);
    if (blink(20)) print(">", 305, 96, CLR_LIGHT_YELLOW);
}

static void scene_taxi(void) {
    cls(CLR_DARK_BLUE);
    // night street through the windshield
    rectfill(0, 0, SCREEN_W, 60, CLR_DARKER_BLUE);
    for (int i = 0; i < 14; i++) pset((i * 47 + frame()) % SCREEN_W, 12 + (i * 13 % 30), CLR_LIGHT_YELLOW);
    rectfill(0, 60, SCREEN_W, FLOOR_TOP - 60, CLR_BROWNISH_BLACK); // road
    for (int i = 0; i < 6; i++) rectfill(40 + i * 50 - (frame() * 2 % 50), 78, 18, 4, CLR_LIGHT_YELLOW);
    // cab interior / dashboard
    rectfill(0, FLOOR_TOP, SCREEN_W, BAR_Y - FLOOR_TOP, CLR_DARK_ORANGE);
    rectfill(0, 92, SCREEN_W, 14, CLR_DARK_BROWN);                // dash
    // cabbie (back of head)
    circfill(70, 78, 16, CLR_DARK_BROWN);
    circfill(70, 70, 9, CLR_DARK_PEACH);
    rectfill(54, 64, 32, 6, CLR_DARK_GREEN);                      // cap
    // meter
    rectfill(150, 70, 30, 22, CLR_BLACK); rect(150, 70, 30, 22, CLR_MEDIUM_GREY);
    print("4.50", 152, 78, CLR_DARK_ORANGE);
    // back-arrow exit
    rectfill(4, 60, 22, 78, CLR_DARK_BROWN);
    if (blink(20)) print("<", 9, 96, CLR_LIGHT_YELLOW);
}

void draw(void) {
    if (!ready) reset_game();

    if (scene == SC_FLAT) scene_flat();
    else if (scene == SC_HALL) scene_hall();
    else scene_taxi();

    draw_zak();

    // hover label (verb + hotspot) when not on the bar
    int hov = hover_hotspot();
    if (!won && hov >= 0 && msg_t <= 0) {
        int n; Hotspot *hs = scene_hs(&n);
        const char *act = held ? (held == IT_PICK ? "Use lockpick on" :
                                  held == IT_CLIP ? "Use clip on" : "Use ticket on")
                                : VERB_NAME[verb];
        draw_label(str("%s %s", act, hs[hov].name), CLR_LIGHT_YELLOW);
    }

    // ── verb bar + inventory ──
    fillp(FILL_CHECKER, CLR_BLACK); rectfill(0, BAR_Y, SCREEN_W, SCREEN_H - BAR_Y, CLR_DARKER_GREY); fillp_reset();
    line(0, BAR_Y, SCREEN_W, BAR_Y, CLR_LIGHT_GREY);
    // four verb buttons in a 2x2
    for (int v = 0; v < NVERB; v++) {
        int bx = (v % 2) * 43, by = BAR_Y + (v / 2) * 12;
        bool sel = (v == verb && !held);
        rectfill(bx + 1, by + 1, 41, 11, sel ? CLR_DARK_GREEN : CLR_BLACK);
        print(VERB_NAME[v], bx + 4, by + 3, sel ? CLR_WHITE : CLR_LIGHT_GREY);
    }
    line(88, BAR_Y, 88, SCREEN_H, CLR_DARK_GREY);
    // inventory slots
    int present[3], pn = 0;
    if (inv & IT_CLIP)   present[pn++] = IT_CLIP;
    if (inv & IT_PICK)   present[pn++] = IT_PICK;
    if (inv & IT_TICKET) present[pn++] = IT_TICKET;
    for (int s = 0; s < 4; s++) {
        int sx = 92 + s * 22, sy = BAR_Y + 4;
        rect(sx, sy, 20, 20, CLR_DARK_GREY);
        if (s < pn) {
            int it = present[s];
            if (it == held) rectfill(sx + 1, sy + 1, 18, 18, CLR_DARK_GREEN);
            if (it == IT_CLIP) { arc(sx + 10, sy + 10, 5, 0, 300, CLR_LIGHT_GREY); }
            else if (it == IT_PICK) { line(sx + 5, sy + 14, sx + 14, sy + 5, CLR_LIGHT_GREY); pset(sx + 5, sy + 13, CLR_WHITE); }
            else if (it == IT_TICKET) { rectfill(sx + 4, sy + 7, 12, 6, CLR_LIGHT_YELLOW); line(sx + 6, sy + 9, sx + 13, sy + 9, CLR_DARK_RED); }
        }
    }
    // message line — small font on its own strip below the bar so long
    // sentences fit the 320px width instead of running off the right edge
    font(FONT_SMALL);
    if (msg_t > 0) print_centered(msg, SCREEN_W/2, BAR_Y + 33, CLR_LIGHT_YELLOW);
    else print_centered(str("ZAK McKRACKEN  -  %s", scene == SC_FLAT ? "apartment" : scene == SC_HALL ? "hallway" : "taxi"), SCREEN_W/2, BAR_Y + 33, CLR_DARK_GREY);
    font(FONT_NORMAL);

    // ── win card — the shared end-screen treatment (endcard.h) ──
    if (won) {
        EndCard c = endcard(win_t, 240, 56, 48, CLR_DARK_BLUE, CLR_LIGHT_YELLOW, CLR_INDIGO);
        if (c.settled) {
            print_centered("OFF TO THE AIRPORT!", SCREEN_W/2, c.y + 12, CLR_LIGHT_YELLOW);
            print_centered("The story of a lifetime awaits.", SCREEN_W/2, c.y + 26, CLR_LIGHT_PEACH);
            if (blink(18)) print_centered("- click to play again -", SCREEN_W/2, c.y + 40, CLR_LIGHT_GREY);
        }
    }
}
