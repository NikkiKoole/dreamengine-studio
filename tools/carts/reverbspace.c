#include "studio.h"
#include "ui.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── REVERB SPACES ───────────────────────────────────────────────────────────
// The showcase for the reverb SEND-BUSES (effects-bus-architecture.md §5,
// Increment C). Until now there was exactly ONE reverb — every instrument
// bloomed into the same room. Now there are independent reverb TANKS, and a
// slot can be routed into its own. This cart puts two instruments in two
// completely different spaces AT THE SAME TIME and lets you hear them ring
// together:
//
//   LEFT  — a bright MALLET in a small, TIGHT, bright ROOM   (tank 1)
//   RIGHT — a soft ORGAN  in a vast, dark, endless PLATE      (tank 2)
//
// Same note family, two spaces, one mix. A single shared reverb can't do this:
// it can only be one room at a time. The two tanks ring independently.
//
// How it maps onto the API:
//   reverb_bus(tank, size, damp)            — build a space on its own bus
//   instrument_reverb_bus(slot, tank, mix)  — send a slot into that space
//
//   ROOM / PLATE knobs   drag the size of each space, live
//   1..4   ring the room     5..8   ring the plate
//   SPACE  AUTO — the two spaces trade phrases so you can just listen

#define SL_ROOM   8     // the tight-room instrument
#define SL_PLATE  9     // the vast-plate instrument
#define TANK_ROOM   1   // tank 0 is reverb()'s master send — bus tanks are 1..2
#define TANK_PLATE  2

// the two live size knobs (ui.h wants floats 0..1)
static float k_room  = 0.34f;   // small, tight
static float k_plate = 0.95f;   // endless

static bool  autop = true;
static bool  show_help;
static int   last_step = -1;
static int   auto_i = 0;

// a bright room arpeggio (mallet) and a dark plate chord (organ)
static const int ROOM_ARP[4]   = { 72, 76, 79, 84 };       // C5 arp — plucky, short
static const int PLATE_CHORD[4][3] = {
    { 48, 55, 60 }, { 45, 52, 57 }, { 53, 60, 64 }, { 50, 57, 62 },
};

// blooms: rings of light, one pool per side so you SEE which space rang
#define NBLOOM 10
typedef struct { float age, life; int x, y; bool plate, on; } Bloom;
static Bloom bloom[NBLOOM];

static void spawn_bloom(int x, int y, float life, bool plate) {
    for (int b = 0; b < NBLOOM; b++)
        if (!bloom[b].on) { bloom[b] = (Bloom){ 0, life, x, y, plate, true }; return; }
}

static void ring_room(int i) {
    hit(ROOM_ARP[i & 3], SL_ROOM, 5, 140);                 // short, bright
    spawn_bloom(40 + (i & 3) * 18, 150, 60.0f + k_room * 120.0f, false);
}
static void ring_plate(int i) {
    for (int n = 0; n < 3; n++) hit(PLATE_CHORD[i & 3][n], SL_PLATE, 4, 480);
    spawn_bloom(232, 110, 120.0f + k_plate * 240.0f, true); // long, dark
}

static void apply_spaces(void) {
    reverb_bus(TANK_ROOM,  k_room,  0.15f);                // tight + bright
    reverb_bus(TANK_PLATE, k_plate, 0.55f);                // vast  + dark
}

void init(void) {
    bpm(96);
    // LEFT: a bright mallet — short, percussive, lands in the small room
    instrument(SL_ROOM, INSTR_MALLET, 1, 0, 3, 120);
    instrument_harmonics(SL_ROOM, 0.6f);
    // RIGHT: a soft organ pad — sustained, blooms into the plate
    instrument(SL_PLATE, INSTR_ORGAN, 24, 0, 6, 520);
    instrument_harmonics(SL_PLATE, 0.5f);
    instrument_timbre(SL_PLATE, 0.45f);
    // the two spaces + the two sends
    apply_spaces();
    instrument_reverb_bus(SL_ROOM,  TANK_ROOM,  0.7f);     // drenched room
    instrument_reverb_bus(SL_PLATE, TANK_PLATE, 0.85f);    // drowned plate
}

void update(void) {
    if (keyp(KEY_SPACE)) { autop = !autop; last_step = -1; }
    if (keyp('H')) show_help = !show_help;
    for (int i = 0; i < 4; i++) {
        if (keyp('1' + i)) ring_room(i);
        if (keyp('5' + i)) ring_plate(i);
    }

    if (autop) {
        int step = beat();
        if (step != last_step) {
            last_step = step;
            ring_room(step);                               // the room patters every beat
            if ((step & 1) == 0) { ring_plate(auto_i); auto_i = (auto_i + 1) & 3; }  // the plate swells every 2
        }
    }

    for (int b = 0; b < NBLOOM; b++) {
        if (!bloom[b].on) continue;
        bloom[b].age += 1.0f;
        if (bloom[b].age >= bloom[b].life) bloom[b].on = false;
    }
}

void draw(void) {
    char buf[48];
    cls(CLR_BLACK);

    // ── the divide: a tight bright box (left) vs a vast dark vault (right) ──
    rectfill(0,   28, 156, 156, CLR_DARKER_BLUE);          // the ROOM — small, lit
    rect(8,  90, 140, 86, CLR_LIGHT_GREY);                 // its hard near walls
    rectfill(164, 28, 156, 156, CLR_BROWNISH_BLACK);       // the PLATE — vast, dark
    for (int a = 0; a < 4; a++)                            // receding arches
        circ(242, 150, 16 + a * 26, CLR_DARK_PURPLE);

    // ── blooms ──
    for (int b = 0; b < NBLOOM; b++) {
        if (!bloom[b].on) continue;
        float t = bloom[b].age / bloom[b].life;
        float rad = 4.0f + t * (bloom[b].plate ? 110.0f : 34.0f);
        int   col = bloom[b].plate
                  ? (t < 0.4f ? CLR_INDIGO : t < 0.75f ? CLR_DARK_PURPLE : CLR_DARKER_PURPLE)
                  : (t < 0.4f ? CLR_LIGHT_YELLOW : t < 0.75f ? CLR_YELLOW : CLR_ORANGE);
        if (t < 0.95f) circ(bloom[b].x, bloom[b].y - (int)(t * (bloom[b].plate ? 70.0f : 20.0f)), (int)rad, col);
    }

    // ── titles ──
    print("REVERB SPACES", 8, 6, CLR_LIGHT_PEACH);
    font(FONT_SMALL);
    print("two reverbs at once", 8, 18, CLR_DARK_GREY);
    font(FONT_NORMAL);
    print("ROOM",  46, 32, CLR_LIGHT_YELLOW);
    print("PLATE", 214, 32, CLR_INDIGO);

    if (show_help) {
        rectfill(24, 24, 272, 156, CLR_BLACK);
        rect(24, 24, 272, 156, CLR_INDIGO);
        print("REVERB SPACES", 96, 32, CLR_LIGHT_YELLOW);
        static const char *HL[] = {
            "Two instruments, two DIFFERENT reverb",
            "spaces, ringing at the same time. A",
            "single shared reverb can only be one",
            "room - these are independent tanks.",
            "",
            "ROOM   tight + bright (tank 1)",
            "PLATE  vast + dark    (tank 2)",
            "1..4   ring the room   5..8 the plate",
            "SPACE  AUTO - it plays itself",
        };
        for (int i = 0; i < 9; i++)
            print(HL[i], 36, 46 + i * 11, i < 4 ? CLR_LIGHT_PEACH : CLR_WHITE);
        ui_begin();
        if (ui_button(130, 162, 60, 14, "CLOSE")) show_help = false;
        ui_end();
        return;
    }

    // ── the two size knobs + auto/help ──
    sprintf(buf, "ROOM %d    PLATE %d", (int)(k_room * 100), (int)(k_plate * 100));
    print(buf, 92, 186, CLR_DARK_GREY);
    ui_begin();
    if (ui_knob(&k_room,  40,  150, "ROOM"))  apply_spaces();
    if (ui_knob(&k_plate, 280, 150, "PLATE")) apply_spaces();
    if (ui_button(120, 60, 80, 16, autop ? "AUTO ON" : "AUTO")) { autop = !autop; last_step = -1; }
    if (ui_button(120, 80, 80, 14, "HELP")) show_help = true;
    ui_end();
}
