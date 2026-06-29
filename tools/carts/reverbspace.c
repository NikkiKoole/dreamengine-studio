/* de:meta
{
  "title": "reverb spaces",
  "status": "active",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "audio-occlusion"
  ],
  "lineage": "Sibling to the effects-bus pedalboard family (groovebox/kaoss); novel in routing two instruments into independent reverb tanks simultaneously to demonstrate spatial separation impossible with a single shared reverb.",
  "description": "The showcase for the reverb SEND-BUSES - two independent reverbs ringing at the same time, which the single shared reverb (cathedral) can't do. A bright MALLET sits in a small, tight, bright ROOM (tank 1) while a soft ORGAN blooms in a vast, dark, endless PLATE (tank 2): same note family, two completely different spaces, one mix. Built on reverb_bus(tank, size, damp) to carve each space on its own bus and instrument_reverb_bus(slot, tank, mix) to send a slot into it. The screen splits into the two rooms; each rings its own colour of light when struck (tight yellow rings on the left, slow indigo swells on the right). Drag ROOM / PLATE to resize each space live; press 1..4 to ring the room and 5..8 the plate; SPACE toggles AUTO (the two spaces trade phrases so you can just listen); H for help. Part of the effects-bus / pedalboard family."
}
de:meta */
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
//   C      CRUSH the plate — an effect runs AFTER the reverb (reverb_bus_fx):
//          the wet plate tail rings, THEN a bitcrusher chews it = a lo-fi,
//          degraded, shoegaze space. A send-bus can do this; a plain send can't.
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
static bool  crush_on = false;   // C: a bitcrusher AFTER the plate reverb (reverb_bus_fx)
static int   last_step = -1;
static int   auto_i = 0;

// the self-playing material: a bright room ARP (mallet) over a slow plate CHORD loop (organ).
// Two songs you can flip between (S) — song 0 is the original calm one; song 1 is a wistful minor.
typedef struct { const char *name; int arp[4]; int chord[4][3]; } Song;
static const Song SONG[2] = {
    { "C major (calm)",
      { 72, 76, 79, 84 },                                  // C5 arp — bright, plucky
      { {48,55,60}, {45,52,57}, {53,60,64}, {50,57,62} } },// C  Am  F  Dm — the original processional
    { "A minor (wistful)",
      { 69, 72, 76, 81 },                                  // A4 arp — Am
      { {45,52,57}, {41,48,53}, {40,47,52}, {43,50,55} } },// Am  F  Em  G — a melancholy loop
};
static int song = 0;

// blooms: rings of light, one pool per side so you SEE which space rang
#define NBLOOM 10
typedef struct { float age, life; int x, y; bool plate, on; } Bloom;
static Bloom bloom[NBLOOM];

static void spawn_bloom(int x, int y, float life, bool plate) {
    for (int b = 0; b < NBLOOM; b++)
        if (!bloom[b].on) { bloom[b] = (Bloom){ 0, life, x, y, plate, true }; return; }
}

static void ring_room(int i) {
    hit(SONG[song].arp[i & 3], SL_ROOM, 5, 140);           // short, bright
    spawn_bloom(40 + (i & 3) * 18, 150, 60.0f + k_room * 120.0f, false);
}
static void ring_plate(int i) {
    for (int n = 0; n < 3; n++) hit(SONG[song].chord[i & 3][n], SL_PLATE, 4, 480);
    spawn_bloom(232, 110, 120.0f + k_plate * 240.0f, true); // long, dark
}

static void apply_spaces(void) {
    reverb_bus(TANK_ROOM,  k_room,  0.15f);                // tight + bright
    reverb_bus(TANK_PLATE, k_plate, 0.55f);                // vast  + dark
}
// an effect AFTER the plate reverb: the wet tail rings, THEN the crusher degrades it.
// mix 0 = bypassed (clean plate); 1 = full lo-fi crushed tail. 5-bit / rate-8 = grainy + aliased.
static void apply_crush(void) { reverb_bus_fx(TANK_PLATE, FX_CRUSH, 5.0f, 8.0f, crush_on ? 1.0f : 0.0f); }

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
    if (keyp('C')) { crush_on = !crush_on; apply_crush(); }
    if (keyp('S')) { song ^= 1; last_step = -1; auto_i = 0; }   // flip the song; restart the loop cleanly
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

    // ── blooms ── (the plate's go BLOCKY when crushed — the tail is being bit-crushed)
    for (int b = 0; b < NBLOOM; b++) {
        if (!bloom[b].on) continue;
        float t = bloom[b].age / bloom[b].life;
        int   r = (int)(4.0f + t * (bloom[b].plate ? 110.0f : 34.0f));
        int   y = bloom[b].y - (int)(t * (bloom[b].plate ? 70.0f : 20.0f));
        int   col = bloom[b].plate
                  ? (t < 0.4f ? CLR_INDIGO : t < 0.75f ? CLR_DARK_PURPLE : CLR_DARKER_PURPLE)
                  : (t < 0.4f ? CLR_LIGHT_YELLOW : t < 0.75f ? CLR_YELLOW : CLR_ORANGE);
        if (t >= 0.95f) continue;
        if (bloom[b].plate && crush_on) {
            int q = ((r >> 3) + 1) << 3;                       // quantize the radius → stepped, aliased growth
            rect(bloom[b].x - q, y - q, q * 2, q * 2, col);    // square ring = the crushed, pixelated tail
        } else {
            circ(bloom[b].x, y, r, col);
        }
    }

    // ── titles ──
    print("REVERB SPACES", 8, 6, CLR_LIGHT_PEACH);
    font(FONT_SMALL);
    print("two reverbs at once", 8, 18, CLR_DARK_GREY);
    font(FONT_NORMAL);
    print("ROOM",  46, 32, CLR_LIGHT_YELLOW);
    print(crush_on ? "PLATE -CRUSHED" : "PLATE", 214, 32, crush_on ? CLR_DARK_ORANGE : CLR_INDIGO);

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
            "1..4 room   5..8 plate",
            "C     CRUSH the plate tail (post-verb)",
            "S     flip the SONG (2 progressions)",
            "SPACE AUTO - it plays itself",
        };
        for (int i = 0; i < 11; i++)
            print(HL[i], 36, 46 + i * 11, i < 4 ? CLR_LIGHT_PEACH : CLR_WHITE);
        ui_begin();
        if (ui_button(130, 162, 60, 14, "CLOSE")) show_help = false;
        ui_end();
        return;
    }

    // ── the size knobs + the control strip ──
    sprintf(buf, "ROOM %d   PLATE %d", (int)(k_room * 100), (int)(k_plate * 100));
    print(buf, 96, 182, CLR_DARK_GREY);
    font(FONT_SMALL);
    sprintf(buf, "SONG: %s", SONG[song].name);             // the current progression, named
    print(buf, 110, 192, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
    ui_begin();
    if (ui_knob(&k_room,  40,  150, "ROOM"))  apply_spaces();
    if (ui_knob(&k_plate, 280, 150, "PLATE")) apply_spaces();
    if (ui_button(120, 50, 80, 15, autop ? "AUTO ON" : "AUTO")) { autop = !autop; last_step = -1; }
    if (ui_button(120, 68, 80, 15, song ? "SONG 2" : "SONG 1")) { song ^= 1; last_step = -1; auto_i = 0; }
    if (ui_button(120, 86, 80, 15, crush_on ? "CRUSH ON" : "CRUSH")) { crush_on = !crush_on; apply_crush(); }
    if (ui_button(120, 104, 80, 13, "HELP")) show_help = true;
    ui_end();
}
